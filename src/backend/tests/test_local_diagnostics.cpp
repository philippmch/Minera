#include "minera/local_diagnostics.hpp"
#include "minera/request_params.hpp"
#include "minera/csv_event_log_importer.hpp"

#include <cassert>
#include <map>
#include <sstream>
#include <string>
#include <stdexcept>

using namespace minera;

// pm4py 2.7.22 on read_csv + format_dataframe(case_id="cid", activity_key="act",
// timestamp_key="ts") of this CSV (Review AT1): "who" and "code" are int
// columns (past 2**53 too), "team" text with a NaN cell, no org:resource.
static EventLog at1_csv_log() {
  std::istringstream csv("cid,act,ts,who,team,code\n"
                         "c1,a,2020-01-01 10:00:00,1,g1,7\n"
                         "c1,b,2020-01-01 11:00:00,2,g2,8\n"
                         "c2,a,2020-01-02 10:00:00,1,g1,7\n"
                         "c2,c,2020-01-02 13:00:00,1,,9\n"
                         "c3,a,2020-01-03 10:00:00,2,g2,7\n"
                         "c3,c,2020-01-03 10:30:00,9007199254740993,g1,9007199254740992\n");
  CsvImportOptions import;
  import.case_id_column = "cid";
  import.activity_column = "act";
  import.timestamp_column = "ts";
  import.resource_column = "who";
  auto log = import_csv_event_log(csv, import);
  assert(log.dataframe_columns);
  return log;
}

static std::string error_of(const EventLog& log, const LocalDiagnosticsOptions& options) {
  try { (void)discover_local_diagnostics(log, options); }
  catch (const std::invalid_argument& error) { return error.what(); }
  return {};
}

// The pandas apply_from_group_attribute: a group's members are the resources
// seen with it, every event of a member counts for its groups (the NaN team
// cell too), resources keep their int names.
static void frame_path_reads_pm4py_frame() {
  const auto log = at1_csv_log();
  LocalDiagnosticsOptions options;
  assert(error_of(log, options).find("KeyError: 'org:resource'") != std::string::npos);
  options.resource_attribute = "who";
  options.group_attribute = "team";
  const auto result = discover_local_diagnostics(log, options);
  const std::map<std::string, std::map<std::string, double>> focus{
      {"g1", {{"a", 2.0 / 3}, {"b", 0.0}, {"c", 1.0}}}, {"g2", {{"a", 1.0 / 3}, {"b", 1.0}, {"c", 0.0}}}};
  assert(result.group_relative_focus == focus);
  const std::map<std::string, std::map<std::string, double>> stake{
      {"g1", {{"a", 2.0 / 3}, {"b", 0.0}, {"c", 1.0 / 3}}}, {"g2", {{"a", 0.5}, {"b", 0.5}, {"c", 0.0}}}};
  assert(result.group_relative_stake == stake);
  const std::map<std::string, std::map<std::string, double>> coverage{
      {"g1", {{"1", 2.0 / 3}, {"9007199254740993", 1.0 / 3}}}, {"g2", {{"2", 1.0}}}};
  assert(result.group_coverage == coverage);
  assert(result.group_member_contribution.at("g1").at("1").at("a") == 2);
  assert(result.group_member_contribution.at("g1").at("1").at("c") == 0);
  assert(result.group_member_contribution.at("g1").at("9007199254740993").at("c") == 1);
  // Explicit groups still read the frame's org:group column.
  LocalDiagnosticsOptions groups;
  groups.resource_attribute = "who";
  groups.explicit_groups = {{"R1", {"1", "2"}}};
  assert(error_of(log, groups).find("KeyError: 'org:group'") != std::string::npos);
}

