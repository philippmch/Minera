#pragma once

// Alignments of a supplied (imported) model as the reference's public
// alignment dispatcher reports them. The bundle aligner keeps the trace
// variants, precision and bounds; with the default unit costs and one of the
// three single-alignment variants, each variant's alignment, cost and
// fitness then come from the reference-semantics engine, and the log fitness
// is the reference's 1 - sum(cost) / sum(best-worst cost) over raw costs.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "minera/conformance.hpp"
#include "minera/event_log.hpp"
#include "minera/petri_net.hpp"
#include "minera/petri_trace_alignment.hpp"

namespace minera::tools {

[[nodiscard]] inline bool reference_imported_alignment_applies(
    const AlignmentOptions& options) {
  const bool single = options.variant == AlignmentVariant::kStateEquationAStar ||
                      options.variant == AlignmentVariant::kDijkstraNoHeuristics ||
                      options.variant == AlignmentVariant::kDijkstraLessMemory;
  return single && options.cost_profile == AlignmentCostProfile::kLegacyUnit &&
         !options.default_trace_cost && !options.default_model_cost &&
         !options.default_sync_cost && !options.default_silent_cost &&
         options.trace_costs.empty() && options.model_costs.empty() &&
         options.sync_costs.empty() && options.trace_costs_by_position.empty() &&
         options.model_costs_by_transition.empty() &&
         options.sync_costs_by_transition.empty() && !options.trace_cost_function &&
         !options.model_cost_function && !options.sync_cost_function;
}

[[nodiscard]] inline AlignmentResult imported_model_alignment(
    const EventLog& log, const PetriNet& net, const AlignmentOptions& options) {
  auto result = check_alignment(log, net, options);
  if (!result.computed || !reference_imported_alignment_applies(options) ||
      result.variants.empty())
    return result;

  // One synthetic trace per bundle variant, aligned by the public log
  // dispatcher (which also applies the reference easy-soundness check).
  EventLog variants_log;
  variants_log.traces.reserve(result.variants.size());
  for (const auto& variant : result.variants) {
    Trace trace;
    for (const auto& activity : variant.activities) {
      Event event;
      event.activity = activity;
      trace.events.push_back(std::move(event));
    }
    variants_log.traces.push_back(std::move(trace));
  }
  PetriLogAlignmentOptions reference;
  reference.trace.variant = options.variant;
  reference.trace.initial_marking = options.initial_marking;
  reference.trace.final_marking = options.final_marking;
  reference.trace.max_states = options.max_states;
  reference.trace.max_time_seconds = options.max_time_seconds;
  reference.trace.cancelled = options.cancelled;
  // The reference's less-memory search keeps one arc per place and
  // transition (parallel arcs collapse; observed black-box, log fitness equal
  // on the probed nets), after the easy-soundness check on the full net.
  std::optional<PetriNet> collapsed;
  const auto unit_arcs = [&net] {
    const auto unit = [](const std::vector<PetriArcWeight>& weights) {
      return std::all_of(weights.begin(), weights.end(), [](const PetriArcWeight& weight) {
        return weight.magnitude == 1 && !weight.negative;
      });
    };
    return std::all_of(net.transitions().begin(), net.transitions().end(),
                       [&](const PetriTransition& transition) {
                         return unit(transition.input_weights) && unit(transition.output_weights);
                       });
  };
  if (options.variant == AlignmentVariant::kDijkstraLessMemory &&
      net.has_compact_ordinary_arcs() && unit_arcs()) {
    const auto distinct = [](const std::vector<std::size_t>& places) {
      std::vector<std::size_t> out;
      for (const auto place : places)
        if (std::find(out.begin(), out.end(), place) == out.end()) out.push_back(place);
      return out;
    };
    collapsed.emplace();
    for (std::size_t p = 0; p < net.place_count(); ++p) collapsed->add_place();
    if (net.place_names()) collapsed->set_place_names(*net.place_names());
    for (const auto& transition : net.transitions())
      collapsed->add_transition(transition.activity, distinct(transition.inputs),
                                distinct(transition.outputs));
    collapsed->set_activities(net.activities());
    if (net.transition_names()) collapsed->set_transition_names(*net.transition_names());
    if (net.has_source()) collapsed->set_source(net.source());
    if (net.has_sink()) collapsed->set_sink(net.sink());
  }
  std::vector<PetriTraceAlignment> aligned;
  try {
    if (collapsed) (void)align_petri_log(EventLog{}, net, reference);
    aligned = align_petri_log(variants_log, collapsed ? *collapsed : net, reference);
  } catch (const std::invalid_argument& error) {
    result.computed = false;
    result.exhausted = std::string(error.what()).find("undecided") != std::string::npos;
    result.variants.clear();
    return result;
  }

  std::vector<int> initial = options.initial_marking;
  if (initial.empty()) {
    initial.assign(net.place_count(), 0);
    initial.at(net.source()) = 1;
  }
  double raw_cost = 0.0;
  double raw_worst = 0.0;
  result.total_cost = 0.0;
  result.fitting_traces = 0;
  for (std::size_t v = 0; v < result.variants.size(); ++v) {
    auto& variant = result.variants[v];
    const auto& entry = aligned.at(v);
    if (!entry.computed || !entry.bwc || !entry.fitness) {
      result.computed = false;
      result.exhausted = entry.exhausted;
      result.time_limit_reached = entry.time_limit_reached;
      result.variants.clear();
      return result;
    }
    std::vector<AlignmentMove> moves;
    std::vector<ProjectedAlignmentMove> projected;
    auto marking = initial;
    for (const auto& move : entry.alignment) {
      const auto transition = move.transition.value_or(static_cast<std::size_t>(-1));
      if (move.model && *move.model == ">>") {
        moves.push_back({AlignmentMoveType::kLog, move.log, transition});
        continue;
      }
      const bool synchronous = move.log != ">>";
      moves.push_back({synchronous ? AlignmentMoveType::kSynchronous : AlignmentMoveType::kModel,
                       synchronous ? move.log : move.model.value_or(std::string{}),
                       transition});
      if (!move.transition) continue;
      const auto before = marking;
      fire_petri_transition(net.transitions().at(*move.transition), marking);
      if (move.model)
        projected.push_back({*move.model, *move.transition, before, marking});
    }
    const double units = std::floor(entry.cost / 10000.0);
    variant.moves = moves;
    variant.complete_moves = moves;
    variant.optimal_alignments = {std::move(moves)};
    variant.projected_model_moves = std::move(projected);
    variant.cost = units;
    variant.worst_cost = std::floor(*entry.bwc / 10000.0);
    variant.fitness = *entry.fitness;
    variant.diagnostics = entry.diagnostics;
    const auto count = static_cast<double>(variant.count);
    raw_cost += entry.cost * count;
    raw_worst += *entry.bwc * count;
    result.total_cost += units * count;
    if (variant.fitness >= 1.0) result.fitting_traces += variant.count;
  }
  // The reference reports a log fitness of 1 when the summed best-worst cost
  // is zero (less-memory search prices no log move), whatever the costs.
  result.fitness = raw_worst <= 0.0 ? 1.0 : 1.0 - raw_cost / raw_worst;
  std::stable_sort(result.variants.begin(), result.variants.end(),
                   [](const VariantAlignment& a, const VariantAlignment& b) {
                     if (a.fitness != b.fitness) return a.fitness < b.fitness;
                     return a.count > b.count;
                   });
  return result;
}

}  // namespace minera::tools
