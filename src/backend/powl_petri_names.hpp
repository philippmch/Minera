#pragma once

// pm4py's names of the places and transitions of the Petri net
// to_petri_net(PowlNode) builds, index for index (round GE). pm4py's token
// replay breaks ties between silent routes by place name (and orders the
// silent transitions by name), so a replay that should follow it on this net
// passes these names (TokenReplayOptions::place_names and
// nullable_transition_names).

#include <optional>
#include <string>
#include <vector>

#include "minera/petri_net.hpp"
#include "minera/powl.hpp"

namespace minera::detail {

struct PowlPetriNames {
  std::vector<std::string> places;
  PetriTransitionNameList transitions;
  // The reference iterates arc sets: in practice the order the arcs were
  // made, a folded arc's replacement taking its place. Per transition its
  // output places, per place its consumers (TokenReplayOptions::
  // place_outgoing_transition_order), in the net's ids.
  std::vector<std::vector<std::size_t>> transition_outputs;
  std::vector<std::vector<std::size_t>> place_outgoing;
};

// The names for `net`, which must be to_petri_net(node); nullopt when the
// named construction does not match the net's structure.
[[nodiscard]] std::optional<PowlPetriNames> powl_petri_names(const PowlNode& node, const PetriNet& net);

// `net` with each transition's outputs in the reference's order (the silent
// path search follows them), indices unchanged.
[[nodiscard]] PetriNet powl_replay_net(const PetriNet& net, const PowlPetriNames& names);

}  // namespace minera::detail
