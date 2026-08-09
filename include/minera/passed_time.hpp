#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "minera/dfg_variants.hpp"
#include "minera/event_log.hpp"

namespace minera {

enum class PassedTimeVariant {
  kPre,
  kPost,
  kPrePost,

  PRE = kPre,
  POST = kPost,
  PREPOST = kPrePost,
};

struct PassedTimeOptions {
  std::string activity;
  PassedTimeVariant variant = PassedTimeVariant::kPre;
  // Empty, "activity", and "concept:name" select Event::activity. Other
  // values are resolved from the event attribute map.
  std::string activity_key = "activity";
  // Empty, "timestamp", and "time:timestamp" select Event::timestamp.
  std::string timestamp_key = "timestamp";
  // Empty uses timestamp_key for the start of the current event. Otherwise
  // the named event attribute supplies the start timestamp.
  std::string start_timestamp_key;
  DfgPerformanceAggregation performance_aggregation =
      DfgPerformanceAggregation::kMean;
  bool use_business_hours = false;
  std::vector<BusinessHourSlot> business_hour_slots;
};

struct PassedTimeEntry {
  std::string activity;
  double performance = 0.0;
  std::uint64_t frequency = 0;

  bool operator==(const PassedTimeEntry&) const = default;
};

struct PassedTimeResult {
  PassedTimeVariant variant = PassedTimeVariant::kPre;
  std::string activity;
  std::vector<PassedTimeEntry> pre;
  std::vector<PassedTimeEntry> post;
  double pre_avg_perf = 0.0;
  double post_avg_perf = 0.0;
};

[[nodiscard]] PassedTimeResult discover_passed_time(
    const EventLog& log, const PassedTimeOptions& options);

[[nodiscard]] inline PassedTimeResult passed_time(
    const EventLog& log, const PassedTimeOptions& options) {
  return discover_passed_time(log, options);
}

[[nodiscard]] const char* passed_time_variant_name(PassedTimeVariant variant);

}  // namespace minera
