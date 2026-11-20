// Review EY1: the dtype of an emptied qualifier column and pm4py 2.7.22's
// classic JSON date declarations (review-harness round-ey: p/e1.py ..
// p/e6.py, p/d1.py). Observed black-box:
//
// - A frame whose rows the reader or a filter removed keeps its qualifier
//   column's dtype: write_ocel2_sqlite then declares an emptied int64 or
//   bool column INTEGER and a float64 one REAL (an object or str column
//   TEXT), and the activity-type matching adds its separator to such a
//   column with numpy's UFuncTypeError, rows or not (an object column of
//   ints raises Python's TypeError).
// - read_ocel2_json and read_ocel_json fill a missing qualifier with ''
//   after dropping the rows of empty ids, empty types and missing
//   activities and before dropping dangling links and unrelated or
//   empty-typed objects' links: a missing qualifier left at that point
//   makes an int, float or bool column an object one.
// - read_ocel2_sqlite types the column from every table row.
// - The E2O lifecycle enrichment leaves a str qualifier column (float64 on
//   a frame without rows); the O2O enrichment's computed links make the
//   O2O column str, and it keeps its dtype without one.
// - read_ocel_json ignores ocel:eventTypes and ocel:objectTypes, so a
//   declared date attribute keeps its texts (a str column the writers
//   declare "string").
//
// See docs/reference-2.7.22-parity.md, Review EY1.
#include "minera/ocel.hpp"
#include "minera/ocel_activity_type_matching.hpp"
#include "minera/ocel_filter.hpp"
#include "minera/ocel_sqlite.hpp"
#include "minera/ocel_transform.hpp"

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

constexpr auto kStr = OcelQualifierKind::kStr;
constexpr auto kInt = OcelQualifierKind::kInt;
constexpr auto kFloat = OcelQualifierKind::kFloat;
constexpr auto kBool = OcelQualifierKind::kBool;

std::string standard(const std::string& events, const std::string& objects) {
  return R"({"objectTypes":[{"name":"T","attributes":[]}],"eventTypes":[{"name":"A","attributes":[]}],"events":[)" +
         events + R"(],"objects":[)" + objects + "]}";
}

std::string event(const std::string& id, const std::string& day, const std::string& relationships,
                  const std::string& type = "A") {
  return R"({"id":")" + id + R"(","type":")" + type + R"(","time":"2024-01-0)" + day +
         R"(T10:00:00Z","relationships":[)" + relationships + "]}";
}

std::string rel(const std::string& object, const std::string& qualifier) {
  return R"({"objectId":")" + object + R"(","qualifier":)" + qualifier + "}";
}

std::string object(const std::string& id, const std::string& relationships = "", const std::string& type = "T") {
  return R"({"id":")" + id + R"(","type":")" + type + R"(","relationships":[)" + relationships + "]}";
}

// Events e1, e2 relating o1 and o2 with qualifier `q`, o1 linking o2 with `q`.
Ocel typed(const std::string& q) {
  return parse_ocel_json(standard(event("e1", "1", rel("o1", q) + "," + rel("o2", q)) + "," +
                                      event("e2", "2", rel("o1", q)),
                                  object("o1", rel("o2", q)) + "," + object("o2")));
}

Ocel emptied(const Ocel& log) {
  auto result = filter_ocel_events(log, {"e1", "e2"}, false);
  result.o2o.clear();
  return result;
}

#if defined(MINERA_HAS_SQLITE)
std::string declared(const Ocel& log, const std::string& table) {
  const auto path = std::filesystem::temp_directory_path() / "minera_review_ey1_written.sqlite";
  std::filesystem::remove(path);
  export_ocel_sqlite(log, path);
  sqlite3* db = nullptr;
  if (sqlite3_open(path.string().c_str(), &db) != SQLITE_OK) throw std::runtime_error("open the written file");
  std::string type;
  sqlite3_stmt* statement = nullptr;
  const std::string info = "SELECT type FROM pragma_table_info('" + table + "') WHERE name = 'ocel_qualifier'";
  if (sqlite3_prepare_v2(db, info.c_str(), -1, &statement, nullptr) == SQLITE_OK &&
      sqlite3_step(statement) == SQLITE_ROW)
    type = reinterpret_cast<const char*>(sqlite3_column_text(statement, 0));
  sqlite3_finalize(statement);
  sqlite3_close(db);
  std::filesystem::remove(path);
  return type;
}

