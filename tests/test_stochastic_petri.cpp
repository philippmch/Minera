#include <cassert>
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

}  // namespace

int main() {
  branching_states_and_generator_are_deterministic();
  immediate_states_and_duplicate_targets_are_handled();
  invalid_inputs_and_cap_are_reported();
  isolated_states_and_duplicate_output_overflow_are_rejected();
  dense_generator_budget_and_precision_are_enforced();
}
