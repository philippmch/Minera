#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "minera/event_log.hpp"
#include "minera/petri_net.hpp"
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

// Token replay against any workflow net (the Alpha Miner produces one directly;
// a process tree or heuristics net is translated with to_petri_net). Replay is
// robust on unsound nets, so unlike alignments it always yields a result.
[[nodiscard]] ConformanceResult check_conformance(const EventLog& event_log,
                                                  const PetriNet& net);

// One move of an optimal alignment between a trace and the model.
enum class AlignmentMoveType {
  kSynchronous,  // log and model agree on the activity — no deviation (cost 0)
  kLog,          // the log event has no matching model step here (cost 1)
  kModel,        // the model must fire a visible transition the log skips (cost 1)
};

// A single step of an alignment. Silent (tau) model moves are routing only and
// are never emitted, so every move carries a non-empty activity name.
struct AlignmentMove {
  AlignmentMoveType type;
  std::string activity;  // the activity name involved in the move
};

// The optimal (least-cost) alignment of one trace variant against the model.
struct VariantAlignment {
  std::vector<std::string> activities;  // activity names, in trace order
  std::uint64_t count = 0;              // traces collapsing to this variant
  std::vector<AlignmentMove> moves;     // the alignment, in execution order
  std::uint64_t cost = 0;               // total cost (log + visible model moves)
  double fitness = 1.0;                 // 1 - cost / worst-case cost, in [0, 1]
};

// Alignment-based conformance of an event log against a discovered process
// tree. Unlike token replay, an alignment finds for each trace the cheapest
// sequence of synchronous, log-only, and model-only moves that walks the model
// from its initial to its final marking — giving an exact, optimal account of
// where and how each trace deviates.
struct AlignmentResult {
  // False when the net is not a sound workflow net — its final marking is
  // unreachable, or the search could not align a trace within its bounds (as
  // can happen for Alpha/Heuristic models, which carry no soundness guarantee).
  // The other fields are then meaningless and `variants` is empty.
  bool computed = true;
  double fitness = 1.0;               // cost-based log fitness in [0, 1]
  std::uint64_t fitting_traces = 0;  // traces with a perfect (cost-0) alignment
  std::uint64_t trace_count = 0;
  std::uint64_t total_cost = 0;      // summed optimal cost over all traces
  std::vector<VariantAlignment> variants;  // worst-fitting first
};

// Computes optimal alignments of every trace variant against the model using a
// uniform-cost search over the synchronous product, and reports cost-based
// fitness overall and per variant. The standard cost function charges 1 for a
// log-only or visible model-only move and 0 for a synchronous or silent move
// (Adriansyah, van der Aalst & van Dongen).
//
// The process-tree overload translates the tree to a (always sound) workflow
// net; the Petri-net overload aligns against any net directly but, because a
// net from the Alpha Miner need not be sound, returns `computed == false`
// rather than misleading numbers when its final marking cannot be reached.
[[nodiscard]] AlignmentResult check_alignment(const EventLog& event_log,
                                              const ProcessTree& tree);
[[nodiscard]] AlignmentResult check_alignment(const EventLog& event_log,
                                              const PetriNet& net);

// The conformance of one discovered model, checked every supported way, so the
// frontend can offer any miner against any method. `miner` is the miner id used
// by the UI registry ("inductive", "alpha", "heuristic").
struct ModelConformance {
  std::string miner;
  ConformanceResult token_replay;
  AlignmentResult alignments;
};

}  // namespace minera
