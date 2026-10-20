#include "minera/pripel_trace_distance.hpp"

#include <algorithm>
#include <numeric>

namespace minera {
namespace {
std::vector<std::string_view> split(std::string_view sequence) {
  std::vector<std::string_view> result;
  std::size_t begin = 0;
  while (true) {
    const auto end = sequence.find('@', begin);
    if (end == std::string_view::npos) {
      result.push_back(sequence.substr(begin));
      return result;
    }
    result.push_back(sequence.substr(begin, end - begin));
    begin = end + 1;
  }
}
}  // namespace
std::size_t pripel_encoded_trace_length(std::string_view sequence) {
  return 1 + static_cast<std::size_t>(std::count(sequence.begin(), sequence.end(), '@'));
}
std::vector<std::pair<std::size_t, std::string>> enumerate_pripel_trace(
    std::string_view sequence) {
  std::vector<std::pair<std::size_t, std::string>> result;
  for (const auto part : split(sequence)) result.emplace_back(result.size(), part);
  return result;
}
std::size_t pripel_trace_levenshtein(std::string_view left, std::string_view right) {
  auto a = split(left), b = split(right);
  if (a.size() < b.size()) a.swap(b);
  std::vector<std::size_t> previous(b.size() + 1), current(b.size() + 1);
  std::iota(previous.begin(), previous.end(), std::size_t{0});
  for (std::size_t i = 0; i < a.size(); ++i) {
    current[0] = i + 1;
    for (std::size_t j = 0; j < b.size(); ++j)
      current[j + 1] = std::min({previous[j + 1] + 1, current[j] + 1,
                               previous[j] + (a[i] == b[j] ? 0U : 1U)});
    previous.swap(current);
  }
  return previous.back();
}
}  // namespace minera
