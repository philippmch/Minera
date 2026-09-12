#include "minera/monte_carlo.hpp"

#include <array>
#include <cassert>
#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

#include "minera/timestamp.hpp"
#include "minera/xes_event_log_importer.hpp"
#include "minera/xes_export.hpp"

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

minera::PetriNet reset_net() {
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto reset_place = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({"A"});
  const auto transition = net.add_transition(0, {source}, {sink});
  net.add_reset_arc(transition, reset_place);
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

minera::PetriNet duplicate_activity_net() {
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto gate = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({"A"});
  net.add_transition(std::nullopt, {source}, {gate});
  net.add_transition(0, {gate}, {sink});
  net.add_transition(0, {gate}, {sink});
  return net;
}

void add_attribute(minera::Event& event, std::string key, std::string value,
                   minera::AttributeKind kind = minera::AttributeKind::kString) {
  event.attributes.push_back(
      minera::EventAttribute{std::move(key), std::move(value), kind});
}

const minera::EventAttribute* find_attribute(const minera::Event& event,
                                             const std::string& key) {
  for (const auto& attribute : event.attributes) {
    if (attribute.key == key) return &attribute;
  }
  return nullptr;
}

template <typename Function>
bool throws_invalid(Function&& function) {
  try {
    function();
  } catch (const std::invalid_argument&) {
    return true;
  }
  return false;
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
  assert(first.diagnostics.cases_ex_time.size() == 3);
  for (const double seconds : first.diagnostics.cases_ex_time)
    assert(seconds == 0.0);
  assert(first.diagnostics.case_arrival_ratio == 2.0);
  assert(first.diagnostics.total_cases_time == 4.0);
  assert(first.diagnostics.median_cases_ex_time == 0.0);
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
  assert(result.diagnostics.cases_ex_time.size() == 2);
  assert(result.diagnostics.cases_ex_time[0] == 0.0);
  assert(result.diagnostics.cases_ex_time[1] == 0.0);
  assert(result.diagnostics.median_cases_ex_time == 0.0);
  assert(result.diagnostics.total_cases_time == 5.0);

  assert(result.output_places_interval_trees.size() == net.place_count());
  const auto& source_intervals = result.output_places_interval_trees[0].intervals;
  assert(source_intervals.size() == 2);
  constexpr double start = 1767225600.0;
  assert(source_intervals[0] ==
         (minera::MonteCarloInterval{start, start + 5.0}));
  assert(source_intervals[1] ==
         (minera::MonteCarloInterval{start, start + 10.0}));
  assert(result.output_places_interval_trees[1].intervals.empty());

  assert(result.output_transitions_interval_trees.size() ==
         net.transitions().size());
  const auto& waiting = result.output_transitions_interval_trees[0].intervals;
  assert(waiting.size() == 1);
  assert(waiting.front() ==
         (minera::MonteCarloInterval{start, start + 5.0}));
}

void reset_arcs_close_only_the_firing_cases_tokens() {
  const auto net = reset_net();
  minera::MonteCarloOptions options;
  options.simulations = 2;
  options.case_arrival_ratio = 2.0;
  options.initial_marking = {1, 2, 0};
  options.transition_durations = {{
      minera::MonteCarloDurationDistribution::kDeterministic, 5.0}};
  options.resources_per_place[net.sink()] = 2;

  const auto result = minera::monte_carlo(net, options);
  assert(result.diagnostics.completed_cases == 2);
  const auto& intervals = result.output_places_interval_trees[1].intervals;
  assert(intervals.size() == 4);
  assert(intervals[0] == (minera::MonteCarloInterval{1767225600.0, 1767225605.0}));
  assert(intervals[1] == (minera::MonteCarloInterval{1767225600.0, 1767225605.0}));
  assert(intervals[2] == (minera::MonteCarloInterval{1767225602.0, 1767225607.0}));
  assert(intervals[3] == (minera::MonteCarloInterval{1767225602.0, 1767225607.0}));

}

void scheduler_budget_limits_reject_unbounded_allocations() {
  const auto net = linear_net();

  minera::MonteCarloOptions options;
  options.default_resources_per_place = 1001;
  assert(throws_invalid([&] { (void)minera::monte_carlo(net, options); }));

  options = {};
  options.resources_per_place[net.sink()] = 1001;
  assert(throws_invalid([&] { (void)minera::monte_carlo(net, options); }));

  options = {};
  options.initial_marking = {1'000'001, 0};
  assert(throws_invalid([&] { (void)minera::monte_carlo(net, options); }));

  options = {};
  options.initial_marking = {600'000, 0};
  options.simulations = 2;
  assert(throws_invalid([&] { (void)minera::monte_carlo(net, options); }));

  options = {};
  options.simulations = std::numeric_limits<std::size_t>::max();
  assert(throws_invalid([&] { (void)minera::monte_carlo(net, options); }));

  minera::PetriNet many_places;
  const auto source = many_places.add_place();
  const auto sink = many_places.add_place();
  for (std::size_t index = 0; index < 999; ++index) {
    many_places.add_place();
  }
  many_places.set_source(source);
  many_places.set_sink(sink);
  many_places.set_activities({"A"});
  many_places.add_transition(0, {source}, {sink});
  options = {};
  options.default_resources_per_place = 1000;
  assert(throws_invalid(
      [&] { (void)minera::monte_carlo(many_places, options); }));
}

void runtime_growth_budgets_reject_amplifying_loops() {
  // A bounded step count is not enough when one firing creates many output
  // tokens. The live-assignment cap must stop this loop before its queues grow
  // beyond the aggregate safety budget.
  minera::PetriNet token_amplifier;
  const auto source = token_amplifier.add_place();
  const auto sink = token_amplifier.add_place();
  token_amplifier.set_source(source);
  token_amplifier.set_sink(sink);
  token_amplifier.set_activities({"A"});
  token_amplifier.add_transition(
      0, {source}, std::vector<std::size_t>(1000, source));

  minera::MonteCarloOptions options;
  options.simulations = 1;
  options.max_steps = 10'000;
  options.transition_durations = {{
      minera::MonteCarloDurationDistribution::kDeterministic, 0.0}};
  options.resources_per_place[source] = 1000;
  bool rejected = false;
  try {
    (void)minera::monte_carlo(token_amplifier, options);
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  assert(rejected);

  // A transition can keep its live token count constant while appending a
  // large number of occupancy intervals on every firing. That aggregate
  // output budget is checked before the next firing mutates resources.
  minera::PetriNet interval_amplifier;
  const auto interval_source = interval_amplifier.add_place();
  const auto interval_sink = interval_amplifier.add_place();
  interval_amplifier.set_source(interval_source);
  interval_amplifier.set_sink(interval_sink);
  interval_amplifier.set_activities({"A"});
  interval_amplifier.add_transition(
      0, std::vector<std::size_t>(1000, interval_source),
      std::vector<std::size_t>(1000, interval_source));

  options = {};
  options.simulations = 1;
  options.max_steps = 10'000;
  options.initial_marking = {1000, 0};
  options.transition_durations = {{
      minera::MonteCarloDurationDistribution::kDeterministic, 1.0}};
  options.resources_per_place[interval_source] = 1000;
  rejected = false;
  try {
    (void)minera::monte_carlo(interval_amplifier, options);
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  assert(rejected);
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

void explicit_markings_and_componentwise_completion() {
  const auto net = sequential_net();
  minera::MonteCarloOptions options;
  options.simulations = 1;
  options.case_arrival_ratio = 0.0;
  options.initial_marking = {2, 0, 0};
  options.final_marking = {0, 0, 1};
  options.transition_weights = {0.0, 1.0};
  options.transition_durations = {
      {minera::MonteCarloDurationDistribution::kDeterministic, 1.0},
      {minera::MonteCarloDurationDistribution::kDeterministic, 1.0}};
  const auto covered = minera::monte_carlo(net, options);
  assert(covered.diagnostics.completed_cases == 1);
  assert(covered.log.traces.front().events.size() == 2);
  // One of the two initial source tokens remains, but the requested sink token
  // is covered and the case completes.
  assert(covered.output_places_interval_trees[0].intervals.size() == 1);

  options.final_marking = {0, 0, 2};
  const auto all_tokens = minera::monte_carlo(net, options);
  assert(all_tokens.diagnostics.completed_cases == 1);
  assert(all_tokens.log.traces.front().events.size() == 4);
  assert(all_tokens.output_places_interval_trees[0].intervals.size() == 2);

  options.initial_marking = {1, 0};
  assert(throws_invalid([&] { (void)minera::monte_carlo(net, options); }));
}

void sparse_stochastic_map_controls_branch_and_duration() {
  const auto net = sequential_net();
  minera::StochasticMap sparse;
  sparse.transitions.push_back(minera::StochasticMapTransition{
      0, minera::StochasticMapTiming::kTimed,
      minera::StochasticMapDistribution::kDeterministic, 2.0, 0.0, 0.0,
      1.0, 0});
  minera::MonteCarloOptions options;
  options.simulations = 1;
  options.case_arrival_seconds = 0.0;
  options.stochastic_map = sparse;
  const auto result = minera::monte_carlo(net, options);
  assert(result.diagnostics.completed_cases == 1);
  assert(result.log.traces.front().events.size() == 2);
  assert(result.log.traces.front().events[0].timestamp ==
         "2026-01-01T00:00:02.000Z");
  // The missing second map record has an immediate duration.
  assert(result.log.traces.front().events[1].timestamp ==
         "2026-01-01T00:00:02.000Z");

  const auto branch_net = choice_net();
  minera::StochasticMap branch_map;
  branch_map.transitions.push_back(minera::StochasticMapTransition{
      0, minera::StochasticMapTiming::kImmediate,
      minera::StochasticMapDistribution::kConstant0, 0.0, 0.0, 0.0, 0.0,
      0});
  branch_map.transitions.push_back(minera::StochasticMapTransition{
      1, minera::StochasticMapTiming::kTimed,
      minera::StochasticMapDistribution::kDeterministic, 4.0, 0.0, 0.0, 1.0,
      0});
  options.stochastic_map = branch_map;
  options.force_distribution =
      minera::MonteCarloDurationDistribution::kUniform;
  options.transition_weights.clear();
  options.transition_durations.clear();
  const auto branch = minera::monte_carlo(branch_net, options);
  assert(branch.log.traces.front().events.size() == 1);
  assert(branch.log.traces.front().events.front().activity == "B");
  assert(branch.log.traces.front().events.front().timestamp ==
         "2026-01-01T00:00:04.000Z");

  // Explicit vectors override corresponding map values.
  options.transition_weights = {0.0, 1.0};
  options.transition_durations = {
      {minera::MonteCarloDurationDistribution::kDeterministic, 3.0},
      {minera::MonteCarloDurationDistribution::kDeterministic, 7.0}};
  const auto explicit_values = minera::monte_carlo(branch_net, options);
  assert(explicit_values.log.traces.front().events.front().activity == "B");
  assert(explicit_values.log.traces.front().events.front().timestamp ==
         "2026-01-01T00:00:07.000Z");

  auto malformed = branch_map;
  malformed.transitions.push_back(malformed.transitions.front());
  options.transition_weights.clear();
  options.transition_durations.clear();
  options.stochastic_map = malformed;
  assert(throws_invalid([&] {
    (void)minera::monte_carlo(branch_net, options);
  }));
  malformed = branch_map;
  malformed.transitions.front().transition = 99;
  options.stochastic_map = malformed;
  assert(throws_invalid([&] {
    (void)minera::monte_carlo(branch_net, options);
  }));
  malformed = branch_map;
  malformed.transitions.front().distribution =
      minera::StochasticMapDistribution::kUniform;
  malformed.transitions.front().parameter = 3.0;
  malformed.transitions.front().secondary_parameter = 2.0;
  malformed.transitions.front().timing = minera::StochasticMapTiming::kTimed;
  options.stochastic_map = malformed;
  assert(throws_invalid([&] {
    (void)minera::monte_carlo(branch_net, options);
  }));
}

void stochastic_map_distribution_parameters_are_converted() {
  const auto net = linear_net();
  minera::MonteCarloOptions options;
  options.simulations = 1;
  options.case_arrival_ratio = 0.0;
  options.stochastic_map = minera::StochasticMap{};

  auto map = minera::StochasticMap{};
  map.transitions.push_back(minera::StochasticMapTransition{
      0, minera::StochasticMapTiming::kTimed,
      minera::StochasticMapDistribution::kUniform, 2.0, 4.0, 0.0, 1.0, 0});
  options.stochastic_map = map;
  const auto uniform = minera::monte_carlo(net, options);
  const auto uniform_time =
      minera::parse_timestamp(uniform.log.traces.front().events.front().timestamp);
  assert(uniform_time.has_value());
  assert(*uniform_time >= 1767225602.0 && *uniform_time <= 1767225604.0);

  map.transitions.front().distribution =
      minera::StochasticMapDistribution::kLogNormal;
  map.transitions.front().parameter = std::log(2.0);
  map.transitions.front().secondary_parameter = 0.0;
  options.stochastic_map = map;
  const auto lognormal = minera::monte_carlo(net, options);
  assert(lognormal.log.traces.front().events.front().timestamp ==
         "2026-01-01T00:00:02.000Z");

  // A negative log-scale location is valid and produces a sub-unit service
  // time when the supplied map uses a finite negative mu.
  map.transitions.front().parameter = std::log(0.5);
  options.stochastic_map = map;
  const auto subunit_lognormal = minera::monte_carlo(net, options);
  assert(subunit_lognormal.log.traces.front().events.front().timestamp ==
         "2026-01-01T00:00:00.500Z");

  map.transitions.front().distribution =
      minera::StochasticMapDistribution::kGamma;
  map.transitions.front().parameter = 3.0;       // scale
  map.transitions.front().secondary_parameter = 2.0;  // shape
  options.stochastic_map = map;
  const auto gamma = minera::monte_carlo(net, options);
  assert(gamma.diagnostics.case_execution_seconds.front() >= 0.0);
}

void arrival_inference_and_custom_selectors_round_trip() {
  const auto net = sequential_net();
  minera::EventLog input;
  const auto two_digits = [](int value) {
    return value < 10 ? "0" + std::to_string(value) : std::to_string(value);
  };
  for (const auto& start : {10, 20, 50}) {
    minera::Trace trace;
    trace.case_id = "source_" + std::to_string(start);
    minera::Event first{trace.case_id, "wrong", ""};
    add_attribute(first, "task", "A");
    add_attribute(first, "when", "2026-01-01T00:00:" + two_digits(start) +
                                "Z",
                  minera::AttributeKind::kDate);
    minera::Event second{trace.case_id, "wrong", ""};
    add_attribute(second, "task", "B");
    const int finish = start + 50;
    add_attribute(second, "when", "2026-01-01T00:" +
                                  two_digits(finish / 60) + ":" +
                                  two_digits(finish % 60) + "Z",
                  minera::AttributeKind::kDate);
    trace.events = {std::move(first), std::move(second)};
    input.traces.push_back(std::move(trace));
  }

  minera::MonteCarloOptions options;
  options.activity_key = "task";
  options.timestamp_key = "when";
  const auto fitted = minera::fit_monte_carlo_options(input, net, options);
  assert(fitted.case_arrival_ratio.has_value());
  assert(*fitted.case_arrival_ratio == 20.0);
  assert(!fitted.case_arrival_seconds.has_value());
  assert(fitted.transition_durations.size() == 2);
  assert(fitted.transition_durations[0].value_seconds == 50.0);
  assert(fitted.transition_weights.size() == 2);
  assert(fitted.transition_weights[0] == 3.0);
  assert(fitted.transition_weights[1] == 3.0);

  options.simulations = 1;
  options.transition_durations = {
      {minera::MonteCarloDurationDistribution::kDeterministic, 1.0},
      {minera::MonteCarloDurationDistribution::kDeterministic, 1.0}};
  const auto output = minera::monte_carlo(input, net, options);
  assert(output.diagnostics.case_arrival_seconds == 20.0);
  const auto& event = output.log.traces.front().events.front();
  const auto* activity_alias = find_attribute(event, "task");
  const auto* timestamp_alias = find_attribute(event, "when");
  assert(activity_alias != nullptr && activity_alias->value == "A");
  assert(activity_alias->kind == minera::AttributeKind::kString);
  assert(timestamp_alias != nullptr);
  assert(timestamp_alias->kind == minera::AttributeKind::kDate);
  assert(timestamp_alias->value == event.timestamp);

  // The generated aliases must remain available after a real XES
  // serialization/import round trip, not only while the result is in memory.
  const auto xml = minera::to_xes(output.log, "Monte Carlo aliases");
  std::istringstream xes_input(xml);
  const auto restored = minera::import_xes_event_log(xes_input);
  const auto& restored_event = restored.traces.front().events.front();
  const auto* restored_activity = find_attribute(restored_event, "task");
  const auto* restored_timestamp = find_attribute(restored_event, "when");
  assert(restored_activity != nullptr);
  assert(restored_activity->value == restored_event.activity);
  assert(restored_timestamp != nullptr);
  assert(restored_timestamp->kind == minera::AttributeKind::kDate);
  assert(restored_timestamp->value == restored_event.timestamp);

  options.case_arrival_ratio = 7.0;
  const auto explicit_ratio = minera::fit_monte_carlo_options(input, net, options);
  assert(*explicit_ratio.case_arrival_ratio == 7.0);
  options.case_arrival_ratio.reset();
  options.case_arrival_seconds = 8.0;
  const auto legacy = minera::fit_monte_carlo_options(input, net, options);
  assert(*legacy.case_arrival_seconds == 8.0);
  options.case_arrival_ratio = 7.0;
  assert(throws_invalid([&] {
    (void)minera::fit_monte_carlo_options(input, net, options);
  }));
}

void replay_selector_changes_fitted_activation_counts() {
  const auto net = duplicate_activity_net();
  minera::EventLog input;
  minera::Trace trace;
  trace.case_id = "replay";
  trace.events = {{trace.case_id, "A", "2026-01-01T00:00:00Z"}};
  input.traces.push_back(std::move(trace));

  minera::MonteCarloOptions forward;
  forward.token_replay_variant = minera::TokenReplayVariant::kForward;
  const auto f = minera::fit_monte_carlo_options(input, net, forward);
  minera::MonteCarloOptions backwards = forward;
  backwards.token_replay_variant = minera::TokenReplayVariant::kBackwards;
  const auto b = minera::fit_monte_carlo_options(input, net, backwards);
  assert(f.transition_weights.size() == 3);
  // Public forward replay fixes the greatest disabled duplicate name before
  // routing, so t2 receives the activation rather than t1.
  assert((f.transition_weights == std::vector<double>{1.0, 0.0, 1.0}));
  // The backwards variation interrupts on the ambiguous duplicate label,
  // leaving no activation-weight fit; forward replay selects one branch.
  assert(b.transition_weights.empty());
}

void compatibility_controls_validate_without_runtime_effects() {
  const auto net = linear_net();
  minera::MonteCarloOptions options;
  options.simulations = 1;
  options.transition_durations = {
      {minera::MonteCarloDurationDistribution::kDeterministic, 0.0}};
  options.diagnostic_interval = 0;
  assert(throws_invalid([&] { (void)minera::monte_carlo(net, options); }));
  options.diagnostic_interval = 32;
  options.small_scale_factor = std::numeric_limits<double>::quiet_NaN();
  assert(throws_invalid([&] { (void)minera::monte_carlo(net, options); }));
  options.small_scale_factor = 864000.0;
  options.max_thread_execution_time = 0.0;
  assert(throws_invalid([&] { (void)minera::monte_carlo(net, options); }));
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
  assert(json.find("output_places_interval_trees") != std::string::npos);
  assert(json.find("output_transitions_interval_trees") != std::string::npos);
  assert(json.find("case_arrival_ratio") != std::string::npos);
  assert(json.find("cases_ex_time") != std::string::npos);
  auto malformed = result;
  ++malformed.diagnostics.completed_cases;
  assert(throws_invalid([&] { (void)minera::monte_carlo_json(malformed); }));
  malformed = result;
  malformed.output_places_interval_trees.front().place_index = 1;
  assert(throws_invalid([&] { (void)minera::monte_carlo_json(malformed); }));
}

}  // namespace

int main() {
  deterministic_seed_and_diagnostics();
  weighted_choices_and_exponential_replay();
  source_log_fits_missing_weights_and_durations();
  source_log_forces_duration_distributions();
  scalar_duration_distributions_are_seeded_and_nonnegative();
  resources_create_virtual_fifo_wait();
  reset_arcs_close_only_the_firing_cases_tokens();
  scheduler_budget_limits_reject_unbounded_allocations();
  runtime_growth_budgets_reject_amplifying_loops();
  deadlocks_and_step_limits_are_reported();
  explicit_markings_and_componentwise_completion();
  sparse_stochastic_map_controls_branch_and_duration();
  stochastic_map_distribution_parameters_are_converted();
  arrival_inference_and_custom_selectors_round_trip();
  replay_selector_changes_fitted_activation_counts();
  compatibility_controls_validate_without_runtime_effects();
  invalid_options_are_rejected();
  diagnostics_can_be_disabled();
  json_contract_is_stable();
}
