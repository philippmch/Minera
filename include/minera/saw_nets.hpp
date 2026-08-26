#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>

#include "minera/ocel.hpp"

namespace minera {

// A stochastic arc-weight distribution records how often a transition used a
// given positive token multiplicity while replaying one object-centric log.
// The key is the observed multiplicity and the value is the number of
// original events with that multiplicity (shared events are counted once).
using SawWeightDistribution = std::map<std::size_t, std::uint64_t>;

struct SawNet {
  std::string object_type;
  PetriNet net;
  // One distribution is shared by all input/output arcs of a transition,
  // matching the stochastic-arc convention while keeping the workflow net
  // representation compact.
  std::map<std::size_t, SawWeightDistribution> transition_weights;
};

struct SawNetDiscoveryResult {
  std::map<std::string, SawNet> ot_saw_nets;
  SawNet multi_saw_net;
  // Decorations for the merged net: every place retains its source object
  // type. Visible transitions are merged by activity label; silent
  // transitions remain object-type-specific.
  std::map<std::size_t, std::string> multi_place_object_types;
  std::map<std::size_t, std::string> multi_silent_transition_object_types;
  // Every normal arc in the merged net is identified by its transition,
  // direction, and position in that transition's arc list.  This keeps
  // decorations stable even when equal activity transitions are merged.
  std::map<std::string, std::string> multi_arc_object_types;
};

struct SawNetDiscoveryOptions {
  // Optional selectors for the activity and object-type labels used by the
  // flattened discovery projection. Empty selectors use canonical fields.
  std::string activity_attribute;
  std::string object_type_attribute;
};

// Discovers a classic stochastic-arc-weight net for every object type and a
// merged net in which visible transitions with the same activity are shared.
// Each per-type model is induced from that type's flattened object cases and
// weighted by clean token-replay firing multiplicities.
[[nodiscard]] SawNetDiscoveryResult discover_saw_nets(const Ocel& log);
[[nodiscard]] SawNetDiscoveryResult discover_saw_nets(
    const Ocel& log, const SawNetDiscoveryOptions& options);

[[nodiscard]] std::string saw_nets_json(const SawNetDiscoveryResult& result);

}  // namespace minera
