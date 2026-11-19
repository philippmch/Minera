// Review EL1: pm4py 2.7.22 on the spoiled OCEL 2.0 SQLite bank of round EK
// (review-harness round-el: b53 through rt/rsw3.py, esw.py and xsw.py, the
// probes el/*.py). Observed black-box:
//
// - read_ocel2_sqlite reads the change frame's time column as it reads the
//   event column (pd.to_datetime(utc=True, format="mixed") over the change
//   rows); a column on which that raises stays as read_sql gave it: Python
//   str, int and float values, NaN for NULL, numbers first in the sort,
//   then texts by code point, NaN last;
// - the routes and writers meet those values as Python values: the JSON
//   writers write them as they are (NaN left out, or KeyError in
//   write_ocel2_json), write_ocel_xml raises lxml's TypeError on a number,
//   write_ocel2_xml AttributeError on the first event's isoformat, the CSV
//   writer their str(), the timestamp filter compares the first one,
//   descriptions and features take timestamp() of the first object's
//   lifecycle (or the first event), OC-DFG performance subtracts the first
//   pair;
// - write_ocel2_sqlite converts each type table's times with
//   pd.to_datetime(errors="coerce"): the first text's format, numbers as
//   epoch nanoseconds when they come first (an aware text then raises
//   "Mixed timezones detected"), a type whose times are all missing loses
//   its time column (KeyError: 'ocel_time'), changes included; a change of
//   the empty field is written;
// - write_ocel2_xml raises KeyError on a change whose field is no column of
//   the changes frame.
//
// See docs/reference-2.7.22-parity.md, Review EL1.
#include "minera/ocel.hpp"
#include "minera/ocel_csv.hpp"
#include "minera/ocel_filter.hpp"
#include "minera/ocel_statistics.hpp"
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

std::string error_of(const std::function<void()>& call) {
  try {
    call();
  } catch (const std::exception& error) {
    return error.what();
  }
  return "";
}

bool contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

#if defined(MINERA_HAS_SQLITE)
// Events e1 (an aware text), e2 (an integer), e3 ("abc", which pandas cannot
// read) of type a and e4 (NULL) of type b; object o1 of type t with two
// changes of field f.
std::string base(const std::string& times_a, const std::string& time_b, const std::string& changes) {
  return "CREATE TABLE \"event\" (\"ocel_id\" TEXT, \"ocel_type\" TEXT);"
         "INSERT INTO \"event\" VALUES('e1','a'),('e2','a'),('e3','a'),('e4','b');"
         "CREATE TABLE \"event_A\" (\"ocel_id\" TEXT, \"ocel_time\" TIMESTAMP, \"ocel:activity\" TEXT);"
         "INSERT INTO \"event_A\" VALUES" +
         times_a +
         ";CREATE TABLE \"event_B\" (\"ocel_id\" TEXT, \"ocel_time\" TIMESTAMP, \"ocel:activity\" TEXT);"
         "INSERT INTO \"event_B\" VALUES('e4'," +
         time_b +
         ",'b');"
         "CREATE TABLE \"event_map_type\" (\"ocel_type\" TEXT, \"ocel_type_map\" TEXT);"
         "INSERT INTO \"event_map_type\" VALUES('a','A'),('b','B');"
         "CREATE TABLE \"event_object\" (\"ocel_event_id\" TEXT, \"ocel_object_id\" TEXT, \"ocel_qualifier\" TEXT);"
         "INSERT INTO \"event_object\" VALUES('e1','o1','q'),('e2','o1','q'),('e3','o1','q'),('e4','o1','q');"
         "CREATE TABLE \"object\" (\"ocel_id\" TEXT, \"ocel_type\" TEXT);"
         "INSERT INTO \"object\" VALUES('o1','t');"
         "CREATE TABLE \"object_T\" (\"ocel_id\" TEXT, \"f\" TEXT, \"ocel_changed_field\" TEXT, \"ocel_time\" TIMESTAMP);"
         "INSERT INTO \"object_T\" VALUES('o1','x',NULL,NULL)" +
         changes +
         ";CREATE TABLE \"object_map_type\" (\"ocel_type\" TEXT, \"ocel_type_map\" TEXT);"
         "INSERT INTO \"object_map_type\" VALUES('t','T');"
         "CREATE TABLE \"object_object\" (\"ocel_source_id\" TEXT, \"ocel_target_id\" TEXT, \"ocel_qualifier\" TEXT);";
}

