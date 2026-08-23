#include "minera/powl_conversion.hpp"

#include <algorithm>
#include <cstddef>
#include <map>
#include <optional>
#include <queue>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace minera {
namespace {

PowlNode tree_node_to_powl(const ProcessTreeNode& node,
                           const std::vector<std::string>& activities) {
  switch (node.op) {
    case ProcessTreeOperator::kActivity:
      if (node.activity >= activities.size()) {
        throw std::out_of_range("process-tree activity id is out of range");
      }
      return PowlNode::transition(activities[node.activity]);
    case ProcessTreeOperator::kTau:
      return PowlNode::silent();
    case ProcessTreeOperator::kSequence: {
      std::vector<PowlNode> children;
      children.reserve(node.children.size());
      for (const auto& child : node.children)
        children.push_back(tree_node_to_powl(child, activities));
      return PowlNode::sequence(std::move(children));
    }
    case ProcessTreeOperator::kParallel: {
      std::vector<PowlNode> children;
      children.reserve(node.children.size());
      for (const auto& child : node.children)
        children.push_back(tree_node_to_powl(child, activities));
      return PowlNode::concurrency(std::move(children));
    }
    case ProcessTreeOperator::kExclusive: {
      if (node.children.size() < 2) {
        throw std::invalid_argument(
            "process-tree exclusive operator requires at least two children");
      }
      std::vector<PowlNode> children;
      children.reserve(node.children.size());
      for (const auto& child : node.children)
        children.push_back(tree_node_to_powl(child, activities));
      return PowlNode::xor_node(std::move(children));
    }
    case ProcessTreeOperator::kLoop: {
      if (node.children.size() != 2) {
        throw std::invalid_argument(
            "process-tree loop operator requires exactly two children");
      }
      PowlNode body = tree_node_to_powl(node.children.front(), activities);
      return PowlNode::loop(std::move(body),
                            tree_node_to_powl(node.children[1], activities));
    }
  }
  throw std::invalid_argument("unknown process-tree operator");
}

class ProcessTreeBuilder {
 public:
  ProcessTree run(const PowlNode& node) {
    auto root = convert(node);
    return ProcessTree(std::move(activities_), std::move(root));
  }

 private:
  std::vector<std::string> activities_;
  std::map<std::string, std::size_t> activity_ids_;

  std::size_t activity_id(const std::string& label) {
    const auto found = activity_ids_.find(label);
    if (found != activity_ids_.end()) return found->second;
    const auto id = activities_.size();
    activity_ids_.emplace(label, id);
    activities_.push_back(label);
    return id;
  }

  static ProcessTreeNode sequence(std::vector<ProcessTreeNode> children) {
    if (children.empty()) return ProcessTreeNode::tau();
    if (children.size() == 1) return std::move(children.front());
    return {ProcessTreeOperator::kSequence, 0, std::move(children)};
  }

  static ProcessTreeNode parallel(std::vector<ProcessTreeNode> children) {
    if (children.empty()) return ProcessTreeNode::tau();
    if (children.size() == 1) return std::move(children.front());
    return {ProcessTreeOperator::kParallel, 0, std::move(children)};
  }

  ProcessTreeNode transition(const PowlNode& node) {
    const auto leaf = ProcessTreeNode::leaf(activity_id(node.label()));
    const auto& frequency = node.frequency();
    if (frequency.minimum == 1 && frequency.maximum &&
        *frequency.maximum == 1) {
      return leaf;
    }

    // A null maximum is the unbounded self-loop form. A silent redo allows
    // the body to repeat without introducing an observable event.
    const ProcessTreeNode repeated{
        ProcessTreeOperator::kLoop, 0, {leaf, ProcessTreeNode::tau()}};
    if (frequency.minimum == 0 && frequency.maximum &&
        *frequency.maximum == 1) {
      return {ProcessTreeOperator::kExclusive, 0,
              {ProcessTreeNode::tau(), leaf}};
    }
    if (frequency.minimum == 0) {
      return {ProcessTreeOperator::kExclusive, 0,
              {ProcessTreeNode::tau(), repeated}};
    }
    return repeated;
  }

