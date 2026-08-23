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

  // Exponential signature growth is rejected before matrix allocation.
  std::vector<std::vector<std::string>> many(1);
  for (int i = 0; i < 9; ++i) many[0].push_back("activity_" + std::to_string(i));
  bool high_cardinality_rejected = false;
  try {
    (void)mine_ilp(log_of(many));
  } catch (const std::invalid_argument&) {
    high_cardinality_rejected = true;
  }
  assert(high_cardinality_rejected);

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
  assert(filtered_sequences.diagnostics.visited_nodes <=
         all_sequences.diagnostics.visited_nodes);
  assert(has_arc(all_sequences.net, 0, 2));
  assert(std::find(filtered_sequences.net.activities().begin(),
                   filtered_sequences.net.activities().end(), "c") !=
         filtered_sequences.net.activities().end());

  // Cumulative occurrence vectors compare continuations after the complete
  // multiset prefix: the rare d continuation shares the {a,b} prefix with a
  // frequent c continuation even though those events reached it in another
  // order.  The rejected suffix is not allowed to constrain the discovered
  // regions either.
  std::vector<std::vector<std::string>> cumulative_rows;
  for (int i = 0; i < 20; ++i) cumulative_rows.push_back({"a", "b", "c"});
  for (int i = 0; i < 20; ++i) cumulative_rows.push_back({"b", "a", "c"});
  cumulative_rows.push_back({"b", "a", "d", "e"});
  ILPMinerOptions cumulative_alpha;
  cumulative_alpha.alpha = 0.5;
  const auto cumulative_result = mine_ilp(log_of(cumulative_rows), cumulative_alpha);
  assert(!has_arc(cumulative_result.net, 0, 3));
  assert(!has_arc(cumulative_result.net, 3, 4));
  assert(check_workflow_net(cumulative_result.net).valid);

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

  // A reversed observed order must not select the smaller region that needs
  // an initial/final token; explicit causal places are required to be
  // zero-marking internal regions so the assembled workflow net retains them.
  ILPMinerOptions reversed_causal;
  reversed_causal.causal_relation = {{"a", "b"}};
  const auto reversed = mine_ilp(log_of({{"b", "a"}}), reversed_causal);
  assert(has_arc(reversed.net, 0, 1));
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
