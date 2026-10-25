#include "minera/decomposed_reference.hpp"

#include <algorithm>
#include <deque>
#include <functional>
#include <map>
#include <queue>
#include <tuple>

namespace minera {

namespace {

constexpr long long kDeviation = 10000;
constexpr long long kSilent = 1;
constexpr std::size_t kMaxStates = 5000000;

using Transition = std::pair<std::string, std::optional<std::string>>;

DecomposedReferenceComponent component_of(const DecomposedReferenceNet& net, const std::set<std::string>& places) {
  DecomposedReferenceComponent out;
  out.net.places.assign(places.begin(), places.end());
  std::set<std::string> touched;
  for (const auto& [source, target] : net.arcs) {
    if (places.count(source)) {
      touched.insert(target);
      out.net.arcs.emplace_back(source, target);
    } else if (places.count(target)) {
      touched.insert(source);
      out.net.arcs.emplace_back(source, target);
    }
  }
  std::sort(out.net.arcs.begin(), out.net.arcs.end());
  for (const auto& transition : net.transitions) {
    if (!touched.count(transition.first)) continue;
    out.net.transitions.push_back(transition);
    if (transition.second) out.labels.push_back(*transition.second);
  }
  std::sort(out.net.transitions.begin(), out.net.transitions.end());
  std::sort(out.labels.begin(), out.labels.end());
  for (const auto& entry : net.initial)
    if (places.count(entry.first)) out.net.initial.push_back(entry);
  for (const auto& entry : net.final)
    if (places.count(entry.first)) out.net.final.push_back(entry);
  std::sort(out.net.initial.begin(), out.net.initial.end());
  std::sort(out.net.final.begin(), out.net.final.end());
  return out;
}

// The synchronous product of a trace and a net, searched by Dijkstra.
class Aligner {
 public:
  Aligner(const DecomposedReferenceNet& net, const std::vector<std::string>& trace) : net_(net), trace_(trace) {
    std::map<std::string, std::size_t> index;
    for (std::size_t p = 0; p < net.places.size(); ++p) index.emplace(net.places[p], p);
    std::map<std::string, std::size_t> transition;
    for (std::size_t t = 0; t < net.transitions.size(); ++t) transition.emplace(net.transitions[t].first, t);
    pre_.resize(net.transitions.size());
    post_.resize(net.transitions.size());
    for (const auto& [source, target] : net.arcs) {
      if (index.count(source) && transition.count(target)) pre_[transition.at(target)].push_back(index.at(source));
      if (transition.count(source) && index.count(target)) post_[transition.at(source)].push_back(index.at(target));
    }
    start_.assign(net.places.size(), 0);
    final_.assign(net.places.size(), 0);
    for (const auto& [place, tokens] : net.initial) start_[index.at(place)] += static_cast<int>(tokens);
    for (const auto& [place, tokens] : net.final) final_[index.at(place)] += static_cast<int>(tokens);
  }

