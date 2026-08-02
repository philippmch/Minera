#include "minera/genetic_miner.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>
#include <random>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

#include "minera/conformance.hpp"

namespace minera {
namespace {

constexpr std::size_t kHardActivities = 32;
constexpr std::size_t kHardPopulation = 10000;
constexpr std::size_t kHardGenerations = 10000;
constexpr std::size_t kHardStates = 1000000;

using Partition = std::vector<std::size_t>;

struct Individual {
  std::vector<std::vector<Partition>> input;
  std::vector<std::vector<Partition>> output;
};

struct NormalizedLog {
  std::vector<std::string> names;
  std::vector<std::vector<std::size_t>> traces;
};

struct Scored {
  Individual individual;
  double fitness = 0.0;
};

bool vector_less(const Partition& a, const Partition& b) { return a < b; }

void normalize_partitions(std::vector<Partition>& partitions) {
  for (auto& partition : partitions) {
    std::sort(partition.begin(), partition.end());
    partition.erase(std::unique(partition.begin(), partition.end()),
                    partition.end());
  }
  partitions.erase(std::remove_if(partitions.begin(), partitions.end(),
                                  [](const auto& p) { return p.empty(); }),
                   partitions.end());
  std::sort(partitions.begin(), partitions.end(), vector_less);
  partitions.erase(std::unique(partitions.begin(), partitions.end()),
                   partitions.end());
}

void normalize_individual(Individual& individual) {
  for (auto& partitions : individual.input) normalize_partitions(partitions);
  for (auto& partitions : individual.output) normalize_partitions(partitions);
}

bool individual_less(const Individual& a, const Individual& b) {
  if (a.input != b.input) return a.input < b.input;
  return a.output < b.output;
}

std::vector<Partition> random_partition(std::vector<std::size_t> values,
                                         std::mt19937_64& rng) {
  std::sort(values.begin(), values.end());
  values.erase(std::unique(values.begin(), values.end()), values.end());
  std::vector<Partition> result;
  while (!values.empty()) {
    std::shuffle(values.begin(), values.end(), rng);
    std::uniform_int_distribution<std::size_t> pick(1, values.size());
    const std::size_t count = pick(rng);
    Partition part(values.begin(), values.begin() + count);
    values.erase(values.begin(), values.begin() + count);
    std::sort(part.begin(), part.end());
    result.push_back(std::move(part));
  }
  normalize_partitions(result);
  return result;
}

std::vector<std::size_t> flatten(const std::vector<Partition>& partitions) {
  std::vector<std::size_t> values;
  for (const auto& partition : partitions)
    values.insert(values.end(), partition.begin(), partition.end());
  std::sort(values.begin(), values.end());
  values.erase(std::unique(values.begin(), values.end()), values.end());
  return values;
}

bool contains(const Partition& partition, std::size_t value) {
  return std::binary_search(partition.begin(), partition.end(), value);
}

void add_singleton(std::vector<Partition>& partitions, std::size_t value) {
  for (const auto& partition : partitions)
    if (contains(partition, value)) return;
  partitions.push_back({value});
  normalize_partitions(partitions);
}

void repair(Individual& individual, const std::vector<std::vector<std::size_t>>&
                                    causal) {
  const std::size_t n = individual.input.size();
  for (std::size_t i = 0; i < n; ++i) {
    if (i >= individual.output.size()) individual.output.resize(n);
    if (individual.input[i].empty()) individual.input[i] = {};
    if (individual.output[i].empty()) individual.output[i] = {};
  }

  // Connect weak components through deterministic, causally strongest arcs.
  std::vector<std::size_t> parent(n);
  std::iota(parent.begin(), parent.end(), 0);
  const auto find = [&](auto self, std::size_t x) -> std::size_t {
    return parent[x] == x ? x : parent[x] = self(self, parent[x]);
  };
  const auto join = [&](std::size_t a, std::size_t b) {
    const auto ra = find(find, a), rb = find(find, b);
    if (ra != rb) parent[rb] = ra;
  };
  for (std::size_t source = 0; source < n; ++source)
    for (const auto target : flatten(individual.output[source])) join(source, target);
  for (std::size_t target = 0; target < n; ++target)
    for (const auto source : flatten(individual.input[target])) join(source, target);

  while (true) {
    std::vector<std::size_t> roots;
    for (std::size_t i = 0; i < n; ++i)
      if (find(find, i) == i) roots.push_back(i);
    if (roots.size() <= 1) break;
    std::sort(roots.begin(), roots.end());
    const std::size_t left = roots[0];
    const std::size_t right = roots[1];
    std::size_t from = left, to = right;
    std::uint64_t best = 0;
    for (std::size_t a = 0; a < n; ++a) {
      if (find(find, a) != left) continue;
      for (std::size_t b = 0; b < n; ++b) {
        if (find(find, b) != right) continue;
        const auto weight = causal[a][b];
        if (weight > best || (weight == best && std::pair{a, b} < std::pair{from, to})) {
          best = weight;
          from = a;
          to = b;
        }
      }
    }
    add_singleton(individual.output[from], to);
    add_singleton(individual.input[to], from);
    parent[right] = left;
  }
  // Keep both projections of the causal matrix coherent after crossover or
  // mutation. Existing partition membership is retained; missing inverse
  // partitions are restored without changing their grouping semantics.
  for (std::size_t source = 0; source < n; ++source)
    for (const auto& partition : individual.output[source])
      for (const auto target : partition)
        if (std::none_of(individual.input[target].begin(), individual.input[target].end(),
                         [&](const auto& inverse) { return contains(inverse, source); }))
          add_singleton(individual.input[target], source);
  for (std::size_t target = 0; target < n; ++target)
    for (const auto& partition : individual.input[target])
      for (const auto source : partition)
        if (std::none_of(individual.output[source].begin(), individual.output[source].end(),
                         [&](const auto& inverse) { return contains(inverse, target); }))
          add_singleton(individual.output[source], target);
  normalize_individual(individual);
}

NormalizedLog normalize(const EventLog& log, const GeneticMinerOptions& options) {
  if (log.traces.empty() || log.event_count() == 0)
    throw std::invalid_argument("genetic miner requires a non-empty event log");
  std::set<std::string> names;
  std::vector<std::vector<std::string>> raw;
  for (const auto& source_trace : log.traces) {
    Trace trace = source_trace;
    order_events_by_time(trace.events);
    std::vector<std::string> sequence;
    for (const auto& event : trace.events) {
      std::string label = event.activity;
      if (!options.activity_key.empty() && options.activity_key != "activity" &&
          options.activity_key != "concept:name") {
        if (options.activity_key == "resource" ||
            options.activity_key == "org:resource") {
          label = event.resource;
        } else if (const auto* value = event.attribute(options.activity_key)) {
          label = *value;
        }
      }
      if (!label.empty()) {
        names.insert(label);
        sequence.push_back(std::move(label));
      }
    }
    if (!sequence.empty()) raw.push_back(std::move(sequence));
  }
  if (raw.empty() || names.empty())
    throw std::invalid_argument("genetic miner found no activities");
  if (names.size() > options.max_activities || names.size() > kHardActivities)
    throw std::invalid_argument("genetic miner activity bound exceeded");
  NormalizedLog normalized;
  normalized.names.assign(names.begin(), names.end());
  std::map<std::string, std::size_t> ids;
  for (std::size_t i = 0; i < normalized.names.size(); ++i)
    ids.emplace(normalized.names[i], i);
  for (const auto& sequence : raw) {
    std::vector<std::size_t> encoded;
    for (const auto& label : sequence) encoded.push_back(ids.at(label));
    normalized.traces.push_back(std::move(encoded));
  }
  return normalized;
}

Individual initial_individual(const std::vector<std::vector<std::uint64_t>>& causal,
                              std::mt19937_64& rng) {
  const std::size_t n = causal.size();
  Individual individual;
  individual.input.resize(n);
  individual.output.resize(n);
  for (std::size_t source = 0; source < n; ++source) {
    std::vector<std::size_t> neighbors;
    const auto total = std::accumulate(causal[source].begin(), causal[source].end(),
                                       std::uint64_t{0});
    if (total != 0) {
      for (std::size_t target = 0; target < n; ++target) {
        const double probability = static_cast<double>(causal[source][target]) /
                                   static_cast<double>(total);
        std::bernoulli_distribution draw(probability);
        if (draw(rng)) neighbors.push_back(target);
      }
    }
    individual.output[source] = random_partition(std::move(neighbors), rng);
  }
  for (std::size_t target = 0; target < n; ++target) {
    std::vector<std::size_t> neighbors;
    for (std::size_t source = 0; source < n; ++source) {
      if (contains(flatten(individual.output[source]), target)) neighbors.push_back(source);
    }
    individual.input[target] = random_partition(std::move(neighbors), rng);
  }
  return individual;
}

PetriNet to_petri_net(const Individual& individual,
                      const std::vector<std::string>& names) {
  const std::size_t n = names.size();
  PetriNet net;
  net.set_activities(names);
  const std::size_t source = net.add_place();
  const std::size_t sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);

