// Review EP1: pm4py 2.7.22 on the OCEL 1.0 bank b56, a hand-written OCEL
// CSV bank (b57, with drops, NaT times, offsets and shuffled columns) and
// hand cases (review-harness round-ep: x1/p/p1.py, hc/hc.py, hc/sq.py).
// Observed black-box:
//
// - read_ocel_xml raises the object map's KeyError for the first relation
//   in document order that names no listed object (a dropped event's
//   relation too): the id's repr, or None for a relation element without a
//   value (a nested list included);
// - read_ocel_csv's frames keep the files' columns in file order, an
//   all-empty column and a column only a dropped row fills included, and
//   the frames' own columns where the files put them (a time column first,
//   an object table with the type or the id after an attribute); without an
//   object table the objects frame is (ocel:type, ocel:oid);
//   write_ocel_csv writes the events frame with ocel:eid first and the
//   objects frame as it stands, write_ocel2_sqlite writes the type tables
//   in frame order (ocel:type left out of an object table);
// - read_ocel2_sqlite's frames take the union of the type tables' columns
//   in table order, "ocel:activity" last when no table has it, ocel:type
//   last in the objects frame;
// - write_ocel_json and write_ocel2_json print an event time column with
//   one non-zero offset (datetime64[us, UTC+02:00]) as its wall clock with
//   "Z";
// - with objects an OCEL CSV lists but no relation names, the objects'
//   work in progress takes their lifecycle as starting and ending at 0
//   (the epoch), the endpoint columns of the events raise KeyError on the
//   first such object type by type (types sorted, each in table order),
//   and OCPN discovery raises KeyError on such a type;
// - on a relations frame without ids (a CSV whose rows name no object)
//   each feature family raises where it reads the frame, in pm4py's order
//   of families, the families that read none answer, and the event/object
//   graph raises KeyError on the missing columns;
// - filter_ocel_objects and filter_ocel_object_types at a level above 1
//   build an interaction graph with a node for every listed object (an
//   unrelated one stays a node of its own), adding the edges event by event
//   in event id order: an object the objects table does not list raises
//   KeyError there once it shares an event with another object (the first
//   in relation order), whatever was selected;
// - filter_ocel_object_types_allowed_activities on a JSON read without
//   relations (float64 relation columns) raises numpy's UFuncTypeError
//   adding its separator to the activity column, after a missing activity
//   key and before a missing type key.
//
// See docs/reference-2.7.22-parity.md, Review EP1.
#include "minera/ocel.hpp"
#include "minera/ocel_activity_type_matching.hpp"
#include "minera/ocel_csv.hpp"
#include "minera/ocel_features.hpp"
#include "minera/ocel_filter.hpp"
#include "minera/ocel_sqlite.hpp"
#include "minera/ocel_transform.hpp"
#include "minera/ocel_xml.hpp"

#include <filesystem>
#include <functional>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <tuple>
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

bool contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

std::string error_of(const std::function<void()>& run) {
  try {
    run();
  } catch (const std::exception& error) {
    return error.what();
  }
  return {};
}

OcelCsvExportOptions csv_options() {
  OcelCsvExportOptions options;
  options.allow_lossy = true;
  return options;
}

// The log as read, and after a trip through the transport (the routes'
// input).
std::vector<Ocel> both(const Ocel& log) {
  return {log, parse_ocel_json(serialize_ocel_json(log, OcelJsonFormat::kClassic, true))};
}

std::string first_line(const std::string& text) { return text.substr(0, text.find('\n')); }

// p1.py: the classic XML document of these events and objects.
const std::string kHead =
    "<?xml version='1.0' encoding='UTF-8'?>\n<log><global scope=\"event\"><string key=\"activity\" "
    "value=\"__INVALID__\"/></global><global scope=\"object\"><string key=\"type\" value=\"__INVALID__\"/>"
    "</global>\n<global scope=\"log\"><list key=\"attribute-names\"/><list key=\"object-types\"/><string "
    "key=\"version\" value=\"1.0\"/><string key=\"ordering\" value=\"timestamp\"/></global>";

std::string xml_event(const std::string& id, const std::string& activity, const std::string& time,
                      const std::vector<std::string>& objects) {
  std::string omap;
  for (const auto& object : objects)
    omap += object.starts_with("<") ? object : "<string key=\"object-id\" value=\"" + object + "\"/>";
  return "<event><string key=\"id\" value=\"" + id + "\"/><string key=\"activity\" value=\"" + activity +
         "\"/><date key=\"timestamp\" value=\"" + time + "\"/><list key=\"omap\">" + omap +
         "</list><list key=\"vmap\"></list></event>";
}

