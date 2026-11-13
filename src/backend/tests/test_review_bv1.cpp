// Review BV1: the LLM context with a timestamp attribute on a CSV upload's
// frame, and the error texts of the trace and start-activity log filters.
// pm4py.llm with timestamp_key on the DataFrame: the frame's own timestamp
// (time:timestamp or the CSV's column) describes as no key does; the column
// and features descriptions never read the key; the log skeleton, Declare
// and temporal profile check it as a DataFrame timestamp column (after the
// activity column) and the temporal profile times the pairs by it; the DFG
// and variants order and time each case by a date column, and the event
// stream sorts the rows by the column whatever its type. The trace filter
// reads every event's value before joining them (KeyError first, then the
// join's TypeError); the start filter reads trace[0][key]. Values are taken
// from pm4py 2.7.22 black-box on the same input (review-harness round-bv,
// bv1/mk.py).
#include "minera/csv_event_log_importer.hpp"
#include "minera/llm_context.hpp"
#include "minera/log_boundary_filter.hpp"
#include "minera/log_trace_filter.hpp"

#include <functional>
#include <iostream>
#include <sstream>
#include <string>

using namespace minera;

namespace {

int failures = 0;

void check(const std::string& label, const std::string& seen, const std::string& expected) {
  if (seen == expected) return;
  ++failures;
  std::cerr << label << ":\n  seen     " << seen << "\n  expected " << expected << "\n";
}

void check_error(const std::string& label, const std::function<void()>& run, const std::string& message) {
  try {
    run();
  } catch (const std::exception& error) {
    if (std::string(error.what()) == message) return;
    ++failures;
    std::cerr << label << ":\n  error    " << error.what() << "\n  expected " << message << "\n";
    return;
  }
  ++failures;
  std::cerr << label << ": no error, expected " << message << "\n";
}

const std::string kCsv =
    "case,act,ts,n,t,d2,d3\n"
    "c1,a,2020-01-01 00:00:00,3,x,2021-03-01 10:00:00,2021-03-01\n"
    "c1,b,2020-01-01 00:01:00,1,y,2021-03-01 09:00:00,\n"
    "c1,c,2020-01-01 00:03:00,2,x,2021-03-02 08:30:00,2021-03-02\n"
    "c2,a,2020-01-01 00:00:00,1,y,2021-03-01 12:00:00,2021-03-01\n"
    "c2,b,2020-01-01 00:02:00,2,x,2021-03-01 12:00:00,2021-03-03\n"
    "c3,b,2020-01-01 00:05:00,5,y,2021-03-05 00:00:00,2021-03-05\n";

EventLog import() {
  CsvImportOptions options;
  options.case_id_column = "case";
  options.activity_column = "act";
  options.timestamp_column = "ts";
  std::istringstream input(kCsv);
  return import_csv_event_data(input, options).log;
}

std::string context(const EventLog& log, const std::string& variant, const std::string& timestamp,
                    const std::string& activity = {}) {
  LlmContextOptions options;
  options.variant = variant;
  options.activity_attribute = activity;
  options.timestamp_attribute = timestamp;
  options.include_header = false;
  options.max_len = 100000;
  return build_llm_context(log, options).text;
}

EventAttribute integer(const std::string& key, const std::string& value) {
  return EventAttribute{key, value, AttributeKind::kInt};
}

}  // namespace

