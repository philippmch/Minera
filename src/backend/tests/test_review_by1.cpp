// Review BY1: pandas 3.0.5 table filters with non-default keys raise the
// vectorised errors of their column dtype (`col + ","` on int64, bool and
// datetime columns), compare a datetime column with a parsed string, sort
// case values with NaT and NaN as numpy does (typed arrays collapse them
// into one run at the end, object arrays keep each as its own unordered
// run), and count coverage over the non-missing case ids. pm4py's OCEL
// connected-component split drops O2O rows and object changes; an OCEL 2.0
// SQLite file keeps its event_object rows, and its object type table rows,
// in their own order sorted by time, which the lifecycle qualifiers and the
// event/object graph follow, also through the OCEL 2.0 JSON transport.
// Values are taken from pm4py 2.7.22 black-box on the same input
// (review-harness round-by, by1/mk.py and relord/).
#include "minera/dataframe_format_json.hpp"
#include "minera/ocel.hpp"
#include "minera/ocel_sqlite.hpp"
#include "minera/ocel_transform.hpp"
#include "minera/pandas_attribute_repetition_filter.hpp"
#include "minera/pandas_between_filter.hpp"
#include "minera/pandas_case_filter.hpp"
#include "minera/pandas_paths_filter.hpp"
#include "minera/pandas_timestamp_filter.hpp"
#include "minera/pandas_variants_filter.hpp"

#include <algorithm>
#include <filesystem>
#include <functional>
#include <iostream>
#include <sstream>
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

void check(const std::string& label, const std::string& seen, const std::string& expected) {
  if (seen == expected) return;
  ++failures;
  std::cerr << label << ":\n  seen     " << seen << "\n  expected " << expected << "\n";
}

template <class Error>
void check_error(const std::string& label, const std::function<void()>& run, const std::string& type, const std::string& message) {
  try {
    run();
  } catch (const Error& error) {
    if (error.type == type && error.message == message) return;
    ++failures;
    std::cerr << label << ":\n  error    " << error.type << ": " << error.message << "\n  expected " << type << ": " << message << "\n";
    return;
  } catch (const std::exception& error) {
    ++failures;
    std::cerr << label << ": unexpected error " << error.what() << "\n";
    return;
  }
  ++failures;
  std::cerr << label << ": no error, expected " << type << ": " << message << "\n";
}

std::string quoted(const std::string& value) { return "\"" + value + "\""; }
std::string string_cell(const std::string& key, const std::string& value) {
  return "{\"key\": " + quoted(key) + ", \"value\": " + quoted(value) + "}";
}
// "" is a missing cell (NaT, None, NaN).
std::string date_cell(const std::string& key, const std::string& value, bool utc) {
  if (value.empty()) return "null";
  const auto text = value + (utc ? "+00:00" : "");
  return "{\"key\": " + quoted(key) + ", \"kind\": \"date\", \"value\": " + quoted(text) + ", \"typed_value\": " + quoted(text) +
         (utc ? ", \"date_timezone\": \"UTC\"" : "") + "}";
}
std::string integer_cell(const std::string& key, int value) {
  return "{\"key\": " + quoted(key) + ", \"kind\": \"int\", \"value\": \"" + std::to_string(value) + "\", \"typed_value\": \"" +
         std::to_string(value) + "\", \"integer_storage\": \"int64\"}";
}
std::string boolean_cell(const std::string& key, bool value) {
  return "{\"key\": " + quoted(key) + ", \"kind\": \"boolean\", \"value\": \"" + (value ? "true" : "false") + "\", \"typed_value\": " +
         (value ? "true" : "false") + "}";
}
std::string float_cell(const std::string& key, const std::string& value) {
  if (value.empty()) return "null";
  return "{\"key\": " + quoted(key) + ", \"kind\": \"float\", \"value\": " + quoted(value) + ", \"typed_value\": " + value + "}";
}

