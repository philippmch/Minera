#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "minera/event_log.hpp"
#include "minera/petri_net.hpp"

namespace minera {

enum class DecomposedAlignmentVariant {
  kRecomposMaximal,
  RECOMPOS_MAXIMAL = kRecomposMaximal,
};

struct DecomposedAlignmentMove {
  std::optional<std::string> log;
  std::optional<std::string> model;
  std::size_t transition = static_cast<std::size_t>(-1);
  [[nodiscard]] bool synchronous() const noexcept {
    return log && model && *log != ">>" && *model != ">>" && *log == *model;
  }
};

struct DecomposedAlignmentOptions {
  DecomposedAlignmentVariant variant = DecomposedAlignmentVariant::kRecomposMaximal;
  double threshold_border_agreement = 100000000.0;
  double max_time_trace = 0.0;
  double max_time_total = 0.0;
  std::size_t max_states = 200000;
  std::size_t max_total_states = 2000000;
  bool use_cache = true;
  bool clear_cache = false;
  // Standard costs.
  double log_move_cost = 10000.0;
  double model_move_cost = 10000.0;
  double tau_model_cost = 1.0;
  double sync_cost = 0.0;
  std::unordered_map<std::string, double> activity_log_costs;
  std::unordered_map<std::string, double> activity_model_costs;
  std::unordered_map<std::string, double> activity_sync_costs;
  std::vector<std::size_t> initial_marking;
  std::vector<std::size_t> final_marking;
  // Event field used to construct alignment words.
  std::string activity_attribute = "concept:name";
  // By default standard costs without time or state limits run the reference
  // port (decomposed_reference.hpp); true always runs the native search.
  bool native_search = false;
  // Polled while the computation runs (the server sets it to notice a client
  // that disconnected); once it returns true the call throws
  // OperationCancelled (minera/cancellation.hpp) and returns nothing. Unset
  // never cancels, and a run that is not cancelled is unchanged.
  std::function<bool()> cancelled;
};

struct DecomposedAlignmentTrace {
  std::vector<std::string> activities;
  std::uint64_t count = 1;
  bool complete = false;
  bool timed_out = false;
  bool exhausted = false;
  double cost = 0.0;
  double fitness = 0.0;
  double best_worst_cost = 0.0;
  std::vector<DecomposedAlignmentMove> alignment;
};

struct DecomposedAlignmentDiagnostics {
  std::size_t decompositions = 0;
  std::size_t components = 0;
  std::size_t merges = 0;
  std::size_t cache_hits = 0;
  std::size_t cache_misses = 0;
  std::size_t border_disagreements = 0;
  std::size_t agreement_iterations = 0;
  std::size_t visited_states = 0;
  std::size_t queued_states = 0;
  std::size_t traversed_arcs = 0;
  bool whole_net_fallback = false;
  std::string note;
};

struct DecomposedAlignmentResult {
  bool complete = true;
  bool timed_out = false;
  bool exhausted = false;
  double fitness = 1.0;
  double best_worst_cost = 0.0;
  std::uint64_t trace_count = 0;
  double total_cost = 0.0;
  DecomposedAlignmentDiagnostics diagnostics;
  std::vector<DecomposedAlignmentTrace> traces;
  // One row per source trace, including duplicate variants.
  [[nodiscard]] const std::vector<DecomposedAlignmentTrace>& alignments() const noexcept {
    return traces;
  }
};

using DecomposedAlignmentLogResult = DecomposedAlignmentResult;
using DecomposedTraceAlignment = DecomposedAlignmentTrace;
using DecomposedMove = DecomposedAlignmentMove;

[[nodiscard]] DecomposedAlignmentResult align_decomposed(
    const EventLog& log, const PetriNet& net,
    const DecomposedAlignmentOptions& options = {});

[[nodiscard]] inline DecomposedAlignmentResult align_log_decomposed(
    const EventLog& log, const PetriNet& net,
    const DecomposedAlignmentOptions& options = {}) {
  return align_decomposed(log, net, options);
}

[[nodiscard]] inline DecomposedAlignmentTrace align_trace_decomposed(
    const std::vector<std::string>& activities, const PetriNet& net,
    const DecomposedAlignmentOptions& options = {}) {
  EventLog log; Trace trace;
  for (const auto& activity : activities) trace.events.push_back(Event{"", activity});
  log.traces.push_back(std::move(trace));
  auto result = align_decomposed(log, net, options);
  return result.traces.empty() ? DecomposedAlignmentTrace{} : result.traces.front();
}

[[nodiscard]] inline DecomposedAlignmentResult decomposed_alignment(
    const EventLog& log, const PetriNet& net,
    const DecomposedAlignmentOptions& options = {}) {
  return align_decomposed(log, net, options);
}

[[nodiscard]] inline DecomposedAlignmentResult apply_decomposed_alignment(
    const EventLog& log, const PetriNet& net,
    const DecomposedAlignmentOptions& options = {}) {
  return align_decomposed(log, net, options);
}

}  // namespace minera
