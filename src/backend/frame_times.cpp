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