Ocel sqlite_round_trip(const Ocel& log) {
  const auto path = std::filesystem::temp_directory_path() / "minera_review_ey1_round_trip.sqlite";
  std::filesystem::remove(path);
  export_ocel_sqlite(log, path);
  auto read = import_ocel_sqlite(path);
  std::filesystem::remove(path);
  return read;
}
#endif

const std::string kUfunc = "UFuncTypeError: ufunc 'add' did not contain a loop with signature matching types (dtype('";

std::string matching_error(const Ocel& log, const std::string& activity_key, const std::string& type_key) {
  OcelActivityTypeMatchingOptions options;
  options.activity_key = activity_key;
  options.object_type_key = type_key;
  return error_of([&] { (void)filter_ocel_activity_type_matching_exact(log, {{"T", {"A"}}}, options); });
}

// The readers' dtypes (p/e1.py, p/e5.py, p/e6.py).
void reader_dtypes() {
  expect(typed("2").relation_qualifier_dtype == kInt && typed("2").o2o_qualifier_dtype == kInt, "ints: int64");
  expect(typed("2.5").relation_qualifier_dtype == kFloat, "floats: float64");
  expect(typed("true").o2o_qualifier_dtype == kBool, "bools: bool");
  expect(typed("\"q\"").relation_qualifier_dtype == kStr, "texts: str");
  // An int next to a null the reader keeps: object (2.0 and '').
  auto log = parse_ocel_json(standard(event("e1", "1", rel("o1", "2") + "," + rel("o2", "null")),
                                      object("o1") + "," + object("o2")));
  expect(log.relation_qualifier_dtype == kStr, "a kept null makes an object column");
  // ... on a row of an event the reader drops (an empty type): float64.
  log = parse_ocel_json(standard(event("e1", "1", rel("o1", "2") + "," + rel("o2", "3")) + "," +
                                     event("e2", "2", rel("o1", "null"), ""),
                                 object("o1") + "," + object("o2")));
  expect(log.relation_qualifier_dtype == kFloat, "a null dropped before the fill leaves float64");
  // A null on a dangling link: filled before the drop, object.
  log = parse_ocel_json(standard(event("e1", "1", rel("o1", "2") + "," + rel("o2", "3")),
                                 object("o1", rel("o2", "7") + "," + rel("zz", "null")) + "," + object("o2")));
  expect(log.o2o_qualifier_dtype == kStr, "a dangling null link makes an object O2O column");
  // ... on an empty-id object's link: dropped first, float64.
  log = parse_ocel_json(standard(event("e1", "1", rel("o1", "2") + "," + rel("o2", "3") + "," + rel("", "4")),
                                 object("o1", rel("o2", "7")) + "," + object("o2") + "," +
                                     object("", rel("o1", "null"))));
  expect(log.o2o_qualifier_dtype == kFloat, "an empty-id object's null link leaves float64");
  // Classic: a kept missing typedOmap qualifier, one on an event with an
  // empty activity.
  const auto classic = [](const std::string& extra) {
    return parse_ocel_json(
        R"({"ocel:global-event":{},"ocel:global-object":{},"ocel:global-log":{},"ocel:events":{"e1":{"ocel:activity":"x","ocel:timestamp":"2024-01-01T00:00:00Z","ocel:omap":["o1","o2"],"ocel:vmap":{},"ocel:typedOmap":[{"ocel:oid":"o1","ocel:qualifier":2})" +
        extra +
        R"(},"ocel:objects":{"o1":{"ocel:type":"A","ocel:ovmap":{}},"o2":{"ocel:type":"A","ocel:ovmap":{}}}})");
  };
  expect(classic("]}").relation_qualifier_dtype == kStr, "classic: a kept missing qualifier, object");
  expect(classic(R"(,{"ocel:oid":"o2","ocel:qualifier":3}]},"e2":{"ocel:activity":"","ocel:timestamp":"2024-01-02T00:00:00Z","ocel:omap":["o1"],"ocel:vmap":{},"ocel:typedOmap":[]})")
                 .relation_qualifier_dtype == kFloat,
         "classic: a missing one on a dropped event, float64");
}

