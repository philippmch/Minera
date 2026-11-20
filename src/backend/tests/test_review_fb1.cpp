// Review FB1: "ocel:"-prefixed attributes in the exact filters' input, the
// exact window on an attribute column, CSV columns of naive datetimes
// (review-harness round-fb: c3/c3gen.py banks c63/c65, o2/o2tgen.py bank
// o71, q/xf.py, t1/*.json). Observed black-box on pm4py 2.7.22 (pandas
// 3.0.5):
//
// - FB1-1: read_ocel_json keeps an "ocel:"-prefixed vmap or ovmap key (but
//   the frame's own columns) as an attribute column, so an exact filter's
//   result keeps it in every download and an exact filter on it selects by
//   its values; the literal reading of the exact filters dropped it (the
//   downloads lost it, the filters on it raised KeyError).
// - FB1-2: filter_ocel_events_timestamp with timestamp_key compares the
//   events frame's column with aware datetimes: an int64, float64 or bool
//   column raises, a naive datetime64 one raises for its dtype, and an
//   object column raises at its first text, number, boolean or naive
//   datetime (missing cells skipped); a column of nulls alone is float64
//   when a row lacks the key. Native compared the timestamp texts and
//   skipped the rest.
// - FB1-3: write_ocel_csv prints a column of naive datetimes alone (an OCEL
//   2.0 JSON time attribute dateutil reads as naive) in one format: the date
//   alone when every value is a midnight, else one fraction width.
// - FB1-4: write_ocel2_json, write_ocel2_xml and write_ocel_json declare a
//   type's attributes with a static value first (objects frame order), then
//   those it holds only by changes (changes frame order); native kept the
//   objects frame's order for every column it holds.
// - FB1-5: write_ocel2_sqlite's type table concatenates the objects frame's
//   naive datetime64 column (texts dateutil reads, "02/03/2024") with the
//   changes frame's aware one as an object column: TEXT, isoformats; native
//   declared TIMESTAMP.
//
// See docs/reference-2.7.22-parity.md, Review FB1.
#include "minera/ocel.hpp"
#include "minera/ocel_csv.hpp"
#include "minera/ocel_filter.hpp"
#include "minera/ocel_sqlite.hpp"
#include "minera/ocel_xml.hpp"

#include <algorithm>
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

bool contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

std::string error_of(const std::function<void()>& run) {
  try {
    run();
  } catch (const std::exception& error) {
    return error.what();
  }
  return {};
}

// A route's input: the transport of `log`, read as the exact filter route
// reads it (literal names).
Ocel exact_input(const Ocel& log) { return parse_ocel_json_literal_names(serialize_ocel_json(log)); }

std::vector<std::string> event_ids(const Ocel& log) {
  std::vector<std::string> ids;
  for (const auto& event : log.events) ids.push_back(event.id);
  std::sort(ids.begin(), ids.end());
  return ids;
}

std::vector<std::string> object_ids(const Ocel& log) {
  std::vector<std::string> ids;
  for (const auto& object : log.objects) ids.push_back(object.id);
  std::sort(ids.begin(), ids.end());
  return ids;
}

// The CSV downloads as the route writes them (ocel_csv_allow_lossy=true).
OcelCsvExportOptions lossy() {
  OcelCsvExportOptions options;
  options.allow_lossy = true;
  return options;
}
std::string events_csv(const Ocel& log) { return serialize_ocel_csv(log, lossy()); }
std::string objects_csv(const Ocel& log) { return serialize_ocel_objects_csv(log, lossy()); }

bool has_attribute(const std::vector<OcelAttribute>& attributes, const std::string& key) {
  return std::any_of(attributes.begin(), attributes.end(),
                     [&](const OcelAttribute& attribute) { return attribute.key == key; });
}