  ProcessTreeNode partial_order(const PowlNode& node) {
    const auto& children = node.children();
    if (children.empty()) return ProcessTreeNode::tau();

    const std::size_t count = children.size();
    const auto reduction = node.transitive_reduction();
    std::vector<std::set<std::size_t>> neighbours(count);
    std::vector<std::vector<std::size_t>> successors(count);
    for (const auto [source, target] : reduction) {
      neighbours[source].insert(target);
      neighbours[target].insert(source);
      successors[source].push_back(target);
    }
    for (auto& next : successors) std::sort(next.begin(), next.end());

    std::vector<std::vector<std::size_t>> components;
    std::vector<bool> visited(count, false);
    for (std::size_t root = 0; root < count; ++root) {
      if (visited[root]) continue;
      std::vector<std::size_t> component{root};
      visited[root] = true;
      for (std::size_t position = 0; position < component.size(); ++position) {
        for (const auto candidate : neighbours[component[position]]) {
          if (visited[candidate]) continue;
          visited[candidate] = true;
          component.push_back(candidate);
        }
      }
      std::sort(component.begin(), component.end());
      components.push_back(std::move(component));
    }

    std::vector<ProcessTreeNode> component_trees;
    component_trees.reserve(components.size());
    for (const auto& component : components) {
      std::set<std::size_t> members(component.begin(), component.end());
      std::vector<std::size_t> local_indegree(count, 0);
      for (const auto source : component) {
        for (const auto target : successors[source]) {
          if (members.count(target) != 0) ++local_indegree[target];
        }
      }
      std::queue<std::size_t> ready;
      for (const auto index : component)
        if (local_indegree[index] == 0) ready.push(index);

      std::map<std::size_t, std::size_t> levels;
      while (!ready.empty()) {
        const auto current = ready.front();
        ready.pop();
        const auto current_level = levels[current];
        for (const auto target : successors[current]) {
          if (members.count(target) == 0) continue;
          if (!levels.contains(target)) levels[target] = current_level + 1;
          if (--local_indegree[target] == 0) ready.push(target);
        }
      }
      if (levels.size() != component.size())
        throw std::invalid_argument("POWL partial order contains a cycle");

      std::size_t max_level = 0;
      for (const auto [index, level] : levels) {
        (void)index;
        max_level = std::max(max_level, level);
      }
      std::vector<std::vector<ProcessTreeNode>> layers(max_level + 1);
      for (const auto index : component)
        layers[levels.at(index)].push_back(convert(children[index]));

      std::vector<ProcessTreeNode> sequence_layers;
      sequence_layers.reserve(layers.size());
      for (auto& layer : layers)
        sequence_layers.push_back(parallel(std::move(layer)));
      component_trees.push_back(sequence(std::move(sequence_layers)));
    }
    return parallel(std::move(component_trees));
  }

  ProcessTreeNode convert(const PowlNode& node) {
    switch (node.kind()) {
      case PowlNodeKind::kTransition:
        return transition(node);
      case PowlNodeKind::kSilent:
        return ProcessTreeNode::tau();
      case PowlNodeKind::kXor: {
        std::vector<ProcessTreeNode> children;
        children.reserve(node.children().size());
        for (const auto& child : node.children()) children.push_back(convert(child));
        return {ProcessTreeOperator::kExclusive, 0, std::move(children)};
      }
      case PowlNodeKind::kLoop: {
        if (node.children().size() != 2)
          throw std::invalid_argument("POWL loop requires exactly two children");
        return {ProcessTreeOperator::kLoop, 0,
                {convert(node.children()[0]), convert(node.children()[1])}};
      }
      case PowlNodeKind::kStrictPartialOrder:
        return partial_order(node);
    }
    throw std::invalid_argument("unknown POWL node kind");
  }
};

struct Fragment {
  std::size_t in = 0;
  std::size_t out = 0;
};

class PowlBuilder {
 public:
  explicit PowlBuilder(const PowlNode& root) : root_(root) {}

  PetriNet run() {
    collect(root_);
    net_.set_activities(labels_);
    const std::size_t source = net_.add_place();
    const std::size_t sink = net_.add_place();
    net_.set_source(source);
    net_.set_sink(sink);
    build(root_, source, sink);
    return std::move(net_);
  }

 private:
  const PowlNode& root_;
  PetriNet net_;
  std::vector<std::string> labels_;
  std::map<std::string, std::size_t> label_ids_;

  void collect(const PowlNode& node) {
    if (node.kind() == PowlNodeKind::kTransition) {
      if (label_ids_.find(node.label()) == label_ids_.end()) {
        const auto id = labels_.size();
        label_ids_.emplace(node.label(), id);
        labels_.push_back(node.label());
      }
    }
    for (const auto& child : node.children()) collect(child);
  }

  std::size_t activity_id(const std::string& label) const {
    return label_ids_.at(label);
  }

