#include "minera/monte_carlo.hpp"

#include <array>
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

minera::PetriNet sequential_net() {
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto middle = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({"A", "B"});
  net.add_transition(0, {source}, {middle});
  net.add_transition(1, {middle}, {sink});
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

void source_log_fits_missing_weights_and_durations() {
  const auto net = sequential_net();
  minera::EventLog input;
  for (int i = 0; i < 2; ++i) {
    minera::Trace trace;
    trace.case_id = "source_" + std::to_string(i);
    trace.events = {
        minera::Event{trace.case_id, "A", "2026-01-01T00:00:00Z"},
        minera::Event{trace.case_id, "B", "2026-01-01T00:00:04Z"}};
    input.traces.push_back(std::move(trace));
  }
  const auto fitted = minera::fit_monte_carlo_options(input, net);
  assert(fitted.transition_weights.size() == 2);
  assert(fitted.transition_weights[0] == 2.0 &&
         fitted.transition_weights[1] == 2.0);
  assert(fitted.transition_durations.size() == 2);
  assert(fitted.transition_durations[0].distribution ==
         minera::MonteCarloDurationDistribution::kDeterministic);
  assert(fitted.transition_durations[0].value_seconds == 4.0);
  assert(fitted.transition_durations[1].value_seconds == 1.0);

  minera::MonteCarloOptions options;
  options.simulations = 1;
  options.resources_per_place[net.sink()] = 1;
  const auto result = minera::monte_carlo(input, net, options);
  assert(result.diagnostics.completed_cases == 1);
  assert(result.diagnostics.case_execution_seconds.size() == 1);
  assert(result.diagnostics.case_execution_seconds.front() == 5.0);
}

void source_log_forces_duration_distributions() {
  const auto net = sequential_net();
  minera::EventLog input;
  const std::array<int, 3> elapsed = {2, 4, 6};
  for (std::size_t index = 0; index < elapsed.size(); ++index) {
    minera::Trace trace;
    trace.case_id = "fit_" + std::to_string(index);
    trace.events = {
        minera::Event{trace.case_id, "A", "2026-01-01T00:00:00Z"},
        minera::Event{trace.case_id, "B",
                      "2026-01-01T00:00:0" + std::to_string(elapsed[index]) + "Z"}};
    input.traces.push_back(std::move(trace));
  }

  minera::MonteCarloOptions options;
  options.force_distribution =
      minera::MonteCarloDurationDistribution::kNormal;
  const auto normal = minera::fit_monte_carlo_options(input, net, options);
  assert(normal.transition_durations[0].distribution ==
         minera::MonteCarloDurationDistribution::kNormal);
  assert(normal.transition_durations[0].value_seconds == 4.0);
  assert(std::abs(normal.transition_durations[0].secondary_seconds -
                  std::sqrt(8.0 / 3.0)) < 1e-9);

  options.force_distribution =
      minera::MonteCarloDurationDistribution::kUniform;
  const auto uniform = minera::fit_monte_carlo_options(input, net, options);
  assert(uniform.transition_durations[0].distribution ==
         minera::MonteCarloDurationDistribution::kUniform);
  assert(uniform.transition_durations[0].value_seconds == 2.0);
  assert(uniform.transition_durations[0].secondary_seconds == 4.0);

  options.force_distribution =
      minera::MonteCarloDurationDistribution::kExponential;
  const auto exponential = minera::fit_monte_carlo_options(input, net, options);
  assert(exponential.transition_durations[0].distribution ==
         minera::MonteCarloDurationDistribution::kExponential);
  assert(exponential.transition_durations[0].value_seconds == 4.0);

  options.transition_durations = {
      {minera::MonteCarloDurationDistribution::kDeterministic, 9.0},
      {minera::MonteCarloDurationDistribution::kDeterministic, 1.0}};
  const auto explicit_durations =
      minera::fit_monte_carlo_options(input, net, options);
  assert(explicit_durations.transition_durations[0].distribution ==
         minera::MonteCarloDurationDistribution::kDeterministic);
  assert(explicit_durations.transition_durations[0].value_seconds == 9.0);

  minera::EventLog degenerate;
  minera::Trace zero_trace;
  zero_trace.case_id = "zero";
  zero_trace.events = {
      minera::Event{"zero", "A", "2026-01-01T00:00:00Z"},
      minera::Event{"zero", "B", "2026-01-01T00:00:00Z"}};
  degenerate.traces.push_back(std::move(zero_trace));
  options.transition_durations.clear();
  options.force_distribution =
      minera::MonteCarloDurationDistribution::kUniform;
  const auto fallback = minera::fit_monte_carlo_options(degenerate, net, options);
  assert(fallback.transition_durations[0].distribution ==
         minera::MonteCarloDurationDistribution::kDeterministic);
  assert(fallback.transition_durations[0].value_seconds == 0.0);

  options.force_distribution =
      static_cast<minera::MonteCarloDurationDistribution>(99);
  bool rejected = false;
  try {
    (void)minera::fit_monte_carlo_options({}, net, options);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

void scalar_duration_distributions_are_seeded_and_nonnegative() {
  const auto net = linear_net();
  const auto options_for = [&](minera::MonteCarloDuration duration) {
    minera::MonteCarloOptions options;
    options.simulations = 8;
    options.case_arrival_seconds = 0.0;
    options.seed = 901;
    options.resources_per_place[net.sink()] = options.simulations;
    options.transition_durations = {duration};
    return options;
  };
  const auto assert_replay = [&](const minera::MonteCarloOptions& options) {
    const auto first = minera::monte_carlo(net, options);
    const auto second = minera::monte_carlo(net, options);
    assert_same_logs(first.log, second.log);
    assert(first.diagnostics.case_execution_seconds.size() ==
           options.simulations);
    for (const double seconds : first.diagnostics.case_execution_seconds) {
      assert(std::isfinite(seconds));
      assert(seconds >= 0.0);
    }
  };

  auto constant0 = options_for(
      {minera::MonteCarloDurationDistribution::kConstant0, 99.0});
  assert_replay(constant0);
  assert(constant0.transition_durations.front().value_seconds == 99.0);
  const auto zero_result = minera::monte_carlo(net, constant0);
  for (const double seconds : zero_result.diagnostics.case_execution_seconds)
    assert(seconds == 0.0);

  assert_replay(options_for(
      {minera::MonteCarloDurationDistribution::kImmediate, 4.0}));
  const auto normal = options_for(
      {minera::MonteCarloDurationDistribution::kNormal, 3.0, 0.0});
  const auto normal_result = minera::monte_carlo(net, normal);
  for (const double seconds : normal_result.diagnostics.case_execution_seconds)
    assert(seconds == 3.0);
  const auto uniform = options_for(
      {minera::MonteCarloDurationDistribution::kUniform, 2.0, 0.0});
  const auto uniform_result = minera::monte_carlo(net, uniform);
  for (const double seconds : uniform_result.diagnostics.case_execution_seconds)
    assert(seconds == 2.0);

  assert_replay(options_for(
      {minera::MonteCarloDurationDistribution::kLogNormal, 1.0, 0.0, 1.0}));
  assert_replay(options_for(
      {minera::MonteCarloDurationDistribution::kGamma, 1.0, 0.0, 1.0}));
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
    options.force_distribution =
        static_cast<minera::MonteCarloDurationDistribution>(99);
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

  rejected = false;
  try {
    minera::MonteCarloOptions options;
    options.transition_durations = {{
        static_cast<minera::MonteCarloDurationDistribution>(99), 1.0}};
    (void)minera::monte_carlo(net, options);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);

  for (const auto duration : {
           minera::MonteCarloDuration{
               minera::MonteCarloDurationDistribution::kNormal, 1.0, -1.0},
           minera::MonteCarloDuration{
               minera::MonteCarloDurationDistribution::kUniform, 1.0, -1.0},
           minera::MonteCarloDuration{
               minera::MonteCarloDurationDistribution::kLogNormal, 0.0, 0.0,
               1.0},
           minera::MonteCarloDuration{
               minera::MonteCarloDurationDistribution::kGamma, 1.0, 0.0,
               0.0}}) {
    rejected = false;
    try {
      minera::MonteCarloOptions options;
      options.transition_durations = {duration};
      (void)minera::monte_carlo(net, options);
    } catch (const std::invalid_argument&) {
      rejected = true;
    }
    assert(rejected);
  }
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

void json_contract_is_stable() {
  const auto result = minera::monte_carlo(linear_net(), minera::MonteCarloOptions{});
  const auto json = minera::monte_carlo_json(result);
  assert(json.find("\"family\":\"MONTE_CARLO\"") != std::string::npos);
  assert(json.find("\"variant\":\"PETRI_SEMAPH_FIFO\"") !=
         std::string::npos);
  assert(json.find("\"trace_count\":100") != std::string::npos);
  assert(json.find("\"simulated_cases\":100") != std::string::npos);
  assert(json.find("case_execution_seconds") != std::string::npos);
}

}  // namespace

int main() {
  deterministic_seed_and_diagnostics();
  weighted_choices_and_exponential_replay();
  source_log_fits_missing_weights_and_durations();
  source_log_forces_duration_distributions();
  scalar_duration_distributions_are_seeded_and_nonnegative();
  resources_create_virtual_fifo_wait();
  deadlocks_and_step_limits_are_reported();
  invalid_options_are_rejected();
  diagnostics_can_be_disabled();
  json_contract_is_stable();
}
