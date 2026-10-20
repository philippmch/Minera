#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace minera {
// PRIPEL's encoded trace grammar uses literal '@' separators. Empty input is
// one empty token; leading, trailing and repeated separators retain empty tokens.
[[nodiscard]] std::size_t pripel_encoded_trace_length(std::string_view sequence);
[[nodiscard]] std::vector<std::pair<std::size_t, std::string>> enumerate_pripel_trace(
    std::string_view sequence);
[[nodiscard]] std::size_t pripel_trace_levenshtein(
    std::string_view left, std::string_view right);
}  // namespace minera
