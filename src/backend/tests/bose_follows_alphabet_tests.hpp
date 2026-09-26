#pragma once

#include <cassert>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "minera/concept_drift.hpp"

namespace minera_bose_follows_alphabet_tests {

inline minera::EventAttribute text(std::string value,
                                   std::string key = {}) {
  return minera::EventAttribute{std::move(key), std::move(value)};
}

inline minera::EventAttribute container(
    minera::AttributeContainerKind kind,
    std::vector<minera::EventAttribute> children,
    std::string key = {}) {
  minera::EventAttribute result{std::move(key), ""};
  result.container = kind;
  result.children = std::move(children);
  return result;
}

inline bool throws_invalid_argument(const auto& function) {
  try {
    function();
  } catch (const std::invalid_argument&) {
    return true;
  }
  return false;
}

}  // namespace minera_bose_follows_alphabet_tests

inline void test_bose_follows_alphabet() {
  using namespace minera_bose_follows_alphabet_tests;
  using minera::AttributeContainerKind;
  using minera::EventAttribute;

  const auto mutable_list = container(AttributeContainerKind::kList,
                                      {text("A")});
  assert(minera::compute_concept_drift_typed_follows_relation(
             {}, {mutable_list})
             .empty());
  assert(minera::compute_concept_drift_typed_follows_relation(
             {text("A"), text("B")}, {mutable_list})
             .empty());

  // Mutable sets compare by members with trace-side frozensets, including
  // when the frozenset is a target absent from the alphabet's source prefix.
  const auto frozen_a = container(AttributeContainerKind::kFrozenSet,
                                  {text("A")});
  const auto mutable_a = container(AttributeContainerKind::kSet,
                                   {text("A")});
  auto relations = minera::compute_concept_drift_typed_follows_relation(
      {text("source"), frozen_a}, {mutable_a});
  assert(relations.size() == 1);
  assert(relations.front().first.value == "source");
  assert(relations.front().second == frozen_a);
  assert(minera::compute_concept_drift_typed_follows_relation(
             {text("source"), frozen_a},
             {container(AttributeContainerKind::kSet, {text("B")})})
             .empty());

  relations = minera::compute_concept_drift_typed_follows_relation(
      {frozen_a, text("B")}, {mutable_a, text("B")});
  assert(relations.size() == 1);
  assert(relations.front().first == frozen_a);
  assert(relations.front().second.value == "B");

  // Tuples compare left to right. A mutable child can mismatch and allow a
  // later hashable alphabet entry to match the trace target.
  const auto tuple_target = container(
      AttributeContainerKind::kTuple, {text("A"), text("B")});
  const auto tuple_with_list = container(
      AttributeContainerKind::kTuple,
      {text("A"), container(AttributeContainerKind::kList, {text("B")})});
  relations = minera::compute_concept_drift_typed_follows_relation(
      {text("source"), tuple_target}, {tuple_with_list, tuple_target});
  assert(relations.size() == 1);
  assert(relations.front().second == tuple_target);

  const auto tuple_with_set = container(
      AttributeContainerKind::kTuple,
      {mutable_a, text("tail")});
  const auto tuple_with_frozen = container(
      AttributeContainerKind::kTuple,
      {frozen_a, text("tail")});
  relations = minera::compute_concept_drift_typed_follows_relation(
      {text("source"), tuple_with_frozen}, {tuple_with_set});
  assert(relations.size() == 1);
  assert(relations.front().second == tuple_with_frozen);

  // Containers compare false to pandas.NA. Ordered alphabet scanning can
  // continue to a later identical NA and retains tuple short-circuit behavior.
  const auto marker = minera::make_native_na_attribute("trace-na");
  const auto list_none = container(
      AttributeContainerKind::kList,
      {EventAttribute{"", "", minera::AttributeKind::kString, true}});
  for (const auto& ordered_alphabet :
       {std::vector<EventAttribute>{mutable_list, marker},
        std::vector<EventAttribute>{marker, mutable_list}}) {
    relations = minera::compute_concept_drift_typed_follows_relation(
        {text("source"), marker}, ordered_alphabet);
    assert(relations.size() == 1);
    assert(relations.front().second == marker);
  }
  assert(minera::compute_concept_drift_typed_follows_relation(
             {text("source"), marker}, {list_none})
             .empty());

  const auto marker_tuple = container(
      AttributeContainerKind::kTuple, {text("A"), marker});
  const auto list_tuple = container(
      AttributeContainerKind::kTuple,
      {text("A"), container(AttributeContainerKind::kList, {list_none.children.front()})});
  assert(minera::compute_concept_drift_typed_follows_relation(
             {text("source"), marker_tuple}, {list_tuple})
             .empty());

  const auto ambiguous_prefix_target = container(
      AttributeContainerKind::kTuple, {marker, text("A")});
  const auto unequal_prefix = container(
      AttributeContainerKind::kTuple,
      {text("X"), container(AttributeContainerKind::kList, {})});
  assert(throws_invalid_argument([&] {
    (void)minera::compute_concept_drift_typed_follows_relation(
        {text("source"), ambiguous_prefix_target}, {unequal_prefix});
  }));
  const auto same_marker_prefix = container(
      AttributeContainerKind::kTuple,
      {marker, container(AttributeContainerKind::kList, {})});
  assert(minera::compute_concept_drift_typed_follows_relation(
             {text("source"), ambiguous_prefix_target}, {same_marker_prefix})
             .empty());

  // Dictionary-shaped mutable alphabet entries are valid but cannot match a
  // hashable trace activity. Their nested values are still structurally checked.
  auto dictionary = container(AttributeContainerKind::kContainer, {});
  auto dictionary_value = text("", "x");
  dictionary_value.is_null = true;
  dictionary.children.push_back(dictionary_value);
  auto nested_dictionary = container(AttributeContainerKind::kContainer, {});
  nested_dictionary.children.push_back(text("1", "y"));
  dictionary.children.push_back(
      container(AttributeContainerKind::kContainer, {text("1", "y")}, "nested"));
  assert(minera::compute_concept_drift_typed_follows_relation(
             {text("source"), marker}, {dictionary, nested_dictionary, marker})
             .size() == 1);

  const auto mutable_trace_value = container(AttributeContainerKind::kList,
                                              {text("A")});
  assert(throws_invalid_argument([&] {
    (void)minera::compute_concept_drift_typed_follows_relation(
        {mutable_trace_value}, {});
  }));
  assert(throws_invalid_argument([&] {
    (void)minera::extract_concept_drift_typed_features(
        {{text("A"), text("B")}}, {mutable_list});
  }));
  assert(throws_invalid_argument([&] {
    (void)minera::extract_concept_drift_typed_activities(
        {{mutable_trace_value}});
  }));
}
