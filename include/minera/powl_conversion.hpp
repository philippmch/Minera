#pragma once

#include "minera/petri_net.hpp"
#include "minera/powl.hpp"
#include "minera/process_tree.hpp"

namespace minera {

// Converts a block-structured process tree to the corresponding POWL model.
// Activity labels are resolved through ProcessTree::activities(); invalid
// activity references are rejected with std::out_of_range.
[[nodiscard]] PowlNode to_powl(const ProcessTree& tree);

// Converts a POWL model to a deterministic block-structured process tree.
// Connected partial-order components become parallel branches; each component
// is layered by a stable topological traversal. Sparse adjacent layers are
// represented by the closest block-structured approximation.
[[nodiscard]] ProcessTree to_process_tree(const PowlNode& node);

// Converts an ordinary source/sink workflow net to a block-structured process
// tree. The net is validated through the reverse workflow-net translator and
// residual partial orders are rejected when they cannot be reduced exactly to
// sequence, exclusive, parallel, or loop blocks. When the source and sink
// metadata are left at their default value, a unique structural source and
// sink are inferred without changing the caller's net.
[[nodiscard]] ProcessTree to_process_tree(const PetriNet& net);

// Converts a POWL model to a sound source/sink workflow net. POWL transition
// labels are registered deterministically in first-encounter order; an empty
// label is a visible activity and only PowlNode::silent() is tau.
[[nodiscard]] PetriNet to_petri_net(const PowlNode& node);

// Converts a structurally valid ordinary workflow net into a POWL model.
// The designated source/sink places (or a unique structural pair when the
// metadata is absent) must cover a reachable final marking; a silent return
// into the source is allowed for loop fragments. The translator preserves
// deterministic labels and recognizes exclusive, loop, sequence, and
// partial-order fragments. Extended arcs and nets whose structure cannot be
// proven to fit those fragments are rejected.
[[nodiscard]] PowlNode to_powl(const PetriNet& net);

}  // namespace minera
