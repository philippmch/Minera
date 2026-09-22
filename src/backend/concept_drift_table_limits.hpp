#pragma once

#include <cstddef>
#include "minera/concept_drift.hpp"

namespace minera::detail {
// Internal materialization policy, injectable for boundary tests without
// allocating millions of input rows. Public entry points use the fixed cap.
ConceptDriftTableResult apply_concept_drift_table_bounded(
    const EventTable& table, ConceptDriftTableOptions options, std::mt19937& rng,
    std::size_t trace_limit);
}  // namespace minera::detail
