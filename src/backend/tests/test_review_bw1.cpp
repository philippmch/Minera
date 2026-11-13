// Review BW1: nanosecond and millisecond timestamps on the native table
// filters and the OCEL features and SQLite export. pandas sorts a case's
// events by the exact datetime64[ns] instant before reading its paths.
// Polars Datetime columns carry ns, us or ms units: a column compares with
// the microsecond literal of a Python datetime at the coarser unit (a ns
// column floors to microseconds, a typed bound floors to a ms column's
// milliseconds), a naive column's error names its unit, and durations count
// whole seconds of the column unit. pm4py's OCEL features read
// Timestamp.timestamp() (microseconds, rounded) and write_ocel2_sqlite floors
// the event times to microseconds. Values are taken from pm4py 2.7.22
// black-box on the same input (review-harness round-bw, bw1/mk.py).
#include "minera/dataframe_format_json.hpp"
#include "minera/ocel.hpp"
#include "minera/ocel_features.hpp"
#include "minera/ocel_sqlite.hpp"
#include "minera/pandas_paths_filter.hpp"
#include "minera/polars_case_filter.hpp"
#include "minera/polars_paths_filter.hpp"
#include "minera/polars_timestamp_filter.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
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

void check_error(const std::string& label, const std::function<void()>& run, const std::string& message) {
  try {
    run();
  } catch (const std::exception& error) {
    if (std::string(error.what()) == message) return;
    ++failures;
    std::cerr << label << ":\n  error    " << error.what() << "\n  expected " << message << "\n";
    return;
  }
  ++failures;
  std::cerr << label << ": no error, expected " << message << "\n";
}

struct Row {
  std::string case_id, activity, time;
};

