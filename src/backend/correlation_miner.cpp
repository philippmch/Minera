#include "minera/correlation_miner.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <queue>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "minera/timestamp.hpp"

namespace minera {
namespace {

constexpr double kMissingCost = 100000000000.0;
constexpr double kInvalidMatchingCost = 1.0e18;
constexpr double kComparisonTolerance = 1.0e-12;

struct PreparedEvent {
  std::string activity;
  double start = 0.0;
  double complete = 0.0;
  std::size_t input_index = 0;
};

using PreparedEvents = std::vector<PreparedEvent>;
using GroupedEvents = std::map<std::string, PreparedEvents>;

struct PairStatistics {
  double precede_succeed_probability = 0.0;
  double duration = 0.0;
};

bool event_less(const PreparedEvent& left, const PreparedEvent& right) {
  if (left.start != right.start) return left.start < right.start;
  if (left.complete != right.complete) {
    return left.complete < right.complete;
  }
  return left.input_index < right.input_index;
}

double mean_or_zero(const std::vector<double>& values) {
  if (values.empty()) return 0.0;
  double total = 0.0;
  for (const double value : values) total += value;
  return total / static_cast<double>(values.size());
}

std::vector<double> fifo_delays(const PreparedEvents& source,
                                const PreparedEvents& target) {
  std::vector<double> delays;
  std::size_t source_index = 0;
  std::size_t target_index = 0;
  while (source_index < source.size()) {
    while (target_index < target.size()) {
      if (source[source_index].complete < target[target_index].start) {
        delays.push_back(target[target_index].start -
                         source[source_index].complete);
        ++target_index;
        break;
      }
      ++target_index;
    }
    ++source_index;
  }
  return delays;
}

std::vector<double> reverse_lifo_delays(const PreparedEvents& source,
                                        const PreparedEvents& target) {
  std::vector<double> delays;
  std::ptrdiff_t source_index =
      static_cast<std::ptrdiff_t>(source.size()) - 1;
  std::ptrdiff_t target_index =
      static_cast<std::ptrdiff_t>(target.size()) - 1;
  while (target_index >= 0) {
    while (source_index >= 0) {
      const auto& source_event =
          source[static_cast<std::size_t>(source_index)];
      const auto& target_event =
          target[static_cast<std::size_t>(target_index)];
      if (source_event.complete < target_event.start) {
        delays.push_back(target_event.start - source_event.complete);
        --source_index;
        break;
      }
      --source_index;
    }
    --target_index;
  }
  return delays;
}

double greedy_duration(const PreparedEvents& source,
                       const PreparedEvents& target) {
  return std::min(mean_or_zero(fifo_delays(source, target)),
                  mean_or_zero(reverse_lifo_delays(source, target)));
}

// Minimum-weight full matching for a rectangular bipartite graph. The smaller
// partition is covered. Invalid (complete > start) pairs receive a finite high
// cost so the full assignment remains feasible, then are discarded exactly as
// in the reference behavior before the mean is calculated.
double exact_duration(const PreparedEvents& source,
                      const PreparedEvents& target) {
  if (source.empty() || target.empty()) return 0.0;

  const bool source_is_rows = source.size() <= target.size();
  const std::size_t row_count =
      source_is_rows ? source.size() : target.size();
  const std::size_t column_count =
      source_is_rows ? target.size() : source.size();

  const auto directed_delay = [&](std::size_t row,
                                  std::size_t column) {
    const auto source_index = source_is_rows ? row : column;
    const auto target_index = source_is_rows ? column : row;
    return target[target_index].start - source[source_index].complete;
  };
  const auto assignment_cost = [&](std::size_t row,
                                   std::size_t column) {
    const double delay = directed_delay(row, column);
    return delay >= 0.0 ? delay : kInvalidMatchingCost;
  };

  // Rectangular Hungarian algorithm, one-based internally.
  std::vector<double> row_potential(row_count + 1, 0.0);
  std::vector<double> column_potential(column_count + 1, 0.0);
  std::vector<std::size_t> column_match(column_count + 1, 0);
  std::vector<std::size_t> previous_column(column_count + 1, 0);
  for (std::size_t row = 1; row <= row_count; ++row) {
    column_match[0] = row;
    std::size_t current_column = 0;
    std::vector<double> minimum(column_count + 1,
                                kInvalidMatchingCost);
    std::vector<bool> used(column_count + 1, false);
    do {
      used[current_column] = true;
      const std::size_t current_row = column_match[current_column];
      double delta = kInvalidMatchingCost;
      std::size_t next_column = 0;
      for (std::size_t column = 1; column <= column_count; ++column) {
        if (used[column]) continue;
        const double reduced =
            assignment_cost(current_row - 1, column - 1) -
            row_potential[current_row] - column_potential[column];
        if (reduced < minimum[column] - kComparisonTolerance) {
          minimum[column] = reduced;
          previous_column[column] = current_column;
        }
        if (minimum[column] < delta - kComparisonTolerance) {
          delta = minimum[column];
          next_column = column;
        }
      }
      for (std::size_t column = 0; column <= column_count; ++column) {
        if (used[column]) {
          row_potential[column_match[column]] += delta;
          column_potential[column] -= delta;
        } else {
          minimum[column] -= delta;
        }
      }
      current_column = next_column;
    } while (column_match[current_column] != 0);

    do {
      const std::size_t next_column =
          previous_column[current_column];
      column_match[current_column] = column_match[next_column];
      current_column = next_column;
    } while (current_column != 0);
  }

  std::vector<double> delays;
  delays.reserve(row_count);
  for (std::size_t column = 1; column <= column_count; ++column) {
    if (column_match[column] == 0) continue;
    const double delay =
        directed_delay(column_match[column] - 1, column - 1);
    if (delay >= 0.0) delays.push_back(delay);
  }
  return mean_or_zero(delays);
}

double precede_succeed_probability(const PreparedEvents& source,
                                   const PreparedEvents& target) {
  if (source.empty() || target.empty()) return 0.0;
  std::uint64_t valid = 0;
  for (const auto& source_event : source) {
    for (const auto& target_event : target) {
      if (source_event.complete < target_event.start) ++valid;
    }
  }
  return static_cast<double>(valid) /
         static_cast<double>(source.size() * target.size());
}

PairStatistics pair_statistics(const PreparedEvents& source,
                               const PreparedEvents& target,
                               bool exact_time_matching) {
  return {precede_succeed_probability(source, target),
          exact_time_matching ? exact_duration(source, target)
                              : greedy_duration(source, target)};
}

GroupedEvents group_events(const PreparedEvents& events) {
  GroupedEvents grouped;
  for (const auto& event : events) {
    grouped[event.activity].push_back(event);
  }
  return grouped;
}

PairStatistics trace_pair_statistics(
    const std::vector<PreparedEvents>& traces,
    const std::string& source_activity,
    const std::string& target_activity) {
  std::uint64_t valid = 0;
  std::uint64_t possible = 0;
  std::vector<double> fifo;
  std::vector<double> reverse_lifo;
  for (const auto& trace : traces) {
    PreparedEvents source;
    PreparedEvents target;
    for (const auto& event : trace) {
      if (event.activity == source_activity) source.push_back(event);
      if (event.activity == target_activity) target.push_back(event);
    }
    if (source.empty() || target.empty()) continue;
    possible += static_cast<std::uint64_t>(source.size() * target.size());
    for (const auto& source_event : source) {
      for (const auto& target_event : target) {
        if (source_event.complete < target_event.start) ++valid;
      }
    }
    const auto trace_fifo = fifo_delays(source, target);
    fifo.insert(fifo.end(), trace_fifo.begin(), trace_fifo.end());
    const auto trace_reverse = reverse_lifo_delays(source, target);
    reverse_lifo.insert(reverse_lifo.end(),
                        trace_reverse.begin(), trace_reverse.end());
  }
  return {
      possible == 0
          ? 0.0
          : static_cast<double>(valid) / static_cast<double>(possible),
      std::min(mean_or_zero(fifo), mean_or_zero(reverse_lifo))};
}

struct FlowEdge {
  int target = 0;
  int reverse = 0;
  std::int64_t capacity = 0;
  double cost = 0.0;
  std::int64_t flow = 0;
};

void add_flow_edge(std::vector<std::vector<FlowEdge>>& graph,
                   int source, int target, std::int64_t capacity,
                   double cost) {
  const auto forward_reverse =
      static_cast<int>(graph[target].size());
  const auto reverse_reverse =
      static_cast<int>(graph[source].size());
  graph[source].push_back(
      {target, forward_reverse, capacity, cost, 0});
  graph[target].push_back(
      {source, reverse_reverse, 0, -cost, 0});
}

std::vector<std::vector<std::int64_t>> solve_transportation(
    const std::vector<std::int64_t>& supply,
    const std::vector<std::int64_t>& demand,
    const std::vector<std::vector<double>>& costs) {
  const int activity_count = static_cast<int>(supply.size());
  const int source_node = 2 * activity_count;
  const int sink_node = source_node + 1;
  std::vector<std::vector<FlowEdge>> graph(
      static_cast<std::size_t>(sink_node + 1));
  for (int source = 0; source < activity_count; ++source) {
    add_flow_edge(graph, source_node, source, supply[source], 0.0);
    add_flow_edge(graph, activity_count + source, sink_node,
                  demand[source], 0.0);
    for (int target = 0; target < activity_count; ++target) {
      add_flow_edge(
          graph, source, activity_count + target,
          std::numeric_limits<std::int64_t>::max() / 4,
          costs[static_cast<std::size_t>(source)]
               [static_cast<std::size_t>(target)]);
    }
  }

  std::int64_t remaining = 0;
  for (const auto count : supply) remaining += count;
  std::vector<double> potential(graph.size(), 0.0);
  while (remaining > 0) {
    const double infinity = std::numeric_limits<double>::infinity();
    std::vector<double> distance(graph.size(), infinity);
    std::vector<int> previous_node(graph.size(), -1);
    std::vector<int> previous_edge(graph.size(), -1);
    using QueueEntry = std::pair<double, int>;
    std::priority_queue<QueueEntry, std::vector<QueueEntry>,
                        std::greater<QueueEntry>>
        queue;
    distance[source_node] = 0.0;
    queue.push({0.0, source_node});
    while (!queue.empty()) {
      const auto [current_distance, node] = queue.top();
      queue.pop();
      if (current_distance >
          distance[static_cast<std::size_t>(node)] +
              kComparisonTolerance) {
        continue;
      }
      for (std::size_t edge_index = 0;
           edge_index < graph[static_cast<std::size_t>(node)].size();
           ++edge_index) {
        const auto& edge =
            graph[static_cast<std::size_t>(node)][edge_index];
        if (edge.capacity <= 0) continue;
        double reduced =
            edge.cost + potential[static_cast<std::size_t>(node)] -
            potential[static_cast<std::size_t>(edge.target)];
        if (reduced < 0.0 && reduced > -kComparisonTolerance) {
          reduced = 0.0;
        }
        const double candidate = current_distance + reduced;
        if (candidate <
            distance[static_cast<std::size_t>(edge.target)] -
                kComparisonTolerance) {
          distance[static_cast<std::size_t>(edge.target)] = candidate;
          previous_node[static_cast<std::size_t>(edge.target)] = node;
          previous_edge[static_cast<std::size_t>(edge.target)] =
              static_cast<int>(edge_index);
          queue.push({candidate, edge.target});
        }
      }
    }
    if (previous_node[static_cast<std::size_t>(sink_node)] < 0) {
      throw std::runtime_error(
          "correlation transportation problem is infeasible");
    }
    for (std::size_t node = 0; node < graph.size(); ++node) {
      if (std::isfinite(distance[node])) potential[node] += distance[node];
    }
    std::int64_t augment = remaining;
    for (int node = sink_node; node != source_node;
         node = previous_node[static_cast<std::size_t>(node)]) {
      const auto& edge =
          graph[static_cast<std::size_t>(
                    previous_node[static_cast<std::size_t>(node)])]
               [static_cast<std::size_t>(
                    previous_edge[static_cast<std::size_t>(node)])];
      augment = std::min(augment, edge.capacity);
    }
    for (int node = sink_node; node != source_node;
         node = previous_node[static_cast<std::size_t>(node)]) {
      const int parent =
          previous_node[static_cast<std::size_t>(node)];
      auto& edge =
          graph[static_cast<std::size_t>(parent)]
               [static_cast<std::size_t>(
                    previous_edge[static_cast<std::size_t>(node)])];
      auto& reverse =
          graph[static_cast<std::size_t>(node)]
               [static_cast<std::size_t>(edge.reverse)];
      edge.capacity -= augment;
      reverse.capacity += augment;
      edge.flow += augment;
      reverse.flow -= augment;
    }
    remaining -= augment;
  }

  std::vector<std::vector<std::int64_t>> flow(
      supply.size(), std::vector<std::int64_t>(demand.size(), 0));
  for (int source = 0; source < activity_count; ++source) {
    for (const auto& edge : graph[static_cast<std::size_t>(source)]) {
      if (edge.target >= activity_count &&
          edge.target < 2 * activity_count && edge.flow > 0) {
        flow[static_cast<std::size_t>(source)]
            [static_cast<std::size_t>(edge.target - activity_count)] +=
            edge.flow;
      }
    }
  }
  return flow;
}

}  // namespace

CorrelationMinerResult discover_correlation_miner(
    const EventLog& log, const CorrelationMinerOptions& options) {
  if (options.variant == CorrelationVariant::kClassicSplit &&
      options.sample_size == 0) {
    throw std::invalid_argument(
        "correlation miner sample_size must be positive");
  }

  CorrelationMinerResult result;
  result.variant = options.variant;
  PreparedEvents raw_flat;
  raw_flat.reserve(log.event_count());
  std::vector<PreparedEvents> prepared_traces;
  prepared_traces.reserve(log.traces.size());
  std::size_t input_index = 0;
  for (const auto& trace : log.traces) {
    PreparedEvents prepared_trace;
    prepared_trace.reserve(trace.events.size());
    for (const auto& event : trace.events) {
      const auto complete = parse_timestamp(event.timestamp);
      if (!complete) {
        throw std::invalid_argument(
            "correlation miner: invalid completion timestamp");
      }
      double start = *complete;
      if (!options.start_timestamp_attribute.empty()) {
        const auto* start_text =
            event.attribute(options.start_timestamp_attribute);
        if (start_text == nullptr) {
          throw std::invalid_argument(
              "correlation miner: missing start timestamp attribute '" +
              options.start_timestamp_attribute + "'");
        }
        const auto parsed_start = parse_timestamp(*start_text);
        if (!parsed_start) {
          throw std::invalid_argument(
              "correlation miner: invalid start timestamp attribute '" +
              options.start_timestamp_attribute + "'");
        }
        start = *parsed_start;
      }
      prepared_trace.push_back(
          {event.activity, start, *complete, input_index++});
    }
    if (options.variant == CorrelationVariant::kTraceBased) {
      std::stable_sort(prepared_trace.begin(), prepared_trace.end(),
                       event_less);
    }
    raw_flat.insert(raw_flat.end(), prepared_trace.begin(),
                    prepared_trace.end());
    prepared_traces.push_back(std::move(prepared_trace));
  }
  if (raw_flat.empty()) return result;

  PreparedEvents globally_sorted = raw_flat;
  if (options.variant != CorrelationVariant::kTraceBased) {
    std::stable_sort(globally_sorted.begin(), globally_sorted.end(),
                     event_less);
  }
  const auto global_groups = group_events(globally_sorted);
  std::vector<std::string> activities;
  activities.reserve(global_groups.size());
  for (const auto& [activity, events] : global_groups) {
    (void)events;
    activities.push_back(activity);
  }

  const std::size_t activity_count = activities.size();
  std::vector<std::int64_t> occurrences(activity_count, 0);
  for (std::size_t index = 0; index < activity_count; ++index) {
    occurrences[index] =
        static_cast<std::int64_t>(
            global_groups.at(activities[index]).size());
  }
  std::vector<std::vector<double>> probability(
      activity_count,
      std::vector<double>(activity_count, 0.0));
  std::vector<std::vector<double>> duration(
      activity_count,
      std::vector<double>(activity_count, 0.0));

  if (options.variant == CorrelationVariant::kTraceBased) {
    for (std::size_t source = 0; source < activity_count; ++source) {
      for (std::size_t target = 0; target < activity_count; ++target) {
        if (source == target) continue;
        const auto statistics = trace_pair_statistics(
            prepared_traces, activities[source], activities[target]);
        probability[source][target] =
            statistics.precede_succeed_probability;
        duration[source][target] = statistics.duration;
      }
    }
  } else if (options.variant == CorrelationVariant::kClassicSplit) {
    const std::size_t chunk_count =
        (raw_flat.size() + options.sample_size - 1) /
        options.sample_size;
    for (std::size_t source = 0; source < activity_count; ++source) {
      for (std::size_t target = 0; target < activity_count; ++target) {
        if (source == target) continue;
        double probability_total = 0.0;
        double maximum_duration = 0.0;
        for (std::size_t chunk = 0; chunk < chunk_count; ++chunk) {
          const auto begin = raw_flat.begin() +
                             static_cast<std::ptrdiff_t>(
                                 chunk * options.sample_size);
          const auto end = raw_flat.begin() +
                           static_cast<std::ptrdiff_t>(std::min(
                               raw_flat.size(),
                               (chunk + 1) * options.sample_size));
          PreparedEvents events(begin, end);
          std::stable_sort(events.begin(), events.end(), event_less);
          const auto grouped = group_events(events);
          const auto source_group = grouped.find(activities[source]);
          const auto target_group = grouped.find(activities[target]);
          if (source_group == grouped.end() ||
              target_group == grouped.end()) {
            continue;
          }
          const auto statistics = pair_statistics(
              source_group->second, target_group->second,
              options.exact_time_matching);
          probability_total +=
              statistics.precede_succeed_probability;
          maximum_duration =
              std::max(maximum_duration, statistics.duration);
        }
        probability[source][target] =
            probability_total / static_cast<double>(chunk_count);
        duration[source][target] = maximum_duration;
      }
    }
  } else {
    for (std::size_t source = 0; source < activity_count; ++source) {
      for (std::size_t target = 0; target < activity_count; ++target) {
        if (source == target) continue;
        const auto statistics = pair_statistics(
            global_groups.at(activities[source]),
            global_groups.at(activities[target]),
            options.exact_time_matching);
        probability[source][target] =
            statistics.precede_succeed_probability;
        duration[source][target] = statistics.duration;
      }
    }
  }

  std::vector<std::vector<double>> costs(
      activity_count,
      std::vector<double>(activity_count, kMissingCost));
  for (std::size_t source = 0; source < activity_count; ++source) {
    for (std::size_t target = 0; target < activity_count; ++target) {
      if (source == target || probability[source][target] <= 0.0) {
        continue;
      }
      const double value =
          duration[source][target] / probability[source][target] /
          static_cast<double>(
              std::min(occurrences[source], occurrences[target]));
      if (value != 0.0) costs[source][target] = value;
    }
  }

  const auto flow =
      solve_transportation(occurrences, occurrences, costs);
  for (std::size_t source = 0; source < activity_count; ++source) {
    for (std::size_t target = 0; target < activity_count; ++target) {
      const DfgActivityPair pair{
          activities[source], activities[target]};
      result.precede_succeed_probability[pair] =
          probability[source][target];
      result.duration_matrix[pair] = duration[source][target];
      if (flow[source][target] <= 0) continue;
      result.frequency_dfg[pair] =
          static_cast<std::uint64_t>(flow[source][target]);
      result.performance_dfg[pair] = duration[source][target];
    }
  }
  return result;
}

}  // namespace minera
