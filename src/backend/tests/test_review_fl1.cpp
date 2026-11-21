// Review FL1 (round FL, re-review of FK): checks for the fixes of this round.
//
// - FL2-1 Business hours read a time with more than six fraction digits at
//   its microsecond, floored, as pm4py's BusinessHours does with a Timestamp
//   at nanoseconds: the temporal profile, its conformance and the EventLog
//   path of the case statistics, the service time and the case arrival.
//   Native rounded the epoch double to the nearest microsecond.
// - FL2-2 The temporal profile of a CSV or XES upload with business hours
//   follows pm4py's DataFrame variant (the arithmetic of the frame, business
//   seconds between floored microseconds), and on a CSV upload it counts the
//   frame's UTC clock, as format_dataframe converts an offset; so does the
//   conformance. Native took the EventLog algorithm on each time's own wall
//   clock.
// - FL2-3 The heuristics miner's performance with business hours counts the
//   UTC clock on a CSV upload too.
// - FL2-4 The ancestors/descendants split takes the objects of the type in
//   the objects table's order, as pm4py lists them (and caps them at
//   max_objs). Native took them in id order.
//
// See docs/reference-2.7.22-parity.md, Review FL1.
#include "minera/case_arrival.hpp"
#include "minera/case_statistics.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/dataframe_columns.hpp"
#include "minera/heuristic_miner.hpp"
#include "minera/ocel.hpp"
#include "minera/ocel_csv.hpp"
#include "minera/ocel_transform.hpp"
#include "minera/service_time.hpp"
#include "minera/temporal_profile.hpp"
#include "minera/xes_event_log_importer.hpp"

#include <iostream>
#include <algorithm>
#include <memory>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

using namespace minera;

