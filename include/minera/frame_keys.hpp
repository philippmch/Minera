#pragma once

#include "minera/event_log.hpp"

#include <map>
#include <set>
#include <string>

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

// The value of `key` for one event of the frame view (a key of frame_keys).
[[nodiscard]] FrameValue frame_value(const Trace& trace, const Event& event,
                                     const std::string& key, FrameKeyScope scope);

// A number's value (True 1, False 0).
[[nodiscard]] double frame_number(const FrameValue& value);

// The names values take as Python dict keys and the order sorted() gives
// them: numbers equal in value are one key (True, 1 and 1.0), named by the
// first one met; numbers order by value, text by code point.
class FrameNames {
 public:
  std::string name(const FrameValue& value);
  [[nodiscard]] bool less(const std::string& left, const std::string& right) const;

 private:
  std::map<double, std::string> numbers_;
  std::map<std::string, double> number_of_;
};

// Throws std::invalid_argument with Python's TypeError text when sorted()
// would compare values of two classes (text beside numbers, a NaN cell or
// dates). kMissing counts as a number (NaN is a float).
void require_sortable_classes(const std::set<FrameValueClass>& classes);

}  // namespace minera
