#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "minera/petri_net.hpp"

namespace minera {

// The public result contains a dense generator matrix. Keep its worst-case
// footprint bounded so a caller-controlled state cap cannot exhaust memory.
inline constexpr std::size_t kStochasticPetriMaxGeneratorStates = 2000;
inline constexpr std::size_t kStochasticPetriMaxGeneratorEntries =
    kStochasticPetriMaxGeneratorStates * kStochasticPetriMaxGeneratorStates;

// Reachability controls for a finite-state ordinary Petri-net CTMC view.
// Empty transition_rates assigns rate 1.0 to every transition.
struct StochasticPetriOptions {
  std::vector<double> transition_rates;
  std::vector<std::uint32_t> initial_marking;
  std::vector<std::size_t> immediate_transitions;
  std::size_t max_states = kStochasticPetriMaxGeneratorStates;
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
  bool exhausted = false;
  std::string diagnostic;
};

[[nodiscard]] StochasticPetriResult stochastic_petri_reachability(
    const PetriNet& net, const StochasticPetriOptions& options = {});

}  // namespace minera
