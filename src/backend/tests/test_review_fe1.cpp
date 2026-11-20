// Review FE1: the flattening's activity and timestamp selectors, NULL cells
// of SQLite type tables, and the event time as the activity of an OCEL CSV
// log with an offset (review-harness round-fe: q/fl.py, t1/flsel.py,
// t2/nulls.py, t2/sqf.py and t3/off.py over banks o81, o81s2 and q29).
// Observed black-box on pm4py 2.7.22 (pandas 3.0.5):
//
// - FE1-1: flattening.flatten(ocel, type, parameters) merges the events
//   frame with the "case:"-prefixed objects frame and renames
//   {activity: "concept:name", timestamp: "time:timestamp"} (a dict literal,
//   so a selector naming ocel:timestamp as the activity collapses into the
//   time rename): the selected cells become concept:name and time:timestamp
//   as they are (any type; str() of the cell for the XES concept:name), the
//   canonical activity and time stay as ocel:activity and ocel:timestamp
//   columns, a selector naming an object column ("case:w", "case:ocel:type")
//   moves that cell onto the event, and renaming case:concept:name raises
//   "the specified case ID column is not contained in the dataframe". Native
//   required the selected attribute on every event and refused the others.
// - FE1-2: read_ocel2_sqlite reads each type table with read_sql, whose NULL
//   cells are None in a column of texts next to numbers, of BLOBs or of
//   NULLs alone (object), NaN in a column of texts alone (str) or numbers
//   alone (float64); an exact attribute filter with None then matches the
//   None cells. Native dropped every NULL cell (a missing attribute, NaN).
// - FE1-3: read_ocel_csv keeps a time column sharing one offset as
//   datetime64[us, UTC+02:00], so the activity of ocel:timestamp is its wall
//   clock with that offset ("2024-02-01 02:00:00.500000+02:00"), a UTC
//   column's "+00:00", a naive one's without offset. Native printed UTC.
//
// See docs/reference-2.7.22-parity.md, Review FE1.
#include "minera/ocel.hpp"
#include "minera/ocel_csv.hpp"
#include "minera/ocel_filter.hpp"
#include "minera/ocel_sqlite.hpp"
#include "minera/ocel_transform.hpp"

#include <algorithm>
#include <filesystem>
#include <functional>
#include <iostream>
#include <limits>
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

std::string error_of(const std::function<void()>& run) {
  try {
    run();
  } catch (const std::exception& error) {
    return error.what();
  }
  return {};
}

const std::string* attribute(const Event& event, std::string_view key) { return event.attribute(key); }

bool has_trace_attribute(const Trace& trace, std::string_view key) {
  return std::any_of(trace.attributes.begin(), trace.attributes.end(),
                     [&](const EventAttribute& attribute) { return attribute.key == key; });
}

// The t1/flsel.py log: one order o1 (w = 7) and two events, e1 of type A
// with x, n (integer, a float64 column), f and d (a time attribute), e2 of
// type B without them.
Ocel selector_log() {
  return parse_ocel_json(
      R"({"objectTypes":[{"name":"order","attributes":[{"name":"w","type":"integer"}]}],)"
      R"("eventTypes":[{"name":"A","attributes":[{"name":"x","type":"string"},{"name":"n","type":"integer"},)"
      R"({"name":"f","type":"float"},{"name":"d","type":"time"}]},{"name":"B","attributes":[{"name":"x","type":"string"}]}],)"
      R"("objects":[{"id":"o1","type":"order","attributes":[{"name":"w","value":7,"time":"1970-01-01T00:00:00Z"}]}],)"
      R"("events":[{"id":"e1","type":"A","time":"2024-01-01T10:00:00.5Z","attributes":[{"name":"x","value":"X1"},)"
      R"({"name":"n","value":3},{"name":"f","value":1.0},{"name":"d","value":"2024-02-02T00:00:00Z"}],)"
      R"("relationships":[{"objectId":"o1","qualifier":"q"}]},)"
      R"({"id":"e2","type":"B","time":"2024-01-02T10:00:00Z","attributes":[],"relationships":[{"objectId":"o1","qualifier":"q"}]}]})");
}

