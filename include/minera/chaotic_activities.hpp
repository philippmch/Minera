#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "minera/event_log.hpp"

namespace minera {

struct ChaoticActivitiesOptions {
  // Empty, "activity", and "concept:name" use Event::activity. Any other
  // key is resolved from the event's typed attribute map.
  std::string activity_key = "activity";
  // Nullopt selects the default 1/|A| smoothing; zero requests raw entropy.
  std::optional<double> alpha;
  std::size_t max_activities = 100000;
  std::size_t max_events = 1000000;
};

struct ChaoticActivityMetrics {
  std::string activity;
  std::uint64_t frequency = 0;
  double entropy = 0.0;
  double entropy_smooth = 0.0;
  double entropy_gain = 0.0;
  double chaotic_score = 0.0;

  bool operator==(const ChaoticActivityMetrics&) const = default;
};

struct ChaoticActivitiesDiagnostics {
  double alpha = 0.0;
  std::size_t activity_count = 0;
  std::size_t event_count = 0;
  bool output_truncated = false;
  std::string message;
};

struct ChaoticActivitiesResult {
  std::vector<ChaoticActivityMetrics> activities;
  ChaoticActivitiesDiagnostics diagnostics;
};

[[nodiscard]] ChaoticActivitiesResult discover_chaotic_activities(
    const EventLog& log, const ChaoticActivitiesOptions& options = {});

[[nodiscard]] inline ChaoticActivitiesResult chaotic_activities(
    const EventLog& log, const ChaoticActivitiesOptions& options = {}) {
  return discover_chaotic_activities(log, options);
}

}  // namespace minera
