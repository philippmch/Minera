#include "minera/stochastic_petri.hpp"

#include <algorithm>
#include <cmath>
#include <deque>
#include <limits>
#include <optional>
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

using Marking = std::vector<std::uint32_t>;

void fire_checked(const PetriTransition& transition, Marking& marking);

struct ImmediateChoice {
  std::size_t transition = 0;
  double probability = 0.0;
};

[[nodiscard]] std::vector<ImmediateChoice> enabled_immediate_transitions(
    const PetriNet& net, const Marking& marking,
    const std::vector<bool>& immediate, const std::vector<int>& priorities,
    const std::vector<double>& weights) {
  std::optional<int> highest;
  for (std::size_t transition = 0; transition < net.transitions().size();
       ++transition) {
    if (!immediate[transition] ||
        !petri_transition_enabled(net.transitions()[transition], marking))
      continue;
    if (!highest || priorities[transition] > *highest)
      highest = priorities[transition];
  }
  if (!highest) return {};
  double total = 0.0;
  std::vector<ImmediateChoice> choices;
  for (std::size_t transition = 0; transition < net.transitions().size();
       ++transition) {
    if (!immediate[transition] || priorities[transition] != *highest ||
        !petri_transition_enabled(net.transitions()[transition], marking))
      continue;
    total += weights[transition];
    choices.push_back({transition, 0.0});
  }
  if (!std::isfinite(total))
    throw std::invalid_argument(
        "stochastic Petri immediate activation weights must be finite");
  if (total <= 0.0) {
    const auto probability = 1.0 / static_cast<double>(choices.size());
    for (auto& choice : choices) choice.probability = probability;
  } else {
    for (auto& choice : choices)
      choice.probability = weights[choice.transition] / total;
  }
  return choices;
}

struct ImmediateClosure {
  std::vector<std::size_t> tangible_states;
  std::vector<std::vector<double>> probabilities;
  bool incomplete = false;
};

