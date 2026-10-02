#include "minera/petri_net.hpp"
#include "minera/heuristic_miner.hpp"
#include "minera/petri_analysis.hpp"
#include "minera/json_export.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cmath>
#include <cstdint>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using Marking = std::vector<std::size_t>;
using Trace = std::vector<std::string>;

struct Language {
  std::set<Trace> traces;
};

Language enumerate(const minera::PetriNet& net, std::size_t max_visible = 5,
                   std::size_t max_firings = 64) {
  Language result;
  Marking initial(net.place_count(), 0);
  initial[net.source()] = 1;
  Marking final(net.place_count(), 0);
  final[net.sink()] = 1;
  std::set<std::pair<Marking, Trace>> path;

  const auto visit = [&](const auto& self, const Marking& marking,
                         const Trace& trace, std::size_t firings) -> void {
    if (firings > max_firings || trace.size() > max_visible) return;
    if (marking == final) result.traces.insert(trace);
    if (!path.emplace(marking, trace).second) return;
    for (const auto& transition : net.transitions()) {
      std::vector<std::size_t> required(net.place_count(), 0);
      bool enabled = true;
      for (const auto place : transition.inputs) {
        if (place >= marking.size() || ++required[place] > marking[place]) {
          enabled = false;
          break;
        }
      }
      if (!enabled) continue;
      Marking next = marking;
      for (const auto place : transition.inputs) --next[place];
      for (const auto place : transition.outputs) ++next[place];
      Trace next_trace = trace;
      if (transition.activity) {
        if (*transition.activity >= net.activities().size()) continue;
        next_trace.push_back(net.activities()[*transition.activity]);
      }
      self(self, next, next_trace, firings + 1);
    }
    path.erase({marking, trace});
  };

  visit(visit, initial, {}, 0);
  return result;
}

bool contains(const std::vector<std::size_t>& values, std::size_t value) {
  for (const auto candidate : values) {
    if (candidate == value) return true;
  }
  return false;
}

std::size_t silent_count(const minera::PetriNet& net) {
  std::size_t count = 0;
  for (const auto& transition : net.transitions()) {
    if (transition.silent()) ++count;
  }
  return count;
}

minera::ProcessTreeNode node(minera::ProcessTreeOperator op,
                             std::vector<minera::ProcessTreeNode> children) {
  return minera::ProcessTreeNode{op, 0, std::move(children)};
}

void leaf_and_tau_keep_labels_and_language() {
  const auto activity = minera::to_petri_net_transition_bordered(
      minera::ProcessTree({"A"}, minera::ProcessTreeNode::leaf(0)));
  assert(activity.source() == 0 && activity.sink() == 1);
  assert(activity.activities() == std::vector<std::string>{"A"});
  assert(activity.transitions().size() == 1);
  assert(activity.transitions()[0].activity == 0);
  assert(contains(activity.transitions()[0].inputs, activity.source()));
  assert(contains(activity.transitions()[0].outputs, activity.sink()));
  assert(enumerate(activity).traces == (std::set<Trace>{{"A"}}));

  const auto tau = minera::to_petri_net(
      minera::ProcessTree({}, minera::ProcessTreeNode::tau()),
      minera::ProcessTreePetriNetVariant::kTransitionBordered);
  assert(silent_count(tau) == 1);
  assert(enumerate(tau).traces == (std::set<Trace>{Trace{}}));
}

