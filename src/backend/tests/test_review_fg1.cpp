// Review FG1 (round FG, re-review of FF): checks for the fixes of this round.
//
// - FG1-1 The OCEL features' attribute families in pm4py's names
//   (feature_name_style=compatibility): the numeric attributes are float()
//   of each row's cell (a null 0, a text float() cannot read ValueError, a
//   datetime TypeError), the string attributes one column per str() of a
//   non-null cell, the objects' string family concatenating the raw value to
//   the name (TypeError for one that is no str, the first value of the
//   column's Python set), every family named "@@event_num_<key>" for the
//   numeric attributes, a missing key KeyError "['key'] not in index", the
//   families between the relation counts and the endpoints, a repeated key
//   one string family. Native ignored the lists under these names.
// - FG1-2 A JSON null column whose events without relations the reader
//   dropped: pandas types the column over every row before the drop, so
//   nulls alone keep it object (None) and a dropped row without the
//   attribute makes it float64 (NaN). Native counted the kept rows' nulls
//   against every row (NaN). The frame hint now carries the dropped rows'
//   nulls, through the transport too.
// - FG1-3 The edge metrics' activity of a datetime64 attribute column: a
//   missing cell is the label NaT, as pm4py's associations per edge key it.
//
// See docs/reference-2.7.22-parity.md, Review FG1.
#include "minera/ocdfg_edge_metrics.hpp"
#include "minera/ocdfg_edge_performance.hpp"
#include "minera/ocel.hpp"
#include "minera/ocel_csv.hpp"
#include "minera/ocel_features.hpp"

#include <algorithm>
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
  } catch (const OcdfgPerformanceError& error) {
    return error.type + ": " + error.what();
  } catch (const std::exception& error) {
    return error.what();
  }
  return {};
}

// t4/l1.json: events e0..e3 of "a" related to o0; n = 1, "2.5", null,
// (none); z = "x", true, "x", (none).
const char* const kEventLog =
    R"({"objectTypes":[{"name":"t","attributes":[]}],"eventTypes":[{"name":"a","attributes":[)"
    R"({"name":"n","type":"string"},{"name":"z","type":"string"}]}],"objects":[{"id":"o0","type":"t"}],"events":[)"
    R"({"id":"e0","type":"a","time":"2024-01-01T10:00:00Z","attributes":[{"name":"n","value":1},{"name":"z","value":"x"}],)"
    R"("relationships":[{"objectId":"o0","qualifier":"q"}]},)"
    R"({"id":"e1","type":"a","time":"2024-01-01T11:00:00Z","attributes":[{"name":"n","value":"2.5"},{"name":"z","value":true}],)"
    R"("relationships":[{"objectId":"o0","qualifier":"q"}]},)"
    R"({"id":"e2","type":"a","time":"2024-01-01T12:00:00Z","attributes":[{"name":"n","value":null},{"name":"z","value":"x"}],)"
    R"("relationships":[{"objectId":"o0","qualifier":"q"}]},)"
    R"({"id":"e3","type":"a","time":"2024-01-01T13:00:00Z","attributes":[],"relationships":[{"objectId":"o0","qualifier":"q"}]}]})";

// t4/l2.json: objects o0 (s = 7) and o1 (s = true), both related to e0.
const char* const kObjectLog =
    R"({"objectTypes":[{"name":"t","attributes":[{"name":"s","type":"string"}]}],"eventTypes":[{"name":"a","attributes":[]}],)"
    R"("objects":[{"id":"o0","type":"t","attributes":[{"name":"s","value":7,"time":"1970-01-01T00:00:00Z"}]},)"
    R"({"id":"o1","type":"t","attributes":[{"name":"s","value":true,"time":"1970-01-01T00:00:00Z"}]}],)"
    R"("events":[{"id":"e0","type":"a","time":"2024-01-01T10:00:00Z","relationships":[)"
    R"({"objectId":"o0","qualifier":"q"},{"objectId":"o1","qualifier":"q"}]}]})";

OcelFeatureTable features(const Ocel& log, OcelFeatureVariant variant, const std::function<void(OcelFeatureOptions&)>& set) {
  OcelFeatureOptions options;
  options.feature_name_style = OcelFeatureNameStyle::kCompatibility;
  set(options);
  return extract_ocel_features(log, variant, options);
}

// The column `name` of `table` (empty when it has none).
std::vector<double> column(const OcelFeatureTable& table, const std::string& name) {
  const auto at = std::find(table.feature_names.begin(), table.feature_names.end(), name);
  std::vector<double> values;
  if (at == table.feature_names.end()) return values;
  const auto index = static_cast<std::size_t>(at - table.feature_names.begin());
  for (const auto& row : table.values) values.push_back(row[index]);
  return values;
}

std::size_t count_prefix(const OcelFeatureTable& table, const std::string& prefix) {
  return static_cast<std::size_t>(std::count_if(table.feature_names.begin(), table.feature_names.end(),
                                                [&](const std::string& name) { return name.rfind(prefix, 0) == 0; }));
}

