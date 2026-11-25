#pragma once

// The typed key columns of an XES upload's frame and the pandas operations
// pm4py's Alpha, temporal profile and Inductive miners apply to them (round
// GN, observed black-box against pm4py 2.7.22 on pandas 3.0.5).
//
// xes_frame_cells: each cell as read_xes's frame holds it (moved from the
// heuristics miner, rounds GK-GM).
//
// DataFrame.sort_values(keys) builds Categorical(column, ordered=True) for
// each key, the last key first: an object column's dict cell is unhashable
// ("TypeError: unhashable type: 'dict'"); its distinct values (None, NaN and
// NaT left out) sort as safe_sort sorts them, numbers, dates and strings
// each among themselves, and the non-strings before the strings when dates
// or numbers meet strings; numbers beside dates raise "'values' is not
// ordered, ...". The rows then sort stably by the codes, a gap last.
//
// The temporal profile (get_partial_order_dataframe) pairs each row with
// the later rows of its case (the rows without a case one case) in sorted
// order and compares the earlier row's timestamp with the later row's start
// (the timestamp again without a start column), then subtracts the two over
// the pairs the comparison kept and reads .dt of the difference. How each
// step behaves depends on the two columns' dtypes (xes_pandas_tp).

#include "business_hours.hpp"
#include "heuristic_plus_frame_intervals.hpp"
#include "minera/event_log.hpp"
#include "minera/timestamp.hpp"
#include "pandas_to_datetime.hpp"
#include "polars_missing_column.hpp"
#include "polars_sort.hpp"
#include "python_scalar.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace minera::detail {

// A pandas object column's dict cell (an XES <list> with children, or a
// scalar with children): Python names it 'dict' (round GL).
constexpr auto kPlusDictCell = static_cast<PlusCellKind>(64);

inline bool xes_frame_typed_nat(const EventAttribute& attribute) {
  const auto* date = std::get_if<NativeDate>(&attribute.typed_value);
  return !attribute.is_null && attribute.kind == AttributeKind::kDate && date != nullptr && date->value == "NaT";
}

// Python's repr of an XES value as read_xes's frame holds it (round GM): a
// <list> with children {'value': None, 'children': [('k', v), ...]}, a
// scalar with children {'value': v, 'children': {'k': v, ...}} (or pairs),
// a valueless string or id None; text, ints, floats and booleans as Python
// prints them. nullopt for other kinds (dates among the children).
inline std::optional<std::string> xes_frame_value_repr(const EventAttribute& value) {
  if (value.xes_public_shape == XesPublicShape::kNone) return std::string("None");
  const bool xes_list = value.xes_public_shape == XesPublicShape::kWrapper;
  const bool wrapped = xes_list || (value.container == AttributeContainerKind::kScalar &&
                                    (value.scalar_wrapper || value.scalar_children_list || !value.children.empty()));
  const auto scalar = [](const EventAttribute& cell) -> std::optional<std::string> {
    if (cell.is_null) return std::string("None");
    switch (cell.kind) {
      case AttributeKind::kString:
      case AttributeKind::kId: return frame_python_str_repr(cell.value);
      case AttributeKind::kInt:
        if (const auto* number = std::get_if<std::int64_t>(&cell.typed_value)) return std::to_string(*number);
        return std::nullopt;
      case AttributeKind::kFloat:
        if (const auto* number = std::get_if<double>(&cell.typed_value)) return python::float_repr(*number);
        return std::nullopt;
      case AttributeKind::kBoolean:
        if (const auto* truth = std::get_if<bool>(&cell.typed_value)) return std::string(*truth ? "True" : "False");
        return std::nullopt;
      default: return std::nullopt;
    }
  };
  if (!wrapped) {
    if (value.container != AttributeContainerKind::kScalar) return std::nullopt;
    return scalar(value);
  }
  std::string head = "None";
  if (!xes_list) {
    const auto own = scalar(value);
    if (!own) return std::nullopt;
    head = *own;
  }
  std::vector<std::pair<std::string, std::string>> children;
  const bool pairs = xes_list ? value.container == AttributeContainerKind::kList : value.scalar_children_list;
  for (const auto& child : value.children) {
    if (child.xes_public_shape == XesPublicShape::kAbsent) continue;
    const auto item = xes_frame_value_repr(child);
    if (!item) return std::nullopt;
    if (!pairs) {
      // Later duplicate keys replace earlier values, as in a dictionary.
      const auto existing = std::find_if(children.begin(), children.end(),
                                         [&](const auto& entry) { return entry.first == child.key; });
      if (existing != children.end()) {
        existing->second = *item;
        continue;
      }
    }
    children.emplace_back(child.key, *item);
  }
  std::string body;
  for (const auto& [key, item] : children) {
    if (!body.empty()) body += ", ";
    body += pairs ? "(" + frame_python_str_repr(key) + ", " + item + ")" : frame_python_str_repr(key) + ": " + item;
  }
  return "{'value': " + head + ", 'children': " + (pairs ? "[" + body + "]" : "{" + body + "}") + "}";
}

