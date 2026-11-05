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
        if (result.starts[*value]++ == 0) result.start_order.push_back(*value);
    }
    if (include_end) {
      if (const auto* value = activity_value(trace.events.back(),
                                             options.activity_key))
        if (result.ends[*value]++ == 0) result.end_order.push_back(*value);
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

const char* activity_position_summary_variant_name(
    ActivityPositionSummaryVariant variant) {
  switch (variant) {
    case ActivityPositionSummaryVariant::kPandas: return "PANDAS";
    case ActivityPositionSummaryVariant::kLog: return "LOG";
  }
  throw std::invalid_argument("activity position summary: unknown variant");
}

ActivityPositionSummary get_activity_position_summary(
    const EventLog& log, const ActivityPositionSummaryOptions& options) {
  if (options.variant != ActivityPositionSummaryVariant::kPandas &&
      options.variant != ActivityPositionSummaryVariant::kLog)
    throw std::invalid_argument("activity position summary: unknown variant");
  ActivityPositionSummary result;
  result.activity = options.activity;
  result.activity_key = options.activity_key;
  result.variant = options.variant;
  // Positions count every event of the case, also those without the key.
  std::map<std::uint64_t, std::size_t> index;
  for (const auto& trace : log.traces) {
    for (std::size_t position = 0; position < trace.events.size(); ++position) {
      const auto* value = activity_value(trace.events[position], options.activity_key);
      if (!value || *value != options.activity) continue;
      const auto [it, inserted] = index.emplace(position, result.positions.size());
      if (inserted) result.positions.push_back({position, 0});
      ++result.positions[it->second].second;
    }
  }
  if (options.variant == ActivityPositionSummaryVariant::kPandas)
    std::stable_sort(result.positions.begin(), result.positions.end(),
                     [](const auto& left, const auto& right) {
                       return left.second > right.second;
                     });
  return result;
}

}  // namespace minera
