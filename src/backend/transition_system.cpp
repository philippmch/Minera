#include "minera/transition_system.hpp"

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <string>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

#include "polars_missing_column.hpp"
#include "python_scalar.hpp"
#include "frame_typed_activities.hpp"
#include "xes_frame_columns.hpp"

namespace minera {
namespace {

using Payload = TransitionPayload;

struct TraceSequence {
  std::size_t occurrence_trace_index = 0;
  TransitionPayload activities;
  std::vector<std::size_t> event_indices;
  // A typed column's own str() of each cell when `activities` holds the
  // name of its value's first label (empty otherwise).
  TransitionPayload shown;
};

Payload make_payload(const TransitionPayload& activities,
                     std::size_t begin, std::size_t end,
                     TransitionView view) {
  Payload payload(activities.begin() + static_cast<std::ptrdiff_t>(begin),
                  activities.begin() + static_cast<std::ptrdiff_t>(end));
  if (view == TransitionView::kSequence) return payload;
  std::sort(payload.begin(), payload.end());
  if (view == TransitionView::kSet) {
    payload.erase(std::unique(payload.begin(), payload.end()), payload.end());
  }
  return payload;
}

struct StateKey {
  Payload payload;
  Payload representation_order;

  friend bool operator<(const StateKey& left, const StateKey& right) {
    if (left.payload != right.payload) return left.payload < right.payload;
    return left.representation_order < right.representation_order;
  }
};

// Transitions are identified by the values of their endpoint states and their
// label, so a window whose activities arrive in another order reuses the edge.
struct EdgeKey {
  Payload source;
  Payload target;
  TransitionActivity label;

