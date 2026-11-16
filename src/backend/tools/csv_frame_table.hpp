#pragma once

// A CSV source's views as pm4py's format_dataframe frame (rounds BM-BO),
// shared by the server's session routes and the CLI.

#include "minera/active_event_source.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/dataframe_columns.hpp"
#include "minera/event_data.hpp"
#include "../python_scalar.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// format_dataframe adds case:concept:name, concept:name and time:timestamp
// as copies of the selected case, activity and timestamp columns, replacing
// a CSV column of such a name, so pm4py reads a CSV upload under those keys
// as the selected columns (round BO, observed black-box). The key that names
// that column in Minera's CSV views (its own header), else `key`. A selected
// column the CSV lacks (an optional one) replaces nothing: `columns`, when
// given, are the CSV's header.
inline std::string csv_frame_key(const minera::ActiveSourceSelectors& selectors,
                                 minera::FirstClassAttributeRole role, std::string key,
                                 const std::vector<std::string>* columns = nullptr) {
  const auto replace = [&](std::string_view standard, const std::string& selected) {
    if (key != standard || selected.empty() || selected == standard) return;
    if (columns && std::find(columns->begin(), columns->end(), selected) == columns->end()) return;
    key = selected;
  };
  switch (role) {
    case minera::FirstClassAttributeRole::kCaseId: replace("case:concept:name", selectors.case_id_key); break;
    case minera::FirstClassAttributeRole::kActivity: replace("concept:name", selectors.activity_key); break;
    case minera::FirstClassAttributeRole::kTimestamp: replace("time:timestamp", selectors.timestamp_key); break;
    default: break;
  }
  return key;
}

inline std::string csv_frame_key(const minera::ActiveEventSource& source,
                          minera::FirstClassAttributeRole role, std::string key) {
  if (source.file().format != "csv") return key;
  return csv_frame_key(source.selectors(), role, std::move(key), &source.original_table().columns);
}

// Whether `key` is a CSV column format_dataframe replaces by a copy (see
// csv_frame_key): the frame, its log and its stream do not hold its cells.
inline bool csv_frame_replaced_column(const minera::ActiveEventSource& source, std::string_view key) {
  using Role = minera::FirstClassAttributeRole;
  for (const auto& [standard, role] : {std::pair{"case:concept:name", Role::kCaseId},
                                       std::pair{"concept:name", Role::kActivity},
                                       std::pair{"time:timestamp", Role::kTimestamp}})
    if (key == standard) return csv_frame_key(source, role, std::string(key)) != key;
  return false;
}

