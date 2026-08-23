#include "minera/powl_conversion.hpp"

#include "minera/conformance.hpp"

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

class PetriToPowlBuilder {
  using IdSet = std::set<std::size_t>;

  struct Fragment {
    IdSet starts;
    IdSet ends;
    IdSet transitions;
  };

 public:
  explicit PetriToPowlBuilder(const PetriNet& net) : net_(net) {
    place_out_.resize(net_.place_count());
    for (std::size_t index = 0; index < net_.transitions().size(); ++index) {
      const auto& transition = net_.transitions()[index];
      transition_inputs_.push_back(to_set(transition.inputs));
      transition_outputs_.push_back(to_set(transition.outputs));
      for (const auto place : transition.inputs) {
        if (place < place_out_.size()) place_out_[place].push_back(index);
      }
    }
    for (auto& transitions : place_out_) {
      std::sort(transitions.begin(), transitions.end());
      transitions.erase(std::unique(transitions.begin(), transitions.end()),
                        transitions.end());
    }
  }

  PowlNode run() const {
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

    IdSet transitions;
    for (std::size_t index = 0; index < net_.transitions().size(); ++index)
      transitions.insert(index);
    return convert(Fragment{{boundary.first},
                            {boundary.second},
                            std::move(transitions)});
  }

 private:
  const PetriNet& net_;
  std::vector<IdSet> transition_inputs_;
  std::vector<IdSet> transition_outputs_;
  std::vector<std::vector<std::size_t>> place_out_;

  static IdSet to_set(const std::vector<std::size_t>& values) {
    return IdSet(values.begin(), values.end());
  }