const char* const kXmlHead =
    "<?xml version='1.0' encoding='UTF-8'?>\n<log>\n"
    "<global scope=\"event\"><string key=\"activity\" value=\"__INVALID__\"/></global>\n"
    "<global scope=\"object\"><string key=\"type\" value=\"__INVALID__\"/></global>\n"
    "<global scope=\"log\"><list key=\"attribute-names\"/><list key=\"object-types\"/>"
    "<string key=\"version\" value=\"1.0\"/><string key=\"ordering\" value=\"timestamp\"/></global>\n";

// c63_0000-like: an event key "ocel:d", a date key "ocel:t", an object key
// "ocel:w".
const std::string kPrefixedXml =
    std::string(kXmlHead) +
    "<events>"
    "<event><string key=\"id\" value=\"e0\"/><string key=\"activity\" value=\"a\"/>"
    "<date key=\"timestamp\" value=\"2024-01-15T11:30:50+00:00\"/><list key=\"omap\">"
    "<string key=\"object-id\" value=\"o0\"/></list><list key=\"vmap\"><string key=\"ocel:d\" value=\"x\"/>"
    "<date key=\"ocel:t\" value=\"2024-02-02T10:00:00Z\"/><string key=\"v\" value=\"p\"/></list></event>"
    "<event><string key=\"id\" value=\"e1\"/><string key=\"activity\" value=\"a\"/>"
    "<date key=\"timestamp\" value=\"2024-01-16T11:30:50+00:00\"/><list key=\"omap\">"
    "<string key=\"object-id\" value=\"o1\"/></list><list key=\"vmap\"><string key=\"ocel:d\" value=\"y\"/>"
    "<date key=\"ocel:t\" value=\"2024-02-20T10:00:00Z\"/><string key=\"v\" value=\"p\"/></list></event>"
    "</events><objects>"
    "<object><string key=\"id\" value=\"o0\"/><string key=\"type\" value=\"t\"/><list key=\"ovmap\">"
    "<string key=\"ocel:w\" value=\"k\"/></list></object>"
    "<object><string key=\"id\" value=\"o1\"/><string key=\"type\" value=\"t\"/><list key=\"ovmap\">"
    "<string key=\"ocel:w\" value=\"m\"/></list></object>"
    "</objects></log>\n";

void exact_filters_keep_prefixed_attributes() {
  const auto input = exact_input(parse_ocel_xml(kPrefixedXml));
  expect(!input.events.empty() && has_attribute(input.events.front().attributes, "ocel:d") &&
             has_attribute(input.events.front().attributes, "ocel:t"),
         "FB1-1: the exact filters' input keeps the events' ocel:d and ocel:t");
  expect(!input.objects.empty() && has_attribute(input.objects.front().attributes, "ocel:w"),
         "FB1-1: the exact filters' input keeps the objects' ocel:w");
  const auto kept = filter_ocel_event_attribute_exact(input, "v", {std::string("p")});
  const auto csv = events_csv(kept);
  const auto objects = objects_csv(kept);
  expect(contains(csv.substr(0, csv.find('\n')), "ocel:d") && contains(csv, ",x,"),
         "FB1-1: write_ocel_csv of an exact filter result prints ocel:d: " + csv);
  expect(contains(objects, "ocel:w") && contains(objects, "o0,t,k"),
         "FB1-1: the objects CSV prints ocel:w: " + objects);
  expect(event_ids(filter_ocel_event_attribute_exact(input, "ocel:d", {std::string("y")})) ==
             std::vector<std::string>{"e1"},
         "FB1-1: event_attribute_exact on ocel:d keeps e1");
  expect(object_ids(filter_ocel_object_attribute_exact(input, "ocel:w", {std::string("k")})) ==
             std::vector<std::string>{"o0"},
         "FB1-1: object_attribute_exact on ocel:w keeps o0");
  const auto window = filter_ocel_events_timestamp_exact(input, std::string("2024-02-01 00:00:00"),
                                                         std::string("2024-02-10 00:00:00"), "ocel:t", true);
  expect(event_ids(window) == std::vector<std::string>{"e0"}, "FB1-1: the exact window on ocel:t keeps e0");
  // The frame's own columns stay out of the attributes.
  const auto literal = parse_ocel_json_literal_names(
      R"({"ocel:global-log":{},"ocel:events":{"e1":{"ocel:activity":"a","ocel:timestamp":"2024-01-01T00:00:00+00:00",)"
      R"("ocel:omap":["o1"],"ocel:vmap":{"ocel:activity":"zz","ocel:eid":"q","ocel:timestamp":"x","ocel:x":1}}},)"
      R"("ocel:objects":{"o1":{"ocel:type":"t","ocel:ovmap":{"ocel:type":"u","ocel:oid":"w","ocel:y":2}}}})");
  expect(literal.events.size() == 1 && literal.events[0].attributes.size() == 1 &&
             literal.events[0].attributes[0].key == "ocel:x" && literal.objects.size() == 1 &&
             literal.objects[0].attributes.size() == 1 && literal.objects[0].attributes[0].key == "ocel:y",
         "FB1-1: the literal reading keeps ocel:x and ocel:y alone");
}

