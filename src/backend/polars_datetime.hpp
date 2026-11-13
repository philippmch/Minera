#pragma once

// Polars Datetime columns of native EventTable frames carry a time unit: ns,
// us or ms (round BW). A cell reads at its column unit, floored like a Polars
// cast. A comparison with a microsecond literal (a Python datetime) runs at
// the coarser of the two units, Polars' datetime supertype: a nanosecond
// column floors to microseconds, a millisecond literal floors to the column's
// milliseconds. Date columns read as microseconds.

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "minera/timestamp.hpp"

namespace minera::polars_datetime {

enum class Unit { kNanoseconds, kMicroseconds, kMilliseconds };

inline std::optional<Unit> unit(std::string_view dtype) {
  constexpr std::string_view prefix = "Datetime(time_unit='";
  if (!dtype.starts_with(prefix)) return std::nullopt;
  const auto rest = dtype.substr(prefix.size());
  if (rest.starts_with("ns', time_zone=")) return Unit::kNanoseconds;
  if (rest.starts_with("us', time_zone=")) return Unit::kMicroseconds;
  if (rest.starts_with("ms', time_zone=")) return Unit::kMilliseconds;
  return std::nullopt;
}
inline bool datetime(std::string_view dtype) { return unit(dtype).has_value(); }
inline bool temporal(std::string_view dtype) { return dtype == "Date" || datetime(dtype); }
inline bool naive(std::string_view dtype) { return datetime(dtype) && dtype.ends_with("time_zone=None)"); }
// The unit a Date or Datetime column's cells read at.
inline Unit column_unit(std::string_view dtype) { return unit(dtype).value_or(Unit::kMicroseconds); }
// The unit a column compares with a microsecond literal at.
inline Unit comparison_unit(Unit column) { return column == Unit::kMilliseconds ? column : Unit::kMicroseconds; }
// The unit as Polars prints it in a dtype ("Datetime('μs')").
inline std::string symbol(Unit u) { return u == Unit::kNanoseconds ? "ns" : u == Unit::kMicroseconds ? "\xce\xbcs" : "ms"; }
// A Datetime dtype as Polars prints it in messages: "Datetime('ns')" or
// "Datetime('μs', 'UTC')".
inline std::string display(std::string_view dtype) {
  const auto u = unit(dtype);
  if (!u) return std::string(dtype);
  std::string out = "Datetime('" + symbol(*u) + "'";
  const auto at = dtype.find("time_zone=");
  if (at != std::string_view::npos) {
    auto zone = dtype.substr(at + 10);
    if (zone.ends_with(')')) zone.remove_suffix(1);
    if (zone != "None") out += ", " + std::string(zone);
  }
  return out + ")";
}
inline std::int64_t per_second(Unit u) {
  return u == Unit::kNanoseconds ? 1000000000 : u == Unit::kMicroseconds ? 1000000 : 1000;
}

inline std::int64_t floor_div(std::int64_t a, std::int64_t b) {
  const auto q = a / b;
  return (a % b != 0 && ((a < 0) != (b < 0))) ? q - 1 : q;
}
// Ticks re-expressed in another unit, floored like a Polars cast.
inline std::int64_t convert(std::int64_t ticks, Unit from, Unit to) {
  const auto a = per_second(from), b = per_second(to);
  return a >= b ? floor_div(ticks, a / b) : ticks * (b / a);
}
// An exact instant with its fraction floored to a unit.
inline ParsedExactTimestamp floor(ParsedExactTimestamp value, Unit u) {
  const auto step = static_cast<std::uint32_t>(1000000000 / per_second(u));
  value.nanoseconds -= value.nanoseconds % step;
  return value;
}
inline std::int64_t ticks(const ParsedExactTimestamp& value, Unit u) {
  const auto ps = per_second(u);
  return value.seconds * ps + static_cast<std::int64_t>(value.nanoseconds) / (1000000000 / ps);
}
// A cell text's ticks of a unit since the epoch (naive texts read as UTC).
inline std::optional<std::int64_t> ticks(std::string_view text, Unit u) {
  if (u == Unit::kMicroseconds) return parse_timestamp_microseconds(text);
  const auto parsed = parse_timestamp_exact(text);
  if (!parsed) return std::nullopt;
  return ticks(*parsed, u);
}

}  // namespace minera::polars_datetime
