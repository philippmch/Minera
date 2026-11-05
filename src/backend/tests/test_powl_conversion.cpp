#include "minera/powl_conversion.hpp"
#include "minera/conformance.hpp"
#include "minera/pnml.hpp"
#include "minera/ptml.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

using minera::PowlNode;
using minera::ProcessTree;
using minera::ProcessTreeNode;
using Op = minera::ProcessTreeOperator;

using Marking = std::vector<std::size_t>;
using Trace = std::vector<std::string>;

struct ReferenceWorkflowCase {
  std::string name;
  std::string pnml;
  std::set<Trace> traces;
};

#include "workflow_net_tree_reference_cases.inc"
#include "workflow_net_powl_reference_cases.inc"

struct ReferenceTreeShapeCase {
  std::string name;
  std::string pnml;
  std::string expected;  // canonical tree text, or '!' and the exception type
};

#include "net_to_tree_shape_reference_cases.inc"

struct LanguageEnumeration {
  std::set<Trace> traces;
  std::set<std::size_t> live_transitions;
  std::set<Marking> completed_markings;
};

// Enumerate the bounded marking language directly from the generated net.
// The path-local state set cuts silent cycles while still allowing a visible
// loop to revisit a marking (the trace is part of the key).  This is stronger
// than a reachability check: only executions ending in the exact source/sink
// final marking contribute traces and transition liveness.
LanguageEnumeration enumerate_language(const minera::PetriNet& net,
                                        std::size_t max_visible_length = 3,
                                        std::size_t max_firings = 64) {
  LanguageEnumeration result;
  const Marking initial(net.place_count(), 0);
  Marking start = initial;
  start[net.source()] = 1;
  Marking expected_final(net.place_count(), 0);
  expected_final[net.sink()] = 1;
  std::vector<std::size_t> path;
  std::set<std::pair<Marking, Trace>> path_states;

  const auto is_enabled = [](const Marking& marking,
                             const minera::PetriTransition& transition) {
    std::map<std::size_t, std::size_t> required;
    for (const auto place : transition.inputs) ++required[place];
    for (const auto [place, count] : required) {
      if (place >= marking.size() || marking[place] < count) return false;
    }
    return true;
  };
  const auto fire = [](Marking marking,
                       const minera::PetriTransition& transition) {
    for (const auto place : transition.inputs) --marking[place];
    for (const auto place : transition.outputs) ++marking[place];
    return marking;
  };
  const auto is_final = [&](const Marking& marking) {
    return marking == expected_final;
  };

  const auto visit = [&](const auto& self, const Marking& marking,
                         const Trace& trace, std::size_t firings) -> void {
    if (firings > max_firings) return;
    const auto key = std::make_pair(marking, trace);
    if (!path_states.insert(key).second) return;
    // Reaching the sink is completion, never an intermediate marking: a sink
    // token alongside any residual token (or multiple sink tokens) is an
    // improper workflow-net completion and must fail the conversion test.
    if (marking[net.sink()] > 0) assert(is_final(marking));
    if (is_final(marking)) {
      result.traces.insert(trace);
      result.completed_markings.insert(marking);
      result.live_transitions.insert(path.begin(), path.end());
    }
    for (std::size_t index = 0; index < net.transitions().size(); ++index) {
      const auto& transition = net.transitions()[index];
      if (!is_enabled(marking, transition)) continue;
      if (transition.activity && trace.size() >= max_visible_length) continue;
      const auto next_marking = fire(marking, transition);
      Trace next_trace = trace;
      if (transition.activity) {
        if (*transition.activity >= net.activities().size()) continue;
        next_trace.push_back(net.activities()[*transition.activity]);
      }
      path.push_back(index);
      self(self, next_marking, next_trace, firings + 1);
      path.pop_back();
    }
    path_states.erase(key);
  };

  visit(visit, start, {}, 0);
  assert(!result.completed_markings.empty());
  assert(result.completed_markings.size() == 1);
  assert(*result.completed_markings.begin() == expected_final);
  return result;
}

Trace t(std::initializer_list<const char*> labels) {
  Trace result;
  for (const auto* label : labels) result.emplace_back(label);
  return result;
}

std::set<Trace> traces(std::initializer_list<Trace> values) {
  return std::set<Trace>(values.begin(), values.end());
}