const std::string kRawA = "('e1','2024-01-03 10:00:00+00:00','a'),('e2',1707127200,'a'),('e3','abc','a')";
const std::string kChanges = ",('o1','y','f','2024-01-05 00:00:00+00:00'),('o1','z','f','2024-01-04 00:00:00+00:00')";

Ocel read(const std::string& script, const std::string& name) {
  const auto path = std::filesystem::temp_directory_path() / ("minera_review_el1_" + name + ".sqlite");
  std::filesystem::remove(path);
  sqlite3* db = nullptr;
  if (sqlite3_open(path.string().c_str(), &db) != SQLITE_OK) throw std::runtime_error("open failed");
  char* error = nullptr;
  const auto status = sqlite3_exec(db, script.c_str(), nullptr, nullptr, &error);
  if (error != nullptr) {
    std::cerr << error << '\n';
    sqlite3_free(error);
  }
  sqlite3_close(db);
  if (status != SQLITE_OK) throw std::runtime_error("write failed");
  auto log = import_ocel_sqlite(path);
  std::filesystem::remove(path);
  return log;
}

std::string write_error(const Ocel& log) {
  const auto path = std::filesystem::temp_directory_path() / "minera_review_el1_written.sqlite";
  auto result = error_of([&] { export_ocel_sqlite(log, path); });
  std::filesystem::remove(path);
  return result;
}

void raw_event_column() {
  const auto log = read(base(kRawA, "NULL", kChanges), "events");
  // Numbers first, then texts by code point, NaN last.
  std::vector<std::string> ids;
  for (const auto& event : log.events) ids.push_back(event.id);
  expect(ids == std::vector<std::string>{"e2", "e1", "e3", "e4"}, "raw column order");
  expect(log.events[0].raw_time == OcelRawTime::kInt && log.events[0].timestamp == "1707127200", "int kept");
  expect(log.events[1].raw_time == OcelRawTime::kStr && log.events[1].timestamp == "2024-01-03 10:00:00+00:00",
         "text kept as read");
  expect(log.events[3].raw_time == OcelRawTime::kNaN && log.events[3].timestamp.empty(), "NULL is NaN");

  // The transport keeps the kinds and the order.
  const auto back = parse_ocel_json(serialize_ocel_json(log));
  expect(back.events.size() == 4 && back.events[0].id == "e2" && back.events[0].raw_time == OcelRawTime::kInt &&
             back.events[3].raw_time == OcelRawTime::kNaN,
         "transport keeps the raw kinds");

  // write_ocel_json: numbers as numbers, NaN left out.
  const auto classic = serialize_ocel_json(log, OcelJsonFormat::kClassic, false);
  expect(contains(classic, "\"e2\":{\"ocel:activity\":\"a\",\"ocel:timestamp\":1707127200,"), "JSON number time");
  expect(contains(classic, "\"e4\":{\"ocel:activity\":\"b\",\"ocel:omap\""), "NaN time left out");
  expect(error_of([&] { (void)serialize_ocel_json(log, OcelJsonFormat::kStandard, false); }) ==
             "KeyError: 'ocel:timestamp'",
         "write_ocel2_json on NaN");
  OcelXmlExportOptions classic_xml;
  classic_xml.allow_lossy = true;
  expect(error_of([&] { (void)serialize_ocel_xml(log, classic_xml); }) ==
             "TypeError: Argument must be bytes or unicode, got 'int'",
         "write_ocel_xml on an int");
  OcelXmlExportOptions standard_xml;
  standard_xml.format = OcelXmlFormat::kStandard;
  expect(error_of([&] { (void)serialize_ocel_xml(log, standard_xml); }) ==
             "AttributeError: 'int' object has no attribute 'isoformat'",
         "write_ocel2_xml on an int");
  OcelCsvExportOptions lossy_csv;
  lossy_csv.allow_lossy = true;
  const auto csv = serialize_ocel_csv(log, lossy_csv);
  expect(contains(csv, "e2,1707127200,a") && contains(csv, "e3,abc,a") && contains(csv, "e4,,b"), "CSV str()");
  expect(write_error(log) == "ValueError: Mixed timezones detected. Pass utc=True in to_datetime or tz='UTC' in "
                             "DatetimeIndex to convert to a common timezone.",
         "write_ocel2_sqlite: a number first, then an aware text");

  expect(error_of([&] { (void)filter_ocel_events_timestamp(log, "2024-01-01 00:00:00", "2024-02-01 00:00:00"); }) ==
             "TypeError: '>=' not supported between instances of 'int' and 'datetime.datetime'",
         "timestamp filter");
  OcelEdgeAssociationOptions measured;
  measured.measure_raw_times = true;
  expect(error_of([&] { (void)edge_associations(log, OcelEdgeAssociationOptions{}); }).empty(),
         "edge associations without performance");
  const auto edge_error = error_of([&] { (void)edge_associations(log, measured); });
  expect(edge_error == "TypeError: unsupported operand type(s) for -: 'str' and 'int'",
         "edge associations: the first pair: " + edge_error);
  OcelDescriptionOptions without_times;
  without_times.include_timestamps = false;
  // Round EM: the first object's last event with a time (e3's "abc"; e4's
  // NaN is skipped), checked against pm4py (probe em dp.py).
  expect(error_of([&] { (void)describe_ocel_tuples(log, without_times); }) ==
             "AttributeError: 'str' object has no attribute 'timestamp'",
         "description: the first object's last event with a time");
}

