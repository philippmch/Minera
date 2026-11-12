#include "minera/process_tree_reduction.hpp"
#include "reference_activity_selector.hpp"

#include <algorithm>
#include <set>
#include <stdexcept>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace minera {
namespace {

using ActivityNames = std::unordered_set<std::string>;

struct Metadata {
  ActivityNames activities;
  bool skippable = false;
  std::vector<Metadata> children;
};

bool is_flattenable(ProcessTreeOperator op) {
  return op == ProcessTreeOperator::kSequence ||
         op == ProcessTreeOperator::kParallel ||
         op == ProcessTreeOperator::kInterleaving ||
         op == ProcessTreeOperator::kExclusive ||
         op == ProcessTreeOperator::kInclusiveChoice;
}

void add_activities(ActivityNames& target, const Metadata& source) {
  target.insert(source.activities.begin(), source.activities.end());
}

Metadata inspect(const ProcessTreeNode& node,
                 const std::vector<std::string>& activities) {
  switch (node.op) {
    case ProcessTreeOperator::kActivity:
    case ProcessTreeOperator::kTau:
      if (!node.children.empty()) {
        throw std::invalid_argument(
            "process tree activity and tau nodes must not have children");
      }
      break;
    case ProcessTreeOperator::kSequence:
    case ProcessTreeOperator::kExclusive:
    case ProcessTreeOperator::kInclusiveChoice:
    case ProcessTreeOperator::kParallel:
    case ProcessTreeOperator::kInterleaving:
      if (node.children.empty()) {
        throw std::invalid_argument(
            "process tree sequence, exclusive, inclusive, parallel, and interleaving operators require "
            "at least one child");
      }
      break;
    case ProcessTreeOperator::kLoop:
      // Minera permits a body-only loop and n-ary loops: child 0 is the body,
      // and every later child is an alternative redo path.
      if (node.children.empty()) {
        throw std::invalid_argument(
            "process tree loop requires at least one body child");
      }
      break;
    default:
      throw std::invalid_argument("process tree contains an invalid operator");
  }

  Metadata result;
  result.children.reserve(node.children.size());
  for (const auto& child : node.children) {
    result.children.push_back(inspect(child, activities));
    add_activities(result.activities, result.children.back());
  }

  switch (node.op) {
    case ProcessTreeOperator::kTau:
      result.skippable = true;
      break;
    case ProcessTreeOperator::kActivity:
      if (node.activity >= activities.size()) {
        throw std::invalid_argument(
            "process tree activity index is out of range");
      }
      result.activities.insert(activities[node.activity]);
      result.skippable = false;
      break;
    case ProcessTreeOperator::kSequence:
    case ProcessTreeOperator::kParallel:
    case ProcessTreeOperator::kInterleaving: {
      result.skippable = true;
      for (const auto& child : result.children) {
        result.skippable = result.skippable && child.skippable;
      }
      break;
    }
    case ProcessTreeOperator::kExclusive:
    case ProcessTreeOperator::kInclusiveChoice:
      result.skippable = false;
      for (const auto& child : result.children) {
        result.skippable = result.skippable || child.skippable;
      }
      break;
    case ProcessTreeOperator::kLoop:
      // Minera represents a loop as body followed by zero or more redo
      // paths.  A loop can be empty exactly when its body can be empty.
      result.skippable = result.children.empty() ||
                         result.children.front().skippable;
      break;
  }
  return result;
}

bool disjoint(const Metadata& metadata, const ActivityNames& trace_activities) {
  for (const auto& activity : metadata.activities) {
    if (trace_activities.find(activity) != trace_activities.end()) return false;
  }
  return true;
}

ProcessTreeNode reduce_node(const ProcessTreeNode& node, const Metadata& metadata,
                            const ActivityNames& trace_activities, bool is_root) {
  ProcessTreeNode reduced = node;
  reduced.children.clear();
  reduced.children.reserve(node.children.size());
  for (std::size_t i = 0; i < node.children.size(); ++i) {
    reduced.children.push_back(reduce_node(node.children[i], metadata.children[i],
                                           trace_activities, false));
  }
  // Children are visited first to mirror the bottom-up node order. The
  // metadata remains that of the original (unmodified) subtree, as in the
  // footprint-based implementation.
  if (!is_root && metadata.skippable && disjoint(metadata, trace_activities)) {
    return ProcessTreeNode::tau();
  }
  return reduced;
}

ProcessTreeNode canonicalize(ProcessTreeNode node) {
  for (auto& child : node.children) child = canonicalize(std::move(child));

  if (is_flattenable(node.op)) {
    std::vector<ProcessTreeNode> flattened;
    flattened.reserve(node.children.size());
    for (auto& child : node.children) {
      if (child.op == node.op) {
        for (auto& grandchild : child.children) {
          flattened.push_back(std::move(grandchild));
        }
      } else {
        flattened.push_back(std::move(child));
      }
    }
    node.children = std::move(flattened);

    if (node.op == ProcessTreeOperator::kExclusive ||
        node.op == ProcessTreeOperator::kInclusiveChoice) {
      std::vector<ProcessTreeNode> deduplicated;
      deduplicated.reserve(node.children.size());
      bool saw_tau = false;
      for (auto& child : node.children) {
        if (child.op == ProcessTreeOperator::kTau) {
          if (saw_tau) continue;
          saw_tau = true;
        }
        deduplicated.push_back(std::move(child));
      }
      node.children = std::move(deduplicated);
    } else {
      std::vector<ProcessTreeNode> non_tau;
      non_tau.reserve(node.children.size());
      bool saw_tau = false;
      for (auto& child : node.children) {
        if (child.op == ProcessTreeOperator::kTau) {
          saw_tau = true;
        } else {
          non_tau.push_back(std::move(child));
        }
      }
      if (!non_tau.empty()) {
        node.children = std::move(non_tau);
      } else {
        node.children.clear();
        if (saw_tau) node.children.push_back(ProcessTreeNode::tau());
      }
    }

    if (node.children.size() == 1) return std::move(node.children.front());
    if (node.children.empty()) return ProcessTreeNode::tau();
    return node;
  }

  if ((node.op == ProcessTreeOperator::kLoop) && node.children.size() == 1) {
    return std::move(node.children.front());
  }
  return node;
}

ActivityNames trace_activities(const ProcessTree& tree, const Trace& trace,
                               const ProcessTreeReductionOptions& options) {
  ActivityNames known;
  known.reserve(tree.activities().size());
  for (const auto& activity : tree.activities()) known.insert(activity);

  ActivityNames result;
  const bool native_activity = options.activity_key.empty() ||
                               options.activity_key == "concept:name";
  for (const auto& event : trace.events) {
    std::string_view value;
    if (native_activity) {
      value = event.activity;
    } else {
      const EventAttribute* selected = nullptr;
      for (const auto& attribute : event.attributes) {
        if (!attribute.is_null && attribute.key == options.activity_key) {
          selected = &attribute;
          break;
        }
      }
      if (selected == nullptr) {
        // A selected column's own header (its source key, a CSV header or
        // the key the log was prepared under) reads that projection, a
        // string, as pm4py reads the column (round BQ).
        if (const auto* projected = reference_selected_field(event, options.activity_key)) {
          if (known.find(*projected) != known.end()) result.emplace(*projected);
          continue;
        }
        throw std::invalid_argument("event is missing activity attribute '" +
                                    options.activity_key + "'");
      }
      if (selected->kind != AttributeKind::kString) {
        throw std::invalid_argument("activity attribute '" +
                                    options.activity_key +
                                    "' must be a string");
      }
      value = selected->value;
    }
    if (known.find(std::string(value)) != known.end()) result.emplace(value);
  }
  return result;
}

}  // namespace