void process_tree_to_powl_variations() {
  const ProcessTree tree({"A", "", "B"},
                         ProcessTreeNode{Op::kSequence, 0,
                                         {ProcessTreeNode::leaf(0),
                                          ProcessTreeNode{Op::kParallel, 0,
                                                          {ProcessTreeNode::leaf(1),
                                                           ProcessTreeNode::leaf(2)}}}});
  const auto model = minera::to_powl(tree);
  assert(model.kind() == minera::PowlNodeKind::kStrictPartialOrder);
  assert(model.children().size() == 2);
  assert(model.children()[0] == PowlNode::transition("A"));
  assert(model.children()[1].kind() == minera::PowlNodeKind::kStrictPartialOrder);
  assert(model.children()[1].children()[0].label().empty());
  assert(model.children()[1].order().empty());

  ProcessTreeNode loop{Op::kLoop, 0,
                       {ProcessTreeNode::leaf(0), ProcessTreeNode::leaf(1)}};
  const auto loop_model = minera::to_powl(ProcessTree({"A", "B", "C"}, loop));
  assert(loop_model.kind() == minera::PowlNodeKind::kLoop);
  assert(loop_model.children()[0].label() == "A");
  assert(loop_model.children()[1].label() == "B");

  ProcessTreeNode inclusive{Op::kInclusiveChoice, 0,
                            {ProcessTreeNode::leaf(0),
                             ProcessTreeNode::leaf(1)}};
  const auto inclusive_model =
      minera::to_powl(ProcessTree({"A", "B"}, inclusive));
  assert(inclusive_model.kind() == minera::PowlNodeKind::kXor);
  assert(inclusive_model.children().size() == 3);
  bool saw_a = false;
  bool saw_b = false;
  bool saw_ab = false;
  for (const auto& child : inclusive_model.children()) {
    if (child.kind() == minera::PowlNodeKind::kTransition && child.label() == "A")
      saw_a = true;
    if (child.kind() == minera::PowlNodeKind::kTransition && child.label() == "B")
      saw_b = true;
    if (child.kind() == minera::PowlNodeKind::kStrictPartialOrder &&
        child.children().size() == 2 && child.order().empty())
      saw_ab = true;
  }
  assert(saw_a && saw_b && saw_ab);

  const auto interleaving_model = minera::to_powl(ProcessTree(
      {"A", "B", "C"}, ProcessTreeNode{Op::kInterleaving, 0,
          {ProcessTreeNode{Op::kSequence, 0,
                           {ProcessTreeNode::leaf(0), ProcessTreeNode::leaf(1)}},
           ProcessTreeNode::leaf(2)}}));
  assert(interleaving_model.kind() == minera::PowlNodeKind::kXor);
  assert(interleaving_model.children().size() == 2);
  const auto interleaving_language = enumerate_language(
      minera::to_petri_net(interleaving_model), 3);
  assert(interleaving_language.traces ==
         traces({t({"A", "B", "C"}), t({"C", "A", "B"})}));

  bool invalid = false;
  try {
    (void)minera::to_powl(ProcessTree({"A"}, ProcessTreeNode::leaf(1)));
  } catch (const std::out_of_range&) {
    invalid = true;
  }
  assert(invalid);

  ProcessTreeNode zero_xor{Op::kExclusive, 0, {}};
  ProcessTreeNode unary_xor{Op::kExclusive, 0,
                            {ProcessTreeNode::leaf(0)}};
  for (const auto& malformed : {zero_xor, unary_xor}) {
    bool malformed_xor = false;
    try {
      (void)minera::to_powl(ProcessTree({"A"}, malformed));
    } catch (const std::invalid_argument&) {
      malformed_xor = true;
    }
    assert(malformed_xor);
  }

  ProcessTreeNode zero_loop{Op::kLoop, 0, {}};
  ProcessTreeNode unary_loop{Op::kLoop, 0, {ProcessTreeNode::leaf(0)}};
  ProcessTreeNode ternary_loop{Op::kLoop, 0,
                               {ProcessTreeNode::leaf(0),
                                ProcessTreeNode::leaf(1),
                                ProcessTreeNode::leaf(0)}};
  for (const auto& malformed : {zero_loop, unary_loop, ternary_loop}) {
    bool rejected = false;
    try {
      (void)minera::to_powl(ProcessTree({"A", "B"}, malformed));
    } catch (const std::invalid_argument&) {
      rejected = true;
    }
    assert(rejected);
  }

  // A transition-free net names the tree conversion (its PTML export) and
  // keeps the POWL wording for the POWL conversion.
  minera::PetriNet bare;
  bare.set_source(bare.add_place());
  bare.set_sink(bare.add_place());
  const auto message = [&](auto convert) {
    try { (void)convert(); } catch (const std::invalid_argument& error) { return std::string(error.what()); }
    return std::string("<none>");
  };
  assert(message([&] { return minera::to_process_tree(bare); }) ==
         "workflow-net to process tree conversion requires a transition");
  assert(message([&] { return minera::to_powl(bare); }) ==
         "workflow-net to POWL conversion requires a transition");
}