// An OCEL 2.0 JSON document whose event type declares "d" and whose object
// type declares "z" a time (t1/mk.py, mk2.py); `d` and `z` are JSON values
// ("" for no attribute).
std::string time_document(const std::vector<std::string>& d, const std::vector<std::string>& z) {
  std::string out =
      R"({"objectTypes":[{"name":"t","attributes":[{"name":"z","type":"time"}]}],)"
      R"("eventTypes":[{"name":"a","attributes":[{"name":"d","type":"time"}]}],"objects":[)";
  for (std::size_t index = 0; index < z.size(); ++index) {
    if (index) out += ",";
    out += R"({"id":"o)" + std::to_string(index) + R"(","type":"t","attributes":[)";
    if (!z[index].empty()) out += R"({"name":"z","time":"1970-01-01T00:00:00Z","value":)" + z[index] + "}";
    out += "]}";
  }
  out += R"(],"events":[)";
  for (std::size_t index = 0; index < d.size(); ++index) {
    if (index) out += ",";
    out += R"({"id":"e)" + std::to_string(index) + R"(","type":"a","time":"2024-01-0)" + std::to_string(index + 1) +
           R"(T10:00:00Z","attributes":[)";
    if (!d[index].empty()) out += R"({"name":"d","value":)" + d[index] + "}";
    out += R"(],"relationships":[{"objectId":"o0","qualifier":"q"}]})";
  }
  return out + "]}";
}

