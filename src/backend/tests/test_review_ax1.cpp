// Review AX1: case statistics and passed time on uploads (pm4py's DataFrame
// path), a True/False column with a blank cell, and pandas' column
// subtraction for text and object columns. Values and messages are taken
// from pm4py 2.7.22 black-box on read_csv + format_dataframe(case_id="cid",
// activity_key="act", timestamp_key="ts") of the same text, or on read_xes.
#include "../frame_times.hpp"
#include "minera/case_statistics.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/json_export.hpp"
#include "minera/passed_time.hpp"
#include "minera/service_time.hpp"
#include "minera/xes_event_log_importer.hpp"

#include <cassert>
#include <cmath>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace minera;

namespace {

EventLog upload(const std::string& text) {
  std::istringstream csv(text);
  CsvImportOptions import;
  import.case_id_column = "cid";
  import.activity_column = "act";
  import.timestamp_column = "ts";
  import.resource_column = "who";
  auto log = import_csv_event_log(csv, import);
  assert(log.dataframe_columns);
  return log;
}

// The call fails with exactly `expected`.
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

const std::string kLog =
    "cid,act,ts,who,st,txt,flag,ok,num\n"
    "c1,a,2020-01-03 07:30:00.5,R,2020-01-03 07:00:00,,True,True,1\n"
    "c1,b,2020-01-03 07:50:00.123456,S,,y,,False,2\n"
    "c2,a,2020-01-01 02:10:00.123456,R,2020-01-01 02:00:00,z,False,True,3\n"
    "c2,b,2020-01-01 02:31:00.123456,R,2020-01-01 02:30:00,x,True,False,4\n"
    "c2,c,2020-01-01 03:00:00,S,2020-01-01 02:40:00,w,True,True,5\n"
    "c3,a,2020-01-02 09:00:00,S,,w,False,False,6\n"
    "c4,d,2020-01-04 10:00:00,R,,v,True,True,7\n"
    "c4,e,2020-01-04 11:00:00,R,,v,False,True,8\n";

const std::string kNoDt = "AttributeError: Can only use .dt accessor with datetimelike values";

std::vector<std::string> ids(const CaseStatisticsResult& result) {
  std::vector<std::string> out;
  for (const auto& entry : result.cases) out.push_back(entry.case_id);
  return out;
}

void case_statistics_on_uploads(const EventLog& log) {
  // Unless told otherwise the frame is sorted by the start time.
  const auto plain = discover_case_statistics(log, {});
  assert((ids(plain) == std::vector<std::string>{"c2", "c3", "c1", "c4"}));
  assert(plain.cases[0].case_duration == 2999.876544);
  assert(plain.options.sort_by == CaseStatisticsSortBy::kStartTime);

  // Any column groups the cases.
  CaseStatisticsOptions who;
  who.case_id_key = "who";
  const auto by_who = discover_case_statistics(log, who);
  assert((ids(by_who) == std::vector<std::string>{"R", "S"}));
  assert(by_who.cases[0].case_duration == 98999.5);
  assert(by_who.cases[1].case_duration == -82200.123456);
  who.sort_by = CaseStatisticsSortBy::kEndTime;
  who.sort_by_given = true;
  who.sort_ascending = false;
  assert((ids(discover_case_statistics(log, who)) == std::vector<std::string>{"R", "S"}));

  CaseStatisticsOptions grouper;
  grouper.case_id_key = "ts";
  grouper.timestamp_key = "ts";
  expect_error([&] { (void)discover_case_statistics(log, grouper); }, "ValueError: Grouper for 'ts' not 1-dimensional");

  const auto on = [&](const std::string& key, bool business) {
    CaseStatisticsOptions options;
    options.timestamp_key = key;
    options.use_business_hours = business;
    return [&log, options] { (void)discover_case_statistics(log, options); };
  };
  // A True/False column with a blank cell is an object column (AX1a).
  expect_error(on("flag", false), kNoDt);
  expect_error(on("ok", false),
               "TypeError: numpy boolean subtract, the `-` operator, is not supported, use the bitwise_xor, the `^` "
               "operator, or the logical_xor function instead.");
  expect_error(on("txt", false), "TypeError: unsupported operand type(s) for -: 'str' and 'str'");
  expect_error(on("num", true), "AttributeError: 'numpy.int64' object has no attribute 'replace'");
  expect_error(on("txt", true), "TypeError: str.replace() takes no keyword arguments");
  expect_error(on("flag", true), "AttributeError: 'bool' object has no attribute 'replace'");
  expect_error(on("st", true), "ValueError: cannot convert float NaN to integer");
}

PassedTimeResult passed(const EventLog& log, const std::string& activity, PassedTimeVariant variant,
                        const std::string& timestamp = "", const std::string& start = "", bool business = false) {
  PassedTimeOptions options;
  options.activity = activity;
  options.variant = variant;
  options.timestamp_key = timestamp;
  options.start_timestamp_key = start;
  options.use_business_hours = business;
  return discover_passed_time(log, options);
}

void passed_time_on_uploads(const EventLog& log) {
  const auto both = passed(log, "b", PassedTimeVariant::kPrePost);
  assert(both.pre.size() == 1 && both.pre[0].activity == "a" && both.pre[0].frequency == 2);
  assert(both.pre[0].performance == 1229.8117280000001 && both.pre_avg_perf == 1229.8117280000001);
  assert(both.post.size() == 1 && both.post[0].activity == "c" && both.post[0].performance == 1739.876544);

  // A start column raises each next start to the row's timestamp.
  const auto raised = passed(log, "b", PassedTimeVariant::kPrePost, "", "st");
  assert(raised.pre[0].performance == 599.938272 && raised.post[0].performance == 539.876544);
  const auto starts = passed(log, "a", PassedTimeVariant::kPost, "st");
  assert(starts.post.size() == 1 && starts.post[0].performance == 1800.0 && starts.post[0].frequency == 2);
  const auto hours = passed(log, "c", PassedTimeVariant::kPre, "", "", true);
  assert(hours.pre.size() == 1 && hours.pre[0].activity == "b" && hours.pre[0].performance == 0.0);

  // Pairs without both times have no mean: NaN, null in JSON.
  const auto missing = passed(log, "e", PassedTimeVariant::kPrePost, "st");
  assert(missing.pre.size() == 1 && missing.pre[0].activity == "d" && std::isnan(missing.pre[0].performance));
  assert(std::isnan(missing.pre_avg_perf) && missing.post.empty() && missing.post_avg_perf == 0.0);
  const auto json = passed_time_json(missing);
  assert(json.find("\"performance\":null") != std::string::npos);
  assert(json.find("\"pre_avg_perf\":null") != std::string::npos);

  expect_error([&] { (void)passed(log, "a", PassedTimeVariant::kPost, "txt"); },
               "TypeError: unsupported operand type(s) for -: 'str' and 'str'");
  expect_error([&] { (void)passed(log, "a", PassedTimeVariant::kPost, "nope"); }, "KeyError: 'nope'");
  expect_error([&] { (void)passed(log, "a", PassedTimeVariant::kPost, "ok", "st", true); },
               "TypeError: '>=' not supported between instances of 'NaTType' and 'bool'");
}

void service_time_messages(const EventLog& log) {
  const auto service = [&](const std::string& timestamp, const std::string& start) {
    ServiceTimeOptions options;
    options.timestamp_key = timestamp;
    options.start_timestamp_key = start;
    return [&log, options] { (void)discover_service_time(log, options); };
  };
  // A text column subtracts only the rows with both cells present.
  expect_error(service("txt", "who"), "TypeError: unsupported operand type(s) for -: 'str' and 'str'");
  expect_error(service("time:timestamp", "txt"), "TypeError: unsupported operand type(s) for -: 'Timestamp' and 'str'");
  expect_error(service("time:timestamp", "flag"),
               "TypeError: unsupported operand type(s) for -: 'Timestamp' and 'bool'");
  expect_error(service("st", "num"),
               "TypeError: Addition/subtraction of integers and integer-arrays with DatetimeArray is no longer "
               "supported.  Instead of adding/subtracting `n`, use `n * obj.freq`");
}

const std::string kXes = R"(<?xml version="1.0" encoding="UTF-8"?>
<log xes.version="1.0">
  <trace>
    <string key="concept:name" value="c1"/>
    <event>
      <string key="concept:name" value="a"/>
      <date key="time:timestamp" value="2020-01-01T10:00:00.000+00:00"/>
      <date key="mix" value="2020-01-01T09:00:00.000+00:00"/>
      <string key="txt" value="x"/>
    </event>
    <event>
      <string key="concept:name" value="b"/>
      <date key="time:timestamp" value="2020-01-01T11:00:00.000+00:00"/>
      <string key="mix" value="later"/>
    </event>
  </trace>
  <trace>
    <string key="concept:name" value="c2"/>
    <event>
      <string key="concept:name" value="a"/>
      <date key="time:timestamp" value="2020-01-02T10:00:00.000+00:00"/>
      <date key="mix" value="2020-01-02T09:30:00.000+00:00"/>
      <string key="txt" value="y"/>
    </event>
  </trace>
</log>
)";

