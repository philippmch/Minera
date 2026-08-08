#pragma once

#include <cstddef>
#include <map>
#include <set>
#include <string>
#include <string_view>

#include "minera/ocel.hpp"

namespace minera {

// Options are reserved for future activity/object-type selection.  Native
// OCEL records already expose the fixed fields used by these summaries.
struct OcelObjectTypeActivityOptions {};

using OcelObjectTypeActivities =
    std::map<std::string, std::set<std::string>>;
using OcelObjectsOtCount =
    std::map<std::string, std::map<std::string, std::size_t>>;

enum class OcelStatisticsVariant {
  kObjectTypeActivities,
  kObjectsOtCount,
  OBJECT_TYPE_ACTIVITIES = kObjectTypeActivities,
  OBJECTS_OT_COUNT = kObjectsOtCount,
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
    const Ocel& log,
    OcelStatisticsVariant variant =
        OcelStatisticsVariant::kObjectTypeActivities);

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
