#pragma once
#include "minera/event_log.hpp"
#include <string_view>

namespace minera {
// Exact scalar tokens for the public case-filter bounds. Integers retain
// their lexical precision; invalid comparisons remain lazy in each engine.
[[nodiscard]] EventAttribute parse_case_filter_bound(std::string_view input);
}
