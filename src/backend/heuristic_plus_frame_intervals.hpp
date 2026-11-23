#pragma once

// Heuristics++ on the frame of an XES upload with a start_timestamp column
// (round GC, observed black-box against pm4py 2.7.22). Every converted event
// then holds the start key, so to_interval keeps the log as it is and the
// abstraction reads each event's start as the frame's own Python object:
// a pandas datetime column gives Timestamp or NaT, an object column (dates
// beside text) datetime.datetime, str or float NaN; a Polars datetime column
// gives datetime.datetime or None, a String column str or None. In order:
// the eventually-follows pass sorts each trace by its starts (CPython's
// list.sort) and compares each event's completion with the later starts
// (tc1 <= ts2, first following only); the performance DFG subtracts each
// event's start from its predecessor's completion (trace order); the
// service times call timestamp() on each start, then each completion. The
// first operation that raises decides the error.

#include "minera/event_log.hpp"
#include "minera/heuristic_miner.hpp"
#include "minera/timestamp.hpp"
#include "python_list_sort.hpp"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace minera::detail {

// kInt and kBool: a CSV start column of ints or bools (the value in micros;
// an int beyond int64, of a uint64 or object column, as its decimal text in
// `text`); kFloat: a float64 CSV start column's number (in `number`).
enum class PlusCellKind { kTimestamp, kNaT, kDatetime, kText, kNaN, kNone, kInt, kBool, kFloat };

struct PlusCell {
  PlusCellKind kind = PlusCellKind::kNone;
  std::int64_t micros = 0;
  std::string text;
  double number = 0.0;
  // A date's nanoseconds past `micros` (0-999): pandas keeps them, the
  // classic waits subtract them (round GK).
  std::int32_t nanos = 0;
};

// Python's `a < b` on two ints given as decimal text (an optional '-', no
// leading zeros).
inline bool plus_decimal_less(const std::string& a, const std::string& b) {
  const bool na = !a.empty() && a[0] == '-', nb = !b.empty() && b[0] == '-';
  if (na != nb) return na;
  const auto da = a.substr(na ? 1 : 0), db = b.substr(nb ? 1 : 0);
  const bool less = da.size() != db.size() ? da.size() < db.size() : da < db;
  return na ? (da != db && !less) : less;
}

inline std::string plus_int_text(const PlusCell& cell) {
  return cell.kind == PlusCellKind::kInt && !cell.text.empty() ? cell.text : std::to_string(cell.micros);
}

inline long double plus_number(const PlusCell& cell) {
  if (cell.kind == PlusCellKind::kFloat) return cell.number;
  if (cell.kind == PlusCellKind::kInt && !cell.text.empty()) return std::strtold(cell.text.c_str(), nullptr);
  return static_cast<long double>(cell.micros);
}

inline const char* plus_cell_type(PlusCellKind kind) {
  // A pandas object column's dict cell (xes_frame_typed.hpp's kPlusDictCell).
  if (static_cast<int>(kind) == 64) return "dict";
  switch (kind) {
    case PlusCellKind::kTimestamp: return "Timestamp";
    case PlusCellKind::kNaT: return "NaTType";
    case PlusCellKind::kDatetime: return "datetime.datetime";
    case PlusCellKind::kText: return "str";
    case PlusCellKind::kNaN: return "float";
    case PlusCellKind::kNone: return "NoneType";
    case PlusCellKind::kInt: return "int";
    case PlusCellKind::kBool: return "bool";
    case PlusCellKind::kFloat: return "float";
  }
  return "object";
}

