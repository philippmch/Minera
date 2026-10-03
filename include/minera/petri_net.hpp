#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
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

// Retained arc weight for interchange and classic graph evaluation.
// Reset/inhibitor execution still follows the arc type, independent of weight.
struct PetriArcWeight {
  std::uint64_t magnitude = 1;
  bool negative = false;
};

// A transition in a labeled workflow net. A transition with no activity is
// silent (tau): it routes tokens but consumes no event during replay.
struct PetriTransition {
  std::optional<std::size_t> activity;  // index into PetriNet::activities()
  std::vector<std::size_t> inputs;      // place ids consumed when firing
  std::vector<std::size_t> outputs;     // place ids produced when firing
  std::vector<std::size_t> reset_inputs;      // reset arcs (place -> transition)
  std::vector<std::size_t> inhibitor_inputs;  // inhibitor arcs
  std::vector<PetriArcWeight> reset_weights;
  std::vector<PetriArcWeight> inhibitor_weights;
  // Empty uses the historical repeated-unit endpoints. Otherwise one weight
  // accompanies each ordinary arc occurrence, including zero and signed arcs.
  std::vector<PetriArcWeight> input_weights;
  std::vector<PetriArcWeight> output_weights;

  [[nodiscard]] PetriArcWeight input_arc_weight(std::size_t index) const {
    if (input_weights.empty()) return {};
    if (input_weights.size() != inputs.size())
      throw std::invalid_argument("Petri input weight dimensions are invalid");
    return input_weights.at(index);
  }
  [[nodiscard]] PetriArcWeight output_arc_weight(std::size_t index) const {
    if (output_weights.empty()) return {};
    if (output_weights.size() != outputs.size())
      throw std::invalid_argument("Petri output weight dimensions are invalid");
    return output_weights.at(index);
  }
  [[nodiscard]] bool has_compact_ordinary_arcs() const noexcept {
    return !input_weights.empty() || !output_weights.empty();
  }

  [[nodiscard]] PetriArcWeight reset_arc_weight(std::size_t index) const {
    if (reset_weights.empty()) return {};
    if (reset_weights.size() != reset_inputs.size())
      throw std::invalid_argument("Petri reset weight dimensions are invalid");
    return reset_weights.at(index);
  }
  [[nodiscard]] PetriArcWeight inhibitor_arc_weight(std::size_t index) const {
    if (inhibitor_weights.empty()) return {};
    if (inhibitor_weights.size() != inhibitor_inputs.size())
      throw std::invalid_argument("Petri inhibitor weight dimensions are invalid");
    return inhibitor_weights.at(index);
  }

  [[nodiscard]] bool silent() const noexcept { return !activity.has_value(); }
  [[nodiscard]] bool has_extended_arcs() const noexcept {
    return !reset_inputs.empty() || !inhibitor_inputs.empty();
  }
};

