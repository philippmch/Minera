#include "minera/resource_profiles.hpp"

#include <cassert>
#include <functional>
#include <map>
#include <optional>
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
  // An EventLog follows pm4py's log variant, which pairs the transitions
  // (round FP): A's interval is 0s..10s, and without a start key it starts
  // at its own end, as the first of the case (PM4Py 2.7.22 EventLog variant:
  // 0.0; the pandas variant of a frame, where every event takes part, 5.0).
  assert(only_value(query(mixed, ResourceProfileMetric::kAverageDurationActivity, 0, 20, "r", "A")) == 0.0);
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

// Round FO: an EventLog without frame columns (a raw log) follows pm4py
// 2.7.22's log variant; values from pm4py on the same EventLogs (CSV-read
// Timestamps), t1/t2 as get_dt_from_string reads them (UTC).
static void review_fo_log_variant() {
  using M = ResourceProfileMetric;
  const auto ev = [](std::string act, std::string ts, std::string res, std::string st = "") {
    Event e{"", std::move(act), std::move(ts), std::move(res), 0, {}, ""};
    if (!st.empty()) e.attributes.push_back({"st", st});
    return e;
  };
  const auto make = [](std::vector<std::pair<std::string, std::vector<Event>>> traces) {
    EventLog log;
    for (auto& [id, events] : traces) {
      Trace trace; trace.case_id = id;
      for (auto& e : events) { e.case_id = id; trace.events.push_back(e); }
      log.traces.push_back(std::move(trace));
    }
    return log;
  };
  const auto run = [](const EventLog& log, M metric, std::string resource, std::string t1, std::string t2,
                      std::string activity = "", std::string key = "", std::string start = "") {
    ResourceProfilesOptions o; o.metric = metric; o.resource = std::move(resource);
    o.t1 = std::move(t1); o.t2 = std::move(t2); o.activity = std::move(activity);
    o.timestamp_attribute = std::move(key); o.start_timestamp_attribute = std::move(start);
    return discover_resource_profiles(log, o);
  };
  const std::string lo = "2020-01-09 00:00:00", hi = "2020-01-11 00:00:00";
  const auto l1 = make({{"c1", {ev("A", "2020-01-10 01:00:00.123456789+05:30", "r1", "2020-01-10 00:30:00.000000250+05:30"),
                                ev("B", "2020-01-10 03:00:00.987654321+05:30", "r1", "2020-01-10 02:00:00.1+05:30")}},
                        {"c2", {ev("A", "2020-01-10 02:00:00.5+05:30", "r2", "2020-01-10 01:00:00+05:30"),
                                ev("B", "2020-01-10 05:00:00.000000999+05:30", "r1", "2020-01-10 04:00:00+05:30")}}});
  // Timestamp.timestamp() seconds (microseconds) and statistics.mean.
  assert(only_value(run(l1, M::kAverageCaseDuration, "r1", lo, hi)) == 9000.182098984718);
  assert(only_value(run(l1, M::kAverageDurationActivity, "r1", lo, hi, "B")) == 9000.182098984718);
  assert(only_value(run(l1, M::kAverageDurationActivity, "r1", lo, hi, "B", "", "st")) == 3600.44382750988);
  // A start key naming the log's own time: every interval has no length.
  assert(only_value(run(l1, M::kAverageWorkload, "r1", lo, hi, "", "", "time:timestamp")) == 0.0);
  assert(only_value(run(l1, M::kAverageWorkload, "r1", lo, "2020-01-09 22:00:00")) == 2.0);
  // t1 >= t2: an empty window (average_workload reads t2 only).
  assert(only_value(run(l1, M::kDistinctActivities, "r1", hi, lo)) == 0.0);
  assert(only_value(run(l1, M::kAverageWorkload, "r1", "2020-01-09 22:00:00", "2020-01-09 22:00:00")) == 2.0);
  // Naive Timestamps compared with the UTC-aware bounds: pandas words the
  // error by the compared Timestamp's nanoseconds; the intervals compare
  // seconds and do not raise.
  const auto l2 = make({{"c1", {ev("A", "2020-01-10 01:00:00.000000001", "r1"), ev("B", "2020-01-10 03:00:00", "r1")}}});
  assert(raises_text([&] { (void)run(l2, M::kDistinctActivities, "r1", lo, hi); },
                     "TypeError: Cannot compare tz-naive and tz-aware timestamps"));
  assert(raises_text([&] { (void)run(l2, M::kCaseCompletions, "r1", lo, hi); },
                     "TypeError: can't compare offset-naive and offset-aware datetimes"));
  assert(only_value(run(l2, M::kAverageWorkload, "r1", lo, hi)) == 0.0);
  // A timestamp key an event lacks; a start key the first event of the
  // sorted log lacks makes every interval start at its own time.
  const auto l3 = make({{"c1", {ev("A", "2020-01-10 01:00:00Z", "r1", "2020-01-10 00:00:00Z"), ev("B", "2020-01-10 03:00:00Z", "r1")}},
                        {"c2", {ev("A", "2020-01-09 01:00:00Z", "r2"), ev("B", "2020-01-10 05:00:00Z", "r1", "2020-01-10 04:00:00Z")}}});
  assert(raises_text([&] { (void)run(l3, M::kDistinctActivities, "r1", lo, hi, "", "st"); }, "KeyError: 'st'"));
  assert(only_value(run(l3, M::kAverageWorkload, "r1", lo, "2020-01-10 04:30:00", "", "", "st")) == 0.0);
  assert(only_value(run(l3, M::kMultitasking, "r1", lo, hi, "", "", "st")) == 0.0);
  // With the start key on the first sorted event, a resource's event that
  // lacks it is pm4py's KeyError.
  auto l4 = l3; l4.traces[1].events[0].attributes.push_back({"st", "2020-01-09 00:00:00Z"});
  assert(raises_text([&] { (void)run(l4, M::kAverageWorkload, "r1", lo, "2020-01-10 04:30:00", "", "", "st"); },
                     "KeyError: 'st'"));
  // The social window is closed.
  assert(only_value(run(l3, M::kSocialPosition, "r1", "2020-01-10 01:00:00", "2020-01-10 05:00:00")) == 1.0);
  // Social position and average_case_duration compare the events' wall
  // clocks (offset dropped, nanoseconds kept) with the naive bounds, so a
  // naive log does not raise there; interaction_two_resources compares only
  // the cases holding both resources.
  const auto l5 = make({{"c1", {ev("A", "2020-01-10 03:00:00+05:30", "r1"), ev("B", "2020-01-10 03:30:00.000000001+05:30", "r1")}}});
  assert(only_value(run(l5, M::kSocialPosition, "r1", "2020-01-10 00:00:00", "2020-01-10 04:00:00")) == 1.0);
  assert(only_value(run(l5, M::kSocialPosition, "r1", "2020-01-09 21:00:00", "2020-01-09 23:00:00")) == 0.0);
  assert(only_value(run(l5, M::kSocialPosition, "r1", "2020-01-10 03:30:00", "2020-01-10 03:30:00")) == 0.0);
  assert(only_value(run(l5, M::kAverageCaseDuration, "r1", "2020-01-10 00:00:00", "2020-01-10 04:00:00")) == 1800.0);
  assert(raises_text([&] { (void)run(l5, M::kAverageCaseDuration, "r1", "2020-01-09 21:00:00", "2020-01-09 23:00:00"); },
                     "mean requires at least one data point"));
  assert(only_value(run(l2, M::kSocialPosition, "r1", lo, hi)) == 1.0);
  ResourceProfilesOptions pair; pair.metric = M::kInteractionTwoResources; pair.t1 = lo; pair.t2 = hi;
  pair.resource = "zz"; pair.resource2 = "r1";
  assert(only_value(discover_resource_profiles(l2, pair)) == 0.0);
}

