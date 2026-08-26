#include "minera/performance.hpp"

#include <algorithm>
#include <cctype>
#include <deque>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "minera/timestamp.hpp"

namespace minera {
namespace {

// Case-insensitive lifecycle comparison: the XES standard spells the
// transitions lowercase, but real logs (e.g. BPI 2012) ship them uppercase.
bool lifecycle_is(std::string_view value, std::string_view expected) {
  if (value.size() != expected.size()) return false;
  for (std::size_t i = 0; i < value.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(value[i])) != expected[i]) {
      return false;
    }
  }
  return true;
}

std::uint64_t edge_key(std::size_t source, std::size_t target) {
  return (static_cast<std::uint64_t>(source) << 32U) |
         static_cast<std::uint64_t>(target);
}

}  // namespace

// Sorts a local copy to take the median; an even count averages the two middle
// values.
DurationStats summarize_durations(std::vector<double> durations) {
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

PerformanceMetrics compute_performance_metrics(
    const EventLog& log,
    const DirectlyFollowsGraph& graph) {
  std::unordered_map<std::uint64_t, std::vector<double>> edge_durations;
  std::map<std::size_t, std::vector<double>> service_durations;
  std::vector<double> case_durations;
  std::vector<double> case_starts;

  for (const auto& trace : log.traces) {
    std::optional<double> previous;
    std::size_t previous_activity = 0;
    std::optional<double> first;
    std::optional<double> last;
    // Unmatched lifecycle starts per activity, paired first-in-first-out with
    // the activity's next complete (events are time-ordered within a trace).
    std::unordered_map<std::size_t, std::deque<double>> pending_starts;

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

        if (lifecycle_is(event.lifecycle, "start")) {
          pending_starts[*activity].push_back(*instant);
        } else if (lifecycle_is(event.lifecycle, "complete")) {
          // A complete without a pending start is an atomic observation and
          // contributes no service time, like an untimed event elsewhere.
          auto it = pending_starts.find(*activity);
          if (it != pending_starts.end() && !it->second.empty()) {
            const double duration = *instant - it->second.front();
            it->second.pop_front();
            if (duration >= 0.0) {
              service_durations[*activity].push_back(duration);
            }
          }
        }
      }
    }

    if (first && last) {
      case_durations.push_back(*last - *first);
    }
    if (first) {
      case_starts.push_back(*first);
    }
  }

  PerformanceMetrics metrics;
  metrics.timed_traces = case_durations.size();
  metrics.case_duration = summarize_durations(case_durations);

  // Case arrival rate: order the cases by start instant and summarize the
  // gaps between consecutive arrivals.
  std::sort(case_starts.begin(), case_starts.end());
  std::vector<double> arrival_gaps;
  if (case_starts.size() > 1) {
    arrival_gaps.reserve(case_starts.size() - 1);
    for (std::size_t i = 1; i < case_starts.size(); ++i) {
      arrival_gaps.push_back(case_starts[i] - case_starts[i - 1]);
    }
  }
  metrics.case_arrival = summarize_durations(std::move(arrival_gaps));

  // Emit edge timings in the graph's edge order so the two line up by index.
  metrics.edges.reserve(graph.edges().size());
  for (const auto& edge : graph.edges()) {
    EdgePerformance performance;
    performance.source = edge.source;
    performance.target = edge.target;
    const auto it = edge_durations.find(edge_key(edge.source, edge.target));
    if (it != edge_durations.end()) {
      performance.stats = summarize_durations(std::move(it->second));
    }
    metrics.edges.push_back(std::move(performance));
  }

  // Activity service times, sparse and in activity-id order (the map is
  // ordered): only activities with at least one paired start/complete appear.
  for (auto& [activity, durations] : service_durations) {
    ActivityPerformance performance;
    performance.activity = activity;
    performance.stats = summarize_durations(std::move(durations));
    metrics.activity_service.push_back(std::move(performance));
  }

  return metrics;
}

std::vector<DurationStats> compute_variant_durations(
    const EventLog& log,
    const DirectlyFollowsGraph& graph) {
  // Group case throughput times by the trace's activity sequence — the same
  // grouping the directly-follows graph uses for its variants.
  std::map<std::vector<std::string>, std::vector<double>> durations_by_variant;

  for (const auto& trace : log.traces) {
    if (trace.events.empty()) {
      continue;
    }

    std::vector<std::string> variant;
    variant.reserve(trace.events.size());
    std::optional<double> first;
    std::optional<double> last;

    for (const auto& event : trace.events) {
      variant.push_back(event.activity);
      const auto instant = parse_timestamp(event.timestamp);
      if (instant) {
        if (!first) {
          first = *instant;
        }
        last = *instant;
      }
    }

    auto& bucket = durations_by_variant[std::move(variant)];
    if (first && last) {
      bucket.push_back(*last - *first);
    }
  }

  // Emit aligned to the graph's variant order so a caller can zip the two.
  std::vector<DurationStats> result;
  result.reserve(graph.variants().size());
  for (const auto& variant : graph.variants()) {
    const auto it = durations_by_variant.find(variant.activities);
    result.push_back(it != durations_by_variant.end() ? summarize_durations(it->second)
                                                       : DurationStats{});
  }
  return result;
}

}  // namespace minera