// A table of case, activity, timestamp and a unique "eid" per row (e0, e1,
// ...); the timestamps are ISO texts with an optional nanosecond fraction.
EventTable table(const std::vector<Row>& rows, const std::string& time_dtype, bool polars) {
  const bool utc = time_dtype.find("UTC") != std::string::npos;
  const bool nanos = time_dtype.find("ns") != std::string::npos;
  const std::string text = polars ? "String" : "object";
  std::ostringstream out;
  out << R"({"columns": ["case:concept:name", "concept:name", "time:timestamp", "eid"], "dtypes": [")" << text
      << R"(", ")" << text << R"(", ")" << time_dtype << R"(", ")" << text << R"("], "rows": [)";
  for (std::size_t i = 0; i < rows.size(); ++i) {
    const auto time = rows[i].time + (utc ? "+00:00" : "");
    out << (i ? ", " : "") << R"([{"key": "case:concept:name", "value": ")" << rows[i].case_id
        << R"("}, {"key": "concept:name", "value": ")" << rows[i].activity
        << R"("}, {"key": "time:timestamp", "kind": "date", "value": ")" << time << R"(", "typed_value": ")" << time
        << '"' << (nanos ? R"(, "date_resolution": "ns")" : "") << (utc ? R"(, "date_timezone": "UTC")" : "")
        << R"(}, {"key": "eid", "value": "e)" << i << R"("}])";
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
EventAttribute date(std::string instant) {
  EventAttribute out{"", instant, AttributeKind::kDate};
  out.typed_value = NativeDate{instant};
  return out;
}
EventAttribute collection(AttributeContainerKind kind, std::vector<EventAttribute> children) {
  EventAttribute out{"", ""};
  out.container = kind;
  out.children = std::move(children);
  return out;
}
EventAttribute path(const std::string& a, const std::string& b) {
  return collection(AttributeContainerKind::kTuple, {text(a), text(b)});
}

void pandas_paths_order_nanoseconds() {
  // c1's A comes 600 ns after its B, in the same microsecond, row first.
  const auto frame = table({{"c1", "A", "2024-01-01T00:00:00.0000006"}, {"c1", "B", "2024-01-01T00:00:00"},
                            {"c1", "C", "2024-01-01T00:00:01"}, {"c2", "A", "2024-01-01T00:00:00"},
                            {"c2", "B", "2024-01-01T00:00:00.0000006"}},
                           "datetime64[ns, UTC]", false);
  check("pandas paths B,A on ns", eids(filter_pandas_paths(frame, collection(AttributeContainerKind::kList, {path("B", "A")}))), "e1 e0 e2");
  check("pandas paths A,B on ns", eids(filter_pandas_paths(frame, collection(AttributeContainerKind::kList, {path("A", "B")}))), "e3 e4");
  PandasPathsFilterOptions options;
  options.min_performance = integer(0);
  options.max_performance = integer(0);
  check("pandas path performance B,A at most 0 s", eids(filter_pandas_path_performance(frame, path("B", "A"), options)), "");
  options.max_performance = integer(1);
  check("pandas path performance B,A at most 1 s", eids(filter_pandas_path_performance(frame, path("B", "A"), options)), "e1 e0 e2");
}

void polars_timestamp_units() {
  const std::vector<Row> rows{{"c1", "A", "2024-01-01T00:00:00"}, {"c1", "A", "2024-01-01T00:00:00.0000006"},
                              {"c1", "A", "2024-01-01T00:00:00.000001"}, {"c1", "A", "2024-01-01T00:00:01"}};
  const auto ns_utc = table(rows, "Datetime(time_unit='ns', time_zone='UTC')", true);
  check("Polars events on ns, equal bounds", eids(filter_polars_timestamp_events(ns_utc, text("2024-01-01 00:00:00"), text("2024-01-01 00:00:00"))), "e0 e1");
  check("Polars events on ns, range", eids(filter_polars_timestamp_events(ns_utc, text("2023-12-31 23:00:00"), text("2024-01-01 00:00:00"))), "e0 e1");
  const std::string expression = "[(col(\"time:timestamp\")) >= (2023-12-31 23:00:00 UTC)]";
  const std::string tail = " and series 'literal' of dtype: Datetime('\xce\xbcs', 'UTC')\n\nThis error occurred in the following expression:\n\t" +
                           expression + "\nwhile evaluating this larger expression:\n\t[(" + expression +
                           ") & ([(col(\"time:timestamp\")) <= (2024-01-01 00:00:00 UTC)])]\n";
  for (const std::string unit : {"ns", "ms"}) {
    const auto naive = table(rows, "Datetime(time_unit='" + unit + "', time_zone=None)", true);
    check_error("Polars events on naive " + unit, [&] { (void)filter_polars_timestamp_events(naive, text("2023-12-31 23:00:00"), text("2024-01-01 00:00:00")); },
                "could not evaluate '>=' comparison between series 'time:timestamp' of dtype: Datetime('" + unit + "')" + tail);
  }
  // On a ns column '>' compares the exact nanoseconds and '<=' the cell
  // floored to microseconds: 03:00:00.0000006 is after 03:00 and at most
  // 03:00.
  const auto span = table({{"c1", "A", "2024-01-01T01:05:00"}, {"c1", "A", "2024-01-01T03:00:00.0000006"}},
                          "Datetime(time_unit='ns', time_zone='UTC')", true);
  check("Polars intersecting on ns", eids(filter_polars_timestamp_intersecting(span, text("2024-01-01 02:00:00"), text("2024-01-01 03:00:00"))), "e0 e1");
  check("Polars contained on ns", eids(filter_polars_timestamp_contained(span, text("2024-01-01 01:00:00"), text("2024-01-01 03:00:00"))), "e0 e1");
  // A datetime bound with a sub-millisecond fraction floors to the ms column.
  const auto ms_utc = table({{"c1", "A", "2024-01-01T00:00:00"}, {"c1", "A", "2024-01-01T00:00:00.001"}, {"c1", "A", "2024-01-01T00:00:00.002"}},
                            "Datetime(time_unit='ms', time_zone='UTC')", true);
  check("Polars events on ms, typed bound", eids(filter_polars_timestamp_events(ms_utc, date("2024-01-01T00:00:00.001500+00:00"), date("2024-01-01T01:00:00+00:00"))), "e1 e2");
}

void polars_durations() {
  const auto ns_utc = table({{"c1", "A", "2024-01-01T00:00:00"}, {"c1", "A", "2024-01-01T00:00:01.999999999"},
                             {"c2", "A", "2024-01-01T00:00:00"}, {"c2", "A", "2024-01-01T00:00:02"}},
                            "Datetime(time_unit='ns', time_zone='UTC')", true);
  check("Polars case performance on ns", eids(filter_polars_case_performance(ns_utc, integer(2), integer(10))), "e2 e3");
  const auto ms_naive = table({{"c1", "A", "2024-01-01T00:00:00"}, {"c1", "B", "2024-01-01T00:00:01.999"},
                               {"c2", "A", "2024-01-01T00:00:00"}, {"c2", "B", "2024-01-01T00:00:02"}},
                              "Datetime(time_unit='ms', time_zone=None)", true);
  PolarsPathsFilterOptions options;
  options.min_performance = integer(2);
  options.max_performance = integer(10);
  check("Polars path performance on ms", eids(filter_polars_path_performance(ms_naive, path("A", "B"), options)), "e2 e3");
}

Ocel nanosecond_ocel() {
  Ocel log;
  log.events = {{"e1", "a", "2033-06-07T06:41:59.593824219Z", {}}, {"e2", "b", "2033-06-07T06:42:58.593877840Z", {}}};
  log.objects = {{"o1", "t", {}}};
  log.relations = {{"e1", "o1", ""}, {"e2", "o1", ""}};
  return log;
}

std::string number(double value) {
  char buffer[64];
  std::snprintf(buffer, sizeof buffer, "%.17g", value);
  return buffer;
}

void ocel_features_nanoseconds() {
  const auto log = nanosecond_ocel();
  const auto events = extract_ocel_features(log, OcelFeatureVariant::kEvents);
  const auto at = std::find(events.feature_names.begin(), events.feature_names.end(), "@@event_timestamp") - events.feature_names.begin();
  check("OCEL event timestamp feature e1", number(events.values[0][at]), number(2001739319.593824));
  check("OCEL event timestamp feature e2", number(events.values[1][at]), number(2001739378.593878));
  const auto objects = extract_ocel_features(log, OcelFeatureVariant::kObjects);
  const auto feature = [&](const std::string& name) {
    const auto found = std::find(objects.feature_names.begin(), objects.feature_names.end(), name);
    return found == objects.feature_names.end() ? std::string("absent") : number(objects.values[0][found - objects.feature_names.begin()]);
  };
  check("OCEL lifecycle duration", feature("@@object_lifecycle_duration"), number(59.000054121017456));
  check("OCEL lifecycle start", feature("@@object_lifecycle_start_timestamp"), number(2001739319.593824));
  check("OCEL lifecycle end", feature("@@object_lifecycle_end_timestamp"), number(2001739378.593878));
}

void ocel_sqlite_microseconds() {
  const auto path = std::filesystem::temp_directory_path() / "minera_review_bw1.sqlite";
  std::filesystem::remove(path);
  export_ocel_sqlite(nanosecond_ocel(), path);
  const auto back = import_ocel_sqlite(path);
  std::filesystem::remove(path);
  std::string times;
  for (const auto& event : back.events) times += (times.empty() ? "" : " ") + event.id + "=" + event.timestamp;
  // pm4py's write_ocel2_sqlite writes the same texts.
  check("OCEL SQLite event times", times, "e1=2033-06-07 06:41:59.593824+00:00 e2=2033-06-07 06:42:58.593877+00:00");
}

}  // namespace

int main() {
  pandas_paths_order_nanoseconds();
  polars_timestamp_units();
  polars_durations();
  ocel_features_nanoseconds();
  ocel_sqlite_microseconds();
  if (failures) {
    std::cerr << failures << " review BW1 check(s) failed\n";
    return 1;
  }
  std::cout << "review BW1 checks passed\n";
  return 0;
}