std::string xml_log(const std::string& events) {
  return kHead + "<events>" + events +
         "</events><objects><object><string key=\"id\" value=\"o1\"/><string key=\"type\" value=\"t\"/>"
         "<list key=\"ovmap\"></list></object></objects></log>";
}

// p1.py EP1-1.
void unknown_relation_objects() {
  const std::string t1 = "2024-01-01T10:00:00", t2 = "2024-01-02T10:00:00";
  const std::vector<std::pair<std::string, std::string>> cases{
      {xml_event("e1", "a", t1, {"o1", "x1"}) + xml_event("e2", "b", t2, {"x2"}), "KeyError: 'x1'"},
      {xml_event("e1", "a", t2, {"o1"}) + xml_event("e2", "b", t1, {"x2", "x3"}), "KeyError: 'x2'"},
      {xml_event("e1", "a", t1, {"<string key=\"object-id\"/>", "x1"}), "KeyError: None"},
      {xml_event("e1", "a", t1, {"x1", "<string key=\"object-id\"/>"}), "KeyError: 'x1'"},
      {xml_event("e1", "", t1, {"x1"}) + xml_event("e2", "b", t2, {"o1"}), "KeyError: 'x1'"},
      {xml_event("e1", "a", t1, {""}), "KeyError: ''"},
      {xml_event("e1", "a", t1, {"<list key=\"object-id\"><string key=\"object-id\" value=\"o1\"/></list>"}),
       "KeyError: None"},
      {xml_event("e1", "a", t1, {"<int key=\"object-id\" value=\"7\"/>"}), "KeyError: '7'"},
  };
  for (const auto& [events, message] : cases) {
    const auto error = error_of([&] { (void)parse_ocel_xml(xml_log(events), {}); });
    expect(error == message, "read_ocel_xml raises " + message + ", got " + error);
  }
}

#if defined(MINERA_HAS_SQLITE)
// A type table's columns ("name TYPE," each) in the file export_ocel_sqlite
// writes, by its type in the event or object type map.
std::string written_columns(const Ocel& log, const std::string& map, const std::string& type) {
  const auto path = std::filesystem::temp_directory_path() / "minera_review_ep1_written.sqlite";
  std::filesystem::remove(path);
  export_ocel_sqlite(log, path);
  sqlite3* db = nullptr;
  sqlite3_open(path.string().c_str(), &db);
  sqlite3_stmt* statement = nullptr;
  std::string table, out;
  sqlite3_prepare_v2(db, ("SELECT ocel_type_map FROM " + map + "_map_type WHERE ocel_type = ?1").c_str(), -1,
                     &statement, nullptr);
  sqlite3_bind_text(statement, 1, type.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(statement) == SQLITE_ROW)
    table = map + "_" + std::string(reinterpret_cast<const char*>(sqlite3_column_text(statement, 0)));
  sqlite3_finalize(statement);
  sqlite3_prepare_v2(db, ("PRAGMA table_info(\"" + table + "\")").c_str(), -1, &statement, nullptr);
  while (sqlite3_step(statement) == SQLITE_ROW)
    out += std::string(reinterpret_cast<const char*>(sqlite3_column_text(statement, 1))) + " " +
           reinterpret_cast<const char*>(sqlite3_column_text(statement, 2)) + ",";
  sqlite3_finalize(statement);
  sqlite3_close(db);
  std::filesystem::remove(path);
  return out;
}

Ocel sqlite_round_trip(const Ocel& log) {
  const auto path = std::filesystem::temp_directory_path() / "minera_review_ep1_trip.sqlite";
  std::filesystem::remove(path);
  export_ocel_sqlite(log, path);
  auto read = import_ocel_sqlite(path);
  std::filesystem::remove(path);
  return read;
}

void exec(sqlite3* db, const std::string& sql) {
  char* error = nullptr;
  if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &error) != SQLITE_OK) {
    const std::string message = error == nullptr ? "SQLite error" : error;
    sqlite3_free(error);
    throw std::runtime_error(message);
  }
}

