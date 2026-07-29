#include "minera/petri_analysis.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <stdexcept>

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

void classic_equation() {
  const auto net = chain();
  auto result = solve_marking_equation(net);
  assert(result.computed && result.feasible && !result.exhausted);
  assert(std::abs(result.objective - 20000.0) < 1e-5);
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

void unsound_models() {
  PetriNet improper; improper.set_activities({"A"});
  const auto s = improper.add_place(), extra = improper.add_place(), sink = improper.add_place();
  improper.set_source(s); improper.set_sink(sink);
  improper.add_transition(0, {s}, {sink, extra});
  improper.add_transition(std::nullopt, {extra}, {sink});
  auto result = analyze_soundness(improper);
  assert(result.status == SoundnessStatus::UNSOUND && !result.proper_completion && !result.improper_markings.empty());

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

  PetriNet growing; growing.set_activities({"start", "grow", "finish"});
  const auto gs = growing.add_place(), gp = growing.add_place(), gk = growing.add_place();
  growing.set_source(gs); growing.set_sink(gk);
  growing.add_transition(0, {gs}, {gp});
  growing.add_transition(1, {gp}, {gp, gp});
  growing.add_transition(2, {gp}, {gk});
  result = analyze_soundness(growing);
  assert(result.status == SoundnessStatus::UNSOUND && !result.bounded);
  assert(result.unbounded_witness == std::vector<std::size_t>({0, 1}));
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
}

}  // namespace

int main() {
  classic_equation();
  incidence_feasible_not_realizable();
  workflow_structure();
  sound_models();
  unsound_models();
  caps_markings_determinism_and_weights();
}
