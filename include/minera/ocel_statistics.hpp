#pragma once

#include <cstddef>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include "minera/ocel.hpp"
#include "minera/temporal_profile.hpp"

namespace minera {

// Options are reserved for future activity/object-type selection.  Native
// OCEL records already expose the fixed fields used by these summaries.
struct OcelObjectTypeActivityOptions {};

using OcelObjectTypeActivities =
    std::map<std::string, std::set<std::string>>;
using OcelObjectsOtCount =
    std::map<std::string, std::map<std::string, std::size_t>>;

// Deterministic association summaries used by the native OCEL statistics
// implementation.  The pair is (event id, object id).
using OcelAssociationPair = std::pair<std::string, std::string>;
using OcelActivityAssociations =
    std::map<std::string, std::set<OcelAssociationPair>>;
using OcelObjectTypeActivityAssociations =
    std::map<std::string, OcelActivityAssociations>;
using OcelActivityAssociationMap = OcelActivityAssociations;
using OcelObjectTypeAssociationMap = OcelObjectTypeActivityAssociations;

enum class OcelAssociationPrefilter {
  kNone,
  kStart,
  kEnd,
  NONE = kNone,
  START = kStart,
  END = kEnd,
};
using OcelAssociationFilter = OcelAssociationPrefilter;
using OcelPrefilterMode = OcelAssociationPrefilter;

struct OcelAssociationAggregates {
  std::set<std::string> event_ids;
  std::set<std::string> object_ids;
  std::size_t pair_count = 0;
};

using OcelEdgeAssociations =
    std::map<std::string,
             std::map<std::pair<std::string, std::string>, OcdfgEdgeMetric>>;
using OcelEdgeAssociationMap = OcelEdgeAssociations;

struct OcelEdgeAssociationOptions {
  // When enabled, performance samples measure active seconds in the supplied
  // weekly schedule instead of absolute timestamp seconds.  Empty slots use
  // the deterministic Monday-Friday 07:00-17:00 schedule.
  bool use_business_hours = false;
  std::vector<BusinessHourSlot> business_hour_slots;
};

[[nodiscard]] OcelActivityAssociations activity_associations(
    const Ocel& log,
    OcelAssociationPrefilter prefilter = OcelAssociationPrefilter::kNone);
[[nodiscard]] OcelObjectTypeActivityAssociations
object_type_activity_associations(
    const Ocel& log,
    OcelAssociationPrefilter prefilter = OcelAssociationPrefilter::kNone);
[[nodiscard]] OcelAssociationAggregates aggregate_associations(
    const OcelActivityAssociations& associations);
[[nodiscard]] std::set<std::string> association_event_ids(
    const OcelActivityAssociations& associations);
[[nodiscard]] std::set<std::string> association_object_ids(
    const OcelActivityAssociations& associations);
[[nodiscard]] std::size_t association_pair_count(
    const OcelActivityAssociations& associations);
[[nodiscard]] OcelEdgeAssociations edge_associations(
    const Ocel& log,
    const OcelEdgeAssociationOptions& options = {});

// Explicitly named aliases for callers that prefer the ``find_*`` spelling.
[[nodiscard]] inline OcelActivityAssociations find_activity_associations(
    const Ocel& log,
    OcelAssociationPrefilter prefilter = OcelAssociationPrefilter::kNone) {
  return activity_associations(log, prefilter);
}
[[nodiscard]] inline OcelObjectTypeActivityAssociations
find_object_type_activity_associations(
    const Ocel& log,
    OcelAssociationPrefilter prefilter = OcelAssociationPrefilter::kNone) {
  return object_type_activity_associations(log, prefilter);
}
[[nodiscard]] inline OcelEdgeAssociations find_edge_associations(
    const Ocel& log,
    const OcelEdgeAssociationOptions& options = {}) {
  return edge_associations(log, options);
}
[[nodiscard]] inline std::set<std::string> association_events(
    const OcelActivityAssociations& associations) {
  return association_event_ids(associations);
}
[[nodiscard]] inline std::set<std::string> association_objects(
    const OcelActivityAssociations& associations) {
  return association_object_ids(associations);
}
[[nodiscard]] inline std::size_t association_count(
    const OcelActivityAssociations& associations) {
  return association_pair_count(associations);
}

enum class OcelStatisticsVariant {
  kObjectTypeActivities,
  kObjectsOtCount,
  kActivityAssociations,
  kObjectTypeActivityAssociations,
  kEdgeAssociations,
  OBJECT_TYPE_ACTIVITIES = kObjectTypeActivities,
  OBJECTS_OT_COUNT = kObjectsOtCount,
  ACTIVITY_ASSOCIATIONS = kActivityAssociations,
  OBJECT_TYPE_ACTIVITY_ASSOCIATIONS = kObjectTypeActivityAssociations,
  EDGE_ASSOCIATIONS = kEdgeAssociations,
};

// Returns the distinct activities related to each object type.  Only valid
// event-object relations contribute; each activity is included once per type.
[[nodiscard]] OcelObjectTypeActivities object_type_activities(
    const Ocel& log,
    const OcelObjectTypeActivityOptions& options = {});

// Counts event-object relation rows by event id and object type.  Duplicate
// relation rows are counted independently.
[[nodiscard]] OcelObjectsOtCount objects_ot_count(const Ocel& log);

[[nodiscard]] const char* ocel_statistics_variant_name(
    OcelStatisticsVariant variant) noexcept;

// Serializes either summary map in a deterministic, self-describing JSON
// envelope.  The overload taking a log computes the selected variant first.
[[nodiscard]] std::string serialize_ocel_statistics_json(
    const OcelObjectTypeActivities& activities);
[[nodiscard]] std::string serialize_ocel_statistics_json(
    const OcelObjectsOtCount& counts);
[[nodiscard]] std::string serialize_ocel_statistics_json(
    const OcelActivityAssociations& associations);
[[nodiscard]] std::string serialize_ocel_statistics_json(
    const OcelObjectTypeActivityAssociations& associations);
[[nodiscard]] std::string serialize_ocel_statistics_json(
    const OcelEdgeAssociations& associations);
[[nodiscard]] std::string serialize_ocel_statistics_json(
    const Ocel& log,
    OcelStatisticsVariant variant =
        OcelStatisticsVariant::kObjectTypeActivities);
[[nodiscard]] std::string serialize_ocel_statistics_json(
    const Ocel& log, OcelStatisticsVariant variant,
    const OcelEdgeAssociationOptions& edge_options);

// Descriptive aliases for callers that prefer an explicit getter naming
// convention or an OCEL-prefixed function name.
[[nodiscard]] inline OcelObjectTypeActivities get_object_type_activities(
    const Ocel& log,
    const OcelObjectTypeActivityOptions& options = {}) {
  return object_type_activities(log, options);
}

[[nodiscard]] inline OcelObjectTypeActivities ocel_object_type_activities(
    const Ocel& log,
    const OcelObjectTypeActivityOptions& options = {}) {
  return object_type_activities(log, options);
}

[[nodiscard]] inline OcelObjectsOtCount get_objects_ot_count(const Ocel& log) {
  return objects_ot_count(log);
}

[[nodiscard]] inline OcelObjectsOtCount ocel_objects_ot_count(const Ocel& log) {
  return objects_ot_count(log);
}

}  // namespace minera