void composite_fragments_have_silent_borders() {
  const auto sequence = minera::to_petri_net_transition_bordered(
      minera::ProcessTree(
          {"A", "B"},
          node(minera::ProcessTreeOperator::kSequence,
               {minera::ProcessTreeNode::leaf(0),
                minera::ProcessTreeNode::leaf(1)})));
  assert(sequence.source() == 0 && sequence.sink() == 1);
  assert(silent_count(sequence) == 2);
  assert(enumerate(sequence).traces == (std::set<Trace>{{"A", "B"}}));

  const auto choice = minera::to_petri_net(
      minera::ProcessTree(
          {"A", "B"},
          node(minera::ProcessTreeOperator::kExclusive,
               {minera::ProcessTreeNode::leaf(0),
                minera::ProcessTreeNode::leaf(1)})),
      minera::ProcessTreePetriNetVariant::TRANSITION_BORDERED);
  assert(silent_count(choice) == 2);
  assert(enumerate(choice).traces ==
         (std::set<Trace>{{"A"}, {"B"}}));

  const auto parallel = minera::to_petri_net_transition_bordered(
      minera::ProcessTree(
          {"A", "B"},
          node(minera::ProcessTreeOperator::kParallel,
               {minera::ProcessTreeNode::leaf(0),
                minera::ProcessTreeNode::leaf(1)})));
  assert(silent_count(parallel) == 2);
  assert(enumerate(parallel).traces ==
         (std::set<Trace>{{"A", "B"}, {"B", "A"}}));

  bool saw_split = false;
  bool saw_join = false;
  for (const auto& transition : parallel.transitions()) {
    if (transition.silent() && transition.inputs.size() == 1 &&
        transition.outputs.size() == 2) {
      saw_split = true;
    }
    if (transition.silent() && transition.inputs.size() == 2 &&
        transition.outputs.size() == 1) {
      saw_join = true;
    }
  }
  assert(saw_split && saw_join);
}

void binary_loop_preserves_language() {
  const auto loop = minera::to_petri_net_transition_bordered(
      minera::ProcessTree(
          {"A", "B"},
          node(minera::ProcessTreeOperator::kLoop,
               {minera::ProcessTreeNode::leaf(0),
                minera::ProcessTreeNode::leaf(1)})));
  assert(loop.source() == 0 && loop.sink() == 1);
  assert(silent_count(loop) == 2);
  assert(enumerate(loop, 5).traces ==
         (std::set<Trace>{{"A"},
                          {"A", "B", "A"},
                          {"A", "B", "A", "B", "A"}}));
}

void unsupported_and_malformed_trees_are_rejected() {
  const auto inclusive = node(
      minera::ProcessTreeOperator::kInclusiveChoice,
      {minera::ProcessTreeNode::leaf(0), minera::ProcessTreeNode::leaf(1)});
  bool rejected_inclusive = false;
  try {
    (void)minera::to_petri_net_transition_bordered(
        minera::ProcessTree({"A", "B"}, inclusive));
  } catch (const std::invalid_argument&) {
    rejected_inclusive = true;
  }
  assert(rejected_inclusive);

  for (std::size_t arity : {0U, 1U, 3U}) {
    std::vector<minera::ProcessTreeNode> children;
    for (std::size_t i = 0; i < arity; ++i) {
      children.push_back(minera::ProcessTreeNode::leaf(0));
    }
    bool rejected_loop = false;
    try {
      (void)minera::to_petri_net_transition_bordered(
          minera::ProcessTree({"A"}, node(minera::ProcessTreeOperator::kLoop,
                                           std::move(children))));
    } catch (const std::invalid_argument&) {
      rejected_loop = true;
    }
    assert(rejected_loop);
  }

  bool rejected_activity = false;
  try {
    (void)minera::to_petri_net_transition_bordered(
        minera::ProcessTree({"A"}, minera::ProcessTreeNode::leaf(1)));
  } catch (const std::out_of_range&) {
    rejected_activity = true;
  }
  assert(rejected_activity);
}

void explicit_standard_variant_matches_default() {
  const minera::ProcessTree tree(
      {"A", "B"},
      node(minera::ProcessTreeOperator::kSequence,
           {minera::ProcessTreeNode::leaf(0),
            minera::ProcessTreeNode::leaf(1)}));
  const auto default_net = minera::to_petri_net(tree);
  const auto explicit_net = minera::to_petri_net(
      tree, minera::ProcessTreePetriNetConversionVariant::kStandard);
  assert(default_net.place_count() == explicit_net.place_count());
  assert(default_net.source() == explicit_net.source());
  assert(default_net.sink() == explicit_net.sink());
  assert(default_net.activities() == explicit_net.activities());
  assert(default_net.transitions().size() == explicit_net.transitions().size());
}

