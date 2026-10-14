#include "minera/resource_profiles.hpp"

#include <cassert>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

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

int main() {
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
  assert(durations.rows.size() == 1 && durations.rows[0].value == 5.0);

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
  assert(average_workload.rows.size() == 1 && average_workload.rows[0].value == 2.0);

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
