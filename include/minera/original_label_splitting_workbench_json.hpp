#pragma once

#include "minera/discovery_workbench_json.hpp"
#include "minera/label_splitting.hpp"
#include <string_view>

namespace minera {
[[nodiscard]] LabelSplittingOptions parse_original_label_splitting_options_json(std::string_view);
[[nodiscard]] LabelSplittingResult parse_original_label_splitting_result_json(std::string_view);
[[nodiscard]] std::string serialize_original_label_splitting_options_json(const LabelSplittingOptions&);
[[nodiscard]] std::string serialize_original_label_splitting_result_json(const LabelSplittingResult&);
[[nodiscard]] DiscoveryWorkbenchResponse execute_original_label_splitting_workbench(
    const std::vector<DiscoveryWorkbenchPart>&,
    const std::vector<DiscoveryWorkbenchPart>& query = {},
    std::string_view input_order = "provided_order",
    std::string_view transport_error = {});
} // namespace minera
