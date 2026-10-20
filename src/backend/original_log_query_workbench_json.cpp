#include "minera/original_log_query_workbench_json.hpp"
#include "minera/exact_log_query_json.hpp"
#include "minera/exact_filter_discovery_json.hpp"
#include "minera/quality_workbench_json.hpp"
#include "minera/embedding_workbench_json.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
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
  throw std::invalid_argument("Original log query JSON: " + message);
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

const auto q = serialize_embedding_workbench_string_json;
const Json& required(const Json::Object& object, std::string_view key) {
  const auto found = object.find(key);
  if (found == object.end()) invalid("missing field " + std::string(key));
  return found->second;
}
void exact_fields(const Json::Object& object,
                  std::initializer_list<std::string_view> names) {
  for (const auto& [key, value] : object) {
    (void)value;
    if (std::find(names.begin(), names.end(), key) == names.end())
      invalid("unknown field " + key);
  }
  for (const auto name : names) (void)required(object, name);
}
template<class Integer> Integer integer(const Json& value) {
  Number string_token;
  const auto* token = std::get_if<Number>(&value.value);
  if (const auto* text = std::get_if<std::string>(&value.value)) {
    string_token.text = *text;
    token = &string_token;
  }
  if (!token || token->text.empty() || token->text.find_first_of(".eE") != std::string::npos)
    invalid("expected an exact native decimal integer");
  if constexpr (std::is_unsigned_v<Integer>) {
    if (!std::all_of(token->text.begin(), token->text.end(),
        [](char c) { return c >= '0' && c <= '9'; }))
      invalid("expected unsigned decimal integer token");
  }
  Integer result{};
  const auto parsed = std::from_chars(token->text.data(),
      token->text.data() + token->text.size(), result);
  if (parsed.ec != std::errc{} || parsed.ptr != token->text.data() + token->text.size())
    invalid("integer exceeds native storage range");
  return result;
}
double real(const Json& value) {
  if (const auto* tag = std::get_if<std::string>(&value.value)) {
    if (*tag == "NaN") return std::numeric_limits<double>::quiet_NaN();
    if (*tag == "INF") return std::numeric_limits<double>::infinity();
    if (*tag == "-INF") return -std::numeric_limits<double>::infinity();
    invalid("unknown IEEE double tag");
  }
  const auto* token = std::get_if<Number>(&value.value);
  if (!token) invalid("expected double token or IEEE tag");
  double result{};
  const auto parsed = std::from_chars(token->text.data(),
      token->text.data() + token->text.size(), result, std::chars_format::general);
  if (parsed.ec == std::errc{} && parsed.ptr == token->text.data() + token->text.size())
    return result;
  if (parsed.ec == std::errc::result_out_of_range) {
    std::istringstream input(token->text);
    input.imbue(std::locale::classic());
    input >> result;
    if (result == 0.0 && input.eof())
      return token->text.front() == '-' ? -0.0 : 0.0;
  }
  invalid("double token exceeds finite native storage; use an explicit IEEE tag");
}
void allowed_fields(const Json::Object& object,
                    std::initializer_list<std::string_view> names) {
  for (const auto& [key,value] : object) {
    (void)value;
    if (std::find(names.begin(),names.end(),key)==names.end()) invalid("unknown field "+key);
  }
}
const Json* field(const Json::Object& object,std::string_view name) {
  const auto found=object.find(name);
  return found==object.end()?nullptr:&found->second;
}
template<class T,class F> std::vector<T> vector_value(const Json& value,F convert) {
  std::vector<T> result;
  for (const auto& item : value.array()) result.push_back(convert(item));
  return result;
}
std::vector<std::string> strings(const Json& value) {
  return vector_value<std::string>(value,[](const Json& item) { return item.string(); });
}
NativeAttributeValue native_value(const Json& value) {
  const auto& object=value.object();exact_fields(object,{"index","value"});
  const auto index=integer<unsigned>(required(object,"index"));
  const auto& payload=required(object,"value");
  switch(index) {
    case 0:
      if (!payload.null()) invalid("monostate attribute requires null");
      return {};
    case 1:return NativeAttributeValue{std::in_place_type<std::int64_t>,integer<std::int64_t>(payload)};
    case 2:return NativeAttributeValue{std::in_place_type<double>,real(payload)};
    case 3:return NativeAttributeValue{std::in_place_type<bool>,payload.boolean()};
    case 4: {
      const auto& date=payload.object();exact_fields(date,{"value","resolution_index","timezone"});
      NativeDate result;
      result.value=required(date,"value").string();
      result.resolution=static_cast<NativeDateResolution>(integer<std::int32_t>(required(date,"resolution_index")));
      const auto& zone=required(date,"timezone");
      if (!zone.null()) result.timezone=zone.string();
      return NativeAttributeValue{std::in_place_type<NativeDate>,std::move(result)};
    }
    case 5:return NativeAttributeValue{std::in_place_type<std::uint64_t>,integer<std::uint64_t>(payload)};
    case 6:
      if (payload.string()!="NA") invalid("native NA payload must be NA");
      return NativeAttributeValue{std::in_place_type<NativeNA>,NativeNA{}};
    default:invalid("attribute typed value index must be0through6");
  }
}
EventAttribute attribute_value(const Json& value) {
  const auto& object=value.object();
  allowed_fields(object,{"key","value","kind_index","container_index","null",
      "scalar_wrapper","scalar_children_list","first_class_role_index",
      "first_class_state_index","first_class_source_key","typed_value","children"});
  EventAttribute result;
  if (const auto* x=field(object,"key")) result.key=x->string();
  if (const auto* x=field(object,"value")) result.value=x->string();
  if (const auto* x=field(object,"kind_index")) result.kind=static_cast<AttributeKind>(integer<std::int32_t>(*x));
  if (const auto* x=field(object,"container_index")) result.container=static_cast<AttributeContainerKind>(integer<std::int32_t>(*x));
  if (const auto* x=field(object,"null")) result.is_null=x->boolean();
  if (const auto* x=field(object,"scalar_wrapper")) result.scalar_wrapper=x->boolean();
  if (const auto* x=field(object,"scalar_children_list")) result.scalar_children_list=x->boolean();
  if (const auto* x=field(object,"first_class_role_index")) result.first_class_role=static_cast<FirstClassAttributeRole>(integer<std::int32_t>(*x));
  if (const auto* x=field(object,"first_class_state_index")) result.first_class_state=static_cast<FirstClassAttributeState>(integer<std::int32_t>(*x));
  if (const auto* x=field(object,"first_class_source_key")) result.first_class_source_key=x->string();
  if (const auto* x=field(object,"typed_value")) result.typed_value=native_value(*x);
  if (const auto* x=field(object,"children")) result.children=vector_value<EventAttribute>(*x,attribute_value);
  return result;
}
std::vector<EventAttribute> attributes_value(const Json& value) {
  return vector_value<EventAttribute>(value,attribute_value);
}
LogGlobal global_value(const Json& value) {
  const auto& object=value.object();exact_fields(object,{"scope","attributes"});
  return {required(object,"scope").string(),attributes_value(required(object,"attributes"))};
}
EventDataMetadata metadata_value(const Json& value) {
  const auto& object=value.object();
  exact_fields(object,{"extensions","classifiers","trace_globals","event_globals",
      "properties","globals","compatibility_snapshot_valid","compatibility_globals_snapshot",
      "compatibility_trace_globals_snapshot","compatibility_event_globals_snapshot"});
  EventDataMetadata result;
  for (const auto& value : required(object,"extensions").array()) {
    const auto& item=value.object();exact_fields(item,{"name","prefix","uri"});
    result.extensions.push_back({required(item,"name").string(),required(item,"prefix").string(),required(item,"uri").string()});
  }
  for (const auto& value : required(object,"classifiers").array()) {
    const auto& item=value.object();exact_fields(item,{"name","keys"});
    result.classifiers.push_back({required(item,"name").string(),strings(required(item,"keys"))});
  }
  result.trace_globals=attributes_value(required(object,"trace_globals"));
  result.event_globals=attributes_value(required(object,"event_globals"));
  result.properties=attributes_value(required(object,"properties"));
  result.globals=vector_value<LogGlobal>(required(object,"globals"),global_value);
  result.compatibility_snapshot_valid=required(object,"compatibility_snapshot_valid").boolean();
  result.compatibility_globals_snapshot=vector_value<LogGlobal>(required(object,"compatibility_globals_snapshot"),global_value);
  result.compatibility_trace_globals_snapshot=attributes_value(required(object,"compatibility_trace_globals_snapshot"));
  result.compatibility_event_globals_snapshot=attributes_value(required(object,"compatibility_event_globals_snapshot"));
  return result;
}
EventTableIndexLabel index_value(const Json& value) {
  const auto& object=value.object();exact_fields(object,{"index","value"});
  const auto index=integer<unsigned>(required(object,"index"));
  if (index==0) return {attribute_value(required(object,"value"))};
  if (index==1) return {vector_value<EventTableIndexLabel>(required(object,"value"),index_value)};
  invalid("table index label variant must be0or1");
}
std::string unhex(std::string_view value) {
  if (value.size()%2) invalid("odd-length raw table byte string");
  const auto digit=[](char c)->int {
    return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;
  };
  std::string result;result.reserve(value.size()/2);
  for (std::size_t i=0;i<value.size();i+=2) {
    const int a=digit(value[i]),b=digit(value[i+1]);
    if (a<0||b<0) invalid("invalid raw table hexadecimal bytes");
    result+=static_cast<char>((a<<4)|b);
  }
  return result;
}
void decode_attribute(EventAttribute& value) {
  value.key=unhex(value.key);value.value=unhex(value.value);
  value.first_class_source_key=unhex(value.first_class_source_key);
  if (auto* date=std::get_if<NativeDate>(&value.typed_value)) {
    date->value=unhex(date->value);if (date->timezone) *date->timezone=unhex(*date->timezone);
  }
  for (auto& child : value.children) decode_attribute(child);
}
void decode_attributes(std::vector<EventAttribute>& values) {
  for (auto& value : values) decode_attribute(value);
}
void decode_metadata(EventDataMetadata& value) {
  for (auto& x : value.extensions) { x.name=unhex(x.name);x.prefix=unhex(x.prefix);x.uri=unhex(x.uri); }
  for (auto& x : value.classifiers) { x.name=unhex(x.name);for (auto& key : x.keys) key=unhex(key); }
  decode_attributes(value.trace_globals);decode_attributes(value.event_globals);decode_attributes(value.properties);
  for (auto* globals : {&value.globals,&value.compatibility_globals_snapshot})
    for (auto& x : *globals) { x.scope=unhex(x.scope);decode_attributes(x.attributes); }
  decode_attributes(value.compatibility_trace_globals_snapshot);
  decode_attributes(value.compatibility_event_globals_snapshot);
}
void decode_index(EventTableIndexLabel& label) {
  if (auto* a=std::get_if<EventAttribute>(&label.value)) decode_attribute(*a);
  else for (auto& child : std::get<std::vector<EventTableIndexLabel>>(label.value)) decode_index(child);
}
EventTable table_value(const Json& document) {
  const auto& root=document.object();
  const bool shadow=root.contains("native_byte_strings");
  const Json* payload=&document;
  if (shadow) {
    exact_fields(root,{"native_byte_strings","native_schema","value"});
    if (required(root,"native_byte_strings").string()!="hex-v1" ||
        required(root,"native_schema").string()!="event-table-raw-v1") invalid("unknown raw table wrapper");
    payload=&required(root,"value");
  }
  const auto& object=payload->object();
  exact_fields(object,{"columns","rows","attributes","metadata","dtypes","index","categorical_dtypes"});
  EventTable result;
  result.columns=strings(required(object,"columns"));
  for (const auto& row : required(object,"rows").array()) {
    EventTable::Row cells;
    for (const auto& cell : row.array())
      cells.push_back(cell.null()?std::nullopt:std::make_optional(attribute_value(cell)));
    result.rows.push_back(std::move(cells));
  }
  result.attributes=attributes_value(required(object,"attributes"));
  result.metadata=metadata_value(required(object,"metadata"));
  result.dtypes=strings(required(object,"dtypes"));
  const auto& index=required(object,"index");
  if (!index.null()) {
    const auto& object=index.object();exact_fields(object,{"kind","dtype","names","labels"});
    EventTableIndex parsed;parsed.kind=required(object,"kind").string();parsed.dtype=required(object,"dtype").string();
    parsed.names.clear();
    for (const auto& name : required(object,"names").array())
      parsed.names.push_back(name.null()?std::nullopt:std::make_optional(name.string()));
    parsed.labels=vector_value<EventTableIndexLabel>(required(object,"labels"),index_value);
    result.index=std::move(parsed);
  }
  for (const auto& value : required(object,"categorical_dtypes").array()) {
    if (value.null()) { result.categorical_dtypes.push_back(std::nullopt);continue; }
    const auto& object=value.object();exact_fields(object,{"categories_dtype","categories","ordered"});
    result.categorical_dtypes.push_back(EventTableCategoricalDtype{
        required(object,"categories_dtype").string(),attributes_value(required(object,"categories")),
        required(object,"ordered").boolean()});
  }
  if (shadow) {
    for (auto& x : result.columns) x=unhex(x);
    for (auto& row : result.rows) for (auto& cell : row) if (cell) decode_attribute(*cell);
    decode_attributes(result.attributes);decode_metadata(result.metadata);
    for (auto& x : result.dtypes) x=unhex(x);
    if (result.index) {
      auto& index=*result.index;index.kind=unhex(index.kind);index.dtype=unhex(index.dtype);
      for (auto& name : index.names) if (name) *name=unhex(*name);
      for (auto& label : index.labels) decode_index(label);
    }
    for (auto& x : result.categorical_dtypes) if (x) {
      x->categories_dtype=unhex(x->categories_dtype);decode_attributes(x->categories);
    }
  }
  // No validate(): duplicate/ragged/latent states belong to the original
  // constructor and the chosen native consumer decides their semantics.
  return result;
}
QueryValue query_value(const Json& document) {
  const auto& object=document.object();exact_fields(object,{"index","value"});
  const auto index=integer<unsigned>(required(object,"index"));const auto& value=required(object,"value");
  switch(index) {
    case 0:if (!value.null()) invalid("monostate query value requires null");return std::monostate{};
    case 1:return integer<std::int64_t>(value);
    case 2:return real(value);
    case 3:return value.string();
    case 4: {
      const auto& blob=value.object();exact_fields(blob,{"hex"});
      return QueryBlob{required(blob,"hex").string()};
    }
    default:invalid("QueryValue index must be0through4");
  }
}
const Json& unwrapped(const Json& value,std::string_view type) {
  const auto& object=value.object();
  if (!object.contains("type")) return value;
  exact_fields(object,{"type","value"});
  if (required(object,"type").string()!=type) invalid("unexpected typed constructor wrapper");
  return required(object,"value");
}
std::vector<std::size_t> indices_value(const Json& value) {
  return vector_value<std::size_t>(value,[](const Json& x) { return integer<std::size_t>(x); });
}
std::string typed(std::string_view type,const std::string& value) {
  return "{\"type\":"+q(type)+",\"value\":"+value+'}';
}
std::string parts_json(const std::vector<DiscoveryWorkbenchPart>& parts) {
  std::string out="[";
  for (const auto& p : parts) {
    if (out.size()>1) out+=',';
    out+="{\"name\":"+q(p.name)+",\"filename\":"+(p.file?q(p.filename):"null")+
        ",\"bytes\":"+q(p.bytes)+",\"file\":"+(p.file?"true":"false")+'}';
  }
  return out+']';
}
std::string nullable(std::string_view value) { return value.empty()?"null":q(value); }
struct Artifact { std::optional<std::string> bytes,error; };
std::string artifact_json(const Artifact& a) {
  return "{\"bytes\":"+(a.bytes?q(*a.bytes):"null")+",\"error\":"+(a.error?q(*a.error):"null")+'}';
}
template<class T,class F> std::string object_json(const std::map<std::string,T>& fields,F convert) {
  std::string out="{";
  for (const auto& [key,value] : fields) { if (out.size()>1) out+=',';out+=q(key)+':'+convert(value); }
  return out+'}';
}
} // namespace

