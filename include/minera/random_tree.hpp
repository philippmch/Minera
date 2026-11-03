#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "minera/process_tree.hpp"

namespace minera {

// algo.simulation.tree_generator of PM4Py 2.7.22. Both variants are exact:
// the draws are the reference's global generators, random.seed(seed) (and
// numpy.random.seed(seed) for PT-and-log), in the reference order, so a seed
// reproduces the reference trees and their labels whatever the string-hash
// seed (see random_tree.cpp for the draw order). BASIC labels its leaves
// with six random characters from A-Z0-9; PT-and-log names its activities
// a, b, ..., z, aa, ab, ... in creation order. PT-and-log is the default.
enum class RandomTreeVariant {
  kBasic,
  kPtAndLogGenerator,
  kPtAndLog = kPtAndLogGenerator,
  BASIC = kBasic,
  PTANDLOG = kPtAndLogGenerator,
  PT_AND_LOG = kPtAndLogGenerator,
  PT_AND_LOG_GENERATOR = kPtAndLogGenerator,
  PTANDLOGGENERATOR = kPtAndLogGenerator,
};

struct RandomTreeOptions {
  RandomTreeVariant variant = RandomTreeVariant::kPtAndLogGenerator;

  // Parameters of the recursive BASIC generator.
  std::size_t rec_depth = 0;
  std::size_t min_rec_depth = 1;
  std::size_t max_rec_depth = 3;
  double prob_leaf = 0.25;
  std::uint64_t seed = 1;
  // Empty (the default): the reference's random six-character labels.
  // Declared extension: BASIC draws each leaf label from these names with
  // one randbelow(n) instead. PT-and-log ignores them.
  std::vector<std::string> activities;

  // Parameters of the PT-and-log population generator.  The activity count
  // is sampled from a triangular distribution (min, mode, max).
  std::size_t mode = 20;
  std::size_t min = 10;
  std::size_t max = 30;
  double sequence = 0.25;
  double choice = 0.25;
  double parallel = 0.25;
  double loop = 0.25;
  // C++ reserves `or` as an alternative token, so this is the spelling of
  // the OR operator probability in the native API.
  double or_probability = 0.0;
  double silent = 0.25;
  double duplicate = 0.0;
  std::size_t no_models = 1;
};

// The plural generator is intentionally bounded so a request cannot reserve
// an unbounded number of process trees before generation starts.
inline constexpr std::size_t kMaxRandomTreeModels = 1024;
inline constexpr std::size_t kMaxRandomTreeActivityBudget = 250'000;

[[nodiscard]] std::vector<ProcessTree> generate_random_process_trees(
    const RandomTreeOptions& options = {});

// The singular API remains available for callers that request exactly one
// PT-and-log model. BASIC ignores no_models and continues to return one tree.
[[nodiscard]] ProcessTree generate_random_process_tree(
    const RandomTreeOptions& options = {});

[[nodiscard]] inline ProcessTree random_process_tree(
    const RandomTreeOptions& options = {}) {
  return generate_random_process_tree(options);
}

}  // namespace minera
