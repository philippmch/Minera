#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "minera/ocel.hpp"

namespace minera {

[[nodiscard]] Ocel filter_ocel_event_attribute(
    const Ocel& log, std::string_view attribute_key,
    const std::vector<OcelValue>& values, bool positive = true);

[[nodiscard]] Ocel filter_ocel_object_attribute(
    const Ocel& log, std::string_view attribute_key,
    const std::vector<OcelValue>& values, bool positive = true);

[[nodiscard]] Ocel filter_ocel_object_types(
    const Ocel& log, const std::vector<std::string>& types,
    bool positive = true);

[[nodiscard]] Ocel filter_ocel_objects(
    const Ocel& log, const std::vector<std::string>& object_ids,
    bool positive = true);

[[nodiscard]] Ocel filter_ocel_events(
    const Ocel& log, const std::vector<std::string>& event_ids,
    bool positive = true);

[[nodiscard]] Ocel filter_ocel_events_timestamp(
    const Ocel& log, std::string_view from, std::string_view to);

// Keep only event-object relations whose activity is allowed for the related
// object's type, then propagate the relation selection to all other tables.
[[nodiscard]] Ocel filter_ocel_activity_type_matching(
    const Ocel& log,
    const std::map<std::string, std::vector<std::string>>& allowed);

// Count event-object links by object type. Events with no valid relation are
// absent from the result; nested maps are deterministic.
[[nodiscard]] std::map<std::string, std::map<std::string, std::size_t>>
ocel_event_object_type_counts(const Ocel& log);

// Keep events related to at least the requested number of objects of every
// listed type, then propagate the selection to objects and relations.
[[nodiscard]] Ocel filter_ocel_events_object_type_counts(
    const Ocel& log,
    const std::map<std::string, std::size_t>& minimum_counts);

// Keep the first/last relation event for each object of the selected type.
[[nodiscard]] Ocel filter_ocel_start_events_per_object_type(
    const Ocel& log, std::string_view object_type);

[[nodiscard]] Ocel filter_ocel_end_events_per_object_type(
    const Ocel& log, std::string_view object_type);

}  // namespace minera
