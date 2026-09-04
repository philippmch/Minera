#pragma once

#include "minera/directly_follows_graph.hpp"
#include "minera/event_log.hpp"
#include "minera/petri_net.hpp"

namespace minera {

struct AlphaMinerOptions {
  // When set, runs the Alpha+ variant, which repairs the two
  // short-loop blind spots of the classic algorithm: activities that directly
  // repeat (length-one loops) are mined out of the log and re-attached as self
  // loops on the place between their neighbours, and a length-two loop
  // (a b a b …) is recognised as causality in both directions instead of being
  // mistaken for concurrency. On a log without short loops the result is
  // identical to the classic miner. Clear it for the textbook classic Alpha.
  // The omitted/default variant is the classic Alpha miner; callers opt into
  // Alpha+ explicitly by setting this to true.
  bool short_loops = false;

  // Event field used as the activity label.  The default preserves the
  // conventional Event::activity projection.  Non-default selectors are
  // resolved strictly: every event in a non-empty trace must contain a
  // non-null value for the selected field.
  std::string activity_attribute = "concept:name";

  // Alpha+ keeps transitions that the footprint cannot connect by default,
  // matching the historical native surface.  Set this only when the caller
  // explicitly wants disconnected visible transitions removed.
  bool remove_unconnected = false;
};

// Return a copy of the event log with the selected activity field projected
// into Event::activity.  All other event, trace, and log data is preserved in
// input order.  Missing or explicitly null selected fields are rejected.
// An empty selector is treated as the default "concept:name" field.
[[nodiscard]] EventLog normalize_alpha_event_log(
    const EventLog& event_log, const AlphaMinerOptions& options = {});

// Discovers a Petri net from an event log using the Alpha Miner. The algorithm
// reads the log's footprint — direct succession, causality, parallel and
// choice relations between activities — then finds the maximal pairs of
// activity sets (A, B) where every a in A causes every b in B and each side is
// internally unrelated. Each such pair becomes a place wired from the A
// transitions to the B transitions; a source place feeds the start activities
// and a sink place drains the end activities.
//
// By default the classic Alpha variant runs; Alpha+ is selected through
// AlphaMinerOptions::short_loops.
// Like the textbook algorithm it offers no soundness guarantee, but it
// reconstructs the control flow of well-structured logs directly as a Petri
// net.
[[nodiscard]] PetriNet discover_petri_net(const EventLog& event_log,
                                          const AlphaMinerOptions& options = {});

// Direct graph input is the classic Alpha footprint: graph edges and start/end
// counts contain no trace context for the short-loop repair, so Alpha+ is not
// a valid request for this overload.
[[nodiscard]] PetriNet discover_petri_net(
    const DirectlyFollowsGraph& graph,
    const AlphaMinerOptions& options = {});

}  // namespace minera
