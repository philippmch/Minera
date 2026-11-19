// Review EN1: pm4py 2.7.22 on round EK's and EM's spoiled SQLite banks, a
// new spoiled OCEL 2.0 XML bank and hand cases (review-harness round-en:
// en/hc/hc.py, en/hc/nn2.py, en/cc/cc.py, en/fh/fh.py, en/xp/xt.py,
// en/jp/jt.py). Observed black-box:
//
// - read_ocel2_sqlite keeps a change row's other non-null cells in its
//   changes frame, and a "@@cumcount" column (the row's place among its
//   object's rows over the object tables in type order); write_ocel_json
//   prints them in the change entries, write_ocel2_json and write_ocel2_xml
//   declare "@@cumcount" a "string" attribute, and write_ocel2_sqlite
//   writes them back;
// - write_ocel2_sqlite writes a type's table from its object rows and its
//   change rows with their empty columns dropped, concatenated: the object
//   columns in the objects frame's order, then the change columns in the
//   changes frame's order, typed by the concatenation;
// - read_ocel2_sqlite's frames take their columns from the type tables
//   with rows, in type order (an all-NULL column too); read_ocel2_json
//   builds its events frame in file order before it sorts the events;
// - read_sql reads a NULL of an object column (texts next to numbers, or
//   NULLs only) as None, which OC-DFG's pair names `NoneType`;
// - read_ocel2_xml and read_ocel2_json keep a change time
//   "1970-01-01T00:00:00..." or "0" as a str (write_ocel2_xml then raises).
//
// See docs/reference-2.7.22-parity.md, Review EN1.
#include "minera/ocel.hpp"
#include "minera/ocel_csv.hpp"
#include "minera/ocel_statistics.hpp"
#include "minera/ocel_sqlite.hpp"
#include "minera/ocel_xml.hpp"

#include <filesystem>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <variant>
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

OcelXmlExportOptions xml_options(OcelXmlFormat format) {
  OcelXmlExportOptions options;
  options.format = format;
  options.allow_lossy = true;
  return options;
}

// The log as read, and after a trip through the transport (the routes'
// input).
std::vector<Ocel> both(const Ocel& log) {
  return {log, parse_ocel_json(serialize_ocel_json(log, OcelJsonFormat::kClassic, true))};
}

std::string csv_events_header(const Ocel& log) {
  OcelCsvExportOptions options;
  options.allow_lossy = true;
  const auto csv = serialize_ocel_csv(log, options);
  return csv.substr(0, csv.find('\n'));
}

// The JSON2 events e1 (2024-01-02, x0 and x1) and e2 (2024-01-01, x1 only):
// the frame's columns follow the file (x0, x1), not the sorted events.
void json2_file_column_order() {
  const auto log = parse_ocel_json(
      "{\"objectTypes\":[{\"name\":\"t\",\"attributes\":[]}],\"eventTypes\":[{\"name\":\"a\",\"attributes\":"
      "[{\"name\":\"x0\",\"type\":\"string\"},{\"name\":\"x1\",\"type\":\"string\"}]}],"
      "\"objects\":[{\"id\":\"o1\",\"type\":\"t\",\"attributes\":[]}],\"events\":["
      "{\"id\":\"e1\",\"type\":\"a\",\"time\":\"2024-01-02T00:00:00Z\",\"attributes\":[{\"name\":\"x0\",\"value\":"
      "\"u\"},{\"name\":\"x1\",\"value\":\"v\"}],\"relationships\":[{\"objectId\":\"o1\",\"qualifier\":\"q\"}]},"
      "{\"id\":\"e2\",\"type\":\"a\",\"time\":\"2024-01-01T00:00:00Z\",\"attributes\":[{\"name\":\"x1\",\"value\":"
      "\"w\"}],\"relationships\":[{\"objectId\":\"o1\",\"qualifier\":\"q\"}]}]}");
  expect(log.events.front().id == "e2", "JSON2: events sorted by time");
  for (const auto& read : both(log))
    expect(csv_events_header(read) == "ocel:eid,ocel:timestamp,ocel:activity,x0,x1,ocel:type:t",
           "JSON2: the events frame keeps the file's column order: " + csv_events_header(read));
}

