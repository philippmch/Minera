#pragma once

#include <set>
#include <string>
#include <string_view>

#include "minera/genetic_matrix_conversion.hpp"

namespace minera {

// Only native conversion enum storage; no genetic-mining parameters apply.
struct GeneticMatrixWorkbenchOptions {
  GeneticMatrixConversionVariant variant = GeneticMatrixConversionVariant::kToPetriNet;
};

// Activity values accept scalar JSON strings or exact NativeString hex-v1 tags.
// Writers preserve old quoted text only when its native-byte roundtrip is exact.
// These parsers preserve constructor storage. Semantic matrix validation and
// normalization belong exclusively to convert_genetic_matrix. Presence sets
// update only after the entire carrier parses successfully.
[[nodiscard]] GeneticCausalMatrix parse_genetic_matrix_workbench_model_json(
    std::string_view input, std::set<std::string>* supplied = nullptr);
[[nodiscard]] GeneticMatrixWorkbenchOptions parse_genetic_matrix_workbench_options_json(
    std::string_view input, std::set<std::string>* supplied = nullptr);
[[nodiscard]] std::string serialize_genetic_matrix_workbench_model_json(
    const GeneticCausalMatrix& matrix);
[[nodiscard]] std::string serialize_genetic_matrix_workbench_options_json(
    const GeneticMatrixWorkbenchOptions& options);
// Complete native net storage, including inactive boundary values and optional
// transition-name state. Previously lossless valid rich getter JSON is unchanged.
// Otherwise the full nine-field PetriNetLiteralStorage carrier is authoritative;
// it preserves arbitrary byte labels and public literal state without validation.
[[nodiscard]] std::string serialize_genetic_matrix_workbench_net_json(const PetriNet& net);
[[nodiscard]] std::string serialize_genetic_matrix_workbench_markings_json(
    const WorkflowMarkings& markings);
[[nodiscard]] std::string serialize_genetic_matrix_workbench_result_json(
    const GeneticMatrixConversionResult& result);

}  // namespace minera
