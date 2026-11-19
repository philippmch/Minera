// Review EJ1: pm4py 2.7.22's OCEL 2.0 readers and writers on the round-EJ
// banks (review-harness round-ej: g2/o2gen.py logs with declared attribute
// types, object changes and O2O links written by hand and by pm4py's
// writers, rt/rsw2.py sweeps, g2/xfz.py and g2/jfz.py reader fuzz, t/ and
// hc/ hand probes). Observed black-box:
//
// - read_ocel2_xml walks the root's sections in document order and reads
//   each value where it meets it, by the type its table declared so far: a
//   type naming "float" goes through float(), one naming "date" through
//   dateutil, any other (integer and boolean too) stays text, no text gives
//   "None" (float() and dateutil raise on it); an object attribute's value
//   is read before its time (none raises AttributeError on .startswith),
//   an event's time before its attributes; a declared attribute without a
//   type raises on a value under it; a relation to an object not read yet
//   is dropped, and a file without relations raises AttributeError on
//   .index;
// - read_ocel2_json takes no time for an attribute's first entry and reads
//   a change without a time (or with null) as NaT; write_ocel2_json raises
//   KeyError 'ocel:timestamp' on it, write_ocel_json leaves the key out and
//   write_ocel2_xml prints "NaT";
// - both OCEL 2.0 writers list an object's changes in log order;
// - pandas types each frame column over all of its rows, so an integer
//   column with a missing cell is float64: in read_ocel2_json's frames
//   also through rows it drops later, in read_ocel2_sqlite's through the
//   change rows of the same type table;
// - ocel_flattening keeps a declared date attribute a datetime, which the
//   XES exporter writes as <date>.
//
// See docs/reference-2.7.22-parity.md, Review EJ1.
#include "minera/ocel.hpp"
#include "minera/ocel_sqlite.hpp"
#include "minera/ocel_transform.hpp"
#include "minera/ocel_xml.hpp"

#include <filesystem>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <variant>
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

const OcelValue* value_of(const std::vector<OcelAttribute>& attributes, const std::string& key) {
  for (const auto& attribute : attributes)
    if (attribute.key == key) return &attribute.value;
  return nullptr;
}

bool is_text(const OcelValue* value, const std::string& text) {
  return value != nullptr && std::holds_alternative<std::string>(*value) && std::get<std::string>(*value) == text;
}

bool is_double(const OcelValue* value, double number) {
  return value != nullptr && std::holds_alternative<double>(*value) && std::get<double>(*value) == number;
}

const std::string kHead = "<?xml version='1.0' encoding='UTF-8'?>\n<log>";
const std::string kObject = "<objects><object id=\"o1\" type=\"t\"><attributes>";
const std::string kEvent =
    "<events><event id=\"e1\" type=\"a\" time=\"2024-01-01T10:00:00Z\"><attributes/><objects><relationship "
    "object-id=\"o1\" qualifier=\"q\"/></objects></event></events>";

std::string object_types(const std::string& attributes) {
  return "<object-types><object-type name=\"t\"><attributes>" + attributes + "</attributes></object-type></object-types>";
}

std::string event_types(const std::string& attributes = "") {
  return "<event-types><event-type name=\"a\"><attributes>" + attributes + "</attributes></event-type></event-types>";
}

// Types, one object o1 with the given attribute entries, one event e1.
std::string xml_with_object(const std::string& declared, const std::string& entries) {
  return kHead + object_types(declared) + event_types() + kObject + entries + "</attributes></object></objects>" +
         kEvent + "</log>";
}

