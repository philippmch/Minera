#pragma once

#include <cassert>
#include <cstdint>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "minera/concept_drift.hpp"

namespace minera_bose_na_tests {

inline minera::EventAttribute na(std::string key = {}) {
  return minera::make_native_na_attribute(std::move(key));
}

inline minera::EventAttribute text(std::string value,
                                   std::string key = {}) {
  return minera::EventAttribute{std::move(key), std::move(value)};
}

inline minera::EventAttribute integer(std::int64_t value,
                                      std::string key = {}) {
  minera::EventAttribute result{std::move(key), std::to_string(value),
                                minera::AttributeKind::kInt};
  result.typed_value = value;
  return result;
}

inline minera::EventAttribute null_value() {
  minera::EventAttribute result{"", "", minera::AttributeKind::kString,
                                true};
  result.first_class_state = minera::FirstClassAttributeState::kNull;
  return result;
}

inline minera::EventAttribute date(std::string value,
                                   std::string key = {}) {
  minera::EventAttribute result{std::move(key), value,
                                minera::AttributeKind::kDate};
  result.typed_value = minera::NativeDate{std::move(value)};
  return result;
}

inline minera::EventAttribute tuple(
    std::vector<minera::EventAttribute> children,
    std::string key = {}) {
  minera::EventAttribute result{std::move(key), ""};
  result.container = minera::AttributeContainerKind::kTuple;
  result.children = std::move(children);
  return result;
}

inline std::set<std::pair<std::string, std::string>> rendered_pairs(
    const std::vector<std::pair<minera::EventAttribute,
                                minera::EventAttribute>>& pairs) {
  std::set<std::pair<std::string, std::string>> result;
  for (const auto& [source, target] : pairs)
    result.emplace(source.value, target.value);
  return result;
}

inline minera::ConceptDriftTableOptions table_options(std::int64_t size) {
  minera::ConceptDriftTableOptions options;
  options.sub_log_size = size;
  options.window_size = 1;
  options.num_permutations = 1;
  options.threshold = 1.1;
  options.max_no_change_points = 5;
  return options;
}

inline minera::EventTable valid_table_with_na_activity() {
  minera::EventTable table;
  table.columns = {"case:concept:name", "concept:name", "time:timestamp"};
  table.dtypes = {"str", "object", "datetime64[ns, UTC]"};
  table.rows = {
      {text("b", "case:concept:name"), na("concept:name"),
       date("2020-01-01T00:00:03Z", "time:timestamp")},
      {text("a", "case:concept:name"), na("concept:name"),
       date("2020-01-01T00:00:02Z", "time:timestamp")},
      {text("b", "case:concept:name"), na("concept:name"),
       date("2020-01-01T00:00:01Z", "time:timestamp")},
      {text("a", "case:concept:name"), na("concept:name"),
       date("2020-01-01T00:00:00Z", "time:timestamp")}};
  return table;
}

}  // namespace minera_bose_na_tests

