// Review EW1: number qualifiers in pm4py 2.7.22's OCEL 2.0 SQLite relation
// tables (review-harness round-ew: q/wq.py runs each case through pm4py's
// reader and every writer). Observed black-box:
//
// - read_ocel2_sqlite reads event_object and object_object with read_sql,
//   which types a qualifier column from its cells: ints only give an int64
//   column, a float or a NULL next to numbers a float64 one (an int prints
//   as a float there), a text cell an object column (ints and floats kept
//   as they are). NULLs become '' after that, so a float64 column with a
//   NULL holds floats and ''.
// - The writers keep the numbers: write_ocel2_json and write_ocel_json print
//   them as JSON numbers; write_ocel2_xml raises lxml's "TypeError: Argument
//   must be bytes or unicode, got 'int'" (or 'float') on an object's number
//   qualifier and prints an event's as its str; write_ocel2_sqlite declares
//   the column as to_sql infers it (INTEGER for ints only, REAL for floats
//   only, TEXT otherwise, where a number is written as its str).
//
// See docs/reference-2.7.22-parity.md, Review EW1.
#include "minera/ocel.hpp"
#include "minera/ocel_sqlite.hpp"
#include "minera/ocel_xml.hpp"

#include <filesystem>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
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

[[maybe_unused]] std::string error_of(const std::function<void()>& run) {
  try {
    run();
  } catch (const std::exception& error) {
    return error.what();
  }
  return {};
}

