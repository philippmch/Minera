#include "minera/knowledge_exact_json.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/literal_controls_json.hpp"
#include <algorithm>
#include <charconv>
#include <cstdint>
#include <initializer_list>
#include <locale>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <variant>
namespace minera { namespace {
[[noreturn]] void invalid(std::string message) {
  throw std::invalid_argument("Knowledge exact JSON: " + message);
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
  explicit Parser(std::string_view text) : text_(text) {}
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
    if (depth > 64) invalid("nesting exceeds 64 levels");
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

// LITERAL_CONTROLS_CODEC
const Json* field(const Json::Object& o,std::string_view key){const auto i=o.find(key);return i==o.end()?nullptr:&i->second;}
const Json& required(const Json::Object& o,std::string_view key){if(const auto* v=field(o,key))return *v;invalid("missing field "+std::string(key));}
void allowed(const Json::Object& o,std::initializer_list<std::string_view> keys){for(const auto& [name,v]:o){(void)v;if(std::find(keys.begin(),keys.end(),name)==keys.end())invalid("unknown field "+name);}}
std::string native_string(const Json& v){
 if(const auto* s=std::get_if<std::string>(&v.value))return *s;
 const auto& o=v.object();allowed(o,{"native_bytes","data"});if(required(o,"native_bytes").string()!="hex-v1")invalid("unknown native byte string version");
 const auto& h=required(o,"data").string();if(h.size()%2)invalid("odd hexadecimal byte string length");
 const auto digit=[](char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;};
 std::string out;out.reserve(h.size()/2);for(std::size_t i=0;i<h.size();i+=2){const int a=digit(h[i]),b=digit(h[i+1]);if(a<0||b<0)invalid("invalid hexadecimal byte string");out+=static_cast<char>((a<<4)|b);}return out;
}
std::vector<std::string> strings(const Json& value){std::vector<std::string> out;for(const auto& v:value.array())out.push_back(native_string(v));return out;}
std::size_t size_value(const Json& v) {
 const auto* n=std::get_if<Number>(&v.value);const auto* s=std::get_if<std::string>(&v.value);
 if(!n&&!s)invalid("expected size_t integer token or decimal string");
 const auto& text=n?n->text:*s;
 if(text.empty()||!std::all_of(text.begin(),text.end(),[](char c){return c>='0'&&c<='9';}))invalid("expected unsigned decimal integer");
 std::size_t out=0;const auto parsed=std::from_chars(text.data(),text.data()+text.size(),out);
 if(parsed.ec!=std::errc{}||parsed.ptr!=text.data()+text.size())invalid("integer exceeds native size_t");return out;
}

std::map<std::string,std::string> context_value(const Json& doc) {
 std::map<std::string,std::string> result;
 for(const auto& row:doc.array()){const auto& pair=row.array();if(pair.size()!=2)invalid("context entries require exactly two values");auto name=native_string(pair[0]),value=native_string(pair[1]);if(!result.emplace(std::move(name),std::move(value)).second)invalid("duplicate decoded context key");}return result;
}
const std::vector<std::string>& legacy_names(){static const std::vector<std::string> names={"knowledge_variant","knowledge_description","knowledge_query","knowledge_database_schema","knowledge_max_input_bytes","knowledge_max_prompt_bytes"};return names;}
bool known(std::string_view field){const auto& names=legacy_names();return knowledge_exact_control(field)||std::find(names.begin(),names.end(),field)!=names.end();}
std::size_t decimal_size(std::string_view input){return size_value(Json{std::string(input)});}
const auto q=serialize_embedding_workbench_string_json;
void strings_json(std::ostream& out,const std::vector<std::string>& values){out<<'[';bool first=true;for(const auto& value:values){if(!first)out<<',';first=false;out<<q(value);}out<<']';}
void context_json(std::ostream& out,const std::map<std::string,std::string>& values){out<<'[';bool first=true;for(const auto& [name,value]:values){if(!first)out<<',';first=false;out<<'['<<q(name)<<','<<q(value)<<']';}out<<']';}
std::ostringstream stream(){std::ostringstream out;out.imbue(std::locale::classic());return out;}
}  // namespace
bool knowledge_exact_control(std::string_view field){return field=="knowledge_assumptions_json"||field=="knowledge_assumptions_file"||field=="knowledge_context_json"||field=="knowledge_context_file"||field=="knowledge_options_json"||field=="knowledge_options_file";}
std::vector<std::string> parse_knowledge_exact_assumptions_json(std::string_view input){return strings(Parser(input).parse());}
std::map<std::string,std::string> parse_knowledge_exact_context_json(std::string_view input){return context_value(Parser(input).parse());}
KnowledgeInjectionOptions parse_knowledge_exact_options_json(std::string_view input){
 const auto doc=Parser(input).parse();const auto& o=doc.object();allowed(o,{"variant","description","query","database_schema","assumptions","context","max_input_bytes","max_prompt_bytes"});
 KnowledgeInjectionOptions result;result.variant=knowledge_injection_variant_from_string(required(o,"variant").string());result.description=native_string(required(o,"description"));result.query=native_string(required(o,"query"));result.database_schema=native_string(required(o,"database_schema"));result.assumptions=strings(required(o,"assumptions"));result.context=context_value(required(o,"context"));result.max_input_bytes=size_value(required(o,"max_input_bytes"));result.max_prompt_bytes=size_value(required(o,"max_prompt_bytes"));return result;
}
KnowledgeInjectionOptions parse_knowledge_exact_inputs(const std::vector<KnowledgeExactInput>& inputs){
 std::map<std::string,const KnowledgeExactInput*> fields;
 for(const auto& input:inputs){if(!known(input.field))throw std::invalid_argument("unknown knowledge field: "+input.field);if(!fields.emplace(input.field,&input).second)throw std::invalid_argument("duplicate knowledge field: "+input.field);if(input.field.ends_with("_file")&&!input.name)throw std::invalid_argument(input.field+" requires an actual File input");}
 const auto has=[&](std::string_view field){return fields.contains(std::string(field));};
 const auto value=[&](std::string_view field)->const std::string&{return fields.at(std::string(field))->content;};
 for(const auto* stem:{"assumptions","context","options"}){const auto prefix="knowledge_"+std::string(stem);if(has(prefix+"_json")&&has(prefix+"_file"))throw std::invalid_argument(prefix+"_json and "+prefix+"_file are mutually exclusive");}
 const bool full=has("knowledge_options_json")||has("knowledge_options_file");
 if(full){for(const auto& [field,input]:fields){(void)input;if(field!="knowledge_options_json"&&field!="knowledge_options_file")throw std::invalid_argument("complete knowledge options cannot combine with "+field);}return parse_knowledge_exact_options_json(value(has("knowledge_options_json")?"knowledge_options_json":"knowledge_options_file"));}
 KnowledgeInjectionOptions result;
 if(has("knowledge_variant")&&!value("knowledge_variant").empty())result.variant=knowledge_injection_variant_from_string(value("knowledge_variant"));
 if(has("knowledge_description"))result.description=value("knowledge_description");if(has("knowledge_query"))result.query=value("knowledge_query");if(has("knowledge_database_schema"))result.database_schema=value("knowledge_database_schema");
 if(has("knowledge_max_input_bytes"))result.max_input_bytes=decimal_size(value("knowledge_max_input_bytes"));if(has("knowledge_max_prompt_bytes"))result.max_prompt_bytes=decimal_size(value("knowledge_max_prompt_bytes"));
 if(has("knowledge_assumptions_json")||has("knowledge_assumptions_file"))result.assumptions=parse_knowledge_exact_assumptions_json(value(has("knowledge_assumptions_json")?"knowledge_assumptions_json":"knowledge_assumptions_file"));
 if(has("knowledge_context_json")||has("knowledge_context_file"))result.context=parse_knowledge_exact_context_json(value(has("knowledge_context_json")?"knowledge_context_json":"knowledge_context_file"));return result;
}
std::string serialize_knowledge_exact_options_json(const KnowledgeInjectionOptions& options){
 auto out=stream();out<<"{\"variant\":"<<q(knowledge_injection_variant_name(options.variant))<<",\"description\":"<<q(options.description)<<",\"query\":"<<q(options.query)<<",\"database_schema\":"<<q(options.database_schema)<<",\"assumptions\":";strings_json(out,options.assumptions);out<<",\"context\":";context_json(out,options.context);out<<",\"max_input_bytes\":"<<options.max_input_bytes<<",\"max_prompt_bytes\":"<<options.max_prompt_bytes<<'}';return out.str();
}
std::string serialize_knowledge_exact_result_json(const KnowledgeInjectionResult& result){auto out=stream();out<<"{\"family\":\"KNOWLEDGE_INJECTION\",\"variant\":"<<q(knowledge_injection_variant_name(result.variant))<<",\"computed\":"<<(result.computed?"true":"false")<<",\"prompt\":"<<q(result.prompt)<<",\"input_bytes\":"<<result.input_bytes<<",\"error\":"<<q(result.error)<<'}';return out.str();}
std::string serialize_knowledge_exact_envelope_json(const std::optional<KnowledgeInjectionOptions>& options,const std::optional<KnowledgeInjectionResult>& result,const std::vector<KnowledgeExactInput>& inputs,std::string_view error){
 std::string legacy="null",legacy_error="null",transport_error="null";
 if(result)try{const auto raw=knowledge_injection_json(*result);legacy=q(raw);if(const auto fault=literal_controls_transport_error(raw))transport_error=q(*fault);}catch(const std::exception& e){legacy_error=q(e.what());}
 auto out=stream();out<<"{\"family\":\"KNOWLEDGE_INJECTION\",\"operation\":\"exact\",\"result\":"<<(result?serialize_knowledge_exact_result_json(*result):"null")<<",\"error\":"<<(error.empty()?"null":"{\"type\":\"native_error\",\"message\":"+q(error)+"}")<<",\"exact_knowledge\":{\"native_schema\":\"knowledge-exact-v1\",\"options\":"<<(options?serialize_knowledge_exact_options_json(*options):"null")<<",\"original_inputs\":[";bool first=true;
 for(const auto& input:inputs){if(!first)out<<',';first=false;out<<"{\"field\":"<<q(input.field)<<",\"name\":"<<(input.name?q(*input.name):"null")<<",\"transport_filename\":"<<(input.transport_filename?q(*input.transport_filename):"null")<<",\"bytes\":"<<input.content.size()<<",\"content\":"<<q(input.content)<<'}';}
 out<<"],\"supplied_fields\":[";first=true;std::set<std::string> supplied;for(const auto& input:inputs)supplied.insert(input.field);for(const auto& field:supplied){if(!first)out<<',';first=false;out<<q(field);}out<<"],\"prompt_bytes\":"<<(result?std::to_string(result->prompt.size()):"null")<<",\"legacy_result_bytes\":"<<legacy<<",\"legacy_result_error\":"<<legacy_error<<",\"legacy_result_transport_error\":"<<transport_error<<"}}";return out.str();
}
}  // namespace minera