void powl_to_process_tree_preserves_blocks_and_frequencies() {
  const auto sequence = PowlNode::sequence(
      {PowlNode::transition("A"), PowlNode::transition("B")});
  const auto sequence_tree = minera::to_process_tree(sequence);
  assert((sequence_tree.activities() == std::vector<std::string>{"A", "B"}));
  assert(sequence_tree.root().op == Op::kSequence);
  assert(sequence_tree.root().children.size() == 2);
  assert(sequence_tree.root().children[0].activity == 0);
  assert(sequence_tree.root().children[1].activity == 1);

  // pm4py 2.7.22 keeps a FrequentTransition as one leaf with its label.
  const auto optional = minera::to_process_tree(
      PowlNode::transition("A", {0, 1}));
  assert(optional.root().op == Op::kActivity);
  assert((optional.activities() == std::vector<std::string>{"A\n[1,1]"}));

  const auto repeated = minera::to_process_tree(
      PowlNode::transition("A", {1, std::nullopt}));
  assert(repeated.root().op == Op::kActivity);
  assert((repeated.activities() == std::vector<std::string>{"A\n[1,-]"}));

  const auto diamond = PowlNode::strict_partial_order(
      {PowlNode::transition("A"), PowlNode::transition("B"),
       PowlNode::transition("C"), PowlNode::transition("D")},
      {{0, 1}, {0, 2}, {1, 3}, {2, 3}});
  const auto diamond_tree = minera::to_process_tree(diamond);
  assert(diamond_tree.root().op == Op::kSequence);
  assert(diamond_tree.root().children.size() == 3);
  assert(diamond_tree.root().children[1].op == Op::kParallel);
  assert(diamond_tree.root().children[1].children.size() == 2);

  // Disconnected nodes become parallel components, while a sparse adjacent
  // layer remains deterministic and block-structured.
  const auto sparse = PowlNode::strict_partial_order(
      {PowlNode::transition("A"), PowlNode::transition("B"),
       PowlNode::transition("C"), PowlNode::transition("D"),
       PowlNode::transition("E")},
      {{0, 1}, {0, 2}, {3, 2}});
  const auto sparse_tree = minera::to_process_tree(sparse);
  assert(sparse_tree.root().op == Op::kParallel);
  assert(sparse_tree.root().children.size() == 2);

  const auto round_trip = minera::to_powl(sequence_tree);
  assert(round_trip.kind() == minera::PowlNodeKind::kStrictPartialOrder);
  assert(round_trip.has_edge(0, 1));
}

void petri_conversion_sound_and_deterministic() {
  const auto check = [](const PowlNode& node, const std::set<Trace>& expected,
                        std::size_t max_visible_length = 3) {
    const auto net = minera::to_petri_net(node);
    assert(net.place_count() > 1);
    assert(!net.transitions().empty());
    for (const auto& t : net.transitions()) assert(!t.inputs.empty());
    assert(minera::final_marking_reachable(net));
    const auto language = enumerate_language(net, max_visible_length);
    assert(language.traces == expected);
    assert(language.live_transitions.size() == net.transitions().size());
    return net;
  };
  const auto empty = check(PowlNode::transition(""), traces({t({""})}));
  assert(empty.activities().size() == 1 && empty.activities()[0].empty());
  check(PowlNode::silent(), traces({t({})}));
  const auto exactly_once = check(PowlNode::transition("A", {1, 1}),
                                  traces({t({"A"})}));
  assert(exactly_once.transitions().size() == 1);
  // As the reference FrequentTransition: one visible transition whose label
  // carries the frequency, "<label>\n[1,<max or ->]".
  const auto optional = check(PowlNode::transition("A", {0, 1}), traces({t({"A\n[1,1]"})}));
  assert(optional.transitions().size() == 1);
  check(PowlNode::transition("A", {1, std::nullopt}), traces({t({"A\n[1,-]"})}));
  check(PowlNode::transition("A", {0, std::nullopt}), traces({t({"A\n[1,-]"})}));
  // Reference structure: X(a,b,c) has 4 places and 5 transitions, and the
  // loop *(a, X(tau, b)) keeps its silent redo return (5 places, 6
  // transitions), as pm4py 2.7.22 convert_to_petri_net builds them.
  {
    const auto x3 = minera::to_petri_net(PowlNode::xor_node(
        {PowlNode::transition("a"), PowlNode::transition("b"), PowlNode::transition("c")}));
    assert(x3.place_count() == 4 && x3.transitions().size() == 5);
    const auto loop = minera::to_petri_net(PowlNode::loop(
        PowlNode::transition("a"),
        PowlNode::xor_node({PowlNode::silent(), PowlNode::transition("b")})));
    assert(loop.place_count() == 5 && loop.transitions().size() == 6);
  }
  check(PowlNode::xor_node({PowlNode::transition("A"), PowlNode::transition("B")} ),
        traces({t({"A"}), t({"B"})}));
  check(PowlNode::loop(PowlNode::transition("A"), PowlNode::transition("B")),
        traces({t({"A"}), t({"A", "B", "A"})}));
  check(PowlNode::sequence({PowlNode::transition("A"), PowlNode::transition("B")} ),
        traces({t({"A", "B"})}));
  check(PowlNode::concurrency({PowlNode::transition("A"), PowlNode::transition("B")} ),
        traces({t({"A", "B"}), t({"B", "A"})}));
  const auto diamond = PowlNode::strict_partial_order(
      {PowlNode::transition("A"), PowlNode::transition("B"),
       PowlNode::transition("C"), PowlNode::transition("D")},
      {{0, 1}, {0, 2}, {1, 3}, {2, 3}});
  assert(diamond.has_edge(0, 3));
  const auto first = check(
      diamond, traces({t({"A", "B", "C", "D"}),
                       t({"A", "C", "B", "D"})}), 4);
  const auto second = check(
      diamond, traces({t({"A", "B", "C", "D"}),
                       t({"A", "C", "B", "D"})}), 4);
  assert(first.activities() == second.activities());
  assert(first.place_count() == second.place_count());
  assert(first.transitions().size() == second.transitions().size());
}

// Shape of a converted net up to place renaming: the place count and, per
// transition, its label and its input/output places written as "s" (source),
// "f" (sink) or "p" (inner).
struct NetShape {
  std::size_t places = 0;
  std::multiset<std::string> transitions;
  bool operator==(const NetShape&) const = default;
};

