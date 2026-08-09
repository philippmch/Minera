#include "minera/boundary_activities.hpp"

#include <stdexcept>
#include <string_view>

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

}  // namespace minera
