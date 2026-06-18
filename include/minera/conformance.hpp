#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "minera/event_log.hpp"
#include "minera/process_tree.hpp"

namespace minera {

// Token-replay fitness of a single trace variant against the model.
struct VariantConformance {
  std::vector<std::string> activities;  // activity names, in trace order
  std::uint64_t count = 0;              // traces collapsing to this variant
  double fitness = 1.0;                 // token-replay fitness in [0, 1]
};

// Conformance of an event log against a discovered process tree, combining
// token-based replay fitness (does the model reproduce the log?) with
// escaping-edges precision (does the model allow much more than the log?).
struct ConformanceResult {
  double fitness = 1.0;    // log-level replay fitness in [0, 1]
  double precision = 1.0;  // escaping-edges precision in [0, 1]

  std::uint64_t fitting_traces = 0;  // traces replayed with no deviation
  std::uint64_t trace_count = 0;

  // Aggregate token-replay bookkeeping, summed over all traces.
  std::uint64_t produced = 0;
  std::uint64_t consumed = 0;
  std::uint64_t missing = 0;
  std::uint64_t remaining = 0;

  std::vector<VariantConformance> variants;  // worst-fitting first
};

// Replays `event_log` against `tree` and reports fitness and precision. The
// process tree is translated to a workflow net; fitness comes from token-based
// replay (Rozinat & van der Aalst) and precision from escaping edges
// (Munoz-Gama & Carmona).
[[nodiscard]] ConformanceResult check_conformance(const EventLog& event_log,
                                                  const ProcessTree& tree);

}  // namespace minera
