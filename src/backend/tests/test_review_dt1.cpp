// Review DT1: degenerate OCELs through the reference readers and the OCEL
// routes' functions. pm4py 2.7.22's read_ocel_json and read_ocel2_json fail
// on a file that lists no event (KeyError: 'ocel:timestamp'), read a file
// that lists no object as an objects frame without columns (every later
// lookup of ocel:oid or ocel:type raises KeyError), and read a relation-free
// file as a log without events and objects whose relations frame is float64
// (the flattening's merge and the lifecycle enrichment fail). In-memory
// empty logs (filter results, a relation-free SQLite read) keep typed frames
// and work. The SQLite reader fails on an event or object table without rows
// (No objects to concatenate), and filter_ocel_objects with a level above 1
// looks every requested id up in its interaction graph. Observed black-box
// (review-harness round-dt, nb/ocdeg*.py, oc/rd.py, oc/sq.py, oc/typed.py,
// oc/lv.py; the server sweep oc/nat.sh agreed on every route variant).
#include "minera/ocel.hpp"
#include "minera/ocel_features.hpp"
#include "minera/ocel_filter.hpp"
#include "minera/ocel_sqlite.hpp"
#include "minera/ocel_statistics.hpp"
#include "minera/ocel_transform.hpp"

#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace minera;

