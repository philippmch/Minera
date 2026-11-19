// Review ER1: pm4py 2.7.22's read_ocel2_sqlite on hand-built SQLite files
// carrying native's tables and unusual type tables, its writers on the
// frames they give, and the OCPN reference playout's time limit
// (review-harness round-er: hc/er1sql.py runs these files through pm4py).
// Observed black-box:
//
// - views and WITHOUT ROWID tables read like tables; a type table may name
//   its id and time columns "ocel:eid"/"ocel:oid" and "ocel:timestamp";
// - a missing column raises KeyError in the reader's order: the event and
//   object tables' ocel_id, the map tables' columns, a type table's id
//   ('ocel:eid', 'ocel:oid'), "['ocel:timestamp'] not in index" when no
//   event table has a time column, an object_object row without its
//   columns (an empty table needs none);
// - an event table's ocel_changed_field, "@@cumcount" and "ocel:" columns
//   are attributes, an object table's "ocel:type" is not (the object keeps
//   its type) and its "ocel:activity" is; a BOOLEAN column reads as stored;
//   write_ocel2_sqlite writes them back as columns, write_ocel_json writes
//   an event's "ocel:" attribute as a key of the event entry and leaves an
//   object's out; write_ocel_xml writes an event's id, activity, timestamp
//   and unprefixed "ocel:" attributes in the events frame's column order;
// - an object table without a time column keeps its change rows with a NaT
//   time when another object table has one (write_ocel2_json then raises
//   KeyError 'ocel:timestamp'); with no time column anywhere the reader
//   raises KeyError 'ocel:timestamp';
// - a listed type's table without rows adds its columns to the frames as
//   object columns (a column it lacks holds NaN there) and makes the
//   events' time column object.
//
// See docs/reference-2.7.22-parity.md, Review ER1.
#include "minera/ocel.hpp"
#include "minera/ocel_csv.hpp"
#include "minera/ocel_sqlite.hpp"
#include "minera/ocel_xml.hpp"
#include "minera/ocpn_reference_playout.hpp"

#include <algorithm>
#include <filesystem>
#include <functional>
#include <iostream>
#include <map>
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
  } catch (const std::exception& error) {
    return error.what();
  }
  return {};
}

#if defined(MINERA_HAS_SQLITE)

// The tables of a small pm4py-shaped file, by name; a case replaces some.
std::map<std::string, std::string> base_tables() {
  return {
      {"event", "CREATE TABLE event (ocel_id TEXT, ocel_type TEXT); INSERT INTO event VALUES ('e1','a'),('e2','a');"},
      {"event_map_type",
       "CREATE TABLE event_map_type (ocel_type TEXT, ocel_type_map TEXT); INSERT INTO event_map_type VALUES ('a','A');"},
      {"object", "CREATE TABLE object (ocel_id TEXT, ocel_type TEXT); INSERT INTO object VALUES ('o1','t'),('o2','t');"},
      {"object_map_type",
       "CREATE TABLE object_map_type (ocel_type TEXT, ocel_type_map TEXT); INSERT INTO object_map_type VALUES ('t','T');"},
      {"event_object",
       "CREATE TABLE event_object (ocel_event_id TEXT, ocel_object_id TEXT, ocel_qualifier TEXT); "
       "INSERT INTO event_object VALUES ('e1','o1','q'),('e2','o1','q'),('e2','o2','r');"},
      {"object_object",
       "CREATE TABLE object_object (ocel_source_id TEXT, ocel_target_id TEXT, ocel_qualifier TEXT); "
       "INSERT INTO object_object VALUES ('o1','o2','p');"},
      {"event_A",
       "CREATE TABLE event_A (ocel_id TEXT, ocel_time TIMESTAMP, \"ocel:activity\" TEXT, x INTEGER); "
       "INSERT INTO event_A VALUES ('e1','2024-01-01 10:00:00','a',1),('e2','2024-01-02 10:00:00','a',2);"},
      {"object_T",
       "CREATE TABLE object_T (ocel_id TEXT, y REAL, ocel_time TIMESTAMP, ocel_changed_field TEXT); "
       "INSERT INTO object_T VALUES ('o1',1.5,NULL,NULL),('o2',2.5,NULL,NULL),('o1',3.5,'2024-01-03 10:00:00','y');"},
  };
}

