#include "minera/process_tree_playout.hpp"

#include <cassert>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <vector>

#include "minera/conformance.hpp"
#include "minera/json_export.hpp"
#include "minera/petri_net.hpp"
#include "minera/ptml.hpp"

using namespace minera;

namespace {
ProcessTreeNode seq(std::initializer_list<ProcessTreeNode> c) {
  return ProcessTreeNode{ProcessTreeOperator::kSequence, 0,
                         std::vector<ProcessTreeNode>(c)};
}
ProcessTreeNode xor_node(std::initializer_list<ProcessTreeNode> c) {
  return ProcessTreeNode{ProcessTreeOperator::kExclusive, 0,
                         std::vector<ProcessTreeNode>(c)};
}
ProcessTreeNode or_node(std::initializer_list<ProcessTreeNode> c) {
  return ProcessTreeNode{ProcessTreeOperator::kInclusiveChoice, 0,
                         std::vector<ProcessTreeNode>(c)};
}
std::vector<std::string> labels(const Trace& t) {
  std::vector<std::string> out;
  for (const auto& e : t.events) out.push_back(e.activity);
  return out;
}
}  // namespace

int main() {
  ProcessTree tree({"a", "b"}, seq({ProcessTreeNode::leaf(0), ProcessTreeNode::leaf(1)}));
  ProcessTreePlayoutOptions ex;
  ex.variant = ProcessTreePlayoutVariant::EXTENSIVE;
  ex.max_length = 3;
  ex.min_length = 2;
  ex.max_traces = 10;
  auto log = process_tree_playout(tree, ex);
  assert(log.traces.size() == 1);
  assert((labels(log.traces[0]) == std::vector<std::string>{"a", "b"}));

  ProcessTree choice({"a", "b"}, xor_node({ProcessTreeNode::leaf(0), ProcessTreeNode::leaf(1)}));
  ex.max_length = 1;
  ex.min_length = 0;
  ex.max_traces = 10;
  log = process_tree_playout(choice, ex);
  assert(log.traces.size() == 2);

  ex.variant = ProcessTreePlayoutVariant::TOPBOTTOM;
  ex.traces = 3;
  auto topbottom = process_tree_playout(choice, ex);
  assert(topbottom.traces.size() == 3);

  ex.variant = ProcessTreePlayoutVariant::EXTENSIVE;
  ex.max_length = 2;
  ProcessTree parallel({"a", "b"},
      ProcessTreeNode{ProcessTreeOperator::kParallel, 0,
                      {ProcessTreeNode::leaf(0), ProcessTreeNode::leaf(1)}});
  log = process_tree_playout(parallel, ex);
  assert(log.traces.size() == 2);

  ProcessTree inclusive(
      {"a", "b"}, or_node({ProcessTreeNode::leaf(0),
                              ProcessTreeNode::leaf(1)}));
  ex.max_length = 2;
  ex.min_length = 0;
  ex.max_traces = 10;
  log = process_tree_playout(inclusive, ex);
  assert(log.traces.size() == 4);
  assert(labels(log.traces[0]) == std::vector<std::string>{"a"});
  assert(labels(log.traces[1]) == std::vector<std::string>{"b"});
  assert(labels(log.traces[2]) == std::vector<std::string>({"a", "b"}) ||
         labels(log.traces[2]) == std::vector<std::string>({"b", "a"}));
  assert(labels(log.traces[3]) == std::vector<std::string>({"a", "b"}) ||
         labels(log.traces[3]) == std::vector<std::string>({"b", "a"}));
  assert(labels(log.traces[2]) != labels(log.traces[3]));

  // Basic sampling selects a non-empty child subset and remains deterministic
  // for a fixed seed.
  ProcessTreePlayoutOptions sampled;
  sampled.traces = 24;
  sampled.max_length = 2;
  sampled.seed = 77;
  const auto sampled_first = process_tree_playout(inclusive, sampled);
  const auto sampled_second = process_tree_playout(inclusive, sampled);
  assert(sampled_first.traces.size() == sampled_second.traces.size());
  for (std::size_t i = 0; i < sampled_first.traces.size(); ++i) {
    assert(labels(sampled_first.traces[i]) == labels(sampled_second.traces[i]));
    assert(!sampled_first.traces[i].events.empty());
    assert(sampled_first.traces[i].events.size() <= 2);
  }

  const auto ptml = to_ptml(inclusive);
  assert(ptml.find("<or ") != std::string::npos);
  const auto ptml_round_trip = parse_ptml(ptml);
  assert(ptml_round_trip.root().op == ProcessTreeOperator::kInclusiveChoice);
  const auto json = random_tree_json(inclusive);
  assert(json.find("inclusive_or") != std::string::npos);

  // The workflow net accepts every non-empty subset, including the concurrent
  // a+b branch, without leaving residual tokens at completion.
  const auto inclusive_net = to_petri_net(inclusive);
  assert(final_marking_reachable(inclusive_net));
  assert(check_alignment(log, inclusive_net).fitness == 1.0);

  bool invalid_or = false;
  try {
    (void)process_tree_playout(
        ProcessTree({"a"}, ProcessTreeNode{ProcessTreeOperator::kInclusiveChoice,
                                             0, {}}),
        ex);
  } catch (const std::invalid_argument&) {
    invalid_or = true;
  }
  assert(invalid_or);
  invalid_or = false;
  try {
    (void)to_petri_net(
        ProcessTree({"a"}, ProcessTreeNode{ProcessTreeOperator::kInclusiveChoice,
                                             0, {}}));
  } catch (const std::invalid_argument&) {
    invalid_or = true;
  }
  assert(invalid_or);

  ProcessTree with_tau({"a"}, seq({ProcessTreeNode::tau(), ProcessTreeNode::leaf(0)}));
  log = process_tree_playout(with_tau, ex);
  assert(log.traces.size() == 1 && log.traces[0].events.size() == 1);

  ProcessTreePlayoutOptions basic;
  basic.traces = 5;
  basic.max_length = 3;
  basic.seed = 42;
  auto first = process_tree_playout(choice, basic);
  auto second = process_tree_playout(choice, basic);
  assert(first.traces.size() == second.traces.size());
  for (std::size_t i = 0; i < first.traces.size(); ++i)
    assert(labels(first.traces[i]) == labels(second.traces[i]));

  ProcessTree loop({"a", "b"},
      ProcessTreeNode{ProcessTreeOperator::kLoop, 0,
                      {ProcessTreeNode::leaf(0), ProcessTreeNode::leaf(1)}});
  basic.traces = 1;
  basic.max_loop_occurrences = 2;
  basic.max_length = 8;
  bool variants_differ = false;
  for (std::uint64_t seed = 1; seed <= 64 && !variants_differ; ++seed) {
    basic.seed = seed;
    basic.variant = ProcessTreePlayoutVariant::BASIC_PLAYOUT;
    const auto basic_loop = process_tree_playout(loop, basic);
    basic.variant = ProcessTreePlayoutVariant::TOPBOTTOM;
    const auto topbottom_loop = process_tree_playout(loop, basic);
    variants_differ =
        labels(basic_loop.traces.front()) !=
        labels(topbottom_loop.traces.front());
  }
  assert(variants_differ);

  // A wide parallel cut has factorially many interleavings. The extensive
  // budget must stop generation before traversing the full language.
  std::vector<std::string> wide_names;
  std::vector<ProcessTreeNode> wide_children;
  for (std::size_t i = 0; i < 12; ++i) {
    wide_names.push_back("w" + std::to_string(i));
    wide_children.push_back(ProcessTreeNode::leaf(i));
  }
  ProcessTree wide_parallel(
      wide_names,
      ProcessTreeNode{ProcessTreeOperator::kParallel, 0,
                      std::move(wide_children)});
  ProcessTreePlayoutOptions bounded;
  bounded.variant = ProcessTreePlayoutVariant::EXTENSIVE;
  bounded.max_length = 12;
  bounded.max_traces = 1;
  const auto bounded_log = process_tree_playout(wide_parallel, bounded);
  assert(bounded_log.traces.size() == 1);
  assert(bounded_log.traces.front().events.size() == 12);

  ProcessTree wide_choice(
      wide_names,
      ProcessTreeNode{ProcessTreeOperator::kExclusive, 0,
                      {ProcessTreeNode::tau(), wide_parallel.root()}});
  bounded.min_length = 1;
  bool budget_error = false;
  try {
    (void)process_tree_playout(wide_choice, bounded);
  } catch (const std::runtime_error&) {
    budget_error = true;
  }
  assert(budget_error);

  bool threw = false;
  try { (void)process_tree_playout(ProcessTree({"a"}, ProcessTreeNode::leaf(3)), basic); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  return 0;
}
