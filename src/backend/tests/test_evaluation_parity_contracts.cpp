#include "minera/dfg_precision.hpp"
#include "minera/evaluation.hpp"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace minera;

EventLog log(std::initializer_list<std::initializer_list<const char*>> traces) {
  EventLog result;
  std::size_t order = 0;
  std::size_t case_number = 0;
  for (const auto activities : traces) {
    Trace trace;
    trace.case_id = "case-" + std::to_string(case_number++);
    for (const auto activity : activities) {
      Event event;
      event.case_id = trace.case_id;
      event.activity = activity;
      event.input_order = order++;
      trace.events.push_back(std::move(event));
    }
    result.traces.push_back(std::move(trace));
  }
  return result;
}

EventLog custom_activity_log() {
  EventLog result;
  Trace trace;
  trace.case_id = "custom";
  Event first;
  first.case_id = trace.case_id;
  first.activity = "raw-a";
  first.input_order = 0;
  first.attributes.push_back({"phase", "A", AttributeKind::kString});
  Event second;
  second.case_id = trace.case_id;
  second.activity = "raw-b";
  second.input_order = 1;
  second.attributes.push_back({"phase", "B", AttributeKind::kString});
  trace.events = {std::move(first), std::move(second)};
  result.traces.push_back(std::move(trace));
  return result;
}