void xml_values() {
  const auto read = [](const std::string& xml) { return error_of([&] { (void)parse_ocel_xml(xml); }); };
  const auto log = parse_ocel_xml(xml_with_object(
      "<attribute name=\"n\" type=\"integer\"/><attribute name=\"b\" type=\"boolean\"/>"
      "<attribute name=\"f\" type=\"float\"/>",
      "<attribute name=\"n\" time=\"1970-01-01T00:00:00Z\">5</attribute>"
      "<attribute name=\"b\" time=\"1970-01-01T00:00:00Z\">true</attribute>"
      "<attribute name=\"f\" time=\"1970-01-01T00:00:00Z\">1e3</attribute>"));
  expect(is_text(value_of(log.objects.at(0).attributes, "n"), "5"), "an integer-typed value stays text");
  expect(is_text(value_of(log.objects.at(0).attributes, "b"), "true"), "a boolean-typed value stays text");
  expect(is_double(value_of(log.objects.at(0).attributes, "f"), 1000.0), "a float-typed value goes through float()");
  expect(read(xml_with_object("<attribute name=\"f\" type=\"float\"/>",
                              "<attribute name=\"f\" time=\"1970-01-01T00:00:00Z\">abc</attribute>")) ==
             "could not convert string to float: 'abc'",
         "an unreadable float");
  expect(read(xml_with_object("<attribute name=\"f\" type=\"float\"/>",
                              "<attribute name=\"f\" time=\"1970-01-01T00:00:00Z\"/>")) ==
             "float() argument must be a string or a real number, not 'NoneType'",
         "a float without text");
  const auto none = parse_ocel_xml(xml_with_object("<attribute name=\"s\" type=\"string\"/>",
                                                   "<attribute name=\"s\" time=\"1970-01-01T00:00:00Z\"/>"));
  expect(is_text(value_of(none.objects.at(0).attributes, "s"), "None"), "a text without text reads \"None\"");
  const auto empty_name = parse_ocel_xml(xml_with_object(
      "<attribute name=\"\" type=\"string\"/>", "<attribute name=\"\" time=\"1970-01-01T00:00:00Z\">x</attribute>"));
  expect(is_text(value_of(empty_name.objects.at(0).attributes, ""), "x"), "an empty name is a column");
  expect(read(xml_with_object("<attribute name=\"s\"/>",
                              "<attribute name=\"s\" time=\"1970-01-01T00:00:00Z\">x</attribute>")) ==
             "argument of type 'NoneType' is not iterable",
         "a declaration without a type");
}

void xml_walk_order() {
  const auto read = [](const std::string& xml) { return error_of([&] { (void)parse_ocel_xml(xml); }); };
  expect(read(xml_with_object("<attribute name=\"s\" type=\"string\"/>", "<attribute name=\"s\">x</attribute>")) ==
             "'NoneType' object has no attribute 'startswith'",
         "an object attribute without a time");
  expect(read(xml_with_object("<attribute name=\"f\" type=\"float\"/>", "<attribute name=\"f\">abc</attribute>")) ==
             "could not convert string to float: 'abc'",
         "an object attribute's value is read before its time");
  expect(read(kHead + object_types("") + event_types("<attribute name=\"f\" type=\"float\"/>") + kObject +
              "</attributes></object></objects><events><event id=\"e1\" type=\"a\" time=\"garbage\"><attributes>"
              "<attribute name=\"f\">abc</attribute></attributes><objects><relationship object-id=\"o1\" "
              "qualifier=\"q\"/></objects></event></events></log>") ==
             "String does not contain a date: garbage",
         "an event's time is read before its attributes");
  // A change time "0" is a static entry's (not parsed).
  const auto epoch = parse_ocel_xml(xml_with_object(
      "<attribute name=\"s\" type=\"string\"/>",
      "<attribute name=\"s\" time=\"0\">x</attribute><attribute name=\"s\" time=\"2024-02-01T00:00:00Z\">y</attribute>"));
  expect(epoch.object_changes.size() == 1 && is_text(value_of(epoch.objects.at(0).attributes, "s"), "x"),
         "a static entry with time 0");
  // Type tables after the objects and events: the values read before them
  // stay text, and a date declared after its values is a text column.
  const auto late = parse_ocel_xml(
      kHead + kObject + "<attribute name=\"d\" time=\"1970-01-01T00:00:00Z\">2024-02-01T10:00:00Z</attribute>" +
      "</attributes></object></objects>" +
      "<events><event id=\"e1\" type=\"a\" time=\"2024-01-01T10:00:00Z\"><attributes><attribute name=\"f\">2.5"
      "</attribute></attributes><objects><relationship object-id=\"o1\" qualifier=\"q\"/></objects></event>"
      "</events>" +
      object_types("<attribute name=\"d\" type=\"date\"/>") + event_types("<attribute name=\"f\" type=\"float\"/>") +
      "</log>");
  expect(is_text(value_of(late.events.at(0).attributes, "f"), "2.5"), "a value read before its type table");
  expect(late.object_type_attributes.at("t").at("d") == "string", "a date declared after its values");
  // A relation to an object not read yet is dropped.
  const std::string types = object_types("") + event_types();
  expect(read(kHead + types + kEvent + "<objects><object id=\"o1\" type=\"t\"><attributes/></object></objects></log>") ==
             "'NoneType' object has no attribute 'index'",
         "events before the objects");
  const auto split = parse_ocel_xml(
      kHead + types + "<objects><object id=\"o1\" type=\"t\"><attributes/></object></objects>" +
      "<events><event id=\"e1\" type=\"a\" time=\"2024-01-01T10:00:00Z\"><objects><relationship object-id=\"o1\" "
      "qualifier=\"q\"/><relationship object-id=\"o2\" qualifier=\"q\"/></objects></event></events>" +
      "<objects><object id=\"o2\" type=\"t\"><attributes/></object></objects>" +
      "<events><event id=\"e2\" type=\"a\" time=\"2024-01-01T11:00:00Z\"><objects><relationship object-id=\"o2\" "
      "qualifier=\"q\"/></objects></event></events></log>");
  expect(split.relations.size() == 2 && split.relations.at(0).object_id == "o1" &&
             split.relations.at(1).event_id == "e2",
         "split sections: only relations to objects read before");
  expect(read(kHead + types + "<objects><object id=\"o1\" type=\"t\"><attributes/></object></objects>" +
              "<events><event id=\"e1\" type=\"a\" time=\"2024-01-01T10:00:00Z\"><attributes/></event></events></log>") ==
             "'NoneType' object has no attribute 'index'",
         "a file without relations");
}

