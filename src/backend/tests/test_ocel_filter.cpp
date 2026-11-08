#include "minera/ocel_filter.hpp"

#include <cassert>
#include <stdexcept>

namespace {

minera::Ocel sample() {
  minera::Ocel log;
  log.objects = {
      {"o1", "Order", {{"status", std::string("new")}}},
      {"o2", "Item", {{"color", std::string("red")}}},
      {"o3", "Item", {{"color", std::string("blue")}}},
  };
  log.events = {
      {"e1", "create", "2024-01-01T00:00:00Z", {{"channel", std::string("web")}}},
      {"e2", "ship", "2024-01-02T00:00:00Z", {{"channel", std::string("store")}}},
      {"e3", "inspect", "2024-01-03T00:00:00Z", {{"channel", std::string("web")}}},
  };
  log.relations = {
      {"e1", "o1", "owner"}, {"e1", "o2", "item"},
      {"e2", "o1", "owner"}, {"e3", "o3", "item"},
  };
  log.o2o = {{"o1", "o2", "contains"}, {"o1", "o3", "contains"}};
  log.e2e = {{"e1", "e3", "precedes"}, {"e2", "e3", "precedes"}};
  log.object_changes = {
      {"o1", "Order", "2024-01-02T00:00:00Z", "status",
       std::string("paid")},
  };
  return log;
}

}  // namespace

