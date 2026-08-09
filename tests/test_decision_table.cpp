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

  bool threw = false;
  try {
    minera::DecisionTableOptions invalid;
    invalid.lookback = 0;
    (void)minera::mine_decision_tables(sample_log(), branching_net(), invalid);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  const auto json = minera::serialize_decision_tables_json(result);
  assert(json.find("DECISION_TABLES") != std::string::npos);
  assert(json.find("feature_names") != std::string::npos);
  assert(json.find("region=north") != std::string::npos);
  return 0;
}
