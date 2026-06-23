#pragma once

#include <optional>
#include <string_view>

namespace minera {

// Parses an ISO-8601 timestamp into seconds since the Unix epoch (UTC). Accepts
// "YYYY-MM-DDTHH:MM:SS" with an optional space instead of the 'T' separator,
// optional fractional seconds, and an optional timezone (a trailing 'Z' or a
// ±HH:MM / ±HHMM offset). A bare date "YYYY-MM-DD" is read as midnight UTC.
// Returns nullopt when the text is empty or not a recognizable timestamp, so
// callers can treat untimed events as having no instant rather than guessing.
[[nodiscard]] std::optional<double> parse_timestamp(std::string_view text);

}  // namespace minera
