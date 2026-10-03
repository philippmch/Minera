#include "minera/conformance.hpp"
#include "minera/json_export.hpp"
#include "minera/request_params.hpp"

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace minera;
namespace {
Event event(std::string activity, std::string payload = {}) {
  Event result;
  result.activity = std::move(activity);
  if (!payload.empty()) result.attributes.push_back({"payload", std::move(payload)});
  return result;
}
Trace trace(std::string name, std::vector<Event> events) {
  Trace result;
  result.case_id = std::move(name);
  result.events = std::move(events);
  return result;
}
PetriNet model() {
  PetriNet net;
  net.set_activities({"A", "hidden"});
  const auto source = net.add_place(), gate = net.add_place(), sink = net.add_place();
  const auto unused = net.add_place();
  net.set_source(source); net.set_sink(sink);
  net.add_transition(0, {source, gate, gate}, {sink});
  net.add_transition(1, {unused}, {sink});
  net.add_transition(std::nullopt, {unused}, {unused});
  return net;
}
TokenReplayOptions options() {
  TokenReplayOptions result;
  result.initial_marking = {1, 0, 0, 0};
  result.final_marking = {0, 0, 1, 0};
  result.enable_pltr_fitness = true;
  return result;
}
void variants_and_keys() {
  EventLog log;
  log.traces = {trace("first", {event("A", "same")}),
                trace("second", {event("A", "same")}),
                trace("first", {event("A", "same")})};
  const auto net = model();
  auto policy = options();
  const auto legacy = token_replay_traces_json(replay_token_traces(log, net, policy));
  const auto full = replay_token_result(log, net, policy);
  const auto& fitness = *full.pltr_fitness;
  assert(full.traces.size() == 3 && fitness.trace_references.size() == 2);
  assert(fitness.trace_references[0].source_trace_index == 0);
  assert(fitness.trace_references[1].source_trace_index == 1);
  assert(fitness.places[1].missing == 6 && fitness.places[1].consumed == 6);
  assert(fitness.places[1].underfed_traces.size() == 2);
  assert(fitness.transitions.size() == 2 && fitness.transitions[1].fit_traces.empty());
  assert(fitness.transitions[0].underfed_traces.size() == 2);
  assert(fitness.transitions[0].underfed_traces[0].contexts.size() == 2);
  policy.disable_variants = false;
  policy.return_names = true;
  const auto grouped = replay_token_result(log, net, policy);
  const auto& grouped_fitness = *grouped.pltr_fitness;
  assert(grouped.traces.size() == 3 && grouped_fitness.trace_references.size() == 1);
  assert(!grouped_fitness.trace_references[0].source_trace_index);
  assert(grouped_fitness.trace_references[0].value.attributes.empty());
  assert(grouped_fitness.places[1].missing == 2 && grouped_fitness.places[1].consumed == 6);
  assert(grouped_fitness.transitions[0].underfed_traces[0].contexts.size() == 1);
  assert(grouped_fitness.transitions[0].underfed_traces[0].contexts[0].attributes.size() == 1);
  assert(token_replay_traces_json(replay_token_traces(log, net, policy)) == legacy);
  assert(replay_token_diagnostics(log, net, policy).places[1].missing == 6);
  const auto json = token_replay_result_json(grouped);
  assert(json.find("\"type\":\"tuple\"") != std::string::npos);
  assert(json.find("activated_transitions_labels") != std::string::npos);
  assert(json.find("\"type\":\"Transition\"") != std::string::npos);
  assert(json.find("\"type\":\"float\",\"hex\":") != std::string::npos);
  log.traces = {trace("one", {}), trace("two", {}), trace("three", {})};
  const auto empty = replay_token_result(log, net, policy);
  assert(empty.pltr_fitness->places[2].missing == 3);
  assert(empty.pltr_fitness->places[2].underfed_traces.empty());
}
void contexts_and_stop() {
  auto net = model();
  auto policy = options();
  policy.initial_marking = {2, 2, 0, 0};
  EventLog log;
  log.traces = {trace("known", {event("A", "first"), event("unknown", "ignored"),
                               event("unknown", "ignored again"), event("A", "last")})};
  auto result = replay_token_result(log, net, policy);
  const auto& fitness = *result.pltr_fitness;
  const auto& transition = fitness.transitions[0];
  assert(transition.fit_traces.size() == 1 && transition.underfed_traces.size() == 1);
  assert(*transition.fit_traces[0].contexts[0].attribute("payload") == "last");
  assert(fitness.unknown_activities.size() == 1);
  assert(fitness.unknown_activities[0].traces[0].contexts.size() == 1);
  assert(*fitness.unknown_activities[0].traces[0].contexts[0].attribute("payload") == "last");
  policy.stop_immediately_unfit = true;
  result = replay_token_result(log, net, policy);
  assert(result.traces[0].replay_stopped && result.traces[0].replayed_events == 4);
  assert(result.pltr_fitness->transitions[0].underfed_traces.empty());
  assert(*result.pltr_fitness->transitions[0].fit_traces[0].contexts[0].attribute("payload") == "last");
  policy.enable_pltr_fitness = false;
  assert(!replay_token_result(log, net, policy).pltr_fitness);
}
void rejection_and_transport() {
  auto policy = options();
  Event null;
  null.activity_state = FirstClassAttributeState::kNull;
  EventLog log;
  log.traces = {trace("known", {event("A")}), trace("null", {null})};
  bool rejected = false;
  try { (void)replay_token_result(log, model(), policy); }
  catch (const std::invalid_argument& error) {
    rejected = std::string(error.what()) == "'<' not supported between instances of 'str' and 'NoneType'";
  }
  assert(rejected && replay_token_traces(log, model(), policy).size() == 2);
  policy.variant = TokenReplayVariant::kBackwards;
  rejected = false;
  try { (void)replay_token_result(EventLog{}, model(), policy); }
  catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected);
  const auto parsed = parse_token_replay_params([](std::string_view name) {
    return name == "token_replay_disable_variants" ? std::string("false")
        : name == "token_replay_enable_pltr_fitness" || name == "token_replay_return_names"
          ? std::string("true") : std::string{};
  });
  assert(parsed.enable_pltr_fitness && parsed.return_names && parsed.disable_variants == false);
}

