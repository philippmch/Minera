#include "minera/declare_workbench_json.hpp"
#include "minera/graph_alignment_workbench_json.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <initializer_list>
#include <limits>
#include <locale>
#include <map>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace minera {
namespace {
[[noreturn]] void invalid(std::string message) {
  throw std::invalid_argument("Declare workbench JSON: " + message);
}

struct Number { std::string text; };
struct Json {
  using Array = std::vector<Json>;
  using Object = std::map<std::string, Json, std::less<>>;
  std::variant<std::nullptr_t, bool, Number, std::string, Array, Object> value;
  bool null() const { return std::holds_alternative<std::nullptr_t>(value); }
  const Object& object() const {
    if (const auto* result = std::get_if<Object>(&value)) return *result;
    invalid("expected an object");
  }
  const Array& array() const {
    if (const auto* result = std::get_if<Array>(&value)) return *result;
    invalid("expected an array");
  }
  const std::string& string() const {
    if (const auto* result = std::get_if<std::string>(&value)) return *result;
    invalid("expected a string");
  }
  bool boolean() const {
    if (const auto* result = std::get_if<bool>(&value)) return *result;
    invalid("expected a boolean");
  }
};

void append_utf8(std::string& output, std::uint32_t value) {
  if (value <= 0x7f) output += static_cast<char>(value);
  else if (value <= 0x7ff) {
    output += static_cast<char>(0xc0 | (value >> 6));
    output += static_cast<char>(0x80 | (value & 63));
  } else if (value <= 0xffff) {
    output += static_cast<char>(0xe0 | (value >> 12));
    output += static_cast<char>(0x80 | ((value >> 6) & 63));
    output += static_cast<char>(0x80 | (value & 63));
  } else {
    output += static_cast<char>(0xf0 | (value >> 18));
    output += static_cast<char>(0x80 | ((value >> 12) & 63));
    output += static_cast<char>(0x80 | ((value >> 6) & 63));
    output += static_cast<char>(0x80 | (value & 63));
  }
}

// This parser retains numeric tokens rather than rounding JSON integers to
// double. Its grammar, UTF-8, duplicate keys, size and recursion are checked.
class Parser {
 public:
  explicit Parser(std::string_view text, bool escaped_lone_surrogates = true,
                  bool bounded_transport = true)
      : text_(text), escaped_lone_surrogates_(escaped_lone_surrogates),
        bounded_transport_(bounded_transport) {
    if (bounded_transport_ && text.size() > 32u * 1024u * 1024u)
      invalid("request exceeds 32 MiB");
  }
  Json parse() {
    auto result = value(0);
    whitespace();
    if (position_ != text_.size()) invalid("trailing data");
    return result;
  }
 private:
  bool digit() const { return position_ < text_.size() && text_[position_] >= '0' && text_[position_] <= '9'; }
  void whitespace() {
    while (position_ < text_.size() && (text_[position_] == ' ' || text_[position_] == '\t' || text_[position_] == '\n' || text_[position_] == '\r')) ++position_;
  }
  bool take(char c) {
    whitespace();
    if (position_ < text_.size() && text_[position_] == c) { ++position_; return true; }
    return false;
  }
  void require(char c) { if (!take(c)) invalid("unexpected character"); }
  std::uint32_t hex4() {
    std::uint32_t result = 0;
    for (int i = 0; i < 4; ++i) {
      if (position_ == text_.size()) invalid("short Unicode escape");
      const char c = text_[position_++];
      const int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
      if (d < 0) invalid("invalid Unicode escape");
      result = (result << 4) | static_cast<unsigned>(d);
    }
    return result;
  }
  std::string string() {
    require('"');
    std::string result;
    while (position_ < text_.size()) {
      const auto c = static_cast<unsigned char>(text_[position_++]);
      if (c == '"') return result;
      if (c < 0x20) invalid("unescaped control character");
      if (c >= 0x80) {
        const int count = c >= 0xc2 && c <= 0xdf ? 1 : c >= 0xe0 && c <= 0xef ? 2 : c >= 0xf0 && c <= 0xf4 ? 3 : -1;
        if (count < 0) invalid("invalid UTF-8");
        std::uint32_t code = c & (count == 1 ? 31 : count == 2 ? 15 : 7);
        for (int i = 0; i < count; ++i) {
          if (position_ == text_.size()) invalid("short UTF-8 sequence");
          const auto next = static_cast<unsigned char>(text_[position_++]);
          if ((next & 0xc0) != 0x80) invalid("invalid UTF-8 continuation");
          code = (code << 6) | (next & 63);
        }
        if ((count == 1 && code < 0x80) || (count == 2 && code < 0x800) || (count == 3 && code < 0x10000) || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) invalid("invalid UTF-8 scalar");
        append_utf8(result, code);
        continue;
      }
      if (c != '\\') { result += static_cast<char>(c); continue; }
      if (position_ == text_.size()) invalid("unterminated escape");
      switch (text_[position_++]) {
        case '"': result += '"'; break;
        case '\\': result += '\\'; break;
        case '/': result += '/'; break;
        case 'b': result += '\b'; break;
        case 'f': result += '\f'; break;
        case 'n': result += '\n'; break;
        case 'r': result += '\r'; break;
        case 't': result += '\t'; break;
        case 'u': {
          auto code = hex4();
          if (code >= 0xd800 && code <= 0xdbff) {
            if (position_ + 2 <= text_.size() && text_[position_] == '\\' && text_[position_ + 1] == 'u') {
              const auto saved = position_;
              position_ += 2;
              const auto low = hex4();
              if (low >= 0xdc00 && low <= 0xdfff)
                code = 0x10000 + ((code - 0xd800) << 10) + low - 0xdc00;
              else {
                if (!escaped_lone_surrogates_) invalid("unpaired Unicode surrogate");
                position_ = saved;
              }
            } else if (!escaped_lone_surrogates_) invalid("unpaired Unicode surrogate");
          } else if (code >= 0xdc00 && code <= 0xdfff && !escaped_lone_surrogates_)
            invalid("unpaired Unicode surrogate");
          append_utf8(result, code);
          break;
        }
        default: invalid("unknown escape");
      }
    }
    invalid("unterminated string");
  }
  Json value(unsigned depth) {
    if (bounded_transport_ && depth > 64) invalid("nesting exceeds 64 levels");
    if (++nodes_ > 1000000 && bounded_transport_) invalid("request exceeds one million values");
    whitespace();
    if (position_ == text_.size()) invalid("unexpected end of input");
    const auto c = text_[position_];
    if (c == '"') return Json{string()};
    if (take('{')) {
      Json::Object result;
      if (!take('}')) {
        do {
          auto key = string();
          require(':');
          if (!result.emplace(std::move(key), value(depth + 1)).second) invalid("duplicate object key");
        } while (take(','));
        require('}');
      }
      return Json{std::move(result)};
    }
    if (take('[')) {
      Json::Array result;
      if (!take(']')) {
        do { result.push_back(value(depth + 1)); } while (take(','));
        require(']');
      }
      return Json{std::move(result)};
    }
    for (const auto token : {std::string_view("null"), std::string_view("true"), std::string_view("false")}) {
      if (text_.substr(position_, token.size()) == token) {
        position_ += token.size();
        if (token == "null") return Json{nullptr};
        return Json{token == "true"};
      }
    }
    const auto start = position_;
    if (text_[position_] == '-') ++position_;
    if (!digit()) invalid("expected a value");
    if (text_[position_] == '0') ++position_;
    else while (digit()) ++position_;
    if (position_ < text_.size() && text_[position_] == '.') {
      ++position_;
      if (!digit()) invalid("missing fractional digits");
      while (digit()) ++position_;
    }
    if (position_ < text_.size() && (text_[position_] == 'e' || text_[position_] == 'E')) {
      ++position_;
      if (position_ < text_.size() && (text_[position_] == '+' || text_[position_] == '-')) ++position_;
      if (!digit()) invalid("missing exponent digits");
      while (digit()) ++position_;
    }
    return Json{Number{std::string(text_.substr(start, position_ - start))}};
  }
  std::string_view text_;
  bool escaped_lone_surrogates_ = false;
  bool bounded_transport_ = true;
  std::size_t position_ = 0, nodes_ = 0;
};

void allowed_fields(const Json::Object& object,
                    std::initializer_list<std::string_view> allowed) {
  for (const auto& [name, value] : object) {
    (void)value;
    if (std::find(allowed.begin(), allowed.end(), name) == allowed.end())
      invalid("unknown field " + name);
  }
}

const Json* field(const Json::Object& object, std::string_view name) {
  const auto found = object.find(name);
  return found == object.end() ? nullptr : &found->second;
}

std::uint64_t unsigned_integer(const Json& value) {
  const auto* number = std::get_if<Number>(&value.value);
  if (!number || number->text.empty() ||
      !std::all_of(number->text.begin(), number->text.end(),
                   [](char c) { return c >= '0' && c <= '9'; }))
    invalid("expected an unsigned JSON integer token");
  std::uint64_t result = 0;
  const auto parsed = std::from_chars(number->text.data(),
      number->text.data() + number->text.size(), result);
  if (parsed.ec != std::errc{} || parsed.ptr != number->text.data() + number->text.size())
    invalid("unsigned integer exceeds uint64 range");
  return result;
}

template <class Integer>
Integer native_unsigned(const Json& value) {
  const auto result = unsigned_integer(value);
  if (result > std::numeric_limits<Integer>::max())
    invalid("integer exceeds native unsigned storage range");
  return static_cast<Integer>(result);
}

int native_int(const Json& value) {
  const auto* number = std::get_if<Number>(&value.value);
  if (!number || number->text.empty() ||
      number->text.find_first_of(".eE") != std::string::npos)
    invalid("expected an exact native int token");
  int result = 0;
  const auto parsed = std::from_chars(number->text.data(),
      number->text.data() + number->text.size(), result);
  if (parsed.ec != std::errc{} || parsed.ptr != number->text.data() + number->text.size())
    invalid("integer exceeds native int storage range");
  return result;
}

double ieee_double(const Json& value) {
  if (const auto* tag = std::get_if<std::string>(&value.value)) {
    if (*tag == "NaN") return std::numeric_limits<double>::quiet_NaN();
    if (*tag == "INF") return std::numeric_limits<double>::infinity();
    if (*tag == "-INF") return -std::numeric_limits<double>::infinity();
    invalid("expected an IEEE double tag NaN, INF, or -INF");
  }
  const auto* number = std::get_if<Number>(&value.value);
  if (!number) invalid("expected a numeric double or IEEE tag");
  double result = 0;
  const auto parsed = std::from_chars(number->text.data(),
      number->text.data() + number->text.size(), result,
      std::chars_format::general);
  if (parsed.ec == std::errc{} && parsed.ptr == number->text.data() + number->text.size())
    return result;
  // from_chars reports range errors for decimal tokens that round below the
  // smallest subnormal as well as for overflow. Preserve signed underflow
  // zero without accepting numeric overflow as an implicit infinity tag.
  if (parsed.ec == std::errc::result_out_of_range) {
    std::istringstream input(number->text);
    input.imbue(std::locale::classic());
    input >> result;
    if (result == 0.0 && input.eof())
      return !number->text.empty() && number->text.front() == '-' ? -0.0 : 0.0;
  }
  invalid("double token exceeds finite native storage; use an explicit IEEE tag");
}

template <class T, class Convert>
std::vector<T> vector_value(const Json& value, Convert convert) {
  std::vector<T> result;
  result.reserve(value.array().size());
  for (const auto& item : value.array()) result.push_back(convert(item));
  return result;
}

void quoted(std::ostream& out, const std::string& value) {
  const auto array = serialize_alignment_trace_words_json({value});
  out.write(array.data() + 1, static_cast<std::streamsize>(array.size() - 2));
}

void number(std::ostream& out, double value) {
  if (std::isnan(value)) out << "\"NaN\"";
  else if (std::isinf(value)) out << (value < 0 ? "\"-INF\"" : "\"INF\"");
  else if (value == 0.0 && std::signbit(value)) out << "-0.0";
  else out << std::setprecision(std::numeric_limits<double>::max_digits10) << value;
}

template <class T, class Write>
void array_json(std::ostream& out, const std::vector<T>& values, Write write) {
  out << '[';
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i) out << ',';
    write(out, values[i]);
  }
  out << ']';
}

