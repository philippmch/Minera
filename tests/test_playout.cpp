#include "minera/playout.hpp"

#include <cassert>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

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

minera::EventLog source_log(std::initializer_list<std::vector<std::string>> rows) {
  minera::EventLog log;
  std::size_t case_number = 0;
  for (const auto& activities : rows) {
    minera::Trace trace;
    trace.case_id = "source_" + std::to_string(++case_number);
    for (std::size_t i = 0; i < activities.size(); ++i) {
      minera::Event event;
      event.case_id = trace.case_id;
      event.activity = activities[i];
      event.input_order = i;
      trace.events.push_back(std::move(event));
    }
    log.traces.push_back(std::move(trace));
  }
  return log;
}

minera::PetriNet branching_net(bool with_unseen_transition = false) {
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto sink = net.add_place();
  const auto dead = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({"a", "b", "never"});
  net.add_transition(0, {source}, {sink});
  net.add_transition(1, {source}, {sink});
  if (with_unseen_transition) net.add_transition(2, {dead}, {sink});
  return net;
}

std::string activity_signature(const minera::EventLog& log) {
  std::string signature;
  for (const auto& trace : log.traces) {
    signature.push_back('|');
    for (const auto& event : trace.events) signature += event.activity + ',';
  }
  return signature;
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

void basic_playout_acceptance_controls() {
  const auto net = linear_net();

  minera::PlayoutOptions exact;
  exact.traces = 2;
  exact.max_steps = 1;
  exact.add_only_if_fm_is_reached = true;
  const auto accepted = minera::playout(net, exact);
  assert(accepted.trace_count() == 2);
  assert(accepted.traces.front().events.size() == 1);

  minera::PetriNet unreachable;
  const auto source = unreachable.add_place();
  const auto sink = unreachable.add_place();
  const auto dead = unreachable.add_place();
  unreachable.set_source(source);
  unreachable.set_sink(sink);
  unreachable.set_activities({"dead"});
  unreachable.add_transition(0, {source}, {dead});
  minera::PlayoutOptions require_final;
  require_final.traces = 2;
  require_final.max_steps = 2;
  require_final.add_only_if_fm_is_reached = true;
  assert(minera::playout(unreachable, require_final).traces.empty());

  minera::PetriNet superset;
  const auto superset_source = superset.add_place();
  const auto superset_sink = superset.add_place();
  const auto extra = superset.add_place();
  superset.set_source(superset_source);
  superset.set_sink(superset_sink);
  superset.set_activities({"finish"});
  superset.add_transition(0, {superset_source}, {superset_sink, extra});

  auto exact_only = require_final;
  exact_only.traces = 1;
  exact_only.max_steps = 1;
  exact_only.add_only_if_fm_is_reached = true;
  assert(minera::playout(superset, exact_only).traces.empty());

  auto superset_accepted = exact_only;
  superset_accepted.fm_leq_accepted = true;
  const auto superset_log = minera::playout(superset, superset_accepted);
  assert(superset_log.trace_count() == 1);
  assert(superset_log.traces.front().events.size() == 1);

  const auto repeat = minera::playout(superset, superset_accepted);
  assert(activity_signature(superset_log) == activity_signature(repeat));
}

void basic_playout_timing_is_validated() {
  const auto net = linear_net();
  for (const auto invalid : {std::numeric_limits<double>::quiet_NaN(),
                             std::numeric_limits<double>::infinity()}) {
    minera::PlayoutOptions options;
    options.start = invalid;
    bool rejected = false;
    try {
      (void)minera::playout(net, options);
    } catch (const std::invalid_argument&) {
      rejected = true;
    }
    assert(rejected);
  }
  minera::PlayoutOptions negative;
  negative.case_interval = -1.0;
  bool rejected = false;
  try {
    (void)minera::playout(net, negative);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

void extensive_playout_accepts_explicit_markers() {
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto initial = net.add_place();
  const auto final = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({"a"});
  net.add_transition(0, {initial}, {final});

  minera::ExtensivePlayoutOptions options;
  options.max_traces = 4;
  options.initial_marking = {0, 1, 0, 0};
  options.final_marking = {0, 0, 1, 0};
  const auto generated = minera::extensive_playout(net, options);
  assert(generated.trace_count() == 1);
  assert(generated.traces.front().events.size() == 1);
  assert(generated.traces.front().events.front().activity == "a");

  options.final_marking = {0, 0, 1};
  bool rejected = false;
  try {
    (void)minera::extensive_playout(net, options);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

void sampled_variants_accept_explicit_markers() {
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto initial = net.add_place();
  const auto final = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({"a"});
  net.add_transition(0, {initial}, {final});

  minera::PlayoutOptions basic;
  basic.traces = 1;
  basic.max_steps = 1;
  basic.initial_marking = {0, 1, 0, 0};
  basic.final_marking = {0, 0, 1, 0};
  const auto basic_log = minera::playout(net, basic);
  assert(basic_log.trace_count() == 1);
  assert(basic_log.traces.front().events.size() == 1);

  minera::StochasticPlayoutOptions stochastic;
  stochastic.traces = 1;
  stochastic.max_steps = 1;
  stochastic.initial_marking = {0, 1, 0, 0};
  stochastic.final_marking = {0, 0, 1, 0};
  const auto stochastic_log = minera::stochastic_playout(net, stochastic);
  assert(stochastic_log.trace_count() == 1);
  assert(stochastic_log.traces.front().events.size() == 1);

  basic.final_marking = {0, 0, 1};
  bool rejected = false;
  try {
    (void)minera::playout(net, basic);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

void fitting_tracks_reachable_rates_and_silent_routing() {
  const auto net = branching_net(true);
  const auto fitted = minera::fit_stochastic_transition_weights(
      net, source_log({{"a"}}));
  assert(fitted.size() == 3);
  assert(fitted[0] == 1.0);
  assert(fitted[1] == 0.0);  // reachable, enabled, but never selected
  assert(fitted[2] == 1.0);  // never enabled: retain neutral weight

  minera::PetriNet routed;
  const auto source = routed.add_place();
  const auto middle = routed.add_place();
  const auto sink = routed.add_place();
  routed.set_source(source);
  routed.set_sink(sink);
  routed.set_activities({"a"});
  routed.add_transition(std::nullopt, {source}, {middle});
  routed.add_transition(0, {middle}, {sink});
  const auto routed_weights = minera::fit_stochastic_transition_weights(
      routed, source_log({{"a"}}));
  assert(routed_weights.size() == 2);
  assert(routed_weights[0] == 1.0 && routed_weights[1] == 1.0);
}

void fitting_handles_duplicate_labels_unknowns_and_empty_logs() {
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto middle = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({"a"});
  net.add_transition(std::nullopt, {source}, {middle});
  net.add_transition(0, {middle}, {sink});
  net.add_transition(0, {middle}, {sink});

  const auto weights = minera::fit_stochastic_transition_weights(
      net, source_log({{"unknown", "a"}}));
  assert(weights[0] == 1.0);
  assert(weights[1] == 1.0);
  assert(weights[2] == 0.0);  // stable duplicate-label tie break

  const auto empty = minera::fit_stochastic_transition_weights(
      net, minera::EventLog{});
  assert(empty.size() == 3);
  for (const auto weight : empty) assert(weight == 1.0);

  auto malformed = net;
  malformed.set_source(99);
  bool rejected = false;
  try {
    (void)minera::fit_stochastic_transition_weights(
        malformed, source_log({{"a"}}));
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

void fitting_uses_activation_ratios_and_seeded_precedence() {
  const auto net = branching_net();
  const auto fitted = minera::fit_stochastic_transition_weights(
      net, source_log({{"a"}, {"b"}}));
  assert(fitted[0] == 0.5 && fitted[1] == 0.5);

  minera::StochasticPlayoutOptions options;
  options.traces = 64;
  options.seed = 73;
  const auto first = minera::stochastic_playout(net, source_log({{"a"}, {"b"}}),
                                                options);
  const auto second = minera::stochastic_playout(net, source_log({{"a"}, {"b"}}),
                                                 options);
  assert(activity_signature(first) == activity_signature(second));

  bool varied = false;
  const auto baseline = activity_signature(first);
  for (std::uint64_t seed = 1; seed < 9; ++seed) {
    options.seed = seed;
    if (activity_signature(minera::stochastic_playout(
            net, source_log({{"a"}, {"b"}}), options)) != baseline) {
      varied = true;
      break;
    }
  }
  assert(varied);

  options.seed = 1;
  options.fit_from_log = true;
  options.transition_weights = {0.0, 1.0};
  const auto explicit_log = minera::stochastic_playout(
      net, source_log({{"a"}}), options);
  for (const auto& trace : explicit_log.traces) {
    assert(trace.events.size() == 1);
    assert(trace.events.front().activity == "b");
  }
}

void fitting_replay_budget_is_validated() {
  const auto net = linear_net();
  minera::StochasticPlayoutOptions options;
  options.fitting_max_states = 0;
  bool rejected = false;
  try {
    (void)minera::fit_stochastic_transition_weights(
        net, source_log({{"a"}}), options);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

}  // namespace

int main() {
  deterministic_and_timestamped();
  weighted_input_multiplicity_and_silent_steps();
  invalid_weights_are_rejected();
  huge_weights_are_scaled_without_overflow();
  uniform_playout_honors_weighted_input_arcs();
  basic_playout_acceptance_controls();
  basic_playout_timing_is_validated();
  extensive_playout_accepts_explicit_markers();
  sampled_variants_accept_explicit_markers();
  fitting_tracks_reachable_rates_and_silent_routing();
  fitting_handles_duplicate_labels_unknowns_and_empty_logs();
  fitting_uses_activation_ratios_and_seeded_precedence();
  fitting_replay_budget_is_validated();
}
