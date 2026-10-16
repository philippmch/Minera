#include "minera/embedding_workbench_json.hpp"
#include "minera/graph_alignment_workbench_json.hpp"
#include "minera/footprints_workbench_json.hpp"
#include "minera/declare_workbench_json.hpp"
#include "minera/active_event_source.hpp"
#include "minera/dataframe_format_json.hpp"
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

namespace minera {
namespace {
[[noreturn]] void invalid(std::string message) {
  throw std::invalid_argument("Embedding workbench JSON: " + message);
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


std::int32_t signed_index(const Json& value) {
  const auto* token = std::get_if<Number>(&value.value);
  if (!token || token->text.empty()) invalid("expected a signed int32 JSON integer token");
  const auto& text = token->text;
  const auto start = text.front() == '-' ? std::size_t{1} : std::size_t{0};
  if (start == text.size() || !std::all_of(text.begin() + start, text.end(),
      [](char c) { return c >= '0' && c <= '9'; }))
    invalid("expected a signed int32 JSON integer token");
  std::int32_t result = 0;
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
  if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
    invalid("variant index exceeds int32 storage");
  return result;
}


void supplied_members(const Json::Object& object,std::set<std::string>* supplied) {
  if(!supplied)return;
  std::set<std::string> members;
  for(const auto& [name,unused]:object){(void)unused;members.insert(name);}
  *supplied=std::move(members);
}
const Json& required(const Json::Object& object,std::string_view name){
  if(const auto* v=field(object,name))return *v;
  invalid("missing field "+std::string(name));
}
std::string_view enum_name(std::int32_t index,bool provider){
  if(index==0)return provider?"hashing":"cases_transformers";
  if(index==1)return provider?"transformer":"events_transformers";
  return {};
}
std::int32_t enum_value(const Json& value,bool provider){
  if(const auto* name=std::get_if<std::string>(&value.value)){
    for(std::int32_t index=0;index<2;++index)if(*name==enum_name(index,provider))return index;
    invalid(provider?"unknown provider name":"unknown variant name");
  }
  if(const auto* object=std::get_if<Json::Object>(&value.value)){
    allowed_fields(*object,{"name","index"});
    const auto index=signed_index(required(*object,"index"));
    if(const auto* name=field(*object,"name")){
      const auto expected=enum_name(index,provider);
      if(expected.empty()){if(!name->null())invalid("unknown enum index requires null name metadata");}
      else if(name->string()!=expected)invalid("enum name metadata differs from index");
    }
    return index;
  }
  return signed_index(value);
}
void enum_json(std::ostream& out,std::int32_t index,bool provider){
  const auto name=enum_name(index,provider);
  out<<"{\"name\":";
  if(name.empty())out<<"null";else quoted(out,std::string(name));
  out<<",\"index\":"<<index<<'}';
}
double real_value(const Json& value){
  if(const auto* tag=std::get_if<std::string>(&value.value)){
    if(*tag=="NaN")return std::numeric_limits<double>::quiet_NaN();
    if(*tag=="INF")return std::numeric_limits<double>::infinity();
    if(*tag=="-INF")return -std::numeric_limits<double>::infinity();
    invalid("unknown IEEE double tag");
  }
  const auto* token=std::get_if<Number>(&value.value);
  if(!token)invalid("expected numeric double or IEEE tag");
  double result=0.0;
  const auto parsed=std::from_chars(token->text.data(),token->text.data()+token->text.size(),result);
  if(parsed.ec!=std::errc{}||parsed.ptr!=token->text.data()+token->text.size()||!std::isfinite(result))
    invalid("numeric token exceeds finite double storage");
  return result;
}
std::vector<double> real_vector(const Json& value){return vector_value<double>(value,real_value);}
void real_json(std::ostream& out,double value){
  if(std::isnan(value))out<<"\"NaN\"";
  else if(std::isinf(value))out<<(value<0?"\"-INF\"":"\"INF\"");
  else if(value==0&&std::signbit(value))out<<"-0.0";
  else{auto scalar=output();scalar<<std::setprecision(std::numeric_limits<double>::max_digits10)<<value;auto text=scalar.str();if(text.find_first_of(".eE")==std::string::npos)text+=".0";out<<text;}
}

bool scalar_utf8(std::string_view bytes){
 for(std::size_t i=0;i<bytes.size();){const auto a=static_cast<unsigned char>(bytes[i++]);if(a<128)continue;const int n=a>=0xc2&&a<=0xdf?1:a>=0xe0&&a<=0xef?2:a>=0xf0&&a<=0xf4?3:-1;if(n<0||i+static_cast<std::size_t>(n)>bytes.size())return false;unsigned code=a&(n==1?31:n==2?15:7);for(int j=0;j<n;++j){const auto b=static_cast<unsigned char>(bytes[i++]);if((b&192)!=128)return false;code=(code<<6)|(b&63);}if((n==1&&code<128)||(n==2&&code<2048)||(n==3&&code<65536)||code>0x10ffff||(code>=0xd800&&code<=0xdfff))return false;}
 return true;
}
std::string hex_bytes(std::string_view bytes){std::string result;constexpr char h[]="0123456789abcdef";result.reserve(bytes.size()*2);for(const unsigned char byte:bytes){result+=h[byte>>4];result+=h[byte&15];}return result;}
std::string unhex_bytes(std::string_view hex){
 if(hex.size()%2)invalid("hex byte string has odd length");std::string result;result.reserve(hex.size()/2);const auto digit=[](char c)->unsigned{if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;invalid("invalid hexadecimal byte string");};for(std::size_t i=0;i<hex.size();i+=2)result+=static_cast<char>((digit(hex[i])<<4)|digit(hex[i+1]));return result;
}
std::string native_string(const Json& value){
 if(const auto* s=std::get_if<std::string>(&value.value))return *s;
 const auto& o=value.object();allowed_fields(o,{"native_bytes","data"});if(required(o,"native_bytes").string()!="hex-v1")invalid("unknown native byte string version");return unhex_bytes(required(o,"data").string());
}
void native_string_json(std::ostream& out,std::string_view value){if(scalar_utf8(value))quoted(out,std::string(value));else{out<<"{\"native_bytes\":\"hex-v1\",\"data\":";quoted(out,hex_bytes(value));out<<'}';}}
std::string json_text(const Json& value){return std::visit([&](const auto& v)->std::string{using T=std::decay_t<decltype(v)>;if constexpr(std::is_same_v<T,std::nullptr_t>)return "null";else if constexpr(std::is_same_v<T,bool>)return v?"true":"false";else if constexpr(std::is_same_v<T,Number>)return v.text;else if constexpr(std::is_same_v<T,std::string>){auto out=output();quoted(out,v);return out.str();}else if constexpr(std::is_same_v<T,Json::Array>){std::string result="[";for(const auto& x:v){if(result.size()>1)result+=',';result+=json_text(x);}return result+']';}else{std::string result="{";for(const auto& [key,x]:v){if(result.size()>1)result+=',';result+=json_text(Json{key})+':'+json_text(x);}return result+'}';}},value.value);}
template<class F>void attribute_strings(EventAttribute& a,const F& f){f(a.key);f(a.value);f(a.first_class_source_key);if(auto* date=std::get_if<NativeDate>(&a.typed_value)){f(date->value);if(date->timezone)f(*date->timezone);}for(auto& child:a.children)attribute_strings(child,f);}
template<class F>void attributes_strings(std::vector<EventAttribute>& a,const F& f){for(auto& x:a)attribute_strings(x,f);}
template<class F>void metadata_strings(EventDataMetadata& m,const F& f){for(auto& x:m.extensions){f(x.name);f(x.prefix);f(x.uri);}for(auto& x:m.classifiers){f(x.name);for(auto& k:x.keys)f(k);}attributes_strings(m.trace_globals,f);attributes_strings(m.event_globals,f);attributes_strings(m.properties,f);const auto globals=[&](auto& values){for(auto& x:values){f(x.scope);attributes_strings(x.attributes,f);}};globals(m.globals);globals(m.compatibility_globals_snapshot);attributes_strings(m.compatibility_trace_globals_snapshot,f);attributes_strings(m.compatibility_event_globals_snapshot,f);}
template<class F>void log_strings(EventLog& log,const F& f){attributes_strings(log.attributes,f);metadata_strings(log.metadata,f);for(auto& t:log.traces){f(t.case_id);f(t.case_id_source_key);attributes_strings(t.attributes,f);attributes_strings(t.properties,f);if(t.case_id_attribute)attribute_strings(*t.case_id_attribute,f);for(auto& e:t.events){f(e.case_id);f(e.activity);f(e.timestamp);f(e.resource);f(e.lifecycle);f(e.case_id_source_key);f(e.activity_source_key);f(e.timestamp_source_key);f(e.resource_source_key);f(e.lifecycle_source_key);attributes_strings(e.attributes,f);for(auto* a:{&e.activity_attribute,&e.timestamp_attribute,&e.resource_attribute,&e.lifecycle_attribute})if(*a)attribute_strings(**a,f);}}}
template<class F>void index_strings(EventTableIndexLabel& index,const F& f){if(auto* a=std::get_if<EventAttribute>(&index.value))attribute_strings(*a,f);else for(auto& x:std::get<std::vector<EventTableIndexLabel>>(index.value))index_strings(x,f);}
template<class F>void table_strings(EventTable& table,const F& f){for(auto& x:table.columns)f(x);for(auto& row:table.rows)for(auto& cell:row)if(cell)attribute_strings(*cell,f);attributes_strings(table.attributes,f);metadata_strings(table.metadata,f);for(auto& x:table.dtypes)f(x);if(table.index){auto& i=*table.index;f(i.kind);f(i.dtype);for(auto& n:i.names)if(n)f(*n);for(auto& x:i.labels)index_strings(x,f);}for(auto& c:table.categorical_dtypes)if(c){f(c->categories_dtype);attributes_strings(c->categories,f);}}
template<class F>void stream_strings(EventStream& stream,const F& f){attributes_strings(stream.attributes,f);metadata_strings(stream.metadata,f);for(auto& record:stream.events)attributes_strings(record.fields,f);}
Json attribute_array(const std::vector<EventAttribute>& attrs){Trace holder;holder.attributes=attrs;const auto doc=Parser(serialize_declare_workbench_trace_json(holder),true,false).parse();return required(doc.object(),"attributes");}
std::vector<EventAttribute> raw_attributes(const Json& attrs){return parse_declare_workbench_trace_json("{\"attributes\":"+json_text(attrs)+'}').attributes;}
Json metadata_carrier(const EventDataMetadata& metadata){EventLog holder;holder.metadata=metadata;const auto doc=Parser(serialize_footprints_workbench_raw_log_json(holder),true,false).parse();return required(doc.object(),"metadata");}
EventDataMetadata raw_metadata(const Json& metadata){return parse_footprints_workbench_raw_log_json("{\"metadata\":"+json_text(metadata)+'}').metadata;}
Json strings_carrier(const std::vector<std::string>& strings){Json::Array a;for(const auto& s:strings)a.push_back(Json{s});return Json{std::move(a)};}
Json raw_index_json(const EventTableIndexLabel& label){Json payload;if(const auto* a=std::get_if<EventAttribute>(&label.value)){const auto list=attribute_array({*a});payload=list.array()[0];}else{Json::Array children;for(const auto& child:std::get<std::vector<EventTableIndexLabel>>(label.value))children.push_back(raw_index_json(child));payload=Json{std::move(children)};}return Json{Json::Object{{"index",Json{Number{std::to_string(label.value.index())}}},{"value",std::move(payload)}}};}
EventTableIndexLabel raw_index_value(const Json& value){const auto& o=value.object();allowed_fields(o,{"index","value"});const auto i=unsigned_integer(required(o,"index"));if(i==0)return {raw_attributes(Json{Json::Array{required(o,"value")}}).front()};if(i==1)return {vector_value<EventTableIndexLabel>(required(o,"value"),raw_index_value)};invalid("raw table index variant must be0or1");}
Json raw_table_carrier(const EventTable& table){
 Json::Array rows;for(const auto& row:table.rows){Json::Array cells;for(const auto& c:row)cells.push_back(c?attribute_array({*c}).array()[0]:Json{nullptr});rows.push_back(Json{std::move(cells)});}Json index{nullptr};if(table.index){const auto& i=*table.index;Json::Array names,labels;for(const auto& n:i.names)names.push_back(n?Json{*n}:Json{nullptr});for(const auto& x:i.labels)labels.push_back(raw_index_json(x));index=Json{Json::Object{{"kind",Json{i.kind}},{"dtype",Json{i.dtype}},{"names",Json{std::move(names)}},{"labels",Json{std::move(labels)}}}};}
 Json::Array categories;for(const auto& c:table.categorical_dtypes)categories.push_back(c?Json{Json::Object{{"categories_dtype",Json{c->categories_dtype}},{"categories",attribute_array(c->categories)},{"ordered",Json{c->ordered}}}}:Json{nullptr});return Json{Json::Object{{"columns",strings_carrier(table.columns)},{"rows",Json{std::move(rows)}},{"attributes",attribute_array(table.attributes)},{"metadata",metadata_carrier(table.metadata)},{"dtypes",strings_carrier(table.dtypes)},{"index",std::move(index)},{"categorical_dtypes",Json{std::move(categories)}}}};
}
EventTable raw_table_value(const Json& value){
 const auto& o=value.object();allowed_fields(o,{"columns","rows","attributes","metadata","dtypes","index","categorical_dtypes"});EventTable t;const auto strings=[](const Json& x){return vector_value<std::string>(x,[](const Json& s){return s.string();});};if(const auto* v=field(o,"columns"))t.columns=strings(*v);if(const auto* v=field(o,"rows"))for(const auto& row:v->array()){EventTable::Row r;for(const auto& c:row.array())r.push_back(c.null()?std::nullopt:std::make_optional(raw_attributes(Json{Json::Array{c}}).front()));t.rows.push_back(std::move(r));}if(const auto* v=field(o,"attributes"))t.attributes=raw_attributes(*v);if(const auto* v=field(o,"metadata"))t.metadata=raw_metadata(*v);if(const auto* v=field(o,"dtypes"))t.dtypes=strings(*v);
 if(const auto* v=field(o,"index");v&&!v->null()){const auto& i=v->object();allowed_fields(i,{"kind","dtype","names","labels"});EventTableIndex parsed;if(const auto* x=field(i,"kind"))parsed.kind=x->string();if(const auto* x=field(i,"dtype"))parsed.dtype=x->string();if(const auto* x=field(i,"names")){parsed.names.clear();for(const auto& n:x->array())parsed.names.push_back(n.null()?std::nullopt:std::make_optional(n.string()));}if(const auto* x=field(i,"labels"))parsed.labels=vector_value<EventTableIndexLabel>(*x,raw_index_value);t.index=std::move(parsed);}
 if(const auto* v=field(o,"categorical_dtypes"))for(const auto& c:v->array()){if(c.null()){t.categorical_dtypes.push_back(std::nullopt);continue;}const auto& d=c.object();allowed_fields(d,{"categories_dtype","categories","ordered"});EventTableCategoricalDtype parsed;if(const auto* x=field(d,"categories_dtype"))parsed.categories_dtype=x->string();if(const auto* x=field(d,"categories"))parsed.categories=raw_attributes(*x);if(const auto* x=field(d,"ordered"))parsed.ordered=x->boolean();t.categorical_dtypes.push_back(std::move(parsed));}return t;
}
EmbeddingItem item_value(const Json& value){
  const auto& object=value.object();allowed_fields(object,{"id","values"});EmbeddingItem result;
  if(const auto* v=field(object,"id"))result.id=native_string(*v);
  if(const auto* v=field(object,"values"))result.values=real_vector(*v);
  return result;
}
EmbeddingResult result_value(const Json& value){
  const auto& object=value.object();allowed_fields(object,{"variant","provider","embedding_model","attribute_key","dimensions","items"});EmbeddingResult result;
  if(const auto* v=field(object,"variant"))result.variant=static_cast<EmbeddingVariant>(enum_value(*v,false));
  if(const auto* v=field(object,"provider"))result.provider=static_cast<EmbeddingProvider>(enum_value(*v,true));
  if(const auto* v=field(object,"embedding_model"))result.embedding_model=native_string(*v);
  if(const auto* v=field(object,"attribute_key"))result.attribute_key=native_string(*v);
  if(const auto* v=field(object,"dimensions"))result.dimensions=native_unsigned<std::size_t>(*v);
  if(const auto* v=field(object,"items"))result.items=vector_value<EmbeddingItem>(*v,item_value);
  return result;
}
void result_json(std::ostream& out,const EmbeddingResult& result){
  out<<"{\"variant\":";enum_json(out,static_cast<std::int32_t>(result.variant),false);
  out<<",\"provider\":";enum_json(out,static_cast<std::int32_t>(result.provider),true);
  out<<",\"embedding_model\":";native_string_json(out,result.embedding_model);
  out<<",\"attribute_key\":";native_string_json(out,result.attribute_key);
  out<<",\"dimensions\":"<<result.dimensions<<",\"items\":";
  array_json(out,result.items,[](std::ostream& target,const EmbeddingItem& item){target<<"{\"id\":";native_string_json(target,item.id);target<<",\"values\":";array_json(target,item.values,real_json);target<<'}';});out<<'}';
}
}  // namespace

EmbeddingOptions parse_embedding_workbench_options_json(std::string_view input,std::set<std::string>* supplied){
  const auto json=Parser(input).parse();const auto& object=json.object();allowed_fields(object,{"variant","provider","embedding_model","attribute_key","event_id_key","dimensions","max_items","keep_cases"});EmbeddingOptions result;
  if(const auto* v=field(object,"variant"))result.variant=static_cast<EmbeddingVariant>(enum_value(*v,false));
  if(const auto* v=field(object,"provider"))result.provider=static_cast<EmbeddingProvider>(enum_value(*v,true));
  if(const auto* v=field(object,"embedding_model"))result.embedding_model=native_string(*v);
  if(const auto* v=field(object,"attribute_key"))result.attribute_key=native_string(*v);
  if(const auto* v=field(object,"event_id_key"))result.event_id_key=native_string(*v);
  if(const auto* v=field(object,"dimensions"))result.dimensions=native_unsigned<std::size_t>(*v);
  if(const auto* v=field(object,"max_items"))result.max_items=native_unsigned<std::size_t>(*v);
  if(const auto* v=field(object,"keep_cases"))result.keep_cases=v->boolean();
  supplied_members(object,supplied);return result;
}
std::string serialize_embedding_workbench_options_json(const EmbeddingOptions& options){
  auto out=output();out<<"{\"variant\":";enum_json(out,static_cast<std::int32_t>(options.variant),false);
  out<<",\"provider\":";enum_json(out,static_cast<std::int32_t>(options.provider),true);
  out<<",\"embedding_model\":";native_string_json(out,options.embedding_model);
  out<<",\"attribute_key\":";native_string_json(out,options.attribute_key);
  out<<",\"event_id_key\":";native_string_json(out,options.event_id_key);
  out<<",\"dimensions\":"<<options.dimensions<<",\"max_items\":"<<options.max_items<<",\"keep_cases\":";boolean(out,options.keep_cases);out<<'}';return out.str();
}
EmbeddingResult parse_embedding_workbench_result_json(std::string_view input,std::set<std::string>* supplied){
  const auto json=Parser(input).parse();auto result=result_value(json);supplied_members(json.object(),supplied);return result;
}
std::string serialize_embedding_workbench_result_json(const EmbeddingResult& result){auto out=output();result_json(out,result);return out.str();}
std::vector<EmbeddingSimilarity> parse_embedding_workbench_matches_json(std::string_view input){
  const auto json=Parser(input).parse();return vector_value<EmbeddingSimilarity>(json,[](const Json& v){const auto& object=v.object();allowed_fields(object,{"id","score"});EmbeddingSimilarity result;if(const auto* x=field(object,"id"))result.id=native_string(*x);if(const auto* x=field(object,"score"))result.score=real_value(*x);return result;});
}
std::string serialize_embedding_workbench_matches_json(const std::vector<EmbeddingSimilarity>& matches){auto out=output();array_json(out,matches,[](std::ostream& target,const EmbeddingSimilarity& match){target<<"{\"id\":";native_string_json(target,match.id);target<<",\"score\":";real_json(target,match.score);target<<'}';});return out.str();}
std::vector<double> parse_embedding_workbench_vector_json(std::string_view input){return real_vector(Parser(input).parse());}
std::string serialize_embedding_workbench_vector_json(const std::vector<double>& values){auto out=output();array_json(out,values,real_json);return out.str();}
std::size_t parse_embedding_workbench_size(std::string_view input){return native_unsigned<std::size_t>(Json{Number{std::string(input)}});}

std::string serialize_embedding_workbench_string_json(std::string_view value){auto out=output();native_string_json(out,value);return out.str();}
EventLog parse_embedding_workbench_raw_log_json(std::string_view input){const auto doc=Parser(input).parse();const auto& o=doc.object();if(!field(o,"native_byte_strings"))return parse_footprints_workbench_raw_log_json(input);allowed_fields(o,{"native_byte_strings","value"});if(required(o,"native_byte_strings").string()!="hex-v1")invalid("unknown raw log byte wrapper");auto log=parse_footprints_workbench_raw_log_json(json_text(required(o,"value")));log_strings(log,[](std::string& s){s=unhex_bytes(s);});return log;}
std::string serialize_embedding_workbench_raw_log_json(const EventLog& value){auto shadow=value;bool unsafe=false;log_strings(shadow,[&](std::string& s){unsafe=unsafe||!scalar_utf8(s);s=hex_bytes(s);});if(!unsafe)return serialize_footprints_workbench_raw_log_json(value);return "{\"native_byte_strings\":\"hex-v1\",\"value\":"+serialize_footprints_workbench_raw_log_json(shadow)+'}';}
EventTable parse_embedding_workbench_table_json(std::string_view input){const auto doc=Parser(input).parse();const auto& o=doc.object();if(!field(o,"native_byte_strings"))return parse_event_table_json(input);allowed_fields(o,{"native_byte_strings","native_schema","value"});if(required(o,"native_byte_strings").string()!="hex-v1"||required(o,"native_schema").string()!="event-table-raw-v1")invalid("unknown raw table byte wrapper");auto table=raw_table_value(required(o,"value"));table_strings(table,[](std::string& s){s=unhex_bytes(s);});return table;}
std::string serialize_embedding_workbench_table_json(const EventTable& value){auto shadow=value;bool unsafe=false;table_strings(shadow,[&](std::string& s){unsafe=unsafe||!scalar_utf8(s);s=hex_bytes(s);});if(!unsafe)try{return serialize_event_table_input_json(value);}catch(const std::exception&){/* Exact raw constructor fallback does not narrow stored state. */}return "{\"native_byte_strings\":\"hex-v1\",\"native_schema\":\"event-table-raw-v1\",\"value\":"+json_text(raw_table_carrier(shadow))+'}';}
std::string serialize_embedding_workbench_stream_json(const EventStream& value){auto shadow=value;bool unsafe=false;stream_strings(shadow,[&](std::string& s){unsafe=unsafe||!scalar_utf8(s);s=hex_bytes(s);});if(!unsafe)try{return serialize_active_source_stream_json(value);}catch(const std::exception&){/* Full raw fallback keeps native fields even when legacy inspection rejects. */}Json::Array events;for(const auto& event:shadow.events)events.push_back(Json{Json::Object{{"fields",attribute_array(event.fields)}}});return "{\"native_byte_strings\":\"hex-v1\",\"native_schema\":\"event-stream-raw-v1\",\"value\":"+json_text(Json{Json::Object{{"attributes",attribute_array(shadow.attributes)},{"metadata",metadata_carrier(shadow.metadata)},{"events",Json{std::move(events)}}}})+'}';}
}  // namespace minera
