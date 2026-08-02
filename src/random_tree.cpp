#include "minera/random_tree.hpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <set>
#include <stdexcept>

namespace minera {
namespace {

std::size_t random_index(std::mt19937_64& generator, std::size_t size) {
  std::uniform_int_distribution<std::size_t> distribution(0, size - 1);
  return distribution(generator);
}

bool random_event(std::mt19937_64& generator, double probability) {
  return std::bernoulli_distribution(probability)(generator);
}

ProcessTreeNode generate_node(std::mt19937_64& generator,
                              const RandomTreeOptions& options,
                              std::size_t depth) {
  if (depth == 0) {
    ProcessTreeNode root{ProcessTreeOperator::kSequence, 0, {}};
    root.children.push_back(
        ProcessTreeNode::leaf(random_index(generator, options.activities.size())));
    root.children.push_back(generate_node(generator, options, depth + 1));
    root.children.push_back(
        ProcessTreeNode::leaf(random_index(generator, options.activities.size())));
    return root;
  }

  bool leaf = depth > options.max_rec_depth;
  if (!leaf && depth >= options.min_rec_depth && depth <= options.max_rec_depth) {
    std::bernoulli_distribution choose_leaf(options.prob_leaf);
    leaf = choose_leaf(generator);
  }
  if (leaf) {
    return ProcessTreeNode::leaf(
        random_index(generator, options.activities.size()));
  }

  const int operator_index =
      std::uniform_int_distribution<int>(0, 3)(generator);
  const auto append_children = [&](ProcessTreeOperator op,
                                   std::size_t count) {
    ProcessTreeNode result{op, 0, {}};
    result.children.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
      result.children.push_back(generate_node(generator, options, depth + 1));
    return result;
  };
  switch (operator_index) {
    case 0:
      return append_children(
          ProcessTreeOperator::kSequence,
          std::uniform_int_distribution<std::size_t>(2, 5)(generator));
    case 1: {
      ProcessTreeNode result{ProcessTreeOperator::kLoop, 0, {}};
      result.children.push_back(generate_node(generator, options, depth + 1));
      result.children.push_back(generate_node(generator, options, depth + 1));
      result.children.push_back(ProcessTreeNode::tau());
      return result;
    }
    case 2:
      return append_children(
          ProcessTreeOperator::kExclusive,
          std::uniform_int_distribution<std::size_t>(2, 4)(generator));
    default:
      return append_children(
          ProcessTreeOperator::kParallel,
          std::uniform_int_distribution<std::size_t>(2, 3)(generator));
  }
}

std::size_t draw_activity_count(std::mt19937_64& generator,
                                const RandomTreeOptions& options) {
  if (options.min == options.max) return options.min;
  const double span = static_cast<double>(options.max - options.min);
  const double left_span = static_cast<double>(options.mode - options.min);
  const double right_span = static_cast<double>(options.max - options.mode);
  const double split = left_span / span;
  const double sample = std::uniform_real_distribution<double>(0.0, 1.0)(
      generator);
  const double continuous =
      sample < split
          ? static_cast<double>(options.min) +
                std::sqrt(sample * span * left_span)
          : static_cast<double>(options.max) -
                std::sqrt((1.0 - sample) * span * right_span);
  const auto rounded = static_cast<long long>(std::llround(continuous));
  return static_cast<std::size_t>(std::clamp(
      rounded, static_cast<long long>(options.min),
      static_cast<long long>(options.max)));
}

ProcessTreeNode generate_ptandlog_tree(std::mt19937_64& generator,
                                       const RandomTreeOptions& options) {
  std::vector<ProcessTreeNode> population;
  const auto activity_count = draw_activity_count(generator, options);
  population.reserve(activity_count);

  std::vector<std::size_t> assigned;
  assigned.reserve(activity_count);
  for (std::size_t i = 0; i < activity_count; ++i) {
    std::size_t activity = i % options.activities.size();
    if (!assigned.empty() && random_event(generator, options.duplicate))
      activity = assigned[random_index(generator, assigned.size())];
    assigned.push_back(activity);
    population.push_back(ProcessTreeNode::leaf(activity));
  }

  std::vector<double> operator_weights = {
      options.sequence, options.choice, options.parallel, options.loop,
      options.or_probability};
  while (population.size() > 1) {
    const auto first = random_index(generator, population.size());
    std::size_t second = random_index(generator, population.size() - 1);
    if (second >= first) ++second;
    const auto op_index = std::discrete_distribution<std::size_t>(
        operator_weights.begin(), operator_weights.end())(generator);

    ProcessTreeNode combined;
    switch (op_index) {
      case 0:
        combined.op = ProcessTreeOperator::kSequence;
        combined.children = {std::move(population[first]),
                             std::move(population[second])};
        break;
      case 1:
        combined.op = ProcessTreeOperator::kExclusive;
        combined.children = {std::move(population[first]),
                             std::move(population[second])};
        if (random_event(generator, options.silent))
          combined.children.push_back(ProcessTreeNode::tau());
        break;
      case 2:
        combined.op = ProcessTreeOperator::kParallel;
        combined.children = {std::move(population[first]),
                             std::move(population[second])};
        break;
      case 3:
        combined.op = ProcessTreeOperator::kLoop;
        combined.children.push_back(std::move(population[first]));
        combined.children.push_back(std::move(population[second]));
        if (random_event(generator, options.silent))
          combined.children.push_back(ProcessTreeNode::tau());
        break;
      default:  // OR is represented by the process-tree XOR operator.
        combined.op = ProcessTreeOperator::kExclusive;
        combined.children = {std::move(population[first]),
                             std::move(population[second])};
        break;
    }

    const auto high = std::max(first, second);
    const auto low = std::min(first, second);
    population.erase(population.begin() + high);
    population.erase(population.begin() + low);
    population.push_back(std::move(combined));
  }
  return std::move(population.front());
}

}  // namespace

ProcessTree generate_random_process_tree(const RandomTreeOptions& options) {
  if (options.min_rec_depth > options.max_rec_depth)
    throw std::invalid_argument(
        "random tree: min_rec_depth must not exceed max_rec_depth");
  if (options.max_rec_depth > 512)
    throw std::invalid_argument("random tree: max_rec_depth is too large");
  if (!std::isfinite(options.prob_leaf) || options.prob_leaf < 0.0 ||
      options.prob_leaf > 1.0)
    throw std::invalid_argument(
        "random tree: prob_leaf must be finite and in [0,1]");
  if (options.activities.empty())
    throw std::invalid_argument("random tree: activities must not be empty");
  std::set<std::string> unique(options.activities.begin(), options.activities.end());
  if (unique.size() != options.activities.size() ||
      unique.contains(std::string{}))
    throw std::invalid_argument(
        "random tree: activities must be non-empty and unique");

  if (options.variant == RandomTreeVariant::kPtAndLogGenerator) {
    if (options.min == 0 || options.min > options.max ||
        options.mode < options.min || options.mode > options.max)
      throw std::invalid_argument(
          "random tree: PT-and-log min, mode, and max must satisfy 1 <= min <= mode <= max");
    if (options.max > 10000)
      throw std::invalid_argument("random tree: PT-and-log max is too large");
    const auto check_probability = [](double value, const char* name) {
      if (!std::isfinite(value) || value < 0.0 || value > 1.0)
        throw std::invalid_argument(std::string("random tree: ") + name +
                                    " must be finite and in [0,1]");
    };
    check_probability(options.sequence, "sequence");
    check_probability(options.choice, "choice");
    check_probability(options.parallel, "parallel");
    check_probability(options.loop, "loop");
    check_probability(options.or_probability, "or");
    check_probability(options.silent, "silent");
    check_probability(options.duplicate, "duplicate");
    if (options.no_models == 0)
      throw std::invalid_argument("random tree: no_models must be positive");
    if (options.no_models > 1)
      throw std::invalid_argument(
          "random tree: native PT-and-log generation returns one tree");
    if (options.or_probability > 0.0)
      throw std::invalid_argument(
          "random tree: inclusive OR is not supported by the native process-tree model");
    if (options.sequence + options.choice + options.parallel + options.loop +
            options.or_probability <=
        0.0)
      throw std::invalid_argument(
          "random tree: at least one PT-and-log operator probability must be positive");
  }

  std::mt19937_64 generator(options.seed);
  if (options.variant == RandomTreeVariant::kPtAndLogGenerator)
    return ProcessTree(options.activities,
                       generate_ptandlog_tree(generator, options));
  return ProcessTree(options.activities,
                     generate_node(generator, options, options.rec_depth));
}

}  // namespace minera
