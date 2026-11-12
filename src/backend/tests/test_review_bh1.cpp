// Review BH1: the LLM descriptions of pm4py's log skeleton, temporal profile
// and declare models on an XES upload, which reach pm4py as read_xes's frame:
// the skeleton lists the activities of the first case by sorted id, then the
// rest by value_counts, and prints each count set sorted as text; the
// temporal profile pairs each case's rows sorted by timestamp, takes
// pandas' Timedelta parts for a duration, Kahan's sum for the mean and
// Welford's deviation; the declare discovery reads each case as the frame's
// rows from its first row on, as many as it has, and so do the top-k and
// coverage variant filters when they count variants. Values are taken from
// pm4py 2.7.22 black-box on the same inputs (review-harness round-bh, bh1/).
#include "minera/dataframe_columns.hpp"
#include "minera/llm_context.hpp"
#include "minera/log_filter.hpp"
#include "minera/xes_event_log_importer.hpp"

#include <iostream>
#include <memory>
#include <set>
#include <sstream>
#include <string>

using namespace minera;

namespace {

EventLog xes_upload(const std::string& text) {
  std::istringstream xes(text);
  auto log = import_xes_dataframe_log(xes);
  log.dataframe_columns = std::make_shared<DataFrameColumns>(xes_dataframe_columns(log));
  return log;
}

std::string event(const std::string& activity, const std::string& time) {
  return "<event><string key=\"concept:name\" value=\"" + activity +
         "\"/><date key=\"time:timestamp\" value=\"2020-01-01T" + time + "+00:00\"/></event>";
}

std::string trace(const std::string& name, const std::string& events) {
  return "<trace><string key=\"concept:name\" value=\"" + name + "\"/>" + events + "</trace>";
}

std::string clock(int minute, int second) {
  std::string text = "00:";
  text += static_cast<char>('0' + minute / 10);
  text += static_cast<char>('0' + minute % 10);
  text += ':';
  text += static_cast<char>('0' + second / 10);
  text += static_cast<char>('0' + second % 10);
  return text;
}

// File order y, w, x, z, v, then x again: x's rows are apart. Sorted, the
// first case is w.
EventLog skeleton_upload() {
  std::string w = event("c", "00:00:01"), x = event("c", "00:10:19.313500"), z, v;
  for (int i = 0; i < 8; ++i) {
    w += event("a", clock(1, i));
    x += event("a", clock(11, i));
  }
  z = event("b", "00:20:00") + event("c", "00:20:05") + event("d", "00:20:06") + event("d", "00:20:07");
  for (int i = 0; i < 12; ++i) v += event("d", clock(30, i));
  return xes_upload("<?xml version=\"1.0\" encoding=\"UTF-8\"?><log xes.version=\"1.0\">" +
                    trace("y", event("b", "00:00:00")) + trace("w", w) + trace("x", x) + trace("z", z) +
                    trace("v", v) + trace("x", event("b", "00:10:23.818500")) + "</log>");
}

std::string context(const EventLog& log, const std::string& variant) {
  LlmContextOptions options;
  options.variant = variant;
  options.include_header = false;
  options.max_len = 100000;
  return build_llm_context(log, options).text;
}

int check(const std::string& label, const std::string& seen, const std::string& expected) {
  if (seen == expected) return 0;
  std::cerr << label << ":\n  seen     " << seen << "\n  expected " << expected << "\n";
  return 1;
}

int descriptions() {
  const auto log = skeleton_upload();
  int failures = 0;
  // Before: "b: 0, 1; c: 0, 1; a: 0, 8; d: 0, 2, 12;" (traces in file order,
  // counts ascending).
  failures += check("log skeleton", context(log, "log_skeleton"),
                    "\nEquivalence: \n\nAlways Before: \n\nAlways After: \n\nNever Together:  ('a', 'd') ('d', "
                    "'a')\n\nActivity Occurrences:  d: 0, 12, 2; a: 0, 8; b: 0, 1; c: 0, 1;\n\nDirectly-Follows "
                    "Constraints: \n\n");
  // Before: no b -> a (x's b is its last row but precedes its a's), and
  // c -> b 4.51 (4.505000114... from epoch seconds; pandas' 4 + 505000e-6).
  failures += check("temporal profile", context(log, "temporal_profile"),
                    "\n\na -> a :  AVG: 3.00 s  STD: 1.75 s\nb -> a :  AVG: 39.68 s  STD: 2.45 s\nb -> c :  AVG: "
                    "5.00 s  STD: 0.00 s\nb -> d :  AVG: 6.50 s  STD: 0.71 s\nc -> a :  AVG: 53.34 s  STD: 9.75 "
                    "s\nc -> b :  AVG: 4.50 s  STD: 0.00 s\nc -> d :  AVG: 1.50 s  STD: 0.71 s\nd -> d :  AVG: 4.28 "
                    "s  STD: 2.72 s");
  // x, y, x: pm4py reads x as its first row and the next (a, c) and y as c.
  // Its template order follows the hash seed, so the lines compare as a set.
  // Before: the non co-existence of a and c, and no existence of c.
  const auto declare = context(xes_upload("<?xml version=\"1.0\" encoding=\"UTF-8\"?><log xes.version=\"1.0\">" +
                                          trace("x", event("a", "00:00:01")) + trace("y", event("c", "00:00:02")) +
                                          trace("x", event("b", "00:00:03")) + "</log>"),
                               "declare");
  std::set<std::string> lines;
  std::istringstream in(declare.substr(declare.find("model:\n") + 7));
  for (std::string line; std::getline(in, line);)
    if (!line.empty()) lines.insert(line);
  const std::set<std::string> expected = {"Exactly 1: c", "Existence: c", "Non Succession: ('c', 'a')",
                                          "Non Chain succession: ('c', 'a')"};
  if (lines != expected) {
    ++failures;
    std::cerr << "declare:\n" << declare << "\n";
  }
  return failures;
}

std::string kept(const EventLog& log, const LogFilterOptions& options) {
  std::set<std::string> cases;
  for (const auto& trace : filter_event_log(log, options).traces) cases.insert(trace.case_id);
  std::string text;
  for (const auto& id : cases) text += id + " ";
  return text;
}

// x a, y a, x b, y b, z c, w d: pm4py counts x as (a, a) and y as (a, b),
// each once, then keeps the cases whose own variant was chosen.
int variant_filters() {
  const auto log = xes_upload("<?xml version=\"1.0\" encoding=\"UTF-8\"?><log xes.version=\"1.0\">" +
                              trace("x", event("a", "00:00:00")) + trace("y", event("a", "00:00:01")) +
                              trace("x", event("b", "00:00:02")) + trace("y", event("b", "00:00:03")) +
                              trace("z", event("c", "00:00:04")) + trace("w", event("d", "00:00:05")) + "</log>");
  int failures = 0;
  LogFilterOptions top;
  top.top_variants = 1;
  // Before: "x y " ((a, b) twice). pm4py's four variants tie; (d) ranks first.
  failures += check("top variants", kept(log, top), "w ");
  top.top_variants = 3;
  // (d), (c), (a, b): x keeps its own (a, b) although its slice reads (a, a).
  failures += check("top 3 variants", kept(log, top), "w x y z ");
  LogFilterOptions minimum;
  minimum.variant_min_coverage = 0.4;
  // Before: "x y " (a share of 0.5). Every slice has a share of 0.25.
  failures += check("minimum coverage", kept(log, minimum), "");
  LogFilterOptions maximum;
  maximum.variant_max_coverage = 0.3;
  // Before: "w z ".
  failures += check("maximum coverage", kept(log, maximum), "w x y z ");
  return failures;
}

}  // namespace

int main() {
  const int failures = descriptions() + variant_filters();
  if (failures != 0) {
    std::cerr << failures << " review BH1 failure(s)\n";
    return 1;
  }
  std::cout << "review BH1 tests passed\n";
  return 0;
}