// sq.py: two events (e1 of A, e2 of B) and o1 of t.
const std::string kSqliteBase =
    "CREATE TABLE event (ocel_id TEXT, ocel_type TEXT); CREATE TABLE event_map_type (ocel_type TEXT, "
    "ocel_type_map TEXT); CREATE TABLE object (ocel_id TEXT, ocel_type TEXT); CREATE TABLE object_map_type "
    "(ocel_type TEXT, ocel_type_map TEXT); CREATE TABLE event_object (ocel_event_id TEXT, ocel_object_id TEXT, "
    "ocel_qualifier TEXT); CREATE TABLE object_object (ocel_source_id TEXT, ocel_target_id TEXT, ocel_qualifier "
    "TEXT); INSERT INTO object VALUES('o1','t'); INSERT INTO object_map_type VALUES('t','T'); INSERT INTO event "
    "VALUES('e1','a'); INSERT INTO event VALUES('e2','b'); INSERT INTO event_map_type VALUES('a','A'); INSERT "
    "INTO event_map_type VALUES('b','B'); INSERT INTO event_object VALUES('e1','o1','q'); INSERT INTO "
    "event_object VALUES('e2','o1','q');";
const std::string kObjectTable =
    "CREATE TABLE object_T (ocel_id TEXT, p0 TEXT, ocel_time TIMESTAMP, ocel_changed_field TEXT); INSERT INTO "
    "object_T VALUES('o1','5','1970-01-01 00:00:00',NULL);";

Ocel sqlite_log(const std::string& name, const std::string& sql) {
  const auto path = std::filesystem::temp_directory_path() / ("minera_review_ep1_" + name + ".sqlite");
  std::filesystem::remove(path);
  sqlite3* db = nullptr;
  if (sqlite3_open(path.string().c_str(), &db) != SQLITE_OK) throw std::runtime_error("open the SQLite file");
  exec(db, kSqliteBase + sql);
  sqlite3_close(db);
  auto log = import_ocel_sqlite(path);
  std::filesystem::remove(path);
  return log;
}
#endif

// hc.py EP1-2: the time column first, x1 empty everywhere, x2 only on the
// dropped row, the object table's type first.
void csv_file_column_order() {
  const auto log = parse_ocel_csv(
      "ocel:timestamp,ocel:eid,ocel:activity,x0,x1,x2,ocel:type:t\n2024-02-01 10:00:00,e1,a,1,,,['o1']\n"
      "2024-02-01 11:00:00,e2,b,,,,['o1']\n2024-02-01 12:00:00,,c,,,z,['o1']\n",
      "ocel:type,ocel:oid,p0\nt,o1,5\n");
  for (const auto& read : both(log)) {
    expect(serialize_ocel_csv(read, csv_options()) ==
               "ocel:eid,ocel:timestamp,ocel:activity,x0,x1,x2,ocel:type:t\n"
               "e1,2024-02-01 10:00:00,a,1,,,['o1']\ne2,2024-02-01 11:00:00,b,,,,['o1']\n",
           "write_ocel_csv keeps the file's columns: " + serialize_ocel_csv(read, csv_options()));
    expect(serialize_ocel_objects_csv(read, csv_options()) == "ocel:type,ocel:oid,p0\nt,o1,5\n",
           "and the object table's order: " + serialize_ocel_objects_csv(read, csv_options()));
    expect(contains(serialize_ocel_json(read, OcelJsonFormat::kClassic, false),
                    "\"ocel:attribute-names\":[\"p0\",\"x0\",\"x1\",\"x2\"]"),
           "write_ocel_json lists every file column");
#if defined(MINERA_HAS_SQLITE)
    expect(written_columns(read, "event", "a") == "ocel_time TIMESTAMP,ocel_id TEXT,ocel:activity TEXT,x0 TEXT,",
           "write_ocel2_sqlite writes the event table in frame order: " + written_columns(read, "event", "a"));
    expect(written_columns(read, "object", "t") == "ocel_id TEXT,p0 TEXT,", "and the object table without the type");
    const auto back = sqlite_round_trip(read);
    expect(first_line(serialize_ocel_csv(back, csv_options())) ==
               "ocel:eid,ocel:timestamp,ocel:activity,x0,ocel:type:t",
           "read_ocel2_sqlite reads the table's order back");
    expect(serialize_ocel_objects_csv(back, csv_options()) == "ocel:oid,p0,ocel:type\no1,5,t\n",
           "with ocel:type last in its objects frame: " + serialize_ocel_objects_csv(back, csv_options()));
#endif
  }
  const auto bare = parse_ocel_csv("ocel:eid,ocel:activity,ocel:timestamp,x0,ocel:type:t\ne1,a,2024-02-01 10:00:00,1,['o1']\n");
  for (const auto& read : both(bare)) {
    expect(first_line(serialize_ocel_csv(read, csv_options())) ==
               "ocel:eid,ocel:activity,ocel:timestamp,x0,ocel:type:t",
           "the activity column before the time");
    expect(serialize_ocel_objects_csv(read, csv_options()) == "ocel:type,ocel:oid\nt,o1\n",
           "without an object table the objects frame is (ocel:type, ocel:oid): " +
               serialize_ocel_objects_csv(read, csv_options()));
  }
  const auto middle = parse_ocel_csv("ocel:eid,ocel:timestamp,ocel:activity,ocel:type:t\ne1,2024-02-01 10:00:00,a,['o1']\n",
                                     "p0,ocel:oid,ocel:type,p1\n5,o1,t,x\n");
  for (const auto& read : both(middle)) {
    expect(serialize_ocel_objects_csv(read, csv_options()) == "p0,ocel:oid,ocel:type,p1\n5,o1,t,x\n",
           "the id after an attribute: " + serialize_ocel_objects_csv(read, csv_options()));
#if defined(MINERA_HAS_SQLITE)
    expect(written_columns(read, "object", "t") == "p0 TEXT,ocel_id TEXT,p1 TEXT,",
           "write_ocel2_sqlite puts ocel_id there: " + written_columns(read, "object", "t"));
#endif
  }
}

