#include "minera/timestamp.hpp"

#include <cctype>
#include <cstdint>

namespace minera {
namespace {

// Days from the civil date 1970-01-01 to y-m-d, by Howard Hinnant's algorithm.
// Valid for the proleptic Gregorian calendar; returns a signed day count so
// dates before the epoch parse too.
std::int64_t days_from_civil(std::int64_t year, unsigned month, unsigned day) {
  year -= month <= 2;
  const std::int64_t era = (year >= 0 ? year : year - 399) / 400;
  const auto year_of_era = static_cast<unsigned>(year - era * 400);
  const unsigned day_of_year =
      (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  const unsigned day_of_era =
      year_of_era * 365 + year_of_era / 4 - year_of_era / 100 + day_of_year;
  return era * 146097 + static_cast<std::int64_t>(day_of_era) - 719468;
}

// Reads `count` ASCII digits starting at `pos`, advancing it. Returns false if
// fewer than `count` digits are present.
bool read_uint(std::string_view text, std::size_t& pos, std::size_t count,
               unsigned& out) {
  if (pos + count > text.size()) {
    return false;
  }
  unsigned value = 0;
  for (std::size_t i = 0; i < count; ++i) {
    const char ch = text[pos + i];
    if (std::isdigit(static_cast<unsigned char>(ch)) == 0) {
      return false;
    }
    value = value * 10 + static_cast<unsigned>(ch - '0');
  }
  pos += count;
  out = value;
  return true;
}

}  // namespace

std::optional<double> parse_timestamp(std::string_view text) {
  std::size_t pos = 0;

  // Date: YYYY-MM-DD.
  unsigned year = 0;
  unsigned month = 0;
  unsigned day = 0;
  if (!read_uint(text, pos, 4, year) || pos >= text.size() || text[pos] != '-') {
    return std::nullopt;
  }
  ++pos;
  if (!read_uint(text, pos, 2, month) || pos >= text.size() || text[pos] != '-') {
    return std::nullopt;
  }
  ++pos;
  if (!read_uint(text, pos, 2, day)) {
    return std::nullopt;
  }
  if (month < 1 || month > 12 || day < 1 || day > 31) {
    return std::nullopt;
  }

  double seconds =
      static_cast<double>(days_from_civil(year, month, day)) * 86400.0;

  // Optional time, introduced by 'T' or a space.
  if (pos < text.size() && (text[pos] == 'T' || text[pos] == 't' ||
                            text[pos] == ' ')) {
    ++pos;
    unsigned hour = 0;
    unsigned minute = 0;
    if (!read_uint(text, pos, 2, hour) || pos >= text.size() ||
        text[pos] != ':') {
      return std::nullopt;
    }
    ++pos;
    if (!read_uint(text, pos, 2, minute)) {
      return std::nullopt;
    }
    seconds += hour * 3600.0 + minute * 60.0;

    // Optional :SS and fractional seconds.
    if (pos < text.size() && text[pos] == ':') {
      ++pos;
      unsigned second = 0;
      if (!read_uint(text, pos, 2, second)) {
        return std::nullopt;
      }
      seconds += second;
      if (pos < text.size() && text[pos] == '.') {
        ++pos;
        double scale = 0.1;
        bool any = false;
        while (pos < text.size() &&
               std::isdigit(static_cast<unsigned char>(text[pos])) != 0) {
          seconds += (text[pos] - '0') * scale;
          scale *= 0.1;
          ++pos;
          any = true;
        }
        if (!any) {
          return std::nullopt;
        }
      }
    }

    // Optional timezone: 'Z' (UTC) or ±HH:MM / ±HHMM offset, subtracted to
    // normalize the instant to UTC.
    if (pos < text.size() && (text[pos] == 'Z' || text[pos] == 'z')) {
      ++pos;
    } else if (pos < text.size() && (text[pos] == '+' || text[pos] == '-')) {
      const int sign = text[pos] == '-' ? -1 : 1;
      ++pos;
      unsigned off_hour = 0;
      unsigned off_minute = 0;
      if (!read_uint(text, pos, 2, off_hour)) {
        return std::nullopt;
      }
      if (pos < text.size() && text[pos] == ':') {
        ++pos;
      }
      // Minutes are optional in the basic ±HH form.
      read_uint(text, pos, 2, off_minute);
      seconds -= sign * (off_hour * 3600.0 + off_minute * 60.0);
    }
  }

  // Trailing whitespace is tolerated; anything else means this was not a clean
  // timestamp and we would rather report nothing than a half-parsed instant.
  while (pos < text.size() &&
         std::isspace(static_cast<unsigned char>(text[pos])) != 0) {
    ++pos;
  }
  if (pos != text.size()) {
    return std::nullopt;
  }
  return seconds;
}

}  // namespace minera
