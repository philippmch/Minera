#include "minera/alpha_miner.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

#include "minera/dfg_variants.hpp"
#include "minera/alpha_relations.hpp"

namespace minera {
namespace {

// A sorted, duplicate-free set of activity ids. Kept sorted so set operations
// (union, subset, equality) are cheap and pairs deduplicate in an ordered set.
using ActivitySet = std::vector<std::size_t>;
using Place = std::pair<ActivitySet, ActivitySet>;

ActivitySet unite(const ActivitySet& a, const ActivitySet& b) {
  ActivitySet out;
  std::set_union(a.begin(), a.end(), b.begin(), b.end(), std::back_inserter(out));
  return out;
}

bool is_subset(const ActivitySet& sub, const ActivitySet& sup) {
  return std::includes(sup.begin(), sup.end(), sub.begin(), sub.end());
}

// The log with activity names interned to dense ids, so the full-log and
// reduced-log footprints of the Alpha+ variant share one id space.
struct IdLog {
  std::vector<std::string> names;
  std::vector<std::vector<std::size_t>> traces;
};

IdLog intern_log(const EventLog& log) {
  IdLog out;
  std::unordered_map<std::string, std::size_t> ids;
  for (const auto& trace : log.traces) {
    std::vector<std::size_t> sequence;
    sequence.reserve(trace.events.size());
    for (const auto& event : trace.events) {
      const auto [it, inserted] =
          ids.try_emplace(event.activity, out.names.size());
      if (inserted) out.names.push_back(event.activity);
      sequence.push_back(it->second);
    }
    out.traces.push_back(std::move(sequence));
  }
  return out;
}

// The log footprint: which activities directly follow which, plus the start and
// end activities. All higher-level relations derive from this matrix. With
// short_loops set, "a b a" patterns are also recorded so that a length-two loop
// — where both directions of succession appear — can be told apart from true
// concurrency when reading causality.
struct Footprint {
  std::vector<std::vector<bool>> follows;   // follows[a][b]: a directly precedes b
  std::vector<std::vector<bool>> two_loop;  // two_loop[a][b]: "a b a" occurs
  std::vector<std::vector<bool>> classic_causal;
  std::set<std::size_t> starts;
  std::set<std::size_t> ends;
  bool short_loops = false;

