// Review EX1: number, bool and null qualifiers in pm4py 2.7.22's OCEL JSON
// readers (review-harness round-ex: p/p1.py .. p5c.py read hand-made files,
// p/wj.py and p/wj2.py run the reads through every writer). Observed
// black-box:
//
// - read_ocel2_json and read_ocel_json keep a JSON number or bool qualifier
//   as the Python value. Their relations and O2O frames are built from
//   records, so a qualifier column's dtype comes from every row the frame is
//   built from: ints only give an int64 column, a float or a missing
//   qualifier next to ints a float64 one (an int prints as a float), bools
//   only a bool column, any other mix an object column keeping each value.
//   A missing qualifier (null, NaN, or an omap entry without a typedOmap
//   qualifier) then reads as ''.
// - The rows that count: the O2O frame holds every link of each object's
//   (last) listing, dangling links and links of objects the reader then
//   drops included; the relations frame holds each event's (last) listing
//   without its relations to unlisted objects, one per object (OCEL 2.0: the
//   last; OCEL 1.0: the omap's), events and objects the reader then drops
//   (empty ids, missing activities) included.
// - write_ocel2_json and write_ocel_json print a bool as a JSON bool;
//   write_ocel2_xml raises "TypeError: Argument must be bytes or unicode, got
//   'bool'" on an object's bool qualifier and prints an event's as True or
//   False; write_ocel2_sqlite declares a bool-only column INTEGER (1 and 0)
//   and stores a bool in a TEXT column as '1' or '0'.
//
// Round EW's OCEL 2.0 transport (an OCEL SQLite import's open source view)
// carried number qualifiers without saying so, so every route refused it;
// it now carries "minera:typedQualifiers" as the classic transport does.
//
// See docs/reference-2.7.22-parity.md, Review EX1.
#include "minera/ocel.hpp"
#include "minera/ocel_activity_type_matching.hpp"
#include "minera/ocel_sqlite.hpp"
#include "minera/ocel_transform.hpp"
#include "minera/ocel_xml.hpp"

#include <filesystem>
#include <functional>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
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

bool contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

using Kinds = std::vector<std::pair<std::string, OcelQualifierKind>>;

Kinds relation_kinds(const Ocel& log) {
  Kinds kinds;
  for (const auto& relation : log.relations) kinds.emplace_back(relation.qualifier, relation.qualifier_kind);
  return kinds;
}

Kinds o2o_kinds(const Ocel& log) {
  Kinds kinds;
  for (const auto& relation : log.o2o) kinds.emplace_back(relation.qualifier, relation.qualifier_kind);
  return kinds;
}

constexpr auto kStr = OcelQualifierKind::kStr;
constexpr auto kInt = OcelQualifierKind::kInt;
constexpr auto kFloat = OcelQualifierKind::kFloat;
constexpr auto kBool = OcelQualifierKind::kBool;

// An OCEL 2.0 JSON file of events e1, e2 (type A) and objects (type T);
// `events` and `objects` are the JSON arrays' items.
std::string standard(const std::string& events, const std::string& objects) {
  return R"({"objectTypes":[{"name":"T","attributes":[]}],"eventTypes":[{"name":"A","attributes":[]}],"events":[)" +
         events + R"(],"objects":[)" + objects + "]}";
}

std::string event(const std::string& id, const std::string& day, const std::string& relationships) {
  return R"({"id":")" + id + R"(","type":"A","time":"2024-01-0)" + day + R"(T10:00:00Z","relationships":[)" +
         relationships + "]}";
}

std::string rel(const std::string& object, const std::string& qualifier) {
  return R"({"objectId":")" + object + R"(","qualifier":)" + qualifier + "}";
}

std::string object(const std::string& id, const std::string& relationships = "") {
  return R"({"id":")" + id + R"(","type":"T","relationships":[)" + relationships + "]}";
}

