#pragma once

#include <string>
#include <utility>

#include "minera/event_log.hpp"
#include "minera/process_tree.hpp"

namespace minera {

// Trace-based process-tree reduction, corresponding to PM4Py's
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