// One object type t (p string), one event e1 related to o1 with the given
// attribute entries.
std::string json_with_object(const std::string& entries, const std::string& more_objects = "") {
  return "{\"objectTypes\":[{\"name\":\"t\",\"attributes\":[{\"name\":\"p\",\"type\":\"string\"}]}],"
         "\"eventTypes\":[{\"name\":\"a\",\"attributes\":[]}],"
         "\"objects\":[{\"id\":\"o1\",\"type\":\"t\",\"attributes\":[" +
         entries + "]}" + more_objects +
         "],\"events\":[{\"id\":\"e1\",\"type\":\"a\",\"time\":\"2024-01-01T00:00:00Z\","
         "\"relationships\":[{\"objectId\":\"o1\",\"qualifier\":\"q\"}]}]}";
}

void json_change_times() {
  const auto read = [](const std::string& json) { return error_of([&] { (void)parse_ocel_json(json); }); };
  expect(read(json_with_object("{\"name\":\"p\",\"time\":null,\"value\":\"x\"}")).empty(), "a static value's null time");
  expect(read(json_with_object("{\"name\":\"p\",\"time\":0,\"value\":\"x\"}")).empty(), "a static value's int time");
  const auto log = parse_ocel_json(json_with_object(
      "{\"name\":\"p\",\"time\":\"1970-01-01T00:00:00Z\",\"value\":\"x\"},{\"name\":\"p\",\"value\":\"y\"},"
      "{\"name\":\"p\",\"time\":\"2024-03-01T00:00:00Z\",\"value\":\"z\"},{\"name\":\"p\",\"time\":\"2024-02-01T00:00:00Z\","
      "\"value\":\"w\"}"));
  expect(log.object_changes.size() == 3 && log.object_changes.at(0).timestamp.empty(),
         "a change without a time is NaT");
  // A change time of another JSON type stays refused, after the walk.
  expect(read(json_with_object("{\"name\":\"p\",\"time\":\"1970-01-01T00:00:00Z\",\"value\":\"x\"},"
                               "{\"name\":\"p\",\"time\":5,\"value\":\"y\"}")) ==
             "OCEL field must be string: time",
         "an int change time");
  expect(read(json_with_object("{\"name\":\"p\",\"time\":\"1970-01-01T00:00:00Z\",\"value\":\"x\"},"
                               "{\"name\":\"p\",\"time\":5,\"value\":\"y\"}",
                               ",{\"id\":\"o2\",\"type\":\"t\",\"attributes\":[{\"name\":\"p\",\"time\":"
                               "\"1970-01-01T00:00:00Z\"}]}")) == "KeyError: 'value'",
         "a later KeyError before the int change time");
  // The writers.
  expect(error_of([&] { (void)serialize_ocel_json(log, OcelJsonFormat::kStandard, false); }) ==
             "KeyError: 'ocel:timestamp'",
         "write_ocel2_json on a NaT change time");
  const auto classic = serialize_ocel_json(log, OcelJsonFormat::kClassic, false);
  expect(classic.find("{\"ocel:oid\":\"o1\",\"ocel:type\":\"t\",\"ocel:field\":\"p\"") != std::string::npos,
         "write_ocel_json leaves the NaT change time out");
  const auto transport = serialize_ocel_json(log);
  expect(transport.find("\"minera:unreadableChangeTimes\":true") != std::string::npos,
         "the transport marks a NaT change time");
  const auto back = parse_ocel_json(transport);
  expect(back.object_changes.size() == 3 && back.object_changes.at(0).timestamp.empty(),
         "the transport keeps a NaT change time");
  OcelXmlExportOptions xml;
  xml.format = OcelXmlFormat::kStandard;
  const auto written = serialize_ocel_xml(log, xml);
  const auto nat = written.find("time=\"NaT\"");
  const auto march = written.find("time=\"2024-03-01T00:00:00+00:00\"");
  const auto february = written.find("time=\"2024-02-01T00:00:00+00:00\"");
  expect(nat != std::string::npos && march != std::string::npos && february != std::string::npos && nat < march &&
             march < february,
         "write_ocel2_xml prints NaT and lists the changes in log order");
}

