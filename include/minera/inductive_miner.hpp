#pragma once

#include "minera/directly_follows_graph.hpp"
#include "minera/event_log.hpp"
#include "minera/process_tree.hpp"

namespace minera {

enum class InductiveMinerVariant {
  // The standard Inductive Miner: cuts are found on the directly-follows
  // graph of each sublog, and the *log* is split and re-mined per child, so
  // every recursion level sees the true relations of its own traces.
  kLog,
  // IMf: the infrequent variant, using the configured noise threshold with
  // the same log-splitting recursion.
  kImf,
  // IMd, the directly-follows variant: the log is read exactly once to build
  // the directly-follows graph, and the recursion then projects the *graph*
  // onto each child instead of splitting traces. Memory and time become
  // proportional to the number of activities rather than the number of
  // events, at some model-quality cost: a projected graph can imply orderings
  // no trace exhibits, which the log-splitting recursion would filter out.
  kDfg,
  IMF = kImf,
  IM = kLog,
  IMD = kDfg,
};

struct InductiveMinerOptions {
  // Noise filter in [0, 1], the Inductive Miner – infrequent (IMf) threshold.
  // Cuts are always sought on the unfiltered relations first; only when a
  // sublog exhibits no cut is the filter applied and the search retried. The
  // filter keeps a directly-follows edge a->b only when its frequency is
  // strictly greater than this fraction of a's strongest observed behaviour
  // (its heaviest outgoing edge, or how often the trace ends at a). Start
  // activities use an inclusive comparison; observed end activities remain
  // available to the endpoint cut. Empty traces are retained when their share
  // is strictly greater than the threshold. 0.0 disables filtering and
  // reproduces the classic Inductive Miner; higher values resist noise that
  // would otherwise collapse to a flower model. Applies to both variants.
  double noise_threshold = 0.0;
  InductiveMinerVariant variant = InductiveMinerVariant::kLog;
  // When enabled, use the normal sequence cut instead of the stricter cut
  // that merges groups made optional by observed skips.
  bool disable_strict_sequence_cut = false;
  // Suppress specialized fall-throughs. The empty-trace and flower cases
  // remain available so the miner still returns a process tree.
  bool disable_fallthroughs = false;
};

// Discovers a block-structured process tree from an event log using the
// Inductive Miner. The algorithm recursively detects exclusive-choice,
// sequence, parallel and loop cuts on the directly-follows graph of the
// (sub)log, splits the log accordingly, and recurses; logs without a cut fall
// through to a flower model so the result always fits the log. With a non-zero
// noise threshold it runs the infrequent variant (IMf), filtering rare
// behaviour so noisy logs still yield a structured model. The kDfg variant
// (IMd) recurses on projections of the directly-follows graph instead of
// sublogs, scaling to huge logs at some model-quality cost.
[[nodiscard]] ProcessTree discover_process_tree(
    const EventLog& event_log,
    const InductiveMinerOptions& options = {});

// Direct graph input always uses the IMd graph-recursion semantics. The
// selector is accepted for scalar compatibility, but is intentionally ignored
// because graph input has no traces to feed the log-recursive variants.
[[nodiscard]] ProcessTree discover_process_tree(
    const DirectlyFollowsGraph& graph,
    const InductiveMinerOptions& options = {});

}  // namespace minera
