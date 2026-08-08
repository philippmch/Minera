#include "minera/path_filter.hpp"

#include <cmath>
#include <stdexcept>
#include <string>

#include "minera/timestamp.hpp"

namespace minera {
namespace {

void validate_options(const AdjacentPathFilterOptions& options) {
  if (options.predecessor.empty() || options.successor.empty()) {
    throw std::invalid_argument(
        "adjacent path filter: predecessor and successor are required");
  }
  if (options.min_elapsed_seconds &&
      !std::isfinite(*options.min_elapsed_seconds)) {
    throw std::invalid_argument(
        "adjacent path filter: minimum elapsed seconds must be finite");
  }
  if (options.max_elapsed_seconds &&
      !std::isfinite(*options.max_elapsed_seconds)) {
    throw std::invalid_argument(
        "adjacent path filter: maximum elapsed seconds must be finite");
  }
  const double minimum = options.min_elapsed_seconds.value_or(0.0);
  if (options.max_elapsed_seconds && minimum > *options.max_elapsed_seconds) {
    throw std::invalid_argument(
        "adjacent path filter: minimum elapsed seconds exceeds maximum");
  }
}

bool trace_matches(const Trace& trace,
                   const AdjacentPathFilterOptions& options) {
  const bool by_time = options.min_elapsed_seconds.has_value() ||
                       options.max_elapsed_seconds.has_value();
  for (std::size_t i = 0; i + 1 < trace.events.size(); ++i) {
    const auto& predecessor = trace.events[i];
    const auto& successor = trace.events[i + 1];
    if (predecessor.activity != options.predecessor ||
        successor.activity != options.successor) {
      continue;
    }
    if (!by_time) return true;

    const auto start = parse_timestamp(predecessor.timestamp);
    const auto end = parse_timestamp(successor.timestamp);
    if (!start || !end) {
      throw std::invalid_argument(
          "adjacent path filter: matching path has an invalid timestamp");
    }
    const double elapsed = *end - *start;
    if (!std::isfinite(elapsed)) {
      throw std::invalid_argument(
          "adjacent path filter: matching path has a non-finite elapsed time");
    }
    // A performance range defaults its lower bound to zero when only a
    // maximum is supplied, so backwards-in-time pairs stay outside it.
    // An explicit negative lower bound remains valid.
    const double minimum = options.min_elapsed_seconds.value_or(0.0);
    if (elapsed < minimum) {
      continue;
    }
    if (options.max_elapsed_seconds &&
        elapsed > *options.max_elapsed_seconds) {
      continue;
    }
    return true;
  }
  return false;
}

}  // namespace

EventLog filter_adjacent_path(const EventLog& event_log,
                              const AdjacentPathFilterOptions& options) {
  validate_options(options);
  EventLog result;
  result.attributes = event_log.attributes;
  result.traces.reserve(event_log.traces.size());
  for (const auto& trace : event_log.traces) {
    const bool matches = trace_matches(trace, options);
    if (matches == options.positive) result.traces.push_back(trace);
  }
  return result;
}

EventLog filter_adjacent_path_event_log(
    const EventLog& event_log, const AdjacentPathFilterOptions& options) {
  return filter_adjacent_path(event_log, options);
}

EventLog filter_adjacent_path_performance(
    const EventLog& event_log, const AdjacentPathFilterOptions& options) {
  return filter_adjacent_path(event_log, options);
}

EventLog filter_adjacent_path_performance_event_log(
    const EventLog& event_log, const AdjacentPathFilterOptions& options) {
  return filter_adjacent_path(event_log, options);
}

}  // namespace minera