void json_float_columns() {
  // o2 has no relation (dropped later) and no value of n: the column is
  // float64, so o1's integer reads as a float.
  const auto log = parse_ocel_json(
      "{\"objectTypes\":[{\"name\":\"t\",\"attributes\":[{\"name\":\"n\",\"type\":\"integer\"}]}],"
      "\"eventTypes\":[{\"name\":\"a\",\"attributes\":[]}],"
      "\"objects\":[{\"id\":\"o1\",\"type\":\"t\",\"attributes\":[{\"name\":\"n\",\"time\":\"1970-01-01T00:00:00Z\","
      "\"value\":5}]},{\"id\":\"o2\",\"type\":\"t\"}],"
      "\"events\":[{\"id\":\"e1\",\"type\":\"a\",\"time\":\"2024-01-01T00:00:00Z\","
      "\"relationships\":[{\"objectId\":\"o1\",\"qualifier\":\"q\"}]}]}");
  expect(log.objects.size() == 1 && is_double(value_of(log.objects.at(0).attributes, "n"), 5.0),
         "an integer column with a missing cell in a dropped row");
}

void flatten_dates() {
  const auto log = parse_ocel_json(
      "{\"objectTypes\":[{\"name\":\"t\",\"attributes\":[{\"name\":\"d\",\"type\":\"date\"}]}],"
      "\"eventTypes\":[{\"name\":\"a\",\"attributes\":[{\"name\":\"x\",\"type\":\"time\"}]}],"
      "\"objects\":[{\"id\":\"o1\",\"type\":\"t\",\"attributes\":[{\"name\":\"d\",\"time\":\"1970-01-01T00:00:00Z\","
      "\"value\":\"2024-02-01T12:00:00+02:00\"}]}],"
      "\"events\":[{\"id\":\"e1\",\"type\":\"a\",\"time\":\"2024-01-01T00:00:00Z\",\"attributes\":[{\"name\":\"x\","
      "\"value\":\"2024-03-01T12:30:00.5Z\"}],\"relationships\":[{\"objectId\":\"o1\",\"qualifier\":\"q\"}]}]}");
  const auto flat = flatten_ocel_to_event_log(log, "t");
  expect(flat.traces.size() == 1 && flat.traces.at(0).events.size() == 1, "one trace with one event");
  if (flat.traces.size() != 1 || flat.traces.at(0).events.size() != 1) return;
  bool event_date = false, trace_date = false;
  for (const auto& attribute : flat.traces.at(0).events.at(0).attributes)
    if (attribute.key == "x")
      event_date = attribute.kind == AttributeKind::kDate && attribute.value == "2024-03-01T12:30:00.500000+00:00";
  for (const auto& attribute : flat.traces.at(0).attributes)
    if (attribute.key == "d")
      // read_ocel2_json reads an object attribute's date as its wall clock
      // in UTC (observed black-box, as before this round).
      trace_date = attribute.kind == AttributeKind::kDate && attribute.value == "2024-02-01T12:00:00+00:00";
  expect(event_date, "a declared time event attribute is a date");
  expect(trace_date, "a declared date object attribute is a date");
}

