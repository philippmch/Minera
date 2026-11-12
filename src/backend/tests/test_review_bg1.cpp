// Review BG1: an XES upload's frame in read_xes's file order when a trace
// name repeats apart from itself, in attribute statistics (value_counts'
// order of equal counts and the first of equal values, once per case too)
// and in case statistics grouped by another column (each group's first and
// last row); and pandas' mixed date parsing of fractions longer than six
// digits, comma fractions (after a basic YYYYMMDD date, after one-digit
// seconds) and a clock before a month-name date. Values are taken from
// pm4py 2.7.22 and pandas 3.0.5 black-box on the same inputs.
#include "minera/attribute_statistics.hpp"
#include "minera/case_statistics.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/dataframe_columns.hpp"
#include "minera/xes_event_log_importer.hpp"

#include <cstdint>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <tuple>
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

std::string event(const char* activity, const char* time, const char* tag, const char* value) {
  return std::string("<event><string key=\"concept:name\" value=\"") + activity +
         "\"/><date key=\"time:timestamp\" value=\"2020-01-01T" + time + "\"/><" + tag + " key=\"n\" value=\"" +
         value + "\"/></event>";
}

std::string trace(const char* name, const std::string& events) {
  return std::string("<trace><string key=\"concept:name\" value=\"") + name + "\"/>" + events + "</trace>";
}

// Three <trace> elements, c1 twice: read_xes's rows are c1, c2, c2, c1, c1
// (by case they would be c1, c1, c1, c2, c2).
EventLog interleaved() {
  return xes_upload("<?xml version=\"1.0\" encoding=\"UTF-8\"?><log xes.version=\"1.0\">" +
                    trace("c1", event("a", "10:00:00", "string", "p")) +
                    trace("c2", event("a", "08:00:00", "string", "q") + event("b", "09:00:00", "int", "1")) +
                    trace("c1", event("a", "12:00:00", "boolean", "true") + event("b", "07:00:00", "string", "r")) +
                    "</log>");
}

int interleaved_statistics() {
  const auto log = interleaved();
  int failures = 0;
  // n in file order is ['p', 'q', 1, True, 'r']: the int 1 is kept for 1 and
  // True, and 'q' comes before 'r'. By case it would be True, then 'p', 'r',
  // 'q'. Once per case the [case, value] groups list c1's values first.
  using Entries = std::vector<std::tuple<std::string, std::string, std::uint64_t>>;
  const Entries all = {{"1", "int", 2}, {"p", "str", 1}, {"q", "str", 1}, {"r", "str", 1}};
  const Entries once = {{"1", "int", 2}, {"p", "str", 1}, {"r", "str", 1}, {"q", "str", 1}};
  for (const bool keep_once : {false, true}) {
    AttributeStatisticsOptions options;
    options.attribute_key = "n";
    options.keep_once_per_case = keep_once;
    const auto result = discover_attribute_statistics(log, options);
    Entries seen;
    for (const auto& entry : result.entries) seen.emplace_back(entry.value, entry.type, entry.count);
    if (seen == (keep_once ? once : all)) continue;
    ++failures;
    std::cerr << "attribute statistics" << (keep_once ? " once" : "") << ":";
    for (const auto& [value, type, count] : seen) std::cerr << " " << value << "/" << type << "=" << count;
    std::cerr << "\n";
  }
  // Grouped by the activity, a group's first and last rows are those of the
  // file: b starts 09:00 and ends 07:00, a runs 10:00 to 12:00, b first by
  // start time. By case, a would end 08:00 and b would run 07:00 to 09:00.
  CaseStatisticsOptions options;
  options.case_id_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  options.dataframe = true;
  const auto result = discover_case_statistics(log, options);
  std::vector<std::tuple<std::string, double, double>> seen;
  for (const auto& entry : result.cases) seen.emplace_back(entry.case_id, entry.start_time, entry.end_time);
  const double day = 1577836800.0;
  const decltype(seen) expected = {{"b", day + 9 * 3600, day + 7 * 3600}, {"a", day + 10 * 3600, day + 12 * 3600}};
  if (seen != expected) {
    ++failures;
    std::cerr << "case statistics:";
    for (const auto& [id, start, end] : seen) std::cerr << " " << id << " " << start << ".." << end;
    std::cerr << "\n";
  }
  return failures;
}

