#pragma once

// An activity column of an XES upload's frame as the Python objects pm4py's
// frame paths key their models by (round GZ, observed black-box against
// pm4py 2.7.22 on pandas 3.0.5 and Polars 1.44.1).
//
// pandas types the column as read_xes does (xes_frame_cells with ints past
// int64: uint64 alone, float64 beside floats or gaps, object beside a
// negative int): each cell an int, a float (a gap NaN), a bool, a Timestamp
// (a gap NaT), a str (a gap NaN) or, in an object column, each value its own
// type (a date a datetime.datetime, a valueless value None). Polars' copy
// holds Int64/UInt64, Float64, Boolean, Datetime and String columns (an
// object column str() of each cell), a gap None.
//
// The Python objects meet as dictionary and set keys: equal numbers are one
// key (0, False and 0.0; an int past 2**53 equals only the float that holds
// it exactly), the first object inserted names it.

#include "python_scalar.hpp"
#include "xes_frame_typed.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace minera::detail {

struct XesFrameActivityCell {
  python::Scalar value;  // the object (a missing cell: NaN, None or a NaT date)
  std::string type;      // Python type name
  bool missing = false;  // NaN, NaT or None: a key groupby drops
  bool nat = false;
};

struct XesFrameActivityColumn {
  XesFrameDtype dtype = XesFrameDtype::kObject;
  bool polars = false;
  std::vector<XesFrameActivityCell> cells;
  // Whether any present cell is no text (the frame's own text path reads
  // the rest).
  bool typed = false;
};

// str() of the object.
inline std::string xes_frame_activity_label(const XesFrameActivityCell& cell) {
  if (cell.nat) return "NaT";
  if (cell.value.kind == python::Scalar::Kind::kDate) return python::timestamp_str(cell.value.date);
  return python::str(cell.value);
}

// Equal in Python <=> equal key. Missing values: every NaN of pandas' gap
// is numpy's one NaN object, NaT one object, None one object.
inline std::string xes_frame_activity_identity(const XesFrameActivityCell& cell) {
  if (cell.nat) return "NaT";
  return python::identity(cell.value);
}

// The cell as pandas' shift() leaves it: an int64 or uint64 column becomes
// float64 (its last row NaN).
inline XesFrameActivityCell xes_frame_activity_shifted(const XesFrameActivityColumn& column, const XesFrameActivityCell& cell) {
  if (column.polars || column.dtype != XesFrameDtype::kInt || cell.value.kind != python::Scalar::Kind::kInt) return cell;
  XesFrameActivityCell out = cell;
  out.value = python::real(std::strtod(python::str(cell.value).c_str(), nullptr));
  out.type = "float";
  return out;
}

// nullopt when the table lacks the column or holds kinds outside the model
// (a dict cell, an unparsed date).
inline std::optional<XesFrameActivityColumn> xes_frame_activity_column(const EventTable& table, const std::string& name,
                                                                       bool polars) {
  const auto found = std::find(table.columns.begin(), table.columns.end(), name);
  if (found == table.columns.end()) return std::nullopt;
  const auto at = static_cast<std::size_t>(found - table.columns.begin());
  XesFrameActivityColumn column;
  column.polars = polars;
  const auto cells = xes_frame_cells(table, at, polars, &column.dtype, true);
  if (!cells) return std::nullopt;
  column.cells.reserve(cells->size());
  for (std::size_t r = 0; r < cells->size(); ++r) {
    const auto& cell = (*cells)[r];
    XesFrameActivityCell out;
    switch (cell.kind) {
      case PlusCellKind::kText:
        out.value = python::text(cell.text);
        out.type = "str";
        break;
      case PlusCellKind::kNaN:
        out.value = python::real(std::numeric_limits<double>::quiet_NaN());
        out.type = "float";
        out.missing = true;
        break;
      case PlusCellKind::kNone:
        out.value = python::none();
        out.type = "NoneType";
        out.missing = true;
        break;
      case PlusCellKind::kNaT:
        out.value = python::none();
        out.type = "NaTType";
        out.missing = out.nat = true;
        break;
      case PlusCellKind::kInt:
        if (!cell.text.empty()) {
          const auto parsed = python::integer_literal(cell.text);
          if (!parsed) return std::nullopt;
          out.value = *parsed;
        } else {
          out.value = python::integer(cell.micros);
        }
        out.type = "int";
        column.typed = true;
        break;
      case PlusCellKind::kFloat:
        out.value = python::real(cell.number);
        out.type = "float";
        column.typed = true;
        break;
      case PlusCellKind::kBool:
        out.value = python::boolean(cell.micros != 0);
        out.type = "bool";
        column.typed = true;
        break;
      case PlusCellKind::kTimestamp:
      case PlusCellKind::kDatetime: {
        const auto& raw = *table.rows[r][at];
        auto parsed = parse_timestamp_exact(raw.value);
        if (!parsed) return std::nullopt;
        // A datetime64 column holds UTC Timestamps (read_xes converts);
        // Polars' Datetime column too; an object column keeps each date's
        // own offset.
        if (cell.kind == PlusCellKind::kTimestamp || polars) {
          parsed->offset_minutes = 0;
          parsed->timezone_aware = true;
        }
        out.value.kind = python::Scalar::Kind::kDate;
        out.value.date = *parsed;
        out.value.text = raw.value;
        out.type = cell.kind == PlusCellKind::kTimestamp ? "Timestamp" : "datetime.datetime";
        column.typed = true;
        break;
      }
      default: return std::nullopt;
    }
    column.cells.push_back(std::move(out));
  }
  // Polars' copy of an object column holds str() of each value, which is
  // not always the XES text (True, a date's str(), a float's repr).
  if (column.dtype == XesFrameDtype::kObject) column.typed = true;
  return column;
}

// A TypeError of values from an object column, whose dates are
// datetime.datetime objects (python::Scalar names a date Timestamp).
inline std::string xes_frame_activity_type_error(const XesFrameActivityColumn& column, std::string message) {
  if (column.dtype != XesFrameDtype::kObject || column.polars) return message;
  const std::string from = "'Timestamp'", to = "'datetime.datetime'";
  for (std::size_t at = message.find(from); at != std::string::npos; at = message.find(from, at + to.size()))
    message.replace(at, from.size(), to);
  return message;
}

}  // namespace minera::detail
