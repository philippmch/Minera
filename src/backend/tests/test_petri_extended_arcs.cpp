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
  bool rejected = false;
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
  equation_guard_and_workflow_topology();
  normal_alignment_smoke();
}
