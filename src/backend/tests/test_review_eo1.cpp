// Review EO1: pm4py 2.7.22 on the spoiled banks of rounds EK to EO and hand
// cases (review-harness round-eo: eo/hc/hc.py, eo/hc/hc2.py, eo/d/dsw.py).
// Observed black-box:
//
// - an OCEL 2.0 XML date column with aware and naive times is datetime64
//   (UTC), so write_ocel2_json and write_ocel2_xml declare it "date";
// - read_ocel_json builds its frames over every row before it drops the
//   unrelated ones: write_ocel_csv keeps their columns, write_ocel_xml and
//   write_ocel_json list them as "attribute-names" (write_ocel_json computes
//   its whole "ocel:global-log"); write_ocel_csv keeps an all-null column;
// - read_ocel_xml fails on a file without a relation like read_ocel2_xml;
// - the O2O frame is float64 when a reader found no link (read_ocel2_json:
//   when the file lists none, a dangling one makes it str), so
//   write_ocel2_sqlite declares object_object REAL;
// - write_ocel_json, write_ocel2_json and write_ocel_xml print an events or
//   objects column that is object only through a dropped row (pandas'
//   infer_objects): ints next to a missing cell print as floats there,
//   while write_ocel2_xml and write_ocel_csv keep them;
// - read_ocel2_json's changes frame puts its field column first and its
//   time column after the first change's field (write_ocel2_sqlite's type
//   tables show it), and reads a naive epoch change time as UTC;
// - read_ocel2_sqlite splits the union of the type tables by
//   ocel_changed_field, so an int column another table lacks is float64 in
//   the changes frame too, and "@@cumcount" is NaN (float64) for a row
//   without an id; it reads the type table of an object listed without id;
// - for a log without relations write_ocel_json writes no type catalogs
//   and no "ocel:objectChanges", write_ocel2_sqlite REAL map tables;
// - write_ocel_csv quotes as Python's csv module does (QUOTE_MINIMAL), not
//   a value with leading or trailing blanks;
// - read_ocel2_json and read_ocel2_xml keep a change time they cannot read
//   (a "+25:00" offset) as a str next to the datetimes, which
//   write_ocel2_sqlite writes as NULL; write_ocel2_xml raises first on a
//   change without a time there (None beside datetimes, NaN without).
//
// See docs/reference-2.7.22-parity.md, Review EO1.
#include "minera/ocel.hpp"
#include "minera/ocel_csv.hpp"
#include "minera/ocel_sqlite.hpp"
#include "minera/ocel_xml.hpp"

#include <filesystem>
#include <functional>
#include <iostream>
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

std::string classic_download(const Ocel& log) { return serialize_ocel_json(log, OcelJsonFormat::kClassic, false); }
std::string standard_download(const Ocel& log) { return serialize_ocel_json(log, OcelJsonFormat::kStandard, false); }

// hc2.py EO1-1: d (declared date) holds an aware and a naive time.
void aware_and_naive_dates() {
  const auto log = parse_ocel_xml(
      "<?xml version='1.0' encoding='UTF-8'?>\n<log><object-types><object-type name=\"t\"><attributes/>"
      "</object-type></object-types><event-types><event-type name=\"a\"><attributes><attribute name=\"d\" "
      "type=\"date\"/></attributes></event-type></event-types><objects><object id=\"o1\" type=\"t\"><attributes/>"
      "</object></objects><events><event id=\"e1\" type=\"a\" time=\"2024-01-01T00:00:00Z\"><attributes>"
      "<attribute name=\"d\">2024-02-01T10:00:00Z</attribute></attributes><objects><relationship object-id=\"o1\" "
      "qualifier=\"q\"/></objects></event><event id=\"e2\" type=\"a\" time=\"2024-01-02T00:00:00Z\"><attributes>"
      "<attribute name=\"d\">2024-02-02T00:00:00</attribute></attributes><objects><relationship object-id=\"o1\" "
      "qualifier=\"q\"/></objects></event></events></log>",
      {});
  for (const auto& read : both(log)) {
    expect(contains(standard_download(read), "\"eventTypes\":[{\"name\":\"a\",\"attributes\":[{\"name\":\"d\","
                                             "\"type\":\"date\"}]}]"),
           "write_ocel2_json declares the aware and naive column a date");
    expect(contains(serialize_ocel_xml(read, xml_options(OcelXmlFormat::kStandard)),
                    "<attribute name=\"d\" type=\"date\"/>"),
           "write_ocel2_xml declares it a date");
  }
}

