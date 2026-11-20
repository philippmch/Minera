// Review EZ2: filter and split results keep their source frames
// (review-harness round-ez: t2/h3.jsonocel, t2/h2ref.py, q/fdsw.py,
// q/ssw.py). Observed black-box on pm4py 2.7.22 with a classic JSON log
// whose o1 and o3 (type t) hold ints p and whose o2 (type u) holds a float p
// and a text q, whose e2 holds a text x1:
//
// - filter_ocel_object_types(["t"]) selects rows of the source frames, so
//   the result keeps every column (q and x1, empty there) in build order and
//   its dtypes (p float64): write_ocel_json prints p 1.0 and lists q and x1
//   among the attribute names, write_ocel_csv's headers keep x1 and q,
//   write_ocel2_sqlite declares object_t's p REAL.
// - The connected-components split builds each component from its events,
//   objects and relations (rows of the source frames, so the same columns
//   and dtypes) as a new OCEL: its O2O frame is float64 (object_object REAL)
//   and its globals are empty (the OCEL 1.0 writers print the default global
//   scopes), while the filter keeps the source's.
// - The ancestors/descendants split keeps the source frames' columns too.
//
// See docs/reference-2.7.22-parity.md, Review EZ2.
#include "minera/ocel.hpp"
#include "minera/ocel_csv.hpp"
#include "minera/ocel_filter.hpp"
#include "minera/ocel_sqlite.hpp"
#include "minera/ocel_transform.hpp"

#include <algorithm>
#include <filesystem>
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

const char* const kJson = R"({"ocel:global-event":{"ocel:activity":"zz"},"ocel:global-object":{"ocel:type":"__INVALID__"},
"ocel:global-log":{"ocel:attribute-names":["k","x1","p","q"],"ocel:object-types":["t","u"],"ocel:version":"1.0","ocel:ordering":"timestamp"},
"ocel:events":{
 "e1":{"ocel:activity":"a","ocel:timestamp":"2024-01-01T10:00:00+00:00","ocel:omap":["o1","o3"],"ocel:vmap":{"k":1}},
 "e2":{"ocel:activity":"b","ocel:timestamp":"2024-01-02T10:00:00+00:00","ocel:omap":["o2","o4"],"ocel:vmap":{"k":2,"x1":"z"}}},
"ocel:objects":{
 "o1":{"ocel:type":"t","ocel:ovmap":{"p":1}},
 "o2":{"ocel:type":"u","ocel:ovmap":{"p":2.5,"q":"w"}},
 "o3":{"ocel:type":"t","ocel:ovmap":{"p":3}},
 "o4":{"ocel:type":"u","ocel:ovmap":{}}}})";

// write_ocel_csv writes the pair whatever the log holds.
OcelCsvExportOptions lossy() {
  OcelCsvExportOptions options;
  options.allow_lossy = true;
  return options;
}

std::vector<std::string> object_ids(const Ocel& log) {
  std::vector<std::string> ids;
  for (const auto& object : log.objects) ids.push_back(object.id);
  std::sort(ids.begin(), ids.end());
  return ids;
}

