// Review EQ1: pm4py 2.7.22 on fresh OCEL CSV banks (b60, b61, one bank per
// generator feature), a fresh OCEL 1.0 bank (b59), SQLite files written
// from time-first CSV logs (b62) and the object filters at levels 2 to 4
// (review-harness round-eq: hc/f2/). Observed black-box:
//
// - read_ocel2_sqlite reads a native SQLite file through its type tables
//   like any other: the frames take the union of the type tables' columns
//   in table order, whatever order native's attribute tables hold, and the
//   values, changes and types are the type tables' (a boolean reads as an
//   integer, a date as its text, a change gets "@@cumcount"), without the
//   native tables' event links and globals;
// - filter_ocel_objects at a level above 1 raises KeyError on a requested
//   id the objects table does not list; with several such ids pm4py names
//   the first in its set's order (hash-seeded, declared), native the
//   smallest, on every standard library.
//
// See docs/reference-2.7.22-parity.md, Review EQ1.
#include "minera/ocel.hpp"
#include "minera/ocel_csv.hpp"
#include "minera/ocel_filter.hpp"
#include "minera/ocel_sqlite.hpp"

#include <filesystem>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace minera;

namespace {

int failures = 0;

void expect(bool condition, const std::string& what) {
  if (!condition) {
    ++failures;
    std::cerr << "FAILED: " << what << '\n';
  }
}

std::string error_of(const std::function<void()>& run) {
  try {
    run();
  } catch (const std::exception& error) {
    return error.what();
  }
  return {};
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

// hc/f2: x1 and p0 are empty on the first rows, so native's attribute tables
// list x2 before x1 and p1 before p0.
const std::string kEvents =
    "ocel:timestamp,ocel:eid,ocel:activity,x0,x1,x2,ocel:type:t\n"
    "2024-02-01 10:00:00,e1,a,1,,z,['o1']\n"
    "2024-02-01 11:00:00,e2,a,2,y,,\"['o1', 'o2']\"\n"
    "2024-02-01 12:00:00,e3,b,,y,z,['o2']\n";
const std::string kObjects = "ocel:oid,ocel:type,p0,p1\no1,t,,5\no2,t,4,6\n";

// EQ1-1: a native SQLite file read back.
void native_sqlite_frame_order() {
#if defined(MINERA_HAS_SQLITE)
  const auto log = parse_ocel_csv(kEvents, kObjects);
  const auto path = std::filesystem::temp_directory_path() / "minera_review_eq1_native.sqlite";
  std::filesystem::remove(path);
  export_ocel_sqlite(log, path);
  const auto back = import_ocel_sqlite(path);
  std::filesystem::remove(path);
  for (const auto& read : both(back)) {
    const auto events = first_line(serialize_ocel_csv(read, csv_options()));
    expect(events == "ocel:eid,ocel:timestamp,ocel:activity,x0,x1,x2,ocel:type:t",
           "the events frame takes the type tables' order: " + events);
    const auto objects = first_line(serialize_ocel_objects_csv(read, csv_options()));
    expect(objects == "ocel:oid,p0,p1,ocel:type", "and the objects frame: " + objects);
  }
#endif
}

// EQ1-1: the values of a native SQLite file read back (pm4py 2.7.22's
// read_ocel2_sqlite on the same file: x0 and p0 int64 1 and 0, x1 and p1 the
// texts "2024-01-05 00:00:00+00:00" and "2024-01-02 00:00:00+00:00", one
// change of p0 to 0).
void native_sqlite_values() {
#if defined(MINERA_HAS_SQLITE)
  auto log = parse_ocel_json(R"({"objectTypes":[{"name":"t","attributes":[{"name":"p0","type":"boolean"},{"name":"p1","type":"date"}]}],
 "eventTypes":[{"name":"a","attributes":[{"name":"x0","type":"boolean"},{"name":"x1","type":"date"},{"name":"x2","type":"integer"}]}],
 "objects":[{"id":"o1","type":"t","attributes":[{"name":"p0","time":"1970-01-01T00:00:00Z","value":"true"},{"name":"p1","time":"1970-01-01T00:00:00Z","value":"2024-01-02T00:00:00Z"},{"name":"p0","time":"2024-02-01T12:00:00Z","value":"false"}]},
            {"id":"o2","type":"t","attributes":[{"name":"p0","time":"1970-01-01T00:00:00Z","value":"false"}]}],
 "events":[{"id":"e1","type":"a","time":"2024-02-01T10:00:00Z","attributes":[{"name":"x0","value":"true"},{"name":"x1","value":"2024-01-05T00:00:00Z"},{"name":"x2","value":"3"}],"relationships":[{"objectId":"o1","qualifier":"q"}]},
           {"id":"e2","type":"a","time":"2024-02-01T11:00:00Z","attributes":[{"name":"x0","value":"false"},{"name":"x2","value":"4"}],"relationships":[{"objectId":"o1","qualifier":"q"},{"objectId":"o2","qualifier":"r"}]}]})");
  log.e2e.push_back({"e1", "e2", "next"});
  log.globals["g"] = std::string("v");
  const auto path = std::filesystem::temp_directory_path() / "minera_review_eq1_values.sqlite";
  std::filesystem::remove(path);
  export_ocel_sqlite(log, path);
  const auto back = import_ocel_sqlite(path);
  OcelSqliteOptions lossless;
  lossless.read_native_tables = true;
  const auto kept = import_ocel_sqlite(path, lossless);
  std::filesystem::remove(path);
  const auto value = [](const auto& attributes, const std::string& key) -> OcelValue {
    for (const auto& attribute : attributes)
      if (attribute.key == key) return attribute.value;
    return nullptr;
  };
  for (const auto& read : both(back)) {
    expect(read.events.size() == 2 && read.objects.size() == 2, "two events and two objects");
    if (read.events.size() != 2 || read.objects.size() != 2) continue;
    expect(value(read.events[0].attributes, "x0") == OcelValue(std::int64_t{1}), "a boolean reads as an integer");
    expect(value(read.events[1].attributes, "x0") == OcelValue(std::int64_t{0}), "false too");
    expect(value(read.events[0].attributes, "x1") == OcelValue(std::string("2024-01-05 00:00:00+00:00")),
           "a date reads as the type table's text");
    expect(value(read.objects[0].attributes, "p0") == OcelValue(std::int64_t{1}), "an object boolean too");
    expect(value(read.objects[0].attributes, "p1") == OcelValue(std::string("2024-01-02 00:00:00+00:00")),
           "and an object date");
    expect(read.object_changes.size() == 1 && read.object_changes[0].field == "p0" &&
               read.object_changes[0].value == OcelValue(std::int64_t{0}),
           "the change is the type table's");
    // (the transport adds the classic JSON's global-event defaults)
    expect(read.e2e.empty() && !read.globals.contains("g"), "no event links or globals");
    expect(!read.object_changes.empty() &&
               value(read.object_changes[0].cells, kOcelCumcountColumn) == OcelValue(std::int64_t{1}),
           "with its \"@@cumcount\" 1");
  }
  expect(value(kept.events[0].attributes, "x0") == OcelValue(true) && kept.e2e.size() == 1 &&
             kept.globals.size() == 1,
         "read_native_tables keeps the native values, event links and globals");
#endif
}

// EQ1-2: several unlisted ids at a deeper level.
void deeper_filter_unlisted_ids() {
  const auto log = parse_ocel_csv(kEvents, kObjects);
  for (const auto& read : both(log)) {
    for (const std::size_t level : {std::size_t{2}, std::size_t{3}}) {
      for (const auto& ids : std::vector<std::vector<std::string>>{
               {"o9", "o8"}, {"o8", "o9"}, {"q", "o8", "o9", "p"}, {"o1", "z", "o9"}}) {
        const auto smallest = ids.size() == 4 ? std::string("'o8'") : ids[0] == "o1" ? std::string("'o9'")
                                                                                    : std::string("'o8'");
        const auto error = error_of([&] { (void)filter_ocel_objects(read, ids, true, level); });
        expect(error == "KeyError: " + smallest, "native names the smallest unlisted id " + smallest + ", got " + error);
        const auto negated = error_of([&] { (void)filter_ocel_objects(read, ids, false, level); });
        expect(negated == "KeyError: " + smallest, "negated too, got " + negated);
      }
    }
  }
}

}  // namespace

int main() {
  native_sqlite_frame_order();
  native_sqlite_values();
  deeper_filter_unlisted_ids();
  if (failures != 0) {
    std::cerr << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "review EQ1 tests passed\n";
  return 0;
}