// An OCEL 2.0 XML / JSON log whose object o1 has the entries `times` (its
// attribute a, values v0, v1, ...).
std::string xml_log(const std::vector<std::string>& times) {
  std::string entries;
  for (std::size_t i = 0; i < times.size(); ++i)
    entries += "<attribute name=\"a\" time=\"" + times[i] + "\">v" + std::to_string(i) + "</attribute>";
  return "<?xml version='1.0' encoding='UTF-8'?>\n<log><object-types><object-type name=\"T\"><attributes>"
         "<attribute name=\"a\" type=\"string\"/></attributes></object-type></object-types><event-types>"
         "<event-type name=\"E\"><attributes/></event-type></event-types><objects><object id=\"o1\" type=\"T\">"
         "<attributes>" +
         entries +
         "</attributes></object></objects><events><event id=\"e1\" type=\"E\" time=\"2024-01-01T00:00:00Z\">"
         "<attributes/><objects><relationship object-id=\"o1\" qualifier=\"q\"/></objects></event></events></log>";
}
std::string json2_log(const std::vector<std::string>& times) {
  std::string entries;
  for (std::size_t i = 0; i < times.size(); ++i)
    entries += std::string(i ? "," : "") + "{\"name\":\"a\",\"time\":\"" + times[i] + "\",\"value\":\"v" +
               std::to_string(i) + "\"}";
  return "{\"objectTypes\":[{\"name\":\"T\",\"attributes\":[{\"name\":\"a\",\"type\":\"string\"}]}],"
         "\"eventTypes\":[{\"name\":\"E\",\"attributes\":[]}],\"objects\":[{\"id\":\"o1\",\"type\":\"T\","
         "\"attributes\":[" +
         entries +
         "]}],\"events\":[{\"id\":\"e1\",\"type\":\"E\",\"time\":\"2024-01-01T00:00:00Z\",\"attributes\":[],"
         "\"relationships\":[{\"objectId\":\"o1\",\"qualifier\":\"q\"}]}]}";
}

void epoch_change_texts() {
  const auto xml = parse_ocel_xml(xml_log({"1970-01-01T00:00:00Z", "2024-05-01T00:00:00Z", "1970-01-01T00:00:00"}));
  for (const auto& read : both(xml)) {
    expect(read.object_changes.size() == 2 && read.object_changes[0].raw_time == OcelRawTime::kNone &&
               read.object_changes[1].raw_time == OcelRawTime::kStr &&
               read.object_changes[1].timestamp == "1970-01-01T00:00:00",
           "XML: the naive epoch change time stays a str");
    expect(error_of([&] { (void)serialize_ocel_xml(read, xml_options(OcelXmlFormat::kStandard)); }) ==
               "AttributeError: 'str' object has no attribute 'isoformat'",
           "write_ocel2_xml raises on the str change time");
    const auto classic = serialize_ocel_json(read, OcelJsonFormat::kClassic, false);
    expect(contains(classic, "\"ocel:timestamp\":\"1970-01-01T00:00:00\""),
           "write_ocel_json prints the str change time as is");
#if defined(MINERA_HAS_SQLITE)
    const auto path = std::filesystem::temp_directory_path() / "minera_review_en1_epoch.sqlite";
    std::filesystem::remove(path);
    expect(error_of([&] { export_ocel_sqlite(read, path); }) ==
               "ValueError: Mixed timezones detected. Pass utc=True in to_datetime or tz='UTC' in DatetimeIndex "
               "to convert to a common timezone.",
           "write_ocel2_sqlite: a naive text next to UTC datetimes");
    std::filesystem::remove(path);
#endif
  }
  const auto json = parse_ocel_json(
      json2_log({"1970-01-01T00:00:00Z", "2024-05-01T00:00:00Z", "1970-01-01T00:00:00", "0"}));
  expect(json.object_changes.size() == 3 && json.object_changes[1].raw_time == OcelRawTime::kStr &&
             json.object_changes[2].raw_time == OcelRawTime::kStr && json.object_changes[2].timestamp == "0",
         "JSON2: a change time \"0\" keeps the column object, the epoch text a str");
  // Without the "0", pd.to_datetime reads the naive epoch text as UTC.
  const auto converted = parse_ocel_json(json2_log({"1970-01-01T00:00:00Z", "2024-05-01T00:00:00Z", "1970-01-01T00:00:00"}));
  expect(converted.object_changes.size() == 2 && converted.object_changes[1].raw_time == OcelRawTime::kNone,
         "JSON2: the epoch text converts with the column");
}

#if defined(MINERA_HAS_SQLITE)
void exec(sqlite3* db, const std::string& sql) {
  char* error = nullptr;
  if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &error) != SQLITE_OK) {
    const std::string message = error == nullptr ? "sqlite error" : error;
    sqlite3_free(error);
    throw std::runtime_error(message);
  }
}

