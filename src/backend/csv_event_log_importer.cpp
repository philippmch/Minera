#include "minera/csv_event_log_importer.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "minera/filtering_common.hpp"
#include "minera/timestamp.hpp"

namespace minera {
namespace {

std::string trim(std::string value) {
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return {};
  }

  const auto last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}

// Quoting follows pandas.read_csv's default tokenizer (quotechar '"',
// doublequote), the reference's CSV path: a quote opens a quoted field only
// as a field's first character; anywhere else in an unquoted field it is a
// literal character (5" pipe). Inside a quoted field "" is one quote, and
// text after the closing quote is appended to the field ("ab"cd is abcd).
// An unterminated quoted field is an error, as read_csv's "EOF inside string".
enum class CsvQuoteState { kFieldStart, kUnquoted, kQuoted, kQuoteInQuoted };

std::vector<std::string> parse_csv_row(
    std::string_view row,
    char delimiter,
    bool trim_fields) {
  std::vector<std::string> fields;
  std::string field;
  auto state = CsvQuoteState::kFieldStart;

  for (const char ch : row) {
    if (state == CsvQuoteState::kQuoted) {
      if (ch == '"') state = CsvQuoteState::kQuoteInQuoted;
      else field.push_back(ch);
    } else if (state == CsvQuoteState::kQuoteInQuoted && ch == '"') {
      field.push_back('"');
      state = CsvQuoteState::kQuoted;
    } else if (ch == delimiter) {
      fields.push_back(trim_fields ? trim(std::move(field)) : std::move(field));
      field.clear();
      state = CsvQuoteState::kFieldStart;
    } else if (state == CsvQuoteState::kFieldStart && ch == '"') {
      state = CsvQuoteState::kQuoted;
    } else {
      field.push_back(ch);
      state = CsvQuoteState::kUnquoted;
    }
  }

  if (state == CsvQuoteState::kQuoted) {
    throw std::runtime_error("CSV row contains an unterminated quoted field");
  }

  fields.push_back(trim_fields ? trim(std::move(field)) : std::move(field));
  return fields;
}

// A quoted field can span physical lines. Keep its exact CR/LF bytes; only an
// unquoted record terminator is consumed. Escaped quotes retain their spelling
// for the field parser to decode; quotes follow the same rule as
// parse_csv_row, so a literal quote inside an unquoted field does not open a
// multi-line field. `lone_cr`, when given, tells whether the record ended
// with a "\r" not followed by "\n".
bool read_csv_record(std::istream& input, std::string& record, char delimiter,
                     bool* lone_cr = nullptr) {
  record.clear();
  if (lone_cr) *lone_cr = false;
  auto state = CsvQuoteState::kFieldStart;
  bool read_any = false;
  char ch;
  while (input.get(ch)) {
    read_any = true;
    if (state == CsvQuoteState::kQuoted) {
      record.push_back(ch);
      if (ch == '"') state = CsvQuoteState::kQuoteInQuoted;
    } else if (state == CsvQuoteState::kQuoteInQuoted && ch == '"') {
      record.push_back(ch);
      state = CsvQuoteState::kQuoted;
    } else if (ch == '\r' || ch == '\n') {
      if (ch == '\r' && input.peek() == '\n') input.get(ch);
      else if (ch == '\r' && lone_cr) *lone_cr = true;
      return true;
    } else {
      record.push_back(ch);
      state = ch == delimiter ? CsvQuoteState::kFieldStart
            : state == CsvQuoteState::kFieldStart && ch == '"' ? CsvQuoteState::kQuoted
            : CsvQuoteState::kUnquoted;
    }
  }
  return read_any;
}

std::unordered_map<std::string, std::size_t> header_index(
    const std::vector<std::string>& header) {
  std::unordered_map<std::string, std::size_t> index;
  for (std::size_t i = 0; i < header.size(); ++i) {
    index.emplace(header[i], i);
  }
  return index;
}

std::size_t required_column(
    const std::unordered_map<std::string, std::size_t>& columns,
    const std::string& name) {
  const auto it = columns.find(name);
  if (it == columns.end()) {
    throw std::invalid_argument("CSV column not found: " + name);
  }
  return it->second;
}

std::vector<std::string_view> selector_aliases(std::string_view name) {
  if (name == "activity" || name == "concept:name")
    return {"activity", "concept:name"};
  if (name == "resource" || name == "org:resource")
    return {"resource", "org:resource"};
  if (name == "timestamp" || name == "time:timestamp")
    return {"timestamp", "time:timestamp"};
  if (name == "lifecycle" || name == "lifecycle:transition")
    return {"lifecycle", "lifecycle:transition"};
  return {};
}

std::size_t required_selector_column(
    const std::unordered_map<std::string, std::size_t>& columns,
    const std::string& name) {
  if (name.find_first_not_of(" \t\r\n") == std::string::npos) {
    throw std::invalid_argument("CSV activity column must not be blank");
  }
  if (const auto it = columns.find(name); it != columns.end()) return it->second;
  for (const auto alias : selector_aliases(name)) {
    if (const auto it = columns.find(std::string(alias)); it != columns.end())
      return it->second;
  }
  throw std::invalid_argument("CSV column not found: " + name);
}

std::optional<std::size_t> optional_selector_column(
    const std::unordered_map<std::string, std::size_t>& columns,
    const std::string& name) {
  if (const auto it = columns.find(name); it != columns.end()) return it->second;
  for (const auto alias : selector_aliases(name)) {
    if (const auto it = columns.find(std::string(alias)); it != columns.end())
      return it->second;
  }
  return std::nullopt;
}

std::string field_at(
    const std::vector<std::string>& fields,
    std::size_t index,
    std::size_t line_number) {
  if (index >= fields.size()) {
    throw std::runtime_error(
        "CSV row " + std::to_string(line_number) + " has too few fields");
  }
  return fields[index];
}

// read_csv skips a line made only of spaces and tabs (none of them the
// delimiter) like an empty one; other whitespace (\v, \f, ...) is a value.
bool is_reference_blank_line(std::string_view line, char delimiter) {
  return std::all_of(line.begin(), line.end(), [&](char ch) {
    return (ch == ' ' || ch == '\t') && ch != delimiter;
  });
}

// pandas' tokenizer quirk after a skipped blank line that a lone "\r" ends
// (observed black-box on read_csv): a delimiter right after it is dropped,
// so "\r\r,5,6" reads as the row 5,6 (shifted left). A line that then starts
// with spaces or tabs before other text is misread by pandas (rows of
// garbage), so it is refused. `record` is the line after the blank one.
void apply_lone_cr_blank_quirk(std::string& record, char delimiter, std::size_t line_number) {
  if (!record.empty() && record.front() == delimiter) record.erase(0, 1);
  if (record.empty() || (record.front() != ' ' && record.front() != '\t') ||
      record.front() == delimiter || is_reference_blank_line(record, delimiter))
    return;
  throw std::runtime_error(
      "CSV line " + std::to_string(line_number) +
      " starts with spaces or tabs after a blank line ended by a lone carriage return, "
      "which pandas.read_csv misreads; this reader refuses it");
}

// pandas.read_csv's default NA spellings. The reference flow (read_csv, then
// format_dataframe) drops every row whose case id, activity or timestamp is
// NA, so these exact cell texts mark a missing first-class value. Whitespace
// is not NA in pandas.
bool is_reference_na_token(std::string_view value) {
  static constexpr std::string_view kTokens[] = {
      "",     "#N/A", "#N/A N/A", "#NA",  "-1.#IND", "-1.#QNAN", "-NaN",
      "-nan", "1.#IND", "1.#QNAN", "<NA>", "N/A",     "NA",       "NULL",
      "NaN",  "None", "n/a",      "nan",  "null"};
  return std::find(std::begin(kTokens), std::end(kTokens), value) !=
         std::end(kTokens);
}

// --- Reference timestamp inference -----------------------------------------
// PM4Py 2.7.22's format_dataframe converts a textual timestamp column the way
// pandas' to_datetime(format="mixed", utc=True) does: every cell is parsed on
// its own (no format is inferred from the first value), month-first when the
// leading two date numbers are ambiguous and day-first when the first exceeds
// 12, naive values read as UTC. If any cell cannot be parsed the column stays
// textual and every reference consumer then rejects the frame, so the native
// importer rejects a column that mixes dates with unconvertible cells instead
// of keeping uninterpretable text (see the column rule after the row scan).
// ISO-8601 cells the native parser already reads keep their spelling; other
// recognized shapes are rewritten to ISO. Shapes outside the grammar below
// (time-only cells, partial dates, named zones other than UTC/GMT, epoch
// numbers) count as unconvertible.

struct TimestampTokens {
  std::string_view text;
  std::size_t pos = 0;
  void skip_spaces() {
    while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\t')) ++pos;
  }
  bool done() const { return pos >= text.size(); }
  char peek() const { return done() ? '\0' : text[pos]; }
  // Reads 1..max_digits ASCII digits; returns the count read (0 if none).
  std::size_t digits(unsigned& value, std::size_t max_digits) {
    std::size_t count = 0;
    value = 0;
    while (!done() && count < max_digits && text[pos] >= '0' && text[pos] <= '9') {
      value = value * 10 + static_cast<unsigned>(text[pos] - '0');
      ++pos;
      ++count;
    }
    // A longer digit run than allowed is not this field.
    if (count == max_digits && !done() && text[pos] >= '0' && text[pos] <= '9')
      return 0;
    return count;
  }
  std::string word() {
    std::string out;
    while (!done() && std::isalpha(static_cast<unsigned char>(text[pos]))) {
      out.push_back(static_cast<char>(
          std::tolower(static_cast<unsigned char>(text[pos]))));
      ++pos;
    }
    return out;
  }
};

int month_from_name(const std::string& word) {
  static constexpr std::string_view kMonths[] = {
      "january", "february", "march",     "april",   "may",      "june",
      "july",    "august",   "september", "october", "november", "december"};
  if (word.size() < 3) return 0;
  for (int i = 0; i < 12; ++i) {
    const auto name = kMonths[i];
    if (word == name || (word == name.substr(0, 3)) ||
        (i == 8 && word == "sept"))
      return i + 1;
  }
  return 0;
}

bool is_weekday_name(const std::string& word) {
  static constexpr std::string_view kDays[] = {
      "monday", "tuesday", "wednesday", "thursday", "friday", "saturday",
      "sunday"};
  if (word.size() < 3) return false;
  for (const auto day : kDays)
    if (word == day || word == day.substr(0, 3)) return true;
  return false;
}

bool leap_year(int year) {
  return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

int month_days(int year, int month) {
  static constexpr int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return month == 2 && leap_year(year) ? 29 : kDays[month - 1];
}

// dateutil maps a two-digit year into the century window centred on the
// current year.
int expand_two_digit_year(unsigned year) {
  const auto now = std::chrono::system_clock::now();
  const auto today = std::chrono::year_month_day(
      std::chrono::floor<std::chrono::days>(now));
  const int current = static_cast<int>(today.year());
  int expanded = static_cast<int>(year) + current / 100 * 100;
  if (expanded >= current + 50) expanded -= 100;
  else if (expanded < current - 50) expanded += 100;
  return expanded;
}

// The year of a date token read with `digits` digits. dateutil keeps a year
// joined to the rest of the date by '-', '/' or '.' as text, so three or four
// digits are literal ("8-May-0001" is year 1); a year standing alone after
// white space is read as a number, so any value below 100 is a two-digit
// year ("8 May 0001" is 2001).
std::optional<int> reference_year(unsigned value, std::size_t digits, bool joined) {
  if (digits == 0) return std::nullopt;
  if (joined ? digits <= 2 : value < 100) return expand_two_digit_year(value);
  return static_cast<int>(value);
}

// A recognized cell in ISO spelling. `iso_path` marks the shapes pandas reads
// with its ISO-8601 parser (YYYY-M-D with `-`, `/`, `.` or a single space, a
// `T` or single space before a clock of an hour, HH:MM or HH:MM:SS (a
// one-digit hour too), a `.` fraction, an uppercase `Z` or a numeric offset):
// only those keep nanoseconds and read a spaced YYYY MM DD literally; every
// other shape goes through dateutil, which truncates the fraction to
// microseconds and orders three plain numbers itself.
struct NormalizedTimestamp {
  std::string text;
  bool iso_path = false;
  // The text keeps a fraction of more than six digits (nanosecond resolution).
  bool nanosecond = false;
};

// A decimal number spelled as its significant digits (no leading zeros) times
// 10^exponent; empty digits is zero.
struct DecimalDigits {
  std::string digits;
  int exponent = 0;
};

// Rounds to 28 significant digits, half to even, as Python's decimal module
// does in its default context. `sticky` tells that nonzero digits beyond
// `digits` were left out.
DecimalDigits round_28_digits(DecimalDigits value, bool sticky = false) {
  constexpr std::size_t kPrecision = 28;
  if (value.digits.size() <= kPrecision) return value;
  const char next = value.digits[kPrecision];
  bool rest = sticky;
  for (std::size_t i = kPrecision + 1; i < value.digits.size() && !rest; ++i)
    rest = value.digits[i] != '0';
  value.exponent += static_cast<int>(value.digits.size() - kPrecision);
  value.digits.resize(kPrecision);
  const bool odd = (value.digits.back() - '0') % 2 == 1;
  if (next > '5' || (next == '5' && (rest || odd))) {
    std::size_t i = value.digits.size();
    while (i > 0 && value.digits[i - 1] == '9') value.digits[--i] = '0';
    if (i == 0) {
      // 99...9 rounds up to 10...0: one more digit, so drop a zero.
      value.digits.insert(value.digits.begin(), '1');
      value.digits.pop_back();
      ++value.exponent;
    } else {
      ++value.digits[i - 1];
    }
  }
  return value;
}

// The whole seconds dateutil gives a fraction of a minute ("10:30.5" is
// 10:30:30): int(60 * (minutes % 1)) over decimal.Decimal, so both the
// fraction and the product are rounded to 28 significant digits. Returns 60
// or more when the product rounds up to a whole minute, which the reference
// refuses (review Y1-1).
unsigned reference_minute_fraction_seconds(std::string_view fraction_digits) {
  std::size_t zeros = 0;
  while (zeros < fraction_digits.size() && fraction_digits[zeros] == '0') ++zeros;
  if (zeros == fraction_digits.size()) return 0;
  // 29 significant digits decide the rounding; the rest only as nonzero.
  DecimalDigits fraction;
  fraction.digits = std::string(fraction_digits.substr(zeros, 29));
  bool sticky = false;
  for (std::size_t i = zeros + 29; i < fraction_digits.size() && !sticky; ++i)
    sticky = fraction_digits[i] != '0';
  fraction.exponent = -static_cast<int>(zeros + fraction.digits.size());
  fraction = round_28_digits(std::move(fraction), sticky);
  // Times 60: times 6, then one more power of ten.
  std::string product(fraction.digits.size() + 1, '0');
  unsigned carry = 0;
  for (std::size_t i = fraction.digits.size(); i-- > 0;) {
    const unsigned digit = static_cast<unsigned>(fraction.digits[i] - '0') * 6 + carry;
    product[i + 1] = static_cast<char>('0' + digit % 10);
    carry = digit / 10;
  }
  product[0] = static_cast<char>('0' + carry);
  if (product[0] == '0') product.erase(product.begin());
  auto rounded = round_28_digits(DecimalDigits{product, fraction.exponent + 1});
  // The integer part (at most two digits: the value is at most 60).
  const int integer_digits = static_cast<int>(rounded.digits.size()) + rounded.exponent;
  if (integer_digits <= 0) return 0;
  unsigned seconds = 0;
  for (int i = 0; i < integer_digits; ++i) {
    const char digit = i < static_cast<int>(rounded.digits.size()) ? rounded.digits[static_cast<std::size_t>(i)] : '0';
    seconds = seconds * 10 + static_cast<unsigned>(digit - '0');
    if (seconds >= 60) return 60;
  }
  return seconds;
}

// `allow_year_zero` accepts year 0 (proleptic, a leap year), which pandas'
// ISO path holds and dateutil rejects; the caller decides by `iso_path`.
std::optional<NormalizedTimestamp> normalize_reference_timestamp_shapes(
    std::string_view text, bool allow_year_zero = false) {
  TimestampTokens in{text};
  in.skip_spaces();
  int year = 0, month = 0, day = 0;
  // Whether pandas' ISO-8601 parser takes the cell (see NormalizedTimestamp).
  bool iso_path = true;
  // A space-separated YYYY MM DD: pandas reads it on its ISO path, but a cell
  // that leaves that path reaches dateutil, which reads three numbers.
  std::optional<std::array<unsigned, 3>> spaced_numbers;

  // Optional leading weekday name ("Tue 01/02/2024", "Tuesday, Jan 2 2024").
  if (std::isalpha(static_cast<unsigned char>(in.peek()))) {
    const auto saved = in.pos;
    const auto name = in.word();
    if (is_weekday_name(name)) {
      iso_path = false;
      if (in.peek() == ',') ++in.pos;
      in.skip_spaces();
    } else {
      in.pos = saved;
    }
  }

  const auto date_separator = [](char ch) {
    return ch == '/' || ch == '-' || ch == '.' || ch == ' ';
  };
  // The separator after the second date number: the first one again, or (on
  // dateutil's path only) another of `-`, `/` and white space, which dateutil
  // reads alike ("2024-12/07", "2024 3/5", "07-12/2024"). A mix with `.` is
  // left unparsed: dateutil reads such a token as a decimal number.
  const auto second_separator = [&](char first, char ch) {
    if (ch == first) return true;
    if (first == '.' || ch == '.' || !date_separator(ch)) return false;
    iso_path = false;
    return true;
  };
  if (std::isalpha(static_cast<unsigned char>(in.peek()))) {
    // Mon D[,] YYYY
    iso_path = false;
    month = month_from_name(in.word());
    if (month == 0) return std::nullopt;
    if (!date_separator(in.peek())) return std::nullopt;
    ++in.pos;
    in.skip_spaces();
    unsigned d = 0, y = 0;
    if (in.digits(d, 2) == 0) return std::nullopt;
    if (in.peek() == ',') ++in.pos;
    if (!date_separator(in.peek())) return std::nullopt;
    const bool joined = in.peek() != ' ';
    ++in.pos;
    in.skip_spaces();
    const auto year_digits = in.digits(y, 4);
    const auto parsed_year = reference_year(y, year_digits, joined);
    if (!parsed_year) return std::nullopt;
    year = *parsed_year;
    day = static_cast<int>(d);
  } else {
    unsigned first = 0;
    const auto first_digits = in.digits(first, 4);
    if (first_digits == 0) return std::nullopt;
    const char sep = in.peek();
    if (!date_separator(sep)) return std::nullopt;
    ++in.pos;
    const bool joined = sep != ' ';
    // dateutil keeps a token joined by '-', '/' or '.' as text, so a leading
    // three-digit one is a literal year ("075.08.22" is year 75); white
    // space separates numbers, so "075 06 11" reads as 75, 6, 11.
    const bool leading_year =
        first_digits == 4 || (first_digits == 3 && (joined || first >= 100));
    if (first_digits == 3) iso_path = false;
    if (leading_year && std::isalpha(static_cast<unsigned char>(in.peek()))) {
      // YYYY-Mon-D
      iso_path = false;
      month = month_from_name(in.word());
      unsigned d = 0;
      if (month == 0 || in.peek() != sep) return std::nullopt;
      ++in.pos;
      if (in.digits(d, 2) == 0) return std::nullopt;
      year = static_cast<int>(first);
      day = static_cast<int>(d);
      // Space-separated, a leading number below 100 is not a year: it is
      // the day when it can be one, and the trailing number is the year.
      if (!joined && first < 100) {
        if (first <= 31) {
          day = static_cast<int>(first);
          year = expand_two_digit_year(d);
        } else {
          year = expand_two_digit_year(first);
        }
      }
    } else if (leading_year) {
      // YYYY-M-D in any of the separators (space-separated too on the ISO
      // path; see the dateutil reading after the clock).
      unsigned m = 0, d = 0;
      if (in.digits(m, 2) == 0 || !second_separator(sep, in.peek())) return std::nullopt;
      // Joined by `-` or `/` and then another separator (dateutil's path),
      // the day may carry leading zeros up to four digits ("2024-1/0015" is
      // 2024-01-15, "2024/01 031" 2024-01-31; review W1-1). Separated by
      // white space alone, so it may too, and the three numbers then take
      // dateutil's order below ("2023 05 012" is 2023-05-12, "0012 05 012"
      // 2012-12-05).
      const bool long_day = joined ? in.peek() != sep : in.peek() == sep;
      ++in.pos;
      const auto day_digits = in.digits(d, long_day ? 4 : 2);
      if (day_digits == 0) return std::nullopt;
      if (day_digits > 2) iso_path = false;
      year = static_cast<int>(first);
      month = static_cast<int>(m);
      day = static_cast<int>(d);
      if (!joined) spaced_numbers = std::array<unsigned, 3>{first, m, d};
    } else if (std::isalpha(static_cast<unsigned char>(in.peek()))) {
      // D Mon YYYY / D-Mon-YYYY
      iso_path = false;
      month = month_from_name(in.word());
      if (month == 0 || in.peek() != sep) return std::nullopt;
      ++in.pos;
      unsigned y = 0;
      const auto year_digits = in.digits(y, 4);
      const auto parsed_year = reference_year(y, year_digits, joined);
      if (!parsed_year) return std::nullopt;
      year = *parsed_year;
      day = static_cast<int>(first);
      // A first number that cannot be a day is the year ("40 Sep 28").
      if (first > 31 && year_digits <= 2 && y >= 1 && y <= 31) {
        year = expand_two_digit_year(first);
        day = static_cast<int>(y);
      }
    } else {
      // A/B/C: year-first when the first number cannot be a day ("40-03-03"),
      // else month-first unless the first number cannot be a month.
      iso_path = false;
      unsigned second = 0, y = 0;
      if (in.digits(second, 2) == 0 || !second_separator(sep, in.peek())) return std::nullopt;
      // Whether the trailing year is joined: by its own `-` or `/` after a
      // space ("28 07-0057" is year 57), but not after a different joined
      // separator, where it reads as a plain number ("15-04/0001" is 2001).
      const bool mixed = in.peek() != sep;
      const bool year_joined = sep == ' ' ? in.peek() != ' ' : !mixed;
      ++in.pos;
      const auto year_digits = in.digits(y, 4);
      if (year_digits == 0) return std::nullopt;
      if (first > 31 && mixed && sep == ' ' && year_digits > 2) {
        // "57 7/0001" is 0057-07-01: the leading number is a literal year
        // and the joined last one, written with more than two digits, the
        // day. With one or two digits the leading number is a two-digit year
        // ("40 3/15" is 2040-03-15, "99 05/27" 1999-05-27; review W1-1).
        year = static_cast<int>(first);
        month = static_cast<int>(second);
        day = static_cast<int>(y);
      } else if (first > 31) {
        // "40-03-03" and, mixed, "33-01 0011" (2033-01-11): a two-digit year.
        // Separated by white space alone, the day may carry leading zeros
        // ("99 05 012" is 1999-05-12; review W1-1).
        if (year_digits > 2 && !mixed && sep != ' ') return std::nullopt;
        year = expand_two_digit_year(first);
        month = static_cast<int>(second);
        day = static_cast<int>(y);
      } else {
        year = *reference_year(y, year_digits, year_joined);
        if (first > 12) {
          day = static_cast<int>(first);
          month = static_cast<int>(second);
        } else {
          month = static_cast<int>(first);
          day = static_cast<int>(second);
        }
      }
    }
  }
  const auto valid_date = [&] {
    return year >= (allow_year_zero ? 0 : 1) && month >= 1 && month <= 12 && day >= 1 &&
           day <= month_days(year, month);
  };
  // A space-separated YYYY MM DD is checked once its reading is known.
  if (!spaced_numbers && !valid_date()) return std::nullopt;
  // Trailing white space after a date alone is ignored, but after a spaced
  // YYYY M D it leaves pandas' ISO path ("0060 3 20 " is 2060-03-20, while
  // "0060-03-20 " and "0060 3 20 10:00 " stay year 60).
  if (const auto saved = in.pos; true) {
    in.skip_spaces();
    if (!in.done()) in.pos = saved;
    else if (in.pos != saved && spaced_numbers) iso_path = false;
  }

  int hour = 0, minute = 0, second = 0;
  std::string fraction;
  bool comma_fraction = false;
  std::string offset;
  int offset_minutes = 0;
  if (!in.done()) {
    if (in.peek() != ' ' && in.peek() != 'T' && in.peek() != 't')
      return std::nullopt;
    if (in.peek() == 't') iso_path = false;
    ++in.pos;
    const auto clock_start = in.pos;
    in.skip_spaces();
    if (in.pos != clock_start) iso_path = false;
    // A `T` may also follow white space, with or without more white space
    // after it (dateutil: "2024-01-05 T13:38", "20240105  T11:07"; review
    // Y1-3).
    if ((in.pos != clock_start || in.text[clock_start - 1] == ' ') &&
        (in.peek() == 'T' || in.peek() == 't') && in.pos + 1 < in.text.size() &&
        (std::isdigit(static_cast<unsigned char>(in.text[in.pos + 1])) ||
         in.text[in.pos + 1] == ' ')) {
      iso_path = false;
      ++in.pos;
      in.skip_spaces();
    }
    unsigned h = 0, m = 0, s = 0;
    // After a `.` without digits, a lone `a`/`p` fails at the end of the
    // text or before a `Z`, `z`, digit, `:` or `,` ("3:04.P", "09:32:02.p",
    // ".pz" and ".p5" fail; "3:04.pm", "3:04.p.m.", "3:04. P", ".p ", ".p Z"
    // and ".p-05" read; reviews Y1-3, Z1-5).
    const auto lone_meridiem_letter = [&in]() {
      const char ch = in.peek();
      if (ch != 'a' && ch != 'A' && ch != 'p' && ch != 'P') return false;
      if (in.pos + 1 >= in.text.size()) return true;
      const char next = in.text[in.pos + 1];
      return next == 'Z' || next == 'z' || next == ':' || next == ',' ||
             std::isdigit(static_cast<unsigned char>(next));
    };
    // After a `.` without digits, a zone letter at the very end of the text
    // fails, except `Z` on pandas' ISO path ("09:32:02.Z" reads, while
    // "2024-01-05  09:32:02.Z", "01/05/2024 09:32:02.Z" and any ".z" fail;
    // ".Z pm", ".z pm" and ".Z " read; review Z1-1).
    const auto empty_fraction_zone_fails = [&in, &iso_path]() {
      if (in.pos + 1 != in.text.size()) return false;
      return in.peek() == 'z' || (in.peek() == 'Z' && !iso_path);
    };
    // The digits of a `.` seconds fraction (`,` too when `comma` is allowed).
    const auto read_seconds_fraction = [&](bool comma) -> bool {
      if (in.peek() != '.' && !(comma && in.peek() == ',')) return true;
      if (in.peek() == ',') { iso_path = false; comma_fraction = true; }
      ++in.pos;
      std::size_t fraction_digits = 0;
      while (!in.done() && std::isdigit(static_cast<unsigned char>(in.peek()))) {
        if (fraction.size() < 9) fraction.push_back(in.peek());
        ++fraction_digits;
        ++in.pos;
      }
      if (fraction_digits == 0) {
        // A `.` with no digits is dateutil's, and adds nothing
        // ("09:32:02." and "09:32:02.Z" read, "09:32:02.z" fails; reviews
        // Y1-3, Z1-1).
        if (comma_fraction || empty_fraction_zone_fails() || lone_meridiem_letter())
          return false;
        iso_path = false;
        return true;
      }
      // pandas' ISO path reads at most 18 fraction digits; a longer
      // fraction goes through dateutil ("...56.3650503944960724188" keeps
      // microseconds only, 18 digits keep nanoseconds).
      if (fraction_digits > 18) iso_path = false;
      return true;
    };
    // A basic clock HHMM or HHMMSS ("2024-01-05 1107", "20240105T110700.5";
    // review Y1-3); only the six-digit one takes a fraction.
    std::size_t run = 0;
    while (in.pos + run < in.text.size() &&
           std::isdigit(static_cast<unsigned char>(in.text[in.pos + run])))
      ++run;
    const bool basic_clock = run == 4 || run == 6;
    std::size_t hour_digits = 0;
    if (basic_clock) {
      const auto pair = [&](std::size_t at) {
        return static_cast<unsigned>((in.text[in.pos + at] - '0') * 10 +
                                     (in.text[in.pos + at + 1] - '0'));
      };
      h = pair(0);
      m = pair(2);
      if (run == 6) s = pair(4);
      in.pos += run;
      hour_digits = 2;
      // HHMM takes a `.` without digits, on dateutil's path ("1000." and
      // "1000. pm" read, "1000.5" and "1000.Z" fail; review Z1-5).
      if (run == 4 && in.peek() == '.') {
        if (in.pos + 1 < in.text.size() &&
            std::isdigit(static_cast<unsigned char>(in.text[in.pos + 1])))
          return std::nullopt;
        iso_path = false;
      }
      if (in.peek() == ',') return std::nullopt;
      if (!read_seconds_fraction(false)) return std::nullopt;
    } else {
      hour_digits = in.digits(h, 2);
      if (hour_digits == 0) return std::nullopt;
    }
    // pandas' ISO path also takes a one-digit hour, an hour alone and HH:MM
    // ("0047 1 8 9:36", "0047 01 08 19" are year 47; "2024-01-01
    // 9:00:00.123456789" keeps its nanoseconds). A one-digit hour alone is
    // no clock to either parser unless AM/PM follows ("2089-05-26 9" fails,
    // "03/26/2089 9 PM" is 21:00).
    const bool bare_digit_hour = !basic_clock && hour_digits == 1 && in.peek() != ':';
    bool meridiem_read = false;
    if (!basic_clock && in.peek() == ':') {
      ++in.pos;
      // One-digit minutes and seconds read too, on pandas' ISO path as well
      // ("10:3:5"; "0000-01-05 10:3" is year 0).
      if (in.digits(m, 2) == 0) return std::nullopt;
      if (in.peek() == '.') {
        // A fraction of a minute (dateutil): the seconds are the whole
        // seconds of 60 times the fraction, with no sub-second part
        // ("10:30.5" is 10:30:30, "06:37.9" 06:37:54, "10:30." 10:30:00;
        // review X1).
        iso_path = false;
        ++in.pos;
        const auto fraction_start = in.pos;
        while (!in.done() && std::isdigit(static_cast<unsigned char>(in.peek()))) ++in.pos;
        const std::size_t used = in.pos - fraction_start;
        // int(60 * fraction) in 28-digit decimal arithmetic (review Y1-1:
        // "10:07.01" + 28 sixes is 10:07:01; 29 nines round to 60 and fail).
        s = reference_minute_fraction_seconds(in.text.substr(fraction_start, used));
        if (s >= 60) return std::nullopt;
        // An empty fraction right before a closing `Z` fails ("10:30.Z";
        // "10:30.5Z", "10:30. Z", "10:30.Z pm", "10:55.Z " and "10:30.UTC"
        // read; review Z1-5).
        if (used == 0 && (empty_fraction_zone_fails() || lone_meridiem_letter()))
          return std::nullopt;
      } else if (in.peek() == ':') {
        ++in.pos;
        if (in.digits(s, 2) == 0) return std::nullopt;
        if (!read_seconds_fraction(true)) return std::nullopt;
      }
    }
    // AM/PM takes an hour of 0 to 12 ("0:30 PM" is 12:30, "0 AM" 00:00).
    // It may also follow the offset or zone ("3:08+05pm", "3:08 UTC pm";
    // review X1). dateutil also reads `a`/`p` alone and dotted spellings
    // whose `m` is lowercase ("3:08 p", "p.m.", "P.m.", "p. m.", "pm.";
    // "P.M." fails), and applies a repeated one again, so it fails once the
    // hour is past 12 ("12:30pm pm" is 12:30, "3:08pm pm" fails). A bare
    // one-digit hour cannot take one after two or more blanks ("1  pm"
    // fails, "11  pm" and "1:00  pm" read; review Y1-3).
    const auto read_meridiem = [&]() -> bool {
      for (;;) {
        const auto before = in.pos;
        in.skip_spaces();
        const bool wide = in.pos - before >= 2;
        if (!(in.peek() == 'a' || in.peek() == 'A' || in.peek() == 'p' || in.peek() == 'P'))
          return true;
        const auto saved = in.pos;
        const auto meridiem = in.word();
        bool pm = false;
        if (meridiem == "pm" || meridiem == "p") pm = true;
        else if (meridiem != "am" && meridiem != "a") {
          in.pos = saved;
          return true;
        }
        if (meridiem.size() == 1) {
          auto j = in.pos;
          while (j < in.text.size() && (in.text[j] == '.' || in.text[j] == ' ')) ++j;
          if (j > in.pos && j < in.text.size() && in.text[j] == 'm' &&
              !(j + 1 < in.text.size() &&
                std::isalpha(static_cast<unsigned char>(in.text[j + 1]))))
            in.pos = j + 1;
        }
        while (in.peek() == '.') ++in.pos;
        if (!meridiem_read && bare_digit_hour && wide) return false;
        iso_path = false;
        meridiem_read = true;
        if (h > 12) return false;
        if (!pm && h == 12) h = 0;
        else if (pm && h != 12) h += 12;
      }
    };
    if (!read_meridiem()) return std::nullopt;
    if (bare_digit_hour && !meridiem_read) return std::nullopt;
    if (h > 23 || m > 59 || s > 59) return std::nullopt;
    hour = static_cast<int>(h);
    minute = static_cast<int>(m);
    second = static_cast<int>(s);
    if (!in.done()) {
      const char ch = in.peek();
      if (ch == 'Z' || ch == 'z') {
        if (ch == 'z') iso_path = false;
        ++in.pos;
        // A word right after it is no zone ("Zpm" fails, "Z pm" reads).
        if (std::isalpha(static_cast<unsigned char>(in.peek()))) return std::nullopt;
        offset = "+00:00";
      } else if (ch == '+' || ch == '-') {
        ++in.pos;
        unsigned oh = 0, om = 0;
        // Offsets are +HH, +HH:MM or +HHMM; read the hour and minute pairs
        // exactly, since the minutes may follow the hours without a colon.
        const auto two_digits = [&in](unsigned& value) {
          if (in.pos + 2 > in.text.size() || in.text[in.pos] < '0' || in.text[in.pos] > '9' ||
              in.text[in.pos + 1] < '0' || in.text[in.pos + 1] > '9')
            return false;
          value = static_cast<unsigned>((in.text[in.pos] - '0') * 10 + (in.text[in.pos + 1] - '0'));
          in.pos += 2;
          return true;
        };
        // A one-digit hour, and a one-digit minute after a colon, read too
        // ("+5" is +05:00, "+5:3" +05:03, "+0:30" +00:30; "+530" fails;
        // review Y1-3).
        const auto one_digit = [&in](unsigned& value) {
          if (in.done() || !std::isdigit(static_cast<unsigned char>(in.peek()))) return false;
          if (in.pos + 1 < in.text.size() &&
              std::isdigit(static_cast<unsigned char>(in.text[in.pos + 1])))
            return false;
          value = static_cast<unsigned>(in.peek() - '0');
          ++in.pos;
          return true;
        };
        if (!two_digits(oh) && !one_digit(oh)) return std::nullopt;
        const bool colon = in.peek() == ':';
        if (colon) ++in.pos;
        // An hour-only offset followed by white space is a dateutil shape.
        if (!in.done() && (in.peek() == ' ' || in.peek() == '\t')) iso_path = false;
        if (!in.done() && in.peek() != ' ' && in.peek() != '\t' &&
            !std::isalpha(static_cast<unsigned char>(in.peek())) && !two_digits(om) &&
            !(colon && one_digit(om)))
          return std::nullopt;
        // Minutes 60-99 are dateutil's: they add to the hours ("+01:90" is
        // +02:30) while the whole offset stays below 24 hours ("+23:60"
        // fails); the ISO path refuses them, so the fraction is truncated.
        if (om > 59) iso_path = false;
        const unsigned total = oh * 60 + om;
        if (oh > 23 || om > 99 || total >= 24 * 60) return std::nullopt;
        offset_minutes = (ch == '-' ? -1 : 1) * static_cast<int>(total);
        char buffer[16];
        std::snprintf(buffer, sizeof(buffer), "%c%02u:%02u", ch, total / 60, total % 60);
        offset = buffer;
      } else {
        iso_path = false;
        // Only the uppercase names are zones; "utc" or "Gmt" fail.
        const auto zone_start = in.pos;
        in.word();
        const auto zone = in.text.substr(zone_start, in.pos - zone_start);
        if (zone != "UTC" && zone != "GMT") return std::nullopt;
        offset = "+00:00";
      }
      if (!read_meridiem()) return std::nullopt;
      hour = static_cast<int>(h);
    }
  }
  if (!in.done()) return std::nullopt;
  // A spaced YYYY MM DD that is no date on the ISO path ("0024 11 31") falls
  // back to dateutil too, which then truncates the fraction.
  if (spaced_numbers && (!iso_path || !valid_date())) {
    // dateutil's order for three plain numbers: year-first when the first
    // cannot be a day, day-first when it cannot be a month, else month-first;
    // a year below 100 is two-digit ("0040 09 14T14:41:11 z" is 2040,
    // "0024 11 31" is 2031-11-24).
    iso_path = false;
    const auto [a, b, c] = *spaced_numbers;
    const auto year_of = [](unsigned value) {
      return value < 100 ? expand_two_digit_year(value) : static_cast<int>(value);
    };
    if (a > 31) {
      year = year_of(a); month = static_cast<int>(b); day = static_cast<int>(c);
    } else if (a > 12) {
      day = static_cast<int>(a); month = static_cast<int>(b); year = year_of(c);
    } else {
      month = static_cast<int>(a); day = static_cast<int>(b); year = year_of(c);
    }
  }
  if (spaced_numbers && !valid_date()) return std::nullopt;

  // pandas accepts offsets up to 23:59 where the native timestamp grammar
  // stops at 14:00; such a cell is stored as its UTC instant. When that
  // instant leaves years 1..9999 (pandas still holds it), the same instant is
  // stored at the nearest offset the grammar allows, -14:00 or +14:00, if
  // its wall clock fits; otherwise the cell counts as unconvertible.
  if (offset_minutes > 14 * 60 || offset_minutes < -14 * 60) {
    using namespace std::chrono;
    const auto wall = sys_days{std::chrono::year{year} / month / day} + hours{hour} +
                      minutes{minute};
    const auto set_clock = [&](sys_time<minutes> local, int stored_offset) {
      const auto date = floor<days>(local);
      const year_month_day ymd{date};
      const hh_mm_ss clock{local - date};
      const int stored_year = static_cast<int>(ymd.year());
      if (stored_year < 1 || stored_year > 9999) return false;
      year = stored_year;
      month = static_cast<int>(static_cast<unsigned>(ymd.month()));
      day = static_cast<int>(static_cast<unsigned>(ymd.day()));
      hour = static_cast<int>(clock.hours().count());
      minute = static_cast<int>(clock.minutes().count());
      char text[8];
      std::snprintf(text, sizeof(text), "%c%02d:00", stored_offset < 0 ? '-' : '+',
                    std::abs(stored_offset) / 60);
      offset = text;
      return true;
    };
    const auto utc = wall - minutes{offset_minutes};
    const int nearest = offset_minutes < 0 ? -14 * 60 : 14 * 60;
    if (!set_clock(utc, 0) && !set_clock(utc + minutes{nearest}, nearest))
      return std::nullopt;
  }
  // Off the ISO path pandas keeps microseconds only, unless a `.` fraction's
  // microseconds end in three zeros: then its nanosecond digits are kept too
  // ("2024-01-01t10:00:00.123000456" holds .123000456, ".1234567" .123456).
  if (!iso_path && fraction.size() > 6 &&
      (comma_fraction || fraction.compare(3, 3, "000") != 0))
    fraction.resize(6);

  char buffer[40];
  std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d", year,
                month, day, hour, minute, second);
  std::string out = buffer;
  if (!fraction.empty()) out += "." + fraction;
  return NormalizedTimestamp{out + offset, iso_path, fraction.size() > 6};
}

// dateutil's reading of a leading all-numeric date of three numbers written
// with `-`, `/` and single spaces, for the long or mixed spellings the shapes
// above leave out ("2024-1/00015", "02 0007 2031", "2057 004 22", "07-0016
// 24"; review X1).
// Numbers joined by `-` or `/` are kept as text, and one of more than two
// digits is the year (its century given); a mix of the two separators in one
// token leaves the last number standing alone. A standalone number is read
// by value, and one above 100 is the year. More than one year fails. The
// order is then year-first when the first number cannot be a day or is the
// year, day-first when it cannot be a month, else month-first; a year below
// 100 without its century is two-digit. Returns the date spelled MM/DD/YYYY
// followed by the rest of the text, or nullopt. Numbers of more than five
// digits are left unparsed (a standalone one is a dateutil clock shape;
// declared, review Y1-3). Runs of spaces separate the numbers like single
// ones ("2057  8 31"; review Y1-3).
std::optional<std::string> reference_numeric_date(std::string_view text) {
  std::size_t at = 0;
  while (at < text.size() && text[at] == ' ') ++at;
  struct Component {
    unsigned value;
    bool year;
  };
  std::vector<Component> components;
  const auto number = [&](unsigned& value, std::size_t& width) {
    const auto begin = at;
    value = 0;
    while (at < text.size() && std::isdigit(static_cast<unsigned char>(text[at]))) {
      if (at - begin >= 5) return false;
      value = value * 10 + static_cast<unsigned>(text[at] - '0');
      ++at;
    }
    width = at - begin;
    return width > 0;
  };
  while (components.size() < 3) {
    if (!components.empty()) {
      if (at >= text.size() || text[at] != ' ') return std::nullopt;
      while (at < text.size() && text[at] == ' ') ++at;
    }
    std::array<unsigned, 3> values{};
    std::array<std::size_t, 3> widths{};
    std::array<char, 2> separators{};
    std::size_t count = 0;
    if (!number(values[0], widths[0])) return std::nullopt;
    count = 1;
    while (count < 3 && at < text.size() && (text[at] == '-' || text[at] == '/')) {
      separators[count - 1] = text[at];
      ++at;
      if (!number(values[count], widths[count])) return std::nullopt;
      ++count;
    }
    if (components.size() + count > 3) return std::nullopt;
    for (std::size_t i = 0; i < count; ++i) {
      const bool standalone = count == 1 || (count == 3 && i == 2 && separators[0] != separators[1]);
      components.push_back(
          {values[i], standalone ? values[i] > 100 : widths[i] > 2});
    }
  }
  if (at < text.size() && text[at] != ' ' && text[at] != '\t' && text[at] != 'T' &&
      text[at] != 't')
    return std::nullopt;
  std::size_t years = 0, year_index = 3;
  for (std::size_t i = 0; i < 3; ++i)
    if (components[i].year) { ++years; year_index = i; }
  if (years > 1) return std::nullopt;
  const unsigned a = components[0].value, b = components[1].value, c = components[2].value;
  unsigned y = 0, m = 0, d = 0;
  if (a > 31 || year_index == 0) { y = a; m = b; d = c; }
  else if (a > 12) { d = a; m = b; y = c; }
  else { m = a; d = b; y = c; }
  const int year = y < 100 && years == 0 ? expand_two_digit_year(y) : static_cast<int>(y);
  if (year < 1 || year > 9999 || m < 1 || m > 12 || d < 1 ||
      static_cast<int>(d) > month_days(year, static_cast<int>(m)))
    return std::nullopt;
  char buffer[32];
  std::snprintf(buffer, sizeof(buffer), "%02u/%02u/%04d", m, d, year);
  return std::string(buffer) + std::string(text.substr(at));
}

// A basic YYYYMMDD date followed by a clock after `T`, `t` or white space
// ("20240105 11:07:52", "20240105T1107"): pandas reads it like YYYY-MM-DD
// with that clock, nanoseconds included, but refuses a `,` fraction
// ("20240105 11:07:52,5") and a date alone (review Y1-3). Returns the cell
// with the date spelled YYYY-MM-DD, or nullopt.
std::optional<std::string> reference_basic_date(std::string_view text) {
  if (text.size() < 10) return std::nullopt;
  for (std::size_t i = 0; i < 8; ++i)
    if (!std::isdigit(static_cast<unsigned char>(text[i]))) return std::nullopt;
  if (text[8] != ' ' && text[8] != 'T' && text[8] != 't') return std::nullopt;
  if (text.find(',') != std::string_view::npos) return std::nullopt;
  std::string out;
  out.reserve(text.size() + 2);
  out.append(text.substr(0, 4)).push_back('-');
  out.append(text.substr(4, 2)).push_back('-');
  out.append(text.substr(6));
  return out;
}

std::optional<NormalizedTimestamp> normalize_reference_timestamp(std::string_view text,
                                                                 bool allow_year_zero = false) {
  if (auto shaped = normalize_reference_timestamp_shapes(text, allow_year_zero)) return shaped;
  if (const auto basic = reference_basic_date(text))
    return normalize_reference_timestamp_shapes(*basic, allow_year_zero);
  const auto date = reference_numeric_date(text);
  if (!date) return std::nullopt;
  auto shaped = normalize_reference_timestamp_shapes(*date);
  if (shaped) shaped->iso_path = false;
  return shaped;
}

// The digits of the seconds fraction of a timestamp text: those after the
// first '.' or ',' that follows the clock's first ':' (a date may use '.' as
// its separator, "2024.10.09 06:17:12.186788426").
std::size_t timestamp_fraction_digits(std::string_view text) {
  const auto colon = text.find(':');
  if (colon == std::string_view::npos) return 0;
  const auto dot = text.find_first_of(".,", colon);
  if (dot == std::string_view::npos) return 0;
  std::size_t end = dot + 1;
  while (end < text.size() && std::isdigit(static_cast<unsigned char>(text[end]))) ++end;
  return end - dot - 1;
}

// The stored text of a present CSV cell, and whether pandas needs nanosecond
// resolution for it (an ISO-path cell with more than six fraction digits).
struct ReferenceTimestamp {
  std::string text;
  bool nanosecond_fraction = false;
};

// Returns the timestamp to store for a present CSV cell, or nullopt when the
// reference conversion would fail for it.
// `padded` tells that the source cell ended in white space the importer
// trimmed, which matters after an hour-only offset.
std::optional<ReferenceTimestamp> reference_timestamp(const std::string& value,
                                                      bool padded = false) {
  const auto digits = timestamp_fraction_digits(value);
  if (const auto exact = parse_timestamp_exact(value)) {
    // Year 0 (a wall clock before 0001-01-01) is held only on pandas' ISO
    // path; dateutil rejects it, so a lowercase `t` or `z` or an hour-only
    // offset before white space makes the cell unconvertible.
    constexpr std::int64_t kYearOneSeconds = -62135596800;
    if (exact->seconds + std::int64_t{exact->offset_minutes} * 60 < kYearOneSeconds) {
      const auto zero = normalize_reference_timestamp(padded ? value + " " : value, true);
      if (zero && !zero->iso_path) return std::nullopt;
    }
    if (digits <= 6) return ReferenceTimestamp{value, false};
    // A shape outside the ISO path (a lowercase `t` or `z`, an hour-only
    // offset before white space) goes through dateutil, which keeps
    // microseconds only (but see the zero-microseconds rule in
    // normalize_reference_timestamp); the normalized spelling truncates it.
    auto normalized = normalize_reference_timestamp(padded ? value + " " : value);
    if (!normalized || normalized->iso_path || !parse_timestamp_exact(normalized->text))
      return ReferenceTimestamp{value, true};
    return ReferenceTimestamp{std::move(normalized->text), normalized->nanosecond};
  }
  auto normalized = normalize_reference_timestamp(padded ? value + " " : value);
  if (normalized && parse_timestamp_exact(normalized->text))
    return ReferenceTimestamp{std::move(normalized->text), normalized->nanosecond};
  return std::nullopt;
}

// pandas holds nanosecond timestamps in int64: from 1677-09-21
// 00:12:43.145224193 to 2262-04-11 23:47:16.854775807. Both the wall clock
// and the UTC instant of a cell must fit once the column needs them.
bool outside_nanosecond_range(const std::string& text) {
  const auto parsed = parse_timestamp_exact(text);
  if (!parsed) return false;
  constexpr std::int64_t kMinSeconds = -9223372037, kMaxSeconds = 9223372036;
  const auto outside = [](std::int64_t seconds, std::uint32_t nanoseconds) {
    return seconds < kMinSeconds || (seconds == kMinSeconds && nanoseconds < 145224193u) ||
           seconds > kMaxSeconds || (seconds == kMaxSeconds && nanoseconds > 854775807u);
  };
  return outside(parsed->seconds, parsed->nanoseconds) ||
         outside(parsed->seconds + std::int64_t{parsed->offset_minutes} * 60,
                 parsed->nanoseconds);
}

// --- Reference case-id typing ------------------------------------------------
// read_csv + format_dataframe identify a case by the text of its case-id cell
// after pandas has typed the whole column and format_dataframe has converted
// it to strings. The rules below reproduce that observed behaviour (pandas
// 3.0.5, checked by differential fuzzing): "007", "+7" and "7" are one case
// "7"; an NA cell turns an integer column into floats ("1.0"); floats print
// as Python's repr; true/false spellings print as "True"/"False"; anything
// else keeps its text. Which cells count as missing also depends on the type.

bool is_space_ascii(char ch) {
  return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\v' || ch == '\f' ||
         ch == '\r';
}

bool is_digit_ascii(char ch) { return ch >= '0' && ch <= '9'; }

bool equals_ascii_ignoring_case(std::string_view text, std::string_view lower) {
  if (text.size() != lower.size()) return false;
  for (std::size_t i = 0; i < text.size(); ++i)
    if (std::tolower(static_cast<unsigned char>(text[i])) != lower[i])
      return false;
  return true;
}

// An integer-shaped cell: optional whitespace, sign and digits, then optional
// whitespace. A digit run too large for the target range counts as an
// overflow only when nothing (not even whitespace) follows it.
enum class IntegerCell { kNotInteger, kFits, kOverflow };

struct IntegerToken {
  bool negative = false;
  std::string_view digits;
  bool trailing = false;  // anything after the digit run
  bool tail_is_space = true;
};

std::optional<IntegerToken> integer_token(std::string_view text,
                                          bool allow_minus) {
  std::size_t pos = 0;
  while (pos < text.size() && is_space_ascii(text[pos])) ++pos;
  IntegerToken token;
  if (pos < text.size() && (text[pos] == '+' || (allow_minus && text[pos] == '-'))) {
    token.negative = text[pos] == '-';
    ++pos;
  }
  const auto start = pos;
  while (pos < text.size() && is_digit_ascii(text[pos])) ++pos;
  if (pos == start) return std::nullopt;
  token.digits = text.substr(start, pos - start);
  token.trailing = pos < text.size();
  for (auto i = pos; i < text.size(); ++i)
    if (!is_space_ascii(text[i])) token.tail_is_space = false;
  return token;
}

// The digit run's value when it does not exceed `limit`.
std::optional<std::uint64_t> bounded_value(std::string_view digits,
                                           std::uint64_t limit) {
  std::uint64_t value = 0;
  for (const char ch : digits) {
    const auto digit = static_cast<std::uint64_t>(ch - '0');
    if (value > (limit - digit) / 10) return std::nullopt;
    value = value * 10 + digit;
  }
  return value;
}

IntegerCell signed_cell(std::string_view text, std::int64_t& value) {
  const auto token = integer_token(text, true);
  if (!token || !token->tail_is_space) return IntegerCell::kNotInteger;
  constexpr std::uint64_t kMinMagnitude = std::uint64_t{1} << 63;
  const auto magnitude = bounded_value(
      token->digits, token->negative ? kMinMagnitude : kMinMagnitude - 1);
  if (!magnitude)
    return token->trailing ? IntegerCell::kNotInteger : IntegerCell::kOverflow;
  if (!token->negative)
    value = static_cast<std::int64_t>(*magnitude);
  else
    value = *magnitude == kMinMagnitude ? INT64_MIN
                                        : -static_cast<std::int64_t>(*magnitude);
  return IntegerCell::kFits;
}

// Unsigned reading used once a column has a too-large signed integer. A cell
// beginning with '-' is only noted as signed.
struct UnsignedScan {
  bool saw_minus = false;
  bool saw_above_int64 = false;
  bool saw_na = false;
};

IntegerCell unsigned_cell(std::string_view text, UnsignedScan& scan,
                          std::uint64_t& value) {
  const auto first = text.find_first_not_of(" \t\n\v\f\r");
  if (first != std::string_view::npos && text[first] == '-') {
    scan.saw_minus = true;
    value = 0;
    return IntegerCell::kFits;
  }
  const auto token = integer_token(text, false);
  if (!token || !token->tail_is_space) return IntegerCell::kNotInteger;
  const auto magnitude = bounded_value(token->digits, UINT64_MAX);
  if (!magnitude)
    return token->trailing ? IntegerCell::kNotInteger : IntegerCell::kOverflow;
  value = *magnitude;
  if (value > static_cast<std::uint64_t>(INT64_MAX)) scan.saw_above_int64 = true;
  return IntegerCell::kFits;
}

// Python int() syntax in base 10: whitespace, sign, digits with single
// underscores between them. Returns the int's decimal text.
std::optional<std::string> python_int_text(std::string_view text) {
  const auto first = text.find_first_not_of(" \t\n\v\f\r");
  if (first == std::string_view::npos) return std::nullopt;
  const auto last = text.find_last_not_of(" \t\n\v\f\r");
  auto body = text.substr(first, last - first + 1);
  bool negative = false;
  if (body.front() == '+' || body.front() == '-') {
    negative = body.front() == '-';
    body.remove_prefix(1);
  }
  if (body.empty() || body.front() == '_' || body.back() == '_')
    return std::nullopt;
  std::string digits;
  for (std::size_t i = 0; i < body.size(); ++i) {
    if (body[i] == '_') {
      if (body[i - 1] == '_') return std::nullopt;
    } else if (is_digit_ascii(body[i])) {
      digits.push_back(body[i]);
    } else {
      return std::nullopt;
    }
  }
  if (digits.size() > 4300) return std::nullopt;  // Python's digit limit
  const auto nonzero = digits.find_first_not_of('0');
  if (nonzero == std::string::npos) return std::string("0");
  return (negative ? "-" : "") + digits.substr(nonzero);
}

const std::array<double, 309>& decimal_powers() {
  static const std::array<double, 309> powers = [] {
    std::array<double, 309> values{};
    for (std::size_t i = 0; i < values.size(); ++i) {
      const std::string literal = "1e" + std::to_string(i);
      std::from_chars(literal.data(), literal.data() + literal.size(), values[i]);
    }
    return values;
  }();
  return powers;
}

// The double pandas' default float reading gives a cell, or nullopt.
// Observed rules: at most 17 significant digits are kept, counted from the
// first written digit (so leading zeros use up the budget); further integer
// digits only scale the value and further fraction digits are dropped. The
// kept digits are folded into a double one at a time and then scaled by the
// remaining power of ten, which is why "0.30000000000000004" reads as 0.3.
std::optional<double> pandas_float_value(std::string_view text) {
  struct Lexed {
    bool negative = false;
    std::string kept;     // significant digits retained
    long scale = 0;       // power of ten applied to `kept`
    bool any_digit = false;
  } lexed;
  std::size_t pos = 0;
  const auto peek = [&] { return pos < text.size() ? text[pos] : '\0'; };
  while (is_space_ascii(peek())) ++pos;
  if (peek() == '+' || peek() == '-') {
    lexed.negative = peek() == '-';
    ++pos;
  }
  constexpr std::size_t kKept = 17;
  for (; is_digit_ascii(peek()); ++pos) {
    lexed.any_digit = true;
    if (lexed.kept.size() < kKept) lexed.kept.push_back(peek());
    else ++lexed.scale;
  }
  if (peek() == '.') {
    ++pos;
    for (; is_digit_ascii(peek()); ++pos) {
      lexed.any_digit = true;
      if (lexed.kept.size() < kKept) {
        lexed.kept.push_back(peek());
        --lexed.scale;
      }
    }
  }
  if (!lexed.any_digit) return std::nullopt;
  if (peek() == 'e' || peek() == 'E') {
    // An exponent may be preceded by whitespace; without digits the 'e' is
    // left unread (and the cell then fails).
    auto q = pos + 1;
    while (q < text.size() && std::isspace(static_cast<unsigned char>(text[q]))) ++q;
    bool exponent_negative = false;
    if (q < text.size() && (text[q] == '+' || text[q] == '-')) {
      exponent_negative = text[q] == '-';
      ++q;
    }
    const auto digits_start = q;
    long exponent = 0;
    constexpr long kSaturated = 1000000;  // far beyond any finite result
    for (; q < text.size() && is_digit_ascii(text[q]); ++q)
      exponent = std::min(kSaturated, exponent * 10 + (text[q] - '0'));
    if (q > digits_start) {
      lexed.scale += exponent_negative ? -exponent : exponent;
      pos = q;
    }
  }
  while (is_space_ascii(peek())) ++pos;
  if (pos < text.size()) return std::nullopt;

  double value = 0.;
  for (const char digit : lexed.kept) value = value * 10. + (digit - '0');
  if (lexed.negative) value = -value;
  const auto& powers = decimal_powers();
  if (lexed.scale > 308) {
    if (value != 0) value = value < 0 ? -HUGE_VAL : HUGE_VAL;
    else value = 0.;
  } else if (lexed.scale >= 0) {
    if (lexed.scale > 0) value *= powers[static_cast<std::size_t>(lexed.scale)];
  } else if (lexed.scale >= -308) {
    value /= powers[static_cast<std::size_t>(-lexed.scale)];
  } else if (lexed.scale >= -616) {
    value /= powers[static_cast<std::size_t>(-308 - lexed.scale)];
    value /= powers[308];
  } else {
    value = 0.;
  }
  return value;
}

std::optional<double> pandas_float_cell(std::string_view text) {
  if (auto value = pandas_float_value(text)) return value;
  for (const auto spelling : {"inf", "+inf", "infinity", "+infinity"})
    if (equals_ascii_ignoring_case(text, spelling)) return HUGE_VAL;
  for (const auto spelling : {"-inf", "-infinity"})
    if (equals_ascii_ignoring_case(text, spelling)) return -HUGE_VAL;
  return std::nullopt;
}

std::string python_float_text(double value) {
  if (std::isinf(value)) return value > 0 ? "inf" : "-inf";
  return filtering_float_repr(value);
}

// The text each cell of one column has after read_csv's typing and
// format_dataframe's string conversion (used for the case-id and activity
// columns); nullopt is a missing cell, whose row the reference drops.
// `values`, when given, marks cells that are never NA: an NA spelling that
// only trimming exposed (" NA"), which read_csv keeps as text (review Z1-4).
ReferenceCsvColumn infer_reference_column(
    const std::vector<std::string>& cells,
    const std::vector<char>* values = nullptr) {
  const auto count = cells.size();
  using Column = std::vector<std::optional<std::string>>;
  using Kind = ReferenceCsvColumnKind;
  const auto na = [&](std::size_t i) {
    return !(values && (*values)[i] != 0) && is_reference_na_token(cells[i]);
  };
  // Every cell's text as written, NA spellings included.
  const auto literal = [&] { return Column(cells.begin(), cells.end()); };
  // A column read as Python ints (NA cells missing), when every cell is one.
  const auto python_ints = [&]() -> std::optional<Column> {
    Column column(count);
    for (std::size_t i = 0; i < count; ++i) {
      if (na(i)) continue;
      auto text = python_int_text(cells[i]);
      if (!text) return std::nullopt;
      column[i] = std::move(*text);
    }
    return column;
  };

  // Signed 64-bit integers: the first non-NA cell that is not one decides.
  bool integer_overflow = false;
  {
    std::vector<std::int64_t> values(count);
    std::size_t missing = 0;
    std::optional<IntegerCell> first_failure;
    for (std::size_t i = 0; i < count && !first_failure; ++i) {
      if (na(i)) {
        ++missing;
        continue;
      }
      const auto cell = signed_cell(cells[i], values[i]);
      if (cell != IntegerCell::kFits) first_failure = cell;
    }
    if (!first_failure) {
      Column column(count);
      for (std::size_t i = 0; i < count; ++i) {
        if (na(i)) continue;
        if (missing == 0) {
          column[i] = std::to_string(values[i]);
        } else if (values[i] != INT64_MIN) {
          // NA upcasts the column to float; INT64_MIN reads as NA too.
          column[i] = python_float_text(static_cast<double>(values[i]));
        }
      }
      return {missing == 0 ? Kind::kInteger : Kind::kFloat, std::move(column)};
    }
    integer_overflow = *first_failure == IntegerCell::kOverflow;
  }

  // A too-large signed integer: read the column as unsigned 64-bit integers.
  if (integer_overflow) {
    UnsignedScan scan;
    std::vector<std::uint64_t> values(count);
    std::optional<IntegerCell> first_failure;
    for (std::size_t i = 0; i < count && !first_failure; ++i) {
      if (na(i)) {
        scan.saw_na = true;
        continue;
      }
      const auto cell = unsigned_cell(cells[i], scan, values[i]);
      if (cell != IntegerCell::kFits) first_failure = cell;
    }
    // Python ints and literal text both make an object column.
    const auto ints_or_literal = [&] {
      if (auto column = python_ints()) return ReferenceCsvColumn{Kind::kObject, *column};
      return ReferenceCsvColumn{Kind::kObject, literal()};
    };
    if (first_failure == IntegerCell::kOverflow) return ints_or_literal();
    if (!first_failure) {
      // Values above the signed range cannot share a column with NA or
      // negative cells: the column then keeps its text unfiltered.
      if (scan.saw_above_int64 && (scan.saw_na || scan.saw_minus))
        return {Kind::kObject, literal()};
      if (scan.saw_minus) return ints_or_literal();
      Column column(count);
      for (std::size_t i = 0; i < count; ++i) column[i] = std::to_string(values[i]);
      return {Kind::kInteger, std::move(column)};
    }
    // Otherwise a non-integer cell: continue with floats.
  }

  // Floats.
  {
    Column column(count);
    bool all = true;
    for (std::size_t i = 0; i < count && all; ++i) {
      if (na(i)) continue;
      if (const auto value = pandas_float_cell(cells[i]))
        column[i] = python_float_text(*value);
      else
        all = false;
    }
    if (all) return {Kind::kFloat, std::move(column)};
  }

  // Booleans (any letter case).
  {
    Column column(count);
    bool all = true;
    for (std::size_t i = 0; i < count && all; ++i) {
      if (na(i)) continue;
      if (equals_ascii_ignoring_case(cells[i], "true")) column[i] = "True";
      else if (equals_ascii_ignoring_case(cells[i], "false")) column[i] = "False";
      else all = false;
    }
    if (all) return {Kind::kBoolean, std::move(column)};
  }

  // Text, NA cells missing.
  Column column(count);
  for (std::size_t i = 0; i < count; ++i)
    if (!na(i)) column[i] = cells[i];
  return {Kind::kText, std::move(column)};
}

}  // namespace