  [[nodiscard]] std::size_t size() const { return follows.size(); }
  [[nodiscard]] bool causal(std::size_t a, std::size_t b) const {
    if (!short_loops) return classic_causal[a][b];
    if (!follows[a][b]) return false;
    if (!follows[b][a]) return true;
    // Succession in both directions: concurrency for the classic miner, but a
    // length-two loop — hence causal both ways — when "a b a" and "b a b" both
    // occur, which interleaved concurrent activities cannot produce.
    return short_loops && two_loop[a][b] && two_loop[b][a];
  }
  [[nodiscard]] bool choice(std::size_t a, std::size_t b) const {
    if (short_loops) {
      const bool a_outgoing = std::any_of(follows[a].begin(), follows[a].end(),
                                         [](bool value) { return value; });
      const bool b_outgoing = std::any_of(follows[b].begin(), follows[b].end(),
                                         [](bool value) { return value; });
      if (a_outgoing != b_outgoing) return false;
    }
    return !follows[a][b] && !follows[b][a];
  }
};

void set_classic_relations(Footprint& fp,
                           const std::vector<std::string>& names,
                           const DfgFrequency& graph) {
  const auto abstraction = make_alpha_classic_abstraction(graph);
  const auto n = names.size();
  fp.classic_causal.assign(n, std::vector<bool>(n));
  for (std::size_t a = 0; a < n; ++a) {
    for (std::size_t b = 0; b < n; ++b) {
      fp.classic_causal[a][b] =
          abstraction.causal.contains({names[a], names[b]});
    }
  }
}

// Builds the footprint over the interned log. Activities marked in `exclude`
// (the Alpha+ length-one-loop removal) are read as absent from every trace.
Footprint build_footprint(const IdLog& log, const std::vector<bool>* exclude,
                          bool short_loops) {
  const std::size_t n = log.names.size();
  Footprint fp;
  fp.short_loops = short_loops;
  fp.follows.assign(n, std::vector<bool>(n, false));
  if (short_loops) fp.two_loop.assign(n, std::vector<bool>(n, false));

  std::vector<std::size_t> sequence;
  DfgFrequency graph;
  for (const auto& trace : log.traces) {
    sequence.clear();
    for (const auto id : trace) {
      if (!exclude || !(*exclude)[id]) sequence.push_back(id);
    }
    if (sequence.empty()) continue;
    fp.starts.insert(sequence.front());
    fp.ends.insert(sequence.back());
    for (std::size_t i = 1; i < sequence.size(); ++i) {
      fp.follows[sequence[i - 1]][sequence[i]] = true;
      if (!short_loops)
        graph[{log.names[sequence[i - 1]], log.names[sequence[i]]}] = 1;
      if (short_loops && i >= 2 && sequence[i - 2] == sequence[i]) {
        fp.two_loop[sequence[i]][sequence[i - 1]] = true;
      }
    }
  }
  if (!short_loops) set_classic_relations(fp, log.names, graph);
  return fp;
}

// A set is admissible on one side of a place only if its members are pairwise
// in the choice relation and none has a self-loop — both required for the place
// to be a valid Petri-net construct under the Alpha algorithm.
bool admissible(const ActivitySet& set, const Footprint& fp) {
  for (std::size_t i = 0; i < set.size(); ++i) {
    if (fp.follows[set[i]][set[i]]) return false;  // self-loop: a is not a # a
    for (std::size_t j = i + 1; j < set.size(); ++j) {
      if (!fp.choice(set[i], set[j])) return false;
    }
  }
  return true;
}

bool all_causal(const ActivitySet& a, const ActivitySet& b, const Footprint& fp) {
  for (const auto from : a) {
    for (const auto to : b) {
      if (!fp.causal(from, to)) return false;
    }
  }
  return true;
}

const std::string& selected_activity(const Event& event,
                                     std::string_view selector) {
  const auto selected = selector.empty() ? std::string_view("concept:name")
                                         : selector;
  const bool canonical_selector = selected == "concept:name" ||
                                  selected == "activity";
  if (canonical_selector) {
    // The canonical projection remains authoritative even when an adapter did
    // not retain a source spelling.  A stale retained cell must not turn a
    // null or genuinely missing first-class value into an activity label.
    if (event.activity_state == FirstClassAttributeState::kNull ||
        (event.activity_state == FirstClassAttributeState::kMissing &&
         event.activity.empty())) {
      throw std::invalid_argument(
          "Alpha: event is missing activity attribute '" +
          std::string(selected) + "'");
    }
    return event.activity;
  }
  // A custom first-class projection is authoritative for its source key.
  // Do this before consulting the retained attribute copy: adapters may keep
  // that copy for round-tripping even after the projected cell changes.
  if (!event.activity_source_key.empty() &&
      event.activity_source_key == selected) {
    if (event.activity_state == FirstClassAttributeState::kNull ||
        (event.activity_state == FirstClassAttributeState::kMissing &&
         event.activity.empty())) {
      throw std::invalid_argument(
          "Alpha: event is missing activity attribute '" +
          std::string(selected) + "'");
    }
    return event.activity;
  }
  if (event.activity_attribute && event.activity_attribute->key == selected) {
    if (!event.activity_attribute->is_null) return event.activity_attribute->value;
    throw std::invalid_argument(
        "Alpha: event is missing activity attribute '" +
        std::string(selected) + "'");
  }
  if (const auto* value = dfg_event_field_value(event, selected)) return *value;
  throw std::invalid_argument(
      "Alpha: event is missing activity attribute '" +
      std::string(selected) + "'");
}

std::vector<Place> maximal_places(const Footprint& fp);

PetriNet build_classic_net(const std::vector<std::string>& names,
                           const Footprint& fp, bool remove_unconnected) {
  PetriNet net;
  net.set_activities(names);
  const std::size_t source = net.add_place();
  const std::size_t sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);

  const std::size_t n = fp.size();
  std::vector<std::vector<std::size_t>> inputs(n);
  std::vector<std::vector<std::size_t>> outputs(n);
  for (const auto start : fp.starts) inputs[start].push_back(source);
  for (const auto end : fp.ends) outputs[end].push_back(sink);

  for (const auto& pair : maximal_places(fp)) {
    const std::size_t place = net.add_place();
    for (const auto from : pair.first) outputs[from].push_back(place);
    for (const auto to : pair.second) inputs[to].push_back(place);
  }

