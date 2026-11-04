#include "minera/monte_carlo.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <queue>
#include <random>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "minera/timestamp.hpp"
#include "minera/random.hpp"

namespace minera {
namespace {

// The native (non-numpy) draws: Minera's own portable algorithms over
// std::mt19937_64, never the implementation-defined std distributions.
namespace portable = ::minera::rng;

using Marking = std::vector<std::uint64_t>;

struct CaseState {
  std::size_t index = 0;
  double arrival = 0.0;
  double now = 0.0;
  Marking marking;
  std::vector<bool> marking_keys;
  Trace trace;
  std::size_t steps = 0;
  bool finished = false;
  bool completed = false;
  bool deadlocked = false;
  bool step_limited = false;
};

struct QueueEntry {
  double time = 0.0;
  std::size_t case_index = 0;
  std::uint64_t serial = 0;
};

struct QueueEntryGreater {
  bool operator()(const QueueEntry& lhs, const QueueEntry& rhs) const {
    return std::tie(lhs.time, lhs.case_index, lhs.serial) >
           std::tie(rhs.time, rhs.case_index, rhs.serial);
  }
};

struct PreparedOptions {
  std::vector<double> weights;
  std::vector<MonteCarloDuration> durations;
  std::vector<std::size_t> capacities;
  Marking initial_marking;
  Marking final_marking;
  std::vector<bool> initial_keys;
  std::vector<bool> final_keys;
};

struct RuntimeBudget {
  std::size_t live_assignments = 0;
  std::size_t interval_records = 0;
};

constexpr std::string_view kDefaultActivityKey = "concept:name";
constexpr std::string_view kActivityAlias = "activity";
constexpr std::string_view kDefaultTimestampKey = "time:timestamp";
constexpr std::string_view kTimestampAlias = "timestamp";

// These limits keep the native API bounded as well as the HTTP adapter. They
// are deliberately expressed in terms of the allocations performed by the
// scheduler, rather than relying on allocator failures for malformed input.
constexpr std::size_t kMaxMonteCarloSimulations = 10'000;
constexpr std::size_t kMaxMonteCarloSteps = 10'000;
constexpr std::size_t kMaxMonteCarloResourceCapacity = 1'000;
constexpr std::size_t kMaxMonteCarloResourceSlots = 1'000'000;
constexpr std::size_t kMaxMonteCarloInitialTokens = 1'000'000;
constexpr std::size_t kMaxMonteCarloSimulationSteps = 1'000'000;
constexpr std::size_t kMaxMonteCarloRuntimeAssignments = 1'000'000;
constexpr std::size_t kMaxMonteCarloRuntimeIntervals = 1'000'000;

[[nodiscard]] bool is_activity_alias(std::string_view key) {
  return key == kDefaultActivityKey || key == kActivityAlias;
}

[[nodiscard]] bool is_timestamp_alias(std::string_view key) {
  return key == kDefaultTimestampKey || key == kTimestampAlias;
}

void validate_selectors(const MonteCarloOptions& options) {
  const auto blank = [](std::string_view key) {
    return key.empty() || std::all_of(key.begin(), key.end(), [](char c) {
             return std::isspace(static_cast<unsigned char>(c)) != 0;
           });
  };
  if (blank(options.activity_key) || blank(options.timestamp_key)) {
    throw std::invalid_argument(
        "Monte Carlo activity_key and timestamp_key must not be blank");
  }
  if (options.activity_key == options.timestamp_key) {
    throw std::invalid_argument(
        "Monte Carlo activity_key and timestamp_key must be distinct");
  }
}

[[nodiscard]] double effective_arrival_seconds(
    const MonteCarloOptions& options) {
  if (options.case_arrival_ratio) return *options.case_arrival_ratio;
  if (options.case_arrival_seconds) return *options.case_arrival_seconds;
  return 60.0;
}

void validate_arrival_aliases(const MonteCarloOptions& options) {
  if (options.case_arrival_ratio && options.case_arrival_seconds) {
    throw std::invalid_argument(
        "Monte Carlo case_arrival_ratio and case_arrival_seconds are aliases; supply only one");
  }
  for (const auto* value : {options.case_arrival_ratio ?
                                &*options.case_arrival_ratio : nullptr,
                            options.case_arrival_seconds ?
                                &*options.case_arrival_seconds : nullptr}) {
    if (value != nullptr && (!std::isfinite(*value) || *value < 0.0)) {
      throw std::invalid_argument(
          "Monte Carlo case arrival must be finite and non-negative");
    }
  }
}

void validate_force_distribution(const MonteCarloOptions& options) {
  if (options.force_distribution &&
      *options.force_distribution != MonteCarloDurationDistribution::kNormal &&
      *options.force_distribution != MonteCarloDurationDistribution::kUniform &&
      *options.force_distribution != MonteCarloDurationDistribution::kExponential) {
    throw std::invalid_argument(
        "Monte Carlo force_distribution must be normal, uniform, or exponential");
  }
}

[[nodiscard]] bool is_final_marking(const Marking& marking,
                                    const Marking& final_marking,
                                    const std::vector<bool>& keys,
                                    const std::vector<bool>& final_keys) {
  if (marking.size() != final_marking.size()) return false;
  for (std::size_t place = 0; place < marking.size(); ++place) {
    if ((final_keys[place] && !keys[place]) ||
        marking[place] < final_marking[place]) return false;
  }
  return true;
}

void validate_net(const PetriNet& net) {
  if (net.place_count() == 0 || net.source() >= net.place_count() ||
      net.sink() >= net.place_count()) {
    throw std::invalid_argument(
        "Monte Carlo requires valid source and sink places");
  }
  for (const auto& transition : net.transitions()) {
    if (transition.activity &&
        *transition.activity >= net.activities().size()) {
      throw std::invalid_argument(
          "Monte Carlo transition activity index is invalid");
    }
    for (const auto place : transition.inputs) {
      if (place >= net.place_count()) {
        throw std::invalid_argument(
            "Monte Carlo transition input place is invalid");
      }
    }
    for (const auto place : transition.outputs) {
      if (place >= net.place_count()) {
        throw std::invalid_argument(
            "Monte Carlo transition output place is invalid");
      }
    }
    for (const auto place : transition.reset_inputs) {
      if (place >= net.place_count()) throw std::invalid_argument("Monte Carlo transition reset place is invalid");
    }
    for (const auto place : transition.inhibitor_inputs) {
      if (place >= net.place_count()) throw std::invalid_argument("Monte Carlo transition inhibitor place is invalid");
    }
  }
}

void validate_stochastic_map_record(const StochasticMapTransition& transition,
                                    std::size_t transition_count) {
  if (transition.transition >= transition_count) {
    throw std::invalid_argument(
        "Monte Carlo stochastic_map transition index is out of range");
  }
  if (!std::isfinite(transition.activation_weight) ||
      !std::isfinite(transition.parameter) ||
      !std::isfinite(transition.secondary_parameter) ||
      !std::isfinite(transition.tertiary_parameter) ||
      (transition.secondary_parameter < 0.0 &&
       transition.distribution != StochasticMapDistribution::kUniform) ||
      transition.tertiary_parameter < 0.0) {
    throw std::invalid_argument(
        "Monte Carlo stochastic_map record values must be finite with non-negative auxiliary parameters");
  }
  switch (transition.timing) {
    case StochasticMapTiming::kTimed:
    case StochasticMapTiming::kImmediate:
      break;
    default:
      throw std::invalid_argument(
          "Monte Carlo stochastic_map record timing is invalid");
  }
  switch (transition.distribution) {
    case StochasticMapDistribution::kNormal:
      break;
    case StochasticMapDistribution::kDeterministic:
      if (transition.parameter < 0.0) {
        throw std::invalid_argument(
            "Monte Carlo stochastic_map duration mean must be non-negative");
      }
      break;
    case StochasticMapDistribution::kExponential:
      if (transition.parameter < 0.0 ||
          (transition.timing == StochasticMapTiming::kTimed &&
           transition.parameter <= 0.0)) {
        throw std::invalid_argument(
            "Monte Carlo stochastic_map exponential rate is invalid");
      }
      break;
    case StochasticMapDistribution::kUniform:
      if (transition.secondary_parameter < transition.parameter) {
        throw std::invalid_argument(
            "Monte Carlo stochastic_map uniform bounds are invalid");
      }
      break;
    case StochasticMapDistribution::kLogNormal:
      // parameter is mu and secondary_parameter is sigma. Mu may be negative,
      // while sigma remains a finite non-negative scale parameter.
      if (transition.secondary_parameter < 0.0) {
        throw std::invalid_argument(
            "Monte Carlo stochastic_map lognormal sigma must be non-negative");
      }
      break;
    case StochasticMapDistribution::kGamma:
      // parameter is scale and secondary_parameter is shape.
      if (transition.parameter < 0.0 ||
          transition.secondary_parameter <= 0.0) {
        throw std::invalid_argument(
            "Monte Carlo stochastic_map gamma requires nonnegative scale and positive shape");
      }
      break;
    case StochasticMapDistribution::kConstant0:
    case StochasticMapDistribution::kImmediate:
      if (transition.parameter != 0.0 ||
          transition.secondary_parameter != 0.0 ||
          transition.tertiary_parameter != 0.0) {
        throw std::invalid_argument(
            "Monte Carlo stochastic_map immediate records must have zero parameters");
      }
      break;
    default:
      throw std::invalid_argument(
          "Monte Carlo stochastic_map record distribution is invalid");
  }
}

void validate_stochastic_map_sparse(const StochasticMap& map,
                                    std::size_t transition_count) {
  std::vector<bool> seen(transition_count, false);
  for (const auto& transition : map.transitions) {
    validate_stochastic_map_record(transition, transition_count);
    if (seen[transition.transition]) {
      throw std::invalid_argument(
          "Monte Carlo stochastic_map contains duplicate transition records");
    }
    seen[transition.transition] = true;
  }
}

[[nodiscard]] MonteCarloDuration duration_from_stochastic_map(
    const StochasticMapTransition& transition) {
  if (transition.timing == StochasticMapTiming::kImmediate ||
      transition.distribution == StochasticMapDistribution::kConstant0 ||
      transition.distribution == StochasticMapDistribution::kImmediate) {
    return {MonteCarloDurationDistribution::kImmediate, 0.0};
  }
  switch (transition.distribution) {
    case StochasticMapDistribution::kDeterministic:
      return {MonteCarloDurationDistribution::kDeterministic,
              transition.parameter};
    case StochasticMapDistribution::kExponential:
      return {MonteCarloDurationDistribution::kExponential,
              transition.parameter == 0.0 ? 0.0
                                          : 1.0 / transition.parameter};
    case StochasticMapDistribution::kNormal:
      return {MonteCarloDurationDistribution::kNormal, transition.parameter,
              transition.secondary_parameter};
    case StochasticMapDistribution::kUniform:
      if (transition.parameter < 0.0) {
        if (transition.secondary_parameter <= 0.0)
          throw std::invalid_argument(
              "Monte Carlo uniform duration must have nonnegative conditional support");
        // Map parameters are endpoints. Condition before converting to a
        // width, preserving a small positive upper endpoint and avoiding
        // overflow when the original interval spans very wide signed bounds.
        return {MonteCarloDurationDistribution::kUniform, 0.0,
                transition.secondary_parameter};
      }
      return {MonteCarloDurationDistribution::kUniform, transition.parameter,
              transition.secondary_parameter - transition.parameter};
    case StochasticMapDistribution::kLogNormal:
      // The native representation stores (shape, loc, scale), while map
      // records carry (mu, sigma). scipy-style scale is exp(mu).
      if (!std::isfinite(std::exp(transition.parameter))) {
        throw std::invalid_argument(
            "Monte Carlo stochastic_map lognormal scale is not finite");
      }
      if (transition.secondary_parameter == 0.0) {
        return {MonteCarloDurationDistribution::kDeterministic,
                std::exp(transition.parameter)};
      }
      return {MonteCarloDurationDistribution::kLogNormal,
              transition.secondary_parameter, 0.0,
              std::exp(transition.parameter)};
    case StochasticMapDistribution::kGamma:
      // The native representation stores (shape, loc, scale), while map
      // records carry (scale, shape).
      return {MonteCarloDurationDistribution::kGamma,
              transition.secondary_parameter, 0.0, transition.parameter};
    case StochasticMapDistribution::kConstant0:
    case StochasticMapDistribution::kImmediate:
      return {MonteCarloDurationDistribution::kImmediate, 0.0};
    default:
      throw std::invalid_argument(
          "Monte Carlo stochastic_map record distribution is invalid");
  }
}

[[nodiscard]] Marking resolve_marking(const std::vector<std::uint32_t>& source,
                                      std::size_t place_count,
                                      std::size_t default_place,
                                      const char* name) {
  Marking result;
  if (source.empty()) {
    result.assign(place_count, 0);
    result[default_place] = 1;
    return result;
  }
  if (source.size() != place_count) {
    throw std::invalid_argument(std::string("Monte Carlo ") + name +
                                " marking must match the place count");
  }
  result.reserve(source.size());
  for (const auto value : source) result.push_back(value);
  return result;
}

[[nodiscard]] std::vector<bool> resolve_marking_keys(
    const Marking& marking, const std::vector<bool>& supplied, const char* name) {
  if (!supplied.empty() && supplied.size() != marking.size())
    throw std::invalid_argument(std::string("Monte Carlo ") + name +
                                " marking keys must match the place count");
  auto keys = supplied.empty() ? std::vector<bool>(marking.size(), false) : supplied;
  for (std::size_t place = 0; place < marking.size(); ++place) {
    if (supplied.empty()) keys[place] = marking[place] != 0;
    else if (marking[place] != 0 && !keys[place])
      throw std::invalid_argument(std::string("Monte Carlo ") + name +
                                  " positive marking must have a key");
  }
  return keys;
}

void validate_scheduler_budgets(const PetriNet& net,
                                const MonteCarloOptions& options,
                                const Marking& initial_marking,
                                const std::vector<std::size_t>& capacities) {
  if (options.simulations > kMaxMonteCarloSimulations) {
    throw std::invalid_argument(
        "Monte Carlo simulation count exceeds the native safety limit");
  }
  if (options.max_steps > kMaxMonteCarloSteps) {
    throw std::invalid_argument(
        "Monte Carlo max_steps exceeds the native safety limit");
  }
  if (capacities.size() != net.place_count()) {
    throw std::invalid_argument("Monte Carlo resource capacity vector is invalid");
  }
  // Case markings remain dense even when capacities are zero or initial
  // token counts are sparse. Bound those allocations independently.
  if (net.place_count() > kMaxMonteCarloRuntimeAssignments ||
      (options.simulations != 0 && net.place_count() >
          kMaxMonteCarloRuntimeAssignments / options.simulations)) {
    throw std::invalid_argument(
        "Monte Carlo case marking allocation exceeds the native safety limit");
  }

  std::size_t total_resources = 0;
  for (const auto capacity : capacities) {
    if (capacity > kMaxMonteCarloResourceCapacity) {
      throw std::invalid_argument(
          "Monte Carlo resource capacity exceeds the native safety limit");
    }
    if (total_resources > kMaxMonteCarloResourceSlots - capacity) {
      throw std::invalid_argument(
          "Monte Carlo total resource capacity exceeds the native safety limit");
    }
    total_resources += capacity;
  }

  std::size_t initial_tokens = 0;
  for (const auto tokens : initial_marking) {
    if (tokens > kMaxMonteCarloInitialTokens ||
        initial_tokens > kMaxMonteCarloInitialTokens - tokens) {
      throw std::invalid_argument(
          "Monte Carlo initial marking exceeds the native safety limit");
    }
    initial_tokens += static_cast<std::size_t>(tokens);
  }
  if (options.max_steps != 0 && options.simulations != 0 &&
      options.simulations >
          kMaxMonteCarloSimulationSteps / options.max_steps) {
    throw std::invalid_argument(
        "Monte Carlo simulation step budget exceeds the native safety limit");
  }
}

void validate_conditional_duration(const MonteCarloDuration& duration) {
  if (duration.distribution == MonteCarloDurationDistribution::kNormal) {
    if (duration.secondary_seconds < 0.0 ||
        (duration.value_seconds < 0.0 && duration.secondary_seconds == 0.0))
      throw std::invalid_argument(
          "Monte Carlo normal duration must have nonnegative conditional support");
  } else if (duration.distribution == MonteCarloDurationDistribution::kUniform) {
    const double upper = duration.value_seconds + duration.secondary_seconds;
    if (duration.secondary_seconds < 0.0 || !std::isfinite(upper) ||
        (duration.value_seconds < 0.0 && upper <= 0.0))
      throw std::invalid_argument(
          "Monte Carlo uniform duration must have finite nonnegative conditional support");
  }
}

PreparedOptions prepare_options(const PetriNet& net,
                                const MonteCarloOptions& options) {
  validate_selectors(options);
  validate_arrival_aliases(options);
  if (!std::isfinite(options.start) ||
      !std::isfinite(options.default_duration_seconds) ||
      options.default_duration_seconds < 0.0) {
    throw std::invalid_argument(
        "Monte Carlo time values must be finite and non-negative");
  }
  if (!std::isfinite(options.small_scale_factor) ||
      options.small_scale_factor <= 0.0 ||
      !std::isfinite(options.max_thread_execution_time) ||
      options.max_thread_execution_time <= 0.0 ||
      options.diagnostic_interval == 0) {
    throw std::invalid_argument(
        "Monte Carlo compatibility controls must be finite and positive");
  }
  if (options.default_resources_per_place > kMaxMonteCarloResourceCapacity) {
    throw std::invalid_argument(
        "Monte Carlo default resource capacity exceeds the native safety limit");
  }
  validate_force_distribution(options);

  const auto transition_count = net.transitions().size();
  if (!options.transition_weights.empty() &&
      options.transition_weights.size() != transition_count) {
    throw std::invalid_argument(
        "Monte Carlo transition_weights must be empty or match the transition count");
  }
  for (const double weight : options.transition_weights) {
    if (!std::isfinite(weight)) {
      throw std::invalid_argument(
          "Monte Carlo transition_weights must be finite");
    }
  }
  if (!options.transition_durations.empty() &&
      options.transition_durations.size() != transition_count) {
    throw std::invalid_argument(
        "Monte Carlo transition_durations must be empty or match the transition count");
  }
  for (const auto& duration : options.transition_durations) {
    if (!std::isfinite(duration.value_seconds) ||
        !std::isfinite(duration.secondary_seconds) ||
        !std::isfinite(duration.tertiary_seconds)) {
      throw std::invalid_argument(
          "Monte Carlo transition duration parameters must be finite");
    }
    switch (duration.distribution) {
      case MonteCarloDurationDistribution::kDeterministic:
        if (duration.value_seconds < 0.0) {
          throw std::invalid_argument(
              "Monte Carlo transition duration must be non-negative");
        }
        break;
      case MonteCarloDurationDistribution::kExponential:
        if (duration.value_seconds < 0.0 ||
            (duration.value_seconds == 0.0 && duration.secondary_seconds < 0.0))
          throw std::invalid_argument(
              "Monte Carlo exponential requires nonnegative scale and conditional support");
        break;
      case MonteCarloDurationDistribution::kConstant0:
      case MonteCarloDurationDistribution::kImmediate:
        // These distributions have no numeric parameter and always return 0.
        break;
      case MonteCarloDurationDistribution::kNormal:
      case MonteCarloDurationDistribution::kUniform:
        validate_conditional_duration(duration);
        break;
      case MonteCarloDurationDistribution::kLogNormal:
      case MonteCarloDurationDistribution::kGamma:
        if (duration.value_seconds <= 0.0 || duration.tertiary_seconds < 0.0 ||
            (duration.tertiary_seconds == 0.0 && duration.secondary_seconds < 0.0))
          throw std::invalid_argument(
              "Monte Carlo lognormal/gamma requires positive shape and nonnegative conditional support");
        break;
      default:
        throw std::invalid_argument("Monte Carlo duration distribution is invalid");
    }
  }

  PreparedOptions prepared;
  prepared.initial_marking = resolve_marking(
      options.initial_marking, net.place_count(), net.source(), "initial");
  prepared.final_marking = resolve_marking(
      options.final_marking, net.place_count(), net.sink(), "final");
  prepared.initial_keys = resolve_marking_keys(prepared.initial_marking,
                                               options.initial_marking_keys, "initial");
  prepared.final_keys = resolve_marking_keys(prepared.final_marking,
                                             options.final_marking_keys, "final");
  prepared.weights.assign(transition_count, 1.0);
  prepared.durations.assign(
      transition_count,
      MonteCarloDuration{MonteCarloDurationDistribution::kDeterministic,
                         options.default_duration_seconds});
  if (options.stochastic_map) {
    validate_stochastic_map_sparse(*options.stochastic_map, transition_count);
    std::fill(prepared.weights.begin(), prepared.weights.end(), 1.0);
    std::fill(prepared.durations.begin(), prepared.durations.end(),
              MonteCarloDuration{MonteCarloDurationDistribution::kImmediate,
                                 0.0});
    for (const auto& transition : options.stochastic_map->transitions) {
      prepared.weights[transition.transition] = transition.activation_weight;
      // Complete explicit duration vectors override map timing, including
      // conversion and runtime domain checks for those unused durations.
      if (!options.transition_durations.empty()) continue;
      const auto duration = duration_from_stochastic_map(transition);
      if (!std::isfinite(duration.value_seconds) ||
          !std::isfinite(duration.secondary_seconds) ||
          !std::isfinite(duration.tertiary_seconds)) {
        throw std::invalid_argument(
            "Monte Carlo stochastic_map duration conversion is not finite");
      }
      validate_conditional_duration(duration);
      prepared.durations[transition.transition] = duration;
    }
  }
  if (!options.transition_weights.empty())
    prepared.weights = options.transition_weights;
  if (!options.transition_durations.empty())
    prepared.durations = options.transition_durations;

  prepared.capacities.assign(net.place_count(),
                             std::max<std::size_t>(1, options.default_resources_per_place));
  for (const auto& [place, capacity] : options.resources_per_place) {
    if (place >= net.place_count()) {
      throw std::invalid_argument(
          "Monte Carlo resource place index is invalid");
    }
    if (capacity > kMaxMonteCarloResourceCapacity) {
      throw std::invalid_argument(
          "Monte Carlo resource capacity exceeds the native safety limit");
    }
    prepared.capacities[place] = std::max<std::size_t>(1, capacity);
  }
  validate_scheduler_budgets(net, options, prepared.initial_marking,
                             prepared.capacities);
  return prepared;
}

[[nodiscard]] std::vector<std::size_t> enabled_transitions(
    const PetriNet& net, const Marking& marking) {
  std::vector<std::size_t> enabled;
  enabled.reserve(net.transitions().size());
  for (std::size_t index = 0; index < net.transitions().size(); ++index) {
    const auto& transition = net.transitions()[index];
    if (petri_transition_enabled(transition, marking)) enabled.push_back(index);
  }
  return enabled;
}

[[nodiscard]] std::size_t choose_transition(
    const std::vector<std::size_t>& enabled,
    const std::vector<double>& weights,
    std::mt19937_64& rng) {
  if (enabled.empty()) throw std::invalid_argument("no enabled transitions");
  double total = 0.0;
  for (const auto index : enabled) total += weights[index];
  if (!std::isfinite(total)) {
    throw std::invalid_argument(
        "Monte Carlo enabled transition weight sum must be finite");
  }
  if (total == 0.0) {
    return enabled[portable::below(rng, enabled.size())];
  }
  std::vector<double> scaled;
  scaled.reserve(enabled.size());
  for (const auto index : enabled) {
    const double probability = weights[index] / total;
    if (!std::isfinite(probability) || probability < 0.0) {
      throw std::invalid_argument(
          "Monte Carlo enabled transition probabilities must be finite and non-negative");
    }
    scaled.push_back(probability);
  }
  const auto picked = portable::weighted_index(rng, scaled);
  return enabled[picked];
}

// numpy.random.choice over the enabled transitions with probabilities
// weight / total (1 / n for a zero total): one uniform against the
// cumulative sum normalized by its last entry.
[[nodiscard]] std::size_t choose_transition_numpy(
    std::vector<std::size_t> enabled, const std::vector<double>& weights,
    const std::vector<std::size_t>& rank, rng::NumpyRandomState& rng) {
  if (enabled.empty()) throw std::invalid_argument("no enabled transitions");
  std::stable_sort(enabled.begin(), enabled.end(),
                   [&](std::size_t left, std::size_t right) { return rank[left] < rank[right]; });
  double total = 0.0;
  for (const auto index : enabled) total += weights[index];
  if (!std::isfinite(total)) {
    throw std::invalid_argument(
        "Monte Carlo enabled transition weight sum must be finite");
  }
  std::vector<double> cumulative;
  cumulative.reserve(enabled.size());
  double sum = 0.0;
  for (const auto index : enabled) {
    const double probability = total == 0.0
        ? 1.0 / static_cast<double>(enabled.size()) : weights[index] / total;
    if (!(probability >= 0.0)) {
      throw std::invalid_argument(
          "Monte Carlo enabled transition probabilities must be finite and non-negative");
    }
    sum += probability;
    cumulative.push_back(sum);
  }
  if (std::fabs(sum - 1.0) > std::sqrt(std::numeric_limits<double>::epsilon()))
    throw std::invalid_argument("Monte Carlo enabled transition probabilities do not sum to 1");
  const double last = cumulative.back();
  for (auto& value : cumulative) value /= last;
  const double draw = rng.random_sample();
  const auto picked = static_cast<std::size_t>(
      std::upper_bound(cumulative.begin(), cumulative.end(), draw) - cumulative.begin());
  return enabled[std::min(picked, enabled.size() - 1)];
}

// The reference service-time laws on numpy's legacy draws, redrawn while
// negative.
[[nodiscard]] double sample_duration_numpy(const MonteCarloDuration& duration,
                                           rng::NumpyRandomState& rng) {
  const auto draw = [&]() -> double {
    switch (duration.distribution) {
      case MonteCarloDurationDistribution::kDeterministic:
        return duration.value_seconds;
      case MonteCarloDurationDistribution::kConstant0:
      case MonteCarloDurationDistribution::kImmediate:
        return 0.0;
      case MonteCarloDurationDistribution::kExponential:
        return rng.standard_exponential() * duration.value_seconds + duration.secondary_seconds;
      case MonteCarloDurationDistribution::kNormal:
        if (duration.secondary_seconds == 0.0) return duration.value_seconds;
        return duration.value_seconds + duration.secondary_seconds * rng.gauss();
      case MonteCarloDurationDistribution::kUniform:
        return rng.random_sample() * duration.secondary_seconds + duration.value_seconds;
      case MonteCarloDurationDistribution::kLogNormal:
        return std::exp(duration.value_seconds * rng.gauss()) * duration.tertiary_seconds +
               duration.secondary_seconds;
      case MonteCarloDurationDistribution::kGamma:
        return rng.standard_gamma(duration.value_seconds) * duration.tertiary_seconds +
               duration.secondary_seconds;
      default:
        throw std::invalid_argument("Monte Carlo duration distribution is invalid");
    }
  };
  for (std::size_t attempt = 0; attempt < 10000; ++attempt) {
    const double value = draw();
    if (!(value < 0.0)) return value;
  }
  throw std::runtime_error("Monte Carlo rejection sampling budget exceeded");
}

// Robert (2009), Proposition 2.3: https://arxiv.org/pdf/0907.4010.
// For Z >= a >= 0, propose Z=a+E/alpha, E~Exp(1), where
// alpha=(a+hypot(a,2))/2, accepting exp(-(Z-alpha)^2/2).
// alpha-a=1/alpha avoids cancellation: Z-alpha=(E-1)/alpha,
// and physical duration is sigma*E/alpha, never mu+sigma*Z.
[[nodiscard]] double sample_negative_mean_normal(double mu, double sigma,
                                                  std::mt19937_64& rng,
                                                  double* log_draw = nullptr) {
  const double a = (-mu) / sigma;
  double inverse_alpha;
  if (std::isfinite(a)) {
    const double half = a * 0.5;
    inverse_alpha = 1.0 / (half + std::hypot(half, 1.0));
  } else {
    // a exceeds double range. The correction alpha-a is below representable
    // relative precision; compute the reciprocal without the overflowing a.
    inverse_alpha = sigma / (-mu);
  }
  for (std::size_t attempt = 0; attempt < 10000; ++attempt) {
    const double exponential = -std::log1p(-portable::unit(rng));
    const double difference = (exponential - 1.0) * inverse_alpha;
    const double acceptance = -std::log1p(-portable::unit(rng));
    if (acceptance >= 0.5 * difference * difference) {
      // Multiply in exponent/mantissa form so a subnormal intermediate scale
      // cannot lose precision before multiplication by the exponential draw.
      int sigma_exponent = 0, inverse_exponent = 0, draw_exponent = 0;
      const double mantissa = std::frexp(sigma, &sigma_exponent) *
          std::frexp(inverse_alpha, &inverse_exponent) *
          std::frexp(exponential, &draw_exponent);
      const double draw = std::scalbn(mantissa,
          sigma_exponent + inverse_exponent + draw_exponent);
      if (!std::isfinite(draw))
        throw std::runtime_error("Monte Carlo normal sample exceeds finite duration range");
      if (log_draw != nullptr) {
        const double log_inverse = inverse_alpha > 0.0 ? std::log(inverse_alpha)
            : std::log(sigma) - std::log(-mu);
        *log_draw = std::log(sigma) + log_inverse + std::log(exponential);
      }
      // An excess smaller than the representable double range rounds to zero.
      return draw;
    }
  }
  throw std::runtime_error("Monte Carlo normal rejection sampling budget exceeded");
}

[[nodiscard]] double unit_exponential(std::mt19937_64& rng) {
  return -std::log1p(-portable::unit(rng));
}

[[nodiscard]] double log_positive_ratio(double numerator, double denominator) {
  const double ratio = numerator / denominator;
  if (ratio >= 0.5 && ratio <= 2.0)
    return std::log1p((numerator - denominator) / denominator);
  return std::log(numerator) - std::log(denominator);
}

// Compute b*(exp(t)-1) from log(t), preserving tiny t whose product with b
// is representable, and large t whose exponential alone would overflow.
[[nodiscard]] double physical_excess(double b, double log_t) {
  if (log_t == -std::numeric_limits<double>::infinity()) return 0.0;
  if (log_t > std::log(2048.0))
    throw std::runtime_error("Monte Carlo conditional sample exceeds finite duration range");
  const double t = std::exp(log_t);
  const double log_expm1 = t < 1e-5
      ? log_t + std::log1p(t * (0.5 + t / 6.0))
      : (t > 50.0 ? t + std::log1p(-std::exp(-t)) : std::log(std::expm1(t)));
  const double result = std::exp(std::log(b) + log_expm1);
  if (!std::isfinite(result))
    throw std::runtime_error("Monte Carlo conditional sample exceeds finite duration range");
  return result;
}

[[nodiscard]] double sample_shifted_lognormal(double shape, double location,
                                              double scale, std::mt19937_64& rng) {
  const double b = -location;
  const double threshold = log_positive_ratio(b, scale);
  double log_excess;
  if (threshold > 0.0) {
    (void)sample_negative_mean_normal(-threshold, shape, rng, &log_excess);
  } else {
    bool accepted = false;
    for (std::size_t attempt = 0; attempt < 10000; ++attempt) {
      const double z = portable::normal(rng, 0.0, 1.0);
      if (threshold == 0.0) {
        if (z < 0.0) continue;
        log_excess = std::log(shape) + std::log(z);
      } else {
        const double excess = std::fma(shape, z, -threshold);
        if (excess < 0.0) continue;
        log_excess = std::log(excess);
      }
      accepted = true;
      break;
    }
    if (!accepted)
      throw std::runtime_error("Monte Carlo lognormal rejection sampling budget exceeded");
  }
  return physical_excess(b, log_excess);
}

[[nodiscard]] double sample_shifted_gamma(double shape, double location,
                                          double scale, std::mt19937_64& rng) {
  const double b = -location;
  const double threshold = b / scale;
  // At or below mean+sd, ordinary gamma rejection retains substantial mass.
  if (shape >= 1.0 && threshold <= shape + std::sqrt(shape)) {
    for (std::size_t attempt = 0; attempt < 10000; ++attempt) {
      const double result = std::fma(scale, portable::gamma(rng, shape, 1.0), -b);
      if (result >= 0.0) return result;
    }
  } else if (shape < 1.0 && threshold < 1.0) {
    // Envelope x^(shape-1) on [a,1], exp(-x) on [1,infinity).
    // On the bounded part acceptance is exp(-x)>=exp(-1); the tail
    // accepts x^(shape-1). Its mixture avoids rare tiny-shape survival.
    const double log_a = log_positive_ratio(b, scale);
    const double q = -shape * log_a;
    const double body = -log_a * (q == 0.0 ? 1.0 : -std::expm1(-q) / q);
    const double body_probability = body / (body + std::exp(-1.0));
    for (std::size_t attempt = 0; attempt < 10000; ++attempt) {
      if (portable::unit(rng) < body_probability) {
        const double u = portable::unit(rng);
        double delta;
        if (q < 1e-12) {
          delta = -log_a * (u + 0.5 * u * (1.0 - u) * q);
        } else if (q < 0.5) {
          delta = std::log1p(u * std::expm1(q)) / shape;
        } else {
          const double log_x = std::log1p(-(1.0 - u) * (-std::expm1(-q))) / shape;
          delta = log_x - log_a;
        }
        const double x = std::exp(log_a + delta);
        if (unit_exponential(rng) >= x)
          return physical_excess(b, std::log(std::max(0.0, delta)));
      } else {
        const double excess = unit_exponential(rng);
        if (unit_exponential(rng) >= (1.0 - shape) * std::log1p(excess))
          return std::fma(scale, excess, scale - b);
      }
    }
  } else {
    // For shape>=1 the density is log-concave: its tangent at a gives
    // an exponential envelope. Farther than mean+sd its rate is well
    // separated from zero. For shape<1,a>=1 use rate one instead.
    const double rate = shape < 1.0 ? 1.0
        : std::fma(-(shape - 1.0), scale, b) / b;
    if (!(rate > 0.0) || !std::isfinite(rate))
      throw std::runtime_error("Monte Carlo gamma conditional rate is not representable");
    for (std::size_t attempt = 0; attempt < 10000; ++attempt) {
      const double excess = unit_exponential(rng) / rate;
      const double ratio = excess * (scale / b);
      double log_acceptance;
      if (shape < 1.0) log_acceptance = (shape - 1.0) * std::log1p(ratio);
      else if (ratio < 1e-3)
        log_acceptance = ((shape - 1.0) * ratio) * ratio *
            (-0.5 + ratio * (1.0/3.0 + ratio * (-0.25 + ratio * 0.2)));
      else log_acceptance = (shape - 1.0) * (std::log1p(ratio) - ratio);
      if (unit_exponential(rng) >= -log_acceptance) return scale * excess;
    }
  }
  throw std::runtime_error("Monte Carlo gamma rejection sampling budget exceeded");
}

[[nodiscard]] double sample_duration(const MonteCarloDuration& duration,
                                     std::mt19937_64& rng) {
  switch (duration.distribution) {
    case MonteCarloDurationDistribution::kDeterministic:
      return duration.value_seconds;
    case MonteCarloDurationDistribution::kExponential: {
      if (duration.value_seconds == 0.0) return duration.secondary_seconds;
      // Memorylessness removes a negative location after conditioning on
      // nonnegative service time. This avoids rare-event rejection and
      // cancellation even when the location is far into the exponential tail.
      const double rate = 1.0 / duration.value_seconds;
      const double draw = std::isfinite(rate)
          ? portable::exponential(rng, rate)
          : duration.value_seconds * unit_exponential(rng);
      return std::max(0.0, duration.secondary_seconds) + draw;
    }
    case MonteCarloDurationDistribution::kConstant0:
    case MonteCarloDurationDistribution::kImmediate:
      return 0.0;
    case MonteCarloDurationDistribution::kNormal: {
      if (duration.secondary_seconds == 0.0) return duration.value_seconds;
      if (duration.value_seconds < 0.0)
        return sample_negative_mean_normal(duration.value_seconds,
                                            duration.secondary_seconds, rng);
      for (std::size_t attempt = 0; attempt < 10000; ++attempt) {
        const double draw = portable::normal(rng, duration.value_seconds,
                                             duration.secondary_seconds);
        if (!(draw < 0.0)) return draw;
      }
      throw std::runtime_error("Monte Carlo normal rejection sampling budget exceeded");
    }
    case MonteCarloDurationDistribution::kUniform:
      if (duration.secondary_seconds == 0.0) return duration.value_seconds;
      return portable::uniform_real(
          rng, std::max(0.0, duration.value_seconds),
          duration.value_seconds + duration.secondary_seconds);
    case MonteCarloDurationDistribution::kLogNormal: {
      if (duration.tertiary_seconds == 0.0) return duration.secondary_seconds;
      if (duration.secondary_seconds < 0.0)
        return sample_shifted_lognormal(duration.value_seconds, duration.secondary_seconds,
                                        duration.tertiary_seconds, rng);
      const double draw = portable::lognormal(rng, 0.0, duration.value_seconds);
      return duration.secondary_seconds + duration.tertiary_seconds * draw;
    }
    case MonteCarloDurationDistribution::kGamma:
      if (duration.tertiary_seconds == 0.0) return duration.secondary_seconds;
      if (duration.secondary_seconds < 0.0)
        return sample_shifted_gamma(duration.value_seconds, duration.secondary_seconds,
                                    duration.tertiary_seconds, rng);
      return duration.secondary_seconds +
             portable::gamma(rng, duration.value_seconds,
                             duration.tertiary_seconds);
    default:
      throw std::invalid_argument("Monte Carlo duration distribution is invalid");
  }
}

void append_interval(std::vector<MonteCarloInterval>& intervals,
                     double start, double end, RuntimeBudget& budget) {
  if (!std::isfinite(start) || !std::isfinite(end)) {
    throw std::runtime_error("Monte Carlo interval has invalid finite bounds");
  }
  // Shared reservation timestamps may exceed this case's nominal finish.
  // Such intervals are absent from the public result, as are zero spans.
  if (end <= start) return;
  if (budget.interval_records >= kMaxMonteCarloRuntimeIntervals) {
    throw std::runtime_error(
        "Monte Carlo interval record budget exceeded");
  }
  intervals.push_back({start, end});
  ++budget.interval_records;
}

// Initial arrivals append directly, while subsequent operations use the
// observed binary min-heap operations. The initial append can temporarily
// violate heap order, so std::pop_heap's heap precondition is inappropriate.
void push_timestamp(std::vector<double>& values, double value,
                    RuntimeBudget& budget, bool initial = false) {
  if (budget.live_assignments >= kMaxMonteCarloRuntimeAssignments)
    throw std::runtime_error("Monte Carlo live assignment budget exceeded");
  values.push_back(value);
  ++budget.live_assignments;
  if (initial) return;
  std::size_t position = values.size() - 1;
  while (position > 0) {
    const auto parent = (position - 1) / 2;
    if (!(value < values[parent])) break;
    values[position] = values[parent];
    position = parent;
  }
  values[position] = value;
}

double pop_timestamp(std::vector<double>& values, RuntimeBudget& budget) {
  const double result = values.front();
  const double last = values.back();
  values.pop_back();
  --budget.live_assignments;
  if (values.empty()) return result;
  std::size_t position = 0;
  while (2 * position + 1 < values.size()) {
    auto child = 2 * position + 1;
    if (child + 1 < values.size() && !(values[child] < values[child + 1]))
      ++child;
    values[position] = values[child];
    position = child;
  }
  while (position > 0) {
    const auto parent = (position - 1) / 2;
    if (!(last < values[parent])) break;
    values[position] = values[parent];
    position = parent;
  }
  values[position] = last;
  return result;
}

// Fires like the reference's semantics.execute: each ordinary arc moves its
// weight (an imported PNML keeps weighted and parallel arcs as weighted
// occurrences, see parse_pnml_document), a place whose count reaches zero
// loses its marking key, and every output place gets one.
void fire_transition(const PetriTransition& transition,
                     Marking& marking, std::vector<bool>& keys) {
  if (!petri_transition_enabled(transition, marking))
    throw std::runtime_error("Monte Carlo fired a disabled transition");
  for (const auto place : transition.inputs)
    if (place >= marking.size()) throw std::runtime_error("Monte Carlo fired a disabled transition");
  for (const auto place : transition.reset_inputs)
    if (place >= marking.size()) throw std::runtime_error("Monte Carlo reset place is invalid");
  for (const auto place : transition.outputs)
    if (place >= marking.size()) throw std::overflow_error("Monte Carlo marking token overflow");
  if (!transition.has_compact_ordinary_arcs()) {
    std::vector<std::uint64_t> required(marking.size(), 0);
    for (const auto place : transition.inputs) {
      if (required[place] == std::numeric_limits<std::uint64_t>::max())
        throw std::runtime_error("Monte Carlo input multiplicity overflow");
      ++required[place];
    }
    for (std::size_t place = 0; place < marking.size(); ++place) {
      if (required[place] > marking[place])
        throw std::runtime_error("Monte Carlo fired a disabled transition");
      marking[place] -= required[place];
      if (required[place] != 0 && marking[place] == 0) keys[place] = false;
    }
    for (const auto place : transition.reset_inputs) {
      marking[place] = 0;
      keys[place] = false;
    }
    for (const auto place : transition.outputs) {
      if (marking[place] == std::numeric_limits<std::uint64_t>::max())
        throw std::overflow_error("Monte Carlo marking token overflow");
      ++marking[place];
      keys[place] = true;
    }
    return;
  }
  try {
    fire_petri_transition(transition, marking);
  } catch (const std::overflow_error&) {
    throw std::overflow_error("Monte Carlo marking token overflow");
  }
  for (const auto place : transition.inputs)
    if (marking[place] == 0) keys[place] = false;
  for (const auto place : transition.reset_inputs) keys[place] = false;
  for (const auto place : transition.outputs) keys[place] = true;
}

void finalize_case(CaseState& state, MonteCarloDiagnostics& diagnostics,
                   const MonteCarloOptions& options,
                   std::vector<double>* execution_by_case) {
  state.finished = true;
  if (state.completed) {
    ++diagnostics.completed_cases;
  } else if (state.step_limited) {
    ++diagnostics.step_limited_cases;
  } else if (state.deadlocked) {
    ++diagnostics.deadlocked_cases;
  }
  if (options.enable_diagnostics) {
    const double execution = std::max(0.0, state.now - state.arrival);
    if (execution_by_case != nullptr &&
        state.index < execution_by_case->size()) {
      (*execution_by_case)[state.index] = execution;
    }
  }
}

[[nodiscard]] double median(std::vector<double> values) {
  if (values.empty()) return 0.0;
  std::sort(values.begin(), values.end());
  const auto middle = values.size() / 2;
  if (values.size() % 2 == 1) return values[middle];
  return (values[middle - 1] + values[middle]) / 2.0;
}

[[nodiscard]] std::optional<std::string> selected_timestamp_value(
    const Event& event, std::string_view key) {
  if (is_timestamp_alias(key)) return event.timestamp;
  if (const auto* value = event.attribute(key)) return *value;
  return std::nullopt;
}

[[nodiscard]] double infer_case_arrival_seconds(
    const EventLog& input_log, std::string_view timestamp_key) {
  std::vector<double> first_event_times;
  first_event_times.reserve(input_log.traces.size());
  for (const auto& trace : input_log.traces) {
    if (trace.events.empty()) continue;
    const auto timestamp =
        selected_timestamp_value(trace.events.front(), timestamp_key);
    if (!timestamp) continue;
    const auto parsed = parse_timestamp(*timestamp);
    if (parsed && std::isfinite(*parsed)) first_event_times.push_back(*parsed);
  }
  if (first_event_times.size() < 2) return 0.0;
  std::sort(first_event_times.begin(), first_event_times.end());
  long double total = 0.0L;
  for (std::size_t i = 1; i < first_event_times.size(); ++i) {
    const double difference = first_event_times[i] - first_event_times[i - 1];
    if (!std::isfinite(difference) || difference < 0.0) return 0.0;
    total += static_cast<long double>(difference);
  }
  const long double mean =
      total / static_cast<long double>(first_event_times.size() - 1);
  const double result = static_cast<double>(mean);
  return std::isfinite(result) && result >= 0.0 ? result : 0.0;
}

MonteCarloResult run_monte_carlo(const PetriNet& net,
                                 const MonteCarloOptions& options) {
  validate_net(net);
  const auto prepared = prepare_options(net, options);
  const double arrival_seconds = effective_arrival_seconds(options);

  MonteCarloResult result;
  result.diagnostics.simulated_cases = options.simulations;
  result.diagnostics.case_arrival_seconds = arrival_seconds;
  result.diagnostics.case_arrival_ratio = arrival_seconds;
  result.log.traces.reserve(options.simulations);
  result.output_places_interval_trees.resize(net.place_count());
  for (std::size_t place = 0; place < net.place_count(); ++place) {
    result.output_places_interval_trees[place].place_index = place;
  }
  result.output_transitions_interval_trees.resize(net.transitions().size());
  for (std::size_t transition = 0; transition < net.transitions().size();
       ++transition) {
    result.output_transitions_interval_trees[transition].transition_index =
        transition;
  }
  if (options.enable_diagnostics) {
    result.diagnostics.case_execution_seconds.assign(options.simulations, 0.0);
  }
  if (options.simulations == 0)
    throw std::invalid_argument("Monte Carlo requires at least one simulated case");

  std::vector<CaseState> cases;
  cases.reserve(options.simulations);
  std::priority_queue<QueueEntry, std::vector<QueueEntry>, QueueEntryGreater>
      queue;
  RuntimeBudget runtime_budget;
  std::uint64_t serial = 0;
  for (std::size_t index = 0; index < options.simulations; ++index) {
    const double arrival =
        options.start + static_cast<double>(index) * arrival_seconds;
    if (!std::isfinite(arrival))
      throw std::invalid_argument("Monte Carlo case arrival is not finite");
    CaseState state;
    state.index = index;
    state.arrival = arrival;
    state.now = arrival;
    state.marking = prepared.initial_marking;
    state.marking_keys = prepared.initial_keys;
    state.trace.case_id = "case_" + std::to_string(index + 1);
    cases.push_back(std::move(state));
  }

  std::vector<std::vector<double>> timestamps(net.place_count());
  std::vector<std::int64_t> occupied(net.place_count(), 0);
  std::mt19937_64 rng(options.seed);
  std::optional<rng::NumpyRandomState> numpy_rng;
  std::vector<std::size_t> choice_rank(net.transitions().size());
  if (options.numpy_legacy_seed) {
    numpy_rng.emplace(*options.numpy_legacy_seed);
    std::iota(choice_rank.begin(), choice_rank.end(), net.transitions().size());
    for (std::size_t position = 0; position < options.numpy_choice_order.size(); ++position) {
      const auto index = options.numpy_choice_order[position];
      if (index >= choice_rank.size())
        throw std::invalid_argument("Monte Carlo numpy choice order names an unknown transition");
      choice_rank[index] = std::min(choice_rank[index], position);
    }
  }
  auto advance = [&](CaseState& state) {
    if (options.stop_at_final &&
        is_final_marking(state.marking, prepared.final_marking,
                         state.marking_keys, prepared.final_keys)) {
      state.completed = true;
      for (std::size_t place = 0; place < state.marking.size(); ++place)
        if (state.marking_keys[place]) --occupied[place];
      finalize_case(state, result.diagnostics, options,
                    &result.diagnostics.case_execution_seconds);
      return;
    }
    if (state.steps >= options.max_steps) {
      state.step_limited = true;
      finalize_case(state, result.diagnostics, options,
                    &result.diagnostics.case_execution_seconds);
      return;
    }

    const auto enabled = enabled_transitions(net, state.marking);
    if (enabled.empty()) {
      state.deadlocked = true;
      finalize_case(state, result.diagnostics, options,
                    &result.diagnostics.case_execution_seconds);
      return;
    }

    const auto transition_index = numpy_rng
        ? choose_transition_numpy(enabled, prepared.weights, choice_rank, *numpy_rng)
        : choose_transition(enabled, prepared.weights, rng);
    const auto& transition = net.transitions()[transition_index];
    const double duration = numpy_rng
        ? sample_duration_numpy(prepared.durations[transition_index], *numpy_rng)
        : sample_duration(prepared.durations[transition_index], rng);
    if (!std::isfinite(duration) || duration < 0.0) {
      throw std::runtime_error("Monte Carlo sampled an invalid duration");
    }

    double finish = state.now + duration;
    if (!std::isfinite(finish) || finish < state.now)
      throw std::runtime_error("Monte Carlo virtual time overflow");
    // Markings retain arc multiplicity. Shared timing and resource accounting
    // operate once per distinct place, including weighted ordinary arcs.
    std::map<std::size_t, bool> consumed;
    for (const auto place : transition.inputs) consumed[place] = true;
    // Reset arcs retain case-local marking semantics; they must not drain
    // timestamps belonging to the shared scheduler's other cases.
    for (const auto place : transition.reset_inputs)
      if (state.marking[place] != 0) consumed[place] = true;
    std::map<std::size_t, bool> produced;
    for (const auto place : transition.outputs) produced[place] = true;
    double available_time = state.now;
    // Acquire all output resources before consuming inputs. One firing emits
    // one wait interval, even if several output places delay its reservation.
    for (const auto& [place, unused] : produced) {
      (void)unused;
      if (!timestamps[place].empty() &&
          occupied[place] >= static_cast<std::int64_t>(prepared.capacities[place])) {
        available_time = std::max(available_time,
            pop_timestamp(timestamps[place], runtime_budget));
        --occupied[place];
      }
      ++occupied[place];
    }
    append_interval(result.output_transitions_interval_trees[transition_index].intervals,
                    state.now, available_time, runtime_budget);
    // Resource availability bounds the completion timestamp; it does not add
    // another service duration after waiting.
    finish = std::max(finish, available_time);
    for (const auto& [place, unused] : produced) {
      (void)unused;
      push_timestamp(timestamps[place], finish, runtime_budget);
    }
    for (const auto& [place, unused] : consumed) {
      (void)unused;
      --occupied[place];
      if (!timestamps[place].empty()) {
        const double assigned = pop_timestamp(timestamps[place], runtime_budget);
        append_interval(result.output_places_interval_trees[place].intervals,
                        assigned, finish, runtime_budget);
      }
      push_timestamp(timestamps[place], finish, runtime_budget);
    }
    fire_transition(transition, state.marking, state.marking_keys);
    state.now = finish;
    ++state.steps;

    if (!transition.silent()) {
      Event event;
      event.case_id = state.trace.case_id;
      event.activity = net.activities()[*transition.activity];
      event.timestamp = format_timestamp_microseconds(finish);
      event.input_order = state.trace.events.size();
      if (!is_activity_alias(options.activity_key)) {
        event.attributes.push_back(
            {options.activity_key, event.activity, AttributeKind::kString});
      }
      if (!is_timestamp_alias(options.timestamp_key)) {
        event.attributes.push_back(
            {options.timestamp_key, event.timestamp, AttributeKind::kDate});
      }
      state.trace.events.push_back(std::move(event));
    }
    queue.push({state.now, state.index, serial++});
  };
  // First firings are primed in generation order, before any continuation.
  const auto first_initial = std::find(prepared.initial_keys.begin(),
      prepared.initial_keys.end(), true);
  if (first_initial == prepared.initial_keys.end())
    throw std::invalid_argument("Monte Carlo requires an initial marking key");
  for (auto& state : cases) {
    if (first_initial != prepared.initial_keys.end()) {
      const auto place = static_cast<std::size_t>(
          first_initial - prepared.initial_keys.begin());
      push_timestamp(timestamps[place], state.arrival, runtime_budget, true);
      ++occupied[place];
    }
    advance(state);
  }
  while (!queue.empty()) {
    const auto entry = queue.top();
    queue.pop();
    auto& state = cases[entry.case_index];
    if (state.finished) continue;
    state.now = entry.time;
    advance(state);
  }

  // The scheduler finalizes cases in virtual-time order; preserve generation
  // order in the returned event log and aggregate diagnostics.
  double first_arrival = std::numeric_limits<double>::infinity();
  double latest_finish = -std::numeric_limits<double>::infinity();
  for (auto& state : cases) {
    result.log.traces.push_back(std::move(state.trace));
    first_arrival = std::min(first_arrival, state.arrival);
    latest_finish = std::max(latest_finish, state.now);
  }
  if (std::isfinite(first_arrival) && std::isfinite(latest_finish)) {
    result.diagnostics.total_cases_seconds =
        std::max(0.0, latest_finish - first_arrival);
  }
  if (options.enable_diagnostics) {
    result.diagnostics.median_case_execution_seconds =
        median(result.diagnostics.case_execution_seconds);

  }
  {
    // The contract's case execution time uses the first and last visible
    // event timestamps. Keep the legacy scheduler elapsed time above because
    // it also accounts for silent work and is part of the existing API.
    result.diagnostics.cases_ex_time.reserve(result.log.traces.size());
    double first_visible = std::numeric_limits<double>::infinity();
    double last_visible = -std::numeric_limits<double>::infinity();
    for (const auto& trace : result.log.traces) {
      if (trace.events.empty()) {
        throw std::runtime_error("Monte Carlo simulated case has no visible events");
      }
      const auto first = parse_timestamp(trace.events.front().timestamp);
      const auto last = parse_timestamp(trace.events.back().timestamp);
      if (!first || !last || !std::isfinite(*first) ||
          !std::isfinite(*last) || *last < *first) {
        result.diagnostics.cases_ex_time.push_back(0.0);
        continue;
      }
      result.diagnostics.cases_ex_time.push_back(*last - *first);
      // The reference measures from the first event of the first case, not
      // the earliest event of any case.
      if (&trace == &result.log.traces.front()) first_visible = *first;
      last_visible = std::max(last_visible, *last);
    }
    result.diagnostics.median_cases_ex_time =
        median(result.diagnostics.cases_ex_time);
    result.diagnostics.total_cases_time =
        std::isfinite(first_visible) && std::isfinite(last_visible)
            ? std::max(0.0, last_visible - first_visible)
            : result.diagnostics.total_cases_seconds;
  }
  for (auto& tree : result.output_places_interval_trees) {
    std::sort(tree.intervals.begin(), tree.intervals.end(),
              [](const MonteCarloInterval& lhs, const MonteCarloInterval& rhs) {
                return std::tie(lhs.start, lhs.end) <
                       std::tie(rhs.start, rhs.end);
              });
  }
  for (auto& tree : result.output_transitions_interval_trees) {
    std::sort(tree.intervals.begin(), tree.intervals.end(),
              [](const MonteCarloInterval& lhs, const MonteCarloInterval& rhs) {
                return std::tie(lhs.start, lhs.end) <
                       std::tie(rhs.start, rhs.end);
              });
  }
  return result;
}

}  // namespace

MonteCarloOptions fit_monte_carlo_options(const EventLog& input_log,
                                          const PetriNet& net,
                                          const MonteCarloOptions& options) {
  validate_selectors(options);
  validate_arrival_aliases(options);
  validate_force_distribution(options);
  // Marking membership also controls simulation with supplied distributions.
  // Validate it before any fitting bypass; node names remain fitting-only.
  if (!options.initial_marking_keys.empty() || !options.final_marking_keys.empty()) {
    validate_net(net);
    if (!options.initial_marking_keys.empty())
      (void)resolve_marking_keys(resolve_marking(options.initial_marking,
          net.place_count(), net.source(), "initial"), options.initial_marking_keys, "initial");
    if (!options.final_marking_keys.empty())
      (void)resolve_marking_keys(resolve_marking(options.final_marking,
          net.place_count(), net.sink(), "final"), options.final_marking_keys, "final");
  }
  MonteCarloOptions fitted = options;
  if (!fitted.case_arrival_ratio && !fitted.case_arrival_seconds) {
    fitted.case_arrival_ratio =
        infer_case_arrival_seconds(input_log, fitted.timestamp_key);
  }
  if (fitted.stochastic_map) {
    validate_stochastic_map_sparse(*fitted.stochastic_map,
                                   net.transitions().size());
    return fitted;
  }
  if (!fitted.transition_weights.empty() && !fitted.transition_durations.empty())
    return fitted;
  if (input_log.traces.empty()) {
    fitted.stochastic_map = StochasticMap{};
    return fitted;
  }
  StochasticMapFitOptions fit;
  fit.fit_activation_weights = fitted.transition_weights.empty();
  fit.fit_timing = fitted.transition_durations.empty();
  if (fitted.force_distribution) {
    switch (*fitted.force_distribution) {
      case MonteCarloDurationDistribution::kNormal:
        fit.distribution = StochasticMapDistribution::kNormal; break;
      case MonteCarloDurationDistribution::kUniform:
        fit.distribution = StochasticMapDistribution::kUniform; break;
      case MonteCarloDurationDistribution::kExponential:
        fit.distribution = StochasticMapDistribution::kExponential; break;
      default: break;  // validate_force_distribution already rejected this.
    }
  }
  fit.replay.variant = fitted.token_replay_variant;
  fit.replay.activity_attribute = fitted.activity_key;
  fit.replay.timestamp_attribute = fitted.timestamp_key;
  const auto replay_marking = [](const std::vector<std::uint32_t>& marking) {
    std::vector<int> result;
    result.reserve(marking.size());
    for (const auto tokens : marking) {
      if (tokens > static_cast<std::uint32_t>(std::numeric_limits<int>::max()))
        throw std::invalid_argument(
            "Monte Carlo stochastic fitting marking exceeds signed token range");
      result.push_back(static_cast<int>(tokens));
    }
    return result;
  };
  fit.replay.initial_marking = replay_marking(fitted.initial_marking);
  fit.replay.final_marking = replay_marking(fitted.final_marking);
  fit.replay.place_names = fitted.place_names;
  fit.replay.transition_names = fitted.transition_names;
  fit.replay.initial_marking_keys = fitted.initial_marking_keys;
  fit.replay.final_marking_keys = fitted.final_marking_keys;
  fitted.stochastic_map = fit_stochastic_map_with_report(input_log, net, fit).map;
  return fitted;
}

MonteCarloResult monte_carlo(const PetriNet& net,
                             const MonteCarloOptions& options) {
  return run_monte_carlo(net, options);
}

MonteCarloResult monte_carlo(const EventLog& input_log, const PetriNet& net,
                             const MonteCarloOptions& options) {
  return run_monte_carlo(net, fit_monte_carlo_options(input_log, net, options));
}

}  // namespace minera