// sq.py EP1-3: read_ocel2_sqlite's frames over odd type tables.
void sqlite_frame_order() {
#if defined(MINERA_HAS_SQLITE)
  const std::vector<std::pair<std::string, std::pair<std::string, std::string>>> cases{
      {"CREATE TABLE event_A (ocel_time TIMESTAMP, x0 TEXT, ocel_id TEXT); INSERT INTO event_A "
       "VALUES('2024-01-01 10:00:00','1','e1'); CREATE TABLE event_B (ocel_id TEXT, ocel_time TIMESTAMP, x1 TEXT); "
       "INSERT INTO event_B VALUES('e2','2024-01-01 11:00:00','2');" +
           kObjectTable,
       {"ocel:eid,ocel:timestamp,x0,x1,ocel:activity,ocel:type:t", "ocel:oid,p0,ocel:type"}},
      {"CREATE TABLE event_A (x0 TEXT, ocel_id TEXT, \"ocel:activity\" TEXT, ocel_time TIMESTAMP); INSERT INTO "
       "event_A VALUES('1','e1','a','2024-01-01 10:00:00'); CREATE TABLE event_B (ocel_time TIMESTAMP, ocel_id "
       "TEXT, \"ocel:activity\" TEXT); INSERT INTO event_B VALUES('2024-01-01 11:00:00','e2','b');" +
           kObjectTable,
       {"ocel:eid,x0,ocel:activity,ocel:timestamp,ocel:type:t", "ocel:oid,p0,ocel:type"}},
      {"CREATE TABLE event_A (ocel_id TEXT, ocel_time TIMESTAMP); INSERT INTO event_A VALUES('e1','2024-01-01 "
       "10:00:00'); CREATE TABLE event_B (ocel_id TEXT, ocel_time TIMESTAMP); INSERT INTO event_B "
       "VALUES('e2','2024-01-01 11:00:00'); CREATE TABLE object_T (p0 TEXT, ocel_time TIMESTAMP, ocel_id TEXT, "
       "ocel_changed_field TEXT, p1 TEXT); INSERT INTO object_T VALUES('5','1970-01-01 00:00:00','o1',NULL,'x');",
       {"ocel:eid,ocel:timestamp,ocel:activity,ocel:type:t", "p0,ocel:oid,p1,ocel:type"}},
  };
  std::size_t index = 0;
  for (const auto& [sql, expected] : cases) {
    const auto log = sqlite_log("frame" + std::to_string(index++), sql);
    for (const auto& read : both(log)) {
      expect(first_line(serialize_ocel_csv(read, csv_options())) == expected.first,
             "events frame " + expected.first + ", got " + first_line(serialize_ocel_csv(read, csv_options())));
      expect(first_line(serialize_ocel_objects_csv(read, csv_options())) == expected.second,
             "objects frame " + expected.second + ", got " +
                 first_line(serialize_ocel_objects_csv(read, csv_options())));
    }
  }
#endif
}

