#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "minera/concept_drift.hpp"

namespace minera_bose_typed_helpers_tests {

inline minera::EventAttribute integer(std::int64_t value,
                                      std::string key = {}) {
  minera::EventAttribute result{std::move(key), std::to_string(value),
                                minera::AttributeKind::kInt};
  result.typed_value = value;
  return result;
}

inline minera::EventAttribute boolean(bool value, std::string key = {}) {
  minera::EventAttribute result{std::move(key), value ? "True" : "False",
                                minera::AttributeKind::kBoolean};
  result.typed_value = value;
  return result;
}

inline minera::EventAttribute text(std::string value,
                                   std::string key = {}) {
  return minera::EventAttribute{std::move(key), std::move(value)};
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

inline minera::EventAttribute null_value(std::string key = {}) {
  auto result = text("", std::move(key));
  result.is_null = true;
  result.first_class_state = minera::FirstClassAttributeState::kNull;
  return result;
}

inline minera::EventAttribute nan_value() {
  minera::EventAttribute result{"", "nan", minera::AttributeKind::kFloat};
  result.typed_value = std::numeric_limits<double>::quiet_NaN();
  return result;
}

inline minera::EventTable mixed_date_table() {
  minera::EventTable table;
  table.columns = {"case:concept:name", "concept:name", "time:timestamp"};
  table.dtypes = {"str", "object", "datetime64[ns, UTC]"};
  table.rows = {
      {text("case-date", "case:concept:name"),
       date("2020-01-01", "concept:name"),
       date("2020-01-01T00:00:00Z", "time:timestamp")},
      {text("case-datetime", "case:concept:name"),
       date("2020-01-01T00:00:00", "concept:name"),
       date("2020-01-02T00:00:00Z", "time:timestamp")}};
  return table;
}

inline minera::EventLog mixed_date_log() {
  minera::EventLog log;
  for (const auto& [activity, timestamp] :
       std::vector<std::pair<std::string, std::string>>{
           {"2020-01-01", "2020-01-01T00:00:00Z"},
           {"2020-01-01T00:00:00", "2020-01-02T00:00:00Z"}}) {
    minera::Event event;
    event.activity = activity;
    event.activity_state = minera::FirstClassAttributeState::kPresent;
    event.activity_attribute = date(activity, "concept:name");
    event.timestamp = timestamp;
    event.timestamp_state = minera::FirstClassAttributeState::kPresent;
    event.timestamp_attribute = date(timestamp, "time:timestamp");
    minera::Trace trace;
    trace.events.push_back(std::move(event));
    log.traces.push_back(std::move(trace));
  }
  return log;
}

inline std::set<std::pair<std::string, std::string>> rendered_pairs(
    const std::vector<std::pair<minera::EventAttribute,
                                minera::EventAttribute>>& pairs) {
  std::set<std::pair<std::string, std::string>> result;
  for (const auto& [source, target] : pairs)
    result.emplace(source.value, target.value);
  return result;
}

}  // namespace minera_bose_typed_helpers_tests

inline void test_bose_typed_helpers() {
  using namespace minera_bose_typed_helpers_tests;

  // Unique activities are sorted while keeping the first original scalar
  // representative of equality aliases and preserving copied identity.
  const auto first_int = integer(1, "first-int");
  const auto first_bool = boolean(true, "first-bool");
  const auto false_bool = boolean(false, "false-bool");
  const std::vector<std::vector<minera::EventAttribute>> unique_input{{
      first_int, first_bool, integer(2), false_bool, integer(0)}};
  const auto unique = minera::extract_concept_drift_typed_activities(unique_input);
  assert(unique.size() == 3);
  assert(unique[0].kind == minera::AttributeKind::kBoolean);
  assert(unique[0].typed_value.scalar_identity() ==
         false_bool.typed_value.scalar_identity());
  assert(unique[1].kind == minera::AttributeKind::kInt);
  assert(unique[1].typed_value.scalar_identity() ==
         first_int.typed_value.scalar_identity());
  assert(unique[2].value == "2");

  const auto bool_first = boolean(true, "bool-first");
  const auto int_second = integer(1, "int-second");
  const auto first_alias = minera::extract_concept_drift_typed_activities(
      {{bool_first, int_second}});
  assert(first_alias.size() == 1);
  assert(first_alias.front().kind == minera::AttributeKind::kBoolean);
  assert(first_alias.front().typed_value.scalar_identity() ==
         bool_first.typed_value.scalar_identity());
  assert(minera::extract_concept_drift_typed_activities({}).empty());

  bool rejected = false;
  try {
    (void)minera::extract_concept_drift_typed_activities(
        {{integer(1), text("1")}});
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
  rejected = false;
  try {
    (void)minera::extract_concept_drift_typed_activities(
        {{date("2020-01-01"), date("2020-01-01T00:00:00")}});
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);

  const auto shared_nan = nan_value();
  const auto second_nan = nan_value();
  const auto nan_unique = minera::extract_concept_drift_typed_activities(
      {{shared_nan, shared_nan, second_nan}});
  assert(nan_unique.size() == 2);
  std::set<std::uint64_t> nan_identities;
  for (const auto& item : nan_unique)
    nan_identities.insert(item.typed_value.scalar_identity());
  assert(nan_identities.contains(shared_nan.typed_value.scalar_identity()));
  assert(nan_identities.contains(second_nan.typed_value.scalar_identity()));

  // Explicit typed alphabets do not sort or coerce mixed values. Duplicate
  // entries retain their positions and multiply that label's feature counts.
  const std::vector<std::vector<minera::EventAttribute>> mixed_trace{{
      integer(1), text("A"), boolean(true)}};
  const std::vector<minera::EventAttribute> mixed_alphabet{
      text("A"), integer(1)};
  const auto mixed_features = minera::extract_concept_drift_typed_features(
      mixed_trace, mixed_alphabet);
  assert((mixed_features == std::vector<double>{1, 0, 1, 2, 0, 0}));

  const std::vector<std::vector<minera::EventAttribute>> repeated_trace{{
      text("A"), text("B"), text("A")}};
  const std::vector<minera::EventAttribute> duplicate_alphabet{
      text("A"), text("A"), text("B")};
  const auto duplicate_features = minera::extract_concept_drift_typed_features(
      repeated_trace, duplicate_alphabet);
  assert((duplicate_features ==
          std::vector<double>{3, 0, 0, 3, 0, 0, 2, 0, 1}));
  assert((minera::extract_concept_drift_typed_features(
              {}, {text("A")}) == std::vector<double>{0, 0, 1}));
  assert(minera::extract_concept_drift_typed_features({}, {}).empty());

  const auto calendar_date = date("2020-01-01");
  const auto midnight = date("2020-01-01T00:00:00");
  const std::vector<std::vector<minera::EventAttribute>> date_trace{{
      calendar_date, midnight, calendar_date}};
  const auto date_features = minera::extract_concept_drift_typed_features(
      date_trace, {calendar_date, midnight});
  assert((date_features == std::vector<double>{2, 0, 0, 1, 0, 1}));

  rejected = false;
  try {
    (void)minera::extract_concept_drift_typed_features(
        {{text("A"), text("B")}}, {text("A")});
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);

  // Follows filters target values only and retains first generated source and
  // target representatives when Python-style equality aliases are present.
  const auto bool_target = boolean(true);
  const auto alias_pairs = minera::compute_concept_drift_typed_follows_relation(
      {integer(1), bool_target}, {bool_target});
  assert(alias_pairs.size() == 1);
  assert(alias_pairs[0].first.kind == minera::AttributeKind::kInt);
  assert(alias_pairs[0].first.value == "1");
  assert(alias_pairs[0].second.kind == minera::AttributeKind::kBoolean);
  assert(alias_pairs[0].second.typed_value.scalar_identity() ==
         bool_target.typed_value.scalar_identity());

  const auto partial_pairs = minera::compute_concept_drift_typed_follows_relation(
      {text("A"), text("B"), text("C"), text("A")},
      {text("A"), text("C")});
  const std::set<std::pair<std::string, std::string>> expected_partial{
      {"A", "A"}, {"A", "C"}, {"B", "A"}, {"B", "C"}, {"C", "A"}};
  assert(rendered_pairs(partial_pairs) == expected_partial);
  assert(minera::compute_concept_drift_typed_follows_relation(
             {text("X"), text("Y")}, {})
             .empty());

  const auto date_pairs = minera::compute_concept_drift_typed_follows_relation(
      date_trace.front(), {calendar_date, midnight});
  const std::set<std::pair<std::string, std::string>> expected_date_pairs{
      {"2020-01-01", "2020-01-01"},
      {"2020-01-01", "2020-01-01T00:00:00"},
      {"2020-01-01T00:00:00", "2020-01-01"}};
  assert(rendered_pairs(date_pairs) == expected_date_pairs);

  auto same_nan = nan_value();
  const auto shared_nan_pairs = minera::compute_concept_drift_typed_follows_relation(
      {same_nan, text("A"), same_nan}, {same_nan, text("A")});
  assert(shared_nan_pairs.size() == 3);
  const auto other_nan = nan_value();
  const auto distinct_nan_pairs =
      minera::compute_concept_drift_typed_follows_relation(
          {same_nan, text("A"), other_nan}, {nan_value(), text("A")});
  assert(distinct_nan_pairs.size() == 1);
  assert(distinct_nan_pairs.front().first.typed_value.scalar_identity() ==
         same_nan.typed_value.scalar_identity());
  assert(distinct_nan_pairs.front().second.value == "A");

  // Mixed calendar-date/datetime activity comparison rejects before detector
  // work, including negative sub-log size on raw, EventLog, and table paths.
  auto negative = minera::ConceptDriftTraceOptions{};
  negative.sub_log_size = -1;
  negative.window_size = 1;
  negative.num_permutations = 1;
  std::mt19937 typed_rng(17), typed_untouched(17);
  rejected = false;
  try {
    (void)minera::detect_concept_drift_typed_traces(
        {{calendar_date}, {midnight}}, negative, typed_rng);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
  assert(typed_rng() == typed_untouched());

  auto apply_options = minera::ConceptDriftTableOptions{};
  apply_options.sub_log_size = -1;
  apply_options.window_size = 1;
  apply_options.num_permutations = 1;
  std::mt19937 log_rng(19), log_untouched(19);
  rejected = false;
  try {
    (void)minera::apply_concept_drift_log(mixed_date_log(), apply_options,
                                          log_rng);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
  assert(log_rng() == log_untouched());

  std::mt19937 table_rng(23), table_untouched(23);
  rejected = false;
  try {
    (void)minera::apply_concept_drift_table(mixed_date_table(), apply_options,
                                            table_rng);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
  assert(table_rng() == table_untouched());
}
