// Review FD1: the OC-DFG discovery's own selectors, the exact attribute
// filters with None, NaT and NaN, and the activity/type matching filter's
// temporary column on a relations column (review-harness round-fd: q/ocd.py,
// q/xe.py, q/csw.py and q/crsw.py over banks o81 and q28). Observed
// black-box on pm4py 2.7.22 (pandas 3.0.5):
//
// - FD1-1: the OC-DFG discovery with an activity, object type or event id
//   selector reads the events, objects and relations frames as they are
//   (the activity from the events frame, the type from the objects frame,
//   the relations frame's six columns), and with an events attribute column
//   as the time column it subtracts the frame's Python cells: two numbers
//   measure 0, two datetimes their difference, anything else raises with
//   the operands' type names (a missing cell is a float NaN, an explicit
//   null None, a date-read text a datetime.datetime), naive next to aware
//   raises, and with business hours each side's replace() raises first;
//   the first pair measured is the first couple of the first edge of the
//   first object type, types and edges in the order of their first couple
//   in the traversal of the events. Native joined relation rows with the
//   canonical columns and timed every pair by the cells' texts.
// - FD1-2: the exact filters pass the values to isin, which matches None,
//   NaT and NaN by the column's dtype: a str column matches its missing
//   cells with any of the three, an object column an explicit None with
//   None and a missing cell with NaN, a float64 column NaN alone, a
//   datetime64 column NaT alone. Native compared None, NaN and NaT as
//   values.
// - FD1-3: activity_type_matching drops its temporary column from the
//   result's relations frame, a canonical one too, so the routes that read
//   that column then raise KeyError (every writer for the qualifier, the
//   CSV writer and the per-type filters for the type, ...; the object type
//   activities of a two-row frame without the activity or the type give
//   {'c': {'o'}}). Native kept it.
// - FD1-4: read_ocel2_sqlite drops the event-object rows naming ids the
//   event or object table does not list before it fills a NULL qualifier,
//   so a NULL there leaves a REAL column float64. Native made it object.
//
// See docs/reference-2.7.22-parity.md, Review FD1.
#include "minera/ocdfg_edge_performance.hpp"
#include "minera/ocel.hpp"
#include "minera/ocel_activity_type_matching.hpp"
#include "minera/ocel_csv.hpp"
#include "minera/ocel_filter.hpp"
#include "minera/ocel_sqlite.hpp"
#include "minera/ocel_statistics.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#if defined(MINERA_HAS_SQLITE)
#if defined(_WIN32)
#include <winsqlite/winsqlite3.h>
#else
#include <sqlite3.h>
#endif
#endif

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

std::vector<std::string> event_ids(const Ocel& log) {
  std::vector<std::string> ids;
  for (const auto& event : log.events) ids.push_back(event.id);
  std::sort(ids.begin(), ids.end());
  return ids;
}

// An OCEL 2.0 JSON log: events e0..e<n-1> of activity "a" one hour apart,
// each related to o1 (type "t"), event e<i> holding the attribute `cells[i]`
// (a JSON member, empty for none); "s" declared as `declared`.
Ocel chain_log(const std::vector<std::string>& cells, const std::string& declared = "string") {
  std::string events;
  for (std::size_t i = 0; i < cells.size(); ++i) {
    if (i) events += ",";
    events += R"({"id":"e)" + std::to_string(i) + R"(","type":"a","time":"2024-01-01T0)" + std::to_string(i) +
              R"(:00:00Z","attributes":[)" + cells[i] + R"(],"relationships":[{"objectId":"o1","qualifier":"q"}]})";
  }
  return parse_ocel_json(R"({"objectTypes":[{"name":"t","attributes":[]}],"eventTypes":[{"name":"a","attributes":[{"name":"s","type":")" +
                         declared + R"("}]}],"objects":[{"id":"o1","type":"t"}],"events":[)" + events + "]}");
}

std::string cell(const std::string& value) { return R"({"name":"s","value":)" + value + "}"; }

