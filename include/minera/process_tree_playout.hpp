#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "minera/event_log.hpp"
#include "minera/process_tree.hpp"

namespace minera {

// Strategies for generating a log from a block-structured process tree.
enum class ProcessTreePlayoutVariant {
  kBasicPlayout,
  kExtensive,
  kTopBottom,
  kBasic = kBasicPlayout,
  kTopbottom = kTopBottom,
  BASIC_PLAYOUT = kBasicPlayout,
  EXTENSIVE = kExtensive,
  TOPBOTTOM = kTopBottom,
  TOP_BOTTOM = kTopBottom,
};

// algo.simulation.playout.process_tree of PM4Py 2.7.22.
//
// TOPBOTTOM (the default) is exact: the draws are Python's random module
// seeded with random.seed(seed), in the reference order (see
// process_tree_playout.cpp), so `traces` traces reproduce the reference log
// for the same seed whatever the string-hash seed. BASIC_PLAYOUT follows the
// reference's distribution (its enabled-vertex set iterates in string-hash
// order, so no seed reproduces it); EXTENSIVE builds the reference's
// bottom-up word sets with its length filters (see process_tree_playout.cpp),
// so it is not the tree's language: words outside the length bounds can stay
// and words inside them can be dropped, a loop reads only its first redo
// child, and the trees the reference rejects (inclusive choice,
// interleaving, a sequence or parallel cut with one child, equal subtrees
// that leave its bottom-up walk short of the root) are rejected. The words
// come sorted by length, then by activities. The TOPBOTTOM and EXTENSIVE logs have the activity only (no case
// id, no timestamp); BASIC_PLAYOUT names its cases "0", "1", ... and stamps
// the events one second apart from epoch 10,000,000 (1970-04-26T17:46:40Z),
// counting on across cases. A tree with an inclusive choice raises under TOPBOTTOM, as the
// reference raises TypeError.
struct ProcessTreePlayoutOptions {
  ProcessTreePlayoutVariant variant = ProcessTreePlayoutVariant::kTopBottom;
  std::size_t traces = 1000;                // basic/top-bottom cases (num_traces)
  // Safety bound on the visible events of one sampled trace (together with
  // max_length); a longer trace raises instead of being dropped, so a run
  // either reproduces the reference or fails.
  std::size_t max_steps = 1000;
  std::uint64_t seed = 1;
  // EXTENSIVE max_trace_length; unset, the tree's shortest word length, as
  // in the reference. The sampled variants use it as a second safety bound
  // next to max_steps (unset: 1000, so the per-trace limit is
  // min(max_steps, 1000)).
  std::optional<std::size_t> max_length;
  std::size_t min_length = 1;                // EXTENSIVE min_trace_length
  // EXTENSIVE max_limit_num_traces: the cap on a parallel cut's words.
  std::size_t max_traces = 100000;
  // EXTENSIVE max_loop_occ; unset, half of max_trace_length (rounded down),
  // as in the reference. The sampled variants repeat loops without a bound,
  // as the reference does, unless cap_sampled_loops opts in (a declared
  // extension that changes the draws; unset, three redos).
  std::optional<std::size_t> max_loop_occurrences;
  bool cap_sampled_loops = false;
  // Declared extension: case ids case_1, case_2, ... and timestamps
  // start + i * case_interval + j * event_interval instead of the reference
  // envelope above. Off by default.
  bool add_envelope = false;
  double start = 1767225600.0;
  double case_interval = 60.0;
  double event_interval = 1.0;
  // Extensive mode can expose the canonical activity-word set through the
  // result helper while the legacy EventLog API remains unchanged.
  bool return_set_strings = false;
  // Declared extension (server bound): EXTENSIVE keeps at most this many
  // words of the final result (the shortest first, in the result order) and
  // marks it truncated. 0, the default, keeps them all, as the reference.
  std::size_t output_trace_cap = 0;
  // Polled while a playout runs (see minera/cancellation.hpp); returning true
  // throws OperationCancelled. Unset never cancels.
  std::function<bool()> cancelled;
};

// The EXTENSIVE bounds a run uses once unset options take the reference
// defaults: max_length (unset: the tree's shortest word length) and
// max_loop_occurrences (unset: max_length / 2). Validates the tree.
struct ProcessTreeExtensiveBounds {
  std::size_t max_length = 0;
  std::size_t max_loop_occurrences = 0;
};
[[nodiscard]] ProcessTreeExtensiveBounds resolve_process_tree_extensive_bounds(
    const ProcessTree& tree, const ProcessTreePlayoutOptions& options);

struct ProcessTreePlayoutResult {
  EventLog log;
  std::vector<std::vector<std::string>> trace_strings;
  // Extensive enumeration is finite only under the configured language and
  // loop bounds. This flag makes a safety-budget cutoff observable instead of
  // presenting a truncated language as complete.
  bool truncated = false;
};

[[nodiscard]] ProcessTreePlayoutResult process_tree_playout_result(
    const ProcessTree& tree, ProcessTreePlayoutOptions options = {});

[[nodiscard]] EventLog process_tree_playout(
    const ProcessTree& tree, ProcessTreePlayoutOptions options = {});

// Convenience aliases.  The ProcessTree overload is distinct from the
// Petri-net playout API in minera/playout.hpp.
[[nodiscard]] inline EventLog playout(
    const ProcessTree& tree, ProcessTreePlayoutOptions options = {}) {
  return process_tree_playout(tree, std::move(options));
}

[[nodiscard]] inline EventLog extensive_process_tree_playout(
    const ProcessTree& tree, ProcessTreePlayoutOptions options = {}) {
  options.variant = ProcessTreePlayoutVariant::kExtensive;
  return process_tree_playout(tree, std::move(options));
}

[[nodiscard]] inline EventLog basic_process_tree_playout(
    const ProcessTree& tree, ProcessTreePlayoutOptions options = {}) {
  options.variant = ProcessTreePlayoutVariant::kBasicPlayout;
  return process_tree_playout(tree, std::move(options));
}

[[nodiscard]] inline EventLog topbottom_process_tree_playout(
    const ProcessTree& tree, ProcessTreePlayoutOptions options = {}) {
  options.variant = ProcessTreePlayoutVariant::kTopBottom;
  return process_tree_playout(tree, std::move(options));
}

}  // namespace minera
