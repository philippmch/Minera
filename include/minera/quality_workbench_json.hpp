#pragma once
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "minera/evaluation.hpp"
#include "minera/pnml.hpp"
#include "minera/discovery_workbench_json.hpp"
namespace minera {
// Exact scoped constructor/options readers. Native consumers own semantics.
// Full outputs retain uncomputed/inactive payloads and have no input caps.
// Cost-function JSON members describe binding presence; execute_* binds the
// separately supplied finite callback tables before entering an evaluator.
[[nodiscard]] ReplayFitnessOptions parse_quality_replay_options_json(std::string_view,std::set<std::string>* supplied=nullptr);
[[nodiscard]] std::string serialize_quality_workbench_options_json(const ReplayFitnessOptions&);
[[nodiscard]] PrecisionOptions parse_quality_precision_options_json(std::string_view,std::set<std::string>* supplied=nullptr);
[[nodiscard]] std::string serialize_quality_workbench_options_json(const PrecisionOptions&);
[[nodiscard]] GeneralizationOptions parse_quality_generalization_options_json(std::string_view,std::set<std::string>* supplied=nullptr);
[[nodiscard]] std::string serialize_quality_workbench_options_json(const GeneralizationOptions&);
[[nodiscard]] SimplicityOptions parse_quality_simplicity_options_json(std::string_view,std::set<std::string>* supplied=nullptr);
[[nodiscard]] std::string serialize_quality_workbench_options_json(const SimplicityOptions&);
[[nodiscard]] CompositeEvaluationOptions parse_quality_composite_options_json(std::string_view,std::set<std::string>* supplied=nullptr);
[[nodiscard]] std::string serialize_quality_workbench_options_json(const CompositeEvaluationOptions&);
[[nodiscard]] std::string serialize_quality_workbench_result_json(const ReplayFitnessResult&);
[[nodiscard]] std::string serialize_quality_workbench_result_json(const PrecisionResult&);
[[nodiscard]] std::string serialize_quality_workbench_result_json(const GeneralizationResult&);
[[nodiscard]] std::string serialize_quality_workbench_result_json(const SimplicityResult&);
[[nodiscard]] std::string serialize_quality_workbench_result_json(const CompositeEvaluationResult&);
[[nodiscard]] ProcessTree parse_quality_workbench_tree_json(std::string_view);
[[nodiscard]] std::string serialize_quality_workbench_tree_json(const ProcessTree&);
[[nodiscard]] PnmlDocument parse_quality_workbench_document_json(std::string_view);
[[nodiscard]] std::string serialize_quality_workbench_document_json(const PnmlDocument&);
[[nodiscard]] PetriNetPresentation parse_quality_workbench_presentation_json(std::string_view);
[[nodiscard]] std::string serialize_quality_workbench_presentation_json(const PetriNetPresentation&);
[[nodiscard]] EventStream parse_quality_workbench_stream_json(std::string_view);
[[nodiscard]] std::string serialize_quality_workbench_stream_json(const EventStream&);
// Always complete raw7-field table carrier, including all10 metadata fields.
[[nodiscard]] std::string serialize_quality_workbench_table_json(const EventTable&);
struct QualityWorkbenchResponse { int status=200; std::string json; };
[[nodiscard]] QualityWorkbenchResponse execute_quality_workbench(const std::vector<DiscoveryWorkbenchPart>& parts,const std::vector<DiscoveryWorkbenchPart>& query={});
[[nodiscard]] QualityWorkbenchResponse execute_quality_workbench_multipart(std::string_view content_type,std::string_view body,const std::vector<DiscoveryWorkbenchPart>& query={},bool transport_complete=true);
} // namespace minera
