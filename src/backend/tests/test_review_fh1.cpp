// Review FH1 (round FH, re-review of FG): checks for the fixes of this round.
//
// - FH1-1 The edge metrics' event id key on a missing event time: with
//   ocel:timestamp as the event id, an OCEL CSV row's NaT is no group in
//   pm4py (its groupby drops the key), so the event takes part in no edge.
//   Native keyed it by the empty text and added its edges.
// - FH1-2 The exact window's timestamp key on a column of nulls whose
//   dropped rows hold nulls too: pandas typed it object (None) over every
//   row, which compares nothing, so the window keeps no event. Native read
//   any dropped row as one lacking the key (float64, TypeError 'float').
//   The window now reads Columns::all_nulls, as FG1-2's consumers do.
// - FH1-3 An aware datetime in the objects' string attribute family: pm4py
//   raises for the first value of the column's set that is no str, and an
//   aware datetime hashes free of the seed (its UTC days, seconds and
//   microseconds as a tuple), so it takes its slot before a float's.
//   Native gave datetimes no slot and named the float.
// - FH1-4 The objects' related events run pm4py's whole events algorithm,
//   whose attribute families (str, then num) come before the endpoints:
//   num_ev_attr's ValueError before an unrelated object's KeyError. Native
//   checked the related events without their attribute families.
//
// See docs/reference-2.7.22-parity.md, Review FH1.
#include "minera/ocdfg_edge_metrics.hpp"
#include "minera/ocel.hpp"
#include "minera/ocel_csv.hpp"
#include "minera/ocel_features.hpp"
#include "minera/ocel_filter.hpp"
#include "minera/ocel_xml.hpp"

#include <functional>
#include <iostream>
#include <stdexcept>
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

std::string error_of(const std::function<void()>& run) {
  try {
    run();
  } catch (const std::exception& error) {
    return error.what();
  }
  return {};
}

void fh1_1_nat_event_id() {
  // fh t4/m1.csv: e1's time is empty (NaT).
  const auto log = parse_ocel_csv(
      "ocel:eid,ocel:timestamp,ocel:activity,ocel:type:t\n"
      "e0,2024-01-01 10:00:00,a,['t0']\ne1,,b,['t0']\ne2,2024-01-01 12:00:00,c,['t0']\n",
      "ocel:oid,ocel:type\nt0,t\n");
  OcdfgAssociationOptions by_time;
  by_time.event_id_attribute = "ocel:timestamp";
  const auto timed = find_ocdfg_edge_associations(log, by_time);
  expect(timed.size() == 1 && timed.count("t") && timed.at("t").size() == 1 &&
             timed.at("t").count({"a", "c"}) && timed.at("t").at({"a", "c"}).size() == 1,
         "FH1-1 the NaT event takes part in no edge when the time is the event id");
  const auto plain = find_ocdfg_edge_associations(log, OcdfgAssociationOptions{});
  expect(plain.count("t") && plain.at("t").count({"c", "b"}), "FH1-1 by its id the NaT event keeps its edge");
}

// e0..e2 hold kk = null, e2 has no relations (dropped by the reader); `last`
// replaces e2's attributes.
Ocel dropped_log(const std::string& last) {
  std::string events;
  for (int i = 0; i < 3; ++i) {
    if (i) events += ",";
    events += R"({"id":"e)" + std::to_string(i) + R"(","type":"a","time":"2024-01-01T1)" + std::to_string(i) +
              R"(:00:00Z","attributes":[)" + (i == 2 ? last : R"({"name":"kk","value":null})") + R"(],"relationships":[)" +
              (i < 2 ? R"({"objectId":"o0","qualifier":"q"})" : "") + "]}";
  }
  return parse_ocel_json(R"({"objectTypes":[{"name":"t","attributes":[]}],"eventTypes":[{"name":"a","attributes":[)"
                         R"({"name":"kk","type":"string"}]}],"objects":[{"id":"o0","type":"t"}],"events":[)" + events + "]}");
}

std::string window_error(const Ocel& log, std::size_t* kept = nullptr) {
  return error_of([&] {
    const auto result = filter_ocel_events_timestamp_exact(log, std::string("2024-01-01 00:00:00"),
                                                           std::string("2024-12-31 00:00:00"), "kk", true);
    if (kept) *kept = result.events.size();
  });
}

