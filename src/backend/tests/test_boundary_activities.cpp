#include "minera/boundary_activities.hpp"
#include "minera/json_export.hpp"

#include <cassert>
#include <iostream>

namespace {

minera::Event event(std::string activity, std::string phase) {
  minera::Event value;
  value.activity = std::move(activity);
  value.attributes.push_back({"phase", std::move(phase),
                              minera::AttributeKind::kString});
  return value;
}

minera::EventLog fixture() {
  minera::EventLog log;
  minera::Trace first;
  first.events = {event("A", "open"), event("B", "close")};
  minera::Trace second;
  second.events = {event("A", "open"), event("C", "close")};
  minera::Trace empty;
  log.traces = {std::move(first), std::move(second), std::move(empty)};
  return log;
}

}  // namespace

int main() {
  const auto log = fixture();
  const auto both = minera::discover_boundary_activities(log);
  assert(both.starts.at("A") == 2);
  assert(both.ends.at("B") == 1 && both.ends.at("C") == 1);
  const auto sorted_starts = minera::get_sorted_start_activities_list(log);
  assert(sorted_starts.size() == 1 && sorted_starts.front().first == "A");
  const auto sorted_ends = minera::get_sorted_end_activities_list(log);
  assert(sorted_ends.size() == 2 && sorted_ends.front().first == "B");
  const auto json = minera::boundary_activities_json(both);
  assert(json.find("\"family\":\"BOUNDARY_ACTIVITIES\"") !=
         std::string::npos);

  minera::BoundaryActivitiesOptions options;
  options.variant = minera::BoundaryActivitiesVariant::kStart;
  options.activity_key = "phase";
  const auto starts = minera::discover_boundary_activities(log, options);
  assert(starts.starts.at("open") == 2);
  assert(starts.ends.empty());
  assert(std::string(minera::boundary_activities_variant_name(
             starts.variant)) == "START");

  options.variant = minera::BoundaryActivitiesVariant::kEnd;
  const auto ends = minera::discover_boundary_activities(log, options);
  assert(ends.starts.empty());
  assert(ends.ends.at("close") == 2);
  std::cout << "Boundary-activities tests passed\n";
}
