#include "minera/genetic_miner.hpp"
#include "minera/json_export.hpp"

#include <algorithm>
#include <cassert>
#include <limits>
#include <stdexcept>

namespace {

minera::EventLog chain_log() {
  minera::EventLog log;
  log.traces.push_back({"c1", {{"c1", "A", "2020-01-01T00:00:00Z", "", 0, {}, ""},
                               {"c1", "B", "2020-01-01T00:01:00Z", "", 1, {}, ""}}});
  log.traces.push_back({"c2", {{"c2", "A", "2020-01-02T00:00:00Z", "", 0, {}, ""},
                               {"c2", "B", "2020-01-02T00:01:00Z", "", 1, {}, ""}}});
  return log;
}

minera::EventLog disconnected_log() {
  minera::EventLog log;
  log.traces.push_back({"x", {{"x", "A", "2020-01-01T00:00:00Z", "", 0, {}, ""}}});
  log.traces.push_back({"y", {{"y", "B", "2020-01-02T00:00:00Z", "", 0, {}, ""}}});
  return log;
}

minera::EventLog custom_fields_log() {
  minera::EventLog log;
  minera::Trace first;
  first.case_id = "source-1";
  first.attributes.push_back({"case_ref", "merged", minera::AttributeKind::kString});
  first.events.push_back({"source-1", "B", "", "", 0,
                          {{"occurred_at", "2020-01-01T00:01:00Z"}}, ""});
  first.events.push_back({"source-1", "A", "", "", 1,
                          {{"occurred_at", "2020-01-01T00:00:00Z"}}, ""});
  minera::Trace second;
  second.case_id = "source-2";
  second.attributes.push_back({"case_ref", "merged", minera::AttributeKind::kString});
  second.events.push_back({"source-2", "B", "", "", 0,
                           {{"occurred_at", "2020-01-02T00:01:00Z"}}, ""});
  second.events.push_back({"source-2", "A", "", "", 1,
                           {{"occurred_at", "2020-01-02T00:00:00Z"}}, ""});
  log.traces = {std::move(first), std::move(second)};
  return log;
}

minera::EventLog interleaved_event_case_log() {
  minera::EventLog log;
  minera::Trace trace;
  trace.case_id = "source";
  trace.events.push_back({"source", "A", "", "", 0,
                          {{"case_ref", "x"},
                           {"occurred_at", "2020-01-01T00:00:00Z"}}, ""});
  trace.events.push_back({"source", "B", "", "", 1,
                          {{"case_ref", "y"},
                           {"occurred_at", "2020-01-01T00:01:00Z"}}, ""});
  trace.events.push_back({"source", "A", "", "", 2,
                          {{"case_ref", "x"},
                           {"occurred_at", "2020-01-01T00:02:00Z"}}, ""});
  trace.events.push_back({"source", "B", "", "", 3,
                          {{"case_ref", "y"},
                           {"occurred_at", "2020-01-01T00:03:00Z"}}, ""});
  log.traces.push_back(std::move(trace));
  return log;
}

minera::EventLog explicit_event_case_log() {
  minera::EventLog log;
  minera::Trace x;
  x.case_id = "x";
  x.events = {{"x", "A", "", "", 0,
               {{"case_ref", "x"}, {"occurred_at", "2020-01-01T00:00:00Z"}}, ""},
              {"x", "A", "", "", 1,
               {{"case_ref", "x"}, {"occurred_at", "2020-01-01T00:01:00Z"}}, ""}};
  minera::Trace y;
  y.case_id = "y";
  y.events = {{"y", "B", "", "", 0,
               {{"case_ref", "y"}, {"occurred_at", "2020-01-02T00:00:00Z"}}, ""},
              {"y", "B", "", "", 1,
               {{"case_ref", "y"}, {"occurred_at", "2020-01-02T00:01:00Z"}}, ""}};
  log.traces = {std::move(x), std::move(y)};
  return log;
}

minera::EventLog equal_timestamp_log(bool reverse_document_order) {
  minera::EventLog log;
  minera::Trace trace;
  trace.case_id = "equal";
  const minera::Event a{"equal", "A", "", "", 0,
                        {{"occurred_at", "2020-01-01T00:00:00Z"}}, ""};
  const minera::Event b{"equal", "B", "", "", 1,
                        {{"occurred_at", "2020-01-01T00:00:00Z"}}, ""};
  trace.events = reverse_document_order ? std::vector<minera::Event>{b, a}
                                        : std::vector<minera::Event>{a, b};
  log.traces.push_back(std::move(trace));
  return log;
}

minera::EventLog mixed_activity_attribute_log() {
  minera::EventLog log;
  minera::Trace trace;
  trace.case_id = "mixed";
  trace.events.push_back({"mixed", "A", "2020-01-01T00:00:00Z", "", 0,
                          {{"phase", "Start"},
                           {"occurred_at", "2020-01-01T00:00:00Z"}}, ""});
  trace.events.push_back({"mixed", "B", "2020-01-01T00:01:00Z", "", 1,
                          {{"occurred_at", "2020-01-01T00:01:00Z"}}, ""});
  log.traces.push_back(std::move(trace));
  return log;
}

}  // namespace