NetShape net_shape(const minera::PetriNet& net) {
  NetShape shape;
  shape.places = net.place_count();
  const auto roles = [&](const std::vector<std::size_t>& places) {
    std::multiset<char> sorted;
    for (const auto place : places)
      sorted.insert(place == net.source() ? 's' : place == net.sink() ? 'f' : 'p');
    return std::string(sorted.begin(), sorted.end());
  };
  for (const auto& transition : net.transitions()) {
    const std::string label =
        transition.activity ? net.activities()[*transition.activity] : "tau";
    shape.transitions.insert(label + "|" + roles(transition.inputs) + ">" +
                             roles(transition.outputs));
  }
  return shape;
}

// pm4py 2.7.22 convert_to_petri_net gives an empty partial order a silent
// split with no outputs and a silent join with no inputs, wherever it is
// nested; the simple reduction then folds whatever it can around them.
void empty_partial_order_matches_reference_nets() {
  const auto empty = [] { return PowlNode::strict_partial_order({}, {}); };
  const auto a = [] { return PowlNode::transition("A"); };
  const auto shape = [](std::size_t places,
                        std::initializer_list<std::string> transitions) {
    return NetShape{places, std::multiset<std::string>(transitions)};
  };
  assert(net_shape(minera::to_petri_net(empty())) ==
         shape(2, {"tau|s>", "tau|>f"}));
  assert(net_shape(minera::to_petri_net(PowlNode::parse("PO=(nodes={}, order={})"))) ==
         shape(2, {"tau|s>", "tau|>f"}));
  assert(net_shape(minera::to_petri_net(
             PowlNode::strict_partial_order({empty()}, {}))) ==
         shape(2, {"tau|s>", "tau|>f"}));
  assert(net_shape(minera::to_petri_net(PowlNode::xor_node({a(), empty()}))) ==
         shape(4, {"A|p>p", "tau|>p", "tau|s>p", "tau|p>", "tau|p>f"}));
  assert(net_shape(minera::to_petri_net(
             PowlNode::strict_partial_order({a(), empty()}, {{0, 1}}))) ==
         shape(2, {"A|s>", "tau|>f"}));
  assert(net_shape(minera::to_petri_net(
             PowlNode::strict_partial_order({empty(), a()}, {{0, 1}}))) ==
         shape(2, {"tau|s>", "A|>f"}));
  assert(net_shape(minera::to_petri_net(
             PowlNode::strict_partial_order({a(), empty()}, {}))) ==
         shape(2, {"A|s>f"}));
  assert(net_shape(minera::to_petri_net(PowlNode::loop(empty(), a()))) ==
         shape(4, {"A|p>p", "tau|>p", "tau|s>p", "tau|p>", "tau|p>f"}));
  assert(net_shape(minera::to_petri_net(PowlNode::loop(a(), empty()))) ==
         shape(4, {"A|p>p", "tau|>p", "tau|s>p", "tau|p>", "tau|p>f"}));
}

// The text form T("label",min,max|*) accepts the bounds pm4py's
// FrequentTransition takes: min 0 is skippable, an unbounded max
// self-loops, and other bounds only change the label.
void frequency_bounds_follow_reference_flags() {
  struct Case {
    std::string text;
    bool skippable;
    bool self_loop;
    std::string label;
  };
  for (const auto& c : std::vector<Case>{
           {"T(\"A\",0,3)", true, false, "A\n[1,3]"},
           {"T(\"A\",2,*)", false, true, "A\n[1,-]"},
           {"T(\"A\",2,5)", false, false, "A"},
           {"T(\"A\",5,*)", false, true, "A\n[1,-]"},
           {"T(\"A\",0,0)", true, false, "A\n[1,0]"},
           {"T(\"A\",3,1)", false, false, "A"},
           {"T(\"A\",0,1)", true, false, "A\n[1,1]"},
           {"T(\"A\",1,*)", false, true, "A\n[1,-]"},
           {"T(\"A\",1,1)", false, false, "A"}}) {
    const auto node = PowlNode::parse(c.text);
    assert(node.skippable() == c.skippable);
    assert(node.self_loop() == c.self_loop);
    assert(PowlNode::parse(node.serialize()) == node);
    const auto net = minera::to_petri_net(node);
    assert(net_shape(net) == (NetShape{2, {c.label + "|s>f"}}));
    const auto tree = minera::to_process_tree(node);
    assert(tree.root().op == Op::kActivity);
    assert((tree.activities() == std::vector<std::string>{c.label}));
  }
  const auto nested = minera::to_petri_net(
      PowlNode::parse("X ( T(\"A\",0,3), T(\"B\",2,*) )"));
  assert(net_shape(nested) ==
         (NetShape{4, {"tau|s>p", "A\n[1,3]|p>p", "B\n[1,-]|p>p", "tau|p>f"}}));
}

