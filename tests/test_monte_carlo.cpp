#include "minera/monte_carlo.hpp"

#include <cassert>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

minera::PetriNet linear_net() {
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({"A"});
  net.add_transition(0, {source}, {sink});
  return net;
}

minera::PetriNet choice_net() {
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({"A", "B"});
  net.add_transition(0, {source}, {sink});
  net.add_transition(1, {source}, {sink});
  return net;
}

minera::PetriNet deadlock_net() {
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto middle = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({"A"});
  net.add_transition(0, {source}, {middle});
  return net;
}

void assert_same_logs(const minera::EventLog& lhs, const minera::EventLog& rhs) {
  assert(lhs.traces.size() == rhs.traces.size());
  for (std::size_t i = 0; i < lhs.traces.size(); ++i) {
    assert(lhs.traces[i].case_id == rhs.traces[i].case_id);
    assert(lhs.traces[i].events.size() == rhs.traces[i].events.size());
    for (std::size_t j = 0; j < lhs.traces[i].events.size(); ++j) {
      const auto& a = lhs.traces[i].events[j];
      const auto& b = rhs.traces[i].events[j];
      assert(a.case_id == b.case_id);
      assert(a.activity == b.activity);
      assert(a.timestamp == b.timestamp);
      assert(a.input_order == b.input_order);
    }
  }
}

void deterministic_seed_and_diagnostics() {
  const auto net = linear_net();
  minera::MonteCarloOptions options;
  options.simulations = 3;
  options.seed = 17;
  options.start = 1767225600.0;
  options.case_arrival_seconds = 2.0;
  options.transition_durations = {{
      minera::MonteCarloDurationDistribution::kDeterministic, 5.0}};
  options.resources_per_place[net.sink()] = options.simulations;

  const auto first = minera::monte_carlo(net, options);
  const auto second = minera::monte_carlo(net, options);
  assert(first.log.traces.size() == 3);
  assert_same_logs(first.log, second.log);
  assert(first.diagnostics.simulated_cases == 3);
  assert(first.diagnostics.completed_cases == 3);
  assert(first.diagnostics.deadlocked_cases == 0);
  assert(first.diagnostics.step_limited_cases == 0);
  assert(first.diagnostics.case_execution_seconds.size() == 3);
  assert(std::abs(first.diagnostics.median_case_execution_seconds - 5.0) <
         1e-9);
  assert(first.diagnostics.total_cases_seconds >= 9.0);
  assert(first.log.traces[0].events[0].timestamp ==
         "2026-01-01T00:00:05.000Z");
}

void weighted_choices_and_exponential_replay() {
  const auto net = choice_net();
  minera::MonteCarloOptions weighted;
  weighted.simulations = 32;
  weighted.transition_weights = {0.0, 1.0};
  weighted.transition_durations = {
      {minera::MonteCarloDurationDistribution::kDeterministic, 1.0},
      {minera::MonteCarloDurationDistribution::kDeterministic, 2.0}};
  const auto all_b = minera::monte_carlo(net, weighted);
  for (const auto& trace : all_b.log.traces) {
    assert(trace.events.size() == 1);
    assert(trace.events.front().activity == "B");
  }

  minera::MonteCarloOptions exponential;
  exponential.simulations = 4;
  exponential.seed = 1234;
  exponential.transition_durations = {
      {minera::MonteCarloDurationDistribution::kExponential, 3.0},
      {minera::MonteCarloDurationDistribution::kExponential, 3.0}};
  const auto first = minera::monte_carlo(net, exponential);
  const auto second = minera::monte_carlo(net, exponential);
  assert_same_logs(first.log, second.log);
  for (const auto& trace : first.log.traces) {
    assert(trace.events.size() == 1);
    assert(!trace.events.front().timestamp.empty());
  }
}

void resources_create_virtual_fifo_wait() {
  const auto net = linear_net();
  minera::MonteCarloOptions options;
  options.simulations = 2;
  options.case_arrival_seconds = 0.0;
  options.transition_durations = {{
      minera::MonteCarloDurationDistribution::kDeterministic, 5.0}};
  options.resources_per_place[net.sink()] = 1;
  const auto result = minera::monte_carlo(net, options);
  assert(result.log.traces[0].events.size() == 1);
  assert(result.log.traces[1].events.size() == 1);
  assert(result.log.traces[0].events[0].timestamp ==
         "2026-01-01T00:00:05.000Z");
  assert(result.log.traces[1].events[0].timestamp ==
         "2026-01-01T00:00:10.000Z");
  assert(result.diagnostics.case_execution_seconds.size() == 2);
  assert(result.diagnostics.case_execution_seconds[0] == 5.0);
  assert(result.diagnostics.case_execution_seconds[1] == 10.0);
}

void deadlocks_and_step_limits_are_reported() {
  const auto deadlock = deadlock_net();
  minera::MonteCarloOptions options;
  options.simulations = 1;
  const auto deadlock_result = minera::monte_carlo(deadlock, options);
  assert(deadlock_result.diagnostics.deadlocked_cases == 1);
  assert(deadlock_result.diagnostics.completed_cases == 0);
  assert(deadlock_result.log.traces[0].events.size() == 1);

  const auto loop = [] {
    minera::PetriNet net;
    const auto source = net.add_place();
    const auto sink = net.add_place();
    net.set_source(source);
    net.set_sink(sink);
    net.set_activities({"A"});
    net.add_transition(0, {source}, {source});
    return net;
  }();
  options.max_steps = 3;
  const auto limited = minera::monte_carlo(loop, options);
  assert(limited.diagnostics.step_limited_cases == 1);
  assert(limited.log.traces[0].events.size() == 3);
}

void invalid_options_are_rejected() {
  const auto net = linear_net();
  bool rejected = false;
  try {
    minera::MonteCarloOptions options;
    options.transition_weights = {1.0, 2.0};
    (void)minera::monte_carlo(net, options);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);

  rejected = false;
  try {
    minera::MonteCarloOptions options;
    options.transition_durations = {{
        minera::MonteCarloDurationDistribution::kDeterministic, -1.0}};
    (void)minera::monte_carlo(net, options);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);

  rejected = false;
  try {
    minera::MonteCarloOptions options;
    options.default_resources_per_place = 0;
    (void)minera::monte_carlo(net, options);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);

  rejected = false;
  try {
    minera::MonteCarloOptions options;
    options.default_duration_seconds = std::numeric_limits<double>::quiet_NaN();
    (void)minera::monte_carlo(net, options);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

void diagnostics_can_be_disabled() {
  const auto net = linear_net();
  minera::MonteCarloOptions options;
  options.simulations = 2;
  options.enable_diagnostics = false;
  const auto result = minera::monte_carlo(net, options);
  assert(result.log.traces.size() == 2);
  assert(result.diagnostics.simulated_cases == 2);
  assert(result.diagnostics.case_execution_seconds.empty());
  assert(result.diagnostics.median_case_execution_seconds == 0.0);
}

}  // namespace

int main() {
  deterministic_seed_and_diagnostics();
  weighted_choices_and_exponential_replay();
  resources_create_virtual_fifo_wait();
  deadlocks_and_step_limits_are_reported();
  invalid_options_are_rejected();
  diagnostics_can_be_disabled();
}
