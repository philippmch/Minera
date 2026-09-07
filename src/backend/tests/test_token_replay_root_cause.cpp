#include "minera/conformance.hpp"
#include "minera/log_conversion.hpp"
#include "minera/xes_event_log_importer.hpp"

#include <algorithm>
#include <iostream>
#include <limits>
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
void equal_trace_membership_contracts() {
  EventLog log;
  log.traces = {trace({event("a", "1")}), trace({event("a", "1")}), trace({event("b", "8")})};
  log.traces[0].events[0].attributes[0].kind = AttributeKind::kInt;
  log.traces[0].events[0].attributes[0].typed_value = std::int64_t{1};
  log.traces[1].properties.push_back({"ignored", "metadata"});
  RootCauseOptions options;
  options.numeric_attributes = {"cost"};
  TokenReplayTransitionRootCauseCohort cohort{7, {{0,event("", "3")}}, {{2,event("", "8")}}};
  auto rows = token_replay_transition_root_cause_diagnostics(log, {cohort}, options);
  require(rows.size() == 1 && rows[0].source_trace_indices == std::vector<std::size_t>({0,1,2}),
          "one trace-map entry matches equal source values without losing row identities");
  require(rows[0].data == std::vector<std::vector<double>>({{3},{3},{8}}) && rows[0].fit_count == 2,
          "matched traces share the supplied first context");
  cohort.fit_contexts = {{0,event("", "3")}, {1,event("", "99")}, {2,event("", "2")}};
  cohort.problem_contexts = {{1,event("", "9")}};
  rows = token_replay_transition_root_cause_diagnostics(log, {cohort}, options);
  require(rows[0].source_trace_indices == std::vector<std::size_t>({2,0,1}) &&
          rows[0].data == std::vector<std::vector<double>>({{2},{9},{9}}),
          "problem membership overrides the whole equal-trace class");
  TokenReplayOptions replay;
  replay.root_cause = options;
  const auto unknown = token_replay_unknown_root_cause_diagnostics(log,
      {{"X",false,{{0,event("", "9")}}}}, replay);
  require(unknown.size() == 1 && unknown[0].source_trace_indices == std::vector<std::size_t>({2,0,1}) &&
          unknown[0].data == std::vector<std::vector<double>>({{8},{9},{9}}),
          "unknown problem membership also expands trace-map equality");
  log.traces[1].case_id = "different-case";
  cohort.fit_contexts = {{0,event("", "3")}};
  cohort.problem_contexts = {{2,event("", "8")}};
  rows = token_replay_transition_root_cause_diagnostics(log, {cohort}, options);
  require(rows[0].source_trace_indices == std::vector<std::size_t>({0,2}),
          "different trace attributes prevent event-equal traces from matching");
  log.traces.assign(2000, trace({event("a", "1")}));
  log.traces.push_back(trace({event("b", "8")}));
  cohort.fit_contexts.clear();
  for (std::size_t i = 0; i < 2000; ++i)
    cohort.fit_contexts.push_back({i,event("", i == 0 ? "1" : "99")});
  cohort.problem_contexts = {{2000,event("", "8")}};
  rows = token_replay_transition_root_cause_diagnostics(log, {cohort}, options);
  require(rows[0].data.size() == 2001 && rows[0].fit_count == 2000 &&
          rows[0].data[1999][0] == 1 && rows[0].source_trace_indices[1999] == 1999,
          "large repeated memberships expand once and retain all original source rows");
}
void nan_trace_membership_contracts() {
  EventLog original;
  original.traces = {trace({event("a", "1")})};
  EventAttribute cell{"x", "nan", AttributeKind::kFloat};
  cell.typed_value = std::numeric_limits<double>::quiet_NaN();
  original.traces[0].attributes.push_back(cell);
  const auto streamed = event_stream_to_event_log(event_log_to_event_stream(original));
  const auto tabled = event_table_to_event_log(event_log_to_table(original));
  EventLog log = original;
  log.traces.push_back(original.traces[0]);
  log.traces.push_back(streamed.traces.at(0));
  log.traces.push_back(tabled.traces.at(0));
  log.traces.push_back(original.traces[0]);
  log.traces.back().attributes[0].typed_value = std::numeric_limits<double>::quiet_NaN();
  log.traces.push_back(trace({event("b", "8")}));
  RootCauseOptions options;
  options.numeric_attributes = {"cost"};
  const auto rows = token_replay_transition_root_cause_diagnostics(log,
      {{7, {{0,event("", "1")}}, {{5,event("", "8")}}}}, options);
  require(rows.size() == 1 && rows[0].source_trace_indices == std::vector<std::size_t>({0,1,2,3,5}),
          "copied and converted NaN trace aliases match, independently assigned NaN does not");
  require(rows[0].data == std::vector<std::vector<double>>({{1},{1},{1},{1},{8}}),
          "NaN trace alias membership retains each original source row");
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
void typed_unknown_activity_keys() {
  const auto typed_event = [](EventAttribute activity, const std::string& cost) {
    auto value = event("ignored", cost);
    activity.key = "kind";
    value.attributes.push_back(std::move(activity));
    return value;
  };
  EventAttribute integer{"kind", "1", AttributeKind::kInt};
  EventAttribute boolean{"kind", "true", AttributeKind::kBoolean};
  EventAttribute real{"kind", "1.0", AttributeKind::kFloat};
  EventAttribute null{"kind", "ignored", AttributeKind::kString, true};
  EventLog log;
  log.traces = {trace({typed_event(integer, "1")}), trace({typed_event({"kind", "1"}, "2")}),
                trace({typed_event(boolean, "3")}), trace({typed_event(real, "4")}),
                trace({typed_event(null, "5")}), trace({typed_event({"kind", "None"}, "6")}),
                trace({typed_event({"kind", "known"}, "7")})};
  TokenReplayOptions options;
  options.activity_attribute = "kind";
  options.root_cause.numeric_attributes = {"cost"};
  TokenReplayUnknownRootCauseCohort cohort{"legacy ignored", true, {{0, event("", "99")}}, integer};
  auto rows = token_replay_unknown_root_cause_diagnostics(log, {cohort}, options);
  require(rows.size() == 1, "typed numeric key produces a model");
  require(rows[0].source_trace_indices == std::vector<std::size_t>({1,4,5,6,0}), "integer key excludes equal bool and float but not string");
  require(rows[0].activity == "1" && !rows[0].activity_is_null && rows[0].activity_attribute && rows[0].activity_attribute->kind == AttributeKind::kInt, "typed output overrides legacy fields");
  cohort.activity_attribute.reset(); cohort.activity = "1"; cohort.activity_is_null = false;
  cohort.problem_contexts = {{1, event("", "99")}};
  rows = token_replay_unknown_root_cause_diagnostics(log, {cohort}, options);
  require(rows[0].source_trace_indices == std::vector<std::size_t>({0,2,3,4,5,6,1}), "string key remains distinct from numeric key");
  cohort.activity_attribute = null; cohort.problem_contexts = {{4, event("", "99")}};
  rows = token_replay_unknown_root_cause_diagnostics(log, {cohort}, options);
  require(rows[0].source_trace_indices == std::vector<std::size_t>({0,1,2,3,5,6,4}), "null key does not exclude literal None");
  require(rows[0].activity_is_null && rows[0].activity_attribute->is_null, "typed null output retained");
  // Original-source provenance still selects the typed generic cell.
  for (auto& item : log.traces) {
    auto& cell = item.events[0].attributes.back();
    cell.key = "transport";
    cell.first_class_source_key = "kind";
  }
  cohort.activity_attribute = boolean; cohort.problem_contexts = {{2, event("", "99")}};
  rows = token_replay_unknown_root_cause_diagnostics(log, {cohort}, options);
  require(rows[0].source_trace_indices == std::vector<std::size_t>({1,4,5,6,2}), "typed selector provenance preserved");

  EventAttribute nan{"kind", "NaN", AttributeKind::kFloat};
  nan.typed_value = std::numeric_limits<double>::quiet_NaN();
  auto independent = nan;
  independent.typed_value = std::numeric_limits<double>::quiet_NaN();
  log.traces = {trace({typed_event(nan, "1")}), trace({typed_event(nan, "2")}),
                trace({typed_event(independent, "3")}), trace({typed_event({"kind", "known"}, "4")})};
  cohort.activity_attribute = nan; cohort.problem_contexts = {{0, event("", "99")}};
  rows = token_replay_unknown_root_cause_diagnostics(log, {cohort}, options);
  require(rows[0].source_trace_indices == std::vector<std::size_t>({2,3,0}), "NaN unknown membership follows scalar aliases");
  cohort.activity_attribute->children.push_back({"nested", "value"});
  bool rejected = false;
  try { (void)token_replay_unknown_root_cause_diagnostics(log, {cohort}, options); }
  catch (const std::invalid_argument&) { rejected = true; }
  require(rejected, "nested explicit keys are unhashable");
  cohort.activity_attribute = nan;
  log.traces[3].events[0].attributes.back().container = AttributeContainerKind::kList;
  rows = token_replay_unknown_root_cause_diagnostics(log, {cohort}, options);
  require(rows[0].source_trace_indices == std::vector<std::size_t>({2,3,0}), "nested selected activities cannot match scalar keys and stay baseline");
  EventAttribute date{"kind", "2026-01-02T03:04:05.123456+02:30", AttributeKind::kDate};
  EventAttribute same_date{"kind", "2026-01-02T00:34:05.123456Z", AttributeKind::kDate};
  log.traces = {trace({typed_event(date, "1")}), trace({typed_event(same_date, "2")}),
                trace({typed_event({"kind", date.value}, "3")}), trace({typed_event({"kind", "known"}, "4")})};
  cohort.activity_attribute = date;
  rows = token_replay_unknown_root_cause_diagnostics(log, {cohort}, options);
  require(rows[0].source_trace_indices == std::vector<std::size_t>({2,3,0}), "typed date membership compares instants but not date strings");
}

void unknown_baseline_cache_preserves_lazy_validation() {
  EventLog log;
  auto baseline = event("a", "1");
  baseline.attributes.push_back({"kind", "known"});
  auto problematic = event("b", "8");
  problematic.attributes.push_back({"kind", "X"});
  problematic.attributes.push_back({"kind", "Y"});
  log.traces = {trace({baseline}), trace({problematic})};
  TokenReplayOptions options;
  options.activity_attribute = "kind";
  options.root_cause.numeric_attributes = {"cost"};
  std::vector<TokenReplayUnknownRootCauseCohort> cohorts{
      {"X", false, {{1, event("", "8")}}},
      {"Y", false, {{1, event("", "9")}}}};
  const auto rows = token_replay_unknown_root_cause_diagnostics(log, cohorts, options);
  require(rows.size() == 2 && rows[0].data[0][0] == 1 && rows[1].data[0][0] == 1,
          "baseline caching never evaluates a trace while it belongs to the problem cohort");
  cohorts.push_back({"Z", false, {{0, event("", "10")}}});
  bool rejected = false;
  try { (void)token_replay_unknown_root_cause_diagnostics(log, cohorts, options); }
  catch (const std::invalid_argument&) { rejected = true; }
  require(rejected, "selector errors still arise when a formerly problematic trace first becomes a baseline candidate");
}

void integrated_typed_unknown_activity_keys() {
  const auto unknown_event = [](EventAttribute cell, const std::string& cost) {
    auto value = event("1", cost);
    value.activity_state = FirstClassAttributeState::kPresent;
    value.activity_source_key = "concept:name";
    cell.key = "concept:name";
    value.activity_attribute = std::move(cell);
    return value;
  };
  EventLog log;
  log.traces = {trace({event("a", "1"), event("b", "2")}),
                trace({event("a", "3"), unknown_event({"", "1", AttributeKind::kInt}, "8"), event("b", "9")}),
                trace({event("a", "4"), unknown_event({"", "1"}, "10"), event("b", "11")})};
  std::vector<TokenReplayTraceDiagnostics> diagnostics(3);
  for (std::size_t i = 0; i < diagnostics.size(); ++i) {
    diagnostics[i].trace_index = i;
    diagnostics[i].replayed_events = log.traces[i].events.size();
    if (i) {
      diagnostics[i].unknown_event_indices = {1};
      diagnostics[i].unknown_activities = {"1"};
      diagnostics[i].unknown_activity_is_null = {false};
    }
  }
  TokenReplayOptions options;
  options.root_cause.numeric_attributes = {"cost"};
  const auto result = compute_token_replay_root_cause_diagnostics(log, sequence(), diagnostics, options);
  require(result.unknown_activities.size() == 2, "same spelling retains distinct typed unknown groups");
  require(result.unknown_activities[0].activity_attribute->kind == AttributeKind::kInt && result.unknown_activities[1].activity_attribute->kind == AttributeKind::kString, "integrated diagnostics preserve original typed cells");
  require(result.unknown_activities[0].data == std::vector<std::vector<double>>({{2},{9}}) && result.unknown_activities[1].data == std::vector<std::vector<double>>({{2},{11}}), "typed groups preserve known-event replay contexts and shared baseline exclusion");
}

int main() {
  typed_unknown_activity_keys();
  unknown_baseline_cache_preserves_lazy_validation();
  integrated_typed_unknown_activity_keys();
  cohort_contracts();
  integration_contracts();
  equal_trace_membership_contracts();
  nan_trace_membership_contracts();
  xes_encoding_contracts();
  std::cout << "root-cause replay contracts passed\n";
}
