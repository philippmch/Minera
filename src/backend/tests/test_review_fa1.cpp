// Review FA1: exact filters on transports, OCEL 1.0 XML object changes
// (review-harness round-fa: q/csw.py chains, c2/c2gen.py banks c61/c62,
// t1/c1.xmlocel, t1/f3.xmlocel). Observed black-box on pm4py 2.7.22:
//
// - FA1-1: an exact filter reads its input with literal names; a SQLite
//   import's change rows keep their other cells and "@@cumcount" there too,
//   so the result's downloads print them as write_ocel_json does.
// - FA1-2: an exact attribute filter after an object type filter finds a
//   column only the removed rows held (the frame keeps it), so it selects
//   nothing (or everything, negated) instead of raising KeyError.
// - FA1-3: under a key a <date> declares for an object type, a <string>
//   "" and a <float> 1.5 of the same type stay a str and a float of the
//   object column, which pm4py's isin matches as they are.
// - FA1-4: read_ocel_xml's changes frame puts its time column before its
//   field column, which write_ocel2_sqlite's type tables show.
// - FA1-5: a <string> change under a key a <date> declares stays its text
//   in every writer, through both transports.
// - FA1-6: write_ocel2_xml prints an int of a float64 column (an exact
//   filter's input keeps its ints) as a float.
//
// See docs/reference-2.7.22-parity.md, Review FA1.
#include "minera/ocel.hpp"
#include "minera/ocel_filter.hpp"
#include "minera/ocel_sqlite.hpp"
#include "minera/ocel_xml.hpp"
#include "minera/timestamp.hpp"

#include <algorithm>
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

// A route's input: the transport of `log`, read as the exact filter route
// reads it (literal names) or as the other routes do.
Ocel exact_input(const Ocel& log) { return parse_ocel_json_literal_names(serialize_ocel_json(log)); }
Ocel route_input(const Ocel& log, OcelJsonFormat format = OcelJsonFormat::kClassic) {
  return parse_ocel_json(serialize_ocel_json(log, format, true));
}

std::vector<std::string> object_ids(const Ocel& log) {
  std::vector<std::string> ids;
  for (const auto& object : log.objects) ids.push_back(object.id);
  std::sort(ids.begin(), ids.end());
  return ids;
}

std::vector<std::string> event_ids(const Ocel& log) {
  std::vector<std::string> ids;
  for (const auto& event : log.events) ids.push_back(event.id);
  std::sort(ids.begin(), ids.end());
  return ids;
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

Ocel sqlite_log(const std::string& sql) {
  const auto path = std::filesystem::temp_directory_path() / "minera_review_fa1_source.sqlite";
  std::filesystem::remove(path);
  sqlite3* db = nullptr;
  if (sqlite3_open(path.string().c_str(), &db) != SQLITE_OK) throw std::runtime_error("open the SQLite file");
  exec(db,
       "CREATE TABLE event (ocel_id TEXT, ocel_type TEXT); CREATE TABLE object (ocel_id TEXT, ocel_type TEXT);"
       "CREATE TABLE event_map_type (ocel_type TEXT, ocel_type_map TEXT);"
       "CREATE TABLE object_map_type (ocel_type TEXT, ocel_type_map TEXT);"
       "CREATE TABLE event_object (ocel_event_id TEXT, ocel_object_id TEXT, ocel_qualifier TEXT);"
       "CREATE TABLE object_object (ocel_source_id TEXT, ocel_target_id TEXT, ocel_qualifier TEXT);" +
           sql);
  sqlite3_close(db);
  auto log = import_ocel_sqlite(path);
  std::filesystem::remove(path);
  return log;
}

// An object type's table (found by object_map_type) as export_ocel_sqlite
// writes it: its columns (name and declared type), then its rows (cells as
// text, "NULL" for NULL).
std::string written_table(const Ocel& log, const std::string& type) {
  const auto path = std::filesystem::temp_directory_path() / "minera_review_fa1_written.sqlite";
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
      out += sqlite3_column_type(statement, i) == SQLITE_NULL
                 ? std::string("NULL")
                 : std::string(reinterpret_cast<const char*>(sqlite3_column_text(statement, i)));
    }
  }
  sqlite3_finalize(statement);
  sqlite3_close(db);
  std::filesystem::remove(path);
  return out;
}

