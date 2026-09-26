#pragma once

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "minera/concept_drift.hpp"

namespace minera_bose_log_apply_tests {

inline minera::EventLog input_log() {
  const std::array<std::vector<std::string>, 4> labels{{
      {"A", "B"}, {"C", "D"}, {"E", "F"}, {"G", "H"}}};
  const std::array<std::string, 4> start_times{{
      "1970-01-01T00:00:01Z", "1970-01-01T00:00:11Z",
      "1970-01-01T00:00:21Z", "1970-01-01T00:00:31Z"}};
  minera::EventLog log;
  log.attributes.push_back({"log_marker", "input-log"});
  log.metadata.properties.push_back({"metadata_marker", "input-metadata"});
  for (std::size_t trace_index = 0; trace_index < labels.size(); ++trace_index) {
    minera::Trace trace;
    trace.case_id = "case-" + std::to_string(trace_index);
    trace.case_id_state = minera::FirstClassAttributeState::kPresent;
    trace.attributes.push_back({"trace_marker", "input-trace"});
    for (const auto& label : labels[trace_index]) {
      minera::Event event;
      event.case_id = trace.case_id;
      event.case_id_state = minera::FirstClassAttributeState::kPresent;
      event.activity = label;
      event.activity_state = minera::FirstClassAttributeState::kPresent;
      event.timestamp = start_times[trace_index];
      event.timestamp_state = minera::FirstClassAttributeState::kPresent;
      event.attributes.push_back({"event_marker", "input-event"});
      event.attributes.push_back({"selected_activity", label + "!"});
      minera::EventAttribute selected_time{
          "selected_time", start_times[trace_index], minera::AttributeKind::kDate};
      selected_time.typed_value = minera::NativeDate{
          start_times[trace_index], minera::NativeDateResolution::kMicrosecond,
          std::string("UTC")};
      event.attributes.push_back(std::move(selected_time));
      trace.events.push_back(std::move(event));
    }
    log.traces.push_back(std::move(trace));
  }
  return log;
}

}  // namespace minera_bose_log_apply_tests

