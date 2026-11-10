#include "minera/resource_profiles.hpp"

#include <cassert>
#include <functional>
#include <map>
#include <sstream>
#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "minera/csv_event_log_importer.hpp"
#include "minera/xes_event_log_importer.hpp"

using namespace minera;

static EventLog sample() {
  EventLog log;
  Trace a; a.case_id = "a";
  a.events.push_back({"a","A","2024-01-01T00:00:00Z","alice",0,{},"start"});
  a.events.push_back({"a","A","2024-01-01T00:00:10Z","alice",1,{},"complete"});
  a.events.push_back({"a","B","2024-01-01T00:00:10Z","bob",2,{},""});
  Trace b; b.case_id = "b";
  b.events.push_back({"b","A","2024-01-01T00:00:10Z","alice",3,{},""});
  b.events.push_back({"b","C","2024-01-01T00:00:20Z","bob",4,{},""});
  log.traces = {a,b}; return log;
}

// Reference values from PM4Py 2.7.22 resource_profiles (public calls).
static Event timed(std::string activity, std::string resource, int second, std::string lifecycle = "") {
  Event e{"c", std::move(activity), "2026-01-01T00:00:" + std::string(second < 10 ? "0" : "") + std::to_string(second) + "Z",
          std::move(resource), 0, {}, std::move(lifecycle)};
  e.resource_state = FirstClassAttributeState::kPresent;
  e.activity_state = FirstClassAttributeState::kPresent;
  return e;
}
static EventLog one_case(std::vector<Event> events) {
  Trace trace; trace.case_id = "c";
  for (std::size_t i = 0; i < events.size(); ++i) { events[i].input_order = i; trace.events.push_back(events[i]); }
  EventLog log; log.traces.push_back(std::move(trace)); return log;
}
static ResourceProfilesResult query(const EventLog& log, ResourceProfileMetric metric, int t1, int t2,
                                    std::string resource, std::string activity = "", bool literal = false) {
  ResourceProfilesOptions o; o.metric = metric;
  o.t1 = "2026-01-01T00:00:" + std::string(t1 < 10 ? "0" : "") + std::to_string(t1) + "Z";
  o.t2 = "2026-01-01T00:00:" + std::string(t2 < 10 ? "0" : "") + std::to_string(t2) + "Z";
  o.resource = std::move(resource); o.activity = std::move(activity);
  o.resource_given = literal; o.activity_given = literal && !o.activity.empty();
  return discover_resource_profiles(log, o);
}
static double only_value_at(const ResourceProfilesResult& result, int line) {
  if (result.rows.size() != 1) std::fprintf(stderr, "line %d: %zu rows\n", line, result.rows.size());
  assert(result.rows.size() == 1);
  return result.rows.front().value;
}
#define only_value(result) only_value_at((result), __LINE__)
template <class F> static bool raises(F f) {
  try { f(); } catch (const std::invalid_argument&) { return true; }
  return false;
}

