// Review BC1: SNA on XES uploads whose resource column is a datetime column
// with gaps (pm4py keeps the missing cells as pd.NaT, one resource "NaT", in
// every variant but the joint-activities pandas one, which drops them), the
// SNA errors' exact texts (the sort's Python type names, the case-grouped
// pandas variants' 'Column not found' and ordered-Categorical errors,
// scipy's pearsonr message), and resource profiles on XES object columns
// (the case metrics read each case's last timestamp, average_case_duration
// takes each group's least timestamp and windows by least and greatest,
// average_duration_activity subtracts two object columns cell by cell),
// network analysis' sort and groupby errors on an XES object column, and
// the trace-based default selection of a column of numbers, bools and gaps
// (pm4py types it by the first element of the Python set of its cells).
// Values and messages are taken from pm4py 2.7.22 black-box on the same XES
// text (pm4py.read_xes, then the organizational and resource-profile
// algorithms).
#include "minera/dataframe_columns.hpp"
#include "minera/log_to_features.hpp"
#include "minera/network_analysis.hpp"
#include "minera/organizational_mining.hpp"
#include "minera/resource_profiles.hpp"
#include "minera/xes_event_log_importer.hpp"
#include "../python_scalar.hpp"

#include <cassert>
#include <algorithm>
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

int expect_message(const std::function<void()>& call, const std::string& expected) {
  try {
    call();
  } catch (const std::invalid_argument& error) {
    if (error.what() == expected) return 0;
    std::cerr << "unexpected message: " << error.what() << "\n  wanted: " << expected << "\n";
    return 1;
  }
  std::cerr << "no error, wanted: " << expected << "\n";
  return 1;
}

constexpr const char* kD1 = "2020-01-01 00:00:00+00:00";
constexpr const char* kD2 = "2021-01-01 00:00:00+00:00";

// One trace a group of (activity, resource) events; an empty resource is
// a missing org:resource; "true" is a boolean, "5" an int, other values
// dates.
std::string resource_xes(const std::vector<std::vector<std::pair<std::string, std::string>>>& traces) {
  std::string out = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><log xes.version=\"1.0\">";
  int n = 0;
  for (std::size_t c = 0; c < traces.size(); ++c) {
    out += "<trace><string key=\"concept:name\" value=\"c" + std::to_string(c) + "\"/>";
    for (const auto& [activity, resource] : traces[c]) {
      const auto minute = std::to_string(n++);
      out += "<event><string key=\"concept:name\" value=\"" + activity +
             "\"/><date key=\"time:timestamp\" value=\"2020-01-01T00:" + (minute.size() < 2 ? "0" : "") + minute +
             ":00\"/>";
      if (resource == "true") out += "<boolean key=\"org:resource\" value=\"true\"/>";
      else if (resource == "5") out += "<int key=\"org:resource\" value=\"5\"/>";
      else if (!resource.empty()) out += "<date key=\"org:resource\" value=\"" + resource + "\"/>";
      out += "</event>";
    }
    out += "</trace>";
  }
  return out + "</log>";
}

std::map<std::pair<std::string, std::string>, double> network(const EventLog& log, OrganizationalSnaVariant variant,
                                                              double beta = 0.0, std::size_t n = 2) {
  OrganizationalSnaOptions options;
  options.variant = variant;
  options.beta = beta;
  options.n = n;
  const auto result = discover_organizational_sna(log, options);
  std::map<std::pair<std::string, std::string>, double> edges;
  for (const auto& edge : result.edges)
    edges[{result.resources[edge.source], result.resources[edge.target]}] = edge.weight;
  return edges;
}

int expect_network(const std::string& label, const std::map<std::pair<std::string, std::string>, double>& got,
                   const std::map<std::pair<std::string, std::string>, double>& wanted) {
  bool same = got.size() == wanted.size();
  for (const auto& [pair, weight] : wanted) {
    const auto it = got.find(pair);
    same = same && it != got.end() && std::abs(it->second - weight) <= 1e-12;
  }
  if (same) return 0;
  std::cerr << label << ": got";
  for (const auto& [pair, weight] : got) std::cerr << " (" << pair.first << ", " << pair.second << "): " << weight;
  std::cerr << "\n";
  return 1;
}

