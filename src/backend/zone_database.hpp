#pragma once

#include <cstdint>
#include <string>
#include <string_view>

// IANA time-zone lookups behind one seam. Standard libraries that ship the
// C++20 time-zone database (libstdc++ 14+, MSVC) answer directly; others
// (Apple libc++) read the operating system's zoneinfo through the vendored
// date/tz library. CMake picks the backend when it configures minera_core.
namespace minera::zone_database {

enum class LocalResult { kUnique, kNonexistent, kAmbiguous };

struct SysInfo {
  std::int64_t offset_seconds = 0;
  std::string abbrev;
};

struct LocalInfo {
  LocalResult result = LocalResult::kUnique;
  // For kUnique the zone's offset; for kAmbiguous the earlier offset; for
  // kNonexistent the offset before the gap.
  std::int64_t first_offset_seconds = 0;
  // kAmbiguous: the later offset; kNonexistent: the offset after the gap.
  std::int64_t second_offset_seconds = 0;
};

// Both lookups throw std::runtime_error for a name the database lacks.
[[nodiscard]] SysInfo info_at_utc(std::string_view zone, std::int64_t utc_seconds);
[[nodiscard]] LocalInfo info_at_local(std::string_view zone, std::int64_t local_seconds);

// True when the vendored portable database is in use.
[[nodiscard]] bool portable_backend() noexcept;

}  // namespace minera::zone_database