EventTable frame(const std::vector<std::pair<std::string, std::string>>& columns, const std::vector<std::vector<std::string>>& rows) {
  std::ostringstream out;
  out << "{\"columns\": [";
  for (std::size_t i = 0; i < columns.size(); ++i) out << (i ? ", " : "") << quoted(columns[i].first);
  out << "], \"dtypes\": [";
  for (std::size_t i = 0; i < columns.size(); ++i) out << (i ? ", " : "") << quoted(columns[i].second);
  out << "], \"rows\": [";
  for (std::size_t r = 0; r < rows.size(); ++r) {
    out << (r ? ", " : "") << "[";
    for (std::size_t i = 0; i < rows[r].size(); ++i) out << (i ? ", " : "") << rows[r][i];
    out << "]";
  }
  out << "]}";
  return parse_event_table_json(out.str());
}

std::string column_text(const EventTable& result, const std::string& column) {
  const auto at = std::find(result.columns.begin(), result.columns.end(), column) - result.columns.begin();
  std::string out;
  for (const auto& row : result.rows) out += (out.empty() ? "" : " ") + (row[at] ? row[at]->value : std::string("None"));
  return out;
}
std::string eids(const EventTable& result) { return column_text(result, "eid"); }

EventAttribute text(std::string value) { return EventAttribute{"", std::move(value)}; }
EventAttribute integer(long long value) {
  EventAttribute out{"", std::to_string(value), AttributeKind::kInt};
  out.typed_value = static_cast<std::int64_t>(value);
  return out;
}
EventAttribute number(double value) {
  std::ostringstream text_value;
  text_value << value;
  EventAttribute out{"", text_value.str(), AttributeKind::kFloat};
  out.typed_value = value;
  return out;
}
EventAttribute collection(AttributeContainerKind kind, std::vector<EventAttribute> children) {
  EventAttribute out{"", ""};
  out.container = kind;
  out.children = std::move(children);
  return out;
}
EventAttribute path(const std::string& a, const std::string& b) { return collection(AttributeContainerKind::kTuple, {text(a), text(b)}); }
EventAttribute paths(const std::string& a, const std::string& b) { return collection(AttributeContainerKind::kList, {path(a, b)}); }

const std::string kCase = "case:concept:name", kActivity = "concept:name", kTime = "time:timestamp";

// c1: A, B; c2: A, hourly UTC (ns); i int64, b bool, dz datetime64[us, UTC]
// with a NaT.
EventTable first_frame(bool empty = false) {
  const std::vector<std::string> cases{"c1", "c1", "c2"}, activities{"A", "B", "A"}, dz{"2024-01-01T00:00:00", "", "2024-01-01T01:00:00"};
  const int numbers[] = {1, 2, 1};
  const bool flags[] = {true, false, true};
  std::vector<std::vector<std::string>> rows;
  for (std::size_t i = 0; i < 3 && !empty; ++i)
    rows.push_back({string_cell(kCase, cases[i]), string_cell(kActivity, activities[i]),
                    date_cell(kTime, "2024-01-01T0" + std::to_string(i) + ":00:00", true), integer_cell("i", numbers[i]),
                    boolean_cell("b", flags[i]), date_cell("dz", dz[i], true), string_cell("eid", "e" + std::to_string(i))});
  return frame({{kCase, "object"}, {kActivity, "object"}, {kTime, "datetime64[ns, UTC]"}, {"i", "int64"}, {"b", "bool"},
                {"dz", "datetime64[us, UTC]"}, {"eid", "object"}},
               rows);
}

std::string ufunc(const std::string& dtype) {
  return "ufunc 'add' did not contain a loop with signature matching types (dtype('" + dtype + "'), dtype('<U1')) -> None";
}

