#include "minera/conformance.hpp"
#include "minera/bpmn.hpp"
#include "minera/pnml.hpp"
#include "minera/petri_usage_comparison.hpp"
#include "../tools/pnml_model_options.hpp"
#include "../petri_replay_internal.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

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

EventAttribute selected_activity(std::string value,
                                 AttributeKind kind = AttributeKind::kString,
                                 bool is_null = false) {
  EventAttribute attribute;
  attribute.key = "user:activity";
  attribute.value = std::move(value);
  attribute.kind = kind;
  attribute.is_null = is_null;
  if (kind == AttributeKind::kInt && !is_null)
    attribute.typed_value = std::int64_t{1};
  if (is_null)
    attribute.first_class_state = FirstClassAttributeState::kNull;
  return attribute;
}

EventLog typed_activity_fixture() {
  EventLog log;
  const auto string_a = selected_activity("A");
  auto string_a_with_child_metadata = string_a;
  string_a_with_child_metadata.children = {{"detail", "retained"}};
  const auto integer_one = selected_activity("1", AttributeKind::kInt);
  const auto string_one = selected_activity("1");
  const auto null_activity = selected_activity("", AttributeKind::kString, true);
  const auto empty_activity = selected_activity("");
  const auto add_trace = [&](std::initializer_list<EventAttribute> attributes) {
    Trace trace;
    trace.case_id = "typed-" + std::to_string(log.traces.size());
    for (const auto& attribute : attributes) {
      Event event;
      event.attributes.push_back(attribute);
      trace.events.push_back(std::move(event));
    }
    log.traces.push_back(std::move(trace));
  };
  add_trace({string_a, null_activity, integer_one});
  add_trace({string_a, integer_one});
  add_trace({string_a, string_one});
  add_trace({string_a, empty_activity});
  // A scalar string's selected activity identity is its value. Retained
  // child metadata must not split an otherwise equal replay variant.
  add_trace({string_a_with_child_metadata, null_activity, integer_one});
  add_trace({string_a, integer_one});
  return log;
}

void consistent(const EventLog& log, const PetriNet& net, TokenReplayOptions options) {
  const auto rows = replay_token_traces(log, net, options);
  const auto aggregate = check_conformance(log, net, options);
  const auto places = replay_token_diagnostics(log, net, options);
  assert(!aggregate.trace_diagnostics);
  minera::TokenInteger produced = 0, consumed = 0, missing = 0, remaining = 0;
  std::uint64_t fitting = 0;
  std::vector<std::uint64_t> fired(net.transitions().size());
  std::vector<TokenReplayPlaceDiagnostics> totals(net.place_count());
  for (const auto& row : rows) {
    assert(row.event_diagnostics.size() == row.replayed_events);
    assert(row.replayed_events <= log.traces[row.trace_index].events.size());
    std::vector<std::size_t> event_firings, event_problems;
    for (std::size_t index = 0; index < row.event_diagnostics.size(); ++index) {
      const auto& event = row.event_diagnostics[index];
      assert(event.event_index == index);
      event_firings.insert(event_firings.end(), event.activated_transitions.begin(),
                          event.activated_transitions.end());
      event_problems.insert(event_problems.end(), event.transitions_with_problems.begin(),
                           event.transitions_with_problems.end());
    }
    assert(event_problems == row.transitions_with_problems);
    assert(event_firings.size() <= row.activated_transitions.size());
    assert(std::equal(event_firings.begin(), event_firings.end(), row.activated_transitions.begin()));
    for (std::size_t i = event_firings.size(); i < row.activated_transitions.size(); ++i)
      assert(!net.transitions()[row.activated_transitions[i]].activity);
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
    const auto& events = (*attached.trace_diagnostics)[i].event_diagnostics;
    assert(events.size() == rows[i].event_diagnostics.size());
    for (std::size_t e = 0; e < events.size(); ++e) {
      assert(events[e].event_index == rows[i].event_diagnostics[e].event_index);
      assert(events[e].activated_transitions == rows[i].event_diagnostics[e].activated_transitions);
      assert(events[e].transitions_with_problems == rows[i].event_diagnostics[e].transitions_with_problems);
    }
  }
}

void expect_backwards_fitness_undefined(const EventLog& log, const PetriNet& net,
                                       TokenReplayOptions options = {}) {
  options.variant = TokenReplayVariant::kBackwards;
  constexpr const char* diagnostic =
      "backwards token-replay fitness is undefined when consumed or "
      "produced tokens are zero";
  const auto rejects_fitness = [&](auto&& call) {
    bool rejected = false;
    try {
      call();
    } catch (const std::domain_error& error) {
      rejected = true;
      assert(std::string(error.what()) == diagnostic);
    }
    assert(rejected);
  };
  rejects_fitness([&] { (void)replay_token_traces(log, net, options); });
  rejects_fitness([&] { (void)check_conformance(log, net, options); });

  // This diagnostics API exposes token counts but requests no fitness value.
  const auto counts = replay_token_diagnostics(log, net, options);
  assert(counts.trace_count == log.traces.size());
}

