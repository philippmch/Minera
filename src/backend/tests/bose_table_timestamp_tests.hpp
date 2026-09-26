#pragma once

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "minera/concept_drift.hpp"

namespace minera_bose_table_timestamp_tests {

inline minera::EventAttribute timestamp_cell(std::string key,
                                             const std::string& value) {
  minera::EventAttribute result{std::move(key), value,
                                minera::AttributeKind::kDate};
  result.typed_value = minera::NativeDate{
      value, minera::NativeDateResolution::kNanosecond, std::string("UTC")};
  return result;
}

inline minera::EventTable timestamp_table(const std::string& standard_time,
                                          const std::string& custom_time) {
  minera::EventTable table;
  table.columns = {"case:concept:name", "concept:name", "time:timestamp",
                   "custom:timestamp"};
  table.dtypes = {"str", "str", "datetime64[ns, UTC]",
                  "datetime64[ns, UTC]"};
  const std::vector<std::string> cases = {"b", "a", "b", "a"};
  const std::vector<std::string> activities = {"B", "A", "C", "D"};
  for (std::size_t index = 0; index < cases.size(); ++index) {
    table.rows.push_back({
        minera::EventAttribute{"case:concept:name", cases[index]},
        minera::EventAttribute{"concept:name", activities[index]},
        timestamp_cell("time:timestamp", standard_time),
        timestamp_cell("custom:timestamp", custom_time)});
  }
  return table;
}

inline minera::ConceptDriftTableOptions timestamp_options() {
  minera::ConceptDriftTableOptions options;
  options.sub_log_size = 1;
  options.window_size = 0;
  options.num_permutations = 3;
  options.threshold = 1.1;
  options.max_no_change_points = 1;
  return options;
}

struct TimestampRun {
  minera::ConceptDriftTableResult result;
  std::uint32_t next_rng_word = 0;
};

inline TimestampRun run_timestamp_table(const std::string& standard_time,
                                        const std::string& custom_time,
                                        const std::string& selected_key) {
  auto options = timestamp_options();
  options.timestamp_attribute = selected_key;
  std::mt19937 rng(0);
  auto result = minera::apply_concept_drift_table(
      timestamp_table(standard_time, custom_time), options, rng);
  return {std::move(result), rng()};
}

inline void expect_epoch(const std::string& value, double expected,
                         bool negative_zero = false) {
  const auto run = run_timestamp_table(
      value, "2031-06-01T00:00:00.000000000Z", "time:timestamp");
  assert(run.result.change_timestamps.size() == 1);
  assert(run.result.change_timestamps.front() == expected);
  assert(std::signbit(run.result.change_timestamps.front()) ==
         (expected == 0.0 ? negative_zero : std::signbit(expected)));
}

}  // namespace minera_bose_table_timestamp_tests

inline void test_bose_table_timestamps() {
  using namespace minera_bose_table_timestamp_tests;

  // Public Timestamp.timestamp() results follow decimal six-place rounding of
  // the binary64 epoch value, including values at and around half-microseconds.
  expect_epoch("1970-01-01T00:00:00.000000001Z", 0.0);
  expect_epoch("1970-01-01T00:00:00.000000500Z", 0.0);
  expect_epoch("1970-01-01T00:00:00.000000999Z", 0.000001);
  expect_epoch("1970-01-01T00:00:00.000001500Z", 0.000002);
  expect_epoch("1970-01-01T00:00:00.000002500Z", 0.000003);
  expect_epoch("1970-01-01T00:00:00.000012500Z", 0.000013);

  expect_epoch("1969-12-31T23:59:59.999999999Z", -0.0, true);
  expect_epoch("1969-12-31T23:59:59.999999500Z", -0.0, true);
  expect_epoch("1969-12-31T23:59:59.999999001Z", -0.000001);
  expect_epoch("1969-12-31T23:59:59.999998500Z", -0.000002);
  expect_epoch("1969-12-31T23:59:59.999997500Z", -0.000003);
  expect_epoch("1969-12-31T23:59:59.999987500Z", -0.000013);

  // At modern epochs, submicrosecond values can be lost when the instant is
  // first represented as binary64. The selected key must control the result.
  constexpr const char* modern_submicrosecond =
      "2024-06-07T08:09:10.000000250Z";
  const auto modern = run_timestamp_table(
      modern_submicrosecond, "2031-06-01T00:00:00.000000000Z",
      "time:timestamp");
  assert(modern.result.change_timestamps ==
         std::vector<double>{1717747750.0});

  const auto custom = run_timestamp_table(
      "2032-06-01T00:00:00.000000999Z", modern_submicrosecond,
      "custom:timestamp");
  assert(custom.result.change_timestamps ==
         std::vector<double>{1717747750.0});

  // Changing only the selected instant cannot alter permutation state.
  const auto other_time = run_timestamp_table(
      "1970-01-01T00:00:00.000002500Z",
      "2031-06-01T00:00:00.000000000Z", "time:timestamp");
  const auto same_activity_seed = run_timestamp_table(
      "1970-01-01T00:00:00.000012500Z",
      "2031-06-01T00:00:00.000000000Z", "time:timestamp");
  assert(other_time.next_rng_word == same_activity_seed.next_rng_word);
  assert(other_time.result.p_values == same_activity_seed.result.p_values);
}
