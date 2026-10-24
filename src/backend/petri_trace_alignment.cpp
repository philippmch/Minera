#include "minera/petri_trace_alignment.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <utility>

namespace minera {
namespace {

constexpr double kStandardMoveCost = 10000.0;

bool generator_variant(AlignmentVariant variant) {
  return variant == AlignmentVariant::kGeneratorDijkstraNoHeuristics ||
         variant == AlignmentVariant::kGeneratorDijkstraLessMemory;
}

// DIJKSTRA_LESS_MEMORY charges a silent transition 1 only when a visible
// transition sharing one of its input places carries a label of the trace,
// and 0 otherwise (observed black-box on the reference corpus).
std::vector<double> less_memory_silent_costs(const std::vector<std::string>& trace,
                                             const PetriNet& net) {
  const std::unordered_set<std::string> labels(trace.begin(), trace.end());
  const auto& transitions = net.transitions();
  std::vector<double> costs(transitions.size(), kStandardMoveCost);
  for (std::size_t t = 0; t < transitions.size(); ++t) {
    if (!transitions[t].silent()) continue;
    costs[t] = 0.0;
    for (const auto& other : transitions) {
      if (other.silent() || !labels.contains(net.activities()[*other.activity])) continue;
      const bool shared = std::any_of(
          other.inputs.begin(), other.inputs.end(), [&](std::size_t place) {
            return std::find(transitions[t].inputs.begin(), transitions[t].inputs.end(), place) !=
                   transitions[t].inputs.end();
          });
      if (shared) {
        costs[t] = 1.0;
        break;
      }
    }
  }
  return costs;
}

AlignmentOptions search_options(const std::vector<std::string>& trace, const PetriNet& net,
                                const PetriTraceAlignmentOptions& options) {
  AlignmentOptions search;
  search.variant = options.variant;
  search.cost_profile = AlignmentCostProfile::kStandard;
  search.initial_marking = options.initial_marking;
  search.final_marking = options.final_marking;
  search.max_states = options.max_states;
  search.max_alignments = options.max_alignments;
  switch (options.variant) {
    case AlignmentVariant::kDijkstraLessMemory:
    case AlignmentVariant::kGeneratorDijkstraLessMemory:
      if (options.variant == AlignmentVariant::kDijkstraLessMemory)
        search.model_costs_by_transition = less_memory_silent_costs(trace, net);
      break;
    case AlignmentVariant::kDiscountedAStar:
      // Unit deviations (silent moves included), discounted by 2^-depth.
      search.default_trace_cost = 1.0;
      search.default_model_cost = 1.0;
      search.default_silent_cost = 1.0;
      search.exponent = 2.0;
      break;
    default:
      break;
  }
  return search;
}

EventLog single_trace_log(const std::vector<std::string>& trace) {
  Trace value;
  value.case_id = "0";
  for (std::size_t i = 0; i < trace.size(); ++i)
    value.events.push_back(Event{"0", trace[i], {}, {}, i});
  EventLog log;
  log.traces.push_back(std::move(value));
  return log;
}

std::vector<PetriTraceAlignmentMove> reference_moves(const std::vector<AlignmentMove>& moves) {
  std::vector<PetriTraceAlignmentMove> out;
  out.reserve(moves.size());
  for (const auto& move : moves) {
    const std::optional<std::size_t> transition =
        move.transition == static_cast<std::size_t>(-1) ? std::nullopt
                                                        : std::optional{move.transition};
    switch (move.type) {
      case AlignmentMoveType::kSynchronous:
        out.push_back({move.activity, move.activity, transition});
        break;
      case AlignmentMoveType::kLog:
        out.push_back({move.activity, std::string(">>"), std::nullopt});
        break;
      case AlignmentMoveType::kModel:
        out.push_back({">>", move.activity.empty() ? std::nullopt : std::optional{move.activity},
                       transition});
        break;
    }
  }
  return out;
}

double reference_fitness(double cost, double bwc) {
  const double denominator = std::floor(bwc / kStandardMoveCost);
  if (denominator == 0.0) return 0.0;
  return 1.0 - std::floor(cost / kStandardMoveCost) / denominator;
}

std::vector<PetriTraceAlignment> run(const std::vector<std::string>& trace, const PetriNet& net,
                                     const PetriTraceAlignmentOptions& options) {
  const auto search = search_options(trace, net, options);
  const auto result = check_alignment(single_trace_log(trace), net, search);
  std::vector<PetriTraceAlignment> out;
  if (!result.computed || result.variants.empty()) {
    out.emplace_back();
    return out;
  }
  const auto& variant = result.variants.front();
  std::optional<double> bwc;
  if (!generator_variant(options.variant)) {
    if (options.variant == AlignmentVariant::kDiscountedAStar) {
      bwc = 0.0;
    } else {
      const auto empty =
          check_alignment(single_trace_log({}), net, search_options({}, net, options));
      if (!empty.computed || empty.variants.empty()) {
        out.emplace_back();
        return out;
      }
      double value = empty.variants.front().cost;
      // Less-memory search measures the empty run alone; the other variants
      // add one log move per event.
      if (options.variant != AlignmentVariant::kDijkstraLessMemory)
        value += kStandardMoveCost * static_cast<double>(trace.size());
      bwc = value;
    }
  }
  const auto& alignments = generator_variant(options.variant)
                               ? variant.optimal_alignments
                               : std::vector<std::vector<AlignmentMove>>{variant.moves};
  for (const auto& moves : alignments) {
    PetriTraceAlignment item;
    item.computed = true;
    item.alignment = reference_moves(moves);
    item.cost = variant.cost;
    item.bwc = bwc;
    if (bwc) item.fitness = reference_fitness(item.cost, *bwc);
    item.diagnostics = variant.diagnostics;
    out.push_back(std::move(item));
  }
  return out;
}

}  // namespace

PetriTraceAlignment align_petri_trace(const std::vector<std::string>& trace,
                                      const PetriNet& net,
                                      const PetriTraceAlignmentOptions& options) {
  return run(trace, net, options).front();
}

std::vector<PetriTraceAlignment> align_petri_trace_all(
    const std::vector<std::string>& trace, const PetriNet& net,
    const PetriTraceAlignmentOptions& options) {
  return run(trace, net, options);
}

}  // namespace minera
