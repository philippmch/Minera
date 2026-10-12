#include "minera/evaluation.hpp"
#include "minera/json_export.hpp"
#include "minera/precomputed_replay_json.hpp"
#include "minera/pnml.hpp"
#include "minera/log_conversion.hpp"
#include <cassert>
#include <cmath>
#include <limits>

using namespace minera;

static void precomputed_json_keeps_native_aggregation() {
  const auto rows = parse_precomputed_token_replay_json(R"([
    {"computed":false,"trace_is_fit":true,"trace_fitness":1,
     "missing_tokens":0,"missing":"9999999999999999999999999",
     "consumed_tokens":"340282366920938463463374607431768211456",
     "produced":"340282366920938463463374607431768211456",
     "transition_activations":[18446744073709551615],"error":"資料☃🚀"},
    {"exhausted":true,"trace_is_fit":false,"trace_fitness":0.5,
     "missing":1,"consumed":4,"remaining":1,"produced":4,
     "transition_activations":[1]}
  ])");
  assert(rows.size()==2 && rows[0].error=="資料☃🚀");
  assert(rows[0].missing_tokens && *rows[0].missing_tokens==0);
  const auto fitness=evaluate_replay_fitness(rows);
  assert(fitness.computed && fitness.exhausted && fitness.trace_count==2);
  assert(fitness.fitting_traces==1 && fitness.average_trace_fitness==0.75);
  assert(fitness.token_result.consumed.decimal()=="340282366920938463463374607431768211460");
  assert(fitness.token_result.transition_frequencies.empty());
  const auto aliases=parse_precomputed_token_replay_json(R"({"trace_diagnostics":[
    {"case_id":"case","missing_tokens":null,"missing":-3,"produced":4,"consumed":4}
  ]})");
  assert(!aliases[0].missing_tokens && aliases[0].missing==-3);
  const auto wrapped=parse_precomputed_token_replay_json(R"({"miner":"native","replay_fitness":{
    "token_replay":{"trace_diagnostics":[{"trace_is_fit":true,"trace_fitness":1,"produced":4,"consumed":4}]}
  }})");
  assert(wrapped.size()==1 && evaluate_replay_fitness(wrapped).fitness==1);
  const auto alignment=parse_precomputed_alignment_replay_json(R"([
    null,{"computed":false,"fitness":"nan"},
    {"trace_is_fit":false,"fitness":1,"cost":0,"bwc":0},
    {"fitness":0.5,"cost":1,"best_worst_cost":0,"bwc":2}
  ])");
  const auto aligned=evaluate_replay_fitness(alignment);
  assert(alignment.size()==4 && !alignment[0].computed && aligned.trace_count==2);
  assert(aligned.fitting_traces==1 && aligned.average_trace_fitness==0.75);
  assert(aligned.fitness==0.5 && aligned.total_cost==1);
  const auto nonfinite=precomputed_replay_fitness_json(
      R"([{"trace_fitness":"nan","produced":1,"consumed":1}])","token");
  assert(nonfinite.find("\"average_trace_fitness\": null")!=std::string::npos);
  assert(nonfinite.find("\"hex\":\"nan\"")!=std::string::npos);
  const auto precise=precomputed_replay_fitness_json(
      R"([{"trace_fitness":0.33333333333333331,"produced":1,"consumed":1}])","token");
  const auto marker=precise.find("\"average_trace_fitness\": ");
  assert(marker!=std::string::npos && std::stod(precise.substr(marker+25))==1.0/3.0);
  assert(precomputed_replay_fitness_json("[]","token").find("\"submitted_rows\":0")!=std::string::npos);
  assert(precomputed_replay_fitness_json("[]","alignment").find("\"submitted_rows\":0")!=std::string::npos);
  const auto mismatch=precomputed_replay_fitness_json("[]","token",ReplayFitnessVariant::AUTO);
  assert(mismatch.find("precomputed token results require token-based replay")!=std::string::npos);
  for (const auto bad : {"[null]","[{\"missing\":1.5}]","[{\"computed\":1}]",
                         "[{\"trace_fitness\":1,\"trace_fitness\":0}]",
                         "[{\"case_id\":\"\\ud800\"}]","[] trailing"}) {
    bool rejected=false;
    try { (void)parse_precomputed_token_replay_json(bad); }
    catch(const std::invalid_argument&) { rejected=true; }
    assert(rejected);
  }
}

static PetriNet sequence_net() {
  PetriNet n; auto p0=n.add_place(), p1=n.add_place(), p2=n.add_place(); n.set_source(p0); n.set_sink(p2); n.set_activities({"A","B"});
  n.add_transition(0,{p0},{p1}); n.add_transition(1,{p1},{p2}); return n;
}
static EventLog log(std::initializer_list<std::initializer_list<const char*>> xs) { EventLog l; for(auto tr:xs){Trace t;for(auto a:tr)t.events.push_back(Event{"",a});l.traces.push_back(std::move(t));}return l; }

static void flat_document_fitness_preserves_markings_and_case_selection() {
  auto source=log({{"A","B"},{"A","B"},{"A","B"}});
  for(std::size_t i=0;i<source.traces.size();++i) {
    source.traces[i].case_id="case-"+std::to_string(i);
    source.traces[i].attributes.push_back({"bucket",i<2?"shared":"separate",AttributeKind::kString});
  }
  PnmlDocument document;
  document.net=sequence_net();document.initial_marking={1,0,0};document.final_marking={0,0,1};
  document.initial_marking_keys={true,false,false};document.final_marking_keys={false,false,true};
  document.place_ids={"p-start","p-middle","p-end"};document.transition_ids={"transition-A","transition-B"};
  const auto stream=event_log_to_event_stream(source);const auto table=event_log_to_table(source);const auto lazy=event_log_to_lazy_table(source);
  for(const auto& input:{EventDataView(stream),EventDataView(table),EventDataView(lazy)}) {
    const auto result=evaluate_replay_fitness(input,document);
    assert(result.computed && result.fitness==1 && result.trace_count==3);
    ReplayFitnessOptions grouped;grouped.case_id_attribute="case:bucket";
    const auto regrouped=evaluate_replay_fitness(input,document,ReplayFitnessVariant::TOKEN_BASED,grouped);
    assert(regrouped.computed && regrouped.trace_count==2);
    grouped.max_rows_to_materialize=5;
    bool capped=false;
    try { (void)evaluate_replay_fitness(input,document,ReplayFitnessVariant::TOKEN_BASED,grouped); }
    catch(const std::length_error&) { capped=true; }
    assert(capped);
  }
  document.initial_marking={TokenInteger("18446744073709551616"),0,0};
  const auto direct=evaluate_replay_fitness(source,document,ReplayFitnessVariant::TOKEN_BASED);
  const auto flat=evaluate_replay_fitness(EventDataView(stream),document);
  assert(direct.computed && flat.computed && direct.token_result.remaining==flat.token_result.remaining &&
         direct.token_result.produced==flat.token_result.produced && direct.fitness==flat.fitness);
}

