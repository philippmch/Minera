#pragma once
#include "minera/ocel_filter.hpp"

namespace minera {
[[nodiscard]] std::vector<OcelEventAttributeFilterValue>
parse_ocel_event_filter_values_json(std::string_view);
[[nodiscard]] OcelEventTimestampBound
parse_ocel_event_timestamp_bound_json(std::string_view);
[[nodiscard]] std::map<std::string,OcelObjectTypeCountThreshold>
parse_ocel_exact_minimum_counts_json(std::string_view);
[[nodiscard]] std::map<std::string,std::vector<std::string>>
parse_ocel_exact_activity_type_correspondence_json(std::string_view);
[[nodiscard]] OcelEventAttributeColumnKind
parse_ocel_event_attribute_column_kind(std::string_view);
// Mark exact-filter interchange outputs so ordinary file opening can retain
// literal names through its reader, without changing unmarked input defaults.
[[nodiscard]] std::string serialize_ocel_exact_filter_json(
    const Ocel&,OcelJsonFormat = OcelJsonFormat::kClassic);
}
