// Review ES1: pm4py 2.7.22's OCEL writers and readers on attributes and
// columns named like the frames' own columns (review-harness round-es:
// hc/t/es1ref.py and the probes under hc/x2 to hc/x8 run these cases
// through pm4py). Observed black-box:
//
// - write_ocel_xml sets an event's "ocel:" attribute as the element's value
//   as it is: a datetime is a <date>, a str a <string>, and an int, float or
//   bool raises lxml's TypeError;
// - write_ocel2_json and write_ocel2_xml drop an "ocel:" object column but
//   write the changes of such a field;
// - write_ocel2_sqlite writes the change rows of every field ("@@cumcount",
//   "ocel:activity", ...) as rows of their type table; an object attribute
//   named "ocel_changed_field" or ocel_time shares the table's changed-field
//   or time column with the change rows; an attribute named ocel_time or
//   ocel_id with a value repeats a column of its table, and pandas raises:
//   ValueError (the time conversion), AttributeError (a dtype) or
//   InvalidIndexError (the concatenation of change rows), event types
//   first, each kind's types in sorted order;
// - read_ocel_json, read_ocel2_json, read_ocel_xml, read_ocel2_xml and
//   read_ocel2_sqlite lose an event attribute named "@@index"; read_ocel_csv
//   raises TypeError on such a column, and KeyError on missing columns in
//   its own order; an objects file's "ocel:timestamp" column is an
//   attribute;
// - read_ocel2_sqlite raises on a type table holding its id or time column
//   under both names, and on a column "ocel:field" next to a changed-field
//   column, in the order of its steps.
//
// See docs/reference-2.7.22-parity.md, Review ES1.
#include "minera/ocel.hpp"
#include "minera/ocel_csv.hpp"
#include "minera/ocel_sqlite.hpp"
#include "minera/ocel_xml.hpp"

#include <filesystem>
#include <functional>
#include <iostream>
#include <map>
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

std::string joined(const std::vector<std::string>& parts) {
  std::string text;
  for (const auto& part : parts) text += (text.empty() ? "" : "|") + part;
  return text;
}

bool has_attribute(const std::vector<OcelAttribute>& attributes, const std::string& key) {
  for (const auto& attribute : attributes)
    if (attribute.key == key) return true;
  return false;
}

// An OCEL 2.0 JSON log with one event type "a" declaring `event_declared`,
// the events `events` (each relating to o1) and the object types and
// objects given.
std::string standard_doc(const std::string& object_types, const std::string& event_declared,
                         const std::string& objects, const std::string& events) {
  return R"({"objectTypes":)" + object_types + R"(,"eventTypes":[{"name":"a","attributes":)" + event_declared +
         R"(}],"objects":)" + objects + R"(,"events":)" + events + "}";
}

std::string one_event(const std::string& attributes) {
  return R"([{"id":"e0","type":"a","time":"2024-01-01T10:00:00Z","attributes":)" + attributes +
         R"(,"relationships":[{"objectId":"o1","qualifier":"q"}]}])";
}

const std::string kPlainObjects = R"([{"id":"o1","type":"t","attributes":[]}])";
const std::string kPlainObjectTypes = R"([{"name":"t","attributes":[]}])";

// write_ocel_xml on an event's "ocel:foo" (probe es x2).
void classic_xml_prefixed() {
  const auto xml_of = [](const std::string& type, const std::string& value) {
    const auto log = parse_ocel_json(standard_doc(kPlainObjectTypes,
                                                  R"([{"name":"ocel:foo","type":")" + type + R"("}])",
                                                  kPlainObjects,
                                                  one_event(R"([{"name":"ocel:foo","value":)" + value + "}]")));
    OcelXmlExportOptions options;
    options.allow_lossy = true;  // the qualifiers
    return serialize_ocel_xml(log, options);
  };
  const auto text = xml_of("string", R"("x")");
  expect(text.find(R"(<string key="foo" value="x"/>)") != std::string::npos, "a str is a <string>: " + text);
  const auto date = xml_of("time", R"("2024-02-01T10:00:00Z")");
  expect(date.find(R"(<date key="foo" value="2024-02-01T10:00:00Z"/>)") != std::string::npos,
         "a datetime is a <date>: " + date);
  const auto error = error_of([&] { (void)xml_of("integer", "3"); });
  expect(error == "TypeError: Argument must be bytes or unicode, got 'int'", "an int raises lxml's TypeError: " + error);
}

