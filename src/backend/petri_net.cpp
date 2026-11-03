#include "minera/petri_net.hpp"
#include "minera/petri_net_reduction.hpp"
#include "minera/process_tree_petri.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace minera {
namespace {

struct BorderedFragment {
  std::size_t source_transition = 0;
  std::size_t sink_transition = 0;
};

// Reference-compatible standard conversion. The construction, the root
// wrapper taus, the element names and the two-pass simple reduction follow
// observed reference behaviour (docs/reference-2.7.22-parity.md, "Process-tree
// Petri conversion"); the shared reduce_petri_net rule family is not used.
class ReferenceTreeNetBuilder {
 public:
  struct Node {
    std::optional<std::size_t> activity;
    std::vector<std::size_t> inputs;
    std::vector<std::size_t> outputs;
    std::string name;
    bool alive = true;
  };

  explicit ReferenceTreeNetBuilder(const ProcessTree& tree) : tree_(tree) {}

  ProcessTreePetriNetConversion run() {
    const auto source = named_place("source");
    const auto sink = named_place("sink");
    const auto& root = tree_.root();
    bool initial_wrapper = false;
    bool final_wrapper = false;
    switch (root.op) {
      case ProcessTreeOperator::kExclusive:
      case ProcessTreeOperator::kParallel:
      case ProcessTreeOperator::kLoop:
        initial_wrapper = final_wrapper = true;
        break;
      case ProcessTreeOperator::kSequence:
      case ProcessTreeOperator::kInclusiveChoice:
      case ProcessTreeOperator::kInterleaving:
        if (!root.children.empty()) {
          initial_wrapper = loop_on_first_spine(root.children.front()) ||
                            loop_on_last_spine(root.children.front());
          final_wrapper = loop_on_last_spine(root.children.back());
        }
        break;
      case ProcessTreeOperator::kActivity:
      case ProcessTreeOperator::kTau:
        break;
    }
    std::optional<std::size_t> initial_tau;
    std::optional<std::size_t> final_tau;
    if (initial_wrapper) initial_tau = hidden("tau", {source}, {});
    if (final_wrapper) final_tau = hidden("tau", {}, {sink});
    std::size_t entry = source;
    std::size_t exit = sink;
    if (initial_tau) {
      entry = place();
      nodes_[*initial_tau].outputs.push_back(entry);
    }
    if (final_tau) {
      exit = place();
      nodes_[*final_tau].inputs.push_back(exit);
    }
    build(root, entry, exit, 0);
    reduce();
    return assemble(source, sink);
  }

 private:
  static bool loop_on_first_spine(const ProcessTreeNode& node) {
    const ProcessTreeNode* current = &node;
    while (!current->children.empty()) {
      if (current->op == ProcessTreeOperator::kLoop) return true;
      current = &current->children.front();
    }
    return current->op == ProcessTreeOperator::kLoop;
  }
  static bool loop_on_last_spine(const ProcessTreeNode& node) {
    const ProcessTreeNode* current = &node;
    while (!current->children.empty()) {
      if (current->op == ProcessTreeOperator::kLoop) return true;
      current = &current->children.back();
    }
    return current->op == ProcessTreeOperator::kLoop;
  }

  std::size_t named_place(std::string name) {
    place_names_.push_back(std::move(name));
    place_alive_.push_back(true);
    producers_.emplace_back();
    consumers_.emplace_back();
    return place_names_.size() - 1;
  }
  std::size_t place() { return named_place("p_" + std::to_string(++place_counter_)); }

  std::size_t add_node(Node node) {
    nodes_.push_back(std::move(node));
    return nodes_.size() - 1;
  }
  std::size_t hidden(const char* kind, std::vector<std::size_t> inputs,
                     std::vector<std::size_t> outputs) {
    return add_node({std::nullopt, std::move(inputs), std::move(outputs),
                     std::string(kind) + "_" + std::to_string(++hidden_counter_)});
  }
  std::size_t visible(std::size_t activity, std::size_t in, std::size_t out) {
    static constexpr char hex[] = "0123456789abcdef";
    std::string suffix(12, '0');
    auto value = visible_counter_++;
    for (std::size_t i = 12; i-- > 0 && value != 0; value >>= 4) suffix[i] = hex[value & 15U];
    return add_node({activity, {in}, {out}, "00000000-0000-4000-8000-" + suffix});
  }

