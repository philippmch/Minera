// Review BB1: resource profiles on typed CSV columns (int, float with
// blanks, bool, bool with a blank, text, dates with a blank) in every role.
// The workload metrics convert each row of the resource to an interval
// with .timestamp() before any window comparison, a masked frame reads its
// typed column's dtype, average_duration_activity with its start column on
// one named twice subtracts by that column's dtype, the case metrics
// compare only rows with a case, and without a start column the start of a
// row is the previous dated row of its case, an object column when no case
// has one (one row alone then subtracts; rows without a case still count).
// Roles (pandas variant) and local diagnostics on XES object columns: the
// groupby sorts each level over every row (numbers before text; dates
// beside numbers raise, named datetime.datetime), also rows the other
// level's null drops, and a date member's KeyError shows its repr.
// Also the OCEL 2.0 JSON transport, which carries "minera:nanoseconds"
// now: the frontend opens an OCEL SQLite file as that form. Values and
// messages are taken from pm4py 2.7.22 black-box: read_csv +
// format_dataframe(case_id="cid", activity_key="act", timestamp_key="ts")
// of the same text; pm4py's NaN mean is Minera's declared error.
#include "minera/csv_event_log_importer.hpp"
#include "minera/dataframe_columns.hpp"
#include "minera/local_diagnostics.hpp"
#include "minera/ocel.hpp"
#include "minera/resource_profiles.hpp"
#include "minera/role_discovery.hpp"
#include "minera/xes_event_log_importer.hpp"

#include <cassert>
#include <cmath>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace minera;

