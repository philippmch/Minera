#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "minera/conformance.hpp"

namespace {
minera::EventLog log_of(std::initializer_list<std::string> activities) {
  minera::Trace trace;
  trace.case_id = "1";
  std::size_t i = 0;
  for (const auto& activity : activities)
    trace.events.push_back(minera::Event{"1", activity, {}, {}, i++});
  return minera::EventLog{{std::move(trace)}};
}

minera::ProcessTree chain() {
  return minera::ProcessTree(
      {"a", "b"},
      {minera::ProcessTreeOperator::kSequence, 0,
       {minera::ProcessTreeNode::leaf(0), minera::ProcessTreeNode::leaf(1)}});
}
}

int main() {
  const auto model = chain();
  const auto exact = log_of({"a", "b"});
  for (const auto variant : {minera::AlignmentVariant::kDijkstraNoHeuristics,
                             minera::AlignmentVariant::kDijkstraLessMemory,
                             minera::AlignmentVariant::kDiscountedAStar,
                             minera::AlignmentVariant::kGeneratorDijkstraNoHeuristics,
                             minera::AlignmentVariant::kGeneratorDijkstraLessMemory}) {
    minera::AlignmentOptions options;
    options.variant = variant;
    const auto result = minera::check_alignment(exact, model, options);
  assert(result.computed);
  assert(result.total_cost < 1e-9);
  assert(result.diagnostics.visited_states > 0);
    assert(result.diagnostics.queued_states > 0);
    assert(result.diagnostics.traversed_arcs > 0);
    assert(!result.variants.empty());
  }
  minera::AlignmentOptions state_equation;
  state_equation.variant = minera::AlignmentVariant::kStateEquationAStar;
  state_equation.max_states = 199999;
  const auto lp_result = minera::check_alignment(exact, model, state_equation);
  assert(lp_result.diagnostics.lp_solved > 0);
  const auto lp_prefix = minera::check_alignment(log_of({"a"}), model, state_equation);
  assert(lp_prefix.computed && std::abs(lp_prefix.total_cost - 1.0) < 1e-9);
  const minera::ProcessTree tau_model({}, minera::ProcessTreeNode::tau());
  state_equation.trace_costs_by_position = {0.25};
  const auto tau_alignment = minera::check_alignment(log_of({"unknown"}), tau_model,
                                                      state_equation);
  assert(tau_alignment.computed && std::abs(tau_alignment.total_cost - 0.25) < 1e-9);
  assert(std::any_of(tau_alignment.variants.front().moves.begin(),
                     tau_alignment.variants.front().moves.end(),
                     [](const auto& move) {
                       return move.type == minera::AlignmentMoveType::kModel &&
                              move.activity.empty();
                     }));

  // Custom per-position and per-transition costs are fractional and repeated
  // labels can be addressed independently through callbacks/vectors.
  const auto deviating = log_of({"a", "z"});
  minera::AlignmentOptions custom;
  custom.variant = minera::AlignmentVariant::kDijkstraNoHeuristics;
  custom.trace_costs_by_position = {0.25, 0.75};
  custom.model_costs_by_transition = {2.5, 3.5};
  const auto custom_result = minera::check_alignment(deviating, model, custom);
  assert(custom_result.computed);
  assert(std::abs(custom_result.total_cost - 4.25) < 1e-9);

  // PM4Py's standard profile is explicitly selectable and charges visible
  // deviations at 10000 while silent routing costs one.
  minera::AlignmentOptions pm;
  pm.variant = minera::AlignmentVariant::kDijkstraNoHeuristics;
  pm.cost_profile = minera::AlignmentCostProfile::kPm4PyStandard;
  const auto pm_result = minera::check_alignment(log_of({"a"}), model, pm);
  assert(pm_result.computed);
  assert(pm_result.total_cost >= 10000.0);
  pm.default_trace_cost = 7.0;
  pm.default_model_cost = 11.0;
  pm.default_silent_cost = 3.0;
  const auto overridden = minera::check_alignment(log_of({"a"}), model, pm);
  assert(overridden.computed);
  assert(overridden.total_cost >= 11.0 && overridden.total_cost < 100.0);

  minera::AlignmentOptions discounted;
  discounted.variant = minera::AlignmentVariant::kDiscountedAStar;
  discounted.exponent = 2.0;
  const auto d0 = minera::check_alignment(log_of({"a", "z"}), model, discounted);
  discounted.synchronous_dijkstra = true;
  const auto d1 = minera::check_alignment(log_of({"a", "z"}), model, discounted);
  assert(d0.computed && d1.computed);
  assert(std::abs(d0.total_cost - d1.total_cost) < 1e-9);
  const auto late = minera::check_alignment(log_of({"a", "b", "z"}), model, discounted);
  const auto early = minera::check_alignment(log_of({"z", "a", "b"}), model, discounted);
  assert(late.computed && early.computed);
  assert(std::abs(late.total_cost - 0.25) < 1e-9);
  assert(std::abs(early.total_cost - 1.0) < 1e-9);

  // A small state cap reports exhaustion rather than returning a fake optimum.
  minera::AlignmentOptions capped;
  capped.variant = minera::AlignmentVariant::kDijkstraNoHeuristics;
  capped.max_states = 1;
  const auto capped_result = minera::check_alignment(exact, model, capped);
  assert(!capped_result.computed && capped_result.exhausted);

  minera::AlignmentOptions timed;
  timed.variant = minera::AlignmentVariant::kDijkstraNoHeuristics;
  timed.max_total_time_seconds = 1e-12;
  const auto timed_result = minera::check_alignment(exact, model, timed);
  assert(!timed_result.computed && timed_result.exhausted);

  // Generator variants expose a deduplicated optimal path collection.
  minera::AlignmentOptions generator;
  generator.variant = minera::AlignmentVariant::kGeneratorDijkstraNoHeuristics;
  generator.max_alignments = 8;
  const auto generated = minera::check_alignment(exact, model, generator);
  assert(generated.computed);
  assert(generated.variants.front().optimal_alignments.size() >= 1);
  const minera::ProcessTree tied(
      {"a"},
      {minera::ProcessTreeOperator::kExclusive, 0,
       {minera::ProcessTreeNode::leaf(0), minera::ProcessTreeNode::leaf(0)}});
  const auto tie_result = minera::check_alignment(log_of({"a"}), tied, generator);
  assert(tie_result.computed);
  assert(tie_result.variants.front().optimal_alignments.size() >= 2);
  generator.variant = minera::AlignmentVariant::kGeneratorDijkstraLessMemory;
  const auto tie_less = minera::check_alignment(log_of({"a"}), tied, generator);
  assert(tie_less.computed);
  assert(tie_less.variants.front().optimal_alignments.size() >= 2);
  minera::AlignmentOptions sync_mode = generator;
  sync_mode.variant = minera::AlignmentVariant::kDiscountedAStar;
  sync_mode.synchronous_dijkstra = true;
  const auto sync_compact = minera::check_alignment(log_of({"a"}), tied, sync_mode);
  sync_mode.synchronous_dijkstra = false;
  const auto sync_explicit = minera::check_alignment(log_of({"a"}), tied, sync_mode);
  assert(sync_compact.computed && sync_explicit.computed);
  assert(sync_compact.variants.front().moves.size() == sync_explicit.variants.front().moves.size());

  // Weighted input arcs require multiplicity; one source token cannot fire a
  // transition consuming the same place twice, so the final marking is unsound.
  minera::PetriNet weighted;
  weighted.add_place(); weighted.add_place();
  weighted.set_source(0); weighted.set_sink(1); weighted.set_activities({"a"});
  weighted.add_transition(0, {0, 0}, {1});
  minera::AlignmentOptions weighted_options;
  weighted_options.variant = minera::AlignmentVariant::kDijkstraNoHeuristics;
  const auto weighted_result = minera::check_alignment(log_of({"a"}), weighted,
                                                        weighted_options);
  assert(!weighted_result.computed && !weighted_result.exhausted);

  minera::AlignmentOptions invalid;
  invalid.variant = minera::AlignmentVariant::kDijkstraNoHeuristics;
  invalid.trace_costs_by_position = {-1.0};
  const auto invalid_result = minera::check_alignment(log_of({"a"}), model, invalid);
  assert(!invalid_result.computed);
  minera::AlignmentOptions timeout;
  timeout.variant = minera::AlignmentVariant::kDijkstraNoHeuristics;
  timeout.max_time_seconds = 1e-12;
  const auto timeout_result = minera::check_alignment(log_of({"a", "b"}), model, timeout);
  assert(!timeout_result.computed && timeout_result.exhausted);

  minera::AlignmentOptions nan_cost;
  nan_cost.variant = minera::AlignmentVariant::kDijkstraNoHeuristics;
  nan_cost.trace_cost_function = [](const std::string&, std::size_t) {
    return std::numeric_limits<double>::quiet_NaN();
  };
  const auto nan_result = minera::check_alignment(log_of({"a"}), model, nan_cost);
  assert(!nan_result.computed);
  int callback_calls = 0;
  minera::AlignmentOptions stateful_cost;
  stateful_cost.variant = minera::AlignmentVariant::kDijkstraNoHeuristics;
  stateful_cost.trace_cost_function = [&](const std::string&, std::size_t) {
    ++callback_calls;
    return 0.5;
  };
  const auto stateful_result = minera::check_alignment(log_of({"a"}), model, stateful_cost);
  assert(stateful_result.computed && callback_calls == 1);
  assert(std::abs(stateful_result.variants.front().fitness -
                  (1.0 - stateful_result.variants.front().cost / 2.5)) < 1e-9);

  minera::AlignmentOptions custom_less;
  custom_less.variant = minera::AlignmentVariant::kDijkstraLessMemory;
  custom_less.trace_costs_by_position = {0.3, 0.7};
  const auto custom_less_result = minera::check_alignment(deviating, model, custom_less);
  custom_less.variant = minera::AlignmentVariant::kDijkstraNoHeuristics;
  const auto custom_full_result = minera::check_alignment(deviating, model, custom_less);
  assert(custom_less_result.computed && custom_full_result.computed);
  assert(std::abs(custom_less_result.total_cost - custom_full_result.total_cost) < 1e-9);

  std::vector<std::string> long_names;
  std::vector<minera::ProcessTreeNode> long_children;
  std::vector<std::string> long_events;
  for (std::size_t i = 0; i < 40; ++i) {
    long_names.push_back("x" + std::to_string(i));
    long_children.push_back(minera::ProcessTreeNode::leaf(i));
    long_events.push_back(long_names.back());
  }
  const minera::ProcessTree long_tree(
      long_names,
      {minera::ProcessTreeOperator::kSequence, 0, std::move(long_children)});
  minera::Trace long_trace;
  long_trace.case_id = "long";
  for (std::size_t i = 0; i < long_events.size(); ++i)
    long_trace.events.push_back(minera::Event{"long", long_events[i], {}, {}, i});
  const auto long_log = minera::EventLog{{std::move(long_trace)}};
  minera::AlignmentOptions long_options;
  long_options.variant = minera::AlignmentVariant::kDijkstraNoHeuristics;
  const auto long_result = minera::check_alignment(long_log, long_tree, long_options);
  assert(long_result.computed && long_result.total_cost == 0.0);

  minera::PetriNet cycle;
  cycle.add_place(); cycle.add_place(); cycle.add_place();
  cycle.set_source(0); cycle.set_sink(2); cycle.set_activities({"a"});
  cycle.add_transition(std::nullopt, {0}, {1});
  cycle.add_transition(std::nullopt, {1}, {1});
  cycle.add_transition(0, {1}, {2});
  minera::AlignmentOptions cycle_options;
  cycle_options.variant = minera::AlignmentVariant::kGeneratorDijkstraNoHeuristics;
  cycle_options.max_alignments = 8;
  const auto cycle_result = minera::check_alignment(log_of({"a"}), cycle, cycle_options);
  assert(cycle_result.computed && cycle_result.variants.front().optimal_alignments.size() <= 8);
  cycle_options.variant = minera::AlignmentVariant::kDiscountedAStar;
  const auto discounted_cycle = minera::check_alignment(log_of({"a"}), cycle, cycle_options);
  assert(discounted_cycle.computed);

  // Positive-cost discounted cycles remain finite under PM4Py's simple-state
  // closure and must still terminate with a goal.
  minera::PetriNet positive_cycle;
  positive_cycle.add_place();
  positive_cycle.add_place();
  positive_cycle.add_place();
  positive_cycle.set_source(0);
  positive_cycle.set_sink(2);
  positive_cycle.set_activities({"loop", "finish"});
  positive_cycle.add_transition(std::nullopt, {0}, {1});
  positive_cycle.add_transition(0, {1}, {1});
  positive_cycle.add_transition(1, {1}, {2});
  minera::AlignmentOptions positive_options;
  positive_options.variant = minera::AlignmentVariant::kDiscountedAStar;
  positive_options.exponent = 2.0;
  positive_options.model_costs_by_transition = {0.0, 1.0, 100.0};
  positive_options.max_states = 10000;
  const auto positive_result =
      minera::check_alignment(log_of({"unknown"}), positive_cycle, positive_options);
  assert(positive_result.computed);

  // Discounted PM4Py semantics close by product state.  Equal-cost arrivals
  // remain queued, and the tuple ordering prefers the deeper tied prefix at a
  // merge; both successor scheduling modes must agree on that pinned result.
  minera::PetriNet merged;
  merged.add_place();  // source
  merged.add_place();  // shallow branch
  merged.add_place();  // deep branch
  merged.add_place();  // deep routing intermediate
  merged.add_place();  // sink
  merged.set_source(0);
  merged.set_sink(4);
  merged.set_activities({"shallow", "finish"});
  merged.add_transition(0, {0}, {1});       // shallow cost 1
  merged.add_transition(std::nullopt, {0}, {2});
  merged.add_transition(std::nullopt, {2}, {3});
  merged.add_transition(0, {3}, {1});       // deeper prefix cost 1
  merged.add_transition(1, {1}, {4});       // cost 10, discounted suffix
  for (const bool synchronous : {false, true}) {
    minera::AlignmentOptions merge_options;
    merge_options.variant = minera::AlignmentVariant::kDiscountedAStar;
    merge_options.exponent = 2.0;
    merge_options.synchronous_dijkstra = synchronous;
    merge_options.default_trace_cost = 1.0;
    merge_options.default_model_cost = 1.0;
    merge_options.model_costs_by_transition = {1.0, 0.0, 0.0, 1.0, 10.0};
    const auto merged_result =
        minera::check_alignment(log_of({"unknown"}), merged, merge_options);
    assert(merged_result.computed);
    assert(std::abs(merged_result.total_cost - 1.0) < 1e-9);
  }

  // Reachability preflight obeys the configured state budget.  This long
  // silent chain is reachable, but a one-state limit must report exhaustion
  // instead of spending the historical fixed 200k preflight allowance.
  minera::PetriNet long_chain;
  constexpr std::size_t kChainPlaces = 64;
  for (std::size_t i = 0; i < kChainPlaces; ++i) long_chain.add_place();
  long_chain.set_source(0);
  long_chain.set_sink(kChainPlaces - 1);
  long_chain.set_activities({"finish"});
  for (std::size_t i = 0; i + 1 < kChainPlaces; ++i)
    long_chain.add_transition(i + 1 == kChainPlaces - 1 ? std::optional<std::size_t>(0)
                                                         : std::nullopt,
                              {i}, {i + 1});
  minera::AlignmentOptions preflight_cap;
  preflight_cap.variant = minera::AlignmentVariant::kDijkstraNoHeuristics;
  preflight_cap.max_states = 1;
  const auto preflight_result =
      minera::check_alignment(log_of({"finish"}), long_chain, preflight_cap);
  assert(!preflight_result.computed && preflight_result.exhausted);
  return 0;
}
