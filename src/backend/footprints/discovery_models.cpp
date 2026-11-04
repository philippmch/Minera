#include "discovery_registry.hpp"

#include <algorithm>
#include <cstdint>
#include <deque>
#include <iterator>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace minera::footprints_detail {
namespace {

using PairSet = std::set<FootprintPair>;

struct ModelFootprint {
  PairSet sequence;
  PairSet parallel;
  PairSet atomic_interleaving;
  std::set<std::string> starts;
  std::set<std::string> ends;
  std::set<std::string> activities;
  std::set<std::string> always_happening;
  bool skippable = true;
  std::size_t min_length = 0;
  std::optional<std::size_t> max_length = std::size_t{0};
};

void fix_relations(Footprint& footprint) {
  for (const auto& pair : footprint.parallel)
    footprint.sequence.erase(pair);
  std::vector<FootprintPair> conflicts;
  for (const auto& pair : footprint.sequence) {
    if (footprint.sequence.find({pair.second, pair.first}) !=
            footprint.sequence.end() &&
        footprint.atomic_interleaving.find(pair) ==
            footprint.atomic_interleaving.end())
      conflicts.push_back(pair);
  }
  for (const auto& pair : conflicts) {
    footprint.sequence.erase(pair);
    footprint.parallel.insert(pair);
  }
}

void fix_relations(ModelFootprint& footprint) {
  for (const auto& pair : footprint.parallel)
    footprint.sequence.erase(pair);
  std::vector<FootprintPair> conflicts;
  for (const auto& pair : footprint.sequence) {
    if (footprint.sequence.find({pair.second, pair.first}) !=
            footprint.sequence.end() &&
        footprint.atomic_interleaving.find(pair) ==
            footprint.atomic_interleaving.end())
      conflicts.push_back(pair);
  }
  for (const auto& pair : conflicts) {
    footprint.sequence.erase(pair);
    footprint.parallel.insert(pair);
  }
}

Footprint to_public(const ModelFootprint& model, bool include_max) {
  Footprint result;
  result.dfg_present = false;
  result.sequence = model.sequence;
  result.parallel = model.parallel;
  result.atomic_interleaving = model.atomic_interleaving;
  result.start_activities = model.starts;
  result.end_activities = model.ends;
  result.activities = model.activities;
  result.skippable = model.skippable;
  result.activities_always_happening = model.always_happening;
  result.min_trace_length = model.min_length;
  result.max_trace_length_without_loops =
      include_max ? model.max_length : std::nullopt;
  fix_relations(result);
  return result;
}

ModelFootprint merge_parallel(const std::vector<ModelFootprint>& children) {
  ModelFootprint result;
  result.skippable = true;
  result.max_length = std::size_t{0};
  for (const auto& child : children) {
    result.starts.insert(child.starts.begin(), child.starts.end());
    result.ends.insert(child.ends.begin(), child.ends.end());
    result.activities.insert(child.activities.begin(), child.activities.end());
    result.sequence.insert(child.sequence.begin(), child.sequence.end());
    result.parallel.insert(child.parallel.begin(), child.parallel.end());
    result.atomic_interleaving.insert(child.atomic_interleaving.begin(),
                                      child.atomic_interleaving.end());
    result.skippable = result.skippable && child.skippable;
    result.min_length += child.min_length;
    if (child.max_length) {
      if (result.max_length) *result.max_length += *child.max_length;
    } else {
      result.max_length.reset();
    }
    if (!child.skippable)
      result.always_happening.insert(child.always_happening.begin(),
                                     child.always_happening.end());
  }
  for (std::size_t i = 0; i < children.size(); ++i) {
    for (std::size_t j = i + 1; j < children.size(); ++j) {
      for (const auto& left : children[i].activities) {
        for (const auto& right : children[j].activities) {
          result.parallel.insert({left, right});
          result.parallel.insert({right, left});
        }
      }
    }
  }
  fix_relations(result);
  return result;
}

ModelFootprint merge_xor(const std::vector<ModelFootprint>& children) {
  ModelFootprint result;
  result.skippable = false;
  bool first = true;
  for (const auto& child : children) {
    result.starts.insert(child.starts.begin(), child.starts.end());
    result.ends.insert(child.ends.begin(), child.ends.end());
    result.activities.insert(child.activities.begin(), child.activities.end());
    result.sequence.insert(child.sequence.begin(), child.sequence.end());
    result.parallel.insert(child.parallel.begin(), child.parallel.end());
    result.atomic_interleaving.insert(child.atomic_interleaving.begin(),
                                      child.atomic_interleaving.end());
    result.skippable = result.skippable || child.skippable;
    if (first) {
      result.min_length = child.min_length;
      result.max_length = child.max_length;
      result.always_happening = child.always_happening;
      first = false;
    } else {
      result.min_length = std::min(result.min_length, child.min_length);
      if (result.max_length && child.max_length) {
        *result.max_length = std::max(*result.max_length, *child.max_length);
      } else {
        result.max_length.reset();
      }
      if (!child.skippable) {
        std::set<std::string> intersection;
        std::set_intersection(
            result.always_happening.begin(), result.always_happening.end(),
            child.always_happening.begin(), child.always_happening.end(),
            std::inserter(intersection, intersection.end()));
        result.always_happening = std::move(intersection);
      }
    }
    if (child.skippable) result.always_happening.clear();
  }
  fix_relations(result);
  return result;
}

ModelFootprint model_tree(const ProcessTreeNode& node,
                          const std::vector<std::string>& names) {
  if (node.op == ProcessTreeOperator::kTau) return ModelFootprint{};
  if (node.op == ProcessTreeOperator::kActivity) {
    ModelFootprint result;
    const auto label = node.activity < names.size()
                           ? names[node.activity]
                           : std::to_string(node.activity);
    result.starts.insert(label);
    result.ends.insert(label);
    result.activities.insert(label);
    result.always_happening.insert(label);
    result.skippable = false;
    result.min_length = 1;
    result.max_length = 1;
    return result;
  }

  std::vector<ModelFootprint> children;
  children.reserve(node.children.size());
  for (const auto& child : node.children)
    children.push_back(model_tree(child, names));

  if (node.op == ProcessTreeOperator::kExclusive)
    return merge_xor(children);
  // The OR operator in the footprint algebra has the same mandatory-child
  // summary as parallel composition.  In particular, OR(A,B) is not treated
  // as a one-of choice: both activities happen and may interleave.
  if (node.op == ProcessTreeOperator::kInclusiveChoice)
    return merge_parallel(children);
  if (node.op == ProcessTreeOperator::kParallel)
    return merge_parallel(children);
  if (node.op == ProcessTreeOperator::kInterleaving) {
    ModelFootprint result;
    result.skippable = true;
    result.max_length = std::size_t{0};
    for (const auto& child : children) {
      result.activities.insert(child.activities.begin(), child.activities.end());
      result.starts.insert(child.starts.begin(), child.starts.end());
      result.ends.insert(child.ends.begin(), child.ends.end());
      result.sequence.insert(child.sequence.begin(), child.sequence.end());
      result.parallel.insert(child.parallel.begin(), child.parallel.end());
      result.atomic_interleaving.insert(child.atomic_interleaving.begin(),
                                        child.atomic_interleaving.end());
      result.skippable = result.skippable && child.skippable;
      result.min_length += child.min_length;
      if (result.max_length && child.max_length) {
        *result.max_length += *child.max_length;
      } else {
        result.max_length.reset();
      }
      if (!child.skippable)
        result.always_happening.insert(child.always_happening.begin(),
                                       child.always_happening.end());
    }
    for (std::size_t i = 0; i < children.size(); ++i) {
      for (std::size_t j = 0; j < children.size(); ++j) {
        if (i == j) continue;
        for (const auto& left : children[i].ends) {
          for (const auto& right : children[j].starts) {
            result.sequence.insert({left, right});
            result.atomic_interleaving.insert({left, right});
          }
        }
      }
    }
    return result;
  }
  if (node.op == ProcessTreeOperator::kSequence) {
    ModelFootprint result;
    result.skippable = true;
    result.max_length = std::size_t{0};
    for (const auto& child : children) {
      result.activities.insert(child.activities.begin(), child.activities.end());
      result.sequence.insert(child.sequence.begin(), child.sequence.end());
      result.parallel.insert(child.parallel.begin(), child.parallel.end());
      result.atomic_interleaving.insert(child.atomic_interleaving.begin(),
                                        child.atomic_interleaving.end());
      result.skippable = result.skippable && child.skippable;
      result.min_length += child.min_length;
      if (result.max_length && child.max_length) {
        *result.max_length += *child.max_length;
      } else {
        result.max_length.reset();
      }
      if (!child.skippable)
        result.always_happening.insert(child.always_happening.begin(),
                                       child.always_happening.end());
    }
    for (std::size_t i = 0; i < children.size(); ++i) {
      for (std::size_t j = i + 1; j < children.size(); ++j) {
        for (const auto& left : children[i].ends)
          for (const auto& right : children[j].starts)
            result.sequence.insert({left, right});
        if (!children[j].skippable) break;
      }
    }
    for (const auto& child : children) {
      result.starts.insert(child.starts.begin(), child.starts.end());
      if (!child.skippable) break;
    }
    for (std::size_t i = children.size(); i-- > 0;) {
      result.ends.insert(children[i].ends.begin(), children[i].ends.end());
      if (!children[i].skippable) break;
    }
    fix_relations(result);
    return result;
  }

  // A loop's first child is the body; remaining children are alternative
  // redo paths.  Redo repetition is intentionally excluded from the finite
  // maximum trace length summary.
  ModelFootprint result;
  if (children.empty()) return result;
  const auto& body = children.front();
  ModelFootprint redo;
  if (children.size() == 1) {
    redo = ModelFootprint{};
  } else if (children.size() == 2) {
    redo = children[1];
  } else {
    std::vector<ModelFootprint> rest(children.begin() + 1, children.end());
    redo = merge_xor(rest);
  }
  result = body;
  result.activities.insert(redo.activities.begin(), redo.activities.end());
  result.sequence.insert(redo.sequence.begin(), redo.sequence.end());
  result.parallel.insert(redo.parallel.begin(), redo.parallel.end());
  result.atomic_interleaving.insert(redo.atomic_interleaving.begin(),
                                    redo.atomic_interleaving.end());
  result.skippable = body.skippable;
  result.min_length = body.min_length;
  result.always_happening = body.skippable
                                ? std::set<std::string>{}
                                : body.always_happening;
  result.starts = body.starts;
  result.ends = body.ends;
  if (body.skippable) {
    result.starts.insert(redo.starts.begin(), redo.starts.end());
    result.ends.insert(redo.ends.begin(), redo.ends.end());
  }
  for (const auto& left : body.ends)
    for (const auto& right : redo.starts)
      result.sequence.insert({left, right});
  for (const auto& left : redo.ends)
    for (const auto& right : body.starts)
      result.sequence.insert({left, right});
  if (body.skippable)
    for (const auto& left : redo.ends)
      for (const auto& right : redo.starts)
        result.sequence.insert({left, right});
  if (redo.skippable)
    for (const auto& left : body.ends)
      for (const auto& right : body.starts)
        result.sequence.insert({left, right});
  fix_relations(result);
  return result;
}

ModelFootprint model_powl(const PowlNode& node) {
  if (node.kind() == PowlNodeKind::kSilent) return ModelFootprint{};
  if (node.kind() == PowlNodeKind::kTransition) {
    ModelFootprint result;
    result.skippable = node.skippable();
    result.min_length = node.frequency().minimum > 0
                            ? static_cast<std::size_t>(node.frequency().minimum)
                            : 0;
    if (node.frequency().maximum)
      result.max_length = static_cast<std::size_t>(*node.frequency().maximum);
    else
      result.max_length.reset();
    if (node.self_loop()) result.sequence.insert({node.label(), node.label()});
    result.starts.insert(node.label());
    result.ends.insert(node.label());
    result.activities.insert(node.label());
    if (!node.skippable()) result.always_happening.insert(node.label());
    return result;
  }

  const auto& children = node.children();
  if (node.kind() == PowlNodeKind::kXor) {
    std::vector<ModelFootprint> values;
    values.reserve(children.size());
    for (const auto& child : children) values.push_back(model_powl(child));
    return merge_xor(values);
  }
  if (node.kind() == PowlNodeKind::kLoop) {
    std::vector<ModelFootprint> values;
    values.reserve(children.size());
    for (const auto& child : children) values.push_back(model_powl(child));
    if (values.empty()) return ModelFootprint{};
    ModelFootprint result = values.front();
    if (values.size() > 1) {
      const auto& redo = values[1];
      result.activities.insert(redo.activities.begin(), redo.activities.end());
      result.sequence.insert(redo.sequence.begin(), redo.sequence.end());
      result.parallel.insert(redo.parallel.begin(), redo.parallel.end());
      result.atomic_interleaving.insert(redo.atomic_interleaving.begin(),
                                        redo.atomic_interleaving.end());
      result.skippable = values.front().skippable;
      result.always_happening = values.front().skippable
                                    ? std::set<std::string>{}
                                    : values.front().always_happening;
      if (values.front().skippable) {
        result.starts.insert(redo.starts.begin(), redo.starts.end());
        result.ends.insert(redo.ends.begin(), redo.ends.end());
      }
      for (const auto& left : values.front().ends)
        for (const auto& right : redo.starts)
          result.sequence.insert({left, right});
      for (const auto& left : redo.ends)
        for (const auto& right : values.front().starts)
          result.sequence.insert({left, right});
      if (values.front().skippable)
        for (const auto& left : redo.ends)
          for (const auto& right : redo.starts)
            result.sequence.insert({left, right});
      if (redo.skippable)
        for (const auto& left : values.front().ends)
          for (const auto& right : values.front().starts)
            result.sequence.insert({left, right});
    }
    fix_relations(result);
    return result;
  }

  // Strict partial order composition.  The order relation is already
  // canonicalized by PowlNode; closure is rebuilt locally so optional nodes
  // can be handled without relying on child positions being contiguous.
  std::vector<ModelFootprint> values;
  values.reserve(children.size());
  for (const auto& child : children) values.push_back(model_powl(child));
  ModelFootprint result;
  result.skippable = true;
  for (const auto& value : values) {
    result.activities.insert(value.activities.begin(), value.activities.end());
    result.sequence.insert(value.sequence.begin(), value.sequence.end());
    result.parallel.insert(value.parallel.begin(), value.parallel.end());
    result.atomic_interleaving.insert(value.atomic_interleaving.begin(),
                                      value.atomic_interleaving.end());
    result.skippable = result.skippable && value.skippable;
    result.min_length += value.min_length;
    if (value.max_length) {
      if (!result.max_length) result.max_length = 0;
      *result.max_length += *value.max_length;
    } else {
      result.max_length.reset();
    }
    if (!value.skippable)
      result.always_happening.insert(value.always_happening.begin(),
                                     value.always_happening.end());
  }
  const std::size_t child_count = children.size();
  std::vector<std::set<std::size_t>> closure(child_count);
  for (std::size_t i = 0; i < child_count; ++i) {
    std::deque<std::size_t> todo;
    for (const auto& edge : node.order())
      if (edge.first == i) todo.push_back(edge.second);
    while (!todo.empty()) {
      const auto current = todo.front();
      todo.pop_front();
      if (!closure[i].insert(current).second) continue;
      for (const auto& edge : node.order())
        if (edge.first == current) todo.push_back(edge.second);
    }
  }
  for (std::size_t i = 0; i < child_count; ++i) {
    bool is_start = true;
    bool is_end = true;
    for (std::size_t j = 0; j < child_count; ++j) {
      if (i == j) continue;
      if (!values[j].skippable && closure[j].contains(i)) is_start = false;
      if (!values[j].skippable && closure[i].contains(j)) is_end = false;
    }
    if (is_start) result.starts.insert(values[i].starts.begin(),
                                       values[i].starts.end());
    if (is_end)
      result.ends.insert(values[i].ends.begin(), values[i].ends.end());
  }
  for (const auto& edge : node.order()) {
    bool redundant = false;
    for (std::size_t middle = 0; middle < child_count; ++middle) {
      if (middle == edge.first || middle == edge.second) continue;
      if (closure[edge.first].contains(middle) &&
          closure[middle].contains(edge.second)) {
        redundant = true;
        break;
      }
    }
    if (redundant) continue;
    for (const auto& left : values[edge.first].ends)
      for (const auto& right : values[edge.second].starts)
        result.sequence.insert({left, right});
  }
  for (std::size_t i = 0; i < child_count; ++i) {
    for (std::size_t j = 0; j < child_count; ++j) {
      if (i == j || !closure[i].contains(j)) continue;
      bool only_skippable_middle = true;
      for (std::size_t middle = 0; middle < child_count; ++middle) {
        if (middle == i || middle == j) continue;
        if (!values[middle].skippable && closure[i].contains(middle) &&
            closure[middle].contains(j)) {
          only_skippable_middle = false;
          break;
        }
      }
      if (!only_skippable_middle) continue;
      for (const auto& left : values[i].ends)
        for (const auto& right : values[j].starts)
          result.sequence.insert({left, right});
    }
  }
  for (std::size_t i = 0; i < child_count; ++i) {
    for (std::size_t j = i + 1; j < child_count; ++j) {
      if (closure[i].contains(j) || closure[j].contains(i)) continue;
      for (const auto& left : values[i].activities)
        for (const auto& right : values[j].activities) {
          result.parallel.insert({left, right});
          result.parallel.insert({right, left});
        }
    }
  }
  fix_relations(result);
  return result;
}

using Marking = std::vector<std::uint64_t>;
struct ReachEdge {
  std::size_t transition = 0;
  std::size_t target = 0;
};
struct ReachState {
  Marking marking;
  std::vector<ReachEdge> edges;
};
struct ReachGraph {
  std::vector<ReachState> states;
};

// Extended arcs break the monotonicity argument used by ordinary Petri-net
// coverability.  Their exact graph is still explored, but an infinite graph
// is not a finite footprint input and is rejected at a deterministic safety
// bound rather than being allowed to consume the process indefinitely.
constexpr std::size_t kMaxExtendedReachabilityStates = 100000;

bool dominates(const Marking& left, const Marking& right) {
  bool strictly = false;
  for (std::size_t i = 0; i < left.size(); ++i) {
    if (left[i] < right[i]) return false;
    strictly = strictly || left[i] > right[i];
  }
  return strictly;
}

ReachGraph build_reachability(const PetriNet& net,
                              const DiscoveryParameters& parameters) {
  const std::size_t places = net.place_count();
  Marking initial(places, 0);
  if (parameters.initial_marking) {
    if (parameters.initial_marking->size() != places)
      throw std::invalid_argument(
          "Petri initial marking length does not match place count");
    initial = *parameters.initial_marking;
  } else if (net.has_source() && net.source() < places) {
    initial[net.source()] = 1;
  }

  // Fires like petri_transition_enabled checks: each ordinary arc moves its
  // weight (the reference's semantics.execute), parallel arcs each their own.
  auto fire = [places](Marking marking, const PetriTransition& transition) {
    for (const auto* side : {&transition.inputs, &transition.outputs, &transition.reset_inputs})
      for (const auto place : *side)
        if (place >= places) throw std::runtime_error("Petri reachability fired a disabled transition");
    try {
      fire_petri_transition(transition, marking);
    } catch (const std::overflow_error&) {
      throw std::runtime_error("Petri reachability marking overflow");
    }
    return marking;
  };

  ReachGraph graph;
  graph.states.push_back({initial, {}});
  std::map<Marking, std::size_t> ids{{initial, 0}};
  struct Frame {
    std::size_t state = 0;
    std::size_t next_transition = 0;
  };
  std::vector<Frame> stack{{0, 0}};
  std::vector<Marking> ancestors{initial};
  while (!stack.empty()) {
    auto& frame = stack.back();
    if (frame.next_transition == net.transitions().size()) {
      stack.pop_back();
      ancestors.pop_back();
      continue;
    }
    const auto transition_index = frame.next_transition++;
    const auto& transition = net.transitions()[transition_index];
    const auto current = graph.states[frame.state].marking;
    if (!petri_transition_enabled(transition, current)) continue;
    const auto next = fire(current, transition);
    if (!net.has_extended_arcs()) {
      // Ancestor dominance is a sound unboundedness certificate only for
      // ordinary arcs. Inhibitor/reset guards can disable the same path after
      // a coordinate increases, so the certificate must not be used there.
      for (const auto& ancestor : ancestors) {
        if (dominates(next, ancestor))
          throw std::runtime_error(
              "Petri reachability graph is unbounded along an ancestor path");
      }
    }
    const auto [it, inserted] = ids.emplace(next, graph.states.size());
    if (inserted) {
      if (net.has_extended_arcs() &&
          graph.states.size() >= kMaxExtendedReachabilityStates)
        throw std::runtime_error(
            "extended Petri reachability graph exceeded its finite-state safety limit");
      graph.states.push_back({next, {}});
    }
    graph.states[frame.state].edges.push_back({transition_index, it->second});
    if (inserted) {
      ancestors.push_back(next);
      stack.push_back({it->second, 0});
    }
  }
  return graph;
}

}  // namespace

