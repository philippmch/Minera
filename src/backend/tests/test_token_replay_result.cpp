#include "minera/conformance.hpp"
#include "minera/alpha_miner.hpp"
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

PetriNet detached_model(TokenInteger initial = 1, TokenInteger final = 1,
                        bool shared = false) {
  PetriNet net;
  net.set_activities({"A"});
  const auto live = net.add_place(), end = net.add_place();
  // These flags deliberately disagree with discovery's initial place.
  net.set_source(end); net.set_sink(end);
  net.add_transition(0, {live}, {end});
  PetriDiscoveryMarkings markings;
  markings.detached_places.push_back({0, HeuristicStructuredValue{std::string("same")}});
  markings.initial.push_back({{PetriMarkingPlaceKind::kDetached, 0}, initial});
  markings.final.push_back({{shared ? PetriMarkingPlaceKind::kDetached : PetriMarkingPlaceKind::kRoot,
                             shared ? 0u : end}, final});
  net.set_discovery_markings(std::move(markings));
  return net;
}

void discovery_marking_replay() {
  EventLog log;
  log.traces = {trace("empty", {})};
  const auto discovered = discover_alpha_plus_result(log);
  const auto& net = discovered.net;
  assert(net.place_count() == 1 && !net.has_source());
  auto result = replay_token_result(log, net);
  const auto& row = result.traces[0];
  assert(row.produced == 1 && row.consumed == 1 && row.missing == 1 && row.remaining == 1);
  assert(!row.trace_is_fit && row.trace_fitness == 0.0);
  assert(row.marking_domain && row.marking_domain->places.size() == 2);
  assert(row.marking_domain->places[0] == (PetriMarkingPlace{PetriMarkingPlaceKind::kRoot, 0}));
  assert(row.marking_domain->places[1].kind == PetriMarkingPlaceKind::kDetached);
  assert(row.reached_marking_keys == (std::vector<bool>{false, true}));
  assert(row.reached_marking == (TokenMarking{0, 1}));
  assert(row.reached_marking_order == (std::vector<std::size_t>{1}));
  const auto raw = token_replay_traces_json(result.traces);
  assert(raw.find("\"marking_domain\"") != std::string::npos && raw.find("\"kind\":\"detached\"") != std::string::npos);
  const auto full = token_replay_result_json(result);
  assert(full.find("\"kind\":\"detached\"") != std::string::npos && full.find("artificial_start") != std::string::npos);
  TokenReplayOptions policy;
  policy.cleaning_token_flood = true;
  assert(replay_token_result(log, net, policy).traces[0].remaining == 1);
  policy.enable_pltr_fitness = true;
  bool rejected = false;
  try { (void)replay_token_result(log, net, policy); }
  catch (const TokenReplayPlaceKeyError& error) {
    rejected = error.place_key() == row.marking_domain->places[1];
    assert(std::holds_alternative<HeuristicStructuredValue::Object>(error.place_name().value));
  }
  assert(rejected && net.place_count() == 1 && !net.has_source());
}

