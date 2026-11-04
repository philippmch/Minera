#include "minera/ocel_sqlite.hpp"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
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

namespace {

template <typename F>
void throws(F&& function) {
  bool rejected = false;
  try {
    function();
  } catch (const std::exception&) {
    rejected = true;
  }
  assert(rejected);
}

#if defined(MINERA_HAS_SQLITE)
void exec_sql(sqlite3* db, const char* sql) {
  char* error = nullptr;
  const auto status = sqlite3_exec(db, sql, nullptr, nullptr, &error);
  if (status != SQLITE_OK) {
    if (error != nullptr) sqlite3_free(error);
    assert(false);
  }
}

std::string first_text(sqlite3* db, const std::string& sql,
                       const std::string& parameter) {
  sqlite3_stmt* statement = nullptr;
  assert(sqlite3_prepare_v2(db, sql.c_str(), -1, &statement, nullptr) ==
         SQLITE_OK);
  assert(sqlite3_bind_text(statement, 1, parameter.c_str(), -1,
                           SQLITE_TRANSIENT) == SQLITE_OK);
  assert(sqlite3_step(statement) == SQLITE_ROW);
  const auto* value = sqlite3_column_text(statement, 0);
  const std::string result = value == nullptr
                                 ? std::string{}
                                 : std::string(reinterpret_cast<const char*>(value));
  assert(sqlite3_finalize(statement) == SQLITE_OK);
  return result;
}

void assert_standard_type_tables(const std::filesystem::path& path) {
  sqlite3* db = nullptr;
  assert(sqlite3_open_v2(path.string().c_str(), &db, SQLITE_OPEN_READONLY,
                         nullptr) == SQLITE_OK);
  const auto event_token = first_text(
      db, "SELECT ocel_type_map FROM event_map_type WHERE ocel_type=?1",
      "Create");
  const auto object_token = first_text(
      db, "SELECT ocel_type_map FROM object_map_type WHERE ocel_type=?1",
      "sales:order");
  sqlite3_stmt* event_statement = nullptr;
  // A column without values is not written, as the reference frame drops it.
  sqlite3_stmt* missing = nullptr;
  assert(sqlite3_prepare_v2(db, ("SELECT empty FROM event_" + event_token).c_str(), -1, &missing,
                            nullptr) != SQLITE_OK);
  sqlite3_finalize(missing);
  const auto event_query = "SELECT amount, note FROM event_" + event_token;
  assert(sqlite3_prepare_v2(db, event_query.c_str(), -1, &event_statement,
                            nullptr) == SQLITE_OK);
  assert(sqlite3_step(event_statement) == SQLITE_ROW);
  assert(sqlite3_column_double(event_statement, 0) == 2.5);
  assert(std::string(reinterpret_cast<const char*>(sqlite3_column_text(event_statement, 1))) ==
         "quoted value");
  assert(sqlite3_finalize(event_statement) == SQLITE_OK);

  sqlite3_stmt* object_statement = nullptr;
  const auto object_query = "SELECT status, ocel_changed_field FROM object_" + object_token +
                             " ORDER BY rowid";
  assert(sqlite3_prepare_v2(db, object_query.c_str(), -1, &object_statement,
                            nullptr) == SQLITE_OK);
  assert(sqlite3_step(object_statement) == SQLITE_ROW);
  assert(std::string(reinterpret_cast<const char*>(sqlite3_column_text(object_statement, 0))) ==
         "open");
  assert(sqlite3_column_type(object_statement, 1) == SQLITE_NULL);
  assert(sqlite3_step(object_statement) == SQLITE_ROW);
  assert(std::string(reinterpret_cast<const char*>(sqlite3_column_text(object_statement, 0))) ==
         "paid");
  assert(std::string(reinterpret_cast<const char*>(sqlite3_column_text(object_statement, 1))) ==
         "status");
  assert(sqlite3_step(object_statement) == SQLITE_DONE);
  assert(sqlite3_finalize(object_statement) == SQLITE_OK);
  assert(sqlite3_close(db) == SQLITE_OK);
}

// The tables the reference writer creates, each as its PRAGMA columns and
// then one line per row (rowid order) of typeof:quote per cell. Type tables
// are labelled by their OCEL type, because the table-name tokens differ (a
// declared difference: native tokens cannot collide).
std::string reference_tables_dump(const std::filesystem::path& path) {
  sqlite3* db = nullptr;
  assert(sqlite3_open_v2(path.string().c_str(), &db, SQLITE_OPEN_READONLY, nullptr) == SQLITE_OK);
  const auto column_texts = [&](const std::string& sql) {
    std::vector<std::vector<std::string>> rows;
    sqlite3_stmt* statement = nullptr;
    assert(sqlite3_prepare_v2(db, sql.c_str(), -1, &statement, nullptr) == SQLITE_OK);
    while (sqlite3_step(statement) == SQLITE_ROW) {
      std::vector<std::string> row;
      for (int i = 0; i < sqlite3_column_count(statement); ++i) {
        const auto* text = sqlite3_column_text(statement, i);
        row.emplace_back(text == nullptr ? "" : reinterpret_cast<const char*>(text));
      }
      rows.push_back(std::move(row));
    }
    assert(sqlite3_finalize(statement) == SQLITE_OK);
    return rows;
  };
  std::string out;
  const auto dump_table = [&](const std::string& table, const std::string& label) {
    const auto columns = column_texts("PRAGMA table_info(\"" + table + "\")");
    out += label + "(";
    std::string expression;
    for (std::size_t i = 0; i < columns.size(); ++i) {
      out += (i ? "," : "") + columns[i][1] + " " + columns[i][2];
      const auto quoted = "\"" + columns[i][1] + "\"";
      expression += (i ? "||'|'||" : "") + std::string("typeof(") + quoted + ")||':'||quote(" + quoted + ")";
    }
    out += ")\n";
    for (const auto& row : column_texts("SELECT " + expression + " FROM \"" + table + "\" ORDER BY rowid"))
      out += "  " + row[0] + "\n";
  };
  for (const char* table : {"event", "object", "event_object", "object_object"}) dump_table(table, table);
  for (const auto& [map, prefix] : {std::pair<std::string, std::string>{"event_map_type", "event_"},
                                    std::pair<std::string, std::string>{"object_map_type", "object_"}}) {
    auto types = column_texts("SELECT ocel_type, ocel_type_map FROM " + map);
    std::sort(types.begin(), types.end());
    for (const auto& type : types) dump_table(prefix + type[1], prefix + "[" + type[0] + "]");
  }
  assert(sqlite3_close(db) == SQLITE_OK);
  return out;
}

// Captured with PM4Py 2.7.22: read_ocel2_json on kFixture, then
// write_ocel2_sqlite, dumped with reference_tables_dump. Columns take the
// pandas to_sql type of the frame column (an int or bool column INTEGER, a
// float column REAL), timestamps the text pandas writes, and an object's
// changes follow its base row.
constexpr const char* kFixture = R"REF({"objectTypes":[{"name":"order","attributes":[{"name":"color","type":"string"}]},{"name":"item","attributes":[{"name":"weight","type":"float"}]}],
 "eventTypes":[{"name":"create","attributes":[]},{"name":"pick, item","attributes":[]}],
 "objects":[
  {"id":"o1","type":"order","attributes":[{"name":"color","time":"1970-01-01T00:00:00Z","value":"red"},{"name":"color","time":"2020-01-02T00:00:00Z","value":"blue"}],"relationships":[{"objectId":"i1","qualifier":"contains"}]},
  {"id":"i1","type":"item","attributes":[{"name":"weight","time":"1970-01-01T00:00:00Z","value":2.5}]},
  {"id":"i'2","type":"item","attributes":[{"name":"weight","time":"1970-01-01T00:00:00Z","value":3}]}],
 "events":[
  {"id":"e1","type":"create","time":"2020-01-01T00:00:00Z","attributes":[{"name":"n","value":4},{"name":"flag","value":false},{"name":"cost","value":0.1},{"name":"note","value":"a<b&\"c\"\nd"}],"relationships":[{"objectId":"o1","qualifier":"q"}]},
  {"id":"e2","type":"pick, item","time":"2020-01-01T00:00:01.250000Z","attributes":[{"name":"n","value":5},{"name":"flag","value":true}],"relationships":[{"objectId":"i'2","qualifier":""}]},
  {"id":"e3","type":"create","time":"2020-01-01T00:00:02.000001Z","attributes":[{"name":"n","value":-6},{"name":"flag","value":true},{"name":"cost","value":1e20}],"relationships":[{"objectId":"i1","qualifier":""}]},
  {"id":"e4","type":"pick, item","time":"2020-01-01T00:00:03Z","attributes":[{"name":"n","value":7},{"name":"flag","value":false}],"relationships":[{"objectId":"o1","qualifier":"r"}]}]})REF";