OcdfgGraph timed(const Ocel& log, bool business_hours = false) {
  OcdfgDiscoveryOptions options;
  OcdfgEdgePerformanceOptions timing;
  timing.timestamp_attribute = "s";
  timing.business_hours = business_hours;
  options.edge_performance_options = timing;
  return discover_ocdfg_classic_with_options(log, options);
}

std::vector<double> couple_performance(const OcdfgGraph& graph) {
  std::vector<double> result;
  for (const auto& [type, metrics] : graph.edges)
    for (const auto& [pair, edge] : metrics)
      result.insert(result.end(), edge.performance_event_couples.begin(), edge.performance_event_couples.end());
  return result;
}

void fd1_1_timestamp_attribute_cells() {
  // An object column (an int and a text): the difference of the second
  // event's str and the first's int.
  expect(error_of([] { (void)timed(chain_log({cell("1"), cell(R"("x")")})); }) ==
             "TypeError: unsupported operand type(s) for -: 'str' and 'int'",
         "FD1-1 object column int then str raises with both type names");
  // A float64 column: two numbers measure 0.
  const auto numbers = timed(chain_log({cell("1.5"), cell("2.5"), cell("4")}));
  expect(couple_performance(numbers) == std::vector<double>({0.0, 0.0}), "FD1-1 float64 column measures 0");
  // A missing cell next to a text is a float NaN.
  expect(error_of([] { (void)timed(chain_log({cell(R"("x")"), ""})); }) ==
             "TypeError: unsupported operand type(s) for -: 'float' and 'str'",
         "FD1-1 str column missing cell is a float");
  // Business hours: the first side's replace() raises first.
  expect(error_of([] { (void)timed(chain_log({cell("true"), cell(R"("x")")}), true); }) ==
             "AttributeError: 'bool' object has no attribute 'replace'",
         "FD1-1 business hours bool cell has no replace");
  // Naive next to aware dates (declared time; a reader keeping the naive
  // text): an object column of datetimes.
  auto mixed = chain_log({cell(R"("2024-01-01T10:00:00Z")"), cell(R"("2024-01-02T10:00:00Z")")}, "time");
  mixed.events[0].attributes[0].value = std::string("2024-01-01T10:00:00");
  expect(error_of([&] { (void)timed(mixed); }) == "TypeError: can't subtract offset-naive and offset-aware datetimes",
         "FD1-1 naive and aware dates cannot subtract");
  // Aware dates alone: a datetime64 column measures as the canonical one.
  const auto dates = timed(chain_log({cell(R"("2024-01-01T10:00:00Z")"), cell(R"("2024-01-01T12:30:00Z")")}, "time"));
  expect(couple_performance(dates) == std::vector<double>({9000.0}), "FD1-1 datetime64 column measures its difference");
  // The first pair measured: the types by their first couple in the
  // traversal of the events, then each type's edges by their first couple.
  const auto ordered = [](const std::string& types, const std::string& objects, const std::string& events) {
    return parse_ocel_json(R"({"objectTypes":[)" + types + R"(],"eventTypes":[{"name":"a","attributes":[{"name":"s","type":"string"}]},)" +
                           R"({"name":"b","attributes":[]},{"name":"c","attributes":[]},{"name":"d","attributes":[]}],"objects":[)" + objects +
                           R"(],"events":[)" + events + "]}");
  };
  const auto event = [](int i, const std::string& activity, const std::string& attributes, const std::string& object) {
    return R"({"id":"e)" + std::to_string(i) + R"(","type":")" + activity + R"(","time":"2024-01-01T0)" + std::to_string(i) +
           R"(:00:00Z","attributes":[)" + attributes + R"(],"relationships":[{"objectId":")" + object + R"(","qualifier":"q"}]})";
  };
  // One type: the edge (d, c) has its couple before (a, b), whose first
  // event comes first.
  const auto edges = ordered(R"({"name":"t","attributes":[]})", R"({"id":"o1","type":"t"},{"id":"o2","type":"t"})",
                             event(0, "a", "", "o1") + "," + event(1, "d", cell(R"("x")"), "o2") + "," +
                                 event(2, "c", cell(R"("y")"), "o2") + "," + event(3, "b", "", "o1"));
  expect(error_of([&] { (void)timed(edges, true); }) == "TypeError: str.replace() takes no keyword arguments",
         "FD1-1 edges measure in the order of their first couple");
  // Two types: u's couple comes first, though t sorts first.
  const auto types = ordered(R"({"name":"t","attributes":[]},{"name":"u","attributes":[]})",
                             R"({"id":"o1","type":"u"},{"id":"o2","type":"t"})",
                             event(0, "a", "", "o1") + "," + event(1, "a", "", "o1") + "," + event(2, "a", cell(R"("x")"), "o2") +
                                 "," + event(3, "a", cell(R"("y")"), "o2"));
  expect(error_of([&] { (void)timed(types, true); }) == "AttributeError: 'float' object has no attribute 'replace'",
         "FD1-1 types measure in the order of their first couple");
  expect(error_of([&] { (void)timed(types); }) == "TypeError: unsupported operand type(s) for -: 'str' and 'str'",
         "FD1-1 two texts cannot subtract");
}

