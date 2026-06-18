// Keep assertions live even in NDEBUG (Release) builds: this file *is* the
// test, so its asserts must never compile away. Must precede <cassert>.
#undef NDEBUG

#include "minera/conformance.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/directly_follows_graph.hpp"
#include "minera/inductive_miner.hpp"
#include "minera/json_export.hpp"
#include "minera/log_filter.hpp"
#include "minera/petri_net.hpp"
#include "minera/process_tree.hpp"

#include <algorithm>
#include <cassert>
#include <exception>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

std::filesystem::path write_temp_log(const std::string& content) {
  auto path = std::filesystem::temp_directory_path() / "minera_core_test.csv";
  std::ofstream output(path);
  output << content;
  return path;
}

void imports_csv_and_discovers_directly_follows_graph() {
  const auto path = write_temp_log(
      "case_id,activity,timestamp\n"
      "1,A,2026-01-01T00:00:00\n"
      "1,B,2026-01-01T00:01:00\n"
      "2,A,2026-01-01T00:00:00\n"
      "2,C,2026-01-01T00:01:00\n"
      "2,B,2026-01-01T00:02:00\n");

  const auto log = minera::import_csv_event_log(path);
  assert(log.trace_count() == 2);
  assert(log.event_count() == 5);

  const auto graph = minera::discover_directly_follows_graph(log);
  assert(graph.activities().size() == 3);
  assert(graph.edges().size() == 3);
  assert(graph.activity_count("A") == 2);
  assert(graph.activity_count("B") == 2);
  assert(graph.activity_count("C") == 1);
  assert(graph.start_activity_count("A") == 2);
  assert(graph.end_activity_count("B") == 2);
  assert(graph.edge_count("A", "B") == 1);
  assert(graph.edge_count("A", "C") == 1);
  assert(graph.edge_count("C", "B") == 1);
  assert(graph.edge_count("B", "A") == 0);
  assert(graph.variants().size() == 2);
}

void handles_quoted_csv_fields_and_timestamp_sorting() {
  const auto path = write_temp_log(
      "case_id,activity,timestamp\n"
      "\"case,1\",Done,2026-01-01T00:02:00\n"
      "\"case,1\",\"Review \"\"fast\"\"\",2026-01-01T00:01:00\n"
      "\"case,1\",Start,2026-01-01T00:00:00\n");

  const auto log = minera::import_csv_event_log(path);
  assert(log.trace_count() == 1);
  assert(log.traces.front().case_id == "case,1");
  assert(log.traces.front().events[0].activity == "Start");
  assert(log.traces.front().events[1].activity == "Review \"fast\"");
  assert(log.traces.front().events[2].activity == "Done");

  const auto graph = minera::discover_directly_follows_graph(log);
  assert(graph.edge_count("Start", "Review \"fast\"") == 1);
  assert(graph.edge_count("Review \"fast\"", "Done") == 1);
}

void aggregates_repeated_trace_variants() {
  const auto path = write_temp_log(
      "case_id,activity\n"
      "1,A\n"
      "1,B\n"
      "2,A\n"
      "2,B\n"
      "3,A\n"
      "3,C\n");

  minera::CsvImportOptions options;
  options.timestamp_column = "missing_timestamp";

  const auto log = minera::import_csv_event_log(path, options);
  const auto graph = minera::discover_directly_follows_graph(log);
  assert(graph.edge_count("A", "B") == 2);
  assert(graph.edge_count("A", "C") == 1);
  assert(graph.variants().size() == 2);
  assert(graph.variants()[0].count == 2);
  assert(graph.variants()[1].count == 1);
}

minera::EventLog log_from_variants(
    const std::vector<std::vector<std::string>>& variants) {
  minera::EventLog log;
  std::size_t case_index = 0;
  for (const auto& variant : variants) {
    minera::Trace trace;
    trace.case_id = std::to_string(case_index++);
    for (const auto& activity : variant) {
      minera::Event event;
      event.case_id = trace.case_id;
      event.activity = activity;
      trace.events.push_back(event);
    }
    log.traces.push_back(std::move(trace));
  }
  return log;
}

// Renders a process tree to a canonical S-expression: ->(a, b), X(a, b),
// +(a, b), *(a, b), leaf activity names, and "tau" for silent steps.
std::string render(const minera::ProcessTree& tree,
                   const minera::ProcessTreeNode& node) {
  using Op = minera::ProcessTreeOperator;
  switch (node.op) {
    case Op::kActivity:
      return tree.activities()[node.activity];
    case Op::kTau:
      return "tau";
    default:
      break;
  }

  std::string symbol;
  switch (node.op) {
    case Op::kSequence: symbol = "->"; break;
    case Op::kExclusive: symbol = "X"; break;
    case Op::kParallel: symbol = "+"; break;
    case Op::kLoop: symbol = "*"; break;
    default: symbol = "?"; break;
  }

  std::string out = symbol + "(";
  for (std::size_t i = 0; i < node.children.size(); ++i) {
    out += render(tree, node.children[i]);
    if (i + 1 < node.children.size()) {
      out += ", ";
    }
  }
  out += ")";
  return out;
}

