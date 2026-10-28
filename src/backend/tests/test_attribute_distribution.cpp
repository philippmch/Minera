#include "minera/attribute_distribution.hpp"
#include "minera/json_export.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <stdexcept>

namespace {

minera::Event event(std::string timestamp) {
  minera::Event value;
  value.timestamp = std::move(timestamp);
  return value;
}

}  // namespace

int main() {
  minera::EventLog log;
  minera::Trace trace;
  trace.events = {event("2024-01-01T01:00:00Z"),
                  event("2024-02-03T23:00:00Z")};
  log.traces.push_back(std::move(trace));

  auto options = minera::AttributeDistributionOptions{};
  options.distribution = minera::EventDistributionType::kMonths;
  const auto months = minera::discover_event_distribution(log, options);
  assert(months.labels.size() == 12 && months.counts.size() == 12);
  assert(months.counts[0] == 1 && months.counts[1] == 1);
  assert(months.timed_events == 2);
  const auto json = minera::attribute_distribution_json(months);
  assert(json.find("\"family\":\"ATTRIBUTE_DISTRIBUTION\"") !=
         std::string::npos);

  options.distribution = minera::EventDistributionType::kDaysWeek;
  const auto weekdays = minera::discover_event_distribution(log, options);
  assert(weekdays.labels.front() == "Monday");
  assert(weekdays.labels.back() == "Sunday");
  assert(std::string(minera::event_distribution_name(
             minera::EventDistributionType::kWeeks)) == "WEEKS");

  options.distribution = minera::EventDistributionType::kWeeks;
  log.traces.front().events.push_back(event("2024-12-31T23:00:00Z"));
  const auto weeks = minera::discover_event_distribution(log, options);
  const auto week_one = std::find(weeks.labels.begin(), weeks.labels.end(), "01");
  assert(week_one != weeks.labels.end());

  log.traces.front().events.pop_back();
  options.timestamp_key = "occurred_at";
  options.distribution = minera::EventDistributionType::kHours;
  log.traces.front().events.front().attributes.push_back(
      {"occurred_at", "2024-03-01T12:00:00Z"});
  log.traces.front().events.back().attributes.push_back(
      {"occurred_at", "2024-03-01T13:00:00Z"});
  const auto hours = minera::discover_event_distribution(log, options);
  assert(hours.counts[12] == 1 && hours.counts[13] == 1);

  bool rejected = false;
  try {
    options.timestamp_key = "missing";
    (void)minera::discover_event_distribution(log, options);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);

  // Reference: YEARS spans min..max of the observed years, so a log without
  // timed events raises; the fixed-range buckets stay all zero.
  const minera::EventLog untimed{.traces = {minera::Trace{}}};
  rejected = false;
  try {
    options.timestamp_key = "time:timestamp";
    options.distribution = minera::EventDistributionType::kYears;
    (void)minera::discover_event_distribution(untimed, options);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
  options.distribution = minera::EventDistributionType::kHours;
  const auto empty_hours = minera::discover_event_distribution(untimed, options);
  assert(empty_hours.labels.size() == 24 && empty_hours.timed_events == 0);
  std::cout << "Attribute-distribution tests passed\n";
}
