#include "minera/edit_distance_workbench_json.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/footprints_workbench_json.hpp"
#include "minera/graph_alignment_workbench_json.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/xes_event_log_importer.hpp"
#include "minera/log_conversion.hpp"
#include "minera/json_export.hpp"
#include "minera/xes_export.hpp"
#include <algorithm>
#include <charconv>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <initializer_list>
#include <limits>
#include <locale>
#include <iterator>
#include <map>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>
namespace minera {
namespace {
[[noreturn]] void invalid(std::string message) {
  throw std::invalid_argument("Edit-distance workbench JSON: " + message);
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

using Object=Json::Object;using Array=Json::Array;
Json nil(){return Json{nullptr};}
Json text(std::string value){return Json{std::move(value)};}
Json count(std::uint64_t value){return Json{Number{std::to_string(value)}};}
Json signed_count(std::int64_t value){return Json{Number{std::to_string(value)}};}
Json flag(bool value){return Json{value};}
Json document(std::string_view value){return Parser(value,true,false).parse();}
Json byte_string(std::string_view value){return document(serialize_embedding_workbench_string_json(value));}
Json real(double value){auto out=output();real_json(out,value);return document(out.str());}
Json strings(const std::vector<std::string>& values){Array out;for(const auto& v:values)out.push_back(byte_string(v));return Json{std::move(out)};}
std::vector<std::string> string_values(const Json& value){return vector_value<std::string>(value,native_string);}
Json bytes(std::string_view name,std::string_view value){const bool utf8=scalar_utf8(value);return Json{Object{{"name",byte_string(name)},{"encoding",text(utf8?"utf8":"hex")},{"data",text(utf8?std::string(value):hex_bytes(value))},{"bytes",count(value.size())},{"sha256",text(footprints_workbench_sha256(value))}}};}
Json exception(std::string_view phase,const std::exception& error){const char* type=dynamic_cast<const std::invalid_argument*>(&error)?"invalid_argument":dynamic_cast<const std::out_of_range*>(&error)?"out_of_range":dynamic_cast<const std::overflow_error*>(&error)?"overflow_error":dynamic_cast<const std::length_error*>(&error)?"length_error":dynamic_cast<const std::runtime_error*>(&error)?"runtime_error":"exception";return Json{Object{{"phase",text(std::string(phase))},{"exception_type",text(type)},{"message",byte_string(error.what())}}};}
std::int64_t int64(const Json& value){const auto* n=std::get_if<Number>(&value.value);if(!n)invalid("expected int64 token");const auto& s=n->text;const auto start=!s.empty()&&s[0]=='-'?1u:0u;if(s.size()==start||!std::all_of(s.begin()+start,s.end(),[](char c){return c>='0'&&c<='9';}))invalid("expected int64 token");std::int64_t out=0;auto p=std::from_chars(s.data(),s.data()+s.size(),out);if(p.ec!=std::errc{}||p.ptr!=s.data()+s.size())invalid("int64 storage overflow");return out;}
std::int32_t enum_index(const Json& value,std::string_view known){if(const auto* s=std::get_if<std::string>(&value.value)){if(*s==known)return 0;invalid("unknown enum name");}if(const auto* o=std::get_if<Object>(&value.value)){allowed_fields(*o,{"index","name"});const auto result=signed_index(required(*o,"index"));if(const auto* name=field(*o,"name")){if(result==0){if(name->string()!=known)invalid("enum name disagrees with index");}else if(!name->null())invalid("unknown index requires null enum name");}return result;}return signed_index(value);}
Json enumeration(std::int32_t index,std::string_view name){return Json{Object{{"index",signed_count(index)},{"name",index==0?text(std::string(name)):nil()}}};}
EditDistanceOptions options_value(const Json& value){const auto& o=value.object();allowed_fields(o,{"variant","perform_anti_alignment","deviation_cost","activity_attribute","timestamp_attribute","case_id_attribute"});EditDistanceOptions v;if(auto* x=field(o,"variant"))v.variant=static_cast<EditDistanceVariant>(enum_index(*x,"edit_distance"));if(auto* x=field(o,"perform_anti_alignment"))v.perform_anti_alignment=x->boolean();if(auto* x=field(o,"deviation_cost"))v.deviation_cost=unsigned_integer(*x);if(auto* x=field(o,"activity_attribute"))v.activity_attribute=native_string(*x);if(auto* x=field(o,"timestamp_attribute"))v.timestamp_attribute=native_string(*x);if(auto* x=field(o,"case_id_attribute"))v.case_id_attribute=native_string(*x);return v;}
Json options_json(const EditDistanceOptions& v){return Json{Object{{"variant",enumeration(static_cast<std::int32_t>(v.variant),"edit_distance")},{"perform_anti_alignment",flag(v.perform_anti_alignment)},{"deviation_cost",count(v.deviation_cost)},{"activity_attribute",byte_string(v.activity_attribute)},{"timestamp_attribute",byte_string(v.timestamp_attribute)},{"case_id_attribute",byte_string(v.case_id_attribute)}}};}
// Exact public65 raw carriers supply the complete attribute constructor.
EventAttribute attribute(const Json& value){const auto& o=value.object();if(field(o,"native_byte_strings")){allowed_fields(o,{"native_byte_strings","value"});if(required(o,"native_byte_strings").string()!="hex-v1")invalid("unknown attribute byte wrapper");auto result=parse_embedding_workbench_raw_log_json("{\"native_byte_strings\":\"hex-v1\",\"value\":{\"attributes\":["+json_text(required(o,"value"))+"]}}");return std::move(result.attributes.front());}auto result=parse_embedding_workbench_raw_log_json("{\"attributes\":["+json_text(value)+"]}");return std::move(result.attributes.front());}
Json attribute_json(const EventAttribute& value){EventLog holder;holder.attributes.push_back(value);const auto result=document(serialize_embedding_workbench_raw_log_json(holder));if(const auto* schema=field(result.object(),"native_byte_strings")){return Json{Object{{"native_byte_strings",*schema},{"value",required(required(result.object(),"value").object(),"attributes").array().front()}}};}return required(result.object(),"attributes").array().front();}
std::vector<EventAttribute> attributes(const Json& value){return vector_value<EventAttribute>(value,attribute);}
Json attributes_json(const std::vector<EventAttribute>& value){Array out;for(const auto& v:value)out.push_back(attribute_json(v));return Json{std::move(out)};}
Trace trace_value(const Json& value){const auto& o=value.object();if(field(o,"native_byte_strings")){allowed_fields(o,{"native_byte_strings","value"});if(required(o,"native_byte_strings").string()!="hex-v1")invalid("unknown trace byte wrapper");return parse_embedding_workbench_raw_log_json("{\"native_byte_strings\":\"hex-v1\",\"value\":{\"traces\":["+json_text(required(o,"value"))+"]}}").traces.front();}return parse_embedding_workbench_raw_log_json("{\"traces\":["+json_text(value)+"]}").traces.front();}
Json trace_json(const Trace& trace){EventLog holder;holder.traces.push_back(trace);auto result=document(serialize_embedding_workbench_raw_log_json(holder));if(const auto* schema=field(result.object(),"native_byte_strings"))return Json{Object{{"native_byte_strings",*schema},{"value",required(required(result.object(),"value").object(),"traces").array().front()}}};return required(result.object(),"traces").array().front();}
std::optional<EventAttribute> optional_attribute(const Json& value){return value.null()?std::nullopt:std::make_optional(attribute(value));}
Json optional_attribute_json(const std::optional<EventAttribute>& value){return value?attribute_json(*value):nil();}
EditDistanceMove move_value(const Json& value){const auto& o=value.object();allowed_fields(o,{"type","log_activity","model_activity","activity","log","model"});EditDistanceMove v;if(auto* x=field(o,"type"))v.type=static_cast<EditDistanceMoveType>(signed_index(*x));if(auto* x=field(o,"log_activity"))v.log_activity=native_string(*x);if(auto* x=field(o,"model_activity"))v.model_activity=native_string(*x);if(auto* x=field(o,"activity"))v.activity=native_string(*x);if(auto* x=field(o,"log"))v.log=native_string(*x);if(auto* x=field(o,"model"))v.model=native_string(*x);return v;}
Json move_json(const EditDistanceMove& v){return Json{Object{{"type",signed_count(static_cast<std::int32_t>(v.type))},{"log_activity",byte_string(v.log_activity)},{"model_activity",byte_string(v.model_activity)},{"activity",byte_string(v.activity)},{"log",byte_string(v.log)},{"model",byte_string(v.model)}}};}
EditDistanceTypedMove typed_move_value(const Json& value){const auto& o=value.object();allowed_fields(o,{"type","log_activity","model_activity"});EditDistanceTypedMove v;if(auto* x=field(o,"type"))v.type=static_cast<EditDistanceMoveType>(signed_index(*x));if(auto* x=field(o,"log_activity"))v.log_activity=optional_attribute(*x);if(auto* x=field(o,"model_activity"))v.model_activity=optional_attribute(*x);return v;}
Json move_json(const EditDistanceTypedMove& v){return Json{Object{{"type",signed_count(static_cast<std::int32_t>(v.type))},{"log_activity",optional_attribute_json(v.log_activity)},{"model_activity",optional_attribute_json(v.model_activity)}}};}
template<class T>Json moves_json(const std::vector<T>& values){Array out;for(const auto& v:values)out.push_back(move_json(v));return Json{std::move(out)};}
template<class Alignment>Alignment alignment_value(const Json& value){const auto& o=value.object();allowed_fields(o,{"alignment","moves","source_variant","target_variant","target_index","cost","fitness","best_worst_cost","bwc","denominator"});Alignment v;
 if constexpr(std::is_same_v<Alignment,EditDistanceAlignment>){if(auto* x=field(o,"alignment"))v.alignment=vector_value<EditDistanceMove>(*x,move_value);if(auto* x=field(o,"moves"))v.moves=vector_value<EditDistanceMove>(*x,move_value);if(auto* x=field(o,"source_variant"))v.source_variant=string_values(*x);if(auto* x=field(o,"target_variant"))v.target_variant=string_values(*x);}
 else{if(auto* x=field(o,"alignment"))v.alignment=vector_value<EditDistanceTypedMove>(*x,typed_move_value);if(auto* x=field(o,"moves"))v.moves=vector_value<EditDistanceTypedMove>(*x,typed_move_value);if(auto* x=field(o,"source_variant"))v.source_variant=attributes(*x);if(auto* x=field(o,"target_variant"))v.target_variant=attributes(*x);}
 if(auto* x=field(o,"target_index"))v.target_index=native_unsigned<std::size_t>(*x);if(auto* x=field(o,"cost"))v.cost=unsigned_integer(*x);if(auto* x=field(o,"fitness"))v.fitness=real_value(*x);if(auto* x=field(o,"best_worst_cost"))v.best_worst_cost=unsigned_integer(*x);if(auto* x=field(o,"bwc"))v.bwc=unsigned_integer(*x);if(auto* x=field(o,"denominator"))v.denominator=unsigned_integer(*x);return v;}
template<class Alignment>Json alignment_json(const Alignment& v){Json source,target;if constexpr(std::is_same_v<Alignment,EditDistanceAlignment>){source=strings(v.source_variant);target=strings(v.target_variant);}else{source=attributes_json(v.source_variant);target=attributes_json(v.target_variant);}return Json{Object{{"alignment",moves_json(v.alignment)},{"moves",moves_json(v.moves)},{"source_variant",std::move(source)},{"target_variant",std::move(target)},{"target_index",count(v.target_index)},{"cost",count(v.cost)},{"fitness",real(v.fitness)},{"best_worst_cost",count(v.best_worst_cost)},{"bwc",count(v.bwc)},{"denominator",count(v.denominator)}}};}
template<class Alignment>Json alignments_json(const std::vector<Alignment>& values){Array out;for(const auto& v:values)out.push_back(alignment_json(v));return Json{std::move(out)};}
template<class Result>Result direct_result(const Json& value){const auto& o=value.object();allowed_fields(o,{"alignment","cost"});Result v;if(auto* x=field(o,"cost"))v.cost=unsigned_integer(*x);if(auto* x=field(o,"alignment")){if constexpr(std::is_same_v<Result,EditDistanceTraceResult>)v.alignment=vector_value<EditDistanceMove>(*x,move_value);else v.alignment=vector_value<EditDistanceTypedMove>(*x,typed_move_value);}return v;}
template<class Result>Json direct_result_json(const Result& v){return Json{Object{{"alignment",moves_json(v.alignment)},{"cost",count(v.cost)}}};}

// Import/configuration codecs retain every declared native field and default.
struct RoleConfig {CsvImportOptions csv;XesImportOptions xes;LogConversionOptions preparation;Json selectors=nil();};
CsvImportOptions csv_options(const Json& value){const auto& o=value.object();allowed_fields(o,{"case_id_column","activity_column","timestamp_column","resource_column","lifecycle_column","require_timestamp_column","has_header","trim_fields","delimiter","allow_case_id_aliases"});CsvImportOptions v;
 #define ED_STRING(n) if(auto* x=field(o,#n))v.n=native_string(*x)
 #define ED_BOOL(n) if(auto* x=field(o,#n))v.n=x->boolean()
 ED_STRING(case_id_column);ED_STRING(activity_column);ED_STRING(timestamp_column);ED_STRING(resource_column);ED_STRING(lifecycle_column);ED_BOOL(require_timestamp_column);ED_BOOL(has_header);ED_BOOL(trim_fields);ED_BOOL(allow_case_id_aliases);if(auto* x=field(o,"delimiter")){const auto s=native_string(*x);if(s.size()!=1)invalid("CSV delimiter requires one native byte");v.delimiter=s[0];}return v;}
Json csv_json(const CsvImportOptions& v){return Json{Object{{"case_id_column",byte_string(v.case_id_column)},{"activity_column",byte_string(v.activity_column)},{"timestamp_column",byte_string(v.timestamp_column)},{"resource_column",byte_string(v.resource_column)},{"lifecycle_column",byte_string(v.lifecycle_column)},{"require_timestamp_column",flag(v.require_timestamp_column)},{"has_header",flag(v.has_header)},{"trim_fields",flag(v.trim_fields)},{"delimiter",byte_string(std::string(1,v.delimiter))},{"allow_case_id_aliases",flag(v.allow_case_id_aliases)}}};}
LogConversionOptions conversion_options(const Json& value){const auto& o=value.object();allowed_fields(o,{"variant","include_case_attributes","case_attribute_prefix","case_id_key","remove_null_fields","max_rows_to_materialize","deep_copy","compress","lazy_output","graph_case_id_attribute","include_directly_follows","case_attributes_as_nodes","event_attributes_as_nodes","return_variants","max_variants_to_return","max_graph_nodes","max_graph_edges"});LogConversionOptions v;
 if(auto* x=field(o,"variant"))v.variant=static_cast<LogConversionVariant>(signed_index(*x));ED_BOOL(include_case_attributes);ED_STRING(case_attribute_prefix);ED_STRING(case_id_key);ED_BOOL(remove_null_fields);if(auto* x=field(o,"max_rows_to_materialize"))v.max_rows_to_materialize=x->null()?std::nullopt:std::make_optional(native_unsigned<std::size_t>(*x));ED_BOOL(deep_copy);ED_BOOL(compress);ED_BOOL(lazy_output);ED_STRING(graph_case_id_attribute);ED_BOOL(include_directly_follows);if(auto* x=field(o,"case_attributes_as_nodes"))v.case_attributes_as_nodes=string_values(*x);if(auto* x=field(o,"event_attributes_as_nodes"))v.event_attributes_as_nodes=string_values(*x);ED_BOOL(return_variants);if(auto* x=field(o,"max_variants_to_return"))v.max_variants_to_return=x->null()?std::nullopt:std::make_optional(native_unsigned<std::size_t>(*x));if(auto* x=field(o,"max_graph_nodes"))v.max_graph_nodes=native_unsigned<std::size_t>(*x);if(auto* x=field(o,"max_graph_edges"))v.max_graph_edges=native_unsigned<std::size_t>(*x);return v;}
Json conversion_json(const LogConversionOptions& v){return Json{Object{{"variant",signed_count(static_cast<std::int32_t>(v.variant))},{"include_case_attributes",flag(v.include_case_attributes)},{"case_attribute_prefix",byte_string(v.case_attribute_prefix)},{"case_id_key",byte_string(v.case_id_key)},{"remove_null_fields",flag(v.remove_null_fields)},{"max_rows_to_materialize",v.max_rows_to_materialize?count(*v.max_rows_to_materialize):nil()},{"deep_copy",flag(v.deep_copy)},{"compress",flag(v.compress)},{"lazy_output",flag(v.lazy_output)},{"graph_case_id_attribute",byte_string(v.graph_case_id_attribute)},{"include_directly_follows",flag(v.include_directly_follows)},{"case_attributes_as_nodes",strings(v.case_attributes_as_nodes)},{"event_attributes_as_nodes",strings(v.event_attributes_as_nodes)},{"return_variants",flag(v.return_variants)},{"max_variants_to_return",v.max_variants_to_return?count(*v.max_variants_to_return):nil()},{"max_graph_nodes",count(v.max_graph_nodes)},{"max_graph_edges",count(v.max_graph_edges)}}};}
XesImportOptions xes_options(const Json& value){const auto& o=value.object();allowed_fields(o,{"variant","case_attribute","activity_attribute","timestamp_attribute","resource_attribute","lifecycle_attribute","require_timestamp","timestamp_sort","reverse_sort","max_traces","attributes_to_read","max_decompressed_bytes","return_legacy_log_object","return_pl_lazyframe","conversion_case_id_key","conversion_case_attribute_prefix","conversion_stream_postprocessing","conversion_deepcopy","conversion_include_case_attributes","conversion_compress","conversion_extensions"});XesImportOptions v;
 if(auto* x=field(o,"variant"))v.variant=static_cast<XesImportVariant>(signed_index(*x));ED_STRING(case_attribute);ED_STRING(activity_attribute);ED_STRING(timestamp_attribute);ED_STRING(resource_attribute);ED_STRING(lifecycle_attribute);ED_BOOL(require_timestamp);ED_BOOL(timestamp_sort);ED_BOOL(reverse_sort);if(auto* x=field(o,"max_traces"))v.max_traces=native_unsigned<std::size_t>(*x);if(auto* x=field(o,"attributes_to_read"))v.attributes_to_read=string_values(*x);if(auto* x=field(o,"max_decompressed_bytes"))v.max_decompressed_bytes=std::min(native_unsigned<std::size_t>(*x),XesImportOptions{}.max_decompressed_bytes); /* never above the default bound */ED_BOOL(return_legacy_log_object);ED_BOOL(return_pl_lazyframe);if(auto* x=field(o,"conversion_case_id_key"))v.conversion_case_id_key=x->null()?std::nullopt:std::make_optional(native_string(*x));if(auto* x=field(o,"conversion_case_attribute_prefix"))v.conversion_case_attribute_prefix=x->null()?std::nullopt:std::make_optional(native_string(*x));ED_BOOL(conversion_stream_postprocessing);ED_BOOL(conversion_deepcopy);ED_BOOL(conversion_include_case_attributes);ED_BOOL(conversion_compress);
 if(auto* x=field(o,"conversion_extensions")){if(x->null())v.conversion_extensions=std::nullopt;else{std::vector<LogExtension> out;for(const auto& item:x->array()){const auto& e=item.object();allowed_fields(e,{"name","prefix","uri"});LogExtension ext;if(auto* y=field(e,"name"))ext.name=native_string(*y);if(auto* y=field(e,"prefix"))ext.prefix=native_string(*y);if(auto* y=field(e,"uri"))ext.uri=native_string(*y);out.push_back(std::move(ext));}v.conversion_extensions=std::move(out);}}return v;}
 #undef ED_STRING
 #undef ED_BOOL
Json xes_json(const XesImportOptions& v){Json extensions=nil();if(v.conversion_extensions){Array out;for(const auto& x:*v.conversion_extensions)out.push_back(Json{Object{{"name",byte_string(x.name)},{"prefix",byte_string(x.prefix)},{"uri",byte_string(x.uri)}}});extensions=Json{std::move(out)};}return Json{Object{{"variant",signed_count(static_cast<std::int32_t>(v.variant))},{"case_attribute",byte_string(v.case_attribute)},{"activity_attribute",byte_string(v.activity_attribute)},{"timestamp_attribute",byte_string(v.timestamp_attribute)},{"resource_attribute",byte_string(v.resource_attribute)},{"lifecycle_attribute",byte_string(v.lifecycle_attribute)},{"require_timestamp",flag(v.require_timestamp)},{"timestamp_sort",flag(v.timestamp_sort)},{"reverse_sort",flag(v.reverse_sort)},{"max_traces",count(v.max_traces)},{"attributes_to_read",strings(v.attributes_to_read)},{"max_decompressed_bytes",count(v.max_decompressed_bytes)},{"return_legacy_log_object",flag(v.return_legacy_log_object)},{"return_pl_lazyframe",flag(v.return_pl_lazyframe)},{"conversion_case_id_key",v.conversion_case_id_key?byte_string(*v.conversion_case_id_key):nil()},{"conversion_case_attribute_prefix",v.conversion_case_attribute_prefix?byte_string(*v.conversion_case_attribute_prefix):nil()},{"conversion_stream_postprocessing",flag(v.conversion_stream_postprocessing)},{"conversion_deepcopy",flag(v.conversion_deepcopy)},{"conversion_include_case_attributes",flag(v.conversion_include_case_attributes)},{"conversion_compress",flag(v.conversion_compress)},{"conversion_extensions",std::move(extensions)}}};}

template<class F>void attribute_strings(EventAttribute& a,const F& f){f(a.key);f(a.value);f(a.first_class_source_key);if(auto* date=std::get_if<NativeDate>(&a.typed_value)){f(date->value);if(date->timezone)f(*date->timezone);}for(auto& child:a.children)attribute_strings(child,f);}
template<class F>void attribute_strings(std::vector<EventAttribute>& values,const F& f){for(auto& a:values)attribute_strings(a,f);}
template<class F>void metadata_strings(EventDataMetadata& m,const F& f){for(auto& x:m.extensions){f(x.name);f(x.prefix);f(x.uri);}for(auto& x:m.classifiers){f(x.name);for(auto& k:x.keys)f(k);}attribute_strings(m.trace_globals,f);attribute_strings(m.event_globals,f);attribute_strings(m.properties,f);const auto globals=[&](auto& values){for(auto& x:values){f(x.scope);attribute_strings(x.attributes,f);}};globals(m.globals);globals(m.compatibility_globals_snapshot);attribute_strings(m.compatibility_trace_globals_snapshot,f);attribute_strings(m.compatibility_event_globals_snapshot,f);}
Json stream_json(const EventStream& stream){auto shadow=stream;const auto hex=[](std::string& s){s=hex_bytes(s);};attribute_strings(shadow.attributes,hex);metadata_strings(shadow.metadata,hex);for(auto& event:shadow.events)attribute_strings(event.fields,hex);EventLog holder;holder.attributes=shadow.attributes;holder.metadata=shadow.metadata;auto state=document(serialize_footprints_workbench_raw_log_json(holder));Object body{{"attributes",required(state.object(),"attributes")},{"metadata",required(state.object(),"metadata")}};Array events;for(const auto& event:shadow.events){EventLog row;row.attributes=event.fields;const auto item=document(serialize_footprints_workbench_raw_log_json(row));events.push_back(Json{Object{{"fields",required(item.object(),"attributes")}}});}body.emplace("events",Json{std::move(events)});return Json{Object{{"native_byte_strings",text("hex-v1")},{"native_schema",text("event-stream-raw-v1")},{"value",Json{std::move(body)}}}};}
struct Source {
 std::string kind="log";EventLog log;EventStream stream;EventTable table;LazyEventTable lazy;bool lazy_valid=true;std::optional<std::size_t> declared_count;
 void refresh_lazy(){auto owned=std::make_shared<const EventTable>(table);LazyEventTable::Scanner scan;if(lazy_valid)scan=[owned](const LazyEventTable::RowConsumer& consume){for(const auto& row:owned->rows)consume(row);};else if(!declared_count){auto stored=table;stored.rows.clear();if(serialize_embedding_workbench_table_json(stored)==serialize_embedding_workbench_table_json(EventTable{})){lazy=LazyEventTable{};return;}}lazy=LazyEventTable(table.columns,std::move(scan),table.attributes,table.metadata,declared_count,table.dtypes,table.index,table.categorical_dtypes);}
 EventDataView view() const{if(kind=="log")return EventDataView(log);if(kind=="stream")return EventDataView(stream);if(kind=="table")return EventDataView(table);return EventDataView(lazy);}
 EventLog& require_log(){if(kind!="log")invalid("selected public EventLog signature requires an original or explicitly prepared log");return log;}
 Json json() const{if(kind=="log")return document(serialize_embedding_workbench_raw_log_json(log));if(kind=="stream")return stream_json(stream);if(kind=="table")return document(serialize_embedding_workbench_table_json(table));return Json{Object{{"table",document(serialize_embedding_workbench_table_json(table))},{"valid",flag(lazy_valid)},{"row_count",declared_count?count(*declared_count):nil()}}};}
};
EventStream stream_value(const Json& value){const Json* content=&value;bool hex=false;if(const auto* schema=field(value.object(),"native_byte_strings")){allowed_fields(value.object(),{"native_byte_strings","native_schema","value"});if(schema->string()!="hex-v1"||required(value.object(),"native_schema").string()!="event-stream-raw-v1")invalid("unknown raw stream byte wrapper");content=&required(value.object(),"value");hex=true;}
 const auto& o=content->object();allowed_fields(o,{"attributes","metadata","events"});const auto log_from=[&](std::string inner){return parse_embedding_workbench_raw_log_json(hex?"{\"native_byte_strings\":\"hex-v1\",\"value\":"+inner+'}':inner);};EventStream out;Object holder;if(auto* v=field(o,"attributes"))holder.emplace("attributes",*v);if(auto* v=field(o,"metadata"))holder.emplace("metadata",*v);auto parsed=log_from(json_text(Json{std::move(holder)}));out.attributes=std::move(parsed.attributes);out.metadata=std::move(parsed.metadata);if(auto* events=field(o,"events"))for(const auto& item:events->array()){const auto& event=item.object();allowed_fields(event,{"fields"});EventRecord record;if(auto* fields=field(event,"fields"))record.fields=log_from("{\"attributes\":"+json_text(*fields)+'}').attributes;out.events.push_back(std::move(record));}return out;}

using Parts=std::map<std::string,const EditDistanceWorkbenchPart*,std::less<>>;
const EditDistanceWorkbenchPart* part(const Parts& parts,std::string_view key){const auto it=parts.find(key);return it==parts.end()?nullptr:it->second;}
std::string control(const Parts& parts,std::string_view key,std::string fallback={}){const auto* value=part(parts,key);return value?value->bytes:std::move(fallback);}
Json parsed_control(const Parts& parts,const std::string& key){return Parser(parts.at(key)->bytes).parse();}
template<class F>void recorded_call(Array& calls,std::string name,F make){try{make();calls.push_back(Json{Object{{"name",text(std::move(name))},{"returned",flag(true)},{"error",nil()}}});}catch(const std::exception& e){calls.push_back(Json{Object{{"name",text(std::move(name))},{"returned",flag(false)},{"error",exception("preparation_error",e)}}});throw;}}
void read_source(Source& out,Json& context,const std::string& role,const Parts& parts,Array& parsed){
 const auto* file=part(parts,role+"_file");if(!file)invalid("missing "+role+"_file");auto format=control(parts,role+"_format","auto");const auto shape=control(parts,role+"_shape","original");const auto import_key=role+"_import_options_json",select_key=role+"_selectors_json",prep_key=role+"_preparation_options_json";
 context=Json{Object{{"format",text(format)},{"shape",text(shape)},{"original_kind",nil()},{"selected_kind",nil()},{"import_options",nil()},{"selectors",nil()},{"preparation_options",nil()},{"preparation_calls",Json{Array{}}}}};auto& ctx=std::get<Object>(context.value);auto& calls=std::get<Array>(ctx.at("preparation_calls").value);
 if(shape!="original"&&shape!="log"&&shape!="stream"&&shape!="table"&&shape!="lazy")invalid("unknown "+role+" shape");if(shape=="original"&&part(parts,prep_key))invalid("preparation options require explicit conversion shape");
 if(format=="auto"){auto name=file->filename;std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});const auto first=file->bytes.find_first_not_of(" \t\r\n");format=(name.ends_with(".xes")||name.ends_with(".xes.gz")||(file->bytes.size()>1&&static_cast<unsigned char>(file->bytes[0])==0x1f&&static_cast<unsigned char>(file->bytes[1])==0x8b)||(first!=std::string::npos&&file->bytes[first]=='<'))?"xes":"csv";ctx.emplace("detected_format",text(format));}
 const bool imported=format=="csv"||format=="xes";if(!imported&&(part(parts,import_key)||part(parts,select_key)))invalid("raw constructors do not accept importer/selector controls");RoleConfig config;Json import=nil(),prep=nil(),selectors=nil();
 if(imported){import=part(parts,import_key)?parsed_control(parts,import_key):Json{Object{}};if(format=="csv")config.csv=csv_options(import);else config.xes=xes_options(import);if(part(parts,import_key))parsed.push_back(text(import_key));ctx["import_options"]=format=="csv"?csv_json(config.csv):xes_json(config.xes);}
 if(part(parts,prep_key)){prep=parsed_control(parts,prep_key);config.preparation=conversion_options(prep);parsed.push_back(text(prep_key));ctx["preparation_options"]=conversion_json(config.preparation);}else if(shape!="original")ctx["preparation_options"]=conversion_json(config.preparation);
 if(part(parts,select_key)){selectors=parsed_control(parts,select_key);const auto& s=selectors.object();allowed_fields(s,{"case_id_key","activity_key","timestamp_key","resource_key","lifecycle_key"});const auto& explicit_import=import.object();for(const auto& [key,value]:s){const auto selected=native_string(value);std::string destination;if(key=="case_id_key")destination=format=="csv"?"case_id_column":"case_attribute";else if(key=="activity_key")destination=format=="csv"?"activity_column":"activity_attribute";else if(key=="timestamp_key")destination=format=="csv"?"timestamp_column":"timestamp_attribute";else if(key=="resource_key")destination=format=="csv"?"resource_column":"resource_attribute";else destination=format=="csv"?"lifecycle_column":"lifecycle_attribute";if(field(explicit_import,destination))invalid("selector collides with explicit importer member "+destination);if(key=="case_id_key"&&shape!="original"){if(!prep.null()&&field(prep.object(),"case_id_key"))invalid("selector collides with explicit preparation case_id_key");config.preparation.case_id_key=selected;}
 if(format=="csv"){if(key=="case_id_key")config.csv.case_id_column=selected;else if(key=="activity_key")config.csv.activity_column=selected;else if(key=="timestamp_key")config.csv.timestamp_column=selected;else if(key=="resource_key")config.csv.resource_column=selected;else config.csv.lifecycle_column=selected;}else{if(key=="case_id_key")config.xes.case_attribute=selected;else if(key=="activity_key")config.xes.activity_attribute=selected;else if(key=="timestamp_key")config.xes.timestamp_attribute=selected;else if(key=="resource_key")config.xes.resource_attribute=selected;else config.xes.lifecycle_attribute=selected;}}
 parsed.push_back(text(select_key));ctx["selectors"]=selectors;ctx["import_options"]=format=="csv"?csv_json(config.csv):xes_json(config.xes);if(shape!="original")ctx["preparation_options"]=conversion_json(config.preparation);}
 if(format=="raw_log"){out.log=parse_embedding_workbench_raw_log_json(file->bytes);out.kind="log";}
 else if(format=="raw_stream"){out.stream=stream_value(Parser(file->bytes).parse());out.kind="stream";}
 else if(format=="raw_table"){out.table=parse_embedding_workbench_table_json(file->bytes);out.kind="table";}
 else if(format=="raw_lazy"){const auto doc=Parser(file->bytes).parse();const auto& o=doc.object();allowed_fields(o,{"table","valid","row_count"});out.table=parse_embedding_workbench_table_json(json_text(required(o,"table")));out.lazy_valid=field(o,"valid")?required(o,"valid").boolean():true;out.declared_count=out.lazy_valid?std::make_optional(out.table.rows.size()):std::nullopt;if(const auto* v=field(o,"row_count"))out.declared_count=v->null()?std::nullopt:std::make_optional(native_unsigned<std::size_t>(*v));out.refresh_lazy();out.kind="lazy";}
 else if(format=="csv"){recorded_call(calls,"import_csv_event_data("+role+",options)",[&]{std::istringstream input(file->bytes);auto result=import_csv_event_data(input,config.csv);out.log=std::move(result.log);out.table=std::move(result.table);out.kind="log";});ctx.emplace("imported_outputs",Json{Object{{"log",document(serialize_embedding_workbench_raw_log_json(out.log))},{"table",document(serialize_embedding_workbench_table_json(out.table))}}});}
 else if(format=="xes"){recorded_call(calls,"import_xes_result("+role+",options)",[&]{std::istringstream input(file->bytes);auto result=import_xes_result(input,config.xes);out.log=std::move(result.log);out.table=std::move(result.table);if(result.kind==EventDataKind::kEventLog)out.kind="log";else if(result.kind==EventDataKind::kEventTable)out.kind="table";else if(result.kind==EventDataKind::kLazyEventTable){out.kind="lazy";out.lazy_valid=result.lazy_table.valid();out.declared_count=result.lazy_table.row_count();out.lazy=std::move(result.lazy_table);}else invalid("unexpected native XES source kind");});if(out.kind=="lazy"){recorded_call(calls,"LazyEventTable.materialize("+role+" imported lazy finite rows)",[&]{out.table=out.lazy.materialize();});out.refresh_lazy();}ctx.emplace("imported_outputs",Json{Object{{"log",document(serialize_embedding_workbench_raw_log_json(out.log))},{"table",document(serialize_embedding_workbench_table_json(out.table))}}});}
 else invalid("unknown "+role+" format");parsed.push_back(text(role+"_file"));ctx["original_kind"]=text(out.kind);ctx.emplace("original_constructor",out.json());
 if(shape!="original"){Source converted;converted.kind=shape;recorded_call(calls,"event_data_to_"+shape+"("+role+",options)",[&]{if(shape=="log")converted.log=event_data_to_event_log(out.view(),config.preparation);else if(shape=="stream")converted.stream=event_data_to_event_stream(out.view(),config.preparation);else if(shape=="table")converted.table=event_data_to_table(out.view(),config.preparation);else{converted.lazy=event_data_to_lazy_table(out.view(),config.preparation);converted.lazy_valid=converted.lazy.valid();converted.declared_count=converted.lazy.row_count();}});if(shape=="lazy"){recorded_call(calls,"LazyEventTable.materialize("+role+" prepared finite rows)",[&]{converted.table=converted.lazy.materialize(config.preparation.max_rows_to_materialize);});converted.refresh_lazy();}out=std::move(converted);}
 ctx["selected_kind"]=text(out.kind);
}

struct Direct {
 std::vector<std::string> list,set_original;std::set<std::string> set;EditDistanceTraceMapping mapping;EditDistanceTypedTraceMapping typed_mapping;std::optional<EditDistanceTraceCache> cache;std::vector<std::pair<std::string,EditDistanceTraceHandle>> handles;
 std::string id(const EditDistanceTraceHandle& handle){if(!handle)return {};for(const auto& [name,value]:handles)if(value==handle)return name;std::string name="native-result";for(std::size_t suffix=1;std::any_of(handles.begin(),handles.end(),[&](const auto& item){return item.first==name;});++suffix)name="native-result-"+std::to_string(suffix);handles.emplace_back(name,handle);return name;}
 Json handle(const EditDistanceTraceHandle& h){if(!h)return Json{Object{{"id",nil()},{"null_handle",flag(true)},{"has_value",flag(false)},{"type",nil()},{"value",nil()}}};const auto name=id(h);std::string type="empty_any";Json value=nil();if(h->has_value()){if(const auto* v=std::any_cast<std::string>(h.get())){type="string";value=byte_string(*v);}else if(const auto* v=std::any_cast<std::int64_t>(h.get())){type="int64";value=signed_count(*v);}else if(const auto* v=std::any_cast<std::uint64_t>(h.get())){type="uint64";value=count(*v);}else if(const auto* v=std::any_cast<double>(h.get())){type="double";value=real(*v);}else if(const auto* v=std::any_cast<bool>(h.get())){type="bool";value=flag(*v);}else if(const auto* v=std::any_cast<EditDistanceTraceResult>(h.get())){type="trace_result";value=direct_result_json(*v);}else if(const auto* v=std::any_cast<EditDistanceTypedTraceResult>(h.get())){type="typed_trace_result";value=direct_result_json(*v);}else invalid("unavailable native std::any payload type");}return Json{Object{{"id",byte_string(name)},{"null_handle",flag(false)},{"has_value",flag(h->has_value())},{"type",text(type)},{"value",std::move(value)}}};}
 Json json(bool typed){Array mapping_json,set_json,cache_json,handles_json;for(const auto& s:set)set_json.push_back(byte_string(s));if(typed){for(const auto& [key,value]:typed_mapping)mapping_json.push_back(Json{Array{attribute_json(key),byte_string(value)}});}else for(const auto& [key,value]:mapping)mapping_json.push_back(Json{Array{byte_string(key),byte_string(value)}});if(cache)for(const auto& [encoding,h]:*cache)cache_json.push_back(Json{Object{{"encoding",byte_string(encoding)},{"handle_ref",h?byte_string(id(h)):nil()},{"handle",handle(h)}}});for(std::size_t i=0;i<handles.size();++i){const auto name=handles[i].first;const auto value=handles[i].second;handles_json.push_back(Json{Object{{"id",byte_string(name)},{"handle",handle(value)}}});}return Json{Object{{"list_encodings",strings(list)},{"set_encodings",strings(set_original)},{"native_set_encodings",Json{std::move(set_json)}},{"mapping",Json{std::move(mapping_json)}},{"cache",cache?Json{std::move(cache_json)}:nil()},{"handles",Json{std::move(handles_json)}}}};}
};
void read_direct(Direct& out,const Json& value,bool typed){const auto& o=value.object();allowed_fields(o,{"list_encodings","set_encodings","mapping","cache","handles"});if(auto* v=field(o,"list_encodings"))out.list=string_values(*v);if(auto* v=field(o,"set_encodings")){out.set_original=string_values(*v);out.set.insert(out.set_original.begin(),out.set_original.end());}if(auto* v=field(o,"mapping"))for(const auto& pair:v->array()){const auto& entry=pair.array();if(entry.size()!=2)invalid("mapping pair requires two stored members");if(typed)out.typed_mapping.emplace_back(attribute(entry[0]),native_string(entry[1]));else out.mapping.emplace_back(native_string(entry[0]),native_string(entry[1]));}
 if(auto* v=field(o,"handles"))for(const auto& item:v->array()){const auto& h=item.object();allowed_fields(h,{"id","type","value"});const auto name=native_string(required(h,"id"));if(std::any_of(out.handles.begin(),out.handles.end(),[&](const auto& x){return x.first==name;}))invalid("duplicate request-local handle ID");const auto& t=required(h,"type");EditDistanceTraceHandle handle;if(!t.null()){const auto type=t.string();handle=std::make_shared<std::any>();if(type=="empty_any"){}else if(type=="string")*handle=native_string(required(h,"value"));else if(type=="int64")*handle=int64(required(h,"value"));else if(type=="uint64")*handle=unsigned_integer(required(h,"value"));else if(type=="double")*handle=real_value(required(h,"value"));else if(type=="bool")*handle=required(h,"value").boolean();else if(type=="trace_result")*handle=direct_result<EditDistanceTraceResult>(required(h,"value"));else if(type=="typed_trace_result")*handle=direct_result<EditDistanceTypedTraceResult>(required(h,"value"));else invalid("unavailable std::any constructor type "+type);}out.handles.emplace_back(name,std::move(handle));}
 if(auto* v=field(o,"cache");v&&!v->null()){out.cache.emplace();for(const auto& item:v->array()){const auto& c=item.object();allowed_fields(c,{"encoding","handle_ref"});const auto encoding=native_string(required(c,"encoding"));const auto& ref=required(c,"handle_ref");EditDistanceTraceHandle handle;if(!ref.null()){const auto name=native_string(ref);const auto it=std::find_if(out.handles.begin(),out.handles.end(),[&](const auto& h){return h.first==name;});if(it==out.handles.end())invalid("unknown request-local handle_ref");handle=it->second;}out.cache->emplace_back(encoding,std::move(handle));}}
}

struct Location {std::string owner;Array path;EventAttribute* value;};
using Locations=std::vector<Location>;
Array child(Array path,std::string name){path.push_back(text(std::move(name)));return path;}
Array child(Array path,std::size_t index){path.push_back(count(index));return path;}
void locate(Locations& out,const std::string& owner,Array path,EventAttribute& value){out.push_back({owner,path,&value});for(std::size_t i=0;i<value.children.size();++i)locate(out,owner,child(child(path,"children"),i),value.children[i]);}
void locate(Locations& out,const std::string& owner,Array path,std::vector<EventAttribute>& values){for(std::size_t i=0;i<values.size();++i)locate(out,owner,child(path,i),values[i]);}
void locate(Locations& out,const std::string& owner,Array path,EventDataMetadata& value){locate(out,owner,child(path,"trace_globals"),value.trace_globals);locate(out,owner,child(path,"event_globals"),value.event_globals);locate(out,owner,child(path,"properties"),value.properties);locate(out,owner,child(path,"compatibility_trace_globals_snapshot"),value.compatibility_trace_globals_snapshot);locate(out,owner,child(path,"compatibility_event_globals_snapshot"),value.compatibility_event_globals_snapshot);const auto globals=[&](std::string name,auto& values){for(std::size_t i=0;i<values.size();++i)locate(out,owner,child(child(child(path,name),i),"attributes"),values[i].attributes);};globals("globals",value.globals);globals("compatibility_globals_snapshot",value.compatibility_globals_snapshot);}
void locate(Locations& out,const std::string& owner,Array path,Event& value){locate(out,owner,child(path,"attributes"),value.attributes);const auto optional=[&](std::string name,auto& v){if(v)locate(out,owner,child(path,name),*v);};optional("activity_attribute",value.activity_attribute);optional("timestamp_attribute",value.timestamp_attribute);optional("resource_attribute",value.resource_attribute);optional("lifecycle_attribute",value.lifecycle_attribute);}
void locate(Locations& out,const std::string& owner,Array path,Trace& value){locate(out,owner,child(path,"attributes"),value.attributes);locate(out,owner,child(path,"properties"),value.properties);if(value.case_id_attribute)locate(out,owner,child(path,"case_id_attribute"),*value.case_id_attribute);for(std::size_t i=0;i<value.events.size();++i)locate(out,owner,child(child(path,"events"),i),value.events[i]);}
void locate(Locations& out,const std::string& owner,Array path,EventLog& value){locate(out,owner,child(path,"attributes"),value.attributes);locate(out,owner,child(path,"metadata"),value.metadata);for(std::size_t i=0;i<value.traces.size();++i)locate(out,owner,child(child(path,"traces"),i),value.traces[i]);}
void locate(Locations& out,const std::string& owner,Array path,EventTableIndexLabel& label){if(auto* a=std::get_if<EventAttribute>(&label.value))locate(out,owner,child(path,"value"),*a);else{auto& tuple=std::get<std::vector<EventTableIndexLabel>>(label.value);for(std::size_t i=0;i<tuple.size();++i)locate(out,owner,child(child(path,"value"),i),tuple[i]);}}
void locate(Locations& out,const std::string& owner,Array path,EventTable& value){locate(out,owner,child(path,"attributes"),value.attributes);locate(out,owner,child(path,"metadata"),value.metadata);for(std::size_t row=0;row<value.rows.size();++row)for(std::size_t col=0;col<value.rows[row].size();++col)if(value.rows[row][col])locate(out,owner,child(child(child(path,"rows"),row),col),*value.rows[row][col]);if(value.index)for(std::size_t i=0;i<value.index->labels.size();++i)locate(out,owner,child(child(child(path,"index"),"labels"),i),value.index->labels[i]);for(std::size_t i=0;i<value.categorical_dtypes.size();++i)if(value.categorical_dtypes[i])locate(out,owner,child(child(child(path,"categorical_dtypes"),i),"categories"),value.categorical_dtypes[i]->categories);}
void locate(Locations& out,const std::string& owner,Source& value){if(value.kind=="log")locate(out,owner,{},value.log);else if(value.kind=="stream"){locate(out,owner,{text("attributes")},value.stream.attributes);locate(out,owner,{text("metadata")},value.stream.metadata);for(std::size_t i=0;i<value.stream.events.size();++i)locate(out,owner,child(child(Array{text("events")},i),"fields"),value.stream.events[i].fields);}else locate(out,owner,value.kind=="lazy"?Array{text("table")}:Array{},value.table);}
void locate(Locations& out,const std::string& owner,Array path,EditDistanceTypedMove& value){if(value.log_activity)locate(out,owner,child(path,"log_activity"),*value.log_activity);if(value.model_activity)locate(out,owner,child(path,"model_activity"),*value.model_activity);}
void locate(Locations& out,const std::string& owner,Array path,std::vector<EditDistanceTypedMove>& values){for(std::size_t i=0;i<values.size();++i)locate(out,owner,child(path,i),values[i]);}
void locate(Locations& out,const std::string& owner,Array path,EditDistanceTypedAlignment& value){locate(out,owner,child(path,"source_variant"),value.source_variant);locate(out,owner,child(path,"target_variant"),value.target_variant);locate(out,owner,child(path,"alignment"),value.alignment);locate(out,owner,child(path,"moves"),value.moves);}
Json location_json(const Location& value){return Json{Object{{"owner",text(value.owner)},{"path",Json{value.path}}}};}
Json identity_classes(const Locations& values){std::vector<std::uint64_t> identities;Array groups;for(const auto& location:values){const auto identity=location.value->typed_value.scalar_identity();const auto found=std::find(identities.begin(),identities.end(),identity);const auto position=static_cast<std::size_t>(found-identities.begin());if(found==identities.end()){identities.push_back(identity);const auto* d=std::get_if<double>(&location.value->typed_value);groups.push_back(Json{Object{{"locations",Json{Array{location_json(location)}}},{"typed_index",count(location.value->typed_value.index())},{"is_nan",flag(d&&std::isnan(*d))}}});}else std::get<Array>(std::get<Object>(groups[position].value).at("locations").value).push_back(location_json(location));}return Json{std::move(groups)};}
Location resolve(const Locations& values,const Json& pointer){const auto& object=pointer.object();allowed_fields(object,{"owner","path"});const auto owner=required(object,"owner").string();const auto& path=required(object,"path").array();for(const auto& step:path){if(std::holds_alternative<std::string>(step.value))continue;(void)native_unsigned<std::size_t>(step);}const auto key=json_text(Json{path});for(const auto& v:values)if(v.owner==owner&&json_text(Json{v.path})==key)return v;invalid("copy location does not name an existing constructed EventAttribute");}

struct State {
 std::optional<Source> source,reference;std::optional<Trace> trace;std::vector<std::string> variant;std::vector<EventAttribute> typed_variant;std::vector<EditDistanceAlignment> alignments;std::vector<EditDistanceTypedAlignment> typed_alignments;std::optional<Direct> direct;bool typed=false;std::set<std::string> ready;
 Locations locations(){Locations out;if(source&&ready.contains("source_file"))locate(out,"source",*source);if(reference&&ready.contains("reference_file"))locate(out,"reference",*reference);if(trace)locate(out,"trace",{},*trace);locate(out,"variant",{},typed_variant);for(std::size_t i=0;i<typed_alignments.size();++i)locate(out,"alignments",Array{count(i)},typed_alignments[i]);if(direct){for(std::size_t i=0;i<direct->typed_mapping.size();++i)locate(out,"mapping",Array{count(i),count(0)},direct->typed_mapping[i].first);for(std::size_t i=0;i<direct->handles.size();++i)if(direct->handles[i].second){if(auto* result=std::any_cast<EditDistanceTypedTraceResult>(direct->handles[i].second.get()))locate(out,"handles",Array{count(i),text("value"),text("alignment")},result->alignment);}}return out;}
 Json snapshot(){Object out;if(source&&ready.contains("source_file"))out.emplace("source",source->json());if(reference&&ready.contains("reference_file"))out.emplace("reference",reference->json());if(trace)out.emplace("trace",trace_json(*trace));if(ready.contains("variant_file"))out.emplace("variant",typed?attributes_json(typed_variant):strings(variant));if(ready.contains("alignments_file"))out.emplace("alignments",typed?alignments_json(typed_alignments):alignments_json(alignments));if(direct&&ready.contains("direct_input_file"))out.emplace("direct_input",direct->json(typed));return Json{std::move(out)};}
 void refresh(){if(source&&source->kind=="lazy")source->refresh_lazy();if(reference&&reference->kind=="lazy")reference->refresh_lazy();}
};
struct Spec{std::vector<std::string> roles;bool typed=false,view=false;std::string result;};
const std::map<std::string,Spec,std::less<>> specs={
 {"align",{{"source_file","reference_file"},false,false,"EditDistanceAlignment[]"}},
 {"align_view",{{"source_file","reference_file"},false,true,"EditDistanceAlignment[]"}},
 {"align_typed",{{"source_file","reference_file"},true,false,"EditDistanceTypedAlignment[]"}},
 {"align_typed_view",{{"source_file","reference_file"},true,true,"EditDistanceTypedAlignment[]"}},
 {"trace",{{"trace_file","direct_input_file"},false,false,"EditDistanceTraceHandle"}},
 {"trace_typed",{{"trace_file","direct_input_file"},true,false,"EditDistanceTraceHandle"}},
 {"project_variant",{{"source_file","variant_file"},false,false,"EventLog"}},
 {"project_variant_view",{{"source_file","variant_file"},false,true,"EventLog"}},
 {"project_typed_variant",{{"source_file","variant_file"},true,false,"EventLog"}},
 {"project_typed_variant_view",{{"source_file","variant_file"},true,true,"EventLog"}},
 {"project_alignments_default",{{"source_file","alignments_file"},false,false,"EventLog"}},
 {"project_alignments",{{"source_file","alignments_file"},false,false,"EventLog"}},
 {"project_typed_alignments",{{"source_file","alignments_file"},true,false,"EventLog"}}
};
std::string canonical(const std::string& operation){if(operation=="trace")return "apply_edit_distance_trace";if(operation=="trace_typed")return "apply_typed_edit_distance_trace";if(operation.starts_with("align"))return operation.find("typed")!=std::string::npos?"align_typed_edit_distance":"align_edit_distance";if(operation.starts_with("project_typed_variant"))return "project_log_on_typed_variant";if(operation.starts_with("project_variant"))return "project_log_on_variant";return operation=="project_typed_alignments"?"project_log_on_typed_alignments":"project_log_on_alignments";}
bool alias_allowed(const std::string& operation,const std::string& alias){if(alias==canonical(operation))return true;if(operation=="align"||operation=="align_view")return alias=="apply_edit_distance"||alias=="apply";if(operation=="align_typed"||operation=="align_typed_view")return alias=="apply_typed_edit_distance";if(operation=="trace")return alias=="apply_trace"||alias=="align_trace"||alias=="align_edit_distance_trace";if(operation=="trace_typed")return alias=="apply_typed_trace"||alias=="align_typed_trace";return false;}
template<class SourceType>std::vector<EditDistanceAlignment> string_align(const SourceType& source,const SourceType& target,const std::string& alias,EditDistanceOptions options){if(alias=="apply_edit_distance")return apply_edit_distance(source,target,options);if(alias=="apply")return apply(source,target,options);return align_edit_distance(source,target,options);}
template<class SourceType>std::vector<EditDistanceTypedAlignment> typed_align(const SourceType& source,const SourceType& target,const std::string& alias,EditDistanceOptions options){if(alias=="apply_typed_edit_distance")return apply_typed_edit_distance(source,target,options);return align_typed_edit_distance(source,target,options);}
} // namespace

