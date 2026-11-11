// Review AZ1: two-row uploads, where pandas reads a list of two names as the
// frame's row labels when one of them is no column (local diagnostics, the
// pandas roles; the per-resource duration check of resource profiles), the
// organizational routes reading a `case:` column per row (it may vary within
// a case), and CPython's year range in a naive datetime.timestamp(). Values
// and messages are taken from pm4py 2.7.22 black-box on read_csv +
// format_dataframe(case_id="cid", activity_key="act", timestamp_key="ts") of
// the same text, or from CPython under TZ=Europe/Berlin.
#include "../local_time.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/local_diagnostics.hpp"
#include "minera/network_analysis.hpp"
#include "minera/organizational_mining.hpp"
#include "minera/resource_profiles.hpp"
#include "minera/role_discovery.hpp"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace minera;

namespace {

EventLog upload(const std::string& text) {
  std::istringstream csv(text);
  CsvImportOptions import;
  import.case_id_column = "cid";
  import.activity_column = "act";
  import.timestamp_column = "ts";
  import.resource_column = "res";
  auto log = import_csv_event_log(csv, import);
  assert(log.dataframe_columns);
  return log;
}

void expect_error(const std::function<void()>& call, const std::string& expected) {
  try {
    call();
  } catch (const std::invalid_argument& error) {
    if (error.what() == expected) return;
    std::cerr << "unexpected message: " << error.what() << "\n  wanted: " << expected << "\n";
    assert(false);
  }
  std::cerr << "no error, wanted: " << expected << "\n";
  assert(false);
}

bool near(double a, double b) { return std::abs(a - b) <= 1e-12; }

// Two cases of one event each, no org:resource column.
const std::string kTwo =
    "cid,act,ts,res,grp,amount\n"
    "c0,b,2020-01-01 00:05:00,R,g,1\n"
    "c1,b,2020-01-02 05:01:00,T,,2\n";
const std::string kTwoGroups =
    "cid,act,ts,res,org:group\n"
    "c0,b,2020-01-01 00:05:00,R,g\n"
    "c1,b,2020-01-02 05:01:00,T,h\n";

void two_row_local_diagnostics() {
  const auto two = upload(kTwo);
  const auto diagnose = [&](const std::string& activity, const std::string& resource, const std::string& group,
                            std::map<std::string, std::vector<std::string>> roles = {}) {
    LocalDiagnosticsOptions options;
    options.activity_attribute = activity;
    options.resource_attribute = resource;
    if (!group.empty()) options.group_attribute = group;
    options.explicit_groups = std::move(roles);
    return discover_local_diagnostics(two, options);
  };
  // [concept:name, org:resource] keys the two rows; the group column is read next.
  expect_error([&] { (void)diagnose("", "", ""); }, "KeyError: 'org:group'");
  // Groups 'r' (member 'g' of "grp") and 'r' of "org:resource": 'g' has no activity.
  expect_error([&] { (void)diagnose("", "", "grp"); }, "ZeroDivisionError: float division by zero");
  expect_error([&] { (void)diagnose("", "P", "grp"); }, "IndexError: string index out of range");
  // A resource column: coverage reads the missing activity column.
  expect_error([&] { (void)diagnose("AB", "res", "grp"); }, "KeyError: 'AB'");
  expect_error([&] { (void)diagnose("", "RS", "grp", {{"Role 1", {"R"}}}); }, "KeyError: 'RS'");
  expect_error([&] { (void)diagnose("AB", "res", "grp", {{"Role 1", {"R"}}}); },
               "ZeroDivisionError: float division by zero");

  const auto grouped = upload(kTwoGroups);
  LocalDiagnosticsOptions options;
  auto result = discover_local_diagnostics(grouped, options);
  // pm4py: {'group_relative_focus': {'r': {'c': 1.0, 'o': 0.0}}, 'group_relative_stake': {'r': {'c': 1.0,
  // 'o': 0.0}}, 'group_coverage': {'r': {'o': 1.0}}, 'group_member_contribution': {'r': {'o': {'c': 1}}}}
  assert(result.group_relative_focus.size() == 1);
  assert(result.group_relative_focus.at("r").at("c") == 1.0 && result.group_relative_focus.at("r").at("o") == 0.0);
  assert(result.group_relative_stake.at("r").at("c") == 1.0 && result.group_relative_stake.at("r").at("o") == 0.0);
  assert(result.group_coverage.at("r").size() == 1 && result.group_coverage.at("r").at("o") == 1.0);
  assert(result.group_member_contribution.at("r").at("o").size() == 1);
  assert(result.group_member_contribution.at("r").at("o").at("c") == 1);
  options.resource_attribute = "xx";
  result = discover_local_diagnostics(grouped, options);
  // pm4py: focus {'r': {'c': 1.0, 'x': 0.0}, 'x': {'c': 0.0, 'x': 1.0}}, coverage {'r': {'o': 1.0},
  // 'x': {'x': 1.0}}, contribution {'r': {'o': {'c': 1}}, 'x': {'x': {'x': 1}}}
  assert(result.group_relative_focus.size() == 2);
  assert(result.group_relative_focus.at("r").at("c") == 1.0 && result.group_relative_focus.at("r").at("x") == 0.0);
  assert(result.group_relative_focus.at("x").at("c") == 0.0 && result.group_relative_focus.at("x").at("x") == 1.0);
  assert(result.group_coverage.at("r").at("o") == 1.0 && result.group_coverage.at("x").at("x") == 1.0);
  assert(result.group_member_contribution.at("x").at("x").at("x") == 1);
}

void two_row_durations_per_resource() {
  // pm4py: average_duration_activity(df, t1, t2, r, "b", resource_key="res") is 0.0 for R and for T
  // (each resource's one row); the two rows together would have no .dt accessor.
  const auto two = upload(kTwo);
  ResourceProfilesOptions options;
  options.metric = ResourceProfileMetric::kAverageDurationActivity;
  options.resource_attribute = "res";
  options.activity = "b";
  options.activity_given = true;
  options.t1 = "2019-06-01 00:00:00";
  options.t2 = "2021-01-01 00:00:00";
  const auto result = discover_resource_profiles(two, options);
  for (const auto& row : result.rows) assert(row.value == 0.0);
  options.resource = "Z";
  options.resource_given = true;
  expect_error([&] { (void)discover_resource_profiles(two, options); },
               "resource profiles: AttributeError: Can only use .dt accessor with datetimelike values");
}

// The pandas roles group by [resource, activity]: on two rows the key names
// are the labels, a label's first character a resource and its second an
// activity.
void two_row_pandas_roles() {
  const auto two = upload(kTwo);
  const auto roles = [&](const std::string& activity, const std::string& resource) {
    RoleDiscoveryOptions options;
    options.variant = RoleDiscoveryVariant::kPandas;
    options.activity_attribute = activity;
    options.resource_attribute = resource;
    std::map<std::vector<std::string>, std::map<std::string, std::size_t>> out;
    for (const auto& role : discover_roles(two, options).roles)
      for (const auto& member : role.resources) out[role.activities][member.name] = member.count;
    return out;
  };
  using R = std::map<std::vector<std::string>, std::map<std::string, std::size_t>>;
  // pm4py: [(['r'], {'o': 1}), (['o'], {'c': 1})]
  assert((roles("concept:name", "org:resource") == R{{{"r"}, {{"o", 1}}}, {{"o"}, {{"c", 1}}}}));
  assert((roles("concept:name", "res") == R{{{"b"}, {{"R", 1}, {"T", 1}}}}));
  assert((roles("act", "xy") == R{{{"y"}, {{"x", 1}}}, {{"c"}, {{"a", 1}}}}));
  assert((roles("co", "xo") == R{{{"o"}, {{"c", 1}, {"x", 1}}}}));
  assert((roles("ZZ", "ZZ") == R{{{"Z"}, {{"Z", 2}}}}));
  assert((roles("\xc3\xa4\xc3\xb6", "res") == R{{{"\xc3\xb6"}, {{"\xc3\xa4", 1}}}, {{"e"}, {{"r", 1}}}}));
  expect_error([&] { (void)roles("Q", "res"); }, "IndexError: string index out of range");
}

// case:region varies within both cases.
const std::string kRegions =
    "cid,act,ts,res,case:region,grp\n"
    "c0,a,2020-01-01 00:00:00,R,N,g1\n"
    "c0,b,2020-01-01 01:00:00,S,S,g2\n"
    "c0,c,2020-01-01 02:00:00,R,W,g1\n"
    "c1,a,2020-01-02 00:00:00,S,S,g2\n"
    "c1,b,2020-01-02 01:00:00,R,N,g1\n";

void case_columns_per_row() {
  const auto log = upload(kRegions);
  LocalDiagnosticsOptions diagnostics;
  diagnostics.resource_attribute = "case:region";
  diagnostics.group_attribute = "grp";
  const auto local = discover_local_diagnostics(log, diagnostics);
  // pm4py: focus {'g1': {'a': 0.5, 'b': 0.5, 'c': 1.0}, 'g2': {'a': 0.5, 'b': 0.5, 'c': 0.0}},
  // coverage {'g1': {'N': 2/3, 'W': 1/3}, 'g2': {'S': 1.0}}, contribution {'g1': {'N': {'a': 1, 'b': 1},
  // 'W': {'c': 1}}, 'g2': {'S': {'a': 1, 'b': 1}}}
  assert(local.group_relative_focus.at("g1").at("a") == 0.5 && local.group_relative_focus.at("g1").at("c") == 1.0);
  assert(local.group_relative_focus.at("g2").at("c") == 0.0);
  assert(near(local.group_coverage.at("g1").at("N"), 2.0 / 3.0) && local.group_coverage.at("g2").at("S") == 1.0);
  assert(local.group_member_contribution.at("g1").at("W").at("c") == 1);
  assert(!local.group_member_contribution.at("g1").contains("S"));

  NetworkAnalysisOptions network;
  network.source_node_attribute = "case:region";
  network.target_node_attribute = "case:region";
  const auto edges = discover_network_analysis(log, network);
  // pm4py: {('N', 'S'): {'a': 1}, ('S', 'N'): {'a': 1}, ('S', 'W'): {'b': 1}}
  std::map<std::string, std::uint64_t> counted;
  for (const auto& edge : edges.edges) counted[edge.source + ">" + edge.target + ":" + edge.activity] = edge.count;
  assert((counted == std::map<std::string, std::uint64_t>{{"N>S:a", 1}, {"S>N:a", 1}, {"S>W:b", 1}}));

  ResourceProfilesOptions profiles;
  profiles.resource_attribute = "case:region";
  profiles.t1 = "2019-01-01 00:00:00";
  profiles.t2 = "2021-01-01 00:00:00";
  // pm4py: distinct_activities N 2, S 2, W 1.
  std::map<std::string, double> distinct;
  for (const auto& row : discover_resource_profiles(log, profiles).rows) distinct[row.resource] = row.value;
  assert((distinct == std::map<std::string, double>{{"N", 2.0}, {"S", 2.0}, {"W", 1.0}}));

  const auto handover = [&](const std::string& resource, const std::string& case_id) {
    OrganizationalSnaOptions sna;
    sna.variant = OrganizationalSnaVariant::kHandoverPandas;
    sna.resource_attribute = resource;
    sna.case_id_attribute = case_id;
    const auto result = discover_organizational_sna(log, sna);
    std::map<std::string, double> weights;
    for (const auto& edge : result.edges)
      weights[result.resources.at(edge.source) + ">" + result.resources.at(edge.target)] = edge.weight;
    return weights;
  };
  // pm4py: {('S', 'N'): 1/3, ('S', 'W'): 1/3, ('N', 'S'): 1/3}
  const auto regions = handover("case:region", "case:concept:name");
  assert(regions.size() == 3 && near(regions.at("S>N"), 1.0 / 3.0) && near(regions.at("S>W"), 1.0 / 3.0) &&
         near(regions.at("N>S"), 1.0 / 3.0));
  // Cases keyed by the region of each row: pm4py {('S', 'S'): 0.5, ('R', 'R'): 0.5}
  const auto by_region = handover("res", "case:region");
  assert(by_region.size() == 2 && by_region.at("S>S") == 0.5 && by_region.at("R>R") == 0.5);

  RoleDiscoveryOptions roles;
  roles.variant = RoleDiscoveryVariant::kPandas;
  roles.resource_attribute = "case:region";
  // pm4py: [Activities: ['a', 'b'] Originators importance {'N': 2, 'S': 2},
  //         Activities: ['c'] Originators importance {'W': 1}]
  std::map<std::string, std::map<std::string, std::size_t>> found;
  for (const auto& role : discover_roles(log, roles).roles) {
    std::string name;
    for (const auto& activity : role.activities) name += activity;
    for (const auto& resource : role.resources) found[name][resource.name] = resource.count;
  }
  assert((found == std::map<std::string, std::map<std::string, std::size_t>>{
                       {"ab", {{"N", 2}, {"S", 2}}}, {"c", {{"W", 1}}}}));
}

void naive_timestamp_year_range() {
  // Windows reads the zone through its time-zone database (local_time.cpp),
  // whose data for year 1 and 9999 this does not assume.
#ifndef _WIN32
  setenv("TZ", "Europe/Berlin", 1);
  tzset();
  const auto refused = [](int y, int m, int d, int hh) -> std::int64_t {
    std::int64_t year = -1;
    const auto seconds = local_time::naive_timestamp(local_time::civil_seconds(y, m, d, hh, 0, 0), &year);
    assert(!seconds || year == -1);
    return seconds ? -1 : year;
  };
  // CPython: datetime(1, 1, 1).timestamp() raises "ValueError: year 0 is out of range" (the probe a
  // day earlier), datetime(9999, 12, 31, 23).timestamp() "year 10000 is out of range".
  assert(refused(1, 1, 1, 0) == 0);
  assert(refused(1, 1, 1, 1) == 0);
  assert(refused(9999, 12, 31, 23) == 10000);
  // datetime(1, 1, 2).timestamp() == -62135513608.0 (the local mean time +00:53:28).
  assert(*local_time::naive_timestamp(local_time::civil_seconds(1, 1, 2, 0, 0, 0)) == -62135513608);
#endif
}

}  // namespace

int main() {
  two_row_local_diagnostics();
  two_row_durations_per_resource();
  two_row_pandas_roles();
  case_columns_per_row();
  naive_timestamp_year_range();
  std::cout << "review AZ1 tests passed\n";
  return 0;
}