void fg1_1_attribute_families() {
  const auto log = parse_ocel_json(kEventLog);
  const auto events = OcelFeatureVariant::kEvents;
  const auto numeric = features(log, events, [](OcelFeatureOptions& o) { o.numeric_event_attributes = {"n"}; });
  expect(column(numeric, "@@event_num_n") == std::vector<double>({1.0, 2.5, 0.0, 0.0}),
         "FG1-1 the numeric attribute is float() of the cell, a null or missing cell 0");
  const auto strings = features(log, events, [](OcelFeatureOptions& o) { o.string_event_attributes = {"z"}; });
  expect(column(strings, "@@event_attr_value_z_x") == std::vector<double>({1.0, 0.0, 1.0, 0.0}) &&
             column(strings, "@@event_attr_value_z_True") == std::vector<double>({0.0, 1.0, 0.0, 0.0}) &&
             count_prefix(strings, "@@event_attr_value_") == 2,
         "FG1-1 the string attribute is one column per str() of a cell");
  const auto repeated = features(log, events, [](OcelFeatureOptions& o) { o.string_event_attributes = {"z", "z"}; });
  expect(count_prefix(repeated, "@@event_attr_value_") == 2, "FG1-1 a repeated key is one string family");
  expect(error_of([&] { (void)features(log, events, [](OcelFeatureOptions& o) { o.numeric_event_attributes = {"z"}; }); }) ==
             "ValueError: could not convert string to float: 'x'",
         "FG1-1 a text float() cannot read raises ValueError");
  expect(error_of([&] { (void)features(log, events, [](OcelFeatureOptions& o) { o.numeric_event_attributes = {"zz"}; }); }) ==
             "KeyError: \"['zz'] not in index\"",
         "FG1-1 a missing key raises KeyError");

  const auto objects_log = parse_ocel_json(kObjectLog);
  const auto objects = OcelFeatureVariant::kObjects;
  // {7, True} iterates True (slot 1) before 7 (slot 7) in CPython's set.
  expect(error_of([&] { (void)features(objects_log, objects, [](OcelFeatureOptions& o) { o.string_object_attributes = {"s"}; }); }) ==
             "TypeError: can only concatenate str (not \"bool\") to str",
         "FG1-1 the objects' string family raises on the set's first value that is no str");
  const auto object_numbers = features(objects_log, objects, [](OcelFeatureOptions& o) { o.numeric_object_attributes = {"s"}; });
  expect(column(object_numbers, "@@event_num_s") == std::vector<double>({7.0, 1.0}),
         "FG1-1 the objects' numeric family is named @@event_num_<key>");

  // An OCEL CSV whose objects file lists t1 without relations: the numeric
  // attributes fail before the endpoints' lookup of t1.
  const auto csv = parse_ocel_csv("ocel:eid,ocel:timestamp,ocel:activity,n,ocel:type:t\ne0,2024-01-01 10:00:00,a,x y,['t0']\n",
                                  "ocel:oid,ocel:type\nt0,t\nt1,t\n");
  expect(error_of([&] { (void)features(csv, events, [](OcelFeatureOptions& o) { o.numeric_event_attributes = {"n"}; }); }) ==
             "ValueError: could not convert string to float: 'x y'",
         "FG1-1 the numeric attributes come before the endpoints");
  expect(error_of([&] { (void)features(csv, events, [](OcelFeatureOptions&) {}); }) == "KeyError: 't1'",
         "FG1-1 without attributes the endpoints raise");
}

// t4/l4.json: e0..e2 hold kk = null, e2 has no relations (dropped by the
// reader); `last` replaces e2's attributes.
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

std::string timed_error(const Ocel& log) {
  return error_of([&] {
    OcdfgDiscoveryOptions options;
    OcdfgEdgePerformanceOptions timing;
    timing.timestamp_attribute = "kk";
    options.edge_performance_options = timing;
    (void)discover_ocdfg_classic_with_options(log, options);
  });
}

void fg1_2_dropped_nulls() {
  const std::string none = "TypeError: unsupported operand type(s) for -: 'NoneType' and 'NoneType'";
  const auto nulls = dropped_log(R"({"name":"kk","value":null})");
  expect(nulls.events.size() == 2, "FG1-2 the reader drops the event without relations");
  expect(timed_error(nulls) == none, "FG1-2 a dropped row's null keeps the column of nulls object (None)");
  expect(timed_error(parse_ocel_json(serialize_ocel_json(nulls))) == none,
         "FG1-2 the transport carries the dropped rows' nulls");
  expect(timed_error(dropped_log("")).empty(), "FG1-2 a dropped row without the attribute makes the nulls NaN");
}

void fg1_3_nat_label() {
  const auto log = parse_ocel_json(
      R"({"objectTypes":[{"name":"t","attributes":[]}],"eventTypes":[{"name":"a","attributes":[{"name":"d","type":"time"}]}],)"
      R"("objects":[{"id":"o0","type":"t"}],"events":[)"
      R"({"id":"e0","type":"a","time":"2024-01-01T10:00:00Z","attributes":[{"name":"d","value":"2024-02-01T10:00:00Z"}],)"
      R"("relationships":[{"objectId":"o0","qualifier":"q"}]},)"
      R"({"id":"e1","type":"a","time":"2024-01-01T11:00:00Z","attributes":[{"name":"d","value":"2024-02-02T10:00:00Z"}],)"
      R"("relationships":[{"objectId":"o0","qualifier":"q"}]},)"
      R"({"id":"e2","type":"a","time":"2024-01-01T12:00:00Z","attributes":[],"relationships":[{"objectId":"o0","qualifier":"q"}]}]})");
  OcdfgAssociationOptions association;
  association.activity_attribute = "d";
  const auto associations = find_ocdfg_edge_associations(log, association);
  bool nat = false;
  if (associations.count("t"))
    for (const auto& [pair, rows] : associations.at("t")) nat = nat || pair.second == "NaT";
  expect(nat, "FG1-3 a missing cell of a datetime64 activity column is the label NaT");
}

}  // namespace

int main() {
  fg1_1_attribute_families();
  fg1_2_dropped_nulls();
  fg1_3_nat_label();
  if (failures) {
    std::cerr << failures << " review FG1 check(s) failed\n";
    return 1;
  }
  std::cout << "review FG1 checks passed\n";
  return 0;
}
