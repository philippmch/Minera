#pragma once

#include "minera/event_log.hpp"
#include "minera/process_tree.hpp"

namespace minera {

struct InductiveMinerOptions {
  // Noise filter in [0, 1], the Inductive Miner – infrequent (IMf) threshold.
  // Before each cut is sought, a directly-follows edge a->b is kept only when
  // its frequency is at least this fraction of a's strongest outgoing edge;
  // rare start/end activities, rare empty traces, and rare repetitions are
  // likewise treated as noise. The full log is still split along the cut, so
  // filtering changes which structure is found, not what behaviour is routed
  // where. 0.0 disables filtering and reproduces the classic Inductive Miner;
  // higher values resist noise that would otherwise collapse to a flower model.
  double noise_threshold = 0.0;
};

// Discovers a block-structured process tree from an event log using the
// Inductive Miner. The algorithm recursively detects exclusive-choice,
// sequence, parallel and loop cuts on the directly-follows graph of the
// (sub)log, splits the log accordingly, and recurses; logs without a cut fall
// through to a flower model so the result always fits the log. With a non-zero
// noise threshold it runs the infrequent variant (IMf), filtering rare
// behaviour so noisy logs still yield a structured model.
[[nodiscard]] ProcessTree discover_process_tree(
    const EventLog& event_log,
    const InductiveMinerOptions& options = {});

}  // namespace minera
