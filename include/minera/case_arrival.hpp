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
  // How the gaps are averaged. false: as pm4py's EventLog path, for a
  // native JSON or EventLog input: each gap floored to the microsecond, then
  // statistics.mean (the exact sum divided by the count, rounded once).
  // true: as pm4py's DataFrame path (what pm4py.get_case_arrival_average
  // returns for a CSV or XES read as a DataFrame): each gap is the whole
  // seconds plus the microseconds times 1e-6 plus the nanoseconds left times
  // 1e-9, and the mean is numpy's pairwise float64 sum divided by the count
  // (observed black-box).
  bool dataframe = false;
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
