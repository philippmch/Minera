#include "minera/case_arrival.hpp"
#include "minera/json_export.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {

minera::Trace trace(std::string first, std::string last) {
  minera::Trace value;
  minera::Event start;
  start.timestamp = std::move(first);
  minera::Event finish;
  finish.timestamp = std::move(last);
  value.events = {std::move(start), std::move(finish)};
  return value;
}

}  // namespace

int main() {
  minera::EventLog log;
  log.traces = {trace("2024-01-01T08:00:00Z", "2024-01-01T09:00:00Z"),
                trace("2024-01-01T10:00:00Z", "2024-01-01T11:00:00Z"),
                trace("2024-01-01T12:00:00Z", "2024-01-01T13:00:00Z")};
  const auto result = minera::discover_case_arrival(log);
  assert(result.timed_starts == 3 && result.timed_ends == 3);
  assert(std::abs(result.arrival_average - 7200.0) < 1e-9);
  assert(std::abs(result.dispersion_average - 7200.0) < 1e-9);
  const auto json = minera::case_arrival_json(result);
  assert(json.find("\"family\":\"CASE_ARRIVAL\"") != std::string::npos);
  assert(json.find("\"arrival_average\":7200") != std::string::npos);
  assert(std::abs(minera::get_case_arrival_average(log) - 7200.0) < 1e-9);

  minera::CaseArrivalOptions options;
  options.use_business_hours = true;
  options.business_hour_slots = {{8.0 * 3600.0, 12.0 * 3600.0}};
  const auto business = minera::discover_case_arrival(log, options);
  assert(std::abs(business.arrival_average - 7200.0) < 1e-9);

  minera::EventLog one;
  one.traces = {trace("2024-01-01T08:00:00Z", "2024-01-01T09:00:00Z")};
  const auto single = minera::discover_case_arrival(one);
  assert(single.arrival_average == 0.0 && single.dispersion_average == 0.0);

  bool rejected = false;
  try {
    minera::EventLog invalid;
    invalid.traces = {trace("invalid", "2024-01-01T09:00:00Z")};
    (void)minera::discover_case_arrival(invalid);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
  std::cout << "Case-arrival tests passed\n";
}
