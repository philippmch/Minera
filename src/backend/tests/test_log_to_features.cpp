#include "minera/log_to_features.hpp"
#include "../root_cause_trace_key.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>

#include "minera/json_export.hpp"
#include "minera/timestamp.hpp"

using namespace minera;

static bool operator==(const TemporalFeatureCell& cell, double expected) {
  return std::holds_alternative<double>(cell) && std::get<double>(cell) == expected;
}

static bool operator==(const std::vector<TemporalFeatureCell>& row,
                       const std::vector<double>& expected) {
  return row.size() == expected.size() &&
      std::equal(row.begin(), row.end(), expected.begin(),
                 [](const auto& cell, double value) { return cell == value; });
}

static bool operator==(const LogToFeaturesCell& cell, double expected) {
  return std::holds_alternative<double>(cell) && std::get<double>(cell) == expected;
}

static bool operator==(const std::vector<LogToFeaturesCell>& row,
                       const std::vector<double>& expected) {
  return row.size() == expected.size() &&
      std::equal(row.begin(), row.end(), expected.begin(),
                 [](const auto& cell, double value) { return cell == value; });
}

static bool operator==(const std::vector<std::vector<LogToFeaturesCell>>& rows,
                       const std::vector<std::vector<double>>& expected) {
  return rows.size() == expected.size() &&
      std::equal(rows.begin(), rows.end(), expected.begin(),
                 [](const auto& row, const auto& values) { return row == values; });
}

static EventLog fixture() {
  EventLog log;
  Trace t; t.case_id = "c1";
  t.attributes.push_back({"segment", "gold", AttributeKind::kString});
  Event a{"c1", "A", "2024-01-01T00:00:00Z", "r1", 0};
  a.attributes.push_back({"amount", "2.5", AttributeKind::kFloat});
  a.attributes.push_back({"kind", "x", AttributeKind::kString});
  Event b{"c1", "B", "2024-01-01T01:00:00Z", "r1", 1};
  b.attributes.push_back({"amount", "3", AttributeKind::kFloat});
  t.events = {a, b}; log.traces.push_back(t); return log;
}

static void trace_identifier_feature_preserves_native_cells() {
  auto log = fixture();
  LogToFeaturesOptions options;
  options.event_string_attributes_supplied = true;
  options.add_case_identifier_column = true;
  options.feature_names = {"unknown", "@@case_id_column"};
  options.enable_case_duration = true;
  auto result = log_to_features(log, options);
  assert((result.feature_names == std::vector<std::string>{
      "@@case_id_column", "unknown", "@@case_id_column", "@@caseDuration"}));
  assert(std::get<EventAttribute>(result.trace_rows[0][0]).value == "c1");
  assert(result.trace_rows[0][1] == 0 && result.trace_rows[0][2] == 0 && result.trace_rows[0][3] == 3600);
  assert(log_to_features_json(result).find("\"trace_rows\":[[\"c1\",0,0,3600]]") != std::string::npos);

  options.case_id_key = "identifier";
  options.feature_names.clear();
  options.feature_names_supplied = true;
  options.enable_case_duration = false;
  const auto check = [&](const EventAttribute& attribute, const std::string& json_value) {
    log.traces[0].attributes = {attribute};
    const auto actual = log_to_features(log, options);
    assert(actual.feature_names == std::vector<std::string>{"@@case_id_column"});
    assert(std::get<EventAttribute>(actual.trace_rows[0][0]) == attribute);
    assert(log_to_features_json(actual).find("\"trace_rows\":[[" + json_value + "]]") != std::string::npos);
  };
  check({"identifier", ""}, "\"\"");
  check({"identifier", "opaque", AttributeKind::kId}, "\"opaque\"");
  check({"identifier", "ignored", AttributeKind::kString, true}, "null");
  const auto integer_detail = [](const std::string& value) {
    return "{\"key\":\"identifier\",\"value\":\"" + value +
        "\",\"kind\":\"int\",\"container\":\"scalar\",\"null\":false}";
  };
  check({"identifier", "9007199254740991", AttributeKind::kInt}, "9007199254740991");
  check({"identifier", "-9007199254740991", AttributeKind::kInt}, "-9007199254740991");
  check({"identifier", "9007199254740993", AttributeKind::kInt}, integer_detail("9007199254740993"));
  check({"identifier", "-9007199254740992", AttributeKind::kInt}, integer_detail("-9007199254740992"));
  check({"identifier", "+000123456789012345678901234567890", AttributeKind::kInt}, integer_detail("123456789012345678901234567890"));
  check({"identifier", "1.25", AttributeKind::kFloat}, "1.25");
  check({"identifier", "1e-400", AttributeKind::kFloat}, "0");
  check({"identifier", "1e400", AttributeKind::kFloat}, "\"Infinity\"");
  log.traces[0].attributes = {{"identifier", "0x1p2", AttributeKind::kFloat}};
  bool hex_rejected = false;
  try { (void)log_to_features_json(log_to_features(log, options)); }
  catch (const std::invalid_argument&) { hex_rejected = true; }
  assert(hex_rejected);
  check({"identifier", "true", AttributeKind::kBoolean}, "true");
  EventAttribute native{"identifier", "stale", AttributeKind::kInt};
  native.typed_value = std::int64_t{9007199254740993};
  check(native, integer_detail("9007199254740993"));
  native.kind = AttributeKind::kBoolean;
  native.typed_value = false;
  check(native, "false");
  native.kind = AttributeKind::kFloat;
  native.typed_value = std::numeric_limits<double>::infinity();
  check(native, "\"Infinity\"");
  native.typed_value = -std::numeric_limits<double>::infinity();
  check(native, "\"-Infinity\"");
  native.typed_value = std::nan("");
  check(native, "\"NaN\"");
  native.kind = AttributeKind::kDate;
  native.typed_value = NativeDate{"2024-01-01T00:00:00Z"};
  check(native, "\"2024-01-01T00:00:00Z\"");

  auto bytes = make_bytes_attribute("identifier", std::string("a\0b", 3));
  log.traces[0].attributes = {bytes};
  result = log_to_features(log, options);
  assert(std::get<EventAttribute>(result.trace_rows[0][0]) == bytes);
  assert(log_to_features_json(result).find("\"kind\":\"bytes\"") != std::string::npos);
  EventAttribute nested{"identifier", "", AttributeKind::kString};
  nested.container = AttributeContainerKind::kTuple;
  nested.children = {bytes, native};
  nested.first_class_role = FirstClassAttributeRole::kCaseId;
  nested.first_class_state = FirstClassAttributeState::kPresent;
  nested.first_class_source_key = "identifier";
  log.traces[0].attributes = {nested};
  result = log_to_features(log, options);
  assert(std::get<EventAttribute>(result.trace_rows[0][0]) == nested);
  const auto nested_json = log_to_features_json(result);
  assert(nested_json.find("\"container\":\"tuple\"") != std::string::npos);
  assert(nested_json.find("\"children\":[{") != std::string::npos);
  assert(nested_json.find("\"typed_value\":\"2024-01-01T00:00:00Z\"") != std::string::npos);
  EventAttribute unsafe_child{"large", "stale", AttributeKind::kInt};
  unsafe_child.typed_value = std::int64_t{9007199254740993};
  EventAttribute inner{"inner", "", AttributeKind::kString};
  inner.container = AttributeContainerKind::kList;
  inner.children = {unsafe_child,
      EventAttribute{"larger", "+000123456789012345678901234567890", AttributeKind::kInt}};
  nested.children.push_back(inner);
  log.traces[0].attributes = {nested};
  result = log_to_features(log, options);
  assert(std::get<EventAttribute>(result.trace_rows[0][0]) == nested);
  const auto exact_nested_json = log_to_features_json(result);
  assert(exact_nested_json.find("\"key\":\"large\",\"value\":\"9007199254740993\"") != std::string::npos);
  assert(exact_nested_json.find("\"key\":\"larger\",\"value\":\"123456789012345678901234567890\"") != std::string::npos);
  assert(exact_nested_json.find("\"typed_value\":9007199254740993") == std::string::npos);

  const auto rejects_json = [](const LogToFeaturesResult& malformed) {
    bool rejected = false;
    try { (void)log_to_features_json(malformed); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
  };
  result.feature_names[0] = "unknown";
  rejects_json(result);
  result.feature_names = {"unknown", "@@case_id_column"};
  result.trace_rows[0].insert(result.trace_rows[0].begin(), 0.0);
  rejects_json(result);
  log.traces[0].attributes.clear();
  bool missing_rejected = false;
  try { (void)log_to_features(log, options); }
  catch (const std::invalid_argument&) { missing_rejected = true; }
  assert(missing_rejected);
  options.case_id_key = "concept:name";
  log.traces[0].case_id_attribute = EventAttribute{"concept:name", "stale", AttributeKind::kInt};
  log.traces[0].case_id_attribute->typed_value = std::int64_t{42};
  log.traces[0].case_id_state = FirstClassAttributeState::kPresent;
  result = log_to_features(log, options);
  assert(std::get<EventAttribute>(result.trace_rows[0][0]) == *log.traces[0].case_id_attribute);
  assert(log_to_features_json(result).find("\"trace_rows\":[[42]]") != std::string::npos);
}

static Event timed_event(const std::string& case_id, const std::string& activity,
                         const std::string& start, const std::string& finish,
                         std::size_t order) {
  Event event{case_id, activity, finish, "r1", order};
  event.attributes.push_back({"start_time", start, AttributeKind::kDate});
  return event;
}

static void occurrence_timing_uses_input_order_and_interval_endpoints() {
  EventLog log;
  Trace ordered;
  ordered.events = {
      timed_event("ordered", "A", "2024-01-01T00:00:00Z", "2024-01-01T00:00:10Z", 0),
      timed_event("ordered", "B", "2024-01-01T00:00:15Z", "2024-01-01T00:00:20Z", 1),
      timed_event("ordered", "A", "2024-01-01T00:00:25Z", "2024-01-01T00:00:30Z", 2)};
  Trace unsorted;
  unsorted.events = {
      timed_event("unsorted", "A", "2024-01-01T00:00:20Z", "2024-01-01T00:00:30Z", 0),
      timed_event("unsorted", "B", "2024-01-01T00:00:00Z", "2024-01-01T00:00:10Z", 1),
      timed_event("unsorted", "A", "2024-01-01T00:00:15Z", "2024-01-01T00:00:50Z", 2)};
  log.traces = {ordered, unsorted, Trace{}};
  LogToFeaturesOptions options;
  options.event_string_attributes_supplied = true;
  options.start_timestamp_key = "start_time";
  options.enable_case_duration = true;
  options.enable_times_from_first_occurrence = true;
  options.enable_times_from_last_occurrence = true;
  options.default_not_present = -7;
  const auto result = log_to_features(log, options);
  assert((result.feature_names == std::vector<std::string>{
      "@@caseDuration", "startToFirstOcc@@A", "firstOccToEnd@@A",
      "startToFirstOcc@@B", "firstOccToEnd@@B", "startToLastOcc@@A",
      "lastOccToEnd@@A", "startToLastOcc@@B", "lastOccToEnd@@B"}));
  assert((result.trace_rows == std::vector<std::vector<double>>{
      {30, -10, 15, 5, 5, 15, -5, 5, 5},
      {30, -10, -15, -30, 5, -15, -35, -30, 5},
      {0, -7, -7, -7, -7, -7, -7, -7, -7}}));
  options.start_timestamp_key.clear();
  const auto completion_starts = log_to_features(log, options);
  assert((completion_starts.trace_rows[0] == std::vector<double>{20, 0, 20, 10, 10, 20, 0, 10, 10}));
  assert((completion_starts.trace_rows[1] == std::vector<double>{20, 0, 20, -20, 40, 20, 0, -20, 40}));

  // Only the selected occurrence and boundary timestamps are accessed.
  options.start_timestamp_key = "start_time";
  log.traces = {ordered};
  log.traces[0].events[1] = Event{"ordered", "A", "", "", 1};
  const auto missing_middle = log_to_features(log, options);
  assert((missing_middle.trace_rows[0] == std::vector<double>{30, -10, 15, 15, -5}));
  options.enable_times_from_first_occurrence = false;
  options.enable_times_from_last_occurrence = false;
  log.traces[0].events[1].activity = "B";
  assert(log_to_features(log, options).trace_rows[0][0] == 30);
  options.enable_times_from_first_occurrence = true;
  bool rejected = false;
  try { (void)log_to_features(log, options); }
  catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected);

  // A missing first start does not affect last-occurrence times.
  options.enable_case_duration = false;
  options.enable_times_from_first_occurrence = false;
  options.enable_times_from_last_occurrence = true;
  log.traces[0].events = {ordered.events.front(), ordered.events.back()};
  log.traces[0].events.front().attributes.clear();
  assert((log_to_features(log, options).trace_rows[0] == std::vector<double>{15, -5}));
  // A missing last completion does not affect first-occurrence times.
  options.enable_times_from_first_occurrence = true;
  options.enable_times_from_last_occurrence = false;
  log.traces[0].events.front() = ordered.events.front();
  log.traces[0].events.back().timestamp.clear();
  assert((log_to_features(log, options).trace_rows[0] == std::vector<double>{-10, 15}));
}

static void extra_features_append_to_custom_core_schema_and_respect_overrides() {
  const auto log = fixture();
  LogToFeaturesOptions options;
  options.event_string_attributes_supplied = true;
  options.feature_names_supplied = true;
  options.enable_case_duration = true;
  auto result = log_to_features(log, options);
  assert(result.feature_names == std::vector<std::string>{"@@caseDuration"});
  assert(result.trace_rows == std::vector<std::vector<double>>{{3600}});
  options.feature_names = {"@@caseDuration"};
  result = log_to_features(log, options);
  assert((result.feature_names == std::vector<std::string>{"@@caseDuration", "@@caseDuration"}));
  assert((result.trace_rows == std::vector<std::vector<double>>{{0, 3600}}));
  options.feature_names.clear();
  options.enable_case_duration = false;
  options.enable_all_extra_features = true;
  result = log_to_features(log, options);
  assert(result.case_ids.empty());
  assert((result.feature_names == std::vector<std::string>{
      "@@caseDuration", "startToFirstOcc@@A", "firstOccToEnd@@A",
      "startToFirstOcc@@B", "firstOccToEnd@@B", "startToLastOcc@@A",
      "lastOccToEnd@@A", "startToLastOcc@@B", "lastOccToEnd@@B",
      "directPathPerformanceLastOcc@@A##B", "@@work_in_progress",
      "resource_workload@@r1", "firstIndexAct@@A", "lastIndexAct@@A",
      "firstIndexAct@@B", "lastIndexAct@@B", "@@max_concurrent_activities_general",
      "@@max_concurrent_activities_like_A", "@@max_concurrent_activities_like_B"}));
  options.enable_case_duration_supplied = true;
  options.enable_times_from_first_occurrence_supplied = true;
  options.enable_times_from_last_occurrence_supplied = true;
  options.enable_direct_paths_times_last_occ_supplied = true;
  options.enable_indirect_paths_times_last_occ_supplied = true;
  options.enable_work_in_progress_supplied = true;
  options.enable_resource_workload_supplied = true;
  options.enable_first_last_activity_index_supplied = true;
  options.enable_max_concurrent_events_supplied = true;
  options.enable_max_concurrent_events_per_activity_supplied = true;
  result = log_to_features(log, options);
  assert(result.feature_names.empty());
  assert(result.trace_rows == std::vector<std::vector<double>>{{}});
  options.enable_case_duration = true;
  assert(log_to_features(log, options).trace_rows == std::vector<std::vector<double>>{{3600}});
}

static void observed_path_timing_uses_configured_starts() {
  EventLog log;
  Trace paths;
  paths.case_id = "paths";
  paths.events = {
      timed_event("paths", "A", "2024-01-01T00:00:00Z", "2024-01-01T00:00:10Z", 0),
      timed_event("paths", "B", "2024-01-01T00:00:15Z", "2024-01-01T00:00:30Z", 1),
      timed_event("paths", "A", "2024-01-01T00:00:35Z", "2024-01-01T00:00:40Z", 2),
      timed_event("paths", "B", "2024-01-01T00:00:20Z", "2024-01-01T00:00:42Z", 3),
      timed_event("paths", "C", "2024-01-01T00:00:50Z", "2024-01-01T00:01:00Z", 4),
  };
  log.traces.push_back(paths);
  Trace self_loop;
  self_loop.case_id = "self";
  self_loop.events = {
      timed_event("self", "A", "2024-01-01T00:01:00Z", "2024-01-01T00:01:10Z", 0),
      timed_event("self", "A", "2024-01-01T00:01:15Z", "2024-01-01T00:01:20Z", 1),
  };
  log.traces.push_back(self_loop);

  LogToFeaturesOptions options;
  options.variant = LogToFeaturesVariant::TRACE_BASED;
  options.start_timestamp_key = "start_time";
  options.enable_succ_def_representation = false;
  options.enable_direct_paths_times_last_occ = true;
  options.enable_indirect_paths_times_last_occ = true;
  const auto result = log_to_features(log, options);

  const auto find_feature = [&](const std::string& name) {
    return std::find(result.feature_names.begin(), result.feature_names.end(), name);
  };
  const auto direct_ab = find_feature("directPathPerformanceLastOcc@@A##B");
  assert(direct_ab != result.feature_names.end());
  assert(result.trace_rows.front()[static_cast<std::size_t>(direct_ab - result.feature_names.begin())] == 5.0);
  assert(find_feature("directPathPerformanceLastOcc@@A##C") == result.feature_names.end());
  assert(find_feature("directPathPerformanceLastOcc@@A##A") != result.feature_names.end());

  const auto indirect_ac = find_feature("indirectPathPerformanceLastOcc@@A##C");
  assert(indirect_ac != result.feature_names.end());
  assert(result.trace_rows.front()[static_cast<std::size_t>(indirect_ac - result.feature_names.begin())] == 10.0);
  const auto indirect_aa = find_feature("indirectPathPerformanceLastOcc@@A##A");
  assert(indirect_aa != result.feature_names.end());
  assert(result.trace_rows.front()[static_cast<std::size_t>(indirect_aa - result.feature_names.begin())] == 25.0);
}

static void indirect_paths_include_every_nonadjacent_later_event() {
  const auto check = [](const std::string& sequence, const std::vector<std::string>& pairs,
                        const std::vector<double>& expected, bool reverse_last = false) {
    EventLog log;
    Trace trace;
    for (std::size_t i = 0; i < sequence.size(); ++i) {
      const auto time_index = reverse_last && i + 1 == sequence.size() ? 0 : i;
      const auto start = std::string(1, static_cast<char>('0' + time_index)) + "0";
      const auto finish = std::string(1, static_cast<char>('0' + time_index)) + "5";
      trace.events.push_back(timed_event("c", sequence.substr(i, 1),
          "1970-01-01T00:00:" + start + "Z", "1970-01-01T00:00:" + finish + "Z", i));
    }
    log.traces.push_back(trace);
    LogToFeaturesOptions options;
    options.event_string_attributes_supplied = true;
    options.enable_indirect_paths_times_last_occ = true;
    options.start_timestamp_key = "start_time";
    const auto result = log_to_features(log, options);
    std::vector<std::string> names;
    for (const auto& pair : pairs) names.push_back("indirectPathPerformanceLastOcc@@" + pair);
    assert(result.feature_names == names);
    assert(result.trace_rows[0] == expected);
  };
  check("ABCD", {"A##C", "A##D", "B##D"}, {15,25,15});
  check("ABCDE", {"A##C", "A##D", "A##E", "B##D", "B##E", "C##E"}, {15,25,35,15,25,15});
  check("ABAB", {"A##A", "A##B", "B##B"}, {15,25,15});
  check("AAAA", {"A##A"}, {15});
  check("AAAA", {"A##A"}, {15}, true);
}

