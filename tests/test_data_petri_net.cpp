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

minera::EventLog two_predecessor_log() {
  minera::EventLog log;
  for (const auto& [case_id, route, first, second] :
       std::vector<std::tuple<std::string, std::string, std::string, std::string>>{
           {"c1", "C", "10", "20"}, {"c2", "C", "30", "40"},
           {"c3", "D", "50", "60"}, {"c4", "D", "70", "80"}}) {
    minera::Trace trace;
    trace.case_id = case_id;
    minera::Event a{case_id, "A", "", "", 0, {}, ""};
    a.attributes.push_back({"amount", first});
    minera::Event b{case_id, "B", "", "", 1, {}, ""};
    b.attributes.push_back({"amount", second});
    minera::Event branch{case_id, route, "", "", 2, {}, ""};
    trace.events = {std::move(a), std::move(b), std::move(branch)};
    log.traces.push_back(std::move(trace));
  }
  return log;
}

minera::PetriNet two_predecessor_net() {
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto after_a = net.add_place();
  const auto choice = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({"A", "B", "C", "D"});
  net.add_transition(0, {source}, {after_a});
  net.add_transition(1, {after_a}, {choice});
  net.add_transition(2, {choice}, {sink});
  net.add_transition(3, {choice}, {sink});
  return net;
}

void mines_guards_and_serializes() {
  const auto result = minera::mine_data_petri_net(decision_log(), decision_net());
  assert(result.decisions.observed_traces == 4);
  assert(!result.decisions.decision_points.empty());
  assert(result.decision_tables.tables.size() == 1);
  assert(!result.guarded_transitions.empty());
  assert(result.guarded_transitions.front().transition_index == 1 ||
         result.guarded_transitions.front().transition_index == 2);
  const auto json = minera::serialize_data_petri_net_json(result, "test");
  assert(json.find("\"family\":\"DATA_PETRI_NET\"") != std::string::npos);
  assert(json.find("\"guard\":") != std::string::npos);
  assert(json.find("\"decisions\":") != std::string::npos);
  assert(json.find("\"decision_tables\":") != std::string::npos);
}

void shares_decision_controls_with_guards() {
  minera::DataPetriNetOptions options;
  options.decision_options.max_depth = 0;
  options.decision_options.include_trace_attributes = false;
  options.decision_options.attributes = {"customer"};
  const auto result =
      minera::mine_data_petri_net(decision_log(), decision_net(), options);
  assert(result.decisions.decision_points.size() == 1);
  assert(result.decisions.decision_points.front().nodes.size() == 1);
  assert(result.guarded_transitions.size() == 1);
  assert(!result.guarded_transitions.front().guard.empty());
  assert(result.guarded_transitions.front().guard.find("amount") ==
         std::string::npos);
  assert(result.guarded_transitions.front().read_variables.empty());
  assert(result.decision_tables.tables.size() == 1);
}

void propagates_table_lookback() {
  minera::DataPetriNetOptions options;
  options.decision_options.min_split = 2;
  options.decision_options.min_leaf = 1;
  options.decision_options.attributes = {"amount"};
  const auto one =
      minera::mine_data_petri_net(two_predecessor_log(), two_predecessor_net(),
                                  options);
  options.decision_table_lookback = 2;
  const auto two =
      minera::mine_data_petri_net(two_predecessor_log(), two_predecessor_net(),
                                  options);
  assert(one.decision_tables.tables.size() == 1);
  assert(two.decision_tables.tables.size() == 1);
  assert(one.decision_tables.tables.front().rows.size() == 4);
  assert(two.decision_tables.tables.front().rows.size() == 8);
}

}  // namespace

int main() {
  mines_guards_and_serializes();
  shares_decision_controls_with_guards();
  propagates_table_lookback();
  return 0;
}
