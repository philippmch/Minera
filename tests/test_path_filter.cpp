#include "minera/path_filter.hpp"

#include <cassert>
#include <stdexcept>

using minera::AttributeKind;
using minera::Event;
using minera::EventLog;
using minera::Trace;

namespace {

Event make_event(const char* id, const char* activity, const char* timestamp,
                 std::size_t order) {
  return Event{id, activity, timestamp, "worker", order,
               {{"tag", "retained", AttributeKind::kString}}, "complete"};
}

EventLog sample() {
  EventLog log;
  log.attributes.push_back({"source", "fixture", AttributeKind::kString});
  Trace good;
  good.case_id = "good";
  good.attributes.push_back({"owner", "alice", AttributeKind::kString});
  good.events = {make_event("good", "A", "2024-01-01T00:00:00Z", 0),
                 make_event("good", "B", "2024-01-01T00:00:05Z", 1),
                 make_event("good", "C", "2024-01-01T00:00:06Z", 2)};
  Trace too_slow;
  too_slow.case_id = "too-slow";
  too_slow.events = {make_event("too-slow", "A", "2024-01-01T00:00:00Z", 0),
                     make_event("too-slow", "B", "2024-01-01T00:00:20Z", 1)};
  Trace nonmatching;
  nonmatching.case_id = "nonmatching";
  nonmatching.events = {make_event("nonmatching", "X", "not-a-time", 0),
                        make_event("nonmatching", "Y", "also-not-a-time", 1)};
  log.traces = {good, too_slow, nonmatching};
  return log;
}

}  // namespace

int main() {
  const auto log = sample();
  minera::AdjacentPathFilterOptions options;
  options.predecessor = "A";
  options.successor = "B";
  options.min_elapsed_seconds = 5.0;
  options.max_elapsed_seconds = 5.0;
  auto result = minera::filter_adjacent_path(log, options);
  assert(result.traces.size() == 1 && result.traces[0].case_id == "good");
  assert(result.attributes.size() == log.attributes.size());
  assert(result.attributes[0].key == "source" &&
         result.attributes[0].value == "fixture");
  assert(result.traces[0].attributes.size() == 1 &&
         result.traces[0].attributes[0].value == "alice");
  assert(result.traces[0].events.size() == log.traces[0].events.size());
  for (std::size_t i = 0; i < result.traces[0].events.size(); ++i) {
    assert(result.traces[0].events[i].activity ==
           log.traces[0].events[i].activity);
    assert(result.traces[0].events[i].timestamp ==
           log.traces[0].events[i].timestamp);
    assert(result.traces[0].events[i].attributes[0].value == "retained");
  }

  options.min_elapsed_seconds = 10.0;
  options.max_elapsed_seconds = 30.0;
  options.positive = false;
  result = minera::filter_adjacent_path_event_log(log, options);
  assert(result.traces.size() == 2);
  assert(result.traces[0].case_id == "good");
  assert(result.traces[1].case_id == "nonmatching");

  // With no performance bounds this is the ordinary adjacent-path filter.
  options.min_elapsed_seconds.reset();
  options.max_elapsed_seconds.reset();
  options.positive = true;
  result = minera::path_filter(log, options);
  assert(result.traces.size() == 2);

  bool threw = false;
  try {
    options.min_elapsed_seconds = 0.0;
    (void)minera::filter_adjacent_path(log, options);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(!threw);  // the malformed timestamps belong to a nonmatching path

  EventLog malformed = log;
  malformed.traces[0].events[1].timestamp = "not-a-time";
  threw = false;
  try {
    (void)minera::filter_adjacent_path(malformed, options);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  threw = false;
  try {
  options.min_elapsed_seconds = 2.0;
  options.max_elapsed_seconds = 1.0;
    (void)minera::filter_adjacent_path(log, options);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  // A max-only performance range retains the default zero lower bound.
  EventLog backwards;
  Trace backwards_trace;
  backwards_trace.case_id = "backwards";
  backwards_trace.events = {
      make_event("backwards", "A", "2024-01-01T00:00:05Z", 0),
      make_event("backwards", "B", "2024-01-01T00:00:00Z", 1),
  };
  backwards.traces.push_back(backwards_trace);
  options.min_elapsed_seconds.reset();
  options.max_elapsed_seconds = 10.0;
  options.positive = true;
  assert(minera::filter_adjacent_path(backwards, options).traces.empty());

  threw = false;
  options.positive = false;
  options.max_elapsed_seconds = -1.0;
  try {
    (void)minera::filter_adjacent_path(backwards, options);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
  return 0;
}
