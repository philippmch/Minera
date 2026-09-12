#include "minera/stochastic_map.hpp"

#include <array>
#include <cassert>
#include <cmath>
#include <stdexcept>
#include <string>

using namespace minera;

namespace {

PetriNet linear_net() {
  PetriNet net;
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

EventLog timed_log() {
  EventLog log;
  log.traces.push_back(Trace{
      "case-1",
      {Event{"case-1", "A", "2026-01-01T00:00:00Z", "", 0, {}, ""},
       Event{"case-1", "B", "2026-01-01T00:00:02Z", "", 1, {}, ""}},
      {}});
  log.traces.push_back(Trace{
      "case-2",
      {Event{"case-2", "A", "2026-01-01T00:00:10Z", "", 0, {}, ""},
       Event{"case-2", "B", "2026-01-01T00:00:14Z", "", 1, {}, ""}},
      {}});
  return log;
}

EventLog subsecond_log() {
  EventLog log;
  log.traces.push_back(Trace{
      "short",
      {Event{"short", "A", "2026-01-01T00:00:00.000Z", "", 0, {}, ""},
       Event{"short", "B", "2026-01-01T00:00:00.500Z", "", 1, {}, ""}},
      {}});
  return log;
}

EventLog events(std::initializer_list<std::pair<const char*, int>> values) {
  EventLog log;
  Trace trace;
  trace.case_id = "case";
  for (const auto& [label, second] : values) {
    const auto seconds = (second < 10 ? "0" : "") + std::to_string(second);
    trace.events.push_back(Event{"case", label,
        "2026-01-01T00:00:" + seconds + "Z", "", trace.events.size(), {}, ""});
  }
  log.traces.push_back(std::move(trace));
  return log;
}

void public_statistics_regressions() {
  const auto net = linear_net();
  const auto base = fit_stochastic_map_with_report(timed_log(), net);
  assert(base.statistics[0].performance == std::vector<double>({0, 0}));
  assert(base.statistics[1].performance == std::vector<double>({2, 4}));
  assert(base.statistics[1].log_indices == std::vector<std::size_t>({0, 1}));
  assert(base.map.transitions[0].priority == 1);
  for (const auto& sample : {events({{"A", 0}, {"unknown", 1}, {"B", 7}}),
                             events({{"A", 0}, {"unknown", 1}, {"unknown", 5}, {"B", 9}})}) {
    const auto fit = fit_stochastic_map_with_report(sample, net);
    assert(fit.statistics[1].performance == std::vector<double>({1}));
  }
  const auto unknown_first = fit_stochastic_map_with_report(
      events({{"unknown", 0}, {"A", 2}, {"B", 7}}), net);
  assert(unknown_first.statistics[1].performance == std::vector<double>({0}));
  const auto negative = fit_stochastic_map_with_report(events({{"A", 5}, {"B", 1}}), net);
  assert(negative.statistics[1].performance == std::vector<double>({-4}));
  assert(negative.map.transitions[1].distribution == StochasticMapDistribution::kImmediate);
  assert(fit_stochastic_map(EventLog{}, net).transitions.empty());
  assert(fit_stochastic_map(events({}), net).transitions.empty());
  const auto tail = fit_stochastic_map(events({{"A", 0}}), net);
  assert(tail.transitions.size() == 1 && tail.transitions[0].transition == 0);
  validate_sparse_stochastic_map(tail, 2);
  bool rejected = false;
  try { (void)stochastic_map_to_options(tail); }
  catch (const std::invalid_argument&) { rejected = true; }
  assert(!rejected); // The legacy overload has no net count and sees a complete prefix.
  const auto dense = stochastic_map_to_options(net, tail);
  assert(dense.transition_rates.size() == 2 && dense.transition_rates[1] == 1);
  assert(tail.transitions.size() == 1);
  StochasticMap hole;
  hole.transitions.push_back(base.map.transitions[1]);
  validate_sparse_stochastic_map(hole, 2);
  assert(serialize_stochastic_map_json(hole).find("\"transition\":1") != std::string::npos);
  rejected = false;
  try { (void)stochastic_map_to_options(hole); }
  catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected);
  assert(stochastic_map_to_options(net, hole).transition_rates.size() == 2);

  StochasticMapFitOptions context;
  context.replay.initial_marking = {1, 1, 0};
  context.replay.final_marking = {0, 1, 1};
  const auto multi = fit_stochastic_map_with_report(timed_log(), net, context);
  assert(multi.statistics[1].enablings == 4);
  assert(multi.map.transitions[1].activation_weight == 0.5);
  context.replay.initial_marking = {0, 0, 0};
  context.replay.final_marking = {0, 0, 0};
  context.replay.initial_marking_keys = {true, false, false};
  context.replay.final_marking_keys = {false, false, true};
  const auto zeros = fit_stochastic_map_with_report(timed_log(), net, context);
  assert(zeros.statistics[0].performance.empty());
  assert(zeros.statistics[1].performance == std::vector<double>({2, 4}));
  context.replay.initial_marking = {1};
  rejected = false;
  try { (void)fit_stochastic_map(EventLog{}, net, context); }
  catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected);