// Python's str() of a datetime read from an XES date's text: "YYYY-MM-DD
// HH:MM:SS", the microseconds when there are any, and the UTC offset.
inline std::string plus_python_datetime_str(const std::string& iso) {
  if (iso.size() < 19) return iso;
  std::string out = iso.substr(0, 10) + ' ' + iso.substr(11, 8);
  std::size_t at = 19;
  std::string fraction;
  if (at < iso.size() && iso[at] == '.') {
    ++at;
    while (at < iso.size() && iso[at] >= '0' && iso[at] <= '9') fraction += iso[at++];
  }
  fraction = fraction.substr(0, 6);
  while (fraction.size() < 6) fraction += '0';
  if (fraction != "000000") out += '.' + fraction;
  if (at < iso.size()) out += (iso[at] == 'Z' || iso[at] == 'z') ? std::string("+00:00") : iso.substr(at);
  return out;
}

// The column's cells as Python reads them, or nullopt when the column holds
// other kinds (numbers, booleans, unparsable dates): not modelled.
inline std::optional<std::vector<PlusCell>> plus_frame_cells(const EventTable& table, std::size_t column,
                                                             bool polars) {
  const auto missing = [&](const auto& row) {
    if (column >= row.size() || !row[column] || row[column]->is_null) return true;
    const auto& cell = *row[column];
    if (const auto* number = std::get_if<double>(&cell.typed_value); number && std::isnan(*number)) return true;
    const auto* date = std::get_if<NativeDate>(&cell.typed_value);
    return cell.kind == AttributeKind::kDate && date != nullptr && date->value == "NaT";
  };
  bool all_dates = true;
  for (const auto& row : table.rows) {
    if (missing(row)) continue;
    const auto kind = row[column]->kind;
    if (kind == AttributeKind::kDate) continue;
    if (kind != AttributeKind::kString && kind != AttributeKind::kId) return std::nullopt;
    all_dates = false;
  }
  std::vector<PlusCell> cells;
  cells.reserve(table.rows.size());
  for (const auto& row : table.rows) {
    PlusCell cell;
    if (missing(row)) {
      cell.kind = polars ? PlusCellKind::kNone : all_dates ? PlusCellKind::kNaT : PlusCellKind::kNaN;
    } else if (row[column]->kind == AttributeKind::kDate) {
      const auto micros = parse_timestamp_microseconds(row[column]->value);
      if (!micros) return std::nullopt;
      cell.micros = *micros;
      if (polars && !all_dates) {
        cell.kind = PlusCellKind::kText;
        cell.text = plus_python_datetime_str(row[column]->value);
      } else {
        cell.kind = polars || !all_dates ? PlusCellKind::kDatetime : PlusCellKind::kTimestamp;
      }
    } else {
      cell.kind = PlusCellKind::kText;
      cell.text = row[column]->value;
    }
    cells.push_back(std::move(cell));
  }
  return cells;
}

inline bool plus_date_like(PlusCellKind kind) {
  return kind == PlusCellKind::kTimestamp || kind == PlusCellKind::kDatetime;
}

// Python's `a < b` (or `a <= b`): its result, or the TypeError it raises.
inline bool plus_compare(const PlusCell& a, const PlusCell& b, bool or_equal) {
  if (plus_date_like(a.kind) && plus_date_like(b.kind)) return or_equal ? a.micros <= b.micros : a.micros < b.micros;
  if ((a.kind == PlusCellKind::kNaT && (plus_date_like(b.kind) || b.kind == PlusCellKind::kNaT)) ||
      (b.kind == PlusCellKind::kNaT && plus_date_like(a.kind)))
    return false;
  if (a.kind == PlusCellKind::kText && b.kind == PlusCellKind::kText) return or_equal ? a.text <= b.text : a.text < b.text;
  if (a.kind == PlusCellKind::kNaN && b.kind == PlusCellKind::kNaN) return false;
  // Numbers (ints, bools, floats, NaN of an object column of ints) compare
  // by value; a comparison with NaN is false (round GI).
  const auto integral = [](PlusCellKind kind) { return kind == PlusCellKind::kInt || kind == PlusCellKind::kBool; };
  const auto numeric = [&](PlusCellKind kind) {
    return integral(kind) || kind == PlusCellKind::kNaN || kind == PlusCellKind::kFloat;
  };
  if (numeric(a.kind) && numeric(b.kind)) {
    if (a.kind == PlusCellKind::kNaN || b.kind == PlusCellKind::kNaN) return false;
    if (integral(a.kind) && integral(b.kind)) {
      if (a.text.empty() && b.text.empty()) return or_equal ? a.micros <= b.micros : a.micros < b.micros;
      const auto x = plus_int_text(a), y = plus_int_text(b);
      return or_equal ? !plus_decimal_less(y, x) : plus_decimal_less(x, y);
    }
    const auto x = plus_number(a), y = plus_number(b);
    return or_equal ? x <= y : x < y;
  }
  throw HeuristicPandasError("builtins.TypeError", std::string("'") + (or_equal ? "<=" : "<") +
                                                       "' not supported between instances of '" +
                                                       plus_cell_type(a.kind) + "' and '" + plus_cell_type(b.kind) + "'");
}

