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

minera::Ocel interaction_sample() {
  minera::Ocel log;
  log.events = {
      {"e1", "Link", "2026-01-01T00:00:00Z", {}},
      {"e2", "Touch", "2026-01-01T01:00:00Z", {}}};
  log.objects = {
      {"o1", "order", {}}, {"o2", "order", {}},
      {"i1", "item", {}},  {"i2", "item", {}},
      {"x1", "other", {}},  {"z1", "other", {}}};
  log.relations = {
      {"e1", "o1", ""}, {"e1", "o2", ""}, {"e1", "o2", ""},
      {"e1", "i1", ""}, {"e2", "i1", ""}, {"e2", "i1", ""}};
  log.o2o = {{"i2", "x1", "contains"}, {"i2", "x1", "contains"}};
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

  const auto interactions = interaction_sample();
  auto interaction_options = minera::OcelFeatureOptions{};
  interaction_options.include_activity_features = false;
  interaction_options.include_timestamp_features = false;
  interaction_options.include_relation_features = false;
  interaction_options.include_endpoint_features = false;
  interaction_options.include_attribute_counts = false;
  interaction_options.include_lifecycle_features = false;
  interaction_options.include_interaction_by_type_features = true;
  const auto interaction_table = minera::extract_ocel_features(
      interactions, minera::OcelFeatureVariant::kObjects, interaction_options);
  const auto degree_column = std::find(
      interaction_table.feature_names.begin(), interaction_table.feature_names.end(),
      "@@object_degree_centrality");
  const auto item_column = std::find(
      interaction_table.feature_names.begin(), interaction_table.feature_names.end(),
      "@@object_interaction_graph_ot_item");
  const auto order_column = std::find(
      interaction_table.feature_names.begin(), interaction_table.feature_names.end(),
      "@@object_interaction_graph_ot_order");
  const auto other_column = std::find(
      interaction_table.feature_names.begin(), interaction_table.feature_names.end(),
      "@@object_interaction_graph_ot_other");
  assert(degree_column != interaction_table.feature_names.end() &&
         item_column != interaction_table.feature_names.end() &&
         order_column != interaction_table.feature_names.end() &&
         other_column != interaction_table.feature_names.end());
  const auto column = [&](decltype(degree_column) iterator) {
    return static_cast<std::size_t>(iterator - interaction_table.feature_names.begin());
  };
  const auto degree = column(degree_column);
  const auto item = column(item_column);
  const auto order = column(order_column);
  const auto other = column(other_column);
  // o1 and o2 share an event (same-type), o1/i1 and o2/i1 cross types,
  // i2/x1 is O2O-only, and z1 is isolated. Duplicate links count once.
  assert(interaction_table.values[0][item] == 1.0 &&
         interaction_table.values[0][order] == 1.0 &&
         interaction_table.values[0][other] == 0.0);
  assert(interaction_table.values[1][item] == 1.0 &&
         interaction_table.values[1][order] == 1.0);
  assert(interaction_table.values[2][item] == 0.0 &&
         interaction_table.values[2][order] == 2.0);
  assert(interaction_table.values[3][other] == 1.0);
  assert(interaction_table.values[4][item] == 1.0);
  assert(interaction_table.values[5][item] == 0.0 &&
         interaction_table.values[5][order] == 0.0 &&
         interaction_table.values[5][other] == 0.0);
  for (const auto& row : interaction_table.values)
    assert(row[item] + row[order] + row[other] == row[degree] * 5.0);

  auto no_by_type = interaction_options;
  no_by_type.include_interaction_by_type_features = false;
  const auto ordinary_interactions = minera::extract_ocel_features(
      interactions, minera::OcelFeatureVariant::kObjects, no_by_type);
  assert(ordinary_interactions.feature_names.size() == 1 &&
         ordinary_interactions.feature_names[0] == "@@object_degree_centrality");
  auto capped = interaction_options;
  capped.include_interaction_features = false;
  capped.max_features = 2;
  bool rejected_interaction_cap = false;
  try {
    (void)minera::extract_ocel_features(
        interactions, minera::OcelFeatureVariant::kObjects, capped);
  } catch (const std::invalid_argument&) {
    rejected_interaction_cap = true;
  }
  assert(rejected_interaction_cap);

  bool rejected_interaction_events = false;
  try {
    (void)minera::extract_ocel_features(
        interactions, minera::OcelFeatureVariant::kEvents, interaction_options);
  } catch (const std::invalid_argument&) {
    rejected_interaction_events = true;
  }
  assert(rejected_interaction_events);
  bool rejected_interaction_events_objects = false;
  try {
    (void)minera::extract_ocel_features(
        interactions, minera::OcelFeatureVariant::kEventsObjects,
        interaction_options);
  } catch (const std::invalid_argument&) {
    rejected_interaction_events_objects = true;
  }
  assert(rejected_interaction_events_objects);

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

  auto exploded_log = log;
  exploded_log.relations.insert(exploded_log.relations.begin() + 1,
                                {"e1", "o1", "duplicate"});
  const auto exploded = minera::extract_ocel_features(
      exploded_log, minera::OcelFeatureVariant::kEventsObjects, options);
  assert(exploded.row_ids.size() == exploded_log.relations.size());
  assert(exploded.row_ids[0] == "e1::o1" &&
         exploded.row_ids[1] == "e1::o1#2" &&
         exploded.row_ids[2] == "e1::i1");
  assert(exploded.values.size() == exploded.row_types.size());
  const auto exploded_events = extract_ocel_features(
      exploded_log, minera::OcelFeatureVariant::kEvents, options);
  const auto exploded_objects = extract_ocel_features(
      exploded_log, minera::OcelFeatureVariant::kObjects, options);
  assert(exploded.feature_names.size() ==
         exploded_events.feature_names.size() + exploded_objects.feature_names.size());
  const auto exploded_column = [&](const std::string& name) {
    const auto it = std::find(exploded.feature_names.begin(),
                              exploded.feature_names.end(), name);
    assert(it != exploded.feature_names.end());
    return static_cast<std::size_t>(it - exploded.feature_names.begin());
  };
  const auto event_amount = exploded_column("@@event_num_amount");
  const auto event_kind_fast = exploded_column("@@event_str_kind_fast");
  const auto event_object_count = exploded_column("@@event_num_rel_objs");
  const auto event_order_count =
      exploded_column("@@event_num_rel_objs_type_order");
  const auto event_start_order = exploded_column("@@event_start_order");
  const auto object_priority = exploded_column("@@object_num_priority");
  const auto object_color_blue = exploded_column("@@object_str_color_blue");
  const auto object_item_type = exploded_column("@@object_type_item");
  const auto object_lifecycle =
      exploded_column("@@object_lifecycle_length");
  assert(exploded.values[0][event_amount] == 2.0 &&
         exploded.values[0][event_kind_fast] == 0.0);
  assert(exploded.values[1][event_amount] == 2.0 &&
         exploded.values[1][event_object_count] == 3.0 &&
         exploded.values[1][event_order_count] == 2.0);
  assert(exploded.values[2][event_kind_fast] == 0.0 &&
         exploded.values[2][event_start_order] == 1.0 &&
         exploded.values[2][object_color_blue] == 1.0 &&
         exploded.values[2][object_item_type] == 1.0 &&
         exploded.values[2][object_lifecycle] == 2.0);
  assert(exploded.values[4][event_kind_fast] == 1.0 &&
         exploded.values[4][object_color_blue] == 1.0 &&
         exploded.values[4][object_item_type] == 1.0);
  assert(exploded.values[0][object_priority] == 3.0 &&
         exploded.values[0][object_item_type] == 0.0);
  for (std::size_t row = 0; row < exploded_log.relations.size(); ++row) {
    const auto event_it = std::find_if(
        exploded_log.events.begin(), exploded_log.events.end(),
        [&](const auto& event) {
          return event.id == exploded_log.relations[row].event_id;
        });
    const auto object_it = std::find_if(
        exploded_log.objects.begin(), exploded_log.objects.end(),
        [&](const auto& object) {
          return object.id == exploded_log.relations[row].object_id;
        });
    const auto event_row = static_cast<std::size_t>(
        event_it - exploded_log.events.begin());
    const auto object_row = static_cast<std::size_t>(
        object_it - exploded_log.objects.begin());
    for (std::size_t column = 0;
         column < exploded_events.feature_names.size(); ++column)
      assert(exploded.values[row][column] ==
             exploded_events.values[event_row][column]);
    for (std::size_t column = 0;
         column < exploded_objects.feature_names.size(); ++column)
      assert(exploded.values[row][exploded_events.feature_names.size() + column] ==
             exploded_objects.values[object_row][column]);
  }

  auto disabled = minera::OcelFeatureOptions{};
  disabled.include_activity_features = false;
  disabled.include_timestamp_features = false;
  disabled.include_relation_features = false;
  disabled.include_endpoint_features = false;
  disabled.include_attribute_counts = false;
  disabled.include_interaction_features = false;
  disabled.include_lifecycle_features = false;
  const auto disabled_exploded = minera::extract_ocel_features(
      log, minera::OcelFeatureVariant::kEventsObjects, disabled);
  assert(disabled_exploded.row_ids.size() == log.relations.size());
  assert(disabled_exploded.feature_names.empty());
  for (const auto& row : disabled_exploded.values) assert(row.empty());

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
