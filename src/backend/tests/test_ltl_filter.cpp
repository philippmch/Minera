#include "minera/ltl_filter.hpp"

#include <cassert>
#include <cmath>
#include <stdexcept>

using minera::AttributeKind;
using minera::Event;
using minera::EventAttribute;
using minera::EventLog;
using minera::LtlFilterOptions;
using minera::LtlFilterVariant;
using minera::Trace;

namespace {
Event event(const char* id, const char* activity, const char* timestamp,
            const char* resource = "") {
  return {id, activity, timestamp, resource, 0, {}, {}};
}

Event generic(const char* id, const char* activity, const char* value,
              const char* resource) {
  Event result = event(id, activity, "", resource);
  result.attributes.push_back({"kind", value, AttributeKind::kString});
  return result;
}

EventLog sample() {
  EventLog log;
  log.attributes.push_back({"origin", "fixture", AttributeKind::kString});
  log.traces = {
      {"good", {event("good", "A", "2024-01-01T00:00:00Z", "u1"),
                 event("good", "x", "2024-01-01T00:00:02Z", "u1"),
                 event("good", "B", "2024-01-01T00:00:05Z", "u2")},
       {{"case_attr", "one", AttributeKind::kString}}},
      {"bad", {event("bad", "A", "2024-01-01T00:00:00Z", "u1"),
                event("bad", "B", "2024-01-01T00:00:20Z", "u1")},
       {}}};
  return log;
}
}  // namespace

int main() {
  const auto log = sample();

  LtlFilterOptions eventually;
  eventually.attribute_values = {"A", "B"};
  eventually.enable_timestamp = true;
  eventually.timestamp_diff_boundaries = {{0.0, 10.0}};
  auto result = minera::filter_ltl_event_log(log, eventually);
  assert(result.traces.size() == 1 && result.traces[0].case_id == "good");
  assert(result.attributes[0].value == "fixture");
  assert(result.traces[0].attributes[0].value == "one");

  eventually.positive = false;
  result = minera::filter_ltl(log, eventually);
  assert(result.traces.size() == 1 && result.traces[0].case_id == "bad");

  LtlFilterOptions next{LtlFilterVariant::kANextBNextC, "", {"A", "x", "B"}};
  assert(minera::filter_event_log_ltl(log, next).traces.size() == 1);

  EventLog people;
  people.traces = {{"p", {generic("p", "ignored", "v", "alice"),
                           generic("p", "ignored", "v", "bob")}, {}}};
  LtlFilterOptions different{LtlFilterVariant::kAttributeValueDifferentPersons,
                             "kind", {"v"}};
  assert(minera::filter_ltl_event_log(people, different).traces.size() == 1);

  LtlFilterOptions eyes{LtlFilterVariant::kFourEyes, "", {"A", "B"}};
  assert(minera::filter_ltl_event_log(log, eyes).traces.size() == 1);
  eyes.positive = false;
  result = minera::filter_ltl_event_log(log, eyes);
  assert(result.traces.size() == 1 && result.traces[0].case_id == "bad");

  EventLog missing_side;
  missing_side.traces = {
      {"only_a", {event("only_a", "A", "", "u1")}, {}},
      {"both_shared", {event("both_shared", "A", "", "u1"),
                         event("both_shared", "B", "", "u1")}, {}}};
  eyes.positive = true;
  assert(minera::filter_ltl_event_log(missing_side, eyes).traces.empty());
  eyes.positive = false;
  result = minera::filter_ltl_event_log(missing_side, eyes);
  assert(result.traces.size() == 1 && result.traces[0].case_id == "both_shared");

  bool rejected = false;
  try {
    LtlFilterOptions malformed;
    malformed.attribute_values = {"A", ""};
    (void)minera::filter_ltl_event_log(log, malformed);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
  rejected = false;
  try {
    LtlFilterOptions malformed;
    malformed.attribute_values = {"A", "B"};
    malformed.enable_timestamp = true;
    malformed.timestamp_diff_boundaries = {{0.0, 1.0}, {0.0, 1.0}};
    (void)minera::filter_ltl_event_log(log, malformed);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
  rejected = false;
  try {
    LtlFilterOptions malformed;
    malformed.attribute_values = {"A", "B"};
    malformed.enable_timestamp = true;
    malformed.timestamp_diff_boundaries = {{0.0, INFINITY}};
    (void)minera::filter_ltl_event_log(log, malformed);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
  rejected = false;
  try {
    LtlFilterOptions malformed;
    malformed.variant = static_cast<LtlFilterVariant>(99);
    malformed.attribute_values = {"A"};
    (void)minera::filter_ltl_event_log(log, malformed);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
  return 0;
}
