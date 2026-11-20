// Review EZ1: a date-like text next to a date of the same key and type in an
// OCEL 1.0 XML file (review-harness round-ez: t/h1.xmlocel, t/h1ref.py).
// Observed black-box on pm4py 2.7.22:
//
// - read_ocel_xml types each element: a <date> becomes a datetime, a
//   <string> under the same key and type stays its text, so the column is
//   object (o1's datetime next to o2's and o3's texts).
// - Every writer keeps the texts as written and prints the datetime as its
//   isoformat ("2024-02-01T10:00:00+00:00"): write_ocel_json and
//   write_ocel2_json declare the attribute "string", write_ocel_csv prints
//   the texts, write_ocel2_sqlite declares a TEXT column, write_ocel_xml
//   writes <string> elements.
// - filter_ocel_object_attribute and filter_ocel_event_attribute match a
//   datetime only to the datetime and a str only to the text.
// - ocel_flattening keeps the text as a str (case:s).
//
// See docs/reference-2.7.22-parity.md, Review EZ1.
#include "minera/ocel.hpp"
#include "minera/ocel_csv.hpp"
#include "minera/ocel_filter.hpp"
#include "minera/ocel_sqlite.hpp"
#include "minera/ocel_transform.hpp"
#include "minera/ocel_xml.hpp"
#include "minera/timestamp.hpp"

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

const char* const kXml = R"(<?xml version="1.0" encoding="UTF-8"?>
<log>
  <events>
    <event><string key="id" value="e1"/><string key="activity" value="a"/><date key="timestamp" value="2024-01-01T10:00:00Z"/><list key="omap"><string key="object-id" value="o1"/><string key="object-id" value="o2"/></list><list key="vmap"><date key="d" value="2024-03-01T00:00:00Z"/></list></event>
    <event><string key="id" value="e2"/><string key="activity" value="a"/><date key="timestamp" value="2024-01-02T10:00:00Z"/><list key="omap"><string key="object-id" value="o2"/><string key="object-id" value="o3"/></list><list key="vmap"><string key="d" value="2024-03-02 00:00:00"/></list></event>
  </events>
  <objects>
    <object><string key="id" value="o1"/><string key="type" value="t"/><list key="ovmap"><date key="s" value="2024-02-01T10:00:00Z"/></list></object>
    <object><string key="id" value="o2"/><string key="type" value="t"/><list key="ovmap"><string key="s" value="2024-02-01T10:00:00Z"/></list></object>
    <object><string key="id" value="o3"/><string key="type" value="t"/><list key="ovmap"><string key="s" value="2024-02-02 00:00:00"/></list></object>
  </objects>
</log>
)";

const OcelAttribute* attribute_of(const std::vector<OcelAttribute>& attributes, const std::string& key) {
  for (const auto& attribute : attributes)
    if (attribute.key == key) return &attribute;
  return nullptr;
}

const OcelObject* object_of(const Ocel& log, const std::string& id) {
  for (const auto& object : log.objects)
    if (object.id == id) return &object;
  return nullptr;
}

std::vector<std::string> object_ids(const Ocel& log) {
  std::vector<std::string> ids;
  for (const auto& object : log.objects) ids.push_back(object.id);
  std::sort(ids.begin(), ids.end());
  return ids;
}

std::vector<std::string> event_ids(const Ocel& log) {
  std::vector<std::string> ids;
  for (const auto& event : log.events) ids.push_back(event.id);
  std::sort(ids.begin(), ids.end());
  return ids;
}

void check_marks(const Ocel& log, const std::string& where) {
  const auto* o1 = object_of(log, "o1");
  const auto* o2 = object_of(log, "o2");
  const auto* o3 = object_of(log, "o3");
  expect(o1 && o2 && o3, where + ": three objects");
  if (!o1 || !o2 || !o3) return;
  const auto* s1 = attribute_of(o1->attributes, "s");
  const auto* s2 = attribute_of(o2->attributes, "s");
  const auto* s3 = attribute_of(o3->attributes, "s");
  expect(s1 && !s1->text, where + ": the <date> is no kept text");
  expect(s2 && s2->text && std::get<std::string>(s2->value) == "2024-02-01T10:00:00Z",
         where + ": o2's <string> is a kept text as written");
  expect(s3 && s3->text && std::get<std::string>(s3->value) == "2024-02-02 00:00:00",
         where + ": o3's <string> is a kept text as written");
  const OcelAttribute* d2 = nullptr;
  for (const auto& event : log.events)
    if (event.id == "e2") d2 = attribute_of(event.attributes, "d");
  expect(d2 && d2->text && std::get<std::string>(d2->value) == "2024-03-02 00:00:00",
         where + ": e2's <string> is a kept text as written");
}