int nat_resources() {
  const auto D1 = "2020-01-01T00:00:00", D2 = "2021-01-01T00:00:00";
  const auto log = xes_upload(resource_xes({{{"a", ""}, {"b", D1}, {"c", ""}}, {{"a", D2}, {"b", ""}, {"c", D2}}}));
  int failures = 0;
  const double sixth = 1.0 / 6.0;
  // pm4py: {('NaT', D1): 1/6, ('NaT', 'NaT'): 1/6, ('NaT', D2): 1/6, (D1, 'NaT'): 1/6, (D2, 'NaT'): 1/6, (D2, D2): 1/6}
  const std::map<std::pair<std::string, std::string>, double> handover = {
      {{"NaT", kD1}, sixth}, {{"NaT", "NaT"}, sixth}, {{"NaT", kD2}, sixth},
      {{kD1, "NaT"}, sixth}, {{kD2, "NaT"}, sixth},   {{kD2, kD2}, sixth}};
  failures += expect_network("handover", network(log, OrganizationalSnaVariant::kHandover, 1.0), handover);
  failures += expect_network("handover pandas", network(log, OrganizationalSnaVariant::kHandoverPandas, 1.0), handover);
  failures += expect_network("working together", network(log, OrganizationalSnaVariant::kWorkingTogether),
                             {{{"NaT", kD1}, 0.5}, {{kD1, "NaT"}, 0.5}, {{"NaT", kD2}, 0.5}, {{kD2, "NaT"}, 0.5}});
  failures += expect_network("working together pandas", network(log, OrganizationalSnaVariant::kWorkingTogetherPandas),
                             {{{"NaT", kD1}, sixth}, {{kD1, "NaT"}, sixth}, {{"NaT", kD2}, sixth}, {{kD2, "NaT"}, sixth}});
  failures += expect_network("subcontracting", network(log, OrganizationalSnaVariant::kSubcontracting, 0.0, 2),
                             {{{"NaT", kD1}, 0.5}, {{kD2, "NaT"}, 0.5}});
  failures += expect_network("subcontracting pandas", network(log, OrganizationalSnaVariant::kSubcontractingPandas, 0.0, 2),
                             {{{"NaT", kD1}, 0.5}, {{kD2, "NaT"}, 0.5}});
  failures += expect_network("joint activities pandas", network(log, OrganizationalSnaVariant::kJointActivitiesPandas),
                             {{{kD1, kD2}, -1.0}, {{kD2, kD1}, -1.0}});
  // One activity: pearsonr refuses the vectors (D1 and NaT are two resources).
  const auto single = xes_upload(resource_xes({{{"c", D1}, {"c", ""}, {"c", ""}}}));
  failures += expect_message([&] { (void)network(single, OrganizationalSnaVariant::kJointActivities); },
                             "ValueError: `x` and `y` must have length at least 2.");
  return failures;
}

