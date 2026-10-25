#pragma once

// algo.conformance.alignments.process_tree.util of the PM4Py 2.7.22
// reference: search_graph_pt_replay_semantics (the vertex-state replay the
// SEARCH_GRAPH_PT variant searches over) and search_graph_pt_frequency_
// annotation.
//
// Nodes are numbered in preorder and a state is one character per node:
// 'E' enabled, 'O' open, 'C' closed, 'F' future. A path lists the
// (node, new state) assignments in the order the reference makes them.
//
// The reference compares process trees structurally (operator, label and the
// ordered children), so a node's position among its siblings is the first
// structurally equal sibling ("first equal sibling" below):
// - can_enable: the node is F, its parent O, and by the parent: parallel
//   always; choice when every child is F; sequence when its first equal
//   sibling is the first child or the child before that is C; loop when the
//   node equals the redo child and the do child is C. The root never can.
// - can_open: the node is E. can_close: a leaf that is O; a sequence or
//   parallel whose children are all C; a choice whose children are all C or
//   F with at least one C; a loop whose do is C and redo F.
// - enable_vertex sets E; under a choice every other child's subtree becomes
//   C, under a loop the redo child turns the do subtree F. open_vertex sets O
//   and the children F, except that a sequence or loop enables its first
//   child. close_vertex closes a loop's redo subtree first, then the node;
//   closing the redo of an open loop re-enables the do and sets the redo
//   subtree F. Subtree assignments skip nodes already in the target state
//   (transform_tree).
// - shortest_path_to_open/_enable/_close compose those moves: enabling an
//   open node closes it first; enabling a closed node resets the nearest
//   enclosing loop that can take it again (opening the loop when it is not
//   open, else closing the redo, or closing the do and running the redo);
//   then the parent is opened and earlier sequence siblings closed (by real
//   position), a loop's do closed before its redo (or an active redo closed
//   before the do), or a choice with an active sibling reset. Closing an
//   operator closes its children in order; a choice closes its active child,
//   or, with none closed, the first child whose own shortest closing opens
//   the fewest visible leaves.
//
// The replay covers the operators SEARCH_GRAPH_PT aligns: sequence, choice,
// parallel and binary loops. Other operators, and states no replay reaches,
// raise ProcessTreeSearchGraphReferenceError.

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "minera/process_tree.hpp"

namespace minera {

class ProcessTreeSearchGraphReferenceError : public std::invalid_argument {
 public:
  using std::invalid_argument::invalid_argument;
};

struct ProcessTreeReplayStep {
  std::size_t node = 0;
  char state = 'F';
  bool operator==(const ProcessTreeReplayStep&) const = default;
};

using ProcessTreeReplayPath = std::vector<ProcessTreeReplayStep>;

// A move's outcome: no path (and no state) when the reference returns
// (None, None).
struct ProcessTreeReplayResult {
  std::optional<ProcessTreeReplayPath> path;
  std::string state;
};

class ProcessTreeReplaySemantics {
 public:
  explicit ProcessTreeReplaySemantics(const ProcessTree& tree);

  [[nodiscard]] std::size_t size() const noexcept { return nodes_.size(); }
  // get_initial_state: the root E, every other node F.
  [[nodiscard]] std::string initial_state() const;

  [[nodiscard]] bool can_enable(std::size_t node, const std::string& state) const;
  [[nodiscard]] bool can_open(std::size_t node, const std::string& state) const;
  [[nodiscard]] bool can_close(std::size_t node, const std::string& state) const;

  [[nodiscard]] ProcessTreeReplayResult transform_tree(std::size_t node, char target,
                                                       const std::string& state) const;
  [[nodiscard]] ProcessTreeReplayResult enable_vertex(std::size_t node, const std::string& state) const;
  [[nodiscard]] ProcessTreeReplayResult open_vertex(std::size_t node, const std::string& state) const;
  [[nodiscard]] ProcessTreeReplayResult close_vertex(std::size_t node, const std::string& state) const;
  [[nodiscard]] ProcessTreeReplayResult shortest_path_to_open(std::size_t node,
                                                              const std::string& state) const;
  [[nodiscard]] ProcessTreeReplayResult shortest_path_to_enable(std::size_t node,
                                                                const std::string& state) const;
  [[nodiscard]] ProcessTreeReplayResult shortest_path_to_close(std::size_t node,
                                                               const std::string& state) const;

  static constexpr std::size_t kNoParent = static_cast<std::size_t>(-1);
  [[nodiscard]] std::size_t parent(std::size_t node) const { return nodes_.at(node).parent; }
  [[nodiscard]] const std::vector<std::size_t>& children(std::size_t node) const {
    return nodes_.at(node).children;
  }
  // Structural class of a node: equal ids iff the subtrees compare equal.
  [[nodiscard]] std::size_t structure(std::size_t node) const { return nodes_.at(node).structure; }

 private:
  struct Node {
    ProcessTreeOperator op = ProcessTreeOperator::kTau;
    std::size_t parent = kNoParent;
    std::vector<std::size_t> children;
    std::size_t structure = 0;
    std::size_t end = 0;  // one past the subtree's last preorder index
  };

  void check(std::size_t node, const std::string& state) const;
  [[nodiscard]] std::size_t first_equal_sibling(std::size_t node) const;
  void assign(std::string& state, ProcessTreeReplayPath& path, std::size_t node, char value) const;
  void assign_subtree(std::string& state, ProcessTreeReplayPath& path, std::size_t node, char value) const;
  [[nodiscard]] ProcessTreeReplayResult reset(std::size_t node, const std::string& state) const;

  std::vector<Node> nodes_;
};

// One search state of a SEARCH_GRAPH_PT result: its vertex states and the
// replay path that led to it from its parent.
struct ProcessTreeSearchSegment {
  std::string state;
  ProcessTreeReplayPath path;
};

struct ProcessTreeFrequency {
  long long num_events = 0;
  long long num_cases = 0;
};

// search_graph_pt_frequency_annotation.apply over the search-state chains
// (root first) of each alignment. The reference walks
// get_bottomup_nodes, which keeps one node per structural class: leaves in
// breadth-first order (first of each class), then repeatedly the parent of a
// kept node when no kept node equals it and every child equals a kept node.
// Each kept node gets, over every structurally equal node, the ENABLED
// assignments in the chains' paths (OPEN for the root), and the number of
// alignments with at least one; the other nodes get no properties
// (nullopt).
[[nodiscard]] std::vector<std::optional<ProcessTreeFrequency>> reference_search_graph_frequency_annotation(
    const ProcessTree& tree, const std::vector<std::vector<ProcessTreeSearchSegment>>& alignments);

}  // namespace minera