// A second object type u (object o3, related to e2).
std::map<std::string, std::string> two_types() {
  return {
      {"object",
       "CREATE TABLE object (ocel_id TEXT, ocel_type TEXT); INSERT INTO object VALUES ('o1','t'),('o2','t'),('o3','u');"},
      {"object_map_type", "CREATE TABLE object_map_type (ocel_type TEXT, ocel_type_map TEXT); "
                          "INSERT INTO object_map_type VALUES ('t','T'),('u','U');"},
      {"event_object", "CREATE TABLE event_object (ocel_event_id TEXT, ocel_object_id TEXT, ocel_qualifier TEXT); "
                       "INSERT INTO event_object VALUES ('e1','o1','q'),('e2','o1','q'),('e2','o2','r'),('e2','o3','s');"},
  };
}

void exec_sql(sqlite3* db, const std::string& sql) {
  char* error = nullptr;
  if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &error) != SQLITE_OK) {
    const std::string message = error != nullptr ? error : "unknown";
    sqlite3_free(error);
    throw std::runtime_error("fixture SQL failed: " + message);
  }
}

std::filesystem::path build(const std::string& name, const std::map<std::string, std::string>& replaced) {
  auto tables = base_tables();
  for (const auto& [table, sql] : replaced) tables[table] = sql;
  const auto path = std::filesystem::temp_directory_path() / ("minera_review_er1_" + name + ".sqlite");
  std::filesystem::remove(path);
  sqlite3* db = nullptr;
  if (sqlite3_open(path.string().c_str(), &db) != SQLITE_OK) throw std::runtime_error("cannot open fixture");
  for (const auto& [table, sql] : tables) exec_sql(db, sql);
  sqlite3_close(db);
  return path;
}

// Imports a case (and removes its file); the error text when it raises.
std::string read_error(const std::string& name, const std::map<std::string, std::string>& replaced) {
  const auto path = build(name, replaced);
  const auto error = error_of([&] { (void)import_ocel_sqlite(path); });
  std::filesystem::remove(path);
  return error;
}

Ocel read_case(const std::string& name, const std::map<std::string, std::string>& replaced) {
  const auto path = build(name, replaced);
  auto log = import_ocel_sqlite(path);
  std::filesystem::remove(path);
  return log;
}

OcelValue value(const std::vector<OcelAttribute>& attributes, const std::string& key) {
  for (const auto& attribute : attributes)
    if (attribute.key == key) return attribute.value;
  return nullptr;
}

bool has(const std::vector<OcelAttribute>& attributes, const std::string& key) {
  for (const auto& attribute : attributes)
    if (attribute.key == key) return true;
  return false;
}

// The plain file as pm4py reads it.
void expect_plain(const Ocel& log, const std::string& what) {
  expect(log.events.size() == 2 && log.objects.size() == 2, what + ": two events and two objects");
  if (log.events.size() != 2 || log.objects.size() != 2) return;
  expect(log.events[0].id == "e1" && value(log.events[0].attributes, "x") == OcelValue(std::int64_t{1}) &&
             log.events[1].id == "e2",
         what + ": the events in table order");
  expect(log.relations.size() == 3 && log.o2o.size() == 1, what + ": three relations and one object link");
  expect(log.object_changes.size() == 1 && log.object_changes[0].object_id == "o1" &&
             log.object_changes[0].field == "y",
         what + ": the change of o1");
}

// The column names of the type table that `kind`_map_type maps `type` to.
std::vector<std::string> exported_columns(const std::filesystem::path& path, const std::string& kind,
                                          const std::string& type) {
  sqlite3* db = nullptr;
  std::vector<std::string> names;
  if (sqlite3_open(path.string().c_str(), &db) != SQLITE_OK) return names;
  std::string token;
  sqlite3_stmt* statement = nullptr;
  const auto map_sql = "SELECT ocel_type_map FROM " + kind + "_map_type WHERE ocel_type = ?1";
  if (sqlite3_prepare_v2(db, map_sql.c_str(), -1, &statement, nullptr) == SQLITE_OK) {
    sqlite3_bind_text(statement, 1, type.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(statement) == SQLITE_ROW)
      token = reinterpret_cast<const char*>(sqlite3_column_text(statement, 0));
  }
  sqlite3_finalize(statement);
  const auto info_sql = "PRAGMA table_info(\"" + kind + "_" + token + "\")";
  if (sqlite3_prepare_v2(db, info_sql.c_str(), -1, &statement, nullptr) == SQLITE_OK)
    while (sqlite3_step(statement) == SQLITE_ROW)
      names.emplace_back(reinterpret_cast<const char*>(sqlite3_column_text(statement, 1)));
  sqlite3_finalize(statement);
  sqlite3_close(db);
  return names;
}

