#pragma once

#include <cstdint>
#include <optional>
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

// Round CK: Python's repr of a list or dict argument (a column key or a
// value), its items through `item` and its keys (strs) through `key`; any
// other value goes to `item` as it is.
template <typename Item, typename Key>
std::string pandas_container_repr(const EventAttribute& value, const Item& item, const Key& key) {
  if (value.container == AttributeContainerKind::kContainer) {
    std::string out = "{";
    for (std::size_t i = 0; i < value.children.size(); ++i) {
      if (i) out += ", ";
      out += key(value.children[i].key) + ": " + pandas_container_repr(value.children[i], item, key);
    }
    return out + "}";
  }
  if (value.container == AttributeContainerKind::kList) {
    std::string out = "[";
    for (std::size_t i = 0; i < value.children.size(); ++i) {
      if (i) out += ", ";
      out += pandas_container_repr(value.children[i], item, key);
    }
    return out + "]";
  }
  return item(value);
}

// Round CK: the Python type of the first part of a value that hash() refuses
// (a dict, list or set, or one inside a tuple or frozenset); std::nullopt
// when the value hashes.
inline std::optional<std::string> pandas_unhashable_type(const EventAttribute& value) {
  if (value.is_null) return std::nullopt;
  switch (value.container) {
    case AttributeContainerKind::kContainer: return std::string("dict");
    case AttributeContainerKind::kList: return std::string("list");
    case AttributeContainerKind::kSet: return std::string("set");
    case AttributeContainerKind::kTuple:
    case AttributeContainerKind::kFrozenSet:
      for (const auto& child : value.children)
        if (auto inner = pandas_unhashable_type(child)) return inner;
      return std::nullopt;
    default: return std::nullopt;
  }
}

// Round CK: `key in df.columns` (or any other hash of the key) on an
// unhashable key.
template <typename Fail>
void pandas_check_hashable_key(const EventAttribute& key, Fail&& fail) {
  if (const auto type = pandas_unhashable_type(key)) fail("TypeError", "unhashable type: '" + *type + "'");
}

// Round CK: df[key] on a key pandas refuses before looking it up: a dict or
// set ("Passing a dict as an indexer"), a tuple holding one, or a list with
// an item that does not hash (a nested list becomes a tuple first).
template <typename Fail>
void pandas_check_getitem_key(const EventAttribute& key, Fail&& fail) {
  if (key.is_null) return;
  const auto passing = [&](const char* type) {
    fail("TypeError", std::string("Passing a ") + type + " as an indexer is not supported. Use a list instead.");
  };
  if (key.container == AttributeContainerKind::kContainer) passing("dict");
  if (key.container == AttributeContainerKind::kSet) passing("set");
  if (key.container == AttributeContainerKind::kTuple) {
    for (const auto& child : key.children) {
      if (child.container == AttributeContainerKind::kContainer) passing("dict");
      if (child.container == AttributeContainerKind::kSet) passing("set");
    }
    pandas_check_hashable_key(key, fail);
    return;
  }
  if (key.container == AttributeContainerKind::kList) {
    // Nested lists become tuples only when the key is a regular 2-D array
    // (every item a list of one length holding no sequence); a ragged key
    // keeps its lists.
    const auto sequence = [](const EventAttribute& part) {
      return !part.is_null && (part.container == AttributeContainerKind::kList || part.container == AttributeContainerKind::kTuple);
    };
    bool regular = !key.children.empty();
    for (const auto& child : key.children) {
      if (!sequence(child) || child.children.size() != key.children.front().children.size()) { regular = false; break; }
      for (const auto& inner : child.children) if (sequence(inner)) regular = false;
    }
    for (const auto& child : key.children) {
      if (child.container == AttributeContainerKind::kList && regular) {
        for (const auto& inner : child.children) pandas_check_hashable_key(inner, fail);
        continue;
      }
      pandas_check_hashable_key(child, fail);
    }
  }
}

// Round CK: df.set_index(key) on a key holding a dict, set or a tuple that
// does not hash ("Received column of type <class 'dict'>").
template <typename Fail>
void pandas_check_set_index_key(const EventAttribute& key, Fail&& fail) {
  const auto refuse = [&](const EventAttribute& part) {
    const auto type = carrier_type_name(part);
    fail("TypeError", "The parameter \"keys\" may be a column key, one-dimensional array, or a list containing only "
                      "valid column keys and one-dimensional arrays.. Received column of type <class '" + type + "'>");
  };
  const auto bad = [](const EventAttribute& part) {
    return !part.is_null && (part.container == AttributeContainerKind::kContainer ||
                             part.container == AttributeContainerKind::kSet ||
                             (part.container == AttributeContainerKind::kTuple && pandas_unhashable_type(part)));
  };
  if (bad(key)) refuse(key);
  if (key.container == AttributeContainerKind::kList)
    for (const auto& child : key.children)
      if (bad(child)) refuse(child);
}

}  // namespace minera::detail