template <class Integer>
void integer(std::ostream& out, Integer value) { out << value; }

void boolean(std::ostream& out, bool value) { out << (value ? "true" : "false"); }

std::ostringstream output() {
  std::ostringstream result;
  result.imbue(std::locale::classic());
  return result;
}

const Json& required(const Json::Object& object, std::string_view name) {
  if (const auto* value = field(object, name)) return *value;
  invalid("missing field " + std::string(name));
}

std::int64_t signed_integer(const Json& value) {
  const auto* token = std::get_if<Number>(&value.value);
  if (!token || token->text.empty() || token->text.find_first_of(".eE") != std::string::npos)
    invalid("expected an exact int64 integer token");
  std::int64_t result = 0;
  const auto parsed = std::from_chars(token->text.data(), token->text.data() + token->text.size(), result);
  if (parsed.ec != std::errc{} || parsed.ptr != token->text.data() + token->text.size())
    invalid("integer exceeds int64 storage range");
  return result;
}

void set_supplied(const Json::Object& object, std::set<std::string>* supplied) {
  if (!supplied) return;
  std::set<std::string> actual;
  for (const auto& [key, value] : object) { (void)value; actual.insert(key); }
  *supplied = std::move(actual);
}

DeclareConstraint rule_value(const Json& value) {
  const auto& object = value.object();
  allowed_fields(object, {"first", "second", "binary", "support", "confidence"});
  DeclareConstraint result;
  result.first = required(object, "first").string();
  result.second = required(object, "second").string();
  result.binary = required(object, "binary").boolean();
  result.support = native_int(required(object, "support"));
  result.confidence = native_int(required(object, "confidence"));
  return result;
}