void detached_counts_keys_and_overrides() {
  EventLog log;
  log.traces = {trace("empty", {})};
  auto net = detached_model();
  TokenReplayOptions policy;
  policy.place_names = {"same", "end"};
  const auto result = replay_token_result(log, net, policy);
  assert(result.place_names.size() == 2 && result.marking_domain->places.size() == 3);
  const auto& row = result.traces[0];
  assert(row.missing == 1 && row.remaining == 1 && row.produced == 1 && row.consumed == 1);
  assert(row.marking_domain->places[0] == (PetriMarkingPlace{PetriMarkingPlaceKind::kRoot, 0}));
  assert(row.marking_domain->places[2] == (PetriMarkingPlace{PetriMarkingPlaceKind::kDetached, 0}));
  assert(row.reached_marking == (TokenMarking{0, 0, 1}));
  const auto aggregate = replay_token_diagnostics(log, net, policy);
  assert(aggregate.places.size() == 3 && aggregate.marking_domain);
  assert(aggregate.places[2].produced == 1 && aggregate.places[2].remaining == 1);
  policy.cleaning_token_flood = true;
  log.traces = {trace("forced", {event("A")})};
  const auto forced = replay_token_result(log, net, policy);
  assert(forced.traces[0].missing == 1 && forced.traces[0].remaining == 1);
  assert(forced.traces[0].cleaned_place_indices.empty());
  policy.initial_marking = {1, 0}; policy.final_marking = {0, 1};
  const auto explicit_root = replay_token_result(log, net, policy);
  assert(explicit_root.traces[0].trace_is_fit && !explicit_root.marking_domain);
  assert(explicit_root.traces[0].reached_marking.size() == net.place_count());
  policy.exact_initial_marking = TokenMarking{}; policy.exact_final_marking = TokenMarking{};
  assert(replay_token_result(log, net, policy).marking_domain);

  log.traces = {trace("zero", {})};
  net = detached_model(0, 0, true); policy = {};
  const auto zero = replay_token_result(log, net, policy);
  assert(zero.traces[0].trace_is_fit && zero.traces[0].reached_marking_keys[2]);
  assert(zero.traces[0].reached_marking[2] == 0 && zero.traces[0].reached_marking_order == (std::vector<std::size_t>{2}));
  policy.enable_pltr_fitness = true;
  bool rejected = false;
  try { (void)replay_token_result(log, net, policy); }
  catch (const TokenReplayPlaceKeyError& error) { rejected = error.place_key().kind == PetriMarkingPlaceKind::kDetached && error.place_key().id == 0; }
  assert(rejected);
  auto invalid = detached_model();
  invalid.add_transition(0, {invalid.place_count()}, {0});
  rejected = false;
  try { (void)replay_token_result(log, invalid); }
  catch (const std::invalid_argument& error) {
    rejected = std::string(error.what()) == "token replay arc place is outside root topology";
  }
  assert(rejected);
  policy.initial_marking_keys = {false, false, false}; policy.final_marking_keys = {false, false, false};
  assert(replay_token_result(log, net, policy).pltr_fitness);

  const TokenInteger wide("1000000000000000000000000000000000000007");
  net = detached_model(wide, wide, true); policy = {};
  const auto shared = replay_token_result(log, net, policy);
  assert(shared.traces[0].trace_is_fit && shared.traces[0].produced == wide && shared.traces[0].consumed == wide);
  assert(shared.traces[0].missing == 0 && shared.traces[0].remaining == 0);
  policy.variant = TokenReplayVariant::kBackwards;
  const auto backwards = replay_token_traces(log, net, policy);
  assert(backwards[0].trace_is_fit && backwards[0].produced == wide && backwards[0].consumed == wide);
  assert(backwards[0].marking_domain->places[2].kind == PetriMarkingPlaceKind::kDetached);
  PetriNet rootless;
  rootless.set_discovery_markings({{{0, HeuristicStructuredValue{std::string("same")}}},
      {{{PetriMarkingPlaceKind::kDetached, 0}, 1}}, {{{PetriMarkingPlaceKind::kDetached, 0}, 1}}});
  assert(replay_token_traces(log, rootless, policy)[0].trace_is_fit && rootless.place_count() == 0);
  policy.variant = TokenReplayVariant::kForward;
  assert(replay_token_result(log, rootless, policy).traces[0].trace_is_fit);
  net = detached_model(-1, 1, true); policy.variant = TokenReplayVariant::kBackwards;
  const auto negative = replay_token_traces(log, net, policy);
  assert(negative[0].produced == -1 && negative[0].consumed == -1 && negative[0].missing == 2);
  assert(!negative[0].trace_is_fit && negative[0].trace_fitness == 2.0);
}

