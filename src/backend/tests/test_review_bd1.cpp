// Review BD1: the trace-based default selection of an XES trace attribute
// of numbers, bools and gaps (the first element of the Python set of the
// cases' cells, as for event columns), the workload metrics without a start
// column on an XES object timestamp column (the per-case shift is float64
// when every shifted cell is a number, and the intervals are converted in
// the order of a second sort by start and timestamp), the case-grouped SNA
// pandas variants on a CSV case column with gaps (a row without a case
// takes no part in the resource sort), and the event distribution on
// uploads (pm4py's DataFrame statistics: NaT makes the parts floats, the
// frame's datetimes are UTC, pandas' errors). Values and messages are taken
// from pm4py 2.7.22 black-box on the same texts (pm4py.read_xes, or
// read_csv and format_dataframe).
#include "minera/attribute_distribution.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/dataframe_columns.hpp"
#include "minera/log_to_features.hpp"
#include "minera/organizational_mining.hpp"
#include "minera/resource_profiles.hpp"
#include "minera/xes_event_log_importer.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
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
  import.resource_column = "org:resource";
  auto log = import_csv_event_log(csv, import);
  assert(log.dataframe_columns);
  return log;
}

std::string message_of(const std::function<void()>& call) {
  try {
    call();
  } catch (const std::invalid_argument& error) {
    return error.what();
  }
  return "";
}

// One trace a cell of the trace attribute f (empty: absent).
std::string trace_xes(const std::vector<std::string>& cells) {
  std::string out = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><log xes.version=\"1.0\">";
  for (std::size_t i = 0; i < cells.size(); ++i)
    out += "<trace><string key=\"concept:name\" value=\"c" + std::to_string(i) + "\"/>" + cells[i] +
           "<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2020-01-01T00:0" +
           std::to_string(i) + ":00\"/></event></trace>";
  return out + "</log>";
}

int trace_selection() {
  struct Case {
    std::vector<std::string> cells;
    bool numeric;
  };
  // [True, 0.5, 2.0]: 0.5 hashes 2**60, slot 0, so numeric; [True, 2.5]
  // and [True, 3]: True (slot 1) first; [8, True]: 8 (slot 0) first; [16,
  // True, False]: False probes past 16 and True. (A gap's NaN hashes by its
  // address, declared since BC, so no case here has one.)
  const std::vector<Case> cases = {
      {{"<boolean key=\"f\" value=\"true\"/>", "<float key=\"f\" value=\"0.5\"/>", "<float key=\"f\" value=\"2.0\"/>"}, true},
      {{"<boolean key=\"f\" value=\"true\"/>", "<float key=\"f\" value=\"2.5\"/>"}, false},
      {{"<boolean key=\"f\" value=\"true\"/>", "<int key=\"f\" value=\"3\"/>"}, false},
      {{"<int key=\"f\" value=\"16\"/>", "<boolean key=\"f\" value=\"true\"/>", "<boolean key=\"f\" value=\"false\"/>"}, true},
      {{"<int key=\"f\" value=\"8\"/>", "<boolean key=\"f\" value=\"true\"/>"}, true},
  };
  int failures = 0;
  for (const auto& entry : cases) {
    const auto log = xes_upload(trace_xes(entry.cells));
    const auto features = trace_based_log_features(log, {});
    const bool has = std::find(features.feature_names.begin(), features.feature_names.end(), "trace:f") !=
                     features.feature_names.end();
    if (has != entry.numeric) {
      ++failures;
      std::cerr << "trace selection of " << entry.cells.front() << "...:";
      for (const auto& name : features.feature_names) std::cerr << " " << name;
      std::cerr << "\n";
    }
  }
  return failures;
}

struct Row {
  const char* case_id;
  const char* resource;
  const char* kind;
  const char* value;
};

std::string object_xes(const std::vector<Row>& rows) {
  std::string out = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><log xes.version=\"1.0\">";
  std::string last;
  int minute = 0;
  for (const auto& row : rows) {
    if (row.case_id != last) {
      if (!last.empty()) out += "</trace>";
      out += std::string("<trace><string key=\"concept:name\" value=\"") + row.case_id + "\"/>";
      last = row.case_id;
    }
    ++minute;
    out += std::string("<event><string key=\"concept:name\" value=\"a\"/><string key=\"res\" value=\"") + row.resource +
           "\"/><date key=\"time:timestamp\" value=\"2020-01-01T00:" + (minute < 10 ? "0" : "") +
           std::to_string(minute) + ":00\"/><" + row.kind + " key=\"mo\" value=\"" + row.value + "\"/></event>";
  }
  return out + "</trace></log>";
}