// eo/hc/u8: write_ocel_csv quotes as Python's csv module (QUOTE_MINIMAL).
void csv_blank_edges() {
  const auto log = parse_ocel_json(
      "{\"ocel:global-event\":{},\"ocel:global-object\":{},\"ocel:global-log\":{\"ocel:attribute-names\":[],"
      "\"ocel:object-types\":[\"t\"]},\"ocel:events\":{\"e1\":{\"ocel:activity\":\"a\",\"ocel:timestamp\":"
      "\"2024-01-01T00:00:00Z\",\"ocel:omap\":[\"o1\"],\"ocel:vmap\":{\"k\":\" 3\",\"j\":\"a,b\",\"q\":\"x \"}}},"
      "\"ocel:objects\":{\"o1\":{\"ocel:type\":\"t\",\"ocel:ovmap\":{\"m\":\"\\tv\"}}}}");
  for (const auto& read : both(log)) {
    expect(serialize_ocel_csv(read, csv_options()) ==
               "ocel:eid,ocel:timestamp,ocel:activity,k,j,q,ocel:type:t\n"
               "e1,2024-01-01 00:00:00+00:00,a, 3,\"a,b\",x ,['o1']\n",
           "write_ocel_csv leaves blank-edged values unquoted: " + serialize_ocel_csv(read, csv_options()));
    expect(serialize_ocel_objects_csv(read, csv_options()) == "ocel:oid,ocel:type,m\no1,t,\tv\n",
           "and in the objects file: " + serialize_ocel_objects_csv(read, csv_options()));
  }
}

// hc2.py EO1-2 and EO1-3: e2 and o2 take part in no relation.
void dropped_rows_columns() {
  const auto log = parse_ocel_json(
      "{\"ocel:global-event\":{},\"ocel:global-object\":{},\"ocel:global-log\":{\"ocel:attribute-names\":[],"
      "\"ocel:object-types\":[\"t\"]},\"ocel:events\":{\"e1\":{\"ocel:activity\":\"a\",\"ocel:timestamp\":"
      "\"2024-01-01T00:00:00Z\",\"ocel:omap\":[\"o1\"],\"ocel:vmap\":{\"k\":1}},\"e2\":{\"ocel:activity\":\"a\","
      "\"ocel:timestamp\":\"2024-01-02T00:00:00Z\",\"ocel:omap\":[],\"ocel:vmap\":{\"n\":2,\"z\":\"q\"}}},"
      "\"ocel:objects\":{\"o1\":{\"ocel:type\":\"t\",\"ocel:ovmap\":{\"m\":1}},\"o2\":{\"ocel:type\":\"u\","
      "\"ocel:ovmap\":{\"s\":\"v\"}}}}");
  expect(log.events.size() == 1 && log.objects.size() == 1, "read_ocel_json drops the unrelated rows");
  for (const auto& read : both(log)) {
    expect(first_line(serialize_ocel_csv(read, csv_options())) == "ocel:eid,ocel:timestamp,ocel:activity,k,n,z,ocel:type:t",
           "write_ocel_csv keeps the dropped event's columns: " + first_line(serialize_ocel_csv(read, csv_options())));
    expect(first_line(serialize_ocel_objects_csv(read, csv_options())) == "ocel:oid,ocel:type,m,s",
           "write_ocel_csv keeps the dropped object's columns");
    const auto xml = serialize_ocel_xml(read, xml_options(OcelXmlFormat::kClassic));
    expect(contains(xml, "<string key=\"attribute-name\" value=\"k\"/>\n      <string key=\"attribute-name\" "
                         "value=\"m\"/>\n      <string key=\"attribute-name\" value=\"n\"/>\n      <string "
                         "key=\"attribute-name\" value=\"s\"/>\n      <string key=\"attribute-name\" value=\"z\"/>"),
           "write_ocel_xml lists the frames' columns");
    expect(contains(classic_download(read),
                    "\"ocel:global-log\":{\"ocel:object-types\":[\"t\"],\"ocel:attribute-names\":[\"k\",\"m\",\"n\","
                    "\"s\",\"z\"],\"ocel:version\":\"1.0\",\"ocel:ordering\":\"timestamp\"}"),
           "write_ocel_json computes its log scope");
  }
  const auto nulls = parse_ocel_json(
      "{\"ocel:global-event\":{},\"ocel:global-object\":{},\"ocel:global-log\":{\"ocel:attribute-names\":[],"
      "\"ocel:object-types\":[\"t\"]},\"ocel:events\":{\"e1\":{\"ocel:activity\":\"a\",\"ocel:timestamp\":"
      "\"2024-01-01T00:00:00Z\",\"ocel:omap\":[\"o1\"],\"ocel:vmap\":{\"k\":null,\"w\":1}}},\"ocel:objects\":"
      "{\"o1\":{\"ocel:type\":\"t\",\"ocel:ovmap\":{\"m\":null}}}}");
  for (const auto& read : both(nulls)) {
    expect(serialize_ocel_csv(read, csv_options()) ==
               "ocel:eid,ocel:timestamp,ocel:activity,k,w,ocel:type:t\ne1,2024-01-01 00:00:00+00:00,a,,1,['o1']\n",
           "write_ocel_csv keeps the all-null column: " + serialize_ocel_csv(read, csv_options()));
    expect(serialize_ocel_objects_csv(read, csv_options()) == "ocel:oid,ocel:type,m\no1,t,\n",
           "and the objects' all-null column");
  }
}

