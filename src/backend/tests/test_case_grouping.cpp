#include "minera/case_grouping.hpp"

#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

minera::EventLog sample_log() {
  minera::EventLog log;
  minera::Trace trace;
  trace.case_id = "case-1";
  trace.events = {
      {"case-1", "A", "2024-01-01T00:00:00Z", "", 0, {}, ""},
      {"case-1", "A", "2024-01-01T00:00:00Z", "", 1, {}, ""},
      {"case-1", "B", "2024-01-01T00:00:00Z", "", 2, {}, ""},
      {"case-1", "B", "2024-01-01T00:01:00Z", "", 3, {}, ""},
      {"case-1", "A", "2024-01-01T00:01:00Z", "", 4, {}, ""},
  };
  log.traces.push_back(std::move(trace));
  return log;
}

std::vector<std::string> activities(const minera::Trace& trace) {
  std::vector<std::string> result;
  for (const auto& event : trace.events) result.push_back(event.activity);
  return result;
}

}  // namespace

int main() {
  const auto log = sample_log();
  minera::ConsecutiveActivityGroupingOptions first;
  const auto first_grouped = minera::group_consecutive_activities(log, first);
  assert(activities(first_grouped.traces[0]) ==
         std::vector<std::string>({"A", "B", "A"}));

  first.filter_type = minera::CaseGroupingFilterType::kLast;
  const auto last_grouped = minera::filter_consecutive_activity_groups(log, first);
  assert(activities(last_grouped.traces[0]) ==
         std::vector<std::string>({"A", "B", "A"}));
  assert(last_grouped.traces[0].events[0].input_order == 1);

  minera::TimestampCaseGroupingOptions timestamp;
  const auto timestamp_first = minera::group_same_timestamp_events(log, timestamp);
  assert(activities(timestamp_first.traces[0]) ==
         std::vector<std::string>({"A", "B"}));
  timestamp.filter_type = minera::TimestampCaseGroupingType::kLast;
  const auto timestamp_last = minera::filter_timestamp_case_groups(log, timestamp);
  assert(activities(timestamp_last.traces[0]) ==
         std::vector<std::string>({"B", "A"}));

  timestamp.filter_type = minera::TimestampCaseGroupingType::kConcat;
  const auto concatenated = minera::group_same_timestamp_events(log, timestamp);
  assert(activities(concatenated.traces[0]) ==
         std::vector<std::string>({"A & A & B", "A & B"}));

  timestamp.activity_key = "channel";
  timestamp.concat_separator = "+";
  minera::EventLog generic = log;
  generic.traces[0].events[0].attributes.push_back(
      {"channel", "z", minera::AttributeKind::kString});
  generic.traces[0].events[1].attributes.push_back(
      {"channel", "a", minera::AttributeKind::kString});
  const auto generic_result = minera::group_same_timestamp_events(generic, timestamp);
  assert(generic_result.traces[0].events[0].attribute("channel") != nullptr);
  assert(*generic_result.traces[0].events[0].attribute("channel") == "a+z");

  timestamp.concat_separator.clear();
  bool threw = false;
  try {
    (void)minera::group_same_timestamp_events(log, timestamp);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
  return 0;
}