int date_cells() {
  // pandas.to_datetime(format="mixed", utc=True) of the cell; "" is NaT.
  const std::vector<std::pair<const char*, const char*>> cases = {
      // Off the ISO path a `.` fraction keeps its nanoseconds only when its
      // fourth to sixth digits are 000 and the clock is H:MM:SS with
      // two-digit minutes and seconds; otherwise microseconds.
      {"Jan 5 2020 10:30:15.123000789", "2020-01-05T10:30:15.123000789+00:00"},
      {"05.01.2020 10:30:15.123000789", "2020-05-01T10:30:15.123000789+00:00"},
      {"Jan 5 2020 10:30:15.123456789", "2020-01-05T10:30:15.123456+00:00"},
      {"Jan 5 2020 10:30:5.123000789", "2020-01-05T10:30:05.123000+00:00"},
      {"Jan 5 2020 10:3:15.123000789", "2020-01-05T10:03:15.123000+00:00"},
      {"Jan 5 2020 103015.123000789", "2020-01-05T10:30:15.123000+00:00"},
      {"05.01.2020 10:30:15,123000789", "2020-05-01T10:30:15.123000+00:00"},
      // A comma fraction after a basic date reads as after YYYY-MM-DD, and
      // needs two-digit seconds.
      {"20240105 11:07:52,5", "2024-01-05T11:07:52.500000+00:00"},
      {"20240105 11:07:52,123456789", "2024-01-05T11:07:52.123456+00:00"},
      {"Jan 5 2020 10:30:15,5", "2020-01-05T10:30:15.500000+00:00"},
      {"20240105 11:07:5,5", ""},
      {"Jan 5 2020 10:30:5,5", ""},
      {"05/01/2020 10:30:5,5", ""},
      // A clock before a month-name date, a zone or offset between them.
      {"10:30 Jan 5 2020", "2020-01-05T10:30:00+00:00"},
      {"10:30:15 Jan 5, 2020", "2020-01-05T10:30:15+00:00"},
      {"7:05 PM Mon Jan 6 2020", "2020-01-06T19:05:00+00:00"},
      {"10:30 5 January 2020", "2020-01-05T10:30:00+00:00"},
      {"10:30 UTC Jan 5 2020", "2020-01-05T10:30:00+00:00"},
      {"10:30 +01:00 Jan 5 2020", "2020-01-05T09:30:00+00:00"},
      {"10:30 Jan 5 2020 UTC", "2020-01-05T10:30:00+00:00"},
      {"10:30:15,5 Jan 5 2020", "2020-01-05T10:30:15.500000+00:00"},
      {"10:30:5,5 Jan 5 2020", ""},
      {"10:30 Jan Feb 2020", ""},
      // Declared: pandas fills a missing day with today's ("10:30 Jan 2020").
      {"10:30 Jan 2020", ""},
  };
  int failures = 0;
  for (const auto& [cell, expected] : cases) {
    const auto parsed = reference_mixed_timestamp(cell);
    std::string seen;
    if (parsed) {
      const auto value = csv_utc_date_cell("d", parsed->text);
      seen = value ? value->value : "no cell: " + parsed->text;
    }
    if (seen == expected) continue;
    ++failures;
    std::cerr << "cell '" << cell << "': " << seen << "\n";
  }
  return failures;
}

}  // namespace

int main() {
  int failures = 0;
  failures += interleaved_statistics();
  failures += date_cells();
  if (failures != 0) {
    std::cerr << failures << " review BG1 failure(s)\n";
    return 1;
  }
  std::cout << "review BG1 tests passed\n";
  return 0;
}
