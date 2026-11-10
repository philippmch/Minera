#include "minera/role_discovery.hpp"

#include <cassert>
#include <cmath>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "minera/csv_event_log_importer.hpp"

#include "minera/json_export.hpp"
#include "minera/request_params.hpp"

using namespace minera;

namespace {
// pm4py 2.7.22 on read_csv + format_dataframe(case_id="cid",
// activity_key="act", timestamp_key="ts") of this CSV (Review AS1): keys
// are frame columns, so org:resource and resource raise KeyError; "who" is
// an int column, "num" a float one with a NaN, "txt" text with a NaN.
EventLog as1_csv_log() {
  std::istringstream csv("cid,act,ts,who,num,txt\n"
                         "c1,a,2020-01-01 00:00:00,1,,x\n"
                         "c1,b,2020-01-01 00:01:00,2,1,y\n"
                         "c1,a,2020-01-01 00:02:00,1,2,\n"
                         "c2,a,2020-01-02 00:00:00,2,3,x\n"
                         "c2,b,2020-01-02 00:05:00,10,1,y\n");
  CsvImportOptions import;
  import.case_id_column = "cid";
  import.activity_column = "act";
  import.timestamp_column = "ts";
  import.resource_column = "who";
  auto log = import_csv_event_log(csv, import);
  assert(log.dataframe_columns);
  return log;
}

bool raises(const std::function<void()>& call, const std::string& text) {
  try {
    call();
  } catch (const std::invalid_argument& error) {
    return std::string(error.what()).find(text) != std::string::npos;
  }
  return false;
}

using Roles = std::vector<std::pair<std::vector<std::string>, std::vector<std::pair<std::string, std::size_t>>>>;
Roles roles_of(const RoleDiscoveryResult& result) {
  Roles roles;
  for (const auto& role : result.roles) {
    std::vector<std::pair<std::string, std::size_t>> resources;
    for (const auto& resource : role.resources) resources.emplace_back(resource.name, resource.count);
    roles.emplace_back(role.activities, resources);
  }
  return roles;
}

void frame_path_reads_pm4py_keys() {
  const auto log = as1_csv_log();
  RoleDiscoveryOptions options;
  assert(raises([&] { (void)discover_roles(log, options); }, "KeyError: 'org:resource'"));
  options.resource_attribute = "who";
  // The groupby orders a role's resources by value (2 before 10).
  const Roles who{{{"b"}, {{"2", 1}, {"10", 1}}}, {{"a"}, {{"1", 2}, {"2", 1}}}};
  assert(roles_of(discover_roles(log, options)) == who);
  options.variant = RoleDiscoveryVariant::kPandas;
  assert(roles_of(discover_roles(log, options)) == who);
  options.threshold = 0.0;
  const Roles merged{{{"a", "b"}, {{"2", 2}, {"10", 1}, {"1", 2}}}};
  assert(roles_of(discover_roles(log, options)) == merged);
  // NaN cells: the pandas groupby drops them; floats print as Python floats.
  options.threshold = 0.65;
  options.resource_attribute = "num";
  const Roles num{{{"a"}, {{"2.0", 1}, {"3.0", 1}}}, {{"b"}, {{"1.0", 2}}}};
  assert(roles_of(discover_roles(log, options)) == num);
  // Both variants join the activity names: a float activity raises.
  options.resource_attribute = "who";
  options.activity_attribute = "num";
  options.variant = RoleDiscoveryVariant::kLog;
  assert(raises([&] { (void)discover_roles(log, options); },
                "TypeError: sequence item 0: expected str instance"));
}
// pm4py 2.7.22 on read_csv + format_dataframe(case_id="cid", activity_key="act",
// timestamp_key="ts") of this CSV (Review AT1): "who" and "code" are int
// columns (past 2**53 too), "team" text with a NaN cell, no org:resource.
EventLog at1_csv_log() {
  std::istringstream csv("cid,act,ts,who,team,code\n"
                         "c1,a,2020-01-01 10:00:00,1,g1,7\n"
                         "c1,b,2020-01-01 11:00:00,2,g2,8\n"
                         "c2,a,2020-01-02 10:00:00,1,g1,7\n"
                         "c2,c,2020-01-02 13:00:00,1,,9\n"
                         "c3,a,2020-01-03 10:00:00,2,g2,7\n"
                         "c3,c,2020-01-03 10:30:00,9007199254740993,g1,9007199254740992\n");
  CsvImportOptions import;
  import.case_id_column = "cid";
  import.activity_column = "act";
  import.timestamp_column = "ts";
  import.resource_column = "who";
  auto log = import_csv_event_log(csv, import);
  assert(log.dataframe_columns);
  return log;
}

// Review AT1: ints past 2**53 keep their exact names, the pandas variant
// orders roles and resources by value, an int activity column raises.
void frame_path_typed_values() {
  const auto log = at1_csv_log();
  RoleDiscoveryOptions options;
  options.variant = RoleDiscoveryVariant::kPandas;
  options.resource_attribute = "code";
  const Roles code{{{"c"}, {{"9", 1}, {"9007199254740992", 1}}}, {{"b"}, {{"8", 1}}}, {{"a"}, {{"7", 3}}}};
  assert(roles_of(discover_roles(log, options)) == code);
  options.resource_attribute = "who";
  const Roles who{{{"c"}, {{"1", 1}, {"9007199254740993", 1}}}, {{"a"}, {{"1", 2}, {"2", 1}}}, {{"b"}, {{"2", 1}}}};
  assert(roles_of(discover_roles(log, options)) == who);
  options.activity_attribute = "code";
  assert(raises([&] { (void)discover_roles(log, options); },
                "TypeError: sequence item 0: expected str instance, int found"));
}
}  // namespace