  static bool equal(const std::vector<std::size_t>& values,
                    const IdSet& expected) {
    return to_set(values) == expected;
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

  std::pair<std::size_t, std::size_t> validate_boundaries() const {
    const auto source = net_.source();
    const auto sink = net_.sink();
    if (source >= net_.place_count() || sink >= net_.place_count() ||
        source == sink) {
      throw std::invalid_argument(
          "workflow-net conversion requires distinct source and sink places");
    }

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

    for (const auto& transition : net_.transitions()) {
      if (std::find(transition.outputs.begin(), transition.outputs.end(),
                    sink) != transition.outputs.end()) {
        continue;
      }
      if (std::find(transition.inputs.begin(), transition.inputs.end(), sink) !=
          transition.inputs.end()) {
        throw std::invalid_argument(
            "workflow-net conversion rejects transitions leaving the sink");
      }
      if (!transition.silent() &&
          std::find(transition.outputs.begin(), transition.outputs.end(),
                    source) != transition.outputs.end()) {
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
    const auto from_source = visit(source, outgoing);
    const auto to_sink = visit(sink, incoming);
    for (std::size_t node = 0; node < node_count; ++node) {
      if (!from_source[node] || !to_sink[node]) {
        throw std::invalid_argument(
            "workflow-net conversion found a node outside the source-to-sink region");
      }
    }
    if (!final_marking_reachable(net_)) {
      throw std::invalid_argument(
          "workflow-net conversion requires a reachable final marking");
    }
    return {source, sink};
  }

  bool all_inputs_available(std::size_t transition,
                            const IdSet& available) const {
    return subset(transition_inputs_[transition], available);
  }

  IdSet viable_transitions(const IdSet& ends,
                           const IdSet& allowed) const {
    IdSet viable_places = ends;
    IdSet viable;
    bool changed = true;
    while (changed) {
      changed = false;
      for (const auto transition : allowed) {
        if (viable.contains(transition)) continue;
        if (!intersects(transition_outputs_[transition], viable_places))
          continue;
        viable.insert(transition);
        for (const auto place : transition_inputs_[transition])
          changed = viable_places.insert(place).second || changed;
      }
    }
    return viable;
  }

  IdSet collect_to(const IdSet& starts, const IdSet& ends,
                   const IdSet& allowed) const {
    const auto viable = viable_transitions(ends, allowed);
    IdSet available = starts;
    IdSet result;
    std::queue<std::size_t> places;
    for (const auto place : starts) places.push(place);

    while (!places.empty()) {
      const auto place = places.front();
      places.pop();
      if (place >= place_out_.size()) continue;
      for (const auto transition : place_out_[place]) {
        if (!allowed.contains(transition) || !viable.contains(transition) ||
            result.contains(transition) ||
            !all_inputs_available(transition, available)) {
          continue;
        }
        result.insert(transition);
        for (const auto output : transition_outputs_[transition]) {
          if (ends.contains(output) || starts.contains(output)) continue;
          if (available.insert(output).second) places.push(output);
        }
      }
    }
    return result;
  }

  IdSet collect_seed(std::size_t seed, const IdSet& starts,
                     const IdSet& ends, const IdSet& allowed) const {
    if (!allowed.contains(seed) ||
        !all_inputs_available(seed, starts)) {
      return {};
    }
    const auto viable = viable_transitions(ends, allowed);
    if (!viable.contains(seed)) return {};

    IdSet available = starts;
    IdSet result{seed};
    std::queue<std::size_t> places;
    for (const auto output : transition_outputs_[seed]) {
      if (ends.contains(output) || starts.contains(output)) continue;
      if (available.insert(output).second) places.push(output);
    }

    while (!places.empty()) {
      const auto place = places.front();
      places.pop();
      if (place >= place_out_.size()) continue;
      for (const auto transition : place_out_[place]) {
        if (!allowed.contains(transition) || !viable.contains(transition) ||
            result.contains(transition) ||
            !all_inputs_available(transition, available)) {
          continue;
        }
        result.insert(transition);
        for (const auto output : transition_outputs_[transition]) {
          if (ends.contains(output) || starts.contains(output)) continue;
          if (available.insert(output).second) places.push(output);
        }
      }
    }
    return result;
  }

  bool reaches(const IdSet& starts, const IdSet& ends,
               const IdSet& allowed,
               std::optional<std::size_t> blocked_place = std::nullopt) const {
    if (intersects(starts, ends)) return true;
    IdSet available = starts;
    IdSet seen_transitions;
    std::queue<std::size_t> places;
    for (const auto place : starts) {
      if (blocked_place && *blocked_place == place) return false;
      places.push(place);
    }
    while (!places.empty()) {
      const auto place = places.front();
      places.pop();
      if (place >= place_out_.size()) continue;
      for (const auto transition : place_out_[place]) {
        if (!allowed.contains(transition) || seen_transitions.contains(transition) ||
            !all_inputs_available(transition, available)) {
          continue;
        }
        if (blocked_place &&
            (transition_inputs_[transition].contains(*blocked_place) ||
             transition_outputs_[transition].contains(*blocked_place))) {
          continue;
        }
        seen_transitions.insert(transition);
        for (const auto output : transition_outputs_[transition]) {
          if (blocked_place && output == *blocked_place) continue;
          if (ends.contains(output)) return true;
          if (starts.contains(output)) continue;
          if (available.insert(output).second) places.push(output);
        }
      }
    }
    return false;
  }

  bool transition_reaches(std::size_t source, std::size_t target,
                          const IdSet& allowed) const {
    if (source == target) return false;
    IdSet visited_places;
    std::queue<std::size_t> places;
    for (const auto place : transition_outputs_[source]) {
      if (visited_places.insert(place).second) places.push(place);
    }
    while (!places.empty()) {
      const auto place = places.front();
      places.pop();
      if (place >= place_out_.size()) continue;
      for (const auto transition : place_out_[place]) {
        if (!allowed.contains(transition)) continue;
        if (transition == target) return true;
        for (const auto output : transition_outputs_[transition])
          if (visited_places.insert(output).second) places.push(output);
      }
    }
    return false;
  }

  bool transition_has_cycle(std::size_t source, const IdSet& allowed) const {
    IdSet visited_places;
    std::queue<std::size_t> places;
    for (const auto place : transition_outputs_[source]) {
      if (visited_places.insert(place).second) places.push(place);
    }
    while (!places.empty()) {
      const auto place = places.front();
      places.pop();
      if (place >= place_out_.size()) continue;
      for (const auto transition : place_out_[place]) {
        if (!allowed.contains(transition)) continue;
        if (transition == source) return true;
        for (const auto output : transition_outputs_[transition])
          if (visited_places.insert(output).second) places.push(output);
      }
    }
    return false;
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
            !equal(net_.transitions()[transition].inputs, fragment.starts)) {
          continue;
        }
        ++entry_count;
        entry = transition;
      }
      std::size_t boundary_entry_count = 0;
      for (const auto transition : fragment.transitions) {
        if (subset(transition_inputs_[transition], fragment.starts))
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
            !equal(net_.transitions()[transition].outputs, fragment.ends)) {
          continue;
        }
        ++exit_count;
        exit = transition;
      }
      std::size_t boundary_exit_count = 0;
      for (const auto transition : fragment.transitions) {
        if (subset(transition_outputs_[transition], fragment.ends))
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

  std::optional<std::size_t> sequence_cut(const Fragment& fragment) const {
    for (std::size_t place = 0; place < net_.place_count(); ++place) {
      if (fragment.starts.contains(place) || fragment.ends.contains(place))
        continue;
      const IdSet singleton{place};
      if (!reaches(fragment.starts, singleton, fragment.transitions) ||
          !reaches(singleton, fragment.ends, fragment.transitions) ||
          reaches(fragment.starts, fragment.ends, fragment.transitions, place)) {
        continue;
      }
      const auto prefix = collect_to(fragment.starts, singleton,
                                     fragment.transitions);
      const auto suffix = collect_to(singleton, fragment.ends,
                                     fragment.transitions);
      if (prefix.empty() || suffix.empty() || intersects(prefix, suffix))
        continue;
      IdSet union_set = prefix;
      union_set.insert(suffix.begin(), suffix.end());
      if (union_set == fragment.transitions) return place;
    }
    return std::nullopt;
  }

  std::optional<std::vector<IdSet>> xor_cut(const Fragment& fragment) const {
    if (fragment.starts.size() != 1) return std::nullopt;
    std::vector<std::size_t> entries;
    for (const auto transition : fragment.transitions) {
      if (equal(net_.transitions()[transition].inputs, fragment.starts))
        entries.push_back(transition);
    }
    std::sort(entries.begin(), entries.end());
    if (entries.size() < 2) return std::nullopt;

    std::vector<IdSet> branches;
    IdSet union_set;
    for (const auto entry : entries) {
      auto branch = collect_seed(entry, fragment.starts, fragment.ends,
                                 fragment.transitions);
      if (branch.empty()) return std::nullopt;
      for (const auto& other : branches)
        if (intersects(branch, other)) return std::nullopt;
      union_set.insert(branch.begin(), branch.end());
      branches.push_back(std::move(branch));
    }
    if (union_set != fragment.transitions) return std::nullopt;
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

  PowlNode partial_order(const Fragment& fragment) const {
    std::vector<std::size_t> visible;
    for (const auto transition : fragment.transitions)
      if (!net_.transitions()[transition].silent()) visible.push_back(transition);
    if (visible.empty()) return PowlNode::silent();
    for (const auto transition : visible) {
      if (transition_has_cycle(transition, fragment.transitions)) {
        throw std::invalid_argument(
            "workflow-net conversion found an unresolved transition cycle");
      }
    }
    std::vector<PowlNode> children;
    children.reserve(visible.size());
    for (const auto transition : visible)
      children.push_back(transition_node(transition));
    if (children.size() == 1) return std::move(children.front());

    std::vector<std::pair<std::size_t, std::size_t>> order;
    for (std::size_t source = 0; source < visible.size(); ++source) {
      for (std::size_t target = 0; target < visible.size(); ++target) {
        if (source == target) continue;
        const bool forward =
            transition_reaches(visible[source], visible[target],
                               fragment.transitions);
        const bool backward =
            transition_reaches(visible[target], visible[source],
                               fragment.transitions);
        if (forward && backward) {
          throw std::invalid_argument(
              "workflow-net conversion found an unresolved transition cycle");
        }
        if (forward) order.emplace_back(source, target);
      }
    }
    return PowlNode::strict_partial_order(std::move(children), order);
  }

  PowlNode convert(Fragment fragment) const {
    normalize(fragment);
    if (fragment.transitions.empty()) return PowlNode::silent();

    if (fragment.transitions.size() == 1) {
      return transition_node(*fragment.transitions.begin());
    }

    const auto do_part = collect_to(fragment.starts, fragment.ends,
                                    fragment.transitions);
    const auto redo_part = collect_to(fragment.ends, fragment.starts,
                                      fragment.transitions);
    if (!do_part.empty() && !redo_part.empty() &&
        !intersects(do_part, redo_part)) {
      IdSet union_set = do_part;
      union_set.insert(redo_part.begin(), redo_part.end());
      if (union_set == fragment.transitions) {
        return PowlNode::loop(
            convert(Fragment{fragment.starts, fragment.ends, do_part}),
            convert(Fragment{fragment.ends, fragment.starts, redo_part}));
      }
    }

    if (const auto cut = sequence_cut(fragment)) {
      const IdSet cut_set{*cut};
      const auto prefix = collect_to(fragment.starts, cut_set,
                                     fragment.transitions);
      const auto suffix = collect_to(cut_set, fragment.ends,
                                     fragment.transitions);
      return sequence_nodes(
          convert(Fragment{fragment.starts, cut_set, prefix}),
          convert(Fragment{cut_set, fragment.ends, suffix}));
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

PetriNet to_petri_net(const PowlNode& node) { return PowlBuilder(node).run(); }

PowlNode to_powl(const PetriNet& net) { return PetriToPowlBuilder(net).run(); }

}  // namespace minera
