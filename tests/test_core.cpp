// Keep assertions live even in NDEBUG (Release) builds: this file *is* the
// test, so its asserts must never compile away. Must precede <cassert>.
#undef NDEBUG

#include "minera/alpha_miner.hpp"
#include "minera/conformance.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/directly_follows_graph.hpp"
#include "minera/eventually_follows.hpp"
#include "minera/heuristic_miner.hpp"
#include "minera/inductive_miner.hpp"
#include "minera/json_export.hpp"
#include "minera/log_filter.hpp"
#include "minera/petri_net.hpp"
#include "minera/playout.hpp"
#include "minera/pnml.hpp"
#include "minera/process_tree.hpp"
#include "minera/timestamp.hpp"
#include "minera/xes_event_log_importer.hpp"
#include "minera/xes_export.hpp"

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

void inductive_miner_infrequent_filters_an_edge_at_the_cutoff() {
  // Node `a` has a dominant successor `b` (ten a-b-c traces) and an infrequent
  // shortcut straight to `c` (two a-c traces). IMf keeps an outgoing edge only
  // when its weight clears the threshold times the node's strongest outgoing
  // weight: 0.15 * 10 = 1.5 and 0.25 * 10 = 2.5 bracket the shortcut's weight of
  // 2. So a 0.15 threshold keeps the shortcut — b becomes skippable — while 0.25
  // drops it as noise and recovers the clean chain. This pins the direction of
  // the cutoff: raising the threshold removes infrequent behaviour, never adds
  // it. (The exact integer boundary is left untested on purpose: f * weight lands
  // on floating-point values like 0.1 * 10 whose side of >= is not meaningful.)
  std::vector<std::vector<std::string>> variants(10, {"a", "b", "c"});
  variants.push_back({"a", "c"});
  variants.push_back({"a", "c"});
  assert(mine_tree_imf(variants, 0.15) == "->(a, X(tau, b), c)");
  assert(mine_tree_imf(variants, 0.25) == "->(a, b, c)");
}

void inductive_miner_infrequent_terminates_on_a_filtered_exclusive_cut() {
  // The noise filter drops a->c (1 vs a's dominant a->b at 10), so the
  // filtered graph falls apart into {a, b} and {c} and an exclusive cut
  // fires even though the a-c trace spans both groups. The split must route
  // that trace by majority and project it onto the winning group; routing it
  // whole (by its first activity) reproduces the parent sublog verbatim and
  // recursed forever.
  std::vector<std::vector<std::string>> variants(10, {"a", "b"});
  variants.push_back({"a", "c"});
  assert(mine_tree_imf(variants, 0.5) == "->(a, X(b, c))");
}

void inductive_miner_infrequent_keeps_structure_the_filter_would_destroy() {
  // a and b are genuinely parallel, but at threshold 0.5 the filter drops b
  // as a rare start and a as a rare end, which kills the parallel cut. IMf
  // filters only when the unfiltered relations have no cut, so the parallel
  // structure must survive a high threshold instead of collapsing to a
  // flower.
  std::vector<std::vector<std::string>> variants(10, {"a", "b"});
  variants.push_back({"b", "a"});
  variants.push_back({"b", "a"});
  assert(mine_tree_imf(variants, 0.5) == "+(a, b)");
}

std::string mine_tree_imd(const std::vector<std::vector<std::string>>& variants,
                          double noise_threshold = 0.0) {
  const auto log = log_from_variants(variants);
  minera::InductiveMinerOptions options;
  options.noise_threshold = noise_threshold;
  options.variant = minera::InductiveMinerVariant::kDfg;
  const auto tree = minera::discover_process_tree(log, options);
  return render(tree, tree.root());
}

void inductive_miner_dfg_matches_log_recursion_on_structured_logs() {
  // On logs whose structure the directly-follows graph fully captures, the
  // graph-projection recursion must find the same tree as the log recursion.
  assert(mine_tree_imd({{"a", "b", "c"}}) == "->(a, b, c)");
  assert(mine_tree_imd({{"a", "b", "d"}, {"a", "c", "d"}}) ==
         "->(a, X(b, c), d)");
  assert(mine_tree_imd({{"a", "b", "c", "d"}, {"a", "c", "b", "d"}}) ==
         "->(a, +(b, c), d)");
  assert(mine_tree_imd({{"a"}, {"a", "b", "a"}, {"a", "b", "a", "b", "a"}}) ==
         "*(a, b)");
  // The flower fall-through is shared too.
  assert(mine_tree_imd({{"a", "b"}, {"b", "c"}, {"c", "a"}}) ==
         "*(tau, a, b, c)");
}

void inductive_miner_dfg_skips_empty_traces_at_the_root() {
  // A directly-follows graph cannot see empty traces, so IMd decides the skip
  // once at the root instead of riding it through every split like the log
  // recursion (which yields "->(X(tau, a), X(tau, b))" here).
  assert(mine_tree_imd({{}, {"a", "b"}}) == "X(tau, ->(a, b))");
}

void inductive_miner_dfg_filters_noise_on_fallthrough() {
  // The same noisy sequence the IMf test recovers: the rare a->c / c->a hops
  // leave no cut, so the sub-graph is filtered and re-cut. The self-loop base
  // case and the filter run on graph counts alone.
  std::vector<std::vector<std::string>> variants(20, {"a", "b", "c", "d"});
  variants.push_back({"a", "c", "a", "b", "c", "d"});
  assert(mine_tree_imd(variants, 0.0) != "->(a, b, c, d)");
  assert(mine_tree_imd(variants, 0.2) == "->(a, b, c, d)");
}

