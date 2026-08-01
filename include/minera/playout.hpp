#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "minera/event_log.hpp"
#include "minera/petri_net.hpp"

namespace minera {

struct PlayoutOptions {
  // Number of cases to simulate.
  std::size_t traces = 100;
  // Firing cap per case, so a livelocking (unsound) net cannot spin forever.
  // A case that hits the cap or deadlocks is emitted as far as it got.
  std::size_t max_steps = 1000;
  // Seed for the firing choices; the same seed replays the same log.
  std::uint64_t seed = 1;
  // Timestamp of the first case's first event, seconds since the Unix epoch.
  // Defaults to 2026-01-01T00:00:00Z.
  double start = 1767225600.0;
  // Seconds between consecutive case starts and between a case's events, so
  // the generated log carries a plausible, strictly ordered time perspective.
  double case_interval = 60.0;
  double event_interval = 1.0;
};

// Controls weighted stochastic playout of a Petri net.  Transition weights
// are indexed by the transition's position in PetriNet::transitions(), rather
// than by activity label (two transitions may legitimately share a label).
// An empty vector means that every transition has weight 1.0.  When a vector
// is supplied it must contain one entry per transition.
struct StochasticPlayoutOptions {
  // Number of cases to simulate.
  std::size_t traces = 1000;
  // Firing cap per case.  Silent firings count towards this cap, preventing
  // an unsound net from livelocking without adding visible events.
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
  // Stop a case as soon as the exact workflow final marking is reached.
  bool stop_at_final = true;
};

// Plays out a workflow net into a synthetic event log: each case starts from
// one token on the source place and fires uniformly random enabled
// transitions until the final marking (a single token on the sink) is
// reached. Visible transitions append an event; silent ones only route. On a
// sound net every generated trace fits the model perfectly, which makes the
// playout a self-check for discovered models and a test-log generator for
// imported ones. Case ids are "case_1".. in generation order.
[[nodiscard]] EventLog playout(const PetriNet& net,
                               const PlayoutOptions& options = {});

// Samples enabled Petri-net transitions according to transition_weights and
// returns one synthetic trace per requested case.  Input arc multiplicity is
// honoured when determining enablement (and when firing), silent transitions
// route tokens without appending events, and max_steps bounds every firing.
[[nodiscard]] EventLog stochastic_playout(
    const PetriNet& net, const StochasticPlayoutOptions& options = {});

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

}  // namespace minera
