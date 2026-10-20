#include "minera/otg_threshold_json.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/literal_controls_json.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <initializer_list>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <variant>
#include <utility>
namespace minera { namespace {
[[noreturn]] void invalid(std::string message) {
  throw std::invalid_argument("OTG exact JSON: " + message);
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
                  bool bounded_transport = true, bool raw_byte_strings = false)
      : text_(text), escaped_lone_surrogates_(escaped_lone_surrogates),
        bounded_transport_(bounded_transport), raw_byte_strings_(raw_byte_strings) {
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
      if (c >= 0x80 && !raw_byte_strings_) {
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
  bool raw_byte_strings_ = false;
  std::size_t position_ = 0, nodes_ = 0;
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
double real_value(const Json& v){
 if(const auto* s=std::get_if<std::string>(&v.value)){if(*s=="NaN")return std::numeric_limits<double>::quiet_NaN();if(*s=="INF")return std::numeric_limits<double>::infinity();if(*s=="-INF")return -std::numeric_limits<double>::infinity();invalid("unknown IEEE double tag");}
 const auto* n=std::get_if<Number>(&v.value);if(!n)invalid("expected double or IEEE tag");double out=0;const auto parsed=std::from_chars(n->text.data(),n->text.data()+n->text.size(),out);if(parsed.ec!=std::errc{}||parsed.ptr!=n->text.data()+n->text.size()||!std::isfinite(out))invalid("numeric token exceeds finite double storage");return out;
}

void real_json(std::ostream& out,double v) {
 if(std::isnan(v))out<<"\"NaN\"";
 else if(std::isinf(v))out<<(v<0?"\"-INF\"":"\"INF\"");
 else if(v==0&&std::signbit(v))out<<"-0.0";
 else {std::ostringstream scalar;scalar.imbue(std::locale::classic());scalar<<std::setprecision(std::numeric_limits<double>::max_digits10)<<v;auto t=scalar.str();if(t.find_first_of(".eE")==std::string::npos)t+=".0";out<<t;}
}
void write_string(std::ostream& out,std::string_view v) {out<<serialize_embedding_workbench_string_json(v);}
void write_strings(std::ostream& out,const std::vector<std::string>& v) {out<<'[';bool first=true;for(const auto& s:v){if(!first)out<<',';first=false;write_string(out,s);}out<<']';}
void write_edge(std::ostream& out,const std::tuple<std::string,std::string,std::string>& e) {out<<'[';write_string(out,std::get<0>(e));out<<',';write_string(out,std::get<1>(e));out<<',';write_string(out,std::get<2>(e));out<<']';}
std::ostringstream stream() {std::ostringstream out;out.imbue(std::locale::classic());return out;}
}  // namespace
std::map<std::string,double> parse_otg_theta_by_relation_json(std::string_view input) {
 const auto doc=Parser(input).parse();std::map<std::string,double> out;
 for(const auto& row:doc.array()){const auto& pair=row.array();if(pair.size()!=2)invalid("threshold entries require exactly two values");auto k=native_string(pair[0]);auto v=real_value(pair[1]);if(!out.emplace(std::move(k),v).second)invalid("duplicate decoded relation key");}return out;
}
OtgGraph parse_exact_otg_graph_json(std::string_view input) {
 const auto doc=Parser(input,true,true,true).parse();const auto& o=doc.object();
 for(const auto* key:{"activities","activities_indep","activities_ot","start_activities","end_activities","performance_computed"})if(field(o,key))invalid("OTG graph envelope does not match the selected kind");
 OtgGraph graph;graph.object_types=strings(required(o,"object_types"));
 for(const auto& raw:required(o,"edges").array()){const auto& e=raw.object();graph.edges.push_back({native_string(required(e,"source_type")),native_string(required(e,"relation")),native_string(required(e,"target_type")),size_value(required(e,"frequency"))});}return graph;
}
std::string serialize_exact_otg_graph_json(const OtgGraph& graph) {
 auto out=stream();out<<"{\"object_types\":";write_strings(out,graph.object_types);out<<",\"edges\":[";bool first=true;
 for(const auto& e:graph.edges){if(!first)out<<',';first=false;out<<"{\"source_type\":";write_string(out,e.source_type);out<<",\"relation\":";write_string(out,e.relation);out<<",\"target_type\":";write_string(out,e.target_type);out<<",\"frequency\":"<<e.frequency<<'}';}out<<"]}";return out.str();
}
std::string serialize_exact_otg_options_json(const OtgConformanceOptions& options) {
 auto out=stream();out<<"{\"alpha\":";real_json(out,options.alpha);out<<",\"beta\":";real_json(out,options.beta);out<<",\"gamma\":";real_json(out,options.gamma);out<<",\"theta\":";real_json(out,options.theta);out<<",\"theta_by_relation\":[";bool first=true;
 for(const auto& [k,v]:options.theta_by_relation){if(!first)out<<',';first=false;out<<'[';write_string(out,k);out<<',';real_json(out,v);out<<']';}out<<"]}";return out.str();
}
std::string serialize_exact_otg_result_json(const OtgConformanceResult& result) {
 auto out=stream();out<<"{\"fitness\":";real_json(out,result.fitness);out<<",\"missing_object_types\":";write_strings(out,result.missing_object_types);out<<",\"additional_object_types\":";write_strings(out,result.additional_object_types);
 out<<",\"missing_edges\":[";bool first=true;for(const auto& e:result.missing_edges){if(!first)out<<',';first=false;write_edge(out,e);}out<<"],\"additional_edges\":[";first=true;for(const auto& e:result.additional_edges){if(!first)out<<',';first=false;write_edge(out,e);}out<<"],\"non_conforming_edges\":[";first=true;
 for(const auto& [e,delta]:result.non_conforming_edges){if(!first)out<<',';first=false;out<<"{\"source_type\":";write_string(out,std::get<0>(e));out<<",\"relation\":";write_string(out,std::get<1>(e));out<<",\"target_type\":";write_string(out,std::get<2>(e));out<<",\"delta\":";real_json(out,delta);out<<'}';}out<<"]}";return out.str();
}
std::string serialize_exact_otg_conformance_json(const OtgConformanceResult& result,const OtgGraph& real,const OtgGraph& normative,const OtgConformanceOptions& options,std::string_view original_real,std::string_view original_normative,std::string_view original_threshold_json) {
 std::string legacy="null",fault="null",transport="null";
 const auto legacy_graph=[](const OtgGraph& graph){
  std::string bytes="null",error="null",transport="null";
  try{const auto raw=otg_json(graph);bytes=serialize_embedding_workbench_string_json(raw);if(const auto e=literal_controls_transport_error(raw))transport=serialize_embedding_workbench_string_json(*e);}
  catch(const std::exception& e){error=serialize_embedding_workbench_string_json(e.what());}
  return "{\"bytes\":"+bytes+",\"error\":"+error+",\"transport_error\":"+transport+"}";
 };
 try {const auto bytes=otg_conformance_json(result);legacy=serialize_embedding_workbench_string_json(bytes);if(const auto e=literal_controls_transport_error(bytes))transport=serialize_embedding_workbench_string_json(*e);}
 catch(const std::exception& e){fault=serialize_embedding_workbench_string_json(e.what());}
 auto out=stream();out<<"{\"family\":\"OCEL_CONFORMANCE\",\"variant\":\"otg\",\"result\":"<<serialize_exact_otg_result_json(result)<<",\"exact_otg\":{\"native_schema\":\"otg-exact-v1\",\"original_real\":";write_string(out,original_real);out<<",\"original_normative\":";write_string(out,original_normative);out<<",\"original_threshold_json\":";write_string(out,original_threshold_json);out<<",\"real_graph\":"<<serialize_exact_otg_graph_json(real)<<",\"normative_graph\":"<<serialize_exact_otg_graph_json(normative)<<",\"options\":"<<serialize_exact_otg_options_json(options)<<",\"legacy_real_graph\":"<<legacy_graph(real)<<",\"legacy_normative_graph\":"<<legacy_graph(normative)<<",\"legacy_result_bytes\":"<<legacy<<",\"legacy_result_error\":"<<fault<<",\"legacy_result_transport_error\":"<<transport<<"}}";return out.str();
}
}  // namespace minera
