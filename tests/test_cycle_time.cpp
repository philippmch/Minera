#include "minera/cycle_time.hpp"
#include "minera/json_export.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {

minera::Event event(std::string timestamp, std::string start = {}) {
  minera::Event value;
  value.timestamp = std::move(timestamp);
  if (!start.empty())
    value.attributes.push_back({"started_at", std::move(start),
                                minera::AttributeKind::kDate});
  return value;
}

minera::EventLog fixture() {
  minera::EventLog log;
  minera::Trace first;
  first.events = {event("2024-01-01T00:00:10Z", "2024-01-01T00:00:00Z"),
                  event("2024-01-01T00:00:20Z", "2024-01-01T00:00:10Z")};
  minera::Trace second;
  second.events = {event("2024-01-01T00:01:00Z", "2024-01-01T00:00:50Z"),
                   event("2024-01-01T00:01:10Z", "2024-01-01T00:01:00Z")};
  log.traces = {std::move(first), std::move(second)};
  return log;
}

}  // namespace

int main() {
  const auto log = fixture();
  minera::CycleTimeOptions options;
  options.start_timestamp_key = "started_at";
  const auto result = minera::discover_cycle_time(log, options);
  assert(result.instances == 2);
  assert(result.timed_events == 4);
  assert(std::abs(result.cycle_time - 10.0) < 1e-9);
  const auto json = minera::cycle_time_json(result);
  assert(json.find("\"family\":\"CYCLE_TIME\"") != std::string::npos);
  assert(json.find("\"cycle_time\":10") != std::string::npos);

  minera::EventLog custom;
  minera::Trace trace;
  trace.events = {event("2024-01-01T00:00:20Z",
                        "2024-01-01T00:00:00Z")};
  custom.traces.push_back(std::move(trace));
  options.start_timestamp_key = "started_at";
  const auto custom_result = minera::discover_cycle_time(custom, options);
  assert(custom_result.timed_events == 1);
  assert(custom_result.cycle_time == 0.0);

  bool rejected = false;
  try {
    minera::EventLog invalid;
    minera::Trace invalid_trace;
    invalid_trace.events.push_back(event("not-a-time"));
    invalid.traces.push_back(std::move(invalid_trace));
    (void)minera::discover_cycle_time(invalid);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
  std::cout << "Cycle-time tests passed\n";
}
