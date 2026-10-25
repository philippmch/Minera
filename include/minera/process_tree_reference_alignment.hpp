#pragma once

// algo.conformance.alignments.process_tree DYNAMIC_PROGRAMMING of the PM4Py
// 2.7.22 reference, alignments included.
//
// The reference first nests n-ary sequence, choice and parallel operators to
// the right (->(a, b, c) is ->(a, ->(b, c))), then aligns each trace segment
// per node with unit log and model move costs, caching per (node, segment):
// - a leaf on a segment syncs the first occurrence of its label (log moves
//   around it) or logs every event and adds a model move; tau logs every
//   event;
// - a choice takes its first cheapest child;
// - a binary sequence tries every split, keeping the cheapest and, among
//   equal totals, the cheaper left part (first such split);
// - a binary parallel aligns each child on the trace projected to the
//   child's labels (an event may go to both), adds a log move per event in
//   neither, and lists one move per event: the event's first child's next
//   move (other moves, model moves included, are left out);
// - a loop on an empty segment costs nothing; otherwise it is a dynamic
//   program over the segment's positions with states after a body (D) and
//   after a redo (R): D[j] is the body on [0, j) or R[i] plus the body on
//   [i, j); R[j] is a redo on [0, j) or D[i] plus a redo on [i, j) (i >= 1);
//   the result is the first cheapest, by i, of the body on everything, R[i]
//   plus the body on [i, n), and D[i] plus a second body on [i, n). A redo
//   is the first cheapest redo child.
// fitness = 1 - cost / (length + empty-trace cost), 0 when that is 0/0.
//
// Remaining divergences (about 0.2% of fuzzed traces): the reference
// sometimes returns an alignment of another segment from its caches (its log
// moves then do not spell the trace), and in rare loop ties it prefers a
// different decomposition of equal cost.

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "minera/process_tree.hpp"

namespace minera {

// Raised for operators the reference variant does not implement (inclusive
// choice and interleaving: NotImplementedError).
class ProcessTreeReferenceAlignmentError : public std::invalid_argument {
 public:
  using std::invalid_argument::invalid_argument;
};

struct ProcessTreeReferenceMove {
  std::string log;    // ">>" for a model move
  std::string model;  // ">>" for a log move
};

struct ProcessTreeReferenceAlignment {
  std::vector<ProcessTreeReferenceMove> moves;
  double cost = 0.0;
  double fitness = 0.0;
};

[[nodiscard]] std::vector<ProcessTreeReferenceAlignment> reference_process_tree_dp_alignments(
    const ProcessTree& tree, const std::vector<std::vector<std::string>>& traces);

}  // namespace minera