// The reads of p/p1.py and p/p3.py.
void standard_columns() {
  const auto objects = object("o1", rel("o2", "7")) + "," + object("o2");
  // e3 relates o2, so the reader keeps o1's link to it.
  auto log = parse_ocel_json(standard(event("e1", "1", rel("o1", "2")) + "," + event("e2", "2", rel("o1", "3")) +
                                          "," + event("e3", "2", rel("o2", "4")),
                                      objects));
  expect(relation_kinds(log) == Kinds{{"2", kInt}, {"3", kInt}, {"4", kInt}}, "ints only: an int64 column");
  expect(o2o_kinds(log) == Kinds{{"7", kInt}}, "an int O2O qualifier");

  log = parse_ocel_json(standard(event("e1", "1", rel("o1", "2")) + "," + event("e2", "2", rel("o1", "null")) + "," +
                                     event("e3", "2", rel("o2", "5")),
                                 object("o1", rel("o2", "7") + "," + rel("o2", "null")) + "," + object("o2")));
  expect(relation_kinds(log) == Kinds{{"2.0", kFloat}, {"", kStr}, {"5.0", kFloat}},
         "an int next to a null: 2.0 and ''");
  expect(o2o_kinds(log) == Kinds{{"7.0", kFloat}, {"", kStr}}, "the O2O column likewise");

  log = parse_ocel_json(standard(event("e1", "1", rel("o1", "2") + "," + rel("o2", "\"q\"")) + "," +
                                     event("e2", "2", rel("o1", "2.5")),
                                 objects));
  expect(relation_kinds(log) == Kinds{{"2", kInt}, {"q", kStr}, {"2.5", kFloat}},
         "a str keeps an object column: the int, the str and the float");

  log = parse_ocel_json(standard(event("e1", "1", rel("o1", "true")) + "," + event("e2", "2", rel("o1", "false")) +
                                     "," + event("e3", "2", rel("o2", "true")),
                                 object("o1", rel("o2", "true") + "," + rel("o2", "null")) + "," + object("o2")));
  expect(relation_kinds(log) == Kinds{{"True", kBool}, {"False", kBool}, {"True", kBool}}, "bools only: a bool column");
  expect(o2o_kinds(log) == Kinds{{"True", kBool}, {"", kStr}}, "a bool next to a null stays a bool");

  log = parse_ocel_json(standard(event("e1", "1", rel("o1", "true") + "," + rel("o2", "2")) + "," +
                                     event("e2", "2", rel("o1", "1.5")),
                                 objects));
  expect(relation_kinds(log) == Kinds{{"True", kBool}, {"2", kInt}, {"1.5", kFloat}},
         "a bool next to numbers: an object column");

  log = parse_ocel_json(standard(event("e1", "1", rel("o1", "NaN")) + "," + event("e2", "2", rel("o1", "1e20")),
                                 objects));
  expect(relation_kinds(log) == Kinds{{"", kStr}, {"1e+20", kFloat}}, "NaN reads as ''; a float prints its repr");
}

// The rows a frame is built from (p/p3.py, p4.py, p5.py).
void standard_rows() {
  // A relation to an unlisted object is no row; a link to one is.
  auto log = parse_ocel_json(standard(
      event("e1", "1", rel("o1", "2")) + "," + event("e2", "2", rel("o2", "5") + "," + rel("o9", "2.5")),
      object("o1", rel("o2", "7") + "," + rel("o9", "2.5")) + "," + object("o2")));
  expect(relation_kinds(log) == Kinds{{"2", kInt}, {"5", kInt}}, "a relation to an unlisted object does not count");
  expect(o2o_kinds(log) == Kinds{{"7.0", kFloat}}, "a dangling link counts");

  // A relation the reader replaces by a later one to the same object, and an
  // event's earlier listing, are no rows.
  log = parse_ocel_json(standard(event("e1", "1", rel("o1", "2.5") + "," + rel("o1", "2")) + "," +
                                     event("e2", "2", rel("o2", "5")) + "," + event("e2", "2", rel("o2", "6")),
                                 object("o1") + "," + object("o2")));
  expect(relation_kinds(log) == Kinds{{"2", kInt}, {"6", kInt}}, "replaced relations do not count");

  // An event the reader then drops (an empty id) counts.
  log = parse_ocel_json(standard(event("e1", "1", rel("o1", "2")) + "," + event("", "2", rel("o2", "2.5")),
                                 object("o1") + "," + object("o2")));
  expect(relation_kinds(log) == Kinds{{"2.0", kFloat}}, "a dropped event's relation counts");

  // An object's earlier listing is no row; a link of an object the reader
  // drops (an unrelated one) is.
  log = parse_ocel_json(standard(event("e1", "1", rel("o1", "\"a\"") + "," + rel("o2", "\"b\"")),
                                 object("o1", rel("o2", "2.5")) + "," + object("o2") + "," +
                                     object("o1", rel("o2", "7")) + "," + object("o3", rel("o1", "true"))));
  expect(o2o_kinds(log) == Kinds{{"7", kInt}}, "an unrelated object's bool link makes an object column");
}