void xes_object_columns() {
  std::istringstream input(kXes);
  const auto log = import_xes_dataframe_log(input);
  assert(log.dataframe_columns);
  const auto service = [&](const std::string& timestamp, const std::string& start) {
    ServiceTimeOptions options;
    options.timestamp_key = timestamp;
    options.start_timestamp_key = start;
    return [&log, options] { (void)discover_service_time(log, options); };
  };
  const std::string prefix = "TypeError: unsupported operand type(s) for -: ";
  expect_error(service("time:timestamp", "mix"), prefix + "'Timestamp' and 'str'");
  expect_error(service("mix", "txt"), prefix + "'datetime.datetime' and 'str'");
  expect_error(service("txt", "mix"), prefix + "'str' and 'datetime.datetime'");
  expect_error(service("mix", "time:timestamp"), "TypeError: cannot subtract DatetimeArray from ndarray[object]");
  expect_error(service("txt", "concept:name"), prefix + "'str' and 'str'");

  const auto statistics = [&](const std::string& key, bool business) {
    CaseStatisticsOptions options;
    options.timestamp_key = key;
    options.use_business_hours = business;
    return [&log, options] { (void)discover_case_statistics(log, options); };
  };
  expect_error(statistics("mix", true), "TypeError: str.replace() takes no keyword arguments");
  expect_error(statistics("mix", false), prefix + "'str' and 'datetime.datetime'");
  expect_error(statistics("txt", false), prefix + "'str' and 'str'");

  expect_error([&] { (void)passed(log, "a", PassedTimeVariant::kPost, "txt", "mix"); }, prefix + "'str' and 'str'");
  expect_error([&] { (void)passed(log, "a", PassedTimeVariant::kPost, "mix", "txt"); },
               "TypeError: '>=' not supported between instances of 'float' and 'datetime.datetime'");
  expect_error([&] { (void)passed(log, "a", PassedTimeVariant::kPost, "", "mix"); },
               "TypeError: '>=' not supported between instances of 'str' and 'Timestamp'");
}

