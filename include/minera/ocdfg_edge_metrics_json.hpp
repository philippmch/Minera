#pragma once

#include <string>

#include "minera/ocdfg_edge_metrics.hpp"

namespace minera {

// Associations retain their ordered list and duplicates. Aggregate arrays
// contain deterministically ordered mathematical-set members. Edge endpoints
// stay separate string fields, avoiding ambiguous concatenated label keys.
[[nodiscard]] std::string serialize_ocdfg_edge_metrics_json(
    const OcdfgEdgeAssociations&);

}  // namespace minera
