#include "minera/attributes_filter_controls.hpp"
#include "minera/literal_controls_json.hpp"
#include "minera/log_attribute_repetition_controls.hpp"
#include "minera/case_filter_controls.hpp"
#include <stdexcept>
namespace minera {
EventAttribute parse_attributes_filter_values_json(std::string_view input, bool typed) {
  if(typed)return parse_log_attribute_repetition_target_json(input);
  EventAttribute result;result.container=AttributeContainerKind::kList;
  for(const auto& value:parse_literal_string_vector_json(input))result.children.push_back({std::to_string(result.children.size()),value});
  return result;
}
EventAttribute parse_attributes_filter_bound_json(std::string_view input) {
  const auto at=input.find_first_not_of(" \t\r\n");
  return at!=std::string_view::npos && input[at]=='{'?parse_log_attribute_repetition_target_json(input):parse_case_filter_bound(input);
}
std::vector<std::pair<EventAttribute,EventAttribute>> parse_attributes_filter_pairs_json(std::string_view input) {
  const auto value=parse_log_attribute_repetition_target_json(input);
  if(value.is_null || value.container!=AttributeContainerKind::kList)
    throw std::invalid_argument("attribute dictionary controls require a typed list of key/value pairs");
  std::vector<std::pair<EventAttribute,EventAttribute>> result;
  for(const auto& pair:value.children) {
    if(pair.is_null || (pair.container!=AttributeContainerKind::kList && pair.container!=AttributeContainerKind::kTuple) || pair.children.size()!=2)
      throw std::invalid_argument("each attribute dictionary entry requires exactly two typed values");
    result.emplace_back(pair.children[0],pair.children[1]);
  }
  return result;
}
}
