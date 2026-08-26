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

}  // namespace

int main() {
  const auto net = linear_net();
  const auto log = timed_log();

  const auto fitted = fit_stochastic_map_with_report(log, net);
  assert(fitted.replayed_traces == 2);
  assert(fitted.skipped_traces == 0);
  assert(fitted.map.transitions.size() == 2);
  assert(fitted.map.transitions[0].transition == 0);
  assert(fitted.map.transitions[0].distribution ==
         StochasticMapDistribution::kExponential);
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
  assert(weighted_options.transition_rates[0] > 1.99 &&
         weighted_options.transition_rates[0] < 2.01);
  assert(weighted_options.transition_rates[1] > 0.99 &&
         weighted_options.transition_rates[1] < 1.01);

  EventLog bad = log;
  bad.traces.push_back(Trace{
      "bad", {Event{"bad", "unknown", "2026-01-01T00:00:00Z", "", 0, {}, ""}}, {}});
  const auto skipped = fit_stochastic_map_with_report(bad, net);
  assert(skipped.replayed_traces == 2 && skipped.skipped_traces == 1);

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
    invalid.transitions[0].parameter = 0.0;
    validate_stochastic_map(invalid, net.transitions().size());
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}
