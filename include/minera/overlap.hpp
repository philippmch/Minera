#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "minera/event_log.hpp"

namespace minera {

// Controls how event intervals are constructed and compared.  An empty source
// selects Event::timestamp; otherwise the source is looked up as a generic
// string event attribute and parsed as a timestamp.
struct OverlapOptions {
  // Empty uses Event::activity; otherwise the named generic event attribute
  // supplies the activity label for concurrent-activity statistics.
  std::string activity_attribute;
  std::string activity_key;
  std::string start_timestamp_attribute;
  std::string completion_timestamp_attribute;
  std::string start_timestamp_source;
  std::string completion_timestamp_source;
  // Generic completion aliases used by APIs that call the end instant simply
  // ``timestamp``.
  std::string timestamp_attribute;
  std::string timestamp_source;
  std::string timestamp_key;
  // ``*_key`` spellings are retained for callers that use key terminology;
  // the attribute spelling takes precedence, followed by source, then key.
  std::string start_timestamp_key;
  std::string completion_timestamp_key;
  double epsilon = 1e-5;
  bool strict = false;
};

using IntervalEventOverlapOptions = OverlapOptions;
using CaseOverlapOptions = OverlapOptions;
using OverlapCounts = std::vector<std::size_t>;
using IntervalEventOverlapCounts = OverlapCounts;
using CaseOverlapCounts = OverlapCounts;

// Counts are returned in flattened input event order (interval variant), or
// trace order (case variant).  Each interval counts itself in the interval
// variant; each non-empty case envelope counts itself in the case variant.
[[nodiscard]] IntervalEventOverlapCounts discover_interval_event_overlap_counts(
    const EventLog& log, const OverlapOptions& options = {});

[[nodiscard]] CaseOverlapCounts discover_case_overlap_counts(
    const EventLog& log, const OverlapOptions& options = {});

// Readable aliases for the two native variants.
[[nodiscard]] inline IntervalEventOverlapCounts interval_event_overlap_counts(
    const EventLog& log, const OverlapOptions& options = {}) {
  return discover_interval_event_overlap_counts(log, options);
}

[[nodiscard]] inline CaseOverlapCounts case_overlap_counts(
    const EventLog& log, const OverlapOptions& options = {}) {
  return discover_case_overlap_counts(log, options);
}

[[nodiscard]] inline IntervalEventOverlapCounts discover_event_overlap(
    const EventLog& log, const OverlapOptions& options = {}) {
  return discover_interval_event_overlap_counts(log, options);
}

[[nodiscard]] inline CaseOverlapCounts discover_trace_overlap(
    const EventLog& log, const OverlapOptions& options = {}) {
  return discover_case_overlap_counts(log, options);
}

using IntervalOverlapOptions = OverlapOptions;
using TraceOverlapOptions = OverlapOptions;
using IntervalEventOverlapResult = IntervalEventOverlapCounts;
using CaseOverlapResult = CaseOverlapCounts;

// Concurrent activity pairs are counted within each trace. Pair labels are
// normalized lexicographically, so (A,B) and (B,A) share one map entry.
using ConcurrentActivityOverlapOptions = OverlapOptions;
using ConcurrentActivityOverlapKey = std::pair<std::string, std::string>;
using ConcurrentActivityOverlapResult =
    std::map<ConcurrentActivityOverlapKey, std::size_t>;
using ConcurrentActivityOverlapCounts = ConcurrentActivityOverlapResult;

[[nodiscard]] ConcurrentActivityOverlapResult discover_concurrent_activity_overlap(
    const EventLog& log,
    const ConcurrentActivityOverlapOptions& options = {});

[[nodiscard]] inline ConcurrentActivityOverlapResult
discover_concurrent_activity_overlaps(
    const EventLog& log,
    const ConcurrentActivityOverlapOptions& options = {}) {
  return discover_concurrent_activity_overlap(log, options);
}

[[nodiscard]] inline ConcurrentActivityOverlapResult
concurrent_activity_overlap_counts(
    const EventLog& log,
    const ConcurrentActivityOverlapOptions& options = {}) {
  return discover_concurrent_activity_overlap(log, options);
}

[[nodiscard]] inline ConcurrentActivityOverlapResult
discover_concurrent_activity_overlap_counts(
    const EventLog& log,
    const ConcurrentActivityOverlapOptions& options = {}) {
  return discover_concurrent_activity_overlap(log, options);
}

[[nodiscard]] inline ConcurrentActivityOverlapResult concurrent_activity_overlaps(
    const EventLog& log,
    const ConcurrentActivityOverlapOptions& options = {}) {
  return discover_concurrent_activity_overlap(log, options);
}

[[nodiscard]] inline ConcurrentActivityOverlapResult compute_concurrent_activity_overlap(
    const EventLog& log,
    const ConcurrentActivityOverlapOptions& options = {}) {
  return discover_concurrent_activity_overlap(log, options);
}

[[nodiscard]] inline ConcurrentActivityOverlapResult concurrent_activity_overlap(
    const EventLog& log,
    const ConcurrentActivityOverlapOptions& options = {}) {
  return discover_concurrent_activity_overlap(log, options);
}

[[nodiscard]] inline IntervalEventOverlapCounts compute_interval_event_overlap(
    const EventLog& log, const OverlapOptions& options = {}) {
  return discover_interval_event_overlap_counts(log, options);
}

[[nodiscard]] inline CaseOverlapCounts compute_case_overlap(
    const EventLog& log, const OverlapOptions& options = {}) {
  return discover_case_overlap_counts(log, options);
}

}  // namespace minera