bool contains(const std::vector<std::string>& names, const std::string& name) {
  for (const auto& candidate : names)
    if (candidate == name) return true;
  return false;
}

#endif

// ER1-1: views, WITHOUT ROWID tables and the renamed id and time columns.
void tables_and_names() {
#if defined(MINERA_HAS_SQLITE)
  expect_plain(read_case("plain", {}), "the plain file");
  expect_plain(
      read_case("views",
                {{"event", "CREATE TABLE event_base (ocel_id TEXT, ocel_type TEXT); "
                           "INSERT INTO event_base VALUES ('e1','a'),('e2','a'); "
                           "CREATE VIEW event AS SELECT * FROM event_base;"},
                 {"event_object", "CREATE TABLE eo_base (ocel_event_id TEXT, ocel_object_id TEXT, ocel_qualifier TEXT); "
                                  "INSERT INTO eo_base VALUES ('e1','o1','q'),('e2','o1','q'),('e2','o2','r'); "
                                  "CREATE VIEW event_object AS SELECT * FROM eo_base;"},
                 {"event_A", "CREATE TABLE event_A (ocel_id TEXT PRIMARY KEY, ocel_time TIMESTAMP, \"ocel:activity\" TEXT, "
                             "x INTEGER) WITHOUT ROWID; INSERT INTO event_A VALUES "
                             "('e2','2024-01-02 10:00:00','a',2),('e1','2024-01-01 10:00:00','a',1);"}}),
      "views and a WITHOUT ROWID table");
  expect_plain(read_case("aliases", {{"event_A", "CREATE TABLE event_A (\"ocel:eid\" TEXT, \"ocel:timestamp\" TIMESTAMP, "
                                                 "\"ocel:activity\" TEXT, x INTEGER); INSERT INTO event_A VALUES "
                                                 "('e1','2024-01-01 10:00:00','a',1),('e2','2024-01-02 10:00:00','a',2);"}}),
               "a type table naming its columns ocel:eid and ocel:timestamp");
  const auto empty_o2o = read_case("o2o_empty", {{"object_object", "CREATE TABLE object_object (zz TEXT);"}});
  expect(empty_o2o.events.size() == 2 && empty_o2o.o2o.empty(), "an empty object_object table needs no columns");
#endif
}

// ER1-2: the reader's KeyError order on missing columns.
void missing_columns() {
#if defined(MINERA_HAS_SQLITE)
  auto error = read_error("noid_event", {{"event", "CREATE TABLE event (zz TEXT, ocel_type TEXT); "
                                                   "INSERT INTO event VALUES ('e1','a'),('e2','a');"},
                                         {"object", "CREATE TABLE object (zz TEXT, ocel_type TEXT); "
                                                    "INSERT INTO object VALUES ('o1','t'),('o2','t');"}});
  expect(error == "KeyError: 'ocel_id'", "the event table's id first: " + error);
  error = read_error("noid_etype", {{"event_A", "CREATE TABLE event_A (zz TEXT, ocel_time TIMESTAMP, \"ocel:activity\" TEXT, "
                                                "x INTEGER); INSERT INTO event_A VALUES ('e1','2024-01-01 10:00:00','a',1);"}});
  expect(error == "KeyError: 'ocel:eid'", "an event type table without its id: " + error);
  error = read_error("noid_otype",
                     {{"object_T", "CREATE TABLE object_T (zz TEXT, y REAL); INSERT INTO object_T VALUES ('o1',1.5);"}});
  expect(error == "KeyError: 'ocel:oid'", "an object type table without its id: " + error);
  error = read_error("nomap_col", {{"event_map_type", "CREATE TABLE event_map_type (ocel_type TEXT, zz TEXT); "
                                                      "INSERT INTO event_map_type VALUES ('a','A');"}});
  expect(error == "KeyError: 'ocel_type_map'", "a map table without its token column: " + error);
  error = read_error("notime_events", {{"event_A", "CREATE TABLE event_A (ocel_id TEXT, \"ocel:activity\" TEXT, x INTEGER); "
                                                   "INSERT INTO event_A VALUES ('e1','a',1),('e2','a',2);"}});
  expect(error == "KeyError: \"['ocel:timestamp'] not in index\"", "no event table with a time column: " + error);
  error = read_error("o2o_row", {{"object_object", "CREATE TABLE object_object (zz TEXT); "
                                                   "INSERT INTO object_object VALUES ('x');"}});
  expect(error == "KeyError: 'ocel:oid'", "an object link row without its columns: " + error);
#endif
}

