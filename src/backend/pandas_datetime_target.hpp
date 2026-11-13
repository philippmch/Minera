#pragma once

// A string compared with a pandas datetime64 column (`series == text`):
// pandas parses the text as a Timestamp and compares instants (round BY,
// observed black-box on pandas 3.0.5). The texts pandas reads as NaT
// ("NaT", "nat", "NAT", "NaN", "nan", "NAN" and "") match no cell; a naive
// text on an aware column takes the column's zone; an aware text never
// equals a naive column (the caller compares awareness). Texts outside
// ISO-8601 go through pandas' dateutil fallback, which Minera does not
// reproduce: they throw std::invalid_argument.

#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include "minera/timestamp.hpp"

namespace minera::pandas_datetime_target {

inline bool nat_text(std::string_view text) {
  return text.empty() || text == "NaT" || text == "nat" || text == "NAT" || text == "NaN" || text == "nan" ||
         text == "NAN";
}

// The zone of "datetime64[ns, UTC]"; nullopt on a naive column.
inline std::optional<std::string> zone(std::string_view dtype) {
  const auto comma = dtype.find(", ");
  if (!dtype.starts_with("datetime64") || comma == std::string_view::npos || !dtype.ends_with("]")) return std::nullopt;
  return std::string(dtype.substr(comma + 2, dtype.size() - comma - 3));
}

// nullopt: the text matches no cell.
inline std::optional<ParsedExactTimestamp> instant(std::string_view text, const std::optional<std::string>& column_zone,
                                                   const char* unsupported) {
  if (nat_text(text)) return std::nullopt;
  auto parsed = parse_timestamp_exact(text);
  if (!parsed) throw std::invalid_argument(unsupported);
  if (column_zone && !parsed->timezone_aware) return localize_timestamp_wall_time(*parsed, *column_zone);
  return parsed;
}

}  // namespace minera::pandas_datetime_target