static void reference_audit_contracts() {
  // O1: a case counts for a window it spans, or that holds either endpoint
  // (closed); A/r at 0s and B/s at 30s average 30s over [5,15).
  const auto spanning = one_case({timed("A", "r", 0), timed("B", "s", 30)});
  assert(only_value(query(spanning, ResourceProfileMetric::kAverageCaseDuration, 5, 15, "r")) == 30.0);
  assert(only_value(query(spanning, ResourceProfileMetric::kAverageCaseDuration, 5, 15, "s")) == 30.0);
  assert(only_value(query(spanning, ResourceProfileMetric::kAverageCaseDuration, 30, 40, "r")) == 30.0);
  assert(only_value(query(spanning, ResourceProfileMetric::kAverageCaseDuration, 0, 5, "r")) == 30.0);
  assert(raises([&] { (void)query(spanning, ResourceProfileMetric::kAverageCaseDuration, 31, 40, "r"); }));

  // O2: only unmarked and "complete" events form intervals, each starting at
  // the previous kept event: B gets 10s..15s, A's completion a zero interval.
  const auto mixed = one_case({timed("A", "r", 0, "start"), timed("A", "r", 10, "complete"), timed("B", "r", 15)});
  assert(only_value(query(mixed, ResourceProfileMetric::kAverageDurationActivity, 0, 20, "r", "B")) == 5.0);
  // average_duration_activity follows the reference DataFrame path (CSV/XES
  // inputs): every event takes part, so A start@0 gives 0s and A complete@10
  // 10s (PM4Py 2.7.22 pandas variant: 5.0; the EventLog variant gives 0.0).
  assert(only_value(query(mixed, ResourceProfileMetric::kAverageDurationActivity, 0, 20, "r", "A")) == 5.0);
  assert(only_value(query(mixed, ResourceProfileMetric::kMultitasking, 0, 20, "r")) == 1.0);
  const auto upper = one_case({timed("A", "r", 0, "START"), timed("A", "r", 10, "COMPLETE"), timed("B", "r", 15, "COMPLETE")});
  assert(only_value(query(upper, ResourceProfileMetric::kAverageDurationActivity, 0, 20, "r", "B")) == 5.0);
  const auto paired = one_case({timed("A", "r", 0, "start"), timed("B", "r", 2, "start"),
                                timed("A", "r", 10, "complete"), timed("B", "r", 12, "complete")});
  assert(only_value(query(paired, ResourceProfileMetric::kAverageDurationActivity, 0, 20, "r", "B")) == 2.0);
  const auto atomic = one_case({timed("A", "r", 0), timed("B", "r", 10), timed("C", "r", 15)});
  assert(only_value(query(atomic, ResourceProfileMetric::kAverageDurationActivity, 0, 20, "r", "B")) == 10.0);
  assert(only_value(query(atomic, ResourceProfileMetric::kAverageDurationActivity, 0, 20, "r", "C")) == 5.0);

  // C2: an unsorted trace runs from its first to its last supplied event
  // (-10s), completes at its last supplied event, and intervals still sort.
  const auto unsorted = one_case({timed("A", "r", 20), timed("B", "r", 10)});
  assert(only_value(query(unsorted, ResourceProfileMetric::kAverageCaseDuration, 0, 30, "r")) == -10.0);
  assert(only_value(query(unsorted, ResourceProfileMetric::kAverageCaseDuration, 19, 21, "r")) == -10.0);
  assert(raises([&] { (void)query(unsorted, ResourceProfileMetric::kAverageCaseDuration, 12, 18, "r"); }));
  assert(only_value(query(unsorted, ResourceProfileMetric::kCaseCompletions, 0, 15, "r")) == 1.0);
  assert(only_value(query(unsorted, ResourceProfileMetric::kCaseCompletions, 15, 25, "r")) == 0.0);
  assert(only_value(query(unsorted, ResourceProfileMetric::kAverageDurationActivity, 0, 30, "r", "A")) == 10.0);
  assert(only_value(query(unsorted, ResourceProfileMetric::kAverageDurationActivity, 0, 30, "r", "B")) == 0.0);

  // O4: a present empty resource is a resource; *_given selects it literally.
  const auto empty = one_case({timed("A", "", 0), timed("B", "", 9)});
  assert(only_value(query(empty, ResourceProfileMetric::kDistinctActivities, 0, 30, "", "", true)) == 2.0);
  assert(only_value(query(empty, ResourceProfileMetric::kCaseCompletions, 0, 30, "", "", true)) == 1.0);
  assert(only_value(query(empty, ResourceProfileMetric::kFractionCaseCompletions, 0, 30, "", "", true)) == 1.0);
  assert(only_value(query(empty, ResourceProfileMetric::kAverageCaseDuration, 0, 30, "", "", true)) == 9.0);
  assert(only_value(query(empty, ResourceProfileMetric::kSocialPosition, 0, 30, "", "", true)) == 1.0);
  assert(only_value(query(empty, ResourceProfileMetric::kActivityFrequency, 0, 30, "", "A", true)) == 0.5);
  assert(only_value(query(empty, ResourceProfileMetric::kAverageDurationActivity, 0, 30, "", "B", true)) == 9.0);
  assert(only_value(query(empty, ResourceProfileMetric::kMultitasking, 0, 30, "", "", true)) == 1.0);
  ResourceProfilesOptions pair; pair.metric = ResourceProfileMetric::kInteractionTwoResources;
  pair.resource_given = pair.resource2_given = true;
  assert(only_value(discover_resource_profiles(empty, pair)) == 1.0);
  (void)resource_profiles_json(query(empty, ResourceProfileMetric::kDistinctActivities, 0, 30, "", "", true));
  // A missing resource is no resource at all.
  auto missing = empty;
  for (auto& e : missing.traces[0].events) e.resource_state = FirstClassAttributeState::kMissing;
  assert(discover_resource_profiles(missing, {}).rows.empty());

  // C3: a one-resource query with no observation. The reference averages
  // raise StatisticsError; every other metric returns zero.
  {
    const auto log = one_case({timed("A", "r", 0), timed("B", "s", 10)});
    for (const auto metric : {ResourceProfileMetric::kAverageDurationActivity,
                              ResourceProfileMetric::kAverageCaseDuration}) {
      assert(raises([&] { (void)query(log, metric, 0, 50, "x", "A"); }));
      assert(raises([&] { (void)query(log, metric, 40, 50, "r", "A"); }));
      assert(raises([&] { (void)query(EventLog{}, metric, 0, 50, "x", "A"); }));
    }
    for (const auto metric : {ResourceProfileMetric::kDistinctActivities, ResourceProfileMetric::kActivityFrequency,
                              ResourceProfileMetric::kActivityCompletions, ResourceProfileMetric::kCaseCompletions,
                              ResourceProfileMetric::kFractionCaseCompletions, ResourceProfileMetric::kAverageWorkload,
                              ResourceProfileMetric::kMultitasking, ResourceProfileMetric::kSocialPosition}) {
      for (const auto& [lo, hi, resource] : std::vector<std::tuple<int, int, std::string>>{{0, 50, "x"}, {40, 50, "r"}}) {
        const auto zero = query(log, metric, lo, hi, resource, "A");
        assert(only_value(zero) == 0.0 && zero.rows[0].count == 0 && zero.rows[0].resource == resource);
        (void)resource_profiles_json(zero);
      }
      assert(only_value(query(EventLog{}, metric, 0, 50, "x", "A")) == 0.0);
    }
    // Without a resource the native all-resources form has no rows.
    assert(query(log, ResourceProfileMetric::kAverageCaseDuration, 40, 50, "").rows.empty());
  }

  // Explicit start timestamps set each interval's start.
  auto explicit_start = one_case({timed("A", "r", 10), timed("B", "r", 15)});
  explicit_start.traces[0].events[0].attributes.push_back({"st", "2026-01-01T00:00:04Z"});
  explicit_start.traces[0].events[1].attributes.push_back({"st", "2026-01-01T00:00:14Z"});
  ResourceProfilesOptions with_start; with_start.metric = ResourceProfileMetric::kAverageDurationActivity;
  with_start.start_timestamp_attribute = "st"; with_start.resource = "r";
  const auto starts = discover_resource_profiles(explicit_start, with_start);
  assert(starts.rows.size() == 2 && starts.rows[0].value == 6.0 && starts.rows[1].value == 1.0);
}

