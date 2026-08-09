#pragma once

#include <string>

#include "minera/event_log.hpp"

namespace minera {

enum class CaseGroupingFilterType {
  kFirst,
  kLast,
};

enum class TimestampCaseGroupingType {
  kFirst,
  kLast,
  kConcat,
};

struct ConsecutiveActivityGroupingOptions {
  CaseGroupingFilterType filter_type = CaseGroupingFilterType::kFirst;
  // Empty, "activity", and "concept:name" select Event::activity. Other
  // keys compare the corresponding first-class or generic event attribute.
  std::string activity_key;
};

struct TimestampCaseGroupingOptions {
  TimestampCaseGroupingType filter_type = TimestampCaseGroupingType::kFirst;
  std::string activity_key;
  std::string timestamp_key;
  std::string concat_separator = " & ";
};

[[nodiscard]] EventLog group_consecutive_activities(
    const EventLog& event_log,
    const ConsecutiveActivityGroupingOptions& options = {});

[[nodiscard]] EventLog filter_consecutive_activity_groups(
    const EventLog& event_log,
    const ConsecutiveActivityGroupingOptions& options = {});

[[nodiscard]] EventLog group_same_timestamp_events(
    const EventLog& event_log,
    const TimestampCaseGroupingOptions& options = {});

[[nodiscard]] EventLog filter_timestamp_case_groups(
    const EventLog& event_log,
    const TimestampCaseGroupingOptions& options = {});

}  // namespace minera
