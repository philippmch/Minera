#include "minera/petri_analysis.hpp"
#include "minera/petri_analysis_json.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

using namespace minera;

namespace {

PetriNet chain() {
  PetriNet net;
  net.set_activities({"A", "B"});
  const auto p0 = net.add_place(), p1 = net.add_place(), p2 = net.add_place();
  net.set_source(p0); net.set_sink(p2);
  net.add_transition(0, {p0}, {p1});
  net.add_transition(1, {p1}, {p2});
  return net;
}

PetriNet xor_net() {
  PetriNet net; net.set_activities({"A", "B"});
  const auto source = net.add_place(), sink = net.add_place();
  net.set_source(source); net.set_sink(sink);
  net.add_transition(0, {source}, {sink});
  net.add_transition(1, {source}, {sink});
  return net;
}

PetriNet parallel_net() {
  PetriNet net; net.set_activities({"A", "B"});
  const auto source = net.add_place(), a = net.add_place(), b = net.add_place();
  const auto c = net.add_place(), d = net.add_place(), sink = net.add_place();
  net.set_source(source); net.set_sink(sink);
  net.add_transition(std::nullopt, {source}, {a, b});
  net.add_transition(0, {a}, {c});
  net.add_transition(1, {b}, {d});
  net.add_transition(std::nullopt, {c, d}, {sink});
  return net;
}

PetriNet branching_pair_net() {
  PetriNet net;
  net.set_activities({"left", "right", "join"});
  const auto source = net.add_place();
  const auto left = net.add_place();
  const auto right = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.add_transition(0, {source}, {left});
  net.add_transition(1, {source}, {right});
  net.add_transition(2, {left, right}, {sink});
  return net;
}

PetriNet splitting_pair_net() {
  PetriNet net;
  net.set_activities({"left", "right"});
  const auto source = net.add_place();
  const auto left = net.add_place();
  const auto right = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.add_transition(std::nullopt, {source}, {left, right});
  net.add_transition(0, {left}, {sink});
  net.add_transition(1, {right}, {sink});
  return net;
}

void classic_equation() {
  const auto net = chain();
  auto result = solve_marking_equation(net);
  assert(result.computed && result.feasible && !result.exhausted);
  assert(std::abs(result.objective) < 1e-5);
  assert(result.transition_counts.size() == 2);
  assert(std::abs(result.transition_counts[0] - 1) < 1e-7);
  assert(result.firing_sequence == std::vector<std::size_t>({0, 1}));
  assert(result.reaches_final && result.activated_transitions.size() == 2);
  assert(std::all_of(result.residual.begin(), result.residual.end(), [](double x) { return std::abs(x) < 1e-7; }));

  MarkingEquationOptions changed;
  changed.initial_marking = {0, 1, 0}; changed.final_marking = {0, 0, 1};
  changed.transition_costs = {5, 7};
  result = solve_marking_equation(net, changed);
  assert(result.feasible && std::abs(result.objective - 7) < 1e-7);
  assert(result.transition_counts[0] < 1e-7 && std::abs(result.transition_counts[1] - 1) < 1e-7);

  PetriNet impossible; impossible.set_activities({"A"});
  const auto p = impossible.add_place(), q = impossible.add_place();
  impossible.set_source(p); impossible.set_sink(q);
  impossible.add_transition(0, {p}, {p});
  result = solve_marking_equation(impossible);
  assert(result.computed && !result.feasible && !result.error.empty());

  bool threw = false;
  try {
    MarkingEquationOptions too_large;
    too_large.initial_marking = {std::numeric_limits<std::int64_t>::max(), 0, 0};
    (void)solve_marking_equation(net, too_large);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void incidence_feasible_not_realizable() {
  PetriNet net; net.set_activities({"A"});
  const auto p0 = net.add_place(), p1 = net.add_place();
  const auto p2 = net.add_place(), p3 = net.add_place();
  net.set_source(p0); net.set_sink(p3);
  net.add_transition(0, {p0, p2}, {p1});
  net.add_transition(std::nullopt, {p1}, {p2, p3});
  const auto result = solve_marking_equation(net);
  assert(result.feasible && result.transition_counts == std::vector<double>({1, 1}));
  assert(result.recovery_attempted && !result.reaches_final && result.firing_sequence.empty());
}

void extended_equation() {
  const auto net = chain();
  auto matching = solve_extended_marking_equation({"A", "B"}, net);
  assert(matching.computed && matching.feasible && matching.reaches_final);
  assert(std::abs(matching.objective) < 1e-8);
  assert(matching.firing_sequence == std::vector<std::size_t>({0, 1}));
  assert(matching.segment_count == 2 &&
         matching.retained_splits == std::vector<std::size_t>({1}));

  const auto deviation = solve_extended_marking_equation({"A", "X", "B"}, net);
  assert(deviation.feasible && std::abs(deviation.objective - 10000.0) < 1e-7);

  const auto reversed = solve_extended_marking_equation({"B", "A"}, net);
  assert(reversed.feasible && std::abs(reversed.objective - 20000.0) < 1e-7);

  const auto long_trace = solve_extended_marking_equation(
      {"A", "B", "C", "D", "E", "F", "G"}, net);
  assert(long_trace.retained_splits ==
         std::vector<std::size_t>({1, 2, 3, 4, 5, 6}));
  ExtendedMarkingEquationOptions capped;
  capped.max_k_value = 2;
  const auto capped_trace = solve_extended_marking_equation(
      {"A", "B", "C", "D", "E", "F", "G"}, net, capped);
  assert(capped_trace.retained_splits ==
         std::vector<std::size_t>({1, 2, 5, 6}));
  ExtendedMarkingEquationOptions boundary_only;
  boundary_only.max_k_value = 0;
  const auto boundary_trace = solve_extended_marking_equation(
      {"A", "B", "C", "D", "E", "F", "G"}, net, boundary_only);
  assert(boundary_trace.retained_splits ==
         std::vector<std::size_t>({1, 6}));

  ExtendedMarkingEquationOptions no_splits;
  no_splits.max_k_value = 0;
  const auto empty = solve_extended_marking_equation({}, net, no_splits);
  assert(empty.feasible && empty.segment_count == 1 &&
         std::abs(empty.objective - 20000.0) < 1e-7);

  ExtendedMarkingEquationOptions explicit_splits;
  explicit_splits.split_indices = {1};
  const auto split = solve_extended_marking_equation({"A", "B"}, net,
                                                      explicit_splits);
  assert(split.retained_splits == std::vector<std::size_t>({1}));
  bool threw = false;
  try {
    explicit_splits.split_indices = {0};
    (void)solve_extended_marking_equation({"A", "B"}, net, explicit_splits);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void workflow_structure() {
  auto net = chain();
  const auto before_places = net.place_count();
  const auto before_inputs = net.transitions()[0].inputs;
  const auto result = check_workflow_net(net);
  assert(result.valid && result.source_candidates == std::vector<std::size_t>({0}));
  assert(is_workflow_net(net));
  assert(net.place_count() == before_places && net.transitions()[0].inputs == before_inputs);

  PetriNet empty;
  assert(!check_workflow_net(empty).valid);
  PetriNet isolated; isolated.add_place(); isolated.add_place(); isolated.set_source(0); isolated.set_sink(1);
  auto bad = check_workflow_net(isolated);
  assert(!bad.valid && bad.source_candidates.size() == 2 && bad.sink_candidates.size() == 2);

  PetriNet disconnected; disconnected.set_activities({"A", "B"});
  const auto s = disconnected.add_place(), k = disconnected.add_place();
  const auto x = disconnected.add_place(), y = disconnected.add_place();
  disconnected.set_source(s); disconnected.set_sink(k);
  disconnected.add_transition(0, {s}, {k}); disconnected.add_transition(1, {x}, {y});
  bad = check_workflow_net(disconnected);
  assert(!bad.valid && !bad.offending_places.empty());

  PetriNet invalid; invalid.set_activities({"A"}); invalid.add_place(); invalid.set_source(0); invalid.set_sink(0);
  invalid.add_transition(0, {0}, {9});
  assert(!check_workflow_net(invalid).valid);
}

void sound_models() {
  for (const auto& net : {chain(), xor_net(), parallel_net()}) {
    const auto result = analyze_soundness(net, {.diagnostics = true});
    assert(result.status == SoundnessStatus::SOUND && result.computed && result.bounded);
    assert(result.final_reachable && result.option_to_complete && result.proper_completion);
    assert(result.dead_transitions.empty() && result.deadlocks.empty());
    assert(!result.place_invariant_basis.empty());
  }
}

void positive_invariant_diagnostics() {
  const auto sequential = analyze_soundness(chain(), {.diagnostics = true});
  assert(sequential.s_components.has_value());
  assert(sequential.s_components->size() == 1);
  assert(sequential.s_components->front() == std::vector<std::size_t>({0, 1, 2}));
  assert(sequential.uncovered_places_s_component.empty());
  assert(sequential.uncovered_places.empty());
  assert(!sequential.uniform_place_invariants.empty());
  assert(!sequential.weighted_place_invariants.empty());
  assert(sequential.uncovered_places_uniform.empty());
  assert(sequential.uncovered_places_weighted.empty());
  assert(sequential.reachability_markings.size() == 3);
  assert((sequential.reachability_edges ==
          std::vector<std::vector<std::size_t>>{{0, 1, 0}, {1, 2, 1}}));
  assert(!sequential.coverability_markings.empty());
  assert(!sequential.coverability_edges.empty());
  for (const auto& invariant : sequential.place_invariant_basis) {
    assert(std::all_of(invariant.begin(), invariant.end(),
                       [](double value) { return value >= -1e-8; }));
    assert(std::abs(invariant[0] - invariant[1]) < 1e-7);
    assert(std::abs(invariant[1] - invariant[2]) < 1e-7);
  }
  const auto serialized = petri_analysis_json(sequential);
  assert(serialized.find("\"s_components\"") != std::string::npos);
  assert(serialized.find("\"handled_pairs\"") != std::string::npos);
  assert(serialized.find("\"not_well_handled_pairs\"") != std::string::npos);
  assert(serialized.find("\"uniform_place_invariants\"") != std::string::npos);
  assert(serialized.find("\"weighted_place_invariants\"") != std::string::npos);
  assert(serialized.find("\"reachability_edges\"") != std::string::npos);
  assert(sequential.handled_pairs.has_value());
  assert(sequential.handled_pairs->empty());
  assert(sequential.completion_witness == std::vector<std::size_t>({0, 1}));
  assert(sequential.structural_node_labels.size() == 10);
  assert(sequential.structural_node_labels[0] == "place:0:in");
  assert(sequential.structural_node_labels[6] == "transition:0:A:in");

  const auto parallel = analyze_soundness(parallel_net());
  assert(parallel.s_components.has_value());
  assert(parallel.uncovered_places.empty());
  for (const auto& invariant : parallel.place_invariant_basis) {
    assert(std::all_of(invariant.begin(), invariant.end(),
                       [](double value) { return value >= -1e-8; }));
    for (const auto& transition : parallel_net().transitions()) {
      double consumed = 0.0;
      double produced = 0.0;
      for (const auto p : transition.inputs) consumed += invariant[p];
      for (const auto p : transition.outputs) produced += invariant[p];
      assert(std::abs(consumed - produced) < 1e-7);
    }
  }
}

void structural_pair_diagnostics() {
  const auto result = analyze_soundness(branching_pair_net(), {.diagnostics = true});
  assert(result.workflow.valid);
  assert(result.handled_pairs.has_value());
  const auto& pairs = *result.handled_pairs;
  assert(std::find(pairs.begin(), pairs.end(),
                   std::vector<std::size_t>({1, 12})) != pairs.end());
  assert(std::any_of(result.diagnostic_messages.begin(),
                     result.diagnostic_messages.end(), [](const auto& message) {
                       return message.find("structural pair diagnostics") !=
                              std::string::npos;
                     }));
  const auto serialized = petri_analysis_json(result);
  assert(serialized.find("\"not_well_handled_pairs\":[[") !=
         std::string::npos);
  assert(serialized.find("\"structural_node_labels\":[\"place:0:in\"") !=
         std::string::npos);

  SoundnessOptions woflan_options;
  woflan_options.variant = SoundnessVariant::kWoflan;
  const auto woflan_result = analyze_soundness(branching_pair_net(),
                                                woflan_options);
  assert(woflan_result.variant == SoundnessVariant::kWoflan);
  const auto woflan_json = petri_analysis_json(woflan_result);
  assert(woflan_json.find("\"family\":\"woflan\"") !=
         std::string::npos);
  assert(woflan_json.find("\"variant\":\"woflan\"") !=
         std::string::npos);

  const auto split = analyze_soundness(splitting_pair_net());
  assert(split.handled_pairs.has_value());
  assert(std::find(split.handled_pairs->begin(), split.handled_pairs->end(),
                   std::vector<std::size_t>({9, 6})) !=
         split.handled_pairs->end());
}

void woflan_short_circuit_diagnostics() {
  SoundnessOptions options;
  options.variant = SoundnessVariant::kWoflan;
  options.diagnostics = true;
  const auto result = analyze_soundness(chain(), options);
  assert(result.short_circuit_valid);
  assert(result.short_circuit_strongly_connected);
  assert(result.short_circuit_transition.has_value() &&
         *result.short_circuit_transition == 2);
  assert(!result.short_circuit_diagnostic_messages.empty());
  assert(!result.short_circuit_reachability_markings.empty());
  assert(std::find(result.short_circuit_reachability_edges.begin(),
                   result.short_circuit_reachability_edges.end(),
                   std::vector<std::size_t>({2, 0, 2})) !=
         result.short_circuit_reachability_edges.end());
  assert(std::find(result.coverability_edges.begin(),
                   result.coverability_edges.end(),
                   std::vector<std::size_t>({2, 0, 2})) !=
         result.coverability_edges.end());
  assert(result.short_circuit_reachability_markings ==
         result.reachability_markings);
  assert(result.short_circuit_reachability_strongly_connected);
  const auto json = petri_analysis_json(result);
  assert(json.find("\"short_circuit_valid\":true") != std::string::npos);
  assert(json.find("\"short_circuit_transition\":2") !=
         std::string::npos);
  assert(json.find("\"short_circuit_reachability_edges\"") !=
         std::string::npos);
  assert(json.find("\"short_circuit_reachability_strongly_connected\":true") !=
         std::string::npos);

  auto stale_boundaries = chain();
  stale_boundaries.set_source(1);
  stale_boundaries.set_sink(1);
  const auto stale_result = analyze_soundness(stale_boundaries, options);
  assert(stale_result.status == SoundnessStatus::SOUND);
  assert(stale_result.completion_witness == std::vector<std::size_t>({0, 1}));

  SoundnessOptions malformed_marking = options;
  malformed_marking.initial_marking = {1, 0, 1};
  bool threw = false;
  try {
    (void)analyze_soundness(chain(), malformed_marking);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  PetriNet invalid;
  invalid.set_activities({"A"});
  invalid.add_place();
  invalid.add_place();
  invalid.set_source(0);
  invalid.set_sink(1);
  const auto invalid_result = analyze_soundness(invalid, options);
  assert(!invalid_result.short_circuit_valid);
  assert(!invalid_result.short_circuit_diagnostic_messages.empty());
  assert(invalid_result.status == SoundnessStatus::UNSOUND);
}

void unsound_models() {
  PetriNet improper; improper.set_activities({"A"});
  const auto s = improper.add_place(), extra = improper.add_place(), sink = improper.add_place();
  improper.set_source(s); improper.set_sink(sink);
  improper.add_transition(0, {s}, {sink, extra});
  improper.add_transition(std::nullopt, {extra}, {sink});
  auto result = analyze_soundness(improper);
  assert(result.status == SoundnessStatus::UNSOUND && !result.proper_completion && !result.improper_markings.empty());
  assert(!result.improper_witnesses.empty());
  assert(result.improper_witnesses.front() == std::vector<std::size_t>({0}));

  PetriNet dead; dead.set_activities({"A", "B"});
  const auto ds = dead.add_place(), dk = dead.add_place(); dead.set_source(ds); dead.set_sink(dk);
  dead.add_transition(0, {ds}, {dk}); dead.add_transition(1, {ds, ds}, {dk});
  result = analyze_soundness(dead, {.diagnostics = true});
  assert(result.status == SoundnessStatus::UNSOUND && result.dead_transitions == std::vector<std::size_t>({1}));

  PetriNet locking; locking.set_activities({"A", "B"});
  const auto ls = locking.add_place(), lp = locking.add_place(), lk = locking.add_place();
  locking.set_source(ls); locking.set_sink(lk);
  locking.add_transition(0, {ls}, {lp}); locking.add_transition(1, {lp, lp}, {lk});
  result = analyze_soundness(locking);
  assert(result.status == SoundnessStatus::UNSOUND && !result.deadlocks.empty());
  assert(result.locking_witness == std::vector<std::size_t>({0}));
  assert(result.dead_tasks == std::vector<std::size_t>({1}));
  assert(result.locking_scenarios ==
         std::vector<std::vector<std::size_t>>{{0}});
  assert(result.deadlock_witnesses ==
         std::vector<std::vector<std::size_t>>{{0}});
  const auto locking_json = petri_analysis_json(result);
  assert(locking_json.find("\"dead_tasks\":[1]") != std::string::npos);
  assert(locking_json.find("\"locking_scenarios\":[[0]]") !=
         std::string::npos);
  assert(locking_json.find("\"deadlock_witnesses\":[[0]]") !=
         std::string::npos);

  SoundnessOptions early;
  early.variant = SoundnessVariant::kWoflan;
  early.diagnostics = true;
  early.return_asap_when_unsound = true;
  const auto early_locking = analyze_soundness(locking, early);
  assert(early_locking.returned_asap && early_locking.computed &&
         early_locking.status == SoundnessStatus::UNSOUND);
  assert(early_locking.deadlock_witnesses ==
         std::vector<std::vector<std::size_t>>{{0}});
  assert(early_locking.reachability_markings.size() == 2);
  const auto early_json = petri_analysis_json(early_locking);
  assert(early_json.find("\"returned_asap\":true") !=
         std::string::npos);
  const auto early_sound = analyze_soundness(chain(), early);
  assert(!early_sound.returned_asap &&
         early_sound.status == SoundnessStatus::SOUND);

  PetriNet growing; growing.set_activities({"start", "grow", "finish"});
  const auto gs = growing.add_place(), gp = growing.add_place(), gk = growing.add_place();
  growing.set_source(gs); growing.set_sink(gk);
  growing.add_transition(0, {gs}, {gp});
  growing.add_transition(1, {gp}, {gp, gp});
  growing.add_transition(2, {gp}, {gk});
  result = analyze_soundness(growing, {.diagnostics = true});
  assert(result.status == SoundnessStatus::UNSOUND && !result.bounded);
  assert(result.unbounded_witness == std::vector<std::size_t>({0, 1}));
  assert(result.coverability_node_count >= 2);
  assert(!result.coverability_markings.empty());
  assert(std::any_of(result.coverability_markings.begin(),
                     result.coverability_markings.end(), [](const auto& marking) {
                       return std::any_of(marking.begin(), marking.end(),
                                          [](auto value) { return value == -1; });
                     }));
  assert(!result.coverability_edges.empty());
  assert(std::any_of(result.diagnostic_messages.begin(),
                     result.diagnostic_messages.end(), [](const auto& message) {
                       return message.find("coverability") != std::string::npos;
                     }));

  // A smaller reachable marking from one branch must not subsume the larger
  // branch that contains the unbounded growth.
  PetriNet cross_branch;
  cross_branch.set_activities({"small", "large", "grow", "finish"});
  const auto cb_source = cross_branch.add_place();
  const auto cb_mid = cross_branch.add_place();
  const auto cb_growth = cross_branch.add_place();
  const auto cb_sink = cross_branch.add_place();
  cross_branch.set_source(cb_source);
  cross_branch.set_sink(cb_sink);
  cross_branch.add_transition(0, {cb_source}, {cb_mid});
  cross_branch.add_transition(1, {cb_source}, {cb_mid, cb_growth});
  cross_branch.add_transition(2, {cb_mid, cb_growth},
                               {cb_mid, cb_growth, cb_growth});
  cross_branch.add_transition(3, {cb_growth}, {cb_sink});
  result = analyze_soundness(cross_branch, {.diagnostics = true});
  assert(result.status == SoundnessStatus::UNSOUND && !result.bounded);
  assert(result.unbounded_witness == std::vector<std::size_t>({1, 2}));
}

void caps_markings_determinism_and_weights() {
  bool threw = false;
  try { SoundnessOptions bad; bad.initial_marking = {-1, 0, 0}; (void)analyze_soundness(chain(), bad); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  auto limited = analyze_soundness(chain(), {.max_states = 1});
  assert(limited.status == SoundnessStatus::UNKNOWN && limited.exhausted && !limited.computed);

  const auto first = analyze_soundness(parallel_net());
  const auto second = analyze_soundness(parallel_net());
  assert(first.status == second.status && first.reachable_marking_count == second.reachable_marking_count);
  assert(first.dead_transitions == second.dead_transitions && first.place_invariant_basis == second.place_invariant_basis);

  PetriNet weighted; weighted.set_activities({"A"});
  const auto source = weighted.add_place(), sink = weighted.add_place(); weighted.set_source(source); weighted.set_sink(sink);
  weighted.add_transition(0, {source}, {sink});
  SoundnessOptions options; options.initial_marking = {2, 0}; options.final_marking = {0, 2};
  const auto sound = analyze_soundness(weighted, options);
  assert(sound.status == SoundnessStatus::SOUND && sound.reachable_marking_count == 3);
  assert(sound.improper_markings.empty());

  PetriNet malformed;
  malformed.set_activities({"A"});
  const auto mp = malformed.add_place();
  malformed.set_source(mp);
  malformed.set_sink(mp);
  malformed.add_transition(0, {mp}, {mp + 1});
  const auto malformed_result = analyze_soundness(malformed);
  assert(malformed_result.status == SoundnessStatus::UNSOUND);
}

}  // namespace

int main() {
  classic_equation();
  incidence_feasible_not_realizable();
  extended_equation();
  workflow_structure();
  sound_models();
  positive_invariant_diagnostics();
  structural_pair_diagnostics();
  woflan_short_circuit_diagnostics();
  unsound_models();
  caps_markings_determinism_and_weights();
}
