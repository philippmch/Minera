#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "minera/event_log.hpp"
#include "minera/petri_net.hpp"

namespace minera {

// A binary split test on one attribute. A row satisfies an equality split when
// it carries the key with exactly `value`, and a threshold split when it
// carries the key with a numeric value <= `threshold`; a row missing the key
// always falls to the else side.
struct DecisionSplit {
  std::string key;
  bool numeric = false;
  std::string value;       // equality splits
  double threshold = 0.0;  // threshold splits
};

// One node of a mined decision tree, stored in a flat vector; children are
// indices into it. A leaf has no children; its prediction is the branch with
// the largest count.
struct DecisionNode {
  std::vector<std::uint64_t> branch_counts;  // observations per branch here
  DecisionSplit split;   // meaningful only on internal nodes
  int then_child = -1;   // rows satisfying the split; -1 on a leaf
  int else_child = -1;   // rows failing it (or missing the key); -1 on a leaf
};

// One decision point of the workflow net: a place with several outgoing
// transitions, so replay must choose which one consumes its token. `branches`
// names the alternatives — a visible transition by its activity, a silent one
// by the visible activities it silently leads to. `nodes[0]` is the root of
// the decision tree mined over the attributes known before the choice; a
// single root node means no attribute split improved on always guessing the
// majority branch.
struct DecisionPoint {
  std::size_t place = 0;
  std::vector<std::string> branches;
  std::uint64_t observations = 0;
  std::vector<DecisionNode> nodes;
  double accuracy = 0.0;        // share of observations the tree gets right
  double majority_share = 0.0;  // baseline: always guessing the top branch
};

// Bounds and feature-scope controls for the deterministic decision tree.
// Empty attributes keeps every context key; otherwise only the named keys
// participate in split selection. The context still records all values so a
// selected key may arrive at any point before a later decision.
struct DecisionMiningOptions {
  std::size_t max_depth = 4;
  std::size_t min_split = 10;
  std::size_t min_leaf = 5;
  std::size_t max_categories = 16;
  std::size_t max_thresholds = 32;
  double min_gain = 1e-9;
  bool include_trace_attributes = true;
  std::vector<std::string> attributes;
};

// Decision mining over a log and a model (Rozinat & van der Aalst): where the
// net offers alternatives, learn from the event and case attributes which
// branch a case takes.
struct DecisionMiningResult {
  // The UI registry id of the model the decisions belong to ("inductive",
  // "alpha", …). mine_decisions leaves it empty; callers that serialize the
  // result set it, so a stored export knows which model it describes.
  std::string miner;
  std::uint64_t observed_traces = 0;  // traces whose clean replay contributed
  std::uint64_t trace_count = 0;
  std::vector<DecisionPoint> decision_points;  // only points observed at all
};

// Mines the decision points of `net` from the traces of `log` that replay
// cleanly (deviating traces are skipped: a forced replay's choices are not
// trustworthy). Each choice is observed together with the attributes in scope
// at that moment — the case attributes plus the latest value of every event
// attribute (and the performing resource, under "resource") seen strictly
// before the event that triggered the choice — and a small CART-style tree
// (Gini impurity, binary splits, missing values falling to the else side) is
// grown per decision point. On a model discovered from the log itself (the
// Inductive Miner guarantees a perfect fit) every trace contributes.
[[nodiscard]] DecisionMiningResult mine_decisions(const EventLog& log,
                                                  const PetriNet& net,
                                                  const DecisionMiningOptions& options = {});

}  // namespace minera
