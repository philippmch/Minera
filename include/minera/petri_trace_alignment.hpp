#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "minera/conformance.hpp"
#include "minera/event_log.hpp"
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
  // When !computed, as AlignmentResult has them: true when max_states or
  // max_time_seconds stopped the search (generator variants then return this
  // one entry instead of an empty list), false when the final marking is
  // unreachable; and, when exhausted, whether max_time_seconds had run out.
  bool exhausted = false;
  bool time_limit_reached = false;
  std::vector<PetriTraceAlignmentMove> alignment;
  // Variant cost units: STATE_EQUATION_A_STAR, DIJKSTRA_NO_HEURISTICS and the
  // generators charge 10000 per log or visible model move and 1 per silent
  // model move; DIJKSTRA_LESS_MEMORY charges a silent move 1 only when a
  // visible transition whose input places are all inputs of the silent one has
  // a label occurring in the trace, else 0; DISCOUNTED_A_STAR charges 2^-depth
  // per non-sync move. All variants except DIJKSTRA_LESS_MEMORY and its
  // generator read every ordinary arc as weight 1, as the reference does. On
  // parallel arcs, state-equation A* fires a transition once each input place
  // holds one token per arc from it and takes one; the Dijkstra searches
  // (no-heuristics, its generator, discounted A*) fire it on one token; the
  // less-memory searches read parallel unit arcs as one arc. The others put
  // one token per output arc object.
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
  // Bounds as AlignmentOptions has them: seconds per search (zero means
  // unlimited) and a callback the server sets to notice a client that went
  // away (unset never cancels). A search past its time stops as one past
  // max_states does, marked exhausted and time_limit_reached; a cancelled one
  // throws OperationCancelled. The log dispatcher's easy-soundness check
  // refuses the net on time and throws OperationCancelled when cancelled.
  double max_time_seconds = 0.0;
  std::function<bool()> cancelled;
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

// Log-level dispatcher (the public algorithm.apply on an EventLog): one entry
// per trace in log order. Traces with equal activity sequences are aligned
// once and share the result. A net whose final marking is unreachable raises
// std::invalid_argument up front (the public easy-soundness check), although a
// single trace would get None there.
// trace.trace_costs is per event and cannot apply to a whole log, so it must be
// empty. A DataFrame aligns as the log of its cases in ascending case-id order.
struct PetriLogAlignmentOptions {
  PetriTraceAlignmentOptions trace;
  // "concept:name" (or "activity") reads Event::activity; other keys read the
  // named event attribute, and an event without it raises std::invalid_argument.
  std::string activity_key = "concept:name";
  // ENABLE_BEST_WORST_COST: false drops bwc and fitness from every entry.
  bool enable_best_worst_cost = true;
};

[[nodiscard]] std::vector<PetriTraceAlignment> align_petri_log(
    const EventLog& log, const PetriNet& net, const PetriLogAlignmentOptions& options = {});

// The public dispatcher's easy-soundness check (the one align_petri_log runs
// first): true when the final marking is reachable under the reference's
// rules for the net. Throws std::invalid_argument when a plain net's
// reachability search stops at its limit, and OperationCancelled when
// options.cancelled says so; a net with weighted or parallel arcs whose search
// passes max_states or max_time_seconds is refused (false).
[[nodiscard]] bool reference_easy_sound(const PetriNet& net,
                                        const PetriTraceAlignmentOptions& options);

// True when reference_log_alignment can report `options`: one of the three
// single-alignment variants with the default costs (either cost profile and
// no cost overrides).
[[nodiscard]] bool reference_log_alignment_applies(const AlignmentOptions& options);

// Alignments of a log as the reference's public dispatcher reports them. The
// bundle aligner (check_alignment) keeps the trace variants, precision and
// bounds; when reference_log_alignment_applies, each variant's alignment,
// cost and fitness then come from align_petri_log (with its easy-soundness
// check; a net it refuses gives an uncomputed result, exhausted when the check
// was undecided), and the log fitness is 1 - sum(cost) / sum(best-worst cost)
// over raw costs, or 1 when the summed best-worst cost is zero. Costs are
// reported in the options' profile: raw (10000 per move, 1 per silent move)
// for kStandard, whole moves for kLegacyUnit.
[[nodiscard]] AlignmentResult reference_log_alignment(const EventLog& log, const PetriNet& net,
                                                      const AlignmentOptions& options);

}  // namespace minera
