// Review ET1: pm4py 2.7.22's OCEL readers on columns named like the frames'
// own columns (review-harness round-et: hc/t1 and the probes under hc run
// these cases through pm4py). Observed black-box:
//
// - read_ocel2_sqlite renames the object tables' ocel_changed_field to
//   "ocel:field" after concatenating them, so when no object table has an
//   ocel_changed_field column an "ocel:field" column is their changed
//   field: a row with one is a change (its object is not read from it), and
//   change rows without a time column in any object table raise KeyError
//   'ocel:timestamp';
// - write_ocel2_json leaves out a change whose value is missing (NaN, or a
//   field without a column) before it converts the change times, so a NaT
//   time of such a change does not raise KeyError 'ocel:timestamp';
// - write_ocel_xml writes one omap element per relation row, so an object
//   related to an event twice (two qualifiers) is listed twice.
//
// See docs/reference-2.7.22-parity.md, Review ET1.
#include "minera/ocel.hpp"
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

[[maybe_unused]] std::string error_of(const std::function<void()>& run) {
  try {
    run();
  } catch (const std::exception& error) {
    return error.what();
  }
  return {};
}

[[maybe_unused]] bool has_attribute(const std::vector<OcelAttribute>& attributes, const std::string& key) {
  for (const auto& attribute : attributes)
    if (attribute.key == key) return true;
  return false;
}

#if defined(MINERA_HAS_SQLITE)

void exec_sql(sqlite3* db, const std::string& sql) {
  char* error = nullptr;
  if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &error) != SQLITE_OK) {
    const std::string message = error != nullptr ? error : "unknown";
    sqlite3_free(error);
    throw std::runtime_error("fixture SQL failed: " + message);
  }
}

// A pm4py-shaped file (one event type, object types T and U, a change of
// o1); `sql` runs after (probe et t1 mk.py).
std::filesystem::path reader_case(const std::string& name, const std::string& sql) {
  const auto path = std::filesystem::temp_directory_path() / ("minera_review_et1_" + name + ".sqlite");
  std::filesystem::remove(path);
  sqlite3* db = nullptr;
  if (sqlite3_open(path.string().c_str(), &db) != SQLITE_OK) throw std::runtime_error("cannot open fixture");
  exec_sql(db, R"(
    CREATE TABLE event (ocel_id TEXT, ocel_type TEXT); INSERT INTO event VALUES ('e1','x'),('e2','x');
    CREATE TABLE object (ocel_id TEXT, ocel_type TEXT); INSERT INTO object VALUES ('o1','A'),('o2','B'),('o3','A');
    CREATE TABLE event_map_type (ocel_type TEXT, ocel_type_map TEXT); INSERT INTO event_map_type VALUES ('x','X');
    CREATE TABLE object_map_type (ocel_type TEXT, ocel_type_map TEXT); INSERT INTO object_map_type VALUES ('A','A'),('B','B');
    CREATE TABLE event_object (ocel_event_id TEXT, ocel_object_id TEXT, ocel_qualifier TEXT);
    INSERT INTO event_object VALUES ('e1','o1','q'),('e1','o2',''),('e2','o3','q');
    CREATE TABLE object_object (ocel_source_id TEXT, ocel_target_id TEXT, ocel_qualifier TEXT);
    CREATE TABLE event_X (ocel_id TEXT, ocel_time TIMESTAMP, "ocel:activity" TEXT, v INTEGER);
    INSERT INTO event_X VALUES ('e1','2024-01-01 10:00:00+00:00','x',1),('e2','2024-01-02 10:00:00+00:00','x',2);
    CREATE TABLE object_A (ocel_id TEXT, a1 TEXT, ocel_changed_field TEXT, ocel_time TIMESTAMP);
    INSERT INTO object_A VALUES ('o1','s',NULL,NULL),('o3','u',NULL,NULL),('o1','t','a1','2024-01-05 00:00:00+00:00');
    CREATE TABLE object_B (ocel_id TEXT); INSERT INTO object_B VALUES ('o2');)");
  if (!sql.empty()) exec_sql(db, sql);
  sqlite3_close(db);
  return path;
}