[[nodiscard]] ImmediateClosure calculate_immediate_closure(
    const PetriNet& net, const std::vector<Marking>& reachable,
    const std::unordered_map<Marking, std::size_t, MarkingHash>& index,
    const std::vector<bool>& tangible, const std::vector<bool>& immediate,
    const std::vector<int>& priorities, const std::vector<double>& weights,
    bool exhausted) {
  ImmediateClosure closure;
  for (std::size_t state = 0; state < reachable.size(); ++state)
    if (tangible[state]) closure.tangible_states.push_back(state);
  const auto tangible_count = closure.tangible_states.size();
  closure.probabilities.assign(
      reachable.size(), std::vector<double>(tangible_count, 0.0));
  std::vector<std::size_t> vanishing;
  std::vector<std::size_t> tangible_column(reachable.size(),
                                           std::numeric_limits<std::size_t>::max());
  for (std::size_t column = 0; column < tangible_count; ++column) {
    const auto state = closure.tangible_states[column];
    tangible_column[state] = column;
    closure.probabilities[state][column] = 1.0;
  }
  for (std::size_t state = 0; state < reachable.size(); ++state)
    if (!tangible[state]) vanishing.push_back(state);
  if (vanishing.empty()) return closure;

  const auto count = vanishing.size();
  std::vector<std::vector<double>> matrix(
      count, std::vector<double>(count + tangible_count, 0.0));
  std::vector<std::size_t> vanishing_row(reachable.size(),
                                         std::numeric_limits<std::size_t>::max());
  std::vector<bool> unresolved(count, false);
  for (std::size_t row = 0; row < count; ++row)
    vanishing_row[vanishing[row]] = row;
  for (std::size_t row = 0; row < count; ++row) {
    const auto state = vanishing[row];
    matrix[row][row] = 1.0;
    for (const auto choice : enabled_immediate_transitions(
             net, reachable[state], immediate, priorities, weights)) {
      auto next = reachable[state];
      fire_checked(net.transitions()[choice.transition], next);
      const auto found = index.find(next);
      if (found == index.end()) {
        if (!exhausted)
          throw std::invalid_argument(
              "stochastic Petri immediate closure reached an unknown marking");
        unresolved[row] = true;
        closure.incomplete = true;
        continue;
      }
      if (tangible[found->second]) {
        matrix[row][count + tangible_column[found->second]] +=
            choice.probability;
      } else {
        matrix[row][vanishing_row[found->second]] -= choice.probability;
      }
    }
  }
  // Complete the vanishing-state linear system. A singular system denotes a
  // closed immediate cycle with no well-defined tangible projection.
  for (std::size_t column = 0; column < count; ++column) {
    std::size_t pivot = column;
    for (std::size_t row = column + 1; row < count; ++row)
      if (std::abs(matrix[row][column]) >
          std::abs(matrix[pivot][column]))
        pivot = row;
    if (std::abs(matrix[pivot][column]) <= 1e-12)
      throw std::invalid_argument(
          "stochastic Petri immediate closure has no tangible resolution");
    std::swap(matrix[pivot], matrix[column]);
    const auto divisor = matrix[column][column];
    for (std::size_t entry = column; entry < matrix[column].size(); ++entry)
      matrix[column][entry] /= divisor;
    for (std::size_t row = 0; row < count; ++row) {
      if (row == column) continue;
      const auto factor = matrix[row][column];
      if (factor == 0.0) continue;
      for (std::size_t entry = column; entry < matrix[row].size(); ++entry)
        matrix[row][entry] -= factor * matrix[column][entry];
    }
  }
  for (std::size_t row = 0; row < count; ++row) {
    if (unresolved[row]) continue;
    double mass = 0.0;
    for (std::size_t column = 0; column < tangible_count; ++column) {
      auto probability = matrix[row][count + column];
      if (probability < 0.0 && probability > -1e-10) probability = 0.0;
      if (!std::isfinite(probability) || probability < 0.0)
        throw std::invalid_argument(
            "stochastic Petri immediate closure produced invalid probabilities");
      closure.probabilities[vanishing[row]][column] = probability;
      mass += probability;
    }
    if (!std::isfinite(mass) || mass <= 1e-12) {
      if (exhausted) {
        closure.incomplete = true;
        continue;
      }
      throw std::invalid_argument(
          "stochastic Petri immediate closure has no tangible resolution");
    }
    if (exhausted && mass < 1.0 - 1e-10) {
      // Keep the surviving sub-probability mass. Renormalizing it would turn
      // a capped partial branch into a false complete distribution.
      closure.incomplete = true;
      continue;
    }
    for (std::size_t column = 0; column < tangible_count; ++column)
      closure.probabilities[vanishing[row]][column] /= mass;
  }
  return closure;
}

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

constexpr double kProbabilityTolerance = 1e-10;
constexpr double kUniformizationTolerance = 1e-13;
constexpr std::size_t kUniformizationTermBudget = 100000;
constexpr std::size_t kUniformizationMaxDenseOperations = 500'000'000;

