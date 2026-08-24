#include "minera/dfg_playout.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <queue>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "minera/timestamp.hpp"

namespace minera {
namespace {

struct PartialTrace {
  double cost = 0.0;
  std::vector<std::string> activities;
};

struct PartialTraceGreater {
  bool operator()(const PartialTrace& lhs, const PartialTrace& rhs) const {
    if (lhs.cost != rhs.cost) return lhs.cost > rhs.cost;
    return lhs.activities > rhs.activities;
  }
};

double start_epoch(const DfgPlayoutOptions& options) {
  const auto parsed = parse_timestamp(options.start_time);
  if (!parsed || !std::isfinite(*parsed)) {
    throw std::invalid_argument("start_time must be a valid ISO-8601 timestamp");
  }
  return *parsed;
}

double performance_mean(const DfgPerformance* performance,
                        const DfgActivityPair& edge) {
  if (performance == nullptr) return 0.0;
  const auto it = performance->find(edge);
  if (it == performance->end()) return 0.0;
  const auto& stats = it->second;
  double result = stats.summary.mean;
  if (const auto* scalar = std::get_if<double>(&stats.value)) {
    result = *scalar;
  } else if (const auto* values = std::get_if<std::vector<double>>(&stats.value)) {
    if (!values->empty()) {
      result = 0.0;
      for (const double value : *values) result += value;
      result /= static_cast<double>(values->size());
    }
  }
  if (!std::isfinite(result) || result < 0.0) {
    throw std::invalid_argument(
        "performance DFG means must be finite and non-negative");
  }
  return result > 0.0 ? result : 0.0;
}

void validate_options(const DfgPlayoutOptions& options,
                      const DfgPerformance* performance) {
  if (!std::isfinite(options.min_weighted_probability) ||
      options.min_weighted_probability < 0.0 ||
      options.min_weighted_probability > 1.0) {
    throw std::invalid_argument(
        "min_weighted_probability must be between 0 and 1");
  }
  if (!std::isfinite(options.case_arrival_rate) ||
      options.case_arrival_rate < 0.0) {
    throw std::invalid_argument(
        "case_arrival_rate must be finite and non-negative");
  }
  if (!parse_timestamp(options.start_time).has_value()) {
    throw std::invalid_argument("start_time must be a valid ISO-8601 timestamp");
  }
  if (performance == nullptr) return;
  for (const auto& [edge, statistics] : *performance) {
    (void)edge;
    if (!std::isfinite(statistics.summary.mean) ||
        statistics.summary.mean < 0.0) {
      throw std::invalid_argument(
          "performance DFG means must be finite and non-negative");
    }
    for (const double value : statistics.raw_values) {
      if (!std::isfinite(value) || value < 0.0) {
        throw std::invalid_argument(
            "performance DFG observations must be finite and non-negative");
      }
    }
    if (const auto* scalar = std::get_if<double>(&statistics.value)) {
      if (!std::isfinite(*scalar) || *scalar < 0.0) {
        throw std::invalid_argument(
            "performance DFG values must be finite and non-negative");
      }
    } else if (const auto* values =
                   std::get_if<std::vector<double>>(&statistics.value)) {
      for (const double value : *values) {
        if (!std::isfinite(value) || value < 0.0) {
          throw std::invalid_argument(
              "performance DFG values must be finite and non-negative");
        }
      }
    }
  }
}

Event make_event(const std::string& case_id, const std::string& activity,
                 double instant, std::size_t order) {
  Event event;
  event.case_id = case_id;
  event.activity = activity;
  event.timestamp = format_timestamp(instant);
  event.input_order = order;
  return event;
}

std::uint64_t variant_occurrence_count(double probability,
                                       std::size_t max_variants) {
  if (!std::isfinite(probability) || probability <= 0.0 || max_variants == 0)
    return 0;
  const long double scaled =
      std::ceil(static_cast<long double>(probability) *
                static_cast<long double>(max_variants));
  if (scaled >= static_cast<long double>(
                    std::numeric_limits<std::uint64_t>::max()))
    return std::numeric_limits<std::uint64_t>::max();
  return static_cast<std::uint64_t>(scaled);
}

DfgPlayoutResult classic_playout(const DfgFrequency& frequency,
                                  const DfgActivityCounts& starts,
                                  const DfgActivityCounts& ends,
                                  const DfgPlayoutOptions& options) {
  DfgPlayoutResult result;
  result.variant = DfgPlayoutVariant::kClassic;

  // Log probabilities turn products into additive costs and make a best-first
  // queue numerically stable. std::map iteration gives lexical tie ordering.
  std::map<std::string, std::map<std::string, double>> transitions;
  for (const auto& [edge, count] : frequency) {
    if (count != 0) transitions[edge.first][edge.second] += count;
  }
  for (const auto& [activity, count] : ends) {
    if (count != 0) transitions[activity][std::string{}] += count;
  }

  std::priority_queue<PartialTrace, std::vector<PartialTrace>,
                      PartialTraceGreater>
      queue;
  double start_total = 0.0;
  for (const auto& [activity, count] : starts) start_total += count;
  std::set<std::string> seen_starts;
  std::set<std::string> seen_ends;
  std::set<DfgActivityPair> seen_edges;
  double cumulative_probability = 0.0;
  std::size_t max_occ = 0;
  const std::size_t expansion_budget =
      options.max_variants > (std::numeric_limits<std::size_t>::max() / 128)
          ? std::numeric_limits<std::size_t>::max()
          : std::max<std::size_t>(10000, options.max_variants * 128);
  std::size_t expansions = 0;
  const auto started = std::chrono::steady_clock::now();
  const auto timed_out = [&] {
    if (options.max_execution_time_ms == 0) return false;
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - started)
               .count() >= static_cast<long long>(options.max_execution_time_ms);
  };
  const auto coverage_complete = [&] {
    for (const auto& [edge, count] : frequency) {
      if (count == 0) continue;
      if (!seen_edges.contains(edge)) return false;
    }
    for (const auto& [activity, count] : starts) {
      (void)count;
      if (count != 0 && !seen_starts.contains(activity)) return false;
    }
    for (const auto& [activity, count] : ends) {
      (void)count;
      if (count != 0 && !seen_ends.contains(activity)) return false;
    }
    return true;
  };