// write_ocel2_sqlite on emptied frames (p/e1.py).
void emptied_tables() {
#if defined(MINERA_HAS_SQLITE)
  expect(declared(emptied(typed("2")), "event_object") == "INTEGER", "an emptied int64 column is INTEGER");
  expect(declared(emptied(typed("2")), "object_object") == "INTEGER", "the O2O one too");
  expect(declared(emptied(typed("2.5")), "event_object") == "REAL", "float64: REAL");
  expect(declared(emptied(typed("true")), "event_object") == "INTEGER", "bool: INTEGER");
  expect(declared(emptied(typed("\"q\"")), "event_object") == "TEXT", "str: TEXT");
  // Every link dangles: the reader empties the O2O frame and keeps int64.
  const auto dangling = parse_ocel_json(standard(event("e1", "1", rel("o1", "\"a\"") + "," + rel("o2", "\"b\"")),
                                                 object("o1", rel("zz", "5")) + "," + object("o2")));
  expect(dangling.o2o.empty() && declared(dangling, "object_object") == "INTEGER",
         "dangling int links leave an INTEGER object_object");
  // An object column of ints (a str on a dropped event's row) empties to TEXT.
  const auto object_ints = parse_ocel_json(standard(event("e1", "1", rel("o1", "2") + "," + rel("o2", "2")) + "," +
                                                        event("e2", "2", rel("o1", "\"s\""), ""),
                                                    object("o1") + "," + object("o2")));
  expect(declared(object_ints, "event_object") == "INTEGER", "its rows still write INTEGER");
  expect(declared(emptied(object_ints), "event_object") == "TEXT", "emptied, an object column is TEXT");
  // read_ocel2_sqlite: an INTEGER table's dtype stays when a filter empties it.
  const auto read = sqlite_round_trip(typed("2"));
  expect(read.relation_qualifier_dtype == kInt, "read_ocel2_sqlite types the column int64");
  expect(declared(emptied(read), "event_object") == "INTEGER", "and its emptied frame writes INTEGER");
#endif
}

// The activity-type matching follows the dtype (p/e1.py, p/e2.py).
void matching() {
  expect(matching_error(emptied(typed("2")), "ocel:qualifier", "ocel:type") ==
             kUfunc + "int64'), dtype('<U4')) -> None",
         "an emptied int64 column still raises numpy's error");
  expect(matching_error(emptied(typed("2")), "ocel:activity", "ocel:qualifier").empty(),
         "as the type key an emptied column filters");
  const auto object_ints = parse_ocel_json(standard(event("e1", "1", rel("o1", "2") + "," + rel("o2", "2")) + "," +
                                                        event("e2", "2", rel("o1", "\"s\""), ""),
                                                    object("o1") + "," + object("o2")));
  expect(matching_error(object_ints, "ocel:qualifier", "ocel:type") ==
             "TypeError: unsupported operand type(s) for +: 'int' and 'str'",
         "an object column of ints raises Python's TypeError");
}

// Both transports carry a dtype the rows do not show.
void transports() {
  const auto log = emptied(typed("2.5"));
  for (const auto format : {OcelJsonFormat::kClassic, OcelJsonFormat::kStandard}) {
    const auto json = serialize_ocel_json(log, format, true);
    expect(contains(json, R"("minera:qualifierDtypes":{"relations":"float64","o2o":"float64"})"),
           "the transport names the dtypes");
    const auto read = parse_ocel_json(json);
    expect(read.relation_qualifier_dtype == kFloat && read.o2o_qualifier_dtype == kFloat, "and reads them back");
  }
  const auto rows = typed("2");
  expect(!contains(serialize_ocel_json(rows, OcelJsonFormat::kStandard, true), "minera:qualifierDtypes"),
         "rows that show the dtype need no marker");
  auto unknown = serialize_ocel_json(rows, OcelJsonFormat::kStandard, true);
  unknown.insert(unknown.size() - 1, R"(,"minera:qualifierDtypes":{"relations":"int32"})");
  expect(error_of([&] { (void)parse_ocel_json(unknown); }).find("minera:qualifierDtypes") != std::string::npos,
         "an unknown dtype is refused: " + error_of([&] { (void)parse_ocel_json(unknown); }));
}

