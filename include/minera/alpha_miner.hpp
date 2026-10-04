#pragma once

#include "minera/directly_follows_graph.hpp"
#include "minera/dfg_variants.hpp"
#include "minera/event_log.hpp"
#include "minera/event_data.hpp"
#include "minera/petri_net.hpp"

namespace minera {

struct AlphaMinerOptions {
  // When set, runs the Alpha+ variant. Activities that directly repeat
  // (length-one loops) are removed before finding the maximal places and
  // receive separate self-loop places for their qualifying contexts. These
  // places are not connected to the surrounding visible transitions. A length-two loop
  // (a b a b …) is recognised as causality in both directions instead of being
  // mistaken for concurrency. Alpha+ also processes trace boundaries, even
  // when there are no short loops. Clear it for the textbook classic Alpha.
  // The omitted/default variant is the classic Alpha miner; callers opt into
  // Alpha+ explicitly by setting this to true.
  bool short_loops = false;

  // Event field used as the activity label.  The default preserves the
  // conventional Event::activity projection. Classic grouped-log discovery
  // preserves explicit null values as unlabeled transitions and accepts an
  // isolated missing event. Missing selected fields in adjacent pairs fail.
  // Alpha+ also preserves null values, but requires every selected field.
  // String normalization requires non-null selected values.
  std::string activity_attribute = "concept:name";

  // Alpha+ keeps transitions that the footprint cannot connect by default,
  // matching the historical native surface.  Set this only when the caller
  // explicitly wants disconnected visible transitions removed.
  bool remove_unconnected = false;

  // Flat input grouping and classic-table ordering selectors. Grouped logs,
  // streams, and Alpha+ tables retain their per-case event order. Classic
  // tables use chronological edges but original nonnull per-case endpoints.
  std::string case_id_attribute = "case:concept:name";
  std::string timestamp_attribute = "time:timestamp";
  std::optional<std::size_t> max_rows_to_materialize;
  // Empty selects an existing start_timestamp column, or completion when it
  // is absent. Null values in an existing start column sort last.
  // Classic table discovery sorts footprint rows by case/start/completion.
  // Empty start selector uses start_timestamp when present, else completion.
  // Its start/end activities still follow the original per-case row order.
  // These ordering settings do not reorder streams, grouped logs, or Alpha+.
  std::string start_timestamp_attribute;
  bool sort_caseid_required = true;
  bool sort_timestamp_along_case_id = true;
};

// Return a copy of the event log with the selected activity field projected
// into Event::activity.  All other event, trace, and log data is preserved in
// input order.  Missing or explicitly null selected fields are rejected.
// An empty selector is treated as the default "concept:name" field.
[[nodiscard]] EventLog normalize_alpha_event_log(
    const EventLog& event_log, const AlphaMinerOptions& options = {});

// Flat normalization groups cases in first-seen order and preserves their
// source event order. It is a data projection, not the complete classic table
// discovery path: table discovery computes its footprint order separately.
[[nodiscard]] EventLog normalize_alpha_event_log(
    const EventDataView& data, const AlphaMinerOptions& options = {});
[[nodiscard]] EventLog normalize_alpha_event_log(
    const EventStream& data, const AlphaMinerOptions& options = {});
[[nodiscard]] EventLog normalize_alpha_event_log(
    const EventTable& data, const AlphaMinerOptions& options = {});
[[nodiscard]] EventLog normalize_alpha_event_log(
    const LazyEventTable& data, const AlphaMinerOptions& options = {});

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

// Named variant entry points select their algorithm regardless of short_loops.
[[nodiscard]] PetriNet discover_alpha_classic(
    const EventLog& event_log, const AlphaMinerOptions& options = {});
[[nodiscard]] PetriNet discover_alpha_plus(
    const EventLog& event_log, const AlphaMinerOptions& options = {});

// Names are structural: distinct places may carry the same maximal-pair name.
struct AlphaPlaceName {
  std::string boundary;
  std::vector<std::optional<std::string>> before;
  std::vector<std::optional<std::string>> after;
  bool operator==(const AlphaPlaceName&) const = default;
};
struct AlphaDetachedMarking {
  AlphaPlaceName name;
  std::uint64_t tokens = 1;
};
struct AlphaPlusDiscoveryResult {
  PetriNet net;
  std::vector<AlphaPlaceName> place_names;
  std::vector<std::uint64_t> initial_marking;
  std::vector<std::uint64_t> final_marking;
  std::vector<AlphaDetachedMarking> detached_initial_marking;
  std::vector<AlphaDetachedMarking> detached_final_marking;
};
// Preserves markings on places removed by boundary cleanup. Marking vectors
// index live net places; detached entries never identify live same-name places.
// A detached initial marking leaves net.has_source() false.
// Both this result's net and discover_alpha_plus retain the exact sparse
// root/detached marking identities in PetriNet::discovery_markings().
[[nodiscard]] AlphaPlusDiscoveryResult discover_alpha_plus_result(
    const EventLog& event_log, const AlphaMinerOptions& options = {});

[[nodiscard]] PetriNet discover_petri_net(
    const EventDataView& data, const AlphaMinerOptions& options = {});
[[nodiscard]] PetriNet discover_petri_net(
    const EventStream& data, const AlphaMinerOptions& options = {});
[[nodiscard]] PetriNet discover_petri_net(
    const EventTable& data, const AlphaMinerOptions& options = {});
[[nodiscard]] PetriNet discover_petri_net(
    const LazyEventTable& data, const AlphaMinerOptions& options = {});

// Direct graph input is the classic Alpha footprint: graph edges and start/end
// counts contain no trace context for the short-loop repair, so Alpha+ is not
// a valid request for this overload.
[[nodiscard]] PetriNet discover_petri_net(
    const DirectlyFollowsGraph& graph,
    const AlphaMinerOptions& options = {});

// Sparse graph entry points infer boundary activities when none are supplied.
// Explicit boundary maps preserve isolated singleton activities as well;
// empty maps suppress inference. Zero-valued edge/endpoint keys remain part
// of the declared topology, while only positive edge values cause relations.
[[nodiscard]] PetriNet discover_petri_net(
    const DfgFrequency& graph, const AlphaMinerOptions& options = {});
[[nodiscard]] PetriNet discover_petri_net(
    const DfgFrequency& graph,
    const std::map<std::string, std::uint64_t>& start_activities,
    const std::map<std::string, std::uint64_t>& end_activities,
    const AlphaMinerOptions& options = {});
[[nodiscard]] PetriNet discover_petri_net(
    const DfgCleanAnalysis& graph, const AlphaMinerOptions& options = {});

}  // namespace minera
