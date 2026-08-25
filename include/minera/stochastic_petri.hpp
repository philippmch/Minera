#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "minera/petri_net.hpp"

namespace minera {

// The public result contains a dense generator matrix. Keep its worst-case
// footprint bounded so a caller-controlled state cap cannot exhaust memory.
inline constexpr std::size_t kStochasticPetriMaxGeneratorStates = 2000;
inline constexpr std::size_t kStochasticPetriMaxGeneratorEntries =
    kStochasticPetriMaxGeneratorStates * kStochasticPetriMaxGeneratorStates;
inline constexpr double kStochasticPetriMaxTransientTimeSeconds = 1'000'000.0;

enum class StochasticPetriAnalysisVariant {
  kReachability,
  kTransient,
  kSteadyState,
};

// Reachability controls for a finite-state ordinary Petri-net CTMC view.
// Empty transition_rates assigns rate 1.0 to every transition.
struct StochasticPetriOptions {
  std::vector<double> transition_rates;
  std::vector<std::uint32_t> initial_marking;
  std::vector<std::size_t> immediate_transitions;
  // Optional activation weights for immediate transitions. Empty means equal
  // weight for every eligible transition.
  std::vector<double> immediate_weights;
  // Optional per-transition priority for immediate firings. When multiple
  // enabled immediate transitions are present, only those at the highest
  // priority are eligible. Empty uses priority zero for every transition.
  std::vector<int> transition_priorities;
  std::size_t max_states = kStochasticPetriMaxGeneratorStates;
  // Adapter-only analysis selector. Native reachability ignores this field;
  // adapters dispatch to the explicit probability APIs below.
  StochasticPetriAnalysisVariant analysis =
      StochasticPetriAnalysisVariant::kReachability;
  double analysis_time_seconds = 0.0;
  std::optional<std::size_t> analysis_initial_state;
  std::vector<double> analysis_initial_probabilities;
};

struct StochasticPetriState {
  std::vector<std::uint32_t> marking;
};

struct StochasticPetriEdge {
  std::size_t source = 0;
  std::size_t target = 0;
  std::size_t transition = 0;
  double rate = 0.0;
};

struct StochasticPetriResult {
  std::vector<StochasticPetriState> states;
  std::vector<StochasticPetriEdge> edges;
  std::vector<std::vector<double>> generator;
  // Index of the projected initial marking when it remains a tangible state.
  // A missing value means the initial marking was removed by projection.
  std::optional<std::size_t> initial_state;
  // Initial probability mass after immediate-state projection, aligned with
  // states. A single nonzero entry is also exposed through initial_state.
  std::vector<double> initial_probabilities;
  bool exhausted = false;
  std::string diagnostic;
};

struct StochasticPetriDistributionResult {
  StochasticPetriAnalysisVariant variant =
      StochasticPetriAnalysisVariant::kTransient;
  std::vector<double> probabilities;
  double time_seconds = 0.0;
  std::optional<std::size_t> initial_state;
  bool computed = false;
  bool unique = true;
  std::string diagnostic;
};

[[nodiscard]] StochasticPetriResult stochastic_petri_reachability(
    const PetriNet& net, const StochasticPetriOptions& options = {});

[[nodiscard]] StochasticPetriDistributionResult stochastic_petri_transient(
    const StochasticPetriResult& result, double time_seconds,
    std::size_t initial_state);

[[nodiscard]] StochasticPetriDistributionResult stochastic_petri_transient(
    const StochasticPetriResult& result, double time_seconds,
    const std::vector<double>& initial_probabilities);

[[nodiscard]] StochasticPetriDistributionResult stochastic_petri_steady_state(
    const StochasticPetriResult& result);

}  // namespace minera
