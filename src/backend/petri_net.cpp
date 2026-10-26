#include "minera/petri_net.hpp"
#include "minera/petri_net_reduction.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace minera {
namespace {

struct BorderedFragment {
  std::size_t source_transition = 0;
  std::size_t sink_transition = 0;
};

// Wires the fragment for `node` between the given entry and exit places,
// appending the transitions (and any intermediate places) to `net`.
void build(const ProcessTreeNode& node,
           std::size_t in,
           std::size_t out,
           PetriNet& net);

BorderedFragment build_transition_bordered(const ProcessTreeNode& node,
                                           const ProcessTree& tree,
                                           PetriNet& net);

void build_inclusive_or(const ProcessTreeNode& node,
                        std::size_t in,
                        std::size_t out,
                        PetriNet& net) {
  // Split one control token into one selector token per child. Each selector
  // can skip its child or start it. The first selected child moves the state
  // token from `not_started` to `started`; later selected children preserve
  // that token. Requiring `started` at the join excludes the empty subset
  // without enumerating all 2^n choices.
  if (node.children.empty()) {
    throw std::invalid_argument(
        "process-tree inclusive OR requires at least one child");
  }
  const std::size_t not_started = net.add_place();
  const std::size_t started = net.add_place();
  std::vector<std::size_t> selectors;
  std::vector<std::size_t> completions;
  selectors.reserve(node.children.size());
  completions.reserve(node.children.size());

  for (const auto& child : node.children) {
    const std::size_t selector = net.add_place();
    const std::size_t child_in = net.add_place();
    const std::size_t child_out = net.add_place();
    const std::size_t completion = net.add_place();
    selectors.push_back(selector);
    completions.push_back(completion);

    build(child, child_in, child_out, net);
    net.add_transition(std::nullopt, {child_out}, {completion});
    net.add_transition(std::nullopt, {selector}, {completion});
    net.add_transition(std::nullopt, {selector, not_started},
                       {child_in, started});
    net.add_transition(std::nullopt, {selector, started},
                       {child_in, started});
  }

  std::vector<std::size_t> split_outputs = selectors;
  split_outputs.push_back(not_started);
  net.add_transition(std::nullopt, {in}, std::move(split_outputs));

  std::vector<std::size_t> join_inputs = completions;
  join_inputs.push_back(started);
  net.add_transition(std::nullopt, std::move(join_inputs), {out});
}

void build(const ProcessTreeNode& node,
           std::size_t in,
           std::size_t out,
           PetriNet& net) {
  switch (node.op) {
    case ProcessTreeOperator::kActivity:
      net.add_transition(node.activity, {in}, {out});
      return;
    case ProcessTreeOperator::kTau:
      net.add_transition(std::nullopt, {in}, {out});
      return;
    case ProcessTreeOperator::kSequence: {
      std::size_t prev = in;
      for (std::size_t i = 0; i < node.children.size(); ++i) {
        const std::size_t next =
            i + 1 < node.children.size() ? net.add_place() : out;
        build(node.children[i], prev, next, net);
        prev = next;
      }
      return;
    }
    case ProcessTreeOperator::kExclusive:
      // Every branch shares the entry and exit places, so the single entry
      // token enables exactly one branch's first transition.
      for (const auto& child : node.children) {
        build(child, in, out, net);
      }
      return;
    case ProcessTreeOperator::kInclusiveChoice:
      build_inclusive_or(node, in, out, net);
      return;
    case ProcessTreeOperator::kParallel: {
      // Silent AND-split forks a token into every branch; the AND-join waits
      // for all branches before producing the exit token.
      std::vector<std::size_t> branch_in;
      std::vector<std::size_t> branch_out;
      branch_in.reserve(node.children.size());
      branch_out.reserve(node.children.size());
      for (const auto& child : node.children) {
        const std::size_t ci = net.add_place();
        const std::size_t co = net.add_place();
        build(child, ci, co, net);
        branch_in.push_back(ci);
        branch_out.push_back(co);
      }
      net.add_transition(std::nullopt, {in}, branch_in);
      net.add_transition(std::nullopt, branch_out, {out});
      return;
    }
    case ProcessTreeOperator::kInterleaving: {
      if (node.children.empty()) {
        net.add_transition(std::nullopt, {in}, {out});
        return;
      }
      if (node.children.size() >= std::numeric_limits<std::size_t>::digits)
        throw std::invalid_argument("process-tree interleaving has too many children");
      const std::size_t states = std::size_t{1} << node.children.size();
      std::vector<std::size_t> state(states);
      state.front() = in;
      state.back() = out;
      for (std::size_t mask = 1; mask + 1 < states; ++mask)
        state[mask] = net.add_place();
      for (std::size_t mask = 0; mask < states; ++mask) {
        for (std::size_t child = 0; child < node.children.size(); ++child) {
          const auto bit = std::size_t{1} << child;
          if ((mask & bit) == 0)
            build(node.children[child], state[mask], state[mask | bit], net);
        }
      }
      return;
    }
    case ProcessTreeOperator::kLoop: {
      // child[0] is the body (executed at least once between in and out);
      // child[1..] are redo paths that lead from the exit back to the entry.
      if (node.children.empty()) {
        net.add_transition(std::nullopt, {in}, {out});
        return;
      }
      // Both loop borders are private places behind silent transitions. A
      // shared exit would let a sibling or a preceding fragment that also
      // produces `out` start a redo path, so XOR(LOOP(A, B), C) would admit
      // C, B, A.
      const std::size_t loop_entry = net.add_place();
      const std::size_t loop_exit = net.add_place();
      net.add_transition(std::nullopt, {in}, {loop_entry});
      build(node.children[0], loop_entry, loop_exit, net);
      for (std::size_t i = 1; i < node.children.size(); ++i) {
        build(node.children[i], loop_exit, loop_entry, net);
      }
      net.add_transition(std::nullopt, {loop_exit}, {out});
      return;
    }
  }
}

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

PetriNet to_petri_net(const ProcessTree& tree) {
  PetriNet net;
  net.set_activities(tree.activities());
  const std::size_t source = net.add_place();
  const std::size_t sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  build(tree.root(), source, sink, net);
  return reduce_petri_net(net).net;
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