  std::optional<DecomposedReferenceAlignment> run() {
    struct Step {
      std::size_t parent;
      int kind;  // 0 log, 1 model, 2 sync
      std::size_t transition;
    };
    std::map<State, std::size_t> seen;
    std::vector<State> states;
    std::vector<long long> costs;
    std::vector<Step> steps;
    std::vector<bool> closed;
    using Entry = std::pair<long long, std::size_t>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<>> open;
    auto reach = [&](State state, long long cost, Step step) {
      const auto found = seen.find(state);
      if (found == seen.end()) {
        seen.emplace(state, states.size());
        states.push_back(std::move(state));
        costs.push_back(cost);
        steps.push_back(step);
        closed.push_back(false);
        open.emplace(cost, states.size() - 1);
      } else if (!closed[found->second] && cost < costs[found->second]) {
        costs[found->second] = cost;
        steps[found->second] = step;
        open.emplace(cost, found->second);
      }
    };
    reach({start_, 0}, 0, {static_cast<std::size_t>(-1), -1, 0});
    while (!open.empty()) {
      const auto [cost, at] = open.top();
      open.pop();
      if (closed[at] || cost != costs[at]) continue;
      closed[at] = true;
      const State state = states[at];
      if (state.second == trace_.size() && state.first == final_) return path(at, costs[at], states, steps);
      if (states.size() > kMaxStates) throw DecomposedReferenceError("alignment search exceeds its state limit");
      if (state.second < trace_.size()) {
        reach({state.first, state.second + 1}, cost + kDeviation, {at, 0, 0});
      }
      for (std::size_t t = 0; t < net_.transitions.size(); ++t) {
        bool enabled = true;
        auto marking = state.first;
        for (const auto p : pre_[t]) {
          if (marking[p] == 0) {
            enabled = false;
            break;
          }
          --marking[p];
        }
        if (!enabled) continue;
        for (const auto p : post_[t]) ++marking[p];
        const auto& label = net_.transitions[t].second;
        reach({marking, state.second}, cost + (label ? kDeviation : kSilent), {at, 1, t});
        if (label && state.second < trace_.size() && trace_[state.second] == *label)
          reach({marking, state.second + 1}, cost, {at, 2, t});
      }
    }
    return std::nullopt;
  }

 private:
  using State = std::pair<std::vector<int>, std::size_t>;

  template <typename Step>
  DecomposedReferenceAlignment path(std::size_t at, long long cost, const std::vector<State>& states,
                                    const std::vector<Step>& steps) const {
    DecomposedReferenceAlignment out;
    out.cost = cost;
    while (steps[at].kind >= 0) {
      const auto& step = steps[at];
      const auto position = states[step.parent].second;
      DecomposedReferenceMove move;
      if (step.kind == 1) {
        move.trace_transition = ">>";
        move.log = ">>";
      } else {
        move.log = trace_[position];
        move.trace_transition = "t_" + move.log + "_" + std::to_string(position);
      }
      if (step.kind == 0) {
        move.model_transition = ">>";
        move.model = ">>";
      } else {
        move.model_transition = net_.transitions[step.transition].first;
        move.model = net_.transitions[step.transition].second;
      }
      out.moves.push_back(std::move(move));
      at = step.parent;
    }
    std::reverse(out.moves.begin(), out.moves.end());
    return out;
  }