// hc2.py EO1-4.
void classic_xml_without_relations() {
  const std::string xml =
      "<?xml version='1.0' encoding='UTF-8'?>\n<log><global scope=\"event\"><string key=\"activity\" "
      "value=\"__INVALID__\"/></global><global scope=\"object\"><string key=\"type\" value=\"__INVALID__\"/>"
      "</global>\n<global scope=\"log\"><list key=\"attribute-names\"/><list key=\"object-types\"><string "
      "key=\"object-type\" value=\"t\"/></list><string key=\"version\" value=\"1.0\"/><string key=\"ordering\" "
      "value=\"timestamp\"/></global>\n<events><event><string key=\"id\" value=\"e1\"/><string key=\"activity\" "
      "value=\"a\"/><date key=\"timestamp\" value=\"2024-01-01T00:00:00Z\"/><list key=\"omap\"/><list "
      "key=\"vmap\"/></event></events>\n<objects><object><string key=\"id\" value=\"o1\"/><string key=\"type\" "
      "value=\"t\"/><list key=\"ovmap\"/></object></objects></log>";
  expect(error_of([&] { (void)parse_ocel_xml(xml, {}); }) == "'NoneType' object has no attribute 'index'",
         "read_ocel_xml fails on a file without a relation");
}

// hc.py EO1-6: x0 is an object column through the dropped event ed alone.
void inferred_float_columns() {
  const auto log = parse_ocel_json(
      "{\"objectTypes\":[{\"name\":\"cust\",\"attributes\":[]}],\"eventTypes\":[{\"name\":\"a\",\"attributes\":"
      "[{\"name\":\"x0\",\"type\":\"integer\"}]}],\"objects\":[{\"id\":\"o0\",\"type\":\"cust\"}],\"events\":["
      "{\"id\":\"e0\",\"type\":\"a\",\"time\":\"2024-01-01T00:00:00Z\",\"attributes\":[{\"name\":\"x0\",\"value\":"
      "-3}],\"relationships\":[{\"objectId\":\"o0\",\"qualifier\":\"q\"}]},{\"id\":\"e1\",\"type\":\"a\",\"time\":"
      "\"2024-01-02T00:00:00Z\",\"attributes\":[{\"name\":\"x0\",\"value\":1}],\"relationships\":[{\"objectId\":"
      "\"o0\",\"qualifier\":\"q\"}]},{\"id\":\"e2\",\"type\":\"a\",\"time\":\"2024-01-03T00:00:00Z\",\"attributes\":"
      "[],\"relationships\":[{\"objectId\":\"o0\",\"qualifier\":\"q\"}]},{\"id\":\"ed\",\"type\":\"a\",\"time\":"
      "\"2024-01-09T00:00:00Z\",\"attributes\":[{\"name\":\"x0\",\"value\":\"hello\"}],\"relationships\":[]}]}");
  for (const auto& read : both(log)) {
    const auto classic = classic_download(read);
    expect(contains(classic, "\"x0\":-3.0") && contains(classic, "\"x0\":1.0") && contains(classic, "\"x0\":\"string\""),
           "write_ocel_json prints the ints as floats, the type a string: " + classic);
    const auto standard = standard_download(read);
    expect(contains(standard, "{\"name\":\"x0\",\"value\":-3.0}") && contains(standard, "{\"name\":\"x0\",\"value\":1.0}"),
           "write_ocel2_json prints them as floats");
    const auto xml = serialize_ocel_xml(read, xml_options(OcelXmlFormat::kClassic));
    expect(contains(xml, "<string key=\"x0\" value=\"-3.0\"/>") && contains(xml, "<string key=\"x0\" value=\"1.0\"/>"),
           "write_ocel_xml prints the float reprs as strings");
    const auto standard_xml = serialize_ocel_xml(read, xml_options(OcelXmlFormat::kStandard));
    expect(contains(standard_xml, ">-3</attribute>") && contains(standard_xml, ">1</attribute>"),
           "write_ocel2_xml keeps the ints");
    const auto csv = serialize_ocel_csv(read, csv_options());
    expect(contains(csv, "e0,2024-01-01 00:00:00+00:00,a,-3,['o0']"), "write_ocel_csv keeps the ints");
  }
}