[[nodiscard]] std::size_t validate_generator(
    const StochasticPetriResult& result) {
  const auto count = result.states.size();
  if (count == 0)
    throw std::invalid_argument("stochastic Petri probability analysis requires states");
  if (result.exhausted)
    throw std::invalid_argument(
        "stochastic Petri probability analysis requires a complete reachability graph");
  if (count > kStochasticPetriMaxGeneratorStates)
    throw std::invalid_argument(
        "stochastic Petri probability analysis exceeds the dense generator budget");
  if (result.generator.size() != count)
    throw std::invalid_argument("stochastic Petri generator dimensions are invalid");
  for (const auto& row : result.generator) {
    if (row.size() != count)
      throw std::invalid_argument("stochastic Petri generator dimensions are invalid");
    for (const auto value : row) {
      if (!std::isfinite(value))
        throw std::invalid_argument("stochastic Petri generator must be finite");
    }
  }
  for (std::size_t source = 0; source < count; ++source) {
    if (result.generator[source][source] > 0.0)
      throw std::invalid_argument(
          "stochastic Petri generator diagonals must be non-positive");
    double row_sum = 0.0;
    for (std::size_t target = 0; target < count; ++target) {
      if (source != target && result.generator[source][target] < 0.0)
        throw std::invalid_argument(
            "stochastic Petri generator off-diagonals must be non-negative");
      row_sum += result.generator[source][target];
    }
    if (!std::isfinite(row_sum) || std::abs(row_sum) > kProbabilityTolerance)
      throw std::invalid_argument(
          "stochastic Petri generator rows must sum to zero");
  }
  return count;
}

void validate_time(const double time_seconds) {
  if (!std::isfinite(time_seconds) || time_seconds < 0.0)
    throw std::invalid_argument(
        "stochastic Petri analysis time must be finite and non-negative");
  if (time_seconds > kStochasticPetriMaxTransientTimeSeconds)
    throw std::invalid_argument(
        "stochastic Petri analysis time exceeds the transient safety limit");
}

[[nodiscard]] std::vector<double> validate_probability_vector(
    const StochasticPetriResult& result,
    const std::vector<double>& probabilities) {
  const auto count = validate_generator(result);
  if (probabilities.size() != count)
    throw std::invalid_argument(
        "stochastic Petri initial probability vector must match the state count");
  double mass = 0.0;
  for (const auto probability : probabilities) {
    if (!std::isfinite(probability) || probability < 0.0)
      throw std::invalid_argument(
          "stochastic Petri initial probabilities must be finite and non-negative");
    mass += probability;
  }
  if (!std::isfinite(mass) || std::abs(mass - 1.0) > kProbabilityTolerance)
    throw std::invalid_argument(
        "stochastic Petri initial probabilities must have unit mass");
  return probabilities;
}

[[nodiscard]] std::vector<double> apply_uniformized_step(
    const std::vector<double>& vector, const std::vector<std::vector<double>>& q,
    const double lambda) {
  const auto count = vector.size();
  std::vector<double> next(count, 0.0);
  for (std::size_t source = 0; source < count; ++source) {
    if (vector[source] == 0.0) continue;
    for (std::size_t target = 0; target < count; ++target) {
      const double transition = source == target
                                    ? 1.0 + q[source][target] / lambda
                                    : q[source][target] / lambda;
      if (transition != 0.0) next[target] += vector[source] * transition;
    }
  }
  return next;
}

[[nodiscard]] std::vector<double> normalize_probabilities(
    std::vector<double> probabilities) {
  double mass = 0.0;
  for (auto& probability : probabilities) {
    if (probability < 0.0 && probability > -kProbabilityTolerance)
      probability = 0.0;
    if (!std::isfinite(probability) || probability < 0.0)
      throw std::runtime_error(
          "stochastic Petri probability propagation became non-finite");
    mass += probability;
  }
  if (!std::isfinite(mass) || mass <= 0.0)
    throw std::runtime_error(
        "stochastic Petri probability propagation lost all mass");
  for (auto& probability : probabilities) probability /= mass;
  return probabilities;
}

