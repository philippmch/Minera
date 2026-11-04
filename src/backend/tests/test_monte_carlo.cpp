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

const minera::StochasticMapTransition& fitted_record(
    const minera::MonteCarloOptions& options, std::size_t transition) {
  assert(options.stochastic_map.has_value());
  for (const auto& record : options.stochastic_map->transitions)
    if (record.transition == transition) return record;
  throw std::logic_error("missing fitted transition");
}

std::vector<double> fitted_weights(const minera::MonteCarloOptions& options,
                                   std::size_t count) {
  assert(options.stochastic_map.has_value());
  std::vector<double> weights(count, 0.0);
  for (const auto& record : options.stochastic_map->transitions)
    weights[record.transition] = record.activation_weight;
  return weights;
}

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
         "2026-01-01T00:00:05.000000Z");
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

void signed_activation_weights_normalize_enabled_sets() {
  const auto net = choice_net();
  minera::MonteCarloOptions options;
  options.simulations = 2000;
  options.max_steps = 4;
  options.seed = 731;
  options.default_duration_seconds = 0.0;
  const auto fraction_a = [](const minera::MonteCarloResult& result) {
    std::size_t count = 0;
    for (const auto& trace : result.log.traces) {
      assert(trace.events.size() == 1);
      count += trace.events.front().activity == "A";
    }
    return static_cast<double>(count) / result.log.traces.size();
  };
  for (const auto& weights : {std::vector<double>{-1.0, -3.0},
                              std::vector<double>{-1e-320, -3e-320},
                              std::vector<double>{1e-320, 3e-320}}) {
    options.transition_weights = weights;
    assert(std::abs(fraction_a(minera::monte_carlo(net, options)) - 0.25) < 0.05);
  }
  for (const auto& weights : {std::vector<double>{-1.0, 1.0},
                              std::vector<double>{0.0, 0.0}}) {
    options.transition_weights = weights;
    assert(std::abs(fraction_a(minera::monte_carlo(net, options)) - 0.5) < 0.05);
  }
  options.transition_weights = {-1.0, 0.0};
  assert(fraction_a(minera::monte_carlo(net, options)) == 1.0);
  auto three_way = choice_net();
  three_way.set_activities({"A", "B", "C"});
  three_way.add_transition(2, {three_way.source()}, {three_way.sink()});
  options.transition_weights = {-1.0, -1.0, 2.0};
  const auto cancelled = minera::monte_carlo(three_way, options);
  std::array<std::size_t, 3> counts{};
  for (const auto& trace : cancelled.log.traces)
    ++counts[trace.events.front().activity.front() - 'A'];
  for (const auto count : counts)
    assert(std::abs(static_cast<double>(count) / options.simulations - 1.0 / 3.0) < 0.05);

  minera::StochasticMap map;
  map.transitions.push_back({0, minera::StochasticMapTiming::kImmediate,
      minera::StochasticMapDistribution::kConstant0, 0.0, 0.0, 0.0, -1.0, 0});
  options.stochastic_map = map;
  options.transition_weights.clear();
  // The absent record defaults to +1, cancelling the supplied -1.
  assert(std::abs(fraction_a(minera::monte_carlo(net, options)) - 0.5) < 0.05);
  map.transitions.push_back({1, minera::StochasticMapTiming::kImmediate,
      minera::StochasticMapDistribution::kConstant0, 0.0, 0.0, 0.0, -3.0, 0});
  options.stochastic_map = map;
  assert(std::abs(fraction_a(minera::monte_carlo(net, options)) - 0.25) < 0.05);
  options.stochastic_map->transitions[1].activation_weight = 2.0;
  options.transition_weights = {0.0, -1.0};
  assert(fraction_a(minera::monte_carlo(net, options)) == 0.0);
  // Unused map weights remain subject to finite metadata validation.
  options.stochastic_map->transitions[0].activation_weight =
      std::numeric_limits<double>::infinity();
  assert(throws_invalid([&] { (void)minera::monte_carlo(net, options); }));
  options.stochastic_map.reset();

  for (const auto& weights : {std::vector<double>{-1.0, 2.0},
                              std::vector<double>{-2.0, 1.0},
                              std::vector<double>{1e308, 1e308},
                              std::vector<double>{-1e308, -1e308}}) {
    options.transition_weights = weights;
    assert(throws_invalid([&] { (void)minera::monte_carlo(net, options); }));
    // These values are individually valid when enabled in separate stages.
    const auto sequential = minera::monte_carlo(sequential_net(), options);
    assert(sequential.diagnostics.completed_cases == options.simulations);
    for (const auto& trace : sequential.log.traces) {
      assert(trace.events.size() == 2);
      assert(trace.events[0].activity == "A" && trace.events[1].activity == "B");
    }
  }
  for (const double invalid : {std::numeric_limits<double>::infinity(),
                               std::numeric_limits<double>::quiet_NaN()}) {
    options.transition_weights = {1.0, invalid};
    options.max_steps = 1;  // The invalid second transition is never reached.
    assert(throws_invalid([&] {
      (void)minera::monte_carlo(sequential_net(), options);
    }));
  }
  // Invalid normalization is deferred until the conflicting set is enabled.
  auto staged = sequential_net();
  staged.set_activities({"A", "B", "C"});
  staged.add_transition(2, {1}, {staged.sink()});
  for (const auto& weights : {std::vector<double>{1.0, -1.0, 2.0},
                              std::vector<double>{1.0, 1e308, 1e308}}) {
    options.transition_weights = weights;
    options.max_steps = 1;
    assert(minera::monte_carlo(staged, options).diagnostics.step_limited_cases ==
           options.simulations);
    options.max_steps = 2;
    assert(throws_invalid([&] { (void)minera::monte_carlo(staged, options); }));
  }
}

