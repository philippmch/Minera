// Review BA1: activity_frequency and distinct_activities with the activity
// and resource on the timestamp column (pandas masks one column selected
// three times instead of realigning it), the case metrics with the case
// column on the timestamp column, average_case_duration's start key, OCEL
// 1.0 JSON files with digits past the microsecond (read_ocel_json keeps
// microseconds; Minera's own transport documents say when they keep
// nanoseconds), a case: column of True/False with a blank (object dtype),
// passed time on an XES object column of dates with the text NaT, and the
// export CLI on XES files, which now reads the frame pm4py.read_xes returns,
// as the server does. Values and messages are taken from pm4py 2.7.22
// black-box: read_csv + format_dataframe(case_id="cid", activity_key="act",
// timestamp_key="ts") of the same text, read_ocel_json and read_xes of the
// same files.
#include "minera/case_statistics.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/dataframe_columns.hpp"
#include "minera/ocel.hpp"
#include "minera/passed_time.hpp"
#include "minera/resource_profiles.hpp"
#include "minera/xes_event_log_importer.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

using namespace minera;
namespace fs = std::filesystem;

namespace {

std::string export_binary;

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

void expect_error(const std::function<void()>& call, const std::string& expected) {
  try {
    call();
  } catch (const std::invalid_argument& error) {
    if (error.what() == expected) return;
    std::cerr << "unexpected message: " << error.what() << "\n  wanted: " << expected << "\n";
    assert(false);
  }
  std::cerr << "no error, wanted: " << expected << "\n";
  assert(false);
}

const std::string kProfiles =
    "cid,act,ts,res,start\n"
    "c1,a,2020-01-01 10:00:00,R,2020-01-01 09:00:00\n"
    "c1,b,2020-01-01 11:00:00,S,2020-01-01 10:30:00\n"
    "c2,a,2020-01-01 10:30:00,R,2020-01-01 09:30:00\n";

// activity_frequency selects [activity, timestamp, resource]: one column
// named three times keeps every row under its masks, so pm4py returns
// len / len = 1.0 for any resource and activity, unless the timestamp
// comparison meets a resource's cell that is no date.
void activity_frequency_on_one_column() {
  const auto log = upload(kProfiles);
  const auto profile = [&](const std::string& key, const std::string& resource, const std::string& activity) {
    ResourceProfilesOptions options;
    options.metric = ResourceProfileMetric::kActivityFrequency;
    options.resource_attribute = options.activity_attribute = options.timestamp_attribute = key;
    options.t1 = "2019-06-01 00:00:00";
    options.t2 = "2021-01-01 00:00:00";
    options.resource = resource;
    options.activity = activity;
    return discover_resource_profiles(log, options);
  };
  for (const auto& [key, resource, activity] :
       std::vector<std::tuple<std::string, std::string, std::string>>{
           {"time:timestamp", "2020-01-01 10:00:00+00:00", "a"},
           {"time:timestamp", "Q", "Q"},
           {"res", "Q", "a"},
           {"start", "Q", "R"}}) {
    const auto result = profile(key, resource, activity);
    assert(result.rows.size() == 1);
    assert(result.rows[0].resource == resource);
    assert(result.rows[0].activity == activity);
    assert(result.rows[0].value == 1.0);
  }
  expect_error([&] { (void)profile("res", "R", "a"); },
               "resource profiles: TypeError: '>=' not supported between instances of 'str' and "
               "'datetime.datetime'");
  // Every resource of the column: each one's call gives 1.0.
  const auto all = profile("time:timestamp", "", "a");
  assert(all.rows.size() == 3);
  for (const auto& row : all.rows) assert(row.value == 1.0 && row.activity == "a");
}

// distinct_activities on one column: nunique of each masked copy counts
// the resource's one value when a row of it lies in the window (pm4py: that
// count three times, a Series); a matching text cell fails the comparison.
// The case metrics group by the case column first: on the timestamp column
// it is two. average_case_duration reads its start column after the
// comparison and the groupby.
void one_column_selections() {
  const auto log = upload(kProfiles);
  const auto run = [&](ResourceProfileMetric metric, const std::string& r, const std::string& t,
                       const std::string& c, const std::string& resource, const std::string& start = "") {
    ResourceProfilesOptions options;
    options.metric = metric;
    options.resource_attribute = r;
    options.activity_attribute = r;
    options.timestamp_attribute = t;
    options.case_id_attribute = c;
    options.start_timestamp_attribute = start;
    options.t1 = "2019-06-01 00:00:00";
    options.t2 = "2021-01-01 00:00:00";
    options.resource = resource;
    return discover_resource_profiles(log, options);
  };
  const auto distinct = [&](const std::string& key, const std::string& resource) {
    const auto result = run(ResourceProfileMetric::kDistinctActivities, key, key, "case:concept:name", resource);
    assert(result.rows.size() == 1);
    return result.rows[0].value;
  };
  assert(distinct("time:timestamp", "2020-01-01 10:00:00") == 1.0);
  assert(distinct("time:timestamp", "2020-01-01 09:00:00") == 0.0);
  assert(distinct("start", "2020-01-01 09:00:00") == 1.0);
  assert(distinct("res", "Q") == 0.0);
  expect_error([&] { (void)distinct("res", "R"); },
               "resource profiles: TypeError: '>=' not supported between instances of 'str' and "
               "'datetime.datetime'");
  for (const auto metric : {ResourceProfileMetric::kCaseCompletions, ResourceProfileMetric::kFractionCaseCompletions})
    for (const auto& r : {"res", "concept:name"})
      expect_error([&] { (void)run(metric, r, "res", "res", "R"); },
                   "resource profiles: ValueError: Grouper for 'res' not 1-dimensional");
  // A text timestamp column fails the comparison first; the resource
  // column as the start key is still a KeyError after it.
  expect_error([&] { (void)run(ResourceProfileMetric::kAverageCaseDuration, "res", "concept:name", "case:concept:name", "R",
                                    "start"); },
               "resource profiles: TypeError: '<' not supported between instances of 'str' and 'datetime.datetime'");
  expect_error([&] { (void)run(ResourceProfileMetric::kAverageCaseDuration, "res", "time:timestamp", "case:concept:name", "R",
                                    "res"); },
               "resource profiles: KeyError: 'res'");
  expect_error([&] { (void)run(ResourceProfileMetric::kAverageCaseDuration, "res", "concept:name", "concept:name", "Q",
                                    "start"); },
               "resource profiles: ValueError: Grouper for 'concept:name' not 1-dimensional");
}

// read_csv makes a True/False column with a blank object dtype, a case:
// column too; the case statistics' .dt accessor then fails.
void case_bool_column_with_blank() {
  std::istringstream csv(
      "cid,act,ts,case:flag\n"
      "c1,a,2020-01-01 10:00:00,True\n"
      "c1,b,2020-01-01 11:00:00,\n"
      "c2,a,2020-01-02 10:30:00,False\n");
  CsvImportOptions import;
  import.case_id_column = "cid";
  import.activity_column = "act";
  import.timestamp_column = "ts";
  const auto log = import_csv_event_log(csv, import);
  CaseStatisticsOptions options;
  options.case_id_key = "case:concept:name";
  options.timestamp_key = "case:flag";
  options.dataframe = true;
  expect_error([&] { (void)discover_case_statistics(log, options); },
               "AttributeError: Can only use .dt accessor with datetimelike values");
}

// An XES column of dates whose last row holds the text NaT is an object
// column; the pairs' raised starts (a date column) less it is one pair's
// whole-array subtraction, a timedelta; two pairs go cell by cell.
void passed_time_on_object_column() {
  const auto read = [](const std::string& text) {
    std::istringstream xes(text);
    auto log = import_xes_dataframe_log(xes);
    log.dataframe_columns = std::make_shared<DataFrameColumns>(xes_dataframe_columns(log));
    return log;
  };
  const auto event = [](const std::string& activity, const std::string& ts, const std::string& st) {
    return "<event><string key=\"concept:name\" value=\"" + activity +
           "\"/><date key=\"time:timestamp\" value=\"" + ts + "\"/><date key=\"ts\" value=\"" + ts + "\"/>" +
           st + "</event>";
  };
  const std::string head = R"(<?xml version="1.0" encoding="UTF-8"?><log xes.version="1.0" xmlns="http://www.xes-standard.org/">)";
  const std::string first = "<trace><string key=\"concept:name\" value=\"c0\"/>" +
                            event("a", "2020-01-01T01:00:00+00:00", "<date key=\"st\" value=\"2020-01-01T00:30:00+00:00\"/>") +
                            event("b", "2020-01-01T02:00:00+00:00", "<string key=\"st\" value=\"NaT\"/>") + "</trace>";
  const std::string second = "<trace><string key=\"concept:name\" value=\"c1\"/>" +
                             event("a", "2020-01-01T05:00:00+00:00", "<date key=\"st\" value=\"2020-01-01T04:00:00+00:00\"/>") +
                             event("b", "2020-01-01T06:00:00+00:00", "<date key=\"st\" value=\"2020-01-01T05:30:00+00:00\"/>") +
                             "</trace>";
  PassedTimeOptions options;
  options.activity = "a";
  options.variant = PassedTimeVariant::kPost;
  options.activity_key = "concept:name";
  options.timestamp_key = "st";
  options.start_timestamp_key = "ts";
  options.dataframe = true;
  const auto one = discover_passed_time(read(head + first + "</log>"), options);
  assert(one.post.size() == 1 && one.post[0].activity == "b" && one.post[0].performance == 5400.0 &&
         one.post[0].frequency == 1 && one.post_avg_perf == 5400.0);
  options.activity = "b";
  options.variant = PassedTimeVariant::kPre;
  const auto pre = discover_passed_time(read(head + first + "</log>"), options);
  assert(pre.pre.size() == 1 && pre.pre[0].activity == "a" && pre.pre_avg_perf == 5400.0);
  expect_error([&] { (void)discover_passed_time(read(head + first + second + "</log>"), options); },
               "AttributeError: Can only use .dt accessor with datetimelike values");
  // A column missing from the frame named twice in the selection: pandas
  // names it once.
  options.start_timestamp_key = "absent";
  options.timestamp_key = "absent";
  expect_error([&] { (void)discover_passed_time(read(head + first + "</log>"), options); },
               "KeyError: \"['absent'] not in index\"");
}

const std::string kNanoseconds =
    R"({"ocel:global-event": {"ocel:activity": "__INVALID__"}, "ocel:global-object": {"ocel:type": "__INVALID__"},
 "ocel:global-log": {"ocel:object-types": ["t"], "ocel:attribute-names": [], "ocel:version": "1.0", "ocel:ordering": "timestamp"},
 "ocel:events": {"e1": {"ocel:activity": "a", "ocel:timestamp": "2029-05-07T06:15:00.123456789Z", "ocel:vmap": {}, "ocel:omap": ["o1"]},
                 "e2": {"ocel:activity": "b", "ocel:timestamp": "2029-05-07T08:15:00.000000001Z", "ocel:vmap": {}, "ocel:omap": ["o1"]}},
 MARKER"ocel:objects": {"o1": {"ocel:type": "t", "ocel:ovmap": {}}}})";

