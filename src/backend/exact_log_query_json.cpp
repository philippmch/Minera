#include "minera/exact_log_query_json.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/literal_controls_json.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <locale>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>

namespace minera { namespace {
[[noreturn]] void invalid(std::string message) {
  throw std::invalid_argument("Exact log query JSON: " + message);
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
std::string raw_json(const Json& value) {
  if (value.null()) return "null";
  if (const auto* v = std::get_if<bool>(&value.value)) return *v ? "true" : "false";
  if (const auto* v = std::get_if<Number>(&value.value)) return v->text;
  if (const auto* v = std::get_if<std::string>(&value.value)) return q(*v);
  std::string out;
  if (const auto* values = std::get_if<Json::Array>(&value.value)) {
    out = "[";
    for (const auto& item : *values) {
      if (out.size() > 1) out += ',';
      out += raw_json(item);
    }
    return out + ']';
  }
  out = "{";
  for (const auto& [key, item] : value.object()) {
    if (out.size() > 1) out += ',';
    out += q(key) + ':' + raw_json(item);
  }
  return out + '}';
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
std::string real_json(double value) {
  if (std::isnan(value)) return q("NaN");
  if (std::isinf(value)) return q(value<0?"-INF":"INF");
  std::ostringstream out;
  out.imbue(std::locale::classic());
  out.precision(std::numeric_limits<double>::max_digits10);
  out<<value;
  return out.str();
}

QueryValue parameter(const Json& j) {
  const auto& o=j.object();exact_fields(o,{"index","value"});
  const auto index=integer<unsigned>(required(o,"index"));const auto& value=required(o,"value");
  switch(index) {
    case 0:if(!value.null())invalid("monostate parameter requires null");return std::monostate{};
    case 1:return integer<std::int64_t>(value);
    case 2:return real(value);
    case 3:return value.string();
    case 4:{const auto& b=value.object();exact_fields(b,{"hex"});return QueryBlob{required(b,"hex").string()};}
    default:invalid("QueryValue index must be0through4");
  }
}
std::string parts_json(const std::vector<DiscoveryWorkbenchPart>& parts) {
  std::string s="[";
  for(const auto& p:parts){if(s.size()>1)s+=',';s+="{\"name\":"+q(p.name)+",\"filename\":"+(p.file?q(p.filename):"null")+",\"bytes\":"+q(p.bytes)+",\"file\":"+(p.file?"true":"false")+'}';}
  return s+']';
}
std::string object_json(const std::map<std::string,std::string>& fields) {
  std::string s="{";for(const auto& [k,v]:fields){if(s.size()>1)s+=',';s+=q(k)+':'+v;}return s+'}';
}
std::string call_json(const ExactQueryCall& c,std::string_view backend={},bool query=false) {
  return "{"+std::string(query?"\"backend\":"+(backend.empty()?std::string("null"):q(backend))+",":"")+
      "\"entered\":"+(!c.known?"null":c.entered?"true":"false")+",\"returned\":"+(!c.known?"null":c.returned?"true":"false")+",\"error\":"+c.error+'}';
}
std::string context(const DiscoveryWorkbenchPart* p,std::string_view mode) {
  return "{\"mode\":"+(mode.empty()?std::string("null"):q(mode))+",\"filename\":"+(p&&p->file?q(p->filename):"null")+",\"bytes\":"+(p?q(p->bytes):"null")+'}';
}
template<class F> void optional(ExactLogQueryReport& r,const std::string& name,F f) {
  try{r.snapshots[name]=f();}catch(const std::exception& e){r.optional_errors[name]=q(e.what());}
}
} // namespace

std::string exact_query_exception_type(const std::exception& e){
  if(dynamic_cast<const std::invalid_argument*>(&e))return "invalid_argument";
  if(dynamic_cast<const std::out_of_range*>(&e))return "out_of_range";
  if(dynamic_cast<const std::runtime_error*>(&e))return "runtime_error";
  return "std_exception";
}
std::string exact_query_string(std::string_view v){return q(v);}
std::string exact_query_error(std::string_view type,std::string_view message){return "{\"type\":"+q(type)+",\"message\":"+q(message)+'}';}
ExactLogQueryOptions parse_exact_log_query_options_json(std::string_view input,std::string_view engine) {
  if(engine!="sqlite"&&engine!="duckdb")invalid("query engine must be sqlite or duckdb");
  const auto j=Parser(input).parse();const auto& o=j.object();
  if(engine=="duckdb"){
    if(o.contains("max_vm_steps"))invalid("max_vm_steps is SQLite-only");
    exact_fields(o,{"table_name","max_rows","parameters"});
  }else exact_fields(o,{"table_name","max_rows","max_vm_steps","parameters"});
  ExactLogQueryOptions result;result.engine=engine;result.value.table_name=required(o,"table_name").string();
  result.value.max_rows=integer<std::size_t>(required(o,"max_rows"));
  if(engine=="sqlite")result.value.max_vm_steps=integer<std::size_t>(required(o,"max_vm_steps"));
  for(const auto& v:required(o,"parameters").array())result.value.parameters.push_back(parameter(v));
  return result;
}
std::string serialize_exact_log_query_value_json(const QueryValue& v) {
  std::string value;
  switch(v.index()){
    case 0:value="null";break;
    case 1:value=q(std::to_string(std::get<std::int64_t>(v)));break;
    case 2:value=real_json(std::get<double>(v));break;
    case 3:value=q(std::get<std::string>(v));break;
    default:value="{\"hex\":"+q(std::get<QueryBlob>(v).hex)+'}';break;
  }
  return "{\"index\":"+std::to_string(v.index())+",\"value\":"+value+'}';
}
std::string serialize_exact_log_query_options_json(const ExactLogQueryOptions& options) {
  const auto& o=options.value;std::string s="{\"table_name\":"+q(o.table_name)+",\"max_rows\":"+std::to_string(o.max_rows);
  if(options.engine=="sqlite")s+=",\"max_vm_steps\":"+std::to_string(o.max_vm_steps);
  s+=",\"parameters\":[";for(std::size_t i=0;i<o.parameters.size();++i){if(i)s+=',';s+=serialize_exact_log_query_value_json(o.parameters[i]);}return s+"]}";
}
std::string serialize_exact_log_query_result_json(const LogQueryResult& r) {
  std::string s="{\"engine\":"+q(r.engine)+",\"sql\":"+q(r.sql)+",\"event_index_column\":"+q(r.event_index_column)+",\"columns\":[";
  for(std::size_t i=0;i<r.columns.size();++i){if(i)s+=',';s+=q(r.columns[i]);}s+="],\"rows\":[";
  for(std::size_t i=0;i<r.rows.size();++i){if(i)s+=',';s+='[';for(std::size_t j=0;j<r.rows[i].size();++j){if(j)s+=',';s+=serialize_exact_log_query_value_json(r.rows[i][j]);}s+=']';}
  s+="],\"output_truncated\":"+std::string(r.output_truncated?"true":"false")+",\"can_filter\":"+(r.can_filter?"true":"false")+",\"selected_event_indices\":[";
  for(std::size_t i=0;i<r.selected_event_indices.size();++i){if(i)s+=',';s+=std::to_string(r.selected_event_indices[i]);}return s+"]}";
}
std::string serialize_exact_log_query_table_json(const LogQueryTable& t){return "{\"type\":\"LogQueryTable\",\"value\":{\"table\":"+serialize_embedding_workbench_table_json(t.table)+",\"event_index_column\":"+q(t.event_index_column)+"}}";}
ExactLogQueryReport::ExactLogQueryReport(std::string op):operation(std::move(op)) {
  for(const auto* key:{"source_before","source_after","options_before","options_after","prepared_log_before","prepared_log_after"})snapshots[key]="null";
  for(const auto* key:{"source_before","source_after","options_before","options_after","prepared_log_before","prepared_log_after","result","legacy_transport"})optional_errors[key]="null";
  for(const auto* key:{"canonical_source_json","canonical_options_json","canonical_result_json","legacy_result_json","filtered_log_json","filtered_table_json","export_bytes"})artifacts[key]={};
}
void ExactLogQueryReport::fail(std::string_view phase,std::string_view message,std::string_view type){status=400;if(error=="null")error="{\"phase\":"+q(phase)+",\"type\":"+q(type)+",\"message\":"+q(message)+'}';}
std::string ExactLogQueryReport::json()const {
  std::string artifacts_json="{";for(const auto& [k,v]:artifacts){if(artifacts_json.size()>1)artifacts_json+=',';artifacts_json+=q(k)+":{\"bytes\":"+(v.bytes?q(*v.bytes):"null")+",\"error\":"+(v.error?q(*v.error):"null")+'}';}artifacts_json+='}';
  std::string unchanged="{";for(const auto* key:{"source","options","prepared_log"}){if(unchanged.size()>1)unchanged+=',';const auto& a=snapshots.at(std::string(key)+"_before");const auto& b=snapshots.at(std::string(key)+"_after");unchanged+=q(key)+':'+(a=="null"||b=="null"?"null":a==b?"true":"false");}unchanged+='}';
  return "{\"family\":\"EXACT_LOG_QUERY\",\"operation\":"+q(operation)+",\"http_status\":"+std::to_string(status)+",\"engine\":"+(engine.empty()?"null":q(engine))+",\"source_id\":"+(source_id.empty()?"null":q(source_id))+
    ",\"input_order\":"+q(input_order)+",\"original_inputs\":"+parts_json(original_inputs)+",\"query_inputs\":"+parts_json(query_inputs)+
    ",\"input_context\":{\"sql\":"+sql_context+",\"options\":"+options_context+",\"source\":"+source_context+"},\"configured_options\":"+configured_options+
    ",\"snapshots\":"+object_json(snapshots)+",\"unchanged\":"+unchanged+",\"result\":"+result+",\"selection\":"+selection+",\"filtered\":"+filtered+",\"output\":"+output+
    ",\"calls\":{\"query\":"+call_json(query_call,engine,true)+",\"filter\":"+call_json(filter_call)+",\"export\":"+call_json(export_call)+"},\"error\":"+error+",\"artifacts\":"+artifacts_json+",\"optional_errors\":"+object_json(optional_errors)+",\"cli\":"+cli+'}';
}
ExactLogQueryRequest prepare_exact_log_query(ExactLogQueryReport& r,const std::vector<DiscoveryWorkbenchPart>& parts,const std::vector<DiscoveryWorkbenchPart>& query,bool require_id,std::string_view transport_error) {
  r.original_inputs=parts;r.query_inputs=query;std::map<std::string,std::vector<const DiscoveryWorkbenchPart*>> fields;
  for(const auto& p:parts)fields[p.name].push_back(&p);for(const auto& p:query)fields[p.name].push_back(&p);
  const auto unique=[&](const std::string& k)->const DiscoveryWorkbenchPart*{const auto i=fields.find(k);return i!=fields.end()&&i->second.size()==1?i->second.front():nullptr;};
  const auto has=[&](const std::string& k){return fields.contains(k);};
  const auto* e=unique("engine");if(!has("engine"))r.engine="sqlite";else if(e&&(e->bytes=="sqlite"||e->bytes=="duckdb"))r.engine=e->bytes;
  const auto* sid=unique("source_id");
  const auto* sql=unique("sql");const auto* sf=unique("sql_file");const auto* opt=unique("query_options_json");const auto* of=unique("query_options_file");
  r.sql_context=context(has("sql")&&has("sql_file")?nullptr:sql?sql:sf,has("sql")&&has("sql_file")?"":has("sql")?"inline":has("sql_file")?"file":"");
  r.options_context=context(has("query_options_json")&&has("query_options_file")?nullptr:opt?opt:of,has("query_options_json")&&has("query_options_file")?"":has("query_options_json")?"inline_json":has("query_options_file")?"file":"");
  try{
    if(!transport_error.empty())throw std::invalid_argument(std::string(transport_error));
  }catch(const std::exception& x){r.fail("transport",x.what());throw;}
  try{
    static const std::set<std::string> allowed={"engine","source_id","sql","sql_file","query_options_json","query_options_file","limit","mode","format"};
    for(const auto& [key,v]:fields){if(!allowed.contains(key))invalid("unknown exact query control: "+key);if(v.size()!=1)invalid("duplicate exact query control: "+key);}
    if(r.engine.empty())invalid("query engine must be sqlite or duckdb");
    if(has("limit"))invalid("full query options conflict with limit by presence");
    if(require_id&&!sid)invalid("source_id is required for exact query");
    if(has("sql")==has("sql_file"))invalid("supply exactly one sql or sql_file");
    if(has("query_options_json")==has("query_options_file"))invalid("supply exactly one query_options_json or query_options_file");
    for(const auto& p:query)if(p.name=="sql_file"||p.name=="query_options_file")invalid(p.name+" cannot be a query control");
    if(sf&&!sf->file)invalid("sql_file requires actual filename presence");if(of&&!of->file)invalid("query_options_file requires actual filename presence");
    if(r.operation!="export"&&(has("mode")||has("format")))invalid("mode and format apply only to export");
    ExactLogQueryRequest result;result.source_id=sid?sid->bytes:"";result.sql=(sql?sql:sf)->bytes;
    if(result.sql.size()>1024u*1024u)invalid("SQL must be within 1 MiB");
    if(const auto* p=unique("mode"))result.mode=p->bytes;if(const auto* p=unique("format"))result.format=p->bytes;
    if(result.mode!="cases"&&result.mode!="events")invalid("filter mode must be cases or events");if(result.format!="xes"&&result.format!="json")invalid("filter export format must be xes or json");
    try{result.options=parse_exact_log_query_options_json((opt?opt:of)->bytes,r.engine);}catch(const std::exception& x){r.fail("options_parse",x.what());throw;}
    const auto canonical=serialize_exact_log_query_options_json(result.options);r.artifacts["canonical_options_json"].bytes=canonical;
    r.configured_options="{\"engine\":"+q(r.engine)+",\"value\":"+canonical+'}';r.snapshots["options_before"]=r.configured_options;r.snapshots["options_after"]=r.configured_options;
    return result;
  }catch(const std::exception& x){r.fail("control",x.what());throw;}
}
void record_exact_query_options(ExactLogQueryReport& r,const ExactLogQueryOptions& value,bool after){
  optional(r,after?"options_after":"options_before",[&]{return "{\"engine\":"+q(value.engine)+",\"value\":"+serialize_exact_log_query_options_json(value)+'}';});
}
void record_exact_query_source(ExactLogQueryReport& r,const LogQueryTable& t,bool after){optional(r,after?"source_after":"source_before",[&]{return serialize_exact_log_query_table_json(t);});if(!after&&r.snapshots["source_before"]!="null")r.artifacts["canonical_source_json"].bytes=r.snapshots["source_before"];}
void record_exact_query_log(ExactLogQueryReport& r,const EventLog& log,bool after){optional(r,after?"prepared_log_after":"prepared_log_before",[&]{return "{\"type\":\"EventLog\",\"value\":"+serialize_embedding_workbench_raw_log_json(log)+'}';});}
LogQueryResult execute_exact_sqlite_query(ExactLogQueryReport& r,const LogQueryTable& table,std::string_view sql,const ExactLogQueryOptions& options){
  record_exact_query_source(r,table);r.query_call.entered=true;
  try{
    auto value=query_log_sqlite(table,sql,options.value);r.query_call.returned=true;record_exact_query_source(r,table,true);
    try{const auto canonical=serialize_exact_log_query_result_json(value);r.result="{\"type\":\"LogQueryResult\",\"value\":"+canonical+'}';r.artifacts["canonical_result_json"].bytes=canonical;}catch(const std::exception& e){r.optional_errors["result"]=q(e.what());r.result="{\"type\":\"LogQueryResult\",\"value\":null}";}
    try{auto bytes=log_query_json(value);if(auto error=literal_controls_transport_error(bytes))r.optional_errors["legacy_transport"]=q(*error);r.artifacts["legacy_result_json"].bytes=std::move(bytes);}catch(const std::exception& e){r.artifacts["legacy_result_json"].error=e.what();}
    return value;
  }catch(const std::exception& e){record_exact_query_source(r,table,true);r.query_call.error=exact_query_error(exact_query_exception_type(e),e.what());r.fail("query",e.what(),exact_query_exception_type(e));throw;}
}
ExactQuerySelection parse_exact_duck_query_selection(std::string_view body,std::size_t count){
  const auto value=Parser(body).parse();const auto& o=value.object();ExactQuerySelection result;
  result.can_filter=required(o,"can_filter").boolean();result.output_truncated=required(o,"output_truncated").boolean();
  for(const auto& item:required(o,"selected_event_indices").array()){auto i=integer<std::size_t>(item);if(i>=count)invalid("query selection contains an invalid event index");result.indices.push_back(i);}
  if((!result.can_filter||result.output_truncated)&&!result.indices.empty())invalid("incomplete query cannot retain selection indices");
  return result;
}
void record_exact_duck_query(ExactLogQueryReport& r,std::string_view body,int status){
  r.artifacts["legacy_result_json"].bytes=std::string(body);
  r.query_call.known=false;
  const auto value=Parser(body).parse();const auto& o=value.object();
  const auto& call=required(o,"exact_call").object();exact_fields(call,{"entered","returned","error"});
  const bool entered=required(call,"entered").boolean(),returned=required(call,"returned").boolean();
  if(returned&&!entered)invalid("invalid exact runtime call flags");
  const auto& error=required(call,"error");
  if(!error.null()){const auto& fields=error.object();exact_fields(fields,{"type","message"});r.query_call.error=exact_query_error(required(fields,"type").string(),required(fields,"message").string());}
  r.query_call.entered=entered;r.query_call.returned=returned;r.query_call.known=true;
  if(status!=200||!returned){r.fail("runtime",body,"runtime_error");throw std::runtime_error(std::string(body));}
  // The observed remote return is authoritative even if an optional writer
  // subsequently fails. It is never inferred from the HTTP status alone.
  try{const auto canonical=raw_json(value);r.result="{\"type\":\"DuckDbQueryResult\",\"value\":"+canonical+'}';r.artifacts["canonical_result_json"].bytes=canonical;}
  catch(const std::exception& e){r.optional_errors["result"]=q(e.what());r.result="{\"type\":\"DuckDbQueryResult\",\"value\":null}";}
}

std::string exact_duck_query_body(const LogQueryTable& table,std::string_view sql,const ExactLogQueryOptions& options,std::string_view source){
  const auto body="{\"sql\":"+q(sql)+",\"source_id\":"+q(source)+",\"data\":"+log_query_table_json(table)+",\"exact_options\":"+serialize_exact_log_query_options_json(options)+'}';
  if(auto error=literal_controls_transport_error(body))throw std::invalid_argument("DuckDB source requires scalar UTF8 JSON: "+*error);return body;
}
void record_exact_query_filtered(ExactLogQueryReport& r,const EventLog& value){try{auto bytes=serialize_embedding_workbench_raw_log_json(value);r.filtered="{\"type\":\"EventLog\",\"value\":"+bytes+'}';r.artifacts["filtered_log_json"].bytes=std::move(bytes);}catch(const std::exception& e){r.artifacts["filtered_log_json"].error=e.what();}}
void record_exact_query_filtered(ExactLogQueryReport& r,const EventTable& value){try{auto bytes=serialize_embedding_workbench_table_json(value);r.filtered="{\"type\":\"EventTable\",\"value\":"+bytes+'}';r.artifacts["filtered_table_json"].bytes=std::move(bytes);}catch(const std::exception& e){r.artifacts["filtered_table_json"].error=e.what();}}
void record_exact_query_output(ExactLogQueryReport& r,std::string_view format,std::string_view mime,std::string_view bytes){r.output="{\"format\":"+q(format)+",\"mime\":"+q(mime)+",\"bytes\":"+q(bytes)+'}';r.artifacts["export_bytes"].bytes=bytes;}
} // namespace minera
