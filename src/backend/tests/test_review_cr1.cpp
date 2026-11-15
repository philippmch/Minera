// Review CR1: pm4py's link analysis and contextual label splitting on a CSV
// upload's frame (format_dataframe over read_csv). Link analysis sorts the
// rows by the sorting column's typed cells with numpy's default quicksort
// (an introsort, so equal cells leave row order from 17 rows on), numbers
// them, and merges the frame with itself on out = in under pandas' merge
// key checks; label splitting keys the contexts by the typed cells and
// cannot append the suffix to a label that is no str. Values are taken from
// pm4py 2.7.22 black-box on the same CSV with NPY_DISABLE_CPU_FEATURES
// "X86_V3 X86_V4 AVX512_ICL AVX512_SPR" (numpy's portable sort, as the
// other network sweeps; review-harness round-cr, t1/ref.py).
#include "minera/csv_event_log_importer.hpp"
#include "minera/label_splitting.hpp"
#include "minera/link_analysis.hpp"

#include <algorithm>
#include <functional>
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

void check_error(const std::string& label, const std::function<void()>& run, const std::string& message) {
  try {
    run();
  } catch (const std::exception& error) {
    if (std::string(error.what()).find(message) != std::string::npos) return;
    ++failures;
    std::cerr << label << ":\n  error    " << error.what() << "\n  expected " << message << "\n";
    return;
  }
  ++failures;
  std::cerr << label << ": no error, expected " << message << "\n";
}

const std::string kCsv =
    "case,act,ts,num,flt,txt\n"
    "c1,a,2021-01-01 00:00:00,2,1.0,x\n"
    "c1,b,2021-01-02 01:00:00,2,2.0,y\n"
    "c1,c,2021-01-03 02:00:00,3,4.0,\n"
    "c1,b,2021-01-04 03:00:00,2,3.5,z\n"
    "c1,d,2021-01-05 04:00:00,4,1.0,x\n"
    "c2,a,2021-01-01 05:00:00,1,2.0,y\n"
    "c2,b,2021-01-02 06:00:00,2,4.0,\n"
    "c2,c,2021-01-03 07:00:00,3,,z\n"
    "c2,b,2021-01-04 08:00:00,2,1.0,x\n"
    "c2,d,2021-01-05 09:00:00,2,2.0,y\n"
    "c3,a,2021-01-01 00:00:00,1,4.0,\n"
    "c3,c,2021-01-02 01:00:00,3,3.5,z\n"
    "c3,b,2021-01-03 02:00:00,2,1.0,x\n"
    "c3,d,2021-01-04 03:00:00,4,2.0,y\n"
    "c4,a,2021-01-01 04:00:00,1,4.0,\n"
    "c4,b,2021-01-02 05:00:00,2,3.5,z\n"
    "c4,d,2021-01-03 06:00:00,4,1.0,x\n"
    "c5,a,2021-01-01 07:00:00,1,2.0,y\n"
    "c5,b,2021-01-02 08:00:00,2,4.0,\n"
    "c5,c,2021-01-03 09:00:00,3,3.5,z\n"
    "c5,b,2021-01-04 00:00:00,2,1.0,x\n"
    "c5,d,2021-01-05 01:00:00,2,2.0,y\n";

EventLog import() {
  CsvImportOptions options;
  options.case_id_column = "case";
  options.activity_column = "act";
  options.timestamp_column = "ts";
  std::istringstream input(kCsv);
  return import_csv_event_data(input, options).log;
}

std::string pairs(const LinkAnalysisResult& result) {
  std::vector<std::pair<std::size_t, std::size_t>> all;
  for (const auto& link : result.links) all.emplace_back(link.source.index, link.target.index);
  std::sort(all.begin(), all.end());
  std::string out;
  for (const auto& [a, b] : all) out += (out.empty() ? "" : " ") + std::to_string(a) + "-" + std::to_string(b);
  return out;
}

void link_analysis(const EventLog& log) {
  LinkAnalysisOptions options;
  options.out_column = "num";
  options.in_column = "txt";
  check_error("merge of an int64 and a str column", [&] { (void)discover_link_analysis(log, options); },
              "ValueError: You are trying to merge on int64 and str columns for key 'num'. If you wish to proceed you "
              "should use pd.concat");
  options.in_column = "flt";
  options.sorting_column = "num";
  check("int64 out column meets float64 in column, sorted by ties", pairs(discover_link_analysis(log, options)),
        "0-6 0-7 0-9 0-13 0-20 0-21 1-6 1-7 1-9 1-13 1-20 1-21 2-6 2-7 2-9 2-13 2-20 2-21 3-6 3-7 3-9 3-13 3-20 3-21 "
        "4-8 4-12 4-14 4-19 5-8 5-12 5-14 5-19 6-8 6-12 6-14 6-19 7-8 7-12 7-14 7-19 8-12 8-14 8-19 9-12 9-14 9-19 "
        "10-12 10-14 10-19 11-12 11-14 11-19 12-14 12-19 13-14 13-19 14-19");
  options.out_column = "txt";
  options.in_column = "txt";
  options.propagate = true;
  check("str column with a gap, propagated", pairs(discover_link_analysis(log, options)),
        "0-2 0-4 0-10 0-17 1-3 1-8 1-12 1-14 1-19 2-4 2-10 2-17 3-8 3-12 3-14 3-19 4-10 4-17 5-11 5-15 5-16 5-18 "
        "6-7 6-9 6-13 6-20 6-21 7-9 7-13 7-20 7-21 8-12 8-14 8-19 9-13 9-20 9-21 10-17 11-15 11-16 11-18 12-14 "
        "12-19 13-20 13-21 14-19 15-16 15-18 16-18 20-21");
  // The quicksort's order of the num column's ties (pm4py's @@index).
  const auto sorted = discover_link_analysis(log, options);
  std::vector<std::string> order(22);
  for (const auto& link : sorted.links) {
    order[link.source.index] = link.source.case_id + link.source.activity;
    order[link.target.index] = link.target.case_id + link.target.activity;
  }
  std::string text;
  for (const auto& row : order) text += (text.empty() ? "" : " ") + row;
  check("rows numbered in the quicksort's order", text,
        "c3a c5a c4a c2a c5b c4b c3b c5b c2d c1a c2b c1b c1b c2b c5d c2c c3c c1c c5c c3d c1d c4d");
}

void label_splitting(const EventLog& log) {
  LabelSplittingOptions options;
  options.activity_key = "concept:name";
  const auto result = split_activity_labels(log, options);
  std::string text;
  for (const auto& trace : result.log.traces) {
    std::string row;
    for (const auto& event : trace.events) row += (row.empty() ? "" : " ") + event.activity;
    text += (text.empty() ? "" : " | ") + row;
  }
  check("contextual splitting on the frame", text, "a_0 b c b d | a_0 b c b d | a_1 c b d | a_0 b d | a_0 b c b d");
  options.activity_key = "num";
  check_error("an int label takes no suffix", [&] { (void)split_activity_labels(log, options); },
              "TypeError: unsupported operand type(s) for +: 'int' and 'str'");
}

}  // namespace

int main() {
  const auto log = import();
  link_analysis(log);
  label_splitting(log);
  if (failures != 0) {
    std::cerr << failures << " review CR1 check(s) failed\n";
    return 1;
  }
  std::cout << "review CR1 checks passed\n";
  return 0;
}