void rule_json(std::ostream& out, const DeclareConstraint& rule) {
  out << "{\"first\":"; quoted(out, rule.first);
  out << ",\"second\":"; quoted(out, rule.second);
  out << ",\"binary\":"; boolean(out, rule.binary);
  out << ",\"support\":" << rule.support << ",\"confidence\":" << rule.confidence << '}';
}

void deviation_json(std::ostream& out, const DeclareDeviation& value) {
  out << "{\"template_name\":"; quoted(out, value.template_name);
  out << ",\"first\":"; quoted(out, value.first);
  out << ",\"second\":"; quoted(out, value.second);
  out << ",\"binary\":"; boolean(out, value.binary); out << '}';
}

void conformance_json(std::ostream& out, const DeclareConformance& result) {
  out << "{\"deviations\":"; array_json(out, result.deviations, deviation_json);
  out << ",\"no_constr_total\":" << result.no_constr_total;
  out << ",\"no_dev_total\":" << result.no_dev_total;
  out << ",\"dev_fitness\":"; number(out, result.dev_fitness);
  out << ",\"is_fit\":"; boolean(out, result.is_fit); out << '}';
}

NativeAttributeValue typed_value(const Json& value) {
  const auto& object = value.object();
  allowed_fields(object, {"index", "value"});
  const auto index = unsigned_integer(required(object, "index"));
  const auto& payload = required(object, "value");
  switch (index) {
    case 0:
      if (!payload.null()) invalid("monostate payload must be null");
      return NativeAttributeValue{};
    case 1: return NativeAttributeValue{std::in_place_type<std::int64_t>, signed_integer(payload)};
    case 2: return NativeAttributeValue{std::in_place_type<double>, ieee_double(payload)};
    case 3: return NativeAttributeValue{std::in_place_type<bool>, payload.boolean()};
    case 4: {
      const auto& date = payload.object();
      allowed_fields(date, {"value", "resolution_index", "timezone"});
      NativeDate result;
      if (const auto* item = field(date, "value")) result.value = item->string();
      if (const auto* item = field(date, "resolution_index")) result.resolution = static_cast<NativeDateResolution>(native_int(*item));
      if (const auto* item = field(date, "timezone")) result.timezone = item->null() ? std::nullopt : std::optional<std::string>{item->string()};
      return NativeAttributeValue{std::in_place_type<NativeDate>, std::move(result)};
    }
    case 5: return NativeAttributeValue{std::in_place_type<std::uint64_t>, unsigned_integer(payload)};
    case 6:
      if (payload.string() != "NA") invalid("native NA payload must be NA");
      return NativeAttributeValue{std::in_place_type<NativeNA>, NativeNA{}};
    default: invalid("typed value index must be from 0 through 6");
  }
}

