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

// dateutil.parser.parse(text, fuzzy=fuzzy) without a default or tzinfos, as
// pm4py 2.7.22's OCEL 2.0 readers call it on a text fromisoformat refuses
// (read_ocel2_xml fuzzy, read_ocel2_json not; round EB, observed black-box
// on dateutil 2.9). Fields the text leaves out come from `today` at midnight
// (the day cut to the month's length), a weekday without a day moves on to
// that weekday, two-digit years follow today's year. Throws
// std::invalid_argument with str() of dateutil's error ("Unknown string
// format: X", "String does not contain a date: X", datetime's range
// messages and calendar's "bad month number N; must be 1-12" followed by
// ": X", "signed integer is greater than maximum", "date value out of
// range"). nullopt outside the model: characters other than printable ASCII
// and ASCII whitespace, integers past 18 digits, a zone name of the host's
// own zone other than UTC, GMT and Z.
struct Date {
  int year = 1970, month = 1, day = 1;
};

struct DateutilReading {
  Value value;
  // nullopt for a naive reading; dateutil keeps any offset, even a day or
  // more, which pandas refuses when it reads the value.
  std::optional<std::int64_t> offset_seconds;
};

std::optional<DateutilReading> dateutil_parser_parse(std::string_view text, bool fuzzy, Date today);
// Today is the host's local date.
std::optional<DateutilReading> dateutil_parser_parse(std::string_view text, bool fuzzy);

// pd.to_datetime(column, utc=True, format="mixed") of an object column, as
// pm4py 2.7.22's read_ocel2_sqlite reads the event time column (round EB,
// observed black-box): every value on its own. NULL, NaN and the texts
// to_datetime reads as missing (empty, NaT, nat, NAT, nan, NaN, NAN) are
// NaT; an integer counts nanoseconds since the epoch, a float too
// (truncated); a text reads as to_datetime reads it without a format
// (above), except that each value converts to UTC on its own, a text that
// starts with a time and has no date takes today's date through
// dateutil.parser.parse, and a float-like text is an error. Throws
// std::invalid_argument when pandas raises (the reader then keeps the
// column as it is); nullopt outside the model: blobs, characters other than
// printable ASCII and tab, "%", years outside 1000-9999, a year outside the
// nanosecond range next to a number or a nanosecond text, and the other
// shapes to_datetime leaves out.
struct Cell {
  enum class Kind { kNull, kInteger, kReal, kText, kBlob };
  Kind kind = Kind::kNull;
  std::int64_t integer = 0;
  double real = 0;
  std::string_view text;
};

struct UtcInstant {
  std::int64_t seconds = 0;
  std::uint32_t nanoseconds = 0;
  bool aware = false;  // the text gave an offset or a zone
};

std::optional<std::vector<std::optional<UtcInstant>>> to_datetime_utc_mixed(const std::vector<Cell>& cells,
                                                                            Date today);
// Today is the host's local date.
std::optional<std::vector<std::optional<UtcInstant>>> to_datetime_utc_mixed(const std::vector<Cell>& cells);

}  // namespace minera::pandas_to_datetime
