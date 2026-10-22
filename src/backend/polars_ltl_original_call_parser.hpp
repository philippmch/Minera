#pragma once

#include "minera/attributes_filter_controls.hpp"
#include "minera/literal_controls_json.hpp"
#include "minera/polars_ltl_filter.hpp"

#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace minera::polars_ltl_original_call_detail {

// Keep the typed subtrees as slices of the caller's text. In particular, no
// floating-point intermediary or object-map roundtrip changes their tokens.
struct Node {
  enum Kind { kObject, kArray, kString, kNumber, kBoolean, kNull } kind = kNull;
  std::size_t begin = 0, end = 0;
  std::string text;
  bool boolean = false;
  std::vector<std::pair<std::string, Node>> fields;
  std::vector<Node> items;
};

class Reader {
 public:
  explicit Reader(std::string_view input) : input_(input) {}
  Node read() {
    auto result = value(0);
    space();
    if (at_ != input_.size()) invalid("trailing JSON content");
    return result;
  }

 private:
  std::string_view input_;
  std::size_t at_ = 0;

  [[noreturn]] static void invalid(std::string_view message) {
    throw std::invalid_argument("original Polars call_json: " + std::string(message));
  }
  void space() {
    while (at_ < input_.size() && (input_[at_] == ' ' || input_[at_] == '\t' ||
           input_[at_] == '\n' || input_[at_] == '\r')) ++at_;
  }
  bool take(char character) {
    space();
    if (at_ == input_.size() || input_[at_] != character) return false;
    ++at_;
    return true;
  }
  void require(char character) {
    if (!take(character)) invalid("invalid JSON separator");
  }
  bool digit() const { return at_ < input_.size() && input_[at_] >= '0' && input_[at_] <= '9'; }
  static bool hex(char character) {
    return (character >= '0' && character <= '9') || (character >= 'a' && character <= 'f') ||
           (character >= 'A' && character <= 'F');
  }
  std::string string() {
    space();
    const auto begin = at_;
    require('"');
    while (at_ < input_.size()) {
      const auto character = static_cast<unsigned char>(input_[at_++]);
      if (character == '"') {
        const auto strings = parse_literal_string_vector_json("[" + std::string(input_.substr(begin, at_ - begin)) + "]");
        return strings.at(0);
      }
      if (character < 0x20) invalid("unescaped JSON control character");
      if (character != '\\') continue;
      if (at_ == input_.size()) invalid("truncated JSON escape");
      const char escape = input_[at_++];
      if (escape == 'u') {
        for (int i = 0; i < 4; ++i)
          if (at_ == input_.size() || !hex(input_[at_++])) invalid("invalid JSON Unicode escape");
      } else if (std::string_view("\"\\/bfnrt").find(escape) == std::string_view::npos) {
        invalid("invalid JSON escape");
      }
    }
    invalid("unterminated JSON string");
  }
  Node value(unsigned depth) {
    if (depth > 64) invalid("JSON nesting limit exceeded");
    space();
    if (at_ == input_.size()) invalid("missing JSON value");
    Node result;
    result.begin = at_;
    const char token = input_[at_];
    if (token == '{') {
      result.kind = Node::kObject;
      ++at_;
      std::set<std::string> keys;
      if (!take('}')) {
        do {
          const auto key = string();
          if (!keys.insert(key).second) invalid("duplicate decoded JSON object key");
          require(':');
          result.fields.emplace_back(key, value(depth + 1));
          if (take('}')) break;
          require(',');
        } while (true);
      }
    } else if (token == '[') {
      result.kind = Node::kArray;
      ++at_;
      if (!take(']')) {
        do {
          result.items.push_back(value(depth + 1));
          if (take(']')) break;
          require(',');
        } while (true);
      }
    } else if (token == '"') {
      result.kind = Node::kString;
      result.text = string();
    } else if (input_.substr(at_, 4) == "null") {
      result.kind = Node::kNull;
      at_ += 4;
    } else if (input_.substr(at_, 4) == "true") {
      result.kind = Node::kBoolean;
      result.boolean = true;
      at_ += 4;
    } else if (input_.substr(at_, 5) == "false") {
      result.kind = Node::kBoolean;
      at_ += 5;
    } else {
      result.kind = Node::kNumber;
      if (input_[at_] == '-') ++at_;
      if (!digit()) invalid("invalid JSON value");
      if (input_[at_] == '0') ++at_;
      else while (digit()) ++at_;
      if (at_ < input_.size() && input_[at_] == '.') {
        ++at_;
        if (!digit()) invalid("invalid JSON number");
        while (digit()) ++at_;
      }
      if (at_ < input_.size() && (input_[at_] == 'e' || input_[at_] == 'E')) {
        ++at_;
        if (at_ < input_.size() && (input_[at_] == '+' || input_[at_] == '-')) ++at_;
        if (!digit()) invalid("invalid JSON exponent");
        while (digit()) ++at_;
      }
    }
    result.end = at_;
    return result;
  }
};

