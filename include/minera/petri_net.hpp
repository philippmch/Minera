#pragma once

#include <algorithm>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "minera/heuristic_miner.hpp"
#include "minera/process_tree.hpp"

namespace minera {

// Ordinary arcs consume/produce their multiplicity. Reset arcs remove every
// token from their place when the transition fires, while inhibitor arcs only
// allow firing when their place is empty.
enum class PetriArcType { kNormal, kReset, kInhibitor };

// A transition in a labeled workflow net. A transition with no activity is
// silent (tau): it routes tokens but consumes no event during replay.
struct PetriTransition {
  std::optional<std::size_t> activity;  // index into PetriNet::activities()
  std::vector<std::size_t> inputs;      // place ids consumed when firing
  std::vector<std::size_t> outputs;     // place ids produced when firing
  std::vector<std::size_t> reset_inputs;      // reset arcs (place -> transition)
  std::vector<std::size_t> inhibitor_inputs;  // inhibitor arcs

  [[nodiscard]] bool silent() const noexcept { return !activity.has_value(); }
  [[nodiscard]] bool has_extended_arcs() const noexcept {
    return !reset_inputs.empty() || !inhibitor_inputs.empty();
  }
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
        PetriTransition{activity, std::move(inputs), std::move(outputs), {}, {}});
    return transitions_.size() - 1;
  }

  void add_arc(std::size_t transition, std::size_t place,
               PetriArcType type = PetriArcType::kNormal) {
    if (transition >= transitions_.size())
      throw std::out_of_range("Petri transition index is invalid");
    if (place >= place_count_)
      throw std::out_of_range("Petri place index is invalid");
    auto& target = transitions_[transition];
    auto contains = [&](const std::vector<std::size_t>& values) {
      return std::find(values.begin(), values.end(), place) != values.end();
    };
    if (type == PetriArcType::kNormal) {
      if (contains(target.reset_inputs) || contains(target.inhibitor_inputs))
        throw std::invalid_argument("Petri transition cannot mix arc types for a place");
      target.inputs.push_back(place);
      return;
    }
    if (contains(target.inputs) || contains(target.reset_inputs) ||
        contains(target.inhibitor_inputs)) {
      throw std::invalid_argument("Petri transition cannot duplicate arc types for a place");
    }
    if (type == PetriArcType::kReset) target.reset_inputs.push_back(place);
    else target.inhibitor_inputs.push_back(place);
  }

  void add_reset_arc(std::size_t transition, std::size_t place) {
    add_arc(transition, place, PetriArcType::kReset);
  }

  void add_inhibitor_arc(std::size_t transition, std::size_t place) {
    add_arc(transition, place, PetriArcType::kInhibitor);
  }

  [[nodiscard]] bool has_extended_arcs() const noexcept {
    return std::any_of(transitions_.begin(), transitions_.end(),
                       [](const PetriTransition& transition) {
                         return transition.has_extended_arcs();
                       });
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

// Shared marking semantics for search and simulation code. The template keeps
// the helper usable with signed replay markings and unsigned simulation ones.
template <typename Token>
[[nodiscard]] bool petri_transition_enabled(
    const PetriTransition& transition, const std::vector<Token>& marking) {
  std::vector<std::size_t> required(marking.size(), 0);
  for (const auto place : transition.inputs) {
    if (place >= marking.size()) return false;
    if constexpr (std::is_signed_v<Token>) {
      if (marking[place] < 0) return false;
    }
    ++required[place];
    if (required[place] > static_cast<std::make_unsigned_t<Token>>(marking[place]))
      return false;
  }
  for (const auto place : transition.inhibitor_inputs) {
    if (place >= marking.size() || marking[place] != 0) return false;
  }
  return true;
}

template <typename Token>
void fire_petri_transition(const PetriTransition& transition,
                           std::vector<Token>& marking) {
  for (const auto place : transition.inputs) --marking[place];
  for (const auto place : transition.reset_inputs) marking[place] = 0;
  for (const auto place : transition.outputs) ++marking[place];
}

// Translates a block-structured process tree into an equivalent workflow net.
// Each operator expands to the textbook fragment: sequence chains its children
// through intermediate places, exclusive choice shares one entry and one exit
// place, inclusive choice gives each child an execute/skip selector and joins
// after at least one child was selected, parallel adds silent AND-split/join
// transitions, and loop wires the redo children from the body's exit back to
// its entry.
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
