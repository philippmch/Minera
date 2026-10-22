#pragma once

#include "minera/log_ltl_filter.hpp"
#include "polars_ltl_original_call_parser.hpp"
#include "root_cause_trace_key.hpp"

namespace minera::log_ltl_original_call_detail {
namespace shared = polars_ltl_original_call_detail;

struct Call {
  std::vector<EventAttribute> args;
  LogLtlFilterOptions options;
};

[[noreturn]] inline void invalid(std::string_view message) {
  throw std::invalid_argument("original LOG call_json: " + std::string(message));
}

inline bool canonical_uuid(std::string_view value) {
  if (value.size() != 36) return false;
  for (std::size_t i = 0; i < value.size(); ++i) {
    if (i == 8 || i == 13 || i == 18 || i == 23) {
      if (value[i] != '-') return false;
    } else if (!((value[i] >= '0' && value[i] <= '9') || (value[i] >= 'a' && value[i] <= 'f'))) return false;
  }
  return true;
}

// Extend only the original LOG transport. Recurse before constructing containers
// so NaT/UUID retain their exact native kinds even when nested in an operand.
inline EventAttribute log_typed(std::string_view raw, const shared::Node& node, std::string key = {}) {
  if (node.kind != shared::Node::kObject) invalid("typed values require an object");
  for (const auto& [name, value] : node.fields) {
    (void)value;
    if (name != "type" && name != "value") invalid("unknown typed value field");
  }
  const auto* tag = shared::field(node, "type");
  const auto* value = shared::field(node, "value");
  if (!tag || tag->kind != shared::Node::kString) invalid("typed values require a string type");
  if (tag->text == "nat") {
    if (value) invalid("nat must omit value");
    EventAttribute result{std::move(key), "NaT", AttributeKind::kDate, true};
    result.typed_value = NativeDate{"NaT", NativeDateResolution::kNanosecond};
    return result;
  }
  if (tag->text == "uuid") {
    if (!value || value->kind != shared::Node::kString || !canonical_uuid(value->text))
      invalid("uuid requires a canonical lowercase UUID string");
    return EventAttribute{std::move(key), value->text, AttributeKind::kId};
  }
  const bool sequence = tag->text == "list" || tag->text == "tuple" || tag->text == "set" || tag->text == "frozenset";
  if (sequence || tag->text == "map") {
    if (!value || value->kind != (sequence ? shared::Node::kArray : shared::Node::kObject))
      invalid("typed container has an invalid value shape");
    shared::validate_map_order(node);
    EventAttribute result{std::move(key), ""};
    result.container = tag->text == "map" ? AttributeContainerKind::kContainer :
        tag->text == "list" ? AttributeContainerKind::kList : tag->text == "tuple" ? AttributeContainerKind::kTuple :
        tag->text == "set" ? AttributeContainerKind::kSet : AttributeContainerKind::kFrozenSet;
    if (sequence) {
      for (std::size_t i = 0; i < value->items.size(); ++i)
        result.children.push_back(log_typed(raw, value->items[i], std::to_string(i)));
    } else {
      for (const auto& [name, child] : value->fields) result.children.push_back(log_typed(raw, child, name));
    }
    if (tag->text == "set" || tag->text == "frozenset")
      for (const auto& child : result.children)
        if (!root_cause_key_is_hashable(child)) invalid("set members require immutable typed values");
    return result;
  }
  auto result = shared::typed(raw, node);
  result.key = std::move(key);
  return result;
}

inline Call parse(std::string_view raw, std::size_t arity, bool helper) {
  try {
    const auto root = shared::Reader(raw).read();
    if (root.kind != shared::Node::kObject) invalid("call must be an object");
    for (const auto& [name, value] : root.fields) {
      (void)value;
      if (name != "schema" && name != "args" && name != "parameter_value" && name != "parameter_entries")
        invalid("unknown call field");
    }
    const auto* schema = shared::field(root, "schema");
    if (!schema || schema->kind != shared::Node::kString || schema->text != "minera-log-ltl-original-call-v1")
      invalid("unsupported or missing schema");
    const auto* args = shared::field(root, "args");
    if (!args || args->kind != shared::Node::kArray || args->items.size() != arity)
      invalid("args must contain exactly " + std::to_string(arity) + " typed positional operands");
    const auto* raw_parameters = shared::field(root, "parameter_value");
    const auto* entries = shared::field(root, "parameter_entries");
    if (helper && (raw_parameters || entries)) invalid("timestamp helpers forbid parameters by presence");
    if (raw_parameters && entries) invalid("parameter_value and parameter_entries are mutually exclusive by presence");
    Call call;
    for (const auto& arg : args->items) call.args.push_back(log_typed(raw, arg));
    if (raw_parameters) {
      call.options.raw_parameters = log_typed(raw, *raw_parameters);
      if (call.options.raw_parameters->container == AttributeContainerKind::kContainer)
        invalid("mapping parameters require ordered parameter_entries");
    }
    if (entries) {
      if (entries->kind != shared::Node::kArray) invalid("parameter_entries must be an array");
      std::set<std::string> underlying_names;
      for (const auto& entry : entries->items) {
        const auto* key = shared::field(entry, "key");
        const auto* enum_key = shared::field(entry, "enum_key");
        const auto* value = shared::field(entry, "value");
        if (entry.kind != shared::Node::kObject || entry.fields.size() != 3 || !key || !enum_key || !value ||
            key->kind != shared::Node::kString || enum_key->kind != shared::Node::kBoolean)
          invalid("parameter entries require exactly string key, Boolean enum_key and typed value");
        if (!underlying_names.insert(key->text).second)
          invalid("repeated underlying parameter names are unsupported for LOG calls");
        auto parsed = log_typed(raw, *value);
        if (key->text == "pm4py:param:attribute_key") call.options.attribute_key = std::move(parsed);
        else if (key->text == "pm4py:param:resource_key") call.options.resource_key = std::move(parsed);
        else if (key->text == "pm4py:param:timestamp_key") call.options.timestamp_key = std::move(parsed);
        else if (key->text == "positive") call.options.positive = std::move(parsed);
        else if (key->text == "enable_timestamp") call.options.enable_timestamp = std::move(parsed);
        else if (key->text == "timestamp_diff_boundaries") call.options.timestamp_diff_boundaries = std::move(parsed);
        else invalid("unsupported LOG parameter identity");
      }
    }
    return call;
  } catch (const std::invalid_argument& error) {
    const std::string message = error.what();
    constexpr std::string_view prefix = "original Polars call_json: ";
    if (message.starts_with(prefix)) invalid(std::string_view(message).substr(prefix.size()));
    throw;
  }
}

}  // namespace minera::log_ltl_original_call_detail
