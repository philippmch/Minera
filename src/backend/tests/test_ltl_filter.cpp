#include "minera/ltl_filter.hpp"

#include <cassert>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

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

  // Captured from pm4py 2.7.22 ltl_checker on the first two traces of a
  // random XES fixture (timestamps, activities and resources verbatim).
  {
    EventLog captured;
    captured.traces = {
        {"c0", {event("c0", "b", "2024-01-01T09:00:15+00:00", "r3"),
                event("c0", "\u00e9", "2024-01-01T09:00:29+00:00", "r2"),
                event("c0", "\u00e9", "2024-01-02T09:00:57+00:00", "r3"),
                event("c0", "a", "2024-01-02T09:06:27+00:00", "r2"),
                event("c0", "\u00e9", "2024-01-02T10:06:27+00:00", "r3"),
                event("c0", "\u00e9", "2024-01-02T10:07:55+00:00", "r1")}, {}},
        {"c1", {event("c1", "c", "2024-01-03T10:08:01+00:00", "r3"),
                event("c1", "b", "2024-01-03T10:13:01+00:00", "r3"),
                event("c1", "a", "2024-01-03T10:15:14+00:00", "r3"),
                event("c1", "a", "2024-01-03T10:17:38+00:00", "r1"),
                event("c1", "a", "2024-01-03T10:17:56+00:00", "r1")}, {}}};
    const auto ids = [](const EventLog& filtered) {
      std::string joined;
      for (const auto& trace : filtered.traces) joined += trace.case_id + ";";
      return joined;
    };
    // Imported logs follow pm4py's DataFrame ltl_checker (captured on the
    // DataFrame of these rows): bounds hold between the matched b and a
    // events, so c0's b -> a (about 24 h) and c1's (133 s) both pass.
    LtlFilterOptions bounded;
    bounded.attribute_values = {"b", "a"};
    bounded.enable_timestamp = true;
    bounded.timestamp_diff_boundaries = {{60.0, 100000.0}};
    assert(ids(minera::filter_ltl_event_log(captured, bounded)) == "c0;c1;");
    bounded.positive = false;
    assert(ids(minera::filter_ltl_event_log(captured, bounded)).empty());
    // The EventLog checker (a log arriving as an EventLog object) instead
    // reads the bound at trace positions i and i + 1: c0's first gap is 14 s,
    // c1's is 300 s.
    constexpr auto object = minera::LtlInputShape::kEventLogObject;
    bounded.positive = true;
    assert(ids(minera::filter_ltl_event_log(captured, bounded, object)) == "c1;");
    bounded.positive = false;
    assert(ids(minera::filter_ltl_event_log(captured, bounded, object)) == "c0;");
    // Standard XES keys select the importer's first-class fields.
    LtlFilterOptions named{LtlFilterVariant::kEventuallyFollows, "concept:name", {"b", "a"}};
    assert(ids(minera::filter_ltl_event_log(captured, named)) == "c0;c1;");
    LtlFilterOptions by_resource{LtlFilterVariant::kEventuallyFollows, "org:resource", {"r3", "r2"}};
    assert(ids(minera::filter_ltl_event_log(captured, by_resource)) == "c0;");
    LtlFilterOptions persons{LtlFilterVariant::kAttributeValueDifferentPersons, "", {"\u00e9"}};
    persons.resource_key = "org:resource";
    assert(ids(minera::filter_ltl_event_log(captured, persons)) == "c0;");
    // Four eyes with the roles swapped: the A (r3) events' "resources" are
    // b, \u00e9, \u00e9; the DataFrame compares them with the last B (r2)
    // event's only, a, so c0 satisfies the principle. The EventLog checker
    // compares with every B (\u00e9 and a) and finds a violation.
    LtlFilterOptions swapped{LtlFilterVariant::kFourEyes, "org:resource", {"r3", "r2"}};
    swapped.resource_key = "concept:name";
    assert(ids(minera::filter_ltl_event_log(captured, swapped)) == "c0;");
    assert(ids(minera::filter_ltl_event_log(captured, swapped, object)).empty());
    swapped.positive = false;
    assert(ids(minera::filter_ltl_event_log(captured, swapped)).empty());
    assert(ids(minera::filter_ltl_event_log(captured, swapped, object)) == "c0;");

    // Captured: a case B(r1), A(r1), B(r2) passes the DataFrame four-eyes
    // check (only the last B's r2 is compared) and fails the EventLog one.
    EventLog repeated;
    repeated.traces = {
        {"1", {event("1", "B", "2023-01-01T00:00:00Z", "r2"),
               event("1", "A", "2023-01-01T01:00:00Z", "r1")}, {}},
        {"2", {event("2", "B", "2023-01-01T00:00:00Z", "r1"),
               event("2", "A", "2023-01-01T01:00:00Z", "r1"),
               event("2", "B", "2023-01-01T02:00:00Z", "r2")}, {}},
        {"3", {event("3", "A", "2023-01-01T00:00:00Z", "r1"),
               event("3", "B", "2023-01-01T00:00:00Z", "r2")}, {}}};
    LtlFilterOptions eyes{LtlFilterVariant::kFourEyes, "", {"A", "B"}};
    assert(ids(minera::filter_ltl_event_log(repeated, eyes)) == "1;2;3;");
    assert(ids(minera::filter_ltl_event_log(repeated, eyes, object)) == "1;3;");
    eyes.positive = false;
    assert(ids(minera::filter_ltl_event_log(repeated, eyes)).empty());
    assert(ids(minera::filter_ltl_event_log(repeated, eyes, object)) == "2;");

    // Captured: A,B,C,A,B,X fails the DataFrame A-next-B-next-C check (its
    // last B is not followed by its last C) and passes the EventLog one.
    EventLog later;
    later.traces = {{"n1", {event("n1", "A", ""), event("n1", "B", ""),
                            event("n1", "C", ""), event("n1", "A", ""),
                            event("n1", "B", ""), event("n1", "X", "")}, {}}};
    LtlFilterOptions abc{LtlFilterVariant::kANextBNextC, "", {"A", "B", "C"}};
    assert(ids(minera::filter_ltl_event_log(later, abc)).empty());
    assert(ids(minera::filter_ltl_event_log(later, abc, object)) == "n1;");
  }

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

  // A float NaN resource (XES <float value="nan"/>) is missing, as in the
  // DataFrame read_xes returns: it is neither a performer of A nor of B.
  {
    EventAttribute nan{"org:resource", "nan", AttributeKind::kFloat};
    nan.typed_value = std::numeric_limits<double>::quiet_NaN();
    auto unknown = event("n", "A", "", "nan");
    unknown.resource_state = minera::FirstClassAttributeState::kPresent;
    unknown.resource_attribute = nan;
    EventLog nan_log;
    nan_log.traces = {{"n", {unknown, event("n", "B", "", "r1")}, {}}};
    LtlFilterOptions eyes{LtlFilterVariant::kFourEyes, "", {"A", "B"}};
    assert(minera::filter_ltl_event_log(nan_log, eyes).traces.empty());
    LtlFilterOptions persons{LtlFilterVariant::kAttributeValueDifferentPersons, "", {"A"}};
    nan_log.traces[0].events.push_back(event("n", "A", "", "r2"));
    assert(minera::filter_ltl_event_log(nan_log, persons).traces.empty());
    persons.resource_key = "org:resource";
    assert(minera::filter_ltl_event_log(nan_log, persons).traces.empty());
  }
  return 0;
}