  context = {};
  context.replay.initial_marking = {1, 0, 0};
  context.replay.initial_marking_keys = {false, false, false};
  rejected = false;
  try { (void)fit_stochastic_map(EventLog{}, net, context); }
  catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected);
  context = {};
  context.replay.initial_marking = {-1, 0, 0};
  rejected = false;
  try { (void)fit_stochastic_map(EventLog{}, net, context); }
  catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected);
  context = {};
  context.replay.transition_names = {"only-one-name"};
  rejected = false;
  try { (void)fit_stochastic_map(EventLog{}, net, context); }
  catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected);

  context = {};
  context.replay.initial_marking = {2, 0, 0};
  context.replay.final_marking = {0, 0, 2};
  const auto fifo = fit_stochastic_map_with_report(
      events({{"A", 0}, {"A", 2}, {"B", 5}, {"B", 9}}), net, context);
  assert(fifo.statistics[1].performance == std::vector<double>({5, 7}));
  assert(fifo.statistics[1].enablings == 3);
  assert(std::abs(fifo.map.transitions[1].activation_weight - 2.0 / 3.0) < 1e-12);
  auto weighted = linear_net();
  weighted.add_arc(1, 1);
  const auto weighted_fit = fit_stochastic_map_with_report(
      events({{"A", 0}, {"A", 3}, {"B", 6}}), weighted, context);
  assert(weighted_fit.statistics[1].performance == std::vector<double>({6}));

  PetriNet hidden;
  for (int i = 0; i < 5; ++i) hidden.add_place();
  hidden.set_source(0); hidden.set_sink(4); hidden.set_activities({"A", "B", "C"});
  hidden.add_transition(0, {0}, {1});
  hidden.add_transition(std::nullopt, {1}, {2});
  hidden.add_transition(1, {2}, {3});
  hidden.add_transition(2, {3}, {4});
  const auto hidden_fit = fit_stochastic_map_with_report(
      events({{"A", 0}, {"B", 3}, {"C", 8}}), hidden);
  assert(hidden_fit.statistics[1].performance.empty());
  assert(hidden_fit.statistics[2].performance.empty());
  assert(hidden_fit.statistics[3].performance == std::vector<double>({5}));

  StochasticMapFitOptions fixed;
  fixed.fit_timing = false; fixed.fit_activation_weights = false;
  fixed.default_parameter = 2; fixed.default_priority = 7;
  const auto fixed_fit = fit_stochastic_map(timed_log(), net, fixed);
  assert(fixed_fit.transitions[0].parameter == 0.5);
  assert(fixed_fit.transitions[0].priority == 7);
  assert(fixed_fit.transitions[0].activation_weight == 1);
}

}  // namespace