// An XES frame column's cells as Python reads them for the classic
// performance view (round GK; every XES kind since round GL, observed
// black-box): pandas types the column from its present cells, a gap being
// an event without the key (or a NaN float, or a typed NaT) and None a
// valueless string or id or a <list> without children:
//   - dates (and None): datetime64, each date a Timestamp, a gap or None NaT;
//   - text (and None): str, a gap or None NaN;
//   - ints and floats: int64 without a gap or None, else float64 (NaN);
//   - booleans: bool without a gap or None, else object;
//   - None alone: object (None), float64 (NaN) beside a gap;
//   - any other mix object: each cell its own type (a date a Python
//     datetime, a <list> with children or a scalar with children a dict),
//     None None, a gap NaN.
// Polars' copy holds Datetime, String, Int64, Float64 and Boolean columns
// as such and an object column as String (str() of each cell), a gap and
// None as None (an object column's cell str() of its value: True/False,
// Python's float repr, a dict's repr; round GN). nullopt for other kinds.
// How pandas types the column (round GM: `dtype`, when given).
enum class XesFrameDtype { kDatetime, kString, kInt, kFloat, kBool, kObject, kNoneObject, kNaNFloat };

//
// `big_ints` (round GX): ints past INT64_MAX (an XES uint64 cell) are typed
// as pandas types them: a column of such ints alone is uint64 (each an int),
// beside floats or gaps float64, beside a negative int an object column of
// ints; the cells keep the decimal text of each such int.
inline std::optional<std::vector<PlusCell>> xes_frame_cells(const EventTable& table, std::size_t column, bool polars,
                                                           XesFrameDtype* dtype_out = nullptr, bool big_ints = false) {
  enum class Cell { kGap, kNone, kDict, kDate, kText, kInt, kFloat, kBool };
  std::vector<Cell> kinds;
  kinds.reserve(table.rows.size());
  bool gap = false, none = false, dict = false, date = false, text = false, integer = false, real = false,
       boolean = false, above_int64 = false, negative_int = false;
  // pandas' inference stops checking the ints' range once it met a None
  // (not a NaN gap): an int past INT64_MAX and a negative one make the
  // column object only when both come before the first None (round HJ,
  // observed black-box: [2**63, None, -1] is float64, [2**63, nan, -1] and
  // [-1, 2**63, None] object).
  bool range_conflict = false, range_negative = false, range_above = false;
  for (const auto& row : table.rows) {
    Cell kind = Cell::kGap;
    if (column < row.size() && row[column] && row[column]->xes_public_shape != XesPublicShape::kAbsent) {
      const auto& cell = *row[column];
      const auto* number = std::get_if<double>(&cell.typed_value);
      if (cell.xes_public_shape == XesPublicShape::kNone || (cell.is_null && cell.container == AttributeContainerKind::kScalar &&
                                                             !cell.scalar_wrapper && cell.children.empty())) {
        kind = Cell::kNone;
      } else if (cell.xes_public_shape == XesPublicShape::kWrapper || cell.scalar_wrapper || cell.scalar_children_list ||
                 cell.container != AttributeContainerKind::kScalar || !cell.children.empty()) {
        kind = Cell::kDict;
      } else if (cell.is_null || xes_frame_typed_nat(cell) || (number && std::isnan(*number))) {
        kind = Cell::kGap;
      } else {
        switch (cell.kind) {
          case AttributeKind::kDate:
            if (!parse_timestamp_microseconds(cell.value)) return std::nullopt;
            kind = Cell::kDate;
            break;
          case AttributeKind::kString:
          case AttributeKind::kId: kind = Cell::kText; break;
          case AttributeKind::kInt:
            if (const auto* value = std::get_if<std::int64_t>(&cell.typed_value)) {
              negative_int = negative_int || *value < 0;
              if (!none) range_negative = range_negative || *value < 0;
            } else if (big_ints && std::holds_alternative<std::uint64_t>(cell.typed_value)) {
              above_int64 = true;
              if (!none) range_above = true;
            } else {
              return std::nullopt;
            }
            range_conflict = range_conflict || (range_negative && range_above);
            kind = Cell::kInt;
            break;
          case AttributeKind::kFloat:
            if (!number) return std::nullopt;
            kind = Cell::kFloat;
            break;
          case AttributeKind::kBoolean:
            if (!std::holds_alternative<bool>(cell.typed_value)) return std::nullopt;
            kind = Cell::kBool;
            break;
          default: return std::nullopt;
        }
      }
    }
    switch (kind) {
      case Cell::kGap: gap = true; break;
      case Cell::kNone: none = true; break;
      case Cell::kDict: dict = true; break;
      case Cell::kDate: date = true; break;
      case Cell::kText: text = true; break;
      case Cell::kInt: integer = true; break;
      case Cell::kFloat: real = true; break;
      case Cell::kBool: boolean = true; break;
    }
    kinds.push_back(kind);
  }
  using Dtype = XesFrameDtype;
  const int families = int(dict) + int(date) + int(text) + int(integer || real) + int(boolean);
  Dtype dtype = Dtype::kObject;
  if (families == 0) dtype = gap ? Dtype::kNaNFloat : Dtype::kNoneObject;
  else if (families > 1 || dict) dtype = Dtype::kObject;
  else if (date) dtype = Dtype::kDatetime;
  else if (text) dtype = Dtype::kString;
  else if (above_int64 && negative_int && (range_conflict || !none)) dtype = Dtype::kObject;
  else if (integer || real) dtype = real || gap || none ? Dtype::kFloat : Dtype::kInt;
  else dtype = gap || none ? Dtype::kObject : Dtype::kBool;
  if (dtype_out) *dtype_out = dtype;
  std::vector<PlusCell> cells;
  cells.reserve(table.rows.size());
  for (std::size_t r = 0; r < table.rows.size(); ++r) {
    PlusCell out;
    const auto kind = kinds[r];
    if (kind == Cell::kGap || kind == Cell::kNone) {
      if (polars) out.kind = PlusCellKind::kNone;
      else if (dtype == Dtype::kDatetime) out.kind = PlusCellKind::kNaT;
      else if (kind == Cell::kNone && (dtype == Dtype::kObject || dtype == Dtype::kNoneObject)) out.kind = PlusCellKind::kNone;
      else out.kind = PlusCellKind::kNaN;
      cells.push_back(std::move(out));
      continue;
    }
    const auto& cell = *table.rows[r][column];
    if (polars && dtype == Dtype::kObject) {
      // Polars' copy of an object column: str() of each cell.
      out.kind = PlusCellKind::kText;
      switch (kind) {
        case Cell::kDate: out.text = plus_python_datetime_str(cell.value); break;
        case Cell::kInt:
          if (const auto* natural = std::get_if<std::uint64_t>(&cell.typed_value)) out.text = std::to_string(*natural);
          else out.text = std::to_string(std::get<std::int64_t>(cell.typed_value));
          break;
        case Cell::kFloat: out.text = python::float_repr(std::get<double>(cell.typed_value)); break;
        case Cell::kBool: out.text = std::get<bool>(cell.typed_value) ? "True" : "False"; break;
        case Cell::kDict: {
          const auto repr = xes_frame_value_repr(cell);
          out.text = repr ? *repr : cell.value;
          break;
        }
        default: out.text = cell.value; break;
      }
      cells.push_back(std::move(out));
      continue;
    }
    switch (kind) {
      case Cell::kDict: out.kind = kPlusDictCell; break;
      case Cell::kDate:
        out.micros = *parse_timestamp_microseconds(cell.value);
        if (dtype == Dtype::kDatetime) {
          out.kind = polars ? PlusCellKind::kDatetime : PlusCellKind::kTimestamp;
        } else {
          out.kind = PlusCellKind::kDatetime;
          out.text = cell.value;
        }
        break;
      case Cell::kText:
        out.kind = PlusCellKind::kText;
        out.text = cell.value;
        break;
      case Cell::kInt: {
        const auto* natural = std::get_if<std::uint64_t>(&cell.typed_value);
        if (dtype == Dtype::kFloat) {
          out.kind = PlusCellKind::kFloat;
          out.number = natural ? static_cast<double>(*natural) : static_cast<double>(std::get<std::int64_t>(cell.typed_value));
        } else if (natural) {
          out.kind = PlusCellKind::kInt;
          out.text = std::to_string(*natural);
        } else {
          out.kind = PlusCellKind::kInt;
          out.micros = std::get<std::int64_t>(cell.typed_value);
        }
        break;
      }
      case Cell::kFloat:
        out.kind = PlusCellKind::kFloat;
        out.number = std::get<double>(cell.typed_value);
        break;
      case Cell::kBool:
        out.kind = PlusCellKind::kBool;
        out.micros = std::get<bool>(cell.typed_value) ? 1 : 0;
        break;
      default: break;
    }
    cells.push_back(std::move(out));
  }
  return cells;
}


