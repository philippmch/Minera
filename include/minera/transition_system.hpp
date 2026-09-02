#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "minera/event_data.hpp"
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
  // Selectors are used by flat event-data adapters. Canonical EventLog case
  // selectors retain enclosing trace boundaries; an explicit custom case
  // selector may regroup legacy mixed-case traces. The *_attribute aliases
  // are accepted for callers that use the table-oriented vocabulary.
  std::string activity_key = "concept:name";
  std::string case_id_key = "case:concept:name";
  std::string activity_attribute;
  std::string case_id_attribute;
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

  friend bool operator==(const TransitionSystemState&,
                         const TransitionSystemState&) = default;
};

struct TransitionSystemTransition {
  std::size_t source = 0;
  std::size_t target = 0;
  std::string label;
  std::size_t count = 0;
  std::vector<TransitionOccurrence> occurrences;

  [[nodiscard]] const std::string& activity() const noexcept { return label; }

  friend bool operator==(const TransitionSystemTransition&,
                         const TransitionSystemTransition&) = default;
};

using TransitionState = TransitionSystemState;
using Transition = TransitionSystemTransition;

class TransitionSystem {
 public:
  TransitionSystem() = default;
  TransitionSystem(TransitionSystemOptions options,
                   std::vector<TransitionSystemState> states,
                   std::vector<TransitionSystemTransition> transitions)
      : options_(std::move(options)),
        states_(std::move(states)),
        transitions_(std::move(transitions)) {}

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
  friend TransitionSystem discover_transition_system(
      const EventStream&, const TransitionSystemOptions&);
  friend TransitionSystem discover_transition_system(
      const EventTable&, const TransitionSystemOptions&);
  friend TransitionSystem discover_transition_system(
      const LazyEventTable&, const TransitionSystemOptions&);
  friend TransitionSystem discover_transition_system(
      const EventDataView&, const TransitionSystemOptions&);

  TransitionSystemOptions options_;
  std::vector<TransitionSystemState> states_;
  std::vector<TransitionSystemTransition> transitions_;
};

// The dispatcher has one currently supported transition-system algorithm.
// Keeping the selector explicit gives callers the same callable surface for
// direct, stream, eager-table, and lazy-table inputs.
enum class TransitionSystemVariant {
  kViewBased,
  VIEW_BASED = kViewBased,
};

struct TransitionSystemSelectorDescriptor {
  TransitionSystemVariant selector = TransitionSystemVariant::kViewBased;
  std::string name;
};

[[nodiscard]] TransitionSystem discover_transition_system(
    const EventLog& event_log,
    const TransitionSystemOptions& options = {});

[[nodiscard]] TransitionSystem discover_transition_system(
    const EventStream& event_stream,
    const TransitionSystemOptions& options = {});

[[nodiscard]] TransitionSystem discover_transition_system(
    const EventTable& event_table,
    const TransitionSystemOptions& options = {});

[[nodiscard]] TransitionSystem discover_transition_system(
    const LazyEventTable& event_table,
    const TransitionSystemOptions& options = {});

[[nodiscard]] TransitionSystem discover_transition_system(
    const EventDataView& data,
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
    const EventStream& event_stream,
    const TransitionSystemOptions& options = {}) {
  return discover_transition_system(event_stream, options);
}

[[nodiscard]] inline TransitionSystem discover_view_based_transition_system(
    const EventTable& event_table,
    const TransitionSystemOptions& options = {}) {
  return discover_transition_system(event_table, options);
}

[[nodiscard]] inline TransitionSystem discover_view_based_transition_system(
    const LazyEventTable& event_table,
    const TransitionSystemOptions& options = {}) {
  return discover_transition_system(event_table, options);
}

[[nodiscard]] inline TransitionSystem discover_view_based_transition_system(
    const EventDataView& data,
    const TransitionSystemOptions& options = {}) {
  return discover_transition_system(data, options);
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

class ViewBasedTransitionSystemDiscovery {
 public:
  [[nodiscard]] static TransitionSystem apply(
      const EventLog& event_log,
      const TransitionSystemOptions& options = {}) {
    return discover_transition_system(event_log, options);
  }
  [[nodiscard]] static TransitionSystem apply(
      const EventStream& event_stream,
      const TransitionSystemOptions& options = {}) {
    return discover_transition_system(event_stream, options);
  }
  [[nodiscard]] static TransitionSystem apply(
      const EventTable& event_table,
      const TransitionSystemOptions& options = {}) {
    return discover_transition_system(event_table, options);
  }
  [[nodiscard]] static TransitionSystem apply(
      const LazyEventTable& event_table,
      const TransitionSystemOptions& options = {}) {
    return discover_transition_system(event_table, options);
  }
  [[nodiscard]] static TransitionSystem apply(
      const EventDataView& data,
      const TransitionSystemOptions& options = {}) {
    return discover_transition_system(data, options);
  }
};

class TransitionSystemSelectorRegistry {
 public:
  [[nodiscard]] static const std::vector<TransitionSystemSelectorDescriptor>&
  all();
  [[nodiscard]] static bool supports(
      TransitionSystemVariant selector) noexcept;
  [[nodiscard]] static std::optional<TransitionSystemVariant> parse(
      std::string_view name);

  [[nodiscard]] static TransitionSystem dispatch(
      const EventLog& event_log, TransitionSystemVariant selector,
      const TransitionSystemOptions& options = {});
  [[nodiscard]] static TransitionSystem dispatch(
      const EventStream& event_stream, TransitionSystemVariant selector,
      const TransitionSystemOptions& options = {});
  [[nodiscard]] static TransitionSystem dispatch(
      const EventTable& event_table, TransitionSystemVariant selector,
      const TransitionSystemOptions& options = {});
  [[nodiscard]] static TransitionSystem dispatch(
      const LazyEventTable& event_table, TransitionSystemVariant selector,
      const TransitionSystemOptions& options = {});
  [[nodiscard]] static TransitionSystem dispatch(
      const EventDataView& data, TransitionSystemVariant selector,
      const TransitionSystemOptions& options = {});
};

[[nodiscard]] std::string_view transition_system_variant_name(
    TransitionSystemVariant selector);
[[nodiscard]] TransitionSystemVariant parse_transition_system_variant(
    std::string_view name);

}  // namespace minera