int workload_order() {
  struct Case {
    std::vector<Row> rows;
    const char* resource;
    const char* type;
  };
  const std::vector<Case> cases = {
      // The open case of round BC (objtime.py seed 279): c1 sorts [1, 'x'],
      // the shifted column [NaN, NaN, 1] is float64, and the second sort puts
      // c1's 'x' (start 1.0) before c0's 'x'.
      {{{"c0", "R", "string", "x"}, {"c1", "R", "string", "x"}, {"c1", "S", "int", "1"}}, "R", "float"},
      // The second sort puts numbers before text across cases.
      {{{"c0", "S", "string", "x"}, {"c1", "S", "int", "3"}}, "S", "int"},
      {{{"c0", "S", "string", "x"}, {"c1", "S", "string", "y"}, {"c1", "R", "int", "3"}}, "S", "float"},
      // A case's first row keeps its own int.
      {{{"c0", "R", "string", "x"}, {"c0", "R", "int", "1"}}, "R", "int"},
      // A text cell among the shifted ones keeps the column object.
      {{{"c0", "S", "string", "y"}, {"c0", "S", "string", "z"}, {"c1", "S", "int", "2"}, {"c1", "R", "int", "3"}}, "R", "int"},
  };
  int failures = 0;
  for (const auto& entry : cases) {
    const auto log = xes_upload(object_xes(entry.rows));
    for (const auto* metric : {"average_workload", "multitasking"}) {
      ResourceProfilesOptions options;
      options.metric = resource_profile_metric_from_string(metric);
      options.resource_attribute = "res";
      options.timestamp_attribute = "mo";
      options.resource = entry.resource;
      options.t1 = "2019-06-01 00:00:00";
      options.t2 = "2021-01-01 00:00:00";
      const auto got = message_of([&] { (void)discover_resource_profiles(log, options); });
      const auto wanted = std::string("resource profiles: AttributeError: '") + entry.type +
                          "' object has no attribute 'timestamp'";
      if (got != wanted) {
        ++failures;
        std::cerr << metric << " " << entry.rows.size() << " rows: got " << got << "\n  wanted: " << wanted << "\n";
      }
    }
  }
  return failures;
}

int grouped_sna() {
  // Case column x; the second row has no case and no resource: it takes no
  // part in the resource sort. With a case it is a NaN beside text.
  const std::string text =
      "case:concept:name,concept:name,time:timestamp,org:resource,x\n"
      "c0,a,2020-01-01 00:01:00,R,k\nc0,b,2020-01-01 00:02:00,,\n"
      "c1,a,2020-01-01 00:03:00,5,k\nc1,b,2020-01-01 00:04:00,S,k\n";
  int failures = 0;
  const auto run = [](const EventLog& log, OrganizationalSnaVariant variant) {
    OrganizationalSnaOptions options;
    options.variant = variant;
    options.resource_attribute = "org:resource";
    options.case_id_attribute = "x";
    options.beta = 1.0;
    const auto result = discover_organizational_sna(log, options);
    std::map<std::pair<std::string, std::string>, double> edges;
    for (const auto& edge : result.edges)
      edges[{result.resources[edge.source], result.resources[edge.target]}] = edge.weight;
    return edges;
  };
  const auto close = [](const std::map<std::pair<std::string, std::string>, double>& got,
                        const std::map<std::pair<std::string, std::string>, double>& wanted) {
    if (got.size() != wanted.size()) return false;
    for (const auto& [pair, weight] : wanted) {
      const auto found = got.find(pair);
      if (found == got.end() || std::abs(found->second - weight) > 1e-12) return false;
    }
    return true;
  };
  const auto log = csv_upload(text);
  const double third = 1.0 / 3.0;
  if (!close(run(log, OrganizationalSnaVariant::kHandoverPandas), {{{"R", "5"}, third}, {{"R", "S"}, third}, {{"5", "S"}, third}})) {
    ++failures;
    std::cerr << "handover pandas on a case column with gaps\n";
  }
  if (!close(run(log, OrganizationalSnaVariant::kWorkingTogetherPandas),
             {{{"5", "R"}, 0.25}, {{"R", "5"}, 0.25}, {{"5", "S"}, 0.25}, {{"S", "5"}, 0.25}, {{"R", "S"}, 0.25}, {{"S", "R"}, 0.25}})) {
    ++failures;
    std::cerr << "working together pandas on a case column with gaps\n";
  }
  auto with_case = text;
  with_case.replace(with_case.find("00:02:00,,\n"), 11, "00:02:00,,k\n");
  const auto log2 = csv_upload(with_case);
  for (const auto variant : {OrganizationalSnaVariant::kHandoverPandas, OrganizationalSnaVariant::kWorkingTogetherPandas}) {
    const auto got = message_of([&] { (void)run(log2, variant); });
    if (got != "TypeError: '<' not supported between instances of 'float' and 'str'") {
      ++failures;
      std::cerr << "grouped SNA with a NaN resource: " << got << "\n";
    }
  }
  return failures;
}

