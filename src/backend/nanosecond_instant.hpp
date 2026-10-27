#pragma once

#include <compare>
#include <cstdint>
#include <optional>

namespace minera::nanosecond_instant {

// An instant as whole seconds plus 0 <= nanoseconds < 1e9; real timestamps
// (years 1-9999) exceed an int64 nanosecond count, so no single integer is
// used and no 128-bit type is needed.
struct Instant {
  std::int64_t seconds = 0;
  std::int64_t nanoseconds = 0;
  friend auto operator<=>(const Instant&, const Instant&) = default;
};
constexpr std::int64_t kNanosPerSecond = 1'000'000'000;
// INT64_MIN and INT64_MAX nanoseconds.
constexpr Instant kStartTime{-9'223'372'037, 145'224'192};
constexpr Instant kEndTime{9'223'372'036, 854'775'807};

// target - source in nanoseconds, or nullopt when it leaves the int64 range.
inline std::optional<std::int64_t> elapsed_nanoseconds(const Instant& source, const Instant& target) {
  std::int64_t seconds = target.seconds - source.seconds;
  std::int64_t nanos = target.nanoseconds - source.nanoseconds;
  if (nanos < 0) { nanos += kNanosPerSecond; --seconds; }
  if (seconds > kEndTime.seconds || (seconds == kEndTime.seconds && nanos > kEndTime.nanoseconds))
    return std::nullopt;
  if (seconds < kStartTime.seconds || (seconds == kStartTime.seconds && nanos < kStartTime.nanoseconds))
    return std::nullopt;
  // seconds * 1e9 alone overflows at the INT64_MIN edge and (seconds + 1) * 1e9
  // at the INT64_MAX edge, so each sign takes the form that stays in range.
  if (seconds >= 0) return seconds * kNanosPerSecond + nanos;
  return (seconds + 1) * kNanosPerSecond + (nanos - kNanosPerSecond);
}

}  // namespace minera::nanosecond_instant
