#pragma once
#include "minera/discovery_workbench_json.hpp"

namespace minera {
// Native EventTable JSON only: engine selection never converts a table to a log.
[[nodiscard]] DiscoveryWorkbenchResponse execute_table_attribute_repetition(
    const std::vector<DiscoveryWorkbenchPart>& parts,
    const std::vector<DiscoveryWorkbenchPart>& query = {},
    std::string_view transport_error = {});
}