void assert_forward_fitness_remains_finite(const EventLog& log, const PetriNet& net,
                                           TokenReplayOptions options = {}) {
  options.variant = TokenReplayVariant::kForward;
  const auto rows = replay_token_traces(log, net, options);
  for (const auto& row : rows) assert(std::isfinite(row.trace_fitness));
  const auto aggregate = check_conformance(log, net, options);
  assert(std::isfinite(aggregate.fitness));
  for (const auto& variant : aggregate.variants)
    assert(std::isfinite(variant.fitness));
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

PetriNet exhaustive_competing_route_net();

void enabled_transition_worklist_public_contract() {
  const auto empty_trace = log_of({{}});

  // A silent transition can be fired repeatedly on successive markings.  The
  // worklist must retain its latest marking for the duplicate queue entries;
  // deduplicating the transition at enqueue time misses the visible step.
  PetriNet finite_repeat;
  finite_repeat.set_activities({"A"});
  finite_repeat.add_place();  // p0
  finite_repeat.add_place();  // a
  finite_repeat.add_place();  // sink
  finite_repeat.set_source(0);
  finite_repeat.set_sink(2);
  finite_repeat.add_transition(std::nullopt, {0}, {1});
  finite_repeat.add_transition(0, {1, 1}, {2});
  for (const int tokens : {1, 2, 3}) {
    TokenReplayOptions options;
    options.initial_marking = {tokens, 0, 0};
    options.final_marking = {0, 0, 0};
    options.try_to_reach_final_marking_through_hidden = false;
    const auto row = replay_token_traces(empty_trace, finite_repeat, options).front();
    assert(row.enabled_transitions == (tokens == 1
        ? std::vector<std::size_t>{}
        : std::vector<std::size_t>{1}));
  }

  // Weighted input arcs must be checked with their full multiplicity.
  PetriNet weighted;
  weighted.set_activities({"A"});
  weighted.add_place();  // p0
  weighted.add_place();  // a
  weighted.add_place();  // sink
  weighted.set_source(0);
  weighted.set_sink(2);
  weighted.add_transition(std::nullopt, {0, 0}, {1});
  weighted.add_transition(0, {1}, {2});
  for (const int tokens : {1, 2, 3}) {
    TokenReplayOptions options;
    options.initial_marking = {tokens, 0, 0};
    options.final_marking = {0, 0, 0};
    options.try_to_reach_final_marking_through_hidden = false;
    const auto row = replay_token_traces(empty_trace, weighted, options).front();
    assert(row.enabled_transitions == (tokens == 1
        ? std::vector<std::size_t>{}
        : std::vector<std::size_t>{1}));
  }

  // A finite self-loop must not be revisited forever when its marking is
  // unchanged, while the visible transition at that marking remains enabled.
  PetriNet self_loop;
  self_loop.set_activities({"A"});
  self_loop.add_place();
  self_loop.add_place();
  self_loop.set_source(0);
  self_loop.set_sink(1);
  self_loop.add_transition(std::nullopt, {0}, {0});
  self_loop.add_transition(0, {0}, {1});
  TokenReplayOptions self_loop_options;
  self_loop_options.initial_marking = {1, 0};
  self_loop_options.final_marking = {0, 0};
  self_loop_options.try_to_reach_final_marking_through_hidden = false;
  assert(replay_token_traces(empty_trace, self_loop, self_loop_options).front()
             .enabled_transitions == std::vector<std::size_t>{1});

  // Duplicate queue entries from many unchanged self-loops remain bounded by
  // the distinct transition/marking expansion limit rather than an arbitrary
  // fixed queue-size cutoff.
  PetriNet many_self_loops;
  many_self_loops.set_activities({"A"});
  many_self_loops.add_place();
  many_self_loops.add_place();
  many_self_loops.set_source(0);
  many_self_loops.set_sink(1);
  for (int i = 0; i < 129; ++i)
    many_self_loops.add_transition(std::nullopt, {0}, {0});
  many_self_loops.add_transition(0, {0}, {1});
  TokenReplayOptions many_self_loop_options;
  many_self_loop_options.initial_marking = {1, 0};
  many_self_loop_options.final_marking = {0, 0};
  many_self_loop_options.try_to_reach_final_marking_through_hidden = false;
  assert(replay_token_traces(empty_trace, many_self_loops, many_self_loop_options)
             .front().enabled_transitions == std::vector<std::size_t>{129});

  // The unbounded token-producing variant is rejected by the native diagnostic
  // search guard instead of silently returning a partial enabled set.
  PetriNet producer;
  producer.set_activities({"A"});
  producer.add_place();
  producer.add_place();
  producer.add_place();
  producer.set_source(0);
  producer.set_sink(2);
  producer.add_transition(std::nullopt, {0}, {0, 1});
  producer.add_transition(0, {1, 1}, {2});
  TokenReplayOptions producer_options;
  producer_options.initial_marking = {1, 0, 0};
  producer_options.final_marking = {0, 0, 0};
  producer_options.try_to_reach_final_marking_through_hidden = false;
  bool producer_rejected = false;
  try {
    (void)replay_token_traces(empty_trace, producer, producer_options);
  } catch (const std::runtime_error&) {
    producer_rejected = true;
  }
  assert(producer_rejected);

  // Effective transition names, including metadata supplied by callers, set
  // the traversal order.  Renaming the competing branch changes reachability.
  for (const auto& names_and_expected : {
      std::pair<std::vector<std::string>, std::vector<std::size_t>>{
          {"visible", "t0", "t1", "t3", "t4"}, {}},
      std::pair<std::vector<std::string>, std::vector<std::size_t>>{
          {"visible", "t0", "t1", "a", "t4"}, {0}}}) {
    auto net = exhaustive_competing_route_net();
    TokenReplayOptions options;
    options.initial_marking = {1, 1, 0, 0, 0, 0};
    options.final_marking = {0, 0, 0, 0, 0, 0};
    options.try_to_reach_final_marking_through_hidden = false;
    options.transition_names = names_and_expected.first;
    const auto row = replay_token_traces(empty_trace, net, options).front();
    assert(row.enabled_transitions == names_and_expected.second);
  }

  // This weighted competing-route fixture catches state-at-enqueue
  // deduplication: t0 must be replayed from its refreshed marking before the
  // visible transition can be discovered.
  PetriNet weighted_competing;
  weighted_competing.set_activities({"A"});
  for (int i = 0; i < 7; ++i) weighted_competing.add_place();
  weighted_competing.set_source(0);
  weighted_competing.set_sink(6);
  weighted_competing.add_transition(0, {5, 5}, {6});
  weighted_competing.add_transition(std::nullopt, {1, 1}, {3, 5});
  weighted_competing.add_transition(std::nullopt, {1}, {4});
  weighted_competing.add_transition(std::nullopt, {3}, {4, 5});
  weighted_competing.add_transition(std::nullopt, {3}, {4});
  weighted_competing.add_transition(std::nullopt, {0, 0}, {1});
  weighted_competing.add_transition(std::nullopt, {2}, {5});
  weighted_competing.add_transition(std::nullopt, {1}, {4, 5});
  weighted_competing.add_transition(std::nullopt, {3}, {5, 4});
  TokenReplayOptions weighted_options;
  weighted_options.initial_marking = {3, 0, 0, 1, 0, 0, 0};
  weighted_options.final_marking = {0, 0, 0, 0, 0, 0, 0};
  weighted_options.try_to_reach_final_marking_through_hidden = false;
  weighted_options.transition_names = {
      "visible", "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7"};
  assert(replay_token_traces(empty_trace, weighted_competing, weighted_options)
             .front().enabled_transitions == std::vector<std::size_t>{0});
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
  assert(!rows[1].replay_stopped && rows[1].replayed_events == 3);
  assert(rows[1].trace_is_fit && rows[1].trace_fitness == 1.0);
  assert((rows[1].unknown_event_indices == std::vector<std::size_t>{0}));
  assert((rows[1].unknown_activities == std::vector<std::string>{"unknown"}));
  consistent(log, net, options);
  options.consider_activities_not_in_model_in_fitness = true;
  row = replay_token_traces(log, net, options)[1];
  assert(!row.trace_is_fit && !row.replay_stopped && row.replayed_events == 3);
  assert(row.trace_fitness == 1.0 && row.missing == 0 && row.consumed == 3);
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
  const auto invalid_rows = log_of({{"a"}, {"a", "a"}, {"unknown"}, {}});
  expect_backwards_fitness_undefined(invalid_rows, net, options);
  consistent(log_of({{"a"}, {"a", "a"}}), net, options);
  const auto hidden = sequential(true);
  options.walk_through_hidden_transitions = false;
  options.stop_immediately_unfit = true;
  row = replay_token_traces(log_of({{"a", "b"}}), hidden, options).front();
  assert((row.activated_transitions == std::vector<std::size_t>{1}));
  assert(row.replay_stopped && row.replayed_events == 1);
  consistent(log_of({{"a", "b"}}), hidden, options);
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
  const auto invalid_rows = log_of({{"a", "b"}, {}});
  expect_backwards_fitness_undefined(invalid_rows, net, options);
  consistent(log_of({{"a", "b"}}), net, options);
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

void final_shortages_do_not_become_event_shortages() {
  const auto net = sequential();
  const auto log = log_of({{"a"}, {"b"}, {}, {"a", "b"}});
  TokenReplayOptions options;
  const auto strict = replay_token_traces(log, net, options);
  options.consider_remaining_in_fitness = false;
  const auto relaxed = replay_token_traces(log, net, options);
  assert(!strict[0].trace_is_fit && relaxed[0].trace_is_fit);
  assert(relaxed[0].missing == 1 && relaxed[0].remaining == 1);
  assert(relaxed[0].trace_fitness == 0.5);
  assert(!relaxed[1].trace_is_fit);  // A real missing input at event b.
  assert(relaxed[2].trace_is_fit && relaxed[2].missing == 1);
  for (std::size_t i = 0; i < strict.size(); ++i) {
    assert(strict[i].trace_fitness == relaxed[i].trace_fitness);
    assert(strict[i].missing == relaxed[i].missing);
    assert(strict[i].consumed == relaxed[i].consumed);
  }
  consistent(log, net, options);

  options.initial_marking = {1, 0, 0};
  options.final_marking = {0, 0, 2};
  auto row = replay_token_traces(log_of({{"a", "b"}}), net, options).front();
  assert(row.trace_is_fit && row.missing == 1 && row.consumed == 4);
  assert(row.places[2].missing == 1 && row.places[2].consumed == 2);
  options.stop_immediately_unfit = true;
  row = replay_token_traces(log_of({{"b", "a"}}), net, options).front();
  assert(!row.trace_is_fit && row.replay_stopped);
  consistent(log, net, options);

  options = {};
  options.stop_immediately_unfit = true;
  row = replay_token_traces(log_of({{"b", "a"}}), net, options).front();
  assert(row.produced == 1 && row.consumed == 1);
  assert(row.missing == 2 && row.remaining == 1 && row.trace_fitness == -0.5);
  options.variant = TokenReplayVariant::kBackwards;
  options.consider_remaining_in_fitness = false;
  assert(!replay_token_traces(log_of({{"a"}}), net, options).front().trace_is_fit);
  expect_backwards_fitness_undefined(log, net, options);
  consistent(log_of({{"a"}, {"b"}, {"a", "b"}}), net, options);
}

void input_event_caps_apply_before_replay() {
  const auto net = sequential();
  const auto log = log_of({{"a"}, {"a", "b"}});
  for (const auto variant : {TokenReplayVariant::kForward, TokenReplayVariant::kBackwards}) {
    TokenReplayOptions options;
    options.variant = variant;
    options.max_rows_to_materialize = 3;
    consistent(log, net, options);
    for (const std::size_t cap : {std::size_t{0}, std::size_t{2}}) {
      options.max_rows_to_materialize = cap;
      const auto rejects = [](auto call) {
        bool rejected = false;
        try { call(); } catch (const std::length_error&) { rejected = true; }
        assert(rejected);
      };
      rejects([&] { (void)check_conformance(log, net, options); });
      rejects([&] { (void)replay_token_traces(log, net, options); });
      rejects([&] { (void)replay_token_diagnostics(log, net, options); });
      rejects([&] { (void)replay_token_activation_sequences(log, net, options); });
    }
    options.max_rows_to_materialize = 0;
    const auto empty_traces = log_of({{}, {}});
    if (variant == TokenReplayVariant::kBackwards)
      expect_backwards_fitness_undefined(empty_traces, net, options);
    else
      consistent(empty_traces, net, options);
    consistent(EventLog{}, net, options);
  }
}

void unknown_policy_is_classification_only_even_when_stopping() {
  const auto net = sequential();
  const auto log = log_of({{"unknown", "a", "b"}, {"unknown"},
                           {"a", "unknown", "b", "unknown"},
                           {"unknown", "b", "a"}});
  for (const bool penalize : {false, true}) {
    for (const bool stop : {false, true}) {
      for (const auto variant : {TokenReplayVariant::kForward, TokenReplayVariant::kBackwards}) {
        TokenReplayOptions options;
        options.variant = variant;
        options.consider_activities_not_in_model_in_fitness = penalize;
        options.stop_immediately_unfit = stop;
        if (variant == TokenReplayVariant::kBackwards) {
          expect_backwards_fitness_undefined(log, net, options);
          const auto valid_log = log_of({{"unknown", "a", "b"},
                                         {"a", "unknown", "b", "unknown"},
                                         {"unknown", "b", "a"}});
          const auto rows = replay_token_traces(valid_log, net, options);
          for (const auto index : {0, 1}) {
            const auto& row = rows[index];
            assert(row.trace_is_fit);
            assert(!row.replay_stopped &&
                   row.replayed_events == valid_log.traces[index].events.size());
            assert(row.trace_fitness == 1.0 && row.produced == 3 && row.consumed == 3);
            assert(row.missing == 0 && row.remaining == 0);
            assert((row.activated_transitions == std::vector<std::size_t>{0, 1}));
          }
          assert(rows[2].unknown_activities.size() == 1);
          assert(rows[2].replay_stopped == stop);
          if (stop) assert(rows[2].replayed_events == 2);
          consistent(valid_log, net, options);
        } else {
          const auto rows = replay_token_traces(log, net, options);
          for (const auto index : {0, 2}) {
            const auto& row = rows[index];
            assert(row.trace_is_fit == !penalize);
            assert(!row.replay_stopped && row.replayed_events == log.traces[index].events.size());
            assert(row.trace_fitness == 1.0 && row.produced == 3 && row.consumed == 3);
            assert(row.missing == 0 && row.remaining == 0);
            assert((row.activated_transitions == std::vector<std::size_t>{0, 1}));
          }
          assert(rows[1].unknown_activities.size() == 1 && rows[1].replayed_events == 1);
          assert(!rows[1].replay_stopped && rows[1].activated_transitions.empty());
          assert(rows[1].missing == 1 && rows[1].produced == 1 && rows[1].remaining == 1);
          assert(rows[3].replay_stopped == stop);
          if (stop) assert(rows[3].replayed_events == 2);
          consistent(log, net, options);
        }
      }
    }
  }
}

void final_demand_and_zero_denominator_trace_fitness() {
  const auto net = sequential();
  TokenReplayOptions options;
  options.final_marking = {0, 0, 2};
  const auto row = replay_token_traces(log_of({{"a", "b"}}), net, options).front();
  // Fit classification is settled from replay shortages and residual tokens;
  // an unmet final demand changes counters/fitness but not that classification.
  assert(row.trace_is_fit && row.missing == 1 && row.remaining == 0);
  assert(row.produced == 3 && row.consumed == 4 && row.trace_fitness == 0.875);
  consistent(log_of({{"a", "b"}}), net, options);
  options.final_marking = {0, 0, 0};
  const auto empty = replay_token_traces(log_of({{}}), net, options).front();
  assert(!empty.trace_is_fit && empty.trace_fitness == 1.0);
  assert(empty.produced == 1 && empty.consumed == 0 && empty.remaining == 1);
  // Log-level token aggregation is intentionally a distinct metric.
  const auto aggregate = check_conformance(log_of({{}}), net, options);
  assert(aggregate.variants.front().fitness == 1.0 && aggregate.fitness == 0.5);
  consistent(log_of({{}}), net, options);
  options.initial_marking = {0, 0, 0};
  options.final_marking = {0, 0, 1};
  const auto no_initial = replay_token_traces(log_of({{}}), net, options).front();
  assert(no_initial.produced == 0 && no_initial.consumed == 1 && no_initial.missing == 1);
  assert(no_initial.trace_is_fit && no_initial.trace_fitness == 1.0);
  consistent(log_of({{}}), net, options);
}

void backwards_zero_denominator_fitness_is_undefined() {
  const auto linear = sequential();

  // The captured empty-marking fixture has no tokens on either side of the
  // backwards fitness equation. An unknown-only trace is ignored by replay
  // and reaches the same undefined fitness result.
  TokenReplayOptions options;
  options.initial_marking = {0, 0, 0};
  options.final_marking = {0, 0, 0};
  const auto empty_trace = log_of({{}});
  expect_backwards_fitness_undefined(empty_trace, linear, options);
  assert_forward_fitness_remains_finite(empty_trace, linear, options);
  expect_backwards_fitness_undefined(log_of({{"unknown"}}), linear, options);
  assert_forward_fitness_remains_finite(log_of({{"unknown"}}), linear, options);

  // A supplied initial token with an empty final marking gives a positive
  // produced denominator and a zero consumed denominator.
  options.initial_marking = {1, 0, 0};
  options.final_marking = {0, 0, 0};
  expect_backwards_fitness_undefined(empty_trace, linear, options);
  assert_forward_fitness_remains_finite(empty_trace, linear, options);

  // This output-free transition consumes an input when weak-fired from the
  // empty marking, but contributes no produced token.
  PetriNet output_free;
  output_free.set_activities({"A"});
  output_free.add_place();
  output_free.add_transition(0, {0}, {});
  options.initial_marking = {0};
  options.final_marking = {0};
  const auto output_free_trace = log_of({{"A"}});
  expect_backwards_fitness_undefined(output_free_trace, output_free, options);
  assert_forward_fitness_remains_finite(output_free_trace, output_free, options);

  // Both denominators are positive on an ordinary blocked transition, so its
  // backwards numeric fitness remains available.
  options.initial_marking = {0, 0, 0};
  options.final_marking = {0, 1, 0};
  options.variant = TokenReplayVariant::kBackwards;
  const auto blocked = replay_token_traces(log_of({{"a"}}), linear, options).front();
  assert(blocked.produced == 1 && blocked.consumed == 2);
  assert(blocked.missing == 1 && blocked.remaining == 0);
  assert(blocked.trace_fitness == 0.75);
  const auto aggregate = check_conformance(log_of({{"a"}}), linear, options);
  assert(aggregate.variants.size() == 1 && aggregate.variants.front().fitness == 0.75);

  // An empty log has no per-trace fitness values to compute.
  const EventLog no_traces;
  assert(replay_token_traces(no_traces, linear, options).empty());
  assert(check_conformance(no_traces, linear, options).trace_count == 0);
  assert(replay_token_diagnostics(no_traces, linear, options).trace_count == 0);
}

void activation_sequence_replay_skips_fitness_only() {
  const auto net = sequential();
  const auto log = log_of({{"a", "b"}, {"unknown", "a", "b"}});
  for (const auto variant : {TokenReplayVariant::kForward,
                             TokenReplayVariant::kBackwards}) {
    TokenReplayOptions options;
    options.variant = variant;
    const auto rows = replay_token_traces(log, net, options);
    const auto sequences = replay_token_activation_sequences(log, net, options);
    assert(sequences.size() == rows.size());
    for (std::size_t index = 0; index < rows.size(); ++index)
      assert(sequences[index] == rows[index].activated_transitions);
  }

  // Fitness remains undefined on the empty-token backwards replay, while the
  // count-only API still returns one (empty) sequence per source trace.
  TokenReplayOptions options;
  options.variant = TokenReplayVariant::kBackwards;
  options.initial_marking = {0, 0, 0};
  options.final_marking = {0, 0, 0};
  const auto empty_tokens = log_of({{"unknown"}, {}});
  const auto sequences = replay_token_activation_sequences(empty_tokens, net, options);
  assert(sequences.size() == 2 && sequences[0].empty() && sequences[1].empty());
  expect_backwards_fitness_undefined(empty_tokens, net, options);

  // An ambiguous duplicate label interrupts backwards routing before any
  // activation. Consumers that only need activation weights can still use it.
  PetriNet duplicate;
  const auto source = duplicate.add_place();
  const auto gate = duplicate.add_place();
  const auto sink = duplicate.add_place();
  duplicate.set_source(source);
  duplicate.set_sink(sink);
  duplicate.set_activities({"A"});
  duplicate.add_transition(std::nullopt, {source}, {gate});
  duplicate.add_transition(0, {gate}, {sink});
  duplicate.add_transition(0, {gate}, {sink});
  const auto ambiguous = log_of({{"A"}});
  options = {};
  options.variant = TokenReplayVariant::kBackwards;
  const auto ambiguous_sequences =
      replay_token_activation_sequences(ambiguous, duplicate, options);
  assert(ambiguous_sequences.size() == 1 && ambiguous_sequences[0].empty());
  expect_backwards_fitness_undefined(ambiguous, duplicate, options);

  // The count-only entry point retains the same replay-context validation.
  options.initial_marking = {1};
  bool rejected = false;
  try {
    (void)replay_token_activation_sequences(log_of({{"a"}}), net, options);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

void null_activity_is_unknown_not_an_empty_label() {
  PetriNet net;
  net.set_activities({""});
  net.add_place();
  net.add_place();
  net.set_source(0);
  net.set_sink(1);
  net.add_transition(0, {0}, {1});
  auto log = log_of({{""}, {""}, {""}});
  log.traces[0].events[0].activity_state = FirstClassAttributeState::kNull;
  log.traces[1].events[0].activity_state = FirstClassAttributeState::kPresent;
  log.traces[2].events[0].activity_state = FirstClassAttributeState::kNull;
  TokenReplayOptions options;
  const auto rows = replay_token_traces(log, net, options);
  assert(rows[0].activated_transitions.empty() && rows[0].replayed_events == 1);
  assert((rows[0].unknown_activities == std::vector<std::string>{""}));
  assert((rows[0].unknown_activity_is_null == std::vector<bool>{true}));
  assert((rows[1].activated_transitions == std::vector<std::size_t>{0}));
  assert(rows[1].unknown_activities.empty() && rows[1].trace_is_fit);
  const auto aggregate = check_conformance(log, net, options);
  assert(aggregate.variants.size() == 2);
  assert(aggregate.variants[0].count == 2 &&
         aggregate.variants[0].null_activity_indices == std::vector<std::size_t>{0});
  assert(aggregate.variants[1].count == 1 && aggregate.variants[1].null_activity_indices.empty());
  consistent(log, net, options);

  options.variant = TokenReplayVariant::kBackwards;
  expect_backwards_fitness_undefined(log, net, options);
  EventLog present_only;
  present_only.traces.push_back(log.traces[1]);
  const auto backwards = replay_token_traces(present_only, net, options).front();
  assert((backwards.activated_transitions == std::vector<std::size_t>{0}));
  assert(backwards.unknown_activities.empty() && backwards.trace_is_fit);
  consistent(present_only, net, options);
  // An exact nullable selector wins over a non-null canonical projection.
  auto custom = log_of({{"not-selected"}});
  EventAttribute selected;
  selected.key = "chosen";
  selected.is_null = true;
  custom.traces[0].events[0].attributes.push_back(selected);
  options = {};
  options.activity_attribute = "chosen";
  assert(replay_token_traces(custom, net, options)[0].unknown_activity_is_null[0]);
  // Provenance-only selection preserves the same explicit-null semantics.
  auto& attribute = custom.traces[0].events[0].attributes[0];
  attribute.key = "transported";
  attribute.first_class_source_key = "chosen";
  assert(replay_token_traces(custom, net, options)[0].unknown_activity_is_null[0]);
  custom.traces[0].events[0].attributes.clear();
  bool rejected = false;
  try { (void)replay_token_traces(custom, net, options); }
  catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected);
}

void typed_activity_values_do_not_alias_string_labels() {
  auto net = sequential();
  net.set_activities({"A", "1"});
  const auto log = typed_activity_fixture();
  TokenReplayOptions options;
  options.activity_attribute = "user:activity";

  const auto rows = replay_token_traces(log, net, options);
  assert(rows.size() == 6);
  assert((rows[0].activated_transitions == std::vector<std::size_t>{0}));
  assert((rows[0].unknown_event_indices == std::vector<std::size_t>{1, 2}));
  assert((rows[0].unknown_activities == std::vector<std::string>{"", "1"}));
  assert((rows[0].unknown_activity_is_null == std::vector<bool>{true, false}));
  assert((rows[1].activated_transitions == std::vector<std::size_t>{0}));
  assert((rows[1].unknown_event_indices == std::vector<std::size_t>{1}));
  assert((rows[1].unknown_activities == std::vector<std::string>{"1"}));
  assert((rows[1].unknown_activity_is_null == std::vector<bool>{false}));
  assert((rows[2].activated_transitions == std::vector<std::size_t>{0, 1}));
  assert(rows[2].unknown_event_indices.empty() && rows[2].trace_is_fit);
  assert((rows[3].activated_transitions == std::vector<std::size_t>{0}));
  assert((rows[3].unknown_event_indices == std::vector<std::size_t>{1}));
  assert((rows[3].unknown_activities == std::vector<std::string>{""}));
  assert((rows[3].unknown_activity_is_null == std::vector<bool>{false}));

  const auto aggregate = check_conformance(log, net, options);
  assert(aggregate.variants.size() == 4);
  std::uint64_t int_one_count = 0;
  std::uint64_t string_one_count = 0;
  std::uint64_t null_count = 0;
  std::uint64_t empty_count = 0;
  for (const auto& variant : aggregate.variants) {
    if (variant.activities == std::vector<std::string>{"A", "1"}) {
      if (variant.count == 2) int_one_count += variant.count;
      if (variant.count == 1) string_one_count += variant.count;
    } else if (variant.activities == std::vector<std::string>{"A", "", "1"}) {
      if (variant.count == 2 &&
          variant.null_activity_indices == std::vector<std::size_t>{1})
        null_count += variant.count;
    } else if (variant.activities == std::vector<std::string>{"A", ""}) {
      if (variant.count == 1 && variant.null_activity_indices.empty())
        empty_count += variant.count;
    }
  }
  assert(int_one_count == 2 && string_one_count == 1);
  assert(null_count == 2 && empty_count == 1);
  assert((aggregate.transition_frequencies == std::vector<std::uint64_t>{6, 1}));

  // Standard selectors use the retained first-class scalar metadata instead
  // of the lossy string projection in Event::activity.
  Event typed_metadata_event;
  typed_metadata_event.activity = "1";
  typed_metadata_event.activity_state = FirstClassAttributeState::kPresent;
  EventAttribute typed_metadata;
  typed_metadata.key = "concept:name";
  typed_metadata.value = "1";
  typed_metadata.kind = AttributeKind::kInt;
  typed_metadata.typed_value = std::int64_t{1};
  typed_metadata.first_class_role = FirstClassAttributeRole::kActivity;
  typed_metadata.first_class_state = FirstClassAttributeState::kPresent;
  typed_metadata_event.activity_attribute = typed_metadata;
  EventLog metadata_log;
  Trace metadata_trace;
  metadata_trace.case_id = "metadata";
  metadata_trace.events.push_back(std::move(typed_metadata_event));
  metadata_log.traces.push_back(std::move(metadata_trace));
  const auto metadata_row = replay_token_traces(metadata_log, net).front();
  assert(metadata_row.activated_transitions.empty());
  assert((metadata_row.unknown_event_indices == std::vector<std::size_t>{0}));
  assert((metadata_row.unknown_activities == std::vector<std::string>{"1"}));
  assert((metadata_row.unknown_activity_is_null == std::vector<bool>{false}));
}

void backwards_module_acceptance() {
  auto net = sequential();
  net.set_activities({"A", "1"});
  const auto log = typed_activity_fixture();
  TokenReplayOptions options;
  options.variant = TokenReplayVariant::kBackwards;
  options.activity_attribute = "user:activity";

  const auto rows = replay_token_traces(log, net, options);
  assert(rows.size() == 6);
  struct ExpectedTrace {
    bool fit;
    double fitness;
    std::uint64_t produced;
    std::uint64_t consumed;
    std::uint64_t missing;
    std::uint64_t remaining;
    std::vector<std::size_t> activated;
    std::vector<std::size_t> unknown_indices;
    std::vector<std::string> unknown_activities;
    std::vector<bool> unknown_is_null;
    std::vector<int> marking;
    std::vector<std::size_t> enabled;
    std::vector<std::vector<std::size_t>> event_activations;
  };
  const std::vector<ExpectedTrace> expected{
      {false, 0.25, 2, 1, 1, 1, {0}, {1, 2}, {"", "1"}, {true, false},
       {0, 1, 0}, {1}, {{0}, {}, {}}},
      {false, 0.25, 2, 1, 1, 1, {0}, {1}, {"1"}, {false},
       {0, 1, 0}, {1}, {{0}, {}}},
      {true, 1.0, 3, 3, 0, 0, {0, 1}, {}, {}, {},
       {0, 0, 1}, {}, {{0}, {1}}},
      {false, 0.25, 2, 1, 1, 1, {0}, {1}, {""}, {false},
       {0, 1, 0}, {1}, {{0}, {}}},
      {false, 0.25, 2, 1, 1, 1, {0}, {1, 2}, {"", "1"}, {true, false},
       {0, 1, 0}, {1}, {{0}, {}, {}}},
      {false, 0.25, 2, 1, 1, 1, {0}, {1}, {"1"}, {false},
       {0, 1, 0}, {1}, {{0}, {}}},
  };
  for (std::size_t i = 0; i < rows.size(); ++i) {
    const auto& row = rows[i];
    const auto& want = expected[i];
    assert(row.trace_index == i && row.case_id == "typed-" + std::to_string(i));
    assert(row.trace_is_fit == want.fit && row.trace_fitness == want.fitness);
    assert(row.produced == want.produced && row.consumed == want.consumed);
    assert(row.missing == want.missing && row.remaining == want.remaining);
    assert(row.activated_transitions == want.activated);
    assert(row.transitions_with_problems.empty() && !row.replay_stopped);
    assert(row.unknown_event_indices == want.unknown_indices);
    assert(row.unknown_activities == want.unknown_activities);
    assert(row.unknown_activity_is_null == want.unknown_is_null);
    assert(row.reached_marking == want.marking);
    assert(row.enabled_transitions == want.enabled);
    assert(row.replayed_events == want.event_activations.size());
    assert(row.event_diagnostics.size() == want.event_activations.size());
    for (std::size_t event = 0; event < want.event_activations.size(); ++event) {
      assert(row.event_diagnostics[event].event_index == event);
      assert(row.event_diagnostics[event].activated_transitions ==
             want.event_activations[event]);
      assert(row.event_diagnostics[event].transitions_with_problems.empty());
    }
    assert(row.places.size() == 3);
    if (want.fit) {
      for (const auto& place : row.places)
        assert(place.produced == 1 && place.consumed == 1 &&
               place.missing == 0 && place.remaining == 0);
    } else {
      assert(row.places[0].produced == 1 && row.places[0].consumed == 1 &&
             row.places[0].missing == 0 && row.places[0].remaining == 0);
      assert(row.places[1].produced == 1 && row.places[1].consumed == 0 &&
             row.places[1].missing == 0 && row.places[1].remaining == 1);
      assert(row.places[2].produced == 0 && row.places[2].consumed == 0 &&
             row.places[2].missing == 1 && row.places[2].remaining == 0);
    }
  }

  const auto aggregate = check_conformance(log, net, options);
  assert(aggregate.trace_count == 6 && aggregate.fitting_traces == 1);
  assert(aggregate.produced == 13 && aggregate.consumed == 8);
  assert(aggregate.missing == 5 && aggregate.remaining == 5);
  assert((aggregate.transition_frequencies == std::vector<std::uint64_t>{6, 1}));
  assert(aggregate.variants.size() == 4);
  std::uint64_t typed_integer_count = 0;
  std::uint64_t string_label_count = 0;
  std::uint64_t nullable_count = 0;
  for (const auto& variant : aggregate.variants) {
    if (variant.activities == std::vector<std::string>{"A", "1"}) {
      if (variant.count == 2) typed_integer_count += variant.count;
      if (variant.count == 1) string_label_count += variant.count;
    } else if (variant.activities == std::vector<std::string>{"A", "", "1"} &&
               variant.null_activity_indices == std::vector<std::size_t>{1}) {
      nullable_count += variant.count;
    }
  }
  assert(typed_integer_count == 2 && string_label_count == 1);
  assert(nullable_count == 2);
}

void replay_selector_priority_and_missing_state() {
  auto log = log_of({{"not-selected", "b"}});
  auto& event = log.traces.front().events.front();
  EventAttribute low_one;
  low_one.key = "transported-one";
  low_one.value = "wrong";
  low_one.first_class_source_key = "phase";
  auto low_two = low_one;
  low_two.key = "transported-two";
  EventAttribute exact;
  exact.key = "phase";
  exact.value = "a";
  EventAttribute second;
  second.key = "phase";
  second.value = "b";
  log.traces.front().events.back().attributes = {second};
  TokenReplayOptions options;
  options.activity_attribute = "phase";
  event.attributes = {low_one, low_two, exact};
  assert(replay_token_traces(log, sequential(), options).front().trace_is_fit);
  event.attributes = {exact, low_two, low_one};
  assert(replay_token_traces(log, sequential(), options).front().trace_is_fit);
  for (const auto& attributes : {
           std::vector<EventAttribute>{exact, exact},
           std::vector<EventAttribute>{low_one, low_two}}) {
    event.attributes = attributes;
    bool rejected = false;
    try { (void)replay_token_traces(log, sequential(), options); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
  }
  exact.first_class_role = FirstClassAttributeRole::kActivity;
  exact.first_class_state = FirstClassAttributeState::kMissing;
  event.attributes = {exact};
  bool rejected = false;
  try { (void)replay_token_traces(log, sequential(), options); }
  catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected);
  event.attributes.front().first_class_state = FirstClassAttributeState::kNull;
  assert(replay_token_traces(log, sequential(), options).front().unknown_activity_is_null.front());
}

void event_records_preserve_routing_and_source_positions() {
  const auto net = sequential(true);
  const auto log = log_of({{"unknown", "a", "unknown", "b"}, {}});
  for (const auto variant : {TokenReplayVariant::kForward, TokenReplayVariant::kBackwards}) {
    TokenReplayOptions options;
    options.variant = variant;
    EventLog route_log = log;
    if (variant == TokenReplayVariant::kBackwards) {
      expect_backwards_fitness_undefined(log, net, options);
      route_log.traces.resize(1);
    }
    const auto rows = replay_token_traces(route_log, net, options);
    const auto& events = rows[0].event_diagnostics;
    assert(events.size() == 4);
    assert(events[0].activated_transitions.empty() && events[0].transitions_with_problems.empty());
    assert((events[1].activated_transitions == std::vector<std::size_t>{0, 1}));
    assert(events[2].activated_transitions.empty() && events[2].transitions_with_problems.empty());
    assert((events[3].activated_transitions == std::vector<std::size_t>{2}));
    assert(rows[0].activated_transitions.size() ==
           (variant == TokenReplayVariant::kForward ? 4 : 3));
    if (variant == TokenReplayVariant::kForward)
      assert(rows[1].event_diagnostics.empty());
    consistent(route_log, net, options);
  }
  // A silent-only completion has no source event to which it can be assigned.
  PetriNet silent;
  silent.add_place();
  silent.add_place();
  silent.set_source(0);
  silent.set_sink(1);
  silent.add_transition(std::nullopt, {0}, {1});
  const auto row = replay_token_traces(log_of({{}}), silent).front();
  assert((row.activated_transitions == std::vector<std::size_t>{0}));
  assert(row.event_diagnostics.empty() && row.replayed_events == 0);
  consistent(log_of({{}}), silent, {});
}

void event_records_preserve_repeated_problems_and_stop_boundaries() {
  const auto net = sequential();
  const auto log = log_of({{"unknown", "b", "b", "a"}});
  for (const auto variant : {TokenReplayVariant::kForward, TokenReplayVariant::kBackwards}) {
    for (const bool stop : {false, true}) {
      TokenReplayOptions options;
      options.variant = variant;
      options.stop_immediately_unfit = stop;
      const auto row = replay_token_traces(log, net, options).front();
      assert(row.event_diagnostics.size() == (stop ? 2 : 4));
      assert(row.event_diagnostics[0].activated_transitions.empty());
      assert(row.event_diagnostics[0].transitions_with_problems.empty());
      assert((row.event_diagnostics[1].transitions_with_problems == std::vector<std::size_t>{1}));
      if (stop && variant == TokenReplayVariant::kForward)
        assert(row.event_diagnostics[1].activated_transitions.empty());
      else
        assert((row.event_diagnostics[1].activated_transitions == std::vector<std::size_t>{1}));
      if (!stop) {
        assert((row.event_diagnostics[2].activated_transitions == std::vector<std::size_t>{1}));
        assert((row.event_diagnostics[2].transitions_with_problems == std::vector<std::size_t>{1}));
        assert((row.event_diagnostics[3].activated_transitions == std::vector<std::size_t>{0}));
        assert(row.event_diagnostics[3].transitions_with_problems.empty());
      }
      consistent(log, net, options);
    }
  }
}

void backward_event_records_group_duplicate_firings_and_ambiguity() {
  PetriNet net;
  net.set_activities({"a"});
  for (int i = 0; i < 3; ++i) net.add_place();
  net.set_source(0);
  net.set_sink(2);
  net.add_transition(0, {0}, {1});
  net.add_transition(0, {1}, {2});
  TokenReplayOptions options;
  options.variant = TokenReplayVariant::kBackwards;
  const auto log = log_of({{"a", "a", "unknown"}});
  const auto row = replay_token_traces(log, net, options).front();
  assert(row.replay_stopped && row.event_diagnostics.size() == 2);
  assert((row.event_diagnostics[0].activated_transitions == std::vector<std::size_t>{0, 1}));
  assert(row.event_diagnostics[0].transitions_with_problems.empty());
  assert(row.event_diagnostics[1].activated_transitions.empty());
  assert(row.event_diagnostics[1].transitions_with_problems.empty());
  assert(row.unknown_event_indices.empty());  // The third event was never examined.
  consistent(log, net, options);
}

void replay_variant_keys_are_unambiguous() {
  auto log = log_of({{"a", "b"}, {"a"}, {"a", "b"}});
  log.traces[1].events[0].activity = std::string("a") + '\x1f' + "b";
  const auto aggregate = check_conformance(log, sequential());
  assert(aggregate.variants.size() == 2);
  std::uint64_t ordinary = 0, combined = 0;
  for (const auto& variant : aggregate.variants) {
    if (variant.activities.size() == 2) ordinary = variant.count;
    else combined = variant.count;
  }
  assert(ordinary == 2 && combined == 1);
}

void forward_cleaning_public_counter_contract() {
  const auto net = sequential();
  TokenReplayOptions options;
  const auto log = log_of({{"b", "a"}});
  const auto ordinary = replay_token_traces(log, net, options).front();
  options.cleaning_token_flood = true;
  const auto cleaned = replay_token_traces(log, net, options).front();
  assert(cleaned.produced == 3 && cleaned.consumed == 3);
  assert(cleaned.missing == 3 && cleaned.remaining == 3 && cleaned.trace_fitness == 0.0);
  assert(cleaned.remaining > ordinary.remaining);
  assert((cleaned.reached_marking == std::vector<int>{0, 1, 0}));
  assert((cleaned.reached_marking_keys == std::vector<bool>{false, true, false}));
  for (const auto& place : cleaned.places)
    assert(place.produced == 1 && place.consumed == 1 && place.missing == 1 && place.remaining == 1);
  consistent(log, net, options);

  options.initial_marking = {3, 0, 0};
  auto row = replay_token_traces(log_of({{"b"}}), net, options).front();
  assert(row.produced == 4 && row.consumed == 2 && row.missing == 1 && row.remaining == 1);
  assert(row.trace_fitness == 0.625 && row.places[0].remaining == 1);
  options.initial_marking = {0, 0, 0};
  options.initial_marking_keys = {true, false, false};
  row = replay_token_traces(log_of({{"b"}}), net, options).front();
  assert(row.produced == 1 && row.consumed == 2 && row.missing == 1 && row.remaining == 1);
  assert(row.places[0].remaining == 1 && row.trace_fitness == 0.25);
  consistent(log_of({{"b"}}), net, options);

  options = {};
  options.variant = TokenReplayVariant::kBackwards;
  const auto backwards = replay_token_traces(log, net, options).front();
  options.cleaning_token_flood = true;
  const auto unchanged = replay_token_traces(log, net, options).front();
  assert(unchanged.reached_marking == backwards.reached_marking);
  assert(unchanged.missing == backwards.missing && unchanged.remaining == backwards.remaining);
}

void forward_weighted_repair_and_partial_stop_contract() {
  PetriNet weighted;
  weighted.add_place(); weighted.add_place();
  weighted.set_source(0); weighted.set_sink(1); weighted.set_activities({"a"});
  weighted.add_transition(0, {0, 0, 0}, {1});
  const auto repaired = replay_token_traces(log_of({{"a"}}), weighted).front();
  assert(repaired.produced == 2 && repaired.consumed == 4);
  assert(repaired.missing == 2 && repaired.remaining == 1 && repaired.trace_fitness == 0.5);
  assert((repaired.reached_marking == std::vector<int>{1, 1}));
  consistent(log_of({{"a"}}), weighted, {});

  PetriNet partial;
  for (int i = 0; i < 5; ++i) partial.add_place();
  partial.set_source(0); partial.set_sink(4); partial.set_activities({"a"});
  partial.add_transition(std::nullopt, {0}, {1});
  partial.add_transition(std::nullopt, {1, 2}, {3});
  partial.add_transition(0, {3}, {4});
  TokenReplayOptions options;
  options.try_to_reach_final_marking_through_hidden = false;
  options.stop_immediately_unfit = true;
  const auto stopped = replay_token_traces(log_of({{"a"}}), partial, options).front();
  assert(stopped.replay_stopped && stopped.replayed_events == 1);
  assert((stopped.activated_transitions == std::vector<std::size_t>{0}));
  assert((stopped.transitions_with_problems == std::vector<std::size_t>{2}));
  assert(stopped.produced == 2 && stopped.consumed == 2);
  assert(stopped.missing == 2 && stopped.remaining == 1 && stopped.trace_fitness == 0.25);
  consistent(log_of({{"a"}}), partial, options);
}

void forward_final_zero_key_and_path_boundary_contract() {
  PetriNet silent;
  silent.add_place(); silent.add_place();
  silent.set_source(0); silent.set_sink(1);
  silent.add_transition(std::nullopt, {0}, {1});
  TokenReplayOptions options;
  options.final_marking = {0, 0};
  options.final_marking_keys = {false, true};
  const auto absent = replay_token_traces(log_of({{}}), silent, options).front();
  assert((absent.activated_transitions == std::vector<std::size_t>{0}));
  assert(absent.produced == 2 && absent.consumed == 1 && absent.remaining == 1);
  options.initial_marking_keys = {true, true};
  const auto present = replay_token_traces(log_of({{}}), silent, options).front();
  assert(present.activated_transitions.empty());
  assert((present.reached_marking_keys == std::vector<bool>{true, true}));
  assert(present.produced == 1 && present.consumed == 0 && present.trace_fitness == 1.0);
  consistent(log_of({{}}), silent, options);
  for (const std::size_t depth : {51, 52}) {
    PetriNet chain;
    for (std::size_t place = 0; place <= depth; ++place) chain.add_place();
    chain.set_source(0); chain.set_sink(depth);
    for (std::size_t place = 0; place < depth; ++place)
      chain.add_transition(std::nullopt, {place}, {place + 1});
    const auto row = replay_token_traces(log_of({{}}), chain).front();
    assert(row.activated_transitions.size() == (depth == 51 ? depth : 0));
    assert(row.missing == (depth == 51 ? 0 : 1));
  }
}

void forward_empty_log_cleaning_initialization_contract() {
  for (const bool empty_net : {false, true}) {
    const auto net = empty_net ? PetriNet{} : sequential();
    TokenReplayOptions options;
    options.initial_marking.assign(net.place_count(), 0);
    options.initial_marking_keys.assign(net.place_count(), false);
    const EventLog log;
    assert(replay_token_traces(log, net, options).empty());
    consistent(log, net, options);
    options.cleaning_token_flood = true;
    for (int api = 0; api < 3; ++api) {
      bool rejected = false;
      try {
        if (api == 0) (void)replay_token_traces(log, net, options);
        else if (api == 1) (void)replay_token_diagnostics(log, net, options);
        else (void)check_conformance(log, net, options);
      } catch (const std::invalid_argument& error) {
        rejected = std::string(error.what()).find("initial marking") != std::string::npos;
      }
      assert(rejected);
    }
    if (!empty_net) {
      options.initial_marking_keys[0] = true;
      assert(replay_token_traces(log, net, options).empty());
      consistent(log, net, options);
    }
  }
}

void forward_extended_arc_silent_completion_contract() {
  for (const bool reset : {false, true}) {
    PetriNet net;
    net.add_place(); net.add_place();
    net.set_source(0); net.set_sink(1);
    const auto transition = net.add_transition(std::nullopt, {}, {1});
    if (reset) net.add_reset_arc(transition, 0);
    else net.add_inhibitor_arc(transition, 0);
    TokenReplayOptions options;
    options.initial_marking = {reset ? 2 : 0, 0};
    const auto log = log_of({{}});
    const auto row = replay_token_traces(log, net, options).front();
    assert((row.activated_transitions == std::vector<std::size_t>{transition}));
    assert((row.reached_marking == std::vector<int>{0, 1}));
    assert(row.missing == 0 && row.remaining == 0 && row.trace_is_fit);
    assert(row.event_diagnostics.empty());
    consistent(log, net, options);
    options.try_to_reach_final_marking_through_hidden = false;
    const auto unrouted = replay_token_traces(log, net, options).front();
    assert(unrouted.activated_transitions.empty() && unrouted.missing == 1);
  }
}

void forward_bpmn_greedy_routing_public_contract() {
  const ProcessTree inclusive(
      {"a", "b"},
      {ProcessTreeOperator::kInclusiveChoice, 0,
       {ProcessTreeNode::leaf(0), ProcessTreeNode::tau(), ProcessTreeNode::leaf(1)}});
  // The public probe imports this exact PNML. Serialization orders arc
  // endpoints, which fixes the tie between equally short silent paths.
  const auto inclusive_net = parse_pnml(to_pnml(parse_bpmn(to_bpmn(inclusive))));
  const auto empty = log_of({{}});
  const auto skipped = replay_token_traces(empty, inclusive_net).front();
  assert(!skipped.trace_is_fit);
  assert(skipped.consumed == 6 && skipped.produced == 9);
  assert(skipped.missing == 1 && skipped.remaining == 4);
  assert(std::abs(skipped.trace_fitness - 25.0 / 36.0) < 1e-12);
  assert((skipped.activated_transitions == std::vector<std::size_t>{0, 2, 7, 4}));
  assert(skipped.transitions_with_problems.empty() && skipped.event_diagnostics.empty());
  consistent(empty, inclusive_net, {});

  const ProcessTree interleaving(
      {"a", "b", "c", "d"},
      {ProcessTreeOperator::kInterleaving, 0,
       {{ProcessTreeOperator::kSequence, 0, {ProcessTreeNode::leaf(0), ProcessTreeNode::leaf(1)}},
        {ProcessTreeOperator::kSequence, 0, {ProcessTreeNode::leaf(2), ProcessTreeNode::leaf(3)}}}});
  const auto interleaving_net = parse_pnml(to_pnml(parse_bpmn(to_bpmn(interleaving))));
  const auto log = log_of({{"a", "b", "c", "d"}, {"c", "d", "a", "b"}});
  for (const auto& row : replay_token_traces(log, interleaving_net)) {
    assert(!row.trace_is_fit && row.trace_fitness == 0.6);
    assert(row.consumed == 5 && row.produced == 5);
    assert(row.missing == 2 && row.remaining == 2);
    assert(row.activated_transitions.size() == 4 && row.transitions_with_problems.size() == 2);
  }
  consistent(log, interleaving_net, {});
}

PetriNet exhaustive_minimal_net() {
  PetriNet net;
  net.set_activities({"A"});
  const auto p0 = net.add_place();
  const auto p1 = net.add_place();
  const auto p5 = net.add_place();
  const auto sink = net.add_place();
  net.set_source(p0);
  net.set_sink(sink);
  net.add_transition(0, {p5}, {sink});
  net.add_transition(std::nullopt, {p0, p1}, {p5});
  net.add_transition(std::nullopt, {p0}, {p1});
  return net;
}

PetriNet exhaustive_competing_route_net() {
  PetriNet net;
  net.set_activities({"A"});
  const auto p0 = net.add_place();
  const auto p2 = net.add_place();
  const auto p3 = net.add_place();
  const auto p4 = net.add_place();
  const auto p5 = net.add_place();
  const auto sink = net.add_place();
  net.set_source(p0);
  net.set_sink(sink);
  net.add_transition(0, {p3, p5}, {sink});
  net.add_transition(std::nullopt, {p0}, {p4});
  net.add_transition(std::nullopt, {p0}, {p2, p3});
  net.add_transition(std::nullopt, {p2}, {p4, p3});
  net.add_transition(std::nullopt, {p0, p4}, {p5});
  return net;
}

PetriNet deficient_input_route_net(std::size_t weight) {
  PetriNet net;
  net.set_activities({"A"});
  const auto p0 = net.add_place();
  const auto p1 = net.add_place();
  const auto p2 = net.add_place();
  const auto p3 = net.add_place();
  net.add_place();  // p4 is intentionally disconnected, as in the public net.
  const auto p5 = net.add_place();
  const auto sink = net.add_place();
  net.set_source(p0);
  net.set_sink(sink);
  const auto repeated = [weight](std::size_t place) {
    return std::vector<std::size_t>(weight, place);
  };
  net.add_transition(0, [&] {
    auto inputs = repeated(p3);
    const auto p5_inputs = repeated(p5);
    inputs.insert(inputs.end(), p5_inputs.begin(), p5_inputs.end());
    return inputs;
  }(), repeated(sink));
  net.add_transition(std::nullopt, repeated(p0), [&] {
    auto outputs = repeated(p1);
    const auto p3_outputs = repeated(p3);
    outputs.insert(outputs.end(), p3_outputs.begin(), p3_outputs.end());
    return outputs;
  }());
  net.add_transition(std::nullopt, [&] {
    auto inputs = repeated(p2);
    const auto p1_inputs = repeated(p1);
    inputs.insert(inputs.end(), p1_inputs.begin(), p1_inputs.end());
    return inputs;
  }(), repeated(p3));
  net.add_transition(std::nullopt, [&] {
    auto inputs = repeated(p2);
    const auto p3_inputs = repeated(p3);
    inputs.insert(inputs.end(), p3_inputs.begin(), p3_inputs.end());
    return inputs;
  }(), repeated(p5));
  net.add_transition(std::nullopt, repeated(p0), repeated(p2));
  return net;
}

PetriNet exhaustive_weighted_deficit_net() {
  PetriNet net;
  net.set_activities({"A"});
  const auto p0 = net.add_place();
  net.add_place();  // p1 is intentionally disconnected.
  const auto p2 = net.add_place();
  net.add_place();  // p3 is intentionally disconnected.
  const auto p4 = net.add_place();
  const auto p5 = net.add_place();
  const auto p6 = net.add_place();
  const auto sink = net.add_place();
  net.set_source(p0);
  net.set_sink(sink);
  net.add_transition(0, {p6, p5}, {sink});
  net.add_transition(std::nullopt, {p0}, {p2});
  net.add_transition(std::nullopt, {p2, p2}, {p4, p5});
  return net;
}

PetriNet final_path_order_net() {
  PetriNet net;
  net.set_activities({"A"});
  const auto p0 = net.add_place();
  const auto p1 = net.add_place();
  net.add_place();  // p2 is intentionally disconnected.
  const auto p3 = net.add_place();
  const auto p4 = net.add_place();
  const auto p5 = net.add_place();
  const auto p6 = net.add_place();
  const auto sink = net.add_place();
  net.set_source(p0);
  net.set_sink(sink);
  net.add_transition(0, {p6, p4, p4, p5}, {sink});
  net.add_transition(std::nullopt, {p0, p0, p1}, {p5});
  net.add_transition(std::nullopt, {p3}, {p5});
  net.add_transition(std::nullopt, {p0}, {p3, p6});
  return net;
}

PetriNet exhaustive_pool_replay_net() {
  PetriNet net;
  net.set_activities({"A"});
  const auto p0 = net.add_place();
  const auto p2 = net.add_place();
  const auto p4 = net.add_place();
  const auto p5 = net.add_place();
  const auto p6 = net.add_place();
  const auto sink = net.add_place();
  net.set_source(p0);
  net.set_sink(sink);
  net.add_transition(0, {p4, p5}, {sink});
  net.add_transition(std::nullopt, {p0}, {p2, p6});
  net.add_transition(std::nullopt, {p2}, {p5, p6});
  net.add_transition(std::nullopt, {p0}, {p2, p4});
  return net;
}

void assert_hidden_route_usage(const PetriNet& net, int initial_tokens,
                              bool exhaustive,
                              const std::vector<std::size_t>& expected) {
  PetriUsageComparisonOptions options;
  options.initial_marking.assign(net.place_count(), 0);
  options.initial_marking[0] = static_cast<std::uint32_t>(initial_tokens);
  options.final_marking.assign(net.place_count(), 0);
  options.final_marking.back() = 1;
  options.try_to_reach_final_marking_through_hidden = false;
  options.exhaustive_invisible_exploration = exhaustive;
  const auto comparison = compare_petri_usage(net, log_of({{"A"}}), EventLog{}, options);
  assert(comparison.transitions.size() == net.transitions().size());
  for (const auto& record : comparison.transitions) {
    const auto count = static_cast<std::uint64_t>(std::count(
        expected.begin(), expected.end(), record.id));
    assert(record.log1_occurrences == count);
    assert(record.log2_occurrences == 0 && record.total_occurrences == count);
  }
}

void forward_exhaustive_hidden_path_trials_public_contract() {
  const auto log = log_of({{"A"}});
  for (const bool exhaustive : {false, true}) {
    for (const int tokens : {1, 2}) {
      auto net = exhaustive_minimal_net();
      TokenReplayOptions options;
      options.initial_marking = {tokens, 0, 0, 0};
      options.final_marking = {0, 0, 0, 1};
      options.exhaustive_invisible_exploration = exhaustive;
      const auto row = replay_token_traces(log, net, options).front();
      const std::vector<std::size_t> expected = tokens == 1
          ? std::vector<std::size_t>{2, 0}
          : std::vector<std::size_t>{2, 1, 0};
      assert(row.activated_transitions == expected);
      assert(row.reached_marking == (tokens == 1
          ? std::vector<int>{0, 1, 0, 1}
          : std::vector<int>{0, 0, 0, 1}));
      assert(row.missing == (tokens == 1 ? 1U : 0U));
      assert(row.remaining == (tokens == 1 ? 1U : 0U));
      assert(row.trace_is_fit == (tokens == 2));
      consistent(log, net, options);
      assert_hidden_route_usage(net, tokens, exhaustive, expected);
    }
  }

  // The competing routes share p4.  The public non-exhaustive mode keeps the
  // later route after t4, while exhaustive mode trials discard the dead-end
  // t0 path when only two p0 tokens are available.
  const std::vector<std::vector<std::size_t>> expected{
      {2, 1, 3, 0}, {2, 1, 4, 3, 0}, {2, 1, 4, 3, 0},
      {2, 3, 4, 0}, {2, 1, 4, 0}, {2, 1, 4, 0}};
  std::size_t expected_index = 0;
  for (const bool exhaustive : {false, true}) {
    for (const int tokens : {2, 3, 4}) {
      auto net = exhaustive_competing_route_net();
      TokenReplayOptions options;
      options.initial_marking = {tokens, 0, 0, 0, 0, 0};
      options.final_marking = {0, 0, 0, 0, 0, 1};
      options.exhaustive_invisible_exploration = exhaustive;
      const auto row = replay_token_traces(log, net, options).front();
      const auto& wanted = expected[expected_index++];
      assert(row.activated_transitions == wanted);
      assert(row.event_diagnostics.size() == 1);
      assert(row.event_diagnostics.front().activated_transitions == wanted);
      consistent(log, net, options);
      assert_hidden_route_usage(net, tokens, exhaustive, wanted);
    }
  }
}

void forward_deficient_input_keys_public_contract() {
  const auto log = log_of({{"A"}});
  for (const std::size_t weight : {std::size_t{1}, std::size_t{2}}) {
    for (const bool exhaustive : {false, true}) {
      auto net = deficient_input_route_net(weight);
      TokenReplayOptions options;
      options.initial_marking.assign(net.place_count(), 0);
      options.initial_marking[0] = static_cast<int>(2 * weight);
      options.final_marking.assign(net.place_count(), 0);
      options.final_marking.back() = static_cast<int>(weight);
      options.place_names = {"p0", "p1", "p2", "p3", "p4", "p5", "sink"};
      options.transition_names = {"visible", "t0", "t1", "t3", "t5"};
      options.try_to_reach_final_marking_through_hidden = false;
      options.exhaustive_invisible_exploration = exhaustive;
      const auto row = replay_token_traces(log, net, options).front();
      const std::vector<std::size_t> expected = exhaustive
          ? std::vector<std::size_t>{1, 4, 3, 0}
          : std::vector<std::size_t>{1, 4, 2, 0};
      assert(row.activated_transitions == expected);
      assert(row.event_diagnostics.size() == 1);
      assert(row.event_diagnostics.front().activated_transitions == expected);
      consistent(log, net, options);

      PetriUsageComparisonOptions usage_options;
      usage_options.initial_marking.assign(options.initial_marking.begin(), options.initial_marking.end());
      usage_options.final_marking.assign(options.final_marking.begin(), options.final_marking.end());
      usage_options.place_names = options.place_names;
      usage_options.transition_names = options.transition_names;
      usage_options.try_to_reach_final_marking_through_hidden = false;
      usage_options.exhaustive_invisible_exploration = exhaustive;
      const auto comparison = compare_petri_usage(net, log, EventLog{}, usage_options);
      for (const auto& transition : comparison.transitions) {
        const auto count = static_cast<std::uint64_t>(std::count(
            expected.begin(), expected.end(), transition.id));
        assert(transition.log1_occurrences == count);
        assert(transition.log2_occurrences == 0 && transition.total_occurrences == count);
      }
    }
  }
}

void forward_exhaustive_visited_trial_public_contract() {
  const auto log = log_of({{"A"}});
  for (const bool exhaustive : {false, true}) {
    for (const int tokens : {2, 3}) {
      auto net = exhaustive_weighted_deficit_net();
      TokenReplayOptions options;
      options.initial_marking = {tokens, 0, 0, 0, 0, 0, 0, 0};
      options.final_marking = {0, 0, 0, 0, 0, 0, 0, 1};
      options.place_names = {"p0", "p1", "p2", "p3", "p4", "p5", "p6", "sink"};
      options.transition_names = {"visible", "t2", "t3"};
      options.try_to_reach_final_marking_through_hidden = false;
      options.exhaustive_invisible_exploration = exhaustive;
      const auto row = replay_token_traces(log, net, options).front();
      const std::vector<std::size_t> expected{1, 0};
      assert(row.activated_transitions == expected);
      assert(row.event_diagnostics.size() == 1);
      assert(row.event_diagnostics.front().activated_transitions == expected);
      consistent(log, net, options);
      assert_hidden_route_usage(net, tokens, exhaustive, expected);
    }
  }
}

void forward_final_path_source_order_public_contract() {
  const auto empty = log_of({{}});
  for (const bool renamed : {false, true}) {
    const auto net = final_path_order_net();
    TokenReplayOptions options;
    options.initial_marking = {2, 0, 0, 0, 0, 0, 0, 0};
    options.final_marking = {0, 0, 0, 0, 0, 1, 1, 0};
    options.place_names = renamed
        ? std::vector<std::string>{"z0", "p1", "p2", "a3", "p4", "p5", "p6", "sink"}
        : std::vector<std::string>{"p0", "p1", "p2", "p3", "p4", "p5", "p6", "sink"};
    options.transition_names = {"visible", "t0", "t1", "t2"};
    options.try_to_reach_final_marking_through_hidden = true;
    const auto row = replay_token_traces(empty, net, options).front();
    const std::vector<std::size_t> expected = renamed
        ? std::vector<std::size_t>{3, 2}
        : std::vector<std::size_t>{3, 3, 2};
    assert(row.activated_transitions == expected);
    assert(row.event_diagnostics.empty());
    consistent(empty, net, options);

    PetriUsageComparisonOptions usage_options;
    usage_options.initial_marking = {2, 0, 0, 0, 0, 0, 0, 0};
    usage_options.final_marking = {0, 0, 0, 0, 0, 1, 1, 0};
    usage_options.place_names = options.place_names;
    usage_options.transition_names = options.transition_names;
    usage_options.try_to_reach_final_marking_through_hidden = true;
    const auto comparison = compare_petri_usage(net, empty, EventLog{}, usage_options);
    for (const auto& transition : comparison.transitions) {
      const auto count = static_cast<std::uint64_t>(std::count(
          expected.begin(), expected.end(), transition.id));
      assert(transition.log1_occurrences == count);
      assert(transition.log2_occurrences == 0 && transition.total_occurrences == count);
    }
  }
}

void forward_exhaustive_candidate_pool_public_contract() {
  const auto log = log_of({{"A", "A", "A"}});
  const auto net = exhaustive_pool_replay_net();
  TokenReplayOptions options;
  options.initial_marking = {3, 0, 0, 0, 0, 0};
  options.final_marking = {0, 0, 0, 0, 0, 1};
  options.place_names = {"p0", "p2", "p4", "p5", "p6", "sink"};
  options.transition_names = {"visible", "t1", "t2", "t8"};
  options.exhaustive_invisible_exploration = true;
  const auto row = replay_token_traces(log, net, options).front();
  const std::vector<std::size_t> expected{3, 2, 0, 3, 2, 0, 3, 2, 0};
  assert(row.activated_transitions == expected);
  assert(row.event_diagnostics.size() == 3);
  for (const auto& event : row.event_diagnostics)
    assert((event.activated_transitions == std::vector<std::size_t>{3, 2, 0}));
  consistent(log, net, options);

  PetriUsageComparisonOptions usage_options;
  usage_options.initial_marking = {3, 0, 0, 0, 0, 0};
  usage_options.final_marking = {0, 0, 0, 0, 0, 1};
  usage_options.place_names = options.place_names;
  usage_options.transition_names = options.transition_names;
  usage_options.exhaustive_invisible_exploration = true;
  const auto comparison = compare_petri_usage(net, log, EventLog{}, usage_options);
  for (const auto& transition : comparison.transitions) {
    const auto count = static_cast<std::uint64_t>(std::count(
        expected.begin(), expected.end(), transition.id));
    assert(transition.log1_occurrences == count);
    assert(transition.log2_occurrences == 0 && transition.total_occurrences == count);
  }
}

void exhaustive_hidden_trial_resource_guard_contract() {
  constexpr std::size_t branches = 13;
  std::vector<int> marking(branches + 1, 1);
  marking.back() = 0;
  const auto initial_marking = marking;
  std::vector<bool> keys(branches + 1, true);
  keys.back() = false;
  const auto initial_keys = keys;
  std::vector<detail::HiddenPlacePath> paths;
  for (std::size_t transition = 0; transition < branches; ++transition)
    paths.push_back({0, 1, {transition}});
  std::vector<bool> visited(branches + 1, false);
  bool rejected = false;
  try {
    (void)detail::exhaustive_hidden_transition_trial(
        branches, paths, visited, marking, keys,
        [](const std::vector<int>& state, std::size_t transition) {
          return state[transition] > 0;
        },
        [](std::vector<int>& state, std::vector<bool>& state_keys,
           std::size_t transition) {
          --state[transition];
          state_keys[transition] = false;
        });
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  assert(rejected);
  assert(marking == initial_marking && keys == initial_keys);
}

void exhaustive_hidden_trial_memory_guard_contract() {
  constexpr std::size_t places = 100000;
  constexpr std::size_t branches = 13;
  std::vector<int> marking(places, 0);
  for (std::size_t branch = 0; branch < branches; ++branch) marking[branch] = 1;
  const auto initial_marking = marking;
  std::vector<bool> keys(places, true);
  const auto initial_keys = keys;
  std::vector<detail::HiddenPlacePath> paths;
  for (std::size_t transition = 0; transition < branches; ++transition)
    paths.push_back({0, 1, {transition}});
  std::vector<bool> visited(branches + 1, false);
  std::size_t fire_count = 0;
  bool rejected = false;
  std::string message;
  try {
    (void)detail::exhaustive_hidden_transition_trial(
        branches, paths, visited, marking, keys,
        [](const std::vector<int>& state, std::size_t transition) {
          return state[transition] > 0;
        },
        [&](std::vector<int>& state, std::vector<bool>& state_keys,
            std::size_t transition) {
          ++fire_count;
          --state[transition];
          state_keys[transition] = false;
        });
  } catch (const std::runtime_error& error) {
    rejected = true;
    message = error.what();
  }
  assert(rejected);
  assert(message.find("memory limit") != std::string::npos);
  assert(fire_count < detail::kReplayStructuralSearchCap);
  assert(marking == initial_marking && keys == initial_keys);
}

// Public 2.7.22 nested-AND probes: blocked joins remain visited, while later
// steps of their paths can still fire. These are routing, not language, tests.
struct NestedRoutingNet {
  PetriNet net;
  TokenReplayOptions options;
};
NestedRoutingNet nested_routing_net(int depth, bool prefired, bool tail) {
  NestedRoutingNet fixture;
  auto& net = fixture.net;
  auto& options = fixture.options;
  net.set_activities({"A"});
  auto place = [&](const std::string& name) {
    options.place_names.push_back(name);
    return net.add_place();
  };
  auto suffix = [](int i) { return std::string("0") + std::to_string(i); };
  place("entry"); place("block_out_00");
  for (int i = 0; i < depth; ++i)
    for (const auto* prefix : {"direct_branch_", "direct_end_", "nested_branch_", "nested_end_"})
      place(prefix + suffix(i));
  const auto sink = place("sink");
  if (tail) place("tail");
  auto id = [&](const std::string& name) {
    const auto found = std::find(options.place_names.begin(), options.place_names.end(), name);
    assert(found != options.place_names.end());
    return static_cast<std::size_t>(found - options.place_names.begin());
  };
  auto transition = [&](const std::string& name, std::vector<std::size_t> inputs,
                        std::vector<std::size_t> outputs, bool visible = false) {
    options.transition_names.push_back(name);
    net.add_transition(visible ? std::optional<std::size_t>{0} : std::nullopt,
                       std::move(inputs), std::move(outputs));
  };
  for (int i = 0; i < depth; ++i) {
    const auto n = suffix(i);
    transition("split_" + n, {id(i == 0 ? "entry" : "nested_branch_" + suffix(i - 1))},
               {id("direct_branch_" + n), id("nested_branch_" + n)});
    transition("direct_" + n, {id("direct_branch_" + n)}, {id("direct_end_" + n)});
    transition("join_" + n, {id("direct_end_" + n), id("nested_end_" + n)},
               {id(i == 0 ? "block_out_00" : "nested_end_" + suffix(i - 1))});
  }
  const auto last = suffix(depth - 1);
  transition("nested_" + last, {id("nested_branch_" + last)}, {id("nested_end_" + last)});
  transition("visible", {id(tail ? "tail" : "block_out_00")}, {sink}, true);
  if (tail) transition("tail_move", {id("block_out_00")}, {id("tail")});
  net.set_source(0); net.set_sink(sink);
  options.initial_marking.assign(net.place_count(), 0);
  if (prefired) {
    options.initial_marking[id("direct_branch_00")] = 1;
    options.initial_marking[id("nested_branch_00")] = 1;
  } else options.initial_marking[0] = 1;
  options.final_marking.assign(net.place_count(), 0);
  options.final_marking[sink] = 1;
  options.try_to_reach_final_marking_through_hidden = false;
  return fixture;
}

void forward_nested_hidden_routing_public_contract() {
  struct Expected {
    int depth;
    bool prefired, tail, exhaustive, stop;
    std::uint64_t consumed, produced, missing, remaining;
    double fitness;
    bool fit;
    std::vector<std::string> activated;
    std::vector<int> marking;
    std::vector<std::uint64_t> place_usage; // Empty when only replay was probed.
  };
  // Exact public results from token-replay-nested-followup-reference.json,
  // token-replay-nested-routing-reference.json and both comparison captures.
  const std::vector<Expected> cases{
    {2, false, false, false, false, 11, 11, 0, 0, 1.0, true,
     {"split_00", "direct_00", "split_01", "direct_01", "nested_01", "join_01", "join_00", "visible"},
     {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {2, false, false, false, true, 11, 11, 0, 0, 1.0, true,
     {"split_00", "direct_00", "split_01", "direct_01", "nested_01", "join_01", "join_00", "visible"},
     {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {2, false, false, true, true, 5, 7, 2, 3, 0.5857142857142856, false,
     {"split_00", "direct_00", "split_01", "direct_01"},
     {0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0}, {1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1}},
    {3, false, false, false, false, 13, 14, 1, 2, 0.8901098901098902, false,
     {"split_00", "direct_00", "split_01", "direct_01", "split_02", "direct_02", "nested_02", "join_02", "join_01", "visible"},
     {0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1}, {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {3, false, false, false, true, 12, 13, 2, 2, 0.8397435897435898, false,
     {"split_00", "direct_00", "split_01", "direct_01", "split_02", "direct_02", "nested_02", "join_02", "join_01"},
     {0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {3, false, false, true, true, 5, 7, 2, 3, 0.5857142857142856, false,
     {"split_00", "direct_00", "split_01", "direct_01"},
     {0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0}, {1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 1}},
    {4, false, false, false, false, 10, 14, 1, 5, 0.7714285714285714, false,
     {"split_00", "direct_00", "split_01", "direct_01", "split_02", "direct_02", "split_03", "direct_03", "visible"},
     {0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 1}, {1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1}},
    {4, false, false, false, true, 9, 13, 2, 5, 0.6965811965811965, false,
     {"split_00", "direct_00", "split_01", "direct_01", "split_02", "direct_02", "split_03", "direct_03"},
     {0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0}, {1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1}},
    {4, false, false, true, true, 5, 7, 2, 3, 0.5857142857142856, false,
     {"split_00", "direct_00", "split_01", "direct_01"},
     {0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}},
    {3, true, false, false, true, 11, 12, 2, 2, 0.8257575757575757, false,
     {"direct_00", "split_01", "direct_01", "split_02", "direct_02", "nested_02", "join_02", "join_01"},
     {0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {}},
    {3, false, true, false, true, 12, 13, 2, 2, 0.8397435897435898, false,
     {"split_00", "direct_00", "split_01", "direct_01", "split_02", "direct_02", "nested_02", "join_02", "join_01"},
     {0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {}},
    {4, true, false, false, true, 13, 15, 2, 3, 0.823076923076923, false,
     {"direct_00", "split_01", "direct_01", "split_02", "direct_02", "split_03", "direct_03", "nested_03", "join_03", "join_02"},
     {0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {}},
    {4, false, true, false, true, 16, 17, 2, 2, 0.8786764705882353, false,
     {"split_00", "direct_00", "split_01", "direct_01", "split_02", "direct_02", "split_03", "direct_03", "nested_03", "join_03", "join_02", "join_01"},
     {0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {}},
  };
  const auto log = log_of({{"A"}});
  for (const auto& expected : cases) {
    auto fixture = nested_routing_net(expected.depth, expected.prefired, expected.tail);
    auto& options = fixture.options;
    options.exhaustive_invisible_exploration = expected.exhaustive;
    options.stop_immediately_unfit = expected.stop;
    const auto row = replay_token_traces(log, fixture.net, options).front();
    std::vector<std::string> activated;
    for (const auto t : row.activated_transitions) activated.push_back(options.transition_names[t]);
    assert(activated == expected.activated);
    assert(row.reached_marking == expected.marking);
    assert(row.reached_marking_keys.size() == expected.marking.size());
    for (std::size_t i = 0; i < expected.marking.size(); ++i)
      assert(row.reached_marking_keys[i] == (expected.marking[i] > 0));
    assert(row.consumed == expected.consumed && row.produced == expected.produced);
    assert(row.missing == expected.missing && row.remaining == expected.remaining);
    assert(std::abs(row.trace_fitness - expected.fitness) < 1e-12);
    assert(row.trace_is_fit == expected.fit);
    assert(row.replay_stopped == (expected.stop && !expected.fit));
    assert(row.event_diagnostics.size() == 1);
    assert(row.transitions_with_problems.size() == (expected.fit ? 0U : 1U));
    consistent(log, fixture.net, options);
    if (!expected.place_usage.empty()) {
      PetriUsageComparisonOptions comparison_options;
      comparison_options.initial_marking.assign(options.initial_marking.begin(), options.initial_marking.end());
      comparison_options.final_marking.assign(options.final_marking.begin(), options.final_marking.end());
      comparison_options.place_names = options.place_names;
      comparison_options.transition_names = options.transition_names;
      comparison_options.try_to_reach_final_marking_through_hidden = false;
      comparison_options.stop_immediately_unfit = expected.stop;
      comparison_options.exhaustive_invisible_exploration = expected.exhaustive;
      const auto comparison = compare_petri_usage(fixture.net, log, EventLog{}, comparison_options);
      assert(comparison.places.size() == expected.place_usage.size());
      for (const auto& place : comparison.places) {
        assert(place.log1_occurrences == expected.place_usage[place.id]);
        assert(place.log2_occurrences == 0 && place.total_occurrences == place.log1_occurrences);
      }
      for (const auto& transition : comparison.transitions) {
        const auto count = static_cast<std::uint64_t>(std::count(
            expected.activated.begin(), expected.activated.end(), options.transition_names[transition.id]));
        assert(transition.log1_occurrences == count);
        assert(transition.log2_occurrences == 0 && transition.total_occurrences == count);
      }
    }
  }
}
}  // namespace

namespace {
void supplied_pnml_model_defaults_and_keys() {
  const std::string skeleton = R"(<pnml><net id="n"><place id="start"/>
    <place id="middle"/><place id="finish"/>
    <transition id="visible-a"><name><text>a</text></name></transition>
    <transition id="visible-b"><name><text>b</text></name></transition>
    <arc source="start" target="visible-a"/><arc source="visible-a" target="middle"/>
    <arc source="middle" target="visible-b"/><arc source="visible-b" target="finish"/>
    </net></pnml>)";
  const auto inferred = tools::parse_pnml_model(skeleton);
  assert((inferred.initial_marking == std::vector<std::int64_t>{1, 0, 0}));
  assert((inferred.final_marking == std::vector<std::int64_t>{0, 0, 1}));
  assert((inferred.initial_marking_keys == std::vector<bool>{true, false, false}));
  assert((inferred.final_marking_keys == std::vector<bool>{false, false, true}));
  auto zero_xml = skeleton;
  zero_xml.replace(zero_xml.find("<place id=\"start\"/>"),
                   std::string("<place id=\"start\"/>").size(),
                   "<place id=\"start\"><initialMarking><text>0</text>"
                   "</initialMarking></place>");
  zero_xml.insert(zero_xml.find("</net>"),
      "<finalmarkings><marking><place idref=\"finish\"><text>0</text>"
      "</place></marking></finalmarkings>");
  const auto zero = tools::parse_pnml_model(zero_xml);
  assert((zero.initial_marking == std::vector<std::int64_t>{0, 0, 0}));
  assert(zero.initial_marking_keys == inferred.initial_marking_keys);
  assert((zero.final_marking == std::vector<std::int64_t>{0, 0, 0}));
  assert(zero.final_marking_keys == inferred.final_marking_keys);
  const auto again = tools::parse_pnml_model(to_pnml(zero));
  assert(again.initial_marking == zero.initial_marking);
  assert(again.final_marking == zero.final_marking);

  TokenReplayOptions replay_base;
  replay_base.cleaning_token_flood = true;
  replay_base.exhaustive_invisible_exploration = true;
  replay_base.activity_attribute = "selected";
  const auto replay = tools::pnml_replay_options(zero, replay_base);
  assert(replay.cleaning_token_flood && replay.exhaustive_invisible_exploration);
  assert(replay.activity_attribute == "selected");
  assert(replay.place_names == zero.place_ids);
  assert(replay.transition_names == zero.transition_ids);
  assert(replay.initial_marking_keys == zero.initial_marking_keys);
  assert(replay.final_marking_keys == zero.final_marking_keys);
  AlignmentOptions alignment_base;
  alignment_base.activity_attribute = "selected";
  const auto alignment = tools::pnml_alignment_options(zero, alignment_base);
  assert(alignment.initial_marking == replay.initial_marking);
  assert(alignment.final_marking == replay.final_marking);
  assert(alignment.activity_attribute == "selected");
  PetriUsageComparisonOptions comparison_base;
  comparison_base.stop_immediately_unfit = true;
  const auto comparison = tools::pnml_comparison_options(zero, comparison_base);
  assert(comparison.stop_immediately_unfit);
  assert(comparison.place_names == zero.place_ids);
  assert(comparison.transition_names == zero.transition_ids);
  assert(comparison.initial_marking_keys == zero.initial_marking_keys);
  assert(comparison.final_marking_keys == zero.final_marking_keys);

  const auto ambiguous = tools::parse_pnml_model(
      "<pnml><net id=\"n\"><place id=\"x\"/><place id=\"y\"/></net></pnml>");
  assert((ambiguous.initial_marking == std::vector<std::int64_t>{0, 0}));
  assert((ambiguous.final_marking == std::vector<std::int64_t>{0, 0}));
  assert((ambiguous.initial_marking_keys == std::vector<bool>{false, false}));
  const auto empty = tools::parse_pnml_model("<pnml><net id=\"n\"/></pnml>");
  assert(empty.initial_marking.empty() && empty.final_marking.empty());

  const auto rejects = [](const auto& query) {
    bool rejected = false;
    try { query(); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
  };
  assert(tools::pnml_marking<int>({2147483647}).front() == 2147483647);
  rejects([] { (void)tools::pnml_marking<int>({2147483648}); });
  rejects([] { (void)tools::pnml_marking<std::uint32_t>({-1}); });
  rejects([] { (void)tools::pnml_marking<std::uint32_t>({4294967296}); });
  rejects([] { (void)tools::parse_pnml_model(
      "<pnml><net id=\"n\"><place id=\"x\"><initialMarking>"
      "<text>2147483648</text></initialMarking></place></net></pnml>"); });
}

void explicit_marking_conformance_queries() {
  PetriNet weighted;
  weighted.set_activities({"a", "b"});
  const auto source = weighted.add_place();
  const auto middle = weighted.add_place();
  const auto sink = weighted.add_place();
  weighted.set_source(source);
  weighted.set_sink(sink);
  weighted.add_transition(0, {source, source}, {middle});
  weighted.add_transition(1, {middle}, {sink});
  const auto log = log_of({{"a", "b"}});
  const auto defaults = check_footprints(log, weighted);
  const auto explicit_default = check_footprints(log, weighted, {1, 0, 0});
  assert(defaults.model_relations == 0 && defaults.fitness == 0.0);
  assert(explicit_default.model_relations == defaults.model_relations);
  assert(explicit_default.fitness == defaults.fitness);
  const auto supplied = check_footprints(log, weighted, {2, 0, 0});
  assert(supplied.model_relations == 1 && supplied.shared_relations == 1);
  assert(supplied.fitness == 1.0 && supplied.precision == 1.0);
  assert(supplied.log_only.empty() && supplied.model_only.empty());
  assert(!supplied.exhausted);
  assert(!final_marking_reachable(weighted));
  assert(!final_marking_reachable(weighted, {1, 0, 0}, {0, 0, 1}));
  assert(final_marking_reachable(weighted, {2, 0, 0}, {0, 0, 1}));
  assert(!final_marking_reachable(weighted, {2, 0, 0}, {0, 0, 2}));

  const auto sequence = sequential();
  assert(final_marking_reachable(sequence));
  assert(final_marking_reachable(sequence, {}, {}));
  assert(final_marking_reachable(sequence, {1, 0, 0}, {0, 0, 1}));
  assert(!final_marking_reachable(sequence, {0, 0, 0}, {0, 0, 1}));
  assert(final_marking_reachable(sequence, {0, 0, 0}, {0, 0, 0}));
  assert(check_footprints(log, sequence).shared_relations == 1);
  assert(check_footprints(log, sequence, {}).shared_relations == 1);
  assert(check_footprints(log, sequence, {0, 0, 0}).shared_relations == 0);

  // An already marked sink must survive the run; the target is an exact
  // marking, rather than a test for the presence of any sink token.
  assert(final_marking_reachable(sequence, {1, 0, 1}, {0, 1, 1}));
  assert(final_marking_reachable(sequence, {1, 0, 1}, {0, 0, 2}));
  assert(!final_marking_reachable(sequence, {1, 0, 1}, {0, 0, 1}));
  assert(check_footprints(log, sequence, {1, 0, 1}).shared_relations == 1);

  const PetriNet empty;
  const auto empty_footprint = check_footprints(EventLog{}, empty);
  assert(empty_footprint.model_relations == 0 && !empty_footprint.exhausted);
  assert(empty_footprint.fitness == 1.0 && empty_footprint.precision == 1.0);
  assert(check_footprints(log, empty, {}).model_relations == 0);
  assert(final_marking_reachable(empty));
  assert(final_marking_reachable(empty, {}, {}));

  const auto rejects = [](const auto& query) {
    bool rejected = false;
    try { query(); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
  };
  for (const auto& invalid : std::vector<std::vector<int>>{
           {1}, {1, 0, 0, 0}, {0, -1, 0}}) {
    rejects([&] { (void)check_footprints(log, sequence, invalid); });
    rejects([&] { (void)final_marking_reachable(sequence, invalid, {0, 0, 1}); });
    rejects([&] { (void)final_marking_reachable(sequence, {1, 0, 0}, invalid); });
  }
  rejects([&] { (void)check_footprints(log, empty, {0}); });
  rejects([&] { (void)final_marking_reachable(empty, {0}, {}); });
  rejects([&] { (void)final_marking_reachable(empty, {}, {0}); });
}

void supplied_marking_firing_overflow() {
  PetriNet net;
  net.set_activities({"a"});
  net.add_place();
  net.add_place();
  net.add_transition(0, {1}, {0});
  const std::vector<int> initial{std::numeric_limits<int>::max(), 1};
  const auto rejects_overflow = [](const auto& query) {
    bool rejected = false;
    try { query(); } catch (const std::overflow_error&) { rejected = true; }
    assert(rejected);
  };
  rejects_overflow([&] { (void)final_marking_reachable(net, initial, {0, 0}); });
  rejects_overflow([&] { (void)check_footprints(log_of({{"a"}}), net, initial); });
  TokenReplayOptions replay;
  replay.initial_marking = initial;
  replay.final_marking = {0, 0};
  const auto exact_replay = replay_token_traces(log_of({{"a"}}), net, replay);
  assert(exact_replay.size() == 1);
  const TokenInteger wide = TokenInteger(std::numeric_limits<int>::max()) + 1;
  assert(exact_replay[0].reached_marking == TokenMarking({wide, 0}));
  assert(exact_replay[0].produced == wide + 1 && exact_replay[0].consumed == 1);
  assert(exact_replay[0].missing == 0 && exact_replay[0].remaining == wide);
  AlignmentOptions alignment;
  alignment.initial_marking = initial;
  alignment.final_marking = {0, 0};
  rejects_overflow([&] { (void)check_alignment(log_of({{"a"}}), net, alignment); });
  std::vector<std::uint32_t> unsigned_marking{
      std::numeric_limits<std::uint32_t>::max(), 1};
  rejects_overflow([&] { fire_petri_transition(net.transitions()[0], unsigned_marking); });

  // Consume/reset before producing, so firings at the limit remain valid
  // when their actual postmarking is representable.
  PetriTransition self_loop{0, {0}, {0}, {}, {}};
  auto at_limit = initial;
  fire_petri_transition(self_loop, at_limit);
  assert(at_limit == initial);
  PetriTransition reset{0, {}, {0}, {0}, {}};
  fire_petri_transition(reset, at_limit);
  assert((at_limit == std::vector<int>{1, 1}));
}
}  // namespace

int main() {
  supplied_pnml_model_defaults_and_keys();
  supplied_marking_firing_overflow();
  explicit_marking_conformance_queries();
  forward_exhaustive_hidden_path_trials_public_contract();
  forward_deficient_input_keys_public_contract();
  forward_exhaustive_visited_trial_public_contract();
  forward_final_path_source_order_public_contract();
  forward_exhaustive_candidate_pool_public_contract();
  exhaustive_hidden_trial_resource_guard_contract();
  exhaustive_hidden_trial_memory_guard_contract();
  forward_nested_hidden_routing_public_contract();
  enabled_transition_worklist_public_contract();
  forward_cleaning_public_counter_contract();
  forward_weighted_repair_and_partial_stop_contract();
  forward_final_zero_key_and_path_boundary_contract();
  forward_empty_log_cleaning_initialization_contract();
  forward_extended_arc_silent_completion_contract();
  forward_bpmn_greedy_routing_public_contract();
  ordered_trace_rows_and_markings();
  hidden_routing_policies_and_enabled_ids();
  deviations_unknowns_and_stopping();
  backward_contract_and_duplicate_labels();
  explicit_markings_and_remaining_classification();
  precision_uses_selected_hidden_routing_policy();
  unknown_positions_and_custom_activity_selector();
  final_shortages_do_not_become_event_shortages();
  input_event_caps_apply_before_replay();
  unknown_policy_is_classification_only_even_when_stopping();
  final_demand_and_zero_denominator_trace_fitness();
  backwards_zero_denominator_fitness_is_undefined();
  activation_sequence_replay_skips_fitness_only();
  null_activity_is_unknown_not_an_empty_label();
  typed_activity_values_do_not_alias_string_labels();
  backwards_module_acceptance();
  replay_variant_keys_are_unambiguous();
  replay_selector_priority_and_missing_state();
  event_records_preserve_routing_and_source_positions();
  event_records_preserve_repeated_problems_and_stop_boundaries();
  backward_event_records_group_duplicate_firings_and_ambiguity();
  std::cout << "Token replay diagnostics tests passed\n";
}