  // A sophisticated place is identified by its complete predecessor and
  // successor sets.  The compatibility checks are the matrix definition:
  // every predecessor offers the successor partition and vice versa.
  using PlaceKey = std::pair<Partition, Partition>;
  std::set<PlaceKey> keys;
  std::vector<Partition> all_inputs, all_outputs;
  for (const auto& partitions : individual.input)
    all_inputs.insert(all_inputs.end(), partitions.begin(), partitions.end());
  for (const auto& partitions : individual.output)
    all_outputs.insert(all_outputs.end(), partitions.begin(), partitions.end());
  normalize_partitions(all_inputs);
  normalize_partitions(all_outputs);
  for (const auto& predecessors : all_inputs) {
    for (const auto& successors : all_outputs) {
      bool compatible = true;
      for (const auto predecessor : predecessors) {
        bool found = false;
        for (const auto& partition : individual.output[predecessor])
          if (partition == successors) { found = true; break; }
        if (!found) { compatible = false; break; }
      }
      if (!compatible) continue;
      for (const auto successor : successors) {
        bool found = false;
        for (const auto& partition : individual.input[successor])
          if (partition == predecessors) { found = true; break; }
        if (!found) { compatible = false; break; }
      }
      if (compatible) keys.emplace(predecessors, successors);
    }
  }
  std::map<PlaceKey, std::size_t> places;
  for (const auto& key : keys) places.emplace(key, net.add_place());
  std::vector<std::vector<std::vector<std::size_t>>> input_alternatives(n),
      output_alternatives(n);
  for (std::size_t activity = 0; activity < n; ++activity) {
    for (const auto& partition : individual.input[activity]) {
      std::vector<std::size_t> alternative;
      for (const auto& [key, place] : places)
        if (key.first == partition && contains(key.second, activity))
          alternative.push_back(place);
      if (!alternative.empty()) input_alternatives[activity].push_back(std::move(alternative));
    }
    for (const auto& partition : individual.output[activity]) {
      std::vector<std::size_t> alternative;
      for (const auto& [key, place] : places)
        if (contains(key.first, activity) && key.second == partition)
          alternative.push_back(place);
      if (!alternative.empty()) output_alternatives[activity].push_back(std::move(alternative));
    }
  }
  // A purely cyclic candidate has no structural source/sink activity. Route
  // one deterministic boundary activity through the workflow boundaries so
  // the returned net remains reachable and has canonical markings.
  if (std::none_of(input_alternatives.begin(), input_alternatives.end(),
                   [](const auto& alternatives) { return alternatives.empty(); }))
    input_alternatives.front().push_back({source});
  if (std::none_of(output_alternatives.begin(), output_alternatives.end(),
                   [](const auto& alternatives) { return alternatives.empty(); }))
    output_alternatives.back().push_back({sink});
  for (std::size_t activity = 0; activity < n; ++activity) {
    auto merge = [&](std::vector<std::vector<std::size_t>> alternatives,
                     bool joining, std::size_t boundary) {
      if (alternatives.empty()) alternatives.push_back({boundary});
      if (alternatives.size() == 1) return std::move(alternatives.front());
      const std::size_t hub = net.add_place();
      for (auto& alternative : alternatives) {
        if (joining)
          net.add_transition(std::nullopt, std::move(alternative), {hub});
        else
          net.add_transition(std::nullopt, {hub}, std::move(alternative));
      }
      return std::vector<std::size_t>{hub};
    };
    auto inputs = merge(std::move(input_alternatives[activity]), true, source);
    auto outputs = merge(std::move(output_alternatives[activity]), false, sink);
    net.add_transition(activity, std::move(inputs), std::move(outputs));
  }
  return net;
}

double fitness(const EventLog& log, const PetriNet& net) {
  const auto conformance = check_conformance(log, net);
  if (conformance.trace_count == 0) return 0.0;
  double average = 0.0;
  for (const auto& variant : conformance.variants)
    average += variant.fitness * static_cast<double>(variant.count);
  average /= static_cast<double>(conformance.trace_count);
  const double fitting = static_cast<double>(conformance.fitting_traces) /
                         static_cast<double>(conformance.trace_count);
  return std::clamp(0.4 * average + 0.6 * fitting, 0.0, 1.0);
}

Individual crossover(Individual first, const Individual& second,
                     std::mt19937_64& rng,
                     const std::vector<std::vector<std::uint64_t>>& causal) {
  if (first.input.empty()) return first;
  std::uniform_int_distribution<std::size_t> activity(0, first.input.size() - 1);
  const auto at = activity(rng);
  if (!first.input[at].empty() && !second.input[at].empty()) {
    const std::size_t split = std::uniform_int_distribution<std::size_t>(
        0, std::min(first.input[at].size(), second.input[at].size()) - 1)(rng);
    first.input[at].resize(split);
    first.input[at].insert(first.input[at].end(), second.input[at].begin() + split,
                           second.input[at].end());
  }
  if (!first.output[at].empty() && !second.output[at].empty()) {
    const std::size_t split = std::uniform_int_distribution<std::size_t>(
        0, std::min(first.output[at].size(), second.output[at].size()) - 1)(rng);
    first.output[at].resize(split);
    first.output[at].insert(first.output[at].end(), second.output[at].begin() + split,
                            second.output[at].end());
  }
  repair(first, causal);
  return first;
}

void mutate(Individual& individual, double rate, std::mt19937_64& rng,
            const std::vector<std::vector<std::uint64_t>>& causal) {
  std::bernoulli_distribution mutate_draw(rate);
  for (auto& partitions : individual.input)
    if (mutate_draw(rng)) partitions = random_partition(flatten(partitions), rng);
  for (auto& partitions : individual.output)
    if (mutate_draw(rng)) partitions = random_partition(flatten(partitions), rng);
  repair(individual, causal);
}

}  // namespace

