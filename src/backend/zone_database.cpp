#include "zone_database.hpp"

#include <chrono>
#include <string>

#if MINERA_PORTABLE_TZDB
#include <date/ptz.h>
#include <date/tz.h>

#include <cstdlib>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <sys/stat.h>
#include <unistd.h>
#include <climits>
namespace tz = date;
#else
namespace tz = std::chrono;
#endif

namespace minera::zone_database {
namespace {

std::int64_t seconds_of(std::chrono::seconds value) {
  return static_cast<std::int64_t>(value.count());
}

SysInfo from_sys(const tz::sys_info& info) {
  return {seconds_of(info.offset), info.abbrev};
}

LocalInfo from_local(const tz::local_info& info) {
  LocalInfo result;
  result.first_offset_seconds = seconds_of(info.first.offset);
  // `second` is only meaningful (and, in date/tz, only initialized) for a
  // gap or an overlap.
  if (info.result == tz::local_info::nonexistent) {
    result.result = LocalResult::kNonexistent;
    result.second_offset_seconds = seconds_of(info.second.offset);
  } else if (info.result == tz::local_info::ambiguous) {
    result.result = LocalResult::kAmbiguous;
    result.second_offset_seconds = seconds_of(info.second.offset);
  }
  return result;
}

#if MINERA_PORTABLE_TZDB
// date/tz's OS-zoneinfo reader only knows the explicit transitions of a TZif
// file and repeats the last one forever. TZif v2+ files end with a POSIX TZ
// rule ("CET-1CEST,M3.5.0,M10.5.0/3") that governs everything after their
// table (zic writes tables up to 2037), so past the last transition the rule
// answers instead.
struct Zone {
  const date::time_zone* table = nullptr;
  std::optional<Posix::time_zone> rule;
  date::sys_seconds last_transition{};
};

std::string zoneinfo_dir() {
  if (const char* env = std::getenv("TZDIR"); env != nullptr && *env != '\0') return env;
  struct stat status {};
#if defined(__APPLE__)
  // Mirror date/tz: macOS points /etc/localtime into its zoneinfo tree.
  char target[PATH_MAX + 1] = {};
  if (readlink("/etc/localtime", target, sizeof(target) - 1) > 0) {
    const std::string link(target);
    if (const auto at = link.find("zoneinfo"); at != std::string::npos)
      if (const auto slash = link.find('/', at); slash != std::string::npos)
        return link.substr(0, slash);
  }
  if (stat("/var/db/timezone/zoneinfo", &status) == 0 && S_ISDIR(status.st_mode))
    return "/var/db/timezone/zoneinfo";
#endif
  if (stat("/usr/share/zoneinfo/uclibc", &status) == 0 && S_ISDIR(status.st_mode))
    return "/usr/share/zoneinfo/uclibc";
  return "/usr/share/zoneinfo";
}

// The footer is the text between the last two newlines of a v2+ file.
std::optional<std::string> tzif_footer(const std::string& name) {
  std::ifstream in(zoneinfo_dir() + '/' + name, std::ios::binary);
  if (!in) return std::nullopt;
  const std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  if (bytes.size() < 6 || bytes.compare(0, 4, "TZif") != 0 || bytes[4] < '2' || bytes.back() != '\n')
    return std::nullopt;
  const auto start = bytes.rfind('\n', bytes.size() - 2);
  if (start == std::string::npos) return std::nullopt;
  auto footer = bytes.substr(start + 1, bytes.size() - start - 2);
  if (footer.empty()) return std::nullopt;
  return footer;
}

const Zone& locate(std::string_view name) {
  static std::mutex mutex;
  static std::map<std::string, std::unique_ptr<Zone>, std::less<>> cache;
  std::lock_guard<std::mutex> lock(mutex);
  if (const auto found = cache.find(name); found != cache.end()) return *found->second;
  auto zone = std::make_unique<Zone>();
  zone->table = date::locate_zone(std::string(name));  // throws std::runtime_error
  // The final table entry extends to year::max; its begin is the last
  // explicit transition.
  zone->last_transition =
      zone->table->get_info(date::sys_seconds{date::sys_days{date::year{9000} / 1 / 1}}).begin;
  if (const auto footer = tzif_footer(std::string(name))) {
    try {
      zone->rule.emplace(*footer);
    } catch (const std::exception&) {
      // An unparsable rule leaves the explicit table in charge.
    }
  }
  return *cache.emplace(std::string(name), std::move(zone)).first->second;
}

// Wall times this far past the last transition cannot touch it, whatever the
// zone's offsets (all real offsets are within a day).
constexpr std::chrono::hours kLocalMargin{48};
#endif

}  // namespace

SysInfo info_at_utc(std::string_view zone, std::int64_t utc_seconds) {
  const tz::sys_seconds instant{std::chrono::seconds{utc_seconds}};
#if MINERA_PORTABLE_TZDB
  const auto& located = locate(zone);
  if (located.rule && instant >= located.last_transition)
    return from_sys(located.rule->get_info(instant));
  return from_sys(located.table->get_info(instant));
#else
  return from_sys(tz::locate_zone(std::string(zone))->get_info(instant));
#endif
}

LocalInfo info_at_local(std::string_view zone, std::int64_t local_seconds) {
  const tz::local_seconds wall{std::chrono::seconds{local_seconds}};
#if MINERA_PORTABLE_TZDB
  const auto& located = locate(zone);
  if (located.rule &&
      wall.time_since_epoch() - kLocalMargin >= located.last_transition.time_since_epoch())
    return from_local(located.rule->get_info(wall));
  return from_local(located.table->get_info(wall));
#else
  return from_local(tz::locate_zone(std::string(zone))->get_info(wall));
#endif
}

bool portable_backend() noexcept { return MINERA_PORTABLE_TZDB != 0; }

}  // namespace minera::zone_database
