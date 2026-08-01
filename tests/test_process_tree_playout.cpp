#include "minera/process_tree_playout.hpp"

#include <cassert>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <vector>

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
