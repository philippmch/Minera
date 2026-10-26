#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "minera/event_log.hpp"
#include "minera/process_tree.hpp"

namespace minera {

enum class ProcessTreeAlignmentVariant {
  kSearchGraphPt,
  kDynamicProgramming,
  kMilp,
  kApproximatedOriginal,
  SEARCH_GRAPH_PT = kSearchGraphPt,
  DYNAMIC_PROGRAMMING = kDynamicProgramming,
  MILP = kMilp,
  APPROXIMATED_ORIGINAL = kApproximatedOriginal,
};

enum class ProcessTreeAlignmentMoveType {
  kSynchronous,
  kLog,
  kModel,
  SYNCHRONOUS = kSynchronous,
  LOG = kLog,
  MODEL = kModel,
};

// SEARCH_GRAPH_PT lists every executed tau leaf as a kModel move whose
// model_activity is empty; it costs nothing.
struct ProcessTreeAlignmentMove {
  ProcessTreeAlignmentMoveType type = ProcessTreeAlignmentMoveType::kSynchronous;
  std::string log_activity;
  std::string model_activity;
  std::string activity;
};

struct ProcessTreeAlignmentDiagnostics {
  std::size_t visited_states = 0;
  std::size_t queued_states = 0;
  std::size_t traversed_arcs = 0;
  std::size_t lp_solved = 0;
  std::size_t traces_aligned = 0;
  std::string message;
};

struct ProcessTreeAlignmentOptions {
  ProcessTreeAlignmentVariant variant = ProcessTreeAlignmentVariant::kSearchGraphPt;
  // Zero means unlimited for time and state limits.
  std::size_t max_states = 0;
  double max_time_seconds = 0.0;       // per trace
  double max_total_time_seconds = 0.0; // whole log

  // APPROXIMATED_ORIGINAL controls. A zero threshold is unlimited.
  std::size_t max_trace_length = 1;
  std::size_t max_process_tree_height = 1;
  double max_trace_time_seconds = 0.0;
  double max_per_trace_time_seconds = 0.0;

  // Kept for source compatibility with the DP parameter vocabulary.
  bool use_global_cache = false;
  std::size_t batch_size = 1000;
  bool clear_cache = true;
  // Event field used to construct alignment words.
  std::string activity_attribute = "concept:name";
  // By default every variant runs its reference port
  // (process_tree_search_graph_reference.hpp, process_tree_reference_alignment.hpp,
  // process_tree_milp_reference.hpp, process_tree_approximated_reference.hpp);
  // only max_total_time_seconds applies, checked between distinct traces.
  // true runs the older native searches, which honour every limit above.
  // Setting max_states or a per-trace time limit also selects the native
  // search, as does a tree the variant's port rejects.
  bool native_search = false;
};

// fitness = 1 - cost / (trace length + floor), not clamped, where the floor is
// the variant's cost for the empty trace; APPROXIMATED_ORIGINAL uses the
// reference's binary-operator floor instead (10000 per visible leaf, 1 per tau
// leaf, 2 per binary operator).
struct ProcessTreeTraceAlignment {
  std::vector<std::string> trace;
  std::vector<ProcessTreeAlignmentMove> moves;
  std::uint64_t cost = 0;
  double fitness = 1.0;
  bool completed = true;
  bool timed_out = false;
};

struct ProcessTreeAlignmentResult {
  bool computed = true;
  bool completed = true;
  bool timed_out = false;
  bool exhausted = false;
  ProcessTreeAlignmentVariant variant = ProcessTreeAlignmentVariant::kSearchGraphPt;
  std::uint64_t total_cost = 0;
  double fitness = 1.0;
  ProcessTreeAlignmentDiagnostics diagnostics;
  std::vector<ProcessTreeTraceAlignment> traces;
};

[[nodiscard]] ProcessTreeAlignmentResult align_process_tree(
    const EventLog& event_log, const ProcessTree& tree,
    ProcessTreeAlignmentOptions options = {});

[[nodiscard]] inline ProcessTreeAlignmentResult align_process_tree_alignment(
    const EventLog& event_log, const ProcessTree& tree,
    ProcessTreeAlignmentOptions options = {}) {
  return align_process_tree(event_log, tree, options);
}

[[nodiscard]] inline ProcessTreeAlignmentResult apply_process_tree_alignment(
    const EventLog& event_log, const ProcessTree& tree,
    ProcessTreeAlignmentOptions options = {}) {
  return align_process_tree(event_log, tree, options);
}

}  // namespace minera
