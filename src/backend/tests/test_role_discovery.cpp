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
  assert(result.roles[0].resources[0].count == 2);
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
}