[[nodiscard]] std::vector<double> propagate_uniformization(
    const StochasticPetriResult& result, std::vector<double> probabilities,
    const double time_seconds) {
  validate_time(time_seconds);
  const auto count = validate_generator(result);
  if (time_seconds == 0.0) return normalize_probabilities(std::move(probabilities));

  double lambda = 0.0;
  for (std::size_t state = 0; state < count; ++state)
    lambda = std::max(lambda, -result.generator[state][state]);
  if (!std::isfinite(lambda) || lambda < 0.0)
    throw std::invalid_argument("stochastic Petri generator has invalid exit rates");
  if (lambda == 0.0) return normalize_probabilities(std::move(probabilities));

  // Subdivide long intervals so the Poisson series stays numerically stable
  // and does not require representable exp(-lambda*time) values near zero.
  constexpr double target_mean = 32.0;
  const double total_mean = lambda * time_seconds;
  if (!std::isfinite(total_mean))
    throw std::invalid_argument(
        "stochastic Petri transient interval has an overflowing mean");
  const double requested_slices =
      std::max(1.0, std::ceil(total_mean / target_mean));
  if (!std::isfinite(requested_slices) ||
      requested_slices > static_cast<double>(kUniformizationTermBudget))
    throw std::invalid_argument(
        "stochastic Petri transient interval exceeds the propagation budget");
  const auto slices = static_cast<std::size_t>(requested_slices);
  const double slice_time = time_seconds / static_cast<double>(slices);
  const auto matrix_operations = count * count;
  const double estimated_terms = std::min(
      static_cast<double>(kUniformizationTermBudget),
      std::ceil((lambda * slice_time) + 64.0));
  const long double estimated_operations =
      static_cast<long double>(slices) * estimated_terms *
      static_cast<long double>(matrix_operations);
  if (estimated_operations >
      static_cast<long double>(kUniformizationMaxDenseOperations))
    throw std::invalid_argument(
        "stochastic Petri transient interval exceeds the work budget");
  std::size_t operations = 0;
  for (std::size_t slice = 0; slice < slices; ++slice) {
    const double mean = lambda * slice_time;
    auto current = probabilities;
    auto propagated = std::vector<double>(count, 0.0);
    double poisson = std::exp(-mean);
    for (std::size_t state = 0; state < count; ++state)
      propagated[state] = poisson * current[state];
    std::size_t terms = 0;
    for (std::size_t term = 1; term <= kUniformizationTermBudget; ++term) {
      if (matrix_operations > kUniformizationMaxDenseOperations - operations)
        throw std::invalid_argument(
            "stochastic Petri transient interval exceeds the work budget");
      operations += matrix_operations;
      current = apply_uniformized_step(current, result.generator, lambda);
      poisson *= mean / static_cast<double>(term);
      if (!std::isfinite(poisson))
        throw std::runtime_error(
            "stochastic Petri transient propagation became non-finite");
      for (std::size_t state = 0; state < count; ++state)
        propagated[state] += poisson * current[state];
      terms = term;
      if (term > mean && poisson <= kUniformizationTolerance) break;
    }
    if (terms == kUniformizationTermBudget &&
        poisson > kUniformizationTolerance)
      throw std::invalid_argument(
          "stochastic Petri transient interval exceeds the propagation budget");
    probabilities = normalize_probabilities(std::move(propagated));
  }
  return probabilities;
}

[[nodiscard]] std::vector<std::vector<std::size_t>> generator_graph(
    const StochasticPetriResult& result) {
  const auto count = validate_generator(result);
  std::vector<std::vector<std::size_t>> graph(count);
  for (std::size_t source = 0; source < count; ++source) {
    for (std::size_t target = 0; target < count; ++target) {
      if (source != target && result.generator[source][target] > 0.0)
        graph[source].push_back(target);
    }
  }
  return graph;
}

struct SccResult {
  std::vector<std::size_t> component;
  std::size_t count = 0;
};

