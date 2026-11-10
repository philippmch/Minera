#include "minera/cycle_time.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

#include "frame_times.hpp"
#include "minera/frame_keys.hpp"
#include "minera/timestamp.hpp"

namespace minera {
namespace {

bool builtin_timestamp_key(std::string_view key) {
  return key.empty() || key == "timestamp" || key == "time:timestamp";
}

double required_timestamp(const Event& event, std::string_view key,
                          std::string_view label) {
  std::string value;
  if (builtin_timestamp_key(key)) {
    value = event.timestamp;
  } else if (const auto* attribute = event.attribute(key)) {
    value = *attribute;
  } else {
    throw std::invalid_argument("cycle time: event is missing " +
                                std::string(label) + " attribute '" +
                                std::string(key) + "'");
  }
  const auto parsed = parse_timestamp(value);
  if (!parsed || !std::isfinite(*parsed))
    throw std::invalid_argument("cycle time: invalid " + std::string(label) +
                                " timestamp");
  return *parsed;
}

// Each row's (start, completion) on an upload's frame, as pm4py's DataFrame
// cycle time reads them: the two columns are selected (pandas' KeyError for
// the missing ones), then row by row in frame order the start's and the
// completion's Timestamp.timestamp(): a NaT cell raises ValueError, a cell
// that is no date AttributeError with its Python type (NaN is a float;
// observed black-box).
std::vector<std::pair<double, double>> frame_cycle_events(const EventLog& log, const CycleTimeOptions& options) {
  const frame_times::Frame frame(log);
  const auto start_key = frame_times::timestamp_column(options.start_timestamp_key);
  const auto end_key = frame_times::timestamp_column(options.timestamp_key);
  require_frame_columns(frame.keys(), {start_key, end_key});
  const auto seconds = [&](const Trace& trace, const Event& event, const std::string& key) {
    const auto value = frame.cell(trace, event, key);
    if (value && value->kind == python::Scalar::Kind::kDate) return frame_times::epoch_seconds(*frame.date(trace, event, key));
    if (!value && frame.dtype(key) == DataFrameColumnType::kDate)
      throw std::invalid_argument("ValueError: NaTType does not support timestamp");
    throw std::invalid_argument("AttributeError: '" + frame.cell_type(trace, event, key) +
                                "' object has no attribute 'timestamp'");
  };
  std::vector<std::pair<double, double>> events;
  for (const auto& trace : frame.log().traces)
    for (const auto& event : trace.events) {
      const double start = seconds(trace, event, start_key);
      events.emplace_back(start, seconds(trace, event, end_key));
    }
  return events;
}

}  // namespace

CycleTimeResult discover_cycle_time(const EventLog& log,
                                    const CycleTimeOptions& options) {
  std::vector<std::pair<double, double>> events;
  events.reserve(log.event_count());
  if (log.dataframe_columns) events = frame_cycle_events(log, options);
  static const std::vector<Trace> no_traces;
  for (const auto& trace : log.dataframe_columns ? no_traces : log.traces) {
    for (const auto& event : trace.events) {
      const double start = required_timestamp(
          event, options.start_timestamp_key, "start");
      const double finish =
          required_timestamp(event, options.timestamp_key, "completion");
      events.emplace_back(start, finish);
    }
  }

  CycleTimeResult result;
  result.instances = log.traces.size();
  result.timed_events = events.size();
  if (events.empty() || result.instances == 0) return result;

  std::sort(events.begin(), events.end());
  double interval_start = events.front().first;
  double interval_end = events.front().second;
  double production_time = 0.0;
  for (std::size_t i = 1; i < events.size(); ++i) {
    const auto [start, finish] = events[i];
    if (start > interval_end) {
      production_time += interval_end - interval_start;
      interval_start = start;
    }
    interval_end = std::max(interval_end, finish);
  }
  result.cycle_time =
      production_time / static_cast<double>(result.instances);
  return result;
}

}  // namespace minera