void variant_names_and_json_are_stable() {
  assert(std::string(minera::process_tree_petri_net_variant_name(
             minera::ProcessTreePetriNetVariant::kStandard)) == "standard");
  assert(std::string(minera::process_tree_petri_net_variant_name(
             minera::ProcessTreePetriNetVariant::kTransitionBordered)) ==
         "transition_bordered");
  assert(minera::parse_process_tree_petri_net_variant("") ==
         minera::ProcessTreePetriNetVariant::kStandard);
  assert(minera::parse_process_tree_petri_net_variant("standard") ==
         minera::ProcessTreePetriNetVariant::kStandard);
  assert(minera::parse_process_tree_petri_net_variant("transition_bordered") ==
         minera::ProcessTreePetriNetVariant::kTransitionBordered);
  bool rejected = false;
  try {
    (void)minera::parse_process_tree_petri_net_variant("unsupported");
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);

  const auto tree = minera::ProcessTree({"A\"\\B"},
                                         minera::ProcessTreeNode::leaf(0));
  const auto net = minera::to_petri_net_transition_bordered(tree);
  const auto json = minera::serialize_process_tree_petri_net_json(
      net, minera::ProcessTreePetriNetVariant::kTransitionBordered);
  assert(json.find("\"family\":\"PROCESS_TREE_PETRI_NET\"") !=
         std::string::npos);
  assert(json.find("\"variant\":\"transition_bordered\"") !=
         std::string::npos);
  assert(json.find("A\\\"\\\\B") != std::string::npos);
}

void classic_dfg_conversion_retains_shared_join_bridge() {
  // Immutable public cases 125/126/127 at dependency threshold zero. Only
  // case 126 needs the multi-producer bridge retained; its workflow boundary
  // places are disconnected because the DFG has no inferred starts or ends.
  minera::HeuristicClassicDfgInput input;
  input.dfg = {{{"A", "C"}, 1}, {{"B", "A"}, 1}, {{"B", "B"}, 1},
               {{"B", "C"}, 1}, {{"C", "A"}, 1}, {{"C", "B"}, 1}};
  minera::HeuristicMinerOptions options;
  options.dependency_threshold = 0.0;
  const auto net = minera::to_petri_net(
      minera::discover_heuristics_net(input, options));
  const auto arc_count = [](const minera::PetriNet& value) {
    std::size_t count = 0;
    for (const auto& transition : value.transitions())
      count += transition.inputs.size() + transition.outputs.size();
    return count;
  };
  assert(net.place_count() == 8);
  assert(net.transitions().size() == 6);
  assert(arc_count(net) == 16);
  assert(std::count_if(net.transitions().begin(), net.transitions().end(),
                       [](const auto& transition) {
                         return transition.silent();
                       }) == 3);
  const auto visible = [&](const std::string& label)
      -> const minera::PetriTransition& {
    const auto found = std::find_if(
        net.transitions().begin(), net.transitions().end(),
        [&](const auto& transition) {
          return transition.activity &&
                 net.activities()[*transition.activity] == label;
        });
    assert(found != net.transitions().end());
    return *found;
  };
  const auto& a = visible("A");
  const auto& b = visible("B");
  const auto& c = visible("C");
  assert(a.inputs.size() == 2 && a.outputs.size() == 1);
  assert(b.inputs.size() == 2 && b.outputs.size() == 1);
  assert(c.inputs.size() == 1 && c.outputs.size() == 1);
  const auto contains = [](const auto& places, std::size_t place) {
    return std::find(places.begin(), places.end(), place) != places.end();
  };
  const auto join = c.inputs.front();
  const auto c_output = c.outputs.front();
  const auto b_output = b.outputs.front();
  assert(a.outputs.front() == join);
  assert(contains(a.inputs, c_output) && contains(b.inputs, c_output));
  const auto a_branch = a.inputs[0] == c_output ? a.inputs[1] : a.inputs[0];
  const auto b_branch = b.inputs[0] == c_output ? b.inputs[1] : b.inputs[0];
  const auto bridge = std::find_if(
      net.transitions().begin(), net.transitions().end(),
      [&](const auto& transition) {
        return transition.silent() && transition.inputs.size() == 1 &&
               transition.outputs == std::vector<std::size_t>{join};
      });
  assert(bridge != net.transitions().end());
  const auto bridge_input = bridge->inputs.front();
  std::set<std::size_t> branch_targets;
  for (const auto& transition : net.transitions()) {
    if (transition.silent() && &transition != &*bridge) {
      assert(transition.inputs == std::vector<std::size_t>{b_output});
      assert(transition.outputs.size() == 2);
      assert(contains(transition.outputs, bridge_input));
      branch_targets.insert(transition.outputs[0] == bridge_input
                                ? transition.outputs[1]
                                : transition.outputs[0]);
    }
    assert(!contains(transition.inputs, net.source()));
    assert(!contains(transition.outputs, net.source()));
    assert(!contains(transition.inputs, net.sink()));
    assert(!contains(transition.outputs, net.sink()));
  }
  assert((branch_targets == std::set<std::size_t>{a_branch, b_branch}));
  const auto markings = minera::default_workflow_markings(net);
  minera::Marking expected_initial(net.place_count(), 0);
  minera::Marking expected_final(net.place_count(), 0);
  expected_initial[net.source()] = 1;
  expected_final[net.sink()] = 1;
  assert(markings.initial == expected_initial);
  assert(markings.final == expected_final);

  auto case125 = input;
  case125.dfg.erase({"C", "B"});
  case125.dfg[{"C", "C"}] = 1;
  const auto control125 = minera::to_petri_net(
      minera::discover_heuristics_net(case125, options));
  assert(control125.place_count() == 7);
  assert(control125.transitions().size() == 5);
  assert(arc_count(control125) == 13);
  auto case127 = input;
  case127.dfg[{"C", "C"}] = 1;
  const auto control127 = minera::to_petri_net(
      minera::discover_heuristics_net(case127, options));
  assert(control127.place_count() == 12);
  assert(control127.transitions().size() == 9);
  assert(arc_count(control127) == 26);
}