void workflow_net_to_process_tree_is_strict() {
  const std::vector<PowlNode> models{
      PowlNode::xor_node(
          {PowlNode::transition("A"), PowlNode::transition("B")}),
      PowlNode::sequence(
          {PowlNode::transition("A"),
           PowlNode::concurrency({PowlNode::transition("B"),
                                  PowlNode::transition("C")}),
           PowlNode::transition("D")}),
      PowlNode::loop(PowlNode::sequence({PowlNode::transition("A"),
                                         PowlNode::transition("B")}),
                     PowlNode::transition("C")),
  };

  for (const auto& model : models) {
    const auto source = minera::to_petri_net(model);
    const auto tree = minera::to_process_tree(source);
    const auto recovered = minera::to_petri_net(tree);
    assert(enumerate_language(recovered, 6, 160).traces ==
           enumerate_language(source, 6, 160).traces);
    assert(minera::final_marking_reachable(recovered));
  }

  // A net that is one transition once its silent chain is contracted is
  // rejected, as the reference raises on it.
  for (const auto& model : {PowlNode::silent(), PowlNode::transition("A"),
                            PowlNode::transition("A", {0, 1}),
                            PowlNode::transition("A", {1, std::nullopt})}) {
    bool single = false;
    try {
      (void)minera::to_process_tree(minera::to_petri_net(model));
    } catch (const std::invalid_argument&) {
      single = true;
    }
    assert(single);
  }

  // The N-shaped order is not series-parallel and therefore has no exact
  // block-structured process-tree representation.
  const auto non_block = PowlNode::strict_partial_order(
      {PowlNode::transition("A"), PowlNode::transition("B"),
       PowlNode::transition("C"), PowlNode::transition("D")},
      {{0, 2}, {1, 2}, {1, 3}});
  bool rejected = false;
  try {
    (void)minera::to_process_tree(minera::to_petri_net(non_block));
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);

  bool rejected_extended = false;
  try {
    minera::PetriNet extended;
    const auto source = extended.add_place();
    const auto sink = extended.add_place();
    const auto transition = extended.add_transition(0, {source}, {sink});
    extended.set_activities({"A"});
    extended.set_source(source);
    extended.set_sink(sink);
    extended.add_inhibitor_arc(transition, source);
    (void)minera::to_process_tree(extended);
  } catch (const std::invalid_argument&) {
    rejected_extended = true;
  }
  assert(rejected_extended);
}

// Exported POWL PTML (structured discovery, /api/ptml?miner=powl and
// minera_export --powl-ptml) converts the POWL model directly, as pm4py's
// convert_to_process_tree(powl): a residual partial order is layered
// (a->c, b->c, b->d gives ->(+('a','b'), +('c','d'))) and a one-activity
// model is the leaf 'a', where the reduction of its one-transition workflow
// net raises.
void powl_ptml_converts_the_model_directly() {
  using Op = minera::ProcessTreeOperator;
  const auto block = PowlNode::loop(
      PowlNode::sequence(
          {PowlNode::transition("A"), PowlNode::transition("B")}),
      PowlNode::transition("C"));
  const auto direct = minera::to_process_tree(block);
  const auto strict = minera::to_process_tree(minera::to_petri_net(block));
  assert(enumerate_language(minera::to_petri_net(direct), 6, 160).traces ==
         enumerate_language(minera::to_petri_net(strict), 6, 160).traces);

  const auto non_block = PowlNode::strict_partial_order(
      {PowlNode::transition("A"), PowlNode::transition("B"),
       PowlNode::transition("C"), PowlNode::transition("D")},
      {{0, 2}, {1, 2}, {1, 3}});
  const auto layered = minera::to_process_tree(non_block);
  assert(layered.root().op == Op::kSequence);
  assert(layered.root().children.size() == 2);
  assert(layered.root().children[0].op == Op::kParallel);
  assert(layered.root().children[1].op == Op::kParallel);
  bool rejected = false;
  try {
    (void)minera::to_process_tree(minera::to_petri_net(non_block));
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);

  // The POWL of a one-activity log ([<a>, <a>]).
  const auto single = PowlNode::transition("a");
  const auto leaf = minera::to_process_tree(single);
  assert(leaf.root().op == Op::kActivity);
  assert(leaf.activities().size() == 1 && leaf.activities().front() == "a");
  assert(minera::to_ptml(leaf).find("name=\"a\"") != std::string::npos);
  bool net_rejected = false;
  try {
    (void)minera::to_process_tree(minera::to_petri_net(single));
  } catch (const std::invalid_argument&) {
    net_rejected = true;
  }
  assert(net_rejected);
}