// ER1-3: reserved-looking columns, read and written back.
void reserved_columns() {
#if defined(MINERA_HAS_SQLITE)
  const auto log = read_case(
      "reserved",
      {{"event_A", "CREATE TABLE event_A (ocel_id TEXT, ocel_time TIMESTAMP, \"ocel:activity\" TEXT, x INTEGER, "
                   "ocel_changed_field TEXT, \"@@cumcount\" INTEGER, \"ocel:foo\" TEXT, z BOOLEAN); "
                   "INSERT INTO event_A VALUES ('e1','2024-01-01 10:00:00','a',1,'f',5,'w',1),"
                   "('e2','2024-01-02 10:00:00','a',2,NULL,NULL,NULL,0);"},
       {"object_T", "CREATE TABLE object_T (ocel_id TEXT, y REAL, \"ocel:type\" TEXT, \"ocel:activity\" TEXT); "
                    "INSERT INTO object_T VALUES ('o1',1.5,'zz',NULL),('o2',2.5,'zz','act');"}});
  expect(log.events.size() == 2 && log.objects.size() == 2, "two events and two objects");
  if (log.events.size() != 2 || log.objects.size() != 2) return;
  const auto& first = log.events[0].attributes;
  expect(value(first, "ocel_changed_field") == OcelValue(std::string("f")), "an event's ocel_changed_field is an attribute");
  expect(value(first, kOcelCumcountColumn) == OcelValue(5.0), "and its \"@@cumcount\" (float64 next to a NULL)");
  expect(value(first, "ocel:foo") == OcelValue(std::string("w")), "and its \"ocel:\" column");
  expect(value(first, "z") == OcelValue(std::int64_t{1}) &&
             value(log.events[1].attributes, "z") == OcelValue(std::int64_t{0}),
         "a BOOLEAN column reads as stored");
  expect(log.objects[0].type == "t" && log.objects[1].type == "t" && !has(log.objects[1].attributes, "ocel:type"),
         "an object table's ocel:type is no attribute");
  expect(value(log.objects[1].attributes, "ocel:activity") == OcelValue(std::string("act")) &&
             !has(log.objects[0].attributes, "ocel:activity"),
         "an object table's ocel:activity is");
  for (const auto& read : {log, parse_ocel_json(serialize_ocel_json(log, OcelJsonFormat::kClassic, true))}) {
    const auto path = std::filesystem::temp_directory_path() / "minera_review_er1_reserved_out.sqlite";
    std::filesystem::remove(path);
    export_ocel_sqlite(read, path);
    const auto events = exported_columns(path, "event", "a");
    const auto objects = exported_columns(path, "object", "t");
    std::filesystem::remove(path);
    expect(contains(events, "ocel_changed_field") && contains(events, kOcelCumcountColumn) && contains(events, "ocel:foo") &&
               contains(events, "z"),
           "write_ocel2_sqlite writes the event columns back");
    expect(contains(objects, "ocel:activity") && !contains(objects, "ocel:type"), "and the object's ocel:activity");
  }
  const auto text = serialize_ocel_json(log, OcelJsonFormat::kClassic, false);
  expect(text.find("\"ocel:foo\":\"w\",\"ocel:omap\"") != std::string::npos ||
             text.find("\"ocel:foo\":\"w\",\"ocel:vmap\"") != std::string::npos,
         "write_ocel_json writes an event's \"ocel:\" attribute beside its maps");
  expect(text.find("\"ocel:vmap\":{\"x\":1,\"ocel_changed_field\":\"f\",\"@@cumcount\":5.0,\"z\":1}") != std::string::npos,
         "and leaves it out of the vmap: " + text.substr(0, 400));
  expect(text.find("\"ocel:ovmap\":{\"y\":2.5}") != std::string::npos, "and an object's out of the ovmap");
#endif
}

