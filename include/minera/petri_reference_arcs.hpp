#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

#include "minera/petri_net.hpp"

namespace minera {

// One ordinary arc as the reference sees it: a PetriNet.Arc object with its
// own weight. Native transitions store a side in one of two ways: a compact
// side (input_weights/output_weights nonempty) lists one occurrence per arc
// object, parallel arcs included; a repeated-unit side stores an arc of weight
// w as w repeated endpoints (parse_pnml_document materializes small positive
// weights that way when the transition has no parallel arcs). Folding each
// place's repeated endpoints back into one arc recovers the reference arcs.
struct PetriReferenceArc {
  std::size_t place = 0;
  TokenInteger weight = 1;
  bool negative = false;

  [[nodiscard]] std::int64_t signed_weight() const {
    const auto magnitude = static_cast<std::int64_t>(weight);
    return negative ? -magnitude : magnitude;
  }
};

[[nodiscard]] inline std::vector<PetriReferenceArc> petri_reference_arcs(
    const PetriTransition& transition, bool input) {
  const auto& places = input ? transition.inputs : transition.outputs;
  const auto& weights = input ? transition.input_weights : transition.output_weights;
  std::vector<PetriReferenceArc> arcs;
  if (!weights.empty()) {
    arcs.reserve(places.size());
    for (std::size_t index = 0; index < places.size(); ++index) {
      const auto weight = input ? transition.input_arc_weight(index)
                                : transition.output_arc_weight(index);
      arcs.push_back({places[index], weight.magnitude, weight.negative});
    }
    return arcs;
  }
  for (const auto place : places) {
    const auto found = std::find_if(arcs.begin(), arcs.end(),
                                    [&](const PetriReferenceArc& arc) { return arc.place == place; });
    if (found == arcs.end()) arcs.push_back({place, 1, false});
    else found->weight = found->weight + 1;
  }
  return arcs;
}

// True when some ordinary arc is not a single weight-1 arc object: a compact
// side (parallel, zero, signed or large weights) or a repeated-unit endpoint
// (a weight above 1).
[[nodiscard]] inline bool petri_net_has_weighted_arcs(const PetriNet& net) {
  for (const auto& transition : net.transitions()) {
    if (transition.has_compact_ordinary_arcs()) return true;
    for (const auto* places : {&transition.inputs, &transition.outputs})
      for (std::size_t k = 0; k < places->size(); ++k)
        if (std::find(places->begin(), places->begin() + static_cast<std::ptrdiff_t>(k),
                      (*places)[k]) != places->begin() + static_cast<std::ptrdiff_t>(k))
          return true;
  }
  return false;
}

// The reference incidence matrix (incidence_matrix.construct and every
// marking-equation/state-equation consumer) adds -1 per input arc object and
// +1 per output arc object, whatever the arc's weight: a weight-2 arc counts
// once and two parallel arcs count twice. Indexed [place][transition].
[[nodiscard]] inline std::vector<std::vector<double>> petri_reference_incidence(
    const PetriNet& net) {
  std::vector<std::vector<double>> matrix(
      net.place_count(), std::vector<double>(net.transitions().size(), 0.0));
  for (std::size_t t = 0; t < net.transitions().size(); ++t) {
    for (const auto& arc : petri_reference_arcs(net.transitions()[t], true))
      if (arc.place < matrix.size()) matrix[arc.place][t] -= 1.0;
    for (const auto& arc : petri_reference_arcs(net.transitions()[t], false))
      if (arc.place < matrix.size()) matrix[arc.place][t] += 1.0;
  }
  return matrix;
}

// The Woflan invariant stages (S-components, uniform and weighted place
// invariants) use a weighted incidence instead: each arc object adds its
// signed weight, so parallel arcs sum (two weight-1 arcs give 2, weights 2
// and -1 give 1). Indexed [place][transition].
[[nodiscard]] inline std::vector<std::vector<double>> petri_weighted_incidence(
    const PetriNet& net) {
  std::vector<std::vector<double>> matrix(
      net.place_count(), std::vector<double>(net.transitions().size(), 0.0));
  for (std::size_t t = 0; t < net.transitions().size(); ++t) {
    for (const auto& arc : petri_reference_arcs(net.transitions()[t], true))
      if (arc.place < matrix.size())
        matrix[arc.place][t] -= static_cast<double>(arc.signed_weight());
    for (const auto& arc : petri_reference_arcs(net.transitions()[t], false))
      if (arc.place < matrix.size())
        matrix[arc.place][t] += static_cast<double>(arc.signed_weight());
  }
  return matrix;
}

}  // namespace minera
