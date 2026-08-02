#pragma once

#include <cstddef>
#include <optional>
#include <stdexcept>
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

  // Extends an existing transition while assembling composite model views
  // (for example a merged object-centric net). The public read-only view of
  // transitions remains stable for callers that only inspect a net.
  void append_transition_arcs(std::size_t transition,
                              std::vector<std::size_t> inputs,
                              std::vector<std::size_t> outputs) {
    if (transition >= transitions_.size())
      throw std::out_of_range("Petri transition index is invalid");
    auto& target = transitions_[transition];
    target.inputs.insert(target.inputs.end(), inputs.begin(), inputs.end());
    target.outputs.insert(target.outputs.end(), outputs.begin(), outputs.end());
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

// Translates a Heuristic Miner dependency graph into a workflow net so it can
// be replayed and aligned like any other model, honouring the net's split and
// join semantics. Every accepted dependency edge becomes a place; each
// activity is one visible transition that consumes and produces the places of
// its input and output bundles: a bundle of several edges is an AND split or
// join wired straight onto the transition, while several bundles meet at a
// hub place through one silent transition per bundle, keeping the choice
// exclusive. Starting is one more join alternative (from the source place)
// and finishing one more split alternative (to the sink). Unlike the former
// one-token translation this expresses the concurrency the AND measure
// discovered, so parallel behaviour replays instead of counting as deviation;
// in exchange, soundness is no longer guaranteed on graphs whose splits and
// joins do not balance (alignments report such nets as not computable).
[[nodiscard]] PetriNet to_petri_net(const HeuristicsNet& heuristics);

}  // namespace minera
