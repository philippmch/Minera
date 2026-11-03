#include "minera/overlap.hpp"
#include "minera/eventually_follows.hpp"

#undef NDEBUG
#include <cassert>
#include <cstdio>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>

using namespace minera;

namespace {
Event event(const char* start, const char* complete = nullptr) {
  Event e;
  e.timestamp = start;
  if (complete != nullptr) e.attributes.push_back({"done", complete});
  return e;
}
Event timed(const char* start, const char* complete, const char* activity) {
  Event e = event(start, complete);
  e.activity = activity;
  return e;
}
// Reference capture (pm4py 2.7.22 statistics.eventually_follows.log with
// start_timestamp_key): traces [A 0-10, B 5-15, C 10-20, D 30-40],
// [X 20-25, Y 0-30, X 30-31], [A 0-5, B 0-0] (minutes) give
// {(A,C):1, (A,D):1, (B,D):1, (C,D):1, (X,X):1, (Y,X):1}. Each trace is
// stably sorted by start; a pair counts when the later event starts at or
// after the earlier one completes.
void eventually_follows_interval_reference() {
  const auto at = [](int minute) {
    char text[32];
    std::snprintf(text, sizeof text, "2024-01-01T%02d:%02d:00", minute / 60, minute % 60);
    return std::string(text);
  };
  const auto ev = [&](const char* activity, int start, int end) {
    Event e;
    e.activity = activity;
    e.timestamp = at(end);
    e.attributes.push_back({"start_timestamp", at(start)});
    return e;
  };
  EventLog log;
  log.traces.push_back(Trace{"c1", {ev("A", 0, 10), ev("B", 5, 15), ev("C", 10, 20), ev("D", 30, 40)}});
  log.traces.push_back(Trace{"c2", {ev("X", 20, 25), ev("Y", 0, 30), ev("X", 30, 31)}});
  log.traces.push_back(Trace{"c3", {ev("A", 0, 5), ev("B", 0, 0)}});
  const auto graph = discover_directly_follows_graph(log);
  EventuallyFollowsOptions options;
  options.start_timestamp_attribute = "start_timestamp";
  std::map<std::pair<std::string, std::string>, std::uint64_t> counts;
  for (const auto& edge : discover_eventually_follows(log, graph, options))
    counts[{graph.activities()[edge.source], graph.activities()[edge.target]}] = edge.count;
  const std::map<std::pair<std::string, std::string>, std::uint64_t> expected = {
      {{"A", "C"}, 1}, {{"A", "D"}, 1}, {{"B", "D"}, 1},
      {{"C", "D"}, 1}, {{"X", "X"}, 1}, {{"Y", "X"}, 1}};
  assert(counts == expected);

  // A missing start attribute raises, as the reference's KeyError does.
  log.traces[0].events[1].attributes.clear();
  bool thrown = false;
  try {
    (void)discover_eventually_follows(log, graph, options);
  } catch (const std::invalid_argument&) {
    thrown = true;
  }
  assert(thrown);
}

// Reference capture (pm4py 2.7.22 statistics.overlap.interval_events,
// overlap.cases and concurrent_activities, start_timestamp_key) on events that
// start after they complete (minutes). Interval-event overlap raises
// ("Start must be less than end"); case overlap takes the min-start/max-end
// envelope and raises only when that envelope is inverted; concurrent
// activities sort each trace by start and stop scanning later events at the
// first one that does not overlap.
void inverted_interval_reference() {
  const auto at = [](int minute) {
    char text[32];
    std::snprintf(text, sizeof text, "2024-01-01T%02d:%02d:00", minute / 60, minute % 60);
    return std::string(text);
  };
  const auto ev = [&](const char* activity, int start, int end) {
    Event e;
    e.activity = activity;
    e.timestamp = at(end);
    e.attributes.push_back({"start_timestamp", at(start)});
    return e;
  };
  OverlapOptions options;
  options.start_timestamp_attribute = "start_timestamp";
  const auto raises = [](auto&& call) {
    try {
      (void)call();
    } catch (const std::invalid_argument&) {
      return true;
    }
    return false;
  };

  // [A 60-0, B 30-120], [A 90-100]: interval events raise, cases give
  // [2, 2], and no activities are concurrent.
  EventLog inverted;
  inverted.traces.push_back(Trace{"c0", {ev("A", 60, 0), ev("B", 30, 120)}});
  inverted.traces.push_back(Trace{"c1", {ev("A", 90, 100)}});
  assert(raises([&] { return discover_interval_event_overlap_counts(inverted, options); }));
  assert((discover_case_overlap_counts(inverted, options) == CaseOverlapCounts{2, 2}));
  assert(discover_concurrent_activity_overlap(inverted, options).empty());
  options.strict = true;
  assert(discover_concurrent_activity_overlap(inverted, options).empty());
  options.strict = false;

  // [X 0-10, Y 5-3, Z 6-8]: the inverted Y ends the scan from X, so the
  // overlapping X and Z are not counted; the case envelope is proper.
  EventLog scan{{Trace{"c0", {ev("X", 0, 10), ev("Y", 5, 3), ev("Z", 6, 8)}}}};
  assert(discover_concurrent_activity_overlap(scan, options).empty());
  assert((discover_case_overlap_counts(scan, options) == CaseOverlapCounts{1}));

  // [X 0-10, Y 5-5, Z 6-8]: a zero-length event that only touches does not
  // end the strict scan.
  EventLog zero{{Trace{"c0", {ev("X", 0, 10), ev("Y", 5, 5), ev("Z", 6, 8)}}}};
  const auto zero_pairs = discover_concurrent_activity_overlap(zero, options);
  assert(zero_pairs.size() == 2 && zero_pairs.at({"X", "Y"}) == 1 && zero_pairs.at({"X", "Z"}) == 1);
  options.strict = true;
  const auto zero_strict = discover_concurrent_activity_overlap(zero, options);
  assert(zero_strict.size() == 1 && zero_strict.at({"X", "Z"}) == 1);
  options.strict = false;

  // [A 0-10], [B 20-5]: an inverted case envelope raises.
  EventLog envelope;
  envelope.traces.push_back(Trace{"c0", {ev("A", 0, 10)}});
  envelope.traces.push_back(Trace{"c1", {ev("B", 20, 5)}});
  assert(raises([&] { return discover_case_overlap_counts(envelope, options); }));
  assert(discover_concurrent_activity_overlap(envelope, options).empty());
}

}  // namespace