EventAttribute attribute_value(const Json& value) {
  const auto& object = value.object();
  allowed_fields(object, {"key", "value", "kind_index", "container_index", "null",
      "scalar_wrapper", "scalar_children_list", "first_class_role_index", "first_class_state_index",
      "first_class_source_key", "typed_value", "children"});
  EventAttribute result;
  if (const auto* item = field(object, "key")) result.key = item->string();
  if (const auto* item = field(object, "value")) result.value = item->string();
  if (const auto* item = field(object, "kind_index")) result.kind = static_cast<AttributeKind>(native_int(*item));
  if (const auto* item = field(object, "container_index")) result.container = static_cast<AttributeContainerKind>(native_int(*item));
  if (const auto* item = field(object, "null")) result.is_null = item->boolean();
  if (const auto* item = field(object, "scalar_wrapper")) result.scalar_wrapper = item->boolean();
  if (const auto* item = field(object, "scalar_children_list")) result.scalar_children_list = item->boolean();
  if (const auto* item = field(object, "first_class_role_index")) result.first_class_role = static_cast<FirstClassAttributeRole>(native_int(*item));
  if (const auto* item = field(object, "first_class_state_index")) result.first_class_state = static_cast<FirstClassAttributeState>(native_int(*item));
  if (const auto* item = field(object, "first_class_source_key")) result.first_class_source_key = item->string();
  if (const auto* item = field(object, "typed_value")) result.typed_value = typed_value(*item);
  if (const auto* item = field(object, "children")) result.children = vector_value<EventAttribute>(*item, attribute_value);
  return result;
}

