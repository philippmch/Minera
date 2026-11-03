#include "minera/bose_workbench_json.hpp"
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
  throw std::invalid_argument("BOSE workbench JSON: " + message);
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
EventAttribute attribute(const Json& value){const auto& o=value.object();if(field(o,"native_byte_strings")){allowed_fields(o,{"native_byte_strings","value"});if(required(o,"native_byte_strings").string()!="hex-v1")invalid("unknown attribute byte wrapper");auto result=parse_embedding_workbench_raw_log_json("{\"native_byte_strings\":\"hex-v1\",\"value\":{\"attributes\":["+json_text(required(o,"value"))+"]}}");return std::move(result.attributes.front());}auto result=parse_embedding_workbench_raw_log_json("{\"attributes\":["+json_text(value)+"]}");return std::move(result.attributes.front());}
Json attribute_json(const EventAttribute& value){EventLog holder;holder.attributes.push_back(value);const auto result=document(serialize_embedding_workbench_raw_log_json(holder));if(const auto* schema=field(result.object(),"native_byte_strings")){return Json{Object{{"native_byte_strings",*schema},{"value",required(required(result.object(),"value").object(),"attributes").array().front()}}};}return required(result.object(),"attributes").array().front();}
std::vector<EventAttribute> attributes(const Json& value){return vector_value<EventAttribute>(value,attribute);}
Json attributes_json(const std::vector<EventAttribute>& value){Array out;for(const auto& v:value)out.push_back(attribute_json(v));return Json{std::move(out)};}
Trace trace_value(const Json& value){const auto& o=value.object();if(field(o,"native_byte_strings")){allowed_fields(o,{"native_byte_strings","value"});if(required(o,"native_byte_strings").string()!="hex-v1")invalid("unknown trace byte wrapper");return parse_embedding_workbench_raw_log_json("{\"native_byte_strings\":\"hex-v1\",\"value\":{\"traces\":["+json_text(required(o,"value"))+"]}}").traces.front();}return parse_embedding_workbench_raw_log_json("{\"traces\":["+json_text(value)+"]}").traces.front();}
Json trace_json(const Trace& trace){EventLog holder;holder.traces.push_back(trace);auto result=document(serialize_embedding_workbench_raw_log_json(holder));if(const auto* schema=field(result.object(),"native_byte_strings"))return Json{Object{{"native_byte_strings",*schema},{"value",required(required(result.object(),"value").object(),"traces").array().front()}}};return required(result.object(),"traces").array().front();}
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

using Parts=std::map<std::string,const BoseWorkbenchPart*,std::less<>>;
const BoseWorkbenchPart* part(const Parts& parts,std::string_view key){const auto it=parts.find(key);return it==parts.end()?nullptr:it->second;}
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
Json location_json(const Location& value){return Json{Object{{"owner",text(value.owner)},{"path",Json{value.path}}}};}
Json identity_classes(const Locations& values){std::vector<std::uint64_t> identities;Array groups;for(const auto& location:values){const auto identity=location.value->typed_value.scalar_identity();const auto found=std::find(identities.begin(),identities.end(),identity);const auto position=static_cast<std::size_t>(found-identities.begin());if(found==identities.end()){identities.push_back(identity);const auto* d=std::get_if<double>(&location.value->typed_value);groups.push_back(Json{Object{{"locations",Json{Array{location_json(location)}}},{"typed_index",count(location.value->typed_value.index())},{"is_nan",flag(d&&std::isnan(*d))}}});}else std::get<Array>(std::get<Object>(groups[position].value).at("locations").value).push_back(location_json(location));}return Json{std::move(groups)};}
Location resolve(const Locations& values,const Json& pointer){const auto& object=pointer.object();allowed_fields(object,{"owner","path"});const auto owner=required(object,"owner").string();const auto& path=required(object,"path").array();for(const auto& step:path){if(std::holds_alternative<std::string>(step.value))continue;(void)native_unsigned<std::size_t>(step);}const auto key=json_text(Json{path});for(const auto& v:values)if(v.owner==owner&&json_text(Json{v.path})==key)return v;invalid("copy location does not name an existing constructed EventAttribute");}


template<class T>T trace_options(const Json& value){const auto& o=value.object();
 if constexpr(std::is_same_v<T,ConceptDriftTableOptions>)allowed_fields(o,{"sub_log_size","window_size","num_permutations","threshold","max_no_change_points","seed","activity_attribute","timestamp_attribute","case_id_attribute"});
 else allowed_fields(o,{"sub_log_size","window_size","num_permutations","threshold","max_no_change_points","seed"});
 T v;if(auto* x=field(o,"sub_log_size"))v.sub_log_size=int64(*x);if(auto* x=field(o,"window_size"))v.window_size=int64(*x);if(auto* x=field(o,"num_permutations"))v.num_permutations=int64(*x);if(auto* x=field(o,"threshold"))v.threshold=real_value(*x);if(auto* x=field(o,"max_no_change_points"))v.max_no_change_points=int64(*x);if(auto* x=field(o,"seed"))v.seed=unsigned_integer(*x);
 if constexpr(std::is_same_v<T,ConceptDriftTableOptions>){if(auto* x=field(o,"activity_attribute"))v.activity_attribute=native_string(*x);if(auto* x=field(o,"timestamp_attribute"))v.timestamp_attribute=native_string(*x);if(auto* x=field(o,"case_id_attribute"))v.case_id_attribute=native_string(*x);}return v;}