// The t part of the log (e1, o1, o3) as pm4py's filter or split writes it.
void check_frames(const Ocel& log, const std::string& where, bool split) {
  expect(object_ids(log) == std::vector<std::string>{"o1", "o3"}, where + ": o1 and o3");
  const auto classic = serialize_ocel_json(log, OcelJsonFormat::kClassic, false);
  expect(contains(classic, R"("p":1.0)") && contains(classic, R"("p":3.0)"),
         where + ": classic JSON prints the float64 p: " + classic);
  expect(contains(classic, R"("ocel:attribute-names":["k","p","q","x1"])"),
         where + ": classic JSON lists q and x1 among the attribute names");
  expect(contains(classic, split ? R"("ocel:global-event":{"ocel:activity":"__INVALID__"})"
                                 : R"("ocel:global-event":{"ocel:activity":"zz"})"),
         where + (split ? ": the component's global event scope is the default"
                        : ": the filter keeps the global event scope"));

  const auto objects_csv = serialize_ocel_objects_csv(log, lossy());
  expect(objects_csv.rfind("ocel:oid,ocel:type,p,q\n", 0) == 0 && contains(objects_csv, "o1,t,1.0,\n") &&
             contains(objects_csv, "o3,t,3.0,\n"),
         where + ": the objects CSV keeps q and prints p 1.0: " + objects_csv);
  const auto events_csv = serialize_ocel_csv(log, lossy());
  expect(events_csv.rfind("ocel:eid,ocel:timestamp,ocel:activity,k,x1,ocel:type:t\n", 0) == 0,
         where + ": the events CSV keeps x1: " + events_csv);

#if defined(MINERA_HAS_SQLITE)
  const auto path = std::filesystem::temp_directory_path() / "minera_review_ez2.sqlite";
  std::filesystem::remove(path);
  export_ocel_sqlite(log, path);
  sqlite3* db = nullptr;
  expect(sqlite3_open(path.string().c_str(), &db) == SQLITE_OK, where + ": open the SQLite download");
  const auto query = [&](const std::string& sql) {
    std::vector<std::string> rows;
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK) return rows;
    while (sqlite3_step(statement) == SQLITE_ROW) {
      std::string row;
      for (int column = 0; column < sqlite3_column_count(statement); ++column) {
        const auto* text = sqlite3_column_text(statement, column);
        row += (column ? "|" : "") + std::string(text ? reinterpret_cast<const char*>(text) : "NULL");
      }
      rows.push_back(row);
    }
    sqlite3_finalize(statement);
    return rows;
  };
  const auto object_map = query("select ocel_type_map from object_map_type where ocel_type = 't'");
  expect(object_map.size() == 1, where + ": one object type table for t");
  if (object_map.size() == 1) {
    const auto declared = query("select name, type from pragma_table_info('object_" + object_map[0] + "')");
    expect(std::find(declared.begin(), declared.end(), "p|REAL") != declared.end(), where + ": a REAL column p");
  }
  const auto links = query("select name, type from pragma_table_info('object_object')");
  if (split)
    expect(links == std::vector<std::string>{"ocel_source_id|REAL", "ocel_target_id|REAL", "ocel_qualifier|REAL"},
           where + ": the component's object_object columns are REAL");
  sqlite3_close(db);
  std::filesystem::remove(path);
#endif
}

void test_filter() {
  const auto log = parse_ocel_json(kJson);
  const auto filtered = filter_ocel_object_types(log, {"t"});
  check_frames(filtered, "filter_ocel_object_types", false);
  // The OCEL transport carries the result's frames.
  check_frames(parse_ocel_json(serialize_ocel_json(filtered, OcelJsonFormat::kClassic, true)),
               "filter_ocel_object_types through the transport", false);
  // Other filter families select rows the same way.
  check_frames(filter_ocel_events(log, {"e1"}), "filter_ocel_events", false);
  check_frames(filter_ocel_objects(log, {"o1", "o3"}), "filter_ocel_objects", false);
}

void test_splits() {
  const auto log = parse_ocel_json(kJson);
  OcelSplitOptions options;
  options.variant = OcelSplitVariant::kConnectedComponents;
  const auto components = split_ocel(log, options);
  expect(components.logs.size() == 2, "two connected components");
  for (const auto& component : components.logs) {
    expect(component.globals.empty() && !component.global_event_scope && !component.global_object_scope,
           "a component's globals are a new OCEL's");
    if (object_ids(component) == std::vector<std::string>{"o1", "o3"})
      check_frames(component, "the connected-components split", true);
  }
  options.variant = OcelSplitVariant::kAncestorsDescendants;
  options.object_type = "t";
  const auto sublogs = split_ocel(log, options);
  expect(!sublogs.logs.empty(), "ancestors/descendants sublogs");
  for (const auto& sublog : sublogs.logs) {
    const auto objects_csv = serialize_ocel_objects_csv(sublog, lossy());
    expect(objects_csv.rfind("ocel:oid,ocel:type,p,q\n", 0) == 0 && contains(objects_csv, "o1,t,1.0,\n"),
           "an ancestors/descendants sublog keeps q and prints p 1.0: " + objects_csv);
  }
}

}  // namespace

int main() {
  try {
    test_filter();
    test_splits();
  } catch (const std::exception& error) {
    std::cerr << "FAILED with exception: " << error.what() << '\n';
    return 1;
  }
  if (failures != 0) {
    std::cerr << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "review EZ2 tests passed\n";
  return 0;
}
