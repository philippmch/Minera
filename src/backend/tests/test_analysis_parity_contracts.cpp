#include "minera/petri_analysis.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

using namespace minera;

namespace {

PetriNet chain_net() {
  PetriNet net;
  net.set_activities({"A", "B"});
  const auto source = net.add_place();
  const auto middle = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.add_transition(0, {source}, {middle});
  net.add_transition(1, {middle}, {sink});
  return net;
}

PetriNet branching_net() {
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

PetriNet weighted_chain_net() {
  PetriNet net;
  net.set_activities({"A", "B"});
  const auto source = net.add_place();
  const auto middle = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.add_transition(0, {source}, {middle, middle});
  net.add_transition(1, {middle, middle}, {sink});
  return net;
}

PetriNet growing_net() {
  PetriNet net;
  net.set_activities({"start", "grow", "finish"});
  const auto source = net.add_place();
  const auto middle = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.add_transition(0, {source}, {middle});
  net.add_transition(1, {middle}, {middle, middle});
  net.add_transition(2, {middle}, {sink});
  return net;
}

SoundnessOptions woflan_options() {
  SoundnessOptions options;
  options.variant = SoundnessVariant::kWoflan;
  options.diagnostics = true;
  options.max_states = 64;
  options.invariant_search_limit = 10000;
  return options;
}

template <typename T>
bool contains(const std::vector<T>& values, const T& value) {
  return std::find(values.begin(), values.end(), value) != values.end();
}

void classic_marking_equation_contract() {
  const auto result = solve_marking_equation(chain_net());
  assert(result.variant == MarkingEquationVariant::CLASSIC);
  assert(result.computed && result.feasible && !result.exhausted);
  assert(result.transition_counts == std::vector<double>({1.0, 1.0}));
  assert(result.activated_transitions == std::vector<std::size_t>({0, 1}));
  assert(result.firing_sequence == std::vector<std::size_t>({0, 1}));
  assert(result.reaches_final);
  assert(std::all_of(result.residual.begin(), result.residual.end(),
                     [](const double value) {
                       return std::abs(value) < 1e-7;
                     }));
}

void classic_extended_marking_equation_contract() {
  ExtendedMarkingEquationOptions options;
  options.max_k_value = 1;
  const auto result = solve_extended_marking_equation(
      {"A", "B"}, chain_net(), options);
  assert(result.variant == ExtendedMarkingEquationVariant::CLASSIC);
  assert(result.computed && result.feasible && !result.exhausted);
  assert(result.retained_splits == std::vector<std::size_t>({1}));
  assert(result.segment_count == 2);
  assert(result.transition_counts == std::vector<double>({1.0, 1.0}));
  assert(result.firing_sequence == std::vector<std::size_t>({0, 1}));
  assert(result.reaches_final);
  assert(std::abs(result.objective) < 1e-7);
}

void workflow_net_contract() {
  const auto valid = check_workflow_net(chain_net());
  assert(valid.variant == WorkflowNetVariant::PETRI_NET);
  assert(valid.valid);
  assert(valid.source_candidates == std::vector<std::size_t>({0}));
  assert(valid.sink_candidates == std::vector<std::size_t>({2}));
  assert(valid.unreachable_places.empty());
  assert(valid.unreachable_transitions.empty());

  PetriNet disconnected;
  disconnected.set_activities({"A", "B"});
  const auto source = disconnected.add_place();
  const auto sink = disconnected.add_place();
  const auto isolated_source = disconnected.add_place();
  const auto isolated_sink = disconnected.add_place();
  disconnected.set_source(source);
  disconnected.set_sink(sink);
  disconnected.add_transition(0, {source}, {sink});
  disconnected.add_transition(1, {isolated_source}, {isolated_sink});
  const auto invalid = check_workflow_net(disconnected);
  assert(!invalid.valid);
  assert(!invalid.reason.empty());
  assert(invalid.source_candidates.size() == 2);
  assert(invalid.sink_candidates.size() == 2);
  assert(contains(invalid.offending_places, isolated_source));
  assert(contains(invalid.offending_places, isolated_sink));
}

void woflan_pipeline_contract() {
  const auto result = analyze_soundness(chain_net(), woflan_options());
  assert(result.variant == SoundnessVariant::kWoflan);
  assert(result.workflow.valid);
  assert(result.computed && result.bounded);
  assert(result.status == SoundnessStatus::SOUND);
  assert(result.final_reachable && result.option_to_complete);
  assert(result.proper_completion && result.dead_tasks.empty());
  assert(result.short_circuit_valid);
  assert(result.short_circuit_strongly_connected);
  assert(result.short_circuit_transition.has_value());
  assert(*result.short_circuit_transition == 2);
  assert(result.left.has_value() && *result.left);
}

void woflan_coverability_markings_contract() {
  const auto result = analyze_soundness(growing_net(), woflan_options());
  assert(!result.coverability_markings.empty());
  assert(result.coverability_markings.front() == Marking({1, 0, 0}));
  assert(!result.bounded);
  assert(result.unbounded_witness == std::vector<std::size_t>({0, 1}));
  assert(std::any_of(result.coverability_markings.begin(),
                     result.coverability_markings.end(),
                     [](const Marking& marking) {
                       return contains(marking, std::int64_t{-1});
                     }));
  assert(std::any_of(result.coverability_edges.begin(),
                     result.coverability_edges.end(), [](const auto& edge) {
                       return edge.size() == 3 && edge[2] == 1;
                     }));
}

void woflan_reachability_markings_contract() {
  const auto result = analyze_soundness(chain_net(), woflan_options());
  const std::vector<Marking> expected = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
  assert(result.reachability_markings == expected);
  assert((result.reachability_edges ==
          std::vector<std::vector<std::size_t>>{{0, 1, 0}, {1, 2, 1}}));
}

void woflan_restricted_coverability_markings_contract() {
  const auto result = analyze_soundness(chain_net(), woflan_options());
  const std::vector<Marking> expected = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
  assert(!result.restricted_coverability_tree_exhausted);
  assert(result.restricted_coverability_markings == expected);
  assert((result.restricted_coverability_edges ==
          std::vector<std::vector<std::size_t>>{{0, 1, 0}, {1, 2, 1}}));
}

void woflan_coverability_edges_contract() {
  const auto result = analyze_soundness(chain_net(), woflan_options());
  assert(!result.coverability_edges.empty());
  assert(std::all_of(result.coverability_edges.begin(),
                     result.coverability_edges.end(),
                     [](const auto& edge) { return edge.size() == 3; }));
  assert(contains(result.coverability_edges,
                  std::vector<std::size_t>({0, 1, 0})));
  assert(contains(result.coverability_edges,
                  std::vector<std::size_t>({1, 2, 1})));
  assert(contains(result.coverability_edges,
                  std::vector<std::size_t>({2, 0, 2})));
}

void woflan_not_well_handled_pairs_contract() {
  const auto result = analyze_soundness(branching_net(), woflan_options());
  assert(result.workflow.valid);
  assert(result.not_well_handled_pairs.has_value());
  assert(result.handled_pairs == result.not_well_handled_pairs);
  assert(contains(*result.not_well_handled_pairs,
                  std::vector<std::size_t>({1, 12})));
}

void woflan_place_invariant_basis_contract() {
  const auto result = analyze_soundness(chain_net(), woflan_options());
  assert(!result.place_invariant_basis.empty());
  const bool has_positive_balanced_basis = std::any_of(
      result.place_invariant_basis.begin(), result.place_invariant_basis.end(),
      [](const auto& invariant) {
        return invariant.size() == 3 &&
               std::all_of(invariant.begin(), invariant.end(),
                           [](const double value) { return value > 0.0; }) &&
               std::abs(invariant[0] - invariant[1]) < 1e-7 &&
               std::abs(invariant[1] - invariant[2]) < 1e-7;
      });
  assert(has_positive_balanced_basis);
  assert(result.uncovered_places.empty());
}

void woflan_s_components_contract() {
  const auto result = analyze_soundness(chain_net(), woflan_options());
  assert(result.s_components.has_value());
  assert(contains(*result.s_components,
                  std::vector<std::size_t>({0, 1, 2})));
  assert(result.uncovered_places_s_component.empty());
}

void woflan_uniform_place_invariants_contract() {
  const auto result = analyze_soundness(chain_net(), woflan_options());
  assert(contains(result.uniform_place_invariants,
                  std::vector<double>({1.0, 1.0, 1.0})));
  assert(result.uncovered_places_uniform.empty());
}

void woflan_weighted_place_invariants_contract() {
  const auto result = analyze_soundness(weighted_chain_net(), woflan_options());
  assert(!result.weighted_place_invariants.empty());
  const bool has_expected_weights = std::any_of(
      result.weighted_place_invariants.begin(),
      result.weighted_place_invariants.end(), [](const auto& invariant) {
        return invariant.size() == 3 && invariant[0] > 0.0 &&
               invariant[1] > 0.0 && invariant[2] > 0.0 &&
               std::abs(invariant[0] - 2.0 * invariant[1]) < 1e-7 &&
               std::abs(invariant[2] - 2.0 * invariant[1]) < 1e-7;
      });
  assert(has_expected_weights);
}

}  // namespace

int main() {
  classic_marking_equation_contract();
  classic_extended_marking_equation_contract();
  workflow_net_contract();
  woflan_pipeline_contract();
  woflan_coverability_markings_contract();
  woflan_reachability_markings_contract();
  woflan_restricted_coverability_markings_contract();
  woflan_coverability_edges_contract();
  woflan_not_well_handled_pairs_contract();
  woflan_place_invariant_basis_contract();
  woflan_s_components_contract();
  woflan_uniform_place_invariants_contract();
  woflan_weighted_place_invariants_contract();
}
