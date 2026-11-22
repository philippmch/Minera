#include "minera/alpha_miner.hpp"

#include <algorithm>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string_view>
#include <utility>

#include "minera/log_conversion.hpp"
#include "minera/timestamp.hpp"
#include "alpha_python_nan.hpp"
#include "frame_key_column.hpp"
#include "polars_sort.hpp"
#include "xes_frame_checks.hpp"

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

// The selected column's dtype when the table names it.
std::string_view selected_dtype(const EventTable& table, std::string_view selector) {
  for (std::size_t index = 0; index < table.columns.size() && index < table.dtypes.size(); ++index)
    if (table.columns[index] == selector) return table.dtypes[index];
  return {};
}

bool text_dtype(std::string_view dtype) {
  return dtype == "str" || dtype == "object" || dtype == "string";
}

// pm4py's Alpha on a frame (get_dfg_graph) sorts the frame itself: pandas by
// case, start and completion with a stable lexsort (a missing value last),
// Polars with its unstable pdqsort (a missing value first). A text timestamp
// column (the workflow-input route keeps a CSV's columns as text) sorts as
// text, so "2020-1-5 9:05" follows "2020-01-05 10:00:00" (round FU, observed
// black-box). The sort switches are Minera's own (pm4py's Alpha always sorts
// by case and timestamps); without the timestamps the sort stays stable.
// On the frame of an XES upload (`xes_frame`, round FZ, observed black-box) a
// missing activity is a label of its own among the boundary activities: on
// pandas a case's first and last named activity (GroupBy.first and last skip
// NaN), NaN when the case has none; on Polars the first and last row's,
// None included. The rows without a case id form one case of Polars'
// boundary activities, but no pair of theirs counts (the directly-follows
// pairs need a case id); a pair with a missing activity never counts.
PetriNet discover_classic_table(const EventTable& table,
                                const AlphaMinerOptions& options,
                                bool polars = false, bool xes_frame = false) {
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
  const std::string start = options.start_timestamp_attribute.empty()
                                ? "start_timestamp"
                                : options.start_timestamp_attribute;
  const bool has_start = selector_available(
      table, stream, start, FirstClassAttributeRole::kTimestamp);
  // An XES upload's frame with an explicit start key the frame holds (round
  // GB, observed black-box): the rows sort by case, start and completion,
  // each column by its own values (a text start as text); Polars sorts
  // without the completion when the frame lacks it, and reads a start the
  // frame lacks as an alias of the completion (xes_frame_checks.hpp raises
  // the missing columns' errors).
  const bool explicit_frame_start = xes_frame && !options.start_timestamp_attribute.empty();
  const bool has_completion = selector_available(
      table, stream, options.timestamp_attribute, FirstClassAttributeRole::kTimestamp);
  if (!(explicit_frame_start && polars && has_start && !has_completion))
    check_schema(options.timestamp_attribute, FirstClassAttributeRole::kTimestamp);
  if (!options.start_timestamp_attribute.empty() && !has_start && !xes_frame)
    check_schema(start, FirstClassAttributeRole::kTimestamp);

  // The table footprint uses sorted rows, while its boundary activities come
  // from the original per-case row order. Keep those two observations separate.
  const bool text_times = !explicit_frame_start &&
                          text_dtype(selected_dtype(table, options.timestamp_attribute)) &&
                          (!has_start || text_dtype(selected_dtype(table, start)));
  const auto frame_text = [&](std::string_view selector) { return detail::xes_frame_text_column(table, selector); };
  // Round GC: a start column the frame holds sorts by its ranks
  // (frame_key_column.hpp), whatever its values (numbers, booleans, dates
  // beside text).
  const bool ranked_start = explicit_frame_start && has_start;
  std::vector<python::Scalar> start_cells;
  const bool text_completion = explicit_frame_start && has_completion && frame_text(options.timestamp_attribute);
  struct Row {
    std::optional<std::string> case_id;
    std::optional<std::string> activity;
    double start = std::numeric_limits<double>::infinity();
    double completion = std::numeric_limits<double>::infinity();
    std::optional<std::string> start_text;
    std::optional<std::string> completion_text;
    std::size_t start_rank = 0;
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
  struct FrameEnds {
    std::optional<std::string> first, last;
    bool named = false;
  };
  std::map<std::optional<std::string>, FrameEnds> frame_ends;
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
    if (reads_time && explicit_frame_start) {
      const auto* completion = has_completion ? select_field(record, options.timestamp_attribute,
          FirstClassAttributeRole::kTimestamp, false, true) : nullptr;
      if (text_completion) row.completion_text = value(completion);
      else row.completion = instant(completion);
      if (!has_start) {
        row.start_text = row.completion_text;
        row.start = row.completion;
      } else {
        const auto* begin = select_field(record, start, FirstClassAttributeRole::kTimestamp, false, true);
        start_cells.push_back(detail::frame_key_cell(value(begin) ? begin : nullptr));
      }
    } else if (reads_time && text_times) {
      row.completion_text = value(select_field(record, options.timestamp_attribute,
          FirstClassAttributeRole::kTimestamp, false, true));
      row.start_text = has_start ? value(select_field(record, start,
          FirstClassAttributeRole::kTimestamp, false, true)) : row.completion_text;
    } else if (reads_time) {
      row.completion = instant(select_field(record, options.timestamp_attribute,
          FirstClassAttributeRole::kTimestamp, false, true));
      row.start = has_start ? instant(select_field(record, start,
          FirstClassAttributeRole::kTimestamp, false, true)) : row.completion;
    }
    if (xes_frame && polars) {
      const auto [entry, inserted] = frame_ends.try_emplace(row.case_id);
      if (inserted) entry->second.first = row.activity;
      entry->second.last = row.activity;
    } else if (xes_frame) {
      if (row.case_id) {
        auto& ends = frame_ends[row.case_id];
        if (row.activity) {
          if (!ends.named) ends.first = row.activity;
          ends.named = true;
          ends.last = row.activity;
        }
      }
    } else if (row.case_id && row.activity) {
      const auto [entry, inserted] = endpoints.try_emplace(
          *row.case_id, *row.activity, *row.activity);
      if (!inserted) entry->second.second = *row.activity;
    }
    rows.push_back(std::move(row));
  }
  if (ranked_start && reads_time && start_cells.size() == rows.size()) {
    const auto column = detail::frame_key_column(std::move(start_cells));
    const auto ranks = polars ? detail::frame_key_polars_ranks(column) : detail::frame_key_pandas_ranks(column);
    for (std::size_t index = 0; index < rows.size(); ++index) rows[index].start_rank = ranks[index];
  }
  // -1: left first, 1: right first, 0: tie; a missing value last for
  // pandas and first for Polars.
  const auto compare_missing = [&](bool left_missing, bool right_missing) {
    if (left_missing == right_missing) return 0;
    return left_missing == polars ? -1 : 1;
  };
  const auto compare_text = [&](const std::optional<std::string>& left,
                                const std::optional<std::string>& right) {
    if (!left || !right) return compare_missing(!left, !right);
    return *left < *right ? -1 : (*right < *left ? 1 : 0);
  };
  const auto compare_instant = [&](double left, double right) {
    const bool left_missing = left == std::numeric_limits<double>::infinity();
    const bool right_missing = right == std::numeric_limits<double>::infinity();
    if (left_missing || right_missing) return compare_missing(left_missing, right_missing);
    return left < right ? -1 : (right < left ? 1 : 0);
  };
  const auto compare_rows = [&](const Row& left, const Row& right) {
    if (const int by = compare_text(left.case_id, right.case_id)) return by;
    if (!reads_time) return 0;
    if (explicit_frame_start) {
      if (ranked_start) {
        if (left.start_rank != right.start_rank) return left.start_rank < right.start_rank ? -1 : 1;
      } else if (const int by = text_completion ? compare_text(left.start_text, right.start_text)
                                                : compare_instant(left.start, right.start)) {
        return by;
      }
      return text_completion ? compare_text(left.completion_text, right.completion_text)
                             : compare_instant(left.completion, right.completion);
    }
    if (text_times) {
      if (const int by = compare_text(left.start_text, right.start_text)) return by;
      return compare_text(left.completion_text, right.completion_text);
    }
    if (const int by = compare_instant(left.start, right.start)) return by;
    return compare_instant(left.completion, right.completion);
  };
  if (options.sort_caseid_required) {
    std::vector<std::size_t> order(rows.size());
    for (std::size_t index = 0; index < order.size(); ++index) order[index] = index;
    if (polars && reads_time) {
      polars_sort::sort_unstable(order, [&](std::size_t left, std::size_t right) {
        return compare_rows(rows[left], rows[right]) < 0;
      });
    } else {
      std::stable_sort(order.begin(), order.end(), [&](std::size_t left, std::size_t right) {
        return compare_rows(rows[left], rows[right]) < 0;
      });
    }
    std::vector<Row> sorted_rows;
    sorted_rows.reserve(rows.size());
    for (const auto index : order) sorted_rows.push_back(std::move(rows[index]));
    rows = std::move(sorted_rows);
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
  if (!xes_frame) return discover_petri_net(graph, starts, ends, options);
  bool null_start = false, null_end = false;
  for (const auto& [case_id, bounds] : frame_ends) {
    (void)case_id;
    if (bounds.first) ++starts[*bounds.first]; else null_start = true;
    if (bounds.last) ++ends[*bounds.last]; else null_end = true;
  }
  return discover_petri_net_with_null_endpoints(graph, starts, ends, null_start, null_end, options);
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

AlphaPlusDiscoveryResult discover_alpha_plus_result(
    const EventDataView& data, const AlphaMinerOptions& options) {
  if (data.kind() == EventDataKind::kEventLog)
    return discover_alpha_plus_result(*data.log(), options);
  return discover_alpha_plus_result(normalize_alpha_event_log(data, options), options);
}
AlphaPlusDiscoveryResult discover_alpha_plus_result(
    const EventStream& data, const AlphaMinerOptions& options) {
  return discover_alpha_plus_result(EventDataView(data), options);
}
AlphaPlusDiscoveryResult discover_alpha_plus_result(
    const EventTable& data, const AlphaMinerOptions& options) {
  return discover_alpha_plus_result(EventDataView(data), options);
}
AlphaPlusDiscoveryResult discover_alpha_plus_result(
    const LazyEventTable& data, const AlphaMinerOptions& options) {
  return discover_alpha_plus_result(EventDataView(data), options);
}

PetriNet discover_petri_net(const EventDataView& data,
                            const AlphaMinerOptions& options) {
  if (data.kind() == EventDataKind::kEventLog)
    return discover_petri_net(*data.log(), options);
  if (!options.short_loops && data.kind() == EventDataKind::kEventTable)
    return discover_classic_table(*data.table(), options);
  if (!options.short_loops && data.kind() == EventDataKind::kLazyEventTable)
    return discover_classic_table(
        data.lazy_table()->materialize(options.max_rows_to_materialize), options, true);
  return discover_petri_net(normalize_alpha_event_log(data, options), options);
}
PetriNet discover_petri_net(const EventStream& data,
                            const AlphaMinerOptions& options) {
  return discover_petri_net(EventDataView(data), options);
}

AlphaPlusDiscoveryResult discover_alpha_plus_result_xes_frame(const EventDataView& data,
                                                              const AlphaMinerOptions& options) {
  const bool polars = data.kind() == EventDataKind::kLazyEventTable;
  if (!polars && data.kind() != EventDataKind::kEventTable) return discover_alpha_plus_result(data, options);
  std::optional<EventTable> materialized;
  const EventTable* source = polars
      ? &materialized.emplace(data.lazy_table()->materialize(options.max_rows_to_materialize))
      : data.table();
  std::optional<EventTable> storage;
  const auto& table = detail::xes_frame_present_columns(*source, storage, polars);
  if (table.columns.empty() || table.rows.empty()) throw std::invalid_argument("IndexError: list index out of range");
  const std::string activity_key = options.activity_attribute.empty() ? "concept:name" : options.activity_attribute;
  const auto column_of = [&](const std::string& name) -> std::optional<std::size_t> {
    const auto found = std::find(table.columns.begin(), table.columns.end(), name);
    if (found == table.columns.end()) return std::nullopt;
    return static_cast<std::size_t>(found - table.columns.begin());
  };
  const auto case_at = column_of(options.case_id_attribute);
  if (!case_at) throw std::invalid_argument("KeyError: " + detail::frame_python_str_repr(options.case_id_attribute));
  const auto activity_at = column_of(activity_key);
  if (!activity_at) throw std::invalid_argument("KeyError: " + detail::frame_python_str_repr(activity_key));
  const auto value = [](const EventTable::Row& row, std::size_t at) -> const std::string* {
    if (at >= row.size() || !row[at] || row[at]->is_null) return nullptr;
    return &row[at]->value;
  };
  // Python's str of the missing label, apart from the named activities.
  std::set<std::string> named;
  for (const auto& row : table.rows)
    if (const auto* activity = value(row, *activity_at)) named.insert(*activity);
  std::string null_label = polars ? "None" : "nan";
  while (named.contains(null_label)) null_label += '\x1f';
  bool any_missing = false;
  EventLog log;
  std::map<std::optional<std::string>, std::size_t> trace_of;
  for (const auto& row : table.rows) {
    const auto* case_id = value(row, *case_at);
    const auto key = case_id ? std::optional<std::string>(*case_id) : std::nullopt;
    const auto [it, inserted] = trace_of.try_emplace(key, log.traces.size());
    if (inserted) {
      Trace trace;
      trace.case_id = key.value_or("");
      trace.case_id_state = key ? FirstClassAttributeState::kPresent : FirstClassAttributeState::kNull;
      trace.case_id_source_key = options.case_id_attribute;
      log.traces.push_back(std::move(trace));
    }
    Event event;
    const auto* activity = value(row, *activity_at);
    any_missing = any_missing || !activity;
    event.activity = activity ? *activity : null_label;
    event.activity_state = FirstClassAttributeState::kPresent;
    log.traces[it->second].events.push_back(std::move(event));
  }
  auto projected = options;
  projected.activity_attribute = "concept:name";
  auto result = !polars && any_missing
      ? detail::discover_alpha_plus_result_python_nan(log, projected, null_label)
      : discover_alpha_plus_result(log, projected);
  if (polars && any_missing) result.null_label = null_label;
  return result;
}

PetriNet discover_petri_net_xes_frame(const EventDataView& data,
                                      const AlphaMinerOptions& given) {
  const bool polars = data.kind() == EventDataKind::kLazyEventTable;
  if (given.short_loops || (!polars && data.kind() != EventDataKind::kEventTable))
    return discover_petri_net(data, given);
  // The frame's own start column when no start key is given (round GC).
  auto options = given;
  if (options.start_timestamp_attribute.empty() && detail::xes_frame_has_column(data, "start_timestamp"))
    options.start_timestamp_attribute = "start_timestamp";
  const std::string activity = options.activity_attribute.empty()
                                   ? "concept:name" : options.activity_attribute;
  const std::string start = options.start_timestamp_attribute.empty()
                                ? "start_timestamp" : options.start_timestamp_attribute;
  detail::check_xes_frame_columns(data, detail::XesFrameFamily::kAlpha, options.case_id_attribute,
                                  activity, options.timestamp_attribute, start,
                                  !options.start_timestamp_attribute.empty());
  std::optional<EventTable> materialized;
  const EventTable* source = polars
      ? &materialized.emplace(data.lazy_table()->materialize(options.max_rows_to_materialize))
      : data.table();
  std::optional<EventTable> storage;
  const auto& table = detail::xes_frame_present_columns(*source, storage, polars);
  return discover_classic_table(table, options, polars, true);
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
