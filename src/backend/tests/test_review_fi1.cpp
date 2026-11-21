// Review FI1 (round FI, re-review of FH): checks for the fixes of this round.
//
// - FI1-1 The OC-DFG with ocel:timestamp as the event id on an OCEL CSV log
//   whose row lacks a time: pm4py lists the NaT event among the activities,
//   gives it no edge (its groupby drops the key), and takes an object's
//   start and end events from its first and last row with a time, while
//   the start and end activities stay the first and last row's. Native
//   keyed the event by the empty text and added it everywhere.
// - FI1-2 Activity-type matching with ocel:timestamp as the type key on a
//   CSV log with a NaT: a str activity column masks the NaT rows out first,
//   so the rows with a time decide (none passes, one names the array, more
//   the scalar); the object qualifier column adds cell by cell from the
//   first row (NaTType or Timestamp). Native always raised for the array.
// - FI1-3 A CSV log without any object reference, whose relations frame is
//   one float64 qualifier column without rows: the qualifier as the
//   activity key raises numpy's UFuncTypeError. Native raised its KeyError.
// - FI1-4 The exact attribute filters on a datetime column with a value
//   list of datetimes of one timezone next to None: pandas converts the
//   list to a datetime array first, None becoming NaT, so the NaT rows
//   match. Native read None as None, which matches nothing there.
//
// See docs/reference-2.7.22-parity.md, Review FI1.
#include "minera/ocel.hpp"
#include "minera/ocel_activity_type_matching.hpp"
#include "minera/ocel_csv.hpp"
#include "minera/ocel_filter.hpp"
#include "minera/timestamp.hpp"

#include <algorithm>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
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

std::string error_of(const std::function<void()>& run) {
  try {
    run();
  } catch (const std::exception& error) {
    return error.what();
  }
  return {};
}

bool contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

std::vector<std::string> event_ids(const Ocel& log) {
  std::vector<std::string> ids;
  for (const auto& event : log.events) ids.push_back(event.id);
  std::sort(ids.begin(), ids.end());
  return ids;
}

const std::string kObjects = "ocel:oid,ocel:type\nt0,t\nt1,t\n";

void fi1_1_ocdfg_nat_event_id() {
  // fi t1/m1.csv: e1's time is empty (NaT).
  const auto log = parse_ocel_csv(
      "ocel:eid,ocel:timestamp,ocel:activity,ocel:type:t\n"
      "e0,2024-02-02 10:00:00+00:00,a,['t0']\ne1,,b,['t0']\ne2,2024-02-02 12:00:00+00:00,c,['t0']\n",
      kObjects);
  OcdfgDiscoveryOptions options;
  OcdfgEdgePerformanceOptions timing;
  timing.event_id_attribute = "ocel:timestamp";
  options.edge_performance_options = timing;
  const auto graph = discover_ocdfg_classic_with_options(log, options);
  expect(graph.activities_indep.count("b") && graph.activities_indep.at("b").events == std::vector<std::string>{"NaT"},
         "FI1-1 the NaT event is listed among the activities");
  expect(graph.edges.count("t") && graph.edges.at("t").size() == 1 && graph.edges.at("t").count({"a", "c"}),
         "FI1-1 the NaT event takes part in no edge");
  expect(graph.end_activities.count("t") && graph.end_activities.at("t").size() == 1 &&
             graph.end_activities.at("t").count("b"),
         "FI1-1 the end activity is the last row's");
  if (graph.end_activities.count("t") && graph.end_activities.at("t").count("b") && graph.edges.count("t") &&
      graph.edges.at("t").count({"a", "c"})) {
    const auto& end = graph.end_activities.at("t").at("b").events;
    const auto& couples = graph.edges.at("t").at({"a", "c"}).event_couples;
    expect(end.size() == 1 && couples.size() == 1 && end.front() == couples.front().second,
           "FI1-1 the end event is the last row's with a time");
  }
}

