#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "minera/ocel.hpp"
#include "minera/timestamp.hpp"

namespace minera {

[[nodiscard]] Ocel filter_ocel_event_attribute(
    const Ocel& log, std::string_view attribute_key,
    const std::vector<OcelValue>& values, bool positive = true);

// Public categorical filtering preserves the distinction between a timestamp
// value and a literal string, and compares bool/integer/real scalars numerically.
// A typed datetime missing value is distinct from both null and numeric NaN.
struct OcelAttributeNaT {};
using OcelEventAttributeFilterValue =
    std::variant<std::nullptr_t, bool, std::int64_t, double, std::string,
                 ParsedExactTimestamp, OcelAttributeNaT>;
enum class OcelEventAttributeColumnKind { kAutomatic, kScalar, kTimestamp };
[[nodiscard]] Ocel filter_ocel_event_attribute_exact(
    const Ocel& log, std::string_view attribute_key,
    const std::vector<OcelEventAttributeFilterValue>& values,
    bool positive = true,
    OcelEventAttributeColumnKind column_kind =
        OcelEventAttributeColumnKind::kAutomatic);

// Strings use the public whole-second date format; parsed timestamps represent
// datetime arguments with their exact fractional instant and timezone state.
using OcelEventTimestampBound = std::variant<std::string, ParsedExactTimestamp>;
[[nodiscard]] Ocel filter_ocel_events_timestamp_exact(
    const Ocel& log, const OcelEventTimestampBound& from,
    const OcelEventTimestampBound& to,
    std::string_view timestamp_key = "ocel:timestamp");

[[nodiscard]] Ocel filter_ocel_object_attribute(
    const Ocel& log, std::string_view attribute_key,
    const std::vector<OcelValue>& values, bool positive = true);

using OcelObjectAttributeFilterValue = OcelEventAttributeFilterValue;
using OcelObjectAttributeColumnKind = OcelEventAttributeColumnKind;
using OcelObjectAttributeNaT = OcelAttributeNaT;
[[nodiscard]] Ocel filter_ocel_object_attribute_exact(
    const Ocel& log, std::string_view attribute_key,
    const std::vector<OcelObjectAttributeFilterValue>& values,
    bool positive = true,
    OcelObjectAttributeColumnKind column_kind =
        OcelObjectAttributeColumnKind::kAutomatic);

[[nodiscard]] Ocel filter_ocel_object_types(
    const Ocel& log, const std::vector<std::string>& types,
    bool positive = true, std::size_t level = 1);

[[nodiscard]] Ocel filter_ocel_objects(
    const Ocel& log, const std::vector<std::string>& object_ids,
    bool positive = true, std::size_t level = 1);

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

// Exact signed integer and binary64 lower bounds for the public object-type
// count filter. Relation rows, including duplicates, contribute to the count;
// a required type must occur even when its lower bound is zero or negative.
using OcelObjectTypeCountThreshold = std::variant<TokenInteger, double>;
[[nodiscard]] Ocel filter_ocel_events_object_type_counts_exact(
    const Ocel& log,
    const std::map<std::string, OcelObjectTypeCountThreshold>& minimum_counts);

// Keep the first/last relation event for each object of the selected type.
[[nodiscard]] Ocel filter_ocel_start_events_per_object_type(
    const Ocel& log, std::string_view object_type);

[[nodiscard]] Ocel filter_ocel_end_events_per_object_type(
    const Ocel& log, std::string_view object_type);

// Keep all relations whose event activity occurs on at least one object of
// the selected type, including relations to other object types.
[[nodiscard]] Ocel filter_ocel_activities_connected_object_type(
    const Ocel& log, std::string_view object_type);

[[nodiscard]] Ocel filter_ocel_cc_object(const Ocel& log,
                                         std::string_view object_id);

[[nodiscard]] Ocel filter_ocel_cc_length(const Ocel& log,
                                         std::size_t minimum_size,
                                         std::size_t maximum_size);

[[nodiscard]] Ocel filter_ocel_cc_object_type(const Ocel& log,
                                              std::string_view object_type,
                                              bool positive = true);

[[nodiscard]] Ocel filter_ocel_cc_activity(const Ocel& log,
                                           std::string_view activity);

}  // namespace minera
