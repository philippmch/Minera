#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "minera/conformance.hpp"
#include "minera/event_log.hpp"
#include "minera/petri_net.hpp"

namespace minera::detail {

// One model path for a distinct activity variant. Exact replay paths contain
// synchronous visible moves and silent model routing; non-fitting paths come
// from the optimal alignment and may also contain visible model-only or
// log-only moves.
struct DecisionVariantPath {
  bool available = false;
  bool exact = false;
  std::vector<AlignmentMove> moves;
};

// Decision mining and encoded decision tables must observe the same model
// choices. This shared path collection keeps their replay/alignment fallback
// and trace diagnostics identical.
struct DecisionReplayPaths {
  std::vector<std::vector<std::string>> variants;
  std::vector<std::size_t> variant_of;
  std::vector<DecisionVariantPath> paths;
  std::uint64_t replayed_traces = 0;
  std::uint64_t aligned_traces = 0;
  std::uint64_t alignment_failures = 0;

  [[nodiscard]] std::uint64_t observed_traces() const noexcept {
    return replayed_traces + aligned_traces;
  }
};

[[nodiscard]] DecisionReplayPaths collect_decision_replay_paths(
    const EventLog& log, const PetriNet& net);

}  // namespace minera::detail
