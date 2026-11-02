#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
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
  // Keep only traces that lie entirely inside the range (bounds inclusive).
  // Both trace modes span a trace by its earliest and latest timestamp, as
  // the DataFrame filter does, not by its first and last events.
  traces_contained,
  // Keep only traces that overlap the range, as the DataFrame filter decides:
  // the earliest or latest instant lies strictly inside it, or the trace
  // spans the whole range (endpoints inclusive). A trace that only touches a
  // bound from outside is dropped.
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

// An admitted trace pattern is an activity sequence. The special activity
// token "..." matches zero or more activities, so patterns can describe an
// exact variant ("A,B"), a prefix ("A,B,..."), a suffix ("...,A,B"), or an
// infix ("...,A,B,..."). Literal activities are matched exactly.
using TracePattern = std::vector<std::string>;

// Parses semicolon-separated trace patterns. Within each pattern, activities
// are comma-separated and surrounding whitespace is ignored. Empty patterns
// or activity tokens are rejected; "..." is the only wildcard token.
[[nodiscard]] std::vector<TracePattern> parse_trace_patterns(
    std::string_view text);

struct TracePatternFilterOptions {
  std::vector<TracePattern> patterns;
  bool positive = true;
};

// Exact activity-sequence admission. Unlike trace patterns, an admitted
// variant never treats any token specially. The empty sequence is valid and
// can be represented natively as an empty TracePattern.
struct AdmittedVariantFilterOptions {
  std::vector<TracePattern> variants;
  bool positive = true;
};

// Parses semicolon-separated exact variants. Activities are comma-separated;
// the token [] denotes the empty variant. Empty activities and empty variants
// (other than []) are rejected; ... is treated as a literal activity.
[[nodiscard]] std::vector<TracePattern> parse_admitted_variants(
    std::string_view text);

// Inclusive duration bounds for a complete trace, measured from its first
// event timestamp to its last event timestamp in input order.
struct CasePerformanceFilterOptions {
  std::optional<double> min_duration_seconds;
  std::optional<double> max_duration_seconds;
};

// Inclusive event-count bounds for complete traces. The standalone case-size
// filter defaults to the conventional two-event lower bound; integrated
// LogFilterOptions fields keep zero as "unbounded" for compatibility.
struct CaseSizeFilterOptions {
  std::size_t min_length = 2;
  std::optional<std::size_t> max_length;
};

// Inclusive numeric range for one generic event attribute. Values are parsed
// as finite numbers; missing or non-numeric values never match. In event mode
// matching events survive as a projection; in trace mode the whole case is
// kept or dropped based on whether any event matches the range.
struct NumericAttributeFilterOptions {
  std::string attribute_key;
  std::optional<double> min_value;
  std::optional<double> max_value;
  AttributeFilterMode mode = AttributeFilterMode::events;
  bool positive = true;
};

// Retain events whose activity (or optional generic event attribute) belongs
// to the most frequent values in the input log. A zero limit intentionally
// produces an empty log; the default is the conventional top-25 limit.
struct TopActivitiesFilterOptions {
  std::size_t max_activities = 25;
  // Empty means the event's activity field. A non-empty key reads a generic
  // event attribute and drops events that do not carry that key.
  std::string attribute_key;
};

// Retain events whose activity/attribute value occurs in at least the given
// fraction of cases or events. With keep_once_per_case enabled, each value is
// counted at most once in any trace; otherwise every event occurrence counts.
// The threshold is inclusive and must be finite in [0, 1].
struct RelativeAttributeOccurrenceFilterOptions {
  std::string attribute_key;
  double min_relative_stake = 0.0;
  bool keep_once_per_case = true;
};

// Retain events whose activity/attribute value occurs at least the requested
// absolute number of times in the original log. Empty attribute_key means the
// activity field; a generic key drops events that do not carry it.
struct AttributeOccurrenceThresholdFilterOptions {
  std::string attribute_key;
  std::uint64_t min_occurrences = 1;
};

enum class FilterAnchorOccurrence { first, last };