  void transition_frequency(const PowlNode& node, std::size_t in,
                            std::size_t out) {
    const auto f = node.frequency();
    const auto activity = activity_id(node.label());
    if (f.maximum && *f.maximum == 1) {
      net_.add_transition(activity, {in}, {out});
      if (f.minimum == 0) net_.add_transition(std::nullopt, {in}, {out});
      return;
    }
    // One visible firing followed by zero or more cycles. The control place
    // after the visible transition offers a silent exit or a silent return to
    // the input; this preserves exactly one token on a completed run.
    if (f.minimum == 0) net_.add_transition(std::nullopt, {in}, {out});
    const std::size_t control = net_.add_place();
    net_.add_transition(activity, {in}, {control});
    net_.add_transition(std::nullopt, {control}, {out});
    net_.add_transition(std::nullopt, {control}, {in});
  }

  void build(const PowlNode& node, std::size_t in, std::size_t out) {
    switch (node.kind()) {
      case PowlNodeKind::kSilent:
        net_.add_transition(std::nullopt, {in}, {out});
        return;
      case PowlNodeKind::kTransition:
        transition_frequency(node, in, out);
        return;
      case PowlNodeKind::kXor:
        for (const auto& child : node.children()) build(child, in, out);
        return;
      case PowlNodeKind::kLoop: {
        const std::size_t control = net_.add_place();
        build(node.children().at(0), in, control);
        net_.add_transition(std::nullopt, {control}, {out});
        const std::size_t redo_out = net_.add_place();
        build(node.children().at(1), control, redo_out);
        net_.add_transition(std::nullopt, {redo_out}, {in});
        return;
      }
      case PowlNodeKind::kStrictPartialOrder:
        build_partial_order(node, in, out);
        return;
    }
  }

  void build_partial_order(const PowlNode& node, std::size_t in,
                           std::size_t out) {
    const auto& children = node.children();
    if (children.empty()) {
      net_.add_transition(std::nullopt, {in}, {out});
      return;
    }
    const std::size_t n = children.size();
    std::vector<std::size_t> child_in(n), child_out(n), done(n);
    for (std::size_t i = 0; i < n; ++i) {
      child_in[i] = net_.add_place();
      child_out[i] = net_.add_place();
      build(children[i], child_in[i], child_out[i]);
    }
    const auto reduction = node.transitive_reduction();
    std::vector<std::vector<std::size_t>> deps(n);
    std::vector<std::vector<std::size_t>> successors(n);
    for (const auto [a, b] : reduction) {
      const std::size_t dep = net_.add_place();
      deps[b].push_back(dep);
      successors[a].push_back(dep);
    }

    std::vector<std::size_t> starts;
    std::vector<std::size_t> ends;
    for (std::size_t i = 0; i < n; ++i) {
      bool has_in = false;
      bool has_out = false;
      for (const auto [a, b] : reduction) {
        has_in = has_in || b == i;
        has_out = has_out || a == i;
      }
      if (!has_in) starts.push_back(i);
      if (!has_out) ends.push_back(i);
    }
    std::vector<std::size_t> initial_outputs;
    initial_outputs.reserve(starts.size());
    for (const auto i : starts) initial_outputs.push_back(child_in[i]);
    net_.add_transition(std::nullopt, {in}, std::move(initial_outputs));

    for (std::size_t i = 0; i < n; ++i) {
      if (!deps[i].empty()) {
        net_.add_transition(std::nullopt, deps[i], {child_in[i]});
      }
      // A completion marker is only needed for terminal children. Nonterminal
      // children expose their completion solely through dependency places.
      if (successors[i].empty()) done[i] = net_.add_place();
      std::vector<std::size_t> outputs = successors[i];
      if (successors[i].empty()) outputs.push_back(done[i]);
      net_.add_transition(std::nullopt, {child_out[i]}, std::move(outputs));
    }
    std::vector<std::size_t> join_inputs;
    join_inputs.reserve(ends.size());
    for (const auto i : ends) join_inputs.push_back(done[i]);
    net_.add_transition(std::nullopt, std::move(join_inputs), {out});
  }
};

}  // namespace

PowlNode to_powl(const ProcessTree& tree) {
  return tree_node_to_powl(tree.root(), tree.activities());
}

ProcessTree to_process_tree(const PowlNode& node) {
  return ProcessTreeBuilder().run(node);
}

PetriNet to_petri_net(const PowlNode& node) { return PowlBuilder(node).run(); }

}  // namespace minera