namespace {

EventLog upload(const std::string& text) {
  std::istringstream csv(text);
  CsvImportOptions import;
  import.case_id_column = "cid";
  import.activity_column = "act";
  import.timestamp_column = "ts";
  import.resource_column = "res";
  auto log = import_csv_event_log(csv, import);
  assert(log.dataframe_columns);
  return log;
}

struct Case {
  const char* metric;
  const char* r;
  const char* a;
  const char* s;
  const char* t;
  const char* c;
  const char* resource;
  const char* activity;
  double value;
  const char* error;
};

int check(const std::string& text, const std::vector<Case>& cases) {
  const auto log = upload(text);
  int failures = 0;
  for (const auto& entry : cases) {
    ResourceProfilesOptions options;
    options.metric = resource_profile_metric_from_string(entry.metric);
    options.resource_attribute = entry.r;
    options.activity_attribute = entry.a;
    options.start_timestamp_attribute = entry.s;
    options.timestamp_attribute = entry.t;
    options.case_id_attribute = entry.c;
    options.resource = entry.resource;
    options.activity = entry.activity;
    options.t1 = "2019-06-01 00:00:00";
    options.t2 = "2021-01-01 00:00:00";
    std::string got;
    try {
      const auto result = discover_resource_profiles(log, options);
      if (std::string(entry.error).empty() && result.rows.size() == 1 && result.rows[0].value == entry.value)
        continue;
      got = result.rows.size() == 1 ? std::to_string(result.rows[0].value)
                                    : std::to_string(result.rows.size()) + " rows";
    } catch (const std::invalid_argument& error) {
      if (error.what() == std::string(entry.error)) continue;
      got = error.what();
    }
    ++failures;
    std::cerr << entry.metric << " r=" << entry.r << " a=" << entry.a << " s=" << entry.s << " t=" << entry.t
              << " c=" << entry.c << " " << entry.resource << "/" << entry.activity << ": got " << got
              << "\n  wanted: " << (std::string(entry.error).empty() ? std::to_string(entry.value) : entry.error)
              << "\n";
  }
  return failures;
}

const std::string kTyped =
    "cid,act,ts,res,start,d1,ni,nf,nb,nbx,tx,cn\n"
    "c1,a,2020-01-01 10:00:00,R,2020-01-01 09:00:00,,1,1.5,True,True,u,\n"
    "c1,b,2020-01-01 11:00:00,S,2020-01-01 10:30:00,2020-01-01 10:30:00,2,,False,,,\n"
    "c2,a,2020-01-01 10:30:00,R,2020-01-01 09:30:00,2020-01-01 09:30:00,1,2.5,True,False,v,\n";

// metric, resource, activity, start, timestamp and case columns, the
// resource and activity asked for, then the value or the message.
const std::vector<Case> kTypedCases = {
    {"activity_completions", "nb", "concept:name", "", "nb", "case:concept:name", "R", "", 3.0, ""},
    {"activity_completions", "ni", "concept:name", "", "ni", "case:concept:name", "u", "", 0.0, "resource profiles: TypeError: '>=' not supported between instances of 'float' and 'datetime.datetime'"},
    {"activity_frequency", "nb", "nb", "", "nb", "case:concept:name", "R", "a", 1.0, ""},
    {"activity_frequency", "ni", "ni", "", "ni", "case:concept:name", "R", "a", 0.0, "resource profiles: TypeError: '>=' not supported between instances of 'float' and 'datetime.datetime'"},
    {"distinct_activities", "nb", "nb", "", "nb", "case:concept:name", "R", "", 0.0, ""},
    {"average_workload", "res", "concept:name", "", "tx", "case:concept:name", "R", "", 0.0, "resource profiles: AttributeError: 'str' object has no attribute 'timestamp'"},
    {"average_workload", "res", "concept:name", "", "ni", "case:concept:name", "R", "", 0.0, "resource profiles: AttributeError: 'float' object has no attribute 'timestamp'"},
    {"average_workload", "res", "concept:name", "", "nb", "case:concept:name", "R", "", 0.0, "resource profiles: AttributeError: 'bool' object has no attribute 'timestamp'"},
    {"average_workload", "res", "concept:name", "ni", "time:timestamp", "case:concept:name", "R", "", 0.0, "resource profiles: AttributeError: 'int' object has no attribute 'timestamp'"},
    {"average_workload", "res", "concept:name", "tx", "ni", "case:concept:name", "R", "", 0.0, "resource profiles: AttributeError: 'str' object has no attribute 'timestamp'"},
    {"average_workload", "res", "concept:name", "start", "ni", "case:concept:name", "R", "", 0.0, "resource profiles: AttributeError: 'int' object has no attribute 'timestamp'"},
    {"average_workload", "res", "concept:name", "d1", "time:timestamp", "case:concept:name", "R", "", 0.0, "resource profiles: ValueError: NaTType does not support timestamp"},
    {"average_workload", "res", "concept:name", "d1", "time:timestamp", "case:concept:name", "S", "", 0.0, ""},
    {"multitasking", "res", "concept:name", "nf", "time:timestamp", "case:concept:name", "S", "", 0.0, "resource profiles: AttributeError: 'float' object has no attribute 'timestamp'"},
    {"multitasking", "res", "concept:name", "", "tx", "case:concept:name", "S", "", 0.0, "resource profiles: AttributeError: 'str' object has no attribute 'timestamp'"},
    {"average_workload", "case:concept:name", "case:concept:name", "case:concept:name", "case:concept:name", "res", "R", "", 0.0, "resource profiles: AttributeError: 'NoneType' object has no attribute 'timestamp'"},
    {"average_workload", "cid", "cid", "cid", "cid", "res", "R", "", 0.0, "resource profiles: AttributeError: 'float' object has no attribute 'timestamp'"},
    {"multitasking", "concept:name", "concept:name", "concept:name", "concept:name", "res", "R", "", 0.0, "resource profiles: AttributeError: 'NoneType' object has no attribute 'timestamp'"},
    {"average_case_duration", "res", "concept:name", "", "nb", "case:concept:name", "Q", "", 0.0, "resource profiles: TypeError: numpy boolean subtract, the `-` operator, is not supported, use the bitwise_xor, the `^` operator, or the logical_xor function instead."},
    {"average_case_duration", "res", "concept:name", "", "ni", "case:concept:name", "Q", "", 0.0, "resource profiles: AttributeError: Can only use .dt accessor with datetimelike values"},
    {"average_duration_activity", "res", "concept:name", "ni", "ts", "ni", "R", "zz", 0.0, "resource profiles: TypeError: Cannot subtract NaT from ndarray with dtype int64"},
    {"average_duration_activity", "res", "concept:name", "nb", "ts", "nb", "R", "zz", 0.0, "resource profiles: TypeError: Cannot subtract NaT from ndarray with dtype bool"},
    {"average_duration_activity", "res", "concept:name", "nf", "ts", "nf", "R", "zz", 0.0, "resource profiles: TypeError: Cannot subtract NaT from ndarray with dtype float64"},
    {"average_duration_activity", "res", "concept:name", "start", "ts", "start", "R", "zz", 0.0, "resource profiles: AttributeError: 'DataFrame' object has no attribute 'dt'"},
    {"average_duration_activity", "res", "concept:name", "start", "ni", "start", "Q", "a", 0.0, "resource profiles: TypeError: cannot subtract DatetimeArray from ndarray[float64]"},
    {"average_duration_activity", "res", "concept:name", "start", "nb", "start", "Q", "a", 0.0, "resource profiles: TypeError: cannot subtract DatetimeArray from ndarray[object]"},
    {"average_duration_activity", "res", "concept:name", "start", "tx", "start", "Q", "a", 0.0, "resource profiles: TypeError: unsupported operand type(s) for -: 'DatetimeArray' and 'float'"},
    {"average_duration_activity", "res", "concept:name", "start", "case:concept:name", "start", "Q", "a", 0.0, "resource profiles: TypeError: unsupported operand type(s) for -: 'DatetimeArray' and 'NAType'"},
    {"average_duration_activity", "res", "concept:name", "start", "d1", "start", "Q", "a", 0.0, "resource profiles: AttributeError: 'DataFrame' object has no attribute 'dt'"},
    {"average_duration_activity", "res", "concept:name", "d1", "ts", "case:concept:name", "R", "a", 3600.0, ""},
    {"average_duration_activity", "res", "concept:name", "d1", "ts", "case:concept:name", "S", "b", 1800.0, ""},
    {"case_completions", "res", "concept:name", "", "tx", "cn", "R", "", 0.0, ""},
    {"fraction_case_completions", "res", "concept:name", "", "tx", "cn", "R", "", 0.0, ""},
    {"case_completions", "res", "concept:name", "", "tx", "case:concept:name", "R", "", 0.0, "resource profiles: TypeError: '>=' not supported between instances of 'str' and 'datetime.datetime'"},
};

// The case column nf holds 1.5, 2 and blanks (no two rows share a case),
// cx one value; d1 leaves c0's two rows without a date.
const std::string kPredecessors =
    "cid,act,ts,res,nf,d1,cx\n"
    "c0,a,2020-01-01 08:30:00,R,1.5,,\n"
    "c0,a,2020-01-01 08:40:00,R,,,k\n"
    "c1,a,2020-01-01 09:30:00,R,2,2020-01-01 09:30:00,\n"
    "c2,a,2020-01-01 09:40:00,S,,2020-01-01 09:20:00,\n"
    "c3,a,2020-01-01 09:50:00,R,,2020-01-01 09:25:00,\n";

const std::vector<Case> kPredecessorCases = {
    {"average_duration_activity", "res", "concept:name", "", "ts", "nf", "R", "a", 0.0, "resource profiles: AttributeError: Can only use .dt accessor with datetimelike values"},
    {"average_duration_activity", "res", "concept:name", "", "ts", "nf", "S", "a", 0.0, ""},
    {"average_duration_activity", "res", "concept:name", "", "ts", "nf", "Q", "a", 0.0, "resource profiles: AttributeError: Can only use .dt accessor with datetimelike values"},
    {"average_duration_activity", "res", "concept:name", "", "ts", "cx", "R", "a", 0.0, "resource profiles: AttributeError: Can only use .dt accessor with datetimelike values"},
    {"average_duration_activity", "res", "concept:name", "", "ts", "cx", "S", "a", 0.0, ""},
    {"average_duration_activity", "res", "concept:name", "", "d1", "case:concept:name", "R", "a", 0.0, "resource profiles: AttributeError: Can only use .dt accessor with datetimelike values"},
    {"average_duration_activity", "res", "concept:name", "", "d1", "case:concept:name", "S", "a", 0.0, ""},
    {"average_duration_activity", "res", "concept:name", "", "ts", "case:concept:name", "R", "a", 150.0, ""},
    {"average_duration_activity", "res", "concept:name", "nf", "ts", "cx", "R", "a", 0.0, "resource profiles: TypeError: unsupported operand type(s) for -: 'Timestamp' and 'float'"},
    {"average_duration_activity", "res", "concept:name", "nf", "ts", "cx", "S", "a", 0.0, "resource profiles: TypeError: unsupported operand type(s) for -: 'Timestamp' and 'float'"},
    {"average_workload", "res", "concept:name", "", "d1", "case:concept:name", "S", "", 0.0, ""},
    {"average_workload", "res", "concept:name", "", "d1", "case:concept:name", "R", "", 0.0, "resource profiles: ValueError: NaTType does not support timestamp"},
};

// org:resource holds text and ints, x a date and a float on rows with no
// resource, y text and a date (its +02:00 offset read as UTC wall clock).
const std::string kMixedXes = R"(<?xml version="1.0" encoding="UTF-8"?>
<log xes.version="1.0">
<trace><string key="concept:name" value="c1"/>
<event><string key="concept:name" value="a"/><date key="time:timestamp" value="2020-01-01T00:01:00"/><string key="org:resource" value="R"/><string key="g" value="G"/><string key="y" value="R"/><string key="a2" value="p"/></event>
<event><string key="concept:name" value="b"/><date key="time:timestamp" value="2020-01-01T00:02:00"/><int key="org:resource" value="5"/><string key="g" value="G"/><date key="y" value="2021-01-01T00:00:00+02:00"/></event>
<event><string key="concept:name" value="a"/><date key="time:timestamp" value="2020-01-01T00:03:00"/><int key="org:resource" value="7"/><string key="g" value="H"/><string key="y" value="S"/><string key="a2" value="q"/></event>
<event><string key="concept:name" value="c"/><date key="time:timestamp" value="2020-01-01T00:04:00"/><date key="x" value="2020-02-02T00:00:00"/><string key="g" value="G"/></event>
<event><string key="concept:name" value="b"/><date key="time:timestamp" value="2020-01-01T00:05:00"/><string key="org:resource" value="S"/><float key="x" value="2.5"/><string key="g" value="H"/><string key="y" value="R"/></event>
</trace>
</log>
)";

