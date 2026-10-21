#pragma once
#include "minera/discovery_workbench_json.hpp"
namespace minera {
[[nodiscard]] DiscoveryWorkbenchResponse execute_ltl_filter_workbench(
    const std::vector<DiscoveryWorkbenchPart>& parts,
    const std::vector<DiscoveryWorkbenchPart>& query={},std::string_view transport_error={});
}