void pandas_vectorised_errors() {
  const auto f1 = first_frame();
  PandasPathsFilterOptions options;
  options.attribute_key = text("i");
  check_error<PandasPathsFilterError>("pandas paths on int64", [&] { (void)filter_pandas_paths(f1, paths("1", "2"), options); }, "UFuncTypeError",
                                      ufunc("int64"));
  check_error<PandasPathsFilterError>(
      "pandas paths on an empty int64 frame", [&] { (void)filter_pandas_paths(first_frame(true), paths("1", "2"), options); }, "UFuncTypeError",
      ufunc("int64"));
  options.attribute_key = text("b");
  check_error<PandasPathsFilterError>(
      "pandas path performance on bool", [&] { (void)filter_pandas_path_performance(f1, path("True", "False"), options); }, "UFuncTypeError",
      ufunc("bool"));
  options.attribute_key = text("dz");
  check_error<PandasPathsFilterError>("pandas paths on datetime", [&] { (void)filter_pandas_paths(f1, paths("x", "y"), options); }, "TypeError",
                                      "unsupported operand type(s) for +: 'DatetimeArray' and 'str'");
  const std::string unlabeled = "Can only compare identically-labeled (both index and columns) DataFrame objects";
  options.attribute_key = text(kActivity);
  options.case_id_key = text(kActivity);
  check_error<PandasPathsFilterError>(
      "pandas path performance, case key on the activity", [&] { (void)filter_pandas_path_performance(f1, path("A", "B"), options); },
      "ValueError", unlabeled);
  options.case_id_key = text(kTime);
  check_error<PandasPathsFilterError>(
      "pandas path performance, case key on the timestamp", [&] { (void)filter_pandas_path_performance(f1, path("A", "B"), options); },
      "ValueError", unlabeled);
  PandasCaseFilterOptions case_options;
  case_options.case_id_key = text(kTime);
  check_error<PandasCaseFilterError>(
      "pandas case performance, case key on the timestamp",
      [&] { (void)filter_pandas_case_performance(f1, integer(0), integer(100000), case_options); }, "ValueError",
      "Grouper for 'time:timestamp' not 1-dimensional");
}

void pandas_datetime_targets() {
  const auto f1 = first_frame();
  const auto lo = text("2023-12-31 00:00:00"), hi = text("2024-01-02 00:00:00");
  check("pandas timestamp attribute, aware column vs offset text",
        eids(filter_pandas_timestamp_attribute(f1, text("dz"), text("2024-01-01 03:00:00+02:00"), lo, hi)), "e2");
  check("pandas timestamp attribute, aware column vs naive text",
        eids(filter_pandas_timestamp_attribute(f1, text("dz"), text("2024-01-01 00:00:00"), lo, hi)), "e0 e1");
  check("pandas timestamp attribute, NaT text", eids(filter_pandas_timestamp_attribute(f1, text("dz"), text("NaT"), lo, hi)), "");
  PandasAttributeRepetitionOptions options;
  options.attribute_key = std::string("dz");
  options.minimum = TokenInteger(1);
  options.maximum = TokenInteger(5);
  check("pandas repetition of NaT on a datetime column", eids(filter_pandas_attribute_value_repetition(f1, text("NaT"), options)), "");
  check("pandas repetition of nan on a datetime column", eids(filter_pandas_attribute_value_repetition(f1, text("nan"), options)), "");
}

// cc / act / hourly naive time:timestamp / eid; dtypes given.
EventTable keyed(const std::string& case_dtype, const std::vector<std::string>& cases, const std::string& activity_dtype,
                 const std::vector<std::string>& activities, const std::string& activity_name = "act") {
  std::vector<std::vector<std::string>> rows;
  for (std::size_t i = 0; i < cases.size(); ++i)
    rows.push_back({cases[i], activities[i], date_cell(kTime, "2024-01-01T" + std::string(i < 10 ? "0" : "") + std::to_string(i) + ":00:00", false),
                    string_cell("eid", "e" + std::to_string(i))});
  return frame({{"cc", case_dtype}, {activity_name, activity_dtype}, {kTime, "datetime64[ns]"}, {"eid", "object"}}, rows);
}
std::vector<std::string> strings(const std::string& key, const std::vector<std::string>& values) {
  std::vector<std::string> out;
  for (const auto& value : values) out.push_back(value.empty() ? "null" : string_cell(key, value));
  return out;
}
std::vector<std::string> dates(const std::string& key, const std::vector<std::string>& values, bool utc) {
  std::vector<std::string> out;
  for (const auto& value : values) out.push_back(date_cell(key, value.empty() ? "" : value + "T00:00:00", utc));
  return out;
}
std::vector<std::string> floats(const std::string& key, const std::vector<std::string>& values) {
  std::vector<std::string> out;
  for (const auto& value : values) out.push_back(float_cell(key, value));
  return out;
}

