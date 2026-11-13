// Review BZ1: pandas 3.0.5 hands columns to NumPy through to_numpy(), so an
// Int64 column with a gap is a float array holding NaN (the variants
// statistics and the between filter read it so) and a categorical gap is a
// NaN object; Series.isin on a masked numeric column never matches <NA>
// (attributes and timestamp-attribute case selection). The pandas LTL
// filters fail on duplicated role keys in pandas' order, and the paths
// filter refuses interval and Sparse activity columns with the vectorised
// error. pm4py's OCEL 2.0 SQLite reader keeps repeated event_object rows
// (the transport carries them with "minera:repeatedRelations") and the
// object_object row order ("minera:o2oOrder"); the lifecycle qualifiers
// label an event "other" when one of its repeats sits inside the object's
// rows, OCDFG takes start and end activities from the relation rows of a
// tied log, and the OCPN per-type log keeps the repeats.
// Values are taken from pm4py 2.7.22 black-box on the same input
// (review-harness round-bz, probes/bz1.py, bz1ocel.py, bz1ocel2.py).
#include "minera/dataframe_format_json.hpp"
#include "minera/ocel.hpp"
#include "minera/ocel_transform.hpp"
#include "minera/pandas_attributes_filter.hpp"
#include "minera/pandas_between_filter.hpp"
#include "minera/pandas_ltl_filter.hpp"
#include "minera/pandas_paths_filter.hpp"
#include "minera/pandas_timestamp_filter.hpp"
#include "minera/pandas_variants_filter.hpp"

#include <algorithm>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

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
std::string na_cell(const std::string& key) {
  return "{\"key\": " + quoted(key) + ", \"kind\": \"na\", \"value\": \"<NA>\", \"typed_value\": \"NA\"}";
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
  return "{\"key\": " + quoted(key) + ", \"kind\": \"float\", \"value\": " + quoted(value) + ", \"typed_value\": " + value + "}";
}
std::string date_cell(const std::string& key, const std::string& text) {
  return "{\"key\": " + quoted(key) + ", \"kind\": \"date\", \"value\": " + quoted(text) + ", \"typed_value\": " + quoted(text) + "}";
}

const std::string kActivity = "concept:name", kTime = "time:timestamp";

// Six events e0..e5 on 2024-01-01..06 (UTC) with the case cells given and
// activities a b a c b a unless replaced; `case_dtype` names the case column.
EventTable frame(const std::vector<std::string>& cases, const std::string& case_dtype, const std::string& categories = "",
                 std::vector<std::string> activities = {}, const std::string& activity_dtype = "object") {
  if (activities.empty())
    for (const auto* a : {"a", "b", "a", "c", "b", "a"}) activities.push_back(string_cell(kActivity, a));
  std::ostringstream out;
  out << "{\"columns\": [\"case\", \"concept:name\", \"eid\", \"time:timestamp\"], \"dtypes\": [" << quoted(case_dtype) << ", "
      << quoted(activity_dtype) << ", \"object\", \"datetime64[us, UTC]\"], \"rows\": [";
  for (std::size_t i = 0; i < 6; ++i)
    out << (i ? ", " : "") << "[" << cases[i] << ", " << activities[i] << ", " << string_cell("eid", "e" + std::to_string(i)) << ", "
        << date_cell(kTime, "2024-01-0" + std::to_string(i + 1) + "T00:00:00+00:00") << "]";
  out << "]";
  if (!categories.empty()) out << ", \"categorical_dtypes\": [" << categories << ", null, null, null]";
  out << "}";
  return parse_event_table_json(out.str());
}