void window_on_attribute_columns() {
  const std::string refuse = "TypeError: '>=' not supported between instances of '";
  const std::string naive = "TypeError: can't compare offset-naive and offset-aware datetimes";
  struct Case {
    std::string name;
    std::vector<std::string> d;
    std::string error;
    std::vector<std::string> kept;
  };
  const std::vector<Case> cases{
      {"w3", {R"("2024-02-02T10:00:00Z")", R"("hello")", R"("2024-02-03T10:00:00Z")"}, refuse + "str' and 'datetime.datetime'", {}},
      {"w4", {R"("2024-02-02T10:00:00Z")", "5", ""}, refuse + "int' and 'datetime.datetime'", {}},
      {"w5", {"5", "", "7"}, refuse + "float' and 'datetime.datetime'", {}},
      {"w6", {"5", "6", "7"}, refuse + "int' and 'datetime.datetime'", {}},
      {"w7", {R"("2024-02-02T10:00:00Z")", "", R"("2024-02-20T10:00:00Z")"}, "", {"e0"}},
      {"w9", {"", "1.5", ""}, refuse + "float' and 'datetime.datetime'", {}},
      {"w10", {"true", ""}, refuse + "bool' and 'datetime.datetime'", {}},
      {"n1", {R"("02/03/2024")", "", R"("Feb 4 2024")"}, "TypeError: Invalid comparison between dtype=datetime64[us] and datetime", {}},
      {"n3", {R"("02/03/2024")", R"("2024-02-05T00:00:00Z")"}, naive, {}},
      {"n4", {R"("02/03/2024")", R"("hello")"}, naive, {}},
      // A column of nulls alone: float64 when a row lacks the key, object
      // (compared elementwise, nothing kept) when every row holds a null.
      {"w11", {"null", "", ""}, refuse + "float' and 'datetime.datetime'", {}},
      {"w12", {"null", "null"}, "", {}},
      {"w13", {"", "null", "null"}, refuse + "float' and 'datetime.datetime'", {}},
      {"w14", {"null"}, "", {}}};
  for (const auto& item : cases) {
    const auto input = exact_input(parse_ocel_json(time_document(item.d, {R"("2024-02-02T00:00:00Z")"})));
    Ocel result;
    const auto error = error_of([&] {
      result = filter_ocel_events_timestamp_exact(input, std::string("2024-02-01 00:00:00"),
                                                  std::string("2024-02-10 00:00:00"), "d", true);
    });
    expect(error == item.error, "FB1-2: the window on d, " + item.name + ": " + error);
    if (item.error.empty())
      expect(event_ids(result) == item.kept, "FB1-2: the window on d keeps " + item.name + "'s events");
  }
  // The typed module contract (reference_frame false) keeps the chosen
  // column a datetime column: w3's timestamp texts compare, "hello" is left out.
  const auto w3 = exact_input(parse_ocel_json(time_document(
      {R"("2024-02-02T10:00:00Z")", R"("hello")", R"("2024-02-03T10:00:00Z")"}, {R"("2024-02-02T00:00:00Z")"})));
  Ocel typed;
  const auto typed_error = error_of([&] {
    typed = filter_ocel_events_timestamp_exact(w3, std::string("2024-02-01 00:00:00"),
                                               std::string("2024-02-10 00:00:00"), "d");
  });
  expect(typed_error.empty() && event_ids(typed) == std::vector<std::string>{"e0", "e2"},
         "FB1-2: the module's typed window keeps e0 and e2: " + typed_error);
}

void csv_naive_date_columns() {
  const auto n1 = parse_ocel_json(time_document({R"("02/03/2024")", "", R"("Feb 4 2024")"}, {R"("2024/02/02")"}));
  const auto n1_csv = events_csv(n1);
  const auto n1_objects = objects_csv(n1);
  expect(contains(n1_csv, ",a,2024-02-03,") && contains(n1_csv, ",a,2024-02-04,") && contains(n1_csv, ",a,,"),
         "FB1-3: a column of naive midnights prints dates: " + n1_csv);
  expect(contains(n1_objects, "o0,t,2024-02-02\n"), "FB1-3: the objects CSV too: " + n1_objects);
  const auto n2 = parse_ocel_json(time_document({R"("02/03/2024")", R"("Feb 4 2024 10:30")"}, {R"("2024/02/02")", ""}));
  const auto n2_csv = events_csv(n2);
  expect(contains(n2_csv, ",a,2024-02-03 00:00:00,") && contains(n2_csv, ",a,2024-02-04 10:30:00,"),
         "FB1-3: a column with a time prints the full format: " + n2_csv);
  const auto n4 = parse_ocel_json(time_document({R"("02/03/2024")", R"("hello")"}, {R"("2024/02/02 10:00:00.5")"}));
  const auto n4_csv = events_csv(n4);
  const auto n4_objects = objects_csv(n4);
  expect(contains(n4_objects, "o0,t,2024-02-02 10:00:00.500\n"),
         "FB1-3: a naive column prints its finest fraction width: " + n4_objects);
  expect(contains(n4_csv, ",a,2024-02-03 00:00:00,") && contains(n4_csv, ",a,hello,"),
         "FB1-3: an object column prints each value as before: " + n4_csv);
  // An aware column keeps its offset text.
  const auto w1 = parse_ocel_json(time_document({R"("2024-02-02T00:00:00")", "", R"("2024-02-03T00:00:00")"}, {R"("2024-02-02T00:00:00")"}));
  const auto w1_csv = events_csv(w1);
  expect(contains(w1_csv, ",a,2024-02-02 00:00:00+00:00,"), "FB1-3: an aware column keeps its offset: " + w1_csv);
}