void detached_names_and_error_precedence() {
  PetriNet net;
  net.set_activities({"A"});
  net.add_place(); net.add_transition(0, {0}, {0});
  const TokenInteger wide("1000000000000000000000000000000000000007");
  net.set_discovery_markings({{{0, HeuristicStructuredValue{std::string("same")}}},
      {{{PetriMarkingPlaceKind::kDetached, 0}, wide}, {{PetriMarkingPlaceKind::kRoot, 0}, 1}},
      {{{PetriMarkingPlaceKind::kDetached, 0}, wide}}});
  EventLog log; log.traces = {trace("loop", {event("A")})};
  TokenReplayOptions policy; policy.place_names = {"same"}; policy.transition_names = {"A"}; policy.return_names = true;
  const auto result = replay_token_result(log, net, policy);
  assert(result.traces[0].reached_marking_order == (std::vector<std::size_t>{1, 0}));
  assert(result.traces[0].reached_marking[1] == wide && result.traces[0].reached_marking[0] == 1);
  const auto encoded = token_replay_result_json(result);
  const auto start = encoded.find("\"value\":\"reached_marking\"");
  const auto end = encoded.find("\"value\":\"missing_tokens\"", start);
  const auto marking = encoded.substr(start, end - start);
  assert(marking.find("\"value\":\"same\"") != std::string::npos && marking.find("\"type\":\"int\",\"value\":1}") != std::string::npos);
  assert(marking.find(wide.decimal()) == std::string::npos);
  policy.return_names = false;
  const auto identities = token_replay_result_json(replay_token_result(log, net, policy));
  assert(identities.find("\"kind\":\"detached\",\"id\":0") != std::string::npos && identities.find(wide.decimal()) != std::string::npos);
  net.set_activities({""});
  auto blank = event("");
  blank.activity_state = FirstClassAttributeState::kPresent;
  blank.attributes.push_back(EventAttribute{"concept:name", ""});
  log.traces = {trace("blank", {blank})}; policy.enable_pltr_fitness = true;
  bool rejected = false;
  try { (void)replay_token_result(log, net, policy); }
  catch (const TokenReplayTransitionKeyError& error) { rejected = error.transition_index() == 0; }
  assert(rejected);
  log.traces = {trace("empty", {})}; rejected = false;
  try { (void)replay_token_result(log, net, policy); }
  catch (const TokenReplayPlaceKeyError& error) { rejected = error.place_key() == (PetriMarkingPlace{PetriMarkingPlaceKind::kDetached, 0}); }
  assert(rejected);
}
void nullable_transition_names_and_wire() {
  PetriNet net;
  net.set_activities({"A"});
  net.add_place(); net.add_place(); net.set_source(0); net.set_sink(1);
  net.add_transition(0, {0}, {1});
  net.set_transition_names({std::nullopt});
  auto copy = net;
  net.set_transition_names({std::string("changed")});
  auto owned = std::move(copy);
  assert(owned.transition_names() && !owned.transition_names()->at(0));
  bool invalid = false;
  try { owned.set_transition_names({std::string("wrong"), std::nullopt}); }
  catch (const std::invalid_argument&) { invalid = true; }
  assert(invalid && !owned.transition_names()->at(0));
  EventLog log; log.traces = {trace("named", {event("A")})};
  TokenReplayOptions policy;
  auto result = replay_token_result(log, owned, policy);
  assert(result.traces[0].trace_is_fit && result.nullable_transition_names &&
         !result.nullable_transition_names->at(0));
  auto wire = token_replay_result_json(result);
  assert(wire.find("\"name\":{\"type\":\"NoneType\",\"value\":null}") != std::string::npos);
  policy.return_names = true;
  wire = token_replay_result_json(replay_token_result(log, owned, policy));
  assert(wire.find("\"value\":\"activated_transitions\"},{\"type\":\"list\",\"values\":[{\"type\":\"NoneType\",\"value\":null}]") != std::string::npos);
  policy.enable_pltr_fitness = true;
  assert(replay_token_result(log, owned, policy).pltr_fitness->transitions.size() == 1);
  policy.transition_names = {"legacy"};
  assert(replay_token_result(log, owned, policy).transition_names == std::vector<std::string>{"legacy"});
  policy.transition_names = {"ignored", "invalid-size"};
  policy.nullable_transition_names = PetriTransitionNameList{std::nullopt};
  result = replay_token_result(log, owned, policy);
  assert(result.nullable_transition_names && !result.nullable_transition_names->at(0));
  policy.nullable_transition_names = PetriTransitionNameList{std::string("")};
  result = replay_token_result(log, owned, policy);
  assert(result.nullable_transition_names->at(0) == std::optional<std::string>(""));
  wire = token_replay_result_json(result);
  assert(wire.find("\"values\":[{\"type\":\"str\",\"value\":\"\"}]") != std::string::npos);
  policy.nullable_transition_names = PetriTransitionNameList{}; invalid = false;
  try { (void)replay_token_result(log, owned, policy); }
  catch (const std::invalid_argument& e) { invalid = std::string(e.what()) == "nullable_transition_names must contain one entry per transition"; }
  assert(invalid);
  owned.add_transition(0, {1}, {1});
  assert(owned.transition_names()->at(1) == std::optional<std::string>("t1"));
}

