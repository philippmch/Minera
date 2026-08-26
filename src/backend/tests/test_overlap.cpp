#include "minera/overlap.hpp"

#include <cassert>
#include <stdexcept>

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
}  // namespace

int main() {
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
