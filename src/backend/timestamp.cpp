#include "minera/timestamp.hpp"
#include "zone_database.hpp"

#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <stdexcept>

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

bool is_leap_year(unsigned year) {
  return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

unsigned days_in_month(unsigned year, unsigned month) {
  constexpr unsigned days[] = {31, 28, 31, 30, 31, 30,
                               31, 31, 30, 31, 30, 31};
  if (month == 2 && is_leap_year(year)) return 29;
  return days[month - 1];
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
  if (month < 1 || month > 12 || day < 1 ||
      day > days_in_month(year, month)) {
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
    if (hour > 23 || minute > 59) {
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
      if (second > 59) {
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
      if (off_hour > 14 || off_minute > 59 ||
          (off_hour == 14 && off_minute != 0)) {
        return std::nullopt;
      }
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

std::optional<ParsedExactTimestamp> parse_timestamp_exact(std::string_view text) {
  std::size_t pos = 0;
  unsigned year = 0;
  unsigned month = 0;
  unsigned day = 0;
  if (!read_uint(text, pos, 4, year) || pos >= text.size() ||
      text[pos] != '-')
    return std::nullopt;
  ++pos;
  if (!read_uint(text, pos, 2, month) || pos >= text.size() ||
      text[pos] != '-')
    return std::nullopt;
  ++pos;
  if (!read_uint(text, pos, 2, day) || month < 1 || month > 12 || day < 1 ||
      day > days_in_month(year, month))
    return std::nullopt;

  std::int64_t seconds = days_from_civil(year, month, day) * 86400;
  std::uint32_t nanoseconds = 0;
  bool timezone_aware = false;
  std::int32_t offset_minutes = 0;
  if (pos < text.size() &&
      (text[pos] == 'T' || text[pos] == 't' || text[pos] == ' ')) {
    ++pos;
    unsigned hour = 0;
    unsigned minute = 0;
    if (!read_uint(text, pos, 2, hour) || pos >= text.size() ||
        text[pos] != ':')
      return std::nullopt;
    ++pos;
    if (!read_uint(text, pos, 2, minute) || hour > 23 || minute > 59)
      return std::nullopt;
    seconds += static_cast<std::int64_t>(hour) * 3600 + minute * 60;

    if (pos < text.size() && text[pos] == ':') {
      ++pos;
      unsigned second = 0;
      if (!read_uint(text, pos, 2, second) || second > 59)
        return std::nullopt;
      seconds += second;
      if (pos < text.size() && text[pos] == '.') {
        ++pos;
        std::size_t digits = 0;
        while (pos < text.size() &&
               std::isdigit(static_cast<unsigned char>(text[pos])) != 0) {
          if (digits >= 9) return std::nullopt;
          nanoseconds = nanoseconds * 10 +
                        static_cast<std::uint32_t>(text[pos] - '0');
          ++digits;
          ++pos;
        }
        if (digits == 0) return std::nullopt;
        while (digits++ < 9) nanoseconds *= 10;
      }
    }

    if (pos < text.size() && (text[pos] == 'Z' || text[pos] == 'z')) {
      timezone_aware = true;
      ++pos;
    } else if (pos < text.size() &&
               (text[pos] == '+' || text[pos] == '-')) {
      timezone_aware = true;
      const int sign = text[pos] == '-' ? -1 : 1;
      ++pos;
      unsigned offset_hour = 0;
      unsigned offset_minute = 0;
      if (!read_uint(text, pos, 2, offset_hour)) return std::nullopt;
      if (pos < text.size() && text[pos] == ':') {
        ++pos;
        if (!read_uint(text, pos, 2, offset_minute)) return std::nullopt;
      } else {
        // The existing parser accepts the basic ±HH form as well as a minute
        // component.  Preserve that compatibility while staying integral.
        (void)read_uint(text, pos, 2, offset_minute);
      }
      if (offset_hour > 14 || offset_minute > 59 ||
          (offset_hour == 14 && offset_minute != 0))
        return std::nullopt;
      offset_minutes = sign * static_cast<std::int32_t>(offset_hour * 60 +
                                                         offset_minute);
      seconds -= static_cast<std::int64_t>(offset_minutes) * 60;
    }
  }

  while (pos < text.size() &&
         std::isspace(static_cast<unsigned char>(text[pos])) != 0)
    ++pos;
  if (pos != text.size()) return std::nullopt;
  return ParsedExactTimestamp{seconds, nanoseconds, timezone_aware,
                              offset_minutes};
}

std::optional<std::int64_t> parse_timestamp_microseconds(std::string_view text) {
  // Validate the original shape before removing its fractional part.
  const auto parsed = parse_timestamp(text);
  if (!parsed) return std::nullopt;
  const auto dot = text.find('.');
  if (dot == std::string_view::npos)
    return static_cast<std::int64_t>(*parsed) * 1000000;
  std::size_t end = dot + 1;
  std::int64_t fraction = 0;
  std::size_t digits = 0;
  while (end < text.size() &&
         std::isdigit(static_cast<unsigned char>(text[end])) != 0) {
    if (digits < 6) fraction = fraction * 10 + (text[end] - '0');
    ++digits;
    ++end;
  }
  for (; digits < 6; ++digits) fraction *= 10;
  std::string whole(text.substr(0, dot));
  whole.append(text.substr(end));
  // Four-digit years have exactly representable integral epoch seconds and
  // remain within int64 microseconds, including timezone normalization.
  const auto seconds = parse_timestamp(whole);
  if (!seconds) return std::nullopt;
  return static_cast<std::int64_t>(*seconds) * 1000000 + fraction;
}

std::string format_timestamp(double epoch_seconds) {
  long long milliseconds = std::llround(epoch_seconds * 1000.0);
  long long seconds = milliseconds / 1000;
  long long milli = milliseconds % 1000;
  if (milli < 0) {
    milli += 1000;
    --seconds;
  }
  long long days = seconds / 86400;
  long long of_day = seconds % 86400;
  if (of_day < 0) {
    of_day += 86400;
    --days;
  }

  // The inverse of days_from_civil, by the same algorithm.
  days += 719468;  // shift epoch from 1970-01-01 to 0000-03-01
  const long long era = (days >= 0 ? days : days - 146096) / 146097;
  const unsigned doe = static_cast<unsigned>(days - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  const unsigned day = doy - (153 * mp + 2) / 5 + 1;
  const unsigned month = mp < 10 ? mp + 3 : mp - 9;
  const long long year = static_cast<long long>(yoe) + era * 400 + (month <= 2);

  char buffer[40];
  std::snprintf(buffer, sizeof(buffer),
                "%04lld-%02u-%02uT%02lld:%02lld:%02lld.%03lldZ", year, month,
                day, of_day / 3600, (of_day / 60) % 60, of_day % 60, milli);
  return buffer;
}

std::string format_timestamp_microseconds(double epoch_seconds) {
  constexpr double first_second = -62135596800.0;  // 0001-01-01
  constexpr double past_last_second = 253402300800.0;  // 10000-01-01
  if (!std::isfinite(epoch_seconds) || epoch_seconds < first_second ||
      epoch_seconds >= past_last_second) {
    throw std::invalid_argument("timestamp is outside calendar years 1 through 9999");
  }
  double whole = std::floor(epoch_seconds);
  const double fraction = (epoch_seconds - whole) * 1000000.0;
  auto micros = static_cast<long long>(std::floor(fraction));
  const double remainder = fraction - static_cast<double>(micros);
  // datetime timestamps use round-to-nearest with ties to the even microsecond.
  if (remainder > 0.5 || (remainder == 0.5 && micros % 2 != 0)) ++micros;
  if (micros == 1000000) {
    micros = 0;
    whole += 1.0;
  }
  if (whole >= past_last_second) {
    throw std::invalid_argument("rounded timestamp is outside calendar years 1 through 9999");
  }
  auto result = format_timestamp(whole);
  char fraction_text[9];
  std::snprintf(fraction_text, sizeof(fraction_text), ".%06lldZ", micros);
  result.replace(result.size() - 5, 5, fraction_text);
  return result;
}

std::optional<ParsedExactTimestamp> localize_timestamp_wall_time(
    const ParsedExactTimestamp& wall, std::string_view zone) {
  auto result = wall;
  result.timezone_aware = true;
  if (zone == "UTC") { result.offset_minutes = 0; return result; }
  if (zone.size() == 9 && zone.starts_with("UTC") && (zone[3] == '+' || zone[3] == '-') &&
      zone[6] == ':') {
    const auto digit = [&](std::size_t at) {
      if (!std::isdigit(static_cast<unsigned char>(zone[at])))
        throw std::invalid_argument("invalid fixed timezone offset");
      return zone[at] - '0';
    };
    const int minutes = (digit(4) * 10 + digit(5)) * 60 + digit(7) * 10 + digit(8);
    result.offset_minutes = zone[3] == '-' ? -minutes : minutes;
    result.seconds -= std::int64_t{result.offset_minutes} * 60;
    return result;
  }
  ::minera::zone_database::LocalInfo info;
  try { info = ::minera::zone_database::info_at_local(zone, wall.seconds); }
  catch (const std::runtime_error&) { throw std::invalid_argument("unknown timezone: " + std::string(zone)); }
  if (info.result != ::minera::zone_database::LocalResult::kUnique) return std::nullopt;
  const auto offset = info.first_offset_seconds;
  result.offset_minutes = static_cast<std::int32_t>(offset / 60);
  result.seconds -= offset;
  return result;
}

}  // namespace minera
