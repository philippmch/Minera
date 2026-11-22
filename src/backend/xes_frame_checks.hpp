#pragma once

// The errors pm4py's Alpha and temporal profile raise on the frame of an XES
// upload that lacks a column they read (round FZ, observed black-box).
//
// pandas reads the timestamp column first (KeyError naming it), then
// selects [case, activity, timestamp] (a KeyError naming the labels the
// frame lacks, case first). Polars adds the start column as an alias of the
// timestamp (with_columns, failing over the frame itself when the timestamp
// is missing), keeps the columns the algorithm reads in frame order, and
// fails at the first plan node that names a missing one: Alpha's group_by
// (after its sort by case, start and timestamp when the case column is
// there), the temporal profile's sort by case. The temporal profile with a
// case column but no activity column collects the sorted frame and fails in
// pandas, selecting the activity next to the pair frame's "_2" copy.

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "minera/event_data.hpp"
#include "polars_missing_column.hpp"
#include "xes_frame_columns.hpp"

namespace minera::detail {

enum class XesFrameFamily { kAlpha, kTemporalProfile };

// Polars' ColumnNotFoundError for `missing` at `failed_at` when the plan is
// [SORT BY sort_by] / SELECT selected / WITH_COLUMNS [timestamp as start] /
// DF frame_columns, with the context stack innermost first.
inline std::string polars_select_plan_missing_column(const std::string& missing,
                                                     const std::vector<std::string>& frame_columns,
                                                     const std::vector<std::string>& selected,
                                                     const std::string& timestamp, const std::string& start,
                                                     const std::vector<std::string>& sort_by,
                                                     std::string_view failed_at,
                                                     const std::vector<std::string>& stack,
                                                     bool aliased = true) {
  const auto cols = [](const std::vector<std::string>& names) {
    std::string out = "[";
    for (std::size_t i = 0; i < names.size(); ++i) out += (i ? ", col(" : "col(") + polars_frame_quoted(names[i]) + ")";
    return out + "]";
  };
  std::string message = "unable to find column " + polars_frame_quoted(missing) + "; valid columns: [";
  for (std::size_t i = 0; i < selected.size(); ++i) message += (i ? ", " : "") + polars_frame_quoted(selected[i]);
  message += "]";
  if (const auto hint = polars_column_hint(missing, selected)) message += "\n\nDid you mean " + polars_frame_quoted(*hint) + "?";
  message += "\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING '" + std::string(failed_at) + "' <---\n";
  std::size_t level = 0;
  const auto pad = [&](std::size_t at) { return std::string(2 * at, ' '); };
  if (!sort_by.empty()) {
    message += "SORT BY " + cols(sort_by) + "\n";
    ++level;
  }
  message += pad(level) + "SELECT " + cols(selected) + "\n";
  ++level;
  // A start column the frame holds is read as it is, without the alias.
  if (aliased) {
    message += pad(level) + " WITH_COLUMNS:\n" + pad(level) + " [col(" + polars_frame_quoted(timestamp) + ").alias(" +
               polars_frame_quoted(start) + ")] \n";
    ++level;
  }
  std::string frame = "DF [";
  for (std::size_t i = 0; i < frame_columns.size() && i < 4; ++i) frame += (i ? ", " : "") + polars_frame_quoted(frame_columns[i]);
  if (frame_columns.size() > 4) frame += ", ...";
  frame += "]; PROJECT */" + std::to_string(frame_columns.size()) + " COLUMNS";
  message += pad(level) + frame + "\n\nThis error occurred with the following context stack:\n";
  for (std::size_t i = 0; i < stack.size(); ++i) message += "\t[" + std::to_string(i + 1) + "] '" + stack[i] + "'\n";
  return message;
}

// The "DF [...]; PROJECT */n COLUMNS" node of a Polars plan over `columns`.
inline std::string polars_df_node(const std::vector<std::string>& columns) {
  std::string frame = "DF [";
  for (std::size_t i = 0; i < columns.size() && i < 4; ++i) frame += (i ? ", " : "") + polars_frame_quoted(columns[i]);
  if (columns.size() > 4) frame += ", ...";
  return frame + "]; PROJECT */" + std::to_string(columns.size()) + " COLUMNS";
}

inline std::string polars_plan_cols(const std::vector<std::string>& names) {
  std::string out = "[";
  for (std::size_t i = 0; i < names.size(); ++i) out += (i ? ", col(" : "col(") + polars_frame_quoted(names[i]) + ")";
  return out + "]";
}

inline bool xes_frame_text_dtype(std::string_view dtype) {
  return dtype == "str" || dtype == "object" || dtype == "string" || dtype == "String";
}

// Whether the frame's column `name` holds text: its dtype when the table has
// dtypes, else read from its values as read_xes types the column (text when
// every value is text; a session's frame table carries no dtypes).
inline bool xes_frame_text_column(const EventTable& table, std::string_view name) {
  for (std::size_t column = 0; column < table.columns.size(); ++column) {
    if (table.columns[column] != name) continue;
    if (column < table.dtypes.size()) return xes_frame_text_dtype(table.dtypes[column]);
    bool text = false;
    for (const auto& row : table.rows) {
      if (column >= row.size() || !row[column] || row[column]->is_null) continue;
      const auto& cell = *row[column];
      if (const auto* number = std::get_if<double>(&cell.typed_value); number && std::isnan(*number)) continue;
      if (cell.kind != AttributeKind::kString && cell.kind != AttributeKind::kId) return false;
      text = true;
    }
    return text;
  }
  return false;
}

// Raises the error pm4py's `family` raises on the frame of an XES upload
// (`data`, a pandas or Polars frame) that lacks the case, activity or
// timestamp column; does nothing when all three are there.
inline void check_xes_frame_columns(const EventDataView& data, XesFrameFamily family, const std::string& case_key,
                                    const std::string& activity_key, const std::string& timestamp_key,
                                    const std::string& start_key = "start_timestamp",
                                    bool explicit_start = false) {
  const bool polars = data.kind() == EventDataKind::kLazyEventTable;
  if (!polars && data.kind() != EventDataKind::kEventTable) return;
  std::optional<EventTable> materialized;
  const EventTable* source = polars ? &materialized.emplace(data.lazy_table()->materialize()) : data.table();
  std::optional<EventTable> storage;
  const auto& table = xes_frame_present_columns(*source, storage, polars);
  const auto has = [&](const std::string& name) {
    return std::find(table.columns.begin(), table.columns.end(), name) != table.columns.end();
  };
  // An explicit start key (round GB, observed black-box). A start column the
  // frame holds is read as it is: pandas selects [case, activity,
  // timestamp, start] (Alpha selects the start even when the frame lacks
  // it); Polars keeps the four columns in frame order without the alias,
  // sorts by case, start and timestamp, and fails on a missing column or, for
  // Alpha, on the start's "_2" copy when the start is the timestamp; the
  // temporal profile compares a text start with the timestamp.
  const bool start_held = explicit_start && has(start_key);
  if (explicit_start && !polars && (start_held || family == XesFrameFamily::kAlpha)) {
    // The start key that names the timestamp is selected once.
    std::vector<std::string> keys{case_key, activity_key, timestamp_key};
    if (start_key != timestamp_key) keys.push_back(start_key);
    const auto error = pandas_list_selection_key_error(keys, table.columns);
    if (!error.empty()) throw std::invalid_argument(error);
  }
  const auto dtype_of = [&](const std::string& name) -> std::string_view {
    for (std::size_t i = 0; i < table.columns.size() && i < table.dtypes.size(); ++i)
      if (table.columns[i] == name) return table.dtypes[i];
    return {};
  };
  const bool text_start = start_held && start_key != timestamp_key && xes_frame_text_column(table, start_key) &&
                          has(timestamp_key) && !xes_frame_text_column(table, timestamp_key);
  if (start_held && !polars && family == XesFrameFamily::kTemporalProfile && text_start) {
    // pandas compares each pair's timestamp with the later row's start: a
    // case (the rows without one as one case) with two starts compares a
    // datetime with text; else the empty comparison leaves an object column
    // the timestamps are subtracted from.
    const auto at = [&](const std::string& name) {
      return static_cast<std::size_t>(std::find(table.columns.begin(), table.columns.end(), name) - table.columns.begin());
    };
    const auto case_at = at(case_key), start_at = at(start_key);
    const auto present = [](const std::optional<EventAttribute>& cell) {
      if (!cell || cell->is_null) return false;
      if (const auto* number = std::get_if<double>(&cell->typed_value)) return !std::isnan(*number);
      return true;
    };
    std::map<std::optional<std::string>, std::size_t> starts;
    bool compared = false;
    for (const auto& row : table.rows) {
      if (start_at >= row.size() || !present(row[start_at])) continue;
      const auto key = case_at < row.size() && present(row[case_at]) ? std::optional<std::string>(row[case_at]->value)
                                                                     : std::nullopt;
      if (++starts[key] == 2) compared = true;
    }
    const auto dtype = dtype_of(timestamp_key);
    if (compared)
      throw std::invalid_argument("TypeError: Invalid comparison between dtype=" +
                                  std::string(dtype.empty() ? "datetime64[us, UTC]" : dtype) + " and StringArray");
    throw std::invalid_argument("TypeError: cannot subtract DatetimeArray from ndarray[object]");
  }
  if (start_held && polars) {
    std::vector<std::string> selected;
    for (const auto& column : table.columns)
      if (column == case_key || column == activity_key || column == timestamp_key || column == start_key)
        selected.push_back(column);
    std::vector<std::string> sort_by{case_key, start_key};
    if (has(timestamp_key)) sort_by.push_back(timestamp_key);
    if (family == XesFrameFamily::kAlpha) {
      if (!has(case_key))
        throw std::invalid_argument(polars_select_plan_missing_column(
            case_key, table.columns, selected, timestamp_key, start_key, {}, "group_by",
            {"sort", "with_columns", "filter", "filter", "group_by"}, false));
      if (!has(activity_key))
        throw std::invalid_argument(polars_select_plan_missing_column(
            activity_key, table.columns, selected, timestamp_key, start_key, sort_by, "group_by",
            {"with_columns", "filter", "filter", "group_by"}, false));
      if (start_key == timestamp_key)
        throw std::invalid_argument(
            "the name '" + timestamp_key + "_2' passed to `LazyFrame.with_columns` is duplicate\n\nIt's possible that "
            "multiple expressions are returning the same default column name. If this is the case, try renaming the "
            "columns with `.alias(\"new_name\")` to avoid duplicate column names.\n\nResolved plan until failure:\n\n"
            "\t---> FAILED HERE RESOLVING 'group_by' <---\nSORT BY " + polars_plan_cols(sort_by) + "\n  SELECT " +
            polars_plan_cols(selected) + "\n    " + polars_df_node(table.columns) +
            "\n\nThis error occurred with the following context stack:\n\t[1] 'with_columns'\n\t[2] 'filter'\n"
            "\t[3] 'filter'\n\t[4] 'group_by'\n");
      return;
    }
    if (!has(case_key))
      throw std::invalid_argument(polars_select_plan_missing_column(case_key, table.columns, selected, timestamp_key,
                                                                    start_key, {}, "sort", {"sort"}, false));
    if (!has(timestamp_key))
      throw std::invalid_argument(polars_select_plan_missing_column(timestamp_key, table.columns, selected, timestamp_key,
                                                                    start_key, {}, "sort", {"sort"}, false));
    if (text_start) {
      std::string side;
      for (const char* name : {"LEFT", "RIGHT"})
        side += "    " + std::string(name) + " PLAN ON: [col(" + polars_frame_quoted(case_key) +
                ")]\n      ROW INDEX name: @@index, offset: 0\n        SORT BY " + polars_plan_cols(sort_by) +
                "\n          SELECT " + polars_plan_cols(selected) + "\n            " + polars_df_node(table.columns) + "\n";
      throw std::invalid_argument(
          "cannot compare 'date/datetime/time' to a string value (create native python { 'date', 'datetime', 'time' } "
          "or compare to a temporal column)\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING "
          "'with_columns' <---\nFILTER col(" + polars_frame_quoted(timestamp_key) + ") <= col(" +
          polars_frame_quoted(start_key + "_2") + ")\nFROM\n  FILTER col(\"@@index\") < col(\"@@index_2\")\n  FROM\n"
          "    INNER JOIN:\n" + side + "    END INNER JOIN\n\nThis error occurred with the following context stack:\n"
          "\t[1] 'filter' failed\n\t[2] 'with_columns'\n");
    }
  }
  if (has(case_key) && has(activity_key) && has(timestamp_key)) return;
  if (start_held && !polars) return;
  if (!polars) {
    if (!has(timestamp_key)) throw std::invalid_argument("KeyError: " + frame_python_str_repr(timestamp_key));
    throw std::invalid_argument(pandas_list_selection_key_error({case_key, activity_key, timestamp_key}, table.columns));
  }
  if (!has(timestamp_key))
    throw std::invalid_argument(polars_df_missing_column_error(timestamp_key, table.columns, {"with_columns"}, "with_columns"));
  std::vector<std::string> selected;
  for (const auto& column : table.columns)
    if (column != start_key && (column == case_key || column == activity_key || column == timestamp_key))
      selected.push_back(column);
  selected.push_back(start_key);
  if (family == XesFrameFamily::kTemporalProfile) {
    if (!has(case_key))
      throw std::invalid_argument(polars_select_plan_missing_column(case_key, table.columns, selected, timestamp_key,
                                                                    start_key, {}, "sort", {"sort"}));
    const auto labels = "[" + frame_python_str_repr(activity_key) + ", " + frame_python_str_repr(activity_key + "_2") + "]";
    throw std::invalid_argument("KeyError: " + frame_python_str_repr(labels + " not in index"));
  }
  if (!has(case_key))
    throw std::invalid_argument(polars_select_plan_missing_column(
        case_key, table.columns, selected, timestamp_key, start_key, {}, "group_by",
        {"sort", "with_columns", "filter", "filter", "group_by"}));
  throw std::invalid_argument(polars_select_plan_missing_column(
      activity_key, table.columns, selected, timestamp_key, start_key, {case_key, start_key, timestamp_key}, "group_by",
      {"with_columns", "filter", "filter", "group_by"}));
}

}  // namespace minera::detail
