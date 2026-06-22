#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "minera/heuristic_miner.hpp"
#include "minera/process_tree.hpp"

namespace minera {

// A transition in a labeled workflow net. A transition with no activity is
// silent (tau): it routes tokens but consumes no event during replay.
struct PetriTransition {
  std::optional<std::size_t> activity;  // index into PetriNet::activities()
  std::vector<std::size_t> inputs;      // place ids consumed when firing
  std::vector<std::size_t> outputs;     // place ids produced when firing

  [[nodiscard]] bool silent() const noexcept { return !activity.has_value(); }
};

// A workflow net with exactly one source and one sink place, produced from a
// process tree. The initial marking puts one token on the source; the final
// marking is one token on the sink.
class PetriNet {
 public:
  std::size_t add_place() { return place_count_++; }

  std::size_t add_transition(std::optional<std::size_t> activity,
                             std::vector<std::size_t> inputs,
                             std::vector<std::size_t> outputs) {
    transitions_.push_back(
        PetriTransition{activity, std::move(inputs), std::move(outputs)});
    return transitions_.size() - 1;
  }

  void set_source(std::size_t place) noexcept { source_ = place; }
  void set_sink(std::size_t place) noexcept { sink_ = place; }
  void set_activities(std::vector<std::string> activities) {
    activities_ = std::move(activities);
  }

  [[nodiscard]] std::size_t place_count() const noexcept { return place_count_; }
  [[nodiscard]] std::size_t source() const noexcept { return source_; }
  [[nodiscard]] std::size_t sink() const noexcept { return sink_; }

  [[nodiscard]] const std::vector<PetriTransition>& transitions() const noexcept {
    return transitions_;
  }
  [[nodiscard]] const std::vector<std::string>& activities() const noexcept {
    return activities_;
  }

 private:
  std::size_t place_count_ = 0;
  std::size_t source_ = 0;
  std::size_t sink_ = 0;
  std::vector<PetriTransition> transitions_;
  std::vector<std::string> activities_;
};

// Translates a block-structured process tree into an equivalent workflow net.
// Each operator expands to the textbook fragment: sequence chains its children
// through intermediate places, exclusive choice shares one entry and one exit
// place, parallel adds silent AND-split/join transitions, and loop wires the
// redo children from the body's exit back to its entry.
[[nodiscard]] PetriNet to_petri_net(const ProcessTree& tree);

// Translates a Heuristic Miner dependency graph into a workflow net so it can be
// replayed and aligned like any other model. Each activity becomes a visible
// transition between a "ready" and a "done" place; every accepted dependency
// edge a->b becomes a silent transition from a's done place to b's ready place;
// the source feeds each start activity and each end activity drains to the sink.
// The result is a one-token state machine — always a sound workflow net (a trace
// is one path through the dependency graph), at the cost of not expressing
// concurrency, which the binding-free heuristics net does not capture anyway.
[[nodiscard]] PetriNet to_petri_net(const HeuristicsNet& heuristics);

}  // namespace minera