struct XesTypedColumn {
  XesFrameDtype dtype = XesFrameDtype::kObject;
  std::vector<PlusCell> cells;
};

// The frame's column `name` typed as read_xes types it; nullopt when the
// table lacks it or holds kinds outside xes_frame_cells' model.
inline std::optional<XesTypedColumn> xes_typed_column(const EventTable& table, std::string_view name, bool polars = false,
                                                      bool big_ints = false) {
  const auto found = std::find(table.columns.begin(), table.columns.end(), name);
  if (found == table.columns.end()) return std::nullopt;
  XesTypedColumn column;
  auto cells = xes_frame_cells(table, static_cast<std::size_t>(found - table.columns.begin()), polars, &column.dtype,
                               big_ints);
  if (!cells) return std::nullopt;
  column.cells = std::move(*cells);
  return column;
}

inline bool xes_cell_null(const PlusCell& cell) {
  return cell.kind == PlusCellKind::kNaN || cell.kind == PlusCellKind::kNaT || cell.kind == PlusCellKind::kNone;
}

inline const char* xes_cell_type(const PlusCell& cell) {
  return cell.kind == kPlusDictCell ? "dict" : plus_cell_type(cell.kind);
}

// Python's comparable families: numbers, strings, dates (aware), and the
// rest (dicts, None) comparable with nothing.
enum class XesCellFamily { kNumber, kText, kDate, kOther };

