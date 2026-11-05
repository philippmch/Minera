#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "minera/event_log.hpp"
#include "minera/petri_net.hpp"

namespace minera {

// algo.simulation.playout.petri_net BASIC_PLAYOUT of PM4Py 2.7.22
// (parameters noTraces, maxTraceLength, add_only_if_fm_is_reached,
// fm_leq_accepted). Invariant-only: the process and the envelope are the
// reference's, the draws are not (the reference chooses from a set of
// transition objects, ordered by object identity). See playout().
struct PlayoutOptions {
  // Number of cases to return (noTraces). The adapters default to the
  // reference's 1000; the struct keeps 100 for the hashed workbench captures.
  std::size_t traces = 100;
  // Visible events per case (maxTraceLength, reference default 1000): a case
  // stops once it holds this many, whether or not it reached the final
  // marking.
  std::size_t max_steps = 1000;
  // Seed of the rng::PythonRandom firing choices; the same seed replays the
  // same log.
  std::uint64_t seed = 1;
  // The default envelope is the reference's: cases "0", "1", ... and
  // timestamps from epoch 10,000,000, one second per event, counting on
  // across the returned cases. add_envelope (a declared extension; the
  // adapters set it when start, case_interval or event_interval is supplied)
  // instead names cases "case_1", ... and starts case i at
  // start + i * case_interval with event_interval between its events. (It is
  // declared last, with the safety bounds, to keep positional initialisers.)
  double start = 1767225600.0;
  double case_interval = 60.0;
  double event_interval = 1.0;
  // Optional explicit marker vectors. An empty vector keeps the workflow
  // source/sink defaults; a supplied vector must match the place count.
  std::vector<std::uint32_t> initial_marking;
  std::vector<std::uint32_t> final_marking;
  // Keep only cases that end in an accepted final marking, sampling on until
  // `traces` are accepted; the log is empty when none of the first `traces`
  // cases was accepted (as in the reference).
  bool add_only_if_fm_is_reached = false;
  // Accept (and offer the stop option in) any marking that covers the final
  // marking, not only the exact one.
  bool fm_leq_accepted = false;
  // Adapter-facing opt-in for the visited-elements representation returned by
  // playout_visited; the EventLog-returning playout function remains unchanged.
  bool return_visited_elements = false;
  // Safety bounds (the reference has none and would not return): a case that
  // fires this many transitions (silent ones included; 0 means
  // max(1,000,000, 1000 * (max_steps + 1))) raises, and so does an
  // add_only_if_fm_is_reached run that needs more than this many cases
  // (0 means 10000 * traces + 10000).
  std::size_t max_firings_per_trace = 0;
  std::size_t max_attempts = 0;
  bool add_envelope = false;
};

// Controls weighted stochastic playout of a Petri net.  Transition weights
// are indexed by the transition's position in PetriNet::transitions(), rather
// than by activity label (two transitions may legitimately share a label).
// An empty vector means that every transition has weight 1.0.  When a vector
// is supplied it must contain one entry per transition.
struct StochasticPlayoutOptions {
  // Number of cases to simulate.
  std::size_t traces = 1000;
  // Visible-event cap per case, as the reference's maxTraceLength: silent
  // firings do not count (a silent livelock raises after
  // max(1,000,000, 1000 * (max_steps + 1)) firings).
  std::size_t max_steps = 1000;
  // Seed for transition choices; equal options reproduce equal logs.
  std::uint64_t seed = 1;
  // Timestamp of the first case's first event, seconds since the Unix epoch.
  double start = 1767225600.0;
  // Seconds between case starts and between visible events in a case.
  double case_interval = 60.0;
  double event_interval = 1.0;
  // Per-transition non-negative weights.  Missing weights (an empty vector)
  // default to 1.0; all-zero weights among enabled transitions are sampled
  // uniformly.
  std::vector<double> transition_weights;
  // In the exact final marking "stop" is one more option of weight 1.0
  // beside the enabled transitions, as in the reference (a case there with
  // nothing enabled stops). False never stops a case at the final marking.
  bool stop_at_final = true;
  // Optional explicit marker vectors. An empty vector keeps the workflow
  // source/sink defaults; a supplied vector must match the place count.
  std::vector<std::uint32_t> initial_marking;
  std::vector<std::uint32_t> final_marking;
  // An adapter may request source-log fitting. The native source-log overload
  // below performs fitting directly; this flag keeps the request in one
  // option object while preserving the original aggregate fields.
  bool fit_from_log = false;
  // Retained for compatibility and still validated (it must be positive):
  // source-log fitting now uses the reference token replay of
  // fit_stochastic_map_with_report, which has its own bounds.
  std::size_t fitting_max_states = 200000;
  // Opt-in structured output for stochastic playout adapters. The ordinary
  // stochastic_playout API continues to return an EventLog. Appended to keep
  // existing positional aggregate initialization source-compatible.
  bool return_visited_elements = false;
};

// Plays out a Petri net into a synthetic event log, as the reference
// BASIC_PLAYOUT does: each case starts in the initial marking and fires
// uniformly random enabled transitions (transitions without input places are
// always enabled); in an accepted final marking "stop" is one more equally
// likely option, and a case also stops at max_steps visible events or at a
// deadlock. Visible transitions append an event; silent ones only route.
// Case ids and timestamps follow PlayoutOptions::add_envelope.
[[nodiscard]] EventLog playout(const PetriNet& net,
                               const PlayoutOptions& options = {});

enum class PlayoutVisitedElementKind { MARKING, TRANSITION };

struct PlayoutVisitedElement {
  PlayoutVisitedElementKind kind = PlayoutVisitedElementKind::MARKING;
  // Set for MARKING elements; transition_index is ignored for those values.
  std::vector<std::uint32_t> marking;
  // Set for TRANSITION elements; this is the stable index in the net's
  // transition vector and includes silent transitions.
  std::size_t transition_index = 0;

