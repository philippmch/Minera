#include "minera/conformance.hpp"
#include "minera/evaluation.hpp"
#include "minera/ilp_miner.hpp"
#include "minera/json_export.hpp"
#include "minera/petri_analysis.hpp"
#include "minera/pnml.hpp"
#include "minera/quality_workbench_json.hpp"
#include "minera/playout.hpp"
#include "../tools/pnml_model_options.hpp"

#include <cassert>
#include <initializer_list>
#include <sstream>
#include <limits>
#include <string>
#include <utility>

using namespace minera;

namespace {

EventLog log_of(std::initializer_list<std::initializer_list<const char*>> traces) {
  EventLog log;
  for (const auto& values : traces) {
    Trace trace;
    trace.case_id = "case_" + std::to_string(log.traces.size() + 1);
    for (const char* value : values) {
      Event event;
      event.case_id = trace.case_id;
      event.activity = value;
      event.input_order = trace.events.size();
      trace.events.push_back(std::move(event));
    }
    log.traces.push_back(std::move(trace));
  }
  return log;
}

PetriNet reset_net() {
  PetriNet net;
  net.set_activities({"a", "b"});
  const auto source = net.add_place();
  const auto work = net.add_place();
  const auto debris = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.add_transition(0, {source}, {work});
  net.add_transition(std::nullopt, {work}, {work, debris});
  const auto finish = net.add_transition(1, {work}, {sink});
  net.add_reset_arc(finish, debris);
  return net;
}

PetriNet inhibitor_net() {
  PetriNet net;
  net.set_activities({"a", "b", "c"});
  const auto source = net.add_place();
  const auto gate = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.add_transition(0, {source}, {gate});
  const auto blocked = net.add_transition(1, {source}, {sink});
  net.add_inhibitor_arc(blocked, gate);
  net.add_transition(2, {gate}, {sink});
  return net;
}

void reset_and_inhibitor_enablement() {
  const auto reset = reset_net();
  assert(reset.has_extended_arcs());
  const auto fitting = check_conformance(log_of({{"a", "b"}}), reset);
  assert(fitting.fitting_traces == 1 && fitting.missing == 0 &&
         fitting.remaining == 0);

  const auto inhibitor = inhibitor_net();
  const auto fitting_inhibitor =
      check_conformance(log_of({{"a", "c"}}), inhibitor);
  assert(fitting_inhibitor.fitting_traces == 1 && fitting_inhibitor.missing == 0);
  const auto blocked = check_conformance(log_of({{"a", "b"}}), inhibitor);
  assert(blocked.fitting_traces == 0 && blocked.missing > 0);
}

void playout_and_pnml_round_trip() {
  const auto net = reset_net();
  StochasticPlayoutOptions options;
  options.traces = 1;
  options.max_steps = 8;
  const auto generated = stochastic_playout(net, options);
  assert(generated.traces.size() == 1);
  assert(generated.traces.front().events.size() == 2);
  const auto xml = to_pnml(net, "extended");
  assert(xml.find("<text>reset</text>") != std::string::npos);
  const auto reread = parse_pnml(xml);
  assert(reread.transitions().size() == net.transitions().size());
  assert(reread.transitions()[2].reset_inputs.size() == 1);
  assert(reread.transitions()[2].inhibitor_inputs.empty());

  const auto inhibitor = inhibitor_net();
  const auto inhibitor_xml = to_pnml(inhibitor);
  assert(inhibitor_xml.find("<text>inhibitor</text>") != std::string::npos);
  const auto inhibitor_reread = parse_pnml(inhibitor_xml);
  assert(inhibitor_reread.transitions()[1].inhibitor_inputs.size() == 1);

  std::vector<std::int64_t> initial(net.place_count(), 0);
  std::vector<std::int64_t> final(net.place_count(), 0);
  initial[net.source()] = 2;
  initial[2] = 4;
  final[net.sink()] = 3;
  const auto marked_xml = to_pnml(net, initial, final, "marked");
  assert(marked_xml.find("<initialMarking><text>2</text>") !=
         std::string::npos);
  assert(marked_xml.find("<initialMarking><text>4</text>") !=
         std::string::npos);
  assert(marked_xml.find("<place idref=\"p3\"><text>3</text>") !=
         std::string::npos);
  const auto marked_document = parse_pnml_document(marked_xml);
  assert(marked_document.initial_marking == initial);
  assert(marked_document.final_marking == final);
  assert(marked_document.net.source() == net.source());
  assert(marked_document.net.sink() == net.sink());
  bool rejected = false;
  try {
    (void)parse_pnml(marked_xml);
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  assert(rejected);
  rejected = false;
  try {
    (void)to_pnml(net, std::vector<std::int64_t>{1}, final, "bad");
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
  rejected = false;
  initial[0] = -1;
  try {
    (void)to_pnml(net, initial, final, "bad");
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);

  ILPMinerResult json_result;
  json_result.net = net;
  json_result.markings.initial.assign(net.place_count(), 0);
  json_result.markings.final.assign(net.place_count(), 0);
  json_result.markings.initial[net.source()] = 1;
  json_result.markings.final[net.sink()] = 1;
  const auto json = ilp_miner_json(json_result);
  assert(json.find("\"reset_inputs\":[2]") != std::string::npos);
  assert(json.find("\"inhibitor_inputs\":[]") != std::string::npos);
}

void weighted_arc_round_trip() {
  PetriNet net;
  net.set_activities({"weighted"});
  const auto source = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.add_transition(0, {source, source, source}, {sink, sink});

  const auto xml = to_pnml(net, "weighted");
  assert(xml.find("<inscription><text>3</text></inscription>") !=
         std::string::npos);
  assert(xml.find("<inscription><text>2</text></inscription>") !=
         std::string::npos);
  const auto reread = parse_pnml(xml);
  assert(reread.transitions().size() == 1);
  assert(reread.transitions()[0].inputs.size() == 3);
  assert(reread.transitions()[0].outputs.size() == 2);

  const auto spaced = parse_pnml_document(R"(<pnml><net id="n"><page id="p">
    <place id="p0"><initialMarking><text> 2 </text></initialMarking></place>
    <place id="p1"/><transition id="t0"><name><text>A</text></name></transition>
    <arc id="a0" source="p0" target="t0"><inscription><text>
      2
    </text></inscription></arc><arc id="a1" source="t0" target="p1"/>
    <finalmarkings><marking><place idref="p1"><text> 3 </text></place></marking></finalmarkings>
  </page></net></pnml>)");
  assert(spaced.initial_marking[0] == 2);
  assert(spaced.final_marking[1] == 3);
  assert(spaced.net.transitions()[0].inputs.size() == 2);

  const auto zero_weight = parse_pnml_document(R"(<pnml><net id="n"><page id="p">
      <place id="p0"><initialMarking><text>1</text></initialMarking></place>
      <place id="p1"/><transition id="t0"/>
      <arc id="a0" source="p0" target="t0"><inscription><text>0</text></inscription></arc>
      <arc id="a1" source="t0" target="p1"/>
      <finalmarkings><marking><place idref="p1"><text>1</text></place></marking></finalmarkings>
    </page></net></pnml>)");
  assert(zero_weight.arc_occurrences->front().weight == 0);
  assert(zero_weight.net.transitions().front().inputs == std::vector<std::size_t>{0});
  assert(zero_weight.net.transitions().front().input_arc_weight(0).magnitude == 0);
  assert(parse_pnml_document(to_pnml(zero_weight)).arc_occurrences->front().weight == 0);

  bool rejected = false;
  try {
    (void)parse_pnml_document(R"(<pnml><net id="n"><page id="p">
      <place id="p0"><initialMarking><text>-1</text></initialMarking></place>
      <place id="p1"/><transition id="t0"/>
      <arc id="a0" source="p0" target="t0"/><arc id="a1" source="t0" target="p1"/>
    </page></net></pnml>)");
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  assert(rejected);
}

void import_options_and_stochastic_metadata() {
  const std::string xml = R"(<pnml><net id="n"><page id="p">
    <place id="p0"/><place id="p1"/>
    <transition id="t0"><name><text>A</text></name>
      <toolspecific tool="StochasticPetriNet" version="0.2">
        <property key="distributionType">EXPONENTIAL</property>
        <property key="distributionParameters">2.5</property>
        <property key="priority">4</property><property key="weight">3.5</property>
      </toolspecific>
    </transition>
    <arc id="a0" source="p0" target="t0"/><arc id="a1" source="t0" target="p1"/>
  </page></net></pnml>)";

  PnmlImportOptions options;
  options.infer_initial_marking = false;
  options.infer_final_marking = false;
  options.import_stochastic_map = true;
  const auto document = parse_pnml_document(xml, options);
  assert(document.initial_marking == std::vector<std::int64_t>({0, 0}));
  assert(document.final_marking == std::vector<std::int64_t>({0, 0}));
  assert(document.stochastic_map.has_value());
  const auto& transition = document.stochastic_map->transitions.front();
  assert(transition.distribution == StochasticMapDistribution::kExponential);
  assert(transition.parameter == 2.5);
  assert(transition.priority == 4);
  assert(transition.activation_weight == 3.5);

  const auto inferred = parse_pnml_document(xml);
  assert(inferred.initial_marking == std::vector<std::int64_t>({1, 0}));
  assert(inferred.final_marking == std::vector<std::int64_t>({0, 1}));
  assert(inferred.initial_marking_keys == std::vector<bool>({true, false}));
  assert(inferred.final_marking_keys == std::vector<bool>({false, true}));
  assert(!inferred.stochastic_map.has_value());

  StochasticMap map;
  StochasticMapTransition record;
  record.transition = 0;
  record.distribution = StochasticMapDistribution::kGamma;
  record.parameter = 2.0;
  record.secondary_parameter = 5.0;
  record.activation_weight = 1.75;
  record.priority = 7;
  map.transitions.push_back(record);
  std::vector<std::int64_t> initial{1, 0};
  std::vector<std::int64_t> final{0, 1};
  const auto exported = to_pnml(inferred.net, initial, final, map, "stochastic");
  assert(exported.find("<property key=\"distributionType\">GAMMA</property>") !=
         std::string::npos);
  PnmlImportOptions roundtrip_options;
  roundtrip_options.import_stochastic_map = true;
  const auto roundtrip = parse_pnml_document(exported, roundtrip_options);
  assert(roundtrip.stochastic_map.has_value());
  const auto& imported = roundtrip.stochastic_map->transitions.front();
  assert(imported.distribution == StochasticMapDistribution::kGamma);
  assert(imported.parameter == 2.0);
  assert(imported.secondary_parameter == 5.0);
  assert(imported.activation_weight == 1.75);
  assert(imported.priority == 7);
  const auto json = pnml_document_json(roundtrip);
  assert(json.find("\"family\":\"PNML_DOCUMENT\"") != std::string::npos);
  assert(json.find("\"initial_marking\":[1,0]") != std::string::npos);
  assert(json.find("\"distribution\":\"gamma\"") != std::string::npos);
}

void equation_guard_and_workflow_topology() {
  const auto net = reset_net();
  const auto equation = solve_marking_equation(net);
  assert(!equation.computed && equation.error.find("reset") != std::string::npos);
  const auto extended = solve_extended_marking_equation({"a", "b"}, net);
  assert(!extended.computed && extended.error.find("reset") != std::string::npos);
  assert(check_workflow_net(net).valid);
}

void general_pnml_preserves_empty_markings() {
  PnmlImportOptions options;
  options.infer_initial_marking = false;
  options.infer_final_marking = false;
  options.require_workflow_boundaries = false;
  const auto empty = parse_pnml_document("<pnml><net id=\"empty\"/></pnml>", options);
  assert(empty.net.place_count() == 0 && empty.net.transitions().empty());
  assert(empty.initial_marking.empty() && empty.final_marking.empty());
  assert(empty.initial_marking_keys.empty() && empty.final_marking_keys.empty());
  assert(empty.place_ids.empty());
  const auto isolated = parse_pnml_document(
      "<pnml><net id=\"isolated\"><place id=\"p0\"/><place id=\"p1\"/>"
      "<finalmarkings><marking><place idref=\"p0\"><text>0</text></place>"
      "</marking></finalmarkings></net></pnml>", options);
  assert(isolated.net.place_count() == 2 && isolated.net.transitions().empty());
  assert(isolated.initial_marking == std::vector<std::int64_t>({0, 0}));
  assert(isolated.place_ids == std::vector<std::string>({"p0", "p1"}));
  assert(isolated.final_marking == std::vector<std::int64_t>({0, 0}));
  assert(isolated.initial_marking_keys == std::vector<bool>({false, false}));
  assert(isolated.final_marking_keys == std::vector<bool>({true, false}));
  const auto transition_only = parse_pnml_document(
      "<pnml><net id=\"n\"><transition id=\"a\"><name><text>A</text>"
      "</name></transition></net></pnml>", options);
  assert(transition_only.net.place_count() == 0);
  assert(transition_only.net.transitions().size() == 1);
  assert(transition_only.transition_ids == std::vector<std::string>({"a"}));
  assert(!transition_only.net.transitions()[0].silent());
  for (const auto xml : {"<pnml/>",
       "<pnml><net id=\"n\"><arc source=\"missing\" target=\"a\"/></net></pnml>"}) {
    bool rejected = false;
    try { (void)parse_pnml_document(xml, options); }
    catch (const std::runtime_error&) { rejected = true; }
    assert(rejected);
  }
  bool rejected = false;
  try { (void)parse_pnml_document("<pnml><net id=\"empty\"/></pnml>"); }
  catch (const std::runtime_error&) { rejected = true; }
  assert(rejected);
}

void pnml_marking_key_presence() {
  PnmlImportOptions options;
  options.infer_initial_marking = false;
  options.infer_final_marking = false;
  options.require_workflow_boundaries = false;
  const auto document = parse_pnml_document(R"(<pnml><net id="keys">
    <place id="zero"><initialMarking><text>0</text></initialMarking></place>
    <place id="omitted"/>
    <place id="positive"><initialMarking><text>2</text></initialMarking></place>
    <place id="empty"><initialMarking/></place>
    <finalmarkings>
      <marking><place idref="zero"><text>0</text></place>
        <place idref="positive"><text>3</text></place></marking>
      <marking><place idref="omitted"><text>4</text></place>
        <place idref="empty"><text>0</text></place></marking>
    </finalmarkings>
  </net></pnml>)", options);
  assert(document.initial_marking == std::vector<std::int64_t>({0, 0, 2, 0}));
  assert(document.final_marking == std::vector<std::int64_t>({0, 0, 3, 0}));
  assert(document.initial_marking_keys == std::vector<bool>({true, false, true, true}));
  assert(document.final_marking_keys == std::vector<bool>({true, false, true, false}));
  const auto empty_first = parse_pnml_document(R"(<pnml><net id="first">
    <place id="p"/><finalmarkings><marking/>
      <marking><place idref="p"><text>2</text></place></marking>
    </finalmarkings></net></pnml>)", options);
  assert(empty_first.final_marking == std::vector<std::int64_t>({0}));
  assert(empty_first.final_marking_keys == std::vector<bool>({false}));
}

void pnml_document_export_contract() {
  PnmlDocument document;
  for (int i = 0; i < 5; ++i) document.net.add_place();
  document.net.set_activities({"A & B"});
  document.net.add_transition(0, {0, 0}, {4, 4, 4});
  document.net.add_reset_arc(0, 1);
  document.net.add_inhibitor_arc(0, 2);
  document.net.add_transition(std::nullopt, {4}, {0});
  document.place_ids = {"net1", "page1", "a0", "a0_1", "p<&\"'>"};
  document.transition_ids = {"visible", "silent<&\"'>"};
  document.initial_marking = {2, 0, 0, 0, 0};
  document.final_marking = {0, 0, 0, 0, std::numeric_limits<std::int64_t>::max()};
  document.initial_marking_keys = {true, true, false, false, false};
  document.final_marking_keys = {false, false, true, false, true};
  StochasticMap map;
  StochasticMapTransition timed;
  timed.parameter = 1.2345678901234567;
  timed.activation_weight = 0.12345678901234567;
  timed.priority = 7;
  map.transitions.push_back(timed);
  StochasticMapTransition immediate;
  immediate.transition = 1;
  immediate.timing = StochasticMapTiming::kImmediate;
  immediate.distribution = StochasticMapDistribution::kImmediate;
  immediate.parameter = 0;
  map.transitions.push_back(immediate);
  document.stochastic_map = map;
  const auto xml = to_pnml(document, "Example & model");
  assert(xml.find("<net id=\"net1_1\"") != std::string::npos);
  assert(xml.find("<page id=\"page1_1\"") != std::string::npos);
  assert(xml.find("<arc id=\"a0_2\"") != std::string::npos);
  assert(xml.find("localNodeID=\"silent&lt;&amp;&quot;&apos;&gt;\"") != std::string::npos);
  PnmlImportOptions options;
  options.infer_initial_marking = false;
  options.infer_final_marking = false;
  options.require_workflow_boundaries = false;
  options.import_stochastic_map = true;
  const auto restored = parse_pnml_document(xml, options);
  assert(restored.place_ids == document.place_ids);
  assert(restored.transition_ids == document.transition_ids);
  assert(restored.initial_marking == document.initial_marking);
  assert(restored.final_marking == document.final_marking);
  assert(restored.initial_marking_keys == document.initial_marking_keys);
  assert(restored.final_marking_keys == document.final_marking_keys);
  assert(restored.net.activities() == document.net.activities());
  const auto json = pnml_document_json(restored);
  assert(json.find("\"place_ids\":[\"net1\",\"page1\",\"a0\",\"a0_1\",\"p<&\\\"'>\"]") != std::string::npos);
  assert(json.find("\"transition_ids\":[\"visible\",\"silent<&\\\"'>\"]") != std::string::npos);
  assert(json.find("\"initial_marking_keys\":[true,true,false,false,false]") != std::string::npos);
  assert(json.find("\"final_marking_keys\":[false,false,true,false,true]") != std::string::npos);
  for (std::size_t t = 0; t < document.net.transitions().size(); ++t) {
    const auto& original = document.net.transitions()[t];
    const auto& copy = restored.net.transitions()[t];
    assert(copy.activity == original.activity);
    assert(copy.inputs == original.inputs && copy.outputs == original.outputs);
    assert(copy.reset_inputs == original.reset_inputs);
    assert(copy.inhibitor_inputs == original.inhibitor_inputs);
  }
  assert(restored.stochastic_map);
  const auto& record = restored.stochastic_map->transitions[0];
  assert(record.parameter == timed.parameter);
  assert(record.activation_weight == timed.activation_weight);
  assert(record.priority == timed.priority);
  assert(restored.stochastic_map->transitions[1].distribution == immediate.distribution);
  std::ostringstream formatted;
  formatted << std::hex;
  formatted.precision(2);
  write_pnml(formatted, document, "Example & model");
  assert(formatted.str() == xml);
  assert(formatted.precision() == 2);

  document.stochastic_map.reset();
  document.place_ids.clear();
  document.transition_ids.clear();
  document.initial_marking_keys.clear();
  document.final_marking_keys.clear();
  assert(to_pnml(document) == to_pnml(document.net, document.initial_marking,
                                     document.final_marking));
  const auto derived = parse_pnml_document(to_pnml(document), options);
  assert(derived.initial_marking_keys == std::vector<bool>({true, false, false, false, false}));
  assert(derived.final_marking_keys == std::vector<bool>({false, false, false, false, true}));
  const auto empty = parse_pnml_document(to_pnml(PnmlDocument{}), options);
  assert(empty.net.place_count() == 0 && empty.net.transitions().empty());
  assert(to_pnml(PetriNet{}) == to_pnml(PnmlDocument{}));
}

void pnml_document_export_retains_routing_order() {
  PnmlDocument document;
  for (int p = 0; p < 5; ++p) document.net.add_place();
  document.net.set_activities({"A"});
  document.net.add_transition(std::nullopt, {0}, {2, 1});
  document.net.add_transition(std::nullopt, {1}, {3});
  document.net.add_transition(std::nullopt, {2}, {3});
  document.net.add_transition(0, {3}, {4});
  document.initial_marking = {1, 0, 0, 0, 0};
  document.final_marking = {0, 0, 0, 0, 1};
  PnmlImportOptions import_options;
  import_options.infer_initial_marking = false;
  import_options.infer_final_marking = false;
  import_options.require_workflow_boundaries = false;
  const auto restored = parse_pnml_document(to_pnml(document), import_options);
  assert(restored.net.transitions()[0].outputs == std::vector<std::size_t>({2, 1}));
  TokenReplayOptions replay_options;
  replay_options.initial_marking = {1, 0, 0, 0, 0};
  replay_options.final_marking = {0, 0, 0, 0, 1};
  replay_options.try_to_reach_final_marking_through_hidden = false;
  const auto log = log_of({{"A"}});
  const auto original_trace = replay_token_traces(log, document.net, replay_options).front();
  const auto restored_trace = replay_token_traces(log, restored.net, replay_options).front();
  assert(original_trace.activated_transitions == std::vector<std::size_t>({0, 2, 3}));
  assert(restored_trace.activated_transitions == original_trace.activated_transitions);
  assert(restored_trace.reached_marking == original_trace.reached_marking);
  const auto legacy = parse_pnml_document(
      to_pnml(document.net, document.initial_marking, document.final_marking), import_options);
  assert(legacy.net.transitions()[0].outputs == std::vector<std::size_t>({1, 2}));
  const auto legacy_trace = replay_token_traces(log, legacy.net, replay_options).front();
  assert(legacy_trace.activated_transitions == std::vector<std::size_t>({0, 1, 3}));

  // Repeated endpoints aggregate at their first position on either arc side.
  document.net.add_transition(std::nullopt, {3, 1, 3}, {4, 2, 4});
  const auto weighted = parse_pnml_document(to_pnml(document), import_options);
  assert(weighted.net.transitions().back().inputs == std::vector<std::size_t>({3, 3, 1}));
  assert(weighted.net.transitions().back().outputs == std::vector<std::size_t>({4, 4, 2}));
}

void pnml_document_export_preserves_xml_whitespace() {
  PnmlDocument document;
  for (int p = 0; p < 4; ++p) document.net.add_place();
  document.net.set_activities({"A\rB"});
  document.net.add_transition(0, {0}, {1});
  document.net.add_transition(std::nullopt, {2}, {3});
  document.place_ids = {"a\nb", "a b", "c\td", "c d"};
  document.transition_ids = {"e\rf", "g\t\n\rh"};
  document.initial_marking = {0, 0, 0, 0};
  document.final_marking = {0, 0, 0, 0};
  document.initial_marking_keys = {true, false, true, false};
  document.final_marking_keys = {true, true, true, true};
  const auto xml = to_pnml(document, "name\rwith CR");
  assert(xml.find("id=\"a&#10;b\"") != std::string::npos);
  assert(xml.find("id=\"c&#9;d\"") != std::string::npos);
  assert(xml.find("source=\"a&#10;b\" target=\"e&#13;f\"") != std::string::npos);
  assert(xml.find("localNodeID=\"g&#9;&#10;&#13;h\"") != std::string::npos);
  assert(xml.find("idref=\"a&#10;b\"") != std::string::npos);
  assert(xml.find("<text>A&#13;B</text>") != std::string::npos);
  assert(xml.find("<text>name&#13;with CR</text>") != std::string::npos);
  PnmlImportOptions options;
  options.infer_initial_marking = false;
  options.infer_final_marking = false;
  options.require_workflow_boundaries = false;
  const auto restored = parse_pnml_document(xml, options);
  assert(restored.place_ids == document.place_ids);
  assert(restored.transition_ids == document.transition_ids);
  assert(restored.net.activities() == document.net.activities());
  assert(restored.initial_marking_keys == document.initial_marking_keys);
  assert(restored.final_marking_keys == document.final_marking_keys);
  assert(restored.net.transitions()[0].inputs == std::vector<std::size_t>({0}));
  assert(restored.net.transitions()[0].outputs == std::vector<std::size_t>({1}));
  assert(restored.net.transitions()[1].inputs == std::vector<std::size_t>({2}));
  assert(restored.net.transitions()[1].outputs == std::vector<std::size_t>({3}));
}

// Import full typed arc objects without applying incremental-edit restrictions.
void pnml_mixed_input_arc_occurrences() {
  struct Input { PetriArcType type; std::string weight; };
  const std::vector<std::vector<Input>> cases = {
    {{PetriArcType::kNormal,"2"},{PetriArcType::kReset,"3"}},
    {{PetriArcType::kNormal,"2"},{PetriArcType::kInhibitor,"5"}},
    {{PetriArcType::kReset,"3"},{PetriArcType::kInhibitor,"5"}},
    {{PetriArcType::kNormal,"2"},{PetriArcType::kReset,"3"},{PetriArcType::kInhibitor,"5"}},
    {{PetriArcType::kReset,"3"},{PetriArcType::kReset,"4"}},
    {{PetriArcType::kInhibitor,"5"},{PetriArcType::kInhibitor,"6"}},
    {{PetriArcType::kNormal,"-0"},{PetriArcType::kReset,"-7"},
     {PetriArcType::kInhibitor,"1844674407370955161600000000000000000000"}}
  };
  for (const auto& inputs : cases) {
    std::string xml="<pnml><net id=\"mixed\"><page id=\"page\">"
      "<place id=\"source\"><initialMarking><text>2</text></initialMarking></place>"
      "<place id=\"sink\"/><transition id=\"move\"><name><text>A</text></name></transition>";
    std::size_t resets=0,inhibitors=0;
    for (std::size_t i=0;i<inputs.size();++i) {
      const auto& arc=inputs[i];
      xml+="<arc id=\"input-"+std::to_string(i)+"&#10;&amp;\" source=\"source\" target=\"move\">"
        "<inscription><text>"+arc.weight+"</text></inscription>";
      if (arc.type==PetriArcType::kReset) {xml+="<arctype><text>reset</text></arctype>";++resets;}
      if (arc.type==PetriArcType::kInhibitor) {xml+="<arctype><text>inhibitor</text></arctype>";++inhibitors;}
      xml+="</arc>";
    }
    xml+="<arc id=\"output\" source=\"move\" target=\"sink\"/></page>"
      "<finalmarkings><marking><place idref=\"sink\"><text>1</text></place></marking></finalmarkings></net></pnml>";
    const auto document=parse_pnml_document(xml);
    assert(document.arc_occurrences&&document.arc_occurrences->size()==inputs.size()+1);
    const auto& transition=document.net.transitions().at(0);
    assert(transition.reset_inputs.size()==resets&&transition.inhibitor_inputs.size()==inhibitors);
    for(std::size_t i=0;i<inputs.size();++i) {
      const auto& arc=document.arc_occurrences->at(i);
      const TokenInteger signed_weight(inputs[i].weight);
      assert(arc.input&&arc.place==0&&arc.transition==0&&arc.type==inputs[i].type);
      assert(arc.weight==(signed_weight<0?-signed_weight:signed_weight));
      assert(arc.negative_weight==(signed_weight<0));
      assert(arc.id=="input-"+std::to_string(i)+"\n&");
    }
    const auto before=serialize_quality_workbench_document_json(document);
    const auto graph=to_petri_net_presentation(document);
    assert(graph.arcs.size()==inputs.size()+1);
    const auto restored=parse_pnml_document(to_pnml(document));
    assert(serialize_quality_workbench_document_json(restored)==before);
    assert(serialize_quality_workbench_document_json(document)==before);

    const auto rejects_without_mutation=[](const PnmlDocument& invalid) {
      const auto before=serialize_quality_workbench_document_json(invalid);
      std::ostringstream out;out<<"untouched";bool rejected=false;
      try{write_pnml(out,invalid);}catch(const std::invalid_argument&){rejected=true;}
      assert(rejected&&out.str()=="untouched");
      rejected=false;try{(void)to_petri_net_presentation(invalid);}catch(const std::invalid_argument&){rejected=true;}
      assert(rejected&&serialize_quality_workbench_document_json(invalid)==before);
    };
    auto invalid=document;invalid.arc_occurrences->at(0).transition=99;rejects_without_mutation(invalid);
    invalid=document;invalid.arc_occurrences->at(0).weight+=1;rejects_without_mutation(invalid);
    invalid=document;invalid.arc_occurrences->pop_back();rejects_without_mutation(invalid);
  }
  // This fixes import assembly; the existing incremental editing policy stays.
  PetriNet edited;edited.add_place();edited.add_place();edited.add_transition(0,{0},{1});
  bool rejected=false;try{edited.add_reset_arc(0,0);}catch(const std::invalid_argument&){rejected=true;}
  assert(rejected&&edited.transitions()[0].reset_inputs.empty());
}

void pnml_document_preserves_arc_occurrences() {
  const auto document = parse_pnml_document(R"(<pnml><net id="parallel"><page id="page">
    <place id="p0"><initialMarking><text>3</text></initialMarking></place>
    <place id="p1"/><place id="p2"/><place id="p3"/>
    <transition id="t0"><name><text>A</text></name></transition>
    <transition id="t1"/>
    <arc id="first&#10;&amp;" source="p0" target="t0"/>
    <arc id="out1" source="t0" target="p1"><inscription><text>2</text></inscription></arc>
    <arc id="second" source="p0" target="t0"><inscription><text>2</text></inscription></arc>
    <arc id="inhibit" source="p2" target="t1"><arctype><text>inhibitor</text></arctype></arc>
    <arc id="reset" source="p3" target="t1"><arctype><text>reset</text></arctype></arc>
    <arc id="out2" source="t0" target="p1"/>
    <arc source="t1" target="p3"/>
  </page><finalmarkings><marking><place idref="p1"><text>3</text></place></marking></finalmarkings>
  </net></pnml>)");
  assert(document.arc_occurrences && document.arc_occurrences->size() == 7);
  assert(document.net.transitions()[0].inputs == std::vector<std::size_t>({0, 0, 0}));
  assert(document.net.transitions()[0].outputs == std::vector<std::size_t>({1, 1, 1}));
  const auto graph = to_petri_net_presentation(document);
  assert(graph.arcs.size() == 7 && graph.arcs[1].weight == 2 && graph.arcs[2].weight == 2);
  assert(graph.transitions[0].label == "A" && graph.transitions[1].label == "t1");
  const auto xml = to_pnml(document);
  assert(xml.find("id=\"first&#10;&amp;\"") != std::string::npos);
  assert(xml.find("id=\"second\"") != std::string::npos);
  const auto restored = parse_pnml_document(xml);
  assert(restored.arc_occurrences && restored.arc_occurrences->size() == 7);
  for (std::size_t a = 0; a < document.arc_occurrences->size(); ++a) {
    const auto& before = (*document.arc_occurrences)[a];
    const auto& after = (*restored.arc_occurrences)[a];
    assert(before.place == after.place && before.transition == after.transition);
    assert(before.input == after.input && before.type == after.type && before.weight == after.weight);
    if (!before.id.empty()) assert(before.id == after.id);
  }
  TokenReplayOptions replay;
  replay.initial_marking = {3, 0, 0, 0};
  replay.final_marking = {0, 3, 0, 0};
  const auto original_trace = replay_token_traces(log_of({{"A"}}), document.net, replay).front();
  const auto restored_trace = replay_token_traces(log_of({{"A"}}), restored.net, replay).front();
  assert(original_trace.missing == 0 && original_trace.reached_marking == std::vector<int>({0, 3, 0, 0}));
  assert(restored_trace.reached_marking == original_trace.reached_marking);
  assert(restored_trace.activated_transitions == original_trace.activated_transitions);

  const auto rejects = [](const PnmlDocument& invalid) {
    std::ostringstream out;
    out << "unchanged";
    bool rejected = false;
    try { write_pnml(out, invalid); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected && out.str() == "unchanged");
  };
  auto invalid = document; invalid.arc_occurrences->pop_back(); rejects(invalid);
  invalid = document; (*invalid.arc_occurrences)[0].weight = 0; rejects(invalid);
  invalid = document; (*invalid.arc_occurrences)[0].weight = std::numeric_limits<std::size_t>::max(); rejects(invalid);
  invalid = document; (*invalid.arc_occurrences)[0].place = 99; rejects(invalid);
  invalid = document; (*invalid.arc_occurrences)[0].transition = 99; rejects(invalid);
  invalid = document; (*invalid.arc_occurrences)[0].input = false; rejects(invalid);
  invalid = document; (*invalid.arc_occurrences)[3].weight = 2; rejects(invalid);
  invalid = document; (*invalid.arc_occurrences)[3].type = static_cast<PetriArcType>(99); rejects(invalid);
  invalid = document; (*invalid.arc_occurrences)[3].id = "second"; rejects(invalid);
  invalid = document; (*invalid.arc_occurrences)[3].id = "p0"; rejects(invalid);
  invalid = document; invalid.net.add_transition(0, {0}, {1}); rejects(invalid);
  invalid = document; invalid.arc_occurrences->clear(); rejects(invalid);
  invalid.arc_occurrences.reset();
  assert(!to_pnml(invalid).empty());

  PnmlDocument empty;
  empty.arc_occurrences.emplace();
  assert(to_petri_net_presentation(empty).arcs.empty());
  assert(to_pnml(empty) == to_pnml(PnmlDocument{}));
}

void pnml_document_export_rejects_atomically() {
  PnmlDocument valid;
  valid.net.add_place(); valid.net.add_place();
  valid.net.set_activities({"A"});
  valid.net.add_transition(0, {0}, {1});
  valid.initial_marking = {1, 0}; valid.final_marking = {0, 1};
  const auto rejects = [](const PnmlDocument& document) {
    std::ostringstream out;
    out << "unchanged";
    bool rejected = false;
    try { write_pnml(out, document); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected && out.str() == "unchanged");
  };
  auto invalid = valid; invalid.initial_marking.pop_back(); rejects(invalid);
  invalid = valid; invalid.final_marking[0] = -1; rejects(invalid);
  invalid = valid; invalid.place_ids = {"one"}; rejects(invalid);
  invalid = valid; invalid.place_ids = {"same", "same"}; rejects(invalid);
  invalid = valid; invalid.place_ids = {"", ""}; rejects(invalid);
  invalid = valid; invalid.place_ids = {"one", "two"};
  invalid.transition_ids = {"one"}; rejects(invalid);
  invalid = valid; invalid.place_ids = {"t0", "two"}; rejects(invalid);
  invalid = valid; invalid.transition_ids = {"one", "two"}; rejects(invalid);
  invalid = valid; invalid.initial_marking_keys = {true}; rejects(invalid);
  invalid = valid; invalid.final_marking_keys = {false, false}; rejects(invalid);
  invalid = valid; invalid.initial_marking_keys = {false, true}; rejects(invalid);
  invalid = valid; invalid.stochastic_map = StochasticMap{}; rejects(invalid);
  invalid = valid; invalid.net.add_transition(0, {99}, {}); rejects(invalid);
  invalid = valid; invalid.net.add_transition(2, {}, {}); rejects(invalid);
}

void normal_alignment_smoke() {
  PetriNet net;
  net.set_activities({"A", "B"});
  const auto p0 = net.add_place(), p1 = net.add_place(), p2 = net.add_place();
  net.set_source(p0); net.set_sink(p2);
  net.add_transition(0, {p0}, {p1});
  net.add_transition(1, {p1}, {p2});
  const auto result = check_alignment(log_of({{"A", "X"}}), net);
  assert(result.computed && result.fitness < 1.0);
}

void pnml_arc_type_wire_contract() {
  const std::vector<std::pair<std::string, PetriArcType>> controls = {
    {"<arctype><text>reset</text></arctype>", PetriArcType::kReset},
    {"<arctype><text>inhibitor</text></arctype>", PetriArcType::kInhibitor},
    {"<type><text>reset</text></type>", PetriArcType::kNormal},
    {"<type><text>inhibitor</text></type>", PetriArcType::kNormal},
    {"<arctype><text>reset</text></arctype><type><text>inhibitor</text></type>", PetriArcType::kReset},
    {"<arctype><text>inhibitor</text></arctype><type><text>reset</text></type>", PetriArcType::kInhibitor},
    {"<arctype><text>unknown</text></arctype>", PetriArcType::kNormal},
    {"<arctype><text>RESET</text></arctype>", PetriArcType::kNormal},
    {"<arctype><text> reset </text></arctype>", PetriArcType::kNormal},
    {"<arctype><text/></arctype>", PetriArcType::kNormal}
  };
  for (const auto& [annotation, expected] : controls) {
    const auto document = parse_pnml_document(
      "<pnml><net id=\"typed\"><place id=\"source\"><initialMarking><text>1</text></initialMarking></place>"
      "<place id=\"gate\"/><place id=\"sink\"/><transition id=\"move\"/>"
      "<arc id=\"guard\" source=\"source\" target=\"move\"/>"
      "<arc id=\"typed\" source=\"gate\" target=\"move\">" + annotation + "</arc>"
      "<arc id=\"out\" source=\"move\" target=\"sink\"/>"
      "<finalmarkings><marking><place idref=\"sink\"><text>1</text></place></marking></finalmarkings></net></pnml>");
    assert(document.arc_occurrences->at(1).type == expected);
    const auto& transition = document.net.transitions().front();
    assert(transition.inputs.size() == (expected == PetriArcType::kNormal ? 2 : 1));
    assert(transition.reset_inputs.size() == (expected == PetriArcType::kReset ? 1 : 0));
    assert(transition.inhibitor_inputs.size() == (expected == PetriArcType::kInhibitor ? 1 : 0));
    const auto exported = to_pnml(document);
    assert(exported.find("<type>") == std::string::npos);
    assert((exported.find("<arctype><text>reset</text></arctype>") != std::string::npos) == (expected == PetriArcType::kReset));
    assert((exported.find("<arctype><text>inhibitor</text></arctype>") != std::string::npos) == (expected == PetriArcType::kInhibitor));
    assert(parse_pnml_document(exported).arc_occurrences->at(1).type == expected);
  }
}

void pnml_extended_weight_and_text_contract() {
  struct Weight { std::string text; std::uint64_t magnitude; bool negative; };
  const std::vector<Weight> weights = {
    {"2",2,false},{"0",0,false},{"-1",1,true},{"-2",2,true},{"+2",2,false},
    {"1_0",10,false},{"٢",2,false},{"１２",12,false},
    {"18446744073709551615",std::numeric_limits<std::uint64_t>::max(),false}};
  const auto xml = [](const std::string& annotation) {
    return "<pnml><net id=\"weights\"><place id=\"source\"><initialMarking><text>2</text></initialMarking></place>"
      "<place id=\"gate\"><initialMarking><text>3</text></initialMarking></place><place id=\"sink\"/><transition id=\"move\"/>"
      "<arc source=\"source\" target=\"move\"/><arc source=\"gate\" target=\"move\">" + annotation + "</arc>"
      "<arc source=\"move\" target=\"sink\"/><finalmarkings><marking><place idref=\"sink\"><text>1</text></place></marking></finalmarkings></net></pnml>";
  };
  for (const auto type : {"reset", "inhibitor"}) for (const auto& weight : weights) {
    const auto document = parse_pnml_document(xml("<inscription><text>" + weight.text + "</text></inscription><arctype><text>" + type + "</text></arctype>"));
    const auto& arc = document.arc_occurrences->at(1);
    assert(arc.weight == weight.magnitude && arc.negative_weight == weight.negative);
    const auto& transition = document.net.transitions().front();
    const auto retained = arc.type == PetriArcType::kReset ? transition.reset_arc_weight(0) : transition.inhibitor_arc_weight(0);
    assert(retained.magnitude == weight.magnitude && retained.negative == weight.negative);
    const auto round_trip = parse_pnml_document(to_pnml(document));
    assert(round_trip.arc_occurrences->at(1).weight == weight.magnitude);
    assert(round_trip.arc_occurrences->at(1).negative_weight == weight.negative);
    auto derived = document; derived.arc_occurrences.reset();
    const auto projection = to_petri_net_presentation(derived);
    assert(projection.arcs.at(1).weight == weight.magnitude && projection.arcs.at(1).negative_weight == weight.negative);
    const auto primitive_round_trip = parse_pnml_document(to_pnml(document.net, document.initial_marking, document.final_marking));
    assert(primitive_round_trip.arc_occurrences->at(1).weight == weight.magnitude);
    assert(primitive_round_trip.arc_occurrences->at(1).negative_weight == weight.negative);
  }
  for (const auto& [annotation, type] : std::vector<std::pair<std::string,PetriArcType>>{
      {"<arctype><text>re<![CDATA[set]]></text></arctype>",PetriArcType::kReset},
      {"<arctype><text>re<!--owned-->set</text></arctype>",PetriArcType::kReset},
      {"<arctype><text>reset<b/>inhibitor</text></arctype>",PetriArcType::kReset},
      {"<arctype><text>re<?owned test?>set</text></arctype>",PetriArcType::kNormal},
      {"<arctype><text>reset</text></arctype><arctype><text/></arctype>",PetriArcType::kNormal},
      {"<arctype><text>reset</text><text/></arctype>",PetriArcType::kNormal},
      {"<x:arctype xmlns:x=\"urn:owned\"><x:text>inhibitor</x:text></x:arctype>",PetriArcType::kInhibitor}}) {
    assert(parse_pnml_document(xml(annotation)).arc_occurrences->at(1).type == type);
  }
  for (const auto& [annotation, weight] : std::vector<std::pair<std::string,std::size_t>>{
      {"<inscription><text>1<![CDATA[0]]></text></inscription>",10},
      {"<inscription><text>1<!--owned-->0</text></inscription>",10},
      {"<inscription><text>2<b/>3</text></inscription>",2},
      {"<inscription><text>1<?owned test?>0</text></inscription>",1}}) {
    assert(parse_pnml_document(xml(annotation)).arc_occurrences->at(1).weight == weight);
  }
  bool rejected = false;
  try { (void)parse_pnml_document(xml("<inscription><text>2</text></inscription><inscription><text/></inscription>")); }
  catch (const std::runtime_error&) { rejected = true; }
  assert(rejected);
  auto stale = parse_pnml_document(xml("<inscription><text>2</text></inscription><arctype><text>reset</text></arctype>"));
  stale.arc_occurrences->at(1).weight = 3;
  rejected = false;
  try { (void)to_pnml(stale); } catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected);
}

void pnml_compact_ordinary_weight_contract() {
  struct Weight { const char* text; std::uint64_t magnitude; bool negative; };
  const std::vector<Weight> weights = {{"0",0,false},{"-1",1,true},{"-2",2,true},
    {"1025",1025,false},{"2147483648",2147483648ull,false},
    {"18446744073709551615",std::numeric_limits<std::uint64_t>::max(),false}};
  for (const bool input : {true,false}) for (const auto& weight : weights) {
    const std::string inscription = "<inscription><text>" + std::string(weight.text) + "</text></inscription>";
    const auto xml = std::string("<pnml><net id=\"compact\"><place id=\"source\"><initialMarking><text>2</text></initialMarking></place>") +
      "<place id=\"gate\"><initialMarking><text>3</text></initialMarking></place><place id=\"sink\"/><transition id=\"move\"/>" +
      "<arc source=\"source\" target=\"move\"/><arc source=\"gate\" target=\"move\">" + (input ? inscription : "") + "</arc>" +
      "<arc source=\"move\" target=\"sink\">" + (input ? "" : inscription) + "</arc>" +
      "<finalmarkings><marking><place idref=\"sink\"><text>1</text></place></marking></finalmarkings></net></pnml>";
    const auto document = parse_pnml_document(xml);
    const auto& transition = document.net.transitions().front();
    assert(transition.has_compact_ordinary_arcs());
    assert(transition.inputs.size() == 2 && transition.outputs.size() == 1);
    const auto gate = minera::tools::pnml_conformance_weight_gate(document.net);
    assert(gate && !gate->computed && !gate->error.empty());
    DirectlyFollowsGraph registry;
    const auto unavailable = imported_model_json(*gate,registry);
    assert(unavailable.find("\"computed\": false") != std::string::npos);
    assert(unavailable.find("\"sound\": null") != std::string::npos);
    assert(unavailable.find("\"token_replay\": null") != std::string::npos);
    assert(unavailable.find("\"footprints\": null") != std::string::npos);
    assert(unavailable.find("\"fitness\"") == std::string::npos);
    const auto retained = input ? transition.input_arc_weight(1) : transition.output_arc_weight(0);
    assert(retained.magnitude == weight.magnitude && retained.negative == weight.negative);
    const auto round_trip = parse_pnml_document(to_pnml(document));
    const auto primitive = parse_pnml_document(to_pnml(document.net,document.initial_marking,document.final_marking));
    for (const auto* following : {&round_trip,&primitive}) {
      const auto& arc = following->arc_occurrences->at(input ? 1 : 2);
      assert(arc.weight == weight.magnitude && arc.negative_weight == weight.negative);
    }
    auto derived = document; derived.arc_occurrences.reset();
    const auto projection = to_petri_net_presentation(derived);
    assert(projection.arcs.at(input ? 1 : 2).weight == weight.magnitude);
    assert(projection.arcs.at(input ? 1 : 2).negative_weight == weight.negative);
    std::vector<std::int64_t> marking{2,3,0};
    assert(petri_transition_enabled(transition,marking) == (!input || weight.negative || weight.magnitude <= 3));
    bool overflow = false;
    try { fire_petri_transition(transition,marking); }
    catch (const std::overflow_error&) { overflow = true; }
    const bool output_overflow = !input && !weight.negative && weight.magnitude > std::uint64_t(std::numeric_limits<std::int64_t>::max());
    assert(overflow == output_overflow);
    if (!overflow) {
      assert(marking[0] == 1);
      const auto gate = input ? weight.negative ? 3 + static_cast<std::int64_t>(weight.magnitude)
          : weight.magnitude >= 3 ? 0 : 3 - static_cast<std::int64_t>(weight.magnitude) : 2;
      const auto sink = input ? 1 : weight.negative ? -static_cast<std::int64_t>(weight.magnitude) : static_cast<std::int64_t>(weight.magnitude);
      assert(marking[1] == gate && marking[2] == sink);
    }
    SimplicityOptions options; options.initial_marking = {2,3,0};
    const auto result = evaluate_simplicity(document.net,SimplicityVariant::EXTENDED_CYCLOMATIC,options);
    if (output_overflow) assert(!result.computed && result.error.find("range") != std::string::npos);
    else assert(result.computed && result.value == (input && !weight.negative && weight.magnitude > 3 ? 0.0 : 2.0));
    if (input && !weight.magnitude)
      assert(evaluate_simplicity(document.net,SimplicityVariant::EXTENDED_CARDOSO).value == 2.0);
    auto stale = document; stale.arc_occurrences->at(input ? 1 : 2).weight = 7;
    bool rejected = false;
    try { (void)to_pnml(stale); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
  }
}

void pnml_transition_label_contract() {
  struct Control { const char* xml; bool imported; std::optional<std::string> label; };
  const std::vector<Control> controls = {
{R"labels_fixture(<pnml><net id="label"><page id="page"><place id="p0"><initialMarking><text>1</text></initialMarking></place><place id="p1"/><transition id="t0"></transition><arc source="p0" target="t0"/><arc source="t0" target="p1"/></page><finalmarkings><marking><place idref="p1"><text>1</text></place></marking></finalmarkings></net></pnml>)labels_fixture",true,std::optional<std::string>{"t0"}},
{R"labels_fixture(<pnml><net id="label"><page id="page"><place id="p0"><initialMarking><text>1</text></initialMarking></place><place id="p1"/><transition id="t0"><name/></transition><arc source="p0" target="t0"/><arc source="t0" target="p1"/></page><finalmarkings><marking><place idref="p1"><text>1</text></place></marking></finalmarkings></net></pnml>)labels_fixture",true,std::optional<std::string>{"t0"}},
{R"labels_fixture(<pnml><net id="label"><page id="page"><place id="p0"><initialMarking><text>1</text></initialMarking></place><place id="p1"/><transition id="t0"><name>A</name></transition><arc source="p0" target="t0"/><arc source="t0" target="p1"/></page><finalmarkings><marking><place idref="p1"><text>1</text></place></marking></finalmarkings></net></pnml>)labels_fixture",true,std::optional<std::string>{"t0"}},
{R"labels_fixture(<pnml><net id="label"><page id="page"><place id="p0"><initialMarking><text>1</text></initialMarking></place><place id="p1"/><transition id="t0"><name><text/></name></transition><arc source="p0" target="t0"/><arc source="t0" target="p1"/></page><finalmarkings><marking><place idref="p1"><text>1</text></place></marking></finalmarkings></net></pnml>)labels_fixture",true,std::optional<std::string>{"t0"}},
{R"labels_fixture(<pnml><net id="label"><page id="page"><place id="p0"><initialMarking><text>1</text></initialMarking></place><place id="p1"/><transition id="t0"><name><text></text></name></transition><arc source="p0" target="t0"/><arc source="t0" target="p1"/></page><finalmarkings><marking><place idref="p1"><text>1</text></place></marking></finalmarkings></net></pnml>)labels_fixture",true,std::optional<std::string>{"t0"}},
{R"labels_fixture(<pnml><net id="label"><page id="page"><place id="p0"><initialMarking><text>1</text></initialMarking></place><place id="p1"/><transition id="t0"><name><text>A</text></name></transition><arc source="p0" target="t0"/><arc source="t0" target="p1"/></page><finalmarkings><marking><place idref="p1"><text>1</text></place></marking></finalmarkings></net></pnml>)labels_fixture",true,std::optional<std::string>{"A"}},
{R"labels_fixture(<pnml><net id="label"><page id="page"><place id="p0"><initialMarking><text>1</text></initialMarking></place><place id="p1"/><transition id="t0"><name><text> </text></name></transition><arc source="p0" target="t0"/><arc source="t0" target="p1"/></page><finalmarkings><marking><place idref="p1"><text>1</text></place></marking></finalmarkings></net></pnml>)labels_fixture",true,std::optional<std::string>{" "}},
{R"labels_fixture(<pnml><net id="label"><page id="page"><place id="p0"><initialMarking><text>1</text></initialMarking></place><place id="p1"/><transition id="t0"><name><text>
</text></name></transition><arc source="p0" target="t0"/><arc source="t0" target="p1"/></page><finalmarkings><marking><place idref="p1"><text>1</text></place></marking></finalmarkings></net></pnml>)labels_fixture",true,std::optional<std::string>{"\n"}},
{R"labels_fixture(<pnml><net id="label"><page id="page"><place id="p0"><initialMarking><text>1</text></initialMarking></place><place id="p1"/><transition id="t0"><name><text><![CDATA[]]></text></name></transition><arc source="p0" target="t0"/><arc source="t0" target="p1"/></page><finalmarkings><marking><place idref="p1"><text>1</text></place></marking></finalmarkings></net></pnml>)labels_fixture",true,std::optional<std::string>{"t0"}},
{R"labels_fixture(<pnml><net id="label"><page id="page"><place id="p0"><initialMarking><text>1</text></initialMarking></place><place id="p1"/><transition id="t0"><name><text>A<![CDATA[B]]></text></name></transition><arc source="p0" target="t0"/><arc source="t0" target="p1"/></page><finalmarkings><marking><place idref="p1"><text>1</text></place></marking></finalmarkings></net></pnml>)labels_fixture",true,std::optional<std::string>{"AB"}},
{R"labels_fixture(<pnml><net id="label"><page id="page"><place id="p0"><initialMarking><text>1</text></initialMarking></place><place id="p1"/><transition id="t0"><name><text>A<!--owned-->B</text></name></transition><arc source="p0" target="t0"/><arc source="t0" target="p1"/></page><finalmarkings><marking><place idref="p1"><text>1</text></place></marking></finalmarkings></net></pnml>)labels_fixture",true,std::optional<std::string>{"AB"}},
{R"labels_fixture(<pnml><net id="label"><page id="page"><place id="p0"><initialMarking><text>1</text></initialMarking></place><place id="p1"/><transition id="t0"><name><text>A<?owned test?>B</text></name></transition><arc source="p0" target="t0"/><arc source="t0" target="p1"/></page><finalmarkings><marking><place idref="p1"><text>1</text></place></marking></finalmarkings></net></pnml>)labels_fixture",true,std::optional<std::string>{"A"}},
{R"labels_fixture(<pnml><net id="label"><page id="page"><place id="p0"><initialMarking><text>1</text></initialMarking></place><place id="p1"/><transition id="t0"><name><text>A<b/>B</text></name></transition><arc source="p0" target="t0"/><arc source="t0" target="p1"/></page><finalmarkings><marking><place idref="p1"><text>1</text></place></marking></finalmarkings></net></pnml>)labels_fixture",true,std::optional<std::string>{"A"}},
{R"labels_fixture(<pnml><net id="label"><page id="page"><place id="p0"><initialMarking><text>1</text></initialMarking></place><place id="p1"/><transition id="t0"><name><text><b/>A</text></name></transition><arc source="p0" target="t0"/><arc source="t0" target="p1"/></page><finalmarkings><marking><place idref="p1"><text>1</text></place></marking></finalmarkings></net></pnml>)labels_fixture",true,std::optional<std::string>{"t0"}},
{R"labels_fixture(<pnml><net id="label"><page id="page"><place id="p0"><initialMarking><text>1</text></initialMarking></place><place id="p1"/><transition id="t0"><name><text>A</text></name><name><text>B</text></name></transition><arc source="p0" target="t0"/><arc source="t0" target="p1"/></page><finalmarkings><marking><place idref="p1"><text>1</text></place></marking></finalmarkings></net></pnml>)labels_fixture",true,std::optional<std::string>{"A"}},
{R"labels_fixture(<pnml><net id="label"><page id="page"><place id="p0"><initialMarking><text>1</text></initialMarking></place><place id="p1"/><transition id="t0"><name><text>A</text><text>B</text></name></transition><arc source="p0" target="t0"/><arc source="t0" target="p1"/></page><finalmarkings><marking><place idref="p1"><text>1</text></place></marking></finalmarkings></net></pnml>)labels_fixture",true,std::optional<std::string>{"A"}},
{R"labels_fixture(<pnml><net id="label"><page id="page"><place id="p0"><initialMarking><text>1</text></initialMarking></place><place id="p1"/><transition id="t0"><name><text>A</text></name><name><text/></name></transition><arc source="p0" target="t0"/><arc source="t0" target="p1"/></page><finalmarkings><marking><place idref="p1"><text>1</text></place></marking></finalmarkings></net></pnml>)labels_fixture",true,std::optional<std::string>{"A"}},
{R"labels_fixture(<pnml><net id="label"><page id="page"><place id="p0"><initialMarking><text>1</text></initialMarking></place><place id="p1"/><transition id="t0"><name><text>A</text><text/></name></transition><arc source="p0" target="t0"/><arc source="t0" target="p1"/></page><finalmarkings><marking><place idref="p1"><text>1</text></place></marking></finalmarkings></net></pnml>)labels_fixture",true,std::optional<std::string>{"A"}},
{R"labels_fixture(<pnml><net id="label"><page id="page"><place id="p0"><initialMarking><text>1</text></initialMarking></place><place id="p1"/><transition id="t0"><name><wrapper><text>A</text></wrapper></name></transition><arc source="p0" target="t0"/><arc source="t0" target="p1"/></page><finalmarkings><marking><place idref="p1"><text>1</text></place></marking></finalmarkings></net></pnml>)labels_fixture",true,std::optional<std::string>{"t0"}},
{R"labels_fixture(<pnml><net id="label"><page id="page"><place id="p0"><initialMarking><text>1</text></initialMarking></place><place id="p1"/><transition id="t0"><x:name xmlns:x="urn:owned"><x:text>A</x:text></x:name></transition><arc source="p0" target="t0"/><arc source="t0" target="p1"/></page><finalmarkings><marking><place idref="p1"><text>1</text></place></marking></finalmarkings></net></pnml>)labels_fixture",true,std::optional<std::string>{"A"}},
{R"labels_fixture(<pnml><net id="label"><page id="page"><place id="p0"><initialMarking><text>1</text></initialMarking></place><place id="p1"/><transition id="t0"><name xmlns="urn:owned"><text>A</text></name></transition><arc source="p0" target="t0"/><arc source="t0" target="p1"/></page><finalmarkings><marking><place idref="p1"><text>1</text></place></marking></finalmarkings></net></pnml>)labels_fixture",true,std::optional<std::string>{"A"}},
{R"labels_fixture(<pnml><net id="label"><page id="page"><place id="p0"><initialMarking><text>1</text></initialMarking></place><place id="p1"/><transition id="t0"><toolspecific tool="ProM" version="6.4" activity="$invisible$"/></transition><arc source="p0" target="t0"/><arc source="t0" target="p1"/></page><finalmarkings><marking><place idref="p1"><text>1</text></place></marking></finalmarkings></net></pnml>)labels_fixture",true,std::nullopt},
{R"labels_fixture(<pnml><net id="label"><page id="page"><place id="p0"><initialMarking><text>1</text></initialMarking></place><place id="p1"/><transition id="t0"><toolspecific tool="Other" activity="$invisible$"/></transition><arc source="p0" target="t0"/><arc source="t0" target="p1"/></page><finalmarkings><marking><place idref="p1"><text>1</text></place></marking></finalmarkings></net></pnml>)labels_fixture",true,std::optional<std::string>{"t0"}},
{R"labels_fixture(<pnml><net id="label"><page id="page"><place id="p0"><initialMarking><text>1</text></initialMarking></place><place id="p1"/><transition id="t0"><toolspecific activity="$invisible$"/></transition><arc source="p0" target="t0"/><arc source="t0" target="p1"/></page><finalmarkings><marking><place idref="p1"><text>1</text></place></marking></finalmarkings></net></pnml>)labels_fixture",false,std::nullopt},
{R"labels_fixture(<pnml><net id="label"><page id="page"><place id="p0"><initialMarking><text>1</text></initialMarking></place><place id="p1"/><transition id="t0"><name><text>A</text></name><toolspecific tool="ProM" activity="$invisible$"/></transition><arc source="p0" target="t0"/><arc source="t0" target="p1"/></page><finalmarkings><marking><place idref="p1"><text>1</text></place></marking></finalmarkings></net></pnml>)labels_fixture",true,std::nullopt},
{R"labels_fixture(<pnml><net id="label"><page id="page"><place id="p0"><initialMarking><text>1</text></initialMarking></place><place id="p1"/><transition id="t0"><toolspecific tool="ProM" activity="$invisible$"/><name><text>A</text></name></transition><arc source="p0" target="t0"/><arc source="t0" target="p1"/></page><finalmarkings><marking><place idref="p1"><text>1</text></place></marking></finalmarkings></net></pnml>)labels_fixture",true,std::nullopt},
{R"labels_fixture(<pnml><net id="label"><page id="page"><place id="p0"><initialMarking><text>1</text></initialMarking></place><place id="p1"/><transition id="t0"><name><text>A</text></name><toolspecific tool="ProM" activity="A"/></transition><arc source="p0" target="t0"/><arc source="t0" target="p1"/></page><finalmarkings><marking><place idref="p1"><text>1</text></place></marking></finalmarkings></net></pnml>)labels_fixture",true,std::optional<std::string>{"A"}},
{R"labels_fixture(<pnml><net id="label"><page id="page"><place id="p0"><initialMarking><text>1</text></initialMarking></place><place id="p1"/><transition id=""></transition><arc source="p0" target=""/><arc source="" target="p1"/></page><finalmarkings><marking><place idref="p1"><text>1</text></place></marking></finalmarkings></net></pnml>)labels_fixture",true,std::optional<std::string>{""}},
{R"labels_fixture(<pnml><net id="label"><page id="page"><place id="p0"><initialMarking><text>1</text></initialMarking></place><place id="p1"/><transition id=""><name><text>A</text></name></transition><arc source="p0" target=""/><arc source="" target="p1"/></page><finalmarkings><marking><place idref="p1"><text>1</text></place></marking></finalmarkings></net></pnml>)labels_fixture",true,std::optional<std::string>{"A"}},
{R"labels_fixture(<pnml><net id="label"><page id="page"><place id="p0"><initialMarking><text>1</text></initialMarking></place><place id="p1"/><transition id="t0"><name/><name><text>A</text></name></transition><arc source="p0" target="t0"/><arc source="t0" target="p1"/></page><finalmarkings><marking><place idref="p1"><text>1</text></place></marking></finalmarkings></net></pnml>)labels_fixture",true,std::optional<std::string>{"A"}},
{R"labels_fixture(<pnml><net id="label"><page id="page"><place id="p0"><initialMarking><text>1</text></initialMarking></place><place id="p1"/><transition id="t0"><name><text/><text>A</text></name></transition><arc source="p0" target="t0"/><arc source="t0" target="p1"/></page><finalmarkings><marking><place idref="p1"><text>1</text></place></marking></finalmarkings></net></pnml>)labels_fixture",true,std::optional<std::string>{"A"}},
{R"labels_fixture(<pnml><net id="label"><page id="page"><place id="p0"><initialMarking><text>1</text></initialMarking></place><place id="p1"/><transition id="t0"><name>unused</name><name><text>A</text></name></transition><arc source="p0" target="t0"/><arc source="t0" target="p1"/></page><finalmarkings><marking><place idref="p1"><text>1</text></place></marking></finalmarkings></net></pnml>)labels_fixture",true,std::optional<std::string>{"A"}},
{R"labels_fixture(<pnml><net id="label"><page id="page"><place id="p0"><initialMarking><text>1</text></initialMarking></place><place id="p1"/><transition id="t0"><name><text><b/></text><text>A</text></name></transition><arc source="p0" target="t0"/><arc source="t0" target="p1"/></page><finalmarkings><marking><place idref="p1"><text>1</text></place></marking></finalmarkings></net></pnml>)labels_fixture",true,std::optional<std::string>{"A"}},
{R"labels_fixture(<pnml><net id="label"><page id="page"><place id="p0"><initialMarking><text>1</text></initialMarking></place><place id="p1"/><transition id="t0"><toolspecific tool="ProM_extra" activity="$invisible$"/></transition><arc source="p0" target="t0"/><arc source="t0" target="p1"/></page><finalmarkings><marking><place idref="p1"><text>1</text></place></marking></finalmarkings></net></pnml>)labels_fixture",true,std::nullopt},
{R"labels_fixture(<pnml><net id="label"><page id="page"><place id="p0"><initialMarking><text>1</text></initialMarking></place><place id="p1"/><transition id="t0"><toolspecific tool="Custom_ProM_plugin" activity="$invisible$"/></transition><arc source="p0" target="t0"/><arc source="t0" target="p1"/></page><finalmarkings><marking><place idref="p1"><text>1</text></place></marking></finalmarkings></net></pnml>)labels_fixture",true,std::nullopt},
{R"labels_fixture(<pnml><net id="label"><page id="page"><place id="p0"><initialMarking><text>1</text></initialMarking></place><place id="p1"/><transition id="t0"><toolspecific tool="prom" activity="$invisible$"/></transition><arc source="p0" target="t0"/><arc source="t0" target="p1"/></page><finalmarkings><marking><place idref="p1"><text>1</text></place></marking></finalmarkings></net></pnml>)labels_fixture",true,std::optional<std::string>{"t0"}},
{R"labels_fixture(<pnml><net id="label"><page id="page"><place id="p0"><initialMarking><text>1</text></initialMarking></place><place id="p1"/><transition id="t0"><toolspecific/></transition><arc source="p0" target="t0"/><arc source="t0" target="p1"/></page><finalmarkings><marking><place idref="p1"><text>1</text></place></marking></finalmarkings></net></pnml>)labels_fixture",false,std::nullopt},
{R"labels_fixture(<pnml><net id="label"><page id="page"><place id="p0"><initialMarking><text>1</text></initialMarking></place><place id="p1"/><transition id="t0"><name><text>A</text></name><toolspecific activity="A"/></transition><arc source="p0" target="t0"/><arc source="t0" target="p1"/></page><finalmarkings><marking><place idref="p1"><text>1</text></place></marking></finalmarkings></net></pnml>)labels_fixture",false,std::nullopt},
{R"labels_fixture(<pnml><net id="label"><page id="page"><place id="p0"><initialMarking><text>1</text></initialMarking></place><place id="p1"/><transition id="t0"><toolspecific tool="ProM"/></transition><arc source="p0" target="t0"/><arc source="t0" target="p1"/></page><finalmarkings><marking><place idref="p1"><text>1</text></place></marking></finalmarkings></net></pnml>)labels_fixture",false,std::nullopt},
{R"labels_fixture(<pnml><net id="label"><page id="page"><place id="p0"><initialMarking><text>1</text></initialMarking></place><place id="p1"/><transition id="t0"><toolspecific tool="ProM" activity=" $invisible$ "/></transition><arc source="p0" target="t0"/><arc source="t0" target="p1"/></page><finalmarkings><marking><place idref="p1"><text>1</text></place></marking></finalmarkings></net></pnml>)labels_fixture",true,std::nullopt},
{R"labels_fixture(<pnml><net id="label"><page id="page"><place id="p0"><initialMarking><text>1</text></initialMarking></place><place id="p1"/><transition id="t0"><toolspecific tool="ProM" activity=""/></transition><arc source="p0" target="t0"/><arc source="t0" target="p1"/></page><finalmarkings><marking><place idref="p1"><text>1</text></place></marking></finalmarkings></net></pnml>)labels_fixture",true,std::optional<std::string>{"t0"}},
{R"labels_fixture(<pnml><net id="label"><page id="page"><place id="p0"><initialMarking><text>1</text></initialMarking></place><place id="p1"/><transition id="t0"><toolspecific tool="ProM" activity="$invisible$"/><toolspecific tool="ProM" activity="A"/></transition><arc source="p0" target="t0"/><arc source="t0" target="p1"/></page><finalmarkings><marking><place idref="p1"><text>1</text></place></marking></finalmarkings></net></pnml>)labels_fixture",true,std::nullopt},
{R"labels_fixture(<pnml><net id="label"><page id="page"><place id="p0"><initialMarking><text>1</text></initialMarking></place><place id="p1"/><transition id="t0"><toolspecific tool="ProM" activity="A"/><toolspecific tool="ProM" activity="$invisible$"/></transition><arc source="p0" target="t0"/><arc source="t0" target="p1"/></page><finalmarkings><marking><place idref="p1"><text>1</text></place></marking></finalmarkings></net></pnml>)labels_fixture",true,std::nullopt}
  };
  for (const auto& control : controls) {
    PnmlImportOptions options;
    options.infer_initial_marking = options.infer_final_marking = options.require_workflow_boundaries = false;
    std::optional<PnmlDocument> document;
    bool rejected = false;
    try { document = parse_pnml_document(control.xml,options); }
    catch (const std::runtime_error&) { rejected = true; }
    assert(rejected == !control.imported);
    if (!document) continue;
    const auto presentation = to_petri_net_presentation(*document);
    assert(presentation.transitions.size() == 1);
    assert(presentation.transitions.front().label == control.label);
    const auto restored = parse_pnml_document(to_pnml(*document),options);
    assert(restored.transition_ids == document->transition_ids);
    assert(to_petri_net_presentation(restored).transitions.front().label == control.label);
    EventLog log;
    Trace trace; trace.case_id = "owned-label";
    if (control.label) {
      Event event; event.activity = *control.label;
      event.activity_state = FirstClassAttributeState::kPresent;
      event.activity_source_key = "concept:name";
      event.activity_attribute = EventAttribute{"concept:name",*control.label,AttributeKind::kString,false};
      trace.events.push_back(std::move(event));
    }
    log.traces.push_back(std::move(trace));
    const auto diagnostics = replay_token_traces(log,document->net,
        minera::tools::pnml_replay_options(*document,{}));
    assert(diagnostics.size() == 1 && diagnostics.front().trace_is_fit);
    assert(diagnostics.front().trace_fitness == 1.0);
    assert(diagnostics.front().activated_transitions == std::vector<std::size_t>{0});
    assert(diagnostics.front().produced == 2 && diagnostics.front().consumed == 2);
    assert(diagnostics.front().missing == 0 && diagnostics.front().remaining == 0);
  }
}

void pnml_exact_public_markings() {
  const std::string prefix = "<pnml><net id=\"marked\"><place id=\"source\">";
  const std::string graph = "</place><place id=\"spare\"/><place id=\"target\"/>"
      "<transition id=\"move\"><name><text>A</text></name></transition>"
      "<arc source=\"source\" target=\"move\"/><arc source=\"move\" target=\"target\"/>";
  PnmlImportOptions observed;
  observed.public_marking_semantics = true;
  const auto sparse = parse_pnml_document(prefix +
      "<initialMarking><text>2</text></initialMarking>"
      "<initialMarking><text>-1</text></initialMarking>" + graph +
      "<finalmarkings><marking><place idref=\"target\"><text>2</text></place>"
      "<place idref=\"target\"><text>0</text></place></marking>"
      "<marking><place idref=\"spare\"><text>3</text></place>"
      "<place idref=\"target\"><text>-3</text></place></marking></finalmarkings></net></pnml>", observed);
  assert((sparse.initial_marking == TokenMarking{0, 0, 0}));
  assert((sparse.initial_marking_keys == std::vector<bool>{false, false, false}));
  assert((sparse.final_marking == TokenMarking{0, 3, 2}));
  assert((sparse.final_marking_keys == std::vector<bool>{false, true, true}));
  const auto absent = parse_pnml_document(prefix + graph + "</net></pnml>", observed);
  assert((absent.initial_marking == TokenMarking{0, 0, 0}));
  assert((absent.final_marking == TokenMarking{0, 1, 1}));
  const auto empty = parse_pnml_document(prefix + graph +
      "<finalmarkings/></net></pnml>", observed);
  assert((empty.final_marking == TokenMarking{0, 0, 0}));
  const auto lexical = parse_pnml_document(prefix +
      "<initialMarking><text>\xc2\xa0+\xd9\xa2_\xd9\xa3\xe2\x80\x83</text></initialMarking>" + graph +
      "<finalmarkings><marking><place idref=\"target\"><text>1_024</text></place>"
      "</marking></finalmarkings></net></pnml>", observed);
  assert(lexical.initial_marking[0] == 23 && lexical.final_marking[2] == 1024);

  const std::string huge = "1" + std::string(400, '0') + "1";
  const auto exact = parse_pnml_document(prefix +
      "<initialMarking><text>" + huge + "</text></initialMarking>" + graph +
      "<finalmarkings><marking><place idref=\"target\"><text>" + huge +
      "</text></place></marking></finalmarkings></net></pnml>", observed);
  assert(exact.initial_marking[0] == TokenInteger(huge));
  assert(exact.final_marking[2] == TokenInteger(huge));
  const auto restored = parse_pnml_document(to_pnml(exact), observed);
  assert(restored.initial_marking == exact.initial_marking && restored.final_marking == exact.final_marking);
  bool bounded = false;
  try { (void)minera::tools::pnml_marking<std::int64_t>(exact.initial_marking); }
  catch (const std::invalid_argument& error) {
    bounded = std::string(error.what()) == "PNML marking exceeds supported token range";
  }
  assert(bounded);
  const auto no_traces = evaluate_replay_fitness(EventLog{}, exact, ReplayFitnessVariant::TOKEN_BASED);
  assert(no_traces.computed && no_traces.log_fitness == 0.0 && no_traces.log_fitness_is_integer_zero);
  bool overflow = false;
  try { (void)evaluate_replay_fitness(log_of({{"A"}}), exact, ReplayFitnessVariant::TOKEN_BASED); }
  catch (const std::overflow_error& error) {
    overflow = std::string(error.what()) == "int too large to convert to float";
  }
  assert(overflow);
  bool empty_text = false;
  try { (void)parse_pnml_document(prefix + "<initialMarking><text/></initialMarking>" + graph + "</net></pnml>", observed); }
  catch (const std::runtime_error& error) {
    empty_text = std::string(error.what()) == "int() argument must be a string, a bytes-like object or a real number, not 'NoneType'";
  }
  assert(empty_text);
  const auto invalid = [&](const std::string& text) {
    try { (void)parse_pnml_document(prefix + "<initialMarking><text>" + text + "</text></initialMarking>" + graph + "</net></pnml>", observed); }
    catch (const std::runtime_error& error) { return std::string(error.what()); }
    return std::string{};
  };
  assert(invalid("1.0") == "invalid literal for int() with base 10: '1.0'");
  assert(invalid("&apos;1") == "invalid literal for int() with base 10: \"'1\"");
  assert(invalid("1&#13;X") == "invalid literal for int() with base 10: '1\\rX'");
  assert(invalid("1\rX") == "invalid literal for int() with base 10: '1\\nX'");
  assert(invalid(std::string(201, 'x')) == "invalid literal for int() with base 10: '" + std::string(199, 'x'));
  assert(invalid(std::string(4301, '0')) == "Exceeds the limit (4300 digits) for integer string conversion: value has 4301 digits; use sys.set_int_max_str_digits() to increase the limit");
  const auto boundary = parse_pnml_document(prefix + "<initialMarking><text>" + std::string(4300, '0') + "</text></initialMarking>" + graph + "</net></pnml>", observed);
  assert(boundary.initial_marking[0] == 0 && !boundary.initial_marking_keys[0]);
  const auto joined = parse_pnml_document(prefix + "<initialMarking><text>1<!-- join --><![CDATA[2]]></text></initialMarking>" + graph + "</net></pnml>", observed);
  assert(joined.initial_marking[0] == 12);
  const auto first = parse_pnml_document(prefix + "<initialMarking><text>1<child/>2</text></initialMarking>" + graph + "</net></pnml>", observed);
  assert(first.initial_marking[0] == 1);
  PnmlImportOptions native;
  native.infer_initial_marking = native.infer_final_marking = native.require_workflow_boundaries = false;
  const auto native_empty = parse_pnml_document(prefix + "<initialMarking><text/></initialMarking>" + graph + "<finalmarkings><marking><place idref=\"target\"><text/></place></marking></finalmarkings></net></pnml>", native);
  assert(native_empty.initial_marking[0] == 0 && native_empty.initial_marking_keys[0]);
  assert(native_empty.final_marking[2] == 1 && native_empty.final_marking_keys[2]);
}

}  // namespace

int main() {
  pnml_exact_public_markings();
  reset_and_inhibitor_enablement();
  playout_and_pnml_round_trip();
  weighted_arc_round_trip();
  import_options_and_stochastic_metadata();
  general_pnml_preserves_empty_markings();
  pnml_marking_key_presence();
  pnml_document_export_contract();
  pnml_document_export_rejects_atomically();
  pnml_document_export_retains_routing_order();
  pnml_document_export_preserves_xml_whitespace();
  pnml_mixed_input_arc_occurrences();
  pnml_document_preserves_arc_occurrences();
  equation_guard_and_workflow_topology();
  normal_alignment_smoke();
  pnml_arc_type_wire_contract();
  pnml_extended_weight_and_text_contract();
  pnml_compact_ordinary_weight_contract();
  pnml_transition_label_contract();
}
