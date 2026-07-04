#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "minera/event_log.hpp"

namespace minera {

// A directed dependency between two activities accepted by the Heuristic Miner,
// carrying both how often it was observed and how reliable the ordering is.
struct HeuristicDependency {
  std::size_t source = 0;
  std::size_t target = 0;
  double dependency = 0.0;      // dependency measure in (-1, 1]
  std::uint64_t frequency = 0;  // |source > target|: direct successions observed
};

// One bundle of an activity's split (or join): the neighbours it activates
// (or awaits) together. An activity takes exactly one of its bundles — XOR
// across bundles — and all members of the chosen bundle run in parallel.
using HeuristicBundle = std::vector<std::size_t>;

// A heuristics net: the dependency graph the Heuristic Miner discovers. Unlike
// the raw directly-follows graph it keeps only edges whose dependency measure
// and frequency clear the configured thresholds, which is what lets it ignore
// the infrequent, noisy paths that mislead the Alpha Miner.
struct HeuristicsNet {
  std::vector<std::string> activities;
  std::vector<HeuristicDependency> edges;  // sorted by (source, target)
  std::vector<std::size_t> start_activities;
  std::vector<std::size_t> end_activities;

  // Split/join semantics per activity, derived from the AND measure: outgoing
  // successors grouped into parallel bundles, and incoming predecessors
  // likewise. Indexed by activity id; a self-loop edge is always its own
  // bundle on both sides.
  std::vector<std::vector<HeuristicBundle>> output_bundles;
  std::vector<std::vector<HeuristicBundle>> input_bundles;
};

struct HeuristicMinerOptions {
  // Minimum dependency measure for an edge to be accepted. Higher values keep
  // only strongly directional relations.
  double dependency_threshold = 0.9;
  // Minimum number of direct successions for an edge to be considered.
  std::uint64_t min_frequency = 1;
  // When true, every activity is connected to its strongest dependency even if
  // that edge is below the threshold, so no task is left dangling.
  bool connect_all = true;
  // Minimum AND measure for two edges leaving (or entering) the same activity
  // to be bundled as a parallel split (or join) rather than an exclusive
  // choice. Two successors b, c of a are parallel when they follow each other
  // often enough relative to the split itself:
  //   (|b>c| + |c>b|) / (|a>b| + |a>c| + 1) >= and_threshold
  // (mirrored for joins). 0.65 is the customary default.
  double and_threshold = 0.65;
};

// Discovers a heuristics net from an event log. The dependency of a over b is
// (|a>b| - |b>a|) / (|a>b| + |b>a| + 1); self-loops use |a>a| / (|a>a| + 1).
[[nodiscard]] HeuristicsNet discover_heuristics_net(
    const EventLog& event_log,
    const HeuristicMinerOptions& options = {});

}  // namespace minera