int main() {
  minera::GeneticMinerOptions options;
  options.seed = 17;
  options.population_size = 8;
  options.generations = 4;
  options.max_states = 128;
  const auto first = minera::mine_genetic(chain_log(), options);
  const auto second = minera::mine_genetic(chain_log(), options);
  assert(first.diagnostics.computed);
  assert(first.net.activities() == second.net.activities());
  assert(first.net.place_count() == second.net.place_count());
  assert(first.net.transitions().size() == second.net.transitions().size());
  assert(first.diagnostics.best_fitness == second.diagnostics.best_fitness);
  assert(first.diagnostics.best_fitness > 0.99);
  assert(first.net.activities().size() == 2);
  assert(first.net.source() < first.net.place_count());
  assert(first.net.sink() < first.net.place_count());
  bool saw_a = false, saw_b = false;
  for (const auto& transition : first.net.transitions()) {
    if (transition.activity && *transition.activity == 0) saw_a = true;
    if (transition.activity && *transition.activity == 1) saw_b = true;
  }
  assert(saw_a && saw_b);

  options.population_size = 6;
  options.generations = 2;
  const auto repaired = minera::mine_genetic(disconnected_log(), options);
  assert(repaired.diagnostics.computed);
  assert(repaired.net.activities().size() == 2);
  for (const auto& transition : repaired.net.transitions()) {
    if (transition.activity) {
      assert(!transition.inputs.empty());
      assert(!transition.outputs.empty());
    }
  }

  options.max_states = 1;
  const auto capped = minera::mine_genetic(chain_log(), options);
  assert(capped.diagnostics.exhausted);
  assert(capped.diagnostics.candidates_evaluated == 1);

  bool threw = false;
  options.max_states = 128;
  options.population_size = 1;
  try { (void)minera::mine_genetic(chain_log(), options); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  threw = false;
  try { (void)minera::mine_genetic({}, options); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  auto custom_options = options;
  custom_options.population_size = 4;
  custom_options.generations = 1;
  custom_options.max_states = 16;
  custom_options.timestamp_key = "occurred_at";
  custom_options.case_id_key = "case_ref";
  const auto custom = minera::mine_genetic(custom_fields_log(), custom_options);
  assert(custom.diagnostics.computed);
  assert(custom.diagnostics.activity_count == 2);
  // The merged case is A,B,A,B; the reference fitness of the best of four
  // individuals after one generation stays well below one.
  assert(custom.diagnostics.best_fitness > 0.0 && custom.diagnostics.best_fitness < 1.0);
  assert(custom.diagnostics.best_fitness ==
         minera::mine_genetic(custom_fields_log(), custom_options).diagnostics.best_fitness);
  assert(custom.net.transitions().size() == custom.net.activities().size());
  assert(custom.markings.initial[custom.net.source()] == 1);
  assert(custom.markings.final[custom.net.sink()] == 1);
  auto standard_case = custom_options;
  standard_case.case_id_key = "case:concept:name";
  const auto standard_case_result =
      minera::mine_genetic(custom_fields_log(), standard_case);
  assert(standard_case_result.diagnostics.computed);
  auto event_case_options = custom_options;
  event_case_options.case_id_key = "case_ref";
  const auto split = minera::mine_genetic(interleaved_event_case_log(),
                                          event_case_options);
  const auto expected = minera::mine_genetic(explicit_event_case_log(),
                                             event_case_options);
  assert(minera::genetic_miner_json(split) ==
         minera::genetic_miner_json(expected));

  auto equal_timestamp_options = custom_options;
  equal_timestamp_options.case_id_key.clear();
  equal_timestamp_options.population_size = 6;
  const auto ordered = minera::mine_genetic(
      equal_timestamp_log(false), equal_timestamp_options);
  const auto reversed_document = minera::mine_genetic(
      equal_timestamp_log(true), equal_timestamp_options);
  assert(minera::genetic_miner_json(ordered) ==
         minera::genetic_miner_json(reversed_document));

  auto mixed_activity_options = custom_options;
  mixed_activity_options.case_id_key.clear();
  mixed_activity_options.activity_key = "phase";
  // An event without the selected field is rejected rather than relabeled
  // with its canonical activity; the reference fails on such a log as well.
  threw = false;
  try { (void)minera::mine_genetic(mixed_activity_attribute_log(), mixed_activity_options); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  // A present empty selected value stays a visible "" label (reference
  // labels for stage.code INTAKE and "": ['', 'INTAKE']).
  auto empty_label_log = mixed_activity_attribute_log();
  empty_label_log.traces[0].events[1].attributes.push_back({"phase", ""});
  const auto empty_label = minera::mine_genetic(empty_label_log, mixed_activity_options);
  assert(empty_label.net.activities().size() == 2);
  assert(std::count(empty_label.net.activities().begin(), empty_label.net.activities().end(), "") == 1);
  assert(std::count(empty_label.net.activities().begin(), empty_label.net.activities().end(), "Start") == 1);

  auto odd_no_crossover = custom_options;
  odd_no_crossover.population_size = 5;
  odd_no_crossover.generations = 1;
  odd_no_crossover.crossover_rate = 0.0;
  const auto odd_result =
      minera::mine_genetic(custom_fields_log(), odd_no_crossover);
  assert(odd_result.diagnostics.computed);
  assert(odd_result.diagnostics.candidates_evaluated >= 5);

  auto invalid_timestamp = custom_options;
  invalid_timestamp.timestamp_key = "missing_time";
  threw = false;
  try { (void)minera::mine_genetic(custom_fields_log(), invalid_timestamp); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  auto invalid_case = custom_options;
  invalid_case.case_id_key = "missing_case";
  threw = false;
  try { (void)minera::mine_genetic(custom_fields_log(), invalid_case); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  auto malformed = first;
  malformed.diagnostics.best_fitness =
      std::numeric_limits<double>::quiet_NaN();
  threw = false;
  try { (void)minera::genetic_miner_json(malformed); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  return 0;
}