std::optional<EventAttribute> optional_attribute(const Json& value) {
  return value.null() ? std::nullopt : std::optional<EventAttribute>{attribute_value(value)};
}

Event event_value(const Json& value) {
  const auto& object = value.object();
  allowed_fields(object, {"case_id", "activity", "timestamp", "resource", "lifecycle", "input_order", "attributes",
      "case_id_state_index", "activity_state_index", "timestamp_state_index", "resource_state_index", "lifecycle_state_index",
      "case_id_source_key", "activity_source_key", "timestamp_source_key", "resource_source_key", "lifecycle_source_key",
      "activity_attribute", "timestamp_attribute", "resource_attribute", "lifecycle_attribute"});
  Event result;
  if (const auto* item = field(object, "case_id")) result.case_id = item->string();
  if (const auto* item = field(object, "activity")) result.activity = item->string();
  if (const auto* item = field(object, "timestamp")) result.timestamp = item->string();
  if (const auto* item = field(object, "resource")) result.resource = item->string();
  if (const auto* item = field(object, "lifecycle")) result.lifecycle = item->string();
  if (const auto* item = field(object, "input_order")) result.input_order = native_unsigned<std::size_t>(*item);
  if (const auto* item = field(object, "attributes")) result.attributes = vector_value<EventAttribute>(*item, attribute_value);
  if (const auto* item = field(object, "case_id_state_index")) result.case_id_state = static_cast<FirstClassAttributeState>(native_int(*item));
  if (const auto* item = field(object, "activity_state_index")) result.activity_state = static_cast<FirstClassAttributeState>(native_int(*item));
  if (const auto* item = field(object, "timestamp_state_index")) result.timestamp_state = static_cast<FirstClassAttributeState>(native_int(*item));
  if (const auto* item = field(object, "resource_state_index")) result.resource_state = static_cast<FirstClassAttributeState>(native_int(*item));
  if (const auto* item = field(object, "lifecycle_state_index")) result.lifecycle_state = static_cast<FirstClassAttributeState>(native_int(*item));
  if (const auto* item = field(object, "case_id_source_key")) result.case_id_source_key = item->string();
  if (const auto* item = field(object, "activity_source_key")) result.activity_source_key = item->string();
  if (const auto* item = field(object, "timestamp_source_key")) result.timestamp_source_key = item->string();
  if (const auto* item = field(object, "resource_source_key")) result.resource_source_key = item->string();
  if (const auto* item = field(object, "lifecycle_source_key")) result.lifecycle_source_key = item->string();
  if (const auto* item = field(object, "activity_attribute")) result.activity_attribute = optional_attribute(*item);
  if (const auto* item = field(object, "timestamp_attribute")) result.timestamp_attribute = optional_attribute(*item);
  if (const auto* item = field(object, "resource_attribute")) result.resource_attribute = optional_attribute(*item);
  if (const auto* item = field(object, "lifecycle_attribute")) result.lifecycle_attribute = optional_attribute(*item);
  return result;
}

