#pragma once

// Heuristics++ on a CSV upload mined as a DataFrame or LazyFrame (round GI,
// observed black-box against pm4py 2.7.22). pm4py reads format_dataframe's
// frame, whose columns keep read_csv's dtypes (every date column a UTC
// datetime column), and raises before any interval is formed:
//
// - with a start_timestamp column, every event holds the start key, so
//   to_interval keeps the log as it is and the abstraction reads each start
//   as the frame's Python object (a date column Timestamp or NaT, int64 and
//   uint64 int, float64 float or NaN, bool bool, an object column of ints
//   int or NaN, text str or NaN; on Polars datetime.datetime, int, float,
//   bool, str or None), with the checks of an XES upload's frame
//   (heuristic_plus_frame_intervals.hpp);
// - without one, case by case in the frame's order, event by event, it
//   lower-cases each event's lifecycle:transition: a missing cell raises
//   AttributeError on float (NaN) or NoneType (Polars), a cell of a number,
//   bool or date column on its type.
//
// The checks raise pm4py's first error; when none applies the miner keeps
// its own path, whose results already follow pm4py's on these frames.
//
// Round GJ: a configured start key reads the same way (any event column,
// the resource and timestamp columns too; a `case:` key or one the frame
// lacks reads the lifecycle), and business hours measure the performance
// DFG's and the service times' spans with BusinessHours, which raises on a
// non-date start (replace) or a NaT.

#include "../heuristic_plus_frame_intervals.hpp"
#include "csv_frame_table.hpp"

#include "minera/active_event_source.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/dataframe_columns.hpp"
#include "minera/heuristic_miner.hpp"
#include "minera/timestamp.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace minera::tools {

