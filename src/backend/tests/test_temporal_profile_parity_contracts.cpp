#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <functional>
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
  assert(result[0][0].zero_variance_sentinel);
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
  // The cases in the order of their first rows, c2 then c1, as pm4py's
  // DataFrame variant lists them (round BJ, observed black-box).
  assert(result.size() == 2);
  assert(result[0].empty());
  assert(result[1].size() == 1);
  assert(result[1][0].case_id == "c1");
  assert(result[1][0].elapsed_seconds == 9.0);
  // Zero variance: the reference's sys.maxsize, flagged for exact output.
  assert(result[1][0].zero_variance_sentinel);
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

  // A zero-variance pair carries the reference's integer sys.maxsize, not
  // the double 2**63 (9.2233720368547758e+18).
  minera::TemporalDeviation zero{"z", "A", "A", 0.5, 9223372036854775808.0, true};
  const auto zero_json = minera::temporal_conformance_json({{zero}});
  assert(zero_json.find("\"standard_deviation_distance\":9223372036854775807}") !=
         std::string::npos);
  assert(zero_json.find("e+18") == std::string::npos);
}


minera::EventTable polars_table(const std::vector<std::array<const char*, 3>>& rows) {
  minera::EventTable table;
  table.columns = {"case:concept:name", "concept:name", "time:timestamp"};
  for (const auto& [case_id, activity, time] : rows) {
    minera::EventTable::Row row(3);
    if (case_id) row[0] = minera::EventAttribute{"case:concept:name", case_id};
    row[1] = minera::EventAttribute{"concept:name", activity};
    row[2] = minera::EventAttribute{"time:timestamp", std::string("2021-03-01T") + time + "+00:00",
                                    minera::AttributeKind::kDate};
    table.rows.push_back(std::move(row));
  }
  return table;
}

// Round FV: pm4py's temporal profile on a Polars frame pairs the rows in
// Polars' partial order (an unstable sort of [case, start, completion] with
// the row index after it), never pairs a row without a case, and measures in
// whole seconds; the conformance lists one empty bucket for the missing case.
// Expected values from pm4py 2.7.22 on the Polars LazyFrame (observed
// black-box; the pandas frame gives 108 and 122.96 for (a, b)).
void temporal_profile_polars_frame_contract() {
  const auto ties = polars_table({
      {"c0", "b", "08:07:00"}, {"c0", "d", "08:05:00"}, {"c2", "c", "08:00:00"}, {"c2", "e", "08:00:00"},
      {"c2", "b", "08:02:00"}, {"c2", "d", "08:03:00"}, {"c2", "a", "08:05:00"}, {"c2", "d", "08:07:00"},
      {"c2", "c", "08:09:00"}, {"c3", "e", "08:05:00"}, {"c3", "e", "08:05:00"}, {"c3", "a", "08:07:00"},
      {"c3", "e", "08:09:00"}, {"c3", "b", "08:11:00"}, {"c3", "b", "08:11:00"}, {"c1", "c", "08:03:00"},
      {"c1", "a", "08:03:00"}, {"c1", "a", "08:04:00"}, {"c1", "d", "08:04:00"}, {"c1", "a", "08:04:00"},
      {"c1", "d", "08:04:00"}, {"c1", "b", "08:04:00"}});
  const auto lazy = minera::discover_temporal_profile(minera::LazyEventTable::from_table(ties), {});
  const auto* ab = lazy.find("a", "b");
  assert(ab && ab->mean_seconds == 180.0 && ab->standard_deviation_seconds == 103.92304845413264);
  const auto* da = lazy.find("d", "a");
  assert(da && da->mean_seconds == 30.0 && da->standard_deviation_seconds == 60.0);
  const auto* eb = lazy.find("e", "b");
  assert(eb && eb->mean_seconds == 257.14285714285717 && eb->standard_deviation_seconds == 128.2853961179637);
  assert(lazy.entries.size() == 23);
  const auto pandas = minera::discover_temporal_profile(ties, {});
  assert(pandas.find("a", "b")->mean_seconds == 108.0);

  const auto fractions = polars_table({{"c1", "a", "08:00:00"}, {"c1", "b", "08:00:01.700000"},
                                       {"c1", "b", "08:00:03.200000"}, {nullptr, "a", "08:00:00"},
                                       {nullptr, "b", "08:00:05"}, {"c0", "a", "08:00:00.500000"},
                                       {"c0", "b", "08:00:02.400000"}});
  const minera::LazyEventTable fraction_frame = minera::LazyEventTable::from_table(fractions);
  const auto whole = minera::discover_temporal_profile(fraction_frame, {});
  assert(whole.entries.size() == 2);
  assert(whole.find("a", "b")->mean_seconds == 1.6666666666666667 &&
         whole.find("a", "b")->standard_deviation_seconds == 1.1547005383792517);
  assert(whole.find("b", "b")->mean_seconds == 1.0 && whole.find("b", "b")->standard_deviation_seconds == 0.0);

  minera::TemporalProfile profile;
  profile.entries.push_back({"a", "b", 3, 1.0, 0.1});
  const auto buckets = minera::conform_temporal_profile(minera::EventDataView(fraction_frame), profile, {});
  assert(buckets.size() == 3);
  std::size_t deviations = 0;
  for (const auto& bucket : buckets)
    for (const auto& deviation : bucket) {
      ++deviations;
      assert(deviation.case_id == "c1" && deviation.source == "a" && deviation.target == "b");
      assert(deviation.elapsed_seconds == 3.0 && deviation.standard_deviation_distance == 20.0);
    }
  assert(deviations == 1);
}