void supplied_public_petri_presentations_keep_complete_fields() {
  using namespace minera;
  {
    PetriNetPresentation net;
    net.name = "";
    net.places = {{0, "pre_A\"\\\n\xCE\xA9", {}}, {1, "pre_constructor", {}}, {2, "sink0", {}}, {3, "source0", {}}, {4, "splace_in_toString_A\"\\\n\xCE\xA9", {}}, {5, "splace_in_toString_constructor", {}}};
    net.transitions = {{0, "A\"\\\n\xCE\xA9", "A\"\\\n\xCE\xA9", {}}, {1, "__proto__", "__proto__", {}}, {2, "constructor", "constructor", {}}, {3, "toString", "toString", {}}};
    net.arcs = {{{PetriNodeKind::kPlace, 0}, {PetriNodeKind::kTransition, 0}, 1, {}}, {{PetriNodeKind::kPlace, 1}, {PetriNodeKind::kTransition, 2}, 1, {}}, {{PetriNodeKind::kPlace, 3}, {PetriNodeKind::kTransition, 1}, 1, {}}, {{PetriNodeKind::kPlace, 4}, {PetriNodeKind::kTransition, 3}, 1, {}}, {{PetriNodeKind::kPlace, 5}, {PetriNodeKind::kTransition, 3}, 1, {}}, {{PetriNodeKind::kTransition, 0}, {PetriNodeKind::kPlace, 4}, 1, {}}, {{PetriNodeKind::kTransition, 1}, {PetriNodeKind::kPlace, 0}, 1, {}}, {{PetriNodeKind::kTransition, 1}, {PetriNodeKind::kPlace, 1}, 1, {}}, {{PetriNodeKind::kTransition, 2}, {PetriNodeKind::kPlace, 5}, 1, {}}, {{PetriNodeKind::kTransition, 3}, {PetriNodeKind::kPlace, 2}, 1, {}}};
    net.initial_marking = {{3, 1}};
    net.final_marking = {{2, 1}};
    assert(net.places.size() == 6 && net.transitions.size() == 4 && net.arcs.size() == 10);
    assert(petri_net_presentation_json(net) == "{\"name\":\"\",\"properties\":{},\"places\":[{\"id\":0,\"name\":\"pre_A\\\"\\\\\\n\xCE\xA9\",\"properties\":{}},{\"id\":1,\"name\":\"pre_constructor\",\"properties\":{}},{\"id\":2,\"name\":\"sink0\",\"properties\":{}},{\"id\":3,\"name\":\"source0\",\"properties\":{}},{\"id\":4,\"name\":\"splace_in_toString_A\\\"\\\\\\n\xCE\xA9\",\"properties\":{}},{\"id\":5,\"name\":\"splace_in_toString_constructor\",\"properties\":{}}],\"transitions\":[{\"id\":0,\"name\":\"A\\\"\\\\\\n\xCE\xA9\",\"label\":\"A\\\"\\\\\\n\xCE\xA9\",\"properties\":{}},{\"id\":1,\"name\":\"__proto__\",\"label\":\"__proto__\",\"properties\":{}},{\"id\":2,\"name\":\"constructor\",\"label\":\"constructor\",\"properties\":{}},{\"id\":3,\"name\":\"toString\",\"label\":\"toString\",\"properties\":{}}],\"arcs\":[{\"source\":{\"kind\":\"place\",\"id\":0},\"target\":{\"kind\":\"transition\",\"id\":0},\"weight\":1,\"properties\":{}},{\"source\":{\"kind\":\"place\",\"id\":1},\"target\":{\"kind\":\"transition\",\"id\":2},\"weight\":1,\"properties\":{}},{\"source\":{\"kind\":\"place\",\"id\":3},\"target\":{\"kind\":\"transition\",\"id\":1},\"weight\":1,\"properties\":{}},{\"source\":{\"kind\":\"place\",\"id\":4},\"target\":{\"kind\":\"transition\",\"id\":3},\"weight\":1,\"properties\":{}},{\"source\":{\"kind\":\"place\",\"id\":5},\"target\":{\"kind\":\"transition\",\"id\":3},\"weight\":1,\"properties\":{}},{\"source\":{\"kind\":\"transition\",\"id\":0},\"target\":{\"kind\":\"place\",\"id\":4},\"weight\":1,\"properties\":{}},{\"source\":{\"kind\":\"transition\",\"id\":1},\"target\":{\"kind\":\"place\",\"id\":0},\"weight\":1,\"properties\":{}},{\"source\":{\"kind\":\"transition\",\"id\":1},\"target\":{\"kind\":\"place\",\"id\":1},\"weight\":1,\"properties\":{}},{\"source\":{\"kind\":\"transition\",\"id\":2},\"target\":{\"kind\":\"place\",\"id\":5},\"weight\":1,\"properties\":{}},{\"source\":{\"kind\":\"transition\",\"id\":3},\"target\":{\"kind\":\"place\",\"id\":2},\"weight\":1,\"properties\":{}}],\"initial_marking\":[{\"place_id\":3,\"tokens\":1}],\"final_marking\":[{\"place_id\":2,\"tokens\":1}]}");
  }
  {
    PetriNetPresentation net;
    net.name = "";
    net.places = {{0, "pre_B", {}}, {1, "sink0", {}}, {2, "source0", {}}};
    net.transitions = {{0, "A", "A", {}}, {1, "B", "B", {}}};
    net.arcs = {{{PetriNodeKind::kPlace, 0}, {PetriNodeKind::kTransition, 1}, 1, {}}, {{PetriNodeKind::kPlace, 2}, {PetriNodeKind::kTransition, 0}, 1, {}}, {{PetriNodeKind::kTransition, 0}, {PetriNodeKind::kPlace, 0}, 1, {}}, {{PetriNodeKind::kTransition, 1}, {PetriNodeKind::kPlace, 1}, 1, {}}};
    net.initial_marking = {{2, 1}};
    net.final_marking = {{1, 1}};
    assert(net.places.size() == 3 && net.transitions.size() == 2 && net.arcs.size() == 4);
    assert(petri_net_presentation_json(net) == "{\"name\":\"\",\"properties\":{},\"places\":[{\"id\":0,\"name\":\"pre_B\",\"properties\":{}},{\"id\":1,\"name\":\"sink0\",\"properties\":{}},{\"id\":2,\"name\":\"source0\",\"properties\":{}}],\"transitions\":[{\"id\":0,\"name\":\"A\",\"label\":\"A\",\"properties\":{}},{\"id\":1,\"name\":\"B\",\"label\":\"B\",\"properties\":{}}],\"arcs\":[{\"source\":{\"kind\":\"place\",\"id\":0},\"target\":{\"kind\":\"transition\",\"id\":1},\"weight\":1,\"properties\":{}},{\"source\":{\"kind\":\"place\",\"id\":2},\"target\":{\"kind\":\"transition\",\"id\":0},\"weight\":1,\"properties\":{}},{\"source\":{\"kind\":\"transition\",\"id\":0},\"target\":{\"kind\":\"place\",\"id\":0},\"weight\":1,\"properties\":{}},{\"source\":{\"kind\":\"transition\",\"id\":1},\"target\":{\"kind\":\"place\",\"id\":1},\"weight\":1,\"properties\":{}}],\"initial_marking\":[{\"place_id\":2,\"tokens\":1}],\"final_marking\":[{\"place_id\":1,\"tokens\":1}]}");
  }
}