// ER1-6: write_ocel_xml's event fields in the frame's column order (a CSV
// with an "ocel:" column before its time column).
void classic_xml_field_order() {
  const auto log = parse_ocel_csv(
      "ocel:eid,ocel:foo,ocel:timestamp,ocel:activity,x,ocel:type:t\n"
      "e1,w,2024-01-01 10:00:00,a,1,\"['o1']\"\n"
      "e2,,2024-01-02 10:00:00,b,2,\"['o1']\"\n",
      "ocel:oid,ocel:type\no1,t\n");
  for (const auto& read : {log, parse_ocel_json(serialize_ocel_json(log, OcelJsonFormat::kClassic, true))}) {
    OcelXmlExportOptions options;
    options.allow_lossy = true;
    const auto xml = serialize_ocel_xml(read, options);
    expect(xml.find("<string key=\"id\" value=\"e1\"/>\n      <string key=\"foo\" value=\"w\"/>\n"
                    "      <date key=\"timestamp\" value=\"2024-01-01T10:00:00Z\"/>\n"
                    "      <string key=\"activity\" value=\"a\"/>") != std::string::npos,
           "id, foo, timestamp, activity as the frame orders them");
    expect(xml.find("<string key=\"id\" value=\"e2\"/>\n      <date key=\"timestamp\"") != std::string::npos,
           "an event without the attribute skips it");
  }
}

// ER1-4: change rows of an object table without a time column.
void untimed_changes() {
#if defined(MINERA_HAS_SQLITE)
  auto tables = two_types();
  tables["object_T"] = "CREATE TABLE object_T (ocel_id TEXT, y REAL, ocel_changed_field TEXT); "
                       "INSERT INTO object_T VALUES ('o1',1.5,NULL),('o2',2.5,NULL),('o1',3.5,'y');";
  tables["object_U"] = "CREATE TABLE object_U (ocel_id TEXT, w REAL, ocel_time TIMESTAMP); "
                       "INSERT INTO object_U VALUES ('o3',7.0,NULL);";
  const auto log = read_case("untimed_change", tables);
  expect(log.objects.size() == 3 && log.object_changes.size() == 1 && log.object_changes[0].object_id == "o1" &&
             log.object_changes[0].field == "y",
         "the change is kept");
  for (const auto& read : {log, parse_ocel_json(serialize_ocel_json(log, OcelJsonFormat::kClassic, true))}) {
    const auto error = error_of([&] { (void)serialize_ocel_json(read, OcelJsonFormat::kStandard, false); });
    expect(error == "KeyError: 'ocel:timestamp'", "with a NaT time write_ocel2_json raises on: " + error);
  }
  tables["object_U"] = "CREATE TABLE object_U (ocel_id TEXT, w REAL); INSERT INTO object_U VALUES ('o3',7.0);";
  const auto error = read_error("untimed_nowhere", tables);
  expect(error == "KeyError: 'ocel:timestamp'", "no object table with a time column: " + error);
#endif
}