void typed_json(std::ostream& out, const NativeAttributeValue& value) {
  out << "{\"index\":" << value.index() << ",\"value\":";
  std::visit([&](const auto& payload) {
    using T = std::decay_t<decltype(payload)>;
    if constexpr (std::is_same_v<T, std::monostate>) out << "null";
    else if constexpr (std::is_same_v<T, double>) number(out, payload);
    else if constexpr (std::is_same_v<T, bool>) boolean(out, payload);
    else if constexpr (std::is_same_v<T, NativeNA>) quoted(out, "NA");
    else if constexpr (std::is_same_v<T, NativeDate>) {
      out << "{\"value\":"; quoted(out, payload.value);
      out << ",\"resolution_index\":" << static_cast<int>(payload.resolution) << ",\"timezone\":";
      if (payload.timezone) quoted(out, *payload.timezone); else out << "null";
      out << '}';
    } else out << payload;
  }, static_cast<const NativeAttributeValue::Base&>(value));
  out << '}';
}

void attribute_json(std::ostream& out, const EventAttribute& value) {
  out << "{\"key\":"; quoted(out, value.key); out << ",\"value\":"; quoted(out, value.value);
  out << ",\"kind_index\":" << static_cast<int>(value.kind);
  out << ",\"container_index\":" << static_cast<int>(value.container);
  out << ",\"null\":"; boolean(out, value.is_null);
  out << ",\"scalar_wrapper\":"; boolean(out, value.scalar_wrapper);
  out << ",\"scalar_children_list\":"; boolean(out, value.scalar_children_list);
  out << ",\"first_class_role_index\":" << static_cast<int>(value.first_class_role);
  out << ",\"first_class_state_index\":" << static_cast<int>(value.first_class_state);
  out << ",\"first_class_source_key\":"; quoted(out, value.first_class_source_key);
  out << ",\"typed_value\":"; typed_json(out, value.typed_value);
  out << ",\"children\":"; array_json(out, value.children, attribute_json); out << '}';
}

