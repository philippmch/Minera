#pragma once

#include <cstdint>
#include <optional>
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

// Conformance of an event log against a discovered model: the four standard
// quality dimensions. Token-based replay fitness (does the model reproduce the
// log?), escaping-edges precision (does the model allow much more than the
// log?), generalization (will the model likely accommodate unseen but related
// behaviour, judged by how evenly replay exercises its transitions?), and
// simplicity (is the net itself lean, judged by its mean arc degree?).
struct ConformanceResult {
  double fitness = 1.0;         // log-level replay fitness in [0, 1]
  double precision = 1.0;       // escaping-edges precision in [0, 1]
  double generalization = 1.0;  // token-replay generalization in [0, 1]
  double simplicity = 1.0;      // inverse arc-degree simplicity in (0, 1]

  std::uint64_t fitting_traces = 0;  // traces replayed with no deviation
  std::uint64_t trace_count = 0;

  // Aggregate token-replay bookkeeping, summed over all traces.
  std::uint64_t produced = 0;
  std::uint64_t consumed = 0;
  std::uint64_t missing = 0;
  std::uint64_t remaining = 0;

  std::vector<VariantConformance> variants;  // worst-fitting first
};

// Replays `event_log` against `tree` and reports all four quality dimensions.
// The process tree is translated to a workflow net; fitness comes from
// token-based replay (Rozinat & van der Aalst), precision from escaping edges
// (Munoz-Gama & Carmona), generalization from the replay's per-transition
// execution counts (Buijs, van Dongen & van der Aalst), and simplicity from
// the net's mean arc degree.
[[nodiscard]] ConformanceResult check_conformance(const EventLog& event_log,
                                                  const ProcessTree& tree);

// Token replay against any workflow net (the Alpha Miner produces one directly;
// a process tree or heuristics net is translated with to_petri_net). Replay is
// robust on unsound nets, so unlike alignments it always yields a result.
[[nodiscard]] ConformanceResult check_conformance(const EventLog& event_log,
                                                  const PetriNet& net);

// Footprint (directly-follows relation) conformance between a log and a model.
// The footprint of each is the set of ordered activity pairs (a, b) where b can
// directly follow a — the log's read straight off its traces, the model's found
// by exploring the net's reachable markings. Comparing the two sets tells apart
// behaviour the log shows but the model forbids (a fitness gap) from behaviour
// the model allows but the log never shows (a precision gap). Robust on unsound
// nets, so it yields a reading even where alignments cannot.
struct FootprintRelation {
  std::string source;  // activity a
  std::string target;  // activity b, which directly follows a
};

struct FootprintResult {
  double fitness = 1.0;    // share of log successions the model also allows
  double precision = 1.0;  // share of model successions the log also shows
  std::uint64_t log_relations = 0;     // |L|
  std::uint64_t model_relations = 0;   // |M|
  std::uint64_t shared_relations = 0;  // |L ∩ M|
  bool exhausted = false;  // model exploration hit its bound (relation partial)
  std::vector<FootprintRelation> log_only;    // in log, not model (fitness gap)
  std::vector<FootprintRelation> model_only;  // in model, not log (precision gap)
};

// Footprint conformance of a log against any workflow net, or a process tree
// (translated with to_petri_net). Cheap and robust: it explores only the model's
// reachable markings, so unlike alignments it produces a result even for an
// unsound net.
[[nodiscard]] FootprintResult check_footprints(const EventLog& event_log,
                                               const PetriNet& net);
[[nodiscard]] FootprintResult check_footprints(const EventLog& event_log,
                                               const ProcessTree& tree);

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
  // False when the model could not be aligned. Two distinct causes, told apart
  // by `exhausted`: the final marking is unreachable (an unsound net), or the
  // search exceeded its bound on a reachable but unbounded/very large net. In
  // either case the other fields are meaningless and `variants` is empty.
  bool computed = true;
  bool exhausted = false;  // when !computed: true = search bound hit, false = unsound
  double fitness = 1.0;               // cost-based log fitness in [0, 1]
  // Alignment-based escaping-edges precision: like token-replay precision it
  // asks how much extra behaviour the model allows at the states the log
  // visits, but the states come from each variant's optimal alignment rather
  // than from forced token replay — so on a deviating log the markings are
  // the trustworthy ones an optimal explanation actually reaches.
  double precision = 1.0;
  std::uint64_t fitting_traces = 0;  // traces with a perfect (cost-0) alignment
  std::uint64_t trace_count = 0;
  std::uint64_t total_cost = 0;      // summed optimal cost over all traces
  std::vector<VariantAlignment> variants;  // worst-fitting first
};

// Whether `net`'s final marking is reachable from its initial marking — the
// practical bar for being a sound workflow net, and the precondition for
// alignment. Cheap: a single shortest-run search. The Alpha Miner can produce
// nets that fail this (e.g. concurrent start/end activities overflow the sink).
[[nodiscard]] bool final_marking_reachable(const PetriNet& net);

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

// The conformance of one discovered model, so the frontend can offer any miner
// against any method. `miner` is the UI registry id ("inductive", "alpha",
// "heuristic"). Token replay is always computed (it is cheap and robust);
// `alignments` is optional so the server can defer the costlier alignment to an
// on-demand request while the CLI export still embeds it.
struct ModelConformance {
  std::string miner;
  bool sound = true;  // final marking reachable — whether alignment is possible
  ConformanceResult token_replay;
  FootprintResult footprints;
  std::optional<AlignmentResult> alignments;
};

}  // namespace minera
