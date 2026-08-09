#include "minera/json_export.hpp"
#include "minera/rework.hpp"

#include <cassert>
#include <iostream>

namespace {

minera::Event make_event(std::string activity) {
  minera::Event event;
  event.activity = std::move(activity);
  return event;
}

minera::EventLog fixture() {
  minera::EventLog log;
  minera::Trace first;
  first.case_id = "c1";
  first.events = {make_event("A"), make_event("B"), make_event("A")};
  minera::Trace second;
  second.case_id = "c2";
  second.events = {make_event("A"), make_event("B")};
  log.traces = {std::move(first), std::move(second)};
  return log;
}

}  // namespace

int main() {
  const auto log = fixture();
  minera::ReworkOptions options;
  const auto activities = minera::discover_rework(log, options);
  assert(activities.activity_cases.size() == 1);
  assert(activities.activity_cases.at("A") == 1);
  assert(activities.cases.empty());
  const auto activity_json = minera::rework_json(activities);
  assert(activity_json.find("\"variant\":\"ACTIVITIES\"") !=
         std::string::npos);

  options.variant = minera::ReworkVariant::kCases;
  const auto cases = minera::discover_rework(log, options);
  assert(cases.cases.at("c1") == (minera::ReworkCaseStats{3, 1}));
  assert(cases.cases.at("c2") == (minera::ReworkCaseStats{2, 0}));
  assert(cases.activity_cases.empty());
  const auto case_json = minera::rework_json(cases);
  assert(case_json.find("\"c1\"") != std::string::npos);
  std::cout << "Rework tests passed\n";
}
