#pragma once

// algo.conformance.alignments.process_tree.variants.milp of the PM4Py 2.7.22
// reference: ProcessTreeAligner's flow graph, the mixed-integer program its
// align() hands pm4py.util.lp.solver, the alignment it reads from a solution,
// and apply.
//
// The flow graph (networkx MultiDiGraph order: nodes by first insertion,
// edges per node by neighbour first insertion, then key) starts with a source
// 0 and a sink 1 and builds every subtree between a start and an end node at
// an "inverse capacity" iac (1 at the root):
// - a leaf is an edge start -> end labelled with its activity (None for tau),
//   capacity 1/iac, cost iac for a visible leaf and 0 for tau;
// - a sequence allocates k-1 middle node ids (added once an edge uses them)
//   and chains its children through them; a choice builds every child on
//   start -> end;
// - a parallel operator with k children first adds start and end, then per
//   child allocates and adds a pair (cs, ce), adds tau edges start -> cs and
//   ce -> end of capacity 1/(iac*k) and builds the child on cs -> ce at
//   iac*k. Start and end record the tau edges as a "shuffle" group (a node's
//   first group also sets iac and is_split / is_join);
// - a loop allocates and adds (ls, le), adds tau edges start -> ls and
//   le -> end of capacity 1/iac, then builds the do child on ls -> le and the
//   redo child on le -> ls. A root loop builds do on source -> sink and redo
//   on sink -> source directly; with a tau do the sink is never allocated
//   (source and sink are node 0, which then also carries sink=True) and the
//   redo is built on 0 -> 0. A tau root leaves node 0 alone.
//
// The program for a trace of n events has, in this order: a model-move
// variable per (position 0..n, edge) with the edge's cost and capacity, a
// log-move variable per (position 0..n-1, node) with cost 1 and bound 1, a
// synchronous variable per (position p, edge labelled like event p) with cost
// 1 - edge cost and the edge's capacity, and a binary variable per (position,
// shuffle group). Equality rows are flow conservation per (position, node)
// (supply -1 at the source at position 0, demand 1 at the sink at position n,
// the source's -1 winning when source and sink coincide), then per shuffle
// group and position: the group's model-move flow equals the binary times
// 1/iac. An inequality row per event with two or more synchronous candidates
// bounds the sum of 1/capacity times their flows by 1. A program without
// variables has no rows.
//
// The reported cost is the objective of the solution read back and the
// alignment lists one move per event: synchronous with the label of the first
// candidate edge with positive flow, else a log move. Model moves never
// appear.

#include <array>
#include <cstddef>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "minera/process_tree.hpp"

namespace minera {

class ProcessTreeMilpReferenceError : public std::invalid_argument {
 public:
  using std::invalid_argument::invalid_argument;
};

struct ProcessTreeMilpGraphNode {
  std::size_t id = 0;
  bool source = false;
  bool sink = false;
  bool split = false;
  bool join = false;
  long long iac = 0;  // 0 when the node carries no shuffle group
  // Shuffle groups of (from, to, key) edges.
  std::vector<std::vector<std::array<std::size_t, 3>>> shuffle;
  bool operator==(const ProcessTreeMilpGraphNode&) const = default;
};

struct ProcessTreeMilpGraphEdge {
  std::size_t from = 0;
  std::size_t to = 0;
  std::optional<std::string> label;  // nullopt for tau
  double capacity = 1.0;
  long long cost = 0;
  bool operator==(const ProcessTreeMilpGraphEdge&) const = default;
};

struct ProcessTreeMilpGraph {
  std::vector<ProcessTreeMilpGraphNode> nodes;  // insertion order
  std::vector<ProcessTreeMilpGraphEdge> edges;  // networkx edge order
};

// ProcessTreeAligner(tree).graph. Throws ProcessTreeMilpReferenceError for
// operators other than sequence, choice, parallel and loop.
[[nodiscard]] ProcessTreeMilpGraph reference_milp_process_tree_graph(const ProcessTree& tree);

using ProcessTreeMilpRow = std::vector<std::pair<std::size_t, double>>;

struct ProcessTreeMilpProgram {
  std::vector<double> objective, lower, upper;
  std::vector<bool> integer;
  std::vector<ProcessTreeMilpRow> equalities;
  std::vector<double> equality_rhs;
  std::vector<ProcessTreeMilpRow> inequalities;  // row . x <= rhs
  std::vector<double> inequality_rhs;
};

[[nodiscard]] ProcessTreeMilpProgram reference_milp_alignment_program(const ProcessTreeMilpGraph& graph,
                                                                      const std::vector<std::string>& trace);

// An optimal solution, or nullopt when the program is infeasible or has no
// variables: best-first branch and bound over a bounded primal simplex, with
// every flow of a shuffle-group row tied to its share of the binary (valid in
// integer solutions, and it keeps the relaxations tight).
[[nodiscard]] std::optional<std::vector<double>> solve_reference_milp_program(const ProcessTreeMilpProgram& program);

struct ProcessTreeMilpMove {
  std::string log;
  std::optional<std::string> model;  // nullopt for a log move
  bool operator==(const ProcessTreeMilpMove&) const = default;
};

struct ProcessTreeMilpAlignment {
  std::optional<double> cost;  // nullopt when the reference reports None
  std::vector<ProcessTreeMilpMove> alignment;
  double fitness = 0.0;  // set by reference_milp_alignments only
};

// The (cost, alignment) align() reads from a solution of the program.
[[nodiscard]] ProcessTreeMilpAlignment reference_milp_alignment_from_solution(const ProcessTreeMilpGraph& graph,
                                                                              const std::vector<std::string>& trace,
                                                                              const ProcessTreeMilpProgram& program,
                                                                              const std::vector<double>& solution);

// ProcessTreeAligner.align with the native solver: the cost is the optimum
// (the reference solver reports it to about 1e-7) and ties between optimal
// solutions may sync other events. A program without variables (a tau root)
// gives no cost; an infeasible program (the empty trace on a root loop with a
// tau do) throws ProcessTreeMilpReferenceError, where the reference reports
// whatever its solver left behind.
// `cancelled` is polled before each trace; once it returns true the call
// throws OperationCancelled (minera/cancellation.hpp).
[[nodiscard]] ProcessTreeMilpAlignment reference_milp_align(const ProcessTree& tree,
                                                            const std::vector<std::string>& trace);

// milp.apply: each distinct trace is aligned once; the empty trace's cost,
// rounded as round(cost + 1e-14, 13), is the fitness floor and fitness =
// 1 - cost / (floor + len), or 0 when that is 0. Throws
// ProcessTreeMilpReferenceError where the empty trace has no cost (a tau root,
// where the reference raises a TypeError) or an infeasible program.
[[nodiscard]] std::vector<ProcessTreeMilpAlignment> reference_milp_alignments(
    const ProcessTree& tree, const std::vector<std::vector<std::string>>& traces,
    const std::function<bool()>& cancelled = {});

}  // namespace minera
