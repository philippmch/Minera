#include "minera/powl_conversion.hpp"
#include "minera/conformance.hpp"

#include <cassert>
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

  const auto optional = minera::to_process_tree(
      PowlNode::transition("A", {0, 1}));
  assert(optional.root().op == Op::kExclusive);
  assert(optional.root().children.size() == 2);
  assert(optional.root().children[0].op == Op::kTau);
  assert(optional.root().children[1].op == Op::kActivity);

  const auto repeated = minera::to_process_tree(
      PowlNode::transition("A", {1, std::nullopt}));
  assert(repeated.root().op == Op::kLoop);
  assert(repeated.root().children.size() == 2);
  assert(repeated.root().children[0].op == Op::kActivity);
  assert(repeated.root().children[1].op == Op::kTau);

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
  check(PowlNode::transition("A", {0, 1}), traces({t({}), t({"A"})}));
  check(PowlNode::transition("A", {1, std::nullopt}),
        traces({t({"A"}), t({"A", "A"}), t({"A", "A", "A"})}));
  check(PowlNode::transition("A", {0, std::nullopt}),
        traces({t({}), t({"A"}), t({"A", "A"}),
                t({"A", "A", "A"})}));
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

void workflow_net_to_process_tree_is_strict() {
  const std::vector<PowlNode> models{
      PowlNode::silent(),
      PowlNode::transition("A"),
      PowlNode::transition("A", {0, 1}),
      PowlNode::transition("A", {1, std::nullopt}),
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
    if (model.kind() == minera::PowlNodeKind::kTransition &&
        model.self_loop()) {
      assert(recovered.kind() == minera::PowlNodeKind::kLoop);
    }
    if (model.kind() == minera::PowlNodeKind::kTransition &&
        model.skippable() && !model.self_loop()) {
      assert(recovered.kind() == minera::PowlNodeKind::kXor);
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

}  // namespace

int main() {
  process_tree_to_powl_variations();
  powl_to_process_tree_preserves_blocks_and_frequencies();
  petri_conversion_sound_and_deterministic();
  workflow_net_to_process_tree_is_strict();
  petri_to_powl_round_trips_structured_nets();
  std::cout << "POWL conversion tests passed\n";
}
