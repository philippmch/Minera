#pragma once

#include "minera/event_log.hpp"

#include <string>
#include <vector>

namespace minera {

// The dtype of one event column of a log read on pm4py's DataFrame path
// (read_csv + format_dataframe, or read_xes's frame), as converting the
// frame back to an EventLog (convert_to_event_log) shows it: every event
// then holds a value for every column, a missing cell as NaN (observed
// black-box).
enum class DataFrameColumnType {
  kInt64,    // ints, no missing cell
  kFloat64,  // floats, or ints with a missing cell or beside floats ("1.0")
  kBool,     // True/False, no missing cell
  kString,   // text
  kObject,   // each cell its own Python type (a bool column with a gap too)
  kDate,     // dates; a missing cell NaT
};

struct DataFrameColumn {
  std::string key;
  DataFrameColumnType type = DataFrameColumnType::kString;
  // Which first-class field of a CSV event holds the column's cells (the
  // resource and lifecycle columns); kNone for an ordinary attribute.
  FirstClassAttributeRole role = FirstClassAttributeRole::kNone;
};

// The event columns of a log's frame. For a CSV log (csv_text) the cells are
// the CSV text, typed by read_csv over every data row (rows format_dataframe
// then drops included); kObject is then a column of Python ints. For an XES
// log the cells are the typed XES values, and a column's dtype follows from
// the types it holds (pandas' inference). The activity and timestamp columns
// (and the case-id and `case:` columns, which become trace attributes) are
// not listed: Minera already reads them as the frame holds them. Set on
// EventLog::dataframe_columns by the batch CSV import and by
// import_xes_dataframe_log.
struct DataFrameColumns {
  bool csv_text = true;
  std::vector<DataFrameColumn> columns;
};

// The event columns of an XES log's frame (read_xes): every event key other
// than the selected activity and timestamp, typed as pandas types a column of
// those values (ints with a gap or beside floats are floats, bools with a gap
// objects).
[[nodiscard]] DataFrameColumns xes_dataframe_columns(const EventLog& log);

// One present cell of a CSV column (csv_text) as the frame holds it: the
// CSV text typed by the column (an int column's "007" is the int 7, a float
// column's "2.50" the float 2.5, "true" the bool True).
[[nodiscard]] EventAttribute csv_dataframe_cell(const DataFrameColumn& column,
                                                const std::string& text);

// The log as pm4py's convert_to_event_log of its frame gives it: each event
// of `log` holds every listed column as an attribute, typed by the column
// (ints of a float column as floats, bools as bools), a missing cell as NaN.
// A log without dataframe_columns is returned unchanged. Root-cause
// diagnostics read event contexts from this view.
[[nodiscard]] EventLog dataframe_frame_event_log(const EventLog& log);

}  // namespace minera