PetriNet sequence_net() {
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

PetriNet two_a_net() {
  PetriNet net;
  net.set_activities({"A"});
  const auto source = net.add_place();
  const auto middle = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.add_transition(0, {source}, {middle});
  net.add_transition(0, {middle}, {sink});
  return net;
}

PetriNet branching_net() {
  PetriNet net;
  net.set_activities({"A", "B", "C"});
  const auto source = net.add_place();
  const auto branch = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.add_transition(0, {source}, {branch});
  net.add_transition(1, {branch}, {sink});
  net.add_transition(2, {branch}, {sink});
  return net;
}

PetriNet duplicate_label_net() {
  PetriNet net;
  net.set_activities({"A", "B"});
  const auto source = net.add_place();
  const auto left = net.add_place();
  const auto right = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.add_transition(0, {source}, {left});
  net.add_transition(0, {source}, {right});
  net.add_transition(1, {left}, {sink});
  net.add_transition(1, {right}, {sink});
  return net;
}

PetriNet extended_arc_net() {
  PetriNet net;
  net.set_activities({"A"});
  const auto source = net.add_place();
  const auto reset = net.add_place();
  const auto inhibitor = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  const auto transition = net.add_transition(0, {source}, {sink});
  net.add_reset_arc(transition, reset);
  net.add_inhibitor_arc(transition, inhibitor);
  return net;
}

PetriNet cardoso_net() {
  PetriNet net;
  net.set_activities({"A", "B"});
  const auto source = net.add_place();
  const auto left = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.add_transition(0, {source}, {left, left, sink});
  net.add_transition(1, {source}, {left, sink});
  return net;
}

PetriNet cyclomatic_scc_net() {
  // Both source and side start places have no preset.  The default initial
  // marking therefore explores the four distinct token distributions, while
  // the reachability graph remains acyclic and has one SCC per state.
  PetriNet net;
  net.set_activities({"A", "B"});
  const auto source = net.add_place();
  const auto side = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.add_transition(0, {source}, {sink});
  net.add_transition(1, {side}, {sink});
  return net;
}

PetriNet unsound_net() {
  PetriNet net;
  net.set_activities({"A"});
  const auto source = net.add_place();
  const auto dead = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.add_transition(0, {source}, {dead});
  return net;
}

DfgCleanAnalysis dfg_model() {
  DfgCleanAnalysis model;
  model.graph[ {"A", "B"} ] = 1;
  model.graph[ {"A", "C"} ] = 1;
  model.start_activities["A"] = 1;
  model.end_activities["B"] = 1;
  model.end_activities["C"] = 1;
  return model;
}

bool close_to(double actual, double expected, double epsilon = 1e-12) {
  return std::abs(actual - expected) <= epsilon;
}

void replay_fitness_token_contract() {
  const auto net = sequence_net();
  const auto result = evaluate_replay_fitness(
      log({{"A"}}), net, ReplayFitnessVariant::TOKEN_BASED);
  assert(result.computed);
  assert(result.variant == ReplayFitnessVariant::TOKEN_BASED);
  assert(result.trace_count == 1 && result.fitting_traces == 0);
  assert(result.token_result.produced == 2 &&
         result.token_result.consumed == 2 && result.total_cost == 0.0);
  assert(result.token_result.missing == 1);
  assert(result.token_result.remaining == 1);
  assert(close_to(result.fitness, 0.5));
  assert(close_to(result.log_fitness, 0.5));
  assert(close_to(result.average_trace_fitness, 0.5));
  assert(result.percentage_fit_traces == 0.0);

  // Evaluator replay excludes unknown labels unless the caller opts into the
  // stricter policy.  This keeps a valid model run valid in the default mode.
  const auto with_unknown = evaluate_replay_fitness(
      log({{"A", "B", "X"}}), net, ReplayFitnessVariant::TOKEN_BASED);
  assert(with_unknown.computed && with_unknown.fitting_traces == 1);
  assert(close_to(with_unknown.fitness, 1.0));

  ReplayFitnessOptions include_unknown;
  include_unknown.consider_activities_not_in_model_in_fitness = true;
  const auto penalized = evaluate_replay_fitness(
      log({{"A", "B", "X"}}), net, ReplayFitnessVariant::TOKEN_BASED,
      include_unknown);
  assert(penalized.computed && penalized.fitting_traces == 0);
  assert(penalized.fitness < with_unknown.fitness);
}

void replay_fitness_backwards_token_contract() {
  const auto net = two_a_net();
  const auto forward = evaluate_replay_fitness(
      log({{"A"}}), net, ReplayFitnessVariant::TOKEN_BASED);
  ReplayFitnessOptions backwards_options;
  backwards_options.token_replay_variant = TokenReplayVariant::kBackwards;
  const auto backwards = evaluate_replay_fitness(
      log({{"A"}}), net, ReplayFitnessVariant::TOKEN_BASED,
      backwards_options);
  assert(forward.computed && backwards.computed);
  assert(forward.fitting_traces == 0);
  assert(backwards.fitting_traces == 1);
  assert(close_to(backwards.fitness, 1.0));
}

void replay_fitness_alignment_contract() {
  ReplayFitnessOptions options;
  options.alignment_options.variant = AlignmentVariant::kDijkstraNoHeuristics;
  options.alignment_options.cost_profile = AlignmentCostProfile::kStandard;
  const auto result = evaluate_replay_fitness(
      log({{"A", "X"}}), sequence_net(),
      ReplayFitnessVariant::ALIGNMENT_BASED, options);
  assert(result.computed);
  assert(result.variant == ReplayFitnessVariant::ALIGNMENT_BASED);
  assert(result.alignment_result.variant ==
         AlignmentVariant::kDijkstraNoHeuristics);
  assert(result.trace_count == 1 && result.fitting_traces == 0);
  // X is a log-only move and B is a visible model-only move.  Under the
  // standard profile each costs 10000, while A is synchronous.
  assert(close_to(result.total_cost, 20000.0));
  assert(close_to(result.fitness, 0.5));

  const auto perfect = evaluate_replay_fitness(
      custom_activity_log(), sequence_net(),
      ReplayFitnessVariant::ALIGNMENT_BASED,
      ReplayFitnessOptions{false, "phase", TokenReplayVariant::kForward,
                           {AlignmentVariant::kDijkstraNoHeuristics,
                            AlignmentCostProfile::kStandard}});
  assert(perfect.computed && close_to(perfect.fitness, 1.0));
  assert(perfect.alignment_result.activity_attribute == "phase");
}

void precision_etconformance_token_contract() {
  PrecisionOptions options;
  options.token_replay_variant = TokenReplayVariant::kForward;
  const auto result = evaluate_precision(
      log({{"A", "B"}}), sequence_net(),
      PrecisionVariant::ETCONFORMANCE_TOKEN, options);
  assert(result.computed);
  assert(result.variant == PrecisionVariant::ETCONFORMANCE_TOKEN);
  assert(result.token_result.activity_attribute == "concept:name");
  assert(close_to(result.precision, 1.0));
}

void precision_align_etconformance_contract() {
  const auto result = evaluate_precision(
      log({{"A", "B"}}), branching_net(),
      PrecisionVariant::ALIGN_ETCONFORMANCE);
  assert(result.computed);
  assert(result.variant == PrecisionVariant::ALIGN_ETCONFORMANCE);
  assert(close_to(result.precision, 2.0 / 3.0));
}

void precision_automaton_after_align_contract() {
  const auto result = evaluate_precision(
      log({{"A", "B"}}), sequence_net(),
      PrecisionVariant::AUTOMATON_AFTER_ALIGN);
  assert(result.computed);
  assert(result.variant == PrecisionVariant::AUTOMATON_AFTER_ALIGN);
  assert(result.alignment_result.variant ==
         AlignmentVariant::kDijkstraLessMemory);
  assert(close_to(result.precision, 1.0));
}

void precision_initial_prefix_contract() {
  // The root prefix is scored as well as the marking after A: A is the only
  // initial choice, then B and C are both enabled and C escapes the log.
  const auto result = evaluate_precision(
      log({{"A", "B"}}), branching_net(),
      PrecisionVariant::ALIGN_ETCONFORMANCE);
  assert(result.computed && close_to(result.precision, 2.0 / 3.0));
}

void precision_excludes_unfit_prefix_contract() {
  const auto result = evaluate_precision(
      log({{"A", "X", "Y"}}), branching_net(),
      PrecisionVariant::ALIGN_ETCONFORMANCE);
  // The fitting root and A prefixes contribute 1 + 2 enabled choices.  The
  // non-replayable A,X prefix is excluded rather than inventing a marking.
  assert(result.computed && !result.exhausted);
  assert(close_to(result.precision, 1.0 / 3.0));
}

void precision_task_ids_contract() {
  const auto net = duplicate_label_net();
  const auto labels = evaluate_precision(
      log({{"A", "B"}}), net, PrecisionVariant::AUTOMATON_AFTER_ALIGN);
  PrecisionOptions options;
  options.use_task_ids = true;
  const auto task_ids = evaluate_precision(
      log({{"A", "B"}}), net, PrecisionVariant::AUTOMATON_AFTER_ALIGN,
      options);
  assert(labels.computed && task_ids.computed);
  assert(close_to(labels.precision, 1.0));
  assert(close_to(task_ids.precision, 2.0 / 3.0));
}

void precision_auto_dispatch_contract() {
  const auto sound = evaluate_precision(log({{"A", "B"}}), sequence_net());
  const auto unsound = evaluate_precision(log({{"A"}}), unsound_net());
  assert(sound.computed &&
         sound.variant == PrecisionVariant::ALIGN_ETCONFORMANCE);
  assert(unsound.computed &&
         unsound.variant == PrecisionVariant::ETCONFORMANCE_TOKEN);
}

void custom_activity_dispatch_contract() {
  const auto events = custom_activity_log();
  const auto net = sequence_net();

  ReplayFitnessOptions replay_options;
  replay_options.activity_attribute = "phase";
  const auto token = evaluate_replay_fitness(
      events, net, ReplayFitnessVariant::TOKEN_BASED, replay_options);
  assert(token.computed && close_to(token.fitness, 1.0));
  assert(token.token_result.activity_attribute == "phase");

  const auto alignment = evaluate_replay_fitness(
      events, net, ReplayFitnessVariant::ALIGNMENT_BASED, replay_options);
  assert(alignment.computed && close_to(alignment.fitness, 1.0));
  assert(alignment.alignment_result.activity_attribute == "phase");

  PrecisionOptions precision_options;
  precision_options.activity_attribute = "phase";
  for (const auto variant : {PrecisionVariant::ETCONFORMANCE_TOKEN,
                             PrecisionVariant::ALIGN_ETCONFORMANCE,
                             PrecisionVariant::AUTOMATON_AFTER_ALIGN}) {
    const auto result = evaluate_precision(
        events, net, variant, precision_options);
    assert(result.computed && close_to(result.precision, 1.0));
  }

  GeneralizationOptions generalization_options;
  generalization_options.activity_attribute = "phase";
  const auto generalization = evaluate_generalization(
      events, net, GeneralizationVariant::GENERALIZATION_TOKEN,
      generalization_options);
  assert(generalization.computed);
  assert(generalization.transition_frequencies ==
         std::vector<std::uint64_t>({1, 1}));

  CompositeEvaluationOptions composite_options;
  composite_options.activity_attribute = "phase";
  const auto composite = evaluate_composite(events, net, composite_options);
  assert(composite.computed && close_to(composite.fitness, 1.0));
}

void marked_net_dispatch_contract() {
  const auto net = sequence_net();
  const auto events = log({{"B"}});
  const std::vector<int> initial{0, 1, 0};
  const std::vector<int> final{0, 0, 1};

  ReplayFitnessOptions replay;
  replay.initial_marking = initial;
  replay.final_marking = final;
  const auto token = evaluate_replay_fitness(
      events, net, ReplayFitnessVariant::TOKEN_BASED, replay);
  const auto alignment = evaluate_replay_fitness(
      events, net, ReplayFitnessVariant::ALIGNMENT_BASED, replay);
  const auto automatic = evaluate_replay_fitness(
      events, net, ReplayFitnessVariant::AUTO, replay);
  assert(token.computed && alignment.computed && automatic.computed);
  assert(close_to(token.fitness, 1.0) && close_to(alignment.fitness, 1.0));
  assert(automatic.variant == ReplayFitnessVariant::ALIGNMENT_BASED &&
         close_to(automatic.fitness, 1.0));

  for (const auto variant : {PrecisionVariant::ETCONFORMANCE_TOKEN,
                             PrecisionVariant::ALIGN_ETCONFORMANCE,
                             PrecisionVariant::AUTOMATON_AFTER_ALIGN}) {
    PrecisionOptions options;
    options.initial_marking = initial;
    options.final_marking = final;
    const auto precision = evaluate_precision(events, net, variant, options);
    assert(precision.computed && close_to(precision.precision, 1.0));
  }

  GeneralizationOptions generalization_options;
  generalization_options.initial_marking = initial;
  generalization_options.final_marking = final;
  const auto generalization = evaluate_generalization(
      events, net, GeneralizationVariant::GENERALIZATION_TOKEN,
      generalization_options);
  assert(generalization.computed &&
         generalization.transition_frequencies ==
             std::vector<std::uint64_t>({0, 1}));

  CompositeEvaluationOptions composite_options;
  composite_options.initial_marking = initial;
  composite_options.final_marking = final;
  const auto composite =
      evaluate_composite(events, net, composite_options);
  assert(composite.computed && close_to(composite.fitness, 1.0) &&
         close_to(composite.precision, 1.0));

  // A marked evaluator may finish at an internal place.  Verify that the
  // requested middle-place goal is propagated through every ordinary
  // alignment engine and every evaluator family.
  const auto marked_events = log({{"A"}});
  const std::vector<int> source_marking{1, 0, 0};
  const std::vector<int> middle_marking{0, 1, 0};
  for (const auto variant : {
           AlignmentVariant::kStateEquationAStar,
           AlignmentVariant::kDijkstraNoHeuristics,
           AlignmentVariant::kDijkstraLessMemory,
           AlignmentVariant::kDiscountedAStar,
           AlignmentVariant::kGeneratorDijkstraNoHeuristics,
           AlignmentVariant::kGeneratorDijkstraLessMemory}) {
    AlignmentOptions alignment;
    alignment.variant = variant;
    alignment.initial_marking = source_marking;
    alignment.final_marking = middle_marking;
    const auto result = check_alignment(marked_events, net, alignment);
    assert(result.computed && close_to(result.fitness, 1.0) &&
           result.fitting_traces == 1);
  }

  ReplayFitnessOptions marked_replay;
  marked_replay.initial_marking = source_marking;
  marked_replay.final_marking = middle_marking;
  marked_replay.decomposed_alignment_options =
      DecomposedAlignmentOptions{};
  const auto marked_decomposed = evaluate_replay_fitness(
      marked_events, net, ReplayFitnessVariant::ALIGNMENT_BASED,
      marked_replay);
  assert(marked_decomposed.computed &&
         close_to(marked_decomposed.fitness, 1.0) &&
         marked_decomposed.decomposed_alignment_result.has_value());

  for (const auto variant : {
           PrecisionVariant::ETCONFORMANCE_TOKEN,
           PrecisionVariant::ALIGN_ETCONFORMANCE,
           PrecisionVariant::AUTOMATON_AFTER_ALIGN,
           PrecisionVariant::AUTO}) {
    PrecisionOptions options;
    options.initial_marking = source_marking;
    options.final_marking = middle_marking;
    const auto result = evaluate_precision(marked_events, net, variant, options);
    assert(result.computed && close_to(result.precision, 1.0));
  }

  GeneralizationOptions marked_generalization_options;
  marked_generalization_options.initial_marking = source_marking;
  marked_generalization_options.final_marking = middle_marking;
  const auto marked_generalization = evaluate_generalization(
      marked_events, net, GeneralizationVariant::GENERALIZATION_TOKEN,
      marked_generalization_options);
  assert(marked_generalization.computed &&
         marked_generalization.transition_frequencies ==
             std::vector<std::uint64_t>({1, 0}));

  CompositeEvaluationOptions marked_composite_options;
  marked_composite_options.initial_marking = source_marking;
  marked_composite_options.final_marking = middle_marking;
  const auto marked_composite = evaluate_composite(
      marked_events, net, marked_composite_options);
  assert(marked_composite.computed &&
         close_to(marked_composite.fitness, 1.0) &&
         close_to(marked_composite.precision, 1.0));
}

void invalid_evaluator_selector_contract() {
  const auto events = log({{"A", "B"}});
  const auto net = sequence_net();
  assert(!evaluate_replay_fitness(
              events, net, static_cast<ReplayFitnessVariant>(999))
              .computed);
  assert(!evaluate_precision(events, net,
                             static_cast<PrecisionVariant>(999))
              .computed);
  assert(!evaluate_generalization(
              events, net, static_cast<GeneralizationVariant>(999))
              .computed);
  assert(!evaluate_simplicity(net, static_cast<SimplicityVariant>(999))
              .computed);
}

void generalization_token_contract() {
  const auto result = evaluate_generalization(
      log({{"A"}}), sequence_net(),
      GeneralizationVariant::GENERALIZATION_TOKEN);
  assert(result.computed);
  assert(result.variant == GeneralizationVariant::GENERALIZATION_TOKEN);
  assert(result.transition_count == 2 && result.executed_transitions == 1);
  assert(result.transition_frequencies ==
         std::vector<std::uint64_t>({1, 0}));
  // The token formula assigns a unit rarity penalty to the unseen B edge and
  // a unit penalty to the once-seen A edge, yielding zero for two transitions.
  assert(close_to(result.generalization, 0.0));
}

void dfg_precision_default_contract() {
  const auto result = evaluate_dfg_precision(
      log({{"A", "B"}, {"A", "C"}}), dfg_model());
  assert(result.computed && close_to(result.precision, 1.0));
  assert(result.prefix_occurrences == 4);
  assert(result.allowed_prefix_occurrences == 4);
  assert(result.activated_transitions == 6);
  assert(result.escaping_transitions == 0);

  const auto empty = evaluate_dfg_precision(EventLog{}, dfg_model());
  assert(empty.computed && close_to(empty.precision, 1.0));
  assert(empty.prefix_occurrences == 0 &&
         empty.allowed_prefix_occurrences == 0 &&
         empty.activated_transitions == 0 && empty.escaping_transitions == 0);
}

void simplicity_arc_degree_contract() {
  SimplicityOptions options;
  options.arc_degree_k = 0.0;
  const auto zero = evaluate_simplicity(
      extended_arc_net(), SimplicityVariant::ARC_DEGREE, options);
  assert(zero.computed && zero.variant == SimplicityVariant::ARC_DEGREE);
  assert(close_to(zero.complexity, 1.6));
  assert(close_to(zero.value, 1.0 / 2.6));

  options.arc_degree_k = -1.0;
  const auto negative = evaluate_simplicity(
      extended_arc_net(), SimplicityVariant::ARC_DEGREE, options);
  assert(negative.computed && close_to(negative.complexity, 2.6));
  assert(close_to(negative.value, 1.0 / 3.6));
}

void simplicity_extended_cardoso_contract() {
  const auto result = evaluate_simplicity(
      cardoso_net(), SimplicityVariant::EXTENDED_CARDOSO);
  assert(result.computed);
  assert(result.variant == SimplicityVariant::EXTENDED_CARDOSO);
  assert(close_to(result.value, 1.0));
  assert(close_to(result.simplicity, 1.0));
}

void simplicity_extended_cyclomatic_contract() {
  const auto result = evaluate_simplicity(
      cyclomatic_scc_net(), SimplicityVariant::EXTENDED_CYCLOMATIC);
  assert(result.computed);
  assert(result.variant == SimplicityVariant::EXTENDED_CYCLOMATIC);
  assert(result.vertices == 4 && result.edges == 4);
  assert(result.strongly_connected_components == 4);
  assert(close_to(result.value, 4.0));
}

void composite_default_contract() {
  const auto events = log({{"A", "B"}, {"A"}});
  const auto net = sequence_net();
  const auto result = evaluate_composite(events, net);
  const auto fitness = evaluate_replay_fitness(
      events, net, ReplayFitnessVariant::TOKEN_BASED);
  const auto precision = evaluate_precision(
      events, net, PrecisionVariant::ETCONFORMANCE_TOKEN);
  const auto generalization = evaluate_generalization(events, net);
  const auto simplicity = evaluate_simplicity(net, SimplicityVariant::ARC_DEGREE);

  assert(result.computed);
  assert(close_to(result.fitness_weight, 0.25));
  assert(close_to(result.precision_weight, 0.25));
  assert(close_to(result.generalization_weight, 0.25));
  assert(close_to(result.simplicity_weight, 0.25));
  assert(close_to(result.fitness, fitness.log_fitness));
  assert(close_to(result.precision, precision.precision));
  assert(close_to(result.generalization, generalization.generalization));
  assert(close_to(result.simplicity, simplicity.simplicity));
  const double expected_average =
      0.25 * (result.fitness + result.precision + result.generalization +
              result.simplicity);
  assert(close_to(result.metrics_average_weight, expected_average));
  const double denominator = result.fitness + result.precision;
  const double expected_fscore = denominator > 0.0
                                     ? 2.0 * result.fitness * result.precision /
                                           denominator
                                     : 0.0;
  assert(close_to(result.fscore, expected_fscore));
}

void emd_pyemd_contract() {
  StochasticLanguage left{{{{"A"}, 0.25},
                           {{"A"}, 0.25},
                           {{"B"}, 0.5}},
                          1.0};
  StochasticLanguage right{{{{"A"}, 0.5}, { {"C"}, 0.5 }}, 1.0};

  EmdOptions options;
  options.variant = EmdVariant::PYEMD;
  options.distance = [](const std::vector<std::string>&,
                        const std::vector<std::string>&) { return 0.0; };
  const auto fast = evaluate_emd(left, right, options);
  assert(fast.computed && fast.variant == EmdVariant::PYEMD);
  assert(fast.use_fast_emd && close_to(fast.distance, 0.0));

  options.use_fast_emd = false;
  const auto fallback = evaluate_emd(left, right, options);
  assert(fallback.computed && !fallback.use_fast_emd);
  assert(close_to(fallback.distance, fast.distance));

  options.distance = {};
  const auto default_distance = evaluate_emd(left, right, options);
  assert(default_distance.computed &&
         close_to(default_distance.distance, 0.5));

  StochasticLanguage unequal{{{{"A"}, 2.0}}, 2.0};
  options.use_fast_emd = true;
  options.normalize_masses = true;
  const auto normalized = evaluate_emd(unequal, right, options);
  assert(normalized.computed && normalized.normalized);

  options.use_fast_emd = false;
  const auto rejected = evaluate_emd(unequal, right, options);
  assert(!rejected.computed && rejected.error == "masses must be equal");

  EmdOptions invalid;
  invalid.variant = static_cast<EmdVariant>(999);
  const auto invalid_result = evaluate_emd(left, right, invalid);
  assert(!invalid_result.computed &&
         invalid_result.error == "unsupported EMD variant");
}

void process_tree_evaluation_overloads_contract() {
  const ProcessTree tree(
      {"A", "B"},
      ProcessTreeNode{ProcessTreeOperator::kSequence, 0,
                      {ProcessTreeNode::leaf(0), ProcessTreeNode::leaf(1)}});
  const auto events = log({{"A", "B"}});
  const auto fitness = evaluate_replay_fitness(
      events, tree, ReplayFitnessVariant::TOKEN_BASED);
  const auto precision = evaluate_precision(
      events, tree, PrecisionVariant::ETCONFORMANCE_TOKEN);
  const auto generalization = evaluate_generalization(events, tree);
  const auto simplicity = evaluate_simplicity(
      tree, SimplicityVariant::ARC_DEGREE);
  const auto composite = evaluate_composite(events, tree);
  assert(fitness.computed && close_to(fitness.fitness, 1.0));
  assert(precision.computed && close_to(precision.precision, 1.0));
  assert(generalization.computed && generalization.transition_count != 0);
  assert(simplicity.computed && simplicity.vertices != 0);
  assert(composite.computed && close_to(composite.fitness, 1.0));
}

}  // namespace

int main() {
  replay_fitness_token_contract();
  replay_fitness_backwards_token_contract();
  replay_fitness_alignment_contract();
  precision_etconformance_token_contract();
  precision_align_etconformance_contract();
  precision_automaton_after_align_contract();
  precision_initial_prefix_contract();
  precision_excludes_unfit_prefix_contract();
  precision_task_ids_contract();
  precision_auto_dispatch_contract();
  custom_activity_dispatch_contract();
  marked_net_dispatch_contract();
  invalid_evaluator_selector_contract();
  generalization_token_contract();
  dfg_precision_default_contract();
  simplicity_arc_degree_contract();
  simplicity_extended_cardoso_contract();
  simplicity_extended_cyclomatic_contract();
  composite_default_contract();
  emd_pyemd_contract();
  process_tree_evaluation_overloads_contract();
  return 0;
}