minera::EventTable fw_business_table(const std::vector<std::array<const char*, 3>>& rows, bool start = false) {
  minera::EventTable table;
  table.columns = {"case:concept:name", "concept:name", "time:timestamp"};
  if (start) table.columns.push_back("st");
  for (const auto& [case_id, activity, time] : rows) {
    minera::EventTable::Row row(table.columns.size());
    row[0] = minera::EventAttribute{"case:concept:name", case_id};
    row[1] = minera::EventAttribute{"concept:name", activity};
    row[2] = minera::EventAttribute{"time:timestamp", std::string(time) + "+00:00", minera::AttributeKind::kDate};
    if (start) {
      // One hour before the completion.
      auto text = std::string(time);
      const auto hour = std::stoi(text.substr(11, 2)) - 1;
      text.replace(11, 2, (hour < 10 ? "0" : "") + std::to_string(hour));
      row[3] = minera::EventAttribute{"st", text + "+00:00", minera::AttributeKind::kDate};
    }
    table.rows.push_back(std::move(row));
  }
  return table;
}

std::string fw_business_error(const std::function<void()>& call) {
  try {
    call();
  } catch (const std::exception& error) {
    return error.what();
  }
  return "";
}

// Round FW: pm4py's temporal profile on a Polars frame with business hours
// measures every pair of the pair frame through map_elements; Polars infers
// Float64 and refuses the first Python int BusinessHours returns (no end
// inside a slot), in discovery and conformance alike, before the join with
// the profile. The pandas frame answers. Expected values and messages from
// pm4py 2.7.22 on the Polars LazyFrame with one Polars thread
// (review-harness round-fw bhorder.py, bhmsg.py, bhconf.py).
void temporal_profile_polars_business_hours_contract() {
  minera::TemporalProfileOptions options;
  options.use_business_hours = true;
  const auto message = [](const char* value, const char* start) {
    return std::string("unexpected value while building Series of type Float64; found value of type Int64: ") + value +
           "\n\nThis error occurred with the following context stack:\n\t[1] while constructing Series "
           "'time:timestamp'\n\n\nThis error occurred in the following expression:\n\tas_struct(\"time:timestamp\", \"" +
           start + "_2\").python_udf()\n";
  };
  const auto nights = fw_business_table({{"c1", "a", "2021-03-08T06:00:00"},
                                         {"c1", "b", "2021-03-09T06:00:00"},
                                         {"c1", "c", "2021-03-11T06:00:00"}});
  const auto lazy = minera::LazyEventTable::from_table(nights);
  assert(fw_business_error([&] { (void)minera::discover_temporal_profile(lazy, options); }) ==
         message("36000", "start_timestamp"));
  minera::TemporalProfile profile;
  profile.entries.push_back({"x", "y", 1, 1.0, 1.0});
  assert(fw_business_error([&] {
           (void)minera::conform_temporal_profile(minera::EventDataView(lazy), profile, options);
         }) == message("36000", "start_timestamp"));
  // The first int pair in the pair frame's order: case c1 sorts first.
  const auto two = minera::LazyEventTable::from_table(fw_business_table({{"c2", "a", "2021-03-08T06:00:00"},
                                                                         {"c2", "b", "2021-03-09T06:00:00"},
                                                                         {"c1", "a", "2021-03-08T06:00:00"},
                                                                         {"c1", "c", "2021-03-11T06:00:00"}}));
  assert(fw_business_error([&] { (void)minera::discover_temporal_profile(two, options); }) ==
         message("108000", "start_timestamp"));
  // The routes pass the completion key as the start key when none is given:
  // pm4py then reads its default start key.
  auto bound_options = options;
  bound_options.source_timestamp_attribute = "time:timestamp";
  bound_options.target_timestamp_attribute = "time:timestamp";
  assert(fw_business_error([&] { (void)minera::discover_temporal_profile(lazy, bound_options); }) ==
         message("36000", "start_timestamp"));
  // A start column names its join copy.
  auto start_options = options;
  start_options.target_timestamp_attribute = "st";
  const auto started = minera::LazyEventTable::from_table(
      fw_business_table({{"c1", "a", "2021-03-08T06:00:00"}, {"c1", "b", "2021-03-09T06:00:00"}}, true));
  assert(fw_business_error([&] { (void)minera::discover_temporal_profile(started, start_options); }) ==
         message("36000", "st"));
  // The pandas frame measures the same pairs without the refusal.
  assert(minera::discover_temporal_profile(nights, options).find("a", "b")->mean_seconds == 36000.0);
  // Durations inside the slots are floats: the profile answers.
  const auto days = minera::LazyEventTable::from_table(fw_business_table({{"c1", "a", "2021-03-08T08:00:00"},
                                                                          {"c1", "b", "2021-03-09T09:30:00"},
                                                                          {"c1", "c", "2021-03-11T12:00:00"}}));
  const auto found = minera::discover_temporal_profile(days, options);
  assert(found.entries.size() == 3);
  assert(found.find("a", "b")->mean_seconds == 41400.0 && found.find("a", "c")->mean_seconds == 122400.0 &&
         found.find("b", "c")->mean_seconds == 81000.0);
  // A tie is a float 0.0.
  const auto tie = minera::LazyEventTable::from_table(
      fw_business_table({{"c1", "a", "2021-03-08T06:00:00"}, {"c1", "b", "2021-03-08T06:00:00"}}));
  assert(minera::discover_temporal_profile(tie, options).find("a", "b")->mean_seconds == 0.0);
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
  temporal_profile_polars_frame_contract();
  temporal_profile_polars_business_hours_contract();
  std::cout << "temporal-profile parity contract tests passed\n";
}