static EventLog attributed_sequence_log() {
  EventLog result;
  Trace trace;
  Event first{"", "raw-a"};
  first.attributes.push_back({"phase", "A", AttributeKind::kString});
  Event second{"", "raw-b"};
  second.attributes.push_back({"phase", "B", AttributeKind::kString});
  trace.events.push_back(std::move(first));
  trace.events.push_back(std::move(second));
  result.traces.push_back(std::move(trace));
  return result;
}

void weighted_input_enablement() {
  PetriNet n; auto p=n.add_place(), q=n.add_place(); n.set_source(p); n.set_sink(q); n.set_activities({"A"}); n.add_transition(0,{p,p},{q});
  auto r=evaluate_precision(log({{"A"}}),n,PrecisionVariant::ETCONFORMANCE_TOKEN);
  // A single token must not satisfy a two-token preset.  The disabled model
  // therefore has no escaping edge and precision is exactly one.
  assert(r.computed && std::abs(r.precision-1.0)<1e-12);
}
void cardoso_deduplicates_weighted_outputs() {
  PetriNet n; auto p=n.add_place(),a=n.add_place(),b=n.add_place(); n.set_source(p); n.set_sink(b); n.set_activities({"A","B"}); n.add_transition(0,{p},{a,a,b}); n.add_transition(1,{p},{a,b});
  auto s=evaluate_simplicity(n,SimplicityVariant::EXTENDED_CARDOSO); assert(s.value==1.0);
}
void cyclomatic_counts_parallel_transition_edges() {
  PetriNet n; auto p=n.add_place(),q=n.add_place(); n.set_source(p); n.set_sink(q); n.set_activities({"A","B"}); n.add_transition(0,{p},{q}); n.add_transition(1,{p},{q});
  auto s=evaluate_simplicity(n,SimplicityVariant::EXTENDED_CYCLOMATIC);
  assert(s.computed && s.vertices==2 && s.edges==2);
  assert(s.strongly_connected_components==2 && s.value==2.0);
}
void unexecuted_transition_penalty() {
  auto n=sequence_net(); auto r=evaluate_generalization(log({{"A"}}),n);
  assert(r.executed_transitions==1 && r.transition_frequencies.size()==2);
  assert(r.transition_frequencies[0]==1 && r.transition_frequencies[1]==0);
  // One seen and one unseen transition each contribute a unit rarity penalty.
  assert(std::abs(r.generalization-0.0)<1e-12);
}
void generalization_counts_silent_routing_and_unseen_branches() {
  PetriNet n;
  const auto source = n.add_place();
  const auto routed = n.add_place();
  const auto choice = n.add_place();
  const auto sink = n.add_place();
  n.set_source(source);
  n.set_sink(sink);
  n.set_activities({"A", "B", "C"});
  n.add_transition(0, {source}, {routed});
  n.add_transition(std::nullopt, {routed}, {choice});
  n.add_transition(1, {choice}, {sink});
  n.add_transition(2, {choice}, {sink});

  const auto result = evaluate_generalization(
      log({{"A", "B"}, {"A", "B"}}), n);
  assert(result.transition_frequencies ==
         std::vector<std::uint64_t>({2, 2, 2, 0}));
  assert(result.executed_transitions == 3 && result.transition_count == 4);
  const double expected = 1.0 - (3.0 / std::sqrt(2.0) + 1.0) / 4.0;
  assert(std::abs(result.generalization - expected) < 1e-12);
}
void projection_records_have_place_markings() {
  auto n=sequence_net(); auto a=check_alignment(log({{"A","B"}}),n); assert(a.computed && !a.variants.empty());
  for(const auto& v:a.variants) for(const auto& p:v.projected_model_moves) assert(p.marking_before.size()==n.place_count() && p.marking_after.size()==n.place_count());
}

static PetriNet optional_after_a_net() {
  PetriNet n; auto p0=n.add_place(), p1=n.add_place(), sink=n.add_place();
  n.set_source(p0); n.set_sink(sink); n.set_activities({"A","B","C"});
  n.add_transition(0,{p0},{p1});
  n.add_transition(std::nullopt,{p1},{sink}); // optional tau completion
  n.add_transition(1,{p1},{sink});
  n.add_transition(2,{p1},{sink});
  return n;
}

static PetriNet choice_after_a_net() {
  PetriNet n; auto p0=n.add_place(), p1=n.add_place(), sink=n.add_place();
  n.set_source(p0); n.set_sink(sink); n.set_activities({"A","B","C"});
  n.add_transition(0,{p0},{p1});
  n.add_transition(1,{p1},{sink});
  n.add_transition(2,{p1},{sink});
  return n;
}

static PetriNet duplicate_label_net() {
  PetriNet n; auto p0=n.add_place(), p1=n.add_place(), p2=n.add_place(), sink=n.add_place();
  n.set_source(p0); n.set_sink(sink); n.set_activities({"A","B"});
  n.add_transition(0,{p0},{p1}); // A, branch one
  n.add_transition(0,{p0},{p2}); // A, branch two
  n.add_transition(1,{p1},{sink});
  n.add_transition(1,{p2},{sink});
  return n;
}

void automaton_model_only_projection() {
  auto n=sequence_net(); auto a=check_alignment(log({{"A"}}),n);
  assert(a.computed && a.variants.size()==1);
  const auto& moves=a.variants[0].projected_model_moves;
  assert(moves.size()==2 && moves[0].activity=="A" && moves[1].activity=="B");
  assert(moves[1].transition==1);
  auto p=evaluate_precision(log({{"A"}}),n,PrecisionVariant::AUTOMATON_AFTER_ALIGN);
  assert(p.computed && std::abs(p.precision-1.0)<1e-12);
}

void automaton_ignores_log_only_moves() {
  auto n=sequence_net(); auto a=check_alignment(log({{"A","X"}}),n);
  assert(a.computed && a.variants.size()==1);
  for (const auto& m:a.variants[0].projected_model_moves) assert(m.activity!="X");
  auto p=evaluate_precision(log({{"A","X"}}),n,PrecisionVariant::AUTOMATON_AFTER_ALIGN);
  assert(p.computed && std::abs(p.precision-1.0)<1e-12);
  auto align=evaluate_precision(log({{"A","X"}}),n,PrecisionVariant::ALIGN_ETCONFORMANCE);
  // The proper prefix A reaches the B-enabled marking; the log-only X does
  // not count as an observed model activity, so ALIGN_ET escapes B.
  assert(align.computed && std::abs(align.precision-0.5)<1e-12);
}