inline void test_bose_log_apply() {
  using namespace minera_bose_log_apply_tests;

  auto log = input_log();
  minera::ConceptDriftTableOptions options;
  options.sub_log_size = 2;
  options.window_size = 1;
  options.num_permutations = 1;
  options.threshold = 1.1;
  options.max_no_change_points = 1;
  options.activity_attribute = "selected_activity";
  options.timestamp_attribute = "selected_time";
  options.case_id_attribute = "unused_case_selector";
  std::mt19937 rng(0);
  const auto applied = minera::apply_concept_drift_log(log, options, rng);
  assert(applied.change_timestamps == std::vector<double>{21.0});
  assert(applied.p_values == std::vector<double>{1.0});
  assert(applied.returned_tables.size() == 2);
  assert(rng() == 2546248239U);

  const auto& first = applied.returned_tables.front();
  const auto& second = applied.returned_tables.back();
  for (const auto* table : {&first, &second}) {
    assert(table->columns == std::vector<std::string>(
                                 {"case:concept:name", "concept:name",
                                  "time:timestamp"}));
    assert(table->dtypes == std::vector<std::string>(
                                {"str", "str", "datetime64[us, UTC]"}));
    assert(table->rows.size() == 4);
    assert(table->attributes.empty());
    assert(table->metadata == minera::EventDataMetadata{});
    assert(table->categorical_dtypes.empty());
    assert(table->index && table->index->kind == "RangeIndex");
    assert(table->index->labels.size() == table->rows.size());
  }
  assert(first.rows[0][0]->value == "0");
  assert(first.rows[0][1]->value == "A!");
  assert(first.rows[1][1]->value == "B!");
  assert(first.rows[2][0]->value == "1");
  assert(first.rows[2][1]->value == "C!");
  assert(std::get<minera::NativeDate>(first.rows[0][2]->typed_value).value ==
         "1970-04-26T17:46:40.000000Z");
  assert(second.rows[0][0]->value == "0");
  assert(second.rows[0][1]->value == "E!");

  auto numeric_log = input_log();
  std::int64_t numeric_label = 0;
  for (auto& trace : numeric_log.traces) {
    for (auto& event : trace.events) {
      for (auto& attribute : event.attributes) {
        if (attribute.key == "selected_activity") {
          attribute.kind = minera::AttributeKind::kInt;
          attribute.value = std::to_string(numeric_label);
          attribute.typed_value = numeric_label++;
        }
      }
    }
  }
  std::mt19937 numeric_rng(0);
  bool rejected_numeric_output = false;
  try {
    (void)minera::apply_concept_drift_log(numeric_log, options, numeric_rng);
  } catch (const std::invalid_argument&) {
    rejected_numeric_output = true;
  }
  assert(rejected_numeric_output);
  assert(numeric_rng() == 2546248239U);

  auto mixed_log = input_log();
  auto& null_activity = mixed_log.traces.front().events.front().attributes[1];
  null_activity.is_null = true;
  null_activity.first_class_state = minera::FirstClassAttributeState::kNull;
  std::mt19937 mixed_rng(0);
  bool rejected_mixed_activity = false;
  try {
    (void)minera::apply_concept_drift_log(mixed_log, options, mixed_rng);
  } catch (const std::invalid_argument&) {
    rejected_mixed_activity = true;
  }
  assert(rejected_mixed_activity);
  assert(mixed_rng() == 2357136044U);

  auto signed_options = options;
  signed_options.sub_log_size = -1;
  signed_options.window_size = -1;
  signed_options.num_permutations = -1;
  signed_options.max_no_change_points = -1;
  std::mt19937 signed_rng(0);
  const auto signed_result =
      minera::apply_concept_drift_log(log, signed_options, signed_rng);
  assert(signed_result.returned_tables.empty());
  assert(signed_result.change_timestamps ==
         (std::vector<double>{11.0, 1.0}));
  assert(signed_result.p_values.size() == 2);
  assert(std::signbit(signed_result.p_values[0]));
  assert(std::signbit(signed_result.p_values[1]));
  assert(signed_rng() == 2357136044U);

  minera::EventLog empty_log;
  std::mt19937 empty_rng(0);
  const auto empty_result =
      minera::apply_concept_drift_log(empty_log, options, empty_rng);
  assert(empty_result.returned_tables.empty());
  assert(empty_result.change_timestamps.empty());
  assert(empty_result.p_values.empty());
  assert(empty_rng() == 2357136044U);

  minera::EventLog empty_trace_log;
  empty_trace_log.traces.emplace_back();
  std::mt19937 empty_trace_rng(0);
  bool rejected_empty_trace = false;
  try {
    (void)minera::apply_concept_drift_log(empty_trace_log, options,
                                          empty_trace_rng);
  } catch (const std::invalid_argument&) {
    rejected_empty_trace = true;
  }
  assert(rejected_empty_trace);
  assert(empty_trace_rng() == 2357136044U);

  // A negative group size yields no raw groups, while a negative window can
  // still rank a point whose signed timestamp index is invalid. The detector
  // consumes all seven permutation shuffles before the adapter resolves it.
  auto indexed_options = options;
  indexed_options.sub_log_size = -2;
  indexed_options.window_size = -3;
  indexed_options.num_permutations = 1;
  indexed_options.max_no_change_points = 1;
  std::mt19937 indexed_rng(0);
  bool rejected_index = false;
  try {
    (void)minera::apply_concept_drift_log(log, indexed_options, indexed_rng);
  } catch (const std::invalid_argument&) {
    rejected_index = true;
  }
  assert(rejected_index);
  std::mt19937 expected_rng(0);
  expected_rng.discard(7);
  assert(indexed_rng() == expected_rng());
}