void presentation_identity_order_and_properties_are_explicit() {
  using namespace minera;
  PetriNetPresentation net;
  net.name = std::string("named\0net", 9);
  net.places = {{0, "same", {}}, {7, "same", {}}};
  net.transitions = {{0, "same", std::nullopt, {}},
                     {9, "source0", std::string("visible"), {}}};
  net.arcs = {{{PetriNodeKind::kPlace, 0}, {PetriNodeKind::kTransition, 0},
               std::numeric_limits<std::uint64_t>::max(), {}},
              {{PetriNodeKind::kPlace, 0}, {PetriNodeKind::kTransition, 0}, 2, {}},
              {{PetriNodeKind::kTransition, 0}, {PetriNodeKind::kPlace, 7}, 0, {}}};
  net.initial_marking = {{7, 0}, {0, std::numeric_limits<std::int64_t>::min()}};
  net.final_marking = {{0, std::numeric_limits<std::int64_t>::max()}};
  HeuristicStructuredValue::Array nested{
      HeuristicStructuredValue{nullptr}, HeuristicStructuredValue{true},
      HeuristicStructuredValue{std::int64_t{-2}},
      HeuristicStructuredValue{std::uint64_t{3}}, HeuristicStructuredValue{1.25},
      HeuristicStructuredValue{std::string("escaped\"\\\n")},
      HeuristicStructuredValue{HeuristicStructuredValue::Object{
          {"constructor", HeuristicStructuredValue{false}}}}};
  net.properties = {{"__proto__", HeuristicStructuredValue{nested}},
                    {"integer_one", HeuristicStructuredValue{std::int64_t{1}}},
                    {"floating_one", HeuristicStructuredValue{1.0}},
                    {"negative_zero", HeuristicStructuredValue{-0.0}}};
  net.places[0].properties = {{"place", HeuristicStructuredValue{std::int64_t{-4}}}};
  net.transitions[0].properties = {{"transition", HeuristicStructuredValue{std::string("silent")}}};
  net.arcs[1].properties = {{"arc", HeuristicStructuredValue{std::uint64_t{5}}}};
  const auto json = petri_net_presentation_json(net);
  assert(json.find("named\\u0000net") != std::string::npos);
  assert(json.find("\"name\":\"same\",\"label\":null") != std::string::npos);
  assert(json.find("\"weight\":18446744073709551615") != std::string::npos);
  assert(json.find("\"weight\":2") != std::string::npos && json.find("\"weight\":0") != std::string::npos);
  assert(json.find("\"place_id\":7,\"tokens\":0") != std::string::npos);
  assert(json.find("-9223372036854775808") != std::string::npos);
  assert(json.find("9223372036854775807") != std::string::npos);
  assert(json.find("[null, true, -2, 3, 1.25") != std::string::npos);
  assert(json.find("\"place\": -4") != std::string::npos);
  assert(json.find("\"transition\": \"silent\"") != std::string::npos);
  assert(json.find("\"arc\": 5") != std::string::npos);
  assert(json.find("\"integer_one\": 1,") != std::string::npos);
  assert(json.find("\"floating_one\": 1.0000000000000000") != std::string::npos);
  const std::string zero_key = "\"negative_zero\": ";
  const auto zero_pos = json.find(zero_key);
  assert(zero_pos != std::string::npos);
  const auto zero_token = json.substr(zero_pos + zero_key.size());
  assert(zero_token.starts_with("-0."));
  assert(std::signbit(std::stod(zero_token)));
  assert(std::get<std::int64_t>(net.properties.at("integer_one").value) == 1);
  assert(std::get<double>(net.properties.at("floating_one").value) == 1.0);
  // Distinct p0 and t0, repeated names and endpoint occurrences are retained.
  assert(net.arcs.size() == 3 && net.initial_marking.size() == 2);
}

