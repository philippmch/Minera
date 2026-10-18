#include "minera/petri_analysis_workbench_json.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/footprints_workbench_json.hpp"
#include "minera/literal_controls_json.hpp"
#include "minera/object_centric_playout_workbench_json.hpp"
#include "minera/petri_analysis_json.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace minera;
namespace {
// Inspect complete public JSON output independently of the production walker.
// Numeric tokens stay strings: these assertions never round size_t/int64.
std::map<std::string, std::string> fields(const std::string& text) {
  assert(text.size() >= 2 && text.front() == '{' && text.back() == '}');
  std::map<std::string, std::string> result; std::size_t at = 1;
  while (at + 1 < text.size()) {
    assert(text[at++] == '"'); const auto begin = at;
    while (text[at] != '"') ++at;
    const auto key = text.substr(begin, at - begin); assert(text[++at] == ':'); ++at;
    const auto value = at; int depth = 0; bool string = false;
    for (; at < text.size(); ++at) {
      const auto c = text[at];
      if (string) { if (c == '\\') ++at; else if (c == '"') string = false; continue; }
      if (c == '"') string = true;
      else if (c == '{' || c == '[') ++depth;
      else if (c == '}' || c == ']') { if (depth == 0) break; --depth; }
      else if (c == ',' && depth == 0) break;
    }
    assert(result.emplace(key, text.substr(value, at - value)).second);
    if (text[at] == ',') ++at;
  }
  return result;
}
void expected(const std::string& text, const std::map<std::string, std::string>& values) { assert(fields(text) == values); }
std::string word(const std::string& bytes) { return serialize_embedding_workbench_string_json(bytes); }
template<class F> void rejects(F function) { bool thrown = false; try { function(); } catch (const std::invalid_argument&) { thrown = true; } assert(thrown); }
std::string bytes(const std::string& artifact) {
  const auto data = fields(artifact); const auto text = parse_literal_string_vector_json("[" + data.at("data") + "]").front();
  std::string result;
  if (data.at("encoding") == "\"utf8\"") result = text;
  else {
    assert(data.at("encoding") == "\"hex\"" && text.size() % 2 == 0);
    for (std::size_t i = 0; i < text.size(); i += 2) result += static_cast<char>(std::stoul(text.substr(i, 2), nullptr, 16));
  }
  assert(data.at("bytes") == std::to_string(result.size()));
  assert(data.at("sha256") == word(footprints_workbench_sha256(result)));
  return result;
}
PetriNet chain() {
  PetriNet result; result.set_activities({"A", "B"});
  const auto p0 = result.add_place(), p1 = result.add_place(), p2 = result.add_place();
  result.set_source(p0); result.set_sink(p2);
  result.add_transition(0, {p0}, {p1}); result.add_transition(1, {p1}, {p2}); return result;
}
std::vector<DiscoveryWorkbenchPart> request(const std::string& operation, const PetriNet& net) {
  return {{"operation", "", operation, false}, {"model_format", "", "exact", false},
      {"model_file", "net.json", serialize_oc_playout_workbench_petri_net_json(net), true}};
}
std::map<std::string, std::string> successful(const PetriAnalysisWorkbenchResponse& response) {
  assert(response.status == 200); const auto value = fields(response.json);
  assert(value.size() == 20 && value.at("error") == "null" && value.at("executed") == "true");
  assert(value.at("native_entered") == "true" && value.at("native_returned") == "true");
  const auto model = fields(value.at("model")); assert(model.at("before") == model.at("after"));
  assert(fields(value.at("input_context")).at("model_unchanged") == "true");
  const auto artifacts = fields(value.at("artifact_bytes")); assert(artifacts.size() == 6);
  assert(bytes(artifacts.at("source_net_json")) == model.at("before"));
  assert(bytes(artifacts.at("after_net_json")) == model.at("after"));
  assert(bytes(artifacts.at("result_json")) == value.at("result")); return value;
}
void options_storage() {
  const auto max = std::to_string(std::numeric_limits<std::size_t>::max());
  const auto min64 = std::to_string(std::numeric_limits<std::int64_t>::min()), max64 = std::to_string(std::numeric_limits<std::int64_t>::max());
  std::set<std::string> supplied{"retained"};
  auto marking = parse_petri_analysis_workbench_marking_options_json("{}", &supplied);
  assert(supplied.empty() && marking.initial_marking.empty() && marking.final_marking.empty() && marking.transition_costs.empty() && marking.realizability_limit == 200000);
  const auto mark_text = "{\"initial_marking\":[" + min64 + "," + max64 + "],\"final_marking\":[-1,0],\"transition_costs\":[-0.0,\"NaN\",\"INF\",\"-INF\"],\"realizability_limit\":" + max + '}';
  marking = parse_petri_analysis_workbench_marking_options_json(mark_text, &supplied);
  assert(supplied == std::set<std::string>({"initial_marking", "final_marking", "transition_costs", "realizability_limit"}));
  assert(marking.initial_marking[0] == std::numeric_limits<std::int64_t>::min() && marking.initial_marking[1] == std::numeric_limits<std::int64_t>::max());
  assert(marking.final_marking == Marking({-1, 0}) && marking.realizability_limit == std::numeric_limits<std::size_t>::max());
  assert(std::signbit(marking.transition_costs[0]) && std::isnan(marking.transition_costs[1]) && std::isinf(marking.transition_costs[2]) && marking.transition_costs[3] < 0);
  const auto mark_round = parse_petri_analysis_workbench_marking_options_json(serialize_petri_analysis_workbench_options_json(marking));
  assert(mark_round.initial_marking == marking.initial_marking && std::signbit(mark_round.transition_costs[0]) && std::isnan(mark_round.transition_costs[1]));
  marking.transition_costs = {std::nextafter(1.0, 2.0), std::numeric_limits<double>::max(), std::numeric_limits<double>::min()};
  assert(parse_petri_analysis_workbench_marking_options_json(serialize_petri_analysis_workbench_options_json(marking)).transition_costs == marking.transition_costs);
  auto extended = parse_petri_analysis_workbench_extended_options_json("{}", &supplied);
  assert(supplied.empty() && extended.max_k_value == 5 && extended.full_bootstrap_required && extended.realizability_limit == 200000);
  const auto ext_text = "{\"initial_marking\":[-1],\"final_marking\":[2],\"model_transition_costs\":[-0.0,\"INF\"],\"synchronous_transition_costs\":[\"NaN\"],\"log_move_costs\":[\"-INF\"],\"split_indices\":[" + max + ",0,0],\"max_k_value\":0,\"full_bootstrap_required\":false,\"realizability_limit\":0}";
  extended = parse_petri_analysis_workbench_extended_options_json(ext_text, &supplied);
  assert(supplied.size() == 9 && extended.initial_marking == Marking({-1}) && extended.final_marking == Marking({2}));
  assert(std::signbit(extended.model_transition_costs[0]) && std::isinf(extended.model_transition_costs[1]) && std::isnan(extended.synchronous_transition_costs[0]) && extended.log_move_costs[0] < 0);
  assert(extended.split_indices == std::vector<std::size_t>({std::numeric_limits<std::size_t>::max(), 0, 0}) && extended.max_k_value == 0 && !extended.full_bootstrap_required && extended.realizability_limit == 0);
  assert(serialize_petri_analysis_workbench_options_json(parse_petri_analysis_workbench_extended_options_json(serialize_petri_analysis_workbench_options_json(extended))) == serialize_petri_analysis_workbench_options_json(extended));
  auto soundness = parse_petri_analysis_workbench_soundness_options_json("{}", &supplied);
  assert(supplied.empty() && soundness.variant == SoundnessVariant::kSoundness && soundness.max_states == 200000 && soundness.invariant_search_limit == 100000 && !soundness.diagnostics && !soundness.return_asap_when_unsound);
  const auto sound_text = "{\"variant\":{\"name\":\"woflan\",\"index\":1},\"initial_marking\":[-1],\"final_marking\":[-2],\"max_states\":" + max + ",\"invariant_search_limit\":0,\"diagnostics\":true,\"return_asap_when_unsound\":true}";
  soundness = parse_petri_analysis_workbench_soundness_options_json(sound_text, &supplied);
  assert(supplied.size() == 7 && soundness.variant == SoundnessVariant::kWoflan && soundness.initial_marking == Marking({-1}) && soundness.final_marking == Marking({-2}));
  assert(soundness.max_states == std::numeric_limits<std::size_t>::max() && soundness.invariant_search_limit == 0 && soundness.diagnostics && soundness.return_asap_when_unsound);
  assert(serialize_petri_analysis_workbench_options_json(parse_petri_analysis_workbench_soundness_options_json(serialize_petri_analysis_workbench_options_json(soundness))) == serialize_petri_analysis_workbench_options_json(soundness));
  for (const auto* name : {"soundness", "woflan"}) { const auto scalar = parse_petri_analysis_workbench_soundness_options_json("{\"variant\":\"" + std::string(name) + "\"}"); assert(serialize_petri_analysis_workbench_options_json(parse_petri_analysis_workbench_soundness_options_json(serialize_petri_analysis_workbench_options_json(scalar))) == serialize_petri_analysis_workbench_options_json(scalar)); }
  for (const auto index : {std::numeric_limits<std::int32_t>::min(), 2, std::numeric_limits<std::int32_t>::max()}) {
    const auto text = "{\"variant\":{\"name\":null,\"index\":" + std::to_string(index) + "}}";
    const auto value = parse_petri_analysis_workbench_soundness_options_json(text);
    assert(static_cast<std::int32_t>(value.variant) == index);
    assert(serialize_petri_analysis_workbench_options_json(parse_petri_analysis_workbench_soundness_options_json(serialize_petri_analysis_workbench_options_json(value))) == serialize_petri_analysis_workbench_options_json(value));
    assert(parse_petri_analysis_workbench_soundness_options_json("{\"variant_index\":" + std::to_string(index) + "}").variant == value.variant);
  }
  for (const auto* bad : {"null", "[]", "{\"variant\":null}", "{\"variant\":\"SOUNDNESS\"}", "{\"variant_index\":2147483648}", "{\"variant\":null,\"variant_index\":0}", "{\"variant\":{\"name\":\"soundness\",\"index\":1}}", "{\"variant\":{\"name\":null,\"index\":0}}", "{\"variant\":{\"name\":\"future\",\"index\":2}}", "{\"variant\":{\"name\":null,\"index\":2,\"extra\":0}}", "{\"variant\":{\"index\":2}}", "{\"diagnostics\":null}", "{\"max_states\":-0}", "{\"max_states\":1.0}", "{\"max_states\":1e2}", "{\"max_states\":18446744073709551616}", "{\"max_states\":1,\"max_states\":2}", "{\"unknown\":0}", "{\"initial_marking\":[9223372036854775808]}"}) rejects([&] { (void)parse_petri_analysis_workbench_soundness_options_json(bad); });
  supplied = {"unchanged"}; rejects([&] { (void)parse_petri_analysis_workbench_marking_options_json("{\"transition_costs\":[\"not-a-number\"]}", &supplied); }); assert(supplied == std::set<std::string>{"unchanged"});
  rejects([&] { (void)parse_petri_analysis_workbench_marking_options_json("{\"initial_marking\":[-9223372036854775809]}"); });
  rejects([&] { (void)parse_petri_analysis_workbench_extended_options_json("{\"split_indices\":[-1]}"); });
}

void complete_result_storage() {
  const auto limit = std::numeric_limits<std::size_t>::max(); const auto max = std::to_string(limit);
  const auto inf = std::numeric_limits<double>::infinity(), nan = std::numeric_limits<double>::quiet_NaN();
  const std::string byte = std::string("\xff\0\xed\xa0\x80\xed\xb0\x80", 8), tag = word(byte);
  const auto unknown = "{\"name\":null,\"index\":77}";
  WorkflowMarkings markings{{std::numeric_limits<std::int64_t>::min(), -1}, {0, std::numeric_limits<std::int64_t>::max()}};
  const auto min64 = std::to_string(std::numeric_limits<std::int64_t>::min()), max64 = std::to_string(std::numeric_limits<std::int64_t>::max());
  expected(serialize_petri_analysis_workbench_result_json(markings), {{"initial", "[" + min64 + ",-1]"}, {"final", "[0," + max64 + "]"}});
  assert(serialize_petri_analysis_workbench_result_json(false) == "false" && serialize_petri_analysis_workbench_result_json(true) == "true");
  MarkingEquationResult me; me.variant = static_cast<MarkingEquationVariant>(77); me.computed = true; me.feasible = true; me.exhausted = true; me.solver_nodes = limit; me.objective = -0.0; me.lower_bound = inf; me.transition_counts = {nan, inf, -inf, -0.0}; me.residual = {-1.0}; me.activated_transitions = {limit, 0, 0}; me.firing_sequence = {limit}; me.recovery_attempted = true; me.reaches_final = true; me.error = byte;
  expected(serialize_petri_analysis_workbench_result_json(me), {{"variant", unknown}, {"computed", "true"}, {"feasible", "true"}, {"exhausted", "true"}, {"solver_nodes", max}, {"objective", "-0.0"}, {"lower_bound", "\"INF\""}, {"transition_counts", "[\"NaN\",\"INF\",\"-INF\",-0.0]"}, {"residual", "[-1.0]"}, {"activated_transitions", "[" + max + ",0,0]"}, {"firing_sequence", "[" + max + "]"}, {"recovery_attempted", "true"}, {"reaches_final", "true"}, {"error", tag}});
  rejects([&] { (void)petri_analysis_json(me); });
  ExtendedMarkingEquationResult ex; ex.variant = static_cast<ExtendedMarkingEquationVariant>(77); ex.computed = true; ex.feasible = true; ex.exhausted = true; ex.solver_nodes = limit; ex.objective = -0.0; ex.lower_bound = -inf; ex.retained_splits = {limit, 0, 0}; ex.segment_count = limit; ex.transition_counts = {nan}; ex.firing_sequence = {limit}; ex.reaches_final = true; ex.error = byte;
  expected(serialize_petri_analysis_workbench_result_json(ex), {{"variant", unknown}, {"computed", "true"}, {"feasible", "true"}, {"exhausted", "true"}, {"solver_nodes", max}, {"objective", "-0.0"}, {"lower_bound", "\"-INF\""}, {"retained_splits", "[" + max + ",0,0]"}, {"segment_count", max}, {"transition_counts", "[\"NaN\"]"}, {"firing_sequence", "[" + max + "]"}, {"reaches_final", "true"}, {"error", tag}});
  WorkflowNetResult workflow; workflow.variant = static_cast<WorkflowNetVariant>(77); workflow.valid = true; workflow.source_candidates = {limit, 0}; workflow.sink_candidates = {2}; workflow.offending_places = {3}; workflow.offending_transitions = {4}; workflow.unreachable_places = {5}; workflow.unreachable_transitions = {6}; workflow.reason = byte;
  const auto workflow_json = serialize_petri_analysis_workbench_result_json(workflow);
  expected(workflow_json, {{"variant", unknown}, {"valid", "true"}, {"source_candidates", "[" + max + ",0]"}, {"sink_candidates", "[2]"}, {"offending_places", "[3]"}, {"offending_transitions", "[4]"}, {"unreachable_places", "[5]"}, {"unreachable_transitions", "[6]"}, {"reason", tag}});
  SoundnessResult sound; sound.variant = static_cast<SoundnessVariant>(77); sound.status = static_cast<SoundnessStatus>(77); sound.computed = true; sound.exhausted = true; sound.returned_asap = true; sound.workflow = workflow; sound.bounded = true; sound.final_reachable = true; sound.option_to_complete = true; sound.proper_completion = true;
  sound.dead_transitions = {limit}; sound.dead_tasks = {2}; sound.deadlocks = {{-1}}; sound.deadlock_witnesses = {{3}}; sound.improper_markings = {{-2}}; sound.improper_witnesses = {{4}}; sound.completion_witness = {5}; sound.locking_witness = {6}; sound.locking_scenarios = {{}, {limit}}; sound.unbounded_witness = {7}; sound.reachable_marking_count = limit; sound.coverability_node_count = limit; sound.diagnostic_messages = {byte}; sound.place_invariant_basis = {{nan, -0.0}}; sound.uncovered_places = {8}; sound.uniform_place_invariants = {{inf}}; sound.uncovered_places_uniform = {9}; sound.uniform_invariant_search_exhausted = true; sound.weighted_place_invariants = {{-inf}}; sound.uncovered_places_weighted = {10}; sound.uncovered_places_s_component = {11}; sound.left = false;
  sound.coverability_markings = {{-1, std::numeric_limits<std::int64_t>::min()}}; sound.coverability_edges = {{limit, 0, 1}}; sound.restricted_coverability_tree_exhausted = true; sound.restricted_coverability_markings = {{-3}}; sound.restricted_coverability_edges = {{12}}; sound.reachability_markings = {{std::numeric_limits<std::int64_t>::max()}}; sound.reachability_edges = {{13}}; sound.s_components = std::vector<std::vector<std::size_t>>{}; sound.handled_pairs = std::vector<std::vector<std::size_t>>{{}}; sound.not_well_handled_pairs = std::vector<std::vector<std::size_t>>{{limit}}; sound.structural_node_labels = {byte}; sound.short_circuit_valid = true; sound.short_circuit_strongly_connected = true; sound.short_circuit_transition = 0; sound.short_circuit_diagnostic_messages = {byte}; sound.short_circuit_reachability_exhausted = true; sound.short_circuit_reachability_strongly_connected = true; sound.short_circuit_reachability_markings = {{-4}}; sound.short_circuit_reachability_edges = {{14}}; sound.error = byte;
  expected(serialize_petri_analysis_workbench_result_json(sound), {{"variant", unknown}, {"status", unknown}, {"computed", "true"}, {"exhausted", "true"}, {"returned_asap", "true"}, {"workflow", workflow_json}, {"bounded", "true"}, {"final_reachable", "true"}, {"option_to_complete", "true"}, {"proper_completion", "true"}, {"dead_transitions", "[" + max + "]"}, {"dead_tasks", "[2]"}, {"deadlocks", "[[-1]]"}, {"deadlock_witnesses", "[[3]]"}, {"improper_markings", "[[-2]]"}, {"improper_witnesses", "[[4]]"}, {"completion_witness", "[5]"}, {"locking_witness", "[6]"}, {"locking_scenarios", "[[],[" + max + "]]"}, {"unbounded_witness", "[7]"}, {"reachable_marking_count", max}, {"coverability_node_count", max}, {"diagnostic_messages", "[" + tag + "]"}, {"place_invariant_basis", "[[\"NaN\",-0.0]]"}, {"uncovered_places", "[8]"}, {"uniform_place_invariants", "[[\"INF\"]]"}, {"uncovered_places_uniform", "[9]"}, {"uniform_invariant_search_exhausted", "true"}, {"weighted_place_invariants", "[[\"-INF\"]]"}, {"uncovered_places_weighted", "[10]"}, {"uncovered_places_s_component", "[11]"}, {"left", "false"}, {"coverability_markings", "[[-1," + min64 + "]]"}, {"coverability_edges", "[[" + max + ",0,1]]"}, {"restricted_coverability_tree_exhausted", "true"}, {"restricted_coverability_markings", "[[-3]]"}, {"restricted_coverability_edges", "[[12]]"}, {"reachability_markings", "[[" + max64 + "]]"}, {"reachability_edges", "[[13]]"}, {"s_components", "[]"}, {"handled_pairs", "[[]]"}, {"not_well_handled_pairs", "[[" + max + "]]"}, {"structural_node_labels", "[" + tag + "]"}, {"short_circuit_valid", "true"}, {"short_circuit_strongly_connected", "true"}, {"short_circuit_transition", "0"}, {"short_circuit_diagnostic_messages", "[" + tag + "]"}, {"short_circuit_reachability_exhausted", "true"}, {"short_circuit_reachability_strongly_connected", "true"}, {"short_circuit_reachability_markings", "[[-4]]"}, {"short_circuit_reachability_edges", "[[14]]"}, {"error", tag}});
  sound.left.reset(); sound.s_components.reset(); sound.handled_pairs.reset(); sound.not_well_handled_pairs.reset(); sound.short_circuit_transition.reset();
  const auto none = fields(serialize_petri_analysis_workbench_result_json(sound));
  for (const auto* key : {"left", "s_components", "handled_pairs", "not_well_handled_pairs", "short_circuit_transition"}) assert(none.at(key) == "null");
  SoundnessResult huge; huge.error.assign(6u * 1024u * 1024u, '\0');
  const auto large = serialize_petri_analysis_workbench_result_json(huge);
  assert(large.size() > 32u * 1024u * 1024u && large.ends_with("\"}"));
}

void all_public_calls_and_native_paths() {
  const auto net = chain(); const auto canonical = serialize_oc_playout_workbench_petri_net_json(net);
  const auto defaults = successful(execute_petri_analysis_workbench({{"model_file", "default.json", canonical, true}}));
  assert(defaults.at("operation") == "\"analyze_soundness\"" && defaults.at("submitted_options") == "null" && defaults.at("supplied_options") == "[]");
  for (const auto* operation : {"default_workflow_markings", "solve_marking_equation", "solve_extended_marking_equation", "check_workflow_net", "is_workflow_net", "analyze_soundness"}) {
    auto parts = request(operation, net);
    if (std::string(operation) == "solve_extended_marking_equation") parts.push_back({"trace_file", "trace.json", "[\"A\",\"B\"]", true});
    const auto response = execute_petri_analysis_workbench(parts); const auto value = successful(response);
    assert(fields(value.at("model")).at("before") == canonical);
    const auto result = fields(value.at("result"));
    if (std::string(operation) == "default_workflow_markings") assert(result.at("value") == serialize_petri_analysis_workbench_result_json(default_workflow_markings(net)));
    if (std::string(operation) == "solve_marking_equation") assert(result.at("value") == serialize_petri_analysis_workbench_result_json(solve_marking_equation(net)));
    if (std::string(operation) == "solve_extended_marking_equation") assert(result.at("value") == serialize_petri_analysis_workbench_result_json(solve_extended_marking_equation({"A", "B"}, net)));
    if (std::string(operation) == "check_workflow_net") assert(result.at("value") == serialize_petri_analysis_workbench_result_json(check_workflow_net(net)));
    if (std::string(operation) == "is_workflow_net") assert(result.at("value") == "true");
    if (std::string(operation) == "analyze_soundness") assert(result.at("value") == serialize_petri_analysis_workbench_result_json(analyze_soundness(net)));
  }
  auto woflan = request("analyze_soundness", net); woflan.push_back({"options_json", "", "{\"variant\":\"woflan\",\"diagnostics\":true}", false});
  const auto woflan_value = successful(execute_petri_analysis_workbench(woflan));
  assert(fields(fields(woflan_value.at("result")).at("value")).at("variant") == "{\"name\":\"woflan\",\"index\":1}");
  const auto options_artifact = bytes(fields(woflan_value.at("artifact_bytes")).at("configured_options_json"));
  woflan.back().bytes = options_artifact;
  const auto reopened = successful(execute_petri_analysis_workbench(woflan));
  assert(reopened.at("result") == woflan_value.at("result"));
  auto exhausted = request("analyze_soundness", net); exhausted.push_back({"options_json", "", "{\"max_states\":1,\"diagnostics\":true}", false});
  const auto exhausted_value = fields(fields(successful(execute_petri_analysis_workbench(exhausted)).at("result")).at("value"));
  assert(exhausted_value.at("computed") == "false" && exhausted_value.at("exhausted") == "true" && exhausted_value.at("status") == "{\"name\":\"unknown\",\"index\":2}");
  auto extended_storage = PetriNetLiteralStorage{}; extended_storage.place_count = 2; extended_storage.has_source = extended_storage.has_sink = true; extended_storage.sink_storage = 1; extended_storage.activities = {"A"};
  PetriTransition transition; transition.activity = 0; transition.inputs = {0}; transition.outputs = {1}; transition.reset_inputs = {0}; extended_storage.transitions = {transition};
  const auto extended_net = PetriNet::from_literal_storage(std::move(extended_storage));
  for (const auto* operation : {"solve_marking_equation", "solve_extended_marking_equation"}) {
    auto parts = request(operation, extended_net); parts.push_back({"options_json", "", "{\"initial_marking\":[-1],\"realizability_limit\":0}", false});
    if (std::string(operation) == "solve_extended_marking_equation") parts.push_back({"trace_file", "", "[]", true});
    const auto value = successful(execute_petri_analysis_workbench(parts));
    assert(fields(fields(value.at("result")).at("value")).at("computed") == "false");
    if (std::string(operation) == "solve_extended_marking_equation") {
      assert(fields(value.at("artifact_bytes")).at("legacy_json") == "null");
      assert(value.at("artifact_errors").find("extended marking equation splits are inconsistent") != std::string::npos);
      assert(value.at("native_calls").find("\"stage\":\"export\"") != std::string::npos);
    }
  }
  auto invalid_marking = request("solve_marking_equation", net); invalid_marking.push_back({"options_json", "", "{\"initial_marking\":[-1]}", false});
  const auto thrown = fields(execute_petri_analysis_workbench(invalid_marking).json);
  assert(thrown.at("native_entered") == "true" && thrown.at("native_returned") == "false" && thrown.at("result") == "null");
  assert(fields(thrown.at("error")).at("phase") == "\"native_error\"");
  assert(fields(thrown.at("model")).at("before") == fields(thrown.at("model")).at("after"));
  auto zero = request("analyze_soundness", PetriNet::from_literal_storage({})); zero.push_back({"options_json", "", "{\"max_states\":0,\"invariant_search_limit\":0}", false});
  assert(fields(fields(successful(execute_petri_analysis_workbench(zero)).at("result")).at("value")).at("computed") == "true");
  auto false_helper = request("is_workflow_net", PetriNet::from_literal_storage({}));
  assert(fields(successful(execute_petri_analysis_workbench(false_helper)).at("result")).at("value") == "false");
  auto no_places = request("default_workflow_markings", PetriNet::from_literal_storage({}));
  assert(execute_petri_analysis_workbench(no_places).status == 400);
}

void full_constructor_bytes_and_transport() {
  std::string all; for (unsigned i = 0; i < 256; ++i) all += static_cast<char>(i);
  const std::string six("\xed\xa0\x80\xed\xb0\x80", 6), scalar("\xf0\x90\x80\x80", 4);
  PetriNetLiteralStorage storage; storage.place_count = 0; storage.source_storage = std::numeric_limits<std::size_t>::max(); storage.sink_storage = storage.source_storage; storage.activities = {all, six, scalar}; storage.transition_names = PetriTransitionNameList{std::nullopt, "", all};
  PetriTransition transition; transition.activity = storage.source_storage; transition.inputs = {storage.source_storage, 0, 0}; transition.outputs = {1}; transition.reset_inputs = {2}; transition.inhibitor_inputs = {3}; transition.input_weights = {{TokenInteger("-2"), true}}; transition.output_weights = {{TokenInteger("999999999999999999999999999999999"), false}}; transition.reset_weights = {{TokenInteger("0"), true}}; transition.inhibitor_weights = {{TokenInteger("3"), false}}; storage.transitions = {transition};
  HeuristicStructuredValue nested; nested.value = HeuristicStructuredValue::Object{{all, HeuristicStructuredValue{HeuristicStructuredValue::Array{HeuristicStructuredValue{six}, HeuristicStructuredValue{scalar}}}}};
  PetriDiscoveryMarkings discovery; discovery.detached_places = {{17, nested}, {17, HeuristicStructuredValue{all}}}; discovery.initial = {{{static_cast<PetriMarkingPlaceKind>(77), 17}, TokenInteger("-99999999999999999999999999999")}}; discovery.final = {{{PetriMarkingPlaceKind::kDetached, 17}, TokenInteger("0")}}; storage.discovery_markings = discovery;
  const auto literal = PetriNet::from_literal_storage(storage); const auto encoded = serialize_oc_playout_workbench_petri_net_json(literal);
  const auto parsed = parse_oc_playout_workbench_petri_net_json(encoded);
  assert(serialize_oc_playout_workbench_petri_net_json(parsed) == encoded && parsed.activities()[0] == all && parsed.activities()[1] == six && parsed.activities()[2] == scalar && six != scalar);
  assert(parsed.source() == storage.source_storage && parsed.sink() == storage.sink_storage && !parsed.has_source() && !parsed.has_sink());
  auto input = request("check_workflow_net", literal); input[2].filename = six;
  const auto value = successful(execute_petri_analysis_workbench(input)); assert(fields(value.at("model")).at("before") == encoded);
  const auto normal = chain(); auto byte_net_storage = PetriNetLiteralStorage{}; byte_net_storage.place_count = 2; byte_net_storage.has_source = byte_net_storage.has_sink = true; byte_net_storage.sink_storage = 1; byte_net_storage.activities = {all}; PetriTransition t; t.activity = 0; t.inputs = {0}; t.outputs = {1}; byte_net_storage.transitions = {t};
  const auto byte_net = PetriNet::from_literal_storage(byte_net_storage);
  const auto byte_soundness = successful(execute_petri_analysis_workbench(request("analyze_soundness", byte_net)));
  const auto byte_soundness_result = fields(fields(byte_soundness.at("result")).at("value"));
  const auto labels = parse_literal_string_vector_json(byte_soundness_result.at("structural_node_labels"));
  bool byte_label_found = false; for (const auto& label : labels) if (label.find(all) != std::string::npos) byte_label_found = true; assert(byte_label_found);
  const auto legacy_bytes = fields(byte_soundness.at("artifact_bytes")).at("legacy_json");
  assert(legacy_bytes != "null" && fields(legacy_bytes).at("encoding") == "\"hex\"" && !bytes(legacy_bytes).empty() && byte_soundness.at("artifact_errors") == "[]");
  auto byte_words = request("solve_extended_marking_equation", byte_net); byte_words.push_back({"trace_file", all, "[" + word(all) + "," + word(six) + "," + word(scalar) + "]", true});
  const auto trace_value = successful(execute_petri_analysis_workbench(byte_words));
  assert(fields(trace_value.at("trace")).at("before") == fields(trace_value.at("trace")).at("after"));
  assert(parse_literal_string_vector_json(bytes(fields(trace_value.at("artifact_bytes")).at("trace_json"))) == std::vector<std::string>({all, six, scalar}));
  const std::string escaped_lone_words = R"(["\ud800","\udc00","\u0000"])";
  byte_words.back().bytes = escaped_lone_words;
  const auto lone = successful(execute_petri_analysis_workbench(byte_words));
  assert(parse_literal_string_vector_json(fields(lone.at("trace")).at("before")) == parse_literal_string_vector_json(escaped_lone_words));
  auto bad = request("solve_marking_equation", normal); bad.push_back({"options_json", "", "{\"transition_costs\":[\"NaN\",-0.0]}", false});
  assert(execute_petri_analysis_workbench(bad).status == 400);
  assert(fields(fields(execute_petri_analysis_workbench(bad).json).at("error")).at("phase") == "\"native_error\"");
  auto physical = request("check_workflow_net", normal);
  std::string body = "--p73\r\nContent-Disposition: form-data; name=\"operation\"\r\n\r\ncheck_workflow_net\r\n--p73\r\nContent-Disposition: form-data; name=\"model_file\"; filename=\"\"\r\n\r\n" + physical[2].bytes + "\r\n--p73--\r\n";
  const auto multipart = execute_petri_analysis_workbench_multipart("multipart/form-data; boundary=p73", body);
  assert(successful(multipart).at("originals").find("\"filename\":\"\",\"file\":true") != std::string::npos);
  assert(execute_petri_analysis_workbench_multipart("multipart/form-data; boundary=p73", body, {{"operation", "", "is_workflow_net", false}}).status == 400);
  const auto incomplete = fields(execute_petri_analysis_workbench_multipart("multipart/form-data; boundary=p73", body, {}, false).json);
  assert(incomplete.at("originals") == "[]" && incomplete.at("configured_options") == "null" && incomplete.at("native_entered") == "false");
  assert(execute_petri_analysis_workbench_multipart("application/json", "{}").status == 400);
  for (const auto* operation : {"default_workflow_markings", "check_workflow_net", "is_workflow_net"}) { auto parts = request(operation, normal); parts.push_back({"options_json", "", "{}", false}); assert(execute_petri_analysis_workbench(parts).status == 400); }
  auto missing = request("solve_extended_marking_equation", normal); assert(execute_petri_analysis_workbench(missing).status == 400);
  auto wrong_trace = request("check_workflow_net", normal); wrong_trace.push_back({"trace_file", "", "[]", true}); assert(execute_petri_analysis_workbench(wrong_trace).status == 400);
  auto unknown = request("check_workflow_net", normal); unknown.push_back({all, "", "x", false});
  const auto unknown_value = fields(execute_petri_analysis_workbench(unknown).json);
  // exception.what() exposes a C string: this name begins with NUL, so the
  // displayed message ends at that NUL; the physical field remains all256bytes.
  const std::string cstring_prefix = "petri analysis workbench JSON: unknown control or upload: ";
  assert(unknown_value.at("originals").find("\"field\":" + word(all) + ",\"filename\":null,\"file\":false") != std::string::npos);
  assert(fields(unknown_value.at("error")).at("message") == word(cstring_prefix));
  const std::string invalid_before_nul("\xff\0tail", 6);
  auto byte_unknown = request("check_workflow_net", normal); byte_unknown.push_back({invalid_before_nul, "", "x", false});
  const auto byte_error = fields(execute_petri_analysis_workbench(byte_unknown).json);
  assert(byte_error.at("originals").find("\"field\":" + word(invalid_before_nul) + ",\"filename\":null,\"file\":false") != std::string::npos);
  assert(fields(byte_error.at("error")).at("message") == word(cstring_prefix + std::string(1, static_cast<char>(0xff))));
}
}  // namespace
int main() { options_storage(); complete_result_storage(); all_public_calls_and_native_paths(); full_constructor_bytes_and_transport(); }
