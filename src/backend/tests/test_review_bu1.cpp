// Review BU1: the LLM model descriptions (log skeleton, Declare, temporal
// profile) keyed by a column of a CSV upload's frame (the case id column
// too), and the OCEL events and objects text and object features on OCEL
// JSON. pm4py.discover_* on the upload's frame check the key as a DataFrame
// activity column (the column, then a str dtype, then no gap); the temporal
// profile keyed by the case column fails on its merge.
// pm4py.llm.abstract_ocel prints read_ocel_json's UTC microseconds as
// str(Timestamp), orders the events by timestamp, activity and id, and joins
// the object lines without a final newline; abstract_ocel_features describes
// the lifecycle paths by default. Values are taken from pm4py 2.7.22
// black-box on the same input (review-harness round-bu, bu1/mk.py).
#include "minera/csv_event_log_importer.hpp"
#include "minera/llm_context.hpp"
#include "minera/ocel.hpp"

#include <algorithm>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace minera;

namespace {

int failures = 0;

void check(const std::string& label, const std::string& seen, const std::string& expected) {
  if (seen == expected) return;
  ++failures;
  std::cerr << label << ":\n  seen     " << seen << "\n  expected " << expected << "\n";
}

// The relation pairs print in Python set order, which follows the hash seed:
// sort the pairs of each relation line on both sides (and Declare's
// constraint lines, which come from a set too).
std::string sorted_pairs(const std::string& text, bool sort_lines) {
  std::istringstream lines(text);
  std::vector<std::string> kept;
  std::string line;
  while (std::getline(lines, line)) {
    const auto open = line.find(" (");
    if (open != std::string::npos && line.ends_with(")")) {
      std::vector<std::string> pairs;
      std::size_t at = open + 1;
      while (at < line.size()) {
        const auto close = line.find(')', at);
        pairs.push_back(line.substr(at, close + 1 - at));
        at = line.find('(', close);
        if (at == std::string::npos) break;
      }
      const bool commas = line.find("), (") != std::string::npos;
      std::sort(pairs.begin(), pairs.end());
      line.resize(open + 1);
      for (std::size_t i = 0; i < pairs.size(); ++i) line += (i ? (commas ? ", " : " ") : "") + pairs[i];
    }
    kept.push_back(line);
  }
  if (sort_lines) std::sort(kept.begin(), kept.end());
  std::string out;
  for (const auto& kept_line : kept) out += kept_line + "\n";
  return out;
}

void check_sets(const std::string& label, const std::string& seen, const std::string& expected,
                bool sort_lines = false) {
  check(label, sorted_pairs(seen, sort_lines), sorted_pairs(expected, sort_lines));
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
    "case,act,ts,n,t,d\n"
    "c1,a,2020-01-01 00:00:00,1,x,2021-03-01\n"
    "c1,b,2020-01-01 00:01:00,007,,2021-03-02\n"
    "c1,c,2020-01-01 00:03:00,2,y,\n"
    "c1,a,2020-01-01 00:04:00,1,x,2021-03-01\n"
    "c2,a,2020-01-01 00:00:00,2,x,2021-03-01\n"
    "c2,b,2020-01-01 00:02:00,1,y,2021-03-02\n";

EventLog import() {
  CsvImportOptions options;
  options.case_id_column = "case";
  options.activity_column = "act";
  options.timestamp_column = "ts";
  std::istringstream input(kCsv);
  return import_csv_event_data(input, options).log;
}

std::string context(const EventLog& log, const std::string& variant, const std::string& key) {
  LlmContextOptions options;
  options.variant = variant;
  options.activity_attribute = key;
  options.include_header = false;
  options.max_len = 100000;
  return build_llm_context(log, options).text;
}

}  // namespace

int main() {
  const auto log = import();
  check_sets("log skeleton keyed by the CSV's activity column", context(log, "log_skeleton", "act"),
    "\n"
    "Equivalence:  ('c', 'b')\n"
    "\n"
    "Always Before:  ('c', 'b') ('c', 'a') ('b', 'a')\n"
    "\n"
    "Always After:  ('c', 'a')\n"
    "\n"
    "Never Together: \n"
    "\n"
    "Activity Occurrences:  a: 1, 2; b: 1; c: 0, 1;\n"
    "\n"
    "Directly-Follows Constraints:  ('c', 'a')\n"
    "\n");
  check_sets("log skeleton keyed by the case id column", context(log, "log_skeleton", "case:concept:name"),
    "\n"
    "Equivalence: \n"
    "\n"
    "Always Before: \n"
    "\n"
    "Always After: \n"
    "\n"
    "Never Together:  ('c1', 'c2') ('c2', 'c1')\n"
    "\n"
    "Activity Occurrences:  c1: 0, 4; c2: 0, 2;\n"
    "\n"
    "Directly-Follows Constraints: \n"
    "\n");
  check_sets("declare keyed by the case id column", context(log, "declare", "case:concept:name"),
    "\n"
    "These are the constraints of the model:\n"
    "Non Co-Existence: ('c1', 'c2'), ('c2', 'c1')\n"
    "Non Succession: ('c1', 'c2'), ('c2', 'c1')\n"
    "Non Chain succession: ('c1', 'c2'), ('c2', 'c1')\n",
    true);
  check("temporal profile keyed by the CSV's case column", context(log, "temporal_profile", "case"),
    "\n"
    "\n"
    "c1 -> c1 :  AVG: 140.00 s  STD: 72.66 s\n"
    "c2 -> c2 :  AVG: 120.00 s  STD: 0.00 s");
  for (const std::string variant : {"log_skeleton", "declare", "temporal_profile"}) {
    check_error(variant + " keyed by an int column", [&] { (void)context(log, variant, "n"); },
                "the activity column should be of type string.");
    check_error(variant + " keyed by a date column", [&] { (void)context(log, variant, "d"); },
                "the activity column should be of type string.");
    check_error(variant + " keyed by a text column with a gap", [&] { (void)context(log, variant, "t"); },
                "the activity column should not contain any empty value.");
    check_error(variant + " keyed by no column", [&] { (void)context(log, variant, "zz"); },
                "the specified activity column is not contained in the dataframe. Available columns: "
                "['@@case_index', '@@index', 'act', 'case', 'case:concept:name', 'concept:name', 'd', 'n', 't', "
                "'time:timestamp', 'ts']");
  }
  check_error("temporal profile keyed by the case id column",
              [&] { (void)context(log, "temporal_profile", "case:concept:name"); },
              "KeyError: \"['case:concept:name_2'] not in index\"");

  const auto ocel = parse_ocel_json("{\"ocel:global-event\": {\"ocel:activity\": \"__INVALID__\"}, \"ocel:global-object\": {\"ocel:type\": \"__INVALID__\"}, \"ocel:global-log\": {\"ocel:object-types\": [\"t\"], \"ocel:attribute-names\": [], \"ocel:version\": \"1.0\", \"ocel:ordering\": \"timestamp\"}, \"ocel:events\": {\"e3\": {\"ocel:activity\": \"b\", \"ocel:timestamp\": \"2020-01-01T00:00:01.000000600Z\", \"ocel:vmap\": {}, \"ocel:omap\": [\"o2\"]}, \"e1\": {\"ocel:activity\": \"b\", \"ocel:timestamp\": \"2020-01-01T00:00:00Z\", \"ocel:vmap\": {}, \"ocel:omap\": [\"o1\", \"o2\"]}, \"e2\": {\"ocel:activity\": \"a\", \"ocel:timestamp\": \"2020-01-01T02:00:01+02:00\", \"ocel:vmap\": {}, \"ocel:omap\": [\"o1\"]}, \"e4\": {\"ocel:activity\": \"c\", \"ocel:timestamp\": \"2020-01-01T00:00:02.25Z\", \"ocel:vmap\": {}, \"ocel:omap\": [\"o1\", \"o2\"]}}, \"ocel:objects\": {\"o1\": {\"ocel:type\": \"t\", \"ocel:ovmap\": {}}, \"o2\": {\"ocel:type\": \"t\", \"ocel:ovmap\": {}}}}");
  LlmContextOptions events;
  events.variant = "ocel";
  events.max_len = 100000;
  check("OCEL events and objects", build_llm_context(ocel, events).text,
    "\n"
    "\n"
    "events:\n"
    "\n"
    "b ( related objects: o1, o2 )  timestamp: 2020-01-01 00:00:00+00:00\n"
    "b ( related objects: o2 )  timestamp: 2020-01-01 00:00:01+00:00\n"
    "c ( related objects: o1, o2 )  timestamp: 2020-01-01 00:00:02.250000+00:00\n"
    "a ( related objects: o1 )  timestamp: 2020-01-01 02:00:01+00:00\n"
    "\n"
    "objects:\n"
    "\n"
    "o1 object type: t ( lifecycle start: 2020-01-01 00:00:00+00:00 ; lifecycle end: 2020-01-01 02:00:01+00:00 ; lifecycle duration: 7201.0 )\n"
    "o2 object type: t ( lifecycle start: 2020-01-01 00:00:00+00:00 ; lifecycle end: 2020-01-01 00:00:02.250000+00:00 ; lifecycle duration: 2.25 )");
  LlmContextOptions features;
  features.variant = "ocel_features";
  features.object_type = "t";
  features.include_header = false;
  features.max_len = 100000;
  check("OCEL object features with the lifecycle paths", build_llm_context(ocel, features).text,
    "\n"
    "Duration of the lifecycle of the object:    number of non-zero values: 2 ; quantiles of the non-zero: {0.0: 2.25, 0.25: 1801.9375, 0.5: 3601.625, 0.75: 5401.3125, 1.0: 7201.0}\n"
    "Number of occurrences of the activity b:    number of non-zero values: 2 ; quantiles of the non-zero: {0.0: 1.0, 0.25: 1.25, 0.5: 1.5, 0.75: 1.75, 1.0: 2.0}\n"
    "Number of unique activities in the lifecycle of the object:    number of non-zero values: 2 ; quantiles of the non-zero: {0.0: 2.0, 0.25: 2.25, 0.5: 2.5, 0.75: 2.75, 1.0: 3.0}\n"
    "Completion timestamp of the lifecycle of the object:    number of non-zero values: 2 ; quantiles of the non-zero: {0.0: 1577836802.25, 0.25: 1577838601.9375, 0.5: 1577840401.625, 0.75: 1577842201.3125, 1.0: 1577844001.0}\n"
    "Start timestamp of the lifecycle of the object:    number of non-zero values: 2 ; quantiles of the non-zero: {0.0: 1577836800.0, 0.25: 1577836800.0, 0.5: 1577836800.0, 0.75: 1577836800.0, 1.0: 1577836800.0}\n"
    "Number of occurrences of the activity c:    number of non-zero values: 2 ; quantiles of the non-zero: {0.0: 1.0, 0.25: 1.0, 0.5: 1.0, 0.75: 1.0, 1.0: 1.0}\n"
    "Number of objects starting their lifecycle together with the current object:    number of non-zero values: 2 ; quantiles of the non-zero: {0.0: 1.0, 0.25: 1.0, 0.5: 1.0, 0.75: 1.0, 1.0: 1.0}\n"
    "Number of objects related in the object interaction graph:    number of non-zero values: 2 ; quantiles of the non-zero: {0.0: 1.0, 0.25: 1.0, 0.5: 1.0, 0.75: 1.0, 1.0: 1.0}\n"
    "Number of object of type t related to the current object in the object interaction graph:    number of non-zero values: 2 ; quantiles of the non-zero: {0.0: 1.0, 0.25: 1.0, 0.5: 1.0, 0.75: 1.0, 1.0: 1.0}\n"
    "Number of events in the lifecycle of the object:    number of non-zero values: 2 ; quantiles of the non-zero: {0.0: 3.0, 0.25: 3.0, 0.5: 3.0, 0.75: 3.0, 1.0: 3.0}\n"
    "Frequency of the path \"b\" -> \"c\" in the lifecycle of the object:    number of non-zero values: 2 ; quantiles of the non-zero: {0.0: 1.0, 0.25: 1.0, 0.5: 1.0, 0.75: 1.0, 1.0: 1.0}\n"
    "Degree centrality of the object in the object interaction graph:    number of non-zero values: 2 ; quantiles of the non-zero: {0.0: 1.0, 0.25: 1.0, 0.5: 1.0, 0.75: 1.0, 1.0: 1.0}\n"
    "Number of occurrences of the activity a:    number of non-zero values: 1 ; quantiles of the non-zero: {0.0: 1.0, 0.25: 1.0, 0.5: 1.0, 0.75: 1.0, 1.0: 1.0}\n"
    "Frequency of the path \"c\" -> \"a\" in the lifecycle of the object:    number of non-zero values: 1 ; quantiles of the non-zero: {0.0: 1.0, 0.25: 1.0, 0.5: 1.0, 0.75: 1.0, 1.0: 1.0}\n"
    "Frequency of the path \"b\" -> \"b\" in the lifecycle of the object:    number of non-zero values: 1 ; quantiles of the non-zero: {0.0: 1.0, 0.25: 1.0, 0.5: 1.0, 0.75: 1.0, 1.0: 1.0}\n");
  features.include_lifecycle_paths = false;
  check("OCEL object features without the lifecycle paths", build_llm_context(ocel, features).text,
    "\n"
    "Duration of the lifecycle of the object:    number of non-zero values: 2 ; quantiles of the non-zero: {0.0: 2.25, 0.25: 1801.9375, 0.5: 3601.625, 0.75: 5401.3125, 1.0: 7201.0}\n"
    "Number of occurrences of the activity b:    number of non-zero values: 2 ; quantiles of the non-zero: {0.0: 1.0, 0.25: 1.25, 0.5: 1.5, 0.75: 1.75, 1.0: 2.0}\n"
    "Number of unique activities in the lifecycle of the object:    number of non-zero values: 2 ; quantiles of the non-zero: {0.0: 2.0, 0.25: 2.25, 0.5: 2.5, 0.75: 2.75, 1.0: 3.0}\n"
    "Completion timestamp of the lifecycle of the object:    number of non-zero values: 2 ; quantiles of the non-zero: {0.0: 1577836802.25, 0.25: 1577838601.9375, 0.5: 1577840401.625, 0.75: 1577842201.3125, 1.0: 1577844001.0}\n"
    "Start timestamp of the lifecycle of the object:    number of non-zero values: 2 ; quantiles of the non-zero: {0.0: 1577836800.0, 0.25: 1577836800.0, 0.5: 1577836800.0, 0.75: 1577836800.0, 1.0: 1577836800.0}\n"
    "Number of occurrences of the activity c:    number of non-zero values: 2 ; quantiles of the non-zero: {0.0: 1.0, 0.25: 1.0, 0.5: 1.0, 0.75: 1.0, 1.0: 1.0}\n"
    "Number of objects starting their lifecycle together with the current object:    number of non-zero values: 2 ; quantiles of the non-zero: {0.0: 1.0, 0.25: 1.0, 0.5: 1.0, 0.75: 1.0, 1.0: 1.0}\n"
    "Number of objects related in the object interaction graph:    number of non-zero values: 2 ; quantiles of the non-zero: {0.0: 1.0, 0.25: 1.0, 0.5: 1.0, 0.75: 1.0, 1.0: 1.0}\n"
    "Number of object of type t related to the current object in the object interaction graph:    number of non-zero values: 2 ; quantiles of the non-zero: {0.0: 1.0, 0.25: 1.0, 0.5: 1.0, 0.75: 1.0, 1.0: 1.0}\n"
    "Number of events in the lifecycle of the object:    number of non-zero values: 2 ; quantiles of the non-zero: {0.0: 3.0, 0.25: 3.0, 0.5: 3.0, 0.75: 3.0, 1.0: 3.0}\n"
    "Degree centrality of the object in the object interaction graph:    number of non-zero values: 2 ; quantiles of the non-zero: {0.0: 1.0, 0.25: 1.0, 0.5: 1.0, 0.75: 1.0, 1.0: 1.0}\n"
    "Number of occurrences of the activity a:    number of non-zero values: 1 ; quantiles of the non-zero: {0.0: 1.0, 0.25: 1.0, 0.5: 1.0, 0.75: 1.0, 1.0: 1.0}\n");

  if (failures) {
    std::cerr << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "review BU1 tests passed\n";
  return 0;
}
