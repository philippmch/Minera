#pragma once

#include <cstddef>
#include <string>

#include "minera/event_log.hpp"
#include "minera/petri_analysis.hpp"
#include "minera/petri_net.hpp"

namespace minera {

// Bounded, deterministic discovery of a workflow net from a traditional
// event log.  The activity key is read from Event::activity when empty (or
// when set to "activity" / "concept:name"), otherwise from the named event
// attribute.  Missing custom attributes fall back to Event::activity.
struct ILPMinerOptions {
  std::string activity_key;
  double noise_threshold = 0.0;
  std::size_t max_nodes = 100000;
};

struct ILPMinerDiagnostics {
  bool computed = false;
  bool exhausted = false;
  std::size_t visited_nodes = 0;
  std::size_t lp_solved = 0;
  std::size_t branch_and_bound_nodes = 0;
  std::string message;
};

struct ILPMinerResult {
  PetriNet net;
  WorkflowMarkings markings;
  ILPMinerDiagnostics diagnostics;

  // Convenience aliases for clients that use the longer terminology.
  [[nodiscard]] const PetriNet& petri_net() const noexcept { return net; }
  [[nodiscard]] const WorkflowMarkings& workflow_markings() const noexcept {
    return markings;
  }
};

[[nodiscard]] ILPMinerResult mine_ilp(
    const EventLog& event_log, const ILPMinerOptions& options = {});

// Descriptive spelling retained for callers that prefer discovery-oriented
// names.  Both entry points have identical semantics.
[[nodiscard]] inline ILPMinerResult discover_ilp_miner(
    const EventLog& event_log, const ILPMinerOptions& options = {}) {
  return mine_ilp(event_log, options);
}

using IlpMinerOptions = ILPMinerOptions;
using IlpMinerDiagnostics = ILPMinerDiagnostics;
using IlpMinerResult = ILPMinerResult;

}  // namespace minera