LogQueryTable parse_original_log_query_table_json(std::string_view text) {
  const auto parsed=Parser(text).parse();const auto& object=unwrapped(parsed,"LogQueryTable").object();
  exact_fields(object,{"table","event_index_column"});
  return {table_value(required(object,"table")),required(object,"event_index_column").string()};
}
LogQueryResult parse_original_log_query_result_json(std::string_view text) {
  const auto parsed=Parser(text).parse();const auto& object=unwrapped(parsed,"LogQueryResult").object();
  exact_fields(object,{"engine","sql","event_index_column","columns","rows",
      "output_truncated","can_filter","selected_event_indices"});
  LogQueryResult result;
  result.engine=required(object,"engine").string();result.sql=required(object,"sql").string();
  result.event_index_column=required(object,"event_index_column").string();result.columns=strings(required(object,"columns"));
  for (const auto& row : required(object,"rows").array()) result.rows.push_back(vector_value<QueryValue>(row,query_value));
  result.output_truncated=required(object,"output_truncated").boolean();
  result.can_filter=required(object,"can_filter").boolean();
  result.selected_event_indices=indices_value(required(object,"selected_event_indices"));
  return result;
}
std::vector<std::size_t> parse_original_log_query_indices_json(std::string_view text) {
  return indices_value(Parser(text).parse());
}
std::string serialize_original_log_query_indices_json(const std::vector<std::size_t>& indices) {
  std::string out="[";
  for (auto value : indices) { if (out.size()>1) out+=',';out+=std::to_string(value); }
  return out+']';
}
std::string serialize_original_log_query_table_json(const LogQueryTable& value) {
  return "{\"table\":"+serialize_quality_workbench_table_json(value.table)+
      ",\"event_index_column\":"+q(value.event_index_column)+'}';
}


