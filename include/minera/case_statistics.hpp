#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "minera/event_log.hpp"
#include "minera/temporal_profile.hpp"

namespace minera {

enum class CaseStatisticsSortBy {
  kCaseId,
  kStartTime,
  kEndTime,
  kDuration,

  CASE_ID = kCaseId,
  START_TIME = kStartTime,
  END_TIME = kEndTime,
  DURATION = kDuration,
};

struct CaseStatisticsOptions {
  std::string case_id_key = "case_id";
  // Empty, "timestamp", and "time:timestamp" select Event::timestamp.
  std::string timestamp_key = "timestamp";
  bool enable_sort = true;
  CaseStatisticsSortBy sort_by = CaseStatisticsSortBy::kCaseId;
  bool sort_ascending = true;
  std::size_t max_cases = 0;  // zero means return every case
  bool use_business_hours = false;
  std::vector<BusinessHourSlot> business_hour_slots;
};

struct CaseStatisticsEntry {
  std::string case_id;
  double start_time = 0.0;
  double end_time = 0.0;
  double case_duration = 0.0;
};

struct CaseStatisticsResult {
  CaseStatisticsOptions options;
  std::vector<CaseStatisticsEntry> cases;
};

[[nodiscard]] CaseStatisticsResult discover_case_statistics(
    const EventLog& log, const CaseStatisticsOptions& options = {});

[[nodiscard]] inline CaseStatisticsResult case_statistics(
    const EventLog& log, const CaseStatisticsOptions& options = {}) {
  return discover_case_statistics(log, options);
}

[[nodiscard]] const char* case_statistics_sort_name(
    CaseStatisticsSortBy sort_by);

}  // namespace minera
