#include "minera/exact_filter_discovery_json.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/process_tree_alignment_workbench_json.hpp"
#include "minera/active_event_source.hpp"
#include "minera/object_centric_playout_workbench_json.hpp"
#include "minera/xes_export.hpp"
#include "minera/footprints_workbench_json.hpp"
#include "minera/workflow_discovery_input_json.hpp"
#include <cctype>
#include <tuple>
#include <array>
#include <fstream>
#include "minera/literal_controls_json.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <initializer_list>
#include <limits>
#include <locale>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>
namespace minera { namespace {
[[noreturn]] void invalid(std::string message) {
  throw std::invalid_argument("Exact filter/discovery JSON: " + message);
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
  std::string string() const {
    if (const auto* result = std::get_if<std::string>(&value)) return *result;
    const auto* object = std::get_if<Object>(&value);
    if(!object||object->size()!=2||!object->contains("native_bytes")||!object->contains("data"))invalid("expected a NativeString");
    const auto* tag=std::get_if<std::string>(&object->at("native_bytes").value);const auto* hex=std::get_if<std::string>(&object->at("data").value);
    if(!tag||*tag!="hex-v1"||!hex||hex->size()%2)invalid("invalid NativeString byte carrier");
    const auto digit=[](char c)->int{return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;};std::string out;out.reserve(hex->size()/2);
    for(std::size_t i=0;i<hex->size();i+=2){const int a=digit((*hex)[i]),b=digit((*hex)[i+1]);if(a<0||b<0)invalid("invalid NativeString hexadecimal bytes");out+=static_cast<char>((a<<4)|b);}return out;
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
// double. Grammar, UTF8/WTF8 and duplicate keys are checked; there is no
// independent byte/node/depth cap beyond transport and process resources.
class Parser {
 public:
  explicit Parser(std::string_view text):text_(text){}
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
        if ((count == 1 && code < 0x80) || (count == 2 && code < 0x800) || (count == 3 && code < 0x10000) || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff && !escaped_lone_surrogates_)) invalid("invalid UTF-8 scalar");
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
  bool escaped_lone_surrogates_ = true;
  std::size_t position_ = 0;
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
  out << serialize_embedding_workbench_string_json(value);
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



Trace trace_value(const Json& document) {
  const auto& object = document.object();
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


std::vector<EventAttribute> attributes_value(const Json& v){return vector_value<EventAttribute>(v,attribute_value);}
LogGlobal global_value(const Json& value){const auto& o=value.object();allowed_fields(o,{"scope","attributes"});LogGlobal r;if(const auto* v=field(o,"scope"))r.scope=v->string();if(const auto* v=field(o,"attributes"))r.attributes=attributes_value(*v);return r;}
EventDataMetadata metadata_value(const Json& value){const auto& o=value.object();allowed_fields(o,{"extensions","classifiers","trace_globals","event_globals","properties","globals","compatibility_snapshot_valid","compatibility_globals_snapshot","compatibility_trace_globals_snapshot","compatibility_event_globals_snapshot"});EventDataMetadata r;
 if(const auto* v=field(o,"extensions"))for(const auto& x:v->array()){const auto& e=x.object();allowed_fields(e,{"name","prefix","uri"});LogExtension a;if(const auto* y=field(e,"name"))a.name=y->string();if(const auto* y=field(e,"prefix"))a.prefix=y->string();if(const auto* y=field(e,"uri"))a.uri=y->string();r.extensions.push_back(std::move(a));}
 if(const auto* v=field(o,"classifiers"))for(const auto& x:v->array()){const auto& e=x.object();allowed_fields(e,{"name","keys"});LogClassifier a;if(const auto* y=field(e,"name"))a.name=y->string();if(const auto* y=field(e,"keys"))a.keys=vector_value<std::string>(*y,[](const Json& x){return x.string();});r.classifiers.push_back(std::move(a));}
 if(const auto* v=field(o,"trace_globals"))r.trace_globals=attributes_value(*v);if(const auto* v=field(o,"event_globals"))r.event_globals=attributes_value(*v);if(const auto* v=field(o,"properties"))r.properties=attributes_value(*v);
 if(const auto* v=field(o,"globals"))r.globals=vector_value<LogGlobal>(*v,global_value);if(const auto* v=field(o,"compatibility_snapshot_valid"))r.compatibility_snapshot_valid=v->boolean();if(const auto* v=field(o,"compatibility_globals_snapshot"))r.compatibility_globals_snapshot=vector_value<LogGlobal>(*v,global_value);if(const auto* v=field(o,"compatibility_trace_globals_snapshot"))r.compatibility_trace_globals_snapshot=attributes_value(*v);if(const auto* v=field(o,"compatibility_event_globals_snapshot"))r.compatibility_event_globals_snapshot=attributes_value(*v);return r;
}

template<class F>void attribute_strings(EventAttribute& a,const F& f){f(a.key);f(a.value);f(a.first_class_source_key);if(auto* date=std::get_if<NativeDate>(&a.typed_value)){f(date->value);if(date->timezone)f(*date->timezone);}for(auto& child:a.children)attribute_strings(child,f);}
template<class F>void attributes_strings(std::vector<EventAttribute>& a,const F& f){for(auto& x:a)attribute_strings(x,f);}
template<class F>void metadata_strings(EventDataMetadata& m,const F& f){for(auto& x:m.extensions){f(x.name);f(x.prefix);f(x.uri);}for(auto& x:m.classifiers){f(x.name);for(auto& k:x.keys)f(k);}attributes_strings(m.trace_globals,f);attributes_strings(m.event_globals,f);attributes_strings(m.properties,f);const auto globals=[&](auto& values){for(auto& x:values){f(x.scope);attributes_strings(x.attributes,f);}};globals(m.globals);globals(m.compatibility_globals_snapshot);attributes_strings(m.compatibility_trace_globals_snapshot,f);attributes_strings(m.compatibility_event_globals_snapshot,f);}
template<class F>void log_strings(EventLog& log,const F& f){attributes_strings(log.attributes,f);metadata_strings(log.metadata,f);for(auto& t:log.traces){f(t.case_id);f(t.case_id_source_key);attributes_strings(t.attributes,f);attributes_strings(t.properties,f);if(t.case_id_attribute)attribute_strings(*t.case_id_attribute,f);for(auto& e:t.events){f(e.case_id);f(e.activity);f(e.timestamp);f(e.resource);f(e.lifecycle);f(e.case_id_source_key);f(e.activity_source_key);f(e.timestamp_source_key);f(e.resource_source_key);f(e.lifecycle_source_key);attributes_strings(e.attributes,f);for(auto* a:{&e.activity_attribute,&e.timestamp_attribute,&e.resource_attribute,&e.lifecycle_attribute})if(*a)attribute_strings(**a,f);}}}

const auto q=serialize_embedding_workbench_string_json;
std::string parts_json(const std::vector<DiscoveryWorkbenchPart>& parts){auto out=output();out<<'[';bool comma=false;for(const auto& p:parts){if(comma)out<<',';comma=true;out<<"{\"name\":"<<q(p.name)<<",\"filename\":"<<q(p.filename)<<",\"bytes\":"<<q(p.bytes)<<",\"file\":"<<(p.file?"true":"false")<<'}';}out<<']';return out.str();}
std::string unhex(std::string_view s){if(s.size()%2)invalid("odd hexadecimal byte string");const auto digit=[](char c)->int{return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;};std::string r;r.reserve(s.size()/2);for(std::size_t i=0;i<s.size();i+=2){const int a=digit(s[i]),b=digit(s[i+1]);if(a<0||b<0)invalid("invalid hexadecimal byte string");r+=static_cast<char>((a<<4)|b);}return r;}
EventLog log_value(const Json& value){const auto& o=value.object();if(const auto* tag=field(o,"native_byte_strings")){allowed_fields(o,{"native_byte_strings","value"});if(tag->string()!="hex-v1")invalid("unknown native raw log byte wrapper");auto log=log_value(required(o,"value"));log_strings(log,[](std::string& x){x=unhex(x);});return log;}allowed_fields(o,{"attributes","metadata","traces"});EventLog log;if(const auto* x=field(o,"attributes"))log.attributes=attributes_value(*x);if(const auto* x=field(o,"metadata"))log.metadata=metadata_value(*x);if(const auto* x=field(o,"traces"))log.traces=vector_value<Trace>(*x,trace_value);return log;}
template<class T>struct Vector:std::false_type{};
template<class T,class A>struct Vector<std::vector<T,A>>:std::true_type{};
template<class T>struct Optional:std::false_type{};
template<class T>struct Optional<std::optional<T>>:std::true_type{};
template<class T>struct Map:std::false_type{};
template<class K,class V,class C,class A>struct Map<std::map<K,V,C,A>>:std::true_type{};
template<class T>struct Set:std::false_type{};
template<class T,class C,class A>struct Set<std::set<T,C,A>>:std::true_type{};
template<class T>struct Pair:std::false_type{};
template<class A,class B>struct Pair<std::pair<A,B>>:std::true_type{};
template<class T>struct Tuple:std::false_type{};
template<class...T>struct Tuple<std::tuple<T...>>:std::true_type{};
template<class T>struct Variant:std::false_type{};
template<class...T>struct Variant<std::variant<T...>>:std::true_type{};
template<class Owner,class T>struct Member{std::string_view name;T Owner::*pointer;};
template<class Owner,class T>auto member(std::string_view name,T Owner::*p){return Member<Owner,T>{name,p};}
template<class T>struct Record;
#define F(name) member(#name,&V::name)
#define RECORD(Type,...) template<>struct Record<Type>{static auto members(){using V=Type;return std::make_tuple(__VA_ARGS__);}};
RECORD(BusinessHourSlot,F(start_seconds),F(end_seconds))
RECORD(HeuristicClassicDfgInput,F(dfg),F(activities),F(activity_counts),F(start_counts),F(end_counts),F(window_two_dfg),F(frequency_triples),F(performance_dfg),F(start_activities),F(end_activities),F(performance_values),F(dfg_window_2_supplied),F(freq_triples_supplied),F(dfg_order))
RECORD(HeuristicMinerOptions,F(variant),F(activity_attribute),F(timestamp_attribute),F(dependency_threshold),F(min_frequency),F(connect_all),F(and_threshold),F(min_activity_count),F(decoration),F(performance_aggregation),F(dfg_pre_cleaning_noise_threshold),F(loop_length_two_threshold),F(keep_once_per_case),F(most_common_paths),F(start_timestamp_attribute),F(lifecycle_instance_attribute),F(use_business_hours),F(business_hour_slots),F(business_hour_slots_explicit),F(case_id_attribute),F(plusplus_decoration_selector),F(plusplus_min_activity_count),F(plusplus_min_frequency),F(plusplus_decoration_value))
RECORD(HeuristicPerformanceSummary,F(mean),F(median),F(minimum),F(maximum),F(stdev),F(sum))
RECORD(HeuristicDependency,F(source),F(target),F(dependency),F(frequency),F(representation),F(length_two_loop),F(representation_value))
RECORD(HeuristicActivityStatistics,F(activity),F(occurrences),F(start_occurrences),F(end_occurrences),F(mean_sojourn_seconds),F(is_start_activity),F(is_end_activity))
RECORD(HeuristicRelationStatistics,F(source),F(target),F(eventually_follows_frequency),F(direct_adjacency_frequency),F(mean_wait_seconds),F(raw_frequency),F(cleaned_frequency),F(representation),F(length_two_loop),F(wait_samples),F(median_wait_seconds),F(min_wait_seconds),F(max_wait_seconds),F(stdev_wait_seconds),F(sum_wait_seconds),F(representation_value))
RECORD(HeuristicConcurrencyStatistics,F(first),F(second),F(frequency))
RECORD(HeuristicRawActivityStatistics,F(activity),F(occurrences),F(mean_sojourn_seconds))
RECORD(HeuristicRawConcurrencyStatistics,F(first),F(second),F(frequency))
RECORD(HeuristicAndMeasure,F(activity),F(first),F(second),F(value))
RECORD(HeuristicDependencyMeasure,F(source),F(target),F(value))
RECORD(HeuristicLoopLengthTwo,F(source),F(target),F(frequency))
RECORD(HeuristicNetMetadata,F(activities),F(activities_occurrences),F(start_activities),F(end_activities))
RECORD(HeuristicsNet,F(variant),F(decoration),F(performance_aggregation),F(activity_attribute),F(timestamp_attribute),F(activity_ids_are_local),F(activities),F(edges),F(start_activities),F(end_activities),F(active_activities),F(output_bundles),F(input_bundles),F(activity_statistics),F(sojourn_times),F(relation_statistics),F(concurrency_statistics),F(raw_activity_statistics),F(raw_concurrency_statistics),F(raw_dfg),F(cleaned_dfg),F(window_two_dfg),F(frequency_triples),F(and_measures_in),F(and_measures_out),F(dependency_measures),F(loop_length_two),F(metadata),F(performance_values),F(classic_public_dfg_window_2_present),F(classic_public_freq_triples_present),F(plusplus_public_dfg_window_2_present),F(plusplus_public_freq_triples_present),F(plusplus_computed_performance_dfg),F(plusplus_computed_performance_matrix))
#undef RECORD
#undef F
template<class T>T read_stored(const Json&);
template<class T,std::size_t...I>T read_tuple(const Json& j,std::index_sequence<I...>){const auto& a=j.array();if(a.size()!=sizeof...(I))invalid("tuple has wrong native arity");return T{read_stored<std::tuple_element_t<I,T>>(a[I])...};}
template<class T,std::size_t I=0>T read_variant(std::size_t index,const Json& j){if constexpr(I<std::variant_size_v<T>){if(index==I)return T{std::in_place_index<I>,read_stored<std::variant_alternative_t<I,T>>(j)};return read_variant<T,I+1>(index,j);}else invalid("variant index exceeds native alternatives");}
template<class T>T read_stored(const Json& j){
 if constexpr(std::is_same_v<T,std::nullptr_t>){if(!j.null())invalid("native null requires null");return nullptr;}
 else if constexpr(std::is_same_v<T,bool>)return j.boolean();
 else if constexpr(std::is_integral_v<T>&&std::is_unsigned_v<T>)return native_unsigned<T>(j);
 else if constexpr(std::is_integral_v<T>){const auto* n=std::get_if<Number>(&j.value);if(!n)invalid("expected exact signed integer token");T value{};const auto r=std::from_chars(n->text.data(),n->text.data()+n->text.size(),value);if(r.ec!=std::errc{}||r.ptr!=n->text.data()+n->text.size())invalid("signed integer exceeds native storage");return value;}
 else if constexpr(std::is_same_v<T,double>)return ieee_double(j);
 else if constexpr(std::is_same_v<T,std::string>)return j.string();
 else if constexpr(std::is_enum_v<T>){if(const auto* o=std::get_if<Json::Object>(&j.value)){allowed_fields(*o,{"name","index"});if(const auto* name=field(*o,"name");name&&!name->null())(void)name->string();return static_cast<T>(native_int(required(*o,"index")));}return static_cast<T>(native_int(j));}
 else if constexpr(Optional<T>::value){if(j.null())return std::nullopt;return T{read_stored<typename T::value_type>(j)};}
 else if constexpr(Vector<T>::value){T result;for(const auto& x:j.array())result.push_back(read_stored<typename T::value_type>(x));return result;}
 else if constexpr(Set<T>::value){T result;for(const auto& x:j.array())result.insert(read_stored<typename T::value_type>(x));return result;}
 else if constexpr(Map<T>::value){T result;for(const auto& x:j.array()){const auto& o=x.object();allowed_fields(o,{"key","value"});if(o.size()!=2)invalid("native map entry requires key and value");if(!result.emplace(read_stored<typename T::key_type>(required(o,"key")),read_stored<typename T::mapped_type>(required(o,"value"))).second)invalid("duplicate decoded native map key");}return result;}
 else if constexpr(Pair<T>::value||Tuple<T>::value)return read_tuple<T>(j,std::make_index_sequence<std::tuple_size_v<T>>{});
 else if constexpr(Variant<T>::value){const auto& o=j.object();allowed_fields(o,{"index","value"});if(o.size()!=2)invalid("native variant requires index and value");return read_variant<T>(native_unsigned<std::size_t>(required(o,"index")),required(o,"value"));}
 else if constexpr(std::is_same_v<T,HeuristicStructuredValue>)return HeuristicStructuredValue{read_stored<HeuristicStructuredValue::Value>(j)};
 else{T result;const auto& o=j.object();std::set<std::string> names;std::apply([&](const auto&...m){([&]{names.emplace(m.name);using U=std::remove_cvref_t<decltype(result.*(m.pointer))>;result.*(m.pointer)=read_stored<U>(required(o,m.name));}(),...);},Record<T>::members());for(const auto& [key,x]:o){(void)x;if(!names.contains(key))invalid("unknown native record field "+key);}return result;}
}
HeuristicPerformanceValue legacy_performance(const Json& j);
HeuristicStructuredValue legacy_structured(const Json& j){const auto& o=j.object();allowed_fields(o,{"type","value"});const auto type=required(o,"type").string();const auto& v=required(o,"value");if(type=="null")return {read_stored<std::nullptr_t>(v)};if(type=="boolean")return {v.boolean()};if(type=="int64")return {read_stored<std::int64_t>(v)};if(type=="uint64")return {unsigned_integer(v)};if(type=="float")return {ieee_double(v)};if(type=="string")return {v.string()};if(type=="array"){HeuristicStructuredValue::Array a;for(const auto& x:v.array())a.push_back(legacy_structured(x));return {std::move(a)};}if(type=="object"){HeuristicStructuredValue::Object a;for(const auto& [k,x]:v.object())a.emplace(k,legacy_structured(x));return {std::move(a)};}invalid("unknown legacy structured type");}
HeuristicPerformanceValue legacy_performance(const Json& j){const auto& o=j.object();allowed_fields(o,{"type","value"});const auto type=required(o,"type").string();const auto& v=required(o,"value");if(type=="scalar")return ieee_double(v);if(type=="samples")return read_stored<std::vector<double>>(v);if(type=="structured")return legacy_structured(v);if(type=="summary")return read_stored<HeuristicPerformanceSummary>(v);invalid("unknown legacy performance type");}
HeuristicClassicDfgInput dfg_value(const Json& j){const auto& o=j.object();
 // Full constructor has all14 fields, including explicit null optionals. Partial
 // legacy objects use their original record-array map spelling below.
 // The complete constructor always spells activity_counts as null or entry
 // array; the legacy full14 object spells it as a string-key count object.
 // No activity label is interpreted as variant metadata.
 if(o.size()==14){const auto* counts=field(o,"activity_counts");if(counts&&(counts->null()||std::holds_alternative<Json::Array>(counts->value)))return read_stored<HeuristicClassicDfgInput>(j);}
 allowed_fields(o,{"dfg","activities","activity_counts","start_counts","end_counts","window_two_dfg","frequency_triples","performance_dfg","start_activities","end_activities","performance_values","dfg_window_2_supplied","freq_triples_supplied","dfg_order"});
 HeuristicClassicDfgInput r;
 const auto pairs=[](const Json& a){HeuristicFrequencyDfg out;for(const auto& x:a.array()){const auto& e=x.object();allowed_fields(e,{"source","target","count"});if(!out.emplace(std::pair{required(e,"source").string(),required(e,"target").string()},unsigned_integer(required(e,"count"))).second)invalid("duplicate legacy DFG path");}return out;};
 const auto counts=[](const Json& a){std::map<std::string,std::uint64_t> out;for(const auto& [k,v]:a.object())out.emplace(k,unsigned_integer(v));return out;};
 const auto boundary=[&](const Json& a)->HeuristicBoundaryActivities{if(std::holds_alternative<Json::Array>(a.value))return read_stored<std::vector<std::string>>(a);return counts(a);};
 r.dfg=pairs(required(o,"dfg"));
 if(const auto* x=field(o,"activities"))r.activities=read_stored<std::vector<std::string>>(*x);
 if(const auto* x=field(o,"activity_counts"))r.activity_counts=counts(*x);
 if(const auto* x=field(o,"start_counts"))r.start_counts=counts(*x);
 if(const auto* x=field(o,"end_counts"))r.end_counts=counts(*x);
 if(const auto* x=field(o,"window_two_dfg"))r.window_two_dfg=pairs(*x);
 if(const auto* x=field(o,"frequency_triples"))for(const auto& v:x->array()){const auto& e=v.object();allowed_fields(e,{"source","via","target","count"});if(!r.frequency_triples.emplace(std::tuple{required(e,"source").string(),required(e,"via").string(),required(e,"target").string()},unsigned_integer(required(e,"count"))).second)invalid("duplicate legacy frequency triple");}
 if(const auto* x=field(o,"performance_dfg")){r.performance_dfg.emplace();for(const auto& v:x->array()){const auto& e=v.object();allowed_fields(e,{"source","target","samples"});if(!r.performance_dfg->emplace(std::pair{required(e,"source").string(),required(e,"target").string()},read_stored<std::vector<double>>(required(e,"samples"))).second)invalid("duplicate legacy performance path");}}
 if(const auto* x=field(o,"performance_values")){r.performance_values.emplace();for(const auto& v:x->array()){const auto& e=v.object();allowed_fields(e,{"source","target","value"});if(!r.performance_values->emplace(std::pair{required(e,"source").string(),required(e,"target").string()},legacy_performance(required(e,"value"))).second)invalid("duplicate legacy decorated path");}}
 if(const auto* x=field(o,"start_activities"))r.start_activities=boundary(*x);
 if(const auto* x=field(o,"end_activities"))r.end_activities=boundary(*x);
 r.dfg_window_2_supplied=field(o,"dfg_window_2_supplied")?required(o,"dfg_window_2_supplied").boolean():field(o,"window_two_dfg")!=nullptr;
 r.freq_triples_supplied=field(o,"freq_triples_supplied")?required(o,"freq_triples_supplied").boolean():field(o,"frequency_triples")!=nullptr;
 if(const auto* x=field(o,"dfg_order")){r.dfg_order.emplace();for(const auto& v:x->array()){const auto& e=v.object();allowed_fields(e,{"source","target"});r.dfg_order->emplace_back(required(e,"source").string(),required(e,"target").string());}}
 return r;
}

template<class T>std::string_view enum_name(T value){const auto index=static_cast<int>(value);
 if constexpr(std::is_same_v<T,HeuristicMinerVariant>){constexpr const char* names[]={"CLASSIC","PLUSPLUS"};return index>=0&&index<2?names[index]:"";}
 else if constexpr(std::is_same_v<T,HeuristicNetDecoration>){constexpr const char* names[]={"FREQUENCY","PERFORMANCE"};return index>=0&&index<2?names[index]:"";}
 else{constexpr const char* names[]={"MEAN","MEDIAN","MINIMUM","MAXIMUM","STANDARD_DEVIATION","SUM","RAW_VALUES","ALL"};return index>=0&&index<8?names[index]:"";}
}
template<class T>void stored(std::ostream& out,const T& value){
 if constexpr(std::is_same_v<T,std::nullptr_t>)out<<"null";
 else if constexpr(std::is_same_v<T,bool>)out<<(value?"true":"false");
 else if constexpr(std::is_integral_v<T>)out<<value;
 else if constexpr(std::is_same_v<T,double>)number(out,value);
 else if constexpr(std::is_same_v<T,std::string>)out<<q(value);
 else if constexpr(std::is_same_v<T,HeuristicOccurrenceCount>)out<<value.to_string();
 else if constexpr(std::is_enum_v<T>)out<<"{\"name\":"<<(enum_name(value).empty()?"null":q(enum_name(value)))<<",\"index\":"<<static_cast<int>(value)<<'}';
 else if constexpr(Optional<T>::value){if(value)stored(out,*value);else out<<"null";}
 else if constexpr(Vector<T>::value||Set<T>::value){out<<'[';bool comma=false;for(const auto& item:value){if(comma)out<<',';comma=true;stored(out,item);}out<<']';}
 else if constexpr(Map<T>::value){out<<'[';bool comma=false;for(const auto& [key,item]:value){if(comma)out<<',';comma=true;out<<"{\"key\":";stored(out,key);out<<",\"value\":";stored(out,item);out<<'}';}out<<']';}
 else if constexpr(Pair<T>::value){out<<'[';stored(out,value.first);out<<',';stored(out,value.second);out<<']';}
 else if constexpr(Tuple<T>::value){out<<'[';bool comma=false;std::apply([&](const auto&...item){([&]{if(comma)out<<',';comma=true;stored(out,item);}(),...);},value);out<<']';}
 else if constexpr(Variant<T>::value){out<<"{\"index\":"<<value.index()<<",\"value\":";std::visit([&](const auto& item){stored(out,item);},value);out<<'}';}
 else if constexpr(std::is_same_v<T,HeuristicStructuredValue>)stored(out,value.value);
 else{out<<'{';bool comma=false;std::apply([&](const auto&...item){([&]{if(comma)out<<',';comma=true;out<<q(item.name)<<':';stored(out,value.*(item.pointer));}(),...);},Record<T>::members());out<<'}';}
}
template<class T>std::string render(const T& value){auto out=output();stored(out,value);return out.str();}
thread_local ExactFilterDiscoveryContext* current_context=nullptr;
const char* ltl_legacy[]={"ltl_variant","ltl_attribute_key","ltl_attribute_values","ltl_positive","ltl_enable_timestamp","ltl_timestamp_diff_boundaries","ltl_resource_key"};
// A body that is one scalar UTF8 JSON object becomes the envelope itself, so
// its legacy snapshot is written as {"body":null,"json":null}: the body is the
// envelope text without its final "exact_controls" member. Any other body is
// kept as its own string. `empty_object` reports a body of "{}" shape.
std::string legacy_response_json(int status,std::string_view body,std::string* json_error=nullptr,bool* empty_object=nullptr){std::string error;bool empty=false;
 try{if(!q(body).starts_with('"'))invalid("legacy body is not scalar UTF8");const auto value=Parser(body).parse();empty=value.object().empty();}
 catch(const std::exception& e){error=e.what();}
 if(json_error)*json_error=error;
 if(empty_object)*empty_object=empty;
 if(error.empty())return "{\"status\":"+std::to_string(status)+",\"body\":null,\"json\":null,\"json_error\":null}";
 return "{\"status\":"+std::to_string(status)+",\"body\":"+q(body)+",\"json\":null,\"json_error\":"+q(error)+'}';
}
// A source snapshot equal to an earlier one is written as a reference to it,
// {"same_as":"<selected_native member>"}, instead of a second copy.
std::string source_reference(const std::string& value,std::initializer_list<std::pair<const char*,const std::string*>> earlier){
 if(value!="null")for(const auto& [name,other]:earlier)if(value==*other)return "{\"same_as\":\""+std::string(name)+"\"}";
 return value;
}
} // namespace

bool exact_filter_discovery_control(std::string_view name){return name=="ltl_attribute_values_json"||name=="ltl_attribute_values_file"||name=="ltl_options_json"||name=="ltl_options_file"||name=="heuristic_most_common_paths_json"||name=="heuristic_most_common_paths_file"||name=="heuristic_options_json"||name=="heuristic_options_file";}
bool exact_ltl_requested(const FieldPresence& p){return p&&(p("ltl_attribute_values_json")||p("ltl_attribute_values_file")||p("ltl_options_json")||p("ltl_options_file"));}
bool exact_heuristic_pairs_requested(const FieldPresence& p){return p&&(p("heuristic_most_common_paths_json")||p("heuristic_most_common_paths_file"));}
bool exact_heuristic_options_requested(const FieldPresence& p){return p&&(p("heuristic_options_json")||p("heuristic_options_file"));}
HeuristicMinerOptions parse_exact_heuristic_options_json(std::string_view input){return read_stored<HeuristicMinerOptions>(Parser(input).parse());}
HeuristicClassicDfgInput parse_exact_heuristic_dfg_json(std::string_view input){return dfg_value(Parser(input).parse());}
std::string serialize_exact_heuristic_dfg_json(const HeuristicClassicDfgInput& input){return render(input);}
std::vector<std::string> parse_exact_ltl_values_json(std::string_view input){return vector_value<std::string>(Parser(input).parse(),[](const Json& x){return x.string();});}
LtlFilterOptions parse_exact_ltl_options_json(std::string_view input){const auto value=Parser(input).parse();const auto& o=value.object();allowed_fields(o,{"variant_index","attribute_key","attribute_values","positive","enable_timestamp","timestamp_diff_boundaries","resource_key"});if(o.size()!=7)invalid("LTL options require all seven native fields");LtlFilterOptions result;result.variant=static_cast<LtlFilterVariant>(native_int(required(o,"variant_index")));result.attribute_key=required(o,"attribute_key").string();result.attribute_values=vector_value<std::string>(required(o,"attribute_values"),[](const Json& x){return x.string();});result.positive=required(o,"positive").boolean();result.enable_timestamp=required(o,"enable_timestamp").boolean();result.resource_key=required(o,"resource_key").string();for(const auto& entry:required(o,"timestamp_diff_boundaries").array()){const auto& a=entry.array();if(a.size()!=2)invalid("LTL timestamp boundary requires exactly two doubles");result.timestamp_diff_boundaries.emplace_back(ieee_double(a[0]),ieee_double(a[1]));}return result;}
std::set<std::pair<std::string,std::string>> parse_exact_heuristic_pairs_json(std::string_view input){const auto document=Parser(input).parse();std::set<std::pair<std::string,std::string>> result;for(const auto& entry:document.array()){const auto& a=entry.array();if(a.size()!=2)invalid("heuristic protected path requires exactly two NativeStrings");result.emplace(a[0].string(),a[1].string());}return result;}
std::string serialize_exact_ltl_options_json(const LtlFilterOptions& o){return "{\"variant_index\":"+std::to_string(static_cast<int>(o.variant))+",\"attribute_key\":"+q(o.attribute_key)+",\"attribute_values\":"+render(o.attribute_values)+",\"positive\":"+(o.positive?"true":"false")+",\"enable_timestamp\":"+(o.enable_timestamp?"true":"false")+",\"timestamp_diff_boundaries\":"+render(o.timestamp_diff_boundaries)+",\"resource_key\":"+q(o.resource_key)+'}';}
std::string serialize_exact_heuristic_options_json(const HeuristicMinerOptions& o){return render(o);}
std::string serialize_exact_heuristics_net_json(const HeuristicsNet& net){return render(net);}
EventLog parse_exact_ltl_log_json(std::string_view input){return log_value(Parser(input).parse());}
ExactFilterDiscoveryScope::ExactFilterDiscoveryScope(ExactFilterDiscoveryContext& context):previous_(current_context){current_context=&context;}
ExactFilterDiscoveryScope::~ExactFilterDiscoveryScope(){current_context=previous_;}
void record_exact_parsed_options(const MineParams& p){if(!current_context)return;auto& c=*current_context;if(p.ltl){c.parsed_ltl=serialize_exact_ltl_options_json(*p.ltl);c.configured_ltl=c.parsed_ltl;c.ltl_stage="effective";}if(c.parsed_heuristic=="null")c.parsed_heuristic=render(p.heuristic);if(c.heuristic_stage!="effective")c.heuristic_stage="parsed";c.pairset=render(p.heuristic.most_common_paths);}
void record_exact_effective_options(const MineParams& p){if(!current_context)return;record_exact_parsed_options(p);current_context->configured_heuristic=render(p.heuristic);current_context->heuristic_stage="effective";}
void record_exact_heuristic_options(const HeuristicMinerOptions& p,bool effective){if(!current_context)return;auto& c=*current_context;const auto actual=render(p);if(c.parsed_heuristic=="null")c.parsed_heuristic=actual;c.pairset=render(p.most_common_paths);c.heuristic_stage=effective?"effective":"parsed";if(effective)c.configured_heuristic=actual;}
void record_exact_source(const EventLog& log,bool after){if(!current_context)return;auto& c=*current_context;try{(after?c.source_after:c.source_before)="{\"type\":\"EventLog\",\"value\":"+serialize_embedding_workbench_raw_log_json(log)+'}';}catch(const std::exception& e){(after?c.source_after_error:c.source_before_error)=e.what();}}
void record_exact_source(const EventTable& table,bool after){if(!current_context)return;auto& c=*current_context;try{(after?c.source_after:c.source_before)="{\"type\":\"EventTable\",\"value\":"+serialize_embedding_workbench_table_json(table)+'}';}catch(const std::exception& e){(after?c.source_after_error:c.source_before_error)=e.what();}}
void record_exact_heuristics_net(const HeuristicsNet& net){if(!current_context)return;try{current_context->heuristics_net=render(net);}catch(const std::exception& e){current_context->heuristics_net_error=e.what();}}
void record_exact_source(const HeuristicClassicDfgInput& input,bool after){if(!current_context)return;auto& c=*current_context;(after?c.source_after:c.source_before)="{\"type\":\"HeuristicClassicDfgInput\",\"value\":"+render(input)+'}';}
bool exact_filter_discovery_recording(){return current_context!=nullptr;}
std::string exact_source_snapshot(const HeuristicClassicDfgInput& input){return "{\"type\":\"HeuristicClassicDfgInput\",\"value\":"+render(input)+'}';}
std::string exact_source_snapshot(const EventDataView& input){
 if(input.log())return "{\"type\":\"EventLog\",\"value\":"+serialize_embedding_workbench_raw_log_json(*input.log())+'}';
 if(input.stream())return "{\"type\":\"EventStream\",\"value\":"+serialize_embedding_workbench_stream_json(*input.stream())+'}';
 if(input.table())return "{\"type\":\"EventTable\",\"value\":"+serialize_embedding_workbench_table_json(*input.table())+'}';
 if(input.lazy_table())return "{\"type\":\"LazyEventTable\",\"value\":"+serialize_embedding_workbench_table_json(input.lazy_table()->materialize())+'}';
 throw std::invalid_argument("unknown native input view");
}
void record_exact_heuristic_begin(std::string source,const HeuristicMinerOptions& options){if(!current_context)return;auto& c=*current_context;c.heuristic_entered=true;c.heuristic_returned=false;c.call_source_before=source;c.source_before=std::move(source);c.call_options_before=render(options);c.configured_heuristic=c.call_options_before;c.heuristic_stage="effective";}
void record_exact_heuristic_end(std::string source,const HeuristicMinerOptions& options,const HeuristicsNet* result,std::string_view error_type,std::string_view error){if(!current_context)return;auto& c=*current_context;c.call_source_after=source;c.source_after=std::move(source);c.call_options_after=render(options);if(result){c.heuristic_returned=true;record_exact_heuristics_net(*result);}c.call_error_type=error_type;c.call_error=error;}
void record_exact_snapshot_failure(std::string_view error,bool after){if(current_context)(after?current_context->source_after_error:current_context->source_before_error)=error;}
void record_exact_optional_artifact(std::string_view name,std::string_view bytes,bool error){if(!current_context)return;current_context->optional_artifacts[std::string(name)+(error?"_error":"")]=std::string(bytes);}
void record_exact_petri_net(const PetriNet& net){if(!current_context)return;try{current_context->petri_net=serialize_oc_playout_workbench_petri_net_json(net);}catch(const std::exception& e){current_context->petri_net_error=e.what();}}
std::string exact_filter_discovery_response(const ExactFilterDiscoveryContext& c,std::string_view operation,int status,std::string_view body){const auto nullable=[](const std::string& x){return x.empty()?std::string("null"):q(x);};std::string json_error;bool empty_body=false;const auto legacy=legacy_response_json(status,body,&json_error,&empty_body);
 const auto call=!c.heuristic_entered?std::string("null"):"{\"entered\":true,\"returned\":"+std::string(c.heuristic_returned?"true":"false")+",\"source_type\":"+(c.call_source_before=="null"?std::string("null"):q(c.call_source_before.substr(9,c.call_source_before.find('"',9)-9)))+",\"source_before\":"+source_reference(c.call_source_before,{{"source_before",&c.source_before},{"source_after",&c.source_after}})+",\"source_after\":"+source_reference(c.call_source_after,{{"source_after",&c.source_after},{"source_before",&c.source_before}})+",\"options_before\":"+c.call_options_before+",\"options_after\":"+c.call_options_after+",\"result\":"+(c.heuristic_returned?"{\"type\":\"HeuristicsNet\",\"value\":"+c.heuristics_net+'}':"null")+",\"error\":"+(c.call_error.empty()?"null":"{\"type\":"+q(c.call_error_type)+",\"message\":"+q(c.call_error)+'}')+",\"source_unchanged\":"+(c.call_source_after=="null"?"null":c.call_source_after==c.call_source_before?"true":"false")+",\"options_unchanged\":"+(c.call_options_after=="null"?"null":c.call_options_after==c.call_options_before?"true":"false")+'}';
 auto artifacts=output();artifacts<<'{';bool comma=false;for(const auto& [name,value]:c.optional_artifacts){if(comma)artifacts<<',';comma=true;artifacts<<q(name)<<':'<<q(value);}artifacts<<'}';
 const auto metadata="{\"version\":\"exact-filter-discovery-v1\",\"input_order\":"+q(c.input_order)+",\"original_inputs\":"+parts_json(c.original_inputs)+",\"query_inputs\":"+parts_json(c.query_inputs)+",\"parsed_options\":{\"ltl\":"+c.parsed_ltl+",\"heuristic\":"+c.parsed_heuristic+"},\"configured_options\":{\"ltl\":"+c.configured_ltl+",\"heuristic\":"+c.configured_heuristic+"},\"configuration_stage\":{\"ltl\":"+q(c.ltl_stage)+",\"heuristic\":"+q(c.heuristic_stage)+"},\"canonical_most_common_paths\":"+c.pairset+",\"selected_native\":{\"source_before\":"+c.source_before+",\"source_after\":"+source_reference(c.source_after,{{"source_before",&c.source_before}})+",\"heuristics_net\":"+c.heuristics_net+",\"petri_net\":"+c.petri_net+",\"heuristic_call\":"+call+"},\"selected_native_errors\":{\"source_before\":"+nullable(c.source_before_error)+",\"source_after\":"+nullable(c.source_after_error)+",\"heuristics_net\":"+nullable(c.heuristics_net_error)+",\"petri_net\":"+nullable(c.petri_net_error)+"},\"native_observation\":"+(c.heuristic_entered?"{\"heuristic\":{\"entered\":true,\"returned\":"+std::string(c.heuristic_returned?"true":"false")+"}}":"null")+",\"optional_artifacts\":"+artifacts.str()+",\"legacy_response\":"+legacy+'}';
 if(json_error.empty()){const auto end=body.find_last_not_of(" \t\r\n");std::string envelope;envelope.reserve(body.size()+metadata.size()+20);envelope.append(body.substr(0,end));if(!empty_body)envelope+=',';envelope+="\"exact_controls\":";envelope+=metadata;envelope.append(body.substr(end));return envelope;}
 return "{\"family\":\"EXACT_FILTER_DISCOVERY\",\"operation\":"+q(operation)+",\"legacy_response\":"+legacy+",\"exact_controls\":"+metadata+'}';
}


namespace {
EventDataKind imported_kind(std::string_view name){if(name.empty()||name=="log")return EventDataKind::kEventLog;if(name=="stream")return EventDataKind::kEventStream;if(name=="dataframe")return EventDataKind::kEventTable;if(name=="lazyframe")return EventDataKind::kLazyEventTable;invalid("mining_input_shape must be log,stream,dataframe,lazyframe");}
EventLog imported_ltl_log(std::string_view bytes,const std::string& filename,const FieldLookup& field,const FieldPresence& supplied,std::string& detected){auto p=parse_mine_params(field,supplied);ActiveSourceSelectors selectors;const auto literal=supplied("mining_selectors_json")?parse_workflow_selector_strings_json(field("mining_selectors_json")):std::map<std::string,std::string>{};const auto format=field("mining_input_format");if(!format.empty()&&format!="auto"&&format!="table")invalid("mining_input_format must be auto or table");const auto kind=imported_kind(field("mining_input_shape"));
 const auto override_selectors=[&]{for(const auto& [key,value]:literal){if(key=="case_id_key")selectors.case_id_key=value;else if(key=="activity_key")selectors.activity_key=value;else if(key=="timestamp_key")selectors.timestamp_key=value;else if(key=="resource_key")selectors.resource_key=value;else if(key=="lifecycle_key")selectors.lifecycle_key=value;else invalid("unknown mining source selector "+key);}};
 if(format=="table"){detected="table";override_selectors();auto source=ActiveEventSource::from_table(parse_embedding_workbench_table_json(bytes),{filename,"table",std::string(bytes)},selectors,kind);return source.checked_log();}
 std::string lower=filename;for(auto& c:lower)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));const auto start=bytes.find_first_not_of(" \r\n\t");const bool xes=lower.ends_with(".xes")||lower.ends_with(".xes.gz")||(start!=std::string_view::npos&&bytes[start]=='<');std::istringstream input{std::string(bytes)};
 if(xes){detected="xes";selectors={"case:"+p.xes.case_attribute,p.xes.activity_attribute,p.xes.timestamp_attribute,p.xes.resource_attribute,p.xes.lifecycle_attribute};override_selectors();p.xes.case_attribute=selectors.case_id_key.starts_with("case:")?selectors.case_id_key.substr(5):selectors.case_id_key;p.xes.activity_attribute=selectors.activity_key;p.xes.timestamp_attribute=selectors.timestamp_key;p.xes.resource_attribute=selectors.resource_key;p.xes.lifecycle_attribute=selectors.lifecycle_key;auto source=ActiveEventSource::from_xes_frame(import_xes_event_log(input,p.xes),{filename,"xes",std::string(bytes)},selectors,kind);return source.checked_log();}
 detected="csv";selectors={p.csv.case_id_column,p.csv.activity_column,p.csv.timestamp_column,p.csv.resource_column,p.csv.lifecycle_column};override_selectors();p.csv.case_id_column=selectors.case_id_key;p.csv.activity_column=selectors.activity_key;p.csv.timestamp_column=selectors.timestamp_key;p.csv.resource_column=selectors.resource_key;p.csv.lifecycle_column=selectors.lifecycle_key;auto source=ActiveEventSource::from_csv(import_csv_event_data(input,p.csv),{filename,"csv",std::string(bytes)},selectors,kind);return source.checked_log();
}
}
DiscoveryWorkbenchResponse execute_exact_ltl_filter(const std::vector<DiscoveryWorkbenchPart>& parts,const std::vector<DiscoveryWorkbenchPart>& query,std::string_view input_order,std::string_view transport_error){
 std::vector<DiscoveryWorkbenchPart> all=query;all.insert(all.end(),parts.begin(),parts.end());const auto first=[&](std::string_view key)->const DiscoveryWorkbenchPart*{for(const auto& p:all)if(p.name==key)return &p;return nullptr;};const auto original=first("source_file");const std::string original_source=original?original->bytes:"",filename=original?original->filename:"";std::string format=first("source_format")?first("source_format")->bytes:"native_log",detected=format;
 std::map<std::string,const DiscoveryWorkbenchPart*> fields;const auto supplied=[&](std::string_view key){return fields.contains(std::string(key));};const auto field_value=[&](std::string_view key){const auto found=fields.find(std::string(key));return found==fields.end()?std::string{}:found->second->bytes;};
 std::optional<EventLog> source,result;std::optional<LtlFilterOptions> options;std::string before="null",after="null",options_before="null",options_after="null",error,phase="configuration_error",type;bool entered=false,returned=false;std::map<std::string,std::string> artifacts,artifact_errors;
 try{
  if(!transport_error.empty()){phase="transport_error";throw std::invalid_argument(std::string(transport_error));}
  static const std::set<std::string> imports={"mining_input_format","mining_input_shape","mining_selectors_json","case_column","activity_column","timestamp_column","resource_column","lifecycle_column","delimiter","xes_variant","xes_case_attribute","xes_activity_attribute","xes_timestamp_attribute","xes_resource_attribute","xes_lifecycle_attribute","xes_require_timestamp","xes_timestamp_sort","xes_reverse_sort","xes_max_traces","xes_attributes_to_read","xes_max_decompressed_bytes"};
  for(const auto& p:all){const bool ltl=p.name=="ltl_options_json"||p.name=="ltl_options_file"||p.name=="ltl_attribute_values_json"||p.name=="ltl_attribute_values_file"||std::find(std::begin(ltl_legacy),std::end(ltl_legacy),p.name)!=std::end(ltl_legacy);if(p.name!="operation"&&p.name!="source_file"&&p.name!="source_format"&&!ltl&&!imports.contains(p.name))invalid("unknown original LTL control: "+p.name);if(!fields.emplace(p.name,&p).second)invalid("duplicate original LTL control: "+p.name);if(p.name.ends_with("_file")&&!p.file)invalid(p.name+" requires an actual File");}
  if(!supplied("source_file"))invalid("original LTL requires source_file");if(supplied("operation")&&field_value("operation")!="filter_ltl")invalid("operation must be filter_ltl");if(format!="native_log"&&format!="import")invalid("source_format must be native_log or import");for(const auto& key:imports)if(format=="native_log"&&supplied(key))invalid("source import control is inapplicable: "+key);
  const auto parsed=parse_mine_params(field_value,supplied);options=parsed.ltl.value_or(LtlFilterOptions{});options_before=serialize_exact_ltl_options_json(*options);options_after=options_before;
  phase="source_parse_error";source=format=="native_log"?parse_exact_ltl_log_json(original_source):imported_ltl_log(original_source,filename,field_value,supplied,detected);before=serialize_embedding_workbench_raw_log_json(*source);after=before;
  phase="native_error";entered=true;
  // A native log JSON arrives as an EventLog object and follows pm4py's EventLog
  // ltl_checker; an imported CSV/XES file follows its DataFrame checker.
  result=filter_ltl_event_log(*source,*options,format=="native_log"?LtlInputShape::kEventLogObject:LtlInputShape::kImportedTable);returned=true;
 }catch(const std::exception& e){error=e.what();type=dynamic_cast<const std::invalid_argument*>(&e)?"invalid_argument":dynamic_cast<const std::out_of_range*>(&e)?"out_of_range":dynamic_cast<const std::overflow_error*>(&e)?"overflow_error":"native_error";}
 if(source)after=serialize_embedding_workbench_raw_log_json(*source);if(options)options_after=serialize_exact_ltl_options_json(*options);
 const auto artifact=[&](const std::string& name,const auto& produce){try{artifacts[name]=produce();}catch(const std::exception& e){artifact_errors[name]=e.what();}};
 if(source){artifacts["source_log_json"]=before;artifact("source_xes",[&]{XesExportOptions o;o.preserve_nulls=true;return serialize_xes(*source,o);});}
 std::string result_json="null";if(result){result_json=serialize_embedding_workbench_raw_log_json(*result);artifacts["result_log_json"]=result_json;artifact("legacy_result_json",[&]{return serialize_footprints_workbench_raw_log_json(*result);});artifact("result_xes",[&]{XesExportOptions o;o.preserve_nulls=true;return serialize_xes(*result,o);});}
 const auto text=[&](const auto& map,const std::string& key){const auto i=map.find(key);return i==map.end()?std::string("null"):q(i->second);};auto out=output();out<<"{\"family\":\"ORIGINAL_LTL_FILTER\",\"operation\":\"filter_ltl\",\"native_entered\":"<<(entered?"true":"false")<<",\"native_returned\":"<<(returned?"true":"false")<<",\"executed\":"<<(returned?"true":"false")<<",\"error\":"<<(error.empty()?"null":"{\"phase\":"+q(phase)+",\"type\":"+q(type)+",\"message\":"+q(error)+'}')<<",\"input_context\":{\"source_format\":"<<q(format)<<",\"detected_format\":"<<q(detected)<<",\"filename\":"<<q(filename)<<",\"original_input_bytes\":"<<original_source.size()<<",\"input_order\":"<<q(input_order)<<"},\"original_source\":"<<q(original_source)<<",\"configured_options\":"<<options_before<<",\"snapshots\":{\"source_before\":"<<before<<",\"source_after\":"<<after<<",\"options_before\":"<<options_before<<",\"options_after\":"<<options_after<<"},\"source_unchanged\":"<<(source?(before==after?"true":"false"):"null")<<",\"options_unchanged\":"<<(options?(options_before==options_after?"true":"false"):"null")<<",\"result\":"<<(result?"{\"type\":\"EventLog\",\"value\":"+result_json+'}':"null")<<",\"original_inputs\":"<<parts_json(parts)<<",\"query_inputs\":"<<parts_json(query)<<",\"artifacts\":{";bool comma=false;for(const auto* name:{"source_log_json","result_log_json","legacy_result_json","source_xes","result_xes"}){if(comma)out<<',';comma=true;out<<q(name)<<':'<<text(artifacts,name)<<','<<q(std::string(name)+"_error")<<':'<<text(artifact_errors,name);}out<<"}}";return {error.empty()?200:400,out.str()};
}
} // namespace minera
