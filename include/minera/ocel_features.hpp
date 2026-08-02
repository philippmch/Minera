#pragma once

#include <cstddef>
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
  bool include_lifecycle_features = true;
  std::vector<std::string> numeric_event_attributes;
  std::vector<std::string> string_event_attributes;
  std::vector<std::string> numeric_object_attributes;
  std::vector<std::string> string_object_attributes;
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