  for (std::size_t activity = 0; activity < n; ++activity) {
    if (remove_unconnected && inputs[activity].empty() &&
        outputs[activity].empty()) {
      continue;
    }
    net.add_transition(activity, std::move(inputs[activity]),
                       std::move(outputs[activity]));
  }
  return net;
}

Footprint footprint_from_graph(const DirectlyFollowsGraph& graph) {
  Footprint fp;
  const auto& names = graph.activities();
  const std::size_t n = names.size();
  fp.follows.assign(n, std::vector<bool>(n, false));
  DfgFrequency frequency;
  for (const auto& edge : graph.edges()) {
    if (edge.source >= n || edge.target >= n)
      throw std::invalid_argument("Alpha: graph edge activity id is out of range");
    fp.follows[edge.source][edge.target] = true;
    frequency[{names[edge.source], names[edge.target]}] = edge.count;
  }
  for (std::size_t activity = 0; activity < n; ++activity) {
    if (graph.start_activity_count(names[activity]) != 0)
      fp.starts.insert(activity);
    if (graph.end_activity_count(names[activity]) != 0)
      fp.ends.insert(activity);
  }
  set_classic_relations(fp, names, frequency);
  return fp;
}

// Enumerates the maximal place candidates (A, B). Seeds with singleton causal
// pairs, then grows either side by union — keeping the other side fixed — until
// no new admissible pair appears. Growing one side at a time reaches every
// maximal pair, after which non-maximal pairs are dropped.
std::vector<Place> maximal_places(const Footprint& fp) {
  std::set<Place> candidates;
  const std::size_t n = fp.size();
  for (std::size_t a = 0; a < n; ++a) {
    if (fp.follows[a][a]) continue;
    for (std::size_t b = 0; b < n; ++b) {
      if (fp.follows[b][b]) continue;
      if (fp.causal(a, b)) candidates.insert({{a}, {b}});
    }
  }

  for (bool changed = true; changed;) {
    changed = false;
    const std::vector<Place> snapshot(candidates.begin(), candidates.end());
    for (std::size_t i = 0; i < snapshot.size(); ++i) {
      for (std::size_t j = i + 1; j < snapshot.size(); ++j) {
        const auto& [a1, b1] = snapshot[i];
        const auto& [a2, b2] = snapshot[j];
        if (b1 == b2) {
          auto a = unite(a1, a2);
          if (a != a1 && admissible(a, fp) && all_causal(a, b1, fp)) {
            changed |= candidates.insert({std::move(a), b1}).second;
          }
        }
        if (a1 == a2) {
          auto b = unite(b1, b2);
          if (b != b1 && admissible(b, fp) && all_causal(a1, b, fp)) {
            changed |= candidates.insert({a1, std::move(b)}).second;
          }
        }
      }
    }
  }

  const std::vector<Place> all(candidates.begin(), candidates.end());
  std::vector<Place> maximal;
  for (const auto& place : all) {
    const bool dominated = std::any_of(
        all.begin(), all.end(), [&](const Place& other) {
          return other != place && is_subset(place.first, other.first) &&
                 is_subset(place.second, other.second);
        });
    if (!dominated) maximal.push_back(place);
  }
  return maximal;
}

}  // namespace

EventLog normalize_alpha_event_log(const EventLog& event_log,
                                   const AlphaMinerOptions& options) {
  if (options.max_rows_to_materialize) {
    std::size_t rows = 0;
    for (const auto& trace : event_log.traces) {
      if (trace.events.size() > *options.max_rows_to_materialize - rows)
        throw std::length_error("Alpha: event row materialization limit exceeded");
      rows += trace.events.size();
    }
  }
  EventLog normalized = event_log;
  const std::string selector = options.activity_attribute.empty()
                                   ? "concept:name"
                                   : options.activity_attribute;
  for (auto& trace : normalized.traces) {
    for (auto& event : trace.events) {
      event.activity = selected_activity(event, selector);
      event.activity_state = FirstClassAttributeState::kPresent;
    }
  }
  return normalized;
}

