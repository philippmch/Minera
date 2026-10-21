#pragma once
#include "minera/log_attribute_repetition_filter.hpp"

namespace minera {
[[nodiscard]] EventAttribute parse_log_attribute_repetition_target_json(
    std::string_view input);
[[nodiscard]] LogAttributeRepetitionBound parse_log_attribute_repetition_bound_json(
    std::string_view input);
}  // namespace minera