[[nodiscard]] SccResult strongly_connected_components(
    const std::vector<std::vector<std::size_t>>& graph) {
  const auto count = graph.size();
  std::vector<std::vector<std::size_t>> reverse(count);
  for (std::size_t source = 0; source < count; ++source)
    for (const auto target : graph[source]) reverse[target].push_back(source);
  std::vector<bool> visited(count, false);
  std::vector<std::size_t> order;
  order.reserve(count);
  const auto visit = [&](const auto& self, const std::size_t node,
                         const auto& edges) -> void {
    if (visited[node]) return;
    visited[node] = true;
    for (const auto next : edges[node]) self(self, next, edges);
    order.push_back(node);
  };
  for (std::size_t node = 0; node < count; ++node) visit(visit, node, graph);
  std::fill(visited.begin(), visited.end(), false);
  SccResult result;
  result.component.assign(count, std::numeric_limits<std::size_t>::max());
  const auto assign = [&](const auto& self, const std::size_t node,
                          const std::size_t component) -> void {
    if (visited[node]) return;
    visited[node] = true;
    result.component[node] = component;
    for (const auto next : reverse[node]) self(self, next, component);
  };
  for (auto it = order.rbegin(); it != order.rend(); ++it) {
    if (visited[*it]) continue;
    assign(assign, *it, result.count++);
  }
  return result;
}