int main() {
  frame_path_reads_pm4py_keys();
  frame_path_typed_values();
  EventLog log{{Trace{"c", {Event{"c","A","","r1",0}, Event{"c","A","","r1",1}, Event{"c","B","","r1",2}, Event{"c","B","","r1",3}, Event{"c","B","","r2",4}}}}};
  RoleDiscoveryOptions merge_options; merge_options.threshold = 0.49;
  auto result = discover_roles(log, merge_options);
  assert(result.variant == RoleDiscoveryVariant::kLog);
  assert(result.diagnostics.computed && result.roles.size() == 1);
  assert(result.roles[0].activities.size() == 2);
  // pm4py 2.7.22 roles.apply (LOG and PANDAS, roles_threshold_parameter=0.49)
  // returns [A,B]{r1:4,r2:1}: a merge sums the source counts.
  assert(result.roles[0].resources[0].count == 4);
  assert(result.roles[0].resources[1].count == 1);
  RoleDiscoveryOptions strict; strict.threshold = 1.0;
  assert(discover_roles(log, strict).roles.size() == 2);

  EventLog ordering{{Trace{"order", {
      Event{"order", "A", "", "r1", 0},
      Event{"order", "B", "", "r1", 1},
      Event{"order", "B", "", "r2", 2},
      Event{"order", "C", "", "r3", 3}}}}};
  auto ordered = discover_roles(ordering, strict);
  assert(ordered.roles.size() == 3);
  assert((ordered.roles[0].activities == std::vector<std::string>{"B"}));
  assert((ordered.roles[1].activities == std::vector<std::string>{"C"}));
  assert((ordered.roles[2].activities == std::vector<std::string>{"A"}));

  const auto parsed = parse_role_discovery_params(
      [](std::string_view name) {
        if (name == "roles_variant") return std::string("pandas");
        if (name == "roles_threshold") return std::string("0.4");
        if (name == "roles_max_resources") return std::string("8");
        if (name == "roles_max_activities") return std::string("12");
        return std::string{};
      });
  assert(parsed.variant == RoleDiscoveryVariant::kPandas);
  assert(parsed.threshold == 0.4 && parsed.max_resources == 8 &&
         parsed.max_activities == 12);
  const auto json = role_discovery_json(ordered);
  assert(json.find("\"family\":\"ROLE_DISCOVERY\"") != std::string::npos);
  assert(json.find("\"variant\":\"log\"") != std::string::npos);
  assert(json.find("\"role_count\":3") != std::string::npos);
  auto malformed_result = ordered;
  ++malformed_result.diagnostics.role_count;
  try { (void)role_discovery_json(malformed_result); assert(false); }
  catch (const std::invalid_argument&) {}

  EventLog custom{{Trace{"custom", {
      Event{"custom", "ignored", "", "ignored", 0,
            {{"team:activity", "A"}, {"team:resource", "r1"}}},
      Event{"custom", "ignored", "", "ignored", 1,
            {{"team:activity", "B"}, {"team:resource", "r2"}}},
  }}}};
  RoleDiscoveryOptions custom_options;
  custom_options.activity_attribute = "team:activity";
  custom_options.resource_attribute = "team:resource";
  custom_options.threshold = 1.0;
  const auto custom_result = discover_roles(custom, custom_options);
  assert(custom_result.roles.size() == 2);
  assert(custom_result.roles[0].resources[0].name == "r2" ||
         custom_result.roles[1].resources[0].name == "r2");

  bool threw = false; try { RoleDiscoveryOptions bad; bad.threshold = 2; const auto bad_result = discover_roles(log, bad); (void)bad_result; } catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  threw = false; try { (void)parse_role_discovery_params([](std::string_view) { return std::string("nan"); }); } catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  EventLog blank{{Trace{"x", {Event{"x","A","","",0}}}}};
  const auto blank_result = discover_roles(blank);
  assert(blank_result.roles.empty());

  // Summed merge counts feed the next similarity. Captured from pm4py 2.7.22
  // discover_organizational_roles / roles.apply (LOG and PANDAS, default 0.65):
  // [A,B]{Ann:6,Bob:2} and [C]{Ann:1,Bob:1}; presence counts would also pull C in.
  EventLog summed{{Trace{"1", {
      Event{"1","A","","Ann",0}, Event{"1","A","","Ann",1}, Event{"1","A","","Ann",2},
      Event{"1","A","","Bob",3}, Event{"1","B","","Ann",4}, Event{"1","B","","Ann",5},
      Event{"1","B","","Ann",6}, Event{"1","B","","Bob",7}, Event{"1","C","","Ann",8},
      Event{"1","C","","Bob",9}}}}};
  for (const auto variant : {RoleDiscoveryVariant::kLog, RoleDiscoveryVariant::kPandas}) {
    RoleDiscoveryOptions summed_options; summed_options.variant = variant;
    const auto summed_result = discover_roles(summed, summed_options);
    assert(summed_result.roles.size() == 2);
    assert((summed_result.roles[0].activities == std::vector<std::string>{"A", "B"}));
    assert(summed_result.roles[0].resources.size() == 2);
    assert(summed_result.roles[0].resources[0].name == "Ann" && summed_result.roles[0].resources[0].count == 6);
    assert(summed_result.roles[0].resources[1].name == "Bob" && summed_result.roles[0].resources[1].count == 2);
    assert((summed_result.roles[1].activities == std::vector<std::string>{"C"}));
    assert(summed_result.roles[1].resources[0].count == 1 && summed_result.roles[1].resources[1].count == 1);
  }

  // originator_importance key order. Captured from pm4py 2.7.22: the
  // EventLog gives [A,B,C]{kim:3, amy:3} (each activity's resources in first
  // appearance, Counter(left) + Counter(right) per merge); the DataFrame
  // gives {amy:3, kim:3} (each activity's resources by name).
  EventLog originator_log{{Trace{"1", {
      Event{"1","B","","kim",0}, Event{"1","B","","amy",1}, Event{"1","A","","amy",2},
      Event{"1","A","","kim",3}, Event{"1","C","","kim",4}, Event{"1","C","","amy",5}}}}};
  for (const auto variant : {RoleDiscoveryVariant::kLog, RoleDiscoveryVariant::kPandas}) {
    RoleDiscoveryOptions ordered_options; ordered_options.variant = variant;
    const auto ordered_result = discover_roles(originator_log, ordered_options);
    assert(ordered_result.roles.size() == 1);
    assert((ordered_result.roles[0].activities == std::vector<std::string>{"A", "B", "C"}));
    const auto& resources = ordered_result.roles[0].resources;
    assert(resources.size() == 2 && resources[0].count == 3 && resources[1].count == 3);
    const bool log = variant == RoleDiscoveryVariant::kLog;
    assert(resources[0].name == (log ? "kim" : "amy"));
    assert(resources[1].name == (log ? "amy" : "kim"));
  }
}
