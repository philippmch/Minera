#include "minera/petri_trace_alignment.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <queue>
#include <optional>
#include <functional>
#include <map>
#include <stdexcept>
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
// transition whose input places are all inputs of the silent one (an empty
// preset included) carries a label of the trace, and 0 otherwise (observed
// black-box on the reference corpus and on targeted probe nets).
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
      const bool contained = std::all_of(
          other.inputs.begin(), other.inputs.end(), [&](std::size_t place) {
            return std::find(transitions[t].inputs.begin(), transitions[t].inputs.end(), place) !=
                   transitions[t].inputs.end();
          });
      if (contained) {
        costs[t] = 1.0;
        break;
      }
    }
  }
  return costs;
}

// DIJKSTRA_LESS_MEMORY and its generator are Dijkstra searches over (marking,
// trace position) that also discard a state whose marking was already closed
// at the same or a later trace position. The pruning is not exact: on some
// nets the result costs more than the optimum, and the reference reports that
// cost. Equal-cost states pop deepest trace position first, then FIFO.
struct PrunedSearchResult {
  bool found = false;
  double cost = 0.0;
  std::vector<std::vector<AlignmentMove>> alignments;
  AlignmentDiagnostics diagnostics;
};

bool plain_net(const PetriNet& net) {
  return std::all_of(net.transitions().begin(), net.transitions().end(), [](const auto& t) {
    return t.reset_inputs.empty() && t.inhibitor_inputs.empty();
  });
}

// Ordinary arc occurrence weight: unit when the transition carries no
// per-occurrence weights (repeated endpoints then express multiplicity).
int arc_weight(const PetriTransition& transition, std::size_t occurrence, bool input) {
  const auto& weights = input ? transition.input_weights : transition.output_weights;
  if (weights.empty()) return 1;
  const auto& weight = weights[occurrence];
  const auto magnitude = static_cast<int>(weight.magnitude);
  return weight.negative ? -magnitude : magnitude;
}

std::vector<int> workflow_marking(const PetriNet& net, const std::vector<int>& requested,
                                  bool initial) {
  if (!requested.empty()) {
    if (requested.size() != net.place_count())
      throw std::invalid_argument(initial ? "initial marking dimension does not match net"
                                          : "final marking dimension does not match net");
    return requested;
  }
  std::vector<int> marking(net.place_count(), 0);
  const std::size_t place = initial ? net.source() : net.sink();
  if (place < marking.size()) marking[place] = 1;
  return marking;
}