Footprint ProcessTreeBottomUpFootprintsDiscovery::apply(
    const ProcessTree& tree) {
  return to_public(model_tree(tree.root(), tree.activities()), true);
}

Footprint PowlBottomUpFootprintsDiscovery::apply(const PowlNode& model) {
  return to_public(model_powl(model), false);
}

Footprint PetriReachabilityFootprintsDiscovery::apply(
    const PetriNet& net, const DiscoveryParameters& parameters) {
  Footprint result;
  result.dfg_present = false;
  result.end_activities_present = false;
  result.min_trace_length_present = false;
  result.min_trace_length.reset();
  result.skippable.reset();
  result.activities_always_happening.reset();
  const auto graph = build_reachability(net, parameters);
  const auto state_count = graph.states.size();
  std::vector<std::set<std::size_t>> incoming(state_count);
  for (std::size_t source = 0; source < state_count; ++source) {
    for (const auto& edge : graph.states[source].edges) {
      if (net.transitions()[edge.transition].activity)
        incoming[edge.target].insert(edge.transition);
    }
  }
  auto label = [&net](std::size_t transition) {
    const auto activity = net.transitions()[transition].activity;
    if (!activity)
      throw std::logic_error("silent Petri transition has no activity label");
    return *activity < net.activities().size() ? net.activities()[*activity]
                                               : std::to_string(*activity);
  };
  // The reference scans eventually enabled transitions with a worklist in
  // transition-name order: every silent firing appends the transitions
  // enabled afterwards and moves each of them to that newest marking, and a
  // transition is handled once per marking. A silent transition used on one
  // branch can therefore be unavailable on another.
  std::vector<std::size_t> order(net.transitions().size());
  std::iota(order.begin(), order.end(), std::size_t{0});
  // Explicit names win; otherwise a net that carries a complete name list
  // (for example a converted process tree) orders by those names.
  std::vector<std::string> net_names;
  if (parameters.transition_names.size() != order.size() && net.transition_names() &&
      net.transition_names()->size() == order.size() &&
      std::all_of(net.transition_names()->begin(), net.transition_names()->end(),
                  [](const auto& name) { return name.has_value(); })) {
    for (const auto& name : *net.transition_names()) net_names.push_back(*name);
  }
  const auto& names = parameters.transition_names.size() == order.size()
                          ? parameters.transition_names
                          : net_names;
  if (names.size() == order.size())
    std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
      return names[a] < names[b];
    });
  std::vector<std::size_t> rank(order.size());
  for (std::size_t i = 0; i < order.size(); ++i) rank[order[i]] = i;
  std::vector<std::vector<ReachEdge>> sorted_edges(state_count);
  for (std::size_t state = 0; state < state_count; ++state) {
    sorted_edges[state] = graph.states[state].edges;
    std::stable_sort(sorted_edges[state].begin(), sorted_edges[state].end(),
                     [&](const ReachEdge& a, const ReachEdge& b) {
                       return rank[a.transition] < rank[b.transition];
                     });
  }
  auto eventually_enabled = [&](std::size_t start) {
    std::set<std::size_t> visible;
    std::vector<std::size_t> work;
    std::vector<std::size_t> at(net.transitions().size(), 0);
    for (const auto& edge : sorted_edges[start]) {
      work.push_back(edge.transition);
      at[edge.transition] = start;
    }
    std::set<std::pair<std::size_t, std::size_t>> handled;
    for (std::size_t k = 0; k < work.size(); ++k) {
      const auto transition = work[k];
      const auto state = at[transition];
      if (!handled.emplace(transition, state).second) continue;
      if (net.transitions()[transition].activity) {
        visible.insert(transition);
        continue;
      }
      std::size_t target = state;
      for (const auto& edge : graph.states[state].edges)
        if (edge.transition == transition) { target = edge.target; break; }
      for (const auto& edge : sorted_edges[target]) {
        work.push_back(edge.transition);
        at[edge.transition] = target;
      }
    }
    return visible;
  };
  for (std::size_t transition = 0; transition < net.transitions().size();
       ++transition) {
    if (net.transitions()[transition].activity)
      result.activities.insert(label(transition));
  }
  if (!graph.states.empty()) {
    for (const auto transition : eventually_enabled(0))
      result.start_activities.insert(label(transition));
  }

  std::set<std::pair<std::size_t, std::size_t>> output_pairs;
  std::set<std::pair<std::size_t, std::size_t>> predecessor_pairs;
  std::set<std::pair<std::size_t, std::size_t>> sequence_pairs;
  for (std::size_t state = 0; state < state_count; ++state) {
    std::set<std::size_t> outputs;
    for (const auto& edge : graph.states[state].edges) {
      if (net.transitions()[edge.transition].activity)
        outputs.insert(edge.transition);
    }
    for (auto first = outputs.begin(); first != outputs.end(); ++first) {
      for (auto second = std::next(first); second != outputs.end(); ++second) {
        output_pairs.insert({*first, *second});
        output_pairs.insert({*second, *first});
      }
    }
    const auto eventually = eventually_enabled(state);
    for (const auto predecessor : incoming[state]) {
      for (const auto successor : eventually)
        sequence_pairs.insert({predecessor, successor});
      for (const auto successor : outputs)
        predecessor_pairs.insert({predecessor, successor});
    }
  }
  std::set<std::pair<std::size_t, std::size_t>> parallel_pairs;
  for (const auto& pair : predecessor_pairs) {
    if (predecessor_pairs.count({pair.second, pair.first}) &&
        output_pairs.count(pair))
      parallel_pairs.insert(pair);
  }
  // Like the reference, parallel transition pairs leave the sequence before
  // both relations are relabelled.
  for (const auto& pair : sequence_pairs)
    if (!parallel_pairs.count(pair))
      result.sequence.insert({label(pair.first), label(pair.second)});
  for (const auto& pair : parallel_pairs)
    result.parallel.insert({label(pair.first), label(pair.second)});
  // A label pair in sequence both ways (including a self pair) is parallel.
  std::vector<FootprintPair> symmetric;
  for (const auto& pair : result.sequence)
    if (result.sequence.count({pair.second, pair.first})) symmetric.push_back(pair);
  for (const auto& pair : symmetric) {
    result.parallel.insert(pair);
    result.sequence.erase(pair);
  }
  return result;
}

}  // namespace minera::footprints_detail
