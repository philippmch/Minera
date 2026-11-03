#pragma once

// The ISO 8601 subset that Python's datetime.fromisoformat (3.11) reads, as
// observed black-box: extended or basic calendar dates and week dates, any one
// separator character, clock fields HH[:MM[:SS]] or HHMMSS with an optional
// '.'/',' fraction (truncated to microseconds), then an optional 'Z' or +/-
// offset that is validated and dropped.  The value is the wall clock as
// microseconds since 1970-01-01T00:00:00, which is what the reference XES
// iterparse readers keep (they attach UTC to the clock fields).
//
// The algorithm follows the OCEL reader in ocel.cpp (iso_wall_microseconds),
// which was checked against the pinned reference on about 7,800 generated
// strings; this copy returns nullopt instead of the reference's messages so
// the XES importer can drop a malformed value.  Internal to the library.

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace minera::detail {

struct PythonIsoDateTime {
  std::int64_t wall_microseconds = 0;
  // Lexical digits of the seconds fraction (0 when there is none).
  std::size_t fraction_digits = 0;
};

namespace python_iso {

inline bool digits(std::string_view text, std::size_t position, std::size_t count, int& value) {
  if (position + count > text.size()) return false;
  value = 0;
  for (std::size_t index = position; index < position + count; ++index) {
    if (std::isdigit(static_cast<unsigned char>(text[index])) == 0) return false;
    value = value * 10 + (text[index] - '0');
  }
  return true;
}

// -1 malformed, 0 read the whole text, 1 left one stray character after the
// last field (accepted only in front of a UTC offset).
inline int clock(std::string_view text, int (&fields)[3], std::int64_t& micro,
                 std::size_t* fraction_digits = nullptr) {
  fields[0] = fields[1] = fields[2] = 0;
  micro = 0;
  if (fraction_digits) *fraction_digits = 0;
  std::size_t position = 0;
  bool separated = false;
  bool fraction = false;
  for (int field = 0; field < 3 && !fraction; ++field) {
    if (!digits(text, position, 2, fields[field])) return -1;
    position += 2;
    if (position >= text.size()) return 0;
    const char next = text[position++];
    if (field == 0) separated = next == ':';
    if (position >= text.size()) return 1;
    if (separated && next == ':') continue;
    if (next == '.' || next == ',') fraction = true;
    else if (!separated) --position;
    else return -1;
  }
  const std::size_t remaining = text.size() - position;
  const std::size_t count = std::min<std::size_t>(remaining, 6);
  int digit = 0;
  for (std::size_t index = 0; index < count; ++index) {
    if (!digits(text, position + index, 1, digit)) return -1;
    micro = micro * 10 + digit;
  }
  for (std::size_t index = count; index < 6; ++index) micro *= 10;
  const auto fraction_start = position;
  position += count;
  while (position < text.size() &&
         std::isdigit(static_cast<unsigned char>(text[position])) != 0) ++position;
  if (fraction_digits && fraction) *fraction_digits = position - fraction_start;
  return position < text.size() ? 1 : 0;
}

inline std::int64_t days_from_civil(int year, int month, int day) {
  year -= month <= 2;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const int year_of_era = year - era * 400;
  const int day_of_year = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  const int day_of_era = year_of_era * 365 + year_of_era / 4 - year_of_era / 100 + day_of_year;
  return static_cast<std::int64_t>(era) * 146097 + day_of_era - 719468;
}

inline int monday_index(std::int64_t days) {  // Monday 0 .. Sunday 6
  return static_cast<int>(((days % 7) + 7 + 3) % 7);
}

inline bool leap(int year) { return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0); }

}  // namespace python_iso

