#include "minera/dfg_precision.hpp"
#include "minera/evaluation.hpp"
#include "minera/json_export.hpp"
#include "minera/pnml.hpp"
#include "../tools/pnml_model_options.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <limits>
#include <optional>
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
      event.activity_state = FirstClassAttributeState::kPresent;
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
  // Unknown labels affect the boolean policy only, never numeric token counts.
  assert(penalized.fitness == with_unknown.fitness);
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
  assert(!result.alignment_detail_present);
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
  assert(result.alignment_detail_present);
  assert(close_to(result.precision, 1.0));
}

// After A, E needs the silent transition that also feeds a join, while G's
// loop has its own silent transition. The pinned evaluator searches silent
// transitions in name order and handles each transition once per marking,
// moving waiting entries to the newest marking: when G's silent transition
// sorts first ("s12" before "s9"), E is never reported as available.
PnmlDocument silent_name_order_net(const std::string& to_join, const std::string& to_loop) {
  const auto place = [](const std::string& id) { return "<place id=\"" + id + "\"/>"; };
  const auto visible = [](const std::string& id) {
    return "<transition id=\"" + id + "\"><name><text>" + id + "</text></name></transition>";
  };
  const auto silent = [](const std::string& id) {
    return "<transition id=\"" + id + "\"><name><text>" + id +
           "</text></name><toolspecific tool=\"ProM\" version=\"6.4\" "
           "activity=\"$invisible$\" localNodeID=\"" + id + "\"/></transition>";
  };
  std::string arcs;
  std::size_t index = 0;
  for (const auto& [source, target] : std::vector<std::pair<std::string, std::string>>{
           {"i", "A"}, {"A", "p11"}, {"A", "p16"}, {"A", "p7"}, {"p11", to_join},
           {to_join, "p9"}, {"p16", to_loop}, {to_loop, "p15"}, {"p15", "G"}, {"G", "p16"},
           {"p16", "join"}, {"p7", "join"}, {"p9", "join"}, {"join", "p5"}, {"p5", "go"},
           {"go", "p18"}, {"p18", "E"}, {"E", "o"}})
    arcs += "<arc id=\"a" + std::to_string(index++) + "\" source=\"" + source +
            "\" target=\"" + target + "\"/>";
  std::string places = "<place id=\"i\"><initialMarking><text>1</text></initialMarking></place>";
  for (const auto* id : {"p11", "p16", "p7", "p9", "p15", "p5", "p18", "o"}) places += place(id);
  PnmlImportOptions import;
  import.public_marking_semantics = true;
  return parse_pnml_document(
      "<pnml><net id=\"order\"><page id=\"page\">" + places + visible("A") + visible("G") +
          visible("E") + silent(to_join) + silent(to_loop) + silent("join") + silent("go") +
          arcs + "</page><finalmarkings><marking><place idref=\"o\"><text>1</text></place>"
          "</marking></finalmarkings></net></pnml>",
      import);
}

