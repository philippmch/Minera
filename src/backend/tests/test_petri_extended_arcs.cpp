#include "minera/conformance.hpp"
#include "minera/ilp_miner.hpp"
#include "minera/json_export.hpp"
#include "minera/petri_analysis.hpp"
#include "minera/pnml.hpp"
#include "minera/playout.hpp"

#include <cassert>
#include <initializer_list>
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
  equation_guard_and_workflow_topology();
  normal_alignment_smoke();
}
