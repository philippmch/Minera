#include "minera/timestamp_interleavings.hpp"

#include "../exact_divide.hpp"
#include "../nanosecond_instant.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

minera::Event event(std::string case_id, std::string activity,
                    std::string timestamp, std::size_t index) {
  minera::Event value;
  value.case_id = std::move(case_id);
  value.activity = std::move(activity);
  value.timestamp = std::move(timestamp);
  value.input_order = index;
  return value;
}

minera::EventLog log_with(const std::string& case_id,
                          std::initializer_list<minera::Event> events) {
  minera::EventLog log;
  minera::Trace trace;
  trace.case_id = case_id;
  trace.events.assign(events.begin(), events.end());
  log.traces.push_back(std::move(trace));
  return log;
}

void finds_strict_lr_and_rl_crossings() {
  const auto left = log_with(
      "L", {event("L", "A", "2024-01-01T00:00:01Z", 0),
            event("L", "B", "2024-01-01T00:00:04Z", 1),
            event("L", "C", "2024-01-01T00:00:07Z", 2)});
  const auto right = log_with(
      "R", {event("R", "X", "2024-01-01T00:00:02Z", 0),
            event("R", "Y", "2024-01-01T00:00:05Z", 1),
            event("R", "Z", "2024-01-01T00:00:08Z", 2)});
  const auto result = minera::discover_timestamp_interleavings(
      left, right, {minera::CaseRelation{"L", "R"}});
  // The artificial boundaries make the opposite crossings at the
  // beginning/end of the related traces observable as well. Indices number
  // each log as start boundaries, events, end boundaries (A = 1, X = 1), and
  // rows follow the indices of the two pairs' first events.
  assert(result.interleavings.size() == 5);
  assert(result.interleavings[0].direction ==
         minera::TimestampInterleavingDirection::kRightToLeft);
  assert(result.interleavings[0].source_activity == "X" &&
         result.interleavings[0].target_activity == "B");
  assert(result.interleavings[0].left_index == 2 &&
         result.interleavings[0].right_index == 1 &&
         result.interleavings[0].timestamp_diff_seconds == 2.0);
  assert(result.interleavings[1].direction ==
         minera::TimestampInterleavingDirection::kLeftToRight);
  assert(result.interleavings[1].source_activity == "A" &&
         result.interleavings[1].target_activity == "X");
  assert(result.interleavings[1].left_index == 1 &&
         result.interleavings[1].right_index == 1 &&
         result.interleavings[1].timestamp_diff_seconds == 1.0);
  assert(result.interleavings[2].source_activity == "Y" &&
         result.interleavings[2].target_activity == "C");
  assert(result.interleavings[2].left_index == 3 &&
         result.interleavings[2].right_index == 2 &&
         result.interleavings[2].timestamp_diff_seconds == 2.0);
  assert(result.interleavings[4].source_activity == "Z" &&
         result.interleavings[4].target_activity == "■");
  assert(result.interleavings[4].target_timestamp == "2262-04-11T23:47:16.854775807Z");
}

