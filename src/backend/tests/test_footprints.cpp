#include "minera/footprints.hpp"

#include "../footprints/conformance_log_model.hpp"
#include "../footprints/conformance_trace_extensive.hpp"

#include <cassert>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
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

void conformance_optional_fields_and_duplicate_variants() {
  Footprint log;
  log.activities = {"A"};
  log.start_activities = {"A"};
  log.end_activities = {"A"};
  log.min_trace_length = 1;

  Footprint model = log;
  model.min_trace_length = 99;
  model.min_trace_length_present = false;
  const auto omitted = check_footprints_log_extensive(log, model);
  assert(omitted.min_length_fit && omitted.is_fit);

  Footprint model_with_length = log;
  model_with_length.min_trace_length = 2;
  const auto rejected = check_footprints_log_extensive(log, model_with_length);
  assert(!rejected.min_length_fit && !rejected.is_fit);

  // Trace-extensive conformance reuses an exact sequence variant.  The
  // second object deliberately carries conflicting derived fields so a
  // variant cache is observable and deterministic.
  Footprint first = log;
  first.trace = std::vector<std::string>{"A"};
  Footprint second = first;
  second.start_activities = {"B"};
  second.end_activities = {"B"};
  const auto cached = check_footprints_trace_extensive({first, second}, model);
  assert(cached.size() == 2);
  assert(cached[0].is_fit == cached[1].is_fit);
  assert(cached[1].start_activities.empty());

  EventLog source;
  source.traces.push_back(Trace{"case-a", {Event{"case-a", "A"}}});
  source.traces.push_back(Trace{"case-b", {Event{"case-b", "B"}}});
  const auto diagnostics =
      minera::footprints_conformance::TraceExtensiveVariant::diagnostics(
          source, cached, "case_id");
  assert(diagnostics.size() == 2);
  assert(diagnostics[0].case_id == "case-a");
  assert(diagnostics[1].case_id == "case-b");
}

void conformance_atomic_relation_extension_is_strict() {
  Footprint log;
  log.atomic_interleaving.insert({"A", "B"});
  Footprint model;
  model.sequence.insert({"A", "B"});
  assert(check_footprints_log_model(log, model).empty());
  assert(check_footprints_log_model(log, model, {true}).count({"A", "B"}) ==
         1);
}

void conformance_summary_validates_cardinality() {
  const auto log = discover_trace_footprints(make_log({{"A"}}));
  const auto model = log.front();
  const auto result = check_footprints_trace_extensive(log, model);
  bool mismatched = false;
  try {
    (void)fp_fitness(log, model, {});
  } catch (const std::invalid_argument&) {
    mismatched = true;
  }
  assert(mismatched);

  const auto empty = fp_fitness(
      std::vector<Footprint>{}, model,
      std::vector<FootprintsExtensiveResult>{});
  assert(empty.perc_fit_traces == 0.0 && empty.log_fitness == 1.0);

  bool wrong_size = false;
  try {
    (void)fp_fitness(log, model,
                     std::vector<FootprintsExtensiveResult>{result.front(),
                                                            result.front()});
  } catch (const std::invalid_argument&) {
    wrong_size = true;
  }
  assert(wrong_size);
}

void conformance_diagnostics_validate_case_id_selectors() {
  EventLog source;
  source.traces.push_back(Trace{"anonymous", {}});

  // The default selector can still identify an anonymous native trace by
  // position, which keeps diagnostics total for hand-built empty logs.
  const auto default_log_model =
      minera::footprints_conformance::LogModelVariant::diagnostics(
          source, std::vector<FootprintViolations>{{}});
  assert(default_log_model.size() == 1 &&
         default_log_model.front().case_id == "anonymous");
  const auto default_trace_extensive =
      minera::footprints_conformance::TraceExtensiveVariant::diagnostics(
          source, std::vector<FootprintsExtensiveResult>{{}});
  assert(default_trace_extensive.size() == 1 &&
         default_trace_extensive.front().case_id == "anonymous");

  source.traces.front().case_id.clear();
  const auto fallback_log_model =
      minera::footprints_conformance::LogModelVariant::diagnostics(
          source, std::vector<FootprintViolations>{{}});
  const auto fallback_trace_extensive =
      minera::footprints_conformance::TraceExtensiveVariant::diagnostics(
          source, std::vector<FootprintsExtensiveResult>{{}});
  assert(fallback_log_model.front().case_id == "trace-0");
  assert(fallback_trace_extensive.front().case_id == "trace-0");

  bool log_model_missing = false;
  try {
    (void)minera::footprints_conformance::LogModelVariant::diagnostics(
        source, std::vector<FootprintViolations>{{}}, "external_case");
  } catch (const std::invalid_argument&) {
    log_model_missing = true;
  }
  bool trace_extensive_missing = false;
  try {
    (void)minera::footprints_conformance::TraceExtensiveVariant::diagnostics(
        source, std::vector<FootprintsExtensiveResult>{{}}, "external_case");
  } catch (const std::invalid_argument&) {
    trace_extensive_missing = true;
  }
  assert(log_model_missing && trace_extensive_missing);

  source.traces.front().case_id = "selected-case";
  source.traces.front().case_id_state = FirstClassAttributeState::kPresent;
  source.traces.front().case_id_source_key = "external_case";
  source.traces.front().case_id_attribute =
      EventAttribute{"external_case", "selected-case"};
  const auto selected_log_model =
      minera::footprints_conformance::LogModelVariant::diagnostics(
          source, std::vector<FootprintViolations>{{}}, "external_case");
  const auto selected_trace_extensive =
      minera::footprints_conformance::TraceExtensiveVariant::diagnostics(
          source, std::vector<FootprintsExtensiveResult>{{}},
          "external_case");
  assert(selected_log_model.front().case_id == "selected-case");
  assert(selected_trace_extensive.front().case_id == "selected-case");
  source.traces.front().case_id.clear();
  source.traces.front().case_id_state = FirstClassAttributeState::kMissing;
  source.traces.front().case_id_source_key.clear();
  source.traces.front().case_id_attribute.reset();

  source.traces.front().attributes.push_back(
      EventAttribute{"external_case", "", AttributeKind::kString, true});
  bool log_model_null = false;
  try {
    (void)minera::footprints_conformance::LogModelVariant::diagnostics(
        source, std::vector<FootprintViolations>{{}}, "external_case");
  } catch (const std::invalid_argument&) {
    log_model_null = true;
  }
  bool trace_extensive_null = false;
  try {
    (void)minera::footprints_conformance::TraceExtensiveVariant::diagnostics(
        source, std::vector<FootprintsExtensiveResult>{{}}, "external_case");
  } catch (const std::invalid_argument&) {
    trace_extensive_null = true;
  }
  assert(log_model_null && trace_extensive_null);
}

