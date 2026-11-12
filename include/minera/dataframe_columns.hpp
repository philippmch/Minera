#pragma once

#include "minera/event_log.hpp"

#include <cstdint>
#include <optional>
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
  // Which first-class field of a CSV event holds the column's cells: the
  // resource and lifecycle columns, the case column under its own header
  // (kCaseId), and the timestamp column with its copy time:timestamp
  // (kTimestamp, a UTC date); kNone for an ordinary attribute.
  FirstClassAttributeRole role = FirstClassAttributeRole::kNone;
  // A CSV date column with a cell below the microsecond (more than six
  // fraction digits), in any data row: to_datetime then gives
  // datetime64[ns, UTC], else datetime64[us, UTC] (round BO).
  bool nanoseconds = false;
};

// The event columns of a log's frame. For a CSV log (csv_text) the cells are
// the CSV text, typed by read_csv over every data row (rows format_dataframe
// then drops included); kObject is then a column of Python ints. A CSV frame
// also lists its case column when the header has no `case:` prefix (an
// event column there) and its timestamp column with the copy time:timestamp
// (UTC dates). For an XES log the cells are the typed XES values, and a
// column's dtype follows from the types it holds (pandas' inference). The
// activity column (and for XES the timestamp, and the `case:` columns, which
// become trace attributes) are not listed: Minera already reads them as the
// frame holds them. Set on EventLog::dataframe_columns by the batch CSV
// import and by import_xes_dataframe_log.
struct DataFrameColumns {
  bool csv_text = true;
  std::vector<DataFrameColumn> columns;
  // A CSV frame's "@@index" and "@@case_index" cells, which format_dataframe
  // adds: the row's place in the sorted frame and its case's place, by
  // Event::input_order (filtered copies keep them, as a filtered frame keeps
  // the column). Empty for XES frames.
  std::vector<std::int64_t> row_index;
  std::vector<std::int64_t> case_index;
  // The frame's case: columns (the trace attributes other than the case
  // id) under their unprefixed keys: for XES typed over the traces with
  // events (read_xes has no row for a trace without events), for CSV typed
  // by read_csv over every data row (the trace takes the case's first row);
  // a trace without the key holds NaN (NaT for dates).
  std::vector<DataFrameColumn> trace_columns;
  // The dtype read_csv gives a CSV activity column under its own header
  // (format_dataframe only copies it to concept:name as text): ints of an
  // int column are ints there (observed black-box).
  DataFrameColumnType activity_type = DataFrameColumnType::kString;
  // A CSV frame's column names in its order: read_csv's header (`case:`
  // columns included), then format_dataframe's copies case:concept:name,
  // concept:name and time:timestamp that the header lacks (a column of that
  // name is overwritten in place), then @@index and @@case_index (observed
  // black-box). Empty for XES frames, whose order is the first appearance of
  // each key over the converted event stream.
  std::vector<std::string> order;
};

// The event columns of an XES log's frame (read_xes): every event key other
// than the selected activity and timestamp, typed as pandas types a column of
// those values (ints with a gap or beside floats are floats, bools with a gap
// objects), and its trace_columns likewise from the trace attributes.
[[nodiscard]] DataFrameColumns xes_dataframe_columns(const EventLog& log);

// One present cell of a CSV column (csv_text) as the frame holds it: the
// CSV text typed by the column (an int column's "007" is the int 7, a float
// column's "2.50" the float 2.5, "true" the bool True).
[[nodiscard]] EventAttribute csv_dataframe_cell(const DataFrameColumn& column,
                                                const std::string& text);

// An ISO instant as a CSV frame's date column holds it after
// format_dataframe's to_datetime(utc=True): the UTC instant, printed with
// "+00:00" (nullopt when the text is no instant or outside years 1-9999).
[[nodiscard]] std::optional<EventAttribute> csv_utc_date_cell(const std::string& key,
                                                              const std::string& text);

// The log as pm4py's convert_to_event_log of its frame gives it: each event
// of `log` holds every listed column as an attribute, typed by the column
// (ints of a float column as floats, bools as bools), a missing cell as NaN,
// and for a CSV frame "@@index" and "@@case_index" (each trace holds every
// trace column likewise); a CSV event's resource
// and lifecycle are then only the attributes under their headers (no
// org:resource alias). A log without dataframe_columns is returned
// unchanged. Root-cause diagnostics and trace- and event-based features read
// events from this view.
[[nodiscard]] EventLog dataframe_frame_event_log(const EventLog& log);

}  // namespace minera