inline std::optional<PythonIsoDateTime> parse_python_isoformat(std::string_view original) {
  using namespace python_iso;
  // A trailing Z reads as "+00:00", so a stray character may precede it.
  std::string spelled(original);
  if (!spelled.empty() && spelled.back() == 'Z') spelled.replace(spelled.size() - 1, 1, "+00:00");
  const std::string_view text = spelled;
  if (text.size() < 7) return std::nullopt;
  std::size_t date_end = 0;
  if (text.size() == 7) date_end = 7;
  else if (text[4] == '-') {
    if (text[5] == 'W') {
      if (text.size() < 8) return std::nullopt;
      if (text.size() > 8 && text[8] == '-') {
        if (text.size() == 9) return std::nullopt;
        date_end = text.size() > 10 && std::isdigit(static_cast<unsigned char>(text[10])) != 0 ? 8 : 10;
      } else date_end = 8;
    } else date_end = 10;
  } else if (text[4] == 'W') {
    std::size_t index = 7;
    while (index < text.size() && std::isdigit(static_cast<unsigned char>(text[index])) != 0) ++index;
    date_end = index < 9 ? index : (index % 2 == 0 ? 7 : 8);
  } else date_end = 8;
  if (date_end > text.size()) return std::nullopt;
  const auto date = text.substr(0, date_end);
  int year = 0, month = 1, day = 1, week = 0, weekday = 1;
  bool week_date = false;
  if (!digits(date, 0, 4, year)) return std::nullopt;
  if (date.size() > 5 && date[4] == '-' && date[5] == 'W') {
    week_date = true;
    if (!digits(date, 6, 2, week)) return std::nullopt;
    if (date.size() == 10) {
      if (date[8] != '-' || !digits(date, 9, 1, weekday)) return std::nullopt;
    } else if (date.size() != 8) return std::nullopt;
  } else if (date.size() > 4 && date[4] == '-') {
    if (date.size() != 10 || date[7] != '-' || !digits(date, 5, 2, month) ||
        !digits(date, 8, 2, day)) return std::nullopt;
  } else if (date.size() > 4 && date[4] == 'W') {
    week_date = true;
    if (!digits(date, 5, 2, week)) return std::nullopt;
    if (date.size() == 8) {
      if (!digits(date, 7, 1, weekday)) return std::nullopt;
    } else if (date.size() != 7) return std::nullopt;
  } else if (date.size() != 8 || !digits(date, 4, 2, month) ||
             !digits(date, 6, 2, day)) return std::nullopt;
  if (week_date) {
    const auto january_first = monday_index(days_from_civil(year, 1, 1));
    const int weeks = january_first == 3 || (january_first == 2 && leap(year)) ? 53 : 52;
    if (week < 1 || week > weeks || weekday < 1 || weekday > 7) return std::nullopt;
  }
  int fields[3] = {0, 0, 0};
  std::int64_t micro = 0;
  std::size_t fraction_digits = 0;
  if (date_end < text.size()) {
    const auto time = text.substr(date_end + 1);  // any one separator character
    const auto zone = time.find_first_of("Z+-");
    const int read = clock(time.substr(0, zone), fields, micro, &fraction_digits);
    if (read < 0 || (zone == std::string_view::npos && read != 0)) return std::nullopt;
    if (zone != std::string_view::npos) {
      if (time[zone] == 'Z') {
        if (zone + 1 != time.size()) return std::nullopt;
      } else {
        int offset[3] = {0, 0, 0};
        std::int64_t offset_micro = 0;
        if (clock(time.substr(zone + 1), offset, offset_micro) != 0) return std::nullopt;
        if (offset[0] * 3600 + offset[1] * 60 + offset[2] >= 86400) return std::nullopt;
      }
    }
  }
  if (year < 1) return std::nullopt;
  std::int64_t days = 0;
  if (week_date) {
    const auto january_fourth = days_from_civil(year, 1, 4);
    days = january_fourth - monday_index(january_fourth) + (week - 1) * 7 + (weekday - 1);
  } else {
    if (month < 1 || month > 12) return std::nullopt;
    static constexpr int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const int last = month == 2 && leap(year) ? 29 : kDays[month - 1];
    if (day < 1 || day > last) return std::nullopt;
    days = days_from_civil(year, month, day);
  }
  // A week date can name a day outside years 1..9999.
  if (days < days_from_civil(1, 1, 1) || days > days_from_civil(9999, 12, 31)) return std::nullopt;
  if (fields[0] > 23 || fields[1] > 59 || fields[2] > 59) return std::nullopt;
  return PythonIsoDateTime{
      (days * 86400 + fields[0] * 3600 + fields[1] * 60 + fields[2]) * 1000000 + micro,
      fraction_digits};
}

}  // namespace minera::detail
