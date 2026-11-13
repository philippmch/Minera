// Review CA1: pm4py keeps repeated event_object rows in the OCEL edge
// metrics (an object's events repeat in event order, so a repeat is a
// self-edge) and in the description ("related objects: o1, o1"). The Polars
// table filters take UInt8 (and every other integer width) case and
// activity keys, and the timestamp-attribute filter names the narrow dtype
// when it refuses a string value ("numeric type (u8)"). The Polars sequence
// filters on Categorical/Enum keys are covered by the typed bank
// (docs/polars-sequence-filter-typed-bank.json.gz); two of its rules are
// pinned here too: variant lists only join on a String activity column, and
// is_in on an Enum column fails on values outside its categories.
// Values are taken from pm4py 2.7.22 black-box on the same input
// (review-harness round-ca, ocel/ocelca.py, tbl/tblkeys.py, seq/).
#include "minera/dataframe_format_json.hpp"
#include "minera/ocel.hpp"
#include "minera/ocel_statistics.hpp"
#include "minera/polars_case_filter.hpp"
#include "minera/polars_sequence_filters.hpp"
#include "minera/polars_timestamp_filter.hpp"

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
std::string date_cell(const std::string& key, const std::string& value) {
  return "{\"key\": " + quoted(key) + ", \"kind\": \"date\", \"value\": " + quoted(value) + ", \"typed_value\": " + quoted(value) +
         ", \"date_timezone\": \"UTC\"}";
}
std::string integer_cell(const std::string& key, int value) {
  return "{\"key\": " + quoted(key) + ", \"kind\": \"int\", \"value\": \"" + std::to_string(value) + "\", \"typed_value\": " + std::to_string(value) + "}";
}

EventTable frame(const std::vector<std::pair<std::string, std::string>>& columns, const std::vector<std::vector<std::string>>& rows,
                 const std::string& extra = "") {
  std::ostringstream out;
  out << "{\"columns\": [";
  for (std::size_t i = 0; i < columns.size(); ++i) out << (i ? ", " : "") << quoted(columns[i].first);
  out << "], \"dtypes\": [";
  for (std::size_t i = 0; i < columns.size(); ++i) out << (i ? ", " : "") << quoted(columns[i].second);
  out << "], " << extra << "\"rows\": [";
  for (std::size_t r = 0; r < rows.size(); ++r) {
    out << (r ? ", " : "") << "[";
    for (std::size_t i = 0; i < rows[r].size(); ++i) out << (i ? ", " : "") << rows[r][i];
    out << "]";
  }
  out << "]}";
  return parse_event_table_json(out.str());
}

std::string eids(const EventTable& result) {
  const auto at = std::find(result.columns.begin(), result.columns.end(), "eid") - result.columns.begin();
  std::string out;
  for (const auto& row : result.rows) out += (out.empty() ? "" : " ") + row[at]->value;
  return out;
}

EventAttribute text(std::string value) { return EventAttribute{"", std::move(value)}; }
EventAttribute integer(long long value) {
  EventAttribute out{"", std::to_string(value), AttributeKind::kInt};
  out.typed_value = static_cast<std::int64_t>(value);
  return out;
}

const std::string kCase = "case:concept:name", kActivity = "concept:name", kTime = "time:timestamp";
const std::string kUsUtc = "Datetime(time_unit='us', time_zone='UTC')";

// Three events; o1 has two rows on e1 and two on e3.
Ocel repeated_log() {
  Ocel log;
  log.events = {{"e1", "a", "2024-01-01T00:00:00Z", {}}, {"e2", "b", "2024-01-01T01:00:00Z", {}}, {"e3", "b", "2024-01-01T03:00:00Z", {}}};
  log.objects = {{"o1", "t", {}}, {"o2", "t", {}}};
  log.relations = {{"e1", "o1", ""}, {"e1", "o1", ""}, {"e2", "o1", ""}, {"e2", "o2", ""}, {"e3", "o1", ""}, {"e3", "o1", ""}};
  return log;
}

void ocel_repeated_relations() {
  const auto edges = edge_associations(repeated_log());
  std::string seen;
  for (const auto& [type, metrics] : edges)
    for (const auto& [activities, metric] : metrics) {
      seen += type + " " + activities.first + "->" + activities.second + ":";
      for (const auto& [source, target, object] : metric.event_objects) seen += " " + source + "," + target + "," + object;
      seen += " |";
      for (const auto value : metric.performance_event_couples) seen += " " + std::to_string(static_cast<long long>(value));
      seen += "\n";
    }
  check("OCEL edge associations on repeated relation rows", seen,
        "t a->a: e1,e1,o1 | 0\nt a->b: e1,e2,o1 | 3600\nt b->b: e2,e3,o1 e3,e3,o1 | 0 7200\n");  // pm4py: a set of couples; compared sorted
  OcelDescriptionOptions options;
  options.include_timestamps = false;
  check("OCEL description on repeated relation rows", describe_ocel_text(repeated_log(), options),
        "\n\nevents:\n\na ( related objects: o1, o1 ) \nb ( related objects: o1, o2 ) \nb ( related objects: o1, o1 ) \n\nobjects:\n\no1 "
        "object type: t\no2 object type: t");
}

