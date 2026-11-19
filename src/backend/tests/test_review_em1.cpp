// Review EM1: pm4py 2.7.22 on round EJ's spoiled OCEL 2.0 JSON bank and
// round EK's spoiled SQLite bank (review-harness round-em: em/hc/hc.py, the
// probes em/*.py). Observed black-box:
//
// - pandas types a frame column over every row the frame was built from and
//   keeps its columns in that build's order; read_ocel2_json and
//   read_ocel2_sqlite build their frames before they drop the events and
//   objects without relations (and read_sql reads a whole type table, its
//   change rows and rows without an id included), and the writers drop rows
//   from frames typed before, so an unrelated event's text keeps a date
//   column an object column (isoformat "+00:00", TEXT, declared by the
//   owner's values), an unrelated object's text declares an integer column
//   "string", and an unrelated event's column order wins;
// - write_ocel2_xml prints a naive change time with "+00:00" (the changes
//   frame's time column is UTC);
// - read_ocel2_sqlite keeps a numeric change-time column (every object table
//   read is int64, or int64 and float64, by read_sql's dtype over all of its
//   rows) as numbers, sorted by value, without pd.to_datetime (probe em
//   ct.py); a table of NULLs or without rows keeps the column object;
// - the tuple description's lifecycle takes the first object's last event
//   with a time (groupby's last() skips the missing ones).
//
// See docs/reference-2.7.22-parity.md, Review EM1.
#include "minera/ocel.hpp"
#include "minera/ocel_csv.hpp"
#include "minera/ocel_sqlite.hpp"
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

bool contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

std::string event(const std::string& id, const std::string& attributes, const std::string& relations) {
  return "{\"id\":\"" + id + "\",\"type\":\"a\",\"time\":\"2024-01-0" + id.substr(1) +
         "T10:00:00Z\",\"attributes\":[" + attributes + "],\"relationships\":[" + relations + "]}";
}
const std::string kRelation = "{\"objectId\":\"o1\",\"qualifier\":\"q\"}";

// Case A: event e1 has no relation; its x0 "abc" keeps the time-declared
// column an object column.
std::string case_a() {
  return "{\"objectTypes\":[{\"name\":\"t\",\"attributes\":[]}],\"eventTypes\":[{\"name\":\"a\",\"attributes\":"
         "[{\"name\":\"x0\",\"type\":\"time\"}]}],\"objects\":[{\"id\":\"o1\",\"type\":\"t\",\"attributes\":[]}],"
         "\"events\":[" +
         event("e1", "{\"name\":\"x0\",\"value\":\"abc\"}", "") + "," +
         event("e2", "{\"name\":\"x0\",\"value\":\"2024-02-01T10:00:00Z\"}", kRelation) + "]}";
}

// Case B: object o2 has no relation; its p0 "1e3" (no integer) keeps the
// integer-declared column an object column.
std::string case_b() {
  return "{\"objectTypes\":[{\"name\":\"t\",\"attributes\":[{\"name\":\"p0\",\"type\":\"integer\"}]}],"
         "\"eventTypes\":[{\"name\":\"a\",\"attributes\":[]}],\"objects\":["
         "{\"id\":\"o1\",\"type\":\"t\",\"attributes\":[{\"name\":\"p0\",\"time\":\"1970-01-01T00:00:00Z\","
         "\"value\":2},{\"name\":\"p0\",\"time\":\"2024-01-05T00:00:00Z\",\"value\":3}]},"
         "{\"id\":\"o2\",\"type\":\"t\",\"attributes\":[{\"name\":\"p0\",\"time\":\"1970-01-01T00:00:00Z\","
         "\"value\":\"1e3\"}]}],\"events\":[" +
         event("e1", "", kRelation) + "]}";
}

// Case C: the unrelated event e1 lists z before y.
std::string case_c() {
  return "{\"objectTypes\":[{\"name\":\"t\",\"attributes\":[]}],\"eventTypes\":[{\"name\":\"a\",\"attributes\":"
         "[{\"name\":\"y\",\"type\":\"string\"},{\"name\":\"z\",\"type\":\"string\"}]}],"
         "\"objects\":[{\"id\":\"o1\",\"type\":\"t\",\"attributes\":[]}],\"events\":[" +
         event("e1", "{\"name\":\"z\",\"value\":\"1\"},{\"name\":\"y\",\"value\":\"2\"}", "") + "," +
         event("e2", "{\"name\":\"y\",\"value\":\"3\"},{\"name\":\"z\",\"value\":\"4\"}", kRelation) + "]}";
}

