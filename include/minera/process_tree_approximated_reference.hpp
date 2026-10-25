#pragma once

// algo.conformance.alignments.process_tree.variants.approximated (original,
// utilities and calculate_a_sa_ea_sets) of the PM4Py 2.7.22 reference, as
// observed from the inputs and outputs of its helpers.
//
// apply first makes the tree binary in place
// (process_tree_to_binary_process_tree): an operator with more than two
// children keeps its first child and nests the others right-wise under a copy
// of itself, except that a loop nests its redo children under a choice, so
// *(a, b, c, d) becomes *(a, X(b, X(c, d))). Single-child operators stay and
// make the set computation raise. Per binary node it computes the activity
// set A, the start and end activity sets SA and EA and the tau flag (the node
// accepts the empty trace): a leaf has {label} (nothing and tau for tau); a
// sequence starts with SA1 (and SA2 when its first child is tau) and ends with
// EA2 (and EA1 when its second child is tau) and is tau when both are; a
// choice and a parallel unite both sets, a choice being tau when either child
// is and a parallel when both are; a loop starts with SA_do (and SA_redo when
// the do is tau), ends likewise and is tau when its do is. Heights count
// nodes: 1 for a leaf, 1 + the highest child otherwise.
//
// A trace reaching a node is aligned optimally (calculate_optimal_alignment)
// when its length is at most max_trace_length or the node's height at most
// max_process_tree_height. Otherwise a leaf gives None and an operator splits
// the trace over its two children and recurses into them in order, stopping
// with None at the first child that gives None:
// - a choice sends the whole trace to the child with the fewest mismatches,
//   the first on a tie: the first event outside its SA, the last event
//   outside its EA and each other event outside its A count one each;
// - a sequence, a parallel and a loop solve a binary program with PuLP/CBC.
//   Per event i and child j, x_i_j assigns the event, s_i_j, e_i_j and v_i_j
//   mark it as the first, last or a middle event of the child's (sequence,
//   parallel) or segment's (loop) part and cost 1 when the event lies outside
//   SA, EA or A. A sequence keeps the parts in child order; u_j marks a used
//   child and an unused one costs its weight (0 for a tau child, 1 when SA and
//   EA meet, else 2). A loop alternates do and redo segments, starting and
//   ending with the do; t_i_j puts an empty execution of child j before event
//   i (between two events of the other child, or at an end next to a redo
//   event) at its weight, or -1e-05 for a tau child; n_i_j and p_i_j mark
//   that the other child follows or precedes. The rows are generated in a
//   fixed order the program builder below reproduces.
//   A sequence and a parallel call child 0 with its events and then child 1;
//   a loop calls one child per segment in trace order. Sequence and loop
//   concatenate the child alignments; a parallel walks the trace and, per
//   event, moves its child's alignment up to and including that child's next
//   log-side move, then appends what is left of child 0 and then of child 1.
//
// The optimal alignment converts the subtree with the transition-bordered
// Petri-net conversion (every operator gets a silent entry and exit
// transition, a parallel's being its split and join) and runs the Dijkstra
// variant with the standard costs (10000 per log or visible model move, 1 per
// silent model move, 0 per synchronous move). It keeps the moves of leaf
// transitions only, so a tau leaf shows as (">>", None). Among optimal
// alignments the reference picks by object addresses, so which one comes back
// varies between runs.
//
// apply aligns each distinct trace of the log once. A result's cost counts
// the log moves and the visible model moves, and fitness is
// 1 - cost / (bwc + len(trace)) with bwc the empty trace's Dijkstra cost on
// the whole binary tree (silent moves included).

#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "minera/process_tree.hpp"

