#pragma once

#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "minera/concept_drift.hpp"

namespace minera_bose_table_activity_tests {

inline minera::EventAttribute date_cell(const std::string& value) {
  minera::EventAttribute result{"time:timestamp", value,
                               minera::AttributeKind::kDate};
  result.typed_value = minera::NativeDate{value};
  return result;
}

inline minera::EventTable table_with_activity(
    const std::vector<minera::EventAttribute>& activities) {
  assert(activities.size() == 4);
  minera::EventTable table;
  table.columns = {"case:concept:name", "concept:name", "time:timestamp",
                   "extra_string_ext", "extra_string_str", "real_datetime"};
  table.dtypes = {"str", "object", "datetime64[us, UTC]", "string", "str",
                  "datetime64[us, UTC]"};
  const std::vector<std::string> cases = {"s0", "s0", "s1", "s1"};
  for (std::size_t index = 0; index < activities.size(); ++index) {
    auto activity = activities[index];
    activity.key = "concept:name";
    table.rows.push_back({
        minera::EventAttribute{"case:concept:name", cases[index]}, activity,
        date_cell("2020-01-01T00:00:0" + std::to_string(3 - index) + "Z"),
        minera::EventAttribute{"extra_string_ext", "x"},
        minera::EventAttribute{"extra_string_str", "y"},
        date_cell("2020-01-02T00:00:0" + std::to_string(3 - index) + "Z")});
  }
  return table;
}

inline minera::EventAttribute bytes_activity(const std::string& bytes) {
  auto result = minera::make_bytes_attribute("concept:name", bytes);
  return result;
}

inline minera::EventAttribute tuple_activity(const std::string& value) {
  minera::EventAttribute result{"concept:name", ""};
  result.container = minera::AttributeContainerKind::kTuple;
  result.children.push_back(minera::EventAttribute{"", value});
  return result;
}

inline minera::EventAttribute tuple_activity_children(
    std::vector<minera::EventAttribute> children) {
  minera::EventAttribute result{"concept:name", ""};
  result.container = minera::AttributeContainerKind::kTuple;
  result.children = std::move(children);
  return result;
}

inline minera::ConceptDriftTableOptions activity_options(
    std::int64_t size, std::int64_t window = 1) {
  minera::ConceptDriftTableOptions options;
  options.sub_log_size = size;
  options.window_size = window;
  options.num_permutations = 1;
  options.threshold = 1.1;
  options.max_no_change_points = 0;
  return options;
}

inline void expect_late_activity_failure(const minera::EventTable& table,
                                         minera::ConceptDriftTableOptions options,
                                         std::uint32_t next_word) {
  std::mt19937 rng(0);
  bool rejected = false;
  try {
    (void)minera::apply_concept_drift_table(table, options, rng);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
  assert(rng() == next_word);
}

}  // namespace minera_bose_table_activity_tests

inline void test_bose_table_activity_kinds() {
  using namespace minera_bose_table_activity_tests;

  // Immutable non-string activity keys are encoded for the detector and fail
  // only when a positive-size result would need string output.
  expect_late_activity_failure(
      table_with_activity({bytes_activity("A"), bytes_activity("B"),
                           bytes_activity("A"), bytes_activity("B")}),
      activity_options(1), 2'546'248'239u);
  expect_late_activity_failure(
      table_with_activity({tuple_activity("A"), tuple_activity("B"),
                           tuple_activity("A"), tuple_activity("B")}),
      activity_options(1, 0), 3'626'093'760u);

  minera::EventAttribute null_activity{"concept:name", "",
                                       minera::AttributeKind::kString, true};
  const auto null_table = table_with_activity(
      {null_activity, null_activity, null_activity, null_activity});
  expect_late_activity_failure(null_table, activity_options(1),
                               2'546'248'239u);

  minera::EventAttribute nan_activity{"concept:name", "nan",
                                      minera::AttributeKind::kFloat};
  nan_activity.typed_value = std::numeric_limits<double>::quiet_NaN();
  expect_late_activity_failure(
      table_with_activity({nan_activity, nan_activity, nan_activity,
                           nan_activity}),
      activity_options(1), 2'546'248'239u);

  // Negative sub-log sizes return no materialized frames and do not consume
  // the caller-owned RNG for homogeneous non-string activity values.
  std::mt19937 negative_rng(0);
  const auto negative_result = minera::apply_concept_drift_table(
      table_with_activity({bytes_activity("A"), bytes_activity("B"),
                           bytes_activity("A"), bytes_activity("B")}),
      activity_options(-1), negative_rng);
  assert(negative_result.returned_tables.empty());
  assert(negative_result.change_timestamps.empty());
  assert(negative_rng() == 2'357'136'044u);

  // Incompatible activity families still fail before detector work; null is
  // not silently discarded when it is mixed with ordinary strings.
  minera::EventAttribute string_activity{"concept:name", "A"};
  expect_late_activity_failure(
      table_with_activity({string_activity, null_activity, string_activity,
                           string_activity}),
      activity_options(-1), 2'357'136'044u);

  // A copied NaN preserves its scalar identity. Equal aliases therefore let
  // tuple comparison continue into the incompatible tail and reject before
  // detector work, while distinct NaNs provide a strict ordering tie-break
  // and skip that tail, matching Python's tuple comparison behavior.
  minera::EventAttribute shared_nan{"", "nan", minera::AttributeKind::kFloat};
  shared_nan.typed_value = std::numeric_limits<double>::quiet_NaN();
  minera::EventAttribute string_tail{"", "A"};
  minera::EventAttribute integer_tail{"", "1", minera::AttributeKind::kInt};
  integer_tail.typed_value = std::int64_t{1};
  const auto same_nan_left = tuple_activity_children({shared_nan, string_tail});
  const auto same_nan_right = tuple_activity_children({shared_nan, integer_tail});
  expect_late_activity_failure(
      table_with_activity({same_nan_left, same_nan_right, same_nan_left,
                           same_nan_right}),
      activity_options(-1), 2'357'136'044u);

  minera::EventAttribute first_nan{"", "nan", minera::AttributeKind::kFloat};
  first_nan.typed_value = std::numeric_limits<double>::quiet_NaN();
  minera::EventAttribute second_nan{"", "nan", minera::AttributeKind::kFloat};
  second_nan.typed_value = std::numeric_limits<double>::quiet_NaN();
  const auto distinct_nan_left =
      tuple_activity_children({first_nan, string_tail});
  const auto distinct_nan_right =
      tuple_activity_children({second_nan, integer_tail});
  expect_late_activity_failure(
      table_with_activity({distinct_nan_left, distinct_nan_right,
                           distinct_nan_left, distinct_nan_right}),
      activity_options(1), 2'546'248'239u);

  std::mt19937 distinct_negative_rng(0);
  const auto distinct_negative = minera::apply_concept_drift_table(
      table_with_activity({distinct_nan_left, distinct_nan_right,
                           distinct_nan_left, distinct_nan_right}),
      activity_options(-1), distinct_negative_rng);
  assert(distinct_negative.returned_tables.empty());
  assert(distinct_negative.change_timestamps.empty());
  assert(distinct_negative_rng() == 2'357'136'044u);
}
