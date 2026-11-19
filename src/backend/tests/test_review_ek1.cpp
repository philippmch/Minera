// Review EK1: pm4py 2.7.22's OCEL writers and its OCEL 2.0 SQLite reader on
// the round-EK banks (review-harness round-ek: the spoiled OCEL 2.0 JSON bank
// b52 through rt/rsw2.py, the spoiled SQLite bank b53 from s/sgen.py, the
// probes t/ and s/p9.py). Observed black-box:
//
// - the writers type and spell a column by its pandas dtype: a declared date
//   column holding a text next to dates is an object column, declared
//   "string" by write_ocel2_json, its dates printed as isoformat ("+00:00");
//   a float-declared "nan" is NaN, a missing cell the writers leave out;
// - read_ocel2_sqlite reads the event and object tables, sorts each one's
//   distinct types (a NULL type next to texts is NaN, so sorted() raises
//   "'<' not supported between instances of ..." in CPython's operand
//   order), reads both map tables (a dict in row order), then each event
//   type's table and each object type's in sorted order (KeyError for an
//   unmapped type, Python's concatenation error for a NULL entry,
//   pandas.read_sql's "Execution failed on sql" for a missing table), then
//   concatenates (No objects to concatenate) and reads event_object and
//   object_object;
// - write_ocel2_xml takes each change time's isoformat in log order: a
//   change-time column holding a text pm4py could not read raises
//   AttributeError on the first such text or missing time ('NoneType' next
//   to read dates, 'float' without any); pandas' NaT strings ("NaT", "nan",
//   ...) are missing times; write_ocel2_sqlite writes NULL for those times.
//
// See docs/reference-2.7.22-parity.md, Review EK1.
#include "minera/ocel.hpp"
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

std::string error_of(const std::function<void()>& call) {
  try {
    call();
  } catch (const std::exception& error) {
    return error.what();
  }
  return "";
}

bool contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

void json_writer_types() {
  const auto log = parse_ocel_json(
      "{\"objectTypes\":[{\"name\":\"t\",\"attributes\":[{\"name\":\"d\",\"type\":\"date\"},{\"name\":\"f\","
      "\"type\":\"float\"}]}],\"eventTypes\":[{\"name\":\"a\",\"attributes\":[]}],"
      "\"objects\":[{\"id\":\"o1\",\"type\":\"t\",\"attributes\":[{\"name\":\"d\",\"time\":\"1970-01-01T00:00:00Z\","
      "\"value\":\"2024-01-01T00:00:00Z\"},{\"name\":\"f\",\"time\":\"1970-01-01T00:00:00Z\",\"value\":\"nan\"}]},"
      "{\"id\":\"o2\",\"type\":\"t\",\"attributes\":[{\"name\":\"d\",\"time\":\"1970-01-01T00:00:00Z\",\"value\":"
      "\"abc\"},{\"name\":\"f\",\"time\":\"1970-01-01T00:00:00Z\",\"value\":\"2.5\"}]}],"
      "\"events\":[{\"id\":\"e1\",\"type\":\"a\",\"time\":\"2024-01-01T00:00:00Z\",\"relationships\":["
      "{\"objectId\":\"o1\",\"qualifier\":\"q\"},{\"objectId\":\"o2\",\"qualifier\":\"q\"}]}]}");
  const auto standard = serialize_ocel_json(log, OcelJsonFormat::kStandard, false);
  expect(contains(standard, "{\"name\":\"d\",\"type\":\"string\"},{\"name\":\"f\",\"type\":\"float\"}"),
         "a date column holding a text is declared string");
  expect(contains(standard, "{\"id\":\"o1\",\"type\":\"t\",\"attributes\":[{\"name\":\"d\",\"time\":"
                            "\"1970-01-01T00:00:00Z\",\"value\":\"2024-01-01T00:00:00+00:00\"}]"),
         "its date is printed as isoformat, the NaN float left out");
  const auto classic = serialize_ocel_json(log, OcelJsonFormat::kClassic, false);
  expect(contains(classic, "\"o1\":{\"ocel:type\":\"t\",\"ocel:ovmap\":{\"d\":\"2024-01-01T00:00:00+00:00\"}"),
         "write_ocel_json likewise");
  expect(contains(classic, "\"ocel:ovmap\":{\"d\":\"abc\",\"f\":2.5}"), "the text and the float");
}

