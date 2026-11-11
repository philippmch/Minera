// Review AY1: network analysis on uploads (the sort before the merge checks,
// and business hours: pm4py's BusinessHours on microsecond wall clocks and
// the errors of its row-wise apply), and the process time zone's naive
// datetime.timestamp() (CPython's fold-0 search). Values and messages are
// taken from pm4py 2.7.22 black-box on read_csv + format_dataframe(
// case_id="cid", activity_key="act", timestamp_key="ts") of the same text, on
// read_xes, or from CPython under TZ=Europe/Berlin.
#include "../local_time.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/json_export.hpp"
#include "minera/network_analysis.hpp"
#include "minera/xes_event_log_importer.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <functional>
#include <iostream>
#include <map>
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
    "cid,act,ts,who,st,txt,num,mix\n"
    "c1,a,2021-03-01 08:00:00.25,R,2021-03-01 07:00:00,p,1.5,a\n"
    "c1,b,2021-03-01 08:00:59.5,S,,q,2.5,1.5\n"
    "c1,c,2021-03-01 16:59:59.999999,R,2021-03-01 16:00:00,r,3.5,b\n"
    "c1,d,2021-03-08 07:00:00.000001,S,2021-03-08 06:00:00,s,4.5,2.5\n"
    "c2,a,2021-03-02 06:00:00.123456789,R,2021-03-02 05:00:00,t,,c\n"
    "c2,b,2021-03-02 09:30:00.987654321,S,2021-03-02 09:00:00,,5.5,3.5\n";

NetworkAnalysisOptions business() {
  NetworkAnalysisOptions options;
  options.source_node_attribute = "who";
  options.target_node_attribute = "who";
  options.include_performance = true;
  options.business_hours = true;
  return options;
}

std::map<std::string, std::vector<double>> durations(const NetworkAnalysisResult& result) {
  std::map<std::string, std::vector<double>> out;
  for (const auto& edge : result.edges) {
    auto values = edge.performance;
    std::sort(values.begin(), values.end());
    out[edge.source + "|" + edge.target + "|" + edge.activity] = values;
  }
  return out;
}

void business_hours_on_microsecond_wall_clocks() {
  const auto log = upload(kLog);
  // Nanoseconds are truncated; each week's seconds since Monday are a float.
  using D = std::map<std::string, std::vector<double>>;
  assert((durations(discover_network_analysis(log, business())) ==
          D{{"R|S|a", {59.25, 9000.987653999997}}, {"R|S|c", {144000.00000200002}}, {"S|R|b", {32340.499999}}}));
  auto slots = business();
  slots.business_hour_slots = {{0.0, 3600.5}, {25200.25, 61200.0}};
  assert((durations(discover_network_analysis(log, slots)) ==
          D{{"R|S|a", {0.0, 59.25}}, {"R|S|c", {3600.5000010000003}}, {"S|R|b", {32340.499999}}}));
}

void business_hours_apply_errors() {
  const auto log = upload(kLog);
  auto options = business();
  options.timestamp_attribute = "txt";
  expect_error([&] { (void)discover_network_analysis(log, options); },
               "network analysis: TypeError: str.replace() takes no keyword arguments");
  options.include_performance = false;  // the apply runs without performance too
  expect_error([&] { (void)discover_network_analysis(log, options); },
               "network analysis: TypeError: str.replace() takes no keyword arguments");
  options.timestamp_attribute = "num";
  expect_error([&] { (void)discover_network_analysis(log, options); },
               "network analysis: AttributeError: 'float' object has no attribute 'replace'");
  options.timestamp_attribute = "st";  // a NaT in a joined row
  expect_error([&] { (void)discover_network_analysis(log, options); },
               "network analysis: ValueError: cannot convert float NaN to integer");
  // No joined rows: the applied frame cannot become the duration column.
  const auto single = upload("cid,act,ts,who\nc1,a,2021-03-01 08:00:00,R\nc2,b,2021-03-01 09:00:00,S\n");
  for (const bool performance : {true, false}) {
    auto none = business();
    none.include_performance = performance;
    expect_error([&] { (void)discover_network_analysis(single, none); },
                 "network analysis: ValueError: Cannot set a DataFrame with multiple columns to the single column "
                 "@@timestamp_diff");
  }
}

