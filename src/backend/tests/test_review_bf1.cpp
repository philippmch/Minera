// Review BF1: CPython 3.11's list.sort for 64 items and more (timsort runs,
// powersort merge order and galloping: the comparisons made, the order they
// leave and where an error stops them); an XES upload's frame in read_xes's
// file order when a trace name repeats (the attribute KDE's sort, network
// analysis links between cases, the SNA pandas variants' sequence sort);
// joint activities (pandas) dropping each column's NaN on its own;
// and pandas' mixed date parsing of a six-digit date before a clock and of
// a clock before the date. Values and messages are taken from CPython
// 3.11, pm4py 2.7.22 and pandas 3.0.5 black-box on the same inputs.
#include "minera/attribute_kde.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/dataframe_columns.hpp"
#include "minera/network_analysis.hpp"
#include "minera/organizational_mining.hpp"
#include "minera/timestamp.hpp"
#include "minera/xes_event_log_importer.hpp"

#include "../python_scalar.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
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

// CPython 3.11 list.sort of K(0) .. K(n - 1) whose __lt__ counts its calls:
// the call count, sum((i + 1) * value) mod 2**61 - 1 over the order left
// behind, its first six values and the operands of the comparison that raised.
struct SortCase {
  const char* mode;
  long long n;
  long long calls;
  long long checksum;
  std::vector<long long> head;
  const char* error;
};

int sort_cases() {
  const std::vector<SortCase> cases = {
      {"raw", 200, 1089, 2465562, {100, 1, 50, 51, 10, 64}, ""},
      {"raw", 1000, 6787, 317242197, {1, 10, 64, 72, 88, 73}, ""},
      {"keys", 1000, 8428, 251987250, {0, 50, 100, 150, 200, 250}, ""},
      {"raise", 300, 559, 8955151, {0, 37, 74, 12, 49, 24}, "137 130"},
  };
  int failures = 0;
  for (const auto& entry : cases) {
    const std::string mode = entry.mode;
    long long calls = 0;
    const auto less = [&](long long a, long long b) {
      ++calls;
      if (mode == "raw") return ((a * 2654435761LL + b * 40503LL) >> 7) % 3 == 0;
      if (mode == "keys") return (a * 7919) % 50 < (b * 7919) % 50;
      if (a == 137 || b == 137) throw std::runtime_error(std::to_string(a) + " " + std::to_string(b));
      return (a * 7919) % 1000 < (b * 7919) % 1000;
    };
    std::vector<long long> items;
    for (long long i = 0; i < entry.n; ++i) items.push_back(i);
    std::string error;
    try {
      python::sort(items, less);
    } catch (const std::runtime_error& raised) {
      error = raised.what();
    }
    constexpr unsigned long long kModulus = (1ULL << 61) - 1;
    unsigned long long checksum = 0;
    for (std::size_t i = 0; i < items.size(); ++i)
      checksum = (checksum + static_cast<unsigned long long>(i + 1) * static_cast<unsigned long long>(items[i])) % kModulus;
    const std::vector<long long> head(items.begin(), items.begin() + 6);
    if (calls == entry.calls && static_cast<long long>(checksum) == entry.checksum && head == entry.head &&
        error == entry.error)
      continue;
    ++failures;
    std::cerr << "sort " << mode << " " << entry.n << ": " << calls << " calls, checksum " << checksum
              << ", error '" << error << "'\n";
  }
  return failures;
}

// Four traces, two names: read_xes keeps the file's rows (c1, c2, c1, c2),
// all at one instant, every event in group G.
std::string event(const char* activity, const char* resource, const char* n_tag, const char* n_value) {
  return std::string("<event><string key=\"concept:name\" value=\"") + activity +
         "\"/><string key=\"org:resource\" value=\"" + resource +
         "\"/><string key=\"g\" value=\"G\"/><date key=\"time:timestamp\" value=\"2020-01-01T10:00:00\"/><" +
         n_tag + " key=\"n\" value=\"" + n_value + "\"/></event>";
}

std::string trace(const char* name, const std::string& events) {
  return std::string("<trace><string key=\"concept:name\" value=\"") + name + "\"/>" + events + "</trace>";
}

