#pragma once

#include "minera/event_log.hpp"
#include "minera/petri_net.hpp"

namespace minera {

// Discovers a Petri net from an event log using the classic Alpha Miner. The
// algorithm reads the log's footprint — direct succession, causality, parallel
// and choice relations between activities — then finds the maximal pairs of
// activity sets (A, B) where every a in A causes every b in B and each side is
// internally unrelated. Each such pair becomes a place wired from the A
// transitions to the B transitions; a source place feeds the start activities
// and a sink place drains the end activities.
//
// Like the textbook algorithm it cannot represent short loops (self-loops and
// length-two loops) and offers no soundness guarantee, but it reconstructs the
// control flow of well-structured logs directly as a Petri net.
[[nodiscard]] PetriNet discover_petri_net(const EventLog& event_log);

}  // namespace minera