std::string changes_json(const std::string& entries_o1, const std::string& entries_o2) {
  const std::string head = "{\"name\":\"a\",\"time\":\"1970-01-01T00:00:00Z\",\"value\":\"x\"}";
  return "{\"objectTypes\":[{\"name\":\"t\",\"attributes\":[{\"name\":\"a\",\"type\":\"string\"}]}],"
         "\"eventTypes\":[{\"name\":\"e\",\"attributes\":[]}],"
         "\"objects\":[{\"id\":\"o1\",\"type\":\"t\",\"attributes\":[" + head + entries_o1 + "]},"
         "{\"id\":\"o2\",\"type\":\"t\",\"attributes\":[" + head + entries_o2 + "]}],"
         "\"events\":[{\"id\":\"e1\",\"type\":\"e\",\"time\":\"2024-01-01T00:00:00Z\",\"relationships\":["
         "{\"objectId\":\"o1\",\"qualifier\":\"q\"},{\"objectId\":\"o2\",\"qualifier\":\"q\"}]}]}";
}

std::string change(const std::string& time, const std::string& value) {
  return ",{\"name\":\"a\"," + (time == "null" ? std::string() : "\"time\":" + time + ",") + "\"value\":\"" + value +
         "\"}";
}

void xml2_change_times() {
  OcelXmlExportOptions xml;
  xml.format = OcelXmlFormat::kStandard;
  const auto write = [&](const std::string& json) {
    return error_of([&] { (void)serialize_ocel_xml(parse_ocel_json(json), xml); });
  };
  const std::string date = "\"2024-01-05T00:00:00Z\"";
  expect(write(changes_json(change("\"abc\"", "y"), change("null", "z"))) ==
             "AttributeError: 'str' object has no attribute 'isoformat'",
         "an unreadable change time first");
  expect(write(changes_json(change("null", "z"), change("\"abc\"", "y"))) ==
             "AttributeError: 'float' object has no attribute 'isoformat'",
         "a missing time first, no read date (str dtype: NaN)");
  expect(write(changes_json(change(date, "w") + change("null", "z"), change("\"abc\"", "y"))) ==
             "AttributeError: 'NoneType' object has no attribute 'isoformat'",
         "a missing time first next to a read date (object column: None)");
  const auto nat_text = changes_json(change("\"NaT\"", "z"), change(date, "w"));
  expect(write(nat_text).empty(), "a NaT string is a missing time");
  const auto written = serialize_ocel_xml(parse_ocel_json(nat_text), xml);
  expect(contains(written, "time=\"NaT\""), "printed NaT");
  expect(write(changes_json(change("null", "z"), change(date, "w"))).empty(), "missing times alone print NaT");
}

