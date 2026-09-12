#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "minera/conformance.hpp"
#include "minera/dfg_conversion.hpp"
#include "minera/dfg_variants.hpp"
#include "minera/directly_follows_graph.hpp"
#include "minera/event_log.hpp"
#include "minera/petri_net.hpp"
#include "minera/stochastic_petri.hpp"

namespace minera {

// A transition's timing is kept separate from its distribution.  This lets a
// caller retain an immediate transition's distribution metadata while still
// making its firing semantics explicit.
enum class StochasticMapTiming {
  kTimed,
  kImmediate,

  TIMED = kTimed,
  IMMEDIATE = kImmediate,
};

// The public default_parameter fields are non-negative mean/duration inputs.
// Records store distribution-native parameters: a mean for deterministic and
// normal, rate for exponential, lower/upper bounds for uniform, location and
// log-scale for lognormal, and scale/shape for gamma.
enum class StochasticMapDistribution {
  kDeterministic,
  kExponential,
  kConstant0,
  kImmediate,
  kNormal,
  kUniform,
  kLogNormal,
  kGamma,

  kLognormal = kLogNormal,
  kConstantZero = kConstant0,

  DETERMINISTIC = kDeterministic,
  EXPONENTIAL = kExponential,
  CONSTANT0 = kConstant0,
  IMMEDIATE = kImmediate,
  NORMAL = kNormal,
  UNIFORM = kUniform,
  LOGNORMAL = kLogNormal,
  GAMMA = kGamma,
};

// Records retain original PetriNet transition IDs. Fitted maps can be sparse;
// array positions are not transition IDs.
struct StochasticMapTransition {
  std::size_t transition = 0;
  StochasticMapTiming timing = StochasticMapTiming::kTimed;
  StochasticMapDistribution distribution =
      StochasticMapDistribution::kExponential;
  double parameter = 1.0;
  double secondary_parameter = 0.0;
  double tertiary_parameter = 0.0;
  double activation_weight = 1.0;
  int priority = 0;
};

struct StochasticMap {
  std::vector<StochasticMapTransition> transitions;
};

struct StochasticMapFitOptions {
  StochasticMapDistribution distribution =
      StochasticMapDistribution::kExponential;
  double default_parameter = 1.0;
  bool silent_transitions_immediate = true;
  bool zero_duration_immediate = true;
  bool fit_activation_weights = true;
  bool fit_timing = true;
  int default_priority = 0;
  TokenReplayOptions replay;
};

struct StochasticMapTransitionStatistics {
  std::size_t transition = 0;
  std::size_t activations = 0;
  std::size_t enablings = 0;
  std::vector<double> performance;
  std::vector<std::size_t> log_indices;
};

// All traces contribute their replay paths, including paths from unfit traces.
// skipped_traces is retained for compatibility and remains zero.
struct StochasticMapFitResult {
  StochasticMap map;
  std::size_t replayed_traces = 0;
  std::size_t skipped_traces = 0;
  std::vector<StochasticMapTransitionStatistics> statistics;
};

struct StochasticMapPerformanceOptions {
  DfgPetriConversionOptions conversion;
  StochasticMapDistribution distribution =
      StochasticMapDistribution::kExponential;
  double default_parameter = 1.0;
  double default_activation_weight = 1.0;
  bool zero_duration_immediate = true;
  bool frequency_activation_weights = true;
  int default_priority = 0;
};

struct StochasticMapPetriModel {
  PetriNet net;
  StochasticMap map;
};

[[nodiscard]] const char* stochastic_map_timing_name(
    StochasticMapTiming timing);
[[nodiscard]] const char* stochastic_map_distribution_name(
    StochasticMapDistribution distribution);

// Validates a complete transition-indexed map for consumers requiring all IDs.
void validate_stochastic_map(const StochasticMap& map,
                             std::size_t expected_transition_count);

// Fitted maps may omit transitions that were never enabled or activated.
void validate_sparse_stochastic_map(const StochasticMap& map,
                                    std::size_t expected_transition_count);

[[nodiscard]] StochasticMapFitResult fit_stochastic_map_with_report(
    const EventLog& log, const PetriNet& net,
    const StochasticMapFitOptions& options = {});

[[nodiscard]] inline StochasticMap fit_stochastic_map(
    const EventLog& log, const PetriNet& net,
    const StochasticMapFitOptions& options = {}) {
  return fit_stochastic_map_with_report(log, net, options).map;
}

// Derives a workflow net using the selected DFG conversion and assigns each
// transition a distribution/rate from the performance edge summaries. The
// generated map is transition-index aligned with the returned net.
[[nodiscard]] StochasticMapPetriModel stochastic_map_from_performance_dfg(
    const DirectlyFollowsGraph& graph, const DfgPerformance& performance,
    const StochasticMapPerformanceOptions& options = {});

[[nodiscard]] StochasticMapPetriModel stochastic_map_from_performance_dfg(
    const DfgCleanAnalysis& graph, const DfgPerformance& performance,
    const StochasticMapPerformanceOptions& options = {});

// Converts an aligned map to the exponential/immediate options consumed by
// finite-state reachability and CTMC analysis. Non-exponential timed records
// use the reciprocal of their positive mean-equivalent parameter.
[[nodiscard]] StochasticPetriOptions stochastic_map_to_options(
    const StochasticMap& map,
    std::vector<std::uint32_t> initial_marking = {},
    std::size_t max_states = kStochasticPetriMaxGeneratorStates);

// Materializes defaults internally for missing IDs, preserving the sparse map.
[[nodiscard]] StochasticPetriOptions stochastic_map_to_options(
    const PetriNet& net, const StochasticMap& map,
    const StochasticMapFitOptions& fit_options = {},
    std::vector<std::uint32_t> initial_marking = {},
    std::size_t max_states = kStochasticPetriMaxGeneratorStates);

[[nodiscard]] std::string serialize_stochastic_map_json(
    const StochasticMap& map);

[[nodiscard]] inline std::string stochastic_map_json(
    const StochasticMap& map) {
  return serialize_stochastic_map_json(map);
}

}  // namespace minera