void raw_text_writes() {
  // A text first: the texts read with its format, the number is NaT.
  const auto log = read(base("('e1','2024-01-03 10:00:00','a'),('e2',5,'a'),('e3','abc','a')", "'2024-01-01 00:00:00'",
                             kChanges),
                        "texts");
  expect(write_error(log).empty(), "texts first write");
}

void raw_change_column() {
  const auto log = read(base("('e1','2024-01-03 10:00:00+00:00','a'),('e2','2024-01-02 10:00:00+00:00','a'),"
                             "('e3','2024-01-01 10:00:00+00:00','a')",
                             "'2024-01-01 00:00:00+00:00'", kChanges + ",('o1','w','f','abc')"),
                        "changes");
  expect(log.object_changes.size() == 3, "three changes");
  bool raw = true;
  for (const auto& change : log.object_changes) raw = raw && change.raw_time == OcelRawTime::kStr;
  expect(raw, "change texts kept as str");
  // Texts by code point: "2024-01-04..." < "2024-01-05..." < "abc".
  expect(log.object_changes[0].timestamp == "2024-01-04 00:00:00+00:00" && log.object_changes[2].timestamp == "abc",
         "change order");
  const auto classic = serialize_ocel_json(log, OcelJsonFormat::kClassic, false);
  expect(contains(classic, "\"ocel:timestamp\":\"2024-01-04 00:00:00+00:00\""), "raw change time verbatim");
  OcelXmlExportOptions standard_xml;
  standard_xml.format = OcelXmlFormat::kStandard;
  expect(error_of([&] { (void)serialize_ocel_xml(log, standard_xml); }) ==
             "AttributeError: 'str' object has no attribute 'isoformat'",
         "write_ocel2_xml on a raw change time");
  const auto back = parse_ocel_json(serialize_ocel_json(log));
  expect(back.object_changes.size() == 3 && back.object_changes[0].raw_time == OcelRawTime::kStr,
         "transport keeps raw change kinds");
}

void change_fields_and_missing_times() {
  // A change of the empty field: write_ocel2_xml's KeyError.
  const auto empty_field =
      read(base("('e1','2024-01-03 10:00:00+00:00','a'),('e2','2024-01-02 10:00:00+00:00','a'),"
                "('e3','2024-01-01 10:00:00+00:00','a')",
                "'2024-01-01 00:00:00+00:00'", ",('o1',NULL,'','2024-01-05 00:00:00+00:00')"),
           "field");
  OcelXmlExportOptions standard_xml;
  standard_xml.format = OcelXmlFormat::kStandard;
  expect(error_of([&] { (void)serialize_ocel_xml(empty_field, standard_xml); }) == "KeyError: ''",
         "write_ocel2_xml on the empty field");
  expect(write_error(empty_field).empty(), "write_ocel2_sqlite writes the empty field");
  // A type whose changes all lack a time loses its time column.
  const auto untimed = read(base("('e1','2024-01-03 10:00:00+00:00','a'),('e2','2024-01-02 10:00:00+00:00','a'),"
                                 "('e3','2024-01-01 10:00:00+00:00','a')",
                                 "'2024-01-01 00:00:00+00:00'", ",('o1','y','f',NULL)"),
                            "untimed");
  expect(write_error(untimed) == "KeyError: 'ocel_time'", "write_ocel2_sqlite on untimed changes");
}
#endif

}  // namespace

int main() {
#if defined(MINERA_HAS_SQLITE)
  raw_event_column();
  raw_text_writes();
  raw_change_column();
  change_fields_and_missing_times();
#endif
  if (failures != 0) {
    std::cerr << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "review EL1 tests passed\n";
  return 0;
}
