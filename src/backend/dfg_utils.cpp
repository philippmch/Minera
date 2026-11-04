#include "minera/dfg_utils.hpp"

#include <algorithm>
#include <deque>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>

namespace minera {
namespace {

void add_saturating(std::uint64_t& destination, std::uint64_t value) {
  const auto maximum = std::numeric_limits<std::uint64_t>::max();
  if (maximum - destination < value) {
    destination = maximum;
  } else {
    destination += value;
  }
}

void add_edge_count(DfgFrequency& graph, const DfgActivityPair& edge,
                    std::uint64_t count) {
  add_saturating(graph[edge], count);
}

DfgActivitySet nodes_from_adjacency(const DfgAdjacency& incoming,
                                    const DfgAdjacency& outgoing) {
  DfgActivitySet nodes;
  for (const auto& [activity, neighbours] : incoming) {
    nodes.insert(activity);
    for (const auto& [neighbour, count] : neighbours) {
      (void)count;
      nodes.insert(neighbour);
    }
  }
  for (const auto& [activity, neighbours] : outgoing) {
    nodes.insert(activity);
    for (const auto& [neighbour, count] : neighbours) {
      (void)count;
      nodes.insert(neighbour);
    }
  }
  return nodes;
}

std::uint64_t maximum_adjacency_value(const DfgAdjacency& adjacency,
                                      const std::string& activity,
                                      bool& found) {
  const auto outer = adjacency.find(activity);
  if (outer == adjacency.end()) return 0;

  std::uint64_t result = 0;
  for (const auto& [neighbour, count] : outer->second) {
    (void)neighbour;
    if (!found || count > result) result = count;
    found = true;
  }
  return result;
}

std::uint64_t adjacency_sum(const DfgAdjacency& adjacency,
                            const std::string& activity) {
  const auto found = adjacency.find(activity);
  if (found == adjacency.end()) return 0;

  std::uint64_t result = 0;
  for (const auto& [neighbour, count] : found->second) {
    (void)neighbour;
    add_saturating(result, count);
  }
  return result;
}

void add_undirected_edge(std::map<std::string, DfgActivitySet>& adjacency,
                         const std::string& first,
                         const std::string& second) {
  adjacency[first].insert(second);
  adjacency[second].insert(first);
}

DfgVariantSummary summarize_weighted_variants(
    const std::vector<TraceVariant>& variants) {
  DfgVariantSummary result;
  DfgActivitySet activities;

  for (const auto& variant : variants) {
    if (variant.activities.empty() || variant.count == 0) continue;

    activities.insert(variant.activities.begin(), variant.activities.end());
    add_saturating(result.start_activities[variant.activities.front()],
                   variant.count);
    add_saturating(result.end_activities[variant.activities.back()],
                   variant.count);
    for (std::size_t i = 1; i < variant.activities.size(); ++i) {
      add_edge_count(result.graph,
                     {variant.activities[i - 1], variant.activities[i]},
                     variant.count);
    }
  }

  result.activities.assign(activities.begin(), activities.end());
  return result;
}

DfgRelation transitive_relation(const DfgFrequency& dfg,
                                const DfgActivitySet& requested,
                                bool successor_direction) {
  const DfgActivitySet& nodes = requested;
  DfgRelation result;
  for (const auto& activity : nodes) result.emplace(activity, DfgActivitySet{});

  std::map<std::string, DfgActivitySet> adjacency;
  for (const auto& [edge, count] : dfg) {
    (void)count;
    if (!nodes.count(edge.first) || !nodes.count(edge.second)) continue;
    if (successor_direction) {
      adjacency[edge.first].insert(edge.second);
    } else {
      adjacency[edge.second].insert(edge.first);
    }
  }

  // A breadth-first walk from each node computes the transitive closure and
  // naturally retains the source itself only when a cycle reaches it.
  for (const auto& source : nodes) {
    std::deque<std::string> pending;
    const auto first = adjacency.find(source);
    if (first != adjacency.end()) {
      for (const auto& next : first->second) pending.push_back(next);
    }

    DfgActivitySet visited;
    while (!pending.empty()) {
      const std::string current = std::move(pending.front());
      pending.pop_front();
      if (!visited.insert(current).second) continue;
      result[source].insert(current);

      const auto next = adjacency.find(current);
      if (next == adjacency.end()) continue;
      for (const auto& neighbour : next->second) pending.push_back(neighbour);
    }
  }
  return result;
}

}  // namespace

DfgAdjacency outgoing_adjacency(const DfgFrequency& dfg) {
  DfgAdjacency result;
  for (const auto& [edge, count] : dfg) {
    result[edge.first][edge.second] = count;
  }
  return result;
}

DfgAdjacency incoming_adjacency(const DfgFrequency& dfg) {
  DfgAdjacency result;
  for (const auto& [edge, count] : dfg) {
    result[edge.second][edge.first] = count;
  }
  return result;
}

DfgActivityList infer_start_activities(const DfgFrequency& dfg) {
  const auto incoming = incoming_adjacency(dfg);
  const auto outgoing = outgoing_adjacency(dfg);
  DfgActivityList result;
  for (const auto& [activity, neighbours] : outgoing) {
    (void)neighbours;
    if (!incoming.count(activity)) result.push_back(activity);
  }
  return result;
}

DfgActivityList infer_end_activities(const DfgFrequency& dfg) {
  const auto incoming = incoming_adjacency(dfg);
  const auto outgoing = outgoing_adjacency(dfg);
  DfgActivityList result;
  for (const auto& [activity, neighbours] : incoming) {
    (void)neighbours;
    if (!outgoing.count(activity)) result.push_back(activity);
  }
  return result;
}

DfgActivitySet infer_start_activities_from_parent(
    const DfgFrequency& parent_dfg, const DfgFrequency& current_dfg,
    const DfgActivitySet& activities, bool include_current) {
  DfgActivitySet result;
  for (const auto& [edge, count] : parent_dfg) {
    (void)count;
    if (activities.count(edge.second) && !activities.count(edge.first)) {
      result.insert(edge.second);
    }
  }
  if (include_current) {
    const auto current_starts = infer_start_activities(current_dfg);
    result.insert(current_starts.begin(), current_starts.end());
  }
  return result;
}

DfgActivitySet infer_end_activities_from_parent(
    const DfgFrequency& parent_dfg, const DfgFrequency& current_dfg,
    const DfgActivitySet& activities, bool include_current) {
  DfgActivitySet result;
  for (const auto& [edge, count] : parent_dfg) {
    (void)count;
    if (activities.count(edge.first) && !activities.count(edge.second)) {
      result.insert(edge.first);
    }
  }
  if (include_current) {
    const auto current_ends = infer_end_activities(current_dfg);
    result.insert(current_ends.begin(), current_ends.end());
  }
  return result;
}

DfgActivitySet outside_outputs_to_current_starts(
    const DfgFrequency& parent_dfg, const DfgFrequency& current_dfg,
    const DfgActivitySet& activities) {
  const auto starts = infer_start_activities_from_parent(
      parent_dfg, current_dfg, activities, false);
  DfgActivitySet outside_predecessors;
  for (const auto& [edge, count] : parent_dfg) {
    (void)count;
    if (!activities.count(edge.first) && starts.count(edge.second)) {
      outside_predecessors.insert(edge.first);
    }
  }

  DfgActivitySet result;
  for (const auto& [edge, count] : parent_dfg) {
    (void)count;
    if (outside_predecessors.count(edge.first) &&
        !activities.count(edge.second)) {
      result.insert(edge.second);
    }
  }
  for (const auto& activity : outside_predecessors) result.erase(activity);
  return result;
}

DfgActivitySet outside_inputs_from_current_ends(
    const DfgFrequency& parent_dfg, const DfgFrequency& current_dfg,
    const DfgActivitySet& activities) {
  const auto ends = infer_end_activities_from_parent(
      parent_dfg, current_dfg, activities, false);
  DfgActivitySet outside_successors;
  for (const auto& [edge, count] : parent_dfg) {
    (void)count;
    if (ends.count(edge.first) && !activities.count(edge.second)) {
      outside_successors.insert(edge.second);
    }
  }

  DfgActivitySet result;
  for (const auto& [edge, count] : parent_dfg) {
    (void)count;
    if (outside_successors.count(edge.second) &&
        !activities.count(edge.first)) {
      result.insert(edge.first);
    }
  }
  for (const auto& activity : outside_successors) result.erase(activity);
  return result;
}

DfgActivitySet alphabet(const DfgFrequency& dfg) {
  DfgActivitySet result;
  for (const auto& [edge, count] : dfg) {
    (void)count;
    result.insert(edge.first);
    result.insert(edge.second);
  }
  return result;
}

DfgActivityList activity_names(const DfgFrequency& dfg) {
  const auto values = alphabet(dfg);
  return {values.begin(), values.end()};
}

std::optional<std::uint64_t> maximum_incident_edge(
    const DfgFrequency& dfg, const std::string& activity) {
  const auto incoming = incoming_adjacency(dfg);
  const auto outgoing = outgoing_adjacency(dfg);
  bool found = false;
  std::uint64_t result = maximum_adjacency_value(incoming, activity, found);
  const auto outgoing_max = maximum_adjacency_value(outgoing, activity, found);
  if (outgoing_max > result) result = outgoing_max;
  if (!found) return std::nullopt;
  return result;
}

std::uint64_t sum_adjacency_values(const DfgAdjacency& adjacency,
                                  const std::string& activity) {
  std::uint64_t result = 0;
  for (const auto& [neighbour, count] : adjacency.at(activity)) {
    (void)neighbour;
    add_saturating(result, count);
  }
  return result;
}

std::optional<std::uint64_t> maximum_incident_sum(const DfgFrequency& dfg) {
  if (dfg.empty()) return std::nullopt;
  const auto incoming = incoming_adjacency(dfg);
  const auto outgoing = outgoing_adjacency(dfg);
  std::uint64_t result = 0;
  bool found = false;

  for (const auto& [activity, neighbours] : incoming) {
    (void)neighbours;
    const auto sum = adjacency_sum(incoming, activity);
    if (!found || sum > result) result = sum;
    found = true;
  }
  for (const auto& [activity, neighbours] : outgoing) {
    (void)neighbours;
    const auto sum = adjacency_sum(outgoing, activity);
    if (!found || sum > result) result = sum;
    found = true;
  }
  return found ? std::optional<std::uint64_t>(result) : std::nullopt;
}

std::optional<std::uint64_t> maximum_incident_sum_for_activities(
    const DfgFrequency& dfg, const DfgActivitySet& activities) {
  const auto incoming = incoming_adjacency(dfg);
  const auto outgoing = outgoing_adjacency(dfg);
  std::uint64_t result = 0;
  bool found = false;

  for (const auto& activity : activities) {
    if (incoming.count(activity)) {
      const auto sum = adjacency_sum(incoming, activity);
      if (!found || sum > result) result = sum;
      found = true;
    }
    if (outgoing.count(activity)) {
      const auto sum = adjacency_sum(outgoing, activity);
      if (!found || sum > result) result = sum;
      found = true;
    }
  }
  return found ? std::optional<std::uint64_t>(result) : std::nullopt;
}

std::uint64_t start_activity_total(const DfgFrequency& dfg) {
  const auto outgoing = outgoing_adjacency(dfg);
  std::uint64_t result = 0;
  for (const auto& activity : infer_start_activities(dfg)) {
    add_saturating(result, adjacency_sum(outgoing, activity));
  }
  return result;
}

std::uint64_t end_activity_total(const DfgFrequency& dfg) {
  const auto incoming = incoming_adjacency(dfg);
  std::uint64_t result = 0;
  for (const auto& activity : infer_end_activities(dfg)) {
    add_saturating(result, adjacency_sum(incoming, activity));
  }
  return result;
}

std::uint64_t activity_total(const DfgFrequency& dfg,
                             const DfgActivityList& activities,
                             bool enable_halving) {
  const auto incoming = incoming_adjacency(dfg);
  const auto outgoing = outgoing_adjacency(dfg);
  std::uint64_t result = 0;
  for (const auto& activity : activities) {
    add_saturating(result, adjacency_sum(outgoing, activity));
    add_saturating(result, adjacency_sum(incoming, activity));
    if (enable_halving && incoming.count(activity) &&
        outgoing.count(activity)) {
      result /= 2;
    }
  }
  return result;
}

std::uint64_t activity_total(const DfgFrequency& dfg,
                             const DfgActivitySet& activities,
                             bool enable_halving) {
  return activity_total(dfg, DfgActivityList(activities.begin(), activities.end()),
                        enable_halving);
}

DfgFrequency project_dfg(const DfgFrequency& dfg,
                         const DfgActivitySet& activities) {
  DfgFrequency result;
  for (const auto& [edge, count] : dfg) {
    if (activities.count(edge.first) && activities.count(edge.second)) {
      result.emplace(edge, count);
    }
  }
  return result;
}

DfgFrequency remove_reciprocal_edges(const DfgFrequency& dfg) {
  DfgFrequency result;
  for (const auto& [edge, count] : dfg) {
    if (!dfg.count({edge.second, edge.first})) result.emplace(edge, count);
  }
  return result;
}

DfgDirectionScores activity_direction_scores(const DfgFrequency& dfg) {
  return activity_direction_scores(dfg, alphabet(dfg));
}

DfgDirectionScores activity_direction_scores(
    const DfgFrequency& dfg, const DfgActivitySet& activities) {
  const auto& selected = activities;
  const auto incoming = incoming_adjacency(dfg);
  const auto outgoing = outgoing_adjacency(dfg);
  DfgDirectionScores result;
  for (const auto& activity : selected) {
    // Python divides the exact integers and rounds once. Counts below 2^53
    // convert exactly, so the double difference, sum and quotient round the
    // same way; a long double quotient rounded twice on x86 only. Larger
    // totals can round differently (declared in the parity doc).
    const auto out = static_cast<double>(adjacency_sum(outgoing, activity));
    const auto in = static_cast<double>(adjacency_sum(incoming, activity));
    result.emplace(activity, (out - in) / (in + out + 1.0));
  }
  return result;
}

DfgDirectionRanking rank_activity_directions(
    const DfgDirectionScores& scores) {
  DfgDirectionRanking result(scores.begin(), scores.end());
  std::sort(result.begin(), result.end(),
            [](const auto& first, const auto& second) {
              if (first.second != second.second) {
                return first.second > second.second;
              }
              return first.first > second.first;
            });
  return result;
}

DfgActivityList self_loop_activities(const DfgFrequency& dfg) {
  DfgActivityList result;
  for (const auto& [edge, count] : dfg) {
    (void)count;
    if (edge.first == edge.second) result.push_back(edge.first);
  }
  return result;
}

std::vector<DfgActivitySet> undirected_connected_components(
    const DfgAdjacency& incoming, const DfgAdjacency& outgoing,
    const DfgActivitySet& activities, bool include_isolated_activities) {
  auto nodes = nodes_from_adjacency(incoming, outgoing);
  if (include_isolated_activities) {
    nodes.insert(activities.begin(), activities.end());
  }

  std::map<std::string, DfgActivitySet> undirected;
  for (const auto& node : nodes) undirected.emplace(node, DfgActivitySet{});
  for (const auto& [source, neighbours] : outgoing) {
    for (const auto& [target, count] : neighbours) {
      (void)count;
      add_undirected_edge(undirected, source, target);
    }
  }
  for (const auto& [target, neighbours] : incoming) {
    for (const auto& [source, count] : neighbours) {
      (void)count;
      add_undirected_edge(undirected, target, source);
    }
  }

  std::vector<DfgActivitySet> result;
  DfgActivitySet visited;
  for (const auto& [root, neighbours] : undirected) {
    (void)neighbours;
    if (visited.count(root)) continue;

    DfgActivitySet component;
    std::deque<std::string> pending{root};
    while (!pending.empty()) {
      const std::string current = std::move(pending.front());
      pending.pop_front();
      if (!visited.insert(current).second) continue;
      component.insert(current);
      const auto found = undirected.find(current);
      if (found == undirected.end()) continue;
      for (const auto& next : found->second) {
        if (!visited.count(next)) pending.push_back(next);
      }
    }
    result.push_back(std::move(component));
  }

  // Keep the compatibility fallback for an empty graph: explicitly supplied
  // activities still become singleton components even when isolated insertion
  // was disabled.
  if (result.empty() && !activities.empty()) {
    for (const auto& activity : activities) {
      result.push_back(DfgActivitySet{activity});
    }
  }
  return result;
}

std::vector<DfgActivitySet> assign_to_strongest_component(
    const std::vector<DfgActivitySet>& components,
    const std::string& activity, const DfgAdjacency& incoming,
    const DfgAdjacency& outgoing) {
  std::vector<DfgActivitySet> result = components;
  if (result.empty()) {
    throw std::invalid_argument(
        "cannot assign an activity without a component collection");
  }

  std::size_t strongest = 0;
  std::uint64_t strongest_score = 0;
  bool score_initialized = false;
  for (std::size_t i = 0; i < result.size(); ++i) {
    std::uint64_t score = 0;
    for (const auto& member : result[i]) {
      const auto incoming_outer = incoming.find(member);
      if (incoming_outer != incoming.end()) {
        const auto edge = incoming_outer->second.find(activity);
        if (edge != incoming_outer->second.end()) add_saturating(score, edge->second);
      }
      const auto outgoing_outer = outgoing.find(member);
      if (outgoing_outer != outgoing.end()) {
        const auto edge = outgoing_outer->second.find(activity);
        if (edge != outgoing_outer->second.end()) add_saturating(score, edge->second);
      }
    }
    if (!score_initialized || score > strongest_score) {
      strongest = i;
      strongest_score = score;
      score_initialized = true;
    }
  }
  result[strongest].insert(activity);
  return result;
}

DfgActivitySet direct_successors(const DfgFrequency& dfg,
                                 const std::string& activity) {
  DfgActivitySet result;
  const auto outgoing = outgoing_adjacency(dfg);
  const auto found = outgoing.find(activity);
  if (found == outgoing.end()) return result;
  for (const auto& [neighbour, count] : found->second) {
    (void)count;
    result.insert(neighbour);
  }
  return result;
}

DfgActivitySet direct_predecessors(const DfgFrequency& dfg,
                                   const std::string& activity) {
  DfgActivitySet result;
  const auto incoming = incoming_adjacency(dfg);
  const auto found = incoming.find(activity);
  if (found == incoming.end()) return result;
  for (const auto& [neighbour, count] : found->second) {
    (void)count;
    result.insert(neighbour);
  }
  return result;
}

DfgDenseMatrix make_dfg_matrix(const DfgFrequency& dfg) {
  const auto names = activity_names(dfg);
  DfgDenseMatrix result;
  result.values.assign(names.size(), std::vector<double>(names.size(), 0.0));
  for (std::size_t i = 0; i < names.size(); ++i) {
    result.index_to_activity.emplace(i, names[i]);
    result.activity_to_index.emplace(names[i], i);
  }
  for (const auto& [edge, count] : dfg) {
    const auto source = result.activity_to_index.find(edge.first);
    const auto target = result.activity_to_index.find(edge.second);
    if (source != result.activity_to_index.end() &&
        target != result.activity_to_index.end()) {
      result.values[source->second][target->second] =
          static_cast<double>(count);
    }
  }
  return result;
}

DfgVariantSummary summarize_variants(
    const std::vector<DfgActivityList>& variants) {
  std::set<DfgActivityList> distinct(variants.begin(), variants.end());
  std::vector<TraceVariant> unweighted;
  unweighted.reserve(distinct.size());
  for (const auto& variant : distinct) {
    unweighted.push_back({variant, 1});
  }
  return summarize_weighted_variants(unweighted);
}

DfgVariantSummary summarize_variant_counts(
    const std::vector<TraceVariant>& variants) {
  return summarize_weighted_variants(variants);
}

const DfgActivitySet& DfgDirectedGraph::successors(
    const std::string& activity) const {
  static const DfgActivitySet empty;
  const auto found = successors_.find(activity);
  return found == successors_.end() ? empty : found->second;
}

const DfgActivitySet& DfgDirectedGraph::predecessors(
    const std::string& activity) const {
  static const DfgActivitySet empty;
  const auto found = predecessors_.find(activity);
  return found == predecessors_.end() ? empty : found->second;
}

bool DfgDirectedGraph::has_edge(const std::string& source,
                                const std::string& target) const {
  return edges_.count({source, target}) != 0;
}

bool DfgDirectedGraph::has_activity(const std::string& activity) const {
  return activities_.count(activity) != 0;
}

DfgDirectedGraph make_directed_graph(const DfgFrequency& dfg) {
  return make_directed_graph(dfg, alphabet(dfg));
}

DfgDirectedGraph make_directed_graph(const DfgFrequency& dfg,
                                     const DfgActivitySet& activities) {
  DfgDirectedGraph result;
  result.activities_ = activities;
  for (const auto& [edge, count] : dfg) {
    (void)count;
    result.activities_.insert(edge.first);
    result.activities_.insert(edge.second);
    result.edges_.insert(edge);
    result.successors_[edge.first].insert(edge.second);
    result.predecessors_[edge.second].insert(edge.first);
  }
  for (const auto& activity : result.activities_) {
    result.successors_.try_emplace(activity);
    result.predecessors_.try_emplace(activity);
  }
  return result;
}

DfgRelation transitive_successors(const DfgFrequency& dfg) {
  return transitive_relation(dfg, alphabet(dfg), true);
}

DfgRelation transitive_successors(const DfgFrequency& dfg,
                                  const DfgActivitySet& activities) {
  return transitive_relation(dfg, activities, true);
}

DfgRelation transitive_predecessors(const DfgFrequency& dfg) {
  return transitive_relation(dfg, alphabet(dfg), false);
}

DfgRelation transitive_predecessors(const DfgFrequency& dfg,
                                    const DfgActivitySet& activities) {
  return transitive_relation(dfg, activities, false);
}

DfgTransitiveRelations transitive_relations(const DfgFrequency& dfg) {
  const auto nodes = alphabet(dfg);
  return {transitive_predecessors(dfg, nodes),
          transitive_successors(dfg, nodes)};
}

DfgTransitiveRelations transitive_relations(const DfgFrequency& dfg,
                                            const DfgActivitySet& activities) {
  return {transitive_predecessors(dfg, activities),
          transitive_successors(dfg, activities)};
}

}  // namespace minera
