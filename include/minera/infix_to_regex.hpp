#pragma once

#include <string>
#include <vector>

namespace minera {

// Convert exact activity labels and the special token "..." to the pinned
// comma-separated trace regex syntax. Empty patterns/labels are valid. The
// returned expression retains the observed anchoring and trailing-wildcard
// behavior; it is not a normalized activity-vector matcher.
[[nodiscard]] std::string translate_infix_to_regex(
    const std::vector<std::string>& infix);

}  // namespace minera