// A CSV upload's table as format_dataframe's frame, which pm4py's DataFrame
// functions read (round BM, observed black-box): the rows without a case,
// activity or timestamp dropped, the others in the frame's order (cases by
// id, each case's rows by the upload timestamp, then file order). With
// `index_columns`, also format_dataframe's "@@index" and "@@case_index" (a
// CSV's own columns of those names are overwritten), which the DFG adapters
// read. nullopt for other sources, or when the source's rows and events do
// not pair up.
inline std::optional<minera::EventTable> csv_frame_table(const minera::ActiveEventSource& source,
                                                         bool index_columns = true) {
  if (source.file().format != "csv") return std::nullopt;
  const auto& log = source.checked_log();
  if (!log.dataframe_columns || !log.dataframe_columns->csv_text) return std::nullopt;
  const auto& frame = *log.dataframe_columns;
  const auto& table = source.checked_table();
  const auto& rows = source.representative_row_ordinals();
  if (rows.size() != log.traces.size()) return std::nullopt;
  minera::EventTable result;
  result.columns = table.columns;
  result.attributes = table.attributes;
  result.metadata = table.metadata;
  result.dtypes = table.dtypes;
  result.categorical_dtypes = table.categorical_dtypes;
  const auto column = [&](const char* name) {
    if (!index_columns) return std::size_t{0};
    const auto found = std::find(result.columns.begin(), result.columns.end(), name);
    if (found != result.columns.end()) return static_cast<std::size_t>(found - result.columns.begin());
    result.columns.push_back(name);
    if (!result.dtypes.empty()) result.dtypes.push_back("int64");
    if (!result.categorical_dtypes.empty()) result.categorical_dtypes.push_back(std::nullopt);
    return result.columns.size() - 1;
  };
  const auto index_column = column("@@index");
  const auto case_index_column = column("@@case_index");
  const auto cell = [](const char* key, std::int64_t value) {
    minera::EventAttribute attribute{key, std::to_string(value), minera::AttributeKind::kInt};
    attribute.typed_value = value;
    return attribute;
  };
  std::vector<std::pair<std::int64_t, std::size_t>> positions;  // (@@index, result row)
  for (std::size_t t = 0; t < log.traces.size(); ++t) {
    const auto& events = log.traces[t].events;
    if (rows[t].size() != events.size()) return std::nullopt;
    for (std::size_t e = 0; e < events.size(); ++e) {
      const auto order = events[e].input_order;
      if (!rows[t][e] || *rows[t][e] >= table.rows.size() || order >= frame.row_index.size() ||
          order >= frame.case_index.size())
        return std::nullopt;
      auto row = table.rows[*rows[t][e]];
      if (index_columns) {
        row.resize(result.columns.size());
        row[index_column] = cell("@@index", frame.row_index[order]);
        row[case_index_column] = cell("@@case_index", frame.case_index[order]);
      }
      positions.emplace_back(frame.row_index[order], result.rows.size());
      result.rows.push_back(std::move(row));
    }
  }
  // The frame's own row order.
  std::stable_sort(positions.begin(), positions.end(),
                   [](const auto& left, const auto& right) {return left.first < right.first;});
  std::vector<minera::EventTable::Row> ordered;
  ordered.reserve(positions.size());
  for (const auto& [index, row] : positions) ordered.push_back(std::move(result.rows[row]));
  result.rows = std::move(ordered);
  // format_dataframe converts the timestamp column, and every other column of
  // date texts, to UTC datetimes before it drops rows: datetime64[ns, UTC]
  // when any of the column's cells, those of the rows it drops too, has
  // digits below the microsecond, else datetime64[us, UTC] (round BN,
  // observed black-box; the import records it, round BO, since the upload's
  // tables lack the dropped rows). The DataFrame functions that read the unit
  // (the pandas batches, the heuristics miner's durations) see it; the cells
  // keep their CSV text.
  if (result.dtypes.size() == result.columns.size()) {
    for (const auto& date_column : frame.columns) {
      if (date_column.type != minera::DataFrameColumnType::kDate) continue;
      const auto found = std::find(result.columns.begin(), result.columns.end(), date_column.key);
      if (found == result.columns.end()) continue;
      const auto index = static_cast<std::size_t>(found - result.columns.begin());
      result.dtypes[index] = date_column.nanoseconds ? "datetime64[ns, UTC]" : "datetime64[us, UTC]";
    }
  }
  // A CSV column the frame's copies replace is not in the frame (round BO).
  for (std::size_t index = result.columns.size(); index-- > 0;) {
    if (!csv_frame_replaced_column(source, result.columns[index])) continue;
    result.columns.erase(result.columns.begin() + static_cast<std::ptrdiff_t>(index));
    if (index < result.dtypes.size()) result.dtypes.erase(result.dtypes.begin() + static_cast<std::ptrdiff_t>(index));
    if (index < result.categorical_dtypes.size())
      result.categorical_dtypes.erase(result.categorical_dtypes.begin() + static_cast<std::ptrdiff_t>(index));
    for (auto& row : result.rows)
      if (index < row.size()) row.erase(row.begin() + static_cast<std::ptrdiff_t>(index));
  }
  return result;
}

// A CSV frame's cells that read_csv reads as NaN (a blank cell or a pandas
// NA spelling; the table keeps them as text) marked null, as the DataFrame
// holds them (round BN, observed black-box: the DFG adapters without column
// reduction drop a row with any, the pandas batches drop a row without a
// resource and the Polars ones group it under a null resource).
inline void mark_csv_nan_cells(minera::EventTable& table) {
  for (auto& row : table.rows)
    for (auto& cell : row)
      if (cell && !cell->is_null && cell->kind == minera::AttributeKind::kString &&
          minera::is_pandas_csv_na_text(cell->value)) {
        cell->value.clear();
        cell->is_null = true;
      }
}

// A CSV frame's present cells as read_csv typed their columns (the table
// keeps the CSV text): a column of numbers holds ints, or floats when it has
// a gap or a decimal ("1" is 1.0 there), True/False bools; text and date
// columns keep their cells (round BO, observed black-box: the pandas and Polars
// batches group and name resources by these values, "1.0", and order
// numbers as numbers).
inline void type_csv_frame_cells(minera::EventTable& table, const minera::DataFrameColumns& frame,
                          const std::string& activity_key) {
  for (std::size_t index = 0; index < table.columns.size(); ++index) {
    const auto& key = table.columns[index];
    // The case and activity columns stay text: the routes select them for
    // the frame's text copies case:concept:name and concept:name.
    if (key == activity_key) continue;
    std::optional<minera::DataFrameColumn> column;
    for (const auto& candidate : frame.columns)
      if (candidate.key == key) column = candidate;
    if (!column || column->role == minera::FirstClassAttributeRole::kCaseId) continue;
    const char* dtype = nullptr;
    switch (column->type) {
      case minera::DataFrameColumnType::kInt64: dtype = "int64"; break;
      case minera::DataFrameColumnType::kFloat64: dtype = "float64"; break;
      case minera::DataFrameColumnType::kBool: dtype = "bool"; break;
      case minera::DataFrameColumnType::kObject: dtype = "object"; break;
      case minera::DataFrameColumnType::kString:
      case minera::DataFrameColumnType::kDate: break;
    }
    if (!dtype) continue;
    if (index < table.dtypes.size()) table.dtypes[index] = dtype;
    for (auto& row : table.rows)
      if (index < row.size() && row[index] && !row[index]->is_null)
        *row[index] = minera::csv_dataframe_cell(*column, row[index]->value);
  }
}

