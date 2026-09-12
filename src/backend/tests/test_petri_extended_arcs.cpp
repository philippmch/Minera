#include "minera/conformance.hpp"
#include "minera/ilp_miner.hpp"
#include "minera/json_export.hpp"
#include "minera/petri_analysis.hpp"
#include "minera/pnml.hpp"
#include "minera/playout.hpp"

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

  bool rejected = false;
  try {
    (void)parse_pnml_document(R"(<pnml><net id="n"><page id="p">
      <place id="p0"><initialMarking><text>1</text></initialMarking></place>
      <place id="p1"/><transition id="t0"/>
      <arc id="a0" source="p0" target="t0"><inscription><text>0</text></inscription></arc>
      <arc id="a1" source="t0" target="p1"/>
      <finalmarkings><marking><place idref="p1"><text>1</text></place></marking></finalmarkings>
    </page></net></pnml>)");
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  assert(rejected);

  rejected = false;
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
  invalid = valid; invalid.place_ids = {"", "two"}; rejects(invalid);
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

}  // namespace

int main() {
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
  equation_guard_and_workflow_topology();
  normal_alignment_smoke();
}