int sna_messages() {
  const auto D1 = "2020-01-01T00:00:00";
  int failures = 0;
  const std::string categorical =
      "TypeError: 'values' is not ordered, please explicitly specify the categories order by passing in a categories "
      "argument.";
  // (True,) and (date,): their tuples do not compare, nor does the first
  // position's bool beside a date.
  const auto split = xes_upload(resource_xes({{{"a1", "true"}}, {{"a0", D1}}}));
  for (const auto variant : {OrganizationalSnaVariant::kHandoverPandas, OrganizationalSnaVariant::kWorkingTogetherPandas,
                             OrganizationalSnaVariant::kSubcontractingPandas})
    failures += expect_message([&] { (void)network(split, variant); }, categorical);
  // One variant (True, date) twice: its tuples compare, and the sorted
  // resources raise the plain TypeError (pm4py's pair order follows the
  // hash seed: 'datetime.datetime' and 'bool' here).
  const auto twice = xes_upload(resource_xes({{{"a1", "true"}, {"a0", D1}}, {{"a1", "true"}, {"a0", D1}}}));
  failures += expect_message([&] { (void)network(twice, OrganizationalSnaVariant::kHandoverPandas); },
                             "TypeError: '<' not supported between instances of 'datetime.datetime' and 'bool'");
  // An int and a date: Python's names, not float and Timestamp (the pair's
  // order is Minera's, first appearance; pm4py's follows the hash seed).
  const auto mixed = xes_upload(resource_xes({{{"a1", "5"}, {"a0", D1}}}));
  failures += expect_message([&] { (void)network(mixed, OrganizationalSnaVariant::kWorkingTogether); },
                             "TypeError: '<' not supported between instances of 'datetime.datetime' and 'int'");
  // A resource column the frame lacks: the case-grouped pandas variants
  // group by the case column first, then select the resource column.
  const auto log = xes_upload(resource_xes({{{"a", ""}, {"b", ""}}}));
  failures += expect_message([&] { (void)network(log, OrganizationalSnaVariant::kHandoverPandas); },
                             "KeyError: 'Column not found: org:resource'");
  failures += expect_message([&] { (void)network(log, OrganizationalSnaVariant::kHandover); }, "KeyError: 'org:resource'");
  failures += expect_message([&] { (void)network(log, OrganizationalSnaVariant::kJointActivitiesPandas); },
                             "KeyError: 'org:resource'");
  failures += expect_message(
      [&] {
        OrganizationalSnaOptions options;
        options.variant = OrganizationalSnaVariant::kWorkingTogetherPandas;
        options.case_id_attribute = "idk";
        (void)discover_organizational_sna(log, options);
      },
      "KeyError: 'idk'");
  return failures;
}

// Network analysis on an XES object column of dates and bools: the sort's
// and the groupby's TypeErrors name its dates datetime.datetime, as pm4py's
// do (the frame holds them as datetime.datetime objects).
int network_analysis_messages() {
  const auto D1 = "2020-01-01T00:00:00";
  const auto run = [](const EventLog& log, const std::string& sorting) {
    NetworkAnalysisOptions options;
    options.sorting_attribute = sorting;
    (void)discover_network_analysis(log, options);
  };
  int failures = 0;
  const auto date_bool = xes_upload(resource_xes({{{"a", D1}, {"b", "true"}, {"c", D1}}}));
  failures += expect_message([&] { run(date_bool, "time:timestamp"); },
                             "network analysis: TypeError: '<' not supported between instances of 'bool' and "
                             "'datetime.datetime'");
  const auto bool_date = xes_upload(resource_xes({{{"a", "true"}, {"b", D1}, {"c", "true"}}}));
  failures += expect_message([&] { run(bool_date, "time:timestamp"); },
                             "network analysis: TypeError: '<' not supported between instances of "
                             "'datetime.datetime' and 'bool'");
  failures += expect_message([&] { run(date_bool, "org:resource"); },
                             "network analysis: TypeError: '<' not supported between instances of 'bool' and "
                             "'datetime.datetime'");
  return failures;
}

