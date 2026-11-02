#include "minera/log_util.hpp"
#include "minera/log_util_json.hpp"
#include "minera/csv_event_log_importer.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <utility>

using namespace minera;

namespace {

Event event(std::string case_id, std::string activity, std::string timestamp,
            std::size_t input_order) {
  Event result;
  result.case_id = std::move(case_id);
  result.activity = std::move(activity);
  result.timestamp = std::move(timestamp);
  result.input_order = input_order;
  return result;
}

EventLog source_log() {
  EventLog log;
  log.attributes.push_back({"source", "fixture", AttributeKind::kString});
  log.metadata.extensions.push_back(
      {"Fixture", "fx", "https://example.invalid/fixture"});
  log.metadata.properties.push_back(
      {"case_id_key", "case:concept:name", AttributeKind::kString});
  Trace first;
  first.case_id = "c1";
  first.attributes.push_back({"segment", "one", AttributeKind::kString});
  first.properties.push_back({"owner", "team-a", AttributeKind::kString});
  first.events = {event("c1", "A", "1", 0), event("c1", "B", "2", 1),
                 event("c1", "C", "3", 2)};
  Trace second;
  second.case_id = "c2";
  second.events = {event("c2", "A", "10", 3), event("c2", "B", "12", 4)};
  log.traces = {first, second};
  return log;
}

void split_is_seeded_and_preserves_metadata() {
  const auto log = source_log();
  const auto left = split_log_train_test(log, 0.5, 42);
  const auto right = split_log_train_test(log, 0.5, 42);
  assert(left.training.traces.size() == 2);
  assert(left.testing.traces.empty());
  assert(left.training.traces[0].case_id == right.training.traces[0].case_id);
  assert(left.training.attributes.size() == 1);
  assert(left.training.attributes[0].key == "source");
  assert(left.training.metadata == log.metadata);
  const auto original = std::find_if(
      log.traces.begin(), log.traces.end(), [&](const auto& trace) {
        return trace.case_id == left.training.traces[0].case_id;
      });
  assert(original != log.traces.end());
  assert(left.training.traces[0].attributes.size() ==
         original->attributes.size());
  assert(left.training.traces[0].properties == original->properties);
  bool rejected = false;
  try {
    (void)split_log_train_test(log, 1.1, 0);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

void fixed_and_all_prefixes_match_contract() {
  const auto log = source_log();
  const auto fixed = get_log_prefixes(log, 2);
  assert(fixed.traces.size() == 2);
  assert(fixed.traces[0].events.size() == 2);
  assert(fixed.traces[1].events.size() == 2);
  assert(fixed.metadata == log.metadata);
  assert(fixed.traces[0].properties == log.traces[0].properties);
  const auto all = get_log_with_prefixes(log);
  assert(all.log.traces.size() == 10);  // two entries around each event
  assert(all.change_indexes.size() == 2);
  assert((all.change_indexes[0] == std::vector<std::size_t>{5, 5, 5}));
  assert((all.change_indexes[1] == std::vector<std::size_t>{9, 9}));
  const auto json = log_prefix_json(all.log, LogUtilityVariant::kAllPrefixes,
                                    all.change_indexes);
  assert(json.find("\"family\":\"LOG_UTIL\"") != std::string::npos);
  assert(json.find("\"metadata\":{\"extensions\":[{\"name\":\"Fixture\"") !=
         std::string::npos);
  assert(json.find("\"properties\":[{\"key\":\"owner\"") !=
         std::string::npos);
  assert(json.find("\"change_indexes\":[[5,5,5],[9,9]]") != std::string::npos);
}

void derived_prefix_logs_preserve_typed_case_context() {
  EventLog log;
  Trace trace;
  trace.case_id = "42";
  trace.case_id_state = FirstClassAttributeState::kPresent;
  trace.case_id_source_key = "case.number";
  EventAttribute selected{"case.number", "42", AttributeKind::kInt};
  selected.typed_value = std::int64_t{42};
  trace.case_id_attribute = selected;
  trace.events = {event("42", "A", "1", 0), event("42", "B", "2", 1)};
  log.traces.push_back(std::move(trace));

  const auto fixed = get_log_prefixes(log, 1);
  assert(fixed.traces.front().case_id_source_key == "case.number");
  assert(fixed.traces.front().case_id_attribute &&
         fixed.traces.front().case_id_attribute->kind == AttributeKind::kInt &&
         std::get<std::int64_t>(fixed.traces.front().case_id_attribute->typed_value) ==
             42);
  const auto all = get_log_with_prefixes(log);
  assert(all.log.traces.size() == 4);
  for (const auto& prefix : all.log.traces) {
    assert(prefix.case_id == "42");
    assert(prefix.case_id_source_key == "case.number");
    assert(prefix.case_id_attribute &&
           prefix.case_id_attribute->kind == AttributeKind::kInt);
  }
}

void until_activity_returns_prefix_and_duration() {
  const auto log = source_log();
  LogUtilityOptions options;
  options.activity_key = "activity";
  options.timestamp_key = "timestamp";
  const auto result = get_log_traces_until_activity(log, "C", options);
  assert(result.log.traces.size() == 1);
  assert(result.log.traces[0].case_id == "c1");
  assert(result.log.traces[0].events.size() == 2);
  assert(result.elapsed_seconds.size() == 1);
  assert(std::abs(result.elapsed_seconds[0] - 1.0) < 1e-12);
  const auto json = log_until_activity_json(result);
  assert(json.find("\"elapsed_seconds\":[1]") != std::string::npos);
  const auto split_json = log_train_test_json(split_log_train_test(log, 0.5, 1));
  assert(split_json.find("\"training\"") != std::string::npos);
  assert(split_json.find("\"testing\"") != std::string::npos);

  // pm4py 2.7.22 subtracts the instants as recorded: a target logged before
  // its predecessor gives a negative duration (here -3600 s) rather than an
  // error, in both until-activity and the per-target sublogs.
  EventLog reversed;
  Trace trace;
  trace.case_id = "r1";
  trace.events = {event("r1", "A", "2023-01-01T02:00:00+00:00", 0),
                  event("r1", "B", "2023-01-01T01:00:00+00:00", 1)};
  reversed.traces.push_back(trace);
  const auto backwards = get_log_traces_until_activity(reversed, "B", options);
  assert(backwards.elapsed_seconds.size() == 1);
  assert(backwards.elapsed_seconds[0] == -3600.0);
  const auto targets = get_log_traces_to_activities(
      reversed, std::vector<std::string>{"B"}, options);
  assert(targets.activities == std::vector<std::string>{"B"});
  assert(targets.logs.size() == 1 && targets.logs[0].traces.size() == 1);
}

// A batch CSV import types activities like format_dataframe, so the
// reference's until-activity over "010"/"10" sees one activity "10" and its
// cases in case-id order (captured from pm4py 2.7.22 get_log_traces_until_activity:
// prefixes [["10"],["10"]], durations [3600, 1800]).
void until_activity_on_csv_import_uses_typed_activities() {
  std::istringstream input(
      "case_id,activity,timestamp\n"
      "B,10,2024-01-01T09:00:00+00:00\nA,010,2024-01-01T10:00:00+00:00\n"
      "A,20,2024-01-01T11:00:00+00:00\nB,20,2024-01-01T09:30:00+00:00\n");
  const auto log = import_csv_event_log(input);
  LogUtilityOptions options;
  const auto result = get_log_traces_until_activity(log, "20", options);
  assert(result.log.traces.size() == 2);
  assert(result.log.traces[0].case_id == "A" && result.log.traces[1].case_id == "B");
  for (const auto& trace : result.log.traces)
    assert(trace.events.size() == 1 && trace.events[0].activity == "10");
  assert((result.elapsed_seconds == std::vector<double>{3600.0, 1800.0}));
}

void activity_targets_exclude_competing_targets() {
  auto log = source_log();
  Trace third;
  third.case_id = "c3";
  third.events = {event("c3", "A", "20", 5),
                  event("c3", "D", "21", 6),
                  event("c3", "C", "22", 7)};
  log.traces.push_back(third);

  LogUtilityOptions options;
  options.activity_key = "activity";
  options.timestamp_key = "timestamp";
  const auto result = get_log_traces_to_activities(
      log, std::vector<std::string>{"C", "B"}, options);
  assert((result.activities == std::vector<std::string>{"C", "B"}));
  assert(result.logs.size() == 2);
  assert(result.logs[0].traces.size() == 1);
  assert(result.logs[0].traces[0].case_id == "c3");
  assert(result.logs[0].traces[0].events.size() == 2);
  assert(result.logs[1].traces.size() == 1);
  assert(result.logs[1].traces[0].case_id == "c2");
  assert(result.logs[1].traces[0].events.size() == 1);
  const auto json = log_activity_targets_json(result);
  assert(json.find("\"variant\":\"TO_ACTIVITIES\"") !=
         std::string::npos);
  assert(json.find("\"activities\":[\"C\",\"B\"]") !=
         std::string::npos);

  bool rejected = false;
  try {
    (void)get_log_traces_to_activities(
        log, std::vector<std::string>{"B", "B"}, options);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

void feature_selection_keeps_useful_typed_attributes() {
  auto log = source_log();
  for (std::size_t trace_index = 0; trace_index < log.traces.size();
       ++trace_index) {
    auto& trace = log.traces[trace_index];
    trace.attributes.push_back({"region", trace_index == 0 ? "north" : "south",
                                AttributeKind::kString});
    for (std::size_t event_index = 0; event_index < trace.events.size();
         ++event_index) {
      auto& current = trace.events[event_index];
      current.attributes.push_back({
          "score", std::to_string(trace_index * 10 + event_index),
          AttributeKind::kInt});
      current.attributes.push_back({
          "category", event_index % 2 == 0 ? "x" : "y",
          AttributeKind::kString});
      current.attributes.push_back({"ignored_date", "2024-01-01",
                                    AttributeKind::kDate});
    }
  }
  LogFeatureSelectionOptions options;
  options.min_different_values = 2;
  options.max_different_values = 3;
  const auto result = select_log_features(log, options);
  assert((result.event_attributes ==
          std::vector<std::string>{"activity", "category", "score",
                                   "timestamp"}));
  assert((result.case_attributes == std::vector<std::string>{"region"}));
  const auto json = log_feature_selection_json(result);
  assert(json.find("\"variant\":\"FEATURE_SELECTION\"") !=
         std::string::npos);
  assert(json.find("\"case_attributes\":[\"region\"]") !=
         std::string::npos);

  options.min_different_values = 4;
  options.max_different_values = 2;
  bool rejected = false;
  try {
    (void)select_log_features(log, options);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

}  // namespace

int main() {
  split_is_seeded_and_preserves_metadata();
  fixed_and_all_prefixes_match_contract();
  derived_prefix_logs_preserve_typed_case_context();
  until_activity_returns_prefix_and_duration();
  activity_targets_exclude_competing_targets();
  until_activity_on_csv_import_uses_typed_activities();
  feature_selection_keeps_useful_typed_attributes();
  std::cout << "Log utility tests passed\n";
}