constexpr const char* kReferenceTables = R"REF(event(ocel_id TEXT,ocel_type TEXT)
  text:'e1'|text:'create'
  text:'e2'|text:'pick, item'
  text:'e3'|text:'create'
  text:'e4'|text:'pick, item'
object(ocel_id TEXT,ocel_type TEXT)
  text:'o1'|text:'order'
  text:'i1'|text:'item'
  text:'i''2'|text:'item'
event_object(ocel_event_id TEXT,ocel_object_id TEXT,ocel_qualifier TEXT)
  text:'e1'|text:'o1'|text:'q'
  text:'e2'|text:'i''2'|text:''
  text:'e3'|text:'i1'|text:''
  text:'e4'|text:'o1'|text:'r'
object_object(ocel_source_id TEXT,ocel_target_id TEXT,ocel_qualifier TEXT)
  text:'o1'|text:'i1'|text:'contains'
event_[create](ocel_id TEXT,ocel_time TIMESTAMP,ocel:activity TEXT,n INTEGER,flag INTEGER,cost REAL,note TEXT)
  text:'e1'|text:'2020-01-01 00:00:00+00:00'|text:'create'|integer:4|integer:0|real:0.1|text:'a<b&"c"
d'
  text:'e3'|text:'2020-01-01 00:00:02.000001+00:00'|text:'create'|integer:-6|integer:1|real:1.0e+20|null:NULL
