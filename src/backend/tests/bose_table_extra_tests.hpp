#pragma once

#include <cassert>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "minera/concept_drift.hpp"
#include "../concept_drift_table_limits.hpp"

namespace minera_bose_table_extra_tests {

inline minera::EventAttribute timestamp_cell(const std::string& value) {
  minera::EventAttribute result{"time:timestamp", value,
                               minera::AttributeKind::kDate};
  result.typed_value = minera::NativeDate{value};
  return result;
}

inline minera::EventTable selector_table(
    const std::vector<std::string>& cases,
    const std::vector<std::string>& standard_activities,
    const std::vector<std::string>& selected_activities,
    bool null_timestamps = false,
    const std::vector<std::string>& selected_cases = {}) {
  assert(cases.size() == standard_activities.size());
  assert(cases.size() == selected_activities.size());
  if (!selected_cases.empty()) assert(selected_cases.size() == cases.size());
  minera::EventTable table;
  table.columns = {"case:concept:name", "concept:name", "time:timestamp",
                   "selected_case", "activity"};
  table.dtypes = {"str", "str", "datetime64[us, UTC]", "str", "str"};
  for (std::size_t i = 0; i < cases.size(); ++i) {
    auto timestamp = timestamp_cell("2024-01-01T00:00:0" +
                                    std::to_string(i) + "Z");
    if (null_timestamps) {
      timestamp.is_null = true;
      timestamp.first_class_state = minera::FirstClassAttributeState::kNull;
    }
    table.rows.push_back({
        minera::EventAttribute{"case:concept:name", cases[i]},
        minera::EventAttribute{"concept:name", standard_activities[i]},
        timestamp,
        minera::EventAttribute{"selected_case",
                               selected_cases.empty() ? cases[i]
                                                      : selected_cases[i]},
        minera::EventAttribute{"activity", selected_activities[i]}});
  }
  return table;
}

inline minera::ConceptDriftTableOptions edge_options() {
  minera::ConceptDriftTableOptions options;
  options.sub_log_size = 1;
  options.window_size = 1;
  options.num_permutations = 1;
  options.threshold = 1.1;
  return options;
}

}  // namespace minera_bose_table_extra_tests

