#pragma once

#include <cassert>
#include <cstdint>
#include <optional>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "minera/concept_drift.hpp"

namespace minera_bose_table_order_tests {

inline minera::EventAttribute date_cell(const std::string& value) {
  minera::EventAttribute result{"time:timestamp", value,
                               minera::AttributeKind::kDate};
  result.typed_value = minera::NativeDate{value};
  return result;
}

inline minera::EventAttribute selected_case_cell(const std::string& value,
                                                 bool numeric) {
  if (!numeric) return {"selected_case", value};
  minera::EventAttribute result{"selected_case", value,
                               minera::AttributeKind::kInt};
  result.typed_value = std::int64_t{std::stoll(value)};
  return result;
}

inline minera::EventTable selected_case_table(
    const std::vector<minera::EventAttribute>& selected_cases,
    const std::string& selected_dtype,
    std::optional<minera::EventTableCategoricalDtype> categorical =
        std::nullopt) {
  const std::vector<std::string> columns = {
      "case:concept:name", "concept:name", "time:timestamp",
      "extra_string_ext", "extra_string_str", "real_datetime",
      "selected_case", "selected_time"};
  minera::EventTable table;
  table.columns = columns;
  table.dtypes = {"str", "str", "datetime64[us, UTC]", "string",
                  "str", "datetime64[us, UTC]", selected_dtype,
                  "datetime64[us, UTC]"};
  if (categorical) {
    table.categorical_dtypes.assign(columns.size(), std::nullopt);
    table.categorical_dtypes[6] = std::move(categorical);
  }
  const std::vector<std::string> cases = {"s0", "s0", "s1", "s1"};
  const std::vector<std::string> activities = {"A", "B", "C", "D"};
  const std::vector<int> selected_seconds = {3, 1, 2, 0};
  for (std::size_t i = 0; i < cases.size(); ++i) {
    const auto standard_time =
        date_cell("2020-01-01T00:00:0" + std::to_string(3 - i) + "Z");
    const auto real_time =
        date_cell("2020-01-02T00:00:0" + std::to_string(3 - i) + "Z");
    const auto selected_time =
        date_cell("2020-01-03T00:00:0" + std::to_string(selected_seconds[i]) + "Z");
    table.rows.push_back({
        minera::EventAttribute{"case:concept:name", cases[i]},
        minera::EventAttribute{"concept:name", activities[i]}, standard_time,
        minera::EventAttribute{"extra_string_ext", "x"},
        minera::EventAttribute{"extra_string_str", "y"}, real_time,
        selected_cases[i], selected_time});
  }
  return table;
}

inline minera::ConceptDriftTableOptions order_options() {
  minera::ConceptDriftTableOptions options;
  options.sub_log_size = 1;
  options.window_size = 1;
  options.num_permutations = 1;
  options.threshold = 1.1;
  options.max_no_change_points = 1;
  options.case_id_attribute = "selected_case";
  options.timestamp_attribute = "selected_time";
  return options;
}

inline std::vector<std::string> output_activities(
    const minera::ConceptDriftTableResult& result) {
  std::vector<std::string> values;
  for (const auto& frame : result.returned_tables)
    for (const auto& row : frame.rows) values.push_back(row[1]->value);
  return values;
}

}  // namespace minera_bose_table_order_tests