int interleaved_frame() {
  const auto log = xes_upload("<?xml version=\"1.0\" encoding=\"UTF-8\"?><log xes.version=\"1.0\">" +
                              trace("c1", event("a", "R1", "int", "3")) + trace("c2", event("x", "R2", "string", "x")) +
                              trace("c1", event("b", "R3", "float", "2.5")) + trace("c2", event("y", "R4", "int", "1")) +
                              "</log>");
  int failures = 0;
  // sorted() of n's cells in file order [3, 'x', 2.5, 1] (by case it would
  // be [3, 2.5, 'x', 1]: 'str' and 'float').
  AttributeKdeOptions kde;
  kde.attribute_key = "n";
  kde.variant = AttributeKdeVariant::kNumeric;
  std::string message;
  try {
    (void)discover_attribute_kde(log, kde);
  } catch (const std::invalid_argument& error) {
    message = error.what();
  }
  if (message != "TypeError: '<' not supported between instances of 'str' and 'int'") {
    ++failures;
    std::cerr << "interleaved KDE: " << message << "\n";
  }
  // Links within group g follow the rows' order at one instant: R1 -> R2
  // (a), R2 -> R3 (x), R3 -> R4 (b).
  NetworkAnalysisOptions network;
  network.out_relation_attribute = "g";
  network.in_relation_attribute = "g";
  const auto result = discover_network_analysis(log, network);
  std::vector<std::tuple<std::string, std::string, std::string, std::uint64_t>> edges;
  for (const auto& edge : result.edges) edges.emplace_back(edge.source, edge.target, edge.activity, edge.count);
  std::sort(edges.begin(), edges.end());
  const decltype(edges) expected = {{"R1", "R2", "a", 1}, {"R2", "R3", "x", 1}, {"R3", "R4", "b", 1}};
  if (edges != expected) {
    ++failures;
    std::cerr << "interleaved network:";
    for (const auto& [source, target, activity, count] : edges)
      std::cerr << " " << source << ">" << target << ":" << activity << "=" << count;
    std::cerr << "\n";
  }
  return failures;
}

std::string sna_event(const char* activity, int minute, const char* tag, const char* value) {
  std::string text = std::string("<event><string key=\"concept:name\" value=\"") + activity +
                     "\"/><date key=\"time:timestamp\" value=\"2020-01-01T00:0" + std::to_string(minute) +
                     ":00\"/>";
  if (tag != nullptr) text += std::string("<") + tag + " key=\"org:resource\" value=\"" + value + "\"/>";
  return text + "</event>";
}

int interleaved_sna() {
  // Cases first seen c2, c0, c1: the sequence sort meets (NaN, dt) after
  // (NaN, 5, 5), a datetime beside an int, and pandas' Categorical fallback
  // refuses position 1. In the groupby's sorted case order it would not.
  const char* date = "2021-01-01T00:00:00";
  const auto log = xes_upload(
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?><log xes.version=\"1.0\">" +
      trace("c2", sna_event("c", 1, nullptr, nullptr) + sna_event("c", 2, "int", "5") + sna_event("c", 3, "int", "5")) +
      trace("c0", sna_event("a", 4, nullptr, nullptr)) +
      trace("c1", sna_event("a", 5, "int", "5") + sna_event("c", 6, "date", date) + sna_event("c", 7, nullptr, nullptr)) +
      trace("c0", sna_event("c", 8, "date", date)) + trace("c1", sna_event("c", 9, "int", "7")) + "</log>");
  int failures = 0;
  for (const auto variant : {OrganizationalSnaVariant::kWorkingTogetherPandas, OrganizationalSnaVariant::kHandoverPandas,
                             OrganizationalSnaVariant::kSubcontractingPandas}) {
    OrganizationalSnaOptions options;
    options.variant = variant;
    std::string message;
    try {
      (void)discover_organizational_sna(log, options);
    } catch (const std::invalid_argument& error) {
      message = error.what();
    }
    if (message == "TypeError: 'values' is not ordered, please explicitly specify the categories order by passing in "
                   "a categories argument.")
      continue;
    ++failures;
    std::cerr << "interleaved SNA " << static_cast<int>(variant) << ": " << message << "\n";
  }
  // Joint activities (pandas) sorts the resources of every row with one,
  // also a row without an activity: 7 beside two dates raises.
  const auto joint = xes_upload(
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?><log xes.version=\"1.0\">" +
      trace("c0", sna_event("a", 1, "int", "7") +
                      "<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" "
                      "value=\"2020-01-01T00:02:00\"/><date key=\"org:resource\" value=\"2021-01-01T00:00:00\"/>"
                      "<string key=\"x\" value=\"a\"/></event><event><string key=\"concept:name\" value=\"a\"/>"
                      "<date key=\"time:timestamp\" value=\"2020-01-01T00:03:00\"/><date key=\"org:resource\" "
                      "value=\"2020-01-01T00:00:00\"/><string key=\"x\" value=\"b\"/></event>") +
      "</log>");
  OrganizationalSnaOptions options;
  options.variant = OrganizationalSnaVariant::kJointActivitiesPandas;
  options.activity_attribute = "x";
  std::string message;
  try {
    (void)discover_organizational_sna(joint, options);
  } catch (const std::invalid_argument& error) {
    message = error.what();
  }
  if (message != "TypeError: '<' not supported between instances of 'datetime.datetime' and 'int'") {
    ++failures;
    std::cerr << "joint activities: " << message << "\n";
  }
  return failures;
}

