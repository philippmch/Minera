#include <cassert>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#include "minera/decision_table.hpp"

namespace {

minera::Event event(std::string activity, std::string amount,
                    std::string region = {}) {
  minera::Event result;
  result.activity = std::move(activity);
  result.timestamp = "2024-01-01T00:00:00Z";
  if (!amount.empty()) result.attributes.push_back({"amount", std::move(amount)});
  if (!region.empty()) result.attributes.push_back({"region", std::move(region)});
  return result;
}

minera::PetriNet branching_net() {
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

minera::PetriNet two_step_branching_net() {
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto after_a = net.add_place();
  const auto after_b = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({"A", "B", "C", "D"});
  net.add_transition(0, {source}, {after_a});
  net.add_transition(1, {after_a}, {after_b});
  net.add_transition(2, {after_b}, {sink});
  net.add_transition(3, {after_b}, {sink});
  return net;
}

minera::PetriNet three_step_branching_net() {
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto after_a = net.add_place();
  const auto after_b = net.add_place();
  const auto after_c = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({"A", "B", "C", "D", "E"});
  net.add_transition(0, {source}, {after_a});
  net.add_transition(1, {after_a}, {after_b});
  net.add_transition(2, {after_b}, {after_c});
  net.add_transition(3, {after_c}, {sink});
  net.add_transition(4, {after_c}, {sink});
  return net;
}

minera::EventLog two_step_log() {
  minera::EventLog log;
  minera::Trace trace;
  trace.case_id = "chain";
  trace.events.push_back(event("A", "10"));
  trace.events.push_back(event("B", "20"));
  // This value follows the choice and must not enter either context row.
  trace.events.push_back(event("C", "999"));
  log.traces.push_back(std::move(trace));
  return log;
}

minera::EventLog three_step_log() {
  minera::EventLog log;
  minera::Trace trace;
  trace.case_id = "wrap";
  trace.events.push_back(event("A", "10"));
  trace.events.push_back(event("B", "20"));
  trace.events.push_back(event("C", "30"));
  trace.events.push_back(event("D", "999"));
  log.traces.push_back(std::move(trace));
  return log;
}

minera::EventLog sample_log() {
  minera::EventLog log;
  log.traces.push_back({"c1", {event("A", "10", "north"), event("B", "20")}, {}});
  log.traces.push_back({"c2", {event("A", "30", "south"), event("C", "40")}, {}});
  log.traces.push_back({"c3", {event("A", "50", "north"), event("B", "60")}, {}});
  log.traces.push_back({"bad", {event("A", "70", "north"), event("D", "80")}, {}});
  return log;
}

}  // namespace

int main() {
  const auto result = minera::mine_decision_tables(sample_log(), branching_net());
  assert(result.trace_count == 4);
  assert(result.observed_traces == 3);
  assert(result.tables.size() == 1);
  const auto& table = result.tables.front();
  assert(table.classes.size() == 2);
  assert(table.rows.size() == 3);
  assert(table.feature_names.size() == 3);
  assert(table.feature_names[0] == "amount");
  assert(table.feature_names[1] == "region=north");
  assert(table.feature_names[2] == "region=south");
  assert(table.rows[0].features[0] && *table.rows[0].features[0] == 10.0);
  assert(table.rows[0].features[1] && *table.rows[0].features[1] == 1.0);
  assert(table.rows[1].target == 1);

  minera::DecisionTableOptions selected;
  selected.attributes = {"region"};
  const auto selected_result =
      minera::mine_decision_tables(sample_log(), branching_net(), selected);
  assert(selected_result.tables.front().feature_names.size() == 2);

  minera::DecisionTableOptions with_trace;
  with_trace.include_trace_attributes = true;
  with_trace.lookback = 1;
  const auto with_trace_result =
      minera::mine_decision_tables(sample_log(), branching_net(), with_trace);
  assert(with_trace_result.tables.size() == 1);

  with_trace.mining.max_depth = 0;
  const auto shallow =
      minera::mine_decision_tables(sample_log(), branching_net(), with_trace);
  assert(shallow.tables.size() == 1);

  bool threw = false;
  try {
    minera::DecisionTableOptions invalid;
    invalid.lookback = 0;
    (void)minera::mine_decision_tables(sample_log(), branching_net(), invalid);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  threw = false;
  try {
    minera::DecisionTableOptions invalid;
    invalid.mining.min_leaf = 0;
    (void)minera::mine_decision_tables(sample_log(), branching_net(), invalid);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  minera::DecisionTableOptions invalid;
  invalid.lookback = minera::kMaxDecisionLookback + 1;
  threw = false;
  try {
    (void)minera::mine_decision_tables(sample_log(), branching_net(), invalid);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  minera::DecisionTableOptions one;
  one.attributes = {"amount"};
  const auto one_step = minera::mine_decision_tables(
      two_step_log(), two_step_branching_net(), one);
  assert(one_step.tables.size() == 1);
  assert(one_step.tables.front().rows.size() == 1);
  assert(one_step.tables.front().rows.front().features.size() == 1);
  assert(one_step.tables.front().rows.front().features.front());
  assert(*one_step.tables.front().rows.front().features.front() == 20.0);

  minera::DecisionTableOptions two = one;
  two.lookback = 2;
  const auto two_step = minera::mine_decision_tables(
      two_step_log(), two_step_branching_net(), two);
  assert(two_step.tables.size() == 1);
  assert(two_step.tables.front().rows.size() == 2);
  assert(two_step.tables.front().rows[0].features.front());
  assert(two_step.tables.front().rows[1].features.front());
  assert(*two_step.tables.front().rows[0].features.front() == 10.0);
  assert(*two_step.tables.front().rows[1].features.front() == 20.0);
  for (const auto& row : two_step.tables.front().rows) {
    assert(!row.features.empty());
    assert(!row.features.front() || *row.features.front() != 999.0);
  }

  minera::EventLog stale_log;
  minera::Trace stale_trace;
  stale_trace.case_id = "stale";
  stale_trace.events.push_back(event("A", "10"));
  stale_trace.events.push_back(event("B", ""));
  stale_trace.events.push_back(event("C", "999"));
  stale_log.traces.push_back(std::move(stale_trace));
  const auto stale = minera::mine_decision_tables(
      stale_log, two_step_branching_net(), two);
  assert(stale.tables.front().rows.size() == 2);
  assert(stale.tables.front().rows[0].features.front());
  assert(*stale.tables.front().rows[0].features.front() == 10.0);
  assert(!stale.tables.front().rows[1].features.front());

  const auto wrapped = minera::mine_decision_tables(
      three_step_log(), three_step_branching_net(), two);
  assert(wrapped.tables.size() == 1);
  assert(wrapped.tables.front().rows.size() == 2);
  assert(wrapped.tables.front().rows[0].features.front());
  assert(wrapped.tables.front().rows[1].features.front());
  assert(*wrapped.tables.front().rows[0].features.front() == 30.0);
  assert(*wrapped.tables.front().rows[1].features.front() == 20.0);

  const auto json = minera::serialize_decision_tables_json(result);
  assert(json.find("DECISION_TABLES") != std::string::npos);
  assert(json.find("feature_names") != std::string::npos);
  assert(json.find("region=north") != std::string::npos);
  return 0;
}