int main() {
  eventually_follows_interval_reference();
  inverted_interval_reference();
  Trace a{"a", {event("2024-01-01T00:00:00Z", "2024-01-01T00:00:10Z")}};
  Trace b{"b", {event("2024-01-01T00:00:10Z", "2024-01-01T00:00:20Z")}};
  EventLog log{{a, b}};
  OverlapOptions options;
  options.completion_timestamp_attribute = "done";
  const auto inclusive = discover_interval_event_overlap_counts(log, options);
  assert((inclusive == IntervalEventOverlapCounts{2, 2}));

  options.strict = true;
  const auto strict = interval_event_overlap_counts(log, options);
  assert((strict == IntervalEventOverlapCounts{1, 1}));

  // The tolerance expands both intervals before testing, so a small gap
  // below twice epsilon still counts as overlap.
  Trace near;
  near.case_id = "near";
  near.events = {event("2024-01-01T00:00:10.000015Z", "2024-01-01T00:00:20Z")};
  EventLog near_log{{a, near}};
  options.strict = false;
  options.epsilon = 0.00001;
  const auto near_counts = interval_event_overlap_counts(near_log, options);
  assert((near_counts == IntervalEventOverlapCounts{2, 2}));

  Trace near_concurrent{"near-concurrent", {
      timed("2024-01-01T00:00:00Z", "2024-01-01T00:00:10Z", "A"),
      timed("2024-01-01T00:00:10.000015Z", "2024-01-01T00:00:20Z", "B"),
  }};
  const auto near_concurrent_pairs = discover_concurrent_activity_overlap(
      EventLog{{near_concurrent}}, options);
  // Unlike interval/case overlap, concurrent-activity overlap does not use
  // epsilon to turn a near gap into an intersection.
  assert(near_concurrent_pairs.find({"A", "B"}) == near_concurrent_pairs.end());

  options.strict = false;
  options.epsilon = 0.0;
  const auto cases = case_overlap_counts(log, options);
  assert((cases == CaseOverlapCounts{2, 2}));

  EventLog malformed{{Trace{"x", {event("bad", "2024-01-01")}}}};
  bool threw = false;
  try {
    (void)discover_interval_event_overlap_counts(malformed, options);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  Trace concurrent{"c", {
      timed("2024-01-01T00:00:00Z", "2024-01-01T00:00:10Z", "B"),
      timed("2024-01-01T00:00:00Z", "2024-01-01T00:00:10Z", "A"),
      timed("2024-01-01T00:00:10Z", "2024-01-01T00:00:20Z", "A"),
  }};
  EventLog isolated{{concurrent, a}};
  const auto pairs = discover_concurrent_activity_overlap(isolated, options);
  assert(pairs.at({"A", "B"}) == 2);
  assert(pairs.at({"A", "A"}) == 1);
  options.strict = true;
  const auto strict_pairs = concurrent_activity_overlap_counts(isolated, options);
  assert(strict_pairs.at({"A", "B"}) == 1);
  assert(strict_pairs.find({"A", "A"}) == strict_pairs.end());

  concurrent.events[0].attributes.push_back({"phase", "left"});
  concurrent.events[1].attributes.push_back({"phase", "right"});
  concurrent.events[2].attributes.push_back({"phase", "right"});
  options.activity_attribute = "phase";
  options.strict = false;
  const auto custom_pairs = discover_concurrent_activity_overlap(
      EventLog{{concurrent}}, options);
  assert(custom_pairs.at({"left", "right"}) == 2);
}
