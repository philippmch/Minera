#pragma once

// pd.to_datetime of an object column of texts, as pm4py 2.7.22's
// read_ocel_csv calls it on the timestamp column (pandas 3.0.5, dateutil
// 2.9; observed black-box, review rounds DZ and EA). pandas guesses one
// format from the first text: it reads the text with dateutil, then matches
// the reading's fields (strftime of %Y, %m, %d, %H, %M, %S, %S.%f, %z, %Z,
// %b, %B, %a, %A, %p, and the compact %Y%m%d... forms) against its own
// tokens, and keeps the format only when every numeric token found a field,
// the year, month and day are among them, the first text parses with it
// and strftime gives the tokens back. With a format, an ISO one (a prefix of
// "%Y-%m-%d %H:%M:%S.%f%z" with any of the date separators " /\-." or none)
// goes to an ISO parser first and every other text to the strptime rules,
// whose errors are the ones raised. Without one, every text is read on its
// own: an ISO parser, then pandas' MM/DD/YYYY and year or month-year
// readers, then dateutil with pandas' checks (fields on 0001-01-01, no
// weekday without a day, unknown zone names refused, nanoseconds from an
// H:MM:SS.fffffffff part when the microseconds end in 000). One zone for the
// column, microseconds unless a text gives nanoseconds.
//
// Outside the model (to_datetime returns nullopt; the CSV reader keeps its
// native reading, declared): characters other than printable ASCII and
// tab, "%", years outside 1000-9999, offsets of 24 hours or more, texts
// pandas reads as numbers, texts that start with a time and leave the date
// to today's, quarters, integers past 18 digits, and a nanosecond text next
// to a year outside the nanosecond range.

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace minera::pandas_to_datetime {

struct Value {
  int year = 1900, month = 1, day = 1, hour = 0, minute = 0;
  int second = 0;  // 60 and 61 (the strptime rules) roll over into the next minute
  std::uint32_t nanoseconds = 0;
};

struct Result {
  std::vector<std::optional<Value>> values;  // one per text, nullopt for an empty one
  std::optional<int> offset_minutes;          // the column's zone, nullopt when naive
  bool nanoseconds = false;                   // datetime64[ns] rather than [us]
};

// Empty texts and pandas' NaT spellings (NaT, nat, NAT, nan, NaN, NAN) are
// missing values. Throws std::invalid_argument with
// pandas' error ("ValueError: ...", "DateParseError: ...",
// "OutOfBoundsDatetime: ..."); nullopt when the column is outside the model.
// Two-digit years follow dateutil's window around current_year.
std::optional<Result> to_datetime(const std::vector<std::string_view>& texts, int current_year);
std::optional<Result> to_datetime(const std::vector<std::string_view>& texts);

}  // namespace minera::pandas_to_datetime