// pm4py's BusinessHours(d1, d2).get_seconds() (round GJ, observed
// black-box): replace(tzinfo=None) on d1, then on d2 (str raises TypeError,
// the other non-dates AttributeError), then a NaT on either side raises
// ValueError from timedelta(days=NaT.weekday()).
inline void check_plus_business_hours(const PlusCell& d1, const PlusCell& d2) {
  for (const auto* cell : {&d1, &d2}) {
    if (plus_date_like(cell->kind) || cell->kind == PlusCellKind::kNaT) continue;
    if (cell->kind == PlusCellKind::kText)
      throw HeuristicPandasError("builtins.TypeError", "str.replace() takes no keyword arguments");
    throw HeuristicPandasError("builtins.AttributeError",
                               std::string("'") + plus_cell_type(cell->kind) + "' object has no attribute 'replace'");
  }
  if (d1.kind == PlusCellKind::kNaT || d2.kind == PlusCellKind::kNaT)
    throw HeuristicPandasError("builtins.ValueError", "cannot convert float NaN to integer");
}

// Python's `(later - earlier).total_seconds()` on two cells (round GL's rules
// of the classic performance view, shared with Heuristics++ since round GO,
// observed black-box): two numbers subtract and their difference has no
// total_seconds (an int for ints and booleans, else a float); NaT beside an
// int (not a bool) is NaT; pandas' Timestamp minus an int raises its own
// TypeError; any other non-date (text, None, a dict) raises TypeError naming
// both types, the later first. Returns true when the difference is NaT.
inline bool plus_subtract_seconds(const PlusCell& later, const PlusCell& earlier) {
  const auto number = [](PlusCellKind kind) {
    return kind == PlusCellKind::kInt || kind == PlusCellKind::kBool || kind == PlusCellKind::kFloat ||
           kind == PlusCellKind::kNaN;
  };
  const auto real = [](PlusCellKind kind) { return kind == PlusCellKind::kFloat || kind == PlusCellKind::kNaN; };
  if (number(later.kind) && number(earlier.kind))
    throw HeuristicPandasError("builtins.AttributeError",
                               std::string("'") + (real(later.kind) || real(earlier.kind) ? "float" : "int") +
                                   "' object has no attribute 'total_seconds'");
  if ((later.kind == PlusCellKind::kNaT && earlier.kind == PlusCellKind::kInt) ||
      (later.kind == PlusCellKind::kInt && earlier.kind == PlusCellKind::kNaT))
    return true;
  if (later.kind == PlusCellKind::kTimestamp && earlier.kind == PlusCellKind::kInt)
    throw HeuristicPandasError("builtins.TypeError",
                               "Addition/subtraction of integers and integer-arrays with Timestamp is no longer "
                               "supported.  Instead of adding/subtracting `n`, use `n * obj.freq`");
  const auto subtractable = [](PlusCellKind kind) { return plus_date_like(kind) || kind == PlusCellKind::kNaT; };
  if (!subtractable(later.kind) || !subtractable(earlier.kind))
    throw HeuristicPandasError("builtins.TypeError", std::string("unsupported operand type(s) for -: '") +
                                                         plus_cell_type(later.kind) + "' and '" +
                                                         plus_cell_type(earlier.kind) + "'");
  return later.kind == PlusCellKind::kNaT || earlier.kind == PlusCellKind::kNaT;
}

