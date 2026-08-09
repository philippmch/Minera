#include "minera/data_petri_net.hpp"

#include <cassert>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace {

minera::EventLog decision_log() {
  minera::EventLog log;
  for (const auto& [case_id, route, amount] :
       std::vector<std::tuple<std::string, std::string, std::string>>{
           {"c1", "B", "10"}, {"c2", "B", "20"}, {"c3", "C", "90"},
           {"c4", "C", "100"}}) {
    minera::Trace trace;
    trace.case_id = case_id;
    trace.attributes.push_back({"customer", "retail"});
    minera::Event start{case_id, "A", "", "", 0, {}, ""};
    start.attributes.push_back({"amount", amount});
    minera::Event branch{case_id, route, "", "", 1, {}, ""};
    trace.events = {std::move(start), std::move(branch)};
    log.traces.push_back(std::move(trace));
  }
  return log;
}

minera::PetriNet decision_net() {
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto choice = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({"A", "B", "C"});
  net.add_transition(0, {source}, {choice});
  net.add_transition(1, {choice}, {sink});
  net.add_transition(2, {choice}, {sink});
  return net;
}

void mines_guards_and_serializes() {
  const auto result = minera::mine_data_petri_net(decision_log(), decision_net());
  assert(result.decisions.observed_traces == 4);
  assert(!result.decisions.decision_points.empty());
  assert(!result.guarded_transitions.empty());
  assert(result.guarded_transitions.front().transition_index == 1 ||
         result.guarded_transitions.front().transition_index == 2);
  const auto json = minera::serialize_data_petri_net_json(result, "test");
  assert(json.find("\"family\":\"DATA_PETRI_NET\"") != std::string::npos);
  assert(json.find("\"guard\":") != std::string::npos);
  assert(json.find("\"decisions\":") != std::string::npos);
}

}  // namespace

int main() {
  mines_guards_and_serializes();
  return 0;
}