// ER1-9: a listed type whose type table has no rows: its columns join the
// frames as object columns, a column it lacks holds NaN (an int one is
// float64), and its time column makes the events' one object (numbers are
// then epoch nanoseconds).
void empty_type_tables() {
#if defined(MINERA_HAS_SQLITE)
  auto tables = two_types();
  tables["event"] = "CREATE TABLE event (ocel_id TEXT, ocel_type TEXT); INSERT INTO event VALUES ('e1','a'),('e2','a'),('e3','b');";
  tables["event_map_type"] = "CREATE TABLE event_map_type (ocel_type TEXT, ocel_type_map TEXT); "
                             "INSERT INTO event_map_type VALUES ('a','A'),('b','B');";
  tables["event_B"] = "CREATE TABLE event_B (ocel_id TEXT, ocel_time TIMESTAMP, \"ocel:activity\" TEXT, zz TEXT);";
  tables["object_U"] = "CREATE TABLE object_U (ocel_id TEXT, w REAL, ocel_time TIMESTAMP, ocel_changed_field TEXT);";
  const auto log = read_case("empty_tables", tables);
  expect(log.events.size() == 2 && log.objects.size() == 2, "the listed event and object without rows are not read");
  OcelCsvExportOptions csv;
  csv.allow_lossy = true;
  for (const auto& read : {log, parse_ocel_json(serialize_ocel_json(log, OcelJsonFormat::kClassic, true))}) {
    const auto events = serialize_ocel_csv(read, csv);
    expect(events.rfind("ocel:eid,ocel:timestamp,ocel:activity,x,zz,ocel:type:t\n"
                        "e1,2024-01-01 10:00:00+00:00,a,1.0,,", 0) == 0,
           "the empty table's column joins the events frame, x is float64: " + events.substr(0, 120));
    const auto objects = serialize_ocel_objects_csv(read, csv);
    expect(objects.rfind("ocel:oid,y,w,ocel:type\n", 0) == 0, "and the objects frame: " + objects.substr(0, 60));
    const auto standard = serialize_ocel_json(read, OcelJsonFormat::kStandard, false);
    expect(standard.find("\"eventTypes\":[{\"name\":\"a\",\"attributes\":[{\"name\":\"x\",\"type\":\"float\"}]}]") !=
               std::string::npos,
           "write_ocel2_json declares x a float and no type b");
  }
  tables["event_A"] = "CREATE TABLE event_A (ocel_id TEXT, ocel_time TIMESTAMP, \"ocel:activity\" TEXT, x INTEGER); "
                      "INSERT INTO event_A VALUES ('e1',15,'a',1),('e2',16,'a',2);";
  const auto numbers = read_case("empty_tables_numbers", tables);
  const auto events = serialize_ocel_csv(numbers, csv);
  expect(events.find("\ne1,1970-01-01 00:00:00.000000015+00:00,a,1.0,,") != std::string::npos,
         "integer times next to an empty table's time column are epoch nanoseconds: " + events.substr(0, 160));
#endif
}

// ER1-5: the OCPN reference playout's time limit.
void ocpn_time_limit() {
  // Every order of six activities, one object each: the inductive miner's
  // net runs them in parallel (720 firing sequences).
  std::string activities = "abcdef";
  std::string objects, events;
  std::size_t object = 0, event = 0;
  do {
    const auto id = "o" + std::to_string(object++);
    objects += (objects.empty() ? "" : ",") + ("\"" + id + "\":{\"ocel:type\":\"t\",\"ocel:ovmap\":{}}");
    for (const char activity : activities) {
      const auto two = [](std::size_t n) { return std::string(n < 10 ? "0" : "") + std::to_string(n); };
      const auto time = "2024-01-01T" + two(event / 3600) + ":" + two(event / 60 % 60) + ":" + two(event % 60) + "Z";
      events += (events.empty() ? "" : ",") + ("\"e" + std::to_string(event++) + "\":{\"ocel:activity\":\"" +
                                               std::string(1, activity) + "\",\"ocel:timestamp\":\"" + time + "\",\"ocel:omap\":[\"" + id +
                                               "\"],\"ocel:vmap\":{}}");
    }
  } while (std::next_permutation(activities.begin(), activities.end()));
  const auto log = parse_ocel_json("{\"ocel:global-event\":{},\"ocel:global-object\":{},\"ocel:global-log\":{},"
                                   "\"ocel:objects\":{" + objects + "},\"ocel:events\":{" + events + "}}");
  const auto graph = discover_ocpn(log);
  OcpnReferencePlayoutOptions options;
  const auto all = reference_ocpn_extensive_visit(graph, options, [](const OcpnReferenceTrace&) { return true; });
  expect(all > 256, "the net has many firing sequences: " + std::to_string(all));
  options.time_limit_seconds = 1e-9;
  const auto error = error_of([&] {
    (void)reference_ocpn_extensive_visit(graph, options, [](const OcpnReferenceTrace&) { return true; });
  });
  expect(error.find("exceeded its") != std::string::npos, "a search past its time limit stops: " + error);
}

}  // namespace

int main() {
  tables_and_names();
  missing_columns();
  reserved_columns();
  classic_xml_field_order();
  untimed_changes();
  empty_type_tables();
  ocpn_time_limit();
  if (failures != 0) {
    std::cerr << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "review ER1 tests passed\n";
  return 0;
}
