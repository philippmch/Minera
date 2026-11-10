#pragma once

// Timestamp columns of an upload's frame (CSV and XES logs read on pm4py's
// DataFrame path, see dataframe_columns.hpp) as the reference's DataFrame
// time statistics read them: service time, cycle time, case arrival and case
// statistics. Their keys name frame columns, cells are the frame's typed
// values, a date column's missing cell is NaT (observed black-box).

#include "minera/dataframe_columns.hpp"
#include "minera/event_log.hpp"
#include "minera/timestamp.hpp"
#include "python_scalar.hpp"

#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace minera::frame_times {

// The frame column a statistics key names: the native aliases "" and
// "timestamp" are time:timestamp; any other key is a column name.
[[nodiscard]] std::string timestamp_column(std::string_view key);

// The frame column an activity key names: "" and "activity" are
// concept:name; any other key is a column name.
[[nodiscard]] std::string activity_column(std::string_view key);

// A view of one upload: the frame, its columns and their dtypes.
class Frame {
 public:
  explicit Frame(const EventLog& log);

  [[nodiscard]] const EventLog& log() const noexcept { return frame_; }
  [[nodiscard]] bool has(const std::string& key) const { return keys_.contains(key); }
  [[nodiscard]] const std::set<std::string>& keys() const noexcept { return keys_; }
  // nullopt for a column the frame lacks.
  [[nodiscard]] std::optional<DataFrameColumnType> dtype(const std::string& key) const;
  // The row's cell (nullopt: NaN, NaT, NA or None).
  [[nodiscard]] std::optional<python::Scalar> cell(const Trace& trace, const Event& event,
                                                   const std::string& key) const;
  // The row's date as a UTC instant, nullopt for NaT or a cell that is no date.
  [[nodiscard]] std::optional<ParsedExactTimestamp> date(const Trace& trace, const Event& event,
                                                         const std::string& key) const;
  // The Python type a row's cell has in a non-date column, as the reference's
  // per-value calls name it: a missing cell is NaN, a float.
  [[nodiscard]] std::string cell_type(const Trace& trace, const Event& event, const std::string& key) const;

 private:
  EventLog frame_;
  std::set<std::string> keys_;
};

// One case's present cells of a column, as groupby(case).first() and
// .last() give them (both skip missing cells).
struct CaseCells {
  python::Scalar first;
  python::Scalar last;
};

// The cases with a present cell in `key`, in the groupby's key order (case
// ids sorted; a case without an id is dropped).
[[nodiscard]] std::vector<CaseCells> case_cells(const Frame& frame, const std::string& key);

// Python's `a - b` on two object cells: nullopt when it gives a value, else
// the TypeError's message. An XES object column holds its dates as
// datetime.datetime, any other as pandas Timestamps.
[[nodiscard]] std::optional<std::string> subtraction_error(const python::Scalar& a, const python::Scalar& b,
                                                           bool python_dates = false);

// A UTC instant as text ("2020-01-01 08:00:00.250000+00:00"), which
// parse_timestamp_exact reads back to the same instant.
[[nodiscard]] std::string utc_text(const ParsedExactTimestamp& date);

// The seconds from `start` to `end` as pandas' Timedelta.total_seconds gives
// them: the whole seconds, plus the microseconds times 1e-6, plus the
// nanoseconds left times 1e-9.
[[nodiscard]] double total_seconds(const ParsedExactTimestamp& start, const ParsedExactTimestamp& end);

// Timestamp.timestamp(): the instant's seconds since the epoch as a float.
[[nodiscard]] double epoch_seconds(const ParsedExactTimestamp& date);

}  // namespace minera::frame_times
