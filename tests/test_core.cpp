// Keep assertions live even in NDEBUG (Release) builds: this file *is* the
// test, so its asserts must never compile away. Must precede <cassert>.
#undef NDEBUG

#include "minera/alpha_miner.hpp"
#include "minera/conformance.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/directly_follows_graph.hpp"
#include "minera/heuristic_miner.hpp"
#include "minera/inductive_miner.hpp"
#include "minera/json_export.hpp"
#include "minera/log_filter.hpp"
#include "minera/petri_net.hpp"
#include "minera/process_tree.hpp"
#include "minera/xes_event_log_importer.hpp"

#include <algorithm>
#include <cassert>
#include <exception>
#include <filesystem>
#include <fstream>
#include <sstream>
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

std::string mine_tree_imf(const std::vector<std::vector<std::string>>& variants,
                          double noise_threshold) {
  const auto log = log_from_variants(variants);
  minera::InductiveMinerOptions options;
  options.noise_threshold = noise_threshold;
  const auto tree = minera::discover_process_tree(log, options);
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

void inductive_miner_infrequent_recovers_a_noisy_sequence() {
  // A dominant a->b->c->d sequence with one rare trace that adds infrequent
  // a->c and c->a hops (and an extra a). The infrequent edges form a cycle that
  // derails the classic miner, but the IMf threshold filters them out.
  std::vector<std::vector<std::string>> variants(20, {"a", "b", "c", "d"});
  variants.push_back({"a", "c", "a", "b", "c", "d"});

  // Classic IM (threshold 0) is misled by the rare edges.
  assert(mine_tree_imf(variants, 0.0) != "->(a, b, c, d)");
  // IMf drops the noise and recovers the clean chain.
  assert(mine_tree_imf(variants, 0.2) == "->(a, b, c, d)");
}

void inductive_miner_infrequent_leaves_a_clean_log_untouched() {
  // With no infrequent behaviour there is nothing to filter, so a non-zero
  // threshold yields exactly the same model as the classic miner.
  assert(mine_tree_imf({{"a", "b", "c"}}, 0.3) == "->(a, b, c)");
  assert(mine_tree_imf({{"a", "b", "d"}, {"a", "c", "d"}}, 0.3) ==
         "->(a, X(b, c), d)");
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

bool contains(const std::vector<std::size_t>& places, std::size_t place) {
  return std::find(places.begin(), places.end(), place) != places.end();
}

const minera::PetriTransition* transition_for(const minera::PetriNet& net,
                                              const std::string& activity) {
  for (std::size_t id = 0; id < net.activities().size(); ++id) {
    if (net.activities()[id] != activity) continue;
    for (const auto& transition : net.transitions()) {
      if (transition.activity && *transition.activity == id) return &transition;
    }
  }
  return nullptr;
}

void alpha_miner_discovers_a_sequence() {
  const auto net = minera::discover_petri_net(log_from_variants({{"a", "b", "c"}}));
  // source + sink + one place between each consecutive activity pair.
  assert(net.place_count() == 4);
  assert(net.transitions().size() == 3);
  const auto* a = transition_for(net, "a");
  const auto* c = transition_for(net, "c");
  assert(a != nullptr && c != nullptr);
  assert(contains(a->inputs, net.source()));   // start activity fed by source
  assert(contains(c->outputs, net.sink()));     // end activity drains to sink
  // The Alpha Miner emits only labeled transitions, never silent ones.
  for (const auto& transition : net.transitions()) assert(!transition.silent());
}

void alpha_miner_shares_a_place_for_exclusive_choice() {
  // a, then either b or c, then d. b # c, so one place routes a's token to both
  // branches and one place collects them before d.
  const auto net = minera::discover_petri_net(
      log_from_variants({{"a", "b", "d"}, {"a", "c", "d"}}));
  // source, sink, ({a},{b,c}) and ({b,c},{d}).
  assert(net.place_count() == 4);
  const auto* a = transition_for(net, "a");
  const auto* b = transition_for(net, "b");
  const auto* c = transition_for(net, "c");
  const auto* d = transition_for(net, "d");
  assert(a && b && c && d);
  // a's lone output place is exactly the shared entry of branches b and c.
  assert(a->outputs.size() == 1);
  const std::size_t branch_in = a->outputs.front();
  assert(contains(b->inputs, branch_in) && contains(c->inputs, branch_in));
  // b and c merge into one place that feeds d.
  assert(b->outputs.size() == 1 && b->outputs == c->outputs);
  assert(contains(d->inputs, b->outputs.front()));
}

void alpha_miner_keeps_parallel_branches_on_separate_places() {
  // b and c are concurrent between a and d (a||... no: b||c). Classic Alpha
  // represents concurrency as two independent places, never a shared one, so a
  // forks into two places and d joins from two.
  const auto net = minera::discover_petri_net(
      log_from_variants({{"a", "b", "c", "d"}, {"a", "c", "b", "d"}}));
  // source, sink, ({a},{b}), ({a},{c}), ({b},{d}), ({c},{d}).
  assert(net.place_count() == 6);
  const auto* a = transition_for(net, "a");
  const auto* d = transition_for(net, "d");
  assert(a && d);
  assert(a->outputs.size() == 2);  // one place per concurrent branch
  assert(d->inputs.size() == 2);
  // No place is shared between b and c (that would force an order between them).
  const auto* b = transition_for(net, "b");
  const auto* c = transition_for(net, "c");
  assert(b && c);
  for (const auto place : b->inputs) assert(!contains(c->inputs, place));
}

void alpha_miner_drops_short_loop_activities() {
  // A length-two loop (b after a, a after b) makes a and b parallel under the
  // footprint, so the classic algorithm cannot place either — the textbook
  // short-loop limitation. The transitions still exist but carry no arcs.
  const auto net = minera::discover_petri_net(
      log_from_variants({{"x", "a", "b", "a", "y"}, {"x", "a", "y"}}));
  const auto* a = transition_for(net, "a");
  const auto* b = transition_for(net, "b");
  assert(a && b);
  assert(b->inputs.empty() && b->outputs.empty());
}

void alpha_miner_handles_an_empty_log() {
  const minera::EventLog empty;
  const auto net = minera::discover_petri_net(empty);
  assert(net.place_count() == 2);  // just source and sink
  assert(net.transitions().empty());
}

void json_export_carries_the_petri_net_contract() {
  // The frontend reads these keys by name, so guard them against renames.
  const auto log = log_from_variants({{"a", "b"}, {"a", "c"}});
  const auto graph = minera::discover_directly_follows_graph(log);
  const auto net = minera::discover_petri_net(log);

  const auto json = minera::analysis_json("test", log, graph, nullptr, &net);
  for (const char* key : {"\"petri_net\"", "\"place_count\"", "\"source\"",
                          "\"sink\"", "\"transitions\"", "\"inputs\"",
                          "\"outputs\""}) {
    assert(json.find(key) != std::string::npos);
  }

  // The Petri net is omitted when not supplied (older-export compatibility).
  const auto without = minera::analysis_json("test", log, graph);
  assert(without.find("\"petri_net\"") == std::string::npos);
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
  // The frontend reads these keys by name, so guard them against renames. The
  // "conformance" object is keyed by miner id, each carrying a token-replay and
  // an alignment result.
  const auto log = log_from_variants({{"a", "b"}, {"a", "c"}});
  const auto graph = minera::discover_directly_follows_graph(log);
  const auto tree = minera::discover_process_tree(log);
  const auto net = minera::to_petri_net(tree);
  const std::vector<minera::ModelConformance> conformance = {
      {"inductive", minera::final_marking_reachable(net),
       minera::check_conformance(log, net), minera::check_alignment(log, net)},
  };

  const auto json =
      minera::analysis_json("test", log, graph, &tree, nullptr, nullptr,
                            &conformance);
  for (const char* key : {"\"conformance\"", "\"inductive\"", "\"sound\"",
                          "\"token_replay\"", "\"alignments\"", "\"fitness\"",
                          "\"precision\"", "\"fitting_traces\"",
                          "\"trace_count\"", "\"missing\"", "\"remaining\"",
                          "\"computed\"", "\"exhausted\"", "\"variants\""}) {
    assert(json.find(key) != std::string::npos);
  }

  // Conformance is omitted when not supplied (older-export compatibility).
  const auto without = minera::analysis_json("test", log, graph, &tree);
  assert(without.find("\"conformance\"") == std::string::npos);
}

void conformance_handles_an_empty_log() {
  const minera::EventLog empty;
  const auto tree = minera::discover_process_tree(empty);
  const auto conformance = minera::check_conformance(empty, tree);
  assert(conformance.trace_count == 0);
  assert(conformance.fitting_traces == 0);
}

// Counts alignment moves of a given type in a variant.
std::size_t count_moves(const minera::VariantAlignment& variant,
                        minera::AlignmentMoveType type) {
  return static_cast<std::size_t>(std::count_if(
      variant.moves.begin(), variant.moves.end(),
      [&](const minera::AlignmentMove& move) { return move.type == type; }));
}

void alignment_is_perfect_for_every_operator() {
  // The inductive miner yields a perfectly fitting model, so every trace must
  // align at cost 0 — only synchronous (and dropped silent) moves. This walks
  // the AND-join and loop-redo routing the same way the token-replay test does.
  const std::vector<std::vector<std::vector<std::string>>> logs = {
      {{"a", "b", "c"}},                                    // sequence
      {{"a", "b", "d"}, {"a", "c", "d"}},                   // exclusive choice
      {{"a", "b", "c", "d"}, {"a", "c", "b", "d"}},         // parallel
      {{"a"}, {"a", "b", "a"}, {"a", "b", "a", "b", "a"}},  // loop
      {{}, {"a"}, {"a", "a"}},                              // optional + self loop
      {{"a", "b"}, {"b", "c"}, {"c", "a"}},                 // flower fallback
  };
  for (const auto& variants : logs) {
    const auto log = log_from_variants(variants);
    const auto tree = minera::discover_process_tree(log);
    const auto alignment = minera::check_alignment(log, tree);
    assert(alignment.fitness > 0.9999);
    assert(alignment.total_cost == 0);
    assert(alignment.fitting_traces == alignment.trace_count);
    for (const auto& variant : alignment.variants) {
      assert(variant.cost == 0);
      assert(count_moves(variant, minera::AlignmentMoveType::kLog) == 0);
      assert(count_moves(variant, minera::AlignmentMoveType::kModel) == 0);
      // Every observed event is explained by a synchronous move.
      assert(count_moves(variant, minera::AlignmentMoveType::kSynchronous) ==
             variant.activities.size());
    }
  }
}

void alignment_diagnoses_a_skipped_activity() {
  // Model is a strict a -> b -> c sequence; the trace skips b. The optimal
  // alignment keeps a and c synchronous and inserts a model move for b.
  const auto model_log = log_from_variants({{"a", "b", "c"}});
  const auto tree = minera::discover_process_tree(model_log);

  const auto deviating = log_from_variants({{"a", "c"}});
  const auto alignment = minera::check_alignment(deviating, tree);

  assert(alignment.fitting_traces == 0);
  assert(alignment.fitness < 1.0);
  assert(!alignment.variants.empty());
  const auto& variant = alignment.variants.front();
  assert(variant.cost == 1);  // one visible model move for the skipped b
  assert(count_moves(variant, minera::AlignmentMoveType::kModel) == 1);
  assert(count_moves(variant, minera::AlignmentMoveType::kSynchronous) == 2);
  // The model move names the activity the trace skipped.
  const auto model_move = std::find_if(
      variant.moves.begin(), variant.moves.end(),
      [](const minera::AlignmentMove& m) {
        return m.type == minera::AlignmentMoveType::kModel;
      });
  assert(model_move != variant.moves.end() && model_move->activity == "b");
}

void alignment_diagnoses_an_inserted_activity() {
  // The model knows only a -> b; the trace inserts an unmodeled z, which can
  // only be explained by a log move.
  const auto model_log = log_from_variants({{"a", "b"}});
  const auto tree = minera::discover_process_tree(model_log);

  const auto foreign = log_from_variants({{"a", "z", "b"}});
  const auto alignment = minera::check_alignment(foreign, tree);

  assert(alignment.fitting_traces == 0);
  assert(!alignment.variants.empty());
  const auto& variant = alignment.variants.front();
  assert(variant.cost == 1);  // one log move for the inserted z
  assert(count_moves(variant, minera::AlignmentMoveType::kLog) == 1);
  assert(count_moves(variant, minera::AlignmentMoveType::kSynchronous) == 2);
  const auto log_move = std::find_if(
      variant.moves.begin(), variant.moves.end(),
      [](const minera::AlignmentMove& m) {
        return m.type == minera::AlignmentMoveType::kLog;
      });
  assert(log_move != variant.moves.end() && log_move->activity == "z");
}

void json_export_carries_the_alignment_contract() {
  // A deviating trace makes the per-move alignment keys appear in the JSON, so
  // guard those against renames too.
  const auto model_log = log_from_variants({{"a", "b", "c"}});
  const auto tree = minera::discover_process_tree(model_log);
  const auto net = minera::to_petri_net(tree);
  const auto deviating = log_from_variants({{"a", "c"}});
  const auto graph = minera::discover_directly_follows_graph(deviating);
  const std::vector<minera::ModelConformance> conformance = {
      {"inductive", minera::final_marking_reachable(net),
       minera::check_conformance(deviating, net),
       minera::check_alignment(deviating, net)},
  };

  const auto json = minera::analysis_json("test", deviating, graph, &tree,
                                          nullptr, nullptr, &conformance);
  for (const char* key : {"\"alignments\"", "\"total_cost\"", "\"moves\"",
                          "\"cost\"", "\"type\"", "\"model\"", "\"sync\""}) {
    assert(json.find(key) != std::string::npos);
  }
}

void final_marking_reachable_distinguishes_sound_nets() {
  // A sound model's final marking is reachable; an Alpha net with concurrent end
  // activities overflows its sink and is not reachable.
  const auto sound = log_from_variants({{"a", "b", "c"}});
  assert(minera::final_marking_reachable(
      minera::to_petri_net(minera::discover_process_tree(sound))));

  const auto concurrent = log_from_variants({{"a", "b", "c"}, {"a", "c", "b"}});
  assert(!minera::final_marking_reachable(
      minera::discover_petri_net(concurrent)));
}

void alignment_scores_a_sound_alpha_net() {
  // The Alpha Miner yields a sound, perfectly-fitting net for an exclusive
  // choice (every run keeps a single token), so the log aligns at zero cost.
  // (A parallel log, by contrast, makes Alpha's shared sink unsound — both
  // branches deposit a token — which the aligner correctly refuses to score.)
  const auto log = log_from_variants({{"a", "b", "d"}, {"a", "c", "d"}});
  const auto net = minera::discover_petri_net(log);
  const auto alignment = minera::check_alignment(log, net);

  assert(alignment.computed);
  assert(alignment.fitness > 0.9999);
  assert(alignment.fitting_traces == alignment.trace_count);
}

void alignment_flags_an_unsound_net() {
  // A net whose only transition drains the source but produces nothing leaves
  // the sink unreachable — not a sound workflow net. The aligner must refuse to
  // score it rather than report a misleading number.
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({"a"});
  net.add_transition(0, {source}, {});  // consumes the source, produces nothing

  const auto log = log_from_variants({{"a"}});
  const auto alignment = minera::check_alignment(log, net);
  assert(!alignment.computed);
  assert(!alignment.exhausted);  // unreachable sink, not a search-bound timeout
  assert(alignment.variants.empty());
}

void alignment_flags_an_exhausted_search_on_an_unbounded_net() {
  // A reachable but unbounded net: a silent transition consumes the source and
  // produces the source back plus a token on an auxiliary place, so it can fire
  // forever, growing the marking without bound. The sink is still reachable (a
  // second silent transition drains the source to it), so the net clears the
  // soundness gate — but aligning a trace floods the search with infinitely many
  // zero-cost markings before the goal's positive cost is ever reached. The
  // aligner must hit its bound and report `exhausted` (distinct from an unsound
  // net) rather than spin forever or emit a fabricated score.
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto sink = net.add_place();
  const auto aux = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({});
  net.add_transition(std::nullopt, {source}, {source, aux});  // unbounded growth
  net.add_transition(std::nullopt, {source}, {sink});          // reaches the sink

  const auto log = log_from_variants({{"x"}});
  const auto alignment = minera::check_alignment(log, net);
  assert(!alignment.computed);
  assert(alignment.exhausted);  // search bound hit, distinct from an unsound net
  assert(alignment.variants.empty());
  assert(alignment.fitting_traces == 0);
  assert(alignment.total_cost == 0);
}

void alignment_handles_an_empty_log() {
  const minera::EventLog empty;
  const auto tree = minera::discover_process_tree(empty);
  const auto alignment = minera::check_alignment(empty, tree);
  assert(alignment.trace_count == 0);
  assert(alignment.fitting_traces == 0);
  assert(alignment.total_cost == 0);
  assert(alignment.fitness > 0.9999);
}

void alignment_treats_an_empty_log_as_perfect_for_every_model() {
  // With no traces there is nothing to align, so the result must be trivially
  // perfect for every miner — never reported as unsound just because an empty
  // net's sink is unreachable.
  const minera::EventLog empty;
  const auto alpha = minera::discover_petri_net(empty);
  const auto heuristic = minera::to_petri_net(minera::discover_heuristics_net(empty));
  for (const auto* net : {&alpha, &heuristic}) {
    const auto alignment = minera::check_alignment(empty, *net);
    assert(alignment.computed);
    assert(alignment.trace_count == 0);
    assert(alignment.total_cost == 0);
    assert(alignment.fitness > 0.9999);
  }
}

void heuristics_net_becomes_a_sound_workflow_net() {
  // The Heuristic Miner emits a dependency graph; converting it to a workflow
  // net must always yield a sound, replayable model so both conformance methods
  // work. A strongly-sequential log (every dependency clears the 0.9 threshold)
  // converts to a clean chain that the log fits perfectly.
  const std::vector<std::vector<std::string>> sequence(12,
                                                       {"a", "b", "c", "d"});
  const auto log = log_from_variants(sequence);
  const auto heuristics = minera::discover_heuristics_net(log);
  const auto net = minera::to_petri_net(heuristics);

  // Source and sink plus a ready/done pair per activity.
  assert(net.place_count() == 2 + 2 * heuristics.activities.size());

  const auto replay = minera::check_conformance(log, net);
  assert(replay.fitness > 0.999);

  const auto alignment = minera::check_alignment(log, net);
  assert(alignment.computed);  // the conversion is always a sound state machine
  assert(alignment.fitness > 0.999);
}

void heuristics_net_is_always_sound_even_when_it_drops_edges() {
  // With a high threshold the Heuristic Miner drops weak dependencies, so the
  // log no longer fits perfectly — but the converted net must still be sound
  // (alignment computes a finite cost rather than refusing).
  const auto log = log_from_variants(
      {{"a", "b", "d"}, {"a", "c", "d"}, {"a", "b", "d"}});
  const auto net = minera::to_petri_net(minera::discover_heuristics_net(log));

  const auto alignment = minera::check_alignment(log, net);
  assert(alignment.computed);
  assert(alignment.fitness >= 0.0 && alignment.fitness <= 1.0);
}

void heuristics_net_handles_a_self_loop() {
  // A self-loop dependency (a after a) must route done(a) back to ready(a) so
  // the converted net can replay the repetition rather than deadlock.
  const std::vector<std::vector<std::string>> looping(
      12, {"a", "a", "a", "b"});
  const auto log = log_from_variants(looping);
  const auto net = minera::to_petri_net(minera::discover_heuristics_net(log));

  const auto alignment = minera::check_alignment(log, net);
  assert(alignment.computed);
  assert(alignment.fitness > 0.999);
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

minera::EventLog import_xes(const std::string& xml) {
  std::istringstream input(xml);
  return minera::import_xes_event_log(input);
}

std::filesystem::path write_temp_xes(const std::string& content) {
  auto path = std::filesystem::temp_directory_path() / "minera_core_test.xes";
  std::ofstream output(path, std::ios::binary);
  output << content;
  return path;
}

void xes_importer_reads_a_file_from_disk() {
  // A UTF-8 BOM is stripped, and the file-path overload opens it in binary.
  const auto path = write_temp_xes(
      "\xEF\xBB\xBF"
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<log>\n"
      "  <trace>\n"
      "    <string key=\"concept:name\" value=\"case-1\"/>\n"
      "    <event>\n"
      "      <string key=\"concept:name\" value=\"A\"/>\n"
      "      <date key=\"time:timestamp\" value=\"2026-01-01T00:00:00\"/>\n"
      "    </event>\n"
      "    <event>\n"
      "      <string key=\"concept:name\" value=\"B\"/>\n"
      "      <date key=\"time:timestamp\" value=\"2026-01-01T00:01:00\"/>\n"
      "    </event>\n"
      "  </trace>\n"
      "</log>\n");

  const auto log = minera::import_xes_event_log(path);
  assert(log.trace_count() == 1);
  assert(log.event_count() == 2);
  assert(log.traces[0].case_id == "case-1");
  assert(log.traces[0].events[0].activity == "A");
  assert(log.traces[0].events[1].activity == "B");

  const auto graph = minera::discover_directly_follows_graph(log);
  assert(graph.edge_count("A", "B") == 1);

  // Opening a path that does not exist is an error, not an empty log.
  assert(throws([] {
    (void)minera::import_xes_event_log(
        std::filesystem::temp_directory_path() / "minera_no_such_file.xes");
  }));
}

void xes_importer_reads_traces_and_events() {
  const auto log = import_xes(
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!-- a minimal XES log -->\n"
      "<log xes.version=\"1.0\">\n"
      "  <trace>\n"
      "    <string key=\"concept:name\" value=\"case-1\"/>\n"
      "    <event>\n"
      "      <string key=\"concept:name\" value=\"A\"/>\n"
      "      <date key=\"time:timestamp\" value=\"2026-01-01T00:00:00\"/>\n"
      "    </event>\n"
      "    <event>\n"
      "      <string key=\"concept:name\" value=\"B\"/>\n"
      "      <date key=\"time:timestamp\" value=\"2026-01-01T00:01:00\"/>\n"
      "    </event>\n"
      "  </trace>\n"
      "  <trace>\n"
      "    <string key=\"concept:name\" value=\"case-2\"/>\n"
      "    <event>\n"
      "      <string key=\"concept:name\" value=\"A\"/>\n"
      "    </event>\n"
      "  </trace>\n"
      "</log>\n");

  assert(log.trace_count() == 2);
  assert(log.event_count() == 3);
  assert(log.traces[0].case_id == "case-1");
  assert(log.traces[0].events.size() == 2);
  assert(log.traces[0].events[0].activity == "A");
  assert(log.traces[0].events[0].case_id == "case-1");
  assert(log.traces[0].events[1].activity == "B");
  assert(log.traces[1].case_id == "case-2");
  assert(log.traces[1].events[0].activity == "A");

  // The XES log mines just like a CSV log would.
  const auto graph = minera::discover_directly_follows_graph(log);
  assert(graph.edge_count("A", "B") == 1);
}

void xes_importer_sorts_events_by_timestamp() {
  // Events out of order in the document are reordered by their timestamp.
  const auto log = import_xes(
      "<log>\n"
      "  <trace>\n"
      "    <string key=\"concept:name\" value=\"c\"/>\n"
      "    <event>\n"
      "      <string key=\"concept:name\" value=\"B\"/>\n"
      "      <date key=\"time:timestamp\" value=\"2026-01-01T00:02:00\"/>\n"
      "    </event>\n"
      "    <event>\n"
      "      <string key=\"concept:name\" value=\"A\"/>\n"
      "      <date key=\"time:timestamp\" value=\"2026-01-01T00:01:00\"/>\n"
      "    </event>\n"
      "  </trace>\n"
      "</log>\n");

  assert(log.traces.size() == 1);
  assert(log.traces[0].events.size() == 2);
  assert(log.traces[0].events[0].activity == "A");
  assert(log.traces[0].events[1].activity == "B");
}

void xes_importer_decodes_entities_and_ignores_nesting() {
  // Nested sub-attributes must not be mistaken for case/activity attributes,
  // and XML entities in values are decoded.
  const auto log = import_xes(
      "<log>\n"
      "  <trace>\n"
      "    <string key=\"concept:name\" value=\"Tom &amp; Jerry\"/>\n"
      "    <event>\n"
      "      <string key=\"concept:name\" value=\"Pay &lt;50&gt;\">\n"
      "        <string key=\"concept:name\" value=\"NESTED\"/>\n"
      "      </string>\n"
      "      <string key=\"org:resource\" value=\"clerk\"/>\n"
      "    </event>\n"
      "  </trace>\n"
      "</log>\n");

  assert(log.trace_count() == 1);
  assert(log.traces[0].case_id == "Tom & Jerry");
  assert(log.traces[0].events.size() == 1);
  assert(log.traces[0].events[0].activity == "Pay <50>");
}

void xes_importer_rejects_malformed_logs() {
  // Trace without a case attribute.
  assert(throws([] {
    (void)import_xes("<log><trace><event>"
                     "<string key=\"concept:name\" value=\"A\"/>"
                     "</event></trace></log>");
  }));
  // Event without an activity attribute.
  assert(throws([] {
    (void)import_xes("<log><trace>"
                     "<string key=\"concept:name\" value=\"c\"/>"
                     "<event><string key=\"org:resource\" value=\"x\"/></event>"
                     "</trace></log>");
  }));
  // Unterminated tag.
  assert(throws([] { (void)import_xes("<log><trace"); }));
  // An empty stream yields an empty log rather than throwing.
  assert(import_xes("").trace_count() == 0);
}

std::size_t net_activity(const minera::HeuristicsNet& net, const std::string& name) {
  for (std::size_t i = 0; i < net.activities.size(); ++i) {
    if (net.activities[i] == name) return i;
  }
  assert(false);
  return 0;
}

const minera::HeuristicDependency* net_edge(
    const minera::HeuristicsNet& net,
    const std::string& source,
    const std::string& target) {
  const auto s = net_activity(net, source);
  const auto t = net_activity(net, target);
  for (const auto& edge : net.edges) {
    if (edge.source == s && edge.target == t) return &edge;
  }
  return nullptr;
}

minera::EventLog log_of(const std::vector<std::vector<std::string>>& traces) {
  minera::EventLog log;
  std::size_t order = 0;
  for (std::size_t i = 0; i < traces.size(); ++i) {
    minera::Trace trace;
    trace.case_id = std::to_string(i);
    for (const auto& activity : traces[i]) {
      trace.events.push_back(
          minera::Event{trace.case_id, activity, "", "", order++});
    }
    log.traces.push_back(std::move(trace));
  }
  return log;
}

void heuristic_miner_keeps_a_clean_sequence() {
  // A pure A -> B -> C sequence seen often enough to clear the 0.9 threshold.
  const auto log = log_of(std::vector<std::vector<std::string>>(
      10, {"A", "B", "C"}));
  const auto net = minera::discover_heuristics_net(log);

  assert(net_edge(net, "A", "B") != nullptr);
  assert(net_edge(net, "B", "C") != nullptr);
  // No spurious reverse edges.
  assert(net_edge(net, "B", "A") == nullptr);
  assert(net_edge(net, "C", "B") == nullptr);
  assert(net_edge(net, "A", "B")->frequency == 10);
  assert(net_edge(net, "A", "B")->dependency > 0.9);
  assert(net.start_activities.size() == 1);
  assert(net.start_activities[0] == net_activity(net, "A"));
  assert(net.end_activities.size() == 1);
  assert(net.end_activities[0] == net_activity(net, "C"));
}

void heuristic_miner_filters_noisy_reverse_edges() {
  // A is mostly followed by B, with a single reversed occurrence of noise. The
  // directly-follows graph keeps both directions; the Heuristic Miner does not.
  auto traces = std::vector<std::vector<std::string>>(5, {"A", "B"});
  traces.push_back({"B", "A"});
  const auto log = log_of(traces);

  const auto dfg = minera::discover_directly_follows_graph(log);
  assert(dfg.edge_count("A", "B") == 5);
  assert(dfg.edge_count("B", "A") == 1);

  minera::HeuristicMinerOptions options;
  options.dependency_threshold = 0.5;
  const auto net = minera::discover_heuristics_net(log, options);
  assert(net_edge(net, "A", "B") != nullptr);
  assert(net_edge(net, "B", "A") == nullptr);
}

void heuristic_miner_detects_self_loops() {
  minera::HeuristicMinerOptions options;
  options.dependency_threshold = 0.5;
  const auto log = log_of(std::vector<std::vector<std::string>>(
      3, {"A", "A", "A", "B"}));
  const auto net = minera::discover_heuristics_net(log, options);

  const auto* loop = net_edge(net, "A", "A");
  assert(loop != nullptr);
  assert(loop->frequency == 6);  // two A>A per trace, three traces
  assert(net_edge(net, "A", "B") != nullptr);
}

void heuristic_miner_handles_an_empty_log() {
  const auto net = minera::discover_heuristics_net(minera::EventLog{});
  assert(net.activities.empty());
  assert(net.edges.empty());
  assert(net.start_activities.empty());
  assert(net.end_activities.empty());
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
  inductive_miner_infrequent_recovers_a_noisy_sequence();
  inductive_miner_infrequent_leaves_a_clean_log_untouched();
  petri_net_translates_sequence();
  petri_net_adds_silent_split_and_join_for_parallel();
  alpha_miner_discovers_a_sequence();
  alpha_miner_shares_a_place_for_exclusive_choice();
  alpha_miner_keeps_parallel_branches_on_separate_places();
  alpha_miner_drops_short_loop_activities();
  alpha_miner_handles_an_empty_log();
  json_export_carries_the_petri_net_contract();
  conformance_is_perfect_on_the_discovery_log();
  conformance_is_perfect_for_every_operator();
  conformance_counts_unmodeled_activities();
  conformance_penalizes_a_deviating_trace();
  precision_is_high_for_a_tight_model_and_low_for_a_flower();
  json_export_carries_the_conformance_contract();
  conformance_handles_an_empty_log();
  alignment_is_perfect_for_every_operator();
  alignment_diagnoses_a_skipped_activity();
  alignment_diagnoses_an_inserted_activity();
  json_export_carries_the_alignment_contract();
  final_marking_reachable_distinguishes_sound_nets();
  alignment_scores_a_sound_alpha_net();
  alignment_flags_an_unsound_net();
  alignment_flags_an_exhausted_search_on_an_unbounded_net();
  alignment_handles_an_empty_log();
  alignment_treats_an_empty_log_as_perfect_for_every_model();
  heuristics_net_becomes_a_sound_workflow_net();
  heuristics_net_is_always_sound_even_when_it_drops_edges();
  heuristics_net_handles_a_self_loop();
  importer_handles_a_header_only_log();
  importer_strips_a_utf8_bom();
  dfg_handles_single_event_traces();
  importer_rejects_malformed_rows();
  filter_keeps_frequent_variants();
  filter_keeps_only_the_top_variant();
  filter_projects_activities();
  filter_applies_length_and_endpoint_constraints();
  xes_importer_reads_traces_and_events();
  xes_importer_reads_a_file_from_disk();
  xes_importer_sorts_events_by_timestamp();
  xes_importer_decodes_entities_and_ignores_nesting();
  xes_importer_rejects_malformed_logs();
  heuristic_miner_keeps_a_clean_sequence();
  heuristic_miner_filters_noisy_reverse_edges();
  heuristic_miner_detects_self_loops();
  heuristic_miner_handles_an_empty_log();
  return 0;
}