ProcessTree reduce_process_tree(const ProcessTree& tree, const Trace& trace,
                                ProcessTreeReductionOptions options) {
  return reference_reduce_process_tree(tree, trace, options);
}


namespace {

// A mutable copy of the tree with parent links, as the reference walks it.
struct WalkNode {
  ProcessTreeOperator op = ProcessTreeOperator::kTau;
  std::size_t activity = 0;
  std::vector<std::size_t> children;
  std::optional<std::size_t> parent;
  std::string key;  // structural identity
};

std::size_t copy_into(std::vector<WalkNode>& nodes, const ProcessTreeNode& node,
                      std::optional<std::size_t> parent, const std::vector<std::string>& activities) {
  const auto index = nodes.size();
  nodes.push_back({node.op, node.activity, {}, parent, {}});
  std::string key;
  if (node.op == ProcessTreeOperator::kTau) key = "T";
  else if (node.op == ProcessTreeOperator::kActivity) key = "L:" + activities.at(node.activity);
  else key = std::to_string(static_cast<int>(node.op)) + "(";
  for (std::size_t i = 0; i < node.children.size(); ++i) {
    const auto child = copy_into(nodes, node.children[i], index, activities);
    nodes[index].children.push_back(child);
    key += (i ? "," : "") + nodes[child].key;
  }
  if (!node.children.empty() || (node.op != ProcessTreeOperator::kTau && node.op != ProcessTreeOperator::kActivity))
    key += ")";
  nodes[index].key = std::move(key);
  return index;
}

bool is_leaf(const WalkNode& node) {
  return node.op == ProcessTreeOperator::kTau || node.op == ProcessTreeOperator::kActivity;
}

std::vector<std::size_t> breadth_first(const std::vector<WalkNode>& nodes) {
  std::vector<std::size_t> order{0};
  for (std::size_t at = 0; at < order.size(); ++at)
    for (const auto child : nodes[order[at]].children) order.push_back(child);
  return order;
}

ProcessTreeReductionLeaf leaf_of(const WalkNode& node, const std::vector<std::string>& activities) {
  if (node.op == ProcessTreeOperator::kTau) return std::nullopt;
  return activities.at(node.activity);
}

struct Footprint {
  bool skippable = false;
  std::set<std::string> activities;
  // The subtree holds an interleaving node, which the reference footprints
  // cannot describe.
  bool interleaving = false;
};

Footprint footprint(const std::vector<WalkNode>& nodes, std::size_t index,
                    const std::vector<std::string>& activities) {
  const auto& node = nodes[index];
  Footprint result;
  if (node.op == ProcessTreeOperator::kTau) {
    result.skippable = true;
    return result;
  }
  if (node.op == ProcessTreeOperator::kActivity) {
    result.activities.insert(activities.at(node.activity));
    return result;
  }
  if (node.op == ProcessTreeOperator::kLoop && node.children.size() < 2)
    throw ProcessTreeReductionError("IndexError", "list index out of range");
  result.interleaving = node.op == ProcessTreeOperator::kInterleaving;
  std::vector<Footprint> children;
  for (const auto child : node.children) {
    children.push_back(footprint(nodes, child, activities));
    result.activities.insert(children.back().activities.begin(), children.back().activities.end());
    result.interleaving = result.interleaving || children.back().interleaving;
  }
  if (node.op == ProcessTreeOperator::kExclusive) {
    result.skippable = std::any_of(children.begin(), children.end(), [](const Footprint& f) { return f.skippable; });
  } else if (node.op == ProcessTreeOperator::kLoop) {
    result.skippable = children.front().skippable;
  } else {
    // Sequence, parallel and (as the reference's footprints treat it) or.
    result.skippable = std::all_of(children.begin(), children.end(), [](const Footprint& f) { return f.skippable; });
  }
  return result;
}

ProcessTreeNode materialize(const std::vector<WalkNode>& nodes, std::size_t index) {
  ProcessTreeNode result{nodes[index].op, nodes[index].activity, {}};
  for (const auto child : nodes[index].children) result.children.push_back(materialize(nodes, child));
  return result;
}

}  // namespace