  void build(const ProcessTreeNode& node, std::size_t in, std::size_t out,
             std::size_t depth) {
    if (depth > 100000)
      throw std::invalid_argument("process tree is too deep for Petri conversion");
    const auto& children = node.children;
    switch (node.op) {
      case ProcessTreeOperator::kActivity:
        visible(node.activity, in, out);
        return;
      case ProcessTreeOperator::kTau:
        hidden("skip", {in}, {out});
        return;
      case ProcessTreeOperator::kSequence: {
        std::size_t current = in;
        for (std::size_t i = 0; i < children.size(); ++i) {
          const auto next = i + 1 == children.size() ? out : place();
          build(children[i], current, next, depth + 1);
          current = next;
        }
        return;
      }
      case ProcessTreeOperator::kExclusive:
        for (const auto& child : children) build(child, in, out, depth + 1);
        return;
      case ProcessTreeOperator::kParallel: {
        const auto split = hidden("tauSplit", {in}, {});
        const auto join = hidden("tauJoin", {}, {out});
        for (const auto& child : children) {
          const auto child_in = place();
          const auto child_out = place();
          nodes_[split].outputs.push_back(child_in);
          nodes_[join].inputs.push_back(child_out);
          build(child, child_in, child_out, depth + 1);
        }
        return;
      }
      case ProcessTreeOperator::kLoop: {
        if (children.empty()) {
          hidden("skip", {in}, {out});
          return;
        }
        // Body start B, body end E and redo end R; every redo child runs
        // from E to R and one silent "loop" transition returns R to B.
        const auto body_start = place();
        hidden("init_loop", {in}, {body_start});
        const auto back = hidden("loop", {}, {body_start});
        const auto body_end = place();
        build(children.front(), body_start, body_end, depth + 1);
        if (children.size() > 1) {
          const auto redo_end = place();
          nodes_[back].inputs.push_back(redo_end);
          for (std::size_t i = 1; i < children.size(); ++i)
            build(children[i], body_end, redo_end, depth + 1);
        } else {
          nodes_[back].alive = false;  // no redo path: nothing returns to B
        }
        hidden("skip", {body_end}, {out});
        return;
      }
      case ProcessTreeOperator::kInclusiveChoice: {
        if (children.empty())
          throw std::invalid_argument(
              "process-tree inclusive OR requires at least one child");
        const auto split = hidden("tauSplit", {in}, {});
        const auto join = hidden("tauJoin", {}, {out});
        const auto started = place();
        const auto not_started = place();
        nodes_[split].outputs.push_back(not_started);
        for (const auto& child : children) {
          const auto selector = place();
          const auto child_in = place();
          const auto child_out = place();
          nodes_[split].outputs.push_back(selector);
          nodes_[join].inputs.push_back(child_out);
          hidden("inclusiveStart", {not_started, selector}, {started, child_in});
          hidden("inclusiveLater", {started, selector}, {started, child_in});
          hidden("inclusiveSkip", {started, selector}, {started, child_out});
          build(child, child_in, child_out, depth + 1);
        }
        nodes_[join].inputs.push_back(started);
        return;
      }
      case ProcessTreeOperator::kInterleaving: {
        const auto split = hidden("tauSplit", {in}, {});
        const auto join = hidden("tauJoin", {}, {out});
        const auto mutex = place();
        nodes_[split].outputs.push_back(mutex);
        nodes_[join].inputs.push_back(mutex);
        for (const auto& child : children) {
          const auto selector = place();
          const auto done = place();
          const auto child_in = place();
          const auto child_out = place();
          nodes_[split].outputs.push_back(selector);
          nodes_[join].inputs.push_back(done);
          hidden("iTrans", {mutex, selector}, {child_in});
          hidden("fTrans", {child_out}, {mutex, done});
          build(child, child_in, child_out, depth + 1);
        }
        return;
      }
    }
    throw std::invalid_argument("unknown process-tree operator");
  }

  void index_arcs() {
    for (auto& list : producers_) list.clear();
    for (auto& list : consumers_) list.clear();
    for (std::size_t t = 0; t < nodes_.size(); ++t) {
      if (!nodes_[t].alive) continue;
      for (const auto p : nodes_[t].inputs) consumers_[p].push_back(t);
      for (const auto p : nodes_[t].outputs) producers_[p].push_back(t);
    }
  }
  static void replace_all(std::vector<std::size_t>& values, std::size_t from,
                          std::size_t to) {
    std::replace(values.begin(), values.end(), from, to);
  }
  static void erase_one(std::vector<std::size_t>& values, std::size_t value) {
    const auto found = std::find(values.begin(), values.end(), value);
    if (found != values.end()) values.erase(found);
  }