EditDistanceOptions parse_edit_distance_workbench_options_json(std::string_view input,std::set<std::string>* supplied){const auto doc=Parser(input).parse();const auto result=options_value(doc);supplied_members(doc.object(),supplied);return result;}
std::string serialize_edit_distance_workbench_options_json(const EditDistanceOptions& options){return json_text(options_json(options));}
std::vector<EditDistanceAlignment> parse_edit_distance_workbench_alignments_json(std::string_view input){return vector_value<EditDistanceAlignment>(Parser(input).parse(),alignment_value<EditDistanceAlignment>);}
std::vector<EditDistanceTypedAlignment> parse_edit_distance_workbench_typed_alignments_json(std::string_view input){return vector_value<EditDistanceTypedAlignment>(Parser(input).parse(),alignment_value<EditDistanceTypedAlignment>);}
std::string serialize_edit_distance_workbench_alignments_json(const std::vector<EditDistanceAlignment>& values){return json_text(alignments_json(values));}
std::string serialize_edit_distance_workbench_alignments_json(const std::vector<EditDistanceTypedAlignment>& values){return json_text(alignments_json(values));}

EditDistanceWorkbenchResponse execute_edit_distance_workbench(const std::vector<EditDistanceWorkbenchPart>& submitted){
 Array originals,parsed,calls,artifacts,artifact_errors,option_presence,copy_steps;Parts parts;State state;EditDistanceOptions options;Spec spec;std::string operation,alias,phase="configuration_error";Json configured=nil(),context=Json{Object{{"source",nil()},{"reference",nil()}}},before=Json{Object{}},after=Json{Object{}},after_native=Json{Object{}},copy_plan=nil(),identity_before=Json{Array{}},identity_after=Json{Array{}},result=nil(),failure=nil();bool executed=false,returned=false;int status=200;std::optional<EventLog> projected;std::vector<EditDistanceAlignment> string_result;std::vector<EditDistanceTypedAlignment> typed_result;EditDistanceTraceHandle trace_result;
 for(const auto& p:submitted)originals.push_back(Json{Object{{"field",byte_string(p.name)},{"filename",p.file?byte_string(p.filename):nil()},{"original",bytes(p.name,p.bytes)}}});
 const auto remember=[&](std::string name,const auto& make){try{const std::string value=make();artifacts.push_back(bytes(name,value));}catch(const std::exception& e){artifact_errors.push_back(Json{Object{{"name",text(std::move(name))},{"error",exception("export_error",e)}}});}};
 const auto snapshot=[&](std::string name,Json& destination){try{destination=state.snapshot();}catch(const std::exception& e){artifact_errors.push_back(Json{Object{{"name",text(std::move(name))},{"error",exception("snapshot_error",e)}}});}};
 try{
  static const std::set<std::string> global={"operation","function_alias","options_json","construction_copies_json"};static const std::set<std::string> files={"source_file","reference_file","trace_file","variant_file","alignments_file","direct_input_file"};static const std::set<std::string> role_suffixes={"_format","_shape","_import_options_json","_selectors_json","_preparation_options_json"};
  for(const auto& p:submitted)if(!parts.emplace(p.name,&p).second)invalid("duplicate workbench control/upload "+p.name);
  operation=control(parts,"operation","align_typed_view");const auto found=specs.find(operation);if(found==specs.end())invalid("unknown edit-distance workbench operation");spec=found->second;state.typed=spec.typed;if(part(parts,"operation"))parsed.push_back(text("operation"));alias=control(parts,"function_alias",canonical(operation));if(!alias_allowed(operation,alias))invalid("inapplicable public function alias");if(part(parts,"function_alias"))parsed.push_back(text("function_alias"));
  for(const auto& [name,p]:parts){if(global.contains(name))continue;if(files.contains(name)){if(std::find(spec.roles.begin(),spec.roles.end(),name)==spec.roles.end())invalid("inapplicable upload "+name);if(!p->file)invalid("native input role requires a multipart file "+name);continue;}bool allowed=false;for(const auto& role:{std::string("source"),std::string("reference")})if(std::find(spec.roles.begin(),spec.roles.end(),role+"_file")!=spec.roles.end())for(const auto& suffix:role_suffixes)if(name==role+suffix)allowed=true;if(!allowed)invalid("unknown/inapplicable workbench control "+name);}
  for(const auto& role:spec.roles)if(!part(parts,role))invalid("missing "+role);
  if(operation=="project_alignments_default"&&part(parts,"options_json"))invalid("two-argument project_log_on_alignments does not accept options_json");
  if(const auto* input=part(parts,"options_json")){std::set<std::string> supplied;options=parse_edit_distance_workbench_options_json(input->bytes,&supplied);for(const auto& name:supplied)option_presence.push_back(text(name));parsed.push_back(text("options_json"));}configured=options_json(options);
  phase="preparation_error";
  if(part(parts,"source_file")){state.source.emplace();read_source(*state.source,std::get<Object>(context.value).at("source"),"source",parts,parsed);state.ready.insert("source_file");}
  if(part(parts,"reference_file")){state.reference.emplace();read_source(*state.reference,std::get<Object>(context.value).at("reference"),"reference",parts,parsed);state.ready.insert("reference_file");}
  if(const auto* input=part(parts,"trace_file")){state.trace=trace_value(Parser(input->bytes).parse());parsed.push_back(text("trace_file"));state.ready.insert("trace_file");}
  if(const auto* input=part(parts,"variant_file")){const auto doc=Parser(input->bytes).parse();if(state.typed)state.typed_variant=attributes(doc);else state.variant=string_values(doc);parsed.push_back(text("variant_file"));state.ready.insert("variant_file");}
  if(const auto* input=part(parts,"alignments_file")){if(state.typed)state.typed_alignments=parse_edit_distance_workbench_typed_alignments_json(input->bytes);else state.alignments=parse_edit_distance_workbench_alignments_json(input->bytes);parsed.push_back(text("alignments_file"));state.ready.insert("alignments_file");}
  if(const auto* input=part(parts,"direct_input_file")){state.direct.emplace();read_direct(*state.direct,Parser(input->bytes).parse(),state.typed);parsed.push_back(text("direct_input_file"));state.ready.insert("direct_input_file");}
  phase="construction_error";snapshot("inputs_before_copies",before);identity_before=identity_classes(state.locations());
  if(const auto* input=part(parts,"construction_copies_json")){copy_plan=Parser(input->bytes).parse();const auto& plan=copy_plan.array();for(std::size_t i=0;i<plan.size();++i){const auto& instruction=plan[i].object();allowed_fields(instruction,{"from","to","member"});const auto member=required(instruction,"member").string();if(member!="attribute"&&member!="typed_value")invalid("unknown construction-copy member");const auto locations=state.locations();const EventAttribute source=*resolve(locations,required(instruction,"from")).value;auto* destination=resolve(locations,required(instruction,"to")).value;if(member=="attribute")*destination=source;else destination->typed_value=source.typed_value;state.refresh();copy_steps.push_back(Json{Object{{"instruction",count(i)},{"classes",identity_classes(state.locations())}}});}parsed.push_back(text("construction_copies_json"));}
  snapshot("inputs_after_copies",after);phase="construction_error";
  if(!spec.view){if(state.source)(void)state.source->require_log();if(state.reference)(void)state.reference->require_log();}phase="native_error";
  const auto signature=alias+(spec.view?"(EventDataView":"(EventLog");std::string call_name;
  if(operation.starts_with("align"))call_name=signature+(spec.view?",EventDataView,options)":",EventLog,options)");else if(operation.starts_with("trace"))call_name=alias+"(Trace,list,set,mapping,cache,options)";else if(operation=="project_alignments_default")call_name=alias+"(EventLog,alignments)";else call_name=signature+((operation.find("variant")!=std::string::npos)?",variant,options)":",alignments,options)");
  executed=true;
  try{
   if(operation=="align"||operation=="align_view"){if(spec.view)string_result=string_align(state.source->view(),state.reference->view(),alias,options);else string_result=string_align(state.source->require_log(),state.reference->require_log(),alias,options);returned=true;result=alignments_json(string_result);}
   else if(operation=="align_typed"||operation=="align_typed_view"){if(spec.view)typed_result=typed_align(state.source->view(),state.reference->view(),alias,options);else typed_result=typed_align(state.source->require_log(),state.reference->require_log(),alias,options);returned=true;result=alignments_json(typed_result);}
   else if(operation=="trace"||operation=="trace_typed"){auto& d=*state.direct;auto* cache=d.cache?&*d.cache:nullptr;if(operation=="trace"){if(alias=="apply_trace")trace_result=apply_trace(*state.trace,d.list,d.set,d.mapping,cache,options);else if(alias=="align_trace")trace_result=align_trace(*state.trace,d.list,d.set,d.mapping,cache,options);else if(alias=="align_edit_distance_trace")trace_result=align_edit_distance_trace(*state.trace,d.list,d.set,d.mapping,cache,options);else trace_result=apply_edit_distance_trace(*state.trace,d.list,d.set,d.mapping,cache,options);}else{if(alias=="apply_typed_trace")trace_result=apply_typed_trace(*state.trace,d.list,d.set,d.typed_mapping,cache,options);else if(alias=="align_typed_trace")trace_result=align_typed_trace(*state.trace,d.list,d.set,d.typed_mapping,cache,options);else trace_result=apply_typed_edit_distance_trace(*state.trace,d.list,d.set,d.typed_mapping,cache,options);}returned=true;result=d.handle(trace_result);}
   else{if(operation=="project_variant")projected=project_log_on_variant(state.source->require_log(),state.variant,options);else if(operation=="project_variant_view")projected=project_log_on_variant(state.source->view(),state.variant,options);else if(operation=="project_typed_variant")projected=project_log_on_typed_variant(state.source->require_log(),state.typed_variant,options);else if(operation=="project_typed_variant_view")projected=project_log_on_typed_variant(state.source->view(),state.typed_variant,options);else if(operation=="project_alignments_default")projected=project_log_on_alignments(state.source->require_log(),state.alignments);else if(operation=="project_alignments")projected=project_log_on_alignments(state.source->require_log(),state.alignments,options);else projected=project_log_on_typed_alignments(state.source->require_log(),state.typed_alignments,options);returned=true;result=document(serialize_embedding_workbench_raw_log_json(*projected));}
   calls.push_back(Json{Object{{"name",text(call_name)},{"returned",flag(true)},{"error",nil()}}});
  }catch(const std::exception& e){calls.push_back(Json{Object{{"name",text(call_name)},{"returned",flag(returned)},{"error",exception(returned?"result_serialization_error":"native_error",e)}}});throw;}
 }catch(const std::exception& e){status=returned?200:400;failure=exception(phase,e);}
 if(before.object().empty())snapshot("inputs_before_copies",before);if(after.object().empty())snapshot("inputs_after_copies",after);snapshot("inputs_after_native",after_native);try{auto locations=state.locations();if(returned){for(std::size_t i=0;i<typed_result.size();++i)locate(locations,"native_result",Array{count(i)},typed_result[i]);if(projected)locate(locations,"native_result",{},*projected);if(trace_result)if(auto* typed=std::any_cast<EditDistanceTypedTraceResult>(trace_result.get()))locate(locations,"native_result",Array{text("value"),text("alignment")},typed->alignment);}identity_after=identity_classes(locations);}catch(const std::exception& e){artifact_errors.push_back(Json{Object{{"name",text("identity_proof")},{"error",exception("snapshot_error",e)}}});}
 if(!configured.null())remember("configured_options.json",[&]{return json_text(configured);});remember("inputs_before_copies.json",[&]{return json_text(before);});remember("inputs_after_copies.json",[&]{return json_text(after);});remember("inputs_after_native.json",[&]{return json_text(after_native);});
 if(state.source&&state.ready.contains("source_file")){remember("source_constructor.json",[&]{return json_text(state.source->json());});if(state.source->kind=="log")remember("source.xes",[&]{return serialize_xes(state.source->log);});}
 if(state.reference&&state.ready.contains("reference_file")){remember("reference_constructor.json",[&]{return json_text(state.reference->json());});if(state.reference->kind=="log")remember("reference.xes",[&]{return serialize_xes(state.reference->log);});}
 if(returned){remember("native_result.json",[&]{return json_text(result);});if(spec.result=="EditDistanceAlignment[]")remember("legacy_alignment.json",[&]{return edit_distance_json(string_result,options.perform_anti_alignment);});else if(spec.result=="EditDistanceTypedAlignment[]")remember("legacy_typed_alignment.json",[&]{return typed_edit_distance_json(typed_result,options.perform_anti_alignment);});else if(projected)remember("projected.xes",[&]{return serialize_xes(*projected);});}
 Array boundaries{text("Finite real std::any constructors only; arbitrary caller-defined C++ payloads unavailable."),text("Intentional scalar identity uses actual public copy assignment within this request."),text("Lazy scanner adapter owns finite rows; arbitrary executable scanner functions unavailable."),text("Invalid lazy sources use actual default native invalid state only; the public nondefault constructor rejects an empty scanner.")};
 Object envelope{{"family",text("ORIGINAL_EDIT_DISTANCE_WORKBENCH")},{"version",text("original-edit-distance-workbench-v1")},{"operation",byte_string(operation)},{"function_alias",byte_string(alias)},{"originals",Json{std::move(originals)}},{"parsed_controls",Json{std::move(parsed)}},{"configured_options",std::move(configured)},{"options_supplied",Json{std::move(option_presence)}},{"input_context",std::move(context)},{"inputs_before_copies",std::move(before)},{"inputs_after_copies",std::move(after)},{"inputs_after_native",std::move(after_native)},{"construction_copies",std::move(copy_plan)},{"identity_proof",Json{Object{{"before",std::move(identity_before)},{"steps",Json{std::move(copy_steps)}},{"after_native",std::move(identity_after)}}}},{"native_calls",Json{std::move(calls)}},{"executed",flag(executed)},{"returned",flag(returned)},{"result_type",spec.result.empty()?nil():text(spec.result)},{"native_result",std::move(result)},{"error",std::move(failure)},{"artifacts",Json{std::move(artifacts)}},{"artifact_errors",Json{std::move(artifact_errors)}},{"boundaries",Json{std::move(boundaries)}}};
 return {status,json_text(Json{std::move(envelope)})};
}

