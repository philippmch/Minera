#include "minera/performance.hpp"

#include <algorithm>
#include <limits>
#include <optional>
#include <unordered_map>

#include "minera/timestamp.hpp"

namespace minera {
namespace {

std::uint64_t edge_key(std::size_t source, std::size_t target) {
  return (static_cast<std::uint64_t>(source) << 32U) |
         static_cast<std::uint64_t>(target);
}

// Reduces a list of durations to summary statistics. Sorts a local copy to take
// the median; an even count averages the two middle values.
DurationStats summarize(std::vector<double> durations) {
  DurationStats stats;
  if (durations.empty()) {
    return stats;
  }
  std::sort(durations.begin(), durations.end());
  stats.count = durations.size();
  stats.min = durations.front();
  stats.max = durations.back();
  double total = 0.0;
  for (const double value : durations) {
    total += value;
  }
  stats.total = total;
  stats.mean = total / static_cast<double>(durations.size());
  const std::size_t mid = durations.size() / 2;
  stats.median = durations.size() % 2 == 0
                     ? (durations[mid - 1] + durations[mid]) / 2.0
                     : durations[mid];
  return stats;
}

}  // namespace

PerformanceMetrics compute_performance_metrics(
    const EventLog& log,
    const DirectlyFollowsGraph& graph) {
  std::unordered_map<std::uint64_t, std::vector<double>> edge_durations;
  std::vector<double> case_durations;

  for (const auto& trace : log.traces) {
    std::optional<double> previous;
    std::size_t previous_activity = 0;
    std::optional<double> first;
    std::optional<double> last;

    for (const auto& event : trace.events) {
      const auto instant = parse_timestamp(event.timestamp);
      const auto activity = graph.activity_id(event.activity);

      if (instant && activity) {
        if (!first) {
          first = *instant;
        }
        last = *instant;

        if (previous) {
          const double duration = *instant - *previous;
          if (duration >= 0.0) {
            edge_durations[edge_key(previous_activity, *activity)].push_back(
                duration);
          }
        }
        previous = *instant;
        previous_activity = *activity;
      }
    }

    if (first && last) {
      case_durations.push_back(*last - *first);
    }
  }

  PerformanceMetrics metrics;
  metrics.timed_traces = case_durations.size();
  metrics.case_duration = summarize(case_durations);

  // Emit edge timings in the graph's edge order so the two line up by index.
  metrics.edges.reserve(graph.edges().size());
  for (const auto& edge : graph.edges()) {
    EdgePerformance performance;
    performance.source = edge.source;
    performance.target = edge.target;
    const auto it = edge_durations.find(edge_key(edge.source, edge.target));
    if (it != edge_durations.end()) {
      performance.stats = summarize(std::move(it->second));
    }
    metrics.edges.push_back(std::move(performance));
  }

  return metrics;
}

}  // namespace minera