inline XesCellFamily xes_cell_family(const PlusCell& cell) {
  switch (cell.kind) {
    case PlusCellKind::kInt:
    case PlusCellKind::kFloat:
    case PlusCellKind::kBool:
    case PlusCellKind::kNaN: return XesCellFamily::kNumber;
    case PlusCellKind::kText: return XesCellFamily::kText;
    case PlusCellKind::kTimestamp:
    case PlusCellKind::kDatetime: return XesCellFamily::kDate;
    default: return XesCellFamily::kOther;
  }
}

inline long double xes_cell_number(const PlusCell& cell) {
  if (cell.kind == PlusCellKind::kNaN) return std::numeric_limits<long double>::quiet_NaN();
  return plus_number(cell);
}

// -1, 0, 1 for two cells of one family (numbers by value, strings by their
// code points, dates by instant).
inline int xes_cell_compare(const PlusCell& a, const PlusCell& b) {
  switch (xes_cell_family(a)) {
    case XesCellFamily::kNumber: {
      // Two ints (an XES uint64 cell as decimal text, round GY) exactly.
      const auto integral = [](const PlusCell& cell) {
        return cell.kind == PlusCellKind::kInt || cell.kind == PlusCellKind::kBool;
      };
      if (integral(a) && integral(b) && (!a.text.empty() || !b.text.empty())) {
        const auto x = plus_int_text(a), y = plus_int_text(b);
        return plus_decimal_less(x, y) ? -1 : (plus_decimal_less(y, x) ? 1 : 0);
      }
      const auto x = xes_cell_number(a), y = xes_cell_number(b);
      return x < y ? -1 : (y < x ? 1 : 0);
    }
    case XesCellFamily::kText: return a.text < b.text ? -1 : (b.text < a.text ? 1 : 0);
    case XesCellFamily::kDate:
      if (a.micros != b.micros) return a.micros < b.micros ? -1 : 1;
      return a.nanos < b.nanos ? -1 : (b.nanos < a.nanos ? 1 : 0);
    default: return 0;
  }
}

// Python's `a op b` for op "<=" or ">=" on two cells (None or NaN only
// where the caller does not skip them): the result, or the TypeError.
inline bool xes_python_compare(const PlusCell& a, const PlusCell& b, std::string_view op) {
  const auto fa = xes_cell_family(a), fb = xes_cell_family(b);
  if (fa == fb && fa != XesCellFamily::kOther) {
    if (fa == XesCellFamily::kNumber && (a.kind == PlusCellKind::kNaN || b.kind == PlusCellKind::kNaN)) return false;
    const int by = xes_cell_compare(a, b);
    return op == "<=" ? by <= 0 : by >= 0;
  }
  throw std::invalid_argument("TypeError: '" + std::string(op) + "' not supported between instances of '" +
                              xes_cell_type(a) + "' and '" + xes_cell_type(b) + "'");
}

// Python's `a - b`: raises the TypeError for operands that do not subtract
// (numbers subtract, dates subtract).
inline void xes_python_subtract(const PlusCell& a, const PlusCell& b) {
  const auto fa = xes_cell_family(a), fb = xes_cell_family(b);
  if (fa == fb && (fa == XesCellFamily::kNumber || fa == XesCellFamily::kDate)) return;
  throw std::invalid_argument(std::string("TypeError: unsupported operand type(s) for -: '") + xes_cell_type(a) +
                              "' and '" + xes_cell_type(b) + "'");
}

inline constexpr const char* kXesNotOrdered =
    "TypeError: 'values' is not ordered, please explicitly specify the categories order by passing in a "
    "categories argument.";

