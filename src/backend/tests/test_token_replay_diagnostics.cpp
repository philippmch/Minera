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
  consistent(log, net, options);
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
    }
    options.max_rows_to_materialize = 0;
    consistent(log_of({{}, {}}), net, options);
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
        const auto rows = replay_token_traces(log, net, options);
        for (const auto index : {0, 2}) {
          const auto& row = rows[index];
          assert(row.trace_is_fit == (!penalize || variant == TokenReplayVariant::kBackwards));
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
  for (const auto variant : {TokenReplayVariant::kForward, TokenReplayVariant::kBackwards}) {
    TokenReplayOptions options;
    options.variant = variant;
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
  }
  // An exact nullable selector wins over a non-null canonical projection.
  auto custom = log_of({{"not-selected"}});
  EventAttribute selected;
  selected.key = "chosen";
  selected.is_null = true;
  custom.traces[0].events[0].attributes.push_back(selected);
  TokenReplayOptions options;
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
}  // namespace

int main() {
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
  null_activity_is_unknown_not_an_empty_label();
  replay_variant_keys_are_unambiguous();
  replay_selector_priority_and_missing_state();
  std::cout << "Token replay diagnostics tests passed\n";
}