// Prefix/suffix anchor behavior, as pm4py's filter_prefixes/filter_suffixes:
// a strict cut stops immediately before/after the anchor event and leaves it
// out; a non-strict cut keeps it. A missing anchor, or a cut that leaves no
// events, drops the trace (a DataFrame has no rows for it).
struct PrefixSuffixFilterOptions {
  std::string activity;
  bool strict = true;
  FilterAnchorOccurrence occurrence = FilterAnchorOccurrence::first;
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
// filters (frequency, rank, coverage, and percentage), and finally the
// optional case-count limit.
struct LogFilterOptions {
  // Time-range bounds as ISO-8601 timestamps (a bare date reads as midnight
  // UTC); an empty string leaves that side unbounded. Both bounds are
  // inclusive. Events whose timestamp is missing or unparseable count as
  // outside any range, so they never satisfy an active time filter. A non-empty
  // bound that is not a recognizable timestamp makes the filter throw.
  std::string time_from;
  std::string time_to;
  TimeFilterMode time_mode = TimeFilterMode::events;
  // Optional complete-case duration bounds, applied before event projection.
  // Traces with an empty or unparseable first/last timestamp are excluded
  // whenever either bound is active.
  std::optional<double> min_case_duration_seconds;
  std::optional<double> max_case_duration_seconds;
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
  // Optional numeric range over one generic event attribute. This is
  // independent of the string-valued attribute filter above.
  std::optional<NumericAttributeFilterOptions> numeric_attribute;

  // Optional frequency-based event projections. Counts are derived from the
  // original input log before the ordinary per-trace projection so combining
  // these options remains deterministic.
  std::optional<TopActivitiesFilterOptions> top_activities;
  std::optional<RelativeAttributeOccurrenceFilterOptions>
      relative_attribute_occurrence;
  std::optional<AttributeOccurrenceThresholdFilterOptions>
      attribute_occurrence_threshold;

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

  // Trace-pattern filtering. Any admitted pattern may match. A positive
  // filter keeps matching traces; a negative filter keeps non-matching ones.
  // Patterns are judged after the ordinary event projection and before the
  // remaining case-level filters.
  std::vector<TracePattern> trace_patterns;
  bool trace_pattern_positive = true;

  // Exact admitted-variant filtering, applied after ordinary event
  // projection. Positive polarity keeps only admitted variants; negative
  // polarity removes them. Duplicate admitted variants are harmless and
  // retained in their supplied order.
  std::vector<TracePattern> admitted_variants;
  bool admitted_variants_positive = true;

  // Prefix/suffix cuts: reduce each case to its head before the first (or
  // last) occurrence of `prefix_activity`, and/or its tail after the first (or
  // last) occurrence of `suffix_activity`. Strict cuts leave the anchor out;
  // non-strict cuts keep it. Cases without the anchor, or left empty, are
  // dropped. Both may be set; the prefix cut applies first, so the suffix
  // anchor is sought inside the kept head.
  std::string prefix_activity;
  std::string suffix_activity;
  bool prefix_strict = true;
  bool suffix_strict = true;
  FilterAnchorOccurrence prefix_occurrence = FilterAnchorOccurrence::first;
  FilterAnchorOccurrence suffix_occurrence = FilterAnchorOccurrence::first;

  // Between-extraction: cut each trace into the subtraces that run from an
  // occurrence of `between_start` to the next occurrence of `between_end`, both
  // endpoints included, as pm4py's DataFrame filter_between cuts them. A trace
  // can yield several subtraces, each its own case suffixed ##@@0, ##@@1, …
  // (always suffixed, counted from 0), or none, in which case it is dropped.
  // When the two are equal the closing event opens the next subtrace and
  // belongs to it only. Both must be set together; setting only one throws.
  std::string between_start;
  std::string between_end;

  // Trace-length bounds on the projected traces (0 = unbounded).
  std::size_t min_length = 0;
  std::size_t max_length = 0;
  // Keep only the first max_cases surviving traces, preserving input order.
  // An engaged zero intentionally drops every case.
  std::optional<std::size_t> max_cases;