// o72's o2t72_0006.sqlite as the SQLite import's transport carries it: the
// objects frame's columns z, s; o0 holds s and changes z, o1 holds both.
const char* const kSqliteTransport =
    R"({"ocel:events": {"e0": {"ocel:activity": "a", "ocel:timestamp": "2024-03-16T21:28:45Z", "ocel:omap": ["o1"], ")"
    R"(ocel:typedOmap": [{"ocel:oid": "o1", "ocel:qualifier": "q"}], "ocel:vmap": {"d": "2024-02-04T01:02:03.5Z", "k")"
    R"(: "q"}}, "e3": {"ocel:activity": "a", "ocel:timestamp": "2024-03-22T09:56:44Z", "ocel:omap": ["o1", "o0"], "oc)"
    R"(el:typedOmap": [{"ocel:oid": "o1", "ocel:qualifier": "q"}, {"ocel:oid": "o0", "ocel:qualifier": "q"}], "ocel:v)"
    R"(map": {}}, "e4": {"ocel:activity": "a", "ocel:timestamp": "2024-03-22T11:27:09Z", "ocel:omap": ["o0"], "ocel:t)"
    R"(ypedOmap": [{"ocel:oid": "o0", "ocel:qualifier": "q"}], "ocel:vmap": {"d": "2024-02-01T10:00:00Z"}}}, "ocel:ev)"
    R"(entTypes": {"a": {"d": "string", "k": "string"}}, "ocel:objectTypes": {"t": {"s": "string", "z": "string"}}, ")"
    R"(ocel:objects": {"o0": {"ocel:type": "t", "ocel:ovmap": {"s": "y"}, "ocel:o2o": []}, "o1": {"ocel:type": "t", ")"
    R"(ocel:ovmap": {"z": "", "s": "2024-02-01T10:00:00Z"}, "ocel:o2o": []}}, "ocel:objectChanges": [{"ocel:oid": "o0)"
    R"(", "ocel:type": "t", "ocel:timestamp": "2024-02-06T09:51:25+00:00", "ocel:field": "z", "z": 1.5, "@@cumcount":)"
    R"( 1}], "ocel:global-log": {}, "ocel:global-event": {"ocel:activity": "__INVALID__"}, "ocel:global-object": {"oc)"
    R"(el:type": "__INVALID__"}, "minera:untypedO2o": true, "minera:frameHints": {"events": {"columns": ["d", "k"], ")"
    R"(dropped": [["k", 8, 3], ["d", 8, 1]], "rows": 3, "reserved": [["ocel:eid", 0], ["ocel:timestamp", 0], ["ocel:a)"
    R"(ctivity", -1]]}, "objects": {"columns": ["z", "s"], "dropped": [["z", 4, 1]], "rows": 1, "reserved": [["ocel:o)"
    R"(id", 0], ["ocel:type", -1]]}, "changes": {"columns": ["z", "s", "@@cumcount"], "dropped": [["s", 8, 2], ["@@cu)"
    R"(mcount", 2, 2], ["z", 8, 1]], "rows": 2, "field": 1, "time": 0}}})";

// The attribute names an objectTypes/object-types/ocel:objectTypes entry
// declares, in order, from `from` to the end of the type's entry.
std::vector<std::string> declared_names(const std::string& text, const std::string& from, const std::string& end,
                                        const std::string& name_prefix, char quote) {
  std::vector<std::string> names;
  const auto start = text.find(from);
  if (start == std::string::npos) return names;
  const auto stop = text.find(end, start + from.size());
  for (auto at = text.find(name_prefix, start); at != std::string::npos && at < stop;
       at = text.find(name_prefix, at + 1)) {
    const auto open = at + name_prefix.size();
    names.push_back(text.substr(open, text.find(quote, open) - open));
  }
  return names;
}

