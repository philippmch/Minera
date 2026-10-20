#include "minera/replay_duration_workbench_json.hpp"
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
#include <sstream>
#include <stdexcept>
#include <variant>
namespace minera { namespace {
[[noreturn]] void invalid(std::string message) {
  throw std::invalid_argument("replay duration JSON: " + message);
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
std::vector<std::size_t> indices(const Json& v){std::vector<std::size_t> r;for(const auto& x:v.array())r.push_back(index(x));return r;}
const Json::Array& saved_rows(const Json& v,std::string_view shape){
 if(shape=="native_rows")return v.array();
 if(shape=="native_result")return required(v.object(),"traces").array();
 if(shape=="workbench")return required(required(v.object(),"diagnostics").object(),"traces").array();
 if(shape=="report")return required(required(required(v.object(),"native_result").object(),"diagnostics").object(),"traces").array();
 invalid("saved_input_shape must be native_rows, native_result, workbench or report");
}
std::vector<TokenReplayTraceDiagnostics> project_saved(const Json& value,std::string_view shape){
 std::vector<TokenReplayTraceDiagnostics> result;
 for(const auto& row:saved_rows(value,shape)){
  const auto& c=row.object();TokenReplayTraceDiagnostics d;d.trace_index=index(required(c,"trace_index"));
  d.unknown_event_indices=indices(required(c,"unknown_event_indices"));
  for(const auto& e:required(c,"event_diagnostics").array()){
   const auto& ev=e.object();TokenReplayEventDiagnostics event;event.event_index=index(required(ev,"event_index"));event.transitions_with_problems=indices(required(ev,"transitions_with_problems"));d.event_diagnostics.push_back(std::move(event));
  }
  result.push_back(std::move(d));
 }
 return result;
}
void real(std::ostream& out,double v){if(std::isnan(v))out<<"\"NaN\"";else if(std::isinf(v))out<<(v<0?"\"-INF\"":"\"INF\"");else out<<std::setprecision(std::numeric_limits<double>::max_digits10)<<v;}
void vector_json(std::ostream& out,const std::vector<std::size_t>& v){out<<'[';for(std::size_t i=0;i<v.size();++i){if(i)out<<',';out<<v[i];}out<<']';}
void comparison(std::ostream& out,const TokenReplayDurationComparison& v){
 out<<",\"fit_trace_indices\":";vector_json(out,v.fit_trace_indices);out<<",\"problem_trace_indices\":";vector_json(out,v.problem_trace_indices);
 out<<",\"fit_count\":"<<v.fit_count<<",\"problem_count\":"<<v.problem_count<<",\"fit_median_seconds\":";real(out,v.fit_median_seconds);
 out<<",\"problem_median_seconds\":";real(out,v.problem_median_seconds);out<<",\"relative_throughput\":";real(out,v.relative_throughput);
}
} // namespace
ReplayDurationWorkbenchInput parse_replay_duration_workbench_json(std::string_view text){
 const auto parsed=Parser(text).parse();const auto& o=parsed.object();
 fields(o,{"mode","activity_attribute","timestamp_attribute","transition_cohorts","unknown_activity_cohorts","saved_diagnostics","saved_input_shape","trace_index","trace_indices"});
 ReplayDurationWorkbenchInput r;r.raw_json=std::string(text);r.mode=required(o,"mode").string();
 if(r.mode!="cohorts"&&r.mode!="saved_diagnostics"&&r.mode!="case_duration"&&r.mode!="median_duration")invalid("unknown duration mode");
 if(const auto* v=optional(o,"activity_attribute"))r.activity_attribute=native_string(*v);
 if(const auto* v=optional(o,"timestamp_attribute"))r.timestamp_attribute=native_string(*v);
 if(const auto* v=optional(o,"transition_cohorts"))for(const auto& row:v->array()){
  const auto& c=row.object();fields(c,{"transition_index","activity","problem_trace_indices"});
  const auto& a=required(c,"activity");r.transition_cohorts.push_back({index(required(c,"transition_index")),a.null()?std::nullopt:std::optional<std::string>(native_string(a)),indices(required(c,"problem_trace_indices"))});
 }
 if(const auto* v=optional(o,"unknown_activity_cohorts"))for(const auto& row:v->array()){
  const auto& c=row.object();fields(c,{"activity","activity_is_null","problem_trace_indices"});
  r.unknown_activity_cohorts.push_back({native_string(required(c,"activity")),required(c,"activity_is_null").boolean(),indices(required(c,"problem_trace_indices"))});
 }
 if(const auto* v=optional(o,"saved_input_shape"))r.saved_input_shape=v->string();
 if(const auto* v=optional(o,"saved_diagnostics")){r.has_saved_diagnostics=true;r.saved_diagnostics=project_saved(*v,r.saved_input_shape);}
 if(const auto* v=optional(o,"trace_index"))r.trace_index=index(*v);else if(r.mode=="case_duration")invalid("case_duration requires trace_index");
 if(const auto* v=optional(o,"trace_indices"))r.trace_indices=indices(*v);else if(r.mode=="median_duration")invalid("median_duration requires trace_indices");
 return r;
}
std::vector<TokenReplayTraceDiagnostics> parse_saved_replay_duration_diagnostics_json(std::string_view raw,std::string_view input_shape){
 return project_saved(Parser(raw).parse(),input_shape);
}
std::string serialize_replay_duration_diagnostics_json(const TokenReplayDurationDiagnostics& r){
 std::ostringstream out;out.imbue(std::locale::classic());out<<"{\"timestamp_attribute\":"<<serialize_embedding_workbench_string_json(r.timestamp_attribute)<<",\"transitions\":[";
 for(std::size_t i=0;i<r.transitions.size();++i){if(i)out<<',';const auto& v=r.transitions[i];out<<"{\"transition_index\":"<<v.transition_index;comparison(out,v);out<<'}';}
 out<<"],\"unknown_activities\":[";for(std::size_t i=0;i<r.unknown_activities.size();++i){if(i)out<<',';const auto& v=r.unknown_activities[i];out<<"{\"activity\":"<<serialize_embedding_workbench_string_json(v.activity)<<",\"activity_is_null\":"<<(v.activity_is_null?"true":"false");comparison(out,v);out<<'}';}out<<"]}";return out.str();
}
std::string serialize_replay_duration_workbench_result_json(const EventLog& log,const PetriNet* net,const ReplayDurationWorkbenchInput& input,const TokenReplayOptions& options){
 TokenReplayOptions selected=options;if(input.activity_attribute)selected.activity_attribute=*input.activity_attribute;if(input.timestamp_attribute)selected.timestamp_attribute=*input.timestamp_attribute;
 std::ostringstream out;out.imbue(std::locale::classic());out<<"{\"duration_diagnostics\":";
 if(input.mode=="cohorts"){
  TokenReplayDurationDiagnostics r;r.timestamp_attribute=selected.timestamp_attribute;r.transitions=token_replay_transition_duration_diagnostics(log,input.transition_cohorts,selected);r.unknown_activities=token_replay_unknown_activity_duration_diagnostics(log,input.unknown_activity_cohorts,selected);
  out<<serialize_replay_duration_diagnostics_json(r)<<",\"case_duration_seconds\":null,\"median_duration_seconds\":null}";
 }else if(input.mode=="saved_diagnostics"){
  if(!net)throw std::invalid_argument("saved diagnostic reanalysis requires the original PNML model");
  if(!input.has_saved_diagnostics)throw std::invalid_argument("saved diagnostic reanalysis requires saved diagnostics");
  out<<serialize_replay_duration_diagnostics_json(compute_token_replay_duration_diagnostics(log,*net,input.saved_diagnostics,selected))<<",\"case_duration_seconds\":null,\"median_duration_seconds\":null}";
 }else if(input.mode=="case_duration"){
  if(input.trace_index>=log.traces.size())throw std::out_of_range("token replay duration: trace index out of range");
  out<<"null,\"case_duration_seconds\":";real(out,token_replay_case_duration(log.traces[input.trace_index],selected.timestamp_attribute));out<<",\"median_duration_seconds\":null}";
 }else if(input.mode=="median_duration"){
  out<<"null,\"case_duration_seconds\":null,\"median_duration_seconds\":";real(out,token_replay_median_case_duration(log,input.trace_indices,selected.timestamp_attribute));out<<'}';
 }else throw std::invalid_argument("unknown duration mode");return out.str();
}
} // namespace minera