#if defined(MINERA_HAS_SQLITE)
const char* kBase =
    "CREATE TABLE \"event\" (\"ocel_id\" TEXT, \"ocel_type\" TEXT);"
    "INSERT INTO \"event\" VALUES('e1','a');INSERT INTO \"event\" VALUES('e2','b');"
    "CREATE TABLE \"event_A\" (\"ocel_id\" TEXT, \"ocel_time\" TIMESTAMP, \"ocel:activity\" TEXT);"
    "INSERT INTO \"event_A\" VALUES('e1','2024-01-01 00:00:00+00:00','a');"
    "CREATE TABLE \"event_B\" (\"ocel_id\" TEXT, \"ocel_time\" TIMESTAMP, \"ocel:activity\" TEXT);"
    "INSERT INTO \"event_B\" VALUES('e2','2024-01-02 00:00:00+00:00','b');"
    "CREATE TABLE \"event_map_type\" (\"ocel_type\" TEXT, \"ocel_type_map\" TEXT);"
    "INSERT INTO \"event_map_type\" VALUES('a','A');INSERT INTO \"event_map_type\" VALUES('b','B');"
    "CREATE TABLE \"event_object\" (\"ocel_event_id\" TEXT, \"ocel_object_id\" TEXT, \"ocel_qualifier\" TEXT);"
    "INSERT INTO \"event_object\" VALUES('e1','o1','q');INSERT INTO \"event_object\" VALUES('e2','o1','q');"
    "CREATE TABLE \"object\" (\"ocel_id\" TEXT, \"ocel_type\" TEXT);"
    "INSERT INTO \"object\" VALUES('o1','t');"
    "CREATE TABLE \"object_T\" (\"ocel_id\" TEXT, \"ocel_changed_field\" TEXT, \"ocel_time\" TIMESTAMP);"
    "INSERT INTO \"object_T\" VALUES('o1',NULL,NULL);"
    "CREATE TABLE \"object_map_type\" (\"ocel_type\" TEXT, \"ocel_type_map\" TEXT);"
    "INSERT INTO \"object_map_type\" VALUES('t','T');"
    "CREATE TABLE \"object_object\" (\"ocel_source_id\" TEXT, \"ocel_target_id\" TEXT, \"ocel_qualifier\" TEXT);";

std::string read_spoiled(const std::string& spoil) {
  const auto path = std::filesystem::temp_directory_path() / "minera_review_ek1.sqlite";
  std::filesystem::remove(path);
  sqlite3* db = nullptr;
  if (sqlite3_open(path.string().c_str(), &db) != SQLITE_OK) return "open failed";
  char* error = nullptr;
  const auto status = sqlite3_exec(db, (std::string(kBase) + spoil).c_str(), nullptr, nullptr, &error);
  if (error != nullptr) sqlite3_free(error);
  sqlite3_close(db);
  if (status != SQLITE_OK) return "write failed";
  auto result = error_of([&] { (void)import_ocel_sqlite(path); });
  std::filesystem::remove(path);
  return result.empty() ? "OK" : result;
}

void sqlite_structure() {
  expect(read_spoiled("") == "OK", "the base file reads");
  expect(read_spoiled("UPDATE \"event\" SET ocel_type='zzz' WHERE rowid=2;"
                      "INSERT INTO \"event\" VALUES('e3','aaa');") == "KeyError: 'aaa'",
         "the first unmapped type by name");
  expect(read_spoiled("UPDATE \"event\" SET ocel_type='zzz' WHERE rowid=2;"
                      "UPDATE \"object\" SET ocel_type='qqq';") == "KeyError: 'zzz'",
         "the event types before the object types");
  expect(read_spoiled("UPDATE \"event_map_type\" SET ocel_type_map='Nope' WHERE rowid=1;") ==
             "Execution failed on sql 'SELECT * FROM \"event_Nope\"': no such table: event_Nope",
         "a mapped table that is missing");
  expect(read_spoiled("INSERT INTO \"event_map_type\" VALUES('a','Nope');") ==
             "Execution failed on sql 'SELECT * FROM \"event_Nope\"': no such table: event_Nope",
         "a later map row wins");
  expect(read_spoiled("INSERT INTO \"event_map_type\" VALUES('unused','Nope');") == "OK", "an unused map row");
  expect(read_spoiled("UPDATE \"event_map_type\" SET ocel_type_map=NULL WHERE rowid=1;") ==
             "can only concatenate str (not \"float\") to str",
         "a NULL map entry next to texts");
  expect(read_spoiled("UPDATE \"object_map_type\" SET ocel_type_map=NULL;") ==
             "can only concatenate str (not \"NoneType\") to str",
         "a map column of NULLs");
  expect(read_spoiled("UPDATE \"event\" SET ocel_type=NULL WHERE rowid=1;") ==
             "'<' not supported between instances of 'str' and 'float'",
         "a NULL type first");
  expect(read_spoiled("UPDATE \"event\" SET ocel_type=NULL WHERE rowid=2;") ==
             "'<' not supported between instances of 'float' and 'str'",
         "a NULL type later");
  expect(read_spoiled("UPDATE \"event\" SET ocel_type=NULL;") == "KeyError: None", "every type NULL");
  expect(read_spoiled("UPDATE \"event\" SET ocel_type='' WHERE rowid=2;") == "KeyError: ''", "an empty type");
  expect(read_spoiled("INSERT INTO \"object\" VALUES('o2',NULL);UPDATE \"event\" SET ocel_type='zzz';") ==
             "'<' not supported between instances of 'float' and 'str'",
         "both sorts before the lookups");
  expect(read_spoiled("DROP TABLE \"object_map_type\";UPDATE \"event\" SET ocel_type='zzz' WHERE rowid=1;") ==
             "Execution failed on sql 'SELECT * FROM object_map_type': no such table: object_map_type",
         "both map tables before the lookups");
  expect(read_spoiled("DROP TABLE \"event\";") ==
             "Execution failed on sql 'SELECT * FROM event': no such table: event",
         "a missing event table");
  expect(read_spoiled("DELETE FROM \"event\";UPDATE \"object\" SET ocel_type='qqq';") == "KeyError: 'qqq'",
         "the lookups before the concatenation");
  expect(read_spoiled("UPDATE \"event\" SET ocel_id=NULL;") == "OK", "types of id-less rows are still read");
  expect(read_spoiled("DROP TABLE \"object_object\";") ==
             "Execution failed on sql 'SELECT * FROM object_object': no such table: object_object",
         "a missing object_object table");
  expect(read_spoiled("DROP TABLE \"event_object\";DELETE FROM \"object\";") ==
             "ValueError: No objects to concatenate",
         "the concatenation before event_object");
}

