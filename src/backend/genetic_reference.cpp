#include "minera/genetic_reference.hpp"

#include <algorithm>
#include <map>
#include <optional>
#include <set>
#include <string_view>
#include <tuple>
#include <vector>

#include "minera/evaluation.hpp"

namespace minera {
namespace {

using Set = ReferenceGeneticPartition;

Set normalized(Set members) {
  std::sort(members.begin(), members.end());
  members.erase(std::unique(members.begin(), members.end()), members.end());
  return members;
}

bool contains(const Set& set, const std::string& value) {
  return std::binary_search(set.begin(), set.end(), value);
}

bool strict_subset(const Set& small, const Set& large) {
  return small.size() < large.size() &&
         std::includes(large.begin(), large.end(), small.begin(), small.end());
}

bool intersects(const Set& left, const std::set<std::string>& right) {
  return std::any_of(left.begin(), left.end(),
                     [&](const std::string& value) { return right.count(value) > 0; });
}

// The partition lists of one map with every partition normalized.
struct Partitions {
  const ReferenceGeneticMap* map = nullptr;
  std::map<std::string, std::vector<Set>> normalized_lists;

  explicit Partitions(const ReferenceGeneticMap& source) : map(&source) {
    for (const auto& [key, partitions] : source) {
      auto& list = normalized_lists[key];
      for (const auto& partition : partitions) list.push_back(normalized(partition));
    }
  }

  const std::vector<Set>& at(const std::string& key) const {
    const auto found = normalized_lists.find(key);
    if (found == normalized_lists.end())
      throw ReferenceGeneticError("KeyError", "'" + key + "'");
    return found->second;
  }