std::vector<ProcessTreeReductionLeaf> process_tree_reduction_leaves(const ProcessTree& tree) {
  std::vector<WalkNode> nodes;
  copy_into(nodes, tree.root(), std::nullopt, tree.activities());
  std::vector<ProcessTreeReductionLeaf> leaves;
  for (const auto index : breadth_first(nodes)) {
    if (!is_leaf(nodes[index])) continue;
    const auto leaf = leaf_of(nodes[index], tree.activities());
    if (std::find(leaves.begin(), leaves.end(), leaf) == leaves.end()) leaves.push_back(leaf);
  }
  return leaves;
}

namespace {

// The reference node equality: two childless nodes are equal when their
// labels are (so tau, an empty operator and another tau all compare equal);
// otherwise the operator and the ordered children must match.
bool reference_equal(const ProcessTreeNode& a, const ProcessTreeNode& b) {
  if (a.children.empty() && b.children.empty()) {
    const bool a_label = a.op == ProcessTreeOperator::kActivity;
    const bool b_label = b.op == ProcessTreeOperator::kActivity;
    return a_label == b_label && (!a_label || a.activity == b.activity);
  }
  if (a.op != b.op || a.children.size() != b.children.size()) return false;
  for (std::size_t i = 0; i < a.children.size(); ++i)
    if (!reference_equal(a.children[i], b.children[i])) return false;
  return true;
}

std::size_t reference_index(const std::vector<ProcessTreeNode>& children, const ProcessTreeNode& value) {
  for (std::size_t i = 0; i < children.size(); ++i)
    if (reference_equal(children[i], value)) return i;
  throw std::logic_error("process tree fold: value not in children");
}

// list.remove: drops the first child equal to `value` under reference_equal.
void reference_remove(std::vector<ProcessTreeNode>& children, const ProcessTreeNode& value) {
  const ProcessTreeNode copy = value;
  children.erase(children.begin() + static_cast<std::ptrdiff_t>(reference_index(children, copy)));
}

bool is_real_tau(const ProcessTreeNode& node) { return node.op == ProcessTreeOperator::kTau; }

bool reduces_taus(ProcessTreeOperator op) {
  return op == ProcessTreeOperator::kSequence || op == ProcessTreeOperator::kParallel ||
         op == ProcessTreeOperator::kExclusive || op == ProcessTreeOperator::kInclusiveChoice;
}

std::size_t tau_count(const std::vector<ProcessTreeNode>& children) {
  return static_cast<std::size_t>(std::count_if(children.begin(), children.end(), is_real_tau));
}

// The tau reduction applied to a node before its children fold: when every
// child is tau one is kept; otherwise sequence and parallel remove one equal
// child per tau and xor and or one per tau but the first. Removal is by
// equality, so an empty operator or another tau ahead of a tau can go first.
void reduce_taus_before_fold(ProcessTreeNode& node) {
  if (!reduces_taus(node.op)) return;
  auto& children = node.children;
  const auto taus = tau_count(children);
  if (taus == 0) return;
  if (taus == children.size()) {
    children.resize(1);
    return;
  }
  const bool drop_all = node.op == ProcessTreeOperator::kSequence || node.op == ProcessTreeOperator::kParallel;
  for (std::size_t i = drop_all ? 0 : 1; i < taus; ++i) reference_remove(children, ProcessTreeNode::tau());
}

// The tau reduction applied after a node's children fold and flatten: when
// every child is tau the last is kept; sequence and parallel drop their tau
// children, and xor and or remove equal children until one tau is left.
void reduce_taus_after_fold(ProcessTreeNode& node) {
  for (auto& child : node.children) reduce_taus_after_fold(child);
  if (!reduces_taus(node.op)) return;
  auto& children = node.children;
  if (tau_count(children) == 0) return;
  if (tau_count(children) == children.size()) {
    children.erase(children.begin(), children.end() - 1);
    return;
  }
  if (node.op == ProcessTreeOperator::kSequence || node.op == ProcessTreeOperator::kParallel) {
    children.erase(std::remove_if(children.begin(), children.end(), is_real_tau), children.end());
  } else {
    while (tau_count(children) > 1) reference_remove(children, ProcessTreeNode::tau());
  }
}

}  // namespace