int clock_cells() {
  constexpr std::optional<long long> kNone;
  // pandas.to_datetime(format="mixed", utc=True) of the cell, as UTC seconds.
  const std::vector<std::pair<const char*, std::optional<long long>>> cases = {
      // A six-digit date (DDMMYY-like as dateutil splits it) before a clock.
      {"200113 10:30", 1358677800LL}, {"250699T0730", 930295800LL}, {"991231 23:59:59", 946684799LL},
      {"250677 07:05:01", 236070301LL}, {"991231t23:59", 946684740LL}, {"200113  10:30", 1358677800LL},
      // A clock before the date.
      {"10:30 2020-01-05", 1578220200LL}, {"7:05 PM 05/06/2020", 1588791900LL},
      {"10:30, 2020.01.05", 1578220200LL}, {"23:59:59 2019-12-31 Z", 1577836799LL},
      {"9:00am 2020/02/29", 1582966800LL}, {"1:2 2020 1 5", 1578186120LL},
      {"11:59 pm 1999-12-31 +02:00", 946677540LL}, {"10:30 2020 01 05Z", 1578220200LL},
      {"10:30,, 2020-01-05", 1578220200LL}, {"10:30:00pm, 2020-07-31Z", 1596234600LL},
      {"7:00:00 1960 07 05Z", -299523600LL}, {"10:30 2020 01 05+01:00", 1578216600LL},
      {"10:30 05.06.2020-0500", 1588779000LL}, {"10:30 2020-01-05-0500", 1578238200LL},
      {"7:05PM,05/06/2020", 1588791900LL}, {"10:30:15.5,2020-01-05", 1578220215LL},
      {"10:30 ,, 2020-01-05", 1578220200LL},
      // pandas refuses these.
      {"10:30T2020-01-05", kNone}, {"10:30 31/5-1999", kNone}, {"20011 10:30", kNone}, {"2001134 10:30", kNone},
      {"10:30 2020-01-05T", kNone}, {"10:30 05 06 2020Z", kNone}, {"10:30 05 06 1999+01:00", kNone},
      {"10:30 2020 01 05-0500", kNone},
      // Declared: pandas reads these as a clock on today's date ("10:30,2020"
      // is 10:30:12.12), or reads a `T` before the date when an offset
      // follows ("10:30T2020-01-05 +01:00"; "PMT" is then a zone name).
      {"10:30,2020-01-05", kNone}, {"10:30:15,2020-01-05", kNone}, {"10:30T2020-01-05 +01:00", kNone},
      {"10:30 PMT2020-01-05 +01:00", kNone},
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

int clock_upload() {
  std::istringstream csv(
      "case:concept:name,concept:name,time:timestamp\n"
      "c1,a,200113 10:30\n"
      "c1,b,10:30 2020-01-05\n"
      "c1,c,\"7:05 PM, 05/06/2020\"\n");
  CsvImportOptions import;
  import.case_id_column = "case:concept:name";
  import.activity_column = "concept:name";
  import.timestamp_column = "time:timestamp";
  const auto log = import_csv_event_log(csv, import);
  const std::vector<std::pair<std::string, long long>> expected = {
      {"a", 1358677800LL}, {"b", 1578220200LL}, {"c", 1588791900LL}};
  std::vector<std::pair<std::string, long long>> seen;
  for (const auto& trace : log.traces)
    for (const auto& event : trace.events) {
      const auto exact = parse_timestamp_exact(event.timestamp);
      seen.emplace_back(event.activity, exact ? exact->seconds : -1);
    }
  if (seen == expected) return 0;
  std::cerr << "clock upload:";
  for (const auto& [activity, seconds] : seen) std::cerr << " " << activity << "@" << seconds;
  std::cerr << "\n";
  return 1;
}

}  // namespace

int main() {
  int failures = 0;
  failures += sort_cases();
  failures += interleaved_frame();
  failures += interleaved_sna();
  failures += clock_cells();
  failures += clock_upload();
  if (failures != 0) {
    std::cerr << failures << " review BF1 failure(s)\n";
    return 1;
  }
  std::cout << "review BF1 tests passed\n";
  return 0;
}
