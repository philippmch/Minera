#include "minera/attribute_statistics.hpp"
#include "minera/json_export.hpp"

#include <cassert>
#include <iostream>
#include <stdexcept>

namespace {

minera::Event make_event(std::string activity, std::string resource) {
  minera::Event event;
  event.activity = std::move(activity);
  event.resource = std::move(resource);
  event.attributes.push_back({"channel", event.activity,
                              minera::AttributeKind::kString});
  return event;
}

minera::EventLog fixture() {
  minera::EventLog log;
  minera::Trace first;
  first.case_id = "c1";
  first.attributes.push_back({"region", "north", minera::AttributeKind::kString});
  first.events = {make_event("A", "alice"), make_event("A", "alice"),
                  make_event("B", "bob")};
  minera::Trace second;
  second.case_id = "c2";
  second.attributes.push_back({"region", "south", minera::AttributeKind::kString});
  second.events = {make_event("A", "alice")};
  log.traces = {std::move(first), std::move(second)};
  return log;
}

}  // namespace

int main() {
  const auto log = fixture();
  const auto all = minera::get_attribute_values(log, "activity");
  assert(all.at("A") == 3 && all.at("B") == 1);
  const auto once = minera::get_attribute_values(log, "activity", true);
  assert(once.at("A") == 2 && once.at("B") == 1);

  minera::AttributeStatisticsOptions resource_options;
  resource_options.attribute_key = "org:resource";
  const auto resources =
      minera::discover_attribute_statistics(log, resource_options);
  assert(resources.values.at("alice") == 3);
  assert(std::string(minera::attribute_statistics_scope_name(
             resources.scope)) == "EVENTS");

  minera::AttributeStatisticsOptions trace_options;
  trace_options.scope = minera::AttributeStatisticsScope::kTraces;
  trace_options.attribute_key = "region";
  const auto regions =
      minera::discover_attribute_statistics(log, trace_options);
  assert(regions.values.at("north") == 1 && regions.values.at("south") == 1);
  const auto json = minera::attribute_statistics_json(regions);
  assert(json.find("\"family\":\"ATTRIBUTE_STATISTICS\"") !=
         std::string::npos);
  assert(json.find("\"scope\":\"TRACES\"") != std::string::npos);

  bool rejected = false;
  try {
    (void)minera::discover_attribute_statistics(log, {});
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
  std::cout << "Attribute-statistics tests passed\n";
}