std::string with_marker(const std::string& marker) {
  auto text = kNanoseconds;
  text.replace(text.find("MARKER"), 6, marker);
  return text;
}

// read_ocel_json stores datetime64[us]: "06:15:00.123456789Z" is
// 06:15:00.123456 (pm4py's OCDFG edge a->b: 7199.876544 seconds).
void classic_json_keeps_microseconds() {
  const auto file = parse_ocel_json(with_marker(""));
  assert(file.events.size() == 2);
  assert(file.events[0].timestamp == "2029-05-07T06:15:00.123456+00:00");
  assert(file.events[1].timestamp == "2029-05-07T08:15:00+00:00");
  // Nothing past the microsecond is left, so no marker is written.
  assert(serialize_ocel_json(file).find("minera:nanoseconds") == std::string::npos);

  // A document Minera wrote for a log read at nanoseconds keeps them, also
  // after another round trip.
  const auto transport = parse_ocel_json(with_marker("\"minera:nanoseconds\": true, "));
  assert(transport.events[0].timestamp == "2029-05-07T06:15:00.123456789Z");
  assert(transport.events[1].timestamp == "2029-05-07T08:15:00.000000001Z");
  const auto json = serialize_ocel_json(transport);
  assert(json.find("\"minera:nanoseconds\":true") != std::string::npos);
  const auto again = parse_ocel_json(json);
  assert(again.events[0].timestamp == "2029-05-07T06:15:00.123456789Z");
  // Files written for users carry no transport marker.
  assert(serialize_ocel_json(transport, OcelJsonFormat::kClassic, false).find("minera:nanoseconds") ==
         std::string::npos);
  expect_error([] { (void)parse_ocel_json(with_marker("\"minera:nanoseconds\": 1, ")); },
               "minera:nanoseconds must be boolean");
}

