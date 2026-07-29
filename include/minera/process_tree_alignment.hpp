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
};

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
