#include "minera/ocel_features.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "minera/timestamp.hpp"

namespace {

minera::Ocel component_log() {
  minera::Ocel log;
  log.events = {
      {"e0", "Orphan", "2026-01-01T00:30:00+02:00",
       {{"score", std::int64_t{1}}, {"kind", std::string{}}}},
      {"e1", "A", "2026-01-01T01:00:00Z",
       {{"score", std::int64_t{2}}, {"kind", std::string{"fast"}}}},
      {"e2", "B", "2026-01-01T02:00:00Z",
       {{"score", std::int64_t{3}}, {"kind", true}}},
      {"e3", "A", "2026-01-01T03:00:00Z",
       {{"score", std::int64_t{4}}, {"kind", nullptr}}},
      {"e4", "B", "2026-01-01T04:00:00Z",
       {{"score", std::int64_t{5}}, {"kind", std::string{"fast"}}}}};
  log.objects = {
      {"a", "order", {{"priority", std::int64_t{1}},
                        {"label", std::string{}}}},
      {"b", "item", {{"priority", std::int64_t{2}},
                       {"label", std::string{"blue"}}}},
      {"z", "other", {{"priority", false}, {"label", nullptr}}}};
  log.relations = {{"e1", "a", ""},       {"e1", "a", "duplicate"},
                   {"e1", "b", ""},       {"e2", "a", ""},
                   {"e2", "b", ""},       {"e3", "a", ""},
                   {"e4", "a", ""},       {"e4", "b", ""}};
  return log;
}

minera::Ocel graph_log() {
  minera::Ocel log;
  log.events = {
      {"g1", "Create", "2026-01-01T00:00:00Z", {}},
      {"g2", "Add", "2026-01-01T01:00:00Z", {}},
      {"g3", "Split", "2026-01-01T02:00:00Z", {}},
      {"g4", "Close", "2026-01-01T03:00:00Z", {}}};
  log.objects = {{"a", "order", {{"priority", std::int64_t{1}}}},
                 {"b", "order", {{"priority", std::int64_t{5}}}},
                 {"c", "item", {{"priority", std::int64_t{9}}}},
                 {"d", "item", {{"priority", std::int64_t{12}}}},
                 {"z", "other", {}}};
  log.relations = {{"g1", "a", ""}, {"g1", "b", ""},
                   {"g2", "a", ""}, {"g2", "c", ""},
                   {"g3", "c", ""}, {"g3", "d", ""},
                   {"g4", "b", ""}, {"g4", "d", ""}};
  return log;
}

std::size_t column(const minera::OcelFeatureTable& table,
                   const std::string& name) {
  const auto it = std::find(table.feature_names.begin(),
                            table.feature_names.end(), name);
  assert(it != table.feature_names.end());
  return static_cast<std::size_t>(it - table.feature_names.begin());
}

minera::OcelFeatureOptions event_minimal() {
  minera::OcelFeatureOptions options;
  options.include_activity_features = false;
  options.include_timestamp_features = false;
  options.include_relation_features = false;
  options.include_endpoint_features = false;
  options.include_attribute_counts = false;
  options.include_interaction_features = false;
  return options;
}

minera::OcelFeatureOptions object_minimal() {
  return event_minimal();
}

void prefix_features_contract() {
  auto options = event_minimal();
  options.include_event_pointwise_features = false;
  options.include_object_pointwise_features = false;
  options.include_prefix_features = false;
  options.include_prefix_length_features = true;
  options.include_prefix_timediff_features = false;
  options.include_prefix_one_hot_features = false;
  const auto table = minera::extract_ocel_features(
      component_log(), minera::OcelFeatureVariant::kEventsObjects, options);
  assert(table.feature_names.size() == 1 &&
         table.feature_names.front() == "@@ev_obj_pref_length");
  assert(table.values[0][0] == 0.0 && table.values[1][0] == 0.0 &&
         table.values[3][0] == 1.0);
}

void event_activity_contract() {
  auto options = event_minimal();
  options.include_activity_features = true;
  const auto table = minera::extract_ocel_features(
      component_log(), minera::OcelFeatureVariant::kEvents, options);
  assert(table.feature_names.size() == 3);
  assert(table.feature_names[0] == "@@event_act_Orphan" &&
         table.feature_names[1] == "@@event_act_A" &&
         table.feature_names[2] == "@@event_act_B");
}

void event_end_ot_contract() {
  auto options = event_minimal();
  options.include_event_end_ot_features = true;
  options.include_event_start_ot_features = false;
  const auto table = minera::extract_ocel_features(
      component_log(), minera::OcelFeatureVariant::kEvents, options);
  const auto end = column(table, "@@event_end_order");
  assert(table.values[4][end] == 1.0 && table.values[3][end] == 0.0);
}

void event_numeric_attributes_contract() {
  auto options = event_minimal();
  options.numeric_event_attributes = {"score"};
  const auto table = minera::extract_ocel_features(
      component_log(), minera::OcelFeatureVariant::kEvents, options);
  const auto score = column(table, "@@event_num_score");
  assert(table.values[0][score] == 1.0 && table.values[4][score] == 5.0);
}

void event_num_rel_objs_contract() {
  auto options = event_minimal();
  options.include_event_num_rel_objs_features = true;
  options.include_event_num_rel_objs_type_features = false;
  const auto table = minera::extract_ocel_features(
      component_log(), minera::OcelFeatureVariant::kEvents, options);
  const auto count = column(table, "@@event_num_rel_objs");
  assert(table.values[1][count] == 3.0);
}

void event_num_rel_objs_type_contract() {
  auto options = event_minimal();
  options.include_event_num_rel_objs_features = false;
  options.include_event_num_rel_objs_type_features = true;
  const auto table = minera::extract_ocel_features(
      component_log(), minera::OcelFeatureVariant::kEvents, options);
  const auto order = column(table, "@@event_num_rel_objs_type_order");
  assert(table.values[1][order] == 1.0);
}

void event_start_ot_contract() {
  auto options = event_minimal();
  options.include_event_start_ot_features = true;
  options.include_event_end_ot_features = false;
  auto log = component_log();
  std::reverse(log.relations.begin(), log.relations.end());
  const auto table = minera::extract_ocel_features(
      log, minera::OcelFeatureVariant::kEvents, options);
  const auto start = column(table, "@@event_start_order");
  assert(table.values[4][start] == 1.0 && table.values[3][start] == 0.0);
}

void event_string_attributes_contract() {
  auto options = event_minimal();
  options.string_event_attributes = {"kind", "fraction"};
  auto log = component_log();
  log.events[0].attributes.push_back({"fraction", 0.1});
  const auto table = minera::extract_ocel_features(
      log, minera::OcelFeatureVariant::kEvents, options);
  const auto empty = column(table, "@@event_str_kind_");
  const auto fast = column(table, "@@event_str_kind_fast");
  const auto truth = column(table, "@@event_str_kind_True");
  const auto fraction = column(table, "@@event_str_fraction_0.1");
  assert(table.values[0][empty] == 1.0 && table.values[1][fast] == 1.0 &&
         table.values[2][truth] == 1.0 && table.values[0][fraction] == 1.0);
  for (const auto& name : table.feature_names)
    assert(name.find("nullptr") == std::string::npos);
}

void event_timestamp_contract() {
  auto options = event_minimal();
  options.include_timestamp_features = true;
  auto log = component_log();
  log.events[1].timestamp = "2026-01-01t01:00z";
  const auto table = minera::extract_ocel_features(
      log, minera::OcelFeatureVariant::kEvents, options);
  const auto timestamp = column(table, "@@event_timestamp");
  const auto hour = column(table, "@@event_timestamp_hour");
  const auto weekday = column(table, "@@event_timestamp_dayofweek");
  assert(table.values[0][hour] == 0.0 && table.values[0][weekday] == 3.0 &&
         table.values[1][timestamp] ==
             *minera::parse_timestamp("2026-01-01T01:00Z") &&
         table.values[1][hour] == 1.0);
  auto invalid = component_log();
  invalid.events.front().timestamp = "not-a-timestamp";
  bool rejected = false;
  try {
    (void)minera::extract_ocel_features(
        invalid, minera::OcelFeatureVariant::kEvents, options);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

void event_new_interactions_contract() {
  auto options = event_minimal();
  options.feature_name_style = minera::OcelFeatureNameStyle::kCompatibility;
  options.include_new_interactions_features = true;
  const auto table = minera::extract_ocel_features(
      component_log(), minera::OcelFeatureVariant::kEvents, options);
  const auto interactions = column(table, "@@ev_new_interactions");
  assert(table.values[1][interactions] == 1.0 &&
         table.values[2][interactions] == 0.0 &&
         table.values[4][interactions] == 0.0);
}

void related_objects_features_contract() {
  auto options = event_minimal();
  options.include_related_objects_features = true;
  const auto table = minera::extract_ocel_features(
      component_log(), minera::OcelFeatureVariant::kEvents, options);
  const auto length = column(table,
                             "@@rel_obj_fea_min_@@object_lifecycle_length");
  assert(table.values[1][length] == 3.0);
}

void object_lifecycle_activities_contract() {
  auto options = object_minimal();
  options.include_lifecycle_activity_features = true;
  const auto table = minera::extract_ocel_features(
      component_log(), minera::OcelFeatureVariant::kObjects, options);
  const auto activity = column(table, "@@ocel_lif_activity_A");
  assert(table.values[0][activity] == 3.0);
}

void object_lifecycle_duration_contract() {
  auto options = object_minimal();
  options.include_lifecycle_duration_features = true;
  const auto table = minera::extract_ocel_features(
      component_log(), minera::OcelFeatureVariant::kObjects, options);
  const auto duration = column(table, "@@object_lifecycle_duration");
  assert(table.values[0][duration] == 3.0 * 3600.0);
}

void object_lifecycle_length_contract() {
  auto options = object_minimal();
  options.include_lifecycle_length_features = true;
  const auto table = minera::extract_ocel_features(
      component_log(), minera::OcelFeatureVariant::kObjects, options);
  const auto length = column(table, "@@object_lifecycle_length");
  assert(table.values[0][length] == 5.0 && table.values[2][length] == 0.0);
}

void object_lifecycle_paths_contract() {
  auto options = object_minimal();
  options.include_lifecycle_path_features = true;
  const auto table = minera::extract_ocel_features(
      component_log(), minera::OcelFeatureVariant::kObjects, options);
  const auto path = column(table, "@@ocel_lif_path_A##B");
  assert(table.values[0][path] == 2.0);
}

void object_lifecycle_unq_act_contract() {
  auto options = object_minimal();
  options.include_lifecycle_unique_activity_features = true;
  const auto table = minera::extract_ocel_features(
      component_log(), minera::OcelFeatureVariant::kObjects, options);
  const auto unique = column(table, "@@object_lifecycle_unq_act");
  assert(table.values[0][unique] == 2.0);
}

void object_num_attributes_contract() {
  auto options = object_minimal();
  options.numeric_object_attributes = {"priority"};
  const auto table = minera::extract_ocel_features(
      component_log(), minera::OcelFeatureVariant::kObjects, options);
  const auto priority = column(table, "@@object_num_priority");
  assert(table.values[0][priority] == 1.0 && table.values[2][priority] == 0.0);
}

void object_str_attributes_contract() {
  auto options = object_minimal();
  options.string_object_attributes = {"label", "fraction"};
  auto log = component_log();
  log.objects[0].attributes.push_back({"fraction", 0.1});
  const auto table = minera::extract_ocel_features(
      log, minera::OcelFeatureVariant::kObjects, options);
  const auto empty = column(table, "@@object_str_label_");
  const auto blue = column(table, "@@object_str_label_blue");
  const auto fraction = column(table, "@@object_str_fraction_0.1");
  assert(table.values[0][empty] == 1.0 && table.values[1][blue] == 1.0 &&
         table.values[0][fraction] == 1.0);
}

void object_work_in_progress_contract() {
  auto options = object_minimal();
  options.include_lifecycle_wip_features = true;
  const auto table = minera::extract_ocel_features(
      component_log(), minera::OcelFeatureVariant::kObjects, options);
  const auto wip = column(table, "@@object_wip");
  assert(table.values[2][wip] == 1.0);
}

minera::OcelFeatureOptions graph_minimal() {
  auto options = event_minimal();
  options.include_lifecycle_features = false;
  return options;
}

void obj_con_in_graph_features_contract() {
  auto options = graph_minimal();
  options.include_obj_con_in_graph_features = true;
  options.numeric_object_attributes = {"priority"};
  const auto table = minera::extract_ocel_features(
      graph_log(), minera::OcelFeatureVariant::kObjects, options);
  const auto minimum = column(table,
                              "@@obj_graph_con_min_@@object_lifecycle_length");
  assert(table.values[0][minimum] == 2.0);

  options.object_neighbor_graph =
      minera::OcelObjectNeighborGraph::kCobirth;
  const auto cobirth = minera::extract_ocel_features(
      graph_log(), minera::OcelFeatureVariant::kObjects, options);
  const auto cobirth_min = column(cobirth,
                                  "@@obj_graph_con_min_@@object_num_priority");
  const auto cobirth_max = column(cobirth,
                                  "@@obj_graph_con_max_@@object_num_priority");
  assert(cobirth.values[0][cobirth_min] == 5.0 &&
         cobirth.values[0][cobirth_max] == 5.0);

  options.object_neighbor_graph =
      minera::OcelObjectNeighborGraph::kCodeath;
  const auto codeath = minera::extract_ocel_features(
      graph_log(), minera::OcelFeatureVariant::kObjects, options);
  const auto codeath_min = column(codeath,
                                  "@@obj_graph_con_min_@@object_num_priority");
  assert(codeath.values[1][codeath_min] == 12.0);
}

void object_cobirth_graph_contract() {
  auto options = graph_minimal();
  options.include_cobirth_features = true;
  const auto table = minera::extract_ocel_features(
      graph_log(), minera::OcelFeatureVariant::kObjects, options);
  const auto cobirth = column(table, "@@object_cobirth");
  assert(table.values[0][cobirth] == 1.0 && table.values[1][cobirth] == 1.0);
}

void object_codeath_graph_contract() {
  auto options = graph_minimal();
  options.include_codeath_features = true;
  const auto table = minera::extract_ocel_features(
      graph_log(), minera::OcelFeatureVariant::kObjects, options);
  const auto codeath = column(table, "@@object_codeath");
  assert(table.values[1][codeath] == 1.0 && table.values[3][codeath] == 1.0);
}

void object_degree_centrality_contract() {
  auto options = graph_minimal();
  options.include_degree_centrality_features = true;
  const auto table = minera::extract_ocel_features(
      graph_log(), minera::OcelFeatureVariant::kObjects, options);
  const auto degree = column(table, "@@object_degree_centrality");
  assert(std::abs(table.values[0][degree] - 2.0 / 3.0) < 1e-12 &&
         table.values[4][degree] == 0.0);
}

void object_general_descendants_graph_contract() {
  auto options = graph_minimal();
  options.include_general_descendants_features = true;
  const auto table = minera::extract_ocel_features(
      graph_log(), minera::OcelFeatureVariant::kObjects, options);
  const auto descendants = column(
      table, "@@object_general_descendants_graph_descendants");
  assert(table.values[0][descendants] == 1.0);
}

void object_general_inheritance_graph_contract() {
  auto options = graph_minimal();
  options.include_general_inheritance_features = true;
  const auto table = minera::extract_ocel_features(
      graph_log(), minera::OcelFeatureVariant::kObjects, options);
  const auto descendants = column(
      table, "@@object_general_inheritance_graph_descendants");
  assert(table.values[0][descendants] == 1.0);
}

void object_general_interaction_graph_contract() {
  auto options = graph_minimal();
  options.include_general_interaction_features = true;
  const auto table = minera::extract_ocel_features(
      graph_log(), minera::OcelFeatureVariant::kObjects, options);
  const auto interaction = column(table, "@@object_general_interaction_graph");
  assert(table.values[0][interaction] == 2.0);
}

void objects_interaction_graph_ot_contract() {
  auto options = graph_minimal();
  options.include_interaction_by_type_features = true;
  const auto table = minera::extract_ocel_features(
      graph_log(), minera::OcelFeatureVariant::kObjects, options);
  const auto item = column(table, "@@object_interaction_graph_ot_item");
  assert(table.values[2][item] == 1.0 && table.values[3][item] == 1.0);
}

void related_activities_features_contract() {
  auto options = graph_minimal();
  options.include_related_activities_features = true;
  options.numeric_event_attributes = {"score"};
  const auto table = minera::extract_ocel_features(
      component_log(), minera::OcelFeatureVariant::kObjects, options);
  const auto score = column(table, "@@ev_act_fea_A_@@event_num_score");
  assert(table.values[0][score] == 4.0);
}

void related_events_features_contract() {
  auto options = graph_minimal();
  options.include_related_events_features = true;
  options.numeric_event_attributes = {"score"};
  const auto table = minera::extract_ocel_features(
      component_log(), minera::OcelFeatureVariant::kObjects, options);
  const auto minimum = column(table, "@@rel_eve_min_@@event_num_score");
  assert(table.values[0][minimum] == 2.0);
}

}  // namespace

int main() {
  prefix_features_contract();
  event_activity_contract();
  event_end_ot_contract();
  event_numeric_attributes_contract();
  event_num_rel_objs_contract();
  event_num_rel_objs_type_contract();
  event_start_ot_contract();
  event_string_attributes_contract();
  event_timestamp_contract();
  event_new_interactions_contract();
  related_objects_features_contract();
  object_lifecycle_activities_contract();
  object_lifecycle_duration_contract();
  object_lifecycle_length_contract();
  object_lifecycle_paths_contract();
  object_lifecycle_unq_act_contract();
  object_num_attributes_contract();
  object_str_attributes_contract();
  object_work_in_progress_contract();
  obj_con_in_graph_features_contract();
  object_cobirth_graph_contract();
  object_codeath_graph_contract();
  object_degree_centrality_contract();
  object_general_descendants_graph_contract();
  object_general_inheritance_graph_contract();
  object_general_interaction_graph_contract();
  objects_interaction_graph_ot_contract();
  related_activities_features_contract();
  related_events_features_contract();
}