// pandas' subtraction of two columns (each checked against `a - b` there).
void column_subtraction_model() {
  using Type = DataFrameColumnType;
  using Cells = std::vector<std::optional<python::Scalar>>;
  const auto text = [](const char* value) { return std::optional<python::Scalar>(python::text(value)); };
  const std::optional<python::Scalar> none;
  const auto real = [](double value) { return std::optional<python::Scalar>(python::real(value)); };
  const auto error = [](Type a, const Cells& left, Type b, const Cells& right) {
    return frame_times::column_subtraction(a, left, b, right).error.value_or("OK");
  };
  const std::string prefix = "unsupported operand type(s) for -: ";
  // Text columns skip the rows with a missing cell; none left is no error.
  assert(error(Type::kString, {none, text("y")}, Type::kString, {text("a"), text("b")}) == prefix + "'str' and 'str'");
  assert(error(Type::kString, {none, text("y")}, Type::kFloat64, {real(1.5), real(2.5)}) == prefix + "'str' and 'float'");
  assert(error(Type::kString, {text("x"), none}, Type::kString, {none, text("b")}) == "OK");
  // An object column's cell-by-cell subtraction, retried on present rows.
  assert(error(Type::kObject, {text("x"), real(1.0)}, Type::kFloat64, {none, real(2.0)}) == "OK");
  assert(error(Type::kObject, {real(1.0), text("x")}, Type::kFloat64, {none, real(2.0)}) == prefix + "'str' and 'float'");
  // A date column less a text column: one present row is the whole array.
  ParsedExactTimestamp date;
  date.seconds = 1577836800;
  date.timezone_aware = true;
  python::Scalar instant;
  instant.kind = python::Scalar::Kind::kDate;
  instant.date = date;
  const std::optional<python::Scalar> stamp = instant;
  assert(error(Type::kDate, {stamp, none}, Type::kString, {text("a"), text("b")}) == prefix + "'DatetimeArray' and 'str'");
  assert(error(Type::kDate, {stamp, stamp}, Type::kString, {text("a"), text("b")}) == prefix + "'Timestamp' and 'str'");
  assert(error(Type::kDate, {none, stamp}, Type::kFloat64, {real(1.0), real(2.0)}) == prefix + "'NaTType' and 'float'");
  assert(error(Type::kString, {text("x")}, Type::kDate, {stamp}) == "cannot subtract DatetimeArray from ndarray[object]");
  assert(frame_times::column_subtraction(Type::kDate, {stamp}, Type::kDate, {none}).timedelta);
}

}  // namespace

int main() {
  const auto log = upload(kLog);
  case_statistics_on_uploads(log);
  passed_time_on_uploads(log);
  service_time_messages(log);
  xes_object_columns();
  column_subtraction_model();
  std::cout << "review AX1 tests passed\n";
  return 0;
}
