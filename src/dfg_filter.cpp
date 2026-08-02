#include "minera/dfg_filter.hpp"

#include <algorithm>
#include <cmath>
#include <deque>
#include <iterator>
#include <limits>
#include <set>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace minera {
namespace {

using ActivitySet = std::set<std::string>;
using Adjacency = std::map<std::string, ActivitySet>;

void validate_fraction(double value, const char* name) {
  if (!std::isfinite(value) || value < 0.0 || value > 1.0) {
    throw std::invalid_argument(std::string(name) +
                                " must be finite and in [0, 1]");
  }
}

void validate_threshold(double value, const char* name) {
  if (!std::isfinite(value)) {
    throw std::invalid_argument(std::string(name) + " must be finite");
  }
}

Adjacency forward_adjacency(const DfgFilterData& data) {
  Adjacency out;
  for (const auto& [activity, count] : data.activities) {
    (void)count;
    out[activity];
  }
  for (const auto& [edge, count] : data.edges) {
    (void)count;
    if (data.activities.count(edge.first) && data.activities.count(edge.second)) {
      out[edge.first].insert(edge.second);
    }
  }
  return out;
}

Adjacency reverse_adjacency(const DfgFilterData& data) {
  Adjacency out;
  for (const auto& [activity, count] : data.activities) {
    (void)count;
    out[activity];
  }
  for (const auto& [edge, count] : data.edges) {
    (void)count;
    if (data.activities.count(edge.first) && data.activities.count(edge.second)) {
      out[edge.second].insert(edge.first);
    }
  }
  return out;
}

ActivitySet reachable(const Adjacency& adjacency,
                      const ActivitySet& seeds) {
  ActivitySet result;
  std::deque<std::string> queue;
  for (const auto& seed : seeds) {
    if (adjacency.count(seed) && result.insert(seed).second) queue.push_back(seed);
  }
  while (!queue.empty()) {
    const auto current = std::move(queue.front());
    queue.pop_front();
    const auto it = adjacency.find(current);
    if (it == adjacency.end()) continue;
    for (const auto& next : it->second) {
      if (result.insert(next).second) queue.push_back(next);
    }
  }
  return result;
}

ActivitySet start_set(const DfgFilterData& data) {
  ActivitySet result;
  for (const auto& [activity, count] : data.start_activities) {
    if (count != 0 && data.activities.count(activity)) result.insert(activity);
  }
  return result;
}

ActivitySet end_set(const DfgFilterData& data) {
  ActivitySet result;
  for (const auto& [activity, count] : data.end_activities) {
    if (count != 0 && data.activities.count(activity)) result.insert(activity);
  }
  return result;
}

DfgFilterData prune_to_boundaries(DfgFilterData data) {
  const auto from_start = reachable(forward_adjacency(data), start_set(data));
  const auto to_end = reachable(reverse_adjacency(data), end_set(data));
  ActivitySet keep;
  std::set_intersection(from_start.begin(), from_start.end(), to_end.begin(),
                        to_end.end(), std::inserter(keep, keep.end()));

  for (auto it = data.activities.begin(); it != data.activities.end();) {
    if (keep.count(it->first)) {
      ++it;
    } else {
      data.start_activities.erase(it->first);
      data.end_activities.erase(it->first);
      it = data.activities.erase(it);
    }
  }
  for (auto it = data.edges.begin(); it != data.edges.end();) {
    if (data.activities.count(it->first.first) &&
        data.activities.count(it->first.second)) {
      ++it;
    } else {
      it = data.edges.erase(it);
    }
  }
  return data;
}

bool preserves_required(const DfgFilterData& data,
                        const ActivitySet& required) {
  const auto from_start = reachable(forward_adjacency(data), start_set(data));
  if (!std::includes(from_start.begin(), from_start.end(), required.begin(),
                     required.end())) {
    return false;
  }
  const auto to_end = reachable(reverse_adjacency(data), end_set(data));
  return std::includes(to_end.begin(), to_end.end(), required.begin(),
                       required.end());
}

void erase_activity(DfgFilterData& data, const std::string& activity) {
  data.activities.erase(activity);
  data.start_activities.erase(activity);
  data.end_activities.erase(activity);
  for (auto it = data.edges.begin(); it != data.edges.end();) {
    if (it->first.first == activity || it->first.second == activity) {
      it = data.edges.erase(it);
    } else {
      ++it;
    }
  }
}

enum class BoundaryEdge { kNormal, kStart, kEnd };

struct EdgeKey {
  BoundaryEdge kind = BoundaryEdge::kNormal;
  std::string source;
  std::string target;
};

void erase_edge(DfgFilterData& data, const EdgeKey& edge) {
  if (edge.kind == BoundaryEdge::kStart) {
    data.start_activities.erase(edge.target);
  } else if (edge.kind == BoundaryEdge::kEnd) {
    data.end_activities.erase(edge.source);
  } else {
    data.edges.erase({edge.source, edge.target});
  }
}

std::vector<std::pair<EdgeKey, std::uint64_t>> all_edges(
    const DfgFilterData& data) {
  std::vector<std::pair<EdgeKey, std::uint64_t>> result;
  result.reserve(data.edges.size() + data.start_activities.size() +
                data.end_activities.size());
  for (const auto& [edge, count] : data.edges) {
    result.push_back({{BoundaryEdge::kNormal, edge.first, edge.second}, count});
  }
  for (const auto& [activity, count] : data.start_activities) {
    result.push_back(
        {{BoundaryEdge::kStart, {}, activity}, count});
  }
  for (const auto& [activity, count] : data.end_activities) {
    result.push_back(
        {{BoundaryEdge::kEnd, activity, {}}, count});
  }
  return result;
}

}  // namespace