// The inferred arrival ratio averages pandas Timedelta.total_seconds() of the
// sorted case-start gaps (whole seconds + microseconds / 10**6), not gaps of
// float epochs. Expected value from pm4py's Monte Carlo on the same CSV
// (case_arrival_ratio 10.213296333333332; a Python-timedelta mean of exact
// microseconds / 10**6 gives 10.213296333333334).
void inferred_arrival_uses_exact_subsecond_gaps() {
  const auto net = linear_net();
  minera::EventLog input;
  for (const char* start : {"2020-01-01 00:00:25.027945", "2020-01-01 00:00:00.123457",
                            "2020-01-01 00:00:30.763346", "2020-01-01 00:00:02.626124"}) {
    minera::Trace trace;
    trace.case_id = start;
    trace.events = {minera::Event{trace.case_id, "A", start}};
    input.traces.push_back(std::move(trace));
  }
  minera::MonteCarloOptions options;
  options.transition_weights = {1.0};
  options.transition_durations = {{minera::MonteCarloDurationDistribution::kDeterministic, 1.0}};
  const auto fitted = minera::fit_monte_carlo_options(input, net, options);
  assert(fitted.case_arrival_ratio && *fitted.case_arrival_ratio == 0x1.46d352d500bf8p+3);
  // One gap where pandas' sum and Python timedelta's division differ.
  input.traces.resize(1);
  minera::Trace second;
  second.case_id = "late";
  second.events = {minera::Event{"late", "A", "2020-01-01 00:00:47.429766"}};
  input.traces.push_back(second);
  const auto pair = minera::fit_monte_carlo_options(input, net, options);
  assert(pair.case_arrival_ratio && *pair.case_arrival_ratio == 22.0 + 401821 / 1e6);
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
  assert(fitted.transition_weights.empty());
  assert(fitted.transition_durations.empty());
  assert(fitted_record(fitted, 0).distribution == minera::StochasticMapDistribution::kImmediate);
  assert(fitted_record(fitted, 1).distribution == minera::StochasticMapDistribution::kExponential);
  assert(fitted_record(fitted, 1).parameter == 0.25);
  assert((fitted_weights(fitted, 2) == std::vector<double>{1.0, 1.0}));

  minera::MonteCarloOptions options;
  options.simulations = 1;
  options.resources_per_place[net.sink()] = 1;
  const auto result = minera::monte_carlo(input, net, options);
  assert(result.diagnostics.completed_cases == 1);
  assert(result.diagnostics.case_execution_seconds.size() == 1);
  auto control = options;
  control.stochastic_map = fitted.stochastic_map;
  const auto expected = minera::monte_carlo(net, control);
  assert_same_logs(result.log, expected.log);
  assert(result.diagnostics.case_execution_seconds == expected.diagnostics.case_execution_seconds);
}

void source_log_fitting_preserves_sparse_causal_records_and_overrides() {
  const auto net = sequential_net();
  minera::EventLog input;
  for (int seconds : {2, 4}) {
    minera::Trace trace;
    trace.case_id = std::to_string(seconds);
    trace.events = {{trace.case_id, "A", "2026-01-01T00:00:00Z"},
                    {trace.case_id, "B", "2026-01-01T00:00:0" + std::to_string(seconds) + "Z"}};
    input.traces.push_back(trace);
  }
  minera::MonteCarloOptions options;
  options.initial_marking = {1, 1, 0};
  options.final_marking = {0, 1, 1};
  options.force_distribution = minera::MonteCarloDurationDistribution::kExponential;
  const auto fitted = minera::fit_monte_carlo_options(input, net, options);
  assert(fitted_record(fitted, 1).activation_weight == 0.5);
  assert(fitted_record(fitted, 1).parameter == 1.0 / 3.0);
  options.transition_weights = {7.0, 9.0};
  const auto weights_only = minera::fit_monte_carlo_options(input, net, options);
  assert(weights_only.transition_weights == options.transition_weights);
  assert(weights_only.transition_durations.empty());
  assert(fitted_record(weights_only, 1).parameter == 1.0 / 3.0);
  options.transition_weights.clear();
  options.transition_durations = {{minera::MonteCarloDurationDistribution::kDeterministic, 7.0},
                                  {minera::MonteCarloDurationDistribution::kDeterministic, 9.0}};
  const auto durations_only = minera::fit_monte_carlo_options(input, net, options);
  assert(durations_only.transition_durations[1].value_seconds == 9.0);
  assert(fitted_record(durations_only, 1).activation_weight == 0.5);

  options = {};
  input.traces.resize(1);
  input.traces[0].events.resize(1);
  const auto partial = minera::fit_monte_carlo_options(input, net, options);
  assert(partial.stochastic_map && partial.stochastic_map->transitions.size() == 1);
  assert(partial.stochastic_map->transitions.front().transition == 0);
  const auto empty = minera::fit_monte_carlo_options({}, net, options);
  assert(empty.stochastic_map && empty.stochastic_map->transitions.empty());
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
  assert(fitted_record(normal, 1).distribution == minera::StochasticMapDistribution::kNormal);
  assert(fitted_record(normal, 1).parameter == 4.0);
  assert(std::abs(fitted_record(normal, 1).secondary_parameter - std::sqrt(8.0 / 3.0)) < 1e-9);

  options.force_distribution =
      minera::MonteCarloDurationDistribution::kUniform;
  const auto uniform = minera::fit_monte_carlo_options(input, net, options);
  assert(fitted_record(uniform, 1).distribution == minera::StochasticMapDistribution::kUniform);
  assert(fitted_record(uniform, 1).parameter == 2.0);
  assert(fitted_record(uniform, 1).secondary_parameter == 6.0);

  options.force_distribution =
      minera::MonteCarloDurationDistribution::kExponential;
  const auto exponential = minera::fit_monte_carlo_options(input, net, options);
  assert(fitted_record(exponential, 1).distribution == minera::StochasticMapDistribution::kExponential);
  assert(fitted_record(exponential, 1).parameter == 0.25);

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
  assert(fitted_record(fallback, 0).distribution == minera::StochasticMapDistribution::kImmediate);
  assert(fitted_record(fallback, 1).distribution == minera::StochasticMapDistribution::kImmediate);

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

void resources_record_waits_without_delaying_events() {
  const auto net = linear_net();
  minera::MonteCarloOptions options;
  options.simulations = 2;
  options.case_arrival_seconds = 0.0;
  options.default_duration_seconds = 5.0;
  const auto result = minera::monte_carlo(net, options);
  for (const auto& trace : result.log.traces)
    assert(trace.events.front().timestamp == "1970-01-12T13:46:45.000000Z");
  assert(result.diagnostics.case_execution_seconds == (std::vector<double>{5, 5}));
  assert(result.diagnostics.cases_ex_time == (std::vector<double>{0, 0}));
  assert(result.diagnostics.total_cases_time == 0);
  assert(result.output_places_interval_trees[0].intervals ==
         (std::vector<minera::MonteCarloInterval>{{1000000, 1000005}}));
  assert(result.output_transitions_interval_trees[0].intervals ==
         (std::vector<minera::MonteCarloInterval>{{1000000, 1000005}}));
}

void reset_arcs_preserve_case_markings_with_shared_timing() {
  const auto net = reset_net();
  minera::MonteCarloOptions options;
  options.simulations = 2;
  options.case_arrival_ratio = 2.0;
  options.initial_marking = {1, 2, 0};
  options.default_duration_seconds = 5.0;
  options.resources_per_place[net.sink()] = 2;
  const auto result = minera::monte_carlo(net, options);
  assert(result.diagnostics.completed_cases == 2);
  assert(result.output_places_interval_trees[1].intervals ==
         (std::vector<minera::MonteCarloInterval>{{1000005, 1000007}}));
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
  // Token multiplicity no longer multiplies timestamp allocations.
  assert(minera::monte_carlo(net, options).diagnostics.completed_cases == 2);

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
  options.default_resources_per_place = 0;
  options.simulations = 1000;
  options.max_steps = 1;
  assert(throws_invalid(
      [&] { (void)minera::monte_carlo(many_places, options); }));
}

void runtime_growth_budgets_reject_amplifying_loops() {
  // Distinct places grow shared queues; weighted copies of one arc do not.
  for (bool interval_growth : {false, true}) {
    minera::PetriNet net;
    std::vector<std::size_t> places;
    for (std::size_t i = 0; i < 999; ++i) places.push_back(net.add_place());
    const auto sink = net.add_place();
    net.set_source(places.front());
    net.set_sink(sink);
    net.set_activities({"A"});
    net.add_transition(0, interval_growth ? places : std::vector<std::size_t>{0}, places);
    minera::MonteCarloOptions options;
    options.simulations = 1;
    options.max_steps = 10000;
    options.default_duration_seconds = interval_growth ? 1.0 : 0.0;
    options.default_resources_per_place = interval_growth ? 1 : 1000;
    if (interval_growth) {
      options.initial_marking.assign(net.place_count(), 1);
      options.initial_marking[sink] = 0;
    }
    bool rejected = false;
    try { (void)minera::monte_carlo(net, options); }
    catch (const std::runtime_error&) { rejected = true; }
    assert(rejected);
  }
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
  options.start = 1767225600.0;
  options.simulations = 1;
  options.case_arrival_seconds = 0.0;
  options.stochastic_map = sparse;
  const auto result = minera::monte_carlo(net, options);
  assert(result.diagnostics.completed_cases == 1);
  assert(result.log.traces.front().events.size() == 2);
  assert(result.log.traces.front().events[0].timestamp ==
         "2026-01-01T00:00:02.000000Z");
  // The missing second map record has an immediate duration.
  assert(result.log.traces.front().events[1].timestamp ==
         "2026-01-01T00:00:02.000000Z");

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
         "2026-01-01T00:00:04.000000Z");

  // Explicit vectors override corresponding map values.
  options.transition_weights = {0.0, 1.0};
  options.transition_durations = {
      {minera::MonteCarloDurationDistribution::kDeterministic, 3.0},
      {minera::MonteCarloDurationDistribution::kDeterministic, 7.0}};
  const auto explicit_values = minera::monte_carlo(branch_net, options);
  assert(explicit_values.log.traces.front().events.front().activity == "B");
  assert(explicit_values.log.traces.front().events.front().timestamp ==
         "2026-01-01T00:00:07.000000Z");

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
  options.start = 1767225600.0;
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
         "2026-01-01T00:00:02.000000Z");

  // A negative log-scale location is valid and produces a sub-unit service
  // time when the supplied map uses a finite negative mu.
  map.transitions.front().parameter = std::log(0.5);
  options.stochastic_map = map;
  const auto subunit_lognormal = minera::monte_carlo(net, options);
  assert(subunit_lognormal.log.traces.front().events.front().timestamp ==
         "2026-01-01T00:00:00.500000Z");

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
  assert(fitted.transition_durations.empty());
  assert(fitted_record(fitted, 1).distribution == minera::StochasticMapDistribution::kExponential);
  assert(fitted_record(fitted, 1).parameter == 1.0 / 50.0);
  assert((fitted_weights(fitted, 2) == std::vector<double>{1.0, 1.0}));

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
  assert(f.stochastic_map.has_value());
  // Public forward replay fixes the greatest disabled duplicate name before
  // routing, so t2 receives the activation rather than t1.
  assert((fitted_weights(f, 3) == std::vector<double>{1.0, 0.0, 1.0}));
  // The backwards variation interrupts on the ambiguous duplicate label,
  // leaving no activation-weight fit; forward replay selects one branch.
  assert(b.stochastic_map && b.stochastic_map->transitions.empty());
}