// FA1-1 (round EN's cc.py "two_tables" log): o1 (type T) has a change in
// object_U's table too, so its change rows hold "@@cumcount" 1 and 2.
void exact_filters_keep_change_cells() {
  const auto log = sqlite_log(
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
  const auto input = exact_input(log);
  expect(input.object_changes.size() == 2 && input.object_changes[0].cells.size() == 1 &&
             input.object_changes[0].cells[0].key == "@@cumcount" &&
             input.object_changes[1].cells.size() == 1 && input.object_changes[1].cells[0].key == "@@cumcount",
         "FA1-1: the exact filter's input keeps the change rows' other cells");
  const auto kept = filter_ocel_event_attribute_exact(input, "x", {std::string("v")});
  const auto timed = filter_ocel_events_timestamp_exact(input, std::string("2023-12-31 00:00:00"),
                                                        std::string("2024-01-02 00:00:00"));
  for (const auto* result : {&kept, &timed}) {
    const auto classic = serialize_ocel_json(*result, OcelJsonFormat::kClassic, false);
    expect(result->object_changes.size() == 2 && contains(classic, "\"@@cumcount\":1") &&
               contains(classic, "\"@@cumcount\":2"),
           "FA1-1: write_ocel_json prints the result's @@cumcount: " + classic);
    expect(contains(written_table(*result, "T"), "@@cumcount REAL"),
           "FA1-1: write_ocel2_sqlite writes the result's @@cumcount column");
  }
}

// FA1-6: s107_0027's o1 column holds ints in its own type table and is
// missing in the other's, so the objects frame is float64.
void xml2_prints_float_column_ints() {
  const auto log = sqlite_log(
      "INSERT INTO event VALUES ('e1','A'); INSERT INTO event_map_type VALUES ('A','A');"
      "CREATE TABLE event_A (ocel_id TEXT, ocel_time TIMESTAMP, x TEXT);"
      "INSERT INTO event_A VALUES ('e1','2024-01-01 00:00:00+00:00','v');"
      "INSERT INTO object VALUES ('o1','T'); INSERT INTO object VALUES ('o2','U');"
      "INSERT INTO object_map_type VALUES ('T','T'); INSERT INTO object_map_type VALUES ('U','U');"
      "INSERT INTO event_object VALUES ('e1','o1','q'); INSERT INTO event_object VALUES ('e1','o2','q');"
      "CREATE TABLE object_T (ocel_id TEXT, b INTEGER, ocel_changed_field TEXT, ocel_time TIMESTAMP);"
      "CREATE TABLE object_U (ocel_id TEXT, c REAL, ocel_changed_field TEXT, ocel_time TIMESTAMP);"
      "INSERT INTO object_T VALUES ('o1',1,NULL,NULL); INSERT INTO object_U VALUES ('o2',1.5,NULL,NULL);");
  OcelXmlExportOptions options;
  options.format = OcelXmlFormat::kStandard;
  const auto result = filter_ocel_event_attribute_exact(exact_input(log), "x", {std::string("v")});
  const auto xml = serialize_ocel_xml(result, options);
  expect(contains(xml, ">1.0</attribute>") && !contains(xml, ">1</attribute>"),
         "FA1-6: write_ocel2_xml prints b 1.0: " + xml);
}
#endif

const char* const kFrameJson = R"({"ocel:global-event":{"ocel:activity":"zz"},"ocel:global-object":{"ocel:type":"__INVALID__"},
"ocel:global-log":{"ocel:attribute-names":["k","x1","p","q"],"ocel:object-types":["t","u"],"ocel:version":"1.0","ocel:ordering":"timestamp"},
"ocel:events":{
 "e1":{"ocel:activity":"a","ocel:timestamp":"2024-01-01T10:00:00+00:00","ocel:omap":["o1","o3"],"ocel:vmap":{"k":1}},
 "e2":{"ocel:activity":"b","ocel:timestamp":"2024-01-02T10:00:00+00:00","ocel:omap":["o2","o4"],"ocel:vmap":{"k":2,"x1":"z"}}},
"ocel:objects":{
 "o1":{"ocel:type":"t","ocel:ovmap":{"p":1}},
 "o2":{"ocel:type":"u","ocel:ovmap":{"p":2.5,"q":"w"}},
 "o3":{"ocel:type":"t","ocel:ovmap":{"p":3}},
 "o4":{"ocel:type":"u","ocel:ovmap":{}}}})";

// FA1-2: x1 and q only the removed rows (e2, o2) held.
void exact_filters_find_removed_columns() {
  const auto types = filter_ocel_object_types(parse_ocel_json(kFrameJson), {"t"});
  const auto input = exact_input(types);
  const auto none = filter_ocel_event_attribute_exact(input, "x1", {std::string("z")});
  expect(none.events.empty() && none.objects.empty(), "FA1-2: event_attribute_exact on x1 selects nothing");
  const auto all = filter_ocel_event_attribute_exact(input, "x1", {std::string("z")}, false);
  expect(event_ids(all) == std::vector<std::string>{"e1"} &&
             object_ids(all) == std::vector<std::string>{"o1", "o3"},
         "FA1-2: negated, it keeps e1 with o1 and o3");
  const auto objects = filter_ocel_object_attribute_exact(input, "q", {std::string("w")});
  expect(objects.objects.empty(), "FA1-2: object_attribute_exact on q selects nothing");
  expect(error_of([&] { (void)filter_ocel_event_attribute_exact(input, "nope", {std::string("z")}); }) == "'nope'",
         "FA1-2: a key no frame holds still raises KeyError");
}

