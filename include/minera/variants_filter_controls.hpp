#pragma once
#include "minera/event_log.hpp"
namespace minera {
// Preserve ordinary JSON lists of literal activity sequences or serialized
// variant strings. Arrays remain lists unless tuple interpretation is explicit.
[[nodiscard]] EventAttribute parse_variants_filter_admitted_json(std::string_view input, bool tuple_sequences=false);
[[nodiscard]] EventAttribute parse_variants_filter_string_list_json(std::string_view input);
}
