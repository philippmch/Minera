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
// The same grouping on a CSV or XES upload, as pm4py's DataFrame grouping
// does it: only rows adjacent in the frame's row order merge, so on an XES
// upload whose trace names repeat apart a case's equal activities with
// another case's rows between them stay apart (round BI, observed
// black-box).
[[nodiscard]] EventLog group_consecutive_frame_rows(
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