void typed_variant_and_null_contracts() {
  const auto net = model();
  auto policy = options();
  policy.disable_variants = false;
  Event integer, real, boolean;
  integer.attributes.push_back({"concept:name", "1", AttributeKind::kInt});
  EventAttribute floating{"concept:name", "1.0", AttributeKind::kFloat};
  floating.typed_value = 1.0; real.attributes.push_back(floating);
  EventAttribute truth{"concept:name", "true", AttributeKind::kBoolean};
  truth.typed_value = true; boolean.attributes.push_back(truth);
  EventLog log;
  log.traces = {trace("integer", {integer}), trace("real", {real}), trace("boolean", {boolean})};
  const auto grouped = replay_token_result(log, net, policy);
  assert(grouped.pltr_fitness->trace_references.size() == 1);
  assert(grouped.pltr_fitness->unknown_activities.size() == 1);
  assert(grouped.pltr_fitness->unknown_activities[0].activity.kind == AttributeKind::kInt);
  assert(grouped.pltr_fitness->places[2].missing == 3);
  Event null;
  null.activity_state = FirstClassAttributeState::kNull;
  policy.enable_pltr_fitness = false;
  log.traces = {trace("known", {event("A")}), trace("null", {null}), trace("known again", {event("A")})};
  assert(replay_token_result(log, net, policy).traces.size() == 3);
  Event mutable_activity;
  EventAttribute list{"concept:name", ""};
  list.container = AttributeContainerKind::kList;
  mutable_activity.attributes.push_back(list);
  log.traces = {trace("mutable", {mutable_activity})};
  bool rejected = false;
  try { (void)replay_token_result(log, net, policy); }
  catch (const std::invalid_argument& error) { rejected = std::string(error.what()) == "unhashable activity variant key"; }
  assert(rejected);
  PetriNet silent;
  const auto source = silent.add_place(), sink = silent.add_place();
  silent.set_source(source); silent.set_sink(sink);
  silent.add_transition(std::nullopt, {source}, {sink});
  policy = {}; log.traces = {trace("null", {null})};
  const auto replayed = replay_token_result(log, silent, policy);
  assert(replayed.traces[0].activated_transitions == std::vector<std::size_t>{0});
  assert(replayed.traces[0].unknown_event_indices.empty() && replayed.traces[0].trace_is_fit);
  assert(replay_token_traces(log, silent, policy)[0].unknown_event_indices == std::vector<std::size_t>{0});
  policy.enable_pltr_fitness = true;
  rejected = false;
  try { (void)replay_token_result(log, silent, policy); }
  catch (const std::invalid_argument& error) { rejected = std::string(error.what()) == "null transition PLTR key"; }
  assert(rejected);
  silent.set_ordinary_arc_weights(0, {}, {{TokenInteger("1" + std::string(400, '0')), false}});
  rejected = false;
  try { (void)replay_token_result(log, silent, policy); }
  catch (const std::invalid_argument& error) { rejected = std::string(error.what()) == "null transition PLTR key"; }
  assert(rejected);
  policy.enable_pltr_fitness = false;
  rejected = false;
  try { (void)replay_token_result(log, silent, policy); }
  catch (const std::overflow_error&) { rejected = true; }
  assert(rejected);
}

