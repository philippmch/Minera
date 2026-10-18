#pragma once
#include <string>
#include <string_view>
#include <vector>
#include "minera/concept_drift.hpp"
namespace minera {
// Physical multipart order and filename presence are independent of content type.
struct BoseWorkbenchPart { std::string name, filename, bytes; bool file = false; };
struct BoseWorkbenchResponse { int status = 200; std::string json; };
[[nodiscard]] BoseWorkbenchResponse execute_bose_workbench(const std::vector<BoseWorkbenchPart>&);
[[nodiscard]] std::vector<BoseWorkbenchPart> parse_bose_workbench_multipart(std::string_view content_type,std::string_view body);
[[nodiscard]] BoseWorkbenchResponse execute_bose_workbench_multipart(std::string_view content_type,std::string_view body,const std::vector<BoseWorkbenchPart>& query = {},bool transport_complete = true);
} // namespace minera
