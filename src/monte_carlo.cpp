#include "minera/monte_carlo.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <numeric>
#include <queue>
#include <random>
#include <stdexcept>
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
};

[[nodiscard]] bool is_final_marking(const PetriNet& net,
                                    const Marking& marking) {
  if (marking.size() != net.place_count()) return false;
  for (std::size_t place = 0; place < marking.size(); ++place) {
    const std::uint64_t expected = place == net.sink() ? 1U : 0U;
    if (marking[place] != expected) return false;
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

PreparedOptions prepare_options(const PetriNet& net,
                                const MonteCarloOptions& options) {
  if (!std::isfinite(options.start) ||
      !std::isfinite(options.case_arrival_seconds) ||
      options.case_arrival_seconds < 0.0 ||
      !std::isfinite(options.default_duration_seconds) ||
      options.default_duration_seconds < 0.0) {
    throw std::invalid_argument(
        "Monte Carlo time values must be finite and non-negative");
  }
  if (options.default_resources_per_place == 0) {
    throw std::invalid_argument(
        "Monte Carlo default resource count must be positive");
  }

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
  if (options.transition_weights.empty()) {
    prepared.weights.assign(transition_count, 1.0);
  } else {
    prepared.weights = options.transition_weights;
  }
  prepared.durations.assign(
      transition_count,
      MonteCarloDuration{MonteCarloDurationDistribution::kDeterministic,
                         options.default_duration_seconds});
  if (!options.transition_durations.empty()) {
    prepared.durations = options.transition_durations;
  }

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
    prepared.capacities[place] = capacity;
  }
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
      const double draw = std::normal_distribution<double>(
          duration.value_seconds, duration.secondary_seconds)(rng);
      // Service times cannot be negative; clamp the lower tail while
      // retaining the seeded draw sequence.
      return std::max(0.0, draw);
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

// Reserve output-place resources for one firing.  Every output arc reserves a
// slot for the transition's service interval.  A transition requiring several
// slots on one place waits for the corresponding number of earliest slots.
double reserve_output_resources(
    const PetriTransition& transition,
    const std::vector<std::size_t>& capacities,
    std::vector<std::vector<double>>& available,
    double ready_time,
    double duration) {
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

MonteCarloResult run_monte_carlo(const PetriNet& net,
                                 const MonteCarloOptions& options) {
  validate_net(net);
  const auto prepared = prepare_options(net, options);

  MonteCarloResult result;
  result.diagnostics.simulated_cases = options.simulations;
  result.diagnostics.case_arrival_seconds = options.case_arrival_seconds;
  result.log.traces.reserve(options.simulations);
  if (options.enable_diagnostics) {
    result.diagnostics.case_execution_seconds.assign(options.simulations, 0.0);
  }
  if (options.simulations == 0) return result;

  std::vector<CaseState> cases;
  cases.reserve(options.simulations);
  std::priority_queue<QueueEntry, std::vector<QueueEntry>, QueueEntryGreater>
      queue;
  std::uint64_t serial = 0;
  for (std::size_t index = 0; index < options.simulations; ++index) {
    const double arrival =
        options.start + static_cast<double>(index) * options.case_arrival_seconds;
    if (!std::isfinite(arrival))
      throw std::invalid_argument("Monte Carlo case arrival is not finite");
    CaseState state;
    state.index = index;
    state.arrival = arrival;
    state.now = arrival;
    state.marking.assign(net.place_count(), 0);
    state.marking[net.source()] = 1;
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

    if (options.stop_at_final && is_final_marking(net, state.marking)) {
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
    const double start = reserve_output_resources(
        transition, prepared.capacities, resource_available, state.now,
        duration);
    const double finish = start + duration;
    if (!std::isfinite(finish) || finish < start) {
      throw std::runtime_error("Monte Carlo virtual time overflow");
    }
    fire_transition(transition, state.marking);
    state.now = finish;
    ++state.steps;

    if (!transition.silent()) {
      Event event;
      event.case_id = state.trace.case_id;
      event.activity = net.activities()[*transition.activity];
      event.timestamp = format_timestamp(finish);
      event.input_order = state.trace.events.size();
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
  }
  return result;
}

}  // namespace

MonteCarloOptions fit_monte_carlo_options(const EventLog& input_log,
                                          const PetriNet& net,
                                          const MonteCarloOptions& options) {
  MonteCarloOptions fitted = options;
  if (input_log.traces.empty()) return fitted;

  std::map<std::string, std::uint64_t> activity_counts;
  std::map<std::string, double> elapsed_seconds;
  std::map<std::string, std::size_t> elapsed_samples;
  for (const auto& trace : input_log.traces) {
    for (std::size_t index = 0; index < trace.events.size(); ++index) {
      const auto& event = trace.events[index];
      ++activity_counts[event.activity];
      if (index + 1 >= trace.events.size()) continue;
      const auto current = parse_timestamp(event.timestamp);
      const auto next = parse_timestamp(trace.events[index + 1].timestamp);
      if (!current || !next || !std::isfinite(*current) ||
          !std::isfinite(*next) || *next < *current) {
        continue;
      }
      const double elapsed = *next - *current;
      if (!std::isfinite(elapsed)) continue;
      elapsed_seconds[event.activity] += elapsed;
      ++elapsed_samples[event.activity];
    }
  }

  if (fitted.transition_weights.empty()) {
    bool observed_transition = false;
    std::vector<double> weights(net.transitions().size(), 0.0);
    for (std::size_t index = 0; index < net.transitions().size(); ++index) {
      const auto& transition = net.transitions()[index];
      if (!transition.activity ||
          *transition.activity >= net.activities().size()) {
        weights[index] = 1.0;
        continue;
      }
      const auto it = activity_counts.find(net.activities()[*transition.activity]);
      if (it != activity_counts.end() && it->second != 0) {
        weights[index] = static_cast<double>(it->second);
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
      if (std::isfinite(mean) && mean >= 0.0) durations[index].value_seconds = mean;
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