const std::string kTables =
    "CREATE TABLE event (ocel_id TEXT, ocel_type TEXT); CREATE TABLE object (ocel_id TEXT, ocel_type TEXT);"
    "CREATE TABLE event_map_type (ocel_type TEXT, ocel_type_map TEXT);"
    "CREATE TABLE object_map_type (ocel_type TEXT, ocel_type_map TEXT);"
    "CREATE TABLE event_object (ocel_event_id TEXT, ocel_object_id TEXT, ocel_qualifier TEXT);"
    "CREATE TABLE object_object (ocel_source_id TEXT, ocel_target_id TEXT, ocel_qualifier TEXT);";

Ocel sqlite_log(const std::string& name, const std::string& sql) {
  const auto path = std::filesystem::temp_directory_path() / ("minera_review_en1_" + name + ".sqlite");
  std::filesystem::remove(path);
  sqlite3* db = nullptr;
  if (sqlite3_open(path.string().c_str(), &db) != SQLITE_OK) throw std::runtime_error("open the SQLite file");
  exec(db, kTables + sql);
  sqlite3_close(db);
  auto log = import_ocel_sqlite(path);
  std::filesystem::remove(path);
  return log;
}

// An object type's table (found by object_map_type) as export_ocel_sqlite
// writes it: its columns (name and declared type), then its rows (cells as
// text, "NULL" for NULL).
std::string written_table(const Ocel& log, const std::string& type) {
  const auto path = std::filesystem::temp_directory_path() / "minera_review_en1_written.sqlite";
  std::filesystem::remove(path);
  export_ocel_sqlite(log, path);
  sqlite3* db = nullptr;
  sqlite3_open(path.string().c_str(), &db);
  std::string out, table;
  sqlite3_stmt* statement = nullptr;
  sqlite3_prepare_v2(db, "SELECT ocel_type_map FROM object_map_type WHERE ocel_type = ?1", -1, &statement, nullptr);
  sqlite3_bind_text(statement, 1, type.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(statement) == SQLITE_ROW)
    table = "object_" + std::string(reinterpret_cast<const char*>(sqlite3_column_text(statement, 0)));
  sqlite3_finalize(statement);
  sqlite3_prepare_v2(db, ("PRAGMA table_info(\"" + table + "\")").c_str(), -1, &statement, nullptr);
  while (sqlite3_step(statement) == SQLITE_ROW)
    out += std::string(reinterpret_cast<const char*>(sqlite3_column_text(statement, 1))) + " " +
           reinterpret_cast<const char*>(sqlite3_column_text(statement, 2)) + ",";
  sqlite3_finalize(statement);
  sqlite3_prepare_v2(db, ("SELECT * FROM \"" + table + "\"").c_str(), -1, &statement, nullptr);
  while (sqlite3_step(statement) == SQLITE_ROW) {
    out += "|";
    for (int i = 0; i < sqlite3_column_count(statement); ++i) {
      if (i) out += ",";
      const auto type = sqlite3_column_type(statement, i);
      out += type == SQLITE_NULL    ? std::string("NULL")
             : type == SQLITE_FLOAT ? std::to_string(sqlite3_column_double(statement, i)).substr(0, 3)
                                    : std::string(reinterpret_cast<const char*>(sqlite3_column_text(statement, i)));
    }
  }
  sqlite3_finalize(statement);
  sqlite3_close(db);
  std::filesystem::remove(path);
  return out;
}

