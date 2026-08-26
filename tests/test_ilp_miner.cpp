#include "minera/ilp_miner.hpp"
#include "minera/conformance.hpp"
#include "minera/petri_analysis.hpp"

#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>

using namespace minera;

namespace {
EventLog log_of(const std::vector<std::vector<std::string>>& rows) {
  EventLog log;
  for (std::size_t i = 0; i < rows.size(); ++i) {
    Trace trace;
    trace.case_id = std::to_string(i);
    for (std::size_t j = 0; j < rows[i].size(); ++j)
      trace.events.push_back(Event{trace.case_id, rows[i][j], {}, {}, j, {}, {}});
    log.traces.push_back(std::move(trace));
  }
  return log;
}

bool has_arc(const PetriNet& net, std::size_t from, std::size_t to) {
  for (const auto& transition : net.transitions()) {
    if (!transition.activity || *transition.activity != from) continue;
    for (const auto place : transition.outputs)
      if (place >= 2 && std::find(transition.inputs.begin(), transition.inputs.end(), place) ==
                            transition.inputs.end()) {
        for (const auto& target : net.transitions()) {
          if (!target.activity || *target.activity != to) continue;
          if (std::find(target.inputs.begin(), target.inputs.end(), place) !=
              target.inputs.end())
            return true;
        }
      }
  }
  return false;
}

bool has_self_loop(const PetriNet& net, std::size_t activity) {
  for (const auto& transition : net.transitions()) {
    if (!transition.activity || *transition.activity != activity) continue;
    for (const auto place : transition.inputs)
      if (std::find(transition.outputs.begin(), transition.outputs.end(), place) !=
          transition.outputs.end())
        return true;
  }
  return false;
}

std::size_t visible_transition_count(const PetriNet& net) {
  return static_cast<std::size_t>(std::count_if(
      net.transitions().begin(), net.transitions().end(),
      [](const PetriTransition& transition) { return transition.activity.has_value(); }));
}
}  // namespace