OcelXmlExportOptions xml_options(OcelXmlFormat format) {
  OcelXmlExportOptions options;
  options.format = format;
  options.allow_lossy = true;  // the OCEL 1.0 XML writer leaves the qualifiers out
  return options;
}

// The OCEL as parse_ocel_json reads it, and after a trip through the
// transport (the routes' input).
std::vector<Ocel> both(const std::string& json) {
  const auto log = parse_ocel_json(json);
  return {log, parse_ocel_json(serialize_ocel_json(log, OcelJsonFormat::kClassic, true))};
}

void unrelated_event_date_column() {
  for (const auto& log : both(case_a())) {
    const auto classic = serialize_ocel_json(log, OcelJsonFormat::kClassic, false);
    expect(contains(classic, "\"ocel:vmap\":{\"x0\":\"2024-02-01T10:00:00+00:00\"}"),
           "A: write_ocel_json prints the date of an object column as isoformat");
    const auto standard = serialize_ocel_json(log, OcelJsonFormat::kStandard, false);
    expect(contains(standard, "{\"name\":\"x0\",\"type\":\"date\"}"),
           "A: write_ocel2_json declares the owner's dates a date");
    expect(contains(standard, "{\"name\":\"x0\",\"value\":\"2024-02-01T10:00:00+00:00\"}"),
           "A: write_ocel2_json prints it as isoformat");
    const auto xml2 = serialize_ocel_xml(log, xml_options(OcelXmlFormat::kStandard));
    // (native adds the value's type, which pm4py's reader ignores)
    expect(contains(xml2, ">2024-02-01T10:00:00+00:00</attribute>"), "A: write_ocel2_xml prints it as isoformat");
    const auto xml1 = serialize_ocel_xml(log, xml_options(OcelXmlFormat::kClassic));
    expect(contains(xml1, "<string key=\"x0\" value=\"2024-02-01T10:00:00+00:00\"/>"),
           "A: write_ocel_xml writes it as a string");
    OcelCsvExportOptions csv_options;
    csv_options.allow_lossy = true;
    const auto csv = serialize_ocel_csv(log, csv_options);
    expect(contains(csv, ",a,2024-02-01 10:00:00+00:00,"), "A: write_ocel_csv writes str() of it");
  }
}

void unrelated_object_text() {
  for (const auto& log : both(case_b())) {
    const auto standard = serialize_ocel_json(log, OcelJsonFormat::kStandard, false);
    expect(contains(standard, "\"objectTypes\":[{\"name\":\"t\",\"attributes\":[{\"name\":\"p0\",\"type\":\"string\"}]}]"),
           "B: write_ocel2_json declares the object column's ints a string");
    const auto xml2 = serialize_ocel_xml(log, xml_options(OcelXmlFormat::kStandard));
    expect(contains(xml2, "<attribute name=\"p0\" type=\"string\"/>"), "B: write_ocel2_xml likewise");
    const auto xml1 = serialize_ocel_xml(log, xml_options(OcelXmlFormat::kClassic));
    expect(contains(xml1, "<string key=\"p0\" value=\"2\"/>"), "B: write_ocel_xml writes the int as a string");
  }
}

void unrelated_event_column_order() {
  for (const auto& log : both(case_c())) {
    const auto classic = serialize_ocel_json(log, OcelJsonFormat::kClassic, false);
    expect(contains(classic, "\"ocel:vmap\":{\"z\":\"4\",\"y\":\"3\"}"), "C: write_ocel_json in the frame's order");
    const auto standard = serialize_ocel_json(log, OcelJsonFormat::kStandard, false);
    expect(contains(standard, "[{\"name\":\"z\",\"type\":\"string\"},{\"name\":\"y\",\"type\":\"string\"}]"),
           "C: write_ocel2_json declares in the frame's order");
    expect(contains(standard, "[{\"name\":\"z\",\"value\":\"4\"},{\"name\":\"y\",\"value\":\"3\"}]"),
           "C: and writes in it");
    const auto xml2 = serialize_ocel_xml(log, xml_options(OcelXmlFormat::kStandard));
    const auto events = xml2.substr(xml2.find("<events>"));
    expect(events.find("<attribute name=\"z\"") < events.find("<attribute name=\"y\""),
           "C: write_ocel2_xml in the frame's order");
  }
}