void pandas_missing_case_values() {
  PandasVariantsFilterOptions options;
  options.case_id_key = text("cc");
  options.activity_key = text("act");
  const auto f2 = keyed("object", strings("cc", {"c1", "c1", "c2", "c2"}), "datetime64[us]",
                        dates("act", {"", "2024-01-03", "2024-01-01", "2024-01-02"}, false));
  check("pandas top_k 1, naive NaT activity", eids(filter_pandas_variants_top_k(f2, integer(1), options)), "");
  check("pandas top_k 2, naive NaT activity", eids(filter_pandas_variants_top_k(f2, integer(2), options)), "e2 e3");
  for (const bool utc : {false, true}) {
    const std::string zone = utc ? "aware" : "naive", dtype = utc ? "datetime64[us, UTC]" : "datetime64[us]";
    const auto f3 = keyed("object", strings("cc", {"c1", "c1", "c2", "c2", "c3"}), dtype,
                          dates("act", {"2024-01-01", "", "2024-01-01", "", "2024-01-02"}, utc));
    check("pandas top_k 1, " + zone + " NaT activity twice", eids(filter_pandas_variants_top_k(f3, integer(1), options)),
          utc ? "e0 e1 e2 e3" : "e4");
    const auto f4 = keyed(dtype, dates("cc", {"2024-01-01", "2024-01-01", "", "", "2024-01-02"}, utc), "object",
                          strings("act", {"a", "b", "a", "b", "c"}));
    check("pandas top_k 1, " + zone + " NaT cases", eids(filter_pandas_variants_top_k(f4, integer(1), options)), utc ? "e4" : "e0 e1");
    check("pandas min coverage 0.5, " + zone + " NaT cases", eids(filter_pandas_variants_min_coverage(f4, number(0.5), options)), "e0 e1 e4");
    check("pandas min coverage 0.6, " + zone + " NaT cases", eids(filter_pandas_variants_min_coverage(f4, number(0.6), options)),
          utc ? "" : "e0 e1");
  }
  const auto f5 = keyed("object", strings("cc", {"x", "x", "", "", "y"}), "object", strings("act", {"a", "b", "a", "b", "c"}));
  check_error<PandasVariantsFilterError>("pandas top_k, object cases with None", [&] { (void)filter_pandas_variants_top_k(f5, integer(1), options); },
                                         "TypeError", "'<' not supported between instances of 'str' and 'NoneType'");
  const auto f6 = keyed("float64", floats("cc", {"1.0", "1.0", "", "2.5"}), "datetime64[us]",
                        dates("act", {"2024-01-01", "2024-01-02", "2024-01-03", "2024-01-04"}, false));
  check("pandas top_k 2, float cases with NaN", eids(filter_pandas_variants_top_k(f6, integer(2), options)), "e3");
  PandasBetweenOptions between;
  between.case_id_key = text("cc");
  for (const bool utc : {false, true}) {
    const auto f7 = keyed(utc ? "datetime64[us, UTC]" : "datetime64[us]",
                          dates("cc", {"2024-01-01", "", "2024-01-01", "", "2024-01-02", "2024-01-02"}, utc), "object",
                          strings(kActivity, {"A", "B", "A", "B", "A", "B"}), kActivity);
    const auto result = filter_pandas_between(f7, text("A"), text("B"), between);
    check(std::string("pandas between, ") + (utc ? "aware" : "naive") + " NaT cases", eids(result) + " | " + column_text(result, "cc"),
          utc ? "e4 e5 | 2024-01-02 00:00:00+00:00##@@0 2024-01-02 00:00:00+00:00##@@0"
              : "e0 e1 e4 e5 | 2024-01-01T00:00:00.000000##@@0 2024-01-01T00:00:00.000000##@@0 2024-01-02T00:00:00.000000##@@0 "
                "2024-01-02T00:00:00.000000##@@0");
  }
}

