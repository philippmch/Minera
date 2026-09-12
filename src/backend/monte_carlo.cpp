#include "minera/monte_carlo.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <deque>
#include <limits>
#include <map>
#include <numeric>
#include <queue>
#include <random>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "minera/timestamp.hpp"

namespace minera {
namespace {

using Marking = std::vector<std::uint64_t>;

struct CaseState {
  std::size_t index = 0;
  double arrival = 0.0;
  double now = 0.0;
  Marking marking;
  // Token assignment timestamps are owned by the case that created them.
  // Keeping these queues with the marking prevents one case's ordinary or
  // reset arc from consuming another case's interval history.
  std::vector<std::deque<double>> place_assignments;
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
constexpr std::size_t kMaxMonteCarloInitialAssignments = 1'000'000;
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
                                    const Marking& final_marking) {
  if (marking.size() != final_marking.size()) return false;
  for (std::size_t place = 0; place < marking.size(); ++place) {
    if (marking[place] < final_marking[place]) return false;
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
      transition.activation_weight < 0.0 ||
      !std::isfinite(transition.parameter) ||
      !std::isfinite(transition.secondary_parameter) ||
      !std::isfinite(transition.tertiary_parameter) ||
      transition.secondary_parameter < 0.0 ||
      transition.tertiary_parameter < 0.0) {
    throw std::invalid_argument(
        "Monte Carlo stochastic_map record values must be finite with non-negative auxiliary parameters and weight");
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
    case StochasticMapDistribution::kDeterministic:
    case StochasticMapDistribution::kNormal:
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
      if (transition.parameter < 0.0 ||
          transition.secondary_parameter < transition.parameter) {
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
      if (transition.parameter <= 0.0 ||
          transition.secondary_parameter <= 0.0) {
        throw std::invalid_argument(
            "Monte Carlo stochastic_map gamma scale and shape must be positive");
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

  std::size_t total_resources = 0;
  for (const auto capacity : capacities) {
    if (capacity == 0 || capacity > kMaxMonteCarloResourceCapacity) {
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
  if (initial_tokens != 0 &&
      options.simulations >
          kMaxMonteCarloInitialAssignments / initial_tokens) {
    throw std::invalid_argument(
        "Monte Carlo initial assignment budget exceeds the native safety limit");
  }
  if (options.max_steps != 0 && options.simulations != 0 &&
      options.simulations >
          kMaxMonteCarloSimulationSteps / options.max_steps) {
    throw std::invalid_argument(
        "Monte Carlo simulation step budget exceeds the native safety limit");
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
  if (options.default_resources_per_place == 0) {
    throw std::invalid_argument(
        "Monte Carlo default resource count must be positive");
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
    if (!std::isfinite(weight) || weight < 0.0) {
      throw std::invalid_argument(
          "Monte Carlo transition_weights must be finite and non-negative");
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
      case MonteCarloDurationDistribution::kExponential:
        if (duration.value_seconds < 0.0) {
          throw std::invalid_argument(
              "Monte Carlo transition duration must be non-negative");
        }
        break;
      case MonteCarloDurationDistribution::kConstant0:
      case MonteCarloDurationDistribution::kImmediate:
        // These distributions have no numeric parameter and always return 0.
        break;
      case MonteCarloDurationDistribution::kNormal:
        if (duration.value_seconds < 0.0 ||
            duration.secondary_seconds < 0.0) {
          throw std::invalid_argument(
              "Monte Carlo normal mean and standard deviation must be non-negative");
        }
        break;
      case MonteCarloDurationDistribution::kUniform:
        if (duration.value_seconds < 0.0 ||
            duration.secondary_seconds < 0.0) {
          throw std::invalid_argument(
              "Monte Carlo uniform lower bound and width must be non-negative");
        }
        if (!std::isfinite(duration.value_seconds +
                           duration.secondary_seconds)) {
          throw std::invalid_argument(
              "Monte Carlo uniform upper bound must be finite");
        }
        break;
      case MonteCarloDurationDistribution::kLogNormal:
        if (duration.value_seconds <= 0.0 ||
            duration.secondary_seconds < 0.0 ||
            duration.tertiary_seconds <= 0.0) {
          throw std::invalid_argument(
              "Monte Carlo lognormal shape must be positive with a positive scale");
        }
        break;
      case MonteCarloDurationDistribution::kGamma:
        if (duration.value_seconds <= 0.0 ||
            duration.secondary_seconds < 0.0 ||
            duration.tertiary_seconds <= 0.0) {
          throw std::invalid_argument(
              "Monte Carlo lognormal/gamma shape and scale must be positive, with a non-negative location");
        }
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
      const auto duration = duration_from_stochastic_map(transition);
      if (!std::isfinite(duration.value_seconds) ||
          !std::isfinite(duration.secondary_seconds) ||
          !std::isfinite(duration.tertiary_seconds)) {
        throw std::invalid_argument(
            "Monte Carlo stochastic_map duration conversion is not finite");
      }
      prepared.durations[transition.transition] = duration;
    }
  }
  if (!options.transition_weights.empty())
    prepared.weights = options.transition_weights;
  if (!options.transition_durations.empty())
    prepared.durations = options.transition_durations;

  prepared.capacities.assign(net.place_count(),
                             options.default_resources_per_place);
  for (const auto& [place, capacity] : options.resources_per_place) {
    if (place >= net.place_count()) {
      throw std::invalid_argument(
          "Monte Carlo resource place index is invalid");
    }
    if (capacity == 0) {
      throw std::invalid_argument(
          "Monte Carlo resource count must be positive");
    }
    if (capacity > kMaxMonteCarloResourceCapacity) {
      throw std::invalid_argument(
          "Monte Carlo resource capacity exceeds the native safety limit");
    }
    prepared.capacities[place] = capacity;
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
    if (transition.inputs.empty() && !transition.has_extended_arcs()) continue;
    if (petri_transition_enabled(transition, marking)) enabled.push_back(index);
  }
  return enabled;
}

[[nodiscard]] std::size_t choose_transition(
    const std::vector<std::size_t>& enabled,
    const std::vector<double>& weights,
    std::mt19937_64& rng) {
  if (enabled.empty()) throw std::invalid_argument("no enabled transitions");
  double maximum = 0.0;
  for (const auto index : enabled) maximum = std::max(maximum, weights[index]);
  if (!(maximum > 0.0)) {
    return enabled[std::uniform_int_distribution<std::size_t>(
        0, enabled.size() - 1)(rng)];
  }
  std::vector<double> scaled;
  scaled.reserve(enabled.size());
  for (const auto index : enabled) scaled.push_back(weights[index] / maximum);
  const auto picked =
      std::discrete_distribution<std::size_t>(scaled.begin(), scaled.end())(rng);
  return enabled[picked];
}

[[nodiscard]] double sample_duration(const MonteCarloDuration& duration,
                                     std::mt19937_64& rng) {
  switch (duration.distribution) {
    case MonteCarloDurationDistribution::kDeterministic:
      return duration.value_seconds;
    case MonteCarloDurationDistribution::kExponential:
      if (duration.value_seconds == 0.0) return 0.0;
      return std::exponential_distribution<double>(
          1.0 / duration.value_seconds)(rng);
    case MonteCarloDurationDistribution::kConstant0:
    case MonteCarloDurationDistribution::kImmediate:
      return 0.0;
    case MonteCarloDurationDistribution::kNormal: {
      if (duration.secondary_seconds == 0.0) return duration.value_seconds;
      std::normal_distribution<double> distribution(duration.value_seconds,
                                                     duration.secondary_seconds);
      double draw = distribution(rng);
      while (draw < 0.0) draw = distribution(rng);
      return draw;
    }
    case MonteCarloDurationDistribution::kUniform:
      if (duration.secondary_seconds == 0.0) return duration.value_seconds;
      return std::uniform_real_distribution<double>(
          duration.value_seconds,
          duration.value_seconds + duration.secondary_seconds)(rng);
    case MonteCarloDurationDistribution::kLogNormal: {
      const double draw = std::lognormal_distribution<double>(
          0.0, duration.value_seconds)(rng);
      return duration.secondary_seconds + duration.tertiary_seconds * draw;
    }
    case MonteCarloDurationDistribution::kGamma:
      return duration.secondary_seconds +
             std::gamma_distribution<double>(duration.value_seconds,
                                             duration.tertiary_seconds)(rng);
    default:
      throw std::invalid_argument("Monte Carlo duration distribution is invalid");
  }
}

void append_interval(std::vector<MonteCarloInterval>& intervals,
                     double start, double end, RuntimeBudget& budget) {
  if (!std::isfinite(start) || !std::isfinite(end) || end < start) {
    throw std::runtime_error("Monte Carlo interval has invalid finite bounds");
  }
  if (end <= start) return;
  if (budget.interval_records >= kMaxMonteCarloRuntimeIntervals) {
    throw std::runtime_error(
        "Monte Carlo interval record budget exceeded");
  }
  intervals.push_back({start, end});
  ++budget.interval_records;
}

// Reserve output-place resources for one firing. A transition requiring
// several slots on one place waits for the corresponding number of earliest
// slots. Place occupancy is recorded when an input token is consumed below;
// output reservation itself is not a place-occupancy interval.
double reserve_output_resources(
    std::size_t transition_index,
    const PetriTransition& transition,
    const std::vector<std::size_t>& capacities,
    std::vector<std::vector<double>>& available,
    std::vector<MonteCarloTransitionIntervalTree>& transition_intervals,
    double ready_time, double duration, RuntimeBudget& budget,
    std::size_t interval_limit_before_place_intervals) {
  std::map<std::size_t, std::size_t> output_counts;
  for (const auto place : transition.outputs) ++output_counts[place];
  double start = ready_time;
  for (const auto& [place, count] : output_counts) {
    if (count > capacities[place]) {
      throw std::invalid_argument(
          "Monte Carlo transition output demand exceeds place capacity");
    }
    auto order = std::vector<std::size_t>(available[place].size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](const std::size_t lhs,
                                              const std::size_t rhs) {
      return std::tie(available[place][lhs], lhs) <
             std::tie(available[place][rhs], rhs);
    });
    start = std::max(start, available[place][order[count - 1]]);
  }
  const double finish = start + duration;
  if (!std::isfinite(finish) || finish < start) {
    throw std::runtime_error("Monte Carlo virtual time overflow");
  }
  if (start > ready_time) {
    // Place intervals are appended by the caller after this wait interval.
    // Reserve enough aggregate headroom for both so a budget failure cannot
    // occur after resource availability has been mutated.
    if (interval_limit_before_place_intervals == 0 ||
        budget.interval_records >= interval_limit_before_place_intervals) {
      throw std::runtime_error(
          "Monte Carlo interval record budget exceeded");
    }
    append_interval(transition_intervals[transition_index].intervals,
                    ready_time, start, budget);
  }
  for (const auto& [place, count] : output_counts) {
    auto order = std::vector<std::size_t>(available[place].size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](const std::size_t lhs,
                                              const std::size_t rhs) {
      return std::tie(available[place][lhs], lhs) <
             std::tie(available[place][rhs], rhs);
    });
    for (std::size_t i = 0; i < count; ++i) {
      available[place][order[i]] = finish;
    }
  }
  return start;
}

void assign_place_timestamp(std::vector<std::deque<double>>& assignments,
                            std::size_t place, double timestamp,
                            RuntimeBudget& budget) {
  if (!std::isfinite(timestamp)) {
    throw std::runtime_error("Monte Carlo place assignment is not finite");
  }
  if (budget.live_assignments >= kMaxMonteCarloRuntimeAssignments) {
    throw std::runtime_error(
        "Monte Carlo live assignment budget exceeded");
  }
  auto& queue = assignments[place];
  const auto position = std::upper_bound(queue.begin(), queue.end(), timestamp);
  queue.insert(position, timestamp);
  ++budget.live_assignments;
}

void consume_place_assignment(
    std::vector<std::deque<double>>& assignments, std::size_t place,
    double completion_time, std::vector<MonteCarloInterval>& intervals,
    RuntimeBudget& budget) {
  auto& queue = assignments[place];
  if (queue.empty()) {
    throw std::runtime_error("Monte Carlo place assignment queue underflow");
  }
  const double assigned_time = queue.front();
  append_interval(intervals, assigned_time, completion_time, budget);
  queue.pop_front();
}

void consume_reset_assignments(
    std::vector<std::deque<double>>& assignments, std::size_t place,
    double completion_time, std::vector<MonteCarloInterval>& intervals,
    RuntimeBudget& budget) {
  auto& queue = assignments[place];
  while (!queue.empty()) {
    const double assigned_time = queue.front();
    append_interval(intervals, assigned_time, completion_time, budget);
    queue.pop_front();
  }
}

void validate_assignment_timestamps(const std::deque<double>& assignments,
                                    std::size_t count, double ready_time) {
  if (count > assignments.size()) {
    throw std::runtime_error("Monte Carlo place assignment queue underflow");
  }
  auto timestamp = assignments.begin();
  for (std::size_t index = 0; index < count; ++index, ++timestamp) {
    if (!std::isfinite(*timestamp) || *timestamp > ready_time) {
      throw std::runtime_error(
          "Monte Carlo place assignment is not available at firing time");
    }
  }
}

void validate_all_assignment_timestamps(const std::deque<double>& assignments,
                                        double ready_time) {
  validate_assignment_timestamps(assignments, assignments.size(), ready_time);
}

void fire_transition(const PetriTransition& transition,
                     Marking& marking) {
  if (!petri_transition_enabled(transition, marking))
    throw std::runtime_error("Monte Carlo fired a disabled transition");
  std::vector<std::uint64_t> required(marking.size(), 0);
  for (const auto place : transition.inputs) {
    if (place >= marking.size() ||
        required[place] == std::numeric_limits<std::uint64_t>::max()) {
      throw std::runtime_error("Monte Carlo input multiplicity overflow");
    }
    ++required[place];
  }
  for (std::size_t place = 0; place < marking.size(); ++place) {
    if (required[place] > marking[place]) {
      throw std::runtime_error("Monte Carlo fired a disabled transition");
    }
    marking[place] -= required[place];
  }
  for (const auto place : transition.reset_inputs) {
    if (place >= marking.size()) throw std::runtime_error("Monte Carlo reset place is invalid");
    marking[place] = 0;
  }
  for (const auto place : transition.outputs) {
    if (place >= marking.size() ||
        marking[place] == std::numeric_limits<std::uint64_t>::max()) {
      throw std::overflow_error("Monte Carlo marking token overflow");
    }
    ++marking[place];
  }
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

[[nodiscard]] std::optional<std::string> selected_activity_value(
    const Event& event, std::string_view key) {
  if (is_activity_alias(key)) return event.activity;
  if (const auto* value = event.attribute(key)) return *value;
  return std::nullopt;
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
  if (options.simulations == 0) return result;

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
    state.place_assignments.resize(net.place_count());
    for (std::size_t place = 0; place < state.marking.size(); ++place) {
      for (std::uint64_t token = 0; token < state.marking[place]; ++token) {
        assign_place_timestamp(state.place_assignments, place, state.arrival,
                               runtime_budget);
      }
    }
    state.trace.case_id = "case_" + std::to_string(index + 1);
    cases.push_back(std::move(state));
    queue.push({arrival, index, serial++});
  }

  std::vector<std::vector<double>> resource_available(net.place_count());
  for (std::size_t place = 0; place < net.place_count(); ++place) {
    resource_available[place].assign(prepared.capacities[place], 0.0);
  }
  std::mt19937_64 rng(options.seed);

  while (!queue.empty()) {
    const auto entry = queue.top();
    queue.pop();
    auto& state = cases[entry.case_index];
    if (state.finished) continue;
    state.now = std::max(state.now, entry.time);

    if (options.stop_at_final &&
        is_final_marking(state.marking, prepared.final_marking)) {
      state.completed = true;
      finalize_case(state, result.diagnostics, options,
                    &result.diagnostics.case_execution_seconds);
      continue;
    }
    if (state.steps >= options.max_steps) {
      state.step_limited = true;
      finalize_case(state, result.diagnostics, options,
                    &result.diagnostics.case_execution_seconds);
      continue;
    }

    const auto enabled = enabled_transitions(net, state.marking);
    if (enabled.empty()) {
      state.deadlocked = true;
      finalize_case(state, result.diagnostics, options,
                    &result.diagnostics.case_execution_seconds);
      continue;
    }

    const auto transition_index =
        choose_transition(enabled, prepared.weights, rng);
    const auto& transition = net.transitions()[transition_index];
    const double duration =
        sample_duration(prepared.durations[transition_index], rng);
    if (!std::isfinite(duration) || duration < 0.0) {
      throw std::runtime_error("Monte Carlo sampled an invalid duration");
    }

    // Preflight all assignment and interval effects before reserving shared
    // resources or mutating this case. Reset arcs consume every assignment in
    // their place, so their queues must be included in both aggregate caps.
    std::map<std::size_t, std::size_t> input_counts;
    for (const auto place : transition.inputs) {
      auto& count = input_counts[place];
      if (count == std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error("Monte Carlo input multiplicity overflow");
      }
      ++count;
    }
    std::map<std::size_t, bool> reset_places;
    for (const auto place : transition.reset_inputs) reset_places[place] = true;

    // A reset consumes the complete queue after ordinary inputs have been
    // removed. If malformed callers overlap an ordinary and reset arc on one
    // place, count that queue once rather than charging it twice; duplicate
    // reset arcs likewise have one effective drain.
    std::size_t consumed_assignments = 0;
    for (const auto& [place, count] : input_counts) {
      const auto& assignments = state.place_assignments[place];
      if (reset_places.contains(place)) {
        validate_all_assignment_timestamps(assignments, state.now);
      } else {
        validate_assignment_timestamps(assignments, count, state.now);
      }
      const auto amount = reset_places.contains(place) ? assignments.size()
                                                       : count;
      if (amount >
          kMaxMonteCarloRuntimeAssignments - consumed_assignments) {
        throw std::runtime_error(
            "Monte Carlo live assignment budget exceeded");
      }
      consumed_assignments += amount;
    }
    for (const auto& [place, unused] : reset_places) {
      (void)unused;
      if (input_counts.contains(place)) continue;
      const auto& assignments = state.place_assignments[place];
      if (assignments.size() >
          kMaxMonteCarloRuntimeAssignments - consumed_assignments) {
        throw std::runtime_error(
            "Monte Carlo live assignment budget exceeded");
      }
      consumed_assignments += assignments.size();
      validate_all_assignment_timestamps(assignments, state.now);
    }
    if (consumed_assignments > runtime_budget.live_assignments) {
      throw std::runtime_error(
          "Monte Carlo place assignment accounting is inconsistent");
    }
    const auto remaining_assignments =
        runtime_budget.live_assignments - consumed_assignments;
    if (transition.outputs.size() >
        kMaxMonteCarloRuntimeAssignments - remaining_assignments) {
      throw std::runtime_error(
          "Monte Carlo live assignment budget exceeded");
    }
    if (runtime_budget.interval_records >
        kMaxMonteCarloRuntimeIntervals - consumed_assignments) {
      throw std::runtime_error(
          "Monte Carlo interval record budget exceeded");
    }
    const double start = reserve_output_resources(
        transition_index, transition, prepared.capacities, resource_available,
        result.output_transitions_interval_trees, state.now, duration,
        runtime_budget,
        kMaxMonteCarloRuntimeIntervals - consumed_assignments);
    const double finish = start + duration;
    if (!std::isfinite(finish) || finish < start) {
      throw std::runtime_error("Monte Carlo virtual time overflow");
    }
    fire_transition(transition, state.marking);
    state.now = finish;
    ++state.steps;
    runtime_budget.live_assignments -= consumed_assignments;

    // A place interval spans the time from token assignment to the firing
    // that consumes that token. Output tokens become available at completion.
    for (const auto place : transition.inputs) {
      consume_place_assignment(
          state.place_assignments, place, finish,
          result.output_places_interval_trees[place].intervals,
          runtime_budget);
    }
    for (const auto place : transition.reset_inputs) {
      consume_reset_assignments(
          state.place_assignments, place, finish,
          result.output_places_interval_trees[place].intervals,
          runtime_budget);
    }
    for (const auto place : transition.outputs) {
      assign_place_timestamp(state.place_assignments, place, finish,
                             runtime_budget);
    }

    if (!transition.silent()) {
      Event event;
      event.case_id = state.trace.case_id;
      event.activity = net.activities()[*transition.activity];
      event.timestamp = format_timestamp(finish);
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

    // The contract's case execution time uses the first and last visible
    // event timestamps. Keep the legacy scheduler elapsed time above because
    // it also accounts for silent work and is part of the existing API.
    result.diagnostics.cases_ex_time.reserve(result.log.traces.size());
    double first_visible = std::numeric_limits<double>::infinity();
    double last_visible = -std::numeric_limits<double>::infinity();
    for (const auto& trace : result.log.traces) {
      if (trace.events.empty()) {
        result.diagnostics.cases_ex_time.push_back(0.0);
        continue;
      }
      const auto first = parse_timestamp(trace.events.front().timestamp);
      const auto last = parse_timestamp(trace.events.back().timestamp);
      if (!first || !last || !std::isfinite(*first) ||
          !std::isfinite(*last) || *last < *first) {
        result.diagnostics.cases_ex_time.push_back(0.0);
        continue;
      }
      result.diagnostics.cases_ex_time.push_back(*last - *first);
      first_visible = std::min(first_visible, *first);
      last_visible = std::max(last_visible, *last);
    }
    result.diagnostics.median_cases_ex_time =
        median(result.diagnostics.cases_ex_time);
    result.diagnostics.total_cases_time =
        std::isfinite(first_visible) && std::isfinite(last_visible)
            ? std::max(0.0, last_visible - first_visible)
            : result.diagnostics.total_cases_seconds;
  } else {
    result.diagnostics.total_cases_time = result.diagnostics.total_cases_seconds;
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
  if (input_log.traces.empty()) return fitted;

  std::map<std::string, double> elapsed_seconds;
  std::map<std::string, std::size_t> elapsed_samples;
  std::map<std::string, std::vector<double>> elapsed_values;
  for (const auto& trace : input_log.traces) {
    for (std::size_t index = 0; index < trace.events.size(); ++index) {
      const auto& event = trace.events[index];
      const auto activity =
          selected_activity_value(event, fitted.activity_key);
      if (!activity) continue;
      if (index + 1 >= trace.events.size()) continue;
      const auto current_value =
          selected_timestamp_value(event, fitted.timestamp_key);
      const auto next_value = selected_timestamp_value(
          trace.events[index + 1], fitted.timestamp_key);
      if (!current_value || !next_value) continue;
      const auto current = parse_timestamp(*current_value);
      const auto next = parse_timestamp(*next_value);
      if (!current || !next || !std::isfinite(*current) ||
          !std::isfinite(*next) || *next < *current) {
        continue;
      }
      const double elapsed = *next - *current;
      if (!std::isfinite(elapsed)) continue;
      elapsed_seconds[*activity] += elapsed;
      ++elapsed_samples[*activity];
      elapsed_values[*activity].push_back(elapsed);
    }
  }

  if (fitted.transition_weights.empty()) {
    // Replay activation counts include the selected forward/backwards
    // routing semantics and therefore also distinguish duplicate-label or
    // misfit branches in the fitted scheduler weights.
    TokenReplayOptions replay_options;
    replay_options.variant = fitted.token_replay_variant;
    replay_options.activity_attribute = fitted.activity_key;
    const auto replay_marking = [](const std::vector<std::uint32_t>& marking) {
      std::vector<int> result;
      result.reserve(marking.size());
      for (const auto tokens : marking) {
        if (tokens > static_cast<std::uint32_t>(std::numeric_limits<int>::max()))
          throw std::invalid_argument(
              "Monte Carlo activation-weight replay marking exceeds signed token range");
        result.push_back(static_cast<int>(tokens));
      }
      return result;
    };
    replay_options.initial_marking = replay_marking(fitted.initial_marking);
    replay_options.final_marking = replay_marking(fitted.final_marking);
    replay_options.place_names = fitted.place_names;
    replay_options.transition_names = fitted.transition_names;
    replay_options.initial_marking_keys = fitted.initial_marking_keys;
    replay_options.final_marking_keys = fitted.final_marking_keys;
    const auto replay =
        replay_token_diagnostics(input_log, net, replay_options);
    bool observed_transition = false;
    std::vector<double> weights(net.transitions().size(), 0.0);
    for (std::size_t index = 0; index < net.transitions().size(); ++index) {
      const auto& transition = net.transitions()[index];
      if (!transition.activity ||
          *transition.activity >= net.activities().size()) {
        weights[index] = 1.0;
        continue;
      }
      const auto activations = replay.transition_activations[index];
      if (activations != 0) {
        weights[index] = static_cast<double>(activations);
        observed_transition = true;
      }
    }
    if (observed_transition) fitted.transition_weights = std::move(weights);
  }

  if (fitted.transition_durations.empty()) {
    std::vector<MonteCarloDuration> durations(
        net.transitions().size(),
        MonteCarloDuration{MonteCarloDurationDistribution::kDeterministic,
                           fitted.default_duration_seconds});
    for (std::size_t index = 0; index < net.transitions().size(); ++index) {
      const auto& transition = net.transitions()[index];
      if (!transition.activity ||
          *transition.activity >= net.activities().size()) {
        continue;
      }
      const auto activity = net.activities()[*transition.activity];
      const auto samples = elapsed_samples.find(activity);
      const auto total = elapsed_seconds.find(activity);
      if (samples == elapsed_samples.end() || total == elapsed_seconds.end() ||
          samples->second == 0) {
        continue;
      }
      const double mean = total->second /
                          static_cast<double>(samples->second);
      if (!std::isfinite(mean) || mean < 0.0) continue;
      const auto values = elapsed_values.find(activity);
      const bool force_distribution = fitted.force_distribution.has_value();
      if (!force_distribution || values == elapsed_values.end() ||
          values->second.empty()) {
        durations[index].value_seconds = mean;
        continue;
      }
      const auto& samples_for_activity = values->second;
      const auto [minimum, maximum] = std::minmax_element(
          samples_for_activity.begin(), samples_for_activity.end());
      const double width = *maximum - *minimum;
      double variance = 0.0;
      for (const double value : samples_for_activity) {
        const double delta = value - mean;
        variance += delta * delta;
      }
      variance /= static_cast<double>(samples_for_activity.size());
      if (!std::isfinite(width) || !std::isfinite(variance) ||
          width <= 0.0 || variance <= 0.0 || mean <= 0.0) {
        durations[index].value_seconds = mean;
        continue;
      }
      switch (*fitted.force_distribution) {
        case MonteCarloDurationDistribution::kNormal:
          durations[index] = MonteCarloDuration{
              MonteCarloDurationDistribution::kNormal, mean,
              std::sqrt(variance)};
          break;
        case MonteCarloDurationDistribution::kUniform:
          durations[index] = MonteCarloDuration{
              MonteCarloDurationDistribution::kUniform, *minimum, width};
          break;
        case MonteCarloDurationDistribution::kExponential:
          durations[index] = MonteCarloDuration{
              MonteCarloDurationDistribution::kExponential, mean};
          break;
        default:
          // prepare_options validates this before simulation; retain a safe
          // deterministic fit if this helper is called independently.
          durations[index].value_seconds = mean;
          break;
      }
    }
    fitted.transition_durations = std::move(durations);
  }
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
