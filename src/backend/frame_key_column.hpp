#pragma once

// A key column of an XES upload's frame as pandas and its Polars copy hold
// it (round GC, observed black-box against pm4py 2.7.22): read_xes types the
// column from its values (all dates datetime64; integers int64, or float64
// beside a gap; numbers float64; booleans bool, or object beside a gap; text
// str; any other mix object), and the Polars copy keeps the datetime and
// numeric dtypes and reads str and object columns as String, each value
// spelled as Python's str() spells it. Ranks order the rows as a sort by the
// column does: pandas' Categorical codes (safe_sort's order of the distinct
// values, the gaps last; a column it cannot order raises), Polars' own
// ordering (nulls first, String by its bytes).

#include "minera/event_log.hpp"
#include "python_scalar.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace minera::detail {

enum class FrameKeyDtype { kDatetime, kInt64, kFloat64, kBool, kStr, kObject };

struct FrameKeyColumn {
  FrameKeyDtype dtype = FrameKeyDtype::kObject;
  std::vector<python::Scalar> cells;  // a gap is None
};

inline bool frame_key_missing(const python::Scalar& cell) { return python::is_missing(cell); }

// The cell as the frame holds it: None for a gap (NaN, NaT, a null).
inline python::Scalar frame_key_cell(const EventAttribute* field) {
  if (!field || field->is_null) return python::none();
  if (const auto* date = std::get_if<NativeDate>(&field->typed_value); date && date->value == "NaT") return python::none();
  auto cell = python::from_attribute(*field);
  return python::is_missing(cell) ? python::none() : cell;
}

inline FrameKeyColumn frame_key_column(std::vector<python::Scalar> cells) {
  using Kind = python::Scalar::Kind;
  bool gap = false, date = false, integer = false, real = false, boolean = false, text = false;
  for (const auto& cell : cells) {
    if (frame_key_missing(cell)) gap = true;
    else if (cell.kind == Kind::kDate) date = true;
    else if (cell.kind == Kind::kInt) integer = true;
    else if (cell.kind == Kind::kFloat) real = true;
    else if (cell.kind == Kind::kBool) boolean = true;
    else text = true;
  }
  FrameKeyColumn column;
  column.cells = std::move(cells);
  const int kinds = date + (integer || real) + boolean + text;
  if (kinds != 1) column.dtype = FrameKeyDtype::kObject;
  else if (date) column.dtype = FrameKeyDtype::kDatetime;
  else if (integer || real) column.dtype = real || gap ? FrameKeyDtype::kFloat64 : FrameKeyDtype::kInt64;
  else if (boolean) column.dtype = gap ? FrameKeyDtype::kObject : FrameKeyDtype::kBool;
  else column.dtype = FrameKeyDtype::kStr;
  return column;
}

inline bool frame_key_polars_string(FrameKeyDtype dtype) {
  return dtype == FrameKeyDtype::kStr || dtype == FrameKeyDtype::kObject;
}

inline bool frame_key_numeric(FrameKeyDtype dtype) {
  return dtype == FrameKeyDtype::kInt64 || dtype == FrameKeyDtype::kFloat64 || dtype == FrameKeyDtype::kBool;
}

// pandas' sort_values ranks: the distinct values in safe_sort's order, the
// gaps after them; raises the TypeError Categorical raises when they do not
// order (a date beside a number).
inline std::vector<std::size_t> frame_key_pandas_ranks(const FrameKeyColumn& column) {
  std::vector<python::Scalar> uniques;
  std::map<std::string, std::size_t> slot;
  std::vector<std::size_t> code(column.cells.size(), 0);
  std::vector<bool> gap(column.cells.size(), false);
  for (std::size_t row = 0; row < column.cells.size(); ++row) {
    const auto& cell = column.cells[row];
    if (frame_key_missing(cell)) {
      gap[row] = true;
      continue;
    }
    const auto [it, inserted] = slot.try_emplace(python::identity(cell), uniques.size());
    if (inserted) uniques.push_back(cell);
    code[row] = it->second;
  }
  std::vector<std::size_t> order;
  try {
    order = python::safe_sort_order(uniques);
  } catch (const python::TypeError&) {
    throw std::invalid_argument(
        "TypeError: 'values' is not ordered, please explicitly specify the categories order by passing in a "
        "categories argument.");
  }
  std::vector<std::size_t> rank_of(uniques.size());
  for (std::size_t at = 0; at < order.size(); ++at) rank_of[order[at]] = at;
  std::vector<std::size_t> ranks(column.cells.size());
  for (std::size_t row = 0; row < ranks.size(); ++row) ranks[row] = gap[row] ? uniques.size() : rank_of[code[row]];
  return ranks;
}

// Polars' sort ranks over the copy's dtype: nulls first, then dates by
// instant, numbers by value, String by the bytes of each value's str().
inline std::vector<std::size_t> frame_key_polars_ranks(const FrameKeyColumn& column) {
  const bool as_text = frame_key_polars_string(column.dtype);
  std::vector<std::size_t> rows;
  for (std::size_t row = 0; row < column.cells.size(); ++row)
    if (!frame_key_missing(column.cells[row])) rows.push_back(row);
  std::vector<std::string> spelled(column.cells.size());
  if (as_text)
    for (const auto row : rows) spelled[row] = python::str(column.cells[row]);
  const auto order = [&](std::size_t a, std::size_t b) {
    return as_text ? spelled[a].compare(spelled[b]) : python::compare(column.cells[a], column.cells[b]);
  };
  std::stable_sort(rows.begin(), rows.end(), [&](std::size_t a, std::size_t b) { return order(a, b) < 0; });
  std::vector<std::size_t> ranks(column.cells.size(), 0);
  std::size_t rank = 1;
  for (std::size_t at = 0; at < rows.size(); ++at) {
    if (at > 0 && order(rows[at - 1], rows[at]) != 0) ++rank;
    ranks[rows[at]] = rank;
  }
  return ranks;
}

// Polars' name of the copy's dtype in a supertype error.
inline const char* frame_key_polars_dtype(FrameKeyDtype dtype) {
  switch (dtype) {
    case FrameKeyDtype::kDatetime: return "datetime[\xce\xbcs, UTC]";
    case FrameKeyDtype::kInt64: return "i64";
    case FrameKeyDtype::kFloat64: return "f64";
    case FrameKeyDtype::kBool: return "bool";
    case FrameKeyDtype::kStr:
    case FrameKeyDtype::kObject: return "str";
  }
  return "str";
}

}  // namespace minera::detail