void fi1_2_type_key_time() {
  const auto one_timed = parse_ocel_csv(  // fi t1/a1.csv
      "ocel:eid,ocel:timestamp,ocel:activity,ocel:type:t\ne0,,a,['t0']\ne1,2024-02-02 11:00:00+00:00,b,['t0']\n",
      kObjects);
  const auto none_timed = parse_ocel_csv(  // fi t1/a3.csv
      "ocel:eid,ocel:timestamp,ocel:activity,ocel:type:t\ne0,,a,\"['t0', 't1']\"\ne1,,b,['t0']\n", kObjects);
  const auto single = parse_ocel_csv(  // fi t1/a4.csv
      "ocel:eid,ocel:timestamp,ocel:activity,ocel:type:t\ne0,,a,['t0']\n", kObjects);
  const std::map<std::string, std::vector<std::string>> allowed{{"t", {"a", "b"}}};
  const auto run = [&](const Ocel& log, const std::string& activity_key) {
    OcelActivityTypeMatchingOptions options;
    options.activity_key = activity_key;
    options.object_type_key = "ocel:timestamp";
    return error_of([&] {
      const auto result = filter_ocel_activity_type_matching_exact(log, allowed, options);
      if (!result.events.empty() || !result.relations.empty()) throw std::runtime_error("kept rows");
    });
  };
  const std::string add = "TypeError: unsupported operand type(s) for +: '";
  expect(run(one_timed, "ocel:activity") == add + "DatetimeArray' and 'str'", "FI1-2 one timed row names the array");
  expect(run(one_timed, "ocel:qualifier") == add + "Timestamp' and 'str'", "FI1-2 the qualifier names the first row's Timestamp");
  expect(run(none_timed, "ocel:activity").empty(), "FI1-2 no timed row passes and matches nothing");
  expect(run(none_timed, "ocel:qualifier") == add + "NaTType' and 'str'", "FI1-2 the qualifier names the first row's NaT");
  expect(run(single, "ocel:activity").empty(), "FI1-2 a single NaT row passes");
  expect(run(single, "ocel:qualifier") == add + "DatetimeArray' and 'str'", "FI1-2 a single row names the array");
}

void fi1_3_degenerate_relations() {
  // fi t3/d1.csv: no event refers to an object.
  const auto log = parse_ocel_csv(
      "ocel:eid,ocel:timestamp,ocel:activity,ocel:type:t\ne0,2024-01-01 10:00:00,a,[]\ne1,2024-01-01 11:00:00,b,\n",
      "ocel:oid,ocel:type\nt0,t\n");
  OcelActivityTypeMatchingOptions options;
  options.activity_key = "ocel:qualifier";
  const auto error = error_of([&] { (void)filter_ocel_activity_type_matching_exact(log, {{"t", {"a"}}}, options); });
  expect(error ==
             "UFuncTypeError: ufunc 'add' did not contain a loop with signature matching types (dtype('float64'), "
             "dtype('<U4')) -> None",
         "FI1-3 the float64 qualifier raises numpy's UFuncTypeError: " + error);
  options.activity_key = "ocel:activity";
  expect(contains(error_of([&] { (void)filter_ocel_activity_type_matching_exact(log, {{"t", {"a"}}}, options); }),
                  "ocel:activity"),
         "FI1-3 the activity key still raises its KeyError");
}

void fi1_4_exact_filter_nulls_as_nat() {
  // fi t3/n1.csv: e1's time is empty (NaT).
  const auto log = parse_ocel_csv(
      "ocel:eid,ocel:timestamp,ocel:activity,ocel:type:t\ne0,2024-02-02 10:00:00+00:00,a,['t0']\ne1,,b,['t0']\n",
      "ocel:oid,ocel:type\nt0,t\n");
  const auto aware = *parse_timestamp_exact("2024-02-02T10:00:00+00:00");
  const auto naive = *parse_timestamp_exact("2024-02-02T10:00:00");
  using Values = std::vector<OcelEventAttributeFilterValue>;
  const auto ids = [&](const Values& values, bool positive) {
    return event_ids(filter_ocel_event_attribute_exact(log, "ocel:timestamp", values, positive));
  };
  expect(ids(Values{aware, nullptr}, true) == std::vector<std::string>{"e0", "e1"}, "FI1-4 None next to an aware datetime matches NaT");
  expect(ids(Values{aware, nullptr}, false).empty(), "FI1-4 the negative filter drops the NaT row too");
  expect(ids(Values{aware}, true) == std::vector<std::string>{"e0"}, "FI1-4 without None the NaT row stays out");
  expect(ids(Values{naive, nullptr}, true).empty(), "FI1-4 a naive datetime on an aware column keeps None as None");
}

}  // namespace

int main() {
  fi1_1_ocdfg_nat_event_id();
  fi1_2_type_key_time();
  fi1_3_degenerate_relations();
  fi1_4_exact_filter_nulls_as_nat();
  if (failures) {
    std::cerr << failures << " check(s) failed\n";
    return 1;
  }
  std::cout << "review FI1 checks passed\n";
  return 0;
}
