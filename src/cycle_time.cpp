#include "minera/cycle_time.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

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

}  // namespace

CycleTimeResult discover_cycle_time(const EventLog& log,
                                    const CycleTimeOptions& options) {
  std::vector<std::pair<double, double>> events;
  events.reserve(log.event_count());
  for (const auto& trace : log.traces) {
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