void petri_to_powl_round_trips_structured_nets() {
  const std::vector<PowlNode> models{
      PowlNode::silent(),
      PowlNode::transition("A"),
      PowlNode::transition("A", {0, 1}),
      PowlNode::transition("A", {1, std::nullopt}),
      PowlNode::xor_node(
          {PowlNode::transition("A"), PowlNode::transition("B")}),
      PowlNode::loop(PowlNode::transition("A"), PowlNode::transition("B")),
      PowlNode::sequence(
          {PowlNode::transition("A"), PowlNode::transition("B")}),
      PowlNode::sequence(
          {PowlNode::xor_node({PowlNode::transition("A"),
                               PowlNode::transition("B")}),
           PowlNode::transition("C")}),
      PowlNode::loop(
          PowlNode::sequence(
              {PowlNode::transition("A"), PowlNode::transition("B")}),
          PowlNode::transition("C")),
      PowlNode::concurrency(
          {PowlNode::transition("A"), PowlNode::transition("B")}),
      PowlNode::strict_partial_order(
          {PowlNode::transition("A"), PowlNode::transition("B"),
           PowlNode::transition("C"), PowlNode::transition("D")},
          {{0, 1}, {0, 2}, {1, 3}, {2, 3}}),
  };

  for (const auto& model : models) {
    const auto source_net = minera::to_petri_net(model);
    const auto recovered = minera::to_powl(source_net);
    const auto recovered_net = minera::to_petri_net(recovered);
    const auto expected = enumerate_language(source_net, 6, 160).traces;
    const auto actual = enumerate_language(recovered_net, 6, 160).traces;
    assert(actual == expected);
    assert(minera::final_marking_reachable(recovered_net));
    // A frequent transition is one decorated visible transition in the net,
    // so it comes back as a plain transition with that label.
    if (model.kind() == minera::PowlNodeKind::kTransition &&
        (model.self_loop() || model.skippable())) {
      assert(recovered.kind() == minera::PowlNodeKind::kTransition);
      assert(recovered.label() == (model.self_loop() ? "A\n[1,-]" : "A\n[1,1]"));
    }
  }

  bool rejected_extended = false;
  try {
    minera::PetriNet extended;
    const auto source = extended.add_place();
    const auto sink = extended.add_place();
    const auto transition = extended.add_transition(0, {source}, {sink});
    extended.set_activities({"A"});
    extended.set_source(source);
    extended.set_sink(sink);
    extended.add_reset_arc(transition, source);
    (void)minera::to_powl(extended);
  } catch (const std::invalid_argument&) {
    rejected_extended = true;
  }
  assert(rejected_extended);

  bool rejected_non_workflow = false;
  try {
    minera::PetriNet malformed;
    const auto first_source = malformed.add_place();
    const auto second_source = malformed.add_place();
    const auto sink = malformed.add_place();
    malformed.add_transition(std::nullopt, {first_source}, {sink});
    malformed.add_transition(std::nullopt, {second_source}, {sink});
    (void)minera::to_powl(malformed);
  } catch (const std::invalid_argument&) {
    rejected_non_workflow = true;
  }
  assert(rejected_non_workflow);

  bool rejected_unsound = false;
  try {
    minera::PetriNet unsound;
    const auto source = unsound.add_place();
    const auto first = unsound.add_place();
    const auto second = unsound.add_place();
    const auto sink = unsound.add_place();
    unsound.set_activities({"A", "B", "C"});
    unsound.add_transition(0, {source}, {first});
    unsound.add_transition(1, {source}, {second});
    unsound.add_transition(2, {first, second}, {sink});
    unsound.set_source(source);
    unsound.set_sink(sink);
    (void)minera::to_powl(unsound);
  } catch (const std::invalid_argument&) {
    rejected_unsound = true;
  }
  assert(rejected_unsound);

  bool rejected_sink_escape = false;
  try {
    minera::PetriNet malformed;
    const auto source = malformed.add_place();
    const auto sink = malformed.add_place();
    const auto tail = malformed.add_place();
    malformed.set_activities({"A"});
    malformed.add_transition(0, {source}, {sink});
    malformed.add_transition(std::nullopt, {sink}, {tail});
    malformed.set_source(source);
    malformed.set_sink(sink);
    (void)minera::to_powl(malformed);
  } catch (const std::invalid_argument&) {
    rejected_sink_escape = true;
  }
  assert(rejected_sink_escape);
}

