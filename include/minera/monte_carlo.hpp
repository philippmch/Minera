#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "minera/event_log.hpp"
#include "minera/petri_net.hpp"

namespace minera {

// The duration model is intentionally small and deterministic: a fixed
// duration is useful for fixtures, while an exponential duration gives the
// usual memoryless service-time model with a seeded generator.
enum class MonteCarloDurationDistribution {
  kDeterministic,
  kExponential,
};

struct MonteCarloDuration {
  MonteCarloDurationDistribution distribution =
      MonteCarloDurationDistribution::kDeterministic;
  // Seconds for a deterministic value, or the mean (scale) for an
  // exponential value.  Zero is valid and produces an immediate firing.
  double value_seconds = 1.0;
};

struct MonteCarloOptions {
  // Number of independent cases to schedule.  A zero count returns an empty
  // log and diagnostics without performing any simulation.
  std::size_t simulations = 100;
  // Every case is inserted into the virtual scheduler at this interval.
  double case_arrival_seconds = 60.0;
  // A case cannot fire more than this many transitions, including silent
  // transitions.  This bounds unsound nets and zero-duration loops.
  std::size_t max_steps = 1000;
  std::uint64_t seed = 1;
  // Timestamp of the first case arrival, in Unix epoch seconds.
  double start = 1767225600.0;
  // A missing duration vector uses this deterministic service time.
  double default_duration_seconds = 1.0;
  // Transition weights are indexed by PetriNet::transitions().  An empty
  // vector means uniform choice among the enabled transitions.
  std::vector<double> transition_weights;
  // A missing duration entry uses default_duration_seconds.  When supplied,
  // this vector must contain one entry per transition.
  std::vector<MonteCarloDuration> transition_durations;
  // Output places reserve one virtual resource for the duration of a firing.
  // A missing entry uses default_resources_per_place.
  std::map<std::size_t, std::size_t> resources_per_place;
  std::size_t default_resources_per_place = 1;
  // Diagnostics are inexpensive aggregate values, but callers may disable
  // the per-case vector for very large simulations.
  bool enable_diagnostics = true;
  // Stop a case at the exact workflow final marking.  When false, the case
  // continues until max_steps or a deadlock.
  bool stop_at_final = true;
};

struct MonteCarloDiagnostics {
  std::size_t simulated_cases = 0;
  std::size_t completed_cases = 0;
  std::size_t deadlocked_cases = 0;
  std::size_t step_limited_cases = 0;
  double case_arrival_seconds = 0.0;
  double total_cases_seconds = 0.0;
  double median_case_execution_seconds = 0.0;
  std::vector<double> case_execution_seconds;
};

struct MonteCarloResult {
  EventLog log;
  MonteCarloDiagnostics diagnostics;
};

// Runs a bounded, seeded virtual-time simulation of a workflow net.  Cases
// share output-place resource pools, but each case owns its marking.  The
// overload taking an input log is source-compatible with callers that keep a
// source log beside the model; the current native contract uses explicit
// weights and durations from MonteCarloOptions.
[[nodiscard]] MonteCarloResult monte_carlo(
    const PetriNet& net, const MonteCarloOptions& options = {});

[[nodiscard]] MonteCarloResult monte_carlo(
    const EventLog& input_log, const PetriNet& net,
    const MonteCarloOptions& options = {});

// Serializes the bounded simulation result for JSON adapters. The generated
// payload contains the selected variant, log counts, and diagnostics; event
// details remain available through the EventLog returned by monte_carlo().
[[nodiscard]] std::string monte_carlo_json(const MonteCarloResult& result);

}  // namespace minera
