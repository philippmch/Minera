#pragma once

// The model decoration and the firings it replays, with each place's
// consumers in a given order (round GF): pm4py's silent path search on its
// POWL net follows the arcs in the order its converter made them, so the
// session's POWL net decorates with that order beside pm4py's names (which
// the net itself carries; ReferenceReplayNet in the server).

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "minera/event_log.hpp"
#include "minera/model_decoration.hpp"
#include "minera/petri_net.hpp"

namespace minera::detail {

using PlaceOutgoingOrder = std::optional<std::vector<std::vector<std::size_t>>>;

// token_replay_firing_sequences with TokenReplayOptions::
// place_outgoing_transition_order set to `place_outgoing`.
[[nodiscard]] std::vector<std::optional<std::vector<std::vector<std::size_t>>>>
token_replay_firing_sequences(const PetriNet& net,
                              const std::vector<std::vector<std::string>>& variants,
                              const std::function<bool()>& cancelled,
                              const PlaceOutgoingOrder& place_outgoing);

// decorate_model whose replay takes `place_outgoing` as above.
[[nodiscard]] ModelDecoration decorate_model(const EventLog& log, const PetriNet& net,
                                             std::string_view activity_attribute,
                                             std::string_view timestamp_attribute,
                                             const std::function<bool()>& cancelled,
                                             const PlaceOutgoingOrder& place_outgoing);

}  // namespace minera::detail