event_[pick, item](ocel_id TEXT,ocel_time TIMESTAMP,ocel:activity TEXT,n INTEGER,flag INTEGER)
  text:'e2'|text:'2020-01-01 00:00:01.250000+00:00'|text:'pick, item'|integer:5|integer:1
  text:'e4'|text:'2020-01-01 00:00:03+00:00'|text:'pick, item'|integer:7|integer:0
object_[item](ocel_id TEXT,weight REAL)
  text:'i1'|real:2.5
  text:'i''2'|real:3.0
object_[order](ocel_id TEXT,color TEXT,ocel_changed_field TEXT,ocel_time TIMESTAMP)
  text:'o1'|text:'red'|null:NULL|null:NULL
  text:'o1'|text:'blue'|text:'color'|text:'2020-01-02 00:00:00+00:00'
)REF";

void reference_capture_tests() {
  const auto path = std::filesystem::temp_directory_path() / "minera_ocel_sqlite_reference.sqlite";
  std::error_code ignored;
  std::filesystem::remove(path, ignored);
  const auto log = minera::parse_ocel_json(kFixture);
  minera::export_ocel_sqlite(log, path);
  assert(reference_tables_dump(path) == kReferenceTables);
  const auto round = minera::import_ocel_sqlite(path);
  assert(round.events.size() == 4 && round.objects.size() == 3 && round.relations.size() == 4);
  assert(round.o2o.size() == 1 && round.object_changes.size() == 1);
  std::filesystem::remove(path, ignored);
}
#endif

}  // namespace

