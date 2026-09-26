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

namespace minera_bose_typed_direct_tests {

inline minera::EventAttribute integer(std::int64_t value) {
  minera::EventAttribute result{"", std::to_string(value),
                                minera::AttributeKind::kInt};
  result.typed_value = value;
  return result;
}

inline minera::EventAttribute boolean(bool value) {
  minera::EventAttribute result{"", value ? "True" : "False",
                                minera::AttributeKind::kBoolean};
  result.typed_value = value;
  return result;
}

inline minera::EventAttribute text(std::string value) {
  return minera::EventAttribute{"", std::move(value)};
}

inline minera::EventAttribute tuple(
    std::vector<minera::EventAttribute> children) {
  minera::EventAttribute result{"", ""};
  result.container = minera::AttributeContainerKind::kTuple;
  result.children = std::move(children);
  return result;
}

inline minera::EventAttribute nan_value() {
  minera::EventAttribute result{"", "nan", minera::AttributeKind::kFloat};
  result.typed_value = std::numeric_limits<double>::quiet_NaN();
  return result;
}

inline void assert_attribute_copy(const minera::EventAttribute& source,
                                  const minera::EventAttribute& copy) {
  assert(source == copy);
  assert(source.typed_value.scalar_identity() ==
         copy.typed_value.scalar_identity());
  assert(source.children.size() == copy.children.size());
  for (std::size_t index = 0; index < source.children.size(); ++index)
    assert_attribute_copy(source.children[index], copy.children[index]);
}

inline void assert_trace_copy(
    const std::vector<minera::EventAttribute>& source,
    const std::vector<minera::EventAttribute>& copy) {
  assert(source.size() == copy.size());
  for (std::size_t index = 0; index < source.size(); ++index)
    assert_attribute_copy(source[index], copy[index]);
}

inline minera::ConceptDriftTraceOptions options(std::int64_t size = 1) {
  minera::ConceptDriftTraceOptions result;
  result.sub_log_size = size;
  result.window_size = 1;
  result.num_permutations = 1;
  result.threshold = 1.1;
  result.max_no_change_points = 8;
  result.seed = 42;
  return result;
}

inline void assert_same_result(
    const minera::ConceptDriftTypedTraceResult& left,
    const minera::ConceptDriftTypedTraceResult& right) {
  assert(left.change_points.size() == right.change_points.size());
  for (std::size_t index = 0; index < left.change_points.size(); ++index) {
    assert(left.change_points[index].sublog_index ==
           right.change_points[index].sublog_index);
    assert(left.change_points[index].p_value ==
           right.change_points[index].p_value);
  }
  assert(left.sublogs.size() == right.sublogs.size());
  for (std::size_t group = 0; group < left.sublogs.size(); ++group) {
    assert(left.sublogs[group].size() == right.sublogs[group].size());
    for (std::size_t trace = 0; trace < left.sublogs[group].size(); ++trace)
      assert_trace_copy(left.sublogs[group][trace],
                        right.sublogs[group][trace]);
  }
}

}  // namespace minera_bose_typed_direct_tests