  // Single-entry pass to a fixed point, then single-exit pass to a fixed
  // point. A silent transition t whose only input place p has exactly one
  // incoming arc (from u != t) and exactly one outgoing arc (to t) is removed
  // with p, u taking t's output arcs. Symmetrically, a silent t whose only
  // output place q has one incoming arc (from t) and one outgoing arc (to
  // v != t) is removed with q, v taking t's input arcs.
  void reduce() {
    index_arcs();
    for (bool changed = true; changed;) {
      changed = false;
      for (std::size_t t = 0; t < nodes_.size(); ++t) {
        auto& node = nodes_[t];
        if (!node.alive || node.activity || node.inputs.size() != 1) continue;
        const auto p = node.inputs.front();
        if (producers_[p].size() != 1 || consumers_[p].size() != 1) continue;
        const auto u = producers_[p].front();
        if (u == t) continue;
        erase_one(nodes_[u].outputs, p);
        for (const auto q : node.outputs) {
          nodes_[u].outputs.push_back(q);
          replace_all(producers_[q], t, u);
        }
        producers_[p].clear();
        consumers_[p].clear();
        place_alive_[p] = false;
        node.alive = false;
        changed = true;
      }
    }
    for (bool changed = true; changed;) {
      changed = false;
      for (std::size_t t = 0; t < nodes_.size(); ++t) {
        auto& node = nodes_[t];
        if (!node.alive || node.activity || node.outputs.size() != 1) continue;
        const auto q = node.outputs.front();
        if (producers_[q].size() != 1 || consumers_[q].size() != 1) continue;
        const auto v = consumers_[q].front();
        if (v == t) continue;
        erase_one(nodes_[v].inputs, q);
        for (const auto p : node.inputs) {
          nodes_[v].inputs.push_back(p);
          replace_all(consumers_[p], t, v);
        }
        producers_[q].clear();
        consumers_[q].clear();
        place_alive_[q] = false;
        node.alive = false;
        changed = true;
      }
    }
  }

  ProcessTreePetriNetConversion assemble(std::size_t source, std::size_t sink) {
    ProcessTreePetriNetConversion result;
    std::vector<std::size_t> remap(place_names_.size(), 0);
    for (std::size_t p = 0; p < place_names_.size(); ++p) {
      if (!place_alive_[p]) continue;
      remap[p] = result.net.add_place();
      result.place_names.push_back(place_names_[p]);
    }
    // The net carries the reference's place names so replay tie-breaks on
    // them even when no PNML document accompanies the converted net.
    result.net.set_place_names(result.place_names);
    result.net.set_activities(tree_.activities());
    result.net.set_source(remap[source]);
    result.net.set_sink(remap[sink]);
    PetriTransitionNameList names;
    for (const auto& node : nodes_) {
      if (!node.alive) continue;
      std::vector<std::size_t> inputs;
      std::vector<std::size_t> outputs;
      inputs.reserve(node.inputs.size());
      outputs.reserve(node.outputs.size());
      for (const auto p : node.inputs) inputs.push_back(remap[p]);
      for (const auto p : node.outputs) outputs.push_back(remap[p]);
      result.net.add_transition(node.activity, std::move(inputs), std::move(outputs));
      result.transition_names.push_back(node.name);
      names.emplace_back(node.name);
    }
    result.net.set_transition_names(std::move(names));
    return result;
  }

