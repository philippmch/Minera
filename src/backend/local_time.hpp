#pragma once

#include <cstdint>
#include <optional>

// The process time zone as CPython's datetime sees it on the reference
// platform: the C library's localtime under TZ. glibc and macOS read an IANA
// name in TZ ("Europe/Berlin"); Windows' C runtime reads only the
// "EST5EDT" form (with US daylight rules) and parses an IANA name as
// garbage, so on Windows an IANA name in TZ goes through the time-zone
// database instead. Without TZ every platform uses its C library.
namespace minera::local_time {

// Seconds from 1970-01-01T00:00:00 of a proleptic Gregorian wall clock.
[[nodiscard]] std::int64_t civil_seconds(std::int64_t year, int month, int day, int hour,
                                         int minute, int second) noexcept;

// local(u) of CPython's datetime: the wall clock, as civil_seconds, of POSIX
// second `utc_seconds` in the process time zone; nullopt where the C library
// cannot convert it (Windows' runtime refuses instants before 1970 and
// after 3000).
[[nodiscard]] std::optional<std::int64_t> wall_seconds(std::int64_t utc_seconds);

// datetime.timestamp() of a naive datetime with fold 0 (CPython's
// local_to_seconds): the POSIX second of wall clock `wall` (civil_seconds).
// In a gap the offset before it applies, in an overlap the first instant.
// nullopt where wall_seconds fails for a probe.
[[nodiscard]] std::optional<std::int64_t> naive_timestamp(std::int64_t wall);

}  // namespace minera::local_time