namespace {

CsvEventData import_csv_event_data_impl(
    std::istream& input,
    const CsvImportOptions& options,
    bool preserve_table) {
  if (!options.has_header) {
    throw std::invalid_argument("CSV import currently requires a header row");
  }
  if (options.delimiter == '"') {
    throw std::invalid_argument("CSV delimiter cannot be a quote character");
  }

  // The batch import (read_csv + format_dataframe) infers the case-id and
  // activity columns' types from every data row before any row is dropped,
  // and its missing-value handling depends on those types; rows are therefore
  // collected first and placed once the values are known. Stream readers
  // (keep_na_first_class_values) keep the literal cell text in file order.
  const bool reference_batch = !options.keep_na_first_class_values;

  std::string line;
  std::size_t line_number = 1;
  // Whether the last record was a blank line the batch import skipped and a
  // lone "\r" ended (see apply_lone_cr_blank_quirk).
  bool lone_cr = false, after_lone_cr_blank = false;
  const auto read_record = [&]() {
    if (!read_csv_record(input, line, options.delimiter, &lone_cr)) return false;
    if (after_lone_cr_blank) apply_lone_cr_blank_quirk(line, options.delimiter, line_number + 1);
    after_lone_cr_blank = false;
    return true;
  };
  const auto skipped_blank = [&]() {
    after_lone_cr_blank = reference_batch && lone_cr;
  };
  if (!read_record()) {
    return {};
  }
  // Strip a leading UTF-8 byte-order mark so the first column name still
  // matches; spreadsheet exports routinely prepend one. read_csv drops it
  // only at the start of the file: a BOM line is then blank, and a BOM after
  // a blank line stays in the first column name.
  if (line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF &&
      static_cast<unsigned char>(line[1]) == 0xBB &&
      static_cast<unsigned char>(line[2]) == 0xBF) {
    line.erase(0, 3);
  }
  // read_csv skips blank lines before the header too.
  while (reference_batch && is_reference_blank_line(line, options.delimiter)) {
    skipped_blank();
    if (!read_record()) return {};
    ++line_number;
  }

  // The batch import names the columns as read_csv does: an empty name is
  // "Unnamed: i" and repeats become "name.1", ... (a header starting or
  // ending with the delimiter, a pandas export's index column), so those
  // columns are attributes under that name and a selector finds the first
  // of repeated names. With trim_fields a name of only whitespace is empty.
  // Stream readers (csv.DictReader) keep the header as written.
  auto header = parse_csv_row(line, options.delimiter, options.trim_fields);
  if (reference_batch) header = reference_csv_header_names(std::move(header));
  auto columns = header_index(header);
  std::optional<std::size_t> case_alias_index;
  if (options.allow_case_id_aliases && !columns.contains(options.case_id_column)) {
    const auto alias = options.case_id_column == "case_id" ? "case:concept:name"
        : options.case_id_column == "case:concept:name" ? "case_id" : "";
    if (*alias != '\0') case_alias_index = optional_selector_column(columns, alias);
  }
  const auto case_index = case_alias_index.has_value()
      ? *case_alias_index : required_column(columns, options.case_id_column);
  const auto activity_index =
      required_selector_column(columns, options.activity_column);
  auto timestamp_index =
      optional_selector_column(columns, options.timestamp_column);
  if (!timestamp_index && options.require_timestamp_column) {
    throw std::invalid_argument(
        "CSV column not found: " + options.timestamp_column);
  }
  auto resource_index =
      optional_selector_column(columns, options.resource_column);
  auto lifecycle_index =
      optional_selector_column(columns, options.lifecycle_column);

  // Every remaining named column becomes a generic event attribute, keyed by
  // its header, so domain data (channel, amount, region, …) survives import —
  // except columns prefixed `case:`, which are case attributes: constant per
  // case (a repeated flat-file export of e.g. a requested amount), stored once
  // on the trace under the unprefixed key.
  constexpr std::string_view kCasePrefix = "case:";
  std::vector<std::size_t> attribute_indices;
  std::vector<std::size_t> case_attribute_indices;
  for (std::size_t i = 0; i < header.size(); ++i) {
    if (i == case_index || i == activity_index) continue;
    if (timestamp_index && i == *timestamp_index) continue;
    if (resource_index && i == *resource_index) continue;
    if (lifecycle_index && i == *lifecycle_index) continue;
    if (header[i].empty()) continue;
    if (header[i].size() > kCasePrefix.size() &&
        std::string_view(header[i]).substr(0, kCasePrefix.size()) ==
            kCasePrefix) {
      case_attribute_indices.push_back(i);
    } else {
      attribute_indices.push_back(i);
    }
  }

  CsvEventData result;
  EventLog& log = result.log;
  EventTable& table = result.table;
  if (preserve_table) {
    table.columns = header;
    // CSV keeps every source column as a string. Timestamp-role cells use a
    // date kind marker for first-class selection, but no datetime value is
    // inferred from the textual CSV payload.
    table.dtypes.assign(header.size(), "str");
  }

  struct PendingRow {
    std::size_t line_number = 0;
    std::size_t token = 0;
    // The timestamp cell held only whitespace, which trim_fields blanked.
    bool blank_timestamp = false;
    // The timestamp cell ended in white space that trim_fields removed.
    bool padded_timestamp = false;
    Event event;
    std::vector<EventAttribute> case_attributes;
    EventTable::Row table_row;
  };
  std::vector<std::string> case_tokens;
  std::vector<std::string> activity_tokens;
  // Per token: an NA spelling only after trimming, so a value (review Z1-4).
  std::vector<char> case_padded_na;
  std::vector<char> activity_padded_na;
  std::vector<PendingRow> pending;

  // Row shape in the batch import follows read_csv's default C parser. The
  // first data row fixes the expected field count: when it has k fields more
  // than the header, read_csv reads the first k fields of every row as the
  // (implicit) index, which format_dataframe then discards, so the header
  // names the fields after them. A later row with more fields than that is
  // an error ("Expected N fields in line L, saw M", L counting records,
  // blank ones included, as line_number does); a shorter row reads NaN in
  // its missing trailing columns. Stream readers (csv.DictReader) keep their
  // own shape rules: extra fields are ignored and a short row must still
  // hold the first-class columns.
  std::optional<std::size_t> expected_fields;
  std::size_t index_fields = 0;
  const auto cell_at = [&](const std::vector<std::string>& fields,
                           std::size_t index) {
    if (reference_batch) return index < fields.size() ? fields[index] : std::string{};
    return field_at(fields, index, line_number);
  };

  while (read_record()) {
    ++line_number;
    if (line.empty() ||
        (reference_batch && is_reference_blank_line(line, options.delimiter))) {
      skipped_blank();
      continue;
    }

    // to_datetime cannot convert a whitespace-only timestamp (it is text to
    // the reference), so the batch import tells it apart from an empty cell
    // before trim_fields blanks it.
    const bool untrimmed_timestamp =
        reference_batch && options.trim_fields && timestamp_index.has_value();
    // The batch import trims after reading the row, so that a cell which is
    // an NA spelling only once trimmed (" NA", "None ") stays a value, as the
    // untrimmed text is for read_csv (review Z1-4).
    const bool trim_after_read = reference_batch && options.trim_fields;
    auto fields = parse_csv_row(line, options.delimiter,
                                options.trim_fields && !trim_after_read);
    if (reference_batch) {
      if (!expected_fields) {
        index_fields = fields.size() > header.size() ? fields.size() - header.size() : 0;
        expected_fields = header.size() + index_fields;
      }
      if (fields.size() > *expected_fields) {
        const auto count = [](std::size_t n) {
          return std::to_string(n) + (n == 1 ? " field" : " fields");
        };
        std::string message =
            "CSV line " + std::to_string(line_number) + " has " + count(fields.size()) +
            ", but rows of this file have at most " + count(*expected_fields);
        if (index_fields > 0) {
          message += " (the first data row has " + count(index_fields) +
                     " more than the header, which read_csv reads as an index)";
        } else {
          message += ", as many as the header";
        }
        message += "; pandas.read_csv refuses the file: Error tokenizing data. C error: Expected " +
                   std::to_string(*expected_fields) + " fields in line " +
                   std::to_string(line_number) + ", saw " + std::to_string(fields.size());
        throw std::invalid_argument(message);
      }
      fields.erase(fields.begin(),
                   fields.begin() + static_cast<std::ptrdiff_t>(
                                        std::min(index_fields, fields.size())));
    }
    bool blank_timestamp = false;
    bool padded_timestamp = false;
    if (untrimmed_timestamp && *timestamp_index < fields.size()) {
      const auto& cell = fields[*timestamp_index];
      blank_timestamp = !cell.empty() && cell.find_first_not_of(" \t\r\n") == std::string::npos;
      padded_timestamp = !cell.empty() && (cell.back() == ' ' || cell.back() == '\t');
    }
    // Fields whose text is an NA spelling only after trimming.
    std::vector<char> padded_na;
    if (trim_after_read) {
      padded_na.assign(fields.size(), 0);
      for (std::size_t i = 0; i < fields.size(); ++i) {
        auto trimmed = trim(fields[i]);
        padded_na[i] = trimmed.size() != fields[i].size() && !trimmed.empty() &&
                       is_reference_na_token(trimmed);
        fields[i] = std::move(trimmed);
      }
    }
    const auto padded_na_at = [&](std::size_t index) {
      return index < padded_na.size() && padded_na[index] != 0;
    };
    auto case_id = cell_at(fields, case_index);
    auto activity = cell_at(fields, activity_index);
    auto timestamp = timestamp_index ? cell_at(fields, *timestamp_index)
                                     : std::string{};
    auto resource = resource_index ? cell_at(fields, *resource_index)
                                   : std::string{};
    // read_csv reads a pandas NA spelling in any column as NaN, so in the
    // batch import resource, lifecycle and extra attribute cells holding one
    // are missing like blank cells (stream readers keep the literal text).
    const auto na_attribute_cell = [&](std::size_t index, const std::string& cell) {
      return reference_batch && !padded_na_at(index) && is_reference_na_token(cell);
    };
    if (resource_index && na_attribute_cell(*resource_index, resource)) resource.clear();

    const std::size_t token = case_tokens.size();
    case_tokens.push_back(case_id);
    activity_tokens.push_back(activity);
    case_padded_na.push_back(padded_na_at(case_index) ? 1 : 0);
    activity_padded_na.push_back(padded_na_at(activity_index) ? 1 : 0);
    // Whether an NA case id or activity is missing (the reference drops the
    // row) depends on the inferred column type, decided below. Whitespace-only
    // text is a value there, but the default trim_fields option has already
    // blanked it here. Stream readers keep NA spellings as literal values
    // (keep_na_first_class_values).
    PendingRow row;
    row.line_number = line_number;
    row.token = token;
    row.blank_timestamp = blank_timestamp;
    row.padded_timestamp = padded_timestamp;
    for (const auto index : case_attribute_indices) {
      // The batch import keeps blank cells too: the reference takes a case
      // attribute from one row only, which may be blank (missing).
      // A short row's missing cell is NaN there, like a blank one.
      if (!reference_batch && (index >= fields.size() || fields[index].empty()))
        continue;
      row.case_attributes.push_back(EventAttribute{
          header[index].substr(kCasePrefix.size()), cell_at(fields, index),
          AttributeKind::kString});
    }

    Event event{case_id, activity, timestamp, resource, 0};
    event.case_id_state = FirstClassAttributeState::kPresent;
    event.activity_state = FirstClassAttributeState::kPresent;
    event.case_id_source_key = header[case_index];
    event.activity_source_key = header[activity_index];
    if (timestamp_index) {
      event.timestamp_state = event.timestamp.empty()
                                  ? FirstClassAttributeState::kMissing
                                  : FirstClassAttributeState::kPresent;
      event.timestamp_source_key = header[*timestamp_index];
    }
    if (resource_index) {
      event.resource_state = event.resource.empty()
                                 ? FirstClassAttributeState::kMissing
                                 : FirstClassAttributeState::kPresent;
      event.resource_source_key = header[*resource_index];
    }
    if (lifecycle_index) {
      event.lifecycle = cell_at(fields, *lifecycle_index);
      if (na_attribute_cell(*lifecycle_index, event.lifecycle)) event.lifecycle.clear();
      event.lifecycle_state = event.lifecycle.empty()
                                  ? FirstClassAttributeState::kMissing
                                  : FirstClassAttributeState::kPresent;
      event.lifecycle_source_key = header[*lifecycle_index];
    }
    for (const auto index : attribute_indices) {
      // A blank cell means the event simply has no such attribute; a short row
      // reads the same way rather than erroring, since these columns are extra.
      if (index >= fields.size() || fields[index].empty() ||
          na_attribute_cell(index, fields[index]))
        continue;
      event.attributes.push_back(
          EventAttribute{header[index], fields[index], AttributeKind::kString});
    }
    if (preserve_table) {
      EventTable::Row& table_row = row.table_row;
      table_row.reserve(header.size());
      for (std::size_t index = 0; index < header.size(); ++index) {
        if (index >= fields.size()) {
          table_row.push_back(std::nullopt);
          continue;
        }
        const bool is_timestamp = timestamp_index && index == *timestamp_index;
        EventAttribute cell{header[index], fields[index],
                            is_timestamp ? AttributeKind::kDate
                                         : AttributeKind::kString};
        const bool attribute_column =
            (resource_index && index == *resource_index) ||
            (lifecycle_index && index == *lifecycle_index) ||
            std::find(attribute_indices.begin(), attribute_indices.end(),
                      index) != attribute_indices.end();
        if (attribute_column && na_attribute_cell(index, cell.value)) cell.value.clear();
        const auto set_role = [&](FirstClassAttributeRole role,
                                  FirstClassAttributeState state) {
          cell.first_class_role = role;
          cell.first_class_state = state;
          cell.first_class_source_key = header[index];
        };
        if (index == case_index) {
          set_role(FirstClassAttributeRole::kCaseId,
                   FirstClassAttributeState::kPresent);
        } else if (index == activity_index) {
          set_role(FirstClassAttributeRole::kActivity,
                   FirstClassAttributeState::kPresent);
        } else if (timestamp_index && index == *timestamp_index) {
          set_role(FirstClassAttributeRole::kTimestamp,
                   fields[index].empty() ? FirstClassAttributeState::kMissing
                                         : FirstClassAttributeState::kPresent);
        } else if (resource_index && index == *resource_index) {
          set_role(FirstClassAttributeRole::kResource,
                   cell.value.empty() ? FirstClassAttributeState::kMissing
                                      : FirstClassAttributeState::kPresent);
        } else if (lifecycle_index && index == *lifecycle_index) {
          set_role(FirstClassAttributeRole::kLifecycle,
                   cell.value.empty() ? FirstClassAttributeState::kMissing
                                      : FirstClassAttributeState::kPresent);
        }
        table_row.push_back(std::move(cell));
      }
    }
    row.event = std::move(event);
    pending.push_back(std::move(row));
  }

  // The case id each cell carries after read_csv's dtype inference and
  // format_dataframe's string conversion; nullopt marks a missing case id.
  // The activity column is typed and converted the same way.
  std::vector<std::optional<std::string>> case_ids;
  std::vector<std::optional<std::string>> activities;
  if (reference_batch) {
    case_ids = infer_reference_column(case_tokens, &case_padded_na).text;
    activities = infer_reference_column(activity_tokens, &activity_padded_na).text;
  } else {
    case_ids.assign(case_tokens.begin(), case_tokens.end());
    activities.assign(activity_tokens.begin(), activity_tokens.end());
  }
  case_tokens.clear();
  case_tokens.shrink_to_fit();
  activity_tokens.clear();
  activity_tokens.shrink_to_fit();
  case_padded_na.clear();
  case_padded_na.shrink_to_fit();
  activity_padded_na.clear();
  activity_padded_na.shrink_to_fit();
  // Batch case-attribute cells per kept row (input_order), chosen per case
  // once the events are known.
  std::vector<std::vector<EventAttribute>> row_case_attributes;

  std::unordered_map<std::string, std::size_t> trace_ids;
  std::size_t input_order = 0;
  // Reference-converted timestamp text per kept row (input_order): an empty
  // string for an NA cell, nullopt for a cell the conversion rejects.
  std::vector<std::optional<std::string>> timestamp_texts;
  std::size_t parsed_timestamps = 0;
  std::optional<std::pair<std::size_t, std::string>> first_bad_timestamp;
  // format_dataframe also drops a row whose timestamp is NA once the column
  // converts to dates, as it drops one without a case id or activity. The
  // conversion is decided from every row of the column, rows without a case
  // id or activity too: a cell that parses, or a NaT spelling (to_datetime
  // reads `NaT`, `NAT` and `nat` as missing dates), makes the column dates,
  // and then a cell that cannot be converted (a whitespace-only one too)
  // refuses the frame. A column with no date keeps every row (see the
  // conversion below), and stream readers keep NA spellings as values.
  const auto nat_timestamp = [](std::string_view text) {
    return text == "NaT" || text == "NAT" || text == "nat";
  };
  bool drop_untimed_rows = false;
  std::optional<std::pair<std::size_t, std::string>> first_unconverted_timestamp;
  // to_datetime moves the whole column to nanoseconds when an ISO cell has
  // more than six fraction digits; a date outside that range then fails the
  // conversion and the column stays text.
  bool nanosecond_column = false;
  std::optional<std::pair<std::size_t, std::string>> first_outside_nanoseconds;
  const auto note_timestamp = [&](const ReferenceTimestamp& parsed, std::size_t line,
                                  const std::string& cell) {
    nanosecond_column = nanosecond_column || parsed.nanosecond_fraction;
    if (!first_outside_nanoseconds && outside_nanosecond_range(parsed.text))
      first_outside_nanoseconds = std::make_pair(line, cell);
  };
  if (reference_batch && timestamp_index) {
    for (const auto& row : pending) {
      const auto& timestamp = row.event.timestamp;
      if (row.blank_timestamp) {
        if (!first_unconverted_timestamp)
          first_unconverted_timestamp = std::make_pair(row.line_number, std::string{});
        continue;
      }
      if (is_reference_na_token(timestamp)) continue;
      if (nat_timestamp(timestamp)) {
        drop_untimed_rows = true;
        continue;
      }
      // Once the column holds dates, the scan below parses the kept rows.
      if (drop_untimed_rows && case_ids[row.token] && activities[row.token]) continue;
      if (auto parsed = reference_timestamp(timestamp, row.padded_timestamp)) {
        drop_untimed_rows = true;
        note_timestamp(*parsed, row.line_number, timestamp);
      } else if (!first_unconverted_timestamp)
        first_unconverted_timestamp = std::make_pair(row.line_number, timestamp);
    }
  }
  for (auto& row : pending) {
    const auto& case_id = case_ids[row.token];
    const auto& activity = activities[row.token];
    if (!case_id || !activity) continue;
    if (drop_untimed_rows && (is_reference_na_token(row.event.timestamp) ||
                              nat_timestamp(row.event.timestamp)))
      continue;
    if (timestamp_index) {
      // Decided per column after the scan; see apply below.
      const auto& timestamp = row.event.timestamp;
      if (is_reference_na_token(timestamp)) {
        timestamp_texts.emplace_back(std::string{});
      } else if (auto parsed = reference_timestamp(timestamp, row.padded_timestamp)) {
        note_timestamp(*parsed, row.line_number, timestamp);
        timestamp_texts.push_back(std::move(parsed->text));
        ++parsed_timestamps;
      } else {
        timestamp_texts.emplace_back(std::nullopt);
        if (!first_bad_timestamp)
          first_bad_timestamp = std::make_pair(row.line_number, timestamp);
      }
    }

    auto [it, inserted] = trace_ids.emplace(*case_id, log.traces.size());
    if (inserted) {
      log.traces.push_back(Trace{*case_id, {}, {}});
      auto& inserted_trace = log.traces.back();
      inserted_trace.case_id_state = FirstClassAttributeState::kPresent;
      inserted_trace.case_id_source_key = header[case_index];
    }

    auto& trace = log.traces[it->second];
    if (reference_batch) {
      // The DataFrame keeps every row's case cells: case filters and
      // statistics test them per row (a blank or NA cell is null there).
      row.event.case_row_attributes = row.case_attributes;
      for (auto& cell : row.event.case_row_attributes) {
        if (cell.value.empty() || is_reference_na_token(cell.value)) {
          cell.value.clear();
          cell.is_null = true;
        }
      }
      row_case_attributes.push_back(std::move(row.case_attributes));
    } else {
      for (auto& attribute : row.case_attributes) {
        // The first row carrying a value sets it; later rows of the same case
        // repeat it in a flat file, so they are ignored (as is a conflict).
        if (trace.attribute(attribute.key) != nullptr) continue;
        trace.attributes.push_back(std::move(attribute));
      }
    }

    row.event.case_id = *case_id;
    row.event.activity = *activity;
    row.event.input_order = input_order++;
    if (preserve_table) {
      if (case_index < row.table_row.size() && row.table_row[case_index])
        row.table_row[case_index]->value = *case_id;
      if (activity_index < row.table_row.size() && row.table_row[activity_index])
        row.table_row[activity_index]->value = *activity;
      table.rows.push_back(std::move(row.table_row));
    }
    trace.events.push_back(std::move(row.event));
  }
  pending.clear();
  pending.shrink_to_fit();

  // A column the reference converts (every non-NA cell parses) is stored in
  // its converted spelling. A column mixing dates with unconvertible text is
  // rejected: the reference leaves it textual and its consumers then reject
  // the frame, and keeping it would silently yield untimed events. A column
  // with no date at all keeps its opaque text, the established native
  // contract for non-temporal timestamp selectors.
  if (timestamp_index && (reference_batch ? drop_untimed_rows : parsed_timestamps > 0)) {
    if (first_unconverted_timestamp &&
        (!first_bad_timestamp || first_unconverted_timestamp->first < first_bad_timestamp->first))
      first_bad_timestamp = first_unconverted_timestamp;
    if (reference_batch && nanosecond_column && first_outside_nanoseconds &&
        (!first_bad_timestamp || first_outside_nanoseconds->first < first_bad_timestamp->first))
      throw std::runtime_error(
          "CSV row " + std::to_string(first_outside_nanoseconds->first) + " has a timestamp '" +
          first_outside_nanoseconds->second + "' in column '" + header[*timestamp_index] +
          "' outside the nanosecond range that a fraction of more than six digits gives the "
          "column, so pandas leaves the column unconverted");
    if (first_bad_timestamp) {
      const auto& text = first_bad_timestamp->second;
      throw std::runtime_error(
          "CSV row " + std::to_string(first_bad_timestamp->first) + " has " +
          (text.empty() ? std::string("a whitespace-only timestamp")
                        : "a timestamp '" + text + "'") +
          " in column '" + header[*timestamp_index] +
          "' that cannot be parsed as a date/time");
    }
    for (auto& trace : log.traces) {
      for (auto& event : trace.events) {
        const auto& text = *timestamp_texts[event.input_order];
        event.timestamp = text;
        if (text.empty())
          event.timestamp_state = FirstClassAttributeState::kMissing;
        if (preserve_table) {
          auto& cell = table.rows[event.input_order][*timestamp_index];
          if (cell) {
            cell->value = text;
            if (text.empty())
              cell->first_class_state = FirstClassAttributeState::kMissing;
          }
        }
      }
    }
  }

  // The reference takes every case attribute from the case's first row
  // after sorting by timestamp and row (rows without a timestamp are dropped
  // there, so they count only when no row is timed); a blank or NA cell in
  // that row leaves the attribute missing.
  if (reference_batch && !case_attribute_indices.empty()) {
    for (auto& trace : log.traces) {
      const Event* chosen = trace.events.empty() ? nullptr : &trace.events.front();
      std::optional<ParsedExactTimestamp> chosen_instant;
      const auto earlier = [](const ParsedExactTimestamp& lhs, const ParsedExactTimestamp& rhs) {
        return lhs.seconds != rhs.seconds ? lhs.seconds < rhs.seconds
                                          : lhs.nanoseconds < rhs.nanoseconds;
      };
      for (const auto& event : trace.events) {
        if (!timestamp_index) break;
        const auto instant = parse_timestamp_exact(event.timestamp);
        if (!instant) continue;
        if (!chosen_instant || earlier(*instant, *chosen_instant)) {
          chosen = &event;
          chosen_instant = instant;
        }
      }
      if (!chosen) continue;
      for (auto& attribute : row_case_attributes[chosen->input_order]) {
        if (attribute.value.empty() || is_reference_na_token(attribute.value))
          continue;
        trace.attributes.push_back(std::move(attribute));
      }
    }
  }
  row_case_attributes.clear();

  if (timestamp_index) {
    // format_dataframe sorts the datetime64 column, at nanosecond (or
    // finer-than-double) resolution.
    for (auto& trace : log.traces) {
      order_events_by_exact_time(trace.events);
    }
  }

  // format_dataframe sorts the frame by the string case id (then timestamp
  // and row index), so the converted log lists its cases in code-point order
  // of their ids. UTF-8 byte order is code-point order.
  if (reference_batch) {
    std::stable_sort(log.traces.begin(), log.traces.end(),
                     [](const Trace& left, const Trace& right) {
                       return left.case_id < right.case_id;
                     });
  }

  if (preserve_table) table.validate();
  return result;
}

}  // namespace