ConceptDriftOptions log_options(const Json& value){const auto& o=value.object();allowed_fields(o,{"variant","sub_log_size","window_size","num_permutations","threshold","max_no_change_points","seed","include_timestamps","activity_attribute","timestamp_attribute","case_id_attribute"});ConceptDriftOptions v;
 if(auto* x=field(o,"variant"))v.variant=static_cast<ConceptDriftVariant>(enum_index(*x,"BOSE"));if(auto* x=field(o,"sub_log_size"))v.sub_log_size=native_unsigned<std::size_t>(*x);if(auto* x=field(o,"window_size"))v.window_size=native_unsigned<std::size_t>(*x);if(auto* x=field(o,"num_permutations"))v.num_permutations=native_unsigned<std::size_t>(*x);if(auto* x=field(o,"threshold"))v.threshold=real_value(*x);if(auto* x=field(o,"max_no_change_points"))v.max_no_change_points=native_unsigned<std::size_t>(*x);if(auto* x=field(o,"seed"))v.seed=unsigned_integer(*x);if(auto* x=field(o,"include_timestamps"))v.include_timestamps=x->boolean();if(auto* x=field(o,"activity_attribute"))v.activity_attribute=native_string(*x);if(auto* x=field(o,"timestamp_attribute"))v.timestamp_attribute=native_string(*x);if(auto* x=field(o,"case_id_attribute"))v.case_id_attribute=native_string(*x);return v;}
template<class T>Json options_json(const T& v){Object o{{"sub_log_size",signed_count(v.sub_log_size)},{"window_size",signed_count(v.window_size)},{"num_permutations",signed_count(v.num_permutations)},{"threshold",real(v.threshold)},{"max_no_change_points",signed_count(v.max_no_change_points)},{"seed",count(v.seed)}};
 if constexpr(std::is_same_v<T,ConceptDriftOptions>){o["sub_log_size"]=count(v.sub_log_size);o["window_size"]=count(v.window_size);o["num_permutations"]=count(v.num_permutations);o["max_no_change_points"]=count(v.max_no_change_points);o.emplace("variant",enumeration(static_cast<std::int32_t>(v.variant),"BOSE"));o.emplace("include_timestamps",flag(v.include_timestamps));}
 if constexpr(!std::is_same_v<T,ConceptDriftTraceOptions>){o.emplace("activity_attribute",byte_string(v.activity_attribute));o.emplace("timestamp_attribute",byte_string(v.timestamp_attribute));o.emplace("case_id_attribute",byte_string(v.case_id_attribute));}return Json{std::move(o)};}