[[nodiscard]] std::vector<double> solve_linear_system(
    std::vector<std::vector<double>> matrix, std::vector<double> rhs) {
  const auto count = rhs.size();
  for (std::size_t column = 0; column < count; ++column) {
    std::size_t pivot = column;
    for (std::size_t row = column + 1; row < count; ++row)
      if (std::abs(matrix[row][column]) >
          std::abs(matrix[pivot][column])) pivot = row;
    if (std::abs(matrix[pivot][column]) <= 1e-12)
      throw std::runtime_error("stochastic Petri steady state is singular");
    std::swap(matrix[pivot], matrix[column]);
    std::swap(rhs[pivot], rhs[column]);
    const double divisor = matrix[column][column];
    for (std::size_t entry = column; entry < count; ++entry)
      matrix[column][entry] /= divisor;
    rhs[column] /= divisor;
    for (std::size_t row = 0; row < count; ++row) {
      if (row == column) continue;
      const double factor = matrix[row][column];
      if (factor == 0.0) continue;
      for (std::size_t entry = column; entry < count; ++entry)
        matrix[row][entry] -= factor * matrix[column][entry];
      rhs[row] -= factor * rhs[column];
    }
  }
  return rhs;
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
  if (!options.transition_priorities.empty() &&
      options.transition_priorities.size() != net.transitions().size()) {
    throw std::invalid_argument(
        "stochastic Petri transition priorities must match the transition count");
  }
  std::vector<int> priorities(net.transitions().size(), 0);
  if (!options.transition_priorities.empty())
    priorities = options.transition_priorities;
  std::vector<bool> immediate(net.transitions().size(), false);
  for (const auto transition : options.immediate_transitions) {
    if (transition >= immediate.size())
      throw std::invalid_argument(
          "stochastic Petri immediate transition index is invalid");
    immediate[transition] = true;
  }

  std::vector<double> immediate_weights(net.transitions().size(), 1.0);
  if (!options.immediate_weights.empty() &&
      options.immediate_weights.size() != net.transitions().size()) {
    throw std::invalid_argument(
        "stochastic Petri immediate weights must match the transition count");
  }
  if (!options.immediate_weights.empty())
    immediate_weights = options.immediate_weights;
  for (const auto weight : immediate_weights) {
    if (!std::isfinite(weight) || weight < 0.0)
      throw std::invalid_argument(
          "stochastic Petri immediate weights must be finite and non-negative");
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
    const auto choices = enabled_immediate_transitions(
        net, marking, immediate, priorities, immediate_weights);
    std::vector<bool> eligible(net.transitions().size(), false);
    for (const auto choice : choices) eligible[choice.transition] = true;
    for (std::size_t transition_index = 0;
         transition_index < net.transitions().size(); ++transition_index) {
      const auto& transition = net.transitions()[transition_index];
      if (!petri_transition_enabled(transition, marking)) continue;
      if (!choices.empty()) {
        if (!eligible[transition_index]) continue;
      } else if (immediate[transition_index]) {
        continue;
      }
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
  const auto closure = calculate_immediate_closure(
      net, reachable, index, tangible, immediate, priorities,
      immediate_weights, exhausted);
  struct EffectiveEdge {
    std::size_t source = 0;
    std::size_t target = 0;
    std::size_t transition = 0;
    double rate = 0.0;
  };
  std::vector<EffectiveEdge> effective_edges;
  std::vector<bool> incident(reachable.size(), false);
  for (const auto source : closure.tangible_states) {
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
        if (exhausted) incident[source] = true;
        continue;
      }
      for (std::size_t target_column = 0;
           target_column < closure.tangible_states.size(); ++target_column) {
        const auto probability = closure.probabilities[found->second][target_column];
        if (probability <= 1e-12) continue;
        const auto target = closure.tangible_states[target_column];
        const auto rate = rates[transition] * probability;
        if (!std::isfinite(rate))
          throw std::invalid_argument(
              "stochastic Petri effective rate is not finite");
        effective_edges.push_back({source, target, transition, rate});
        if (target != source) {
          incident[source] = true;
          incident[target] = true;
        }
      }
    }
  }
  const auto start_index = index.find(start);
  const auto start_is_vanishing =
      start_index != index.end() && !tangible[start_index->second];
  if (start_is_vanishing) {
    for (std::size_t column = 0; column < closure.tangible_states.size();
         ++column)
      if (closure.probabilities[start_index->second][column] > 1e-12)
        incident[closure.tangible_states[column]] = true;
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
  if (start_index != index.end() && !closure.incomplete) {
    result.initial_probabilities.assign(order.size(), 0.0);
    if (tangible[start_index->second]) {
      const auto projected_start = projected[start_index->second];
      if (projected_start != std::numeric_limits<std::size_t>::max())
        result.initial_probabilities[projected_start] = 1.0;
    } else {
      for (std::size_t column = 0; column < closure.tangible_states.size();
           ++column) {
        const auto projected_target =
            projected[closure.tangible_states[column]];
        if (projected_target != std::numeric_limits<std::size_t>::max())
          result.initial_probabilities[projected_target] =
              closure.probabilities[start_index->second][column];
      }
    }
    for (std::size_t state = 0; state < result.initial_probabilities.size();
         ++state) {
      if (result.initial_probabilities[state] > 1.0 - 1e-12) {
        result.initial_state = state;
        break;
      }
    }
  }
  if (result.states.size() != 0 &&
      result.states.size() >
          kStochasticPetriMaxGeneratorEntries / result.states.size()) {
    throw std::invalid_argument(
        "stochastic Petri generator exceeds the dense matrix budget");
  }
  result.generator.assign(result.states.size(),
                          std::vector<double>(result.states.size(), 0.0));
  for (const auto& edge : effective_edges) {
    const auto source = projected[edge.source];
    const auto target = projected[edge.target];
    if (source == std::numeric_limits<std::size_t>::max() ||
        target == std::numeric_limits<std::size_t>::max() || source == target)
      continue;
    result.edges.push_back({source, target, edge.transition, edge.rate});
    result.generator[source][target] += edge.rate;
  }
  std::sort(result.edges.begin(), result.edges.end(),
            [](const auto& left, const auto& right) {
              if (left.source != right.source) return left.source < right.source;
              if (left.target != right.target) return left.target < right.target;
              if (left.transition != right.transition)
                return left.transition < right.transition;
              return left.rate < right.rate;
            });
  for (std::size_t source = 0; source < order.size(); ++source) {
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

StochasticPetriDistributionResult stochastic_petri_transient(
    const StochasticPetriResult& result, const double time_seconds,
    const std::size_t initial_state) {
  const auto count = validate_generator(result);
  if (initial_state >= count)
    throw std::invalid_argument("stochastic Petri initial state is invalid");
  std::vector<double> probabilities(count, 0.0);
  probabilities[initial_state] = 1.0;
  auto output = stochastic_petri_transient(result, time_seconds, probabilities);
  output.initial_state = initial_state;
  return output;
}

StochasticPetriDistributionResult stochastic_petri_transient(
    const StochasticPetriResult& result, const double time_seconds,
    const std::vector<double>& initial_probabilities) {
  const auto probabilities = validate_probability_vector(result,
                                                         initial_probabilities);
  auto output = StochasticPetriDistributionResult{};
  output.variant = StochasticPetriAnalysisVariant::kTransient;
  output.time_seconds = time_seconds;
  output.probabilities = propagate_uniformization(result, probabilities,
                                                  time_seconds);
  output.computed = true;
  output.unique = true;
  return output;
}

StochasticPetriDistributionResult stochastic_petri_steady_state(
    const StochasticPetriResult& result) {
  auto output = StochasticPetriDistributionResult{};
  output.variant = StochasticPetriAnalysisVariant::kSteadyState;
  if (result.states.empty()) {
    output.computed = false;
    output.unique = false;
    output.diagnostic = "stochastic Petri steady state requires states";
    return output;
  }
  const auto count = validate_generator(result);
  output.probabilities.assign(count, 0.0);

  const auto graph = generator_graph(result);
  const auto scc = strongly_connected_components(graph);
  std::vector<bool> closed(scc.count, true);
  for (std::size_t source = 0; source < count; ++source)
    for (const auto target : graph[source])
      if (scc.component[source] != scc.component[target])
        closed[scc.component[source]] = false;
  std::vector<std::size_t> recurrent;
  for (std::size_t component = 0; component < scc.count; ++component)
    if (closed[component]) recurrent.push_back(component);
  if (recurrent.empty()) {
    output.computed = false;
    output.unique = false;
    output.diagnostic = "stochastic Petri graph has no recurrent class";
    return output;
  }
  if (recurrent.size() != 1) {
    output.computed = false;
    output.unique = false;
    output.diagnostic = "stochastic Petri graph has multiple recurrent classes";
    return output;
  }

  std::vector<std::size_t> class_states;
  for (std::size_t state = 0; state < count; ++state)
    if (scc.component[state] == recurrent.front()) class_states.push_back(state);
  std::sort(class_states.begin(), class_states.end());
  const auto class_count = class_states.size();
  double rate_scale = 0.0;
  for (const auto state : class_states)
    rate_scale = std::max(rate_scale, -result.generator[state][state]);
  std::vector<std::vector<double>> matrix(
      class_count, std::vector<double>(class_count, 0.0));
  std::vector<double> rhs(class_count, 0.0);
  if (class_count == 1) {
    output.probabilities[class_states.front()] = 1.0;
  } else {
    if (!std::isfinite(rate_scale) || rate_scale <= 0.0)
      throw std::runtime_error(
          "stochastic Petri steady state has invalid recurrent rates");
    for (std::size_t row = 0; row + 1 < class_count; ++row) {
      for (std::size_t column = 0; column < class_count; ++column)
        matrix[row][column] =
            result.generator[class_states[column]][class_states[row]] /
            rate_scale;
    }
    for (std::size_t column = 0; column < class_count; ++column)
      matrix[class_count - 1][column] = 1.0;
    rhs[class_count - 1] = 1.0;
    const auto solution = solve_linear_system(std::move(matrix), std::move(rhs));
    for (std::size_t index = 0; index < class_count; ++index) {
      if (solution[index] < -kProbabilityTolerance ||
          !std::isfinite(solution[index]))
        throw std::runtime_error(
            "stochastic Petri steady state has invalid probabilities");
      output.probabilities[class_states[index]] =
          std::max(0.0, solution[index]);
    }
    output.probabilities = normalize_probabilities(
        std::move(output.probabilities));
  }
  output.computed = true;
  output.unique = true;
  return output;
}

}  // namespace minera