// hc.py EO1-7: o0's p0 has no static value, then two changes.
Ocel json2_changes_log() {
  return parse_ocel_json(
      "{\"objectTypes\":[{\"name\":\"pkg\",\"attributes\":[{\"name\":\"p0\",\"type\":\"integer\"}]}],"
      "\"eventTypes\":[{\"name\":\"a\",\"attributes\":[]}],\"objects\":[{\"id\":\"o0\",\"type\":\"pkg\","
      "\"attributes\":[{\"name\":\"p0\",\"time\":\"1970-01-01T00:00:00Z\",\"value\":null},{\"name\":\"p0\","
      "\"time\":\"2024-01-02T00:00:00Z\",\"value\":2},{\"name\":\"p0\",\"time\":\"1970-01-01T00:00:00\","
      "\"value\":3}]}],\"events\":[{\"id\":\"e0\",\"type\":\"a\",\"time\":\"2024-01-01T00:00:00Z\","
      "\"relationships\":[{\"objectId\":\"o0\",\"qualifier\":\"q\"}]}]}");
}

void json2_changes() {
  for (const auto& read : both(json2_changes_log())) {
    const auto standard = standard_download(read);
    expect(contains(standard, "\"time\":\"2024-01-02T00:00:00+00:00\"") &&
               contains(standard, "\"time\":\"1970-01-01T00:00:00+00:00\""),
           "read_ocel2_json reads the naive epoch change time as UTC: " + standard);
  }
}

