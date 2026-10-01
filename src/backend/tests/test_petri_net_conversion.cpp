#include "minera/petri_net.hpp"
#include "minera/heuristic_miner.hpp"
#include "minera/petri_analysis.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
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

}  // namespace

int main() {
  leaf_and_tau_keep_labels_and_language();
  composite_fragments_have_silent_borders();
  binary_loop_preserves_language();
  unsupported_and_malformed_trees_are_rejected();
  explicit_standard_variant_matches_default();
  variant_names_and_json_are_stable();
  classic_dfg_conversion_retains_shared_join_bridge();
}