void automaton_unions_shared_prefix_observations() {
  auto n=choice_after_a_net();
  auto p=evaluate_precision(log({{"A","B"},{"A","C"}}),n,PrecisionVariant::AUTOMATON_AFTER_ALIGN);
  assert(p.computed && std::abs(p.precision-1.0)<1e-12);
}

void automaton_excludes_short_terminal_prefix_frequency() {
  // A can terminate silently, while B/C remain optional after A.  The
  // shorter [A] trace must not count A as a proper prefix.  The root prefix
  // contributes two enabled/observed mass units and A contributes one of two
  // enabled choices, giving the exact aggregate score 3/4.
  auto n=optional_after_a_net();
  auto p=evaluate_precision(log({{"A","B"},{"A"}}),n,PrecisionVariant::AUTOMATON_AFTER_ALIGN);
  assert(p.computed && std::abs(p.precision-0.75)<1e-12);
}

void automaton_task_ids_disambiguate_duplicate_labels() {
  auto n=duplicate_label_net(); auto l=log({{"A","B"}});
  auto labels=evaluate_precision(l,n,PrecisionVariant::AUTOMATON_AFTER_ALIGN);
  PrecisionOptions ids; ids.use_task_ids=true;
  auto task=evaluate_precision(l,n,PrecisionVariant::AUTOMATON_AFTER_ALIGN,ids);
  assert(labels.computed && task.computed);
  assert(std::abs(labels.precision-1.0)<1e-12);
  assert(std::abs(task.precision-(2.0/3.0))<1e-12);
}

void automaton_differs_from_alignment_precision_on_deviation() {
  auto n=sequence_net(); auto l=log({{"X","A"}});
  auto a=evaluate_precision(l,n,PrecisionVariant::ALIGN_ETCONFORMANCE);
  auto p=evaluate_precision(l,n,PrecisionVariant::AUTOMATON_AFTER_ALIGN);
  assert(a.computed && p.computed);
  assert(std::abs(a.precision)<1e-12);
  assert(std::abs(p.precision-1.0)<1e-12);
}

void alignment_precision_excludes_unfit_prefixes() {
  PetriNet n;
  const auto source = n.add_place();
  const auto branch = n.add_place();
  const auto sink = n.add_place();
  n.set_source(source);
  n.set_sink(sink);
  n.set_activities({"A", "B", "C"});
  n.add_transition(0, {source}, {branch});
  n.add_transition(1, {branch}, {sink});
  n.add_transition(2, {branch}, {sink});

  // The fitting prefix A contributes two escaping edges because X is not
  // available. A,X cannot replay exactly and must not contribute a second
  // copy of that marking through a fabricated log move.
  const auto result = evaluate_precision(
      log({{"A", "X", "Y"}}), n,
      PrecisionVariant::ALIGN_ETCONFORMANCE);
  assert(result.computed && !result.exhausted);
  assert(std::abs(result.precision - (1.0 / 3.0)) < 1e-12);
}

void cyclomatic_default_multiple_empty_preset_places() {
  PetriNet n; auto p0=n.add_place(), p1=n.add_place(), sink=n.add_place();
  n.set_source(p0); n.set_sink(sink); n.set_activities({"A","B"});
  n.add_transition(0,{p0},{sink}); n.add_transition(1,{p1},{sink});
  auto s=evaluate_simplicity(n,SimplicityVariant::EXTENDED_CYCLOMATIC);
  assert(s.computed && s.vertices==4 && s.edges==4 && s.strongly_connected_components==4);
  assert(std::abs(s.value-4.0)<1e-12);
}

void simplicity_counts_extended_arcs_and_accepts_finite_baselines() {
  PetriNet n;
  const auto source = n.add_place();
  const auto reset = n.add_place();
  const auto inhibitor = n.add_place();
  const auto sink = n.add_place();
  n.set_source(source);
  n.set_sink(sink);
  n.set_activities({"A"});
  const auto transition = n.add_transition(0, {source}, {sink});
  n.add_arc(transition, reset, PetriArcType::kReset);
  n.add_arc(transition, inhibitor, PetriArcType::kInhibitor);

  SimplicityOptions arc_options;
  arc_options.arc_degree_k = 0.0;
  const auto arc_degree = evaluate_simplicity(
      n, SimplicityVariant::ARC_DEGREE, arc_options);
  assert(arc_degree.computed);
  assert(std::abs(arc_degree.value - (1.0 / 2.6)) < 1e-12);

  arc_options.arc_degree_k = -1.0;
  const auto lowered = evaluate_simplicity(
      n, SimplicityVariant::ARC_DEGREE, arc_options);
  assert(lowered.computed);
  assert(std::abs(lowered.value - (1.0 / 3.6)) < 1e-12);

  const auto cardoso = evaluate_simplicity(
      n, SimplicityVariant::EXTENDED_CARDOSO);
  assert(cardoso.computed && cardoso.value == 3.0);
}

void default_variant_dispatch_follows_soundness() {
  auto sound=sequence_net();
  auto fit=evaluate_replay_fitness(log({{"A","B"}}),sound);
  auto prec=evaluate_precision(log({{"A","B"}}),sound);
  assert(fit.variant==ReplayFitnessVariant::ALIGNMENT_BASED);
  assert(fit.alignment_result.variant ==
         AlignmentVariant::kDijkstraLessMemory);
  assert(fit.alignment_result.diagnostics.lp_solved == 0);
  assert(prec.variant==PrecisionVariant::ALIGN_ETCONFORMANCE);

  ReplayFitnessOptions capped;
  capped.alignment_options.max_states = 1;
  const auto bounded_auto = evaluate_replay_fitness(
      log({{"A", "B"}}), sound, ReplayFitnessVariant::AUTO, capped);
  assert(bounded_auto.variant == ReplayFitnessVariant::ALIGNMENT_BASED);
  assert(!bounded_auto.computed && bounded_auto.exhausted);

  PetriNet unsound; auto p0=unsound.add_place(), dead=unsound.add_place(), sink=unsound.add_place();
  unsound.set_source(p0); unsound.set_sink(sink); unsound.set_activities({"A"});
  unsound.add_transition(0,{p0},{dead}); // sink is unreachable
  auto uf=evaluate_replay_fitness(log({{"A"}}),unsound);
  auto up=evaluate_precision(log({{"A"}}),unsound);
  assert(uf.variant==ReplayFitnessVariant::TOKEN_BASED);
  assert(up.variant==PrecisionVariant::ETCONFORMANCE_TOKEN);
}