// Categorical(column, ordered=True).codes: each row's rank among the
// column's distinct values in their sorted order, a gap ranked after them.
inline std::vector<std::size_t> xes_pandas_codes(const XesTypedColumn& column) {
  const bool object = column.dtype == XesFrameDtype::kObject || column.dtype == XesFrameDtype::kNoneObject;
  // Equal values in Python share a code: numbers by value (1, 1.0, True),
  // dates by instant.
  using Key = std::tuple<int, long double, std::string, std::int64_t, std::int32_t>;
  std::map<Key, std::size_t> slot;
  std::vector<std::size_t> uniques;  // first row of each distinct value
  std::vector<std::size_t> code(column.cells.size(), 0);
  std::vector<bool> gap(column.cells.size(), false);
  for (std::size_t row = 0; row < column.cells.size(); ++row) {
    const auto& cell = column.cells[row];
    if (xes_cell_null(cell)) {
      gap[row] = true;
      continue;
    }
    if (object && cell.kind == kPlusDictCell) throw std::invalid_argument("TypeError: unhashable type: 'dict'");
    const auto family = xes_cell_family(cell);
    Key key{static_cast<int>(family), 0.0L, std::string(), 0, 0};
    if (family == XesCellFamily::kNumber) {
      std::get<1>(key) = xes_cell_number(cell);
      // An int past int64 that no double holds equals no float: keyed by its
      // digits too, whatever the platform's long double (round GY).
      if (cell.kind == PlusCellKind::kInt && !cell.text.empty()) {
        const double as_double = std::strtod(cell.text.c_str(), nullptr);
        const bool exact = as_double < 18446744073709551616.0 &&
                           std::to_string(static_cast<std::uint64_t>(as_double)) == cell.text;
        if (!exact) std::get<2>(key) = cell.text;
      }
    } else if (family == XesCellFamily::kText) std::get<2>(key) = cell.text;
    else if (family == XesCellFamily::kDate) {
      std::get<3>(key) = cell.micros;
      std::get<4>(key) = cell.nanos;
    }
    const auto [it, inserted] = slot.try_emplace(key, uniques.size());
    if (inserted) uniques.push_back(row);
    code[row] = it->second;
  }
  std::vector<std::size_t> order(uniques.size());
  std::iota(order.begin(), order.end(), std::size_t{0});
  const auto by_value = [&](std::size_t a, std::size_t b) {
    return xes_cell_compare(column.cells[uniques[a]], column.cells[uniques[b]]) < 0;
  };
  bool mixed = false;
  for (const auto u : uniques)
    mixed = mixed || xes_cell_family(column.cells[u]) != xes_cell_family(column.cells[uniques.front()]);
  if (!mixed) {
    std::stable_sort(order.begin(), order.end(), by_value);
  } else {
    // safe_sort: the non-strings sorted (raising when two families meet),
    // then the strings.
    std::vector<std::size_t> others, strings;
    for (const auto at : order) (xes_cell_family(column.cells[uniques[at]]) == XesCellFamily::kText ? strings : others).push_back(at);
    for (const auto at : others)
      if (xes_cell_family(column.cells[uniques[at]]) != xes_cell_family(column.cells[uniques[others.front()]]))
        throw std::invalid_argument(kXesNotOrdered);
    std::stable_sort(others.begin(), others.end(), by_value);
    std::stable_sort(strings.begin(), strings.end(), by_value);
    order = std::move(others);
    order.insert(order.end(), strings.begin(), strings.end());
  }
  std::vector<std::size_t> rank(uniques.size());
  for (std::size_t at = 0; at < order.size(); ++at) rank[order[at]] = at;
  std::vector<std::size_t> codes(column.cells.size());
  for (std::size_t row = 0; row < codes.size(); ++row) codes[row] = gap[row] ? uniques.size() : rank[code[row]];
  return codes;
}

// DataFrame.sort_values(keys): the codes of each key, the last key first
// (its errors first), and the rows stably sorted by them.
inline std::vector<std::size_t> xes_pandas_sort_order(const std::vector<const XesTypedColumn*>& keys) {
  std::vector<std::vector<std::size_t>> codes(keys.size());
  for (std::size_t at = keys.size(); at-- > 0;) codes[at] = xes_pandas_codes(*keys[at]);
  const std::size_t rows = keys.empty() ? 0 : keys.front()->cells.size();
  std::vector<std::size_t> order(rows);
  std::iota(order.begin(), order.end(), std::size_t{0});
  std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
    for (const auto& key : codes)
      if (key[a] != key[b]) return key[a] < key[b];
    return false;
  });
  return order;
}