  result.complete = coverage_complete();
  if (start_total <= 0.0 || options.max_variants == 0) {
    result.interrupted = start_total > 0.0 && options.max_variants == 0;
    return result;
  }
  for (const auto& [activity, count] : starts) {
    if (count == 0) continue;
    queue.push(PartialTrace{-std::log(static_cast<double>(count) / start_total),
                            {activity}});
  }

  while (!queue.empty()) {
    if (++expansions > expansion_budget) {
      result.interrupted = true;
      break;
    }
    if (timed_out()) {
      result.interrupted = true;
      break;
    }
    if (result.log.traces.size() >= options.max_variants) {
      result.interrupted = true;
      break;
    }
    auto state = queue.top();
    queue.pop();
    if (state.activities.empty()) continue;
    const std::string& last = state.activities.back();
    const auto out_it = transitions.find(last);
    if (out_it == transitions.end()) continue;

    std::unordered_map<std::string, std::size_t> occurrences;
    for (const auto& activity : state.activities) ++occurrences[activity];
    for (const auto& [next, weight] : out_it->second) {
      if (weight <= 0.0) continue;
      if (next.empty()) {
        const double probability = std::exp(-state.cost) *
                                   (weight / [&] {
                                     double total = 0.0;
                                     for (const auto& item : out_it->second)
                                       total += item.second;
                                     return total;
                                   }());
        if (!std::isfinite(probability) || probability <= 0.0) continue;
        const std::uint64_t variant_occurrences =
            variant_occurrence_count(probability, options.max_variants);
        max_occ = std::max<std::size_t>(
            max_occ, static_cast<std::size_t>(std::min<std::uint64_t>(
                         variant_occurrences,
                         std::numeric_limits<std::size_t>::max())));
        if (variant_occurrences < options.min_variant_occurrences &&
            options.min_variant_occurrences <= max_occ)
          break;
        if (cumulative_probability > options.min_weighted_probability) {
          result.interrupted = true;
          break;
        }

        bool adds_new = !seen_starts.contains(state.activities.front()) ||
                        !seen_ends.contains(last);
        for (std::size_t i = 1; i < state.activities.size(); ++i) {
          if (!seen_edges.contains(
                  {state.activities[i - 1], state.activities[i]})) {
            adds_new = true;
          }
        }
        if (options.add_trace_if_takes_new_edges && !adds_new) continue;

        seen_starts.insert(state.activities.front());
        seen_ends.insert(last);
        for (std::size_t i = 1; i < state.activities.size(); ++i)
          seen_edges.insert({state.activities[i - 1], state.activities[i]});

        Trace trace;
        trace.case_id =
            "case_" + std::to_string(result.log.traces.size() + 1);
        trace.attributes.push_back(
            {"probability", std::to_string(probability), AttributeKind::kFloat});
        trace.attributes.push_back(
            {"variant_occurrences", std::to_string(variant_occurrences),
             AttributeKind::kInt});
        result.counts[state.activities] = variant_occurrences;
        if (options.return_variants) {
          std::string variant;
          for (const auto& activity : state.activities) {
            if (!variant.empty()) variant.push_back(',');
            variant += activity;
          }
          trace.attributes.push_back(
              {"variant", std::move(variant), AttributeKind::kString});
        }
        double instant = start_epoch(options) +
                          static_cast<double>(result.log.traces.size());
        for (const auto& activity : state.activities) {
          trace.events.push_back(make_event(trace.case_id, activity, instant,
                                            trace.events.size()));
          instant += 1.0;
        }
        result.log.traces.push_back(std::move(trace));
        cumulative_probability += probability;
        result.complete = coverage_complete();

        if (options.interrupt_when_dfg_complete) {
          if (result.complete) break;
        }
      } else {
        if (occurrences[next] >= options.max_occurrences_per_activity) continue;
        auto next_trace = state.activities;
        next_trace.push_back(next);
        double total = 0.0;
        for (const auto& item : out_it->second) total += item.second;
        queue.push(PartialTrace{state.cost - std::log(weight / total),
                                std::move(next_trace)});
      }
    }
    if (options.interrupt_when_dfg_complete && result.complete) break;
  }
  if (options.return_only_if_complete &&
      (result.interrupted || !result.complete)) {
    result.log = {};
    result.counts.clear();
  }
  return result;
}

EventLog performance_playout(const DfgFrequency& frequency,
                             const DfgActivityCounts& starts,
                             const DfgActivityCounts& ends,
                             const DfgPlayoutOptions& options,
                             const DfgPerformance* performance) {
  EventLog log;
  if (options.traces == 0 || options.max_events_per_trace == 0) return log;
  std::mt19937_64 rng(options.seed);
  std::vector<std::string> start_names;
  std::vector<double> start_weights;
  for (const auto& [activity, count] : starts) {
    if (count == 0) continue;
    start_names.push_back(activity);
    start_weights.push_back(static_cast<double>(count));
  }
  if (start_names.empty()) return log;
  std::map<std::string, std::vector<std::pair<std::string, std::uint64_t>>> out;
  for (const auto& [edge, count] : frequency) {
    if (count != 0) out[edge.first].push_back({edge.second, count});
  }
  for (auto& [source, choices] : out) {
    (void)source;
    std::sort(choices.begin(), choices.end(),
              [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });
  }
  const double base = start_epoch(options);

  for (std::size_t trace_index = 0; trace_index < options.traces; ++trace_index) {
    Trace trace;
    trace.case_id = "case_" + std::to_string(trace_index + 1);
    double instant = base + static_cast<double>(trace_index) * options.case_arrival_rate;
    std::discrete_distribution<std::size_t> pick_start(start_weights.begin(),
                                                         start_weights.end());
    std::string current = start_names[pick_start(rng)];
    for (std::size_t step = 0; step < options.max_events_per_trace; ++step) {
      trace.events.push_back(make_event(trace.case_id, current, instant,
                                        trace.events.size()));
      const auto it = out.find(current);
      std::vector<std::string> names;
      std::vector<double> weights;
      if (it != out.end()) {
        for (const auto& [next, count] : it->second) {
          names.push_back(next);
          weights.push_back(static_cast<double>(count));
        }
      }
      const auto end_it = ends.find(current);
      if (end_it != ends.end() && end_it->second != 0) {
        names.push_back({});
        weights.push_back(static_cast<double>(end_it->second));
      }
      if (names.empty()) break;
      std::discrete_distribution<std::size_t> pick(weights.begin(), weights.end());
      const auto selected = pick(rng);
      const std::string next = names[selected];
      const double mean = next.empty()
                              ? 0.0
                              : performance_mean(performance, {current, next});
      if (mean > 0.0) instant += std::exponential_distribution<double>(1.0 / mean)(rng);
      if (next.empty()) break;
      current = next;
    }
    log.traces.push_back(std::move(trace));
  }
  return log;
}

}  // namespace

DfgPlayoutResult dfg_playout_result(
    const DfgFrequency& frequency,
    const DfgActivityCounts& start_activities,
    const DfgActivityCounts& end_activities,
    const DfgPlayoutOptions& options,
    const DfgPerformance* performance) {
  validate_options(options, performance);
  if (options.variant == DfgPlayoutVariant::kPerformance &&
      performance == nullptr) {
    throw std::invalid_argument(
        "performance DFG playout requires performance statistics");
  }
  if (options.variant == DfgPlayoutVariant::kPerformance) {
    DfgPlayoutResult result;
    result.variant = DfgPlayoutVariant::kPerformance;
    result.log = performance_playout(frequency, start_activities,
                                     end_activities, options, performance);
    result.complete = true;
    return result;
  }
  return classic_playout(frequency, start_activities, end_activities, options);
}

EventLog dfg_playout(const DfgFrequency& frequency,
                     const DfgActivityCounts& start_activities,
                     const DfgActivityCounts& end_activities,
                     const DfgPlayoutOptions& options,
                     const DfgPerformance* performance) {
  return dfg_playout_result(frequency, start_activities, end_activities,
                            options, performance)
      .log;
}

}  // namespace minera
