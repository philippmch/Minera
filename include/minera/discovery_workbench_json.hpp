#pragma once
#include <string>
#include <string_view>
#include <vector>
#include "minera/ilp_miner.hpp"
#include "minera/genetic_miner.hpp"
namespace minera {
// Physical multipart order and filename presence are independent of content type.
struct DiscoveryWorkbenchPart { std::string name, filename, bytes; bool file = false; };
struct DiscoveryWorkbenchResponse { int status = 200; std::string json; };
[[nodiscard]] DiscoveryWorkbenchResponse execute_discovery_workbench(const std::vector<DiscoveryWorkbenchPart>&);
[[nodiscard]] std::vector<DiscoveryWorkbenchPart> parse_discovery_workbench_multipart(std::string_view content_type,std::string_view body);
[[nodiscard]] DiscoveryWorkbenchResponse execute_discovery_workbench_multipart(std::string_view content_type,std::string_view body,const std::vector<DiscoveryWorkbenchPart>& query = {},bool transport_complete = true);
} // namespace minera
