#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "minera/event_log.hpp"

namespace minera {

enum class EventDistributionType {
  kDaysMonth,
  kMonths,
  kYears,
  kHours,
  kDaysWeek,
  kWeeks,

  DAYS_MONTH = kDaysMonth,
  MONTHS = kMonths,
  YEARS = kYears,
  HOURS = kHours,
  DAYS_WEEK = kDaysWeek,
  WEEKS = kWeeks,
};

struct AttributeDistributionOptions {
  EventDistributionType distribution = EventDistributionType::kDaysMonth;
  std::string timestamp_key = "timestamp";
};

struct AttributeDistributionResult {
  AttributeDistributionOptions options;
  std::vector<std::string> labels;
  std::vector<std::uint64_t> counts;
  std::uint64_t timed_events = 0;
};

[[nodiscard]] AttributeDistributionResult discover_event_distribution(
    const EventLog& log, const AttributeDistributionOptions& options = {});

[[nodiscard]] inline AttributeDistributionResult event_distribution(
    const EventLog& log, const AttributeDistributionOptions& options = {}) {
  return discover_event_distribution(log, options);
}

[[nodiscard]] inline AttributeDistributionResult get_events_distribution(
    const EventLog& log, EventDistributionType distribution,
    std::string timestamp_key = "timestamp") {
  AttributeDistributionOptions options;
  options.distribution = distribution;
  options.timestamp_key = std::move(timestamp_key);
  return discover_event_distribution(log, options);
}

[[nodiscard]] const char* event_distribution_name(EventDistributionType type);

}  // namespace minera