std::vector<std::optional<std::string>> reference_csv_column_text(
    const std::vector<std::string>& cells) {
  return infer_reference_column(cells).text;
}

ReferenceCsvColumn reference_csv_column(const std::vector<std::string>& cells) {
  return infer_reference_column(cells);
}

std::optional<ReferenceMixedTimestamp> reference_mixed_timestamp(const std::string& cell) {
  auto parsed = reference_timestamp(cell);
  if (!parsed) return std::nullopt;
  const bool outside = outside_nanosecond_range(parsed->text);
  return ReferenceMixedTimestamp{std::move(parsed->text), parsed->nanosecond_fraction, outside};
}

std::string reference_csv_float_text(double value) {
  return python_float_text(value);
}

std::vector<std::string> reference_csv_header_names(std::vector<std::string> names) {
  // Named columns first, then the unnamed ones, each in header order.
  std::vector<std::size_t> order;
  std::vector<std::size_t> unnamed;
  order.reserve(names.size());
  for (std::size_t i = 0; i < names.size(); ++i) {
    if (names[i].empty()) {
      names[i] = "Unnamed: " + std::to_string(i);
      unnamed.push_back(i);
    } else {
      order.push_back(i);
    }
  }
  order.insert(order.end(), unnamed.begin(), unnamed.end());
  const std::unordered_set<std::string> present(names.begin(), names.end());
  // How many times each name has been taken; a repeat tries name.N from
  // that count on, past every N the header itself already spells.
  std::unordered_map<std::string, std::size_t> counts;
  for (const auto i : order) {
    const std::string base = names[i];
    std::string column = base;
    std::size_t count = counts[base];
    while (count > 0) {
      counts[base] = count + 1;
      column = base + "." + std::to_string(count);
      if (present.contains(column)) {
        ++count;
      } else {
        const auto found = counts.find(column);
        count = found == counts.end() ? 0 : found->second;
      }
    }
    names[i] = column;
    counts[column] = count + 1;
  }
  return names;
}