const std::string kXes = R"(<?xml version="1.0" encoding="UTF-8"?>
<log xes.version="1.0" xmlns="http://www.xes-standard.org/">
  <trace>
    <string key="concept:name" value="c1"/>
    <event><string key="concept:name" value="a"/><date key="time:timestamp" value="2020-01-01T10:00:00.000+00:00"/><string key="org:resource" value="R"/><float key="amount" value="1.5"/></event>
    <event><string key="concept:name" value="b"/><date key="time:timestamp" value="2020-01-01T12:00:00.000+00:00"/><string key="org:resource" value="S"/><float key="amount" value="2.5"/></event>
  </trace>
  <trace>
    <string key="concept:name" value="c2"/>
    <event><string key="concept:name" value="a"/><date key="time:timestamp" value="2020-01-02T09:00:00.000+00:00"/><string key="org:resource" value="S"/><float key="amount" value="3.5"/></event>
    <event><string key="concept:name" value="c"/><date key="time:timestamp" value="2020-01-02T09:30:00.000+00:00"/><string key="org:resource" value="R"/><float key="amount" value="4.5"/></event>
  </trace>
</log>
)";

struct Run {
  int status = 0;
  std::string output;
  std::string errors;
};

Run run_export(const fs::path& input, const std::vector<std::string>& arguments) {
  const auto stem = fs::temp_directory_path() / ("minera_review_ba1_" + std::to_string(std::rand()));
  const auto output = fs::path(stem.string() + ".json");
  const auto errors = fs::path(stem.string() + ".err");
  std::string command = "\"" + export_binary + "\" \"" + input.string() + "\"";
  for (const auto& argument : arguments) command += " \"" + argument + "\"";
  command += " -o \"" + output.string() + "\" 2> \"" + errors.string() + "\"";
#ifdef _WIN32
  // cmd /c strips the first and last quote of a line that starts with one.
  command = "\"" + command + "\"";
#endif
  Run run;
  run.status = std::system(command.c_str());
  const auto read = [](const fs::path& path) {
    std::ifstream file(path);
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
  };
  run.output = read(output);
  run.errors = read(errors);
  std::error_code ignored;
  fs::remove(output, ignored);
  fs::remove(errors, ignored);
  return run;
}

