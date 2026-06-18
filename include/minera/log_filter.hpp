#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "minera/event_log.hpp"

namespace minera {

// Log-preprocessing options. Filters are applied in a fixed order so the result
// is well defined when several are combined: activity projection, then trace
// length, then endpoints, then variant frequency.
struct LogFilterOptions {
  // Activity projection. When `keep_activities` is non-empty, only events whose
  // activity appears in it survive; `remove_activities` then drops any listed
  // activity. An empty list means "no constraint" for that field.
  std::vector<std::string> keep_activities;
  std::vector<std::string> remove_activities;

  // Trace-length bounds on the projected traces (0 = unbounded).
  std::size_t min_length = 0;
  std::size_t max_length = 0;

  // Endpoint constraints: keep only traces that start (respectively end) with
  // one of the listed activities. Empty means "any".
  std::vector<std::string> start_activities;
  std::vector<std::string> end_activities;

  // Variant-frequency selection. Keep only traces whose variant (its activity
  // sequence) occurs at least `min_variant_frequency` times, and — when
  // `top_variants` is non-zero — only the `top_variants` most frequent
  // variants. Both default to "keep everything".
  std::uint64_t min_variant_frequency = 0;
  std::size_t top_variants = 0;

  [[nodiscard]] bool any() const {
    return !keep_activities.empty() || !remove_activities.empty() ||
           min_length != 0 || max_length != 0 || !start_activities.empty() ||
           !end_activities.empty() || min_variant_frequency > 1 ||
           top_variants != 0;
  }
};

// Returns a new event log containing the traces and events that survive the
// requested filters. Traces left empty by activity projection are dropped.
[[nodiscard]] EventLog filter_event_log(const EventLog& event_log,
                                        const LogFilterOptions& options);

}  // namespace minera