// ocel:timestamp as the activity on a CSV read with missing times: a null
// label the associations drop, the start and end activities skip, the
// edges label NaT and the activities list as None (naive) or NaT.
void fd1_1_missing_time_activity() {
  for (const bool aware : {false, true}) {
    const std::string zone = aware ? "+00:00" : "";
    const auto log = parse_ocel_csv("ocel:eid,ocel:timestamp,ocel:activity,ocel:type:t,ocel:type:u\n"
                                    "e0,NaT,a,['o1'],['o3']\n"
                                    "e1,2024-01-02 00:00:00" + zone + ",b,['o1'],[]\n"
                                    "e2,NaT,c,\"['o1', 'o2', 'o4']\",['o3']\n"
                                    "e3,2024-01-04 00:00:00" + zone + ",d,['o4'],[]\n");
    std::map<std::string, std::string> time;
    for (const auto& event : log.events) time[event.id] = event.timestamp;
    const auto& second = time.at("e1");
    const auto& fourth = time.at("e3");
    OcdfgDiscoveryOptions options;
    options.activity_attribute = "ocel:timestamp";
    options.compute_edges_performance = false;
    const auto graph = discover_ocdfg_classic_with_options(log, options);
    const std::string what = aware ? " (aware)" : " (naive)";
    const std::string missing = aware ? "NaT" : "None";
    expect(std::find(graph.activities.begin(), graph.activities.end(), missing) != graph.activities.end(),
           "FD1-1 the activities list the missing time" + what);
    const auto events_of = [](const std::map<std::string, OcdfgActivityMetric>& metrics) {
      std::map<std::string, std::vector<std::string>> result;
      for (const auto& [label, metric] : metrics) result[label] = metric.events;
      return result;
    };
    using Events = std::map<std::string, std::vector<std::string>>;
    const Events kept = {{second, {"e1"}}, {fourth, {"e3"}}};
    expect(events_of(graph.activities_indep) == kept, "FD1-1 the associations drop the missing time" + what);
    expect(graph.activities_ot.count("u") && graph.activities_ot.at("u").empty() &&
               events_of(graph.activities_ot.at("t")) == kept,
           "FD1-1 the type associations drop the missing time" + what);
    expect(graph.start_activities.count("u") && graph.start_activities.at("u").empty() &&
               events_of(graph.start_activities.at("t")) == kept,
           "FD1-1 the start activities take the first label" + what);
    expect(graph.end_activities.count("u") && graph.end_activities.at("u").empty() &&
               events_of(graph.end_activities.at("t")) == Events({{second, {"e2"}}, {fourth, {"e2"}}}),
           "FD1-1 the end activities take the last label with the last event" + what);
    std::set<std::pair<std::string, std::string>> edges;
    for (const auto& [pair, edge] : graph.edges.at("t")) edges.insert(pair);
    expect(edges == std::set<std::pair<std::string, std::string>>({{second, "NaT"}, {"NaT", "NaT"}, {fourth, "NaT"}}),
           "FD1-1 the edges label the missing time NaT" + what);
  }
}