std::string mine_tree(const std::vector<std::vector<std::string>>& variants) {
  const auto log = log_from_variants(variants);
  const auto tree = minera::discover_process_tree(log);
  return render(tree, tree.root());
}

void inductive_miner_discovers_sequence() {
  assert(mine_tree({{"a", "b", "c"}}) == "->(a, b, c)");
}

void inductive_miner_discovers_exclusive_choice() {
  // a then either b or c then d.
  assert(mine_tree({{"a", "b", "d"}, {"a", "c", "d"}}) ==
         "->(a, X(b, c), d)");
}

void inductive_miner_discovers_parallel() {
  // b and c are concurrent between a and d.
  const auto tree = mine_tree({{"a", "b", "c", "d"}, {"a", "c", "b", "d"}});
  assert(tree == "->(a, +(b, c), d)");
}

void inductive_miner_discovers_loop() {
  // a, then optionally repeat (b then a).
  const auto tree =
      mine_tree({{"a"}, {"a", "b", "a"}, {"a", "b", "a", "b", "a"}});
  assert(tree == "*(a, b)");
}

void inductive_miner_models_optional_skip() {
  // No cut applies, so the empty trace falls through to an explicit skip around
  // the rest of the behaviour (here a self-loop on the single activity).
  assert(mine_tree({{}, {"a"}, {"a", "a"}}) == "X(tau, *(a, tau))");
}

void inductive_miner_pushes_empty_trace_through_cut() {
  // The canonical miner detects the a->b sequence first; the empty trace rides
  // through the split and becomes an optional skip on each branch.
  assert(mine_tree({{}, {"a", "b"}}) == "->(X(tau, a), X(tau, b))");
}

void inductive_miner_falls_back_to_flower() {
  // A directly-follows cycle (a->b->c->a) has no exclusive, sequence, parallel
  // or loop cut, so it falls through to the flower model.
  const auto tree = mine_tree({{"a", "b"}, {"b", "c"}, {"c", "a"}});
  assert(tree == "*(tau, a, b, c)");
}

minera::ProcessTree tree_from_variants(
    const std::vector<std::vector<std::string>>& variants) {
  return minera::discover_process_tree(log_from_variants(variants));
}

void petri_net_translates_sequence() {
  const auto tree = tree_from_variants({{"a", "b", "c"}});
  const auto net = minera::to_petri_net(tree);
  // source + sink + two places between the three activities.
  assert(net.place_count() == 4);
  assert(net.transitions().size() == 3);
  for (const auto& transition : net.transitions()) {
    assert(!transition.silent());
  }
}

void petri_net_adds_silent_split_and_join_for_parallel() {
  const auto tree = tree_from_variants({{"a", "b", "c", "d"}, {"a", "c", "b", "d"}});
  const auto net = minera::to_petri_net(tree);
  std::size_t silent = 0;
  for (const auto& transition : net.transitions()) {
    if (transition.silent()) ++silent;
  }
  // The parallel block contributes exactly one AND-split and one AND-join.
  assert(silent == 2);
}

void conformance_is_perfect_on_the_discovery_log() {
  const auto log = log_from_variants({{"a", "b", "d"}, {"a", "c", "d"}});
  const auto tree = minera::discover_process_tree(log);
  const auto conformance = minera::check_conformance(log, tree);

  assert(conformance.trace_count == 2);
  assert(conformance.fitting_traces == 2);
  assert(conformance.fitness > 0.999);
  assert(conformance.missing == 0);
  assert(conformance.remaining == 0);
}

void conformance_is_perfect_for_every_operator() {
  // The inductive miner guarantees a perfectly fitting model, so token replay
  // through the workflow net must report fitness 1.0 for every operator — this
  // exercises the parallel AND-join and loop redo paths end to end.
  const std::vector<std::vector<std::vector<std::string>>> logs = {
      {{"a", "b", "c"}},                              // sequence
      {{"a", "b", "d"}, {"a", "c", "d"}},             // exclusive choice
      {{"a", "b", "c", "d"}, {"a", "c", "b", "d"}},   // parallel
      {{"a"}, {"a", "b", "a"}, {"a", "b", "a", "b", "a"}},  // loop
      {{}, {"a"}, {"a", "a"}},                        // optional skip + self loop
      {{"a", "b"}, {"b", "c"}, {"c", "a"}},           // flower fallback
  };
  for (const auto& variants : logs) {
    const auto log = log_from_variants(variants);
    const auto tree = minera::discover_process_tree(log);
    const auto conformance = minera::check_conformance(log, tree);
    assert(conformance.fitness > 0.9999);
    assert(conformance.missing == 0);
    assert(conformance.remaining == 0);
    assert(conformance.fitting_traces == conformance.trace_count);
  }
}

