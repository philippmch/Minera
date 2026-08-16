#include "minera/ocel_sqlite.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

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

}  // namespace

int main() {
#if !defined(MINERA_HAS_SQLITE)
  return 0;
#else
  const auto path = std::filesystem::temp_directory_path() /
                    "minera_ocel_sqlite_roundtrip.sqlite";
  std::error_code ignored;
  std::filesystem::remove(path, ignored);

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

  std::filesystem::remove(path, ignored);
  return 0;
#endif
}
