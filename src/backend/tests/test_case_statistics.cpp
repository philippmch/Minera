#include "minera/case_statistics.hpp"
#include "minera/json_export.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {

minera::Trace trace(std::string id, std::string start, std::string finish) {
  minera::Trace value;
  value.case_id = std::move(id);
  minera::Event first;
  first.timestamp = std::move(start);
  minera::Event last;
  last.timestamp = std::move(finish);
  value.events = {std::move(first), std::move(last)};
  return value;
}

}  // namespace

int main() {
  minera::EventLog log;
  log.traces = {trace("b", "2024-01-01T10:00:00Z",
                      "2024-01-01T11:00:00Z"),
                trace("a", "2024-01-01T08:00:00Z",
                      "2024-01-01T09:00:00Z")};
  const auto result = minera::discover_case_statistics(log);
  assert(result.cases.size() == 2);
  assert(result.cases.front().case_id == "a");
  assert(std::abs(result.cases.front().case_duration - 3600.0) < 1e-9);
  const auto json = minera::case_statistics_json(result);
  assert(json.find("\"family\":\"CASE_STATISTICS\"") !=
         std::string::npos);
  minera::CaseStatisticsOptions options;
  options.sort_by = minera::CaseStatisticsSortBy::kDuration;
  options.sort_ascending = false;
  options.max_cases = 1;
  options.use_business_hours = true;
  options.business_hour_slots = {{8.0 * 3600.0, 12.0 * 3600.0}};
  const auto limited = minera::discover_case_statistics(log, options);
  assert(limited.cases.size() == 1);
  assert(std::string(minera::case_statistics_sort_name(options.sort_by)) ==
         "DURATION");

  bool rejected = false;
  try {
    minera::EventLog invalid;
    invalid.traces = {trace("x", "bad", "2024-01-01T09:00:00Z")};
    (void)minera::discover_case_statistics(invalid);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);

  // A trace whose last event precedes its first: the reference
  // get_all_case_durations returns -3600 for it rather than failing.
  minera::EventLog backwards;
  backwards.traces = {trace("back", "2024-01-01T10:00:00Z",
                            "2024-01-01T09:00:00Z")};
  const auto negative = minera::discover_case_statistics(backwards);
  assert(negative.cases.size() == 1);
  assert(negative.cases.front().case_duration == -3600.0);
  assert(minera::case_statistics_json(negative).find(
             "\"case_duration\":-3600") != std::string::npos);
  std::cout << "Case-statistics tests passed\n";
}