// cc.py "two_tables": o1 (type T) has a change in object_U's table too.
void change_cells_and_cumcount() {
  const auto log = sqlite_log(
      "cells",
      "INSERT INTO event VALUES ('e1','A'); INSERT INTO event_map_type VALUES ('A','A');"
      "CREATE TABLE event_A (ocel_id TEXT, ocel_time TIMESTAMP, x TEXT);"
      "INSERT INTO event_A VALUES ('e1','2024-01-01 00:00:00+00:00','v');"
      "INSERT INTO object VALUES ('o1','T'); INSERT INTO object VALUES ('o2','U');"
      "INSERT INTO object_map_type VALUES ('T','T'); INSERT INTO object_map_type VALUES ('U','U');"
      "INSERT INTO event_object VALUES ('e1','o1','q'); INSERT INTO event_object VALUES ('e1','o2','q');"
      "CREATE TABLE object_T (ocel_id TEXT, a TEXT, b INTEGER, ocel_changed_field TEXT, ocel_time TIMESTAMP);"
      "CREATE TABLE object_U (ocel_id TEXT, c REAL, ocel_changed_field TEXT, ocel_time TIMESTAMP);"
      "INSERT INTO object_T VALUES ('o1','a0',1,NULL,NULL); INSERT INTO object_U VALUES ('o2',1.5,NULL,NULL);"
      "INSERT INTO object_U VALUES ('o1',2.5,'c','2024-01-05 00:00:00+00:00');"
      "INSERT INTO object_T VALUES ('o1','a1',NULL,'a','2024-01-03 00:00:00+00:00');");
  for (const auto& read : both(log)) {
    expect(read.object_changes.size() == 2 && read.object_changes[0].cells.size() == 1 &&
               read.object_changes[0].cells[0].key == "@@cumcount" &&
               read.object_changes[0].cells[0].value == OcelValue{std::int64_t{1}} &&
               read.object_changes[1].cells.size() == 1 &&
               read.object_changes[1].cells[0].value == OcelValue{std::int64_t{2}},
           "@@cumcount counts o1's rows over the tables in type order");
    const auto classic = serialize_ocel_json(read, OcelJsonFormat::kClassic, false);
    expect(contains(classic, "\"@@cumcount\":1") && contains(classic, "\"@@cumcount\":2"),
           "write_ocel_json prints @@cumcount");
    const auto standard = serialize_ocel_json(read, OcelJsonFormat::kStandard, false);
    expect(contains(standard, "{\"name\":\"T\",\"attributes\":[{\"name\":\"a\",\"type\":\"string\"},{\"name\":\"b\","
                              "\"type\":\"float\"},{\"name\":\"c\",\"type\":\"float\"},{\"name\":\"@@cumcount\","
                              "\"type\":\"string\"}]}"),
           "write_ocel2_json declares @@cumcount a string");
    const auto xml2 = serialize_ocel_xml(read, xml_options(OcelXmlFormat::kStandard));
    expect(contains(xml2, "<attribute name=\"@@cumcount\" type=\"string\"/>"), "write_ocel2_xml likewise");
    const auto table = written_table(read, "T");
    expect(table ==
               "ocel_id TEXT,a TEXT,b REAL,ocel_changed_field TEXT,ocel_time TIMESTAMP,c REAL,@@cumcount REAL,"
               "|o1,a0,1.0,NULL,NULL,NULL,NULL|o1,a1,NULL,a,2024-01-03 00:00:00+00:00,NULL,1.0"
               "|o1,NULL,NULL,c,2024-01-05 00:00:00+00:00,2.5,2.0",
           "write_ocel2_sqlite: object_T as pm4py writes it: " + table);
    expect(written_table(read, "U") == "ocel_id TEXT,c REAL,|o2,1.5", "write_ocel2_sqlite: object_U");
  }
}