// write_ocel2_json and write_ocel2_xml on an "ocel:" object attribute with
// a change (probe es x3).
void prefixed_changes() {
  const auto log = parse_ocel_json(standard_doc(
      R"([{"name":"t","attributes":[{"name":"ocel:note","type":"string"},{"name":"k","type":"integer"}]}])",
      "[]",
      R"([{"id":"o1","type":"t","attributes":[{"name":"ocel:note","time":"1970-01-01T00:00:00Z","value":"s0"},)"
      R"({"name":"ocel:note","time":"2024-01-05T00:00:00Z","value":"s1"},)"
      R"({"name":"k","time":"1970-01-01T00:00:00Z","value":1},{"name":"k","time":"2024-01-06T00:00:00Z","value":2}]}])",
      one_event("[]")));
  const auto json = serialize_ocel_json(log, OcelJsonFormat::kStandard, false);
  expect(json.find("\"s1\"") != std::string::npos && json.find("\"s0\"") == std::string::npos,
         "the OCEL 2.0 JSON writes the change, not the static value: " + json);
  OcelXmlExportOptions options;
  options.format = OcelXmlFormat::kStandard;
  const auto xml = serialize_ocel_xml(log, options);
  expect(xml.find(R"(<attribute name="ocel:note" time="2024-01-05T00:00:00+00:00")") != std::string::npos &&
             xml.find(">s1</attribute>") != std::string::npos &&
             xml.find(">s0<") == std::string::npos,
         "the OCEL 2.0 XML writes the change, not the static value: " + xml);
}

// The readers lose an event attribute "@@index" (probe es x5 p2.py, p3.py).
void reader_event_index() {
  const auto log = parse_ocel_json(standard_doc(
      R"([{"name":"t","attributes":[{"name":"@@index","type":"string"}]}])", R"([{"name":"@@index","type":"string"}])",
      R"([{"id":"o1","type":"t","attributes":[{"name":"@@index","time":"1970-01-01T00:00:00Z","value":"w"}]}])",
      one_event(R"([{"name":"@@index","value":"z"}])")));
  expect(log.events.size() == 1 && !has_attribute(log.events[0].attributes, "@@index"),
         "read_ocel2_json loses the event's @@index");
  expect(log.objects.size() == 1 && has_attribute(log.objects[0].attributes, "@@index"),
         "read_ocel2_json keeps the object's @@index");
  const auto declared = log.event_type_attributes.find("a");
  expect(declared == log.event_type_attributes.end() || !declared->second.contains("@@index"),
         "the event type no longer declares @@index");
}