void naive_timestamps_and_start_overflow_raise() {
  bool threw = false;
  try {
    (void)minera::discover_timestamp_interleavings(
        log_with("L", {event("L", "A", "2024-01-01T00:00:01", 0)}),
        log_with("R", {event("R", "X", "2024-01-01T00:00:02Z", 0)}),
        {minera::CaseRelation{"L", "R"}});
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
  // The start boundary precedes X, which precedes A: elapsed time from the
  // start boundary to X does not fit in nanoseconds.
  threw = false;
  try {
    (void)minera::discover_timestamp_interleavings(
        log_with("L", {event("L", "A", "2024-01-01T00:00:03Z", 0)}),
        log_with("R", {event("R", "X", "2024-01-01T00:00:02Z", 0),
                       event("R", "Y", "2024-01-01T00:00:04Z", 1)}),
        {minera::CaseRelation{"L", "R"}});
  } catch (const std::overflow_error&) {
    threw = true;
  }
  assert(threw);
}

void equality_and_unrelated_relations_are_excluded() {
  const auto left = log_with(
      "L", {event("L", "A", "2024-01-01T00:00:01Z", 0)});
  const auto right = log_with(
      "R", {event("R", "X", "2024-01-01T00:00:01Z", 0)});
  const auto result = minera::discover_timestamp_interleavings(
      left, right,
      {minera::CaseRelation{"L", "R"}, minera::CaseRelation{"missing", "R"}});
  assert(result.interleavings.empty());
}

void custom_timestamp_and_bound_are_strict() {
  auto left = log_with(
      "L", {event("L", "A", "", 0), event("L", "B", "", 1),
            event("L", "C", "", 2)});
  auto right = log_with(
      "R", {event("R", "X", "", 0), event("R", "Y", "", 1),
            event("R", "Z", "", 2)});
  const std::array<std::string, 3> left_times = {
      "2024-01-01T00:00:01Z", "2024-01-01T00:00:04Z",
      "2024-01-01T00:00:07Z"};
  const std::array<std::string, 3> right_times = {
      "2024-01-01T00:00:02Z", "2024-01-01T00:00:05Z",
      "2024-01-01T00:00:08Z"};
  for (auto* log : {&left, &right}) {
    for (auto& trace : log->traces) {
      const auto& times = log == &left ? left_times : right_times;
      for (std::size_t i = 0; i < trace.events.size(); ++i)
        trace.events[i].attributes.push_back({"started", times[i]});
    }
  }
  minera::TimestampInterleavingOptions options;
  options.timestamp_key = "started";
  options.max_interleavings = 5;
  assert(minera::discover_timestamp_interleavings(
             left, right, {minera::CaseRelation{"L", "R"}}, options)
             .interleavings.size() == 5);
  options.max_interleavings = 1;
  bool threw = false;
  try {
    (void)minera::discover_timestamp_interleavings(
        left, right, {minera::CaseRelation{"L", "R"}}, options);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void invalid_timestamps_and_json_contract() {
  auto left = log_with(
      "L", {event("L", "A", "not-a-time", 0),
            event("L", "B", "2024-01-01T00:00:03Z", 1)});
  const auto right = log_with(
      "R", {event("R", "X", "2024-01-01T00:00:02Z", 0),
            event("R", "Y", "2024-01-01T00:00:04Z", 1)});
  bool threw = false;
  try {
    (void)minera::discover_timestamp_interleavings(
        left, right, {minera::CaseRelation{"L", "R"}});
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  left.traces[0].events[0].timestamp = "2024-01-01T00:00:01Z";
  left.traces[0].events[0].activity = "A\"quoted";
  const auto json = minera::timestamp_interleavings_json(
      minera::discover_timestamp_interleavings(
          left, right, {minera::CaseRelation{"L", "R"}}));
  assert(json.find("TIMESTAMP_INTERLEAVINGS") != std::string::npos);
  assert(json.find("A\\\"quoted") != std::string::npos);
  assert(json.find("timestamp_diff") != std::string::npos);
}

void malformed_result_is_rejected() {
  minera::TimestampInterleavingsResult result;
  result.interleavings.push_back({
      "A", "B", "2026-01-01T00:00:00Z", "2026-01-01T00:00:01Z", 0, 0,
      static_cast<minera::TimestampInterleavingDirection>(99), 1.0});
  bool threw = false;
  try { (void)minera::timestamp_interleavings_json(result); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);

  result.interleavings.front().direction =
      minera::TimestampInterleavingDirection::kLeftToRight;
  result.interleavings.front().timestamp_diff_seconds =
      std::numeric_limits<double>::quiet_NaN();
  threw = false;
  try { (void)minera::timestamp_interleavings_json(result); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
}

}  // namespace

// Elapsed counts reach exactly INT64_MIN and INT64_MAX without an
// overflowing intermediate, and anything one nanosecond beyond is rejected.
void elapsed_nanoseconds_cover_the_whole_int64_range() {
  namespace ni = minera::nanosecond_instant;
  constexpr auto kMax = std::numeric_limits<std::int64_t>::max();
  constexpr auto kMin = std::numeric_limits<std::int64_t>::min();
  const ni::Instant epoch{0, 0};
  assert(ni::elapsed_nanoseconds(epoch, ni::kEndTime) == kMax);
  assert(ni::elapsed_nanoseconds(epoch, ni::kStartTime) == kMin);
  assert(ni::elapsed_nanoseconds(ni::kEndTime, epoch) == -kMax);
  assert(ni::elapsed_nanoseconds({0, 1}, ni::kEndTime) == kMax - 1);
  assert(ni::elapsed_nanoseconds({0, 1}, ni::kStartTime) == std::nullopt);
  assert(ni::elapsed_nanoseconds({-1, 999'999'999}, ni::kEndTime) == std::nullopt);
  assert(ni::elapsed_nanoseconds({0, 0}, {-9'223'372'037, 145'224'193}) == kMin + 1);
  assert(ni::elapsed_nanoseconds({0, 1}, {-9'223'372'037, 145'224'193}) == kMin);
  assert(ni::elapsed_nanoseconds({1, 0}, ni::kStartTime) == std::nullopt);
  assert(ni::elapsed_nanoseconds({-1, 0}, ni::kEndTime) == std::nullopt);
  assert(ni::elapsed_nanoseconds({5, 250}, {2, 500}) == -2'999'999'750);
}

// Through the public API: X at the epoch makes its X -> end row exactly
// INT64_MAX nanoseconds long, while A before it keeps start -> A in range.
void elapsed_to_end_boundary_at_int64_max() {
  const auto run = [](const std::string& x_timestamp) {
    return minera::discover_timestamp_interleavings(
        log_with("L", {event("L", "A", "1969-12-31T23:59:59Z", 0)}),
        log_with("R", {event("R", "X", x_timestamp, 0)}),
        {minera::CaseRelation{"L", "R"}});
  };
  const auto find_x_to_end = [](const minera::TimestampInterleavingsResult& result) {
    for (const auto& row : result.interleavings)
      if (row.source_activity == "X" && row.target_activity == "■") return row.timestamp_diff_seconds;
    assert(false && "no X -> end row");
    return 0.0;
  };
  constexpr auto kMax = std::numeric_limits<std::int64_t>::max();
  assert(find_x_to_end(run("1970-01-01T00:00:00Z")) == minera::true_divide(kMax, 1'000'000'000));
  assert(find_x_to_end(run("1970-01-01T00:00:00.000000001Z")) ==
         minera::true_divide(kMax - 1, 1'000'000'000));
  bool threw = false;
  try {
    (void)run("1969-12-31T23:59:59.999999999Z");
  } catch (const std::overflow_error&) {
    threw = true;
  }
  assert(threw);
}

int main() {
  elapsed_nanoseconds_cover_the_whole_int64_range();
  elapsed_to_end_boundary_at_int64_max();
  finds_strict_lr_and_rl_crossings();
  naive_timestamps_and_start_overflow_raise();
  equality_and_unrelated_relations_are_excluded();
  custom_timestamp_and_bound_are_strict();
  invalid_timestamps_and_json_contract();
  malformed_result_is_rejected();
  return 0;
}