struct Parameters {ConceptDriftVariant variant=ConceptDriftVariant::kBose;std::string activity_attribute="concept:name";std::int64_t sub_log_size=50,num_permutations=100;bool keep_leftover=true;std::uint64_t seed=0;};
struct Spec {std::vector<std::string> roles;std::string options,result,function,kind;bool rng=false,typed=false;std::vector<std::string> parameters;};
const std::map<std::string,Spec,std::less<>> specs={
 {"table_apply",{{"source_file"},"table","ConceptDriftTableResult","apply_concept_drift_table","table",true,false,{}}},
 {"log_apply",{{"source_file"},"table","ConceptDriftTableResult","apply_concept_drift_log","log",true,false,{}}},
 {"dispatch_table",{{"source_file"},"table","ConceptDriftTableResult","apply_concept_drift","table",true,false,{"variant"}}},
 {"dispatch_log",{{"source_file"},"table","ConceptDriftTableResult","apply_concept_drift","log",true,false,{"variant"}}},
 {"activities_default",{{"source_file"},"","NativeString[]","extract_concept_drift_activities","log",false,false,{}}},
 {"activities",{{"source_file"},"","NativeString[]","extract_concept_drift_activities","log",false,false,{"activity_attribute"}}},
 {"typed_activities",{{"traces_file"},"","EventAttribute[]","extract_concept_drift_typed_activities","",false,true,{}}},
 {"features_traces_default",{{"traces_file","alphabet_file"},"","double[]","extract_concept_drift_features","",false,false,{}}},
 {"features_traces",{{"traces_file","alphabet_file"},"","double[]","extract_concept_drift_features","",false,false,{"activity_attribute"}}},
 {"features_log",{{"source_file"},"","double[]","extract_concept_drift_features","log",false,false,{}}},
 {"typed_features",{{"traces_file","alphabet_file"},"","double[]","extract_concept_drift_typed_features","",false,true,{}}},
 {"follows",{{"trace_file","alphabet_file"},"","pair<NativeString,NativeString>[]","compute_concept_drift_follows_relation","",false,false,{"activity_attribute"}}},
 {"typed_follows",{{"trace_file","alphabet_file"},"","pair<EventAttribute,EventAttribute>[]","compute_concept_drift_typed_follows_relation","",false,true,{}}},
 {"split_sublogs",{{"source_file"},"","EventLog[]","split_concept_drift_sublogs","log",false,false,{"sub_log_size","keep_leftover"}}},
 {"permutation",{{"first_file","second_file"},"","double","concept_drift_permutation_test","",true,false,{"num_permutations","seed"}}},
 {"detect_traces",{{"traces_file"},"trace","ConceptDriftTraceResult","detect_concept_drift_traces","",true,false,{}}},
 {"detect_typed_traces",{{"traces_file"},"trace","ConceptDriftTypedTraceResult","detect_concept_drift_typed_traces","",true,true,{}}},
 {"detect_log",{{"source_file"},"log","ConceptDriftResult","detect_concept_drift","log",false,false,{}}}
};
Json parameters_json(const Parameters& p,const Spec& spec){Object o;for(const auto& k:spec.parameters){if(k=="variant")o.emplace(k,enumeration(static_cast<std::int32_t>(p.variant),"BOSE"));else if(k=="activity_attribute")o.emplace(k,byte_string(p.activity_attribute));else if(k=="sub_log_size")o.emplace(k,signed_count(p.sub_log_size));else if(k=="keep_leftover")o.emplace(k,flag(p.keep_leftover));else if(k=="num_permutations")o.emplace(k,signed_count(p.num_permutations));else if(k=="seed")o.emplace(k,count(p.seed));}return o.empty()?nil():Json{std::move(o)};}
void read_parameters(Parameters& p,const Json& value,const Spec& spec){const auto& o=value.object();for(const auto& [k,v]:o){if(std::find(spec.parameters.begin(),spec.parameters.end(),k)==spec.parameters.end())invalid("inapplicable parameter "+k);if(k=="variant")p.variant=static_cast<ConceptDriftVariant>(enum_index(v,"BOSE"));else if(k=="activity_attribute")p.activity_attribute=native_string(v);else if(k=="sub_log_size")p.sub_log_size=int64(v);else if(k=="keep_leftover")p.keep_leftover=v.boolean();else if(k=="num_permutations")p.num_permutations=int64(v);else p.seed=unsigned_integer(v);}}
struct Rng {bool applicable=false,caller_requested=false;std::optional<std::mt19937> engine;Json constructor=nil();std::optional<std::string> before,after;
 void read(const Json& value){constructor=value;const auto& o=value.object();const auto name=required(o,"constructor").string();std::mt19937 next;
  if(name=="seed32"){allowed_fields(o,{"constructor","seed"});next.seed(native_unsigned<std::uint32_t>(required(o,"seed")));}
  else if(name=="seed_seq_words"){allowed_fields(o,{"constructor","words"});const auto words=vector_value<std::uint32_t>(required(o,"words"),[](const Json& v){return native_unsigned<std::uint32_t>(v);});std::seed_seq sequence(words.begin(),words.end());next.seed(sequence);}
  else if(name=="state_text"){allowed_fields(o,{"constructor","state"});const auto original=native_string(required(o,"state"));std::istringstream input(original);input.imbue(std::locale::classic());if(!(input>>next))invalid("invalid public mt19937 state text");input>>std::ws;if(!input.eof())invalid("trailing data after public mt19937 state text");}
  else invalid("unknown mt19937 constructor");constructor=value;engine=std::move(next);before=state();after=before;}
 std::string state()const{auto out=output();out<<*engine;return out.str();}
 void finish(){if(engine)after=state();}
 Json json()const{return Json{Object{{"applicable",flag(applicable)},{"mode",text(!applicable?"not_applicable":caller_requested?"caller_mt19937":"native_seeded")},{"constructor",constructor},{"before",before?byte_string(*before):nil()},{"after",after?byte_string(*after):nil()},{"options_seed_active",flag(applicable&&!caller_requested)}}};}
};
Json reals(const std::vector<double>& values){Array out;for(const auto v:values)out.push_back(real(v));return Json{std::move(out)};}
Json matrix_json(const std::vector<std::vector<double>>& values){Array out;for(const auto& row:values)out.push_back(reals(row));return Json{std::move(out)};}
template<class T,class F>Json nested_json(const std::vector<T>& values,F write){Array out;for(const auto& value:values)out.push_back(write(value));return Json{std::move(out)};}
Json logs_json(const std::vector<EventLog>& values){return nested_json(values,[](const auto& v){return document(serialize_embedding_workbench_raw_log_json(v));});}
Json tables_json(const std::vector<EventTable>& values){return nested_json(values,[](const auto& v){return document(serialize_embedding_workbench_table_json(v));});}
template<class T>Json changes_json(const std::vector<T>& values){Array out;for(const auto& v:values){Json index;if constexpr(std::is_same_v<T,ConceptDriftTraceChangePoint>)index=signed_count(v.sublog_index);else index=count(v.sublog_index);out.push_back(Json{Object{{"sublog_index",std::move(index)},{"p_value",real(v.p_value)}}});}return Json{std::move(out)};}
Json table_result_json(const ConceptDriftTableResult& v){return Json{Object{{"returned_tables",tables_json(v.returned_tables)},{"change_timestamps",reals(v.change_timestamps)},{"p_values",reals(v.p_values)}}};}
Json trace_result_json(const ConceptDriftTraceResult& v){return Json{Object{{"sublogs",nested_json(v.sublogs,[](const auto& sub){return nested_json(sub,strings);})},{"change_points",changes_json(v.change_points)}}};}
Json typed_result_json(const ConceptDriftTypedTraceResult& v){return Json{Object{{"sublogs",nested_json(v.sublogs,[](const auto& sub){return nested_json(sub,attributes_json);})},{"change_points",changes_json(v.change_points)}}};}
Json log_result_json(const ConceptDriftResult& v){return Json{Object{{"variant",enumeration(static_cast<std::int32_t>(v.variant),"BOSE")},{"activities",strings(v.activities)},{"feature_vectors",matrix_json(v.feature_vectors)},{"sublogs",logs_json(v.sublogs)},{"returned_sublogs",logs_json(v.returned_sublogs)},{"cumulative_sublogs",logs_json(v.cumulative_sublogs)},{"sublog_start_timestamps",reals(v.sublog_start_timestamps)},{"change_points",changes_json(v.change_points)},{"change_timestamps",reals(v.change_timestamps)},{"activity_attribute",byte_string(v.activity_attribute)},{"timestamp_attribute",byte_string(v.timestamp_attribute)},{"case_id_attribute",byte_string(v.case_id_attribute)}}};}
struct State {
 std::optional<Source> source;std::optional<Trace> trace;std::vector<Trace> traces;std::vector<EventAttribute> typed_trace,typed_alphabet;std::vector<std::vector<EventAttribute>> typed_traces;std::vector<std::string> alphabet;std::vector<std::vector<std::string>> string_traces;std::vector<std::vector<double>> first,second;std::string operation;bool typed=false;std::set<std::string> ready;
 Locations locations(){Locations out;if(source&&ready.contains("source_file"))locate(out,"source",*source);if(trace)locate(out,"trace",{},*trace);if(typed)locate(out,"trace",{},typed_trace);for(std::size_t i=0;i<traces.size();++i)locate(out,"traces",Array{count(i)},traces[i]);for(std::size_t i=0;i<typed_traces.size();++i)locate(out,"traces",Array{count(i)},typed_traces[i]);locate(out,"alphabet",{},typed_alphabet);return out;}
 Json snapshot(){Object out;if(source&&ready.contains("source_file"))out.emplace("source",source->json());if(ready.contains("trace_file"))out.emplace("trace",typed?attributes_json(typed_trace):trace_json(*trace));if(ready.contains("traces_file")){if(typed)out.emplace("traces",nested_json(typed_traces,attributes_json));else if(operation=="detect_traces")out.emplace("traces",nested_json(string_traces,strings));else out.emplace("traces",nested_json(traces,trace_json));}if(ready.contains("alphabet_file"))out.emplace("alphabet",typed?attributes_json(typed_alphabet):strings(alphabet));if(ready.contains("first_file"))out.emplace("first",matrix_json(first));if(ready.contains("second_file"))out.emplace("second",matrix_json(second));return Json{std::move(out)};}
 void refresh(){if(source&&source->kind=="lazy")source->refresh_lazy();}
};
} // namespace

