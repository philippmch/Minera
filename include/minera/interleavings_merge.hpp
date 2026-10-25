#pragma once

// algo.discovery.ocel.interleavings.utils.merge_dataframe_rel_cases of the
// PM4Py 2.7.22 reference, on the native event-table carrier (columns, dtypes,
// index, cells as the pandas reference prints them).
//
// directly_follows_dataframe adds a start (▶) and end (■) boundary row per
// case, numbers the rows, and pairs every row with the next row of its case
// (columns suffixed "_2"). merge_dataframes joins the directly-follows tables
// of two logs through a case-relation table with pandas inner-merge order.
// Inputs hold columns of dtype str, int64, float64, bool and
// datetime64[unit(, tz)]; the case column is str or int64.

#include <stdexcept>
#include <string>
#include <utility>

#include "minera/event_data.hpp"

namespace minera {

// The module's Parameters (CASE_ID_KEY, ACTIVITY_KEY, TIMESTAMP_KEY,
// INDEX_KEY, LEFT_SUFFIX, RIGHT_SUFFIX) with the reference defaults.
struct InterleavingsMergeParameters {
  std::string case_id_key = "case:concept:name";
  std::string activity_key = "concept:name";
  std::string timestamp_key = "time:timestamp";
  std::string index_key = "@@index";
  std::string left_suffix = "_LEFT";
  std::string right_suffix = "_RIGHT";
};

// A reference failure: the Python exception class and its str().
class InterleavingsMergeError : public std::invalid_argument {
 public:
  InterleavingsMergeError(std::string type, const std::string& message)
      : std::invalid_argument(message), type(std::move(type)) {}
  std::string type;
};

// Rows: per case (sorted), the boundaries sit at pandas' extreme nanosecond
// timestamps; all rows are numbered as start boundaries (sorted cases), the
// events sorted stably by (case, timestamp), then the end boundaries. The
// table is then sorted stably by (case, timestamp), nulls last, and each row
// with a successor in its case is kept with that successor's columns. Rows
// with a null case have no successor. Columns: case, timestamp, activity,
// the other input columns in input order, the index key (kept in place when
// the input already has it), then the same with "_2". Mixing naive and
// boundary timestamps raises the reference's TypeError.
[[nodiscard]] EventTable interleavings_directly_follows_dataframe(
    const EventTable& dataframe, const InterleavingsMergeParameters& parameters = {});

// Inner-merges directly_follows_dataframe(left) with the relations on
// case + left_suffix, then the result with directly_follows_dataframe(right)
// on case + right_suffix. Columns that both directly-follows tables have
// take the suffixes; other relation columns sit between the two sides.
// A missing relation key column raises KeyError.
[[nodiscard]] EventTable interleavings_merge_dataframes(
    const EventTable& left, const EventTable& right, const EventTable& relations,
    const InterleavingsMergeParameters& parameters = {});

}  // namespace minera