void type_declarations_after_filters() {
  const auto input = parse_ocel_json_literal_names(kSqliteTransport);
  const std::vector<std::string> filtered{"s", "z", "@@cumcount"};
  const auto all = serialize_ocel_json(input, OcelJsonFormat::kStandard, false);
  expect(declared_names(all, "\"objectTypes\"", "]}]", "{\"name\":\"", '"') ==
             std::vector<std::string>{"t", "z", "s", "@@cumcount"},
         "FB1-4: o1 holds z statically, so the type declares z, s: " + all);
  const auto kept = filter_ocel_object_attribute_exact(input, "s", {std::string("y")});
  const auto standard = serialize_ocel_json(kept, OcelJsonFormat::kStandard, false);
  expect(declared_names(standard, "\"objectTypes\"", "]}]", "{\"name\":\"", '"') ==
             std::vector<std::string>{"t", "s", "z", "@@cumcount"},
         "FB1-4: write_ocel2_json declares the change-only z after s: " + standard);
  expect(contains(standard, R"({"name":"z","type":"float"})"), "FB1-4: z typed by the changes frame: " + standard);
  OcelXmlExportOptions options;
  options.format = OcelXmlFormat::kStandard;
  const auto xml = serialize_ocel_xml(kept, options);
  expect(declared_names(xml, "<object-type name=\"t\">", "</object-type>", "<attribute name=\"", '"') == filtered,
         "FB1-4: write_ocel2_xml declares s, z: " + xml);
  const auto classic = serialize_ocel_json(kept, OcelJsonFormat::kClassic, false);
  const auto types = classic.substr(classic.find("\"ocel:objectTypes\""));
  const auto s_at = types.find("\"s\"");
  const auto z_at = types.find("\"z\"");
  expect(s_at != std::string::npos && z_at != std::string::npos && s_at < z_at,
         "FB1-4: write_ocel_json declares s before z: " + classic);
}

#if defined(MINERA_HAS_SQLITE)
// An object type's table (found by object_map_type) as export_ocel_sqlite
// writes it: its columns (name and declared type), then its rows (cells as
// text, "NULL" for NULL).
std::string written_table(const Ocel& log, const std::string& type) {
  const auto path = std::filesystem::temp_directory_path() / "minera_review_fb1_written.sqlite";
  std::filesystem::remove(path);
  export_ocel_sqlite(log, path);
  sqlite3* db = nullptr;
  sqlite3_open(path.string().c_str(), &db);
  std::string out, table;
  sqlite3_stmt* statement = nullptr;
  sqlite3_prepare_v2(db, "SELECT ocel_type_map FROM object_map_type WHERE ocel_type = ?1", -1, &statement, nullptr);
  sqlite3_bind_text(statement, 1, type.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(statement) == SQLITE_ROW)
    table = "object_" + std::string(reinterpret_cast<const char*>(sqlite3_column_text(statement, 0)));
  sqlite3_finalize(statement);
  sqlite3_prepare_v2(db, ("PRAGMA table_info(\"" + table + "\")").c_str(), -1, &statement, nullptr);
  while (sqlite3_step(statement) == SQLITE_ROW)
    out += std::string(reinterpret_cast<const char*>(sqlite3_column_text(statement, 1))) + " " +
           reinterpret_cast<const char*>(sqlite3_column_text(statement, 2)) + ",";
  sqlite3_finalize(statement);
  sqlite3_prepare_v2(db, ("SELECT * FROM \"" + table + "\"").c_str(), -1, &statement, nullptr);
  while (sqlite3_step(statement) == SQLITE_ROW) {
    out += "|";
    for (int i = 0; i < sqlite3_column_count(statement); ++i) {
      if (i) out += ",";
      out += sqlite3_column_type(statement, i) == SQLITE_NULL
                 ? std::string("NULL")
                 : std::string(reinterpret_cast<const char*>(sqlite3_column_text(statement, i)));
    }
  }
  sqlite3_finalize(statement);
  sqlite3_close(db);
  std::filesystem::remove(path);
  return out;
}

