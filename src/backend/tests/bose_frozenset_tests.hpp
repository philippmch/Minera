#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "minera/concept_drift.hpp"

namespace minera_bose_frozenset_tests {

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

inline minera::EventAttribute real(double value, std::string key = {}) {
  minera::EventAttribute result{std::move(key), "stale",
                                minera::AttributeKind::kFloat};
  result.typed_value = value;
  return result;
}

inline minera::EventAttribute boolean(bool value, std::string key = {}) {
  minera::EventAttribute result{std::move(key), value ? "true" : "false",
                                minera::AttributeKind::kBoolean};
  result.typed_value = value;
  return result;
}

inline minera::EventAttribute frozen(
    std::vector<minera::EventAttribute> children,
    std::string key = {}) {
  minera::EventAttribute result{std::move(key), ""};
  result.container = minera::AttributeContainerKind::kFrozenSet;
  result.children = std::move(children);
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

inline bool rejected(std::vector<minera::EventAttribute> activities) {
  try {
    (void)minera::extract_concept_drift_typed_activities({
        std::move(activities)});
  } catch (const std::invalid_argument&) {
    return true;
  }
  return false;
}

}  // namespace minera_bose_frozenset_tests

inline void test_bose_frozenset() {
  using namespace minera_bose_frozenset_tests;
  using minera::AttributeContainerKind;
  using minera::AttributeKind;
  using minera::EventAttribute;
  using minera::NativeDate;
  using minera::NativeDateResolution;

  // Frozenset identity is independent of stored child order and uses the
  // detector's scalar IDs, so numeric aliases deduplicate without rewriting
  // the first representative returned by the unique helper.
  const auto first = frozen({integer(1, "first-int"), text("phase", "first-text")},
                            "first-frozenset");
  const auto equivalent = frozen(
      {text("phase", "later-text"), real(1.0, "float-alias"),
       boolean(true, "bool-alias")},
      "later-frozenset");
  auto unique = minera::extract_concept_drift_typed_activities(
      {{first, equivalent}});
  assert(unique.size() == 1);
  assert(unique.front() == first);
  assert(unique.front().children.size() == 2);
  assert(unique.front().children[0].key == "first-int");
  assert(unique.front().children[1].key == "first-text");

  const auto first_na_set = frozen(
      {minera::make_native_na_attribute("first-na"), text("phase")});
  const auto reordered_na_set = frozen(
      {text("phase", "phase-copy"),
       minera::make_native_na_attribute("later-na")});
  unique = minera::extract_concept_drift_typed_activities(
      {{first_na_set, reordered_na_set}});
  assert(unique.size() == 1);
  assert(unique.front() == first_na_set);
  assert(std::holds_alternative<minera::NativeNA>(
      unique.front().children.front().typed_value));

  const auto empty = frozen({});
  const auto set_a = frozen({text("A")});
  const auto set_b = frozen({text("A"), text("B")});
  // Exercise every arrival order through the public unique helper. The native
  // strict subset extension sorts by unique member count before member IDs.
  const std::vector<EventAttribute> chain{empty, set_a, set_b};
  std::vector<std::size_t> chain_order{0, 1, 2};
  do {
    const auto values = minera::extract_concept_drift_typed_activities({
        {chain[chain_order[0]], chain[chain_order[1]], chain[chain_order[2]]}});
    assert(values.size() == 3);
    assert(values[0].children.empty());
    assert(values[1].children.size() == 1);
    assert(values[2].children.size() == 2);
  } while (std::next_permutation(chain_order.begin(), chain_order.end()));

  // Incomparable equal-size sets use canonical-ID lexical tie-breaking. This
  // deterministic native extension may differ from upstream hash iteration.
  const auto set_c = frozen({text("C")});
  const std::vector<EventAttribute> incomparable{
      set_c, set_a, frozen({text("B")})};
  std::vector<std::size_t> incomparable_order{0, 1, 2};
  do {
    const auto ordered_incomparable =
        minera::extract_concept_drift_typed_activities({
            {incomparable[incomparable_order[0]],
             incomparable[incomparable_order[1]],
             incomparable[incomparable_order[2]]}});
    assert(ordered_incomparable.size() == 3);
    assert(ordered_incomparable[0].children[0].value == "A");
    assert(ordered_incomparable[1].children[0].value == "B");
    assert(ordered_incomparable[2].children[0].value == "C");
  } while (std::next_permutation(incomparable_order.begin(),
                                 incomparable_order.end()));

  // Tuples compare frozen-set prefixes by the same safe extension. Equal
  // prefixes advance to their tails; unequal prefixes decide the order.
  const auto prefix_a = frozen({text("A")});
  const auto prefix_ab = frozen({text("A"), text("B")});
  const auto tuple_values = minera::extract_concept_drift_typed_activities(
      {{tuple({prefix_a, text("z")}), tuple({prefix_a, text("a")})}});
  assert(tuple_values.size() == 2);
  assert(tuple_values[0].children[1].value == "a");
  assert(tuple_values[1].children[1].value == "z");
  const auto tuple_prefix_values =
      minera::extract_concept_drift_typed_activities(
          {{tuple({prefix_ab, text("a")}), tuple({prefix_a, text("z")})}});
  assert(tuple_prefix_values.size() == 2);
  assert(tuple_prefix_values[0].children[0].children.size() == 1);
  assert(tuple_prefix_values[0].children[1].value == "z");
  assert(tuple_prefix_values[1].children[0].children.size() == 2);
  assert(tuple_prefix_values[1].children[1].value == "a");

  // Cached ordering keys also cover frozen sets nested inside tuple prefixes.
  const auto nested_tuple_values =
      minera::extract_concept_drift_typed_activities(
          {{tuple({tuple({prefix_ab, text("inner")}), text("outer-a")}),
            tuple({tuple({prefix_a, text("inner")}), text("outer-z")})}});
  assert(nested_tuple_values.size() == 2);
  assert(nested_tuple_values[0].children[0].children[0].children.size() == 1);
  assert(nested_tuple_values[0].children[1].value == "outer-z");
  assert(nested_tuple_values[1].children[0].children[0].children.size() == 2);
  assert(nested_tuple_values[1].children[1].value == "outer-a");

  // Direct relation membership uses set equality, not subset ordering. The
  // first relation retains the raw reordered target representation.
  const auto source = frozen({text("source")}, "source");
  const auto target = frozen({text("A"), text("B")}, "target");
  const auto target_reordered = frozen({text("B"), text("A")}, "target-copy");
  const auto same_set_relation =
      minera::compute_concept_drift_typed_follows_relation(
          {source, target_reordered}, {target});
  assert(same_set_relation.size() == 1);
  assert(same_set_relation.front().first == source);
  assert(same_set_relation.front().second == target_reordered);
  const auto disjoint = frozen({text("elsewhere")});
  assert(minera::compute_concept_drift_typed_follows_relation(
             {source, disjoint}, {source})
             .empty());
  assert(minera::compute_concept_drift_typed_follows_relation(
             {set_a, set_b}, {set_a})
             .empty());
  const auto empty_relation =
      minera::compute_concept_drift_typed_follows_relation(
          {empty, frozen({})}, {empty});
  assert(empty_relation.size() == 1);

  const auto tuple_target = tuple({target_reordered, text("tail")});
  const auto tuple_target_canonical = tuple({target, text("tail")});
  const auto nested_relation =
      minera::compute_concept_drift_typed_follows_relation(
          {tuple({source, text("head")}), tuple_target},
          {tuple_target_canonical});
  assert(nested_relation.size() == 1);
  assert(nested_relation.front().second == tuple_target);

  const auto marker = minera::make_native_na_attribute("marker");
  const auto set_with_marker = frozen({text("A")});
  const auto marker_first_relation =
      minera::compute_concept_drift_typed_follows_relation(
          {source, set_with_marker}, {marker, set_with_marker});
  const auto marker_last_relation =
      minera::compute_concept_drift_typed_follows_relation(
          {source, marker}, {set_with_marker, marker});
  assert(marker_first_relation.size() == 1);
  assert(marker_last_relation.size() == 1);
  assert(marker_first_relation.front().second == set_with_marker);
  assert(marker_last_relation.front().second == marker);

  const auto reordered_features =
      minera::extract_concept_drift_typed_features(
          {{source, target_reordered}}, {source, target});
  assert((reordered_features == std::vector<double>{0, 0, 2, 1, 0, 1}));
  const auto subset_features = minera::extract_concept_drift_typed_features(
      {{set_a, set_b}}, {set_a, set_b});
  assert((subset_features == std::vector<double>{0, 0, 2, 1, 0, 1}));
  const auto marker_features_forward =
      minera::extract_concept_drift_typed_features(
          {{marker, set_with_marker}}, {marker, set_with_marker});
  const auto marker_features_reverse =
      minera::extract_concept_drift_typed_features(
          {{marker, set_with_marker}}, {set_with_marker, marker});
  assert(marker_features_forward.size() == 6);
  assert(marker_features_reverse.size() == 6);

  // NaNs retain scalar identity and dates retain calendar-vs-datetime and
  // nanosecond distinctions inside canonical unordered member IDs.
  auto nan = real(std::nan(""));
  auto same_nan = nan;
  auto other_nan = real(std::nan(""));
  const auto nan_set = frozen({nan, same_nan});
  const auto other_nan_set = frozen({other_nan});
  const auto nan_unique = minera::extract_concept_drift_typed_activities(
      {{nan_set, other_nan_set}});
  assert(nan_unique.size() == 2);
  assert(std::any_of(nan_unique.begin(), nan_unique.end(), [](const auto& value) {
    return value.children.size() == 2;
  }));

  EventAttribute calendar{"", "2024-01-01", AttributeKind::kDate};
  calendar.typed_value = NativeDate{"2024-01-01"};
  EventAttribute midnight{"", "2024-01-01T00:00:00Z", AttributeKind::kDate};
  midnight.typed_value = NativeDate{"2024-01-01T00:00:00Z",
                                    NativeDateResolution::kNanosecond};
  EventAttribute first_ns{"", "2024-01-01T00:00:00.000000001Z",
                          AttributeKind::kDate};
  first_ns.typed_value = NativeDate{"2024-01-01T00:00:00.000000001Z",
                                    NativeDateResolution::kNanosecond};
  EventAttribute second_ns{"", "2024-01-01T00:00:00.000000002Z",
                           AttributeKind::kDate};
  second_ns.typed_value = NativeDate{"2024-01-01T00:00:00.000000002Z",
                                     NativeDateResolution::kNanosecond};
  const auto date_set = frozen({calendar, midnight, first_ns, second_ns});
  const auto date_unique =
      minera::extract_concept_drift_typed_activities({{date_set}});
  assert(date_unique.size() == 1);
  assert(date_unique.front().children.size() == 4);

  // Frozen-set members may be recursively immutable but mutable set/list
  // containers remain unsupported, at any depth.
  EventAttribute mutable_set = frozen({text("x")});
  mutable_set.container = AttributeContainerKind::kSet;
  EventAttribute mutable_list = frozen({text("x")});
  mutable_list.container = AttributeContainerKind::kList;
  assert(rejected({mutable_set}));
  assert(rejected({mutable_list}));
  assert(rejected({frozen({mutable_set})}));
  assert(rejected({frozen({mutable_list})}));

  EventAttribute too_deep = text("leaf");
  for (std::size_t depth = 0; depth <= 128; ++depth)
    too_deep = frozen({std::move(too_deep)});
  assert(rejected({too_deep}));

  // Raw sublogs retain the original child order and typed scalar aliases;
  // canonical identity is used only for detector labels.
  const auto raw_a = frozen({integer(1, "first"), text("phase", "phase")});
  const auto raw_b = frozen({text("phase", "phase-copy"), boolean(true, "bool")});
  const std::vector<std::vector<EventAttribute>> raw_traces{
      {raw_a}, {raw_b}, {raw_a}, {raw_b}};
  minera::ConceptDriftTraceOptions raw_options;
  raw_options.sub_log_size = 1;
  raw_options.window_size = 1;
  raw_options.num_permutations = 1;
  raw_options.threshold = 1.1;
  raw_options.max_no_change_points = 5;
  std::mt19937 raw_rng(0);
  const auto raw_result = minera::detect_concept_drift_typed_traces(
      raw_traces, raw_options, raw_rng);
  std::vector<EventAttribute> restored;
  for (const auto& group : raw_result.sublogs)
    for (const auto& trace : group)
      restored.insert(restored.end(), trace.begin(), trace.end());
  assert(restored == std::vector<EventAttribute>(
                         {raw_a, raw_b, raw_a, raw_b}));
  assert(raw_rng() == 3626093760U);
}