  const ProcessTree& tree_;
  std::vector<std::string> place_names_;
  std::vector<bool> place_alive_;
  std::vector<std::vector<std::size_t>> producers_;
  std::vector<std::vector<std::size_t>> consumers_;
  std::vector<Node> nodes_;
  std::size_t place_counter_ = 2;
  std::size_t hidden_counter_ = 0;
  std::size_t visible_counter_ = 0;
};

BorderedFragment build_transition_bordered(const ProcessTreeNode& node,
                                           const ProcessTree& tree,
                                           PetriNet& net) {
  switch (node.op) {
    case ProcessTreeOperator::kActivity:
      if (node.activity >= tree.activities().size()) {
        throw std::out_of_range(
            "process-tree activity index is outside the activity table");
      }
      {
        const auto transition =
            net.add_transition(node.activity, {}, {});
        return {transition, transition};
      }
    case ProcessTreeOperator::kTau: {
      const auto transition = net.add_transition(std::nullopt, {}, {});
      return {transition, transition};
    }
    case ProcessTreeOperator::kSequence: {
      if (node.children.empty()) {
        throw std::invalid_argument(
            "process-tree sequence requires at least one child");
      }
      std::vector<BorderedFragment> children;
      children.reserve(node.children.size());
      for (const auto& child : node.children) {
        children.push_back(build_transition_bordered(child, tree, net));
      }
      std::vector<std::size_t> places;
      places.reserve(children.size() + 1);
      for (std::size_t i = 0; i <= children.size(); ++i) {
        places.push_back(net.add_place());
      }
      for (std::size_t i = 0; i < children.size(); ++i) {
        net.add_arc(children[i].source_transition, places[i]);
        net.add_output_arc(children[i].sink_transition, places[i + 1]);
      }
      const auto source = net.add_transition(std::nullopt, {}, {places.front()});
      const auto sink =
          net.add_transition(std::nullopt, {places.back()}, {});
      return {source, sink};
    }
    case ProcessTreeOperator::kExclusive: {
      if (node.children.empty()) {
        throw std::invalid_argument(
            "process-tree exclusive choice requires at least one child");
      }
      std::vector<BorderedFragment> children;
      children.reserve(node.children.size());
      for (const auto& child : node.children) {
        children.push_back(build_transition_bordered(child, tree, net));
      }
      const auto input = net.add_place();
      const auto output = net.add_place();
      for (const auto fragment : children) {
        net.add_arc(fragment.source_transition, input);
        net.add_output_arc(fragment.sink_transition, output);
      }
      const auto source = net.add_transition(std::nullopt, {}, {input});
      const auto sink = net.add_transition(std::nullopt, {output}, {});
      return {source, sink};
    }
    case ProcessTreeOperator::kInclusiveChoice:
      throw std::invalid_argument(
          "transition-bordered process-tree conversion does not support inclusive choice");
    case ProcessTreeOperator::kInterleaving:
      throw std::invalid_argument(
          "transition-bordered process-tree conversion does not support interleaving");
    case ProcessTreeOperator::kParallel: {
      if (node.children.empty()) {
        throw std::invalid_argument(
            "process-tree parallel operator requires at least one child");
      }
      std::vector<std::size_t> inputs;
      std::vector<std::size_t> outputs;
      std::vector<BorderedFragment> children;
      children.reserve(node.children.size());
      inputs.reserve(node.children.size());
      outputs.reserve(node.children.size());
      for (const auto& child : node.children) {
        children.push_back(build_transition_bordered(child, tree, net));
      }
      for (const auto fragment : children) {
        const auto input = net.add_place();
        const auto output = net.add_place();
        net.add_arc(fragment.source_transition, input);
        net.add_output_arc(fragment.sink_transition, output);
        inputs.push_back(input);
        outputs.push_back(output);
      }
      const auto source = net.add_transition(std::nullopt, {}, inputs);
      const auto sink = net.add_transition(std::nullopt, outputs, {});
      return {source, sink};
    }
    case ProcessTreeOperator::kLoop: {
      if (node.children.size() != 2) {
        throw std::invalid_argument(
            "transition-bordered process-tree conversion requires binary loops");
      }
      const auto body =
          build_transition_bordered(node.children[0], tree, net);
      const auto redo =
          build_transition_bordered(node.children[1], tree, net);
      const auto input = net.add_place();
      const auto output = net.add_place();
      net.add_arc(body.source_transition, input);
      net.add_output_arc(body.sink_transition, output);
      net.add_arc(redo.source_transition, output);
      net.add_output_arc(redo.sink_transition, input);
      const auto source = net.add_transition(std::nullopt, {}, {input});
      const auto sink = net.add_transition(std::nullopt, {output}, {});
      return {source, sink};
    }
  }
  throw std::invalid_argument("unknown process-tree operator");
}

}  // namespace

ProcessTreePetriNetConversion convert_process_tree_to_petri_net(
    const ProcessTree& tree) {
  return ReferenceTreeNetBuilder(tree).run();
}

PetriNet to_petri_net(const ProcessTree& tree) {
  return convert_process_tree_to_petri_net(tree).net;
}