void fh1_2_window_on_nulls() {
  const auto nulls = dropped_log(R"({"name":"kk","value":null})");
  expect(nulls.events.size() == 2, "FH1-2 the reader drops the event without relations");
  std::size_t kept = 99;
  expect(window_error(nulls, &kept).empty() && kept == 0,
         "FH1-2 a dropped row's null keeps the column object: the window compares nothing");
  kept = 99;
  expect(window_error(parse_ocel_json(serialize_ocel_json(nulls)), &kept).empty() && kept == 0,
         "FH1-2 through the transport too");
  expect(window_error(dropped_log("")) == "TypeError: '>=' not supported between instances of 'float' and 'datetime.datetime'",
         "FH1-2 a dropped row without the attribute makes the column float64");
}

// fh t5/n1.xmlocel: n = "12", a UTC date, -2.0 (CPython's set: the date in
// slot 2, the float in slot 6, in every seed).
void fh1_3_aware_datetime_slot() {
  const auto log = parse_ocel_xml(
      R"(<?xml version='1.0' encoding='UTF-8'?><log>)"
      R"(<global scope="event"><string key="activity" value="__INVALID__"/></global>)"
      R"(<global scope="object"><string key="type" value="__INVALID__"/></global>)"
      R"(<global scope="log"><list key="attribute-names"/><list key="object-types"/><string key="version" value="1.0"/>)"
      R"(<string key="ordering" value="timestamp"/></global><events><event><string key="id" value="e0"/>)"
      R"(<string key="activity" value="a"/><date key="timestamp" value="2024-01-01T10:00:00Z"/><list key="omap">)"
      R"(<string key="object-id" value="o0"/><string key="object-id" value="o1"/><string key="object-id" value="o2"/>)"
      R"(</list><list key="vmap"></list></event></events><objects>)"
      R"(<object><string key="id" value="o0"/><string key="type" value="t"/><list key="ovmap"><string key="n" value="12"/></list></object>)"
      R"(<object><string key="id" value="o1"/><string key="type" value="t"/><list key="ovmap"><date key="n" value="2024-02-01T10:00:00Z"/></list></object>)"
      R"(<object><string key="id" value="o2"/><string key="type" value="t"/><list key="ovmap"><float key="n" value="-2.0"/></list></object>)"
      R"(</objects></log>)");
  OcelFeatureOptions options;
  options.feature_name_style = OcelFeatureNameStyle::kCompatibility;
  options.include_all_object_features = true;
  options.string_object_attributes = {"n"};
  expect(error_of([&] { (void)extract_ocel_features(log, OcelFeatureVariant::kObjects, options); }) ==
             "TypeError: can only concatenate str (not \"datetime.datetime\") to str",
         "FH1-3 the aware datetime comes before the float in the column's set");
}

// fh t6/m4.csv: x0 = "1", "s"; z1 is related to no event.
void fh1_4_related_event_attributes() {
  const auto log = parse_ocel_csv(
      "ocel:eid,ocel:timestamp,ocel:activity,x0,ocel:type:t\n"
      "e0,2024-01-01 10:00:00,a,1,['t0']\ne1,2024-01-01 11:00:00,b,s,['t0']\n",
      "ocel:oid,ocel:type\nt0,t\nz1,t\n");
  OcelFeatureOptions options;
  options.feature_name_style = OcelFeatureNameStyle::kCompatibility;
  options.numeric_event_attributes = {"x0"};
  const auto run = [&](bool events) {
    auto with = options;
    (events ? with.include_related_events_features : with.include_related_activities_features) = true;
    return error_of([&] { (void)extract_ocel_features(log, OcelFeatureVariant::kObjects, with); });
  };
  expect(run(true) == "ValueError: could not convert string to float: 's'",
         "FH1-4 the related events' num attributes come before the endpoints");
  expect(run(false) == "ValueError: could not convert string to float: 's'",
         "FH1-4 the related activities' too");
  options.numeric_event_attributes.clear();
  options.string_event_attributes = {"x0"};
  expect(run(true) == "KeyError: 'z1'", "FH1-4 without a failing family the endpoints' KeyError stays");
}

}  // namespace

int main() {
  fh1_1_nat_event_id();
  fh1_2_window_on_nulls();
  fh1_3_aware_datetime_slot();
  fh1_4_related_event_attributes();
  if (failures) {
    std::cerr << failures << " review FH1 check(s) failed\n";
    return 1;
  }
  std::cout << "review FH1 checks passed\n";
  return 0;
}