namespace {
std::string mime_lower(std::string_view value){std::string out(value);std::transform(out.begin(),out.end(),out.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});return out;}
std::string mime_trim(std::string_view value){auto begin=value.find_first_not_of(" \t"),end=value.find_last_not_of(" \t");return begin==std::string_view::npos?std::string{}:std::string(value.substr(begin,end-begin+1));}
struct MimeParameter {std::string name,value;};
std::vector<MimeParameter> mime_parameters(std::string_view value){std::vector<MimeParameter> out;std::size_t i=0;while(i<value.size()){while(i<value.size()&&(value[i]==' '||value[i]=='\t'||value[i]==';'))++i;if(i==value.size())break;const auto begin=i;while(i<value.size()&&value[i]!='='&&value[i]!=';')++i;auto name=mime_lower(mime_trim(value.substr(begin,i-begin)));if(name.empty())invalid("empty MIME parameter");std::string item;if(i==value.size()||value[i]!='=')invalid("MIME parameter requires equals");if(i<value.size()&&value[i]=='='){++i;while(i<value.size()&&(value[i]==' '||value[i]=='\t'))++i;if(i<value.size()&&value[i]=='"'){++i;bool closed=false;while(i<value.size()){const char c=value[i++];if(c=='"'){closed=true;break;}if(c=='\\'){if(i==value.size())invalid("short MIME quoted escape");item+=value[i++];}else item+=c;}if(!closed)invalid("unterminated MIME quoted parameter");while(i<value.size()&&(value[i]==' '||value[i]=='\t'))++i;if(i<value.size()&&value[i]!=';')invalid("data after MIME quoted parameter");}else{const auto start=i;while(i<value.size()&&value[i]!=';')++i;item=mime_trim(value.substr(start,i-start));}}out.push_back({std::move(name),std::move(item)});if(i<value.size()&&value[i]==';')++i;}return out;}
std::string mime_percent(std::string_view value){std::string out;for(std::size_t i=0;i<value.size();++i){if(value[i]!='%'){out+=value[i];continue;}if(i+2>=value.size())invalid("short extended MIME filename escape");out+=unhex_bytes(value.substr(i+1,2));i+=2;}if(!scalar_utf8(out))invalid("extended MIME filename is not UTF-8");return out;}
std::string mime_boundary(std::string_view content_type){const auto semi=content_type.find(';');if(mime_lower(mime_trim(content_type.substr(0,semi)))!="multipart/form-data")invalid("edit-distance workbench requires multipart/form-data");const auto params=semi==std::string_view::npos?std::vector<MimeParameter>{}:mime_parameters(content_type.substr(semi+1));std::optional<std::string> boundary;for(const auto& p:params)if(p.name=="boundary"){if(boundary)invalid("duplicate multipart boundary");boundary=p.value;}if(!boundary||boundary->empty()||boundary->size()>70||boundary->back()==' '||!std::all_of(boundary->begin(),boundary->end(),[](unsigned char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||std::string_view("'()+_,-./:=? ").find(static_cast<char>(c))!=std::string_view::npos;}))invalid("invalid multipart boundary");return *boundary;}
}