ProcessTreeNode fold_process_tree_reference(ProcessTreeNode node) {
  reduce_taus_before_fold(node);
  if (!node.children.empty()) {
    for (auto& child : node.children) child = fold_process_tree_reference(std::move(child));
    if (node.children.size() == 1) return std::move(node.children.front());
    if (node.op == ProcessTreeOperator::kSequence || node.op == ProcessTreeOperator::kExclusive ||
        node.op == ProcessTreeOperator::kParallel) {
      auto& children = node.children;
      while (true) {
        const auto same = std::find_if(children.begin(), children.end(),
                                       [&](const ProcessTreeNode& child) { return child.op == node.op; });
        if (same == children.end()) break;
        const ProcessTreeNode absorbed = *same;
        const auto at = reference_index(children, absorbed);
        children.insert(children.begin() + static_cast<std::ptrdiff_t>(at), absorbed.children.begin(),
                        absorbed.children.end());
        reference_remove(children, absorbed);
      }
    }
    reduce_taus_after_fold(node);
    if (node.children.size() == 1) return std::move(node.children.front());
  }
  if (node.op == ProcessTreeOperator::kLoop && node.children.size() == 2 &&
      std::all_of(node.children.begin(), node.children.end(), is_real_tau))
    return ProcessTreeNode::tau();
  return node;
}

