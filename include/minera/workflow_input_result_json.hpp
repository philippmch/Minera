#pragma once

#include <string>

#include "minera/heuristic_miner.hpp"
#include "minera/petri_net.hpp"

namespace minera {

// Complete native state, independent of renderer activity registries and
// workflow assumptions. Sparse signed markings and arc magnitudes remain exact
// JSON integers. Empty native weight vectors retain implicit unit-arc storage.
[[nodiscard]] std::string workflow_input_petri_json(const PetriNet& net);

// Preserve original ids, literal selectors, all optional carriers and their
// native variant types. Finite doubles use max_digits10 and an explicit decimal
// point when integral; nonfinite doubles use "NaN", "INF" and "-INF" tags.
[[nodiscard]] std::string workflow_input_heuristics_json(const HeuristicsNet& net);

}  // namespace minera
