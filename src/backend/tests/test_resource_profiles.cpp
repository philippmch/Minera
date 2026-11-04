#include "minera/resource_profiles.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

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

int main() {
  reference_audit_contracts();
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
