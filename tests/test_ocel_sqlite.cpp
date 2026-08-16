#include "minera/ocel_sqlite.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

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

  minera::export_ocel_sqlite(log, path);
  assert(std::filesystem::exists(path));
  std::ifstream bytes(path, std::ios::binary);
  std::string header(16, '\0');
  bytes.read(header.data(), static_cast<std::streamsize>(header.size()));
  assert(header == std::string("SQLite format 3", 15) + '\0');

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

  std::filesystem::remove(path, ignored);
  std::filesystem::remove(external_path, ignored);
  return 0;
#endif
}