// fh.py "nullcol_emptytab" and "act_col": the frames' column order from
// the tables, an all-NULL column kept, an empty table's columns left out;
// the change time column before the changed field.
void table_column_order() {
  const auto nulls = sqlite_log(
      "nulls",
      "INSERT INTO event_map_type VALUES ('A','A'); INSERT INTO event_map_type VALUES ('B','B');"
      "INSERT INTO event_map_type VALUES ('C','C'); INSERT INTO object_map_type VALUES ('T','T');"
      "INSERT INTO object_map_type VALUES ('U','U');"
      "CREATE TABLE event_A (ocel_id TEXT, ocel_time TIMESTAMP, x TEXT, z TEXT);"
      "CREATE TABLE event_B (ocel_id TEXT, ocel_time TIMESTAMP, w TEXT, x TEXT);"
      "CREATE TABLE event_C (ocel_id TEXT, ocel_time TIMESTAMP, v TEXT);"
      "INSERT INTO event VALUES ('e1','A'); INSERT INTO event VALUES ('e2','C');"
      "INSERT INTO event_A VALUES ('e1','2024-01-01 00:00:00+00:00','a',NULL);"
      "INSERT INTO event_C VALUES ('e2','2024-01-02 00:00:00+00:00','c');"
      "CREATE TABLE object_T (ocel_id TEXT, q TEXT, ocel_changed_field TEXT, ocel_time TIMESTAMP, r TEXT);"
      "CREATE TABLE object_U (ocel_id TEXT, s TEXT, n TEXT);"
      "INSERT INTO object VALUES ('o1','T'); INSERT INTO object VALUES ('o2','U');"
      "INSERT INTO object_T VALUES ('o1','q0',NULL,NULL,NULL);"
      "INSERT INTO object_T VALUES ('o1',NULL,'r','2024-01-03 00:00:00+00:00','r1');"
      "INSERT INTO object_U VALUES ('o2','s0',NULL);"
      "INSERT INTO event_object VALUES ('e1','o1','q'); INSERT INTO event_object VALUES ('e2','o2','q');");
  for (const auto& read : both(nulls)) {
    // (pm4py puts ocel:activity last here, as no table has that column;
    // declared: native keeps the base columns first)
    expect(contains(csv_events_header(read), ",x,z,v,"), "CSV: the tables' column order: " + csv_events_header(read));
    expect(written_table(read, "T") ==
               "ocel_id TEXT,q TEXT,ocel_changed_field TEXT,ocel_time TIMESTAMP,r TEXT,@@cumcount REAL,"
               "|o1,q0,NULL,NULL,NULL,NULL|o1,NULL,r,2024-01-03 00:00:00+00:00,r1,1.0",
           "write_ocel2_sqlite: the change-only column r after the time");
  }
  const auto time_first = sqlite_log(
      "time_first",
      "INSERT INTO event_map_type VALUES ('A','A'); INSERT INTO object_map_type VALUES ('T','T');"
      "INSERT INTO object_map_type VALUES ('U','U');"
      "CREATE TABLE event_A (ocel_id TEXT, ocel_time TIMESTAMP, x TEXT);"
      "INSERT INTO event VALUES ('e1','A'); INSERT INTO event_A VALUES ('e1','2024-01-01 00:00:00+00:00','a');"
      "CREATE TABLE object_T (ocel_id TEXT, ocel_time TIMESTAMP, q TEXT, ocel_changed_field TEXT);"
      "CREATE TABLE object_U (ocel_id TEXT, s TEXT, ocel_changed_field TEXT, ocel_time TIMESTAMP);"
      "INSERT INTO object VALUES ('o1','T'); INSERT INTO object VALUES ('o2','U');"
      "INSERT INTO object_T VALUES ('o1',NULL,'q0',NULL); INSERT INTO object_U VALUES ('o2','s0',NULL,NULL);"
      "INSERT INTO object_U VALUES ('o2','s1','s','2024-01-05 00:00:00+00:00');"
      "INSERT INTO event_object VALUES ('e1','o1','q'); INSERT INTO event_object VALUES ('e1','o2','q');");
  for (const auto& read : both(time_first))
    expect(written_table(read, "U") ==
               "ocel_id TEXT,s TEXT,ocel_time TIMESTAMP,ocel_changed_field TEXT,@@cumcount REAL,"
               "|o2,s0,NULL,NULL,NULL|o2,s1,2024-01-05 00:00:00+00:00,s,1.0",
           "write_ocel2_sqlite: the time column before the changed field, as the changes frame has them");
}

// nn2.py: event_A's time column holds 5, NULL and 'abc' (object): its NULL
// is None, so OC-DFG's first pair (e1 5, e2 None) names NoneType.
void none_times() {
  const auto log = sqlite_log(
      "none",
      "INSERT INTO event_map_type VALUES ('a','A'); INSERT INTO object_map_type VALUES ('t','T');"
      "CREATE TABLE event_A (ocel_id TEXT, ocel_time TIMESTAMP); CREATE TABLE object_T (ocel_id TEXT);"
      "INSERT INTO event VALUES ('e1','a'); INSERT INTO event VALUES ('e2','a'); INSERT INTO event VALUES ('e3','a');"
      "INSERT INTO event_A VALUES ('e1', 5); INSERT INTO event_A VALUES ('e2', NULL);"
      "INSERT INTO event_A VALUES ('e3', 'abc');"
      "INSERT INTO object VALUES ('o1','t'); INSERT INTO object VALUES ('o2','t');"
      "INSERT INTO object_T VALUES ('o1'); INSERT INTO object_T VALUES ('o2');"
      "INSERT INTO event_object VALUES ('e1','o1','q'); INSERT INTO event_object VALUES ('e2','o1','q');"
      "INSERT INTO event_object VALUES ('e3','o2','q');");
  for (const auto& read : both(log)) {
    expect(read.events.size() == 3 && read.events[2].id == "e2" && read.events[2].raw_time == OcelRawTime::kNull,
           "the NULL of an object column is None");
    OcelEdgeAssociationOptions measured;
    measured.measure_raw_times = true;
    const auto message = error_of([&] { (void)edge_associations(read, measured); });
    expect(message == "TypeError: unsupported operand type(s) for -: 'NoneType' and 'int'",
           "OC-DFG's pair names NoneType: " + message);
  }
}
#endif

}  // namespace

int main() {
  json2_file_column_order();
  epoch_change_texts();
#if defined(MINERA_HAS_SQLITE)
  change_cells_and_cumcount();
  table_column_order();
  none_times();
#endif
  if (failures != 0) {
    std::cerr << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "review EN1 tests passed\n";
  return 0;
}
