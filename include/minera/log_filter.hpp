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

// How an attribute filter treats a trace.
enum class AttributeFilterMode {
  // Keep only the events that satisfy the attribute constraint; traces left
  // empty are dropped.
  events,
  // Keep whole traces by their events' attributes: a keep-set requires some
  // event to match, a remove-set drops the trace when any event matches, and
  // a bare key requires some event to carry it.
  traces,
};

// Log-preprocessing options. Filters are applied in a fixed order so the result
// is well defined when several are combined: time range, then activity /
// resource / event-attribute projection, then the case-level attribute filter,
// then rework, then between-extraction, then trace length, then endpoints,
// then variant frequency.
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

  // Resource (performer) projection, applied like the activity projection but on
  // each event's resource attribute: `keep_resources` keeps only events done by
  // a listed resource, `remove_resources` then drops events by a listed one. An
  // event with no resource is never kept by `keep_resources` and never dropped
  // by `remove_resources`. Empty lists mean "no constraint".
  std::vector<std::string> keep_resources;
  std::vector<std::string> remove_resources;

  // Generic attribute filter on one event-attribute key (any extra CSV column
  // or non-standard XES attribute). With `attribute_key` set: a non-empty
  // `keep_attribute_values` keeps only events carrying the key with a listed
  // value; `remove_attribute_values` then drops events whose value is listed
  // (events without the key are never kept by a keep-set and never dropped by
  // a remove-set, like the resource filter); with both lists empty, only
  // events carrying the key at all survive. `attribute_mode` switches from
  // filtering events to keeping/dropping whole cases by the same match.
  // Value lists without a key are a caller error and throw.
  std::string attribute_key;
  std::vector<std::string> keep_attribute_values;
  std::vector<std::string> remove_attribute_values;
  AttributeFilterMode attribute_mode = AttributeFilterMode::events;

  // Rework: keep only traces where `rework_activity` occurs at least
  // `rework_min_occurrences` times (a repeated activity signals rework). An
  // empty activity disables the filter; the minimum defaults to 2, the smallest
  // count that means repetition.
  std::string rework_activity;
  std::uint64_t rework_min_occurrences = 2;

  // Between-extraction: cut each trace into the subtraces that run from an
  // occurrence of `between_start` to the next occurrence of `between_end`, both
  // endpoints included. A trace can yield several subtraces (each becomes its
  // own case, suffixed #1, #2, … when there is more than one) or none, in which
  // case it is dropped. Both must be set together; setting only one throws.
  std::string between_start;
  std::string between_end;

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
           !keep_resources.empty() || !remove_resources.empty() ||
           !attribute_key.empty() || !keep_attribute_values.empty() ||
           !remove_attribute_values.empty() ||
           !rework_activity.empty() || !between_start.empty() ||
           !between_end.empty() ||
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

// Maps an attribute-filter mode name ("events" or "traces") to the mode, or
// nullopt when the name is neither.
[[nodiscard]] std::optional<AttributeFilterMode> parse_attribute_filter_mode(
    std::string_view name);

// Returns a new event log containing the traces and events that survive the
// requested filters. Traces left empty by activity projection are dropped.
[[nodiscard]] EventLog filter_event_log(const EventLog& event_log,
                                        const LogFilterOptions& options);

}  // namespace minera
