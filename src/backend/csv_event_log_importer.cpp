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
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <unordered_map>
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

std::vector<std::string> parse_csv_row(
    std::string_view row,
    char delimiter,
    bool trim_fields) {
  std::vector<std::string> fields;
  std::string field;
  bool in_quotes = false;

  for (std::size_t i = 0; i < row.size(); ++i) {
    const char ch = row[i];
    if (ch == '"') {
      if (in_quotes && i + 1 < row.size() && row[i + 1] == '"') {
        field.push_back('"');
        ++i;
      } else {
        in_quotes = !in_quotes;
      }
    } else if (ch == delimiter && !in_quotes) {
      fields.push_back(trim_fields ? trim(std::move(field)) : std::move(field));
      field.clear();
    } else {
      field.push_back(ch);
    }
  }

  if (in_quotes) {
    throw std::runtime_error("CSV row contains an unterminated quoted field");
  }

  fields.push_back(trim_fields ? trim(std::move(field)) : std::move(field));
  return fields;
}

// A quoted field can span physical lines. Keep its exact CR/LF bytes; only an
// unquoted record terminator is consumed. Escaped quotes retain their spelling
// for the existing field parser to decode.
bool read_csv_record(std::istream& input, std::string& record) {
  record.clear();
  bool in_quotes = false;
  bool read_any = false;
  char ch;
  while (input.get(ch)) {
    read_any = true;
    if (ch == '"') {
      record.push_back(ch);
      if (in_quotes && input.peek() == '"') {
        input.get(ch);
        record.push_back(ch);
      } else in_quotes = !in_quotes;
    } else if (!in_quotes && (ch == '\r' || ch == '\n')) {
      if (ch == '\r' && input.peek() == '\n') input.get(ch);
      return true;
    } else record.push_back(ch);
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

std::optional<std::string> normalize_reference_timestamp(std::string_view text) {
  TimestampTokens in{text};
  in.skip_spaces();
  int year = 0, month = 0, day = 0;

  // Optional leading weekday name ("Tue 01/02/2024", "Tuesday, Jan 2 2024").
  if (std::isalpha(static_cast<unsigned char>(in.peek()))) {
    const auto saved = in.pos;
    const auto name = in.word();
    if (is_weekday_name(name)) {
      if (in.peek() == ',') ++in.pos;
      in.skip_spaces();
    } else {
      in.pos = saved;
    }
  }

  const auto date_separator = [](char ch) {
    return ch == '/' || ch == '-' || ch == '.' || ch == ' ';
  };
  if (std::isalpha(static_cast<unsigned char>(in.peek()))) {
    // Mon D[,] YYYY
    month = month_from_name(in.word());
    if (month == 0) return std::nullopt;
    if (!date_separator(in.peek())) return std::nullopt;
    ++in.pos;
    in.skip_spaces();
    unsigned d = 0, y = 0;
    if (in.digits(d, 2) == 0) return std::nullopt;
    if (in.peek() == ',') ++in.pos;
    if (!date_separator(in.peek())) return std::nullopt;
    ++in.pos;
    in.skip_spaces();
    const auto year_digits = in.digits(y, 4);
    if (year_digits == 4) year = static_cast<int>(y);
    else if (year_digits == 2) year = expand_two_digit_year(y);
    else return std::nullopt;
    day = static_cast<int>(d);
  } else {
    unsigned first = 0;
    const auto first_digits = in.digits(first, 4);
    if (first_digits == 0 || first_digits == 3) return std::nullopt;
    const char sep = in.peek();
    if (!date_separator(sep)) return std::nullopt;
    ++in.pos;
    if (first_digits == 4) {
      // YYYY-M-D in any of the separators.
      unsigned m = 0, d = 0;
      if (in.digits(m, 2) == 0 || in.peek() != sep) return std::nullopt;
      ++in.pos;
      if (in.digits(d, 2) == 0) return std::nullopt;
      year = static_cast<int>(first);
      month = static_cast<int>(m);
      day = static_cast<int>(d);
    } else if (std::isalpha(static_cast<unsigned char>(in.peek()))) {
      // D Mon YYYY / D-Mon-YYYY
      month = month_from_name(in.word());
      if (month == 0 || in.peek() != sep) return std::nullopt;
      ++in.pos;
      unsigned y = 0;
      const auto year_digits = in.digits(y, 4);
      if (year_digits == 4) year = static_cast<int>(y);
      else if (year_digits == 2) year = expand_two_digit_year(y);
      else return std::nullopt;
      day = static_cast<int>(first);
    } else {
      // A/B/YYYY: month-first unless the first number cannot be a month.
      unsigned second = 0, y = 0;
      if (in.digits(second, 2) == 0 || in.peek() != sep) return std::nullopt;
      ++in.pos;
      const auto year_digits = in.digits(y, 4);
      if (year_digits == 4) year = static_cast<int>(y);
      else if (year_digits == 2) year = expand_two_digit_year(y);
      else return std::nullopt;
      if (first > 12 && second <= 12) {
        day = static_cast<int>(first);
        month = static_cast<int>(second);
      } else {
        month = static_cast<int>(first);
        day = static_cast<int>(second);
      }
    }
  }
  if (year < 1 || month < 1 || month > 12 || day < 1 ||
      day > month_days(year, month))
    return std::nullopt;

  int hour = 0, minute = 0, second = 0;
  std::string fraction;
  std::string offset;
  if (!in.done()) {
    if (in.peek() != ' ' && in.peek() != 'T' && in.peek() != 't')
      return std::nullopt;
    ++in.pos;
    in.skip_spaces();
    unsigned h = 0, m = 0, s = 0;
    if (in.digits(h, 2) == 0) return std::nullopt;
    if (in.peek() == ':') {
      ++in.pos;
      if (in.digits(m, 2) != 2) return std::nullopt;
      if (in.peek() == ':') {
        ++in.pos;
        if (in.digits(s, 2) != 2) return std::nullopt;
        if (in.peek() == '.' || in.peek() == ',') {
          ++in.pos;
          while (!in.done() && std::isdigit(static_cast<unsigned char>(in.peek()))) {
            if (fraction.size() < 9) fraction.push_back(in.peek());
            ++in.pos;
          }
          if (fraction.empty()) return std::nullopt;
        }
      }
    }
    in.skip_spaces();
    if (in.peek() == 'a' || in.peek() == 'A' || in.peek() == 'p' ||
        in.peek() == 'P') {
      const auto saved = in.pos;
      const auto meridiem = in.word();
      if (meridiem == "am" || meridiem == "pm") {
        if (h < 1 || h > 12) return std::nullopt;
        if (meridiem == "am" && h == 12) h = 0;
        else if (meridiem == "pm" && h != 12) h += 12;
      } else {
        in.pos = saved;
      }
      in.skip_spaces();
    }
    if (h > 23 || m > 59 || s > 59) return std::nullopt;
    hour = static_cast<int>(h);
    minute = static_cast<int>(m);
    second = static_cast<int>(s);
    if (!in.done()) {
      const char ch = in.peek();
      if (ch == 'Z' || ch == 'z') {
        ++in.pos;
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
        if (!two_digits(oh)) return std::nullopt;
        if (in.peek() == ':') ++in.pos;
        if (!in.done() && in.peek() != ' ' && in.peek() != '\t' && !two_digits(om))
          return std::nullopt;
        if (oh > 23 || om > 59) return std::nullopt;
        char buffer[8];
        std::snprintf(buffer, sizeof(buffer), "%c%02u:%02u", ch, oh, om);
        offset = buffer;
      } else {
        const auto zone = in.word();
        if (zone != "utc" && zone != "gmt") return std::nullopt;
        offset = "+00:00";
      }
      in.skip_spaces();
    }
  }
  if (!in.done()) return std::nullopt;

  char buffer[40];
  std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d", year,
                month, day, hour, minute, second);
  std::string out = buffer;
  if (!fraction.empty()) out += "." + fraction;
  return out + offset;
}

// Returns the timestamp text to store for a present CSV cell, or nullopt when
// the reference conversion would fail for it.
std::optional<std::string> reference_timestamp_text(const std::string& value) {
  if (parse_timestamp_exact(value)) return value;
  auto normalized = normalize_reference_timestamp(value);
  if (normalized && parse_timestamp_exact(*normalized)) return normalized;
  return std::nullopt;
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
ReferenceCsvColumn infer_reference_column(
    const std::vector<std::string>& cells) {
  const auto count = cells.size();
  using Column = std::vector<std::optional<std::string>>;
  using Kind = ReferenceCsvColumnKind;
  const auto na = [&](std::size_t i) { return is_reference_na_token(cells[i]); };
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
  if (!read_csv_record(input, line)) {
    return {};
  }
  // read_csv skips blank lines before the header too.
  while (reference_batch && is_reference_blank_line(line, options.delimiter)) {
    if (!read_csv_record(input, line)) return {};
    ++line_number;
  }

  // Strip a leading UTF-8 byte-order mark so the first column name still
  // matches; spreadsheet exports routinely prepend one.
  if (line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF &&
      static_cast<unsigned char>(line[1]) == 0xBB &&
      static_cast<unsigned char>(line[2]) == 0xBF) {
    line.erase(0, 3);
  }

  const auto header =
      parse_csv_row(line, options.delimiter, options.trim_fields);
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
    Event event;
    std::vector<EventAttribute> case_attributes;
    EventTable::Row table_row;
  };
  std::vector<std::string> case_tokens;
  std::vector<std::string> activity_tokens;
  std::vector<PendingRow> pending;

  while (read_csv_record(input, line)) {
    ++line_number;
    if (line.empty() ||
        (reference_batch && is_reference_blank_line(line, options.delimiter))) {
      continue;
    }

    auto fields = parse_csv_row(line, options.delimiter, options.trim_fields);
    auto case_id = field_at(fields, case_index, line_number);
    auto activity = field_at(fields, activity_index, line_number);
    auto timestamp = timestamp_index
                         ? field_at(fields, *timestamp_index, line_number)
                         : std::string{};
    auto resource = resource_index
                        ? field_at(fields, *resource_index, line_number)
                        : std::string{};

    const std::size_t token = case_tokens.size();
    case_tokens.push_back(case_id);
    activity_tokens.push_back(activity);
    // Whether an NA case id or activity is missing (the reference drops the
    // row) depends on the inferred column type, decided below. Whitespace-only
    // text is a value there, but the default trim_fields option has already
    // blanked it here. Stream readers keep NA spellings as literal values
    // (keep_na_first_class_values).
    PendingRow row;
    row.line_number = line_number;
    row.token = token;
    for (const auto index : case_attribute_indices) {
      // The batch import keeps blank cells too: the reference takes a case
      // attribute from one row only, which may be blank (missing).
      if (index >= fields.size() || (!reference_batch && fields[index].empty()))
        continue;
      row.case_attributes.push_back(EventAttribute{
          header[index].substr(kCasePrefix.size()), fields[index],
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
      event.lifecycle = field_at(fields, *lifecycle_index, line_number);
      event.lifecycle_state = event.lifecycle.empty()
                                  ? FirstClassAttributeState::kMissing
                                  : FirstClassAttributeState::kPresent;
      event.lifecycle_source_key = header[*lifecycle_index];
    }
    for (const auto index : attribute_indices) {
      // A blank cell means the event simply has no such attribute; a short row
      // reads the same way rather than erroring, since these columns are extra.
      if (index >= fields.size() || fields[index].empty()) continue;
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
                   fields[index].empty() ? FirstClassAttributeState::kMissing
                                         : FirstClassAttributeState::kPresent);
        } else if (lifecycle_index && index == *lifecycle_index) {
          set_role(FirstClassAttributeRole::kLifecycle,
                   fields[index].empty() ? FirstClassAttributeState::kMissing
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
    case_ids = infer_reference_column(case_tokens).text;
    activities = infer_reference_column(activity_tokens).text;
  } else {
    case_ids.assign(case_tokens.begin(), case_tokens.end());
    activities.assign(activity_tokens.begin(), activity_tokens.end());
  }
  case_tokens.clear();
  case_tokens.shrink_to_fit();
  activity_tokens.clear();
  activity_tokens.shrink_to_fit();
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
  for (auto& row : pending) {
    const auto& case_id = case_ids[row.token];
    const auto& activity = activities[row.token];
    if (!case_id || !activity) continue;
    if (timestamp_index) {
      // Decided per column after the scan; see apply below.
      const auto& timestamp = row.event.timestamp;
      if (is_reference_na_token(timestamp)) {
        timestamp_texts.emplace_back(std::string{});
      } else if (auto parsed = reference_timestamp_text(timestamp)) {
        timestamp_texts.push_back(std::move(parsed));
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
  if (timestamp_index && parsed_timestamps > 0) {
    if (first_bad_timestamp) {
      throw std::runtime_error(
          "CSV row " + std::to_string(first_bad_timestamp->first) +
          " has a timestamp '" + first_bad_timestamp->second +
          "' in column '" + header[*timestamp_index] +
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
      std::optional<double> chosen_instant;
      for (const auto& event : trace.events) {
        if (!timestamp_index) break;
        const auto instant = parse_timestamp(event.timestamp);
        if (!instant) continue;
        if (!chosen_instant || *instant < *chosen_instant) {
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
    for (auto& trace : log.traces) {
      order_events_by_time(trace.events);
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

std::string reference_csv_float_text(double value) {
  return python_float_text(value);
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