ProcessTree reference_reduce_process_tree(const ProcessTree& tree, const Trace& trace,
                                          const ProcessTreeReductionOptions& options,
                                          const std::vector<ProcessTreeReductionLeaf>* leaf_order) {
  if (options.variant != ProcessTreeReductionVariant::kTreeTrBased)
    throw std::invalid_argument("unsupported process tree reduction variant");
  (void)inspect(tree.root(), tree.activities());
  const auto& activities = tree.activities();
  std::vector<WalkNode> nodes;
  copy_into(nodes, tree.root(), std::nullopt, activities);
  const auto leaves = leaf_order ? *leaf_order : process_tree_reduction_leaves(tree);

  // Distinct leaves, each its first breadth-first instance, in leaf order.
  const auto order = breadth_first(nodes);
  std::vector<std::size_t> walk;
  std::set<std::string> present;
  for (const auto& leaf : leaves) {
    for (const auto index : order) {
      if (is_leaf(nodes[index]) && leaf_of(nodes[index], activities) == leaf) {
        walk.push_back(index);
        present.insert(nodes[index].key);
        break;
      }
    }
  }
  for (std::size_t at = 0; at < walk.size(); ++at) {
    const auto parent = nodes[walk[at]].parent;
    if (!parent || present.contains(nodes[*parent].key)) continue;
    const auto& children = nodes[*parent].children;
    if (std::all_of(children.begin(), children.end(),
                    [&](std::size_t child) { return present.contains(nodes[child].key); })) {
      walk.push_back(*parent);
      present.insert(nodes[*parent].key);
    }
  }

  std::vector<Footprint> footprints;
  for (const auto index : walk) footprints.push_back(footprint(nodes, index, activities));
  // The reference uses the footprint of every visited node but the last (the
  // result) and the root, and fails with TypeError when that footprint covers
  // an interleaving node. Replacing either of those cannot change the result.
  const auto replaceable = [&](std::size_t at) { return at + 1 < walk.size() && nodes[walk[at]].parent; };
  for (std::size_t at = 0; at < walk.size(); ++at)
    if (replaceable(at) && footprints[at].interleaving)
      throw ProcessTreeReductionError(
          "TypeError", "process tree reduction does not support interleaving nodes (the reference footprints have none)");
  const auto observed = trace_activities(tree, trace, options);
  for (std::size_t at = 0; at < walk.size(); ++at) {
    const auto index = walk[at];
    const auto& fp = footprints[at];
    const bool disjoint_trace = std::none_of(fp.activities.begin(), fp.activities.end(),
                                             [&](const std::string& a) { return observed.contains(a); });
    const auto parent = nodes[index].parent;
    if (!replaceable(at) || !fp.skippable || !disjoint_trace) continue;
    const auto& siblings = nodes[*parent].children;
    const auto position = static_cast<std::size_t>(std::find(siblings.begin(), siblings.end(), index) - siblings.begin());
    const auto tau = nodes.size();
    nodes.push_back({ProcessTreeOperator::kTau, 0, {}, *parent, "T"});
    nodes[*parent].children[position] = tau;
  }
  return ProcessTree(activities, fold_process_tree_reference(materialize(nodes, walk.back())));
}

}  // namespace minera
