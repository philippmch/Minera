#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "minera/event_log.hpp"
#include "minera/process_tree.hpp"

namespace minera {

// Trace-based process-tree reduction, corresponding to the reference
// TREE_TR_BASED variant.
enum class ProcessTreeReductionVariant {
  kTreeTrBased,
  TREE_TR_BASED = kTreeTrBased,
};

struct ProcessTreeReductionOptions {
  ProcessTreeReductionVariant variant =
      ProcessTreeReductionVariant::kTreeTrBased;
  // Empty and "concept:name" both use Event::activity.  Other keys are read
  // from Event::attributes and must be present as string attributes.
  std::string activity_key;
};

// Returns a reduced copy of tree for one trace.  The input tree and trace are
// never modified.  Tree validation follows Minera's native representation:
// leaves have no children, non-loop operators have at least one child, and a
// loop has at least its body child (later children are alternative redo paths).
[[nodiscard]] ProcessTree reduce_process_tree(
    const ProcessTree& tree, const Trace& trace,
    ProcessTreeReductionOptions options = {});

// A reference failure: the Python exception class and its str().
class ProcessTreeReductionError : public std::invalid_argument {
 public:
  ProcessTreeReductionError(std::string type, const std::string& message)
      : std::invalid_argument(message), type(std::move(type)) {}
  std::string type;
};

// One distinct leaf: an activity label, or std::nullopt for tau.
using ProcessTreeReductionLeaf = std::optional<std::string>;

// The reference TREE_TR_BASED reduction (PM4Py 2.7.22), including how it
// walks the tree. Its bottom-up node list deduplicates structurally equal
// subtrees: it starts from the distinct leaves (each the first instance in
// breadth-first order) and appends a node's parent once every child is
// structurally present and the parent itself is not, so a subtree whose leaf
// instance belongs elsewhere is never visited. Each visited node that is
// skippable and shares no activity with the trace is replaced by tau in its
// parent, and the result is the folded last visited node, which is not the
// root when the walk stops early. A visited loop with fewer than two children
// raises IndexError. The reference orders the distinct leaves by Python's
// hash-seeded set iteration; `leaf_order` (a permutation of the distinct
// leaves) selects one such order, and the default is breadth-first first
// appearance.
[[nodiscard]] ProcessTree reference_reduce_process_tree(
    const ProcessTree& tree, const Trace& trace, const ProcessTreeReductionOptions& options = {},
    const std::vector<ProcessTreeReductionLeaf>* leaf_order = nullptr);

// The distinct leaves of a tree in breadth-first first appearance.
[[nodiscard]] std::vector<ProcessTreeReductionLeaf> process_tree_reduction_leaves(const ProcessTree& tree);

// The reference fold (process_tree.utils.generic.fold): sequence, parallel
// and xor nodes absorb children of their own operator (or nodes do not); xor
// and or keep only their last tau child, sequence and parallel drop taus
// (keeping one when nothing else is left); one-child nodes become their child;
// a loop of only taus becomes tau.
[[nodiscard]] ProcessTreeNode fold_process_tree_reference(ProcessTreeNode node);

// Convenience aliases matching the naming used by the other Minera modules.
[[nodiscard]] inline ProcessTree apply_process_tree_reduction(
    const ProcessTree& tree, const Trace& trace,
    ProcessTreeReductionOptions options = {}) {
  return reduce_process_tree(tree, trace, std::move(options));
}

[[nodiscard]] inline ProcessTree process_tree_reduction(
    const ProcessTree& tree, const Trace& trace,
    ProcessTreeReductionOptions options = {}) {
  return reduce_process_tree(tree, trace, std::move(options));
}

}  // namespace minera