void optional_attribute_json(std::ostream& out, const std::optional<EventAttribute>& value) {
  if (value) attribute_json(out, *value); else out << "null";
}

void event_json(std::ostream& out, const Event& value) {
  out << "{\"case_id\":"; quoted(out, value.case_id);
  out << ",\"activity\":"; quoted(out, value.activity);
  out << ",\"timestamp\":"; quoted(out, value.timestamp);
  out << ",\"resource\":"; quoted(out, value.resource);
  out << ",\"lifecycle\":"; quoted(out, value.lifecycle);
  out << ",\"input_order\":" << value.input_order << ",\"attributes\":";
  array_json(out, value.attributes, attribute_json);
  out << ",\"case_id_state_index\":" << static_cast<int>(value.case_id_state);
  out << ",\"activity_state_index\":" << static_cast<int>(value.activity_state);
  out << ",\"timestamp_state_index\":" << static_cast<int>(value.timestamp_state);
  out << ",\"resource_state_index\":" << static_cast<int>(value.resource_state);
  out << ",\"lifecycle_state_index\":" << static_cast<int>(value.lifecycle_state);
  out << ",\"case_id_source_key\":"; quoted(out, value.case_id_source_key);
  out << ",\"activity_source_key\":"; quoted(out, value.activity_source_key);
  out << ",\"timestamp_source_key\":"; quoted(out, value.timestamp_source_key);
  out << ",\"resource_source_key\":"; quoted(out, value.resource_source_key);
  out << ",\"lifecycle_source_key\":"; quoted(out, value.lifecycle_source_key);
  out << ",\"activity_attribute\":"; optional_attribute_json(out, value.activity_attribute);
  out << ",\"timestamp_attribute\":"; optional_attribute_json(out, value.timestamp_attribute);
  out << ",\"resource_attribute\":"; optional_attribute_json(out, value.resource_attribute);
  out << ",\"lifecycle_attribute\":"; optional_attribute_json(out, value.lifecycle_attribute); out << '}';
}

}  // namespace

DeclareModel parse_declare_workbench_model(std::string_view input) {
  const auto document = Parser(input).parse();
  const auto& object = document.object();
  allowed_fields(object, {"rules"});
  DeclareModel result;
  result.rules.clear();
  for (const auto& [family, rules] : required(object, "rules").object())
    result.rules.emplace(family, vector_value<DeclareConstraint>(rules, rule_value));
  return result;
}

std::string serialize_declare_workbench_model_json(const DeclareModel& model) {
  auto out = output(); out << "{\"rules\":{";
  bool comma = false;
  for (const auto& [family, rules] : model.rules) {
    if (comma) out << ','; comma = true;
    quoted(out, family); out << ':'; array_json(out, rules, rule_json);
  }
  out << "}}"; return out.str();
}

DeclareConformanceOptions parse_declare_conformance_workbench_options(std::string_view input, std::set<std::string>* supplied) {
  const auto document = Parser(input).parse(); const auto& object = document.object();
  allowed_fields(object, {"activity_key", "case_id_key"});
  DeclareConformanceOptions result;
  if (const auto* item = field(object, "activity_key")) result.activity_key = item->string();
  if (const auto* item = field(object, "case_id_key")) result.case_id_key = item->string();
  set_supplied(object, supplied); return result;
}

DeclareDiagnosticsOptions parse_declare_diagnostics_workbench_options(std::string_view input, std::set<std::string>* supplied) {
  const auto document = Parser(input).parse(); const auto& object = document.object();
  allowed_fields(object, {"case_id_key"}); DeclareDiagnosticsOptions result;
  if (const auto* item = field(object, "case_id_key")) result.case_id_key = item->string();
  set_supplied(object, supplied); return result;
}

