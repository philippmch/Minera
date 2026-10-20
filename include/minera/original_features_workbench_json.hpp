#pragma once

#include "minera/discovery_workbench_json.hpp"
#include "minera/log_to_features.hpp"
#include <string_view>

namespace minera {
[[nodiscard]] LogToFeaturesOptions parse_original_features_options_json(std::string_view);
[[nodiscard]] LogToFeaturesResult parse_original_features_result_json(std::string_view);
[[nodiscard]] DiscoveryWorkbenchResponse execute_original_features_workbench(
    const std::vector<DiscoveryWorkbenchPart>&,
    const std::vector<DiscoveryWorkbenchPart>& query = {},
    std::string_view input_order = "provided_order",
    std::string_view transport_error = {});
} // namespace minera
