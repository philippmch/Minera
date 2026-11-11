#include "local_time.hpp"

#include "portable_time.hpp"

#include <ctime>

#if defined(_WIN32)
#include "zone_database.hpp"

#include <cstdlib>
#include <exception>
#include <mutex>
#include <string>
#endif

namespace minera::local_time {
namespace {

std::int64_t days_from_civil(std::int64_t y, unsigned m, unsigned d) noexcept {
  y -= m <= 2;
  const std::int64_t era = (y >= 0 ? y : y - 399) / 400;
  const auto yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<std::int64_t>(doe) - 719468;
}

#if defined(_WIN32)
// The UTC offset of the IANA zone named by TZ, or nullopt when TZ is unset or
// names no zone of the database (then the C runtime reads it).
std::optional<std::int64_t> tz_zone_offset(std::int64_t utc_seconds) {
  const char* value = std::getenv("TZ");
  if (value == nullptr || *value == '\0') return std::nullopt;
  std::string name = value;
  if (name.front() == ':') name.erase(0, 1);  // glibc's ":Europe/Berlin"
  static std::mutex mutex;
  static std::string checked;
  static bool known = false;
  {
    const std::lock_guard lock(mutex);
    if (name != checked) {
      checked = name;
      try {
        (void)zone_database::info_at_utc(name, 0);
        known = true;
      } catch (const std::exception&) {
        known = false;
      }
    }
    if (!known) return std::nullopt;
  }
  try {
    return zone_database::info_at_utc(name, utc_seconds).offset_seconds;
  } catch (const std::exception&) {
    return std::nullopt;
  }
}
#endif

}  // namespace

std::int64_t civil_seconds(std::int64_t year, int month, int day, int hour, int minute,
                           int second) noexcept {
  return days_from_civil(year, static_cast<unsigned>(month), static_cast<unsigned>(day)) * 86400 +
         std::int64_t{hour} * 3600 + std::int64_t{minute} * 60 + second;
}

std::optional<std::int64_t> wall_seconds(std::int64_t utc_seconds) {
#if defined(_WIN32)
  if (const auto offset = tz_zone_offset(utc_seconds)) return utc_seconds + *offset;
#endif
  const auto value = static_cast<std::time_t>(utc_seconds);
  if (static_cast<std::int64_t>(value) != utc_seconds) return std::nullopt;
  std::tm fields{};
  if (!local_fields(value, fields)) return std::nullopt;
  return civil_seconds(fields.tm_year + std::int64_t{1900}, fields.tm_mon + 1, fields.tm_mday,
                       fields.tm_hour, fields.tm_min, fields.tm_sec);
}

std::optional<std::int64_t> naive_timestamp(std::int64_t wall) {
  constexpr std::int64_t kMaxFoldSeconds = 24 * 3600;
  const auto local = [](std::int64_t u) { return wall_seconds(u); };
  const auto lt = local(wall);
  if (!lt) return std::nullopt;
  const std::int64_t a = *lt - wall;
  const std::int64_t u1 = wall - a;
  const auto t1 = local(u1);
  if (!t1) return std::nullopt;
  std::int64_t b = 0;
  if (*t1 == wall) {
    const std::int64_t earlier = u1 - kMaxFoldSeconds;
    const auto probe = local(earlier);
    if (!probe) return std::nullopt;
    b = *probe - earlier;
    if (a == b) return u1;
  } else {
    b = *t1 - u1;
  }
  const std::int64_t u2 = wall - b;
  const auto t2 = local(u2);
  if (!t2) return std::nullopt;
  if (*t2 == wall) return u2;
  if (*t1 == wall) return u1;
  // Neither offset solves it: the wall clock is in a gap.
  return u1 > u2 ? u1 : u2;
}

}  // namespace minera::local_time