CsvEventData import_csv_event_data(
    std::istream& input,
    const CsvImportOptions& options) {
  return import_csv_event_data_impl(input, options, true);
}

CsvEventData import_csv_event_data(
    const std::filesystem::path& path,
    const CsvImportOptions& options) {
  std::ifstream input(path);
  if (!input) {
    throw std::runtime_error("Unable to open CSV event log: " + path.string());
  }
  return import_csv_event_data(input, options);
}

EventTable project_csv_event_table(const EventTable& source,
                                   const EventLog& filtered) {
  source.validate();

  struct Candidate {
    std::size_t source_row = 0;
    std::size_t encounter = 0;
    const Event* event = nullptr;
    const Trace* trace = nullptr;
  };

  std::vector<Candidate> candidates;
  candidates.reserve(filtered.event_count());
  std::size_t encounter = 0;
  for (const auto& trace : filtered.traces) {
    for (const auto& event : trace.events) {
      if (event.input_order >= source.rows.size()) {
        throw std::invalid_argument(
            "CSV table projection event provenance is outside the source "
            "table");
      }
      candidates.push_back({event.input_order, encounter++, &event, &trace});
    }
  }
  // Importer-created traces are timestamp ordered, while flat correlation
  // semantics use source row order. Stable ordering also gives deterministic
  // placement to the rare transformed event that shares an input_order.
  std::stable_sort(candidates.begin(), candidates.end(),
                   [](const Candidate& left, const Candidate& right) {
                     if (left.source_row != right.source_row)
                       return left.source_row < right.source_row;
                     return left.encounter < right.encounter;
                   });

  EventTable result = source;
  result.rows.clear();
  result.rows.reserve(candidates.size());
  bool identity_rows = candidates.size() == source.rows.size();
  for (std::size_t index = 0; identity_rows && index < candidates.size();
       ++index)
    identity_rows = candidates[index].source_row == index;
  if (!result.index && !identity_rows) {
    // A filtered or reordered projection of an implicit RangeIndex must keep
    // the original source labels. Materialize those labels instead of
    // silently renumbering the result from zero.
    EventTableIndex index;
    index.kind = "Index";
    index.dtype = "int64";
    index.names = {std::nullopt};
    result.index = std::move(index);
  } else if (result.index) {
    // A projection changes row order and cardinality. Keep the explicit
    // index declaration, but select labels by the same source-row mapping as
    // the copied cells so validation and downstream joins see aligned rows.
    if (!identity_rows && result.index->kind == "RangeIndex")
      result.index->kind = "Index";
    result.index->labels.clear();
    result.index->labels.reserve(candidates.size());
  }

  const auto first_class_value = [](const Event& event, const Trace& trace,
                                    FirstClassAttributeRole role)
      -> std::tuple<std::string, FirstClassAttributeState,
                    const std::optional<EventAttribute>*> {
    switch (role) {
      case FirstClassAttributeRole::kCaseId:
        return {event.case_id, event.case_id_state, &trace.case_id_attribute};
      case FirstClassAttributeRole::kActivity:
        return {event.activity, event.activity_state,
                &event.activity_attribute};
      case FirstClassAttributeRole::kTimestamp:
        return {event.timestamp, event.timestamp_state,
                &event.timestamp_attribute};
      case FirstClassAttributeRole::kResource:
        return {event.resource, event.resource_state,
                &event.resource_attribute};
      case FirstClassAttributeRole::kLifecycle:
        return {event.lifecycle, event.lifecycle_state,
                &event.lifecycle_attribute};
      case FirstClassAttributeRole::kNone:
        break;
    }
    return {{}, FirstClassAttributeState::kMissing, nullptr};
  };

  for (const auto& candidate : candidates) {
    EventTable::Row row = source.rows[candidate.source_row];
    const Event& event = *candidate.event;
    for (std::size_t index = 0; index < source.columns.size(); ++index) {
      const auto source_cell = row[index];
      const auto role = source_cell
                            ? source_cell->first_class_role
                            : FirstClassAttributeRole::kNone;
      if (role != FirstClassAttributeRole::kNone) {
        const auto [value, state, metadata] = first_class_value(event, *candidate.trace, role);
        if (!source_cell && state == FirstClassAttributeState::kMissing)
          continue;

        EventAttribute replacement = source_cell.value_or(EventAttribute{
            source.columns[index], value,
            role == FirstClassAttributeRole::kTimestamp
                ? AttributeKind::kDate
                : AttributeKind::kString});
        replacement.key = source.columns[index];
        replacement.value = value;
        replacement.is_null = state == FirstClassAttributeState::kNull;
        replacement.first_class_role = role;
        replacement.first_class_state = state;
        if (replacement.first_class_source_key.empty())
          replacement.first_class_source_key = source.columns[index];
        if (metadata && *metadata &&
            (*metadata)->value == replacement.value &&
            (*metadata)->is_null == replacement.is_null) {
          replacement.kind = (*metadata)->kind;
          replacement.typed_value = (*metadata)->typed_value;
          replacement.container = (*metadata)->container;
          replacement.children = (*metadata)->children;
          replacement.scalar_wrapper = (*metadata)->scalar_wrapper;
          replacement.scalar_children_list = (*metadata)->scalar_children_list;
        } else if (!source_cell || source_cell->value != replacement.value ||
                   source_cell->is_null != replacement.is_null) {
          // A transformed projection without matching typed metadata is a new
          // scalar string; the previous cell's payload no longer describes it.
          replacement.kind = AttributeKind::kString;
          replacement.typed_value = std::monostate{};
          replacement.container = AttributeContainerKind::kScalar;
          replacement.children.clear();
          replacement.scalar_wrapper = false;
          replacement.scalar_children_list = false;
        }
        row[index] = std::move(replacement);
        continue;
      }

      const auto found = std::find_if(
          event.attributes.begin(), event.attributes.end(),
          [&](const auto& attribute) {
            return attribute.key == source.columns[index];
          });
      if (found != event.attributes.end()) {
        auto replacement = *found;
        replacement.key = source.columns[index];
        row[index] = std::move(replacement);
      }
    }
    result.rows.push_back(std::move(row));
    if (result.index && source.index)
      result.index->labels.push_back(source.index->labels[candidate.source_row]);
    else if (result.index) {
      EventAttribute label{"", std::to_string(candidate.source_row),
                           AttributeKind::kInt};
      label.typed_value = static_cast<std::int64_t>(candidate.source_row);
      result.index->labels.push_back({std::move(label)});
    }
  }
  result.validate();
  return result;
}

EventLog import_csv_event_log(
    std::istream& input,
    const CsvImportOptions& options) {
  return import_csv_event_data_impl(input, options, false).log;
}

EventLog import_csv_event_log(
    const std::filesystem::path& path,
    const CsvImportOptions& options) {
  std::ifstream input(path);
  if (!input) {
    throw std::runtime_error("Unable to open CSV event log: " + path.string());
  }
  return import_csv_event_log(input, options);
}

}  // namespace minera
