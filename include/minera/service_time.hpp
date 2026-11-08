#pragma once

#include <map>
#include <string>
#include <vector>

#include "minera/dfg_variants.hpp"
#include "minera/event_log.hpp"

namespace minera {

struct ServiceTimeOptions {
  // Empty, "activity", and "concept:name" select Event::activity.
  std::string activity_key = "activity";
  // Empty, "timestamp", and "time:timestamp" select Event::timestamp.
  std::string start_timestamp_key = "timestamp";
  std::string timestamp_key = "timestamp";
  DfgPerformanceAggregation aggregation = DfgPerformanceAggregation::kMean;
  bool use_business_hours = false;
  std::vector<BusinessHourSlot> business_hour_slots;
  // Business-hour clock: false counts each timestamp's own wall clock (the
  // EventLog path); true counts the UTC clock, as pm4py's DataFrame path does
  // after format_dataframe converts a CSV offset.
  bool dataframe = false;
};

struct ServiceTimeResult {
  DfgPerformanceAggregation aggregation = DfgPerformanceAggregation::kMean;
  std::map<std::string, double> values;
};

[[nodiscard]] ServiceTimeResult discover_service_time(
    const EventLog& log, const ServiceTimeOptions& options = {});

[[nodiscard]] inline ServiceTimeResult service_time(
    const EventLog& log, const ServiceTimeOptions& options = {}) {
  return discover_service_time(log, options);
}

[[nodiscard]] const char* service_time_aggregation_name(
    DfgPerformanceAggregation aggregation);

}  // namespace minera
