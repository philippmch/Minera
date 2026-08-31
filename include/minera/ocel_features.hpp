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

enum class OcelFeatureNameStyle {
  kNative,
  kCompatibility,
  NATIVE = kNative,
  COMPATIBILITY = kCompatibility,
};

enum class OcelInteractionGraphSource {
  kEventAndO2O,
  kEventCooccurrence,
  EVENT_AND_O2O = kEventAndO2O,
  EVENT_COOCCURRENCE = kEventCooccurrence,
};

// Graph family used by neighboring-object min/max projections.  The
// interaction family follows the normalized interaction source, while the
// lifecycle boundary families use the same event-order graph construction as
// their standalone OBJECTS columns.
enum class OcelObjectNeighborGraph {
  kInteraction,
  kCobirth,
  kCodeath,
  INTERACTION = kInteraction,
  COBIRTH = kCobirth,
  CODEATH = kCodeath,
};

struct OcelFeatureOptions {
  bool include_activity_features = true;
  OcelFeatureNameStyle feature_name_style = OcelFeatureNameStyle::kNative;
  bool include_timestamp_features = true;
  bool include_relation_features = true;
  // Event relation-count components inherit include_relation_features when
  // unset, while allowing callers to select the overall and per-type counts
  // independently.
  std::optional<bool> include_event_num_rel_objs_features;
  std::optional<bool> include_event_num_rel_objs_type_features;
  bool include_endpoint_features = true;
  // Event lifecycle endpoint components inherit include_endpoint_features
  // when unset and can be selected independently.
  std::optional<bool> include_event_start_ot_features;
  std::optional<bool> include_event_end_ot_features;
  bool include_attribute_counts = true;
  bool include_interaction_features = true;
  // Selects the edge source for normalized object interaction features and
  // the interaction family of neighboring-object projections. An omitted
  // value uses event co-occurrence; explicit O2O links can be included by
  // selecting kEventAndO2O.
  std::optional<OcelInteractionGraphSource> interaction_graph_source;
  // Event newly-observed interactions inherit include_interaction_features
  // when unset and can be selected independently.
  std::optional<bool> include_new_interactions_features;
  // Normalized object interaction-graph degree. When unset, this follows the
  // legacy interaction toggle; an explicit value enables independent control.
  // The control applies only to OBJECTS.
  std::optional<bool> include_degree_centrality_features;
  // When explicitly true, enable the complete OBJECTS feature preset,
  // including lifecycle metrics and the independent object graph families.
  // Unset and false preserve compact defaults and existing child controls.
  std::optional<bool> include_all_object_features;
  std::optional<bool> include_interaction_by_type_features;
  // Optional object-graph scalar columns. These are valid for OBJECTS and as
  // source controls in the related-object EVENTS context.
  std::optional<bool> include_general_interaction_features;
  std::optional<bool> include_general_descendants_features;
  std::optional<bool> include_general_inheritance_features;
  std::optional<bool> include_cobirth_features;
  std::optional<bool> include_codeath_features;
  // Selects the graph family used by neighboring-object projections.
  OcelObjectNeighborGraph object_neighbor_graph =
      OcelObjectNeighborGraph::kInteraction;
  // The lifecycle group is object-only. Unset preserves the object default
  // without making the control appear as an event-variant override.
  std::optional<bool> include_lifecycle_features;
  std::optional<bool> include_lifecycle_length_features;
  std::optional<bool> include_lifecycle_unique_activity_features;
  std::optional<bool> include_lifecycle_duration_features;
  std::optional<bool> include_lifecycle_activity_features;
  // Work-in-progress uses a directional start-window over lifecycle
  // intervals.  An unset value follows the compact default (disabled), while
  // an explicit value lets object callers opt in or suppress the column.
  std::optional<bool> include_lifecycle_wip_features;
  // Lifecycle paths are an opt-in extension; they are not part of the broad
  // reference-compatible object preset.
  std::optional<bool> include_lifecycle_path_features;
  bool include_related_events_features = false;
  bool include_related_activities_features = false;
  // Aggregate the selected object feature vector over each event's related
  // objects. This selector is valid only for EVENTS and is disabled by default.
  bool include_related_objects_features = false;
  bool include_obj_con_in_graph_features = false;
  // Relation-prefix columns are available only for EVENTS_OBJECTS. They are
  // enabled by default there and describe each relation's object-local past.
  bool include_prefix_features = true;
  // An unset child follows include_prefix_features.  This allows an
  // individual prefix family to be enabled while the group switch is false.
  std::optional<bool> include_prefix_length_features;
  std::optional<bool> include_prefix_timediff_features;
  std::optional<bool> include_prefix_one_hot_features;
  // EVENTS_OBJECTS pointwise projections inherit the complete exploded
  // schema when unset. Explicit controls apply only to that variant.
  std::optional<bool> include_event_pointwise_features;
  std::optional<bool> include_object_pointwise_features;
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
    OcelFeatureVariant variant);

}  // namespace minera
