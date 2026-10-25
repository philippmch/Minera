#pragma once

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

namespace minera::detail {

// The "Did you mean" column of a Polars ColumnNotFoundError: the first
// column at the least edit distance (over code points) from the requested
// name, offered when that distance is at most max(1, length / 3).
inline std::optional<std::string> polars_column_hint(const std::string& name, const std::vector<std::string>& columns) {
  const auto code_points = [](const std::string& text) {
    std::u32string out;
    for (std::size_t i = 0; i < text.size();) {
      const auto c = static_cast<unsigned char>(text[i]);
      const int extra = c < 0x80 ? 0 : c < 0xe0 ? 1 : c < 0xf0 ? 2 : 3;
      char32_t value = extra == 0 ? c : extra == 1 ? c & 0x1f : extra == 2 ? c & 0x0f : c & 0x07;
      for (int k = 1; k <= extra && i + k < text.size(); ++k) value = (value << 6) | (static_cast<unsigned char>(text[i + k]) & 0x3f);
      out.push_back(value);
      i += extra + 1;
    }
    return out;
  };
  const auto wanted = code_points(name);
  std::optional<std::string> best;
  std::size_t best_distance = 0;
  for (const auto& column : columns) {
    const auto other = code_points(column);
    std::vector<std::size_t> previous(other.size() + 1), current(other.size() + 1);
    for (std::size_t j = 0; j <= other.size(); ++j) previous[j] = j;
    for (std::size_t i = 1; i <= wanted.size(); ++i) {
      current[0] = i;
      for (std::size_t j = 1; j <= other.size(); ++j)
        current[j] = std::min({previous[j] + 1, current[j - 1] + 1, previous[j - 1] + (wanted[i - 1] != other[j - 1])});
      std::swap(previous, current);
    }
    if (!best || previous[other.size()] < best_distance) {
      best = column;
      best_distance = previous[other.size()];
    }
  }
  if (best && best_distance <= std::max<std::size_t>(1, wanted.size() / 3)) return best;
  return std::nullopt;
}

}  // namespace minera::detail