inline void test_bose_na() {
  using namespace minera_bose_na_tests;

  // Native pandas.NA is a present activity value, distinct from None and
  // floating NaN. Repeated occurrences collapse to one original representative.
  const auto first_na = na("first-na");
  const auto unique = minera::extract_concept_drift_typed_activities(
      {{first_na, na("second-na"), first_na}});
  assert(unique.size() == 1);
  assert(unique.front().kind == minera::AttributeKind::kNA);
  assert(unique.front().value == "<NA>");
  assert(std::holds_alternative<minera::NativeNA>(unique.front().typed_value));
  assert(unique.front().key == "first-na");

  const auto assert_unique_rejects = [](std::vector<minera::EventAttribute> values) {
    try {
      std::vector<std::vector<minera::EventAttribute>> traces;
      traces.push_back(std::move(values));
      (void)minera::extract_concept_drift_typed_activities(traces);
    } catch (const std::invalid_argument&) {
      return;
    }
    assert(false && "mixed direct pandas.NA activity sorting must reject");
  };
  assert_unique_rejects({na(), text("A")});
  assert_unique_rejects({text("A"), na()});
  assert_unique_rejects({na(), integer(1)});
  assert_unique_rejects({integer(1), na()});

  const auto identical_tuple = tuple({na(), text("A")});
  const auto tuple_unique = minera::extract_concept_drift_typed_activities(
      {{identical_tuple, tuple({na(), text("A")})}});
  assert(tuple_unique.size() == 1);
  assert(tuple_unique.front().container ==
         minera::AttributeContainerKind::kTuple);
  assert(std::holds_alternative<minera::NativeNA>(
      tuple_unique.front().children.front().typed_value));
  assert_unique_rejects(
      {tuple({na(), text("A")}), tuple({na(), integer(1)})});

  // Explicit feature alphabets retain their order and duplicates. None is a
  // determinate non-match against NA, while direct NA/string comparison can
  // raise only when the sequence order reaches the ambiguous candidate first.
  const auto singleton_features =
      minera::extract_concept_drift_typed_features({{na()}}, {na()});
  assert((singleton_features == std::vector<double>{0, 0, 1}));
  const auto na_none_features = minera::extract_concept_drift_typed_features(
      {{na(), null_value()}}, {na(), null_value()});
  assert((na_none_features == std::vector<double>{0, 0, 2, 1, 0, 1}));
  const auto reordered_features = minera::extract_concept_drift_typed_features(
      {{na(), text("A")}}, {text("A"), na()});
  assert((reordered_features == std::vector<double>{1, 0, 1, 0, 0, 2}));

  bool rejected = false;
  try {
    (void)minera::extract_concept_drift_typed_features(
        {{na(), text("A")}}, {na(), text("A")});
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
  rejected = false;
  try {
    (void)minera::extract_concept_drift_typed_features({{na()}}, {});
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);

  const auto tuple_features = minera::extract_concept_drift_typed_features(
      {{tuple({na(), text("A")}), tuple({na(), integer(1)})}},
      {tuple({na(), text("A")}), tuple({na(), integer(1)})});
  assert(tuple_features.size() == 6);

  // Follows relations test each target in the supplied Sigma order, after it
  // has a source. A singleton NA therefore needs no equality comparison.
  assert(minera::compute_concept_drift_typed_follows_relation(
             {na()}, {text("A")})
             .empty());
  const auto follows_reordered =
      minera::compute_concept_drift_typed_follows_relation(
          {na(), text("A")}, {text("A"), na()});
  assert((rendered_pairs(follows_reordered) ==
          std::set<std::pair<std::string, std::string>>{{"<NA>", "A"}}));
  assert(std::holds_alternative<minera::NativeNA>(
      follows_reordered.front().first.typed_value));
  assert(!std::holds_alternative<minera::NativeNA>(
      follows_reordered.front().second.typed_value));

  rejected = false;
  try {
    (void)minera::compute_concept_drift_typed_follows_relation(
        {na(), text("A")}, {na(), text("A")});
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
  const auto na_none_follows =
      minera::compute_concept_drift_typed_follows_relation(
          {na(), null_value()}, {na(), null_value()});
  assert((rendered_pairs(na_none_follows) ==
          std::set<std::pair<std::string, std::string>>{{"<NA>", ""}}));
  const auto tuple_follows =
      minera::compute_concept_drift_typed_follows_relation(
          {tuple({na(), text("A")}), tuple({na(), integer(1)})},
          {tuple({na(), text("A")}), tuple({na(), integer(1)})});
  assert(tuple_follows.size() == 1);

  // Raw detection keeps homogeneous NA values and returns their full typed
  // payloads. Incompatible direct values fail during ordering before RNG use.
  const std::vector<std::vector<minera::EventAttribute>> na_traces{
      {na()}, {na()}, {na()}, {na()}};
  minera::ConceptDriftTraceOptions trace_options;
  trace_options.sub_log_size = 1;
  trace_options.window_size = 1;
  trace_options.num_permutations = 1;
  trace_options.threshold = 1.1;
  trace_options.max_no_change_points = 5;
  std::mt19937 raw_rng(0);
  const auto raw_result = minera::detect_concept_drift_typed_traces(
      na_traces, trace_options, raw_rng);
  assert(raw_result.sublogs.size() == 4);
  assert(raw_result.change_points.size() == 3);
  for (const auto& group : raw_result.sublogs) {
    assert(group.size() == 1 && group.front().size() == 1);
    assert(group.front().front().kind == minera::AttributeKind::kNA);
    assert(std::holds_alternative<minera::NativeNA>(
        group.front().front().typed_value));
  }
  assert(raw_rng() == 3626093760U);

  std::mt19937 rejected_raw_rng(19), untouched_raw_rng(19);
  rejected = false;
  try {
    (void)minera::detect_concept_drift_typed_traces(
        {{na()}, {text("A")}}, trace_options, rejected_raw_rng);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
  assert(rejected_raw_rng() == untouched_raw_rng());

  // Table activity NA is present and reaches the adapter's late output join;
  // negative slicing produces no output frame and therefore accepts it.
  auto activity_table = valid_table_with_na_activity();
  auto negative_options = table_options(-1);
  std::mt19937 negative_table_rng(0), untouched_table_rng(0);
  const auto negative_result = minera::apply_concept_drift_table(
      activity_table, negative_options, negative_table_rng);
  assert(negative_result.returned_tables.empty());
  assert(negative_result.change_timestamps.empty());
  assert(negative_result.p_values.empty());
  assert(negative_table_rng() == untouched_table_rng());

  auto positive_options = table_options(1);
  std::mt19937 positive_table_rng(0);
  rejected = false;
  try {
    (void)minera::apply_concept_drift_table(
        activity_table, positive_options, positive_table_rng);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
  // The pinned seed-0 table call consumes one MT word before its late output
  // join fails; its captured next word is 2546248239.
  assert(positive_table_rng() == 2546248239U);
}
