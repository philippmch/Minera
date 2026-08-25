#include "minera/stochastic_petri.hpp"

#include <algorithm>
#include <cmath>
#include <deque>
#include <limits>
#include <stdexcept>
#include <unordered_map>

namespace minera {
namespace {

struct MarkingHash {
  std::size_t operator()(const std::vector<std::uint32_t>& marking) const noexcept {
    std::size_t hash = 1469598103934665603ULL;
    for (const auto token : marking) {
      hash ^= token + 0x9e3779b9U + (hash << 6U) + (hash >> 2U);
    }
    return hash;
  }
};

bool marking_less(const std::vector<std::uint32_t>& left,
                  const std::vector<std::uint32_t>& right) {
  return std::lexicographical_compare(left.begin(), left.end(), right.begin(),
                                       right.end());
}

std::vector<std::uint32_t> initial_marking(const PetriNet& net,
                                           const StochasticPetriOptions& options) {
  if (!options.initial_marking.empty() &&
      options.initial_marking.size() != net.place_count()) {
    throw std::invalid_argument(
        "stochastic Petri initial marking must match the place count");
  }
  std::vector<std::uint32_t> marking(net.place_count(), 0);
  if (options.initial_marking.empty()) {
    if (net.place_count() == 0 || net.source() >= net.place_count())
      throw std::invalid_argument(
          "stochastic Petri net requires a valid source place");
    marking[net.source()] = 1;
  } else {
    marking = options.initial_marking;
  }
  return marking;
}

void fire_checked(const PetriTransition& transition,
                  std::vector<std::uint32_t>& marking) {
  for (const auto place : transition.inputs) {
    if (place >= marking.size() || marking[place] == 0)
      throw std::invalid_argument("stochastic Petri firing has an invalid input");
    --marking[place];
  }
  for (const auto place : transition.outputs) {
    if (place >= marking.size() ||
        marking[place] == std::numeric_limits<std::uint32_t>::max())
      throw std::invalid_argument(
          "stochastic Petri firing would overflow a marking");
    ++marking[place];
  }
}

}  // namespace

StochasticPetriResult stochastic_petri_reachability(
    const PetriNet& net, const StochasticPetriOptions& options) {
  if (options.max_states == 0)
    throw std::invalid_argument(
        "stochastic Petri max_states must be positive");
  for (const auto& transition : net.transitions()) {
    if (transition.has_extended_arcs())
      throw std::invalid_argument(
          "stochastic Petri reachability supports ordinary arcs only");
  }
  if (!options.transition_rates.empty() &&
      options.transition_rates.size() != net.transitions().size()) {
    throw std::invalid_argument(
        "stochastic Petri transition rates must match the transition count");
  }
  std::vector<double> rates(net.transitions().size(), 1.0);
  if (!options.transition_rates.empty()) rates = options.transition_rates;
  for (const auto rate : rates) {
    if (!std::isfinite(rate) || rate < 0.0)
      throw std::invalid_argument(
          "stochastic Petri transition rates must be finite and non-negative");
  }
  std::vector<bool> immediate(net.transitions().size(), false);
  for (const auto transition : options.immediate_transitions) {
    if (transition >= immediate.size())
      throw std::invalid_argument(
          "stochastic Petri immediate transition index is invalid");
    immediate[transition] = true;
  }

  const auto start = initial_marking(net, options);
  std::vector<std::vector<std::uint32_t>> reachable;
  reachable.reserve(std::min(options.max_states,
                             kStochasticPetriMaxGeneratorStates));
  std::unordered_map<std::vector<std::uint32_t>, std::size_t, MarkingHash> index;
  std::deque<std::vector<std::uint32_t>> queue;
  index.emplace(start, 0);
  reachable.push_back(start);
  queue.push_back(start);
  bool exhausted = false;
  while (!queue.empty()) {
    const auto marking = std::move(queue.front());
    queue.pop_front();
    for (const auto& transition : net.transitions()) {
      if (!petri_transition_enabled(transition, marking)) continue;
      auto next = marking;
      fire_checked(transition, next);
      if (index.contains(next)) continue;
      if (reachable.size() >= options.max_states) {
        exhausted = true;
        queue.clear();
        break;
      }
      if (reachable.size() >= kStochasticPetriMaxGeneratorStates) {
        throw std::invalid_argument(
            "stochastic Petri reachable states exceed the dense generator budget");
      }
      const auto next_index = reachable.size();
      index.emplace(next, next_index);
      reachable.push_back(next);
      queue.push_back(std::move(next));
    }
  }

  std::vector<bool> tangible(reachable.size(), true);
  for (std::size_t state = 0; state < reachable.size(); ++state) {
    for (std::size_t transition = 0; transition < net.transitions().size();
         ++transition) {
      if (immediate[transition] &&
          petri_transition_enabled(net.transitions()[transition],
                                   reachable[state])) {
        tangible[state] = false;
        break;
      }
    }
  }
  // Removing an immediate state also removes every incident edge. A tangible
  // state left with no incident edge is then removed as an isolated vertex.
  std::vector<bool> incident(reachable.size(), false);
  for (std::size_t source = 0; source < reachable.size(); ++source) {
    if (!tangible[source]) continue;
    for (std::size_t transition = 0; transition < net.transitions().size();
         ++transition) {
      if (immediate[transition] ||
          !petri_transition_enabled(net.transitions()[transition],
                                     reachable[source]))
        continue;
      auto next = reachable[source];
      fire_checked(net.transitions()[transition], next);
      const auto found = index.find(next);
      if (found == index.end()) {
        // A state beyond an explicit cap is still evidence that the current
        // tangible state is not isolated; retain the partial source graph.
        if (exhausted) incident[source] = true;
        continue;
      }
      if (!tangible[found->second]) continue;
      incident[source] = true;
      incident[found->second] = true;
    }
  }
  std::vector<std::size_t> order;
  for (std::size_t state = 0; state < reachable.size(); ++state)
    if (tangible[state] && incident[state]) order.push_back(state);
  std::sort(order.begin(), order.end(), [&](std::size_t left, std::size_t right) {
    return marking_less(reachable[left], reachable[right]);
  });
  std::vector<std::size_t> projected(reachable.size(),
                                     std::numeric_limits<std::size_t>::max());
  StochasticPetriResult result;
  result.states.reserve(order.size());
  for (std::size_t state = 0; state < order.size(); ++state) {
    projected[order[state]] = state;
    result.states.push_back({reachable[order[state]]});
  }
  if (result.states.size() != 0 &&
      result.states.size() >
          kStochasticPetriMaxGeneratorEntries / result.states.size()) {
    throw std::invalid_argument(
        "stochastic Petri generator exceeds the dense matrix budget");
  }
  result.generator.assign(result.states.size(),
                          std::vector<double>(result.states.size(), 0.0));
  for (std::size_t source = 0; source < order.size(); ++source) {
    const auto original = order[source];
    for (std::size_t transition = 0; transition < net.transitions().size();
         ++transition) {
      if (immediate[transition] ||
          !petri_transition_enabled(net.transitions()[transition],
                                    reachable[original]))
        continue;
      auto next = reachable[original];
      fire_checked(net.transitions()[transition], next);
      const auto found = index.find(next);
      if (found == index.end() || !tangible[found->second]) continue;
      const auto target = projected[found->second];
      if (target == source) continue;
      result.edges.push_back({source, target, transition, rates[transition]});
      result.generator[source][target] += rates[transition];
    }
    double outgoing = 0.0;
    for (std::size_t target = 0; target < result.states.size(); ++target)
      if (target != source) outgoing += result.generator[source][target];
    result.generator[source][source] = outgoing == 0.0 ? 0.0 : -outgoing;
    for (const auto value : result.generator[source]) {
      if (!std::isfinite(value))
        throw std::invalid_argument(
            "stochastic Petri generator rate sum is not finite");
    }
  }
  result.exhausted = exhausted;
  if (exhausted) result.diagnostic = "max_states exhausted";
  return result;
}

}  // namespace minera
