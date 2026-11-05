#include "minera/boundary_activities.hpp"
#include "minera/json_export.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

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

  // Reference get_start_activities/get_end_activities keep first appearance
  // (an EventLog and a format_dataframe frame agree): Z before A.
  {
    minera::EventLog ordered;
    for (const auto& [first, last] : std::vector<std::pair<std::string, std::string>>{
             {"Z", "Q"}, {"A", "M"}, {"Z", "B"}}) {
      minera::Trace trace;
      trace.events = {event(first, "x"), event(last, "y")};
      ordered.traces.push_back(std::move(trace));
    }
    const auto json = minera::boundary_activities_json(
        minera::discover_boundary_activities(ordered));
    assert(json.find("\"starts\":{\"Z\":2,\"A\":1},\"ends\":{\"Q\":1,\"M\":1,\"B\":1}") !=
           std::string::npos);
    // A hand-built result without the order keeps name order.
    minera::BoundaryActivitiesResult manual;
    manual.starts = {{"b", 1}, {"a", 2}};
    assert(minera::boundary_activities_json(manual).find(
               "\"starts\":{\"a\":2,\"b\":1}") != std::string::npos);
  }

  // pm4py.get_activity_position_summary: DataFrame order is descending count
  // with ties in first appearance; the EventLog order is first appearance.
  {
    minera::EventLog positions;
    for (const auto& activities : std::vector<std::vector<std::string>>{
             {"B", "A"}, {"A", "B", "A"}, {"C", "B", "B", "A"}, {"C", "A"}}) {
      minera::Trace trace;
      for (const auto& activity : activities) trace.events.push_back(event(activity, "p"));
      positions.traces.push_back(std::move(trace));
    }
    minera::ActivityPositionSummaryOptions summary_options;
    summary_options.activity = "A";
    const auto pandas = minera::get_activity_position_summary(positions, summary_options);
    using Positions = std::vector<std::pair<std::uint64_t, std::uint64_t>>;
    assert((pandas.positions == Positions{{1, 2}, {0, 1}, {2, 1}, {3, 1}}));
    summary_options.variant = minera::ActivityPositionSummaryVariant::kLog;
    const auto log_order = minera::get_activity_position_summary(positions, summary_options);
    assert((log_order.positions == Positions{{1, 2}, {0, 1}, {2, 1}, {3, 1}}));
    summary_options.activity = "B";
    assert((minera::get_activity_position_summary(positions, summary_options).positions ==
            Positions{{0, 1}, {1, 2}, {2, 1}}));
    summary_options.variant = minera::ActivityPositionSummaryVariant::kPandas;
    assert((minera::get_activity_position_summary(positions, summary_options).positions ==
            Positions{{1, 2}, {0, 1}, {2, 1}}));
    summary_options.activity = "missing";
    const auto none = minera::get_activity_position_summary(positions, summary_options);
    assert(none.positions.empty());
    assert(minera::activity_position_summary_json(none) ==
           "{\"family\":\"ACTIVITY_POSITION_SUMMARY\",\"variant\":\"PANDAS\","
           "\"activity\":\"missing\",\"activity_key\":\"activity\",\"positions\":[]}");
    summary_options.activity = "B";
    assert(minera::activity_position_summary_json(
               minera::get_activity_position_summary(positions, summary_options))
               .ends_with("\"positions\":[[1,2],[0,1],[2,1]]}"));
  }
  std::cout << "Boundary-activities tests passed\n";
}
