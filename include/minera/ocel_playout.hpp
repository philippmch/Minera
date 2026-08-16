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
  INDEPENDENT = kIndependent,
  SHARED_ACTIVITY = kSharedActivity,
};

struct OcpnPlayoutOptions {
  // Maximum number of firings of any visible activity in one object trace.
  // The extensive search is bounded by this value and by max_states.
  std::size_t max_bindings_per_activity = 2;
  std::size_t max_traces = 100;
  std::size_t max_states = 200000;
  // Optional object ids per object type. Missing types receive one synthetic
  // id named <type>_1.
  std::map<std::string, std::vector<std::string>> objects;
  bool objects_unique_per_trace = false;
  OcpnEventBinding event_binding = OcpnEventBinding::kIndependent;
  std::uint64_t seed = 1;
  double start = 1767225600.0;
  double case_interval = 60.0;
  double event_interval = 1.0;
};

// Enumerates bounded traces of each object-type workflow net and combines the
// object traces into deterministic OCEL event/object/relation records. This is
// the native extensive object-centric playout adapter.
[[nodiscard]] Ocel ocpn_extensive_playout(
    const OcpnGraph& graph, const OcpnPlayoutOptions& options = {});

[[nodiscard]] std::string serialize_ocpn_playout_json(const Ocel& log);

}  // namespace minera