const char* const kXmlHead =
    "<?xml version='1.0' encoding='UTF-8'?>\n<log>\n"
    "<global scope=\"event\"><string key=\"activity\" value=\"__INVALID__\"/></global>\n"
    "<global scope=\"object\"><string key=\"type\" value=\"__INVALID__\"/></global>\n"
    "<global scope=\"log\"><list key=\"attribute-names\"/><list key=\"object-types\"/>"
    "<string key=\"version\" value=\"1.0\"/><string key=\"ordering\" value=\"timestamp\"/></global>\n";

// t1/f3.xmlocel.
const std::string kFilterXml =
    std::string(kXmlHead) +
    "<events>"
    "<event><string key=\"id\" value=\"e0\"/><string key=\"activity\" value=\"a\"/>"
    "<date key=\"timestamp\" value=\"2024-02-15T11:30:50+00:00\"/><list key=\"omap\">"
    "<string key=\"object-id\" value=\"o0\"/><string key=\"object-id\" value=\"o1\"/>"
    "<string key=\"object-id\" value=\"o2\"/><string key=\"object-id\" value=\"o3\"/></list>"
    "<list key=\"vmap\"><date key=\"k\" value=\"2024-01-01T00:00:00Z\"/></list></event>"
    "<event><string key=\"id\" value=\"e1\"/><string key=\"activity\" value=\"a\"/>"
    "<date key=\"timestamp\" value=\"2024-02-16T11:30:50+00:00\"/><list key=\"omap\">"
    "<string key=\"object-id\" value=\"o0\"/></list><list key=\"vmap\"><float key=\"k\" value=\"2.5\"/></list></event>"
    "</events><objects>"
    "<object><string key=\"id\" value=\"o0\"/><string key=\"type\" value=\"t\"/><list key=\"ovmap\">"
    "<string key=\"z\" value=\"\"/></list></object>"
    "<object><string key=\"id\" value=\"o1\"/><string key=\"type\" value=\"t\"/><list key=\"ovmap\">"
    "<float key=\"z\" value=\"1.5\"/></list></object>"
    "<object><string key=\"id\" value=\"o2\"/><string key=\"type\" value=\"t\"/><list key=\"ovmap\">"
    "<date key=\"z\" value=\"2024-02-04T01:02:03.5Z\"/></list></object>"
    "<object><string key=\"id\" value=\"o3\"/><string key=\"type\" value=\"t\"/><list key=\"ovmap\">"
    "<string key=\"z\" value=\"2024-02-01T10:00:00Z\"/></list></object>"
    "</objects></log>\n";

void exact_filters_compare_texts_and_numbers_under_dates() {
  const auto input = exact_input(parse_ocel_xml(kFilterXml));
  const auto instant = [](const std::string& text) { return *parse_timestamp_exact(text); };
  const std::vector<std::pair<OcelObjectAttributeFilterValue, std::vector<std::string>>> cases{
      {std::string(""), {"o0"}},
      {1.5, {"o1"}},
      {instant("2024-02-04T01:02:03.5Z"), {"o2"}},
      {std::string("2024-02-01T10:00:00Z"), {"o3"}},
      {std::string("2024-02-04T01:02:03.5Z"), {}}};
  const std::vector<std::string> every{"o0", "o1", "o2", "o3"};
  for (std::size_t index = 0; index < cases.size(); ++index) {
    const auto& [value, expected] = cases[index];
    const auto kept = filter_ocel_object_attribute_exact(input, "z", {value});
    expect(object_ids(kept) == expected, "FA1-3: object_attribute_exact z, case " + std::to_string(index));
    std::vector<std::string> rest;
    for (const auto& id : every)
      if (std::find(expected.begin(), expected.end(), id) == expected.end()) rest.push_back(id);
    const auto negated = filter_ocel_object_attribute_exact(input, "z", {value}, false);
    expect(object_ids(negated) == rest, "FA1-3: negated, case " + std::to_string(index));
  }
  expect(event_ids(filter_ocel_event_attribute_exact(input, "k", {2.5})) == std::vector<std::string>{"e1"},
         "FA1-3: event_attribute_exact k 2.5 keeps e1 (a <float> under a <date> key)");
  expect(event_ids(filter_ocel_event_attribute_exact(input, "k", {instant("2024-01-01T00:00:00Z")})) ==
             std::vector<std::string>{"e0"},
         "FA1-3: event_attribute_exact k at its date keeps e0");
}