PetriNet to_petri_net(const HeuristicsNet& heuristics) {
  const std::size_t n = heuristics.activities.size();
  PetriNet net;
  net.set_activities(heuristics.activities);
  const std::size_t source = net.add_place();
  const std::size_t sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);

  // One place per accepted dependency edge: a token on it means the edge has
  // been taken and the target is now enabled (or awaited).
  std::map<std::pair<std::size_t, std::size_t>, std::size_t> edge_place;
  for (const auto& edge : heuristics.edges) {
    edge_place[{edge.source, edge.target}] = net.add_place();
  }

  // A hand-built net may carry no bundle information; every edge then forms
  // its own bundle, i.e. plain XOR splits and joins.
  const bool has_bundles = heuristics.output_bundles.size() == n &&
                           heuristics.input_bundles.size() == n;

  // The alternatives of a's split (outgoing) or join: one entry per bundle,
  // holding the edge places the bundle touches.
  const auto alternatives = [&](std::size_t a, bool outgoing) {
    std::vector<std::vector<std::size_t>> alts;
    if (has_bundles) {
      const auto& bundles =
          outgoing ? heuristics.output_bundles[a] : heuristics.input_bundles[a];
      for (const auto& bundle : bundles) {
        std::vector<std::size_t>& places = alts.emplace_back();
        places.reserve(bundle.size());
        for (const std::size_t other : bundle) {
          places.push_back(outgoing ? edge_place.at({a, other})
                                    : edge_place.at({other, a}));
        }
      }
    } else {
      for (const auto& edge : heuristics.edges) {
        if (outgoing ? edge.source == a : edge.target == a) {
          alts.push_back({edge_place.at({edge.source, edge.target})});
        }
      }
    }
    return alts;
  };

  // Collapses a's alternatives into the place set its transition touches. A
  // single alternative wires directly (an AND split/join on the transition
  // itself); several alternatives meet at a hub place with one silent
  // transition per bundle, so exactly one bundle is taken. No input
  // alternative leaves the transition inputless; no output alternative gets
  // a fresh private place where the token parks instead of vanishing.
  const auto merge = [&](std::vector<std::vector<std::size_t>> alts,
                         bool joining) -> std::vector<std::size_t> {
    if (alts.empty()) {
      if (joining) return {};
      return {net.add_place()};
    }
    if (alts.size() == 1) {
      return std::move(alts.front());
    }
    const std::size_t hub = net.add_place();
    for (auto& places : alts) {
      if (joining) {
        net.add_transition(std::nullopt, std::move(places), {hub});
      } else {
        net.add_transition(std::nullopt, {hub}, std::move(places));
      }
    }
    return {hub};
  };

  const auto contains = [](const std::vector<std::size_t>& list, std::size_t a) {
    return std::find(list.begin(), list.end(), a) != list.end();
  };

  std::vector<std::size_t> active = heuristics.active_activities;
  // Legacy hand-built nets omitted active_activities and should continue to
  // use every registry entry.  Discovered DFG nets carry metadata even when
  // their explicit declaration intentionally leaves the active node set
  // empty; preserve that empty graph during conversion.
  if (active.empty() && !heuristics.metadata.has_value()) {
    active.resize(n);
    for (std::size_t i = 0; i < n; ++i) active[i] = i;
  }
  for (const std::size_t a : active) {
    auto in_alts = alternatives(a, false);
    if (contains(heuristics.start_activities, a)) {
      in_alts.push_back({source});  // starting is one more way to get enabled
    }
    auto out_alts = alternatives(a, true);
    if (contains(heuristics.end_activities, a)) {
      out_alts.push_back({sink});  // finishing is one more alternative
    }
    net.add_transition(a, merge(std::move(in_alts), true),
                       merge(std::move(out_alts), false));
  }
  PetriNetReductionOptions reduction_options;
  reduction_options.preserve_join_bridges = true;
  return reduce_petri_net(net, reduction_options).net;
}