// hc.py EP1-4: one shared +02:00 offset.
void offset_event_times() {
  const auto log = parse_ocel_csv(
      "ocel:eid,ocel:timestamp,ocel:activity,ocel:type:t\ne1,2024-02-01 14:00:00+02:00,a,['o1']\n"
      "e2,2024-02-01 15:30:00+02:00,a,['o1']\n");
  for (const auto& read : both(log)) {
    const auto classic = serialize_ocel_json(read, OcelJsonFormat::kClassic, false);
    expect(contains(classic, "\"ocel:timestamp\":\"2024-02-01T14:00:00Z\"") &&
               contains(classic, "\"ocel:timestamp\":\"2024-02-01T15:30:00Z\""),
           "write_ocel_json prints the wall clock with Z: " + classic);
    const auto standard = serialize_ocel_json(read, OcelJsonFormat::kStandard, false);
    expect(contains(standard, "\"time\":\"2024-02-01T14:00:00Z\"") &&
               contains(standard, "\"time\":\"2024-02-01T15:30:00Z\""),
           "and write_ocel2_json: " + standard);
    expect(contains(serialize_ocel_csv(read, csv_options()), "e1,2024-02-01 14:00:00+02:00,a,['o1']"),
           "write_ocel_csv keeps the offset");
  }
}

OcelFeatureOptions reference_features() {
  OcelFeatureOptions options;
  options.feature_name_style = OcelFeatureNameStyle::kCompatibility;
  return options;
}

// The @@object_wip column by object id ("id=value," each).
std::string object_wip(const Ocel& log) {
  auto options = reference_features();
  options.include_all_object_features = false;
  options.include_interaction_features = false;
  options.include_degree_centrality_features = false;
  options.include_attribute_counts = false;
  options.include_lifecycle_length_features = false;
  options.include_lifecycle_unique_activity_features = false;
  options.include_lifecycle_duration_features = false;
  options.include_lifecycle_activity_features = false;
  options.include_lifecycle_wip_features = true;
  const auto table = extract_ocel_features(log, OcelFeatureVariant::kObjects, options);
  std::string out;
  for (std::size_t row = 0; row < table.row_ids.size(); ++row)
    for (std::size_t column = 0; column < table.feature_names.size(); ++column)
      if (table.feature_names[column] == "@@object_wip")
        out += table.row_ids[row] + "=" + std::to_string(static_cast<int>(table.values[row][column])) + ",";
  return out;
}

// w1.py, k1.py, k3.py EP1-5: objects listed but not related.
void unrelated_listed_objects() {
  const std::string two = "ocel:eid,ocel:timestamp,ocel:activity,ocel:type:t\ne1,2024-01-01 10:00:00,a,['o1']\n"
                          "e2,2024-01-01 11:00:00,b,['o2']\n";
  const std::string shared = "ocel:eid,ocel:timestamp,ocel:activity,ocel:type:t\ne1,2024-01-01 10:00:00,a,\"['o1','o2']\"\n"
                             "e2,2024-01-01 11:00:00,b,['o2']\n";
  const std::string early = "ocel:eid,ocel:timestamp,ocel:activity,ocel:type:t\ne1,1969-12-31 23:59:00,a,\"['o1','o2']\"\n"
                            "e2,1970-01-01 00:00:00,b,['o2']\ne3,1970-01-01 00:00:10,b,['o5']\n";
  const std::vector<std::tuple<std::string, std::string, std::string>> cases{
      {two, "ocel:oid,ocel:type\no3,t\no1,t\no4,t\no2,t\n", "o3=2,o1=1,o4=1,o2=1,"},
      {shared, "ocel:oid,ocel:type\no2,t\no3,t\no1,t\no4,t\n", "o2=1,o3=2,o1=2,o4=1,"},
      {early, "ocel:oid,ocel:type\no3,t\no2,t\no1,t\no5,t\n", "o3=1,o2=2,o1=2,o5=1,"},
  };
  for (const auto& [events, objects, expected] : cases) {
    for (const auto& read : both(parse_ocel_csv(events, objects)))
      expect(object_wip(read) == expected, "work in progress " + expected + ", got " + object_wip(read));
  }
  const std::string head = "ocel:eid,ocel:timestamp,ocel:activity,ocel:type:y,ocel:type:z\n"
                           "e1,2024-01-01 10:00:00,a,['b2'],['a2']\n";
  for (const auto& [objects, id] : std::vector<std::pair<std::string, std::string>>{
           {"ocel:oid,ocel:type\na1,z\nb2,y\na2,z\nb1,y\n", "'b1'"},
           {"ocel:oid,ocel:type\nb5,y\nb2,y\nb1,y\na2,z\n", "'b5'"}}) {
    for (const auto& read : both(parse_ocel_csv(head, objects))) {
      auto options = reference_features();
      options.include_activity_features = false;
      options.include_timestamp_features = false;
      options.include_relation_features = false;
      options.include_attribute_counts = false;
      options.include_interaction_features = false;
      const auto error = error_of([&] { (void)extract_ocel_features(read, OcelFeatureVariant::kEvents, options); });
      expect(error == "KeyError: " + id, "the endpoint columns raise KeyError: " + id + ", got " + error);
    }
  }
  const auto missing = parse_ocel_csv(
      "ocel:eid,ocel:timestamp,ocel:activity,ocel:type:m\ne1,2024-01-01 10:00:00,a,['m1']\n"
      "e2,2024-01-01 11:00:00,b,['m1']\n",
      "ocel:oid,ocel:type\nz1,z\nm1,m\nb1,b\n");
  for (const auto& read : both(missing)) {
    const auto error = error_of([&] { (void)discover_ocpn(read); });
    expect(error == "KeyError: 'b'" || error == "KeyError: 'z'", "OCPN discovery raises KeyError on a type: " + error);
  }
}