void evaluator_selectors_reach_every_native_engine() {
  const auto n = sequence_net();
  const auto l = attributed_sequence_log();

  ReplayFitnessOptions token;
  token.activity_attribute = "phase";
  const auto token_result = evaluate_replay_fitness(
      l, n, ReplayFitnessVariant::TOKEN_BASED, token);
  assert(token_result.fitness == 1.0);
  assert(token_result.token_result.activity_attribute == "phase");
  const auto raw_graph = discover_directly_follows_graph(l);
  const auto token_json = replay_fitness_json(token_result, &raw_graph);
  assert(token_json.find("\"activities\": [null, null]") !=
         std::string::npos);
  assert(token_json.find("\"activity_names\": [\"A\", \"B\"]") !=
         std::string::npos);
  assert(token_json.find("\"activities\": [0, 0]") ==
         std::string::npos);

  ReplayFitnessOptions alignment;
  alignment.activity_attribute = "phase";
  alignment.alignment_options.variant =
      AlignmentVariant::kDijkstraNoHeuristics;
  const auto alignment_result = evaluate_replay_fitness(
      l, n, ReplayFitnessVariant::ALIGNMENT_BASED, alignment);
  assert(alignment_result.computed && alignment_result.fitness == 1.0);
  assert(alignment_result.alignment_result.variant ==
         AlignmentVariant::kDijkstraNoHeuristics);
  assert(alignment_result.alignment_result.activity_attribute == "phase");
  const auto alignment_json = replay_fitness_json(
      alignment_result, &raw_graph);
  assert(alignment_json.find(
             "\"activity\": null, \"activity_name\": \"A\", "
             "\"silent\": false") != std::string::npos);

  ReplayFitnessOptions decomposed;
  decomposed.activity_attribute = "phase";
  decomposed.decomposed_alignment_options = DecomposedAlignmentOptions{};
  const auto decomposed_result = evaluate_replay_fitness(
      l, n, ReplayFitnessVariant::ALIGNMENT_BASED, decomposed);
  assert(decomposed_result.computed && decomposed_result.fitness == 1.0);
  assert(decomposed_result.decomposed_alignment_result.has_value());
  assert(decomposed_result.decomposed_alignment_result->trace_count == 1);
  const auto decomposed_json = replay_fitness_json(decomposed_result);
  assert(decomposed_json.find("\"variant\":\"recompos_maximal\"") !=
         std::string::npos);

  PrecisionOptions precision;
  precision.activity_attribute = "phase";
  const auto precision_result = evaluate_precision(
      l, n, PrecisionVariant::AUTOMATON_AFTER_ALIGN, precision);
  assert(precision_result.computed && precision_result.precision == 1.0);
  assert(precision_result.alignment_result.activity_attribute == "phase");

  GeneralizationOptions generalization;
  generalization.activity_attribute = "phase";
  const auto generalization_result = evaluate_generalization(
      l, n, GeneralizationVariant::GENERALIZATION_TOKEN, generalization);
  assert(generalization_result.transition_frequencies ==
         std::vector<std::uint64_t>({1, 1}));

  CompositeEvaluationOptions composite;
  composite.activity_attribute = "phase";
  const auto composite_result = evaluate_composite(l, n, composite);
  assert(composite_result.computed && composite_result.fitness == 1.0);
}