// t1/c1.xmlocel: o0's z is a <date>, then a <string> change; o1's z a <date>
// and a <date> change; o0's w a <string> and a change.
const std::string kChangeXml =
    std::string(kXmlHead) +
    "<events><event><string key=\"id\" value=\"e0\"/><string key=\"activity\" value=\"a\"/>"
    "<date key=\"timestamp\" value=\"2024-02-15T11:30:50+00:00\"/><list key=\"omap\">"
    "<string key=\"object-id\" value=\"o0\"/><string key=\"object-id\" value=\"o1\"/></list>"
    "<list key=\"vmap\"></list></event></events><objects>"
    "<object><string key=\"id\" value=\"o0\"/><string key=\"type\" value=\"t\"/><list key=\"ovmap\">"
    "<date key=\"z\" value=\"2024-03-01\"/>"
    "<string key=\"z\" value=\"2024-02-01T10:00:00Z\" timestamp=\"2024-01-05T00:00:00Z\"/>"
    "<string key=\"w\" value=\"x\"/><string key=\"w\" value=\"y\" timestamp=\"2024-01-06T00:00:00Z\"/></list></object>"
    "<object><string key=\"id\" value=\"o1\"/><string key=\"type\" value=\"t\"/><list key=\"ovmap\">"
    "<date key=\"z\" value=\"2024-03-02T00:00:00Z\"/>"
    "<date key=\"z\" value=\"2024-02-02T10:00:00Z\" timestamp=\"2024-01-07T00:00:00Z\"/></list></object>"
    "</objects></log>\n";

void xml_changes() {
  const auto read = parse_ocel_xml(kChangeXml);
  expect(read.object_changes.size() == 3 && read.object_changes[0].text && !read.object_changes[1].text &&
             !read.object_changes[2].text,
         "FA1-5: the <string> change under z keeps its text mark");
  std::vector<std::pair<std::string, Ocel>> reads{{"read", read},
                                                  {"classic transport", route_input(read)},
                                                  {"standard transport", route_input(read, OcelJsonFormat::kStandard)},
                                                  {"exact input", exact_input(read)}};
  for (const auto& [name, log] : reads) {
    expect(log.object_changes.size() == 3 && log.object_changes[0].text,
           "FA1-5: " + name + " keeps the change's text mark");
    const auto classic = serialize_ocel_json(log, OcelJsonFormat::kClassic, false);
    expect(contains(classic, "\"ocel:field\":\"z\",\"z\":\"2024-02-01T10:00:00Z\"") &&
               contains(classic, "\"ocel:field\":\"z\",\"z\":\"2024-02-02T10:00:00+00:00\""),
           "FA1-5: write_ocel_json prints the text as written, the date's isoformat (" + name + "): " + classic);
    const auto standard = serialize_ocel_json(log, OcelJsonFormat::kStandard, false);
    expect(contains(standard, "{\"name\":\"z\",\"value\":\"2024-02-01T10:00:00Z\",\"time\":\"2024-01-05T00:00:00+00:00\"}"),
           "FA1-5: write_ocel2_json likewise (" + name + "): " + standard);
    OcelXmlExportOptions options;
    options.format = OcelXmlFormat::kStandard;
    const auto xml = serialize_ocel_xml(log, options);
    expect(contains(xml, "time=\"2024-01-05T00:00:00+00:00\" type=\"string\">2024-02-01T10:00:00Z</attribute>"),
           "FA1-5: write_ocel2_xml likewise (" + name + "): " + xml);
  }
#if defined(MINERA_HAS_SQLITE)
  // FA1-4 and FA1-5 together: pm4py's object_T.
  for (const auto& [name, log] : reads) {
    const auto written = written_table(log, "t");
    expect(written == "ocel_id TEXT,z TEXT,w TEXT,ocel_time TIMESTAMP,ocel_changed_field TEXT,"
                      "|o0,2024-03-01T00:00:00+00:00,x,NULL,NULL|o1,2024-03-02T00:00:00+00:00,NULL,NULL,NULL"
                      "|o0,2024-02-01T10:00:00Z,NULL,2024-01-05 00:00:00+00:00,z"
                      "|o0,NULL,y,2024-01-06 00:00:00+00:00,w"
                      "|o1,2024-02-02T10:00:00+00:00,NULL,2024-01-07 00:00:00+00:00,z",
           "FA1-4/5: write_ocel2_sqlite's object_T (" + name + "): " + written);
  }
#endif
}

}  // namespace

int main() {
#if defined(MINERA_HAS_SQLITE)
  exact_filters_keep_change_cells();
  xml2_prints_float_column_ints();
#endif
  exact_filters_find_removed_columns();
  exact_filters_compare_texts_and_numbers_under_dates();
  xml_changes();
  if (failures) {
    std::cerr << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "review FA1 tests passed\n";
  return 0;
}
