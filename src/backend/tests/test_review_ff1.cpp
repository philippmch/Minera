// Review FF1: explicit nulls of an OCEL 2.0 JSON column that pandas makes
// float64, one label per instant for the event time as an OC-DFG or edge
// metrics key, and a missing event time as the edge metrics' activity
// (review-harness round-ff: q/cfl.py, q/emr.py, q/chfl.py, t1/jnull.py,
// t2/ff11.py and t3/tsk2.jsonocel over banks o84, c85, q30 and q31).
// Observed black-box on pm4py 2.7.22 (pandas 3.0.5):
//
// - FF1-1: read_ocel2_json builds the events frame from records: a column
//   holding explicit nulls and no value is object (the nulls None) only
//   when every row holds a null; a row without the attribute (NaN) makes it
//   float64, the nulls NaN. The OC-DFG timed by such a column measures 0
//   (NaN - NaN) where native raised "unsupported operand type(s) for -:
//   'NoneType' and 'float'".
// - FF1-2: the event time is a Timestamp in pm4py's frames, so as the
//   OC-DFG activity or the edge metrics' activity, event id or type key it
//   is one label per instant however the file spells it
//   ("2024-01-01T10:00:00Z" and "2024-01-01T10:00:00+00:00"). Native kept
//   one label per text.
// - FF1-3: a missing event time (an OCEL CSV row's NaT) as the edge
//   metrics' activity is the label NaT, as the OC-DFG's edges print it.
//   Native printed an empty label.
//
// See docs/reference-2.7.22-parity.md, Review FF1.
#include "minera/ocdfg_edge_metrics.hpp"
#include "minera/ocdfg_edge_performance.hpp"
#include "minera/ocel.hpp"
#include "minera/ocel_csv.hpp"

#include <functional>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
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

// An OCEL 2.0 JSON log: events e0..e<n-1> of activity "a" one hour apart,
// each related to o1 (type "t"), event e<i> holding the attribute `cells[i]`
// (a JSON member, empty for none); "s" declared a string.
Ocel chain_log(const std::vector<std::string>& cells) {
  std::string events;
  for (std::size_t i = 0; i < cells.size(); ++i) {
    if (i) events += ",";
    events += R"({"id":"e)" + std::to_string(i) + R"(","type":"a","time":"2024-01-01T0)" + std::to_string(i) +
              R"(:00:00Z","attributes":[)" + cells[i] + R"(],"relationships":[{"objectId":"o1","qualifier":"q"}]})";
  }
  return parse_ocel_json(R"({"objectTypes":[{"name":"t","attributes":[]}],"eventTypes":[{"name":"a","attributes":[)"
                         R"({"name":"s","type":"string"}]}],"objects":[{"id":"o1","type":"t"}],"events":[)" + events + "]}");
}

const std::string kNull = R"({"name":"s","value":null})";

std::vector<double> timed(const Ocel& log) {
  OcdfgDiscoveryOptions options;
  OcdfgEdgePerformanceOptions timing;
  timing.timestamp_attribute = "s";
  options.edge_performance_options = timing;
  const auto graph = discover_ocdfg_classic_with_options(log, options);
  std::vector<double> result;
  for (const auto& [type, metrics] : graph.edges)
    for (const auto& [pair, edge] : metrics)
      result.insert(result.end(), edge.performance_event_couples.begin(), edge.performance_event_couples.end());
  return result;
}

void ff1_1_json_nulls() {
  expect(timed(chain_log({"", kNull, ""})) == std::vector<double>({0.0, 0.0}),
         "FF1-1 a null between missing cells is NaN (float64)");
  expect(timed(chain_log({kNull, kNull, ""})) == std::vector<double>({0.0, 0.0}),
         "FF1-1 nulls next to one missing cell are NaN");
  expect(error_of([] { (void)timed(chain_log({kNull, kNull})); }) ==
             "TypeError: unsupported operand type(s) for -: 'NoneType' and 'NoneType'",
         "FF1-1 a column of nulls alone is object (None)");
  expect(timed(chain_log({kNull, R"({"name":"s","value":1})"})) == std::vector<double>({0.0}),
         "FF1-1 a null next to a number is NaN");
  expect(error_of([] { (void)timed(chain_log({kNull, R"({"name":"s","value":true})"})); }) ==
             "TypeError: unsupported operand type(s) for -: 'bool' and 'NoneType'",
         "FF1-1 a null next to a boolean stays None (object)");
}

