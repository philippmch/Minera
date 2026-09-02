#include "minera/dfg_precision.hpp"

#include <cassert>
#include <cmath>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace minera;

EventLog log(std::initializer_list<std::vector<std::string>> traces) {
  EventLog result;
  std::size_t order = 0;
  std::size_t case_number = 0;
  for (const auto& activities : traces) {
    Trace trace{"case" + std::to_string(case_number++), {}, {}};
    for (const auto& activity : activities)
      trace.events.push_back(
          Event{trace.case_id, activity, "", "", order++, {}, ""});
    result.traces.push_back(std::move(trace));
  }
  return result;
}

DfgCleanAnalysis model(
    std::initializer_list<std::pair<std::string, std::string>> edges,
    std::initializer_list<std::string> starts,
    std::initializer_list<std::string> ends) {
  DfgCleanAnalysis result;
  for (const auto& edge : edges) result.graph[edge] = 1;
  for (const auto& activity : starts) result.start_activities[activity] = 1;
  for (const auto& activity : ends) result.end_activities[activity] = 1;
  return result;
}

int main() {
  const auto precise = model({{"A", "B"}, {"A", "C"}}, {"A"}, {"B", "C"});
  const auto perfect = evaluate_dfg_precision(log({{"A", "B"}, {"A", "C"}}), precise);
  assert(perfect.computed && perfect.precision == 1.0);
  assert(perfect.escaping_transitions == 0);
  assert(perfect.activated_transitions == 6);

  const auto escaping_model = model({{"A", "B"}, {"A", "C"}}, {"A"}, {"B"});
  const auto escaping = evaluate_dfg_precision(
      log({{"A", "B"}, {"A", "B"}}), escaping_model);
  assert(escaping.computed);
  assert(std::abs(escaping.precision - (2.0 / 3.0)) < 1e-12);
  assert(escaping.prefix_occurrences == 4);
  assert(escaping.allowed_prefix_occurrences == 4);
  assert(escaping.activated_transitions == 6);
  assert(escaping.escaping_transitions == 2);
  const auto json = serialize_dfg_precision_json(escaping);
  assert(json.find("DFG_PRECISION") != std::string::npos);

  const auto blocked = evaluate_dfg_precision(
      log({{"X", "B"}}), model({{"A", "B"}}, {"A"}, {}));
  assert(blocked.precision == 0.0);
  assert(blocked.activated_transitions == 1);
  assert(blocked.escaping_transitions == 1);

  EventLog custom = log({{"A", "B"}});
  custom.traces.front().events[0].attributes.push_back(
      {"label", "A", AttributeKind::kString});
  custom.traces.front().events[1].attributes.push_back(
      {"label", "B", AttributeKind::kString});
  const auto custom_result = evaluate_dfg_precision(
      custom, model({{"A", "B"}}, {"A"}, {}),
      DfgPrecisionOptions{"label"});
  assert(custom_result.precision == 1.0);

  // Imported graph maps may retain declared relations with a zero frequency.
  // Precision is relation-based, so those entries still define the model's
  // enabled edges and starts.
  DfgCleanAnalysis zero_frequency;
  zero_frequency.graph[{"A", "B"}] = 0;
  zero_frequency.start_activities["A"] = 0;
  const auto zero_frequency_result = evaluate_dfg_precision(
      log({{"A", "B"}}), zero_frequency);
  assert(zero_frequency_result.computed);
  assert(zero_frequency_result.precision == 1.0);
  assert(zero_frequency_result.activated_transitions == 2);
  assert(zero_frequency_result.escaping_transitions == 0);

  bool threw = false;
  try {
    (void)evaluate_dfg_precision(log({{"A"}}), precise,
                                 DfgPrecisionOptions{"missing"});
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
  return 0;
}