int expect_message(const std::function<void()>& call, const std::string& expected) {
  try {
    call();
  } catch (const std::invalid_argument& error) {
    if (error.what() == expected) return 0;
    std::cerr << "unexpected message: " << error.what() << "\n  wanted: " << expected << "\n";
    return 1;
  }
  std::cerr << "no error, wanted: " << expected << "\n";
  return 1;
}

int mixed_xes_columns() {
  std::istringstream xes(kMixedXes);
  auto log = import_xes_dataframe_log(xes);
  log.dataframe_columns = std::make_shared<DataFrameColumns>(xes_dataframe_columns(log));
  int failures = 0;
  const auto roles = [&](const std::string& r, const std::string& a) {
    RoleDiscoveryOptions options;
    options.variant = RoleDiscoveryVariant::kPandas;
    options.resource_attribute = r;
    options.activity_attribute = a;
    return discover_roles(log, options);
  };
  const auto shown = [](const RoleDiscoveryResult& result) {
    std::string text;
    for (const auto& role : result.roles) {
      text += "[";
      for (const auto& activity : role.activities) text += activity + ",";
      text += "|";
      for (const auto& resource : role.resources) text += resource.name + ":" + std::to_string(resource.count) + ",";
      text += "]";
    }
    return text;
  };
  // pm4py: [(['b'], {5: 1, 'S': 1}), (['a'], {7: 1, 'R': 1})].
  if (const auto got = shown(roles("org:resource", "concept:name")); got != "[b,|5:1,S:1,][a,|7:1,R:1,]") {
    std::cerr << "roles on text and ints: " << got << "\n";
    ++failures;
  }
  if (const auto got = shown(roles("y", "concept:name")); got != "[b,|2021-01-01 00:00:00+00:00:1,R:1,][a,|R:1,S:1,]") {
    std::cerr << "roles on text and a date: " << got << "\n";
    ++failures;
  }
  failures += expect_message([&] { (void)roles("org:resource", "x"); },
                             "TypeError: '<' not supported between instances of 'float' and 'datetime.datetime'");
  failures += expect_message([&] { (void)roles("org:resource", "y"); },
                             "TypeError: sequence item 0: expected str instance, datetime.datetime found");
  const auto diagnostics = [&](const std::string& r, const std::string& a) {
    LocalDiagnosticsOptions options;
    options.resource_attribute = r;
    options.activity_attribute = a;
    options.group_attribute = "g";
    return discover_local_diagnostics(log, options);
  };
  const auto plain = diagnostics("org:resource", "concept:name");
  if (plain.group_coverage.at("G").at("5") != 0.5 || plain.group_coverage.at("H").at("S") != 0.5 ||
      plain.group_relative_focus.at("G").at("a") != 0.5) {
    std::cerr << "diagnostics on text and ints\n";
    ++failures;
  }
  failures += expect_message([&] { (void)diagnostics("org:resource", "x"); },
                             "TypeError: '<' not supported between instances of 'float' and 'datetime.datetime'");
  failures += expect_message([&] { (void)diagnostics("y", "a2"); },
                             "KeyError: datetime.datetime(2021, 1, 1, 0, 0, tzinfo=datetime.timezone.utc)");
  const auto dated = diagnostics("y", "concept:name");
  if (dated.group_coverage.at("G").at("2021-01-01 00:00:00+00:00") != 0.5 ||
      std::abs(dated.group_relative_stake.at("G").at("b") - 2.0 / 3.0) > 1e-12) {
    std::cerr << "diagnostics on text and a date\n";
    ++failures;
  }
  return failures;
}

