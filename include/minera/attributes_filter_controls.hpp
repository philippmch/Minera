#pragma once
#include "minera/event_log.hpp"
#include <utility>
namespace minera {
// Literal string lists are convenient; typed collections remain explicit.
[[nodiscard]] EventAttribute parse_attributes_filter_values_json(std::string_view input, bool typed=false);
[[nodiscard]] EventAttribute parse_attributes_filter_bound_json(std::string_view input);
// Transport an ordered dictionary using explicit typed [key,value] pairs.
[[nodiscard]] std::vector<std::pair<EventAttribute,EventAttribute>> parse_attributes_filter_pairs_json(std::string_view input);
}