// read_ocel_csv's faults (probe es x5 p5.py to p8.py) and an objects file's
// reserved-looking column (round ES CSV bank).
void csv_reader() {
  const auto error = [](const std::string& events, const std::string& objects = {}) {
    return error_of([&] { (void)(objects.empty() ? parse_ocel_csv(events) : parse_ocel_csv(events, objects)); });
  };
  const std::string header = "ocel:eid,ocel:timestamp,ocel:activity,@@index,ocel:type:t\n";
  expect(error(header + "e1,2024-01-01 10:00:00,a,z,['o1']\ne2,2024-01-01 09:00:00,a,y,['o1']\n") ==
             "TypeError: Invalid value 'RangeIndex(start=0, stop=2, step=1)' for dtype 'str'",
         "an @@index column raises on the frame's index");
  expect(error(header).empty(), "an @@index column without rows reads");
  expect(error("ocel:activity,ocel:type:t\na,['o1']\n") == "KeyError: 'ocel:timestamp'",
         "the timestamp column is looked up first");
  expect(error("ocel:timestamp,ocel:type:t\n2024-01-01,['o1']\n") ==
             "KeyError: \"['ocel:eid', 'ocel:activity'] not in index\"",
         "then the id and activity columns together");
  expect(error("ocel:eid,ocel:timestamp,ocel:type:t\ne1,2024-01-01,['o1']\n") ==
             "KeyError: \"['ocel:activity'] not in index\"",
         "a missing activity column alone");
  const std::string events = "ocel:eid,ocel:timestamp,ocel:activity,ocel:type:t\ne1,2024-01-01,a,['o1']\n";
  expect(error(events, "ocel:type,x\nt,1\n") == "KeyError: 'ocel:oid'", "an objects file with rows needs its id");
  const auto log = parse_ocel_csv(events, "ocel:oid,ocel:timestamp,ocel:type\no1,z,t\n");
  expect(log.objects.size() == 1 && has_attribute(log.objects[0].attributes, "ocel:timestamp"),
         "an objects file's ocel:timestamp column is an attribute");
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

std::filesystem::path temp_path(const std::string& name) {
  return std::filesystem::temp_directory_path() / ("minera_review_es1_" + name + ".sqlite");
}

// The type table `kind`_map_type maps `type` to: "name TYPE" per column,
// then one cell per row and column ("NULL", SQLite's text of the value).
std::vector<std::string> type_table(const std::filesystem::path& path, const std::string& kind,
                                    const std::string& type) {
  sqlite3* db = nullptr;
  std::vector<std::string> out;
  if (sqlite3_open(path.string().c_str(), &db) != SQLITE_OK) return out;
  std::string token;
  sqlite3_stmt* statement = nullptr;
  const auto map_sql = "SELECT ocel_type_map FROM " + kind + "_map_type WHERE ocel_type = ?1";
  if (sqlite3_prepare_v2(db, map_sql.c_str(), -1, &statement, nullptr) == SQLITE_OK) {
    sqlite3_bind_text(statement, 1, type.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(statement) == SQLITE_ROW)
      token = reinterpret_cast<const char*>(sqlite3_column_text(statement, 0));
  }
  sqlite3_finalize(statement);
  const auto table = "\"" + kind + "_" + token + "\"";
  if (sqlite3_prepare_v2(db, ("PRAGMA table_info(" + table + ")").c_str(), -1, &statement, nullptr) == SQLITE_OK)
    while (sqlite3_step(statement) == SQLITE_ROW)
      out.push_back(std::string(reinterpret_cast<const char*>(sqlite3_column_text(statement, 1))) + " " +
                    reinterpret_cast<const char*>(sqlite3_column_text(statement, 2)));
  sqlite3_finalize(statement);
  if (sqlite3_prepare_v2(db, ("SELECT * FROM " + table).c_str(), -1, &statement, nullptr) == SQLITE_OK)
    while (sqlite3_step(statement) == SQLITE_ROW)
      for (int column = 0; column < sqlite3_column_count(statement); ++column)
        out.emplace_back(sqlite3_column_type(statement, column) == SQLITE_NULL
                             ? "NULL"
                             : reinterpret_cast<const char*>(sqlite3_column_text(statement, column)));
  sqlite3_finalize(statement);
  sqlite3_close(db);
  return out;
}

// Exports an OCEL 2.0 JSON log; the error text when the writer raises.
std::string export_error(const std::string& name, const std::string& json) {
  const auto log = parse_ocel_json(json);
  const auto path = temp_path(name);
  const auto error = error_of([&] { export_ocel_sqlite(log, path); });
  std::filesystem::remove(path);
  return error;
}

std::vector<std::string> exported_table(const std::string& name, const std::string& json, const std::string& kind,
                                        const std::string& type) {
  const auto log = parse_ocel_json(json);
  const auto path = temp_path(name);
  export_ocel_sqlite(log, path);
  auto table = type_table(path, kind, type);
  std::filesystem::remove(path);
  return table;
}

std::string object_doc(const std::string& declared, const std::string& objects, const std::string& events = {}) {
  return standard_doc(R"([{"name":"t","attributes":)" + declared + "}]", "[]", objects,
                      events.empty() ? one_event("[]") : events);
}

const std::string kStatic = R"("time":"1970-01-01T00:00:00Z")";

// write_ocel2_sqlite's change rows of reserved-looking fields and the shared
// changed-field column (probe es x4).
void sqlite_writer_columns() {
  const auto objects = [](const std::string& name) {
    return R"([{"id":"o1","type":"t","attributes":[{"name":")" + name + R"(",)" + kStatic +
           R"(,"value":"s0"},{"name":")" + name + R"(","time":"2024-01-05T00:00:00Z","value":"s1"},)"
           R"({"name":"k",)" + kStatic + R"(,"value":1},{"name":"k","time":"2024-01-06T00:00:00Z","value":2}]},)"
           R"({"id":"o2","type":"t","attributes":[{"name":")" + name + R"(","time":"2024-01-07T00:00:00Z","value":"c1"}]},)"
           R"({"id":"o3","type":"t","attributes":[{"name":"k",)" + kStatic + R"(,"value":5}]}])";
  };
  const auto declared = [](const std::string& name) {
    return R"([{"name":")" + name + R"(","type":"string"},{"name":"k","type":"integer"}])";
  };
  const auto events = R"([{"id":"e1","type":"a","time":"2024-01-01T10:00:00Z","attributes":[],)"
                      R"("relationships":[{"objectId":"o1","qualifier":"q"},{"objectId":"o2","qualifier":"q"},)"
                      R"({"objectId":"o3","qualifier":"q"}]}])";
  for (const std::string name : {"@@cumcount", "ocel:activity"}) {
    const auto table = exported_table("columns", object_doc(declared(name), objects(name), events), "object", "t");
    expect(joined(table) == "ocel_id TEXT|" + name +
                                " TEXT|k REAL|ocel_changed_field TEXT|ocel_time TIMESTAMP|"
                                "o1|s0|1.0|NULL|NULL|o2|c1|NULL|NULL|NULL|o3|NULL|5.0|NULL|NULL|"
                                "o1|s1|NULL|" + name + "|2024-01-05 00:00:00+00:00|"
                                "o1|NULL|2.0|k|2024-01-06 00:00:00+00:00",
           "the change rows of " + name + ": " + joined(table));
  }
  expect(export_error("changed", object_doc(declared("ocel_changed_field"), objects("ocel_changed_field"), events)) ==
             "InvalidIndexError: Reindexing only valid with uniquely valued Index objects",
         "a change of ocel_changed_field repeats the field column");
}

// write_ocel2_sqlite on attributes named ocel_time and ocel_id (probes es x5
// p4.py, x7 p.py to p3.py).
void sqlite_writer_hard_names() {
  const auto declared = [](const std::string& type) {
    return R"([{"name":"ocel_time","type":")" + type + R"("},{"name":"k","type":"integer"}])";
  };
  const auto with_change = [](const std::string& value) {
    return R"([{"id":"o1","type":"t","attributes":[{"name":"ocel_time",)" + kStatic + R"(,"value":)" + value +
           R"(},{"name":"k",)" + kStatic + R"(,"value":1},{"name":"k","time":"2024-01-03T00:00:00Z","value":2}]},)"
           R"({"id":"o2","type":"t","attributes":[{"name":"k",)" + kStatic + R"(,"value":3}]}])";
  };
  const auto two_objects = R"([{"id":"e1","type":"a","time":"2024-01-01T10:00:00Z","attributes":[],)"
                           R"("relationships":[{"objectId":"o1","qualifier":"q"},{"objectId":"o2","qualifier":"q"}]}])";
  auto table = exported_table("static", object_doc(declared("string"), R"([{"id":"o1","type":"t","attributes":[{"name":"ocel_time",)" +
                                                                            kStatic + R"(,"value":"h"},{"name":"k",)" + kStatic +
                                                                            R"(,"value":1}]}])"),
                              "object", "t");
  expect(joined(table) == "ocel_id TEXT|ocel_time TEXT|k INTEGER|o1|h|1",
         "a static ocel_time without changes is a column: " + joined(table));
  table = exported_table("shared", object_doc(declared("string"), with_change(R"("h")"), two_objects), "object", "t");
  expect(joined(table) == "ocel_id TEXT|ocel_time TEXT|k INTEGER|ocel_changed_field TEXT|"
                          "o1|h|1|NULL|o2|NULL|3|NULL|o1|2024-01-03T00:00:00+00:00|2|k",
         "a static ocel_time shares the time column, the change times as isoformat: " + joined(table));
  table = exported_table("shared_int", object_doc(declared("integer"), with_change("5"), two_objects), "object", "t");
  expect(joined(table) == "ocel_id TEXT|ocel_time TEXT|k INTEGER|ocel_changed_field TEXT|"
                          "o1|5.0|1|NULL|o2|NULL|3|NULL|o1|2024-01-03T00:00:00+00:00|2|k",
         "an integer ocel_time with a missing cell is a float there: " + joined(table));
  table = exported_table("shared_time", object_doc(declared("time"), with_change(R"("2024-05-01T00:00:00Z")"), two_objects),
                         "object", "t");
  expect(joined(table) == "ocel_id TEXT|ocel_time TIMESTAMP|k INTEGER|ocel_changed_field TEXT|"
                          "o1|2024-05-01 00:00:00+00:00|1|NULL|o2|NULL|3|NULL|o1|2024-01-03 00:00:00+00:00|2|k",
         "datetimes alone stay a TIMESTAMP column: " + joined(table));
  table = exported_table(
      "null_change",
      object_doc(R"([{"name":"k","type":"string"},{"name":"ocel_time","type":"string"}])",
                 R"([{"id":"o1","type":"t","attributes":[{"name":"k",)" + kStatic +
                     R"(,"value":1},{"name":"ocel_time","time":"2024-01-03T00:00:00Z","value":"x"},)"
                     R"({"name":"ocel_time","time":"2024-01-04T00:00:00Z","value":null}]}])"),
      "object", "t");
  expect(joined(table) == "ocel_id TEXT|k REAL|ocel_time TEXT|ocel_changed_field TEXT|"
                          "o1|1.0|x|NULL|o1|NULL|2024-01-04T00:00:00+00:00|ocel_time",
         "a change of ocel_time without a value is written: " + joined(table));

  const std::string assemble = "ValueError: cannot assemble with duplicate keys";
  const std::string dtype = "AttributeError: 'DataFrame' object has no attribute 'dtype'";
  const std::string reindex = "InvalidIndexError: Reindexing only valid with uniquely valued Index objects";
  const auto event_doc = [](const std::string& attributes) {
    return standard_doc(kPlainObjectTypes,
                        R"([{"name":"ocel_time","type":"string"},{"name":"ocel_id","type":"string"}])",
                        kPlainObjects, one_event(attributes));
  };
  expect(export_error("ev_time", event_doc(R"([{"name":"ocel_time","value":"v"}])")) == assemble,
         "an event ocel_time raises on the time conversion");
  expect(export_error("ev_id", event_doc(R"([{"name":"ocel_id","value":"v"}])")) == dtype,
         "an event ocel_id raises on a dtype");
  expect(export_error("ev_both", event_doc(R"([{"name":"ocel_id","value":"v"},{"name":"ocel_time","value":"w"}])")) ==
             assemble,
         "the time conversion comes first");
  expect(export_error("ev_null", event_doc(R"([{"name":"ocel_time","value":null}])")).empty(),
         "an event ocel_time without a value is no column");
  const auto two_types = R"({"objectTypes":[{"name":"t","attributes":[]}],"eventTypes":[)"
                         R"({"name":"a","attributes":[{"name":"ocel_id","type":"string"}]},)"
                         R"({"name":"b","attributes":[{"name":"ocel_time","type":"string"}]}],)"
                         R"("objects":[{"id":"o1","type":"t","attributes":[]}],"events":[)"
                         R"({"id":"e1","type":"b","time":"2024-01-01T10:00:00Z","attributes":[{"name":"ocel_time","value":"w"}],"relationships":[{"objectId":"o1","qualifier":"q"}]},)"
                         R"({"id":"e2","type":"a","time":"2024-01-02T10:00:00Z","attributes":[{"name":"ocel_id","value":"v"}],"relationships":[{"objectId":"o1","qualifier":"q"}]}]})";
  expect(export_error("ev_order", two_types) == dtype, "event types go in sorted order");
  const auto id_declared = R"([{"name":"ocel_id","type":"string"},{"name":"k","type":"integer"}])";
  expect(export_error("st_id", object_doc(id_declared, R"([{"id":"o1","type":"t","attributes":[{"name":"ocel_id",)" +
                                                          kStatic + R"(,"value":"h"}]}])")) == dtype,
         "an object ocel_id without change rows raises on a dtype");
  expect(export_error("stch_id",
                      object_doc(id_declared, R"([{"id":"o1","type":"t","attributes":[{"name":"ocel_id",)" + kStatic +
                                                  R"(,"value":"h"},{"name":"k",)" + kStatic +
                                                  R"(,"value":1},{"name":"k","time":"2024-01-03T00:00:00Z","value":2}]}])")) ==
             reindex,
         "with change rows on their concatenation");
  expect(export_error("ch_time",
                      object_doc(R"([{"name":"ocel_time","type":"string"}])",
                                 R"([{"id":"o1","type":"t","attributes":[{"name":"ocel_time","time":"2024-01-03T00:00:00Z","value":"h"},)"
                                 R"({"name":"ocel_time","time":"2024-01-04T00:00:00Z","value":"i"}]}])")) == assemble,
         "a change of ocel_time with a value raises on the time conversion");
  const auto sorted_types =
      R"({"objectTypes":[{"name":"z","attributes":[{"name":"ocel_id","type":"string"}]},)"
      R"({"name":"a","attributes":[{"name":"ocel_id","type":"string"},{"name":"k","type":"integer"}]}],)"
      R"("eventTypes":[{"name":"a","attributes":[]}],"objects":[)"
      R"({"id":"o2","type":"z","attributes":[{"name":"ocel_id",)" + kStatic + R"(,"value":"h"}]},)"
      R"({"id":"o1","type":"a","attributes":[{"name":"ocel_id",)" + kStatic + R"(,"value":"g"},{"name":"k",)" + kStatic +
      R"(,"value":1},{"name":"k","time":"2024-01-04T00:00:00Z","value":2}]}],)"
      R"("events":[{"id":"e1","type":"a","time":"2024-01-01T10:00:00Z","attributes":[],)"
      R"("relationships":[{"objectId":"o2","qualifier":"q"},{"objectId":"o1","qualifier":"q"}]}]})";
  expect(export_error("ob_order", sorted_types) == reindex, "object types go in sorted order");

  // Attributes named like the changes frame's own columns are those columns
  // there (probe es x10).
  const auto framed = [](const std::string& name, const std::string& type, const std::string& value) {
    return object_doc(R"([{"name":")" + name + R"(","type":")" + type + R"("},{"name":"k","type":"integer"}])",
                      R"([{"id":"o1","type":"t","attributes":[{"name":")" + name + R"(",)" + kStatic +
                          R"(,"value":)" + value + R"(},{"name":"k",)" + kStatic +
                          R"(,"value":1},{"name":"k","time":"2024-01-06T00:00:00Z","value":2}]}])");
  };
  table = exported_table("field_name", framed("ocel:field", "string", R"("st")"), "object", "t");
  expect(joined(table) == "ocel_id TEXT|ocel:field TEXT|k INTEGER|ocel_changed_field TEXT|ocel_time TIMESTAMP|"
                          "o1|st|1|NULL|NULL|o1|NULL|2|k|2024-01-06 00:00:00+00:00",
         "an object attribute ocel:field: " + joined(table));
  table = exported_table("time_name", framed("ocel:timestamp", "date", R"("2024-02-02T00:00:00Z")"), "object", "t");
  expect(joined(table) == "ocel_id TEXT|ocel:timestamp TIMESTAMP|k INTEGER|ocel_changed_field TEXT|ocel_time TIMESTAMP|"
                          "o1|2024-02-02 00:00:00+00:00|1|NULL|NULL|o1|NULL|2|k|2024-01-06 00:00:00+00:00",
         "an object attribute ocel:timestamp: " + joined(table));
}

// A small pm4py-shaped file (two event types, two object types, a change of
// o1); `columns` adds TEXT columns ("table.column"), `sql` runs after.
std::filesystem::path reader_case(const std::string& name, const std::vector<std::string>& columns,
                                  const std::string& sql = {}) {
  const auto path = temp_path(name);
  std::filesystem::remove(path);
  sqlite3* db = nullptr;
  if (sqlite3_open(path.string().c_str(), &db) != SQLITE_OK) throw std::runtime_error("cannot open fixture");
  exec_sql(db, R"(
    CREATE TABLE event (ocel_id TEXT, ocel_type TEXT); INSERT INTO event VALUES ('e1','a'),('e2','b');
    CREATE TABLE object (ocel_id TEXT, ocel_type TEXT); INSERT INTO object VALUES ('o1','t'),('o2','u');
    CREATE TABLE event_map_type (ocel_type TEXT, ocel_type_map TEXT); INSERT INTO event_map_type VALUES ('a','A'),('b','B');
    CREATE TABLE object_map_type (ocel_type TEXT, ocel_type_map TEXT); INSERT INTO object_map_type VALUES ('t','T'),('u','U');
    CREATE TABLE event_object (ocel_event_id TEXT, ocel_object_id TEXT, ocel_qualifier TEXT);
    INSERT INTO event_object VALUES ('e1','o1','q'),('e2','o2','q');
    CREATE TABLE object_object (ocel_source_id TEXT, ocel_target_id TEXT, ocel_qualifier TEXT);
    CREATE TABLE event_A (ocel_id TEXT, ocel_time TIMESTAMP, "ocel:activity" TEXT);
    INSERT INTO event_A VALUES ('e1','2024-01-01 10:00:00+00:00','a');
    CREATE TABLE event_B (ocel_id TEXT, ocel_time TIMESTAMP, "ocel:activity" TEXT);
    INSERT INTO event_B VALUES ('e2','2024-01-02 10:00:00+00:00','b');
    CREATE TABLE object_T (ocel_id TEXT, k INTEGER, ocel_changed_field TEXT, ocel_time TIMESTAMP);
    INSERT INTO object_T VALUES ('o1',1,NULL,NULL),('o1',2,'k','2024-01-03 00:00:00+00:00');
    CREATE TABLE object_U (ocel_id TEXT, k INTEGER); INSERT INTO object_U VALUES ('o2',2);)");
  for (const auto& column : columns) {
    const auto dot = column.find('.');
    exec_sql(db, "ALTER TABLE " + column.substr(0, dot) + " ADD COLUMN \"" + column.substr(dot + 1) + "\" TEXT");
  }
  if (!sql.empty()) exec_sql(db, sql);
  sqlite3_close(db);
  return path;
}

std::string read_error(const std::string& name, const std::vector<std::string>& columns, const std::string& sql = {}) {
  const auto path = reader_case(name, columns, sql);
  const auto error = error_of([&] { (void)import_ocel_sqlite(path); });
  std::filesystem::remove(path);
  return error;
}

// read_ocel2_sqlite on repeated labels (probes es x8 p3.py, p4.py).
void sqlite_reader_repeated_labels() {
  const std::string str = "AttributeError: 'DataFrame' object has no attribute 'str'";
  const std::string dtype = "AttributeError: 'DataFrame' object has no attribute 'dtype'";
  const std::string reindex = "InvalidIndexError: Reindexing only valid with uniquely valued Index objects";
  const std::string labels = "ValueError: cannot reindex on an axis with duplicate labels";
  expect(read_error("base", {}).empty(), "the plain file reads");
  expect(read_error("ev_eid", {"event_A.ocel:eid"}) == str, "an event table's id under both names");
  expect(read_error("ob_oid", {"object_U.ocel:oid"}) == str, "an object table's id under both names");
  const std::string untitled_b = "DROP TABLE event_B; CREATE TABLE event_B (ocel_time TIMESTAMP, \"ocel:activity\" TEXT); "
                                 "INSERT INTO event_B VALUES ('2024-01-02 10:00:00+00:00','b')";
  const std::string untitled_a = "DROP TABLE event_A; CREATE TABLE event_A (ocel_time TIMESTAMP, \"ocel:activity\" TEXT); "
                                 "INSERT INTO event_A VALUES ('2024-01-01 10:00:00+00:00','a')";
  expect(read_error("ev_eid_order", {"event_A.ocel:eid"}, untitled_b) == str,
         "each table in turn: the first table's fault");
  expect(read_error("ev_missing_first", {"event_B.ocel:eid"}, untitled_a) ==
             "KeyError: 'ocel:eid'",
         "each table in turn: the first table's missing id");
  expect(read_error("ev_ts", {"event_A.ocel:timestamp"}) == reindex,
         "an event table's time under both names, the tables' columns differing");
  expect(read_error("ev_ts_both", {"event_A.ocel:timestamp", "event_B.ocel:timestamp"}) == dtype,
         "the same columns in every table: no reindex, the dtype raises");
  expect(read_error("ev_ts_ob_oid", {"event_B.ocel:timestamp", "object_T.ocel:oid"}) == str,
         "the object tables' own faults come before the events' concatenation");
  expect(read_error("ob_ts", {"object_T.ocel:timestamp"}) == reindex, "an object table's time under both names");
  expect(read_error("ob_field", {"object_U.ocel:field"}) == labels,
         "an ocel:field column next to a changed-field column");
  expect(read_error("ob_field_alone", {},
                    "DROP TABLE object_T; CREATE TABLE object_T (ocel_id TEXT, k INTEGER, \"ocel:field\" TEXT); "
                    "INSERT INTO object_T VALUES ('o1',1,NULL)")
             .empty(),
         "an ocel:field column without a changed-field column reads");
  expect(read_error("ev_ts_ob_field", {"event_A.ocel:timestamp", "object_U.ocel:field"}) == reindex,
         "the events' concatenation before the objects' split");
  expect(read_error("ob_ts_field", {"object_T.ocel:timestamp", "object_U.ocel:field"}) == reindex,
         "the objects' concatenation before their split");
  const auto path = reader_case("index", {"event_A.@@index", "object_T.@@index"},
                                "UPDATE event_A SET \"@@index\" = 'z'; UPDATE object_T SET \"@@index\" = 'w'");
  const auto log = import_ocel_sqlite(path);
  std::filesystem::remove(path);
  bool event_index = false, object_index = false;
  for (const auto& event : log.events) event_index = event_index || has_attribute(event.attributes, "@@index");
  for (const auto& object : log.objects) object_index = object_index || has_attribute(object.attributes, "@@index");
  expect(!event_index && object_index, "read_ocel2_sqlite loses the event tables' @@index, not the objects'");

  // The O2O qualifier column is looked up after the change rows (probe es
  // x11).
  const std::string o2o = "DROP TABLE object_object; CREATE TABLE object_object (ocel_source_id TEXT, "
                          "ocel_target_id TEXT, q TEXT); INSERT INTO object_object VALUES ('o1','o2','r');";
  const std::string untimed = "DROP TABLE object_T; CREATE TABLE object_T (ocel_id TEXT, k INTEGER, "
                              "ocel_changed_field TEXT); INSERT INTO object_T VALUES ('o1',1,NULL),('o1',2,'k');";
  expect(read_error("q_timed", {}, o2o) == "KeyError: 'ocel:qualifier'", "an O2O table without qualifiers");
  expect(read_error("q_untimed", {}, o2o + untimed) == "KeyError: 'ocel:timestamp'",
         "untimed change rows raise before the O2O qualifier");
}

#endif

}  // namespace

int main() {
  classic_xml_prefixed();
  prefixed_changes();
  reader_event_index();
  csv_reader();
#if defined(MINERA_HAS_SQLITE)
  sqlite_writer_columns();
  sqlite_writer_hard_names();
  sqlite_reader_repeated_labels();
#endif
  if (failures != 0) {
    std::cerr << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "review ES1 tests passed\n";
  return 0;
}
