#pragma once

// algo.conformance.alignments.decomposed.variants.recompos_maximal and the
// maximal decomposition it uses (objects.petri_net.utils.decomposition) of the
// PM4Py 2.7.22 reference, as observed from the inputs and outputs of its
// helpers.
//
// decompose joins two places when a silent transition touches both (as input
// or output place). Of the visible transitions sharing a label, only the last
// one in the net's transition iteration order joins its places; other visible
// transitions join nothing. The components are the connected groups of
// places, in the order of their first place in the net's place iteration
// order. A component holds its places, every transition with an arc to or
// from one of them, those arcs, the markings restricted to its places and the
// sorted labels of its visible transitions (a label once per transition).
// merge_sublist_nets unites components the same way.
//
// apply aligns each distinct trace once, in order of first occurrence, and
// keeps two caches across the log: alignments by (component object,
// projected trace) and merged components by the set of the merged
// components' transition sets. Per trace it walks the component list: a
// component that sees no event of the trace (the trace projected onto its
// labels) gets no alignment; otherwise its projection is aligned (standard
// costs, trace transitions named t_<label>_<index in the projection>) and
// each event activity is mapped to the list of its log-side moves (1 for a
// log move, 0 for a synchronous one). Whenever two aligned components report
// different lists for an activity, every component holding that activity is
// merged (in descending index order), removed and the merge appended, and the
// walk resumes at the first component not yet aligned.
//
// recompose_alignment links component i to j (i != j, both aligned) when the
// labels of i's last move equal those of j's first move. It first runs a
// queue from the components with an initial marking, without any visited
// check (so it never ends when a cycle is reachable from them), appending
// each popped component's alignment, without its first move from the second
// popped component on. It then takes the components not visited yet, bubble
// sorts them (swapping neighbours u, v when v links to u and u does not link
// to v, until stable), and runs a queue over them that skips visited ones
// and appends the moves of each, again without the first from the second
// component on, that it has not appended in this second phase yet.
//
// The cost is the sum of the move costs (10000 per log or visible model move,
// 1 per silent model move); the best-worst cost bwc is 10000 times the
// number of events plus the visible model moves of the empty trace's
// alignment of the whole net, and fitness is 1 - (cost without silent moves)
// / bwc. The empty trace gives cost 0 and its alignment without fitness or
// bwc.

#include <cstddef>
#include <functional>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace minera {

class DecomposedReferenceError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

// An accepting Petri net with ordinary arcs, places and transitions listed in
// the reference's iteration order (which decompose follows).
struct DecomposedReferenceNet {
  std::vector<std::string> places;
  std::vector<std::pair<std::string, std::optional<std::string>>> transitions;  // name, label (nullopt silent)
  std::vector<std::pair<std::string, std::string>> arcs;                          // source, target
  std::vector<std::pair<std::string, std::size_t>> initial, final;                // place, tokens
  bool operator==(const DecomposedReferenceNet&) const = default;
};

// A component: its net (everything sorted by name) and visible labels.
struct DecomposedReferenceComponent {
  DecomposedReferenceNet net;
  std::vector<std::string> labels;  // sorted, once per visible transition
  bool operator==(const DecomposedReferenceComponent&) const = default;
};

[[nodiscard]] std::vector<DecomposedReferenceComponent> reference_decompose(const DecomposedReferenceNet& net);

[[nodiscard]] DecomposedReferenceComponent reference_decomposed_merge(
    const std::vector<DecomposedReferenceComponent>& components);

// One move. trace_transition and log are ">>" for a model move;
// model_transition and model are ">>" for a log move, and model is nullopt
// for a silent transition.
struct DecomposedReferenceMove {
  std::string trace_transition, model_transition;
  std::string log;
  std::optional<std::string> model;
  bool operator==(const DecomposedReferenceMove&) const = default;
  auto operator<=>(const DecomposedReferenceMove&) const = default;
};
using DecomposedReferenceMoves = std::vector<DecomposedReferenceMove>;

[[nodiscard]] long long reference_decomposed_move_cost(const DecomposedReferenceMove& move);

struct DecomposedReferenceAlignment {
  DecomposedReferenceMoves moves;
  long long cost = 0;
  bool operator==(const DecomposedReferenceAlignment&) const = default;
};

// An optimal alignment of the trace with the net under the standard costs
// (Dijkstra over the synchronous product); nullopt when no run reaches the
// final marking. The reference picks among optimal alignments by object
// addresses.
[[nodiscard]] std::optional<DecomposedReferenceAlignment> reference_decomposed_alignment(
    const DecomposedReferenceNet& net, const std::vector<std::string>& trace);

// The visible model moves of the empty trace's alignment of the net.
[[nodiscard]] std::size_t reference_decomposed_best_worst_cost(const DecomposedReferenceNet& net);

// order_nodes_second_round.
[[nodiscard]] std::vector<std::size_t> reference_decomposed_order(
    std::vector<std::size_t> nodes, const std::set<std::pair<std::size_t, std::size_t>>& edges);

// recompose_alignment for components with (true) or without an initial
// marking and their alignments. Throws DecomposedReferenceError where the
// reference never returns (a link cycle reachable from a component with an
// initial marking).
[[nodiscard]] DecomposedReferenceMoves reference_decomposed_recompose(
    const std::vector<bool>& marked, const std::vector<std::optional<DecomposedReferenceMoves>>& alignments);

// Hooks for replaying the reference's own alignments (align replaces the
// native Dijkstra) and observing the run. Components are numbered as objects:
// the decomposition first, then every merge as it is made.
struct DecomposedReferenceOracle {
  std::function<void(const std::vector<std::string>& trace)> variant;
  std::function<std::optional<DecomposedReferenceAlignment>(std::size_t component,
                                                             const DecomposedReferenceComponent& net,
                                                             const std::vector<std::string>& projection)>
      align;
  std::function<void(const std::vector<std::size_t>& inputs, std::size_t merged)> merged;
  std::function<void(const std::vector<std::size_t>& components)> recomposed;
};

struct DecomposedReferenceResult {
  std::vector<std::pair<std::string, std::optional<std::string>>> alignment;  // log, model labels
  long long cost = 0;
  std::optional<double> fitness;
  std::optional<long long> bwc;
  bool operator==(const DecomposedReferenceResult&) const = default;
};

// apply on a log: one result per trace. Throws DecomposedReferenceError where
// the reference never returns.
[[nodiscard]] std::vector<DecomposedReferenceResult> reference_decomposed_alignments(
    const DecomposedReferenceNet& net, const std::vector<std::vector<std::string>>& traces,
    const DecomposedReferenceOracle& oracle = {});

}  // namespace minera
