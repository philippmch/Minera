#pragma once

#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "minera/discovery_workbench_json.hpp"
#include "minera/petri_analysis.hpp"

namespace minera {

// Exact native storage: structural parsing only; the selected public analysis
// function retains its own semantic validation and validation order.
[[nodiscard]] MarkingEquationOptions parse_petri_analysis_workbench_marking_options_json(
    std::string_view, std::set<std::string>* supplied = nullptr);
[[nodiscard]] ExtendedMarkingEquationOptions parse_petri_analysis_workbench_extended_options_json(
    std::string_view, std::set<std::string>* supplied = nullptr);
[[nodiscard]] SoundnessOptions parse_petri_analysis_workbench_soundness_options_json(
    std::string_view, std::set<std::string>* supplied = nullptr);

[[nodiscard]] std::string serialize_petri_analysis_workbench_options_json(const MarkingEquationOptions&);
[[nodiscard]] std::string serialize_petri_analysis_workbench_options_json(const ExtendedMarkingEquationOptions&);
[[nodiscard]] std::string serialize_petri_analysis_workbench_options_json(const SoundnessOptions&);
// Full stored results, including nonfinite NativeFloat64 values, unknown enum
// storage and disengaged versus engaged-empty optionals. No legacy validation.
// NativeFloat64 follows the existing semantic codec: NaN payload/sign/signalling
// state is canonicalized; raw IEEE bit identity is not claimed.
[[nodiscard]] std::string serialize_petri_analysis_workbench_result_json(const WorkflowMarkings&);
[[nodiscard]] std::string serialize_petri_analysis_workbench_result_json(const MarkingEquationResult&);
[[nodiscard]] std::string serialize_petri_analysis_workbench_result_json(const ExtendedMarkingEquationResult&);
[[nodiscard]] std::string serialize_petri_analysis_workbench_result_json(const WorkflowNetResult&);
[[nodiscard]] std::string serialize_petri_analysis_workbench_result_json(const SoundnessResult&);
[[nodiscard]] std::string serialize_petri_analysis_workbench_result_json(bool);

struct PetriAnalysisWorkbenchResponse { int status = 200; std::string json; };
// Parts retain physical filename-attribute presence/order. Query controls are
// separate; duplicate query/body controls are rejected by presence.
[[nodiscard]] PetriAnalysisWorkbenchResponse execute_petri_analysis_workbench(
    const std::vector<DiscoveryWorkbenchPart>& parts,
    const std::vector<DiscoveryWorkbenchPart>& query = {});
[[nodiscard]] PetriAnalysisWorkbenchResponse execute_petri_analysis_workbench_multipart(
    std::string_view content_type, std::string_view body,
    const std::vector<DiscoveryWorkbenchPart>& query = {}, bool transport_complete = true);

}  // namespace minera
