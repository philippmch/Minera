#pragma once

#include <cstddef>
#include <vector>
#include <string>
#include "minera/event_log.hpp"
#include "minera/petri_net.hpp"
#include "minera/petri_anti_alignment.hpp"

namespace minera {

// Options for the discounted A* multi-alignment.
struct PetriMultiAlignmentOptions {
  PetriAlignmentVariant variant = PetriAlignmentVariant::kDiscountedAStar;
  double exponent = 2.0;
  std::size_t marking_limit = 100000;
  std::size_t max_states = 0;
  std::vector<std::size_t> initial_marking;
  std::vector<std::size_t> final_marking;
  // Event field used to construct the log variants.
  std::string activity_attribute = "concept:name";
};

struct PetriMultiAlignmentDiagnostics {
  std::size_t visited_states = 0;
  std::size_t queued_states = 0;
  std::size_t traversed_arcs = 0;
  bool exhausted = false;
};

struct PetriMultiAlignmentResult {
  std::vector<std::string> activities;
  std::vector<std::size_t> transitions;
  [[nodiscard]] const std::vector<std::string>& visible_sequence() const noexcept { return activities; }
  [[nodiscard]] const std::vector<std::size_t>& transition_sequence() const noexcept { return transitions; }
  double cost = 0.0;      // minimum raw maximum discounted distance
  double objective = 0.0; // same value, retained for callers
  double max_distance_to_log = 0.0; // ordinary insert/delete distance
  PetriMultiAlignmentDiagnostics diagnostics;
};

[[nodiscard]] PetriMultiAlignmentResult multi_align_petri(
    const PetriNet& net, const EventLog& log,
    PetriMultiAlignmentOptions options = {});

inline PetriMultiAlignmentResult apply_petri_multi_alignment(
    const PetriNet& net, const EventLog& log,
    PetriMultiAlignmentOptions options = {}) {
  return multi_align_petri(net, log, options);
}
inline PetriMultiAlignmentResult petri_multi_alignment(
    const PetriNet& net, const EventLog& log,
    PetriMultiAlignmentOptions options = {}) {
  return multi_align_petri(net, log, options);
}
inline PetriMultiAlignmentResult multi_alignment(
    const EventLog& log, const PetriNet& net,
    PetriMultiAlignmentOptions options = {}) {
  return multi_align_petri(net, log, options);
}

}  // namespace minera
