#pragma once

#include <string>

#include "minera/petri_analysis.hpp"

namespace minera {

[[nodiscard]] std::string petri_analysis_json(
    const MarkingEquationResult& result);
[[nodiscard]] std::string petri_analysis_json(
    const ExtendedMarkingEquationResult& result);
[[nodiscard]] std::string petri_analysis_json(
    const WorkflowNetResult& result);
[[nodiscard]] std::string petri_analysis_json(
    const SoundnessResult& result);

}  // namespace minera