std::vector<EditDistanceWorkbenchPart> parse_edit_distance_workbench_multipart(std::string_view content_type,std::string_view body){
 const auto boundary=mime_boundary(content_type);const auto separator="--"+boundary;const auto marker="\r\n"+separator;if(!body.starts_with(separator))invalid("multipart body does not start with its boundary");std::size_t cursor=separator.size();std::vector<EditDistanceWorkbenchPart> out;
 while(true){if(body.substr(cursor,2)=="--"){cursor+=2;if(body.substr(cursor,2)=="\r\n")cursor+=2;if(cursor!=body.size())invalid("unexpected multipart epilogue");return out;}if(body.substr(cursor,2)!="\r\n")invalid("invalid multipart boundary suffix");cursor+=2;const auto headers_end=body.find("\r\n\r\n",cursor);if(headers_end==std::string_view::npos||headers_end-cursor>64u*1024u)invalid("missing/oversized multipart headers");std::optional<std::string> disposition;for(auto line_begin=cursor;line_begin<headers_end;){auto line_end=body.find("\r\n",line_begin);if(line_end==std::string_view::npos||line_end>headers_end)line_end=headers_end;const auto line=body.substr(line_begin,line_end-line_begin);const auto colon=line.find(':');if(colon==std::string_view::npos||line.find_first_of("\r\n")!=std::string_view::npos)invalid("invalid multipart header line");const auto name=mime_lower(mime_trim(line.substr(0,colon)));if(name=="content-disposition"){if(disposition)invalid("duplicate multipart Content-Disposition");disposition=mime_trim(line.substr(colon+1));}line_begin=line_end+2;}if(!disposition)invalid("missing multipart Content-Disposition");const auto semi=disposition->find(';');if(mime_lower(mime_trim(std::string_view(*disposition).substr(0,semi)))!="form-data")invalid("multipart disposition must be form-data");const auto params=semi==std::string::npos?std::vector<MimeParameter>{}:mime_parameters(std::string_view(*disposition).substr(semi+1));std::optional<std::string> name,filename,extended;for(const auto& p:params){if(p.name=="name"){if(name)invalid("duplicate multipart part name");name=p.value;}else if(p.name=="filename"){if(filename)invalid("duplicate multipart filename");filename=p.value;}else if(p.name=="filename*"){if(extended)invalid("duplicate extended multipart filename");if(!p.value.starts_with("UTF-8''"))invalid("extended MIME filename requires UTF-8");extended=mime_percent(std::string_view(p.value).substr(7));}}if(!name)invalid("missing multipart part name");const auto data_begin=headers_end+4;auto next=body.find(marker,data_begin);while(next!=std::string_view::npos){const auto suffix=next+marker.size();if(body.substr(suffix,2)=="\r\n"||(body.substr(suffix,2)=="--"&&(suffix+2==body.size()||body.substr(suffix+2,2)=="\r\n")))break;next=body.find(marker,next+1);}if(next==std::string_view::npos)invalid("missing final multipart boundary");out.push_back({std::move(*name),extended?std::move(*extended):filename?std::move(*filename):std::string{},std::string(body.substr(data_begin,next-data_begin)),filename.has_value()||extended.has_value()});if(out.size()>1000)invalid("multipart body exceeds 1000 parts");cursor=next+marker.size();}
}