std::string relation_text(const Ocel& log) {
  std::string out;
  for (const auto& relation : log.relations)
    out += (out.empty() ? "" : " ") + relation.event_id + ":" + relation.object_id + (relation.qualifier.empty() ? "" : "=" + relation.qualifier);
  return out;
}
std::string df_edges(const Ocel& log) {
  OcelHeterogeneousGraphOptions options;
  options.include_object_changes = false;
  std::vector<std::string> edges;
  for (const auto& edge : discover_ocel_heterogeneous_graph(log, options).edges)
    if (edge.type == "DF") edges.push_back(edge.source + ">" + edge.target + "@" + edge.object);
  std::sort(edges.begin(), edges.end());
  std::string out;
  for (const auto& edge : edges) out += (out.empty() ? "" : " ") + edge;
  return out;
}
std::string change_nodes(const Ocel& log) {
  std::string out;
  for (const auto& node : discover_ocel_heterogeneous_graph(log).nodes)
    if (node.type == "CHANGE") out += (out.empty() ? "" : " ") + node.id + "=" + node.object_id + "@" + node.timestamp.substr(17, 2);
  return out;
}
Ocel lifecycle(const Ocel& log) {
  OcelEnrichmentOptions options;
  options.include_interaction = options.include_descendants = options.include_inheritance = false;
  options.include_cobirth = options.include_codeath = false;
  return enrich_ocel(log, options);
}
Ocel transported(const Ocel& log) { return parse_ocel_json(serialize_ocel_json(log, OcelJsonFormat::kStandard)); }

void ocel_split_and_transport_order() {
  Ocel log;
  log.events = {{"e0", "a", "2030-01-01T00:00:00Z", {}}, {"e1", "b", "2030-01-01T00:00:01Z", {}}};
  log.objects = {{"o0", "t", {{"v", 1.0}}}, {"o1", "t", {{"v", 2.0}}}};
  log.relations = {{"e0", "o0", "q"}, {"e1", "o0", "q"}, {"e1", "o1", "q"}};
  log.o2o = {{"o0", "o1", "p"}};
  log.object_changes = {{"o1", "t", "2030-01-01T00:00:03Z", "v", 4.0}};
  OcelSplitOptions split;
  split.variant = OcelSplitVariant::kConnectedComponents;
  const auto components = split_ocel(log, split);
  check("OCEL connected-component split drops O2O rows and changes",
        std::to_string(components.logs.size()) + " " + std::to_string(components.logs[0].o2o.size()) + " " +
            std::to_string(components.logs[0].object_changes.size()),
        "1 0 0");

  // Relations unlike their events at one instant (as an OCEL SQLite file
  // gives them): rows x2, x1, x0 with x0 and x1 tied.
  Ocel tied;
  tied.events = {{"x0", "a", "2030-01-01T00:00:00Z", {}}, {"x1", "a", "2030-01-01T00:00:00Z", {}}, {"x2", "a", "2030-01-01T00:00:01Z", {}}};
  tied.objects = {{"o0", "t", {}}};
  tied.relations = {{"x1", "o0", "q"}, {"x0", "o0", "q"}, {"x2", "o0", "q"}};
  const auto back = transported(tied);
  check("OCEL 2.0 transport keeps the relation order", relation_text(back), "x1:o0=q x0:o0=q x2:o0=q");
  check("OCEL lifecycle qualifiers in relation order", relation_text(lifecycle(back)), "x1:o0=creation x0:o0=other x2:o0=termination");
  check("OCEL event/object graph DF in relation order", df_edges(back), "x0>x2@o0 x1>x0@o0");
  check("OCEL classic transport keeps the relation order", relation_text(parse_ocel_json(serialize_ocel_json(tied))), "x1:o0=q x0:o0=q x2:o0=q");

  // Object changes in time order across objects (as an OCEL SQLite file
  // gives them), which the OCEL 2.0 form nests per object.
  Ocel changed;
  changed.events = {{"e0", "a", "2030-01-01T00:00:00Z", {}}};
  changed.objects = {{"o0", "t", {{"v", 0.0}}}, {"o1", "t", {{"v", 0.0}}}};
  changed.relations = {{"e0", "o0", "q"}, {"e0", "o1", "q"}};
  changed.object_changes = {{"o1", "t", "2030-01-01 00:00:01+00:00", "v", 1.0},
                            {"o0", "t", "2030-01-01 00:00:02+00:00", "v", 2.0},
                            {"o1", "t", "2030-01-01 00:00:03+00:00", "v", 3.0}};
  check("OCEL 2.0 transport keeps the object change order", change_nodes(transported(changed)),
        "@@change##0=o1@01 @@change##1=o0@02 @@change##2=o1@03");
}

#if defined(MINERA_HAS_SQLITE)
void exec_sql(sqlite3* db, const std::string& sql) {
  char* error = nullptr;
  if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &error) != SQLITE_OK) {
    ++failures;
    std::cerr << "SQLite setup failed: " << (error ? error : "") << "\n";
    if (error) sqlite3_free(error);
  }
}

