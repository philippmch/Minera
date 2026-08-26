#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include "minera/petri_analysis.hpp"

namespace minera {

// Options for the conservative silent-transition reduction.  An empty
// marking is unspecified; a supplied marking must contain one value per
// place and is carried through the place mapping.
struct PetriNetReductionOptions {
  Marking initial_marking;
  Marking final_marking;
};

struct PetriNetReductionResult {
  PetriNet net;
  WorkflowMarkings markings;
  // Every entry is indexed by a place in the input net and contains its
  // corresponding place in the reduced net.  A removed place contains
  // kRemovedPlace because a single replacement is not defined for a splice.
  std::vector<std::size_t> old_to_new_place;
  bool changed = false;
};

inline constexpr std::size_t kRemovedPlace =
    std::numeric_limits<std::size_t>::max();

// Applies a fixed-point conservative reduction to a workflow net.  The
// ordinary single-entry/single-exit and series-transition rules remove private
// places around silent transitions, while structural rules also fuse safe
// series places, discard duplicate silent transitions, remove inert silent
// self-loops, eliminate positively marked invariant places, and eliminate
// reset arcs guarded by the same inhibitor.  Extended arcs on unrelated
// places are retained and are never rewritten through a removed place; this
// keeps mixed ordinary/extended nets reducible without changing their token
// semantics.  Source/sink places and places carrying a nonzero supplied
// marking are protected.
[[nodiscard]] PetriNetReductionResult reduce_petri_net(
    const PetriNet& net, const PetriNetReductionOptions& options = {});

// Convenience wrapper for callers that only need the reduced topology.
[[nodiscard]] inline PetriNet reduced_petri_net(
    const PetriNet& net, const PetriNetReductionOptions& options = {}) {
  return reduce_petri_net(net, options).net;
}

}  // namespace minera