inline void test_bose_table_ordering() {
  using namespace minera_bose_table_order_tests;

  const auto numeric_mixed = selected_case_table(
      {selected_case_cell("2", true), selected_case_cell("a", false),
       selected_case_cell("2", true), selected_case_cell("a", false)},
      "object");
  auto mixed_options = order_options();
  const auto mixed_result =
      minera::apply_concept_drift_table(numeric_mixed, mixed_options);
  assert(mixed_result.change_timestamps ==
         std::vector<double>{1578009601.0});
  assert((output_activities(mixed_result) ==
          std::vector<std::string>{"A", "B", "C", "D"}));

  // Category order controls selected-case ordering even when ordered=false;
  // the unused category must not create a group or shift the timestamp index.
  auto category_descriptor = minera::EventTableCategoricalDtype{
      "str",
      {minera::EventAttribute{"selected_case", "unused"},
       minera::EventAttribute{"selected_case", "z"},
       minera::EventAttribute{"selected_case", "a"}},
      true};
  auto category_table = selected_case_table(
      {selected_case_cell("z", false), selected_case_cell("a", false),
       selected_case_cell("z", false), selected_case_cell("a", false)},
      "category", category_descriptor);
  const auto category_result =
      minera::apply_concept_drift_table(category_table, mixed_options);
  assert(category_result.change_timestamps ==
         std::vector<double>{1578009601.0});
  auto category_cap0 = mixed_options;
  category_cap0.max_no_change_points = 0;
  const auto category_cap0_result =
      minera::apply_concept_drift_table(category_table, category_cap0);
  assert(category_cap0_result.change_timestamps.empty());
  assert((output_activities(category_cap0_result) ==
          std::vector<std::string>{"A", "B", "C", "D"}));

  // Standard case projection keeps its homogeneous scalar contract: a
  // numeric/string mixture is rejected before the detector consumes RNG.
  auto standard_mixed = numeric_mixed;
  standard_mixed.rows[1][0] = minera::EventAttribute{
      "case:concept:name", "1", minera::AttributeKind::kInt};
  standard_mixed.rows[1][0]->typed_value = std::int64_t{1};
  std::mt19937 standard_rng(0);
  bool standard_rejected = false;
  try {
    (void)minera::apply_concept_drift_table(standard_mixed, mixed_options,
                                           standard_rng);
  } catch (const std::invalid_argument&) {
    standard_rejected = true;
  }
  assert(standard_rejected);
  assert(standard_rng() == 2'357'136'044u);

  // Standard datetime64[ns] case keys that differ below one microsecond must
  // remain distinct in both ascending and descending input order.
  const std::vector<std::string> ns_values = {
      "2020-01-04T00:00:00.000000000Z",
      "2020-01-04T00:00:00.000000001Z",
      "2020-01-04T00:00:00.000000002Z",
      "2020-01-04T00:00:00.000001000Z"};
  auto make_ns_table = [&](const std::vector<std::size_t>& order) {
    minera::EventTable table;
    table.columns = {"case:concept:name", "concept:name",
                     "time:timestamp", "extra_string_ext",
                     "extra_string_str"};
    table.dtypes = {"datetime64[ns, UTC]", "str", "datetime64[us, UTC]",
                    "string", "str"};
    for (std::size_t row_index = 0; row_index < order.size(); ++row_index) {
      const auto index = order[row_index];
      auto key = date_cell(ns_values[index]);
      key.key = "case:concept:name";
      key.typed_value = minera::NativeDate{
          ns_values[index], minera::NativeDateResolution::kNanosecond,
          std::string("UTC")};
      table.rows.push_back({
          key, minera::EventAttribute{
                   "concept:name",
                   std::string(1, static_cast<char>('A' + row_index))},
          date_cell("2020-01-01T00:00:0" +
                    std::to_string(3 - row_index) + "Z"),
          minera::EventAttribute{"extra_string_ext", "x"},
          minera::EventAttribute{"extra_string_str", "y"}});
    }
    return table;
  };
  auto ns_options = mixed_options;
  ns_options.case_id_attribute = "case:concept:name";
  ns_options.timestamp_attribute = "time:timestamp";
  const auto ascending = minera::apply_concept_drift_table(
      make_ns_table({0, 1, 2, 3}), ns_options);
  assert(ascending.change_timestamps ==
         std::vector<double>{1577836802.0});
  assert((output_activities(ascending) ==
          std::vector<std::string>{"A", "B", "C", "D"}));
  const auto descending = minera::apply_concept_drift_table(
      make_ns_table({3, 2, 1, 0}), ns_options);
  assert(descending.change_timestamps ==
         std::vector<double>{1577836801.0});
  assert((output_activities(descending) ==
          std::vector<std::string>{"D", "C", "B", "A"}));
}