void workflow_boundaries_are_inferred_and_normalized() {
  // A net assembled by a caller that does not carry boundary metadata still
  // has an unambiguous structural source and sink. The conversion must not
  // write those inferred values back into the caller's object.
  minera::PetriNet inferred;
  const auto source = inferred.add_place();
  const auto middle = inferred.add_place();
  const auto sink = inferred.add_place();
  inferred.set_activities({"A", "B"});
  inferred.add_transition(0, {source}, {middle});
  inferred.add_transition(1, {middle}, {sink});
  const auto recovered = minera::to_powl(inferred);
  assert(recovered.kind() == minera::PowlNodeKind::kStrictPartialOrder);
  assert(recovered.children().size() == 2);
  assert(recovered.has_edge(0, 1));
  assert(inferred.source() == 0 && inferred.sink() == 0);

  // Locally duplicate places are treated as one incidence boundary. This is
  // common in imported nets where an equivalent place was retained by a
  // serializer, and must not create a spurious partial-order branch.
  minera::PetriNet duplicate_places;
  const auto duplicate_source = duplicate_places.add_place();
  const auto first = duplicate_places.add_place();
  const auto second = duplicate_places.add_place();
  const auto duplicate_sink = duplicate_places.add_place();
  duplicate_places.set_activities({"A", "B"});
  duplicate_places.add_transition(0, {duplicate_source}, {first, second});
  duplicate_places.add_transition(1, {first, second}, {duplicate_sink});
  const auto duplicate_model = minera::to_powl(duplicate_places);
  assert(duplicate_model.kind() == minera::PowlNodeKind::kStrictPartialOrder);
  assert(duplicate_model.children().size() == 2);
  assert(duplicate_model.has_edge(0, 1));

  // Silent entry/exit wrappers are routing details, not POWL activities.
  minera::PetriNet wrapped;
  const auto wrapped_source = wrapped.add_place();
  const auto wrapped_entry = wrapped.add_place();
  const auto wrapped_exit = wrapped.add_place();
  const auto wrapped_sink = wrapped.add_place();
  wrapped.set_activities({"A"});
  wrapped.add_transition(std::nullopt, {wrapped_source}, {wrapped_entry});
  wrapped.add_transition(0, {wrapped_entry}, {wrapped_exit});
  wrapped.add_transition(std::nullopt, {wrapped_exit}, {wrapped_sink});
  const auto wrapped_model = minera::to_powl(wrapped);
  assert(wrapped_model == PowlNode::transition("A"));

  bool rejected_multiple_sources = false;
  try {
    minera::PetriNet ambiguous;
    const auto first_source = ambiguous.add_place();
    const auto second_source = ambiguous.add_place();
    const auto end = ambiguous.add_place();
    ambiguous.set_activities({"A", "B"});
    ambiguous.add_transition(0, {first_source}, {end});
    ambiguous.add_transition(1, {second_source}, {end});
    (void)minera::to_powl(ambiguous);
  } catch (const std::invalid_argument&) {
    rejected_multiple_sources = true;
  }
  assert(rejected_multiple_sources);

  bool rejected_multiple_sinks = false;
  try {
    minera::PetriNet ambiguous;
    const auto begin = ambiguous.add_place();
    const auto first_sink = ambiguous.add_place();
    const auto second_sink = ambiguous.add_place();
    ambiguous.set_activities({"A", "B"});
    ambiguous.add_transition(0, {begin}, {first_sink});
    ambiguous.add_transition(1, {begin}, {second_sink});
    (void)minera::to_powl(ambiguous);
  } catch (const std::invalid_argument&) {
    rejected_multiple_sinks = true;
  }
  assert(rejected_multiple_sinks);

  // Explicitly malformed boundary metadata must not be mistaken for the
  // unset default. Structural inference is reserved for nets that assign
  // neither boundary.
  bool rejected_equal_metadata = false;
  try {
    minera::PetriNet malformed;
    const auto begin = malformed.add_place();
    const auto middle = malformed.add_place();
    const auto end = malformed.add_place();
    malformed.set_activities({"A", "B"});
    malformed.add_transition(0, {begin}, {middle});
    malformed.add_transition(1, {middle}, {end});
    malformed.set_source(middle);
    malformed.set_sink(middle);
    (void)minera::to_powl(malformed);
  } catch (const std::invalid_argument&) {
    rejected_equal_metadata = true;
  }
  assert(rejected_equal_metadata);

  bool rejected_partial_metadata = false;
  try {
    minera::PetriNet malformed;
    const auto begin = malformed.add_place();
    const auto end = malformed.add_place();
    malformed.set_activities({"A"});
    malformed.add_transition(0, {begin}, {end});
    malformed.set_source(begin);
    (void)minera::to_powl(malformed);
  } catch (const std::invalid_argument&) {
    rejected_partial_metadata = true;
  }
  assert(rejected_partial_metadata);
}

// Reference workflow nets (captured by
// scripts/capture-workflow-net-tree-reference.py: tree-to-net and BPMN
// conversions, a net with two loops sharing an exit place, and seeded
// heuristics nets) must convert to a tree with the reference
// language; nets the reference cannot convert must be rejected, never
// approximated. Concurrent choices, skips and loops used to be flattened into
// a plain parallel block, and a loop after a sequence prefix was rejected.
void workflow_net_to_process_tree_matches_reference() {
  for (const auto& reference : kReferenceWorkflowCases) {
    const auto document = minera::parse_pnml_document(reference.pnml);
    const auto tree = minera::to_process_tree(document.net);
    const auto traces =
        enumerate_language(minera::to_petri_net(tree), 6, 400).traces;
    if (traces != reference.traces) {
      std::cerr << "language mismatch for " << reference.name << "\n";
      assert(false);
    }
  }
  for (const auto& pnml : kReferenceRejectedWorkflowNets) {
    const auto document = minera::parse_pnml_document(pnml);
    bool rejected = false;
    try {
      (void)minera::to_process_tree(document.net);
    } catch (const std::invalid_argument&) {
      rejected = true;
    }
    assert(rejected);
  }
}

// Reference workflow nets of discovered POWL models (captured by
// scripts/capture-workflow-net-powl-reference.py) whose partial orders over
// choice and loop nodes are not series-parallel: the reference converts them
// to POWL but not to a process tree. The POWL conversion must keep their
// language and the process-tree conversion must still reject them. They used
// to be rejected because partial orders were only built over single
// transitions.
void workflow_net_to_powl_matches_reference() {
  for (const auto& reference : kReferencePowlWorkflowCases) {
    const auto document = minera::parse_pnml_document(reference.pnml);
    const auto powl = minera::to_powl(document.net);
    const auto traces =
        enumerate_language(minera::to_petri_net(powl), 4, 400).traces;
    if (traces != reference.traces) {
      std::cerr << "POWL language mismatch for " << reference.name << "\n";
      assert(false);
    }
    bool rejected = false;
    try {
      (void)minera::to_process_tree(document.net);
    } catch (const std::invalid_argument&) {
      rejected = true;
    }
    assert(rejected);
  }
  // The reference converts these nets to a process tree but not to POWL: a
  // loop's redo part leaves from the place the sequence continues from, or a
  // loop's do part repeats through its end place.
  for (const auto& reference : kReferencePowlRejectedTreeCases) {
    const auto document = minera::parse_pnml_document(reference.pnml);
    bool rejected = false;
    try {
      (void)minera::to_powl(document.net);
    } catch (const std::invalid_argument&) {
      rejected = true;
    }
    if (!rejected) {
      std::cerr << "POWL conversion accepted " << reference.name << "\n";
      assert(false);
    }
    const auto tree = minera::to_process_tree(document.net);
    const auto traces =
        enumerate_language(minera::to_petri_net(tree), 4, 400).traces;
    if (traces != reference.traces) {
      std::cerr << "tree language mismatch for " << reference.name << "\n";
      assert(false);
    }
  }
}