// read_ocel_json (p/p2.py, p4.py, p5c.py).
std::string classic(const std::string& events, const std::string& objects) {
  return R"({"ocel:global-event":{},"ocel:global-object":{},"ocel:global-log":{"ocel:attribute-names":[],"ocel:object-types":["T"]},"ocel:events":{)" +
         events + R"(},"ocel:objects":{)" + objects + "}}";
}

std::string classic_event(const std::string& id, const std::string& day, const std::string& omap,
                          const std::string& typed, const std::string& activity = "\"A\"") {
  return "\"" + id + R"(":{"ocel:activity":)" + activity + R"(,"ocel:timestamp":"2024-01-0)" + day +
         R"(T10:00:00","ocel:omap":[)" + omap + R"(],"ocel:vmap":{})" +
         (typed.empty() ? "" : R"(,"ocel:typedOmap":[)" + typed + "]") + "}";
}

std::string typed(const std::string& object, const std::string& qualifier) {
  return R"({"ocel:oid":")" + object + R"(","ocel:qualifier":)" + qualifier + "}";
}

std::string classic_object(const std::string& id, const std::string& o2o = "", const std::string& type = "\"T\"") {
  return "\"" + id + R"(":{"ocel:type":)" + type + R"(,"ocel:ovmap":{})" +
         (o2o.empty() ? "" : R"(,"ocel:o2o":[)" + o2o + "]") + "}";
}

void classic_columns() {
  const auto objects = classic_object("o1", typed("o2", "7")) + "," + classic_object("o2");
  // e2's relation has no typedOmap qualifier: missing, so the column is float64.
  auto log = parse_ocel_json(classic(classic_event("e1", "1", "\"o1\"", typed("o1", "2")) + "," +
                                         classic_event("e2", "2", "\"o2\"", ""),
                                     objects));
  expect(relation_kinds(log) == Kinds{{"2.0", kFloat}, {"", kStr}}, "an omap entry without a qualifier is missing");
  expect(o2o_kinds(log) == Kinds{{"7", kInt}}, "an int O2O qualifier");

  log = parse_ocel_json(classic(classic_event("e1", "1", "\"o1\"", typed("o1", "2")) + "," +
                                    classic_event("e2", "2", "\"o2\"", typed("o2", "5")),
                                objects));
  expect(relation_kinds(log) == Kinds{{"2", kInt}, {"5", kInt}}, "every relation qualified: int64");

  log = parse_ocel_json(classic(classic_event("e1", "1", "\"o1\"", typed("o1", "true")) + "," +
                                    classic_event("e2", "2", "\"o2\"", ""),
                                classic_object("o1", typed("o2", "true") + "," + typed("o2", "null")) + "," +
                                    classic_object("o2")));
  expect(relation_kinds(log) == Kinds{{"True", kBool}, {"", kStr}}, "a bool next to a missing qualifier");
  expect(o2o_kinds(log) == Kinds{{"True", kBool}, {"", kStr}}, "a bool next to a null link qualifier");

  // An event the reader drops (a null activity) counts; a typedOmap entry
  // for an object the omap does not list does not.
  log = parse_ocel_json(classic(classic_event("e1", "1", "\"o1\"", typed("o1", "2") + "," + typed("o2", "2.5")) +
                                    "," + classic_event("e2", "2", "\"o2\"", typed("o2", "5")),
                                objects));
  expect(relation_kinds(log) == Kinds{{"2", kInt}, {"5", kInt}}, "a qualifier outside the omap does not count");
  log = parse_ocel_json(classic(classic_event("e1", "1", "\"o1\"", typed("o1", "2")) + "," +
                                    classic_event("e2", "2", "\"o2\"", typed("o2", "5")) + "," +
                                    classic_event("e3", "2", "\"o2\"", typed("o2", "2.5"), "null"),
                                objects));
  expect(relation_kinds(log) == Kinds{{"2.0", kFloat}, {"5.0", kFloat}}, "a dropped event's qualifier counts");

  // Every link counts, a dangling one too.
  log = parse_ocel_json(classic(classic_event("e1", "1", "\"o1\",\"o2\"", typed("o1", "\"a\"") + "," + typed("o2", "\"b\"")),
                                classic_object("o1", typed("o2", "7") + "," + typed("o9", "2.5")) + "," +
                                    classic_object("o2")));
  expect(o2o_kinds(log) == Kinds{{"7.0", kFloat}}, "a dangling link counts");
}

