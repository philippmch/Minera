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
  log.object_changes = {
      {"o1", "Order", "2024-01-02T00:00:00Z", "status",
       std::string("paid")},
  };
  return log;
}

}  // namespace

int main() {
  const auto log = sample();

  const auto web = minera::filter_ocel_event_attribute(
      log, "channel", {std::string("web")});
  assert(web.events.size() == 2 && web.objects.size() == 3);
  assert(web.relations.size() == 3);
  assert(web.o2o.size() == 2 && web.object_changes.size() == 1);

  const auto not_store = minera::filter_ocel_event_attribute(
      log, "channel", {std::string("store")}, false);
  assert(not_store.events.size() == 2);
  assert(not_store.relations.size() == 3);

  const auto red = minera::filter_ocel_object_attribute(
      log, "color", {std::string("red")});
  assert(red.objects.size() == 1 && red.objects.front().id == "o2");
  assert(red.events.size() == 1 && red.events.front().id == "e1");
  assert(red.relations.size() == 1 && red.o2o.empty());

  const auto items = minera::filter_ocel_object_types(log, {"Item"});
  assert(items.objects.size() == 2 && items.events.size() == 2);
  assert(items.relations.size() == 2 && items.o2o.empty());

  const auto order = minera::filter_ocel_objects(log, {"o1"});
  assert(order.objects.size() == 1 && order.events.size() == 2);
  assert(order.relations.size() == 2 && order.object_changes.size() == 1);

  const auto shipped = minera::filter_ocel_events(log, {"e2"});
  assert(shipped.events.size() == 1 && shipped.objects.size() == 1);
  assert(shipped.relations.size() == 1 && shipped.relations.front().object_id == "o1");

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
  rejected = false;
  try {
    (void)minera::filter_ocel_events_timestamp(
        log, "2024-01-03T00:00:00Z", "2024-01-01T00:00:00Z");
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
  return 0;
}