std::string canonical_tree(const ProcessTree& tree, const ProcessTreeNode& node) {
  switch (node.op) {
    case Op::kActivity: return "'" + tree.activities().at(node.activity) + "'";
    case Op::kTau: return "tau";
    default: break;
  }
  std::vector<std::string> children;
  for (const auto& child : node.children) children.push_back(canonical_tree(tree, child));
  std::string symbol;
  if (node.op == Op::kSequence) symbol = "->";
  else if (node.op == Op::kExclusive) symbol = "X";
  else if (node.op == Op::kParallel) symbol = "+";
  else if (node.op == Op::kLoop) symbol = "*";
  else symbol = "?";
  if (node.op == Op::kExclusive || node.op == Op::kParallel)
    std::sort(children.begin(), children.end());
  std::string text = symbol + "(";
  for (std::size_t index = 0; index < children.size(); ++index)
    text += (index ? "," : "") + children[index];
  return text + ")";
}

// convert_to_process_tree(convert_to_petri_net(tree)) of pm4py 2.7.22 (see
// scripts/capture-net-to-tree-shape-reference.py): the reference folds the
// tree, so nested sequences flatten, an exclusive choice keeps one tau and a
// sequence or parallel drops taus, and it raises on a net that is a single
// transition once its silent chain is contracted. Native used to keep nested
// sequences and repeated taus and to return a leaf for those chains.
void workflow_net_to_process_tree_shapes_match_reference() {
  for (const auto& reference : kReferenceTreeShapeCases) {
    const auto document = minera::parse_pnml_document(reference.pnml);
    std::string actual;
    try {
      const auto tree = minera::to_process_tree(document.net);
      actual = canonical_tree(tree, tree.root());
    } catch (const std::invalid_argument&) {
      actual = "!";
    }
    const auto expected = reference.expected.front() == '!' ? std::string("!") : reference.expected;
    if (actual != expected) {
      std::cerr << "tree shape mismatch for " << reference.name << ": " << actual
                << " vs " << reference.expected << "\n";
      assert(false);
    }
  }
}

}  // namespace

// A wide partial order of loop and choice nodes chained in N shapes is not
// series-parallel, so every node is a reduced region of one partial order.
// The region search must stay polynomial of low degree: this net once took
// about 25 seconds to convert.
void workflow_net_partial_order_regions_convert_quickly() {
  constexpr std::size_t kNodes = 40;
  std::vector<PowlNode> nodes;
  for (std::size_t index = 0; index < kNodes; ++index) {
    const auto label = "a" + std::to_string(index);
    if (index % 2)
      nodes.push_back(PowlNode::xor_node(
          {PowlNode::transition(label), PowlNode::silent()}));
    else
      nodes.push_back(PowlNode::loop(PowlNode::transition(label),
                                     PowlNode::transition("r" + std::to_string(index))));
  }
  std::vector<std::pair<std::size_t, std::size_t>> order;
  for (std::size_t index = 0; index + 3 < kNodes; index += 2) {
    order.emplace_back(index, index + 2);
    order.emplace_back(index, index + 3);
    order.emplace_back(index + 1, index + 3);
  }
  const auto net =
      minera::to_petri_net(PowlNode::strict_partial_order(std::move(nodes), order));
  const auto started = std::chrono::steady_clock::now();
  const auto model = minera::to_powl(net);
  assert(model.kind() == minera::PowlNodeKind::kStrictPartialOrder);
  bool rejected = false;
  try {
    (void)minera::to_process_tree(net);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
  const auto elapsed = std::chrono::steady_clock::now() - started;
  assert(elapsed < std::chrono::seconds(5));
}

int main() {
  process_tree_to_powl_variations();
  powl_to_process_tree_preserves_blocks_and_frequencies();
  petri_conversion_sound_and_deterministic();
  empty_partial_order_matches_reference_nets();
  frequency_bounds_follow_reference_flags();
  workflow_net_to_process_tree_is_strict();
  powl_ptml_converts_the_model_directly();
  petri_to_powl_round_trips_structured_nets();
  workflow_boundaries_are_inferred_and_normalized();
  workflow_net_to_process_tree_matches_reference();
  workflow_net_to_powl_matches_reference();
  workflow_net_partial_order_regions_convert_quickly();
  workflow_net_to_process_tree_shapes_match_reference();
  std::cout << "POWL conversion tests passed\n";
}