PetriNetPresentation to_petri_net_presentation(const HeuristicsNet& heuristics) {

  if (heuristics.variant != HeuristicMinerVariant::kPlusPlus)
    throw std::invalid_argument("Petri presentation requires PLUSPLUS");
  const std::size_t n = heuristics.activities.size();
  PetriNet net;
  net.set_activities(heuristics.activities);
  std::vector<std::string> place_names, transition_names;
  std::size_t hidden_count = 0;
  const auto add_place = [&](std::string name) {
    const auto id = net.add_place();
    place_names.push_back(std::move(name));
    return id;
  };
  const auto add_transition = [&](std::string name,
                                  std::optional<std::size_t> activity,
                                  std::vector<std::size_t> inputs,
                                  std::vector<std::size_t> outputs) {
    const auto id = net.add_transition(activity, std::move(inputs),
                                      std::move(outputs));
    transition_names.push_back(std::move(name));
    return id;
  };
  std::vector<std::vector<std::size_t>> predecessors(n);
  for (const auto& edge : heuristics.edges) {
    if (edge.source >= n || edge.target >= n)
      throw std::invalid_argument("Petri dependency activity index");
    auto& incoming = predecessors[edge.target];
    if (std::find(incoming.begin(), incoming.end(), edge.source) == incoming.end())
      incoming.push_back(edge.source);
  }

  const std::size_t source = add_place("source0");
  const std::size_t sink = add_place("sink0");
  net.set_source(source);
  net.set_sink(sink);

  // One place per accepted dependency edge: a token on it means the edge has
  // been taken and the target is now enabled (or awaited).
  std::map<std::pair<std::size_t, std::size_t>, std::size_t> edge_place;
  for (const auto& edge : heuristics.edges) {
    edge_place[{edge.source, edge.target}] = add_place(
        predecessors[edge.target].size() <= 1
            ? "pre_" + heuristics.activities[edge.target]
            : "splace_in_" + heuristics.activities[edge.target] + "_" +
                  heuristics.activities[edge.source]);
  }

  // A hand-built net may carry no bundle information; every edge then forms
  // its own bundle, i.e. plain XOR splits and joins.
  const bool has_bundles = heuristics.output_bundles.size() == n &&
                           heuristics.input_bundles.size() == n;

  // The alternatives of a's split (outgoing) or join: one entry per bundle,
  // holding the edge places the bundle touches.
  const auto alternatives = [&](std::size_t a, bool outgoing) {
    std::vector<std::vector<std::size_t>> alts;
    if (has_bundles) {
      const auto& bundles =
          outgoing ? heuristics.output_bundles[a] : heuristics.input_bundles[a];
      for (const auto& bundle : bundles) {
        std::vector<std::size_t>& places = alts.emplace_back();
        places.reserve(bundle.size());
        for (const std::size_t other : bundle) {
          places.push_back(outgoing ? edge_place.at({a, other})
                                    : edge_place.at({other, a}));
        }
      }
    } else {
      for (const auto& edge : heuristics.edges) {
        if (outgoing ? edge.source == a : edge.target == a) {
          alts.push_back({edge_place.at({edge.source, edge.target})});
        }
      }
    }
    return alts;
  };

  // Collapses a's alternatives into the place set its transition touches. A
  // single alternative wires directly (an AND split/join on the transition
  // itself); several alternatives meet at a hub place with one silent
  // transition per bundle, so exactly one bundle is taken. No input
  // alternative leaves the transition inputless; no output alternative gets
  // a fresh private place where the token parks instead of vanishing.
  const auto merge = [&](std::vector<std::vector<std::size_t>> alts,
                         bool joining, std::size_t a) -> std::vector<std::size_t> {
    if (alts.empty()) {
      if (joining) return {};
      // Provisional parking name: this domain has no retained public control.
      return {add_place("intplace_" + heuristics.activities.at(a))};
    }
    if (alts.size() == 1) {
      return std::move(alts.front());
    }
    const std::size_t hub = add_place(
        (joining ? "pre_" : "intplace_") + heuristics.activities.at(a));
    for (auto& places : alts) {
      if (joining) {
        add_transition("hid_" + std::to_string(++hidden_count), std::nullopt, std::move(places), {hub});
      } else {
        add_transition("hid_" + std::to_string(++hidden_count), std::nullopt, {hub}, std::move(places));
      }
    }
    return {hub};
  };

  const auto contains = [](const std::vector<std::size_t>& list, std::size_t a) {
    return std::find(list.begin(), list.end(), a) != list.end();
  };

  std::vector<std::size_t> active = heuristics.active_activities;
  // Legacy hand-built nets omitted active_activities and should continue to
  // use every registry entry.  Discovered DFG nets carry metadata even when
  // their explicit declaration intentionally leaves the active node set
  // empty; preserve that empty graph during conversion.
  if (active.empty() && !heuristics.metadata.has_value()) {
    active.resize(n);
    for (std::size_t i = 0; i < n; ++i) active[i] = i;
  }
  // Public silent names count every join before every split, including
  // transitions removed by reduction. Visit each node's own join before its
  // name-ordered outgoing targets, retaining the first visit of each domain.
  // Splits follow the corresponding incoming relation traversal.
  for (const auto a : active)
    if (a >= n) throw std::invalid_argument("Petri active activity index");
  const auto name_order = [&](std::size_t lhs, std::size_t rhs) {
    return heuristics.activities[lhs] < heuristics.activities[rhs];
  };
  std::vector<std::vector<std::size_t>> successors(n);
  for (const auto& edge : heuristics.edges)
    successors[edge.source].push_back(edge.target);
  for (auto& neighbours : successors) {
    std::sort(neighbours.begin(), neighbours.end(), name_order);
    neighbours.erase(std::unique(neighbours.begin(), neighbours.end()), neighbours.end());
  }
  for (auto& neighbours : predecessors) std::sort(neighbours.begin(), neighbours.end(), name_order);
  const auto append_active = [&](std::vector<std::size_t>& order, std::size_t a) {
    if (contains(active, a) && !contains(order, a)) order.push_back(a);
  };
  std::vector<std::size_t> join_order, split_order;
  for (const auto a : active) {
    append_active(join_order, a);
    for (const auto target : successors[a]) append_active(join_order, target);
  }
  for (const auto a : active) append_active(join_order, a);
  for (const auto a : join_order)
    for (const auto predecessor : predecessors[a]) append_active(split_order, predecessor);
  for (const auto a : active) append_active(split_order, a);
  std::vector<std::vector<std::size_t>> inputs(n), outputs(n);
  for (const std::size_t a : join_order) {
    auto in_alts = alternatives(a, false);
    if (contains(heuristics.start_activities, a))
      in_alts.push_back({source});
    inputs[a] = merge(std::move(in_alts), true, a);
  }
  for (const std::size_t a : split_order) {
    auto out_alts = alternatives(a, true);
    if (contains(heuristics.end_activities, a))
      out_alts.push_back({sink});
    outputs[a] = merge(std::move(out_alts), false, a);
  }
  for (const std::size_t a : active) {
    add_transition(heuristics.activities.at(a), a, std::move(inputs[a]),
                   std::move(outputs[a]));
  }
  PetriNetReductionOptions reduction_options;
  reduction_options.preserve_join_bridges = true;

  const auto reduced = reduce_petri_net(net, reduction_options);
  // The reducer exposes alias classes, not a distinguished metadata survivor.
  // Select the lowest original index deterministically when names differ.
  // Public name-survivor parity for such aliases is not yet established.
  const auto project = [](const std::vector<std::string>& names,
                          const std::vector<std::size_t>& mapping,
                          std::size_t count, std::size_t removed) {
    if (names.size() != mapping.size())
      throw std::logic_error("Petri presentation metadata mapping cardinality");
    std::vector<std::optional<std::string>> slots(count);
    for (std::size_t old = 0; old < mapping.size(); ++old) {
      const auto id = mapping[old];
      if (id == removed) continue;
      if (id >= count) throw std::logic_error("Petri presentation mapped index");
      if (!slots[id]) slots[id] = names[old];
    }
    std::vector<std::string> result;
    result.reserve(count);
    for (auto& value : slots) {
      if (!value) throw std::logic_error("Petri presentation missing metadata");
      result.push_back(std::move(*value));
    }
    return result;
  };
  const auto final_places = project(place_names, reduced.old_to_new_place,
                                   reduced.net.place_count(), kRemovedPlace);
  const auto final_transitions = project(
      transition_names, reduced.old_to_new_transition,
      reduced.net.transitions().size(), kRemovedTransition);
  PetriNetPresentation result;
  for (std::size_t p = 0; p < final_places.size(); ++p)
    result.places.push_back({p, final_places[p], {}});
  for (std::size_t t = 0; t < reduced.net.transitions().size(); ++t) {
    const auto& transition = reduced.net.transitions()[t];
    std::optional<std::string> label;
    if (transition.activity)
      label = reduced.net.activities().at(*transition.activity);
    result.transitions.push_back({t, final_transitions[t], std::move(label), {}});
    for (const auto p : transition.inputs)
      result.arcs.push_back({{PetriNodeKind::kPlace, p},
                             {PetriNodeKind::kTransition, t}, 1, {}});
    for (const auto p : transition.outputs)
      result.arcs.push_back({{PetriNodeKind::kTransition, t},
                             {PetriNodeKind::kPlace, p}, 1, {}});
    if (!transition.reset_inputs.empty() || !transition.inhibitor_inputs.empty())
      throw std::logic_error("Heuristic presentation has extended arcs");
  }
  if (reduced.net.has_source()) result.initial_marking.push_back({reduced.net.source(), 1});
  if (reduced.net.has_sink()) result.final_marking.push_back({reduced.net.sink(), 1});
  return result;
}

