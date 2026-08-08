#pragma once

#include <optional>
#include <string>

#include "minera/event_log.hpp"

namespace minera {

// Select traces containing (or, when positive is false, not containing) an
// adjacent predecessor/successor pair.  When either elapsed-time bound is
// supplied, the pair's elapsed time is measured as successor minus
// predecessor in seconds and compared inclusively with the bounds.
//
// Leaving both bounds unset gives the ordinary, non-performance adjacent-path
// predicate.  Bounds are optional so zero is a meaningful lower or upper
// bound.  A matching pair must have two parseable timestamps when a bound is
// active; malformed timestamps are reported rather than silently ignored.
struct AdjacentPathFilterOptions {
  std::string predecessor;
  std::string successor;
  std::optional<double> min_elapsed_seconds;
  std::optional<double> max_elapsed_seconds;
  bool positive = true;
};

using PathFilterOptions = AdjacentPathFilterOptions;
using AdjacentPathPerformanceFilterOptions = AdjacentPathFilterOptions;

// Filters a log by an adjacent path, preserving log/trace/event attributes and
// input order.  Traces remain in their original order, as do events in each
// retained trace.
[[nodiscard]] EventLog filter_adjacent_path(
    const EventLog& event_log, const AdjacentPathFilterOptions& options);

[[nodiscard]] EventLog filter_adjacent_path_event_log(
    const EventLog& event_log, const AdjacentPathFilterOptions& options);

[[nodiscard]] EventLog filter_adjacent_path_performance(
    const EventLog& event_log, const AdjacentPathFilterOptions& options);

[[nodiscard]] EventLog filter_adjacent_path_performance_event_log(
    const EventLog& event_log, const AdjacentPathFilterOptions& options);

// Compatibility noun-first aliases.
[[nodiscard]] inline EventLog adjacent_path_filter(
    const EventLog& event_log, const AdjacentPathFilterOptions& options) {
  return filter_adjacent_path(event_log, options);
}

[[nodiscard]] inline EventLog path_filter(
    const EventLog& event_log, const AdjacentPathFilterOptions& options) {
  return filter_adjacent_path(event_log, options);
}

}  // namespace minera
