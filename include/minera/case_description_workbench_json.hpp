#pragma once
#include "minera/case_description.hpp"
#include "minera/discovery_workbench_json.hpp"
namespace minera {
// Public-value receipt schema: attributes is a tagged builtins.dict; events
// is an array of tagged builtins.dict values. Dictionary items retain order.
[[nodiscard]] CaseDescription parse_case_description_json(std::string_view);
[[nodiscard]] std::string serialize_case_description_json(const CaseDescription&);
[[nodiscard]] CaseDescriptionOptions parse_case_description_options_json(std::string_view);
[[nodiscard]] std::string serialize_case_description_options_json(const CaseDescriptionOptions&);
[[nodiscard]] DiscoveryWorkbenchResponse execute_supplied_case_context(
 const std::vector<DiscoveryWorkbenchPart>& parts,const std::vector<DiscoveryWorkbenchPart>& query={},
 std::string_view input_order="provided_order",std::string_view transport_error={});
} // namespace minera
