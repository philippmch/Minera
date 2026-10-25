#pragma once

#include <cstdint>
#include <string>
#include <variant>

#include "minera/event_log.hpp"

namespace minera::detail {

// The Python type name of a parameters carrier or of one of its elements.
inline std::string carrier_type_name(const EventAttribute& value) {
  switch (value.container) {
    case AttributeContainerKind::kList: return "list";
    case AttributeContainerKind::kTuple: return "tuple";
    case AttributeContainerKind::kContainer: return "dict";
    case AttributeContainerKind::kSet: return "set";
    case AttributeContainerKind::kFrozenSet: return "frozenset";
    default: break;
  }
  if (value.is_null) return "NoneType";
  if (const auto* date = std::get_if<NativeDate>(&value.typed_value))
    return date->resolution == NativeDateResolution::kNanosecond ? "Timestamp" : "datetime.datetime";
  if (value.kind == AttributeKind::kDate) return "datetime.datetime";
  if (value.kind == AttributeKind::kBoolean || std::holds_alternative<bool>(value.typed_value)) return "bool";
  if (value.kind == AttributeKind::kInt || std::holds_alternative<std::int64_t>(value.typed_value) ||
      std::holds_alternative<std::uint64_t>(value.typed_value))
    return "int";
  if (value.kind == AttributeKind::kFloat || std::holds_alternative<double>(value.typed_value)) return "float";
  if (value.kind == AttributeKind::kBytes) return "bytes";
  return "str";
}

// A parameters argument that is not None and not a dictionary. The reference
// reads every key of it (parameters[k] for k in parameters) before looking a
// parameter up, so a non-empty sequence fails on its first element that is
// not a valid index, a string on its first character, a set on
// subscription, and a scalar on iteration. A carrier that survives yields
// the defaults. `fail(type, message)` raises.
template <typename Fail>
void check_parameter_carrier(const EventAttribute& parameters, Fail&& fail) {
  const auto container = parameters.container;
  if (container == AttributeContainerKind::kList || container == AttributeContainerKind::kTuple) {
    const auto name = carrier_type_name(parameters);
    const auto size = static_cast<std::int64_t>(parameters.children.size());
    for (const auto& key : parameters.children) {
      const auto type = carrier_type_name(key);
      if (key.container != AttributeContainerKind::kScalar || (type != "int" && type != "bool"))
        fail("TypeError", name + " indices must be integers or slices, not " + type);
      std::int64_t index = 0;
      if (const auto* flag = std::get_if<bool>(&key.typed_value)) index = *flag ? 1 : 0;
      else if (const auto* number = std::get_if<std::int64_t>(&key.typed_value)) index = *number;
      else if (type == "bool") index = key.value == "true" ? 1 : 0;
      else index = std::stoll(key.value);
      if (index < -size || index >= size) fail("IndexError", name + " index out of range");
    }
    return;
  }
  if (container == AttributeContainerKind::kSet || container == AttributeContainerKind::kFrozenSet) {
    if (!parameters.children.empty()) fail("TypeError", "'" + carrier_type_name(parameters) + "' object is not subscriptable");
    return;
  }
  const auto type = carrier_type_name(parameters);
  if (type == "str") {
    if (parameters.value.empty()) fail("TypeError", "'in <string>' requires string as left operand, not Parameters");
    fail("TypeError", "string indices must be integers, not 'str'");
  }
  fail("TypeError", "'" + type + "' object is not iterable");
}

}  // namespace minera::detail
