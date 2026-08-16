#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "minera/event_log.hpp"

namespace minera {

// Metrics exposed by the organizational resource-profile perspective.
enum class ResourceProfileMetric {
  kDistinctActivities,
  kActivityFrequency,
  kActivityCompletions,
  kCaseCompletions,
  kFractionCaseCompletions,
  kAverageWorkload,
  kMultitasking,
  kAverageDurationActivity,
  kAverageCaseDuration,
  kInteractionTwoResources,
  kSocialPosition,
  DISTINCT_ACTIVITIES = kDistinctActivities,
  ACTIVITY_FREQUENCY = kActivityFrequency,
  ACTIVITY_COMPLETIONS = kActivityCompletions,
  CASE_COMPLETIONS = kCaseCompletions,
  FRACTION_CASE_COMPLETIONS = kFractionCaseCompletions,
  AVERAGE_WORKLOAD = kAverageWorkload,
  MULTITASKING = kMultitasking,
  AVERAGE_DURATION_ACTIVITY = kAverageDurationActivity,
  AVERAGE_CASE_DURATION = kAverageCaseDuration,
  INTERACTION_TWO_RESOURCES = kInteractionTwoResources,
  SOCIAL_POSITION = kSocialPosition,
};

struct ResourceProfilesOptions {
  ResourceProfileMetric metric = ResourceProfileMetric::kDistinctActivities;
  // Standard selectors resolve first-class Event/Trace fields.  Custom keys
  // are read from typed event or trace attributes.
  std::string activity_attribute = "concept:name";
  std::string resource_attribute = "org:resource";
  std::string timestamp_attribute = "time:timestamp";
  std::string start_timestamp_attribute;
  std::string case_id_attribute = "case:concept:name";
  // Empty bounds are unbounded. Bounds are half-open [t1,t2).
  std::string t1;
  std::string t2;
  std::string resource;
  std::string activity;
  std::string resource2;
  std::size_t max_resources = 10000;
  std::size_t max_rows = 100000;
};

struct ResourceProfileRow {
  std::string resource;
  std::string activity;
  std::string resource2;
  double value = 0.0;
  std::uint64_t count = 0;
};

struct ResourceProfilesDiagnostics {
  bool computed = false;
  bool output_truncated = false;
  std::size_t trace_count = 0;
  std::size_t event_count = 0;
  std::size_t resource_count = 0;
  std::string message;
};

struct ResourceProfilesResult {
  ResourceProfileMetric metric = ResourceProfileMetric::kDistinctActivities;
  std::vector<ResourceProfileRow> rows;
  ResourceProfilesDiagnostics diagnostics;
};

using ResourceProfileOptions = ResourceProfilesOptions;
using ResourceProfileResult = ResourceProfilesResult;

[[nodiscard]] ResourceProfilesResult discover_resource_profiles(
    const EventLog& log, const ResourceProfilesOptions& options = {});

inline ResourceProfilesResult mine_resource_profiles(
    const EventLog& log, const ResourceProfilesOptions& options = {}) {
  return discover_resource_profiles(log, options);
}

[[nodiscard]] std::string resource_profiles_json(
    const ResourceProfilesResult& result);

[[nodiscard]] const char* resource_profile_metric_name(ResourceProfileMetric metric);
[[nodiscard]] ResourceProfileMetric resource_profile_metric_from_string(std::string_view name);

}  // namespace minera