PetriNet to_petri_net_transition_bordered(const ProcessTree& tree) {
  PetriNet net;
  net.set_activities(tree.activities());
  // Keep the workflow boundary place ids stable with the ordinary
  // conversion: callers can use source() and sink() without remapping.
  const std::size_t source = net.add_place();
  const std::size_t sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);

  const auto fragment = build_transition_bordered(tree.root(), tree, net);
  net.add_arc(fragment.source_transition, source);
  net.add_output_arc(fragment.sink_transition, sink);
  return net;
}

PetriNet to_petri_net(const ProcessTree& tree,
                     ProcessTreePetriNetVariant variant) {
  switch (variant) {
    case ProcessTreePetriNetVariant::kStandard:
      return to_petri_net(tree);
    case ProcessTreePetriNetVariant::kTransitionBordered:
      return to_petri_net_transition_bordered(tree);
  }
  throw std::invalid_argument("unknown process-tree Petri-net variant");
}

const char* process_tree_petri_net_variant_name(
    ProcessTreePetriNetVariant variant) noexcept {
  switch (variant) {
    case ProcessTreePetriNetVariant::kStandard:
      return "standard";
    case ProcessTreePetriNetVariant::kTransitionBordered:
      return "transition_bordered";
  }
  return "unknown";
}

