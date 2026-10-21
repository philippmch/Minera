#pragma once

#include <optional>
#include <string_view>

#include "minera/ocdfg_edge_performance_options.hpp"
#include "minera/request_params.hpp"

namespace minera {

// Slots use finite numeric JSON pairs. Null and an empty array remain distinct
// and range/reversed endpoints retain the native working-time semantics.
[[nodiscard]] OcdfgBusinessHourSlots parse_ocdfg_business_hour_slots_json(
    std::string_view);
// No supplied performance controls means the original discovery path remains
// selected. Present empty attribute names are literal column selectors.
[[nodiscard]] std::optional<OcdfgEdgePerformanceOptions>
parse_ocdfg_performance_controls(const FieldLookup&, const FieldPresence&);

}  // namespace minera