// CPython's set order of numbers (list(set(values)), each element as the
// index of its first equal value) and the selection it drives.
int set_order_selection() {
  int failures = 0;
  using python::boolean;
  using python::integer;
  using python::real;
  const std::vector<std::pair<std::vector<python::Scalar>, std::vector<std::size_t>>> orders = {
      {{real(1.5), boolean(true), real(2.5), boolean(false)}, {3, 0, 2, 1}},
      {{real(2.5), boolean(true), real(1.5), real(2.0)}, {1, 0, 3, 2}},
      // Two resizes (8, 32, then 64 slots), a hash past the modulus (2**61
      // hashes as 1, beside 7) and -1's hash -2.
      {{integer(7), real(0.5), integer(-1), python::unsigned_integer(std::uint64_t(1) << 61), real(3.25), boolean(true),
        integer(100), integer(9), real(1e20), integer(0), integer(17), integer(-2)},
       {1, 3, 5, 4, 6, 9, 0, 7, 8, 10, 11, 2}},
  };
  for (const auto& [values, wanted] : orders)
    if (python::number_set_order(values) != wanted) {
      ++failures;
      std::cerr << "set order:";
      for (const auto index : python::number_set_order(values)) std::cerr << " " << index;
      std::cerr << "\n";
    }
  // f1 [1.5, True | 2.5, False]: False comes first, so no feature; f2
  // [1.5, 2.0 | 2.5, True]: 1.5 first, a numeric feature.
  std::string xes = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><log xes.version=\"1.0\">";
  const std::vector<std::vector<std::pair<std::string, std::string>>> cells = {
      {{"<float key=\"f1\" value=\"1.5\"/>", "<float key=\"f2\" value=\"1.5\"/>"},
       {"<boolean key=\"f1\" value=\"true\"/>", "<float key=\"f2\" value=\"2.0\"/>"}},
      {{"<float key=\"f1\" value=\"2.5\"/>", "<float key=\"f2\" value=\"2.5\"/>"},
       {"<boolean key=\"f1\" value=\"false\"/>", "<boolean key=\"f2\" value=\"true\"/>"}}};
  int minute = 0;
  for (std::size_t c = 0; c < cells.size(); ++c) {
    xes += "<trace><string key=\"concept:name\" value=\"c" + std::to_string(c) + "\"/>";
    for (const auto& [f1, f2] : cells[c])
      xes += "<event><string key=\"concept:name\" value=\"a\"/><date key=\"time:timestamp\" value=\"2020-01-01T00:0" +
             std::to_string(minute++) + ":00\"/>" + f1 + f2 + "</event>";
    xes += "</trace>";
  }
  const auto log = xes_upload(xes + "</log>");
  const auto features = trace_based_log_features(log, {});
  const auto has = [&](const std::string& name) {
    return std::find(features.feature_names.begin(), features.feature_names.end(), name) !=
           features.feature_names.end();
  };
  if (has("event:f1") || !has("event:f2")) {
    ++failures;
    std::cerr << "selection:";
    for (const auto& name : features.feature_names) std::cerr << " " << name;
    std::cerr << "\n";
  }
  return failures;
}

// Resource-profile uploads: each event token holds the mo and ms cells
// (D, E, G dates, S text, I int, B bool, _ missing) and the resource;
// traces are separated by '/'.
std::string object_xes(const std::string& spec) {
  const std::map<char, std::pair<std::string, std::string>> cells = {
      {'D', {"date", "2020-01-01T08:00:00"}}, {'E', {"date", "2020-06-01T08:00:00"}},
      {'G', {"date", "2020-03-01T08:00:00"}}, {'S', {"string", "x"}},
      {'I', {"int", "1"}},                    {'B', {"boolean", "true"}}};
  std::string out = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><log xes.version=\"1.0\">";
  int n = 1, c = 0;
  std::stringstream traces(spec);
  for (std::string trace; std::getline(traces, trace, '/');) {
    out += "<trace><string key=\"concept:name\" value=\"c" + std::to_string(c++) + "\"/>";
    std::stringstream tokens(trace);
    for (std::string token; std::getline(tokens, token, ',');) {
      const auto minute = std::to_string(n++);
      out += "<event><string key=\"concept:name\" value=\"a\"/><string key=\"res\" value=\"" + token.substr(2, 1) +
             "\"/><date key=\"time:timestamp\" value=\"2020-01-01T00:" + (minute.size() < 2 ? "0" : "") + minute +
             ":00\"/>";
      for (const auto& [key, ch] : {std::pair<std::string, char>{"mo", token[0]}, {"ms", token[1]}})
        if (const auto it = cells.find(ch); it != cells.end())
          out += "<" + it->second.first + " key=\"" + key + "\" value=\"" + it->second.second + "\"/>";
      out += "</event>";
    }
    out += "</trace>";
  }
  return out + "</log>";
}

struct ProfileCase {
  const char* metric;
  const char* spec;
  const char* start;
  const char* resource;
  const char* t1;
  const char* t2;
  double value;
  const char* error;
};