BoseWorkbenchResponse execute_bose_workbench(const std::vector<BoseWorkbenchPart>& submitted){
 Array originals,parsed,calls,artifacts,artifact_errors,option_presence,parameter_presence,copy_steps;Parts parts;State state;Spec spec;Parameters parameters;Rng rng;ConceptDriftTableOptions table_options;ConceptDriftTraceOptions direct_options;ConceptDriftOptions whole_options;std::string operation,phase="configuration_error";Json configured=nil(),configured_parameters=nil(),context=Json{Object{{"source",nil()}}},before=Json{Object{}},after=Json{Object{}},after_native=Json{Object{}},copy_plan=nil(),identity_before=Json{Array{}},identity_after=Json{Array{}},result=nil(),failure=nil();bool executed=false,returned=false;int status=200;
 std::optional<ConceptDriftTableResult> table_result;std::optional<ConceptDriftTraceResult> trace_result;std::optional<ConceptDriftTypedTraceResult> typed_result;std::optional<ConceptDriftResult> log_result;std::vector<EventLog> split_result;std::vector<EventAttribute> activity_result;std::vector<std::pair<EventAttribute,EventAttribute>> typed_pairs;
 for(const auto& p:submitted)originals.push_back(Json{Object{{"field",byte_string(p.name)},{"filename",p.file?byte_string(p.filename):nil()},{"original",bytes(p.name,p.bytes)}}});
 const auto remember=[&](std::string name,const auto& make){try{const std::string value=make();artifacts.push_back(bytes(name,value));}catch(const std::exception& e){artifact_errors.push_back(Json{Object{{"name",text(std::move(name))},{"error",exception("export_error",e)}}});}};
 const auto snapshot=[&](std::string name,Json& destination){try{destination=state.snapshot();}catch(const std::exception& e){artifact_errors.push_back(Json{Object{{"name",text(std::move(name))},{"error",exception("snapshot_error",e)}}});}};
 try{
  static const std::set<std::string> global={"operation","options_json","parameters_json","rng_json","construction_copies_json"};static const std::set<std::string> files={"source_file","trace_file","traces_file","alphabet_file","first_file","second_file"};static const std::set<std::string> source_controls={"source_format","source_shape","source_import_options_json","source_selectors_json","source_preparation_options_json"};
  for(const auto& p:submitted)if(!parts.emplace(p.name,&p).second)invalid("duplicate workbench control/upload "+p.name);
  operation=control(parts,"operation","table_apply");state.operation=operation;const auto found=specs.find(operation);if(found==specs.end())invalid("unknown BOSE workbench operation");spec=found->second;rng.applicable=spec.rng||operation=="detect_log";rng.caller_requested=part(parts,"rng_json")!=nullptr;state.typed=spec.typed;if(part(parts,"operation"))parsed.push_back(text("operation"));
  for(const auto& [name,p]:parts){if(global.contains(name))continue;if(files.contains(name)){if(std::find(spec.roles.begin(),spec.roles.end(),name)==spec.roles.end())invalid("inapplicable upload "+name);if(!p->file)invalid("native input role requires a multipart file "+name);continue;}if(source_controls.contains(name)&&std::find(spec.roles.begin(),spec.roles.end(),"source_file")!=spec.roles.end())continue;invalid("unknown/inapplicable workbench control "+name);}
  for(const auto& role:spec.roles)if(!part(parts,role))invalid("missing "+role);
  if(spec.options.empty()&&part(parts,"options_json"))invalid("selected public helper does not accept options_json");if(spec.parameters.empty()&&part(parts,"parameters_json"))invalid("selected public helper does not accept parameters_json");if(!spec.rng&&part(parts,"rng_json"))invalid("selected public helper has no caller RNG overload");
  if(!spec.options.empty()){const auto doc=part(parts,"options_json")?parsed_control(parts,"options_json"):Json{Object{}};if(spec.options=="table"){table_options=trace_options<ConceptDriftTableOptions>(doc);configured=options_json(table_options);}else if(spec.options=="trace"){direct_options=trace_options<ConceptDriftTraceOptions>(doc);configured=options_json(direct_options);}else{whole_options=log_options(doc);configured=options_json(whole_options);}if(part(parts,"options_json")){for(const auto& [key,value]:doc.object()){(void)value;option_presence.push_back(text(key));}parsed.push_back(text("options_json"));}}
  if(const auto* p=part(parts,"parameters_json")){const auto doc=Parser(p->bytes).parse();read_parameters(parameters,doc,spec);for(const auto& [key,value]:doc.object()){(void)value;parameter_presence.push_back(text(key));}parsed.push_back(text("parameters_json"));}configured_parameters=parameters_json(parameters,spec);
  if(const auto* p=part(parts,"rng_json")){rng.read(Parser(p->bytes).parse());parsed.push_back(text("rng_json"));}
  phase="preparation_error";
  if(part(parts,"source_file")){state.source.emplace();read_source(*state.source,std::get<Object>(context.value).at("source"),"source",parts,parsed);state.ready.insert("source_file");}
  if(const auto* p=part(parts,"trace_file")){const auto doc=Parser(p->bytes).parse();if(spec.typed)state.typed_trace=attributes(doc);else state.trace=trace_value(doc);state.ready.insert("trace_file");parsed.push_back(text("trace_file"));}
  if(const auto* p=part(parts,"traces_file")){const auto doc=Parser(p->bytes).parse();if(spec.typed)state.typed_traces=vector_value<std::vector<EventAttribute>>(doc,attributes);else if(operation=="detect_traces")state.string_traces=vector_value<std::vector<std::string>>(doc,string_values);else state.traces=vector_value<Trace>(doc,trace_value);state.ready.insert("traces_file");parsed.push_back(text("traces_file"));}
  if(const auto* p=part(parts,"alphabet_file")){const auto doc=Parser(p->bytes).parse();if(spec.typed)state.typed_alphabet=attributes(doc);else state.alphabet=string_values(doc);state.ready.insert("alphabet_file");parsed.push_back(text("alphabet_file"));}
  for(const auto& role:{std::string("first"),std::string("second")})if(const auto* p=part(parts,role+"_file")){auto matrix=vector_value<std::vector<double>>(Parser(p->bytes).parse(),real_vector);if(role=="first")state.first=std::move(matrix);else state.second=std::move(matrix);state.ready.insert(role+"_file");parsed.push_back(text(role+"_file"));}
  phase="construction_error";snapshot("inputs_before_copies",before);identity_before=identity_classes(state.locations());
  if(const auto* p=part(parts,"construction_copies_json")){copy_plan=Parser(p->bytes).parse();const auto& plan=copy_plan.array();for(std::size_t i=0;i<plan.size();++i){const auto& instruction=plan[i].object();allowed_fields(instruction,{"from","to","member"});const auto member=required(instruction,"member").string();if(member!="attribute"&&member!="typed_value")invalid("unknown construction-copy member");const auto locations=state.locations();const EventAttribute source=*resolve(locations,required(instruction,"from")).value;auto* destination=resolve(locations,required(instruction,"to")).value;if(member=="attribute")*destination=source;else destination->typed_value=source.typed_value;state.refresh();copy_steps.push_back(Json{Object{{"instruction",count(i)},{"classes",identity_classes(state.locations())}}});}parsed.push_back(text("construction_copies_json"));}
  snapshot("inputs_after_copies",after);if(state.source&&state.source->kind!=spec.kind)invalid("selected public "+spec.kind+" signature requires an original or explicitly prepared "+spec.kind);phase="native_error";
  std::string call_name=spec.function+"(";if(!spec.kind.empty())call_name+=spec.kind=="table"?"EventTable":"EventLog";else if(operation=="permutation")call_name+="matrix,matrix";else if(operation=="follows")call_name+="Trace,alphabet";else if(operation=="typed_follows")call_name+="typed_trace,typed_alphabet";else if(operation.starts_with("features_traces"))call_name+="Trace[],alphabet";else if(operation=="typed_features")call_name+="typed_traces,typed_alphabet";else call_name+=spec.typed?"typed_traces":"string_traces";
  if(!spec.options.empty())call_name+=",options";
  if(operation=="permutation")call_name+=rng.engine?",num_permutations,std::mt19937&":",num_permutations,seed";
  else{if(rng.engine)call_name+=",std::mt19937&";for(const auto& parameter:spec.parameters)call_name+=","+parameter;}call_name+=')';
  executed=true;
  try{
   if(operation=="table_apply")table_result=rng.engine?apply_concept_drift_table(state.source->table,table_options,*rng.engine):apply_concept_drift_table(state.source->table,table_options);
   else if(operation=="log_apply")table_result=rng.engine?apply_concept_drift_log(state.source->log,table_options,*rng.engine):apply_concept_drift_log(state.source->log,table_options);
   else if(operation=="dispatch_table")table_result=rng.engine?apply_concept_drift(state.source->table,table_options,*rng.engine,parameters.variant):apply_concept_drift(state.source->table,table_options,parameters.variant);
   else if(operation=="dispatch_log")table_result=rng.engine?apply_concept_drift(state.source->log,table_options,*rng.engine,parameters.variant):apply_concept_drift(state.source->log,table_options,parameters.variant);
   else if(operation=="activities_default")result=strings(extract_concept_drift_activities(state.source->log));
   else if(operation=="activities")result=strings(extract_concept_drift_activities(state.source->log,parameters.activity_attribute));
   else if(operation=="typed_activities"){activity_result=extract_concept_drift_typed_activities(state.typed_traces);result=attributes_json(activity_result);}
   else if(operation=="features_traces_default")result=reals(extract_concept_drift_features(state.traces,state.alphabet));
   else if(operation=="features_traces")result=reals(extract_concept_drift_features(state.traces,state.alphabet,parameters.activity_attribute));
   else if(operation=="features_log")result=reals(extract_concept_drift_features(state.source->log));
   else if(operation=="typed_features")result=reals(extract_concept_drift_typed_features(state.typed_traces,state.typed_alphabet));
   else if(operation=="follows"){Array pairs;for(const auto& [a,b]:compute_concept_drift_follows_relation(*state.trace,state.alphabet,parameters.activity_attribute))pairs.push_back(Json{Array{byte_string(a),byte_string(b)}});result=Json{std::move(pairs)};}
   else if(operation=="typed_follows"){typed_pairs=compute_concept_drift_typed_follows_relation(state.typed_trace,state.typed_alphabet);Array pairs;for(const auto& [a,b]:typed_pairs)pairs.push_back(Json{Array{attribute_json(a),attribute_json(b)}});result=Json{std::move(pairs)};}
   else if(operation=="split_sublogs"){split_result=split_concept_drift_sublogs(state.source->log,parameters.sub_log_size,parameters.keep_leftover);result=logs_json(split_result);}
   else if(operation=="permutation")result=real(rng.engine?concept_drift_permutation_test(state.first,state.second,parameters.num_permutations,*rng.engine):concept_drift_permutation_test(state.first,state.second,parameters.num_permutations,parameters.seed));
   else if(operation=="detect_traces")trace_result=rng.engine?detect_concept_drift_traces(state.string_traces,direct_options,*rng.engine):detect_concept_drift_traces(state.string_traces,direct_options);
   else if(operation=="detect_typed_traces")typed_result=rng.engine?detect_concept_drift_typed_traces(state.typed_traces,direct_options,*rng.engine):detect_concept_drift_typed_traces(state.typed_traces,direct_options);
   else log_result=detect_concept_drift(state.source->log,whole_options);
   returned=true;if(table_result)result=table_result_json(*table_result);if(trace_result)result=trace_result_json(*trace_result);if(typed_result)result=typed_result_json(*typed_result);if(log_result)result=log_result_json(*log_result);calls.push_back(Json{Object{{"name",text(call_name)},{"returned",flag(true)},{"error",nil()}}});
  }catch(const std::exception& e){calls.push_back(Json{Object{{"name",text(call_name)},{"returned",flag(returned)},{"error",exception(returned?"result_serialization_error":"native_error",e)}}});throw;}
 }catch(const std::exception& e){status=returned?200:400;failure=exception(phase,e);}
 try{rng.finish();}catch(const std::exception& e){artifact_errors.push_back(Json{Object{{"name",text("rng_after")},{"error",exception("snapshot_error",e)}}});}
 if(before.object().empty())snapshot("inputs_before_copies",before);if(after.object().empty())snapshot("inputs_after_copies",after);snapshot("inputs_after_native",after_native);
 try{auto locations=state.locations();locate(locations,"native_result",{},activity_result);for(std::size_t i=0;i<typed_pairs.size();++i){locate(locations,"native_result",Array{count(i),count(0)},typed_pairs[i].first);locate(locations,"native_result",Array{count(i),count(1)},typed_pairs[i].second);}if(typed_result)for(std::size_t i=0;i<typed_result->sublogs.size();++i)for(std::size_t j=0;j<typed_result->sublogs[i].size();++j)locate(locations,"native_result",Array{text("sublogs"),count(i),count(j)},typed_result->sublogs[i][j]);if(table_result)for(std::size_t i=0;i<table_result->returned_tables.size();++i)locate(locations,"native_result",Array{text("returned_tables"),count(i)},table_result->returned_tables[i]);for(std::size_t i=0;i<split_result.size();++i)locate(locations,"native_result",Array{count(i)},split_result[i]);if(log_result){const auto visit=[&](std::string name,auto& values){for(std::size_t i=0;i<values.size();++i)locate(locations,"native_result",Array{text(name),count(i)},values[i]);};visit("sublogs",log_result->sublogs);visit("returned_sublogs",log_result->returned_sublogs);visit("cumulative_sublogs",log_result->cumulative_sublogs);}identity_after=identity_classes(locations);}catch(const std::exception& e){artifact_errors.push_back(Json{Object{{"name",text("identity_proof")},{"error",exception("snapshot_error",e)}}});}
 if(!configured.null())remember("configured_options.json",[&]{return json_text(configured);});if(!configured_parameters.null())remember("configured_parameters.json",[&]{return json_text(configured_parameters);});remember("rng.json",[&]{return json_text(rng.json());});remember("inputs_before_copies.json",[&]{return json_text(before);});remember("inputs_after_copies.json",[&]{return json_text(after);});remember("inputs_after_native.json",[&]{return json_text(after_native);});
 if(state.source&&state.ready.contains("source_file")){remember("source_constructor.json",[&]{return json_text(state.source->json());});if(state.source->kind=="log")remember("source.xes",[&]{return serialize_xes(state.source->log);});}
 if(returned)remember("native_result.json",[&]{return json_text(result);});if(table_result)for(std::size_t i=0;i<table_result->returned_tables.size();++i)remember("returned_table_"+std::to_string(i)+".json",[&,i]{return serialize_embedding_workbench_table_json(table_result->returned_tables[i]);});
 const auto log_artifacts=[&](std::string name,const std::vector<EventLog>& logs){for(std::size_t i=0;i<logs.size();++i){remember(name+"_"+std::to_string(i)+".json",[&,i]{return serialize_embedding_workbench_raw_log_json(logs[i]);});remember(name+"_"+std::to_string(i)+".xes",[&,i]{return serialize_xes(logs[i]);});}};log_artifacts("split_sublog",split_result);if(log_result){log_artifacts("sublog",log_result->sublogs);log_artifacts("returned_sublog",log_result->returned_sublogs);log_artifacts("cumulative_sublog",log_result->cumulative_sublogs);}
 Array boundaries{text("Finite request-owned lazy rows only; arbitrary executable scanner functions unavailable."),text("Intentional scalar identity uses actual public EventAttribute/typed_value copy assignment within this request."),text("Seeded overload local engine state remains private; caller engine constructors and state use public std::mt19937 APIs only."),text("All 25 public BOSE declarations are available; complete reference behavior and performance parity remain separately unproven.")};
 Object envelope{{"family",text("ORIGINAL_BOSE_WORKBENCH")},{"version",text("original-bose-workbench-v1")},{"operation",byte_string(operation)},{"originals",Json{std::move(originals)}},{"parsed_controls",Json{std::move(parsed)}},{"configured_options",std::move(configured)},{"options_supplied",Json{std::move(option_presence)}},{"configured_parameters",std::move(configured_parameters)},{"parameters_supplied",Json{std::move(parameter_presence)}},{"rng",rng.json()},{"input_context",std::move(context)},{"inputs_before_copies",std::move(before)},{"inputs_after_copies",std::move(after)},{"inputs_after_native",std::move(after_native)},{"construction_copies",std::move(copy_plan)},{"identity_proof",Json{Object{{"before",std::move(identity_before)},{"steps",Json{std::move(copy_steps)}},{"after_native",std::move(identity_after)}}}},{"native_calls",Json{std::move(calls)}},{"executed",flag(executed)},{"returned",flag(returned)},{"result_type",spec.result.empty()?nil():text(spec.result)},{"native_result",std::move(result)},{"error",std::move(failure)},{"artifacts",Json{std::move(artifacts)}},{"artifact_errors",Json{std::move(artifact_errors)}},{"boundaries",Json{std::move(boundaries)}}};return {status,json_text(Json{std::move(envelope)})};
}
namespace {
std::string mime_lower(std::string_view value){std::string out(value);std::transform(out.begin(),out.end(),out.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});return out;}
std::string mime_trim(std::string_view value){auto begin=value.find_first_not_of(" \t"),end=value.find_last_not_of(" \t");return begin==std::string_view::npos?std::string{}:std::string(value.substr(begin,end-begin+1));}
struct MimeParameter {std::string name,value;};
std::vector<MimeParameter> mime_parameters(std::string_view value){std::vector<MimeParameter> out;std::size_t i=0;while(i<value.size()){while(i<value.size()&&(value[i]==' '||value[i]=='\t'||value[i]==';'))++i;if(i==value.size())break;const auto begin=i;while(i<value.size()&&value[i]!='='&&value[i]!=';')++i;auto name=mime_lower(mime_trim(value.substr(begin,i-begin)));if(name.empty())invalid("empty MIME parameter");std::string item;if(i==value.size()||value[i]!='=')invalid("MIME parameter requires equals");if(i<value.size()&&value[i]=='='){++i;while(i<value.size()&&(value[i]==' '||value[i]=='\t'))++i;if(i<value.size()&&value[i]=='"'){++i;bool closed=false;while(i<value.size()){const char c=value[i++];if(c=='"'){closed=true;break;}if(c=='\\'){if(i==value.size())invalid("short MIME quoted escape");item+=value[i++];}else item+=c;}if(!closed)invalid("unterminated MIME quoted parameter");while(i<value.size()&&(value[i]==' '||value[i]=='\t'))++i;if(i<value.size()&&value[i]!=';')invalid("data after MIME quoted parameter");}else{const auto start=i;while(i<value.size()&&value[i]!=';')++i;item=mime_trim(value.substr(start,i-start));}}out.push_back({std::move(name),std::move(item)});if(i<value.size()&&value[i]==';')++i;}return out;}
std::string mime_percent(std::string_view value){std::string out;for(std::size_t i=0;i<value.size();++i){if(value[i]!='%'){out+=value[i];continue;}if(i+2>=value.size())invalid("short extended MIME filename escape");out+=unhex_bytes(value.substr(i+1,2));i+=2;}if(!scalar_utf8(out))invalid("extended MIME filename is not UTF-8");return out;}
std::string mime_boundary(std::string_view content_type){const auto semi=content_type.find(';');if(mime_lower(mime_trim(content_type.substr(0,semi)))!="multipart/form-data")invalid("BOSE workbench requires multipart/form-data");const auto params=semi==std::string_view::npos?std::vector<MimeParameter>{}:mime_parameters(content_type.substr(semi+1));std::optional<std::string> boundary;for(const auto& p:params)if(p.name=="boundary"){if(boundary)invalid("duplicate multipart boundary");boundary=p.value;}if(!boundary||boundary->empty()||boundary->size()>70||boundary->back()==' '||!std::all_of(boundary->begin(),boundary->end(),[](unsigned char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||std::string_view("'()+_,-./:=? ").find(static_cast<char>(c))!=std::string_view::npos;}))invalid("invalid multipart boundary");return *boundary;}
}