// Review FP2 (pm4py's log variant with lifecycle transitions, observed
// black-box): when the first event of the sorted log lacks the start key
// ("start_timestamp" without one), the log becomes intervals first: each
// complete (or unmarked) event pairs with the oldest pending start of its
// activity (transitions case-insensitive, others dropped), each case sorted
// by start; without a start key an interval starts at the previous one's
// end and a case the conversion empties raises IndexError.
static void review_fp_log_lifecycle() {
  using M = ResourceProfileMetric;
  const auto at = [](int seconds) {
    const int day = seconds < 0 ? 5 : 6;
    const int s = seconds < 0 ? seconds + 86400 : seconds;
    char text[64];
    std::snprintf(text, sizeof text, "2020-01-%02d %02d:%02d:%02d+00:00", day, s / 3600, s / 60 % 60, s % 60);
    return std::string(text);
  };
  struct E { std::string act; int t; std::string lc; std::optional<int> st; };
  const auto make = [&](std::vector<std::vector<E>> traces) {
    EventLog log;
    for (std::size_t i = 0; i < traces.size(); ++i) {
      Trace trace; trace.case_id = "c" + std::to_string(i);
      for (const auto& x : traces[i]) {
        Event e{trace.case_id, x.act, at(x.t), "r1", 0, {}, ""};
        if (!x.lc.empty()) e.attributes.push_back({"lifecycle:transition", x.lc});
        if (x.st) e.attributes.push_back({"st", at(*x.st)});
        trace.events.push_back(e);
      }
      log.traces.push_back(std::move(trace));
    }
    return log;
  };
  const auto run = [](const EventLog& log, M metric, std::string start = "") {
    ResourceProfilesOptions o; o.metric = metric; o.resource = "r1";
    o.t1 = "2020-01-01 00:00:00"; o.t2 = "2021-01-01 00:00:00";
    if (metric == M::kAverageDurationActivity) o.activity = "A";
    o.start_timestamp_attribute = std::move(start);
    return discover_resource_profiles(log, o);
  };
  const auto ada = [&](std::vector<std::vector<E>> traces, std::string start) {
    return only_value(run(make(std::move(traces)), M::kAverageDurationActivity, std::move(start)));
  };
  const std::optional<int> none;
  // A start and its complete pair; the previous interval's end wins without a key.
  assert(ada({{{"A", 0, "start", none}, {"A", 100, "complete", none}}}, "") == 0.0);
  assert(ada({{{"A", 0, "start", none}, {"A", 100, "complete", none}}}, "st") == 100.0);
  assert(ada({{{"A", 0, "START", none}, {"A", 100, "COMPLETE", none}}}, "st") == 100.0);
  // An unpaired complete starts at its own time, whatever its start key.
  assert(ada({{{"Z", -50, "complete", none}, {"A", 100, "complete", 50}}}, "") == 150.0);
  assert(ada({{{"Z", -50, "complete", none}, {"A", 100, "complete", 50}}}, "st") == 0.0);
  assert(ada({{{"Z", -50, "start", none}, {"A", 100, "", 50}}}, "") == 0.0);
  // Pairs per activity, oldest start first, sorted by start.
  assert(ada({{{"A", 0, "start", none}, {"B", 10, "start", none}, {"A", 100, "complete", none}, {"B", 200, "complete", none}}}, "st") == 100.0);
  assert(ada({{{"A", 0, "start", none}, {"A", 10, "start", none}, {"A", 100, "complete", none}, {"A", 200, "complete", none}}}, "") == 50.0);
  assert(ada({{{"A", 0, "start", none}, {"A", 10, "start", none}, {"A", 100, "complete", none}, {"A", 200, "complete", none}}}, "st") == 145.0);
  assert(ada({{{"A", 0, "schedule", none}, {"A", 100, "complete", none}}}, "st") == 0.0);
  // The start key on the first sorted event: no conversion, so the complete
  // event without it is pm4py's KeyError.
  assert(ada({{{"A", 0, "start", -40}, {"A", 100, "complete", none}}}, "") == 0.0);
  assert(raises_text([&] { (void)run(make({{{"A", 0, "start", -40}, {"A", 100, "complete", none}}}), M::kAverageDurationActivity, "st"); },
                     "KeyError: 'st'"));
  // A case the conversion empties raises without a start key (an empty case
  // does not); the first sorted event decides the conversion.
  const auto starts_only = make({{{"A", 100, "complete", 50}}, {{"B", 0, "start", none}}});
  for (const auto metric : {M::kAverageDurationActivity, M::kMultitasking, M::kAverageWorkload})
    assert(raises_text([&] { (void)run(starts_only, metric); }, "IndexError: list index out of range"));
  assert(raises_text([&] { (void)run(make({{{"A", 100, "complete", 50}}, {{"B", 0, "schedule", none}}}), M::kAverageWorkload); },
                     "IndexError: list index out of range"));
  assert(only_value(run(starts_only, M::kAverageDurationActivity, "st")) == 0.0);
  assert(only_value(run(starts_only, M::kMultitasking, "st")) == 0.0);
  const auto late_start = make({{{"A", 100, "complete", 50}}, {{"B", 200, "start", none}}});
  assert(only_value(run(late_start, M::kAverageDurationActivity, "st")) == 50.0);
  assert(raises_text([&] { (void)run(late_start, M::kMultitasking, "st"); }, "KeyError: 'st'"));
  assert(ada({{{"A", 100, "complete", 50}}, {}}, "") == 0.0);
}