// Additional table-adapter checks kept outside the large legacy test source so
// the parent test harness can include and call this focused group directly.
inline void test_bose_table_edges() {
  using namespace minera_bose_table_extra_tests;

  // Exercise each grouping path at a small injected internal limit. This
  // checks early rejection without allocating millions of table cells.
  auto bounded_options = edge_options();
  bounded_options.case_id_attribute = "selected_case";
  const auto selected_overflow = selector_table(
      {"same", "same", "same"}, {"A", "B", "C"}, {"A", "B", "C"},
      false, {"a", "b", "c"});
  const auto standard_overflow = selector_table(
      {"a", "b", "c"}, {"A", "B", "C"}, {"A", "B", "C"},
      false, {"same", "same", "same"});
  for (const auto* table : {&selected_overflow, &standard_overflow}) {
    std::mt19937 rng(0);
    bool rejected = false;
    try {
      (void)minera::detail::apply_concept_drift_table_bounded(
          *table, bounded_options, rng, 2);
    } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
    assert(rng() == 2'357'136'044u);
  }
  const auto exact_limit = selector_table({"a", "b"}, {"A", "B"}, {"A", "B"});
  std::mt19937 exact_limit_rng(0);
  const auto bounded_result = minera::detail::apply_concept_drift_table_bounded(
      exact_limit, bounded_options, exact_limit_rng, 2);
  assert(bounded_result.returned_tables.size() == 2);
  assert(exact_limit_rng() == 2'546'248'239u);

  const auto base = selector_table(
      {"b", "a", "b", "a"}, {"std-b0", "std-a0", "std-b1", "std-a1"},
      {"custom-b0", "custom-a0", "custom-b1", "custom-a1"});

  // A selected activity column controls the projected labels.  The standard
  // concept:name values are deliberately different so a mistaken selector is
  // visible in the regenerated frames.
  auto custom_activity_options = edge_options();
  custom_activity_options.activity_attribute = "activity";
  const auto custom_activity =
      minera::apply_concept_drift_table(base, custom_activity_options);
  bool saw_custom_label = false;
  bool saw_standard_label = false;
  for (const auto& frame : custom_activity.returned_tables) {
    for (const auto& row : frame.rows) {
      if (row[1]->value.rfind("custom-", 0) == 0) saw_custom_label = true;
      if (row[1]->value.rfind("std-", 0) == 0) saw_standard_label = true;
    }
  }
  assert(saw_custom_label);
  assert(!saw_standard_label);

  // Selected case/timestamp grouping is independent of standard-case
  // projection.  With one selected case, the positive boundary produced by
  // this four-trace input has no timestamp index.  The detector has already
  // consumed its shuffle before that lookup is rejected.
  auto selected_case_options = edge_options();
  selected_case_options.case_id_attribute = "selected_case";
  const auto one_selected_case = selector_table(
      {"b", "a", "b", "a"}, {"B", "A", "C", "D"},
      {"B", "A", "C", "D"}, false, {"one", "one", "one", "one"});
  std::mt19937 rejected_rng(0);
  bool selected_case_rejected = false;
  try {
    (void)minera::apply_concept_drift_table(one_selected_case,
                                           selected_case_options, rejected_rng);
  } catch (const std::invalid_argument&) {
    selected_case_rejected = true;
  }
  assert(selected_case_rejected);
  assert(rejected_rng() == 2'546'248'239u);

  // A selected case group with no non-null timestamp cannot supply a change
  // timestamp, even though the unselected schema validator allows null cells.
  const auto null_timestamps = selector_table(
      {"b", "a", "b", "a"}, {"B", "A", "C", "D"},
      {"B", "A", "C", "D"}, true);
  bool null_timestamp_rejected = false;
  try {
    (void)minera::apply_concept_drift_table(null_timestamps, edge_options());
  } catch (const std::invalid_argument&) {
    null_timestamp_rejected = true;
  }
  assert(null_timestamp_rejected);

  // Empty tables with a complete, typed schema are valid and produce no
  // returned frames or change points.
  minera::EventTable empty;
  empty.columns = {"case:concept:name", "concept:name", "time:timestamp"};
  empty.dtypes = {"str", "str", "datetime64[us, UTC]"};
  const auto empty_result =
      minera::apply_concept_drift_table(empty, edge_options());
  assert(empty_result.returned_tables.empty());
  assert(empty_result.change_timestamps.empty());
  assert(empty_result.p_values.empty());

  // Native scalar case ordering is numeric even when an extra string column
  // supplies the second string dtype required by dataframe validation.
  minera::EventTable numeric_cases;
  numeric_cases.columns = {"case:concept:name", "concept:name",
                           "time:timestamp", "extra"};
  numeric_cases.dtypes = {"int64", "str", "datetime64[us, UTC]", "str"};
  numeric_cases.rows.push_back({
      minera::EventAttribute{"case:concept:name", "10", minera::AttributeKind::kInt},
      minera::EventAttribute{"concept:name", "TEN"},
      timestamp_cell("2024-01-01T00:00:00Z"),
      minera::EventAttribute{"extra", "unused"}});
  numeric_cases.rows.push_back({
      minera::EventAttribute{"case:concept:name", "2", minera::AttributeKind::kInt},
      minera::EventAttribute{"concept:name", "TWO"},
      timestamp_cell("2024-01-01T00:00:01Z"),
      minera::EventAttribute{"extra", "unused"}});
  // Typed integer identity is what controls native ordering; the textual
  // projection above remains the source-compatible value field.
  numeric_cases.rows[0][0]->typed_value = std::int64_t{10};
  numeric_cases.rows[1][0]->typed_value = std::int64_t{2};
  const auto numeric_result =
      minera::apply_concept_drift_table(numeric_cases, edge_options());
  assert(numeric_result.returned_tables.size() == 2);
  assert(numeric_result.returned_tables[0].rows.size() == 1);
  assert(numeric_result.returned_tables[1].rows.size() == 1);
  assert(numeric_result.returned_tables[0].rows[0][1]->value == "TWO");
  assert(numeric_result.returned_tables[1].rows[0][1]->value == "TEN");
}