PrunedSearchResult pruned_dijkstra(const std::vector<std::string>& trace, const PetriNet& net,
                                   const PetriTraceAlignmentOptions& options,
                                   const std::vector<double>& model_costs,
                                   const std::vector<double>& trace_costs, bool enumerate) {
  struct State {
    std::vector<int> marking;
    std::size_t position = 0;
    double cost = 0.0;
    bool closed = false;
    std::vector<std::pair<std::size_t, AlignmentMove>> parents;
  };
  struct Entry {
    double cost;
    std::size_t position;
    std::uint64_t seq;
    std::size_t state;
    bool operator>(const Entry& other) const {
      if (cost != other.cost) return cost > other.cost;
      if (position != other.position) return position < other.position;
      return seq > other.seq;
    }
  };
  struct KeyHash {
    std::size_t operator()(const std::pair<std::vector<int>, std::size_t>& key) const noexcept {
      std::size_t h = std::hash<std::size_t>{}(key.second);
      for (const int v : key.first) h = h * 1000003u ^ std::hash<int>{}(v);
      return h;
    }
  };
  struct MarkingHash {
    std::size_t operator()(const std::vector<int>& marking) const noexcept {
      std::size_t h = 0;
      for (const int v : marking) h = h * 1000003u ^ std::hash<int>{}(v);
      return h;
    }
  };
  const auto& transitions = net.transitions();
  const auto initial = workflow_marking(net, options.initial_marking, true);
  const auto goal = workflow_marking(net, options.final_marking, false);
  std::vector<State> states;
  std::unordered_map<std::pair<std::vector<int>, std::size_t>, std::size_t, KeyHash> index;
  std::unordered_map<std::vector<int>, std::size_t, MarkingHash> closed_position;
  std::priority_queue<Entry, std::vector<Entry>, std::greater<>> queue;
  std::uint64_t seq = 0;
  PrunedSearchResult result;
  auto& diagnostics = result.diagnostics;

  const auto relax = [&](std::size_t from, std::vector<int> marking, std::size_t position,
                         double cost, AlignmentMove move) {
    ++diagnostics.traversed_arcs;
    auto key = std::make_pair(std::move(marking), position);
    auto found = index.find(key);
    if (found == index.end()) {
      states.push_back({key.first, position, cost, false, {{from, std::move(move)}}});
      index.emplace(std::move(key), states.size() - 1);
      queue.push({cost, position, seq++, states.size() - 1});
      ++diagnostics.queued_states;
      return;
    }
    auto& state = states[found->second];
    if (state.closed) return;
    if (cost < state.cost) {
      state.cost = cost;
      state.parents.assign(1, {from, std::move(move)});
      queue.push({cost, position, seq++, found->second});
      ++diagnostics.queued_states;
    } else if (cost == state.cost && enumerate) {
      state.parents.emplace_back(from, std::move(move));
    }
  };

  states.push_back({initial, 0, 0.0, false, {}});
  index.emplace(std::make_pair(initial, std::size_t{0}), 0);
  queue.push({0.0, 0, seq++, 0});
  ++diagnostics.queued_states;
  std::optional<std::size_t> target;
  while (!queue.empty()) {
    const auto entry = queue.top();
    queue.pop();
    auto& current = states[entry.state];
    if (current.closed || entry.cost > current.cost) continue;
    const auto closed = closed_position.find(current.marking);
    if (closed != closed_position.end() && closed->second >= current.position) continue;
    if (diagnostics.visited_states >= options.max_states) break;
    current.closed = true;
    ++diagnostics.visited_states;
    if (closed == closed_position.end()) closed_position.emplace(current.marking, current.position);
    else closed->second = current.position;
    if (current.position == trace.size() && current.marking == goal) {
      target = entry.state;
      break;
    }
    const auto marking = current.marking;
    const auto position = current.position;
    const auto cost = current.cost;
    if (position < trace.size())
      relax(entry.state, marking, position + 1, cost + trace_costs[position],
            {AlignmentMoveType::kLog, trace[position]});
    for (std::size_t t = 0; t < transitions.size(); ++t) {
      const auto& transition = transitions[t];
      auto next = marking;
      bool enabled = true;
      for (std::size_t k = 0; k < transition.inputs.size(); ++k) {
        const auto weight = arc_weight(transition, k, true);
        auto& tokens = next[transition.inputs[k]];
        if (tokens < weight) {
          enabled = false;
          break;
        }
        tokens -= weight;
      }
      if (!enabled) continue;
      for (std::size_t k = 0; k < transition.outputs.size(); ++k)
        next[transition.outputs[k]] += arc_weight(transition, k, false);
      if (transition.silent()) {
        relax(entry.state, std::move(next), position, cost + model_costs[t],
              {AlignmentMoveType::kModel, std::string{}, t});
        continue;
      }
      const auto& label = net.activities()[*transition.activity];
      if (position < trace.size() && trace[position] == label)
        relax(entry.state, next, position + 1, cost, {AlignmentMoveType::kSynchronous, label, t});
      relax(entry.state, std::move(next), position, cost + model_costs[t],
            {AlignmentMoveType::kModel, label, t});
    }
  }
  diagnostics.closed_set_length = closed_position.size();
  diagnostics.num_visited_markings = closed_position.size();
  if (!target) return result;
  result.found = true;
  result.cost = states[*target].cost;
  // Every path through recorded equal-cost parents, up to the limit.
  std::vector<AlignmentMove> reversed;
  const std::size_t limit = enumerate ? std::max<std::size_t>(options.max_alignments, 1) : 1;
  const auto walk = [&](const auto& self, std::size_t state) -> void {
    if (result.alignments.size() >= limit) return;
    if (states[state].parents.empty()) {
      result.alignments.emplace_back(reversed.rbegin(), reversed.rend());
      return;
    }
    for (const auto& [parent, move] : states[state].parents) {
      reversed.push_back(move);
      self(self, parent);
      reversed.pop_back();
      if (result.alignments.size() >= limit) return;
    }
  };
  walk(walk, *target);
  return result;
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
  // DISCOUNTED_A_STAR searches with unit discounted costs whatever cost
  // functions are supplied; they only enter its best-worst cost.
  if (options.variant == AlignmentVariant::kDiscountedAStar) return search;
  if (!options.model_costs.empty()) search.model_costs_by_transition = options.model_costs;
  if (!options.sync_costs.empty() && options.variant != AlignmentVariant::kDijkstraLessMemory &&
      options.variant != AlignmentVariant::kGeneratorDijkstraLessMemory)
    search.sync_costs_by_transition = options.sync_costs;
  if (!options.trace_costs.empty()) {
    search.trace_costs_by_position = options.trace_costs;
    search.trace_costs_by_position.resize(trace.size());
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

struct SearchOutcome {
  bool found = false;
  double cost = 0.0;
  std::vector<std::vector<AlignmentMove>> alignments;
  AlignmentDiagnostics diagnostics;
};

bool less_memory_variant(AlignmentVariant variant) {
  return variant == AlignmentVariant::kDijkstraLessMemory ||
         variant == AlignmentVariant::kGeneratorDijkstraLessMemory;
}

// The reference synchronous-product variants (everything except
// DIJKSTRA_LESS_MEMORY) read every ordinary arc as weight 1: a weighted arc
// moves one token. Repeated endpoints are PNML multiplicities and collapse too.
PetriNet unit_weight_net(const PetriNet& net) {
  const auto unique = [](const std::vector<std::size_t>& places) {
    std::vector<std::size_t> out;
    for (const auto place : places)
      if (std::find(out.begin(), out.end(), place) == out.end()) out.push_back(place);
    return out;
  };
  PetriNet copy;
  for (std::size_t p = 0; p < net.place_count(); ++p) copy.add_place();
  for (const auto& transition : net.transitions())
    copy.add_transition(transition.activity, unique(transition.inputs), unique(transition.outputs));
  copy.set_activities(net.activities());
  if (net.has_source()) copy.set_source(net.source());
  if (net.has_sink()) copy.set_sink(net.sink());
  if (net.transition_names()) copy.set_transition_names(*net.transition_names());
  if (net.discovery_markings()) copy.set_discovery_markings(*net.discovery_markings());
  return copy;
}

bool weighted_net(const PetriNet& net) {
  for (const auto& transition : net.transitions()) {
    if (transition.has_compact_ordinary_arcs()) return true;
    for (const auto* places : {&transition.inputs, &transition.outputs})
      for (std::size_t k = 0; k < places->size(); ++k)
        if (std::find(places->begin(), places->begin() + k, (*places)[k]) != places->begin() + k)
          return true;
  }
  return false;
}

SearchOutcome search(const std::vector<std::string>& trace, const PetriNet& net,
                     const PetriTraceAlignmentOptions& options) {
  SearchOutcome out;
  if (less_memory_variant(options.variant) && plain_net(net)) {
    std::vector<double> model_costs = options.model_costs;
    if (model_costs.empty()) {
      if (options.variant == AlignmentVariant::kDijkstraLessMemory) {
        model_costs = less_memory_silent_costs(trace, net);
      } else {
        model_costs.assign(net.transitions().size(), kStandardMoveCost);
        for (std::size_t t = 0; t < model_costs.size(); ++t)
          if (net.transitions()[t].silent()) model_costs[t] = 1.0;
      }
    }
    std::vector<double> trace_costs(trace.size(), kStandardMoveCost);
    for (std::size_t i = 0; i < trace.size() && i < options.trace_costs.size(); ++i)
      trace_costs[i] = options.trace_costs[i];
    auto pruned = pruned_dijkstra(trace, net, options, model_costs, trace_costs,
                                  generator_variant(options.variant));
    if (pruned.found && generator_variant(options.variant) && !weighted_net(net)) {
      // The generator lists every alignment of the pruned search's cost. When
      // that cost is optimal, the exact equal-cost enumeration supplies them;
      // otherwise only the pruned search's own equal-cost paths are known.
      // Weighted nets can pump tokens through silent cycles, where the exact
      // enumeration does not terminate in reasonable time; they keep the
      // pruned search's paths.
      auto exact_options = options;
      exact_options.model_costs = model_costs;
      exact_options.trace_costs = trace_costs;
      exact_options.variant = AlignmentVariant::kGeneratorDijkstraNoHeuristics;
      exact_options.sync_costs.clear();
      const auto exact = check_alignment(single_trace_log(trace), net,
                                         search_options(trace, net, exact_options));
      if (exact.computed && !exact.variants.empty() &&
          exact.variants.front().cost == pruned.cost)
        pruned.alignments = exact.variants.front().optimal_alignments;
    }
    out.found = pruned.found;
    out.cost = pruned.cost;
    out.alignments = std::move(pruned.alignments);
    out.diagnostics = pruned.diagnostics;
    return out;
  }
  const auto result =
      plain_net(net) && weighted_net(net)
          ? check_alignment(single_trace_log(trace), unit_weight_net(net),
                            search_options(trace, net, options))
          : check_alignment(single_trace_log(trace), net, search_options(trace, net, options));
  if (!result.computed || result.variants.empty()) return out;
  const auto& variant = result.variants.front();
  out.found = true;
  out.cost = variant.cost;
  out.alignments = generator_variant(options.variant)
                       ? variant.optimal_alignments
                       : std::vector<std::vector<AlignmentMove>>{variant.moves};
  out.diagnostics = variant.diagnostics;
  return out;
}

std::vector<PetriTraceAlignment> run(const std::vector<std::string>& trace, const PetriNet& net,
                                     const PetriTraceAlignmentOptions& options) {
  const auto result = search(trace, net, options);
  std::vector<PetriTraceAlignment> out;
  if (!result.found) {
    // The reference returns no alignment (generators: an empty sequence)
    // when the final marking is unreachable; its discounted variant raises.
    if (options.variant == AlignmentVariant::kDiscountedAStar)
      throw std::runtime_error("discounted alignment: final marking is unreachable");
    if (!generator_variant(options.variant)) out.emplace_back();
    return out;
  }
  std::optional<double> bwc;
  if (!generator_variant(options.variant)) {
    // Best-worst cost: the cheapest model run for the empty trace plus one log
    // move per event. Less-memory search only counts log moves priced by a
    // supplied trace cost function; discounted search counts nothing but
    // those.
    const bool discounted = options.variant == AlignmentVariant::kDiscountedAStar;
    double value = 0.0;
    if (!discounted) {
      const auto empty = search({}, net, options);
      if (!empty.found) {
        out.emplace_back();
        return out;
      }
      value = empty.cost;
    }
    const bool default_log_moves =
        options.variant != AlignmentVariant::kDijkstraLessMemory && !discounted;
    for (std::size_t i = 0; i < trace.size(); ++i) {
      if (i < options.trace_costs.size()) value += options.trace_costs[i];
      else if (default_log_moves) value += kStandardMoveCost;
    }
    bwc = value;
  }
  for (const auto& moves : result.alignments) {
    PetriTraceAlignment item;
    item.computed = true;
    item.alignment = reference_moves(moves);
    item.cost = result.cost;
    item.bwc = bwc;
    if (bwc) item.fitness = reference_fitness(item.cost, *bwc);
    item.diagnostics = result.diagnostics;
    out.push_back(std::move(item));
  }
  return out;
}

}  // namespace

PetriTraceAlignment align_petri_trace(const std::vector<std::string>& trace,
                                      const PetriNet& net,
                                      const PetriTraceAlignmentOptions& options) {
  auto all = run(trace, net, options);
  return all.empty() ? PetriTraceAlignment{} : std::move(all.front());
}

std::vector<PetriTraceAlignment> align_petri_trace_all(
    const std::vector<std::string>& trace, const PetriNet& net,
    const PetriTraceAlignmentOptions& options) {
  return run(trace, net, options);
}

std::vector<PetriTraceAlignment> align_petri_log(const EventLog& log, const PetriNet& net,
                                                 const PetriLogAlignmentOptions& options) {
  if (!options.trace.trace_costs.empty())
    throw std::invalid_argument("per-event trace costs cannot apply to a whole log");
  // The public log dispatcher first checks easy soundness and raises for a
  // net whose final marking is unreachable, where a single trace would get
  // None.
  if (!final_marking_reachable(net, options.trace.initial_marking, options.trace.final_marking))
    throw std::invalid_argument("trying to apply alignments on a Petri net that is not easy sound");
  const bool standard = options.activity_key == "concept:name" || options.activity_key == "activity";
  std::map<std::vector<std::string>, PetriTraceAlignment> variants;
  std::vector<PetriTraceAlignment> out;
  out.reserve(log.traces.size());
  for (const auto& trace : log.traces) {
    std::vector<std::string> activities;
    activities.reserve(trace.events.size());
    for (const auto& event : trace.events) {
      if (standard) {
        activities.push_back(event.activity);
        continue;
      }
      const auto* value = event.attribute(options.activity_key);
      if (!value) throw std::invalid_argument("event without activity key '" + options.activity_key + "'");
      activities.push_back(*value);
    }
    auto found = variants.find(activities);
    if (found == variants.end()) {
      auto aligned = align_petri_trace(activities, net, options.trace);
      if (!options.enable_best_worst_cost) {
        aligned.bwc.reset();
        aligned.fitness.reset();
      }
      found = variants.emplace(std::move(activities), std::move(aligned)).first;
    }
    out.push_back(found->second);
  }
  return out;
}

}  // namespace minera
