#include "minera/conformance.hpp"

#include <cassert>
#include <cmath>
#include <iostream>

using namespace minera;

namespace {
EventLog log_of(std::initializer_list<std::initializer_list<const char*>> traces) {
  EventLog log;
  for (const auto& labels : traces) {
    Trace trace;
    trace.case_id = "case-" + std::to_string(log.traces.size());
    for (const auto* label : labels) {
      Event event;
      event.activity = label;
      trace.events.push_back(std::move(event));
    }
    log.traces.push_back(std::move(trace));
  }
  return log;
}

PetriNet sequential(bool hidden = false) {
  PetriNet net;
  net.set_activities({"a", "b"});
  const auto source = net.add_place();
  const auto middle = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  if (hidden) {
    const auto before = net.add_place();
    const auto after = net.add_place();
    net.add_transition(std::nullopt, {source}, {before});
    net.add_transition(0, {before}, {middle});
    net.add_transition(1, {middle}, {after});
    net.add_transition(std::nullopt, {after}, {sink});
  } else {
    net.add_transition(0, {source}, {middle});
    net.add_transition(1, {middle}, {sink});
  }
  return net;
}

void consistent(const EventLog& log, const PetriNet& net, TokenReplayOptions options) {
  const auto rows = replay_token_traces(log, net, options);
  const auto aggregate = check_conformance(log, net, options);
  const auto places = replay_token_diagnostics(log, net, options);
  assert(!aggregate.trace_diagnostics);
  std::uint64_t produced = 0, consumed = 0, missing = 0, remaining = 0, fitting = 0;
  std::vector<std::uint64_t> fired(net.transitions().size());
  std::vector<TokenReplayPlaceDiagnostics> totals(net.place_count());
  for (const auto& row : rows) {
    produced += row.produced;
    consumed += row.consumed;
    missing += row.missing;
    remaining += row.remaining;
    fitting += row.trace_is_fit;
    for (const auto t : row.activated_transitions) ++fired[t];
    for (std::size_t p = 0; p < totals.size(); ++p) {
      totals[p].produced += row.places[p].produced;
      totals[p].consumed += row.places[p].consumed;
      totals[p].missing += row.places[p].missing;
      totals[p].remaining += row.places[p].remaining;
    }
  }
  assert(produced == aggregate.produced && consumed == aggregate.consumed);
  assert(missing == aggregate.missing && remaining == aggregate.remaining);
  assert(fitting == aggregate.fitting_traces && fitting == places.fitting_traces);
  assert(fired == aggregate.transition_frequencies && fired == places.transition_activations);
  for (std::size_t p = 0; p < totals.size(); ++p) {
    assert(totals[p].produced == places.places[p].produced);
    assert(totals[p].consumed == places.places[p].consumed);
    assert(totals[p].missing == places.places[p].missing);
    assert(totals[p].remaining == places.places[p].remaining);
  }
  options.include_trace_diagnostics = true;
  const auto attached = check_conformance(log, net, options);
  assert(attached.trace_diagnostics && attached.trace_diagnostics->size() == rows.size());
  assert(attached.fitness == aggregate.fitness && attached.precision == aggregate.precision);
  for (std::size_t i = 0; i < rows.size(); ++i) {
    assert((*attached.trace_diagnostics)[i].case_id == rows[i].case_id);
    assert((*attached.trace_diagnostics)[i].activated_transitions == rows[i].activated_transitions);
    assert((*attached.trace_diagnostics)[i].reached_marking == rows[i].reached_marking);
  }
}

void ordered_trace_rows_and_markings() {
  const auto net = sequential();
  const auto log = log_of({{"a", "b"}, {"a", "b"}, {}});
  const auto rows = replay_token_traces(log, net);
  assert(rows.size() == 3);
  for (std::size_t i = 0; i < rows.size(); ++i) {
    assert(rows[i].trace_index == i && rows[i].case_id == "case-" + std::to_string(i));
  }
  assert(rows[0].trace_is_fit && rows[0].trace_fitness == 1.0);
  assert(rows[0].produced == 3 && rows[0].consumed == 3);
  assert((rows[0].activated_transitions == std::vector<std::size_t>{0, 1}));
  assert((rows[0].reached_marking == std::vector<int>{0, 0, 1}));
  assert(rows[0].enabled_transitions.empty() && rows[0].replayed_events == 2);
  assert(!rows[2].trace_is_fit && rows[2].replayed_events == 0);
  assert((rows[2].enabled_transitions == std::vector<std::size_t>{0}));
  assert(replay_token_traces(EventLog{}, net).empty());
  consistent(log, net, {});
}

void hidden_routing_policies_and_enabled_ids() {
  const auto net = sequential(true);
  auto row = replay_token_traces(log_of({{"a", "b"}}), net).front();
  assert(row.trace_is_fit);
  assert((row.activated_transitions == std::vector<std::size_t>{0, 1, 2, 3}));
  TokenReplayOptions options;
  options.try_to_reach_final_marking_through_hidden = false;
  row = replay_token_traces(log_of({{"a", "b"}}), net, options).front();
  assert((row.activated_transitions == std::vector<std::size_t>{0, 1, 2}));
  assert(row.reached_marking[4] == 1 && row.reached_marking[2] == 0);
  assert(row.missing == 1 && row.remaining == 1);
  options.try_to_reach_final_marking_through_hidden = true;
  options.walk_through_hidden_transitions = false;
  row = replay_token_traces(log_of({{"a", "b"}}), net, options).front();
  assert((row.activated_transitions == std::vector<std::size_t>{1, 2, 3}));
  assert((row.transitions_with_problems == std::vector<std::size_t>{1}));
  assert(row.missing == 1 && row.remaining == 1);
  assert((row.enabled_transitions == std::vector<std::size_t>{1}));
  consistent(log_of({{"a", "b"}, {"a"}, {}}), net, options);
}

void deviations_unknowns_and_stopping() {
  const auto net = sequential();
  const auto log = log_of({{"b", "a"}, {"unknown", "a", "b"}});
  auto row = replay_token_traces(log, net).front();
  assert((row.activated_transitions == std::vector<std::size_t>{1, 0}));
  assert((row.transitions_with_problems == std::vector<std::size_t>{1}));
  assert(!row.trace_is_fit && row.missing == 1 && row.remaining == 1);
  TokenReplayOptions options;
  options.stop_immediately_unfit = true;
  auto rows = replay_token_traces(log, net, options);
  assert(rows[0].replay_stopped && rows[0].replayed_events == 1);
  assert(rows[0].activated_transitions.empty());
  assert((rows[0].transitions_with_problems == std::vector<std::size_t>{1}));
  assert((rows[0].reached_marking == std::vector<int>{1, 0, 0}));
  assert(rows[1].replay_stopped && rows[1].replayed_events == 1);
  assert((rows[1].unknown_event_indices == std::vector<std::size_t>{0}));
  assert((rows[1].unknown_activities == std::vector<std::string>{"unknown"}));
  consistent(log, net, options);
  options.consider_activities_not_in_model_in_fitness = false;
  row = replay_token_traces(log, net, options)[1];
  assert(row.trace_is_fit && !row.replay_stopped && row.replayed_events == 3);
  assert(row.unknown_activities.size() == 1);
  consistent(log, net, options);
}

void backward_contract_and_duplicate_labels() {
  PetriNet net;
  net.set_activities({"a"});
  for (int i = 0; i < 3; ++i) net.add_place();
  net.set_source(0);
  net.set_sink(2);
  net.add_transition(0, {0}, {1});
  net.add_transition(0, {1}, {2});
  TokenReplayOptions options;
  options.variant = TokenReplayVariant::kBackwards;
  auto row = replay_token_traces(log_of({{"unknown", "a"}}), net, options).front();
  assert(row.trace_is_fit);
  assert((row.activated_transitions == std::vector<std::size_t>{0, 1}));
  assert(row.unknown_event_indices.size() == 1);
  row = replay_token_traces(log_of({{"a", "a"}}), net, options).front();
  assert(row.replay_stopped && !row.trace_is_fit && row.replayed_events == 2);
  consistent(log_of({{"a"}, {"a", "a"}, {"unknown"}, {}}), net, options);
  const auto hidden = sequential(true);
  options.walk_through_hidden_transitions = false;
  options.stop_immediately_unfit = true;
  row = replay_token_traces(log_of({{"a", "b"}}), hidden, options).front();
  assert((row.activated_transitions == std::vector<std::size_t>{1}));
  assert(row.replay_stopped && row.replayed_events == 1);
  consistent(log_of({{"a", "b"}, {}}), hidden, options);
}

void explicit_markings_and_remaining_classification() {
  const auto net = sequential();
  TokenReplayOptions options;
  options.initial_marking = {2, 0, 0};
  options.final_marking = {0, 0, 1};
  auto row = replay_token_traces(log_of({{"a", "b"}}), net, options).front();
  assert(!row.trace_is_fit && row.remaining == 1 && row.missing == 0);
  const auto fitness = row.trace_fitness;
  options.consider_remaining_in_fitness = false;
  row = replay_token_traces(log_of({{"a", "b"}}), net, options).front();
  assert(row.trace_is_fit && row.trace_fitness == fitness && fitness < 1.0);
  assert((row.reached_marking == std::vector<int>{1, 0, 1}));
  consistent(log_of({{"a", "b"}, {}}), net, options);
  options.variant = TokenReplayVariant::kBackwards;
  assert(!replay_token_traces(log_of({{"a", "b"}}), net, options).front().trace_is_fit);
  consistent(log_of({{"a", "b"}, {}}), net, options);
}

void precision_uses_selected_hidden_routing_policy() {
  auto net = sequential(true);
  net.set_activities({"a", "b", "c"});
  net.add_transition(2, {0}, {2});
  const auto log = log_of({{"a", "b"}});
  const auto routed = check_conformance(log, net);
  TokenReplayOptions options;
  options.walk_through_hidden_transitions = false;
  const auto direct = check_conformance(log, net, options);
  // Prefix replay changes with the routing policy, while the enabled model
  // alternatives always include visible steps behind silent transitions.
  assert(std::abs(routed.precision - 2.0 / 3.0) < 1e-12);
  assert(std::abs(direct.precision - 2.0 / 5.0) < 1e-12);
}

void unknown_positions_and_custom_activity_selector() {
  auto net = sequential();
  net.set_activities({"a", "b", "unused"});
  auto log = log_of({{"a", "unused", "missing", "b", "missing"}});
  TokenReplayOptions options;
  options.consider_activities_not_in_model_in_fitness = false;
  const auto row = replay_token_traces(log, net, options).front();
  assert(row.trace_is_fit);
  assert((row.unknown_event_indices == std::vector<std::size_t>{1, 2, 4}));
  assert((row.unknown_activities == std::vector<std::string>{"unused", "missing", "missing"}));
  for (auto& event : log.traces[0].events) {
    EventAttribute attribute;
    attribute.key = "selected";
    attribute.value = event.activity;
    event.attributes.push_back(std::move(attribute));
    event.activity = "ignored";
  }
  options.activity_attribute = "selected";
  const auto selected = replay_token_traces(log, net, options).front();
  assert(selected.activated_transitions == row.activated_transitions);
  assert(selected.unknown_activities == row.unknown_activities);
  consistent(log, net, options);
}
}  // namespace

int main() {
  ordered_trace_rows_and_markings();
  hidden_routing_policies_and_enabled_ids();
  deviations_unknowns_and_stopping();
  backward_contract_and_duplicate_labels();
  explicit_markings_and_remaining_classification();
  precision_uses_selected_hidden_routing_policy();
  unknown_positions_and_custom_activity_selector();
  std::cout << "Token replay diagnostics tests passed\n";
}
