#include "zone_database.hpp"

#include <chrono>
#include <string>

#if MINERA_PORTABLE_TZDB
#include <date/tz.h>
namespace tz = date;
#else
namespace tz = std::chrono;
#endif

namespace minera::zone_database {
namespace {

const tz::time_zone* locate(std::string_view zone) {
  // Both backends throw std::runtime_error for unknown names.
  return tz::locate_zone(std::string(zone));
}

std::int64_t seconds_of(std::chrono::seconds value) {
  return static_cast<std::int64_t>(value.count());
}

}  // namespace

SysInfo info_at_utc(std::string_view zone, std::int64_t utc_seconds) {
  const auto info = locate(zone)->get_info(
      tz::sys_seconds{std::chrono::seconds{utc_seconds}});
  return {seconds_of(info.offset), info.abbrev};
}

LocalInfo info_at_local(std::string_view zone, std::int64_t local_seconds) {
  const auto info = locate(zone)->get_info(
      tz::local_seconds{std::chrono::seconds{local_seconds}});
  LocalInfo result;
  result.first_offset_seconds = seconds_of(info.first.offset);
  result.second_offset_seconds = seconds_of(info.second.offset);
  if (info.result == tz::local_info::nonexistent)
    result.result = LocalResult::kNonexistent;
  else if (info.result == tz::local_info::ambiguous)
    result.result = LocalResult::kAmbiguous;
  return result;
}

bool portable_backend() noexcept { return MINERA_PORTABLE_TZDB != 0; }

}  // namespace minera::zone_database
