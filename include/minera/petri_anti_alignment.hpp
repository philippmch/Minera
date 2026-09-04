#pragma once

#include <cstddef>
#include <vector>
#include <string>
#include "minera/event_log.hpp"
#include "minera/petri_net.hpp"

namespace minera {

enum class PetriAlignmentVariant {
  kDiscountedAStar,
  VERSION_DISCOUNTED_A_STAR = kDiscountedAStar,
};

// Options for the discounted A* anti-alignment. Search may
// revisit a marking; marking_limit bounds the number of closed expansions.
struct PetriAntiAlignmentOptions {
  PetriAlignmentVariant variant = PetriAlignmentVariant::kDiscountedAStar;
  double exponent = 2.0;
  double epsilon = 0.005;  // length-divisor parameter
  std::size_t marking_limit = 100000;
  std::size_t max_states = 0;  // compatibility alias; zero uses marking_limit
  std::vector<std::size_t> initial_marking;
  std::vector<std::size_t> final_marking;
  // Event field used to construct the log variants.
  std::string activity_attribute = "concept:name";
};

struct PetriAntiAlignmentDiagnostics {
  std::size_t visited_states = 0;
  std::size_t queued_states = 0;
  std::size_t traversed_arcs = 0;
  bool exhausted = false;
};

struct PetriAntiAlignmentResult {
  // Visible model sequence and complete transition sequence (tau included).
  std::vector<std::string> activities;
  std::vector<std::size_t> transitions;
  double cost = 0.0;      // negative anti-alignment objective
  double objective = 0.0; // same negative value, retained for callers
  double precision = 1.0;
  [[nodiscard]] const std::vector<std::string>& visible_sequence() const noexcept { return activities; }
  [[nodiscard]] const std::vector<std::size_t>& transition_sequence() const noexcept { return transitions; }
  PetriAntiAlignmentDiagnostics diagnostics;
};

[[nodiscard]] PetriAntiAlignmentResult anti_align_petri(
    const PetriNet& net, const EventLog& log,
    PetriAntiAlignmentOptions options = {});

// Compatibility spellings used by wrappers.
inline PetriAntiAlignmentResult apply_petri_anti_alignment(
    const PetriNet& net, const EventLog& log,
    PetriAntiAlignmentOptions options = {}) {
  return anti_align_petri(net, log, options);
}
inline PetriAntiAlignmentResult petri_anti_alignment(
    const PetriNet& net, const EventLog& log,
    PetriAntiAlignmentOptions options = {}) {
  return anti_align_petri(net, log, options);
}
inline PetriAntiAlignmentResult anti_alignment(
    const EventLog& log, const PetriNet& net,
    PetriAntiAlignmentOptions options = {}) {
  return anti_align_petri(net, log, options);
}

}  // namespace minera
