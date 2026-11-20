// Review FC1: the timestamp column in the activity/type matching filter, the
// OC-DFG edge metrics' selectors, the exact window on a CSV column of empty
// cells and on a frame without rows (review-harness round-fc: q/atm.py,
// q/em.py, q/csw.py over banks o81, q26 and o73; t1/atmts.py, t2/em1.py,
// t2/w.py). Observed black-box on pm4py 2.7.22 (pandas 3.0.5):
//
// - FC1-1: activity_type_matching adds the separator to the relations
//   frame's activity column and then the type column to that; with
//   ocel:timestamp (datetime64) as the activity key it raises TypeError
//   ('DatetimeArray' and 'str'), rows or not; as the type key it raises
//   per cell ('DatetimeArray' with one row, 'Timestamp' with more) and
//   passes a frame without rows. Native kept no event.
// - FC1-2: find_associations_per_edge reads the event id and activity from
//   the events frame, the object id from the relations frame (its six
//   columns), the type from the objects frame, and walks the events
//   frame's ids; native joined each relation row with its event's and
//   object's attributes (event first), so a type key an event attribute
//   shares took the event's value, a repeated event id key paired relation
//   rows, and an attribute as the object id key was accepted.
// - FC1-3: read_ocel_csv reads with dtype=str, so a column of empty cells is
//   a str column and the exact window on it compares nothing (no event
//   kept); native raised for a float64 column.
// - FC1-4: the exact window on a naive datetime64 column raises for its
//   dtype on a frame without rows too (a filter's result keeps its source's
//   dtypes); native passed every empty frame.
//
// See docs/reference-2.7.22-parity.md, Review FC1.
#include "minera/ocdfg_edge_metrics.hpp"
#include "minera/ocel.hpp"
#include "minera/ocel_activity_type_matching.hpp"
#include "minera/ocel_csv.hpp"
#include "minera/ocel_filter.hpp"

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
  } catch (const std::exception& error) {
    return error.what();
  }
  return {};
}

// A route's input: the transport of `log`, read as the exact routes read it
// (literal names).
Ocel exact_input(const Ocel& log) { return parse_ocel_json_literal_names(serialize_ocel_json(log)); }

std::vector<std::string> event_ids(const Ocel& log) {
  std::vector<std::string> ids;
  for (const auto& event : log.events) ids.push_back(event.id);
  std::sort(ids.begin(), ids.end());
  return ids;
}

// Three events of activities A, B, A; e0 and e2 hold o1 (type T) and o2
// (type U), e1 holds o1; an event and an object attribute share the name k.
const char* const kEdgeLog =
    R"({"ocel:global-event":{"ocel:activity":"__INVALID__"},"ocel:global-object":{"ocel:type":"__INVALID__"},)"
    R"("ocel:global-log":{"ocel:attribute-names":["k","lab"],"ocel:object-types":["T","U"],"ocel:version":"1.0",)"
    R"("ocel:ordering":"timestamp"},"ocel:events":{)"
    R"("e0":{"ocel:activity":"A","ocel:timestamp":"2024-01-01T00:00:00+00:00","ocel:omap":["o1","o2"],"ocel:vmap":{"k":"ek0","lab":"s"}},)"
    R"("e1":{"ocel:activity":"B","ocel:timestamp":"2024-01-02T00:00:00+00:00","ocel:omap":["o1"],"ocel:vmap":{"k":"ek1","lab":"f"}},)"
    R"("e2":{"ocel:activity":"A","ocel:timestamp":"2024-01-03T00:00:00+00:00","ocel:omap":["o1","o2"],"ocel:vmap":{"k":"ek2","lab":"s"}}},)"
    R"("ocel:objects":{"o1":{"ocel:type":"T","ocel:ovmap":{"k":"ok1"}},"o2":{"ocel:type":"U","ocel:ovmap":{"k":"ok2"}}}})";

void activity_type_matching_on_the_timestamp() {
  const auto input = parse_ocel_json_literal_names(kEdgeLog);
  const std::map<std::string, std::vector<std::string>> correspondence = {{"T", {"A"}}};
  OcelActivityTypeMatchingOptions activity;
  activity.activity_key = "ocel:timestamp";
  const std::string datetime_array = "TypeError: unsupported operand type(s) for +: 'DatetimeArray' and 'str'";
  expect(error_of([&] { (void)filter_ocel_activity_type_matching_exact(input, correspondence, activity); }) ==
             datetime_array,
         "FC1-1: the timestamp as the activity key raises");
  OcelActivityTypeMatchingOptions type;
  type.object_type_key = "ocel:timestamp";
  expect(error_of([&] { (void)filter_ocel_activity_type_matching_exact(input, correspondence, type); }) ==
             "TypeError: unsupported operand type(s) for +: 'Timestamp' and 'str'",
         "FC1-1: the timestamp as the type key raises per cell");
  auto one = input;
  one.relations.resize(1);
  expect(error_of([&] { (void)filter_ocel_activity_type_matching_exact(exact_input(one), correspondence, type); }) ==
             datetime_array,
         "FC1-1: one relation row names the array");
  // A frame without rows: the activity key still raises, the type key
  // passes (relations typed as the reader typed them).
  const auto none = filter_ocel_events(input, {"none"});
  expect(none.relations.empty(), "FC1-1: the probe keeps no relation");
  expect(error_of([&] { (void)filter_ocel_activity_type_matching_exact(exact_input(none), correspondence, activity); }) ==
             datetime_array,
         "FC1-1: the activity key raises without rows");
  expect(error_of([&] { (void)filter_ocel_activity_type_matching_exact(exact_input(none), correspondence, type); })
             .empty(),
         "FC1-1: the type key passes without rows");
}

