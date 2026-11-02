#include "minera/attribute_statistics.hpp"
#include "minera/json_export.hpp"

#include <cassert>
#include <iostream>
#include <limits>
#include <map>
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
  // As pm4py's DataFrame get_trace_attribute_values: one count per event row,
  // and the `case:` column name selects the same trace attribute.
  assert(regions.values.at("north") == 3 && regions.values.at("south") == 1);
  trace_options.attribute_key = "case:region";
  assert(minera::discover_attribute_statistics(log, trace_options).values ==
         regions.values);
  const auto json = minera::attribute_statistics_json(regions);
  assert(json.find("\"family\":\"ATTRIBUTE_STATISTICS\"") !=
         std::string::npos);
  assert(json.find("\"scope\":\"TRACES\"") != std::string::npos);
  const auto event_attributes =
      minera::get_all_event_attributes_from_log(log);
  assert(event_attributes.contains("concept:name") &&
         event_attributes.contains("channel"));
  const auto trace_attributes =
      minera::get_all_trace_attributes_from_log(log);
  assert(trace_attributes.contains("region"));

  // Missing cells are not counted: a blank CSV resource (no first-class
  // value) and a float NaN cell, which a DataFrame reads as NaN.
  {
    auto sparse = fixture();
    sparse.traces[0].events[0].resource.clear();
    minera::EventAttribute nan{"score", "nan", minera::AttributeKind::kFloat};
    nan.typed_value = std::numeric_limits<double>::quiet_NaN();
    sparse.traces[0].events[1].attributes.push_back(nan);
    minera::EventAttribute one{"score", "1.0", minera::AttributeKind::kFloat};
    one.typed_value = 1.0;
    sparse.traces[1].events[0].attributes.push_back(one);
    auto nan_resource = sparse.traces[1].events[0];
    nan_resource.resource = "nan";
    nan_resource.resource_state = minera::FirstClassAttributeState::kPresent;
    nan_resource.resource_attribute = nan;
    nan_resource.resource_attribute->key = "org:resource";
    sparse.traces[1].events.push_back(nan_resource);
    minera::AttributeStatisticsOptions options;
    options.attribute_key = "org:resource";
    const auto counted = minera::discover_attribute_statistics(sparse, options).values;
    assert((counted == std::map<std::string, std::uint64_t>{{"alice", 2}, {"bob", 1}}));
    options.attribute_key = "score";
    assert((minera::discover_attribute_statistics(sparse, options).values ==
            std::map<std::string, std::uint64_t>{{"1.0", 2}}));
  }

  bool rejected = false;
  try {
    (void)minera::discover_attribute_statistics(log, {});
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
  std::cout << "Attribute-statistics tests passed\n";
}