int main() {
  public_statistics_regressions();
  const auto net = linear_net();
  const auto log = timed_log();

  const auto fitted = fit_stochastic_map_with_report(log, net);
  assert(fitted.replayed_traces == 2);
  assert(fitted.skipped_traces == 0);
  assert(fitted.map.transitions.size() == 2);
  assert(fitted.map.transitions[0].transition == 0);
  assert(fitted.map.transitions[0].distribution ==
         StochasticMapDistribution::kImmediate);
  assert(fitted.map.transitions[1].parameter > 0.32 &&
         fitted.map.transitions[1].parameter < 0.34);
  assert(fitted.map.transitions[0].activation_weight == 1.0);
  assert(fitted.map.transitions[1].activation_weight == 1.0);
  const auto options = stochastic_map_to_options(fitted.map);
  assert(options.transition_rates.size() == 2);
  assert(options.immediate_weights.size() == 2);
  assert(options.transition_priorities.size() == 2);
  assert(options.transition_rates[1] > 0.32 &&
         options.transition_rates[1] < 0.34);
  const auto json = serialize_stochastic_map_json(fitted.map);
  assert(json.find("STOCHASTIC_MAP") != std::string::npos);
  assert(json.find("exponential") != std::string::npos);

  auto weighted = fitted.map;
  weighted.transitions[0].activation_weight = 2.0;
  weighted.transitions[1].activation_weight = 3.0;
  const auto weighted_options = stochastic_map_to_options(weighted);
  assert(weighted_options.transition_rates[0] == 0.0);
  assert(weighted_options.immediate_weights[0] == 2.0);
  assert(weighted_options.transition_rates[1] > 0.99 &&
         weighted_options.transition_rates[1] < 1.01);

  EventLog bad = log;
  bad.traces.push_back(Trace{
      "bad", {Event{"bad", "unknown", "2026-01-01T00:00:00Z", "", 0, {}, ""}}, {}});
  const auto skipped = fit_stochastic_map_with_report(bad, net);
  assert(skipped.replayed_traces == 3 && skipped.skipped_traces == 0);

  const auto graph = discover_directly_follows_graph(log);
  const auto performance = discover_performance_dfg(log);
  StochasticMapPerformanceOptions performance_options;
  performance_options.conversion.variant =
      DfgPetriConversionVariant::kActivityDefinesPlace;
  const auto performance_model =
      stochastic_map_from_performance_dfg(graph, performance,
                                          performance_options);
  assert(performance_model.net.transitions().size() ==
         performance_model.map.transitions.size());
  assert(!performance_model.map.transitions.empty());
  const auto performance_options_for_ctmc =
      stochastic_map_to_options(performance_model.map);
  assert(performance_options_for_ctmc.transition_rates.size() ==
         performance_model.net.transitions().size());
  assert(!performance_options_for_ctmc.immediate_transitions.empty());

  const std::array distributions{
      StochasticMapDistribution::kDeterministic,
      StochasticMapDistribution::kExponential,
      StochasticMapDistribution::kConstant0,
      StochasticMapDistribution::kImmediate,
      StochasticMapDistribution::kNormal,
      StochasticMapDistribution::kUniform,
      StochasticMapDistribution::kLogNormal,
      StochasticMapDistribution::kGamma,
  };
  for (const auto distribution : distributions) {
    StochasticMapFitOptions fit_options;
    fit_options.distribution = distribution;
    fit_options.default_parameter = 2.0;
    const auto family = fit_stochastic_map_with_report(log, net, fit_options);
    validate_stochastic_map(family.map, net.transitions().size());
    const auto family_ctmc = stochastic_map_to_options(family.map);
    assert(family_ctmc.transition_rates.size() == net.transitions().size());

    StochasticMapPerformanceOptions empty_options;
    empty_options.distribution = distribution;
    empty_options.default_parameter = 2.0;
    const auto fallback = stochastic_map_from_performance_dfg(
        graph, DfgPerformance{}, empty_options);
    validate_stochastic_map(fallback.map, fallback.net.transitions().size());
    const auto fallback_ctmc = stochastic_map_to_options(fallback.map);
    assert(fallback_ctmc.transition_rates.size() ==
           fallback.net.transitions().size());

    const auto performance_family = stochastic_map_from_performance_dfg(
        graph, performance, empty_options);
    validate_stochastic_map(performance_family.map,
                            performance_family.net.transitions().size());
    const auto performance_family_ctmc =
        stochastic_map_to_options(performance_family.map);
    assert(performance_family_ctmc.transition_rates.size() ==
           performance_family.net.transitions().size());
  }

  StochasticMapFitOptions lognormal_options;
  lognormal_options.distribution = StochasticMapDistribution::kLogNormal;
  const auto subsecond = fit_stochastic_map_with_report(
      subsecond_log(), net, lognormal_options);
  assert(subsecond.map.transitions[1].parameter < 0.0);
  validate_stochastic_map(subsecond.map, net.transitions().size());
  const auto subsecond_ctmc = stochastic_map_to_options(subsecond.map);
  assert(subsecond_ctmc.transition_rates[1] > 0.0);

  bool threw = false;
  try {
    auto invalid = fitted.map;
    invalid.transitions[0].transition = 1;
    validate_stochastic_map(invalid, net.transitions().size());
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  threw = false;
  try {
    auto invalid = fitted.map;
    invalid.transitions[1].parameter = 0.0;
    validate_stochastic_map(invalid, net.transitions().size());
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}