std::vector<BoseWorkbenchPart> parse_bose_workbench_multipart(std::string_view content_type,std::string_view body){
 const auto boundary=mime_boundary(content_type);const auto separator="--"+boundary;const auto marker="\r\n"+separator;if(!body.starts_with(separator))invalid("multipart body does not start with its boundary");std::size_t cursor=separator.size();std::vector<BoseWorkbenchPart> out;
 while(true){if(body.substr(cursor,2)=="--"){cursor+=2;if(body.substr(cursor,2)=="\r\n")cursor+=2;if(cursor!=body.size())invalid("unexpected multipart epilogue");return out;}if(body.substr(cursor,2)!="\r\n")invalid("invalid multipart boundary suffix");cursor+=2;const auto headers_end=body.find("\r\n\r\n",cursor);if(headers_end==std::string_view::npos||headers_end-cursor>64u*1024u)invalid("missing/oversized multipart headers");std::optional<std::string> disposition;for(auto line_begin=cursor;line_begin<headers_end;){auto line_end=body.find("\r\n",line_begin);if(line_end==std::string_view::npos||line_end>headers_end)line_end=headers_end;const auto line=body.substr(line_begin,line_end-line_begin);const auto colon=line.find(':');if(colon==std::string_view::npos||line.find_first_of("\r\n")!=std::string_view::npos)invalid("invalid multipart header line");const auto name=mime_lower(mime_trim(line.substr(0,colon)));if(name=="content-disposition"){if(disposition)invalid("duplicate multipart Content-Disposition");disposition=mime_trim(line.substr(colon+1));}line_begin=line_end+2;}if(!disposition)invalid("missing multipart Content-Disposition");const auto semi=disposition->find(';');if(mime_lower(mime_trim(std::string_view(*disposition).substr(0,semi)))!="form-data")invalid("multipart disposition must be form-data");const auto params=semi==std::string::npos?std::vector<MimeParameter>{}:mime_parameters(std::string_view(*disposition).substr(semi+1));std::optional<std::string> name,filename,extended;for(const auto& p:params){if(p.name=="name"){if(name)invalid("duplicate multipart part name");name=p.value;}else if(p.name=="filename"){if(filename)invalid("duplicate multipart filename");filename=p.value;}else if(p.name=="filename*"){if(extended)invalid("duplicate extended multipart filename");if(!p.value.starts_with("UTF-8''"))invalid("extended MIME filename requires UTF-8");extended=mime_percent(std::string_view(p.value).substr(7));}}if(!name)invalid("missing multipart part name");const auto data_begin=headers_end+4;auto next=body.find(marker,data_begin);while(next!=std::string_view::npos){const auto suffix=next+marker.size();if(body.substr(suffix,2)=="\r\n"||(body.substr(suffix,2)=="--"&&(suffix+2==body.size()||body.substr(suffix+2,2)=="\r\n")))break;next=body.find(marker,next+1);}if(next==std::string_view::npos)invalid("missing final multipart boundary");out.push_back({std::move(*name),extended?std::move(*extended):filename?std::move(*filename):std::string{},std::string(body.substr(data_begin,next-data_begin)),filename.has_value()||extended.has_value()});if(out.size()>1000)invalid("multipart body exceeds 1000 parts");cursor=next+marker.size();}
}

