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

  // The broad structural rules are quadratic or worse on large nets. Keep
  // them opt-in for compatibility with the conservative fixed-point default;
  // callers that need the complete structural rule family can enable them.
  bool enable_parallel_place_fusion = false;
  bool enable_abstraction = false;
  // Keep a unary silent bridge when its input has multiple ordinary producers
  // and its output has an ordinary producer other than the bridge. Heuristic
  // conversion uses this to retain the separate branches feeding a join.
  bool preserve_join_bridges = false;
};

struct PetriNetReductionResult {
  PetriNet net;
  WorkflowMarkings markings;
  // Every entry is indexed by a place in the input net and contains its
  // corresponding place in the reduced net. A removed place contains
  // kRemovedPlace when a single replacement is not defined for a splice;
  // places fused by the parallel-place rule instead point at their surviving
  // representative.
  std::vector<std::size_t> old_to_new_place;
  bool changed = false;
  // Every entry is indexed by a transition in the input net. Surviving
  // transitions map to their reduced index; fused duplicate silent
  // transitions share their surviving representative's index. A removed
  // transition, including a subsequently removed representative, contains
  // kRemovedTransition.
  std::vector<std::size_t> old_to_new_transition;
};

inline constexpr std::size_t kRemovedPlace =
    std::numeric_limits<std::size_t>::max();
inline constexpr std::size_t kRemovedTransition =
    std::numeric_limits<std::size_t>::max();

// Applies a fixed-point conservative reduction to a workflow net.  The
// ordinary single-entry/single-exit and series-transition rules remove private
// places around silent transitions, while structural rules also fuse safe
// series places, discard duplicate silent transitions, remove inert silent
// self-loops, eliminate positively marked invariant places, and eliminate
// reset arcs guarded by the same inhibitor. Extended arcs on unrelated places
// are retained and are never rewritten through a removed place; this keeps
// mixed ordinary/extended nets reducible without changing their token
// semantics. Source/sink places and places carrying a nonzero supplied
// marking are protected by the ordinary rules; parallel-place fusion accepts
// equal supplied markings and carries their token sum through its alias map.
// Optional parallel-place fusion and abstraction passes cover the remaining
// structural variants.
[[nodiscard]] PetriNetReductionResult reduce_petri_net(
    const PetriNet& net, const PetriNetReductionOptions& options = {});

// Convenience wrapper for callers that only need the reduced topology.
[[nodiscard]] inline PetriNet reduced_petri_net(
    const PetriNet& net, const PetriNetReductionOptions& options = {}) {
  return reduce_petri_net(net, options).net;
}

}  // namespace minera