void table_projection_keeps_aliases_aligned_after_reordering() {
  EventLog log;
  Trace second{"c2", {Event{"c2", "B", "2024-01-02T00:00:00Z"}}};
  second.events.front().input_order = 1;
  Trace first{"c1", {Event{"c1", "A", "2024-01-01T00:00:00Z"}}};
  first.events.front().input_order = 0;
  log.traces = {std::move(second), std::move(first)};

  FootprintsDiscoveryOptions options;
  options.activity_attribute = "activity";
  const auto table = prepare_footprints_event_table(log, options);
  const auto activity = table.column_index("activity");
  const auto case_id = table.column_index("case:concept:name");
  assert(table.rows.size() == 2);
  assert(table.rows[0][activity] && table.rows[0][activity]->value == "A");
  assert(table.rows[0][case_id] && table.rows[0][case_id]->value == "c1");
  assert(table.rows[1][activity] && table.rows[1][activity]->value == "B");
  assert(table.rows[1][case_id] && table.rows[1][case_id]->value == "c2");
  assert(table.dtypes.empty());
  assert(!table.index);

  const auto lazy = prepare_footprints_lazy_table(log, options);
  assert(lazy.row_count() == 2);
  assert(lazy.dtypes().empty());
  assert(!lazy.index());
  const auto materialized = lazy.materialize();
  assert(materialized.rows == table.rows);
}

// DFG totals past 2^64 (counts as JSON footprints may carry) neither wrap
// nor lose the reference's rounding: exact sums, each rounded to a double
// once (expected values from pm4py fp_fitness on the same dicts).
void fitness_totals_past_uint64() {
  const std::uint64_t max = std::numeric_limits<std::uint64_t>::max();
  const auto fitness = [](std::map<FootprintPair, std::uint64_t> dfg,
                          std::set<FootprintPair> deviations) {
    Footprint log;
    for (const auto& [pair, count] : dfg) log.sequence.insert(pair);
    log.dfg = std::move(dfg);
    log.start_activities = {"A"};
    log.end_activities = {"D"};
    FootprintsExtensiveResult conformance;
    conformance.footprints = std::move(deviations);
    conformance.end_activities = {"D"};
    return fp_fitness(log, Footprint{}, conformance);
  };
  const std::uint64_t half = std::uint64_t{1} << 63;
  assert(fitness({{{"A", "B"}, half}, {{"B", "C"}, half}}, {{"A", "B"}}) == 0x1p-1);
  assert(fitness({{{"A", "B"}, max}, {{"B", "C"}, max}, {{"C", "D"}, max}},
                 {{"A", "B"}, {"C", "D"}}) == 0x1.999999999999ap-2);
  assert(fitness({{{"A", "B"}, max}, {{"B", "C"}, 3}, {{"C", "D"}, (std::uint64_t{1} << 53) + 1}},
                 {{"B", "C"}}) == 0x1.999999999999ap-1);
  assert(fitness({{{"A", "B"}, max}, {{"B", "C"}, max}, {{"C", "D"}, 12345}},
                 {{"C", "D"}}) == 0x1.9999999999998p-1);
}

int main() {
  fitness_totals_past_uint64();
  discovery_variants_are_deterministic();
  dfg_boundaries_are_inferred();
  conformance_variants_and_evaluation();
  conformance_optional_fields_and_duplicate_variants();
  conformance_atomic_relation_extension_is_strict();
  conformance_summary_validates_cardinality();
  conformance_diagnostics_validate_case_id_selectors();
  table_projection_keeps_aliases_aligned_after_reordering();
}