  friend bool operator==(const PlayoutVisitedElement&,
                         const PlayoutVisitedElement&) = default;
};

using PlayoutVisitedTrace = std::vector<PlayoutVisitedElement>;

struct PlayoutVisitedResult {
  std::vector<PlayoutVisitedTrace> traces;
  // Set when a bounded exhaustive search stopped before its queue was empty.
  // Sampled variants leave these diagnostics false because their requested
  // trace and firing limits define the result rather than truncating a search.
  bool truncated = false;
  bool trace_limit_reached = false;
  bool state_limit_reached = false;
};

// Returns the alternating marking/transition visitation sequence for each
// returned basic-playout case (the same cases, draw for draw, as playout). Every case starts with its initial marking;
// each fired transition is followed by the resulting marking, including
// silent transitions. Cases rejected by the acceptance options are omitted.
[[nodiscard]] PlayoutVisitedResult playout_visited(
    const PetriNet& net, const PlayoutOptions& options = {});

// Samples enabled Petri-net transitions according to transition_weights and
// returns one synthetic trace per requested case.  Input arc multiplicity is
// honoured when determining enablement (and when firing), silent transitions
// route tokens without appending events, and max_steps bounds the visible
// events of a case.
[[nodiscard]] EventLog stochastic_playout(
    const PetriNet& net, const StochasticPlayoutOptions& options = {});

// Returns alternating markings and fired transitions for stochastic playout.
// The initial marking is always included; each fired transition, including a
// silent transition, is followed by its resulting marking. The final marking
// reached after the last allowed firing is included, even when max_steps is
// the stopping condition.
[[nodiscard]] PlayoutVisitedResult stochastic_playout_visited(
    const PetriNet& net, const StochasticPlayoutOptions& options = {});
[[nodiscard]] PlayoutVisitedResult stochastic_playout_visited(
    const PetriNet& net, const EventLog& source_log,
    const StochasticPlayoutOptions& options = {});

// Fits one activation weight per transition as the reference stochastic
// playout does with a source log: the activation/enabling ratios of
// get_map_from_log_and_net (fit_stochastic_map_with_report, token replay on
// the playout markings). Enabled transitions never selected receive zero;
// transitions the map lacks keep the reference's weight one. Explicit
// transition weights always take precedence below.
[[nodiscard]] std::vector<double> fit_stochastic_transition_weights(
    const PetriNet& net, const EventLog& source_log,
    const StochasticPlayoutOptions& options);

[[nodiscard]] std::vector<double> fit_stochastic_transition_weights(
    const PetriNet& net, const EventLog& source_log);

// Plays out using weights fitted from source_log unless options already
// supplies an explicit transition_weights vector.
[[nodiscard]] EventLog stochastic_playout_fitted(
    const PetriNet& net, const EventLog& source_log,
    const StochasticPlayoutOptions& options = {});

// Source-log overload retained as a concise alternative spelling.
[[nodiscard]] EventLog stochastic_playout(
    const PetriNet& net, const EventLog& source_log,
    const StochasticPlayoutOptions& options = {});

struct ExtensivePlayoutOptions {
  // Longest visible trace to enumerate (events per case).
  std::size_t max_length = 10;
  // Cap on the distinct traces emitted, so a permissive model cannot explode
  // the log; the shortest traces win because enumeration is breadth-first.
  std::size_t max_traces = 1000;
  // Exploration budget (distinct search states), the guard against nets whose
  // silent structure can grow the marking without growing the trace.
  std::size_t max_states = 200000;
  // Synthetic timestamps, as in PlayoutOptions.
  double start = 1767225600.0;
  double case_interval = 60.0;
  double event_interval = 1.0;
  // Optional explicit marker vectors.  An empty vector keeps the workflow
  // source/sink defaults; a supplied vector must have one entry per place.
  // This makes bounded language enumeration useful for nets whose accepted
  // initial or final marking is not the designated workflow boundary.
  std::vector<std::uint32_t> initial_marking;
  std::vector<std::uint32_t> final_marking;
  // Optional per-state transition branching cap. Zero preserves exhaustive
  // breadth-first ordering; fractional values are stochastically rounded.
  // These fields are appended to preserve existing positional aggregate
  // initializers for the original option members.
  double branching_factor_transitions = 0.0;
  std::uint64_t seed = 1;
  // Opt-in structured visitation output. Appended to preserve positional
  // aggregate initializers for the original option members.
  bool return_elements = false;
  // Maximum occurrences of any marking along one candidate. Omitted means
  // unlimited; an explicit zero is invalid.
  std::optional<std::size_t> max_marking_occ;
};

// Enumerates the model's language instead of sampling it: every distinct
// visible trace of at most `max_length` events that walks the net from the
// initial to the final marking becomes one case, shortest traces first (the
// exploration is a breadth-first search over marking × trace states, so two
// silent routings of the same visible trace collapse into one case). The
// exhaustive counterpart of `playout` — a fitting log of everything the model
// allows, for inspecting its language or probing precision. Bounded by
// `max_traces` and `max_states`, so an infinite language yields its shortest
// members rather than spinning forever.
[[nodiscard]] EventLog extensive_playout(
    const PetriNet& net,
    const ExtensivePlayoutOptions& options = {});

// Enumerates accepted extensive candidates as alternating markings and fired
// transition indices, including silent transitions. The initial marking and
// the marking reached by the final firing are included.
[[nodiscard]] PlayoutVisitedResult extensive_playout_visited(
    const PetriNet& net,
    const ExtensivePlayoutOptions& options = {});

}  // namespace minera