void conformance_counts_unmodeled_activities() {
  // The model knows only a and b; a trace carrying an unseen activity z cannot
  // be replayed cleanly and must register a deviation.
  const auto model_log = log_from_variants({{"a", "b"}});
  const auto tree = minera::discover_process_tree(model_log);

  const auto foreign = log_from_variants({{"a", "z", "b"}});
  const auto conformance = minera::check_conformance(foreign, tree);
  assert(conformance.missing > 0);
  assert(conformance.fitness < 1.0);
  assert(conformance.fitting_traces == 0);
}

void conformance_penalizes_a_deviating_trace() {
  // Model is a strict a -> b -> c sequence; the log skips b and reorders.
  const auto model_log = log_from_variants({{"a", "b", "c"}});
  const auto tree = minera::discover_process_tree(model_log);

  const auto deviating = log_from_variants({{"a", "c"}});
  const auto conformance = minera::check_conformance(deviating, tree);

  assert(conformance.fitting_traces == 0);
  assert(conformance.fitness < 1.0);
  assert(conformance.missing > 0);
  // The worst-fitting variant is surfaced first.
  assert(!conformance.variants.empty());
  assert(conformance.variants.front().fitness < 1.0);
}

void precision_is_high_for_a_tight_model_and_low_for_a_flower() {
  const auto sequence_log = log_from_variants({{"a", "b", "c"}});
  const auto sequence_tree = minera::discover_process_tree(sequence_log);
  const auto tight = minera::check_conformance(sequence_log, sequence_tree);
  assert(tight.precision > 0.99);

  // A directly-follows cycle yields a flower model that permits any activity at
  // any point, so escaping-edges precision drops sharply.
  const auto flower_log = log_from_variants({{"a", "b"}, {"b", "c"}, {"c", "a"}});
  const auto flower_tree = minera::discover_process_tree(flower_log);
  const auto loose = minera::check_conformance(flower_log, flower_tree);
  assert(loose.fitness > 0.999);  // a flower still reproduces the log
  assert(loose.precision < 0.85);
  assert(loose.precision < tight.precision);
}

void json_export_carries_the_conformance_contract() {
  // The frontend reads these keys by name, so guard them against renames.
  const auto log = log_from_variants({{"a", "b"}, {"a", "c"}});
  const auto graph = minera::discover_directly_follows_graph(log);
  const auto tree = minera::discover_process_tree(log);
  const auto conformance = minera::check_conformance(log, tree);

  const auto json = minera::analysis_json("test", log, graph, &tree, &conformance);
  for (const char* key : {"\"conformance\"", "\"fitness\"", "\"precision\"",
                          "\"fitting_traces\"", "\"trace_count\"", "\"missing\"",
                          "\"remaining\"", "\"variants\""}) {
    assert(json.find(key) != std::string::npos);
  }

  // Conformance is omitted when not supplied (older-export compatibility).
  const auto without = minera::analysis_json("test", log, graph, &tree, nullptr);
  assert(without.find("\"conformance\"") == std::string::npos);
}

void conformance_handles_an_empty_log() {
  const minera::EventLog empty;
  const auto tree = minera::discover_process_tree(empty);
  const auto conformance = minera::check_conformance(empty, tree);
  assert(conformance.trace_count == 0);
  assert(conformance.fitting_traces == 0);
}

template <typename Fn>
bool throws(Fn&& fn) {
  try {
    fn();
  } catch (const std::exception&) {
    return true;
  }
  return false;
}

void importer_handles_a_header_only_log() {
  const auto path = write_temp_log("case_id,activity,timestamp\n");
  const auto log = minera::import_csv_event_log(path);
  assert(log.trace_count() == 0);
  assert(log.event_count() == 0);

  // The downstream passes must tolerate an empty log without crashing.
  const auto graph = minera::discover_directly_follows_graph(log);
  assert(graph.activities().empty());
  assert(graph.edges().empty());
}

void importer_strips_a_utf8_bom() {
  const auto path = write_temp_log(
      "\xEF\xBB\xBF"
      "case_id,activity\n"
      "1,A\n");
  const auto log = minera::import_csv_event_log(path);
  assert(log.trace_count() == 1);
  assert(log.traces.front().events.front().activity == "A");
}

void dfg_handles_single_event_traces() {
  const auto path = write_temp_log(
      "case_id,activity\n"
      "1,A\n"
      "2,A\n");
  const auto log = minera::import_csv_event_log(path);
  const auto graph = minera::discover_directly_follows_graph(log);
  assert(graph.activities().size() == 1);
  assert(graph.edges().empty());  // no directly-follows pair within length-1 traces
  assert(graph.start_activity_count("A") == 2);
  assert(graph.end_activity_count("A") == 2);
}