// eo/hc/u9 (g52_0224): next to a kept str, a change without a time is None
// beside datetimes and NaN without them, and write_ocel2_xml raises on it
// first.
void missing_change_time_beside_str() {
  const auto log = [](const std::string& middle) {
    return parse_ocel_json(
        "{\"objectTypes\":[{\"name\":\"t\",\"attributes\":[{\"name\":\"t1\",\"type\":\"string\"}]}],"
        "\"eventTypes\":[{\"name\":\"a\",\"attributes\":[]}],\"objects\":[{\"id\":\"o0\",\"type\":\"t\","
        "\"attributes\":[{\"name\":\"t1\",\"time\":\"1970-01-01T00:00:00Z\",\"value\":\"x\"},{\"name\":\"t1\","
        "\"value\":\"a\"}," + middle + "{\"name\":\"t1\",\"time\":\"2024-02-01T10:00:00+25:00\",\"value\":\"b\"}]}],"
        "\"events\":[{\"id\":\"e0\",\"type\":\"a\",\"time\":\"2024-01-01T00:00:00Z\",\"relationships\":"
        "[{\"objectId\":\"o0\",\"qualifier\":\"q\"}]}]}");
  };
  const auto message = [](const Ocel& read) {
    return error_of([&] { (void)serialize_ocel_xml(read, xml_options(OcelXmlFormat::kStandard)); });
  };
  for (const auto& read : both(log("")))
    expect(message(read) == "AttributeError: 'float' object has no attribute 'isoformat'",
           "NaN without datetimes: " + message(read));
  for (const auto& read : both(log("{\"name\":\"t1\",\"time\":\"2024-01-05T00:00:00Z\",\"value\":\"c\"},")))
    expect(message(read) == "AttributeError: 'NoneType' object has no attribute 'isoformat'",
           "None beside datetimes: " + message(read));
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
const std::string kEvent =
    "INSERT INTO event VALUES('e1','E'); INSERT INTO event_map_type VALUES('E','E');"
    "CREATE TABLE event_E (ocel_id TEXT, ocel_time TIMESTAMP);"
    "INSERT INTO event_E VALUES('e1','2024-01-01 00:00:00+00:00');";

Ocel sqlite_log(const std::string& name, const std::string& sql) {
  const auto path = std::filesystem::temp_directory_path() / ("minera_review_eo1_" + name + ".sqlite");
  std::filesystem::remove(path);
  sqlite3* db = nullptr;
  if (sqlite3_open(path.string().c_str(), &db) != SQLITE_OK) throw std::runtime_error("open the SQLite file");
  exec(db, kTables + kEvent + sql);
  sqlite3_close(db);
  auto log = import_ocel_sqlite(path);
  std::filesystem::remove(path);
  return log;
}

// A table's columns ("name TYPE," each) in the file export_ocel_sqlite
// writes; an object type's table by its name in object_map_type ("@type").
std::string written_columns(const Ocel& log, const std::string& table_or_type) {
  const auto path = std::filesystem::temp_directory_path() / "minera_review_eo1_written.sqlite";
  std::filesystem::remove(path);
  export_ocel_sqlite(log, path);
  sqlite3* db = nullptr;
  sqlite3_open(path.string().c_str(), &db);
  std::string table = table_or_type, out;
  sqlite3_stmt* statement = nullptr;
  if (table.starts_with("@")) {
    sqlite3_prepare_v2(db, "SELECT ocel_type_map FROM object_map_type WHERE ocel_type = ?1", -1, &statement, nullptr);
    sqlite3_bind_text(statement, 1, table.c_str() + 1, -1, SQLITE_TRANSIENT);
    table.clear();
    if (sqlite3_step(statement) == SQLITE_ROW)
      table = "object_" + std::string(reinterpret_cast<const char*>(sqlite3_column_text(statement, 0)));
    sqlite3_finalize(statement);
  }
  sqlite3_prepare_v2(db, ("PRAGMA table_info(\"" + table + "\")").c_str(), -1, &statement, nullptr);
  while (sqlite3_step(statement) == SQLITE_ROW)
    out += std::string(reinterpret_cast<const char*>(sqlite3_column_text(statement, 1))) + " " +
           reinterpret_cast<const char*>(sqlite3_column_text(statement, 2)) + ",";
  sqlite3_finalize(statement);
  sqlite3_close(db);
  std::filesystem::remove(path);
  return out;
}

// The ocel_time cells of an object type's table in the file
// export_ocel_sqlite writes ("NULL" for a NULL), comma separated.
std::string written_times(const Ocel& log, const std::string& type) {
  const auto path = std::filesystem::temp_directory_path() / "minera_review_eo1_times.sqlite";
  std::filesystem::remove(path);
  export_ocel_sqlite(log, path);
  sqlite3* db = nullptr;
  sqlite3_open(path.string().c_str(), &db);
  sqlite3_stmt* statement = nullptr;
  std::string table, out;
  sqlite3_prepare_v2(db, "SELECT ocel_type_map FROM object_map_type WHERE ocel_type = ?1", -1, &statement, nullptr);
  sqlite3_bind_text(statement, 1, type.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(statement) == SQLITE_ROW)
    table = "object_" + std::string(reinterpret_cast<const char*>(sqlite3_column_text(statement, 0)));
  sqlite3_finalize(statement);
  sqlite3_prepare_v2(db, ("SELECT ocel_time FROM \"" + table + "\"").c_str(), -1, &statement, nullptr);
  while (sqlite3_step(statement) == SQLITE_ROW)
    out += (sqlite3_column_type(statement, 0) == SQLITE_NULL
                ? std::string("NULL")
                : std::string(reinterpret_cast<const char*>(sqlite3_column_text(statement, 0)))) +
           ",";
  sqlite3_finalize(statement);
  sqlite3_close(db);
  std::filesystem::remove(path);
  return out;
}

// eo/hc/u7 (g52_0020, x52_0020): a change time with a "+25:00" offset
// stays a str next to the datetimes, which write_ocel2_sqlite writes as
// NULL; the other change times keep their UTC wall clock.
void unreadable_change_time() {
  const std::string attributes =
      "{\"name\":\"t1\",\"time\":\"1970-01-01T00:00:00Z\",\"value\":\"x\"},{\"name\":\"t1\",\"time\":"
      "\"2024-01-23T07:31:24-05:30\",\"value\":\"y\"},{\"name\":\"t1\",\"time\":\"2024-02-01T10:00:00+25:00\","
      "\"value\":\"z\"}";
  const auto json2 = parse_ocel_json(
      "{\"objectTypes\":[{\"name\":\"t\",\"attributes\":[{\"name\":\"t1\",\"type\":\"string\"}]}],"
      "\"eventTypes\":[{\"name\":\"a\",\"attributes\":[]}],\"objects\":[{\"id\":\"o0\",\"type\":\"t\","
      "\"attributes\":[" + attributes + "]}],\"events\":[{\"id\":\"e0\",\"type\":\"a\",\"time\":"
      "\"2024-01-01T00:00:00Z\",\"relationships\":[{\"objectId\":\"o0\",\"qualifier\":\"q\"}]}]}");
  const auto xml2 = parse_ocel_xml(
      "<log><object-types><object-type name=\"t\"><attributes><attribute name=\"t1\" type=\"string\"/>"
      "</attributes></object-type></object-types><event-types><event-type name=\"a\"><attributes/></event-type>"
      "</event-types><objects><object id=\"o0\" type=\"t\"><attributes>"
      "<attribute name=\"t1\" time=\"1970-01-01T00:00:00Z\">x</attribute>"
      "<attribute name=\"t1\" time=\"2024-01-23T07:31:24-05:30\">y</attribute>"
      "<attribute name=\"t1\" time=\"2024-02-01T10:00:00+25:00\">z</attribute></attributes><objects/></object>"
      "</objects><events><event id=\"e0\" type=\"a\" time=\"2024-01-01T00:00:00Z\"><attributes/><objects>"
      "<relationship object-id=\"o0\" qualifier=\"q\"/></objects></event></events></log>");
  for (const auto* log : {&json2, &xml2})
    for (const auto& read : both(*log))
      expect(written_times(read, "t") == "NULL,2024-01-23 07:31:24+00:00,NULL,",
             "write_ocel2_sqlite writes NULL for the kept str: " + written_times(read, "t"));
}

const std::string kRealLinks = "ocel_source_id REAL,ocel_target_id REAL,ocel_qualifier REAL,";
const std::string kTextLinks = "ocel_source_id TEXT,ocel_target_id TEXT,ocel_qualifier TEXT,";

// hc2.py EO1-5 (and EO1-1, EO1-2: XML and OCEL 1.0 JSON reads).
void untyped_links() {
  const auto json2 = [](const std::string& links) {
    return parse_ocel_json("{\"objectTypes\":[{\"name\":\"t\",\"attributes\":[]}],\"eventTypes\":[{\"name\":\"a\","
                           "\"attributes\":[]}],\"objects\":[{\"id\":\"o1\",\"type\":\"t\"" +
                           links +
                           "}],\"events\":[{\"id\":\"e1\",\"type\":\"a\",\"time\":\"2024-01-01T00:00:00Z\","
                           "\"relationships\":[{\"objectId\":\"o1\",\"qualifier\":\"q\"}]}]}");
  };
  for (const auto& read : both(json2(""))) {
    expect(written_columns(read, "object_object") == kRealLinks, "JSON2 without links: object_object REAL");
    expect(written_columns(read, "event_object") == "ocel_event_id TEXT,ocel_object_id TEXT,ocel_qualifier TEXT,",
           "event_object stays TEXT");
  }
  for (const auto& read : both(json2(",\"relationships\":[{\"objectId\":\"zz\",\"qualifier\":\"q\"}]")))
    expect(written_columns(read, "object_object") == kTextLinks,
           "JSON2 with a dangling link: object_object TEXT: " + written_columns(read, "object_object"));
  const auto xml = parse_ocel_xml(
      "<?xml version='1.0' encoding='UTF-8'?>\n<log><global scope=\"event\"><string key=\"activity\" "
      "value=\"__INVALID__\"/></global><global scope=\"object\"><string key=\"type\" value=\"__INVALID__\"/>"
      "</global><events><event><string key=\"id\" value=\"e1\"/><string key=\"activity\" value=\"a\"/><date "
      "key=\"timestamp\" value=\"2024-01-01T00:00:00Z\"/><list key=\"omap\"><string key=\"object-id\" "
      "value=\"o1\"/></list><list key=\"vmap\"/></event></events><objects><object><string key=\"id\" "
      "value=\"o1\"/><string key=\"type\" value=\"t\"/><list key=\"ovmap\"/></object></objects></log>",
      {});
  for (const auto& read : both(xml))
    expect(written_columns(read, "object_object") == kRealLinks, "OCEL 1.0 XML: object_object REAL");
}

// hc.py EO1-7.
void json2_change_columns() {
  for (const auto& read : both(json2_changes_log()))
    expect(written_columns(read, "@pkg") == "ocel_id TEXT,ocel_changed_field TEXT,p0 REAL,ocel_time TIMESTAMP,",
           "the changes frame's field column stands before p0: " + written_columns(read, "@pkg"));
}

// hc.py EO1-9: order lacks i0; item has an id-less row.
void union_changes_frame() {
  const auto log = sqlite_log(
      "union",
      "INSERT INTO object VALUES('o4','item'),('o3','order');"
      "INSERT INTO object_map_type VALUES('item','Item'),('order','Order');"
      "INSERT INTO event_object VALUES('e1','o4','q'),('e1','o3','q');"
      "CREATE TABLE object_Item (ocel_id TEXT, i0 INTEGER, ocel_changed_field TEXT, ocel_time TIMESTAMP);"
      "INSERT INTO object_Item VALUES('o4',1,NULL,NULL),('o4',2,'i0','2024-01-12 16:01:22+00:00');"
      "CREATE TABLE object_Order (ocel_id TEXT, o0 INTEGER); INSERT INTO object_Order VALUES('o3',0);");
  for (const auto& read : both(log)) {
    expect(contains(standard_download(read), "{\"name\":\"i0\",\"value\":2.0,\"time\":\"2024-01-12T16:01:22+00:00\"}"),
           "write_ocel2_json prints the change of the float64 column as a float: " + standard_download(read));
    expect(contains(serialize_ocel_xml(read, xml_options(OcelXmlFormat::kStandard)),
                    "time=\"2024-01-12T16:01:22+00:00\" type=\"float\">2.0</attribute>"),
           "write_ocel2_xml too");
  }
  const auto idless = sqlite_log(
      "idless",
      "INSERT INTO object VALUES('o1','pkg'),('o0','item');"
      "INSERT INTO object_map_type VALUES('item','Item'),('pkg','Pkg');"
      "INSERT INTO event_object VALUES('e1','o1','q');"
      "CREATE TABLE object_Item (ocel_id TEXT, i0 INTEGER); INSERT INTO object_Item VALUES(NULL, NULL);"
      "CREATE TABLE object_Pkg (ocel_id TEXT, p1 TEXT, ocel_changed_field TEXT, ocel_time TIMESTAMP);"
      "INSERT INTO object_Pkg VALUES('o1','a',NULL,NULL),('o1','b','p1','2024-01-05 00:00:00+00:00');");
  for (const auto& read : both(idless))
    expect(contains(classic_download(read), "\"@@cumcount\":1.0"),
           "@@cumcount is float64 next to the id-less row: " + classic_download(read));
}

// hc.py EO1-11: the object listed without id has its type table read.
void idless_listing() {
  const auto log = sqlite_log(
      "listing",
      "INSERT INTO object VALUES('o1','t'),('','cust');"
      "INSERT INTO object_map_type VALUES('t','T'),('cust','Cust');"
      "INSERT INTO event_object VALUES('e1','o1','q');"
      "CREATE TABLE object_T (ocel_id TEXT, t0 TEXT); INSERT INTO object_T VALUES('o1','x');"
      "CREATE TABLE object_Cust (ocel_id TEXT, c0 TEXT); INSERT INTO object_Cust VALUES('o2','y');");
  for (const auto& read : both(log)) {
    expect(read.objects.size() == 1, "o2 is no object");
    expect(contains(classic_download(read), "\"ocel:attribute-names\":[\"c0\",\"t0\"]"),
           "the frame holds cust's column: " + classic_download(read));
  }
}

// hc.py EO1-10: no relation, so no event and no object stays.
void log_without_relations() {
  const auto log = parse_ocel_json(
      "{\"ocel:global-event\":{},\"ocel:global-object\":{},\"ocel:global-log\":{\"ocel:attribute-names\":[],"
      "\"ocel:object-types\":[\"t\"]},\"ocel:events\":{\"e0\":{\"ocel:activity\":\"pay\",\"ocel:timestamp\":"
      "\"2024-02-06T02:22:46Z\",\"ocel:omap\":[],\"ocel:vmap\":{}}},\"ocel:objects\":{\"o0\":{\"ocel:type\":\"t\","
      "\"ocel:ovmap\":{\"n\":\"a b\"}}}}");
  for (const auto& read : both(log)) {
    const auto classic = classic_download(read);
    expect(!contains(classic, "ocel:eventTypes") && !contains(classic, "ocel:objectTypes") &&
               !contains(classic, "ocel:objectChanges"),
           "write_ocel_json writes no catalogs and no changes: " + classic);
    expect(written_columns(read, "event_map_type") == "ocel_type REAL,ocel_type_map REAL," &&
               written_columns(read, "object_map_type") == "ocel_type REAL,ocel_type_map REAL,",
           "write_ocel2_sqlite: REAL map tables");
    expect(written_columns(read, "object_object") == kRealLinks, "and REAL links");
  }
}
#endif

}  // namespace

int main() {
  aware_and_naive_dates();
  dropped_rows_columns();
  classic_xml_without_relations();
  inferred_float_columns();
  csv_blank_edges();
  json2_changes();
#if defined(MINERA_HAS_SQLITE)
  untyped_links();
  json2_change_columns();
  union_changes_frame();
  idless_listing();
  log_without_relations();
  unreadable_change_time();
#endif
  missing_change_time_beside_str();
  if (failures != 0) {
    std::cerr << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "review EO1 tests passed\n";
  return 0;
}
