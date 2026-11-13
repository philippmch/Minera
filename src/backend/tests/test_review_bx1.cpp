// Review BX1: Polars 1.44.1 sorts the native table frames by case and
// timestamp with its parallel unstable sort, so rows with tied timestamps
// leave their input order the way that sort leaves them (on frames of more
// than 20 rows). A Datetime or Date activity column reads as its String cast
// ("2024-01-01 00:00:00.000000001+00:00", "2024-01-01"). top_k with the
// activity key on the case column fails resolving the group_by. The
// timestamp attribute filter matches a Boolean column through its String
// cast, rejects a string value on a temporal column, and prints the '<='
// bound of a nanosecond UTC column 999 ns on. pm4py's OCEL description
// prints nanosecond timestamps as str(Timestamp) and the lifecycle duration
// as the float repr of Timestamp.timestamp() differences. Values are taken
// from pm4py 2.7.22 black-box on the same input (review-harness round-bx,
// bx1/mk.py).
#include "minera/dataframe_format_json.hpp"
#include "minera/ocel.hpp"
#include "minera/polars_paths_filter.hpp"
#include "minera/polars_timestamp_filter.hpp"
#include "minera/polars_variants_filter.hpp"

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
std::string date_cell(const std::string& key, const std::string& value, bool nanoseconds, bool utc) {
  return "{\"key\": " + quoted(key) + ", \"kind\": \"date\", \"value\": " + quoted(value) + ", \"typed_value\": " + quoted(value) +
         (nanoseconds ? ", \"date_resolution\": \"ns\"" : "") + (utc ? ", \"date_timezone\": \"UTC\"" : "") + "}";
}
std::string boolean_cell(const std::string& key, bool value) {
  return "{\"key\": " + quoted(key) + ", \"kind\": \"boolean\", \"value\": \"" + (value ? "true" : "false") + "\", \"typed_value\": " +
         (value ? "true" : "false") + "}";
}
std::string integer_cell(const std::string& key, int value) {
  return "{\"key\": " + quoted(key) + ", \"kind\": \"int\", \"value\": \"" + std::to_string(value) + "\", \"typed_value\": " + std::to_string(value) + "}";
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
EventAttribute collection(AttributeContainerKind kind, std::vector<EventAttribute> children) {
  EventAttribute out{"", ""};
  out.container = kind;
  out.children = std::move(children);
  return out;
}
EventAttribute sequence(const std::vector<std::string>& values) {
  std::vector<EventAttribute> children;
  for (const auto& value : values) children.push_back(text(value));
  return collection(AttributeContainerKind::kTuple, std::move(children));
}
EventAttribute list_of(EventAttribute item) { return collection(AttributeContainerKind::kList, {std::move(item)}); }

const std::string kCase = "case:concept:name", kActivity = "concept:name", kTime = "time:timestamp";
const std::string kUsUtc = "Datetime(time_unit='us', time_zone='UTC')", kNsUtc = "Datetime(time_unit='ns', time_zone='UTC')";

// 24 rows at one instant; the cases alternate c2, c1 and activity a<i> is
// row i.
EventTable tied_frame() {
  std::vector<std::vector<std::string>> rows;
  for (int i = 0; i < 24; ++i)
    rows.push_back({string_cell(kCase, i % 2 == 0 ? "c2" : "c1"), string_cell(kActivity, "a" + std::to_string(i)),
                    date_cell(kTime, "2024-01-01T00:00:00+00:00", false, true), string_cell("eid", "e" + std::to_string(i))});
  return frame({{kCase, "String"}, {kActivity, "String"}, {kTime, kUsUtc}, {"eid", "String"}}, rows);
}

void polars_tied_timestamps() {
  const auto tied = tied_frame();
  // Polars orders c1's tied rows e13 e1 e23 e3 ... and c2's e12 e2 e14 e0 ...
  check("Polars paths a13,a1 on tied rows", eids(filter_polars_paths(tied, list_of(sequence({"a13", "a1"})))),
        "e13 e1 e23 e3 e21 e5 e19 e7 e17 e9 e15 e11");
  check("Polars paths a1,a3 (input order) on tied rows", eids(filter_polars_paths(tied, list_of(sequence({"a1", "a3"})))), "");
  check("Polars variants of c2's tied rows",
        eids(filter_polars_variants(tied, list_of(sequence({"a12", "a2", "a14", "a0", "a16", "a10", "a18", "a8", "a20", "a6", "a22", "a4"})))),
        "e0 e2 e4 e6 e8 e10 e12 e14 e16 e18 e20 e22");
  PolarsVariantsFilterOptions options;
  options.activity_key = text(kCase);
  check_error<PolarsVariantsFilterError>(
      "Polars top_k with the activity key on the case column", [&] { (void)filter_polars_variants_top_k(tied, integer(1), options); },
      "DuplicateError",
      "column with name 'case:concept:name' has more than one occurrence\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING "
      "'group_by' <---\nSORT BY [col(\"case:concept:name\"), col(\"time:timestamp\")]\n  DF [\"case:concept:name\", \"concept:name\", "
      "\"time:timestamp\", \"eid\"]; PROJECT */4 COLUMNS\n\nThis error occurred with the following context stack:\n\t[1] 'group_by'\n");
}

void polars_temporal_activities() {
  const auto build = [](const std::string& dtype, const std::vector<std::string>& activities, bool nanoseconds, bool utc) {
    std::vector<std::vector<std::string>> rows;
    const std::vector<std::string> cases{"c1", "c1", "c2", "c2"};
    const std::vector<std::string> times{"2024-01-01T00:00:00+00:00", "2024-01-01T00:00:01+00:00", "2024-01-01T00:00:00+00:00",
                                         "2024-01-01T00:00:01+00:00"};
    for (std::size_t i = 0; i < 4; ++i)
      rows.push_back({string_cell(kCase, cases[i]), date_cell(kActivity, activities[i], nanoseconds, utc), date_cell(kTime, times[i], false, true),
                      string_cell("eid", "e" + std::to_string(i))});
    return frame({{kCase, "String"}, {kActivity, dtype}, {kTime, kUsUtc}, {"eid", "String"}}, rows);
  };
  const auto ns = build(kNsUtc,
                        {"2024-01-01T00:00:00.000000001+00:00", "2024-01-02T00:00:00+00:00", "2024-01-02T00:00:00+00:00",
                         "2024-01-01T00:00:00.000000001+00:00"},
                        true, true);
  const std::string first = "2024-01-01 00:00:00.000000001+00:00", second = "2024-01-02 00:00:00.000000000+00:00";
  check("Polars paths on a Datetime ns activity", eids(filter_polars_paths(ns, list_of(sequence({first, second})))), "e0 e1");
  check("Polars variants on a Datetime ns activity", eids(filter_polars_variants(ns, list_of(sequence({second, first})))), "e2 e3");
  const auto dates = build("Date", {"2024-01-01", "2024-01-02", "2024-01-02", "2024-01-01"}, false, false);
  check("Polars paths on a Date activity", eids(filter_polars_paths(dates, list_of(sequence({"2024-01-01", "2024-01-02"})))), "e0 e1");
}

void polars_timestamp_attribute() {
  const std::vector<std::string> times{"2024-01-01T00:00:00+00:00", "2024-01-01T00:00:00.0000006+00:00", "2024-01-01T00:00:01+00:00"};
  const std::vector<std::string> cases{"c1", "c1", "c2"}, activities{"A", "B", "A"};
  const bool flags[] = {false, true, true};
  std::vector<std::vector<std::string>> rows;
  for (std::size_t i = 0; i < 3; ++i)
    rows.push_back({string_cell(kCase, cases[i]), string_cell(kActivity, activities[i]), date_cell(kTime, times[i], true, true),
                    boolean_cell("flag", flags[i]), integer_cell("n", static_cast<int>(i) + 1), date_cell("d", times[i], true, true),
                    string_cell("eid", "e" + std::to_string(i))});
  const auto source = frame({{kCase, "String"}, {kActivity, "String"}, {kTime, kNsUtc}, {"flag", "Boolean"}, {"n", "Int64"}, {"d", kNsUtc}, {"eid", "String"}},
                            rows);
  const auto lo = text("2023-12-31 00:00:00"), hi = text("2024-01-02 00:00:00");
  check("Polars timestamp attribute Boolean == 'true'", eids(filter_polars_timestamp_attribute(source, text("flag"), text("true"), lo, hi)), "e0 e1 e2");
  check("Polars timestamp attribute Boolean == 'false'", eids(filter_polars_timestamp_attribute(source, text("flag"), text("false"), lo, hi)), "e0 e1");
  const auto numeric = [](const std::string& low, const std::string& high) {
    return "cannot compare string with numeric type (i64)\n\nThis error occurred in the following expression:\n\t[(col(\"n\")) == (\"x\")]\nwhile "
           "evaluating this larger expression:\n\t[([(col(\"n\")) == (\"x\")]) & ([([(col(\"time:timestamp\")) >= (" +
           low + " UTC)]) & ([(col(\"time:timestamp\")) <= (" + high + " UTC)])])]\n";
  };
  check_error<PolarsTimestampFilterError>(
      "Polars timestamp attribute Int64 vs string on ns", [&] { (void)filter_polars_timestamp_attribute(source, text("n"), text("x"), lo, hi); },
      "ComputeError", numeric("2023-12-31 00:00:00", "2024-01-02 00:00:00.000000999"));
  const auto at = text("2024-01-01 00:00:00");
  check_error<PolarsTimestampFilterError>(
      "Polars timestamp attribute Int64 vs string on ns, equal bounds",
      [&] { (void)filter_polars_timestamp_attribute(source, text("n"), text("x"), at, at); }, "ComputeError",
      numeric("2024-01-01 00:00:00", "2024-01-01 00:00:00.000000999"));
  check_error<PolarsTimestampFilterError>(
      "Polars timestamp attribute Datetime vs string", [&] { (void)filter_polars_timestamp_attribute(source, text("d"), text("x"), lo, hi); },
      "InvalidOperationError",
      "cannot compare 'date/datetime/time' to a string value (create native python { 'date', 'datetime', 'time' } or compare to a temporal "
      "column)\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'join' <---\nFILTER col(\"d\") == \"x\"\nFROM\n  DF "
      "[\"case:concept:name\", \"concept:name\", \"time:timestamp\", \"flag\", ...]; PROJECT */7 COLUMNS\n\nThis error occurred with the "
      "following context stack:\n\t[1] 'filter' failed\n\t[2] 'select'\n\t[3] 'unique'\n\t[4] 'join right'\n\t[5] 'join'\n");
}

void ocel_description_nanoseconds() {
  Ocel log;
  log.events = {{"e1", "a", "2033-06-07T06:41:59.593824219Z", {}}, {"e2", "b", "2033-06-08T07:44:58.593877840Z", {}}};
  log.objects = {{"o1", "t", {}}};
  log.relations = {{"e1", "o1", ""}, {"e2", "o1", ""}};
  check("OCEL description on ns timestamps", describe_ocel_text(log),
        "\n\nevents:\n\na ( related objects: o1 )  timestamp: 2033-06-07 06:41:59.593824219+00:00\nb ( related objects: o1 )  timestamp: "
        "2033-06-08 07:44:58.593877840+00:00\n\nobjects:\n\no1 object type: t ( lifecycle start: 2033-06-07 06:41:59.593824219+00:00 ; "
        "lifecycle end: 2033-06-08 07:44:58.593877840+00:00 ; lifecycle duration: 90179.00005412102 )");
}

}  // namespace

int main() {
  polars_tied_timestamps();
  polars_temporal_activities();
  polars_timestamp_attribute();
  ocel_description_nanoseconds();
  if (failures) {
    std::cerr << failures << " review BX1 check(s) failed\n";
    return 1;
  }
  std::cout << "review BX1 checks passed\n";
  return 0;
}
