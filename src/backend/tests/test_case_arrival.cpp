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

  // Reference capture (pm4py 2.7.22 get_case_arrival_avg and
  // get_case_dispersion_avg): the gaps are averaged as statistics.mean does,
  // the exact sum rounded once. A running double sum gives 5288.883333333334.
  minera::EventLog exact;
  exact.traces = {trace("2024-01-01T00:28:54.365Z", "2024-01-01T00:28:54.365Z"),
                  trace("2024-01-01T03:21:57.038Z", "2024-01-01T03:21:57.038Z"),
                  trace("2024-01-01T03:43:54.029Z", "2024-01-01T03:43:54.029Z"),
                  trace("2024-01-01T04:53:21.015Z", "2024-01-01T04:53:21.015Z")};
  const auto exact_result = minera::discover_case_arrival(exact);
  assert(exact_result.arrival_average == 5288.883333333333);
  assert(exact_result.dispersion_average == 5288.883333333333);
  // The DataFrame path (pm4py.get_case_arrival_average on the
  // format_dataframe frame of the same CSV, or on read_xes's DataFrame):
  // pandas total_seconds gaps averaged by numpy give 5288.883333333334.
  minera::CaseArrivalOptions frame;
  frame.dataframe = true;
  const auto frame_result = minera::discover_case_arrival(exact, frame);
  assert(frame_result.arrival_average == 5288.883333333334);
  assert(frame_result.dispersion_average == 5288.883333333334);
  // Gaps of 232001 us and 707535 us: the reference's total_seconds reads
  // 0.23200099999999998 and 0.707535, so the DataFrame mean is
  // 0.46976799999999996 where the exact mean is 0.469768.
  minera::EventLog sub_second;
  sub_second.traces = {trace("2024-01-01T00:00:00Z", "2024-01-01T00:00:00Z"),
                       trace("2024-01-01T00:00:00.232001Z", "2024-01-01T00:00:00.232001Z"),
                       trace("2024-01-01T00:00:00.939536Z", "2024-01-01T00:00:00.939536Z")};
  assert(minera::discover_case_arrival(sub_second, frame).arrival_average == 0.46976799999999996);
  assert(minera::discover_case_arrival(sub_second).arrival_average == 0.469768);
  // A nanosecond gap: the DataFrame path adds the nanoseconds times 1e-9,
  // the EventLog path floors the gap to the microsecond.
  minera::EventLog nano;
  nano.traces = {trace("2024-01-01T00:00:00Z", "2024-01-01T00:00:00Z"),
                 trace("2024-01-01T00:00:01.000002999Z", "2024-01-01T00:00:01.000002999Z")};
  assert(minera::discover_case_arrival(nano, frame).arrival_average == 1.0 + 2 * 1e-6 + 999 * 1e-9);
  assert(minera::discover_case_arrival(nano).arrival_average == 1.000002);

  minera::CaseArrivalOptions options;
  options.use_business_hours = true;
  options.business_hour_slots = {{8.0 * 3600.0, 12.0 * 3600.0}};
  const auto business = minera::discover_case_arrival(log, options);
  assert(std::abs(business.arrival_average - 7200.0) < 1e-9);

  minera::EventLog one;
  one.traces = {trace("2024-01-01T08:00:00Z", "2024-01-01T09:00:00Z")};
  const auto single = minera::discover_case_arrival(one);
  assert(single.arrival_average == 0.0 && single.dispersion_average == 0.0);
  // Review V2-5: pm4py.get_case_arrival_average on a one-case DataFrame
  // (and the dispersion average) is NaN; the JSON writes it as null.
  const auto single_frame = minera::discover_case_arrival(one, frame);
  assert(std::isnan(single_frame.arrival_average) &&
         std::isnan(single_frame.dispersion_average));
  const auto single_json = minera::case_arrival_json(single_frame);
  assert(single_json.find("\"arrival_average\":null,\"dispersion_average\":null") !=
         std::string::npos);

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