int main() {
  const auto log = sample();

  // filter_ocel_event_attribute / filter_ocel_object_attribute select on the
  // frame's id, activity and type columns too (pm4py 2.7.22, fuzzed on 320
  // OCELs through /api/ocel/filter): native matched no event before.
  const auto by_activity = minera::filter_ocel_event_attribute(
      log, "ocel:activity", {std::string("ship"), std::string("inspect")});
  assert(by_activity.events.size() == 2 && by_activity.events[0].id == "e2" &&
         by_activity.events[1].id == "e3" && by_activity.objects.size() == 2);
  const auto not_create = minera::filter_ocel_event_attribute(
      log, "ocel:activity", {std::string("create")}, false);
  assert(not_create.events.size() == 2 && not_create.events[0].id == "e2");
  const auto by_id = minera::filter_ocel_event_attribute(log, "ocel:eid", {std::string("e1")});
  assert(by_id.events.size() == 1 && by_id.relations.size() == 2);
  // Strings never equal the datetime timestamp column.
  assert(minera::filter_ocel_event_attribute(
             log, "ocel:timestamp", {std::string("2024-01-01T00:00:00Z")}).events.empty());
  const auto typed_items = minera::filter_ocel_object_attribute(log, "ocel:type", {std::string("Item")});
  assert(typed_items.objects.size() == 2 && typed_items.objects[0].id == "o2" && typed_items.events.size() == 2);
  const auto not_o1 = minera::filter_ocel_object_attribute(log, "ocel:oid", {std::string("o1")}, false);
  assert(not_o1.objects.size() == 2 && not_o1.objects[0].id == "o2");

  // pandas isin compares like Python ==: an integer value selects a float
  // cell (pm4py writes 4.0 when some event lacks the attribute), and booleans
  // count as 0/1; strings stay strings (review X2-1).
  {
    auto numeric = log;
    numeric.events[0].attributes.push_back({"cost", 4.0});
    numeric.events[2].attributes.push_back({"cost", std::int64_t{1}});
    numeric.objects[0].attributes.push_back({"w", std::int64_t{2}});
    numeric.objects[1].attributes.push_back({"w", true});
    const auto four = minera::filter_ocel_event_attribute(numeric, "cost", {std::int64_t{4}});
    assert(four.events.size() == 1 && four.events[0].id == "e1");
    const auto one = minera::filter_ocel_event_attribute(numeric, "cost", {true, 4.5});
    assert(one.events.size() == 1 && one.events[0].id == "e3");
    assert(minera::filter_ocel_event_attribute(numeric, "cost", {std::string("4")}).events.empty());
    const auto not_four = minera::filter_ocel_event_attribute(numeric, "cost", {4.0}, false);
    assert(not_four.events.size() == 2 && not_four.events[0].id == "e2");
    const auto two = minera::filter_ocel_object_attribute(numeric, "w", {2.0});
    assert(two.objects.size() == 1 && two.objects[0].id == "o1");
    const auto truthy = minera::filter_ocel_object_attribute(numeric, "w", {std::int64_t{1}});
    assert(truthy.objects.size() == 1 && truthy.objects[0].id == "o2");
    assert(minera::filter_ocel_object_attribute(numeric, "w", {nullptr}).objects.empty());
  }

  const auto web = minera::filter_ocel_event_attribute(
      log, "channel", {std::string("web")});
  assert(web.events.size() == 2 && web.objects.size() == 3);
  assert(web.relations.size() == 3);
  assert(web.o2o.size() == 2 && web.object_changes.size() == 1);
  assert(web.e2e.size() == 1 && web.e2e.front().source_id == "e1");

  const auto not_store = minera::filter_ocel_event_attribute(
      log, "channel", {std::string("store")}, false);
  assert(not_store.events.size() == 2);
  assert(not_store.relations.size() == 3);

  const auto red = minera::filter_ocel_object_attribute(
      log, "color", {std::string("red")});
  assert(red.objects.size() == 1 && red.objects.front().id == "o2");
  assert(red.events.size() == 1 && red.events.front().id == "e1");
  assert(red.relations.size() == 1 && red.o2o.empty() && red.e2e.empty());

  const auto items = minera::filter_ocel_object_types(log, {"Item"});
  assert(items.objects.size() == 2 && items.events.size() == 2);
  assert(items.relations.size() == 2 && items.o2o.empty());

  const auto order = minera::filter_ocel_objects(log, {"o1"});
  assert(order.objects.size() == 1 && order.events.size() == 2);
  assert(order.relations.size() == 2 && order.object_changes.size() == 1);

  const auto shipped = minera::filter_ocel_events(log, {"e2"});
  assert(shipped.events.size() == 1 && shipped.objects.size() == 1);
  assert(shipped.relations.size() == 1 && shipped.relations.front().object_id == "o1");
  assert(shipped.e2e.empty());

  const auto window = minera::filter_ocel_events_timestamp(
      log, "2024-01-02T00:00:00Z", "2024-01-03T00:00:00Z");
  assert(window.events.size() == 2 && window.relations.size() == 2);

  const auto matching = minera::filter_ocel_activity_type_matching(
      log, {{"Order", {"create", "ship"}}, {"Item", {"inspect"}}});
  assert(matching.events.size() == 3);
  assert(matching.objects.size() == 2);
  assert(matching.relations.size() == 3);
  assert(matching.relations[0].object_id == "o1");
  assert(matching.relations[1].object_id == "o1");
  assert(matching.relations[2].object_id == "o3");

  const auto none = minera::filter_ocel_activity_type_matching(log, {});
  assert(none.events.empty() && none.objects.empty() && none.relations.empty());

  const auto counts = minera::ocel_event_object_type_counts(log);
  assert(counts.at("e1").at("Order") == 1);
  assert(counts.at("e1").at("Item") == 1);
  assert(counts.at("e2").at("Order") == 1);
  assert(counts.at("e3").at("Item") == 1);

  const auto at_least_order_and_item =
      minera::filter_ocel_events_object_type_counts(
          log, {{"Order", 1}, {"Item", 1}});
  assert(at_least_order_and_item.events.size() == 1);
  assert(at_least_order_and_item.events.front().id == "e1");
  assert(at_least_order_and_item.objects.size() == 2);
  assert(at_least_order_and_item.relations.size() == 2);

  const auto starts =
      minera::filter_ocel_start_events_per_object_type(log, "Item");
  assert(starts.events.size() == 2);
  assert(starts.events[0].id == "e1" && starts.events[1].id == "e3");
  assert(starts.objects.size() == 3 && starts.relations.size() == 3);

  const auto ends =
      minera::filter_ocel_end_events_per_object_type(log, "Item");
  assert(ends.events.size() == 2);
  assert(ends.events[0].id == "e1" && ends.events[1].id == "e3");
  assert(ends.objects.size() == 3 && ends.relations.size() == 3);

  const auto empty_requirements =
      minera::filter_ocel_events_object_type_counts(log, {});
  assert(empty_requirements.events.size() == 3);
  const auto missing_type = minera::filter_ocel_events_object_type_counts(
      log, {{"Missing", 0}});
  assert(missing_type.events.empty() && missing_type.objects.empty());

  const auto unknown_endpoint =
      minera::filter_ocel_start_events_per_object_type(log, "Missing");
  assert(unknown_endpoint.events.empty() && unknown_endpoint.objects.empty());

  const auto connected_order =
      minera::filter_ocel_activities_connected_object_type(log, "Order");
  assert(connected_order.events.size() == 2);
  assert(connected_order.objects.size() == 2);
  assert(connected_order.relations.size() == 3);

  const auto level_one = minera::filter_ocel_objects(log, {"o1"});
  assert(level_one.objects.size() == 1 && level_one.events.size() == 2);
  const auto level_two = minera::filter_ocel_objects(log, {"o1"}, true, 2);
  assert(level_two.objects.size() == 2 && level_two.events.size() == 2);
  assert(level_two.relations.size() == 3);
  const auto type_level_two =
      minera::filter_ocel_object_types(log, {"Order"}, true, 2);
  assert(type_level_two.objects.size() == 2 && type_level_two.relations.size() == 3);

  const auto order_component = minera::filter_ocel_cc_object(log, "o1");
  assert(order_component.objects.size() == 2);
  assert(order_component.events.size() == 2);
  assert(order_component.relations.size() == 3);
  const auto singleton_component = minera::filter_ocel_cc_object(log, "o3");
  assert(singleton_component.events.size() == 1 &&
         singleton_component.objects.size() == 1 &&
         singleton_component.relations.size() == 1);

  const auto length_two = minera::filter_ocel_cc_length(log, 2, 2);
  assert(length_two.objects.size() == 2 && length_two.events.size() == 2);
  const auto length_one = minera::filter_ocel_cc_length(log, 1, 1);
  assert(length_one.events.empty() && length_one.objects.empty());

  const auto component_order_type =
      minera::filter_ocel_cc_object_type(log, "Order");
  assert(component_order_type.objects.size() == 2);
  const auto component_non_order_type =
      minera::filter_ocel_cc_object_type(log, "Order", false);
  assert(component_non_order_type.objects.size() == 2);
  const auto component_create = minera::filter_ocel_cc_activity(log, "create");
  assert(component_create.objects.size() == 2);
  const auto component_inspect =
      minera::filter_ocel_cc_activity(log, "inspect");
  assert(component_inspect.events.empty() && component_inspect.objects.empty());

  minera::Ocel duplicate = log;
  duplicate.relations.push_back({"e1", "o2", "duplicate"});
  assert(minera::ocel_event_object_type_counts(duplicate).at("e1").at("Item") ==
         2);

  bool rejected = false;
  try {
    (void)minera::filter_ocel_event_attribute(log, "", {});
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
  // Reversed bounds give an empty OCEL (no events, objects or relations), as
  // pm4py 2.7.22 filter_ocel_events_timestamp does.
  const auto reversed = minera::filter_ocel_events_timestamp(
      log, "2024-01-03T00:00:00Z", "2024-01-01T00:00:00Z");
  assert(reversed.events.empty() && reversed.objects.empty() && reversed.relations.empty());
  assert(reversed.e2e.empty() && reversed.o2o.empty());
  return 0;
}