DfgFilterData make_dfg_filter_data(const DirectlyFollowsGraph& graph) {
  DfgFilterData data;
  for (const auto& activity : graph.activities()) {
    data.activities[activity] = graph.activity_count(activity);
    const auto starts = graph.start_activity_count(activity);
    const auto ends = graph.end_activity_count(activity);
    if (starts != 0) data.start_activities[activity] = starts;
    if (ends != 0) data.end_activities[activity] = ends;
  }
  for (const auto& edge : graph.edges()) {
    if (edge.source >= graph.activities().size() ||
        edge.target >= graph.activities().size()) {
      throw std::invalid_argument("DFG edge refers to an unknown activity");
    }
    data.edges[{graph.activities()[edge.source], graph.activities()[edge.target]}] =
        edge.count;
  }
  return data;
}

DfgFilterData filter_dfg_on_activities_percentage(
    const DfgFilterData& input, double percentage) {
  validate_fraction(percentage, "DFG activity percentage");
  DfgFilterData data = input;
  if (data.activities.size() <= 1 || data.edges.size() <= 1) {
    return data;
  }

  std::vector<std::pair<std::string, std::uint64_t>> sorted(
      data.activities.begin(), data.activities.end());
  std::sort(sorted.begin(), sorted.end(),
            [](const auto& left, const auto& right) {
              return std::tie(left.second, left.first) >
                     std::tie(right.second, right.first);
            });
  const auto keep_count = static_cast<std::size_t>(
      std::ceil((sorted.size() - 1) * percentage) + 1.0);
  ActivitySet required;
  for (std::size_t i = 0; i < std::min(keep_count, sorted.size()); ++i) {
    required.insert(sorted[i].first);
  }

  std::vector<std::string> candidates;
  for (std::size_t i = sorted.size(); i > keep_count; --i) {
    candidates.push_back(sorted[i - 1].first);
  }
  for (const auto& activity : candidates) {
    DfgFilterData trial = data;
    erase_activity(trial, activity);
    if (preserves_required(trial, required)) data = std::move(trial);
  }
  return prune_to_boundaries(std::move(data));
}

DfgFilterData filter_dfg_on_paths_percentage(const DfgFilterData& input,
                                             double percentage,
                                             bool keep_all_activities) {
  validate_fraction(percentage, "DFG path percentage");
  DfgFilterData data = input;
  if (data.activities.size() <= 1 || data.edges.size() <= 1) {
    return data;
  }

  auto scored = all_edges(data);
  std::sort(scored.begin(), scored.end(),
            [](const auto& left, const auto& right) {
              return std::tie(left.second, left.first.kind, left.first.source,
                              left.first.target) >
                     std::tie(right.second, right.first.kind, right.first.source,
                              right.first.target);
            });
  const auto keep_count = static_cast<std::size_t>(
      std::ceil((scored.size() - 1) * percentage) + 1.0);
  ActivitySet required;
  if (keep_all_activities) {
    for (const auto& [activity, count] : data.activities) {
      (void)count;
      required.insert(activity);
    }
  } else {
    for (std::size_t i = 0; i < std::min(keep_count, scored.size()); ++i) {
      const auto& edge = scored[i].first;
      if (edge.kind != BoundaryEdge::kStart) required.insert(edge.source);
      if (edge.kind != BoundaryEdge::kEnd) required.insert(edge.target);
    }
  }
  std::vector<EdgeKey> candidates;
  for (std::size_t i = scored.size(); i > keep_count; --i) {
    candidates.push_back(scored[i - 1].first);
  }
  for (const auto& edge : candidates) {
    DfgFilterData trial = data;
    erase_edge(trial, edge);
    if (preserves_required(trial, required)) data = std::move(trial);
  }
  return prune_to_boundaries(std::move(data));
}