// Review FP1 (pm4py's pandas variant on a CSV frame with nanosecond times,
// observed black-box): the window compares the datetime64 cells exactly (a
// time a nanosecond before t1 is out, two before t2 in), the view keeps the
// nanoseconds, average_duration_activity is pandas' mean of the exact
// Timedelta seconds (sorted by start, timestamp and position without a start
// column, the frame's order with one, a NaT start a zero that does not
// count), average_case_duration statistics.mean of the exact durations.
static void review_fp_frame_exact() {
  using M = ResourceProfileMetric;
  const auto log = au_csv(
      "cid,act,ts,who,st\n"
      "c0,a,2020-01-10 04:07:14.439589176,R,2020-01-10 04:06:45.598385933\n"
      "c0,b,2020-01-10 04:17:55.960338225,R,2020-01-10 04:15:57.554655321\n"
      "c0,a,2020-01-10 04:25:37.384332940,R,2020-01-10 04:23:28.496265830\n"
      "c0,b,2020-01-10 04:35:59.410926396,R,2020-01-10 04:35:25.937392936\n"
      "c0,a,2020-01-10 04:40:04.122078729,R,2020-01-10 04:38:47.244949619\n"
      "c0,b,2020-01-10 04:49:57.447136430,R,2020-01-10 04:49:00.623808871\n"
      "c0,a,2020-01-10 04:51:42.125786802,R,2020-01-10 04:47:10.145715038\n"
      "c1,a,2020-01-10 05:14:12.240019583,R,2020-01-10 05:09:57.725111887\n"
      "c1,b,2020-01-10 05:18:44.110448685,R,2020-01-10 05:17:01.112389170\n"
      "c1,a,2020-01-10 05:24:12.995094237,R,2020-01-10 05:19:40.552813590\n"
      "c1,b,2020-01-10 05:32:25.754309630,R,\n"
      "c1,a,2020-01-10 05:43:32.710923783,R,2020-01-10 05:42:49.030960887\n"
      "c1,b,2020-01-10 05:46:34.895374064,R,2020-01-10 05:43:26.162026862\n"
      "c1,a,2020-01-10 05:47:59.369717953,R,2020-01-10 05:44:53.404886066\n"
      "c2,a,2020-01-10 06:09:08.013645774,R,2020-01-10 06:08:31.161371344\n"
      "c2,b,2020-01-10 06:14:08.423763621,R,2020-01-10 06:13:31.907153775\n"
      "c2,a,2020-01-10 06:24:42.563353383,R,2020-01-10 06:20:34.712950793\n"
      "c2,b,2020-01-10 06:31:11.093376186,R,2020-01-10 06:26:40.876051898\n"
      "c2,a,2020-01-10 06:36:27.925524348,R,2020-01-10 06:32:52.599855820\n"
      "c2,b,2020-01-10 06:39:28.660244836,R,2020-01-10 06:35:47.976563027\n"
      "c2,a,2020-01-10 06:42:02.778204862,R,2020-01-10 06:38:03.822195385\n"
      "c3,a,2020-01-10 03:59:59.999999999,S,\n"
      "c3,b,2020-01-10 08:59:59.999999998,S,\n");
  const auto run = [&](M metric, const std::string& resource, const std::string& activity = "",
                       const std::string& start = "") {
    ResourceProfilesOptions o;
    o.metric = metric;
    o.resource_attribute = "who";
    o.t1 = "2020-01-10 04:00:00";
    o.t2 = "2020-01-10 09:00:00";
    o.resource = resource;
    o.resource_given = true;
    if (!activity.empty()) { o.activity = activity; o.activity_given = true; }
    o.start_timestamp_attribute = start;
    const auto result = discover_resource_profiles(log, o);
    assert(result.rows.size() == 1);
    return result.rows.front().value;
  };
  assert(run(M::kAverageDurationActivity, "R", "a") == 249.6849249136667);
  assert(run(M::kAverageDurationActivity, "R", "b") == 408.15126179111115);
  assert(run(M::kAverageDurationActivity, "R", "a", "st") == 166.84773411483334);
  assert(run(M::kAverageDurationActivity, "R", "b", "st") == 128.481445822875);
  assert(run(M::kAverageCaseDuration, "R") == 2223.193485028);
  assert(run(M::kAverageCaseDuration, "S") == 17999.999999999);
  assert(run(M::kActivityCompletions, "S") == 1.0);
  assert(run(M::kDistinctActivities, "S") == 1.0);
  assert(run(M::kMultitasking, "R") == 1.0);
  assert(run(M::kAverageWorkload, "R") == 0.0);
}