// The enrichments (p/e3b.py).
void enrichments() {
#if defined(MINERA_HAS_SQLITE)
  OcelEnrichmentOptions lifecycle;
  lifecycle.include_interaction = lifecycle.include_descendants = lifecycle.include_inheritance =
      lifecycle.include_cobirth = lifecycle.include_codeath = false;
  const auto enriched = enrich_ocel(typed("2"), lifecycle);
  expect(enriched.relation_qualifier_dtype == kStr, "lifecycle texts leave a str column");
  expect(declared(emptied(enriched), "event_object") == "TEXT", "emptied afterwards: TEXT");
  const auto empty_enriched = enrich_ocel(emptied(typed("\"q\"")), lifecycle);
  expect(declared(empty_enriched, "event_object") == "REAL", "lifecycle on a frame without rows: REAL");
  OcelEnrichmentOptions links;
  links.include_lifecycle_qualifiers = false;
  const auto linked = enrich_ocel(typed("2"), links);
  expect(linked.o2o_qualifier_dtype == kStr, "computed links make the O2O column str");
  auto unlinked = parse_ocel_json(standard(event("e1", "1", rel("o1", "2")) + "," + event("e2", "2", rel("o2", "2")),
                                           object("o1", rel("o2", "7")) + "," + object("o2")));
  links.include_cobirth = links.include_codeath = links.include_descendants = links.include_inheritance = false;
  const auto same = enrich_ocel(unlinked, links);
  expect(same.o2o.size() == 1 && same.o2o_qualifier_dtype == kInt, "without a computed link it stays int64");
#endif
}

// read_ocel_json ignores the classic catalog (p/d1.py).
void classic_dates() {
  const std::string file =
      R"({"ocel:global-event":{},"ocel:global-object":{},"ocel:global-log":{},"ocel:events":{"e1":{"ocel:activity":"x","ocel:timestamp":"2024-01-01T00:00:00Z","ocel:omap":["o1"],"ocel:vmap":{"d":"2024-02-02T00:00:00Z"}}},"ocel:objects":{"o1":{"ocel:type":"A","ocel:ovmap":{"od":"2024-03-01T00:00:00Z"}}},"ocel:eventTypes":{"x":{"d":"date"}},"ocel:objectTypes":{"A":{"od":"Timestamp"}}})";
  const auto log = parse_ocel_json(file);
  expect(log.event_type_attributes.at("x").at("d") == "string" && log.object_type_attributes.at("A").at("od") == "string",
         "declared dates read as strings");
  const auto json = serialize_ocel_json(log, OcelJsonFormat::kStandard, false);
  expect(contains(json, R"({"name":"d","type":"string"})") && contains(json, R"("2024-02-02T00:00:00Z")"),
         "write_ocel2_json declares the texts string");
  expect(!contains(serialize_ocel_json(log, OcelJsonFormat::kClassic, true), "minera:declaredDates"),
         "its classic transport needs no marker");
  // A log whose dates are datetimes (an OCEL 2.0 read) keeps them through
  // the classic transport.
  const auto dated = parse_ocel_json(
      R"({"objectTypes":[{"name":"T","attributes":[{"name":"od","type":"date"}]}],"eventTypes":[{"name":"A","attributes":[]}],"events":[{"id":"e1","type":"A","time":"2024-01-01T10:00:00Z","relationships":[{"objectId":"o1","qualifier":"q"}]}],"objects":[{"id":"o1","type":"T","attributes":[{"name":"od","value":"2024-03-01T00:00:00Z","time":"1970-01-01T00:00:00Z"}]}]})");
  const auto transport = serialize_ocel_json(dated, OcelJsonFormat::kClassic, true);
  expect(contains(transport, R"("minera:declaredDates":true)"), "the classic transport marks declared dates");
  expect(parse_ocel_json(transport).object_type_attributes.at("T").at("od") == "date", "and keeps them");
}

}  // namespace

int main() {
  reader_dtypes();
  emptied_tables();
  matching();
  transports();
  enrichments();
  classic_dates();
  if (failures != 0) {
    std::cerr << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "review EY1 tests passed\n";
  return 0;
}
