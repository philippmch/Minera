#include "frame_times.hpp"

#include "frame_cells.hpp"
#include "minera/frame_keys.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <utility>

namespace minera::frame_times {

std::string timestamp_column(std::string_view key) {
  if (key.empty() || key == "timestamp") return "time:timestamp";
  return std::string(key);
}

std::string activity_column(std::string_view key) {
  if (key.empty() || key == "activity") return "concept:name";
  return std::string(key);
}

Frame::Frame(const EventLog& log)
    : frame_(dataframe_frame_event_log(log)), keys_(frame_keys(frame_, FrameKeyScope::kDataFrame)) {}

std::optional<DataFrameColumnType> Frame::dtype(const std::string& key) const {
  if (!has(key)) return std::nullopt;
  return frame_column_type(frame_, key);
}

std::optional<python::Scalar> Frame::cell(const Trace& trace, const Event& event, const std::string& key) const {
  auto value = frame_row_scalar(frame_, trace, event, key);
  if (value && python::is_missing(*value)) return std::nullopt;
  return value;
}

std::optional<ParsedExactTimestamp> Frame::date(const Trace& trace, const Event& event,
                                                const std::string& key) const {
  const auto value = cell(trace, event, key);
  if (!value || value->kind != python::Scalar::Kind::kDate) return std::nullopt;
  auto date = value->date;
  date.timezone_aware = true;
  date.offset_minutes = 0;
  return date;
}

std::string Frame::cell_type(const Trace& trace, const Event& event, const std::string& key) const {
  const auto value = cell(trace, event, key);
  return value ? python::type_name(*value) : std::string("float");
}

std::vector<CaseCells> case_cells(const Frame& frame, const std::string& key) {
  std::vector<std::pair<python::Scalar, CaseCells>> cases;
  for (const auto& trace : frame.log().traces) {
    std::optional<python::Scalar> id, first, last;
    for (const auto& event : trace.events) {
      if (!id) id = frame.cell(trace, event, "case:concept:name");
      if (auto value = frame.cell(trace, event, key)) {
        if (!first) first = value;
        last = std::move(value);
      }
    }
    if (id && first) cases.push_back({std::move(*id), {std::move(*first), std::move(*last)}});
  }
  std::stable_sort(cases.begin(), cases.end(), [](const auto& a, const auto& b) {
    try {
      return python::compare(a.first, b.first) < 0;
    } catch (const python::TypeError&) {
      return python::str(a.first) < python::str(b.first);
    }
  });
  std::vector<CaseCells> cells;
  cells.reserve(cases.size());
  for (auto& entry : cases) cells.push_back(std::move(entry.second));
  return cells;
}

std::optional<std::string> subtraction_error(const python::Scalar& a, const python::Scalar& b, bool python_dates) {
  using Kind = python::Scalar::Kind;
  if (python::is_number(a) && python::is_number(b)) return std::nullopt;
  if (a.kind == Kind::kDate && b.kind == Kind::kDate) return std::nullopt;
  if (!python_dates && a.kind == Kind::kDate && b.kind == Kind::kInt)
    return "Addition/subtraction of integers and integer-arrays with Timestamp is no longer supported.  "
           "Instead of adding/subtracting `n`, use `n * obj.freq`";
  const auto name = [&](const python::Scalar& value) {
    return python_dates && value.kind == Kind::kDate ? std::string("datetime.datetime") : python::type_name(value);
  };
  return "unsupported operand type(s) for -: '" + name(a) + "' and '" + name(b) + "'";
}

namespace {

using Kind = python::Scalar::Kind;

std::string integer_message(const std::string& owner) {
  return "Addition/subtraction of integers and integer-arrays with " + owner +
         " is no longer supported.  Instead of adding/subtracting `n`, use `n * obj.freq`";
}

std::string unsupported(const std::string& left, const std::string& right) {
  return "unsupported operand type(s) for -: '" + left + "' and '" + right + "'";
}

// A cell's Python type in the subtraction: a date column's missing cell is
// NaT, any other missing cell NaN (a float).
std::string operand_name(const std::optional<python::Scalar>& cell, bool date_column) {
  if (date_column) return cell ? "Timestamp" : "NaTType";
  if (!cell) return "float";
  if (cell->kind == Kind::kDate) return "datetime.datetime";
  return python::type_name(*cell);
}

// Python's `a - b` on two cells (only the left may come from a date column).
std::optional<std::string> cell_subtraction(const std::optional<python::Scalar>& a, bool a_dates,
                                            const std::optional<python::Scalar>& b) {
  const bool a_number = !a_dates && (!a || python::is_number(*a));
  const bool b_number = !b || python::is_number(*b);
  if (a_number && b_number) return std::nullopt;
  if (a_dates && b && b->kind == Kind::kInt) {
    if (!a) return std::nullopt;  // NaT - n is NaT
    return integer_message("Timestamp");
  }
  const bool a_time = a_dates || (a && a->kind == Kind::kDate);
  if (a_time && b && b->kind == Kind::kDate) return std::nullopt;
  return unsupported(operand_name(a, a_dates), operand_name(b, false));
}

}  // namespace

ColumnSubtraction column_subtraction(std::optional<DataFrameColumnType> left_type,
                                     const std::vector<std::optional<python::Scalar>>& left,
                                     std::optional<DataFrameColumnType> right_type,
                                     const std::vector<std::optional<python::Scalar>>& right) {
  using Type = DataFrameColumnType;
  const bool left_dates = left_type == Type::kDate;
  if (left_dates && right_type == Type::kDate) return {std::nullopt, true};
  if (right_type == Type::kDate) {
    const std::string array = left_type == Type::kFloat64 ? "float64"
                              : left_type == Type::kInt64 ? "int64"
                              : left_type == Type::kBool  ? "bool"
                                                          : "object";
    return {"cannot subtract DatetimeArray from ndarray[" + array + "]"};
  }
  const auto rows = std::min(left.size(), right.size());
  std::vector<std::size_t> present;
  for (std::size_t i = 0; i < rows; ++i)
    if (left[i] && right[i]) present.push_back(i);
  if (left_type == Type::kString || right_type == Type::kString) {
    if (present.empty()) return {};
    if (left_dates && present.size() == 1)
      return {unsupported("DatetimeArray", operand_name(right[present.front()], false))};
    for (const auto i : present)
      if (auto error = cell_subtraction(left[i], left_dates, right[i])) return {std::move(error)};
    return {};
  }
  const auto numeric = [](std::optional<Type> type) {
    return type == Type::kInt64 || type == Type::kFloat64 || type == Type::kBool;
  };
  if (left_dates) {
    if (right_type == Type::kInt64) return {integer_message("DatetimeArray")};
    if (right_type == Type::kFloat64 || right_type == Type::kBool) {
      if (rows == 0) return {};
      return {unsupported(operand_name(left.front(), true), right_type == Type::kBool ? "bool" : "float")};
    }
    if (rows == 1) {
      const auto& cell = right.front();
      if (cell && cell->kind == Kind::kInt) return {integer_message("DatetimeArray")};
      if (cell_subtraction(left.front(), true, cell)) return {unsupported("DatetimeArray", operand_name(cell, false))};
      return {std::nullopt, true};
    }
    for (std::size_t i = 0; i < rows; ++i)
      if (auto error = cell_subtraction(left[i], true, right[i])) return {std::move(error)};
    return {};
  }
  if (numeric(left_type) && numeric(right_type)) {
    if (left_type == Type::kBool && right_type == Type::kBool)
      return {"numpy boolean subtract, the `-` operator, is not supported, use the bitwise_xor, the `^` operator, or "
              "the logical_xor function instead."};
    return {};
  }
  for (const auto i : present)
    if (auto error = cell_subtraction(left[i], false, right[i])) return {std::move(error)};
  return {};
}

std::string utc_text(const ParsedExactTimestamp& date) {
  auto utc = date;
  utc.timezone_aware = true;
  utc.offset_minutes = 0;
  return python::timestamp_str(utc);
}

double total_seconds(const ParsedExactTimestamp& start, const ParsedExactTimestamp& end) {
  const std::int64_t nanoseconds = (end.seconds - start.seconds) * 1000000000LL +
                                   (static_cast<std::int64_t>(end.nanoseconds) - start.nanoseconds);
  std::int64_t whole = nanoseconds / 1000000000LL;
  std::int64_t rest = nanoseconds % 1000000000LL;
  if (rest < 0) {
    rest += 1000000000LL;
    --whole;
  }
  return static_cast<double>(whole) + static_cast<double>(rest / 1000) * 1e-6 +
         static_cast<double>(rest % 1000) * 1e-9;
}

double epoch_seconds(const ParsedExactTimestamp& date) {
  if (date.nanoseconds % 1000 == 0) {
    const std::int64_t micros = date.seconds * 1000000LL + date.nanoseconds / 1000;
    return static_cast<double>(micros) / 1e6;
  }
  // round(value / 10**9, 6) of a nanosecond Timestamp.
  const double seconds = static_cast<double>(date.seconds) + static_cast<double>(date.nanoseconds) / 1e9;
  char buffer[64];
  std::snprintf(buffer, sizeof(buffer), "%.6f", seconds);
  return std::strtod(buffer, nullptr);
}

}  // namespace minera::frame_times