// rf.py, rf2.py EP1-6: no row names an object.
void relation_free_frames() {
  const std::string objects = "ocel:oid,ocel:type,p0\nt0,t,1\nt1,t,\n";
  const auto plain = parse_ocel_csv(
      "ocel:eid,ocel:timestamp,ocel:activity,x0,ocel:type:t\ne0,2024-02-01 06:00:01,c,5,[]\n"
      "e1,2024-02-01 06:01:01,b,,[]\n",
      objects);
  const auto nat = parse_ocel_csv(
      "ocel:eid,ocel:timestamp,ocel:activity,x0,ocel:type:t\ne0,2024-02-01 06:00:01,c,5,[]\ne2,NaT,b,,[]\n", objects);
  const auto events_error = [](const Ocel& log, const OcelFeatureOptions& options) {
    return error_of([&] { (void)extract_ocel_features(log, OcelFeatureVariant::kEvents, options); });
  };
  const auto objects_error = [](const Ocel& log, const OcelFeatureOptions& options) {
    return error_of([&] { (void)extract_ocel_features(log, OcelFeatureVariant::kObjects, options); });
  };
  auto only_timestamps = reference_features();
  only_timestamps.include_activity_features = false;
  only_timestamps.include_relation_features = false;
  only_timestamps.include_endpoint_features = false;
  only_timestamps.include_attribute_counts = false;
  only_timestamps.include_interaction_features = false;
  auto only_endpoints = only_timestamps;
  only_endpoints.include_timestamp_features = false;
  only_endpoints.include_endpoint_features = true;
  auto cobirth = reference_features();
  cobirth.include_all_object_features = false;
  cobirth.include_lifecycle_features = false;
  cobirth.include_interaction_features = false;
  cobirth.include_degree_centrality_features = false;
  cobirth.include_attribute_counts = false;
  cobirth.include_cobirth_features = true;
  for (const auto& read : both(plain)) {
    expect(events_error(read, only_timestamps).empty(), "the timestamp columns read no relation: " +
                                                           events_error(read, only_timestamps));
    expect(events_error(read, only_endpoints) == "KeyError: 'ocel:oid'", "the endpoint columns raise on the object id");
    expect(events_error(read, reference_features()) == "KeyError: 'ocel:eid'", "the default families on the event id");
    expect(objects_error(read, reference_features()) == "KeyError: 'ocel:oid'", "the objects' on the object id");
    expect(objects_error(read, cobirth) == "KeyError: 'ocel:eid'", "the cobirth graph on the event id");
    const auto graph = error_of([&] { (void)discover_ocel_heterogeneous_graph(read, {}); });
    expect(graph == "KeyError: \"['ocel:eid', 'ocel:oid'] not in index\"", "the event/object graph raises: " + graph);
  }
  for (const auto& read : both(nat)) {
    expect(events_error(read, reference_features()) == "ValueError: NaTType does not support timestamp",
           "a NaT time raises first: " + events_error(read, reference_features()));
    expect(objects_error(read, cobirth) == "KeyError: 'ocel:eid'", "the cobirth graph reads no time");
  }
}