// `log_shape`: the upload's log shape keyed by a typed or gapped column,
// whose EventLog variant raises the pandas frame's errors (round HD).
inline void check_plus_plus_csv_frame(const ActiveEventSource& source, const HeuristicMinerOptions& options,
                                      bool log_shape = false) {
  if (options.variant != HeuristicMinerVariant::kPlusPlus) return;
  // An explicitly empty start key is Minera's own fallback (no pm4py call).
  if (options.start_timestamp_attribute && options.start_timestamp_attribute->empty()) return;
  const auto kind = source.selected_kind();
  if (!log_shape && kind != EventDataKind::kEventTable && kind != EventDataKind::kLazyEventTable) return;
  const bool polars = !log_shape && kind == EventDataKind::kLazyEventTable;
  std::optional<EventTable> table;
  try {
    table = csv_frame_table(source, false);
  } catch (const std::exception&) {
    return;  // the miner reports the source's own error
  }
  if (!table || table->rows.empty()) return;
  const auto& log = source.checked_log();
  if (!log.dataframe_columns) return;
  const auto& frame = *log.dataframe_columns;
  const auto& selectors = source.selectors();
  const auto column_of = [&](const std::string& name) -> std::optional<std::size_t> {
    const auto found = std::find(table->columns.begin(), table->columns.end(), name);
    if (found == table->columns.end()) return std::nullopt;
    return static_cast<std::size_t>(found - table->columns.begin());
  };
  const auto case_at = column_of(selectors.case_id_key);
  const auto timestamp_at = column_of(selectors.timestamp_key);
  if (!case_at || !timestamp_at) return;
  const auto text = [&](const EventTable::Row& row, std::size_t at) -> const std::string* {
    if (at >= row.size() || !row[at] || row[at]->is_null || is_pandas_csv_na_text(row[at]->value)) return nullptr;
    return &row[at]->value;
  };
  // Cases in the frame's order (format_dataframe sorts them by first row),
  // rows in frame order.
  std::map<std::string, std::size_t> trace_of;
  std::vector<std::vector<std::size_t>> traces;
  for (std::size_t row = 0; row < table->rows.size(); ++row) {
    const auto* case_id = text(table->rows[row], *case_at);
    if (!case_id) return;  // format_dataframe drops such rows
    const auto [it, inserted] = trace_of.try_emplace(*case_id, traces.size());
    if (inserted) traces.emplace_back();
    traces[it->second].push_back(row);
  }
  // The start key (round GJ: a configured one too) names a column of
  // format_dataframe's frame: a CSV column under its header, or the copies
  // concept:name and time:timestamp of the activity and timestamp columns;
  // the event log moves the `case:` columns to the trace, so its events
  // never hold such a key.
  const std::string start_key = options.start_timestamp_attribute.value_or("start_timestamp");
  std::optional<std::size_t> start_at;
  if (!start_key.starts_with("case:")) {
    if (start_key == "time:timestamp")
      start_at = timestamp_at;
    else if (start_key == "concept:name")
      start_at = column_of(selectors.activity_key);
    else
      start_at = column_of(start_key);
  }
  // The activity column (round GK): its copy concept:name is a str column,
  // the column under its header keeps read_csv's dtype.
  std::optional<DataFrameColumn> activity_column;
  if (start_at && *start_at != *timestamp_at && table->columns[*start_at] == selectors.activity_key) {
    activity_column.emplace();
    activity_column->key = table->columns[*start_at];
    activity_column->type = start_key == "concept:name" ? DataFrameColumnType::kString : frame.activity_type;
  }
  if (start_at) {
    const auto column = activity_column ? activity_column : csv_frame_column(frame, table->columns[*start_at]);
    if (!column) return;
    std::vector<detail::PlusCell> starts, completes;
    starts.reserve(table->rows.size());
    completes.reserve(table->rows.size());
    for (const auto& row : table->rows) {
      detail::PlusCell start, complete;
      const auto* cell = text(row, *start_at);
      const auto missing = polars ? detail::PlusCellKind::kNone : detail::PlusCellKind::kNaN;
      switch (column->type) {
        case DataFrameColumnType::kDate: {
          if (!cell) {
            start.kind = polars ? detail::PlusCellKind::kNone : detail::PlusCellKind::kNaT;
            break;
          }
          const auto utc = csv_utc_date_cell(table->columns[*start_at], *cell);
          const auto micros = utc ? parse_timestamp_microseconds(utc->value) : std::nullopt;
          if (!micros) return;
          start.kind = polars ? detail::PlusCellKind::kDatetime : detail::PlusCellKind::kTimestamp;
          start.micros = *micros;
          break;
        }
        case DataFrameColumnType::kString:
          if (!cell) {
            start.kind = missing;
          } else {
            start.kind = detail::PlusCellKind::kText;
            start.text = *cell;
          }
          break;
        case DataFrameColumnType::kFloat64: {
          if (!cell) {
            start.kind = missing;
            break;
          }
          const auto typed = csv_dataframe_cell(*column, *cell);
          const auto* number = std::get_if<double>(&typed.typed_value);
          if (!number) return;
          start.kind = std::isnan(*number) ? missing : detail::PlusCellKind::kFloat;
          start.number = *number;
          break;
        }
        case DataFrameColumnType::kInt64:
        case DataFrameColumnType::kBool:
        case DataFrameColumnType::kObject: {
          // read_csv keeps a True/False column with a gap (in any data row)
          // as objects, as it does Python ints; Polars' copy of such a
          // column holds str() of each object, a gap null (round HC).
          const bool objects = column->type == DataFrameColumnType::kObject ||
                               (column->type == DataFrameColumnType::kBool && column->missing_cell);
          if (!cell) {
            // An object column with a gap holds NaN; the others have none.
            if (!objects) return;
            start.kind = polars ? detail::PlusCellKind::kNone : detail::PlusCellKind::kNaN;
            break;
          }
          const auto typed = csv_dataframe_cell(*column, *cell);
          if (objects && polars) {
            start.kind = detail::PlusCellKind::kText;
            start.text = python::str(python::from_attribute(typed));
            break;
          }
          if (const auto* value = std::get_if<std::int64_t>(&typed.typed_value)) {
            start.kind = detail::PlusCellKind::kInt;
            start.micros = *value;
          } else if (const auto* flag = std::get_if<bool>(&typed.typed_value)) {
            start.kind = detail::PlusCellKind::kBool;
            start.micros = *flag ? 1 : 0;
          } else if (std::holds_alternative<std::uint64_t>(typed.typed_value) || typed.kind == AttributeKind::kInt) {
            start.kind = detail::PlusCellKind::kInt;
            start.text = typed.value;
          } else {
            return;
          }
          break;
        }
      }
      const auto* time = text(row, *timestamp_at);
      const auto utc = time ? csv_utc_date_cell(selectors.timestamp_key, *time) : std::nullopt;
      const auto micros = utc ? parse_timestamp_microseconds(utc->value) : std::nullopt;
      if (!micros) return;
      complete.kind = polars ? detail::PlusCellKind::kDatetime : detail::PlusCellKind::kTimestamp;
      complete.micros = *micros;
      starts.push_back(std::move(start));
      completes.push_back(std::move(complete));
    }
    detail::check_plus_interval_cells(traces, starts, completes, options.use_business_hours);
    return;
  }
  const std::string lifecycle_key = "lifecycle:transition";
  const auto lifecycle_at = column_of(lifecycle_key);
  if (!lifecycle_at) return;
  const auto column = csv_frame_column(frame, lifecycle_key);
  if (!column) return;
  if (column->type == DataFrameColumnType::kObject && polars) return;
  const auto raise = [](const std::string& type) {
    throw HeuristicPandasError("builtins.AttributeError", "'" + type + "' object has no attribute 'lower'");
  };
  for (const auto& trace : traces)
    for (const auto row : trace) {
      const auto* cell = text(table->rows[row], *lifecycle_at);
      if (!cell) raise(polars ? "NoneType" : "float");
      switch (column->type) {
        case DataFrameColumnType::kString: break;
        case DataFrameColumnType::kInt64: raise("int");
        case DataFrameColumnType::kFloat64: raise("float");
        case DataFrameColumnType::kBool: raise("bool");
        case DataFrameColumnType::kDate: raise(polars ? "datetime.datetime" : "Timestamp");
        case DataFrameColumnType::kObject: {
          const auto typed = csv_dataframe_cell(*column, *cell);
          if (typed.kind == AttributeKind::kInt) raise("int");
          // A bool column with a gap is an object column of bools and NaN.
          std::string lower;
          for (const char ch : *cell) lower += static_cast<char>(ch >= 'A' && ch <= 'Z' ? ch - 'A' + 'a' : ch);
          if (lower == "true" || lower == "false") raise("bool");
          break;
        }
      }
    }
}

}  // namespace minera::tools
