#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "minera/event_log.hpp"

namespace minera {

// How a time-range filter treats a trace.
enum class TimeFilterMode {
  // Keep only the events whose instant lies in the range; traces left empty
  // are dropped.
  events,
  // Keep only traces that lie entirely inside the range.
  traces_contained,
  // Keep only traces that overlap the range at all.
  traces_intersecting,
};

// Log-preprocessing options. Filters are applied in a fixed order so the result
// is well defined when several are combined: time range, then activity
// projection, then trace length, then endpoints, then variant frequency.
struct LogFilterOptions {
  // Time-range bounds as ISO-8601 timestamps (a bare date reads as midnight
  // UTC); an empty string leaves that side unbounded. Both bounds are
  // inclusive. Events whose timestamp is missing or unparseable count as
  // outside any range, so they never satisfy an active time filter. A non-empty
  // bound that is not a recognizable timestamp makes the filter throw.
  std::string time_from;
  std::string time_to;
  TimeFilterMode time_mode = TimeFilterMode::events;
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
    return !time_from.empty() || !time_to.empty() ||
           !keep_activities.empty() || !remove_activities.empty() ||
           min_length != 0 || max_length != 0 || !start_activities.empty() ||
           !end_activities.empty() || min_variant_frequency > 1 ||
           top_variants != 0;
  }
};

// Maps a time-filter mode name ("events", "traces_contained", or
// "traces_intersecting") to the mode, or nullopt when the name is not one of
// the three.
[[nodiscard]] std::optional<TimeFilterMode> parse_time_filter_mode(
    std::string_view name);

// Returns a new event log containing the traces and events that survive the
// requested filters. Traces left empty by activity projection are dropped.
[[nodiscard]] EventLog filter_event_log(const EventLog& event_log,
                                        const LogFilterOptions& options);

}  // namespace minera
