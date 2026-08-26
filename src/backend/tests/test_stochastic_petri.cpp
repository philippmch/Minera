#include <cassert>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "minera/stochastic_petri.hpp"
#include "minera/json_export.hpp"

namespace {

minera::PetriNet branching_net() {
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto left = net.add_place();
  const auto right = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.add_transition(std::nullopt, {source}, {left});
  net.add_transition(std::nullopt, {source}, {right});
  net.add_transition(std::nullopt, {left}, {sink});
  net.add_transition(std::nullopt, {right}, {sink});
  return net;
}

void branching_states_and_generator_are_deterministic() {
  const auto net = branching_net();
  minera::StochasticPetriOptions options;
  options.transition_rates = {2.0, 3.0, 5.0, 7.0};
  const auto result = minera::stochastic_petri_reachability(net, options);
  assert(!result.exhausted);
  assert(result.states.size() == 4);
  assert(result.states[0].marking == std::vector<std::uint32_t>({0, 0, 0, 1}));
  assert(result.states[3].marking == std::vector<std::uint32_t>({1, 0, 0, 0}));
  assert(result.generator[3][2] == 2.0);
  assert(result.generator[3][1] == 3.0);
  assert(result.generator[3][3] == -5.0);
  assert(result.generator[2][0] == 5.0);
  assert(result.generator[2][2] == -5.0);
  assert(result.generator[1][0] == 7.0);
  assert(result.generator[1][1] == -7.0);
  const auto json = minera::serialize_stochastic_petri_json(result);
  assert(json.find("\"family\":\"STOCHASTIC_PETRI\"") !=
         std::string::npos);
  assert(json.find("\"variant\":\"TANGIBLE_CTMC\"") !=
         std::string::npos);
}

void immediate_states_and_duplicate_targets_are_handled() {
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto middle = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  const auto immediate = net.add_transition(std::nullopt, {source}, {middle});
  net.add_transition(std::nullopt, {middle}, {sink});
  minera::StochasticPetriOptions options;
  options.transition_rates = {1.0, 4.0};
  options.immediate_transitions = {immediate};
  const auto result = minera::stochastic_petri_reachability(net, options);
  assert(result.states.size() == 2);
  assert(result.edges.size() == 1);
  assert(result.edges.front().rate == 4.0);

  minera::PetriNet duplicate;
  const auto dsource = duplicate.add_place();
  const auto dsink = duplicate.add_place();
  duplicate.set_source(dsource);
  duplicate.set_sink(dsink);
  duplicate.add_transition(std::nullopt, {dsource}, {dsink});
  duplicate.add_transition(std::nullopt, {dsource}, {dsink});
  options = {};
  options.transition_rates = {2.0, 3.0};
  const auto combined = minera::stochastic_petri_reachability(duplicate, options);
  assert(combined.edges.size() == 2);
  assert(combined.generator[1][0] == 5.0);
  assert(combined.generator[1][1] == -5.0);
}

void immediate_priorities_select_only_highest_enabled_firings() {
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto preferred = net.add_place();
  const auto suppressed = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  const auto high = net.add_transition(std::nullopt, {source}, {preferred});
  const auto low = net.add_transition(std::nullopt, {source}, {suppressed});
  net.add_transition(std::nullopt, {preferred}, {sink});
  net.add_transition(std::nullopt, {suppressed}, {sink});

  minera::StochasticPetriOptions options;
  options.immediate_transitions = {high, low};
  options.transition_priorities = {4, 1, 0, 0};
  const auto result = minera::stochastic_petri_reachability(net, options);
  for (const auto& state : result.states)
    assert(state.marking != std::vector<std::uint32_t>({0, 0, 1, 0}));
  assert(result.states.size() == 2);

  options.transition_priorities = {4, 1};
  bool rejected = false;
  try {
    (void)minera::stochastic_petri_reachability(net, options);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

void immediate_closure_projects_timed_successors_and_weighted_initials() {
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto vanishing = net.add_place();
  const auto left = net.add_place();
  const auto right = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.add_transition(std::nullopt, {source}, {vanishing});
  const auto left_choice =
      net.add_transition(std::nullopt, {vanishing}, {left});
  const auto right_choice =
      net.add_transition(std::nullopt, {vanishing}, {right});
  net.add_transition(std::nullopt, {left}, {sink});
  net.add_transition(std::nullopt, {right}, {sink});

  minera::StochasticPetriOptions options;
  options.transition_rates = {5.0, 1.0, 1.0, 2.0, 3.0};
  options.immediate_transitions = {left_choice, right_choice};
  options.immediate_weights = {0.0, 2.0, 1.0, 1.0, 1.0};
  const auto result = minera::stochastic_petri_reachability(net, options);
  assert(result.initial_state);
  assert(result.initial_probabilities.size() == result.states.size());
  assert(result.initial_probabilities[*result.initial_state] == 1.0);
  const auto source_state = *result.initial_state;
  std::vector<double> projected_rates;
  for (const auto& edge : result.edges) {
    if (edge.source != source_state) continue;
    projected_rates.push_back(edge.rate);
  }
  assert(projected_rates.size() == 2);
  std::sort(projected_rates.begin(), projected_rates.end());
  assert(std::abs(projected_rates[0] - 5.0 / 3.0) < 1e-12);
  assert(std::abs(projected_rates[1] - 10.0 / 3.0) < 1e-12);

  // Starting directly in the vanishing marking must retain a usable
  // probability distribution over its tangible closure.
  options.initial_marking = {0, 1, 0, 0, 0};
  const auto vanishing_result =
      minera::stochastic_petri_reachability(net, options);
  assert(!vanishing_result.initial_state);
  assert(vanishing_result.initial_probabilities.size() ==
         vanishing_result.states.size());
  double initial_mass = 0.0;
  std::size_t nonzero = 0;
  for (const auto probability : vanishing_result.initial_probabilities) {
    initial_mass += probability;
    if (probability > 1e-12) ++nonzero;
  }
  assert(nonzero == 2);
  assert(std::abs(initial_mass - 1.0) < 1e-12);

  options.max_states = 1;
  const auto capped_vanishing =
      minera::stochastic_petri_reachability(net, options);
  assert(capped_vanishing.exhausted);

  options.max_states = 2;
  const auto capped_split =
      minera::stochastic_petri_reachability(net, options);
  assert(capped_split.exhausted);
  assert(capped_split.initial_probabilities.empty());
}

void invalid_inputs_and_cap_are_reported() {
  const auto net = branching_net();
  for (const auto rates : {std::vector<double>{0.0, 1.0, 1.0, 1.0},
                           std::vector<double>{-1.0, 1.0, 1.0, 1.0},
                           std::vector<double>{std::numeric_limits<double>::infinity(),
                                                1.0, 1.0, 1.0},
                           std::vector<double>{1.0}}) {
    minera::StochasticPetriOptions options;
    options.transition_rates = rates;
    bool rejected = false;
    try {
      (void)minera::stochastic_petri_reachability(net, options);
    } catch (const std::invalid_argument&) {
      rejected = true;
    }
    if (rates.front() == 0.0) assert(!rejected);
    else assert(rejected);
  }
  minera::StochasticPetriOptions capped;
  capped.max_states = 1;
  const auto result = minera::stochastic_petri_reachability(net, capped);
  assert(result.exhausted);
  assert(result.states.size() == 1);
  assert(result.diagnostic == "max_states exhausted");
  capped.max_states = 0;
  bool rejected = false;
  try {
    (void)minera::stochastic_petri_reachability(net, capped);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

void isolated_states_and_duplicate_output_overflow_are_rejected() {
  minera::PetriNet isolated;
  const auto place = isolated.add_place();
  isolated.set_source(place);
  isolated.set_sink(place);
  const auto result = minera::stochastic_petri_reachability(isolated);
  assert(result.states.empty());

  minera::PetriNet overflow;
  const auto overflow_place = overflow.add_place();
  overflow.set_source(overflow_place);
  overflow.set_sink(overflow_place);
  overflow.add_transition(std::nullopt, {}, {overflow_place, overflow_place});
  minera::StochasticPetriOptions options;
  options.initial_marking = {
      std::numeric_limits<std::uint32_t>::max() - 1U};
  bool rejected = false;
  try {
    (void)minera::stochastic_petri_reachability(overflow, options);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

void dense_generator_budget_and_precision_are_enforced() {
  minera::PetriNet chain;
  std::vector<std::size_t> places;
  places.reserve(minera::kStochasticPetriMaxGeneratorStates + 1);
  for (std::size_t i = 0; i <= minera::kStochasticPetriMaxGeneratorStates;
       ++i)
    places.push_back(chain.add_place());
  chain.set_source(places.front());
  chain.set_sink(places.back());
  for (std::size_t i = 0; i < minera::kStochasticPetriMaxGeneratorStates;
       ++i)
    chain.add_transition(std::nullopt, {places[i]}, {places[i + 1]});
  minera::StochasticPetriOptions bounded;
  bounded.max_states = minera::kStochasticPetriMaxGeneratorStates + 1;
  bool rejected = false;
  try {
    (void)minera::stochastic_petri_reachability(chain, bounded);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);

  minera::PetriNet precise;
  const auto source = precise.add_place();
  const auto sink = precise.add_place();
  precise.set_source(source);
  precise.set_sink(sink);
  precise.add_transition(std::nullopt, {source}, {sink});
  minera::StochasticPetriOptions rates;
  rates.transition_rates = {1.2345678901234567};
  const auto json = minera::serialize_stochastic_petri_json(
      minera::stochastic_petri_reachability(precise, rates));
  assert(json.find("1.2345678901234567") != std::string::npos);
}

void transient_and_steady_state_are_deterministic() {
  const auto net = [&] {
    minera::PetriNet value;
    const auto source = value.add_place();
    const auto sink = value.add_place();
    value.set_source(source);
    value.set_sink(sink);
    value.add_transition(std::nullopt, {source}, {sink});
    return value;
  }();
  minera::StochasticPetriOptions options;
  options.transition_rates = {2.0};
  const auto graph = minera::stochastic_petri_reachability(net, options);
  assert(graph.initial_state && *graph.initial_state == 1);
  const auto at_zero = minera::stochastic_petri_transient(graph, 0.0, 1);
  assert(at_zero.computed && at_zero.initial_state == 1);
  assert(at_zero.probabilities == std::vector<double>({0.0, 1.0}));
  const auto transient = minera::stochastic_petri_transient(graph, 0.5, 1);
  assert(transient.computed);
  assert(std::abs(transient.probabilities[0] - (1.0 - std::exp(-1.0))) < 1e-9);
  assert(std::abs(transient.probabilities[1] - std::exp(-1.0)) < 1e-9);
  const auto vector = minera::stochastic_petri_transient(
      graph, 0.5, std::vector<double>({0.0, 1.0}));
  assert(vector.probabilities == transient.probabilities);
  const auto steady = minera::stochastic_petri_steady_state(graph);
  assert(steady.computed && steady.unique);
  assert(steady.probabilities == std::vector<double>({1.0, 0.0}));
  assert(minera::serialize_stochastic_petri_distribution_json(steady).find(
             "STEADY_STATE") != std::string::npos);
  const auto precise = minera::stochastic_petri_transient(graph, 0.5, 1);
  const auto precise_json =
      minera::serialize_stochastic_petri_distribution_json(precise);
  assert(precise_json.find("0.632120558828") != std::string::npos);

  for (const auto scale : {1e-15, 1e15}) {
    minera::StochasticPetriResult scaled;
    scaled.states = {{{0}}, {{1}}};
    scaled.generator = {{-scale, scale}, {2.0 * scale, -2.0 * scale}};
    const auto stationary = minera::stochastic_petri_steady_state(scaled);
    assert(stationary.computed && stationary.unique);
    assert(std::abs(stationary.probabilities[0] - (2.0 / 3.0)) < 1e-10);
    assert(std::abs(stationary.probabilities[1] - (1.0 / 3.0)) < 1e-10);
  }
}

void probability_inputs_and_non_unique_states_are_rejected_deterministically() {
  minera::StochasticPetriResult graph;
  graph.states = {{{0}}, {{1}}};
  graph.generator = {{-1.0, 1.0}, {1.0, -1.0}};
  for (const auto time : {-1.0, std::numeric_limits<double>::infinity(),
                          1'000'001.0}) {
    bool rejected = false;
    try {
      (void)minera::stochastic_petri_transient(graph, time, 0);
    } catch (const std::invalid_argument&) {
      rejected = true;
    }
    assert(rejected);
  }
  graph.exhausted = true;
  bool incomplete_rejected = false;
  try {
    (void)minera::stochastic_petri_steady_state(graph);
  } catch (const std::invalid_argument&) {
    incomplete_rejected = true;
  }
  assert(incomplete_rejected);
  graph.exhausted = false;
  for (const auto probabilities : {std::vector<double>{0.5},
                                   std::vector<double>{0.2, 0.2},
                                   std::vector<double>{-0.1, 1.1}}) {
    bool rejected = false;
    try {
      (void)minera::stochastic_petri_transient(graph, 1.0, probabilities);
    } catch (const std::invalid_argument&) {
      rejected = true;
    }
    assert(rejected);
  }
  minera::StochasticPetriResult multiple;
  multiple.states = {{{0}}, {{1}}};
  multiple.generator = {{0.0, 0.0}, {0.0, 0.0}};
  const auto steady = minera::stochastic_petri_steady_state(multiple);
  assert(!steady.computed && !steady.unique);
  assert(steady.diagnostic.find("multiple recurrent") != std::string::npos);
  const auto json = minera::serialize_stochastic_petri_distribution_json(steady);
  assert(json == minera::serialize_stochastic_petri_distribution_json(steady));
  const auto empty = minera::stochastic_petri_steady_state({});
  assert(!empty.computed && !empty.unique);
  assert(empty.diagnostic.find("requires states") != std::string::npos);
}

void transient_work_budget_rejects_oversized_dense_requests() {
  constexpr std::size_t count = minera::kStochasticPetriMaxGeneratorStates;
  minera::StochasticPetriResult graph;
  graph.states.resize(count);
  graph.generator.assign(count, std::vector<double>(count, 0.0));
  for (std::size_t state = 0; state < count; ++state) {
    graph.states[state].marking = {static_cast<std::uint32_t>(state)};
    graph.generator[state][state] = -1.0;
    graph.generator[state][(state + 1) % count] = 1.0;
  }
  bool rejected = false;
  try {
    (void)minera::stochastic_petri_transient(graph, 1'000'000.0, 0);
  } catch (const std::invalid_argument& error) {
    rejected = std::string(error.what()).find("work budget") !=
               std::string::npos;
  }
  assert(rejected);
}

}  // namespace

int main() {
  branching_states_and_generator_are_deterministic();
  immediate_states_and_duplicate_targets_are_handled();
  immediate_priorities_select_only_highest_enabled_firings();
  immediate_closure_projects_timed_successors_and_weighted_initials();
  invalid_inputs_and_cap_are_reported();
  isolated_states_and_duplicate_output_overflow_are_rejected();
  dense_generator_budget_and_precision_are_enforced();
  transient_and_steady_state_are_deterministic();
  probability_inputs_and_non_unique_states_are_rejected_deterministically();
  transient_work_budget_rejects_oversized_dense_requests();
}
