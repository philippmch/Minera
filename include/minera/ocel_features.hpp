#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "minera/ocel.hpp"

namespace minera {

enum class OcelFeatureVariant {
  kEvents,
  kObjects,
  kEventsObjects,
  EVENTS = kEvents,
  OBJECTS = kObjects,
  EVENTS_OBJECTS = kEventsObjects,
};

struct OcelFeatureOptions {
  bool include_activity_features = true;
  bool include_timestamp_features = true;
  bool include_relation_features = true;
  bool include_endpoint_features = true;
  bool include_attribute_counts = true;
  bool include_interaction_features = true;
  // Normalized object interaction-graph degree. When unset, this follows the
  // legacy interaction toggle; an explicit value enables independent control.
  // The control applies only to OBJECTS.
  std::optional<bool> include_degree_centrality_features;
  bool include_interaction_by_type_features = false;
  // Optional object-graph scalar columns. These are valid only for OBJECTS.
  bool include_general_interaction_features = false;
  bool include_general_descendants_features = false;
  bool include_general_inheritance_features = false;
  bool include_cobirth_features = false;
  bool include_codeath_features = false;
  bool include_lifecycle_features = true;
  bool include_lifecycle_length_features = true;
  bool include_lifecycle_unique_activity_features = true;
  bool include_lifecycle_duration_features = true;
  bool include_lifecycle_activity_features = true;
  bool include_lifecycle_wip_features = true;
  bool include_lifecycle_path_features = true;
  bool include_related_events_features = false;
  bool include_related_activities_features = false;
  // Aggregate the selected object feature vector over each event's related
  // objects. This selector is valid only for EVENTS and is disabled by default.
  bool include_related_objects_features = false;
  bool include_obj_con_in_graph_features = false;
  // Relation-prefix columns are available only for EVENTS_OBJECTS. They are
  // enabled by default there and describe each relation's object-local past.
  bool include_prefix_features = true;
  bool include_prefix_length_features = true;
  bool include_prefix_timediff_features = true;
  bool include_prefix_one_hot_features = true;
  std::vector<std::string> numeric_event_attributes;
  std::vector<std::string> string_event_attributes;
  std::vector<std::string> numeric_object_attributes;
  std::vector<std::string> string_object_attributes;
  // When set, OBJECTS output contains only rows with this object type.
  // This selector is invalid for EVENTS and EVENTS_OBJECTS output.
  std::optional<std::string> filter_per_type;
  std::size_t max_features = 10000;
};

struct OcelFeatureTable {
  OcelFeatureVariant variant = OcelFeatureVariant::kEvents;
  std::vector<std::string> row_ids;
  std::vector<std::string> row_types;
  std::vector<std::string> feature_names;
  std::vector<std::vector<double>> values;
};

[[nodiscard]] OcelFeatureTable extract_ocel_features(
    const Ocel& log, OcelFeatureVariant variant = OcelFeatureVariant::kEvents,
    const OcelFeatureOptions& options = {});

[[nodiscard]] std::string serialize_ocel_features_json(
    const OcelFeatureTable& table);

[[nodiscard]] const char* ocel_feature_variant_name(
    OcelFeatureVariant variant) noexcept;

}  // namespace minera
