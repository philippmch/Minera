#include "minera/playout.hpp"

#include <cassert>
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
  net.set_activities({"a"});
  net.add_transition(0, {source}, {sink});
  return net;
}

void deterministic_and_timestamped() {
  const auto net = linear_net();
  minera::StochasticPlayoutOptions options;
  options.traces = 2;
  options.seed = 17;
  options.start = 1767225600.0;
  const auto first = minera::stochastic_playout(net, options);
  const auto second = minera::stochastic_playout(net, options);
  assert(first.traces.size() == 2);
  assert(first.traces[0].events.size() == 1);
  assert(first.traces[0].events[0].timestamp ==
         second.traces[0].events[0].timestamp);
  assert(first.traces[0].events[0].timestamp ==
         "2026-01-01T00:00:00.000Z");
}

void weighted_input_multiplicity_and_silent_steps() {
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto gate = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({"a"});
  net.add_transition(std::nullopt, {source}, {gate, gate});
  net.add_transition(0, {gate, gate}, {sink});

  minera::StochasticPlayoutOptions options;
  options.traces = 1;
  options.max_steps = 2;
  const auto log = minera::stochastic_playout(net, options);
  assert(log.traces.size() == 1);
  assert(log.traces[0].events.size() == 1);
  assert(log.traces[0].events[0].activity == "a");
}

void invalid_weights_are_rejected() {
  const auto net = linear_net();
  minera::StochasticPlayoutOptions options;
  options.transition_weights = {1.0, 2.0};
  bool rejected = false;
  try {
    (void)minera::stochastic_playout(net, options);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

void huge_weights_are_scaled_without_overflow() {
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({"a", "b"});
  net.add_transition(0, {source}, {sink});
  net.add_transition(1, {source}, {sink});
  minera::StochasticPlayoutOptions options;
  options.traces = 100;
  options.transition_weights = {std::numeric_limits<double>::max(),
                                 std::numeric_limits<double>::max()};
  const auto log = minera::stochastic_playout(net, options);
  bool saw_a = false;
  bool saw_b = false;
  for (const auto& trace : log.traces) {
    assert(trace.events.size() == 1);
    saw_a = saw_a || trace.events.front().activity == "a";
    saw_b = saw_b || trace.events.front().activity == "b";
  }
  assert(saw_a && saw_b);
}

void uniform_playout_honors_weighted_input_arcs() {
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto gate = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({"a"});
  net.add_transition(std::nullopt, {source}, {gate, gate});
  net.add_transition(0, {gate, gate}, {sink});
  minera::PlayoutOptions options;
  options.traces = 1;
  options.max_steps = 2;
  const auto log = minera::playout(net, options);
  assert(log.traces.size() == 1);
  assert(log.traces[0].events.size() == 1);
}

}  // namespace

int main() {
  deterministic_and_timestamped();
  weighted_input_multiplicity_and_silent_steps();
  invalid_weights_are_rejected();
  huge_weights_are_scaled_without_overflow();
  uniform_playout_honors_weighted_input_arcs();
}
