#include "minera/ocel_features.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <stdexcept>

#include "minera/timestamp.hpp"

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

minera::Ocel graph_feature_sample() {
  minera::Ocel log;
  log.events = {
      {"e1", "Create", "2026-01-01T00:00:00Z", {}},
      {"e2", "Add", "2026-01-01T01:00:00Z", {}},
      {"e3", "Split", "2026-01-01T02:00:00Z", {}},
      {"e4", "Close", "2026-01-01T03:00:00Z", {}}};
  log.objects = {{"a", "order", {}}, {"b", "order", {}},
                 {"c", "item", {}}, {"d", "item", {}}};
  log.relations = {{"e1", "a", ""}, {"e1", "b", ""},
                   {"e1", "b", "duplicate"}, {"e2", "a", ""},
                   {"e2", "c", ""}, {"e3", "c", ""},
                   {"e3", "d", ""}, {"e4", "b", ""},
                   {"e4", "d", ""}};
  return log;
}

minera::Ocel related_feature_sample() {
  minera::Ocel log;
  log.events = {
      {"r1", "Review", "2026-01-02T00:00:00Z", {{"score", std::int64_t{10}}}},
      {"r2", "Review", "2026-01-01T00:00:00Z", {{"score", std::int64_t{20}}}},
      {"r3", "Ship", "2026-01-03T00:00:00Z", {{"score", std::int64_t{30}}}}};
  log.objects = {
      {"a", "order", {{"priority", std::int64_t{1}}}},
      {"b", "order", {{"priority", std::int64_t{5}}}},
      {"c", "item", {{"priority", std::int64_t{9}}}},
      {"d", "item", {{"priority", std::int64_t{12}}}}};
  // The relation stream intentionally puts r1 before r2 even though r2 has
  // the earlier timestamp; related-activity features must use this order.
  log.relations = {{"r1", "a", ""}, {"r2", "a", ""},
                   {"r1", "b", ""}, {"r3", "b", ""},
                   {"r3", "c", ""}};
  log.o2o = {{"a", "c", "connects"}, {"a", "c", "duplicate"}};
  return log;
}