#if defined(MINERA_HAS_SQLITE)
void sqlite_float_columns() {
  const auto path = std::filesystem::temp_directory_path() / "minera_review_ej1.sqlite";
  std::filesystem::remove(path);
  sqlite3* db = nullptr;
  if (sqlite3_open(path.string().c_str(), &db) != SQLITE_OK) {
    expect(false, "open the SQLite file");
    return;
  }
  // As write_ocel2_sqlite writes a boolean b with a change of m: the change
  // row leaves b NULL.
  const char* sql =
      "CREATE TABLE \"event\" (\"ocel_id\" TEXT, \"ocel_type\" TEXT);"
      "INSERT INTO \"event\" VALUES('e1','a');"
      "CREATE TABLE \"event_A\" (\"ocel_id\" TEXT, \"ocel_time\" TIMESTAMP, \"ocel:activity\" TEXT);"
      "INSERT INTO \"event_A\" VALUES('e1','2024-01-01 00:00:00+00:00','a');"
      "CREATE TABLE \"event_map_type\" (\"ocel_type\" TEXT, \"ocel_type_map\" TEXT);"
      "INSERT INTO \"event_map_type\" VALUES('a','A');"
      "CREATE TABLE \"event_object\" (\"ocel_event_id\" TEXT, \"ocel_object_id\" TEXT, \"ocel_qualifier\" TEXT);"
      "INSERT INTO \"event_object\" VALUES('e1','o1','q');"
      "CREATE TABLE \"object\" (\"ocel_id\" TEXT, \"ocel_type\" TEXT);"
      "INSERT INTO \"object\" VALUES('o1','T');"
      "CREATE TABLE \"object_T\" (\"ocel_id\" TEXT, \"b\" INTEGER, \"m\" TEXT, \"n\" INTEGER, "
      "\"ocel_changed_field\" TEXT, \"ocel_time\" TIMESTAMP);"
      "INSERT INTO \"object_T\" VALUES('o1',1,'x',7,NULL,NULL);"
      "INSERT INTO \"object_T\" VALUES('o1',NULL,'y',8,'m','2024-02-01 00:00:00+00:00');"
      "CREATE TABLE \"object_map_type\" (\"ocel_type\" TEXT, \"ocel_type_map\" TEXT);"
      "INSERT INTO \"object_map_type\" VALUES('T','T');"
      "CREATE TABLE \"object_object\" (\"ocel_source_id\" REAL, \"ocel_target_id\" REAL, \"ocel_qualifier\" REAL);";
  char* error = nullptr;
  const auto status = sqlite3_exec(db, sql, nullptr, nullptr, &error);
  if (error != nullptr) sqlite3_free(error);
  sqlite3_close(db);
  expect(status == SQLITE_OK, "write the SQLite file");
  const auto log = import_ocel_sqlite(path);
  std::filesystem::remove(path);
  expect(log.objects.size() == 1 && is_double(value_of(log.objects.at(0).attributes, "b"), 1.0),
         "an integer column with a NULL in a change row reads as floats");
  expect(log.objects.size() == 1 && std::holds_alternative<std::int64_t>(*value_of(log.objects.at(0).attributes, "n")),
         "an integer column without NULL stays int64");
}
#endif

}  // namespace

int main() {
  xml_values();
  xml_walk_order();
  json_change_times();
  json_float_columns();
  flatten_dates();
#if defined(MINERA_HAS_SQLITE)
  sqlite_float_columns();
#endif
  if (failures != 0) {
    std::cerr << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "review EJ1 tests passed\n";
  return 0;
}