void sqlite_change_times() {
  const auto log = parse_ocel_json(changes_json(change("\"abc\"", "y"), change("\"2024-01-05T00:00:00Z\"", "w")));
  const auto path = std::filesystem::temp_directory_path() / "minera_review_ek1_written.sqlite";
  std::filesystem::remove(path);
  export_ocel_sqlite(log, path);
  sqlite3* db = nullptr;
  std::vector<std::string> times;
  if (sqlite3_open(path.string().c_str(), &db) == SQLITE_OK) {
    // The object type table: the change rows' times.
    sqlite3_stmt* tables = nullptr;
    sqlite3_prepare_v2(db,
                       "SELECT name FROM sqlite_master WHERE type='table' AND name LIKE 'object_%' AND name NOT IN "
                       "('object_map_type','object_object','object_attributes','object_type_attributes',"
                       "'object_changes')",
                       -1, &tables, nullptr);
    while (sqlite3_step(tables) == SQLITE_ROW) {
      const std::string name = reinterpret_cast<const char*>(sqlite3_column_text(tables, 0));
      sqlite3_stmt* rows = nullptr;
      sqlite3_prepare_v2(db, ("SELECT ocel_time FROM \"" + name + "\" WHERE ocel_changed_field IS NOT NULL").c_str(),
                         -1, &rows, nullptr);
      while (sqlite3_step(rows) == SQLITE_ROW)
        times.push_back(sqlite3_column_type(rows, 0) == SQLITE_NULL
                            ? std::string("NULL")
                            : std::string(reinterpret_cast<const char*>(sqlite3_column_text(rows, 0))));
      sqlite3_finalize(rows);
    }
    sqlite3_finalize(tables);
    sqlite3_close(db);
  }
  std::filesystem::remove(path);
  expect(times == std::vector<std::string>{"NULL", "2024-01-05 00:00:00+00:00"},
         "write_ocel2_sqlite writes an unreadable change time as NULL");
}
#endif

}  // namespace

int main() {
  json_writer_types();
  xml2_change_times();
#if defined(MINERA_HAS_SQLITE)
  sqlite_structure();
  sqlite_change_times();
#endif
  if (failures != 0) {
    std::cerr << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "review EK1 tests passed\n";
  return 0;
}
