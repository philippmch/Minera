#undef NDEBUG
#include "minera/conformance.hpp"
#include "minera/directly_follows_graph.hpp"
#include "minera/json_export.hpp"
#include "minera/request_params.hpp"

#include <cassert>
#include <map>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
using Fields = std::map<std::string, std::string>;
minera::FieldLookup lookup(const Fields& fields) {
  return [&fields](std::string_view name) {
    const auto found = fields.find(std::string(name));
    return found == fields.end() ? std::string{} : found->second;
  };
}
minera::FieldPresence presence(const Fields& fields) {
  return [&fields](std::string_view name) { return fields.contains(std::string(name)); };
}

void replay_policy_request_contract() {
  const Fields empty;
  const auto defaults = minera::parse_token_replay_params(lookup(empty));
  assert(defaults.consider_remaining_in_fitness);
  assert(defaults.try_to_reach_final_marking_through_hidden);
  assert(!defaults.consider_activities_not_in_model_in_fitness);
  assert(defaults.case_id_attribute == "case:concept:name");
  assert(!defaults.max_rows_to_materialize);
  assert(defaults.walk_through_hidden_transitions);
  assert(!defaults.cleaning_token_flood);
  assert(!defaults.exhaustive_invisible_exploration);
  assert(!defaults.include_trace_diagnostics);
  assert(!defaults.include_duration_diagnostics);
  assert(!defaults.include_root_cause_diagnostics);
  assert(!defaults.root_cause.enable_multiplier);
  assert(defaults.root_cause.string_attributes.empty() && defaults.root_cause.numeric_attributes.empty());
  assert(defaults.timestamp_attribute == "time:timestamp");
  const Fields configured{
      {"token_replay_variant", "backwards"},
      {"token_replay_activity_attribute", " phase "},
      {"token_replay_stop_immediately_unfit", "1"},
      {"token_replay_consider_remaining_in_fitness", "false"},
      {"token_replay_try_to_reach_final_marking_through_hidden", "0"},
      {"token_replay_consider_activities_not_in_model_in_fitness", "false"},
      {"token_replay_cleaning_token_flood", "true"},
      {"token_replay_exhaustive_invisible_exploration", "true"},
      {"token_replay_walk_through_hidden_transitions", "false"},
      {"token_replay_include_trace_diagnostics", "true"},
      {"token_replay_include_duration_diagnostics", "true"},
      {"token_replay_include_root_cause_diagnostics", "true"},
      {"token_replay_root_cause_enable_multiplier", "1"},
      {"token_replay_root_cause_string_attributes", " team ,region,team "},
      {"token_replay_root_cause_numeric_attributes", "cost,cost"},
      {"token_replay_timestamp_attribute", " completed_at "},
      {"token_replay_case_id_attribute", " group "},
      {"token_replay_max_rows_to_materialize", "0"}};
  const auto options = minera::parse_token_replay_params(lookup(configured), presence(configured));
  assert(options.cleaning_token_flood);
  assert(options.exhaustive_invisible_exploration);
  assert(options.variant == minera::TokenReplayVariant::kBackwards);
  assert(options.activity_attribute == "phase" && options.stop_immediately_unfit);
  assert(!options.consider_remaining_in_fitness);
  assert(!options.try_to_reach_final_marking_through_hidden);
  assert(!options.consider_activities_not_in_model_in_fitness);
  assert(!options.walk_through_hidden_transitions && options.include_trace_diagnostics);
  assert(options.include_duration_diagnostics && options.timestamp_attribute == "completed_at");
  assert(options.include_root_cause_diagnostics && options.root_cause.enable_multiplier);
  assert((options.root_cause.string_attributes == std::vector<std::string>{"team", "region", "team"}));
  assert((options.root_cause.numeric_attributes == std::vector<std::string>{"cost", "cost"}));
  assert(options.case_id_attribute == "group");
  assert(options.max_rows_to_materialize && *options.max_rows_to_materialize == 0);
  const auto mine = minera::parse_mine_params(lookup(configured), presence(configured));
  assert(mine.token_replay.cleaning_token_flood);
  assert(mine.token_replay.exhaustive_invisible_exploration);
  assert(mine.token_replay_activity_attribute_supplied);
  assert(mine.token_replay.include_trace_diagnostics);
  assert(mine.token_replay.include_duration_diagnostics);
  assert(mine.token_replay.include_root_cause_diagnostics && mine.token_replay.root_cause.enable_multiplier);
  assert(mine.token_replay.root_cause.numeric_attributes == options.root_cause.numeric_attributes);
  assert(mine.token_replay.timestamp_attribute == "completed_at");
  assert(!mine.token_replay.walk_through_hidden_transitions);
  assert(!mine.token_replay.consider_remaining_in_fitness);

  for (const auto* name : {"token_replay_variant", "token_replay_activity_attribute",
         "token_replay_stop_immediately_unfit", "token_replay_consider_remaining_in_fitness",
         "token_replay_try_to_reach_final_marking_through_hidden",
         "token_replay_consider_activities_not_in_model_in_fitness",
         "token_replay_cleaning_token_flood",
         "token_replay_exhaustive_invisible_exploration",
         "token_replay_walk_through_hidden_transitions", "token_replay_include_trace_diagnostics",
         "token_replay_include_duration_diagnostics", "token_replay_timestamp_attribute",
         "token_replay_include_root_cause_diagnostics", "token_replay_root_cause_enable_multiplier",
         "token_replay_case_id_attribute", "token_replay_max_rows_to_materialize"}) {
    for (const auto* value : {"", "  ", "invalid"}) {
      if ((std::string_view(name) == "token_replay_activity_attribute" ||
           std::string_view(name) == "token_replay_case_id_attribute" ||
           std::string_view(name) == "token_replay_timestamp_attribute") &&
          std::string_view(value) == "invalid") continue;
      const Fields fields{{name, value}};
      bool rejected = false;
      try { (void)minera::parse_token_replay_params(lookup(fields), presence(fields)); }
      catch (const std::invalid_argument&) { rejected = true; }
      assert(rejected);
      rejected = false;
      try { (void)minera::parse_mine_params(lookup(fields), presence(fields)); }
      catch (const std::invalid_argument&) { rejected = true; }
      assert(rejected);
    }
  }
  for (const auto* value : {"false", "0"}) {
    const Fields exhaustive_fields{{"token_replay_exhaustive_invisible_exploration", value}};
    assert(!minera::parse_token_replay_params(lookup(exhaustive_fields), presence(exhaustive_fields)).exhaustive_invisible_exploration);
    const Fields fields{{"token_replay_cleaning_token_flood", value}};
    assert(!minera::parse_token_replay_params(lookup(fields), presence(fields)).cleaning_token_flood);
  }
  for (const auto* value : {"-1", "0.5", "184467440737095516160"}) {
    const Fields invalid{{"token_replay_max_rows_to_materialize", value}};
    bool rejected = false;
    try { (void)minera::parse_token_replay_params(lookup(invalid), presence(invalid)); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
  }
  for (const auto* name : {"token_replay_root_cause_string_attributes", "token_replay_root_cause_numeric_attributes"}) {
    for (const auto* value : {",", "a,", ",b", "a, ,b", "a\nb"}) {
      const Fields invalid{{name, value}};
      bool rejected = false;
      try { (void)minera::parse_token_replay_params(lookup(invalid), presence(invalid)); }
      catch (const std::invalid_argument&) { rejected = true; }
      assert(rejected);
    }
    const Fields empty_list{{name, ""}};
    const auto parsed = minera::parse_token_replay_params(lookup(empty_list), presence(empty_list));
    assert(parsed.root_cause.string_attributes.empty() && parsed.root_cause.numeric_attributes.empty());
  }
}

void replay_diagnostics_json_contract() {
  minera::PetriNet net;
  net.set_activities({"A"});
  const auto source = net.add_place(), sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.add_transition(0, {source}, {sink});
  minera::EventLog log;
  minera::Trace trace;
  trace.case_id = "case\"one";
  minera::Event event;
  event.activity = "A";
  trace.events.push_back(event);
  log.traces.push_back(trace);
  log.traces.push_back(trace);
  minera::DirectlyFollowsGraph graph;
  graph.ensure_activity("A");
  minera::ModelConformance model;
  model.miner = "imported";
  model.token_replay = minera::check_conformance(log, net);
  assert(!model.token_replay.trace_diagnostics);
  const auto compact = minera::imported_model_json(model, graph);
  assert(compact.find("trace_diagnostics") == std::string::npos);
  minera::TokenReplayOptions options;
  options.include_trace_diagnostics = true;
  model.token_replay = minera::check_conformance(log, net, options);
  const auto json = minera::imported_model_json(model, graph);
  assert(json.find("\"trace_diagnostics\": [") != std::string::npos);
  assert(json.find("\"trace_index\": 1") != std::string::npos);
  assert(json.find("\"case_id\": \"case\\\"one\"") != std::string::npos);
  assert(json.find("\"trace_is_fit\": true") != std::string::npos);
  assert(json.find("\"activated_transitions\": [0]") != std::string::npos);
  assert(json.find("\"event_diagnostics\": [{\"event_index\": 0, \"activated_transitions\": [0], \"transitions_with_problems\": []}]") != std::string::npos);
  assert(json.find("\"reached_marking\": [0, 1]") != std::string::npos);
  assert(json.find("\"unknown_activities\": []") != std::string::npos);
  assert(json.find("\"places\": [{") != std::string::npos);
  assert(model.token_replay.variants.size() == 1);
  assert(model.token_replay.trace_diagnostics->size() == 2);
  log.traces.clear();
  model.token_replay = minera::check_conformance(log, net, options);
  assert(minera::imported_model_json(model, graph).find("\"trace_diagnostics\": []") != std::string::npos);
}
void replay_duration_json_contract() {
  minera::PetriNet net;
  net.set_activities({"A", "B"});
  for (int i = 0; i < 3; ++i) net.add_place();
  net.set_source(0);
  net.set_sink(2);
  net.add_transition(0, {0}, {1});
  net.add_transition(1, {1}, {2});
  minera::EventLog log;
  const auto append = [&](std::initializer_list<std::pair<const char*, const char*>> steps) {
    minera::Trace trace;
    trace.case_id = "shared-case-id";
    for (const auto& [activity, timestamp] : steps) {
      minera::Event event;
      event.activity = activity;
      event.attributes.push_back({"finished", timestamp});
      trace.events.push_back(std::move(event));
    }
    log.traces.push_back(std::move(trace));
  };
  append({{"A", "2026-01-01T00:00:00Z"}, {"B", "2026-01-01T00:00:10Z"}});
  append({{"B", "2026-01-01T00:00:00Z"}, {"B", "2026-01-01T00:00:30Z"}});
  append({{"A", "2026-01-01T00:00:00Z"}, {"X", "2026-01-01T00:00:20Z"},
          {"B", "2026-01-01T00:00:40Z"}});
  minera::DirectlyFollowsGraph graph;
  graph.ensure_activity("A");
  graph.ensure_activity("B");
  minera::ModelConformance model;
  model.miner = "imported";
  model.token_replay = minera::check_conformance(log, net);
  assert(minera::imported_model_json(model, graph).find("duration_diagnostics") == std::string::npos);
  minera::TokenReplayOptions options;
  options.include_duration_diagnostics = true;
  options.timestamp_attribute = "finished";
  model.token_replay = minera::check_conformance(log, net, options);
  assert(!model.token_replay.trace_diagnostics);
  const auto json = minera::imported_model_json(model, graph);
  assert(json.find("\"duration_diagnostics\": {\"timestamp_attribute\": \"finished\"") != std::string::npos);
  assert(json.find("\"transition_index\": 1, \"fit_count\": 2, \"problem_count\": 1, \"fit_median_seconds\": 25, \"problem_median_seconds\": 30, \"relative_throughput\": 1.2") != std::string::npos);
  assert(json.find("\"fit_trace_indices\": [0, 2], \"problem_trace_indices\": [1]") != std::string::npos);
  assert(json.find("\"activity\": \"X\", \"activity_is_null\": false, \"fit_count\": 2, \"problem_count\": 1, \"fit_median_seconds\": 20, \"problem_median_seconds\": 40, \"relative_throughput\": 2") != std::string::npos);
  auto& comparison = model.token_replay.duration_diagnostics->transitions.front();
  comparison.fit_median_seconds = 0.12345678912345678;
  comparison.relative_throughput = 8.0 / 3.0;
  const auto precise = minera::imported_model_json(model, graph);
  for (const auto& [key, expected] : {
      std::pair<std::string, double>{"\"fit_median_seconds\": ", comparison.fit_median_seconds},
      {"\"relative_throughput\": ", comparison.relative_throughput}}) {
    const auto position = precise.find(key);
    assert(position != std::string::npos);
    assert(std::stod(precise.substr(position + key.size())) == expected);
  }
  log.traces.clear();
  model.token_replay = minera::check_conformance(log, net, options);
  assert(minera::imported_model_json(model, graph).find("\"transitions\": [], \"unknown_activities\": []") != std::string::npos);
}

void replay_root_cause_json_contract() {
  minera::DirectlyFollowsGraph graph;
  minera::ModelConformance model;
  assert(minera::imported_model_json(model, graph).find("root_cause_diagnostics") == std::string::npos);
  model.token_replay.root_cause_diagnostics.emplace();
  auto& diagnostics = *model.token_replay.root_cause_diagnostics;
  diagnostics.options.numeric_attributes = {"cost", "cost"};
  diagnostics.options.enable_multiplier = true;
  const auto observation = [](double value, std::size_t trace) {
    minera::RootCauseObservation result;
    result.trace_index = trace;
    minera::EventAttribute attribute;
    attribute.key = "cost";
    attribute.kind = minera::AttributeKind::kFloat;
    attribute.typed_value = value;
    result.context.attributes.push_back(attribute);
    return result;
  };
  const double value = 0.12345678912345678;
  const auto trained = minera::fit_root_cause_model(
      {observation(value, 2)}, {observation(std::numeric_limits<double>::quiet_NaN(), 7)},
      diagnostics.options);
  assert(trained && trained->tree.front().missing_only);
  minera::TokenReplayTransitionRootCauseDiagnostics transition;
  static_cast<minera::RootCauseModel&>(transition) = *trained;
  transition.transition_index = 3;
  diagnostics.transitions.push_back(transition);
  minera::TokenReplayUnknownRootCauseDiagnostics unknown;
  static_cast<minera::RootCauseModel&>(unknown) = *trained;
  unknown.classes[1] = "containing";
  unknown.activity = "unknown\"activity";
  unknown.activity_is_null = true;
  diagnostics.unknown_activities.push_back(unknown);
  const auto json = minera::imported_model_json(model, graph);
  assert(json.find("\"root_cause_diagnostics\": {\"string_attributes\": [], \"numeric_attributes\": [\"cost\", \"cost\"], \"enable_multiplier\": true") != std::string::npos);
  assert(json.find("\"transition_index\": 3, \"fit_count\": 1, \"problem_count\": 1") != std::string::npos);
  assert(json.find("\"feature_names\": [\"event:cost\", \"event:cost\"]") != std::string::npos);
  assert(json.find("\"classes\": [\"fit\", \"underfed\"]") != std::string::npos);
  assert(json.find("\"classes\": [\"fit\", \"containing\"]") != std::string::npos);
  assert(json.find("\"target\": [0, 1], \"source_trace_indices\": [2, 7]") != std::string::npos);
  assert(json.find("\"feature_index\": 1, \"threshold\": 0, \"left\": 1, \"right\": 2, \"class_counts\": [1, 1], \"missing_go_to_left\": false, \"missing_only\": true") != std::string::npos);
  assert(json.find("\"activity\": \"unknown\\\"activity\", \"activity_is_null\": true") != std::string::npos);
  const std::string data_prefix = "\"data\": [[0, ";
  const auto start = json.find(data_prefix);
  assert(start != std::string::npos && std::stod(json.substr(start + data_prefix.size())) == value);
  assert(json.find("[0, null]") != std::string::npos);
  assert(json.find("nan") == std::string::npos && json.find("Infinity") == std::string::npos);
  diagnostics.unknown_activities[0].activity_attribute = minera::EventAttribute{
      "concept:name", "1", minera::AttributeKind::kInt};
  diagnostics.unknown_activities[0].activity_attribute->typed_value = std::int64_t{1};
  const auto typed_json = minera::imported_model_json(model, graph);
  assert(typed_json.find("\"activity_attribute\": {\"key\":\"concept:name\",\"value\":\"1\",\"kind\":\"int\",\"container\":\"scalar\",\"null\":false,\"typed_value\":1}") != std::string::npos);
  diagnostics.unknown_activities[0].activity_attribute->container = minera::AttributeContainerKind::kTuple;
  diagnostics.unknown_activities[0].activity_attribute->children = {{"", "x"}};
  const auto tuple_json = minera::imported_model_json(model, graph);
  assert(tuple_json.find("\"container\":\"tuple\"") != std::string::npos);
  assert(tuple_json.find("\"children\":[{\"key\":\"\",\"value\":\"x\"") != std::string::npos);
  for (const auto container : {minera::AttributeContainerKind::kSet, minera::AttributeContainerKind::kFrozenSet}) {
    diagnostics.unknown_activities[0].activity_attribute->container = container;
    const auto set_json = minera::imported_model_json(model, graph);
    const std::string name = container == minera::AttributeContainerKind::kSet ? "set" : "frozenset";
    assert(set_json.find("\"container\":\"" + name + "\"") != std::string::npos);
    assert(set_json.find("\"children\":[{\"key\":\"\",\"value\":\"x\"") != std::string::npos);
  }
  diagnostics.unknown_activities[0].activity_attribute = minera::make_bytes_attribute("task", std::string("\0\xff", 2));
  const auto bytes_json = minera::imported_model_json(model, graph);
  assert(bytes_json.find("\"key\":\"task\",\"value\":\"00ff\",\"kind\":\"bytes\"") != std::string::npos);
  diagnostics.transitions.clear();
  diagnostics.unknown_activities.clear();
  assert(minera::imported_model_json(model, graph).find("\"transitions\": [], \"unknown_activities\": []") != std::string::npos);
}

void replay_null_labels_survive_json() {
  minera::PetriNet net;
  net.set_activities({""});
  net.set_source(net.add_place());
  net.set_sink(net.add_place());
  net.add_transition(0, {0}, {1});
  minera::EventLog log;
  minera::Trace trace;
  minera::Event event;
  event.activity_state = minera::FirstClassAttributeState::kNull;
  trace.events.push_back(event);
  log.traces.push_back(trace);
  log.traces.push_back(trace);
  log.traces.back().events.front().activity_state = minera::FirstClassAttributeState::kPresent;
  minera::TokenReplayOptions options;
  options.include_trace_diagnostics = true;
  minera::ModelConformance model;
  model.token_replay = minera::check_conformance(log, net, options);
  assert(model.token_replay.variants.size() == 2);
  minera::DirectlyFollowsGraph graph;
  graph.ensure_activity("");
  const auto json = minera::imported_model_json(model, graph);
  assert(json.find("\"null_activity_indices\": [0]") != std::string::npos);
  assert(json.find("\"unknown_activity_is_null\": [true]") != std::string::npos);
  assert(json.find("\"activities\": [null]") != std::string::npos);
  assert(json.find("\"activities\": [0]") != std::string::npos);
}
void standalone_replay_json_contract() {
  minera::TokenReplayTraceDiagnostics trace;
  trace.case_id = "case\"quoted";
  trace.trace_fitness = 1.0 / 3.0;
  trace.reached_marking = {0, 2, 0};
  trace.reached_marking_keys = {true, true, false};
  trace.activated_transitions = {2, 0, 2};
  const auto json = minera::token_replay_traces_json({trace});
  assert(json.find("\"family\":\"TOKEN_REPLAY\"") != std::string::npos);
  assert(json.find("\"variant\":\"forward\"") != std::string::npos);
  assert(json.find("\"trace_fitness\": 0.33333333333333331") != std::string::npos);
  assert(json.find("\"reached_marking_keys\": [true, true, false]") != std::string::npos);
  assert(json.find("\"reached_marking\": [0, 2, 0]") != std::string::npos);
  assert(json.find("\"activated_transitions\": [2, 0, 2]") != std::string::npos);
  assert(json.find("case\\\"quoted") != std::string::npos);
  trace.reached_marking_keys.clear();
  const auto backwards = minera::token_replay_traces_json(
      {trace}, minera::TokenReplayVariant::kBackwards);
  assert(backwards.find("\"variant\":\"backwards\"") != std::string::npos);
  assert(backwards.find("reached_marking_keys") == std::string::npos);
  assert(minera::token_replay_traces_json({}).find("\"traces\":[]") != std::string::npos);
  bool rejected = false;
  try { (void)minera::token_replay_traces_json({}, static_cast<minera::TokenReplayVariant>(-1)); }
  catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected);
  trace.reached_marking_keys = {true};
  rejected = false;
  try { (void)minera::token_replay_traces_json({trace}); }
  catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected);
}
}  // namespace

int main() {
  replay_policy_request_contract();
  replay_diagnostics_json_contract();
  replay_duration_json_contract();
  replay_root_cause_json_contract();
  replay_null_labels_survive_json();
  standalone_replay_json_contract();
}