// The CLI read XES files as an event log, so the time statistics took the
// log path: a case key that is an event column grouped nothing, and a float
// start column was "invalid start timestamp" where pandas raises.
void export_cli_reads_the_xes_frame() {
  if (export_binary.empty()) return;
  const auto path = fs::temp_directory_path() / ("minera_review_ba1_" + std::to_string(std::rand()) + ".xes");
  {
    std::ofstream file(path);
    file << kXes;
  }
  // get_cases_description(read_xes(...), case_id_key="org:resource").
  const auto cases = run_export(path, {"--case-statistics", "--case-statistics-case-id-key", "org:resource"});
  assert(cases.status == 0);
  assert(cases.output.find("{\"case_id\":\"R\",\"start_time\":1577872800,\"end_time\":1577957400,"
                           "\"case_duration\":84600}") != std::string::npos);
  assert(cases.output.find("{\"case_id\":\"S\",\"start_time\":1577880000,\"end_time\":1577955600,"
                           "\"case_duration\":75600}") != std::string::npos);
  // cycle_time.apply with start_timestamp_key="amount".
  const auto cycle = run_export(path, {"--cycle-time", "--cycle-time-start-timestamp-key", "amount"});
  assert(cycle.status != 0);
  assert(cycle.errors.find("AttributeError: 'float' object has no attribute 'timestamp'") != std::string::npos);
  std::error_code ignored;
  fs::remove(path, ignored);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc > 1) export_binary = argv[1];
  activity_frequency_on_one_column();
  one_column_selections();
  case_bool_column_with_blank();
  passed_time_on_object_column();
  classic_json_keeps_microseconds();
  export_cli_reads_the_xes_frame();
  std::cout << "review BA1 tests passed\n";
  return 0;
}