void check_downloads(const Ocel& log, const std::string& where) {
  const auto classic = serialize_ocel_json(log, OcelJsonFormat::kClassic, false);
  expect(contains(classic, R"("o2":{"ocel:type":"t","ocel:ovmap":{"s":"2024-02-01T10:00:00Z"})"),
         where + ": classic JSON keeps o2's text: " + classic);
  expect(contains(classic, R"("s":"2024-02-01T10:00:00+00:00")"), where + ": classic JSON prints o1's datetime");
  expect(contains(classic, R"("s":"2024-02-02 00:00:00")"), where + ": classic JSON keeps o3's text");
  expect(contains(classic, R"("d":"2024-03-02 00:00:00")"), where + ": classic JSON keeps e2's text");
  expect(contains(classic, R"("ocel:objectTypes":{"t":{"s":"string"}})"),
         where + ": classic JSON declares the object column string");
  expect(contains(classic, R"("ocel:eventTypes":{"a":{"d":"string"}})"),
         where + ": classic JSON declares the event column string");
  expect(!contains(classic, "minera:textValues"), where + ": a download carries no marker");

  const auto standard = serialize_ocel_json(log, OcelJsonFormat::kStandard, false);
  expect(contains(standard, R"({"name":"s","type":"string"})"), where + ": OCEL 2.0 JSON declares s string: " + standard);
  expect(contains(standard, R"("value":"2024-02-01T10:00:00Z")"), where + ": OCEL 2.0 JSON keeps o2's text");
  expect(contains(standard, R"("value":"2024-03-02 00:00:00")"), where + ": OCEL 2.0 JSON keeps e2's text");

  const auto objects_csv = serialize_ocel_objects_csv(log);
  expect(contains(objects_csv, "o2,t,2024-02-01T10:00:00Z\n"), where + ": CSV keeps o2's text: " + objects_csv);
  expect(contains(objects_csv, "o3,t,2024-02-02 00:00:00\n"), where + ": CSV keeps o3's text");
  expect(contains(objects_csv, "o1,t,2024-02-01 10:00:00+00:00\n"), where + ": CSV prints o1's datetime");
  OcelCsvExportOptions pair;
  pair.allow_lossy = true;  // the object table goes separately (write_ocel_csv's pair)
  const auto events_csv = serialize_ocel_csv(log, pair);
  expect(contains(events_csv, "e2,") && contains(events_csv, ",2024-03-02 00:00:00,") &&
             contains(events_csv, ",2024-03-01 00:00:00+00:00,"),
         where + ": CSV keeps e2's text and prints e1's datetime: " + events_csv);

  const auto xml = serialize_ocel_xml(log);
  expect(contains(xml, R"(<string key="s" value="2024-02-01T10:00:00Z"/>)"), where + ": OCEL 1.0 XML keeps o2's text: " + xml);
  expect(contains(xml, R"(<string key="s" value="2024-02-01T10:00:00+00:00"/>)"),
         where + ": OCEL 1.0 XML prints o1's datetime as a string");
  expect(contains(xml, R"(<string key="d" value="2024-03-02 00:00:00"/>)"), where + ": OCEL 1.0 XML keeps e2's text");
  expect(!contains(xml, "<date key=\"s\""), where + ": no <date> for the object column");

#if defined(MINERA_HAS_SQLITE)
  const auto path = std::filesystem::temp_directory_path() / "minera_review_ez1.sqlite";
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
  // The type tables, found through the map tables (pm4py names them
  // object_t and event_a).
  const auto object_map = query("select ocel_type_map from object_map_type where ocel_type = 't'");
  const auto event_map = query("select ocel_type_map from event_map_type where ocel_type = 'a'");
  expect(object_map.size() == 1 && event_map.size() == 1, where + ": one type table each");
  if (object_map.size() != 1 || event_map.size() != 1) {
    sqlite3_close(db);
    return;
  }
  const auto object_table = "\"object_" + object_map[0] + "\"";
  const auto event_table = "\"event_" + event_map[0] + "\"";
  const auto declared = query("select name, type from pragma_table_info('object_" + object_map[0] + "')");
  expect(std::find(declared.begin(), declared.end(), "s|TEXT") != declared.end(), where + ": a TEXT column s");
  const auto values = query("select ocel_id, s from " + object_table + " order by ocel_id");
  expect(values == std::vector<std::string>{"o1|2024-02-01T10:00:00+00:00", "o2|2024-02-01T10:00:00Z",
                                            "o3|2024-02-02 00:00:00"},
         where + ": SQLite keeps the texts and prints the datetime");
  const auto event_values = query("select ocel_id, d from " + event_table + " order by ocel_id");
  expect(event_values == std::vector<std::string>{"e1|2024-03-01T00:00:00+00:00", "e2|2024-03-02 00:00:00"},
         where + ": SQLite keeps e2's text");
  sqlite3_close(db);
  std::filesystem::remove(path);
#endif
}