DeclarePlayoutOptions parse_declare_playout_workbench_options(std::string_view input, std::set<std::string>* supplied) {
  const auto document = Parser(input).parse(); const auto& object = document.object();
  allowed_fields(object, {"n_traces", "min_length", "max_length", "seed"}); DeclarePlayoutOptions result;
  if (const auto* item = field(object, "n_traces")) result.n_traces = native_unsigned<std::size_t>(*item);
  if (const auto* item = field(object, "min_length")) result.min_length = native_unsigned<std::size_t>(*item);
  if (const auto* item = field(object, "max_length")) result.max_length = native_unsigned<std::size_t>(*item);
  if (const auto* item = field(object, "seed")) result.seed = item->null() ? std::nullopt : std::optional<std::uint64_t>{unsigned_integer(*item)};
  set_supplied(object, supplied); return result;
}

std::string serialize_declare_workbench_options_json(const DeclareConformanceOptions& value) {
  auto out = output(); out << "{\"activity_key\":"; quoted(out, value.activity_key);
  out << ",\"case_id_key\":"; quoted(out, value.case_id_key); out << '}'; return out.str();
}
std::string serialize_declare_workbench_options_json(const DeclareDiagnosticsOptions& value) {
  auto out = output(); out << "{\"case_id_key\":"; quoted(out, value.case_id_key); out << '}'; return out.str();
}
std::string serialize_declare_workbench_options_json(const DeclarePlayoutOptions& value) {
  auto out = output(); out << "{\"n_traces\":" << value.n_traces << ",\"min_length\":" << value.min_length << ",\"max_length\":" << value.max_length << ",\"seed\":";
  if (value.seed) out << *value.seed; else out << "null"; out << '}'; return out.str();
}
std::string serialize_declare_workbench_conformance_json(const DeclareConformance& value) {
  auto out = output(); conformance_json(out, value); return out.str();
}
std::string serialize_declare_workbench_conformance_json(const std::vector<DeclareConformance>& values) {
  auto out = output(); array_json(out, values, conformance_json); return out.str();
}

Trace parse_declare_workbench_trace_json(std::string_view input) {
  const auto document = Parser(input).parse(); const auto& object = document.object();
  allowed_fields(object, {"case_id", "case_id_state_index", "case_id_source_key", "case_id_attribute", "attributes", "properties", "events"});
  Trace result;
  if (const auto* item = field(object, "case_id")) result.case_id = item->string();
  if (const auto* item = field(object, "case_id_state_index")) result.case_id_state = static_cast<FirstClassAttributeState>(native_int(*item));
  if (const auto* item = field(object, "case_id_source_key")) result.case_id_source_key = item->string();
  if (const auto* item = field(object, "case_id_attribute")) result.case_id_attribute = optional_attribute(*item);
  if (const auto* item = field(object, "attributes")) result.attributes = vector_value<EventAttribute>(*item, attribute_value);
  if (const auto* item = field(object, "properties")) result.properties = vector_value<EventAttribute>(*item, attribute_value);
  if (const auto* item = field(object, "events")) result.events = vector_value<Event>(*item, event_value);
  return result;
}

std::string serialize_declare_workbench_trace_json(const Trace& value) {
  auto out = output(); out << "{\"case_id\":"; quoted(out, value.case_id);
  out << ",\"case_id_state_index\":" << static_cast<int>(value.case_id_state);
  out << ",\"case_id_source_key\":"; quoted(out, value.case_id_source_key);
  out << ",\"case_id_attribute\":"; optional_attribute_json(out, value.case_id_attribute);
  out << ",\"attributes\":"; array_json(out, value.attributes, attribute_json);
  out << ",\"properties\":"; array_json(out, value.properties, attribute_json);
  out << ",\"events\":"; array_json(out, value.events, event_json); out << '}'; return out.str();
}

}  // namespace minera
