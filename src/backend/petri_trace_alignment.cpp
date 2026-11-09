#include "minera/petri_trace_alignment.hpp"
#include "minera/petri_analysis.hpp"
#include "minera/petri_reference_arcs.hpp"
#include "minera/cancellation.hpp"

#include <limits>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <queue>
#include <optional>
#include <functional>
#include <map>
#include <set>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace minera {
namespace {

constexpr double kStandardMoveCost = 10000.0;

// max_time_seconds and cancelled, polled every 256 steps as the Aligner
// polls its callback: true once the search must stop.
class SearchBudget {
 public:
  explicit SearchBudget(const PetriTraceAlignmentOptions& options)
      : options_(options), started_(std::chrono::steady_clock::now()) {}
  bool exhausted() {
    if ((++calls_ & 255u) != 0) return false;
    if (options_.cancelled && options_.cancelled()) return cancelled_ = true;
    return options_.max_time_seconds > 0.0 &&
           std::chrono::duration<double>(std::chrono::steady_clock::now() - started_).count() >=
               options_.max_time_seconds;
  }
  [[nodiscard]] bool cancelled() const { return cancelled_; }

 private:
  const PetriTraceAlignmentOptions& options_;
  std::chrono::steady_clock::time_point started_;
  unsigned calls_ = 0;
  bool cancelled_ = false;
};

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
  bool exhausted = false;           // max_states or max_time_seconds stopped it
  bool time_limit_reached = false;  // max_time_seconds had run out
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
  SearchBudget budget(options);
  while (!queue.empty()) {
    const auto entry = queue.top();
    queue.pop();
    auto& current = states[entry.state];
    if (current.closed || entry.cost > current.cost) continue;
    const auto closed = closed_position.find(current.marking);
    if (closed != closed_position.end() && closed->second >= current.position) continue;
    if (diagnostics.visited_states >= options.max_states) {
      result.exhausted = true;
      break;
    }
    // As the aligner does: a cancelled search throws, one past its time is
    // uncomputed and marked as having reached the time limit.
    if (budget.exhausted()) {
      if (budget.cancelled()) throw OperationCancelled("alignment was cancelled");
      result.exhausted = true;
      result.time_limit_reached = true;
      break;
    }
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
  search.max_time_seconds = options.max_time_seconds;
  search.cancelled = options.cancelled;
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
  bool exhausted = false;
  bool time_limit_reached = false;
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
// The net the reference aligns on when arcs are weighted or parallel. Its
// synchronous product gives every arc object weight 1 and keeps parallel arcs,
// and its search then fires a transition, as observed black-box, once each
// input place holds one token per arc object from it, taking one token there
// and putting one token per output arc object. As repeated unit endpoints: k
// parallel input arcs from a place become k inputs and k - 1 outputs back to
// it (enabled at k tokens, one consumed); outputs are one endpoint per arc.
PetriNet unit_weight_net(const PetriNet& net) {
  PetriNet copy;
  for (std::size_t p = 0; p < net.place_count(); ++p) copy.add_place();
  for (const auto& transition : net.transitions()) {
    std::vector<std::size_t> inputs, outputs;
    std::vector<std::pair<std::size_t, std::size_t>> counts;
    for (const auto& arc : petri_reference_arcs(transition, true)) {
      inputs.push_back(arc.place);
      const auto found = std::find_if(counts.begin(), counts.end(),
                                      [&](const auto& entry) { return entry.first == arc.place; });
      if (found == counts.end()) counts.emplace_back(arc.place, 1);
      else ++found->second;
    }
    for (const auto& [place, count] : counts) outputs.insert(outputs.end(), count - 1, place);
    for (const auto& arc : petri_reference_arcs(transition, false)) outputs.push_back(arc.place);
    copy.add_transition(transition.activity, std::move(inputs), std::move(outputs));
  }
  copy.set_activities(net.activities());
  if (net.has_source()) copy.set_source(net.source());
  if (net.has_sink()) copy.set_sink(net.sink());
  if (net.transition_names()) copy.set_transition_names(*net.transition_names());
  if (net.place_names()) copy.set_place_names(*net.place_names());
  if (net.discovery_markings()) copy.set_discovery_markings(*net.discovery_markings());
  return copy;
}

// The net the reference's Dijkstra searches (no-heuristics, its generator,
// and discounted A*) align on when arcs are weighted or parallel. They fire a
// synchronous-product transition, as observed black-box, once each of its
// input places holds one token, take one token there, and put one token per
// output arc object: two parallel input arcs from a place act as one, two
// parallel output arcs put two tokens, and a weight is read as 1. State-
// equation A* does not reach such firings (its marking-equation heuristic
// prunes them), so it keeps unit_weight_net.
PetriNet set_semantics_net(const PetriNet& net) {
  PetriNet copy;
  for (std::size_t p = 0; p < net.place_count(); ++p) copy.add_place();
  for (const auto& transition : net.transitions()) {
    std::vector<std::size_t> inputs, outputs;
    for (const auto& arc : petri_reference_arcs(transition, true))
      if (std::find(inputs.begin(), inputs.end(), arc.place) == inputs.end())
        inputs.push_back(arc.place);
    for (const auto& arc : petri_reference_arcs(transition, false)) outputs.push_back(arc.place);
    copy.add_transition(transition.activity, std::move(inputs), std::move(outputs));
  }
  copy.set_activities(net.activities());
  if (net.has_source()) copy.set_source(net.source());
  if (net.has_sink()) copy.set_sink(net.sink());
  if (net.transition_names()) copy.set_transition_names(*net.transition_names());
  if (net.place_names()) copy.set_place_names(*net.place_names());
  if (net.discovery_markings()) copy.set_discovery_markings(*net.discovery_markings());
  return copy;
}

bool set_semantics_variant(AlignmentVariant variant) {
  return variant == AlignmentVariant::kDijkstraNoHeuristics ||
         variant == AlignmentVariant::kGeneratorDijkstraNoHeuristics ||
         variant == AlignmentVariant::kDiscountedAStar;
}

// The net a synchronous-product variant searches on a net with weighted or
// parallel arcs (state-equation A*: unit_weight_net; the Dijkstra searches:
// set_semantics_net).
PetriNet reference_search_net(const PetriNet& net, AlignmentVariant variant) {
  return set_semantics_variant(variant) ? set_semantics_net(net) : unit_weight_net(net);
}

bool weighted_net(const PetriNet& net) { return petri_net_has_weighted_arcs(net); }

// The public log dispatcher's easy-soundness check on a net with weighted or
// parallel arcs, as observed black-box (98.9% of 1,500 random nets with
// weighted and parallel arcs; the rest are unbounded or tie cases): the
// marking equation's LP relaxation over the reference incidence (one -1/+1
// per arc object) must be feasible, and the final marking must be reachable
// when a transition is enabled once each input place holds the summed weight
// of its arcs, takes the weight of one of them (the reference keeps whichever
// arc its set yields last, here the last in document order) and puts the
// summed weight of its output arcs. A search that passes max_states refuses
// the net, as the reference does for the unbounded nets probed, and so does
// one past max_time_seconds; a cancelled one throws OperationCancelled.
bool weighted_easy_sound(const PetriNet& net, const std::vector<int>& requested_initial,
                          const std::vector<int>& requested_final,
                          const PetriTraceAlignmentOptions& options) {
  const auto max_states = options.max_states;
  const auto initial = workflow_marking(net, requested_initial, true);
  const auto final = workflow_marking(net, requested_final, false);
  if (!marking_equation_relaxation_feasible(net, Marking(initial.begin(), initial.end()),
                                            Marking(final.begin(), final.end())))
    return false;
  struct Entry { std::size_t place; std::int64_t need, take; };
  struct Step { std::vector<Entry> inputs; std::vector<std::pair<std::size_t, std::int64_t>> put; };
  std::vector<Step> steps;
  const auto checked_sum = [](std::int64_t lhs, std::int64_t rhs) {
    if ((rhs > 0 && lhs > std::numeric_limits<std::int64_t>::max() - rhs) ||
        (rhs < 0 && lhs < std::numeric_limits<std::int64_t>::min() - rhs))
      throw std::overflow_error("easy-soundness arc weight overflow");
    return lhs + rhs;
  };
  for (const auto& transition : net.transitions()) {
    Step step;
    for (const auto& arc : petri_reference_arcs(transition, true)) {
      const auto found = std::find_if(step.inputs.begin(), step.inputs.end(),
                                      [&](const Entry& entry) { return entry.place == arc.place; });
      if (found == step.inputs.end()) step.inputs.push_back({arc.place, arc.signed_weight(), arc.signed_weight()});
      else { found->need = checked_sum(found->need, arc.signed_weight()); found->take = arc.signed_weight(); }
    }
    for (const auto& arc : petri_reference_arcs(transition, false)) {
      const auto found = std::find_if(step.put.begin(), step.put.end(),
                                      [&](const auto& entry) { return entry.first == arc.place; });
      if (found == step.put.end()) step.put.emplace_back(arc.place, arc.signed_weight());
      else found->second = checked_sum(found->second, arc.signed_weight());
    }
    steps.push_back(std::move(step));
  }
  std::vector<std::int64_t> start(initial.begin(), initial.end());
  const std::vector<std::int64_t> goal(final.begin(), final.end());
  std::set<std::vector<std::int64_t>> seen{start};
  std::queue<std::vector<std::int64_t>> pending;
  pending.push(start);
  SearchBudget budget(options);
  while (!pending.empty()) {
    if (budget.exhausted()) {
      if (budget.cancelled()) throw OperationCancelled("easy-soundness check was cancelled");
      return false;
    }
    const auto marking = std::move(pending.front());
    pending.pop();
    if (marking == goal) return true;
    for (const auto& step : steps) {
      if (std::any_of(step.inputs.begin(), step.inputs.end(),
                      [&](const Entry& entry) { return marking[entry.place] < entry.need; }))
        continue;
      auto next = marking;
      for (const auto& entry : step.inputs) next[entry.place] = checked_sum(next[entry.place], -entry.take);
      for (const auto& [place, weight] : step.put) next[place] = checked_sum(next[place], weight);
      if (!seen.insert(next).second) continue;
      if (seen.size() > max_states) return false;
      pending.push(std::move(next));
    }
  }
  return false;
}

// The net the reference's less-memory searches (DIJKSTRA_LESS_MEMORY and its
// generator) read when some transition has parallel unit arcs: one arc per
// place and transition (observed black-box: per-trace costs equal on 120
// random BPMN models with duplicated flows). Nullopt when there is nothing
// to collapse (no compact arcs, or a compact arc that is not one plain unit
// arc, whose collapsed weight is not determined).
std::optional<PetriNet> less_memory_net(const PetriNet& net) {
  const auto unit = [](const std::vector<PetriArcWeight>& weights) {
    return std::all_of(weights.begin(), weights.end(), [](const PetriArcWeight& weight) {
      return weight.magnitude == 1 && !weight.negative;
    });
  };
  if (!net.has_compact_ordinary_arcs() || net.has_extended_arcs() ||
      !std::all_of(net.transitions().begin(), net.transitions().end(),
                   [&](const PetriTransition& transition) {
                     return unit(transition.input_weights) && unit(transition.output_weights);
                   }))
    return std::nullopt;
  const auto distinct = [](const std::vector<std::size_t>& places) {
    std::vector<std::size_t> out;
    for (const auto place : places)
      if (std::find(out.begin(), out.end(), place) == out.end()) out.push_back(place);
    return out;
  };
  PetriNet collapsed;
  for (std::size_t p = 0; p < net.place_count(); ++p) collapsed.add_place();
  if (net.place_names()) collapsed.set_place_names(*net.place_names());
  for (const auto& transition : net.transitions())
    collapsed.add_transition(transition.activity, distinct(transition.inputs),
                             distinct(transition.outputs));
  collapsed.set_activities(net.activities());
  if (net.transition_names()) collapsed.set_transition_names(*net.transition_names());
  if (net.has_source()) collapsed.set_source(net.source());
  if (net.has_sink()) collapsed.set_sink(net.sink());
  if (net.discovery_markings()) collapsed.set_discovery_markings(*net.discovery_markings());
  return collapsed;
}

SearchOutcome search(const std::vector<std::string>& trace, const PetriNet& net,
                     const PetriTraceAlignmentOptions& options) {
  SearchOutcome out;
  if (less_memory_variant(options.variant) && plain_net(net)) {
    if (const auto collapsed = less_memory_net(net)) return search(trace, *collapsed, options);
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
      throw_if_cancelled(options.cancelled, "alignment was cancelled");
      if (exact.computed && !exact.variants.empty() &&
          exact.variants.front().cost == pruned.cost)
        pruned.alignments = exact.variants.front().optimal_alignments;
    }
    out.found = pruned.found;
    out.exhausted = pruned.exhausted;
    out.time_limit_reached = pruned.time_limit_reached;
    out.cost = pruned.cost;
    out.alignments = std::move(pruned.alignments);
    out.diagnostics = pruned.diagnostics;
    return out;
  }
  const auto result =
      plain_net(net) && weighted_net(net)
          ? check_alignment(single_trace_log(trace), reference_search_net(net, options.variant),
                            search_options(trace, net, options))
          : check_alignment(single_trace_log(trace), net, search_options(trace, net, options));
  if (!result.computed) {
    // The aligner reports a cancelled search as exhausted; here it throws, as
    // the pruned search does.
    if (result.exhausted) throw_if_cancelled(options.cancelled, "alignment was cancelled");
    out.exhausted = result.exhausted;
    out.time_limit_reached = result.time_limit_reached;
    return out;
  }
  if (result.variants.empty()) return out;
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
  // A search stopped by max_states or max_time_seconds proves nothing about
  // the final marking: every variant reports one uncomputed, exhausted entry.
  const auto stopped = [&out](const SearchOutcome& outcome) {
    PetriTraceAlignment item;
    item.exhausted = true;
    item.time_limit_reached = outcome.time_limit_reached;
    item.diagnostics = outcome.diagnostics;
    out.push_back(std::move(item));
    return out;
  };
  if (!result.found && result.exhausted) return stopped(result);
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
      if (!empty.found && empty.exhausted) return stopped(empty);
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

bool reference_easy_sound(const PetriNet& net, const PetriTraceAlignmentOptions& options) {
  if (plain_net(net) && weighted_net(net))
    return weighted_easy_sound(net, options.initial_marking, options.final_marking, options);
  const auto reachability =
      final_marking_reachability(net, options.initial_marking, options.final_marking);
  // A search that stopped at its limit has not shown the net unsound
  // (review W1-5).
  if (reachability == FinalMarkingReachability::kExhausted)
    throw std::invalid_argument(
        "easy soundness undecided: the reachability search reached its limit "
        "before deciding whether the final marking is reachable");
  return reachability == FinalMarkingReachability::kReachable;
}

std::vector<PetriTraceAlignment> align_petri_log(const EventLog& log, const PetriNet& net,
                                                 const PetriLogAlignmentOptions& options) {
  if (!options.trace.trace_costs.empty())
    throw std::invalid_argument("per-event trace costs cannot apply to a whole log");
  // The public log dispatcher first checks easy soundness and raises for a
  // net whose final marking is unreachable, where a single trace would get
  // None.
  if (!reference_easy_sound(net, options.trace))
    throw std::invalid_argument("trying to apply alignments on a Petri net that is not easy sound");
  const bool standard = options.activity_key == "concept:name" || options.activity_key == "activity";
  std::map<std::vector<std::string>, PetriTraceAlignment> variants;
  std::vector<PetriTraceAlignment> out;
  out.reserve(log.traces.size());
  for (const auto& trace : log.traces) {
    throw_if_cancelled(options.trace.cancelled, "alignment was cancelled");
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

bool reference_log_alignment_applies(const AlignmentOptions& options) {
  const bool single = options.variant == AlignmentVariant::kStateEquationAStar ||
                      options.variant == AlignmentVariant::kDijkstraNoHeuristics ||
                      options.variant == AlignmentVariant::kDijkstraLessMemory;
  return single && !options.default_trace_cost && !options.default_model_cost &&
         !options.default_sync_cost && !options.default_silent_cost &&
         options.trace_costs.empty() && options.model_costs.empty() &&
         options.sync_costs.empty() && options.trace_costs_by_position.empty() &&
         options.model_costs_by_transition.empty() &&
         options.sync_costs_by_transition.empty() && !options.trace_cost_function &&
         !options.model_cost_function && !options.sync_cost_function;
}

AlignmentResult reference_log_alignment(const EventLog& log, const PetriNet& net,
                                        const AlignmentOptions& options) {
  if (!reference_log_alignment_applies(options)) return check_alignment(log, net, options);
  // The less-memory searches read parallel arcs as one (less_memory_net).
  const auto collapsed = options.variant == AlignmentVariant::kDijkstraLessMemory
                             ? less_memory_net(net)
                             : std::optional<PetriNet>{};
  // The bundle aligner groups the variants and keeps precision and bounds.
  // On a net with weighted or parallel arcs it aligns on the net the
  // reference search reads (native firing sums parallel arcs, which can pump
  // tokens through a cycle until the search runs out of states).
  std::optional<PetriNet> searched;
  if (!collapsed && plain_net(net) && weighted_net(net) &&
      options.variant != AlignmentVariant::kDijkstraLessMemory)
    searched = reference_search_net(net, options.variant);
  const PetriNet& bundle_net = collapsed ? *collapsed : searched ? *searched : net;
  auto result = check_alignment(log, bundle_net, options);
  if (!result.computed || result.variants.empty()) return result;

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
  std::vector<PetriTraceAlignment> aligned;
  try {
    aligned = align_petri_log(variants_log, net, reference);
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
  const bool raw_costs = options.cost_profile == AlignmentCostProfile::kStandard;
  const auto reported = [raw_costs](double cost) {
    return raw_costs ? cost : std::floor(cost / kStandardMoveCost);
  };
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
      // Projected markings follow the firing the alignment was searched with.
      fire_petri_transition(bundle_net.transitions().at(*move.transition), marking);
      if (move.model)
        projected.push_back({*move.model, *move.transition, before, marking});
    }
    variant.moves = moves;
    variant.complete_moves = moves;
    variant.optimal_alignments = {std::move(moves)};
    variant.projected_model_moves = std::move(projected);
    variant.cost = reported(entry.cost);
    variant.worst_cost = reported(*entry.bwc);
    variant.fitness = *entry.fitness;
    variant.diagnostics = entry.diagnostics;
    const auto count = static_cast<double>(variant.count);
    raw_cost += entry.cost * count;
    raw_worst += *entry.bwc * count;
    result.total_cost += variant.cost * count;
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

}  // namespace minera