DfgFilterData filter_dfg_keep_connected(const DfgFilterData& input,
                                        double dependency_threshold,
                                        bool keep_all_activities) {
  validate_threshold(dependency_threshold, "DFG dependency threshold");
  DfgFilterData data = input;
  if (data.activities.size() <= 1 || data.edges.size() <= 1) {
    return data;
  }

  std::vector<std::pair<EdgeKey, double>> scored;
  scored.reserve(data.edges.size() + data.start_activities.size() +
                 data.end_activities.size());
  for (const auto& [edge, count] : data.edges) {
    const auto inverse = data.edges.find({edge.second, edge.first});
    const double score =
        inverse == data.edges.end()
            ? static_cast<double>(count) / (static_cast<double>(count) + 1.0)
            : (static_cast<double>(count) -
               static_cast<double>(inverse->second)) /
                  (static_cast<double>(count) +
                   static_cast<double>(inverse->second) + 1.0);
    scored.push_back({{BoundaryEdge::kNormal, edge.first, edge.second}, score});
  }
  for (const auto& [activity, count] : data.start_activities) {
    (void)count;
    scored.push_back({{BoundaryEdge::kStart, {}, activity}, 1.0});
  }
  for (const auto& [activity, count] : data.end_activities) {
    (void)count;
    scored.push_back({{BoundaryEdge::kEnd, activity, {}}, 1.0});
  }
  std::sort(scored.begin(), scored.end(),
            [](const auto& left, const auto& right) {
              return std::tie(left.second, left.first.kind, left.first.source,
                              left.first.target) >
                     std::tie(right.second, right.first.kind, right.first.source,
                              right.first.target);
            });
  ActivitySet required;
  if (keep_all_activities) {
    for (const auto& [activity, count] : data.activities) {
      (void)count;
      required.insert(activity);
    }
  } else {
    for (const auto& [edge, score] : scored) {
      if (score < dependency_threshold) continue;
      if (edge.kind != BoundaryEdge::kStart) required.insert(edge.source);
      if (edge.kind != BoundaryEdge::kEnd) required.insert(edge.target);
    }
  }
  for (auto it = scored.rbegin(); it != scored.rend(); ++it) {
    if (it->second >= dependency_threshold) continue;
    DfgFilterData trial = data;
    erase_edge(trial, it->first);
    if (preserves_required(trial, required)) data = std::move(trial);
  }
  return prune_to_boundaries(std::move(data));
}

DfgFilterData filter_dfg_to_activity(const DfgFilterData& input,
                                     const std::string& activity) {
  if (!input.activities.count(activity)) {
    throw std::invalid_argument("DFG target activity is unknown: " + activity);
  }
  DfgFilterData data = input;
  for (auto it = data.edges.begin(); it != data.edges.end();) {
    if (it->first.first == activity) it = data.edges.erase(it);
    else ++it;
  }
  data.end_activities.clear();
  data.end_activities[activity] = data.activities.at(activity);
  return prune_to_boundaries(std::move(data));
}

DfgFilterData filter_dfg_from_activity(const DfgFilterData& input,
                                       const std::string& activity) {
  if (!input.activities.count(activity)) {
    throw std::invalid_argument("DFG source activity is unknown: " + activity);
  }
  DfgFilterData data = input;
  for (auto it = data.edges.begin(); it != data.edges.end();) {
    if (it->first.second == activity) it = data.edges.erase(it);
    else ++it;
  }
  data.start_activities.clear();
  data.start_activities[activity] = data.activities.at(activity);
  return prune_to_boundaries(std::move(data));
}

DfgFilterData filter_dfg_contain_activity(const DfgFilterData& input,
                                          const std::string& activity) {
  if (!input.activities.count(activity)) {
    throw std::invalid_argument("DFG containment activity is unknown: " +
                                activity);
  }
  const auto forward = reachable(forward_adjacency(input), {activity});
  const auto backward = reachable(reverse_adjacency(input), {activity});
  ActivitySet keep = forward;
  keep.insert(backward.begin(), backward.end());
  DfgFilterData data = input;
  for (auto it = data.activities.begin(); it != data.activities.end();) {
    if (keep.count(it->first)) ++it;
    else it = data.activities.erase(it);
  }
  for (auto it = data.edges.begin(); it != data.edges.end();) {
    if (keep.count(it->first.first) && keep.count(it->first.second)) ++it;
    else it = data.edges.erase(it);
  }
  for (auto it = data.start_activities.begin(); it != data.start_activities.end();) {
    if (keep.count(it->first)) ++it;
    else it = data.start_activities.erase(it);
  }
  for (auto it = data.end_activities.begin(); it != data.end_activities.end();) {
    if (keep.count(it->first)) ++it;
    else it = data.end_activities.erase(it);
  }
  return prune_to_boundaries(std::move(data));
}

}  // namespace minera
