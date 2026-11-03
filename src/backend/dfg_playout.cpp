#include "minera/dfg_playout.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <queue>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "minera/filtering_common.hpp"
#include "minera/random.hpp"
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

// The reference stamps generated events from epoch 10,000,000
// (1970-04-26T17:46:40Z); an explicit start_time replaces that base (a
// declared extension).
constexpr double kReferenceEpoch = 10000000.0;

double start_epoch(const DfgPlayoutOptions& options) {
  if (options.start_time.empty()) return kReferenceEpoch;
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
  if (options.activity_key.empty() || options.timestamp_key.empty() ||
      options.case_id_key.empty()) {
    throw std::invalid_argument(
        "DFG playout attribute keys must not be empty");
  }
  if (options.artificial_start_activity.empty() ||
      options.artificial_end_activity.empty()) {
    throw std::invalid_argument(
        "DFG playout artificial boundary labels must not be empty");
  }
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
  if (!options.start_time.empty() && !parse_timestamp(options.start_time).has_value()) {
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
                 double instant, std::size_t order,
                 const DfgPlayoutOptions& options) {
  Event event;
  event.case_id = case_id;
  event.activity = activity;
  // datetime.fromtimestamp(t, utc): microseconds, ties to even.
  event.timestamp = format_timestamp_microseconds(instant);
  event.input_order = order;
  if (options.activity_key != "activity" &&
      options.activity_key != "concept:name") {
    event.attributes.push_back(
        {options.activity_key, activity, AttributeKind::kString});
  }
  if (options.timestamp_key != "timestamp" &&
      options.timestamp_key != "time:timestamp") {
    event.attributes.push_back(
        {options.timestamp_key, event.timestamp, AttributeKind::kDate});
  }
  return event;
}

// The default key is the trace's concept:name (case:concept:name in frame
// terms), so only another name adds an attribute (a declared extension).
void add_case_id_attribute(Trace& trace, const DfgPlayoutOptions& options) {
  if (options.case_id_key == "case_id" ||
      options.case_id_key == "concept:name" ||
      options.case_id_key == "case:concept:name") {
    return;
  }
  trace.attributes.push_back(
      {options.case_id_key, trace.case_id, AttributeKind::kString});
}

std::uint64_t variant_occurrence_count(double probability,
                                       std::size_t max_variants) {
  if (!std::isfinite(probability) || probability <= 0.0 || max_variants == 0)
    return 0;
  // ceil(probability * max_variants) in binary64, as the reference computes
  // it: 0.2 * 5 is 1.0 there (a wider product would round up to 2).
  const double scaled =
      std::ceil(probability * static_cast<double>(max_variants));
  if (scaled >= 18446744073709551616.0)
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
  double clock = start_epoch(options);
  if (start_total <= 0.0 || options.max_variants == 0) {
    result.interrupted = start_total > 0.0 && options.max_variants == 0;
    return result;
  }
  // A trace is found when its own state leaves the queue and its last
  // activity can end; the caps, the probability budget and the coverage
  // interruption apply in that search order. The traces found are then
  // listed as the reference lists them: by descending probability, equal
  // probabilities by their activity sequence. Case ids and the one-second
  // clock follow that list.
  struct FoundTrace {
    std::vector<std::string> activities;
    double probability = 0.0;
  };
  std::vector<FoundTrace> found;
  std::map<std::string, double> out_totals;
  for (const auto& [activity, outgoing] : transitions) {
    double total = 0.0;
    for (const auto& item : outgoing) total += item.second;
    out_totals[activity] = total;
  }
  // false: the search stops (a trace below the minimum occurrences, the
  // probability budget spent, or the DFG covered when that interrupts).
  bool stopped = false;
  const auto consider = [&](const std::vector<std::string>& activities, double path_cost) {
    const auto out_it = transitions.find(activities.back());
    if (out_it == transitions.end()) return true;
    const auto end_it = out_it->second.find(std::string{});
    if (end_it == out_it->second.end() || end_it->second <= 0.0) return true;
    const double cost = path_cost - std::log(end_it->second / out_totals[activities.back()]);
    const double probability = std::exp(-cost);
    if (!std::isfinite(probability) || probability <= 0.0) return true;
    // The cap applies to a trace found beyond it, so a search that ends
    // with exactly max_variants traces is not cut short.
    if (found.size() >= options.max_variants) {
      result.interrupted = true;
      return false;
    }
    const std::uint64_t variant_occurrences =
        variant_occurrence_count(probability, options.max_variants);
    max_occ = std::max<std::size_t>(
        max_occ, static_cast<std::size_t>(std::min<std::uint64_t>(
                     variant_occurrences, std::numeric_limits<std::size_t>::max())));
    if (variant_occurrences < options.min_variant_occurrences &&
        options.min_variant_occurrences <= max_occ)
      return false;
    if (cumulative_probability > options.min_weighted_probability) {
      result.interrupted = true;
      return false;
    }
    // A trace skipped for adding nothing new still spends its probability.
    cumulative_probability += probability;
    bool adds_new = !seen_starts.contains(activities.front()) ||
                    !seen_ends.contains(activities.back());
    for (std::size_t i = 1; i < activities.size(); ++i)
      if (!seen_edges.contains({activities[i - 1], activities[i]})) adds_new = true;
    if (options.add_trace_if_takes_new_edges && !adds_new) return true;
    seen_starts.insert(activities.front());
    seen_ends.insert(activities.back());
    for (std::size_t i = 1; i < activities.size(); ++i)
      seen_edges.insert({activities[i - 1], activities[i]});
    result.counts[activities] = variant_occurrences;
    found.push_back({activities, probability});
    result.complete = coverage_complete();
    return !(options.interrupt_when_dfg_complete && result.complete);
  };

  for (const auto& [activity, count] : starts) {
    if (count == 0) continue;
    queue.push(PartialTrace{-std::log(static_cast<double>(count) / start_total), {activity}});
  }

  while (!queue.empty() && !stopped) {
    if (++expansions > expansion_budget) {
      result.interrupted = true;
      break;
    }
    if (timed_out()) {
      result.interrupted = true;
      break;
    }
    auto state = queue.top();
    queue.pop();
    if (state.activities.empty()) continue;
    const auto out_it = transitions.find(state.activities.back());
    if (out_it == transitions.end()) continue;

    std::unordered_map<std::string, std::size_t> occurrences;
    for (const auto& activity : state.activities) ++occurrences[activity];
    const double total = out_totals[state.activities.back()];
    // The end weight sorts first (the empty name) in the successor map.
    for (const auto& [next, weight] : out_it->second) {
      if (weight <= 0.0) continue;
      if (next.empty()) {
        if (!consider(state.activities, state.cost)) {
          stopped = true;
          break;
        }
        continue;
      }
      if (occurrences[next] >= options.max_occurrences_per_activity) continue;
      PartialTrace child{state.cost - std::log(weight / total), state.activities};
      child.activities.push_back(next);
      queue.push(std::move(child));
    }
  }
  std::sort(found.begin(), found.end(), [](const auto& lhs, const auto& rhs) {
    if (lhs.probability != rhs.probability) return lhs.probability > rhs.probability;
    return lhs.activities < rhs.activities;
  });
  for (const auto& item : found) {
    Trace trace;
    // The reference names its cases "0", "1", ... and stamps events one
    // second apart from its epoch, counting on across cases.  Its traces
    // carry concept:name and the probability as a full-precision float;
    // the estimated occurrences stay in result.counts.
    trace.case_id = std::to_string(result.log.traces.size());
    add_case_id_attribute(trace, options);
    trace.attributes.push_back(
        {"probability", filtering_float_repr(item.probability), AttributeKind::kFloat});
    if (options.return_variants) {
      std::string variant;
      for (const auto& activity : item.activities) {
        if (!variant.empty()) variant.push_back(',');
        variant += activity;
      }
      trace.attributes.push_back({"variant", std::move(variant), AttributeKind::kString});
    }
    for (const auto& activity : item.activities) {
      trace.events.push_back(make_event(trace.case_id, activity, clock,
                                        trace.events.size(), options));
      clock += 1.0;
    }
    result.log.traces.push_back(std::move(trace));
  }
  // "Complete" is the search's: the reference returns nothing when the
  // search was cut short (the variant cap or the probability budget stopped
  // it, or a safety bound), whether or not the traces cover the DFG.
  if (options.return_only_if_complete && result.interrupted) {
    result.log = {};
    result.counts.clear();
  }
  return result;
}

// PERFORMANCE (exact under numpy.random.seed(seed)): the case clock starts at
// the epoch and gains case_arrival_rate once per case, before the case (a
// running sum, as the reference adds it, not epoch + (i + 1) * rate, which
// rounds differently for rates such as 0.3), with one numpy choice over the start
// activities (in start_activity_order, else map order); each step is one
// numpy choice over the activity's successors (edge-map order) followed by
// its end weight, and a chosen edge with a non-zero mean duration d adds
// d * standard_exponential() to the clock. The choice normalises the weights,
// cumulates them, divides by the last sum and takes the first index above one
// random_sample. Cases are "0", "1", ...; events carry the activity and the
// timestamp only.
DfgPlayoutResult performance_playout(const DfgFrequency& frequency,
                             const DfgActivityCounts& starts,
                             const DfgActivityCounts& ends,
                             const DfgPlayoutOptions& options,
                             const DfgPerformance* performance) {
  DfgPlayoutResult result;
  result.variant = DfgPlayoutVariant::kPerformance;
  if (options.seed > 0xffffffffULL)
    throw std::invalid_argument("DFG performance playout seed must be below 2^32 (numpy.random.seed)");
  if (options.traces == 0) {
    result.complete = true;
    return result;
  }
  if (options.max_events_per_trace == 0) {
    result.truncated_traces = options.traces;
    result.interrupted = true;
    return result;
  }
  rng::NumpyRandomState numpy(static_cast<std::uint32_t>(options.seed));
  struct Choice {
    std::string next;  // empty: the end of the case
    double weight = 0.0;
  };
  std::map<std::string, std::vector<Choice>> out;
  for (const auto& [edge, count] : frequency)
    if (count != 0) out[edge.first].push_back({edge.second, static_cast<double>(count)});
  for (const auto& [activity, count] : ends)
    if (count != 0) out[activity].push_back({std::string{}, static_cast<double>(count)});
  std::vector<Choice> start_choices;
  if (!options.start_activity_order.empty()) {
    std::set<std::string> listed;
    for (const auto& activity : options.start_activity_order) {
      const auto found = starts.find(activity);
      if (found == starts.end() || !listed.insert(activity).second)
        throw std::invalid_argument(
            "start_activity_order must list each start activity once");
      if (found->second != 0) start_choices.push_back({activity, static_cast<double>(found->second)});
    }
    for (const auto& [activity, count] : starts)
      if (count != 0 && !listed.contains(activity))
        throw std::invalid_argument("start_activity_order must list each start activity once");
  } else {
    for (const auto& [activity, count] : starts)
      if (count != 0) start_choices.push_back({activity, static_cast<double>(count)});
  }
  const auto draw = [&](const std::vector<Choice>& choices) -> const Choice& {
    std::vector<double> weights;
    weights.reserve(choices.size());
    for (const auto& choice : choices) weights.push_back(choice.weight);
    return choices[numpy.choice_index(weights)];
  };
  double case_start = start_epoch(options);

  for (std::size_t trace_index = 0; trace_index < options.traces; ++trace_index) {
    Trace trace;
    trace.case_id = std::to_string(trace_index);
    add_case_id_attribute(trace, options);
    case_start += options.case_arrival_rate;
    double instant = case_start;
    bool reached_end = false;
    if (!start_choices.empty()) {
      std::string current = draw(start_choices).next;
      trace.events.push_back(make_event(trace.case_id, current, instant, 0, options));
      while (true) {
        const auto it = out.find(current);
        if (it == out.end()) break;  // the reference has no successor either
        const auto& choice = draw(it->second);
        if (choice.next.empty()) {
          reached_end = true;
          break;
        }
        if (trace.events.size() >= options.max_events_per_trace) break;
        const double mean = performance_mean(performance, {current, choice.next});
        if (mean != 0.0) instant += mean * numpy.standard_exponential();
        current = choice.next;
        trace.events.push_back(make_event(trace.case_id, current, instant,
                                          trace.events.size(), options));
      }
    }
    result.log.traces.push_back(std::move(trace));
    if (reached_end)
      ++result.completed_traces;
    else
      ++result.truncated_traces;
  }
  result.complete = result.truncated_traces == 0;
  result.interrupted = !result.complete;
  return result;
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
    return performance_playout(frequency, start_activities, end_activities,
                               options, performance);
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