  friend bool operator<(const EdgeKey& left, const EdgeKey& right) {
    if (left.source < right.source) return true;
    if (right.source < left.source) return false;
    if (left.target < right.target) return true;
    if (right.target < left.target) return false;
    return left.label < right.label;
  }
};

const std::string* first_class_value(
    const std::string& value, FirstClassAttributeState state) {
  if (state == FirstClassAttributeState::kNull) return nullptr;
  if (state != FirstClassAttributeState::kMissing || !value.empty())
    return &value;
  return nullptr;
}

bool matches_source(std::string_view source_key, std::string_view selector) {
  return !source_key.empty() && source_key == selector;
}

const std::string* selected_event_value(const Event& event,
                                        std::string_view key,
                                        std::string_view role) {
  if (role == "activity" &&
      (key.empty() || key == "activity" || key == "concept:name" ||
       matches_source(event.activity_source_key, key))) {
    if (const auto* value =
            first_class_value(event.activity, event.activity_state))
      return value;
    if (event.activity_state != FirstClassAttributeState::kNull &&
        event.activity_attribute && !event.activity_attribute->is_null &&
        (key.empty() || key == event.activity_attribute->key ||
         matches_source(event.activity_source_key, key)))
      return &event.activity_attribute->value;
  }
  if (role == "resource" &&
      (key == "resource" || key == "org:resource" ||
       matches_source(event.resource_source_key, key))) {
    if (const auto* value =
            first_class_value(event.resource, event.resource_state))
      return value;
    if (event.resource_state != FirstClassAttributeState::kNull &&
        event.resource_attribute && !event.resource_attribute->is_null)
      return &event.resource_attribute->value;
  }
  if (role == "lifecycle" &&
      (key == "lifecycle" || key == "lifecycle:transition" ||
       matches_source(event.lifecycle_source_key, key))) {
    if (const auto* value =
            first_class_value(event.lifecycle, event.lifecycle_state))
      return value;
    if (event.lifecycle_state != FirstClassAttributeState::kNull &&
        event.lifecycle_attribute && !event.lifecycle_attribute->is_null)
      return &event.lifecycle_attribute->value;
  }
  return event.attribute(key);
}

TransitionActivity selected_activity(const Event& event, std::string_view key) {
  const auto role_is_null = [&](FirstClassAttributeState state,
                                std::string_view source,
                                std::initializer_list<std::string_view> aliases) {
    return state == FirstClassAttributeState::kNull &&
           (matches_source(source, key) ||
            std::find(aliases.begin(), aliases.end(), key) != aliases.end());
  };
  if (role_is_null(event.activity_state, event.activity_source_key,
                   {"activity", "concept:name"}) ||
      role_is_null(event.resource_state, event.resource_source_key,
                   {"resource", "org:resource"}) ||
      role_is_null(event.lifecycle_state, event.lifecycle_source_key,
                   {"lifecycle", "lifecycle:transition"}))
    return std::nullopt;
  if (const auto* value = selected_event_value(event, key, "activity"))
    return *value;
  if (const auto* value = selected_event_value(event, key, "resource"))
    return *value;
  if (const auto* value = selected_event_value(event, key, "lifecycle"))
    return *value;
  for (const auto& attribute : event.attributes) {
    if (attribute.key != key) continue;
    return attribute.is_null ? TransitionActivity{} : TransitionActivity{attribute.value};
  }
  throw std::invalid_argument("'" + std::string(key) + "'");
}

const EventAttribute* find_stream_field(
    const EventRecord& record, std::string_view key,
    FirstClassAttributeRole role,
    std::initializer_list<std::string_view> aliases) {
  if (!key.empty()) {
    if (const auto* exact = record.field(key)) return exact;
    for (const auto& field : record.fields) {
      if (field.first_class_role == role &&
          field.first_class_source_key == key)
        return &field;
    }
  }
  const auto is_alias = std::any_of(
      aliases.begin(), aliases.end(),
      [&](std::string_view alias) { return alias == key; });
  if (key.empty() || is_alias) {
    for (const auto& field : record.fields) {
      if (field.first_class_role == role) return &field;
    }
  }
  if (key.empty() || is_alias) {
    for (const auto alias : aliases) {
      if (const auto* exact = record.field(alias)) return exact;
    }
  }
  return nullptr;
}

const EventAttribute* find_table_field(
    const EventTable& table, const EventTable::Row& row, std::string_view key,
    FirstClassAttributeRole role,
    std::initializer_list<std::string_view> aliases) {
  auto find_column = [&](std::string_view name) -> const EventAttribute* {
    const auto found = std::find(table.columns.begin(), table.columns.end(),
                                 name);
    if (found == table.columns.end()) return nullptr;
    const auto index = static_cast<std::size_t>(
        std::distance(table.columns.begin(), found));
    if (!row[index]) return nullptr;
    return &*row[index];
  };
  if (!key.empty()) {
    if (const auto* exact = find_column(key)) return exact;
    for (std::size_t index = 0; index < table.columns.size(); ++index) {
      if (!row[index] || row[index]->first_class_role != role) continue;
      if (row[index]->first_class_source_key == key) return &*row[index];
    }
  }
  const auto is_alias = std::any_of(
      aliases.begin(), aliases.end(),
      [&](std::string_view alias) { return alias == key; });
  if (key.empty() || is_alias) {
    for (std::size_t index = 0; index < table.columns.size(); ++index) {
      if (!row[index]) continue;
      if (row[index]->first_class_role == role) return &*row[index];
    }
  }
  if (key.empty() || is_alias) {
    for (const auto alias : aliases) {
      if (const auto* exact = find_column(alias)) return exact;
    }
  }
  return nullptr;
}

const EventAttribute& required_flat_field(const EventAttribute* field,
                                          std::string_view kind,
                                          std::string_view key) {
  if (field == nullptr || field->is_null ||
      field->first_class_state == FirstClassAttributeState::kNull) {
    throw std::invalid_argument("transition system event is missing " +
                                std::string(kind) + " attribute '" +
                                std::string(key) + "'");
  }
  return *field;
}

std::string effective_activity_key(const TransitionSystemOptions& options) {
  const auto& key = options.activity_attribute.empty()
                        ? options.activity_key
                        : options.activity_attribute;
  if (key.empty())
    throw std::invalid_argument(
        "transition system activity selector must not be empty");
  return key;
}

std::string effective_case_id_key(const TransitionSystemOptions& options) {
  const auto& key = options.case_id_attribute.empty()
                        ? options.case_id_key
                        : options.case_id_attribute;
  if (key.empty())
    throw std::invalid_argument(
        "transition system case-id selector must not be empty");
  return key;
}

bool canonical_case_selector(std::string_view key) {
  return key.empty() || key == "case_id" || key == "case:id" ||
         key == "case:concept:name" || key == "concept:name" ||
         key == "case";
}

const std::string* selected_event_case(const Event& event,
                                       std::string_view key) {
  if (key.empty() || key == "case" || key == "case_id" ||
      key == "case:id" || key == "case:concept:name" ||
      key == "concept:name" || matches_source(event.case_id_source_key, key))
    if (const auto* value =
            first_class_value(event.case_id, event.case_id_state))
      return value;
  return event.attribute(key);
}

const std::string* selected_trace_case(const Trace& trace,
                                       std::string_view key) {
  if (key.empty() || key == "case" || key == "case_id" ||
      key == "case:id" || key == "case:concept:name" ||
      key == "concept:name" || matches_source(trace.case_id_source_key, key)) {
    if (const auto* value =
            first_class_value(trace.case_id, trace.case_id_state))
      return value;
    if (trace.case_id_state != FirstClassAttributeState::kNull &&
        trace.case_id_attribute && !trace.case_id_attribute->is_null)
      return &trace.case_id_attribute->value;
  }
  return trace.attribute(key);
}

std::string selected_case_id(const Trace& trace, const Event& event,
                             std::string_view key) {
  if (canonical_case_selector(key)) {
    if (const auto* value = selected_trace_case(trace, key)) return *value;
  } else {
    if (const auto* value = selected_event_case(event, key)) return *value;
    if (const auto* value = selected_trace_case(trace, key)) return *value;
  }
  if (key.rfind("case:", 0) == 0) {
    if (const auto* value = trace.attribute(key.substr(5))) return *value;
  } else if (const auto* value =
                 trace.attribute("case:" + std::string(key))) {
    return *value;
  }
  throw std::invalid_argument(
      "transition system trace is missing case-id attribute '" +
      std::string(key) + "'");
}

std::vector<TraceSequence> event_log_sequences(
    const EventLog& log, const TransitionSystemOptions& options) {
  const auto activity_key = effective_activity_key(options);
  const auto case_id_key = effective_case_id_key(options);
  std::vector<TraceSequence> sequences;
  sequences.reserve(log.traces.size());
  for (std::size_t trace_index = 0; trace_index < log.traces.size();
       ++trace_index) {
    const auto& trace = log.traces[trace_index];
    if (canonical_case_selector(case_id_key)) {
      TraceSequence sequence;
      sequence.occurrence_trace_index = trace_index;
      sequence.activities.reserve(trace.events.size());
      sequence.event_indices.reserve(trace.events.size());
      for (std::size_t event_index = 0; event_index < trace.events.size();
           ++event_index) {
        sequence.activities.push_back(
            selected_activity(trace.events[event_index], activity_key));
        sequence.event_indices.push_back(event_index);
      }
      sequences.push_back(std::move(sequence));
      continue;
    }

    // A custom case selector is useful for legacy trace objects that contain
    // several logical cases. The built-in selector above intentionally keeps
    // the enclosing trace as the boundary.
    std::map<std::string, std::size_t> local_groups;
    for (std::size_t event_index = 0; event_index < trace.events.size();
         ++event_index) {
      const auto& event = trace.events[event_index];
      const auto case_id = selected_case_id(trace, event, case_id_key);
      const auto [found, inserted] = local_groups.emplace(
          case_id, sequences.size());
      if (inserted) {
        TraceSequence sequence;
        sequence.occurrence_trace_index = trace_index;
        sequences.push_back(std::move(sequence));
      }
      auto& sequence = sequences[found->second];
      sequence.activities.push_back(selected_activity(event, activity_key));
      sequence.event_indices.push_back(event_index);
    }
  }
  return sequences;
}

template <typename RowReader>
std::vector<TraceSequence> flat_sequences(std::string_view activity_key,
                                           std::string_view case_id_key,
                                           RowReader&& read_rows) {
  if (activity_key.empty() || case_id_key.empty())
    throw std::invalid_argument(
        "transition system selectors must not be empty");
  std::vector<TraceSequence> sequences;
  std::map<std::string, std::size_t> groups;
  std::size_t row_index = 0;
  read_rows([&](const EventAttribute* activity,
                const EventAttribute* case_id) {
    const auto& selected_activity_field =
        required_flat_field(activity, "activity", activity_key);
    const auto& selected_case_field =
        required_flat_field(case_id, "case-id", case_id_key);
    const auto [found, inserted] = groups.emplace(selected_case_field.value,
                                                   sequences.size());
    if (inserted) {
      TraceSequence sequence;
      sequence.occurrence_trace_index = sequences.size();
      sequences.push_back(std::move(sequence));
    }
    auto& sequence = sequences[found->second];
    sequence.activities.push_back(selected_activity_field.value);
    sequence.event_indices.push_back(row_index++);
  });
  return sequences;
}

std::vector<TraceSequence> stream_sequences(
    const EventStream& stream, const TransitionSystemOptions& options) {
  const auto activity_key = effective_activity_key(options);
  const auto case_id_key = effective_case_id_key(options);
  return flat_sequences(activity_key, case_id_key,
                        [&](const auto& consume) {
                          for (const auto& record : stream.events) {
                            consume(find_stream_field(
                                        record, activity_key,
                                        FirstClassAttributeRole::kActivity,
                                        {"activity", "concept:name"}),
                                    find_stream_field(
                                        record, case_id_key,
                                        FirstClassAttributeRole::kCaseId,
                                        {"case_id", "case:id",
                                         "case:concept:name"}));
                          }
                        });
}

std::vector<TraceSequence> table_sequences(
    const EventTable& table, const TransitionSystemOptions& options) {
  table.validate();
  const auto activity_key = effective_activity_key(options);
  const auto case_id_key = effective_case_id_key(options);
  return flat_sequences(activity_key, case_id_key,
                        [&](const auto& consume) {
                          for (const auto& row : table.rows) {
                            consume(find_table_field(
                                        table, row, activity_key,
                                        FirstClassAttributeRole::kActivity,
                                        {"activity", "concept:name"}),
                                    find_table_field(
                                        table, row, case_id_key,
                                        FirstClassAttributeRole::kCaseId,
                                        {"case_id", "case:id",
                                         "case:concept:name"}));
                          }
                        });
}

std::pair<std::size_t, std::size_t> window_bounds(
    std::size_t index, std::size_t size, const TransitionSystemOptions& options) {
  if (options.window >= 0) {
    const auto width = static_cast<std::uint64_t>(options.window);
    if (options.direction == TransitionDirection::kForward)
      return {index, index + static_cast<std::size_t>(std::min<std::uint64_t>(width, size - index))};
    return {width > index ? 0 : index - static_cast<std::size_t>(width), index};
  }
  const auto magnitude = static_cast<std::uint64_t>(-(options.window + 1)) + 1;
  if (options.direction == TransitionDirection::kBackward) return {index, index};
  // Python slice endpoints below zero are relative to the trace end.
  const auto end = magnitude > index
      ? (magnitude - index > size ? 0 : size - static_cast<std::size_t>(magnitude - index))
      : index - static_cast<std::size_t>(magnitude);
  return {index, std::max(index, end)};
}

std::pair<StateKey, TransitionSystemState> window_state(
    const TransitionPayload& activities, std::size_t index,
    const TransitionSystemOptions& options) {
  const auto [begin, end] = window_bounds(index, activities.size(), options);
  TransitionSystemState state;
  state.payload = make_payload(activities, begin, end, options.view);
  if (options.view == TransitionView::kSequence) {
    state.name_order = state.payload;
  } else if (options.view == TransitionView::kSet) {
    state.name_order = state.payload;
  } else {
    for (auto position = begin; position < end; ++position) {
      if (std::find(state.name_order.begin(), state.name_order.end(), activities[position]) == state.name_order.end())
        state.name_order.push_back(activities[position]);
    }
  }
  auto representation_order = state.name_order;
  if (options.view == TransitionView::kMultiset) {
    std::map<TransitionActivity, std::size_t> counts;
    for (const auto& value : state.payload) ++counts[value];
    std::stable_sort(representation_order.begin(), representation_order.end(),
                     [&](const auto& left, const auto& right) { return counts.at(left) > counts.at(right); });
  }
  return {StateKey{state.payload, std::move(representation_order)}, std::move(state)};
}

template <typename Builder>
TransitionSystem build_transition_system(
    const std::vector<TraceSequence>& sequences,
    const TransitionSystemOptions& options, Builder&& occurrence_builder,
    bool distinct_missing_labels = false, bool typed_labels = false) {
  if (options.direction != TransitionDirection::kForward &&
      options.direction != TransitionDirection::kBackward) {
    throw std::invalid_argument("transition-system direction is invalid");
  }
  if (options.view != TransitionView::kSequence &&
      options.view != TransitionView::kSet &&
      options.view != TransitionView::kMultiset) {
    throw std::invalid_argument("transition-system view is invalid");
  }
  // Root membership follows the state's own representation (two equal
  // multisets written in a different order are both kept when they are
  // created in the same step), while a lookup matches any root state by
  // value and returns the first one registered with that value.
  std::map<StateKey, std::size_t> roots;
  std::map<Payload, std::size_t> roots_by_value;
  std::map<EdgeKey, std::size_t> edges;
  std::vector<TransitionSystemState> result_states;
  std::vector<StateKey> state_keys;
  std::vector<TransitionSystemTransition> result_transitions;
  // The step that created each state and transition.
  struct Origin {
    std::size_t sequence = 0;
    std::size_t index = 0;
  };
  std::vector<Origin> state_origins;
  std::vector<Origin> edge_origins;
  const auto lookup_state = [&](const StateKey& key, TransitionSystemState state, Origin origin) {
    if (const auto found = roots_by_value.find(key.payload); found != roots_by_value.end())
      return found->second;
    const auto identity = result_states.size();
    state.in_root = false;
    state.id = identity;
    result_states.push_back(std::move(state));
    state_keys.push_back(key);
    state_origins.push_back(origin);
    return identity;
  };
  // A typed column's own objects of a window, in the order of `values` (the
  // window's position for a sequence, a value's first cell for a set or
  // multiset): its repr, which places a state in the root set.
  const auto shown_order = [&](const TraceSequence& sequence, std::size_t index, const Payload& values) {
    const auto [begin, end] = window_bounds(index, sequence.activities.size(), options);
    Payload out;
    if (options.view == TransitionView::kSequence) {
      for (auto position = begin; position < end; ++position) out.push_back(sequence.shown[position]);
      return out;
    }
    for (const auto& value : values) {
      auto shown = value;
      for (auto position = begin; position < end; ++position)
        if (sequence.activities[position] == value) {
          shown = sequence.shown[position];
          break;
        }
      out.push_back(std::move(shown));
    }
    return out;
  };
  for (std::size_t sequence_index = 0; sequence_index < sequences.size(); ++sequence_index) {
    const auto& sequence = sequences[sequence_index];
    const auto& activities = sequence.activities;
    const bool shown = sequence.shown.size() == activities.size();
    for (std::size_t index = 0; index < activities.size(); ++index) {
      auto [source_key, source_state] = window_state(activities, index, options);
      auto [target_key, target_state] = window_state(activities, index + 1, options);
      if (shown) {
        source_key.representation_order = shown_order(sequence, index, source_key.representation_order);
        target_key.representation_order = shown_order(sequence, index + 1, target_key.representation_order);
      }
      // Both lookups precede registration. A new equal-valued target remains
      // a distinct endpoint object when the source takes the root-set slot.
      const auto source = lookup_state(source_key, std::move(source_state), Origin{sequence_index, index});
      const auto target = lookup_state(target_key, std::move(target_state), Origin{sequence_index, index + 1});
      const auto register_root = [&](std::size_t identity) {
        if (!roots.emplace(state_keys[identity], identity).second) return;
        result_states[identity].in_root = true;
        roots_by_value.emplace(state_keys[identity].payload, identity);
      };
      register_root(source);
      register_root(target);
      if (index == 0) ++result_states[source].start_count;
      if (index + 1 == activities.size()) ++result_states[target].end_count;
      EdgeKey key{source_key.payload, target_key.payload, activities[index]};
      // A missing activity of a pandas frame is NaN, which equals no label
      // (not even itself), so each step it labels is a transition of its own.
      std::size_t edge_index = result_transitions.size();
      bool inserted = true;
      // A typed column's NaN and NaT equal no label either (round GQ).
      const auto& label = activities[index];
      const bool never_equal = typed_labels ? label && !label->empty() && (label->front() == '\x1e' || label->front() == '\x1d')
                                            : distinct_missing_labels && !label;
      if (!never_equal) {
        const auto found = edges.emplace(key, result_transitions.size());
        edge_index = found.first->second;
        inserted = found.second;
      }
      if (inserted) {
        TransitionSystemTransition edge;
        edge.source = source;
        edge.target = target;
        edge.label = activities[index];
        result_transitions.push_back(std::move(edge));
        edge_origins.push_back(Origin{sequence_index, index});
      }
      auto& edge = result_transitions[edge_index];
      ++edge.count;
      if (options.include_data) {
        const auto occurrence = occurrence_builder(sequence, index);
        result_states[source].outgoing_occurrences.push_back(occurrence);
        result_states[target].incoming_occurrences.push_back(occurrence);
        edge.occurrences.push_back(occurrence);
      }
    }
  }
  // A typed column's equal values (1 and True, 0 and False, 1 and 1.0) share
  // a node, named after the object that created it: a state shows the window
  // of its first step, a transition the activity of its first step, as
  // Python's dict and set keep the first of equal keys (round GT, observed
  // black-box).
  for (std::size_t identity = 0; identity < result_states.size(); ++identity) {
    const auto& origin = state_origins[identity];
    const auto& sequence = sequences[origin.sequence];
    if (sequence.shown.size() != sequence.activities.size()) continue;
    const auto [begin, end] = window_bounds(origin.index, sequence.activities.size(), options);
    auto& state = result_states[identity];
    if (options.view == TransitionView::kSequence) {
      for (std::size_t k = 0; k < state.payload.size() && begin + k < end; ++k)
        state.payload[k] = sequence.shown[begin + k];
      state.name_order = state.payload;
      continue;
    }
    const auto own = [&](const TransitionActivity& value) {
      for (auto position = begin; position < end; ++position)
        if (sequence.activities[position] == value) return sequence.shown[position];
      return value;
    };
    for (auto& value : state.payload) value = own(value);
    for (auto& value : state.name_order) value = own(value);
  }
  for (std::size_t index = 0; index < result_transitions.size(); ++index) {
    const auto& origin = edge_origins[index];
    const auto& sequence = sequences[origin.sequence];
    if (sequence.shown.size() == sequence.activities.size() && origin.index < sequence.shown.size())
      result_transitions[index].label = sequence.shown[origin.index];
  }
  std::vector<std::size_t> order(result_states.size());
  std::iota(order.begin(), order.end(), 0);
  std::stable_sort(order.begin(), order.end(), [&](auto left, auto right) {
    if (state_keys[left] < state_keys[right]) return true;
    if (state_keys[right] < state_keys[left]) return false;
    return result_states[left].in_root && !result_states[right].in_root;
  });
  std::vector<std::size_t> remap(order.size());
  std::vector<TransitionSystemState> ordered_states;
  ordered_states.reserve(order.size());
  for (const auto previous : order) {
    remap[previous] = ordered_states.size();
    result_states[previous].id = ordered_states.size();
    ordered_states.push_back(std::move(result_states[previous]));
  }
  for (auto& edge : result_transitions) {
    edge.source = remap[edge.source];
    edge.target = remap[edge.target];
  }
  std::stable_sort(result_transitions.begin(), result_transitions.end(), [](const auto& left, const auto& right) {
    if (left.source != right.source) return left.source < right.source;
    if (left.target != right.target) return left.target < right.target;
    return left.label < right.label;
  });
  for (std::size_t index = 0; index < result_transitions.size(); ++index) {
    const auto& edge = result_transitions[index];
    ordered_states[edge.source].outgoing.push_back(index);
    ordered_states[edge.target].incoming.push_back(index);
  }
  return TransitionSystem(options, std::move(ordered_states), std::move(result_transitions));
}

TransitionSystem build_event_log_system(
    const EventLog& log, const TransitionSystemOptions& options) {
  const auto sequences = event_log_sequences(log, options);
  // A CSV log keyed by a column with gaps labels a NaN "\x1e" and a NaT
  // "\x1d" (round GR, the server's csv_keyed_family_log): they equal no
  // label, as on an XES frame (round GQ).
  bool typed_labels = false;
  for (const auto& sequence : sequences)
    for (const auto& activity : sequence.activities)
      if (activity && !activity->empty() && (activity->front() == '\x1e' || activity->front() == '\x1d'))
        typed_labels = true;
  return build_transition_system(
      sequences, options, [](const TraceSequence& sequence,
                             std::size_t event_index) {
        const auto source_index = event_index < sequence.event_indices.size()
                                      ? sequence.event_indices[event_index]
                                      : event_index;
        return TransitionOccurrence{sequence.occurrence_trace_index,
                                    source_index};
      }, false, typed_labels);
}

TransitionSystem build_flat_system(
    const std::vector<TraceSequence>& sequences,
    const TransitionSystemOptions& options) {
  return build_transition_system(
      sequences, options, [](const TraceSequence& sequence,
                             std::size_t event_index) {
        const auto source_index = event_index < sequence.event_indices.size()
                                      ? sequence.event_indices[event_index]
                                      : event_index;
        return TransitionOccurrence{sequence.occurrence_trace_index,
                                    source_index};
      });
}

// pm4py's view-based transition system on the frame of an XES upload
// (round FY, observed black-box). It reads each case's activities with a
// groupby over the case column: on pandas groupby(case)[activity] (sorted
// cases, rows without a case id left out; KeyError for a missing case
// column, then "Column not found" for a missing activity column), on Polars
// select(case, activity).group_by(case, maintain_order=True) (cases in the
// order of their first row, the rows without a case id one case of their
// own; a missing column fails the plan). A missing activity is a label
// (NaN, or None on Polars); NaN equals no label, so on pandas every step it
// labels is a transition of its own, while a state's name compares its list
// by identity and so matches.
std::vector<TraceSequence> xes_frame_sequences(const EventTable& source, bool polars,
                                               const TransitionSystemOptions& options, bool* typed_labels) {
  const auto activity_key = effective_activity_key(options);
  const auto case_id_key = effective_case_id_key(options);
  std::optional<EventTable> storage;
  const auto& table = detail::xes_frame_present_columns(source, storage, polars);
  const auto column_of = [&](const std::string& name) -> std::optional<std::size_t> {
    const auto found = std::find(table.columns.begin(), table.columns.end(), name);
    if (found == table.columns.end()) return std::nullopt;
    return static_cast<std::size_t>(found - table.columns.begin());
  };
  const auto case_at = column_of(case_id_key);
  const auto activity_at = column_of(activity_key);
  if (polars) {
    for (const auto& key : {case_id_key, activity_key})
      if (!column_of(key))
        throw std::invalid_argument(detail::polars_df_missing_column_error(key, table.columns, {"select", "group_by"}));
    // select(case, activity) of one column twice: the group_by's
    // resolution raises DuplicateError (round GQ, observed black-box).
    if (case_id_key == activity_key) {
      std::string frame = "DF [";
      for (std::size_t i = 0; i < table.columns.size() && i < 4; ++i)
        frame += (i ? ", " : "") + detail::polars_frame_quoted(table.columns[i]);
      if (table.columns.size() > 4) frame += ", ...";
      frame += "]; PROJECT */" + std::to_string(table.columns.size()) + " COLUMNS";
      throw std::invalid_argument(
          "projections contained duplicate output name '" + case_id_key +
          "'. It's possible that multiple expressions are returning the same default column name. If this is the "
          "case, try renaming the columns with `.alias(\"new_name\")` to avoid duplicate column names.\n\n"
          "Resolved plan until failure:\n\n\t---> FAILED HERE RESOLVING 'group_by' <---\n" + frame +
          "\n\nThis error occurred with the following context stack:\n\t[1] 'select'\n\t[2] 'group_by'\n");
    }
  } else {
    if (!case_at) throw std::invalid_argument("KeyError: " + detail::frame_python_str_repr(case_id_key));
    if (!activity_at)
      throw std::invalid_argument("KeyError: " + detail::frame_python_str_repr("Column not found: " + activity_key));
  }
  const auto cell = [](const EventTable::Row& row, std::size_t at) -> const EventAttribute* {
    if (at >= row.size() || !row[at] || row[at]->is_null) return nullptr;
    return &*row[at];
  };
  std::vector<std::vector<std::size_t>> groups;
  if (polars) {
    std::map<std::optional<std::string>, std::size_t> index;
    for (std::size_t row = 0; row < table.rows.size(); ++row) {
      const auto* id = cell(table.rows[row], *case_at);
      const auto [it, added] = index.emplace(id ? std::optional<std::string>(python::identity(python::from_attribute(*id)))
                                                : std::nullopt, groups.size());
      if (added) groups.emplace_back();
      groups[it->second].push_back(row);
    }
  } else {
    std::vector<std::vector<python::Scalar>> keys;
    keys.reserve(table.rows.size());
    for (const auto& row : table.rows) {
      const auto* id = cell(row, *case_at);
      keys.push_back({id ? python::from_attribute(*id) : python::real(std::numeric_limits<double>::quiet_NaN())});
    }
    for (auto& group : python::groupby(keys)) groups.push_back(std::move(group.rows));
  }
  // A column of other kinds than text reads each cell's Python object (round
  // GQ, observed black-box): str() names it, Python's == matches labels
  // (int 1 and float 1.0 alike, a NaN or NaT never), a state's list or set
  // matches a NaN by the object (each cell of a float64 column its own) and
  // NaT always; pandas' set and multiset views hash a dict cell, which
  // raises. Polars' copy holds a gap as None.
  const auto typed = frame_typed_activities(table, *activity_at, polars);
  if (typed) *typed_labels = true;
  std::vector<TraceSequence> sequences;
  sequences.reserve(groups.size());
  for (const auto& rows : groups) {
    TraceSequence sequence;
    sequence.occurrence_trace_index = sequences.size();
    for (const auto row : rows) {
      if (typed) {
        const auto& value = typed->cells[row];
        const auto& name = typed->names[row];
        if (!polars && value.type == "dict" && options.view != TransitionView::kSequence)
          throw std::invalid_argument("TypeError: unhashable type: 'dict'");
        if (value.type == "NoneType")
          sequence.activities.push_back(polars ? TransitionActivity{} : TransitionActivity("\x1c" + name));
        else if (value.nan && !polars)
          sequence.activities.push_back("\x1e" + name);
        else if (value.type == "NaTType" && !polars)
          sequence.activities.push_back("\x1d" + name);
        else
          sequence.activities.push_back(name);
        // pandas' cells keep their own objects; Polars' typed column one
        // dtype, whose equal values print alike.
        sequence.shown.push_back(!polars && !value.missing && !value.nan && value.type != "NaTType" &&
                                         value.type != "NoneType"
                                     ? TransitionActivity(value.label)
                                     : sequence.activities.back());
      } else {
        const auto* activity = cell(table.rows[row], *activity_at);
        sequence.activities.push_back(activity ? TransitionActivity(activity->value) : std::nullopt);
      }
      sequence.event_indices.push_back(row);
    }
    sequences.push_back(std::move(sequence));
  }
  return sequences;
}

}  // namespace

TransitionSystem discover_transition_system_xes_frame(const EventDataView& data,
                                                      const TransitionSystemOptions& options) {
  if (data.kind() != EventDataKind::kEventTable && data.kind() != EventDataKind::kLazyEventTable)
    return discover_transition_system(data, options);
  const bool polars = data.kind() == EventDataKind::kLazyEventTable;
  if (polars && !data.lazy_table()->valid())
    throw std::invalid_argument("transition system lazy table is invalid");
  bool typed_labels = false;
  const auto sequences = polars ? xes_frame_sequences(data.lazy_table()->materialize(), true, options, &typed_labels)
                                : xes_frame_sequences(*data.table(), false, options, &typed_labels);
  return build_transition_system(
      sequences, options,
      [](const TraceSequence& sequence, std::size_t event_index) {
        const auto source_index = event_index < sequence.event_indices.size()
                                      ? sequence.event_indices[event_index]
                                      : event_index;
        return TransitionOccurrence{sequence.occurrence_trace_index, source_index};
      },
      /*distinct_missing_labels=*/!polars, typed_labels);
}

TransitionActivity transition_activity_shown(const TransitionActivity& activity) {
  if (!activity || activity->empty()) return activity;
  std::string_view shown(*activity);
  if (shown.front() == '\x1e' || shown.front() == '\x1c') return std::nullopt;
  if (shown.front() == '\x1d') shown.remove_prefix(1);
  const auto end = shown.find_last_not_of('\x1f');
  return std::string(end == std::string_view::npos ? shown : shown.substr(0, end + 1));
}

TransitionActivity transition_system_event_activity(
    const Event& event, std::string_view activity_key) {
  return selected_activity(event, activity_key);
}

TransitionSystem discover_transition_system(
    const EventLog& event_log, const TransitionSystemOptions& options) {
  return build_event_log_system(event_log, options);
}

TransitionSystem discover_transition_system(
    const EventStream& event_stream, const TransitionSystemOptions& options) {
  return build_flat_system(stream_sequences(event_stream, options), options);
}

TransitionSystem discover_transition_system(
    const EventTable& event_table, const TransitionSystemOptions& options) {
  return build_flat_system(table_sequences(event_table, options), options);
}

TransitionSystem discover_transition_system(
    const LazyEventTable& event_table, const TransitionSystemOptions& options) {
  if (!event_table.valid())
    throw std::invalid_argument("transition system lazy table is invalid");
  return build_flat_system(table_sequences(event_table.materialize(), options),
                           options);
}

TransitionSystem discover_transition_system(
    const EventDataView& data, const TransitionSystemOptions& options) {
  switch (data.kind()) {
    case EventDataKind::kEventLog:
      return discover_transition_system(*data.log(), options);
    case EventDataKind::kEventStream:
      return discover_transition_system(*data.stream(), options);
    case EventDataKind::kEventTable:
      return discover_transition_system(*data.table(), options);
    case EventDataKind::kLazyEventTable:
      return discover_transition_system(*data.lazy_table(), options);
  }
  throw std::invalid_argument("event data: invalid input kind");
}

TransitionSystem discover_transition_system(
    const EventLog& event_log, TransitionDirection direction,
    std::int64_t window, TransitionView view, bool include_data) {
  TransitionSystemOptions options;
  options.direction = direction;
  options.view = view;
  options.window = window;
  options.include_data = include_data;
  return discover_transition_system(event_log, options);
}

const std::vector<TransitionSystemSelectorDescriptor>&
TransitionSystemSelectorRegistry::all() {
  static const std::vector<TransitionSystemSelectorDescriptor> selectors = {
      {TransitionSystemVariant::kViewBased, "VIEW_BASED"}};
  return selectors;
}

bool TransitionSystemSelectorRegistry::supports(
    TransitionSystemVariant selector) noexcept {
  return selector == TransitionSystemVariant::kViewBased;
}

std::optional<TransitionSystemVariant> TransitionSystemSelectorRegistry::parse(
    std::string_view name) {
  if (name == "VIEW_BASED" || name == "view_based" ||
      name == "view-based" || name == "view")
    return TransitionSystemVariant::kViewBased;
  return std::nullopt;
}

TransitionSystem TransitionSystemSelectorRegistry::dispatch(
    const EventLog& event_log, TransitionSystemVariant selector,
    const TransitionSystemOptions& options) {
  if (!supports(selector))
    throw std::invalid_argument("transition system: unsupported selector");
  return ViewBasedTransitionSystemDiscovery::apply(event_log, options);
}

TransitionSystem TransitionSystemSelectorRegistry::dispatch(
    const EventStream& event_stream, TransitionSystemVariant selector,
    const TransitionSystemOptions& options) {
  if (!supports(selector))
    throw std::invalid_argument("transition system: unsupported selector");
  return ViewBasedTransitionSystemDiscovery::apply(event_stream, options);
}

TransitionSystem TransitionSystemSelectorRegistry::dispatch(
    const EventTable& event_table, TransitionSystemVariant selector,
    const TransitionSystemOptions& options) {
  if (!supports(selector))
    throw std::invalid_argument("transition system: unsupported selector");
  return ViewBasedTransitionSystemDiscovery::apply(event_table, options);
}

TransitionSystem TransitionSystemSelectorRegistry::dispatch(
    const LazyEventTable& event_table, TransitionSystemVariant selector,
    const TransitionSystemOptions& options) {
  if (!supports(selector))
    throw std::invalid_argument("transition system: unsupported selector");
  return ViewBasedTransitionSystemDiscovery::apply(event_table, options);
}

TransitionSystem TransitionSystemSelectorRegistry::dispatch(
    const EventDataView& data, TransitionSystemVariant selector,
    const TransitionSystemOptions& options) {
  if (!supports(selector))
    throw std::invalid_argument("transition system: unsupported selector");
  return ViewBasedTransitionSystemDiscovery::apply(data, options);
}

std::string_view transition_system_variant_name(
    TransitionSystemVariant selector) {
  if (selector == TransitionSystemVariant::kViewBased) return "VIEW_BASED";
  throw std::invalid_argument("transition system: invalid selector");
}

TransitionSystemVariant parse_transition_system_variant(std::string_view name) {
  const auto parsed = TransitionSystemSelectorRegistry::parse(name);
  if (!parsed)
    throw std::invalid_argument("transition system: unsupported selector");
  return *parsed;
}

}  // namespace minera
