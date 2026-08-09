#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "minera/event_log.hpp"
#include "minera/temporal_profile.hpp"

namespace minera {

struct CaseArrivalOptions {
  // Empty, "timestamp", and "time:timestamp" select Event::timestamp.
  std::string timestamp_key = "timestamp";
  bool use_business_hours = false;
  // Empty selects Monday-Friday 07:00-17:00.
  std::vector<BusinessHourSlot> business_hour_slots;
};

struct CaseArrivalResult {
  double arrival_average = 0.0;
  double dispersion_average = 0.0;
  std::size_t timed_starts = 0;
  std::size_t timed_ends = 0;
};

[[nodiscard]] CaseArrivalResult discover_case_arrival(
    const EventLog& log, const CaseArrivalOptions& options = {});

[[nodiscard]] inline CaseArrivalResult case_arrival(
    const EventLog& log, const CaseArrivalOptions& options = {}) {
  return discover_case_arrival(log, options);
}

[[nodiscard]] inline double get_case_arrival_average(
    const EventLog& log, const CaseArrivalOptions& options = {}) {
  return discover_case_arrival(log, options).arrival_average;
}

[[nodiscard]] inline double get_case_dispersion_average(
    const EventLog& log, const CaseArrivalOptions& options = {}) {
  return discover_case_arrival(log, options).dispersion_average;
}

}  // namespace minera