EventLog flatten_with(const Ocel& log, const std::string& type, const std::string& activity,
                      const std::string& timestamp) {
  OcelFlattenOptions options;
  options.object_type = type;
  options.event_activity_attribute = activity;
  options.event_timestamp_attribute = timestamp;
  return flatten_ocel_to_event_log(log, options);
}

void fe1_1_selectors() {
  const auto log = selector_log();
  {
    const auto flat = flatten_with(log, "order", "", "");
    const auto& e1 = flat.traces.at(0).events.at(0);
    expect(e1.activity == "A" && !e1.timestamp.empty() && attribute(e1, "ocel:activity") == nullptr,
           "FE1-1 no selector keeps the canonical columns");
  }
  {
    const auto flat = flatten_with(log, "order", "x", "");
    const auto& e1 = flat.traces.at(0).events.at(0);
    const auto& e2 = flat.traces.at(0).events.at(1);
    expect(e1.activity == "X1" && attribute(e1, "x") == nullptr && attribute(e1, "ocel:activity") != nullptr &&
               *attribute(e1, "ocel:activity") == "A",
           "FE1-1 an attribute activity renames x and keeps ocel:activity");
    expect(e2.activity_state == FirstClassAttributeState::kMissing && *attribute(e2, "ocel:activity") == "B",
           "FE1-1 an event without the cell has no concept:name");
  }
  {
    const auto flat = flatten_with(log, "order", "", "x");
    const auto& e1 = flat.traces.at(0).events.at(0);
    const auto& e2 = flat.traces.at(0).events.at(1);
    expect(e1.timestamp.empty() && attribute(e1, "time:timestamp") != nullptr &&
               *attribute(e1, "time:timestamp") == "X1" && attribute(e1, "ocel:timestamp") != nullptr,
           "FE1-1 an attribute time renames x and keeps ocel:timestamp");
    expect(e2.timestamp.empty() && attribute(e2, "time:timestamp") == nullptr,
           "FE1-1 an event without the time cell has no time:timestamp");
  }
  {
    const auto flat = flatten_with(log, "order", "ocel:eid", "");
    const auto& e1 = flat.traces.at(0).events.at(0);
    expect(e1.activity == "e1" && attribute(e1, "ocel:eid") == nullptr && *attribute(e1, "ocel:activity") == "A",
           "FE1-1 the event id as the activity");
  }
  {
    // {ocel:timestamp: concept:name, ocel:timestamp: time:timestamp} keeps the
    // last entry: the activity column stays ocel:activity.
    const auto flat = flatten_with(log, "order", "ocel:timestamp", "");
    const auto& e1 = flat.traces.at(0).events.at(0);
    expect(e1.activity_state == FirstClassAttributeState::kMissing && !e1.timestamp.empty() &&
               *attribute(e1, "ocel:activity") == "A",
           "FE1-1 the time as the activity collapses into the time rename");
  }
  {
    const auto flat = flatten_with(log, "order", "ocel:timestamp", "x");
    const auto& e1 = flat.traces.at(0).events.at(0);
    const auto& e2 = flat.traces.at(0).events.at(1);
    expect(e1.activity == "2024-01-01 10:00:00.500000+00:00" && e2.activity == "2024-01-02 10:00:00+00:00" &&
               *attribute(e1, "time:timestamp") == "X1",
           "FE1-1 the time as the activity prints str() of the UTC timestamp");
  }
  {
    const auto flat = flatten_with(log, "order", "n", "");
    expect(flat.traces.at(0).events.at(0).activity == "3.0", "FE1-1 an integer of a float64 column prints 3.0");
    const auto dated = flatten_with(log, "order", "d", "");
    expect(dated.traces.at(0).events.at(0).activity == "2024-02-02 00:00:00+00:00",
           "FE1-1 a date attribute prints str() of its timestamp");
  }
  {
    const auto flat = flatten_with(log, "order", "case:ocel:type", "");
    const auto& trace = flat.traces.at(0);
    expect(trace.events.at(0).activity == "order" && trace.events.at(1).activity == "order" &&
               !has_trace_attribute(trace, "ocel:type"),
           "FE1-1 the object type moves onto the events");
    const auto weighted = flatten_with(log, "order", "case:w", "");
    expect(weighted.traces.at(0).events.at(0).activity == "7" && !has_trace_attribute(weighted.traces.at(0), "w"),
           "FE1-1 an object attribute moves onto the events");
  }
  {
    const auto flat = flatten_with(log, "order", "w", "");
    expect(flat.traces.at(0).events.at(0).activity_state == FirstClassAttributeState::kMissing,
           "FE1-1 an object attribute without the prefix names no column");
  }
  expect(error_of([&] { (void)flatten_with(log, "order", "case:concept:name", ""); }) ==
             "the specified case ID column is not contained in the dataframe. Available columns: "
             "['case:ocel:type', 'case:w', 'concept:name', 'd', 'f', 'n', 'ocel:activity', 'ocel:eid', "
             "'time:timestamp', 'x']",
         "FE1-1 renaming the case id raises the writer's column check");
}

