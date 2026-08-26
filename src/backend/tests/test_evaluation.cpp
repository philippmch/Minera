#include "minera/evaluation.hpp"
#include "minera/json_export.hpp"
#include <cassert>
#include <cmath>
#include <limits>

using namespace minera;

static PetriNet sequence_net() {
  PetriNet n; auto p0=n.add_place(), p1=n.add_place(), p2=n.add_place(); n.set_source(p0); n.set_sink(p2); n.set_activities({"A","B"});
  n.add_transition(0,{p0},{p1}); n.add_transition(1,{p1},{p2}); return n;
}
static EventLog log(std::initializer_list<std::initializer_list<const char*>> xs) { EventLog l; for(auto tr:xs){Trace t;for(auto a:tr)t.events.push_back(Event{"",a});l.traces.push_back(std::move(t));}return l; }

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
void cyclomatic_retains_parallel_transition_edges() {
  PetriNet n; auto p=n.add_place(),q=n.add_place(); n.set_source(p); n.set_sink(q); n.set_activities({"A","B"}); n.add_transition(0,{p},{q}); n.add_transition(1,{p},{q});
  auto s=evaluate_simplicity(n,SimplicityVariant::EXTENDED_CYCLOMATIC); assert(s.edges==2);
}
void unexecuted_transition_penalty() {
  auto n=sequence_net(); auto r=evaluate_generalization(log({{"A"}}),n);
  assert(r.executed_transitions==1 && r.transition_frequencies.size()==2);
  assert(r.transition_frequencies[0]==1 && r.transition_frequencies[1]==0);
  // One seen and one unseen transition each contribute a unit rarity penalty.
  assert(std::abs(r.generalization-0.0)<1e-12);
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
  assert(std::abs(a.precision-0.5)<1e-12);
  assert(std::abs(p.precision-1.0)<1e-12);
}

void cyclomatic_default_multiple_empty_preset_places() {
  PetriNet n; auto p0=n.add_place(), p1=n.add_place(), sink=n.add_place();
  n.set_source(p0); n.set_sink(sink); n.set_activities({"A","B"});
  n.add_transition(0,{p0},{sink}); n.add_transition(1,{p1},{sink});
  auto s=evaluate_simplicity(n,SimplicityVariant::EXTENDED_CYCLOMATIC);
  assert(s.computed && s.vertices==4 && s.edges==4 && s.strongly_connected_components==4);
  assert(std::abs(s.value-4.0)<1e-12);
}

void default_variant_dispatch_follows_soundness() {
  auto sound=sequence_net();
  auto fit=evaluate_replay_fitness(log({{"A","B"}}),sound);
  auto prec=evaluate_precision(log({{"A","B"}}),sound);
  assert(fit.variant==ReplayFitnessVariant::ALIGNMENT_BASED);
  assert(prec.variant==PrecisionVariant::ALIGN_ETCONFORMANCE);

  PetriNet unsound; auto p0=unsound.add_place(), dead=unsound.add_place(), sink=unsound.add_place();
  unsound.set_source(p0); unsound.set_sink(sink); unsound.set_activities({"A"});
  unsound.add_transition(0,{p0},{dead}); // sink is unreachable
  auto uf=evaluate_replay_fitness(log({{"A"}}),unsound);
  auto up=evaluate_precision(log({{"A"}}),unsound);
  assert(uf.variant==ReplayFitnessVariant::TOKEN_BASED);
  assert(up.variant==PrecisionVariant::ETCONFORMANCE_TOKEN);
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
  // The explicit selector owns its normalized trace costs and does not
  // silently switch to a caller's unrelated distance function.
  pyemd.distance = [](const auto&, const auto&) { return 0.0; };
  const auto first = evaluate_emd(left, right, pyemd);
  const auto second = evaluate_emd(left, right, pyemd);
  assert(first.computed && first.variant == EmdVariant::PYEMD);
  assert(first.use_fast_emd);
  assert(std::abs(first.distance - 0.5) < 1e-12);
  assert(first.distance == second.distance);

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
  weighted_input_enablement(); cardoso_deduplicates_weighted_outputs(); cyclomatic_retains_parallel_transition_edges();
  unexecuted_transition_penalty(); projection_records_have_place_markings();
  automaton_model_only_projection(); automaton_ignores_log_only_moves();
  automaton_unions_shared_prefix_observations(); automaton_excludes_short_terminal_prefix_frequency();
  automaton_task_ids_disambiguate_duplicate_labels(); automaton_differs_from_alignment_precision_on_deviation();
  cyclomatic_default_multiple_empty_preset_places(); default_variant_dispatch_follows_soundness();
  precision_option_errors_preserve_variant_and_convert_trees();
  composite_evaluation_uses_equal_and_custom_weights();
  composite_evaluation_serializes_as_a_model_family();
  emd_pyemd_variant_is_deterministic_and_strict();
}
