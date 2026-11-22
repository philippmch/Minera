#pragma once

// The frame pm4py builds from an XES upload has a column only for the
// attributes some event (or trace) carries; Minera's table keeps the first
// class columns even when every cell is missing. The frame checks and the
// frame variants see the table without such columns (round FW, observed
// black-box: an upload whose traces are all unnamed fails the column count,
// or raises KeyError on the case column when other text columns make up the
// count; round FX: the footprints frame variants name the column missing).

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "minera/event_data.hpp"

namespace minera::detail {

// `table`, or `storage` holding it without the columns that have no value.
// With `read_order`, the columns also follow read_xes's order: the first
// appearance of each key over the events, an event's own attributes before
// its trace's case: attributes (Minera's table places case:concept:name at
// the first event even when that trace has no name; round FX, observed
// black-box in the Polars plan's DF line).
inline const EventTable& xes_frame_present_columns(const EventTable& table, std::optional<EventTable>& storage,
                                                   bool read_order = false) {
  std::vector<bool> keep(table.columns.size(), false);
  std::vector<std::size_t> order;
  const auto is_case = [&](std::size_t column) { return table.columns[column].starts_with("case:"); };
  for (const auto& row : table.rows)
    for (const bool case_columns : {false, true})
      for (std::size_t column = 0; column < row.size() && column < keep.size(); ++column)
        if (is_case(column) == case_columns && !keep[column] && row[column] && !row[column]->is_null) {
          keep[column] = true;
          order.push_back(column);
        }
  if (!read_order) {
    order.clear();
    for (std::size_t column = 0; column < keep.size(); ++column)
      if (keep[column]) order.push_back(column);
  }
  if (order.size() == table.columns.size() && std::is_sorted(order.begin(), order.end())) return table;
  EventTable& present = storage.emplace();
  present.attributes = table.attributes;
  present.metadata = table.metadata;
  for (const auto column : order) {
    present.columns.push_back(table.columns[column]);
    if (column < table.dtypes.size()) present.dtypes.push_back(table.dtypes[column]);
    if (column < table.categorical_dtypes.size()) present.categorical_dtypes.push_back(table.categorical_dtypes[column]);
  }
  if (present.dtypes.size() != present.columns.size()) present.dtypes.clear();
  if (present.categorical_dtypes.size() != present.columns.size()) present.categorical_dtypes.clear();
  present.rows.reserve(table.rows.size());
  for (const auto& row : table.rows) {
    auto& kept = present.rows.emplace_back();
    for (const auto column : order) kept.push_back(column < row.size() ? row[column] : std::nullopt);
  }
  return present;
}

// `table` without the columns `empty` (the columns of the source table that
// have no value) names, for a frame built from that table whose missing
// cells hold NaN.
inline void drop_xes_frame_columns(EventTable& table, const EventTable& source) {
  std::optional<EventTable> storage;
  const auto& present = xes_frame_present_columns(source, storage);
  if (!storage) return;
  for (std::size_t column = table.columns.size(); column-- > 0;) {
    if (std::find(present.columns.begin(), present.columns.end(), table.columns[column]) != present.columns.end() ||
        std::find(source.columns.begin(), source.columns.end(), table.columns[column]) == source.columns.end())
      continue;
    table.columns.erase(table.columns.begin() + static_cast<std::ptrdiff_t>(column));
    if (column < table.dtypes.size()) table.dtypes.erase(table.dtypes.begin() + static_cast<std::ptrdiff_t>(column));
    if (column < table.categorical_dtypes.size())
      table.categorical_dtypes.erase(table.categorical_dtypes.begin() + static_cast<std::ptrdiff_t>(column));
    for (auto& row : table.rows)
      if (column < row.size()) row.erase(row.begin() + static_cast<std::ptrdiff_t>(column));
  }
}

}  // namespace minera::detail