void fd1_2_null_values() {
  const OcelEventAttributeFilterValue none = nullptr, nat = OcelAttributeNaT{},
                                      nan = std::numeric_limits<double>::quiet_NaN();
  const auto kept = [](const Ocel& log, const OcelEventAttributeFilterValue& value) {
    return event_ids(filter_ocel_event_attribute_exact(log, "s", {value}));
  };
  // A str column: e1's missing cell and e2's null are NaN, matched by all three.
  const auto texts = chain_log({cell(R"("y")"), "", cell("null")});
  for (const auto& value : {none, nat, nan})
    expect(kept(texts, value) == std::vector<std::string>({"e1", "e2"}), "FD1-2 str column matches its missing cells");
  // An object column: None matches the explicit null, NaN the missing cell.
  const auto objects = chain_log({cell("1"), cell(R"("y")"), cell("null"), ""});
  expect(kept(objects, none) == std::vector<std::string>({"e2"}), "FD1-2 object column None matches None");
  expect(kept(objects, nan) == std::vector<std::string>({"e3"}), "FD1-2 object column NaN matches a missing cell");
  expect(kept(objects, nat).empty(), "FD1-2 object column NaT matches nothing");
  // A float64 column: NaN alone matches.
  const auto reals = chain_log({cell("1.5"), ""});
  expect(kept(reals, nan) == std::vector<std::string>({"e1"}), "FD1-2 float64 column NaN matches");
  expect(kept(reals, none).empty() && kept(reals, nat).empty(), "FD1-2 float64 column None and NaT match nothing");
  // A datetime64 column (declared time): NaT alone matches.
  const auto dates = chain_log({cell(R"("2024-01-01T10:00:00Z")"), ""}, "time");
  expect(kept(dates, nat) == std::vector<std::string>({"e1"}), "FD1-2 datetime64 column NaT matches");
  expect(kept(dates, nan).empty() && kept(dates, none).empty(), "FD1-2 datetime64 column None and NaN match nothing");
  // Negated: the other events.
  expect(event_ids(filter_ocel_event_attribute_exact(objects, "s", {none}, false)) ==
             std::vector<std::string>({"e0", "e1", "e3"}),
         "FD1-2 negated None keeps the rest");
}

void fd1_3_temporary_column() {
  const auto log = chain_log({cell(R"("y")"), cell(R"("z")")});
  const std::map<std::string, std::vector<std::string>> allowed = {{"t", {"a"}}};
  for (const std::string column : {"ocel:type", "ocel:qualifier", "ocel:activity", "ocel:timestamp"}) {
    OcelActivityTypeMatchingOptions options;
    options.temporary_column = column;
    // A route reads its input from the transport of the previous result.
    const auto result = parse_ocel_json(serialize_ocel_json(filter_ocel_activity_type_matching_exact(log, allowed, options)));
    expect(result.events.size() == 2, "FD1-3 kept events with " + column);
    expect(ocel_relation_column_dropped(result, column), "FD1-3 relations frame lacks " + column);
    expect(error_of([&] { require_ocel_relation_column(result, column); }) == "KeyError: '" + column + "'",
           "FD1-3 reading " + column + " raises");
    expect(error_of([&] { require_ocel_relation_column_found(result, column); }) ==
               "KeyError: 'Column not found: " + column + "'",
           "FD1-3 a groupby on " + column + " raises");
    expect(error_of([&] { require_ocel_relation_column(result, "ocel:eid"); }).empty(), "FD1-3 other columns stay");
  }
  // Another matching filter on the dropped type raises KeyError.
  OcelActivityTypeMatchingOptions options;
  options.temporary_column = "ocel:type";
  const auto dropped = parse_ocel_json(serialize_ocel_json(filter_ocel_activity_type_matching_exact(log, allowed, options)));
  expect(error_of([&] { (void)filter_ocel_activity_type_matching_exact(dropped, allowed); }) == "'ocel:type'",
         "FD1-3 matching on a dropped type raises");
  // A log the reference builds anew (a filter's, an enrichment's) has a
  // qualifier column again; the other columns stay dropped.
  OcelActivityTypeMatchingOptions qualifier;
  qualifier.temporary_column = "ocel:qualifier";
  auto rebuilt = filter_ocel_activity_type_matching_exact(log, allowed, qualifier);
  restore_ocel_relation_qualifier(rebuilt);
  expect(!ocel_relation_column_dropped(rebuilt, "ocel:qualifier") && !rebuilt.table_columns.relations,
         "FD1-3 a new OCEL has its qualifier again");
  auto typed = dropped;
  restore_ocel_relation_qualifier(typed);
  expect(ocel_relation_column_dropped(typed, "ocel:type"), "FD1-3 a dropped type stays dropped");
  // The object type activities of a two-row frame without the column:
  // {'c': {'o'}}; with three rows KeyError.
  const OcelObjectTypeActivities quirk = {{"c", {"o"}}};
  expect(object_type_activities(dropped) == quirk, "FD1-3 two rows without the type give {'c': {'o'}}");
  auto three = parse_ocel_json(serialize_ocel_json(filter_ocel_activity_type_matching_exact(
      chain_log({cell(R"("y")"), cell(R"("z")"), cell(R"("w")")}), allowed, options)));
  expect(error_of([&] { (void)object_type_activities(three); }) == "KeyError: 'ocel:type'",
         "FD1-3 three rows without the type raise");
  // The log the filter started from keeps its frame.
  expect(!ocel_relation_column_dropped(log, "ocel:type"), "FD1-3 source frame unchanged");
}

