#undef NDEBUG

#include <cassert>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include "minera/browser_history.hpp"

#ifdef MINERA_HAS_SQLITE
#ifdef _WIN32
#include <winsqlite/winsqlite3.h>
#else
#include <sqlite3.h>
#endif
#endif

namespace {

#ifdef MINERA_HAS_SQLITE
void sql(const std::filesystem::path& path, const char* statement) {
  sqlite3* db = nullptr;
  assert(sqlite3_open(path.string().c_str(), &db) == SQLITE_OK);
  char* error = nullptr;
  const auto status = sqlite3_exec(db, statement, nullptr, nullptr, &error);
  if (error) sqlite3_free(error);
  sqlite3_close(db);
  assert(status == SQLITE_OK);
}
#endif

bool rejects(const auto& fn) {
  try { fn(); } catch (const std::invalid_argument&) { return true; }
  return false;
}

}  // namespace

int main() {
#ifndef MINERA_HAS_SQLITE
  return 0;
#else
  const auto root = std::filesystem::temp_directory_path() /
                    "minera_browser_history_test";
  std::error_code error;
  std::filesystem::remove_all(root, error);
  std::filesystem::create_directories(root / "Profile 2");
  std::filesystem::create_directories(root / "Default");

  const auto chrome = root / "Default" / "History";
  sql(chrome,
      "CREATE TABLE urls(id INTEGER PRIMARY KEY,url TEXT,title TEXT,"
      "typed_count INTEGER,visit_count INTEGER);"
      "CREATE TABLE visits(id INTEGER PRIMARY KEY,url INTEGER,visit_time INTEGER,"
      "visit_duration INTEGER,transition INTEGER,from_visit INTEGER);"
      "INSERT INTO urls VALUES(1,'https://example.com/a?q=1','A',2,5);"
      "INSERT INTO urls VALUES(2,'https://example.com/b','B',0,1);"
      "INSERT INTO visits VALUES(1,1,13348540800000000,1500000,1,0);"
      "INSERT INTO visits VALUES(2,2,13348540801000000,0,8,1);");
  minera::BrowserHistoryOptions options;
  options.history_path = root;
  const auto chrome_log = minera::import_browser_history(options);
  assert(chrome_log.trace_count() == 1 && chrome_log.event_count() == 2);
  assert(chrome_log.traces[0].case_id == "Default");
  assert(chrome_log.traces[0].events[0].activity == "example.com/a");
  assert(chrome_log.traces[0].events[0].timestamp == "2024-01-01T00:00:00Z");
  assert(*chrome_log.traces[0].events[0].attribute("transition") == "TYPED");
  assert(*chrome_log.traces[0].events[1].attribute("referrer") ==
         "https://example.com/a?q=1");
  const auto chrome_json = minera::browser_history_json(
      chrome_log, minera::BrowserHistoryVariant::kChrome);
  assert(chrome_json.find("\"variant\":\"chrome_history\"") !=
         std::string::npos);

  const auto firefox = root / "Profile 2" / "places.sqlite";
  sql(firefox,
      "CREATE TABLE moz_places(id INTEGER PRIMARY KEY,url TEXT,title TEXT,"
      "visit_count INTEGER,typed INTEGER,frecency INTEGER,guid TEXT);"
      "CREATE TABLE moz_historyvisits(id INTEGER PRIMARY KEY,place_id INTEGER,"
      "visit_date INTEGER,visit_type INTEGER,session INTEGER,from_visit INTEGER);"
      "INSERT INTO moz_places VALUES(1,'https://mozilla.org/start?x=1','Start',3,1,99,'abc');"
      "INSERT INTO moz_historyvisits VALUES(1,1,1704067200000000,2,7,0);");
  options.variant = minera::BrowserHistoryVariant::kFirefox;
  const auto firefox_log = minera::import_browser_history(options);
  assert(firefox_log.trace_count() == 1 && firefox_log.event_count() == 1);
  assert(firefox_log.traces[0].case_id == "Profile 2");
  assert(firefox_log.traces[0].events[0].activity == "mozilla.org/start");
  assert(*firefox_log.traces[0].events[0].attribute("visit_type_desc") ==
         "BOOKMARK");
  assert(*firefox_log.traces[0].events[0].attribute("typed") == "true");

  options.history_path = firefox;
  const auto single = minera::import_browser_history(options);
  assert(single.traces[0].case_id == "DEFAULT");
  options.max_events = 0;
  assert(rejects([&] { (void)minera::import_browser_history(options); }));
  options.max_events = 1;
  options.history_path = root / "missing";
  assert(rejects([&] { (void)minera::import_browser_history(options); }));

  std::filesystem::remove_all(root, error);
  return 0;
#endif
}