int main() {
#if !defined(MINERA_HAS_SQLITE)
  return 0;
#else
  const auto path = std::filesystem::temp_directory_path() /
                    "minera_ocel_sqlite_roundtrip.sqlite";
  std::error_code ignored;
  std::filesystem::remove(path, ignored);

  const auto external_path = std::filesystem::temp_directory_path() /
                             "minera_ocel_sqlite_external.sqlite";
  std::filesystem::remove(external_path, ignored);

  minera::Ocel log;
  log.events = {
      {"e2", "Ship", "2024-01-02T00:00:00Z",
       {{"ok", true}, {"count", std::int64_t(2)}}},
      {"e1", "Create", "2024-01-01T00:00:00Z",
       {{"amount", 2.5}, {"note", std::string("quoted value")},
        {"empty", nullptr}}},
  };
  log.objects = {
      {"o1", "sales:order", {{"status", std::string("open")}}},
      {"i1", "item", {{"ready", false}}},
  };
  log.relations = {
      {"e1", "o1", "owner"}, {"e2", "o1", "ships"},
      {"e2", "i1", "line"},
  };
  log.o2o = {{"o1", "i1", "contains"}};
  log.e2e = {{"e1", "e2", "precedes"}};
  log.object_changes = {
      {"o1", "sales:order", "2024-01-03T00:00:00Z", "status",
       std::string("paid")},
  };
  log.globals["source"] = std::string("fixture");
  log.globals["version"] = std::int64_t(2);
  log.event_type_attributes["EmptyEvent"] = {{"reason", "string"}};
  log.event_type_attributes["Create"] = {{"count", "int"}};
  log.object_type_attributes["EmptyObject"] = {{"label", "string"}};
  log.object_type_attributes["sales:order"] = {{"priority", "int"}};

  minera::export_ocel_sqlite(log, path);
  assert(std::filesystem::exists(path));
  std::ifstream bytes(path, std::ios::binary);
  std::string header(16, '\0');
  bytes.read(header.data(), static_cast<std::streamsize>(header.size()));
  assert(header == std::string("SQLite format 3", 15) + '\0');
  assert_standard_type_tables(path);
  reference_capture_tests();

  const auto round = minera::import_ocel_sqlite(path);
  assert(round.events.size() == 2 && round.objects.size() == 2);
  assert(round.events[0].id == "e1" && round.events[0].activity == "Create");
  assert(std::get<double>(round.events[0].attributes[0].value) == 2.5);
  assert(std::get<bool>(round.events[1].attributes[0].value));
  assert(std::get<std::nullptr_t>(round.events[0].attributes[2].value) == nullptr);
  assert(round.objects[1].type == "sales:order");
  assert(std::get<bool>(round.objects[0].attributes[0].value) == false);
  assert(round.relations.size() == 3 && round.o2o.size() == 1 &&
         round.e2e.size() == 1);
  assert(round.object_changes.size() == 1 && round.object_changes[0].field == "status");
  assert(std::get<std::string>(round.globals.at("source")) == "fixture");
  assert(std::get<std::int64_t>(round.globals.at("version")) == 2);
  assert(round.event_type_attributes.at("EmptyEvent").at("reason") ==
         "string");
  assert(round.event_type_attributes.at("Create").at("count") == "int");
  assert(round.object_type_attributes.at("EmptyObject").at("label") ==
         "string");
  assert(round.object_type_attributes.at("sales:order").at("priority") ==
         "int");

  throws([&] {
    auto invalid = log;
    invalid.e2e.push_back({"e1", "unknown", "bad"});
    minera::export_ocel_sqlite(invalid, path);
  });

  sqlite3* external_db = nullptr;
  assert(sqlite3_open_v2(external_path.string().c_str(), &external_db,
                         SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) ==
         SQLITE_OK);
  exec_sql(external_db,
           "CREATE TABLE event (ocel_id TEXT PRIMARY KEY, ocel_type TEXT NOT NULL);"
           "CREATE TABLE object (ocel_id TEXT PRIMARY KEY, ocel_type TEXT NOT NULL);"
           "CREATE TABLE event_map_type (ocel_type TEXT PRIMARY KEY, ocel_type_map TEXT NOT NULL);"
           "CREATE TABLE object_map_type (ocel_type TEXT PRIMARY KEY, ocel_type_map TEXT NOT NULL);"
           "CREATE TABLE event_object (ocel_event_id TEXT, ocel_object_id TEXT, ocel_qualifier TEXT);"
           "CREATE TABLE event_Create (ocel_id TEXT PRIMARY KEY, ocel_time TEXT, priority INTEGER, active BOOLEAN);"
           "CREATE TABLE object_order (ocel_id TEXT, ocel_time TEXT, status TEXT, ocel_changed_field TEXT, \"@@cumcount\" INTEGER);"
           "INSERT INTO event VALUES ('e1', 'Create');"
           "INSERT INTO object VALUES ('o1', 'order');"
           "INSERT INTO event_map_type VALUES ('Create', 'Create');"
           "INSERT INTO object_map_type VALUES ('order', 'order');"
           "INSERT INTO event_Create VALUES ('e1', '2024-02-01T00:00:00Z', 7, 1);"
           "INSERT INTO object_order VALUES ('o1', NULL, 'open', 'status', 0);"
           "INSERT INTO object_order VALUES ('o1', '2024-02-02T00:00:00Z', 'paid', 'status', 1);"
           "INSERT INTO event_object VALUES ('e1', 'o1', 'uses');");
  assert(sqlite3_close(external_db) == SQLITE_OK);

  const auto external = minera::import_ocel_sqlite(external_path);
  assert(external.events.size() == 1 && external.objects.size() == 1);
  assert(external.events[0].timestamp == "2024-02-01T00:00:00Z");
  assert(std::get<std::int64_t>(external.events[0].attributes[0].value) == 7);
  assert(std::get<bool>(external.events[0].attributes[1].value));
  assert(std::get<std::string>(external.objects[0].attributes[0].value) == "open");
  assert(external.relations.size() == 1);
  assert(external.object_changes.size() == 1);
  assert(external.object_changes[0].object_type == "order");
  assert(external.object_changes[0].field == "status");
  assert(std::get<std::string>(external.object_changes[0].value) == "paid");
  assert(external.event_type_attributes.at("Create").at("priority") == "int");
  assert(external.event_type_attributes.at("Create").at("active") == "boolean");
  assert(external.object_type_attributes.at("order").at("status") == "string");

  // Reference capture (pm4py 2.7.22 read_ocel2_sqlite): an event and an object
  // without event-object relations are dropped with the object's changes and
  // the O2O link touching it -> 1 event, 1 object, 1 relation, 0 O2O, 1 change.
  const auto orphan_path = external_path.string() + ".orphans.sqlite";
  std::filesystem::remove(orphan_path, ignored);
  sqlite3* orphan_db = nullptr;
  assert(sqlite3_open_v2(orphan_path.c_str(), &orphan_db,
                         SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) ==
         SQLITE_OK);
  exec_sql(orphan_db,
           "CREATE TABLE event (ocel_id TEXT PRIMARY KEY, ocel_type TEXT NOT NULL);"
           "CREATE TABLE object (ocel_id TEXT PRIMARY KEY, ocel_type TEXT NOT NULL);"
           "CREATE TABLE event_map_type (ocel_type TEXT PRIMARY KEY, ocel_type_map TEXT NOT NULL);"
           "CREATE TABLE object_map_type (ocel_type TEXT PRIMARY KEY, ocel_type_map TEXT NOT NULL);"
           "CREATE TABLE event_object (ocel_event_id TEXT, ocel_object_id TEXT, ocel_qualifier TEXT);"
           "CREATE TABLE object_object (ocel_source_id TEXT, ocel_target_id TEXT, ocel_qualifier TEXT);"
           "CREATE TABLE event_Create (ocel_id TEXT PRIMARY KEY, ocel_time TEXT, priority INTEGER, active BOOLEAN);"
           "CREATE TABLE object_order (ocel_id TEXT, ocel_time TEXT, status TEXT, ocel_changed_field TEXT, \"@@cumcount\" INTEGER);"
           "INSERT INTO event VALUES ('e1', 'Create');"
           "INSERT INTO event VALUES ('e2', 'Create');"
           "INSERT INTO object VALUES ('o1', 'order');"
           "INSERT INTO object VALUES ('o2', 'order');"
           "INSERT INTO event_map_type VALUES ('Create', 'Create');"
           "INSERT INTO object_map_type VALUES ('order', 'order');"
           "INSERT INTO event_Create VALUES ('e1', '2024-02-01T00:00:00Z', 7, 1);"
           "INSERT INTO event_Create VALUES ('e2', '2024-02-03T00:00:00Z', 8, 0);"
           "INSERT INTO object_order VALUES ('o1', NULL, 'open', 'status', 0);"
           "INSERT INTO object_order VALUES ('o1', '2024-02-02T00:00:00Z', 'paid', 'status', 1);"
           "INSERT INTO object_order VALUES ('o2', NULL, 'open', 'status', 0);"
           "INSERT INTO object_order VALUES ('o2', '2024-02-04T00:00:00Z', 'paid', 'status', 1);"
           "INSERT INTO event_object VALUES ('e1', 'o1', 'uses');"
           "INSERT INTO object_object VALUES ('o1', 'o2', 'next');");
  assert(sqlite3_close(orphan_db) == SQLITE_OK);
  const auto orphans = minera::import_ocel_sqlite(orphan_path);
  assert(orphans.events.size() == 1 && orphans.events[0].id == "e1");
  assert(orphans.objects.size() == 1 && orphans.objects[0].id == "o1");
  assert(orphans.relations.size() == 1 && orphans.o2o.empty());
  assert(orphans.object_changes.size() == 1 &&
         orphans.object_changes[0].object_id == "o1");
  std::filesystem::remove(orphan_path, ignored);

  // Naive times read as UTC, as pm4py.read_ocel2_sqlite stores them
  // (datetime64[us, UTC]): the event time is stamped "Z", the change time
  // "+00:00", as the reference JSON writers print them. An id listed twice in
  // the object table is refused: the reference keeps an object row per
  // type-table row, all typed by the id's last "object" row (declared).
  const auto naive_path = external_path.string() + ".naive.sqlite";
  for (const bool repeated : {false, true}) {
    std::filesystem::remove(naive_path, ignored);
    sqlite3* naive_db = nullptr;
    assert(sqlite3_open_v2(naive_path.c_str(), &naive_db,
                           SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) == SQLITE_OK);
    exec_sql(naive_db,
             "CREATE TABLE event (ocel_id TEXT, ocel_type TEXT);"
             "CREATE TABLE object (ocel_id TEXT, ocel_type TEXT);"
             "CREATE TABLE event_map_type (ocel_type TEXT, ocel_type_map TEXT);"
             "CREATE TABLE object_map_type (ocel_type TEXT, ocel_type_map TEXT);"
             "CREATE TABLE event_object (ocel_event_id TEXT, ocel_object_id TEXT, ocel_qualifier TEXT);"
             "CREATE TABLE object_object (ocel_source_id TEXT, ocel_target_id TEXT, ocel_qualifier TEXT);"
             "CREATE TABLE event_Create (ocel_id TEXT, ocel_time TEXT);"
             "CREATE TABLE object_order (ocel_id TEXT, ocel_time TEXT, status TEXT, ocel_changed_field TEXT);"
             "INSERT INTO event VALUES ('e1', 'Create');"
             "INSERT INTO object VALUES ('o1', 'order');"
             "INSERT INTO event_map_type VALUES ('Create', 'Create');"
             "INSERT INTO object_map_type VALUES ('order', 'order');"
             "INSERT INTO event_Create VALUES ('e1', '2024-02-01 00:00:00');"
             "INSERT INTO object_order VALUES ('o1', NULL, 'open', NULL);"
             "INSERT INTO object_order VALUES ('o1', '2024-02-02 10:00:00.5', 'paid', 'status');"
             "INSERT INTO event_object VALUES ('e1', 'o1', 'uses');");
    if (repeated) exec_sql(naive_db, "INSERT INTO object VALUES ('o1', 'item');");
    assert(sqlite3_close(naive_db) == SQLITE_OK);
    if (repeated) {
      bool refused = false;
      try {
        (void)minera::import_ocel_sqlite(naive_path);
      } catch (const std::invalid_argument& error) {
        refused = std::string(error.what()).find("o1 is listed twice (types order and item)") !=
                  std::string::npos;
      }
      assert(refused);
      continue;
    }
    const auto naive = minera::import_ocel_sqlite(naive_path);
    assert(naive.events.size() == 1 && naive.events[0].timestamp == "2024-02-01T00:00:00Z");
    assert(naive.object_changes.size() == 1 &&
           naive.object_changes[0].timestamp == "2024-02-02T10:00:00.500000+00:00");
  }
  std::filesystem::remove(naive_path, ignored);

  std::filesystem::remove(path, ignored);
  std::filesystem::remove(external_path, ignored);
  return 0;
#endif
}
