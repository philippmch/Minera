#pragma once

#include <charconv>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "minera/event_log.hpp"
#include "python_scalar.hpp"

namespace minera::detail {

// pandas makes the index that set_index builds from an int64 column a
// RangeIndex when the values step evenly. The step test runs in int64 with
// wraparound but the range is built from exact integers, so a progression
// that wraps past the int64 bounds becomes a range of another length, and
// setting it as the frame's index raises ValueError ("Length mismatch").
// Returns that range's length, or nullopt when the index keeps the values.
inline std::optional<std::uint64_t> wrapped_range_length(const std::vector<std::int64_t>& values) {
  if (values.size() < 2) return std::nullopt;
  const auto wrap = [](std::uint64_t bits) { return static_cast<std::int64_t>(bits); };
  const auto step = wrap(static_cast<std::uint64_t>(values[1]) - static_cast<std::uint64_t>(values[0]));
  if (step == 0) return std::nullopt;
  for (std::size_t i = 2; i < values.size(); ++i)
    if (wrap(static_cast<std::uint64_t>(values[i]) - static_cast<std::uint64_t>(values[i - 1])) != step)
      return std::nullopt;
  const auto start = values.front();
  const auto stop = wrap(static_cast<std::uint64_t>(values.back()) + static_cast<std::uint64_t>(step));
  // The distance between two int64 values and the step's magnitude fit in
  // uint64, so the length is exact without a wider type.
  std::uint64_t distance = 0, by = 0;
  if (step > 0 && stop > start) {
    distance = static_cast<std::uint64_t>(stop) - static_cast<std::uint64_t>(start);
    by = static_cast<std::uint64_t>(step);
  } else if (step < 0 && stop < start) {
    distance = static_cast<std::uint64_t>(start) - static_cast<std::uint64_t>(stop);
    by = std::uint64_t(0) - static_cast<std::uint64_t>(step);
  }
  const std::uint64_t length = by == 0 ? 0 : distance / by + (distance % by != 0);
  if (length == values.size()) return std::nullopt;
  return length;
}

// The same for an int64 key column of a table (nullopt for other dtypes).
inline std::optional<std::uint64_t> wrapped_range_length(const EventTable& df, std::size_t column) {
  if (column >= df.dtypes.size() || df.dtypes[column] != "int64") return std::nullopt;
  std::vector<std::int64_t> values;
  values.reserve(df.rows.size());
  for (const auto& row : df.rows) {
    if (!row[column]) return std::nullopt;
    const auto value = python::from_attribute(*row[column]);
    std::uint64_t magnitude = 0;
    const auto* end = value.digits.data() + value.digits.size();
    if (value.kind != python::Scalar::Kind::kInt || std::from_chars(value.digits.data(), end, magnitude).ptr != end)
      return std::nullopt;
    values.push_back(static_cast<std::int64_t>(value.negative ? std::uint64_t(0) - magnitude : magnitude));
  }
  return wrapped_range_length(values);
}

}  // namespace minera::detail