BoseWorkbenchResponse execute_bose_workbench_multipart(std::string_view content_type,std::string_view body,const std::vector<BoseWorkbenchPart>& query,bool transport_complete){
 BoseWorkbenchResponse response;try{if(!transport_complete)invalid("multipart stream read did not complete");auto parts=query;auto form=parse_bose_workbench_multipart(content_type,body);parts.insert(parts.end(),std::make_move_iterator(form.begin()),std::make_move_iterator(form.end()));response=execute_bose_workbench(parts);}catch(const std::exception& error){response=execute_bose_workbench({});auto envelope=document(response.json);auto& object=std::get<Object>(envelope.value);object["error"]=exception("transport_error",error);Array originals;for(const auto& p:query)originals.push_back(Json{Object{{"field",byte_string(p.name)},{"filename",nil()},{"original",bytes(p.name,p.bytes)}}});object["originals"]=Json{std::move(originals)};object["operation"]=byte_string(control([&]{Parts parts;for(const auto& p:query)parts.emplace(p.name,&p);return parts;}(),"operation","table_apply"));response.status=400;response.json=json_text(envelope);}
 auto envelope=document(response.json);auto& object=std::get<Object>(envelope.value);object["transport_original"]=bytes("multipart_body",body);object["content_type_original"]=bytes("Content-Type",content_type);response.json=json_text(envelope);return response;
}

} // namespace minera
