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

}  // namespace minera
