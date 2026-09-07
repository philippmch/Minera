#include "minera/log_to_features.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <stdexcept>

#include "minera/json_export.hpp"
#include "minera/timestamp.hpp"

using namespace minera;

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

static Event timed_event(const std::string& case_id, const std::string& activity,
                         const std::string& start, const std::string& finish,
                         std::size_t order) {
  Event event{case_id, activity, finish, "r1", order};
  event.attributes.push_back({"start_time", start, AttributeKind::kDate});
  return event;
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

static void trace_defaults_infer_case_attributes_and_scope_missing_values() {
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
  const auto result = log_to_features(log, options);
  const auto find_feature = [&](const std::string& name) {
    return std::find(result.feature_names.begin(), result.feature_names.end(), name);
  };
  const auto segment = find_feature("trace:segment@gold");
  const auto priority = find_feature("case:priority");
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
  assert(weekly.temporal_rows[0].values[0] == 1.0 &&
         weekly.temporal_rows[1].values[1] == 1.0);

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
  options.feature_names = {"event:value", "case:case_value"};
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
    for (double actual : result.trace_rows[0]) {
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
  options.feature_names = {"event:resource", "case:case_id"};
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
  options.feature_names = {"event:worker", "case:identifier"};
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
  event.activity.clear();
  event.activity_state = FirstClassAttributeState::kPresent;
  rejects();
  event.activity = "5";
  options.case_numeric_attributes = {"activity"};
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
  assert(std::find(inferred.feature_names.begin(), inferred.feature_names.end(), "case:number") != inferred.feature_names.end());
  typed.kind = AttributeKind::kInt;
  for (const auto kind : {AttributeContainerKind::kList, AttributeContainerKind::kTuple,
                          AttributeContainerKind::kSet, AttributeContainerKind::kFrozenSet}) {
    typed.container = kind;
    for (auto& entry : inferred_log.traces[0].events) entry.attributes = {typed};
    inferred_log.traces[0].attributes = {typed};
    const auto nested = log_to_features(inferred_log);
    assert(std::find(nested.feature_names.begin(), nested.feature_names.end(), "event:number") == nested.feature_names.end());
    assert(std::find(nested.feature_names.begin(), nested.feature_names.end(), "case:number") == nested.feature_names.end());
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

int main() {
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
  assert(std::find(tr.feature_names.begin(), tr.feature_names.end(), "activity:A") != tr.feature_names.end());
  e.enable_all_extra_features = true;
  e.add_case_identifier_column = true;
  const auto rich = log_to_features(log, e);
  assert(rich.case_ids.size() == 1 && rich.case_ids[0] == "c1");
  assert(std::find(rich.feature_names.begin(), rich.feature_names.end(), "@@caseDuration") != rich.feature_names.end());
  assert(std::find(rich.feature_names.begin(), rich.feature_names.end(), "@@max_concurrent_activities_general") != rich.feature_names.end());
  assert(std::find(rich.feature_names.begin(), rich.feature_names.end(), "succession:activity@A#B") != rich.feature_names.end());
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
  trace_defaults_infer_case_attributes_and_scope_missing_values();
  overlap_extras_use_case_intervals_and_other_events();
  temporal_calendar_frequency_uses_period_boundaries();
}
