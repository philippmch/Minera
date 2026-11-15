#include "minera/interval_tree.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <tuple>

#include "minera/dataframe_columns.hpp"
#include "minera/frame_keys.hpp"
#include "minera/timestamp.hpp"
#include "frame_cells.hpp"
#include "frame_times.hpp"

#include <map>
#include <optional>
#include <set>

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

// pm4py reads each log event's instant as Timestamp.timestamp(): a cell with
// nanosecond digits (a CSV frame keeps them) is rounded to the microsecond
// (round BR, observed black-box); other cells are the parsed seconds.
double require_timestamp(const Event& event, const std::string& key) {
  const auto* text = timestamp_value(event, key);
  const auto parsed = text == nullptr ? std::optional<double>{}
                                      : parse_timestamp(*text);
  if (!parsed || !std::isfinite(*parsed))
    throw std::invalid_argument("interval tree: invalid timestamp");
  if (const auto exact = parse_timestamp_exact(*text); exact && exact->nanoseconds % 1000 != 0)
    return frame_times::epoch_seconds(*exact);
  return *parsed;
}

// An upload's events as pm4py reads them from its frame (round CR, observed
// black-box): the activity key's cells as str() of their Python values, the
// timestamp keys' cells through Timestamp.timestamp() (NaT and cells of
// other types fail as Python fails), and each event's attributes for the
// sort's Event comparison.
struct FrameEvents {
  EventLog frame;
  std::set<std::string> keys;
  explicit FrameEvents(const EventLog& log) : frame(dataframe_frame_event_log(log)), keys(frame_keys(frame, FrameKeyScope::kDataFrame)) {}
  std::optional<python::Scalar> cell(std::size_t t, std::size_t e, const std::string& key) const {
    const auto& trace = frame.traces[t];
    if (!keys.contains(key)) throw std::invalid_argument("KeyError: '" + key + "'");
    return frame_scalar(frame, trace, trace.events[e], key, FrameKeyScope::kDataFrame);
  }
  std::string label(std::size_t t, std::size_t e, const std::string& requested) const {
    const std::string key = requested.empty() || requested == "activity" ? std::string("concept:name") : requested;
    const auto value = cell(t, e, key);
    if (!value || python::is_missing(*value)) {
      const auto column = frame_column_type(frame, key);
      return column && *column == DataFrameColumnType::kDate ? "NaT" : "nan";
    }
    return python::str(*value);
  }
  double time(std::size_t t, std::size_t e, const std::string& requested) const {
    const auto key = frame_times::timestamp_column(requested);
    const auto value = cell(t, e, key);
    if (value && value->kind == python::Scalar::Kind::kDate) return frame_times::epoch_seconds(value->date);
    if (!value || python::is_missing(*value)) {
      const auto column = frame_column_type(frame, key);
      if (column && *column == DataFrameColumnType::kDate) throw std::invalid_argument("ValueError: NaTType does not support timestamp");
      throw std::invalid_argument("AttributeError: 'float' object has no attribute 'timestamp'");
    }
    throw std::invalid_argument("AttributeError: '" + python::type_name(*value) + "' object has no attribute 'timestamp'");
  }
  // Event == Event: every attribute alike (an upload's frame gives each CSV
  // row its own @@index, so two CSV events never are).
  bool same_event(std::size_t t1, std::size_t e1, std::size_t t2, std::size_t e2) const {
    const auto& a = frame.traces[t1].events[e1];
    const auto& b = frame.traces[t2].events[e2];
    if (a.activity != b.activity || a.timestamp != b.timestamp || a.attributes.size() != b.attributes.size()) return false;
    std::map<std::string, std::pair<std::string, bool>> left;
    for (const auto& attribute : a.attributes) left[attribute.key] = {attribute.value, attribute.is_null};
    for (const auto& attribute : b.attributes) {
      const auto it = left.find(attribute.key);
      if (it == left.end() || it->second != std::pair{attribute.value, attribute.is_null}) return false;
    }
    return true;
  }
};

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
  // A filter couple may name the empty activity; the reference accepts it.

  std::optional<FrameEvents> frame;
  if (log.dataframe_columns) {
    frame.emplace(log);
    if (frame->frame.traces.size() != log.traces.size()) frame.reset();
  }
  const auto source_time_of = [&](std::size_t t, std::size_t e) {
    return frame ? frame->time(t, e, options.timestamp_key) : require_timestamp(log.traces[t].events[e], options.timestamp_key);
  };
  const auto target_time_of = [&](std::size_t t, std::size_t e) {
    return frame ? frame->time(t, e, options.start_timestamp_key) : require_timestamp(log.traces[t].events[e], options.start_timestamp_key);
  };
  const auto label_of = [&](std::size_t t, std::size_t e) {
    return frame ? frame->label(t, e, options.activity_key) : event_value(log.traces[t].events[e], options.activity_key);
  };
  std::vector<OpenPathInterval> result;
  bool empty_interval = false;
  for (std::size_t trace_index = 0; trace_index < log.traces.size();
       ++trace_index) {
    const auto& trace = log.traces[trace_index];
    for (std::size_t source_index = 0; source_index + 1 < trace.events.size();
         ++source_index) {
      const double source_time = source_time_of(trace_index, source_index);
      const std::string source_activity = label_of(trace_index, source_index);
      for (std::size_t target_index = source_index + 1;
           target_index < trace.events.size(); ++target_index) {
        const double target_time = target_time_of(trace_index, target_index);
        if (target_time < source_time) continue;
        const std::string target_activity = label_of(trace_index, target_index);
        if (!options.filter_activity_couple ||
            (source_activity == options.filter_activity_couple->first &&
             target_activity == options.filter_activity_couple->second)) {
          const double start = source_time - options.epsilon;
          const double end = target_time + options.epsilon;
          if (!std::isfinite(start) || !std::isfinite(end))
            throw std::invalid_argument("interval tree: interval is not finite");
          // The reference interval tree rejects an empty interval, such as
          // two events at the same time with epsilon 0.
          if (!(start < end)) {
            // Round CR: on an upload pm4py builds the tree after its sort,
            // whose Event comparison fails first.
            if (!frame) throw std::invalid_argument("interval tree: ValueError: Start must be less than end");
            empty_interval = true;
          }
          result.push_back({start, end, trace.case_id, source_activity,
                            target_activity, trace_index, source_index,
                            target_index});
        }
        break;
      }
    }
  }
  // Round CR: pm4py sorts the intervals by (start, end, source event,
  // target event); two intervals with one start and end compare their
  // events, which Python cannot order unless they are equal (observed
  // black-box): an upload's tied intervals raise there.
  if (frame) {
    std::map<std::pair<double, double>, std::size_t> first;
    for (std::size_t i = 0; i < result.size(); ++i) {
      const auto [it, added] = first.emplace(std::pair{result[i].start, result[i].end}, i);
      if (added) continue;
      const auto& other = result[it->second];
      if (!frame->same_event(other.trace_index, other.source_event_index, result[i].trace_index, result[i].source_event_index) ||
          !frame->same_event(other.trace_index, other.target_event_index, result[i].trace_index, result[i].target_event_index))
        throw std::invalid_argument("TypeError: '<' not supported between instances of 'Event' and 'Event'");
    }
    if (empty_interval) throw std::invalid_argument("interval tree: ValueError: Start must be less than end");
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