namespace {

int failures = 0;

void expect(bool condition, const std::string& what) {
  if (!condition) {
    ++failures;
    std::cerr << "FAILED: " << what << '\n';
  }
}

void expect_equal(double seen, double expected, const std::string& what) {
  if (seen != expected) {
    ++failures;
    std::cerr.precision(17);
    std::cerr << "FAILED: " << what << ": seen " << seen << ", expected " << expected << '\n';
  }
}

EventLog csv_upload(const std::string& text) {
  std::istringstream csv(text);
  CsvImportOptions import;
  import.case_id_column = "case:concept:name";
  import.activity_column = "concept:name";
  import.timestamp_column = "time:timestamp";
  import.resource_column = "org:resource";
  auto log = import_csv_event_log(csv, import);
  expect(static_cast<bool>(log.dataframe_columns), "a CSV upload carries its frame columns");
  return log;
}

EventLog xes_upload(const std::string& text) {
  std::istringstream xes(text);
  auto log = import_xes_dataframe_log(xes);
  log.dataframe_columns = std::make_shared<DataFrameColumns>(xes_dataframe_columns(log));
  return log;
}

using Pairs = std::vector<std::tuple<std::string, std::string, double, double>>;

void expect_profile(const TemporalProfile& profile, const Pairs& expected, const std::string& what) {
  expect(profile.entries.size() == expected.size(), what + ": the pairs");
  for (const auto& [source, target, mean, deviation] : expected) {
    const auto* entry = profile.find(source, target);
    expect(entry != nullptr, what + ": pair (" + source + ", " + target + ")");
    if (!entry) continue;
    expect_equal(entry->mean_seconds, mean, what + ": mean of (" + source + ", " + target + ")");
    expect_equal(entry->standard_deviation_seconds, deviation,
                 what + ": deviation of (" + source + ", " + target + ")");
  }
}

TemporalProfileOptions business_hours() {
  TemporalProfileOptions options;
  options.use_business_hours = true;
  return options;
}

// fl t2/b2.csv: times with seven fraction digits, no offsets.
const char* const b2 =
    "case:concept:name,concept:name,time:timestamp,org:resource,st\n"
    "c1,a,2024-02-05 09:00:00.1234567,r1,2024-02-05 08:30:00.5000001\n"
    "c1,b,2024-02-05 10:00:00.2000001,r2,2024-02-05 09:30:00.4444449\n"
    "c1,c,2024-02-06 11:15:00.7000009,r1,2024-02-06 11:00:00.5555552\n"
    "c2,a,2024-02-07 09:00:00.0000002,r1,2024-02-07 08:00:00.1111119\n"
    "c2,c,2024-02-07 12:00:00.7777777,r2,2024-02-07 11:00:00.0000001\n";

// fl t4/c1.csv and x1.xes: the same wall clocks with offsets; the CSV frame
// holds them in UTC, the XES reader keeps the wall clock.
const char* const c1 =
    "case:concept:name,concept:name,time:timestamp\n"
    "c1,a,2024-02-05 09:00:00+02:00\n"
    "c1,b,2024-02-05 16:30:00+02:00\n"
    "c2,a,2024-02-06 06:30:00-05:00\n"
    "c2,b,2024-02-06 16:45:00-05:00\n";

const char* const x1 =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<log xes.version=\"1.0\" xmlns=\"http://www.xes-standard.org/\">\n"
    "<trace><string key=\"concept:name\" value=\"c1\"/>\n"
    "<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" "
    "value=\"2024-02-05T09:00:00.000+02:00\"/></event>\n"
    "<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" "
    "value=\"2024-02-05T16:30:00.000+02:00\"/></event>\n"
    "</trace>\n"
    "<trace><string key=\"concept:name\" value=\"c2\"/>\n"
    "<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" "
    "value=\"2024-02-06T06:30:00.000-05:00\"/></event>\n"
    "<event><string key=\"concept:name\" value=\"b\"/><date key=\"time:timestamp\" "
    "value=\"2024-02-06T16:45:00.000-05:00\"/></event>\n"
    "</trace>\n"
    "</log>\n";

void fl2_1_and_2_temporal_profile_at_the_microsecond() {
  const auto log = csv_upload(b2);
  // pm4py: temporal_profile.variants.dataframe and .log with business_hours
  // agree on the formatted frame and its EventLog.
  const Pairs expected = {{"a", "b", 3600.076543999996, 0.0},
                          {"a", "c", 27450.677160500003, 23546.513520293123},
                          {"b", "c", 40500.5, 0.0}};
  expect_profile(discover_temporal_profile_as_uploaded(log, business_hours()), expected,
                 "FL2-2 b2.csv upload, business hours");
  expect_profile(discover_temporal_profile(log, business_hours()), expected,
                 "FL2-1 b2.csv EventLog algorithm, business hours");
}

void fl2_2_csv_upload_on_the_utc_clock() {
  const auto csv = csv_upload(c1);
  const Pairs expected = {{"a", "b", 23400.0, 5091.168824543142}};
  expect_profile(discover_temporal_profile_as_uploaded(csv, business_hours()), expected,
                 "FL2-2 c1.csv upload, business hours on the UTC clock");
  expect_profile(discover_temporal_profile(csv, business_hours()), expected,
                 "FL2-2 c1.csv EventLog algorithm, business hours on the UTC clock");

  const auto xes = xes_upload(x1);
  const Pairs wall = {{"a", "b", 31050.0, 5727.564927611035}};
  expect_profile(discover_temporal_profile_as_uploaded(xes, business_hours()), wall,
                 "FL2-2 x1.xes upload, business hours on the wall clock");
  expect_profile(discover_temporal_profile(xes, business_hours()), wall,
                 "FL2-2 x1.xes EventLog algorithm, business hours on the wall clock");

  // pm4py: conformance variants dataframe and log, zeta 1, both
  // [[('a', 'b', 27000.0, 7.0)], []].
  TemporalProfile profile;
  profile.entries.push_back({"a", "b", 1, 20000.0, 1000.0});
  auto options = business_hours();
  options.zeta = 1.0;
  const auto result = conform_temporal_profile_log(csv, profile, options);
  expect(result.size() == 2, "FL2-2 conformance keeps a bucket per case");
  if (result.size() == 2) {
    expect(result[0].size() == 1 && result[1].empty(), "FL2-2 conformance flags c1 alone");
    if (result[0].size() == 1) {
      const auto& deviation = result[0][0];
      expect(deviation.source == "a" && deviation.target == "b", "FL2-2 conformance pair");
      expect_equal(deviation.elapsed_seconds, 27000.0, "FL2-2 conformance elapsed business seconds");
      expect_equal(deviation.standard_deviation_distance, 7.0, "FL2-2 conformance distance");
    }
  }
}

double heuristics_performance(const EventLog& log, bool business) {
  HeuristicMinerOptions options;
  options.decoration = HeuristicNetDecoration::kPerformance;
  options.use_business_hours = business;
  const auto net = discover_heuristics_net(log, options);
  for (const auto& edge : net.edges)
    if (net.activities.at(edge.source) == "a" && net.activities.at(edge.target) == "b")
      return edge.representation;
  expect(false, "the heuristics net has the edge (a, b)");
  return -1.0;
}

void fl2_3_heuristics_on_the_utc_clock() {
  // pm4py heuristics apply_heu, heu_net_decoration performance: 31950
  // without business hours, 23400 with them on the CSV frame and its
  // EventLog; 31050 on the XES DataFrame.
  const auto csv = csv_upload(c1);
  expect_equal(heuristics_performance(csv, false), 31950.0, "FL2-3 c1.csv performance");
  expect_equal(heuristics_performance(csv, true), 23400.0, "FL2-3 c1.csv performance, business hours");
  expect_equal(heuristics_performance(xes_upload(x1), true), 31050.0,
               "FL2-3 x1.xes performance, business hours");
}

void fl2_1_event_log_statistics_at_the_microsecond() {
  // pm4py's EventLog statistics (case_statistics.get_cases_description,
  // case_arrival, service_time.log with start key st) with business hours
  // on the EventLog of b2.csv (a log without the upload's frame: on the
  // frame the case arrival takes pm4py's pandas path, which has no business
  // hours).
  auto log = csv_upload(b2);
  log.dataframe_columns.reset();
  CaseStatisticsOptions cases;
  cases.use_business_hours = true;
  const auto described = discover_case_statistics(log, cases);
  expect(described.cases.size() == 2, "FL2-1 case statistics: two cases");
  for (const auto& entry : described.cases) {
    if (entry.case_id == "c1")
      expect_equal(entry.case_duration, 44100.576543999996, "FL2-1 case statistics c1 duration");
    else if (entry.case_id == "c2")
      expect_equal(entry.case_duration, 10800.77777700001, "FL2-1 case statistics c2 duration");
    else
      expect(false, "FL2-1 case statistics: case " + entry.case_id);
  }

  CaseArrivalOptions arrival;
  arrival.use_business_hours = true;
  const auto arrived = discover_case_arrival(log, arrival);
  expect_equal(arrived.arrival_average, 71999.876544, "FL2-1 case arrival average");
  expect_equal(arrived.dispersion_average, 38700.07777700001, "FL2-1 case dispersion average");

  ServiceTimeOptions service;
  service.start_timestamp_key = "st";
  service.use_business_hours = true;
  const auto served = discover_service_time(log, service);
  expect(served.values.size() == 3, "FL2-1 service time: three activities");
  const std::vector<std::pair<std::string, double>> expected = {
      {"a", 2699.756172499998}, {"b", 1799.7555559999964}, {"c", 2250.461111000004}};
  for (const auto& [activity, value] : expected) {
    const auto found = served.values.find(activity);
    expect(found != served.values.end(), "FL2-1 service time of " + activity);
    if (found != served.values.end())
      expect_equal(found->second, value, "FL2-1 service time of " + activity);
  }
}

std::vector<std::string> sorted_ids(const Ocel& log) {
  std::vector<std::string> ids;
  for (const auto& object : log.objects) ids.push_back(object.id);
  std::sort(ids.begin(), ids.end());
  return ids;
}

void fl2_4_ancestors_split_in_the_objects_order() {
  // fl t5/s1.csv: the objects file lists t3, t2, u0, t0, t1. pm4py's
  // ancestors/descendants split on type t: centers t3, t2, t0, t1 (t3 and
  // t0 meet through u0, t2 has no events); with max_objs 2, t3 and t2.
  const auto log = parse_ocel_csv("ocel:eid,ocel:timestamp,ocel:activity,ocel:type:t,ocel:type:u\n"
                                  "e0,2024-02-02 07:00:00+02:00,a,['t1'],[]\n"
                                  "e1,2024-02-02 07:00:01+02:00,b,['t3'],['u0']\n"
                                  "e2,2024-02-04 07:00:00+02:00,d,['t0'],['u0']\n",
                                  "ocel:oid,ocel:type\nt3,t\nt2,t\nu0,u\nt0,t\nt1,t\n");
  OcelSplitOptions options;
  options.variant = OcelSplitVariant::kAncestorsDescendants;
  options.object_type = "t";
  const auto all = split_ocel(log, options);
  using Ids = std::vector<std::string>;
  expect(all.central_objects == Ids{"t3", "t2", "t0", "t1"}, "FL2-4 the centers in the objects order");
  if (all.logs.size() == 4) {
    expect(sorted_ids(all.logs[0]) == Ids{"t0", "t3", "u0"}, "FL2-4 t3's part");
    expect(sorted_ids(all.logs[1]) == Ids{"t2"} && all.logs[1].events.empty(), "FL2-4 t2's part");
    expect(sorted_ids(all.logs[2]) == Ids{"t0", "t3", "u0"}, "FL2-4 t0's part");
    expect(sorted_ids(all.logs[3]) == Ids{"t1"} && all.logs[3].events.size() == 1, "FL2-4 t1's part");
  } else {
    expect(false, "FL2-4 four parts");
  }
  options.max_objects = 2;
  const auto capped = split_ocel(log, options);
  expect(capped.central_objects == Ids{"t3", "t2"}, "FL2-4 max_objects 2 keeps the first two objects listed");
  expect(capped.truncated, "FL2-4 the capped split is truncated");
}

}  // namespace

int main() {
  fl2_1_and_2_temporal_profile_at_the_microsecond();
  fl2_2_csv_upload_on_the_utc_clock();
  fl2_3_heuristics_on_the_utc_clock();
  fl2_1_event_log_statistics_at_the_microsecond();
  fl2_4_ancestors_split_in_the_objects_order();
  if (failures) {
    std::cerr << failures << " check(s) failed\n";
    return 1;
  }
  std::cout << "review FL1 checks passed\n";
  return 0;
}