std::vector<std::string> activity_names(const minera::Trace& trace) {
  std::vector<std::string> names;
  for (const auto& event : trace.events) names.push_back(event.activity);
  return names;
}

void filter_keeps_frequent_variants() {
  const auto log = log_from_variants(
      {{"a", "b"}, {"a", "b"}, {"a", "b"}, {"a", "c"}, {"a", "d"}});

  minera::LogFilterOptions options;
  options.min_variant_frequency = 2;
  const auto filtered = minera::filter_event_log(log, options);

  assert(filtered.trace_count() == 3);
  for (const auto& trace : filtered.traces) {
    assert((activity_names(trace) == std::vector<std::string>{"a", "b"}));
  }
}

void filter_keeps_only_the_top_variant() {
  const auto log = log_from_variants(
      {{"a", "b"}, {"a", "b"}, {"a", "b"}, {"a", "c"}, {"a", "d"}});

  minera::LogFilterOptions options;
  options.top_variants = 1;
  const auto filtered = minera::filter_event_log(log, options);

  assert(filtered.trace_count() == 3);
  assert((activity_names(filtered.traces.front()) ==
          std::vector<std::string>{"a", "b"}));
}

void filter_projects_activities() {
  const auto log = log_from_variants({{"a", "b", "c"}});

  minera::LogFilterOptions remove;
  remove.remove_activities = {"b"};
  const auto without_b = minera::filter_event_log(log, remove);
  assert((activity_names(without_b.traces.front()) ==
          std::vector<std::string>{"a", "c"}));

  minera::LogFilterOptions keep;
  keep.keep_activities = {"a", "c"};
  const auto only_ac = minera::filter_event_log(log, keep);
  assert((activity_names(only_ac.traces.front()) ==
          std::vector<std::string>{"a", "c"}));
}

void filter_applies_length_and_endpoint_constraints() {
  const auto log = log_from_variants({{"a"}, {"a", "b", "c"}});
  minera::LogFilterOptions length;
  length.min_length = 2;
  const auto longer = minera::filter_event_log(log, length);
  assert(longer.trace_count() == 1);
  assert(longer.traces.front().events.size() == 3);

  const auto ends_log = log_from_variants({{"a", "b", "x"}, {"a", "b", "y"}});
  minera::LogFilterOptions ends;
  ends.end_activities = {"x"};
  const auto ending_x = minera::filter_event_log(ends_log, ends);
  assert(ending_x.trace_count() == 1);
  assert(ending_x.traces.front().events.back().activity == "x");
}

void importer_rejects_malformed_rows() {
  // Too few fields for the declared header.
  assert(throws([] {
    const auto path = write_temp_log("case_id,activity\n1\n");
    (void)minera::import_csv_event_log(path);
  }));
  // Empty activity value.
  assert(throws([] {
    const auto path = write_temp_log("case_id,activity\n1,\n");
    (void)minera::import_csv_event_log(path);
  }));
  // Unterminated quoted field.
  assert(throws([] {
    const auto path = write_temp_log("case_id,activity\n1,\"open\n");
    (void)minera::import_csv_event_log(path);
  }));
  // Missing required column.
  assert(throws([] {
    const auto path = write_temp_log("id,activity\n1,A\n");
    (void)minera::import_csv_event_log(path);
  }));
}

}  // namespace

int main() {
  imports_csv_and_discovers_directly_follows_graph();
  handles_quoted_csv_fields_and_timestamp_sorting();
  aggregates_repeated_trace_variants();
  inductive_miner_discovers_sequence();
  inductive_miner_discovers_exclusive_choice();
  inductive_miner_discovers_parallel();
  inductive_miner_discovers_loop();
  inductive_miner_models_optional_skip();
  inductive_miner_pushes_empty_trace_through_cut();
  inductive_miner_falls_back_to_flower();
  petri_net_translates_sequence();
  petri_net_adds_silent_split_and_join_for_parallel();
  conformance_is_perfect_on_the_discovery_log();
  conformance_is_perfect_for_every_operator();
  conformance_counts_unmodeled_activities();
  conformance_penalizes_a_deviating_trace();
  precision_is_high_for_a_tight_model_and_low_for_a_flower();
  json_export_carries_the_conformance_contract();
  conformance_handles_an_empty_log();
  importer_handles_a_header_only_log();
  importer_strips_a_utf8_bom();
  dfg_handles_single_event_traces();
  importer_rejects_malformed_rows();
  filter_keeps_frequent_variants();
  filter_keeps_only_the_top_variant();
  filter_projects_activities();
  filter_applies_length_and_endpoint_constraints();
  return 0;
}