std::string column_text(const EventTable& result, const std::string& column) {
  const auto at = std::find(result.columns.begin(), result.columns.end(), column) - result.columns.begin();
  std::string out;
  for (const auto& row : result.rows) out += (out.empty() ? "" : " ") + (row[at] ? row[at]->value : std::string("None"));
  return out.empty() ? "(empty)" : out;
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
EventAttribute boolean(bool value) {
  EventAttribute out{"", value ? "true" : "false", AttributeKind::kBoolean};
  out.typed_value = value;
  return out;
}
EventAttribute collection(AttributeContainerKind kind, std::vector<EventAttribute> children) {
  EventAttribute out{"", ""};
  out.container = kind;
  out.children = std::move(children);
  return out;
}
EventAttribute list(std::vector<EventAttribute> children) { return collection(AttributeContainerKind::kList, std::move(children)); }

std::vector<std::string> int64_cases() {
  return {integer_cell("case", 1), integer_cell("case", 1), na_cell("case"), na_cell("case"), integer_cell("case", 2), integer_cell("case", 2)};
}

void pandas_numpy_views() {
  const auto ints = frame(int64_cases(), "Int64");
  PandasVariantsFilterOptions variants;
  variants.case_id_key = text("case");
  check("pandas top_k, Int64 case with <NA>", eids(filter_pandas_variants_top_k(ints, integer(1), variants)), "e4 e5");
  check("pandas min coverage, Int64 case with <NA>", eids(filter_pandas_variants_min_coverage(ints, number(0.3), variants)), "e0 e1 e4 e5");

  const auto flags = frame({boolean_cell("case", true), boolean_cell("case", true), na_cell("case"), na_cell("case"), boolean_cell("case", false),
                            boolean_cell("case", false)},
                           "boolean");
  check_error<PandasVariantsFilterError>("pandas top_k, boolean case with <NA>", [&] { (void)filter_pandas_variants_top_k(flags, integer(1), variants); },
                                         "TypeError", "boolean value of NA is ambiguous");
  const std::string object_categories =
      "{\"categories_dtype\": \"object\", \"ordered\": false, \"categories\": [{\"key\": \"case\", \"value\": \"x\"}, {\"key\": \"case\", \"value\": \"y\"}]}";
  const auto categorical = frame({string_cell("case", "x"), string_cell("case", "x"), "null", "null", string_cell("case", "y"), string_cell("case", "y")},
                                 "category", object_categories);
  check_error<PandasVariantsFilterError>("pandas top_k, categorical case with NaN",
                                         [&] { (void)filter_pandas_variants_top_k(categorical, integer(1), variants); }, "TypeError",
                                         "'<' not supported between instances of 'str' and 'float'");
  PandasBetweenOptions between;
  between.case_id_key = text("case");
  check_error<PandasBetweenError>("pandas between, categorical case with NaN", [&] { (void)filter_pandas_between(categorical, text("a"), text("b"), between); },
                                  "TypeError", "'<' not supported between instances of 'str' and 'float'");

  // Activities 1 <NA> 2 | 1 2 2 (Int64): floats in NumPy, the gap a fresh NaN.
  const std::vector<std::string> two_cases{string_cell("case", "c1"), string_cell("case", "c1"), string_cell("case", "c1"),
                                           string_cell("case", "c2"), string_cell("case", "c2"), string_cell("case", "c2")};
  const auto int_activities = frame(two_cases, "object", "",
                                    {integer_cell(kActivity, 1), na_cell(kActivity), integer_cell(kActivity, 2), integer_cell(kActivity, 1),
                                     integer_cell(kActivity, 2), integer_cell(kActivity, 2)},
                                    "Int64");
  const auto between_result = filter_pandas_between(int_activities, list({integer(1)}), list({integer(2)}), between);
  check("pandas between, Int64 activity with <NA>", eids(between_result) + " | " + column_text(between_result, "case"),
        "e0 e1 e2 e3 e4 | c1##@@0 c1##@@0 c1##@@0 c2##@@0 c2##@@0");
  check("pandas top_k, Int64 activity with <NA>", eids(filter_pandas_variants_top_k(int_activities, integer(1), variants)), "(empty)");
  const std::vector<std::string> three_cases{string_cell("case", "c1"), string_cell("case", "c1"), string_cell("case", "c2"),
                                             string_cell("case", "c2"), string_cell("case", "c3"), string_cell("case", "c3")};
  const auto gap = frame(three_cases, "object", "",
                         {integer_cell(kActivity, 1), integer_cell(kActivity, 2), integer_cell(kActivity, 1), integer_cell(kActivity, 2),
                          integer_cell(kActivity, 1), na_cell(kActivity)},
                         "Int64");
  check("pandas top_k 2, Int64 activity gap", eids(filter_pandas_variants_top_k(gap, integer(2), variants)), "e0 e1 e2 e3");

  // Int64 case a b | <NA>: a b | 2: a b -> NaN groups once and prints "nan".
  const auto int_between = frame(int64_cases(), "Int64", "",
                                 {string_cell(kActivity, "a"), string_cell(kActivity, "b"), string_cell(kActivity, "a"), string_cell(kActivity, "b"),
                                  string_cell(kActivity, "a"), string_cell(kActivity, "b")});
  check("pandas between, Int64 case with <NA>", column_text(filter_pandas_between(int_between, text("a"), text("b"), between), "case"),
        "1.0##@@0 1.0##@@0 nan##@@0 nan##@@0 2.0##@@0 2.0##@@0");
}

void pandas_masked_isin() {
  const auto ints = frame(int64_cases(), "Int64");
  PandasAttributesFilterOptions attributes;
  attributes.case_id_key = text("case");
  attributes.positive = boolean(false);
  check("pandas attributes negative, Int64 case with <NA>", eids(filter_pandas_attributes(ints, list({text("a")}), attributes)), "e2 e3");
  attributes.positive = boolean(true);
  check("pandas attributes positive, Int64 case with <NA>", eids(filter_pandas_attributes(ints, list({text("c")}), attributes)), "(empty)");
  const auto flags = frame({boolean_cell("case", true), boolean_cell("case", true), na_cell("case"), na_cell("case"), boolean_cell("case", false),
                            boolean_cell("case", false)},
                           "boolean");
  check("pandas attributes positive, boolean case with <NA>", eids(filter_pandas_attributes(flags, list({text("c")}), attributes)), "e2 e3");
  PandasTimestampFilterOptions timestamp;
  timestamp.case_id_key = text("case");
  const auto lo = text("2023-01-01 00:00:00"), hi = text("2025-01-01 00:00:00");
  check("pandas timestamp attribute, Int64 case with <NA> (c)",
        eids(filter_pandas_timestamp_attribute(ints, text(kActivity), text("c"), lo, hi, timestamp)), "(empty)");
  check("pandas timestamp attribute, Int64 case with <NA> (a)",
        eids(filter_pandas_timestamp_attribute(ints, text(kActivity), text("a"), lo, hi, timestamp)), "e0 e1 e4 e5");
}

void pandas_paths_dtypes() {
  const std::vector<std::string> one_case(6, string_cell("case", "c1"));
  PandasPathsFilterOptions paths;
  paths.case_id_key = text("case");
  const auto pair = collection(AttributeContainerKind::kList, {collection(AttributeContainerKind::kTuple, {text("A"), text("B")})});
  const auto sparse = frame(one_case, "object", "",
                            {float_cell(kActivity, "1.5"), "null", float_cell(kActivity, "2.0"), float_cell(kActivity, "1.5"), float_cell(kActivity, "2.0"),
                             float_cell(kActivity, "2.0")},
                            "Sparse[float64, nan]");
  check_error<PandasPathsFilterError>("pandas paths on Sparse[float64, nan]", [&] { (void)filter_pandas_paths(sparse, pair, paths); }, "UFuncTypeError",
                                      "ufunc 'add' did not contain a loop with signature matching types (dtype('float64'), dtype('<U1')) -> None");
  const auto interval = frame(one_case, "object", "", {}, "interval[int64, right]");
  check_error<PandasPathsFilterError>("pandas paths on interval[int64, right]", [&] { (void)filter_pandas_paths(interval, pair, paths); }, "TypeError",
                                      "unsupported operand type(s) for +: 'IntervalArray' and 'str'");
}

void pandas_ltl_duplicate_keys() {
  const auto source = parse_event_table_json(
      "{\"columns\": [\"c\", \"a\", \"t\", \"r\"], \"dtypes\": [\"object\", \"object\", \"datetime64[ns]\", \"object\"], \"rows\": [" +
      std::string("[") + string_cell("c", "1") + ", " + string_cell("a", "A") + ", " + date_cell("t", "2024-01-01T00:00:00") + ", " + string_cell("r", "x") + "], [" +
      string_cell("c", "1") + ", " + string_cell("a", "B") + ", " + date_cell("t", "2024-01-02T00:00:00") + ", " + string_cell("r", "y") + "], [" +
      string_cell("c", "2") + ", " + string_cell("a", "A") + ", " + date_cell("t", "2024-01-03T00:00:00") + ", " + string_cell("r", "x") + "]]}");
  const std::string cannot_set = "Cannot set a DataFrame with multiple columns to the single column @@conceptname";
  PandasLtlFilterOptions options;
  options.case_id_key = options.attribute_key = options.resource_key = text("c");
  check_error<PandasLtlFilterError>("pandas four eyes, one column for all keys", [&] { (void)filter_pandas_ltl_four_eyes(source, text("A"), text("B"), options); },
                                    "ValueError", cannot_set);
  options.case_id_key = options.attribute_key = options.resource_key = text("r");
  check_error<PandasLtlFilterError>("pandas different persons, one column for all keys",
                                    [&] { (void)filter_pandas_ltl_different_persons(source, text("A"), options); }, "ValueError", cannot_set);
  PandasLtlFilterOptions timed;
  timed.enable_timestamp = boolean(true);
  timed.timestamp_diff_boundaries = list({collection(AttributeContainerKind::kTuple, {integer(0), integer(1000000000)})});
  timed.case_id_key = text("c");
  timed.attribute_key = text("a");
  timed.timestamp_key = text("c");
  check_error<PandasLtlFilterError>("pandas eventually, timestamp key on the case column",
                                    [&] { (void)filter_pandas_ltl_eventually_follows(source, list({text("A"), text("B")}), timed); }, "ValueError",
                                    "The column label 'c' is not unique.");
  timed.case_id_key = text("r");
  timed.timestamp_key = text("a");
  check_error<PandasLtlFilterError>("pandas eventually, timestamp key on the activity column",
                                    [&] { (void)filter_pandas_ltl_eventually_follows(source, list({text("A"), text("B")}), timed); }, "ValueError",
                                    "cannot reindex on an axis with duplicate labels");
}

std::string relations(const Ocel& log) {
  std::string out;
  for (const auto& relation : log.relations) out += (out.empty() ? "" : " ") + relation.event_id + ":" + relation.qualifier;
  return out;
}

// OCEL 2.0 JSON as Minera's /api/ocel/sqlite import writes it for the SQLite
// files of probes/bz1ocel.py and bz1ocel2.py.
const char* kRepeatedF =
    R"({"ocelVersion":"2.0","objectTypes":[{"name":"t","attributes":[]}],"eventTypes":[{"name":"a","attributes":[]}],"events":[{"id":"e0","type":"a","time":"2024-01-01 00:00:00+00:00","attributes":[],"relationships":[{"objectId":"o0","qualifier":"q"}]},{"id":"e1","type":"a","time":"2024-01-01 00:00:01+00:00","attributes":[],"relationships":[{"objectId":"o0","qualifier":"q"},{"objectId":"o0","qualifier":"q"}]},{"id":"e2","type":"a","time":"2024-01-01 00:00:02+00:00","attributes":[],"relationships":[{"objectId":"o0","qualifier":"q"},{"objectId":"o0","qualifier":"q"}]}],"objects":[{"id":"o0","type":"t","attributes":[],"relationships":[]}],"minera:repeatedRelations":true})";
const char* kRepeatedH =
    R"({"ocelVersion":"2.0","objectTypes":[{"name":"t","attributes":[]}],"eventTypes":[{"name":"a","attributes":[]}],"events":[{"id":"e0","type":"a","time":"2024-01-01 00:00:00+00:00","attributes":[],"relationships":[{"objectId":"o0","qualifier":"q"}]},{"id":"e1","type":"a","time":"2024-01-01 00:00:00+00:00","attributes":[],"relationships":[{"objectId":"o0","qualifier":"q"},{"objectId":"o0","qualifier":"q"}]},{"id":"e2","type":"a","time":"2024-01-01 00:00:01+00:00","attributes":[],"relationships":[{"objectId":"o0","qualifier":"q"}]}],"objects":[{"id":"o0","type":"t","attributes":[],"relationships":[]}],"minera:relationOrder":["e1","e0","e1","e2"],"minera:repeatedRelations":true})";
const char* kTiedStart =
    R"({"ocelVersion":"2.0","objectTypes":[{"name":"t","attributes":[]}],"eventTypes":[{"name":"a","attributes":[]},{"name":"b","attributes":[]},{"name":"c","attributes":[]}],"events":[{"id":"e0","type":"a","time":"2024-01-01 00:00:00+00:00","attributes":[],"relationships":[{"objectId":"o0","qualifier":"q"}]},{"id":"e1","type":"b","time":"2024-01-01 00:00:00+00:00","attributes":[],"relationships":[{"objectId":"o0","qualifier":"q"}]},{"id":"e2","type":"c","time":"2024-01-01 00:00:01+00:00","attributes":[],"relationships":[{"objectId":"o0","qualifier":"q"},{"objectId":"o1","qualifier":"q"}]},{"id":"e3","type":"a","time":"2024-01-01 00:00:01+00:00","attributes":[],"relationships":[{"objectId":"o1","qualifier":"q"}]}],"objects":[{"id":"o0","type":"t","attributes":[],"relationships":[]},{"id":"o1","type":"t","attributes":[],"relationships":[]}],"minera:relationOrder":["e1","e0","e2","e3","e2"]})";
const char* kRepeatedB =
    R"({"ocelVersion":"2.0","objectTypes":[{"name":"t","attributes":[]}],"eventTypes":[{"name":"a","attributes":[]},{"name":"b","attributes":[]},{"name":"c","attributes":[]}],"events":[{"id":"e0","type":"a","time":"2024-01-01 00:00:00+00:00","attributes":[],"relationships":[{"objectId":"o0","qualifier":"q"}]},{"id":"e1","type":"b","time":"2024-01-01 00:00:01+00:00","attributes":[],"relationships":[{"objectId":"o0","qualifier":"q"},{"objectId":"o0","qualifier":"q"}]},{"id":"e2","type":"c","time":"2024-01-01 00:00:02+00:00","attributes":[],"relationships":[{"objectId":"o0","qualifier":"q"}]}],"objects":[{"id":"o0","type":"t","attributes":[],"relationships":[]}],"minera:repeatedRelations":true})";
const char* kO2oOrder =
    R"({"ocelVersion":"2.0","objectTypes":[{"name":"t","attributes":[]}],"eventTypes":[{"name":"a","attributes":[]}],"events":[{"id":"e0","type":"a","time":"2024-01-01 00:00:00+00:00","attributes":[],"relationships":[{"objectId":"o0","qualifier":"q"},{"objectId":"o1","qualifier":"q"}]}],"objects":[{"id":"o0","type":"t","attributes":[],"relationships":[{"objectId":"o1","qualifier":"s"}]},{"id":"o1","type":"t","attributes":[],"relationships":[{"objectId":"o0","qualifier":"r"},{"objectId":"o0","qualifier":"u"}]}],"minera:o2oOrder":["o1","o0","o1"]})";

std::string activity_keys(const std::map<std::string, OcdfgActivityMetric>& metrics) {
  std::string out;
  for (const auto& [activity, metric] : metrics) out += (out.empty() ? "" : " ") + activity;
  return out;
}

void ocel_repeated_relations() {
  OcelEnrichmentOptions lifecycle;
  lifecycle.include_interaction = lifecycle.include_descendants = lifecycle.include_inheritance = false;
  lifecycle.include_cobirth = lifecycle.include_codeath = false;
  const auto f = parse_ocel_json(kRepeatedF);
  check("OCEL repeated relations kept", std::to_string(f.relations.size()), "5");
  check("OCEL lifecycle, repeated inner rows", relations(enrich_ocel(f, lifecycle)), "e0:creation e1:other e1:other e2:other e2:other");
  const auto h = parse_ocel_json(kRepeatedH);
  check("OCEL lifecycle, repeat around a tie", relations(enrich_ocel(h, lifecycle)), "e1:other e0:other e1:other e2:termination");
  check("OCEL repeated relations round trip", serialize_ocel_json(h, OcelJsonFormat::kStandard), kRepeatedH);

  const auto tied = discover_ocdfg_classic(parse_ocel_json(kTiedStart));
  check("OCDFG start activities on a tied log", activity_keys(tied.start_activities.at("t")), "a b");
  check("OCDFG end activities on a tied log", activity_keys(tied.end_activities.at("t")), "c");
  std::string edges;
  for (const auto& [edge, metric] : tied.edges.at("t")) edges += (edges.empty() ? "" : " ") + edge.first + ">" + edge.second;
  check("OCDFG edges on a tied log", edges, "a>b b>c c>a");

  const auto ocpn = discover_ocpn(parse_ocel_json(kRepeatedB));
  const auto& net = ocpn.petri_nets.at("t");
  std::size_t silent = 0, arcs = 0;
  for (const auto& transition : net.transitions()) {
    silent += transition.activity ? 0 : 1;
    arcs += transition.inputs.size() + transition.outputs.size();
  }
  check("OCPN on a repeated relation (places transitions silent arcs)",
        std::to_string(net.place_count()) + " " + std::to_string(net.transitions().size()) + " " + std::to_string(silent) + " " + std::to_string(arcs),
        "4 4 1 8");

  const auto o2o = parse_ocel_json(kO2oOrder);
  std::string rows;
  for (const auto& row : o2o.o2o) rows += (rows.empty() ? "" : " ") + row.source_id + ">" + row.target_id + ":" + row.qualifier;
  check("OCEL O2O row order across sources", rows, "o1>o0:r o0>o1:s o1>o0:u");
  check("OCEL O2O order round trip", serialize_ocel_json(o2o, OcelJsonFormat::kStandard), kO2oOrder);
}

}  // namespace

int main() {
  pandas_numpy_views();
  pandas_masked_isin();
  pandas_paths_dtypes();
  pandas_ltl_duplicate_keys();
  ocel_repeated_relations();
  if (failures) {
    std::cerr << failures << " review BZ1 check(s) failed\n";
    return 1;
  }
  std::cout << "review BZ1 checks passed\n";
  return 0;
}
