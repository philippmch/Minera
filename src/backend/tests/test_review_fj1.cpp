// Review FJ1 (round FJ, re-review of FI): checks for the fixes of this round.
//
// - FJ1-1 The exact attribute filters on a datetime column with a value
//   list whose datetimes have the other awareness (naive datetimes on an
//   aware column, aware ones of one offset on a naive column) next to NaT,
//   NaN or None: pandas converts the list to a datetime array first and
//   finds it incompatible with the column, so nothing matches, NaT
//   included. Native matched the NaT rows.
// - FJ1-2 The non-exact attribute filters: None matches by the column's
//   dtype (any missing cell of a str column, an explicit None of an object
//   column, nothing in a float64 or datetime64 column), and a cell the
//   frame holds as a datetime equals no text. Native matched explicit nulls
//   alone, and a time's text.
// - FJ1-3 The exact attribute filters with a datetime finer than the
//   column's unit (microseconds as the readers keep them): the converted
//   value array is floored to the column's unit, so the value matches the
//   cell it falls in. Native compared the nanoseconds.
//
// See docs/reference-2.7.22-parity.md, Review FJ1.
#include "minera/ocel.hpp"
#include "minera/ocel_csv.hpp"
#include "minera/ocel_filter.hpp"
#include "minera/timestamp.hpp"

#include <algorithm>
#include <iostream>
#include <limits>
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

std::vector<std::string> event_ids(const Ocel& log) {
  std::vector<std::string> ids;
  for (const auto& event : log.events) ids.push_back(event.id);
  std::sort(ids.begin(), ids.end());
  return ids;
}

std::vector<std::string> object_ids(const Ocel& log) {
  std::vector<std::string> ids;
  for (const auto& object : log.objects) ids.push_back(object.id);
  std::sort(ids.begin(), ids.end());
  return ids;
}

using Ids = std::vector<std::string>;
using Values = std::vector<OcelEventAttributeFilterValue>;

// fj t4/k1.json: the time attribute s is aware (e1 null, e2 without it), f a
// float with a null, x a string with a null; the objects' time since is
// aware (o2 null).
Ocel k1() {
  return parse_ocel_json(
      R"({"objectTypes":[{"name":"t","attributes":[{"name":"since","type":"time"}]}],)"
      R"("eventTypes":[{"name":"a","attributes":[{"name":"s","type":"time"},{"name":"f","type":"float"},{"name":"x","type":"string"}]}],)"
      R"("objects":[{"id":"o1","type":"t","attributes":[{"name":"since","time":"1970-01-01T00:00:00Z","value":"2024-01-01T10:00:00Z"}]},)"
      R"({"id":"o2","type":"t","attributes":[{"name":"since","time":"1970-01-01T00:00:00Z","value":null}]}],)"
      R"("events":[{"id":"e0","type":"a","time":"2024-01-01T00:00:00Z","attributes":[{"name":"s","value":"2024-01-01T10:00:00Z"},)"
      R"({"name":"f","value":1.5},{"name":"x","value":"p"}],"relationships":[{"objectId":"o1","qualifier":"q"}]},)"
      R"({"id":"e1","type":"a","time":"2024-01-01T01:00:00Z","attributes":[{"name":"s","value":null},{"name":"f","value":null},)"
      R"({"name":"x","value":null}],"relationships":[{"objectId":"o2","qualifier":"q"}]},)"
      R"({"id":"e2","type":"a","time":"2024-01-01T02:00:00Z","attributes":[{"name":"f","value":2.5}],)"
      R"("relationships":[{"objectId":"o1","qualifier":"q"}]}]})");
}

