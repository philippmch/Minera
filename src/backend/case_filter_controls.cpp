#include "minera/case_filter_controls.hpp"
#include "minera/log_attribute_repetition_controls.hpp"
#include "minera/embedding_workbench_json.hpp"
#include <type_traits>

namespace minera {
EventAttribute parse_case_filter_bound(std::string_view input) {
  return std::visit([](const auto& value) -> EventAttribute {
    using T = std::decay_t<decltype(value)>;
    EventAttribute result{"", ""};
    if constexpr (std::is_same_v<T, std::nullptr_t>) result.is_null = true;
    else if constexpr (std::is_same_v<T, bool>) {
      result.kind = AttributeKind::kBoolean; result.value = value ? "true" : "false"; result.typed_value = value;
    } else if constexpr (std::is_same_v<T, TokenInteger>) {
      return parse_log_attribute_repetition_target_json("{\"type\":\"integer\",\"value\":" +
          serialize_embedding_workbench_string_json(value.decimal()) + "}");
    } else if constexpr (std::is_same_v<T, double>) {
      result.kind = AttributeKind::kFloat; result.typed_value = value;
    } else if constexpr (std::is_same_v<T, std::string>) result.value = value;
    else if constexpr (std::is_same_v<T, EventAttribute>) return value;
    else result.typed_value = value;
    return result;
  }, parse_log_attribute_repetition_bound_json(input));
}
}
