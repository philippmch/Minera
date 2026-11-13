#pragma once

#include <optional>
#include <string_view>

#include "minera/dataframe_columns.hpp"
#include "minera/event_log.hpp"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace minera {

// Keys of a log read on pm4py's DataFrame path (CSV and XES uploads, see
// dataframe_columns.hpp), as pm4py's functions resolve them on that frame:
// a key is a column name, with no alias (org:resource is only the column of
// that name, so a CSV resource column under another header has no
// org:resource), and a cell is the frame's typed value (observed
// black-box with the organizational mining functions).
//
// kEventLog reads the keys of convert_to_event_log's events (the `case:`
// columns then sit on the trace and are no event key); kDataFrame reads the
// columns of the frame itself, the `case:` columns included.
enum class FrameKeyScope { kEventLog, kDataFrame };

// What Python's sorted() can compare a value with: bools and numbers compare
// with each other (NaN too), text only with text, dates only with dates.
enum class FrameValueClass { kText, kNumber, kDate, kMissing };

struct FrameValue {
  // str() of the frame's Python value ("7" for the int 7, "2.5" for the float
  // 2.5, "True"); empty for a missing cell.
  std::string text;
  FrameValueClass value_class = FrameValueClass::kMissing;
  bool missing() const { return value_class == FrameValueClass::kMissing; }
};

// The keys `frame` (dataframe_frame_event_log's view of a log with
// dataframe_columns) holds in `scope`.
[[nodiscard]] std::set<std::string> frame_keys(const EventLog& frame, FrameKeyScope scope);

// Throws std::invalid_argument("KeyError: '<key>'") when `keys` lacks `key`,
// as the reference raises KeyError for a column the frame does not have.
void require_frame_key(const std::set<std::string>& keys, const std::string& key);

// The value of `key` for one event of `frame` (dataframe_frame_event_log's
// view; a key of frame_keys). A CSV activity column under its own header is
// typed as read_csv typed it (DataFrameColumns::activity_type), its copy
// concept:name is text.
[[nodiscard]] FrameValue frame_value(const EventLog& frame, const Trace& trace,
                                     const Event& event, const std::string& key,
                                     FrameKeyScope scope);

// Throws std::invalid_argument with pandas' KeyError text for the columns of
// a `df[[...]]` selection the frame lacks: "['x'] not in index" when some of
// them exist, "None of [Index([...], dtype='str')] are in the [columns]" when
// none does.
void require_frame_columns(const std::set<std::string>& keys,
                           const std::vector<std::string>& selection);

// A number's value (True 1, False 0).
[[nodiscard]] double frame_number(const FrameValue& value);

// The names values take as Python dict keys and the order sorted() gives
// them: numbers equal in value are one key (True, 1 and 1.0, compared
// exactly, so ints past 2**53 stay apart), named by the first one met;
// numbers order by value, text by code point.
class FrameNames {
 public:
  std::string name(const FrameValue& value);
  [[nodiscard]] bool less(const std::string& left, const std::string& right) const;

 private:
  // A number's Python identity (python_scalar.hpp) and the name it took.
  std::map<std::string, std::string> numbers_;
  // A name's number, as its Python literal ("7", "2.5", "True").
  std::map<std::string, std::string> number_of_;
};

// Throws std::invalid_argument with Python's TypeError text when sorted()
// would compare values of two classes (text beside numbers, a NaN cell or
// dates). kMissing counts as a number (NaN is a float).
void require_sortable_classes(const std::set<FrameValueClass>& classes);

// pm4py's EventLog algorithms (alpha, heuristics, Inductive, POWL) keyed by a
// column of an upload's frame read convert_to_event_log's cells: an activity
// is the cell's Python object, so a number, a date or a missing cell is no
// str (round BS, observed black-box). `log` is the input with each event's
// activity set to str() of its cell ("7", "2.0", "True", "2020-01-01
// 00:00:00+00:00", "nan" for a missing cell, "NaT" in a date column) under
// concept:name; `types` holds each event's Python type name ("" for a str).
struct FrameKeyedActivities {
  EventLog log;
  std::vector<std::vector<std::string>> types;
  bool any_text = false;
  bool non_text = false;            // some cell is no str
  std::string first_non_text_type;  // in log order
  std::size_t distinct = 0;         // distinct labels
  std::optional<DataFrameColumnType> column;  // the key's dtype
};

// nullopt when the log has no frame, or the key is concept:name (or
// "activity") or no column of the frame. With every cell text (non_text
// false) the labels are the cells' text, which the log's own lookup reads
// too, except for a first-class column such as the case column.
[[nodiscard]] std::optional<FrameKeyedActivities> frame_keyed_activities(const EventLog& log,
                                                                         std::string_view key);

// The TypeError pm4py's heuristics miner raises on such activities when a
// directly-follows pair holds one that is no str (empty when none does).
[[nodiscard]] std::string frame_keyed_heuristics_error(const FrameKeyedActivities& keyed);

// The TypeError pm4py's Inductive miner raises on such activities (any cell
// that is no str), and the one its POWL discovery raises (only text beside
// another type: sorted() of the labels); empty when none.
[[nodiscard]] std::string frame_keyed_inductive_error(const FrameKeyedActivities& keyed);
[[nodiscard]] std::string frame_keyed_powl_error(const FrameKeyedActivities& keyed);

}  // namespace minera
