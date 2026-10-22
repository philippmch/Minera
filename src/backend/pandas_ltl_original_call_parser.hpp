#pragma once

#include "minera/pandas_ltl_filter.hpp"
#include "polars_ltl_original_call_parser.hpp"

namespace minera::pandas_ltl_original_call_detail {

// A private transport carrier for the admitted fixed-arity pandas calls.
struct Call {
  std::vector<EventAttribute> args;
  std::optional<EventAttribute> parameter_value;
  std::optional<std::vector<PandasLtlFilterParameter>> parameter_entries;
};

[[noreturn]] inline void invalid(std::string_view message) {
  throw std::invalid_argument("original pandas call_json: " + std::string(message));
}

inline Call parse(std::string_view raw, std::size_t arity) {
  // Reuse the unchanged strict scanner and raw typed subtree slices. Relabel
  // its contextual errors without changing Polars parsing or diagnostics.
  try {
    namespace shared = polars_ltl_original_call_detail;
    const auto root = shared::Reader(raw).read();
    if (root.kind != shared::Node::kObject) invalid("call must be an object");
    for (const auto& [key, value] : root.fields) {
      (void)value;
      if (key != "schema" && key != "args" && key != "parameter_value" && key != "parameter_entries")
        invalid("unknown call field");
    }
    const auto* schema = shared::field(root, "schema");
    if (!schema || schema->kind != shared::Node::kString || schema->text != "minera-pandas-ltl-original-call-v1")
      invalid("unsupported or missing schema");
    const auto* args = shared::field(root, "args");
    if (!args || args->kind != shared::Node::kArray || args->items.size() != arity)
      invalid("args must contain exactly " + std::to_string(arity) + " typed positional operands");
    const auto* raw_parameters = shared::field(root, "parameter_value");
    const auto* entries = shared::field(root, "parameter_entries");
    if (raw_parameters && entries)
      invalid("parameter_value and parameter_entries are mutually exclusive by presence");
    Call call;
    for (const auto& arg : args->items) call.args.push_back(shared::typed(raw, arg));
    if (raw_parameters) {
      call.parameter_value = shared::typed(raw, *raw_parameters);
      if (call.parameter_value->container == AttributeContainerKind::kContainer)
        invalid("mapping parameters require ordered parameter_entries");
    }
    if (entries) {
      if (entries->kind != shared::Node::kArray) invalid("parameter_entries must be an array");
      call.parameter_entries.emplace();
      std::set<std::pair<bool, std::string>> identities;
      static const std::set<std::string> enum_keys = {
          "pm4py:param:case_id_key", "pm4py:param:attribute_key", "pm4py:param:timestamp_key",
          "pm4py:param:resource_key", "positive", "enable_timestamp", "timestamp_diff_boundaries"};
      for (const auto& entry : entries->items) {
        const auto* key = shared::field(entry, "key");
        const auto* enum_key = shared::field(entry, "enum_key");
        const auto* value = shared::field(entry, "value");
        if (entry.kind != shared::Node::kObject || entry.fields.size() != 3 || !key || !enum_key || !value ||
            key->kind != shared::Node::kString || enum_key->kind != shared::Node::kBoolean)
          invalid("parameter entries require exactly string key, Boolean enum_key and typed value");
        if (enum_key->boolean && !enum_keys.contains(key->text)) invalid("unsupported enum parameter identity");
        if (!identities.emplace(enum_key->boolean, key->text).second) invalid("duplicate parameter key identity");
        auto parsed = shared::typed(raw, *value);
        parsed.key = key->text;
        call.parameter_entries->push_back({std::move(parsed), enum_key->boolean});
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

}  // namespace minera::pandas_ltl_original_call_detail