// The int 1 and True in one XES column are one resource; the EventLog
// variant's Counter names it per role by its first value there, the left
// role's on a merge.
const std::string kOneTrueXes = R"(<?xml version="1.0" encoding="UTF-8"?>
<log xes.version="1.0">
<trace><string key="concept:name" value="c1"/>
<event><string key="concept:name" value="a"/><date key="time:timestamp" value="2020-01-01T00:01:00"/><int key="mix" value="1"/></event>
<event><string key="concept:name" value="b"/><date key="time:timestamp" value="2020-01-01T00:02:00"/><boolean key="mix" value="true"/></event>
<event><string key="concept:name" value="a"/><date key="time:timestamp" value="2020-01-01T00:03:00"/><string key="mix" value="R"/></event>
<event><string key="concept:name" value="b"/><date key="time:timestamp" value="2020-01-01T00:04:00"/><string key="mix" value="S"/></event>
<event><string key="concept:name" value="b"/><date key="time:timestamp" value="2020-01-01T00:05:00"/><int key="mix" value="1"/></event>
</trace>
</log>
)";

int log_roles_name_per_role() {
  std::istringstream xes(kOneTrueXes);
  auto log = import_xes_dataframe_log(xes);
  log.dataframe_columns = std::make_shared<DataFrameColumns>(xes_dataframe_columns(log));
  int failures = 0;
  for (const auto& [threshold, expected] : std::vector<std::pair<double, std::string>>{
           {0.65, "[b,|True:2,S:1,][a,|1:1,R:1,]"}, {0.1, "[a,b,|True:3,S:1,R:1,]"}}) {
    RoleDiscoveryOptions options;
    options.variant = RoleDiscoveryVariant::kLog;
    options.resource_attribute = "mix";
    options.threshold = threshold;
    const auto result = discover_roles(log, options);
    std::string got;
    for (const auto& role : result.roles) {
      got += "[";
      for (const auto& activity : role.activities) got += activity + ",";
      got += "|";
      for (const auto& resource : role.resources) got += resource.name + ":" + std::to_string(resource.count) + ",";
      got += "]";
    }
    if (got != expected) {
      std::cerr << "log roles at " << threshold << ": " << got << "\n  wanted: " << expected << "\n";
      ++failures;
    }
  }
  return failures;
}