inline const Node* field(const Node& object, std::string_view name) {
  for (const auto& [key, value] : object.fields) if (key == name) return &value;
  return nullptr;
}
[[noreturn]] inline void invalid(std::string_view message) {
  throw std::invalid_argument("original Polars call_json: " + std::string(message));
}
inline void validate_map_order(const Node& node) {
  const auto* type = field(node, "type");
  const auto* contents = field(node, "value");
  if (type && type->kind == Node::kString && type->text == "map" && contents && contents->kind == Node::kObject) {
    for (std::size_t i = 1; i < contents->fields.size(); ++i)
      if (contents->fields[i - 1].first > contents->fields[i].first)
        invalid("operand map order cannot be retained by the existing typed codec");
  }
  for (const auto& [key, child] : node.fields) { (void)key; validate_map_order(child); }
  for (const auto& child : node.items) validate_map_order(child);
}
inline EventAttribute typed(std::string_view raw, const Node& node) {
  if (node.kind != Node::kObject) invalid("typed values require an object");
  validate_map_order(node);
  return parse_attributes_filter_values_json(raw.substr(node.begin, node.end - node.begin), true);
}

inline PolarsLtlFilterCall parse(std::string_view raw) {
  const auto root = Reader(raw).read();
  if (root.kind != Node::kObject) invalid("call must be an object");
  for (const auto& [key, value] : root.fields) {
    (void)value;
    if (key != "schema" && key != "values" && key != "parameter_value" && key != "parameter_entries")
      invalid("unknown call field");
  }
  const auto* schema = field(root, "schema");
  if (!schema || schema->kind != Node::kString || schema->text != "minera-polars-ltl-original-call-v1")
    invalid("unsupported or missing schema");
  const auto* raw_parameters = field(root, "parameter_value");
  const auto* entries = field(root, "parameter_entries");
  if (raw_parameters && entries) invalid("parameter_value and parameter_entries are mutually exclusive by presence");
  PolarsLtlFilterCall call;
  if (const auto* values = field(root, "values")) call.values = typed(raw, *values);
  if (raw_parameters) {
    call.parameter_value = typed(raw, *raw_parameters);
    if (call.parameter_value->container == AttributeContainerKind::kContainer)
      invalid("mapping parameters require ordered parameter_entries");
  }
  if (entries) {
    if (entries->kind != Node::kArray) invalid("parameter_entries must be an array");
    call.parameters.emplace();
    std::set<std::pair<bool, std::string>> identities;
    static const std::set<std::string> enum_keys = {
        "pm4py:param:case_id_key", "pm4py:param:attribute_key", "pm4py:param:timestamp_key",
        "pm4py:param:resource_key", "positive", "enable_timestamp", "timestamp_diff_boundaries"};
    for (const auto& entry : entries->items) {
      const auto* key = field(entry, "key");
      const auto* enum_key = field(entry, "enum_key");
      const auto* value = field(entry, "value");
      if (entry.kind != Node::kObject || entry.fields.size() != 3 || !key || !enum_key || !value ||
          key->kind != Node::kString || enum_key->kind != Node::kBoolean)
        invalid("parameter entries require exactly string key, Boolean enum_key and typed value");
      if (enum_key->boolean && !enum_keys.contains(key->text)) invalid("unsupported enum parameter identity");
      if (!identities.emplace(enum_key->boolean, key->text).second) invalid("duplicate parameter key identity");
      call.parameters->push_back({key->text, typed(raw, *value), enum_key->boolean});
    }
  }
  return call;
}

}  // namespace minera::polars_ltl_original_call_detail