// lv/lv.py, lv2.py, lv3.py, lv4.py EP1-7: deeper object filters.
std::string kept(const Ocel& log) {
  std::string text;
  for (const auto& object : log.objects) text += object.id + ",";
  text += "|";
  for (const auto& event : log.events) text += event.id + ",";
  return text;
}

void deeper_object_filters() {
  const auto listed = parse_ocel_csv(
      "ocel:eid,ocel:timestamp,ocel:activity,ocel:type:t,ocel:type:u,ocel:type:w\n"
      "e1,2024-01-01 10:00:00,a,['t1'],[],\"['w0', 'w1']\"\n"
      "e2,2024-01-01 11:00:00,b,[],['u0'],[]\n",
      "ocel:oid,ocel:type\nw0,w\nu0,u\nt1,t\nt0,t\nw1,w\nz9,w\n");
  for (const auto& read : both(listed)) {
    for (const std::size_t level : {std::size_t{2}, std::size_t{3}}) {
      const auto types = kept(filter_ocel_object_types(read, {"t"}, true, level));
      expect(types == "w0,t1,t0,w1,|e1,", "an unrelated object of the type stays: " + types);
      const auto other = kept(filter_ocel_object_types(read, {"t"}, false, level));
      expect(other == "u0,z9,|e2,", "and leaves the complement: " + other);
      const auto alone = kept(filter_ocel_objects(read, {"t0"}, true, level));
      expect(alone == "t0,|", "an unrelated listed id is a node of its own: " + alone);
      const auto rest = kept(filter_ocel_objects(read, {"t0"}, false, level));
      expect(rest == "w0,u0,t1,w1,z9,|e1,e2,", "its complement: " + rest);
      const auto none = kept(filter_ocel_object_types(read, {"q"}, true, level));
      expect(none == "|", "no object of the type: " + none);
      const auto missing = error_of([&] { (void)filter_ocel_objects(read, {"nope"}, true, level); });
      expect(missing == "KeyError: 'nope'", "an id the table does not list still raises: " + missing);
    }
  }
  const std::vector<std::pair<std::string, std::string>> unlisted{
      {"e1,2024-01-01 10:00:00,a,['t1'],['r6'],[]\ne2,2024-01-01 11:00:00,b,[],['r5'],['w0']\n", "'r6'"},
      {"e1,2024-01-01 10:00:00,a,['t1'],\"['r5', 'r6']\",[]\n", "'r5'"},
      {"e1,2024-01-01 10:00:00,a,[],\"['r6', 'r6']\",['w0']\ne2,2024-01-01 10:00:00,a,['t1'],['r5'],[]\n", "'r6'"},
      {"e2,2024-01-01 10:00:00,a,['t1'],['q2'],[]\ne10,2024-01-01 11:00:00,b,[],['q1'],['w0']\n", "'q1'"},
  };
  // Native's CSV reader lists the objects the events name (declared), so the
  // frame pm4py reads is modelled by leaving them out of the objects.
  const auto drop_unlisted = [](Ocel log) {
    std::erase_if(log.objects, [](const OcelObject& object) { return object.type == "r"; });
    return log;
  };
  for (const auto& [rows, id] : unlisted) {
    const auto log = drop_unlisted(parse_ocel_csv(
        "ocel:eid,ocel:timestamp,ocel:activity,ocel:type:t,ocel:type:r,ocel:type:w\n" + rows,
        "ocel:oid,ocel:type\nw0,w\nt1,t\n"));
    {
      const auto& read = log;
      for (const auto& run : std::vector<std::function<void()>>{
               [&] { (void)filter_ocel_objects(read, {"t1"}, true, 2); },
               [&] { (void)filter_ocel_objects(read, {"nope"}, false, 2); },
               [&] { (void)filter_ocel_objects(read, {}, true, 2); },
               [&] { (void)filter_ocel_object_types(read, {"q"}, true, 2); }}) {
        const auto error = error_of(run);
        expect(error == "KeyError: " + id, "an unlisted object in a shared event raises " + id + ", got " + error);
      }
      expect(error_of([&] { (void)filter_ocel_objects(read, {"t1"}, true, 1); }).empty(), "level 1 builds no graph");
    }
  }
  const auto lone = drop_unlisted(parse_ocel_csv(
      "ocel:eid,ocel:timestamp,ocel:activity,ocel:type:t,ocel:type:r\n"
      "e1,2024-01-01 10:00:00,a,['t1'],[]\ne2,2024-01-01 11:00:00,b,[],\"['q2', 'q2']\"\n",
      "ocel:oid,ocel:type\nt1,t\n"));
  {
    const auto& read = lone;
    const auto alone = kept(filter_ocel_objects(read, {"t1"}, true, 2));
    expect(alone == "t1,|e1,", "an unlisted object alone in its event adds no edge: " + alone);
    const auto error = error_of([&] { (void)filter_ocel_objects(read, {"q2"}, true, 2); });
    expect(error == "KeyError: 'q2'", "but selecting it raises: " + error);
  }
}

