#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace minera {

// An exact parsed timestamp keeps the integral UTC second and nanosecond
// fraction separate from the source timezone awareness.  This is used for
// equality/grouping paths where converting through double would collapse
// adjacent nanoseconds or round large calendar values.
struct ParsedExactTimestamp {
  std::int64_t seconds = 0;
  std::uint32_t nanoseconds = 0;
  bool timezone_aware = false;
  // Signed local offset in minutes.  It is metadata for dtype projection;
  // equality of aware timestamps uses seconds/nanoseconds only.
  std::int32_t offset_minutes = 0;
};

// Parses the same ISO-8601 timestamp forms as parse_timestamp, retaining up
// to nanosecond precision and whether a timezone was supplied.  The result is
// nullopt for malformed input; no floating-point conversion is performed.
[[nodiscard]] std::optional<ParsedExactTimestamp> parse_timestamp_exact(
    std::string_view text);

// Parses an ISO-8601 timestamp into seconds since the Unix epoch (UTC). Accepts
// "YYYY-MM-DDTHH:MM:SS" with an optional space instead of the 'T' separator,
// optional fractional seconds, and an optional timezone (a trailing 'Z' or a
// ±HH:MM / ±HHMM offset). A bare date "YYYY-MM-DD" is read as midnight UTC.
// Calendar days, clock fields, and ISO offsets are range-checked. Returns
// nullopt when the text is empty or not recognizable, so callers can treat
// untimed events as having no instant rather than guessing.
[[nodiscard]] std::optional<double> parse_timestamp(std::string_view text);

// Parses the same timestamp shapes into exact microseconds. Fractional digits
// beyond six are truncated, matching datetime microsecond precision.
[[nodiscard]] std::optional<std::int64_t> parse_timestamp_microseconds(
    std::string_view text);

// Formats seconds since the Unix epoch as ISO-8601 UTC with millisecond
// precision ("2024-01-01T10:00:00.000Z"), the xs:dateTime shape XES tools
// expect. Locale- and timezone-independent; the inverse of parse_timestamp.
[[nodiscard]] std::string format_timestamp(double epoch_seconds);

// Formats finite epoch seconds to microseconds for simulation event timestamps.
// Rounds the fractional second without first multiplying the whole epoch.
// Throws when the rounded result lies outside calendar years 1 through 9999.
[[nodiscard]] std::string format_timestamp_microseconds(double epoch_seconds);

}  // namespace minera
