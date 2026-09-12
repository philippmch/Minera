#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "minera/conformance.hpp"
#include "minera/event_log.hpp"
#include "minera/petri_net.hpp"
#include "minera/stochastic_map.hpp"

namespace minera {

// Duration families use a common, compact parameter record.  The first value
// is the fixed value/mean for deterministic and exponential durations, the
// mean for normal durations, the lower bound for uniform durations, and the
// shape for lognormal/gamma durations.  Secondary and tertiary values carry
// the remaining parameters described on MonteCarloDuration below.
enum class MonteCarloDurationDistribution {
  kDeterministic,
  kExponential,
  kConstant0,
  kImmediate,
  kNormal,
  kUniform,
  kLogNormal,
  kGamma,

  // Spelling aliases retained for callers that use the conventional camel
  // case form of the lognormal family name.
  kLognormal = kLogNormal,
  kConstantZero = kConstant0,

  // Readable aliases for callers that mirror wire-level names.
  DETERMINISTIC = kDeterministic,
  EXPONENTIAL = kExponential,
  CONSTANT0 = kConstant0,
  IMMEDIATE = kImmediate,
  NORMAL = kNormal,
  UNIFORM = kUniform,
  LOGNORMAL = kLogNormal,
  GAMMA = kGamma,
};

struct MonteCarloDuration {
  MonteCarloDurationDistribution distribution =
      MonteCarloDurationDistribution::kDeterministic;
  // Seconds for a deterministic value, mean for an exponential value, mean
  // for a normal value, lower bound for a uniform value, or shape for a
  // lognormal/gamma value. Constant-zero and immediate durations ignore it.
  double value_seconds = 1.0;
  // Normal: standard deviation (must be non-negative).
  // Uniform: interval width/scale (must be non-negative).
  // Lognormal/gamma: location (must be non-negative).
  double secondary_seconds = 0.0;
  // Lognormal/gamma: positive scale. Unused by the other distributions.
  double tertiary_seconds = 0.0;
};

struct MonteCarloOptions {
  // Number of independent cases to schedule.  A zero count returns an empty
  // log and diagnostics without performing any simulation.
  std::size_t simulations = 100;
  // Every case is inserted into the virtual scheduler at this interval. The
  // two names are wire-compatible aliases; at most one may be supplied.
  // A missing value is inferred from a source log, or is 60 seconds for a
  // net-only simulation.
  std::optional<double> case_arrival_ratio;
  std::optional<double> case_arrival_seconds;
  // Attribute selectors used when fitting from a source log and when adding
  // aliases to generated events. Canonical names use the first-class fields.
  std::string activity_key = "concept:name";
  std::string timestamp_key = "time:timestamp";
  // Empty markings resolve to one token on the net source/sink. Non-empty
  // markings must have one entry per place.
  std::vector<std::uint32_t> initial_marking;
  std::vector<std::uint32_t> final_marking;
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
  // Stop a case when its marking covers every token required by final_marking.
  // When false, the case continues until max_steps or a deadlock.
  bool stop_at_final = true;
  // Optional distribution used only when fitting missing durations from a
  // source event log. Explicit transition_durations take precedence.
  std::optional<MonteCarloDurationDistribution> force_distribution;
  // Optional sparse transition map. Missing records use weight 1 and an
  // immediate duration; explicit vectors take precedence over map records.
  std::optional<StochasticMap> stochastic_map;
  // The replay implementation used to derive fitted activation weights.
  TokenReplayVariant token_replay_variant = TokenReplayVariant::kForward;
  // Compatibility controls retained for callers of the timed simulator.
  // They are validated but intentionally inert in this bounded implementation.
  std::size_t diagnostic_interval = 32;
  double small_scale_factor = 864000.0;
  double max_thread_execution_time = 60.0;
  // Model IDs and zero-token marking membership used only by automatic
  // activation-weight replay. Empty values use the replay defaults. These
  // fields do not affect simulation or explicit weight/map inputs.
  std::vector<std::string> place_names;
  std::vector<std::string> transition_names;
  std::vector<bool> initial_marking_keys;
  std::vector<bool> final_marking_keys;
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

  // Names used by the simulation-result contract. The *_seconds fields above
  // remain the compatibility spelling used by existing callers.
  double case_arrival_ratio = 0.0;
  double total_cases_time = 0.0;
  double median_cases_ex_time = 0.0;
  std::vector<double> cases_ex_time;
};

struct MonteCarloInterval {
  double start = 0.0;
  double end = 0.0;
  friend bool operator==(const MonteCarloInterval&, const MonteCarloInterval&) =
      default;
};

struct MonteCarloPlaceIntervalTree {
  std::size_t place_index = 0;
  std::vector<MonteCarloInterval> intervals;
  friend bool operator==(const MonteCarloPlaceIntervalTree&,
                         const MonteCarloPlaceIntervalTree&) = default;
};

struct MonteCarloTransitionIntervalTree {
  std::size_t transition_index = 0;
  std::vector<MonteCarloInterval> intervals;
  friend bool operator==(const MonteCarloTransitionIntervalTree&,
                         const MonteCarloTransitionIntervalTree&) = default;
};

struct MonteCarloResult {
  EventLog log;
  MonteCarloDiagnostics diagnostics;
  // One deterministic interval tree is returned for every net place and
  // transition, including trees with no intervals. Records are ordered by
  // place/transition index and then by interval start/end.
  std::vector<MonteCarloPlaceIntervalTree> output_places_interval_trees;
  std::vector<MonteCarloTransitionIntervalTree>
      output_transitions_interval_trees;
};

// Fits optional transition weights and service durations from a source log.
// Visible transitions use activity frequencies; when timestamps are available,
// the elapsed time to the next event estimates the current activity's service
// interval. Explicit options always take precedence.
[[nodiscard]] MonteCarloOptions fit_monte_carlo_options(
    const EventLog& input_log, const PetriNet& net,
    const MonteCarloOptions& options = {});

// Runs a bounded, seeded virtual-time simulation of a workflow net.  Cases
// share output-place resource pools, but each case owns its marking.  The
// overload taking an input log fits missing transition weights and durations
// before running the same scheduler.
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
