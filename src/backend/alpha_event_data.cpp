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
#include "xes_frame_typed.hpp"
#include "xes_frame_activity.hpp"
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
                                bool polars = false, bool xes_frame = false,
                                std::vector<std::string>* display = nullptr) {
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
    std::size_t index = 0;  // the frame row
  };
  std::vector<Row> rows;
  // Round GZ: an activity column of other values than text, as the Python
  // objects pandas or Polars holds (xes_frame_activity.hpp). Rows key their
  // activity by the object's identity; the net names each identity by its
  // first row's text.
  std::optional<detail::XesFrameActivityColumn> typed_activity;
  if (xes_frame) {
    typed_activity = detail::xes_frame_activity_column(table, activity, polars);
    if (typed_activity && (!typed_activity->typed || typed_activity->cells.size() != stream.events.size()))
      typed_activity.reset();
  }
  std::map<std::string, std::string> typed_name;  // identity -> the net's activity
  std::set<std::string> typed_names;
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
  // Round GN: pandas' sort of an XES upload's frame reads each key column
  // as read_xes types it (xes_frame_typed.hpp), whatever its values, and
  // raises the sort's own errors; no timestamp is parsed.
  std::optional<std::vector<std::size_t>> typed_order;
  // Polars sorts its copy of the frame (String, Int64, Float64, Boolean or
  // Datetime columns) with its unstable sort, a missing value first.
  if (xes_frame && reads_time && options.sort_caseid_required && (!polars || has_completion)) {
    const auto cases = detail::xes_typed_column(table, options.case_id_attribute, polars);
    const auto times = detail::xes_typed_column(table, options.timestamp_attribute, polars, true);
    const auto starts = has_start ? detail::xes_typed_column(table, start, polars, true) : times;
    if (cases && times && starts && cases->cells.size() == stream.events.size())
      typed_order = polars ? detail::xes_polars_sort_order({&*cases, &*starts, &*times})
                           : detail::xes_pandas_sort_order({&*cases, &*starts, &*times});
  }
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
    row.index = rows.size();
    row.activity = value(select_field(record, activity,
        FirstClassAttributeRole::kActivity, false, true));
    if (typed_activity) {
      const auto& cell = typed_activity->cells[row.index];
      if (cell.missing) {
        row.activity.reset();
      } else {
        const auto identity = detail::xes_frame_activity_identity(cell);
        auto [entry, added] = typed_name.try_emplace(identity);
        if (added) {
          // Two objects of one text (an int and a str "5") stay apart.
          std::string name = row.activity ? *row.activity : detail::xes_frame_activity_label(cell);
          if (!typed_names.insert(name).second) {
            name += "\x1f" + identity;
            typed_names.insert(name);
          }
          entry->second = std::move(name);
        }
        row.activity = identity;
      }
    }
    if (typed_order) {
      // The typed order decides; nothing to read.
    } else if (reads_time && explicit_frame_start) {
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
  if (!typed_order && ranked_start && reads_time && start_cells.size() == rows.size()) {
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
  if (typed_order) {
    std::vector<Row> sorted_rows;
    sorted_rows.reserve(rows.size());
    for (const auto index : *typed_order) sorted_rows.push_back(std::move(rows[index]));
    rows = std::move(sorted_rows);
  } else if (options.sort_caseid_required) {
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
  // Round GZ: the labels pm4py's net shows for a typed activity column
  // (identity -> str() of the first object its label set holds).
  std::map<std::string, std::string> shown;
  if (typed_activity && !polars) {
    // get_dfg_graph pairs each row with the next (shift(), which makes an
    // int64 or uint64 column float64) where the case ids match, and groups
    // the pairs by both activities (sorted, a missing one dropped); the
    // label set takes each pair's source, then its target.
    const auto& column = *typed_activity;
    std::vector<std::vector<python::Scalar>> keys;
    for (std::size_t index = 1; index < rows.size(); ++index) {
      const auto& previous = rows[index - 1];
      const auto& current = rows[index];
      if (!previous.case_id || previous.case_id != current.case_id) continue;
      keys.push_back({column.cells[previous.index].value,
                      detail::xes_frame_activity_shifted(column, column.cells[current.index]).value});
    }
    std::vector<python::Group> groups;
    try {
      groups = python::groupby(keys);
    } catch (const python::TypeError& error) {
      throw std::invalid_argument("TypeError: " + detail::xes_frame_activity_type_error(column, error.what()));
    }
    for (const auto& group : groups) {
      const auto source = python::identity(group.labels[0]);
      const auto target = python::identity(group.labels[1]);
      shown.try_emplace(source, python::str(group.labels[0]));
      shown.try_emplace(target, python::str(group.labels[1]));
      // A float target no int of the column equals (an int past 2**53)
      // is an activity of its own.
      auto [entry, added] = typed_name.try_emplace(target);
      if (added) {
        std::string name = python::str(group.labels[1]);
        if (!typed_names.insert(name).second) {
          name += "\x1f" + target;
          typed_names.insert(name);
        }
        entry->second = std::move(name);
      }
      graph[{typed_name.at(source), typed_name.at(target)}] += group.rows.size();
    }
  } else {
    for (std::size_t index = 1; index < rows.size(); ++index) {
      const auto& previous = rows[index - 1];
      const auto& current = rows[index];
      if (previous.case_id && previous.case_id == current.case_id &&
          previous.activity && current.activity) {
        if (!typed_activity) {
          ++graph[{*previous.activity, *current.activity}];
          continue;
        }
        // Polars keeps each column's dtype: the labels are str() of the
        // values.
        for (const auto* row : {&previous, &current})
          shown.try_emplace(*row->activity, detail::xes_frame_activity_label(typed_activity->cells[row->index]));
        ++graph[{typed_name.at(*previous.activity), typed_name.at(*current.activity)}];
      }
    }
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
  // A case without an activity starts and ends with pandas' GroupBy.first
  // and last of the column (round GZ, observed black-box): a dates column's
  // NaT and an object column's None are one activity of their own, while a
  // float64 column's NaN is a new object each time, so every such case adds
  // a "nan" start and a "nan" end activity.
  const auto typed_dtype = typed_activity ? typed_activity->dtype : detail::XesFrameDtype::kString;
  const bool float_bounds = typed_activity && !polars &&
                            (typed_dtype == detail::XesFrameDtype::kFloat || typed_dtype == detail::XesFrameDtype::kNaNFloat);
  const bool nat_bounds = typed_activity && !polars &&
                          (typed_dtype == detail::XesFrameDtype::kDatetime || typed_dtype == detail::XesFrameDtype::kObject ||
                           typed_dtype == detail::XesFrameDtype::kNoneObject || float_bounds);
  std::size_t float_bound_count = 0;
  std::map<std::string, std::string> first_label;  // identity -> its first row's object
  if (typed_activity)
    for (const auto& row : rows)
      if (row.activity)
        first_label.try_emplace(*row.activity, detail::xes_frame_activity_label(typed_activity->cells[row.index]));
  const auto bound = [&](const std::optional<std::string>& activity, std::map<std::string, std::uint64_t>& counts,
                         bool& null_bound) {
    if (!activity) {
      if (!nat_bounds) {
        null_bound = true;
        return;
      }
      const std::string label = typed_dtype == detail::XesFrameDtype::kDatetime ? "NaT" : float_bounds ? "nan" : "None";
      const std::string identity =
          float_bounds ? "\x1fnan#" + std::to_string(float_bound_count++) : "\x1f" + label;
      auto [entry, added] = typed_name.try_emplace(identity);
      if (added) {
        std::string name = label;
        if (!typed_names.insert(name).second) {
          name += identity;
          typed_names.insert(name);
        }
        entry->second = std::move(name);
      }
      shown.try_emplace(identity, label);
      ++counts[entry->second];
      return;
    }
    if (!typed_activity) {
      ++counts[*activity];
      return;
    }
    shown.try_emplace(*activity, first_label.at(*activity));
    ++counts[typed_name.at(*activity)];
  };
  for (const auto& [case_id, bounds] : frame_ends) {
    (void)case_id;
    bound(bounds.first, starts, null_start);
    bound(bounds.last, ends, null_end);
  }
  auto net = discover_petri_net_with_null_endpoints(graph, starts, ends, null_start, null_end, options);
  if (display && typed_activity) {
    std::map<std::string, std::string> by_name;
    for (const auto& [identity, name] : typed_name)
      if (const auto found = shown.find(identity); found != shown.end()) by_name.emplace(name, found->second);
    display->clear();
    for (const auto& name : net.activities()) {
      const auto found = by_name.find(name);
      display->push_back(found != by_name.end() ? found->second : name);
    }
  }
  return net;
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
                                      const AlphaMinerOptions& given,
                                      std::vector<std::string>* display) {
  const bool polars = data.kind() == EventDataKind::kLazyEventTable;
  if (display) display->clear();
  if (given.short_loops || (!polars && data.kind() != EventDataKind::kEventTable))
    return discover_petri_net(data, given);
  // The frame's own start column when no start key is given (round GC).
  auto options = given;
  const bool frame_start = detail::xes_frame_has_column(data, "start_timestamp");
  if (options.start_timestamp_attribute.empty() && frame_start)
    options.start_timestamp_attribute = "start_timestamp";
  // Round GZ (observed black-box): without a start key pandas adds the
  // frame's missing start_timestamp column as a copy of the timestamp
  // column, which the activity key start_timestamp then reads.
  std::optional<EventTable> with_start;
  if (!polars && given.start_timestamp_attribute.empty() && !frame_start &&
      options.activity_attribute == "start_timestamp" && detail::xes_frame_has_column(data, options.timestamp_attribute)) {
    std::optional<EventTable> present_storage;
    const auto& present = detail::xes_frame_present_columns(*data.table(), present_storage, false);
    with_start = present;
    const auto at = static_cast<std::size_t>(
        std::find(with_start->columns.begin(), with_start->columns.end(), options.timestamp_attribute) -
        with_start->columns.begin());
    with_start->columns.push_back("start_timestamp");
    if (!with_start->dtypes.empty())
      with_start->dtypes.push_back(at < with_start->dtypes.size() ? with_start->dtypes[at] : std::string());
    if (!with_start->categorical_dtypes.empty()) with_start->categorical_dtypes.push_back(std::nullopt);
    for (auto& row : with_start->rows) {
      std::optional<EventAttribute> copy;
      if (at < row.size() && row[at]) {
        copy = *row[at];
        copy->key = "start_timestamp";
        copy->first_class_role = FirstClassAttributeRole::kNone;
        copy->first_class_source_key.clear();
      }
      row.push_back(std::move(copy));
    }
    return discover_petri_net_xes_frame(EventDataView(*with_start), given, display);
  }
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
  return discover_classic_table(table, options, polars, true, display);
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