void marked_net_options_reach_every_evaluator_engine() {
  const auto n = sequence_net();
  const auto suffix = log({{"B"}});
  const std::vector<int> initial{0, 1, 0};
  const std::vector<int> final{0, 0, 1};

  TokenReplayOptions replay;
  replay.initial_marking = initial;
  replay.final_marking = final;
  const auto direct_replay = check_conformance(suffix, n, replay);
  assert(direct_replay.fitness == 1.0 && direct_replay.fitting_traces == 1);
  const auto diagnostics = replay_token_diagnostics(suffix, n, replay);
  assert(diagnostics.fitting_traces == 1 &&
         diagnostics.transition_activations ==
             std::vector<std::uint64_t>({0, 1}));
  replay.variant = TokenReplayVariant::kBackwards;
  const auto backwards_replay = check_conformance(suffix, n, replay);
  assert(backwards_replay.fitness == 1.0 &&
         backwards_replay.fitting_traces == 1);

  TokenReplayOptions underfilled;
  underfilled.initial_marking = initial;
  underfilled.final_marking = {0, 0, 2};
  const auto forward_underfilled = check_conformance(suffix, n, underfilled);
  underfilled.variant = TokenReplayVariant::kBackwards;
  const auto backwards_underfilled = check_conformance(suffix, n, underfilled);
  assert(std::abs(forward_underfilled.fitness - (5.0 / 6.0)) < 1e-12);
  assert(std::abs(backwards_underfilled.fitness - 0.75) < 1e-12);

  for (const auto variant : {
           AlignmentVariant::kStateEquationAStar,
           AlignmentVariant::kDijkstraNoHeuristics,
           AlignmentVariant::kDijkstraLessMemory,
           AlignmentVariant::kDiscountedAStar,
           AlignmentVariant::kGeneratorDijkstraNoHeuristics,
           AlignmentVariant::kGeneratorDijkstraLessMemory}) {
    AlignmentOptions alignment;
    alignment.variant = variant;
    alignment.initial_marking = initial;
    alignment.final_marking = final;
    const auto result = check_alignment(suffix, n, alignment);
    assert(result.computed && result.fitness == 1.0 &&
           result.fitting_traces == 1);
  }

  ReplayFitnessOptions fitness;
  fitness.initial_marking = initial;
  fitness.final_marking = final;
  const auto token = evaluate_replay_fitness(
      suffix, n, ReplayFitnessVariant::TOKEN_BASED, fitness);
  const auto aligned = evaluate_replay_fitness(
      suffix, n, ReplayFitnessVariant::ALIGNMENT_BASED, fitness);
  const auto automatic = evaluate_replay_fitness(
      suffix, n, ReplayFitnessVariant::AUTO, fitness);
  assert(token.computed && token.fitness == 1.0);
  assert(aligned.computed && aligned.fitness == 1.0);
  assert(automatic.computed && automatic.fitness == 1.0 &&
         automatic.variant == ReplayFitnessVariant::ALIGNMENT_BASED);

  fitness.decomposed_alignment_options = DecomposedAlignmentOptions{};
  const auto decomposed = evaluate_replay_fitness(
      suffix, n, ReplayFitnessVariant::ALIGNMENT_BASED, fitness);
  assert(decomposed.computed && decomposed.fitness == 1.0 &&
         decomposed.decomposed_alignment_result.has_value());

  ReplayFitnessOptions nested_decomposed;
  nested_decomposed.decomposed_alignment_options =
      DecomposedAlignmentOptions{};
  nested_decomposed.decomposed_alignment_options->initial_marking = {0, 1, 0};
  nested_decomposed.decomposed_alignment_options->final_marking = {0, 0, 1};
  const auto nested_automatic = evaluate_replay_fitness(
      suffix, n, ReplayFitnessVariant::AUTO, nested_decomposed);
  assert(nested_automatic.computed && nested_automatic.fitness == 1.0 &&
         nested_automatic.variant == ReplayFitnessVariant::ALIGNMENT_BASED &&
         nested_automatic.decomposed_alignment_result.has_value());

  for (const auto variant : {
           PrecisionVariant::ETCONFORMANCE_TOKEN,
           PrecisionVariant::ALIGN_ETCONFORMANCE,
           PrecisionVariant::AUTOMATON_AFTER_ALIGN,
           PrecisionVariant::AUTO}) {
    PrecisionOptions precision;
    precision.initial_marking = initial;
    precision.final_marking = final;
    const auto result = evaluate_precision(suffix, n, variant, precision);
    assert(result.computed && result.precision == 1.0);
  }

  GeneralizationOptions generalization;
  generalization.initial_marking = initial;
  generalization.final_marking = final;
  const auto generalized = evaluate_generalization(
      suffix, n, GeneralizationVariant::GENERALIZATION_TOKEN,
      generalization);
  assert(generalized.computed &&
         generalized.transition_frequencies ==
             std::vector<std::uint64_t>({0, 1}));

  CompositeEvaluationOptions composite;
  composite.initial_marking = initial;
  composite.final_marking = final;
  const auto combined = evaluate_composite(suffix, n, composite);
  assert(combined.computed && combined.fitness == 1.0 &&
         combined.precision == 1.0);

  // A non-default final marking must be honored by every evaluator.  The
  // trace stops after A, so the middle place is the requested goal rather
  // than the workflow sink.
  const auto marked_prefix = log({{"A"}});
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
    const auto result = check_alignment(marked_prefix, n, alignment);
    assert(result.computed && result.fitness == 1.0 &&
           result.fitting_traces == 1);
  }

  ReplayFitnessOptions marked_fitness;
  marked_fitness.initial_marking = source_marking;
  marked_fitness.final_marking = middle_marking;
  const auto marked_token = evaluate_replay_fitness(
      marked_prefix, n, ReplayFitnessVariant::TOKEN_BASED, marked_fitness);
  const auto marked_alignment = evaluate_replay_fitness(
      marked_prefix, n, ReplayFitnessVariant::ALIGNMENT_BASED,
      marked_fitness);
  marked_fitness.decomposed_alignment_options =
      DecomposedAlignmentOptions{};
  const auto marked_decomposed = evaluate_replay_fitness(
      marked_prefix, n, ReplayFitnessVariant::ALIGNMENT_BASED,
      marked_fitness);
  assert(marked_token.computed && marked_alignment.computed &&
         marked_decomposed.computed && marked_token.fitness == 1.0 &&
         marked_alignment.fitness == 1.0 && marked_decomposed.fitness == 1.0);

  for (const auto variant : {
           PrecisionVariant::ETCONFORMANCE_TOKEN,
           PrecisionVariant::ALIGN_ETCONFORMANCE,
           PrecisionVariant::AUTOMATON_AFTER_ALIGN,
           PrecisionVariant::AUTO}) {
    PrecisionOptions precision_options;
    precision_options.initial_marking = source_marking;
    precision_options.final_marking = middle_marking;
    const auto result = evaluate_precision(
        marked_prefix, n, variant, precision_options);
    assert(result.computed && result.precision == 1.0);
  }

  GeneralizationOptions marked_generalization;
  marked_generalization.initial_marking = source_marking;
  marked_generalization.final_marking = middle_marking;
  const auto marked_generalization_result = evaluate_generalization(
      marked_prefix, n, GeneralizationVariant::GENERALIZATION_TOKEN,
      marked_generalization);
  assert(marked_generalization_result.computed &&
         marked_generalization_result.transition_frequencies ==
             std::vector<std::uint64_t>({1, 0}));

  CompositeEvaluationOptions marked_composite;
  marked_composite.initial_marking = source_marking;
  marked_composite.final_marking = middle_marking;
  const auto marked_composite_result =
      evaluate_composite(marked_prefix, n, marked_composite);
  assert(marked_composite_result.computed &&
         marked_composite_result.fitness == 1.0 &&
         marked_composite_result.precision == 1.0);
}

void evaluators_reject_invalid_markings_and_variants() {
  const auto n = sequence_net();
  const auto l = log({{"A", "B"}});

  ReplayFitnessOptions bad_marking;
  bad_marking.initial_marking = {1, -1, 0};
  const auto invalid_marking = evaluate_replay_fitness(
      EventLog{}, n, ReplayFitnessVariant::TOKEN_BASED, bad_marking);
  assert(!invalid_marking.computed && !invalid_marking.error.empty());

  PrecisionOptions short_marking;
  short_marking.final_marking = {0, 1};
  const auto invalid_precision = evaluate_precision(
      l, n, PrecisionVariant::ETCONFORMANCE_TOKEN, short_marking);
  assert(!invalid_precision.computed && !invalid_precision.error.empty());

  bool replay_threw = false;
  try {
    TokenReplayOptions invalid;
    invalid.initial_marking = {1, 0};
    (void)check_conformance(l, n, invalid);
  } catch (const std::invalid_argument&) {
    replay_threw = true;
  }
  assert(replay_threw);

  AlignmentOptions invalid_alignment;
  invalid_alignment.final_marking = {0, -1, 1};
  assert(!check_alignment(l, n, invalid_alignment).computed);

  const auto bad_fitness = evaluate_replay_fitness(
      l, n, static_cast<ReplayFitnessVariant>(999));
  const auto bad_precision = evaluate_precision(
      l, n, static_cast<PrecisionVariant>(999));
  const auto bad_generalization = evaluate_generalization(
      l, n, static_cast<GeneralizationVariant>(999));
  const auto bad_simplicity = evaluate_simplicity(
      n, static_cast<SimplicityVariant>(999));
  assert(!bad_fitness.computed && !bad_fitness.error.empty());
  assert(!bad_precision.computed && !bad_precision.error.empty());
  assert(!bad_generalization.computed && !bad_generalization.error.empty());
  assert(!bad_simplicity.computed && !bad_simplicity.error.empty());

  ReplayFitnessOptions bad_replay_engine;
  bad_replay_engine.token_replay_variant =
      static_cast<TokenReplayVariant>(999);
  const auto bad_engine = evaluate_replay_fitness(
      l, n, ReplayFitnessVariant::TOKEN_BASED, bad_replay_engine);
  assert(!bad_engine.computed && !bad_engine.error.empty());
}

