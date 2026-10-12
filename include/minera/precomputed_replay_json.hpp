#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "minera/evaluation.hpp"

namespace minera {

// Numeric JSON tokens and decimal strings retain exact signed token counts.
// Additional row metadata remains in the original input of the result envelope.
[[nodiscard]] std::vector<PrecomputedTokenReplay>
parse_precomputed_token_replay_json(std::string_view json);
[[nodiscard]] std::vector<PrecomputedAlignmentReplay>
parse_precomputed_alignment_replay_json(std::string_view json);

// Defaults are those of the corresponding native precomputed evaluator.
// Explicit incompatible variants retain the evaluator's uncomputed result.
[[nodiscard]] std::string precomputed_replay_fitness_json(
    std::string_view json, std::string_view input_kind,
    std::optional<ReplayFitnessVariant> variant = std::nullopt);

}  // namespace minera
