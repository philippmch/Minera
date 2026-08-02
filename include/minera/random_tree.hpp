#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "minera/process_tree.hpp"

namespace minera {

// Random process-tree construction strategies.  BASIC is retained as the
// default for source compatibility; PT-and-log uses a bounded population
// construction controlled by the activity-count and operator probabilities.
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
  RandomTreeVariant variant = RandomTreeVariant::kBasic;

  // Parameters of the recursive BASIC generator.
  std::size_t rec_depth = 0;
  std::size_t min_rec_depth = 1;
  std::size_t max_rec_depth = 3;
  double prob_leaf = 0.25;
  std::uint64_t seed = 1;
  std::vector<std::string> activities = {"A", "B", "C", "D", "E", "F"};

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

[[nodiscard]] ProcessTree generate_random_process_tree(
    const RandomTreeOptions& options = {});

[[nodiscard]] inline ProcessTree random_process_tree(
    const RandomTreeOptions& options = {}) {
  return generate_random_process_tree(options);
}

}  // namespace minera
