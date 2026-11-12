#pragma once

#include "minera/dataframe_columns.hpp"
#include "minera/frame_keys.hpp"
#include "python_scalar.hpp"

#include <optional>
#include <unordered_map>
#include <string>
#include <utility>
#include <vector>

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

// A TypeError message from comparing a column's cells, as Python words it
// for that column: an XES object column holds its dates as
// datetime.datetime, so its 'Timestamp' names become 'datetime.datetime';
// any other column keeps them (round BC, observed black-box).
[[nodiscard]] std::string column_type_error(std::string message, std::optional<DataFrameColumnType> type);

// One row of a frame: the event and the trace (case) it belongs to.
struct FrameRow {
  const Trace& trace;
  const Event& event;
};

// The frame's rows in the DataFrame's row order. A CSV frame lists them by
// case, as format_dataframe sorted them, which is the traces' order. An XES
// frame keeps read_xes's file order: a case whose <trace> elements repeat
// its name apart from each other has its rows interleaved with other cases'
// rows there, while the frame's traces hold each case's rows together (round
// BF, observed black-box through sort ties, first/last rows and sorted()).
[[nodiscard]] std::vector<FrameRow> frame_rows(const EventLog& frame);

// Each case as several pm4py DataFrame functions read it: the frame's rows
// from the case's first row on, as many as the case has rows (they take a
// case's rows to be adjacent). A CSV frame is sorted by case, so a slice is
// the case's own rows; an XES frame whose trace names repeat apart from each
// other gives a case other cases' rows (declare discovery, get_variants
// counts, filter_between, the activity split; rounds BH and BI, observed
// black-box). The result lists the frame's traces in their order, each with
// its slice's events; a trace without events is left out.
[[nodiscard]] EventLog frame_slice_cases(const EventLog& frame);

// An XES upload's frame keeps read_xes's file order, so its cases need not
// be sorted by id nor their rows adjacent (trace names repeated apart). A
// CSV frame is sorted by case. pm4py's DataFrame activity split and
// filter_between (the mining filter's and the segment filter's) read each
// case as the rows from its first row on, as many as it has
// (frame_slice_cases); the activity split also lists the cases by sorted id
// and hands the labels out to the frame's rows in row order (rounds BI and
// BJ, observed black-box).
struct FrameCases {
  std::vector<FrameRow> rows;                   // the frame's rows in row order
  std::vector<const Trace*> traces;             // the traces with rows, stably sorted by id
  std::unordered_map<const Trace*, std::pair<std::size_t, std::size_t>> extent;  // first row, rows
  bool adjacent = true;                         // every case's rows are adjacent
  bool sorted = true;                           // ... and the cases follow in sorted id order
};

[[nodiscard]] FrameCases frame_cases(const EventLog& frame);

// The traces of a frame whose rows were given new case labels (nullptr: the
// row is dropped), in the order of their first rows, each event in row
// order. A trace takes the case context of its first row's trace; where its
// rows come from traces with other case cells, each row keeps its own
// trace's cells (Event::case_row_attributes), as the frame's rows do.
[[nodiscard]] std::vector<Trace> relabel_frame_rows(const std::vector<FrameRow>& rows,
                                                    const std::vector<const std::string*>& labels);

// A FrameValue of class kNumber as its Python number (bool, int or float).
[[nodiscard]] python::Scalar frame_number_scalar(const FrameValue& value);

// pandas' groupby on a list of keys as long as the frame's rows, when not
// every key is a column, reads the list as one array of row labels (row i
// keyed by keys[i]), so a two-row frame groups by the key names themselves.
// True for that case.
[[nodiscard]] bool pandas_row_label_keys(std::size_t rows, bool all_keys_are_columns);

// The first two characters (code points) of a label, as Python's label[0]
// and label[1] read them; IndexError for a shorter one.
[[nodiscard]] std::pair<std::string, std::string> label_characters(const std::string& label);

}  // namespace minera