  const DecomposedReferenceNet& net_;
  const std::vector<std::string>& trace_;
  std::vector<std::vector<std::size_t>> pre_, post_;
  std::vector<int> start_, final_;
};

using Activities = std::map<std::string, std::vector<int>>;

Activities activities_of(const DecomposedReferenceMoves& moves) {
  Activities out;
  for (const auto& move : moves)
    if (move.log != ">>") out[move.log].push_back(move.model == ">>" ? 1 : 0);
  return out;
}

std::vector<std::string> transition_set(const DecomposedReferenceComponent& component) {
  std::vector<std::string> out;
  for (const auto& transition : component.net.transitions) out.push_back(transition.first);
  std::sort(out.begin(), out.end());
  return out;
}

}  // namespace

std::vector<DecomposedReferenceComponent> reference_decompose(const DecomposedReferenceNet& net) {
  std::map<std::string, std::size_t> index;
  for (std::size_t p = 0; p < net.places.size(); ++p) index.emplace(net.places[p], p);
  std::map<std::string, std::vector<std::size_t>> touching;
  for (const auto& [source, target] : net.arcs) {
    if (index.count(source)) touching[target].push_back(index.at(source));
    if (index.count(target)) touching[source].push_back(index.at(target));
  }
  std::map<std::string, std::size_t> label_count;
  std::map<std::string, std::string> last;
  for (const auto& [name, label] : net.transitions)
    if (label) ++label_count[*label], last[*label] = name;
  std::vector<std::size_t> parent(net.places.size());
  for (std::size_t p = 0; p < parent.size(); ++p) parent[p] = p;
  std::function<std::size_t(std::size_t)> root = [&](std::size_t p) {
    return parent[p] == p ? p : parent[p] = root(parent[p]);
  };
  for (const auto& [name, label] : net.transitions) {
    if (label && (label_count[*label] < 2 || last[*label] != name)) continue;
    const auto& places = touching[name];
    for (std::size_t k = 1; k < places.size(); ++k) parent[root(places[k])] = root(places[0]);
  }
  std::vector<std::size_t> order;
  std::map<std::size_t, std::set<std::string>> groups;
  for (std::size_t p = 0; p < net.places.size(); ++p) {
    const auto r = root(p);
    if (!groups.count(r)) order.push_back(r);
    groups[r].insert(net.places[p]);
  }
  std::vector<DecomposedReferenceComponent> out;
  for (const auto r : order) out.push_back(component_of(net, groups[r]));
  return out;
}

DecomposedReferenceComponent reference_decomposed_merge(const std::vector<DecomposedReferenceComponent>& components) {
  std::set<std::string> places;
  std::map<std::string, std::optional<std::string>> transitions;
  std::set<std::pair<std::string, std::string>> arcs;
  std::map<std::string, std::size_t> initial, final;
  for (const auto& component : components) {
    places.insert(component.net.places.begin(), component.net.places.end());
    for (const auto& transition : component.net.transitions) transitions[transition.first] = transition.second;
    arcs.insert(component.net.arcs.begin(), component.net.arcs.end());
    for (const auto& [place, tokens] : component.net.initial) initial[place] = tokens;
    for (const auto& [place, tokens] : component.net.final) final[place] = tokens;
  }
  DecomposedReferenceComponent out;
  out.net.places.assign(places.begin(), places.end());
  for (const auto& transition : transitions) {
    out.net.transitions.push_back(transition);
    if (transition.second) out.labels.push_back(*transition.second);
  }
  std::sort(out.labels.begin(), out.labels.end());
  out.net.arcs.assign(arcs.begin(), arcs.end());
  out.net.initial.assign(initial.begin(), initial.end());
  out.net.final.assign(final.begin(), final.end());
  return out;
}

long long reference_decomposed_move_cost(const DecomposedReferenceMove& move) {
  if (move.log != ">>" && move.model != ">>") return 0;
  return move.log == ">>" && !move.model ? kSilent : kDeviation;
}

std::optional<DecomposedReferenceAlignment> reference_decomposed_alignment(const DecomposedReferenceNet& net,
                                                                           const std::vector<std::string>& trace) {
  return Aligner(net, trace).run();
}

std::size_t reference_decomposed_best_worst_cost(const DecomposedReferenceNet& net) {
  const auto alignment = reference_decomposed_alignment(net, {});
  if (!alignment) throw DecomposedReferenceError("the final marking is unreachable");
  std::size_t visible = 0;
  for (const auto& move : alignment->moves) visible += move.model.has_value();
  return visible;
}

std::vector<std::size_t> reference_decomposed_order(std::vector<std::size_t> nodes,
                                                    const std::set<std::pair<std::size_t, std::size_t>>& edges) {
  for (bool swapped = true; swapped;) {
    swapped = false;
    for (std::size_t i = 0; i + 1 < nodes.size(); ++i) {
      if (edges.count({nodes[i + 1], nodes[i]}) && !edges.count({nodes[i], nodes[i + 1]})) {
        std::swap(nodes[i], nodes[i + 1]);
        swapped = true;
      }
    }
  }
  return nodes;
}

DecomposedReferenceMoves reference_decomposed_recompose(
    const std::vector<bool>& marked, const std::vector<std::optional<DecomposedReferenceMoves>>& alignments) {
  const auto count = alignments.size();
  auto aligned = [&](std::size_t i) { return alignments[i] && !alignments[i]->empty(); };
  std::vector<std::size_t> nodes;
  for (std::size_t i = 0; i < count; ++i)
    if (aligned(i)) nodes.push_back(i);
  std::set<std::pair<std::size_t, std::size_t>> edges;
  std::vector<std::vector<std::size_t>> next(count);
  auto labels = [](const DecomposedReferenceMove& move) { return std::tie(move.log, move.model); };
  for (const auto i : nodes) {
    for (const auto j : nodes) {
      if (i != j && labels(alignments[i]->back()) == labels(alignments[j]->front())) {
        edges.insert({i, j});
        next[i].push_back(j);
      }
    }
  }
  // The first queue never ends once it reaches a link cycle.
  std::vector<int> colour(count, 0);
  std::function<bool(std::size_t)> cyclic = [&](std::size_t i) {
    colour[i] = 1;
    for (const auto j : next[i]) {
      if (colour[j] == 1 || (colour[j] == 0 && cyclic(j))) return true;
    }
    colour[i] = 2;
    return false;
  };
  std::deque<std::size_t> queue;
  for (std::size_t i = 0; i < count; ++i) {
    if (!marked[i]) continue;
    queue.push_back(i);
    if (colour[i] == 0 && cyclic(i)) throw DecomposedReferenceError("recompose_alignment does not terminate");
  }
  DecomposedReferenceMoves out;
  std::vector<bool> visited(count, false);
  std::size_t popped = 0;
  while (!queue.empty()) {
    const auto at = queue.front();
    queue.pop_front();
    queue.insert(queue.end(), next[at].begin(), next[at].end());
    if (alignments[at]) out.insert(out.end(), alignments[at]->begin() + (popped > 0 && !alignments[at]->empty()), alignments[at]->end());
    visited[at] = true;
    ++popped;
  }
  std::vector<std::size_t> rest;
  for (const auto i : nodes)
    if (!visited[i]) rest.push_back(i);
  std::deque<std::size_t> second;
  for (const auto i : reference_decomposed_order(rest, edges)) second.push_back(i);
  std::set<DecomposedReferenceMove> added;
  while (!second.empty()) {
    const auto at = second.front();
    second.pop_front();
    if (visited[at]) continue;
    second.insert(second.end(), next[at].begin(), next[at].end());
    const auto& moves = *alignments[at];
    for (std::size_t k = popped > 0 ? 1 : 0; k < moves.size(); ++k) {
      if (added.insert(moves[k]).second) out.push_back(moves[k]);
    }
    visited[at] = true;
    ++popped;
  }
  return out;
}

std::vector<DecomposedReferenceResult> reference_decomposed_alignments(
    const DecomposedReferenceNet& net, const std::vector<std::vector<std::string>>& traces,
    const DecomposedReferenceOracle& oracle) {
  const auto bwc = static_cast<long long>(reference_decomposed_best_worst_cost(net));
  const auto decomposition = reference_decompose(net);
  // Component objects: the decomposition, then every merge.
  std::vector<DecomposedReferenceComponent> objects(decomposition.begin(), decomposition.end());
  std::map<std::pair<std::size_t, std::vector<std::string>>, std::optional<DecomposedReferenceAlignment>> alignment_cache;
  std::map<std::set<std::vector<std::string>>, std::size_t> merge_cache;
  std::map<std::vector<std::string>, DecomposedReferenceResult> done;
  std::vector<DecomposedReferenceResult> out;
  for (const auto& trace : traces) {
    if (const auto found = done.find(trace); found != done.end()) {
      out.push_back(found->second);
      continue;
    }
    if (oracle.variant) oracle.variant(trace);
    std::vector<std::size_t> components;
    for (std::size_t k = 0; k < decomposition.size(); ++k) components.push_back(k);
    auto cache_of = [&] {
      std::map<std::string, std::vector<std::size_t>> cache;
      for (std::size_t i = 0; i < components.size(); ++i)
        for (const auto& label : objects[components[i]].labels) cache[label].push_back(i);
      return cache;
    };
    auto activity_cache = cache_of();
    std::vector<std::optional<DecomposedReferenceAlignment>> results;
    std::vector<std::optional<Activities>> agreements;
    std::size_t i = 0;
    while (i < components.size()) {
      const auto object = components[i];
      const auto& labels = objects[object].labels;
      std::vector<std::string> projection;
      for (const auto& event : trace)
        if (std::binary_search(labels.begin(), labels.end(), event)) projection.push_back(event);
      if (projection.empty()) {
        results.emplace_back();
        agreements.emplace_back();
        ++i;
        continue;
      }
      const auto key = std::make_pair(object, projection);
      auto cached = alignment_cache.find(key);
      if (cached == alignment_cache.end()) {
        auto alignment = oracle.align ? oracle.align(object, objects[object], projection)
                                      : reference_decomposed_alignment(objects[object].net, projection);
        cached = alignment_cache.emplace(key, std::move(alignment)).first;
      }
      results.push_back(cached->second);
      agreements.push_back(cached->second ? std::optional<Activities>(activities_of(cached->second->moves)) : std::nullopt);
      std::set<std::size_t> to_merge;
      for (const auto& [activity, holders] : activity_cache) {
        std::vector<const std::vector<int>*> values;
        static const std::vector<int> missing;
        for (const auto j : holders) {
          if (j >= agreements.size() || !agreements[j]) continue;
          const auto found = agreements[j]->find(activity);
          values.push_back(found == agreements[j]->end() ? nullptr : &found->second);
        }
        bool differ = false;
        for (std::size_t a = 0; a < values.size() && !differ; ++a) {
          for (std::size_t b = a + 1; b < values.size() && !differ; ++b) {
            const auto& left = values[a] ? *values[a] : missing;
            const auto& right = values[b] ? *values[b] : missing;
            differ = (values[a] == nullptr) != (values[b] == nullptr) || left != right;
          }
        }
        if (differ) to_merge.insert(holders.begin(), holders.end());
      }
      if (to_merge.empty()) {
        ++i;
        continue;
      }
      std::vector<std::size_t> descending(to_merge.rbegin(), to_merge.rend());
      std::vector<std::size_t> inputs;
      std::set<std::vector<std::string>> merge_key;
      std::vector<DecomposedReferenceComponent> parts;
      for (const auto k : descending) {
        inputs.push_back(components[k]);
        merge_key.insert(transition_set(objects[components[k]]));
        parts.push_back(objects[components[k]]);
      }
      std::size_t merged;
      if (const auto found = merge_cache.find(merge_key); found != merge_cache.end()) {
        merged = found->second;
      } else {
        merged = objects.size();
        objects.push_back(reference_decomposed_merge(parts));
        merge_cache.emplace(merge_key, merged);
        if (oracle.merged) oracle.merged(inputs, merged);
      }
      for (const auto k : descending) {
        components.erase(components.begin() + static_cast<std::ptrdiff_t>(k));
        if (k < results.size()) {
          results.erase(results.begin() + static_cast<std::ptrdiff_t>(k));
          agreements.erase(agreements.begin() + static_cast<std::ptrdiff_t>(k));
        }
      }
      components.push_back(merged);
      activity_cache = cache_of();
      i = results.size();
    }
    if (oracle.recomposed) oracle.recomposed(components);
    std::vector<bool> marked;
    std::vector<std::optional<DecomposedReferenceMoves>> moves;
    for (std::size_t k = 0; k < components.size(); ++k) {
      marked.push_back(!objects[components[k]].net.initial.empty());
      if (k < results.size() && results[k]) moves.emplace_back(results[k]->moves);
      else moves.emplace_back();
    }
    const auto alignment = reference_decomposed_recompose(marked, moves);
    DecomposedReferenceResult result;
    long long deviations = 0;
    for (const auto& move : alignment) {
      const auto cost = reference_decomposed_move_cost(move);
      result.cost += cost;
      if (cost >= kDeviation) deviations += cost;
      result.alignment.emplace_back(move.log, move.model);
    }
    if (!trace.empty()) {
      const auto total = (bwc + static_cast<long long>(trace.size())) * kDeviation;
      result.bwc = total;
      result.fitness = 1.0 - static_cast<double>(deviations) / static_cast<double>(total);
    }
    done.emplace(trace, result);
    out.push_back(std::move(result));
  }
  return out;
}

}  // namespace minera