inline void test_bose_typed_direct() {
  using namespace minera_bose_typed_direct_tests;

  // Python numeric aliases share one encoded label; result sublogs retain the
  // original bool/int values and their scalar identities.
  const std::vector<std::vector<minera::EventAttribute>> numeric_traces{
      {integer(1)}, {boolean(true)}, {integer(2)}, {boolean(false)}, {}};
  auto numeric_options = options(2);
  std::mt19937 state_rng(static_cast<std::uint32_t>(numeric_options.seed));
  std::mt19937 replay_rng(static_cast<std::uint32_t>(numeric_options.seed));
  const auto state_result = minera::detect_concept_drift_typed_traces(
      numeric_traces, numeric_options, state_rng);
  const auto seeded_result = minera::detect_concept_drift_typed_traces(
      numeric_traces, numeric_options);
  const auto replay_result = minera::detect_concept_drift_typed_traces(
      numeric_traces, numeric_options, replay_rng);
  assert_same_result(state_result, seeded_result);
  assert_same_result(state_result, replay_result);
  assert(state_result.sublogs.size() == 3);
  assert(state_result.sublogs[0].size() == 2);
  assert(state_result.sublogs[1].size() == 2);
  assert(state_result.sublogs[2].size() == 1);
  std::size_t trace_index = 0;
  for (const auto& group : state_result.sublogs)
    for (const auto& trace : group)
      assert_trace_copy(numeric_traces[trace_index++], trace);

  // Explicitly compare the typed detector's encoding and random continuation
  // with the existing direct string detector.
  const std::vector<std::vector<std::string>> numeric_surrogates{
      {"1000000001"}, {"1000000001"}, {"1000000002"},
      {"1000000000"}, {}};
  auto string_rng = std::mt19937(static_cast<std::uint32_t>(numeric_options.seed));
  const auto string_result = minera::detect_concept_drift_traces(
      numeric_surrogates, numeric_options, string_rng);
  auto typed_continuation = state_rng;
  assert(string_result.change_points.size() == state_result.change_points.size());
  for (std::size_t index = 0; index < string_result.change_points.size(); ++index) {
    assert(string_result.change_points[index].sublog_index ==
           state_result.change_points[index].sublog_index);
    assert(string_result.change_points[index].p_value ==
           state_result.change_points[index].p_value);
  }
  assert(typed_continuation() == string_rng());
  assert(state_rng() == replay_rng());

  // Nested immutable tuples are accepted and copied recursively.
  const auto nested_left = tuple({text("prefix"), tuple({integer(1), text("x")})});
  const auto nested_right = tuple({text("prefix"), tuple({integer(2), text("y")})});
  const std::vector<std::vector<minera::EventAttribute>> tuple_traces{
      {nested_left}, {nested_right}, {nested_left}, {nested_right}};
  const auto tuple_result = minera::detect_concept_drift_typed_traces(
      tuple_traces, options());
  assert(tuple_result.sublogs.size() == tuple_traces.size());
  for (std::size_t index = 0; index < tuple_traces.size(); ++index)
    assert_trace_copy(tuple_traces[index], tuple_result.sublogs[index][0]);

  // Null labels remain values in the original raw sublogs.
  auto null_value = text("");
  null_value.is_null = true;
  null_value.first_class_state = minera::FirstClassAttributeState::kNull;
  const std::vector<std::vector<minera::EventAttribute>> null_traces{
      {null_value}, {}, {null_value}, {}};
  const auto null_result = minera::detect_concept_drift_typed_traces(
      null_traces, options());
  assert(null_result.sublogs.size() == null_traces.size());
  for (std::size_t index = 0; index < null_traces.size(); ++index)
    assert_trace_copy(null_traces[index], null_result.sublogs[index][0]);

  // Shared NaN tuple members continue comparison into their tails and reject
  // incompatible values before advancing a caller-owned engine. Distinct NaN
  // identities provide the ordering tie-break and preserve their source copy.
  const auto shared_nan = nan_value();
  const auto same_nan_text = tuple({shared_nan, text("tail")});
  const auto same_nan_int = tuple({shared_nan, integer(1)});
  const std::vector<std::vector<minera::EventAttribute>> shared_nan_traces{
      {same_nan_text}, {same_nan_int}, {same_nan_text}, {same_nan_int}};
  std::mt19937 rejected_rng(7), untouched_rng(7);
  bool rejected = false;
  try {
    (void)minera::detect_concept_drift_typed_traces(
        shared_nan_traces, options(), rejected_rng);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
  assert(rejected_rng() == untouched_rng());

  const auto first_nan = nan_value();
  const auto second_nan = nan_value();
  const auto distinct_nan_text = tuple({first_nan, text("tail")});
  const auto distinct_nan_int = tuple({second_nan, integer(1)});
  const std::vector<std::vector<minera::EventAttribute>> distinct_nan_traces{
      {distinct_nan_text}, {distinct_nan_int}, {distinct_nan_text},
      {distinct_nan_int}};
  const auto distinct_nan_result = minera::detect_concept_drift_typed_traces(
      distinct_nan_traces, options());
  assert(distinct_nan_result.sublogs.size() == distinct_nan_traces.size());
  for (std::size_t index = 0; index < distinct_nan_traces.size(); ++index)
    assert_trace_copy(distinct_nan_traces[index],
                      distinct_nan_result.sublogs[index][0]);

  // Incompatible scalar families and zero signed size reject without RNG
  // draws; negative sizes retain the existing empty-slice behavior.
  const std::vector<std::vector<minera::EventAttribute>> mixed_traces{
      {integer(1)}, {text("1")}, {integer(1)}, {text("1")}};
  std::mt19937 mixed_rng(9), mixed_untouched(9);
  rejected = false;
  try {
    (void)minera::detect_concept_drift_typed_traces(
        mixed_traces, options(), mixed_rng);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
  assert(mixed_rng() == mixed_untouched());

  auto zero_options = options(0);
  std::mt19937 zero_rng(11), zero_untouched(11);
  rejected = false;
  try {
    (void)minera::detect_concept_drift_typed_traces(
        numeric_traces, zero_options, zero_rng);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
  assert(zero_rng() == zero_untouched());

  auto negative_options = options(-1);
  std::mt19937 negative_rng(13), negative_untouched(13);
  const auto negative_result = minera::detect_concept_drift_typed_traces(
      numeric_traces, negative_options, negative_rng);
  assert(negative_result.sublogs.empty());
  assert(negative_result.change_points.empty());
  assert(negative_rng() == negative_untouched());
}