// pm4py 2.7.22 resource_profiles on read_csv + format_dataframe(case_id="cid",
// activity_key="act", timestamp_key="ts") of this CSV (Review AT1): keys are
// frame columns, cells are typed ("who" int, "num" float with NaN cells,
// "start" and "ts" dates), the resource argument is text.
static EventLog at1_csv_log() {
  std::istringstream csv("cid,act,ts,who,num,start,region\n"
                         "c1,a,2020-01-01 10:00:00,1,,2020-01-01 09:00:00,N\n"
                         "c1,b,2020-01-01 11:00:00,2,1,2020-01-01 10:30:00,N\n"
                         "c2,a,2020-01-02 10:00:00,1,2,2020-01-02 09:00:00,N\n"
                         "c2,c,2020-01-02 13:00:00,1,3,2020-01-02 11:00:00,N\n"
                         "c3,a,2020-01-03 10:00:00,2,1,2020-01-03 09:30:00,M\n"
                         "c3,c,2020-01-03 10:30:00,10,,2020-01-03 10:00:00,M\n");
  CsvImportOptions import;
  import.case_id_column = "cid";
  import.activity_column = "act";
  import.timestamp_column = "ts";
  import.resource_column = "who";
  auto log = import_csv_event_log(csv, import);
  assert(log.dataframe_columns);
  return log;
}

static bool raises_text(const std::function<void()>& call, const std::string& text) {
  try {
    call();
  } catch (const std::invalid_argument& error) {
    if (std::string(error.what()).find(text) != std::string::npos) return true;
    std::fprintf(stderr, "unexpected error: %s\n", error.what());
    return false;
  }
  return false;
}

static void frame_path_reads_pm4py_frame() {
  const auto log = at1_csv_log();
  const auto profile = [&](ResourceProfileMetric metric, const std::string& resource_key,
                           std::map<std::string, std::string> extra = {}) {
    ResourceProfilesOptions o;
    o.metric = metric;
    o.resource_attribute = resource_key;
    o.t1 = "2019-01-01 00:00:00";
    o.t2 = "2021-01-01 00:00:00";
    if (extra.contains("resource")) { o.resource = extra["resource"]; o.resource_given = true; }
    if (extra.contains("activity")) { o.activity = extra["activity"]; o.activity_given = true; }
    if (extra.contains("case")) o.case_id_attribute = extra["case"];
    if (extra.contains("timestamp")) o.timestamp_attribute = extra["timestamp"];
    if (extra.contains("start")) o.start_timestamp_attribute = extra["start"];
    if (extra.contains("activity_key")) o.activity_attribute = extra["activity_key"];
    if (extra.contains("t1")) o.t1 = extra["t1"];
    if (extra.contains("t2")) o.t2 = extra["t2"];
    return discover_resource_profiles(log, o);
  };
  const auto values = [](const ResourceProfilesResult& result) {
    std::map<std::string, double> out;
    for (const auto& row : result.rows) out[row.resource] = row.value;
    return out;
  };
  using M = ResourceProfileMetric;
  // No org:resource column on this frame.
  assert(raises_text([&] { (void)profile(M::kDistinctActivities, "org:resource"); },
                     "KeyError: \"['org:resource'] not in index\""));
  // Int resources are named by their value; the text "1" matches no int.
  assert((values(profile(M::kActivityCompletions, "who")) == std::map<std::string, double>{{"1", 3}, {"10", 1}, {"2", 2}}));
  assert(only_value(profile(M::kActivityCompletions, "who", {{"resource", "1"}})) == 0.0);
  assert((values(profile(M::kActivityCompletions, "num")) == std::map<std::string, double>{{"1.0", 2}, {"2.0", 1}, {"3.0", 1}}));
  // average_case_duration groups the resource's cases by the case column.
  assert((values(profile(M::kAverageCaseDuration, "who", {{"case", "region"}})) ==
          std::map<std::string, double>{{"1", 97200}, {"10", 1800}, {"2", 2700}}));
  assert(raises_text([&] { (void)profile(M::kAverageCaseDuration, "who", {{"start", "start"}}); }, "KeyError: 'start'"));
  assert(raises_text([&] { (void)profile(M::kSocialPosition, "who", {{"case", "region"}}); },
                     "KeyError: \"None of ['case:concept:name'] are in the columns\""));
  assert((values(profile(M::kSocialPosition, "who")) == std::map<std::string, double>{{"1", 2.0 / 3}, {"10", 1.0 / 3}, {"2", 2.0 / 3}}));
  // A timestamp column of no dates fails once rows are compared.
  assert(raises_text([&] { (void)profile(M::kCaseCompletions, "who", {{"timestamp", "@@index"}}); },
                     "TypeError: '>=' not supported between instances of 'int' and 'datetime.datetime'"));
  assert(only_value(profile(M::kDistinctActivities, "who", {{"timestamp", "@@index"}, {"resource", "Q"}})) == 0.0);
  // `==` parses text against a datetime column, isin does not.
  assert(only_value(profile(M::kActivityCompletions, "ts", {{"resource", "2020-01-01 10:00:00"}})) == 1.0);
  assert(only_value(profile(M::kSocialPosition, "ts", {{"resource", "2020-01-01 10:00:00"}})) == 0.0);
  // Durations from the start column, or from the previous event of the case.
  assert((values(profile(M::kAverageDurationActivity, "who", {{"activity", "a"}, {"start", "start"}})) ==
          std::map<std::string, double>{{"1", 3600}, {"2", 1800}}));
  assert((values(profile(M::kAverageDurationActivity, "who", {{"activity", "c"}})) ==
          std::map<std::string, double>{{"1", 10800}, {"10", 1800}}));
  // A column selected twice.
  assert(raises_text([&] { (void)profile(M::kActivityFrequency, "who", {{"activity_key", "who"}, {"resource", "1"}, {"activity", "a"}}); },
                     "cannot reindex on an axis with duplicate labels"));
  // The resource on the timestamp column: both masks keep every row.
  assert(only_value(profile(M::kActivityCompletions, "time:timestamp",
                            {{"resource", "Q"}, {"t1", "2020-01-02 00:00:00"}, {"t2", "2020-01-02 12:00:00"}})) == 6.0);
  // A case column with NaN cells: the groupby drops them.
  assert(values(profile(M::kCaseCompletions, "who", {{"case", "num"}})).at("1") == 2.0);
}