// Opt-in presentation metadata, independent of the executable workflow net.
// IDs are explicit and local to each node kind; names need not be unique.
// Ordered arcs and sparse markings retain occurrences and explicit zero entries.
// Properties contain only recursive JSON values with string keys. Strings are
// byte strings: callers must supply UTF-8 for interoperable JSON text.
struct PetriPresentedPlace {
  std::size_t id;
  std::string name;
  HeuristicStructuredValue::Object properties;
};
struct PetriPresentedTransition {
  std::size_t id;
  std::string name;
  std::optional<std::string> label;
  HeuristicStructuredValue::Object properties;
};
enum class PetriNodeKind { kPlace, kTransition };
struct PetriNodeRef {
  PetriNodeKind kind;
  std::size_t id;
};
struct PetriPresentedArc {
  PetriNodeRef source;
  PetriNodeRef target;
  std::uint64_t weight = 1;
  HeuristicStructuredValue::Object properties;
  // weight is the absolute magnitude; negative zero is equivalent to zero.
  bool negative_weight = false;
};
struct PetriSparseMarkingEntry {
  std::size_t place_id;
  std::int64_t tokens;
};
struct PetriNetPresentation {
  std::string name;
  HeuristicStructuredValue::Object properties;
  std::vector<PetriPresentedPlace> places;
  std::vector<PetriPresentedTransition> transitions;
  std::vector<PetriPresentedArc> arcs;
  std::vector<PetriSparseMarkingEntry> initial_marking;
  std::vector<PetriSparseMarkingEntry> final_marking;
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
        PetriTransition{activity, std::move(inputs), std::move(outputs), {}, {}, {}, {}, {}, {}});
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
      if (!target.input_weights.empty()) target.input_weights.push_back({});
      return;
    }
    if (contains(target.inputs) || contains(target.reset_inputs) ||
        contains(target.inhibitor_inputs)) {
      throw std::invalid_argument("Petri transition cannot duplicate arc types for a place");
    }
    auto& endpoints = type == PetriArcType::kReset ? target.reset_inputs : target.inhibitor_inputs;
    auto& weights = type == PetriArcType::kReset ? target.reset_weights : target.inhibitor_weights;
    if (weights.empty()) weights.resize(endpoints.size());
    if (weights.size() != endpoints.size())
      throw std::invalid_argument("Petri extended weight dimensions are invalid");
    endpoints.push_back(place);
    weights.push_back({});
  }

  // Adds the output side of an ordinary transition-place arc.  Repeated
  // output places are retained as arc multiplicity, just like repeated input
  // places passed to add_transition/add_arc.
  void add_output_arc(std::size_t transition, std::size_t place) {
    if (transition >= transitions_.size())
      throw std::out_of_range("Petri transition index is invalid");
    if (place >= place_count_)
      throw std::out_of_range("Petri place index is invalid");
    transitions_[transition].outputs.push_back(place);
    if (!transitions_[transition].output_weights.empty())
      transitions_[transition].output_weights.push_back({});
  }

  // Explicit ordinary weights avoid multiplicity-sized allocations. Supplying
  // no weights on a side preserves the historical repeated-unit convention.
  void set_ordinary_arc_weights(std::size_t transition,
                               std::vector<PetriArcWeight> inputs,
                               std::vector<PetriArcWeight> outputs) {
    auto& target = transitions_.at(transition);
    if ((!inputs.empty() && inputs.size() != target.inputs.size()) ||
        (!outputs.empty() && outputs.size() != target.outputs.size()))
      throw std::invalid_argument("Petri ordinary weight dimensions are invalid");
    for (auto& weight : inputs) weight.negative = weight.negative && weight.magnitude;
    for (auto& weight : outputs) weight.negative = weight.negative && weight.magnitude;
    target.input_weights = std::move(inputs);
    target.output_weights = std::move(outputs);
  }

  void add_reset_arc(std::size_t transition, std::size_t place,
                     std::uint64_t weight = 1, bool negative = false) {
    add_arc(transition, place, PetriArcType::kReset);
    transitions_[transition].reset_weights.back() = {weight, negative && weight != 0};
  }

  void add_inhibitor_arc(std::size_t transition, std::size_t place,
                         std::uint64_t weight = 1, bool negative = false) {
    add_arc(transition, place, PetriArcType::kInhibitor);
    transitions_[transition].inhibitor_weights.back() = {weight, negative && weight != 0};
  }

  [[nodiscard]] bool has_extended_arcs() const noexcept {
    return std::any_of(transitions_.begin(), transitions_.end(),
                       [](const PetriTransition& transition) {
                         return transition.has_extended_arcs();
                       });
  }

  [[nodiscard]] bool has_compact_ordinary_arcs() const noexcept {
    return std::any_of(transitions_.begin(), transitions_.end(),
        [](const PetriTransition& transition) { return transition.has_compact_ordinary_arcs(); });
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
    if (!target.input_weights.empty())
      target.input_weights.insert(target.input_weights.end(), inputs.size(), PetriArcWeight{});
    if (!target.output_weights.empty())
      target.output_weights.insert(target.output_weights.end(), outputs.size(), PetriArcWeight{});
    target.inputs.insert(target.inputs.end(), inputs.begin(), inputs.end());
    target.outputs.insert(target.outputs.end(), outputs.begin(), outputs.end());
  }

  void set_source(std::size_t place) noexcept {
    source_ = place;
    source_set_ = true;
  }
  void set_sink(std::size_t place) noexcept {
    sink_ = place;
    sink_set_ = true;
  }
  void set_activities(std::vector<std::string> activities) {
    activities_ = std::move(activities);
  }

  [[nodiscard]] std::size_t place_count() const noexcept { return place_count_; }
  [[nodiscard]] std::size_t source() const noexcept { return source_; }
  [[nodiscard]] std::size_t sink() const noexcept { return sink_; }
  [[nodiscard]] bool has_source() const noexcept { return source_set_; }
  [[nodiscard]] bool has_sink() const noexcept { return sink_set_; }

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
  bool source_set_ = false;
  bool sink_set_ = false;
  std::vector<PetriTransition> transitions_;
  std::vector<std::string> activities_;
};