int main() {
  frame_path_reads_pm4py_frame();
  EventLog log;
  Trace t; t.case_id = "c";
  t.events.push_back(Event{"c", "A", "", "r1", 0, {{"org:group", "g1", AttributeKind::kString}}, {}});
  t.events.push_back(Event{"c", "A", "", "r2", 1, {{"org:group", "g1", AttributeKind::kString}}, {}});
  t.events.push_back(Event{"c", "B", "", "r1", 2, {{"org:group", "g1", AttributeKind::kString}}, {}});
  t.events.push_back(Event{"c", "A", "", "r3", 3, {{"org:group", "g2", AttributeKind::kString}}, {}});
  t.events.push_back(Event{"c", "", "", "r4", 4, {{"org:group", "g2", AttributeKind::kString}}, {}});
  log.traces.push_back(std::move(t));
  const auto result = discover_local_diagnostics(log);
  assert(result.diagnostics.computed && result.diagnostics.skipped_events == 1);
  assert(result.group_member_contribution.at("g1").at("r1").at("A") == 1);
  assert(result.group_relative_focus.at("g1").at("A") == 2.0 / 3.0);
  assert(result.group_relative_stake.at("g1").at("A") == 2.0 / 3.0);
  assert(result.group_coverage.at("g1").at("r1") == 2.0 / 3.0);

  EventLog denominator_log = log;
  denominator_log.traces[0].events.push_back(
      Event{"c", "A", "", "r5", 5, {}, {}});
  const auto denominator_result = discover_local_diagnostics(denominator_log);
  assert(denominator_result.group_relative_focus.at("g1").at("A") == 0.5);

  // Reference apply_from_group_attribute: groups are resource sets, and every
  // event of a member counts for each of its groups (even an untagged one);
  // coverage alone uses the events tagged with the group. Values observed
  // from PM4Py 2.7.22.
  {
    EventLog multi;
    Trace m; m.case_id = "c";
    m.events.push_back(Event{"c", "A", "", "r1", 0, {{"org:group", "g1", AttributeKind::kString}}, {}});
    m.events.push_back(Event{"c", "B", "", "r1", 1, {{"org:group", "g2", AttributeKind::kString}}, {}});
    m.events.push_back(Event{"c", "A", "", "r1", 2, {}, {}});
    m.events.push_back(Event{"c", "A", "", "r2", 3, {{"org:group", "g2", AttributeKind::kString}}, {}});
    multi.traces.push_back(std::move(m));
    const auto r = discover_local_diagnostics(multi);
    assert(r.group_relative_focus.at("g1").at("A") == 2.0 / 3.0);
    assert(r.group_relative_focus.at("g1").at("B") == 1.0);
    assert(r.group_relative_focus.at("g2").at("A") == 1.0);
    assert(r.group_relative_stake.at("g1").at("A") == 2.0 / 3.0);
    assert(r.group_relative_stake.at("g2").at("A") == 0.75);
    assert(r.group_relative_stake.at("g2").at("B") == 0.25);
    assert(r.group_coverage.at("g1").at("r1") == 1.0);
    assert(r.group_coverage.at("g2").at("r1") == 0.5 && r.group_coverage.at("g2").at("r2") == 0.5);
    assert(r.group_member_contribution.at("g1").at("r1").at("A") == 2);
    assert(r.group_member_contribution.at("g2").at("r1").at("A") == 2);
    assert(r.group_member_contribution.at("g2").at("r1").at("B") == 1);
    assert(r.group_member_contribution.at("g2").at("r2").at("A") == 1);
    assert(r.diagnostics.skipped_events == 0);
  }

  LocalDiagnosticsOptions explicit_mode;
  explicit_mode.explicit_groups = {{"g1", {"r1", "r2"}}};
  const auto explicit_result = discover_local_diagnostics(log, explicit_mode);
  assert(explicit_result.group_relative_focus.at("g1").at("A") == 2.0 / 3.0);

  LocalDiagnosticsOptions capped;
  capped.max_rows = 1;
  const auto capped_result = discover_local_diagnostics(log, capped);
  assert(capped_result.diagnostics.output_truncated && capped_result.diagnostics.row_count == 1);
  assert(capped_result.group_relative_focus.at("g1").at("A") == 2.0 / 3.0);
  assert(capped_result.diagnostics.group_count == 2);
  const auto capped_json = local_diagnostics_json(capped_result);
  assert(capped_json.find("\"row_count\":1") != std::string::npos &&
         capped_json.find("\"group_count\":2") != std::string::npos);
  LocalDiagnosticsOptions empty_group;
  empty_group.explicit_groups = {{"unobserved", {}}};
  const auto empty_group_result = discover_local_diagnostics(EventLog{}, empty_group);
  assert(empty_group_result.diagnostics.group_count == 1 &&
         empty_group_result.diagnostics.row_count == 0);
  assert(!local_diagnostics_json(empty_group_result).empty());
  auto bad_rows = capped_result;
  ++bad_rows.diagnostics.row_count;
  try { (void)local_diagnostics_json(bad_rows); assert(false); }
  catch (const std::invalid_argument&) {}

  const auto json = local_diagnostics_json(result);
  assert(json.find("LOCAL_DIAGNOSTICS") != std::string::npos);
  auto malformed_result = result;
  malformed_result.group_relative_focus["g1"]["A"] = 2.0;
  try { (void)local_diagnostics_json(malformed_result); assert(false); }
  catch (const std::invalid_argument&) {}

  const auto parsed = parse_local_diagnostics_params([](std::string_view n) {
    if (n == "local_diagnostics_groups") return std::string("g1:r1,r2;g2:r3");
    return std::string{};
  });
  assert(parsed.explicit_groups.size() == 2 && parsed.explicit_groups.at("g1").size() == 2);
  for (const auto& invalid : {"g1:r1,,r2", "g1:r1,", "g1:r1;"}) {
    try {
      (void)parse_local_diagnostics_params([&](std::string_view n) {
        return n == "local_diagnostics_groups" ? std::string(invalid) : std::string{};
      });
      assert(false);
    } catch (const std::invalid_argument&) {
    }
  }
  return 0;
}