// t2/p11.py's tz1 and tz2: an object type declaring z (or ocel_time) a time
// with "02/03/2024"-like values (dateutil: naive in the objects frame) and a
// change (aware in the changes frame).
std::string naive_change_document(const std::string& key, bool change_of_key) {
  const std::string head = R"({"objectTypes":[{"name":"t","attributes":[{"name":")" + key +
                           R"(","type":"time"},{"name":"w","type":"float"}]}],"eventTypes":[{"name":"a","attributes":[]}],)";
  const std::string epoch = R"(","time":"1970-01-01T00:00:00Z","value":)";
  std::string o0 = R"({"name":")" + key + epoch + R"("02/03/2024"})";
  o0 += change_of_key ? R"(,{"name":")" + key + R"(","time":"2024-03-10T02:26:48Z","value":"02/05/2024"})"
                      : R"(,{"name":"w)" + epoch + R"(1.0},{"name":"w","time":"2024-03-10T02:26:48Z","value":2.0})";
  return head + R"("objects":[{"id":"o0","type":"t","attributes":[)" + o0 +
         R"(]},{"id":"o1","type":"t","attributes":[{"name":")" + key + epoch + R"("02/04/2024"}]}],)" +
         R"("events":[{"id":"e0","type":"a","time":"2024-01-01T10:00:00Z","attributes":[],)" +
         R"("relationships":[{"objectId":"o0","qualifier":"q"},{"objectId":"o1","qualifier":"q"}]}]})";
}

void sqlite_naive_next_to_aware() {
  const auto z = parse_ocel_json(serialize_ocel_json(parse_ocel_json(naive_change_document("z", true))));
  const auto z_table = written_table(z, "t");
  expect(contains(z_table, "ocel_id TEXT,z TEXT,ocel_changed_field TEXT,ocel_time TIMESTAMP,|"),
         "FB1-5: naive objects next to an aware change make z TEXT: " + z_table);
  expect(contains(z_table, "|o0,2024-02-03T00:00:00,NULL,NULL") && contains(z_table, "|o1,2024-02-04T00:00:00,NULL,NULL") &&
             contains(z_table, "|o0,2024-02-05T00:00:00+00:00,z,2024-03-10 02:26:48+00:00"),
         "FB1-5: z's isoformats: " + z_table);
  const auto time = parse_ocel_json(serialize_ocel_json(parse_ocel_json(naive_change_document("ocel_time", false))));
  const auto time_table = written_table(time, "t");
  expect(contains(time_table, "ocel_id TEXT,ocel_time TEXT,w REAL,ocel_changed_field TEXT,|"),
         "FB1-5: a naive ocel_time attribute next to the change times is TEXT: " + time_table);
  expect(contains(time_table, "|o0,2024-02-03T00:00:00,1.0,NULL") &&
             contains(time_table, "|o0,2024-03-10T02:26:48+00:00,2.0,w"),
         "FB1-5: ocel_time's isoformats: " + time_table);
}
#endif

}  // namespace

int main() {
  exact_filters_keep_prefixed_attributes();
  window_on_attribute_columns();
  csv_naive_date_columns();
  type_declarations_after_filters();
#if defined(MINERA_HAS_SQLITE)
  sqlite_naive_next_to_aware();
#endif
  if (failures != 0) {
    std::cerr << failures << " FB1 check(s) failed\n";
    return 1;
  }
  std::cout << "review FB1 tests passed\n";
  return 0;
}