const std::string kStandard = R"({"objectTypes": [{"name": "t", "attributes": []}],
 "eventTypes": [{"name": "a", "attributes": []}, {"name": "b", "attributes": []}],
 "objects": [{"id": "o1", "type": "t"}],
 MARKER"events": [{"id": "e1", "type": "a", "time": "2029-05-07T06:15:00.123456789Z", "relationships": [{"objectId": "o1", "qualifier": ""}]},
            {"id": "e2", "type": "b", "time": "2029-05-07T08:15:00.000000001Z", "relationships": [{"objectId": "o1", "qualifier": ""}]}]})";

std::string standard_with(const std::string& marker) {
  auto text = kStandard;
  text.replace(text.find("MARKER"), 6, marker);
  return text;
}

// read_ocel2_json keeps microseconds; a standard document Minera wrote for
// a log read at nanoseconds (the frontend's opened OCEL SQLite) says so and
// keeps them, through another round trip; a file for users has no marker.
int standard_json_keeps_nanoseconds() {
  const auto file = parse_ocel_json(standard_with(""));
  assert(file.events.size() == 2);
  assert(file.events[0].timestamp == "2029-05-07T06:15:00.123456+00:00");
  assert(serialize_ocel_json(file, OcelJsonFormat::kStandard).find("minera:nanoseconds") == std::string::npos);
  const auto transport = parse_ocel_json(standard_with("\"minera:nanoseconds\": true, "));
  assert(transport.events[0].timestamp == "2029-05-07T06:15:00.123456789Z");
  assert(transport.events[1].timestamp == "2029-05-07T08:15:00.000000001Z");
  const auto json = serialize_ocel_json(transport, OcelJsonFormat::kStandard);
  assert(json.find("\"minera:nanoseconds\":true") != std::string::npos);
  const auto again = parse_ocel_json(json);
  assert(again.events[0].timestamp == "2029-05-07T06:15:00.123456789Z");
  assert(again.events[1].timestamp == "2029-05-07T08:15:00.000000001Z");
  assert(serialize_ocel_json(transport, OcelJsonFormat::kStandard, false).find("minera:nanoseconds") ==
         std::string::npos);
  return 0;
}

}  // namespace

int main() {
  int failures = check(kTyped, kTypedCases);
  failures += check(kPredecessors, kPredecessorCases);
  failures += mixed_xes_columns();
  failures += log_roles_name_per_role();
  failures += standard_json_keeps_nanoseconds();
  if (failures != 0) {
    std::cerr << failures << " review BB1 cases failed\n";
    return 1;
  }
  std::cout << "review BB1 tests passed\n";
  return 0;
}
