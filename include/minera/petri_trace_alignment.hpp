#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "minera/conformance.hpp"
#include "minera/petri_net.hpp"

namespace minera {

// Reference-shaped single-trace Petri-net alignment, as returned per trace by
// the reference dispatcher (algo.conformance.alignments.petri_net). Each move
// pairs a log side and a model side; ">>" marks the absent side and a silent
// model transition has no model label.
struct PetriTraceAlignmentMove {
  std::string log;
  std::optional<std::string> model;
  std::optional<std::size_t> transition;  // model transition id, if any
  bool operator==(const PetriTraceAlignmentMove&) const = default;
};

struct PetriTraceAlignment {
  bool computed = false;  // false when the final marking is unreachable or a bound is hit
  std::vector<PetriTraceAlignmentMove> alignment;
  // Variant cost units: STATE_EQUATION_A_STAR, DIJKSTRA_NO_HEURISTICS and the
  // generators charge 10000 per log or visible model move and 1 per silent
  // model move; DIJKSTRA_LESS_MEMORY charges a silent move 1 only when a
  // visible transition whose input places are all inputs of the silent one has
  // a label occurring in the trace, else 0; DISCOUNTED_A_STAR charges 2^-depth
  // per non-sync move. All variants except DIJKSTRA_LESS_MEMORY and its
  // generator read every ordinary arc as weight 1, as the reference does.
  double cost = 0.0;
  // Best-worst cost and fitness are reported by the four enum variants only.
  // fitness = 1 - floor(cost / 10000) / floor(bwc / 10000), or 0 when the
  // integral denominator is zero. DISCOUNTED_A_STAR reports bwc 0.
  std::optional<double> bwc;
  std::optional<double> fitness;
  AlignmentDiagnostics diagnostics;
};

struct PetriTraceAlignmentOptions {
  AlignmentVariant variant = AlignmentVariant::kDijkstraLessMemory;
  // Empty vectors select the workflow defaults (source/sink token).
  std::vector<int> initial_marking;
  std::vector<int> final_marking;
  std::size_t max_states = 200000;
  // Generator variants: maximum number of equal-cost alignments returned.
  std::size_t max_alignments = 64;
  // Optional cost functions (the reference model/sync/trace cost function
  // parameters). Nonempty vectors hold one cost per transition or per trace
  // event and replace the variant's default costs. Sync costs are ignored by
  // DIJKSTRA_LESS_MEMORY, which has no sync cost function.
  std::vector<double> model_costs;
  std::vector<double> sync_costs;
  std::vector<double> trace_costs;
};

// Aligns one trace (activity labels) with the reference variant semantics.
[[nodiscard]] PetriTraceAlignment align_petri_trace(
    const std::vector<std::string>& trace, const PetriNet& net,
    const PetriTraceAlignmentOptions& options = {});

// Generator variants enumerate every optimal alignment (up to
// max_alignments). Enum variants return a single element.
[[nodiscard]] std::vector<PetriTraceAlignment> align_petri_trace_all(
    const std::vector<std::string>& trace, const PetriNet& net,
    const PetriTraceAlignmentOptions& options = {});

}  // namespace minera
