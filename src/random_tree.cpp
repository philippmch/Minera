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

  std::mt19937_64 generator(options.seed);
  return ProcessTree(options.activities,
                     generate_node(generator, options, options.rec_depth));
}

}  // namespace minera