// Case key U (UInt8): cases 1 (e0, e1), 2 (e2), null (e3).
EventTable uint8_frame() {
  const std::vector<std::string> times{"2024-01-01T00:00:00+00:00", "2024-01-01T01:00:00+00:00", "2024-01-01T02:00:00+00:00",
                                       "2024-01-01T03:00:00+00:00"};
  std::vector<std::vector<std::string>> rows;
  const int cases[] = {1, 1, 2, 0};
  for (int i = 0; i < 4; ++i)
    rows.push_back({string_cell(kCase, "c"), string_cell(kActivity, i % 2 ? "B" : "A"), date_cell(kTime, times[i]),
                    i == 3 ? std::string("{\"key\": \"U\", \"null\": true}") : integer_cell("U", cases[i]),
                    string_cell("eid", "e" + std::to_string(i))});
  return frame({{kCase, "String"}, {kActivity, "String"}, {kTime, kUsUtc}, {"U", "UInt8"}, {"eid", "String"}}, rows);
}

void polars_uint8_keys() {
  const auto source = uint8_frame();
  PolarsCaseFilterOptions options;
  options.case_id_key = text("U");
  EventAttribute none;
  none.is_null = true;
  check("Polars case size on a UInt8 case key", eids(filter_polars_case_size(source, integer(2), none, options)), "e0 e1");
  const auto lo = text("2023-12-31 00:00:00"), hi = text("2024-01-02 00:00:00");
  check_error<PolarsTimestampFilterError>(
      "Polars timestamp attribute UInt8 vs string",
      [&] { (void)filter_polars_timestamp_attribute(source, text("U"), text("1"), lo, hi); }, "ComputeError",
      "cannot compare string with numeric type (u8)\n\nThis error occurred in the following expression:\n\t[(col(\"U\")) == (\"1\")]\nwhile "
      "evaluating this larger expression:\n\t[([(col(\"U\")) == (\"1\")]) & ([([(col(\"time:timestamp\")) >= (2023-12-31 00:00:00 UTC)]) & "
      "([(col(\"time:timestamp\")) <= (2024-01-02 00:00:00 UTC)])])]\n");
}

// An Enum activity column (categories b, a) on cases 1 and 2.
EventTable enum_frame() {
  std::vector<std::vector<std::string>> rows;
  const std::vector<std::string> times{"2020-01-01T00:00:00+00:00", "2020-01-01T01:00:00+00:00", "2020-01-01T02:00:00+00:00"};
  const int cases[] = {1, 1, 2};
  const char* acts[] = {"a", "b", "a"};
  for (int i = 0; i < 3; ++i)
    rows.push_back({integer_cell(kCase, cases[i]), string_cell(kActivity, acts[i]), date_cell(kTime, times[i]), string_cell("eid", "e" + std::to_string(i))});
  return frame({{kCase, "Int64"}, {kActivity, "Enum(categories=['b', 'a'])"}, {kTime, kUsUtc}, {"eid", "String"}}, rows,
               "\"categorical_dtypes\": [null, {\"categories\": [{\"key\": \"\", \"value\": \"b\"}, {\"key\": \"\", \"value\": \"a\"}], "
               "\"categories_dtype\": \"String\", \"ordered\": false}, null, null], ");
}

void polars_enum_sequence_filters() {
  const auto source = enum_frame();
  check_error<PolarsSequenceFilterError>(
      "Polars trace variants on an Enum activity", [&] { (void)polars_trace_filter_get_variants_df(source, kCase, kActivity); },
      "InvalidOperationError",
      "attempted list join with non-string dtype: list[enum]\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'with_columns' "
      "<---\nAGGREGATE[maintain_order: true]\n  [col(\"concept:name\").implode().alias(\"variant_list\"), len().alias(\"count\")] BY "
      "[col(\"case:concept:name\")]\n  FROM\n  SORT BY [col(\"case:concept:name\"), col(\"time:timestamp\")]\n    DF [\"case:concept:name\", "
      "\"concept:name\", \"time:timestamp\", \"eid\"]; PROJECT */4 COLUMNS\n\nThis error occurred with the following context stack:\n\t[1] "
      "'with_columns'\n");
  const auto values = PolarsFilterValue::list({PolarsFilterValue::of("a"), PolarsFilterValue::of("x"), PolarsFilterValue::none()});
  check_error<PolarsSequenceFilterError>(
      "Polars start activities outside the Enum categories", [&] { (void)polars_start_activities_apply(source, values, {}); },
      "InvalidOperationError",
      "conversion from `str` to `enum` failed in column '' for 1 out of 3 values: [\"x\"]\n\nEnsure that all values in the input column are "
      "present in the categories of the enum datatype.\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'join' <---\nFILTER "
      "col(\"start_activity\").is_in([[\"a\", \"x\", null]])\nFROM\n  AGGREGATE[maintain_order: false]\n    "
      "[col(\"concept:name\").first().alias(\"start_activity\")] BY [col(\"case:concept:name\")]\n    FROM\n    DF [\"case:concept:name\", "
      "\"concept:name\", \"time:timestamp\", \"eid\"]; PROJECT */4 COLUMNS\n\nThis error occurred with the following context stack:\n\t[1] "
      "'filter' failed\n\t[2] 'select'\n\t[3] 'join right'\n\t[4] 'join'\n");
  check("Polars start activities inside the Enum categories",
        eids(polars_start_activities_apply(source, PolarsFilterValue::list({PolarsFilterValue::of("a")}), {})), "e0 e1 e2");
}

}  // namespace

int main() {
  ocel_repeated_relations();
  polars_uint8_keys();
  polars_enum_sequence_filters();
  if (failures) {
    std::cerr << failures << " review CA1 check(s) failed\n";
    return 1;
  }
  std::cout << "review CA1 checks passed\n";
  return 0;
}