// The t3/tsk2.jsonocel log: e0 at 10:00Z, e1 at 10:00+00:00 (the same
// instant), e2 at 11:00Z, all related to o0 (type "t").
Ocel instant_log() {
  return parse_ocel_json(
      R"({"ocel:global-event":{"ocel:activity":"__INVALID__"},"ocel:global-object":{"ocel:type":"__INVALID__"},)"
      R"("ocel:global-log":{"ocel:attribute-names":[],"ocel:object-types":["t"],"ocel:version":"1.0","ocel:ordering":"timestamp"},)"
      R"("ocel:events":{"e0":{"ocel:activity":"a","ocel:timestamp":"2024-01-01T10:00:00Z","ocel:omap":["o0"],"ocel:vmap":{}},)"
      R"("e1":{"ocel:activity":"b","ocel:timestamp":"2024-01-01T10:00:00+00:00","ocel:omap":["o0"],"ocel:vmap":{}},)"
      R"("e2":{"ocel:activity":"c","ocel:timestamp":"2024-01-01T11:00:00Z","ocel:omap":["o0"],"ocel:vmap":{}}},)"
      R"("ocel:objects":{"o0":{"ocel:type":"t","ocel:ovmap":{}}}})");
}

using Edges = std::set<std::pair<std::string, std::string>>;

void ff1_2_instant_labels() {
  const auto log = instant_log();
  const std::string ten = "2024-01-01T10:00:00Z", eleven = "2024-01-01T11:00:00Z";
  OcdfgDiscoveryOptions options;
  options.activity_attribute = "ocel:timestamp";
  options.compute_edges_performance = false;
  const auto graph = discover_ocdfg_classic_with_options(log, options);
  expect(graph.activities == std::vector<std::string>({ten, eleven}), "FF1-2 the OC-DFG lists one activity per instant");
  Edges edges;
  for (const auto& [pair, edge] : graph.edges.at("t")) edges.insert(pair);
  expect(edges == Edges({{ten, ten}, {ten, eleven}}), "FF1-2 the OC-DFG's edges join the instant's events");
  expect(graph.activities_indep.count(ten) && graph.activities_indep.at(ten).events == std::vector<std::string>({"e0", "e1"}),
         "FF1-2 the instant's associations hold both events");

  OcdfgAssociationOptions association;
  association.activity_attribute = "ocel:timestamp";
  const auto associations = find_ocdfg_edge_associations(log, association);
  Edges metric_edges;
  for (const auto& [pair, rows] : associations.at("t")) metric_edges.insert(pair);
  expect(metric_edges == Edges({{ten, ten}, {ten, eleven}}), "FF1-2 the edge metrics join the instant's events");
}

void ff1_3_missing_time_label() {
  const auto log = parse_ocel_csv("ocel:eid,ocel:timestamp,ocel:activity,ocel:type:t\n"
                                  "e0,2024-01-01 10:00:00,a,['o1']\n"
                                  "e1,NaT,b,['o1']\n");
  OcdfgAssociationOptions association;
  association.activity_attribute = "ocel:timestamp";
  const auto associations = find_ocdfg_edge_associations(log, association);
  expect(associations.count("t") && associations.at("t").size() == 1 &&
             associations.at("t").begin()->first.second == "NaT",
         "FF1-3 a missing event time is the label NaT");
}

}  // namespace

int main() {
  ff1_1_json_nulls();
  ff1_2_instant_labels();
  ff1_3_missing_time_label();
  if (failures) {
    std::cerr << failures << " review FF1 check(s) failed\n";
    return 1;
  }
  std::cout << "review FF1 checks passed\n";
  return 0;
}
