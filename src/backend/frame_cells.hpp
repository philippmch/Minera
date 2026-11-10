#pragma once

#include "minera/dataframe_columns.hpp"
#include "minera/frame_keys.hpp"
#include "python_scalar.hpp"

#include <optional>
#include <string>

namespace minera {

// The cell of `key` for one event of `frame` (dataframe_frame_event_log's
// view) as the Python object pandas holds there: typed numbers and bools,
// text, and dates as UTC Timestamps (CSV timestamp columns and XES dates
// alike). nullopt for a missing cell (NaN, NaT, None) or a key the frame
// does not have for this event.
[[nodiscard]] std::optional<python::Scalar> frame_scalar(const EventLog& frame, const Trace& trace,
                                                         const Event& event, const std::string& key,
                                                         FrameKeyScope scope);

// The cell of `key` in one row of `frame` (DataFrame scope): the row's own
// `case:` cell where the row carries one (CSV rows, merged XES cases), typed
// by its column, else frame_scalar's cell.
[[nodiscard]] std::optional<python::Scalar> frame_row_scalar(const EventLog& frame, const Trace& trace,
                                                             const Event& event, const std::string& key);

// The dtype of a frame column (nullopt for no such column): the CSV and XES
// columns as typed, the case and activity copies text, the CSV activity
// column under its own header as read_csv typed it, @@index ints, an XES
// time:timestamp dates.
[[nodiscard]] std::optional<DataFrameColumnType> frame_column_type(const EventLog& frame, const std::string& key);

// A FrameValue of class kNumber as its Python number (bool, int or float).
[[nodiscard]] python::Scalar frame_number_scalar(const FrameValue& value);

}  // namespace minera
