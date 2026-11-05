#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "minera/event_log.hpp"

namespace minera {

enum class BoundaryActivitiesVariant {
  kStart,
  kEnd,
  kBoth,

  START = kStart,
  END = kEnd,
  BOTH = kBoth,
};

struct BoundaryActivitiesOptions {
  BoundaryActivitiesVariant variant = BoundaryActivitiesVariant::kBoth;
  // Empty, "activity", and "concept:name" select Event::activity.
  std::string activity_key = "activity";
};

struct BoundaryActivitiesResult {
  BoundaryActivitiesVariant variant = BoundaryActivitiesVariant::kBoth;
  std::map<std::string, std::uint64_t> starts;
  std::map<std::string, std::uint64_t> ends;
  // The keys of `starts` and `ends` in first appearance over the traces, the
  // key order of the reference's get_start_activities/get_end_activities
  // dicts (an EventLog and a format_dataframe frame agree).
  std::vector<std::string> start_order;
  std::vector<std::string> end_order;
};

using BoundaryActivityCounts = std::map<std::string, std::uint64_t>;
using SortedBoundaryActivities =
    std::vector<std::pair<std::string, std::uint64_t>>;

[[nodiscard]] BoundaryActivitiesResult discover_boundary_activities(
    const EventLog& log, const BoundaryActivitiesOptions& options = {});

[[nodiscard]] inline BoundaryActivitiesResult boundary_activities(
    const EventLog& log, const BoundaryActivitiesOptions& options = {}) {
  return discover_boundary_activities(log, options);
}

[[nodiscard]] const char* boundary_activities_variant_name(
    BoundaryActivitiesVariant variant);

[[nodiscard]] BoundaryActivityCounts get_start_activities(
    const EventLog& log, std::string activity_key = "activity");
[[nodiscard]] BoundaryActivityCounts get_end_activities(
    const EventLog& log, std::string activity_key = "activity");
[[nodiscard]] SortedBoundaryActivities get_sorted_start_activities_list(
    const EventLog& log, std::string activity_key = "activity");
[[nodiscard]] SortedBoundaryActivities get_sorted_end_activities_list(
    const EventLog& log, std::string activity_key = "activity");

// pm4py.get_activity_position_summary: how many times the activity occurs at
// each 0-based position of its case. kPandas (the DataFrame a CSV or XES
// import gives) lists positions by descending count, equal counts in first
// appearance; kLog (an EventLog object) lists them in first appearance.
enum class ActivityPositionSummaryVariant { kPandas, kLog };

struct ActivityPositionSummaryOptions {
  std::string activity;
  // Empty, "activity", and "concept:name" select Event::activity.
  std::string activity_key = "activity";
  ActivityPositionSummaryVariant variant = ActivityPositionSummaryVariant::kPandas;
};

struct ActivityPositionSummary {
  std::string activity;
  std::string activity_key;
  ActivityPositionSummaryVariant variant = ActivityPositionSummaryVariant::kPandas;
  // (position, count) in the reference dict's key order.
  std::vector<std::pair<std::uint64_t, std::uint64_t>> positions;
};

[[nodiscard]] ActivityPositionSummary get_activity_position_summary(
    const EventLog& log, const ActivityPositionSummaryOptions& options);

[[nodiscard]] const char* activity_position_summary_variant_name(
    ActivityPositionSummaryVariant variant);

}  // namespace minera
