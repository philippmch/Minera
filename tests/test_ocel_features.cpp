#include "minera/ocel_features.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <stdexcept>

namespace {

minera::Ocel sample() {
  minera::Ocel log;
  log.events = {
      {"e1", "Create", "2026-01-01T00:00:00Z", {{"amount", std::int64_t{2}}}},
      {"e2", "Ship", "2026-01-01T01:00:00Z", {{"kind", std::string("fast")}}},
      {"e3", "Close", "2026-01-01T02:00:00Z", {}}};
  log.objects = {
      {"o1", "order", {{"priority", std::int64_t{3}}}},
      {"i1", "item", {{"color", std::string("blue")}}},
      {"i2", "item", {}}};
  log.relations = {
      {"e1", "o1", ""}, {"e1", "i1", ""}, {"e2", "o1", ""},
      {"e2", "i1", ""}, {"e3", "o1", ""}, {"e3", "i2", ""}};
  log.e2e = {{"e1", "e2", "precedes"}, {"e2", "e3", "precedes"}};
  return log;
}

}  // namespace

int main() {
  const auto log = sample();
  minera::OcelFeatureOptions options;
  options.numeric_event_attributes = {"amount"};
  options.string_event_attributes = {"kind"};
  options.numeric_object_attributes = {"priority"};
  options.string_object_attributes = {"color"};

  const auto events = minera::extract_ocel_features(
      log, minera::OcelFeatureVariant::kEvents, options);
  assert(events.row_ids.size() == 3 && events.values.size() == 3);
  assert(events.feature_names.size() == events.values.front().size());
  assert(events.feature_names.front() == "@@event_act_Close");
  assert(events.feature_names.back() == "@@event_new_interactions");
  assert(events.values[0][events.feature_names.size() - 1] == 1.0);
  const auto e2e_out = std::find(events.feature_names.begin(),
                                 events.feature_names.end(),
                                 "@@event_num_e2e_out");
  const auto e2e_in = std::find(events.feature_names.begin(),
                                events.feature_names.end(),
                                "@@event_num_e2e_in");
  assert(e2e_out != events.feature_names.end() &&
         e2e_in != events.feature_names.end());
  assert(events.values[0][static_cast<std::size_t>(e2e_out - events.feature_names.begin())] == 1.0);
  assert(events.values[2][static_cast<std::size_t>(e2e_in - events.feature_names.begin())] == 1.0);
  const auto events_json = minera::serialize_ocel_features_json(events);
  assert(events_json.find("\"family\":\"OCEL_FEATURES\"") != std::string::npos);
  assert(events_json.find("\"variant\":\"EVENTS\"") != std::string::npos);

  const auto objects = minera::extract_ocel_features(
      log, minera::OcelFeatureVariant::kObjects, options);
  assert(objects.row_ids.size() == 3 && objects.row_types[0] == "order");
  const auto length = std::find(objects.feature_names.begin(),
                                objects.feature_names.end(),
                                "@@object_lifecycle_length");
  assert(length != objects.feature_names.end());
  assert(objects.values[0][static_cast<std::size_t>(
             length - objects.feature_names.begin())] == 3.0);

  auto filtered_options = options;
  filtered_options.filter_per_type = "item";
  const auto filtered = minera::extract_ocel_features(
      log, minera::OcelFeatureVariant::kObjects, filtered_options);
  assert(filtered.row_ids.size() == 2 && filtered.row_ids[0] == "i1" &&
         filtered.row_ids[1] == "i2");
  assert(filtered.row_types.size() == 2 && filtered.row_types[0] == "item" &&
         filtered.row_types[1] == "item");
  assert(filtered.feature_names == objects.feature_names);
  assert(filtered.values.size() == 2 && filtered.values[0] == objects.values[1] &&
         filtered.values[1] == objects.values[2]);

  filtered_options.filter_per_type = "missing";
  const auto no_rows = minera::extract_ocel_features(
      log, minera::OcelFeatureVariant::kObjects, filtered_options);
  assert(no_rows.row_ids.empty() && no_rows.row_types.empty() &&
         no_rows.values.empty() && no_rows.feature_names == objects.feature_names);

  bool rejected_empty = false;
  try {
    filtered_options.filter_per_type = "";
    (void)minera::extract_ocel_features(
        log, minera::OcelFeatureVariant::kObjects, filtered_options);
  } catch (const std::invalid_argument&) {
    rejected_empty = true;
  }
  assert(rejected_empty);

  bool rejected_events = false;
  try {
    filtered_options.filter_per_type = "item";
    (void)minera::extract_ocel_features(
        log, minera::OcelFeatureVariant::kEvents, filtered_options);
  } catch (const std::invalid_argument&) {
    rejected_events = true;
  }
  assert(rejected_events);

  bool rejected_events_objects = false;
  try {
    (void)minera::extract_ocel_features(
        log, minera::OcelFeatureVariant::kEventsObjects, filtered_options);
  } catch (const std::invalid_argument&) {
    rejected_events_objects = true;
  }
  assert(rejected_events_objects);

  const auto exploded = minera::extract_ocel_features(
      log, minera::OcelFeatureVariant::kEventsObjects, options);
  assert(exploded.row_ids.size() == log.relations.size());
  assert(exploded.row_ids[0] == "e1::o1");
  assert(exploded.values.size() == exploded.row_types.size());

  bool rejected = false;
  try {
    auto bounded = options;
    bounded.max_features = 1;
    (void)minera::extract_ocel_features(log, minera::OcelFeatureVariant::kEvents,
                                        bounded);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
  std::cout << "OCEL feature tests passed\n";
}