void fj1_1_exact_filters_other_awareness() {
  const auto log = k1();
  const auto naive = *parse_timestamp_exact("2024-01-01T10:00:00");
  const auto other_offset = *parse_timestamp_exact("2024-01-01T12:00:00+02:00");
  const double nan = std::numeric_limits<double>::quiet_NaN();
  const auto events = [&](const Values& values, bool positive) {
    return event_ids(filter_ocel_event_attribute_exact(log, "s", values, positive));
  };
  const auto objects = [&](const Values& values, bool positive) {
    return object_ids(filter_ocel_object_attribute_exact(log, "since", values, positive));
  };
  expect(events(Values{naive, OcelAttributeNaT{}}, true).empty(), "FJ1-1 a naive datetime next to NaT on an aware column matches nothing");
  expect(events(Values{naive, OcelAttributeNaT{}}, false) == Ids{"e0", "e1", "e2"}, "FJ1-1 negated, every event stays");
  expect(events(Values{OcelAttributeNaT{}, nan, nullptr, naive}, true).empty(), "FJ1-1 NaT, NaN and None next to a naive datetime");
  expect(objects(Values{naive, OcelAttributeNaT{}}, true).empty(), "FJ1-1 the objects' filter matches nothing either");
  expect(objects(Values{naive, OcelAttributeNaT{}}, false) == Ids{"o1", "o2"}, "FJ1-1 negated, every object stays");
  // Controls: an aware datetime of another offset converts with the column's
  // awareness (FI1-4), NaT alone matches the NaT cells.
  expect(events(Values{other_offset, nullptr}, true) == Ids{"e0", "e1", "e2"}, "FJ1-1 another offset next to None matches NaT");
  expect(events(Values{other_offset, OcelAttributeNaT{}}, true) == Ids{"e0", "e1", "e2"}, "FJ1-1 another offset next to NaT");
  expect(objects(Values{other_offset, nullptr}, true) == Ids{"o1", "o2"}, "FJ1-1 the objects' filter with another offset");
  expect(events(Values{OcelAttributeNaT{}}, true) == Ids{"e1", "e2"}, "FJ1-1 NaT alone matches the NaT cells");
  // An OCEL CSV log's aware time column with a naive datetime next to NaT.
  const auto csv = parse_ocel_csv(
      "ocel:eid,ocel:timestamp,ocel:activity,ocel:type:t\ne0,2024-02-02 10:00:00+00:00,a,['t0']\ne1,,b,['t0']\n",
      "ocel:oid,ocel:type\nt0,t\n");
  const auto stamp = *parse_timestamp_exact("2024-02-02T10:00:00");
  expect(event_ids(filter_ocel_event_attribute_exact(csv, "ocel:timestamp", Values{stamp, OcelAttributeNaT{}}, true)).empty(),
         "FJ1-1 ocel:timestamp with a naive datetime next to NaT matches nothing");
}

void fj1_2_plain_filters() {
  const auto log = k1();
  const auto events = [&](const std::string& key, const std::vector<OcelValue>& values) {
    return event_ids(filter_ocel_event_attribute(log, key, values, true));
  };
  expect(events("s", {nullptr}).empty(), "FJ1-2 None on a datetime64 column matches nothing");
  expect(events("s", {std::string("2024-01-01T10:00:00Z")}).empty(), "FJ1-2 a time's text equals no datetime cell");
  expect(event_ids(filter_ocel_event_attribute(log, "s", {nullptr}, false)) == Ids{"e0", "e1", "e2"},
         "FJ1-2 negated, every event stays");
  expect(events("f", {nullptr}).empty(), "FJ1-2 None on a float64 column matches nothing");
  expect(events("f", {nullptr, 1.5}) == Ids{"e0"}, "FJ1-2 None next to a number on a float64 column");
  expect(events("x", {nullptr}) == Ids{"e1", "e2"}, "FJ1-2 None on a str column matches every missing cell");
  expect(events("x", {nullptr, std::string("p")}) == Ids{"e0", "e1", "e2"}, "FJ1-2 None next to a text on a str column");
  expect(object_ids(filter_ocel_object_attribute(log, "since", {nullptr}, true)).empty(),
         "FJ1-2 None on the objects' datetime64 column matches nothing");
}

void fj1_3_exact_filters_column_unit() {
  const auto log = k1();
  const auto at = [](const char* text) { return *parse_timestamp_exact(text); };
  const auto events = [&](const std::string& key, const Values& values) {
    return event_ids(filter_ocel_event_attribute_exact(log, key, values, true));
  };
  expect(events("s", Values{at("2024-01-01T10:00:00.0000005Z")}) == Ids{"e0"}, "FJ1-3 a finer datetime matches its microsecond");
  expect(events("s", Values{at("2024-01-01T10:00:00.000000999+00:00"), nullptr}) == Ids{"e0", "e1", "e2"},
         "FJ1-3 next to None too");
  expect(events("s", Values{at("2024-01-01T09:59:59.9999999Z")}).empty(), "FJ1-3 floored, not rounded");
  expect(events("ocel:timestamp", Values{at("2024-01-01T00:00:00.0000003Z")}) == Ids{"e0"}, "FJ1-3 the time column too");
  expect(object_ids(filter_ocel_object_attribute_exact(log, "since", Values{at("2024-01-01T10:00:00.0000009Z")}, true)) ==
             Ids{"o1"},
         "FJ1-3 the objects' filter too");
  // A list the reference compares value by value keeps the nanoseconds.
  expect(events("s", Values{at("2024-01-01T10:00:00.0000005Z"), at("2024-01-01T10:00:00")}).empty(),
         "FJ1-3 a list of both kinds compares the values as given");
}

}  // namespace

int main() {
  fj1_1_exact_filters_other_awareness();
  fj1_2_plain_filters();
  fj1_3_exact_filters_column_unit();
  if (failures) {
    std::cerr << failures << " check(s) failed\n";
    return 1;
  }
  std::cout << "review FJ1 checks passed\n";
  return 0;
}
