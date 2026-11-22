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
                                                     const std::vector<std::string>& stack) {
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
  message += pad(level) + " WITH_COLUMNS:\n" + pad(level) + " [col(" + polars_frame_quoted(timestamp) + ").alias(" +
             polars_frame_quoted(start) + ")] \n";
  ++level;
  std::string frame = "DF [";
  for (std::size_t i = 0; i < frame_columns.size() && i < 4; ++i) frame += (i ? ", " : "") + polars_frame_quoted(frame_columns[i]);
  if (frame_columns.size() > 4) frame += ", ...";
  frame += "]; PROJECT */" + std::to_string(frame_columns.size()) + " COLUMNS";
  message += pad(level) + frame + "\n\nThis error occurred with the following context stack:\n";
  for (std::size_t i = 0; i < stack.size(); ++i) message += "\t[" + std::to_string(i + 1) + "] '" + stack[i] + "'\n";
  return message;
}

// Raises the error pm4py's `family` raises on the frame of an XES upload
// (`data`, a pandas or Polars frame) that lacks the case, activity or
// timestamp column; does nothing when all three are there.
inline void check_xes_frame_columns(const EventDataView& data, XesFrameFamily family, const std::string& case_key,
                                    const std::string& activity_key, const std::string& timestamp_key,
                                    const std::string& start_key = "start_timestamp") {
  const bool polars = data.kind() == EventDataKind::kLazyEventTable;
  if (!polars && data.kind() != EventDataKind::kEventTable) return;
  std::optional<EventTable> materialized;
  const EventTable* source = polars ? &materialized.emplace(data.lazy_table()->materialize()) : data.table();
  std::optional<EventTable> storage;
  const auto& table = xes_frame_present_columns(*source, storage, polars);
  const auto has = [&](const std::string& name) {
    return std::find(table.columns.begin(), table.columns.end(), name) != table.columns.end();
  };
  if (has(case_key) && has(activity_key) && has(timestamp_key)) return;
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