using Edges = std::map<OcdfgEdgeKey, std::vector<OcdfgEdgeAssociation>>;

void edge_metrics_selectors() {
  const auto input = parse_ocel_json_literal_names(kEdgeLog);
  const auto run = [&](OcdfgAssociationOptions options) { return find_ocdfg_edge_associations(input, options); };
  const auto canonical = run({});
  expect(canonical == OcdfgEdgeAssociations{{"T", Edges{{{"A", "B"}, {{"e0", "e1", "o1"}}}, {{"B", "A"}, {{"e1", "e2", "o1"}}}}},
                                            {"U", Edges{{{"A", "A"}, {{"e0", "e2", "o2"}}}}}},
         "FC1-2: the canonical associations");
  OcdfgAssociationOptions type;
  type.object_type_attribute = "k";
  expect(run(type) == OcdfgEdgeAssociations{{"ok1", Edges{{{"A", "B"}, {{"e0", "e1", "o1"}}}, {{"B", "A"}, {{"e1", "e2", "o1"}}}}},
                                            {"ok2", Edges{{{"A", "A"}, {{"e0", "e2", "o2"}}}}}},
         "FC1-2: the type key k reads the objects frame");
  OcdfgAssociationOptions repeated;
  repeated.event_id_attribute = "ocel:activity";
  expect(run(repeated) ==
             OcdfgEdgeAssociations{{"T", Edges{{{"A", "A"}, {{"A", "A", "o1"}, {"A", "A", "o1"}}},
                                               {{"A", "B"}, {{"A", "B", "o1"}}},
                                               {{"B", "A"}, {{"B", "A", "o1"}}}}},
                                   {"U", Edges{{{"A", "A"}, {{"A", "A", "o2"}, {"A", "A", "o2"}, {"A", "A", "o2"}}}}}},
         "FC1-2: a repeated event id key walks the events frame's ids with each group's objects");
  OcdfgAssociationOptions by_type;
  by_type.object_id_attribute = "ocel:type";
  expect(run(by_type) == OcdfgEdgeAssociations{{"T", Edges{{{"A", "B"}, {{"e0", "e1", "T"}}}, {{"B", "A"}, {{"e1", "e2", "T"}}}}},
                                               {"U", Edges{{{"A", "A"}, {{"e0", "e2", "U"}}}}}},
         "FC1-2: the type as the object id key");
  OcdfgAssociationOptions label;
  label.activity_attribute = "lab";
  expect(run(label) == OcdfgEdgeAssociations{{"T", Edges{{{"s", "f"}, {{"e0", "e1", "o1"}}}, {{"f", "s"}, {{"e1", "e2", "o1"}}}}},
                                             {"U", Edges{{{"s", "s"}, {{"e0", "e2", "o2"}}}}}},
         "FC1-2: the activity key lab");
  const auto failure = [&](OcdfgAssociationOptions options) { return error_of([&] { (void)run(options); }); };
  OcdfgAssociationOptions object_attribute;
  object_attribute.object_id_attribute = "k";
  expect(failure(object_attribute) == "'Column not found: k'", "FC1-2: an attribute as the object id key raises");
  OcdfgAssociationOptions event_attribute;
  event_attribute.event_id_attribute = "k";
  expect(failure(event_attribute) == "'k'", "FC1-2: an event attribute as the event id key raises for the relations");
  OcdfgAssociationOptions missing_type;
  missing_type.object_type_attribute = "missing";
  expect(failure(missing_type) == "'Column not found: missing'", "FC1-2: a missing type key");
  OcdfgAssociationOptions missing_activity;
  missing_activity.activity_attribute = "missing";
  expect(failure(missing_activity) == "'Column not found: missing'", "FC1-2: a missing activity key");
  // One text, a str under type t and a datetime under type order: two
  // groups in pm4py (a str and a Timestamp key), so the string report
  // rejects the object column's datetime (t2/em3.py).
  const auto mixed = parse_ocel_json_literal_names(serialize_ocel_json(parse_ocel_json(
      R"({"objectTypes":[{"name":"t","attributes":[{"name":"s","type":"string"}]},)"
      R"({"name":"order","attributes":[{"name":"s","type":"time"}]}],)"
      R"("eventTypes":[{"name":"a","attributes":[]},{"name":"b","attributes":[]}],"objects":[)"
      R"({"id":"o1","type":"t","attributes":[{"name":"s","time":"1970-01-01T00:00:00Z","value":"2024-02-01T10:00:00Z"}]},)"
      R"({"id":"o2","type":"order","attributes":[{"name":"s","time":"1970-01-01T00:00:00Z","value":"2024-02-01T10:00:00Z"}]}],)"
      R"("events":[{"id":"e0","type":"a","time":"2024-01-01T10:00:00Z","attributes":[],)"
      R"("relationships":[{"objectId":"o1","qualifier":"q"},{"objectId":"o2","qualifier":"q"}]},)"
      R"({"id":"e1","type":"b","time":"2024-01-02T10:00:00Z","attributes":[],)"
      R"("relationships":[{"objectId":"o1","qualifier":"q"},{"objectId":"o2","qualifier":"q"}]}]})")));
  OcdfgAssociationOptions mixed_type;
  mixed_type.object_type_attribute = "s";
  const auto mixed_error = error_of([&] { (void)find_ocdfg_edge_associations(mixed, mixed_type); });
  expect(mixed_error == "OCDFG association companion requires string cells: s",
         "FC1-2: a datetime next to texts in an object column rejects: " + mixed_error);
}