DiscoveryWorkbenchResponse execute_original_log_query_workbench(
    const std::vector<DiscoveryWorkbenchPart>& parts,
    const std::vector<DiscoveryWorkbenchPart>& query,
    std::string_view input_order,std::string_view transport_error,
    const std::vector<std::string>* cli_argv) {
  std::map<std::string,std::vector<const DiscoveryWorkbenchPart*>> fields;
  for (const auto& p : query) fields[p.name].push_back(&p);
  for (const auto& p : parts) fields[p.name].push_back(&p);
  const auto has=[&](std::string_view key) { return fields.contains(std::string(key)); };
  const auto unique=[&](std::string_view key)->const DiscoveryWorkbenchPart* {
    const auto found=fields.find(std::string(key));
    return found!=fields.end()&&found->second.size()==1?found->second.front():nullptr;
  };
  const auto role_context=[&](std::string_view name) {
    const auto* p=unique(name);
    return p?"{\"filename\":"+(p->file?q(p->filename):"null")+",\"bytes\":"+q(p->bytes)+'}':std::string("null");
  };
  static const std::set<std::string> operations={"log_query_table","query_log_sqlite",
      "filter_query_events","log_query_json","log_query_table_json","sqlite_log_query_available"};
  std::string operation;
  if (const auto* p=unique("operation");p&&operations.contains(p->bytes)) operation=p->bytes;
  const auto context=[&](std::string_view inline_name,std::string_view file_name,
                         std::string_view inline_mode) {
    if (has(inline_name)==has(file_name)) return std::string("null");
    const bool file=has(file_name);const auto* p=unique(file?file_name:inline_name);
    if (!p) return std::string("null");
    return "{\"mode\":"+q(file?"file":inline_mode)+",\"filename\":"+
        (p->file?q(p->filename):"null")+",\"bytes\":"+q(p->bytes)+'}';
  };
  const auto sql_context=context("sql","sql_file","inline");
  const auto indices_context=context("indices_json","indices_file","inline");
  std::string options_context="null",whole_context="null";
  const bool explicit_options=has("query_options_json")||has("query_options_file");
  const auto* option_part=has("query_options_file")?unique("query_options_file"):unique("query_options_json");
  if (operation=="query_log_sqlite"&&!explicit_options)
    options_context="{\"mode\":\"native_default\",\"filename\":null,\"bytes\":null}";
  else if (has("query_options_json")!=has("query_options_file")&&option_part)
    options_context="{\"mode\":\"explicit_options\",\"filename\":"+
        (option_part->file?q(option_part->filename):"null")+",\"bytes\":"+q(option_part->bytes)+'}';
  bool whole_cases=false;
  if (operation=="filter_query_events"&&!has("whole_cases"))
    whole_context="{\"mode\":\"native_default\",\"value\":false}";
  else if (const auto* p=unique("whole_cases");p&&(p->bytes=="true"||p->bytes=="false")) {
    whole_cases=p->bytes=="true";
    whole_context="{\"mode\":\"explicit_whole_cases\",\"value\":"+std::string(whole_cases?"true":"false")+'}';
  }
  std::string cli="null";
  if (cli_argv) {
    cli="{\"argv\":[";
    for (const auto& arg : *cli_argv) { if (cli.back()!='[') cli+=',';cli+=q(arg); }
    cli+="]}";
  }
  std::map<std::string,std::string> snapshots,optional_errors;
  for (const auto* kind : {"source","table","input_result","options","indices"})
    for (const auto* suffix : {"_before","_after"}) {
      const auto key=std::string(kind)+suffix;snapshots[key]="null";optional_errors[key]="";
    }
  optional_errors["result"]="";
  std::map<std::string,Artifact> artifacts;
  for (const auto* name : {"source_log_json","query_table_json","query_result_json","options_json",
      "indices_json","filtered_log_json","native_json","selected_event_indices_json"}) artifacts.emplace(name,Artifact{});
  std::optional<EventLog> source,filtered;
  std::optional<LogQueryTable> table,computed_table;
  std::optional<LogQueryResult> input_result,computed_result;
  std::optional<LogQueryOptions> options;
  std::optional<std::vector<std::size_t>> indices;
  std::optional<bool> available;
  std::string sql,primary_string;
  bool entered=false,returned=false,before_attempted=false;
  std::string argument_form,phase="control",error_type,error_message,native_error="null",result_json="null";
  int status=200;
  const auto snapshot=[&](const std::string& name,auto make) {
    try { snapshots[name]=make(); }
    catch (const std::exception& e) { optional_errors[name]=e.what(); }
  };
  const auto capture=[&](bool after) {
    const std::string suffix=after?"_after":"_before";
    if (source) snapshot("source"+suffix,[&] { return typed("EventLog",serialize_embedding_workbench_raw_log_json(*source)); });
    if (table) snapshot("table"+suffix,[&] { return typed("LogQueryTable",serialize_original_log_query_table_json(*table)); });
    if (input_result) snapshot("input_result"+suffix,[&] { return typed("LogQueryResult",serialize_exact_log_query_result_json(*input_result)); });
    if (options) snapshot("options"+suffix,[&] { return typed("LogQueryOptions",serialize_exact_log_query_options_json({"sqlite",*options})); });
    if (indices) snapshot("indices"+suffix,[&] { return typed("size_t[]",serialize_original_log_query_indices_json(*indices)); });
  };
  const auto artifact=[&](const std::string& name,auto make) {
    try { artifacts[name].bytes=make(); }
    catch (const std::exception& e) { artifacts[name].error=e.what(); }
  };
  try {
    if (!transport_error.empty()) { phase="transport";throw std::invalid_argument(std::string(transport_error)); }
    static const std::set<std::string> allowed={"operation","source_file","table_file","result_file",
        "sql","sql_file","query_options_json","query_options_file","indices_json","indices_file","whole_cases"};
    for (const auto& [name,values] : fields) {
      if (values.size()!=1) throw std::invalid_argument("duplicate original log query control: "+name);
      if (!allowed.contains(name)) throw std::invalid_argument("unknown original log query control: "+name);
    }
    if (operation.empty()) throw std::invalid_argument("missing or unsupported original log query operation");
    const bool uses_source=operation=="log_query_table"||operation=="filter_query_events";
    const bool uses_table=operation=="query_log_sqlite"||operation=="log_query_table_json";
    const bool uses_result=operation=="log_query_json";
    const bool query_sqlite=operation=="query_log_sqlite",filter=operation=="filter_query_events";
    if (has("source_file")!=uses_source||has("table_file")!=uses_table||has("result_file")!=uses_result)
      throw std::invalid_argument("operation requires exactly its original source_file, table_file or result_file roles");
    if (!query_sqlite&&(has("sql")||has("sql_file")||explicit_options))
      throw std::invalid_argument("SQL/options controls are inapplicable to this operation");
    if (!filter&&(has("indices_json")||has("indices_file")||has("whole_cases")))
      throw std::invalid_argument("indices/whole_cases controls are inapplicable to this operation");
    if (query_sqlite&&has("sql")==has("sql_file"))
      throw std::invalid_argument("query_log_sqlite requires exactly one sql or sql_file by presence");
    if (has("query_options_json")&&has("query_options_file"))
      throw std::invalid_argument("query_options_json and query_options_file conflict by presence");
    if (filter&&has("indices_json")==has("indices_file"))
      throw std::invalid_argument("filter_query_events requires exactly one indices_json or indices_file by presence");
    static const std::set<std::string> file_roles={"source_file","table_file","result_file","sql_file","query_options_file","indices_file"};
    for (const auto& p : query) if (file_roles.contains(p.name))
      throw std::invalid_argument("original log query File role cannot be a query control: "+p.name);
    for (const auto& name : file_roles) if (const auto* p=unique(name);p&&!p->file)
      throw std::invalid_argument(name+" requires actual filename presence");
    if (filter&&has("whole_cases")&&whole_context=="null")
      throw std::invalid_argument("whole_cases must be true or false");
    if (query_sqlite) {
      phase="options_parse";
      if (explicit_options) options=parse_exact_log_query_options_json(option_part->bytes,"sqlite").value;
      else options.emplace();
      phase="sql_parse";
      sql=unique(has("sql_file")?"sql_file":"sql")->bytes;
    }
    if (uses_source) { phase="source_parse";source=parse_exact_ltl_log_json(unique("source_file")->bytes); }
    if (uses_table) { phase="table_parse";table=parse_original_log_query_table_json(unique("table_file")->bytes); }
    if (uses_result) { phase="result_parse";input_result=parse_original_log_query_result_json(unique("result_file")->bytes); }
    if (filter) { phase="indices_parse";indices=parse_original_log_query_indices_json(unique(has("indices_file")?"indices_file":"indices_json")->bytes); }
    before_attempted=true;
    capture(false);
    argument_form=query_sqlite?(explicit_options?"explicit_options":"native_default"):
        filter?(has("whole_cases")?"explicit_whole_cases":"native_default"):"direct";
    phase="native";entered=true;
    if (operation=="log_query_table") computed_table=log_query_table(*source);
    else if (query_sqlite) {
      // These are actual default-argument invocations, not reconstructed
      // explicit arguments which merely happen to equal today's defaults.
      if (explicit_options) computed_result=query_log_sqlite(*table,sql,*options);
      else computed_result=query_log_sqlite(*table,sql);
    } else if (filter) {
      if (has("whole_cases")) filtered=filter_query_events(*source,*indices,whole_cases);
      else filtered=filter_query_events(*source,*indices);
    } else if (operation=="log_query_json") primary_string=log_query_json(*input_result);
    else if (operation=="log_query_table_json") primary_string=log_query_table_json(*table);
    else available=sqlite_log_query_available();
    returned=true;
    capture(true);
  } catch (const std::exception& e) {
    status=400;error_type=exact_query_exception_type(e);error_message=e.what();
    if (entered&&!returned) native_error=exact_query_error(error_type,error_message);
    // Retain every complete parsed input, even when a later structural input
    // fails. Snapshot failures remain optional and never change call flags.
    if (!before_attempted) { before_attempted=true;capture(false); }
    capture(true);
  }
  if (source) artifact("source_log_json",[&] { return serialize_embedding_workbench_raw_log_json(*source); });
  if (table||computed_table) artifact("query_table_json",[&] {
    return serialize_original_log_query_table_json(computed_table?*computed_table:*table);
  });
  if (input_result||computed_result) {
    const auto& value=computed_result?*computed_result:*input_result;
    artifact("query_result_json",[&] { return serialize_exact_log_query_result_json(value); });
    artifact("selected_event_indices_json",[&] { return serialize_original_log_query_indices_json(value.selected_event_indices); });
  }
  if (options) artifact("options_json",[&] { return serialize_exact_log_query_options_json({"sqlite",*options}); });
  if (indices) artifact("indices_json",[&] { return serialize_original_log_query_indices_json(*indices); });
  if (filtered) artifact("filtered_log_json",[&] { return serialize_embedding_workbench_raw_log_json(*filtered); });
  if (returned) {
    if (operation=="log_query_json"||operation=="log_query_table_json") artifacts["native_json"].bytes=primary_string;
    else if (computed_result) artifact("native_json",[&] { return log_query_json(*computed_result); });
    else if (computed_table) artifact("native_json",[&] { return log_query_table_json(*computed_table); });
    const auto type=computed_table?"LogQueryTable":computed_result?"LogQueryResult":filtered?"EventLog":available?"bool":"NativeString";
    result_json=typed(type,"null");
    try {
      if (computed_table) result_json=typed(type,serialize_original_log_query_table_json(*computed_table));
      else if (computed_result) result_json=typed(type,serialize_exact_log_query_result_json(*computed_result));
      else if (filtered) result_json=typed(type,serialize_embedding_workbench_raw_log_json(*filtered));
      else if (available) result_json=typed(type,*available?"true":"false");
      else result_json=typed(type,q(primary_string));
    } catch (const std::exception& e) { optional_errors["result"]=e.what(); }
  } else if (entered&&(operation=="log_query_json"||operation=="log_query_table_json"))
    artifacts["native_json"].error=error_message;
  std::string unchanged="{";
  for (const auto* kind : {"source","table","input_result","options","indices"}) {
    if (unchanged.size()>1) unchanged+=',';
    const auto& before=snapshots[std::string(kind)+"_before"];
    const auto& after=snapshots[std::string(kind)+"_after"];
    unchanged+=q(kind)+':'+(before=="null"||after=="null"?"null":before==after?"true":"false");
  }
  unchanged+='}';
  std::string out="{\"family\":\"ORIGINAL_LOG_QUERY_WORKBENCH\",\"operation\":"+nullable(operation)+
      ",\"http_status\":"+std::to_string(status)+",\"input_order\":"+q(input_order)+
      ",\"original_inputs\":"+parts_json(parts)+",\"query_inputs\":"+parts_json(query)+
      ",\"input_context\":{\"source\":"+role_context("source_file")+",\"table\":"+role_context("table_file")+
      ",\"result\":"+role_context("result_file")+",\"sql\":"+sql_context+",\"options\":"+options_context+
      ",\"indices\":"+indices_context+",\"whole_cases\":"+whole_context+"},\"snapshots\":"+
      object_json(snapshots,[](const auto& value) { return value; })+",\"unchanged\":"+unchanged+
      ",\"call\":{\"name\":"+nullable(operation)+",\"argument_form\":"+nullable(argument_form)+
      ",\"entered\":"+(entered?"true":"false")+",\"returned\":"+(returned?"true":"false")+
      ",\"error\":"+native_error+"},\"result\":"+result_json+",\"artifacts\":"+
      object_json(artifacts,artifact_json)+",\"optional_errors\":"+
      object_json(optional_errors,[](const auto& value) { return nullable(value); })+",\"cli\":"+cli+",\"error\":";
  out+=status==200?"null":"{\"phase\":"+q(phase)+",\"type\":"+q(error_type)+",\"message\":"+q(error_message)+'}';
  out+='}';return {status,std::move(out)};
}
} // namespace minera
