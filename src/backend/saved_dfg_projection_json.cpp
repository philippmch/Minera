#include "minera/saved_dfg_projection_json.hpp"
#include "minera/embedding_workbench_json.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <initializer_list>
#include <limits>
#include <locale>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <variant>
namespace minera { namespace {
[[noreturn]] void invalid(std::string message) {
  throw std::invalid_argument("saved DFG JSON: " + message);
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
          // Existing native saved diagnostics can contain escaped lone UTF-16
          // code units. Preserve them as WTF-8 internally; raw UTF-8 is still
          // scalar-validated above, and output NativeString safely carries it.
          if (code >= 0xd800 && code <= 0xdbff && position_ + 6 <= text_.size() &&
              text_[position_] == '\\' && text_[position_ + 1] == 'u') {
            const auto saved_position = position_;
            position_ += 2;
            const auto low = hex4();
            if (low >= 0xdc00 && low <= 0xdfff)
              code = 0x10000 + ((code - 0xd800) << 10) + low - 0xdc00;
            else position_ = saved_position;
          }
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



std::string native_string(const Json& value) {
 if (const auto* s=std::get_if<std::string>(&value.value)) return *s;
 const auto& o=value.object(); fields(o,{"native_bytes","data"});
 if(required(o,"native_bytes").string()!="hex-v1") invalid("unknown native byte string version");
 const auto& h=required(o,"data").string(); if(h.size()%2)invalid("odd hexadecimal byte length");
 const auto digit=[](char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;};
 std::string out;out.reserve(h.size()/2);for(std::size_t i=0;i<h.size();i+=2){const int a=digit(h[i]),b=digit(h[i+1]);if(a<0||b<0)invalid("invalid hexadecimal byte string");out+=static_cast<char>((a<<4)|b);}return out;
}
std::size_t index(const Json& v){
 const auto s=number_text(v);if(s.empty()||!std::all_of(s.begin(),s.end(),[](char c){return c>='0'&&c<='9';}))invalid("expected unsigned decimal integer token or string");
 return number<std::size_t>(s);
}

double score(const Json& v) {
 if(const auto* s=std::get_if<std::string>(&v.value)) {
  if(*s=="NaN")return std::numeric_limits<double>::quiet_NaN();
  if(*s=="INF")return std::numeric_limits<double>::infinity();
  if(*s=="-INF")return -std::numeric_limits<double>::infinity();
  invalid("score strings must be NaN, INF or -INF");
 }
 const auto* token=std::get_if<Number>(&v.value);if(!token)invalid("score must be a numeric token or nonfinite tag");
 return number<double>(token->text);
}
std::uint64_t count(const Json& v) {
 const auto s=number_text(v);if(s.empty()||!std::all_of(s.begin(),s.end(),[](char c){return c>='0'&&c<='9';}))invalid("expected unsigned decimal count token or string");
 return number<std::uint64_t>(s);
}
void floating(std::ostream& out,double v) {
 if(std::isnan(v)){out<<"\"NaN\"";return;}if(std::isinf(v)){out<<(v<0?"\"-INF\"":"\"INF\"");return;}
 std::ostringstream n;n.imbue(std::locale::classic());n<<std::setprecision(std::numeric_limits<double>::max_digits10)<<v;auto s=n.str();if(s.find_first_of(".eE")==std::string::npos)s+=".0";out<<s;
}
std::map<std::string,std::uint64_t> boundaries(const Json& v) {
 std::map<std::string,std::uint64_t> r;
 if(const auto* o=std::get_if<Json::Object>(&v.value)){for(const auto& [k,c]:*o)r.emplace(k,count(c));}
 else for(const auto& row:v.array()){const auto& o=row.object();fields(o,{"activity","count"});if(!r.emplace(native_string(required(o,"activity")),count(required(o,"count"))).second)invalid("duplicate decoded boundary activity");}
 return r;
}
void write_boundaries(std::ostream& out,const std::map<std::string,std::uint64_t>& v) {
 out<<'[';bool first=true;for(const auto& [k,c]:v){if(!first)out<<',';first=false;out<<"{\"activity\":"<<serialize_embedding_workbench_string_json(k)<<",\"count\":"<<c<<'}';}out<<']';
}
} // namespace
DfgAlignmentLogResult parse_saved_dfg_alignment_result_json(std::string_view input,std::string_view shape) {
 auto parsed=Parser(input).parse();const Json* result=&parsed;
 if(shape=="workbench")result=&required(parsed.object(),"result");
 else if(shape=="report")result=&required(required(parsed.object(),"native_result").object(),"result");
 else if(shape!="native_result")invalid("saved_input_shape must be native_result, workbench or report");
 const auto& o=result->object();fields(o,{"alignments","unique_variants","projection"});
 DfgAlignmentLogResult r;r.unique_variants=index(required(o,"unique_variants"));
 for(const auto& row:required(o,"alignments").array()) {
  const auto& c=row.object();fields(c,{"computed","alignment","cost","internal_cost","fitness","best_worst_cost","visited_states","closed","closed_states"});
  DfgAlignmentResult a;a.computed=required(c,"computed").boolean();
  for(const auto& move:required(c,"alignment").array()){const auto& m=move.object();fields(m,{"log","model"});a.alignment.push_back({native_string(required(m,"log")),native_string(required(m,"model"))});}
  a.cost=score(required(c,"cost"));a.internal_cost=score(required(c,"internal_cost"));a.fitness=score(required(c,"fitness"));a.best_worst_cost=score(required(c,"best_worst_cost"));
  a.visited_states=index(required(c,"visited_states"));a.closed=index(required(c,"closed"));a.closed_states=index(required(c,"closed_states"));r.alignments.push_back(std::move(a));
 }
 return r;
}
std::string serialize_saved_dfg_alignment_result_json(const DfgAlignmentLogResult& r) {
 std::ostringstream out;out.imbue(std::locale::classic());out<<"{\"alignments\":[";
 for(std::size_t i=0;i<r.alignments.size();++i){if(i)out<<',';const auto& a=r.alignments[i];out<<"{\"computed\":"<<(a.computed?"true":"false")<<",\"alignment\":[";
  for(std::size_t j=0;j<a.alignment.size();++j){if(j)out<<',';out<<"{\"log\":"<<serialize_embedding_workbench_string_json(a.alignment[j].log)<<",\"model\":"<<serialize_embedding_workbench_string_json(a.alignment[j].model)<<'}';}
  out<<"],\"cost\":";floating(out,a.cost);out<<",\"internal_cost\":";floating(out,a.internal_cost);out<<",\"fitness\":";floating(out,a.fitness);out<<",\"best_worst_cost\":";floating(out,a.best_worst_cost);out<<",\"visited_states\":"<<a.visited_states<<",\"closed\":"<<a.closed<<",\"closed_states\":"<<a.closed_states<<'}';
 }out<<"],\"unique_variants\":"<<r.unique_variants<<",\"projection\":null}";return out.str();
}
AlphaDfgDiscoveryInput parse_saved_dfg_projection_model_json(std::string_view input) {
 const auto parsed=Parser(input).parse();const auto& o=parsed.object();fields(o,{"kind","dfg","start_activities","end_activities","activities"});AlphaDfgDiscoveryInput r;
 const auto kind=required(o,"kind").string();if(kind=="clean")r.kind=AlphaDfgDiscoveryInput::Kind::kClean;else if(kind=="sparse")r.kind=AlphaDfgDiscoveryInput::Kind::kSparse;else if(kind=="graph")r.kind=AlphaDfgDiscoveryInput::Kind::kGraph;else invalid("unknown DFG kind");
 for(const auto& row:required(o,"dfg").array()){const auto& e=row.object();fields(e,{"source","target","count"});if(!r.dfg.emplace(DfgActivityPair{native_string(required(e,"source")),native_string(required(e,"target"))},count(required(e,"count"))).second)invalid("duplicate decoded DFG edge");}
 r.start_activities=boundaries(required(o,"start_activities"));r.end_activities=boundaries(required(o,"end_activities"));
 if(kind=="graph"){
  std::set<std::string> labels;for(const auto& v:required(o,"activities").array()){auto k=native_string(v);if(!labels.insert(k).second)invalid("duplicate registry label");r.activities.push_back(std::move(k));}
  for(const auto& [e,c]:r.dfg){(void)c;if(!labels.contains(e.first)||!labels.contains(e.second))invalid("edge absent from registry");}
  for(const auto* b:{&*r.start_activities,&*r.end_activities})for(const auto& [k,c]:*b){(void)c;if(!labels.contains(k))invalid("boundary absent from registry");}
 }else if(optional(o,"activities"))invalid("activity registry requires graph kind");
 return r;
}
std::string serialize_saved_dfg_projection_model_json(const AlphaDfgDiscoveryInput& r) {
 if(!r.start_activities||!r.end_activities)invalid("projection model requires explicit boundaries");
 const auto kind=r.kind==AlphaDfgDiscoveryInput::Kind::kClean?"clean":r.kind==AlphaDfgDiscoveryInput::Kind::kSparse?"sparse":r.kind==AlphaDfgDiscoveryInput::Kind::kGraph?"graph":nullptr;if(!kind)invalid("unknown DFG kind");
 std::ostringstream out;out.imbue(std::locale::classic());out<<"{\"kind\":\""<<kind<<"\",\"dfg\":[";bool first=true;
 for(const auto& [e,c]:r.dfg){if(!first)out<<',';first=false;out<<"{\"source\":"<<serialize_embedding_workbench_string_json(e.first)<<",\"target\":"<<serialize_embedding_workbench_string_json(e.second)<<",\"count\":"<<c<<'}';}
 out<<"],\"start_activities\":";write_boundaries(out,*r.start_activities);out<<",\"end_activities\":";write_boundaries(out,*r.end_activities);
 if(r.kind==AlphaDfgDiscoveryInput::Kind::kGraph){out<<",\"activities\":[";for(std::size_t i=0;i<r.activities.size();++i){if(i)out<<',';out<<serialize_embedding_workbench_string_json(r.activities[i]);}out<<']';}out<<'}';return out.str();
}
std::string serialize_saved_dfg_projection_metrics_json(const DfgAlignmentProjection& r) {
 std::ostringstream out;out.imbue(std::locale::classic());out<<"{\"dfg\":[";bool first=true;
 for(const auto& [e,c]:r.dfg){if(!first)out<<',';first=false;out<<"{\"source\":"<<serialize_embedding_workbench_string_json(e.first)<<",\"target\":"<<serialize_embedding_workbench_string_json(e.second)<<",\"sync\":"<<c.sync<<",\"mm\":"<<c.mm<<'}';}
 out<<"],\"activities\":[";first=true;for(const auto& [k,c]:r.activities){if(!first)out<<',';first=false;out<<"{\"activity\":"<<serialize_embedding_workbench_string_json(k)<<",\"sync\":"<<c.sync<<",\"mm\":"<<c.mm<<",\"lm\":"<<c.lm<<'}';}out<<"]}";return out.str();
}

std::map<std::string,std::string> parse_saved_dfg_projection_selectors_json(std::string_view input) {
 const auto document=Parser(input).parse();const auto& o=document.object();fields(o,{"alignment_case_id_attribute","dfg_activity_attribute","dfg_timestamp_attribute"});std::map<std::string,std::string> r;for(const auto& [k,v]:o)r.emplace(k,native_string(v));return r;
}
std::string serialize_saved_dfg_projection_options_json(const DfgAlignmentOptions& o) {
 std::ostringstream out;out.imbue(std::locale::classic());out<<"{\"variant\":\"CLASSIC\",\"variant_index\":"<<static_cast<int>(o.variant)<<",\"activity_attribute\":"<<serialize_embedding_workbench_string_json(o.activity_attribute)<<",\"timestamp_attribute\":"<<serialize_embedding_workbench_string_json(o.timestamp_attribute)<<",\"max_visited_states\":"<<o.max_visited_states<<",\"max_time_ms\":"<<o.max_time_ms;
 const auto map=[&](const char* key,const auto& costs){out<<",\""<<key<<"\":[";bool first=true;for(const auto& [a,c]:costs){if(!first)out<<',';first=false;out<<"{\"activity\":"<<serialize_embedding_workbench_string_json(a)<<",\"cost\":";floating(out,c);out<<'}';}out<<']';};
 map("sync_cost",o.sync_cost);map("model_move_cost",o.model_move_cost);map("log_move_cost",o.log_move_cost);map("internal_log_move_cost",o.internal_log_move_cost);
 out<<",\"sync_cost_supplied\":"<<(o.sync_cost_supplied?"true":"false")<<",\"model_move_cost_supplied\":"<<(o.model_move_cost_supplied?"true":"false")<<",\"log_move_cost_supplied\":"<<(o.log_move_cost_supplied?"true":"false")<<",\"internal_log_move_cost_supplied\":"<<(o.internal_log_move_cost_supplied?"true":"false")<<'}';return out.str();
}
} // namespace minera