void nullable_name_failure_precedence_and_variants() {
  PetriNet net;
  net.set_activities({"A"});
  net.add_place(); net.add_place(); net.set_source(0); net.set_sink(1);
  net.add_transition(0, {0}, {1}); net.add_transition(0, {0}, {1});
  const auto reject = [&](PetriTransitionNameList names, const std::string& message) {
    net.set_transition_names(std::move(names));
    for (const auto missing : {false, true}) {
      EventLog log;
      if (missing) log.traces = {trace("missing", {Event{}})};
      bool rejected = false;
      try { (void)replay_token_result(log, net, {}); }
      catch (const std::invalid_argument& e) { rejected = std::string(e.what()) == message; }
      assert(rejected);
    }
    TokenReplayOptions backwards; backwards.variant = TokenReplayVariant::kBackwards;
    assert(replay_token_traces(EventLog{}, net, backwards).empty());
    EventLog log; log.traces = {trace("backwards", {event("A")})};
    const auto rows = replay_token_traces(log, net, backwards);
    assert(rows[0].trace_is_fit && rows[0].activated_transitions == std::vector<std::size_t>{0});
  };
  reject({std::nullopt, std::string("A")}, "'<' not supported between instances of 'str' and 'NoneType'");
  reject({std::string("A"), std::nullopt}, "'<' not supported between instances of 'NoneType' and 'str'");
  reject({std::nullopt, std::nullopt}, "'<' not supported between instances of 'NoneType' and 'NoneType'");
  net.set_transition_names({std::string("B"), std::string("A")});
  EventLog log; log.traces = {trace("choice", {event("A")})};
  const auto result = replay_token_result(log, net, {});
  assert(result.traces[0].activated_transitions == std::vector<std::size_t>{1});
}

void alpha_plus_default_nullable_name_metadata() {
  Event null;
  null.activity_state = FirstClassAttributeState::kNull;
  null.attributes.push_back(EventAttribute{"concept:name", "", AttributeKind::kString, true});
  EventLog log; log.traces = {trace("null-loop", {null, null})};
  auto discovered = discover_alpha_plus_result(log);
  assert(discovered.net.transition_names() && discovered.net.transition_names()->size() == discovered.net.transitions().size());
  assert(discovered.net.transitions().size() == 1 && !discovered.net.transition_names()->at(0));
  TokenReplayOptions policy; policy.return_names = true;
  const auto result = replay_token_result(log, discovered.net, policy);
  assert(result.nullable_transition_names && !result.nullable_transition_names->at(0));
  const auto wire = token_replay_result_json(result);
  assert(wire.find("\"values\":[{\"type\":\"NoneType\",\"value\":null},{\"type\":\"NoneType\",\"value\":null}]") != std::string::npos);
}
}  // namespace
int main() {
  variants_and_keys();
  contexts_and_stop();
  rejection_and_transport();
  typed_variant_and_null_contracts();
  supplied_hidden_paths();
  cleaned_underfed_place_precedence();
  discovery_marking_replay();
  detached_counts_keys_and_overrides();
  detached_names_and_error_precedence();
  nullable_transition_names_and_wire();
  nullable_name_failure_precedence_and_variants();
  alpha_plus_default_nullable_name_metadata();
  std::cout << "full forward token replay result contracts passed\n";
}
