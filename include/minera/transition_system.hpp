#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "minera/event_log.hpp"

namespace minera {

// The direction in which a trace window is read.  Enumerators with and
// without the k prefix are provided so callers can use either house style.
enum class TransitionDirection {
  kForward,
  kBackward,
  Forward = kForward,
  Backward = kBackward,
};

// The activity-view abstraction used for each state.
enum class TransitionView {
  kSequence,
  kSet,
  kMultiset,
  Sequence = kSequence,
  Set = kSet,
  Multiset = kMultiset,
};

using Direction = TransitionDirection;
using View = TransitionView;

struct TransitionSystemOptions {
  TransitionDirection direction = TransitionDirection::kForward;
  TransitionView view = TransitionView::kSequence;
  std::size_t window = 2;
  bool include_data = false;
};

// A stable reference to an event traversal in the source event log.
struct TransitionOccurrence {
  std::size_t trace_index = 0;
  std::size_t event_index = 0;

  friend bool operator==(const TransitionOccurrence&, const TransitionOccurrence&) = default;
};

struct TransitionSystemState {
  std::size_t id = 0;
  // Canonical state payload. Sequence views retain order and duplicates;
  // set/multiset views are sorted (set additionally removes duplicates).
  std::vector<std::string> payload;
  std::size_t start_count = 0;
  std::size_t end_count = 0;
  std::vector<TransitionOccurrence> incoming_occurrences;
  std::vector<TransitionOccurrence> outgoing_occurrences;

  [[nodiscard]] const std::vector<std::string>& view() const noexcept {
    return payload;
  }
  [[nodiscard]] const std::vector<std::string>& activities() const noexcept {
    return payload;
  }
};

struct TransitionSystemTransition {
  std::size_t source = 0;
  std::size_t target = 0;
  std::string label;
  std::size_t count = 0;
  std::vector<TransitionOccurrence> occurrences;

  [[nodiscard]] const std::string& activity() const noexcept { return label; }
};

using TransitionState = TransitionSystemState;
using Transition = TransitionSystemTransition;

class TransitionSystem {
 public:
  [[nodiscard]] const std::vector<TransitionSystemState>& states() const noexcept {
    return states_;
  }
  [[nodiscard]] const std::vector<TransitionSystemTransition>& transitions() const noexcept {
    return transitions_;
  }
  [[nodiscard]] const TransitionSystemOptions& options() const noexcept {
    return options_;
  }
  [[nodiscard]] std::size_t state_count() const noexcept { return states_.size(); }
  [[nodiscard]] std::size_t transition_count() const noexcept {
    return transitions_.size();
  }

 private:
  friend TransitionSystem discover_transition_system(
      const EventLog&, const TransitionSystemOptions&);

  TransitionSystemOptions options_;
  std::vector<TransitionSystemState> states_;
  std::vector<TransitionSystemTransition> transitions_;
};

[[nodiscard]] TransitionSystem discover_transition_system(
    const EventLog& event_log,
    const TransitionSystemOptions& options = {});

// Convenience overload matching the positional discovery parameters.
[[nodiscard]] TransitionSystem discover_transition_system(
    const EventLog& event_log,
    TransitionDirection direction,
    std::size_t window = 2,
    TransitionView view = TransitionView::kSequence,
    bool include_data = false);

[[nodiscard]] inline TransitionSystem discover_view_based_transition_system(
    const EventLog& event_log,
    const TransitionSystemOptions& options = {}) {
  return discover_transition_system(event_log, options);
}

[[nodiscard]] inline TransitionSystem discover_view_based_transition_system(
    const EventLog& event_log,
    TransitionDirection direction,
    std::size_t window = 2,
    TransitionView view = TransitionView::kSequence,
    bool include_data = false) {
  return discover_transition_system(event_log, direction, window, view,
                                    include_data);
}

}  // namespace minera
