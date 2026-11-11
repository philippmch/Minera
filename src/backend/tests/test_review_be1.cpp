// Review BE1: the attribute KDE on uploads (pm4py's DataFrame statistics on
// the frame: the column's cells without the missing ones, sorted by Python,
// pandas' and numpy's errors for text, dates and mixed columns, no @@index
// on an XES frame, a date grid outside datetime64[ns] overflows), and
// pandas' mixed date parsing of all-numeric cells in text columns (partial
// dates such as "2020-01", "07/2020" and "2020", compact "20200101", mixed
// separators "03.24-2020", dateutil's defaults for "007" and "07-12"), in a
// timestamp column and in other text columns of a CSV upload. Values and
// messages are taken from pm4py 2.7.22 and pandas 3.0.5 black-box on the
// same texts (pm4py.read_xes, or read_csv and format_dataframe).
#include "minera/attribute_kde.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/dataframe_columns.hpp"
#include "minera/timestamp.hpp"
#include "minera/xes_event_log_importer.hpp"

#include <cassert>
#include <cmath>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace minera;

namespace {

EventLog xes_upload(const std::string& text) {
  std::istringstream xes(text);
  auto log = import_xes_dataframe_log(xes);
  log.dataframe_columns = std::make_shared<DataFrameColumns>(xes_dataframe_columns(log));
  return log;
}

EventLog csv_upload(const std::string& text) {
  std::istringstream csv(text);
  CsvImportOptions import;
  import.case_id_column = "case:concept:name";
  import.activity_column = "concept:name";
  import.timestamp_column = "time:timestamp";
  auto log = import_csv_event_log(csv, import);
  assert(log.dataframe_columns);
  return log;
}

bool close(double a, double b) { return std::fabs(a - b) <= 1e-9 * std::max(1.0, std::fabs(a)); }

struct KdeCase {
  const char* key;
  AttributeKdeVariant variant;
  // ok: point count, first and last x, y[0] and y[size / 2]; else the error.
  std::size_t points;
  double x_first, x_last, y_first, y_middle;
  const char* error;
};

int run_kde(const EventLog& log, const std::vector<KdeCase>& cases, const char* name) {
  int failures = 0;
  for (const auto& entry : cases) {
    AttributeKdeOptions options;
    options.attribute_key = entry.key;
    options.variant = entry.variant;
    try {
      const auto result = discover_attribute_kde(log, options);
      if (std::string(entry.error).empty() && result.x.size() == entry.points && result.y.size() == entry.points &&
          close(result.x.front(), entry.x_first) && close(result.x.back(), entry.x_last) &&
          close(result.y.front(), entry.y_first) && close(result.y[entry.points / 2], entry.y_middle))
        continue;
      ++failures;
      std::cerr << name << " " << entry.key << ": " << result.x.size() << " points";
      if (!result.x.empty())
        std::cerr << " x " << result.x.front() << ".." << result.x.back() << " y " << result.y.front() << " "
                  << result.y[result.y.size() / 2];
      std::cerr << "\n";
    } catch (const std::invalid_argument& error) {
      if (error.what() == std::string(entry.error)) continue;
      ++failures;
      std::cerr << name << " " << entry.key << ": " << error.what() << "\n";
    }
  }
  return failures;
}

constexpr auto kNumeric = AttributeKdeVariant::kNumeric;
constexpr auto kDate = AttributeKdeVariant::kDate;

int csv_kde() {
  const auto log = csv_upload(
      "case:concept:name,concept:name,time:timestamp,n,t,d,b,one,word\n"
      "c1,a,2020-01-01 10:00:00,3,x,2020-03-01 10:00:00,True,12,x\n"
      "c1,b,2020-01-02 10:00:00,7,5,2021-07-04 00:30:00,False,12,x\n"
      "c2,a,2020-01-03 10:00:00,-2,y,,True,,x\n");
  const std::vector<KdeCase> cases = {
      {"n", kNumeric, 100, -2.0, 7.0, 0.05255853718772946, 0.07037701749904296, ""},
      {"t", kNumeric, 0, 0, 0, 0, 0,
       "TypeError: The `dtype` and `signature` arguments to ufuncs only select the general DType and not details "
       "such as the byte order or time unit. You can avoid this error by using the scalar types `np.float64` or the "
       "dtype string notation."},
      {"d", kNumeric, 0, 0, 0, 0, 0, "TypeError: unsupported operand type(s) for *: 'Timestamp' and 'float'"},
      {"b", kNumeric, 100, 0.0, 1.0, 0.3428870868248324, 0.48290693330136963, ""},
      {"one", kNumeric, 200, 11.88, 12.12, 0.0, 1.0, ""},
      {"word", kNumeric, 0, 0, 0, 0, 0, "ValueError: could not convert string to float: np.str_('x')"},
      {"n", kDate, 0, 0, 0, 0, 0, "AttributeError: 'int' object has no attribute 'replace'"},
      {"t", kDate, 0, 0, 0, 0, 0, "TypeError: str.replace() takes no keyword arguments"},
      {"d", kDate, 200, 1583056800.0, 1625358600.0, 3.121904895861543e-08, 2.6092609682658687e-10, ""},
      {"b", kDate, 0, 0, 0, 0, 0, "AttributeError: 'bool' object has no attribute 'replace'"},
      {"zz", kNumeric, 0, 0, 0, 0, 0, "KeyError: ['zz']"},
      {"@@index", kNumeric, 100, 0.0, 2.0, 0.2493425105905702, 0.31814976842414, ""},
      {"time:timestamp", kDate, 200, 1577872800.0, 1578045600.0, 6.269073484897336e-06, 6.222729476781028e-06, ""},
      {"concept:name", kDate, 0, 0, 0, 0, 0, "TypeError: str.replace() takes no keyword arguments"},
  };
  return run_kde(log, cases, "csv");
}

int xes_kde() {
  const auto log = xes_upload(
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?><log xes.version=\"1.0\"><trace><string key=\"concept:name\" "
      "value=\"c1\"/><event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" "
      "value=\"2020-01-01T10:00:00\"/><int key=\"m\" value=\"3\"/><date key=\"o\" value=\"2020-03-01T10:00:00\"/>"
      "<date key=\"old\" value=\"1600-01-01T00:00:00\"/></event><event><string key=\"concept:name\" value=\"b\"/>"
      "<date key=\"time:timestamp\" value=\"2020-01-02T10:00:00\"/><string key=\"m\" value=\"x\"/><int key=\"o\" "
      "value=\"4\"/><date key=\"old\" value=\"2020-01-01T00:00:00\"/></event></trace></log>");
  const std::vector<KdeCase> cases = {
      {"m", kNumeric, 0, 0, 0, 0, 0, "TypeError: '<' not supported between instances of 'str' and 'int'"},
      {"o", kDate, 0, 0, 0, 0, 0, "TypeError: '<' not supported between instances of 'int' and 'datetime.datetime'"},
      {"@@index", kNumeric, 0, 0, 0, 0, 0, "KeyError: ['@@index']"},
      {"old", kDate, 0, 0, 0, 0, 0, "OverflowError: Python int too large to convert to C long"},
  };
  return run_kde(log, cases, "xes");
}

int numeric_cells() {
  constexpr std::optional<long long> kNone;
  // pandas.to_datetime(format="mixed", utc=True) of the cell, as UTC seconds.
  const std::vector<std::pair<const char*, std::optional<long long>>> cases = {
      {"2020-01", 1577836800LL},       {"2020/5", 1588291200LL},          {"2020.5", 1588291200LL},
      {"2020 05", 1588291200LL},       {"07/2020", 1593561600LL},         {"5-2021", 1619827200LL},
      {"2020", 1577836800LL},          {"1999", 915148800LL},             {"20200101", 1577836800LL},
      {"202001011230", 1577881800LL},  {"20200101123045", 1577881845LL},  {"250699", 930268800LL},
      {"012345", 2368742400LL},        {"007", -62135078400LL},           {"039", 2177452800LL},
      {"015", -62134387200LL},         {"07-12", -62119008000LL},         {"1/20", -62133955200LL},
      {"12/", -62134646400LL},         {"1128.", -26571024000LL},         {"2992.20", 32251219200LL},
      {"06.45", -62135164800LL},       {"014 ", -61725369600LL},          {"0001 13", -62134560000LL},
      {"9 7 0001/", -62114083200LL},   {"03.24-2020", 1585008000LL},      {"3-4.2020", 1583280000LL},
      {"24.03-2020", 1585008000LL},    {"2020..5", 1588291200LL},         {"5 -2020", 1588291200LL},
      // pandas refuses these.
      {"5", kNone}, {"999", kNone}, {"1.5", kNone}, {"2020.5 ", kNone}, {"2020-13", kNone}, {"202001", kNone},
      {"13/2020", kNone}, {"1999-01-", kNone}, {"12345", kNone}, {"0", kNone},
      // Declared: pandas reads year 0 or a negative year ("/08" is
      // 0000-08-01), a decimal comma ("1,5" is 0001-01-05) and a clock
      // ("123456.7" is 0001-01-01 12:34:56.7).
      {"/08", kNone}, {"0000", kNone}, {"-2020", kNone}, {"1,5", kNone}, {"123456.7", kNone},
  };
  int failures = 0;
  for (const auto& [cell, expected] : cases) {
    const auto parsed = reference_mixed_timestamp(cell);
    std::optional<long long> seconds;
    if (parsed) {
      const auto exact = parse_timestamp_exact(parsed->text);
      seconds = exact ? std::optional<long long>(exact->seconds) : std::optional<long long>(-1);
    }
    if (seconds == expected) continue;
    ++failures;
    std::cerr << "cell '" << cell << "': " << (parsed ? parsed->text : std::string("none")) << "\n";
  }
  return failures;
}

int partial_dates_upload() {
  const auto log = csv_upload(
      "case:concept:name,concept:name,time:timestamp,d,k\n"
      "c1,a,2020-01,2020-03-01 10:00:00,20200101\n"
      "c1,b,07/2020,2020,20200102\n"
      "c1,c,2021,007,20200103\n"
      "c2,a,03.24-2020,1999/12,20200104\n"
      "c2,b,202001011230,9 7 0001/,20200105\n");
  int failures = 0;
  // format_dataframe converts the timestamp column and the text column d;
  // read_csv keeps k as ints.
  const std::map<std::pair<std::string, std::string>, long long> instants = {
      {{"c1", "a"}, 1577836800LL}, {{"c1", "b"}, 1593561600LL}, {{"c1", "c"}, 1609459200LL},
      {{"c2", "a"}, 1585008000LL}, {{"c2", "b"}, 1577881800LL}};
  std::size_t seen = 0;
  for (const auto& trace : log.traces)
    for (const auto& event : trace.events) {
      ++seen;
      const auto exact = parse_timestamp_exact(event.timestamp);
      const auto found = instants.find({trace.case_id, event.activity});
      if (exact && found != instants.end() && exact->seconds == found->second) continue;
      ++failures;
      std::cerr << "timestamp " << trace.case_id << "/" << event.activity << ": " << event.timestamp << "\n";
    }
  if (seen != instants.size()) {
    ++failures;
    std::cerr << seen << " events\n";
  }
  std::map<std::string, DataFrameColumnType> types;
  for (const auto& column : log.dataframe_columns->columns) types[column.key] = column.type;
  if (types["d"] != DataFrameColumnType::kDate || types["k"] != DataFrameColumnType::kInt64) {
    ++failures;
    std::cerr << "column types d " << static_cast<int>(types["d"]) << " k " << static_cast<int>(types["k"]) << "\n";
  }
  // pm4py's date KDE of d meets year 1 (from "007"): outside datetime64[ns].
  AttributeKdeOptions options;
  options.attribute_key = "d";
  options.variant = kDate;
  std::string message;
  try {
    (void)discover_attribute_kde(log, options);
  } catch (const std::invalid_argument& error) {
    message = error.what();
  }
  if (message != "OverflowError: Python int too large to convert to C long") {
    ++failures;
    std::cerr << "d date KDE: " << message << "\n";
  }
  // A timestamp column of numbers stays numbers (read_csv types it).
  const auto numbers = csv_upload(
      "case:concept:name,concept:name,time:timestamp\n"
      "c1,a,20200101\n"
      "c1,b,20200102\n");
  for (const auto& trace : numbers.traces)
    for (const auto& event : trace.events)
      if (event.timestamp != "20200101" && event.timestamp != "20200102") {
        ++failures;
        std::cerr << "number timestamp: " << event.timestamp << "\n";
      }
  return failures;
}

}  // namespace

int main() {
  int failures = 0;
  failures += csv_kde();
  failures += xes_kde();
  failures += numeric_cells();
  failures += partial_dates_upload();
  if (failures != 0) {
    std::cerr << failures << " review BE1 failure(s)\n";
    return 1;
  }
  std::cout << "review BE1 tests passed\n";
  return 0;
}
