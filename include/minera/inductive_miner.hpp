#pragma once

#include "minera/directly_follows_graph.hpp"
#include "minera/event_data.hpp"
#include "minera/event_log.hpp"
#include "minera/process_tree.hpp"

#include <cstdint>
#include <string>
#include <vector>

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
  // would otherwise collapse to a flower model. The selector is effective
  // only for the infrequent variant; the standard and graph variants ignore
  // it.
  double noise_threshold = 0.0;
  InductiveMinerVariant variant = InductiveMinerVariant::kLog;
  // Event-data selectors.  The defaults retain the conventional flat event
  // column names; an EventLog still uses its native activity projection unless
  // a custom selector is supplied.
  std::string activity_key = "concept:name";
  std::string timestamp_key = "time:timestamp";
  std::string case_id_key = "case:concept:name";
  // EventLog callers historically supplied events in case order.  Keep that
  // default while allowing callers using a custom timestamp projection to ask
  // for deterministic chronological ordering explicitly.
  bool sort_events_by_timestamp = false;
  // When enabled, use the normal sequence cut instead of the stricter cut
  // that merges groups made optional by observed skips.
  bool disable_strict_sequence_cut = false;
  // Suppress specialized fall-throughs. The empty-trace and flower cases
  // remain available so the miner still returns a process tree.
  bool disable_fallthroughs = false;
};

// A compact, weighted unique-variant log.  Each entry represents one distinct
// activity sequence and count is the number of cases having that sequence.
// This is useful when a caller already has compressed variants and avoids
// expanding them into an EventLog before discovery.
struct InductiveMinerWeightedVariant {
  std::vector<std::string> activities;
  std::uint64_t count = 1;
};

using InductiveMinerVariantLog =
    std::vector<InductiveMinerWeightedVariant>;

// Applies the EventLog selector, regrouping and optional timestamp ordering
// rules used by discovery.  The returned value is an independent log suitable
// for model decoration and other consumers that must share the exact discovery
// alphabet.
[[nodiscard]] EventLog prepare_inductive_event_log(
    const EventLog& event_log,
    const InductiveMinerOptions& options = {});

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

// Discovers from a generic event-data adapter.  EventStream, EventTable and
// LazyEventTable inputs are grouped by case_id_key, validated for all selected
// fields, and stably ordered by timestamp_key before ordinary discovery.
[[nodiscard]] ProcessTree discover_process_tree(
    const EventDataView& data,
    const InductiveMinerOptions& options = {});

[[nodiscard]] ProcessTree discover_process_tree(
    const InductiveMinerVariantLog& variants,
    const InductiveMinerOptions& options = {});

}  // namespace minera