PetriNet discover_alpha_classic(const EventLog& log,
                                const AlphaMinerOptions& options) {
  const EventLog normalized = normalize_alpha_event_log(log, options);
  const IdLog ids = intern_log(normalized);
  const std::size_t n = ids.names.size();
  Footprint footprint = build_footprint(ids, nullptr, false);
  const auto endpoints = alpha_log_endpoints(normalized);
  footprint.starts.clear();
  footprint.ends.clear();
  for (std::size_t activity = 0; activity < n; ++activity) {
    if (endpoints.starts.contains(ids.names[activity])) footprint.starts.insert(activity);
    if (endpoints.ends.contains(ids.names[activity])) footprint.ends.insert(activity);
  }
  return build_classic_net(ids.names, footprint, false);
}

AlphaPlusDiscoveryResult discover_alpha_plus_result(const EventLog& log,
                             const AlphaMinerOptions& options) {
  const EventLog normalized = normalize_alpha_event_log(log, options);
  IdLog ids = intern_log(normalized);
  const auto visible_names = ids.names;
  if (ids.traces.empty())
    throw std::invalid_argument("Alpha+: a trace-bearing log is required");
  // Boundary labels participate in ordinary label identity and loop removal.
  const auto boundary_id = [&](const std::string& name) {
    const auto found = std::find(ids.names.begin(), ids.names.end(), name);
    if (found != ids.names.end())
      return static_cast<std::size_t>(found - ids.names.begin());
    ids.names.push_back(name);
    return ids.names.size() - 1;
  };
  const auto start_label = boundary_id("artificial_start");
  const auto end_label = boundary_id("artificial_end");
  IdLog boundary_log = ids;
  for (auto& sequence : boundary_log.traces) {
    sequence.insert(sequence.begin(), start_label);
    sequence.push_back(end_label);
  }
  const std::size_t n = ids.names.size();
  const Footprint full = build_footprint(boundary_log, nullptr, true);
  std::vector<bool> loops(n, false);
  for (std::size_t a = 0; a < n; ++a) loops[a] = full.follows[a][a];
  const auto reduced = build_footprint(boundary_log, &loops, true);
  if (reduced.starts.empty() || reduced.ends.empty())
    throw std::invalid_argument("Alpha+: boundary reduction leaves no endpoints");
  const auto& boundary_exclude = loops;
  const std::vector<Place> boundary_pairs = maximal_places(
      build_footprint(boundary_log, &boundary_exclude, true));

  // Retain the last immediate non-loop predecessor of each loop label while
  // accumulating all immediate non-loop successors, in original trace order.
  std::vector<ActivitySet> predecessors(n);
  std::vector<ActivitySet> successors(n);
  const auto insert_sorted = [](ActivitySet& values, std::size_t value) {
    const auto at = std::lower_bound(values.begin(), values.end(), value);
    if (at == values.end() || *at != value) values.insert(at, value);
  };
  for (const auto& trace : boundary_log.traces) {
    for (std::size_t i = 1; i < trace.size(); ++i) {
      const auto before = trace[i - 1];
      const auto after = trace[i];
      const bool before_is_loop = before < n && loops[before];
      const bool after_is_loop = after < n && loops[after];
      if (!before_is_loop && after_is_loop)
        predecessors[after] = {before};
      if (before_is_loop && !after_is_loop)
        insert_sorted(successors[before], after);
    }
  }

  // Every maximal pair containing both context differences contributes its
  // own fresh loop place. Empty differences participate in the subset tests.
  std::vector<std::vector<std::size_t>> loop_pairs(n);
  for (std::size_t t = 0; t < n; ++t) {
    if (!loops[t]) continue;
    if (predecessors[t].empty() || successors[t].empty()) continue;
    ActivitySet before_only;
    ActivitySet after_only;
    std::set_difference(predecessors[t].begin(), predecessors[t].end(),
                        successors[t].begin(), successors[t].end(),
                        std::back_inserter(before_only));
    std::set_difference(successors[t].begin(), successors[t].end(),
                        predecessors[t].begin(), predecessors[t].end(),
                        std::back_inserter(after_only));
    for (std::size_t pair_index = 0; pair_index < boundary_pairs.size();
         ++pair_index) {
      const auto& candidate = boundary_pairs[pair_index];
      if (!is_subset(before_only, candidate.first) ||
          !is_subset(after_only, candidate.second)) {
        continue;
      }
      loop_pairs[t].push_back(pair_index);
    }
  }

  std::vector<std::vector<std::size_t>> transition_inputs(n);
  std::vector<std::vector<std::size_t>> transition_outputs(n);
  for (std::size_t pair_index = 0; pair_index < boundary_pairs.size();
       ++pair_index) {
    for (const auto from : boundary_pairs[pair_index].first)
      transition_outputs[from].push_back(pair_index);
    for (const auto to : boundary_pairs[pair_index].second)
      transition_inputs[to].push_back(pair_index);
  }
  // Assemble the complete graph before cleanup: reserved names can occur
  // inside traces, and reduced traces can have several distinct endpoints.
  std::vector<AlphaPlaceName> names{{"start", {}, {}}, {"end", {}, {}}};
  const auto pair_name = [&](std::size_t index) {
    AlphaPlaceName name;
    for (const auto id : boundary_pairs[index].first)
      name.before.push_back(boundary_log.names[id]);
    for (const auto id : boundary_pairs[index].second)
      name.after.push_back(boundary_log.names[id]);
    std::sort(name.before.begin(), name.before.end());
    std::sort(name.after.begin(), name.after.end());
    return name;
  };
  for (std::size_t i = 0; i < boundary_pairs.size(); ++i)
    names.push_back(pair_name(i));
  auto inputs = transition_inputs;
  auto outputs = transition_outputs;
  for (std::size_t t = 0; t < n; ++t) {
    for (auto& p : inputs[t]) p += 2;
    for (auto& p : outputs[t]) p += 2;
    if (reduced.starts.contains(t)) inputs[t].push_back(0);
    if (reduced.ends.contains(t)) outputs[t].push_back(1);
    if (loops[t]) {
      for (const auto pair : loop_pairs[t]) {
        inputs[t].push_back(names.size());
        outputs[t].push_back(names.size());
        names.push_back(pair_name(pair));
      }
    }
  }
  std::vector<bool> live_places(names.size(), true), live_transitions(n, true);
  const auto erase_place = [&](std::size_t p) {
    live_places[p] = false;
    for (auto& values : inputs) std::erase(values, p);
    for (auto& values : outputs) std::erase(values, p);
  };
  const auto erase_transition = [&](std::size_t t) {
    live_transitions[t] = false;
    inputs[t].clear();
    outputs[t].clear();
  };
  const auto incident = [&](std::size_t p, const auto& arcs) {
    std::vector<std::size_t> result;
    for (std::size_t t = 0; t < n; ++t)
      if (live_transitions[t] && std::find(arcs[t].begin(), arcs[t].end(), p) != arcs[t].end())
        result.push_back(t);
    return result;
  };
  // The public implementation selects an identity-ordered incident arc.
  // Allocation identity is not portable; sample tied incident choices.
  // Uniform independent draws model observed alternatives, not the unverified
  // allocation-dependent probabilities or correlations of the reference.
  const auto choose = [](const std::vector<std::size_t>& candidates) {
    if (candidates.empty())
      throw std::invalid_argument("Alpha+: boundary cleanup has no incident transition");
    static thread_local std::mt19937_64 generator(std::random_device{}());
    return candidates[std::uniform_int_distribution<std::size_t>(0, candidates.size() - 1)(generator)];
  };
  std::size_t initial = 0;
  const auto first = choose(incident(0, inputs));
  if (outputs[first].size() == 1) {
    const auto next = outputs[first].front();
    if (incident(next, outputs).size() == 1) {
      initial = next;
      erase_transition(first);
      erase_place(0);
    }
  }
  const auto last = choose(incident(1, outputs));
  if (inputs[last].size() == 1) {
    const auto previous = inputs[last].front();
    const auto producers = incident(previous, outputs);
    const bool exclusive = std::all_of(producers.begin(), producers.end(),
        [&](std::size_t t) { return outputs[t].size() == 1; });
    if (exclusive && incident(previous, inputs).size() == 1) {
      erase_transition(last);
      erase_place(previous);
      for (const auto t : producers)
        if (live_transitions[t]) outputs[t].push_back(1);
    }
  }
  AlphaPlusDiscoveryResult result;
  auto& net = result.net;
  net.set_activities(visible_names);
  std::vector<std::size_t> place_ids(names.size());
  for (std::size_t p = 0; p < names.size(); ++p) {
    if (!live_places[p]) continue;
    place_ids[p] = net.add_place();
    result.place_names.push_back(names[p]);
  }
  for (std::size_t t = 0; t < n; ++t) {
    if (!live_transitions[t]) continue;
    if (options.remove_unconnected && inputs[t].empty() && outputs[t].empty()) continue;
    for (auto& p : inputs[t]) p = place_ids[p];
    for (auto& p : outputs[t]) p = place_ids[p];
    const bool hidden = !loops[t] && (t == start_label || t == end_label);
    net.add_transition(hidden ? std::nullopt : std::optional<std::size_t>(t),
                       std::move(inputs[t]), std::move(outputs[t]));
  }
  result.initial_marking.resize(net.place_count());
  result.final_marking.resize(net.place_count());
  if (live_places[initial]) {
    net.set_source(place_ids[initial]);
    result.initial_marking[place_ids[initial]] = 1;
  } else result.detached_initial_marking.push_back({names[initial], 1});
  if (live_places[1]) {
    net.set_sink(place_ids[1]);
    result.final_marking[place_ids[1]] = 1;
  } else result.detached_final_marking.push_back({names[1], 1});

  return result;
}