int main() {
  const auto log = import();
  const std::string dfg = "\n\na -> b ( frequency = 2  performance = 90.000 )\nb -> c ( frequency = 1  performance = 120.000 )\n\n\n";
  check("DFG without a timestamp attribute", context(log, "dfg", ""), dfg);
  check("DFG timed by the CSV's own timestamp column", context(log, "dfg", "ts"), dfg);
  check("DFG timed by time:timestamp", context(log, "dfg", "time:timestamp"), dfg);
  check("DFG timed by another date column", context(log, "dfg", "d2"),
        "\n\nb -> a ( frequency = 1  performance = 3600.000 )\na -> c ( frequency = 1  performance = 81000.000 )\n"
        "a -> b ( frequency = 1  performance = 0.000 )\n\n\n");
  check("DFG keyed by a column and timed by a date column", context(log, "dfg", "d2", "t"),
        "\n\ny -> x ( frequency = 2  performance = 1800.000 )\nx -> x ( frequency = 1  performance = 81000.000 )\n\n\n");
  check("variants timed by a date column", context(log, "variants", "d2"),
        "\n\n a -> b -> c ( frequency = 1  performance = 84600.000 )\n b ( frequency = 1  performance = 0.000 )\n"
        " a -> b ( frequency = 1  performance = 0.000 )\n\n\n");
  check("event stream sorted by an int column", context(log, "event_stream", "n"),
        "\n\n"
        "{'case': 'c2', 'act': 'a', 'ts': Timestamp('2020-01-01 00:00:00+0000', tz='UTC'), 'n': 1, 't': 'y', 'd2': Timestamp('2021-03-01 12:00:00+0000', tz='UTC'), 'd3': Timestamp('2021-03-01 00:00:00+0000', tz='UTC'), 'case:concept:name': 'c2', 'concept:name': 'a', 'time:timestamp': Timestamp('2020-01-01 00:00:00+0000', tz='UTC'), '@@index': 3, '@@case_index': 1}\n"
        "{'case': 'c1', 'act': 'b', 'ts': Timestamp('2020-01-01 00:01:00+0000', tz='UTC'), 'n': 1, 't': 'y', 'd2': Timestamp('2021-03-01 09:00:00+0000', tz='UTC'), 'case:concept:name': 'c1', 'concept:name': 'b', 'time:timestamp': Timestamp('2020-01-01 00:01:00+0000', tz='UTC'), '@@index': 1, '@@case_index': 0}\n"
        "{'case': 'c2', 'act': 'b', 'ts': Timestamp('2020-01-01 00:02:00+0000', tz='UTC'), 'n': 2, 't': 'x', 'd2': Timestamp('2021-03-01 12:00:00+0000', tz='UTC'), 'd3': Timestamp('2021-03-03 00:00:00+0000', tz='UTC'), 'case:concept:name': 'c2', 'concept:name': 'b', 'time:timestamp': Timestamp('2020-01-01 00:02:00+0000', tz='UTC'), '@@index': 4, '@@case_index': 1}\n"
        "{'case': 'c1', 'act': 'c', 'ts': Timestamp('2020-01-01 00:03:00+0000', tz='UTC'), 'n': 2, 't': 'x', 'd2': Timestamp('2021-03-02 08:30:00+0000', tz='UTC'), 'd3': Timestamp('2021-03-02 00:00:00+0000', tz='UTC'), 'case:concept:name': 'c1', 'concept:name': 'c', 'time:timestamp': Timestamp('2020-01-01 00:03:00+0000', tz='UTC'), '@@index': 2, '@@case_index': 0}\n"
        "{'case': 'c1', 'act': 'a', 'ts': Timestamp('2020-01-01 00:00:00+0000', tz='UTC'), 'n': 3, 't': 'x', 'd2': Timestamp('2021-03-01 10:00:00+0000', tz='UTC'), 'd3': Timestamp('2021-03-01 00:00:00+0000', tz='UTC'), 'case:concept:name': 'c1', 'concept:name': 'a', 'time:timestamp': Timestamp('2020-01-01 00:00:00+0000', tz='UTC'), '@@index': 0, '@@case_index': 0}\n"
        "{'case': 'c3', 'act': 'b', 'ts': Timestamp('2020-01-01 00:05:00+0000', tz='UTC'), 'n': 5, 't': 'y', 'd2': Timestamp('2021-03-05 00:00:00+0000', tz='UTC'), 'd3': Timestamp('2021-03-05 00:00:00+0000', tz='UTC'), 'case:concept:name': 'c3', 'concept:name': 'b', 'time:timestamp': Timestamp('2020-01-01 00:05:00+0000', tz='UTC'), '@@index': 5, '@@case_index': 2}\n"
        "\n");
  check_error("event stream sorted by a column with a gap", [&] { (void)context(log, "event_stream", "d3"); },
              "KeyError: 'd3'");
  check("temporal profile timed by a date column", context(log, "temporal_profile", "d2"),
        "\n\na -> b :  AVG: 0.00 s  STD: 0.00 s\na -> c :  AVG: 81000.00 s  STD: 0.00 s\nb -> c :  AVG: 84600.00 s  STD: 0.00 s");
  check("temporal profile keyed by a column and timed by a date column", context(log, "temporal_profile", "d2", "t"),
        "\n\nx -> x :  AVG: 81000.00 s  STD: 0.00 s\ny -> x :  AVG: 42300.00 s  STD: 59821.23 s");
  check_error("log skeleton timed by a date column with a gap", [&] { (void)context(log, "log_skeleton", "d3"); },
              "the timestamp column should not contain any empty value.");
  check_error("log skeleton timed by an int column", [&] { (void)context(log, "log_skeleton", "n"); },
              "the timestamp column should be of time datetime. Use the function pandas.to_datetime");
  check_error("Declare timed by a column the frame lacks", [&] { (void)context(log, "declare", "nope"); },
              "the specified timestamp column is not contained in the dataframe. Available columns: ['@@case_index', "
              "'@@index', 'act', 'case', 'case:concept:name', 'concept:name', 'd2', 'd3', 'n', 't', 'time:timestamp', 'ts']");
  check_error("Declare checks the activity column first", [&] { (void)context(log, "declare", "nope", "n"); },
              "the activity column should be of type string.");
  check_error("DFG timed by an int column", [&] { (void)context(log, "dfg", "n"); },
              "AttributeError: Can only use .dt accessor with datetimelike values");
  check_error("DFG timed by a column the frame lacks", [&] { (void)context(log, "dfg", "nope"); }, "KeyError: 'nope'");
  check_error("variants timed by an int column", [&] { (void)context(log, "variants", "n"); },
              "AttributeError: 'int' object has no attribute 'timestamp'");
  check_error("variants timed by a column the frame lacks", [&] { (void)context(log, "variants", "nope"); },
              "KeyError: 'Column not found: nope'");
  check("log attributes ignore the timestamp attribute", context(log, "log_attributes", "n"), context(log, "log_attributes", ""));

  // The trace filter reads every value of a trace before the join.
  EventLog events;
  Trace trace;
  trace.case_id = "c1";
  Event first, second;
  first.attributes = {integer("k", "1"), integer("k2", "1")};
  second.attributes = {EventAttribute{"k2", "b"}};
  trace.events = {first, second};
  events.traces = {trace};
  check_error("trace filter: a missing value raises KeyError before the join",
              [&] { (void)filter_log_traces(events, {{"a"}}, LogTraceFilterOptions{true, "k"}); }, "'k'");
  check_error("trace filter: the join's TypeError names the first value that is no str",
              [&] { (void)filter_log_traces(events, {{"a"}}, LogTraceFilterOptions{true, "k2"}); },
              "sequence item 0: expected str instance, int found");
  check_error("start filter: a missing value raises KeyError",
              [&] { (void)filter_log_start_activities(events, {"a"}, LogBoundaryFilterOptions{"missing", true}); },
              "'missing'");

  if (failures) {
    std::cerr << failures << " review BV1 check(s) failed\n";
    return 1;
  }
  std::cout << "review BV1 checks passed\n";
  return 0;
}