void refusals() {
  const auto objects = object("o1") + "," + object("o2");
  for (const auto& [value, part] : std::vector<std::pair<std::string, std::string>>{
           {"[1]", "is a list ([1])"},
           {"{\"a\":1}", "is a dict ({'a': 1})"},
           {"9223372036854775808", "is an integer past 64 bits (9223372036854775808)"},
           {"Infinity", "is an infinite float (inf)"}}) {
    const auto error = error_of([&] { (void)parse_ocel_json(standard(event("e1", "1", rel("o1", value)), objects)); });
    expect(contains(error, part) && contains(error, "which a native OCEL cannot hold"),
           "a " + value + " qualifier is refused (" + error + ")");
  }
  expect(error_of([&] {
           (void)parse_ocel_json(standard(event("e1", "1", R"({"objectId":"o1"})"), objects));
         }) == "KeyError: 'qualifier'",
         "a relationship without a qualifier raises KeyError 'qualifier'");
}

std::string xml2(const Ocel& log) {
  OcelXmlExportOptions options;
  options.format = OcelXmlFormat::kStandard;
  return serialize_ocel_xml(log, options);
}

#if defined(MINERA_HAS_SQLITE)
std::pair<std::string, std::vector<std::string>> written_column(const Ocel& log, const std::string& table) {
  const auto path = std::filesystem::temp_directory_path() / "minera_review_ex1_written.sqlite";
  std::filesystem::remove(path);
  export_ocel_sqlite(log, path);
  sqlite3* db = nullptr;
  if (sqlite3_open(path.string().c_str(), &db) != SQLITE_OK) throw std::runtime_error("open the written file");
  std::string declared;
  std::vector<std::string> cells;
  sqlite3_stmt* statement = nullptr;
  const std::string info = "SELECT type FROM pragma_table_info('" + table + "') WHERE name = 'ocel_qualifier'";
  if (sqlite3_prepare_v2(db, info.c_str(), -1, &statement, nullptr) == SQLITE_OK &&
      sqlite3_step(statement) == SQLITE_ROW)
    declared = reinterpret_cast<const char*>(sqlite3_column_text(statement, 0));
  sqlite3_finalize(statement);
  const std::string select = "SELECT typeof(ocel_qualifier), ocel_qualifier FROM " + table + " ORDER BY rowid";
  if (sqlite3_prepare_v2(db, select.c_str(), -1, &statement, nullptr) == SQLITE_OK)
    while (sqlite3_step(statement) == SQLITE_ROW)
      cells.push_back(std::string(reinterpret_cast<const char*>(sqlite3_column_text(statement, 0))) + ":" +
                      reinterpret_cast<const char*>(sqlite3_column_text(statement, 1)));
  sqlite3_finalize(statement);
  sqlite3_close(db);
  std::filesystem::remove(path);
  return {declared, cells};
}
#endif

// p/wj.py and wj2.py.
void writers() {
  const auto bools = parse_ocel_json(standard(
      event("e1", "1", rel("o1", "true") + "," + rel("o2", "false")) + "," + event("e2", "2", rel("o1", "true")),
      object("o1", rel("o2", "true")) + "," + object("o2", rel("o1", "false"))));
  const auto json2 = serialize_ocel_json(bools, OcelJsonFormat::kStandard, false);
  expect(contains(json2, R"("qualifier":true})") && contains(json2, R"("qualifier":false})"),
         "write_ocel2_json prints bools as JSON bools");
  const auto json1 = serialize_ocel_json(bools, OcelJsonFormat::kClassic, false);
  expect(contains(json1, R"("ocel:qualifier":true})") && contains(json1, R"("ocel:qualifier":false})"),
         "write_ocel_json too");
  expect(error_of([&] { (void)xml2(bools); }) == "TypeError: Argument must be bytes or unicode, got 'bool'",
         "write_ocel2_xml raises on an object's bool qualifier");
  const auto events_only = parse_ocel_json(standard(
      event("e1", "1", rel("o1", "true") + "," + rel("o2", "2")) + "," + event("e2", "2", rel("o1", "2.5")),
      object("o1", rel("o2", "\"x\"")) + "," + object("o2")));
  const auto xml = xml2(events_only);
  expect(contains(xml, R"(object-id="o1" qualifier="True")") && contains(xml, R"(object-id="o2" qualifier="2")"),
         "an event's bool qualifier prints True in the OCEL 2.0 XML");
  const auto graph = serialize_ocel_heterogeneous_graph_json(discover_ocel_heterogeneous_graph(bools));
  expect(contains(graph, R"("qualifier":true)") && contains(graph, R"("qualifier":false)"),
         "the event-object graph carries the bools");
#if defined(MINERA_HAS_SQLITE)
  const auto [declared, cells] = written_column(bools, "event_object");
  expect(declared == "INTEGER" && cells == std::vector<std::string>{"integer:1", "integer:0", "integer:1"},
         "write_ocel2_sqlite writes a bool column INTEGER, 1 and 0");
  const auto [text_declared, text_cells] = written_column(events_only, "event_object");
  expect(text_declared == "TEXT" && text_cells == std::vector<std::string>{"text:1", "text:2", "text:2.5"},
         "a bool in a TEXT column is '1'");
#endif
}

