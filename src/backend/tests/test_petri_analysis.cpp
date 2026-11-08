#include "minera/petri_analysis.hpp"
#include "minera/petri_analysis_json.hpp"
#include "minera/event_log.hpp"
#include "minera/petri_trace_alignment.hpp"
#include "minera/cancellation.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

using namespace minera;

namespace {

// A net whose transitions list compact sides: one occurrence per PNML arc
// object with its own weight (parallel arcs, zero or signed weights), as
// parse_pnml_document stores them. A side without weights keeps repeated
// unit endpoints. Place 0 is the source, place_count - 1 the sink.
struct CompactArcs { std::vector<std::size_t> places; std::vector<std::int64_t> weights; };
struct CompactTransition { std::optional<std::size_t> activity; CompactArcs inputs, outputs; };
PetriNet compact_net(std::size_t places, std::vector<std::string> activities,
                     const std::vector<CompactTransition>& transitions) {
  PetriNetLiteralStorage storage;
  storage.place_count = places;
  storage.source_storage = 0;
  storage.sink_storage = places - 1;
  storage.has_source = storage.has_sink = true;
  storage.activities = std::move(activities);
  const auto weights = [](const std::vector<std::int64_t>& values) {
    std::vector<PetriArcWeight> out;
    for (const auto value : values)
      out.push_back(PetriArcWeight{static_cast<std::uint64_t>(value < 0 ? -value : value), value < 0});
    return out;
  };
  for (const auto& spec : transitions) {
    PetriTransition transition;
    transition.activity = spec.activity;
    transition.inputs = spec.inputs.places;
    transition.outputs = spec.outputs.places;
    transition.input_weights = weights(spec.inputs.weights);
    transition.output_weights = weights(spec.outputs.weights);
    storage.transitions.push_back(std::move(transition));
  }
  return PetriNet::from_literal_storage(std::move(storage));
}

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
  assert(result.solver_nodes > 0);
  assert(std::abs(result.objective) < 1e-5);
  assert(result.transition_counts.size() == 2);
  assert(std::abs(result.transition_counts[0] - 1) < 1e-7);
  assert(result.firing_sequence == std::vector<std::size_t>({0, 1}));
  assert(result.reaches_final && result.activated_transitions.size() == 2);
  assert(std::all_of(result.residual.begin(), result.residual.end(), [](double x) { return std::abs(x) < 1e-7; }));
  const auto json = petri_analysis_json(result);
  assert(json.find("\"solver_nodes\":") != std::string::npos);

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

void classic_equation_requires_integer_solutions() {
  PetriNet net;
  net.set_activities({"double", "single"});
  const auto source = net.add_place(), sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  (void)source; (void)sink;
  // "double" holds two parallel unit arcs on each side (arc objects count
  // once each, so its column is -2/+2 and the relaxation picks it half).
  net = compact_net(2, {"double", "single"},
      {{0, {{0, 0}, {1, 1}}, {{1, 1}, {1, 1}}}, {1, {{0}, {}}, {{1}, {}}}});

  MarkingEquationOptions options;
  options.initial_marking = {1, 0};
  options.final_marking = {0, 1};
  options.transition_costs = {1, 3};
  const auto result = solve_marking_equation(net, options);
  assert(result.computed && result.feasible && !result.exhausted);
  assert(std::abs(result.objective - 3.0) < 1e-7);
  assert(std::abs(result.lower_bound - 0.5) < 1e-7);
  assert(std::abs(result.transition_counts[0]) < 1e-7);
  assert(std::abs(result.transition_counts[1] - 1.0) < 1e-7);
  assert(result.recovery_attempted && result.reaches_final);
  assert(result.firing_sequence == std::vector<std::size_t>({1}));
  assert(result.activated_transitions == std::vector<std::size_t>({1}));
  assert(result.solver_nodes >= 2);
  assert(std::all_of(result.residual.begin(), result.residual.end(),
                     [](double value) { return std::abs(value) < 1e-7; }));
}

void extended_equation() {
  const auto net = chain();
  auto matching = solve_extended_marking_equation({"A", "B"}, net);
  assert(matching.computed && matching.feasible && matching.reaches_final);
  assert(matching.solver_nodes > 0);
  assert(std::abs(matching.objective) < 1e-8);
  assert(matching.firing_sequence == std::vector<std::size_t>({0, 1}));
  assert(matching.segment_count == 2 &&
         matching.retained_splits == std::vector<std::size_t>({1}));
  assert(petri_analysis_json(matching).find("\"solver_nodes\":") !=
         std::string::npos);

  auto malformed = matching;
  malformed.segment_count = 3;
  bool malformed_rejected = false;
  try {
    (void)petri_analysis_json(malformed);
  } catch (const std::invalid_argument&) {
    malformed_rejected = true;
  }
  assert(malformed_rejected);

  // Every trace length serializes: the solver uses max(2, split points)
  // blocks, and given split points keep their order and repeats.
  const auto serializes = [](const ExtendedMarkingEquationResult& result) {
    const auto json = petri_analysis_json(result);
    return json.find("\"segment_count\":" +
                     std::to_string(std::max<std::size_t>(
                         2, result.retained_splits.size()))) != std::string::npos;
  };
  for (const auto& trace : std::vector<std::vector<std::string>>{
           {}, {"A"}, {"A", "B", "C"}, {"B", "A", "B", "A", "B"}}) {
    const auto result = solve_extended_marking_equation(trace, net);
    assert(result.computed && serializes(result));
  }
  for (const auto& splits : std::vector<std::vector<std::size_t>>{
           {1, 1}, {2, 1}, {0}, {2, 0, 2}}) {
    ExtendedMarkingEquationOptions given;
    given.split_indices = splits;
    const auto result =
        solve_extended_marking_equation({"A", "B", "A"}, net, given);
    assert(result.retained_splits == splits && serializes(result));
  }
  auto missing_block = matching;
  missing_block.retained_splits = {1, 1, 1};
  missing_block.segment_count = 2;
  malformed_rejected = false;
  try {
    (void)petri_analysis_json(missing_block);
  } catch (const std::invalid_argument&) {
    malformed_rejected = true;
  }
  assert(malformed_rejected);

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

  // Sampling is deterministic and keeps the two boundaries while spreading
  // the requested points across a longer trace.
  const std::vector<std::string> sampled_trace = {
      "A", "B", "C", "D", "E", "F", "G", "H", "I", "J"};
  const auto sampled = solve_extended_marking_equation(sampled_trace, net);
  assert(sampled.retained_splits ==
         std::vector<std::size_t>({1, 2, 4, 6, 8, 9}));
  const auto sampled_again = solve_extended_marking_equation(sampled_trace, net);
  assert(sampled_again.retained_splits == sampled.retained_splits);
  ExtendedMarkingEquationOptions explicit_many;
  explicit_many.split_indices = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  explicit_many.max_k_value = 5;
  const auto explicit_sampled = solve_extended_marking_equation(
      sampled_trace, net, explicit_many);
  assert(explicit_sampled.retained_splits == sampled.retained_splits);

  ExtendedMarkingEquationOptions boundary_only;
  boundary_only.max_k_value = 0;
  const auto boundary_trace = solve_extended_marking_equation(
      {"A", "B", "C", "D", "E", "F", "G"}, net, boundary_only);
  assert(boundary_trace.retained_splits ==
         std::vector<std::size_t>({1, 6}));

  ExtendedMarkingEquationOptions no_splits;
  no_splits.max_k_value = 0;
  const auto empty = solve_extended_marking_equation({}, net, no_splits);
  // The reference always builds at least two segments, so even an empty
  // trace must fire one move as y_1.
  assert(empty.feasible && empty.segment_count == 2 &&
         std::abs(empty.objective - 20000.0) < 1e-7);

  ExtendedMarkingEquationOptions explicit_splits;
  explicit_splits.split_indices = {1};
  const auto split = solve_extended_marking_equation({"A", "B"}, net,
                                                      explicit_splits);
  assert(split.retained_splits == std::vector<std::size_t>({1}));
  // Like the reference, given split points keep their order and
  // repetitions and may name the first event; only positions past the
  // trace are rejected.
  explicit_splits.split_indices = {1, 0, 1};
  const auto repeated = solve_extended_marking_equation({"A", "B"}, net,
                                                         explicit_splits);
  assert(repeated.retained_splits == std::vector<std::size_t>({1, 0, 1}));
  // y_1 is a B move, so x_0 spends the A event that y_2 would need.
  assert(repeated.segment_count == 3 && repeated.computed && !repeated.feasible);
  explicit_splits.split_indices = {0, 1};
  const auto from_start = solve_extended_marking_equation({"A", "B"}, net,
                                                           explicit_splits);
  assert(from_start.feasible && std::abs(from_start.objective) < 1e-9);
  bool threw = false;
  try {
    explicit_splits.split_indices = {2};
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
  assert(petri_analysis_json(result).find("\"variant\":\"petri_net\"") !=
         std::string::npos);

  auto malformed = result;
  malformed.source_candidates.push_back(malformed.source_candidates.front());
  bool malformed_rejected = false;
  try {
    (void)petri_analysis_json(malformed);
  } catch (const std::invalid_argument&) {
    malformed_rejected = true;
  }
  assert(malformed_rejected);

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
    assert(petri_analysis_json(result).find("\"status\":\"sound\"") !=
           std::string::npos);
  }

  auto malformed = analyze_soundness(chain());
  malformed.dead_tasks.push_back(0);
  bool malformed_rejected = false;
  try {
    (void)petri_analysis_json(malformed);
  } catch (const std::invalid_argument&) {
    malformed_rejected = true;
  }
  assert(malformed_rejected);
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
  assert(sequential.not_well_handled_pairs == sequential.handled_pairs);
  assert(sequential.completion_witness == std::vector<std::size_t>({0, 1}));
  assert(sequential.structural_node_labels.size() == 10);
  assert(sequential.structural_node_labels[0] == "place:0:in");
  assert(sequential.structural_node_labels[6] == "transition:0:A:in");

  const auto parallel = analyze_soundness(parallel_net());
  assert(parallel.s_components.has_value());
  assert(parallel.uncovered_places.empty());
  const auto parallel_model = parallel_net();
  for (const auto& invariant : parallel.place_invariant_basis) {
    assert(std::all_of(invariant.begin(), invariant.end(),
                       [](double value) { return value >= -1e-8; }));
    for (const auto& transition : parallel_model.transitions()) {
      double consumed = 0.0;
      double produced = 0.0;
      for (const auto p : transition.inputs) consumed += invariant[p];
      for (const auto p : transition.outputs) produced += invariant[p];
      assert(std::abs(consumed - produced) < 1e-7);
    }
  }
}

void weighted_invariant_contract() {
  PetriNet net;
  net.set_activities({"A"});
  const auto source = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.add_transition(0, {source}, {sink, sink});

  const auto result = analyze_soundness(net, {.diagnostics = true});
  assert(result.workflow.valid);
  assert(result.place_invariant_basis.size() == 1);
  assert(result.uniform_place_invariants.empty());
  assert(result.uncovered_places_uniform == std::vector<std::size_t>({0, 1}));
  assert(result.s_components.has_value() && result.s_components->empty());
  assert(result.uncovered_places_s_component ==
         std::vector<std::size_t>({0, 1}));
  assert(result.weighted_place_invariants.size() == 1);
  const auto& weighted = result.weighted_place_invariants.front();
  assert(weighted.size() == 2);
  assert(std::abs(weighted[0] - 2.0 * weighted[1]) < 1e-7);
  double balance = 0.0;
  for (const auto place : net.transitions().front().inputs)
    balance -= weighted[place];
  for (const auto place : net.transitions().front().outputs)
    balance += weighted[place];
  assert(std::abs(balance) < 1e-7);

  SoundnessOptions woflan;
  woflan.variant = SoundnessVariant::kWoflan;
  const auto woflan_result = analyze_soundness(net, woflan);
  assert(woflan_result.s_components.has_value() &&
         woflan_result.s_components->empty());
  assert(!woflan_result.left.has_value() || !*woflan_result.left);
  assert(!woflan_result.coverability_markings.empty());
  assert(!woflan_result.short_circuit_reachability_markings.empty());

  // Structural S-component failure is an early Woflan witness.  The
  // return-asap form must not spend the remaining budget on behavioral
  // projections after this decision is already conclusive.
  woflan.diagnostics = true;
  woflan.return_asap_when_unsound = true;
  const auto early = analyze_soundness(net, woflan);
  assert(early.status == SoundnessStatus::UNSOUND && early.computed &&
         early.returned_asap);
  assert(!early.left.has_value());
  assert(early.structural_node_labels.empty());
  assert(!early.handled_pairs.has_value());
  assert(early.reachability_markings.empty());
  assert(early.coverability_markings.empty());
  assert(early.restricted_coverability_markings.empty());
  assert(std::any_of(early.diagnostic_messages.begin(),
                     early.diagnostic_messages.end(), [](const auto& message) {
                       return message.find("S-component coverage") !=
                              std::string::npos;
                     }));

  // A multi-step counterbalanced net makes the distinction observable: its
  // behavioral projections contain a complete path, but the structural
  // uniform-invariant test still has no certificate.  The early option must
  // stop at that structural witness without constructing later artifacts.
  PetriNet scaled_chain;
  scaled_chain.set_activities({"A", "B"});
  const auto scaled_source = scaled_chain.add_place();
  const auto scaled_middle = scaled_chain.add_place();
  const auto scaled_sink = scaled_chain.add_place();
  scaled_chain.set_source(scaled_source);
  scaled_chain.set_sink(scaled_sink);
  scaled_chain.add_transition(0, {scaled_source},
                              {scaled_middle, scaled_middle});
  scaled_chain.add_transition(1, {scaled_middle, scaled_middle},
                              {scaled_sink});
  SoundnessOptions default_woflan = woflan;
  default_woflan.return_asap_when_unsound = false;
  const auto scaled_default = analyze_soundness(scaled_chain, default_woflan);
  assert(!scaled_default.returned_asap);
  assert(!scaled_default.reachability_markings.empty());
  assert(!scaled_default.coverability_markings.empty());
  const auto scaled_early = analyze_soundness(scaled_chain, woflan);
  assert(scaled_early.status == SoundnessStatus::UNSOUND &&
         scaled_early.returned_asap && scaled_early.computed);
  assert(scaled_early.s_components.has_value() &&
         scaled_early.s_components->empty());
  assert(!scaled_early.uncovered_places_s_component.empty());
  assert(scaled_early.structural_node_labels.empty());
  assert(!scaled_early.handled_pairs.has_value());
  assert(scaled_early.reachability_markings.empty());
  assert(scaled_early.coverability_markings.empty());
  assert(scaled_early.restricted_coverability_markings.empty());
}

void uniform_invariant_combination() {
  // The positive cone has two weighted rays, while their shared support is a
  // valid unit-weight invariant.  A support-only pass over either ray misses
  // the uniform certificate.
  PetriNet net;
  net.set_activities({"A"});
  const auto first = net.add_place();
  const auto middle = net.add_place();
  const auto last = net.add_place();
  net.set_source(middle);
  net.set_sink(last);
  net.add_transition(0, {middle, middle}, {first, last});

  const auto result = analyze_soundness(net, {.diagnostics = true});
  assert(result.place_invariant_basis.size() >= 2);
  const std::vector<double> expected{1.0, 1.0, 1.0};
  assert(std::find(result.uniform_place_invariants.begin(),
                   result.uniform_place_invariants.end(), expected) !=
         result.uniform_place_invariants.end());
  double balance = 0.0;
  for (const auto place : net.transitions().front().inputs) balance -= expected[place];
  for (const auto place : net.transitions().front().outputs) balance += expected[place];
  assert(std::abs(balance) < 1e-7);
}

void s_component_uses_distinct_places() {
  PetriNet net;
  net.set_activities({"A"});
  const auto source = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.add_transition(0, {source, source}, {sink, sink});

  const auto result = analyze_soundness(net, {.diagnostics = true});
  const std::vector<std::vector<double>> expected_uniform{{1.0, 1.0}};
  assert(result.uniform_place_invariants == expected_uniform);
  assert(result.s_components.has_value());
  const std::vector<std::vector<std::size_t>> expected_components{{0, 1}};
  assert(*result.s_components == expected_components);
  assert(result.uncovered_places_s_component.empty());
}

void uniform_invariant_search_exhaustion_is_explicit() {
  SoundnessOptions options;
  options.variant = SoundnessVariant::kWoflan;
  options.diagnostics = true;
  options.invariant_search_limit = 1;
  const auto result = analyze_soundness(branching_pair_net(), options);
  assert(result.uniform_invariant_search_exhausted);
  assert(!result.left.has_value());
  const auto serialized = petri_analysis_json(result);
  assert(serialized.find("\"uniform_invariant_search_exhausted\":true") !=
         std::string::npos);
  assert(std::any_of(result.diagnostic_messages.begin(),
                     result.diagnostic_messages.end(), [](const auto& message) {
                       return message.find("uniform invariant search budget exhausted") !=
                              std::string::npos;
                     }));
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

  // Reference Woflan: a marking naming more or fewer than one place makes the
  // net unsound; with one place each, it analyzes source -> sink with one
  // token whatever place or count was given (a 2-token source, a token on the
  // middle place, or a 2-token sink are all sound on this chain).
  SoundnessOptions malformed_marking = options;
  malformed_marking.initial_marking = {1, 1, 0};
  const auto two_places = analyze_soundness(chain(), malformed_marking);
  assert(two_places.status == SoundnessStatus::UNSOUND && two_places.computed);
  assert(two_places.error == "initial marking must contain exactly one marked place");
  malformed_marking.initial_marking = {1, 0, 0};
  malformed_marking.final_marking = {0, 0, 0};
  assert(analyze_soundness(chain(), malformed_marking).status == SoundnessStatus::UNSOUND);
  for (const auto& [initial, final] : std::vector<std::pair<Marking, Marking>>{
           {{2, 0, 0}, {0, 0, 1}}, {{0, 1, 0}, {0, 0, 1}},
           {{1, 0, 0}, {0, 0, 2}}, {{1, 0, 0}, {0, 1, 0}}}) {
    SoundnessOptions single = options;
    single.initial_marking = initial;
    single.final_marking = final;
    assert(analyze_soundness(chain(), single).status == SoundnessStatus::SOUND);
  }
  malformed_marking.initial_marking = {-1, 0, 0};
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

void woflan_ignores_dead_silent_tasks() {
  PetriNet net;
  net.set_activities({"A"});
  const auto source = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.add_transition(0, {source}, {sink});
  // This silent transition is structurally on the workflow path but needs
  // two source tokens, so it is never enabled from the single-token initial
  // marking. Woflan task diagnostics intentionally ignore silent routing.
  net.add_transition(std::nullopt, {source, source}, {sink, sink});

  const auto ordinary = analyze_soundness(net);
  assert(ordinary.status == SoundnessStatus::UNSOUND);
  assert(ordinary.dead_transitions == std::vector<std::size_t>({1}));

  SoundnessOptions options;
  options.variant = SoundnessVariant::kWoflan;
  options.diagnostics = true;
  const auto woflan = analyze_soundness(net, options);
  assert(woflan.status == SoundnessStatus::SOUND);
  assert(woflan.dead_transitions == std::vector<std::size_t>({1}));
  assert(woflan.dead_tasks.empty());
  assert(woflan.not_well_handled_pairs == woflan.handled_pairs);
  const auto json = petri_analysis_json(woflan);
  assert(json.find("\"dead_tasks\":[]") != std::string::npos);
  assert(json.find("\"not_well_handled_pairs\":[]") !=
         std::string::npos);
}

void woflan_restricted_tree_artifacts() {
  SoundnessOptions options;
  options.variant = SoundnessVariant::kWoflan;
  options.diagnostics = true;
  const auto result = analyze_soundness(chain(), options);
  assert(result.left.has_value() && *result.left);
  assert(!result.restricted_coverability_tree_exhausted);
  const std::vector<Marking> expected_markings = {
      {1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
  const std::vector<std::vector<std::size_t>> expected_edges = {
      {0, 1, 0}, {1, 2, 1}};
  assert(result.restricted_coverability_markings == expected_markings);
  assert(result.restricted_coverability_edges == expected_edges);
  const auto json = petri_analysis_json(result);
  assert(json.find("\"left\":true") != std::string::npos);
  assert(json.find("\"restricted_coverability_markings\"") !=
         std::string::npos);

  PetriNet growing;
  growing.set_activities({"start", "grow", "finish"});
  const auto source = growing.add_place();
  const auto middle = growing.add_place();
  const auto sink = growing.add_place();
  growing.set_source(source);
  growing.set_sink(sink);
  growing.add_transition(0, {source}, {middle});
  growing.add_transition(1, {middle}, {middle, middle});
  growing.add_transition(2, {middle}, {sink});
  const auto unbounded = analyze_soundness(growing, options);
  assert(!unbounded.restricted_coverability_markings.empty());
  assert(std::any_of(
      unbounded.restricted_coverability_markings.begin(),
      unbounded.restricted_coverability_markings.end(), [](const auto& marking) {
        return std::any_of(marking.begin(), marking.end(),
                           [](const auto value) { return value == -1; });
      }));

  SoundnessOptions capped = options;
  capped.max_states = 2;
  const auto limited = analyze_soundness(chain(), capped);
  assert(limited.restricted_coverability_tree_exhausted);
  assert(limited.restricted_coverability_markings.size() == 2);

  PetriNet extended;
  extended.set_activities({"A", "B"});
  const auto ep0 = extended.add_place();
  const auto ep1 = extended.add_place();
  const auto ep2 = extended.add_place();
  extended.set_source(ep0);
  extended.set_sink(ep2);
  extended.add_transition(0, {ep0}, {ep1});
  extended.add_transition(1, {ep1}, {ep2});
  extended.add_reset_arc(1, ep0);
  const auto extended_result = analyze_soundness(extended, options);
  assert(extended_result.restricted_coverability_tree_exhausted);
  assert(extended_result.restricted_coverability_markings.empty());
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
  assert(early_locking.deadlock_witnesses.empty());
  assert(early_locking.reachability_markings.empty());
  assert(early_locking.coverability_markings.empty());
  assert(early_locking.restricted_coverability_markings.empty());
  const auto early_json = petri_analysis_json(early_locking);
  assert(early_json.find("\"returned_asap\":true") !=
         std::string::npos);

  // Once structural coverage is complete, return-asap still stops on the
  // first reachable non-final deadlock and retains its short witness.
  PetriNet covered_deadlock;
  covered_deadlock.set_activities({"A"});
  const auto covered_source = covered_deadlock.add_place();
  const auto covered_sink = covered_deadlock.add_place();
  covered_deadlock.set_source(covered_source);
  covered_deadlock.set_sink(covered_sink);
  covered_deadlock.add_transition(0, {covered_source, covered_source},
                                  {covered_sink, covered_sink});
  const auto covered_early = analyze_soundness(covered_deadlock, early);
  assert(covered_early.returned_asap && covered_early.computed &&
         covered_early.status == SoundnessStatus::UNSOUND);
  assert(covered_early.left.has_value() && *covered_early.left);
  assert(covered_early.deadlock_witnesses ==
         std::vector<std::vector<std::size_t>>{{}});
  assert(covered_early.reachability_markings.size() == 1);
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

// The reference marking equation adds -1/+1 per arc object, whatever its
// weight (formerly declared DC4): for the sequence A -> B with every arc of
// weight w and w tokens in/out it reports 2w, and native now does too.
void weighted_marking_equation_counts_arc_objects() {
  for (const std::int64_t w : {1, 2, 3}) {
    minera::PetriNet net;
    const auto p0 = net.add_place(), p1 = net.add_place(), p2 = net.add_place();
    net.set_activities({"A", "B"});
    net.add_transition(0, std::vector<std::size_t>(w, p0), std::vector<std::size_t>(w, p1));
    net.add_transition(1, std::vector<std::size_t>(w, p1), std::vector<std::size_t>(w, p2));
    minera::MarkingEquationOptions options;
    options.initial_marking = {w, 0, 0};
    options.final_marking = {0, 0, w};
    options.transition_costs = {1.0, 1.0};
    const auto result = minera::solve_marking_equation(net, options);
    assert(result.computed && result.feasible);
    assert(result.objective == 2.0 * static_cast<double>(w));  // reference: 2, 4, 6
  }
  // Two parallel unit arcs count twice: a -> pm (x2) -> b, one token in and
  // out, gives the reference objective 2 as well.
  const auto parallel = compact_net(3, {"a", "b"},
      {{0, {{0}, {1}}, {{1, 1}, {1, 1}}}, {1, {{1, 1}, {1, 1}}, {{2}, {1}}}});
  minera::MarkingEquationOptions options;
  options.initial_marking = {1, 0, 0};
  options.final_marking = {0, 0, 1};
  options.transition_costs = {1.0, 1.0};
  const auto result = minera::solve_marking_equation(parallel, options);
  assert(result.computed && result.feasible && result.objective == 2.0);
}

// Woflan's invariant stages use a weighted incidence (each arc object adds
// its signed weight), so an AND branch whose arcs weigh 2 is outside every
// S-component, and a net with weighted or parallel arcs that is not
// S-coverable is unsound, as the reference reports even when the net behaves
// soundly. Parallel weights 2 and -1 sum to 1 and keep the net S-coverable
// and sound. Black-box values: woflan and check_soundness False, False, True.
void woflan_weighted_and_parallel_arcs() {
  // ps -a-> p1, p2 (weight 2); p1 -b-> q1; p2 (2) -c-> q2; q1, q2 -d-> pe.
  PetriNet branch;
  branch.set_activities({"a", "b", "c", "d"});
  for (int i = 0; i < 6; ++i) branch.add_place();
  branch.set_source(0); branch.set_sink(5);
  branch.add_transition(0, {0}, {1, 2, 2});
  branch.add_transition(1, {1}, {3});
  branch.add_transition(2, {2, 2}, {4});
  branch.add_transition(3, {3, 4}, {5});
  // ps -a-> pm (weight 2) -b(2)-> pe and its parallel-arc twin.
  PetriNet doubled;
  doubled.set_activities({"a", "b"});
  for (int i = 0; i < 3; ++i) doubled.add_place();
  doubled.set_source(0); doubled.set_sink(2);
  doubled.add_transition(0, {0}, {1, 1});
  doubled.add_transition(1, {1, 1}, {2});
  const auto parallel = compact_net(3, {"a", "b"},
      {{0, {{0}, {1}}, {{1, 1}, {1, 1}}}, {1, {{1, 1}, {1, 1}}, {{2}, {1}}}});
  const auto cancelling = compact_net(3, {"a", "b"},
      {{0, {{0}, {1}}, {{1, 1}, {2, -1}}}, {1, {{1}, {1}}, {{2}, {1}}}});
  for (const auto variant : {SoundnessVariant::kSoundness, SoundnessVariant::kWoflan}) {
    SoundnessOptions options;
    options.variant = variant;
    const auto split = analyze_soundness(branch, options);
    assert(split.computed && split.status == SoundnessStatus::UNSOUND);
    assert(split.uncovered_places_s_component == std::vector<std::size_t>({2, 4}));
    assert(split.uncovered_places_uniform == std::vector<std::size_t>({2, 4}));
    assert(analyze_soundness(doubled, options).status == SoundnessStatus::UNSOUND);
    assert(analyze_soundness(parallel, options).status == SoundnessStatus::UNSOUND);
    const auto sound = analyze_soundness(cancelling, options);
    assert(sound.status == SoundnessStatus::SOUND && sound.uncovered_places_s_component.empty());
  }
}

// The log dispatcher on weighted and parallel arcs, against reference values:
// the weight-2 sequence is easy sound and aligns <a, b> at cost 0, its
// parallel-arc twin is refused as not easy sound, and on the synchronous
// product (every arc weight 1, parallel arcs kept) a transition with k
// parallel input arcs from a place needs k tokens there but takes one, so
// <a, c, b> cannot finish through c and b (20001; 10001 for <a, b>).
void alignment_weighted_and_parallel_arcs() {
  const auto log_of = [](std::vector<std::vector<std::string>> traces) {
    EventLog log;
    for (std::size_t i = 0; i < traces.size(); ++i) {
      Trace trace;
      trace.case_id = std::to_string(i);
      for (const auto& activity : traces[i]) {
        Event event;
        event.case_id = trace.case_id;
        event.activity = activity;
        trace.events.push_back(event);
      }
      log.traces.push_back(trace);
    }
    return log;
  };
  // The reference default variant (state-equation A*).
  PetriLogAlignmentOptions options;
  options.trace.variant = AlignmentVariant::kStateEquationAStar;
  PetriNet doubled;
  doubled.set_activities({"a", "b"});
  for (int i = 0; i < 3; ++i) doubled.add_place();
  doubled.set_source(0); doubled.set_sink(2);
  doubled.add_transition(0, {0}, {1, 1});
  doubled.add_transition(1, {1, 1}, {2});
  const auto weighted = align_petri_log(log_of({{"a", "b"}, {"a", "c", "b"}}), doubled, options);
  assert(weighted.size() == 2 && weighted[0].computed && weighted[0].cost == 0.0);
  assert(weighted[1].computed && weighted[1].cost == 10000.0);
  const auto parallel = compact_net(3, {"a", "b"},
      {{0, {{0}, {1}}, {{1, 1}, {1, 1}}}, {1, {{1, 1}, {1, 1}}, {{2}, {1}}}});
  bool refused = false;
  try { (void)align_petri_log(log_of({{"a", "b"}}), parallel, options); }
  catch (const std::invalid_argument&) { refused = true; }
  assert(refused);
  // Review W1-5: a reset net whose reachability search stops at its limit is
  // refused as undecided, not as "not easy sound".
  {
    PetriNet endless;
    endless.set_activities({"a"});
    const auto source = endless.add_place(), p1 = endless.add_place(), q = endless.add_place(),
               sink = endless.add_place();
    endless.add_transition(std::nullopt, {source}, {p1});
    const auto pump = endless.add_transition(std::nullopt, {p1}, {p1, q});
    endless.add_reset_arc(pump, sink);
    endless.set_source(source);
    endless.set_sink(sink);
    std::string message;
    try { (void)align_petri_log(log_of({{"a"}}), endless, options); }
    catch (const std::invalid_argument& error) { message = error.what(); }
    assert(message.find("undecided") != std::string::npos);
  }
  // ps -a-> pm; pm -c-> pq (x2); pq (x2) -b-> pe; pm -tau-> pe.
  const auto sync = compact_net(4, {"a", "b", "c"},
      {{0, {{0}, {}}, {{1}, {}}},
       {2, {{1}, {}}, {{2, 2}, {1, 1}}},
       {1, {{2, 2}, {1, 1}}, {{3}, {}}},
       {std::nullopt, {{1}, {}}, {{3}, {}}}});
  const auto aligned = align_petri_log(log_of({{"a", "c", "b"}, {"a", "b"}}), sync, options);
  assert(aligned.size() == 2 && aligned[0].computed && aligned[0].cost == 20001.0);
  assert(aligned[1].computed && aligned[1].cost == 10001.0);
  // DIJKSTRA_LESS_MEMORY fires the net itself (each arc its own weight), so
  // c and b synchronise there: 0 and 10000, as the reference variant gives.
  options.trace.variant = AlignmentVariant::kDijkstraLessMemory;
  const auto direct = align_petri_log(log_of({{"a", "c", "b"}, {"a", "b"}}), sync, options);
  assert(direct[0].computed && direct[0].cost == 0.0 && direct[1].cost == 10000.0);

  // The easy-soundness search honours the aligner's time limit and
  // cancellation. "pump" adds two tokens to the middle place, which "b"
  // empties two at a time from an odd count, so the final marking is never
  // reached although the marking equation's relaxation is feasible, and the
  // search grows without bound.
  const auto unbounded = compact_net(3, {"a", "pump", "b"},
      {{0, {{0}, {1}}, {{1}, {1}}}, {1, {{1}, {1}}, {{1}, {3}}}, {2, {{1}, {2}}, {{2}, {1}}}});
  options.trace.variant = AlignmentVariant::kStateEquationAStar;
  options.trace.max_states = std::numeric_limits<std::size_t>::max();
  options.trace.max_time_seconds = 0.05;
  refused = false;
  try { (void)align_petri_log(log_of({{"a", "b"}}), unbounded, options); }
  catch (const std::invalid_argument&) { refused = true; }
  assert(refused);
  options.trace.max_time_seconds = 0.0;
  options.trace.cancelled = [] { return true; };
  bool cancelled = false;
  try { (void)align_petri_log(log_of({{"a", "b"}}), unbounded, options); }
  catch (const OperationCancelled&) { cancelled = true; }
  assert(cancelled);

  // The less-memory variants' own pruned search, on the same net and trace
  // (the single-trace entry point skips the easy-soundness check): a cancel
  // throws, and a search past its time is uncomputed, exhausted and marked
  // as having reached the time limit, as the aligner reports it. A search
  // past max_states is exhausted with time to spare.
  for (const auto variant : {AlignmentVariant::kDijkstraLessMemory,
                             AlignmentVariant::kGeneratorDijkstraLessMemory}) {
    PetriTraceAlignmentOptions trace_options;
    trace_options.variant = variant;
    trace_options.max_states = std::numeric_limits<std::size_t>::max();
    trace_options.cancelled = [] { return true; };
    cancelled = false;
    try { (void)align_petri_trace_all({"a", "b"}, unbounded, trace_options); }
    catch (const OperationCancelled&) { cancelled = true; }
    assert(cancelled);
    trace_options.cancelled = nullptr;
    trace_options.max_time_seconds = 0.05;
    const auto timed = align_petri_trace_all({"a", "b"}, unbounded, trace_options);
    assert(timed.size() == 1 && !timed[0].computed && timed[0].exhausted &&
           timed[0].time_limit_reached && timed[0].diagnostics.visited_states > 0);
    trace_options.max_time_seconds = 0.0;
    trace_options.max_states = 50;
    const auto bounded = align_petri_trace_all({"a", "b"}, unbounded, trace_options);
    assert(bounded.size() == 1 && !bounded[0].computed && bounded[0].exhausted &&
           !bounded[0].time_limit_reached);
  }
  // The other variants run the aligner, whose cancel now throws here too and
  // whose time limit marks the entry. Eight parallel branches of three steps
  // each against a trace of 40 foreign events give the uninformed search far
  // more than 0.05 s of states.
  std::vector<std::string> labels{"x"};
  std::vector<CompactTransition> steps;
  CompactTransition split{std::nullopt, {{0}, {1}}, {{}, {}}};
  CompactTransition join{std::nullopt, {{}, {}}, {{33}, {1}}};
  for (std::size_t branch = 0; branch < 8; ++branch) {
    const std::size_t first = 1 + branch * 4;
    split.outputs.places.push_back(first);
    split.outputs.weights.push_back(1);
    for (std::size_t step = 0; step < 3; ++step) {
      labels.push_back("t" + std::to_string(branch) + std::to_string(step));
      steps.push_back({labels.size() - 1, {{first + step}, {1}}, {{first + step + 1}, {1}}});
    }
    join.inputs.places.push_back(first + 3);
    join.inputs.weights.push_back(1);
  }
  steps.push_back(split);
  steps.push_back(join);
  const auto leaky = compact_net(34, labels, steps);
  const std::vector<std::string> foreign(40, "x");
  PetriTraceAlignmentOptions plain;
  plain.variant = AlignmentVariant::kDijkstraNoHeuristics;
  plain.max_states = std::numeric_limits<std::size_t>::max();
  plain.cancelled = [] { return true; };
  cancelled = false;
  try { (void)align_petri_trace(foreign, leaky, plain); }
  catch (const OperationCancelled&) { cancelled = true; }
  assert(cancelled);
  plain.cancelled = nullptr;
  plain.max_time_seconds = 0.05;
  const auto timed = align_petri_trace(foreign, leaky, plain);
  assert(!timed.computed && timed.exhausted && timed.time_limit_reached);
}

int main() {
  weighted_marking_equation_counts_arc_objects();
  woflan_weighted_and_parallel_arcs();
  alignment_weighted_and_parallel_arcs();
  classic_equation();
  incidence_feasible_not_realizable();
  classic_equation_requires_integer_solutions();
  extended_equation();
  workflow_structure();
  sound_models();
  positive_invariant_diagnostics();
  weighted_invariant_contract();
  uniform_invariant_combination();
  s_component_uses_distinct_places();
  uniform_invariant_search_exhaustion_is_explicit();
  structural_pair_diagnostics();
  woflan_short_circuit_diagnostics();
  woflan_ignores_dead_silent_tasks();
  woflan_restricted_tree_artifacts();
  unsound_models();
  caps_markings_determinism_and_weights();
}
