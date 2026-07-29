#include "minera/process_tree_reduction.hpp"

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
         op == ProcessTreeOperator::kExclusive;
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
    case ProcessTreeOperator::kParallel:
      if (node.children.empty()) {
        throw std::invalid_argument(
            "process tree sequence, exclusive, and parallel operators require "
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
    case ProcessTreeOperator::kParallel: {
      result.skippable = true;
      for (const auto& child : result.children) {
        result.skippable = result.skippable && child.skippable;
      }
      break;
    }
    case ProcessTreeOperator::kExclusive:
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

    if (node.op == ProcessTreeOperator::kExclusive) {
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
        if (attribute.key == options.activity_key) {
          selected = &attribute;
          break;
        }
      }
      if (selected == nullptr) {
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
  if (options.variant != ProcessTreeReductionVariant::kTreeTrBased) {
    throw std::invalid_argument("unsupported process tree reduction variant");
  }

  const auto metadata = inspect(tree.root(), tree.activities());
  const auto observed = trace_activities(tree, trace, options);
  auto reduced = reduce_node(tree.root(), metadata, observed, true);
  reduced = canonicalize(std::move(reduced));
  return ProcessTree(tree.activities(), std::move(reduced));
}

}  // namespace minera