int main() {
  review_fp_log_lifecycle();
  review_fp_frame_exact();
  review_fo_log_variant();
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
  // Round FO: pm4py's log variant closes the social window (t1 <= ev <= t2),
  // so bob's C@20 puts case b in it too (1.0 for both, as pm4py).
  assert(social.rows[0].value == 1.0);
  assert(social.rows[1].value == 1.0);

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
  // pm4py's log variant (PM4Py 2.7.22 EventLog variant, round FP): the
  // conversion pairs A start@0 with A complete@10, and without a start key
  // the first interval of a case starts at its own end (0s in case a and in
  // case b). The pandas variant of a frame gives 5s.
  assert(durations.rows.size() == 1 && durations.rows[0].value == 0.0);
  {
    // Overlapping lifecycle intervals: B complete@0, D start@10 (r2),
    // C complete@50, D complete@58 (x). The converted intervals sort by
    // start (B, D from 10, C), so D follows B: 58 with or without C (the
    // pandas variant of a frame: 8 and 48).
    const auto overlap = one_case({timed("B", "r2", 0, "complete"), timed("D", "r2", 10, "start"),
                                   timed("C", "r1", 50, "complete"), timed("D", "x", 58, "complete")});
    assert(only_value(query(overlap, ResourceProfileMetric::kAverageDurationActivity, 0, 59, "x", "D")) == 58.0);
    const auto csc = one_case({timed("B", "r2", 0, "complete"), timed("D", "r2", 10, "start"),
                               timed("D", "x", 58, "complete")});
    assert(only_value(query(csc, ResourceProfileMetric::kAverageDurationActivity, 0, 59, "x", "D")) == 58.0);
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
  // pm4py's log variant pairs the transitions (round FP): no interval
  // starts before 10s and ends at or after it, so 0.0 (PM4Py 2.7.22
  // EventLog variant; the pandas variant of a frame, where the A
  // complete@10 interval spans [5,10], gives 3.0). Both give 2.0 at 12s.
  assert(only_value(average_workload) == 0.0);
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
  // Round FO: t1 > t2 is an empty window, as in pm4py (no rows for all
  // resources).
  bad = {}; bad.t1 = "2024-01-02"; bad.t2 = "2024-01-01";
  assert(discover_resource_profiles(log, bad).rows.empty());

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