// A NaT in a joined row is pandas' NaN difference, kept in the edge's list
// in the joined frame's order (review AY1-3).
void nat_differences_stay_in_the_list() {
  const auto log = upload(kLog);
  auto options = business();
  options.business_hours = false;
  options.timestamp_attribute = "st";
  std::map<std::string, std::vector<double>> got;
  for (const auto& edge : discover_network_analysis(log, options).edges)
    got[edge.source + "|" + edge.target + "|" + edge.activity] = edge.performance;
  assert(got.size() == 3);
  assert(got["R|S|a"].size() == 2 && std::isnan(got["R|S|a"][0]) && got["R|S|a"][1] == 14400.0);
  assert(got["R|S|c"] == std::vector<double>{568800.0});
  assert(got["S|R|b"].size() == 1 && std::isnan(got["S|R|b"][0]));
  const auto json = network_analysis_json(discover_network_analysis(log, options));
  assert(json.find("\"performance\":[null,14400]") != std::string::npos);
}

const std::string kMixedXes = R"(<?xml version="1.0" encoding="UTF-8"?>
<log xes.version="1.0">
<trace><string key="concept:name" value="c1"/>
<event><string key="concept:name" value="a"/><string key="org:resource" value="R"/><date key="time:timestamp" value="2021-03-01T08:00:00.000+00:00"/><string key="mix" value="x"/><float key="num" value="1.5"/></event>
<event><string key="concept:name" value="b"/><string key="org:resource" value="S"/><date key="time:timestamp" value="2021-03-01T09:00:00.000+00:00"/><float key="mix" value="2.5"/><float key="num" value="2.5"/></event>
</trace>
<trace><string key="concept:name" value="c2"/>
<event><string key="concept:name" value="a"/><string key="org:resource" value="R"/><date key="time:timestamp" value="2021-03-02T08:00:00.000+00:00"/><string key="mix" value="y"/><float key="num" value="3.5"/></event>
</trace>
</log>
)";

void sort_fails_before_the_merge() {
  std::istringstream input(kMixedXes);
  const auto log = import_xes_dataframe_log(input);
  NetworkAnalysisOptions options;
  options.in_relation_attribute = "num";
  expect_error([&] { (void)discover_network_analysis(log, options); },
               "network analysis: ValueError: You are trying to merge on str and float64 columns for key "
               "'case:concept:name'. If you wish to proceed you should use pd.concat");
  options.sorting_attribute = "mix";
  expect_error([&] { (void)discover_network_analysis(log, options); },
               "network analysis: TypeError: '<' not supported between instances of 'float' and 'str'");
}

void naive_timestamps_in_the_process_zone() {
#ifdef _WIN32
  _putenv_s("TZ", "Europe/Berlin");
  _tzset();
#else
  setenv("TZ", "Europe/Berlin", 1);
  tzset();
#endif
  const auto at = [](int y, int m, int d, int hh, int mm) {
    return *local_time::naive_timestamp(local_time::civil_seconds(y, m, d, hh, mm, 0));
  };
  assert(at(2021, 7, 1, 12, 0) == 1625133600);
  assert(at(2021, 3, 28, 2, 30) == 1616895000);   // in the gap: the offset before it
  assert(at(2021, 10, 31, 2, 30) == 1635640200);  // twice: the first
  assert(at(1960, 6, 1, 12, 0) == -302446800);  // Windows' runtime cannot; the database can
  assert(*local_time::wall_seconds(1625133600) == local_time::civil_seconds(2021, 7, 1, 12, 0, 0));
}

}  // namespace

int main() {
  business_hours_on_microsecond_wall_clocks();
  business_hours_apply_errors();
  nat_differences_stay_in_the_list();
  sort_fails_before_the_merge();
  naive_timestamps_in_the_process_zone();
  std::cout << "review AY1 tests passed\n";
  return 0;
}