// The frame with format_dataframe's "@@index" and "@@case_index" columns,
// whose "@@index" the batches read as the default event id, its NaN cells
// null (round BN) and its cells typed (round BO); nullopt as csv_frame_table.
inline std::optional<minera::EventTable> indexed_csv_frame_table(const minera::ActiveEventSource& source) {
  auto table = csv_frame_table(source, true);
  if (!table) return table;
  mark_csv_nan_cells(*table);
  const auto& log = source.checked_log();
  if (log.dataframe_columns)
    type_csv_frame_cells(*table, *log.dataframe_columns, source.selectors().activity_key);
  return table;
}

// A CSV upload's table as the whole frame format_dataframe hands pm4py's
// DataFrame functions: read_csv's columns with their typed cells (the case
// and activity columns typed too, every date column UTC dates, NaN cells
// null), then the copies case:concept:name and concept:name (the str of the
// case and activity cells: an int case 007 is "7", a float activity 1 is
// "1.0") and time:timestamp, then "@@index" and "@@case_index"; rows in the
// frame's order (round CV, observed black-box: BOSE concept drift groups
// and sorts by these keys). A header column of a copy's name is replaced by
// the copy. nullopt as csv_frame_table.
inline std::optional<minera::EventTable> formatted_csv_frame_table(const minera::ActiveEventSource& source) {
  auto table = indexed_csv_frame_table(source);
  if (!table) return table;
  const auto& frame = *source.checked_log().dataframe_columns;
  const auto& selectors = source.selectors();
  const auto find = [&](std::string_view key) -> std::optional<std::size_t> {
    const auto found = std::find(table->columns.begin(), table->columns.end(), key);
    if (found == table->columns.end()) return std::nullopt;
    return static_cast<std::size_t>(found - table->columns.begin());
  };
  const bool dtypes = table->dtypes.size() == table->columns.size();
  // The case and activity columns as read_csv typed them, and the dates.
  for (std::size_t index = 0; index < table->columns.size(); ++index) {
    const auto& key = table->columns[index];
    std::optional<minera::DataFrameColumn> column;
    if (key == selectors.activity_key) {
      column = minera::DataFrameColumn{key, frame.activity_type};
    } else {
      for (const auto& candidate : frame.columns)
        if (candidate.key == key) column = candidate;
      if (!column) continue;
      if (column->role != minera::FirstClassAttributeRole::kCaseId &&
          column->type != minera::DataFrameColumnType::kDate) continue;
    }
    if (column->type == minera::DataFrameColumnType::kDate) {
      for (auto& row : table->rows) {
        if (index >= row.size() || !row[index] || row[index]->is_null) continue;
        if (row[index]->kind == minera::AttributeKind::kDate) continue;
        if (auto date = minera::csv_utc_date_cell(key, row[index]->value)) *row[index] = *date;
      }
      continue;
    }
    const char* dtype = nullptr;
    switch (column->type) {
      case minera::DataFrameColumnType::kInt64: dtype = "int64"; break;
      case minera::DataFrameColumnType::kFloat64: dtype = "float64"; break;
      case minera::DataFrameColumnType::kBool: dtype = "bool"; break;
      case minera::DataFrameColumnType::kObject: dtype = "object"; break;
      case minera::DataFrameColumnType::kString: case minera::DataFrameColumnType::kDate: break;
    }
    if (!dtype) continue;
    if (dtypes) table->dtypes[index] = dtype;
    for (auto& row : table->rows)
      if (index < row.size() && row[index] && !row[index]->is_null)
        *row[index] = minera::csv_dataframe_cell(*column, row[index]->value);
  }
  // read_csv's missing cells: NaN, a new float per cell in a float64 column
  // and one shared NaN object elsewhere (NaT in a date column stays null).
  for (std::size_t index = 0; index < table->columns.size(); ++index) {
    const auto& key = table->columns[index];
    bool date = false;
    for (const auto& candidate : frame.columns)
      if (candidate.key == key && candidate.type == minera::DataFrameColumnType::kDate) date = true;
    if (date || key == "@@index" || key == "@@case_index") continue;
    const bool float64 = dtypes && table->dtypes[index] == "float64";
    std::optional<minera::EventAttribute> shared;
    for (auto& row : table->rows) {
      if (index >= row.size() || (row[index] && !row[index]->is_null)) continue;
      if (!float64 && shared) { row[index] = shared; continue; }
      minera::EventAttribute nan{key, "nan", minera::AttributeKind::kFloat};
      nan.typed_value = std::numeric_limits<double>::quiet_NaN();
      if (!float64) shared = nan;
      row[index] = std::move(nan);
    }
  }
  // The copies, before "@@index".
  const auto insert_at = find("@@index").value_or(table->columns.size());
  std::size_t added = 0;
  const auto copy = [&](const char* name, const std::string& selected, bool text) {
    const auto source_column = find(selected);
    if (selected == name || !source_column) return;
    auto target = find(name);
    if (!target) {
      const auto at = insert_at + added++;
      table->columns.insert(table->columns.begin() + static_cast<std::ptrdiff_t>(at), name);
      if (dtypes) table->dtypes.insert(table->dtypes.begin() + static_cast<std::ptrdiff_t>(at), "string");
      if (!table->categorical_dtypes.empty())
        table->categorical_dtypes.insert(table->categorical_dtypes.begin() + static_cast<std::ptrdiff_t>(at), std::nullopt);
      for (auto& row : table->rows) row.insert(row.begin() + static_cast<std::ptrdiff_t>(std::min(at, row.size())), std::nullopt);
      target = at;
    }
    const auto from = *find(selected);
    if (dtypes) table->dtypes[*target] = text ? std::string("string") : table->dtypes[from];
    for (auto& row : table->rows) {
      if (from >= row.size() || *target >= row.size()) continue;
      auto cell = row[from];
      if (cell && !cell->is_null && text) {
        const auto value = minera::python::str(minera::python::from_attribute(*cell));
        cell = minera::EventAttribute{name, value, minera::AttributeKind::kString};
      } else if (cell) {
        cell->key = name;
      }
      row[*target] = std::move(cell);
    }
  };
  copy("case:concept:name", selectors.case_id_key, true);
  copy("concept:name", selectors.activity_key, true);
  copy("time:timestamp", selectors.timestamp_key, false);
  return table;
}