GeneticMinerResult mine_genetic(const EventLog& event_log,
                                const GeneticMinerOptions& options) {
  if (options.population_size < 2 || options.population_size > kHardPopulation)
    throw std::invalid_argument("genetic miner population_size must be in [2,10000]");
  if (options.generations > kHardGenerations)
    throw std::invalid_argument("genetic miner generations safety bound exceeded");
  if (options.max_states == 0 || options.max_states > kHardStates)
    throw std::invalid_argument("genetic miner max_states must be in [1,1000000]");
  const auto valid_rate = [](double value) {
    return std::isfinite(value) && value >= 0.0 && value <= 1.0;
  };
  if (!valid_rate(options.elitism_rate) || !valid_rate(options.crossover_rate) ||
      !valid_rate(options.mutation_rate))
    throw std::invalid_argument("genetic miner rates must be finite and in [0,1]");
  if (options.elitism_min_sample == 0 ||
      options.elitism_min_sample >= options.population_size)
    throw std::invalid_argument("genetic miner elitism_min_sample must be in [1,population)");

  const auto log = normalize(event_log, options);
  GeneticMinerResult result;
  result.diagnostics.seed = options.seed;
  result.diagnostics.population_size = options.population_size;
  result.diagnostics.activity_count = log.names.size();

  const std::size_t n = log.names.size();
  std::vector<std::vector<std::uint64_t>> causal(n, std::vector<std::uint64_t>(n));
  for (const auto& trace : log.traces)
    for (std::size_t i = 1; i < trace.size(); ++i)
      if (causal[trace[i - 1]][trace[i]] != std::numeric_limits<std::uint64_t>::max())
        ++causal[trace[i - 1]][trace[i]];

  std::mt19937_64 rng(options.seed);
  std::vector<Scored> population;
  population.reserve(options.population_size);
  for (std::size_t i = 0; i < options.population_size; ++i) {
    auto candidate = initial_individual(causal, rng);
    repair(candidate, causal);
    population.push_back({std::move(candidate), 0.0});
  }

  const auto evaluate = [&](std::vector<Scored>& candidates) {
    for (auto& candidate : candidates) {
      if (result.diagnostics.candidates_evaluated >= options.max_states) {
        result.diagnostics.exhausted = true;
        break;
      }
      candidate.fitness = fitness(event_log, to_petri_net(candidate.individual, log.names));
      ++result.diagnostics.candidates_evaluated;
    }
    std::sort(candidates.begin(), candidates.end(), [](const Scored& a, const Scored& b) {
      if (a.fitness != b.fitness) return a.fitness > b.fitness;
      return individual_less(a.individual, b.individual);
    });
  };
  evaluate(population);
  if (population.empty()) throw std::runtime_error("genetic miner produced no candidates");
  Individual best = population.front().individual;
  double best_fitness = population.front().fitness;
  result.diagnostics.best_fitness = best_fitness;
  std::vector<double> history;
  for (std::size_t generation = 0; generation < options.generations; ++generation) {
    if (best_fitness >= 1.0 - 1e-12 || result.diagnostics.exhausted) break;
    const std::size_t stable_window = std::max<std::size_t>(1, options.generations / 2);
    if (history.size() >= stable_window &&
        std::all_of(history.end() - static_cast<std::ptrdiff_t>(stable_window), history.end(),
                    [&](double value) { return std::abs(value - best_fitness) < 1e-12; }))
      break;
    history.push_back(best_fitness);
    const std::size_t elite = static_cast<std::size_t>(
        std::llround(options.elitism_rate * static_cast<double>(options.population_size)));
    std::vector<Scored> next;
    next.reserve(options.population_size);
    for (std::size_t i = 0; i < std::min(elite, population.size()); ++i)
      next.push_back({population[i].individual, population[i].fitness});
    while (next.size() < options.population_size) {
      std::vector<std::size_t> indices(population.size());
      std::iota(indices.begin(), indices.end(), 0);
      std::shuffle(indices.begin(), indices.end(), rng);
      std::size_t parent1 = indices[0], parent2 = indices[1];
      for (std::size_t i = 0; i < std::min(options.elitism_min_sample, indices.size()); ++i)
        if (population[indices[i]].fitness > population[parent1].fitness) parent1 = indices[i];
      parent2 = indices[0] == parent1 ? indices[1] : indices[0];
      for (std::size_t i = 0; i < std::min(options.elitism_min_sample, indices.size()); ++i)
        if (indices[i] != parent1 && population[indices[i]].fitness > population[parent2].fitness)
          parent2 = indices[i];
      Individual child = population[parent1].individual;
      if (std::bernoulli_distribution(options.crossover_rate)(rng))
        child = crossover(std::move(child), population[parent2].individual, rng, causal);
      mutate(child, options.mutation_rate, rng, causal);
      next.push_back({std::move(child), 0.0});
    }
    evaluate(next);
    if (!next.empty() && next.front().fitness > best_fitness) {
      best_fitness = next.front().fitness;
      best = next.front().individual;
    }
    population = std::move(next);
    ++result.diagnostics.generations_completed;
  }

  result.net = to_petri_net(best, log.names);
  result.markings.initial.assign(result.net.place_count(), 0);
  result.markings.final.assign(result.net.place_count(), 0);
  result.markings.initial[result.net.source()] = 1;
  result.markings.final[result.net.sink()] = 1;
  result.diagnostics.best_fitness = best_fitness;
  result.diagnostics.computed = true;
  result.diagnostics.message = result.diagnostics.exhausted
                                  ? "candidate bound reached; best model returned"
                                  : "bounded genetic search completed";
  return result;
}

}  // namespace minera
