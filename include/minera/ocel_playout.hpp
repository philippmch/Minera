#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <string>
#include <vector>

#include "minera/ocel.hpp"

namespace minera {

enum class OcpnEventBinding {
  // Keep one emitted event for every object-local firing. This is the legacy
  // projection and remains the default for source compatibility.
  kIndependent,
  // Coalesce firings with the same activity and local firing index into one
  // event, retaining every participating object relation.
  kSharedActivity,
  // Fire one transition per bound object slot in one global event.  This is
  // the synchronized object-centric search; it uses the activity slot map on
  // OcpnGraph and keeps the exact object relations of every binding.
  kSynchronized,
  INDEPENDENT = kIndependent,
  SHARED_ACTIVITY = kSharedActivity,
  SYNCHRONIZED = kSynchronized,
};

[[nodiscard]] const char* ocpn_event_binding_name(OcpnEventBinding binding);

struct OcpnPlayoutOptions {
  // Maximum number of firings of any visible activity in one object trace.
  // The extensive search is bounded by this value and by max_states.
  std::size_t max_bindings_per_activity = 2;
  // Optional branching caps. Zero means unlimited; fractional caps are
  // stochastically rounded for each state or per-object trace choice. The
  // synchronized path applies both caps directly, while legacy independent
  // and shared modes use deterministic derived seeds for their expansions.
  double branching_factor_transitions = 0.0;
  double branching_factor_bindings = 0.0;
  // Causal-net-only cap on enabled activity branches. Zero is unlimited;
  // ordinary object-centric Petri-net playout ignores this field.
  double branching_factor_activities = 0.0;
  std::size_t max_traces = 100;
  std::size_t max_states = 200000;
  // Optional object ids per object type. Missing types receive one synthetic
  // id named <type>_1.
  std::map<std::string, std::vector<std::string>> objects;
  // Optional explicit per-type markers. Each vector is applied to every
  // selected object of that type and must match that net's place count.
  // Omitted types use the net's designated source/sink workflow markers.
  std::map<std::string, std::vector<std::uint32_t>> initial_markings;
  std::map<std::string, std::vector<std::uint32_t>> final_markings;
  bool objects_unique_per_trace = false;
  OcpnEventBinding event_binding = OcpnEventBinding::kIndependent;
  // Return only reachability metadata or include binding sequences in the
  // result envelope.  The regular Ocel API remains available for callers
  // that do not request either diagnostic mode.
  bool exists_trace = false;
  bool return_sequences = false;
  std::uint64_t seed = 1;
  double start = 1767225600.0;
  double case_interval = 60.0;
  double event_interval = 1.0;
};

struct OcpnBinding {
  std::string activity;
  std::vector<std::pair<std::string, std::string>> objects;
};

struct OcpnPlayoutResult {
  Ocel log;
  std::vector<std::vector<OcpnBinding>> sequences;
  bool exists_trace_requested = false;
  bool return_sequences_requested = false;
  bool trace_exists = false;
};

// Enumerates bounded traces of each object-type workflow net and combines the
// object traces into deterministic OCEL event/object/relation records. This is
// the native extensive object-centric playout adapter.
[[nodiscard]] Ocel ocpn_extensive_playout(
    const OcpnGraph& graph, const OcpnPlayoutOptions& options = {});

[[nodiscard]] OcpnPlayoutResult ocpn_extensive_playout_result(
    const OcpnGraph& graph, const OcpnPlayoutOptions& options = {});

[[nodiscard]] std::string serialize_ocpn_playout_json(const Ocel& log);
[[nodiscard]] std::string serialize_ocpn_playout_json(
    const Ocel& log, OcpnEventBinding binding);
[[nodiscard]] std::string serialize_ocpn_playout_json(
    const OcpnPlayoutResult& result, OcpnEventBinding binding);

}  // namespace minera