int object_columns() {
  const char* kWindow1 = "2019-06-01 00:00:00";
  const char* kWindow2 = "2021-01-01 00:00:00";
  const std::vector<ProfileCase> cases = {
      // groupby(case).last(): the last present cell of each case.
      {"case_completions", "D_R,S_R", "", "R", kWindow1, kWindow2, 0, "resource profiles: TypeError: '>=' not supported between instances of 'str' and 'datetime.datetime'"},
      {"case_completions", "S_R,D_R", "", "R", kWindow1, kWindow2, 1, ""},
      {"case_completions", "D_R/S_R,I_R", "", "R", kWindow1, kWindow2, 0, "resource profiles: TypeError: '>=' not supported between instances of 'int' and 'datetime.datetime'"},
      {"fraction_case_completions", "S_R,D_R/D_S", "", "R", kWindow1, kWindow2, 0.5, ""},
      // Each group's minimum: dates beside text have none; a number or a
      // text minimum fails the window comparison.
      {"average_case_duration", "D_R,S_R", "", "R", kWindow1, kWindow2, 0, "resource profiles: TypeError: agg function failed [how->min,dtype->object]"},
      {"average_case_duration", "D_R/I_R,B_R", "", "R", kWindow1, kWindow2, 0, "resource profiles: TypeError: '<' not supported between instances of 'int' and 'datetime.datetime'"},
      {"average_case_duration", "D_R,S_R", "", "Q", kWindow1, kWindow2, 0, "resource profiles: AttributeError: Can only use .dt accessor with datetimelike values"},
      {"average_case_duration", "D_R/S_S", "", "R", kWindow1, kWindow2, 0, "resource profiles: AttributeError: Can only use .dt accessor with datetimelike values"},
      // An unsorted case (June, January, March): the window takes its least
      // and greatest timestamps, the duration its last less its first.
      {"average_case_duration", "E_R,D_R,G_R", "", "R", "2019-01-01 00:00:00", "2020-05-01 00:00:00", -7948800.0, ""},
      {"average_case_duration", "E_R,D_R,G_R", "", "R", "2020-04-01 00:00:00", kWindow2, -7948800.0, ""},
      {"average_case_duration", "E_R,D_R,G_R", "", "R", "2020-07-01 00:00:00", kWindow2, 0, "resource profiles: mean requires at least one data point"},
      // Two object columns subtract cell by cell over the resource's rows.
      {"average_duration_activity", "DIR,SDS", "ms", "R", kWindow1, kWindow2, 0, "resource profiles: TypeError: unsupported operand type(s) for -: 'datetime.datetime' and 'int'"},
      {"average_duration_activity", "DDR,SDS", "ms", "R", kWindow1, kWindow2, 0, "resource profiles: TypeError: cannot subtract DatetimeArray from ndarray[object]"},
  };
  int failures = 0;
  for (const auto& entry : cases) {
    const auto log = xes_upload(object_xes(entry.spec));
    ResourceProfilesOptions options;
    options.metric = resource_profile_metric_from_string(entry.metric);
    options.resource_attribute = "res";
    options.timestamp_attribute = "mo";
    options.start_timestamp_attribute = entry.start;
    options.resource = entry.resource;
    if (std::string(entry.metric) == "average_duration_activity") options.activity = "a";
    options.t1 = entry.t1;
    options.t2 = entry.t2;
    std::string got;
    try {
      const auto result = discover_resource_profiles(log, options);
      if (std::string(entry.error).empty() && result.rows.size() == 1 && result.rows[0].value == entry.value) continue;
      got = result.rows.size() == 1 ? std::to_string(result.rows[0].value) : std::to_string(result.rows.size()) + " rows";
    } catch (const std::invalid_argument& error) {
      if (error.what() == std::string(entry.error)) continue;
      got = error.what();
    }
    ++failures;
    std::cerr << entry.metric << " " << entry.spec << " " << entry.resource << ": got " << got << "\n  wanted: "
              << (std::string(entry.error).empty() ? std::to_string(entry.value) : entry.error) << "\n";
  }
  return failures;
}

}  // namespace

int main() {
  int failures = 0;
  failures += nat_resources();
  failures += sna_messages();
  failures += object_columns();
  failures += network_analysis_messages();
  failures += set_order_selection();
  if (failures != 0) {
    std::cerr << failures << " review BC1 failure(s)\n";
    return 1;
  }
  std::cout << "review BC1 tests passed\n";
  return 0;
}
