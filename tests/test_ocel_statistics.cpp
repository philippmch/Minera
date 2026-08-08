#include "minera/ocel_statistics.hpp"

#include <cassert>
#include <stdexcept>

namespace {

minera::Ocel sample() {
  minera::Ocel log;
  log.events = {
      {"e1", "Create", "2024-01-01T00:00:00Z", {}},
      {"e2", "Ship", "2024-01-02T00:00:00Z", {}},
      {"e3", "Create", "2024-01-03T00:00:00Z", {}},
  };
  log.objects = {
      {"o1", "Order", {}}, {"o2", "Item", {}}, {"o3", "Unused", {}},
  };
  log.relations = {
      {"e1", "o1", ""}, {"e1", "o2", ""}, {"e1", "o2", "duplicate"},
      {"e2", "o2", ""},
  };
  return log;
}

}  // namespace

int main() {
  const minera::Ocel empty;
  assert(minera::object_type_activities(empty).empty());
  assert(minera::objects_ot_count(empty).empty());

  const auto log = sample();
  const auto activities = minera::object_type_activities(log);
  assert(activities.size() == 2);
  const std::set<std::string> order_activities{"Create"};
  const std::set<std::string> item_activities{"Create", "Ship"};
  assert(activities.at("Order") == order_activities);
  assert(activities.at("Item") == item_activities);
  assert(!activities.contains("Unused"));

  const auto counts = minera::objects_ot_count(log);
  assert(counts.size() == 2);
  assert(counts.at("e1").at("Order") == 1);
  assert(counts.at("e1").at("Item") == 2);
  assert(counts.at("e2").at("Item") == 1);
  assert(!counts.contains("e3"));

  assert(minera::get_object_type_activities(log) == activities);
  assert(minera::ocel_object_type_activities(log) == activities);
  assert(minera::get_objects_ot_count(log) == counts);
  assert(minera::ocel_objects_ot_count(log) == counts);
  const auto activities_json =
      minera::serialize_ocel_statistics_json(activities);
  assert(activities_json.find("\"family\":\"OCEL_STATISTICS\"") !=
         std::string::npos);
  assert(activities_json.find("\"variant\":\"OBJECT_TYPE_ACTIVITIES\"") !=
         std::string::npos);
  assert(activities_json.find("\"object_type_activities\"") !=
         std::string::npos);
  const auto counts_json = minera::serialize_ocel_statistics_json(counts);
  assert(counts_json.find("\"variant\":\"OBJECTS_OT_COUNT\"") !=
         std::string::npos);
  assert(counts_json.find("\"objects_ot_count\"") != std::string::npos);
  assert(minera::serialize_ocel_statistics_json(
             log, minera::OcelStatisticsVariant::kObjectTypeActivities) ==
         activities_json);
  assert(minera::serialize_ocel_statistics_json(
             log, minera::OcelStatisticsVariant::kObjectsOtCount) ==
         counts_json);

  auto invalid = log;
  invalid.relations.push_back({"missing", "o1", ""});
  bool rejected = false;
  try {
    (void)minera::object_type_activities(invalid);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);

  invalid = log;
  invalid.relations.push_back({"e1", "missing", ""});
  rejected = false;
  try {
    (void)minera::objects_ot_count(invalid);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);

  invalid = log;
  invalid.events.push_back({"e1", "Other", "", {}});
  rejected = false;
  try {
    (void)minera::objects_ot_count(invalid);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);

  invalid = log;
  invalid.objects.push_back({"o1", "Other", {}});
  rejected = false;
  try {
    (void)minera::object_type_activities(invalid);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);

  return 0;
}