[[maybe_unused]] bool contains(const std::string& text, const std::string& part) {
  return text.find(part) != std::string::npos;
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

// wq.py's log: events e1, e2 of type A, objects o1, o2 of type T; the
// relation tables declare their qualifier NUMERIC, so number texts are
// stored as numbers.
Ocel qualifier_log(const std::string& name, const std::string& event_rows, const std::string& object_rows) {
  const auto path = std::filesystem::temp_directory_path() / ("minera_review_ew1_" + name + ".sqlite");
  std::filesystem::remove(path);
  sqlite3* db = nullptr;
  if (sqlite3_open(path.string().c_str(), &db) != SQLITE_OK) throw std::runtime_error("open the SQLite file");
  exec(db,
       "CREATE TABLE event (ocel_id TEXT, ocel_type TEXT); CREATE TABLE object (ocel_id TEXT, ocel_type TEXT);"
       "CREATE TABLE event_map_type (ocel_type TEXT, ocel_type_map TEXT);"
       "CREATE TABLE object_map_type (ocel_type TEXT, ocel_type_map TEXT);"
       "CREATE TABLE event_object (ocel_event_id TEXT, ocel_object_id TEXT, ocel_qualifier NUMERIC);"
       "CREATE TABLE object_object (ocel_source_id TEXT, ocel_target_id TEXT, ocel_qualifier NUMERIC);"
       "CREATE TABLE event_A (ocel_id TEXT, ocel_time TIMESTAMP); CREATE TABLE object_T (ocel_id TEXT);"
       "INSERT INTO event VALUES ('e1','A'),('e2','A'); INSERT INTO object VALUES ('o1','T'),('o2','T');"
       "INSERT INTO event_map_type VALUES ('A','A'); INSERT INTO object_map_type VALUES ('T','T');"
       "INSERT INTO event_A VALUES ('e1','2024-01-01 10:00:00'),('e2','2024-01-02 10:00:00');"
       "INSERT INTO object_T VALUES ('o1'),('o2');"
       "INSERT INTO event_object VALUES " + event_rows + ";"
       "INSERT INTO object_object VALUES " + object_rows + ";");
  sqlite3_close(db);
  auto log = import_ocel_sqlite(path);
  std::filesystem::remove(path);
  return log;
}

using Kinds = std::vector<std::pair<std::string, OcelQualifierKind>>;

Kinds relation_kinds(const Ocel& log) {
  Kinds kinds;
  for (const auto& relation : log.relations) kinds.emplace_back(relation.qualifier, relation.qualifier_kind);
  return kinds;
}

Kinds o2o_kinds(const Ocel& log) {
  Kinds kinds;
  for (const auto& relation : log.o2o) kinds.emplace_back(relation.qualifier, relation.qualifier_kind);
  return kinds;
}

constexpr auto kStr = OcelQualifierKind::kStr;
constexpr auto kInt = OcelQualifierKind::kInt;
constexpr auto kFloat = OcelQualifierKind::kFloat;

std::string xml2_error(const Ocel& log) {
  OcelXmlExportOptions options;
  options.format = OcelXmlFormat::kStandard;
  return error_of([&] { (void)serialize_ocel_xml(log, options); });
}

// The qualifier column of a written SQLite file: its declaration and its
// cells with their storage class.
std::pair<std::string, std::vector<std::string>> written_column(const Ocel& log, const std::string& table) {
  const auto path = std::filesystem::temp_directory_path() / "minera_review_ew1_written.sqlite";
  std::filesystem::remove(path);
  export_ocel_sqlite(log, path);
  sqlite3* db = nullptr;
  if (sqlite3_open(path.string().c_str(), &db) != SQLITE_OK) throw std::runtime_error("open the written file");
  std::string declared;
  std::vector<std::string> cells;
  sqlite3_stmt* statement = nullptr;
  const std::string info = "SELECT type FROM pragma_table_info('" + table + "') WHERE name = 'ocel_qualifier'";
  if (sqlite3_prepare_v2(db, info.c_str(), -1, &statement, nullptr) == SQLITE_OK &&
      sqlite3_step(statement) == SQLITE_ROW)
    declared = reinterpret_cast<const char*>(sqlite3_column_text(statement, 0));
  sqlite3_finalize(statement);
  const std::string select = "SELECT typeof(ocel_qualifier), ocel_qualifier FROM " + table + " ORDER BY rowid";
  if (sqlite3_prepare_v2(db, select.c_str(), -1, &statement, nullptr) == SQLITE_OK)
    while (sqlite3_step(statement) == SQLITE_ROW)
      cells.push_back(std::string(reinterpret_cast<const char*>(sqlite3_column_text(statement, 0))) + ":" +
                      reinterpret_cast<const char*>(sqlite3_column_text(statement, 1)));
  sqlite3_finalize(statement);
  sqlite3_close(db);
  std::filesystem::remove(path);
  return {declared, cells};
}

void mixed_object_column() {
  const auto log = qualifier_log("mixed", "('e1','o1','2'),('e1','o2','q'),('e2','o1','2.5')",
                                 "('o1','o2','7'),('o2','o1','x')");
  for (const auto& read : {log, parse_ocel_json(serialize_ocel_json(log, OcelJsonFormat::kClassic, true))}) {
    expect(relation_kinds(read) == Kinds{{"2", kInt}, {"q", kStr}, {"2.5", kFloat}},
           "a text cell keeps an object column: the int 2, 'q' and the float 2.5");
    expect(o2o_kinds(read) == Kinds{{"7", kInt}, {"x", kStr}}, "the object_object column likewise");
  }
  const auto standard = serialize_ocel_json(log, OcelJsonFormat::kStandard, false);
  expect(contains(standard, "\"qualifier\":2}") && contains(standard, "\"qualifier\":2.5}") &&
             contains(standard, "\"qualifier\":\"q\"}") && contains(standard, "\"qualifier\":7}"),
         "write_ocel2_json prints the numbers as numbers");
  const auto classic = serialize_ocel_json(log, OcelJsonFormat::kClassic, false);
  expect(contains(classic, "\"ocel:qualifier\":2}") && contains(classic, "\"ocel:qualifier\":7}"),
         "write_ocel_json too, in the typed omap and the o2o lists");
  expect(xml2_error(log) == "TypeError: Argument must be bytes or unicode, got 'int'",
         "write_ocel2_xml raises on the object's int qualifier");
  const auto [declared, cells] = written_column(log, "event_object");
  expect(declared == "TEXT" && cells == std::vector<std::string>{"text:2", "text:q", "text:2.5"},
         "write_ocel2_sqlite writes an object column as TEXT, numbers as their str");
}

void int_and_float_columns() {
  const auto ints = qualifier_log("allint", "('e1','o1','2'),('e1','o2','3'),('e2','o1','2')",
                                  "('o1','o2','7'),('o2','o1','8')");
  expect(relation_kinds(ints) == Kinds{{"2", kInt}, {"3", kInt}, {"2", kInt}}, "ints only: an int64 column");
  expect(o2o_kinds(ints) == Kinds{{"7", kInt}, {"8", kInt}}, "the object_object int64 column");
  const auto [int_declared, int_cells] = written_column(ints, "event_object");
  expect(int_declared == "INTEGER" && int_cells == std::vector<std::string>{"integer:2", "integer:3", "integer:2"},
         "an int64 column is written INTEGER");
  const auto [o2o_declared, o2o_cells] = written_column(ints, "object_object");
  expect(o2o_declared == "INTEGER" && o2o_cells == std::vector<std::string>{"integer:7", "integer:8"},
         "the object_object int64 column is written INTEGER");

  const auto floats = qualifier_log("intflt", "('e1','o1','2'),('e1','o2','3.5')", "('o1','o2','7'),('o2','o1','1.5')");
  expect(relation_kinds(floats) == Kinds{{"2.0", kFloat}, {"3.5", kFloat}},
         "an int next to a float: a float64 column, 2 prints 2.0");
  expect(o2o_kinds(floats) == Kinds{{"7.0", kFloat}, {"1.5", kFloat}}, "the object_object float64 column");
  expect(xml2_error(floats) == "TypeError: Argument must be bytes or unicode, got 'float'",
         "write_ocel2_xml raises on the object's float qualifier");
  const auto [float_declared, float_cells] = written_column(floats, "event_object");
  expect(float_declared == "REAL" && float_cells == std::vector<std::string>{"real:2.0", "real:3.5"},
         "a float64 column is written REAL");

  const auto big = qualifier_log("allflt", "('e1','o1','2.5'),('e1','o2','3.0'),('e2','o1','1e20')",
                                 "('o1','o2','7.25'),('o2','o1','2.0')");
  expect(relation_kinds(big) == Kinds{{"2.5", kFloat}, {"3.0", kFloat}, {"1e+20", kFloat}},
         "floats print as Python's repr");
  const auto standard = serialize_ocel_json(big, OcelJsonFormat::kStandard, false);
  expect(contains(standard, "\"qualifier\":1e+20}") && contains(standard, "\"qualifier\":3.0}"),
         "write_ocel2_json prints the floats as json.dumps does");
}

void null_cells() {
  const auto ints = qualifier_log("nullint", "('e1','o1','2'),('e1','o2',NULL),('e2','o1','3')",
                                  "('o1','o2','7'),('o2','o1',NULL)");
  expect(relation_kinds(ints) == Kinds{{"2.0", kFloat}, {"", kStr}, {"3.0", kFloat}},
         "a NULL next to ints: a float64 column whose NULL becomes ''");
  expect(o2o_kinds(ints) == Kinds{{"7.0", kFloat}, {"", kStr}}, "the object_object column likewise");
  const auto [declared, cells] = written_column(ints, "event_object");
  expect(declared == "TEXT" && cells == std::vector<std::string>{"text:2.0", "text:", "text:3.0"},
         "floats next to '' are written as TEXT");
  const auto texts = qualifier_log("nulltxt", "('e1','o1','a'),('e1','o2',NULL),('e2','o1','2')",
                                   "('o1','o2','x'),('o2','o1',NULL)");
  expect(relation_kinds(texts) == Kinds{{"a", kStr}, {"", kStr}, {"2", kInt}},
         "a NULL next to a text: an object column keeping the int");
  expect(o2o_kinds(texts) == Kinds{{"x", kStr}, {"", kStr}}, "texts and a NULL stay texts");
  expect(xml2_error(texts).empty(), "an event's number qualifier prints in the OCEL 2.0 XML");
  OcelXmlExportOptions options;
  options.format = OcelXmlFormat::kStandard;
  expect(contains(serialize_ocel_xml(texts, options), "object-id=\"o1\" qualifier=\"2\""),
         "as its str");
}
#endif

}  // namespace

int main() {
#if defined(MINERA_HAS_SQLITE)
  mixed_object_column();
  int_and_float_columns();
  null_cells();
#endif
  if (failures != 0) {
    std::cerr << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "review EW1 tests passed\n";
  return 0;
}
