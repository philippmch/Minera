#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace minera {

// Splits one escaped OCEL query field. A false final_level preserves escapes
// belonging to a nested delimiter so a later parser can decode them exactly
// once. The wire grammar uses backslash to escape comma, colon, semicolon,
// pipe, and backslash itself.
[[nodiscard]] std::vector<std::string> split_ocel_query_value(
    std::string_view value, char delimiter, bool final_level = true);

}  // namespace minera