void malformed_petri_presentations_are_rejected() {
  using namespace minera;
  PetriNetPresentation valid;
  valid.places = {{0, "p", {}}};
  valid.transitions = {{0, "named silent", std::nullopt, {}}};
  valid.arcs = {{{PetriNodeKind::kPlace, 0}, {PetriNodeKind::kTransition, 0}, 1, {}}};
  const auto rejected = [&](const auto& mutate) {
    auto bad = valid;
    mutate(bad);
    bool threw = false;
    try { (void)petri_net_presentation_json(bad); }
    catch (const std::invalid_argument&) { threw = true; }
    assert(threw);
  };
  rejected([](auto& n) { n.places.push_back(n.places[0]); });
  rejected([](auto& n) { n.transitions.push_back(n.transitions[0]); });
  rejected([](auto& n) { n.arcs[0].source.id = 99; });
  rejected([](auto& n) { n.arcs[0].target.id = 99; });
  rejected([](auto& n) { n.arcs[0].source.kind = static_cast<PetriNodeKind>(99); });
  rejected([](auto& n) { n.arcs[0].target.kind = static_cast<PetriNodeKind>(99); });
  rejected([](auto& n) { n.arcs[0].target = n.arcs[0].source; });
  rejected([](auto& n) { n.arcs[0].source = n.arcs[0].target; });
  rejected([](auto& n) { n.initial_marking = {{99, 0}}; });
  rejected([](auto& n) { n.final_marking = {{99, 1}}; });
  rejected([](auto& n) { n.initial_marking = {{0, 0}, {0, 1}}; });
  rejected([](auto& n) { n.final_marking = {{0, -1}, {0, 0}}; });
  const HeuristicStructuredValue nonfinite{HeuristicStructuredValue::Object{
      {"nested", HeuristicStructuredValue{HeuristicStructuredValue::Array{
          HeuristicStructuredValue{std::numeric_limits<double>::infinity()}}}}}};
  rejected([&](auto& n) { n.properties["bad"] = nonfinite; });
  rejected([&](auto& n) { n.places[0].properties["bad"] = nonfinite; });
  rejected([&](auto& n) { n.transitions[0].properties["bad"] = nonfinite; });
  rejected([&](auto& n) { n.arcs[0].properties["bad"] = nonfinite; });
  rejected([](auto& n) { n.properties["nan"] = HeuristicStructuredValue{std::numeric_limits<double>::quiet_NaN()}; });
  rejected([](auto& n) { n.properties["negative_infinity"] = HeuristicStructuredValue{-std::numeric_limits<double>::infinity()}; });
  assert(petri_net_presentation_json(PetriNetPresentation{}) ==
         "{\"name\":\"\",\"properties\":{},\"places\":[],\"transitions\":[],\"arcs\":[],\"initial_marking\":[],\"final_marking\":[]}");
}

}  // namespace

int main() {
  leaf_and_tau_keep_labels_and_language();
  composite_fragments_have_silent_borders();
  binary_loop_preserves_language();
  unsupported_and_malformed_trees_are_rejected();
  explicit_standard_variant_matches_default();
  variant_names_and_json_are_stable();
  classic_dfg_conversion_retains_shared_join_bridge();
  supplied_public_petri_presentations_keep_complete_fields();
  presentation_identity_order_and_properties_are_explicit();
  malformed_petri_presentations_are_rejected();
}