EditDistanceWorkbenchResponse execute_edit_distance_workbench_multipart(std::string_view content_type,std::string_view body,const std::vector<EditDistanceWorkbenchPart>& query,bool transport_complete){
 EditDistanceWorkbenchResponse response;try{if(!transport_complete)invalid("multipart stream read did not complete");auto parts=query;auto form=parse_edit_distance_workbench_multipart(content_type,body);parts.insert(parts.end(),std::make_move_iterator(form.begin()),std::make_move_iterator(form.end()));response=execute_edit_distance_workbench(parts);}catch(const std::exception& error){response=execute_edit_distance_workbench({});auto envelope=document(response.json);auto& object=std::get<Object>(envelope.value);object["error"]=exception("transport_error",error);Array originals;for(const auto& p:query)originals.push_back(Json{Object{{"field",byte_string(p.name)},{"filename",nil()},{"original",bytes(p.name,p.bytes)}}});object["originals"]=Json{std::move(originals)};object["operation"]=byte_string(control([&]{Parts parts;for(const auto& p:query)parts.emplace(p.name,&p);return parts;}(),"operation","align_typed_view"));response.status=400;response.json=json_text(envelope);}
 auto envelope=document(response.json);auto& object=std::get<Object>(envelope.value);object["transport_original"]=bytes("multipart_body",body);object["content_type_original"]=bytes("Content-Type",content_type);response.json=json_text(envelope);return response;
}

} // namespace minera