// Process-tree Petri-net conversion variants.  The standard variant keeps
// the historical place-bordered construction; the transition-bordered
// variant keeps an explicit silent entry and exit transition for every
// composite fragment.
enum class ProcessTreePetriNetVariant {
  kStandard,
  kTransitionBordered,

  STANDARD = kStandard,
  TRANSITION_BORDERED = kTransitionBordered,
};

using ProcessTreePetriNetConversionVariant = ProcessTreePetriNetVariant;
using ProcessTreeToPetriNetVariant = ProcessTreePetriNetVariant;

// Shared marking semantics for search and simulation code. The template keeps
// the helper usable with signed replay markings and unsigned simulation ones.
template <typename Token>
[[nodiscard]] bool petri_transition_enabled(
    const PetriTransition& transition, const std::vector<Token>& marking) {
  std::vector<std::size_t> required(marking.size(), 0);
  for (std::size_t index = 0; index < transition.inputs.size(); ++index) {
    const auto place = transition.inputs[index];
    if (place >= marking.size()) return false;
    if (!transition.input_weights.empty()) {
      const auto weight = transition.input_arc_weight(index);
      if (weight.negative) {
        if constexpr (std::is_signed_v<Token>) {
          constexpr auto limit = std::uint64_t{1} << std::numeric_limits<Token>::digits;
          if (weight.magnitude < limit && marking[place] < -static_cast<Token>(weight.magnitude)) return false;
        }
      } else {
        if constexpr (std::is_signed_v<Token>) if (marking[place] < 0) return false;
        if (static_cast<std::uint64_t>(marking[place]) < weight.magnitude) return false;
      }
      continue;
    }
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

// Checked signed-magnitude arithmetic works for signed and unsigned markings
// without converting a uint64 weight to a signed token first.
template <typename Token>
[[nodiscard]] bool petri_adjust_tokens(Token value, std::uint64_t magnitude,
                                       bool negative, Token& result) {
  static_assert(std::is_integral_v<Token> && !std::is_same_v<Token, bool> && sizeof(Token) <= 8);
  if constexpr (!std::is_signed_v<Token>) {
    if (negative) {
      if (magnitude > value) return false;
      result = value - static_cast<Token>(magnitude);
    } else {
      if (magnitude > std::numeric_limits<Token>::max() - value) return false;
      result = value + static_cast<Token>(magnitude);
    }
  } else {
    constexpr auto limit = std::uint64_t{1} << std::numeric_limits<Token>::digits;
    const auto absolute = [](Token number) { return number < 0 ? std::uint64_t(-(number + 1)) + 1 : std::uint64_t(number); };
    if (!negative) {
      if (value >= 0) {
        if (magnitude > std::uint64_t(std::numeric_limits<Token>::max() - value)) return false;
        result = value + static_cast<Token>(magnitude);
      } else if (magnitude >= absolute(value)) {
        const auto remaining = magnitude - absolute(value);
        if (remaining >= limit) return false;
        result = static_cast<Token>(remaining);
      } else {
        const auto remaining = absolute(value) - magnitude;
        result = remaining == limit ? std::numeric_limits<Token>::min() : -static_cast<Token>(remaining);
      }
    } else {
      if (value <= 0) {
        if (magnitude > limit - absolute(value)) return false;
        const auto remaining = absolute(value) + magnitude;
        result = remaining == limit ? std::numeric_limits<Token>::min() : -static_cast<Token>(remaining);
      } else if (magnitude <= std::uint64_t(value)) {
        result = value - static_cast<Token>(magnitude);
      } else {
        const auto remaining = magnitude - std::uint64_t(value);
        if (remaining > limit) return false;
        result = remaining == limit ? std::numeric_limits<Token>::min() : -static_cast<Token>(remaining);
      }
    }
  }
  return true;
}

template <typename Token>
void fire_petri_transition(const PetriTransition& transition,
                           std::vector<Token>& marking) {
  for (std::size_t index = 0; index < transition.inputs.size(); ++index) {
    const auto place = transition.inputs[index];
    if (transition.input_weights.empty()) { --marking.at(place); continue; }
    const auto weight = transition.input_arc_weight(index);
    auto& value = marking.at(place);
    if (!weight.negative && (value <= 0 || weight.magnitude >= static_cast<std::uint64_t>(value))) {
      value = 0; continue;
    }
    Token following;
    if (!petri_adjust_tokens(value, weight.magnitude, !weight.negative, following))
      throw std::overflow_error("Petri firing exceeds supported token range");
    value = following <= 0 ? 0 : following;
  }
  for (const auto place : transition.reset_inputs) marking[place] = 0;
  for (std::size_t index = 0; index < transition.outputs.size(); ++index) {
    const auto place = transition.outputs[index];
    if (!transition.output_weights.empty()) {
      const auto weight = transition.output_arc_weight(index);
      Token following;
      if (!petri_adjust_tokens(marking.at(place),weight.magnitude,weight.negative,following))
        throw std::overflow_error("Petri firing exceeds supported token range");
      marking[place] = following; continue;
    }
    if (marking[place] == std::numeric_limits<Token>::max())
      throw std::overflow_error("Petri firing exceeds supported token range");
    ++marking[place];
  }
}

// Translates a block-structured process tree into an equivalent workflow net.
// Each operator expands to the textbook fragment: sequence chains its children
// through intermediate places, exclusive choice shares one entry and one exit
// place, inclusive choice gives each child an execute/skip selector and joins
// after at least one child was selected, parallel adds silent AND-split/join
// transitions, and loop wires the redo children from the body's exit back to
// its entry.
[[nodiscard]] PetriNet to_petri_net(const ProcessTree& tree);

// Converts a process tree with the requested construction.  The one-argument
// overload above remains the standard conversion for source compatibility.
[[nodiscard]] PetriNet to_petri_net(
    const ProcessTree& tree, ProcessTreePetriNetVariant variant);

// The transition-bordered construction is useful when each recursive
// composite fragment must expose silent source and sink transitions.  Activity
// and tau leaves remain the fragment's transition boundaries.
[[nodiscard]] PetriNet to_petri_net_transition_bordered(
    const ProcessTree& tree);

// Stable wire names used by export adapters.  The parser intentionally
// accepts only the two supported spellings and rejects unknown values.
[[nodiscard]] const char* process_tree_petri_net_variant_name(
    ProcessTreePetriNetVariant variant) noexcept;
[[nodiscard]] ProcessTreePetriNetVariant parse_process_tree_petri_net_variant(
    std::string_view value);

// Serializes a converted process-tree net without changing the established
// analysis envelope.  The variant is metadata for the selected construction.
[[nodiscard]] std::string serialize_process_tree_petri_net_json(
    const PetriNet& net, ProcessTreePetriNetVariant variant);

// Translates a Heuristic Miner dependency graph into a workflow net so it can
// be replayed and aligned like any other model, honouring the net's split and
// join semantics. Every accepted dependency edge becomes a place; each
// activity is one visible transition that consumes and produces the places of
// its input and output bundles: a bundle of several edges is an AND split or
// join wired straight onto the transition, while several bundles meet at a
// hub place through one silent transition per bundle, keeping the choice
// exclusive. Starting is one more join alternative (from the source place)
// and finishing one more split alternative (to the sink). An activity with no
// incoming dependency or start boundary has no input arcs, while an activity
// with no outgoing dependency or end boundary produces a token into a private
// parking place. Unlike the former one-token translation this expresses the
// concurrency the AND measure discovered, so parallel behaviour replays
// instead of counting as deviation; in exchange, soundness is no longer
// guaranteed on graphs whose splits and joins do not balance (alignments
// report such nets as not computable).
[[nodiscard]] PetriNet to_petri_net(const HeuristicsNet& heuristics);

// Opt-in PLUSPLUS names/labels and explicit unit arc occurrences. Generated
// metadata is projected through conservative reduction; names may collide.
[[nodiscard]] PetriNetPresentation to_petri_net_presentation(
    const HeuristicsNet& heuristics);

}  // namespace minera
