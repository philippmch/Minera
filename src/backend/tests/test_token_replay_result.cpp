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
  log.traces = {trace("mixed", {event("A"), null})};
  bool rejected = false;
  try { (void)replay_token_result(log, model(), policy); }
  catch (const std::invalid_argument& error) {
    rejected = std::string(error.what()) == "'<' not supported between instances of 'str' and 'NoneType'";
  }
  assert(rejected && replay_token_traces(log, model(), policy).size() == 1);
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
}  // namespace
int main() {
  variants_and_keys();
  contexts_and_stop();
  rejection_and_transport();
  std::cout << "full forward token replay result contracts passed\n";
}