void empty_replay_fitness_preserves_the_selected_engine() {
  const auto n = sequence_net();
  const EventLog empty_log;
  const DirectlyFollowsGraph empty_graph;

  ReplayFitnessOptions ordinary;
  ordinary.activity_attribute = "phase";
  ordinary.alignment_options.variant =
      AlignmentVariant::kDijkstraNoHeuristics;
  const auto ordinary_result = evaluate_replay_fitness(
      empty_log, n, ReplayFitnessVariant::ALIGNMENT_BASED, ordinary);
  assert(ordinary_result.fitness == 0.0);
  assert(ordinary_result.alignment_result.variant ==
         AlignmentVariant::kDijkstraNoHeuristics);
  assert(ordinary_result.alignment_result.activity_attribute == "phase");
  const auto ordinary_json = replay_fitness_json(ordinary_result, &empty_graph);
  assert(ordinary_json.find("\"variant\": \"dijkstra_no_heuristics\"") !=
         std::string::npos);
  assert(ordinary_json.find("\"activity_attribute\": \"phase\"") !=
         std::string::npos);

  ReplayFitnessOptions decomposed;
  decomposed.decomposed_alignment_options = DecomposedAlignmentOptions{};
  const auto decomposed_result = evaluate_replay_fitness(
      empty_log, n, ReplayFitnessVariant::ALIGNMENT_BASED, decomposed);
  assert(decomposed_result.fitness == 0.0);
  assert(decomposed_result.decomposed_alignment_result.has_value());
  assert(decomposed_result.decomposed_alignment_result->trace_count == 0);
  const auto decomposed_json = replay_fitness_json(decomposed_result);
  assert(decomposed_json.find("\"variant\":\"recompos_maximal\"") !=
         std::string::npos);
  assert(decomposed_json.find("dijkstra_less_memory") == std::string::npos);
}

void empty_replay_fitness_rejects_invalid_selected_engine_options() {
  const auto n = sequence_net();
  const EventLog empty_log;

  ReplayFitnessOptions invalid_alignment;
  invalid_alignment.alignment_options.variant =
      static_cast<AlignmentVariant>(999);
  auto result = evaluate_replay_fitness(
      empty_log, n, ReplayFitnessVariant::ALIGNMENT_BASED,
      invalid_alignment);
  assert(!result.computed && !result.error.empty());

  invalid_alignment = {};
  invalid_alignment.alignment_options.cost_profile =
      static_cast<AlignmentCostProfile>(999);
  result = evaluate_replay_fitness(
      empty_log, n, ReplayFitnessVariant::ALIGNMENT_BASED,
      invalid_alignment);
  assert(!result.computed && !result.error.empty());

  invalid_alignment = {};
  invalid_alignment.alignment_options.default_trace_cost = -1.0;
  result = evaluate_replay_fitness(
      empty_log, n, ReplayFitnessVariant::ALIGNMENT_BASED,
      invalid_alignment);
  assert(!result.computed && !result.error.empty());

  ReplayFitnessOptions invalid_decomposed;
  invalid_decomposed.decomposed_alignment_options =
      DecomposedAlignmentOptions{};
  invalid_decomposed.decomposed_alignment_options->variant =
      static_cast<DecomposedAlignmentVariant>(999);
  result = evaluate_replay_fitness(
      empty_log, n, ReplayFitnessVariant::ALIGNMENT_BASED,
      invalid_decomposed);
  assert(!result.computed && !result.error.empty());

  invalid_decomposed = {};
  invalid_decomposed.decomposed_alignment_options =
      DecomposedAlignmentOptions{};
  invalid_decomposed.decomposed_alignment_options->max_states = 0;
  result = evaluate_replay_fitness(
      empty_log, n, ReplayFitnessVariant::ALIGNMENT_BASED,
      invalid_decomposed);
  assert(!result.computed && !result.error.empty());

  invalid_decomposed = {};
  invalid_decomposed.decomposed_alignment_options =
      DecomposedAlignmentOptions{};
  invalid_decomposed.decomposed_alignment_options->log_move_cost = -1.0;
  result = evaluate_replay_fitness(
      empty_log, n, ReplayFitnessVariant::ALIGNMENT_BASED,
      invalid_decomposed);
  assert(!result.computed && !result.error.empty());
}

void replay_fitness_selects_backwards_token_replay() {
  PetriNet n;
  const auto source = n.add_place();
  const auto middle = n.add_place();
  const auto sink = n.add_place();
  n.set_source(source);
  n.set_sink(sink);
  n.set_activities({"A"});
  n.add_transition(0, {source}, {middle});
  n.add_transition(0, {middle}, {sink});

  ReplayFitnessOptions forward;
  const auto forward_result = evaluate_replay_fitness(
      log({{"A"}}), n, ReplayFitnessVariant::TOKEN_BASED, forward);
  ReplayFitnessOptions backwards;
  backwards.token_replay_variant = TokenReplayVariant::kBackwards;
  const auto backwards_result = evaluate_replay_fitness(
      log({{"A"}}), n, ReplayFitnessVariant::TOKEN_BASED, backwards);
  assert(forward_result.fitting_traces == 0);
  assert(backwards_result.fitting_traces == 1);
}

void token_precision_stops_at_an_unfit_prefix() {
  PetriNet n;
  const auto source = n.add_place();
  const auto disconnected = n.add_place();
  const auto sink = n.add_place();
  n.set_source(source);
  n.set_sink(sink);
  n.set_activities({"A", "X", "B"});
  n.add_transition(0, {source}, {sink});
  n.add_transition(1, {disconnected}, {disconnected});
  n.add_transition(2, {disconnected}, {sink});

  const auto result = evaluate_precision(
      log({{"A"}, {"X", "B"}}), n,
      PrecisionVariant::ETCONFORMANCE_TOKEN);
  assert(result.computed && std::abs(result.precision - 1.0) < 1e-12);
  // Prefix precision does not publish unevaluated full-log replay counts.
  assert(result.token_result_is_precision_only);
  assert(result.token_result.transition_frequencies.empty());
}