int event_distribution() {
  const auto log = csv_upload(
      "case:concept:name,concept:name,time:timestamp,org:resource,t2,n\n"
      "c0,a,2020-01-01 10:00:00,R,2020-03-05 11:00:00,1\n"
      "c0,b,2020-01-02 11:00:00,R,,2\n"
      "c1,a,2020-02-01 12:00:00,S,2021-07-04 09:00:00+02:00,3\n");
  struct Case {
    EventDistributionType type;
    const char* key;
    std::vector<std::string> labels;
    std::vector<std::uint64_t> counts;
    const char* error;
  };
  std::vector<std::string> weeks;
  std::vector<std::uint64_t> week_counts;
  for (int i = 0; i <= 52; ++i) {
    weeks.push_back((i < 10 ? "0" : "") + std::to_string(i));
    week_counts.push_back(i == 10 || i == 26 ? 1 : 0);
  }
  const std::vector<Case> cases = {
      {EventDistributionType::kMonths, "t2", {"01", "02", "04", "05", "06", "08", "09", "10", "11", "12", "3.0", "7.0"},
       {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1}, ""},
      // The +02:00 cell is 07:00 UTC.
      {EventDistributionType::kHours, "t2",
       {"00", "01", "02", "03", "04", "05", "06", "08", "09", "10", "11.0", "12", "13", "14", "15", "16", "17", "18",
        "19", "20", "21", "22", "23", "7.0"},
       {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, ""},
      {EventDistributionType::kDaysWeek, "t2", {}, {}, "KeyError: '3.0'"},
      {EventDistributionType::kYears, "t2", {}, {}, "TypeError: 'float' object cannot be interpreted as an integer"},
      {EventDistributionType::kWeeks, "t2", weeks, week_counts, ""},
      {EventDistributionType::kDaysWeek, "time:timestamp",
       {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"}, {0, 0, 1, 1, 0, 1, 0}, ""},
      {EventDistributionType::kYears, "time:timestamp", {"2020"}, {3}, ""},
      {EventDistributionType::kMonths, "n", {}, {}, "AttributeError: Can only use .dt accessor with datetimelike values"},
      {EventDistributionType::kMonths, "zz", {}, {}, "KeyError: 'zz'"},
  };
  int failures = 0;
  for (const auto& entry : cases) {
    AttributeDistributionOptions options;
    options.distribution = entry.type;
    options.timestamp_key = entry.key;
    try {
      const auto result = discover_event_distribution(log, options);
      if (std::string(entry.error).empty() && result.labels == entry.labels && result.counts == entry.counts) continue;
      ++failures;
      std::cerr << event_distribution_name(entry.type) << " " << entry.key << ":";
      for (std::size_t i = 0; i < result.labels.size(); ++i) std::cerr << " " << result.labels[i] << "=" << result.counts[i];
      std::cerr << "\n";
    } catch (const std::invalid_argument& error) {
      if (error.what() == std::string(entry.error)) continue;
      ++failures;
      std::cerr << event_distribution_name(entry.type) << " " << entry.key << ": " << error.what() << "\n";
    }
  }
  return failures;
}

}  // namespace

int main() {
  int failures = 0;
  failures += trace_selection();
  failures += workload_order();
  failures += grouped_sna();
  failures += event_distribution();
  if (failures != 0) {
    std::cerr << failures << " review BD1 failure(s)\n";
    return 1;
  }
  std::cout << "review BD1 tests passed\n";
  return 0;
}
