#include "minera/literal_controls_json.hpp"
#include "minera/genetic_matrix_workbench_json.hpp"
#include "minera/object_centric_playout_workbench_json.hpp"
#include "minera/embedding_workbench_json.hpp"
#include <algorithm>
#include <tuple>
#include <type_traits>
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
#include <utility>
#include <variant>
namespace minera { namespace {
[[noreturn]] void invalid(std::string message) {
  throw std::invalid_argument("Literal controls JSON: " + message);
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
std::size_t size_value(const Json& v){const auto* n=std::get_if<Number>(&v.value);if(!n||n->text.empty()||!std::all_of(n->text.begin(),n->text.end(),[](char c){return c>='0'&&c<='9';}))invalid("expected size_t integer token");std::size_t out=0;const auto parsed=std::from_chars(n->text.data(),n->text.data()+n->text.size(),out);if(parsed.ec!=std::errc{}||parsed.ptr!=n->text.data()+n->text.size())invalid("integer exceeds native size_t");return out;}
double real_value(const Json& v){
 if(const auto* s=std::get_if<std::string>(&v.value)){if(*s=="NaN")return std::numeric_limits<double>::quiet_NaN();if(*s=="INF")return std::numeric_limits<double>::infinity();if(*s=="-INF")return -std::numeric_limits<double>::infinity();invalid("unknown IEEE double tag");}
 const auto* n=std::get_if<Number>(&v.value);if(!n)invalid("expected double or IEEE tag");double out=0;const auto parsed=std::from_chars(n->text.data(),n->text.data()+n->text.size(),out);if(parsed.ec!=std::errc{}||parsed.ptr!=n->text.data()+n->text.size()||!std::isfinite(out))invalid("numeric token exceeds finite double storage");return out;
}
void members(const Json::Object& o,std::set<std::string>* supplied){if(!supplied)return;std::set<std::string> actual;for(const auto& [name,v]:o){(void)v;actual.insert(name);}*supplied=std::move(actual);}
bool has(const FieldLookup& lookup,const FieldPresence& present,std::string_view key){return present?present(key):!lookup(key).empty();}
void conflict(const FieldLookup& lookup,const FieldPresence& present,std::string_view exact,std::initializer_list<std::string_view> legacy){if(!has(lookup,present,exact))return;for(const auto name:legacy)if(has(lookup,present,name))throw std::invalid_argument(std::string(exact)+": cannot combine with "+std::string(name));}
void real_json(std::ostream& out,double v){if(std::isnan(v))out<<"\"NaN\"";else if(std::isinf(v))out<<(v<0?"\"-INF\"":"\"INF\"");else if(v==0&&std::signbit(v))out<<"-0.0";else{std::ostringstream scalar;scalar.imbue(std::locale::classic());scalar<<std::setprecision(std::numeric_limits<double>::max_digits10)<<v;auto text=scalar.str();if(text.find_first_of(".eE")==std::string::npos)text+=".0";out<<text;}}
template<class T,class M>struct Member{std::string_view name;M T::* pointer;};
template<class T,class M>auto member(std::string_view name,M T::* pointer){return Member<T,M>{name,pointer};}
template<class T>struct Record;
#define F(name) member(#name,&V::name)
#define RECORD(Type,...) template<>struct Record<Type>{static auto fields(){using V=Type;return std::make_tuple(__VA_ARGS__);}};
RECORD(LogToTargetOptions,F(variant),F(activity_key),F(timestamp_key),F(enable_padding),F(pad_size),F(activities))
RECORD(LogToFeaturesOptions,F(variant),F(activity_key),F(timestamp_key),F(start_timestamp_key),F(resource_key),F(window_seconds),F(grouper_freq),F(event_string_attributes),F(event_numeric_attributes),F(case_string_attributes),F(case_numeric_attributes),F(event_attributes),F(case_attributes),F(event_successor_attributes),F(enable_activity_def_representation),F(enable_succ_def_representation),F(enable_all_extra_features),F(enable_case_duration),F(add_case_identifier_column),F(enable_times_from_first_occurrence),F(enable_times_from_last_occurrence),F(enable_direct_paths_times_last_occ),F(enable_indirect_paths_times_last_occ),F(enable_work_in_progress),F(enable_resource_workload),F(enable_first_last_activity_index),F(enable_max_concurrent_events),F(enable_max_concurrent_events_per_activity),F(epsilon),F(min_num_different_string_values),F(max_num_different_string_values),F(feature_names),F(default_not_present),F(event_string_attributes_supplied),F(event_numeric_attributes_supplied),F(feature_names_supplied),F(case_string_attributes_supplied),F(case_numeric_attributes_supplied),F(event_successor_attributes_supplied),F(enable_case_duration_supplied),F(enable_times_from_first_occurrence_supplied),F(enable_times_from_last_occurrence_supplied),F(enable_direct_paths_times_last_occ_supplied),F(enable_indirect_paths_times_last_occ_supplied),F(enable_work_in_progress_supplied),F(enable_resource_workload_supplied),F(enable_first_last_activity_index_supplied),F(enable_max_concurrent_events_supplied),F(enable_max_concurrent_events_per_activity_supplied),F(case_id_key),F(arrival_rate),F(finish_rate),F(service_time),F(waiting_time),F(sojourn_time),F(diff_start_end))
RECORD(ILPMinerOptions,F(activity_key),F(artificial_start_activity),F(artificial_end_activity),F(alpha),F(noise_threshold),F(causal_relation),F(max_nodes))
RECORD(DfgPrecisionOptions,F(activity_attribute),F(case_id_attribute),F(max_rows_to_materialize))
RECORD(LogToTargetResult,F(variant),F(rows),F(classes),F(trace_count),F(target_width),F(padded))
RECORD(TemporalFeatureRow,F(timestamp),F(values),F(timestamp_ns))
RECORD(LogToFeaturesResult,F(variant),F(feature_names),F(case_ids),F(event_rows),F(trace_rows),F(temporal_rows))
RECORD(ILPMinerDiagnostics,F(computed),F(exhausted),F(visited_nodes),F(lp_solved),F(branch_and_bound_nodes),F(message))
RECORD(ILPMinerResult,F(net),F(markings),F(diagnostics))
RECORD(DfgPrecisionResult,F(computed),F(precision),F(prefix_occurrences),F(allowed_prefix_occurrences),F(activated_transitions),F(escaping_transitions),F(error))
#undef RECORD
#undef F
template<class T>struct Vector:std::false_type{};
template<class T,class A>struct Vector<std::vector<T,A>>:std::true_type{};
template<class T>struct Optional:std::false_type{};
template<class T>struct Optional<std::optional<T>>:std::true_type{};
template<class T>struct Pair:std::false_type{};
template<class A,class B>struct Pair<std::pair<A,B>>:std::true_type{};
template<class T>struct Variant:std::false_type{};
template<class... T>struct Variant<std::variant<T...>>:std::true_type{};
template<class T>void write(std::ostream&,const T&);
template<class T>void record_write(std::ostream& out,const T& v){out<<'{';bool first=true;std::apply([&](const auto&... e){([&]{if(!first)out<<',';first=false;out<<serialize_embedding_workbench_string_json(e.name)<<':';write(out,v.*(e.pointer));}(),...);},Record<T>::fields());out<<'}';}
std::string_view variant_name(LogToTargetVariant v){switch(v){case LogToTargetVariant::NEXT_ACTIVITY:return "next_activity";case LogToTargetVariant::NEXT_TIME:return "next_time";case LogToTargetVariant::REMAINING_TIME:return "remaining_time";default:return {};}}
std::string_view variant_name(LogToFeaturesVariant v){switch(v){case LogToFeaturesVariant::EVENT_BASED:return "event_based";case LogToFeaturesVariant::TRACE_BASED:return "trace_based";case LogToFeaturesVariant::TEMPORAL:return "temporal";default:return {};}}
template<class T>void write(std::ostream& out,const T& v){
 if constexpr(std::is_same_v<T,bool>)out<<(v?"true":"false");
 else if constexpr(std::is_same_v<T,std::string>)out<<serialize_embedding_workbench_string_json(v);
 else if constexpr(std::is_same_v<T,double>)real_json(out,v);
 else if constexpr(std::is_same_v<T,LogToTargetVariant>||std::is_same_v<T,LogToFeaturesVariant>){const auto name=variant_name(v);out<<"{\"name\":";if(name.empty())out<<"null";else out<<serialize_embedding_workbench_string_json(name);out<<",\"index\":"<<static_cast<std::int32_t>(v)<<'}';}
 else if constexpr(std::is_integral_v<T>)out<<v;
 else if constexpr(Vector<T>::value){out<<'[';bool first=true;for(const auto& x:v){if(!first)out<<',';first=false;write(out,x);}out<<']';}
 else if constexpr(Optional<T>::value){if(v)write(out,*v);else out<<"null";}
 else if constexpr(Pair<T>::value){out<<'[';write(out,v.first);out<<',';write(out,v.second);out<<']';}
 else if constexpr(Variant<T>::value){out<<"{\"index\":"<<v.index()<<",\"value\":";std::visit([&](const auto& x){write(out,x);},v);out<<'}';}
 else if constexpr(std::is_same_v<T,EventAttribute>){EventLog carrier;Trace trace;trace.attributes.push_back(v);carrier.traces.push_back(std::move(trace));out<<"{\"native_schema\":\"event-attribute-via-raw-log-v1\",\"raw_log\":"<<serialize_embedding_workbench_raw_log_json(carrier)<<'}';}
 else if constexpr(std::is_same_v<T,PetriNet>)out<<serialize_oc_playout_workbench_petri_net_json(v);
 else if constexpr(std::is_same_v<T,WorkflowMarkings>)out<<serialize_genetic_matrix_workbench_markings_json(v);
 else record_write(out,v);
}
template<class T>std::string render(const T& v){std::ostringstream out;out.imbue(std::locale::classic());write(out,v);return out.str();}
bool scalar_utf8(std::string_view bytes){for(std::size_t i=0;i<bytes.size();){const auto a=static_cast<unsigned char>(bytes[i++]);if(a<128)continue;const int n=a>=0xc2&&a<=0xdf?1:a>=0xe0&&a<=0xef?2:a>=0xf0&&a<=0xf4?3:-1;if(n<0||i+static_cast<std::size_t>(n)>bytes.size())return false;unsigned code=a&(n==1?31:n==2?15:7);for(int j=0;j<n;++j){const auto b=static_cast<unsigned char>(bytes[i++]);if((b&192)!=128)return false;code=(code<<6)|(b&63);}if((n==1&&code<128)||(n==2&&code<2048)||(n==3&&code<65536)||code>0x10ffff||(code>=0xd800&&code<=0xdfff))return false;}return true;}
}  // namespace

bool literal_controls_requested(std::string_view group,const FieldPresence& present){
 if(!present)return false;
 if(group=="target")return present("target_activities_json");
 if(group=="ilp")return present("ilp_options_json");
 if(group=="dfg_precision")return present("dfg_precision_options_json");
 if(group=="features")for(const auto* name:{"features_event_string_attributes_json","features_event_numeric_attributes_json","features_case_string_attributes_json","features_case_numeric_attributes_json","features_event_attributes_json","features_case_attributes_json","features_event_successor_attributes_json","features_feature_names_json"})if(present(name))return true;
 return false;
}
std::vector<std::string> parse_literal_string_vector_json(std::string_view input){return strings(Parser(input,true).parse());}
ILPMinerOptions parse_literal_ilp_options_json(std::string_view input,std::set<std::string>* supplied){
 const auto document=Parser(input,true).parse();const auto& o=document.object();allowed(o,{"activity_key","artificial_start_activity","artificial_end_activity","alpha","noise_threshold","causal_relation","max_nodes"});ILPMinerOptions r;
 if(const auto* v=field(o,"activity_key"))r.activity_key=native_string(*v);
 if(const auto* v=field(o,"artificial_start_activity"))r.artificial_start_activity=native_string(*v);
 if(const auto* v=field(o,"artificial_end_activity"))r.artificial_end_activity=native_string(*v);
 if(const auto* v=field(o,"alpha"))r.alpha=real_value(*v);
 if(const auto* v=field(o,"noise_threshold"))r.noise_threshold=real_value(*v);
 if(const auto* v=field(o,"max_nodes"))r.max_nodes=size_value(*v);
 if(const auto* v=field(o,"causal_relation"))for(const auto& item:v->array()){const auto& pair=item.array();if(pair.size()!=2)invalid("causal_relation requires exact pairs");r.causal_relation.emplace_back(native_string(pair[0]),native_string(pair[1]));}
 members(o,supplied);return r;
}
DfgPrecisionOptions parse_literal_dfg_precision_options_json(std::string_view input,std::set<std::string>* supplied){const auto document=Parser(input,true).parse();const auto& o=document.object();allowed(o,{"activity_attribute","case_id_attribute","max_rows_to_materialize"});DfgPrecisionOptions r;if(const auto* v=field(o,"activity_attribute"))r.activity_attribute=native_string(*v);if(const auto* v=field(o,"case_id_attribute"))r.case_id_attribute=native_string(*v);if(const auto* v=field(o,"max_rows_to_materialize"))if(!v->null())r.max_rows_to_materialize=size_value(*v);members(o,supplied);return r;}
LogToTargetOptions parse_log_to_target_literal_params(const FieldLookup& lookup,const FieldPresence& present){conflict(lookup,present,"target_activities_json",{"target_activities"});auto r=parse_log_to_target_params(lookup,present);if(has(lookup,present,"target_activities_json"))r.activities=parse_literal_string_vector_json(lookup("target_activities_json"));return r;}
LogToFeaturesOptions parse_log_to_features_literal_params(const FieldLookup& lookup,const FieldPresence& present,std::vector<std::string>* parsed_controls){
 static constexpr std::string_view lists[]={"event_string_attributes","event_numeric_attributes","case_string_attributes","case_numeric_attributes","event_attributes","case_attributes","event_successor_attributes","feature_names"};
 for(const auto suffix:lists){const auto exact="features_"+std::string(suffix)+"_json",plural="features_"+std::string(suffix),singular="feature_"+std::string(suffix);conflict(lookup,present,exact,{plural,singular});}
 auto r=parse_log_to_features_params(lookup,present);
 const auto assign=[&](std::string_view suffix,std::vector<std::string>& value,bool* flag){const auto key="features_"+std::string(suffix)+"_json";if(has(lookup,present,key)){auto parsed=parse_literal_string_vector_json(lookup(key));value=std::move(parsed);if(flag)*flag=true;if(parsed_controls)parsed_controls->push_back(key);}};
 assign("event_string_attributes",r.event_string_attributes,&r.event_string_attributes_supplied);assign("event_numeric_attributes",r.event_numeric_attributes,&r.event_numeric_attributes_supplied);
 assign("case_string_attributes",r.case_string_attributes,&r.case_string_attributes_supplied);assign("case_numeric_attributes",r.case_numeric_attributes,&r.case_numeric_attributes_supplied);
 assign("event_attributes",r.event_attributes,nullptr);assign("case_attributes",r.case_attributes,nullptr);assign("event_successor_attributes",r.event_successor_attributes,&r.event_successor_attributes_supplied);assign("feature_names",r.feature_names,&r.feature_names_supplied);return r;
}
ILPMinerOptions parse_ilp_literal_params(const FieldLookup& lookup,const FieldPresence& present,std::set<std::string>* supplied){conflict(lookup,present,"ilp_options_json",{"ilp_activity_key","ilp_artificial_start_activity","ilp_artificial_end_activity","ilp_alpha","ilp_noise_threshold","ilp_causal_relation","ilp_max_nodes"});if(has(lookup,present,"ilp_options_json"))return parse_literal_ilp_options_json(lookup("ilp_options_json"),supplied);auto r=parse_ilp_miner_params(lookup,present);if(supplied)supplied->clear();return r;}
#define OPTIONS(Type) std::string serialize_literal_controls_options_json(const Type& value){return render(value);}
OPTIONS(LogToTargetOptions) OPTIONS(LogToFeaturesOptions) OPTIONS(ILPMinerOptions) OPTIONS(DfgPrecisionOptions)
#undef OPTIONS
#define RESULT(Type) std::string serialize_literal_controls_result_json(const Type& value){return render(value);}
RESULT(LogToTargetResult) RESULT(LogToFeaturesResult) RESULT(ILPMinerResult) RESULT(DfgPrecisionResult)
#undef RESULT
std::optional<std::string> literal_controls_transport_error(std::string_view bytes){if(!scalar_utf8(bytes))return "non_scalar_utf8";try{(void)Parser(bytes,true,false).parse().object();return std::nullopt;}catch(const std::exception&){return "invalid_json";}}
}  // namespace minera