void precision_option_errors_preserve_variant_and_convert_trees() {
  PrecisionOptions invalid;
  invalid.max_states=0;
  auto net_result=evaluate_precision(
      log({{"A"}}), sequence_net(),
      PrecisionVariant::AUTOMATON_AFTER_ALIGN, invalid);
  assert(!net_result.computed && net_result.exhausted);
  assert(net_result.variant==PrecisionVariant::AUTOMATON_AFTER_ALIGN);

  ProcessTree tree({"A"}, ProcessTreeNode::leaf(0));
  auto tree_result=evaluate_precision(
      log({{"A"}}), tree, PrecisionVariant::ALIGN_ETCONFORMANCE, invalid);
  assert(!tree_result.computed && tree_result.exhausted);
  assert(tree_result.variant==PrecisionVariant::ALIGN_ETCONFORMANCE);
}

void replay_fitness_excludes_unknown_activities_by_default() {
  auto n = sequence_net();
  // A declared activity without an attached visible transition is still
  // unknown to replay and must not be mistaken for model behavior.
  n.set_activities({"A", "B", "UNUSED"});
  const auto l = log({{"A", "B", "UNUSED", "X"}, {"A", "B"}});

  // The evaluator's default matches token-replay fitness semantics: unknown
  // activities do not make an otherwise fitting trace unfit.
  const auto excluded = evaluate_replay_fitness(
      l, n, ReplayFitnessVariant::TOKEN_BASED);
  assert(excluded.computed && excluded.trace_count == 2);
  assert(excluded.fitness == 1.0 && excluded.fitting_traces == 2);

  ReplayFitnessOptions include_unknown;
  include_unknown.consider_activities_not_in_model_in_fitness = true;
  const auto included = evaluate_replay_fitness(
      l, n, ReplayFitnessVariant::TOKEN_BASED, include_unknown);
  assert(included.computed && included.trace_count == 2);
  // The opt-in policy changes fit classification, not token fitness.
  assert(included.fitness == excluded.fitness);
  assert(included.fitting_traces == 1);

  // The generic API uses the same default: unknown labels are not token moves.
  const auto generic = check_conformance(l, n);
  assert(generic.fitness == excluded.fitness && generic.fitting_traces == 2);
}

void composite_evaluation_uses_equal_and_custom_weights() {
  const auto n = sequence_net();
  const auto l = log({{"A", "B"}, {"A"}});
  const auto fitness = evaluate_replay_fitness(l, n, ReplayFitnessVariant::TOKEN_BASED);
  const auto precision = evaluate_precision(l, n, PrecisionVariant::ETCONFORMANCE_TOKEN);
  const auto generalization = evaluate_generalization(l, n);
  const auto simplicity = evaluate_simplicity(n, SimplicityVariant::ARC_DEGREE);
  const auto equal = evaluate_composite(l, n);
  assert(equal.computed);
  assert(std::abs(equal.metrics_average_weight -
                  (equal.fitness + equal.precision + equal.generalization +
                   equal.simplicity) / 4.0) < 1e-12);
  assert(std::abs(equal.fitness - fitness.log_fitness) < 1e-12);
  assert(std::abs(equal.precision - precision.precision) < 1e-12);
  assert(std::abs(equal.generalization - generalization.generalization) < 1e-12);
  assert(std::abs(equal.simplicity - simplicity.simplicity) < 1e-12);

  CompositeEvaluationOptions custom;
  custom.fitness_weight = 2.0;
  custom.precision_weight = 1.0;
  custom.simplicity_weight = 0.0;
  custom.generalization_weight = 0.0;
  const auto weighted = evaluate_composite(l, n, custom);
  assert(weighted.computed);
  assert(std::abs(weighted.fitness_weight - 2.0 / 3.0) < 1e-12);
  assert(std::abs(weighted.precision_weight - 1.0 / 3.0) < 1e-12);
  assert(std::abs(weighted.metrics_average_weight -
                  (2.0 * weighted.fitness + weighted.precision) / 3.0) <
         1e-12);

  CompositeEvaluationOptions invalid;
  invalid.fitness_weight = invalid.precision_weight =
      invalid.simplicity_weight = invalid.generalization_weight = 0.0;
  const auto rejected = evaluate_composite(l, n, invalid);
  assert(!rejected.computed && !rejected.error.empty());
}

void composite_evaluation_serializes_as_a_model_family() {
  CompositeEvaluationResult result;
  result.fitness = 0.9;
  result.precision = 0.8;
  result.generalization = 0.7;
  result.simplicity = 0.6;
  result.metrics_average_weight = 0.75;
  result.fscore = 0.8470588235;
  result.fitness_weight = 0.5;
  result.precision_weight = 0.25;
  result.generalization_weight = 0.15;
  result.simplicity_weight = 0.10;
  ModelEvaluation model;
  model.miner = "composite-test";
  model.composite = result;
  const auto json = model_evaluation_json(model);
  assert(json.find("\"composite\"") != std::string::npos);
  assert(json.find("\"metrics_average_weight\": 0.75") !=
         std::string::npos);
  assert(json.find("\"fscore\"") != std::string::npos);
  assert(json.find("\"weights\"") != std::string::npos);
}

