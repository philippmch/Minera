#pragma once

// pm4py's names of the places and transitions of the Petri net
// to_petri_net(HeuristicsNet) builds, index for index (round GD). pm4py's
// token replay breaks ties between silent routes by place name and picks a
// None label's silent transition by name, so a replay that should follow it
// on this net passes these names (TokenReplayOptions::place_names and
// nullable_transition_names).

#include <string>
#include <vector>

#include "minera/heuristic_miner.hpp"
#include "minera/petri_net.hpp"

namespace minera::detail {

struct HeuristicsPetriNames {
  std::vector<std::string> places;
  PetriTransitionNameList transitions;  // nullopt: Polars' None label
};

[[nodiscard]] HeuristicsPetriNames heuristics_petri_names(const HeuristicsNet& heuristics);

}  // namespace minera::detail
