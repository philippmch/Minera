#include "minera/log_attribute_repetition_controls.hpp"
#include "minera/literal_controls_json.hpp"
#include "minera/timestamp.hpp"
#include "root_cause_trace_key.hpp"

#include <charconv>
#include <cmath>
#include <map>
#include <stdexcept>

namespace minera {
namespace {
struct JsonValue {
  enum Kind { null, boolean, number, string, array, object } kind = null;
  std::string text;
  std::vector<JsonValue> items;
  std::map<std::string, JsonValue> fields;
};
class Reader {
 public:
  explicit Reader(std::string_view text) : text_(text) {}
  JsonValue read() { auto result = value(0); space(); if (at_ != text_.size()) fail(); return result; }
 private:
  std::string_view text_;
  std::size_t at_ = 0;
  [[noreturn]] void fail() const { throw std::invalid_argument("Attribute repetition controls require valid JSON"); }
  void space() { while (at_ < text_.size() && (text_[at_] == ' ' || text_[at_] == '\t' || text_[at_] == '\r' || text_[at_] == '\n')) ++at_; }
  bool take(char c) { space(); if (at_ < text_.size() && text_[at_] == c) { ++at_; return true; } return false; }
  void require(char c) { if (!take(c)) fail(); }
  bool digit() const { return at_ < text_.size() && text_[at_] >= '0' && text_[at_] <= '9'; }
  std::string string() {
    space(); const auto start = at_; require('"'); bool escaped = false;
    while (at_ < text_.size()) {
      const char c = text_[at_++];
      if (escaped) escaped = false;
      else if (c == '\\') escaped = true;
      else if (c == '"') return parse_literal_string_vector_json("[" + std::string(text_.substr(start, at_ - start)) + "]").at(0);
    }
    fail();
  }
  JsonValue value(unsigned depth) {
    if (depth > 64) fail(); space(); if (at_ == text_.size()) fail(); JsonValue result;
    if (text_[at_] == '"') { result.kind = JsonValue::string; result.text = string(); return result; }
    if (take('[')) {
      result.kind = JsonValue::array; if (take(']')) return result;
      do { result.items.push_back(value(depth + 1)); if (take(']')) return result; require(','); } while (true);
    }
    if (take('{')) {
      result.kind = JsonValue::object; if (take('}')) return result;
      do {
        const auto key = string(); require(':'); auto child = value(depth + 1);
        if (!result.fields.emplace(key, std::move(child)).second) throw std::invalid_argument("Attribute repetition JSON repeats an object key");
        if (take('}')) return result; require(',');
      } while (true);
    }
    for (const std::string_view word : {"null", "true", "false"})
      if (text_.substr(at_, word.size()) == word) { at_ += word.size(); result.kind = word == "null" ? JsonValue::null : JsonValue::boolean; result.text = word; return result; }
    const auto start = at_; if (text_[at_] == '-') ++at_; if (!digit()) fail();
    if (text_[at_] == '0') ++at_; else while (digit()) ++at_;
    if (at_ < text_.size() && text_[at_] == '.') { ++at_; if (!digit()) fail(); while (digit()) ++at_; }
    if (at_ < text_.size() && (text_[at_] == 'e' || text_[at_] == 'E')) {
      ++at_; if (at_ < text_.size() && (text_[at_] == '+' || text_[at_] == '-')) ++at_;
      if (!digit()) fail(); while (digit()) ++at_;
    }
    result.kind = JsonValue::number; result.text = text_.substr(start, at_ - start); return result;
  }
};

double real(std::string_view text) {
  if (text == "NaN") return std::numeric_limits<double>::quiet_NaN();
  if (text == "INF" || text == "+INF") return std::numeric_limits<double>::infinity();
  if (text == "-INF") return -std::numeric_limits<double>::infinity();
  double value = 0;
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
  if (text.empty() || parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) throw std::invalid_argument("Invalid attribute repetition decimal");
  return value;
}

EventAttribute target(const JsonValue& value, std::string key = {}) {
  if (value.kind != JsonValue::object) throw std::invalid_argument("Repetition target requires a typed JSON object");
  for (const auto& [name, child] : value.fields) {
    (void)child;
    if (name != "type" && name != "value") throw std::invalid_argument("Unknown repetition target field: " + name);
  }
  const auto tag = value.fields.find("type");
  if (tag == value.fields.end() || tag->second.kind != JsonValue::string) throw std::invalid_argument("Repetition target requires a string type");
  const auto& type = tag->second.text;
  EventAttribute result{std::move(key), ""};
  const auto payload = value.fields.find("value");
  if (type == "null" || type == "na") {
    if (payload != value.fields.end() && payload->second.kind != JsonValue::null) throw std::invalid_argument("Null repetition target must omit value or use null");
    if(type == "na") return make_native_na_attribute(result.key);
    result.is_null = true; return result;
  }
  if (payload == value.fields.end()) throw std::invalid_argument("Repetition target requires value");
  const auto& data = payload->second;
  if (type == "string" && data.kind == JsonValue::string) { result.value = data.text; return result; }
  if (type == "bytes" && data.kind == JsonValue::string) {
    const auto digit=[](char c) -> int {
      if(c>='0' && c<='9') return c-'0';
      if(c>='a' && c<='f') return c-'a'+10;
      if(c>='A' && c<='F') return c-'A'+10;
      return -1;
    };
    if(data.text.size()%2) throw std::invalid_argument("Repetition bytes require an even-length hexadecimal string");
    std::string bytes;bytes.reserve(data.text.size()/2);
    for(std::size_t i=0;i<data.text.size();i+=2) {
      const int high=digit(data.text[i]),low=digit(data.text[i+1]);
      if(high<0 || low<0) throw std::invalid_argument("Repetition bytes require hexadecimal digits");
      bytes.push_back(static_cast<char>(16*high+low));
    }
    return make_bytes_attribute(result.key,bytes);
  }
  if (type == "integer" && (data.kind == JsonValue::number || data.kind == JsonValue::string)) {
    const TokenInteger integer(data.text); result.value = integer.decimal(); result.kind = AttributeKind::kInt;
    try { result.typed_value = static_cast<std::int64_t>(integer); }
    catch (const std::overflow_error&) {
      try { result.typed_value = static_cast<std::uint64_t>(integer); }
      catch (const std::overflow_error&) { /* Exact lexical integer remains available. */ }
    }
    return result;
  }
  if (type == "decimal" && (data.kind == JsonValue::number || data.kind == JsonValue::string)) {
    result.kind = AttributeKind::kFloat; result.value = data.text; result.typed_value = real(data.text); return result;
  }
  if (type == "boolean" && data.kind == JsonValue::boolean) {
    result.kind = AttributeKind::kBoolean; result.value = data.text; result.typed_value = data.text == "true"; return result;
  }
  if (type == "date" && data.kind == JsonValue::string) {
    const auto parsed = parse_timestamp_exact(data.text);
    if (!parsed) throw std::invalid_argument("Invalid repetition datetime target");
    result.kind = AttributeKind::kDate; result.value = data.text;
    result.typed_value = NativeDate{data.text, NativeDateResolution::kNanosecond}; return result;
  }
  if (type == "map" && data.kind == JsonValue::object) {
    result.container = AttributeContainerKind::kContainer;
    for (const auto& [name, child] : data.fields) result.children.push_back(target(child, name));
    return result;
  }
  if ((type == "list" || type == "tuple" || type == "set" || type == "frozenset") && data.kind == JsonValue::array) {
    result.container = type == "list" ? AttributeContainerKind::kList : type == "tuple" ? AttributeContainerKind::kTuple : type == "set" ? AttributeContainerKind::kSet : AttributeContainerKind::kFrozenSet;
    for (std::size_t i = 0; i < data.items.size(); ++i) result.children.push_back(target(data.items[i], std::to_string(i)));
    if (type == "set" || type == "frozenset")
      for (const auto& child : result.children)
        if (!root_cause_key_is_hashable(child)) throw std::invalid_argument("Repetition set target requires immutable members");
    return result;
  }
  throw std::invalid_argument("Invalid typed repetition target");
}
}  // namespace

EventAttribute parse_log_attribute_repetition_target_json(std::string_view input) {
  return target(Reader(input).read());
}

LogAttributeRepetitionBound parse_log_attribute_repetition_bound_json(std::string_view input) {
  const auto start = input.find_first_not_of(" \t\r\n"), end = input.find_last_not_of(" \t\r\n");
  if (start != std::string_view::npos) {
    const auto token = input.substr(start, end - start + 1);
    if (token == "NaN" || token == "INF" || token == "+INF" || token == "-INF") return real(token);
  }
  const auto value = Reader(input).read();
  if (value.kind == JsonValue::null) return nullptr;
  if (value.kind == JsonValue::boolean) return value.text == "true";
  if (value.kind == JsonValue::string) return value.text;
  if (value.kind == JsonValue::number) {
    if (value.text.find_first_of(".eE") == std::string::npos) return TokenInteger(value.text);
    return real(value.text);
  }
  throw std::invalid_argument("Repetition bounds require numeric, boolean, null or string scalars");
}
}  // namespace minera
