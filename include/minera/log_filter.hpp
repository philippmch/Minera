#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "minera/activity_split.hpp"
#include "minera/attribute_value_repetition.hpp"
#include "minera/case_grouping.hpp"
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

// An ordered activity pair for the succession filters: `predecessor` is
// followed by `successor` — directly (adjacent events) or eventually (later
// anywhere in the same case), depending on the filter it feeds.
struct ActivityPair {
  std::string predecessor;
  std::string successor;
};

// Parses a comma-separated list of successions ("a -> b, c -> d") into
// activity pairs, trimming spaces around each side. An entry without "->" or
// with an empty side throws std::invalid_argument. The shared syntax for the
// CLI's succession flags and the server's form fields.
[[nodiscard]] std::vector<ActivityPair> parse_activity_pairs(
    std::string_view text);

// Log-preprocessing options. Filters are applied in a fixed order so the result
// is well defined when several are combined: the case-attribute filter, then
// time range, then activity / resource / event-attribute projection, then the
// case-level event-attribute filter, then rework, then the succession
// (directly-/eventually-follows) filters, then the prefix/suffix cuts, then
// between-extraction, then trace length, then endpoints, then the variant
// filters (frequency, rank, coverage).
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

  // Lifecycle projection, applied like the resource projection but on each
  // event's lifecycle transition (exact match, so filter with the log's own
  // spelling): `keep_lifecycles` keeps only events with a listed transition —
  // e.g. {"complete"} reduces a start/complete log to atomic events for
  // discovery — and `remove_lifecycles` then drops events with a listed one.
  // An event without a lifecycle is never kept by `keep_lifecycles` and never
  // dropped by `remove_lifecycles`. Empty lists mean "no constraint".
  std::vector<std::string> keep_lifecycles;
  std::vector<std::string> remove_lifecycles;

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

  // Case-attribute filter on one trace-attribute key (a scalar XES trace
  // attribute, or a `case:` CSV column). Always trace-level: with
  // `case_attribute_key` set, a non-empty `keep_case_attribute_values` keeps
  // only cases carrying the key with a listed value;
  // `remove_case_attribute_values` then drops cases whose value is listed
  // (cases without the key are never kept by a keep-set and never dropped by
  // a remove-set); with both lists empty, only cases carrying the key at all
  // survive. Value lists without a key are a caller error and throw.
  std::string case_attribute_key;
  std::vector<std::string> keep_case_attribute_values;
  std::vector<std::string> remove_case_attribute_values;

  // Rework: keep only traces where `rework_activity` occurs at least
  // `rework_min_occurrences` times (a repeated activity signals rework). An
  // empty activity disables the filter; the minimum defaults to 2, the smallest
  // count that means repetition.
  std::string rework_activity;
  std::uint64_t rework_min_occurrences = 2;

  // Succession filters, judged on the projected traces. A keep-list keeps
  // only cases exhibiting at least one listed succession; a remove-list then
  // drops cases exhibiting any listed one. "Directly" means the successor is
  // the very next event; "eventually" means it occurs anywhere later in the
  // same case (so a pair (a, a) matches a case where the activity repeats).
  // Empty lists mean "no constraint".
  std::vector<ActivityPair> keep_directly_follows;
  std::vector<ActivityPair> remove_directly_follows;
  std::vector<ActivityPair> keep_eventually_follows;
  std::vector<ActivityPair> remove_eventually_follows;

  // Prefix/suffix cuts: reduce each case to its head up to the first
  // occurrence of `prefix_activity`, and/or its tail from the first
  // occurrence of `suffix_activity` — the anchor event included on both
  // sides. Cases without the anchor are dropped. Both may be set; the prefix
  // cut applies first, so the suffix anchor is sought inside the kept head.
  std::string prefix_activity;
  std::string suffix_activity;

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

  // Variant selection. Keep only traces whose variant (its activity sequence)
  // occurs at least `min_variant_frequency` times; when `top_variants` is
  // non-zero, only the `top_variants` most frequent variants; and when
  // `variant_coverage` is non-zero, the most frequent variants that together
  // cover at least that share of the surviving cases ((0, 1]; a value outside
  // that range throws). A variant must satisfy every active constraint. All
  // default to "keep everything".
  std::uint64_t min_variant_frequency = 0;
  std::size_t top_variants = 0;
  double variant_coverage = 0.0;

  // Optional case split at one or more activities. The operation runs after
  // the ordinary per-trace projections and before variant selection.
  std::vector<std::string> activity_split_activities;
  std::string activity_split_separator = "##@@";
  ActivitySplitCutMode activity_split_cut_mode = ActivitySplitCutMode::kThis;

  // Optional whole-trace attribute/value repetition range.
  std::optional<AttributeValueRepetitionOptions> attribute_value_repetition;

  // Optional case-grouping transformations. Each is applied after ordinary
  // projections and before variant selection.
  std::optional<ConsecutiveActivityGroupingOptions>
      consecutive_activity_grouping;
  std::optional<TimestampCaseGroupingOptions> timestamp_case_grouping;

  [[nodiscard]] bool any() const {
    return !time_from.empty() || !time_to.empty() ||
           !keep_activities.empty() || !remove_activities.empty() ||
           !keep_resources.empty() || !remove_resources.empty() ||
           !keep_lifecycles.empty() || !remove_lifecycles.empty() ||
           !attribute_key.empty() || !keep_attribute_values.empty() ||
           !remove_attribute_values.empty() ||
           !case_attribute_key.empty() ||
           !keep_case_attribute_values.empty() ||
           !remove_case_attribute_values.empty() ||
           !rework_activity.empty() ||
           !keep_directly_follows.empty() ||
           !remove_directly_follows.empty() ||
           !keep_eventually_follows.empty() ||
           !remove_eventually_follows.empty() ||
           !prefix_activity.empty() || !suffix_activity.empty() ||
           !between_start.empty() || !between_end.empty() ||
           min_length != 0 || max_length != 0 || !start_activities.empty() ||
           !end_activities.empty() || min_variant_frequency > 1 ||
           top_variants != 0 || variant_coverage != 0.0 ||
           !activity_split_activities.empty() ||
           attribute_value_repetition.has_value() ||
           consecutive_activity_grouping.has_value() ||
           timestamp_case_grouping.has_value();
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