void emd_pyemd_variant_is_deterministic_and_strict() {
  StochasticLanguage left{{{{"A"}, 0.25},
                           {{"A"}, 0.25},
                           {{"B"}, 0.5}},
                          1.0};
  StochasticLanguage right{{{{"A"}, 0.5}, {{"C"}, 0.5}}, 1.0};

  EmdOptions pyemd;
  pyemd.variant = EmdVariant::PYEMD;
  // A caller-supplied native distance function is honored by the explicit
  // selector.
  pyemd.distance = [](const auto&, const auto&) { return 0.0; };
  const auto first = evaluate_emd(left, right, pyemd);
  const auto second = evaluate_emd(left, right, pyemd);
  assert(first.computed && first.variant == EmdVariant::PYEMD);
  assert(first.use_fast_emd);
  assert(first.distance == 0.0);
  assert(first.distance == second.distance);

  EmdOptions default_distance = pyemd;
  default_distance.distance = {};
  const auto default_distance_result = evaluate_emd(
      left, right, default_distance);
  assert(default_distance_result.computed);
  assert(std::abs(default_distance_result.distance - 0.5) < 1e-12);

  // Both backend selections remain deterministic when the optional fast
  // runtime is unavailable, and selecting the fallback does not alter the
  // distance.
  EmdOptions fallback = pyemd;
  fallback.use_fast_emd = false;
  const auto fallback_result = evaluate_emd(left, right, fallback);
  assert(fallback_result.computed && !fallback_result.use_fast_emd);
  assert(fallback_result.distance == first.distance);

  const auto empty = evaluate_emd({}, {}, pyemd);
  assert(empty.computed && empty.variant == EmdVariant::PYEMD &&
         empty.distance == 0.0);

  StochasticLanguage unequal{{{{"A"}, 2.0}}, 2.0};
  EmdOptions strict = pyemd;
  strict.strict_equal_mass = true;
  const auto rejected = evaluate_emd(unequal, right, strict);
  assert(!rejected.computed && rejected.variant == EmdVariant::PYEMD &&
         rejected.error == "masses must be equal");
  strict.strict_equal_mass = false;
  strict.normalize_masses = true;
  const auto normalized = evaluate_emd(unequal, right, strict);
  assert(normalized.computed && normalized.normalized);

  StochasticLanguage close_left{{{{"A"}, 1.000009}}, 1.000009};
  StochasticLanguage close_right{{{{"A"}, 1.0}}, 1.0};
  EmdOptions close_strict = pyemd;
  close_strict.strict_equal_mass = true;
  const auto close_result = evaluate_emd(
      close_left, close_right, close_strict);
  assert(close_result.computed && !close_result.normalized &&
         close_result.distance == 0.0);

  // The fallback implementation has the equal-total precondition of the
  // reference solver; the accelerated path performs the normalization above.
  EmdOptions fallback_unequal = pyemd;
  fallback_unequal.use_fast_emd = false;
  const auto fallback_rejected = evaluate_emd(
      unequal, right, fallback_unequal);
  assert(!fallback_rejected.computed &&
         fallback_rejected.error == "masses must be equal");

  StochasticLanguage negative_duplicate{{{{"A"}, -1.0}, {{"A"}, 1.0}}, 0.0};
  const auto invalid_weight = evaluate_emd(negative_duplicate, right, pyemd);
  assert(!invalid_weight.computed && invalid_weight.error == "invalid language weight");

  EmdOptions invalid_cost;
  invalid_cost.distance = [](const auto&, const auto&) {
    return std::numeric_limits<double>::quiet_NaN();
  };
  const auto rejected_cost = evaluate_emd(left, right, invalid_cost);
  assert(!rejected_cost.computed && rejected_cost.error == "invalid distance");

  EmdOptions invalid_variant;
  invalid_variant.variant = static_cast<EmdVariant>(999);
  const auto invalid = evaluate_emd(left, right, invalid_variant);
  assert(!invalid.computed && invalid.error == "unsupported EMD variant");

  const auto json = emd_json(first);
  assert(json.find("\"variant\": \"pyemd\"") != std::string::npos);
  assert(json.find("\"use_fast_emd\": true") != std::string::npos);
  const auto fallback_json = emd_json(fallback_result);
  assert(fallback_json.find("\"use_fast_emd\": false") != std::string::npos);
}

int main() {
  auto n=sequence_net(); auto l=log({{"A","B"},{"A"}});
  auto f=evaluate_replay_fitness(l,n,ReplayFitnessVariant::TOKEN_BASED); assert(f.trace_count==2&&f.fitness<1&&f.percentage_fit_traces==50);
  auto p=evaluate_precision(l,n,PrecisionVariant::ETCONFORMANCE_TOKEN); assert(p.computed&&p.precision>=0&&p.precision<=1);
  auto g=evaluate_generalization(l,n); assert(g.generalization>=0&&g.generalization<=1);
  auto s=evaluate_simplicity(n,SimplicityVariant::EXTENDED_CYCLOMATIC); assert(s.computed&&s.edges>=s.vertices-1);
  auto empty=evaluate_replay_fitness(EventLog{},n); assert(empty.fitness==0&&empty.percentage_fit_traces==0);
  auto a=derive_stochastic_language(log({{"A"},{"B"}}),true); auto b=derive_stochastic_language(log({{"A"}}),true);
  auto e=evaluate_emd(a,b); assert(e.computed&&std::isfinite(e.distance));
  StochasticLanguage x{{{{"x"},0.5},{{"y"},0.5}},1.0};
  StochasticLanguage y{{{{"u"},0.5},{{"v"},0.5}},1.0};
  EmdOptions matrix; matrix.distance=[](const auto& p,const auto& q){
    if(p[0]=="x"&&q[0]=="u") return 0.0; if(p[0]=="x"&&q[0]=="v") return 1.0;
    if(p[0]=="y"&&q[0]=="u") return 1.0; return 100.0; };
  // Greedy cheapest-edge selection would cost 50 here; exact transport is 1.
  assert(std::abs(earth_movers_distance(x,y,matrix)-1.0)<1e-9);
  EmdOptions strict; strict.strict_equal_mass=true; StochasticLanguage unequal{{{{"x"},2.0}},2.0};
  auto bad=evaluate_emd(unequal,a,strict); assert(!bad.computed);
  auto card=evaluate_simplicity(n,SimplicityVariant::EXTENDED_CARDOSO); assert(card.computed&&card.complexity>0);
  weighted_input_enablement(); cardoso_deduplicates_weighted_outputs(); cyclomatic_counts_parallel_transition_edges();
  unexecuted_transition_penalty();
  generalization_counts_silent_routing_and_unseen_branches();
  projection_records_have_place_markings();
  automaton_model_only_projection(); automaton_ignores_log_only_moves();
  automaton_unions_shared_prefix_observations(); automaton_excludes_short_terminal_prefix_frequency();
  automaton_task_ids_disambiguate_duplicate_labels(); automaton_differs_from_alignment_precision_on_deviation();
  alignment_precision_excludes_unfit_prefixes();
  cyclomatic_default_multiple_empty_preset_places(); default_variant_dispatch_follows_soundness();
  simplicity_counts_extended_arcs_and_accepts_finite_baselines();
  precision_option_errors_preserve_variant_and_convert_trees();
  replay_fitness_excludes_unknown_activities_by_default();
  evaluator_selectors_reach_every_native_engine();
  marked_net_options_reach_every_evaluator_engine();
  evaluators_reject_invalid_markings_and_variants();
  empty_replay_fitness_preserves_the_selected_engine();
  empty_replay_fitness_rejects_invalid_selected_engine_options();
  replay_fitness_selects_backwards_token_replay();
  token_precision_stops_at_an_unfit_prefix();
  composite_evaluation_uses_equal_and_custom_weights();
  composite_evaluation_serializes_as_a_model_family();
  emd_pyemd_variant_is_deterministic_and_strict();
  precomputed_json_keeps_native_aggregation();
  flat_document_fitness_preserves_markings_and_case_selection();
}