static void explicit_trace_attributes_scope_missing_values() {
  EventLog log;
  Trace first;
  first.case_id = "c1";
  first.attributes = {{"segment", "gold", AttributeKind::kString},
                      {"priority", "2", AttributeKind::kInt}};
  Event first_a{"c1", "A", "2024-01-01T00:00:00Z", "r1", 0};
  first_a.attributes.push_back({"kind", "x", AttributeKind::kString});
  Event first_b{"c1", "B", "2024-01-01T00:01:00Z", "r1", 1};
  first.events = {first_a, first_b};
  log.traces.push_back(first);

  Trace second;
  second.case_id = "c2";
  second.attributes = {{"segment", "silver", AttributeKind::kString},
                       {"priority", "3", AttributeKind::kInt}};
  Event second_a{"c2", "A", "2024-01-02T00:00:00Z", "r2", 0};
  second_a.attributes.push_back({"kind", "y", AttributeKind::kString});
  Event second_b{"c2", "B", "2024-01-02T00:01:00Z", "r2", 1};
  second_b.attributes.push_back({"kind", "y", AttributeKind::kString});
  second.events = {second_a, second_b};
  log.traces.push_back(second);

  Trace third;
  third.case_id = "c3";
  third.attributes = {{"segment", "bronze", AttributeKind::kString},
                      {"priority", "4", AttributeKind::kInt}};
  third.events = {Event{"c3", "C", "2024-01-03T00:00:00Z", "r3", 0}};
  log.traces.push_back(third);

  LogToFeaturesOptions options;
  options.variant = LogToFeaturesVariant::TRACE_BASED;
  options.enable_succ_def_representation = false;
  options.event_string_attributes = {"kind"};
  options.case_string_attributes = {"segment"};
  options.case_numeric_attributes = {"priority"};
  const auto result = log_to_features(log, options);
  const auto find_feature = [&](const std::string& name) {
    return std::find(result.feature_names.begin(), result.feature_names.end(), name);
  };
  const auto segment = find_feature("trace:segment@gold");
  const auto priority = find_feature("trace:priority");
  const auto undefined = find_feature("event:kind@UNDEFINED");
  const auto kind_x = find_feature("event:kind@x");
  assert(segment != result.feature_names.end());
  assert(priority != result.feature_names.end());
  assert(undefined != result.feature_names.end());
  assert(kind_x != result.feature_names.end());
  assert(result.trace_rows[0][static_cast<std::size_t>(segment - result.feature_names.begin())] == 1.0);
  assert(result.trace_rows[0][static_cast<std::size_t>(priority - result.feature_names.begin())] == 2.0);
  assert(result.trace_rows[0][static_cast<std::size_t>(kind_x - result.feature_names.begin())] == 1.0);
  assert(result.trace_rows[0][static_cast<std::size_t>(undefined - result.feature_names.begin())] == 0.0);
  assert(result.trace_rows[2][static_cast<std::size_t>(undefined - result.feature_names.begin())] == 1.0);
}

static void overlap_extras_use_case_intervals_and_other_events() {
  EventLog log;
  Trace first;
  first.case_id = "c1";
  first.events = {
      timed_event("c1", "A", "2024-01-01T00:00:00Z", "2024-01-01T00:00:10Z", 0),
      timed_event("c1", "A", "2024-01-01T00:00:05Z", "2024-01-01T00:00:20Z", 1),
  };
  log.traces.push_back(first);
  Trace second;
  second.case_id = "c2";
  second.events = {
      timed_event("c2", "C", "2024-01-01T00:00:08Z", "2024-01-01T00:00:15Z", 0),
  };
  log.traces.push_back(second);

  LogToFeaturesOptions options;
  options.variant = LogToFeaturesVariant::TRACE_BASED;
  options.start_timestamp_key = "start_time";
  options.enable_succ_def_representation = false;
  options.enable_work_in_progress = true;
  options.enable_resource_workload = true;
  options.enable_max_concurrent_events = true;
  options.enable_max_concurrent_events_per_activity = true;
  const auto result = log_to_features(log, options);
  const auto find_feature = [&](const std::string& name) {
    return std::find(result.feature_names.begin(), result.feature_names.end(), name);
  };
  const auto wip = find_feature("@@work_in_progress");
  const auto workload = find_feature("resource_workload@@r1");
  const auto concurrent = find_feature("@@max_concurrent_activities_general");
  const auto concurrent_a = find_feature("@@max_concurrent_activities_like_A");
  assert(wip != result.feature_names.end());
  assert(workload != result.feature_names.end());
  assert(concurrent != result.feature_names.end());
  assert(concurrent_a != result.feature_names.end());
  const auto index = [&](decltype(wip) feature) {
    return static_cast<std::size_t>(feature - result.feature_names.begin());
  };
  assert(result.trace_rows[0][index(wip)] == 2.0);
  assert(result.trace_rows[0][index(workload)] == 2.0);
  assert(result.trace_rows[0][index(concurrent)] == 1.0);
  assert(result.trace_rows[0][index(concurrent_a)] == 1.0);
  assert(result.trace_rows[1][index(wip)] == 2.0);
  assert(result.trace_rows[1][index(workload)] == 2.0);
}

