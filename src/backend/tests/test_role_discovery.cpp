#include "minera/role_discovery.hpp"

#include <cassert>
#include <cmath>
#include <stdexcept>

#include "minera/json_export.hpp"
#include "minera/request_params.hpp"

using namespace minera;

int main() {
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
}