minera::Ocel non_monotonic_lifecycle_sample() {
  minera::Ocel log;
  log.events = {
      {"late", "A", "2026-01-02T00:00:00Z", {}},
      {"early", "B", "2026-01-03T00:00:00Z", {}},
      {"last", "A", "2026-01-01T00:00:00Z", {}}};
  log.objects = {{"o", "order", {}}};
  log.relations = {{"late", "o", ""}, {"early", "o", ""},
                   {"last", "o", ""}};
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

  const auto lifecycle_order = non_monotonic_lifecycle_sample();
  auto lifecycle_options = minera::OcelFeatureOptions{};
  lifecycle_options.include_activity_features = false;
  lifecycle_options.include_timestamp_features = false;
  lifecycle_options.include_relation_features = false;
  lifecycle_options.include_endpoint_features = false;
  lifecycle_options.include_attribute_counts = false;
  lifecycle_options.include_interaction_features = false;
  const auto lifecycle_table = minera::extract_ocel_features(
      lifecycle_order, minera::OcelFeatureVariant::kObjects,
      lifecycle_options);
  const auto lifecycle_column = [&](const std::string& name) {
    const auto it = std::find(lifecycle_table.feature_names.begin(),
                              lifecycle_table.feature_names.end(), name);
    assert(it != lifecycle_table.feature_names.end());
    return static_cast<std::size_t>(it - lifecycle_table.feature_names.begin());
  };
  const auto duration = lifecycle_column("@@object_lifecycle_duration");
  const auto start = lifecycle_column("@@object_lifecycle_start_timestamp");
  const auto end = lifecycle_column("@@object_lifecycle_end_timestamp");
  const auto path_ab = lifecycle_column("@@ocel_lif_path_A##B");
  const auto path_ba = lifecycle_column("@@ocel_lif_path_B##A");
  assert(lifecycle_table.values[0][duration] == -86400.0);
  assert(lifecycle_table.values[0][start] ==
         minera::parse_timestamp("2026-01-02T00:00:00Z").value());
  assert(lifecycle_table.values[0][end] ==
         minera::parse_timestamp("2026-01-01T00:00:00Z").value());
  assert(lifecycle_table.values[0][path_ab] == 1.0 &&
         lifecycle_table.values[0][path_ba] == 1.0);

  auto duration_only = lifecycle_options;
  duration_only.include_lifecycle_length_features = false;
  duration_only.include_lifecycle_unique_activity_features = false;
  duration_only.include_lifecycle_activity_features = false;
  duration_only.include_lifecycle_path_features = false;
  duration_only.include_lifecycle_wip_features = false;
  const auto duration_only_table = minera::extract_ocel_features(
      lifecycle_order, minera::OcelFeatureVariant::kObjects, duration_only);
  assert(duration_only_table.feature_names.size() == 3);
  assert(duration_only_table.feature_names[0] == "@@object_lifecycle_duration");
  assert(duration_only_table.feature_names[1] == "@@object_lifecycle_start_timestamp");
  assert(duration_only_table.feature_names[2] == "@@object_lifecycle_end_timestamp");

  auto no_duration = lifecycle_options;
  no_duration.include_lifecycle_duration_features = false;
  const auto no_duration_table = minera::extract_ocel_features(
      lifecycle_order, minera::OcelFeatureVariant::kObjects, no_duration);
  assert(std::none_of(no_duration_table.feature_names.begin(),
                      no_duration_table.feature_names.end(), [](const auto& name) {
                        return name == "@@object_lifecycle_duration" ||
                               name == "@@object_lifecycle_start_timestamp" ||
                               name == "@@object_lifecycle_end_timestamp";
                      }));

  auto no_lifecycle_metrics = lifecycle_options;
  no_lifecycle_metrics.include_lifecycle_features = false;
  no_lifecycle_metrics.include_lifecycle_path_features = true;
  const auto no_lifecycle_metrics_table = minera::extract_ocel_features(
      lifecycle_order, minera::OcelFeatureVariant::kObjects,
      no_lifecycle_metrics);
  assert(std::none_of(no_lifecycle_metrics_table.feature_names.begin(),
                      no_lifecycle_metrics_table.feature_names.end(), [](const auto& name) {
                        return name.rfind("@@object_lifecycle_", 0) == 0 ||
                               name.rfind("@@ocel_lif_", 0) == 0;
                      }));

  auto paths_disabled = options;
  paths_disabled.include_lifecycle_path_features = false;
  const auto no_paths = minera::extract_ocel_features(
      log, minera::OcelFeatureVariant::kObjects, paths_disabled);
  assert(std::none_of(no_paths.feature_names.begin(), no_paths.feature_names.end(),
                      [](const auto& name) {
                        return name.rfind("@@ocel_lif_path_", 0) == 0;
                      }));
  auto lifecycle_disabled = options;
  lifecycle_disabled.include_lifecycle_features = false;
  lifecycle_disabled.include_lifecycle_path_features = true;
  const auto no_lifecycle = minera::extract_ocel_features(
      log, minera::OcelFeatureVariant::kObjects, lifecycle_disabled);
  assert(std::none_of(no_lifecycle.feature_names.begin(),
                      no_lifecycle.feature_names.end(), [](const auto& name) {
                        return name.rfind("@@ocel_lif_path_", 0) == 0;
                      }));
  auto path_disabled_non_object = options;
  path_disabled_non_object.include_lifecycle_path_features = false;
  bool rejected_path_events = false;
  try {
    (void)minera::extract_ocel_features(
        log, minera::OcelFeatureVariant::kEvents,
        path_disabled_non_object);
  } catch (const std::invalid_argument&) {
    rejected_path_events = true;
  }
  assert(rejected_path_events);

  auto lifecycle_control_disabled = options;
  lifecycle_control_disabled.include_lifecycle_length_features = false;
  bool rejected_lifecycle_control_events = false;
  try {
    (void)minera::extract_ocel_features(
        log, minera::OcelFeatureVariant::kEvents,
        lifecycle_control_disabled);
  } catch (const std::invalid_argument&) {
    rejected_lifecycle_control_events = true;
  }
  assert(rejected_lifecycle_control_events);
  lifecycle_control_disabled.include_lifecycle_length_features = true;
  lifecycle_control_disabled.include_lifecycle_unique_activity_features = false;
  bool rejected_unique_control_events = false;
  try {
    (void)minera::extract_ocel_features(
        log, minera::OcelFeatureVariant::kEvents,
        lifecycle_control_disabled);
  } catch (const std::invalid_argument&) {
    rejected_unique_control_events = true;
  }
  assert(rejected_unique_control_events);
  lifecycle_control_disabled.include_lifecycle_unique_activity_features = true;
  lifecycle_control_disabled.include_lifecycle_duration_features = false;
  bool rejected_duration_control_events = false;
  try {
    (void)minera::extract_ocel_features(
        log, minera::OcelFeatureVariant::kEvents,
        lifecycle_control_disabled);
  } catch (const std::invalid_argument&) {
    rejected_duration_control_events = true;
  }
  assert(rejected_duration_control_events);
  lifecycle_control_disabled.include_lifecycle_duration_features = true;
  lifecycle_control_disabled.include_lifecycle_activity_features = false;
  bool rejected_activity_control_events = false;
  try {
    (void)minera::extract_ocel_features(
        log, minera::OcelFeatureVariant::kEvents,
        lifecycle_control_disabled);
  } catch (const std::invalid_argument&) {
    rejected_activity_control_events = true;
  }
  assert(rejected_activity_control_events);
  lifecycle_control_disabled.include_lifecycle_activity_features = true;
  lifecycle_control_disabled.include_lifecycle_wip_features = false;
  bool rejected_wip_control_events = false;
  try {
    (void)minera::extract_ocel_features(
        log, minera::OcelFeatureVariant::kEvents,
        lifecycle_control_disabled);
  } catch (const std::invalid_argument&) {
    rejected_wip_control_events = true;
  }
  assert(rejected_wip_control_events);

  auto related_options = minera::OcelFeatureOptions{};
  related_options.include_activity_features = false;
  related_options.include_timestamp_features = false;
  related_options.include_relation_features = false;
  related_options.include_endpoint_features = false;
  related_options.include_attribute_counts = false;
  related_options.include_interaction_features = false;
  related_options.include_lifecycle_features = false;
  related_options.numeric_event_attributes = {"score"};
  related_options.include_related_events_features = true;
  related_options.include_related_activities_features = true;
  const auto related = minera::extract_ocel_features(
      related_feature_sample(), minera::OcelFeatureVariant::kObjects,
      related_options);
  const auto related_column = [&](const std::string& name) {
    const auto it = std::find(related.feature_names.begin(),
                              related.feature_names.end(), name);
    assert(it != related.feature_names.end());
    return static_cast<std::size_t>(it - related.feature_names.begin());
  };
  const auto score_min = related_column("@@rel_eve_min_@@event_num_score");
  const auto score_max = related_column("@@rel_eve_max_@@event_num_score");
  const auto review_score = related_column(
      "@@ev_act_fea_Review_@@event_num_score");
  assert(related.values[0][score_min] == 10.0 &&
         related.values[0][score_max] == 20.0 &&
         related.values[0][review_score] == 20.0);
  assert(related.values[1][score_min] == 10.0 &&
         related.values[1][score_max] == 30.0 &&
         related.values[1][review_score] == 10.0);
  assert(related.values[2][score_min] == 30.0 &&
         related.values[2][score_max] == 30.0);
  assert(related.values[3][score_min] == 0.0 &&
         related.values[3][score_max] == 0.0);

  auto neighbor_options = minera::OcelFeatureOptions{};
  neighbor_options.include_activity_features = false;
  neighbor_options.include_timestamp_features = false;
  neighbor_options.include_relation_features = false;
  neighbor_options.include_endpoint_features = false;
  neighbor_options.include_attribute_counts = false;
  neighbor_options.include_interaction_features = false;
  neighbor_options.include_lifecycle_features = false;
  neighbor_options.numeric_object_attributes = {"priority"};
  neighbor_options.include_obj_con_in_graph_features = true;
  const auto neighbors = minera::extract_ocel_features(
      related_feature_sample(), minera::OcelFeatureVariant::kObjects,
      neighbor_options);
  const auto neighbor_column = [&](const std::string& name) {
    const auto it = std::find(neighbors.feature_names.begin(),
                              neighbors.feature_names.end(), name);
    assert(it != neighbors.feature_names.end());
    return static_cast<std::size_t>(it - neighbors.feature_names.begin());
  };
  const auto priority_min = neighbor_column(
      "@@obj_graph_con_min_@@object_num_priority");
  const auto priority_max = neighbor_column(
      "@@obj_graph_con_max_@@object_num_priority");
  assert(neighbors.values[0][priority_min] == 5.0 &&
         neighbors.values[0][priority_max] == 9.0);
  assert(neighbors.values[1][priority_min] == 1.0 &&
         neighbors.values[1][priority_max] == 9.0);
  assert(neighbors.values[2][priority_min] == 1.0 &&
         neighbors.values[2][priority_max] == 5.0);
  assert(neighbors.values[3][priority_min] == 0.0 &&
         neighbors.values[3][priority_max] == 0.0);

  auto capped_related = related_options;
  capped_related.include_related_activities_features = false;
  capped_related.max_features = 1;
  bool rejected_related_cap = false;
  try {
    (void)minera::extract_ocel_features(
        related_feature_sample(), minera::OcelFeatureVariant::kObjects,
        capped_related);
  } catch (const std::invalid_argument&) {
    rejected_related_cap = true;
  }
  assert(rejected_related_cap);

  bool rejected_related_events = false;
  try {
    (void)minera::extract_ocel_features(
        related_feature_sample(), minera::OcelFeatureVariant::kEvents,
        related_options);
  } catch (const std::invalid_argument&) {
    rejected_related_events = true;
  }
  assert(rejected_related_events);
  bool rejected_related_events_objects = false;
  try {
    (void)minera::extract_ocel_features(
        related_feature_sample(), minera::OcelFeatureVariant::kEventsObjects,
        related_options);
  } catch (const std::invalid_argument&) {
    rejected_related_events_objects = true;
  }
  assert(rejected_related_events_objects);

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
  auto degree_only = interaction_options;
  degree_only.include_interaction_features = false;
  degree_only.include_degree_centrality_features = true;
  degree_only.include_interaction_by_type_features = false;
  const auto degree_only_table = minera::extract_ocel_features(
      interactions, minera::OcelFeatureVariant::kObjects, degree_only);
  assert(degree_only_table.feature_names.size() == 1 &&
         degree_only_table.feature_names[0] == "@@object_degree_centrality");
  auto interaction_only = interaction_options;
  interaction_only.include_degree_centrality_features = false;
  const auto interaction_only_table = minera::extract_ocel_features(
      interactions, minera::OcelFeatureVariant::kObjects, interaction_only);
  assert(std::find(interaction_only_table.feature_names.begin(),
                   interaction_only_table.feature_names.end(),
                   "@@object_degree_centrality") ==
         interaction_only_table.feature_names.end());
  assert(std::find(interaction_only_table.feature_names.begin(),
                   interaction_only_table.feature_names.end(),
                   "@@object_interaction_graph_ot_item") !=
         interaction_only_table.feature_names.end());
  minera::Ocel singleton;
  singleton.objects = {{"only", "item", {}}};
  auto degree_scalar_options = minera::OcelFeatureOptions{};
  degree_scalar_options.include_activity_features = false;
  degree_scalar_options.include_timestamp_features = false;
  degree_scalar_options.include_relation_features = false;
  degree_scalar_options.include_endpoint_features = false;
  degree_scalar_options.include_attribute_counts = false;
  degree_scalar_options.include_interaction_features = false;
  degree_scalar_options.include_degree_centrality_features = true;
  degree_scalar_options.include_lifecycle_features = false;
  const auto singleton_table = minera::extract_ocel_features(
      singleton, minera::OcelFeatureVariant::kObjects, degree_scalar_options);
  assert(singleton_table.feature_names.size() == 1 &&
         singleton_table.feature_names[0] == "@@object_degree_centrality" &&
         singleton_table.values.size() == 1 &&
         singleton_table.values[0].size() == 1 &&
         singleton_table.values[0][0] == 0.0);
  const auto empty_table = minera::extract_ocel_features(
      minera::Ocel{}, minera::OcelFeatureVariant::kObjects,
      degree_scalar_options);
  assert(empty_table.feature_names.size() == 1 &&
         empty_table.feature_names[0] == "@@object_degree_centrality" &&
         empty_table.values.empty());
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
  auto degree_disabled = minera::OcelFeatureOptions{};
  degree_disabled.include_degree_centrality_features = false;
  bool rejected_degree_events = false;
  try {
    (void)minera::extract_ocel_features(
        interactions, minera::OcelFeatureVariant::kEvents, degree_disabled);
  } catch (const std::invalid_argument&) {
    rejected_degree_events = true;
  }
  assert(rejected_degree_events);
  bool rejected_degree_events_objects = false;
  try {
    (void)minera::extract_ocel_features(
        interactions, minera::OcelFeatureVariant::kEventsObjects,
        degree_disabled);
  } catch (const std::invalid_argument&) {
    rejected_degree_events_objects = true;
  }
  assert(rejected_degree_events_objects);

  auto graph_options = minera::OcelFeatureOptions{};
  graph_options.include_activity_features = false;
  graph_options.include_timestamp_features = false;
  graph_options.include_relation_features = false;
  graph_options.include_endpoint_features = false;
  graph_options.include_attribute_counts = false;
  graph_options.include_interaction_features = false;
  graph_options.include_lifecycle_features = false;
  graph_options.include_general_interaction_features = true;
  graph_options.include_general_descendants_features = true;
  graph_options.include_general_inheritance_features = true;
  graph_options.include_cobirth_features = true;
  graph_options.include_codeath_features = true;
  const auto graph_table = minera::extract_ocel_features(
      graph_feature_sample(), minera::OcelFeatureVariant::kObjects,
      graph_options);
  const auto graph_column = [&](const std::string& name) {
    const auto it = std::find(graph_table.feature_names.begin(),
                              graph_table.feature_names.end(), name);
    assert(it != graph_table.feature_names.end());
    return static_cast<std::size_t>(it - graph_table.feature_names.begin());
  };
  const auto general_interaction =
      graph_column("@@object_general_interaction_graph");
  const auto descendants_asc = graph_column(
      "@@object_general_descendants_graph_ascendants");
  const auto descendants_desc = graph_column(
      "@@object_general_descendants_graph_descendants");
  const auto inheritance_asc = graph_column(
      "@@object_general_inheritance_graph_ascendants");
  const auto inheritance_desc = graph_column(
      "@@object_general_inheritance_graph_descendants");
  const auto cobirth = graph_column("@@object_cobirth");
  const auto codeath = graph_column("@@object_codeath");
  // Event order gives a-b, a-c, c-d, b-d interactions; duplicate rows do not
  // change the graph. New objects create a->c and c->d descendants, while
  // first/last-seen boundaries produce the same inheritance edges. Reverse
  // event order gives b-d as the only codeath edge.
  for (const auto& row : graph_table.values)
    assert(row[general_interaction] == 2.0);
  assert(graph_table.values[0][descendants_asc] == 0.0 &&
         graph_table.values[0][descendants_desc] == 1.0 &&
         graph_table.values[1][descendants_asc] == 0.0 &&
         graph_table.values[1][descendants_desc] == 0.0 &&
         graph_table.values[2][descendants_asc] == 1.0 &&
         graph_table.values[2][descendants_desc] == 1.0 &&
         graph_table.values[3][descendants_asc] == 1.0 &&
         graph_table.values[3][descendants_desc] == 0.0);
  assert(graph_table.values[0][inheritance_asc] == 0.0 &&
         graph_table.values[0][inheritance_desc] == 1.0 &&
         graph_table.values[1][inheritance_asc] == 0.0 &&
         graph_table.values[1][inheritance_desc] == 0.0 &&
         graph_table.values[2][inheritance_asc] == 1.0 &&
         graph_table.values[2][inheritance_desc] == 1.0 &&
         graph_table.values[3][inheritance_asc] == 1.0 &&
         graph_table.values[3][inheritance_desc] == 0.0);
  assert(graph_table.values[0][cobirth] == 1.0 &&
         graph_table.values[1][cobirth] == 1.0 &&
         graph_table.values[2][cobirth] == 0.0 &&
         graph_table.values[3][cobirth] == 0.0);
  assert(graph_table.values[0][codeath] == 0.0 &&
         graph_table.values[1][codeath] == 1.0 &&
         graph_table.values[2][codeath] == 0.0 &&
         graph_table.values[3][codeath] == 1.0);
  bool rejected_graph_events = false;
  try {
    (void)minera::extract_ocel_features(
        graph_feature_sample(), minera::OcelFeatureVariant::kEvents,
        graph_options);
  } catch (const std::invalid_argument&) {
    rejected_graph_events = true;
  }
  assert(rejected_graph_events);
  bool rejected_graph_events_objects = false;
  try {
    (void)minera::extract_ocel_features(
        graph_feature_sample(), minera::OcelFeatureVariant::kEventsObjects,
        graph_options);
  } catch (const std::invalid_argument&) {
    rejected_graph_events_objects = true;
  }
  assert(rejected_graph_events_objects);

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
  // Keep this projection assertion focused on the shared event/object
  // columns; relation-prefix columns are tested separately below.
  auto exploded_options = options;
  exploded_options.include_prefix_features = false;
  exploded_options.include_prefix_length_features = false;
  exploded_options.include_prefix_timediff_features = false;
  exploded_options.include_prefix_one_hot_features = false;
  exploded_log.relations.insert(exploded_log.relations.begin() + 1,
                                {"e1", "o1", "duplicate"});
  const auto exploded = minera::extract_ocel_features(
      exploded_log, minera::OcelFeatureVariant::kEventsObjects, exploded_options);
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
  disabled.include_prefix_features = false;
  disabled.include_prefix_length_features = false;
  disabled.include_prefix_timediff_features = false;
  disabled.include_prefix_one_hot_features = false;
  const auto disabled_exploded = minera::extract_ocel_features(
      log, minera::OcelFeatureVariant::kEventsObjects, disabled);
  assert(disabled_exploded.row_ids.size() == log.relations.size());
  assert(disabled_exploded.feature_names.empty());
  for (const auto& row : disabled_exploded.values) assert(row.empty());

  auto prefix_options = minera::OcelFeatureOptions{};
  const auto prefixes = minera::extract_ocel_features(
      log, minera::OcelFeatureVariant::kEventsObjects, prefix_options);
  const auto prefix_column = [&](const std::string& name) {
    const auto it = std::find(prefixes.feature_names.begin(),
                              prefixes.feature_names.end(), name);
    assert(it != prefixes.feature_names.end());
    return static_cast<std::size_t>(it - prefixes.feature_names.begin());
  };
  const auto prefix_length = prefix_column("@@ev_obj_pref_length");
  const auto prefix_timediff = prefix_column("@@ev_obj_pref_timediff");
  const auto prefix_create = prefix_column("@@ev_obj_pref_act_Create");
  const auto prefix_ship = prefix_column("@@ev_obj_pref_act_Ship");
  assert(prefixes.values[0][prefix_length] == 0.0 &&
         prefixes.values[2][prefix_length] == 1.0 &&
         prefixes.values[4][prefix_length] == 2.0);
  assert(prefixes.values[2][prefix_timediff] == 3600.0 &&
         prefixes.values[4][prefix_timediff] == 7200.0);
  assert(prefixes.values[2][prefix_create] == 1.0 &&
         prefixes.values[2][prefix_ship] == 0.0 &&
         prefixes.values[4][prefix_create] == 1.0 &&
         prefixes.values[4][prefix_ship] == 1.0);

  auto duplicate_prefix_log = log;
  duplicate_prefix_log.relations.insert(duplicate_prefix_log.relations.begin() + 1,
                                         {"e1", "o1", "duplicate"});
  const auto duplicate_prefixes = minera::extract_ocel_features(
      duplicate_prefix_log, minera::OcelFeatureVariant::kEventsObjects,
      prefix_options);
  const auto duplicate_length = static_cast<std::size_t>(
      std::find(duplicate_prefixes.feature_names.begin(),
                duplicate_prefixes.feature_names.end(), "@@ev_obj_pref_length") -
      duplicate_prefixes.feature_names.begin());
  const auto duplicate_create = static_cast<std::size_t>(
      std::find(duplicate_prefixes.feature_names.begin(),
                duplicate_prefixes.feature_names.end(), "@@ev_obj_pref_act_Create") -
      duplicate_prefixes.feature_names.begin());
  assert(duplicate_prefixes.values[0][duplicate_length] == 0.0 &&
         duplicate_prefixes.values[1][duplicate_length] == 1.0 &&
         duplicate_prefixes.values[2][duplicate_length] == 0.0);
  assert(duplicate_prefixes.values[1][duplicate_create] == 1.0);

  auto no_prefix = prefix_options;
  no_prefix.include_prefix_features = false;
  no_prefix.include_prefix_length_features = false;
  no_prefix.include_prefix_timediff_features = false;
  no_prefix.include_prefix_one_hot_features = false;
  const auto no_prefix_table = minera::extract_ocel_features(
      log, minera::OcelFeatureVariant::kEventsObjects, no_prefix);
  assert(std::find(no_prefix_table.feature_names.begin(),
                   no_prefix_table.feature_names.end(), "@@ev_obj_pref_length") ==
         no_prefix_table.feature_names.end());
  bool rejected_prefix_controls = false;
  try {
    (void)minera::extract_ocel_features(
        log, minera::OcelFeatureVariant::kEvents, no_prefix);
  } catch (const std::invalid_argument&) {
    rejected_prefix_controls = true;
  }
  assert(rejected_prefix_controls);
  auto parent_only_disabled = prefix_options;
  parent_only_disabled.include_prefix_features = false;
  const auto parent_only_disabled_table = minera::extract_ocel_features(
      log, minera::OcelFeatureVariant::kEventsObjects, parent_only_disabled);
  assert(std::find(parent_only_disabled_table.feature_names.begin(),
                   parent_only_disabled_table.feature_names.end(),
                   "@@ev_obj_pref_length") ==
         parent_only_disabled_table.feature_names.end());

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