// Case W: case C with a column w that only the unrelated event holds.
void unrelated_event_column() {
  auto json = case_c();
  const std::string from = "{\"name\":\"y\",\"value\":\"2\"}";
  json.replace(json.find(from), from.size(), from + ",{\"name\":\"w\",\"value\":\"9\"}");
  for (const auto& log : both(json)) {
    OcelCsvExportOptions csv_options;
    csv_options.allow_lossy = true;
    const auto csv = serialize_ocel_csv(log, csv_options);
    expect(csv.rfind("ocel:eid,ocel:timestamp,ocel:activity,z,y,w,ocel:type:t\n", 0) == 0,
           "W: write_ocel_csv keeps the column only the dropped event held");
    const auto standard = serialize_ocel_json(log, OcelJsonFormat::kStandard, false);
    expect(!contains(standard, "\"w\""), "W: write_ocel2_json declares no attribute without a value");
  }
}

void naive_change_time() {
  const auto log = parse_ocel_json(
      "{\"objectTypes\":[{\"name\":\"t\",\"attributes\":[{\"name\":\"p\",\"type\":\"integer\"}]}],"
      "\"eventTypes\":[{\"name\":\"a\",\"attributes\":[]}],\"objects\":[{\"id\":\"o1\",\"type\":\"t\","
      "\"attributes\":[{\"name\":\"p\",\"time\":\"1970-01-01T00:00:00Z\",\"value\":2},{\"name\":\"p\",\"time\":"
      "\"1970-01-01T00:00:00\",\"value\":17}]}],\"events\":[" +
      event("e1", "", kRelation) + "]}");
  const auto xml2 = serialize_ocel_xml(log, xml_options(OcelXmlFormat::kStandard));
  expect(contains(xml2, "<attribute name=\"p\" time=\"1970-01-01T00:00:00+00:00\""),
         "write_ocel2_xml prints a naive change time in UTC");
}

#if defined(MINERA_HAS_SQLITE)
void exec(sqlite3* db, const std::string& sql) {
  char* error = nullptr;
  if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &error) != SQLITE_OK) {
    const std::string message = error == nullptr ? "sqlite error" : error;
    sqlite3_free(error);
    throw std::runtime_error(message);
  }
}

