#include "minera/interval_tree.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <tuple>

#include "minera/timestamp.hpp"

namespace minera {
namespace {

std::string event_value(const Event& event, const std::string& key) {
  if (key.empty() || key == "activity" || key == "concept:name")
    return event.activity;
  const auto* value = event.attribute(key);
  return value == nullptr ? std::string{} : *value;
}

const std::string* timestamp_value(const Event& event, const std::string& key) {
  if (key.empty() || key == "time:timestamp" || key == "timestamp")
    return &event.timestamp;
  return event.attribute(key);
}

double require_timestamp(const Event& event, const std::string& key) {
  const auto* text = timestamp_value(event, key);
  const auto parsed = text == nullptr ? std::optional<double>{}
                                      : parse_timestamp(*text);
  if (!parsed || !std::isfinite(*parsed))
    throw std::invalid_argument("interval tree: invalid timestamp");
  return *parsed;
}

}  // namespace

IntervalTreeQueryResult query_open_path_intervals(
    const std::vector<OpenPathInterval>& intervals,
    const IntervalTreeQueryOptions& options) {
  for (const auto& interval : intervals) {
    if (!std::isfinite(interval.start) || !std::isfinite(interval.end) ||
        interval.start > interval.end) {
      throw std::invalid_argument(
          "interval tree: intervals must have finite ordered boundaries");
    }
  }
  if (options.point && !std::isfinite(*options.point))
    throw std::invalid_argument("interval tree: query point must be finite");
  if (options.range) {
    const auto [lower, upper] = *options.range;
    if (!std::isfinite(lower) || !std::isfinite(upper))
      throw std::invalid_argument("interval tree: query range must be finite");
    if (lower > upper)
      throw std::invalid_argument(
          "interval tree: query range start must not exceed end");
  }

  IntervalTreeQueryResult result;
  result.point = options.point;
  result.range = options.range;
  result.intervals = intervals;
  result.interval_count = intervals.size();
  if (options.point) {
    for (std::size_t index = 0; index < intervals.size(); ++index) {
      const auto& interval = intervals[index];
      if (interval.start <= *options.point && *options.point <= interval.end)
        result.point_matches.push_back(index);
    }
  }
  if (options.range) {
    const auto [lower, upper] = *options.range;
    for (std::size_t index = 0; index < intervals.size(); ++index) {
      const auto& interval = intervals[index];
      if (interval.end >= lower && interval.start <= upper)
        result.range_matches.push_back(index);
    }
  }
  return result;
}

IntervalTreeQueryOptions interval_tree_query_options(
    const OpenPathIntervalOptions& options) {
  return {options.query_point, options.query_range};
}

std::vector<OpenPathInterval> discover_open_path_intervals(
    const EventLog& log, const OpenPathIntervalOptions& options) {
  if (options.activity_key.empty() || options.start_timestamp_key.empty() ||
      options.timestamp_key.empty()) {
    throw std::invalid_argument("interval tree: attribute keys must not be empty");
  }
  if (!std::isfinite(options.epsilon) || options.epsilon < 0.0)
    throw std::invalid_argument("interval tree: epsilon must be finite and non-negative");
  if (options.filter_activity_couple &&
      (options.filter_activity_couple->first.empty() ||
       options.filter_activity_couple->second.empty())) {
    throw std::invalid_argument(
        "interval tree: filter activity couple must contain two labels");
  }

  std::vector<OpenPathInterval> result;
  for (std::size_t trace_index = 0; trace_index < log.traces.size();
       ++trace_index) {
    const auto& trace = log.traces[trace_index];
    for (std::size_t source_index = 0; source_index + 1 < trace.events.size();
         ++source_index) {
      const auto& source = trace.events[source_index];
      const double source_time =
          require_timestamp(source, options.timestamp_key);
      const std::string source_activity =
          event_value(source, options.activity_key);
      for (std::size_t target_index = source_index + 1;
           target_index < trace.events.size(); ++target_index) {
        const auto& target = trace.events[target_index];
        const double target_time =
            require_timestamp(target, options.start_timestamp_key);
        if (target_time < source_time) continue;
        const std::string target_activity =
            event_value(target, options.activity_key);
        if (!options.filter_activity_couple ||
            (source_activity == options.filter_activity_couple->first &&
             target_activity == options.filter_activity_couple->second)) {
          const double start = source_time - options.epsilon;
          const double end = target_time + options.epsilon;
          if (!std::isfinite(start) || !std::isfinite(end))
            throw std::invalid_argument("interval tree: interval is not finite");
          result.push_back({start, end, trace.case_id, source_activity,
                            target_activity, trace_index, source_index,
                            target_index});
        }
        break;
      }
    }
  }
  std::sort(result.begin(), result.end(), [](const auto& lhs, const auto& rhs) {
    return std::tie(lhs.start, lhs.end, lhs.case_id, lhs.source_activity,
                    lhs.target_activity, lhs.trace_index,
                    lhs.source_event_index, lhs.target_event_index) <
           std::tie(rhs.start, rhs.end, rhs.case_id, rhs.source_activity,
                    rhs.target_activity, rhs.trace_index,
                    rhs.source_event_index, rhs.target_event_index);
  });
  return result;
}

}  // namespace minera