void inductive_miner_infrequent_drops_a_stray_edge_from_an_end_activity() {
  // The rare trace adds a->c and c->a hops. c ends twenty-one traces, so
  // ending is its dominant behaviour and the stray c->a edge (its only
  // outgoing edge) is noise relative to it. Without weighing the end count,
  // c->a could never be filtered and the unresolvable cycle would collapse
  // this log to a flower.
  std::vector<std::vector<std::string>> variants(20, {"a", "b", "c"});
  variants.push_back({"a", "c", "a", "b", "c"});
  assert(mine_tree_imf(variants, 0.2) == "->(a, b, c)");
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

void alpha_miner_classic_drops_short_loop_activities() {
  // A length-two loop (b after a, a after b) makes a and b parallel under the
  // footprint, so the classic algorithm cannot place either — the textbook
  // short-loop limitation. The transitions still exist but carry no arcs.
  const auto net = minera::discover_petri_net(
      log_from_variants({{"x", "a", "b", "a", "b", "a", "y"}, {"x", "a", "y"}}),
      {.short_loops = false});
  const auto* a = transition_for(net, "a");
  const auto* b = transition_for(net, "b");
  assert(a && b);
  assert(b->inputs.empty() && b->outputs.empty());
}

void alpha_plus_attaches_a_length_one_loop() {
  // t repeats directly between a and b, so classic Alpha sees t > t and can
  // never place it. Alpha+ mines the net without t, then re-attaches t as a
  // self loop on the place between its neighbours a and b.
  const auto net = minera::discover_petri_net(
      log_from_variants({{"a", "t", "t", "b"}, {"a", "b"}}));
  const auto* a = transition_for(net, "a");
  const auto* b = transition_for(net, "b");
  const auto* t = transition_for(net, "t");
  assert(a && b && t);
  // The surrounding flow is the plain a -> b sequence net.
  assert(net.place_count() == 3);
  assert(a->outputs.size() == 1 && a->outputs == b->inputs);
  // t borrows the token from that middle place and puts it straight back.
  assert(t->inputs == a->outputs && t->outputs == a->outputs);
}

void alpha_plus_discovers_a_length_two_loop() {
  // a and b alternate (a b a b a): both "a b a" and "b a b" occur, which true
  // concurrency of single occurrences cannot produce, so Alpha+ reads causality
  // in both directions and wires the loop instead of dropping the arcs.
  const auto net = minera::discover_petri_net(
      log_from_variants({{"x", "a", "b", "a", "b", "a", "y"}, {"x", "a", "y"}}));
  const auto* a = transition_for(net, "a");
  const auto* b = transition_for(net, "b");
  assert(a && b);
  // b loops back: its input is a's output place and its output is a's input.
  assert(a->outputs.size() == 1 && b->inputs == a->outputs);
  assert(a->inputs.size() == 1 && b->outputs == a->inputs);
}

void alpha_plus_matches_classic_without_short_loops() {
  // On a short-loop-free log the plus relations collapse to the classic ones,
  // so both variants must produce the same net.
  const auto log = log_from_variants(
      {{"a", "b", "c", "e"}, {"a", "c", "b", "e"}, {"a", "d", "e"}});
  const auto plus = minera::discover_petri_net(log);
  const auto classic = minera::discover_petri_net(log, {.short_loops = false});
  assert(plus.place_count() == classic.place_count());
  assert(plus.transitions().size() == classic.transitions().size());
  for (std::size_t i = 0; i < plus.transitions().size(); ++i) {
    assert(plus.transitions()[i].inputs == classic.transitions()[i].inputs);
    assert(plus.transitions()[i].outputs == classic.transitions()[i].outputs);
  }
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

void generalization_rewards_a_well_exercised_model() {
  // The a -> b model has exactly two transitions, each firing once per trace.
  // A single trace leaves both barely observed (1/sqrt(1) each), the floor of
  // the measure; four traces halve each rarity term.
  const auto once = log_from_variants({{"a", "b"}});
  const auto tree = minera::discover_process_tree(once);
  const auto sparse = minera::check_conformance(once, tree);
  assert(sparse.generalization < 0.001);

  const auto often =
      log_from_variants({{"a", "b"}, {"a", "b"}, {"a", "b"}, {"a", "b"}});
  const auto dense = minera::check_conformance(often, tree);
  assert(dense.generalization > 0.499 && dense.generalization < 0.501);
}

void simplicity_reflects_routing_density() {
  // A pure sequence needs no arcs beyond the two-per-node chaining baseline,
  // so it sits at the measure's ceiling.
  const auto sequence_log = log_from_variants({{"a", "b", "c"}});
  const auto sequence = minera::check_conformance(
      sequence_log, minera::discover_process_tree(sequence_log));
  assert(sequence.simplicity > 0.999);

  // The flower fallback funnels every activity through shared loop places,
  // pushing the mean degree past the baseline.
  const auto flower_log =
      log_from_variants({{"a", "b"}, {"b", "c"}, {"c", "a"}});
  const auto flower = minera::check_conformance(
      flower_log, minera::discover_process_tree(flower_log));
  assert(flower.simplicity < sequence.simplicity);
  assert(flower.simplicity > 0.0);
}

void footprints_match_the_discovery_log() {
  // The inductive model perfectly fits its log, so every succession the log
  // shows the model also allows: footprint fitness is 1 with no log-only gaps.
  const auto log = log_from_variants(
      {{"a", "b", "c"}, {"a", "c", "b"}});  // b and c parallel
  const auto tree = minera::discover_process_tree(log);
  const auto footprints = minera::check_footprints(log, tree);
  assert(footprints.fitness > 0.999);
  assert(footprints.log_only.empty());
  assert(footprints.log_relations > 0);
  assert(footprints.shared_relations == footprints.log_relations);
}

void footprints_flag_a_forbidden_succession() {
  // Model is a strict a -> b -> c chain. A log that also puts c before b shows
  // a succession (c -> b) the model forbids, so footprint fitness drops and the
  // deviation is reported.
  const auto model_log = log_from_variants({{"a", "b", "c"}});
  const auto tree = minera::discover_process_tree(model_log);

  const auto log = log_from_variants({{"a", "c", "b"}});
  const auto footprints = minera::check_footprints(log, tree);
  assert(footprints.fitness < 1.0);
  bool found_cb = false;
  for (const auto& dev : footprints.log_only) {
    if (dev.source == "c" && dev.target == "b") found_cb = true;
  }
  assert(found_cb);
}

void footprints_detect_a_too_loose_model() {
  // A directly-follows cycle yields a flower model that allows every activity to
  // follow every other, so it has successions the tight log never shows:
  // footprint precision falls well below 1 and model-only pairs are reported.
  const auto log = log_from_variants({{"a", "b"}, {"b", "c"}, {"c", "a"}});
  const auto tree = minera::discover_process_tree(log);
  const auto footprints = minera::check_footprints(log, tree);
  assert(footprints.precision < 1.0);
  assert(!footprints.model_only.empty());
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

void pnml_round_trip_preserves_replay_behavior() {
  // A net written by to_pnml and read back by parse_pnml must replay the log
  // identically — same token counts, same footprint relation.
  const auto log = log_from_variants(
      {{"a", "b", "c", "d"}, {"a", "c", "b", "d"}, {"a", "e", "d"}});
  const auto tree = minera::discover_process_tree(log);
  const auto net = minera::to_petri_net(tree);
  const auto reread = minera::parse_pnml(minera::to_pnml(net));

  assert(reread.place_count() == net.place_count());
  assert(reread.transitions().size() == net.transitions().size());

  const auto before = minera::check_conformance(log, net);
  const auto after = minera::check_conformance(log, reread);
  assert(before.fitness == after.fitness);
  assert(before.produced == after.produced);
  assert(before.missing == after.missing);

  const auto footprints_before = minera::check_footprints(log, net);
  const auto footprints_after = minera::check_footprints(log, reread);
  assert(footprints_before.fitness == footprints_after.fitness);
  assert(footprints_before.model_relations == footprints_after.model_relations);

  assert(minera::final_marking_reachable(reread));
  assert(minera::check_alignment(log, reread).fitness == 1.0);
}

// A hand-written PNML the way other tools emit it: explicit markings, an
// invisible routing transition, an escaped label, and single quotes.
constexpr const char* kForeignPnml = R"(<?xml version="1.0"?>
<pnml xmlns="http://www.pnml.org/version-2009/grammar/pnml">
  <net id='n1' type="http://www.pnml.org/version-2009/grammar/pnmlcoremodel">
    <name><text>foreign</text></name>
    <page id="g1">
      <place id="start"><initialMarking><text>1</text></initialMarking></place>
      <place id="mid"/>
      <place id="end"/>
      <transition id="t_a"><name><text>a &amp; go</text></name></transition>
      <transition id="t_skip">
        <name><text>skip</text></name>
        <toolspecific tool="ProM" version="6.4" activity="$invisible$"/>
      </transition>
      <arc id="1" source="start" target="t_a"/>
      <arc id="2" source="t_a" target="mid"/>
      <arc id="3" source="mid" target="t_skip"/>
      <arc id="4" source="t_skip" target="end"/>
    </page>
    <finalmarkings>
      <marking><place idref="end"><text>1</text></place></marking>
    </finalmarkings>
  </net>
</pnml>)";

void parse_pnml_reads_a_foreign_net() {
  const auto net = minera::parse_pnml(kForeignPnml);
  assert(net.place_count() == 3);
  assert(net.transitions().size() == 2);
  // The toolspecific $invisible$ flag wins over the "skip" label.
  assert((net.activities() == std::vector<std::string>{"a & go"}));
  assert(!net.transitions()[0].silent());
  assert(net.transitions()[1].silent());
  assert(net.source() == 0);  // "start", the marked place
  assert(net.sink() == 2);    // "end", from <finalmarkings>

  // The single visible activity replays a matching one-event log perfectly.
  const auto log = log_from_variants({{"a & go"}});
  assert(minera::check_conformance(log, net).fitness == 1.0);

  // Against a log with a different activity, both deviations are reported:
  // "x" is unmodeled (log-only) and "a & go" is unobserved (model-only).
  const auto other = log_from_variants({{"x", "a & go"}});
  const auto footprints = minera::check_footprints(other, net);
  assert(footprints.fitness == 0.0);
  assert(footprints.log_relations == 1);
}

void parse_pnml_infers_missing_markings() {
  // No initialMarking and no finalmarkings: the unique place without incoming
  // arcs is the source, the unique place without outgoing arcs the sink.
  const auto net = minera::parse_pnml(R"(<pnml><net id="n"><page id="g">
    <place id="p0"/><place id="p1"/>
    <transition id="t"><name><text>a</text></name></transition>
    <arc id="1" source="p0" target="t"/>
    <arc id="2" source="t" target="p1"/>
  </page></net></pnml>)");
  assert(net.source() == 0);
  assert(net.sink() == 1);
  assert(minera::check_conformance(log_from_variants({{"a"}}), net).fitness ==
         1.0);
}

void parse_pnml_rejects_malformed_nets() {
  // An arc referencing an id that exists as neither place nor transition.
  assert(throws([] {
    (void)minera::parse_pnml(R"(<pnml><net id="n"><page id="g">
      <place id="p0"/><place id="p1"/>
      <transition id="t"/>
      <arc id="1" source="p0" target="ghost"/>
      <arc id="2" source="t" target="p1"/>
    </page></net></pnml>)");
  }));
  // Two initially marked places: not a workflow net.
  assert(throws([] {
    (void)minera::parse_pnml(R"(<pnml><net id="n"><page id="g">
      <place id="p0"><initialMarking><text>1</text></initialMarking></place>
      <place id="p1"><initialMarking><text>1</text></initialMarking></place>
      <place id="p2"/>
      <transition id="t"/>
      <arc id="1" source="p0" target="t"/>
      <arc id="2" source="p1" target="t"/>
      <arc id="3" source="t" target="p2"/>
    </page></net></pnml>)");
  }));
  // No net at all.
  assert(throws([] { (void)minera::parse_pnml("<pnml></pnml>"); }));
}

void xes_export_round_trips_the_log() {
  // A log written by to_xes and read back by the XES importer must carry the
  // same cases, activities, resources, and timestamp instants — including
  // XML metacharacters in every field.
  minera::EventLog log;
  minera::Trace trace;
  trace.case_id = "case <1> & \"two\"";
  minera::Event first;
  first.case_id = trace.case_id;
  first.activity = "check & <verify>";
  first.timestamp = "2024-03-01 10:00:00+02:00";  // offset form, space sep
  first.resource = "alice 'a'";
  minera::Event second;
  second.case_id = trace.case_id;
  second.activity = "ship";
  second.timestamp = "2024-03-01T09:30:00.250Z";
  trace.events = {first, second};
  log.traces.push_back(trace);

  const auto xes = minera::to_xes(log, "unit log");
  std::istringstream input(xes);
  const auto reread = minera::import_xes_event_log(input);

  assert(reread.trace_count() == 1);
  assert(reread.traces[0].case_id == trace.case_id);
  assert(reread.traces[0].events.size() == 2);
  assert(reread.traces[0].events[0].activity == first.activity);
  assert(reread.traces[0].events[0].resource == first.resource);
  assert(reread.traces[0].events[1].activity == second.activity);
  assert(reread.traces[0].events[1].resource.empty());

  // Timestamps are normalized to UTC on export, so compare instants.
  for (std::size_t i = 0; i < 2; ++i) {
    const auto before = minera::parse_timestamp(trace.events[i].timestamp);
    const auto after =
        minera::parse_timestamp(reread.traces[0].events[i].timestamp);
    assert(before && after && *before == *after);
  }
  // The offset timestamp reads back as its UTC instant in ISO form.
  assert(reread.traces[0].events[0].timestamp == "2024-03-01T08:00:00.000Z");
}

void xes_export_keeps_unparseable_timestamps_verbatim() {
  // A timestamp that isn't a recognizable instant must not be silently
  // dropped or emitted as an invalid <date>: it survives as a string
  // attribute under the same key, and an empty timestamp is omitted.
  minera::EventLog log;
  minera::Trace trace;
  trace.case_id = "c1";
  minera::Event odd;
  odd.case_id = "c1";
  odd.activity = "a";
  odd.timestamp = "yesterday-ish";
  minera::Event untimed;
  untimed.case_id = "c1";
  untimed.activity = "b";
  trace.events = {odd, untimed};
  log.traces.push_back(trace);

  const auto xes = minera::to_xes(log);
  assert(xes.find("<string key=\"time:timestamp\" value=\"yesterday-ish\"/>") !=
         std::string::npos);
  assert(xes.find("<date") == std::string::npos);
  // No resource anywhere, so the org extension is not declared.
  assert(xes.find("org.xesext") == std::string::npos);

  // The importer re-orders events by timestamp (lexicographic fallback here),
  // so find them by activity rather than position.
  std::istringstream input(xes);
  const auto reread = minera::import_xes_event_log(input);
  assert(reread.traces[0].events.size() == 2);
  for (const auto& event : reread.traces[0].events) {
    assert(event.timestamp == (event.activity == "a" ? "yesterday-ish" : ""));
  }
}

void graph_counts_rework_cases() {
  // a repeats in the first case only, b in the second only; the third case
  // has no repeats at all.
  const auto log = log_from_variants(
      {{"a", "a", "b"}, {"a", "b", "b", "b"}, {"a", "b"}});
  const auto graph = minera::discover_directly_follows_graph(log);
  assert(graph.rework_case_count("a") == 1);
  assert(graph.rework_case_count("b") == 1);
  assert(graph.rework_case_count("unknown") == 0);
}

std::uint64_t efg_count(const std::vector<minera::EventuallyFollowsEdge>& efg,
                        const minera::DirectlyFollowsGraph& graph,
                        std::string_view source, std::string_view target) {
  const auto s = graph.activity_id(source);
  const auto t = graph.activity_id(target);
  assert(s && t);
  for (const auto& edge : efg) {
    if (edge.source == *s && edge.target == *t) return edge.count;
  }
  return 0;
}

void eventually_follows_counts_every_later_occurrence() {
  // ⟨a,b,c⟩ contributes (a,b), (a,c), (b,c); ⟨a,c⟩ contributes (a,c) — so
  // a ↝ c is 2 even though a → c is direct only once.
  const auto log = log_from_variants({{"a", "b", "c"}, {"a", "c"}});
  const auto graph = minera::discover_directly_follows_graph(log);
  const auto efg = minera::discover_eventually_follows(log, graph);

  assert(efg.size() == 3);
  assert(efg_count(efg, graph, "a", "b") == 1);
  assert(efg_count(efg, graph, "a", "c") == 2);
  assert(efg_count(efg, graph, "b", "c") == 1);
  // Never observed in that order.
  assert(efg_count(efg, graph, "c", "a") == 0);
}

void eventually_follows_handles_repeated_activities() {
  // ⟨a,a,b⟩: the position pairs are (a,a), (a,b), (a,b).
  const auto log = log_from_variants({{"a", "a", "b"}});
  const auto graph = minera::discover_directly_follows_graph(log);
  const auto efg = minera::discover_eventually_follows(log, graph);

  assert(efg_count(efg, graph, "a", "a") == 1);
  assert(efg_count(efg, graph, "a", "b") == 2);
}

void alignment_precision_is_perfect_for_an_exact_model() {
  // A strict sequence model of its own log allows nothing extra at any
  // aligned state, so alignment-based precision is exactly 1.
  const auto log = log_from_variants({{"a", "b", "c"}});
  const auto net = minera::to_petri_net(minera::discover_process_tree(log));
  const auto alignment = minera::check_alignment(log, net);
  assert(alignment.computed);
  assert(alignment.fitness == 1.0);
  assert(alignment.precision == 1.0);
}

void alignment_precision_flags_unused_model_behavior() {
  // The model offers a choice of b or c after a, but this log only ever takes
  // b: at the post-a state the model enables an activity the log never shows,
  // so precision drops below 1 while fitness stays perfect.
  const auto model_log = log_from_variants({{"a", "b"}, {"a", "c"}});
  const auto net =
      minera::to_petri_net(minera::discover_process_tree(model_log));
  const auto narrow = log_from_variants({{"a", "b"}});
  const auto alignment = minera::check_alignment(narrow, net);
  assert(alignment.computed);
  assert(alignment.fitness == 1.0);
  assert(alignment.precision < 1.0);
  assert(alignment.precision > 0.0);
}

void alignment_precision_survives_a_deviating_log() {
  // "x" is not in the model at all; the alignment explains it as a log move
  // and precision is still measured at the genuinely reached states.
  const auto model_log = log_from_variants({{"a", "b"}});
  const auto net =
      minera::to_petri_net(minera::discover_process_tree(model_log));
  const auto deviating = log_from_variants({{"a", "x", "b"}});
  const auto alignment = minera::check_alignment(deviating, net);
  assert(alignment.computed);
  assert(alignment.fitness < 1.0);
  assert(alignment.precision > 0.0 && alignment.precision <= 1.0);

  // The JSON contract: the alignments object carries the precision by name.
  const auto graph = minera::discover_directly_follows_graph(deviating);
  const auto json = minera::alignment_json(alignment, graph);
  assert(json.find("\"precision\": ") != std::string::npos);
}

void playout_generates_perfectly_fitting_logs() {
  // A model with a choice and a parallel block. Every simulated trace must
  // replay perfectly on the net it came from, and 50 cases are plenty for
  // both parallel orders (or the choice branch) to show up.
  const auto log = log_from_variants(
      {{"a", "b", "c", "d"}, {"a", "c", "b", "d"}, {"a", "e", "d"}});
  const auto net = minera::to_petri_net(minera::discover_process_tree(log));

  minera::PlayoutOptions options;
  options.traces = 50;
  const auto generated = minera::playout(net, options);

  assert(generated.trace_count() == 50);
  assert(generated.traces.front().case_id == "case_1");
  assert(minera::check_conformance(generated, net).fitness == 1.0);
  const auto graph = minera::discover_directly_follows_graph(generated);
  assert(graph.variants().size() >= 2);

  // Timestamps parse and strictly increase within a case, so the generated
  // log survives a time-ordering importer unchanged.
  for (const auto& trace : generated.traces) {
    double previous = -1.0;
    for (const auto& event : trace.events) {
      const auto instant = minera::parse_timestamp(event.timestamp);
      assert(instant && *instant > previous);
      previous = *instant;
    }
  }

  // The same seed replays the same log.
  const auto replayed = minera::playout(net, options);
  for (std::size_t i = 0; i < generated.traces.size(); ++i) {
    assert(generated.traces[i].events.size() ==
           replayed.traces[i].events.size());
    for (std::size_t j = 0; j < generated.traces[i].events.size(); ++j) {
      assert(generated.traces[i].events[j].activity ==
             replayed.traces[i].events[j].activity);
    }
  }
}

void playout_simulates_an_imported_model() {
  // The foreign net has one visible activity and a silent router, so every
  // simulated case is exactly ⟨"a & go"⟩.
  const auto net = minera::parse_pnml(kForeignPnml);
  minera::PlayoutOptions options;
  options.traces = 5;
  const auto generated = minera::playout(net, options);
  assert(generated.trace_count() == 5);
  for (const auto& trace : generated.traces) {
    assert(trace.events.size() == 1);
    assert(trace.events[0].activity == "a & go");
  }
}

void json_export_carries_the_eventually_follows_contract() {
  // The frontend reads "eventually_follows" by name and expects the same
  // {source, target, count} shape as "edges".
  const auto log = log_from_variants({{"a", "b"}});
  const auto graph = minera::discover_directly_follows_graph(log);
  const auto efg = minera::discover_eventually_follows(log, graph);
  const auto json =
      minera::analysis_json("t", log, graph, nullptr, nullptr, nullptr,
                            nullptr, nullptr, nullptr, nullptr, &efg);
  assert(json.find("\"eventually_follows\": [") != std::string::npos);
  assert(json.find("{\"source\": 0, \"target\": 1, \"count\": 1}") !=
         std::string::npos);
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
       minera::check_conformance(log, net), minera::check_footprints(log, net),
       minera::check_alignment(log, net)},
  };

  const auto json =
      minera::analysis_json("test", log, graph, &tree, nullptr, nullptr,
                            &conformance);
  for (const char* key : {"\"conformance\"", "\"inductive\"", "\"sound\"",
                          "\"token_replay\"", "\"footprints\"", "\"log_only\"",
                          "\"model_only\"", "\"shared_relations\"",
                          "\"alignments\"", "\"fitness\"",
                          "\"precision\"", "\"generalization\"",
                          "\"simplicity\"", "\"fitting_traces\"",
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
       minera::check_footprints(deviating, net),
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

void alignment_prunes_an_unbounded_net_with_dead_growth() {
  // A reachable but unbounded net: a silent transition consumes the source and
  // produces the source back plus a token on an auxiliary place, so it can fire
  // forever, growing the marking without bound. The sink stays reachable (a
  // second silent transition drains the source to it), so the net clears the
  // soundness gate. The flooded markings can never reach the final marking (the
  // auxiliary tokens have no consumer), so the marking-equation heuristic
  // prunes every one of them and the alignment computes despite the flood —
  // plain Dijkstra used to exhaust its bound here.
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
  assert(alignment.computed);
  assert(!alignment.exhausted);
  // The best alignment is one log move for the unknown "x" plus the silent
  // model run to the sink.
  assert(alignment.total_cost == 1);
}

void alignment_flags_an_exhausted_search_on_an_unbounded_net() {
  // The pathological case even the marking-equation heuristic cannot prune: a
  // token-borrowing transition (gate -> gate + gate) makes the equation
  // believe the visible "x" is reachable at zero cost from every marking (its
  // incidence can conjure gate tokens that firing never could, since the gate
  // starts empty), while a source-preserving flood transition grows the
  // marking without bound and a drain keeps the flooded markings equation-
  // feasible. The whole flood sits at estimated cost zero ahead of the real
  // cost-one goal, so the aligner must hit its bound and report `exhausted`
  // (distinct from an unsound net) rather than spin forever or emit a
  // fabricated score.
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto sink = net.add_place();
  const auto aux = net.add_place();
  const auto gate = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({"x"});
  net.add_transition(std::nullopt, {source}, {source, aux});  // unbounded growth
  net.add_transition(std::nullopt, {aux}, {});     // keeps the flood feasible
  net.add_transition(std::nullopt, {source}, {});  // balances the source
  net.add_transition(std::nullopt, {source}, {sink});  // reaches the sink
  net.add_transition(std::nullopt, {gate}, {gate, gate});  // fools the equation
  net.add_transition(0, {gate}, {sink});  // "x": never actually enabled

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

void replay_degrades_gracefully_on_an_unbounded_silent_net() {
  // Token replay routes silent (tau) transitions to enable each visible step,
  // searching the silent reachability space. This net makes that space infinite:
  // a silent transition consumes the source and produces the source back plus a
  // token on an auxiliary place, so it can fire forever. The visible activity
  // "a" needs a token on a `gate` place that nothing ever produces, so the
  // silent search can never reach it and must lean on its bound to stop. Replay
  // must return a finite, deviating result (the forced firing creates missing
  // tokens) rather than spinning forever — this test would hang without the cap.
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto sink = net.add_place();
  const auto aux = net.add_place();
  const auto gate = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({"a"});
  net.add_transition(std::nullopt, {source}, {source, aux});  // unbounded silent
  net.add_transition(0, {gate}, {sink});  // visible "a", gate never produced

  const auto log = log_from_variants({{"a"}});
  const auto replay = minera::check_conformance(log, net);
  assert(replay.trace_count == 1);
  assert(replay.missing > 0);       // the unreachable gate forces a token
  assert(replay.fitness < 1.0);     // so the trace does not fit
  assert(replay.fitting_traces == 0);
}

void heuristics_net_becomes_a_replayable_workflow_net() {
  // The Heuristic Miner emits a dependency graph; converting it to a workflow
  // net must yield a replayable model so both conformance methods work. A
  // strongly-sequential log (every dependency clears the 0.9 threshold)
  // converts to a clean chain — one place per edge, no silent routing at all —
  // that the log fits perfectly.
  const std::vector<std::vector<std::string>> sequence(12,
                                                       {"a", "b", "c", "d"});
  const auto log = log_from_variants(sequence);
  const auto heuristics = minera::discover_heuristics_net(log);
  const auto net = minera::to_petri_net(heuristics);

  // Source and sink plus a place per dependency edge.
  assert(net.place_count() == 2 + heuristics.edges.size());
  for (const auto& transition : net.transitions()) {
    assert(!transition.silent());
  }

  const auto replay = minera::check_conformance(log, net);
  assert(replay.fitness > 0.999);

  const auto alignment = minera::check_alignment(log, net);
  assert(alignment.computed);
  assert(alignment.fitness > 0.999);
}

void heuristics_net_stays_replayable_when_it_drops_edges() {
  // With a high threshold the Heuristic Miner drops weak dependencies, so the
  // log no longer fits perfectly — but the converted net must still replay and
  // align (a finite cost rather than a refusal).
  const auto log = log_from_variants(
      {{"a", "b", "d"}, {"a", "c", "d"}, {"a", "b", "d"}});
  const auto net = minera::to_petri_net(minera::discover_heuristics_net(log));

  const auto alignment = minera::check_alignment(log, net);
  assert(alignment.computed);
  assert(alignment.fitness >= 0.0 && alignment.fitness <= 1.0);
}

void heuristics_net_translates_an_and_split_and_join() {
  // b and c are concurrent between a and d, and the AND measure sees it: b and
  // c follow each other constantly relative to a's split. The translation must
  // wire a real AND split/join — the former one-token state machine could
  // replay only one branch, so every trace here counted as deviating.
  std::vector<std::vector<std::string>> variants(6, {"a", "b", "c", "d"});
  for (int i = 0; i < 6; ++i) variants.push_back({"a", "c", "b", "d"});
  const auto log = log_from_variants(variants);
  const auto heuristics = minera::discover_heuristics_net(log);
  const auto net = minera::to_petri_net(heuristics);

  // a fires both branch places at once; d waits for both.
  const auto* a = transition_for(net, "a");
  const auto* d = transition_for(net, "d");
  assert(a && d);
  assert(a->outputs.size() == 2);
  assert(d->inputs.size() == 2);

  assert(minera::check_conformance(log, net).fitness > 0.999);
  const auto alignment = minera::check_alignment(log, net);
  assert(alignment.computed);
  assert(alignment.fitness > 0.999);
}

void heuristics_net_keeps_exclusive_branches_exclusive() {
  // b and c never follow each other, so the AND measure keeps them in separate
  // bundles: after a exactly one of them may run. Both single-branch traces
  // fit; a trace running b *and* c must deviate.
  std::vector<std::vector<std::string>> variants(6, {"a", "b", "d"});
  for (int i = 0; i < 6; ++i) variants.push_back({"a", "c", "d"});
  const auto log = log_from_variants(variants);
  const auto net = minera::to_petri_net(minera::discover_heuristics_net(log));

  assert(minera::check_conformance(log, net).fitness > 0.999);
  const auto both = log_from_variants({{"a", "b", "c", "d"}});
  assert(minera::check_conformance(both, net).fitness < 1.0);
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

void filter_projects_by_resource() {
  // Two events per case, one by Alice and one by Bob; an unattributed event
  // carries no resource at all.
  minera::EventLog log;
  minera::Trace trace;
  trace.case_id = "1";
  trace.events.push_back(minera::Event{"1", "a", "", "Alice", 0});
  trace.events.push_back(minera::Event{"1", "b", "", "Bob", 1});
  trace.events.push_back(minera::Event{"1", "c", "", "", 2});
  log.traces.push_back(trace);

  minera::LogFilterOptions keep;
  keep.keep_resources = {"Alice"};
  assert(keep.any());
  const auto only_alice = minera::filter_event_log(log, keep);
  assert(only_alice.trace_count() == 1);
  // Only Alice's event survives; Bob's and the unattributed event are dropped.
  assert((activity_names(only_alice.traces.front()) ==
          std::vector<std::string>{"a"}));

  minera::LogFilterOptions remove;
  remove.remove_resources = {"Bob"};
  const auto without_bob = minera::filter_event_log(log, remove);
  // Bob's event goes; the unattributed event is never dropped by a resource
  // remove-set.
  assert((activity_names(without_bob.traces.front()) ==
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

void filter_rework_keeps_cases_with_a_repeated_activity() {
  const auto log = log_from_variants(
      {{"a", "b", "b", "c"}, {"a", "b", "c"}, {"a", "b", "b", "b", "c"}});

  minera::LogFilterOptions rework;
  rework.rework_activity = "b";  // default minimum of 2 = any repetition
  assert(rework.any());
  const auto repeated = minera::filter_event_log(log, rework);
  assert(repeated.trace_count() == 2);

  rework.rework_min_occurrences = 3;
  const auto thrice = minera::filter_event_log(log, rework);
  assert(thrice.trace_count() == 1);
  assert(thrice.traces.front().events.size() == 5);

  // An activity the log never repeats keeps nothing.
  rework.rework_activity = "a";
  rework.rework_min_occurrences = 2;
  assert(minera::filter_event_log(log, rework).trace_count() == 0);
}

void filter_between_extracts_subtraces() {
  // One case passes a->b twice, so it yields two subcases; a case that never
  // closes a segment yields none and vanishes.
  const auto log = log_from_variants(
      {{"x", "a", "m", "b", "y", "a", "n", "b"}, {"x", "a", "m"}});

  minera::LogFilterOptions between;
  between.between_start = "a";
  between.between_end = "b";
  assert(between.any());
  const auto cut = minera::filter_event_log(log, between);
  assert(cut.trace_count() == 2);
  assert((activity_names(cut.traces[0]) ==
          std::vector<std::string>{"a", "m", "b"}));
  assert((activity_names(cut.traces[1]) ==
          std::vector<std::string>{"a", "n", "b"}));
  // Several segments from one case become distinct cases.
  assert(cut.traces[0].case_id != cut.traces[1].case_id);

  // A single segment keeps its original case id.
  const auto single = log_from_variants({{"x", "a", "m", "b", "y"}});
  const auto one = minera::filter_event_log(single, between);
  assert(one.trace_count() == 1);
  assert(one.traces.front().case_id == single.traces.front().case_id);
  assert((activity_names(one.traces.front()) ==
          std::vector<std::string>{"a", "m", "b"}));
}

void filter_between_handles_equal_endpoints_and_rejects_one_sided() {
  // start == end: each segment runs from one occurrence to the next.
  const auto log = log_from_variants({{"a", "x", "a", "y", "a"}});
  minera::LogFilterOptions between;
  between.between_start = "a";
  between.between_end = "a";
  const auto cut = minera::filter_event_log(log, between);
  assert(cut.trace_count() == 2);
  assert((activity_names(cut.traces[0]) ==
          std::vector<std::string>{"a", "x", "a"}));
  assert((activity_names(cut.traces[1]) ==
          std::vector<std::string>{"a", "y", "a"}));

  assert(throws([&] {
    minera::LogFilterOptions one_sided;
    one_sided.between_start = "a";
    (void)minera::filter_event_log(log_from_variants({{"a"}}), one_sided);
  }));
}

// A log for the time-range filter: one January case, one case straddling the
// January/February boundary, one February case, and one untimed case.
minera::EventLog timed_filter_log() {
  minera::EventLog log;
  const std::vector<std::vector<std::pair<std::string, std::string>>> cases = {
      {{"a", "2024-01-01"}, {"b", "2024-01-02"}},
      {{"a", "2024-01-30"}, {"b", "2024-02-02"}},
      {{"a", "2024-02-05"}, {"b", "2024-02-06"}},
      {{"a", ""}, {"b", ""}},
  };
  std::size_t case_index = 0;
  for (const auto& events : cases) {
    minera::Trace trace;
    trace.case_id = std::to_string(case_index++);
    for (const auto& [activity, timestamp] : events) {
      minera::Event event;
      event.case_id = trace.case_id;
      event.activity = activity;
      event.timestamp = timestamp;
      trace.events.push_back(event);
    }
    log.traces.push_back(std::move(trace));
  }
  return log;
}

void filter_time_range_trims_events() {
  const auto log = timed_filter_log();
  minera::LogFilterOptions options;
  options.time_from = "2024-01-01";
  options.time_to = "2024-01-31";
  assert(options.any());

  // events mode keeps only the January events; the straddling case shrinks to
  // its January prefix, and the February and untimed cases vanish entirely.
  const auto filtered = minera::filter_event_log(log, options);
  assert(filtered.trace_count() == 2);
  assert((activity_names(filtered.traces[0]) ==
          std::vector<std::string>{"a", "b"}));
  assert((activity_names(filtered.traces[1]) == std::vector<std::string>{"a"}));
}

void filter_time_range_selects_whole_traces() {
  const auto log = timed_filter_log();
  minera::LogFilterOptions options;
  options.time_from = "2024-01-01";
  options.time_to = "2024-01-31";

  // Contained: only the case that lies fully inside January survives, whole.
  options.time_mode = minera::TimeFilterMode::traces_contained;
  const auto contained = minera::filter_event_log(log, options);
  assert(contained.trace_count() == 1);
  assert(contained.traces.front().case_id == "0");
  assert(contained.traces.front().events.size() == 2);

  // Intersecting: the straddling case survives too, keeping its February tail.
  options.time_mode = minera::TimeFilterMode::traces_intersecting;
  const auto intersecting = minera::filter_event_log(log, options);
  assert(intersecting.trace_count() == 2);
  assert(intersecting.traces[1].case_id == "1");
  assert(intersecting.traces[1].events.size() == 2);
}

void filter_time_range_bounds_are_inclusive_and_one_sided() {
  const auto log = timed_filter_log();

  minera::LogFilterOptions at_instant;
  at_instant.time_from = "2024-01-02";
  at_instant.time_to = "2024-01-02";
  const auto exact = minera::filter_event_log(log, at_instant);
  assert(exact.trace_count() == 1);
  assert((activity_names(exact.traces.front()) ==
          std::vector<std::string>{"b"}));

  minera::LogFilterOptions from_only;
  from_only.time_from = "2024-02-01";
  const auto tail = minera::filter_event_log(log, from_only);
  assert(tail.trace_count() == 2);  // the straddler's tail and the February case
}

void filter_time_range_rejects_a_malformed_bound() {
  assert(throws([] {
    minera::LogFilterOptions options;
    options.time_from = "not-a-date";
    (void)minera::filter_event_log(timed_filter_log(), options);
  }));
  assert((minera::parse_time_filter_mode("traces_contained") ==
          minera::TimeFilterMode::traces_contained));
  assert(!minera::parse_time_filter_mode("bogus"));
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

void csv_importer_captures_extra_columns_as_attributes() {
  // Columns beyond case/activity/timestamp/resource become generic event
  // attributes keyed by their header; blank cells mean "no attribute".
  const auto path = write_temp_log(
      "case_id,activity,timestamp,resource,channel,amount\n"
      "1,A,2026-01-01T00:00:00,alice,web,100\n"
      "1,B,2026-01-01T00:01:00,bob,,250\n");

  const auto log = minera::import_csv_event_log(path);
  const auto& events = log.traces.front().events;
  assert(events[0].attributes.size() == 2);
  assert(events[0].attributes[0].key == "channel");
  assert(events[0].attributes[0].value == "web");
  assert(events[0].attributes[0].kind == minera::AttributeKind::kString);
  assert(*events[0].attribute("amount") == "100");
  // The blank channel cell leaves only the amount on the second event, and
  // the first-class fields never double as generic attributes.
  assert(events[1].attributes.size() == 1);
  assert(events[1].attribute("channel") == nullptr);
  assert(*events[1].attribute("amount") == "250");
  assert(events[1].attribute("resource") == nullptr);
  assert(events[1].resource == "bob");
}

void xes_importer_captures_generic_event_attributes() {
  // Scalar event attributes beyond the standard activity/timestamp/resource
  // keys are kept with their XES type; nested sub-attributes and trace-level
  // attributes are not event attributes.
  const auto log = import_xes(
      "<log>\n"
      "  <trace>\n"
      "    <string key=\"concept:name\" value=\"c\"/>\n"
      "    <string key=\"customer\" value=\"trace-level\"/>\n"
      "    <event>\n"
      "      <string key=\"concept:name\" value=\"A\"/>\n"
      "      <string key=\"org:resource\" value=\"clerk\"/>\n"
      "      <string key=\"channel\" value=\"web\">\n"
      "        <string key=\"nested\" value=\"ignored\"/>\n"
      "      </string>\n"
      "      <int key=\"amount\" value=\"100\"/>\n"
      "      <boolean key=\"approved\" value=\"true\"/>\n"
      "    </event>\n"
      "  </trace>\n"
      "</log>\n");

  const auto& event = log.traces.front().events.front();
  assert(event.activity == "A");
  assert(event.resource == "clerk");
  assert(event.attributes.size() == 3);
  assert(event.attributes[0].key == "channel");
  assert(event.attributes[0].kind == minera::AttributeKind::kString);
  assert(event.attributes[1].key == "amount");
  assert(event.attributes[1].value == "100");
  assert(event.attributes[1].kind == minera::AttributeKind::kInt);
  assert(event.attributes[2].key == "approved");
  assert(event.attributes[2].kind == minera::AttributeKind::kBoolean);
  assert(event.attribute("nested") == nullptr);
  assert(event.attribute("customer") == nullptr);
}

void xes_export_round_trips_generic_attributes() {
  // Attributes survive an export/import cycle with their XES types, so an
  // <int> stays an <int>; a date that no longer parses degrades to a string
  // instead of emitting an invalid xs:dateTime.
  minera::EventLog log;
  minera::Trace trace;
  trace.case_id = "c1";
  minera::Event event;
  event.case_id = "c1";
  event.activity = "a";
  event.attributes.push_back({"amount", "100", minera::AttributeKind::kInt});
  event.attributes.push_back(
      {"channel <&>", "web \"x\"", minera::AttributeKind::kString});
  event.attributes.push_back(
      {"due", "2026-03-01T10:00:00Z", minera::AttributeKind::kDate});
  event.attributes.push_back(
      {"odd-date", "someday", minera::AttributeKind::kDate});
  trace.events.push_back(event);
  log.traces.push_back(trace);

  const auto xes = minera::to_xes(log);
  assert(xes.find("<int key=\"amount\" value=\"100\"/>") != std::string::npos);
  assert(xes.find("<date key=\"due\" value=\"2026-03-01T10:00:00.000Z\"/>") !=
         std::string::npos);
  assert(xes.find("<string key=\"odd-date\" value=\"someday\"/>") !=
         std::string::npos);

  std::istringstream input(xes);
  const auto reread = minera::import_xes_event_log(input);
  const auto& back = reread.traces.front().events.front();
  assert(back.attributes.size() == 4);
  assert(back.attributes[0].kind == minera::AttributeKind::kInt);
  assert(back.attributes[0].value == "100");
  assert(*back.attribute("channel <&>") == "web \"x\"");
  assert(back.attributes[2].kind == minera::AttributeKind::kDate);
  // The unparseable date came back as the string it was written as.
  assert(back.attributes[3].kind == minera::AttributeKind::kString);
  assert(back.attributes[3].value == "someday");
}

// A three-event trace with a channel attribute on two events: web on a,
// phone on b, nothing on c.
minera::EventLog attribute_filter_log() {
  minera::EventLog log;
  minera::Trace trace;
  trace.case_id = "1";
  trace.events.push_back(
      minera::Event{"1", "a", "", "", 0, {{"channel", "web", {}}}});
  trace.events.push_back(
      minera::Event{"1", "b", "", "", 1, {{"channel", "phone", {}}}});
  trace.events.push_back(minera::Event{"1", "c", "", "", 2});
  log.traces.push_back(trace);
  return log;
}

void filter_attribute_events_mode_keeps_and_removes() {
  const auto log = attribute_filter_log();

  minera::LogFilterOptions keep;
  keep.attribute_key = "channel";
  keep.keep_attribute_values = {"web"};
  assert(keep.any());
  const auto only_web = minera::filter_event_log(log, keep);
  // Only the web event survives a keep-set; the attribute-less event drops.
  assert((activity_names(only_web.traces.front()) ==
          std::vector<std::string>{"a"}));

  minera::LogFilterOptions remove;
  remove.attribute_key = "channel";
  remove.remove_attribute_values = {"phone"};
  const auto no_phone = minera::filter_event_log(log, remove);
  // The phone event goes; the attribute-less event is never dropped by a
  // remove-set.
  assert((activity_names(no_phone.traces.front()) ==
          std::vector<std::string>{"a", "c"}));

  minera::LogFilterOptions carriers;
  carriers.attribute_key = "channel";
  const auto with_key = minera::filter_event_log(log, carriers);
  // A bare key keeps only the events that carry the attribute at all.
  assert((activity_names(with_key.traces.front()) ==
          std::vector<std::string>{"a", "b"}));
}

void filter_attribute_traces_mode_selects_cases() {
  auto log = attribute_filter_log();
  minera::Trace other;
  other.case_id = "2";
  other.events.push_back(
      minera::Event{"2", "a", "", "", 0, {{"channel", "phone", {}}}});
  log.traces.push_back(other);
  minera::Trace plain;
  plain.case_id = "3";
  plain.events.push_back(minera::Event{"3", "a", "", "", 0});
  log.traces.push_back(plain);

  minera::LogFilterOptions keep;
  keep.attribute_key = "channel";
  keep.keep_attribute_values = {"web"};
  keep.attribute_mode = minera::AttributeFilterMode::traces;
  const auto web_cases = minera::filter_event_log(log, keep);
  // Case 1 has a web event somewhere, so it survives whole.
  assert(web_cases.trace_count() == 1);
  assert(web_cases.traces.front().events.size() == 3);

  minera::LogFilterOptions remove;
  remove.attribute_key = "channel";
  remove.remove_attribute_values = {"phone"};
  remove.attribute_mode = minera::AttributeFilterMode::traces;
  const auto no_phone_cases = minera::filter_event_log(log, remove);
  // Both cases touching phone drop; the attribute-less case survives.
  assert(no_phone_cases.trace_count() == 1);
  assert(no_phone_cases.traces.front().case_id == "3");

  minera::LogFilterOptions carriers;
  carriers.attribute_key = "channel";
  carriers.attribute_mode = minera::AttributeFilterMode::traces;
  const auto with_key = minera::filter_event_log(log, carriers);
  // A bare key keeps the cases where the attribute appears at all.
  assert(with_key.trace_count() == 2);
}

void filter_attribute_values_require_a_key() {
  assert(throws([] {
    minera::LogFilterOptions options;
    options.keep_attribute_values = {"web"};
    assert(options.any());
    (void)minera::filter_event_log(attribute_filter_log(), options);
  }));
}

void json_export_carries_event_attributes() {
  const auto log = attribute_filter_log();
  const auto graph = minera::discover_directly_follows_graph(log);
  const auto json = minera::analysis_json("test", log, graph);

  // Per-event attributes ride along in the traces block.
  assert(json.find("\"attributes\": {\"channel\": \"web\"}") !=
         std::string::npos);
  // The summary lists the key with its event count and value frequencies.
  assert(json.find("\"event_attributes\": [") != std::string::npos);
  assert(json.find("{\"key\": \"channel\", \"events\": 2, "
                   "\"distinct_values\": 2, \"values\": [") !=
         std::string::npos);
  assert(json.find("{\"value\": \"phone\", \"count\": 1}") !=
         std::string::npos);

  // A log without generic attributes omits the field entirely.
  const auto plain = log_from_variants({{"a", "b"}});
  const auto plain_graph = minera::discover_directly_follows_graph(plain);
  const auto plain_json = minera::analysis_json("test", plain, plain_graph);
  assert(plain_json.find("event_attributes") == std::string::npos);
}

void importers_read_lifecycle_transitions() {
  // The CSV `lifecycle` column and the XES lifecycle:transition attribute
  // both land on the first-class lifecycle field, never on the generic
  // event attributes.
  const auto path = write_temp_log(
      "case_id,activity,timestamp,lifecycle,channel\n"
      "1,A,2026-01-01T00:00:00,start,web\n"
      "1,A,2026-01-01T00:00:30,complete,web\n");
  const auto csv_log = minera::import_csv_event_log(path);
  const auto& csv_events = csv_log.traces.front().events;
  assert(csv_events[0].lifecycle == "start");
  assert(csv_events[1].lifecycle == "complete");
  assert(csv_events[0].attribute("lifecycle") == nullptr);
  assert(*csv_events[0].attribute("channel") == "web");

  const auto xes_log = import_xes(
      "<log>\n"
      "  <trace>\n"
      "    <string key=\"concept:name\" value=\"c\"/>\n"
      "    <event>\n"
      "      <string key=\"concept:name\" value=\"A\"/>\n"
      "      <string key=\"lifecycle:transition\" value=\"start\"/>\n"
      "    </event>\n"
      "  </trace>\n"
      "</log>\n");
  const auto& xes_event = xes_log.traces.front().events.front();
  assert(xes_event.lifecycle == "start");
  assert(xes_event.attribute("lifecycle:transition") == nullptr);
}

void xes_export_round_trips_lifecycle() {
  // Lifecycle transitions survive an export/import cycle, and the export
  // declares the Lifecycle extension only when the log uses one.
  minera::EventLog log;
  minera::Trace trace;
  trace.case_id = "c1";
  trace.events.push_back(minera::Event{"c1", "a", "", "", 0, {}, "start"});
  trace.events.push_back(minera::Event{"c1", "a", "", "", 1, {}, "complete"});
  log.traces.push_back(trace);

  const auto xes = minera::to_xes(log);
  assert(xes.find("lifecycle.xesext") != std::string::npos);
  assert(xes.find("<string key=\"lifecycle:transition\" value=\"start\"/>") !=
         std::string::npos);

  std::istringstream input(xes);
  const auto reread = minera::import_xes_event_log(input);
  assert(reread.traces.front().events[0].lifecycle == "start");
  assert(reread.traces.front().events[1].lifecycle == "complete");

  // An atomic log declares no Lifecycle extension.
  const auto atomic = minera::to_xes(log_from_variants({{"a"}}));
  assert(atomic.find("lifecycle.xesext") == std::string::npos);
}

void filter_projects_by_lifecycle() {
  // One case: a start, a complete, and an atomic b without a lifecycle.
  minera::EventLog log;
  minera::Trace trace;
  trace.case_id = "1";
  trace.events.push_back(minera::Event{"1", "a", "", "", 0, {}, "start"});
  trace.events.push_back(minera::Event{"1", "a", "", "", 1, {}, "complete"});
  trace.events.push_back(minera::Event{"1", "b", "", "", 2});
  log.traces.push_back(trace);

  minera::LogFilterOptions keep;
  keep.keep_lifecycles = {"complete"};
  assert(keep.any());
  const auto completes = minera::filter_event_log(log, keep);
  // Only the complete survives; the lifecycle-less event is never kept by a
  // keep-set — this is the projection that makes a start/complete log atomic.
  assert(completes.traces.front().events.size() == 1);
  assert(completes.traces.front().events.front().lifecycle == "complete");

  minera::LogFilterOptions remove;
  remove.remove_lifecycles = {"start"};
  const auto no_starts = minera::filter_event_log(log, remove);
  // The start goes; the lifecycle-less event is never dropped by a remove-set.
  assert((activity_names(no_starts.traces.front()) ==
          std::vector<std::string>{"a", "b"}));
}

void csv_importer_maps_case_columns_to_case_attributes() {
  // A `case:`-prefixed column is a case attribute: stored once on the trace
  // under the unprefixed key (first value wins, blanks skipped), and never
  // doubled as an event attribute.
  const auto path = write_temp_log(
      "case_id,activity,timestamp,case:region,amount\n"
      "1,A,2026-01-01T00:00:00,,100\n"
      "1,B,2026-01-01T00:01:00,north,250\n"
      "1,C,2026-01-01T00:02:00,south,300\n"
      "2,A,2026-01-01T00:03:00,east,\n");

  const auto log = minera::import_csv_event_log(path);
  const auto& first = log.traces[0];
  // The blank first cell is skipped; the first non-empty value sticks and the
  // later conflicting one is ignored.
  assert(first.attributes.size() == 1);
  assert(first.attributes[0].key == "region");
  assert(*first.attribute("region") == "north");
  assert(first.attributes[0].kind == minera::AttributeKind::kString);
  assert(*log.traces[1].attribute("region") == "east");
  // The column feeds the trace, not the events; other columns still do.
  assert(first.events[0].attribute("region") == nullptr);
  assert(first.events[0].attribute("case:region") == nullptr);
  assert(*first.events[0].attribute("amount") == "100");
}

void xes_importer_captures_case_attributes() {
  // Scalar trace attributes beyond concept:name are case attributes, kept
  // with their XES type; nested sub-attributes are ignored and the case id
  // itself is never doubled.
  const auto log = import_xes(
      "<log>\n"
      "  <trace>\n"
      "    <string key=\"concept:name\" value=\"c\"/>\n"
      "    <int key=\"AMOUNT_REQ\" value=\"20000\"/>\n"
      "    <string key=\"customer\" value=\"acme\">\n"
      "      <string key=\"nested\" value=\"ignored\"/>\n"
      "    </string>\n"
      "    <event>\n"
      "      <string key=\"concept:name\" value=\"A\"/>\n"
      "    </event>\n"
      "  </trace>\n"
      "</log>\n");

  const auto& trace = log.traces.front();
  assert(trace.case_id == "c");
  assert(trace.attributes.size() == 2);
  assert(trace.attributes[0].key == "AMOUNT_REQ");
  assert(trace.attributes[0].value == "20000");
  assert(trace.attributes[0].kind == minera::AttributeKind::kInt);
  assert(*trace.attribute("customer") == "acme");
  assert(trace.attribute("nested") == nullptr);
  assert(trace.attribute("concept:name") == nullptr);
  // The trace-level attributes never leak onto the events.
  assert(trace.events.front().attributes.empty());
}

void xes_export_round_trips_case_attributes() {
  // Case attributes survive an export/import cycle with their XES types,
  // exactly like the generic event attributes.
  minera::EventLog log;
  minera::Trace trace;
  trace.case_id = "c1";
  trace.attributes.push_back(
      {"AMOUNT_REQ", "20000", minera::AttributeKind::kInt});
  trace.attributes.push_back(
      {"customer <&>", "acme \"x\"", minera::AttributeKind::kString});
  trace.events.push_back(minera::Event{"c1", "a", "", "", 0});
  log.traces.push_back(trace);

  const auto xes = minera::to_xes(log);
  assert(xes.find("<int key=\"AMOUNT_REQ\" value=\"20000\"/>") !=
         std::string::npos);

  std::istringstream input(xes);
  const auto reread = minera::import_xes_event_log(input);
  const auto& back = reread.traces.front();
  assert(back.attributes.size() == 2);
  assert(back.attributes[0].kind == minera::AttributeKind::kInt);
  assert(*back.attribute("AMOUNT_REQ") == "20000");
  assert(*back.attribute("customer <&>") == "acme \"x\"");
}

// Three one-event cases: region north on 1, south on 2, none on 3.
minera::EventLog case_attribute_filter_log() {
  minera::EventLog log;
  for (const auto& [id, region] :
       std::vector<std::pair<std::string, std::string>>{
           {"1", "north"}, {"2", "south"}, {"3", ""}}) {
    minera::Trace trace;
    trace.case_id = id;
    if (!region.empty()) {
      trace.attributes.push_back(
          {"region", region, minera::AttributeKind::kString});
    }
    trace.events.push_back(minera::Event{id, "a", "", "", 0});
    trace.events.push_back(minera::Event{id, "b", "", "", 1});
    log.traces.push_back(trace);
  }
  return log;
}

void filter_case_attribute_selects_cases() {
  const auto log = case_attribute_filter_log();

  minera::LogFilterOptions keep;
  keep.case_attribute_key = "region";
  keep.keep_case_attribute_values = {"north"};
  assert(keep.any());
  const auto north = minera::filter_event_log(log, keep);
  // Only the north case survives a keep-set; the attribute-less case drops.
  assert(north.trace_count() == 1);
  assert(north.traces.front().case_id == "1");
  // The surviving trace still carries its case attributes.
  assert(*north.traces.front().attribute("region") == "north");

  minera::LogFilterOptions remove;
  remove.case_attribute_key = "region";
  remove.remove_case_attribute_values = {"south"};
  const auto no_south = minera::filter_event_log(log, remove);
  // The south case goes; the attribute-less case is never dropped by a
  // remove-set.
  assert(no_south.trace_count() == 2);
  assert(no_south.traces[0].case_id == "1");
  assert(no_south.traces[1].case_id == "3");

  minera::LogFilterOptions carriers;
  carriers.case_attribute_key = "region";
  const auto with_key = minera::filter_event_log(log, carriers);
  // A bare key keeps only the cases that carry the attribute at all.
  assert(with_key.trace_count() == 2);

  // Case attributes ride through the other filters: an activity projection
  // rewrites the trace but keeps its attributes.
  minera::LogFilterOptions project;
  project.keep_activities = {"a"};
  const auto projected = minera::filter_event_log(log, project);
  assert(projected.trace_count() == 3);
  assert(*projected.traces.front().attribute("region") == "north");
  assert(projected.traces.front().events.size() == 1);
}

void filter_case_attribute_values_require_a_key() {
  assert(throws([] {
    minera::LogFilterOptions options;
    options.keep_case_attribute_values = {"north"};
    assert(options.any());
    (void)minera::filter_event_log(case_attribute_filter_log(), options);
  }));
}

void json_export_carries_case_attributes() {
  const auto log = case_attribute_filter_log();
  const auto graph = minera::discover_directly_follows_graph(log);
  const auto json = minera::analysis_json("test", log, graph);

  // Per-case attributes ride along in the traces block.
  assert(json.find("\"attributes\": {\"region\": \"north\"}") !=
         std::string::npos);
  // The summary lists the key with its case count and value frequencies.
  assert(json.find("\"case_attributes\": [") != std::string::npos);
  assert(json.find("{\"key\": \"region\", \"cases\": 2, "
                   "\"distinct_values\": 2, \"values\": [") !=
         std::string::npos);
  assert(json.find("{\"value\": \"north\", \"count\": 1}") !=
         std::string::npos);

  // A log without case attributes omits the field entirely.
  const auto plain = log_from_variants({{"a", "b"}});
  const auto plain_graph = minera::discover_directly_follows_graph(plain);
  const auto plain_json = minera::analysis_json("test", plain, plain_graph);
  assert(plain_json.find("case_attributes") == std::string::npos);
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

void heuristic_miner_and_threshold_controls_split_semantics() {
  // b and c are concurrent between a and d: they follow each other as often as
  // a splits to them, giving an AND measure of 12/(6+6+1) ≈ 0.923.
  // A threshold at 0.9 keeps the AND bundle; 0.95 breaks it into XOR branches.
  std::vector<std::vector<std::string>> variants(6, {"a", "b", "c", "d"});
  for (int i = 0; i < 6; ++i) variants.push_back({"a", "c", "b", "d"});
  const auto log = log_of(variants);

  minera::HeuristicMinerOptions and_opts;
  and_opts.and_threshold = 0.9;  // below 0.923 — should produce an AND bundle
  const auto and_net = minera::discover_heuristics_net(log, and_opts);
  const auto a_id_and = net_activity(and_net, "a");
  // Under AND semantics a fires into two parallel bundles.
  assert(and_net.output_bundles[a_id_and].size() == 1);
  assert(and_net.output_bundles[a_id_and][0].size() == 2);

  minera::HeuristicMinerOptions xor_opts;
  xor_opts.and_threshold = 0.95;  // above 0.923 — should produce separate XOR bundles
  const auto xor_net = minera::discover_heuristics_net(log, xor_opts);
  const auto a_id_xor = net_activity(xor_net, "a");
  assert(xor_net.output_bundles[a_id_xor].size() == 2);
  for (const auto& bundle : xor_net.output_bundles[a_id_xor]) {
    assert(bundle.size() == 1);
  }
}

void heuristic_miner_min_frequency_drops_rare_edges() {
  // A has one dominant successor B (10 traces) and one rare successor C (1
  // trace). Both have a positive dependency measure, but with min_frequency=2
  // the single A->C occurrence falls below the cutoff and is dropped.
  // connect_all still gives every activity its strongest edge, so C is
  // re-attached via a forced A->C (its only option), while B stays accepted.
  std::vector<std::vector<std::string>> traces(10, {"A", "B"});
  traces.push_back({"A", "C"});
  const auto log = log_of(traces);

  minera::HeuristicMinerOptions opts;
  opts.dependency_threshold = 0.0;
  opts.min_frequency = 2;
  const auto net = minera::discover_heuristics_net(log, opts);

  // A->B cleared the frequency cutoff via the threshold pass.
  assert(net_edge(net, "A", "B") != nullptr);
  assert(net_edge(net, "A", "B")->frequency == 10);

  // A->C was below min_frequency; connect_all forced it back for C's
  // connectivity. Either way C must have exactly one incoming edge.
  std::size_t c_in = 0;
  const auto c = net_activity(net, "C");
  for (const auto& e : net.edges) { if (e.target == c) ++c_in; }
  assert(c_in == 1);
}

void heuristic_miner_connect_all_false_may_leave_gaps() {
  // With only 5 traces the dependency A->B equals 5/6 ≈ 0.833, which falls
  // below the default 0.9 threshold, so A gets no accepted outgoing edge.
  // A is a start activity but never an end activity, so connect_all is
  // permitted to force an edge for it.
  const auto log = log_of(std::vector<std::vector<std::string>>(
      5, {"A", "B", "C"}));

  minera::HeuristicMinerOptions opts;
  opts.connect_all = false;  // nothing forced
  const auto net = minera::discover_heuristics_net(log, opts);

  const auto a = net_activity(net, "A");
  std::size_t a_out = 0;
  for (const auto& e : net.edges) { if (e.source == a) ++a_out; }
  assert(a_out == 0);

  // With connect_all enabled A must get at least its strongest outgoing edge.
  opts.connect_all = true;
  const auto connected = minera::discover_heuristics_net(log, opts);
  const auto a2 = net_activity(connected, "A");
  std::size_t a2_out = 0;
  for (const auto& e : connected.edges) { if (e.source == a2) ++a2_out; }
  assert(a2_out == 1);
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
  inductive_miner_infrequent_filters_an_edge_at_the_cutoff();
  inductive_miner_infrequent_terminates_on_a_filtered_exclusive_cut();
  inductive_miner_infrequent_keeps_structure_the_filter_would_destroy();
  inductive_miner_infrequent_drops_a_stray_edge_from_an_end_activity();
  inductive_miner_dfg_matches_log_recursion_on_structured_logs();
  inductive_miner_dfg_skips_empty_traces_at_the_root();
  inductive_miner_dfg_filters_noise_on_fallthrough();
  petri_net_translates_sequence();
  petri_net_adds_silent_split_and_join_for_parallel();
  alpha_miner_discovers_a_sequence();
  alpha_miner_shares_a_place_for_exclusive_choice();
  alpha_miner_keeps_parallel_branches_on_separate_places();
  alpha_miner_classic_drops_short_loop_activities();
  alpha_plus_attaches_a_length_one_loop();
  alpha_plus_discovers_a_length_two_loop();
  alpha_plus_matches_classic_without_short_loops();
  alpha_miner_handles_an_empty_log();
  json_export_carries_the_petri_net_contract();
  conformance_is_perfect_on_the_discovery_log();
  conformance_is_perfect_for_every_operator();
  conformance_counts_unmodeled_activities();
  conformance_penalizes_a_deviating_trace();
  precision_is_high_for_a_tight_model_and_low_for_a_flower();
  generalization_rewards_a_well_exercised_model();
  simplicity_reflects_routing_density();
  footprints_match_the_discovery_log();
  footprints_flag_a_forbidden_succession();
  footprints_detect_a_too_loose_model();
  pnml_round_trip_preserves_replay_behavior();
  parse_pnml_reads_a_foreign_net();
  parse_pnml_infers_missing_markings();
  parse_pnml_rejects_malformed_nets();
  xes_export_round_trips_the_log();
  xes_export_keeps_unparseable_timestamps_verbatim();
  graph_counts_rework_cases();
  eventually_follows_counts_every_later_occurrence();
  eventually_follows_handles_repeated_activities();
  alignment_precision_is_perfect_for_an_exact_model();
  alignment_precision_flags_unused_model_behavior();
  alignment_precision_survives_a_deviating_log();
  playout_generates_perfectly_fitting_logs();
  playout_simulates_an_imported_model();
  json_export_carries_the_eventually_follows_contract();
  json_export_carries_the_conformance_contract();
  conformance_handles_an_empty_log();
  alignment_is_perfect_for_every_operator();
  alignment_diagnoses_a_skipped_activity();
  alignment_diagnoses_an_inserted_activity();
  json_export_carries_the_alignment_contract();
  final_marking_reachable_distinguishes_sound_nets();
  alignment_scores_a_sound_alpha_net();
  alignment_flags_an_unsound_net();
  alignment_prunes_an_unbounded_net_with_dead_growth();
  alignment_flags_an_exhausted_search_on_an_unbounded_net();
  alignment_handles_an_empty_log();
  alignment_treats_an_empty_log_as_perfect_for_every_model();
  replay_degrades_gracefully_on_an_unbounded_silent_net();
  heuristics_net_becomes_a_replayable_workflow_net();
  heuristics_net_stays_replayable_when_it_drops_edges();
  heuristics_net_translates_an_and_split_and_join();
  heuristics_net_keeps_exclusive_branches_exclusive();
  heuristics_net_handles_a_self_loop();
  importer_handles_a_header_only_log();
  importer_strips_a_utf8_bom();
  dfg_handles_single_event_traces();
  importer_rejects_malformed_rows();
  filter_keeps_frequent_variants();
  filter_keeps_only_the_top_variant();
  filter_projects_activities();
  filter_applies_length_and_endpoint_constraints();
  filter_rework_keeps_cases_with_a_repeated_activity();
  filter_between_extracts_subtraces();
  filter_between_handles_equal_endpoints_and_rejects_one_sided();
  filter_projects_by_resource();
  filter_time_range_trims_events();
  filter_time_range_selects_whole_traces();
  filter_time_range_bounds_are_inclusive_and_one_sided();
  filter_time_range_rejects_a_malformed_bound();
  csv_importer_captures_extra_columns_as_attributes();
  xes_importer_captures_generic_event_attributes();
  xes_export_round_trips_generic_attributes();
  filter_attribute_events_mode_keeps_and_removes();
  filter_attribute_traces_mode_selects_cases();
  filter_attribute_values_require_a_key();
  json_export_carries_event_attributes();
  importers_read_lifecycle_transitions();
  xes_export_round_trips_lifecycle();
  filter_projects_by_lifecycle();
  csv_importer_maps_case_columns_to_case_attributes();
  xes_importer_captures_case_attributes();
  xes_export_round_trips_case_attributes();
  filter_case_attribute_selects_cases();
  filter_case_attribute_values_require_a_key();
  json_export_carries_case_attributes();
  xes_importer_reads_traces_and_events();
  xes_importer_reads_a_file_from_disk();
  xes_importer_sorts_events_by_timestamp();
  xes_importer_decodes_entities_and_ignores_nesting();
  xes_importer_rejects_malformed_logs();
  heuristic_miner_keeps_a_clean_sequence();
  heuristic_miner_filters_noisy_reverse_edges();
  heuristic_miner_detects_self_loops();
  heuristic_miner_handles_an_empty_log();
  heuristic_miner_and_threshold_controls_split_semantics();
  heuristic_miner_min_frequency_drops_rare_edges();
  heuristic_miner_connect_all_false_may_leave_gaps();
  return 0;
}