void supplied_hidden_paths() {
  PetriNet net;
  net.set_activities({"A"});
  const auto source = net.add_place(), buffer = net.add_place(), sink = net.add_place();
  net.set_source(source); net.set_sink(sink);
  net.add_transition(std::nullopt, {source}, {buffer});
  net.add_transition(0, {buffer}, {sink});
  EventLog log;
  log.traces = {trace("known", {event("A")})};
  TokenReplayOptions policy;
  policy.enable_pltr_fitness = true;
  const auto automatic = replay_token_result(log, net, policy);
  assert(automatic.traces[0].activated_transitions == (std::vector<std::size_t>{0, 1}));
  assert(automatic.traces[0].trace_is_fit);
  policy.places_shortest_path_by_hidden = TokenReplayHiddenPathMatrix{};
  const auto empty = replay_token_result(log, net, policy);
  assert(empty.traces[0].activated_transitions == std::vector<std::size_t>{1});
  assert(empty.traces[0].missing == 1 && empty.traces[0].remaining == 1);
  assert(empty.pltr_fitness->places[buffer].missing == 1);
  assert(empty.pltr_fitness->places[source].overfed_traces.size() == 1);
  policy.cleaning_token_flood = true;
  const auto cleaned = replay_token_result(log, net, policy);
  assert(cleaned.traces[0].cleaned_place_indices == std::vector<std::size_t>{source});
  assert(!cleaned.traces[0].reached_marking_keys[source]);
  assert(cleaned.pltr_fitness->places[source].overfed_traces.size() == 1);
  policy.cleaning_token_flood = false;
  TokenReplayHiddenPathMatrix supplied;
  supplied[source][buffer] = {0};
  policy.places_shortest_path_by_hidden = supplied;
  assert(token_replay_result_json(replay_token_result(log, net, policy)) == token_replay_result_json(automatic));
  EventAttribute list{"", ""};
  list.container = AttributeContainerKind::kList;
  policy.places_shortest_path_by_hidden = list;
  assert(token_replay_result_json(replay_token_result(log, net, policy)) == token_replay_result_json(empty));

  PetriNet final_net;
  const auto start = final_net.add_place(), finish = final_net.add_place();
  final_net.set_source(start); final_net.set_sink(finish);
  final_net.add_transition(std::nullopt, {start}, {finish});
  log.traces = {trace("empty", {})};
  policy = {};
  assert(replay_token_result(log, final_net, policy).traces[0].trace_is_fit);
  policy.places_shortest_path_by_hidden = TokenReplayHiddenPathMatrix{};
  const auto final_empty = replay_token_result(log, final_net, policy);
  assert(final_empty.traces[0].activated_transitions.empty());
  assert(final_empty.traces[0].missing == 1 && final_empty.traces[0].remaining == 1);
  policy.places_shortest_path_by_hidden = EventAttribute{"", "7", AttributeKind::kInt};
  assert(replay_token_result(EventLog{}, final_net, policy).traces.empty());
  bool rejected = false;
  try { (void)replay_token_result(log, final_net, policy); }
  catch (const std::invalid_argument& error) { rejected = std::string(error.what()) == "hidden path cache is not iterable"; }
  assert(rejected);
  Event null;
  null.activity_state = FirstClassAttributeState::kNull;
  log.traces = {trace("null", {null})};
  policy.enable_pltr_fitness = true;
  rejected = false;
  try { (void)replay_token_result(log, final_net, policy); }
  catch (const std::invalid_argument& error) { rejected = std::string(error.what()) == "null transition PLTR key"; }
  assert(rejected);
}

void cleaned_underfed_place_precedence() {
  PetriNet net;
  net.set_activities({"A"});
  const auto source = net.add_place(), gate = net.add_place(), sink = net.add_place();
  net.set_source(source); net.set_sink(sink);
  net.add_transition(std::nullopt, {source}, {gate});
  net.add_transition(0, {gate, gate}, {sink});
  EventLog log;
  log.traces = {trace("known", {event("A")})};
  TokenReplayOptions policy;
  policy.enable_pltr_fitness = true;
  policy.cleaning_token_flood = true;
  const auto result = replay_token_result(log, net, policy);
  const auto& places = result.pltr_fitness->places;
  assert(result.traces[0].cleaned_place_indices == std::vector<std::size_t>{gate});
  assert(places[gate].missing == 1 && places[gate].remaining == 1);
  assert(places[gate].underfed_traces.size() == 1);
  assert(places[gate].overfed_traces.empty());
}
}  // namespace
int main() {
  variants_and_keys();
  contexts_and_stop();
  rejection_and_transport();
  typed_variant_and_null_contracts();
  supplied_hidden_paths();
  cleaned_underfed_place_precedence();
  std::cout << "full forward token replay result contracts passed\n";
}
