#include "minera/dataframe_format_json.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <iomanip>
#include <limits>
#include <locale>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "minera/dataframe_format.hpp"
#include "minera/event_table_json.hpp"
#include "minera/workflow_discovery_input_json.hpp"

namespace minera {
namespace {

[[noreturn]] void invalid(std::string message) {
  throw std::invalid_argument("dataframe JSON: " + message);
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
  explicit Parser(std::string_view text, bool escaped_lone_surrogates = false,
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

const Json* optional(const Json::Object& object, std::string_view key) {
  const auto found = object.find(key);
  return found == object.end() ? nullptr : &found->second;
}
const Json& required(const Json::Object& object, std::string_view key) {
  if (const auto* value = optional(object, key)) return *value;
  invalid("missing field " + std::string(key));
}
void fields(const Json::Object& object, std::initializer_list<std::string_view> allowed) {
  for (const auto& [key, value] : object) {
    (void)value;
    if (std::find(allowed.begin(), allowed.end(), key) == allowed.end()) invalid("unknown field " + key);
  }
}
std::string text(const Json::Object& object, std::string_view key, std::string fallback = {}) {
  const auto* found = optional(object, key);
  return found ? found->string() : std::move(fallback);
}
bool flag(const Json::Object& object, std::string_view key) {
  const auto* found = optional(object, key);
  return found && found->boolean();
}

template <class T> T number(std::string_view value) {
  T result{};
  const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
  if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) invalid("invalid or out-of-range numeric cell");
  return result;
}
std::string number_text(const Json& value) {
  if (const auto* token = std::get_if<Number>(&value.value)) return token->text;
  return value.string();
}

EventAttribute attribute(const Json& value, std::string default_key = {},
                         bool exact_integer_storage = false);
std::vector<EventAttribute> attributes(const Json& value,
                                       bool exact_integer_storage = false) {
  std::vector<EventAttribute> result;
  for (const auto& item : value.array())
    result.push_back(attribute(item, {}, exact_integer_storage));
  return result;
}

EventAttribute attribute(const Json& value, std::string default_key,
                         bool exact_integer_storage) {
  const auto& object = value.object();
  if (exact_integer_storage)
    fields(object, {"key", "value", "kind", "container", "null", "typed_value", "integer_storage", "date_resolution", "date_timezone", "children", "scalar_wrapper", "scalar_children_list", "first_class_role", "first_class_state", "first_class_source_key"});
  else
    fields(object, {"key", "value", "kind", "container", "null", "typed_value", "date_resolution", "date_timezone", "children", "scalar_wrapper", "scalar_children_list", "first_class_role", "first_class_state", "first_class_source_key"});
  EventAttribute result{text(object, "key", std::move(default_key)), text(object, "value"), AttributeKind::kString};
  const auto kind = text(object, "kind", "string");
  if (kind == "string") result.kind = AttributeKind::kString;
  else if (kind == "int") result.kind = AttributeKind::kInt;
  else if (kind == "float") result.kind = AttributeKind::kFloat;
  else if (kind == "boolean") result.kind = AttributeKind::kBoolean;
  else if (kind == "date") result.kind = AttributeKind::kDate;
  else if (kind == "id") result.kind = AttributeKind::kId;
  else if (kind == "bytes") result.kind = AttributeKind::kBytes;
  else if (kind == "na") result.kind = AttributeKind::kNA;
  else invalid("unknown attribute kind " + kind);
  const auto container = text(object, "container", "scalar");
  if (container == "scalar") result.container = AttributeContainerKind::kScalar;
  else if (container == "list") result.container = AttributeContainerKind::kList;
  else if (container == "container") result.container = AttributeContainerKind::kContainer;
  else if (container == "tuple") result.container = AttributeContainerKind::kTuple;
  else if (container == "set") result.container = AttributeContainerKind::kSet;
  else if (container == "frozenset") result.container = AttributeContainerKind::kFrozenSet;
  else invalid("unknown attribute container " + container);
  result.is_null = flag(object, "null");
  result.scalar_wrapper = flag(object, "scalar_wrapper");
  result.scalar_children_list = flag(object, "scalar_children_list");
  if (const auto* children = optional(object, "children"))
    result.children = attributes(*children, exact_integer_storage);
  const auto* integer_storage = exact_integer_storage ? optional(object, "integer_storage") : nullptr;
  if (integer_storage && (result.kind != AttributeKind::kInt || result.is_null ||
      !optional(object, "typed_value") || optional(object, "typed_value")->null()))
    invalid("integer_storage requires a non-null integer typed_value");
  if (result.kind == AttributeKind::kNA) {
    if (result.is_null) invalid("native NA cannot be null");
    if (result.container != AttributeContainerKind::kScalar ||
        result.scalar_wrapper || result.scalar_children_list ||
        optional(object, "children") != nullptr || !result.children.empty())
      invalid("native NA must be a scalar without children");
    if (const auto* lexical = optional(object, "value")) {
      if (lexical->string() != "<NA>")
        invalid("native NA value must be <NA>");
    }
    if (const auto* typed = optional(object, "typed_value")) {
      if (typed->string() != "NA")
        invalid("native NA typed_value must be NA");
    }
    result.value = "<NA>";
    result.typed_value = NativeNA{};
  } else if (const auto* typed = optional(object, "typed_value")) {
    if (result.is_null && !typed->null()) invalid("null cell cannot have a typed value");
    if (!typed->null()) {
      if (result.kind == AttributeKind::kInt) {
        const auto token = number_text(*typed);
        if (integer_storage) {
          const auto storage = integer_storage->string();
          if (storage == "int64") result.typed_value = number<std::int64_t>(token);
          else if (storage == "uint64") result.typed_value = number<std::uint64_t>(token);
          else invalid("unknown integer_storage");
        } else if (token.starts_with('-')) result.typed_value = number<std::int64_t>(token);
        else result.typed_value = number<std::uint64_t>(token);
      } else if (result.kind == AttributeKind::kFloat) {
        const auto token = number_text(*typed);
        if (token == "NaN") result.typed_value = std::numeric_limits<double>::quiet_NaN();
        else if (token == "INF") result.typed_value = std::numeric_limits<double>::infinity();
        else if (token == "-INF") result.typed_value = -std::numeric_limits<double>::infinity();
        else {
          const auto parsed = number<double>(token);
          if (!std::isfinite(parsed)) invalid("non-finite numbers require NaN/INF/-INF tags");
          result.typed_value = parsed;
        }
      } else if (result.kind == AttributeKind::kBoolean) result.typed_value = typed->boolean();
      else if (result.kind == AttributeKind::kDate) result.typed_value = NativeDate{typed->string()};
      else invalid("typed value does not match attribute kind");
      if (!optional(object, "value")) {
        result.value = result.kind == AttributeKind::kBoolean
                           ? (typed->boolean() ? "true" : "false")
                           : number_text(*typed);
      }
      if (result.kind == AttributeKind::kInt) {
        if (const auto* signed_value = std::get_if<std::int64_t>(&result.typed_value)) {
          if (number<std::int64_t>(result.value) != *signed_value) invalid("integer value disagrees with typed_value");
        } else if (number<std::uint64_t>(result.value) != std::get<std::uint64_t>(result.typed_value)) invalid("integer value disagrees with typed_value");
      } else if (result.kind == AttributeKind::kFloat) {
        const auto lexical = number<double>(result.value);
        const auto scalar = std::get<double>(result.typed_value);
        if (!(lexical == scalar || (std::isnan(lexical) && std::isnan(scalar)))) invalid("float value disagrees with typed_value");
      } else if (result.kind == AttributeKind::kBoolean) {
        const bool scalar = std::get<bool>(result.typed_value);
        if (scalar ? result.value != "true" && result.value != "True" && result.value != "1" : result.value != "false" && result.value != "False" && result.value != "0") invalid("boolean value disagrees with typed_value");
      } else if (result.kind == AttributeKind::kDate && result.value != std::get<NativeDate>(result.typed_value).value) invalid("date value disagrees with typed_value");
    }
  }
  if (const auto* resolution = optional(object, "date_resolution")) {
    if (result.is_null || result.kind != AttributeKind::kDate ||
        !std::holds_alternative<NativeDate>(result.typed_value))
      invalid("date_resolution requires a non-null date typed_value");
    const auto value = resolution->string();
    if (value != "us" && value != "ns") invalid("invalid date_resolution");
    std::get<NativeDate>(result.typed_value).resolution =
        value == "ns" ? NativeDateResolution::kNanosecond
                       : NativeDateResolution::kMicrosecond;
  }
  if (const auto* timezone = optional(object, "date_timezone")) {
    if (result.is_null || result.kind != AttributeKind::kDate ||
        !std::holds_alternative<NativeDate>(result.typed_value))
      invalid("date_timezone requires a non-null date typed_value");
    const auto value = timezone->string();
    if (value.empty()) invalid("date_timezone must not be empty");
    std::get<NativeDate>(result.typed_value).timezone = value;
  }
  const auto role = text(object, "first_class_role", "none");
  if (role == "none") result.first_class_role = FirstClassAttributeRole::kNone;
  else if (role == "case_id") result.first_class_role = FirstClassAttributeRole::kCaseId;
  else if (role == "activity") result.first_class_role = FirstClassAttributeRole::kActivity;
  else if (role == "timestamp") result.first_class_role = FirstClassAttributeRole::kTimestamp;
  else if (role == "resource") result.first_class_role = FirstClassAttributeRole::kResource;
  else if (role == "lifecycle") result.first_class_role = FirstClassAttributeRole::kLifecycle;
  else invalid("unknown first-class role");
  const auto state = text(object, "first_class_state", "missing");
  if (state == "missing") result.first_class_state = FirstClassAttributeState::kMissing;
  else if (state == "present") result.first_class_state = FirstClassAttributeState::kPresent;
  else if (state == "null") result.first_class_state = FirstClassAttributeState::kNull;
  else invalid("unknown first-class state");
  result.first_class_source_key = text(object, "first_class_source_key");
  if (result.kind == AttributeKind::kNA &&
      result.first_class_state == FirstClassAttributeState::kNull)
    invalid("native NA cannot have null first-class state");
  if (role == "none" && (state != "missing" || !result.first_class_source_key.empty())) invalid("first-class state/source requires a role");
  return result;
}

std::vector<std::string> strings(const Json& value) {
  std::vector<std::string> result;
  for (const auto& item : value.array()) result.push_back(item.string());
  return result;
}
EventTableIndexLabel index_label(const Json& value, bool exact_integer_storage) {
  const auto& object = value.object();
  const auto kind = required(object, "kind").string();
  if (kind == "scalar") {
    fields(object, {"kind", "value"});
    return {attribute(required(object, "value"), {}, exact_integer_storage)};
  }
  if (kind != "tuple") invalid("unknown index label kind");
  fields(object, {"kind", "items"});
  std::vector<EventTableIndexLabel> result;
  for (const auto& item : required(object, "items").array())
    result.push_back(index_label(item, exact_integer_storage));
  return {std::move(result)};
}

EventDataMetadata metadata(const Json& value, bool exact_integer_storage) {
  const auto& object = value.object();
  fields(object, {"extensions", "classifiers", "trace_globals", "event_globals", "properties", "globals"});
  EventDataMetadata result;
  if (const auto* items = optional(object, "extensions")) for (const auto& item : items->array()) {
    const auto& entry = item.object();
    fields(entry, {"name", "prefix", "uri"});
    result.extensions.push_back({required(entry, "name").string(), required(entry, "prefix").string(), required(entry, "uri").string()});
  }
  if (const auto* items = optional(object, "classifiers")) for (const auto& item : items->array()) {
    const auto& entry = item.object();
    fields(entry, {"name", "keys"});
    result.classifiers.push_back({required(entry, "name").string(), strings(required(entry, "keys"))});
  }
  if (const auto* items = optional(object, "trace_globals")) result.trace_globals = attributes(*items, exact_integer_storage);
  if (const auto* items = optional(object, "event_globals")) result.event_globals = attributes(*items, exact_integer_storage);
  if (const auto* items = optional(object, "properties")) result.properties = attributes(*items, exact_integer_storage);
  if (const auto* items = optional(object, "globals")) for (const auto& item : items->array()) {
    const auto& entry = item.object();
    fields(entry, {"scope", "attributes"});
    result.globals.push_back({required(entry, "scope").string(), attributes(required(entry, "attributes"), exact_integer_storage)});
  }
  return result;
}

EventTable table(const Json& value, bool exact_integer_storage = false) {
  const auto& object = value.object();
  fields(object, {"columns", "rows", "dtypes", "index", "categorical_dtypes", "attributes", "metadata"});
  EventTable result;
  result.columns = strings(required(object, "columns"));
  for (const auto& item : required(object, "rows").array()) {
    const auto& cells = item.array();
    if (cells.size() != result.columns.size()) invalid("row width differs from columns");
    EventTable::Row row;
    for (std::size_t column = 0; column < cells.size(); ++column) {
      if (cells[column].null()) row.emplace_back(std::nullopt);
      else {
        auto cell = attribute(cells[column], result.columns[column], exact_integer_storage);
        if (cell.key != result.columns[column]) invalid("cell key differs from column");
        row.emplace_back(std::move(cell));
      }
    }
    result.rows.push_back(std::move(row));
  }
  if (const auto* dtypes = optional(object, "dtypes")) {
    result.dtypes = strings(*dtypes);
    if (!result.dtypes.empty() && result.dtypes.size() != result.columns.size()) invalid("dtype width differs from columns");
  }
  if (const auto* categories = optional(object, "categorical_dtypes")) {
    for (const auto& item : categories->array()) {
      if (item.null()) { result.categorical_dtypes.push_back(std::nullopt); continue; }
      const auto& entry = item.object();
      fields(entry, {"categories_dtype", "ordered", "categories"});
      result.categorical_dtypes.push_back(EventTableCategoricalDtype{required(entry, "categories_dtype").string(), attributes(required(entry, "categories"), exact_integer_storage), flag(entry, "ordered")});
    }
    if (!result.categorical_dtypes.empty() && result.categorical_dtypes.size() != result.columns.size()) invalid("categorical dtype width differs from columns");
  }
  if (const auto* index = optional(object, "index"); index && !index->null()) {
    const auto& entry = index->object();
    fields(entry, {"class", "dtype", "names", "labels"});
    EventTableIndex parsed;
    parsed.kind = required(entry, "class").string();
    parsed.dtype = required(entry, "dtype").string();
    parsed.names.clear();
    for (const auto& name : required(entry, "names").array()) parsed.names.push_back(name.null() ? std::nullopt : std::optional<std::string>{name.string()});
    for (const auto& label : required(entry, "labels").array()) parsed.labels.push_back(index_label(label, exact_integer_storage));
    if (parsed.labels.size() != result.rows.size()) invalid("index length differs from rows");
    result.index = std::move(parsed);
  }
  if (const auto* items = optional(object, "attributes")) result.attributes = attributes(*items, exact_integer_storage);
  if (const auto* items = optional(object, "metadata")) result.metadata = metadata(*items, exact_integer_storage);
  return result;
}

std::string escape_surrogate_bytes(std::string_view text) {
  std::string result;
  constexpr char hex[] = "0123456789abcdef";
  for (std::size_t i = 0; i < text.size(); ++i) {
    const auto first = static_cast<unsigned char>(text[i]);
    if (first == 0xed && text.size() - i >= 3) {
      const auto second = static_cast<unsigned char>(text[i + 1]);
      const auto third = static_cast<unsigned char>(text[i + 2]);
      if (second >= 0xa0 && second <= 0xbf && (third & 0xc0) == 0x80) {
        const auto code = ((first & 15) << 12) | ((second & 63) << 6) | (third & 63);
        result += "\\u";
        for (int shift = 12; shift >= 0; shift -= 4) result += hex[(code >> shift) & 15];
        i += 2;
        continue;
      }
    }
    result += text[i];
  }
  return result;
}

std::string quoted(std::string_view text, bool preserve_surrogates = false) {
  std::string result = "\"";
  constexpr char hex[] = "0123456789abcdef";
  for (std::size_t i = 0; i < text.size(); ++i) {
    const auto c = static_cast<unsigned char>(text[i]);
    if (preserve_surrogates && c == 0xed && text.size() - i >= 3) {
      const auto next = static_cast<unsigned char>(text[i + 1]);
      const auto last = static_cast<unsigned char>(text[i + 2]);
      if (next >= 0xa0 && next <= 0xbf && (last & 0xc0) == 0x80) {
        result += escape_surrogate_bytes(text.substr(i, 3));
        i += 2;
        continue;
      }
    }
    if (c == '\\' || c == '"') { result += '\\'; result += static_cast<char>(c); }
    else if (c < 32) { result += "\\u00"; result += hex[c >> 4]; result += hex[c & 15]; }
    else result += static_cast<char>(c);
  }
  return result + '"';
}

std::string json_text(const Json& value) {
  return std::visit([](const auto& cell) -> std::string {
    using T = std::decay_t<decltype(cell)>;
    if constexpr (std::is_same_v<T, std::nullptr_t>) return "null";
    else if constexpr (std::is_same_v<T, bool>) return cell ? "true" : "false";
    else if constexpr (std::is_same_v<T, Number>) return cell.text;
    else if constexpr (std::is_same_v<T, std::string>) return quoted(cell, true);
    else if constexpr (std::is_same_v<T, Json::Array>) {
      std::string result = "[";
      for (const auto& item : cell) {
        if (result.size() != 1) result += ',';
        result += json_text(item);
      }
      return result + ']';
    } else {
      std::string result = "{";
      for (const auto& [key, item] : cell) {
        if (result.size() != 1) result += ',';
        result += quoted(key, true) + ':' + json_text(item);
      }
      return result + '}';
    }
  }, value.value);
}

void enrich_attribute(Json& encoded, const EventAttribute& source);
void enrich_attributes(Json& encoded, const std::vector<EventAttribute>& source) {
  auto& items = std::get<Json::Array>(encoded.value);
  for (std::size_t i = 0; i < source.size(); ++i)
    enrich_attribute(items.at(i), source[i]);
}
void enrich_attribute(Json& encoded, const EventAttribute& source) {
  auto& object = std::get<Json::Object>(encoded.value);
  if (std::holds_alternative<std::int64_t>(source.typed_value))
    object.emplace("integer_storage", Json{std::string("int64")});
  else if (std::holds_alternative<std::uint64_t>(source.typed_value))
    object.emplace("integer_storage", Json{std::string("uint64")});
  if (!source.children.empty()) enrich_attributes(object.at("children"), source.children);
}
void enrich_index_label(Json& encoded, const EventTableIndexLabel& source) {
  auto& object = std::get<Json::Object>(encoded.value);
  if (const auto* scalar = std::get_if<EventAttribute>(&source.value))
    enrich_attribute(object.at("value"), *scalar);
  else {
    auto& items = std::get<Json::Array>(object.at("items").value);
    const auto& labels = std::get<std::vector<EventTableIndexLabel>>(source.value);
    for (std::size_t i = 0; i < labels.size(); ++i)
      enrich_index_label(items.at(i), labels[i]);
  }
}
void enrich_table(Json& encoded, const EventTable& source) {
  auto& object = std::get<Json::Object>(encoded.value);
  auto& rows = std::get<Json::Array>(object.at("rows").value);
  for (std::size_t r = 0; r < source.rows.size(); ++r) {
    auto& row = std::get<Json::Array>(rows.at(r).value);
    for (std::size_t c = 0; c < source.rows[r].size(); ++c)
      if (source.rows[r][c]) enrich_attribute(row.at(c), *source.rows[r][c]);
  }
  enrich_attributes(object.at("attributes"), source.attributes);
  if (!source.categorical_dtypes.empty()) {
    auto& categories = std::get<Json::Array>(object.at("categorical_dtypes").value);
    for (std::size_t i = 0; i < source.categorical_dtypes.size(); ++i)
      if (source.categorical_dtypes[i])
        enrich_attributes(std::get<Json::Object>(categories.at(i).value).at("categories"),
                          source.categorical_dtypes[i]->categories);
  }
  if (source.index) {
    auto& labels = std::get<Json::Array>(
        std::get<Json::Object>(object.at("index").value).at("labels").value);
    for (std::size_t i = 0; i < source.index->labels.size(); ++i)
      enrich_index_label(labels.at(i), source.index->labels[i]);
  }
  auto& meta = std::get<Json::Object>(object.at("metadata").value);
  enrich_attributes(meta.at("trace_globals"), source.metadata.trace_globals);
  enrich_attributes(meta.at("event_globals"), source.metadata.event_globals);
  enrich_attributes(meta.at("properties"), source.metadata.properties);
  auto& globals = std::get<Json::Array>(meta.at("globals").value);
  for (std::size_t i = 0; i < source.metadata.globals.size(); ++i)
    enrich_attributes(std::get<Json::Object>(globals.at(i).value).at("attributes"),
                      source.metadata.globals[i].attributes);
}

const std::string& numeric_token(const Json& value) {
  if (const auto* token = std::get_if<Number>(&value.value)) return token->text;
  invalid("expected a JSON numeric token");
}
std::uint64_t count(const Json& value) {
  return number<std::uint64_t>(numeric_token(value));
}
double real(const Json& value) {
  if (const auto* tag = std::get_if<std::string>(&value.value)) {
    if (*tag == "NaN") return std::numeric_limits<double>::quiet_NaN();
    if (*tag == "INF") return std::numeric_limits<double>::infinity();
    if (*tag == "-INF") return -std::numeric_limits<double>::infinity();
    invalid("nonfinite floats require NaN/INF/-INF tags");
  }
  const auto result = number<double>(numeric_token(value));
  if (!std::isfinite(result)) invalid("nonfinite floats require NaN/INF/-INF tags");
  return result;
}
Json count_json(std::uint64_t value) { return Json{Number{std::to_string(value)}}; }
Json real_json(double value) {
  if (std::isnan(value)) return Json{std::string("NaN")};
  if (std::isinf(value)) return Json{std::string(value < 0 ? "-INF" : "INF")};
  std::ostringstream output;
  output.imbue(std::locale::classic());
  output << std::setprecision(std::numeric_limits<double>::max_digits10) << value;
  return Json{Number{output.str()}};
}
using Counts = std::map<std::string, std::uint64_t>;
Counts counts(const Json& value) {
  Counts result;
  for (const auto& [key, item] : value.object()) result.emplace(key, count(item));
  return result;
}
Json counts_json(const Counts& values) {
  Json::Object result;
  for (const auto& [key, value] : values) result.emplace(key, count_json(value));
  return Json{std::move(result)};
}
Json strings_json(const std::vector<std::string>& values) {
  Json::Array result;
  for (const auto& value : values) result.push_back(Json{value});
  return Json{std::move(result)};
}
HeuristicFrequencyDfg pairs(const Json& value) {
  HeuristicFrequencyDfg result;
  for (const auto& item : value.array()) {
    const auto& object = item.object();
    fields(object, {"source", "target", "count"});
    const auto key = std::pair{required(object, "source").string(), required(object, "target").string()};
    if (!result.emplace(key, count(required(object, "count"))).second)
      invalid("duplicate directed pair");
  }
  return result;
}
Json pairs_json(const HeuristicFrequencyDfg& values) {
  Json::Array result;
  for (const auto& [pair, value] : values)
    result.push_back(Json{Json::Object{{"source", Json{pair.first}},
        {"target", Json{pair.second}}, {"count", count_json(value)}}});
  return Json{std::move(result)};
}
HeuristicTripleFrequencies triples(const Json& value) {
  HeuristicTripleFrequencies result;
  for (const auto& item : value.array()) {
    const auto& object = item.object();
    fields(object, {"source", "via", "target", "count"});
    const auto key = std::tuple{required(object, "source").string(),
        required(object, "via").string(), required(object, "target").string()};
    if (!result.emplace(key, count(required(object, "count"))).second)
      invalid("duplicate frequency triple");
  }
  return result;
}
Json triples_json(const HeuristicTripleFrequencies& values) {
  Json::Array result;
  for (const auto& [triple, value] : values)
    result.push_back(Json{Json::Object{{"source", Json{std::get<0>(triple)}},
        {"via", Json{std::get<1>(triple)}}, {"target", Json{std::get<2>(triple)}},
        {"count", count_json(value)}}});
  return Json{std::move(result)};
}
std::vector<double> reals(const Json& value) {
  std::vector<double> result;
  for (const auto& item : value.array()) result.push_back(real(item));
  return result;
}
Json reals_json(const std::vector<double>& values) {
  Json::Array result;
  for (const auto value : values) result.push_back(real_json(value));
  return Json{std::move(result)};
}

HeuristicStructuredValue structured(const Json& value) {
  const auto& object = value.object();
  fields(object, {"type", "value"});
  const auto& type = required(object, "type").string();
  const auto& cell = required(object, "value");
  if (type == "null") {
    if (!cell.null()) invalid("typed null requires null value");
    return HeuristicStructuredValue{nullptr};
  }
  if (type == "boolean") return HeuristicStructuredValue{cell.boolean()};
  if (type == "int64") return HeuristicStructuredValue{number<std::int64_t>(numeric_token(cell))};
  if (type == "uint64") return HeuristicStructuredValue{count(cell)};
  if (type == "float") return HeuristicStructuredValue{real(cell)};
  if (type == "string") return HeuristicStructuredValue{cell.string()};
  if (type == "array") {
    HeuristicStructuredValue::Array result;
    for (const auto& item : cell.array()) result.push_back(structured(item));
    return HeuristicStructuredValue{std::move(result)};
  }
  if (type == "object") {
    HeuristicStructuredValue::Object result;
    for (const auto& [key, item] : cell.object()) result.emplace(key, structured(item));
    return HeuristicStructuredValue{std::move(result)};
  }
  invalid("unknown structured value type");
}
Json structured_json(const HeuristicStructuredValue& value, unsigned depth = 0) {
  return std::visit([depth](const auto& cell) -> Json {
    using T = std::decay_t<decltype(cell)>;
    std::string type;
    Json encoded{nullptr};
    if constexpr (std::is_same_v<T, std::nullptr_t>) type = "null";
    else if constexpr (std::is_same_v<T, bool>) {type = "boolean"; encoded = Json{cell};}
    else if constexpr (std::is_same_v<T, std::int64_t>) {type = "int64"; encoded = Json{Number{std::to_string(cell)}};}
    else if constexpr (std::is_same_v<T, std::uint64_t>) {type = "uint64"; encoded = count_json(cell);}
    else if constexpr (std::is_same_v<T, double>) {type = "float"; encoded = real_json(cell);}
    else if constexpr (std::is_same_v<T, std::string>) {type = "string"; encoded = Json{cell};}
    else if constexpr (std::is_same_v<T, HeuristicStructuredValue::Array>) {
      type = "array";
      Json::Array result;
      for (const auto& item : cell) result.push_back(structured_json(item, depth + 1));
      encoded = Json{std::move(result)};
    } else {
      type = "object";
      Json::Object result;
      for (const auto& [key, item] : cell) result.emplace(key, structured_json(item, depth + 1));
      encoded = Json{std::move(result)};
    }
    return Json{Json::Object{{"type", Json{std::move(type)}}, {"value", std::move(encoded)}}};
  }, value.value);
}
HeuristicPerformanceValue performance(const Json& value) {
  const auto& object = value.object();
  fields(object, {"type", "value"});
  const auto& type = required(object, "type").string();
  const auto& cell = required(object, "value");
  if (type == "scalar") return real(cell);
  if (type == "samples") return reals(cell);
  if (type == "structured") return structured(cell);
  if (type == "summary") {
    const auto& summary = cell.object();
    fields(summary, {"mean", "median", "minimum", "maximum", "stdev", "sum"});
    return HeuristicPerformanceSummary{real(required(summary, "mean")),
        real(required(summary, "median")), real(required(summary, "minimum")),
        real(required(summary, "maximum")), real(required(summary, "stdev")),
        real(required(summary, "sum"))};
  }
  invalid("unknown performance value type");
}
Json performance_json(const HeuristicPerformanceValue& value) {
  return std::visit([](const auto& cell) -> Json {
    using T = std::decay_t<decltype(cell)>;
    std::string type;
    Json encoded{nullptr};
    if constexpr (std::is_same_v<T, double>) {type = "scalar"; encoded = real_json(cell);}
    else if constexpr (std::is_same_v<T, std::vector<double>>) {type = "samples"; encoded = reals_json(cell);}
    else if constexpr (std::is_same_v<T, HeuristicPerformanceSummary>) {
      type = "summary";
      encoded = Json{Json::Object{{"mean", real_json(cell.mean)}, {"median", real_json(cell.median)},
          {"minimum", real_json(cell.minimum)}, {"maximum", real_json(cell.maximum)},
          {"stdev", real_json(cell.stdev)}, {"sum", real_json(cell.sum)}}};
    } else {type = "structured"; encoded = structured_json(cell);}
    return Json{Json::Object{{"type", Json{std::move(type)}}, {"value", std::move(encoded)}}};
  }, value);
}
HeuristicBoundaryActivities boundaries(const Json& value) {
  if (std::holds_alternative<Json::Array>(value.value)) return strings(value);
  return counts(value);
}
Json boundaries_json(const HeuristicBoundaryActivities& value) {
  return std::visit([](const auto& cell) -> Json {
    if constexpr (std::is_same_v<std::decay_t<decltype(cell)>, std::vector<std::string>>)
      return strings_json(cell);
    else return counts_json(cell);
  }, value);
}

}  // namespace

EventTable parse_event_table_json(std::string_view input) {
  auto result = table(Parser(input, true).parse(), true);
  result.validate();
  return result;
}

std::string serialize_event_table_input_json(const EventTable& input) {
  input.validate();
  const auto serialized = escape_surrogate_bytes(serialize_event_table_json(input));
  auto encoded = Parser(serialized, true, false).parse();
  enrich_table(encoded, input);
  const auto result = json_text(encoded);
  // Validate scalar payloads through the same strict transport contract.
  auto checked = table(Parser(result, true, false).parse(), true);
  checked.validate();
  return result;
}

static AlphaDfgDiscoveryInput parse_alpha_dfg_input_json_impl(
    std::string_view input, bool bounded_transport) {
  const auto document = Parser(input, true, bounded_transport).parse();
  const auto& object = document.object();
  fields(object, {"kind", "dfg", "start_activities", "end_activities", "activities"});
  AlphaDfgDiscoveryInput result;
  const auto kind = required(object, "kind").string();
  if (kind == "sparse") result.kind = AlphaDfgDiscoveryInput::Kind::kSparse;
  else if (kind == "clean") result.kind = AlphaDfgDiscoveryInput::Kind::kClean;
  else if (kind == "graph") result.kind = AlphaDfgDiscoveryInput::Kind::kGraph;
  else invalid("unknown Alpha DFG kind");
  result.dfg = pairs(required(object, "dfg"));
  if (const auto* value = optional(object, "start_activities")) result.start_activities = counts(*value);
  if (const auto* value = optional(object, "end_activities")) result.end_activities = counts(*value);
  if (result.start_activities.has_value() != result.end_activities.has_value())
    invalid("Alpha DFG boundaries must both be omitted or both supplied");
  if (kind != "sparse" && !result.start_activities)
    invalid("clean/graph Alpha DFG requires both boundary maps");
  if (kind != "graph" && optional(object, "activities"))
    invalid("an activity registry requires graph Alpha DFG kind");
  if (kind == "graph") {
    result.activities = strings(required(object, "activities"));
    const std::set<std::string> names(result.activities.begin(), result.activities.end());
    if (names.size() != result.activities.size()) invalid("duplicate graph activity label");
    for (const auto& [pair, value] : result.dfg) {
      (void)value;
      if (!names.contains(pair.first) || !names.contains(pair.second)) invalid("DFG path label is absent from activity registry");
    }
    for (const auto* boundary : {&*result.start_activities, &*result.end_activities})
      for (const auto& [label, value] : *boundary) {
        (void)value;
        if (!names.contains(label)) invalid("boundary label is absent from activity registry");
      }
  }
  return result;
}

AlphaDfgDiscoveryInput parse_alpha_dfg_input_json(std::string_view input) {
  return parse_alpha_dfg_input_json_impl(input, true);
}

std::string serialize_alpha_dfg_input_json(const AlphaDfgDiscoveryInput& input) {
  std::string kind;
  switch (input.kind) {
    case AlphaDfgDiscoveryInput::Kind::kSparse: kind = "sparse"; break;
    case AlphaDfgDiscoveryInput::Kind::kClean: kind = "clean"; break;
    case AlphaDfgDiscoveryInput::Kind::kGraph: kind = "graph"; break;
    default: invalid("unknown Alpha DFG kind");
  }
  Json::Object object{{"kind", Json{kind}}, {"dfg", pairs_json(input.dfg)}};
  if (input.start_activities) object.emplace("start_activities", counts_json(*input.start_activities));
  if (input.end_activities) object.emplace("end_activities", counts_json(*input.end_activities));
  if (kind == "graph") object.emplace("activities", strings_json(input.activities));
  else if (!input.activities.empty()) invalid("an activity registry requires graph Alpha DFG kind");
  const auto result = json_text(Json{std::move(object)});
  (void)parse_alpha_dfg_input_json_impl(result, false);
  return result;
}

static HeuristicClassicDfgInput parse_heuristic_dfg_input_json_impl(
    std::string_view input, bool bounded_transport) {
  const auto document = Parser(input, true, bounded_transport).parse();
  const auto& object = document.object();
  fields(object, {"dfg", "activities", "activity_counts", "start_counts", "end_counts",
      "window_two_dfg", "frequency_triples", "performance_dfg", "start_activities",
      "end_activities", "performance_values", "dfg_window_2_supplied", "freq_triples_supplied", "dfg_order"});
  HeuristicClassicDfgInput result;
  result.dfg = pairs(required(object, "dfg"));
  if (const auto* value = optional(object, "activities")) result.activities = strings(*value);
  if (const auto* value = optional(object, "activity_counts")) result.activity_counts = counts(*value);
  if (const auto* value = optional(object, "start_counts")) result.start_counts = counts(*value);
  if (const auto* value = optional(object, "end_counts")) result.end_counts = counts(*value);
  if (const auto* value = optional(object, "window_two_dfg")) result.window_two_dfg = pairs(*value);
  if (const auto* value = optional(object, "frequency_triples")) result.frequency_triples = triples(*value);
  result.dfg_window_2_supplied = optional(object, "dfg_window_2_supplied")
      ? flag(object, "dfg_window_2_supplied") : optional(object, "window_two_dfg") != nullptr;
  result.freq_triples_supplied = optional(object, "freq_triples_supplied")
      ? flag(object, "freq_triples_supplied") : optional(object, "frequency_triples") != nullptr;
  if (const auto* value = optional(object, "start_activities")) result.start_activities = boundaries(*value);
  if (const auto* value = optional(object, "end_activities")) result.end_activities = boundaries(*value);
  if (const auto* value = optional(object, "performance_dfg")) {
    result.performance_dfg.emplace();
    for (const auto& item : value->array()) {
      const auto& record = item.object();
      fields(record, {"source", "target", "samples"});
      const auto pair = std::pair{required(record, "source").string(), required(record, "target").string()};
      if (!result.performance_dfg->emplace(pair, reals(required(record, "samples"))).second)
        invalid("duplicate sample performance pair");
    }
  }
  if (const auto* value = optional(object, "performance_values")) {
    result.performance_values.emplace();
    for (const auto& item : value->array()) {
      const auto& record = item.object();
      fields(record, {"source", "target", "value"});
      const auto pair = std::pair{required(record, "source").string(), required(record, "target").string()};
      if (!result.performance_values->emplace(pair, performance(required(record, "value"))).second)
        invalid("duplicate decorated performance pair");
    }
  }
  if (const auto* value = optional(object, "dfg_order")) {
    result.dfg_order.emplace();
    std::set<std::pair<std::string, std::string>> seen;
    for (const auto& item : value->array()) {
      const auto& record = item.object();
      fields(record, {"source", "target"});
      const auto pair = std::pair{required(record, "source").string(), required(record, "target").string()};
      if (!result.dfg.contains(pair) || !seen.insert(pair).second) invalid("DFG order is not a unique permutation of its paths");
      result.dfg_order->push_back(pair);
    }
    if (seen.size() != result.dfg.size()) invalid("DFG order must contain every path");
  }
  return result;
}

HeuristicClassicDfgInput parse_heuristic_dfg_input_json(std::string_view input) {
  return parse_heuristic_dfg_input_json_impl(input, true);
}

std::string serialize_heuristic_dfg_input_json(const HeuristicClassicDfgInput& input) {
  Json::Object object{{"dfg", pairs_json(input.dfg)},
      {"window_two_dfg", pairs_json(input.window_two_dfg)},
      {"frequency_triples", triples_json(input.frequency_triples)},
      {"dfg_window_2_supplied", Json{input.dfg_window_2_supplied}},
      {"freq_triples_supplied", Json{input.freq_triples_supplied}}};
  if (input.activities) object.emplace("activities", strings_json(*input.activities));
  if (input.activity_counts) object.emplace("activity_counts", counts_json(*input.activity_counts));
  if (input.start_counts) object.emplace("start_counts", counts_json(*input.start_counts));
  if (input.end_counts) object.emplace("end_counts", counts_json(*input.end_counts));
  if (input.start_activities) object.emplace("start_activities", boundaries_json(*input.start_activities));
  if (input.end_activities) object.emplace("end_activities", boundaries_json(*input.end_activities));
  if (input.performance_dfg) {
    Json::Array records;
    for (const auto& [pair, samples] : *input.performance_dfg)
      records.push_back(Json{Json::Object{{"source", Json{pair.first}},
          {"target", Json{pair.second}}, {"samples", reals_json(samples)}}});
    object.emplace("performance_dfg", Json{std::move(records)});
  }
  if (input.performance_values) {
    Json::Array records;
    for (const auto& [pair, value] : *input.performance_values)
      records.push_back(Json{Json::Object{{"source", Json{pair.first}},
          {"target", Json{pair.second}}, {"value", performance_json(value)}}});
    object.emplace("performance_values", Json{std::move(records)});
  }
  if (input.dfg_order) {
    Json::Array records;
    for (const auto& pair : *input.dfg_order)
      records.push_back(Json{Json::Object{{"source", Json{pair.first}}, {"target", Json{pair.second}}}});
    object.emplace("dfg_order", Json{std::move(records)});
  }
  const auto result = json_text(Json{std::move(object)});
  (void)parse_heuristic_dfg_input_json_impl(result, false);
  return result;
}

std::map<std::string, std::string> parse_workflow_selector_strings_json(std::string_view input) {
  const auto document = Parser(input, true).parse();
  std::map<std::string, std::string> result;
  for (const auto& [key, value] : document.object()) result.emplace(key, value.string());
  return result;
}

HeuristicStructuredValue parse_workflow_structured_value_json(std::string_view input) {
  return structured(Parser(input, true).parse());
}

std::string serialize_workflow_structured_value_json(const HeuristicStructuredValue& input) {
  const auto result = json_text(structured_json(input));
  (void)structured(Parser(result, true, false).parse());
  return result;
}

std::string serialize_workflow_performance_value_json(const HeuristicPerformanceValue& input) {
  const auto result = json_text(performance_json(input));
  (void)performance(Parser(result, true, false).parse());
  return result;
}

DataFrameFormatJsonResponse format_dataframe_json(std::string_view request) {
  const auto document = Parser(request).parse();
  const auto& root = document.object();
  fields(root, {"table", "options"});
  auto input = table(required(root, "table"));
  DataFrameFormatOptions options;
  if (const auto* supplied = optional(root, "options")) {
    const auto& object = supplied->object();
    fields(object, {"case_id", "activity_key", "timestamp_key", "start_timestamp_key", "timest_format"});
    options.case_id = text(object, "case_id", options.case_id);
    options.activity_key = text(object, "activity_key", options.activity_key);
    options.timestamp_key = text(object, "timestamp_key", options.timestamp_key);
    if (const auto* start = optional(object, "start_timestamp_key")) options.start_timestamp_key = start->null() ? std::nullopt : std::optional<std::string>{start->string()};
    if (const auto* format = optional(object, "timest_format")) options.timestamp_format = format->null() ? std::nullopt : std::optional<std::string>{format->string()};
  }
  try {
    const auto formatted = format_dataframe(input, options);
    return {true, "{\"family\":\"FORMAT_DATAFRAME\",\"table\":" + serialize_event_table_json(formatted) + ",\"input_after\":" + serialize_event_table_json(input) + '}'};
  } catch (const std::exception& error) {
    return {false, "{\"family\":\"FORMAT_DATAFRAME\",\"error\":" + quoted(error.what()) + ",\"input_after\":" + serialize_event_table_json(input) + '}'};
  }
}

}  // namespace minera