void window_on_csv_and_empty_frames() {
  const auto csv = parse_ocel_csv(
      "ocel:eid,ocel:timestamp,ocel:activity,ocel:type:t,x\n"
      "e0,2024-02-05 09:00:00,a,['o0'],\ne1,2024-02-06 09:00:00,b,['o0'],\n");
  expect(csv.event_frame_hint.str_columns, "FC1-3: the CSV read marks its str columns");
  const auto input = exact_input(csv);
  expect(input.event_frame_hint.str_columns, "FC1-3: the transport keeps the str columns");
  std::vector<std::string> kept{"unset"};
  const auto error = error_of([&] {
    kept = event_ids(filter_ocel_events_timestamp_exact(input, std::string("2024-01-01 00:00:00"),
                                                        std::string("2024-12-31 00:00:00"), "x", true));
  });
  expect(error.empty() && kept.empty(), "FC1-3: the window on a str column of empty cells keeps no event: " + error);
  // FC1-4: an OCEL 2.0 JSON time attribute dateutil reads as naive, after
  // a filter that keeps no event.
  const std::string document =
      R"({"objectTypes":[{"name":"t","attributes":[]},{"name":"u","attributes":[]}],)"
      R"("eventTypes":[{"name":"a","attributes":[{"name":"d","type":"time"}]}],)"
      R"("objects":[{"id":"o0","type":"t","attributes":[]},{"id":"o1","type":"u","attributes":[]}],"events":[)"
      R"({"id":"e0","type":"a","time":"2024-01-01T10:00:00Z","attributes":[{"name":"d","value":"02/03/2024"}],)"
      R"("relationships":[{"objectId":"o0","qualifier":"q"}]},)"
      R"({"id":"e1","type":"a","time":"2024-01-02T10:00:00Z","attributes":[{"name":"d","value":"02/04/2024 10:00"}],)"
      R"("relationships":[{"objectId":"o0","qualifier":"q"}]}]})";
  const auto log = exact_input(parse_ocel_json(document));
  const std::string naive = "TypeError: Invalid comparison between dtype=datetime64[us] and datetime";
  const auto window = [&](const Ocel& source) {
    return error_of([&] {
      (void)filter_ocel_events_timestamp_exact(source, std::string("2024-01-01 00:00:00"),
                                               std::string("2024-12-31 00:00:00"), "d", true);
    });
  };
  expect(window(log) == naive, "FC1-4: the naive column raises with rows");
  const auto empty = exact_input(filter_ocel_object_types(log, {"u"}));
  expect(empty.events.empty(), "FC1-4: the type filter keeps no event");
  expect(window(empty) == naive, "FC1-4: the naive column raises without rows: " + window(empty));
  // An empty frame's text column passes.
  const auto texts = exact_input(filter_ocel_object_types(parse_ocel_json_literal_names(kEdgeLog), {"none"}));
  expect(texts.events.empty(), "FC1-4: the type filter keeps no event of the edge log");
  const auto text_window = error_of([&] {
    (void)filter_ocel_events_timestamp_exact(texts, std::string("2024-01-01 00:00:00"),
                                             std::string("2024-12-31 00:00:00"), "k", true);
  });
  expect(text_window.empty(), "FC1-4: an empty frame's text column passes: " + text_window);
}

}  // namespace

int main() {
  activity_type_matching_on_the_timestamp();
  edge_metrics_selectors();
  window_on_csv_and_empty_frames();
  if (failures != 0) {
    std::cerr << failures << " review FC1 check(s) failed\n";
    return 1;
  }
  std::cout << "Review FC1: timestamp matching keys, edge-metric selectors, CSV and empty-frame windows passed\n";
}
