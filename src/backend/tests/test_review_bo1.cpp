// Review BO1: a CSV upload's frame as pm4py's format_dataframe gives it, read
// through Minera's batch CSV import. The frame's copies concept:name,
// time:timestamp and case:concept:name replace the CSV's own columns of those
// names (the columns order and the events' attributes); a date column is
// datetime64[ns, UTC] when a cell below the microsecond sits in a row the
// frame then drops; the batches' log variant on such a log reads the frame's
// typed cells (a blank resource is NaN, a numeric one with a gap a float, a
// number column as the activity key a float) and the frame's keys (a key
// the frame lacks raises KeyError). Values are taken from pm4py 2.7.22 black-box on the
// same inputs (review-harness round-bo, bo1/mk.py).
#include "minera/batches.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/dataframe_columns.hpp"
#include "minera/event_log.hpp"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace minera;

namespace {

int check(const std::string& label, const std::string& seen, const std::string& expected) {
  if (seen == expected) return 0;
  std::cerr << label << ":\n  seen     " << seen << "\n  expected " << expected << "\n";
  return 1;
}

CsvEventData import(const std::string& text) {
  CsvImportOptions options;
  options.case_id_column = "case";
  options.activity_column = "act";
  options.timestamp_column = "ts";
  options.resource_column = "res";
  std::istringstream input(text);
  return import_csv_event_data(input, options);
}

std::string joined(const std::vector<std::string>& values) {
  std::string out;
  for (const auto& value : values) out += (out.empty() ? "" : ",") + value;
  return out;
}

// (activity, resource, batch count) of each group, or the error.
std::string groups(const std::string& text, const std::string& activity, const std::string& resource) {
  const auto data = import(text);
  BatchDiscoveryOptions options;
  options.merge_distance_seconds = 10;
  options.activity_attribute = activity;
  options.resource_attribute = resource;
  options.timestamp_attribute = "ts";
  options.case_id_attribute = "case";
  try {
    const auto discovery = discover_batches(data.log, options);
    std::string out;
    for (const auto& group : discovery.groups)
      out += "(" + group.activity + "," + group.resource + "," + std::to_string(group.batch_count) + ")";
    return out;
  } catch (const std::exception& error) {
    return std::string("error: ") + error.what();
  }
}

}  // namespace

int main() {
  int failures = 0;
  {
    const auto data = import(
        "case,act,ts,concept:name,time:timestamp,res\n"
        "c1,a,2020-01-01 00:00:05,X,2021-01-01 00:00:00,r1\n"
        "c1,b,2020-01-01 00:00:01,Y,2021-01-01 00:00:00,r1\n"
        "c2,a,2020-01-01 00:00:03,Z,,r2\n"
        ",a,2020-01-01 00:00:04.123456789,W,,r2\n");
    const auto& frame = *data.log.dataframe_columns;
    failures += check("frame columns", joined(frame.order),
                      "case,act,ts,res,case:concept:name,concept:name,time:timestamp,@@index,@@case_index");
    bool nanoseconds = false;
    for (const auto& column : frame.columns)
      if (column.key == "ts") nanoseconds = column.nanoseconds;
    failures += check("ts dtype", nanoseconds ? "datetime64[ns, UTC]" : "datetime64[us, UTC]", "datetime64[ns, UTC]");
    std::string rows;
    for (const auto& trace : data.log.traces)
      for (const auto& event : trace.events) {
        rows += "(" + trace.case_id + "," + event.activity + "," + event.timestamp;
        for (const auto& attribute : event.attributes)
          if (attribute.key == "concept:name" || attribute.key == "time:timestamp") rows += "," + attribute.key;
        rows += ")";
      }
    failures += check("frame rows", rows,
                      "(c1,b,2020-01-01 00:00:01)(c1,a,2020-01-01 00:00:05)(c2,a,2020-01-01 00:00:03)");
  }
  const std::string blank =
      "case,act,ts,res\nc1,a,2020-01-01 00:00:00,\nc2,a,2020-01-01 00:00:05,\n"
      "c3,a,2020-01-01 00:00:30,r1\nc4,a,2020-01-01 00:00:32,r1\n";
  failures += check("blank resource tie", groups(blank, "act", "res"),
                    "error: batches: '<' not supported between instances of 'float' and 'str'");
  failures += check("blank resource group",
                    groups(blank + "c5,a,2020-01-01 00:01:30,\nc6,a,2020-01-01 00:01:32,\n", "act", "res"),
                    "(a,nan,2)(a,r1,1)");
  failures += check("float resource",
                    groups("case,act,ts,res\nc1,a,2020-01-01 00:00:00,\nc2,a,2020-01-01 00:00:05,\n"
                           "c3,a,2020-01-01 00:00:30,1\nc4,a,2020-01-01 00:00:32,1\n", "act", "res"),
                    "(a,1.0,1)");
  const std::string numbers =
      "case,act,ts,res,x\nc1,a,2020-01-01 00:00:00,r,1.5\nc2,a,2020-01-01 00:00:05,r,1.5\n"
      "c3,a,2020-01-01 00:00:30,r,2\nc4,a,2020-01-01 00:00:32,r,2\n"
      "c5,a,2020-01-01 00:01:30,r,10\nc6,a,2020-01-01 00:01:32,r,10\n";
  failures += check("number activity key", groups(numbers, "x", "res"), "(10.0,r,1)(2.0,r,1)(1.5,r,1)");
  failures += check("frame key", groups(numbers, "act", "y"), "error: KeyError: 'y'");
  // Minera's default org:resource names the selected resource column
  // (declared: pm4py's frame has no org:resource column, KeyError).
  failures += check("org:resource names the selected column",
                    groups("case,act,ts,res\nc1,a,2020-01-01 00:00:00,\nc2,a,2020-01-01 00:00:05,\n"
                           "c3,a,2020-01-01 00:00:30,1\nc4,a,2020-01-01 00:00:32,1\n", "act", "org:resource"),
                    "(a,1.0,1)");
  if (failures) return 1;
  std::cout << "review BO1 tests passed\n";
  return 0;
}