// FD1-4: read_ocel2_sqlite fills a NULL qualifier after the rows naming ids the
// event or object table does not list drop: a float64 column stays float64
// then, and a NULL on a listed row makes it object.
void fd1_sqlite_null_qualifier() {
#if defined(MINERA_HAS_SQLITE)
  const auto log = parse_ocel_json(
      R"({"objectTypes":[{"name":"t","attributes":[]}],"eventTypes":[{"name":"a","attributes":[]}],)"
      R"("objects":[{"id":"o1","type":"t"},{"id":"o2","type":"t"}],"events":[)"
      R"({"id":"e1","type":"a","time":"2024-01-01T00:00:00Z","attributes":[],"relationships":[{"objectId":"o1","qualifier":2.5}]},)"
      R"({"id":"e2","type":"a","time":"2024-01-01T01:00:00Z","attributes":[],"relationships":[{"objectId":"o1","qualifier":3.5},{"objectId":"o2","qualifier":4.5}]}]})");
  const auto read_with = [&](const std::string& event, const std::string& object) {
    const auto path = std::filesystem::temp_directory_path() / "minera_review_fd1_null.sqlite";
    std::filesystem::remove(path);
    export_ocel_sqlite(log, path);
    sqlite3* db = nullptr;
    if (sqlite3_open(path.string().c_str(), &db) != SQLITE_OK) throw std::runtime_error("open the written file");
    const std::string insert = "INSERT INTO event_object (ocel_event_id, ocel_object_id, ocel_qualifier) VALUES ('" +
                               event + "', '" + object + "', NULL)";
    const bool inserted = sqlite3_exec(db, insert.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK;
    sqlite3_close(db);
    expect(inserted, "FD1 the NULL row is written");
    auto read = import_ocel_sqlite(path);
    std::filesystem::remove(path);
    return read;
  };
  for (const auto& [event, object] : std::vector<std::pair<std::string, std::string>>{{"zz", "o1"}, {"e1", "zz"}}) {
    const auto read = read_with(event, object);
    expect(read.relations.size() == 3 && read.relation_qualifier_dtype == OcelQualifierKind::kFloat,
           "FD1 a NULL on an unlisted id's row leaves float64 (" + event + ", " + object + ")");
  }
  expect(read_with("e1", "o2").relation_qualifier_dtype == OcelQualifierKind::kStr,
         "FD1 a NULL on a listed row makes the column object");
#endif
}

}  // namespace

int main() {
  fd1_1_timestamp_attribute_cells();
  fd1_1_missing_time_activity();
  fd1_2_null_values();
  fd1_3_temporary_column();
  fd1_sqlite_null_qualifier();
  if (failures != 0) {
    std::cerr << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "review FD1 tests passed\n";
  return 0;
}