namespace {

int failures = 0;

void expect(bool condition, const std::string& what) {
  if (!condition) {
    ++failures;
    std::cerr << "FAILED: " << what << '\n';
  }
}

std::string error_of(const std::function<void()>& call) {
  try {
    call();
  } catch (const std::exception& error) {
    return error.what();
  }
  return {};
}

const std::string kNoObjects2 =
    R"({"objectTypes":[{"name":"t","attributes":[]}],"eventTypes":[{"name":"a","attributes":[]}],"objects":[],)"
    R"("events":[{"id":"e1","type":"a","time":"2024-01-01T00:00:00Z","attributes":[],"relationships":[]}]})";
const std::string kNoObjects1 =
    R"({"ocel:global-event":{},"ocel:global-object":{},"ocel:global-log":{},"ocel:objects":{},)"
    R"("ocel:events":{"e1":{"ocel:activity":"a","ocel:timestamp":"2024-01-01T00:00:00Z","ocel:omap":[],"ocel:vmap":{}}}})";
const std::string kNoEvents2 =
    R"({"objectTypes":[{"name":"t","attributes":[]}],"eventTypes":[{"name":"a","attributes":[]}],)"
    R"("objects":[{"id":"o1","type":"t","attributes":[]}],"events":[]})";
const std::string kNoEvents1 =
    R"({"ocel:global-event":{},"ocel:global-object":{},"ocel:global-log":{},)"
    R"("ocel:objects":{"o1":{"ocel:type":"t","ocel:ovmap":{}}},"ocel:events":{}})";
const std::string kNoRelations2 =
    R"({"objectTypes":[{"name":"t","attributes":[]}],"eventTypes":[{"name":"a","attributes":[]},{"name":"b","attributes":[]}],)"
    R"("objects":[{"id":"o1","type":"t"}],"events":[{"id":"e1","type":"a","time":"2024-01-01T00:00:00Z","attributes":[],)"
    R"("relationships":[]},{"id":"e2","type":"b","time":"2024-01-01T00:00:01Z","attributes":[],"relationships":[]}]})";
const std::string kFull2 =
    R"({"objectTypes":[{"name":"t","attributes":[]}],"eventTypes":[{"name":"a","attributes":[]}],)"
    R"("objects":[{"id":"o1","type":"t","attributes":[]}],"events":[{"id":"e1","type":"a","time":"2024-01-01T00:00:00Z",)"
    R"("attributes":[],"relationships":[{"objectId":"o1","qualifier":""}]}]})";

const std::string kNoneOf =
    "KeyError: \"None of [Index(['ocel:oid', 'ocel:type'], dtype='str')] are in the [columns]\"";
const std::string kLifecycle =
    "UFuncTypeError: ufunc 'add' did not contain a loop with signature matching types (dtype('float64'), "
    "dtype('<U2')) -> None";

void event_free_files_fail_at_read() {
  for (const auto* text : {&kNoEvents2, &kNoEvents1})
    expect(error_of([&] { (void)parse_ocel_json(*text); }) == "KeyError: 'ocel:timestamp'", "event-free read");
  // A document Minera wrote for an empty in-memory log reads as that log.
  const auto empty = filter_ocel_events(parse_ocel_json(kFull2), {"zz"}, true);
  const auto transport = serialize_ocel_json(empty);
  expect(transport.find("\"minera:untypedRelations\":false") != std::string::npos, "typed marker");
  const auto again = parse_ocel_json(transport);
  expect(again.events.empty() && !again.untyped_relations, "typed empty log round trip");
  // A download reads as the reference reads the file.
  expect(error_of([&] { (void)parse_ocel_json(serialize_ocel_json(empty, OcelJsonFormat::kClassic, false)); }) ==
             "KeyError: 'ocel:timestamp'",
         "download of an empty log");
}

void object_free_files_raise_key_errors() {
  for (const auto* text : {&kNoObjects2, &kNoObjects1}) {
    const auto log = parse_ocel_json(*text);
    expect(log.table_columns.objects && log.table_columns.objects->empty(), "frame without columns");
    // Kept through the transport.
    const auto again = parse_ocel_json(serialize_ocel_json(log));
    expect(again.table_columns.objects && again.table_columns.objects->empty(), "frame kept by transport");
    expect(error_of([&] { (void)serialize_ocel_description_json(log, OcelDescriptionVariant::kVariant1, {}); }) ==
               kNoneOf,
           "description");
    expect(error_of([&] { (void)serialize_ocel_description_json(log, OcelDescriptionVariant::kVariant2, {}); }) ==
               kNoneOf,
           "description tuples");
    expect(error_of([&] { (void)extract_ocel_features(log, OcelFeatureVariant::kEvents); }) ==
               "KeyError: 'ocel:type'",
           "event features");
    expect(error_of([&] { (void)extract_ocel_features(log, OcelFeatureVariant::kObjects); }) ==
               "KeyError: 'ocel:oid'",
           "object features");
    expect(error_of([&] { (void)discover_otg_classic(log); }) == "KeyError: 'ocel:type'", "otg");
    expect(error_of([&] { (void)discover_ocpn(log); }) == "KeyError: 'ocel:type'", "ocpn");
    expect(error_of([&] { (void)edge_associations(log, {}); }) == "KeyError: 'ocel:oid'", "edge associations");
    expect(error_of([&] { (void)filter_ocel_object_types(log, {"t"}, true, 1); }) == "KeyError: 'ocel:type'",
           "object types filter");
    expect(error_of([&] { (void)filter_ocel_objects(log, {"o1"}, true, 2); }) == "KeyError: 'ocel:oid'",
           "objects filter");
    expect(error_of([&] { (void)filter_ocel_cc_length(log, 1, 10); }) == "KeyError: 'ocel:oid'", "cc length");
    expect(error_of([&] { (void)filter_ocel_cc_object(log, "o1"); }) == "KeyError: 'ocel:oid'", "cc object");
    expect(error_of([&] { (void)filter_ocel_cc_activity(log, "a"); }) == "KeyError: 'ocel:oid'", "cc activity");
    expect(error_of([&] { (void)filter_ocel_cc_object_type(log, "t", true); }) == "KeyError: 'ocel:type'",
           "cc object type");
    expect(error_of([&] { (void)filter_ocel_object_attribute(log, "ocel:type", {OcelValue(std::string("t"))}, true); }) ==
               "KeyError: 'ocel:type'",
           "object attribute filter");
    expect(error_of([&] { (void)flatten_ocel_to_event_log(log, "t"); }) == "KeyError: 'ocel:type'", "flatten");
    const auto path = std::filesystem::temp_directory_path() / "minera_review_dt1.sqlite";
    std::filesystem::remove(path);
    expect(error_of([&] { export_ocel_sqlite(log, path); }) == kNoneOf, "sqlite writer");
    std::filesystem::remove(path);
    // What the reference reads without the objects frame stays as it was.
    expect(error_of([&] { (void)filter_ocel_events(log, {"e1"}, true); }).empty(), "events filter");
    expect(error_of([&] { (void)discover_etot_classic(log); }).empty(), "etot");
  }
}

void relation_free_files_have_float_relations() {
  const auto log = parse_ocel_json(kNoRelations2);
  expect(log.events.empty() && log.objects.empty() && log.untyped_relations, "relation-free read");
  expect(error_of([&] { (void)flatten_ocel_to_event_log(log, "t"); })
             .starts_with("ValueError: You are trying to merge on str and float64 columns"),
         "flatten merge");
  OcelEnrichmentOptions lifecycle;
  lifecycle.include_lifecycle_qualifiers = true;
  expect(error_of([&] { (void)enrich_ocel(log, lifecycle); }) == kLifecycle, "lifecycle enrichment");
  // A filter keeps the frame types, and so does the transport.
  const auto filtered = parse_ocel_json(serialize_ocel_json(filter_ocel_events(log, {"e1"}, true)));
  expect(filtered.untyped_relations, "filter keeps float relations");
  expect(error_of([&] { (void)enrich_ocel(filtered, lifecycle); }) == kLifecycle, "filtered lifecycle");
  // An in-memory empty log has typed frames.
  const auto empty = filter_ocel_events(parse_ocel_json(kFull2), {"zz"}, true);
  expect(error_of([&] { (void)flatten_ocel_to_event_log(empty, "t"); }).empty(), "typed flatten");
  expect(error_of([&] { (void)enrich_ocel(empty, lifecycle); }).empty(), "typed lifecycle");
  expect(error_of([&] { (void)serialize_ocel_description_json(empty, OcelDescriptionVariant::kVariant1, {}); })
             .empty(),
         "typed description");
}

void sqlite_tables_without_rows() {
  const auto path = std::filesystem::temp_directory_path() / "minera_review_dt1_rows.sqlite";
  std::filesystem::remove(path);
  export_ocel_sqlite(parse_ocel_json(kFull2), path);
  const auto read = import_ocel_sqlite(path);
  expect(read.events.size() == 1 && !read.untyped_relations, "sqlite full read");
  std::filesystem::remove(path);
  // The reference writer writes an empty log's tables without rows, which
  // its reader cannot concatenate.
  export_ocel_sqlite(filter_ocel_events(parse_ocel_json(kFull2), {"zz"}, true), path);
  expect(error_of([&] { (void)import_ocel_sqlite(path); }) == "ValueError: No objects to concatenate",
         "sqlite tables without rows");
  std::filesystem::remove(path);
}

void level_expansion_looks_ids_up() {
  const auto log = parse_ocel_json(kFull2);
  expect(error_of([&] { (void)filter_ocel_objects(log, {"zz"}, true, 2); }) == "KeyError: 'zz'", "unknown id");
  expect(error_of([&] { (void)filter_ocel_objects(log, {"it's"}, true, 3); }) == "KeyError: \"it's\"",
         "unknown id repr");
  expect(error_of([&] { (void)filter_ocel_objects(log, {"zz"}, true, 1); }).empty(), "level 1 keeps any id");
  expect(filter_ocel_objects(log, {"o1"}, true, 3).objects.size() == 1, "known id");
}

}  // namespace

int main() {
  event_free_files_fail_at_read();
  object_free_files_raise_key_errors();
  relation_free_files_have_float_relations();
  sqlite_tables_without_rows();
  level_expansion_looks_ids_up();
  if (failures) {
    std::cerr << failures << " review DT1 checks failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "review DT1 checks passed\n";
  return EXIT_SUCCESS;
}
