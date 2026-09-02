#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "minera/event_data.hpp"
#include "minera/json_export.hpp"
#include "minera/temporal_profile.hpp"

namespace {

minera::Event timed_event(std::string activity, std::string timestamp) {
  minera::Event event;
  event.activity = std::move(activity);
  event.timestamp = std::move(timestamp);
  return event;
}

minera::EventTable temporal_table() {
  minera::EventTable table;
  table.columns = {"cid", "act", "started", "completed"};
  const auto cell = [](std::string key, std::string value,
                       minera::AttributeKind kind =
                           minera::AttributeKind::kString) {
    return std::optional<minera::EventAttribute>(
        minera::EventAttribute{std::move(key), std::move(value), kind});
  };
  table.rows = {
      {cell("cid", "c2"), cell("act", "B"),
       cell("started", "1970-01-01T00:00:20Z",
            minera::AttributeKind::kDate),
       cell("completed", "1970-01-01T00:00:21Z",
            minera::AttributeKind::kDate)},
      {cell("cid", "c1"), cell("act", "B"),
       cell("started", "1970-01-01T00:00:10Z",
            minera::AttributeKind::kDate),
       cell("completed", "1970-01-01T00:00:11Z",
            minera::AttributeKind::kDate)},
      {cell("cid", "c1"), cell("act", "A"),
       cell("started", "1970-01-01T00:00:00Z",
            minera::AttributeKind::kDate),
       cell("completed", "1970-01-01T00:00:01Z",
            minera::AttributeKind::kDate)},
  };
  return table;
}

minera::TemporalProfileOptions table_options() {
  minera::TemporalProfileOptions options;
  options.case_id_attribute = "cid";
  options.activity_attribute = "act";
  options.source_timestamp_attribute = "completed";
  options.target_timestamp_attribute = "started";
  return options;
}

minera::EventLog first_class_log() {
  minera::EventLog log;
  minera::Trace trace;
  trace.case_id = "outer";
  trace.case_id_state = minera::FirstClassAttributeState::kPresent;
  trace.case_id_source_key = "case_col";
  const auto add_event = [&](const char* activity, const char* timestamp) {
    minera::Event event;
    event.case_id = "selected";
    event.case_id_state = minera::FirstClassAttributeState::kPresent;
    event.case_id_source_key = "case_col";
    event.activity = activity;
    event.activity_state = minera::FirstClassAttributeState::kPresent;
    event.activity_source_key = "activity_col";
    event.activity_attribute = minera::EventAttribute{
        "activity_col", activity, minera::AttributeKind::kString, false, {},
        minera::AttributeContainerKind::kScalar, {},
        minera::FirstClassAttributeRole::kActivity,
        minera::FirstClassAttributeState::kPresent, "activity_col"};
    event.timestamp = timestamp;
    event.timestamp_state = minera::FirstClassAttributeState::kPresent;
    event.timestamp_source_key = "timestamp_col";
    event.timestamp_attribute = minera::EventAttribute{
        "timestamp_col", timestamp, minera::AttributeKind::kDate, false, {},
        minera::AttributeContainerKind::kScalar, {},
        minera::FirstClassAttributeRole::kTimestamp,
        minera::FirstClassAttributeState::kPresent, "timestamp_col"};
    trace.events.push_back(std::move(event));
  };
  add_event("A", "1970-01-01T00:00:00Z");
  add_event("B", "1970-01-01T00:00:10Z");
  log.traces.push_back(std::move(trace));
  return log;
}

void temporal_profile_log_variant_contract() {
  minera::EventLog log;
  minera::Trace trace;
  trace.case_id = "c";
  trace.events = {timed_event("A", "1970-01-01T00:00:00Z"),
                  timed_event("B", "1970-01-01T00:00:10Z")};
  log.traces.push_back(std::move(trace));

  const auto profile = minera::discover_temporal_profile_log(log);
  assert(profile.entries.size() == 1);
  assert(profile.entries[0].source == "A");
  assert(profile.entries[0].target == "B");
  assert(profile.entries[0].mean_seconds == 10.0);

  minera::TemporalProfileOptions no_hours;
  no_hours.use_business_hours = true;
  no_hours.business_hour_slots_explicit = true;
  const auto empty_schedule =
      minera::discover_temporal_profile_log(log, no_hours);
  assert(empty_schedule.entries[0].mean_seconds == 0.0);

  auto invalid = log;
  invalid.traces[0].events[1].timestamp = "invalid";
  bool threw = false;
  try {
    (void)minera::discover_temporal_profile_log(invalid);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void temporal_profile_dataframe_variant_contract() {
  const auto table = temporal_table();
  const auto options = table_options();
  const auto profile =
      minera::discover_temporal_profile_dataframe(table, options);
  assert(profile.entries.size() == 1);
  assert(profile.entries[0].source == "A");
  assert(profile.entries[0].target == "B");
  assert(profile.entries[0].mean_seconds == 9.0);
}

void temporal_profile_first_class_selector_contract() {
  auto options = minera::TemporalProfileOptions{};
  options.activity_attribute = "activity_col";
  options.source_timestamp_attribute = "timestamp_col";
  options.target_timestamp_attribute = "timestamp_col";
  const auto profile =
      minera::discover_temporal_profile_log(first_class_log(), options);
  assert(profile.entries.size() == 1);
  assert(profile.entries.front().source == "A");
  assert(profile.entries.front().target == "B");
  assert(profile.entries.front().mean_seconds == 10.0);
}

void temporal_profile_independent_target_default_contract() {
  minera::EventTable table;
  table.columns = {"cid", "act", "completed", "time:timestamp"};
  const auto cell = [](const char* key, const char* value) {
    return std::optional<minera::EventAttribute>(
        minera::EventAttribute{key, value, minera::AttributeKind::kDate});
  };
  table.rows = {
      {std::optional<minera::EventAttribute>(
           minera::EventAttribute{"cid", "c", minera::AttributeKind::kString}),
       std::optional<minera::EventAttribute>(
           minera::EventAttribute{"act", "A", minera::AttributeKind::kString}),
       cell("completed", "1970-01-01T00:00:00Z"),
       cell("time:timestamp", "1970-01-01T00:00:01Z")},
      {std::optional<minera::EventAttribute>(
           minera::EventAttribute{"cid", "c", minera::AttributeKind::kString}),
       std::optional<minera::EventAttribute>(
           minera::EventAttribute{"act", "B", minera::AttributeKind::kString}),
       cell("completed", "1970-01-01T00:00:10Z"),
       cell("time:timestamp", "1970-01-01T00:00:11Z")}};
  auto options = table_options();
  options.source_timestamp_attribute = "completed";
  options.target_timestamp_attribute.clear();
  const auto profile =
      minera::discover_temporal_profile_dataframe(table, options);
  assert(profile.entries.size() == 1);
  assert(profile.entries.front().mean_seconds == 11.0);
}

void temporal_profile_discovery_dispatch_contract() {
  const auto table = temporal_table();
  const auto profile = minera::discover_temporal_profile(
      minera::EventDataView(table), table_options());
  assert(profile.find("A", "B") != nullptr);
}

void temporal_profile_conformance_log_variant_contract() {
  minera::EventLog log;
  minera::Trace first;
  first.case_id = "c1";
  first.events = {timed_event("A", "1970-01-01T00:00:00Z"),
                  timed_event("B", "1970-01-01T00:00:10Z")};
  minera::Trace second;
  second.case_id = "c2";
  second.events = {timed_event("A", "1970-01-01T00:00:00Z"),
                   timed_event("B", "1970-01-01T00:00:00Z")};
  log.traces = {first, second};
  minera::TemporalProfile profile;
  profile.entries.push_back({"A", "B", 1, 0.0, 0.0});
  minera::TemporalProfileOptions options;
  options.zeta = 0.0;
  const auto result =
      minera::conform_temporal_profile_log(log, profile, options);
  assert(result.size() == 2);
  assert(result[0].size() == 1);
  assert(result[0][0].standard_deviation_distance ==
         static_cast<double>(std::numeric_limits<std::int64_t>::max()));
  assert(result[1].empty());

  options.zeta = std::numeric_limits<double>::infinity();
  const auto suppressed =
      minera::conform_temporal_profile_log(log, profile, options);
  assert(suppressed[0].empty() && suppressed[1].empty());
}

void temporal_profile_conformance_dataframe_variant_contract() {
  const auto table = temporal_table();
  minera::TemporalProfile profile;
  profile.entries.push_back({"A", "B", 1, 0.0, 0.0});
  auto options = table_options();
  options.zeta = -1.0;
  const auto result =
      minera::conform_temporal_profile_dataframe(table, profile, options);
  assert(result.size() == 2);
  assert(result[0].size() == 1);
  assert(result[1].empty());
  assert(result[0][0].elapsed_seconds == 9.0);
}

void temporal_profile_conformance_dispatch_contract() {
  const auto table = temporal_table();
  minera::TemporalProfile profile;
  profile.entries.push_back({"A", "B", 1, 9.0, 0.0});
  const auto result = minera::conform_temporal_profile(
      minera::EventDataView(table), profile, table_options());
  assert(result.size() == 2);
  assert(result[0].empty() && result[1].empty());
}

void temporal_profile_conformance_json_contract() {
  minera::TemporalConformanceResult result = {{
      {"c", "A", "B", 10.0,
       static_cast<double>(std::numeric_limits<std::int64_t>::max())}}};
  const auto json = minera::temporal_conformance_json(result);
  assert(json.find("\"family\":\"TEMPORAL_PROFILE_CONFORMANCE\"") !=
         std::string::npos);
  assert(json.find("\"deviation_count\":1") != std::string::npos);
  assert(json.find("\"case_id\":\"c\"") != std::string::npos);
}

}  // namespace

int main() {
  temporal_profile_log_variant_contract();
  temporal_profile_dataframe_variant_contract();
  temporal_profile_first_class_selector_contract();
  temporal_profile_independent_target_default_contract();
  temporal_profile_discovery_dispatch_contract();
  temporal_profile_conformance_log_variant_contract();
  temporal_profile_conformance_dataframe_variant_contract();
  temporal_profile_conformance_dispatch_contract();
  temporal_profile_conformance_json_contract();
  std::cout << "temporal-profile parity contract tests passed\n";
}
