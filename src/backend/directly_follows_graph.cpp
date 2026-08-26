#include "minera/directly_follows_graph.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace minera {
namespace {

std::uint64_t edge_key(std::size_t source, std::size_t target) {
  constexpr auto max_u32 = static_cast<std::size_t>(
      std::numeric_limits<std::uint32_t>::max());
  if (source > max_u32 || target > max_u32) {
    throw std::overflow_error(
        "Directly-follows graph supports up to 2^32 unique activities");
  }
  return (static_cast<std::uint64_t>(source) << 32U) |
         static_cast<std::uint64_t>(target);
}

std::string variant_key(const std::vector<std::string>& activities) {
  std::string key;
  for (const auto& activity : activities) {
    key.append(std::to_string(activity.size()));
    key.push_back(':');
    key.append(activity);
    key.push_back('|');
  }
  return key;
}

}  // namespace

std::optional<std::size_t> DirectlyFollowsGraph::activity_id(
    std::string_view activity) const {
  const auto it = activity_ids_.find(std::string(activity));
  if (it == activity_ids_.end()) {
    return std::nullopt;
  }
  return it->second;
}

std::uint64_t DirectlyFollowsGraph::activity_count(
    std::string_view activity) const {
  const auto id = activity_id(activity);
  return id ? activity_counts_[*id] : 0;
}

std::uint64_t DirectlyFollowsGraph::start_activity_count(
    std::string_view activity) const {
  const auto id = activity_id(activity);
  return id ? start_activity_counts_[*id] : 0;
}

std::uint64_t DirectlyFollowsGraph::end_activity_count(
    std::string_view activity) const {
  const auto id = activity_id(activity);
  return id ? end_activity_counts_[*id] : 0;
}

std::uint64_t DirectlyFollowsGraph::rework_case_count(
    std::string_view activity) const {
  const auto id = activity_id(activity);
  return id ? rework_case_counts_[*id] : 0;
}

std::uint64_t DirectlyFollowsGraph::edge_count(
    std::string_view source,
    std::string_view target) const {
  const auto source_id = activity_id(source);
  const auto target_id = activity_id(target);
  if (!source_id || !target_id) {
    return 0;
  }

  const auto it = edge_ids_.find(edge_key(*source_id, *target_id));
  if (it == edge_ids_.end()) {
    return 0;
  }
  return edges_[it->second].count;
}

std::size_t DirectlyFollowsGraph::intern_activity(
    const std::string& activity) {
  auto [it, inserted] = activity_ids_.emplace(activity, activities_.size());
  if (inserted) {
    activities_.push_back(activity);
    activity_counts_.push_back(0);
    start_activity_counts_.push_back(0);
    end_activity_counts_.push_back(0);
    rework_case_counts_.push_back(0);
  }
  return it->second;
}

void DirectlyFollowsGraph::add_edge(std::size_t source, std::size_t target) {
  const auto key = edge_key(source, target);
  auto [it, inserted] = edge_ids_.emplace(key, edges_.size());
  if (inserted) {
    edges_.push_back(DirectlyFollowsEdge{source, target, 0});
  }
  ++edges_[it->second].count;
}

void DirectlyFollowsGraph::add_variant(std::vector<std::string> activities) {
  auto [it, inserted] = variant_ids_.emplace(variant_key(activities),
                                            variants_.size());
  if (inserted) {
    variants_.push_back(TraceVariant{std::move(activities), 0});
  }
  ++variants_[it->second].count;
}

DirectlyFollowsGraph discover_directly_follows_graph(const EventLog& event_log) {
  DirectlyFollowsGraph graph;

  for (const auto& trace : event_log.traces) {
    if (trace.events.empty()) {
      continue;
    }

    std::vector<std::size_t> activity_ids;
    std::vector<std::string> variant;
    activity_ids.reserve(trace.events.size());
    variant.reserve(trace.events.size());

    for (const auto& event : trace.events) {
      const auto activity_id = graph.intern_activity(event.activity);
      ++graph.activity_counts_[activity_id];
      activity_ids.push_back(activity_id);
      variant.push_back(event.activity);
    }

    ++graph.start_activity_counts_[activity_ids.front()];
    ++graph.end_activity_counts_[activity_ids.back()];
    graph.add_variant(std::move(variant));

    for (std::size_t i = 1; i < activity_ids.size(); ++i) {
      graph.add_edge(activity_ids[i - 1], activity_ids[i]);
    }

    // Rework: each activity that occurs more than once in this case counts
    // the case once. Sorting groups equal ids into runs to scan.
    std::sort(activity_ids.begin(), activity_ids.end());
    for (std::size_t i = 0; i < activity_ids.size();) {
      std::size_t j = i + 1;
      while (j < activity_ids.size() && activity_ids[j] == activity_ids[i]) {
        ++j;
      }
      if (j - i > 1) {
        ++graph.rework_case_counts_[activity_ids[i]];
      }
      i = j;
    }
  }

  return graph;
}

}  // namespace minera
