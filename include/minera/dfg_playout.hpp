#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "minera/dfg_variants.hpp"
#include "minera/event_log.hpp"

namespace minera {

// The two native simulation strategies supported by a directly-follows graph.
enum class DfgPlayoutVariant { kClassic, kPerformance, CLASSIC = kClassic, PERFORMANCE = kPerformance };

struct DfgPlayoutOptions {
  DfgPlayoutVariant variant = DfgPlayoutVariant::kClassic;
  std::size_t max_variants = 3000;
  double min_weighted_probability = 1.0;
  std::size_t max_occurrences_per_activity = 2;
  bool interrupt_when_dfg_complete = false;
  bool add_trace_if_takes_new_edges = false;
  std::size_t max_execution_time_ms = 0; // zero means no wall-clock bound
  bool return_variants = false;
  bool return_only_if_complete = false;
  std::size_t min_variant_occurrences = 1;

  std::size_t traces = 1000;
  double case_arrival_rate = 1.0;
  std::uint64_t seed = 1;
  std::size_t max_events_per_trace = 1000;
  std::string start_time = "2026-01-01T00:00:00Z";
  // Attribute names used by generated traces/events. The first-class native
  // fields remain populated for compatibility; non-standard names are also
  // emitted as typed generic attributes.
  std::string activity_key = "concept:name";
  std::string timestamp_key = "time:timestamp";
  std::string case_id_key = "case:concept:name";
  // Synthetic boundary labels are kept as distinct internal nodes during
  // performance sampling, so labels that occur as real activities (or equal
  // labels) do not change the generated language.
  std::string artificial_start_activity = "▶";
  std::string artificial_end_activity = "■";
  // Compatibility spelling accepted by the request adapters.
  std::size_t max_steps = 0;
};

using DfgPlayoutParams = DfgPlayoutOptions;
using DfgActivityCounts = std::map<std::string, std::uint64_t>;
using DfgVariantCounts =
    std::map<std::vector<std::string>, std::uint64_t>;

// Native result for both DFG playout strategies. Classic playout always
// computes the variant counts; the legacy EventLog wrapper exposes the same
// generated traces without requiring callers to consume the diagnostics.
struct DfgPlayoutResult {
  DfgPlayoutVariant variant = DfgPlayoutVariant::kClassic;
  EventLog log;
  DfgVariantCounts counts;
  bool complete = false;
  bool interrupted = false;
};

// Generates a DFG playout and, for classic playout, the deterministic variant
// occurrence counts. Counts are keyed by complete activity sequences.
[[nodiscard]] DfgPlayoutResult dfg_playout_result(
    const DfgFrequency& frequency,
    const DfgActivityCounts& start_activities,
    const DfgActivityCounts& end_activities,
    const DfgPlayoutOptions& options = {},
    const DfgPerformance* performance = nullptr);

// Samples the complete frequency DFG. Starts and ends are supplied separately
// because a DFG's edge map does not encode boundary frequencies.
[[nodiscard]] EventLog dfg_playout(const DfgFrequency& frequency,
                                    const DfgActivityCounts& start_activities,
                                    const DfgActivityCounts& end_activities,
                                    const DfgPlayoutOptions& options = {},
                                    const DfgPerformance* performance = nullptr);

[[nodiscard]] inline EventLog dfg_playout(
    const DfgFrequency& frequency,
    const DfgActivityCounts& start_activities,
    const DfgActivityCounts& end_activities,
    DfgPlayoutVariant variant,
    DfgPlayoutOptions options = {},
    const DfgPerformance* performance = nullptr) {
  options.variant = variant;
  return dfg_playout(frequency, start_activities, end_activities, options,
                     performance);
}

}  // namespace minera
