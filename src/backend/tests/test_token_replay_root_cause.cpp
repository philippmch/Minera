#include "minera/conformance.hpp"
#include "minera/xes_event_log_importer.hpp"

#include <algorithm>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace minera;
namespace {
void require(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}
Event event(const std::string& activity, const std::string& cost) {
  Event result;
  result.activity = activity;
  if (!cost.empty()) result.attributes.push_back({"cost", cost});
  return result;
}
Trace trace(std::initializer_list<Event> events) {
  Trace result;
  result.case_id = "same-id";
  result.events = events;
  return result;
}
PetriNet sequence() {
  PetriNet net;
  net.set_activities({"a", "b"});
  auto source = net.add_place(), middle = net.add_place(), sink = net.add_place();
  net.set_source(source); net.set_sink(sink);
  net.add_transition(0, {source}, {middle});
  net.add_transition(1, {middle}, {sink});
  return net;
}
void cohort_contracts() {
  EventLog log;
  log.traces = {trace({event("a", "1"), event("b", "2")}),
                trace({event("X", "8")}), trace({event("Y", "9")}),
                trace({event("a", "3")})};
  RootCauseOptions options;
  options.numeric_attributes = {"cost"};
  TokenReplayTransitionRootCauseCohort cohort;
  cohort.transition_index = 7;
  cohort.fit_contexts = {{3, event("", "3")}, {1, event("", "100")}, {0, event("", "1")}};
  cohort.problem_contexts = {{1, event("", "8")}, {1, event("", "99")}};
  auto rows = token_replay_transition_root_cause_diagnostics(log, {cohort}, options);
  require(rows.size() == 1 && rows[0].transition_index == 7, "supplied transition identity");
  require(rows[0].source_trace_indices == std::vector<std::size_t>({0,3,1}),
          "log order, problem precedence and repeated observations");
  require(rows[0].data == std::vector<std::vector<double>>({{1},{3},{8}}), "first supplied context");
  require(rows[0].classes == std::vector<std::string>({"fit","underfed"}), "class names");
  options.enable_multiplier = true;
  rows = token_replay_transition_root_cause_diagnostics(log, {cohort}, options);
  require(rows[0].source_trace_indices == std::vector<std::size_t>({0,3,1,1}), "whole smaller cohort repetition");
  require(rows[0].fit_count == 2 && rows[0].problem_count == 1, "original counts");
  cohort.fit_contexts.clear();
  require(token_replay_transition_root_cause_diagnostics(log, {cohort}, {}).empty(), "one-sided omitted before encoding");
  TokenReplayOptions replay;
  replay.root_cause.numeric_attributes = {"cost"};
  auto unknown = token_replay_unknown_root_cause_diagnostics(log,
      {{"X",false,{{1,event("", "99")}}},{"Y",false,{}}}, replay);
  require(unknown.size() == 1 && unknown[0].source_trace_indices == std::vector<std::size_t>({0,3,1}),
          "all supplied unknown keys exclude baseline, including empty cohort");
  require(unknown[0].data == std::vector<std::vector<double>>({{2},{3},{99}}),
          "baseline final event and supplied problem context");
  require(unknown[0].classes == std::vector<std::string>({"fit","containing"}), "unknown classes");
  log.traces[0].events.back().attributes.clear();
  bool rejected = false;
  try { (void)token_replay_unknown_root_cause_diagnostics(log, {{"X",false,{{1,event("", "99")}}}}, replay); }
  catch (const std::invalid_argument&) { rejected = true; }
  require(rejected, "baseline does not carry earlier attributes");
  log.traces[0].events = {event("", "1")};
  unknown = token_replay_unknown_root_cause_diagnostics(log,
      {{"X",false,{{1,event("", "99")}}}}, replay);
  require(unknown[0].data[0][0] == 1, "direct helper accepts baseline missing activity");
  log.traces = {trace({event("a", "1")}), Trace{}, trace({event("X", "8")})};
  unknown = token_replay_unknown_root_cause_diagnostics(log,
      {{"X",false,{{2,event("", "8")}}}}, replay);
  require(unknown.size() == 1 && unknown[0].source_trace_indices == std::vector<std::size_t>({0,2}),
          "empty baseline traces are omitted");
  log.traces[0].events.clear();
  require(token_replay_unknown_root_cause_diagnostics(log,
      {{"X",false,{{2,event("", "8")}}}}, replay).empty(),
      "empty-only baseline omits model before encoding");
  log.traces = {trace({event("a", "1")}), Trace{}};
  unknown = token_replay_unknown_root_cause_diagnostics(log,
      {{"X",false,{{1,event("", "8")}}}}, replay);
  require(unknown.size() == 1 && unknown[0].source_trace_indices == std::vector<std::size_t>({0,1}),
          "explicit problem context may belong to an empty trace");
  log.traces = {trace({event("X", "1")}), trace({event("a", "2")}), trace({event("X", "8")})};
  auto& projected = log.traces[0].events[0];
  projected.activity_state = FirstClassAttributeState::kPresent;
  projected.activity_attribute = EventAttribute{"task", "X"};
  projected.activity_attribute->first_class_role = FirstClassAttributeRole::kActivity;
  unknown = token_replay_unknown_root_cause_diagnostics(log,
      {{"X",false,{{2,event("", "8")}}}}, replay);
  require(unknown[0].source_trace_indices == std::vector<std::size_t>({0,1,2}),
          "missing retained activity cell does not exclude baseline using stale projection");
  projected.activity_attribute->first_class_state = FirstClassAttributeState::kNull;
  unknown = token_replay_unknown_root_cause_diagnostics(log,
      {{"",true,{{2,event("", "8")}}}}, replay);
  require(unknown[0].source_trace_indices == std::vector<std::size_t>({1,2}),
          "null retained activity cell excludes null unknown baseline");
  projected.activity_attribute->first_class_state = FirstClassAttributeState::kPresent;
  replay.activity_attribute = "task";
  unknown = token_replay_unknown_root_cause_diagnostics(log,
      {{"X",false,{{2,event("", "8")}}}}, replay);
  require(unknown[0].source_trace_indices == std::vector<std::size_t>({1,2}),
          "retained activity key resolves custom selector without source projection");
  for (const auto* key : {"shadow-one", "shadow-two"}) {
    EventAttribute alias{key, "ignored"};
    alias.first_class_source_key = "task";
    projected.attributes.push_back(alias);
  }
  projected.attributes.push_back({"task", "X"});
  unknown = token_replay_unknown_root_cause_diagnostics(log,
      {{"X",false,{{2,event("", "8")}}}}, replay);
  require(unknown[0].source_trace_indices == std::vector<std::size_t>({1,2}),
          "exact activity key overrides duplicate lower-priority aliases");
}
void integration_contracts() {
  EventLog log;
  log.traces = {trace({event("a","1"),event("b","2")}), trace({event("b","8")}),
    trace({event("a","1"),event("X","5"),event("b","9")}),
    trace({event("b","11"),event("b","12")})};
  auto net = sequence();
  auto plain = check_conformance(log, net);
  require(!plain.root_cause_diagnostics, "opt-in default");
  TokenReplayOptions options;
  options.include_root_cause_diagnostics = true;
  options.root_cause.numeric_attributes = {"cost"};
  auto result = check_conformance(log, net, options);
  require(result.root_cause_diagnostics.has_value() && !result.trace_diagnostics, "independent root opt-in");
  require(result.fitness == plain.fitness && result.precision == plain.precision &&
          result.missing == plain.missing && result.remaining == plain.remaining &&
          result.transition_frequencies == plain.transition_frequencies, "metrics unchanged");
  auto& root = *result.root_cause_diagnostics;
  require(root.transitions.size() == 1 && root.transitions[0].transition_index == 1, "underfed transition");
  require(root.transitions[0].source_trace_indices == std::vector<std::size_t>({0,2,1,3}), "cohort source identity");
  require(root.transitions[0].data == std::vector<std::vector<double>>({{2},{9},{8},{12}}), "final accumulated known context");
  require(root.unknown_activities.size() == 1 && root.unknown_activities[0].activity == "X", "unknown model");
  require(root.unknown_activities[0].data == std::vector<std::vector<double>>({{2},{8},{12},{9}}), "unknown context alias");
  log.traces[2].events.back().attributes.clear();
  result = check_conformance(log, net, options);
  require(result.root_cause_diagnostics->transitions[0].data[1][0] == 1 &&
          result.root_cause_diagnostics->unknown_activities[0].data.back()[0] == 1, "unknown attributes never update context");
  options.stop_immediately_unfit = true;
  result = check_conformance(log, net, options);
  require(result.root_cause_diagnostics->transitions.empty(), "blocked unfired transitions have no contexts");
  require(compute_token_replay_root_cause_diagnostics(EventLog{}, net, {}, options).transitions.empty(), "empty diagnostics");

  options.stop_immediately_unfit = false;
  options.root_cause.numeric_attributes.clear();
  options.root_cause.string_attributes = {"org:resource"};
  log.traces = {trace({event("a",""),event("b","")}),trace({event("b","")})};
  log.traces[0].events[0].resource = "earlier";
  log.traces[0].events[1].resource_attribute = EventAttribute{};
  log.traces[0].events[1].resource = "stale missing projection";
  log.traces[1].events[0].resource = "problem";
  result = check_conformance(log, net, options);
  require(result.root_cause_diagnostics->transitions[0].feature_names ==
          std::vector<std::string>({"event:org:resource@earlier","event:org:resource@problem"}),
          "missing first-class placeholders do not erase accumulated context");
  log.traces[0].events[1].resource_state = FirstClassAttributeState::kPresent;
  log.traces[0].events[1].resource_attribute->first_class_role = FirstClassAttributeRole::kResource;
  result = check_conformance(log, net, options);
  require(result.root_cause_diagnostics->transitions[0].feature_names[0] == "event:org:resource@earlier",
          "missing retained cell does not erase earlier context with a present outer projection");
}
void xes_encoding_contracts() {
  const auto verify = [](const std::string& cell, const std::string& category) {
    std::istringstream xml("<log><trace><event><string key=\"concept:name\" value=\"b\"/>" + cell +
      "</event></trace><trace><event><string key=\"concept:name\" value=\"b\"/>"
      "<string key=\"v\" value=\"other\"/></event></trace></log>");
    const auto log = import_xes_event_log(xml);
    RootCauseOptions options;
    options.string_attributes = {"v"};
    const auto rows = token_replay_transition_root_cause_diagnostics(log,
        {{1, {{0, log.traces[0].events[0]}}, {{1, log.traces[1].events[0]}}}}, options);
    require(rows.size() == 1, "imported nested model");
    const auto feature = "event:v@" + category;
    const auto found = std::find(rows[0].feature_names.begin(), rows[0].feature_names.end(), feature);
    require(found != rows[0].feature_names.end(), "imported categorical representation");
    const auto column = static_cast<std::size_t>(found - rows[0].feature_names.begin());
    require(rows[0].data[0][column] == 1 && rows[0].data[1][column] == 0, "imported one-hot identity");
  };
  verify("<string key=\"v\" value=\"parent\"><int key=\"n\" value=\"2\"/>"
         "<boolean key=\"b\" value=\"true\"/></string>",
         "{'value': 'parent', 'children': {'n': 2, 'b': True}}");
  verify("<list key=\"v\"><values><string key=\"x\" value=\"hello\"/>"
         "<int key=\"n\" value=\"2\"/></values></list>",
         "{'value': None, 'children': [('x', 'hello'), ('n', 2)]}");
  verify("<list key=\"v\"><values/></list>", "{'value': None, 'children': []}");
  verify("<date key=\"v\" value=\"2026-01-02T03:04:05.125Z\"/>", "2026-01-02 03:04:05.125000+00:00");
  verify("<float key=\"v\" value=\"2.000\"/>", "2.0");
}
}
int main() {
  cohort_contracts();
  integration_contracts();
  xes_encoding_contracts();
  std::cout << "root-cause replay contracts passed\n";
}
