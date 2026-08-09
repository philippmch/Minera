#include "minera/json_export.hpp"
#include "minera/service_time.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {

minera::Event event(std::string activity, std::string start,
                    std::string finish) {
  minera::Event value;
  value.activity = std::move(activity);
  value.timestamp = finish;
  value.attributes.push_back({"started_at", std::move(start),
                              minera::AttributeKind::kDate});
  value.attributes.push_back({"finished_at", std::move(finish),
                              minera::AttributeKind::kDate});
  return value;
}

minera::EventLog fixture() {
  minera::EventLog log;
  minera::Trace first;
  first.events = {event("A", "2024-01-01T00:00:00Z",
                         "2024-01-01T00:10:00Z"),
                  event("B", "2024-01-01T00:20:00Z",
                         "2024-01-01T00:50:00Z")};
  minera::Trace second;
  second.events = {event("A", "2024-01-02T00:00:00Z",
                          "2024-01-02T00:20:00Z"),
                   event("B", "2024-01-02T00:30:00Z",
                          "2024-01-02T01:10:00Z")};
  log.traces = {std::move(first), std::move(second)};
  return log;
}

}  // namespace

int main() {
  const auto log = fixture();
  minera::ServiceTimeOptions options;
  options.start_timestamp_key = "started_at";
  options.timestamp_key = "finished_at";
  const auto result = minera::discover_service_time(log, options);
  assert(result.values.size() == 2);
  assert(std::abs(result.values.at("A") - 900.0) < 1e-9);
  assert(std::abs(result.values.at("B") - 2100.0) < 1e-9);
  assert(std::string(minera::service_time_aggregation_name(
             result.aggregation)) == "MEAN");
  const auto json = minera::service_time_json(result);
  assert(json.find("\"family\":\"SERVICE_TIME\"") != std::string::npos);
  assert(json.find("\"A\":900") != std::string::npos);

  options.aggregation = minera::DfgPerformanceAggregation::kMedian;
  const auto median = minera::discover_service_time(log, options);
  assert(std::abs(median.values.at("B") - 2100.0) < 1e-9);
  options.aggregation = minera::DfgPerformanceAggregation::kRawValues;
  bool rejected = false;
  try { (void)minera::discover_service_time(log, options); }
  catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected);
  std::cout << "Service-time tests passed\n";
}
