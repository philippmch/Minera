#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace minera {

// Block-structured process model produced by the inductive miner. Operator
// nodes combine their children; leaf nodes are either a concrete activity or a
// silent (tau) step that consumes no event.
enum class ProcessTreeOperator {
  kActivity,   // leaf: the activity at `activity`
  kTau,        // leaf: silent step (no observable event)
  kSequence,   // children executed left to right
  kExclusive,  // exactly one child executed
  kInclusiveChoice,  // one or more children executed concurrently
  kParallel,   // children executed concurrently (any interleaving)
  kLoop,       // child[0] is the body; child[1..] are redo paths
};

struct ProcessTreeNode {
  ProcessTreeOperator op = ProcessTreeOperator::kTau;
  std::size_t activity = 0;  // valid iff op == kActivity; indexes activities()
  std::vector<ProcessTreeNode> children;

  [[nodiscard]] static ProcessTreeNode tau() {
    return ProcessTreeNode{ProcessTreeOperator::kTau, 0, {}};
  }

  [[nodiscard]] static ProcessTreeNode leaf(std::size_t activity) {
    return ProcessTreeNode{ProcessTreeOperator::kActivity, activity, {}};
  }
};

class ProcessTree {
 public:
  ProcessTree() = default;
  ProcessTree(std::vector<std::string> activities, ProcessTreeNode root)
      : activities_(std::move(activities)), root_(std::move(root)) {}

  // Activity names indexed by ProcessTreeNode::activity.
  [[nodiscard]] const std::vector<std::string>& activities() const noexcept {
    return activities_;
  }

  [[nodiscard]] const ProcessTreeNode& root() const noexcept { return root_; }

 private:
  std::vector<std::string> activities_;
  ProcessTreeNode root_;
};

}  // namespace minera