// lv/aa.py, ac.py, ad.py, ae.py EP1-8: c56_0010 and the like.
void relation_free_type_matching() {
  const std::string classic =
      "{\"ocel:global-event\":{\"ocel:activity\":\"__INVALID__\"},\"ocel:global-object\":{\"ocel:type\":\"__INVALID__\"},"
      "\"ocel:global-log\":{\"ocel:attribute-names\":[],\"ocel:object-types\":[\"t\"],\"ocel:version\":\"1.0\","
      "\"ocel:ordering\":\"timestamp\"},\"ocel:events\":{\"e0\":{\"ocel:activity\":\"pay\","
      "\"ocel:timestamp\":\"2024-02-06T02:22:46Z\",\"ocel:omap\":[],\"ocel:vmap\":{}}},"
      "\"ocel:objects\":{\"o0\":{\"ocel:type\":\"t\",\"ocel:ovmap\":{}}}}";
  const std::string ufunc = "UFuncTypeError: ufunc 'add' did not contain a loop with signature matching types "
                            "(dtype('float64'), dtype('<U";
  const auto read = parse_ocel_json(classic);
  for (const auto& log : both(read)) {
    for (const auto& allowed : std::vector<std::map<std::string, std::vector<std::string>>>{{{"t", {"a"}}}, {}}) {
      const auto error = error_of([&] { (void)filter_ocel_activity_type_matching(log, allowed); });
      expect(error == ufunc + "4')) -> None", "the allowed activities raise on float64 relations: " + error);
    }
    const auto exact = [&](OcelActivityTypeMatchingOptions options) {
      return error_of([&] { (void)filter_ocel_activity_type_matching_exact(log, {{"t", {"a"}}}, options); });
    };
    OcelActivityTypeMatchingOptions options;
    expect(exact(options) == ufunc + "4')) -> None", "the default separator: " + exact(options));
    for (const auto& [separator, length] : std::vector<std::pair<std::string, std::string>>{
             {"x", "1"}, {"", "1"}, {"xxxxxx", "6"}, {std::string("x\0", 2), "2"}, {"\xc3\xa4\xc3\xb6", "2"}}) {
      options.temporary_separator = separator;
      expect(exact(options) == ufunc + length + "')) -> None", "a separator of " + length + ": " + exact(options));
    }
    options = {};
    options.object_type_key = "bar";
    expect(exact(options) == ufunc + "4')) -> None", "before a missing type key: " + exact(options));
    options.activity_key = "foo";
    expect(exact(options) == "'foo'", "after a missing activity key: " + exact(options));
  }
  const auto typed = parse_ocel_csv("ocel:eid,ocel:timestamp,ocel:activity,ocel:type:t\ne1,2024-01-01 10:00:00,a,['t1']\n",
                                    "ocel:oid,ocel:type\nt1,t\n");
  OcelActivityTypeMatchingOptions both_missing;
  both_missing.activity_key = "foo";
  both_missing.object_type_key = "bar";
  const auto error = error_of([&] { (void)filter_ocel_activity_type_matching_exact(typed, {{"t", {"a"}}}, both_missing); });
  expect(error == "'foo'", "the activity key is read first: " + error);
}

}  // namespace

int main() {
  unknown_relation_objects();
  csv_file_column_order();
  sqlite_frame_order();
  offset_event_times();
  unrelated_listed_objects();
  relation_free_frames();
  deeper_object_filters();
  relation_free_type_matching();
  if (failures != 0) {
    std::cerr << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "review EP1 tests passed\n";
  return 0;
}