// Review AU1: pm4py 2.7.22 on read_csv + format_dataframe(case_id="cid",
// activity_key="act", timestamp_key="ts") of small CSVs, and read_xes.
static EventLog au_csv(const std::string& text) {
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

static void review_au_frame_paths() {
  using M = ResourceProfileMetric;
  const auto run = [](const EventLog& log, M metric, const std::string& resource,
                      std::map<std::string, std::string> extra = {}) {
    ResourceProfilesOptions o;
    o.metric = metric;
    o.resource_attribute = extra.contains("resource_key") ? extra["resource_key"] : "who";
    o.t1 = "2019-01-01 00:00:00";
    o.t2 = "2021-01-01 00:00:00";
    o.resource = resource;
    o.resource_given = true;
    if (extra.contains("resource2")) { o.resource2 = extra["resource2"]; o.resource2_given = true; }
    if (extra.contains("activity")) { o.activity = extra["activity"]; o.activity_given = true; }
    if (extra.contains("case")) o.case_id_attribute = extra["case"];
    if (extra.contains("timestamp")) o.timestamp_attribute = extra["timestamp"];
    if (extra.contains("start")) o.start_timestamp_attribute = extra["start"];
    if (extra.contains("activity_key")) o.activity_attribute = extra["activity_key"];
    return discover_resource_profiles(log, o);
  };
  const std::string no_dt = "AttributeError: Can only use .dt accessor with datetimelike values";
  // Every case one event: "n" int64, "f" float64 with NaN, "s" text with
  // NaN, "b" bools with NaN (an object column).
  const auto single = au_csv("cid,act,ts,who,n,f,s,b\n"
                             "c1,a,2020-01-01 10:00:00,R,1,,x,True\n"
                             "c2,a,2020-01-02 10:00:00,R,2,,,\n"
                             "c3,b,2020-01-03 10:00:00,S,3,1.5,y,False\n");
  const auto ada = [&](const std::string& resource, const std::string& activity,
                       std::map<std::string, std::string> extra = {}) {
    extra["activity"] = activity;
    return [&single, resource, extra, &run] { (void)run(single, M::kAverageDurationActivity, resource, extra); };
  };
  // Without a start column (the previous event of the case, an object
  // column of dates when no case has two events) one selected row works,
  // none or two have no .dt accessor.
  assert(only_value(run(single, M::kAverageDurationActivity, "S", {{"activity", "b"}})) == 0.0);
  assert(raises_text(ada("R", "a"), no_dt));
  assert(raises_text(ada("Q", "a"), no_dt));
  // The start column's dtype decides pandas' subtraction.
  assert(raises_text(ada("R", "a", {{"start", "n"}}),
                     "TypeError: Addition/subtraction of integers and integer-arrays with DatetimeArray is no "
                     "longer supported.  Instead of adding/subtracting `n`, use `n * obj.freq`"));
  assert(raises_text(ada("R", "a", {{"start", "f"}}), "TypeError: unsupported operand type(s) for -: 'Timestamp' and 'float'"));
  assert(raises_text(ada("Q", "a", {{"start", "f"}}), no_dt));
  assert(raises_text(ada("R", "a", {{"start", "s"}}), "TypeError: unsupported operand type(s) for -: 'DatetimeArray' and 'str'"));
  assert(raises_text(ada("S", "b", {{"start", "b"}}), "TypeError: unsupported operand type(s) for -: 'DatetimeArray' and 'bool'"));
  assert(raises_text(ada("R", "a", {{"start", "b"}}), "TypeError: unsupported operand type(s) for -: 'Timestamp' and 'bool'"));
  // The compared rows are the resource's rows of the activity.
  assert(raises_text(ada("R", "zz", {{"timestamp", "@@index"}}), no_dt));
  assert(raises_text([&] { (void)run(single, M::kActivityFrequency, "R", {{"activity", "zz"}, {"timestamp", "@@index"}}); },
                     "TypeError: '>=' not supported between instances of 'int' and 'datetime.datetime'"));
  // A float64 timestamp column compares its NaN cells too.
  assert(raises_text([&] { (void)run(single, M::kActivityCompletions, "R", {{"timestamp", "f"}}); },
                     "TypeError: '>=' not supported between instances of 'float' and 'datetime.datetime'"));
  assert(only_value(run(single, M::kActivityCompletions, "Q", {{"timestamp", "f"}})) == 0.0);
  // interaction_two_resources filters the cases (case:concept:name) with
  // both resources, then counts the groups of the case column.
  const auto pairs = au_csv("cid,act,ts,who,grp\n"
                            "c1,a,2020-01-01 10:00:00,R,g1\n"
                            "c1,b,2020-01-01 11:00:00,S,g2\n"
                            "c2,a,2020-01-02 10:00:00,R,g1\n"
                            "c2,b,2020-01-02 11:00:00,T,g1\n"
                            "c3,a,2020-01-03 10:00:00,S,g2\n");
  const auto interaction = [&](std::map<std::string, std::string> extra) {
    if (!extra.contains("resource2")) extra["resource2"] = "S";
    return run(pairs, M::kInteractionTwoResources, "R", extra);
  };
  assert(only_value(interaction({{"case", "grp"}, {"activity_key", "case:concept:name"}})) == 2.0);
  assert(only_value(interaction({{"case", "grp"}, {"activity_key", "case:concept:name"}, {"resource2", "T"}})) == 1.0);
  assert(raises_text([&] { (void)interaction({{"case", "grp"}}); },
                     "KeyError: \"None of ['case:concept:name'] are in the columns\""));
  assert(raises_text([&] { (void)interaction({{"case", "who"}}); }, "ValueError: cannot reindex on an axis with duplicate labels"));
  assert(raises_text([&] { (void)interaction({{"case", "grp"}, {"activity_key", "case:concept:name"}, {"timestamp", "grp"}}); },
                     "ValueError: Grouper for 'grp' not 1-dimensional"));
  // Workload and average_duration_activity without a start column sort the
  // frame by the case and timestamp columns: a Timestamp beside a number
  // cannot be an ordered Categorical.
  std::istringstream xes(
      R"XES(<?xml version="1.0" encoding="UTF-8"?><log xes.version="1.0"><trace><string key="concept:name" value="c0"/><event><string key="concept:name" value="a"/><date key="time:timestamp" value="2020-01-01T00:00:00+00:00"/><string key="org:resource" value="R"/><date key="x" value="2020-01-01T00:00:00+00:00"/></event><event><string key="concept:name" value="b"/><date key="time:timestamp" value="2020-01-01T01:00:00+00:00"/><string key="org:resource" value="R"/><float key="x" value="1.5"/></event></trace><trace><string key="concept:name" value="c1"/><event><string key="concept:name" value="a"/><date key="time:timestamp" value="2020-01-02T00:00:00+00:00"/><string key="org:resource" value="S"/><string key="x" value="z"/></event></trace></log>)XES");
  const auto mixed = import_xes_dataframe_log(xes);
  const std::string unordered =
      "TypeError: 'values' is not ordered, please explicitly specify the categories order by passing in a "
      "categories argument.";
  assert(raises_text([&] { (void)run(mixed, M::kAverageWorkload, "R", {{"resource_key", "org:resource"}, {"timestamp", "x"}}); }, unordered));
  assert(raises_text([&] { (void)run(mixed, M::kAverageWorkload, "R", {{"resource_key", "org:resource"}, {"case", "x"}}); }, unordered));
  assert(raises_text([&] { (void)run(mixed, M::kAverageDurationActivity, "R", {{"resource_key", "org:resource"}, {"timestamp", "x"}, {"activity", "a"}}); }, unordered));
}

// Review AV1: pm4py 2.7.22 on read_csv + format_dataframe of small CSVs.
static void review_av_frame_paths() {
  using M = ResourceProfileMetric;
  const auto run = [](const EventLog& log, M metric, const std::string& resource, const std::string& t1,
                      const std::string& t2, std::map<std::string, std::string> extra = {}) {
    ResourceProfilesOptions o;
    o.metric = metric;
    o.resource_attribute = "who";
    o.t1 = t1;
    o.t2 = t2;
    o.resource = resource;
    o.resource_given = true;
    if (extra.contains("activity")) { o.activity = extra["activity"]; o.activity_given = true; }
    if (extra.contains("timestamp")) o.timestamp_attribute = extra["timestamp"];
    if (extra.contains("start")) o.start_timestamp_attribute = extra["start"];
    return discover_resource_profiles(log, o);
  };
  const std::string lo = "2019-01-01 00:00:00", hi = "2021-01-01 00:00:00";
  // multitasking sums each distinct (start, end, activity) interval once,
  // while a copy still counts as overlapping: 2/3, not 1/2.
  const auto copies = au_csv("cid,act,ts,who,st\n"
                             "c1,a,2020-01-01 10:00:00,R,2020-01-01 09:00:00\n"
                             "c1,a,2020-01-01 10:00:00,R,2020-01-01 09:00:00\n"
                             "c2,b,2020-01-01 10:30:00,R,2020-01-01 09:30:00\n"
                             "c2,c,2020-01-01 12:00:00,R,2020-01-01 11:00:00\n");
  assert(std::abs(only_value(run(copies, M::kMultitasking, "R", lo, hi, {{"start", "st"}})) - 2.0 / 3.0) < 1e-12);
  // average_case_duration on a frame keeps a case only when it starts
  // before t2 and ends after t1, both strictly.
  const auto cases = au_csv("cid,act,ts,who,f\n"
                            "c1,a,2020-01-01 10:00:00,R,\n"
                            "c1,b,2020-01-01 12:00:00,R,\n"
                            "c2,a,2020-01-02 10:00:00,R,\n"
                            "c2,b,2020-01-02 11:00:00,R,\n"
                            "c3,a,2020-01-03 10:00:00,R,\n");
  assert(only_value(run(cases, M::kAverageCaseDuration, "R", lo, hi)) == 3600.0);
  assert(only_value(run(cases, M::kAverageCaseDuration, "R", "2020-01-01 12:00:00", hi)) == 1800.0);
  assert(only_value(run(cases, M::kAverageCaseDuration, "R", lo, "2020-01-02 10:00:00")) == 7200.0);
  assert(raises_text([&] { (void)run(cases, M::kAverageCaseDuration, "R", "2020-01-03 10:00:00", hi); },
                     "mean requires at least one data point"));
  // An all-NaN float64 timestamp column compares its NaN cells.
  assert(raises_text([&] { (void)run(cases, M::kAverageCaseDuration, "R", lo, hi, {{"timestamp", "f"}}); },
                     "TypeError: '<' not supported between instances of 'float' and 'datetime.datetime'"));
  // The activity column as the timestamp: distinct_activities and
  // activity_frequency compare the resource's rows before the selection
  // realigns, so only a resource without rows reaches the realignment.
  for (const auto metric : {M::kActivityFrequency, M::kDistinctActivities}) {
    const std::map<std::string, std::string> extra{{"timestamp", "concept:name"}, {"activity", "a"}};
    assert(raises_text([&] { (void)run(cases, metric, "R", lo, hi, extra); },
                       "TypeError: '>=' not supported between instances of 'str' and 'datetime.datetime'"));
    assert(raises_text([&] { (void)run(cases, metric, "Q", lo, hi, extra); },
                       "ValueError: cannot reindex on an axis with duplicate labels"));
  }
}

int main() {
  reference_audit_contracts();
  frame_path_reads_pm4py_frame();
  review_au_frame_paths();
  review_av_frame_paths();
  const auto log = sample();
  auto distinct = discover_resource_profiles(log, {});
  assert(distinct.rows.size() == 2);
  assert(distinct.rows[0].resource == "alice" && distinct.rows[0].value == 1.0);

  EventLog custom;
  Trace custom_trace;
  custom_trace.case_id = "ignored-case";
  custom_trace.attributes.push_back({"case:custom", "custom-case"});
  custom_trace.events.push_back({
      "ignored-case", "ignored", "not-a-timestamp", "ignored", 0,
      {{"team:activity", "A"}, {"team:resource", "r-custom"},
       {"observed_at", "2024-01-01T00:00:10Z"},
       {"started_at", "2024-01-01T00:00:00Z"}},
      "complete"});
  custom.traces.push_back(std::move(custom_trace));
  ResourceProfilesOptions custom_options;
  custom_options.metric = ResourceProfileMetric::kAverageDurationActivity;
  custom_options.activity_attribute = "team:activity";
  custom_options.resource_attribute = "team:resource";
  custom_options.timestamp_attribute = "observed_at";
  custom_options.start_timestamp_attribute = "started_at";
  custom_options.case_id_attribute = "case:custom";
  custom_options.resource = "r-custom";
  custom_options.activity = "A";
  custom_options.t1 = "2024-01-01T00:00:00Z";
  custom_options.t2 = "2024-01-01T00:00:20Z";
  const auto custom_profiles =
      discover_resource_profiles(custom, custom_options);
  assert(custom_profiles.rows.size() == 1 &&
         custom_profiles.rows.front().resource == "r-custom" &&
         custom_profiles.rows.front().activity == "A" &&
         custom_profiles.rows.front().value == 10.0);

  ResourceProfilesOptions f; f.metric = ResourceProfileMetric::kActivityFrequency; f.t1 = "2024-01-01T00:00:10Z"; f.t2 = "2024-01-01T00:00:20Z";
  auto freq = discover_resource_profiles(log, f);
  assert(freq.rows.size() == 2); // left boundary included, right excluded
  assert(freq.rows[0].resource == "alice" && freq.rows[0].count == 2 &&
         freq.rows[0].value == 1.0);

  ResourceProfilesOptions c; c.metric = ResourceProfileMetric::kCaseCompletions; c.t1 = "2024-01-01T00:00:00Z"; c.t2 = "2024-01-01T00:00:20Z";
  auto completions = discover_resource_profiles(log, c);
  assert(completions.rows.size() == 2 && completions.rows[0].resource == "alice");

  ResourceProfilesOptions s; s.metric = ResourceProfileMetric::kSocialPosition; s.t1 = "2024-01-01T00:00:10Z"; s.t2 = "2024-01-01T00:00:20Z";
  auto social = discover_resource_profiles(log, s);
  assert(social.rows.size() == 2);
  assert(std::abs(social.rows[0].value - 1.0) < 1e-12);
  assert(std::abs(social.rows[1].value - 0.5) < 1e-12);

  ResourceProfilesOptions interaction;
  interaction.metric = ResourceProfileMetric::kInteractionTwoResources;
  interaction.t1 = "2024-01-01T00:00:00Z";
  interaction.t2 = "2024-01-01T00:00:20Z";
  interaction.resource = "alice";
  interaction.resource2 = "bob";
  const auto together = discover_resource_profiles(log, interaction);
  assert(together.rows.size() == 1 && together.rows[0].count == 1);

  ResourceProfilesOptions duration;
  duration.metric = ResourceProfileMetric::kAverageDurationActivity;
  duration.t1 = "2024-01-01T00:00:10Z";
  duration.t2 = "2024-01-01T00:00:20Z";
  duration.resource = "alice";
  duration.activity = "A";
  const auto durations = discover_resource_profiles(log, duration);
  // Reference DataFrame path (PM4Py 2.7.22 pandas variant): A complete@10
  // follows A start@0 (10s) in case a and is first in case b (0s), so 5s.
  assert(durations.rows.size() == 1 && durations.rows[0].value == 5.0);
  {
    // Overlapping lifecycle intervals: B complete@0, D start@10 (r2),
    // C complete@50, D complete@58 (x). DataFrame path: 8; without C: 48.
    const auto overlap = one_case({timed("B", "r2", 0, "complete"), timed("D", "r2", 10, "start"),
                                   timed("C", "r1", 50, "complete"), timed("D", "x", 58, "complete")});
    assert(only_value(query(overlap, ResourceProfileMetric::kAverageDurationActivity, 0, 59, "x", "D")) == 8.0);
    const auto csc = one_case({timed("B", "r2", 0, "complete"), timed("D", "r2", 10, "start"),
                               timed("D", "x", 58, "complete")});
    assert(only_value(query(csc, ResourceProfileMetric::kAverageDurationActivity, 0, 59, "x", "D")) == 48.0);
  }

  ResourceProfilesOptions activity_completions;
  activity_completions.metric = ResourceProfileMetric::kActivityCompletions;
  activity_completions.t1 = "2024-01-01T00:00:00Z";
  activity_completions.t2 = "2024-01-01T00:00:20Z";
  activity_completions.resource = "alice";
  activity_completions.activity = "A";
  const auto completion_events = discover_resource_profiles(log, activity_completions);
  // The event-level metric counts timestamped activity events, including
  // explicit lifecycle markers, as the reference log variant does.
  assert(completion_events.rows.size() == 1 && completion_events.rows[0].count == 3);
  {
    // PM4Py 2.7.22 activity_completions(log, t1, t2, "x") takes no activity:
    // one trace A, B, C by x gives 3, a single total per resource.
    const auto abc = one_case({timed("A", "x", 1), timed("B", "x", 2), timed("C", "x", 3), timed("A", "y", 4)});
    const auto per_resource = query(abc, ResourceProfileMetric::kActivityCompletions, 0, 10, "x");
    assert(only_value(per_resource) == 3.0 && per_resource.rows[0].activity.empty());
    const auto ignores_activity = query(abc, ResourceProfileMetric::kActivityCompletions, 0, 10, "x", "A");
    assert(only_value(ignores_activity) == 3.0);
    ResourceProfilesOptions all;
    all.metric = ResourceProfileMetric::kActivityCompletions;
    const auto every = discover_resource_profiles(abc, all);
    assert(every.rows.size() == 2 && every.rows[0].resource == "x" && every.rows[0].value == 3.0 &&
           every.rows[1].resource == "y" && every.rows[1].value == 1.0);
  }

  ResourceProfilesOptions case_duration;
  case_duration.metric = ResourceProfileMetric::kAverageCaseDuration;
  case_duration.t1 = "2024-01-01T00:00:10Z";
  case_duration.t2 = "2024-01-01T00:00:20Z";
  case_duration.resource = "alice";
  const auto case_durations = discover_resource_profiles(log, case_duration);
  assert(case_durations.rows.size() == 1 &&
         case_durations.rows[0].resource == "alice" &&
         case_durations.rows[0].count == 2 &&
         case_durations.rows[0].value == 10.0);

  EventLog overlap;
  Trace overlap_trace; overlap_trace.case_id = "overlap";
  overlap_trace.events.push_back({"overlap", "A", "2024-01-01T00:00:00Z", "alice", 0, {}, "start"});
  overlap_trace.events.push_back({"overlap", "B", "2024-01-01T00:00:05Z", "alice", 1, {}, "start"});
  overlap_trace.events.push_back({"overlap", "A", "2024-01-01T00:00:10Z", "alice", 2, {}, "complete"});
  overlap_trace.events.push_back({"overlap", "B", "2024-01-01T00:00:15Z", "alice", 3, {}, "complete"});
  overlap.traces.push_back(overlap_trace);
  ResourceProfilesOptions multitasking;
  multitasking.metric = ResourceProfileMetric::kMultitasking;
  multitasking.t1 = "2024-01-01T00:00:00Z";
  multitasking.t2 = "2024-01-01T00:00:20Z";
  multitasking.resource = "alice";
  const auto multitask = discover_resource_profiles(overlap, multitasking);
  assert(multitask.rows.size() == 1 && multitask.rows[0].value == 1.0);
  ResourceProfilesOptions workload = multitasking;
  workload.metric = ResourceProfileMetric::kAverageWorkload;
  workload.t2 = "2024-01-01T00:00:10Z";
  const auto average_workload = discover_resource_profiles(overlap, workload);
  // Reference DataFrame path (the path imported logs follow): every event
  // spans from the previous event of its case, so the A complete@10 interval
  // [5,10] ends at t2 = 10s and counts (3.0); the EventLog path pairs
  // lifecycles and gives 0.0. Both give 2.0 at 12s.
  assert(only_value(average_workload) == 3.0);
  workload.t2 = "2024-01-01T00:00:12Z";
  assert(only_value(discover_resource_profiles(overlap, workload)) == 2.0);
  {
    auto P = one_case({timed("A", "alice", 0), timed("B", "alice", 5), timed("C", "alice", 10)});
    auto P2 = one_case({timed("X", "alice", 2), timed("Y", "alice", 8)});
    P2.traces[0].case_id = "d"; P.traces.push_back(P2.traces[0]);
    assert(std::abs(only_value(query(P, ResourceProfileMetric::kAverageWorkload, 0, 7, "alice")) - 39.0 / 11.0) < 1e-12);
    assert(only_value(query(P, ResourceProfileMetric::kAverageWorkload, 3, 9, "alice")) == 3.0);
  }
  ResourceProfilesOptions capped;
  capped.max_rows = 1;
  const auto truncated = discover_resource_profiles(log, capped);
  assert(truncated.rows.size() == 1 && truncated.diagnostics.output_truncated);
  assert(!resource_profiles_json(truncated).empty());
  for (const auto metric : {ResourceProfileMetric::kSocialPosition,
                            ResourceProfileMetric::kInteractionTwoResources}) {
    ResourceProfilesOptions absent;
    absent.metric = metric;
    absent.resource = "not-in-source";
    absent.resource2 = "also-absent";
    const auto zero = discover_resource_profiles(EventLog{}, absent);
    assert(zero.diagnostics.resource_count == 0 && zero.rows.size() == 1 &&
           zero.rows[0].count == 0 && zero.rows[0].value == 0.0);
    const auto zero_json = resource_profiles_json(zero);
    assert(zero_json.find("not-in-source") != std::string::npos &&
           zero_json.find("\"resource_count\":0") != std::string::npos);
    auto bad_zero = zero;
    bad_zero.rows[0].count = 1;
    try { (void)resource_profiles_json(bad_zero); assert(false); }
    catch (const std::invalid_argument&) {}
  }

  ResourceProfilesOptions bad; bad.t1 = "bad";
  try { (void)discover_resource_profiles(log, bad); assert(false); } catch (const std::invalid_argument&) {}
  bad = {}; bad.t1 = "2024-01-02"; bad.t2 = "2024-01-01";
  try { (void)discover_resource_profiles(log, bad); assert(false); } catch (const std::invalid_argument&) {}

  const auto json = resource_profiles_json(distinct);
  assert(json.find("RESOURCE_PROFILES") != std::string::npos);
  assert(json.find("distinct_activities") != std::string::npos);
  auto malformed_result = distinct;
  malformed_result.rows[0].value = std::numeric_limits<double>::infinity();
  try { (void)resource_profiles_json(malformed_result); assert(false); }
  catch (const std::invalid_argument&) {}
  ResourceProfilesResult escaped;
  escaped.diagnostics.message = "line\n\\ok";
  escaped.diagnostics.resource_count = 1;
  escaped.rows.push_back({"a\"b", "x\ny", "", 1.0, 1});
  const auto escaped_json = resource_profiles_json(escaped);
  assert(escaped_json.find("a\\\"b") != std::string::npos);
  assert(escaped_json.find("x\\u000a") != std::string::npos);
}
