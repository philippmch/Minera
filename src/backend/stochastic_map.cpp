#include "minera/stochastic_map.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <iomanip>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

#include "minera/timestamp.hpp"

namespace minera {
namespace {

constexpr double kEpsilon = 1e-12;

double positive_duration(double mean) {
  return mean > kEpsilon ? mean : kEpsilon;
}

const char* timing_name(StochasticMapTiming timing) {
  switch (timing) {
    case StochasticMapTiming::kTimed: return "timed";
    case StochasticMapTiming::kImmediate: return "immediate";
  }
  throw std::invalid_argument("stochastic map: unknown timing");
}

const char* distribution_name(StochasticMapDistribution distribution) {
  switch (distribution) {
    case StochasticMapDistribution::kDeterministic: return "deterministic";
    case StochasticMapDistribution::kExponential: return "exponential";
    case StochasticMapDistribution::kConstant0: return "constant0";
    case StochasticMapDistribution::kImmediate: return "immediate";
    case StochasticMapDistribution::kNormal: return "normal";
    case StochasticMapDistribution::kUniform: return "uniform";
    case StochasticMapDistribution::kLogNormal: return "lognormal";
    case StochasticMapDistribution::kGamma: return "gamma";
  }
  throw std::invalid_argument("stochastic map: unknown distribution");
}

void validate_finite_nonnegative(double value, std::string_view name) {
  if (!std::isfinite(value) || value < 0.0)
    throw std::invalid_argument(std::string("stochastic map: ") +
                                std::string(name) +
                                " must be finite and non-negative");
}

void validate_options(const StochasticMapFitOptions& options) {
  validate_finite_nonnegative(options.default_parameter, "default_parameter");
  if (options.distribution == StochasticMapDistribution::kExponential &&
      options.default_parameter <= 0.0)
    throw std::invalid_argument(
        "stochastic map: exponential default_parameter must be positive");
  if (options.distribution == StochasticMapDistribution::kGamma &&
      options.default_parameter <= 0.0)
    throw std::invalid_argument(
        "stochastic map: gamma default_parameter must be positive");
}

void validate_performance_options(const StochasticMapPerformanceOptions& options) {
  validate_finite_nonnegative(options.default_parameter, "default_parameter");
  validate_finite_nonnegative(options.default_activation_weight,
                              "default_activation_weight");
  if (options.distribution == StochasticMapDistribution::kExponential &&
      options.default_parameter <= 0.0)
    throw std::invalid_argument(
        "stochastic map: exponential default_parameter must be positive");
  if (options.distribution == StochasticMapDistribution::kGamma &&
      options.default_parameter <= 0.0)
    throw std::invalid_argument(
        "stochastic map: gamma default_parameter must be positive");
}

double mean_equivalent(const StochasticMapTransition& transition) {
  switch (transition.distribution) {
    case StochasticMapDistribution::kConstant0:
    case StochasticMapDistribution::kImmediate:
      return 0.0;
    case StochasticMapDistribution::kDeterministic:
    case StochasticMapDistribution::kNormal:
      return transition.parameter;
    case StochasticMapDistribution::kExponential:
      return transition.parameter > kEpsilon ? 1.0 / transition.parameter : 0.0;
    case StochasticMapDistribution::kUniform:
      return (transition.parameter + transition.secondary_parameter) / 2.0;
    case StochasticMapDistribution::kLogNormal:
      if (transition.secondary_parameter == 0.0)
        return std::exp(transition.parameter);
      return std::exp(transition.parameter +
                      0.5 * transition.secondary_parameter *
                          transition.secondary_parameter);
    case StochasticMapDistribution::kGamma:
      return transition.parameter * transition.secondary_parameter;
  }
  return 0.0;
}

void set_zero_duration(StochasticMapTransition& transition) {
  transition.timing = StochasticMapTiming::kImmediate;
  transition.distribution = StochasticMapDistribution::kConstant0;
  transition.parameter = 0.0;
  transition.secondary_parameter = 0.0;
  transition.tertiary_parameter = 0.0;
}

void initialize_distribution(StochasticMapTransition& transition,
                             StochasticMapDistribution distribution,
                             double mean, bool zero_duration_immediate) {
  transition.distribution = distribution;
  transition.timing = StochasticMapTiming::kTimed;
  transition.parameter = 0.0;
  transition.secondary_parameter = 0.0;
  transition.tertiary_parameter = 0.0;
  switch (distribution) {
    case StochasticMapDistribution::kConstant0:
    case StochasticMapDistribution::kImmediate:
      set_zero_duration(transition);
      return;
    case StochasticMapDistribution::kDeterministic:
      transition.parameter = mean;
      break;
    case StochasticMapDistribution::kExponential:
      transition.parameter = 1.0 / positive_duration(mean);
      break;
    case StochasticMapDistribution::kNormal:
      transition.parameter = mean;
      break;
    case StochasticMapDistribution::kUniform:
      transition.parameter = mean;
      transition.secondary_parameter = mean;
      break;
    case StochasticMapDistribution::kLogNormal:
      transition.parameter = std::log(positive_duration(mean));
      break;
    case StochasticMapDistribution::kGamma:
      transition.parameter = positive_duration(mean);
      transition.secondary_parameter = 1.0;
      break;
  }
  if (zero_duration_immediate && mean <= kEpsilon)
    set_zero_duration(transition);
}

void set_fitted_parameter(StochasticMapTransition& transition,
                          const std::vector<double>& samples,
                          bool zero_duration_immediate) {
  if (samples.empty()) return;
  const double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
  const double mean = sum / static_cast<double>(samples.size());
  if (!std::isfinite(mean) || mean < 0.0) return;
  switch (transition.distribution) {
    case StochasticMapDistribution::kConstant0:
    case StochasticMapDistribution::kImmediate:
      set_zero_duration(transition);
      break;
    case StochasticMapDistribution::kExponential:
      if (mean <= kEpsilon) {
        if (zero_duration_immediate) {
          set_zero_duration(transition);
        } else {
          transition.parameter = 1.0 / kEpsilon;
        }
      } else {
        transition.parameter = 1.0 / mean;
      }
      break;
    case StochasticMapDistribution::kDeterministic:
      transition.parameter = mean;
      break;
    case StochasticMapDistribution::kNormal: {
      transition.parameter = mean;
      double variance = 0.0;
      for (const auto value : samples) {
        const double delta = value - mean;
        variance += delta * delta;
      }
      transition.secondary_parameter =
          std::sqrt(variance / static_cast<double>(samples.size()));
      break;
    }
    case StochasticMapDistribution::kUniform: {
      const auto [minimum, maximum] =
          std::minmax_element(samples.begin(), samples.end());
      transition.parameter = *minimum;
      transition.secondary_parameter = *maximum;
      break;
    }
    case StochasticMapDistribution::kLogNormal: {
      std::vector<double> logs;
      logs.reserve(samples.size());
      for (const auto value : samples)
        if (value > kEpsilon) logs.push_back(std::log(value));
      if (logs.empty()) {
        if (zero_duration_immediate) {
          set_zero_duration(transition);
        } else {
          transition.parameter = std::log(kEpsilon);
          transition.secondary_parameter = 0.0;
        }
        break;
      }
      const double log_mean =
          std::accumulate(logs.begin(), logs.end(), 0.0) /
          static_cast<double>(logs.size());
      double variance = 0.0;
      for (const auto value : logs) {
        const double delta = value - log_mean;
        variance += delta * delta;
      }
      transition.parameter = log_mean;
      transition.secondary_parameter =
          std::sqrt(variance / static_cast<double>(logs.size()));
      break;
    }
    case StochasticMapDistribution::kGamma: {
      double variance = 0.0;
      for (const auto value : samples) {
        const double delta = value - mean;
        variance += delta * delta;
      }
      variance /= static_cast<double>(samples.size());
      if (mean <= kEpsilon) {
        if (zero_duration_immediate) {
          set_zero_duration(transition);
        } else {
          transition.parameter = kEpsilon;
          transition.secondary_parameter = 1.0;
        }
      } else if (variance <= kEpsilon) {
        transition.parameter = mean;
        transition.secondary_parameter = 1.0;
      } else {
        transition.secondary_parameter = mean * mean / variance;
        transition.parameter = variance / mean;
      }
      break;
    }
  }
}

StochasticMapTransition default_transition(
    const PetriNet& net, std::size_t index,
    const StochasticMapFitOptions& options) {
  StochasticMapTransition result;
  result.transition = index;
  result.priority = options.default_priority;
  result.activation_weight = 1.0;
  initialize_distribution(result, options.distribution.value_or(
                              StochasticMapDistribution::kExponential),
                          options.default_parameter,
                          options.zero_duration_immediate);
  if (index < net.transitions().size() &&
      !net.transitions()[index].activity && options.silent_transitions_immediate) {
    result.timing = StochasticMapTiming::kImmediate;
    result.distribution = StochasticMapDistribution::kImmediate;
    result.parameter = 0.0;
    result.secondary_parameter = 0.0;
    result.tertiary_parameter = 0.0;
  }
  return result;
}

StochasticMapTransition performance_transition(
    const PetriNet& net, std::size_t index,
    const DfgPerformance& performance,
    const StochasticMapPerformanceOptions& options) {
  StochasticMapTransition result;
  result.transition = index;
  result.activation_weight = options.default_activation_weight;
  result.priority = options.default_priority;
  initialize_distribution(result, options.distribution,
                          options.default_parameter,
                          options.zero_duration_immediate);
  if (index >= net.transitions().size()) return result;
  const auto& petri_transition = net.transitions()[index];
  if (!petri_transition.activity ||
      *petri_transition.activity >= net.activities().size()) {
    if (options.zero_duration_immediate) {
      result.timing = StochasticMapTiming::kImmediate;
      result.distribution = StochasticMapDistribution::kImmediate;
      result.parameter = 0.0;
      result.secondary_parameter = 0.0;
      result.tertiary_parameter = 0.0;
    }
    return result;
  }
  const auto& activity = net.activities()[*petri_transition.activity];
  double weighted_duration = 0.0;
  double weight = 0.0;
  for (const auto& [edge, stats] : performance) {
    if (edge.second != activity || stats.summary.count == 0) continue;
    weighted_duration += stats.summary.mean *
                         static_cast<double>(stats.summary.count);
    weight += static_cast<double>(stats.summary.count);
  }
  if (weight > 0.0 && std::isfinite(weight)) {
    const double mean = weighted_duration / weight;
    initialize_distribution(result, options.distribution, mean,
                            options.zero_duration_immediate);
    if (options.frequency_activation_weights)
      result.activation_weight = weight;
  }
  return result;
}

}  // namespace

const char* stochastic_map_timing_name(StochasticMapTiming timing) {
  return timing_name(timing);
}

const char* stochastic_map_distribution_name(
    StochasticMapDistribution distribution) {
  return distribution_name(distribution);
}

void validate_sparse_stochastic_map(const StochasticMap& map,
                             std::size_t expected_transition_count) {
  for (std::size_t index = 0; index < map.transitions.size(); ++index) {
    const auto& transition = map.transitions[index];
    if (transition.transition >= expected_transition_count ||
        (index > 0 && transition.transition <=
                          map.transitions[index - 1].transition))
      throw std::invalid_argument(
          "stochastic map: transition records must be index ordered");
    validate_finite_nonnegative(transition.activation_weight,
                                "activation_weight");
    if (!std::isfinite(transition.parameter) ||
        !std::isfinite(transition.secondary_parameter) ||
        !std::isfinite(transition.tertiary_parameter))
      throw std::invalid_argument("stochastic map: parameters must be finite");
    if ((transition.secondary_parameter < 0.0 &&
         transition.distribution != StochasticMapDistribution::kUniform) ||
        transition.tertiary_parameter < 0.0)
      throw std::invalid_argument(
        "stochastic map: parameters must be non-negative");
    (void)timing_name(transition.timing);
    (void)distribution_name(transition.distribution);
    switch (transition.distribution) {
      case StochasticMapDistribution::kExponential:
        if (transition.parameter < 0.0)
          throw std::invalid_argument(
              "stochastic map: exponential rates must be non-negative");
        if (transition.timing == StochasticMapTiming::kTimed &&
            transition.parameter <= 0.0)
          throw std::invalid_argument(
              "stochastic map: timed exponential rates must be positive");
        break;
      case StochasticMapDistribution::kUniform:
        if (transition.secondary_parameter < transition.parameter)
          throw std::invalid_argument(
              "stochastic map: uniform bounds must be ordered");
        break;
      case StochasticMapDistribution::kNormal:
        break;
      case StochasticMapDistribution::kDeterministic:
        if (transition.parameter < 0.0)
          throw std::invalid_argument(
              "stochastic map: duration means must be non-negative");
        break;
      case StochasticMapDistribution::kLogNormal:
        // Lognormal location is allowed to be negative for sub-unit durations.
        break;
      case StochasticMapDistribution::kGamma:
        if (transition.parameter <= 0.0 ||
            transition.secondary_parameter <= 0.0)
          throw std::invalid_argument(
              "stochastic map: timed gamma scale and shape must be positive");
        break;
      case StochasticMapDistribution::kConstant0:
      case StochasticMapDistribution::kImmediate:
        if (transition.parameter != 0.0 ||
            transition.secondary_parameter != 0.0 ||
            transition.tertiary_parameter != 0.0)
          throw std::invalid_argument(
              "stochastic map: immediate distributions must have zero parameters");
        break;
    }
  }
}

void validate_stochastic_map(const StochasticMap& map,
                             std::size_t expected_transition_count) {
  validate_sparse_stochastic_map(map, expected_transition_count);
  if (map.transitions.size() != expected_transition_count)
    throw std::invalid_argument(
        "stochastic map: transition record count does not match the net");
}

StochasticMapTransition fit_stochastic_distribution(
    const std::vector<double>& samples,
    std::optional<StochasticMapDistribution> forced) {
  for (double value : samples)
    if (!std::isfinite(value))
      throw std::invalid_argument("stochastic map: fit samples must be finite");
  StochasticMapTransition immediate;
  set_zero_duration(immediate);
  immediate.distribution = StochasticMapDistribution::kImmediate;
  immediate.activation_weight = 0.0;
  immediate.priority = 1;
  const double count = static_cast<double>(samples.size());
  const double mean = samples.empty() ? 0.0 :
      std::accumulate(samples.begin(), samples.end(), 0.0) / count;
  if (!std::isfinite(mean))
    throw std::invalid_argument("stochastic map: fitted mean must be finite");
  auto exponential = immediate;
  exponential.timing = StochasticMapTiming::kTimed;
  exponential.distribution = StochasticMapDistribution::kExponential;
  exponential.priority = 0;
  if (mean > 0.0) exponential.parameter = 1.0 / mean;
  if (forced == StochasticMapDistribution::kExponential)
    return mean > 0.0 ? exponential : immediate;
  if (samples.size() < 2) return immediate;
  if (forced && forced != StochasticMapDistribution::kNormal &&
      forced != StochasticMapDistribution::kUniform) return immediate;
  if (!forced && std::any_of(samples.begin(), samples.end(),
                            [](double value) { return value < 0.0; }))
    throw std::invalid_argument(
        "stochastic map: automatic exponential fit requires non-negative samples");
  double variance = 0.0;
  for (double value : samples) variance += (value - mean) * (value - mean);
  variance /= count;
  const double deviation = std::sqrt(variance);
  const auto [minimum, maximum] = std::minmax_element(samples.begin(), samples.end());
  const double width = *maximum - *minimum;
  auto normal = exponential;
  normal.distribution = StochasticMapDistribution::kNormal;
  normal.parameter = mean;
  normal.secondary_parameter = deviation;
  auto uniform = exponential;
  uniform.distribution = StochasticMapDistribution::kUniform;
  uniform.parameter = *minimum;
  uniform.secondary_parameter = *maximum;
  auto best = immediate;
  double likelihood = std::all_of(samples.begin(), samples.end(),
      [](double value) { return std::abs(value) < 0.0001; })
      ? std::numeric_limits<double>::max() : -std::numeric_limits<double>::max();
  auto consider = [&](const StochasticMapTransition& candidate, double score) {
    if (score > likelihood) { best = candidate; likelihood = score; }
  };
  if ((!forced || forced == StochasticMapDistribution::kNormal) && deviation > 0.0)
    consider(normal, -count * (std::log(deviation) +
                              0.5 * std::log(2.0 * std::acos(-1.0)) + 0.5));
  if ((!forced || forced == StochasticMapDistribution::kUniform) && width > 0.0)
    consider(uniform, -count * std::log(width));
  if (!forced && mean > 0.0)
    consider(exponential, -count * (std::log(mean) + 1.0));
  return best;
}

StochasticMapFitResult fit_stochastic_map_with_report(
    const EventLog& log, const PetriNet& net,
    const StochasticMapFitOptions& options) {
  validate_options(options);
  // Replay owns routing, duplicate-label selection and context validation. Unfit
  // traces still supply the activated path used by the public statistics.
  const auto rows = replay_token_traces(log, net, options.replay);
  StochasticMapFitResult result;
  std::vector<StochasticMapTransitionStatistics> statistics(net.transitions().size());
  for (std::size_t index = 0; index < statistics.size(); ++index)
    statistics[index].transition = index;
  auto selected_attribute = [](const Event& event, std::string_view key)
      -> const EventAttribute* {
    const EventAttribute* selected = nullptr;
    int priority = 0;
    for (const auto& attribute : event.attributes) {
      const int candidate = attribute.key == key ? 3 :
          (!attribute.first_class_source_key.empty() &&
           attribute.first_class_source_key == key ? 2 : 0);
      if (candidate > priority) { selected = &attribute; priority = candidate; }
    }
    return selected;
  };
  auto selected_time = [&](const Event& event) -> std::optional<std::int64_t> {
    const auto key = options.replay.timestamp_attribute.empty()
        ? std::string_view("time:timestamp")
        : std::string_view(options.replay.timestamp_attribute);
    if (const auto* value = selected_attribute(event, key)) {
      if (value->is_null || value->first_class_state == FirstClassAttributeState::kNull)
        return std::nullopt;
      return parse_timestamp_microseconds(value->value);
    }
    if (key == "time:timestamp" || key == "timestamp" ||
        key == event.timestamp_source_key) {
      if (event.timestamp_state == FirstClassAttributeState::kNull) return std::nullopt;
      return parse_timestamp_microseconds(event.timestamp);
    }
    return std::nullopt;
  };
  auto selected_activity = [&](const Event& event) -> std::optional<std::string> {
    const auto key = options.replay.activity_attribute.empty()
        ? std::string_view("concept:name")
        : std::string_view(options.replay.activity_attribute);
    if (const auto* attribute = selected_attribute(event, key)) {
      if (attribute->is_null || attribute->first_class_state == FirstClassAttributeState::kNull)
        return std::nullopt;
      return attribute->value;
    }
    if (event.activity_state == FirstClassAttributeState::kNull) return std::nullopt;
    if (key == "concept:name" || key == "activity" ||
        key == event.activity_source_key) return event.activity;
    return std::nullopt;
  };
  for (std::size_t trace_index = 0; trace_index < rows.size(); ++trace_index) {
    ++result.replayed_traces;
    const auto& trace = log.traces[trace_index];
    std::vector<std::uint64_t> marking(net.place_count(), 0);
    if (options.replay.initial_marking.empty()) {
      if (net.source() < marking.size()) marking[net.source()] = 1;
    } else {
      std::copy(options.replay.initial_marking.begin(),
                options.replay.initial_marking.end(), marking.begin());
    }
    std::vector<std::deque<std::optional<std::int64_t>>> arrivals(net.place_count());
    auto initial_arrivals = marking;
    const auto first_time = trace.events.empty() ? std::nullopt
                                                 : selected_time(trace.events.front());
    std::size_t event_index = 0;
    for (const auto transition_index : rows[trace_index].activated_transitions) {
      for (std::size_t index = 0; index < statistics.size(); ++index)
        if (index == transition_index ||
            petri_transition_enabled(net.transitions()[index], marking))
          ++statistics[index].enablings;
      auto& stats = statistics[transition_index];
      ++stats.activations;
      const auto& transition = net.transitions()[transition_index];
      std::optional<std::int64_t> arrival;
      std::vector<bool> consumed(net.place_count(), false);
      for (const auto place : transition.inputs) {
        if (!consumed[place]) {
          std::optional<std::int64_t> consumed_time;
          if (initial_arrivals[place] > 0) {
            --initial_arrivals[place];
            consumed_time = first_time;
          } else if (!arrivals[place].empty()) {
            consumed_time = arrivals[place].front();
            arrivals[place].pop_front();
          }
          if (consumed_time && (!arrival || *consumed_time > *arrival))
            arrival = consumed_time;
        }
        consumed[place] = true;
        if (marking[place] > 0) --marking[place];
      }
      for (const auto place : transition.reset_inputs) {
        marking[place] = 0;
        arrivals[place].clear();
        initial_arrivals[place] = 0;
      }
      std::optional<std::int64_t> time;
      if (transition.activity && event_index < trace.events.size()) {
        const auto& event = trace.events[event_index];
        time = selected_time(event);
        if (time && arrival) {
          stats.performance.push_back(static_cast<double>(*time - *arrival) / 1000000.0);
          stats.log_indices.push_back(trace_index);
        }
        const auto activity = selected_activity(event);
        if (activity && *activity == net.activities()[*transition.activity])
          ++event_index;
      }
      std::vector<bool> produced(net.place_count(), false);
      for (const auto place : transition.outputs) {
        if (marking[place] == std::numeric_limits<std::uint64_t>::max())
          throw std::overflow_error("stochastic map: marking overflow");
        ++marking[place];
        if (!produced[place]) arrivals[place].push_back(time);
        produced[place] = true;
      }
    }
  }
  for (const auto& stats : statistics) {
    if (stats.enablings == 0 && stats.activations == 0) continue;
    auto record = default_transition(net, stats.transition, options);
    if (options.fit_activation_weights)
      record.activation_weight = static_cast<double>(stats.activations) /
                                 static_cast<double>(stats.enablings);
    if (options.fit_timing &&
        (net.transitions()[stats.transition].activity ||
         options.silent_transitions_immediate)) {
      if (!options.zero_duration_immediate) {
        set_fitted_parameter(record, stats.performance, false);
      } else {
        auto fitted = fit_stochastic_distribution(stats.performance, options.distribution);
        fitted.transition = record.transition;
        fitted.activation_weight = record.activation_weight;
        if (options.default_priority != 0) fitted.priority = options.default_priority;
        record = fitted;
      }
    }
    result.map.transitions.push_back(record);
    result.statistics.push_back(stats);
  }
  validate_sparse_stochastic_map(result.map, net.transitions().size());
  return result;
}

StochasticMapPetriModel stochastic_map_from_performance_dfg(
    const DirectlyFollowsGraph& graph, const DfgPerformance& performance,
    const StochasticMapPerformanceOptions& options) {
  validate_performance_options(options);
  StochasticMapPetriModel result;
  result.net = convert_dfg_to_petri_net(graph, options.conversion);
  result.map.transitions.reserve(result.net.transitions().size());
  for (std::size_t index = 0; index < result.net.transitions().size(); ++index)
    result.map.transitions.push_back(
        performance_transition(result.net, index, performance, options));
  validate_stochastic_map(result.map, result.net.transitions().size());
  return result;
}

StochasticMapPetriModel stochastic_map_from_performance_dfg(
    const DfgCleanAnalysis& graph, const DfgPerformance& performance,
    const StochasticMapPerformanceOptions& options) {
  validate_performance_options(options);
  StochasticMapPetriModel result;
  result.net = convert_dfg_to_petri_net(graph, options.conversion);
  result.map.transitions.reserve(result.net.transitions().size());
  for (std::size_t index = 0; index < result.net.transitions().size(); ++index)
    result.map.transitions.push_back(
        performance_transition(result.net, index, performance, options));
  validate_stochastic_map(result.map, result.net.transitions().size());
  return result;
}

StochasticPetriOptions stochastic_map_to_options(
    const StochasticMap& map, std::vector<std::uint32_t> initial_marking,
    std::size_t max_states) {
  validate_stochastic_map(map, map.transitions.size());
  if (max_states == 0 || max_states > kStochasticPetriMaxGeneratorStates)
    throw std::invalid_argument(
        "stochastic map: max_states exceeds the CTMC safety limit");
  StochasticPetriOptions options;
  options.max_states = max_states;
  options.initial_marking = std::move(initial_marking);
  options.transition_rates.reserve(map.transitions.size());
  options.immediate_weights.reserve(map.transitions.size());
  options.transition_priorities.reserve(map.transitions.size());
  for (const auto& transition : map.transitions) {
    options.immediate_weights.push_back(transition.activation_weight);
    options.transition_priorities.push_back(transition.priority);
    if (transition.timing == StochasticMapTiming::kImmediate ||
        transition.distribution == StochasticMapDistribution::kImmediate ||
        transition.distribution == StochasticMapDistribution::kConstant0) {
      options.immediate_transitions.push_back(transition.transition);
      options.transition_rates.push_back(0.0);
      continue;
    }
    const double mean = mean_equivalent(transition);
    if (!std::isfinite(mean) || mean < 0.0)
      throw std::domain_error(
          "stochastic map: CTMC distribution mean must be finite and non-negative");
    const double rate =
        transition.distribution == StochasticMapDistribution::kExponential
            ? transition.parameter
            : (mean > kEpsilon ? 1.0 / mean : 0.0);
    const double weighted_rate = rate * transition.activation_weight;
    if (!std::isfinite(weighted_rate) || weighted_rate < 0.0)
      throw std::invalid_argument(
          "stochastic map: weighted transition rate is not finite");
    options.transition_rates.push_back(weighted_rate);
    if (mean <= kEpsilon)
      options.immediate_transitions.push_back(transition.transition);
  }
  return options;
}

StochasticPetriOptions stochastic_map_to_options(
    const PetriNet& net, const StochasticMap& map,
    const StochasticMapFitOptions& fit_options,
    std::vector<std::uint32_t> initial_marking, std::size_t max_states) {
  validate_options(fit_options);
  validate_sparse_stochastic_map(map, net.transitions().size());
  StochasticMap dense;
  dense.transitions.reserve(net.transitions().size());
  for (std::size_t index = 0; index < net.transitions().size(); ++index)
    dense.transitions.push_back(default_transition(net, index, fit_options));
  for (const auto& record : map.transitions)
    dense.transitions[record.transition] = record;
  return stochastic_map_to_options(dense, std::move(initial_marking), max_states);
}

std::string serialize_stochastic_map_json(const StochasticMap& map) {
  validate_sparse_stochastic_map(map, std::numeric_limits<std::size_t>::max());
  std::ostringstream out;
  out << "{\"family\":\"STOCHASTIC_MAP\",\"transitions\":[";
  for (std::size_t index = 0; index < map.transitions.size(); ++index) {
    if (index != 0) out << ',';
    const auto& transition = map.transitions[index];
    out << "{\"transition\":" << transition.transition
        << ",\"timing\":\"" << timing_name(transition.timing)
        << "\",\"distribution\":\""
        << distribution_name(transition.distribution)
        << "\",\"parameter\":" << std::setprecision(17)
        << transition.parameter << ",\"secondary_parameter\":"
        << transition.secondary_parameter << ",\"tertiary_parameter\":"
        << transition.tertiary_parameter << ",\"activation_weight\":"
        << transition.activation_weight << ",\"priority\":"
        << transition.priority << '}';
  }
  out << "]}";
  return out.str();
}

}  // namespace minera
