#include "minera/alpha_miner.hpp"

#include <algorithm>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>

#include "minera/log_conversion.hpp"
#include "minera/timestamp.hpp"

namespace minera {
namespace {

bool canonical_key(std::string_view key, FirstClassAttributeRole role) {
  switch (role) {
    case FirstClassAttributeRole::kActivity:
      return key == "concept:name" || key == "activity";
    case FirstClassAttributeRole::kCaseId:
      return key == "case:concept:name" || key == "case:id" ||
             key == "case_id" || key == "case";
    case FirstClassAttributeRole::kTimestamp:
      return key == "time:timestamp" || key == "timestamp";
    default: return false;
  }
}

const EventAttribute* select_field(const EventRecord& record,
                                   std::string_view selector,
                                   FirstClassAttributeRole role,
                                   bool required = true,
                                   bool allow_null = false) {
  const EventAttribute* selected = nullptr;
  int best = 0;
  std::size_t matches = 0;
  for (const auto& field : record.fields) {
    const int priority = field.key == selector ? 3
        : (!field.first_class_source_key.empty() &&
           field.first_class_source_key == selector) ? 2
        : (canonical_key(selector, role) && canonical_key(field.key, role)) ? 1
        : 0;
    if (priority > best) {
      selected = &field;
      best = priority;
      matches = 1;
    } else if (priority != 0 && priority == best) {
      ++matches;
    }
  }
  if (matches > 1)
    throw std::invalid_argument("Alpha: ambiguous selected field '" +
                                std::string(selector) + "'");
  if (!allow_null && selected && (selected->is_null ||
      (selected->first_class_role != FirstClassAttributeRole::kNone &&
       selected->first_class_state != FirstClassAttributeState::kPresent))) {
    if (!required) return nullptr;
    throw std::invalid_argument("Alpha: null or missing selected field '" +
                                std::string(selector) + "'");
  }
  if (!selected && required)
    throw std::invalid_argument("Alpha: missing selected field '" +
                                std::string(selector) + "'");
  return selected;
}

void enforce_cap(std::size_t count, const AlphaMinerOptions& options) {
  if (options.max_rows_to_materialize &&
      count > *options.max_rows_to_materialize)
    throw std::length_error("Alpha: row materialization cap exceeded");
}

EventStream table_stream(const EventTable& table) {
  auto stream = event_table_to_event_stream(table);
  using Origin = std::pair<FirstClassAttributeRole, std::string>;
  std::vector<std::optional<Origin>> origins(table.columns.size());
  std::vector<bool> ambiguous(table.columns.size(), false);
  for (const auto& row : table.rows) {
    for (std::size_t column = 0; column < table.columns.size(); ++column) {
      if (!row[column]) continue;
      const auto& cell = *row[column];
      if (cell.first_class_source_key.empty() &&
          cell.first_class_role == FirstClassAttributeRole::kNone) continue;
      const Origin origin{cell.first_class_role, cell.first_class_source_key};
      if (origins[column] && origins[column] != origin) ambiguous[column] = true;
      else origins[column] = origin;
    }
  }
  // A missing cell in an existing column still wins selector precedence.
  // Retain that absence instead of allowing another alias to fill it in.
  for (std::size_t row = 0; row < table.rows.size(); ++row) {
    for (std::size_t column = 0; column < table.columns.size(); ++column) {
      if (table.rows[row][column]) continue;
      if (ambiguous[column])
        throw std::invalid_argument("Alpha: ambiguous missing column provenance: " +
                                    table.columns[column]);
      EventAttribute missing{table.columns[column], ""};
      missing.is_null = true;
      if (origins[column]) {
        missing.first_class_role = origins[column]->first;
        missing.first_class_source_key = origins[column]->second;
      }
      stream.events[row].fields.push_back(std::move(missing));
    }
  }
  return stream;
}

EventLog project_stream(EventStream stream, const AlphaMinerOptions& options) {
  enforce_cap(stream.size(), options);
  const std::string activity = options.activity_attribute.empty()
                                   ? "concept:name" : options.activity_attribute;
  if (options.case_id_attribute.empty())
    throw std::invalid_argument("Alpha: case-id selector must not be empty");
  for (auto& record : stream.events) {
    // Copy before editing: both selectors may intentionally name the same cell.
    auto selected_case = *select_field(record, options.case_id_attribute,
                                       FirstClassAttributeRole::kCaseId);
    auto selected_activity = *select_field(record, activity,
                                           FirstClassAttributeRole::kActivity);
    for (auto& field : record.fields) {
      if (field.first_class_role != FirstClassAttributeRole::kCaseId &&
          field.first_class_role != FirstClassAttributeRole::kActivity) continue;
      field.first_class_role = FirstClassAttributeRole::kNone;
      field.first_class_state = FirstClassAttributeState::kMissing;
      field.first_class_source_key.clear();
    }
    const auto append = [&](EventAttribute field, FirstClassAttributeRole role,
                            const std::string& selector) {
      field.first_class_role = role;
      field.first_class_state = FirstClassAttributeState::kPresent;
      field.first_class_source_key = selector;
      record.fields.push_back(std::move(field));
    };
    append(std::move(selected_case), FirstClassAttributeRole::kCaseId,
           options.case_id_attribute);
    append(std::move(selected_activity), FirstClassAttributeRole::kActivity,
           activity);
  }
  LogConversionOptions conversion;
  conversion.case_id_key = options.case_id_attribute;
  // Flat attributes remain event attributes. A stale prefixed case label must
  // not replace the explicitly selected case projection during grouping.
  conversion.include_case_attributes = false;
  return event_stream_to_event_log(stream, conversion);
}

EventLog project_table(const EventTable& table, const AlphaMinerOptions& options) {
  // Sentinels enforce schema precedence during validation only. Unselected
  // missing cells must remain absent from the returned event attributes.
  const auto validation = table_stream(table);
  const std::string activity = options.activity_attribute.empty()
                                   ? "concept:name" : options.activity_attribute;
  for (const auto& record : validation.events) {
    (void)select_field(record, options.case_id_attribute,
                       FirstClassAttributeRole::kCaseId);
    (void)select_field(record, activity, FirstClassAttributeRole::kActivity);
  }
  return project_stream(event_table_to_event_stream(table), options);
}

bool selector_available(const EventTable& table, const EventStream& stream,
                         std::string_view selector,
                         FirstClassAttributeRole role) {
  for (const auto& column : table.columns) {
    EventRecord schema{{{column, ""}}};
    if (select_field(schema, selector, role, false, true)) return true;
  }
  for (const auto& row : stream.events)
    if (select_field(row, selector, role, false, true)) return true;
  return false;
}

PetriNet discover_classic_table(const EventTable& table,
                                const AlphaMinerOptions& options) {
  enforce_cap(table.size(), options);
  auto stream = table_stream(table);
  const std::string activity = options.activity_attribute.empty()
                                   ? "concept:name" : options.activity_attribute;
  const auto check_schema = [&](std::string_view selector,
                                FirstClassAttributeRole role) {
    if (selector.empty() || !selector_available(table, stream, selector, role))
      throw std::invalid_argument("Alpha: missing selected table column '" +
                                  std::string(selector) + "'");
  };
  check_schema(options.case_id_attribute, FirstClassAttributeRole::kCaseId);
  check_schema(activity, FirstClassAttributeRole::kActivity);
  const bool reads_time = options.sort_caseid_required &&
                          options.sort_timestamp_along_case_id;
  check_schema(options.timestamp_attribute, FirstClassAttributeRole::kTimestamp);
  const std::string start = options.start_timestamp_attribute.empty()
                                ? "start_timestamp"
                                : options.start_timestamp_attribute;
  const bool has_start = selector_available(
      table, stream, start, FirstClassAttributeRole::kTimestamp);
  if (!options.start_timestamp_attribute.empty() && !has_start)
    check_schema(start, FirstClassAttributeRole::kTimestamp);

  // The table footprint uses sorted rows, while its boundary activities come
  // from the original per-case row order. Keep those two observations separate.
  struct Row {
    std::optional<std::string> case_id;
    std::optional<std::string> activity;
    double start = std::numeric_limits<double>::infinity();
    double completion = std::numeric_limits<double>::infinity();
  };
  std::vector<Row> rows;
  const auto value = [](const EventAttribute* field) -> std::optional<std::string> {
    if (!field || field->is_null ||
        (field->first_class_role != FirstClassAttributeRole::kNone &&
         field->first_class_state != FirstClassAttributeState::kPresent))
      return std::nullopt;
    return field->value;
  };
  const auto instant = [&](const EventAttribute* field) {
    const auto text = value(field);
    if (!text) return std::numeric_limits<double>::infinity();
    const auto* native_date = std::get_if<NativeDate>(&field->typed_value);
    const auto parsed = parse_timestamp(text->empty() && native_date
                                            ? native_date->value : *text);
    if (!parsed) throw std::invalid_argument("Alpha: invalid selected timestamp");
    return *parsed;
  };
  std::map<std::string, std::pair<std::string, std::string>> endpoints;
  for (const auto& record : stream.events) {
    Row row;
    const auto* selected_case = select_field(record, options.case_id_attribute,
        FirstClassAttributeRole::kCaseId, false, true);
    row.case_id = value(selected_case);
    if (row.case_id)
      row.case_id = std::to_string(static_cast<int>(selected_case->kind)) +
                    ":" + *row.case_id;
    row.activity = value(select_field(record, activity,
        FirstClassAttributeRole::kActivity, false, true));
    if (reads_time) {
      row.completion = instant(select_field(record, options.timestamp_attribute,
          FirstClassAttributeRole::kTimestamp, false, true));
      row.start = has_start ? instant(select_field(record, start,
          FirstClassAttributeRole::kTimestamp, false, true)) : row.completion;
    }
    if (row.case_id && row.activity) {
      const auto [entry, inserted] = endpoints.try_emplace(
          *row.case_id, *row.activity, *row.activity);
      if (!inserted) entry->second.second = *row.activity;
    }
    rows.push_back(std::move(row));
  }
  if (options.sort_caseid_required) {
    std::stable_sort(rows.begin(), rows.end(), [&](const Row& left, const Row& right) {
      if (left.case_id != right.case_id) {
        if (!left.case_id) return false;
        if (!right.case_id) return true;
        return *left.case_id < *right.case_id;
      }
      if (reads_time) {
        if (left.start != right.start) return left.start < right.start;
        if (left.completion != right.completion)
          return left.completion < right.completion;
      }
      return false;
    });
  }
  DfgFrequency graph;
  for (std::size_t index = 1; index < rows.size(); ++index) {
    const auto& previous = rows[index - 1];
    const auto& current = rows[index];
    if (previous.case_id && previous.case_id == current.case_id &&
        previous.activity && current.activity)
      ++graph[{*previous.activity, *current.activity}];
  }
  std::map<std::string, std::uint64_t> starts;
  std::map<std::string, std::uint64_t> ends;
  for (const auto& [case_id, bounds] : endpoints) {
    (void)case_id;
    ++starts[bounds.first];
    ++ends[bounds.second];
  }
  return discover_petri_net(graph, starts, ends, options);
}

}  // namespace

EventLog normalize_alpha_event_log(const EventDataView& data,
                                   const AlphaMinerOptions& options) {
  if (data.kind() == EventDataKind::kEventLog) {
    enforce_cap(data.log()->event_count(), options);
    return normalize_alpha_event_log(*data.log(), options);
  }
  LogConversionOptions conversion;
  conversion.max_rows_to_materialize = options.max_rows_to_materialize;
  if (data.kind() == EventDataKind::kEventStream)
    enforce_cap(data.stream()->size(), options);
  if (data.kind() == EventDataKind::kEventTable)
    enforce_cap(data.table()->size(), options);
  if (data.kind() == EventDataKind::kEventTable)
    return project_table(*data.table(), options);
  if (data.kind() == EventDataKind::kLazyEventTable)
    return project_table(data.lazy_table()->materialize(
        options.max_rows_to_materialize), options);
  return project_stream(event_data_to_event_stream(data, conversion), options);
}

EventLog normalize_alpha_event_log(const EventStream& data,
                                   const AlphaMinerOptions& options) {
  return normalize_alpha_event_log(EventDataView(data), options);
}
EventLog normalize_alpha_event_log(const EventTable& data,
                                   const AlphaMinerOptions& options) {
  return normalize_alpha_event_log(EventDataView(data), options);
}
EventLog normalize_alpha_event_log(const LazyEventTable& data,
                                   const AlphaMinerOptions& options) {
  return normalize_alpha_event_log(EventDataView(data), options);
}

PetriNet discover_petri_net(const EventDataView& data,
                            const AlphaMinerOptions& options) {
  if (data.kind() == EventDataKind::kEventLog)
    return discover_petri_net(*data.log(), options);
  if (!options.short_loops && data.kind() == EventDataKind::kEventTable)
    return discover_classic_table(*data.table(), options);
  if (!options.short_loops && data.kind() == EventDataKind::kLazyEventTable)
    return discover_classic_table(
        data.lazy_table()->materialize(options.max_rows_to_materialize), options);
  return discover_petri_net(normalize_alpha_event_log(data, options), options);
}
PetriNet discover_petri_net(const EventStream& data,
                            const AlphaMinerOptions& options) {
  return discover_petri_net(EventDataView(data), options);
}
PetriNet discover_petri_net(const EventTable& data,
                            const AlphaMinerOptions& options) {
  return discover_petri_net(EventDataView(data), options);
}
PetriNet discover_petri_net(const LazyEventTable& data,
                            const AlphaMinerOptions& options) {
  return discover_petri_net(EventDataView(data), options);
}

}  // namespace minera
