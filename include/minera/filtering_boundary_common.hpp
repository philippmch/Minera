#pragma once

#include "minera/boundary_activities.hpp"

namespace minera {

// An insertion-ordered frequency mapping. Labels are exact byte strings and
// must be unique; counts have the native nonnegative frequency domain.
using OrderedBoundaryActivityCounts =
    std::vector<std::pair<std::string, std::uint64_t>>;

// Stable descending frequency order. Equal counts retain input mapping order.
[[nodiscard]] SortedBoundaryActivities get_sorted_start_activities_list(
    const OrderedBoundaryActivityCounts& counts);
[[nodiscard]] SortedBoundaryActivities get_sorted_end_activities_list(
    const OrderedBoundaryActivityCounts& counts);

// The supplied sequence order is significant. Empty input raises out_of_range;
// factors retain their complete double domain, including NaN and infinities.
[[nodiscard]] std::uint64_t get_start_activities_threshold(
    const SortedBoundaryActivities& activities, double decreasing_factor);
[[nodiscard]] std::uint64_t get_end_activities_threshold(
    const SortedBoundaryActivities& activities, double decreasing_factor);

}  // namespace minera