ProcessTreePetriNetVariant parse_process_tree_petri_net_variant(
    std::string_view value) {
  if (value.empty() || value == "standard")
    return ProcessTreePetriNetVariant::kStandard;
  if (value == "transition_bordered")
    return ProcessTreePetriNetVariant::kTransitionBordered;
  throw std::invalid_argument(
      "process_tree_variant: expected standard or transition_bordered");
}

namespace {

void write_json_string(std::ostringstream& out, std::string_view value) {
  out << '"';
  for (const unsigned char c : value) {
    switch (c) {
      case '"': out << "\\\""; break;
      case '\\': out << "\\\\"; break;
      case '\b': out << "\\b"; break;
      case '\f': out << "\\f"; break;
      case '\n': out << "\\n"; break;
      case '\r': out << "\\r"; break;
      case '\t': out << "\\t"; break;
      default:
        if (c < 0x20) {
          static constexpr char hex[] = "0123456789abcdef";
          out << "\\u00" << hex[c >> 4] << hex[c & 0x0f];
        } else {
          out << static_cast<char>(c);
        }
    }
  }
  out << '"';
}

void write_places(std::ostringstream& out,
                  const std::vector<std::size_t>& places) {
  out << '[';
  for (std::size_t i = 0; i < places.size(); ++i) {
    if (i != 0) out << ',';
    out << places[i];
  }
  out << ']';
}

}  // namespace

std::string serialize_process_tree_petri_net_json(
    const PetriNet& net, ProcessTreePetriNetVariant variant) {
  if (variant != ProcessTreePetriNetVariant::kStandard &&
      variant != ProcessTreePetriNetVariant::kTransitionBordered) {
    throw std::invalid_argument("unknown process-tree Petri-net variant");
  }
  std::ostringstream out;
  out << "{\"family\":\"PROCESS_TREE_PETRI_NET\",\"variant\":";
  write_json_string(out, process_tree_petri_net_variant_name(variant));
  out << ",\"place_count\":" << net.place_count()
      << ",\"source\":" << net.source()
      << ",\"sink\":" << net.sink() << ",\"activities\":[";
  for (std::size_t i = 0; i < net.activities().size(); ++i) {
    if (i != 0) out << ',';
    write_json_string(out, net.activities()[i]);
  }
  out << "],\"transitions\":[";
  for (std::size_t i = 0; i < net.transitions().size(); ++i) {
    if (i != 0) out << ',';
    const auto& transition = net.transitions()[i];
    out << "{\"activity\":";
    if (transition.activity.has_value()) out << *transition.activity;
    else out << "null";
    out << ",\"inputs\":";
    write_places(out, transition.inputs);
    out << ",\"outputs\":";
    write_places(out, transition.outputs);
    out << '}';
  }
  out << "]}";
  return out.str();
}

}  // namespace minera
