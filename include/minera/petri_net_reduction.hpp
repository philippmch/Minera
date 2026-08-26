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

// Applies fixed-point single-entry/single-exit elimination to silent
// transitions connected only by ordinary arcs.  A single-entry transition is
// removed when its input place has one producer and no other consumer; its
// producer is rewired to the former outputs.  The dual single-exit rule
// rewires the former inputs to the unique consumer of the output place.
// Source/sink places and places carrying a nonzero supplied marking are
// protected.  Nets containing reset or inhibitor arcs are returned unchanged.
[[nodiscard]] PetriNetReductionResult reduce_petri_net(
    const PetriNet& net, const PetriNetReductionOptions& options = {});

// Convenience wrapper for callers that only need the reduced topology.
[[nodiscard]] inline PetriNet reduced_petri_net(
    const PetriNet& net, const PetriNetReductionOptions& options = {}) {
  return reduce_petri_net(net, options).net;
}

}  // namespace minera