  // Endpoint constraints: keep only traces that start (respectively end) with
  // one of the listed activities. Empty means "any". With positive=false,
  // traces whose endpoint is in the list are excluded instead.
  std::vector<std::string> start_activities;
  std::vector<std::string> end_activities;
  bool start_activities_positive = true;
  bool end_activities_positive = true;

  // Variant selection. Keep only traces whose variant (its activity sequence)
  // occurs at least `min_variant_frequency` times; when `top_variants` is
  // non-zero, only the `top_variants` most frequent variants; and when
  // `variant_coverage` is non-zero, the most frequent variants that together
  // cover at least that share of the surviving cases ((0, 1]; a value outside
  // that range throws). The optional minimum/maximum coverage fields compare
  // each variant's own case share inclusively. `variant_percentage` ranks
  // variants by frequency and keeps enough cases to reach the requested
  // cumulative share, retaining all variants tied at the boundary; it keeps
  // at least the most frequent variant even when the percentage is zero.
  // A variant must satisfy every active constraint. All default to "keep
  // everything".
  std::uint64_t min_variant_frequency = 0;
  std::size_t top_variants = 0;
  double variant_coverage = 0.0;
  std::optional<double> variant_min_coverage;
  std::optional<double> variant_max_coverage;
  std::optional<double> variant_percentage;

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
           numeric_attribute.has_value() ||
           top_activities.has_value() ||
           relative_attribute_occurrence.has_value() ||
           attribute_occurrence_threshold.has_value() ||
           !case_attribute_key.empty() ||
           !keep_case_attribute_values.empty() ||
           !remove_case_attribute_values.empty() ||
           !rework_activity.empty() ||
           !keep_directly_follows.empty() ||
           !remove_directly_follows.empty() ||
           !keep_eventually_follows.empty() ||
           !remove_eventually_follows.empty() ||
           !trace_patterns.empty() ||
           !admitted_variants.empty() ||
           min_case_duration_seconds.has_value() ||
           max_case_duration_seconds.has_value() ||
           !prefix_activity.empty() || !suffix_activity.empty() ||
           !prefix_strict || !suffix_strict ||
           prefix_occurrence != FilterAnchorOccurrence::first ||
           suffix_occurrence != FilterAnchorOccurrence::first ||
           !between_start.empty() || !between_end.empty() ||
           min_length != 0 || max_length != 0 || !start_activities.empty() ||
           !end_activities.empty() || max_cases.has_value() ||
           min_variant_frequency > 1 ||
           top_variants != 0 || variant_coverage != 0.0 ||
           variant_min_coverage.has_value() ||
           variant_max_coverage.has_value() || variant_percentage.has_value() ||
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

// Standalone trace-pattern filter preserving log- and trace-level metadata.
[[nodiscard]] EventLog filter_trace_patterns(
    const EventLog& event_log, const TracePatternFilterOptions& options);

[[nodiscard]] EventLog filter_admitted_variants(
    const EventLog& event_log,
    const AdmittedVariantFilterOptions& options);

[[nodiscard]] EventLog filter_case_performance(
    const EventLog& event_log, const CasePerformanceFilterOptions& options);

[[nodiscard]] EventLog filter_case_size(
    const EventLog& event_log, const CaseSizeFilterOptions& options);

[[nodiscard]] EventLog filter_case_size(
    const EventLog& event_log, std::size_t min_length = 2,
    std::optional<std::size_t> max_length = std::nullopt);

[[nodiscard]] EventLog filter_numeric_attribute(
    const EventLog& event_log,
    const NumericAttributeFilterOptions& options);

[[nodiscard]] EventLog filter_top_activities(
    const EventLog& event_log, const TopActivitiesFilterOptions& options);

[[nodiscard]] EventLog filter_relative_attribute_occurrence(
    const EventLog& event_log,
    const RelativeAttributeOccurrenceFilterOptions& options);

[[nodiscard]] EventLog filter_attribute_occurrence_threshold(
    const EventLog& event_log,
    const AttributeOccurrenceThresholdFilterOptions& options);

// Compatibility spelling for the classic absolute attribute threshold helper.
[[nodiscard]] inline EventLog filter_log_by_attribute_occurrence_threshold(
    const EventLog& event_log, std::string attribute_key,
    std::uint64_t min_occurrences) {
  return filter_attribute_occurrence_threshold(
      event_log,
      AttributeOccurrenceThresholdFilterOptions{std::move(attribute_key),
                                                min_occurrences});
}

// Endpoint filters preserve complete traces and metadata. Positive mode keeps
// listed endpoints; negative mode drops them. Empty activity lists are a
// no-op for positive mode and invalid for negative mode.
[[nodiscard]] EventLog filter_start_activities(
    const EventLog& event_log, const std::vector<std::string>& activities,
    bool positive = true);

[[nodiscard]] EventLog filter_end_activities(
    const EventLog& event_log, const std::vector<std::string>& activities,
    bool positive = true);

// Variant-frequency selection helpers. Coverage bounds are inclusive shares
// of the input cases; percentage is a cumulative top-variant target.
[[nodiscard]] EventLog filter_variants_top_k(const EventLog& event_log,
                                             std::size_t k);

[[nodiscard]] EventLog filter_variants_by_coverage_percentage(
    const EventLog& event_log, double min_coverage_percentage);

[[nodiscard]] EventLog filter_variants_by_maximum_coverage_percentage(
    const EventLog& event_log, double max_coverage_percentage);

[[nodiscard]] EventLog filter_log_variants_percentage(
    const EventLog& event_log, double percentage = 0.8);

// The historical pluralized variant helper keeps the most frequent variants
// until the requested cumulative case share is reached.
[[nodiscard]] inline EventLog filter_variants_variants_percentage(
    const EventLog& event_log, double percentage = 0.0) {
  return filter_log_variants_percentage(event_log, percentage);
}

// Compatibility spellings used by the classic filtering surface.
[[nodiscard]] inline EventLog filter_log_on_max_no_activities(
    const EventLog& event_log, std::size_t max_no_activities = 25) {
  return filter_top_activities(
      event_log, TopActivitiesFilterOptions{max_no_activities, {}});
}

[[nodiscard]] inline EventLog filter_log_relative_occurrence_event_attribute(
    const EventLog& event_log, double min_relative_stake,
    std::string attribute_key = {}, bool keep_once_per_case = true) {
  return filter_relative_attribute_occurrence(
      event_log,
      RelativeAttributeOccurrenceFilterOptions{std::move(attribute_key),
                                               min_relative_stake,
                                               keep_once_per_case});
}

[[nodiscard]] EventLog filter_prefix(
    const EventLog& event_log, const PrefixSuffixFilterOptions& options);

[[nodiscard]] EventLog filter_suffix(
    const EventLog& event_log, const PrefixSuffixFilterOptions& options);

inline EventLog filter_prefixes(
    const EventLog& event_log, const PrefixSuffixFilterOptions& options) {
  return filter_prefix(event_log, options);
}

inline EventLog filter_suffixes(
    const EventLog& event_log, const PrefixSuffixFilterOptions& options) {
  return filter_suffix(event_log, options);
}

[[nodiscard]] EventLog filter_ncases(const EventLog& event_log,
                                     std::size_t max_cases = 1000);

inline EventLog filter_on_case_size(
    const EventLog& event_log, std::size_t min_length = 2,
    std::optional<std::size_t> max_length = std::nullopt) {
  return filter_case_size(event_log, min_length, max_length);
}

inline EventLog filter_on_ncases(const EventLog& event_log,
                                 std::size_t max_cases = 1000) {
  return filter_ncases(event_log, max_cases);
}

[[nodiscard]] inline EventLog trace_filter(
    const EventLog& event_log, const TracePatternFilterOptions& options) {
  return filter_trace_patterns(event_log, options);
}

}  // namespace minera
