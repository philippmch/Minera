#include "minera/powl_conversion.hpp"

#include "minera/conformance.hpp"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <iterator>
#include <map>
#include <numeric>
#include <optional>
#include <queue>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace minera {
namespace {

// The label of a reference FrequentTransition: "<label>\n[1,<max or ->]"
// when it is optional (minimum 0) or self-looping (unbounded maximum).
std::string reference_frequency_label(const PowlNode& node) {
  const auto& f = node.frequency();
  const bool skippable = f.minimum == 0;
  const bool selfloop = !f.maximum.has_value();
  if (!skippable && !selfloop) return node.label();
  return node.label() + "\n[1," + (selfloop ? std::string("-") : std::to_string(*f.maximum)) + "]";
}

constexpr std::size_t kMaxInclusiveChildren = 12;
constexpr std::size_t kMaxInterleavingChildren = 8;

PowlNode inclusive_to_powl(const ProcessTreeNode& node,
                           const std::vector<std::string>& activities);

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
    case ProcessTreeOperator::kInterleaving: {
      if (node.children.empty()) return PowlNode::silent();
      if (node.children.size() > kMaxInterleavingChildren)
        throw std::invalid_argument(
            "process-tree interleaving exceeds the POWL expansion limit of 8 children");
      std::vector<std::size_t> order(node.children.size());
      std::iota(order.begin(), order.end(), 0);
      std::vector<PowlNode> alternatives;
      do {
        std::vector<PowlNode> sequence;
        sequence.reserve(order.size());
        for (const auto index : order)
          sequence.push_back(tree_node_to_powl(node.children[index], activities));
        alternatives.push_back(PowlNode::sequence(std::move(sequence)));
      } while (std::next_permutation(order.begin(), order.end()));
      return alternatives.size() == 1
                 ? std::move(alternatives.front())
                 : PowlNode::xor_node(std::move(alternatives));
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
    case ProcessTreeOperator::kInclusiveChoice:
      return inclusive_to_powl(node, activities);
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

PowlNode inclusive_to_powl(const ProcessTreeNode& node,
                           const std::vector<std::string>& activities) {
  if (node.children.empty()) {
    throw std::invalid_argument(
        "process-tree inclusive OR requires at least one child");
  }
  if (node.children.size() > kMaxInclusiveChildren) {
    throw std::invalid_argument(
        "process-tree inclusive OR exceeds the POWL expansion limit of 12 children");
  }

  std::vector<PowlNode> converted;
  converted.reserve(node.children.size());
  for (const auto& child : node.children)
    converted.push_back(tree_node_to_powl(child, activities));

  std::vector<PowlNode> alternatives;
  std::vector<std::size_t> selected;
  const auto visit = [&](const auto& self, std::size_t index) -> void {
    if (index == converted.size()) {
      if (selected.empty()) return;
      std::vector<PowlNode> branch;
      branch.reserve(selected.size());
      for (const auto child : selected) branch.push_back(converted[child]);
      if (branch.size() == 1)
        alternatives.push_back(std::move(branch.front()));
      else
        alternatives.push_back(PowlNode::concurrency(std::move(branch)));
      return;
    }
    self(self, index + 1);
    selected.push_back(index);
    self(self, index + 1);
    selected.pop_back();
  };
  visit(visit, 0);
  if (alternatives.size() == 1) return std::move(alternatives.front());
  return PowlNode::xor_node(std::move(alternatives));
}

class ProcessTreeBuilder {
 public:
  explicit ProcessTreeBuilder(bool strict = false) : strict_(strict) {}

  ProcessTree run(const PowlNode& node) {
    auto root = convert(node);
    return ProcessTree(std::move(activities_), std::move(root));
  }

 private:
  bool strict_ = false;
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

  // pm4py 2.7.22 convert_to_process_tree(powl) keeps a FrequentTransition
  // as one leaf carrying its label, "<label>\n[1,<max or ->]" when it is
  // optional or self-looping (FrequentTransition("A", 0, "-") is the leaf
  // 'A\n[1,-]'), as its Petri-net conversion does; it adds no choice or loop.
  ProcessTreeNode transition(const PowlNode& node) {
    return ProcessTreeNode::leaf(activity_id(reference_frequency_label(node)));
  }

  ProcessTreeNode partial_order(const PowlNode& node) {
    const auto& children = node.children();
    if (children.empty()) return ProcessTreeNode::tau();

    if (strict_) return strict_partial_order(node);

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

  static PowlNode project(const PowlNode& node,
                          const std::vector<std::size_t>& members) {
    if (members.empty()) return PowlNode::silent();
    if (members.size() == 1) return node.children().at(members.front());

    std::map<std::size_t, std::size_t> local;
    std::vector<PowlNode> children;
    children.reserve(members.size());
    for (std::size_t index = 0; index < members.size(); ++index) {
      local.emplace(members[index], index);
      children.push_back(node.children().at(members[index]));
    }
    std::vector<std::pair<std::size_t, std::size_t>> order;
    for (const auto [source, target] : node.order()) {
      const auto source_it = local.find(source);
      const auto target_it = local.find(target);
      if (source_it != local.end() && target_it != local.end())
        order.emplace_back(source_it->second, target_it->second);
    }
    return PowlNode::strict_partial_order(std::move(children), order);
  }

  ProcessTreeNode strict_partial_order(const PowlNode& node) {
    const auto& children = node.children();
    if (children.empty()) return ProcessTreeNode::tau();
    if (children.size() == 1) return convert(children.front());

    const std::size_t count = children.size();
    std::vector<std::vector<std::size_t>> neighbours(count);
    for (const auto [source, target] : node.order()) {
      neighbours[source].push_back(target);
      neighbours[target].push_back(source);
    }
    for (auto& values : neighbours) {
      std::sort(values.begin(), values.end());
      values.erase(std::unique(values.begin(), values.end()), values.end());
    }

    // A disconnected order is exactly a parallel block. Projecting each
    // component preserves its internal order, so no layering approximation is
    // needed for the strict conversion.
    std::vector<std::vector<std::size_t>> components;
    std::vector<bool> visited(count, false);
    for (std::size_t root = 0; root < count; ++root) {
      if (visited[root]) continue;
      std::vector<std::size_t> component{root};
      visited[root] = true;
      for (std::size_t position = 0; position < component.size(); ++position) {
        for (const auto next : neighbours[component[position]]) {
          if (visited[next]) continue;
          visited[next] = true;
          component.push_back(next);
        }
      }
      std::sort(component.begin(), component.end());
      components.push_back(std::move(component));
    }
    if (components.size() > 1) {
      std::vector<ProcessTreeNode> branches;
      branches.reserve(components.size());
      for (const auto& component : components)
        branches.push_back(convert(project(node, component)));
      return parallel(std::move(branches));
    }

    // A series cut is a non-empty prefix A whose every node precedes every
    // node in B. Ancestor sets of candidate boundary nodes are sufficient for
    // the block-structured nets produced by the forward conversion, while the
    // cross-product check prevents accepting a merely topological prefix.
    std::vector<std::size_t> candidates(count);
    for (std::size_t index = 0; index < count; ++index)
      candidates[index] = index;
    std::stable_sort(candidates.begin(), candidates.end(), [&](auto left,
                                                               auto right) {
      std::size_t left_ancestors = 0;
      std::size_t right_ancestors = 0;
      for (const auto [source, target] : node.order()) {
        left_ancestors += target == left;
        right_ancestors += target == right;
      }
      if (left_ancestors != right_ancestors)
        return left_ancestors < right_ancestors;
      return left < right;
    });
    for (const auto boundary : candidates) {
      std::vector<std::size_t> prefix;
      std::vector<std::size_t> suffix;
      for (std::size_t index = 0; index < count; ++index) {
        if (index == boundary) continue;
        if (node.has_edge(index, boundary)) prefix.push_back(index);
      }
      if (prefix.empty() || prefix.size() == count) continue;
      std::vector<bool> in_prefix(count, false);
      for (const auto index : prefix) in_prefix[index] = true;
      for (std::size_t index = 0; index < count; ++index)
        if (!in_prefix[index]) suffix.push_back(index);

      bool complete = true;
      for (const auto source : prefix) {
        for (const auto target : suffix) {
          if (!node.has_edge(source, target)) {
            complete = false;
            break;
          }
        }
        if (!complete) break;
      }
      if (!complete) continue;

      return sequence({convert(project(node, prefix)),
                       convert(project(node, suffix))});
    }

    throw std::invalid_argument(
        "workflow-net conversion found a non-block-structured partial order");
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

// POWL to Petri net as the reference convert_to_petri_net(powl) builds it:
// the root sits between two inner places wired to source and sink by silent
// transitions; a choice puts its children between its own two places; a loop
// adds a silent entry into its do part, runs its redo part back from the do
// exit, and leaves through a silent exit; a partial order gives every child
// its own pair of places, a silent split into the children without
// predecessors, one place per edge of the transitive reduction (a silent
// transition after each child with successors and before each child with
// predecessors) and a silent join out of the children without successors.
// The reference then applies its simple reduction: first every silent
// transition with one input place whose only producer and consumer are other
// and that transition is folded into the producer (single entry), then every
// silent transition with one output place produced only by it and consumed
// by one other transition is folded into the consumer (single exit).
// A frequent transition (optional or self-looping) is one visible transition
// labelled "<label>\n[1,<max or ->]", as the reference FrequentTransition.
class PowlBuilder {
 public:
  explicit PowlBuilder(const PowlNode& root) : root_(root) {}

  PetriNet run() {
    collect(root_);
    places_ = 2;  // source 0, sink 1
    const std::size_t inner_in = add_place();
    const std::size_t inner_out = add_place();
    add(std::nullopt, {0}, {inner_in});
    build(root_, inner_in, inner_out);
    add(std::nullopt, {inner_out}, {1});
    reduce();
    return materialize();
  }

 private:
  struct Raw {
    std::optional<std::size_t> activity;
    std::vector<std::size_t> inputs;
    std::vector<std::size_t> outputs;
    bool alive = true;
  };

  const PowlNode& root_;
  std::vector<std::string> labels_;
  std::map<std::string, std::size_t> label_ids_;
  std::size_t places_ = 0;
  std::vector<Raw> raw_;
  std::vector<bool> removed_places_;

  static std::string frequency_label(const PowlNode& node) {
    return reference_frequency_label(node);
  }

  void collect(const PowlNode& node) {
    if (node.kind() == PowlNodeKind::kTransition) {
      const auto label = frequency_label(node);
      if (label_ids_.find(label) == label_ids_.end()) {
        const auto id = labels_.size();
        label_ids_.emplace(label, id);
        labels_.push_back(label);
      }
    }
    for (const auto& child : node.children()) collect(child);
  }

  std::size_t add_place() { return places_++; }
  void add(std::optional<std::size_t> activity, std::vector<std::size_t> in,
           std::vector<std::size_t> out) {
    raw_.push_back({activity, std::move(in), std::move(out), true});
  }

  void build(const PowlNode& node, std::size_t in, std::size_t out) {
    switch (node.kind()) {
      case PowlNodeKind::kSilent:
        add(std::nullopt, {in}, {out});
        return;
      case PowlNodeKind::kTransition:
        add(label_ids_.at(frequency_label(node)), {in}, {out});
        return;
      case PowlNodeKind::kXor:
        for (const auto& child : node.children()) build(child, in, out);
        return;
      case PowlNodeKind::kLoop: {
        const std::size_t entry = add_place();
        const std::size_t exit = add_place();
        add(std::nullopt, {in}, {entry});
        build(node.children().at(0), entry, exit);
        // The redo child ends in its own place, and a silent transition
        // leads back to the entry (the reduction removes it unless the
        // redo has several ways out).
        const std::size_t redo_out = add_place();
        build(node.children().at(1), exit, redo_out);
        add(std::nullopt, {redo_out}, {entry});
        add(std::nullopt, {exit}, {out});
        return;
      }
      case PowlNodeKind::kStrictPartialOrder:
        build_partial_order(node, in, out);
        return;
    }
  }

  void build_partial_order(const PowlNode& node, std::size_t in, std::size_t out) {
    // An empty order still gets its silent split and join (in -> nothing,
    // nothing -> out), as in the reference, rather than one in -> out step.
    const auto& children = node.children();
    const std::size_t n = children.size();
    std::vector<std::size_t> child_in(n), child_out(n);
    for (std::size_t i = 0; i < n; ++i) {
      child_in[i] = add_place();
      child_out[i] = add_place();
      build(children[i], child_in[i], child_out[i]);
    }
    std::vector<std::vector<std::size_t>> preds(n), succs(n);
    for (const auto [a, b] : node.transitive_reduction()) {
      const std::size_t edge = add_place();
      succs[a].push_back(edge);
      preds[b].push_back(edge);
    }
    std::vector<std::size_t> split, join;
    for (std::size_t i = 0; i < n; ++i) {
      if (preds[i].empty()) split.push_back(child_in[i]);
      if (succs[i].empty()) join.push_back(child_out[i]);
    }
    add(std::nullopt, {in}, split);
    for (std::size_t i = 0; i < n; ++i) {
      if (!succs[i].empty()) add(std::nullopt, {child_out[i]}, succs[i]);
      if (!preds[i].empty()) add(std::nullopt, preds[i], {child_in[i]});
    }
    add(std::nullopt, join, {out});
  }

  // Producers and consumers of a live place.
  std::vector<std::size_t> producers(std::size_t place) const {
    std::vector<std::size_t> result;
    for (std::size_t t = 0; t < raw_.size(); ++t)
      if (raw_[t].alive)
        for (const auto p : raw_[t].outputs) if (p == place) result.push_back(t);
    return result;
  }
  std::vector<std::size_t> consumers(std::size_t place) const {
    std::vector<std::size_t> result;
    for (std::size_t t = 0; t < raw_.size(); ++t)
      if (raw_[t].alive)
        for (const auto p : raw_[t].inputs) if (p == place) result.push_back(t);
    return result;
  }

  static void add_unique(std::vector<std::size_t>& values, std::size_t value) {
    if (std::find(values.begin(), values.end(), value) == values.end()) values.push_back(value);
  }

  bool single_entry() {
    for (std::size_t t = 0; t < raw_.size(); ++t) {
      auto& silent = raw_[t];
      if (!silent.alive || silent.activity || silent.inputs.size() != 1) continue;
      const std::size_t place = silent.inputs.front();
      const auto in = producers(place);
      const auto out = consumers(place);
      if (in.size() != 1 || out.size() != 1 || in.front() == t) continue;
      auto& producer = raw_[in.front()];
      producer.outputs.erase(std::remove(producer.outputs.begin(), producer.outputs.end(), place),
                             producer.outputs.end());
      for (const auto target : silent.outputs) add_unique(producer.outputs, target);
      silent.alive = false;
      removed_places_[place] = true;
      return true;
    }
    return false;
  }

  bool single_exit() {
    for (std::size_t t = 0; t < raw_.size(); ++t) {
      auto& silent = raw_[t];
      if (!silent.alive || silent.activity || silent.outputs.size() != 1) continue;
      const std::size_t place = silent.outputs.front();
      const auto in = producers(place);
      const auto out = consumers(place);
      if (in.size() != 1 || out.size() != 1 || out.front() == t) continue;
      auto& consumer = raw_[out.front()];
      consumer.inputs.erase(std::remove(consumer.inputs.begin(), consumer.inputs.end(), place),
                            consumer.inputs.end());
      for (const auto source : silent.inputs) add_unique(consumer.inputs, source);
      silent.alive = false;
      removed_places_[place] = true;
      return true;
    }
    return false;
  }

  // Folds until neither rule applies. A single-exit fold can open a new
  // single-entry fold (an empty partial order's split and join leave a join
  // with one input), which the reference folds too.
  void reduce() {
    removed_places_.assign(places_, false);
    bool changed = true;
    while (changed) {
      changed = false;
      while (single_entry()) changed = true;
      while (single_exit()) changed = true;
    }
  }

  PetriNet materialize() const {
    PetriNet net;
    net.set_activities(labels_);
    std::vector<std::size_t> ids(places_, 0);
    for (std::size_t p = 0; p < places_; ++p)
      if (!removed_places_[p]) ids[p] = net.add_place();
    net.set_source(ids[0]);
    net.set_sink(ids[1]);
    for (const auto& t : raw_) {
      if (!t.alive) continue;
      std::vector<std::size_t> in, out;
      for (const auto p : t.inputs) in.push_back(ids[p]);
      for (const auto p : t.outputs) out.push_back(ids[p]);
      net.add_transition(t.activity, std::move(in), std::move(out));
    }
    return net;
  }
};

class PetriToPowlBuilder {
  using IdSet = std::set<std::size_t>;

  struct Fragment {
    IdSet starts;
    IdSet ends;
    IdSet transitions;
  };

  struct Boundary {
    std::size_t source = 0;
    std::size_t sink = 0;
  };

 public:
  PetriToPowlBuilder(const PetriNet& net, bool reference_powl_loops)
      : net_(net), reference_powl_loops_(reference_powl_loops) {}

  PowlNode run() {
    if (net_.has_extended_arcs()) {
      throw std::invalid_argument(
          "workflow-net to POWL conversion requires ordinary arcs");
    }
    if (net_.transitions().empty()) {
      throw std::invalid_argument(
          "workflow-net to POWL conversion requires a transition");
    }
    for (const auto& transition : net_.transitions()) {
      if (transition.inputs.empty() || transition.outputs.empty()) {
        throw std::invalid_argument(
            "workflow-net transitions require an input and an output place");
      }
    }

    const auto boundary = validate_boundaries();
    initialize_topology(boundary);

    IdSet transitions;
    for (std::size_t index = 0; index < net_.transitions().size(); ++index)
      transitions.insert(index);
    return convert(Fragment{{canonical_place(boundary.source)},
                            {canonical_place(boundary.sink)},
                            std::move(transitions)});
  }

 private:
  const PetriNet& net_;
  // The reference's net-to-POWL conversion, unlike its net-to-tree
  // conversion, rejects a loop whose redo part leaves from a place the
  // fragment continues from (a sequence place consumed by its own prefix)
  // and a loop whose do part repeats through the loop's end place. to_powl
  // rejects them too; to_process_tree keeps converting them.
  bool reference_powl_loops_ = false;
  std::vector<IdSet> transition_inputs_;
  std::vector<IdSet> transition_outputs_;
  std::vector<std::vector<std::size_t>> place_out_;
  std::vector<std::size_t> place_alias_;

  std::size_t canonical_place(std::size_t place) const {
    if (place >= place_alias_.size()) return place;
    return place_alias_[place];
  }

  static bool subset(const IdSet& values, const IdSet& containing) {
    return std::includes(containing.begin(), containing.end(), values.begin(),
                          values.end());
  }

  static bool intersects(const IdSet& lhs, const IdSet& rhs) {
    auto first = lhs.begin();
    auto second = rhs.begin();
    while (first != lhs.end() && second != rhs.end()) {
      if (*first == *second) return true;
      if (*first < *second) ++first;
      else ++second;
    }
    return false;
  }

  PetriNet copy_with_boundaries(const Boundary& boundary) const {
    PetriNet copy;
    copy.set_activities(net_.activities());
    for (std::size_t place = 0; place < net_.place_count(); ++place)
      (void)copy.add_place();
    copy.set_source(boundary.source);
    copy.set_sink(boundary.sink);
    for (const auto& transition : net_.transitions()) {
      const auto index = copy.add_transition(
          transition.activity, transition.inputs, transition.outputs);
      for (const auto place : transition.reset_inputs)
        copy.add_reset_arc(index, place);
      for (const auto place : transition.inhibitor_inputs)
        copy.add_inhibitor_arc(index, place);
    }
    return copy;
  }

  Boundary validate_boundaries() const {
    const std::size_t node_count =
        net_.place_count() + net_.transitions().size();
    std::vector<std::vector<std::size_t>> outgoing(node_count);
    std::vector<std::vector<std::size_t>> incoming(node_count);
    for (std::size_t transition = 0;
         transition < net_.transitions().size(); ++transition) {
      const auto node = net_.place_count() + transition;
      const auto& model_transition = net_.transitions()[transition];
      if (model_transition.activity &&
          *model_transition.activity >= net_.activities().size()) {
        throw std::invalid_argument(
            "workflow-net transition has an invalid activity reference");
      }
      for (const auto place : model_transition.inputs) {
        if (place >= net_.place_count()) {
          throw std::invalid_argument(
              "workflow-net transition has an invalid input place");
        }
        outgoing[place].push_back(node);
        incoming[node].push_back(place);
      }
      for (const auto place : model_transition.outputs) {
        if (place >= net_.place_count()) {
          throw std::invalid_argument(
              "workflow-net transition has an invalid output place");
        }
        outgoing[node].push_back(place);
        incoming[place].push_back(node);
      }
    }

    // PetriNet keeps source and sink as lightweight metadata with a zero
    // value before either boundary is assigned. In that state, recover the
    // workflow boundaries from the ordinary incidence graph. A workflow net
    // has exactly one place without a producer and one without a consumer;
    // accepting more than one would make the conversion dependent on place
    // insertion order.
    const bool infer_boundaries = !net_.has_source() && !net_.has_sink();
    if (!infer_boundaries && net_.has_source() != net_.has_sink()) {
      throw std::invalid_argument(
          "workflow-net conversion requires both source and sink metadata");
    }
    std::vector<std::size_t> structural_sources;
    std::vector<std::size_t> structural_sinks;
    for (std::size_t place = 0; place < net_.place_count(); ++place) {
      if (incoming[place].empty()) structural_sources.push_back(place);
      if (outgoing[place].empty()) structural_sinks.push_back(place);
    }

    Boundary boundary;
    if (infer_boundaries) {
      if (structural_sources.size() != 1) {
        throw std::invalid_argument(
            "workflow-net conversion requires a unique structural source place");
      }
      if (structural_sinks.size() != 1) {
        throw std::invalid_argument(
            "workflow-net conversion requires a unique structural sink place");
      }
      boundary.source = structural_sources.front();
      boundary.sink = structural_sinks.front();
    } else {
      boundary.source = net_.source();
      boundary.sink = net_.sink();
    }
    if (boundary.source >= net_.place_count() ||
        boundary.sink >= net_.place_count() ||
        boundary.source == boundary.sink) {
      throw std::invalid_argument(
          "workflow-net conversion requires distinct source and sink places");
    }

    for (const auto& transition : net_.transitions()) {
      if (std::find(transition.outputs.begin(), transition.outputs.end(),
                    boundary.sink) != transition.outputs.end()) {
        continue;
      }
      if (std::find(transition.inputs.begin(), transition.inputs.end(),
                    boundary.sink) != transition.inputs.end()) {
        throw std::invalid_argument(
            "workflow-net conversion rejects transitions leaving the sink");
      }
      if (!transition.silent() &&
          std::find(transition.outputs.begin(), transition.outputs.end(),
                    boundary.source) != transition.outputs.end()) {
        throw std::invalid_argument(
            "workflow-net conversion rejects visible returns into the source");
      }
    }

    const auto visit = [](std::size_t start,
                          const std::vector<std::vector<std::size_t>>& graph) {
      std::vector<bool> seen(graph.size(), false);
      std::queue<std::size_t> pending;
      seen[start] = true;
      pending.push(start);
      while (!pending.empty()) {
        const auto current = pending.front();
        pending.pop();
        for (const auto next : graph[current]) {
          if (seen[next]) continue;
          seen[next] = true;
          pending.push(next);
        }
      }
      return seen;
    };
    const auto from_source = visit(boundary.source, outgoing);
    const auto to_sink = visit(boundary.sink, incoming);
    for (std::size_t node = 0; node < node_count; ++node) {
      if (!from_source[node] || !to_sink[node]) {
        throw std::invalid_argument(
            "workflow-net conversion found a node outside the source-to-sink region");
      }
    }
    if (!final_marking_reachable(copy_with_boundaries(boundary))) {
      throw std::invalid_argument(
          "workflow-net conversion requires a reachable final marking");
    }
    return boundary;
  }

  void initialize_topology(const Boundary& boundary) {
    const auto place_count = net_.place_count();
    place_alias_.resize(place_count);
    for (std::size_t place = 0; place < place_count; ++place)
      place_alias_[place] = place;

    // Places with the same producer and consumer transition multisets are
    // local duplicates. Use the first place as their deterministic
    // representative, while keeping the designated boundaries protected.
    // The conversion works on incidence sets, so this normalization also
    // prevents an otherwise harmless duplicate place from creating a false
    // partial-order cut.
    std::vector<std::map<std::size_t, std::size_t>> producers(place_count);
    std::vector<std::map<std::size_t, std::size_t>> consumers(place_count);
    for (std::size_t transition = 0;
         transition < net_.transitions().size(); ++transition) {
      const auto& model_transition = net_.transitions()[transition];
      for (const auto place : model_transition.outputs)
        ++producers[place][transition];
      for (const auto place : model_transition.inputs)
        ++consumers[place][transition];
    }

    for (std::size_t place = 0; place < place_count; ++place) {
      if (place == boundary.source || place == boundary.sink) continue;
      for (std::size_t candidate = 0; candidate < place; ++candidate) {
        if (candidate == boundary.source || candidate == boundary.sink)
          continue;
        if (producers[place] == producers[candidate] &&
            consumers[place] == consumers[candidate]) {
          place_alias_[place] = place_alias_[candidate];
          break;
        }
      }
    }

    transition_inputs_.clear();
    transition_outputs_.clear();
    transition_inputs_.reserve(net_.transitions().size());
    transition_outputs_.reserve(net_.transitions().size());
    for (const auto& transition : net_.transitions()) {
      IdSet inputs;
      IdSet outputs;
      for (const auto place : transition.inputs)
        inputs.insert(canonical_place(place));
      for (const auto place : transition.outputs)
        outputs.insert(canonical_place(place));
      transition_inputs_.push_back(std::move(inputs));
      transition_outputs_.push_back(std::move(outputs));
    }

    place_out_.assign(place_count, {});
    for (std::size_t transition = 0;
         transition < transition_inputs_.size(); ++transition) {
      for (const auto place : transition_inputs_[transition])
        place_out_[place].push_back(transition);
    }
    for (auto& transitions : place_out_) {
      std::sort(transitions.begin(), transitions.end());
      transitions.erase(std::unique(transitions.begin(), transitions.end()),
                        transitions.end());
    }
  }

  void normalize(Fragment& fragment) const {
    for (;;) {
      if (fragment.transitions.size() <= 1) return;

      std::size_t entry = net_.transitions().size();
      std::size_t entry_count = 0;
      bool has_return_to_start = false;
      for (const auto transition : fragment.transitions) {
        if (intersects(transition_outputs_[transition], fragment.starts))
          has_return_to_start = true;
        if (!net_.transitions()[transition].silent() ||
            transition_inputs_[transition] != fragment.starts) {
          continue;
        }
        ++entry_count;
        entry = transition;
      }
      std::size_t boundary_entry_count = 0;
      for (const auto transition : fragment.transitions) {
        if (intersects(transition_inputs_[transition], fragment.starts))
          ++boundary_entry_count;
      }
      if (!has_return_to_start && entry_count == 1 &&
          boundary_entry_count == 1) {
        fragment.transitions.erase(entry);
        fragment.starts = transition_outputs_[entry];
        continue;
      }

      std::size_t exit = net_.transitions().size();
      std::size_t exit_count = 0;
      bool has_continuation_from_end = false;
      for (const auto transition : fragment.transitions) {
        if (intersects(transition_inputs_[transition], fragment.ends))
          has_continuation_from_end = true;
        if (!net_.transitions()[transition].silent() ||
            transition_outputs_[transition] != fragment.ends) {
          continue;
        }
        ++exit_count;
        exit = transition;
      }
      std::size_t boundary_exit_count = 0;
      for (const auto transition : fragment.transitions) {
        if (intersects(transition_outputs_[transition], fragment.ends))
          ++boundary_exit_count;
      }
      if (!has_continuation_from_end && exit_count == 1 &&
          boundary_exit_count == 1) {
        fragment.transitions.erase(exit);
        fragment.ends = transition_inputs_[exit];
        continue;
      }
      return;
    }
  }

  static PowlNode sequence_nodes(PowlNode first, PowlNode second) {
    if (first.kind() == PowlNodeKind::kSilent) return second;
    if (second.kind() == PowlNodeKind::kSilent) return first;
    return PowlNode::sequence({std::move(first), std::move(second)});
  }

  IdSet places_of(const IdSet& transitions) const {
    IdSet places;
    for (const auto transition : transitions) {
      places.insert(transition_inputs_[transition].begin(),
                    transition_inputs_[transition].end());
      places.insert(transition_outputs_[transition].begin(),
                    transition_outputs_[transition].end());
    }
    return places;
  }

  // Connected components of the fragment's transitions, joined through every
  // shared place except `blocked`. Components are ordered by their smallest
  // transition so the decomposition is deterministic.
  std::vector<IdSet> components(const IdSet& transitions,
                                const IdSet& blocked = {}) const {
    std::map<std::size_t, std::vector<std::size_t>> by_place;
    for (const auto transition : transitions) {
      for (const auto place : places_of(IdSet{transition})) {
        if (blocked.contains(place)) continue;
        by_place[place].push_back(transition);
      }
    }
    std::map<std::size_t, std::size_t> parent;
    for (const auto transition : transitions) parent[transition] = transition;
    const std::function<std::size_t(std::size_t)> find =
        [&](std::size_t value) {
          while (parent[value] != value) {
            parent[value] = parent[parent[value]];
            value = parent[value];
          }
          return value;
        };
    for (const auto& [place, members] : by_place) {
      (void)place;
      for (std::size_t index = 1; index < members.size(); ++index) {
        const auto left = find(members.front());
        const auto right = find(members[index]);
        if (left != right) parent[std::max(left, right)] = std::min(left, right);
      }
    }
    std::map<std::size_t, IdSet> grouped;
    for (const auto transition : transitions)
      grouped[find(transition)].insert(transition);
    std::vector<IdSet> result;
    result.reserve(grouped.size());
    for (auto& [root, members] : grouped) {
      (void)root;
      result.push_back(std::move(members));
    }
    return result;
  }

  // A parallel block: the fragment falls apart into place-disjoint
  // components, each of which owns some of the start and end places.
  std::optional<std::vector<Fragment>> parallel_cut(
      const Fragment& fragment) const {
    const auto parts = components(fragment.transitions);
    if (parts.size() < 2) return std::nullopt;
    std::vector<Fragment> result;
    IdSet covered_starts;
    IdSet covered_ends;
    for (const auto& part : parts) {
      const auto places = places_of(part);
      Fragment child{{}, {}, part};
      for (const auto place : fragment.starts)
        if (places.contains(place)) child.starts.insert(place);
      for (const auto place : fragment.ends)
        if (places.contains(place)) child.ends.insert(place);
      if (child.starts.empty() || child.ends.empty()) {
        throw std::invalid_argument(
            "workflow-net conversion found a concurrent fragment without "
            "workflow boundaries");
      }
      covered_starts.insert(child.starts.begin(), child.starts.end());
      covered_ends.insert(child.ends.begin(), child.ends.end());
      result.push_back(std::move(child));
    }
    if (covered_starts != fragment.starts || covered_ends != fragment.ends) {
      throw std::invalid_argument(
          "workflow-net conversion found an unused fragment boundary place");
    }
    return result;
  }

  struct SequenceSplit {
    std::size_t place = 0;
    IdSet prefix;
    IdSet suffix;
  };

  // A sequence block: removing one inner place separates the fragment into a
  // prefix that owns every start place and a suffix that owns every end
  // place. The prefix may loop back through the cut place, or the suffix may,
  // but not both, since that would let the suffix return into the prefix.
  std::optional<SequenceSplit> sequence_cut(const Fragment& fragment) const {
    for (const auto place : places_of(fragment.transitions)) {
      if (fragment.starts.contains(place) || fragment.ends.contains(place))
        continue;
      const auto parts = components(fragment.transitions, IdSet{place});
      if (parts.size() != 2) continue;
      for (std::size_t order = 0; order < 2; ++order) {
        const auto& prefix = parts[order];
        const auto& suffix = parts[1 - order];
        const auto prefix_places = places_of(prefix);
        const auto suffix_places = places_of(suffix);
        if (!subset(fragment.starts, prefix_places) ||
            intersects(fragment.starts, suffix_places) ||
            !subset(fragment.ends, suffix_places) ||
            intersects(fragment.ends, prefix_places)) {
          continue;
        }
        bool prefix_produces = false;
        bool prefix_consumes = false;
        bool suffix_produces = false;
        bool suffix_consumes = false;
        for (const auto transition : prefix) {
          prefix_produces |= transition_outputs_[transition].contains(place);
          prefix_consumes |= transition_inputs_[transition].contains(place);
        }
        for (const auto transition : suffix) {
          suffix_produces |= transition_outputs_[transition].contains(place);
          suffix_consumes |= transition_inputs_[transition].contains(place);
        }
        if (!prefix_produces || !suffix_consumes) continue;
        if (prefix_consumes && suffix_produces) continue;
        if (reference_powl_loops_ && prefix_consumes)
          continue;
        return SequenceSplit{place, prefix, suffix};
      }
    }
    return std::nullopt;
  }

  struct TransitionSplit {
    std::size_t transition = 0;
    IdSet prefix;
    IdSet suffix;
  };

  // A sequence block through one transition, such as a merged join and
  // split: without it, the fragment separates into a prefix that owns the
  // start places and the transition's inputs and a suffix that owns its
  // outputs and the end places.
  std::optional<TransitionSplit> transition_cut(const Fragment& fragment) const {
    for (const auto transition : fragment.transitions) {
      const auto& inputs = transition_inputs_[transition];
      const auto& outputs = transition_outputs_[transition];
      if (intersects(inputs, outputs)) continue;
      IdSet rest = fragment.transitions;
      rest.erase(transition);
      IdSet before = fragment.starts;
      before.insert(inputs.begin(), inputs.end());
      IdSet after = fragment.ends;
      after.insert(outputs.begin(), outputs.end());
      if (intersects(before, after)) continue;
      TransitionSplit split{transition, {}, {}};
      bool valid = true;
      for (const auto& part : components(rest)) {
        const auto places = places_of(part);
        const bool left = intersects(places, before);
        const bool right = intersects(places, after);
        if (left == right) {
          valid = false;
          break;
        }
        (left ? split.prefix : split.suffix).insert(part.begin(), part.end());
      }
      if (!valid || split.prefix.empty() || split.suffix.empty()) continue;
      return split;
    }
    return std::nullopt;
  }

  // An exclusive block: without the shared start and end places, the
  // fragment separates into branches that each span the whole boundary.
  std::optional<std::vector<IdSet>> xor_cut(const Fragment& fragment) const {
    if (fragment.starts.size() != 1) return std::nullopt;
    // A branch that returns into the shared start, or continues from the
    // shared end, could hand control to another branch.
    for (const auto transition : fragment.transitions) {
      if (intersects(transition_outputs_[transition], fragment.starts) ||
          intersects(transition_inputs_[transition], fragment.ends)) {
        return std::nullopt;
      }
    }
    IdSet boundary = fragment.starts;
    boundary.insert(fragment.ends.begin(), fragment.ends.end());
    auto branches = components(fragment.transitions, boundary);
    if (branches.size() < 2) return std::nullopt;
    for (const auto& branch : branches) {
      const auto places = places_of(branch);
      if (!subset(boundary, places)) return std::nullopt;
    }
    return branches;
  }

  PowlNode transition_node(std::size_t transition) const {
    const auto& model_transition = net_.transitions()[transition];
    if (model_transition.silent()) return PowlNode::silent();
    if (!model_transition.activity ||
        *model_transition.activity >= net_.activities().size()) {
      throw std::invalid_argument(
          "workflow-net transition has an invalid activity reference");
    }
    return PowlNode::transition(net_.activities()[*model_transition.activity]);
  }

  // A residual fragment is exact as a partial order only when it is an
  // acyclic marked graph between its boundaries once its choice, skip and
  // loop regions are contracted: every inner place has one producer and one
  // consumer, start places only a consumer and end places only a producer.
  // Then every node fires exactly once and the language is the set of
  // linear extensions of the causal order. A region is a group of
  // transitions joined through places that break this shape; it must have a
  // single entry place and a single exit place and is converted as its own
  // block, like the reference's partial orders over choice and loop nodes.
  // Anything else cannot be expressed this way.
  struct OrderNode {
    PowlNode node;
    IdSet inputs;
    IdSet outputs;
  };

  [[noreturn]] static void not_block_structured() {
    throw std::invalid_argument(
        "workflow-net conversion found a fragment that is not "
        "block-structured");
  }

  static bool marked_graph_place(std::size_t produced, std::size_t consumed,
                                 bool start, bool end) {
    if (start && end) return false;
    if (start) return produced == 0 && consumed == 1;
    if (end) return produced == 1 && consumed == 0;
    return produced == 1 && consumed == 1;
  }

  // The single-entry single-exit region of the fragment between `entry` and
  // `exit`: the transitions that follow the entry and precede the exit,
  // provided every other place they touch is internal to them. Its
  // transitions may return to the entry or consume the exit to repeat, as a
  // loop does. A reduced net between the two places then behaves as one node
  // of the surrounding partial order.
  // Place-indexed views of a fragment, built once per partial order so the
  // region search does not rescan every transition for every place.
  struct FragmentIndex {
    std::map<std::size_t, IdSet> producing;
    std::map<std::size_t, IdSet> consuming;
    std::map<std::size_t, IdSet> touching;
    std::map<std::size_t, IdSet> after;   // transitions reachable from a place
    std::map<std::size_t, IdSet> before;  // transitions reaching a place
  };

  static const IdSet& lookup(const std::map<std::size_t, IdSet>& index,
                             std::size_t place) {
    static const IdSet empty;
    const auto it = index.find(place);
    return it == index.end() ? empty : it->second;
  }

  IdSet reach(const FragmentIndex& index, std::size_t from, bool forward) const {
    IdSet found;
    IdSet seen{from};
    std::queue<std::size_t> places;
    places.push(from);
    while (!places.empty()) {
      const auto place = places.front();
      places.pop();
      for (const auto transition :
           lookup(forward ? index.consuming : index.producing, place)) {
        if (!found.insert(transition).second) continue;
        const auto& far = forward ? transition_outputs_[transition]
                                  : transition_inputs_[transition];
        for (const auto next : far)
          if (seen.insert(next).second) places.push(next);
      }
    }
    return found;
  }

  // The single-entry single-exit region of the fragment between `entry` and
  // `exit`: the transitions that follow the entry and precede the exit,
  // provided every other place they touch is internal to them. Its
  // transitions may return to the entry or consume the exit to repeat, as a
  // loop does. A reduced net between the two places then behaves as one node
  // of the surrounding partial order. `result` is the candidate set, the
  // intersection of the entry's forward and the exit's backward reach.
  std::optional<IdSet> region(const Fragment& fragment,
                              const FragmentIndex& index, std::size_t entry,
                              std::size_t exit, IdSet result) const {
    if (entry == exit || fragment.ends.contains(entry) ||
        fragment.starts.contains(exit))
      return std::nullopt;
    if (result.empty() || result == fragment.transitions) return std::nullopt;
    const auto result_places = places_of(result);
    if (!result_places.contains(entry) || !result_places.contains(exit))
      return std::nullopt;
    bool consumes_entry = false;
    bool produces_exit = false;
    for (const auto transition : result) {
      consumes_entry |= transition_inputs_[transition].contains(entry);
      produces_exit |= transition_outputs_[transition].contains(exit);
    }
    if (!consumes_entry || !produces_exit) return std::nullopt;
    for (const auto place : result_places) {
      if (place != exit && !subset(lookup(index.consuming, place), result))
        return std::nullopt;
      if (place != entry && !subset(lookup(index.producing, place), result))
        return std::nullopt;
    }
    for (const auto place : result_places) {
      if (place == entry || place == exit) continue;
      if (fragment.starts.contains(place) || fragment.ends.contains(place))
        return std::nullopt;
    }
    return result;
  }

  std::vector<OrderNode> order_nodes(const Fragment& fragment) const {
    std::map<std::size_t, std::size_t> producers;
    std::map<std::size_t, std::size_t> consumers;
    FragmentIndex index;
    auto& producing = index.producing;
    auto& consuming = index.consuming;
    auto& touching = index.touching;
    for (const auto transition : fragment.transitions) {
      for (const auto place : transition_outputs_[transition]) {
        ++producers[place];
        producing[place].insert(transition);
        touching[place].insert(transition);
      }
      for (const auto place : transition_inputs_[transition]) {
        ++consumers[place];
        consuming[place].insert(transition);
        touching[place].insert(transition);
      }
    }
    const auto places = places_of(fragment.transitions);
    if (!subset(fragment.starts, places) || !subset(fragment.ends, places))
      not_block_structured();
    // Every place that breaks the marked-graph shape must lie inside the
    // smallest region that holds all of its transitions.
    struct Region {
      IdSet transitions;
      std::size_t entry = 0;
      std::size_t exit = 0;
    };
    std::vector<Region> regions;
    for (const auto place : places) {
      if (marked_graph_place(producers[place], consumers[place],
                             fragment.starts.contains(place),
                             fragment.ends.contains(place)))
        continue;
      // A region resolves the place when, apart from at most the one
      // producer and one consumer the marked graph allows, every transition
      // on the place lies inside it.
      const bool start = fragment.starts.contains(place);
      const bool end = fragment.ends.contains(place);
      const auto resolves = [&](const IdSet& transitions) {
        if (!intersects(touching[place], transitions)) return false;
        std::size_t outside_producers = 0;
        std::size_t outside_consumers = 0;
        for (const auto transition : producing[place])
          outside_producers += transitions.contains(transition) ? 0 : 1;
        for (const auto transition : consuming[place])
          outside_consumers += transitions.contains(transition) ? 0 : 1;
        return outside_producers <= (start ? 0u : 1u) &&
               outside_consumers <= (end ? 0u : 1u);
      };
      if (std::any_of(regions.begin(), regions.end(), [&](const Region& known) {
            return resolves(known.transitions);
          }))
        continue;
      // Candidates are tried in the same (entry, exit) order as an
      // exhaustive scan, and only the strictly smaller one replaces the
      // best, so the cheap necessary conditions below skip pairs without
      // changing the region chosen: a resolving region lies inside the
      // entry's forward and the exit's backward reach, so each reach must
      // already hold all but the allowed outside producers and consumers
      // of the place, and a candidate no smaller than the best never wins.
      const auto reach_resolves = [&](const IdSet& reached) {
        if (!intersects(touching[place], reached)) return false;
        std::size_t outside_producers = 0;
        std::size_t outside_consumers = 0;
        for (const auto transition : producing[place])
          outside_producers += reached.contains(transition) ? 0 : 1;
        for (const auto transition : consuming[place])
          outside_consumers += reached.contains(transition) ? 0 : 1;
        return outside_producers <= (start ? 0u : 1u) &&
               outside_consumers <= (end ? 0u : 1u);
      };
      const auto reach_of = [&](std::map<std::size_t, IdSet>& cache,
                                std::size_t from, bool forward) -> const IdSet& {
        auto it = cache.find(from);
        if (it == cache.end())
          it = cache.emplace(from, reach(index, from, forward)).first;
        return it->second;
      };
      std::vector<std::size_t> exits;
      for (const auto exit : places)
        if (reach_resolves(reach_of(index.before, exit, false))) exits.push_back(exit);
      std::optional<Region> best;
      for (const auto entry : places) {
        const auto& after = reach_of(index.after, entry, true);
        if (exits.empty() || !reach_resolves(after)) continue;
        for (const auto exit : exits) {
          const auto& before = index.before.at(exit);
          IdSet candidate_set;
          std::set_intersection(after.begin(), after.end(), before.begin(),
                                before.end(),
                                std::inserter(candidate_set, candidate_set.end()));
          if (best && candidate_set.size() >= best->transitions.size()) continue;
          if (!resolves(candidate_set)) continue;
          auto candidate =
              region(fragment, index, entry, exit, std::move(candidate_set));
          if (!candidate) continue;
          best = Region{std::move(*candidate), entry, exit};
        }
      }
      if (!best) not_block_structured();
      std::vector<Region> kept;
      for (auto& known : regions) {
        if (subset(known.transitions, best->transitions)) continue;
        if (intersects(known.transitions, best->transitions)) not_block_structured();
        kept.push_back(std::move(known));
      }
      kept.push_back(std::move(*best));
      regions = std::move(kept);
    }
    IdSet in_region;
    for (const auto& known : regions)
      in_region.insert(known.transitions.begin(), known.transitions.end());
    std::vector<OrderNode> nodes;
    for (const auto transition : fragment.transitions) {
      if (in_region.contains(transition)) continue;
      nodes.push_back({transition_node(transition), transition_inputs_[transition],
                       transition_outputs_[transition]});
    }
    for (const auto& known : regions) {
      const IdSet entry{known.entry};
      const IdSet exit{known.exit};
      nodes.push_back({convert(Fragment{entry, exit, known.transitions}), entry, exit});
    }

    // The contracted fragment must itself be a marked graph.
    std::map<std::size_t, std::size_t> node_producers;
    std::map<std::size_t, std::size_t> node_consumers;
    IdSet node_places;
    for (const auto& node : nodes) {
      for (const auto place : node.outputs) {
        ++node_producers[place];
        node_places.insert(place);
      }
      for (const auto place : node.inputs) {
        ++node_consumers[place];
        node_places.insert(place);
      }
    }
    for (const auto place : node_places) {
      if (!marked_graph_place(node_producers[place], node_consumers[place],
                              fragment.starts.contains(place),
                              fragment.ends.contains(place)))
        not_block_structured();
    }
    if (!subset(fragment.starts, node_places) || !subset(fragment.ends, node_places))
      not_block_structured();
    return nodes;
  }

  PowlNode partial_order(const Fragment& fragment) const {
    auto nodes = order_nodes(fragment);
    const auto count = nodes.size();
    std::vector<std::vector<bool>> reaches(count, std::vector<bool>(count, false));
    for (std::size_t source = 0; source < count; ++source)
      for (std::size_t target = 0; target < count; ++target)
        if (source != target && intersects(nodes[source].outputs, nodes[target].inputs))
          reaches[source][target] = true;
    for (std::size_t middle = 0; middle < count; ++middle)
      for (std::size_t source = 0; source < count; ++source)
        if (reaches[source][middle])
          for (std::size_t target = 0; target < count; ++target)
            if (reaches[middle][target]) reaches[source][target] = true;
    for (std::size_t node = 0; node < count; ++node) {
      if (reaches[node][node]) {
        throw std::invalid_argument(
            "workflow-net conversion found an unresolved transition cycle");
      }
    }
    std::vector<std::size_t> visible;
    for (std::size_t node = 0; node < count; ++node)
      if (nodes[node].node.kind() != PowlNodeKind::kSilent) visible.push_back(node);
    if (visible.empty()) return PowlNode::silent();
    if (visible.size() == 1) return std::move(nodes[visible.front()].node);
    std::vector<PowlNode> children;
    children.reserve(visible.size());
    for (const auto node : visible) children.push_back(std::move(nodes[node].node));
    std::vector<std::pair<std::size_t, std::size_t>> order;
    for (std::size_t source = 0; source < visible.size(); ++source)
      for (std::size_t target = 0; target < visible.size(); ++target)
        if (source != target && reaches[visible[source]][visible[target]])
          order.emplace_back(source, target);
    return PowlNode::strict_partial_order(std::move(children), order);
  }

  // A loop needs its do part to start only once every start place is marked
  // and its redo part only once every end place is marked; otherwise the two
  // parts could overlap, which a loop operator does not allow.
  bool exact_loop(const Fragment& fragment, const IdSet& do_part,
                  const IdSet& redo_part) const {
    IdSet boundary = fragment.starts;
    boundary.insert(fragment.ends.begin(), fragment.ends.end());
    IdSet do_inner;
    for (const auto place : places_of(do_part))
      if (!boundary.contains(place)) do_inner.insert(place);
    for (const auto place : places_of(redo_part))
      if (do_inner.contains(place)) return false;
    for (const auto transition : do_part) {
      const auto& inputs = transition_inputs_[transition];
      if (intersects(inputs, fragment.starts) &&
          !subset(fragment.starts, inputs)) {
        return false;
      }
    }
    for (const auto transition : redo_part) {
      const auto& inputs = transition_inputs_[transition];
      if (intersects(inputs, fragment.ends) && !subset(fragment.ends, inputs))
        return false;
    }
    return true;
  }

  // A place that is both a start and an end and that no transition of the
  // fragment touches only holds an idle token (a reduced silent branch), so
  // it does not change the fragment's language.
  bool prune_idle_boundaries(Fragment& fragment) const {
    const auto touched = places_of(fragment.transitions);
    bool changed = false;
    for (auto it = fragment.starts.begin(); it != fragment.starts.end();) {
      if (fragment.ends.contains(*it) && !touched.contains(*it) &&
          !fragment.transitions.empty()) {
        fragment.ends.erase(*it);
        it = fragment.starts.erase(it);
        changed = true;
      } else {
        ++it;
      }
    }
    return changed;
  }

  // The unique transition leaving every start place (or entering every end
  // place) is a sequence boundary even when it is visible, for example a
  // visible join of concurrent branches.
  std::optional<std::size_t> boundary_transition(const Fragment& fragment,
                                                 bool entry) const {
    const auto& boundary = entry ? fragment.starts : fragment.ends;
    std::optional<std::size_t> found;
    for (const auto transition : fragment.transitions) {
      const auto& near = entry ? transition_inputs_[transition]
                               : transition_outputs_[transition];
      const auto& far = entry ? transition_outputs_[transition]
                              : transition_inputs_[transition];
      if (intersects(far, boundary)) return std::nullopt;
      if (!intersects(near, boundary)) continue;
      if (found || near != boundary) return std::nullopt;
      found = transition;
    }
    return found;
  }

  // A fragment whose single start place is also its end place repeats zero
  // or more cycles through that place. Each place-disjoint cycle must leave
  // or re-enter the place through one transition, which separates it into an
  // ordinary fragment between distinct places.
  PowlNode convert_cycles(const Fragment& fragment) const {
    if (fragment.starts.size() != 1) {
      throw std::invalid_argument(
          "workflow-net conversion found a fragment that is not "
          "block-structured");
    }
    const auto place = *fragment.starts.begin();
    std::vector<PowlNode> cycles;
    for (const auto& part : components(fragment.transitions, IdSet{place})) {
      std::vector<std::size_t> entries;
      std::vector<std::size_t> exits;
      for (const auto transition : part) {
        if (transition_inputs_[transition].contains(place))
          entries.push_back(transition);
        if (transition_outputs_[transition].contains(place))
          exits.push_back(transition);
      }
      if (part.size() == 1 && entries.size() == 1 && exits.size() == 1 &&
          transition_inputs_[entries.front()] == fragment.starts &&
          transition_outputs_[entries.front()] == fragment.starts) {
        cycles.push_back(transition_node(entries.front()));
        continue;
      }
      if (entries.size() == 1 &&
          transition_inputs_[entries.front()] == fragment.starts &&
          !transition_outputs_[entries.front()].contains(place)) {
        const auto entry = entries.front();
        IdSet rest = part;
        rest.erase(entry);
        cycles.push_back(sequence_nodes(
            transition_node(entry),
            convert(Fragment{transition_outputs_[entry], fragment.starts,
                             std::move(rest)})));
        continue;
      }
      if (exits.size() == 1 &&
          transition_outputs_[exits.front()] == fragment.starts &&
          !transition_inputs_[exits.front()].contains(place)) {
        const auto exit = exits.front();
        IdSet rest = part;
        rest.erase(exit);
        cycles.push_back(sequence_nodes(
            convert(Fragment{fragment.starts, transition_inputs_[exit],
                             std::move(rest)}),
            transition_node(exit)));
        continue;
      }
      throw std::invalid_argument(
          "workflow-net conversion found a fragment that is not "
          "block-structured");
    }
    auto body = cycles.size() == 1 ? std::move(cycles.front())
                                   : PowlNode::xor_node(std::move(cycles));
    return PowlNode::loop(PowlNode::silent(), std::move(body));
  }

  // A loop block: without the start and end places, the fragment separates
  // into parts that run forward from the start places (the do part, which
  // may repeat internally through the end places) and parts that only lead
  // from the end places back to the start places (the redo part).
  std::optional<std::pair<IdSet, IdSet>> loop_cut(
      const Fragment& fragment) const {
    if (intersects(fragment.starts, fragment.ends)) return std::nullopt;
    IdSet boundary = fragment.starts;
    boundary.insert(fragment.ends.begin(), fragment.ends.end());
    IdSet do_part;
    IdSet redo_part;
    for (const auto& part : components(fragment.transitions, boundary)) {
      bool consumes_start = false;
      bool consumes_end = false;
      bool produces_start = false;
      bool produces_end = false;
      for (const auto transition : part) {
        consumes_start |= intersects(transition_inputs_[transition], fragment.starts);
        consumes_end |= intersects(transition_inputs_[transition], fragment.ends);
        produces_start |= intersects(transition_outputs_[transition], fragment.starts);
        produces_end |= intersects(transition_outputs_[transition], fragment.ends);
      }
      if (!consumes_start && !produces_end && consumes_end && produces_start)
        redo_part.insert(part.begin(), part.end());
      else
        do_part.insert(part.begin(), part.end());
    }
    if (do_part.empty() || redo_part.empty()) return std::nullopt;
    if (reference_powl_loops_) {
      for (const auto transition : do_part)
        if (intersects(transition_inputs_[transition], fragment.ends))
          return std::nullopt;
    }
    if (!exact_loop(fragment, do_part, redo_part)) return std::nullopt;
    return std::make_pair(std::move(do_part), std::move(redo_part));
  }

  // Cycles through a fragment's single end place that nothing else of the
  // fragment leaves run after the rest: ->(rest, *(tau, cycles)). Cycles
  // through a single start place that nothing else enters run before it.
  // The reference reduces such self-loop parts this way (the activity-
  // defines-place net of a DFG with an a -> a edge into its end activity
  // gives ->(X('a', ->('b', 'a')), *(tau, 'a')) for the do part of a loop);
  // the strict reducer rejected them as not block-structured. Only the tree
  // conversion uses this; net_to_powl keeps its reference-matched cuts.
  std::optional<PowlNode> boundary_cycles(const Fragment& fragment) const {
    if (reference_powl_loops_ || fragment.starts.size() != 1 ||
        fragment.ends.size() != 1 || fragment.starts == fragment.ends)
      return std::nullopt;
    const auto start = *fragment.starts.begin();
    const auto end = *fragment.ends.begin();
    for (const bool at_end : {true, false}) {
      const auto place = at_end ? end : start;
      const auto other = at_end ? start : end;
      IdSet cycles;
      IdSet rest;
      for (const auto& part : components(fragment.transitions, IdSet{start, end})) {
        bool consumes = false, produces = false, touches_other = false;
        for (const auto transition : part) {
          const auto& inputs = transition_inputs_[transition];
          const auto& outputs = transition_outputs_[transition];
          consumes |= inputs.contains(place);
          produces |= outputs.contains(place);
          touches_other |= inputs.contains(other) || outputs.contains(other);
        }
        auto& target = consumes && produces && !touches_other ? cycles : rest;
        target.insert(part.begin(), part.end());
      }
      if (cycles.empty() || rest.empty()) continue;
      // The rest must never leave the end place (or enter the start place),
      // so the cycles can only run once the rest is done (or before it).
      const bool blocked = std::any_of(rest.begin(), rest.end(), [&](std::size_t transition) {
        return at_end ? transition_inputs_[transition].contains(place)
                      : transition_outputs_[transition].contains(place);
      });
      if (blocked) continue;
      auto body = convert(Fragment{fragment.starts, fragment.ends, std::move(rest)});
      auto loop = convert_cycles(Fragment{IdSet{place}, IdSet{place}, std::move(cycles)});
      return at_end ? sequence_nodes(std::move(body), std::move(loop))
                    : sequence_nodes(std::move(loop), std::move(body));
    }
    return std::nullopt;
  }

  PowlNode convert(Fragment fragment) const {
    do {
      normalize(fragment);
    } while (prune_idle_boundaries(fragment));
    if (!fragment.transitions.empty() && fragment.starts == fragment.ends)
      return convert_cycles(fragment);
    if (fragment.transitions.size() > 1) {
      if (const auto entry = boundary_transition(fragment, true)) {
        Fragment rest{transition_outputs_[*entry], fragment.ends,
                      fragment.transitions};
        rest.transitions.erase(*entry);
        return sequence_nodes(transition_node(*entry), convert(rest));
      }
      if (const auto exit = boundary_transition(fragment, false)) {
        Fragment rest{fragment.starts, transition_inputs_[*exit],
                      fragment.transitions};
        rest.transitions.erase(*exit);
        return sequence_nodes(convert(rest), transition_node(*exit));
      }
    }
    if (fragment.transitions.empty()) {
      if (fragment.starts == fragment.ends) return PowlNode::silent();
      throw std::invalid_argument(
          "workflow-net conversion found an empty fragment between distinct "
          "places");
    }

    if (fragment.transitions.size() == 1) {
      const auto transition = *fragment.transitions.begin();
      if (transition_inputs_[transition] != fragment.starts ||
          transition_outputs_[transition] != fragment.ends) {
        throw std::invalid_argument(
            "workflow-net conversion found a transition that does not span "
            "its fragment");
      }
      return transition_node(transition);
    }

    if (const auto parts = parallel_cut(fragment)) {
      std::vector<PowlNode> children;
      children.reserve(parts->size());
      for (const auto& part : *parts) children.push_back(convert(part));
      return PowlNode::strict_partial_order(std::move(children), {});
    }

    if (auto cycles = boundary_cycles(fragment)) return std::move(*cycles);

    if (const auto loop = loop_cut(fragment)) {
      return PowlNode::loop(
          convert(Fragment{fragment.starts, fragment.ends, loop->first}),
          convert(Fragment{fragment.ends, fragment.starts, loop->second}));
    }

    if (const auto cut = sequence_cut(fragment)) {
      const IdSet cut_set{cut->place};
      return sequence_nodes(
          convert(Fragment{fragment.starts, cut_set, cut->prefix}),
          convert(Fragment{cut_set, fragment.ends, cut->suffix}));
    }

    if (const auto cut = transition_cut(fragment)) {
      return sequence_nodes(
          sequence_nodes(
              convert(Fragment{fragment.starts,
                               transition_inputs_[cut->transition],
                               cut->prefix}),
              transition_node(cut->transition)),
          convert(Fragment{transition_outputs_[cut->transition],
                           fragment.ends, cut->suffix}));
    }

    if (const auto branches = xor_cut(fragment)) {
      std::vector<PowlNode> children;
      children.reserve(branches->size());
      for (const auto& branch : *branches) {
        children.push_back(convert(
            Fragment{fragment.starts, fragment.ends, branch}));
      }
      return PowlNode::xor_node(std::move(children));
    }

    return partial_order(fragment);
  }
};

}  // namespace

PowlNode to_powl(const ProcessTree& tree) {
  return tree_node_to_powl(tree.root(), tree.activities());
}

ProcessTree to_process_tree(const PowlNode& node) {
  return ProcessTreeBuilder().run(node);
}

namespace {

// pm4py 2.7.22's convert_to_process_tree(net) raises on a net with a single
// transition (AssertionError for a visible one, AttributeError for a silent
// one) instead of returning a leaf; two or more transitions in a chain
// convert (source -> tau -> a -> sink gives 'a'). Its convert_to_petri_net
// of a tree already folds silent steps, so the net of ->(tau, 'a') is one
// transition and raises too.
void reject_reference_single_transition(const PetriNet& net) {
  if (net.transitions().size() != 1) return;
  throw std::invalid_argument(
      net.transitions().front().activity
          ? "workflow-net conversion: the net has a single visible transition "
            "(pm4py raises AssertionError)"
          : "workflow-net conversion: the net has a single silent transition "
            "(pm4py raises AttributeError)");
}

// The reference folds the converted tree: sequences and parallels drop tau
// children and absorb nested children of their own operator, an exclusive
// choice absorbs nested choices and keeps one tau, a loop of taus is a tau,
// and an operator left with one child is that child.
ProcessTreeNode fold_reference_tree(ProcessTreeNode node) {
  if (node.op == ProcessTreeOperator::kActivity ||
      node.op == ProcessTreeOperator::kTau)
    return node;
  std::vector<ProcessTreeNode> folded;
  folded.reserve(node.children.size());
  for (auto& child : node.children)
    folded.push_back(fold_reference_tree(std::move(child)));
  const auto op = node.op;
  if (op == ProcessTreeOperator::kLoop) {
    if (std::all_of(folded.begin(), folded.end(), [](const auto& child) {
          return child.op == ProcessTreeOperator::kTau;
        }))
      return ProcessTreeNode::tau();
    node.children = std::move(folded);
    return node;
  }
  const bool flattens = op == ProcessTreeOperator::kSequence ||
                        op == ProcessTreeOperator::kParallel ||
                        op == ProcessTreeOperator::kExclusive;
  if (!flattens) {
    node.children = std::move(folded);
    return node;
  }
  std::vector<ProcessTreeNode> children;
  bool tau_kept = false;
  const auto keep = [&](ProcessTreeNode child) {
    if (child.op == ProcessTreeOperator::kTau) {
      if (op != ProcessTreeOperator::kExclusive || tau_kept) return;
      tau_kept = true;
    }
    children.push_back(std::move(child));
  };
  for (auto& child : folded) {
    if (child.op == op) {
      for (auto& grandchild : child.children) keep(std::move(grandchild));
    } else {
      keep(std::move(child));
    }
  }
  if (children.empty()) return ProcessTreeNode::tau();
  if (children.size() == 1) return std::move(children.front());
  node.children = std::move(children);
  return node;
}

}  // namespace

ProcessTree to_process_tree(const PetriNet& net) {
  // Invalid nets keep the reconstruction's own errors; a valid workflow net
  // of one transition is then refused as the reference refuses it.
  const auto powl = PetriToPowlBuilder(net, false).run();
  reject_reference_single_transition(net);
  auto tree = ProcessTreeBuilder(true).run(powl);
  return ProcessTree(tree.activities(), fold_reference_tree(tree.root()));
}

PetriNet to_petri_net(const PowlNode& node) { return PowlBuilder(node).run(); }

PowlNode to_powl(const PetriNet& net) { return PetriToPowlBuilder(net, true).run(); }

}  // namespace minera
