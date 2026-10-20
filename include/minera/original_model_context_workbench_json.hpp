#pragma once
#include "minera/original_model_context.hpp"
#include "minera/discovery_workbench_json.hpp"
namespace minera {
[[nodiscard]] DeclareModel parse_original_context_declare_json(std::string_view);
[[nodiscard]] OrderedDeclareModel parse_original_context_ordered_declare_json(std::string_view);
[[nodiscard]] LogSkeletonModel parse_original_context_skeleton_json(std::string_view);
[[nodiscard]] TemporalProfile parse_original_context_temporal_json(std::string_view);
[[nodiscard]] PetriNet parse_original_context_net_json(std::string_view);
[[nodiscard]] PnmlDocument parse_original_context_document_json(std::string_view);
[[nodiscard]] TokenMarking parse_original_context_marking_json(std::string_view);
[[nodiscard]] std::string serialize_original_context_model_json(const DeclareModel&);
[[nodiscard]] std::string serialize_original_context_model_json(const OrderedDeclareModel&);
[[nodiscard]] std::string serialize_original_context_model_json(const LogSkeletonModel&);
[[nodiscard]] std::string serialize_original_context_model_json(const TemporalProfile&);
[[nodiscard]] std::string serialize_original_context_model_json(const PetriNet&);
[[nodiscard]] std::string serialize_original_context_model_json(const PnmlDocument&);
[[nodiscard]] DiscoveryWorkbenchResponse execute_original_model_context(
 const std::vector<DiscoveryWorkbenchPart>& parts,const std::vector<DiscoveryWorkbenchPart>& query={},
 std::string_view input_order="provided_order",std::string_view transport_error={});
} // namespace minera