PetriNet discover_alpha_plus(const EventLog& log,
                             const AlphaMinerOptions& options) {
  auto result = discover_alpha_plus_result(log, options);
  if (!result.detached_initial_marking.empty() ||
      !result.detached_final_marking.empty())
    throw std::invalid_argument(
        "Alpha+: boundary cleanup leaves no valid initial marking");
  return std::move(result.net);
}

PetriNet discover_petri_net(const EventLog& log,
                            const AlphaMinerOptions& options) {
  return options.short_loops ? discover_alpha_plus(log, options)
                             : discover_alpha_classic(log, options);
}

PetriNet discover_petri_net(const DirectlyFollowsGraph& graph,
                            const AlphaMinerOptions& options) {
  if (options.short_loops) {
    throw std::invalid_argument(
        "Alpha: Alpha+ requires event-log trace context; direct graph input supports classic only");
  }
  return build_classic_net(graph.activities(), footprint_from_graph(graph),
                           false);
}

PetriNet discover_petri_net(
    const DfgFrequency& graph,
    const std::map<std::string, std::uint64_t>& starts,
    const std::map<std::string, std::uint64_t>& ends,
    const AlphaMinerOptions& options) {
  if (options.short_loops)
    throw std::invalid_argument(
        "Alpha: Alpha+ requires event-log trace context; direct graph input supports classic only");
  std::set<std::string> labels;
  for (const auto& [edge, frequency] : graph) {
    (void)frequency;
    labels.insert(edge.first);
    labels.insert(edge.second);
  }
  for (const auto& [label, frequency] : starts) {
    (void)frequency;
    labels.insert(label);
  }
  for (const auto& [label, frequency] : ends) {
    (void)frequency;
    labels.insert(label);
  }
  const std::vector<std::string> names(labels.begin(), labels.end());
  Footprint footprint;
  footprint.follows.assign(names.size(), std::vector<bool>(names.size()));
  for (std::size_t a = 0; a < names.size(); ++a) {
    if (starts.contains(names[a])) footprint.starts.insert(a);
    if (ends.contains(names[a])) footprint.ends.insert(a);
    for (std::size_t b = 0; b < names.size(); ++b)
      footprint.follows[a][b] = graph.contains({names[a], names[b]});
  }
  set_classic_relations(footprint, names, graph);
  return build_classic_net(names, footprint, false);
}

PetriNet discover_petri_net(const DfgFrequency& graph,
                            const AlphaMinerOptions& options) {
  const auto endpoints = infer_alpha_endpoints(graph);
  std::map<std::string, std::uint64_t> starts;
  std::map<std::string, std::uint64_t> ends;
  for (const auto& activity : endpoints.starts) starts.emplace(activity, 1);
  for (const auto& activity : endpoints.ends) ends.emplace(activity, 1);
  return discover_petri_net(graph, starts, ends, options);
}

PetriNet discover_petri_net(const DfgCleanAnalysis& graph,
                            const AlphaMinerOptions& options) {
  return discover_petri_net(graph.graph, graph.start_activities,
                            graph.end_activities, options);
}

}  // namespace minera