namespace minera {

class ProcessTreeApproximatedReferenceError : public std::invalid_argument {
 public:
  using std::invalid_argument::invalid_argument;
};

// process_tree_to_binary_process_tree. Throws ProcessTreeApproximatedReferenceError
// for operators other than sequence, choice, parallel and loop.
[[nodiscard]] ProcessTree reference_approximated_binary_tree(const ProcessTree& tree);

struct ProcessTreeApproximatedSets {
  std::vector<std::string> activities, start, end;  // sorted
  bool tau = false;
  std::size_t height = 0;
  bool operator==(const ProcessTreeApproximatedSets&) const = default;
};

// The sets and height of every node of a binary tree, in preorder. Throws
// ProcessTreeApproximatedReferenceError for an operator without exactly two
// children (the reference's assertion).
[[nodiscard]] std::vector<ProcessTreeApproximatedSets> reference_approximated_sets(const ProcessTree& binary);

// One move: log is ">>" for a model move; model is ">>" for a log move and
// nullopt for a tau leaf.
struct ProcessTreeApproximatedMove {
  std::string log;
  std::optional<std::string> model;
  bool operator==(const ProcessTreeApproximatedMove&) const = default;
};
using ProcessTreeApproximatedMoves = std::vector<ProcessTreeApproximatedMove>;

enum class ProcessTreeApproximatedSplitKind { kSequence, kParallel, kLoop };

struct ProcessTreeApproximatedRow {
  std::vector<std::pair<std::string, double>> terms;
  int sense = 0;          // -1 for <=, 0 for =, 1 for >= (PuLP's)
  double constant = 0.0;  // the row reads terms + constant (sense) 0
};

struct ProcessTreeApproximatedProgram {
  std::vector<std::pair<std::string, double>> objective;  // nonzero terms
  double constant = 0.0;
  std::vector<ProcessTreeApproximatedRow> rows;  // in PuLP's _C1, _C2, ... order
};

// The program a sequence, parallel or loop node hands PuLP for a nonempty
// trace and its two children's sets.
[[nodiscard]] ProcessTreeApproximatedProgram reference_approximated_program(
    ProcessTreeApproximatedSplitKind kind, const std::vector<std::string>& trace,
    const std::vector<ProcessTreeApproximatedSets>& children);

// Objective value of a 0/1 assignment (unlisted variables are 0).
[[nodiscard]] double reference_approximated_objective(const ProcessTreeApproximatedProgram& program,
                                                      const std::map<std::string, double>& solution);

struct ProcessTreeApproximatedSplit {
  std::vector<std::size_t> assignment;  // child per event
  // Loop only: per position 0..n, the child executed empty before event i.
  std::vector<std::optional<std::size_t>> empty;
  bool operator==(const ProcessTreeApproximatedSplit&) const = default;
};

// An optimal split (exact dynamic programming over the program's
// combinatorial structure) and its objective value. CBC may return another
// optimum.
[[nodiscard]] std::pair<ProcessTreeApproximatedSplit, double> reference_approximated_optimal_split(
    ProcessTreeApproximatedSplitKind kind, const std::vector<std::string>& trace,
    const std::vector<ProcessTreeApproximatedSets>& children);

// The split the reference reads from a solution (x_i_j and t_i_j at 1).
[[nodiscard]] ProcessTreeApproximatedSplit reference_approximated_split_from_solution(
    ProcessTreeApproximatedSplitKind kind, std::size_t events, const std::map<std::string, double>& solution);

// The child calls (child index, subtrace) an operator makes for a split.
[[nodiscard]] std::vector<std::pair<std::size_t, std::vector<std::string>>> reference_approximated_plan(
    ProcessTreeApproximatedSplitKind kind, const std::vector<std::string>& trace,
    const ProcessTreeApproximatedSplit& split);

// The operator's alignment from its children's alignments (in plan order).
[[nodiscard]] ProcessTreeApproximatedMoves reference_approximated_compose(
    ProcessTreeApproximatedSplitKind kind, const std::vector<std::string>& trace,
    const ProcessTreeApproximatedSplit& split, const std::vector<ProcessTreeApproximatedMoves>& children);

// The child a choice node sends a nonempty trace to.
[[nodiscard]] std::size_t reference_approximated_choice(const std::vector<std::string>& trace,
                                                        const std::vector<ProcessTreeApproximatedSets>& children);

// An optimal alignment of the subtree rooted at node (as its own tree) with
// the trace, and its cost in the standard units.
[[nodiscard]] std::pair<ProcessTreeApproximatedMoves, long long> reference_approximated_optimal_alignment(
    const ProcessTree& tree, const ProcessTreeNode& node, const std::vector<std::string>& trace);

// The cost apply reports: log moves plus visible model moves.
[[nodiscard]] std::size_t reference_approximated_cost(const ProcessTreeApproximatedMoves& alignment);

// Hooks that replace the native solver and base alignments (for replaying
// the reference's own choices) or observe choices and returns. Nodes are
// preorder indices of the binary tree.
struct ProcessTreeApproximatedOracle {
  std::function<std::map<std::string, double>(std::size_t node, ProcessTreeApproximatedSplitKind kind,
                                              const std::vector<std::string>& trace,
                                              const ProcessTreeApproximatedProgram& program)>
      solve;
  std::function<ProcessTreeApproximatedMoves(std::size_t node, const std::vector<std::string>& trace)> base;
  std::function<void(std::size_t node, const std::vector<std::string>& trace, std::size_t child)> choice;
  std::function<void(std::size_t node, const std::optional<ProcessTreeApproximatedMoves>& alignment)> returned;
};

struct ProcessTreeApproximatedOptions {
  std::size_t max_trace_length = 1;
  std::size_t max_tree_height = 1;
};

struct ProcessTreeApproximatedAlignment {
  ProcessTreeApproximatedMoves alignment;
  std::size_t cost = 0;
  double fitness = 0.0;
};

// original.apply on a log: one entry per trace, nullopt where the reference
// gives None (a leaf reached by a trace longer than max_trace_length while
// max_process_tree_height is 0). Throws ProcessTreeApproximatedReferenceError
// where the reference raises (an operator with one child, or other than the
// four it supports).
[[nodiscard]] std::vector<std::optional<ProcessTreeApproximatedAlignment>> reference_approximated_alignments(
    const ProcessTree& tree, const std::vector<std::vector<std::string>>& traces,
    const ProcessTreeApproximatedOptions& options = {}, const ProcessTreeApproximatedOracle& oracle = {});

}  // namespace minera