void precision_silent_search_name_order_contract() {
  const auto blocked = silent_name_order_net("s9", "s12");
  const auto reached = silent_name_order_net("sa", "sz");
  const auto align = PrecisionVariant::ALIGN_ETCONFORMANCE;
  const auto automaton = PrecisionVariant::AUTOMATON_AFTER_ALIGN;
  assert(close_to(evaluate_precision(log({{"A", "E"}}), blocked, align).precision, 0.5));
  assert(close_to(evaluate_precision(log({{"A", "E"}}), reached, align).precision, 2.0 / 3.0));
  assert(close_to(evaluate_precision(log({{"A", "E"}}), blocked, automaton).precision, 0.5));
  assert(close_to(evaluate_precision(log({{"A", "E"}}), reached, automaton).precision,
                  2.0 / 3.0));
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

void simplicity_imported_arc_occurrences_contract() {
  const auto document = parse_pnml_document(R"(<pnml><net id="parallel">
    <place id="start"><initialMarking><text>3</text></initialMarking></place>
    <place id="finish"/><transition id="A"><name><text>A</text></name></transition>
    <arc id="one" source="start" target="A"/>
    <arc id="two" source="start" target="A"><inscription><text>2</text></inscription></arc>
    <arc id="out" source="A" target="finish"/>
    <finalmarkings><marking><place idref="finish"><text>1</text></place></marking></finalmarkings>
  </net></pnml>)");
  SimplicityOptions options;
  options.arc_degree_k = 0.0;
  const auto parallel = evaluate_simplicity(document, SimplicityVariant::ARC_DEGREE, options);
  assert(parallel.computed && parallel.edges == 3 && parallel.vertices == 3);
  assert(parallel.value == 1.0 / 3.0);
  const auto executable = evaluate_simplicity(document.net, SimplicityVariant::ARC_DEGREE, options);
  assert(executable.value == 1.0 / (1.0 + 4.0 / 3.0));
  const auto restored = parse_pnml_document(to_pnml(document));
  assert(evaluate_simplicity(restored, SimplicityVariant::ARC_DEGREE, options).value == parallel.value);
  const auto cardoso = evaluate_simplicity(document, SimplicityVariant::EXTENDED_CARDOSO);
  assert(cardoso.computed && cardoso.value == 1.0 && cardoso.edges == 3);
  options.initial_marking = {3, 0};
  options.final_marking = {0, 1};
  const auto cyclomatic = evaluate_simplicity(document, SimplicityVariant::EXTENDED_CYCLOMATIC, options);
  const auto native_cyclomatic = evaluate_simplicity(document.net, SimplicityVariant::EXTENDED_CYCLOMATIC, options);
  assert(cyclomatic.computed == native_cyclomatic.computed && cyclomatic.value == native_cyclomatic.value);
  auto stale = document;
  stale.net.add_arc(0, 0);
  const auto rejected = evaluate_simplicity(stale, SimplicityVariant::ARC_DEGREE, options);
  assert(!rejected.computed && !rejected.error.empty());
  stale.arc_occurrences.reset();
  assert(evaluate_simplicity(stale, SimplicityVariant::ARC_DEGREE, options).computed);
}

void simplicity_presented_arc_occurrences_contract() {
  PetriNetPresentation net;
  // Node identities are local to each kind; names do not identify endpoints.
  net.places = {{7, "same", {}}, {19, "same", {}}, {29, "same", {}}};
  net.transitions = {{7, "same", "A", {}}, {37, "same", std::nullopt, {}}};
  const auto p = [](std::size_t id) { return PetriNodeRef{PetriNodeKind::kPlace, id}; };
  const auto t = [](std::size_t id) { return PetriNodeRef{PetriNodeKind::kTransition, id}; };
  net.arcs = {{p(7), t(7), 1, {}}, {p(7), t(37), 1, {}},
              {t(7), p(19), 1, {}}, {t(7), p(29), 2, {}},
              {t(37), p(19), 3, {}}, {t(37), p(29), 1, {}}};
  const auto same = evaluate_simplicity(net, SimplicityVariant::EXTENDED_CARDOSO);
  assert(same.computed && same.value == 1.0 && same.edges == 6);
  net.arcs.pop_back();
  const auto different = evaluate_simplicity(net, SimplicityVariant::EXTENDED_CARDOSO);
  assert(different.computed && different.value == 2.0);

  net.places.resize(2);
  net.transitions.resize(1);
  net.arcs = {{p(7), t(7), 1, {}}, {p(7), t(7), 2, {}},
              {t(7), p(19), 1, {}}};
  SimplicityOptions options;
  options.arc_degree_k = 0.0;
  const auto parallel = evaluate_simplicity(net, SimplicityVariant::ARC_DEGREE, options);
  assert(parallel.computed && parallel.vertices == 3 && parallel.edges == 3);
  assert(close_to(parallel.value, 1.0 / 3.0));
  const auto cardoso = evaluate_simplicity(net, SimplicityVariant::EXTENDED_CARDOSO);
  assert(cardoso.computed && cardoso.value == 1.0);
  net.arcs[1].weight = 100;
  assert(evaluate_simplicity(net, SimplicityVariant::ARC_DEGREE, options).value == parallel.value);
  net.arcs[1].target.id = 999;
  assert(!evaluate_simplicity(net).computed);
  net.arcs[1].target = p(19);
  assert(!evaluate_simplicity(net).computed);
  net.arcs.clear();
  net.places[1].id = 7;
  assert(!evaluate_simplicity(net).computed);
  const auto empty = evaluate_simplicity(PetriNetPresentation{}, SimplicityVariant::EXTENDED_CYCLOMATIC);
  assert(empty.computed && empty.vertices == 1 && empty.value == 0.0);
}

// Independent public outcomes: retained pinned 2.7.22 capture has 982 arc-degree
// calls (628 direct, 354 dispatcher), projected to these 120 degree/k-selector
// records (102 effective numeric states). Hex strings are captured outputs.
void arc_degree_module_acceptance() {
  const auto nan = std::numeric_limits<double>::quiet_NaN();
  const auto infinity = std::numeric_limits<double>::infinity();
  struct PublicCase { std::size_t vertices, arcs; double k; const char* hex; };
  const PublicCase public_cases[] = {
    {4, 0, 2.0, "0x1.0000000000000p+0"}, // default
    {4, 0, 0.0, "0x1.0000000000000p+0"}, // zero
    {4, 1, 2.0, "0x1.0000000000000p+0"}, // default
    {4, 1, 0.0, "0x1.5555555555555p-1"}, // zero
    {4, 2, 2.0, "0x1.0000000000000p+0"}, // default
    {4, 2, 0.0, "0x1.0000000000000p-1"}, // zero
    {4, 3, 2.0, "0x1.0000000000000p+0"}, // default
    {4, 3, 0.0, "0x1.999999999999ap-2"}, // zero
    {4, 4, 2.0, "0x1.0000000000000p+0"}, // default
    {4, 4, 0.0, "0x1.5555555555555p-2"}, // zero
    {4, 5, 2.0, "0x1.5555555555555p-1"}, // default
    {4, 5, 0.0, "0x1.2492492492492p-2"}, // zero
    {4, 6, 2.0, "0x1.0000000000000p-1"}, // default
    {4, 6, 0.0, "0x1.0000000000000p-2"}, // zero
    {4, 7, 2.0, "0x1.999999999999ap-2"}, // default
    {4, 7, 0.0, "0x1.c71c71c71c71cp-3"}, // zero
    {4, 8, 2.0, "0x1.5555555555555p-2"}, // default
    {4, 8, 0.0, "0x1.999999999999ap-3"}, // zero
    {0, 0, 2.0, "0x1.0000000000000p+0"}, // default
    {0, 0, 0.0, "0x1.0000000000000p+0"}, // zero
    {3, 0, 2.0, "0x1.0000000000000p+0"}, // default
    {3, 0, 0.0, "0x1.0000000000000p+0"}, // zero
    {3, 2, 2.0, "0x1.0000000000000p+0"}, // default
    {3, 2, 0.0, "0x1.b6db6db6db6dcp-2"}, // zero
    {3, 3, 2.0, "0x1.0000000000000p+0"}, // default
    {3, 3, 0.0, "0x1.5555555555555p-2"}, // zero
    {5, 6, 2.0, "0x1.6db6db6db6db7p-1"}, // default
    {5, 6, 0.0, "0x1.2d2d2d2d2d2d3p-2"}, // zero
    {5, 4, 2.0, "0x1.0000000000000p+0"}, // default
    {5, 4, 0.0, "0x1.89d89d89d89d8p-2"}, // zero
    {4, 0, -1.0, "0x1.0000000000000p-1"}, // negative
    {4, 0, -0.0, "0x1.0000000000000p+0"}, // negative_zero
    {4, 0, 0.25, "0x1.0000000000000p+0"}, // quarter
    {4, 0, 1.0, "0x1.0000000000000p+0"}, // one
    {4, 0, 2.0, "0x1.0000000000000p+0"}, // two
    {4, 0, 10.0, "0x1.0000000000000p+0"}, // ten
    {4, 0, nan, "nan"}, // nan
    {4, 0, infinity, "0x1.0000000000000p+0"}, // positive_infinity
    {4, 0, -infinity, "0x0.0p+0"}, // negative_infinity
    {4, 0, 0.0, "0x1.0000000000000p+0"}, // enum_zero
    {4, 4, -1.0, "0x1.0000000000000p-2"}, // negative
    {4, 4, -0.0, "0x1.5555555555555p-2"}, // negative_zero
    {4, 4, 0.25, "0x1.745d1745d1746p-2"}, // quarter
    {4, 4, 1.0, "0x1.0000000000000p-1"}, // one
    {4, 4, 2.0, "0x1.0000000000000p+0"}, // two
    {4, 4, 10.0, "0x1.0000000000000p+0"}, // ten
    {4, 4, nan, "nan"}, // nan
    {4, 4, infinity, "0x1.0000000000000p+0"}, // positive_infinity
    {4, 4, -infinity, "0x0.0p+0"}, // negative_infinity
    {4, 4, 0.0, "0x1.5555555555555p-2"}, // enum_zero
    {4, 8, -1.0, "0x1.5555555555555p-3"}, // negative
    {4, 8, -0.0, "0x1.999999999999ap-3"}, // negative_zero
    {4, 8, 0.25, "0x1.af286bca1af28p-3"}, // quarter
    {4, 8, 1.0, "0x1.0000000000000p-2"}, // one
    {4, 8, 2.0, "0x1.5555555555555p-2"}, // two
    {4, 8, 10.0, "0x1.0000000000000p+0"}, // ten
    {4, 8, nan, "nan"}, // nan
    {4, 8, infinity, "0x1.0000000000000p+0"}, // positive_infinity
    {4, 8, -infinity, "0x0.0p+0"}, // negative_infinity
    {4, 8, 0.0, "0x1.999999999999ap-3"}, // enum_zero
    {0, 0, -1.0, "0x1.0000000000000p-1"}, // negative
    {0, 0, -0.0, "0x1.0000000000000p+0"}, // negative_zero
    {0, 0, 0.25, "0x1.0000000000000p+0"}, // quarter
    {0, 0, 1.0, "0x1.0000000000000p+0"}, // one
    {0, 0, 2.0, "0x1.0000000000000p+0"}, // two
    {0, 0, 10.0, "0x1.0000000000000p+0"}, // ten
    {0, 0, nan, "nan"}, // nan
    {0, 0, infinity, "0x1.0000000000000p+0"}, // positive_infinity
    {0, 0, -infinity, "0x0.0p+0"}, // negative_infinity
    {0, 0, 0.0, "0x1.0000000000000p+0"}, // enum_zero
    {3, 0, -1.0, "0x1.0000000000000p-1"}, // negative
    {3, 0, -0.0, "0x1.0000000000000p+0"}, // negative_zero
    {3, 0, 0.25, "0x1.0000000000000p+0"}, // quarter
    {3, 0, 1.0, "0x1.0000000000000p+0"}, // one
    {3, 0, 2.0, "0x1.0000000000000p+0"}, // two
    {3, 0, 10.0, "0x1.0000000000000p+0"}, // ten
    {3, 0, nan, "nan"}, // nan
    {3, 0, infinity, "0x1.0000000000000p+0"}, // positive_infinity
    {3, 0, -infinity, "0x0.0p+0"}, // negative_infinity
    {3, 0, 0.0, "0x1.0000000000000p+0"}, // enum_zero
    {3, 2, -1.0, "0x1.3333333333334p-2"}, // negative
    {3, 2, -0.0, "0x1.b6db6db6db6dcp-2"}, // negative_zero
    {3, 2, 0.25, "0x1.eb851eb851ebap-2"}, // quarter
    {3, 2, 1.0, "0x1.8000000000000p-1"}, // one
    {3, 2, 2.0, "0x1.0000000000000p+0"}, // two
    {3, 2, 10.0, "0x1.0000000000000p+0"}, // ten
    {3, 2, nan, "nan"}, // nan
    {3, 2, infinity, "0x1.0000000000000p+0"}, // positive_infinity
    {3, 2, -infinity, "0x0.0p+0"}, // negative_infinity
    {3, 2, 0.0, "0x1.b6db6db6db6dcp-2"}, // enum_zero
    {3, 3, -1.0, "0x1.0000000000000p-2"}, // negative
    {3, 3, -0.0, "0x1.5555555555555p-2"}, // negative_zero
    {3, 3, 0.25, "0x1.745d1745d1746p-2"}, // quarter
    {3, 3, 1.0, "0x1.0000000000000p-1"}, // one
    {3, 3, 2.0, "0x1.0000000000000p+0"}, // two
    {3, 3, 10.0, "0x1.0000000000000p+0"}, // ten
    {3, 3, nan, "nan"}, // nan
    {3, 3, infinity, "0x1.0000000000000p+0"}, // positive_infinity
    {3, 3, -infinity, "0x0.0p+0"}, // negative_infinity
    {3, 3, 0.0, "0x1.5555555555555p-2"}, // enum_zero
    {5, 6, -1.0, "0x1.d1745d1745d17p-3"}, // negative
    {5, 6, -0.0, "0x1.2d2d2d2d2d2d3p-2"}, // negative_zero
    {5, 6, 0.25, "0x1.4514514514514p-2"}, // quarter
    {5, 6, 1.0, "0x1.aaaaaaaaaaaabp-2"}, // one
    {5, 6, 2.0, "0x1.6db6db6db6db7p-1"}, // two
    {5, 6, 10.0, "0x1.0000000000000p+0"}, // ten
    {5, 6, nan, "nan"}, // nan
    {5, 6, infinity, "0x1.0000000000000p+0"}, // positive_infinity
    {5, 6, -infinity, "0x0.0p+0"}, // negative_infinity
    {5, 6, 0.0, "0x1.2d2d2d2d2d2d3p-2"}, // enum_zero
    {5, 4, -1.0, "0x1.1c71c71c71c72p-2"}, // negative
    {5, 4, -0.0, "0x1.89d89d89d89d8p-2"}, // negative_zero
    {5, 4, 0.25, "0x1.b3bea3677d46dp-2"}, // quarter
    {5, 4, 1.0, "0x1.4000000000000p-1"}, // one
    {5, 4, 2.0, "0x1.0000000000000p+0"}, // two
    {5, 4, 10.0, "0x1.0000000000000p+0"}, // ten
    {5, 4, nan, "nan"}, // nan
    {5, 4, infinity, "0x1.0000000000000p+0"}, // positive_infinity
    {5, 4, -infinity, "0x0.0p+0"}, // negative_infinity
    {5, 4, 0.0, "0x1.89d89d89d89d8p-2"}, // enum_zero
  };
  assert(std::size(public_cases) == 120);
  for (const auto& expected : public_cases) {
    PetriNetPresentation graph;
    graph.name = std::string("raw\0\xff", 5);
    // Equal names and the same ID in different node-kind namespaces do not
    // merge identities. Topology/weights cannot change total incident degree.
    const auto places = expected.vertices / 2;
    const auto transitions = expected.vertices - places;
    for (std::size_t i = 0; i < places; ++i)
      graph.places.push_back({i, "same", {}});
    for (std::size_t i = 0; i < transitions; ++i)
      graph.transitions.push_back({i, "same", std::nullopt, {}});
    for (std::size_t i = 0; i < expected.arcs; ++i) {
      const PetriNodeRef p{PetriNodeKind::kPlace, i % places};
      const PetriNodeRef t{PetriNodeKind::kTransition, i % transitions};
      graph.arcs.push_back({i % 2 ? t : p, i % 2 ? p : t,
                           TokenInteger(i + 1), {}});
    }
    const auto before = petri_net_presentation_json(graph);
    SimplicityOptions options;
    options.arc_degree_k = expected.k;
    // These controls belong to other simplicity variants and must be inactive.
    options.initial_marking = {-7}; options.final_marking = {-9};
    options.max_states = 0;
    const auto result = evaluate_simplicity(graph, SimplicityVariant::ARC_DEGREE, options);
    assert(result.computed && !result.exhausted && result.error.empty());
    assert(result.vertices == expected.vertices && result.edges == expected.arcs);
    assert(simplicity_arc_degree_result_json(result) ==
           std::string("{\"type\":\"float\",\"hex\":\"") + expected.hex + "\"}");
    assert(petri_net_presentation_json(graph) == before);
    assert(std::isnan(expected.k) ? std::isnan(options.arc_degree_k)
           : options.arc_degree_k == expected.k &&
             std::signbit(options.arc_degree_k) == std::signbit(expected.k));
    assert(options.initial_marking == std::vector<int>{-7} &&
           options.final_marking == std::vector<int>{-9} && options.max_states == 0);
    // Default invocation is an actual public overload, not a rewritten k value.
    if (expected.k == 2.0) {
      const auto defaults = evaluate_simplicity(graph);
      assert(simplicity_arc_degree_result_json(defaults) ==
             simplicity_arc_degree_result_json(result));
    }
    std::reverse(graph.arcs.begin(), graph.arcs.end());
    for (auto& arc : graph.arcs) { arc.weight = 0; arc.negative_weight = true; }
    const auto reordered = evaluate_simplicity(graph, SimplicityVariant::ARC_DEGREE, options);
    assert(simplicity_arc_degree_result_json(reordered) ==
           simplicity_arc_degree_result_json(result));
  }
  // Native doubles outside the retained k sample follow the same arithmetic
  // domain; this supplements, and is not described as, a public capture.
  PetriNetPresentation empty;
  for (double k : {std::numeric_limits<double>::max(),
                   -std::numeric_limits<double>::max(),
                   std::numeric_limits<double>::denorm_min(),
                   -std::numeric_limits<double>::denorm_min()}) {
    SimplicityOptions options; options.arc_degree_k = k;
    const auto result = evaluate_simplicity(empty, SimplicityVariant::ARC_DEGREE, options);
    assert(result.computed && std::isfinite(result.value));
    assert(!simplicity_arc_degree_result_json(result).empty());
  }
  // Full imported arc objects use the same occurrence-preserving public view;
  // the executable grouped carrier deliberately has a different arc inventory.
  simplicity_imported_arc_occurrences_contract();
}

void simplicity_nonfinite_baseline_contract() {
  for (const bool empty : {false, true}) {
    PetriNetPresentation presentation;
    if (!empty) {
      presentation.places = {{0, "p", {}}};
      presentation.transitions = {{0, "t", "A", {}}};
      presentation.arcs = {{{PetriNodeKind::kPlace, 0},
                             {PetriNodeKind::kTransition, 0}, 1, {}}};
    }
    const auto executable = empty ? PetriNet{} : sequence_net();
    for (const double k : {std::numeric_limits<double>::quiet_NaN(),
                           std::numeric_limits<double>::infinity(),
                           -std::numeric_limits<double>::infinity()}) {
      SimplicityOptions options;
      options.arc_degree_k = k;
      const auto graph = evaluate_simplicity(presentation, SimplicityVariant::ARC_DEGREE, options);
      const auto net = evaluate_simplicity(executable, SimplicityVariant::ARC_DEGREE, options);
      assert(graph.computed && net.computed);
      if (std::isnan(k)) assert(std::isnan(graph.value) && std::isnan(net.value));
      else assert(graph.value == (k > 0 ? 1.0 : 0.0) && net.value == graph.value);
      assert(std::isnan(k) ? std::isnan(options.arc_degree_k) : options.arc_degree_k == k);
    }
  }
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

void simplicity_reachable_transition_occurrences_contract() {
  PetriNet parallel;
  parallel.add_place(); parallel.add_place();
  parallel.set_activities({"A"});
  parallel.add_transition(0, {0}, {1});
  parallel.add_transition(std::nullopt, {0}, {1});
  const auto alternatives = evaluate_simplicity(parallel, SimplicityVariant::EXTENDED_CYCLOMATIC);
  assert(alternatives.computed && alternatives.vertices == 2 && alternatives.edges == 2);
  assert(alternatives.strongly_connected_components == 2 && alternatives.value == 2.0);

  PetriNet cycle;
  cycle.add_place(); cycle.add_place();
  cycle.set_activities({"A"});
  cycle.add_transition(0, {0}, {1});
  cycle.add_transition(std::nullopt, {1}, {0});
  SimplicityOptions options;
  options.initial_marking = {1, 0};
  const auto one_token = evaluate_simplicity(cycle, SimplicityVariant::EXTENDED_CYCLOMATIC, options);
  assert(one_token.computed && one_token.vertices == 2 && one_token.edges == 2);
  assert(one_token.strongly_connected_components == 1 && one_token.value == 2.0);
  options.initial_marking = {2, 0};
  const auto two_tokens = evaluate_simplicity(cycle, SimplicityVariant::EXTENDED_CYCLOMATIC, options);
  assert(two_tokens.computed && two_tokens.vertices == 3 && two_tokens.edges == 4);
  assert(two_tokens.strongly_connected_components == 1 && two_tokens.value == 4.0);
  PnmlDocument document;
  document.net = cycle;
  assert(evaluate_simplicity(document, SimplicityVariant::EXTENDED_CYCLOMATIC, options).value == 4.0);

  PetriNet isolated;
  isolated.set_activities({"A"});
  isolated.add_transition(0, {}, {});
  isolated.add_transition(std::nullopt, {}, {});
  const auto loops = evaluate_simplicity(isolated, SimplicityVariant::EXTENDED_CYCLOMATIC);
  assert(loops.computed && loops.vertices == 1 && loops.edges == 2 && loops.value == 2.0);
}

void simplicity_presented_reachability_contract() {
  const auto p = [](std::size_t id) { return PetriNodeRef{PetriNodeKind::kPlace, id}; };
  const auto t = [](std::size_t id) { return PetriNodeRef{PetriNodeKind::kTransition, id}; };
  PetriNetPresentation parallel;
  parallel.places = {{7, "source", {}}, {19, "sink", {}}};
  parallel.transitions = {{37, "move", "A", {}}};
  parallel.arcs = {{p(7), t(37), 1, {}}, {p(7), t(37), 1, {}}, {t(37), p(19), 1, {}}};
  SimplicityOptions options;
  options.initial_marking = {1, 0};
  const auto one = evaluate_simplicity(parallel, SimplicityVariant::EXTENDED_CYCLOMATIC, options);
  // Each input occurrence enables against the original token, then both fire.
  assert(one.computed && one.vertices == 2 && one.edges == 1 && one.value == 1.0);
  options.initial_marking = {3, 0};
  const auto three = evaluate_simplicity(parallel, SimplicityVariant::EXTENDED_CYCLOMATIC, options);
  assert(three.computed && three.vertices == 3 && three.edges == 2 && three.value == 2.0);
  options.presentation_initial_marking = std::vector<PetriSparseMarkingEntry>{{19, 0}, {7, 3}};
  assert(evaluate_simplicity(parallel, SimplicityVariant::EXTENDED_CYCLOMATIC, options).value == 2.0);
  options.presentation_initial_marking = std::vector<PetriSparseMarkingEntry>{};
  assert(evaluate_simplicity(parallel, SimplicityVariant::EXTENDED_CYCLOMATIC, options).value == 0.0);
  options.presentation_initial_marking.reset();
  options.max_states = 2;
  const auto capped = evaluate_simplicity(parallel, SimplicityVariant::EXTENDED_CYCLOMATIC, options);
  assert(!capped.computed && capped.exhausted);

  options = {};
  options.initial_marking = {1, 0};
  auto consume = parallel;
  consume.arcs.pop_back();
  const auto depleted = evaluate_simplicity(consume, SimplicityVariant::EXTENDED_CYCLOMATIC, options);
  assert(depleted.computed && depleted.vertices == 2 && depleted.edges == 1 &&
         depleted.strongly_connected_components == 2 && depleted.value == 1.0);
  consume.arcs.push_back({t(37), p(7), 1, {}});
  const auto replenished = evaluate_simplicity(consume, SimplicityVariant::EXTENDED_CYCLOMATIC, options);
  assert(replenished.computed && replenished.vertices == 1 && replenished.edges == 1 && replenished.value == 1.0);
  consume.arcs.back().weight = 2;
  const auto restored = evaluate_simplicity(consume, SimplicityVariant::EXTENDED_CYCLOMATIC, options);
  assert(restored.computed && restored.vertices == 2 && restored.edges == 2 &&
         restored.strongly_connected_components == 2 && restored.value == 2.0);

  PetriNetPresentation branch;
  branch.places = {{7, "p0", {}}, {19, "p1", {}}, {29, "p2", {}}};
  branch.transitions = {{37, "same", "A", {}}, {41, "same", std::nullopt, {}}};
  branch.arcs = {{p(7), t(37), 1, {}}, {t(37), p(19), 1, {}},
                 {p(7), t(41), 1, {}}, {t(41), p(29), 1, {}}};
  const auto names = evaluate_simplicity(branch, SimplicityVariant::EXTENDED_CYCLOMATIC);
  assert(names.computed && names.vertices == 3 && names.edges == 2 && names.value == 1.0);
  branch.places[0].name = "p-0";
  branch.places[1].name = "p0";
  branch.places[2].name = "p_0";
  const auto aliases = evaluate_simplicity(branch, SimplicityVariant::EXTENDED_CYCLOMATIC);
  assert(aliases.computed && aliases.vertices == 2 && aliases.edges == 2 && aliases.value == 1.0);

  const auto document = parse_pnml_document(R"(<pnml><net id="parallel-input">
    <place id="source"/><place id="sink"/><transition id="move"/>
    <arc id="a" source="source" target="move"/><arc id="b" source="source" target="move"/>
    <arc id="c" source="move" target="sink"/>
  </net></pnml>)");
  options = {};
  options.initial_marking = {1, 0};
  assert(evaluate_simplicity(document, SimplicityVariant::EXTENDED_CYCLOMATIC, options).value == 1.0);
  // The imported net keeps both parallel arcs, so it matches the document.
  assert(evaluate_simplicity(document.net, SimplicityVariant::EXTENDED_CYCLOMATIC, options).value == 1.0);
  auto stale = document;
  stale.net.add_arc(0, 0);
  assert(!evaluate_simplicity(stale, SimplicityVariant::EXTENDED_CYCLOMATIC, options).computed);
}

void simplicity_extended_document_cyclomatic_contract() {
  for (const auto type : {"reset", "inhibitor"}) {
    const auto document = parse_pnml_document(
      "<pnml><net id=\"typed\"><place id=\"source\"><initialMarking><text>1</text></initialMarking></place>"
      "<place id=\"gate\"/><place id=\"sink\"/><transition id=\"move\"/>"
      "<arc id=\"guard\" source=\"source\" target=\"move\"/>"
      "<arc id=\"typed\" source=\"gate\" target=\"move\"><arctype><text>" + std::string(type) + "</text></arctype></arc>"
      "<arc id=\"out\" source=\"move\" target=\"sink\"/>"
      "<finalmarkings><marking><place idref=\"sink\"><text>1</text></place></marking></finalmarkings></net></pnml>");
    SimplicityOptions options;
    options.initial_marking = {1, 0, 0};
    const auto blocked = evaluate_simplicity(document, SimplicityVariant::EXTENDED_CYCLOMATIC, options);
    assert(blocked.computed && blocked.vertices == 1 && blocked.value == 0.0);
    assert(evaluate_simplicity(document.net, SimplicityVariant::EXTENDED_CYCLOMATIC, options).value == 0.0);
    options.initial_marking = {2, 1, 0};
    const auto once = evaluate_simplicity(document, SimplicityVariant::EXTENDED_CYCLOMATIC, options);
    assert(once.computed && once.vertices == 2 && once.edges == 1 && once.value == 1.0);
    const auto executable_once = evaluate_simplicity(document.net, SimplicityVariant::EXTENDED_CYCLOMATIC, options);
    assert(executable_once.computed && executable_once.vertices == 2 && executable_once.edges == 1 && executable_once.value == 1.0);
    auto stale = document;
    stale.net.add_output_arc(0, 2);
    assert(!evaluate_simplicity(stale, SimplicityVariant::EXTENDED_CYCLOMATIC, options).computed);
  }
}

void simplicity_signed_weighted_presentation_contract() {
  PetriNetPresentation net;
  net.places = {{0,"source",{}},{1,"gate",{}},{2,"sink",{}}};
  net.transitions = {{0,"move",std::string("A"),{}}};
  net.arcs = {{{PetriNodeKind::kPlace,0},{PetriNodeKind::kTransition,0},1,{}},
              {{PetriNodeKind::kPlace,1},{PetriNodeKind::kTransition,0},2,{}},
              {{PetriNodeKind::kTransition,0},{PetriNodeKind::kPlace,2},1,{}}};
  SimplicityOptions options;
  options.presentation_initial_marking = std::vector<PetriSparseMarkingEntry>{{0,2},{1,3}};
  const auto weighted = evaluate_simplicity(net,SimplicityVariant::EXTENDED_CYCLOMATIC,options);
  assert(weighted.computed && weighted.vertices == 2 && weighted.value == 1.0);
  net.arcs[1].weight = 1; net.arcs[1].negative_weight = true;
  const auto signed_input = evaluate_simplicity(net,SimplicityVariant::EXTENDED_CYCLOMATIC,options);
  assert(signed_input.computed && signed_input.vertices == 3 && signed_input.value == 2.0);
  net.arcs[1].negative_weight = false; net.arcs[1].weight = 0;
  assert(evaluate_simplicity(net,SimplicityVariant::EXTENDED_CYCLOMATIC,options).value == 2.0);
  net.arcs[1].weight = std::numeric_limits<std::uint64_t>::max();
  const auto blocked = evaluate_simplicity(net,SimplicityVariant::EXTENDED_CYCLOMATIC,options);
  assert(blocked.computed && blocked.vertices == 1 && blocked.value == 0.0);
  net.arcs[1].weight = 1; net.arcs[2].negative_weight = true;
  const auto signed_output = evaluate_simplicity(net,SimplicityVariant::EXTENDED_CYCLOMATIC,options);
  assert(signed_output.computed && signed_output.vertices == 3 && signed_output.value == 2.0);
  net.arcs[2].negative_weight = false; net.arcs[2].weight = 2147483648ull;
  const auto large_output = evaluate_simplicity(net,SimplicityVariant::EXTENDED_CYCLOMATIC,options);
  assert(large_output.computed && large_output.vertices == 3 && large_output.value == 2.0);
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
  assert(close_to(result.fitness_score, result.fitness));
  assert(result.fitness_details.computed);
  assert(close_to(result.fitness_details.log_fitness, fitness.log_fitness));
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

void composite_document_and_weight_contract() {
  const auto document = parse_pnml_document(R"(<pnml><net id="parallel">
    <place id="start"><initialMarking><text>4</text></initialMarking></place>
    <place id="finish"/><transition id="A"><name><text>A</text></name></transition>
    <arc id="one" source="start" target="A"/>
    <arc id="two" source="start" target="A"><inscription><text>2</text></inscription></arc>
    <arc id="three" source="start" target="A"/>
    <arc id="out1" source="A" target="finish"><inscription><text>2</text></inscription></arc>
    <arc id="out2" source="A" target="finish"/>
    <finalmarkings><marking><place idref="finish"><text>3</text></place></marking></finalmarkings>
  </net></pnml>)");
  const auto events = log({{"A"}, {"A"}, {"A"}, {"A"}, {"A"}, {"A"}, {"A"}, {"A"}, {"A"}});
  const auto result = evaluate_composite(events, document);
  assert(result.computed && result.fitness == 1.0 && result.precision == 1.0);
  assert(result.simplicity == 3.0 / 7.0 && result.generalization == 0.6666666666666667);
  assert(result.metrics_average_weight == 65.0 / 84.0);
  assert(result.fitness_details.trace_count == 9 && result.fitness_details.fitting_traces == 9);
  assert(evaluate_composite(EventDataView(events), document).metrics_average_weight == result.metrics_average_weight);
  assert(evaluate_composite(events, parse_pnml_document(to_pnml(document))).simplicity == result.simplicity);
  CompositeEvaluationOptions negative;
  negative.fitness_weight = negative.precision_weight =
      negative.simplicity_weight = negative.generalization_weight = -1.0;
  const auto normalized_negative = evaluate_composite(events, document, negative);
  assert(normalized_negative.computed && normalized_negative.fitness_weight == 0.25);
  assert(normalized_negative.metrics_average_weight == result.metrics_average_weight);
  CompositeEvaluationOptions overflow;
  overflow.fitness_weight = overflow.precision_weight =
      overflow.simplicity_weight = overflow.generalization_weight = 1e308;
  const auto normalized_overflow = evaluate_composite(events, document, overflow);
  assert(normalized_overflow.computed && normalized_overflow.fitness_weight == 0.0);
  assert(normalized_overflow.metrics_average_weight == 0.0);
  for (const double weight : {std::numeric_limits<double>::quiet_NaN(),
                              std::numeric_limits<double>::infinity(),
                              -std::numeric_limits<double>::infinity()}) {
    auto options = CompositeEvaluationOptions{};
    options.fitness_weight = weight;
    const auto nonfinite = evaluate_composite(events, document, options);
    assert(nonfinite.computed && std::isnan(nonfinite.metrics_average_weight));
  }
  CompositeEvaluationOptions zero;
  zero.fitness_weight = 1.0; zero.precision_weight = -1.0;
  zero.simplicity_weight = zero.generalization_weight = 0.0;
  assert(!evaluate_composite(events, document, zero).computed);
  auto stale = document;
  stale.net.add_arc(0, 0);
  assert(!evaluate_composite(events, stale).computed);
  CompositeEvaluationOptions explicit_marks;
  explicit_marks.initial_marking = {4, 0};
  explicit_marks.final_marking = {0, 3};
  stale = document;
  stale.initial_marking = {std::numeric_limits<std::int64_t>::max(), 0};
  assert(evaluate_composite(events, stale).computed);
  assert(evaluate_composite(events, stale, explicit_marks).fitness == 1.0);
}

EventStream flat_evaluation_stream() {
  EventStream stream;
  for (const auto& row : {std::pair{"case-1", "A"},
                          std::pair{"case-1", "B"},
                          std::pair{"case-2", "A"},
                          std::pair{"case-2", "B"}}) {
    EventRecord record;
    record.fields.push_back({"case_id", row.first, AttributeKind::kString});
    record.fields.push_back({"phase", row.second, AttributeKind::kString});
    stream.events.push_back(std::move(record));
  }
  return stream;
}

EventTable flat_evaluation_table() {
  EventTable table;
  table.columns = {"case_id", "phase"};
  for (const auto& row : {std::pair{"case-1", "A"},
                          std::pair{"case-1", "B"},
                          std::pair{"case-2", "A"},
                          std::pair{"case-2", "B"}}) {
    EventTable::Row values;
    values.emplace_back(
        EventAttribute{"case_id", row.first, AttributeKind::kString});
    values.emplace_back(
        EventAttribute{"phase", row.second, AttributeKind::kString});
    table.rows.push_back(std::move(values));
  }
  return table;
}

LazyEventTable lazy_evaluation_table() {
  return LazyEventTable(
      {"case_id", "phase"},
      [](const LazyEventTable::RowConsumer& consume) {
        for (const auto& row : {std::pair{"case-1", "A"},
                                std::pair{"case-1", "B"},
                                std::pair{"case-2", "A"},
                                std::pair{"case-2", "B"}}) {
          LazyEventTable::Row values;
          values.emplace_back(
              EventAttribute{"case_id", row.first, AttributeKind::kString});
          values.emplace_back(
              EventAttribute{"phase", row.second, AttributeKind::kString});
          consume(values);
        }
      },
      {}, {}, 4);
}

void evaluation_event_data_adapters_contract() {
  ReplayFitnessOptions fitness_options;
  fitness_options.activity_attribute = "phase";
  fitness_options.case_id_attribute = "case_id";
  PrecisionOptions precision_options;
  precision_options.activity_attribute = "phase";
  precision_options.case_id_attribute = "case_id";
  GeneralizationOptions generalization_options;
  generalization_options.activity_attribute = "phase";
  generalization_options.case_id_attribute = "case_id";
  CompositeEvaluationOptions composite_options;
  composite_options.activity_attribute = "phase";
  composite_options.case_id_attribute = "case_id";

  const auto stream = flat_evaluation_stream();
  const auto table = flat_evaluation_table();
  const auto lazy = lazy_evaluation_table();
  const auto net = sequence_net();
  assert(evaluate_replay_fitness(stream, net,
                                 ReplayFitnessVariant::TOKEN_BASED,
                                 fitness_options)
             .fitness == 1.0);
  assert(evaluate_precision(table, net, PrecisionVariant::ETCONFORMANCE_TOKEN,
                            precision_options)
             .precision == 1.0);
  assert(evaluate_generalization(
             lazy, net, GeneralizationVariant::GENERALIZATION_TOKEN,
             generalization_options)
             .computed);
  const auto composite = evaluate_composite(lazy, net, composite_options);
  assert(composite.computed && composite.fitness == 1.0 &&
         composite.fitness_details.trace_count == 2);

  auto alternate_case = flat_evaluation_stream();
  for (std::size_t index = 0; index < alternate_case.events.size(); ++index) {
    auto& old_case = alternate_case.events[index].fields[0];
    old_case.value = "legacy-case";
    old_case.first_class_role = FirstClassAttributeRole::kCaseId;
    old_case.first_class_state = FirstClassAttributeState::kPresent;
    old_case.first_class_source_key = "legacy_case";
    alternate_case.events[index].fields.push_back(
        {"new_case", index < 2 ? "case-1" : "case-2",
         AttributeKind::kString});
    auto& activity = alternate_case.events[index].fields[1];
    activity.key = "renamed_phase";
    activity.first_class_role = FirstClassAttributeRole::kActivity;
    activity.first_class_state = FirstClassAttributeState::kPresent;
    activity.first_class_source_key = "phase";
    alternate_case.events[index].fields.push_back(
        {"concept:name", "wrong", AttributeKind::kString});
  }
  fitness_options.case_id_attribute = "new_case";
  const auto alternate = evaluate_replay_fitness(
      alternate_case, net, ReplayFitnessVariant::TOKEN_BASED,
      fitness_options);
  assert(alternate.computed && alternate.trace_count == 2 &&
         alternate.fitness == 1.0);

  auto exact_activity = flat_evaluation_stream();
  for (auto& record : exact_activity.events) {
    record.fields[1].key = "activity";
    record.fields.push_back(
        {"concept:name", "wrong", AttributeKind::kString});
  }
  auto exact_options = fitness_options;
  exact_options.activity_attribute = "activity";
  exact_options.case_id_attribute = "case_id";
  const auto exact = evaluate_replay_fitness(
      exact_activity, net, ReplayFitnessVariant::TOKEN_BASED, exact_options);
  assert(exact.computed && exact.fitness == 1.0);

  fitness_options.case_id_attribute = "case_id";
  fitness_options.max_rows_to_materialize = 3;
  bool threw = false;
  try {
    (void)evaluate_replay_fitness(lazy, net, ReplayFitnessVariant::TOKEN_BASED,
                                  fitness_options);
  } catch (const std::length_error&) {
    threw = true;
  }
  assert(threw);
  threw = false;
  try {
    const auto grouped = log({{"A", "B"}});
    ReplayFitnessOptions grouped_options;
    grouped_options.max_rows_to_materialize = 1;
    (void)evaluate_replay_fitness(grouped, net,
                                  ReplayFitnessVariant::TOKEN_BASED,
                                  grouped_options);
  } catch (const std::length_error&) {
    threw = true;
  }
  assert(threw);
}

void exact_log_fitness_marking_contract() {
  PetriNet net;
  net.set_activities({"A"});
  const auto source = net.add_place();
  (void)net.add_place();
  const auto target = net.add_place();
  net.set_source(source);net.set_sink(target);
  net.add_transition(0, {source}, {target});
  ReplayFitnessOptions options;
  const TokenInteger n("9007199254740992");
  options.exact_initial_marking = TokenMarking{n, 0, 0};
  options.exact_final_marking = TokenMarking{0, 0, n};
  options.initial_marking_keys = {true, true, false};
  options.final_marking_keys = {false, true, true};
  options.place_names = {"source", "spare", "target"};
  options.transition_names = {"move"};
  const auto rounded = evaluate_replay_fitness(log({{"A"}}), net,
      ReplayFitnessVariant::TOKEN_BASED, options);
  // Public trace fitness converts operands separately; aggregate fitness
  // divides exact totals. These distinct observed values must both survive.
  assert(rounded.computed && rounded.fitness == 0x1p-52 &&
         rounded.average_trace_fitness == 0x1p-53);
  assert(rounded.token_result.produced == n + 1 && rounded.token_result.consumed == n + 1);
  assert(rounded.token_result.transition_frequencies == std::vector<std::uint64_t>{1});
  assert(rounded.token_result_is_fitness_only && !rounded.log_fitness_is_integer_zero);
  const auto tagged = token_replay_fitness_result_json(rounded);
  assert(tagged.find("0x1.0000000000000p-52") != std::string::npos);
  assert(tagged.find("0x1.0000000000000p-53") != std::string::npos);
  const DirectlyFollowsGraph graph;
  const auto compatible = replay_fitness_json(rounded, &graph);
  assert(compatible.find("\"precision\": null") != std::string::npos &&
         compatible.find("\"generalization\": null") != std::string::npos &&
         compatible.find("\"simplicity\": null") != std::string::npos);
  options.exact_initial_marking = TokenMarking{-3, 0, 0};
  options.exact_final_marking = TokenMarking{0, 0, 0};
  const auto signed_marking = evaluate_replay_fitness(log({{"A"}}), net,
      ReplayFitnessVariant::TOKEN_BASED, options);
  assert(signed_marking.fitness == 0.0 && signed_marking.average_trace_fitness == 0.0 &&
         signed_marking.log_fitness_is_integer_zero && signed_marking.token_result.produced == -2);
  options.exact_initial_marking = TokenMarking{1, 0, 0};
  options.exact_final_marking = TokenMarking{0, 0, -3};
  const auto negative_final = evaluate_replay_fitness(log({{}}), net,
      ReplayFitnessVariant::TOKEN_BASED, options);
  assert(negative_final.log_fitness_is_integer_zero &&
         negative_final.token_result.consumed == -3 &&
         negative_final.token_result.remaining == 1);
  const TokenInteger huge("1" + std::string(400, '0'));
  options.exact_initial_marking = TokenMarking{huge, 0, 0};
  options.exact_final_marking = TokenMarking{0, 0, huge};
  bool overflow = false;
  try { (void)evaluate_replay_fitness(log({{"A"}}), net, ReplayFitnessVariant::TOKEN_BASED, options); }
  catch (const std::overflow_error& error) { overflow = std::string(error.what()) == "int too large to convert to float"; }
  assert(overflow);
  assert(evaluate_replay_fitness(EventLog{}, net, ReplayFitnessVariant::TOKEN_BASED, options).log_fitness_is_integer_zero);

  PnmlImportOptions import;
  import.infer_initial_marking = import.infer_final_marking = import.require_workflow_boundaries = false;
  const auto signed_weight = parse_pnml_document(R"(<pnml><net id="signed">
    <place id="source"><initialMarking><text>3</text></initialMarking></place><place id="target"/>
    <transition id="move"><name><text>A</text></name></transition>
    <arc id="input" source="source" target="move"><inscription><text>-1</text></inscription></arc>
    <arc id="output" source="move" target="target"><inscription><text>0</text></inscription></arc>
    <finalmarkings><marking><place idref="target"><text>1</text></place></marking></finalmarkings>
  </net></pnml>)", import);
  const auto nonpositive = evaluate_replay_fitness(log({{"A"}}), signed_weight);
  assert(nonpositive.computed && nonpositive.log_fitness_is_integer_zero &&
         nonpositive.fitness == 0.0 && nonpositive.token_result.consumed == 0 &&
         nonpositive.token_result.produced == 3);
  auto backwards = ReplayFitnessOptions{};
  backwards.token_replay_variant = TokenReplayVariant::kBackwards;
  bool undefined = false;
  try { (void)evaluate_replay_fitness(log({{}}), sequence_net(), ReplayFitnessVariant::TOKEN_BASED, backwards); }
  catch (const std::domain_error& error) { undefined = std::string(error.what()) == "float division by zero"; }
  assert(undefined);
}

void exact_model_quality_marking_contract() {
  // A full replay cannot convert this marking to float. Precision only needs
  // the initial choices for a one-event trace, so it still has a scalar result.
  const std::string huge = "1" + std::string(399, '0') + "1";
  PnmlImportOptions import;
  import.public_marking_semantics = true;
  const auto document = parse_pnml_document(
      "<pnml><net id=\"marked\"><page id=\"page\">"
      "<place id=\"source\"><initialMarking><text>" + huge +
      "</text></initialMarking></place><place id=\"spare\"/><place id=\"target\"/>"
      "<transition id=\"move\"><name><text>A</text></name></transition>"
      "<arc id=\"input\" source=\"source\" target=\"move\"/>"
      "<arc id=\"output\" source=\"move\" target=\"target\"/></page>"
      "<finalmarkings><marking><place idref=\"target\"><text>" + huge +
      "</text></place></marking></finalmarkings></net></pnml>", import);
  const auto variant = PrecisionVariant::ETCONFORMANCE_TOKEN;
  assert(evaluate_precision(EventLog{}, document, variant).precision == 1.0);
  assert(evaluate_precision(log({{}}), document, variant).precision == 0.0);
  const auto precision = evaluate_precision(log({{"A"}}), document, variant);
  assert(precision.computed && precision.precision == 1.0 &&
         precision.token_result_is_precision_only);
  assert(token_precision_result_json(precision) ==
         "{\"type\":\"float\",\"hex\":\"0x1.0000000000000p+0\"}");
  const DirectlyFollowsGraph graph;
  assert(precision_json(precision, &graph).find("\"token_replay\"") == std::string::npos);
  const auto overflows = [](auto evaluate) {
    bool observed = false;
    try { evaluate(); }
    catch (const std::overflow_error& error) {
      observed = std::string(error.what()) == "int too large to convert to float";
    }
    assert(observed);
  };
  overflows([&] { (void)evaluate_precision(log({{"A", "A"}}), document, variant); });
  overflows([&] { (void)evaluate_generalization(log({{"A"}}), document); });
  overflows([&] { (void)evaluate_composite(log({{"A"}}), document); });
  const auto empty = evaluate_composite(EventLog{}, document);
  assert(empty.computed && empty.fitness_details.log_fitness_is_integer_zero &&
         empty.metrics_average_weight == 0.5);

  // Document markings and explicitly supplied exact markings select the same
  // token evaluation even when their counts cannot fit a native int.
  auto rounded = document;
  const TokenInteger count("9007199254740992");
  rounded.initial_marking = {count, 0, 0};
  rounded.final_marking = {0, 0, count};
  const auto composite = evaluate_composite(log({{"A"}}), rounded);
  assert(composite.computed && composite.fitness == 0x1p-52 &&
         composite.fitness_details.average_trace_fitness == 0x1p-53 &&
         composite.precision == 1.0 && composite.generalization == 0.0 &&
         composite.simplicity == 1.0);
  CompositeEvaluationOptions explicit_markings;
  explicit_markings.exact_initial_marking = rounded.initial_marking;
  explicit_markings.exact_final_marking = rounded.final_marking;
  explicit_markings.initial_marking_keys = rounded.initial_marking_keys;
  explicit_markings.final_marking_keys = rounded.final_marking_keys;
  explicit_markings.place_names = rounded.place_ids;
  explicit_markings.transition_names = rounded.transition_ids;
  assert(composite_evaluation_result_json(composite) == composite_evaluation_result_json(
      evaluate_composite(log({{"A"}}), rounded.net, explicit_markings)));

  const auto empty_document = parse_pnml_document("<pnml><net id=\"empty\"/></pnml>", import);
  assert(evaluate_precision(log({{"A"}}), empty_document, variant).precision == 1.0);
  const auto generalization = evaluate_generalization(log({{"A"}}), empty_document);
  assert(generalization.computed && generalization.generalization == 1.0);
  assert(token_generalization_result_json(generalization) ==
         "{\"type\":\"float\",\"hex\":\"0x1.0000000000000p+0\"}");
  const auto empty_composite = evaluate_composite(EventLog{}, empty_document);
  assert(empty_composite.computed && empty_composite.metrics_average_weight == 0.75);

  const auto weighted = parse_pnml_document(R"(<pnml><net id="weighted">
    <place id="source"><initialMarking><text>2</text></initialMarking></place>
    <place id="gate"><initialMarking><text>3</text></initialMarking></place><place id="sink"/>
    <transition id="move"><name><text>A</text></name></transition>
    <arc id="input" source="source" target="move"/><arc id="guard" source="gate" target="move"/>
    <arc id="output" source="move" target="sink"/>
    <finalmarkings><marking><place idref="sink"><text>1</text></place></marking></finalmarkings>
  </net></pnml>)", import);
  const auto delimiters = log({{"A", ",", "A"}, {"A", "", "u1"}, {"A", "A,B", "A"}});
  assert(evaluate_precision(delimiters, weighted, variant).precision == 0.5);
  const auto collisions = log({{"A,B", "A"}, {"A", "B", "u1"}, {"", "A"}, {"", "u2"}, {",", "A"}});
  assert(evaluate_precision(collisions, weighted, variant).precision == 0x1.d1745d1745d17p-1);
}

void supplied_model_evaluation_transport_contract() {
  const auto xml = std::string("<pnml><net id=\"adapter\">") +
      "<place id=\"source\"><initialMarking><text>9007199254740992</text></initialMarking></place>"
      "<place id=\"sink\"/><transition id=\"move\"><name><text>A</text></name></transition>"
      "<arc source=\"source\" target=\"move\"/><arc source=\"move\" target=\"sink\"/>"
      "<finalmarkings><marking/></finalmarkings></net></pnml>";
  const auto document = minera::tools::parse_pnml_model(xml, true);
  assert(document.initial_marking[0] == TokenInteger("9007199254740992"));
  assert(!document.final_marking_keys[1] && document.final_marking[1] == 0);
  const auto native = minera::tools::parse_pnml_model(xml);
  assert(native.final_marking_keys[1] && native.final_marking[1] == 1);
  const auto gated = minera::tools::pnml_conformance_gate(document.net, &document);
  assert(gated && !gated->computed);
  DirectlyFollowsGraph graph;
  const auto unavailable = imported_model_json(*gated, graph);
  assert(unavailable.find("\"sound\": null") != std::string::npos &&
         unavailable.find("\"token_replay\": null") != std::string::npos &&
         unavailable.find("\"fitness\"") == std::string::npos);

  ModelEvaluation result;
  result.miner = "imported";
  const auto events = log({{"A"}});
  ReplayFitnessOptions overrides;
  overrides.initial_marking = {0, 0};
  overrides.final_marking = {0, 1};
  result.replay_fitness = evaluate_replay_fitness(
      events, document, ReplayFitnessVariant::TOKEN_BASED, overrides);
  assert(result.replay_fitness->computed && result.replay_fitness->fitness == 0.75);
  const auto serialized = model_evaluation_json(result);
  assert(serialized.find("\"public_result\": " +
      token_replay_fitness_result_json(*result.replay_fitness)) != std::string::npos);
  result.replay_fitness = evaluate_replay_fitness(
      events, document, ReplayFitnessVariant::ALIGNMENT_BASED);
  assert(!result.replay_fitness->computed &&
         model_evaluation_json(result).find("public_result") == std::string::npos);
  result.replay_fitness.reset();
  result.precision = evaluate_precision(events, document, PrecisionVariant::ETCONFORMANCE_TOKEN);
  assert(model_evaluation_json(result).find("\"public_result\": " +
      token_precision_result_json(*result.precision)) != std::string::npos);
  result.precision.reset();
  result.simplicity = evaluate_simplicity(document);
  assert(model_evaluation_json(result).find("\"public_result\": " +
      simplicity_arc_degree_result_json(*result.simplicity)) != std::string::npos);
}

void exact_token_result_aggregation_contract() {
  TokenReplayTraceDiagnostics signed_row;
  signed_row.trace_is_fit = false;
  signed_row.trace_fitness = 1.25;
  signed_row.consumed = 2;
  signed_row.produced = 2;
  signed_row.missing = -2;
  signed_row.remaining = -2;
  const auto signed_result = evaluate_replay_fitness(
      std::vector<TokenReplayTraceDiagnostics>{signed_row});
  assert(signed_result.computed && signed_result.fitness == 2.0 &&
         signed_result.average_trace_fitness == 1.25 &&
         signed_result.percentage_fit_traces == 0.0 &&
         !signed_result.log_fitness_is_integer_zero);
  assert(signed_result.token_result.missing == -2 &&
         signed_result.token_result.trace_diagnostics->front().remaining == -2);
  signed_row.trace_is_fit = true;
  signed_row.consumed = -2;
  const auto nonpositive = evaluate_replay_fitness(
      std::vector<TokenReplayTraceDiagnostics>{signed_row});
  assert(nonpositive.fitting_traces == 1 && nonpositive.fitness == 0.0 &&
         nonpositive.average_trace_fitness == 0.0 &&
         nonpositive.percentage_fit_traces == 0.0 &&
         nonpositive.log_fitness_is_integer_zero);
  signed_row.consumed = TokenInteger("9007199254740993");
  signed_row.produced = TokenInteger("9007199254740991");
  signed_row.missing = TokenInteger("9007199254740992");
  signed_row.remaining = TokenInteger("3002399751580330");
  TokenReplayResult full;
  full.traces.push_back(signed_row);
  const auto rounded = evaluate_replay_fitness(full);
  assert(rounded.fitness == 0x1.5555555555557p-2);
  assert(full.traces.front().consumed == TokenInteger("9007199254740993"));
  const std::string huge = "1" + std::string(400, '0');
  full.traces.front().produced = TokenInteger(huge);
  full.traces.front().consumed = TokenInteger(huge);
  full.traces.front().missing = TokenInteger(huge);
  full.traces.front().remaining = 0;
  full.traces.front().trace_fitness = std::numeric_limits<double>::infinity();
  const auto large = evaluate_replay_fitness(full);
  assert(large.fitness == 0.5 && std::isinf(large.average_trace_fitness) &&
         large.token_result.produced == TokenInteger(huge));
  full.traces.front().trace_fitness = std::numeric_limits<double>::quiet_NaN();
  assert(std::isnan(evaluate_replay_fitness(full).average_trace_fitness));
  assert(evaluate_replay_fitness(TokenReplayResult{}).log_fitness_is_integer_zero);
  assert(!evaluate_replay_fitness(full, ReplayFitnessVariant::ALIGNMENT_BASED).computed);

  // Halfway significands, subnormal ties and overflow exercise the rounding
  // needed by large weighted totals, independently of cancellation in fitness.
  assert(token_integer_ratio(TokenInteger("9007199254740993"), 1) == 0x1p+53);
  assert(token_integer_ratio(TokenInteger("9007199254740995"), 1) == 0x1.0000000000002p+53);
  TokenInteger power = 1;
  for (int i = 0; i < 1075; ++i) power *= 2;
  assert(token_integer_ratio(1, power) == 0.0);
  assert(token_integer_ratio(3, power) == 0x0.0000000000002p-1022);
  assert(std::signbit(token_integer_ratio(-1, power)));
  bool overflow = false;
  try { (void)token_integer_ratio(power, 1); }
  catch (const std::overflow_error&) { overflow = true; }
  assert(overflow);
}

void precomputed_replay_aggregation_contract() {
  const std::vector<PrecomputedTokenReplay> token_results{
      PrecomputedTokenReplay{true, false, true, 1.0, 0, 4, 0, 4, {2, 1}, {}},
      PrecomputedTokenReplay{true, false, false, 0.5, 1, 4, 1, 4, {1, 2}, {}}};
  const auto token = evaluate_replay_fitness(token_results);
  assert(token.computed && token.trace_count == 2 && token.fitting_traces == 1);
  assert(close_to(token.average_trace_fitness, 0.75));
  assert(close_to(token.fitness, 0.875));
  assert(close_to(token.token_result.fitness, token.fitness));
  assert(token.token_result.transition_frequencies ==
         std::vector<std::uint64_t>({3, 3}));
  const auto disagreement = evaluate_replay_fitness(
      std::vector<PrecomputedTokenReplay>{
          PrecomputedTokenReplay{true, false, false, 1.0, 0, 1, 0, 1, {}, {}}});
  assert(disagreement.computed && disagreement.fitting_traces == 0);
  const auto token_zero_denominator = evaluate_replay_fitness(
      std::vector<PrecomputedTokenReplay>{
          PrecomputedTokenReplay{true, false, true, 1.0, 0, 0, 0, 0, {}, {}}});
  assert(token_zero_denominator.computed &&
         token_zero_denominator.average_trace_fitness == 0.0 &&
         token_zero_denominator.percentage_fit_traces == 0.0 &&
         token_zero_denominator.log_fitness == 0.0);
  PrecomputedTokenReplay token_aliases;
  token_aliases.trace_is_fit = true;
  token_aliases.trace_fitness = 1.0;
  token_aliases.consumed = 2;
  token_aliases.produced = 2;
  const auto token_alias_result = evaluate_replay_fitness(
      std::vector<PrecomputedTokenReplay>{token_aliases});
  assert(token_alias_result.computed && token_alias_result.fitness == 1.0 &&
         token_alias_result.token_result.consumed == 2 &&
         token_alias_result.token_result.produced == 2);
  PrecomputedTokenReplay near_overflow;
  near_overflow.produced_tokens = std::numeric_limits<std::uint64_t>::max();
  near_overflow.consumed_tokens = 1;
  near_overflow.trace_fitness = 1.0;
  near_overflow.trace_is_fit = true;
  PrecomputedTokenReplay overflow_tail;
  overflow_tail.produced_tokens = 1;
  overflow_tail.consumed_tokens = 1;
  overflow_tail.trace_fitness = 1.0;
  const auto overflow = evaluate_replay_fitness(
      std::vector<PrecomputedTokenReplay>{near_overflow, overflow_tail});
  assert(overflow.computed && overflow.token_result.produced ==
         TokenInteger(std::numeric_limits<std::uint64_t>::max()) + 1 &&
         overflow.token_result.trace_count == 2 && overflow.log_fitness == 1.0);
  token_aliases.consumed_tokens = 0;
  token_aliases.produced_tokens = 0;
  const auto explicit_zero = evaluate_replay_fitness(
      std::vector<PrecomputedTokenReplay>{token_aliases});
  assert(explicit_zero.token_result.consumed == 0 &&
         explicit_zero.token_result.produced == 0 &&
         explicit_zero.log_fitness_is_integer_zero);
  auto metadata = token_results;
  metadata[0].computed = false;
  metadata[0].error = "unfinished";
  metadata[1].exhausted = true;
  const auto unfiltered = evaluate_replay_fitness(metadata);
  assert(unfiltered.computed && unfiltered.exhausted &&
         unfiltered.trace_count == 2 && unfiltered.log_fitness == token.log_fitness);
  metadata[0].trace_fitness = std::numeric_limits<double>::infinity();
  assert(std::isinf(evaluate_replay_fitness(metadata).average_trace_fitness));
  metadata[1].trace_fitness = -std::numeric_limits<double>::infinity();
  assert(std::isnan(evaluate_replay_fitness(metadata).average_trace_fitness));
  metadata[0].produced_tokens = TokenInteger("100000000000000000000000000000000000001");
  metadata[0].consumed_tokens = -4;
  const auto signed_counts = evaluate_replay_fitness(metadata);
  assert(signed_counts.token_result.consumed == 0 &&
         signed_counts.log_fitness_is_integer_zero);
  near_overflow.transition_activations = {std::numeric_limits<std::uint64_t>::max()};
  overflow_tail.transition_activations = {1};
  const auto activation_overflow = evaluate_replay_fitness(
      std::vector<PrecomputedTokenReplay>{near_overflow, overflow_tail});
  assert(activation_overflow.computed && activation_overflow.log_fitness == 1.0 &&
         activation_overflow.token_result.transition_frequencies.empty());

  const std::vector<PrecomputedAlignmentReplay> alignment_results{
      PrecomputedAlignmentReplay{true, false, false, 0.5, 2.0, 4.0, {}},
      PrecomputedAlignmentReplay{false, true, false, 0.0, 0.0, 0.0,
                                 "unfinished"},
      PrecomputedAlignmentReplay{true, false, false, 1.0, 0.0, 0.0, {}}};
  const auto alignment = evaluate_replay_fitness(alignment_results);
  assert(alignment.computed && alignment.trace_count == 2 &&
         alignment.fitting_traces == 1);
  assert(close_to(alignment.average_trace_fitness, 0.75));
  assert(close_to(alignment.fitness, 0.5));
  assert(alignment.alignment_result.variants.size() == 2);

  const auto zero_denominator = evaluate_replay_fitness(
      std::vector<PrecomputedAlignmentReplay>{
          PrecomputedAlignmentReplay{true, false, false, 0.0, 0.0, 0.0, {}}});
  assert(zero_denominator.computed && zero_denominator.trace_count == 1 &&
         zero_denominator.fitness == 1.0);
  const auto empty = evaluate_replay_fitness(
      std::vector<PrecomputedAlignmentReplay>{});
  assert(empty.computed && empty.trace_count == 0 && empty.fitness == 0.0);
  const auto negative_log_fitness = evaluate_replay_fitness(
      std::vector<PrecomputedAlignmentReplay>{
          PrecomputedAlignmentReplay{true, false, false, 0.0, 6.0, 4.0, {}}});
  assert(negative_log_fitness.computed &&
         close_to(negative_log_fitness.log_fitness, -0.5));
  const double largest = std::numeric_limits<double>::max();
  const auto alignment_overflow = evaluate_replay_fitness(
      std::vector<PrecomputedAlignmentReplay>{
          PrecomputedAlignmentReplay{true, false, false, 0.0, largest,
                                     largest, {}},
          PrecomputedAlignmentReplay{true, false, false, 0.0, largest,
                                     largest, {}}});
  assert(!alignment_overflow.computed &&
         alignment_overflow.alignment_result.variants.empty() &&
         alignment_overflow.trace_count == 0);

  // Serialized alignment payloads commonly use `bwc` for the denominator;
  // the native aggregator accepts that alias when the descriptive field is
  // left at its default.
  PrecomputedAlignmentReplay alias_bwc;
  alias_bwc.fitness = 0.5;
  alias_bwc.cost = 1.0;
  alias_bwc.bwc = 2.0;
  const auto alias_result = evaluate_replay_fitness(
      std::vector<PrecomputedAlignmentReplay>{alias_bwc});
  assert(alias_result.computed && close_to(alias_result.fitness, 0.5) &&
         alias_result.alignment_result.variants.front().worst_cost == 2.0);
}

void emd_encoding_and_mass_contract() {
  StochasticLanguage model{{{{"a", "b", "d", "e"}, 0.49},
                            {{"a", "d", "b", "e"}, 0.49},
                            {{"a", "c", "d", "e"}, 0.01},
                            {{"a", "d", "c", "e"}, 0.01}},
                           1.0};
  const auto language = [](std::initializer_list<WeightedTrace> traces) {
    StochasticLanguage result;
    result.traces.assign(traces.begin(), traces.end());
    for (const auto& trace : result.traces) result.total_mass += trace.weight;
    return result;
  };
  const auto l1 = language({{{"a", "b", "d", "e"}, 0.49},
                            {{"a", "d", "b", "e"}, 0.49},
                            {{"a", "c", "d", "e"}, 0.01},
                            {{"a", "d", "c", "e"}, 0.01}});
  const auto l2 = language({{{"a", "b", "e"}, 0.5},
                            {{"a", "b", "d", "e"}, 0.245},
                            {{"a", "d", "b", "e"}, 0.245},
                            {{"a", "c", "d", "e"}, 0.005},
                            {{"a", "d", "c", "e"}, 0.005}});
  const auto l3 = language({{{"a", "b", "d", "e"}, 0.489},
                            {{"a", "d", "b", "e"}, 0.489},
                            {{"a", "c", "d", "e"}, 0.01},
                            {{"a", "d", "c", "e"}, 0.01},
                            {{"a", "b", "e"}, 0.002}});
  const auto l4 = language({{{"a", "b", "d", "e"}, 0.5},
                            {{"a", "d", "b", "e"}, 0.5}});
  const auto l5 = language({{{"a", "c", "d", "e"}, 0.5},
                            {{"a", "d", "c", "e"}, 0.5}});
  assert(close_to(evaluate_emd(model, l1).distance, 0.0));
  assert(close_to(evaluate_emd(model, l2).distance, 0.1275));
  assert(close_to(evaluate_emd(model, l3).distance, 0.0005));
  assert(close_to(evaluate_emd(model, l4).distance, 0.005));
  assert(close_to(evaluate_emd(model, l5).distance, 0.245));

  StochasticLanguage left{{{{"only-left"}, 0.5}, {{"shared"}, 0.5}}, 1.0};
  StochasticLanguage right{{{{"only-right"}, 0.5}, {{"shared"}, 0.5}}, 1.0};
  std::size_t callback_calls = 0;
  bool saw_distinct_union_symbols = false;
  EmdOptions callback_options;
  callback_options.encoded_distance =
      [&](const std::u32string& a, const std::u32string& b) {
        ++callback_calls;
        if (!a.empty() && !b.empty() && a.front() != b.front())
          saw_distinct_union_symbols = true;
        return 0.0;
      };
  const auto callback_result = evaluate_emd(left, right, callback_options);
  assert(callback_result.computed && callback_calls != 0 &&
         saw_distinct_union_symbols);

  std::vector<std::string> many_activities;
  for (std::size_t i = 0; i < 260; ++i)
    many_activities.push_back("activity-" + std::to_string(i));
  StochasticLanguage wide{{{many_activities, 1.0}}, 1.0};
  std::size_t encoded_length = 0;
  char32_t maximum_symbol = 0;
  callback_options.encoded_distance =
      [&](const std::u32string& a, const std::u32string& b) {
        encoded_length = a.size();
        for (const auto symbol : a) maximum_symbol = std::max(maximum_symbol, symbol);
        assert(a == b);
        return 0.0;
      };
  assert(evaluate_emd(wide, wide, callback_options).computed);
  assert(encoded_length == 260 && maximum_symbol > 255);

  StochasticLanguage unequal{{{{"A"}, 1.0}}, 1.0};
  StochasticLanguage double_mass{{{{"A"}, 2.0}}, 2.0};
  const auto normalized = evaluate_emd(unequal, double_mass);
  assert(normalized.computed && normalized.normalized && normalized.distance == 0.0);
  callback_options.encoded_distance = {};
  callback_options.use_fast_emd = false;
  assert(!evaluate_emd(unequal, double_mass, callback_options).computed);
  callback_options.use_fast_emd = true;
  callback_options.normalize_masses = false;
  assert(!evaluate_emd(unequal, double_mass, callback_options).computed);
  callback_options.normalize_masses = true;
  StochasticLanguage zero_left{{{{"zero"}, 0.0}}, 0.0};
  StochasticLanguage zero_right{{{{"other"}, 0.0}}, 0.0};
  assert(evaluate_emd(zero_left, zero_right).computed &&
         evaluate_emd(zero_left, zero_right).distance == 0.0);
  assert(evaluate_emd(StochasticLanguage{}, StochasticLanguage{}).distance == 0.0);
  StochasticLanguage tiny_left{{{{"tiny-left"}, 1e-15}}, 1e-15};
  StochasticLanguage tiny_right{{{{"tiny-right"}, 1e-15}}, 1e-15};
  const auto tiny = evaluate_emd(tiny_left, tiny_right);
  assert(tiny.computed && std::abs(tiny.distance - 1e-15) < 1e-24);

  const double largest_mass = std::numeric_limits<double>::max();
  StochasticLanguage huge_left{{{{"huge-left"}, largest_mass}}, largest_mass};
  StochasticLanguage huge_right{{{{"huge-right"}, largest_mass}}, largest_mass};
  EmdOptions overflowing_cost;
  overflowing_cost.encoded_distance =
      [largest_mass](const std::u32string&, const std::u32string&) {
        return largest_mass;
      };
  const auto overflow =
      evaluate_emd(huge_left, huge_right, overflowing_cost);
  assert(!overflow.computed &&
         overflow.error == "transportation cost overflow");
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

int main(int argc, char** argv) {
  if (argc == 2 && std::string(argv[1]) == "--arc-degree-module") {
    arc_degree_module_acceptance();
    return 0;
  }
  arc_degree_module_acceptance();
  replay_fitness_token_contract();
  replay_fitness_backwards_token_contract();
  replay_fitness_alignment_contract();
  precision_etconformance_token_contract();
  precision_align_etconformance_contract();
  precision_automaton_after_align_contract();
  precision_silent_search_name_order_contract();
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
  simplicity_presented_arc_occurrences_contract();
  simplicity_imported_arc_occurrences_contract();
  simplicity_nonfinite_baseline_contract();
  simplicity_extended_cyclomatic_contract();
  simplicity_reachable_transition_occurrences_contract();
  simplicity_presented_reachability_contract();
  simplicity_extended_document_cyclomatic_contract();
  simplicity_signed_weighted_presentation_contract();
  composite_default_contract();
  composite_document_and_weight_contract();
  evaluation_event_data_adapters_contract();
  precomputed_replay_aggregation_contract();
  exact_token_result_aggregation_contract();
  exact_log_fitness_marking_contract();
  exact_model_quality_marking_contract();
  supplied_model_evaluation_transport_contract();
  emd_pyemd_contract();
  emd_encoding_and_mass_contract();
  process_tree_evaluation_overloads_contract();
  return 0;
}
