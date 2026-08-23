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

// Converts a POWL model to a sound source/sink workflow net. POWL transition
// labels are registered deterministically in first-encounter order; an empty
// label is a visible activity and only PowlNode::silent() is tau.
[[nodiscard]] PetriNet to_petri_net(const PowlNode& node);

}  // namespace minera
