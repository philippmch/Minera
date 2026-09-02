#include "minera/transition_system.hpp"

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <map>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace minera {
namespace {

using Payload = std::vector<std::string>;

struct TraceSequence {
  std::size_t occurrence_trace_index = 0;
  std::vector<std::string> activities;
  std::vector<std::size_t> event_indices;
};

Payload make_payload(const std::vector<std::string>& activities,
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

struct EdgeKey {
  Payload source;
  Payload target;
  std::string label;

  friend bool operator<(const EdgeKey& left, const EdgeKey& right) {
    if (left.source != right.source) return left.source < right.source;
    if (left.target != right.target) return left.target < right.target;
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

std::string selected_activity(const Event& event, std::string_view key) {
  if (const auto* value = selected_event_value(event, key, "activity"))
    return *value;
  if (const auto* value = selected_event_value(event, key, "resource"))
    return *value;
  if (const auto* value = selected_event_value(event, key, "lifecycle"))
    return *value;
  throw std::invalid_argument(
      "transition system event is missing activity attribute '" +
      std::string(key) + "'");
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

template <typename Builder>
TransitionSystem build_transition_system(
    const std::vector<TraceSequence>& sequences,
    const TransitionSystemOptions& options, Builder&& occurrence_builder) {
  if (options.direction != TransitionDirection::kForward &&
      options.direction != TransitionDirection::kBackward) {
    throw std::invalid_argument("transition-system direction is invalid");
  }
  if (options.view != TransitionView::kSequence &&
      options.view != TransitionView::kSet &&
      options.view != TransitionView::kMultiset) {
    throw std::invalid_argument("transition-system view is invalid");
  }

  std::map<Payload, TransitionSystemState> states;
  std::map<EdgeKey, TransitionSystemTransition> edges;
  for (const auto& sequence : sequences) {
    const auto& activities = sequence.activities;
    const std::size_t n = activities.size();
    if (n == 0) continue;

    std::vector<Payload> trace_states;
    trace_states.reserve(n + 1);
    for (std::size_t index = 0; index <= n; ++index) {
      std::size_t begin = 0;
      std::size_t end = 0;
      if (options.direction == TransitionDirection::kForward) {
        begin = index;
        const auto remaining = n - index;
        end = index + std::min(options.window, remaining);
      } else {
        end = index;
        begin = index > options.window ? index - options.window : 0;
      }
      trace_states.push_back(
          make_payload(activities, begin, end, options.view));
      auto& state = states[trace_states.back()];
      if (index == 0) ++state.start_count;
      if (index == n) ++state.end_count;
    }

    for (std::size_t index = 0; index < n; ++index) {
      const auto occurrence = occurrence_builder(sequence, index);
      auto& source = states[trace_states[index]];
      auto& target = states[trace_states[index + 1]];
      if (options.include_data) {
        source.outgoing_occurrences.push_back(occurrence);
        target.incoming_occurrences.push_back(occurrence);
      }
      EdgeKey key{trace_states[index], trace_states[index + 1],
                  activities[index]};
      auto& edge = edges[key];
      ++edge.count;
      if (options.include_data) edge.occurrences.push_back(occurrence);
    }
  }

  std::vector<TransitionSystemState> result_states;
  result_states.reserve(states.size());
  std::map<Payload, std::size_t> ids;
  for (auto& [payload, state] : states) {
    state.id = result_states.size();
    ids.emplace(payload, state.id);
    state.payload = payload;
    result_states.push_back(std::move(state));
  }
  std::vector<TransitionSystemTransition> result_transitions;
  result_transitions.reserve(edges.size());
  for (auto& [key, edge] : edges) {
    edge.source = ids.at(key.source);
    edge.target = ids.at(key.target);
    edge.label = key.label;
    result_transitions.push_back(std::move(edge));
  }
  return TransitionSystem(std::move(options), std::move(result_states),
                          std::move(result_transitions));
}

TransitionSystem build_event_log_system(
    const EventLog& log, const TransitionSystemOptions& options) {
  const auto sequences = event_log_sequences(log, options);
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

}  // namespace

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
    std::size_t window, TransitionView view, bool include_data) {
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
