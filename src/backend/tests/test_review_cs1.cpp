// Review CS1: pm4py's trace-attribute clustering on a CSV upload's frame
// (format_dataframe over read_csv). algorithm.apply converts the frame to an
// EventLog first: every trace then holds every case: column, typed as the
// frame holds it (an int column with a gap as floats, dates as UTC
// Timestamps, a gap NaN or NaT) from the case's first row, and the groups
// are the Python dict keys of those values named str() of each. A float
// column's NaN cells are distinct objects (one group each, whose sublog is
// that trace alone); the NaN of a text or object column and NaT are one
// object each (one group). Values are taken from pm4py 2.7.22 black-box on
// the same CSV (review-harness round-cs, t1/ref.py).
#include "minera/clustering.hpp"
#include "minera/csv_event_log_importer.hpp"

#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace minera;

namespace {

int failures = 0;

void check(const std::string& label, const std::string& seen, const std::string& expected) {
  if (seen == expected) return;
  ++failures;
  std::cerr << label << ":\n  seen     " << seen << "\n  expected " << expected << "\n";
}

const std::string kCsv =
    "case,act,ts,case:dept,case:num,case:flt,case:day,case:boo\n"
    "c0,a,2020-01-01 00:00:00,G0,007,1.5,2021-05-01,True\n"
    "c0,b,2020-01-01 00:05:00,G0,007,1.5,2021-05-01,True\n"
    "c1,b,2020-01-01 00:01:00,,,,,\n"
    "c1,c,2020-01-01 00:07:00,,,,,\n"
    "c2,a,2020-01-01 00:02:00,,2,,,False\n"
    "c2,c,2020-01-01 00:03:00,G1,2,,,False\n"
    "c3,b,2020-01-01 00:03:00,G1,007,2.5,2021-05-02,True\n"
    "c3,a,2020-01-01 00:09:00,G1,007,2.5,2021-05-02,True\n"
    "c3,d,2020-01-01 00:10:00,G1,007,2.5,2021-05-02,True\n"
    "c4,c,2020-01-01 00:04:00,,1,,,True\n"
    "c4,d,2020-01-01 00:06:00,,1,,,True\n"
    "c5,a,2020-01-01 00:01:00,G0,2,1.5,2021-05-01,False\n"
    "c5,d,2020-01-01 00:08:00,G0,2,1.5,2021-05-01,False\n";

EventLog import() {
  CsvImportOptions options;
  options.case_id_column = "case";
  options.activity_column = "act";
  options.timestamp_column = "ts";
  std::istringstream input(kCsv);
  return import_csv_event_data(input, options).log;
}

std::string names(const TraceAttributeClusteringNode& node) {
  std::string out = node.name;
  if (node.children.empty()) return out;
  out += '(';
  for (std::size_t i = 0; i < node.children.size(); ++i) {
    if (i) out += ',';
    out += names(node.children[i]);
  }
  return out + ')';
}

std::string joined(const std::vector<std::string>& values) {
  std::string out;
  for (std::size_t i = 0; i < values.size(); ++i) out += (i ? "," : "") + values[i];
  return out;
}

}  // namespace

int main() {
  const auto log = import();
  using V = TraceAttributeClusteringVariant;
  const std::string d1 = "2021-05-01 00:00:00+00:00", d2 = "2021-05-02 00:00:00+00:00";
  const std::string day = "root(NaT," + d1 + "-" + d2 + "(" + d1 + "," + d2 + ")) | NaT," + d1 + "," + d2;
  const std::vector<std::pair<V, std::vector<std::pair<std::string, std::string>>>> expected = {
      {V::kDmmLeven,
       {{"dept", "root(nan,G0-G1(G0,G1)) | nan,G0,G1"},
        {"num", "root(1.0,2.0-7.0-nan(nan,2.0-7.0(7.0,2.0))) | 1.0,nan,7.0,2.0"},
        {"flt", "root(nan-nan(nan,nan),1.5-2.5-nan(nan,1.5-2.5(1.5,2.5))) | nan,nan,nan,1.5,2.5"},
        {"day", day},
        {"boo", "root(nan,False-True(True,False)) | nan,True,False"}}},
      {V::kAvgLeven,
       {{"dept", "root(nan,G0-G1(G0,G1)) | nan,G0,G1"},
        {"num", "root(1.0,2.0-7.0-nan(nan,2.0-7.0(7.0,2.0))) | 1.0,nan,7.0,2.0"},
        {"flt", "root(nan,1.5-2.5-nan-nan(nan,1.5-2.5-nan(2.5,1.5-nan(1.5,nan)))) | nan,nan,2.5,1.5,nan"},
        {"day", day},
        {"boo", "root(nan,False-True(True,False)) | nan,True,False"}}},
      {V::kDmmVec,
       {{"dept", "root(nan,G0-G1(G0,G1)) | nan,G0,G1"},
        {"num", "root(2.0-7.0(7.0,2.0),1.0-nan(nan,1.0)) | 7.0,2.0,nan,1.0"},
        {"flt", "root(1.5-2.5(1.5,2.5),nan-nan-nan(nan,nan-nan(nan,nan))) | 1.5,2.5,nan,nan,nan"},
        {"day", day},
        {"boo", "root(nan,False-True(True,False)) | nan,True,False"}}},
      {V::kAvgVec,
       {{"dept", "root(nan,G0-G1(G0,G1)) | nan,G0,G1"},
        {"num", "root(2.0-7.0(7.0,2.0),1.0-nan(nan,1.0)) | 7.0,2.0,nan,1.0"},
        {"flt", "root(1.5-2.5(1.5,2.5),nan-nan-nan(nan,nan-nan(nan,nan))) | 1.5,2.5,nan,nan,nan"},
        {"day", day},
        {"boo", "root(nan,False-True(True,False)) | nan,True,False"}}},
  };
  for (const auto& [variant, calls] : expected) {
    for (const auto& [attribute, answer] : calls) {
      TraceAttributeClusteringOptions options;
      options.trace_attribute = attribute;
      options.variant = variant;
      const auto label = "variant " + std::to_string(static_cast<int>(variant)) + " " + attribute;
      try {
        const auto result = discover_trace_attribute_clustering(log, options);
        check(label, names(result.tree) + " | " + joined(result.leaf_names), answer);
        // Repeated leaf names (one NaN group per trace) still serialize.
        (void)trace_attribute_clustering_json(result);
      } catch (const std::exception& error) {
        check(label, std::string("error: ") + error.what(), answer);
      }
    }
  }
  // Each NaN cell of the float column is its own group of one trace, in
  // first-seen order (the case's first row: c2's blank dept is NaN although
  // its second row holds G1).
  std::string groups;
  for (const auto& group : trace_attribute_clustering_groups(log, "flt")) {
    groups += group.name + ":";
    for (const auto index : group.trace_indices) groups += log.traces[index].case_id + ";";
    groups += ' ';
  }
  check("flt groups", groups, "1.5:c0;c5; nan:c1; nan:c2; 2.5:c3; nan:c4; ");
  std::string dept;
  for (const auto& group : trace_attribute_clustering_groups(log, "dept")) {
    dept += group.name + ":";
    for (const auto index : group.trace_indices) dept += log.traces[index].case_id + ";";
    dept += ' ';
  }
  check("dept groups", dept, "G0:c0;c5; nan:c1;c2;c4; G1:c3; ");
  if (failures) {
    std::cerr << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "review CS1: trace-attribute clustering on a CSV frame passed\n";
  return 0;
}