// A pm4py-shaped SQLite log: objects o5, o6 of type order with an integer
// column o1; a change row of field o0 holds the text 'nan' in o1, so read_sql
// reads o1 as an object column (s53_0172).
void sqlite_change_row_cells() {
  const auto path = std::filesystem::temp_directory_path() / "minera_review_em1.sqlite";
  std::filesystem::remove(path);
  sqlite3* db = nullptr;
  if (sqlite3_open(path.string().c_str(), &db) != SQLITE_OK) {
    expect(false, "open the SQLite file");
    return;
  }
  exec(db,
       "CREATE TABLE \"event\" (\"ocel_id\" TEXT, \"ocel_type\" TEXT);"
       "INSERT INTO \"event\" VALUES('e1','c');"
       "CREATE TABLE \"object\" (\"ocel_id\" TEXT, \"ocel_type\" TEXT);"
       "INSERT INTO \"object\" VALUES('o5','order'),('o6','order'),('o7','t');"
       "CREATE TABLE \"event_map_type\" (\"ocel_type\" TEXT, \"ocel_type_map\" TEXT);"
       "INSERT INTO \"event_map_type\" VALUES('c','C');"
       "CREATE TABLE \"object_map_type\" (\"ocel_type\" TEXT, \"ocel_type_map\" TEXT);"
       "INSERT INTO \"object_map_type\" VALUES('order','Order'),('t','T');"
       "CREATE TABLE \"event_object\" (\"ocel_event_id\" TEXT, \"ocel_object_id\" TEXT, \"ocel_qualifier\" TEXT);"
       "INSERT INTO \"event_object\" VALUES('e1','o5','q'),('e1','o6','q'),('e1','o7','q');"
       "CREATE TABLE \"object_object\" (\"ocel_source_id\" TEXT, \"ocel_target_id\" TEXT, \"ocel_qualifier\" TEXT);"
       "CREATE TABLE \"event_C\" (\"ocel_id\" TEXT, \"ocel_time\" TIMESTAMP, \"ocel:activity\" TEXT);"
       "INSERT INTO \"event_C\" VALUES('e1','2024-01-03 16:13:25+00:00','c');"
       "CREATE TABLE \"object_Order\" (\"ocel_id\" TEXT, \"o0\" TEXT, \"o1\" INTEGER, \"ocel_changed_field\" TEXT,"
       " \"ocel_time\" TIMESTAMP);"
       "INSERT INTO \"object_Order\" VALUES('o5','x',1,NULL,NULL),('o6','x',0,NULL,NULL),"
       "('o5','1','nan','o0','2024-02-21 18:32:30+00:00'),('o6',NULL,1,'o1','2024-01-09 12:19:05+00:00');"
       "CREATE TABLE \"object_T\" (\"ocel_id\" TEXT);"
       "INSERT INTO \"object_T\" VALUES('o7');");
  sqlite3_close(db);
  const auto log = import_ocel_sqlite(path);
  std::filesystem::remove(path);
  for (const auto& read : {log, parse_ocel_json(serialize_ocel_json(log, OcelJsonFormat::kClassic, true))}) {
    const auto xml2 = serialize_ocel_xml(read, xml_options(OcelXmlFormat::kStandard));
    expect(contains(xml2, "<attribute name=\"o1\" type=\"string\"/>"),
           "a change row's text in another column keeps o1 an object column (declared string)");
    // The transport keeps the change's integer (its hint types o1 object).
    bool integer = false;
    for (const auto& change : read.object_changes)
      if (change.field == "o1") integer = std::holds_alternative<std::int64_t>(change.value);
    expect(integer, "the o1 change keeps its integer in an object column");
  }
}

// Change times: object_Order's base rows NULL, its change rows numbers
// (float64); object_T `t_times` (its base rows' times, NULL or text).
Ocel numeric_change_log(const std::string& order_times, const std::string& t_rows) {
  const auto path = std::filesystem::temp_directory_path() / "minera_review_em1_ct.sqlite";
  std::filesystem::remove(path);
  sqlite3* db = nullptr;
  if (sqlite3_open(path.string().c_str(), &db) != SQLITE_OK) throw std::runtime_error("open the SQLite file");
  exec(db,
       "CREATE TABLE \"event\" (\"ocel_id\" TEXT, \"ocel_type\" TEXT);"
       "INSERT INTO \"event\" VALUES('e1','c');"
       "CREATE TABLE \"object\" (\"ocel_id\" TEXT, \"ocel_type\" TEXT);"
       "INSERT INTO \"object\" VALUES('o5','order'),('o6','order'),('o7','t');"
       "CREATE TABLE \"event_map_type\" (\"ocel_type\" TEXT, \"ocel_type_map\" TEXT);"
       "INSERT INTO \"event_map_type\" VALUES('c','C');"
       "CREATE TABLE \"object_map_type\" (\"ocel_type\" TEXT, \"ocel_type_map\" TEXT);"
       "INSERT INTO \"object_map_type\" VALUES('order','Order'),('t','T');"
       "CREATE TABLE \"event_object\" (\"ocel_event_id\" TEXT, \"ocel_object_id\" TEXT, \"ocel_qualifier\" TEXT);"
       "INSERT INTO \"event_object\" VALUES('e1','o5','q'),('e1','o6','q'),('e1','o7','q');"
       "CREATE TABLE \"object_object\" (\"ocel_source_id\" TEXT, \"ocel_target_id\" TEXT, \"ocel_qualifier\" TEXT);"
       "CREATE TABLE \"event_C\" (\"ocel_id\" TEXT, \"ocel_time\" TIMESTAMP, \"ocel:activity\" TEXT);"
       "INSERT INTO \"event_C\" VALUES('e1','2024-01-03 16:13:25+00:00','c');"
       "CREATE TABLE \"object_Order\" (\"ocel_id\" TEXT, \"o1\" TEXT, \"ocel_changed_field\" TEXT,"
       " \"ocel_time\" TIMESTAMP);"
       "INSERT INTO \"object_Order\" VALUES('o5','a',NULL,NULL),('o6','a',NULL,NULL)," + order_times + ";"
       "CREATE TABLE \"object_T\" (\"ocel_id\" TEXT, \"t1\" TEXT, \"ocel_changed_field\" TEXT, \"ocel_time\" TIMESTAMP);"
       "INSERT INTO \"object_T\" VALUES" + t_rows + ";");
  sqlite3_close(db);
  auto log = import_ocel_sqlite(path);
  std::filesystem::remove(path);
  return log;
}