static void workload_intervals_and_resource_types_match_exact_boundaries() {
  const auto make_trace = [](const std::string& start, const std::string& finish,
                             const std::optional<std::string>& resource) {
    Trace trace;
    auto event = timed_event("c", "A", "1970-01-01T00:00:" + start + "Z",
                              "1970-01-01T00:00:" + finish + "Z", 0);
    event.resource.clear();
    if (resource) event.attributes.push_back({"worker", *resource});
    trace.events.push_back(event);
    return trace;
  };
  EventLog log;
  log.traces = {make_trace("00", "10", "r"), make_trace("10", "20", "r"),
      make_trace("20.5", "30", "s"), make_trace("00", "10", "r"),
      make_trace("40", "50", ""), make_trace("08", "09", std::nullopt)};
  // Multiple uses of the same resource in one trace count only once.
  log.traces[0].events.push_back(log.traces[0].events[0]);
  LogToFeaturesOptions options;
  options.event_string_attributes_supplied = true;
  options.enable_work_in_progress = true;
  options.enable_resource_workload = true;
  options.resource_key = "worker";
  options.start_timestamp_key = "start_time";
  options.default_not_present = -7;
  options.epsilon = 0;
  auto result = log_to_features(log, options);
  assert((result.feature_names == std::vector<std::string>{"@@work_in_progress",
      "resource_workload@@", "resource_workload@@r", "resource_workload@@s"}));
  assert((result.trace_rows == std::vector<std::vector<double>>{
      {3,-7,2,-7}, {1,-7,1,-7}, {1,-7,-7,1}, {3,-7,2,-7}, {1,1,-7,-7}, {3,-7,-7,-7}}));
  options.epsilon = .25;
  result = log_to_features(log, options);
  assert((result.trace_rows == std::vector<std::vector<double>>{
      {4,-7,3,-7}, {3,-7,3,-7}, {1,-7,-7,1}, {4,-7,3,-7}, {1,1,-7,-7}, {3,-7,-7,-7}}));
  log.traces[2] = make_trace("20.25", "30", "s");
  result = log_to_features(log, options);
  assert(result.trace_rows[1][0] == 4 && result.trace_rows[2][0] == 2);
  const auto rejects = [&](const EventLog& invalid) {
    bool rejected = false;
    try { (void)log_to_features(invalid, options); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
  };
  log.traces = {make_trace("10", "10", "r")};
  assert((log_to_features(log, options).trace_rows[0] == std::vector<double>{1,1}));
  options.epsilon = 0;
  rejects(log);
  options.epsilon = .25;
  log.traces = {make_trace("10.25", "10", "r")};
  assert((log_to_features(log, options).trace_rows[0] == std::vector<double>{1,1}));
  log.traces = {make_trace("10.5", "10", "r")};
  rejects(log);
  log.traces = {make_trace("00", "10", std::nullopt)};
  log.traces[0].events[0].attributes.clear();
  rejects(log);
  log.traces = {make_trace("00", "10", std::nullopt)};
  log.traces[0].events[0].timestamp.clear();
  rejects(log);
  log.traces = {make_trace("00", "10", "r")};
  log.traces[0].events.push_back(Event{"c", "A"});
  log.traces[0].events.push_back(log.traces[0].events.front());
  assert((log_to_features(log, options).trace_rows[0] == std::vector<double>{1,1}));
  log.traces = {Trace{}};
  rejects(log);
  options.enable_resource_workload = false;
  assert((log_to_features(log, options).trace_rows[0] == std::vector<double>{-7}));
  options.enable_resource_workload = true;
  log.traces.clear();
  assert(log_to_features(log, options).trace_rows.empty());

  log.traces = {make_trace("00", "10", std::nullopt)};
  const auto rejects_resource = [&](const EventAttribute& attribute) {
    log.traces[0].events[0].attributes.resize(1);
    log.traces[0].events[0].attributes.push_back(attribute);
    rejects(log);
  };
  rejects_resource({"worker", "1", AttributeKind::kInt});
  rejects_resource({"worker", "1.5", AttributeKind::kFloat});
  rejects_resource({"worker", "true", AttributeKind::kBoolean});
  rejects_resource({"worker", "", AttributeKind::kString, true});
  rejects_resource(make_bytes_attribute("worker", "r"));
  EventAttribute native{"worker", "r"};
  native.typed_value = std::int64_t{1};
  rejects_resource(native);
  native.typed_value = std::monostate{};
  for (const auto kind : {AttributeContainerKind::kList, AttributeContainerKind::kTuple,
                          AttributeContainerKind::kSet, AttributeContainerKind::kFrozenSet,
                          AttributeContainerKind::kContainer}) {
    native.container = kind;
    rejects_resource(native);
  }
}

static void concurrency_uses_exact_comparison_and_required_accesses() {
  Trace trace;
  trace.events = {
      timed_event("c", "A", "1970-01-01T00:00:00Z", "1970-01-01T00:00:10Z", 0),
      timed_event("c", "B", "1970-01-01T00:00:10.1Z", "1970-01-01T00:00:20Z", 1)};
  EventLog log;
  log.traces = {trace, Trace{}};
  LogToFeaturesOptions options;
  options.event_string_attributes_supplied = true;
  options.start_timestamp_key = "start_time";
  options.enable_max_concurrent_events = true;
  options.enable_max_concurrent_events_per_activity = true;
  options.default_not_present = -7;
  options.epsilon = 100;
  auto result = log_to_features(log, options);
  assert((result.feature_names == std::vector<std::string>{
      "@@max_concurrent_activities_general", "@@max_concurrent_activities_like_A",
      "@@max_concurrent_activities_like_B"}));
  assert((result.trace_rows == std::vector<std::vector<double>>{{0,0,0}, {0,0,0}}));
  log.traces[0].events[1].attributes[0].value = "1970-01-01T00:00:10Z";
  assert((log_to_features(log, options).trace_rows[0] == std::vector<double>{1,0,0}));
  log.traces[0].events[0].attributes.clear();
  log.traces[0].events[1].timestamp.clear();
  assert((log_to_features(log, options).trace_rows[0] == std::vector<double>{1,0,0}));
  const auto rejects = [&] {
    bool rejected = false;
    try { (void)log_to_features(log, options); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
  };
  log.traces[0].events[0].timestamp.clear();
  rejects();
  log.traces[0].events[0].timestamp = "1970-01-01T00:00:10Z";
  log.traces[0].events[1].attributes.clear();
  rejects();
  log.traces[0].events.resize(1);
  log.traces[0].events[0].timestamp.clear();
  assert((log_to_features(log, options).trace_rows[0] == std::vector<double>{0,0}));
}

static void trace_extra_timestamps_respect_native_values_and_presence() {
  Trace trace;
  auto event = timed_event("c", "A", "stale", "stale", 0);
  event.attributes[0].typed_value = NativeDate{"1970-01-01T00:00:00Z"};
  event.timestamp_attribute = EventAttribute{"completed", "stale", AttributeKind::kDate};
  event.timestamp_attribute->typed_value = NativeDate{"1970-01-01T00:00:10Z"};
  event.timestamp_source_key = "completed";
  event.timestamp_state = FirstClassAttributeState::kPresent;
  trace.events = {event};
  EventLog log;
  log.traces = {trace};
  LogToFeaturesOptions options;
  options.event_string_attributes_supplied = true;
  options.start_timestamp_key = "start_time";
  options.timestamp_key = "completed";
  options.enable_work_in_progress = true;
  options.enable_case_duration = true;
  options.epsilon = 0;
  assert((log_to_features(log, options).trace_rows[0] == std::vector<double>{10,1}));
  options.timestamp_key = "time:timestamp";
  assert((log_to_features(log, options).trace_rows[0] == std::vector<double>{10,1}));
  const auto rejects = [&] {
    bool rejected = false;
    try { (void)log_to_features(log, options); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
  };
  log.traces[0].events[0].timestamp_state = FirstClassAttributeState::kNull;
  rejects();
  log.traces[0].events[0].timestamp_state = FirstClassAttributeState::kMissing;
  rejects();
  log.traces[0].events[0] = event;
  log.traces[0].events[0].attributes[0].is_null = true;
  rejects();
  log.traces[0].events[0] = event;
  log.traces[0].events[0].attributes[0].container = AttributeContainerKind::kTuple;
  rejects();
  log.traces[0].events[0] = event;
  log.traces[0].events[0].attributes[0] = make_bytes_attribute("start_time", "1970-01-01T00:00:00Z");
  log.traces[0].events[0].attributes[0].typed_value = NativeDate{"1970-01-01T00:00:00Z"};
  rejects();
}

static void default_trace_selection_ignores_custom_core_schema() {
  EventLog log;
  Trace trace;
  trace.case_id = "c";
  trace.events = {timed_event("c", "A", "1970-01-01T00:00:00Z", "1970-01-01T00:00:10Z", 0)};
  trace.events[0].resource.clear();
  trace.events[0].attributes = {{"v", "2", AttributeKind::kInt}};
  log.traces = {trace};
  LogToFeaturesOptions options;
  const auto expected = log_to_features(log, options);
  assert((expected.feature_names == std::vector<std::string>{"event:concept:name@A", "event:v",
      "succession:concept:name@UNDEFINED"}));
  assert((expected.trace_rows[0] == std::vector<double>{1,2,1}));
  options.feature_names_supplied = true;
  for (const auto& schema : {std::vector<std::string>{}, std::vector<std::string>{"unknown"},
                            std::vector<std::string>{"event:v"}}) {
    options.feature_names = schema;
    const auto result = log_to_features(log, options);
    assert(result.feature_names == expected.feature_names);
    assert(result.trace_rows == expected.trace_rows);
  }
  options.event_successor_attributes = {"v"};
  options.event_successor_attributes_supplied = true;
  options.feature_names = {"unknown", "@@caseDuration"};
  options.enable_case_duration = true;
  options.add_case_identifier_column = true;
  const auto extras = log_to_features(log, options);
  assert((extras.feature_names == std::vector<std::string>{"@@case_id_column", "event:concept:name@A",
      "event:v", "succession:concept:name@UNDEFINED", "@@caseDuration"}));
  assert(std::get<EventAttribute>(extras.trace_rows[0][0]).value == "c");
  assert(extras.trace_rows[0][1] == 1 && extras.trace_rows[0][2] == 2 &&
         extras.trace_rows[0][3] == 1 && extras.trace_rows[0][4] == 0);
  options.event_string_attributes_supplied = true;
  options.event_successor_attributes.clear();
  options.feature_names = {"unknown"};
  options.enable_case_duration = false;
  options.add_case_identifier_column = false;
  const auto explicit_empty = log_to_features(log, options);
  assert(explicit_empty.feature_names == std::vector<std::string>{"unknown"});
  assert((explicit_empty.trace_rows[0] == std::vector<double>{0}));
  options.feature_names.clear();
  assert(log_to_features(log, options).trace_rows[0].empty());
}

static void default_trace_strings_have_fixed_cardinality_limit() {
  EventLog log;
  const auto add_trace = [&](int value) {
    Trace trace;
    trace.case_id = "c" + std::to_string(value);
    trace.attributes = {{"group", std::to_string(value)}};
    Event event{trace.case_id, "A" + std::to_string(value)};
    event.attributes = {{"label", std::to_string(value)}, {"number", std::to_string(value), AttributeKind::kInt}};
    trace.events = {event};
    log.traces.push_back(trace);
  };
  for (int i = 0; i < 12; ++i) add_trace(i);
  LogToFeaturesOptions options;
  options.enable_succ_def_representation = false;
  options.min_num_different_string_values = 500;
  options.max_num_different_string_values = 500;
  const auto count_prefix = [](const auto& result, const std::string& prefix) {
    return std::count_if(result.feature_names.begin(), result.feature_names.end(),
                         [&](const auto& name) { return name.starts_with(prefix); });
  };
  auto result = log_to_features(log, options);
  assert(count_prefix(result, "trace:group@") == 12);
  assert(count_prefix(result, "event:label@") == 12);
  assert(count_prefix(result, "event:concept:name@") == 12);
  add_trace(12);
  result = log_to_features(log, options);
  assert(count_prefix(result, "trace:group@") == 0);
  assert(count_prefix(result, "event:label@") == 0);
  assert(count_prefix(result, "event:concept:name@") == 13);
  assert(count_prefix(result, "event:number") == 1);
  options.enable_activity_def_representation = false;
  result = log_to_features(log, options);
  assert(result.feature_names == std::vector<std::string>{"event:number"});
  options.event_string_attributes = {"label"};
  options.case_string_attributes = {"group"};
  result = log_to_features(log, options);
  assert(count_prefix(result, "trace:group@") == 13);
  assert(count_prefix(result, "event:label@") == 13);
}

static void default_trace_selection_validates_all_candidate_hashability() {
  EventLog log;
  log.traces = {Trace{"c1", {Event{"c1", "A"}}}, Trace{"c2", {Event{"c2", "A"}}}};
  EventAttribute mutable_value{"value", "", AttributeKind::kString};
  mutable_value.children = {{"0", "1", AttributeKind::kInt}};
  LogToFeaturesOptions options;
  options.feature_names_supplied = true;
  options.event_successor_attributes = {"unknown"};
  const auto rejects = [&] {
    bool rejected = false;
    try { (void)log_to_features(log, options); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
  };
  for (const auto kind : {AttributeContainerKind::kList, AttributeContainerKind::kContainer,
                          AttributeContainerKind::kSet}) {
    mutable_value.container = kind;
    log.traces[0].events[0].attributes = {mutable_value};
    rejects();
    log.traces[0].events[0].attributes.clear();
    log.traces[0].attributes = {mutable_value};
    rejects();
    log.traces[0].attributes.clear();
  }
  mutable_value.container = AttributeContainerKind::kList;
  EventAttribute tuple{"value", "", AttributeKind::kString};
  tuple.container = AttributeContainerKind::kTuple;
  tuple.children = {mutable_value};
  log.traces[0].events[0].attributes = {tuple};
  rejects();
  tuple.children[0].is_null = true;
  log.traces[0].events[0].attributes = {tuple};
  (void)log_to_features(log, options);
  tuple.children.clear();
  for (const auto kind : {AttributeContainerKind::kTuple, AttributeContainerKind::kFrozenSet}) {
    tuple.container = kind;
    log.traces[0].events[0].attributes = {tuple};
    const auto result = log_to_features(log, options);
    assert(std::none_of(result.feature_names.begin(), result.feature_names.end(),
                        [](const auto& name) { return name.starts_with("event:value"); }));
  }
  log.traces[0].events[0].attributes.clear();
  auto case_id = mutable_value;
  case_id.key = "concept:name";
  log.traces[0].case_id_attribute = case_id;
  log.traces[0].case_id_state = FirstClassAttributeState::kPresent;
  (void)log_to_features(log, options);
  log.traces[0].events[0].activity_attribute = case_id;
  log.traces[0].events[0].activity_state = FirstClassAttributeState::kPresent;
  rejects();
  log.traces[0].events[0].activity_attribute.reset();
  log.traces[0].events[0].attributes = {mutable_value};
  options.event_string_attributes_supplied = true;
  assert(log_to_features(log, options).trace_rows[0].empty());
  options.event_string_attributes = {"value"};
  options.feature_names_supplied = false;
  options.event_successor_attributes.clear();
  const auto explicit_list = log_to_features(log, options);
  // List attributes retain the ordered key/value child wrapper from import.
  assert((explicit_list.feature_names == std::vector<std::string>{"event:value@UNDEFINED",
      "event:value@{'value': None, 'children': [('0', 1)]}"}));
  assert((explicit_list.trace_rows == std::vector<std::vector<double>>{{0,1}, {1,0}}));
}

static void temporal_calendar_uses_selected_column_timezone() {
  const auto check = [](const std::string& first, const std::string& last,
                        const std::string& frequency, const std::vector<std::string>& labels,
                        const std::vector<double>& counts) {
    EventLog log;
    log.traces = {Trace{"c", {Event{"c", "A", first, "r"}, Event{"c", "B", last, "r"}}}};
    LogToFeaturesOptions options;
    options.variant = LogToFeaturesVariant::TEMPORAL;
    options.grouper_freq = frequency;
    const auto result = log_to_features(log, options);
    assert(result.temporal_rows.size() == labels.size());
    for (std::size_t i = 0; i < labels.size(); ++i) {
      const auto& row = result.temporal_rows[i];
      assert(row.timestamp == *parse_timestamp(labels[i]));
      assert(row.values[2] == counts[i]);
      if (counts[i] == 0)
        assert(std::all_of(row.values.begin(), row.values.end(), [](const auto& value) { return value == 0; }));
      else {
        assert(row.values[5] == *parse_timestamp(last) - *parse_timestamp(first));
        assert(row.values[6] == row.values[5] && row.values[7] == 0);
      }
    }
  };
  check("2024-01-02T00:30:00+02:00", "2024-01-02T02:30:00+0200", "D",
      {"2024-01-02T00:00:00+02:00"}, {2});
  check("2024-01-01T23:30:00-05:00", "2024-01-02T00:30:00-05:00", "D",
      {"2024-01-01T00:00:00-05:00", "2024-01-02T00:00:00-05:00"}, {1,1});
  check("2024-01-07T23:30:00-05:00", "2024-01-08T00:30:00-05:00", "W",
      {"2024-01-07T00:00:00-05:00", "2024-01-14T00:00:00-05:00"}, {1,1});
  check("2024-01-31T23:30:00-05:00", "2024-02-01T00:30:00-05:00", "ME",
      {"2024-01-31T00:00:00-05:00", "2024-02-29T00:00:00-05:00"}, {1,1});
  check("2024-02-15T00:30:00+05:30", "2024-08-20T00:30:00+05:30", "2ME",
      {"2024-02-29T00:00:00+05:30", "2024-04-30T00:00:00+05:30",
       "2024-06-30T00:00:00+05:30", "2024-08-31T00:00:00+05:30"}, {1,0,0,1});
  check("2024-01-02T00:30:00Z", "2024-01-02T02:30:00+00:00", "D",
      {"2024-01-02T00:00:00Z"}, {2});
  check("1969-12-31T23:30:00-01:00", "1970-01-01T00:30:00-01:00", "D",
      {"1969-12-31T00:00:00-01:00", "1970-01-01T00:00:00-01:00"}, {1,1});
  check("1969-12-28T23:30:00-01:00", "1969-12-29T00:30:00-01:00", "W",
      {"1969-12-28T00:00:00-01:00", "1970-01-04T00:00:00-01:00"}, {1,1});
  check("1969-12-31T23:30:00-01:00", "1970-01-01T00:30:00-01:00", "ME",
      {"1969-12-31T00:00:00-01:00", "1970-01-31T00:00:00-01:00"}, {1,1});

  EventLog log;
  log.traces = {Trace{"c", {Event{"c", "A", "2024-01-01T22:30:00Z", "r"},
                            Event{"c", "B", "2024-01-01T23:30:00Z", "r"}}}};
  for (std::size_t i = 0; i < 2; ++i) {
    EventAttribute start{"local", "stale", AttributeKind::kString};
    start.typed_value = NativeDate{i == 0 ? "2024-01-02T00:30:00+02:00" : "2024-01-02T01:30:00+02:00"};
    log.traces[0].events[i].attributes.push_back(start);
  }
  LogToFeaturesOptions options;
  options.variant = LogToFeaturesVariant::TEMPORAL;
  options.grouper_freq = "D";
  options.start_timestamp_key = "local";
  auto result = log_to_features(log, options);
  assert(result.temporal_rows.size() == 1);
  assert(result.temporal_rows[0].timestamp == *parse_timestamp("2024-01-02T00:00:00+02:00"));
  assert(result.temporal_rows[0].values[5] == 3600 && result.temporal_rows[0].values[6] == 3600);
  assert(log.traces[0].events[0].attributes[0].value == "stale");
  const auto rejects = [&] {
    bool rejected = false;
    try { (void)log_to_features(log, options); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
  };
  auto& selected = log.traces[0].events[1].attributes[0];
  selected.typed_value = NativeDate{"2024-01-02T01:30:00+03:00"};
  rejects();
  selected.typed_value = NativeDate{"2024-01-02T01:30:00"};
  rejects();
  selected.first_class_state = FirstClassAttributeState::kNull;
  result = log_to_features(log, options);
  assert(result.temporal_rows[0].values[2] == 1 && result.temporal_rows[0].values[5] == 3600);
  selected.first_class_state = FirstClassAttributeState::kPresent;
  selected.typed_value = NativeDate{"2024-01-02T01:30:00+02:00"};
  log.traces[0].events[1].timestamp = "2024-01-02T01:30:00+02:00";
  rejects();
  log.traces[0].events[1].timestamp = "2024-01-01T23:30:00";
  rejects();
  options.grouper_freq.clear();
  options.start_timestamp_key.clear();
  assert(!log_to_features(log, options).temporal_rows.empty());
}

struct FiscalExpected { double timestamp; std::vector<double> values; };
struct FiscalScenario { std::string frequency; std::size_t input; std::vector<FiscalExpected> rows; };
#include "fixtures/temporal_fiscal_calendar.inc"

static void temporal_calendar_fiscal_month_anchors() {
  assert(fiscal_scenarios.size() == 168);
  for (const auto& scenario : fiscal_scenarios) {
    EventLog log;
    const auto& dates = fiscal_inputs.at(scenario.input);
    for (std::size_t i = 0; i < dates.size(); ++i) {
      const auto id = std::to_string(i);
      log.traces.push_back(Trace{id, {Event{id, "A", dates[i], "r"}}});
    }
    LogToFeaturesOptions options;
    options.variant = LogToFeaturesVariant::TEMPORAL;
    options.grouper_freq = scenario.frequency;
    const auto result = log_to_features(log, options);
    const auto require = [&](bool value, const std::string& detail) {
      if (!value) {
        const auto message = "Fiscal calendar " + scenario.frequency +
            ", input " + std::to_string(scenario.input) + ": " + detail;
        std::fprintf(stderr, "%s\n", message.c_str());
        std::fflush(stderr);
        throw std::runtime_error(message);
      }
    };
    require(result.feature_names.size() == 13, "canonical column count");
    require(result.temporal_rows.size() == scenario.rows.size(), "row count");
    for (std::size_t i = 0; i < scenario.rows.size(); ++i) {
      const auto& actual = result.temporal_rows[i];
      const auto& expected = scenario.rows[i];
      require(actual.timestamp == expected.timestamp, "timestamp " + std::to_string(i) +
          ", actual " + std::to_string(actual.timestamp) + ", expected " + std::to_string(expected.timestamp));
      require(actual.values.size() == 13 && expected.values.size() == 13, "cell count");
      for (std::size_t j = 0; j < 13; ++j)
        require(std::holds_alternative<double>(actual.values[j]) &&
                    std::abs(std::get<double>(actual.values[j]) - expected.values[j]) <= 1e-9,
                "row " + std::to_string(i) + ", column " + std::to_string(j) + ", actual " +
                    (std::holds_alternative<double>(actual.values[j]) ? std::to_string(std::get<double>(actual.values[j])) : "string") +
                    ", expected " + std::to_string(expected.values[j]));
    }
  }
  EventLog log;
  for (const auto& date : {"2024-01-01T12:00:00Z", "2024-02-15T12:00:00Z", "2024-06-30T12:00:00Z"}) {
    const auto id = std::to_string(log.traces.size());
    log.traces.push_back(Trace{id, {Event{id, "A", date, "r"}}});
  }
  LogToFeaturesOptions options;
  options.variant = LogToFeaturesVariant::TEMPORAL;
  for (const auto& frequency : {"1QE-JAN", "+2QE-JAN", "2 QE-JAN"}) {
    options.grouper_freq = frequency;
    const auto result = log_to_features(log, options);
    const bool single = options.grouper_freq == "1QE-JAN";
    assert(result.temporal_rows.size() == (single ? 3 : 2));
    assert(result.temporal_rows.front().timestamp == *parse_timestamp("2024-01-31T00:00:00Z"));
    assert(result.temporal_rows.back().timestamp == *parse_timestamp("2024-07-31T00:00:00Z"));
    assert(result.temporal_rows.front().values[2] == 1);
    assert(result.temporal_rows.back().values[2] == (single ? 1 : 2));
    if (single) assert(result.temporal_rows[1].timestamp == *parse_timestamp("2024-04-30T00:00:00Z"));
  }
  for (const auto& frequency : {"qe-jan", "qE-JAN", "QE-jan", "QS-feb", "ys-jul", "YE-Jun",
                                "0QE-JAN", "-1QE-JAN", "QE-FOO", "QE-JAN-extra", "MS-JAN", "MONTH-JAN",
                                "Q-JAN", "Y-JUN", "A-JUN", "AS-JUL"}) {
    options.grouper_freq = frequency;
    bool rejected = false;
    try { (void)log_to_features(log, options); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
  }
}

struct BusinessScenario {
  std::string frequency;
  std::size_t input;
  bool single_case, rejects;
  std::vector<FiscalExpected> rows;
};
#include "fixtures/temporal_business_calendar.inc"

static void temporal_calendar_business_days_and_periods() {
  assert(business_scenarios.size() == 269);
  for (const auto& scenario : business_scenarios) {
    EventLog log;
    if (scenario.single_case) log.traces = {Trace{"c", {}}};
    const auto& dates = business_inputs.at(scenario.input);
    for (std::size_t i = 0; i < dates.size(); ++i) {
      const auto id = scenario.single_case ? "c" : std::to_string(i);
      Event event{id, "A", dates[i], "r"};
      if (scenario.single_case) log.traces[0].events.push_back(event);
      else log.traces.push_back(Trace{id, {event}});
    }
    LogToFeaturesOptions options;
    options.variant = LogToFeaturesVariant::TEMPORAL;
    options.grouper_freq = scenario.frequency;
    const auto require = [&](bool value, const std::string& detail) {
      if (value) return;
      const auto message = "Business calendar " + scenario.frequency + ", input " +
          std::to_string(scenario.input) + ": " + detail;
      std::fprintf(stderr, "%s\n", message.c_str()); std::fflush(stderr);
      throw std::runtime_error(message);
    };
    bool rejected = false;
    try {
      const auto result = log_to_features(log, options);
      require(!scenario.rejects, "expected rejection");
      require(result.temporal_rows.size() == scenario.rows.size(), "row count");
      for (std::size_t i = 0; i < scenario.rows.size(); ++i) {
        const auto& actual = result.temporal_rows[i];
        const auto& expected = scenario.rows[i];
        require(actual.timestamp == expected.timestamp, "timestamp " + std::to_string(i));
        require(actual.values.size() == 13, "cell count");
        for (std::size_t j = 0; j < 13; ++j)
          require(std::holds_alternative<double>(actual.values[j]) &&
                      std::abs(std::get<double>(actual.values[j]) - expected.values[j]) <= 1e-9,
                  "row " + std::to_string(i) + ", column " + std::to_string(j));
      }
    } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected == scenario.rejects, "rejection status");
  }
}

static void temporal_calendar_grid_uses_origin_and_fills_empty_periods() {
  const auto check = [](const std::string& first, const std::string& last,
                        const std::string& frequency, const std::vector<std::string>& labels,
                        const std::vector<double>& counts) {
    EventLog log;
    log.traces = {Trace{"a", {Event{"a", "A", first, "r"}}},
                  Trace{"b", {Event{"b", "B", last, "r"}}}};
    LogToFeaturesOptions options;
    options.variant = LogToFeaturesVariant::TEMPORAL;
    options.grouper_freq = frequency;
    options.default_not_present = -7;
    const auto result = log_to_features(log, options);
    assert(result.feature_names.size() == 13);
    assert(result.temporal_rows.size() == labels.size());
    for (std::size_t i = 0; i < labels.size(); ++i) {
      assert(result.temporal_rows[i].timestamp == *parse_timestamp(labels[i]));
      assert(result.temporal_rows[i].values[2] == counts[i]);
      if (counts[i] == 0)
        assert(std::all_of(result.temporal_rows[i].values.begin(), result.temporal_rows[i].values.end(),
                           [](const auto& value) { return value == 0; }));
    }
  };
  check("2024-01-01T12:00:00Z", "2024-01-03T12:00:00Z", "D",
      {"2024-01-01T00:00:00Z", "2024-01-02T00:00:00Z", "2024-01-03T00:00:00Z"}, {1,0,1});
  check("2024-01-01T12:00:00Z", "2024-01-08T12:00:00Z", "2D",
      {"2024-01-01T00:00:00Z", "2024-01-03T00:00:00Z", "2024-01-05T00:00:00Z", "2024-01-07T00:00:00Z"}, {1,0,0,1});
  check("2024-01-01T12:00:00Z", "2024-01-08T12:00:00Z", "2W",
      {"2024-01-07T00:00:00Z", "2024-01-21T00:00:00Z"}, {1,1});
  check("2024-01-02T03:00:00Z", "2024-01-03T10:00:00Z", "7h",
      {"2024-01-02T00:00:00Z", "2024-01-02T07:00:00Z", "2024-01-02T14:00:00Z", "2024-01-02T21:00:00Z", "2024-01-03T04:00:00Z"}, {1,0,0,0,1});
  check("2024-02-15T12:00:00Z", "2024-08-20T12:00:00Z", "2ME",
      {"2024-02-29T00:00:00Z", "2024-04-30T00:00:00Z", "2024-06-30T00:00:00Z", "2024-08-31T00:00:00Z"}, {1,0,0,1});
  check("2024-02-15T12:00:00Z", "2024-08-20T12:00:00Z", "2MS",
      {"2024-02-01T00:00:00Z", "2024-04-01T00:00:00Z", "2024-06-01T00:00:00Z", "2024-08-01T00:00:00Z"}, {1,0,0,1});
  check("2024-02-15T12:00:00Z", "2024-08-20T12:00:00Z", "2QE",
      {"2024-03-31T00:00:00Z", "2024-09-30T00:00:00Z"}, {1,1});
  check("2024-02-15T12:00:00Z", "2024-08-20T12:00:00Z", "2QS",
      {"2024-01-01T00:00:00Z", "2024-07-01T00:00:00Z"}, {1,1});
  check("2024-02-15T12:00:00Z", "2024-08-20T12:00:00Z", "2YE", {"2024-12-31T00:00:00Z"}, {2});
  check("2024-02-15T12:00:00Z", "2024-08-20T12:00:00Z", "2YS", {"2024-01-01T00:00:00Z"}, {2});
  check("2024-02-29T23:59:59Z", "2024-03-01T00:00:00Z", "ME",
      {"2024-02-29T00:00:00Z", "2024-03-31T00:00:00Z"}, {1,1});
  check("2024-01-07T23:59:59Z", "2024-01-08T00:00:00Z", "W",
      {"2024-01-07T00:00:00Z", "2024-01-14T00:00:00Z"}, {1,1});
  check("2024-01-02T06:59:59Z", "2024-01-02T07:00:00Z", "7h",
      {"2024-01-02T00:00:00Z", "2024-01-02T07:00:00Z"}, {1,1});
  EventLog log;
  log.traces = {Trace{"a", {Event{"a", "A", "2024-01-01T00:00:00Z"}}},
                Trace{"b", {Event{"b", "B", "2024-01-08T00:00:00Z"}}}};
  LogToFeaturesOptions options;
  options.variant = LogToFeaturesVariant::TEMPORAL;
  options.grouper_freq.clear();
  options.window_seconds = 86400;
  assert(log_to_features(log, options).temporal_rows.size() == 2);
  for (const auto& frequency : {"9223372036854775807D", "9223372036854775807QE",
                                 "9223372036854775807ME"}) {
    options.grouper_freq = frequency;
    log.traces.back().events[0].timestamp = "2024-02-08T00:00:00Z";
    bool rejected = false;
    try { (void)log_to_features(log, options); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
  }
}

static void temporal_calendar_metrics_use_completions_and_canonical_schema() {
  EventLog log;
  Trace trace;
  trace.case_id = "c";
  trace.events = {
      timed_event("c", "A", "1970-01-01T00:00:00Z", "1970-01-01T00:00:10Z", 0),
      timed_event("c", "B", "1970-01-01T00:00:20Z", "1970-01-01T00:00:40Z", 1)};
  log.traces = {trace};
  LogToFeaturesOptions options;
  options.variant = LogToFeaturesVariant::TEMPORAL;
  options.grouper_freq = "D";
  options.start_timestamp_key = "start_time";
  const auto expected = log_to_features(log, options);
  assert((expected.temporal_rows[0].values == std::vector<double>{1,2,2,0,0,30,30,0,0,1,2,1,1}));
  options.feature_names_supplied = true;
  options.event_string_attributes = {"", "duplicate", "duplicate"};
  options.event_attributes = {"", "duplicate", "duplicate"};
  options.case_attributes = options.event_attributes;
  for (const auto& schema : {std::vector<std::string>{}, std::vector<std::string>{"unknown"},
                            std::vector<std::string>{"num_events", "num_events", ""}}) {
    options.feature_names = schema;
    const auto actual = log_to_features(log, options);
    assert(actual.feature_names == expected.feature_names);
    assert(actual.temporal_rows[0].values == expected.temporal_rows[0].values);
  }
  log.traces[0].events[1] = timed_event("c", "B", "1970-01-01T00:00:05Z", "1970-01-01T00:00:20Z", 1);
  assert((log_to_features(log, options).temporal_rows[0].values == std::vector<double>{1,2,2,0,0,10,10,0,0,1,2,1,1}));
  log.traces[0].events = {
      timed_event("c", "A", "1970-01-01T00:00:20Z", "1970-01-01T00:00:10Z", 0),
      timed_event("c", "B", "1970-01-01T00:00:50Z", "1970-01-01T00:00:40Z", 1)};
  assert(log_to_features(log, options).temporal_rows[0].values == expected.temporal_rows[0].values);
  options.event_string_attributes.clear();
  options.event_attributes.clear();
  options.case_attributes.clear();
  options.grouper_freq.clear();
  options.feature_names = {"average_service_time", "average_sojourn_time"};
  bool rejected = false;
  try { (void)log_to_features(log, options); }
  catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected);
  log.traces[0] = trace;
  const auto fixed = log_to_features(log, options);
  assert(fixed.feature_names == options.feature_names);
  assert((fixed.temporal_rows[0].values == std::vector<double>{30,40}));
}

static void temporal_calendar_means_weight_distinct_cases_equally() {
  EventLog log;
  log.traces = {
      Trace{"one", {Event{"one", "A", "1970-01-01T00:00:10Z", "r1"}}},
      Trace{"three", {Event{"three", "B", "1970-01-01T00:00:00Z", "r1"},
                       Event{"three", "C", "1970-01-01T00:00:20Z", "r2"},
                       Event{"three", "D", "1970-01-01T00:00:40Z", "r2"}}}};
  LogToFeaturesOptions options;
  options.variant = LogToFeaturesVariant::TEMPORAL;
  options.grouper_freq = "D";
  const auto result = log_to_features(log, options);
  assert(result.temporal_rows.size() == 1);
  assert((result.temporal_rows[0].values == std::vector<double>{
      2,4,4,5,15,20,20,0,0,1.5,2,2,1.5}));
}

static void temporal_calendar_rates_ignore_empty_traces() {
  EventLog log;
  const Trace nonempty{"c", {Event{"c", "A", "1970-01-01T00:00:10Z", "r"}}};
  log.traces = {nonempty};
  LogToFeaturesOptions options;
  options.variant = LogToFeaturesVariant::TEMPORAL;
  options.grouper_freq = "D";
  const auto expected = log_to_features_json(log_to_features(log, options));
  for (const auto& traces : {std::vector<Trace>{Trace{}, nonempty},
                            std::vector<Trace>{nonempty, Trace{}},
                            std::vector<Trace>{Trace{}, nonempty, Trace{}}}) {
    log.traces = traces;
    const auto result = log_to_features(log, options);
    assert(result.temporal_rows[0].values[3] == 0);
    assert(result.temporal_rows[0].values[4] == 0);
    assert(log_to_features_json(result) == expected);
  }
}

static void temporal_calendar_categories_preserve_typed_equality_and_presence() {
  EventLog log;
  LogToFeaturesOptions options;
  options.variant = LogToFeaturesVariant::TEMPORAL;
  options.grouper_freq = "D";
  options.activity_key = "step";
  options.resource_key = "owner";
  const auto event_with = [](EventAttribute value) {
    Event event{"c", "", "1970-01-01T00:00:00Z", ""};
    value.key = "step";
    event.attributes.push_back(value);
    value.key = "owner";
    event.attributes.push_back(value);
    return event;
  };
  Trace first;
  first.case_id = "c";
  first.events = {event_with({"", "true", AttributeKind::kBoolean}),
      event_with({"", "1", AttributeKind::kInt}), event_with({"", "1.0", AttributeKind::kFloat}),
      event_with({"", "1", AttributeKind::kString})};
  log.traces = {first, Trace{"missing", {Event{"missing", "", "1970-01-01T00:00:00Z", ""}}},
      Trace{"empty", {event_with({"", ""})}}};
  auto result = log_to_features(log, options);
  assert((result.temporal_rows[0].values == std::vector<double>{3,3,6,0,0,0,0,0,3,1,2,3,1}));
  EventAttribute native{"", "stale", AttributeKind::kString};
  native.typed_value = std::int64_t{1};
  auto native_event = event_with(native);
  native_event.activity = "projected activity";
  native_event.resource = "projected resource";
  native_event.activity_source_key = "step";
  native_event.resource_source_key = "owner";
  native_event.activity_attribute = native_event.attributes[0];
  native_event.resource_attribute = native_event.attributes[1];
  native_event.activity_state = FirstClassAttributeState::kPresent;
  native_event.resource_state = FirstClassAttributeState::kPresent;
  native_event.attributes.clear();
  log.traces = {Trace{"c", {native_event, event_with({"", "1", AttributeKind::kInt})}}};
  result = log_to_features(log, options);
  assert(result.temporal_rows[0].values[0] == 1 && result.temporal_rows[0].values[1] == 1);
  assert(result.temporal_rows[0].values[8] == 1);
  EventAttribute null{"", "stale", AttributeKind::kString, true};
  log.traces = {Trace{"c", {event_with(null), event_with(null)}}};
  result = log_to_features(log, options);
  assert((result.temporal_rows[0].values == std::vector<double>{0,0,2,0,0,0,0,0,2,0,2,1,0}));
  EventAttribute nonfinite{"", "stale", AttributeKind::kFloat};
  nonfinite.typed_value = std::nan("");
  log.traces = {Trace{"c", {event_with(nonfinite), event_with({"", "NaN", AttributeKind::kFloat}),
      event_with({"", "Infinity", AttributeKind::kFloat}), event_with({"", "-Infinity", AttributeKind::kFloat})}}};
  result = log_to_features(log, options);
  assert(result.temporal_rows[0].values[0] == 2 && result.temporal_rows[0].values[1] == 2);
  assert(result.temporal_rows[0].values[8] == 2);
  log.traces = {Trace{"c", {event_with(make_bytes_attribute("", "same")),
      event_with({"", "same", AttributeKind::kString})}}};
  result = log_to_features(log, options);
  assert(result.temporal_rows[0].values[0] == 2 && result.temporal_rows[0].values[1] == 2);
  EventAttribute tuple{"", "", AttributeKind::kString};
  tuple.container = AttributeContainerKind::kTuple;
  tuple.children = {{"0", "true", AttributeKind::kBoolean}};
  auto equivalent = tuple;
  equivalent.children = {{"different-key", "1", AttributeKind::kInt}};
  log.traces = {Trace{"c", {event_with(tuple), event_with(equivalent)}}};
  result = log_to_features(log, options);
  assert(result.temporal_rows[0].values[0] == 1 && result.temporal_rows[0].values[1] == 1);
  tuple.container = AttributeContainerKind::kFrozenSet;
  equivalent.container = AttributeContainerKind::kFrozenSet;
  log.traces = {Trace{"c", {event_with(tuple), event_with(equivalent)}}};
  assert(log_to_features(log, options).temporal_rows[0].values[0] == 1);
  tuple.container = AttributeContainerKind::kTuple;
  tuple.children = {{"0", "stale", AttributeKind::kString, true}};
  tuple.children[0].container = AttributeContainerKind::kList;
  tuple.children[0].children = {{"invalid", "stale"}};
  equivalent = tuple;
  equivalent.children[0].is_null = false;
  equivalent.children[0].first_class_state = FirstClassAttributeState::kNull;
  equivalent.children[0].value = "different stale text";
  log.traces = {Trace{"c", {event_with(tuple), event_with(equivalent)}}};
  const auto null_members = log_to_features(log, options);
  assert(null_members.temporal_rows[0].values[0] == 1 && null_members.temporal_rows[0].values[1] == 1);
  assert(log.traces[0].events[0].attributes[0].children[0].container == AttributeContainerKind::kList);
  const auto rejects = [&] {
    bool rejected = false;
    try { (void)log_to_features(log, options); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
  };
  for (const auto container : {AttributeContainerKind::kList, AttributeContainerKind::kContainer,
                               AttributeContainerKind::kSet}) {
    tuple.container = container;
    log.traces = {Trace{"c", {event_with(tuple)}}};
    rejects();
  }
  log.traces = {Trace{"c", {event_with({"", "a"})}}};
  log.traces[0].events[0].attributes.pop_back();
  rejects();
  log.traces[0].events[0].attributes = {{"owner", "r"}};
  rejects();
  log.traces = {Trace{}};
  rejects();
  options.grouper_freq.clear();
  assert(log_to_features(log, options).temporal_rows.empty());
  log.traces = {Trace{"c", {Event{"c", "", "1970-01-01T00:00:00Z", ""}}}};
  const auto fixed = log_to_features(log, options);
  assert(fixed.temporal_rows[0].values[0] == 0 && fixed.temporal_rows[0].values[1] == 1);
}

static void temporal_calendar_groups_selected_typed_case_identifiers() {
  EventLog log;
  log.traces = {Trace{"same", {Event{"same", "A", "1970-01-01T00:00:10Z", "r1"}}},
                Trace{"same", {Event{"same", "B", "1970-01-01T00:00:30Z", "r2"}}}};
  LogToFeaturesOptions options;
  options.variant = LogToFeaturesVariant::TEMPORAL;
  options.grouper_freq = "D";
  const std::vector<double> merged{2,2,2,0,0,20,20,0,0,1,2,1,2};
  const std::vector<double> separate{2,2,2,10,10,0,0,0,0,1,1,2,1};
  assert(log_to_features(log, options).temporal_rows[0].values == merged);
  for (auto& trace : log.traces) {
    trace.case_id_attribute = EventAttribute{"concept:name", "stale", AttributeKind::kInt};
    trace.case_id_attribute->typed_value = std::int64_t{42};
    trace.case_id_state = FirstClassAttributeState::kPresent;
  }
  assert(log_to_features(log, options).temporal_rows[0].values == merged);
  options.case_id_key = "concept:name";
  assert(log_to_features(log, options).temporal_rows[0].values == separate);
  for (std::size_t i = 0; i < log.traces.size(); ++i) {
    auto& trace = log.traces[i];
    trace.attributes = {{"group", "shared"}, {"case:id", "double-prefixed"}};
    trace.events[0].attributes = {{"group", std::to_string(i)},
                                  {"case:group", std::to_string(i)},
                                  {"case:concept:name", std::to_string(i)}};
  }
  options.case_id_key = "case:concept:name";
  assert(log_to_features(log, options).temporal_rows[0].values == merged);
  options.case_id_key = "case:group";
  assert(log_to_features(log, options).temporal_rows[0].values == merged);
  options.case_id_key = "group";
  assert(log_to_features(log, options).temporal_rows[0].values == separate);
  options.case_id_key = "case:case:id";
  assert(log_to_features(log, options).temporal_rows[0].values == merged);
  const auto rejects = [&] {
    bool rejected = false;
    try { (void)log_to_features(log, options); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
  };
  options.case_id_key = "case:id";
  rejects();

  options.case_id_key = "case:group";
  log.traces.clear();
  for (auto value : std::vector<EventAttribute>{{"group", "true", AttributeKind::kBoolean},
           {"group", "1", AttributeKind::kInt}, {"group", "1.0", AttributeKind::kFloat},
           {"group", "1", AttributeKind::kString}}) {
    Trace trace{"unused", {Event{"unused", "A", "1970-01-01T00:00:00Z", "r"}}};
    if (value.kind == AttributeKind::kString)
      trace.events[0].timestamp = "1970-01-01T00:00:10Z";
    trace.attributes = {value};
    log.traces.push_back(trace);
  }
  auto result = log_to_features(log, options);
  assert(result.temporal_rows[0].values[11] == 2 && result.temporal_rows[0].values[10] == 2);
  assert(result.temporal_rows[0].values[8] == 2);
  options.case_id_key = "group";
  log.traces = {Trace{"unused", {Event{"unused", "A", "1970-01-01T00:00:00Z", "r"},
                                  Event{"unused", "A", "1970-01-02T00:00:00Z", "r"},
                                  Event{"unused", "A", "1970-01-02T00:00:00Z", "r"},
                                  Event{"unused", "A", "1970-01-02T00:00:00Z", "r"}}}};
  log.traces[0].events[0].attributes = {{"group", "valid"}};
  log.traces[0].events[1].attributes = {{"group", "", AttributeKind::kString, true}};
  log.traces[0].events[3].attributes = {{"group", "NaN", AttributeKind::kFloat}};
  result = log_to_features(log, options);
  assert(result.temporal_rows.size() == 2);
  const auto& no_case = result.temporal_rows[1].values;
  assert(no_case[0] == 1 && no_case[1] == 1 && no_case[2] == 3);
  for (std::size_t i = 3; i < no_case.size(); ++i)
    if (i == 8) assert(std::get<std::string>(no_case[i]).empty());
    else assert(no_case[i] == 0);
  assert(log_to_features_json(result).find("\"\",0,0,0,0]") != std::string::npos);
  auto malformed = result;
  malformed.temporal_rows[1].values[0] = std::string{};
  bool json_rejected = false;
  try { (void)log_to_features_json(malformed); }
  catch (const std::invalid_argument&) { json_rejected = true; }
  assert(json_rejected);
  log.traces[0].events.erase(log.traces[0].events.begin());
  rejects();
  for (auto& event : log.traces[0].events) event.attributes.clear();
  rejects();
}

static void temporal_calendar_string_case_ties_use_lexical_order() {
  const std::vector<Trace> traces{
      Trace{"c", {Event{"c", "A", "1970-01-01T00:00:00Z", "r"}}},
      Trace{"z", {Event{"z", "A", "1970-01-01T00:00:10Z", "r"},
                   Event{"z", "A", "1970-01-01T00:00:20Z", "r"}}},
      Trace{"a", {Event{"a", "A", "1970-01-01T00:00:10Z", "r"},
                   Event{"a", "A", "1970-01-01T00:00:30Z", "r"}}}};
  LogToFeaturesOptions options;
  options.variant = LogToFeaturesVariant::TEMPORAL;
  options.grouper_freq = "10s";
  std::vector<std::size_t> order{0,1,2};
  do {
    EventLog log;
    for (const auto i : order) log.traces.push_back(traces[i]);
    const auto result = log_to_features(log, options);
    assert(result.temporal_rows.size() == 4);
    const std::vector<double> arrivals{0,5,0,10};
    const std::vector<double> finishes{0,15,20,10};
    for (std::size_t i = 0; i < result.temporal_rows.size(); ++i) {
      assert(result.temporal_rows[i].values[3] == arrivals[i]);
      assert(result.temporal_rows[i].values[4] == finishes[i]);
    }
  } while (std::next_permutation(order.begin(), order.end()));
}

struct FrozenExpected { double timestamp, arrival, finish, cases; };
struct FrozenCaseScenario { std::string name; std::vector<std::vector<int>> keys; std::vector<FrozenExpected> rows; };
struct FrozenEndpointScenario { std::vector<std::vector<int>> keys; std::vector<double> ends, rates; };
#include "fixtures/temporal_frozen_case_order.inc"

static void temporal_calendar_frozen_case_tie_order() {
  const auto& scenarios = frozen_case_scenarios;
  assert(scenarios.size() == 67);
  for (const auto& scenario : scenarios) {
    const auto event = [](const std::string& name, const std::vector<int>& members, double time) {
      Event result{name, "A", format_timestamp(1704067200 + time), "r"};
      EventAttribute key{"id", ""};
      key.container = AttributeContainerKind::kFrozenSet;
      for (const auto member : members) key.children.push_back({"", std::to_string(member), AttributeKind::kInt});
      result.attributes = {std::move(key)};
      return result;
    };
    EventLog log;
    log.traces = {Trace{"anchor", {event("anchor", {-1}, 0)}}};
    for (std::size_t i = 0; i < scenario.keys.size(); ++i) {
      const auto id = std::to_string(i);
      log.traces.push_back(Trace{id, {event(id, scenario.keys[i], 10), event(id, scenario.keys[i], 20 + i * 10)}});
    }
    LogToFeaturesOptions options;
    options.variant = LogToFeaturesVariant::TEMPORAL;
    options.grouper_freq = "10s";
    options.case_id_key = "id";
    const auto result = log_to_features(log, options);
    assert(result.temporal_rows.size() == scenario.rows.size());
    for (std::size_t i = 0; i < scenario.rows.size(); ++i) {
      const auto& expected = scenario.rows[i];
      const auto& actual = result.temporal_rows[i];
      if (actual.timestamp != expected.timestamp || actual.values[3] != expected.arrival ||
          actual.values[4] != expected.finish || actual.values[11] != expected.cases)
        throw std::runtime_error("Frozen case tie ordering: " + scenario.name + ", row " + std::to_string(i));
    }
  }
}

static void temporal_calendar_frozen_mixed_endpoints() {
  const auto& scenarios = frozen_endpoint_scenarios;
  assert(scenarios.size() == 29);
  for (std::size_t scenario_index = 0; scenario_index < scenarios.size(); ++scenario_index) {
    const auto& scenario = scenarios[scenario_index];
    // Public wrapper probes cover 20/0, 160/3, 1000/1 and uneven 400/0.
    const auto wrappers = scenario_index == 0 || scenario_index == 14 ||
                          scenario_index == 24 || scenario_index == 25 ? 3 : 1;
    for (int wrapper = 0; wrapper < wrappers; ++wrapper) {
      EventLog log;
      log.traces = {Trace{"c", {}}};
      for (std::size_t i = 0; i < scenario.keys.size(); ++i) {
        Event event{"c", "A", format_timestamp(1704067200 + scenario.ends[i]), "r"};
        EventAttribute key{"id", ""};
        key.container = AttributeContainerKind::kFrozenSet;
        for (const auto member : scenario.keys[i]) key.children.push_back({"", std::to_string(member), AttributeKind::kInt});
        if (wrapper != 0) {
          EventAttribute outer{"id", ""};
          outer.container = AttributeContainerKind::kTuple;
          if (wrapper == 1) outer.children = {{"", "0", AttributeKind::kInt}, key};
          else {
            auto inner = outer;
            inner.children = {key};
            outer.children = {inner};
          }
          key = std::move(outer);
        }
        event.attributes = {key, {"start", format_timestamp(1704067200 + i * 10), AttributeKind::kDate}};
        log.traces[0].events.push_back(event);
      }
      LogToFeaturesOptions options;
      options.variant = LogToFeaturesVariant::TEMPORAL;
      options.grouper_freq = "10s";
      options.case_id_key = "id";
      options.start_timestamp_key = "start";
      const auto result = log_to_features(log, options);
      assert(result.temporal_rows.size() == scenario.keys.size());
      for (std::size_t i = 0; i < scenario.keys.size(); ++i) {
        const auto& row = result.temporal_rows[i];
        assert(row.timestamp == 1704067200 + i * 10);
        if (row.values[3] != scenario.rates[i] || row.values[4] != scenario.rates[i])
          throw std::runtime_error("Frozen mixed endpoints: " + std::to_string(scenario.keys.size()) + ", row " + std::to_string(i));
        assert(row.values[11] == 1 && row.values[5] == 0 && row.values[6] == 0);
      }
      }
  }
}

static void temporal_calendar_nested_object_compatibility() {
  const auto tuple = [](std::vector<EventAttribute> children) {
    EventAttribute value{"selected", ""};
    value.container = AttributeContainerKind::kTuple;
    value.children = std::move(children);
    return value;
  };
  const EventAttribute integer{"", "2", AttributeKind::kInt};
  const EventAttribute text{"", "a"};
  const auto bytes = make_bytes_attribute("", "a");
  const EventAttribute date{"", "2024-01-01T00:00:00Z", AttributeKind::kDate};
  EventAttribute nan{"", "", AttributeKind::kFloat};
  nan.typed_value = std::numeric_limits<double>::quiet_NaN();
  auto frozen = tuple({integer});
  frozen.container = AttributeContainerKind::kFrozenSet;
  auto other_frozen = frozen;
  other_frozen.children = {{"", "3", AttributeKind::kInt}};
  struct Pair { EventAttribute a, b; bool group_error, tie_error; bool missing = false; };
  std::vector<Pair> pairs{
      {frozen, integer, true, true}, {frozen, bytes, true, true}, {frozen, date, true, true},
      {frozen, text, false, true}, {frozen, nan, false, false, true},
      {tuple({frozen}), frozen, true, true}, {tuple({nan}), integer, true, true},
      {tuple({frozen}), tuple({frozen, integer}), false, false},
      {tuple({frozen}), tuple({other_frozen, integer}), false, false},
      {tuple({frozen, integer}), tuple({frozen, text}), false, true},
      {tuple({frozen, integer}), tuple({other_frozen, text}), false, false}};
  EventAttribute null{"", ""};
  null.is_null = true;
  for (const auto& scalar : {integer, bytes, date, text, null, nan}) {
    const bool incompatible = scalar.kind != AttributeKind::kString &&
        !scalar.is_null && !std::holds_alternative<double>(scalar.typed_value);
    pairs.push_back({tuple({{"", "0", AttributeKind::kInt}, frozen}),
                     tuple({{"", "0", AttributeKind::kInt}, scalar}), incompatible, true});
    pairs.push_back({tuple({{"", "0", AttributeKind::kInt}, frozen}),
                     tuple({{"", "1", AttributeKind::kInt}, scalar}), false, false});
  }
  for (const auto& pair : pairs)
    for (const bool reverse : {false, true})
      for (const bool same : {false, true})
        for (const bool resources : {false, true}) {
          auto a = reverse ? pair.b : pair.a, b = reverse ? pair.a : pair.b;
          a.key = b.key = "selected";
          Event first{"c", "A", format_timestamp(resources ? 0 : same ? 10 : 0), "r"};
          Event second{"c", "A", format_timestamp(resources ? same ? 1 : 10 : same ? 10 : 20), "r"};
          first.attributes = {a, {"start", format_timestamp(0), AttributeKind::kDate}};
          second.attributes = {b, {"start", format_timestamp(10), AttributeKind::kDate}};
          EventLog log;
          log.traces = {Trace{"c", {first, second}}};
          LogToFeaturesOptions options;
          options.variant = LogToFeaturesVariant::TEMPORAL;
          options.grouper_freq = "10s";
          if (resources) options.resource_key = "selected";
          else { options.case_id_key = "selected"; options.start_timestamp_key = "start"; }
          bool rejected = false;
          try {
            const auto result = log_to_features(log, options);
            if (resources && same) assert(result.temporal_rows[0].values[0] == (pair.missing ? 1 : 2));
            if (!resources && !pair.missing)
              for (const auto& row : result.temporal_rows) assert(row.values[11] == 1);
          } catch (const std::invalid_argument&) { rejected = true; }
          assert(rejected == (resources ? same && pair.group_error : pair.group_error || (same && pair.tie_error)));
          assert(log.traces[0].events[0].attributes[0] == a);
        }
  for (int depth = 1; depth <= 3; ++depth)
    for (const bool shared : {false, true}) {
      auto independent_nan = nan;
      independent_nan.typed_value = std::numeric_limits<double>::quiet_NaN();
      std::vector<EventAttribute> keys{
          tuple({{"", "0", AttributeKind::kInt}, {"", "0", AttributeKind::kInt}}),
          tuple({nan, {"", "2", AttributeKind::kInt}}),
          tuple({shared ? nan : independent_nan, {"", "1", AttributeKind::kInt}})};
      for (int i = 1; i < depth; ++i)
        for (auto& key : keys) key = tuple({key});
      EventLog log;
      log.traces = {Trace{"c", {}}};
      for (std::size_t i = 0; i < keys.size(); ++i) {
        Event event{"c", "A", format_timestamp(i == 0 ? 0 : 10), "r"};
        event.attributes = {keys[i], {"start", format_timestamp(i * 10), AttributeKind::kDate}};
        log.traces[0].events.push_back(event);
      }
      LogToFeaturesOptions options;
      options.variant = LogToFeaturesVariant::TEMPORAL;
      options.grouper_freq = "10s";
      options.case_id_key = "selected";
      options.start_timestamp_key = "start";
      const auto result = log_to_features(log, options);
      assert(result.temporal_rows[0].values[3] == 0);
      assert(result.temporal_rows[1].values[3] == (shared ? 0 : 10));
      assert(result.temporal_rows[2].values[3] == (shared ? 10 : 0));
    }
}

static void temporal_calendar_frozen_ties_keep_typed_members() {
  EventAttribute shared_nan{"", "", AttributeKind::kFloat};
  shared_nan.typed_value = std::numeric_limits<double>::quiet_NaN();
  EventAttribute smaller{"id", ""};
  smaller.container = AttributeContainerKind::kFrozenSet;
  smaller.children = {{"", "true", AttributeKind::kBoolean}, {"", "1", AttributeKind::kInt}, shared_nan};
  auto larger = smaller;
  larger.children.push_back({"", "2", AttributeKind::kInt});
  auto anchor = smaller;
  anchor.children = {{"", "-1", AttributeKind::kInt}};
  EventLog log;
  log.traces = {
      Trace{"anchor", {Event{"anchor", "A", format_timestamp(0), "r"}}},
      Trace{"large", {Event{"large", "A", format_timestamp(10), "r"}, Event{"large", "A", format_timestamp(20), "r"}}},
      Trace{"small", {Event{"small", "A", format_timestamp(10), "r"}, Event{"small", "A", format_timestamp(30), "r"}}}};
  const std::vector<EventAttribute> keys{anchor, larger, smaller};
  for (std::size_t i = 0; i < keys.size(); ++i)
    for (auto& event : log.traces[i].events) event.attributes = {keys[i]};
  LogToFeaturesOptions options;
  options.variant = LogToFeaturesVariant::TEMPORAL;
  options.grouper_freq = "10s";
  options.case_id_key = "id";
  const auto result = log_to_features(log, options);
  const std::vector<double> arrivals{0,5,0,10};
  for (std::size_t i = 0; i < arrivals.size(); ++i) assert(result.temporal_rows[i].values[3] == arrivals[i]);
  assert(log.traces[2].events[0].attributes[0].children[2].typed_value.scalar_identity() == shared_nan.typed_value.scalar_identity());
}

static void temporal_calendar_tuple_nan_factorization_is_local() {
  const auto nan = [] {
    EventAttribute value{"", "stale", AttributeKind::kDate};
    value.typed_value = std::numeric_limits<double>::quiet_NaN();
    return value;
  };
  const auto wrap = [](EventAttribute child, AttributeContainerKind kind) {
    EventAttribute value{"selected", ""};
    value.container = kind;
    value.children = {std::move(child)};
    return value;
  };
  const auto tuple = [&](EventAttribute child) { return wrap(std::move(child), AttributeContainerKind::kTuple); };
  const auto frozen = [&](EventAttribute child) { return wrap(std::move(child), AttributeContainerKind::kFrozenSet); };
  const auto check = [](EventAttribute first, EventAttribute second, double categories) {
    first.key = second.key = "selected";
    EventLog log;
    Event a{"c", "A", "2024-01-01T00:00:00Z", "r"};
    Event b{"c", "A", "2024-01-01T00:00:20Z", "r"};
    a.attributes = {first};
    b.attributes = {second};
    log.traces = {Trace{"c", {a,b}}};
    for (const auto scope : {0,1,2}) {
      LogToFeaturesOptions options;
      options.variant = LogToFeaturesVariant::TEMPORAL;
      options.grouper_freq = "D";
      if (scope == 0) options.activity_key = "selected";
      if (scope == 1) options.resource_key = "selected";
      if (scope == 2) options.case_id_key = "selected";
      const auto result = log_to_features(log, options);
      const auto& row = result.temporal_rows[0];
      assert(row.values[scope == 0 ? 1 : scope == 1 ? 0 : 11] == categories);
      if (scope == 0) assert(row.values[8] == 2 - categories);
      if (scope == 2) {
        assert(row.values[5] == (categories == 1 ? 20 : 0));
        assert(row.values[6] == (categories == 1 ? 20 : 0));
      }
    }
    assert(log.traces[0].events[0].attributes[0] == first);
    assert(log.traces[0].events[1].attributes[0] == second);
    const auto same_identity = [](auto&& self, const EventAttribute& original,
                                  const EventAttribute& retained) -> void {
      assert(original.typed_value.scalar_identity() == retained.typed_value.scalar_identity());
      for (std::size_t i = 0; i < original.children.size(); ++i)
        self(self, original.children[i], retained.children[i]);
    };
    same_identity(same_identity, first, log.traces[0].events[0].attributes[0]);
    same_identity(same_identity, second, log.traces[0].events[1].attributes[0]);
  };
  const auto first = nan(), second = nan();
  assert(!root_cause_scalar_keys_equal(first, second));
  assert(!root_cause_scalar_keys_equal(tuple(first), tuple(second)));
  check(tuple(first), tuple(second), 1);
  check(tuple(first), tuple(first), 1);
  check(tuple(tuple(first)), tuple(tuple(second)), 1);
  check(tuple(tuple(tuple(first))), tuple(tuple(tuple(second))), 1);
  const EventAttribute lexical{"", "NaN", AttributeKind::kFloat};
  check(tuple(lexical), tuple(second), 1);
  check(frozen(first), frozen(second), 2);
  check(frozen(first), frozen(first), 1);
  check(tuple(frozen(first)), tuple(frozen(second)), 2);
  check(frozen(tuple(first)), frozen(tuple(second)), 2);
  EventAttribute null{"", "stale"};
  null.is_null = true;
  check(tuple(null), tuple(first), 2);
  check(tuple(tuple(null)), tuple(tuple(first)), 2);
  assert(!root_cause_scalar_keys_equal(first, second));
  assert(!root_cause_scalar_keys_equal(tuple(first), tuple(second)));
}

static void temporal_calendar_tuple_case_ordering() {
  const auto integer = [](int value) { return EventAttribute{"", std::to_string(value), AttributeKind::kInt}; };
  const auto tuple = [](std::vector<EventAttribute> children) {
    EventAttribute value{"id", "", AttributeKind::kString};
    value.container = AttributeContainerKind::kTuple;
    value.children = std::move(children);
    return value;
  };
  EventAttribute null{"", "stale"};
  null.is_null = true;
  const auto check = [&](EventAttribute anchor, EventAttribute a, EventAttribute b,
                         const std::vector<double>& arrivals, bool rejects_tie = false,
                         bool distinct = false) {
    std::vector<Trace> traces{
        Trace{"anchor", {Event{"anchor", "X", format_timestamp(0), "r"}}},
        Trace{"a", {Event{"a", "A", format_timestamp(10), "r"}, Event{"a", "A", format_timestamp(20), "r"}}},
        Trace{"b", {Event{"b", "B", format_timestamp(distinct ? 11 : 10), "r"}, Event{"b", "B", format_timestamp(30), "r"}}}};
    const std::vector<EventAttribute> identifiers{anchor, a, b};
    for (std::size_t i = 0; i < traces.size(); ++i)
      for (auto& event : traces[i].events) {
        auto id = identifiers[i];
        id.key = "id";
        event.attributes = {id};
      }
    LogToFeaturesOptions options;
    options.variant = LogToFeaturesVariant::TEMPORAL;
    options.grouper_freq = "10s";
    options.case_id_key = "id";
    std::vector<std::size_t> order{0,1,2};
    do {
      EventLog log;
      for (const auto i : order) log.traces.push_back(traces[i]);
      if (rejects_tie) {
        bool rejected = false;
        try { (void)log_to_features(log, options); }
        catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected);
      } else {
        const auto result = log_to_features(log, options);
        assert(result.temporal_rows.size() == 4);
        const std::vector<double> finishes{0,15,20,10};
        for (std::size_t i = 0; i < 4; ++i) {
          assert(result.temporal_rows[i].values[3] == arrivals[i]);
          assert(result.temporal_rows[i].values[4] == finishes[i]);
        }
      }
    } while (std::next_permutation(order.begin(), order.end()));
  };
  check(tuple({integer(0)}), tuple({integer(2)}), tuple({integer(1)}), {0,5,0,10});
  check(tuple({tuple({integer(0)})}), tuple({tuple({integer(2)})}), tuple({tuple({integer(1)})}), {0,5,0,10});
  check(tuple({null, integer(0)}), tuple({null, integer(1)}), tuple({null, integer(2)}), {0,5,10,0});
  check(tuple({}), tuple({integer(1), integer(2)}), tuple({integer(1)}), {0,5,0,10});
  EventAttribute real{"", "stale", AttributeKind::kFloat};
  real.typed_value = 1.0;
  check(tuple({integer(0)}), tuple({integer(1), integer(2)}), tuple({real, integer(1)}), {0,5,0,10});
  const EventAttribute text{"", "a"};
  check(tuple({tuple({integer(0)})}), tuple({tuple({integer(1)})}), tuple({text}), {}, true);
  check(tuple({tuple({integer(0)})}), tuple({tuple({integer(1)})}), tuple({text}), {0,5.5,10,1}, false, true);
  check(tuple({integer(0)}), tuple({integer(1)}), text, {}, true);
  check(tuple({integer(0)}), tuple({integer(1)}), text, {0,5.5,10,1}, false, true);
  check(tuple({integer(0)}), tuple({tuple({integer(1)})}), tuple({integer(1)}), {}, true);
  check(tuple({integer(0)}), tuple({tuple({integer(1)})}), tuple({integer(1)}), {}, true, true);
  check(tuple({integer(0)}), tuple({integer(2)}), integer(1), {}, true, true);
  check(tuple({integer(0)}), tuple({null}), tuple({integer(1)}), {}, true);
  check(tuple({integer(0)}), tuple({null}), tuple({integer(1)}), {0,5.5,10,1}, false, true);

  const auto bytes = make_bytes_attribute("", "x");
  EventAttribute date{"", "2024-01-01T00:00:00Z", AttributeKind::kDate};
  EventAttribute boolean{"", "true", AttributeKind::kBoolean};
  struct ResourcePair { EventAttribute first, second; bool rejects; };
  const std::vector<ResourcePair> resource_pairs{
      {tuple({integer(1)}), integer(1), true},
      {tuple({integer(1)}), boolean, true},
      {tuple({integer(1)}), date, true},
      {tuple({integer(1)}), bytes, false},
      {tuple({integer(1)}), text, false},
      {tuple({integer(1)}), tuple({bytes}), true},
      {tuple({integer(1)}), tuple({date}), true},
      {tuple({null}), tuple({integer(1)}), false},
      {tuple({integer(1)}), tuple({text}), false},
      {tuple({tuple({integer(1)})}), tuple({bytes}), false},
      {tuple({tuple({integer(1)})}), tuple({null}), false},
      {tuple({tuple({integer(1)})}), tuple({integer(1)}), true},
      {tuple({}), tuple({integer(1)}), false},
      {tuple({integer(1), integer(2)}), tuple({integer(1)}), false},
      {tuple({integer(1), null}), tuple({integer(1), integer(2)}), false}};
  for (const auto& pair : resource_pairs)
    for (const bool separate : {false, true})
      for (const bool reverse : {false, true}) {
        auto first = pair.first, second = pair.second;
        first.key = second.key = "owner";
        Event a{"c", "A", "2024-01-01T00:00:00Z", ""};
        Event b{"c", "A", separate ? "2024-01-02T00:00:00Z" : "2024-01-01T00:00:01Z", ""};
        a.attributes = {reverse ? second : first};
        b.attributes = {reverse ? first : second};
        EventLog log;
        log.traces = {Trace{"c", {a,b}}};
        LogToFeaturesOptions options;
        options.variant = LogToFeaturesVariant::TEMPORAL;
        options.grouper_freq = "D";
        options.resource_key = "owner";
        bool rejected = false;
        try {
          const auto result = log_to_features(log, options);
          assert(result.temporal_rows.size() == (separate ? 2 : 1));
          for (const auto& row : result.temporal_rows) assert(row.values[0] == (separate ? 1 : 2));
        } catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected == (pair.rejects && !separate));
      }
}

static void temporal_calendar_scalar_case_ordering_is_exact() {
  const auto integer = [](const std::string& value) { return EventAttribute{"id", value, AttributeKind::kInt}; };
  const auto real = [](double value) {
    EventAttribute attribute{"id", "stale", AttributeKind::kFloat};
    attribute.typed_value = value;
    return attribute;
  };
  const auto check = [](EventAttribute baseline, EventAttribute smaller, EventAttribute larger) {
    std::vector<Trace> traces{
      Trace{"c", {Event{"c", "A", "1970-01-01T00:00:00Z", "r"}}},
      Trace{"z", {Event{"z", "A", "1970-01-01T00:00:10Z", "r"}, Event{"z", "A", "1970-01-01T00:00:20Z", "r"}}},
      Trace{"a", {Event{"a", "A", "1970-01-01T00:00:10Z", "r"}, Event{"a", "A", "1970-01-01T00:00:30Z", "r"}}}};
    baseline.key = smaller.key = larger.key = "id";
    traces[0].attributes = {baseline};
    traces[1].attributes = {larger};
    traces[2].attributes = {smaller};
    LogToFeaturesOptions options;
    options.variant = LogToFeaturesVariant::TEMPORAL;
    options.grouper_freq = "10s";
    options.case_id_key = "case:id";
    std::vector<std::size_t> order{0,1,2};
    do {
      EventLog log;
      for (const auto i : order) log.traces.push_back(traces[i]);
      const auto result = log_to_features(log, options);
      const std::vector<double> arrivals{0,5,0,10}, finishes{0,15,20,10};
      assert(result.temporal_rows.size() == 4);
      for (std::size_t i = 0; i < 4; ++i) {
        assert(result.temporal_rows[i].values[3] == arrivals[i]);
        assert(result.temporal_rows[i].values[4] == finishes[i]);
      }
    } while (std::next_permutation(order.begin(), order.end()));
  };
  check(integer("-1"), integer("2"), integer("10"));
  check(integer("-1"), integer("9007199254740992"), integer("9007199254740993"));
  check(integer("-100"), integer("-10"), integer("-2"));
  check(integer("-1"), real(.25), integer("1"));
  check(integer("-1"), {"id", "false", AttributeKind::kBoolean}, {"id", "true", AttributeKind::kBoolean});
  check(integer("0"), real(-INFINITY), real(INFINITY));
  check(make_bytes_attribute("id", ""), make_bytes_attribute("id", "\x7f"), make_bytes_attribute("id", "\x80"));
  EventAttribute date{"id", "stale", AttributeKind::kDate};
  date.typed_value = NativeDate{"2000-01-01T00:00:00Z"};
  auto second_date = date, third_date = date;
  second_date.typed_value = NativeDate{"2000-01-02T00:00:00Z"};
  third_date.typed_value = NativeDate{"2000-01-03T00:00:00Z"};
  check(date, second_date, third_date);
  assert(native_scalar_key_compare(integer("9007199254740993"), real(9007199254740992.0)) > 0);
  assert(native_scalar_key_compare(integer("-9007199254740993"), real(-9007199254740992.0)) < 0);
  assert(native_scalar_key_compare(integer("9007199254740992"), real(9007199254740992.0)) == 0);
  assert(native_scalar_key_compare(integer("-9007199254740992"), real(-9007199254740992.0)) == 0);
  assert(native_scalar_key_compare(integer("0"), real(-0.0)) == 0);
  assert(native_scalar_key_compare(integer("-2"), real(-2.5)) > 0);
  assert(native_scalar_key_compare(integer("0"), real(-.25)) > 0);
  assert(native_scalar_key_compare(integer("0"), real(.25)) < 0);
  assert(native_scalar_key_compare(integer("1" + std::string(310, '0')), real(1e308)) > 0);
  assert(native_scalar_key_compare(integer("-1" + std::string(310, '0')), real(-1e308)) < 0);
  assert(native_scalar_key_compare(integer("1" + std::string(310, '0')), real(INFINITY)) < 0);
  assert(native_scalar_key_compare(integer("-1" + std::string(310, '0')), real(-INFINITY)) > 0);
  auto offset_date = date;
  offset_date.typed_value = NativeDate{"2000-01-01T01:00:00+01:00"};
  assert(native_scalar_key_compare(date, offset_date) == 0);
  EventAttribute native_integer{"id", "stale", AttributeKind::kString};
  native_integer.typed_value = std::int64_t{2};
  assert(native_scalar_key_compare(native_integer, integer("10")) < 0);
  LogToFeaturesOptions options;
  options.variant = LogToFeaturesVariant::TEMPORAL;
  options.case_id_key = "case:id";
  EventLog log;
  log.traces = {Trace{"a", {Event{"a", "A", "1970-01-01T00:00:00Z", "r"}}},
                Trace{"b", {Event{"b", "A", "1970-01-01T00:00:10Z", "r"}}}};
  const auto rejects = [&] {
    bool rejected = false;
    try { (void)log_to_features(log, options); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
  };
  for (const auto& nonstring : {integer("1"), EventAttribute{"id", "true", AttributeKind::kBoolean},
                                date, make_bytes_attribute("id", "a")}) {
    log.traces[0].attributes = {nonstring};
    log.traces[1].attributes = {{"id", "string"}};
    log.traces[1].events[0].timestamp = "1970-01-01T00:00:10Z";
    assert(log_to_features(log, options).temporal_rows[0].values[11] == 2);
    log.traces[1].events[0].timestamp = "1970-01-01T00:00:00Z";
    rejects();
    log.traces[1].events[0].timestamp = "1970-01-01T00:00:10Z";
    log.traces[0].events.push_back(Event{"a", "A", "1970-01-01T00:00:10Z", "r"});
    rejects();
    log.traces[0].events.pop_back();
  }
  log.traces[1].events[0].timestamp = "1970-01-01T00:00:10Z";
  for (const auto& pair : std::vector<std::pair<EventAttribute, EventAttribute>>{
           {integer("1"), make_bytes_attribute("id", "a")}, {date, integer("1")},
           {date, make_bytes_attribute("id", "a")}}) {
    log.traces[0].attributes = {pair.first};
    log.traces[1].attributes = {pair.second};
    rejects();
  }
  options.case_id_key.reset();
  options.resource_key = "id";
  options.grouper_freq = "D";
  for (const auto& pair : std::vector<std::pair<EventAttribute, EventAttribute>>{
           {integer("1"), make_bytes_attribute("id", "a")}, {date, integer("1")},
           {date, make_bytes_attribute("id", "a")}}) {
    log.traces[0].events[0].attributes = {pair.first};
    log.traces[1].events[0].attributes = {pair.second};
    log.traces[1].events[0].timestamp = "1970-01-01T00:00:10Z";
    rejects();
    log.traces[1].events[0].timestamp = "1970-01-02T00:00:10Z";
    assert(log_to_features(log, options).temporal_rows.size() == 2);
  }
  log.traces[0].events[0].attributes = {integer("1")};
  log.traces[1].events[0].attributes = {{"id", "string"}};
  log.traces[1].events[0].timestamp = "1970-01-01T00:00:10Z";
  assert(log_to_features(log, options).temporal_rows[0].values[0] == 2);
}

static void temporal_calendar_numeric_columns_promote_before_grouping() {
  const EventAttribute high{"value", "9007199254740993", AttributeKind::kInt};
  const EventAttribute adjacent{"value", "9007199254740992", AttributeKind::kInt};
  const EventAttribute floating{"value", "9007199254740992.0", AttributeKind::kFloat};
  const auto make_event = [](EventAttribute value, const std::string& day = "01") {
    Event event{"c", "", "1970-01-" + day + "T00:00:00Z", ""};
    event.attributes = {value, EventAttribute{"unrelated", "true", AttributeKind::kBoolean}};
    return event;
  };
  EventLog log;
  LogToFeaturesOptions options;
  options.variant = LogToFeaturesVariant::TEMPORAL;
  options.grouper_freq = "D";
  options.activity_key = options.resource_key = "value";
  log.traces = {Trace{"c", {make_event(high), make_event(floating)}}};
  auto result = log_to_features(log, options);
  assert(result.temporal_rows[0].values[0] == 1 && result.temporal_rows[0].values[1] == 1);
  assert(result.temporal_rows[0].values[8] == 1);
  log.traces[0].events.push_back(make_event({"value", "", AttributeKind::kString, true}));
  assert(log_to_features(log, options).temporal_rows[0].values[8] == 2);
  log.traces[0].events.back().attributes.clear();
  assert(log_to_features(log, options).temporal_rows[0].values[8] == 2);
  for (const auto& blocker : {EventAttribute{"value", "true", AttributeKind::kBoolean},
                              EventAttribute{"value", "s", AttributeKind::kString},
                              EventAttribute{"value", "", AttributeKind::kString}}) {
    log.traces[0].events.back() = make_event(blocker);
    result = log_to_features(log, options);
    assert(result.temporal_rows[0].values[0] == 3 && result.temporal_rows[0].values[1] == 3);
  }
  log.traces[0].events = {make_event(high), make_event(adjacent)};
  assert(log_to_features(log, options).temporal_rows[0].values[0] == 2);
  log.traces[0].events.push_back(make_event(floating, "02"));
  result = log_to_features(log, options);
  assert(result.temporal_rows.size() == 2 && result.temporal_rows[0].values[0] == 1);
  log.traces[0].events.back() = make_event({"value", "true", AttributeKind::kBoolean}, "02");
  assert(log_to_features(log, options).temporal_rows[0].values[0] == 2);
  auto native_high = high;
  native_high.kind = AttributeKind::kString;
  native_high.value = "stale";
  native_high.typed_value = std::int64_t{9007199254740993};
  auto native_float = floating;
  native_float.kind = AttributeKind::kString;
  native_float.value = "stale";
  native_float.typed_value = 9007199254740992.0;
  log.traces[0].events = {make_event(native_high), make_event(native_float)};
  assert(log_to_features(log, options).temporal_rows[0].values[0] == 1);
  assert(log.traces[0].events[0].attributes[0] == native_high);
  assert(log.traces[0].events[1].attributes[0] == native_float);
  options.case_id_key = "value";
  result = log_to_features(log, options);
  assert(result.temporal_rows[0].values[11] == 1);
  log.traces[0].events = {make_event(high), make_event(adjacent)};
  assert(log_to_features(log, options).temporal_rows[0].values[11] == 2);
  log.traces[0].events.push_back(make_event({"value", "", AttributeKind::kString, true}));
  result = log_to_features(log, options);
  assert(result.temporal_rows[0].values[11] == 1 && result.temporal_rows[0].values[10] == 2);
  log.traces[0].events = {make_event(high), make_event(floating), make_event({"value", "true", AttributeKind::kBoolean})};
  assert(log_to_features(log, options).temporal_rows[0].values[11] == 3);
  options.case_id_key.reset();
  log.traces = {Trace{"a", {make_event(native_high)}}, Trace{"b", {make_event(native_float)}}};
  log.traces[0].case_id_attribute = native_high;
  log.traces[1].case_id_attribute = native_float;
  for (auto& trace : log.traces) trace.case_id_state = FirstClassAttributeState::kPresent;
  assert(log_to_features(log, options).temporal_rows[0].values[11] == 1);
  options.grouper_freq.clear();
  log.traces = {Trace{"c", {make_event(high), make_event(floating)}}};
  assert(log_to_features(log, options).temporal_rows[0].values[0] == 2);
  options.grouper_freq = "D";
  options.case_id_key = "value";
  const auto check_bounds = [&](const std::vector<EventAttribute>& values, double groups) {
    log.traces = {Trace{"c", {}}};
    for (const auto& value : values) log.traces[0].events.push_back(make_event(value));
    const auto actual = log_to_features(log, options);
    assert(actual.temporal_rows[0].values[0] == groups);
    assert(actual.temporal_rows[0].values[1] == groups);
    assert(actual.temporal_rows[0].values[11] == groups);
  };
  const auto integer = [](const std::string& value) { return EventAttribute{"value", value, AttributeKind::kInt}; };
  const auto real = [](const std::string& value) { return EventAttribute{"value", value, AttributeKind::kFloat}; };
  const EventAttribute null{"value", "", AttributeKind::kString, true};
  check_bounds({integer("18446744073709551617"), real("18446744073709551616")}, 2);
  check_bounds({integer("18446744073709551617"), integer("18446744073709551616"), null}, 2);
  check_bounds({integer("18446744073709551615"), real("18446744073709551616")}, 1);
  check_bounds({integer("9223372036854775809"), integer("9223372036854775808"), null}, 1);
  check_bounds({integer("9223372036854775809"), integer("9223372036854775808"), real("9223372036854775808")}, 1);
  check_bounds({integer("9223372036854775809"), real("9223372036854775808"), real("-1.0")}, 2);
  check_bounds({integer("9223372036854775809"), integer("9223372036854775808"), integer("-1"), null}, 3);
  check_bounds({integer("9223372036854775809"), real("9223372036854775808"), integer("-1")}, 3);
  check_bounds({integer("-9223372036854775809"), real("-9223372036854775808")}, 2);
  check_bounds({integer("-9223372036854775808"), real("-9223372036854775808")}, 1);
  check_bounds({high, adjacent, null}, 1);
  native_high.kind = AttributeKind::kDate;
  native_float.kind = AttributeKind::kDate;
  check_bounds({native_high, native_float}, 1);
  check_bounds({native_high, adjacent, null}, 1);
  assert(log.traces[0].events[0].attributes[0] == native_high);
  check_bounds({native_high, native_float, null}, 1);
  assert(log.traces[0].events[1].attributes[0] == native_float);
}

static void temporal_calendar_categories_use_projected_trace_columns() {
  EventLog log;
  log.traces = {Trace{"a", {Event{"a", "A", "1970-01-01T00:00:00Z", "r"},
                              Event{"a", "B", "1970-01-01T00:00:00Z", "s"}}},
                Trace{"b", {Event{"b", "C", "1970-01-01T00:00:00Z", "t"}}}};
  for (auto& trace : log.traces) {
    trace.attributes = {{"group", "shared"}, {"case:group", "double"}};
    for (auto& event : trace.events) event.attributes = {{"case:group", event.activity}};
  }
  LogToFeaturesOptions options;
  options.variant = LogToFeaturesVariant::TEMPORAL;
  options.grouper_freq = "D";
  options.activity_key = options.resource_key = "case:group";
  auto result = log_to_features(log, options);
  assert(result.temporal_rows[0].values[0] == 1 && result.temporal_rows[0].values[1] == 1);
  assert(result.temporal_rows[0].values[8] == 1);
  log.traces[0].attributes[0].is_null = true;
  result = log_to_features(log, options);
  assert(result.temporal_rows[0].values[0] == 1 && result.temporal_rows[0].values[1] == 1);
  assert(result.temporal_rows[0].values[8] == 2);
  log.traces[0].attributes.erase(log.traces[0].attributes.begin());
  result = log_to_features(log, options);
  assert(result.temporal_rows[0].values[0] == 3 && result.temporal_rows[0].values[1] == 3);
  options.activity_key = options.resource_key = "case:case:group";
  assert(log_to_features(log, options).temporal_rows[0].values[0] == 1);
  options.activity_key = options.resource_key = "case:group";
  EventAttribute high{"group", "stale", AttributeKind::kString};
  high.typed_value = std::int64_t{9007199254740993};
  EventAttribute floating{"group", "stale", AttributeKind::kString};
  floating.typed_value = 9007199254740992.0;
  log.traces[0].attributes = {high};
  log.traces[1].attributes = {floating};
  result = log_to_features(log, options);
  assert(result.temporal_rows[0].values[0] == 1 && result.temporal_rows[0].values[1] == 1);
  assert(log.traces[0].attributes[0] == high && log.traces[1].attributes[0] == floating);
  options.activity_key = options.resource_key = "case:identifier";
  for (auto& trace : log.traces) {
    trace.case_id_source_key = "identifier";
    trace.case_id_state = FirstClassAttributeState::kPresent;
    trace.case_id_attribute = EventAttribute{"identifier", "stale", AttributeKind::kInt};
    trace.case_id_attribute->typed_value = std::int64_t{7};
  }
  assert(log_to_features(log, options).temporal_rows[0].values[0] == 1);
  options.activity_key = options.resource_key = "time:timestamp";
  for (auto& trace : log.traces) for (auto& event : trace.events) event.attributes.clear();
  assert(log_to_features(log, options).temporal_rows[0].values[0] == 1);
  options.activity_key = options.resource_key = "case:group";
  options.grouper_freq.clear();
  result = log_to_features(log, options);
  assert(result.temporal_rows[0].values[0] == 0 && result.temporal_rows[0].values[1] == 1);
}

static void temporal_calendar_timestamps_use_projected_native_dates() {
  EventLog log;
  log.traces = {Trace{"c", {Event{"c", "A", "1970-01-01T00:00:10Z", "r"}}},
                Trace{"c", {Event{"c", "B", "1970-01-01T00:00:40Z", "r"}}}};
  EventAttribute when{"when", "stale", AttributeKind::kString};
  when.typed_value = NativeDate{"1970-01-02T00:00:10Z"};
  log.traces[0].attributes = {when};
  log.traces[0].events[0].attributes = {{"case:when", "1970-01-03T00:00:10Z", AttributeKind::kDate}};
  log.traces[1].events[0].attributes = {{"case:when", "1970-01-02T00:00:30Z", AttributeKind::kDate}};
  LogToFeaturesOptions options;
  options.variant = LogToFeaturesVariant::TEMPORAL;
  options.grouper_freq = "D";
  options.timestamp_key = "case:when";
  auto result = log_to_features(log, options);
  assert(result.temporal_rows.size() == 1 && result.temporal_rows[0].timestamp == 86400);
  assert(result.temporal_rows[0].values[5] == 20 && result.temporal_rows[0].values[2] == 2);
  assert(log.traces[0].attributes[0] == when);
  const auto rejects = [&] {
    bool rejected = false;
    try { (void)log_to_features(log, options); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
  };
  log.traces[0].attributes[0].is_null = true;
  result = log_to_features(log, options);
  assert(result.temporal_rows[0].values[2] == 1 && result.temporal_rows[0].values[5] == 0);
  log.traces[0].case_id = "different";
  rejects();
  log.traces[0].case_id = "c";
  log.traces[0].attributes[0] = {"when", "1970-01-02T00:00:10Z", AttributeKind::kString};
  rejects();
  log.traces[0].attributes[0] = when;
  options.timestamp_key = "time:timestamp";
  options.start_timestamp_key = "case:when";
  log.traces[0].attributes[0].is_null = true;
  result = log_to_features(log, options);
  assert(result.temporal_rows.size() == 1 && result.temporal_rows[0].timestamp == 86400);
  assert(result.temporal_rows[0].values[2] == 1 && result.temporal_rows[0].values[5] == 30);
  assert(result.temporal_rows[0].values[6] == 30);
  log.traces[0].attributes.clear();
  log.traces[0].events[0].attributes.clear();
  assert(log_to_features(log, options).temporal_rows[0].values[2] == 1);
  log.traces[1].events[0].attributes.clear();
  rejects();
  options.start_timestamp_key.clear();
  options.timestamp_key = "finished";
  for (auto& trace : log.traces) for (auto& event : trace.events) {
    event.timestamp_source_key = "finished";
    event.timestamp_attribute = EventAttribute{"finished", "stale", AttributeKind::kDate};
    event.timestamp_attribute->typed_value = NativeDate{event.timestamp};
    event.timestamp_state = FirstClassAttributeState::kPresent;
    event.timestamp = "stale";
  }
  result = log_to_features(log, options);
  assert(result.temporal_rows[0].values[5] == 30);
  assert(log.traces[0].events[0].timestamp == "stale");
  log.traces[0].events[0].timestamp_attribute->kind = AttributeKind::kBytes;
  rejects();
  log.traces[0].events[0].timestamp_attribute->kind = AttributeKind::kDate;
  log.traces[0].events[0].timestamp_state = FirstClassAttributeState::kNull;
  result = log_to_features(log, options);
  assert(result.temporal_rows[0].values[2] == 1 && result.temporal_rows[0].values[5] == 0);
  log.traces[0].case_id = "different";
  rejects();
}

static void temporal_calendar_missing_completions_are_validated_per_case() {
  EventLog log;
  auto valid = timed_event("c", "A", "1970-01-01T00:00:00Z", "1970-01-01T00:00:00Z", 0);
  auto missing = timed_event("c", "A", "1970-01-01T00:00:00Z", "", 1);
  log.traces = {Trace{"c", {valid, missing}}};
  LogToFeaturesOptions options;
  options.variant = LogToFeaturesVariant::TEMPORAL;
  options.grouper_freq = "D";
  options.start_timestamp_key = "start_time";
  auto result = log_to_features(log, options);
  assert(result.temporal_rows[0].values[2] == 2 && result.temporal_rows[0].values[5] == 0);
  assert(result.temporal_rows[0].values[8] == 1);
  log.traces[0].events[1].timestamp_state = FirstClassAttributeState::kNull;
  assert(log_to_features(log, options).temporal_rows[0].values == result.temporal_rows[0].values);
  log.traces = {Trace{"c", {valid}}, Trace{"other", {missing}}};
  bool rejected = false;
  try { (void)log_to_features(log, options); }
  catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected);
  log.traces[1].case_id_state = FirstClassAttributeState::kNull;
  result = log_to_features(log, options);
  assert(result.temporal_rows[0].values[2] == 2 && result.temporal_rows[0].values[11] == 1);
  assert(result.temporal_rows[0].values[8] == 0);
  options.start_timestamp_key.clear();
  result = log_to_features(log, options);
  assert(result.temporal_rows[0].values[2] == 1);
}

static void temporal_calendar_metric_destinations_observe_write_order() {
  EventLog log;
  log.traces = {Trace{"c", {
      timed_event("c", "A", "1970-01-01T00:00:00Z", "1970-01-01T00:00:10Z", 0),
      timed_event("c", "B", "1970-01-01T00:00:20Z", "1970-01-01T00:00:40Z", 1)}}};
  log.traces[0].events[1].resource = "r2";
  LogToFeaturesOptions base;
  base.variant = LogToFeaturesVariant::TEMPORAL;
  base.grouper_freq = "D";
  base.start_timestamp_key = "start_time";
  const auto baseline = log_to_features(log, base);
  for (auto& event : log.traces[0].events)
    for (const auto& key : {"@@arrival_rate", "@@finish_rate", "@@service_time", "@@sojourn_time", "@@waiting_time", "@@diff_start_end"})
      event.attributes.push_back({key, "999", AttributeKind::kInt});
  const auto original_attributes = log.traces[0].events[0].attributes;
  assert(log_to_features(log, base).temporal_rows[0].values == baseline.temporal_rows[0].values);
  auto options = base;
  options.arrival_rate = "arrival";
  options.finish_rate = "finish";
  options.service_time = "";
  options.sojourn_time = "sojourn";
  options.waiting_time = "waiting";
  options.diff_start_end = "difference";
  assert(log_to_features(log, options).temporal_rows[0].values == baseline.temporal_rows[0].values);
  options = base;
  options.activity_key = options.resource_key = "@@service_time";
  auto result = log_to_features(log, options);
  assert(result.temporal_rows[0].values[0] == 1 && result.temporal_rows[0].values[1] == 1);
  assert(result.temporal_rows[0].values[8] == 1);
  options = base;
  options.service_time = "org:resource";
  assert(log_to_features(log, options).temporal_rows[0].values[0] == 1);
  options = base;
  options.waiting_time = "concept:name";
  result = log_to_features(log, options);
  assert(result.temporal_rows[0].values[1] == 1 && result.temporal_rows[0].values[8] == 1);
  options = base;
  options.service_time = options.sojourn_time = "shared";
  result = log_to_features(log, options);
  assert(result.temporal_rows[0].values[5] == 0 && result.temporal_rows[0].values[6] == 30 && result.temporal_rows[0].values[7] == 30);
  options = base;
  options.service_time = options.waiting_time = "shared";
  result = log_to_features(log, options);
  assert(result.temporal_rows[0].values[5] == 30 && result.temporal_rows[0].values[6] == 30 && result.temporal_rows[0].values[7] == 30);
  options = base;
  options.diff_start_end = options.service_time = "shared";
  assert(log_to_features(log, options).temporal_rows[0].values == baseline.temporal_rows[0].values);
  options = base;
  options.diff_start_end = "case:concept:name";
  assert(log_to_features(log, options).temporal_rows[0].values == baseline.temporal_rows[0].values);
  const auto rejects = [&] {
    bool rejected = false;
    try { (void)log_to_features(log, options); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
  };
  for (auto field : {&LogToFeaturesOptions::arrival_rate, &LogToFeaturesOptions::finish_rate,
                     &LogToFeaturesOptions::service_time, &LogToFeaturesOptions::sojourn_time,
                     &LogToFeaturesOptions::waiting_time}) {
    options = base;
    options.*field = "case:concept:name";
    rejects();
    log.traces[0].case_id_attribute = EventAttribute{"concept:name", "stale", AttributeKind::kInt};
    log.traces[0].case_id_attribute->typed_value = std::int64_t{0};
    log.traces[0].case_id_state = FirstClassAttributeState::kPresent;
    rejects();
    log.traces[0].case_id_attribute.reset();
  }
  for (auto field : {&LogToFeaturesOptions::finish_rate, &LogToFeaturesOptions::service_time,
                     &LogToFeaturesOptions::diff_start_end}) {
    options = base;
    options.*field = "time:timestamp";
    rejects();
  }
  options = base;
  options.service_time = "start_time";
  rejects();
  options = base;
  options.waiting_time = "time:timestamp";
  assert(log_to_features(log, options).temporal_rows[0].values == baseline.temporal_rows[0].values);
  assert(log.traces[0].events[0].attributes == original_attributes);
  options = base;
  options.arrival_rate = options.finish_rate = "rates";
  log.traces = {
      Trace{"a", {timed_event("a", "A", "1970-01-01T00:00:00Z", "1970-01-01T00:00:00Z", 0),
                  timed_event("a", "B", "1970-01-01T00:00:20Z", "1970-01-01T00:00:20Z", 1)}},
      Trace{"b", {timed_event("b", "A", "1970-01-01T00:00:40Z", "1970-01-01T00:00:40Z", 0),
                  timed_event("b", "B", "1970-01-01T00:01:30Z", "1970-01-01T00:01:30Z", 1)}}};
  result = log_to_features(log, options);
  assert(result.temporal_rows[0].values[3] == 35 && result.temporal_rows[0].values[4] == 35);
  for (const bool reverse : {false, true}) {
    log.traces = {
        Trace{"c1", {Event{"c1", "A", "1970-01-01T00:00:10Z", "r"},
                     Event{"c1", "A", "1970-01-01T00:00:40Z", "r"}}},
        Trace{"c2", {Event{"c2", "A", "1970-01-01T00:01:00Z", "r"},
                     Event{"c2", "A", "1970-01-01T00:01:50Z", "r"}}}};
    if (reverse) std::reverse(log.traces.begin(), log.traces.end());
    options = base;
    options.start_timestamp_key.clear();
    options.diff_start_end = "case:concept:name";
    result = log_to_features(log, options);
    assert((result.temporal_rows[0].values == std::vector<double>{1,1,4,0,0,100,100,0,3,1,4,1,1}));
    for (auto& trace : log.traces) {
      const std::vector<std::string> starts = trace.case_id == "c1"
          ? std::vector<std::string>{"1970-01-01T00:03:20Z", "1970-01-01T00:03:30Z"}
          : std::vector<std::string>{"1970-01-01T00:00:00Z", "1970-01-01T00:00:01Z"};
      for (std::size_t i = 0; i < trace.events.size(); ++i)
        trace.events[i].attributes = {{"start_time", starts[i], AttributeKind::kDate}};
    }
    options.start_timestamp_key = "start_time";
    result = log_to_features(log, options);
    assert(result.temporal_rows[0].values[3] == 50 && result.temporal_rows[0].values[4] == 70);
    options.start_timestamp_key.clear();
    options.diff_start_end = "@@arrival_rate";
    result = log_to_features(log, options);
    assert(result.temporal_rows[0].values[3] == 0 && result.temporal_rows[0].values[4] == 35);
    options.diff_start_end = "@@finish_rate";
    result = log_to_features(log, options);
    assert(result.temporal_rows[0].values[3] == 25 && result.temporal_rows[0].values[4] == 0);
    options.diff_start_end = "@@diff_start_end";
    options.arrival_rate = "@@diff_start_end";
    result = log_to_features(log, options);
    assert(result.temporal_rows[0].values[3] == 0 && result.temporal_rows[0].values[4] == 35);
  }
}

static void temporal_calendar_metrics_select_first_valid_per_column() {
  EventLog log;
  log.traces = {Trace{"c", {
      Event{"c", "A", "", "r"}, Event{"c", "A", "", "r"}}}};
  log.traces[0].events[0].attributes = {{"start", "1970-01-01T00:00:00Z", AttributeKind::kDate}};
  log.traces[0].events[1].attributes = {{"start", "1970-01-01T00:00:20Z", AttributeKind::kDate},
                                       {"stamp", "1970-01-01T00:00:20Z", AttributeKind::kDate}};
  const auto original = log.traces[0].events[0].attributes;
  LogToFeaturesOptions options;
  options.variant = LogToFeaturesVariant::TEMPORAL;
  options.timestamp_key = "stamp";
  options.start_timestamp_key = "start";
  for (const auto& frequency : {"D", "10s"}) {
    options.grouper_freq = frequency;
    for (const auto& destination : {"@@arrival_rate", "@@finish_rate"}) {
      options.diff_start_end = destination;
      const auto result = log_to_features(log, options);
      assert(result.temporal_rows.size() == (options.grouper_freq == "D" ? 1 : 3));
      for (const auto& row : result.temporal_rows) {
        assert(row.values[3] == 0 && row.values[4] == 0);
        assert(row.values[5] == 0 && row.values[6] == 0 && row.values[7] == 0);
      }
    }
  }
  assert(log.traces[0].events[0].attributes == original);

  log.traces[0].events = {
      Event{"c", "A", "", "r"}, Event{"c", "A", "", "r"}, Event{"c", "A", "", "r"}};
  for (std::size_t i = 0; i < 3; ++i) {
    auto& event = log.traces[0].events[i];
    event.attributes = {{"start", format_timestamp(static_cast<double>(i * 10)), AttributeKind::kDate},
                        {"stamp", format_timestamp(i == 0 ? 0 : i == 1 ? 60 : 10), AttributeKind::kDate}};
    if (i != 0) event.attributes.push_back({"id", i == 1 ? "c2" : "c1"});
  }
  options.case_id_key = "id";
  options.diff_start_end = "id";
  options.grouper_freq = "D";
  auto result = log_to_features(log, options);
  assert((result.temporal_rows[0].values == std::vector<double>{1,1,3,50,50,60,60,0,2,1,3,1,1}));
  options.grouper_freq = "10s";
  result = log_to_features(log, options);
  assert(result.temporal_rows.size() == 3);
  for (std::size_t i = 0; i < 3; ++i) {
    assert(result.temporal_rows[i].values[3] == (i == 1 ? 50 : 0));
    assert(result.temporal_rows[i].values[4] == (i == 1 ? 50 : 0));
    assert(result.temporal_rows[i].values[5] == 60 && result.temporal_rows[i].values[6] == 60);
  }
  assert(log.traces[0].events[0].attributes.size() == 2);
  assert(log.traces[0].events[1].attributes.back().value == "c2");

  options.grouper_freq.clear();
  options.case_id_key.reset();
  options.window_seconds = 100;
  options.diff_start_end = "@@arrival_rate";
  log.traces = {Trace{"c", {timed_event("c", "A", "1970-01-01T00:00:00Z", "1970-01-01T00:00:10Z", 0),
                            timed_event("c", "B", "1970-01-01T00:00:20Z", "1970-01-01T00:00:40Z", 1)}}};
  options.timestamp_key = "time:timestamp";
  options.start_timestamp_key = "start_time";
  result = log_to_features(log, options);
  const auto feature = [&](const std::string& name) {
    const auto position = std::find(result.feature_names.begin(), result.feature_names.end(), name);
    assert(position != result.feature_names.end());
    return result.temporal_rows[0].values[static_cast<std::size_t>(position - result.feature_names.begin())];
  };
  assert(feature("average_service_time") == 30 && feature("average_sojourn_time") == 40);
}

static void temporal_calendar_metric_destination_matrix() {
  struct ExpectedRow { double timestamp; std::vector<double> values; };
  struct DestinationCase {
    std::string name;
    std::vector<std::pair<std::string LogToFeaturesOptions::*, std::string>> destinations;
    std::vector<ExpectedRow> rows;
  };
  const std::vector<DestinationCase> scenarios{
#include "fixtures/temporal_metric_destination_cases.inc"
  };
  assert(scenarios.size() == 128);
  const auto event = [](const std::string& id, const std::string& activity,
                        const std::string& resource, double start, double finish) {
    constexpr double origin = 1704067200;
    Event result{id, activity, format_timestamp(origin + finish), resource};
    result.attributes = {{"start", format_timestamp(origin + start), AttributeKind::kDate}};
    return result;
  };
  EventLog log;
  log.traces = {
      Trace{"c1", {event("c1", "A", "r1", 0, 10), event("c1", "B", "r1", 20, 40)}},
      Trace{"c2", {event("c2", "A", "r2", 30, 50), event("c2", "C", "r3", 60, 70),
                   event("c2", "B", "r2", 80, 110)}},
      Trace{"c3", {event("c3", "D", "r4", 15, 25)}}};
  const std::vector<std::string> names{
      "unique_resources", "unique_activities", "num_events", "average_arrival_rate",
      "average_finish_rate", "average_waiting_time", "average_sojourn_time", "average_service_time",
      "total_number_of_reworked_activities", "avg_cases_per_resource", "avg_events_per_case",
      "number_of_cases", "avg_resources_per_case"};
  for (const auto& scenario : scenarios) {
    LogToFeaturesOptions options;
    options.variant = LogToFeaturesVariant::TEMPORAL;
    options.grouper_freq = scenario.name.substr(0, scenario.name.find('|'));
    options.start_timestamp_key = "start";
    for (const auto& [field, destination] : scenario.destinations) options.*field = destination;
    const auto result = log_to_features(log, options);
    const auto require = [&](bool condition, const std::string& detail) {
      if (!condition) throw std::runtime_error("Temporal destination matrix " + scenario.name + ": " + detail);
    };
    require(result.feature_names == names, "canonical schema");
    require(result.temporal_rows.size() == scenario.rows.size(), "row count");
    for (std::size_t i = 0; i < scenario.rows.size(); ++i) {
      const auto& actual = result.temporal_rows[i];
      const auto& expected = scenario.rows[i];
      require(std::abs(actual.timestamp - expected.timestamp) <= 1e-9, "timestamp " + std::to_string(i));
      require(actual.values.size() == 13 && expected.values.size() == 13, "cell count");
      for (std::size_t j = 0; j < 13; ++j)
        require(std::holds_alternative<double>(actual.values[j]) &&
                    std::abs(std::get<double>(actual.values[j]) - expected.values[j]) <= 1e-9,
                "row " + std::to_string(i) + ", " + names[j]);
    }
  }
}

static void temporal_calendar_frequency_uses_period_boundaries() {
  EventLog log;
  Trace first;
  first.case_id = "c1";
  first.events = {timed_event("c1", "A", "2024-01-01T12:00:00Z",
                               "2024-01-01T12:01:00Z", 0)};
  log.traces.push_back(first);
  Trace second;
  second.case_id = "c2";
  second.events = {timed_event("c2", "B", "2024-01-08T12:00:00Z",
                                "2024-01-08T12:01:00Z", 0)};
  log.traces.push_back(second);

  LogToFeaturesOptions options;
  options.variant = LogToFeaturesVariant::TEMPORAL;
  options.start_timestamp_key = "start_time";
  options.grouper_freq = "W";
  options.feature_names = {"num_events", "number_of_cases"};
  const auto weekly = log_to_features(log, options);
  assert(weekly.temporal_rows.size() == 2);
  const auto jan7 = parse_timestamp("2024-01-07T00:00:00Z");
  const auto jan14 = parse_timestamp("2024-01-14T00:00:00Z");
  assert(jan7 && jan14);
  assert(weekly.temporal_rows[0].timestamp == *jan7);
  assert(weekly.temporal_rows[1].timestamp == *jan14);
  assert(weekly.temporal_rows[0].values[2] == 1.0 &&
         weekly.temporal_rows[1].values[11] == 1.0);

  options.grouper_freq = "MS";
  const auto month_start = log_to_features(log, options);
  assert(month_start.temporal_rows.size() == 1);
  const auto jan1 = parse_timestamp("2024-01-01T00:00:00Z");
  assert(jan1 && month_start.temporal_rows[0].timestamp == *jan1);

  options.grouper_freq = "M";
  const auto month_end = log_to_features(log, options);
  const auto jan31 = parse_timestamp("2024-01-31T00:00:00Z");
  assert(jan31 && month_end.temporal_rows.size() == 1 &&
         month_end.temporal_rows[0].timestamp == *jan31);

  options.grouper_freq = "W-MON";
  const auto monday = log_to_features(log, options);
  const auto first_monday = parse_timestamp("2024-01-01T00:00:00Z");
  const auto second_monday = parse_timestamp("2024-01-08T00:00:00Z");
  assert(first_monday && second_monday && monday.temporal_rows.size() == 2 &&
         monday.temporal_rows[0].timestamp == *first_monday &&
         monday.temporal_rows[1].timestamp == *second_monday);

  options.grouper_freq = "unsupported";
  bool threw = false;
  try {
    (void)log_to_features(log, options);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

static void explicit_trace_numeric_attributes_retain_native_values() {
  LogToFeaturesOptions options;
  options.variant = LogToFeaturesVariant::TRACE_BASED;
  options.event_numeric_attributes = {"value"};
  options.case_numeric_attributes = {"case_value"};
  options.feature_names = {"event:value", "trace:case_value"};
  options.enable_succ_def_representation = false;
  auto log = fixture();
  log.traces[0].events.resize(1);
  const auto check = [&](EventAttribute attribute, double expected) {
    attribute.key = "value";
    log.traces[0].events[0].attributes = {attribute};
    attribute.key = "case_value";
    log.traces[0].attributes = {attribute};
    const auto result = log_to_features(log, options);
    const auto json = log_to_features_json(result);
    if (std::isnan(expected)) assert(json.find("\"NaN\"") != std::string::npos);
    if (std::isinf(expected) && expected < 0) assert(json.find("\"-Infinity\"") != std::string::npos);
    for (const auto& cell : result.trace_rows[0]) {
      const double actual = std::get<double>(cell);
      if (std::isnan(expected)) assert(std::isnan(actual));
      else assert(actual == expected);
    }
  };
  check(make_bytes_attribute("value", "12"), 12);
  check(make_bytes_attribute("value", " 1_2.5\t"), 12.5);
  check({"value", "1_2.5", AttributeKind::kString}, 12.5);
  check({"value", "nan", AttributeKind::kString}, std::nan(""));
  check({"value", "-Infinity", AttributeKind::kString}, -INFINITY);
  EventAttribute native{"value", "stale", AttributeKind::kInt};
  native.typed_value = std::int64_t{16777217};
  check(native, 16777217);
  native.kind = AttributeKind::kFloat;
  native.typed_value = 1.25;
  check(native, 1.25);
  native.kind = AttributeKind::kBoolean;
  native.typed_value = true;
  check(native, 1);
  const auto rejects = [&](EventAttribute attribute) {
    attribute.key = "value";
    log.traces[0].events[0].attributes = {attribute};
    log.traces[0].attributes = {{"case_value", "0", AttributeKind::kInt}};
    bool rejected = false;
    try { (void)log_to_features(log, options); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
  };
  rejects({"value", "true", AttributeKind::kString});
  rejects({"value", "12", AttributeKind::kInt, true});
  rejects(make_bytes_attribute("value", "1__2"));
  rejects(make_bytes_attribute("value", std::string("1\0", 2)));
  rejects(make_bytes_attribute("value", "\xc2\xa0" "1"));
  EventAttribute nested{"value", "12", AttributeKind::kInt};
  nested.typed_value = std::int64_t{12};
  for (const auto kind : {AttributeContainerKind::kTuple,
                          AttributeContainerKind::kSet,
                          AttributeContainerKind::kFrozenSet,
                          AttributeContainerKind::kList,
                          AttributeContainerKind::kContainer}) {
    nested.container = kind;
    rejects(nested);
  }
  log.traces[0].events.push_back(Event{"c1", "B"});
  for (std::size_t position = 0; position < 2; ++position) {
    log.traces[0].events[position].attributes = {{"value", "bad", AttributeKind::kString}};
    log.traces[0].events[1 - position].attributes = {make_bytes_attribute("value", "12")};
    bool rejected = false;
    try { (void)log_to_features(log, options); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
  }
  log.traces[0].events[0].attributes = {make_bytes_attribute("value", "12")};
  log.traces[0].events[1].attributes = {{"value", "13", AttributeKind::kInt}};
  log.traces[0].attributes = {{"case_value", "0", AttributeKind::kInt}};
  assert(log_to_features(log, options).trace_rows[0][0] == 13);
  log.traces[0].attributes = {{"case_value", "12", AttributeKind::kInt, true}};
  bool null_case_rejected = false;
  try { (void)log_to_features(log, options); }
  catch (const std::invalid_argument&) { null_case_rejected = true; }
  assert(null_case_rejected);
  log.traces[0].attributes.clear();
  bool missing_case_rejected = false;
  try { (void)log_to_features(log, options); }
  catch (const std::invalid_argument&) { missing_case_rejected = true; }
  assert(missing_case_rejected);
  log.traces[0].attributes = {{"case_value", "0", AttributeKind::kInt}};
  for (std::size_t position = 0; position < 2; ++position) {
    log.traces[0].events[position].attributes.clear();
    log.traces[0].events[1 - position].attributes = {make_bytes_attribute("value", "12")};
    const auto result = log_to_features(log, options);
    assert(result.trace_rows[0] == std::vector<double>({12, 0}));
  }
  log.traces.push_back(Trace{"missing", {Event{"missing", "A"}}});
  for (int order = 0; order < 2; ++order) {
    bool missing_rejected = false;
    try { (void)log_to_features(log, options); }
    catch (const std::invalid_argument&) { missing_rejected = true; }
    assert(missing_rejected);
    std::reverse(log.traces.begin(), log.traces.end());
  }
  log.traces.back().events.clear();
  bool empty_rejected = false;
  try { (void)log_to_features(log, options); }
  catch (const std::invalid_argument&) { empty_rejected = true; }
  assert(empty_rejected);
  log.traces.clear();
  assert(log_to_features(log, options).trace_rows.empty());
}

static void event_numeric_attributes_require_complete_native_columns() {
  LogToFeaturesOptions options;
  options.variant = LogToFeaturesVariant::EVENT_BASED;
  options.event_numeric_attributes = {"value"};
  auto log = fixture();
  EventAttribute integer{"value", "stale", AttributeKind::kInt};
  integer.typed_value = std::int64_t{16777217};
  EventAttribute boolean{"value", "stale", AttributeKind::kBoolean};
  boolean.typed_value = true;
  log.traces[0].events[0].attributes = {integer};
  log.traces[0].events[1].attributes = {boolean};
  const auto feature_index = [](const LogToFeaturesResult& result) {
    return std::find(result.feature_names.begin(), result.feature_names.end(), "event:value");
  };
  auto result = log_to_features(log, options);
  auto feature = feature_index(result);
  assert(feature != result.feature_names.end());
  auto column = static_cast<std::size_t>(feature - result.feature_names.begin());
  assert(result.event_rows[0][0][column] == 16777216);
  assert(result.event_rows[0][1][column] == 1);
  EventAttribute floating{"value", "stale", AttributeKind::kFloat};
  floating.typed_value = std::nan("");
  log.traces[0].events[0].attributes = {floating};
  floating.typed_value = INFINITY;
  log.traces[0].events[1].attributes = {floating};
  result = log_to_features(log, options);
  column = static_cast<std::size_t>(feature_index(result) - result.feature_names.begin());
  assert(std::isnan(result.event_rows[0][0][column]));
  assert(std::isinf(result.event_rows[0][1][column]));
  const auto nonfinite_json = log_to_features_json(result);
  assert(nonfinite_json.find("\"NaN\"") != std::string::npos);
  assert(nonfinite_json.find("\"Infinity\"") != std::string::npos);
  floating.typed_value = 1.23456789;
  log.traces[0].events[0].attributes = {floating};
  result = log_to_features(log, options);
  assert(result.event_rows[0][0][column] == static_cast<double>(static_cast<float>(1.23456789)));
  assert(log_to_features_json(result).find("1.2345678806304932") != std::string::npos);
  const auto omitted = [&](const std::vector<EventAttribute>& attributes) {
    for (std::size_t position = 0; position < 2; ++position) {
      log.traces[0].events[position].attributes = attributes;
      log.traces[0].events[1 - position].attributes = {integer};
      const auto excluded = log_to_features(log, options);
      assert(feature_index(excluded) == excluded.feature_names.end());
    }
  };
  omitted({make_bytes_attribute("value", "12")});
  omitted({{"value", "12", AttributeKind::kString}});
  omitted({{"value", "12", AttributeKind::kInt, true}});
  omitted({});
  EventAttribute nested = integer;
  for (const auto kind : {AttributeContainerKind::kTuple,
                          AttributeContainerKind::kSet,
                          AttributeContainerKind::kFrozenSet}) {
    nested.container = kind;
    omitted({nested});
  }
  // Completeness includes events in other traces, not just the current row.
  log.traces[0].events[0].attributes = {integer};
  log.traces[0].events[1].attributes = {boolean};
  log.traces.push_back(Trace{"other", {Event{"other", "A"}}});
  result = log_to_features(log, options);
  assert(feature_index(result) == result.feature_names.end());
}

static void numeric_features_resolve_first_class_cells_and_inference() {
  auto log = fixture();
  log.traces[0].events.resize(1);
  auto& trace = log.traces[0];
  auto& event = trace.events[0];
  event.attributes.clear();
  trace.attributes.clear();
  LogToFeaturesOptions options;
  options.event_numeric_attributes = {"resource"};
  options.case_numeric_attributes = {"case_id"};
  options.feature_names = {"event:resource", "trace:case_id"};
  EventAttribute native{"worker", "stale", AttributeKind::kInt};
  native.typed_value = std::int64_t{17};
  event.resource = "stale";
  event.resource_state = FirstClassAttributeState::kPresent;
  event.resource_source_key = "worker";
  event.resource_attribute = native;
  trace.case_id = "stale";
  trace.case_id_state = FirstClassAttributeState::kPresent;
  trace.case_id_source_key = "identifier";
  trace.case_id_attribute = make_bytes_attribute("identifier", "12");
  assert(log_to_features(log, options).trace_rows[0] == std::vector<double>({17, 12}));
  options.event_numeric_attributes = {"worker"};
  options.case_numeric_attributes = {"identifier"};
  options.feature_names = {"event:worker", "trace:identifier"};
  assert(log_to_features(log, options).trace_rows[0] == std::vector<double>({17, 12}));
  event.resource_attribute = make_bytes_attribute("worker", "23");
  assert(log_to_features(log, options).trace_rows[0][0] == 23);
  event.attributes = {{"worker", "31", AttributeKind::kInt}};
  assert(log_to_features(log, options).trace_rows[0][0] == 31);
  event.attributes.clear();
  const auto rejects = [&] {
    bool rejected = false;
    try { (void)log_to_features(log, options); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
  };
  for (const auto state : {FirstClassAttributeState::kMissing, FirstClassAttributeState::kNull}) {
    native.first_class_role = FirstClassAttributeRole::kResource;
    native.first_class_state = state;
    native.first_class_source_key = "worker";
    event.attributes = {native};
    rejects();
  }
  event.attributes.clear();
  event.resource_attribute.reset();
  event.resource.clear();
  rejects(); // Present empty resource is a value, not a missing default.
  event.resource_attribute = make_bytes_attribute("worker", "23");
  trace.case_id_attribute.reset();
  trace.case_id.clear();
  rejects();
  trace.case_id = "12";
  options.event_numeric_attributes = {"activity"};
  options.feature_names = {"event:activity", "trace:identifier"};
  event.activity.clear();
  event.activity_state = FirstClassAttributeState::kPresent;
  rejects();
  event.activity = "5";
  options.case_numeric_attributes = {"activity"};
  options.feature_names = {"event:activity", "trace:activity"};
  rejects(); // Event activity is not a trace case-id alias.

  options.variant = LogToFeaturesVariant::EVENT_BASED;
  options.case_numeric_attributes.clear();
  options.event_numeric_attributes = {"worker"};
  options.feature_names = {"event:worker"};
  native.first_class_role = FirstClassAttributeRole::kResource;
  native.first_class_state = FirstClassAttributeState::kPresent;
  event.resource_attribute = native;
  assert(log_to_features(log, options).event_rows[0][0][0] == 17);

  auto inferred_log = fixture();
  EventAttribute typed{"number", "stale", AttributeKind::kString};
  typed.typed_value = 7.5;
  for (auto& entry : inferred_log.traces[0].events) entry.attributes = {typed};
  inferred_log.traces[0].attributes = {typed};
  const auto inferred = log_to_features(inferred_log);
  assert(std::find(inferred.feature_names.begin(), inferred.feature_names.end(), "event:number") != inferred.feature_names.end());
  assert(std::find(inferred.feature_names.begin(), inferred.feature_names.end(), "trace:number") != inferred.feature_names.end());
  typed.kind = AttributeKind::kInt;
  for (const auto kind : {AttributeContainerKind::kList, AttributeContainerKind::kTuple,
                          AttributeContainerKind::kSet, AttributeContainerKind::kFrozenSet}) {
    typed.container = kind;
    for (auto& entry : inferred_log.traces[0].events) entry.attributes = {typed};
    inferred_log.traces[0].attributes = {typed};
    if (kind == AttributeContainerKind::kList || kind == AttributeContainerKind::kSet) {
      bool rejected = false;
      try { (void)log_to_features(inferred_log); }
      catch (const std::invalid_argument&) { rejected = true; }
      assert(rejected);
      continue;
    }
    const auto nested = log_to_features(inferred_log);
    assert(std::find(nested.feature_names.begin(), nested.feature_names.end(), "event:number") == nested.feature_names.end());
    assert(std::find(nested.feature_names.begin(), nested.feature_names.end(), "trace:number") == nested.feature_names.end());
  }
  LogToFeaturesOptions inference_options;
  inference_options.min_num_different_string_values = 1;
  typed.container = AttributeContainerKind::kScalar;
  for (const auto role : {FirstClassAttributeRole::kNone, FirstClassAttributeRole::kResource}) {
    for (const auto state : {FirstClassAttributeState::kMissing, FirstClassAttributeState::kNull}) {
      if (role == FirstClassAttributeRole::kNone && state == FirstClassAttributeState::kMissing) continue;
      typed.first_class_role = role;
      typed.first_class_state = state;
      for (auto& entry : inferred_log.traces[0].events) entry.attributes = {typed};
      inferred_log.traces[0].attributes = {typed};
      const auto absent = log_to_features(inferred_log, inference_options);
      assert(std::none_of(absent.feature_names.begin(), absent.feature_names.end(), [](const std::string& name) {
        return name.find("number") != std::string::npos;
      }));
    }
  }
}

static void event_schema_respects_typed_selection_and_explicit_empty_lists() {
  auto log = fixture();
  LogToFeaturesOptions options;
  options.variant = LogToFeaturesVariant::EVENT_BASED;
  const auto inferred = log_to_features(log, options);
  assert((inferred.feature_names == std::vector<std::string>{
      "event:amount", "event:concept:name@A", "event:concept:name@B"}));
  assert((inferred.event_rows[0] == std::vector<std::vector<double>>{{2.5, 1, 0}, {3, 0, 1}}));
  auto projected = log;
  for (auto& event : projected.traces[0].events) {
    event.activity_source_key = "task";
    event.timestamp_state = FirstClassAttributeState::kPresent;
    event.timestamp_attribute = EventAttribute{"time:timestamp", event.activity, AttributeKind::kString};
  }
  const auto projected_result = log_to_features(projected, options);
  assert((projected_result.feature_names == std::vector<std::string>{
      "event:amount", "event:task@A", "event:task@B", "event:time:timestamp@A", "event:time:timestamp@B"}));
  assert((projected_result.event_rows[0][0] == std::vector<double>{2.5, 1, 0, 1, 0}));
  auto projection_collision = log;
  for (auto& event : projection_collision.traces[0].events) {
    event.timestamp.clear();
    event.resource_source_key = "time:timestamp";
    event.resource_state = FirstClassAttributeState::kPresent;
    event.resource_attribute = EventAttribute{"time:timestamp", "17", AttributeKind::kInt};
  }
  LogToFeaturesOptions projection_options;
  projection_options.variant = LogToFeaturesVariant::EVENT_BASED;
  projection_options.feature_names = {"event:time:timestamp"};
  const auto resource_collision = log_to_features(projection_collision, projection_options);
  assert((resource_collision.event_rows[0] == std::vector<std::vector<double>>{{17}, {17}}));
  for (auto& event : projection_collision.traces[0].events)
    event.activity_source_key = "time:timestamp";
  projection_options.feature_names = {"event:time:timestamp@A", "event:time:timestamp@B"};
  const auto activity_collision = log_to_features(projection_collision, projection_options);
  assert((activity_collision.event_rows[0] == std::vector<std::vector<double>>{{1, 0}, {0, 1}}));
  options.event_string_attributes_supplied = true;
  auto result = log_to_features(log, options);
  assert((result.feature_names == std::vector<std::string>{"event:amount"}));
  options.event_numeric_attributes_supplied = true;
  result = log_to_features(log, options);
  assert(result.feature_names.empty() && result.event_rows[0].size() == 2 && result.event_rows[0][0].empty());
  options.event_string_attributes_supplied = false;
  result = log_to_features(log, options);
  assert((result.feature_names == std::vector<std::string>{"event:concept:name@A", "event:concept:name@B"}));
  options.feature_names_supplied = true;
  result = log_to_features(log, options);
  assert(result.feature_names.empty() && result.event_rows[0][0].empty());
  options.feature_names_supplied = false;
  options.event_numeric_attributes_supplied = false;
  options.case_string_attributes = {"segment"};
  options.case_numeric_attributes = {"missing"};
  options.activity_key = "not_selected";
  options.timestamp_key = "not_selected";
  options.resource_key = "not_selected";
  log.traces[0].events[0].timestamp = "not a timestamp";
  assert(log_to_features(log, options).feature_names == inferred.feature_names);

  options.event_string_attributes = {"kind"};
  options.event_numeric_attributes_supplied = true;
  options.min_num_different_string_values = 1;
  const auto partial = log_to_features(log, options);
  assert((partial.feature_names == std::vector<std::string>{"event:kind@x"}));
  assert((partial.event_rows[0] == std::vector<std::vector<double>>{{1}, {0}}));
  for (const auto& invalid : std::vector<EventAttribute>{
           {"kind", "x", AttributeKind::kString, true},
           {"kind", "x", AttributeKind::kInt}, make_bytes_attribute("kind", "x")}) {
    log.traces[0].events[1].attributes.push_back(invalid);
    assert(log_to_features(log, options).event_rows == partial.event_rows);
    log.traces[0].events[1].attributes.pop_back();
  }
  EventAttribute nested{"kind", "x", AttributeKind::kString};
  nested.container = AttributeContainerKind::kTuple;
  log.traces[0].events[1].attributes.push_back(nested);
  assert(log_to_features(log, options).event_rows == partial.event_rows);
  log.traces[0].events[1].attributes.pop_back();
  log.traces.push_back(Trace{"empty", {}});
  options.default_not_present = -9;
  const auto padded = log_to_features(log, options);
  assert((padded.event_rows[1] == std::vector<std::vector<double>>{{0}, {0}}));
  assert(padded.event_rows[0] == partial.event_rows[0]);
  options.min_num_different_string_values = 2;
  assert(log_to_features(log, options).feature_names.empty());
  options.min_num_different_string_values = 0;
  options.max_num_different_string_values = 0;
  assert(log_to_features(log, options).feature_names.empty());

  options.event_string_attributes.clear();
  options.event_string_attributes_supplied = true;
  options.feature_names = {"event:amount", "event:amount", "", "unknown", "event:kind@x"};
  const auto direct = log_to_features(log, options);
  assert(direct.feature_names == options.feature_names);
  assert((direct.event_rows[0][0] == std::vector<double>{2.5, 0, 0, 0, 1}));
  assert((direct.event_rows[0][1] == std::vector<double>{3, 0, 0, 0, 0}));
  assert(!log_to_features_json(direct).empty());
  log.traces[0].events[1].attributes = {{"amount", "2.5", AttributeKind::kString}};
  options.feature_names = {"event:amount", "event:amount@2.5"};
  const auto mixed = log_to_features(log, options);
  assert((mixed.event_rows[0] == std::vector<std::vector<double>>{{2.5, 0}, {0, 1}}));
  options.feature_names.clear();

  EventLog collision;
  collision.traces.push_back(Trace{"collision", {Event{"collision", "A"}}});
  auto& collision_attributes = collision.traces[0].events[0].attributes;
  collision_attributes = {{"v", "x", AttributeKind::kString},
                          {"v@x", "7", AttributeKind::kInt}};
  LogToFeaturesOptions collision_options;
  collision_options.variant = LogToFeaturesVariant::EVENT_BASED;
  collision_options.event_string_attributes = {"v"};
  collision_options.event_numeric_attributes = {"v@x"};
  collision_options.min_num_different_string_values = 1;
  for (int order = 0; order < 2; ++order) {
    const auto automatic = log_to_features(collision, collision_options);
    assert((automatic.feature_names == std::vector<std::string>{"event:v@x", "event:v@x"}));
    assert((automatic.event_rows[0][0] == std::vector<double>{7, 0}));
    collision_options.feature_names = {"event:v@x"};
    assert((log_to_features(collision, collision_options).event_rows[0][0] == std::vector<double>{7}));
    collision_options.feature_names = {"event:v@x", "event:v@x"};
    assert((log_to_features(collision, collision_options).event_rows[0][0] == std::vector<double>{7, 0}));
    collision_options.feature_names.clear();
    std::reverse(collision_attributes.begin(), collision_attributes.end());
  }

  bool rejected = false;
  try { (void)log_to_features(EventLog{}, options); }
  catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected);
  EventLog empty_trace;
  empty_trace.traces.push_back(Trace{});
  const auto empty = log_to_features(empty_trace, options);
  assert(empty.feature_names.empty() && empty.event_rows.size() == 1 && empty.event_rows[0].empty());
}

static void trace_core_schema_preserves_selection_and_typed_categories() {
  EventLog log;
  Trace first{"one", {Event{"one", "A", "invalid timestamp"}, Event{"one", "B"}}};
  first.attributes = {{"group", "z"}, {"number", "4", AttributeKind::kInt}};
  first.events[0].attributes = {{"value", ""}, {"number", "2", AttributeKind::kInt}};
  first.events[1].attributes = {make_bytes_attribute("value", "x"), {"number", "3", AttributeKind::kInt}};
  Trace second{"two", {Event{"two", "B"}}};
  second.attributes = {{"group", "a"}, {"number", "7", AttributeKind::kInt}};
  second.events[0].attributes = {{"value", "", AttributeKind::kString, true}, {"number", "8", AttributeKind::kInt}};
  log.traces = {first, second};
  LogToFeaturesOptions options;
  options.case_string_attributes = {"group"};
  options.event_string_attributes = {"value"};
  options.case_numeric_attributes = {"number"};
  options.event_numeric_attributes = {"number"};
  options.event_successor_attributes = {"value"};
  options.enable_succ_def_representation = false;
  options.min_num_different_string_values = 100;
  options.default_not_present = -7;
  const auto result = log_to_features(log, options);
  assert((result.feature_names == std::vector<std::string>{"trace:group@a", "trace:group@z",
      "event:value@", "event:value@None", "event:value@b'x'", "trace:number", "event:number",
      "succession:value@#b'x'", "succession:value@UNDEFINED"}));
  assert((result.trace_rows == std::vector<std::vector<double>>{
      {0,1,1,0,1,4,3,1,0}, {1,0,0,1,0,7,8,0,1}}));
  options.feature_names = {"event:number", "unknown", "event:number", "", "activity_count:A"};
  const auto duplicate = log_to_features(log, options);
  assert((duplicate.trace_rows[0] == std::vector<double>{0,0,3,0,0}));
  assert(log_to_features_json(duplicate).find("\"feature_names\":[\"event:number\",\"unknown\",\"event:number\",\"\",\"activity_count:A\"]") != std::string::npos);
  options.feature_names.clear();
  options.feature_names_supplied = true;
  assert(log_to_features(log, options).trace_rows == std::vector<std::vector<double>>(2));
  auto invalid_numeric = log;
  invalid_numeric.traces[0].events[0].attributes.back() = {"number", "bad"};
  invalid_numeric.traces[0].attributes.back() = {"number", "bad"};
  assert(log_to_features(invalid_numeric, options).trace_rows == std::vector<std::vector<double>>(2));
  options = {};
  options.event_string_attributes_supplied = true;
  const auto none = log_to_features(log, options);
  assert(none.feature_names.empty() && none.trace_rows == std::vector<std::vector<double>>(2));
  options.feature_names = {"event:number", "event:concept:name@A"};
  assert((log_to_features(log, options).trace_rows[0] == std::vector<double>{0,0}));
  options = {};
  options.event_numeric_attributes = {"number", "number"};
  const auto repeated = log_to_features(log, options);
  assert((repeated.feature_names == std::vector<std::string>{"event:number", "event:number"}));
  assert((repeated.trace_rows[0] == std::vector<double>{0,3}));
  options = {};
  options.event_string_attributes = {"value"};
  options.event_successor_attributes = {"value"};
  log.traces[0].events.insert(log.traces[0].events.begin() + 1, Event{"one", "gap"});
  log.traces[1].events[0].attributes.clear();
  const auto gap = log_to_features(log, options);
  assert((gap.feature_names == std::vector<std::string>{"event:value@", "event:value@UNDEFINED",
      "event:value@b'x'", "succession:value@UNDEFINED"}));
  assert((gap.trace_rows == std::vector<std::vector<double>>{{1,0,1,1}, {0,1,0,1}}));
}

static void trace_default_schema_infers_complete_trace_presence() {
  auto log = fixture();
  log.traces[0].events[0].timestamp = "unused invalid timestamp";
  LogToFeaturesOptions options;
  options.min_num_different_string_values = 5;
  options.max_num_different_string_values = 1;
  const auto result = log_to_features(log, options);
  assert((result.feature_names == std::vector<std::string>{"trace:segment@gold",
      "event:concept:name@A", "event:concept:name@B", "event:kind@x", "event:org:resource@r1",
      "event:amount", "succession:concept:name@A#B"}));
  assert((result.trace_rows[0] == std::vector<double>{1,1,1,1,1,3,1}));
  log.traces[0].events[1].attributes.clear();
  const auto sparse = log_to_features(log, options);
  const auto amount = std::find(sparse.feature_names.begin(), sparse.feature_names.end(), "event:amount");
  assert(amount != sparse.feature_names.end());
  assert(sparse.trace_rows[0][static_cast<std::size_t>(amount - sparse.feature_names.begin())] == 2.5);
  log.traces.push_back(Trace{"missing", {Event{"missing", "C"}}});
  const auto missing = log_to_features(log, options);
  assert(std::find(missing.feature_names.begin(), missing.feature_names.end(), "event:amount") == missing.feature_names.end());
  assert(std::find(missing.feature_names.begin(), missing.feature_names.end(), "event:kind@x") == missing.feature_names.end());
  assert(std::find(missing.feature_names.begin(), missing.feature_names.end(), "trace:segment@gold") == missing.feature_names.end());
  for (auto& trace : log.traces) for (auto& event : trace.events) {
    event.attributes = {{"boolean", "true", AttributeKind::kBoolean}, make_bytes_attribute("bytes", "x")};
  }
  const auto omitted = log_to_features(log, options);
  assert(std::none_of(omitted.feature_names.begin(), omitted.feature_names.end(), [](const auto& name) {
    return name.starts_with("event:boolean") || name.starts_with("event:bytes");
  }));
}

int main() {
  temporal_calendar_metric_destinations_observe_write_order();
  temporal_calendar_missing_completions_are_validated_per_case();
  temporal_calendar_timestamps_use_projected_native_dates();
  temporal_calendar_categories_use_projected_trace_columns();
  temporal_calendar_numeric_columns_promote_before_grouping();
  temporal_calendar_frozen_case_tie_order();
  temporal_calendar_frozen_mixed_endpoints();
  temporal_calendar_nested_object_compatibility();
  temporal_calendar_frozen_ties_keep_typed_members();
  temporal_calendar_tuple_nan_factorization_is_local();
  temporal_calendar_tuple_case_ordering();
  temporal_calendar_scalar_case_ordering_is_exact();
  temporal_calendar_string_case_ties_use_lexical_order();
  temporal_calendar_groups_selected_typed_case_identifiers();
  temporal_calendar_categories_preserve_typed_equality_and_presence();
  temporal_calendar_rates_ignore_empty_traces();
  temporal_calendar_means_weight_distinct_cases_equally();
  temporal_calendar_metrics_use_completions_and_canonical_schema();
  temporal_calendar_uses_selected_column_timezone();
  temporal_calendar_grid_uses_origin_and_fills_empty_periods();
  temporal_calendar_fiscal_month_anchors();
  temporal_calendar_business_days_and_periods();
  default_trace_selection_validates_all_candidate_hashability();
  default_trace_strings_have_fixed_cardinality_limit();
  default_trace_selection_ignores_custom_core_schema();
  trace_extra_timestamps_respect_native_values_and_presence();
  concurrency_uses_exact_comparison_and_required_accesses();
  indirect_paths_include_every_nonadjacent_later_event();
  workload_intervals_and_resource_types_match_exact_boundaries();
  trace_identifier_feature_preserves_native_cells();
  occurrence_timing_uses_input_order_and_interval_endpoints();
  extra_features_append_to_custom_core_schema_and_respect_overrides();
  trace_default_schema_infers_complete_trace_presence();
  trace_core_schema_preserves_selection_and_typed_categories();
  event_schema_respects_typed_selection_and_explicit_empty_lists();
  numeric_features_resolve_first_class_cells_and_inference();
  event_numeric_attributes_require_complete_native_columns();
  explicit_trace_numeric_attributes_retain_native_values();
  const auto log = fixture();
  LogToFeaturesOptions e; e.variant = LogToFeaturesVariant::EVENT_BASED; e.event_numeric_attributes = {"amount"}; e.event_string_attributes = {"kind"}; e.case_string_attributes = {"segment"};
  const auto er = log_to_features(log, e);
  assert(er.event_rows.size() == 1 && er.event_rows[0].size() == 2);
  assert(!er.feature_names.empty());
  assert(std::find(er.feature_names.begin(), er.feature_names.end(), "event:amount") != er.feature_names.end());
  e.min_num_different_string_values = 1;
  const auto er_strings = log_to_features(log, e);
  assert(std::find(er_strings.feature_names.begin(), er_strings.feature_names.end(), "event:kind@x") != er_strings.feature_names.end());
  EventLog ragged = log;
  ragged.traces.push_back(Trace{"c2", {Event{"c2", "A", "2024-01-01T02:00:00Z", "r2", 2}}});
  const auto rectangular = log_to_features(ragged, e);
  assert(rectangular.event_rows.size() == 2 && rectangular.event_rows[0].size() == 2 && rectangular.event_rows[1].size() == 2);
  assert(std::all_of(rectangular.event_rows[1][1].begin(), rectangular.event_rows[1][1].end(), [](double value) { return value == 0.0; }));
  const auto event_json = log_to_features_json(er);
  assert(event_json.find("\"family\":\"LOG_TO_FEATURES\"") != std::string::npos);
  assert(event_json.find("\"event_rows\"") != std::string::npos);
  e.variant = LogToFeaturesVariant::TRACE_BASED;
  const auto tr = log_to_features(log, e);
  assert(tr.trace_rows.size() == 1 && tr.trace_rows[0].size() == tr.feature_names.size());
  assert(std::find(tr.feature_names.begin(), tr.feature_names.end(), "event:kind@x") != tr.feature_names.end());
  e.enable_all_extra_features = true;
  e.add_case_identifier_column = true;
  e.event_successor_attributes = {"concept:name"};
  const auto rich = log_to_features(log, e);
  assert(rich.case_ids.size() == 1 && rich.case_ids[0] == "c1");
  assert(std::find(rich.feature_names.begin(), rich.feature_names.end(), "@@caseDuration") != rich.feature_names.end());
  assert(std::find(rich.feature_names.begin(), rich.feature_names.end(), "@@max_concurrent_activities_general") != rich.feature_names.end());
  assert(std::find(rich.feature_names.begin(), rich.feature_names.end(), "succession:concept:name@A#B") != rich.feature_names.end());
  e.variant = LogToFeaturesVariant::TEMPORAL; e.grouper_freq.clear(); e.window_seconds = 3600;
  const auto tm = log_to_features(log, e);
  assert(tm.temporal_rows.size() == 2 && tm.temporal_rows[0].values.size() == tm.feature_names.size());
  assert(std::find(tm.feature_names.begin(), tm.feature_names.end(), "average_service_time") != tm.feature_names.end());
  assert(std::find(tm.feature_names.begin(), tm.feature_names.end(), "number_of_cases") != tm.feature_names.end());
  assert(std::find(tm.feature_names.begin(), tm.feature_names.end(), "unique_resources") != tm.feature_names.end());
  assert(log_to_features_json(tm).find("\"temporal_rows\"") != std::string::npos);
  auto malformed_features = tr;
  malformed_features.trace_rows[0].pop_back();
  bool malformed_rejected = false;
  try { (void)log_to_features_json(malformed_features); }
  catch (const std::invalid_argument&) { malformed_rejected = true; }
  assert(malformed_rejected);
  malformed_features = tr;
  malformed_features.variant = static_cast<LogToFeaturesVariant>(99);
  malformed_rejected = false;
  try { (void)log_to_features_json(malformed_features); }
  catch (const std::invalid_argument&) { malformed_rejected = true; }
  assert(malformed_rejected);
  bool threw = false; try { e.window_seconds = 0; (void)log_to_features(log, e); } catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  EventLog bad = log; bad.traces[0].events[0].timestamp = "nonsense"; e.window_seconds = 3600; threw = false;
  try { (void)log_to_features(bad, e); } catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  EventLog empty;
  const auto empty_temporal = log_to_features(empty, e);
  assert(empty_temporal.temporal_rows.empty() && !empty_temporal.feature_names.empty());
  observed_path_timing_uses_configured_starts();
  explicit_trace_attributes_scope_missing_values();
  overlap_extras_use_case_intervals_and_other_events();
  temporal_calendar_metrics_select_first_valid_per_column();
  temporal_calendar_metric_destination_matrix();
  temporal_calendar_frequency_uses_period_boundaries();
}