// A text as DatetimeArray._from_sequence reads it (on its own): its UTC
// microseconds, or nullopt when it does not read as a date.
inline std::optional<std::int64_t> xes_text_date_micros(const std::string& text) {
  try {
    const auto read = pandas_to_datetime::to_datetime(std::vector<std::string_view>{text});
    if (!read || read->values.empty() || !read->values.front()) return std::nullopt;
    const auto& v = *read->values.front();
    char buffer[48];
    std::snprintf(buffer, sizeof buffer, "%04d-%02d-%02dT%02d:%02d:%02d.%06u", v.year, v.month, v.day, v.hour, v.minute,
                  v.second, v.nanoseconds / 1000);
    auto micros = parse_timestamp_microseconds(buffer);
    if (micros && read->offset_minutes) *micros -= static_cast<std::int64_t>(*read->offset_minutes) * 60000000;
    return micros;
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

// Whether every text reads as a date, as DatetimeArray._from_sequence reads
// a string array (each on its own).
inline bool xes_texts_read_as_dates(const std::vector<const std::string*>& texts) {
  for (const auto* text : texts) {
    try {
      if (!pandas_to_datetime::to_datetime(std::vector<std::string_view>{*text}).has_value()) return false;
    } catch (const std::exception&) {
      return false;
    }
  }
  return true;
}

// The temporal profile's verdict on a frame whose columns read through
// (xes_pandas_tp): the dates path the profile's own discovery measures
// (both columns datetime64), no pair at all, or one pair measured.
struct XesPandasTpVerdict {
  enum class Kind { kDates, kEmpty, kSingle, kPairs };
  Kind kind = Kind::kDates;
  std::size_t earlier = 0, later = 0;  // rows of the single pair
  std::int64_t micros = 0;            // its later start minus earlier timestamp
  // kPairs (business hours, round GP): each kept pair (earlier row, later
  // row) with its BusinessHours seconds, in the pair frame's order.
  std::vector<std::tuple<std::size_t, std::size_t, double>> pairs;
};

// pandas' message when pm4py assigns DataFrame.apply's result over an empty
// pair frame to @@flow_time (business hours, round GP).
inline const char* const kPandasBusinessEmptyFlow =
    "ValueError: Cannot set a DataFrame with multiple columns to the single column @@flow_time";

// BusinessHours(a, b) in a DataFrame.apply row (round GP, observed
// black-box): each argument's replace(tzinfo=None), the first argument
// first (a str: "str.replace() takes no keyword arguments", any other
// non-date AttributeError), then the business seconds between the two wall
// clocks (a datetime64 column's Timestamp in UTC, an object column's
// datetime at its own offset).
inline double xes_business_cells(const PlusCell& a, const PlusCell& b, const std::vector<BusinessHourSlot>& slots) {
  const auto wall = [](const PlusCell& cell) -> std::int64_t {
    if (cell.kind == PlusCellKind::kTimestamp) return cell.micros;
    if (cell.kind == PlusCellKind::kDatetime) {
      if (!cell.text.empty())
        if (const auto local = local_wall_timestamp_microseconds(cell.text)) return *local;
      return cell.micros;
    }
    if (cell.kind == PlusCellKind::kText) throw std::invalid_argument("TypeError: str.replace() takes no keyword arguments");
    throw std::invalid_argument(std::string("AttributeError: '") + xes_cell_type(cell) + "' object has no attribute 'replace'");
  };
  const auto start = wall(a);
  const auto end = wall(b);
  return business_elapsed_seconds_microseconds(start, end, slots);
}

// pm4py's temporal profile on read_xes's frame up to the flow times (see the
// header comment): raises the error pandas raises, or returns the verdict.
// `start` is the start column (the timestamp column when the frame lacks
// one); `timestamp_dtype` names the timestamp column's dtype in pandas'
// messages.
// With `slots` (business hours) the kept pairs are measured with
// BusinessHours row by row instead of subtracted (kPairs; no pair kept
// raises kPandasBusinessEmptyFlow).
// With `kept_out` (pm4py's eventually-follows graph, which counts the pair
// frame's rows, round GW) the pairs the filter keeps on two dates columns
// are returned there (the kDates verdict); the other verdicts and errors
// are the profile's.
inline XesPandasTpVerdict xes_pandas_tp(const XesTypedColumn& cases, const XesTypedColumn& timestamps,
                                        const XesTypedColumn& start, const std::string& timestamp_dtype,
                                        const std::string& start_dtype,
                                        const std::vector<BusinessHourSlot>* slots = nullptr,
                                        std::vector<std::pair<std::size_t, std::size_t>>* kept_out = nullptr) {
  using D = XesFrameDtype;
  const auto order = xes_pandas_sort_order({&cases, &start, &timestamps});
  // Pairs (earlier, later) of a case in sorted order, earlier row first.
  const auto case_key = [&](std::size_t row) {
    const auto& cell = cases.cells[row];
    if (xes_cell_null(cell)) return std::string("\x1fnull");
    const auto family = xes_cell_family(cell);
    if (family == XesCellFamily::kText) return "s" + cell.text;
    if (family == XesCellFamily::kNumber) return "n" + std::to_string(static_cast<double>(xes_cell_number(cell)));
    return "d" + std::to_string(cell.micros);
  };
  std::map<std::string, std::vector<std::size_t>> groups;
  for (std::size_t at = 0; at < order.size(); ++at) groups[case_key(order[at])].push_back(at);
  std::vector<std::pair<std::size_t, std::size_t>> pairs;
  for (std::size_t at = 0; at < order.size(); ++at)
    for (const auto later : groups[case_key(order[at])])
      if (later > at) pairs.emplace_back(order[at], order[later]);
  const auto kind_of = [](D dtype) {
    switch (dtype) {
      case D::kDatetime: return 'd';
      case D::kString: return 's';
      case D::kInt:
      case D::kFloat:
      case D::kNaNFloat:
      case D::kBool: return 'n';
      default: return 'o';
    }
  };
  const char tk = kind_of(timestamps.dtype), sk = kind_of(start.dtype);
  const auto& L = timestamps.cells;
  const auto& R = start.cells;
  const auto both = [&](std::size_t i, std::size_t j) { return !xes_cell_null(L[i]) && !xes_cell_null(R[j]); };
  const auto date_of = [](const PlusCell& cell) -> std::optional<std::int64_t> {
    if (plus_date_like(cell.kind)) return cell.micros;
    if (cell.kind != PlusCellKind::kText) return std::nullopt;
    return xes_text_date_micros(cell.text);
  };
  // DatetimeArray._validate_listlike on an object array: dates (and gaps)
  // convert, and so do strings (and gaps) that all read as dates.
  const auto converts = [&](const std::vector<const PlusCell*>& cells) {
    bool dates = true, texts = true, any = false;
    std::vector<const std::string*> strings;
    for (const auto* cell : cells) {
      if (xes_cell_null(*cell)) continue;
      any = true;
      dates = dates && plus_date_like(cell->kind);
      texts = texts && cell->kind == PlusCellKind::kText;
      if (cell->kind == PlusCellKind::kText) strings.push_back(&cell->text);
    }
    if (any && dates) return true;
    return texts && xes_texts_read_as_dates(strings);
  };
  const auto later_cells = [&]() {
    std::vector<const PlusCell*> out;
    for (const auto& [i, j] : pairs) out.push_back(&R[j]);
    (void)L;
    return out;
  };
  const auto earlier_cells = [&]() {
    std::vector<const PlusCell*> out;
    for (const auto& [i, j] : pairs) out.push_back(&L[i]);
    return out;
  };
  std::vector<bool> keep(pairs.size(), false);
  // Python comparisons pair by pair; `earlier_first`: ts[i] op start[j],
  // else start[j] op ts[i]; `skip`: a gap on either side compares False.
  const auto elementwise = [&](bool earlier_first, std::string_view op, bool skip) {
    for (std::size_t p = 0; p < pairs.size(); ++p) {
      const auto [i, j] = pairs[p];
      if (skip && !both(i, j)) continue;
      keep[p] = earlier_first ? xes_python_compare(L[i], R[j], op) : xes_python_compare(R[j], L[i], op);
    }
  };
  const auto converted = [&](bool earlier) {
    for (std::size_t p = 0; p < pairs.size(); ++p) {
      const auto [i, j] = pairs[p];
      if (!both(i, j)) continue;
      const auto l = earlier ? date_of(L[i]) : std::optional<std::int64_t>(L[i].micros);
      const auto r = earlier ? std::optional<std::int64_t>(R[j].micros) : date_of(R[j]);
      keep[p] = l && r && *l <= *r;
    }
  };
  if (tk == 'd' && sk == 'd') {
    if (kept_out)
      for (const auto& [i, j] : pairs)
        if (both(i, j) && L[i].micros <= R[j].micros) kept_out->emplace_back(i, j);
    return {};
  }
  if (tk == 'd' && sk == 'n')
    throw std::invalid_argument("TypeError: Invalid comparison between dtype=" + timestamp_dtype + " and ndarray");
  if (tk == 'n' && sk == 'd')
    throw std::invalid_argument("TypeError: Invalid comparison between dtype=" + start_dtype + " and ndarray");
  if (tk == 'd' && sk == 's') {
    if (!converts(later_cells()))
      throw std::invalid_argument("TypeError: Invalid comparison between dtype=" + timestamp_dtype + " and StringArray");
    converted(false);
  } else if (tk == 'd' && sk == 'o') {
    if (converts(later_cells())) converted(false);
    else elementwise(true, "<=", true);
  } else if (tk == 'o' && sk == 'd') {
    if (converts(earlier_cells())) converted(true);
    else elementwise(false, ">=", true);
  } else if (tk == 's' && sk == 'd') {
    std::vector<const PlusCell*> valid;
    for (const auto& [i, j] : pairs)
      if (both(i, j)) valid.push_back(&L[i]);
    if (converts(valid)) converted(true);
    else elementwise(false, ">=", true);
  } else if ((tk == 'o' || tk == 'n') && sk == 's') {
    elementwise(false, ">=", true);
  } else if (tk == 'n' && sk == 'o') {
    elementwise(true, "<=", false);
  } else {
    elementwise(true, "<=", true);
  }
  std::vector<std::pair<std::size_t, std::size_t>> kept;
  for (std::size_t p = 0; p < pairs.size(); ++p)
    if (keep[p]) kept.push_back(pairs[p]);
  if (slots) {
    if (kept.empty()) throw std::invalid_argument(kPandasBusinessEmptyFlow);
    XesPandasTpVerdict verdict;
    verdict.kind = XesPandasTpVerdict::Kind::kPairs;
    for (const auto& [i, j] : kept) verdict.pairs.emplace_back(i, j, xes_business_cells(L[i], R[j], *slots));
    return verdict;
  }
  const auto no_accessor = [] {
    return std::invalid_argument("AttributeError: Can only use .dt accessor with datetimelike values");
  };
  if (tk == 'd') throw std::invalid_argument("TypeError: cannot subtract DatetimeArray from ndarray[object]");
  if (sk == 'd') {
    if (kept.empty()) throw no_accessor();
    if (kept.size() == 1) {
      const auto [i, j] = kept.front();
      if (!plus_date_like(L[i].kind))
        throw std::invalid_argument(std::string("TypeError: unsupported operand type(s) for -: 'DatetimeArray' and '") +
                                    xes_cell_type(L[i]) + "'");
      XesPandasTpVerdict verdict;
      verdict.kind = XesPandasTpVerdict::Kind::kSingle;
      verdict.earlier = i;
      verdict.later = j;
      verdict.micros = R[j].micros - L[i].micros;
      return verdict;
    }
    for (const auto& [i, j] : kept)
      if (!plus_date_like(L[i].kind))
        throw std::invalid_argument(std::string("TypeError: unsupported operand type(s) for -: 'Timestamp' and '") +
                                    xes_cell_type(L[i]) + "'");
    throw no_accessor();
  }
  if (timestamps.dtype == D::kBool && start.dtype == D::kBool)
    throw std::invalid_argument(
        "TypeError: numpy boolean subtract, the `-` operator, is not supported, use the bitwise_xor, the `^` operator, "
        "or the logical_xor function instead.");
  for (const auto& [i, j] : kept) xes_python_subtract(R[j], L[i]);
  throw no_accessor();
}

// Polars' order of one column's cells (the copy of read_xes's frame): a
// missing value first, text by its bytes, numbers, dates and booleans by
// value. -1, 0 or 1.
inline int xes_polars_cell_compare(const PlusCell& a, const PlusCell& b) {
  const bool an = xes_cell_null(a), bn = xes_cell_null(b);
  if (an || bn) return an == bn ? 0 : an ? -1 : 1;
  if (a.kind == PlusCellKind::kText || b.kind == PlusCellKind::kText) return a.text < b.text ? -1 : b.text < a.text ? 1 : 0;
  if (a.kind == PlusCellKind::kFloat || b.kind == PlusCellKind::kFloat) {
    const double x = a.kind == PlusCellKind::kFloat ? a.number : static_cast<double>(a.micros);
    const double y = b.kind == PlusCellKind::kFloat ? b.number : static_cast<double>(b.micros);
    return x < y ? -1 : y < x ? 1 : 0;
  }
  // A UInt64 column's ints past int64 hold their decimal text (round GY).
  if (!a.text.empty() || !b.text.empty()) {
    const auto x = plus_int_text(a), y = plus_int_text(b);
    return plus_decimal_less(x, y) ? -1 : (plus_decimal_less(y, x) ? 1 : 0);
  }
  return a.micros < b.micros ? -1 : b.micros < a.micros ? 1 : 0;
}

// The row order of LazyFrame.sort(keys) on the Polars copy (columns typed
// with xes_typed_column(..., true)): Polars' unstable multi-column sort.
inline std::vector<std::size_t> xes_polars_sort_order(const std::vector<const XesTypedColumn*>& keys) {
  std::vector<std::size_t> order(keys.empty() ? 0 : keys.front()->cells.size());
  std::iota(order.begin(), order.end(), std::size_t{0});
  polars_sort::sort_unstable(order, [&](std::size_t l, std::size_t r) {
    for (const auto* key : keys)
      if (const int by = xes_polars_cell_compare(key->cells[l], key->cells[r])) return by < 0;
    return false;
  });
  return order;
}

// read_xes reads a dates column's NaN floats (and typed NaT cells) as NaT:
// the table with those cells missing, as the date paths read a gap; nullopt
// when no named dates column holds one.
inline std::optional<EventTable> xes_frame_date_gaps_missing(const EventTable& table,
                                                             const std::vector<std::string>& names) {
  std::optional<EventTable> out;
  for (const auto& name : names) {
    const auto found = std::find(table.columns.begin(), table.columns.end(), name);
    if (found == table.columns.end()) continue;
    const auto at = static_cast<std::size_t>(found - table.columns.begin());
    XesFrameDtype dtype{};
    const auto cells = xes_frame_cells(table, at, false, &dtype);
    if (!cells || dtype != XesFrameDtype::kDatetime) continue;
    for (std::size_t r = 0; r < cells->size(); ++r) {
      if ((*cells)[r].kind != PlusCellKind::kNaT) continue;
      const auto& source = (out ? *out : table).rows[r];
      if (at >= source.size() || !source[at] || source[at]->is_null) continue;
      if (!out) out = table;
      out->rows[r][at].reset();
    }
  }
  return out;
}

}  // namespace minera::detail