// One OCEL 2.0 SQLite file: events a/b at one instant whose event_object
// rows list e1 first, and object changes in two type tables (pm4py reads
// them by row place, then type, sorted by time).
void ocel_sqlite_row_order() {
  const auto path = std::filesystem::temp_directory_path() / "minera_review_by1.sqlite";
  std::filesystem::remove(path);
  sqlite3* db = nullptr;
  if (sqlite3_open(path.string().c_str(), &db) != SQLITE_OK) {
    ++failures;
    std::cerr << "cannot create " << path << "\n";
    return;
  }
  exec_sql(db,
           "CREATE TABLE event (ocel_id TEXT, ocel_type TEXT);"
           "CREATE TABLE object (ocel_id TEXT, ocel_type TEXT);"
           "CREATE TABLE event_map_type (ocel_type TEXT, ocel_type_map TEXT);"
           "CREATE TABLE object_map_type (ocel_type TEXT, ocel_type_map TEXT);"
           "CREATE TABLE event_object (ocel_event_id TEXT, ocel_object_id TEXT, ocel_qualifier TEXT);"
           "CREATE TABLE object_object (ocel_source_id TEXT, ocel_target_id TEXT, ocel_qualifier TEXT);"
           "INSERT INTO event_map_type VALUES ('a','A'),('b','B');"
           "CREATE TABLE event_A (ocel_id TEXT, ocel_time TIMESTAMP);"
           "CREATE TABLE event_B (ocel_id TEXT, ocel_time TIMESTAMP);"
           "INSERT INTO event VALUES ('e0','a'),('e1','b'),('e2','a');"
           "INSERT INTO event_A VALUES ('e0','2030-01-01 00:00:00+00:00'),('e2','2030-01-01 00:00:09+00:00');"
           "INSERT INTO event_B VALUES ('e1','2030-01-01 00:00:00+00:00');"
           "INSERT INTO event_object VALUES ('e1','o0','q'),('e0','o0','q'),('e2','o1','q'),('e2','o2','q');"
           "INSERT INTO object_map_type VALUES ('t0','T0'),('t1','T1');"
           "CREATE TABLE object_T0 (ocel_id TEXT, ocel_time TIMESTAMP, ocel_changed_field TEXT, v REAL);"
           "CREATE TABLE object_T1 (ocel_id TEXT, ocel_time TIMESTAMP, ocel_changed_field TEXT, v REAL);"
           "INSERT INTO object VALUES ('o0','t0'),('o1','t0'),('o2','t1');"
           "INSERT INTO object_T0 VALUES ('o1','1970-01-01 00:00:00',NULL,0),('o0','1970-01-01 00:00:00',NULL,0),"
           "('o1','2030-01-01 00:00:05+00:00','v',1),('o0','2030-01-01 00:00:03+00:00','v',2),('o1','2030-01-01 00:00:03+00:00','v',3);"
           "INSERT INTO object_T1 VALUES ('o2','1970-01-01 00:00:00',NULL,0),('o2','2030-01-01 00:00:03+00:00','v',4);");
  sqlite3_close(db);
  const auto log = import_ocel_sqlite(path);
  std::filesystem::remove(path);
  check("OCEL SQLite relations in row order by time", relation_text(log), "e1:o0=q e0:o0=q e2:o1=q e2:o2=q");
  check("OCEL SQLite lifecycle through the OCEL 2.0 transport", relation_text(lifecycle(transported(log))),
        "e1:o0=creation e0:o0=termination e2:o1=creation e2:o2=creation");
  check("OCEL SQLite object changes by row place, type and time", change_nodes(transported(log)),
        "@@change##0=o2@03 @@change##1=o0@03 @@change##2=o1@03 @@change##3=o1@05");
}
#endif

}  // namespace

int main() {
  pandas_vectorised_errors();
  pandas_datetime_targets();
  pandas_missing_case_values();
  ocel_split_and_transport_order();
#if defined(MINERA_HAS_SQLITE)
  ocel_sqlite_row_order();
#endif
  if (failures) {
    std::cerr << failures << " review BY1 check(s) failed\n";
    return 1;
  }
  std::cout << "review BY1 checks passed\n";
  return 0;
}