  bool has(const std::string& key, const Set& partition) const {
    const auto& list = at(key);
    return std::find(list.begin(), list.end(), partition) != list.end();
  }
};

using Candidate = std::pair<Set, Set>;  // (X, Y)

std::set<Candidate> candidates(const std::vector<std::string>& transitions,
                               const Partitions& in, const Partitions& out) {
  std::set<Candidate> result;
  for (const auto& x : transitions) {
    for (const auto& y_set : out.at(x)) {
      for (const auto& first : y_set) {
        for (const auto& x_set : in.at(first)) {
          if (!contains(x_set, x)) continue;
          const bool inputs_agree = std::all_of(
              y_set.begin(), y_set.end(),
              [&](const std::string& y) { return in.has(y, x_set); });
          if (!inputs_agree) continue;
          const bool outputs_agree = std::all_of(
              x_set.begin(), x_set.end(),
              [&](const std::string& other) { return out.has(other, y_set); });
          if (outputs_agree) result.emplace(x_set, y_set);
        }
      }
    }
  }
  return result;
}

// Prunes shared places until stable: a place is dropped when a producer has a
// causal edge no shared place covers (or a consumer such an incoming edge), or
// when an uncovered partition of one of its transitions strictly contains its
// own partition on that side.
std::set<Candidate> shared_places(const std::vector<std::string>& transitions,
                                  const Partitions& in, const Partitions& out) {
  auto current = candidates(transitions, in, out);
  std::set<std::pair<std::string, std::string>> edges;
  for (const auto& x : transitions)
    for (const auto& y_set : out.at(x))
      for (const auto& y : y_set) edges.emplace(x, y);
  for (const auto& y : transitions)
    for (const auto& x_set : in.at(y))
      for (const auto& x : x_set) edges.emplace(x, y);
  while (true) {
    std::set<std::pair<std::string, std::string>> covered;
    for (const auto& [x_set, y_set] : current)
      for (const auto& x : x_set)
        for (const auto& y : y_set) covered.emplace(x, y);
    std::set<std::string> bad_producers, bad_consumers;
    for (const auto& edge : edges) {
      if (covered.count(edge)) continue;
      bad_producers.insert(edge.first);
      bad_consumers.insert(edge.second);
    }
    std::vector<std::pair<std::string, Set>> loose_outputs, loose_inputs;
    for (const auto& x : transitions)
      for (const auto& y_set : out.at(x)) {
        const bool used = std::any_of(current.begin(), current.end(), [&](const Candidate& c) {
          return contains(c.first, x) && c.second == y_set;
        });
        if (!used) loose_outputs.emplace_back(x, y_set);
      }
    for (const auto& y : transitions)
      for (const auto& x_set : in.at(y)) {
        const bool used = std::any_of(current.begin(), current.end(), [&](const Candidate& c) {
          return contains(c.second, y) && c.first == x_set;
        });
        if (!used) loose_inputs.emplace_back(y, x_set);
      }
    std::set<Candidate> next;
    for (const auto& candidate : current) {
      const auto& [x_set, y_set] = candidate;
      if (intersects(x_set, bad_producers) || intersects(y_set, bad_consumers)) continue;
      const bool swallowed_output = std::any_of(
          loose_outputs.begin(), loose_outputs.end(), [&](const auto& loose) {
            return contains(x_set, loose.first) && strict_subset(y_set, loose.second);
          });
      const bool swallowed_input = std::any_of(
          loose_inputs.begin(), loose_inputs.end(), [&](const auto& loose) {
            return contains(y_set, loose.first) && strict_subset(x_set, loose.second);
          });
      if (!swallowed_output && !swallowed_input) next.insert(candidate);
    }
    if (next == current) return current;
    current = std::move(next);
  }
}

// One side of get_src_sink_sets_for_wfnet: transitions are visited in order
// and grouped into graphs headed by the transition that opened them. A
// transition joins the first graph that already contains it (or opens one),
// adds its partition members, and absorbs every other graph headed by one of
// those members. The heads are the result.
std::vector<std::string> graph_heads(const std::vector<std::string>& transitions,
                                     const ReferenceGeneticMap& map) {
  struct Graph {
    std::string head;
    std::set<std::string> members;
  };
  std::vector<Graph> graphs;
  for (const auto& t : transitions) {
    const auto found = map.find(t);
    if (found == map.end()) throw ReferenceGeneticError("KeyError", "'" + t + "'");
    std::vector<std::string> successors;
    for (const auto& partition : found->second)
      successors.insert(successors.end(), partition.begin(), partition.end());
    std::size_t target = 0;
    while (target < graphs.size() && !graphs[target].members.count(t)) ++target;
    const bool opened = target == graphs.size();
    Graph fresh{t, {t}};
    auto graph = [&]() -> Graph& { return opened ? fresh : graphs[target]; };
    graph().members.insert(successors.begin(), successors.end());
    for (const auto& successor : successors) {
      std::size_t headed = 0;
      while (headed < graphs.size() &&
             (graphs[headed].head != successor || (!opened && headed == target)))
        ++headed;
      if (headed == graphs.size()) continue;
      auto members = std::move(graphs[headed].members);
      graphs.erase(graphs.begin() + static_cast<std::ptrdiff_t>(headed));
      if (!opened && headed < target) --target;
      graph().members.insert(members.begin(), members.end());
    }
    if (opened) graphs.push_back(std::move(fresh));
  }
  std::vector<std::string> heads;
  for (const auto& graph : graphs) heads.push_back(graph.head);
  return heads;
}

std::string joined(const Set& members) {
  std::string text;
  for (const auto& member : members) text += (text.empty() ? "" : ",") + member;
  return text;
}

}  // namespace

ReferenceGeneticSourcesSinks reference_genetic_sources_sinks(
    const ReferenceGeneticMatrix& matrix) {
  return {graph_heads(matrix.transitions, matrix.outputs),
          graph_heads(matrix.transitions, matrix.inputs)};
}

void reference_genetic_add_singleton_partition(ReferenceGeneticMap& map,
                                               const std::string& key,
                                               const std::string& transition) {
  auto& partitions = map[key];
  const bool present = std::any_of(
      partitions.begin(), partitions.end(), [&](const ReferenceGeneticPartition& p) {
        return std::find(p.begin(), p.end(), transition) != p.end();
      });
  if (!present) partitions.push_back({transition});
}

void reference_genetic_remove_transition_from_partitions(
    ReferenceGeneticMap& map, const std::string& key,
    const std::string& transition) {
  auto& partitions = map[key];
  for (auto it = partitions.begin(); it != partitions.end(); ++it) {
    const auto member = std::find(it->begin(), it->end(), transition);
    if (member == it->end()) continue;
    it->erase(member);
    if (it->empty()) partitions.erase(it);
    return;
  }
}

std::string reference_genetic_place_key(const ReferenceGeneticPlace& place) {
  switch (place.kind) {
    case ReferenceGeneticPlaceKind::kStart: return "i";
    case ReferenceGeneticPlaceKind::kEnd: return "o";
    case ReferenceGeneticPlaceKind::kShared:
      return "p:" + joined(place.producers) + ":" + joined(place.consumers);
    case ReferenceGeneticPlaceKind::kOutput:
      return "o:" + place.transition + ":" + joined(place.consumers);
    case ReferenceGeneticPlaceKind::kInput:
      return "i:" + place.transition + ":" + joined(place.producers);
  }
  return {};
}

ReferenceGeneticNet reference_genetic_net(const ReferenceGeneticMatrix& matrix) {
  const auto& transitions = matrix.transitions;
  const Partitions in(matrix.inputs), out(matrix.outputs);
  const auto shared = shared_places(transitions, in, out);

  // Partitions covered by a shared place, plus uncovered partitions that are a
  // strict subset of a covered partition of the same transition (they vanish).
  std::set<std::pair<std::string, Set>> covered_outputs, covered_inputs;
  for (const auto& [x_set, y_set] : shared) {
    for (const auto& x : x_set) covered_outputs.emplace(x, y_set);
    for (const auto& y : y_set) covered_inputs.emplace(y, x_set);
  }
  auto absorb = [&](std::set<std::pair<std::string, Set>>& covered, const Partitions& side) {
    std::vector<std::pair<std::string, Set>> vanished;
    for (const auto& t : transitions)
      for (const auto& partition : side.at(t))
        for (const auto& [owner, used] : covered)
          if (owner == t && strict_subset(partition, used)) vanished.emplace_back(t, partition);
    covered.insert(vanished.begin(), vanished.end());
  };
  absorb(covered_outputs, out);
  absorb(covered_inputs, in);

  ReferenceGeneticNet net;
  std::map<std::string, std::size_t> index;
  auto place = [&](ReferenceGeneticPlace value) {
    const auto key = reference_genetic_place_key(value);
    const auto found = index.find(key);
    if (found != index.end()) return found->second;
    net.places.push_back(std::move(value));
    return index[key] = net.places.size() - 1;
  };
  net.start = place({ReferenceGeneticPlaceKind::kStart, {}, {}, {}});
  net.end = place({ReferenceGeneticPlaceKind::kEnd, {}, {}, {}});

  std::map<std::string, std::set<std::size_t>> visible_inputs, visible_outputs;
  for (const auto& [x_set, y_set] : shared) {
    const auto id = place({ReferenceGeneticPlaceKind::kShared, {}, x_set, y_set});
    for (const auto& x : x_set) visible_outputs[x].insert(id);
    for (const auto& y : y_set) visible_inputs[y].insert(id);
  }
  for (const auto& x : transitions)
    for (const auto& y_set : out.at(x))
      if (!covered_outputs.count({x, y_set}))
        visible_outputs[x].insert(place({ReferenceGeneticPlaceKind::kOutput, x, {}, y_set}));
  for (const auto& y : transitions)
    for (const auto& x_set : in.at(y))
      if (!covered_inputs.count({y, x_set}))
        visible_inputs[y].insert(place({ReferenceGeneticPlaceKind::kInput, y, x_set, {}}));

  const auto boundary = reference_genetic_sources_sinks(matrix);
  for (const auto& source : boundary.sources) visible_inputs[source].insert(net.start);
  for (const auto& sink : boundary.sinks) visible_outputs[sink].insert(net.end);

  auto sorted_by_key = [&](const std::set<std::size_t>& ids) {
    std::vector<std::size_t> result(ids.begin(), ids.end());
    std::sort(result.begin(), result.end(), [&](std::size_t a, std::size_t b) {
      return reference_genetic_place_key(net.places[a]) < reference_genetic_place_key(net.places[b]);
    });
    return result;
  };
  for (const auto& t : transitions)
    net.transitions.push_back({t, sorted_by_key(visible_inputs[t]), sorted_by_key(visible_outputs[t])});

  // One silent transition per causal edge leaving an uncovered partition.
  std::vector<std::pair<std::string, std::string>> edges;
  auto add_edge = [&](const std::string& x, const std::string& y) {
    if (std::find(edges.begin(), edges.end(), std::make_pair(x, y)) == edges.end())
      edges.emplace_back(x, y);
  };
  for (const auto& x : transitions)
    for (const auto& y_set : out.at(x))
      if (!covered_outputs.count({x, y_set}))
        for (const auto& y : y_set) add_edge(x, y);
  for (const auto& y : transitions)
    for (const auto& x_set : in.at(y))
      if (!covered_inputs.count({y, x_set}))
        for (const auto& x : x_set) add_edge(x, y);
  for (const auto& [x, y] : edges) {
    std::set<std::size_t> inputs, outputs;
    for (const auto& y_set : out.at(x))
      if (!covered_outputs.count({x, y_set}) && contains(y_set, y))
        inputs.insert(index.at(reference_genetic_place_key({ReferenceGeneticPlaceKind::kOutput, x, {}, y_set})));
    const auto& y_inputs = in.at(y);
    for (const auto& x_set : y_inputs)
      if (!covered_inputs.count({y, x_set}) && contains(x_set, x))
        outputs.insert(index.at(reference_genetic_place_key({ReferenceGeneticPlaceKind::kInput, y, x_set, {}})));
    net.transitions.push_back({std::nullopt, sorted_by_key(inputs), sorted_by_key(outputs)});
  }
  return net;
}

std::string reference_genetic_place_name(const ReferenceGeneticPlace& place) {
  auto frozen = [](const Set& members) {
    std::string text = "frozenset({";
    for (std::size_t i = 0; i < members.size(); ++i)
      text += (i ? ", '" : "'") + members[i] + "'";
    return text + "})";
  };
  switch (place.kind) {
    case ReferenceGeneticPlaceKind::kStart: return "i";
    case ReferenceGeneticPlaceKind::kEnd: return "o";
    case ReferenceGeneticPlaceKind::kShared:
      return "(" + frozen(place.producers) + ", " + frozen(place.consumers) + ")";
    case ReferenceGeneticPlaceKind::kOutput:
      return "o('" + place.transition + "', " + frozen(place.consumers) + ")";
    case ReferenceGeneticPlaceKind::kInput:
      return "i('" + place.transition + "', " + frozen(place.producers) + ")";
  }
  return {};
}

std::string reference_genetic_transition_key(const ReferenceGeneticNet& net,
                                             std::size_t transition) {
  const auto& value = net.transitions.at(transition);
  auto keys = [&](const std::vector<std::size_t>& ids) {
    std::vector<std::string> names;
    for (const auto id : ids) names.push_back(reference_genetic_place_key(net.places[id]));
    std::sort(names.begin(), names.end());
    std::string text;
    for (const auto& name : names) text += (text.empty() ? "" : ";") + name;
    return text.empty() ? std::string("-") : text;
  };
  return value.label.value_or("-") + " " + keys(value.inputs) + " " + keys(value.outputs);
}

ReferenceGeneticPetriNet reference_genetic_petri_net(const ReferenceGeneticNet& net,
                                                     const ReferenceGeneticReplayOrder* order) {
  const ReferenceGeneticArcOrder* arc_order = order ? &order->arcs : nullptr;
  ReferenceGeneticPetriNet result;
  std::vector<std::string> activities;
  for (const auto& transition : net.transitions)
    if (transition.label &&
        std::find(activities.begin(), activities.end(), *transition.label) == activities.end())
      activities.push_back(*transition.label);
  result.net.set_activities(activities);
  for (std::size_t i = 0; i < net.places.size(); ++i) result.net.add_place();
  result.net.set_source(net.start);
  result.net.set_sink(net.end);
  result.outgoing_order.resize(net.places.size());
  for (const auto& place : net.places) {
    const auto key = reference_genetic_place_key(place);
    const auto observed = order ? order->place_names.find(key) : std::map<std::string, std::string>::const_iterator{};
    result.place_names.push_back(order && observed != order->place_names.end()
                                     ? observed->second
                                     : reference_genetic_place_name(place));
  }
  for (std::size_t t = 0; t < net.transitions.size(); ++t) {
    const auto& transition = net.transitions[t];
    std::optional<std::size_t> activity;
    if (transition.label)
      activity = static_cast<std::size_t>(
          std::find(activities.begin(), activities.end(), *transition.label) - activities.begin());
    result.net.add_transition(activity, transition.inputs, transition.outputs);
    for (const auto place : transition.inputs) result.outgoing_order[place].push_back(t);
  }
  if (arc_order) {
    for (std::size_t place = 0; place < net.places.size(); ++place) {
      const auto found = arc_order->find(reference_genetic_place_key(net.places[place]));
      if (found == arc_order->end()) continue;
      // Rank each consumer by the first unused matching key in the observed
      // order; transitions sharing a key are interchangeable.
      std::vector<bool> used(found->second.size(), false);
      std::vector<std::pair<std::size_t, std::size_t>> ranked;
      for (const auto t : result.outgoing_order[place]) {
        const auto key = reference_genetic_transition_key(net, t);
        std::size_t rank = found->second.size();
        for (std::size_t i = 0; i < found->second.size(); ++i)
          if (!used[i] && found->second[i] == key) {
            used[i] = true;
            rank = i;
            break;
          }
        ranked.emplace_back(rank, t);
      }
      std::stable_sort(ranked.begin(), ranked.end(),
                       [](const auto& a, const auto& b) { return a.first < b.first; });
      auto& row = result.outgoing_order[place];
      row.clear();
      for (const auto& entry : ranked) row.push_back(entry.second);
    }
  }
  result.markings = default_workflow_markings(result.net);
  return result;
}

namespace {

// Whether token replay reads nothing of an event but its activity label: a
// present first-class activity with no typed value, source key or other
// attribute (the events the genetic miner builds for its fitness log).
bool plain_replay_event(const Event& event) {
  return event.activity_state == FirstClassAttributeState::kPresent &&
         !event.activity_attribute && event.activity_source_key.empty() &&
         event.attributes.empty();
}

// The log's distinct activity sequences, each once (its first trace), and the
// variant of every trace; empty when some event is not plain.
struct GeneticReplayVariants {
  EventLog log;
  std::vector<std::size_t> trace_variant;
};

std::optional<GeneticReplayVariants> genetic_replay_variants(const EventLog& log) {
  GeneticReplayVariants result;
  result.log.attributes = log.attributes;
  result.log.metadata = log.metadata;
  result.trace_variant.reserve(log.traces.size());
  std::map<std::vector<std::string_view>, std::size_t> variants;
  std::vector<std::string_view> key;
  for (const auto& trace : log.traces) {
    key.clear();
    for (const auto& event : trace.events) {
      if (!plain_replay_event(event)) return std::nullopt;
      key.push_back(event.activity);
    }
    const auto [found, added] = variants.try_emplace(key, result.log.traces.size());
    if (added) result.log.traces.push_back(trace);
    result.trace_variant.push_back(found->second);
  }
  return result;
}

}  // namespace

double reference_genetic_fitness(const ReferenceGeneticMatrix& matrix, const EventLog& log,
                                 const ReferenceGeneticReplayOrder* order) {
  const auto petri = reference_genetic_petri_net(reference_genetic_net(matrix), order);
  ReplayFitnessOptions options;
  options.place_outgoing_transition_order = petri.outgoing_order;
  options.place_names = petri.place_names;
  const auto variants = genetic_replay_variants(log);
  if (!variants) {
    const auto replay =
        evaluate_replay_fitness(log, petri.net, ReplayFitnessVariant::TOKEN_BASED, options);
    return 0.4 * replay.average_trace_fitness + 0.6 * replay.percentage_fit_traces / 100.0;
  }
  // Token replay of a trace depends only on its activity sequence, so each
  // variant is replayed once; the totals are then accumulated over every
  // trace in log order exactly as the whole-log replay accumulates them.
  const auto replay = evaluate_replay_fitness(variants->log, petri.net,
                                              ReplayFitnessVariant::TOKEN_BASED, options);
  // Without per-trace rows the replay stopped before any trace (an invalid
  // net or marking), and its result does not depend on the traces.
  if (!replay.token_result.trace_diagnostics)
    return 0.4 * replay.average_trace_fitness + 0.6 * replay.percentage_fit_traces / 100.0;
  const auto& rows = *replay.token_result.trace_diagnostics;
  double fitness_sum = 0.0;
  std::size_t fitting = 0;
  TokenInteger produced = 0, consumed = 0;
  for (const auto variant : variants->trace_variant) {
    const auto& row = rows.at(variant);
    fitting += row.trace_is_fit ? 1 : 0;
    fitness_sum += row.trace_fitness;
    produced += row.produced;
    consumed += row.consumed;
  }
  const std::size_t trace_count = variants->trace_variant.size();
  double average_trace_fitness = 0.0, percentage_fit_traces = 0.0;
  if (trace_count > 0 && consumed > 0 && produced > 0) {
    average_trace_fitness = fitness_sum / double(trace_count);
    percentage_fit_traces = 100.0 * double(fitting) / double(trace_count);
  }
  return 0.4 * average_trace_fitness + 0.6 * percentage_fit_traces / 100.0;
}

ReferenceGeneticTournament reference_genetic_tournament(
    const std::vector<ReferenceGeneticMatrix>& population, const EventLog& log, bool sort,
    const std::vector<ReferenceGeneticReplayOrder>* orders) {
  ReferenceGeneticTournament result;
  std::vector<double> fitness;
  for (std::size_t i = 0; i < population.size(); ++i)
    fitness.push_back(reference_genetic_fitness(
        population[i], log, orders ? &orders->at(i) : nullptr));
  for (std::size_t i = 0; i < population.size(); ++i) result.order.push_back(i);
  if (sort)
    std::stable_sort(result.order.begin(), result.order.end(),
                     [&](std::size_t a, std::size_t b) { return fitness[a] > fitness[b]; });
  for (const auto i : result.order) result.fitness.push_back(fitness[i]);
  return result;
}

bool reference_genetic_matrix_is_well_formed(const ReferenceGeneticMatrix& matrix) {
  const std::set<std::string> names(matrix.transitions.begin(), matrix.transitions.end());
  if (names.size() != matrix.transitions.size()) return false;
  auto keyed_by_names = [&](const ReferenceGeneticMap& map) {
    if (map.size() != names.size()) return false;
    return std::all_of(map.begin(), map.end(),
                       [&](const auto& entry) { return names.count(entry.first) > 0; });
  };
  if (!keyed_by_names(matrix.inputs) || !keyed_by_names(matrix.outputs)) return false;
  std::map<std::string, std::set<std::string>> input_union, output_union;
  auto check_side = [&](const ReferenceGeneticMap& map,
                        std::map<std::string, std::set<std::string>>& unions) {
    for (const auto& [t, partitions] : map) {
      std::set<Set> seen;
      for (const auto& partition : partitions) {
        const auto members = normalized(partition);
        if (members.empty() || members.size() != partition.size()) return false;
        if (!seen.insert(members).second) return false;
        for (const auto& member : members) {
          if (!names.count(member)) return false;
          if (!unions[t].insert(member).second) return false;  // overlapping partitions
        }
      }
    }
    return true;
  };
  if (!check_side(matrix.inputs, input_union) || !check_side(matrix.outputs, output_union))
    return false;
  for (const auto& x : matrix.transitions)
    for (const auto& y : matrix.transitions)
      if (output_union[x].count(y) != input_union[y].count(x)) return false;
  return true;
}

}  // namespace minera