int main() {
  const auto sequence = mine_ilp(log_of({{"a", "b", "c"}, {"a", "b", "c"}}));
  assert(sequence.diagnostics.computed);
  assert(sequence.markings.initial.size() == sequence.net.place_count());
  assert(sequence.net.activities() == std::vector<std::string>({"a", "b", "c"}));
  assert(has_arc(sequence.net, 0, 1) && has_arc(sequence.net, 1, 2));
  assert(std::count_if(sequence.net.transitions().begin(), sequence.net.transitions().end(),
                       [](const PetriTransition& transition) { return transition.silent(); }) >=
         2);
  assert(check_workflow_net(sequence.net).valid);

  const auto choice = mine_ilp(log_of({{"a", "b"}, {"a", "c"}}));
  assert(choice.net.place_count() >= 3);
  assert(check_workflow_net(choice.net).valid);

  const auto parallel = mine_ilp(log_of({{"a", "b", "c", "d"},
                                         {"a", "c", "b", "d"}}));
  assert(parallel.net.activities().size() == 4);
  assert(has_arc(parallel.net, 0, 1) && has_arc(parallel.net, 0, 2));
  assert(check_workflow_net(parallel.net).valid);

  // Region constraints must retain repeated activity and self-loop behavior.
  const auto repeated_log = log_of({{"a", "a", "b"}, {"a", "b"}});
  const auto repeated = mine_ilp(repeated_log);
  assert(check_workflow_net(repeated.net).valid);
  const auto repeated_replay = check_conformance(repeated_log, repeated.net);
  assert(repeated_replay.fitting_traces == repeated_replay.trace_count);
  const auto loop_log = log_of({{"a", "a"}, {"a"}});
  const auto loop = mine_ilp(loop_log);
  assert(loop.net.place_count() >= 3);
  assert(check_workflow_net(loop.net).valid);
  const auto loop_replay = check_conformance(loop_log, loop.net);
  assert(loop_replay.fitting_traces == loop_replay.trace_count);

  // Candidate ordering and diagnostics are stable across runs.
  const auto repeat_again = mine_ilp(repeated_log);
  assert(repeated.net.activities() == repeat_again.net.activities());
  assert(repeated.net.place_count() == repeat_again.net.place_count());
  assert(repeated.diagnostics.visited_nodes == repeat_again.diagnostics.visited_nodes);

  ILPMinerOptions bounded;
  bounded.max_nodes = 1;
  const auto limited = mine_ilp(log_of({{"a", "b", "c"}}), bounded);
  assert(limited.diagnostics.exhausted);
  assert(limited.diagnostics.visited_nodes <= 1);
  assert(limited.net.place_count() >= 2);

  bool rejected = false;
  try {
    ILPMinerOptions invalid;
    invalid.noise_threshold = 2.0;
    (void)mine_ilp(log_of({{"a"}}), invalid);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);

  // Larger alphabets use lazy bounded region generation instead of an
  // arbitrary fixed-width rejection.  The no-boundary form keeps the
  // candidate count focused on the nine visible activities.
  std::vector<std::vector<std::string>> many(1);
  for (int i = 0; i < 9; ++i) many[0].push_back("activity_" + std::to_string(i));
  ILPMinerOptions high_cardinality;
  high_cardinality.artificial_start_activity.clear();
  high_cardinality.artificial_end_activity.clear();
  const auto nine = mine_ilp(log_of(many), high_cardinality);
  assert(nine.diagnostics.computed);
  assert(nine.net.activities().size() == 9);
  assert(check_workflow_net(nine.net).valid);
  const auto nine_again = mine_ilp(log_of(many), high_cardinality);
  assert(nine.net.place_count() == nine_again.net.place_count());
  assert(nine.diagnostics.visited_nodes == nine_again.diagnostics.visited_nodes);

  std::vector<std::vector<std::string>> twelve(1);
  for (int i = 0; i < 12; ++i) twelve[0].push_back("activity_" + std::to_string(i));
  ILPMinerOptions bounded_high = high_cardinality;
  bounded_high.max_nodes = 8;
  const auto twelve_result = mine_ilp(log_of(twelve), bounded_high);
  assert(twelve_result.diagnostics.exhausted);
  assert(twelve_result.diagnostics.visited_nodes <= bounded_high.max_nodes);
  assert(twelve_result.net.activities().size() == 12);

  // Pair seeding preserves late causal relations even when refinement of a
  // larger alphabet reaches the shared candidate budget.
  ILPMinerOptions late_pairs = high_cardinality;
  late_pairs.max_nodes = 64;
  const auto late_result = mine_ilp(log_of(twelve), late_pairs);
  const auto late_id = [&](const std::string& label) {
    return static_cast<std::size_t>(std::distance(
        late_result.net.activities().begin(),
        std::find(late_result.net.activities().begin(),
                  late_result.net.activities().end(), label)));
  };
  for (int i = 0; i < 11; ++i) {
    assert(has_arc(late_result.net, late_id("activity_" + std::to_string(i)),
                   late_id("activity_" + std::to_string(i + 1))));
  }
  assert(late_result.diagnostics.exhausted);

  // The dynamic signature crosses a machine-word boundary without changing
  // activity identity or the requested causal arc.
  std::vector<std::vector<std::string>> wide_rows(1);
  for (int i = 0; i < 65; ++i) {
    const auto index = i < 10 ? "0" + std::to_string(i) : std::to_string(i);
    wide_rows[0].push_back("wide_" + index);
  }
  ILPMinerOptions wide_options = high_cardinality;
  wide_options.causal_relation = {{"wide_63", "wide_64"}};
  wide_options.max_nodes = 16;
  const auto wide_result = mine_ilp(log_of(wide_rows), wide_options);
  assert(wide_result.diagnostics.computed);
  assert(wide_result.net.activities().size() == 65);
  const auto wide_from = static_cast<std::size_t>(std::distance(
      wide_result.net.activities().begin(),
      std::find(wide_result.net.activities().begin(),
                wide_result.net.activities().end(), "wide_63")));
  const auto wide_to = static_cast<std::size_t>(std::distance(
      wide_result.net.activities().begin(),
      std::find(wide_result.net.activities().begin(),
                wide_result.net.activities().end(), "wide_64")));
  assert(wide_from < wide_result.net.activities().size());
  assert(wide_to < wide_result.net.activities().size());
  assert(has_arc(wide_result.net, wide_from, wide_to));
  assert(wide_result.diagnostics.visited_nodes <= wide_options.max_nodes);

  // Sparse large alphabets still respect the candidate budget; branch-and-
  // bound may prove the remaining exponential suffix without exhausting it.
  std::vector<std::vector<std::string>> sparse_wide_rows;
  sparse_wide_rows.push_back({"sparse_00", "sparse_01"});
  for (int i = 2; i < 24; ++i)
    sparse_wide_rows.push_back({"sparse_" + std::to_string(i)});
  ILPMinerOptions sparse_wide_options = high_cardinality;
  sparse_wide_options.max_nodes = 26;
  const auto sparse_wide = mine_ilp(log_of(sparse_wide_rows), sparse_wide_options);
  assert(sparse_wide.diagnostics.visited_nodes <= sparse_wide_options.max_nodes);
  assert(sparse_wide.diagnostics.computed);
  assert(sparse_wide.net.activities().size() == 24);

  // Artificial boundaries are represented as silent transitions and are not
  // leaked into the visible activity alphabet.
  ILPMinerOptions boundaries;
  boundaries.artificial_start_activity = "BEGIN";
  boundaries.artificial_end_activity = "FINISH";
  const auto boundary_result = mine_ilp(log_of({{"a", "b"}, {"a", "b"}}), boundaries);
  assert(boundary_result.net.activities() == std::vector<std::string>({"a", "b"}));
  assert(visible_transition_count(boundary_result.net) == 2);
  assert(std::any_of(boundary_result.net.transitions().begin(), boundary_result.net.transitions().end(),
                     [](const PetriTransition& transition) { return transition.silent(); }));
  assert(check_workflow_net(boundary_result.net).valid);

  // Duplicate variants are collapsed for the bounded search, but their
  // frequencies still drive noise filtering: the singleton branch disappears
  // when it is below half of the dominant variant's support.
  ILPMinerOptions noise;
  noise.noise_threshold = 0.5;
  const auto denoised = mine_ilp(log_of({{"a", "b"}, {"a", "b"}, {"a", "b"},
                                         {"a", "c"}}), noise);
  assert(std::find(denoised.net.activities().begin(), denoised.net.activities().end(),
                   "c") == denoised.net.activities().end());

  // Alpha=1 keeps every continuation; alpha=0.5 cuts a continuation whose
  // support is less than half of its predecessor's strongest continuation.
  ILPMinerOptions alpha_full;
  const auto all_sequences = mine_ilp(log_of({{"a", "b"}, {"a", "b"}, {"a", "b"},
                                               {"a", "c"}}), alpha_full);
  ILPMinerOptions alpha_filtered;
  alpha_filtered.alpha = 0.5;
  const auto filtered_sequences = mine_ilp(
      log_of({{"a", "b"}, {"a", "b"}, {"a", "b"}, {"a", "c"}}), alpha_filtered);
  assert(all_sequences.diagnostics.visited_nodes <= alpha_full.max_nodes);
  assert(filtered_sequences.diagnostics.visited_nodes <= alpha_filtered.max_nodes);
  assert(has_arc(all_sequences.net, 0, 2));
  assert(std::find(filtered_sequences.net.activities().begin(),
                   filtered_sequences.net.activities().end(), "c") !=
         filtered_sequences.net.activities().end());

  // Multiplicity changes support filtering but not the optimization rows: a
  // duplicated variant has the same selected region while alpha keeps all
  // variants.
  ILPMinerOptions variant_rows;
  variant_rows.artificial_start_activity.clear();
  variant_rows.artificial_end_activity.clear();
  variant_rows.causal_relation = {{"a", "b"}};
  const auto one_each = mine_ilp(log_of({{"a", "b"}, {"a", "c"}}), variant_rows);
  const auto one_duplicated = mine_ilp(
      log_of({{"a", "b"}, {"a", "b"}, {"a", "b"}, {"a", "c"}}),
      variant_rows);
  assert(one_each.net.place_count() == one_duplicated.net.place_count());
  assert(one_each.net.transitions().size() == one_duplicated.net.transitions().size());
  assert(one_each.diagnostics.visited_nodes == one_duplicated.diagnostics.visited_nodes);

  // Cumulative occurrence vectors compare continuations after the complete
  // multiset prefix: the rare d continuation shares the {a,b} prefix with a
  // frequent c continuation even though those events reached it in another
  // order.  Causal pairs are derived before alpha filtering, so their
  // composite places remain representable even when the rare suffix rows are
  // omitted from the region constraints.
  std::vector<std::vector<std::string>> cumulative_rows;
  for (int i = 0; i < 20; ++i) cumulative_rows.push_back({"a", "b", "c"});
  for (int i = 0; i < 20; ++i) cumulative_rows.push_back({"b", "a", "c"});
  cumulative_rows.push_back({"b", "a", "d", "e"});
  ILPMinerOptions cumulative_alpha;
  cumulative_alpha.alpha = 0.5;
  const auto cumulative_result = mine_ilp(log_of(cumulative_rows), cumulative_alpha);
  assert(has_arc(cumulative_result.net, 0, 3));
  assert(has_arc(cumulative_result.net, 3, 4));
  assert(check_workflow_net(cumulative_result.net).valid);

  // Final-row feasibility is a pre-fire equality, not a common post-fire
  // marking requirement.  The simple a=>b region remains valid even though
  // the two complete traces finish with different activities.
  ILPMinerOptions final_row_options;
  final_row_options.artificial_start_activity.clear();
  final_row_options.artificial_end_activity.clear();
  final_row_options.causal_relation = {{"a", "b"}};
  const auto final_row_result = mine_ilp(
      log_of({{"a", "b", "a"}, {"a", "b", "c"}}), final_row_options);
  assert(has_arc(final_row_result.net, 0, 1));
  assert(!has_arc(final_row_result.net, 2, 1));
  assert(check_workflow_net(final_row_result.net).valid);

  // Repeated final events remain feasible under the same cumulative-row
  // constraints and retain a self-loop on the repeated activity.
  ILPMinerOptions repeated_final_options;
  repeated_final_options.artificial_start_activity.clear();
  repeated_final_options.artificial_end_activity.clear();
  repeated_final_options.causal_relation = {{"a", "b"}};
  const auto repeated_final = mine_ilp(
      log_of({{"a", "c", "b", "b"}}), repeated_final_options);
  assert(has_arc(repeated_final.net, 0, 1));
  assert(has_self_loop(repeated_final.net, 1));
  assert(check_workflow_net(repeated_final.net).valid);

  // A supplied causal relation is deterministic and receives priority under
  // a tight node budget, so the requested directed region is discoverable.
  ILPMinerOptions causal;
  causal.causal_relation = {{"a", "c"}};
  causal.max_nodes = 1;
  const auto causal_result = mine_ilp(log_of({{"a", "c"}}), causal);
  assert(causal_result.diagnostics.visited_nodes == 1);
  assert(causal_result.diagnostics.lp_solved == 1);
  assert(has_arc(causal_result.net, 0, 1));

  // Explicit causal pairs constrain a region without making the pair list
  // exhaustive.  The only place that fits both variants must add c as an
  // additional consumer of the token produced by a.
  ILPMinerOptions sparse_causal;
  sparse_causal.causal_relation = {{"a", "b"}};
  const auto sparse = mine_ilp(log_of({{"a", "b"}, {"a", "c"}}),
                               sparse_causal);
  assert(sparse.net.activities() == std::vector<std::string>({"a", "b", "c"}));
  assert(has_arc(sparse.net, 0, 1));
  assert(has_arc(sparse.net, 0, 2));
  assert(check_workflow_net(sparse.net).valid);
  const auto sparse_replay = check_conformance(
      log_of({{"a", "b"}, {"a", "c"}}), sparse.net);
  assert(sparse_replay.fitting_traces == sparse_replay.trace_count);
  const auto sparse_repeat = mine_ilp(
      log_of({{"a", "b"}, {"a", "c"}}), sparse_causal);
  assert(sparse.net.place_count() == sparse_repeat.net.place_count());
  assert(sparse.net.transitions().size() == sparse_repeat.net.transitions().size());
  assert(sparse.diagnostics.visited_nodes == sparse_repeat.diagnostics.visited_nodes);
  sparse_causal.max_nodes = 1;
  const auto sparse_limited = mine_ilp(
      log_of({{"a", "b"}, {"a", "c"}}), sparse_causal);
  assert(sparse_limited.diagnostics.exhausted);
  assert(sparse_limited.diagnostics.visited_nodes <= 1);

  // A reversed observed order cannot satisfy a zero-initial-token place for
  // the requested direction: b is consumed before a can produce anything.
  // The workflow fallback remains valid, but it must not fabricate a=>b.
  ILPMinerOptions reversed_causal;
  reversed_causal.artificial_start_activity.clear();
  reversed_causal.artificial_end_activity.clear();
  reversed_causal.causal_relation = {{"a", "b"}};
  const auto reversed = mine_ilp(log_of({{"b", "a"}}), reversed_causal);
  assert(!has_arc(reversed.net, 0, 1));
  assert(check_workflow_net(reversed.net).valid);

  // Overlapping requested pairs still receive independent best regions.  The
  // two variants make a=>b and a=>d share a feasible composite, while a=>b
  // also has a smaller singleton optimum; the composite's c=>b arc must not
  // be lost when the pair results are merged.
  ILPMinerOptions overlapping_causal;
  overlapping_causal.artificial_start_activity.clear();
  overlapping_causal.artificial_end_activity.clear();
  overlapping_causal.causal_relation = {{"a", "b"}, {"a", "d"}};
  const auto overlapping = mine_ilp(
      log_of({{"a", "b"}, {"c", "d"}}), overlapping_causal);
  assert(overlapping.net.activities() ==
         std::vector<std::string>({"a", "b", "c", "d"}));
  assert(has_arc(overlapping.net, 2, 1));
  assert(has_arc(overlapping.net, 0, 3));
  assert(check_workflow_net(overlapping.net).valid);
  const auto overlapping_replay = check_conformance(
      log_of({{"a", "b"}, {"c", "d"}}), overlapping.net);
  assert(overlapping_replay.fitting_traces == overlapping_replay.trace_count);
  assert(!overlapping.diagnostics.exhausted);

  bool invalid_alpha = false;
  try {
    ILPMinerOptions invalid;
    invalid.alpha = 1.1;
    (void)mine_ilp(log_of({{"a"}}), invalid);
  } catch (const std::invalid_argument&) {
    invalid_alpha = true;
  }
  assert(invalid_alpha);
  return 0;
}