void sqlite_numeric_change_times() {
  // float64 tables: the numbers stay, sorted by value, NaN last.
  auto log = numeric_change_log("('o5','b','o1',1.5),('o6','c','o1',-2),('o6','d','o1',NULL)",
                                "('o7','x',NULL,3),('o7','y','t1',0.5)");
  expect(log.object_changes.size() == 4, "four changes");
  if (log.object_changes.size() == 4) {
    expect(log.object_changes[0].timestamp == "-2.0" && log.object_changes[0].raw_time == OcelRawTime::kFloat,
           "a float64 change column keeps -2 as the float -2.0, first");
    expect(log.object_changes[1].timestamp == "0.5" && log.object_changes[2].timestamp == "1.5",
           "then 0.5 and 1.5 by value");
    expect(log.object_changes[3].raw_time == OcelRawTime::kNaN, "NULL is NaN, last");
  }
  // int64 tables: ints.
  log = numeric_change_log("('o5','b','o1',7),('o6','c','o1',5)", "('o7','x',NULL,3),('o7','y','t1',4)");
  // object_Order's base rows are NULL: float64, so the column is float64.
  expect(!log.object_changes.empty() && log.object_changes.front().timestamp == "4.0",
         "an int64 table next to a float64 one gives floats");
  // A table of NULL times (object) makes the column object: pd.to_datetime
  // reads the numbers as nanoseconds since the epoch.
  log = numeric_change_log("('o5','b','o1',1.5),('o6','c','o1',3)", "('o7','x',NULL,NULL)");
  expect(log.object_changes.size() == 2 && log.object_changes.front().raw_time == OcelRawTime::kNone &&
             log.object_changes.front().timestamp.rfind("1970-01-01", 0) == 0,
         "an object table among them converts the numbers to datetimes");
}
#endif

// The first object's last event with a time raises in the description.
void description_last_timed_event() {
  Ocel log;
  log.events = {{"e1", "a", "2024-01-01", {}, OcelRawTime::kStr},
                {"e2", "a", "", {}, OcelRawTime::kNaN},
                {"e3", "a", "5", {}, OcelRawTime::kInt}};
  log.objects = {{"o1", "t", {}}};
  log.relations = {{"e3", "o1", ""}, {"e1", "o1", ""}, {"e2", "o1", ""}};
  log.event_type_attributes["a"];
  log.object_type_attributes["t"];
  try {
    (void)describe_ocel_tuples(log, OcelDescriptionOptions{});
    expect(false, "the description raises on a raw time column");
  } catch (const std::invalid_argument& error) {
    expect(std::string(error.what()) == "AttributeError: 'str' object has no attribute 'timestamp'",
           std::string("the last event with a time is a str: ") + error.what());
  }
}

}  // namespace

int main() {
  unrelated_event_date_column();
  unrelated_object_text();
  unrelated_event_column_order();
  unrelated_event_column();
  naive_change_time();
  description_last_timed_event();
#if defined(MINERA_HAS_SQLITE)
  sqlite_change_row_cells();
  sqlite_numeric_change_times();
#endif
  if (failures != 0) {
    std::cerr << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "review EM1 tests passed\n";
  return 0;
}
