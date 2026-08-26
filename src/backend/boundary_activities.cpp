#include "minera/boundary_activities.hpp"

#include <stdexcept>
#include <string_view>
#include <algorithm>

namespace minera {
namespace {

const std::string* activity_value(const Event& event, std::string_view key) {
  if (key.empty() || key == "activity" || key == "concept:name")
    return &event.activity;
  if (key == "timestamp" || key == "time:timestamp") return &event.timestamp;
  if (key == "resource" || key == "org:resource") return &event.resource;
  if (key == "lifecycle" || key == "lifecycle:transition")
    return &event.lifecycle;
  return event.attribute(key);
}

}  // namespace

namespace {

SortedBoundaryActivities sorted_counts(const BoundaryActivityCounts& counts) {
  SortedBoundaryActivities result(counts.begin(), counts.end());
  std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
    if (left.second != right.second) return left.second > right.second;
    return left.first < right.first;
  });
  return result;
}

}  // namespace

const char* boundary_activities_variant_name(
    BoundaryActivitiesVariant variant) {
  switch (variant) {
    case BoundaryActivitiesVariant::kStart: return "START";
    case BoundaryActivitiesVariant::kEnd: return "END";
    case BoundaryActivitiesVariant::kBoth: return "BOTH";
  }
  throw std::invalid_argument("boundary activities: unknown variant");
}

BoundaryActivitiesResult discover_boundary_activities(
    const EventLog& log, const BoundaryActivitiesOptions& options) {
  BoundaryActivitiesResult result;
  result.variant = options.variant;
  if (options.variant != BoundaryActivitiesVariant::kStart &&
      options.variant != BoundaryActivitiesVariant::kEnd &&
      options.variant != BoundaryActivitiesVariant::kBoth) {
    throw std::invalid_argument("boundary activities: unknown variant");
  }
  const bool include_start = options.variant != BoundaryActivitiesVariant::kEnd;
  const bool include_end = options.variant != BoundaryActivitiesVariant::kStart;
  for (const auto& trace : log.traces) {
    if (trace.events.empty()) continue;
    if (include_start) {
      if (const auto* value = activity_value(trace.events.front(),
                                             options.activity_key))
        ++result.starts[*value];
    }
    if (include_end) {
      if (const auto* value = activity_value(trace.events.back(),
                                             options.activity_key))
        ++result.ends[*value];
    }
  }
  return result;
}

BoundaryActivityCounts get_start_activities(const EventLog& log,
                                            std::string activity_key) {
  BoundaryActivitiesOptions options;
  options.variant = BoundaryActivitiesVariant::kStart;
  options.activity_key = std::move(activity_key);
  return discover_boundary_activities(log, options).starts;
}

BoundaryActivityCounts get_end_activities(const EventLog& log,
                                          std::string activity_key) {
  BoundaryActivitiesOptions options;
  options.variant = BoundaryActivitiesVariant::kEnd;
  options.activity_key = std::move(activity_key);
  return discover_boundary_activities(log, options).ends;
}

SortedBoundaryActivities get_sorted_start_activities_list(
    const EventLog& log, std::string activity_key) {
  return sorted_counts(get_start_activities(log, std::move(activity_key)));
}

SortedBoundaryActivities get_sorted_end_activities_list(
    const EventLog& log, std::string activity_key) {
  return sorted_counts(get_end_activities(log, std::move(activity_key)));
}

}  // namespace minera
