#include "minera/footprints.hpp"

#include <cassert>
#include <initializer_list>
#include <string>

using namespace minera;

EventLog make_log(std::initializer_list<std::initializer_list<const char*>> rows) {
  EventLog log;
  for (const auto row : rows) {
    Trace trace;
    for (const auto activity : row) trace.events.push_back(Event{"", activity});
    log.traces.push_back(std::move(trace));
  }
  return log;
}

void discovery_variants_are_deterministic() {
  const auto log = make_log({{"A", "B"}, {"B", "A"}, {"A", "B"}, {}});
  const auto all = discover_footprints(log);
  assert(all.dfg.at({"A", "B"}) == 2);
  assert(all.dfg.at({"B", "A"}) == 1);
  assert(all.parallel.count({"A", "B"}) == 1);
  assert(all.parallel.count({"B", "A"}) == 1);
  assert(all.sequence.empty());
  assert(all.min_trace_length && *all.min_trace_length == 0);

  const auto traces = discover_trace_footprints(log);
  assert(traces.size() == 4);
  assert(traces[0].sequence.count({"A", "B"}) == 1);
  assert(traces[1].sequence.count({"B", "A"}) == 1);
  assert(traces[3].activities.empty() && traces[3].trace && traces[3].trace->empty());
}

void dfg_boundaries_are_inferred() {
  const std::map<FootprintPair, std::uint64_t> dfg = {{{"A", "B"}, 2}, {{"B", "C"}, 0}};
  const auto fp = discover_dfg_footprints(dfg);
  assert(!fp.dfg_present);
  assert(fp.dfg.empty());
  assert(!fp.min_trace_length_present);
  assert(fp.start_activities == std::set<std::string>{"A"});
  assert(fp.end_activities == std::set<std::string>{"C"});
  assert(fp.sequence.count({"A", "B"}) == 1);

  const auto singleton_graph = discover_directly_follows_graph(make_log({{"A"}}));
  const auto singleton = discover_dfg_footprints(singleton_graph);
  assert(singleton.activities == std::set<std::string>{"A"});
  assert(singleton.start_activities == std::set<std::string>{"A"});
  assert(singleton.end_activities == std::set<std::string>{"A"});
}

void conformance_variants_and_evaluation() {
  const auto model = discover_trace_footprints(make_log({{"A", "B"}}))[0];
  const auto observed = discover_trace_footprints(make_log({{"B", "A"}}))[0];
  assert(check_footprints_log_model(observed, model).count({"B", "A"}) == 1);
  auto strict_model = model;
  strict_model.sequence.clear();
  strict_model.parallel.insert({"A", "B"});
  const auto ordered = discover_trace_footprints(make_log({{"A", "B"}}))[0];
  const auto non_strict = check_footprints_log_model(ordered, strict_model);
  const auto strict = check_footprints_log_model(ordered, strict_model, {true});
  assert(non_strict.empty() && strict.count({"A", "B"}) == 1);

  auto extensive_model = model;
  extensive_model.activities_always_happening = std::set<std::string>{"A", "B"};
  const auto trace_results = check_footprints_trace_extensive(
      discover_trace_footprints(make_log({{"A", "B"}, {"A", "B"}, {"B"}})),
      extensive_model);
  assert(trace_results.size() == 3 && trace_results[2].activities_always_happening.count("A") == 1);
  const auto disabled = check_footprints_trace_extensive(
      discover_trace_footprints(make_log({{"B"}})), extensive_model, {false});
  assert(disabled[0].activities_always_happening.empty());

  // Extensive diagnostics include boundary and minimum-length violations.
  auto length_model = model;
  length_model.min_trace_length = 2;
  const auto short_log = discover_footprints(make_log({{"C"}}));
  const auto extensive = check_footprints_log_extensive(short_log, length_model);
  assert(extensive.start_activities.count("C") == 1);
  assert(extensive.end_activities.count("C") == 1);
  assert(!extensive.min_length_fit && !extensive.is_fit);

  Footprint missing_model_keys = length_model;
  missing_model_keys.start_activities_present = false;
  missing_model_keys.end_activities_present = false;
  missing_model_keys.min_trace_length.reset();
  missing_model_keys.min_trace_length_present = false;
  const auto omitted = check_footprints_log_extensive(short_log, missing_model_keys);
  assert(omitted.start_activities.empty() && omitted.end_activities.empty());
  assert(omitted.min_length_fit && omitted.is_fit);

  // Precision counts model configurations (not log frequencies).
  auto precise_model = discover_trace_footprints(make_log({{"A", "B"}, {"B", "C"}}))[0];
  precise_model.sequence.insert({"B", "C"});
  const auto precise_log = discover_trace_footprints(make_log({{"A", "B"}}))[0];
  assert(fp_precision(precise_log, precise_model) == 0.5);

  // Fitness uses DFG frequencies: a single low-frequency deviation has a
  // smaller impact than a cardinality-based score would give it.
  Footprint weighted_log;
  weighted_log.dfg = {{{"A", "B"}, 9}, {{"B", "C"}, 1}};
  weighted_log.sequence = {{"A", "B"}, {"B", "C"}};
  weighted_log.start_activities = {"A"};
  weighted_log.end_activities = {"C"};
  FootprintsExtensiveResult weighted_conf;
  weighted_conf.footprints.insert({"B", "C"});
  assert(fp_fitness(weighted_log, model, weighted_conf) > 0.9);
  assert(fp_fitness(weighted_log, model, weighted_conf) < 1.0);

  // Trace evaluation returns percent-fit traces and computes log fitness over
  // flattened DFG/relations, preserving duplicate trace occurrences.
  const auto trace_log = discover_trace_footprints(make_log({{"A", "B"}, {"B", "A"}}));
  const auto trace_conf = check_footprints_trace_extensive(trace_log, model);
  const auto summary = fp_fitness(trace_log, model, trace_conf);
  assert(summary.perc_fit_traces == 50.0);
  assert(summary.log_fitness < 1.0);

  const auto empty = discover_footprints(EventLog{});
  assert(fp_precision(empty, empty) == 1.0);
  assert(fp_fitness(empty, empty, check_footprints_log_extensive(empty, empty)) == 1.0);
}

int main() {
  discovery_variants_are_deterministic();
  dfg_boundaries_are_inferred();
  conformance_variants_and_evaluation();
}