void test_reader_and_writers() {
  const auto log = parse_ocel_xml(kXml);
  check_marks(log, "read_ocel_xml");
  check_downloads(log, "read_ocel_xml");
}

void test_transports() {
  const auto log = parse_ocel_xml(kXml);
  for (const auto format : {OcelJsonFormat::kClassic, OcelJsonFormat::kStandard}) {
    const std::string name = format == OcelJsonFormat::kClassic ? "classic transport" : "OCEL 2.0 transport";
    const auto json = serialize_ocel_json(log, format, true);
    expect(contains(json, R"("minera:textValues":{"events":{"e2":["d"]},"objects":{"o2":["s"],"o3":["s"]}})"),
           name + " carries the marks: " + json);
    const auto read = parse_ocel_json(json);
    check_marks(read, name);
    check_downloads(read, name);
  }
  // A log without kept texts carries no marker.
  auto plain = parse_ocel_xml(kXml);
  for (auto& object : plain.objects)
    for (auto& attribute : object.attributes) attribute.text = false;
  for (auto& event : plain.events)
    for (auto& attribute : event.attributes) attribute.text = false;
  expect(!contains(serialize_ocel_json(plain, OcelJsonFormat::kClassic, true), "minera:textValues"),
         "no marker without kept texts");
  // A malformed marker is refused.
  auto bad = serialize_ocel_json(log, OcelJsonFormat::kClassic, true);
  const auto at = bad.find(R"("o2":["s"])");
  if (at != std::string::npos) bad.replace(at, 10, R"("o2":"s")");
  bool refused = false;
  try {
    (void)parse_ocel_json(bad);
  } catch (const std::invalid_argument& error) {
    refused = contains(error.what(), "minera:textValues");
  }
  expect(refused, "a malformed marker is refused");
}

void test_filters() {
  const auto log = parse_ocel_xml(kXml);
  const auto datetime = parse_timestamp_exact("2024-02-01T10:00:00+00:00");
  expect(datetime.has_value(), "parse the datetime bound");
  const auto by_datetime = filter_ocel_object_attribute_exact(log, "s", {*datetime});
  expect(object_ids(by_datetime) == std::vector<std::string>{"o1"}, "a datetime matches only the datetime");
  const auto by_text =
      filter_ocel_object_attribute_exact(log, "s", {OcelObjectAttributeFilterValue{std::string("2024-02-01T10:00:00Z")}});
  expect(object_ids(by_text) == std::vector<std::string>{"o2"}, "a str matches only the text");
  const auto event_text =
      filter_ocel_event_attribute_exact(log, "d", {OcelEventAttributeFilterValue{std::string("2024-03-02 00:00:00")}});
  expect(event_ids(event_text) == std::vector<std::string>{"e2"}, "an event str matches only the text");
  const auto event_datetime = parse_timestamp_exact("2024-03-01T00:00:00+00:00");
  const auto by_event_datetime = filter_ocel_event_attribute_exact(log, "d", {*event_datetime});
  expect(event_ids(by_event_datetime) == std::vector<std::string>{"e1"}, "an event datetime matches only e1");
  // Filters keep the marks.
  check_marks(filter_ocel_object_types(log, {"t"}), "an object type filter");
  check_downloads(filter_ocel_object_types(log, {"t"}), "an object type filter");
}

void test_flattening() {
  const auto log = parse_ocel_xml(kXml);
  const auto flat = flatten_ocel_to_event_log(log, "t");
  bool text_seen = false, date_seen = false;
  for (const auto& trace : flat.traces)
    for (const auto& attribute : trace.attributes) {
      if (attribute.key != "s") continue;
      if (trace.case_id == "o2")
        text_seen = attribute.kind == AttributeKind::kString && attribute.value == "2024-02-01T10:00:00Z";
      if (trace.case_id == "o1") date_seen = attribute.kind == AttributeKind::kDate;
    }
  expect(text_seen, "the flattening keeps o2's text as a string");
  expect(date_seen, "the flattening keeps o1's datetime as a date");
}

}  // namespace

int main() {
  try {
    test_reader_and_writers();
    test_transports();
    test_filters();
    test_flattening();
  } catch (const std::exception& error) {
    std::cerr << "FAILED with exception: " << error.what() << '\n';
    return 1;
  }
  if (failures != 0) {
    std::cerr << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "review EZ1 tests passed\n";
  return 0;
}