// read_ocel2_sqlite: an "ocel:field" column is the changed field when no
// object table has an ocel_changed_field column (probe et t1: f3, f7, f8;
// bank h65's h6403_0004).
void sqlite_reader_field_alias() {
  const auto read = [](const std::string& name, const std::string& sql) {
    const auto path = reader_case(name, sql);
    auto log = import_ocel_sqlite(path);
    std::filesystem::remove(path);
    return log;
  };
  for (const auto& [name, sql] : std::vector<std::pair<std::string, std::string>>{
           {"field", R"(ALTER TABLE object_A RENAME COLUMN ocel_changed_field TO "ocel:field")"},
           {"field_time", R"(ALTER TABLE object_A RENAME COLUMN ocel_changed_field TO "ocel:field";
                            ALTER TABLE object_A RENAME COLUMN ocel_time TO "ocel:timestamp")"}}) {
    const auto log = read(name, sql);
    expect(log.objects.size() == 3, name + ": three objects");
    expect(log.object_changes.size() == 1 && log.object_changes[0].object_id == "o1" &&
               log.object_changes[0].field == "a1" &&
               std::get<std::string>(log.object_changes[0].value) == "t" &&
               log.object_changes[0].timestamp.rfind("2024-01-05", 0) == 0,
           name + ": the ocel:field row is o1's change of a1");
    bool field_attribute = false;
    for (const auto& object : log.objects) field_attribute = field_attribute || has_attribute(object.attributes, "ocel:field");
    for (const auto& [type, schema] : log.object_type_attributes) field_attribute = field_attribute || schema.contains("ocel:field");
    expect(!field_attribute, name + ": ocel:field is no attribute");
  }
  {
    // A row with a field in a table without a time column is a change, so
    // its object is not read (o2 and its relation are dropped).
    const auto log = read("field_other", R"(ALTER TABLE object_A RENAME COLUMN ocel_changed_field TO "ocel:field";
                                            ALTER TABLE object_B ADD COLUMN "ocel:field" TEXT;
                                            UPDATE object_B SET "ocel:field" = 'a1')");
    bool o2 = false;
    for (const auto& object : log.objects) o2 = o2 || object.id == "o2";
    expect(!o2 && log.objects.size() == 2, "a field cell makes o2's only row a change");
    expect(log.object_changes.size() == 1, "the untimed change row of B is not kept");
  }
  {
    // No time column in any object table: change rows raise.
    const auto path = reader_case("field_untimed", R"(DROP TABLE object_A;
        CREATE TABLE object_A (ocel_id TEXT, "ocel:field" TIMESTAMP);
        INSERT INTO object_A VALUES ('o1','2024-02-02 00:00:00+00:00'),('o3',NULL))");
    const auto error = error_of([&] { (void)import_ocel_sqlite(path); });
    std::filesystem::remove(path);
    expect(error == "KeyError: 'ocel:timestamp'", "untimed ocel:field rows raise: " + error);
  }
  {
    // Next to an ocel_changed_field column in another table the split
    // raises (Review ES1-6).
    const auto path = reader_case("field_both", R"(ALTER TABLE object_B ADD COLUMN "ocel:field" TEXT)");
    const auto error = error_of([&] { (void)import_ocel_sqlite(path); });
    std::filesystem::remove(path);
    expect(error == "ValueError: cannot reindex on an axis with duplicate labels", "ocel:field next to a changed field: " + error);
  }
}

// write_ocel2_json and NaT change times (probe et t4 nat.py: v1, v2, v5;
// fuzz file h87_0028).
void json2_nat_changes() {
  const auto export_error = [](const std::string& name, const std::string& sql) {
    const auto path = reader_case(name, sql);
    const auto log = import_ocel_sqlite(path);
    std::filesystem::remove(path);
    return error_of([&] { (void)serialize_ocel_json(log, OcelJsonFormat::kStandard, false); });
  };
  const std::string nat = "UPDATE object_A SET ocel_time = NULL WHERE ocel_changed_field IS NOT NULL";
  expect(export_error("nat_value", nat) == "KeyError: 'ocel:timestamp'", "a NaT change with a value raises");
  expect(export_error("nat_nan", nat + "; UPDATE object_A SET a1 = NULL WHERE ocel_changed_field IS NOT NULL").empty(),
         "a NaT change without a value is left out");
  expect(export_error("nat_nocolumn", nat + "; UPDATE object_A SET ocel_changed_field = 'zz' WHERE ocel_changed_field IS NOT NULL")
             .empty(),
         "a NaT change of a field without a column is left out");
}

// write_ocel_xml lists o1 twice in e1's omap when o1 is related to e1 with
// two qualifiers (probe et t5 om.py; fuzz files h88_0018, h88_0183).
void classic_xml_omap_rows() {
  const auto path = reader_case("omap", "INSERT INTO event_object VALUES ('e1','o1','r')");
  const auto log = import_ocel_sqlite(path);
  std::filesystem::remove(path);
  expect(log.relations.size() == 4, "both relations of e1 and o1 are read");
  OcelXmlExportOptions classic;
  classic.allow_lossy = true;
  const auto xml = serialize_ocel_xml(log, classic);
  const std::string omap =
      "<list key=\"omap\">\n"
      "        <string key=\"object-id\" value=\"o1\"/>\n"
      "        <string key=\"object-id\" value=\"o2\"/>\n"
      "        <string key=\"object-id\" value=\"o1\"/>\n"
      "      </list>";
  expect(xml.find(omap) != std::string::npos, "e1's omap lists o1 twice: " + xml.substr(0, 1200));
}

#endif

}  // namespace

int main() {
#if defined(MINERA_HAS_SQLITE)
  sqlite_reader_field_alias();
  json2_nat_changes();
  classic_xml_omap_rows();
#endif
  if (failures != 0) {
    std::cerr << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "review ET1 tests passed\n";
  return 0;
}