void signed_fitted_metadata_retains_simulation_domain_validation() {
  const auto net = sequential_net();
  minera::EventLog input;
  for (int end : {6, 8}) {
    minera::Trace trace;
    trace.case_id = std::to_string(end);
    trace.events = {{trace.case_id, "A", "2026-01-01T00:00:10Z"},
                    {trace.case_id, "B", "2026-01-01T00:00:0" + std::to_string(end) + "Z"}};
    input.traces.push_back(trace);
  }
  minera::MonteCarloOptions options;
  options.simulations = 1;
  options.force_distribution = minera::MonteCarloDurationDistribution::kNormal;
  const auto normal = minera::fit_monte_carlo_options(input, net, options);
  assert(fitted_record(normal, 1).distribution == minera::StochasticMapDistribution::kNormal);
  assert(fitted_record(normal, 1).parameter == -3.0);
  assert(fitted_record(normal, 1).secondary_parameter == 1.0);
  assert(minera::monte_carlo(net, normal).diagnostics.completed_cases == 1);
  options.force_distribution = minera::MonteCarloDurationDistribution::kUniform;
  const auto uniform = minera::fit_monte_carlo_options(input, net, options);
  assert(fitted_record(uniform, 1).parameter == -4.0);
  assert(fitted_record(uniform, 1).secondary_parameter == -2.0);
  assert(throws_invalid([&] { (void)minera::monte_carlo(net, uniform); }));
  for (auto overridden : {normal, uniform}) {
    overridden.transition_durations = {
        {minera::MonteCarloDurationDistribution::kDeterministic, 2.0},
        {minera::MonteCarloDurationDistribution::kDeterministic, 3.0}};
    const auto result = minera::monte_carlo(net, overridden);
    auto control = overridden;
    control.stochastic_map.reset();
    const auto expected = minera::monte_carlo(net, control);
    assert(result.diagnostics.completed_cases == 1);
    assert(result.diagnostics.case_execution_seconds.front() == 5.0);
    assert_same_logs(result.log, expected.log);
    // Metadata validation remains active even when its durations are unused.
    overridden.stochastic_map->transitions.front().parameter =
        std::numeric_limits<double>::quiet_NaN();
    assert(throws_invalid([&] { (void)minera::monte_carlo(net, overridden); }));
  }
  // A supplied signed map is valid metadata even though simulation rejects it.
  assert(minera::fit_monte_carlo_options({}, net, uniform).stochastic_map.has_value());
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

void fitted_weights_preserve_supplied_replay_context() {
  auto net = duplicate_activity_net();
  // Both A transitions are disabled at the workflow source; original IDs
  // determine which duplicate receives the activation after hidden routing.
  minera::EventLog input;
  minera::Trace trace;
  trace.case_id = "marked";
  trace.events = {{"marked", "A", "2026-01-01T00:00:00Z"}};
  input.traces.push_back(trace);
  const auto check = [&](const minera::MonteCarloOptions& options) {
    minera::TokenReplayOptions replay;
    replay.variant = options.token_replay_variant;
    replay.activity_attribute = options.activity_key;
    replay.initial_marking.assign(options.initial_marking.begin(), options.initial_marking.end());
    replay.final_marking.assign(options.final_marking.begin(), options.final_marking.end());
    replay.place_names = options.place_names;
    replay.transition_names = options.transition_names;
    replay.initial_marking_keys = options.initial_marking_keys;
    replay.final_marking_keys = options.final_marking_keys;
    minera::StochasticMapFitOptions expected_options;
    expected_options.replay = replay;
    const auto expected = minera::fit_stochastic_map_with_report(input, net, expected_options).map;
    const auto fitted = minera::fit_monte_carlo_options(input, net, options);
    assert(fitted.stochastic_map.has_value());
    assert(minera::serialize_stochastic_map_json(*fitted.stochastic_map) ==
           minera::serialize_stochastic_map_json(expected));
    return fitted_weights(fitted, net.transitions().size());
  };
  minera::MonteCarloOptions options;
  options.place_names = {"source-id", "gate-id", "sink-id"};
  options.transition_names = {"route", "z-original", "a-original"};
  options.initial_marking = {1, 0, 0};
  options.final_marking = {0, 0, 1};
  options.initial_marking_keys = {true, false, false};
  options.final_marking_keys = {false, false, true};
  assert((check(options) == std::vector<double>{1.0, 1.0, 0.0}));
  options.transition_names.clear();
  assert((check(options) == std::vector<double>{1.0, 0.0, 1.0}));
  // A supplied gate marking makes both duplicates immediately enabled.
  options.initial_marking = {0, 2, 0};
  options.initial_marking_keys = {false, true, true};
  options.final_marking = {0, 0, 0};
  check(options);
  options.token_replay_variant = minera::TokenReplayVariant::kBackwards;
  check(options);

  // Distinct enabled branches make the numeric marking's effect observable.
  net = minera::PetriNet{};
  net.add_place(); net.add_place(); net.add_place();
  net.set_source(0); net.set_sink(2); net.set_activities({"A"});
  net.add_transition(0, {0}, {2});
  net.add_transition(0, {1}, {2});
  options = {};
  assert((check(options) == std::vector<double>{1.0, 0.0}));
  options.initial_marking = {0, 1, 0};
  options.final_marking = {0, 0, 2};
  assert((check(options) == std::vector<double>{0.0, 1.0}));
}

void replay_context_validation_is_limited_to_map_fitting() {
  const auto net = linear_net();
  minera::EventLog input;
  minera::Trace trace;
  trace.case_id = "validation";
  trace.events = {{"validation", "A", "2026-01-01T00:00:00Z"}};
  input.traces.push_back(trace);
  const auto reject_only_fitting = [&](minera::MonteCarloOptions options) {
    assert(throws_invalid([&] { (void)minera::fit_monte_carlo_options(input, net, options); }));
    // Explicit durations alone still require activation-weight replay.
    options.transition_durations = {{minera::MonteCarloDurationDistribution::kDeterministic, 7.0}};
    assert(throws_invalid([&] { (void)minera::fit_monte_carlo_options(input, net, options); }));
    options.transition_weights = {3.0};
    options.transition_durations.clear();
    assert(throws_invalid([&] { (void)minera::fit_monte_carlo_options(input, net, options); }));
    options.transition_durations = {{minera::MonteCarloDurationDistribution::kDeterministic, 7.0}};
    const auto fitted = minera::fit_monte_carlo_options(input, net, options);
    assert(fitted.transition_weights == options.transition_weights);
    assert(fitted.transition_durations.front().value_seconds == 7.0);
    options.transition_weights.clear();
    options.stochastic_map = minera::StochasticMap{};
    assert(minera::fit_monte_carlo_options(input, net, options).stochastic_map.has_value());
    options.stochastic_map.reset();
    (void)minera::fit_monte_carlo_options({}, net, options);
  };
  minera::MonteCarloOptions options;
  options.initial_marking = {1}; reject_only_fitting(options);
  options = {}; options.final_marking = {1}; reject_only_fitting(options);
  options = {}; options.place_names = {"one"}; reject_only_fitting(options);
  options = {}; options.transition_names = {"one", "two"}; reject_only_fitting(options);
  options = {}; options.initial_marking = {std::numeric_limits<std::uint32_t>::max(), 0};
  reject_only_fitting(options);
  options = {}; options.final_marking = {0, std::numeric_limits<std::uint32_t>::max()};
  reject_only_fitting(options);
  // The simulator retains its wider unsigned final-marking range when replay
  // is unnecessary (initial-token allocation has its own independent budget).
  options.simulations = 0;
  assert(throws_invalid([&] { (void)minera::monte_carlo(net, options); }));
}

void marking_membership_controls_runtime_and_is_always_validated() {
  const auto net = sequential_net();
  minera::MonteCarloOptions options;
  options.simulations = 1;
  options.initial_marking = {1,1,0};
  options.final_marking = {0,0,1};
  options.final_marking_keys = {true,false,true};
  options.transition_weights = {1,0};
  options.transition_durations = {
      {minera::MonteCarloDurationDistribution::kDeterministic,2},
      {minera::MonteCarloDurationDistribution::kDeterministic,3}};
  const auto retained_zero_requirement = minera::monte_carlo(net, options);
  const auto& events = retained_zero_requirement.log.traces.front().events;
  assert(events.size() == 3 && events[0].activity == "A" &&
         events[1].activity == "B" && events[2].activity == "B");
  assert(retained_zero_requirement.diagnostics.deadlocked_cases == 1);
  assert(retained_zero_requirement.diagnostics.cases_ex_time == std::vector<double>{6});
  options.final_marking_keys.clear();
  assert(minera::monte_carlo(net, options).log.traces.front().events.size() == 2);

  // A retained zero key before the positive source owns the arrival timestamp.
  minera::PetriNet reordered;
  for (int i = 0; i < 3; ++i) reordered.add_place();
  reordered.set_source(1); reordered.set_sink(2); reordered.set_activities({"A", "B"});
  reordered.add_transition(0, {1}, {0}); reordered.add_transition(1, {0}, {2});
  options.initial_marking = {0,1,0}; options.initial_marking_keys = {true,true,false};
  const auto zero_first = minera::monte_carlo(reordered, options);
  assert(zero_first.output_places_interval_trees[1].intervals.empty());
  assert(zero_first.output_places_interval_trees[0].intervals ==
      (std::vector<minera::MonteCarloInterval>{{1000002,1000005}}));

  for (bool supplied_map : {false,true}) {
    for (bool initial : {false,true}) {
      for (bool wrong_size : {false,true}) {
        minera::MonteCarloOptions invalid;
        invalid.simulations = 1;
        invalid.transition_weights = {1,1};
        invalid.transition_durations = options.transition_durations;
        if (supplied_map) invalid.stochastic_map = minera::StochasticMap{};
        auto& keys = initial ? invalid.initial_marking_keys : invalid.final_marking_keys;
        keys = wrong_size ? std::vector<bool>{true} : std::vector<bool>{false,false,false};
        assert(throws_invalid([&] { (void)minera::monte_carlo(net, invalid); }));
        assert(throws_invalid([&] { (void)minera::fit_monte_carlo_options({}, net, invalid); }));
      }
    }
  }
  // Name metadata remains fitting-only under the explicit-vector bypass.
  options.initial_marking_keys.clear(); options.initial_marking.clear();
  options.place_names = {"ignored"}; options.transition_names = {"ignored"};
  (void)minera::fit_monte_carlo_options({}, net, options);
  assert(minera::monte_carlo(net, options).diagnostics.completed_cases == 1);
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

  {
    minera::MonteCarloOptions options;
    options.default_resources_per_place = 0;
    assert(minera::monte_carlo(net, options).diagnostics.completed_cases == options.simulations);
  }

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
               minera::MonteCarloDurationDistribution::kGamma, 0.0, 0.0,
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
  assert(result.diagnostics.cases_ex_time == (std::vector<double>{0, 0}));
  assert(result.diagnostics.total_cases_time == 60.0);
}

void public_scheduler_sequences_and_weighted_arcs() {
  auto net = sequential_net();
  minera::MonteCarloOptions options;
  options.simulations = 3;
  options.case_arrival_ratio = 1;
  options.transition_durations = {
      {minera::MonteCarloDurationDistribution::kDeterministic, 2},
      {minera::MonteCarloDurationDistribution::kDeterministic, 3}};
  const auto result = minera::monte_carlo(net, options);
  assert(result.diagnostics.cases_ex_time == (std::vector<double>{3, 3, 3}));
  assert(result.diagnostics.total_cases_time == 5);
  assert(result.output_places_interval_trees[0].intervals ==
      (std::vector<minera::MonteCarloInterval>{{1000000,1000002}, {1000001,1000004}, {1000002,1000003}}));
  assert(result.output_places_interval_trees[1].intervals ==
      (std::vector<minera::MonteCarloInterval>{{1000004,1000005}, {1000005,1000006}, {1000006,1000007}}));
  assert(result.output_transitions_interval_trees[1].intervals ==
      (std::vector<minera::MonteCarloInterval>{{1000003,1000005}, {1000004,1000006}}));

  options.case_arrival_ratio = 10;
  const auto spaced = minera::monte_carlo(net, options);
  // Priming leaves timestamp 1000022 at the middle place before case zero
  // finishes at 1000005. Its reversed interval must be omitted, not rejected.
  assert(spaced.output_places_interval_trees[1].intervals ==
      (std::vector<minera::MonteCarloInterval>{{1000005,1000015}, {1000015,1000025}}));
  assert(spaced.diagnostics.total_cases_time == 23);

  minera::PetriNet weighted;
  for (int i = 0; i < 3; ++i) weighted.add_place();
  weighted.set_source(0); weighted.set_sink(2); weighted.set_activities({"A", "B"});
  weighted.add_transition(0, {0,0}, {1,1});
  weighted.add_transition(1, {1,1}, {2});
  options.initial_marking = {2,0,0};
  options.case_arrival_ratio = 1;
  const auto multiple = minera::monte_carlo(weighted, options);
  assert(multiple.output_places_interval_trees == result.output_places_interval_trees);
  assert(multiple.output_transitions_interval_trees == result.output_transitions_interval_trees);
}

void public_scheduler_parallel_and_hidden_timing() {
  minera::PetriNet net;
  for (int i = 0; i < 5; ++i) net.add_place();
  net.set_source(0); net.set_sink(4); net.set_activities({"A", "B", "C"});
  net.add_transition(0, {0}, {2}); net.add_transition(1, {1}, {3});
  net.add_transition(2, {2,3}, {4});
  minera::MonteCarloOptions options;
  options.simulations = 3; options.case_arrival_ratio = 1;
  options.initial_marking = {1,1,0,0,0}; options.transition_weights = {1,0,1};
  options.transition_durations = {
      {minera::MonteCarloDurationDistribution::kDeterministic, 2},
      {minera::MonteCarloDurationDistribution::kDeterministic, 3},
      {minera::MonteCarloDurationDistribution::kDeterministic, 5}};
  const auto result = minera::monte_carlo(net, options);
  assert(result.output_places_interval_trees[1].intervals ==
      (std::vector<minera::MonteCarloInterval>{{1000005,1000006}, {1000006,1000007}}));
  assert(result.diagnostics.cases_ex_time == (std::vector<double>{8,8,8}));

  minera::PetriNet hidden;
  for (int i = 0; i < 4; ++i) hidden.add_place();
  hidden.set_source(0); hidden.set_sink(3); hidden.set_activities({"A", "B"});
  hidden.add_transition(std::nullopt, {0}, {1});
  hidden.add_transition(0, {1}, {2}); hidden.add_transition(1, {2}, {3});
  options.initial_marking.clear(); options.transition_weights.clear();
  const auto hidden_result = minera::monte_carlo(hidden, options);
  assert(hidden_result.diagnostics.cases_ex_time == (std::vector<double>{5,5,5}));
  assert(hidden_result.diagnostics.total_cases_time == 7);
}

void microsecond_events_and_empty_trace_errors() {
  minera::MonteCarloOptions options;
  options.simulations = 1;
  options.transition_durations = {
      {minera::MonteCarloDurationDistribution::kDeterministic, 0.0001},
      {minera::MonteCarloDurationDistribution::kDeterministic, 0.0002}};
  const auto result = minera::monte_carlo(sequential_net(), options);
  assert(result.log.traces[0].events[0].timestamp == "1970-01-12T13:46:40.000100Z");
  assert(result.log.traces[0].events[1].timestamp == "1970-01-12T13:46:40.000300Z");
  assert(std::abs(result.diagnostics.cases_ex_time[0] - 0.00020000000949949026) < 1e-12);
  assert(minera::format_timestamp_microseconds(-0.000001) == "1969-12-31T23:59:59.999999Z");
  assert(minera::format_timestamp_microseconds(0.0000005) == "1970-01-01T00:00:00.000000Z");
  assert(minera::format_timestamp_microseconds(0.0000015) == "1970-01-01T00:00:00.000002Z");
  assert(minera::format_timestamp_microseconds(0.9999996) == "1970-01-01T00:00:01.000000Z");
  for (double invalid : {std::numeric_limits<double>::infinity(), 253402300800.0}) {
    bool rejected = false;
    try { (void)minera::format_timestamp_microseconds(invalid); }
    catch (const std::exception&) { rejected = true; }
    assert(rejected);
  }
  minera::PetriNet hidden;
  hidden.add_place(); hidden.add_place(); hidden.set_source(0); hidden.set_sink(1);
  hidden.add_transition(std::nullopt, {0}, {1});
  options.transition_durations.clear();
  bool rejected = false;
  try { (void)minera::monte_carlo(hidden, options); }
  catch (const std::runtime_error&) { rejected = true; }
  assert(rejected);
}

void inputless_transitions_require_an_initial_key() {
  minera::PetriNet net;
  net.add_place(); net.add_place(); net.set_source(0); net.set_sink(1);
  net.set_activities({"A"}); net.add_transition(0, {}, {1});
  minera::MonteCarloOptions options;
  options.simulations = 1; options.default_duration_seconds = 2;
  const auto positive = minera::monte_carlo(net, options);
  assert(positive.log.traces.front().events.front().activity == "A");
  assert(positive.diagnostics.completed_cases == 1);
  options.initial_marking = {0,0}; options.initial_marking_keys = {true,false};
  const auto zero = minera::monte_carlo(net, options);
  assert_same_logs(positive.log, zero.log);
  options.initial_marking_keys = {false,false};
  assert(throws_invalid([&] { (void)minera::monte_carlo(net, options); }));
}

void cycle_zero_capacity_and_overlapping_places() {
  minera::PetriNet net;
  for (int i = 0; i < 4; ++i) net.add_place();
  net.set_source(0); net.set_sink(3); net.set_activities({"A", "B"});
  net.add_transition(0, {0,1}, {2}); net.add_transition(1, {2}, {0,3});
  minera::MonteCarloOptions options;
  options.simulations = 1; options.initial_marking = {1,2,0,0};
  options.final_marking = {0,0,0,2};
  options.transition_durations = {
      {minera::MonteCarloDurationDistribution::kDeterministic,2},
      {minera::MonteCarloDurationDistribution::kDeterministic,3}};
  const auto one = minera::monte_carlo(net, options);
  options.default_resources_per_place = 0;
  const auto zero = minera::monte_carlo(net, options);
  assert(one.output_places_interval_trees == zero.output_places_interval_trees);
  assert(zero.output_places_interval_trees[0].intervals ==
      (std::vector<minera::MonteCarloInterval>{{1000000,1000002},{1000002,1000007}}));

  minera::PetriNet overlap;
  overlap.add_place(); overlap.add_place(); overlap.set_source(0); overlap.set_sink(1);
  overlap.set_activities({"A"}); overlap.add_transition(0, {0}, {0,1});
  options = {}; options.simulations = 3; options.case_arrival_ratio = 1;
  options.final_marking = {0,3}; options.default_duration_seconds = 0;
  const auto immediate = minera::monte_carlo(overlap, options);
  assert(immediate.diagnostics.cases_ex_time == (std::vector<double>{2,1,0}));
  for (const auto& trace : immediate.log.traces) {
    assert(trace.events.size() == 3);
    assert(trace.events.back().timestamp == "1970-01-12T13:46:42.000000Z");
  }
  assert(immediate.output_transitions_interval_trees[0].intervals ==
      (std::vector<minera::MonteCarloInterval>{{1000000,1000002},{1000001,1000002}}));
  options.default_duration_seconds = 2;
  const auto timed = minera::monte_carlo(overlap, options);
  // Two delayed output places produce one reservation interval per firing.
  assert(timed.output_transitions_interval_trees[0].intervals ==
      (std::vector<minera::MonteCarloInterval>{{1000001,1000002},{1000002,1000003},
          {1000002,1000004},{1000003,1000004},{1000004,1000005},
          {1000004,1000006},{1000005,1000006}}));
  assert(timed.diagnostics.cases_ex_time == (std::vector<double>{4,4,4}));
}

void signed_service_laws_are_conditioned_without_clamping() {
  const auto net = linear_net();
  minera::MonteCarloOptions options;
  options.start = 0; options.simulations = 1000; options.max_steps = 1;
  options.case_arrival_ratio = 0;
  options.resources_per_place[net.sink()] = options.simulations;
  options.transition_durations = {{minera::MonteCarloDurationDistribution::kNormal,-2,1}};
  const auto normal = minera::monte_carlo(net, options);
  double mean = 0;
  std::size_t below_half = 0, zeros = 0;
  for (double value : normal.diagnostics.case_execution_seconds) {
    assert(std::isfinite(value) && value >= 0);
    mean += value / options.simulations;
    below_half += value <= 0.5;
    zeros += value == 0;
  }
  // Conditional mean and CDF for N(-2,1) | X>=0. These distinguish the
  // conditional law from a clamped Gaussian's large atom at zero.
  assert(std::abs(mean - 0.3732155328228406) < 0.06);
  const double cdf = 1 - std::erfc(2.5 / std::sqrt(2.0)) / std::erfc(2 / std::sqrt(2.0));
  assert(std::abs(static_cast<double>(below_half) / options.simulations - cdf) < 0.06);
  assert(zeros == 0);

  options.transition_durations = {{minera::MonteCarloDurationDistribution::kUniform,-1,2}};
  const auto uniform = minera::monte_carlo(net, options);
  options.transition_durations = {{minera::MonteCarloDurationDistribution::kUniform,0,1}};
  const auto positive_control = minera::monte_carlo(net, options);
  assert_same_logs(uniform.log, positive_control.log);
  mean = 0;
  for (double value : uniform.diagnostics.case_execution_seconds) {
    assert(value >= 0 && value <= 1);
    mean += value / options.simulations;
  }
  assert(std::abs(mean - 0.5) < 0.05);

  // Map uniforms store upper bounds rather than widths.
  options.transition_durations.clear();
  options.stochastic_map = minera::StochasticMap{};
  options.stochastic_map->transitions.push_back({0,minera::StochasticMapTiming::kTimed,
      minera::StochasticMapDistribution::kUniform,-1,1,0,1,0});
  assert_same_logs(minera::monte_carlo(net, options).log, uniform.log);
  auto& record = options.stochastic_map->transitions.front();
  record.distribution = minera::StochasticMapDistribution::kNormal;
  record.parameter = -2; record.secondary_parameter = 1;
  assert_same_logs(minera::monte_carlo(net, options).log, normal.log);
}

void conditional_service_domains_and_extreme_tails_are_bounded() {
  const auto net = linear_net();
  minera::MonteCarloOptions options;
  options.simulations = 16; options.max_steps = 1; options.start = 0;
  options.case_arrival_ratio = 0; options.resources_per_place[net.sink()] = 16;
  for (const auto& parameters : std::vector<std::pair<double,double>>{
      {-100,1}, {-1e308,1e154}, {-std::numeric_limits<double>::max(),1},
      {-std::numeric_limits<double>::max(),std::numeric_limits<double>::denorm_min()}}) {
    options.transition_durations = {{minera::MonteCarloDurationDistribution::kNormal,
        parameters.first,parameters.second}};
    const auto result = minera::monte_carlo(net, options);
    assert(result.diagnostics.completed_cases == options.simulations);
    for (double value : result.diagnostics.case_execution_seconds)
      assert(std::isfinite(value) && value >= 0 && value < 100);
  }
  for (const auto duration : {
      minera::MonteCarloDuration{minera::MonteCarloDurationDistribution::kNormal,-2,0},
      minera::MonteCarloDuration{minera::MonteCarloDurationDistribution::kUniform,-2,0},
      minera::MonteCarloDuration{minera::MonteCarloDurationDistribution::kUniform,-2,1},
      minera::MonteCarloDuration{minera::MonteCarloDurationDistribution::kUniform,-2,2}}) {
    options.transition_durations = {duration};
    assert(throws_invalid([&] { (void)minera::monte_carlo(net, options); }));
  }
  for (const auto distribution : {minera::StochasticMapDistribution::kNormal,
                                  minera::StochasticMapDistribution::kUniform}) {
    options.stochastic_map = minera::StochasticMap{};
    options.stochastic_map->transitions.push_back({0,minera::StochasticMapTiming::kTimed,
        distribution,-2,distribution == minera::StochasticMapDistribution::kNormal ? 0.0 : -1.0,
        0,1,0});
    options.transition_durations.clear();
    assert(throws_invalid([&] { (void)minera::monte_carlo(net, options); }));
    options.transition_durations = {{minera::MonteCarloDurationDistribution::kDeterministic,3}};
    const auto overridden = minera::monte_carlo(net, options);
    for (double value : overridden.diagnostics.case_execution_seconds) assert(value == 3);
  }
}

void signed_uniform_map_preserves_original_upper_endpoint() {
  const auto net = linear_net();
  minera::MonteCarloOptions options;
  options.simulations = 16; options.max_steps = 1; options.start = 0;
  options.case_arrival_ratio = 0; options.resources_per_place[net.sink()] = 16;
  for (const auto bounds : std::vector<std::pair<double,double>>{
      {-1e16,1}, {-1e6,1e-12}, {-std::numeric_limits<double>::max(),1},
      {-std::numeric_limits<double>::max(),
                                std::numeric_limits<double>::max()}}) {
    options.stochastic_map = minera::StochasticMap{};
    options.stochastic_map->transitions.push_back({0,minera::StochasticMapTiming::kTimed,
        minera::StochasticMapDistribution::kUniform,bounds.first,bounds.second,0,1,0});
    // Finite conditional support remains valid when the original width
    // overflows. A huge accepted sample reaches the calendar boundary;
    // conversion itself must not reject its endpoints or width.
    if (bounds.second == std::numeric_limits<double>::max()) {
      options.transition_durations = {{minera::MonteCarloDurationDistribution::kDeterministic,1}};
      assert(minera::monte_carlo(net, options).diagnostics.completed_cases == options.simulations);
      options.transition_durations.clear();
      bool date_range_reached = false;
      try { (void)minera::monte_carlo(net, options); }
      catch (const std::invalid_argument& error) {
        date_range_reached = std::string(error.what()).find("timestamp") != std::string::npos;
      }
      assert(date_range_reached);
      continue;
    }
    const auto mapped = minera::monte_carlo(net, options);
    auto direct = options;
    direct.stochastic_map.reset();
    direct.transition_durations = {{minera::MonteCarloDurationDistribution::kUniform,0,bounds.second}};
    const auto expected = minera::monte_carlo(net, direct);
    assert(mapped.diagnostics.case_execution_seconds == expected.diagnostics.case_execution_seconds);
    assert_same_logs(mapped.log, expected.log);
  }
  options.stochastic_map = minera::StochasticMap{};
  options.stochastic_map->transitions.push_back({0,minera::StochasticMapTiming::kTimed,
      minera::StochasticMapDistribution::kUniform,-1e16,0,0,1,0});
  assert(throws_invalid([&] { (void)minera::monte_carlo(net, options); }));
}

void shifted_positive_service_laws_match_conditional_cdfs() {
  const auto net = linear_net();
  minera::MonteCarloOptions options;
  options.start = 0; options.simulations = 1000; options.max_steps = 1;
  options.case_arrival_ratio = 0; options.resources_per_place[net.sink()] = 1000;
  const auto check = [&](minera::MonteCarloDuration duration, double cutoff, double cdf) {
    options.transition_durations = {duration};
    const auto result = minera::monte_carlo(net, options);
    std::size_t below = 0, positive = 0;
    for (double value : result.diagnostics.case_execution_seconds) {
      assert(std::isfinite(value) && value >= 0);
      below += value <= cutoff;
      positive += value > 0;
    }
    assert(positive == options.simulations);
    assert(std::abs(static_cast<double>(below) / options.simulations - cdf) < 0.065);
  };
  // Shifted exponential is memoryless, including a far-tail cutoff.
  check({minera::MonteCarloDurationDistribution::kGamma,1,-1000,1},1,1-std::exp(-1.0));
  // Shape one-half survival is erfc(sqrt(x)).
  check({minera::MonteCarloDurationDistribution::kGamma,0.5,-0.2,1},0.5,
        1-std::erfc(std::sqrt(0.7))/std::erfc(std::sqrt(0.2)));
  // Shape two survival is exp(-x)*(1+x).
  check({minera::MonteCarloDurationDistribution::kGamma,2,-2,1},1,
        1-std::exp(-1.0)*4.0/3.0);
  const auto normal_tail = [](double z) { return 0.5*std::erfc(z/std::sqrt(2.0)); };
  check({minera::MonteCarloDurationDistribution::kLogNormal,1,-2,1},1,
        1-normal_tail(std::log(3.0))/normal_tail(std::log(2.0)));
  check({minera::MonteCarloDurationDistribution::kLogNormal,0.5,-1,2},1,
        1-normal_tail(0)/normal_tail(std::log(0.5)/0.5));
}

void shifted_service_extremes_and_zero_scales_are_bounded() {
  const auto net = linear_net();
  minera::MonteCarloOptions options;
  options.start = 0; options.simulations = 16; options.max_steps = 1;
  options.case_arrival_ratio = 0; options.resources_per_place[net.sink()] = 16;
  for (const auto duration : {
      minera::MonteCarloDuration{minera::MonteCarloDurationDistribution::kGamma,0.1,-1e100,1},
      minera::MonteCarloDuration{minera::MonteCarloDurationDistribution::kGamma,2,-1e300,1},
      minera::MonteCarloDuration{minera::MonteCarloDurationDistribution::kGamma,1e100,-1e101,1},
      minera::MonteCarloDuration{minera::MonteCarloDurationDistribution::kGamma,
          std::numeric_limits<double>::denorm_min(),-std::numeric_limits<double>::denorm_min(),1},
      minera::MonteCarloDuration{minera::MonteCarloDurationDistribution::kLogNormal,1e-200,-1e308,1},
      minera::MonteCarloDuration{minera::MonteCarloDurationDistribution::kLogNormal,1e-300,-1e300,1e300}}) {
    options.transition_durations = {duration};
    const auto result = minera::monte_carlo(net, options);
    for (double value : result.diagnostics.case_execution_seconds)
      assert(std::isfinite(value) && value >= 0 && value < 100);
  }
  for (const auto distribution : {minera::MonteCarloDurationDistribution::kGamma,
                                  minera::MonteCarloDurationDistribution::kLogNormal}) {
    for (double location : {0.0,3.0}) {
      options.transition_durations = {{distribution,2,location,0}};
      const auto result = minera::monte_carlo(net, options);
      for (double value : result.diagnostics.case_execution_seconds) assert(value == location);
    }
    for (const auto invalid : {minera::MonteCarloDuration{distribution,0,0,0},
                              minera::MonteCarloDuration{distribution,2,-1,0},
                              minera::MonteCarloDuration{distribution,2,0,-1}}) {
      options.transition_durations = {invalid};
      assert(throws_invalid([&] { (void)minera::monte_carlo(net, options); }));
    }
  }
  options.transition_durations.clear();
  options.stochastic_map = minera::StochasticMap{};
  options.stochastic_map->transitions.push_back({0,minera::StochasticMapTiming::kTimed,
      minera::StochasticMapDistribution::kGamma,0,2,0,1,0});
  const auto zero_map = minera::monte_carlo(net, options);
  auto direct = options;
  direct.stochastic_map.reset();
  direct.transition_durations = {{minera::MonteCarloDurationDistribution::kGamma,2,0,0}};
  assert_same_logs(zero_map.log, minera::monte_carlo(net, direct).log);
  options.transition_durations = {{minera::MonteCarloDurationDistribution::kDeterministic,3}};
  const auto overridden = minera::monte_carlo(net, options);
  for (double value : overridden.diagnostics.case_execution_seconds) assert(value == 3);
}

void shifted_exponential_service_laws_preserve_conditional_sampling() {
  const auto net = linear_net();
  minera::MonteCarloOptions options;
  options.start = 0; options.simulations = 1000; options.max_steps = 1;
  options.case_arrival_ratio = 0; options.resources_per_place[net.sink()] = 1000;
  options.transition_durations = {{minera::MonteCarloDurationDistribution::kExponential,2}};
  const auto baseline = minera::monte_carlo(net, options);
  for (double location : {-2.0,-1000.0,-std::numeric_limits<double>::max(),0.0,3.0}) {
    options.transition_durations.front().secondary_seconds = location;
    const auto result = minera::monte_carlo(net, options);
    std::size_t below = 0;
    for (double value : result.diagnostics.case_execution_seconds) {
      assert(std::isfinite(value) && value >= std::max(0.0, location));
      below += value <= std::max(0.0, location) + 2;
    }
    assert(std::abs(static_cast<double>(below) / options.simulations -
                    (1-std::exp(-1.0))) < 0.065);
    if (location <= 0.0) {
      assert(result.diagnostics.case_execution_seconds == baseline.diagnostics.case_execution_seconds);
      assert_same_logs(result.log, baseline.log);
    }
  }
  for (double location : {0.0,3.0}) {
    options.transition_durations = {{minera::MonteCarloDurationDistribution::kExponential,0,location}};
    for (double value : minera::monte_carlo(net, options).diagnostics.case_execution_seconds)
      assert(value == location);
  }
  for (double scale : {1e-308,std::numeric_limits<double>::denorm_min()}) {
    options.transition_durations = {{minera::MonteCarloDurationDistribution::kExponential,scale,-1}};
    const auto result = minera::monte_carlo(net, options);
    std::size_t positive = 0;
    for (double value : result.diagnostics.case_execution_seconds) {
      assert(std::isfinite(value) && value >= 0);
      positive += value > 0;
    }
    assert(positive > 0);
  }
  for (const auto invalid : {
      minera::MonteCarloDuration{minera::MonteCarloDurationDistribution::kExponential,-1,0},
      minera::MonteCarloDuration{minera::MonteCarloDurationDistribution::kExponential,0,-1}}) {
    options.transition_durations = {invalid};
    assert(throws_invalid([&] { (void)minera::monte_carlo(net, options); }));
  }
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

void weighted_and_parallel_arcs_move_their_weight() {
  // An imported PNML keeps weighted and parallel arcs as weighted arc
  // occurrences; firing moves each arc's weight, as the reference's
  // semantics.execute does (two unit arcs in, one arc of weight 2 out: the
  // final marking of two sink tokens is reached).
  minera::PetriNet net = minera::PetriNet::from_literal_storage([] {
    minera::PetriNetLiteralStorage storage;
    storage.place_count = 2;
    storage.source_storage = 0;
    storage.sink_storage = 1;
    storage.has_source = storage.has_sink = true;
    storage.activities = {"A"};
    minera::PetriTransition transition;
    transition.activity = 0;
    transition.inputs = {0, 0};
    transition.outputs = {1};
    transition.input_weights = {minera::PetriArcWeight{1, false}, minera::PetriArcWeight{1, false}};
    transition.output_weights = {minera::PetriArcWeight{2, false}};
    storage.transitions.push_back(transition);
    return storage;
  }());
  minera::MonteCarloOptions options;
  options.simulations = 2;
  options.max_steps = 8;
  options.initial_marking = {2, 0};
  options.final_marking = {0, 2};
  const auto result = minera::monte_carlo(net, options);
  assert(result.diagnostics.completed_cases == 2 && result.diagnostics.deadlocked_cases == 0);
  assert(result.log.traces.front().events.size() == 1);
}

int main() {
  deterministic_seed_and_diagnostics();
  weighted_and_parallel_arcs_move_their_weight();
  weighted_choices_and_exponential_replay();
  signed_activation_weights_normalize_enabled_sets();
  inferred_arrival_uses_exact_subsecond_gaps();
  source_log_fits_missing_weights_and_durations();
  source_log_forces_duration_distributions();
  source_log_fitting_preserves_sparse_causal_records_and_overrides();
  scalar_duration_distributions_are_seeded_and_nonnegative();
  resources_record_waits_without_delaying_events();
  reset_arcs_preserve_case_markings_with_shared_timing();
  scheduler_budget_limits_reject_unbounded_allocations();
  runtime_growth_budgets_reject_amplifying_loops();
  deadlocks_and_step_limits_are_reported();
  explicit_markings_and_componentwise_completion();
  sparse_stochastic_map_controls_branch_and_duration();
  stochastic_map_distribution_parameters_are_converted();
  arrival_inference_and_custom_selectors_round_trip();
  replay_selector_changes_fitted_activation_counts();
  fitted_weights_preserve_supplied_replay_context();
  replay_context_validation_is_limited_to_map_fitting();
  compatibility_controls_validate_without_runtime_effects();
  signed_fitted_metadata_retains_simulation_domain_validation();
  marking_membership_controls_runtime_and_is_always_validated();
  invalid_options_are_rejected();
  diagnostics_can_be_disabled();
  public_scheduler_sequences_and_weighted_arcs();
  public_scheduler_parallel_and_hidden_timing();
  microsecond_events_and_empty_trace_errors();
  inputless_transitions_require_an_initial_key();
  cycle_zero_capacity_and_overlapping_places();
  signed_service_laws_are_conditioned_without_clamping();
  conditional_service_domains_and_extreme_tails_are_bounded();
  signed_uniform_map_preserves_original_upper_endpoint();
  shifted_positive_service_laws_match_conditional_cdfs();
  shifted_service_extremes_and_zero_scales_are_bounded();
  shifted_exponential_service_laws_preserve_conditional_sampling();
  json_contract_is_stable();
}
