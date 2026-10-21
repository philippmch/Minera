#pragma once
#include "minera/event_log.hpp"
#include <string_view>
namespace minera {
// Workspace JSON arrays have explicit pair semantics: each two-string array
// is a tuple, preserving literal labels while avoiding ambiguous CSV splitting.
[[nodiscard]] EventAttribute parse_paths_filter_pair_json(std::string_view);
[[nodiscard]] EventAttribute parse_paths_filter_pairs_json(std::string_view);
}
