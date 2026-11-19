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
// Round EC: a text that starts with a time ("10:00", "10:00 May 2024")
// reads on today's date at midnight through the same dateutil checks; a
// text pandas reads as a number below 1000, or a lone a, A, m, M, p, P, t
// or T, is "not likely a datetime"; quarters ("2024Q1", "1Q24", "4Q-2024")
// read as the quarter's first day, a quarter digit outside 1-4 raising;
// years 1 to 9999 read in microseconds; an offset of a day or more raises
// (dateutil's tzoffset error without a format, the timedelta error with
// one); without a format the ISO parser leaves offsets past 59 minutes and
// five-to-seven-digit runs to dateutil. A column turns nanoseconds at the
// first nanosecond text: later values must fit nanoseconds at once, earlier
// ones in a second pass after the zone check (OutOfBoundsDatetime).
//
// Outside the model (to_datetime returns nullopt; the CSV reader keeps its
// native reading, declared): characters other than printable ASCII and
// tab, "%", negative years (a sign before an ISO text) and year 0,
// integers past 18 digits, offsets with seconds, aware values whose UTC
// instant leaves years 1-9999 or the nanosecond range of a nanosecond
// column, and the guess's own exceptions (nanosecond texts in years 1677
// and 2262, %I without %p).

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace minera::pandas_to_datetime {

struct Date {
  int year = 1970, month = 1, day = 1;
};

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
// Two-digit years follow dateutil's window around current_year; today
// (for texts that start with a time) is the host's local date.
std::optional<Result> to_datetime(const std::vector<std::string_view>& texts, int current_year);
// Two-digit years and texts that start with a time on `today`.
std::optional<Result> to_datetime(const std::vector<std::string_view>& texts, Date today);
// The host's local year and date.
std::optional<Result> to_datetime(const std::vector<std::string_view>& texts);
// Round EL: pd.to_datetime(column, errors="coerce"), as
// pm4py.write_ocel2_sqlite converts the event and change time columns
// (observed black-box, probe el sw3.py): the same guess from the first
// text, but a text that does not match the format (or, without one, does
// not read on its own), a day past the month's end and a value outside the
// nanosecond range of a nanosecond column are NaT. Mixed zones still raise.
std::optional<Result> to_datetime_coerce(const std::vector<std::string_view>& texts, Date today);
std::optional<Result> to_datetime_coerce(const std::vector<std::string_view>& texts);

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
// (above, a text that starts with a time on today's date), except that each
// value converts to UTC on its own. Throws std::invalid_argument when
// pandas raises (the reader then keeps the column as it is): a blob, a
// float past int64, any text to_datetime refuses, and a year outside the
// nanosecond range next to a number or a nanosecond text, and a text
// whose dateutil reading meets an integer past 18 digits. nullopt outside
// the model: an integer equal to NaT's value, and the other texts
// to_datetime leaves out.
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