#if defined(MINERA_HAS_SQLITE)
// The t2/sqf.py file: three objects of type t, columns added by hand to the
// type table (m: 1, 'y', NULL; a: TEXT 'x', NULL, NULL; r: REAL 1.5, NULL,
// NULL; z: NULL alone).
Ocel sqlite_nulls() {
  const auto log = parse_ocel_json(
      R"({"objectTypes":[{"name":"t","attributes":[]}],"eventTypes":[{"name":"a","attributes":[]}],)"
      R"("objects":[{"id":"o1","type":"t"},{"id":"o2","type":"t"},{"id":"o3","type":"t"}],"events":[)"
      R"({"id":"e1","type":"a","time":"2024-01-01T00:00:00Z","attributes":[],"relationships":[)"
      R"({"objectId":"o1","qualifier":""},{"objectId":"o2","qualifier":""},{"objectId":"o3","qualifier":""}]}]})");
  const auto path = std::filesystem::temp_directory_path() / "minera_review_fe1_nulls.sqlite";
  std::filesystem::remove(path);
  export_ocel_sqlite(log, path);
  sqlite3* db = nullptr;
  if (sqlite3_open(path.string().c_str(), &db) != SQLITE_OK) throw std::runtime_error("open the written file");
  std::string table;
  sqlite3_stmt* statement = nullptr;
  if (sqlite3_prepare_v2(db, "SELECT ocel_type_map FROM object_map_type WHERE ocel_type = 't'", -1, &statement,
                         nullptr) == SQLITE_OK &&
      sqlite3_step(statement) == SQLITE_ROW)
    table = "\"object_" + std::string(reinterpret_cast<const char*>(sqlite3_column_text(statement, 0))) + "\"";
  sqlite3_finalize(statement);
  const std::string script = "ALTER TABLE " + table + " ADD COLUMN m; ALTER TABLE " + table +
                             " ADD COLUMN a TEXT; ALTER TABLE " + table + " ADD COLUMN r REAL; ALTER TABLE " +
                             table + " ADD COLUMN z; UPDATE " + table +
                             " SET m = 1, a = 'x', r = 1.5 WHERE ocel_id = 'o1'; UPDATE " + table +
                             " SET m = 'y' WHERE ocel_id = 'o2';";
  const bool written = !table.empty() && sqlite3_exec(db, script.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK;
  sqlite3_close(db);
  expect(written, "FE1-2 the hand-made columns are written");
  auto read = import_ocel_sqlite(path);
  std::filesystem::remove(path);
  return read;
}

std::vector<std::string> object_ids(const Ocel& log) {
  std::vector<std::string> ids;
  for (const auto& object : log.objects) ids.push_back(object.id);
  std::sort(ids.begin(), ids.end());
  return ids;
}

void fe1_2_sqlite_nulls() {
  const auto log = sqlite_nulls();
  const auto cell = [&](const std::string& id, const std::string& key) -> const OcelValue* {
    for (const auto& object : log.objects)
      if (object.id == id)
        for (const auto& attribute : object.attributes)
          if (attribute.key == key) return &attribute.value;
    return nullptr;
  };
  const auto none = [&](const std::string& id, const std::string& key) {
    const auto* value = cell(id, key);
    return value && std::holds_alternative<std::nullptr_t>(*value);
  };
  expect(none("o3", "m") && cell("o1", "m") && cell("o2", "m") && !none("o2", "m"),
         "FE1-2 a NULL next to a number and a text is None");
  expect(!cell("o2", "a") && !cell("o3", "a") && !cell("o2", "r") && !cell("o3", "r"),
         "FE1-2 a NULL of a text or a number column is a missing cell");
  expect(none("o1", "z") && none("o2", "z") && none("o3", "z"), "FE1-2 a column of NULLs alone holds None");
  const OcelObjectAttributeFilterValue none_value = nullptr, nan_value = std::numeric_limits<double>::quiet_NaN();
  const auto kept = [&](const std::string& key, const OcelObjectAttributeFilterValue& value) {
    return object_ids(filter_ocel_object_attribute_exact(log, key, {value}));
  };
  using Ids = std::vector<std::string>;
  expect(kept("m", none_value) == Ids({"o3"}) && kept("m", nan_value).empty(), "FE1-2 None matches m's None");
  expect(kept("a", none_value) == Ids({"o2", "o3"}) && kept("a", nan_value) == Ids({"o2", "o3"}),
         "FE1-2 a str column matches its missing cells");
  expect(kept("r", none_value).empty() && kept("r", nan_value) == Ids({"o2", "o3"}),
         "FE1-2 a float64 column matches NaN alone");
  expect(kept("z", none_value) == Ids({"o1", "o2", "o3"}) && kept("z", nan_value).empty(),
         "FE1-2 None matches a column of NULLs");
}
#endif

void fe1_3_csv_offsets() {
  const auto activities = [](const std::string& first, const std::string& second) {
    const auto log = parse_ocel_csv("ocel:eid,ocel:timestamp,ocel:activity,x0,ocel:type:t\ne0," + first +
                                    ",a,v0,['o1']\ne1," + second + ",a,v1,['o1']\n");
    const auto flat = flatten_with(log, "t", "ocel:timestamp", "x0");
    std::vector<std::string> out;
    for (const auto& event : flat.traces.at(0).events) out.push_back(event.activity);
    return out;
  };
  using Texts = std::vector<std::string>;
  expect(activities("2024-02-01 02:00:00.500+02:00", "2024-02-01 03:30:00.250+02:00") ==
             Texts({"2024-02-01 02:00:00.500000+02:00", "2024-02-01 03:30:00.250000+02:00"}),
         "FE1-3 a +02:00 column prints its wall clock");
  expect(activities("2024-02-01T02:00:00-05:30", "2024-02-01T03:00:00-05:30") ==
             Texts({"2024-02-01 02:00:00-05:30", "2024-02-01 03:00:00-05:30"}),
         "FE1-3 a -05:30 column prints its wall clock");
  expect(activities("2024-02-01 02:00:00+00:00", "2024-02-01 03:00:00+00:00") ==
             Texts({"2024-02-01 02:00:00+00:00", "2024-02-01 03:00:00+00:00"}),
         "FE1-3 a UTC column prints +00:00");
  expect(activities("2024-02-01 02:00:00", "2024-02-01 03:00:00") ==
             Texts({"2024-02-01 02:00:00", "2024-02-01 03:00:00"}),
         "FE1-3 a naive column prints no offset");
}

}  // namespace

int main() {
  fe1_1_selectors();
#if defined(MINERA_HAS_SQLITE)
  fe1_2_sqlite_nulls();
#endif
  fe1_3_csv_offsets();
  if (failures) {
    std::cerr << failures << " review FE1 check(s) failed\n";
    return 1;
  }
  std::cout << "review FE1 checks passed\n";
  return 0;
}
