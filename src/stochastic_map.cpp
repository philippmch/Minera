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

constexpr std::size_t kReplayStateLimit = 200000;
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

bool transition_matches(const PetriNet& net, const PetriTransition& transition,
                        std::string_view activity) {
  return transition.activity &&
         *transition.activity < net.activities().size() &&
         net.activities()[*transition.activity] == activity;
}

bool fire_checked(const PetriTransition& transition,
                  std::vector<std::uint32_t>& marking) {
  if (!petri_transition_enabled(transition, marking)) return false;
  for (const auto place : transition.inputs) {
    if (place >= marking.size() || marking[place] == 0) return false;
    --marking[place];
  }
  for (const auto place : transition.reset_inputs) {
    if (place >= marking.size()) return false;
    marking[place] = 0;
  }
  for (const auto place : transition.outputs) {
    if (place >= marking.size() ||
        marking[place] == std::numeric_limits<std::uint32_t>::max())
      return false;
    ++marking[place];
  }
  return true;
}

std::vector<std::uint32_t> default_marking(const PetriNet& net) {
  if (net.place_count() == 0 || net.source() >= net.place_count() ||
      net.sink() >= net.place_count())
    throw std::invalid_argument(
        "stochastic map: net has no valid source or sink place");
  std::vector<std::uint32_t> marking(net.place_count(), 0);
  marking[net.source()] = 1;
  return marking;
}

struct ReplayState {
  std::vector<std::uint32_t> marking;
  std::size_t event = 0;
  std::vector<std::size_t> path;
};

struct ReplayKey {
  std::vector<std::uint32_t> marking;
  std::size_t event = 0;

  friend bool operator<(const ReplayKey& left, const ReplayKey& right) {
    if (left.event != right.event) return left.event < right.event;
    return left.marking < right.marking;
  }
};

std::optional<std::vector<std::size_t>> replay_trace(
    const Trace& trace, const PetriNet& net) {
  ReplayState initial{default_marking(net), 0, {}};
  std::deque<ReplayState> queue;
  queue.push_back(std::move(initial));
  std::map<ReplayKey, bool> visited;
  visited.emplace(ReplayKey{queue.front().marking, 0}, true);
  std::size_t explored = 0;

  while (!queue.empty()) {
    ReplayState current = std::move(queue.front());
    queue.pop_front();
    if (++explored > kReplayStateLimit) return std::nullopt;
    if (current.event == trace.events.size()) {
      bool final_marking = net.sink() < current.marking.size() &&
                           current.marking[net.sink()] == 1;
      for (std::size_t place = 0; place < current.marking.size(); ++place)
        if (place != net.sink() && current.marking[place] != 0)
          final_marking = false;
      if (final_marking) return current.path;
    }

    const Event* event = current.event < trace.events.size()
                             ? &trace.events[current.event]
                             : nullptr;
    for (std::size_t index = 0; index < net.transitions().size(); ++index) {
      const auto& transition = net.transitions()[index];
      const bool silent = !transition.activity;
      if (!silent && (!event ||
                      !transition_matches(net, transition, event->activity)))
        continue;
      auto next_marking = current.marking;
      if (!fire_checked(transition, next_marking)) continue;
      const auto next_event = silent ? current.event : current.event + 1;
      ReplayKey key{next_marking, next_event};
      if (!visited.emplace(std::move(key), true).second) continue;
      auto next_path = current.path;
      next_path.push_back(index);
      queue.push_back(
          ReplayState{std::move(next_marking), next_event, std::move(next_path)});
    }
  }
  return std::nullopt;
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
  initialize_distribution(result, options.distribution,
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

void validate_stochastic_map(const StochasticMap& map,
                             std::size_t expected_transition_count) {
  if (map.transitions.size() != expected_transition_count)
    throw std::invalid_argument(
        "stochastic map: transition record count does not match the net");
  for (std::size_t index = 0; index < map.transitions.size(); ++index) {
    const auto& transition = map.transitions[index];
    if (transition.transition != index)
      throw std::invalid_argument(
          "stochastic map: transition records must be index ordered");
    validate_finite_nonnegative(transition.activation_weight,
                                "activation_weight");
    if (!std::isfinite(transition.parameter) ||
        !std::isfinite(transition.secondary_parameter) ||
        !std::isfinite(transition.tertiary_parameter))
      throw std::invalid_argument("stochastic map: parameters must be finite");
    if (transition.secondary_parameter < 0.0 ||
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
        if (transition.parameter < 0.0 ||
            transition.secondary_parameter < transition.parameter)
          throw std::invalid_argument(
              "stochastic map: uniform bounds must be ordered and non-negative");
        break;
      case StochasticMapDistribution::kNormal:
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

StochasticMapFitResult fit_stochastic_map_with_report(
    const EventLog& log, const PetriNet& net,
    const StochasticMapFitOptions& options) {
  validate_options(options);
  if (net.transitions().empty())
    throw std::invalid_argument("stochastic map: net has no transitions");
  StochasticMapFitResult result;
  result.map.transitions.reserve(net.transitions().size());
  for (std::size_t index = 0; index < net.transitions().size(); ++index)
    result.map.transitions.push_back(default_transition(net, index, options));

  std::vector<std::size_t> activations(net.transitions().size(), 0);
  std::vector<std::vector<double>> samples(net.transitions().size());
  for (const auto& trace : log.traces) {
    const auto path = replay_trace(trace, net);
    if (!path) {
      ++result.skipped_traces;
      continue;
    }
    ++result.replayed_traces;
    std::size_t event_index = 0;
    std::optional<double> previous_time;
    for (const auto transition_index : *path) {
      if (transition_index >= activations.size()) continue;
      ++activations[transition_index];
      const auto& transition = net.transitions()[transition_index];
      if (!transition.activity || event_index >= trace.events.size()) continue;
      const auto& event = trace.events[event_index++];
      const auto instant = parse_timestamp(event.timestamp);
      if (instant && previous_time && *instant >= *previous_time)
        samples[transition_index].push_back(*instant - *previous_time);
      if (instant) previous_time = *instant;
    }
  }

  if (options.fit_activation_weights) {
    const auto maximum = activations.empty()
                             ? 0U
                             : *std::max_element(activations.begin(),
                                                 activations.end());
    for (std::size_t index = 0; index < activations.size(); ++index)
      result.map.transitions[index].activation_weight =
          maximum == 0 ? 1.0
                       : static_cast<double>(activations[index]) /
                             static_cast<double>(maximum);
  }
  if (options.fit_timing) {
    for (std::size_t index = 0; index < samples.size(); ++index)
      set_fitted_parameter(result.map.transitions[index], samples[index],
                           options.zero_duration_immediate);
  }
  validate_stochastic_map(result.map, net.transitions().size());
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
      throw std::invalid_argument(
          "stochastic map: distribution mean is not finite");
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

std::string serialize_stochastic_map_json(const StochasticMap& map) {
  validate_stochastic_map(map, map.transitions.size());
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