// The checks on traces of cells (indices into `start` and `complete`, each
// trace in its events' order): raises pm4py's first error. With business
// hours the performance DFG and the service times measure each span with
// BusinessHours (round GJ) instead of subtracting and calling timestamp().
inline void check_plus_interval_cells(const std::vector<std::vector<std::size_t>>& traces,
                                      const std::vector<PlusCell>& start,
                                      const std::vector<PlusCell>& complete,
                                      bool business_hours = false) {
  for (const auto& trace : traces) {
    auto sorted = trace;
    python_list_sort(sorted, [&](std::size_t a, std::size_t b) { return plus_compare(start[a], start[b], false); });
    for (std::size_t i = 0; i < sorted.size(); ++i)
      for (std::size_t j = i + 1; j < sorted.size(); ++j)
        if (plus_compare(complete[sorted[i]], start[sorted[j]], true)) break;
  }
  if (business_hours) {
    for (const auto& trace : traces)
      for (std::size_t i = 1; i < trace.size(); ++i) check_plus_business_hours(complete[trace[i - 1]], start[trace[i]]);
    for (const auto& trace : traces)
      for (const auto row : trace) check_plus_business_hours(start[row], complete[row]);
    return;
  }
  for (const auto& trace : traces)
    for (std::size_t i = 1; i < trace.size(); ++i) (void)plus_subtract_seconds(start[trace[i]], complete[trace[i - 1]]);
  const auto stamp = [](const PlusCell& cell) {
    if (plus_date_like(cell.kind)) return;
    if (cell.kind == PlusCellKind::kNaT)
      throw HeuristicPandasError("builtins.ValueError", "NaTType does not support timestamp");
    throw HeuristicPandasError("builtins.AttributeError",
                               std::string("'") + plus_cell_type(cell.kind) + "' object has no attribute 'timestamp'");
  };
  for (const auto& trace : traces)
    for (const auto row : trace) {
      stamp(start[row]);
      stamp(complete[row]);
    }
}

// Raises the error pm4py's Heuristics++ abstraction raises on the frame's
// starts and completions; returns false when a column is not modelled (the
// caller keeps its older path), true when every check passes.
inline bool check_plus_frame_intervals(const EventTable& table, std::size_t case_at, std::size_t start_at,
                                       std::size_t timestamp_at, bool polars, bool business_hours = false) {
  const auto starts = plus_frame_cells(table, start_at, polars);
  auto completes = plus_frame_cells(table, timestamp_at, polars);
  if (!starts || !completes) return false;
  for (const auto& cell : *completes)
    if (cell.kind == PlusCellKind::kText || cell.kind == PlusCellKind::kNaN) return false;
  // Cases in order of their first row, rows in frame order; the rows
  // without a case id are one trace.
  std::map<std::optional<std::string>, std::size_t> trace_of;
  std::vector<std::vector<std::size_t>> traces;
  for (std::size_t row = 0; row < table.rows.size(); ++row) {
    const auto& cells = table.rows[row];
    const bool named = case_at < cells.size() && cells[case_at] && !cells[case_at]->is_null;
    const auto key = named ? std::optional<std::string>(cells[case_at]->value) : std::nullopt;
    const auto [it, inserted] = trace_of.try_emplace(key, traces.size());
    if (inserted) traces.emplace_back();
    traces[it->second].push_back(row);
  }
  check_plus_interval_cells(traces, *starts, *completes, business_hours);
  return true;
}

}  // namespace minera::detail