// Both transports keep the kinds as they are (no new frame typing).
void transports() {
  const auto log = parse_ocel_json(standard(
      event("e1", "1", rel("o1", "true") + "," + rel("o2", "2")) + "," + event("e2", "2", rel("o1", "\"\"")),
      object("o1", rel("o2", "2.0")) + "," + object("o2", rel("o1", "false"))));
  const Kinds relations{{"True", kBool}, {"2", kInt}, {"", kStr}};
  const Kinds o2o{{"2.0", kFloat}, {"False", kBool}};
  expect(relation_kinds(log) == relations && o2o_kinds(log) == o2o, "the read");
  for (const auto format : {OcelJsonFormat::kClassic, OcelJsonFormat::kStandard}) {
    const auto json = serialize_ocel_json(log, format, true);
    expect(contains(json, R"("minera:typedQualifiers":true)"), "the transport says it holds typed qualifiers");
    const auto read = parse_ocel_json(json);
    expect(relation_kinds(read) == relations && o2o_kinds(read) == o2o, "the transport keeps every kind");
  }
}

// The activity-type matching with the qualifier column as a key (p/atm2.py
// on p/wj.py's logs).
void matching_keys() {
  const auto log_of = [](const std::string& q1, const std::string& q2, const std::string& q3) {
    return parse_ocel_json(standard(event("e1", "1", rel("o1", q1) + "," + rel("o2", q2)) + "," +
                                        event("e2", "2", rel("o1", q3)),
                                    object("o1") + "," + object("o2")));
  };
  const auto error = [](const Ocel& log, const std::string& activity_key, const std::string& type_key,
                        std::optional<std::string> separator = std::nullopt) {
    OcelActivityTypeMatchingOptions options;
    options.activity_key = activity_key;
    options.object_type_key = type_key;
    options.temporary_separator = std::move(separator);
    return error_of([&] { (void)filter_ocel_activity_type_matching_exact(log, {{"T", {"A"}}}, options); });
  };
  const std::string ufunc = "UFuncTypeError: ufunc 'add' did not contain a loop with signature matching types (dtype('";
  const auto ints = log_of("2", "3", "4");
  expect(error(ints, "ocel:qualifier", "ocel:type") == ufunc + "int64'), dtype('<U4')) -> None",
         "an int64 qualifier column as the activity key: " + error(ints, "ocel:qualifier", "ocel:type"));
  expect(error(ints, "ocel:qualifier", "ocel:type", "ab") == ufunc + "int64'), dtype('<U2')) -> None",
         "with a separator of two");
  expect(error(log_of("true", "false", "true"), "ocel:qualifier", "ocel:type") == ufunc + "bool'), dtype('<U4')) -> None",
         "a bool column");
  expect(error(log_of("2", "\"q\"", "2.5"), "ocel:qualifier", "ocel:type") ==
             "TypeError: unsupported operand type(s) for +: 'int' and 'str'",
         "an object column names its first value that is no str");
  expect(error(log_of("2", "null", "3"), "ocel:qualifier", "ocel:type") ==
             "TypeError: unsupported operand type(s) for +: 'float' and 'str'",
         "floats next to ''");
  expect(error(ints, "ocel:activity", "ocel:qualifier") == "TypeError: can only concatenate str (not \"int\") to str",
         "the qualifier column as the type key");
  expect(error(log_of("\"a\"", "\"b\"", "\"c\""), "ocel:qualifier", "ocel:type").empty(), "a str column filters");
}

}  // namespace

int main() {
  standard_columns();
  standard_rows();
  classic_columns();
  refusals();
  writers();
  transports();
  matching_keys();
  if (failures != 0) {
    std::cerr << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "review EX1 tests passed\n";
  return 0;
}
