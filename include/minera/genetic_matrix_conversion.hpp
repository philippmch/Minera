#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "minera/petri_analysis.hpp"
#include "minera/petri_net.hpp"

namespace minera {

using GeneticMatrixPartition = std::vector<std::size_t>;
using GeneticMatrixMap =
    std::vector<std::vector<GeneticMatrixPartition>>;

// A causal matrix associates every visible transition with input and output
// groups. Members of one group are alternatives sharing a place; distinct
// groups require independent tokens and therefore express concurrency.
// Activity indexes refer to the stable activities vector.
struct GeneticCausalMatrix {
  std::vector<std::string> activities;
  GeneticMatrixMap input_map;
  GeneticMatrixMap output_map;
};

enum class GeneticMatrixConversionVariant {
  kToPetriNet,
  TO_PETRI_NET = kToPetriNet,
};

struct GeneticMatrixConversionResult {
  GeneticMatrixConversionVariant variant =
      GeneticMatrixConversionVariant::kToPetriNet;
  GeneticCausalMatrix matrix;
  PetriNet net;
  WorkflowMarkings markings;
};

[[nodiscard]] GeneticMatrixConversionVariant
parse_genetic_matrix_conversion_variant(std::string_view value);

[[nodiscard]] std::string_view genetic_matrix_conversion_variant_name(
    GeneticMatrixConversionVariant variant);

[[nodiscard]] GeneticMatrixConversionResult convert_genetic_matrix(
    const GeneticCausalMatrix& matrix,
    GeneticMatrixConversionVariant variant =
        GeneticMatrixConversionVariant::kToPetriNet);

[[nodiscard]] inline GeneticMatrixConversionResult
convert_genetic_matrix_to_petri_net(const GeneticCausalMatrix& matrix) {
  return convert_genetic_matrix(
      matrix, GeneticMatrixConversionVariant::kToPetriNet);
}

[[nodiscard]] inline PetriNet to_petri_net(
    const GeneticCausalMatrix& matrix) {
  return convert_genetic_matrix_to_petri_net(matrix).net;
}

[[nodiscard]] std::string genetic_matrix_conversion_json(
    const GeneticMatrixConversionResult& result);

}  // namespace minera