// An XES upload's table as read_xes's frame hands it to pm4py's DataFrame
// functions: an event or case: column's missing cells NaN (a new float per
// cell in a float64 column, one shared NaN object elsewhere; NaT in a date
// column stays null), a float64 column's ints floats (round CV, observed
// black-box: BOSE concept drift sorts and joins these cells). nullopt for
// other sources.
inline std::optional<minera::EventTable> formatted_xes_frame_table(const minera::ActiveEventSource& source) {
  if (source.file().format == "csv") return std::nullopt;
  const auto& log = source.checked_log();
  if (!log.dataframe_columns || log.dataframe_columns->csv_text) return std::nullopt;
  const auto& frame = *log.dataframe_columns;
  auto table = source.checked_table();
  const auto& selectors = source.selectors();
  for (std::size_t index = 0; index < table.columns.size(); ++index) {
    const auto& key = table.columns[index];
    std::optional<minera::DataFrameColumnType> type;
    for (const auto& candidate : frame.columns)
      if (candidate.key == key) type = candidate.type;
    if (!type && key.starts_with("case:"))
      for (const auto& candidate : frame.trace_columns)
        if ("case:" + candidate.key == key) type = candidate.type;
    if (!type) {
      // The selected timestamp and any column of dates hold NaT.
      if (key == selectors.timestamp_key) continue;
      bool dates = false, others = false;
      for (const auto& row : table.rows) {
        if (index >= row.size() || !row[index] || row[index]->is_null) continue;
        (row[index]->kind == minera::AttributeKind::kDate ? dates : others) = true;
      }
      if (dates && !others) continue;
    }
    if (type == minera::DataFrameColumnType::kDate) continue;
    const bool float64 = type == minera::DataFrameColumnType::kFloat64;
    std::optional<minera::EventAttribute> shared;
    for (auto& row : table.rows) {
      if (index >= row.size()) continue;
      auto& cell = row[index];
      if (cell && !cell->is_null) {
        if (float64) {
          double value = 0.0;
          if (const auto* integer = std::get_if<std::int64_t>(&cell->typed_value)) value = static_cast<double>(*integer);
          else if (const auto* natural = std::get_if<std::uint64_t>(&cell->typed_value)) value = static_cast<double>(*natural);
          else continue;
          minera::EventAttribute converted{key, minera::python::str(minera::python::real(value)), minera::AttributeKind::kFloat};
          converted.typed_value = value;
          cell = std::move(converted);
        }
        continue;
      }
      if (!float64 && shared) { cell = shared; continue; }
      minera::EventAttribute nan{key, "nan", minera::AttributeKind::kFloat};
      nan.typed_value = std::numeric_limits<double>::quiet_NaN();
      if (!float64) shared = nan;
      cell = std::move(nan);
    }
  }
  return table;
}
