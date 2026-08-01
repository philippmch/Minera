#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "minera/process_tree.hpp"

namespace minera {

struct RandomTreeOptions {
  std::size_t rec_depth = 0;
  std::size_t min_rec_depth = 1;
  std::size_t max_rec_depth = 3;
  double prob_leaf = 0.25;
  std::uint64_t seed = 1;
  std::vector<std::string> activities = {"A", "B", "C", "D", "E", "F"};
};

[[nodiscard]] ProcessTree generate_random_process_tree(
    const RandomTreeOptions& options = {});

[[nodiscard]] inline ProcessTree random_process_tree(
    const RandomTreeOptions& options = {}) {
  return generate_random_process_tree(options);
}

}  // namespace minera
