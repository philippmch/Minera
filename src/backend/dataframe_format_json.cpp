#include "minera/dataframe_format_json.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "minera/dataframe_format.hpp"
#include "minera/event_table_json.hpp"

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
  explicit Parser(std::string_view text) : text_(text) {
    if (text.size() > 32u * 1024u * 1024u) invalid("request exceeds 32 MiB");
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
            if (position_ + 2 > text_.size() || text_[position_] != '\\' || text_[position_ + 1] != 'u') invalid("unpaired Unicode surrogate");
            position_ += 2;
            const auto low = hex4();
            if (low < 0xdc00 || low > 0xdfff) invalid("unpaired Unicode surrogate");
            code = 0x10000 + ((code - 0xd800) << 10) + low - 0xdc00;
          } else if (code >= 0xdc00 && code <= 0xdfff) invalid("unpaired Unicode surrogate");
          append_utf8(result, code);
          break;
        }
        default: invalid("unknown escape");
      }
    }
    invalid("unterminated string");
  }
  Json value(unsigned depth) {
    if (depth > 64) invalid("nesting exceeds 64 levels");
    if (++nodes_ > 1000000) invalid("request exceeds one million values");
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

EventAttribute attribute(const Json& value, std::string default_key = {});
std::vector<EventAttribute> attributes(const Json& value) {
  std::vector<EventAttribute> result;
  for (const auto& item : value.array()) result.push_back(attribute(item));
  return result;
}

EventAttribute attribute(const Json& value, std::string default_key) {
  const auto& object = value.object();
  fields(object, {"key", "value", "kind", "container", "null", "typed_value", "children", "scalar_wrapper", "scalar_children_list", "first_class_role", "first_class_state", "first_class_source_key"});
  EventAttribute result{text(object, "key", std::move(default_key)), text(object, "value"), AttributeKind::kString};
  const auto kind = text(object, "kind", "string");
  if (kind == "string") result.kind = AttributeKind::kString;
  else if (kind == "int") result.kind = AttributeKind::kInt;
  else if (kind == "float") result.kind = AttributeKind::kFloat;
  else if (kind == "boolean") result.kind = AttributeKind::kBoolean;
  else if (kind == "date") result.kind = AttributeKind::kDate;
  else if (kind == "id") result.kind = AttributeKind::kId;
  else if (kind == "bytes") result.kind = AttributeKind::kBytes;
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
  if (const auto* children = optional(object, "children")) result.children = attributes(*children);
  if (const auto* typed = optional(object, "typed_value")) {
    if (result.is_null && !typed->null()) invalid("null cell cannot have a typed value");
    if (!typed->null()) {
      if (result.kind == AttributeKind::kInt) {
        const auto token = number_text(*typed);
        if (token.starts_with('-')) result.typed_value = number<std::int64_t>(token);
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
  if (role == "none" && (state != "missing" || !result.first_class_source_key.empty())) invalid("first-class state/source requires a role");
  return result;
}

std::vector<std::string> strings(const Json& value) {
  std::vector<std::string> result;
  for (const auto& item : value.array()) result.push_back(item.string());
  return result;
}
EventTableIndexLabel index_label(const Json& value) {
  const auto& object = value.object();
  const auto kind = required(object, "kind").string();
  if (kind == "scalar") {
    fields(object, {"kind", "value"});
    return {attribute(required(object, "value"))};
  }
  if (kind != "tuple") invalid("unknown index label kind");
  fields(object, {"kind", "items"});
  std::vector<EventTableIndexLabel> result;
  for (const auto& item : required(object, "items").array()) result.push_back(index_label(item));
  return {std::move(result)};
}

EventDataMetadata metadata(const Json& value) {
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
  if (const auto* items = optional(object, "trace_globals")) result.trace_globals = attributes(*items);
  if (const auto* items = optional(object, "event_globals")) result.event_globals = attributes(*items);
  if (const auto* items = optional(object, "properties")) result.properties = attributes(*items);
  if (const auto* items = optional(object, "globals")) for (const auto& item : items->array()) {
    const auto& entry = item.object();
    fields(entry, {"scope", "attributes"});
    result.globals.push_back({required(entry, "scope").string(), attributes(required(entry, "attributes"))});
  }
  return result;
}

EventTable table(const Json& value) {
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
        auto cell = attribute(cells[column], result.columns[column]);
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
      result.categorical_dtypes.push_back(EventTableCategoricalDtype{required(entry, "categories_dtype").string(), attributes(required(entry, "categories")), flag(entry, "ordered")});
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
    for (const auto& label : required(entry, "labels").array()) parsed.labels.push_back(index_label(label));
    if (parsed.labels.size() != result.rows.size()) invalid("index length differs from rows");
    result.index = std::move(parsed);
  }
  if (const auto* items = optional(object, "attributes")) result.attributes = attributes(*items);
  if (const auto* items = optional(object, "metadata")) result.metadata = metadata(*items);
  return result;
}

std::string quoted(std::string_view text) {
  std::string result = "\"";
  constexpr char hex[] = "0123456789abcdef";
  for (const unsigned char c : text) {
    if (c == '\\' || c == '"') { result += '\\'; result += static_cast<char>(c); }
    else if (c < 32) { result += "\\u00"; result += hex[c >> 4]; result += hex[c & 15]; }
    else result += static_cast<char>(c);
  }
  return result + '"';
}

}  // namespace

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
