#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>

#include "minera/event_log.hpp"
#include "minera/process_tree.hpp"

namespace minera {

// Strategies for generating a log from a block-structured process tree.
enum class ProcessTreePlayoutVariant {
  kBasicPlayout,
  kExtensive,
  kTopBottom,
  kBasic = kBasicPlayout,
  kTopbottom = kTopBottom,
  BASIC_PLAYOUT = kBasicPlayout,
  EXTENSIVE = kExtensive,
  TOPBOTTOM = kTopBottom,
  TOP_BOTTOM = kTopBottom,
};

struct ProcessTreePlayoutOptions {
  // The omitted/default variant is top-bottom; basic and extensive remain
  // explicit selections.
  ProcessTreePlayoutVariant variant = ProcessTreePlayoutVariant::kTopBottom;
  std::size_t traces = 1000;                // basic/top-bottom cases
  std::size_t max_steps = 1000;             // visible steps per sampled case
  std::uint64_t seed = 1;
  std::size_t max_length = 1000;             // extensive visible length
  std::size_t min_length = 1;                // extensive lower bound
  std::size_t max_traces = 100000;           // extensive case cap
  std::size_t max_loop_occurrences = 3;       // maximum loop re-do iterations
  double start = 1767225600.0;
  double case_interval = 60.0;
  double event_interval = 1.0;
};

[[nodiscard]] EventLog process_tree_playout(
    const ProcessTree& tree, ProcessTreePlayoutOptions options = {});

// Convenience aliases.  The ProcessTree overload is distinct from the
// Petri-net playout API in minera/playout.hpp.
[[nodiscard]] inline EventLog playout(
    const ProcessTree& tree, ProcessTreePlayoutOptions options = {}) {
  return process_tree_playout(tree, std::move(options));
}

[[nodiscard]] inline EventLog extensive_process_tree_playout(
    const ProcessTree& tree, ProcessTreePlayoutOptions options = {}) {
  options.variant = ProcessTreePlayoutVariant::kExtensive;
  return process_tree_playout(tree, std::move(options));
}

[[nodiscard]] inline EventLog basic_process_tree_playout(
    const ProcessTree& tree, ProcessTreePlayoutOptions options = {}) {
  options.variant = ProcessTreePlayoutVariant::kBasicPlayout;
  return process_tree_playout(tree, std::move(options));
}

[[nodiscard]] inline EventLog topbottom_process_tree_playout(
    const ProcessTree& tree, ProcessTreePlayoutOptions options = {}) {
  options.variant = ProcessTreePlayoutVariant::kTopBottom;
  return process_tree_playout(tree, std::move(options));
}

}  // namespace minera
