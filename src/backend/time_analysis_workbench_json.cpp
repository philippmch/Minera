#include "minera/time_analysis_workbench_json.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/json_export.hpp"
#include "minera/footprints_workbench_json.hpp"
#include "minera/literal_controls_json.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <exception>
#include <limits>
#include <locale>
#include <map>
#include <memory>
#include <new>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
namespace minera {
namespace {
[[noreturn]] void invalid(std::string_view message){throw std::invalid_argument("time-analysis workbench JSON: "+std::string(message));}
std::string q(std::string_view value){return serialize_embedding_workbench_string_json(value);}
std::ostringstream output(){std::ostringstream out;out.imbue(std::locale::classic());return out;}
// A bounded structural index into submitted JSON. Native string/float parsing
// delegates to the existing public codecs; this index never parses output.
struct Json {
  enum class Kind { object, array, string, number, boolean, null } kind;
  std::string_view raw;
  std::map<std::string, Json> fields;
  std::vector<Json> items;
  std::vector<std::string> order;
};
std::string string_token(std::string_view raw) {
  auto values = parse_literal_string_vector_json("[" + std::string(raw) + "]");
  if (values.size() != 1) invalid("expected one JSON string");
  return std::move(values.front());
}
class Parser {
 public:
  explicit Parser(std::string_view text) : text_(text) {
    if (text.size() > 32u * 1024u * 1024u) invalid("request exceeds 32 MiB");
  }
  Json parse() { auto result = value(0); whitespace(); if (at_ != text_.size()) invalid("data after JSON value"); return result; }
 private:
  std::string_view text_; std::size_t at_ = 0, nodes_ = 0;
  void whitespace() { while (at_ < text_.size() && (text_[at_] == ' ' || text_[at_] == '\t' || text_[at_] == '\r' || text_[at_] == '\n')) ++at_; }
  bool take(char c) { whitespace(); if (at_ < text_.size() && text_[at_] == c) { ++at_; return true; } return false; }
  std::string_view quoted() {
    whitespace(); const auto begin = at_;
    if (at_ == text_.size() || text_[at_++] != '"') invalid("expected JSON string");
    bool closed = false;
    while (at_ < text_.size()) {
      const auto c = text_[at_++];
      if (c == '"') { closed = true; break; }
      if (c == '\\') { if (at_ == text_.size()) invalid("short string escape"); ++at_; }
    }
    if (!closed) invalid("unterminated string");
    const auto token = text_.substr(begin, at_ - begin); (void)string_token(token); return token;
  }
  Json value(unsigned depth) {
    whitespace(); if (depth > 64) invalid("nesting exceeds 64 levels");
    if (++nodes_ > 1000000) invalid("request exceeds one million values");
    if (at_ == text_.size()) invalid("missing JSON value");
    const auto begin = at_; Json result{Json::Kind::null, {}, {}, {}};
    if (text_[at_] == '{') {
      result.kind = Json::Kind::object; ++at_;
      if (!take('}')) do {
        const auto key = string_token(quoted()); if (!take(':')) invalid("missing object colon");
        if (!result.fields.emplace(key, value(depth + 1)).second) invalid("duplicate object member");
        result.order.push_back(key);
        if (take('}')) break;
        if (!take(',')) invalid("missing object comma");
      } while (true);
    } else if (text_[at_] == '[') {
      result.kind = Json::Kind::array; ++at_;
      if (!take(']')) do {
        result.items.push_back(value(depth + 1)); if (take(']')) break;
        if (!take(',')) invalid("missing array comma");
      } while (true);
    } else if (text_[at_] == '"') { result.kind = Json::Kind::string; (void)quoted(); }
    else if (text_.substr(at_, 4) == "true" || text_.substr(at_, 5) == "false") {
      result.kind = Json::Kind::boolean; at_ += text_[at_] == 't' ? 4 : 5;
    } else if (text_.substr(at_, 4) == "null") { at_ += 4; }
    else {
      result.kind = Json::Kind::number;
      if (text_[at_] == '-') ++at_;
      if (at_ == text_.size()) invalid("short number");
      if (text_[at_] == '0') ++at_;
      else { if (text_[at_] < '1' || text_[at_] > '9') invalid("invalid JSON value"); while (at_ < text_.size() && text_[at_] >= '0' && text_[at_] <= '9') ++at_; }
      if (at_ < text_.size() && text_[at_] == '.') {
        const auto digits = ++at_; while (at_ < text_.size() && text_[at_] >= '0' && text_[at_] <= '9') ++at_;
        if (digits == at_) invalid("missing fractional digits");
      }
      if (at_ < text_.size() && (text_[at_] == 'e' || text_[at_] == 'E')) {
        ++at_; if (at_ < text_.size() && (text_[at_] == '+' || text_[at_] == '-')) ++at_;
        const auto digits = at_; while (at_ < text_.size() && text_[at_] >= '0' && text_[at_] <= '9') ++at_;
        if (digits == at_) invalid("missing exponent digits");
      }
    }
    result.raw = text_.substr(begin, at_ - begin); return result;
  }
};
const Json* field(const Json& value, std::string_view name) {
  const auto found = value.fields.find(std::string(name)); return found == value.fields.end() ? nullptr : &found->second;
}
template<class T> T integer(const Json& value) {
  if (value.kind != Json::Kind::number || value.raw.empty()) invalid("expected an integer JSON token");
  auto digits = value.raw;
  if constexpr (std::is_signed_v<T>) { if (digits.front() == '-') digits.remove_prefix(1); }
  if (digits.empty() || !std::all_of(digits.begin(), digits.end(), [](char c) { return c >= '0' && c <= '9'; })) invalid("expected an integer JSON token");
  T result{}; const auto parsed = std::from_chars(value.raw.data(), value.raw.data() + value.raw.size(), result);
  if (parsed.ec != std::errc{} || parsed.ptr != value.raw.data() + value.raw.size()) invalid("integer exceeds native storage range");
  return result;
}

void object(const Json& value) { if(value.kind!=Json::Kind::object) invalid("expected object"); }
void allowed(const Json& value,std::initializer_list<std::string_view> names) {
  object(value);for(const auto& [name,item]:value.fields){(void)item;if(std::find(names.begin(),names.end(),name)==names.end())invalid("unknown field "+name);}
}
const Json& required(const Json& value,std::string_view name){object(value);const auto* item=field(value,name);if(!item)invalid("missing field "+std::string(name));return *item;}
std::string native_string(const Json& value){return string_token(value.raw);}
bool boolean(const Json& value){if(value.kind!=Json::Kind::boolean)invalid("expected boolean");return value.raw=="true";}



template<class T>struct Vector:std::false_type{};
template<class T>struct Vector<std::vector<T>>:std::true_type{using Item=T;};
template<class T>struct Map:std::false_type{};
template<class T>struct Map<std::map<std::string,T>>:std::true_type{using Item=T;};
template<class T,class M>struct Member{std::string_view name;M T::*pointer;};
template<class T,class M>auto member(std::string_view name,M T::*pointer){return Member<T,M>{name,pointer};}
template<class T>struct Record;
#define F(name) member(#name,&V::name)
#define RECORD(Type,...) template<>struct Record<Type>{static auto members(){using V=Type;return std::make_tuple(__VA_ARGS__);}};
RECORD(PassedTimeOptions,F(activity),F(variant),F(activity_key),F(timestamp_key),F(start_timestamp_key),F(performance_aggregation),F(use_business_hours),F(business_hour_slots))
RECORD(PassedTimeEntry,F(activity),F(performance),F(frequency))
RECORD(PassedTimeResult,F(variant),F(activity),F(pre),F(post),F(pre_avg_perf),F(post_avg_perf))
RECORD(ServiceTimeOptions,F(activity_key),F(start_timestamp_key),F(timestamp_key),F(aggregation),F(use_business_hours),F(business_hour_slots))
RECORD(ServiceTimeResult,F(aggregation),F(values))
RECORD(CycleTimeOptions,F(timestamp_key),F(start_timestamp_key))
RECORD(CycleTimeResult,F(cycle_time),F(instances),F(timed_events))
RECORD(CaseArrivalOptions,F(timestamp_key),F(use_business_hours),F(business_hour_slots))
RECORD(CaseArrivalResult,F(arrival_average),F(dispersion_average),F(timed_starts),F(timed_ends))
RECORD(CaseStatisticsOptions,F(case_id_key),F(timestamp_key),F(enable_sort),F(sort_by),F(sort_ascending),F(max_cases),F(use_business_hours),F(business_hour_slots))
RECORD(CaseStatisticsEntry,F(case_id),F(start_time),F(end_time),F(case_duration))
RECORD(CaseStatisticsResult,F(options),F(cases))
RECORD(BusinessHourSlot,F(start_seconds),F(end_seconds))
template<class T>struct Enum;
template<>struct Enum<PassedTimeVariant>{static constexpr std::array<std::string_view,3> names={"PRE","POST","PREPOST"};static constexpr std::array<std::string_view,3> aliases={"kPre","kPost","kPrePost"};};
template<>struct Enum<DfgPerformanceAggregation>{static constexpr std::array<std::string_view,8> names={"MEAN","MEDIAN","MIN","MAX","STDEV","SUM","RAW_VALUES","ALL"};static constexpr std::array<std::string_view,8> aliases={"kMean","kMedian","kMinimum","kMaximum","kStandardDeviation","kSum","kRawValues","kAll"};};
template<>struct Enum<CaseStatisticsSortBy>{static constexpr std::array<std::string_view,4> names={"CASE_ID","START_TIME","END_TIME","DURATION"};static constexpr std::array<std::string_view,4> aliases={"kCaseId","kStartTime","kEndTime","kDuration"};};
template<class T>std::string enum_name(T value){const auto index=static_cast<int>(value);return index>=0&&static_cast<std::size_t>(index)<Enum<T>::names.size()?q(Enum<T>::names[static_cast<std::size_t>(index)]):"null";}
template<class T>T read_enum(const Json& value){
 if(value.kind==Json::Kind::number)return static_cast<T>(integer<int>(value));
 if(value.kind==Json::Kind::string){const auto text=native_string(value);for(std::size_t i=0;i<Enum<T>::names.size();++i)if(text==Enum<T>::names[i]||text==Enum<T>::aliases[i])return static_cast<T>(i);invalid("unknown enum spelling");}
 allowed(value,{"name","index"});const auto result=static_cast<T>(integer<int>(required(value,"index")));const auto& name=required(value,"name");if(name.kind!=Json::Kind::null&&read_enum<T>(name)!=result)invalid("enum name/index mismatch");return result;
}
#undef RECORD
#undef F
template<class T>concept HasRecord=requires{Record<T>::members();};
template<class T>T read_value(const Json&);
template<class T>void write(std::ostream&,const T&);
template<class T>std::string render(const T& value){auto out=output();write(out,value);return out.str();}
template<class T>void read_into(T& result,const Json& value){
 object(value);const auto members=Record<T>::members();
 for(const auto& [name,item]:value.fields){(void)item;if(!std::apply([&](const auto&...entry){return ((name==entry.name)||...);},members))invalid("unknown field "+name);}
 std::apply([&](const auto&...entry){([&]{if(const auto* item=field(value,entry.name)){using M=std::remove_cvref_t<decltype(result.*(entry.pointer))>;if constexpr(HasRecord<M>)read_into(result.*(entry.pointer),*item);else result.*(entry.pointer)=read_value<M>(*item);}}(),...);},members);
}
template<class T>T read_record(const Json& value,std::set<std::string>* supplied=nullptr){T result{};read_into(result,value);if(supplied){std::set<std::string> actual;for(const auto& [name,item]:value.fields){(void)item;actual.insert(name);}*supplied=std::move(actual);}return result;}
template<class T>T read_value(const Json& value){
 if constexpr(std::is_same_v<T,bool>)return boolean(value);
 else if constexpr(std::is_enum_v<T>)return read_enum<T>(value);
 else if constexpr(std::is_integral_v<T>)return integer<T>(value);
 else if constexpr(std::is_same_v<T,double>){auto values=parse_embedding_workbench_vector_json("["+std::string(value.raw)+"]");if(values.size()!=1)invalid("expected one double");return values.front();}
 else if constexpr(std::is_same_v<T,std::string>)return native_string(value);
 else if constexpr(Vector<T>::value){if(value.kind!=Json::Kind::array)invalid("expected array");T result;for(const auto& item:value.items)result.push_back(read_value<typename Vector<T>::Item>(item));return result;}
 else if constexpr(Map<T>::value){if(value.kind!=Json::Kind::array)invalid("expected byte-key map entry array");T result;for(const auto& item:value.items){allowed(item,{"key","value"});auto key=native_string(required(item,"key"));auto entry=read_value<typename Map<T>::Item>(required(item,"value"));if(!result.emplace(std::move(key),std::move(entry)).second)invalid("duplicate decoded map key");}return result;}
 else return read_record<T>(value);
}
template<class T>void write(std::ostream& out,const T& value){
 if constexpr(std::is_same_v<T,bool>)out<<(value?"true":"false");
 else if constexpr(std::is_enum_v<T>)out<<"{\"name\":"<<enum_name(value)<<",\"index\":"<<static_cast<int>(value)<<'}';
 else if constexpr(std::is_integral_v<T>)out<<value;
 else if constexpr(std::is_same_v<T,std::string>)out<<q(value);
 else if constexpr(std::is_same_v<T,double>){const auto text=serialize_embedding_workbench_vector_json({value});out.write(text.data()+1,static_cast<std::streamsize>(text.size()-2));}
 else if constexpr(Vector<T>::value){out<<'[';bool first=true;for(const auto& item:value){if(!first)out<<',';first=false;write(out,item);}out<<']';}
 else if constexpr(Map<T>::value){out<<'[';bool first=true;for(const auto& [key,item]:value){if(!first)out<<',';first=false;out<<"{\"key\":"<<q(key)<<",\"value\":";write(out,item);out<<'}';}out<<']';}
 else{out<<'{';bool first=true;std::apply([&](const auto&...entry){([&]{if(!first)out<<',';first=false;out<<q(entry.name)<<':';write(out,value.*(entry.pointer));}(),...);},Record<T>::members());out<<'}';}
}
std::string hex(std::string_view bytes){constexpr char digits[]="0123456789abcdef";std::string result;result.reserve(bytes.size()*2);for(unsigned char c:bytes){result+=digits[c>>4];result+=digits[c&15];}return result;}
std::string carrier(std::string_view dataset,std::string_view bytes){const bool utf8=q(bytes).starts_with('"');return "{\"dataset\":"+q(dataset)+",\"encoding\":"+q(utf8?"utf8":"hex")+",\"data\":"+q(utf8?bytes:std::string_view(hex(bytes)))+",\"bytes\":"+std::to_string(bytes.size())+",\"sha256\":"+q(footprints_workbench_sha256(bytes))+'}';}
std::string exception_type(const std::exception& error){if(dynamic_cast<const std::invalid_argument*>(&error))return "invalid_argument";if(dynamic_cast<const std::domain_error*>(&error))return "domain_error";if(dynamic_cast<const std::out_of_range*>(&error))return "out_of_range";if(dynamic_cast<const std::overflow_error*>(&error))return "overflow_error";if(dynamic_cast<const std::length_error*>(&error))return "length_error";if(dynamic_cast<const std::runtime_error*>(&error))return "runtime_error";if(dynamic_cast<const std::bad_alloc*>(&error))return "bad_alloc";return "exception";}
std::string error_json(std::string_view phase,const std::exception& error){return "{\"phase\":"+q(phase)+",\"type\":"+q(exception_type(error))+",\"message\":"+q(error.what())+'}';}
std::string typed(std::string_view name,std::string_view value){return "{\"type\":"+q(name)+",\"value\":"+std::string(value)+'}';}
constexpr std::array<std::string_view,9> artifact_names={"log_input_json","log_after_json","result_input_json","result_after_json","configured_options_json","selector_input_json","selector_after_json","full_result_json","native_output_bytes"};
struct Declaration{std::string_view operation,signature,input_type,result_type,category,preparation;bool required_options;};
constexpr std::array<Declaration,21> declarations={{
 {"discover_passed_time","PassedTimeResult discover_passed_time( const EventLog& log, const PassedTimeOptions& options)","EventLog","PassedTimeResult","EventLog_consumer","",true},
 {"passed_time","inline PassedTimeResult passed_time( const EventLog& log, const PassedTimeOptions& options)","EventLog","PassedTimeResult","EventLog_consumer","",true},
 {"passed_time_variant_name","const char* passed_time_variant_name(PassedTimeVariant variant)","PassedTimeVariant","NativeString","static_cstring_name","",false},
 {"discover_service_time","ServiceTimeResult discover_service_time( const EventLog& log, const ServiceTimeOptions& options)","EventLog","ServiceTimeResult","EventLog_consumer","",false},
 {"service_time","inline ServiceTimeResult service_time( const EventLog& log, const ServiceTimeOptions& options)","EventLog","ServiceTimeResult","EventLog_consumer","",false},
 {"service_time_aggregation_name","const char* service_time_aggregation_name( DfgPerformanceAggregation aggregation)","DfgPerformanceAggregation","NativeString","static_cstring_name","",false},
 {"discover_cycle_time","CycleTimeResult discover_cycle_time( const EventLog& log, const CycleTimeOptions& options)","EventLog","CycleTimeResult","EventLog_consumer","",false},
 {"cycle_time","inline CycleTimeResult cycle_time( const EventLog& log, const CycleTimeOptions& options)","EventLog","CycleTimeResult","EventLog_consumer","",false},
 {"compute_cycle_time","inline CycleTimeResult compute_cycle_time( const EventLog& log, const CycleTimeOptions& options)","EventLog","CycleTimeResult","EventLog_consumer","",false},
 {"discover_case_arrival","CaseArrivalResult discover_case_arrival( const EventLog& log, const CaseArrivalOptions& options)","EventLog","CaseArrivalResult","EventLog_consumer","",false},
 {"case_arrival","inline CaseArrivalResult case_arrival( const EventLog& log, const CaseArrivalOptions& options)","EventLog","CaseArrivalResult","EventLog_consumer","",false},
 {"get_case_arrival_average","inline double get_case_arrival_average( const EventLog& log, const CaseArrivalOptions& options)","EventLog","NativeFloat64","EventLog_consumer","",false},
 {"get_case_dispersion_average","inline double get_case_dispersion_average( const EventLog& log, const CaseArrivalOptions& options)","EventLog","NativeFloat64","EventLog_consumer","",false},
 {"discover_case_statistics","CaseStatisticsResult discover_case_statistics( const EventLog& log, const CaseStatisticsOptions& options)","EventLog","CaseStatisticsResult","EventLog_consumer","",false},
 {"case_statistics","inline CaseStatisticsResult case_statistics( const EventLog& log, const CaseStatisticsOptions& options)","EventLog","CaseStatisticsResult","EventLog_consumer","",false},
 {"case_statistics_sort_name","const char* case_statistics_sort_name( CaseStatisticsSortBy sort_by)","CaseStatisticsSortBy","NativeString","static_cstring_name","",false},
 {"passed_time_json","std::string passed_time_json(const PassedTimeResult& result)","PassedTimeResult","NativeString","serializer","parse_time_analysis_workbench_passed_time_result_json",false},
 {"service_time_json","std::string service_time_json(const ServiceTimeResult& result)","ServiceTimeResult","NativeString","serializer","parse_time_analysis_workbench_service_time_result_json",false},
 {"cycle_time_json","std::string cycle_time_json(const CycleTimeResult& result)","CycleTimeResult","NativeString","serializer","parse_time_analysis_workbench_cycle_time_result_json",false},
 {"case_arrival_json","std::string case_arrival_json(const CaseArrivalResult& result)","CaseArrivalResult","NativeString","serializer","parse_time_analysis_workbench_case_arrival_result_json",false},
 {"case_statistics_json","std::string case_statistics_json( const CaseStatisticsResult& result)","CaseStatisticsResult","NativeString","serializer","parse_time_analysis_workbench_case_statistics_result_json",false},
}};
using Options=std::variant<std::monostate,PassedTimeOptions,ServiceTimeOptions,CycleTimeOptions,CaseArrivalOptions,CaseStatisticsOptions>;
using Stored=std::variant<std::monostate,PassedTimeResult,ServiceTimeResult,CycleTimeResult,CaseArrivalResult,CaseStatisticsResult>;
using Selector=std::variant<std::monostate,PassedTimeVariant,DfgPerformanceAggregation,CaseStatisticsSortBy>;
using Returned=std::variant<std::monostate,PassedTimeResult,ServiceTimeResult,CycleTimeResult,CaseArrivalResult,CaseStatisticsResult,double,std::string>;
template<class...T>std::string variant_json(const std::variant<T...>& value){return std::visit([](const auto& item)->std::string{if constexpr(std::is_same_v<std::decay_t<decltype(item)>,std::monostate>)return "null";else return render(item);},value);}
bool input_role(std::string_view name){return name=="log_file"||name=="result_file";}
TimeAnalysisWorkbenchResponse execute(const std::vector<DiscoveryWorkbenchPart>& parts,const std::vector<DiscoveryWorkbenchPart>& query,const std::optional<std::string>& transport_error=std::nullopt){
 std::map<std::string,std::vector<const DiscoveryWorkbenchPart*>> by_name;std::string originals="[",controls="[";
 const auto add=[&](const DiscoveryWorkbenchPart& p,bool from_query){by_name[p.name].push_back(&p);const bool file=!from_query&&p.file;if(originals.size()>1)originals+=',';originals+="{\"field\":"+q(p.name)+",\"filename\":"+(file?q(p.filename):"null")+",\"file\":"+(file?"true":"false")+",\"original\":"+carrier(file?p.filename:p.name,p.bytes)+'}';if(from_query||!input_role(p.name)){if(controls.size()>1)controls+=',';controls+="{\"name\":"+q(p.name)+",\"origin\":"+q(from_query?"query":"multipart")+'}';}};
 for(const auto& p:query)add(p,true);for(const auto& p:parts)add(p,false);originals+=']';controls+=']';
 const auto find=[&](std::string_view name)->const DiscoveryWorkbenchPart*{const auto it=by_name.find(std::string(name));return it==by_name.end()||it->second.size()!=1?nullptr:it->second.front();};
 const auto control=[&](std::string_view name,std::string_view fallback){const auto* p=find(name);return p?p->bytes:std::string(fallback);};
 const auto operation=control("operation","");int index=-1;for(std::size_t i=0;i<declarations.size();++i)if(declarations[i].operation==operation){index=static_cast<int>(i);break;}
 const bool consumer=index>=0&&declarations[static_cast<std::size_t>(index)].category=="EventLog_consumer";
 const bool helper=index>=0&&declarations[static_cast<std::size_t>(index)].category=="static_cstring_name";
 const auto form=control("call_form",consumer?"with_options":"direct");const bool defaults=form=="default_options";
 bool selected=false,entered=false,returned=false,log_prepared=false,result_prepared=false,selector_prepared=false,options_prepared=false;
 std::optional<bool> log_unchanged,result_unchanged,selector_unchanged,options_unchanged;
 std::string before_log="null",after_log="null",before_result="null",after_result="null",before_selector="null",after_selector="null",pointer_observation="null";
 EventLog log;Options options;Stored supplied_result;Selector selector;Returned result;const char* returned_name=nullptr;
 std::vector<std::string> parsed,supplied;std::map<std::string,std::optional<std::string>> artifacts;for(const auto name:artifact_names)artifacts.emplace(name,std::nullopt);
 std::string phase="configuration_error",failure="null",calls="[",configured_options="null",configured_selector="null",result_holder="null",native_output="null";
 const auto call=[&](std::string_view name,std::string_view stage,std::string_view kind,bool default_used,const auto& function){if(calls.size()>1)calls+=',';const auto prefix="{\"name\":"+q(name)+",\"stage\":"+q(stage)+",\"input_kind\":"+q(kind)+",\"default_arguments_used\":"+(default_used?"true":"false")+",\"entered\":true";try{function();}catch(const std::exception& e){calls+=prefix+",\"returned\":false,\"error\":"+q(e.what())+'}';throw;}calls+=prefix+",\"returned\":true,\"error\":null}";};
 const auto snapshot_after=[&]{if(!entered)return;if(log_prepared){after_log=serialize_embedding_workbench_raw_log_json(log);artifacts["log_after_json"]=after_log;log_unchanged=before_log==after_log;}if(result_prepared){const auto raw=variant_json(supplied_result);after_result=typed(declarations[static_cast<std::size_t>(index)].input_type,raw);artifacts["result_after_json"]=raw;result_unchanged=before_result==after_result;}if(selector_prepared){after_selector=variant_json(selector);artifacts["selector_after_json"]=after_selector;selector_unchanged=before_selector==after_selector;}if(options_prepared&&!defaults)options_unchanged=configured_options==variant_json(options);};
 try{
  if(transport_error){phase="transport_error";invalid(*transport_error);}
  constexpr std::array<std::string_view,6> accepted={"operation","call_form","log_file","result_file","options_json","selector_json"};
  for(const auto& [name,values]:by_name){if(values.size()!=1)invalid("duplicate control or upload: "+name);if(std::find(accepted.begin(),accepted.end(),name)==accepted.end())invalid("unknown control or upload: "+name);}
  for(const auto& p:query)if(input_role(p.name))invalid("input byte roles cannot be query controls");
  if(!find("operation"))invalid("operation is required");if(index<0)invalid("unknown public operation");const auto& declaration=declarations[static_cast<std::size_t>(index)];
  if(consumer){
   if(form!="with_options"&&(!defaults||declaration.required_options))invalid("unsupported options call form");
   if(!find("log_file"))invalid("log_file is required");if(find("result_file")||find("selector_json"))invalid("result_file/selector_json is inapplicable");
   if(defaults&&find("options_json"))invalid("options_json is inapplicable to default_options");if(declaration.required_options&&!find("options_json"))invalid("options_json is required");
  }else{
   if(form!="direct")invalid("operation requires direct call form");if(find("log_file")||find("options_json"))invalid("log_file/options_json is inapplicable");
   if(helper){if(!find("selector_json"))invalid("selector_json is required");if(find("result_file"))invalid("result_file is inapplicable");}
   else{if(!find("result_file"))invalid("result_file is required");if(find("selector_json"))invalid("selector_json is inapplicable");}
  }
  selected=true;parsed.push_back("operation");parsed.push_back("call_form");
  if(consumer){
   const auto* part=find("options_json");const auto doc=Parser(part?std::string_view(part->bytes):std::string_view("{}")).parse();
   switch(index){case 0:options=read_record<PassedTimeOptions>(doc);break;case 1:options=read_record<PassedTimeOptions>(doc);break;case 3:options=read_record<ServiceTimeOptions>(doc);break;case 4:options=read_record<ServiceTimeOptions>(doc);break;case 6:options=read_record<CycleTimeOptions>(doc);break;case 7:options=read_record<CycleTimeOptions>(doc);break;case 8:options=read_record<CycleTimeOptions>(doc);break;case 9:options=read_record<CaseArrivalOptions>(doc);break;case 10:options=read_record<CaseArrivalOptions>(doc);break;case 11:options=read_record<CaseArrivalOptions>(doc);break;case 12:options=read_record<CaseArrivalOptions>(doc);break;case 13:options=read_record<CaseStatisticsOptions>(doc);break;case 14:options=read_record<CaseStatisticsOptions>(doc);break; default:throw std::logic_error("unreachable options type");}
   options_prepared=true;if(part){supplied=doc.order;parsed.push_back("options_json");}phase="result_serialization_error";configured_options=variant_json(options);artifacts["configured_options_json"]=configured_options;phase="configuration_error";
  }
  if(helper){const auto doc=Parser(find("selector_json")->bytes).parse();switch(index){case 2:selector=read_enum<PassedTimeVariant>(doc);break;case 5:selector=read_enum<DfgPerformanceAggregation>(doc);break;case 15:selector=read_enum<CaseStatisticsSortBy>(doc);break; default:throw std::logic_error("unreachable selector type");}selector_prepared=true;parsed.push_back("selector_json");phase="result_serialization_error";configured_selector=variant_json(selector);before_selector=configured_selector;artifacts["selector_input_json"]=before_selector;}
  phase="input_error";
  if(consumer){call("parse_embedding_workbench_raw_log_json","preparation","EventLog",false,[&]{log=parse_embedding_workbench_raw_log_json(find("log_file")->bytes);log_prepared=true;});parsed.push_back("log_file");phase="result_serialization_error";before_log=serialize_embedding_workbench_raw_log_json(log);artifacts["log_input_json"]=before_log;}
  else if(!helper){call(declaration.preparation,"preparation",declaration.input_type,false,[&]{const auto doc=Parser(find("result_file")->bytes).parse();switch(index){case 16:supplied_result=read_record<PassedTimeResult>(doc);break;case 17:supplied_result=read_record<ServiceTimeResult>(doc);break;case 18:supplied_result=read_record<CycleTimeResult>(doc);break;case 19:supplied_result=read_record<CaseArrivalResult>(doc);break;case 20:supplied_result=read_record<CaseStatisticsResult>(doc);break; default:throw std::logic_error("unreachable stored result type");}result_prepared=true;});parsed.push_back("result_file");phase="result_serialization_error";const auto raw=variant_json(supplied_result);before_result=typed(declaration.input_type,raw);artifacts["result_input_json"]=raw;}
  phase="native_error";
  try{call(declaration.signature,"primary",declaration.input_type,defaults,[&]{entered=true;switch(index){
   case 0:result=discover_passed_time(log,std::get<PassedTimeOptions>(options));break;
   case 1:result=passed_time(log,std::get<PassedTimeOptions>(options));break;
   case 2:returned_name=passed_time_variant_name(std::get<PassedTimeVariant>(selector));break;
   case 3:result=defaults?discover_service_time(log):discover_service_time(log,std::get<ServiceTimeOptions>(options));break;
   case 4:result=defaults?service_time(log):service_time(log,std::get<ServiceTimeOptions>(options));break;
   case 5:returned_name=service_time_aggregation_name(std::get<DfgPerformanceAggregation>(selector));break;
   case 6:result=defaults?discover_cycle_time(log):discover_cycle_time(log,std::get<CycleTimeOptions>(options));break;
   case 7:result=defaults?cycle_time(log):cycle_time(log,std::get<CycleTimeOptions>(options));break;
   case 8:result=defaults?compute_cycle_time(log):compute_cycle_time(log,std::get<CycleTimeOptions>(options));break;
   case 9:result=defaults?discover_case_arrival(log):discover_case_arrival(log,std::get<CaseArrivalOptions>(options));break;
   case 10:result=defaults?case_arrival(log):case_arrival(log,std::get<CaseArrivalOptions>(options));break;
   case 11:result=defaults?get_case_arrival_average(log):get_case_arrival_average(log,std::get<CaseArrivalOptions>(options));break;
   case 12:result=defaults?get_case_dispersion_average(log):get_case_dispersion_average(log,std::get<CaseArrivalOptions>(options));break;
   case 13:result=defaults?discover_case_statistics(log):discover_case_statistics(log,std::get<CaseStatisticsOptions>(options));break;
   case 14:result=defaults?case_statistics(log):case_statistics(log,std::get<CaseStatisticsOptions>(options));break;
   case 15:returned_name=case_statistics_sort_name(std::get<CaseStatisticsSortBy>(selector));break;
   case 16:result=passed_time_json(std::get<PassedTimeResult>(supplied_result));break;
   case 17:result=service_time_json(std::get<ServiceTimeResult>(supplied_result));break;
   case 18:result=cycle_time_json(std::get<CycleTimeResult>(supplied_result));break;
   case 19:result=case_arrival_json(std::get<CaseArrivalResult>(supplied_result));break;
   case 20:result=case_statistics_json(std::get<CaseStatisticsResult>(supplied_result));break;
   default:throw std::logic_error("unreachable public declaration");
  }returned=true;phase="result_serialization_error";});}catch(const std::exception&){const auto original=std::current_exception();const auto old_phase=phase;phase="result_serialization_error";snapshot_after();phase=old_phase;std::rethrow_exception(original);}
  phase="result_serialization_error";
  if(helper){pointer_observation=std::string("{\"is_null\":")+(returned_name?"false}":"true}");if(!returned_name)throw std::logic_error("native name helper returned null");result=std::string(returned_name);}
  snapshot_after();const auto full=variant_json(result);result_holder=typed(declaration.result_type,full);artifacts["full_result_json"]=full;
  if(!consumer){const auto& bytes=std::get<std::string>(result);native_output=carrier("native_output_bytes.bin",bytes);artifacts["native_output_bytes"]=bytes;}
 }catch(const std::exception& error){failure=error_json(phase,error);}
 calls+=']';std::string artifact_map="{";for(const auto name:artifact_names){if(artifact_map.size()>1)artifact_map+=',';const auto& bytes=artifacts.at(std::string(name));artifact_map+=q(name)+':'+(bytes?carrier(std::string(name)+(name=="native_output_bytes"?".bin":".json"),*bytes):"null");}artifact_map+='}';
 const auto bit=[](bool x){return x?"true":"false";};const auto maybe=[](const std::optional<bool>& x){return x?(*x?"true":"false"):"null";};
 const auto submitted=[&](std::string_view name){const auto* p=find(name);return p?carrier(p->file?p->filename:p->name,p->bytes):std::string("null");};
 const auto snapshot=[&](bool after){return "{\"log\":"+(after?after_log:before_log)+",\"result\":"+(after?after_result:before_result)+",\"selector\":"+(after?after_selector:before_selector)+'}';};
 std::string context="{\"size_t_bytes\":"+std::to_string(sizeof(std::size_t))+",\"int_bytes\":"+std::to_string(sizeof(int))+",\"input_type\":"+(selected?q(declarations[static_cast<std::size_t>(index)].input_type):"null")+",\"options_parameter_mode\":"+(selected&&consumer?q(defaults?"default_temporary":"const_reference"):"null")+",\"log_prepared\":"+bit(log_prepared)+",\"result_prepared\":"+bit(result_prepared)+",\"selector_prepared\":"+bit(selector_prepared)+",\"options_prepared\":"+bit(options_prepared)+",\"log_unchanged\":"+maybe(log_unchanged)+",\"result_unchanged\":"+maybe(result_unchanged)+",\"selector_unchanged\":"+maybe(selector_unchanged)+",\"options_unchanged\":"+maybe(options_unchanged)+",\"cstring_pointer_observation\":"+pointer_observation+'}';
 TimeAnalysisWorkbenchResponse response;response.status=failure=="null"?200:phase=="result_serialization_error"?500:400;
 response.json="{\"family\":\"ORIGINAL_TIME_ANALYSIS_WORKBENCH\",\"version\":\"original-time-analysis-workbench-v1\",\"operation\":"+q(operation)+",\"call_form\":"+q(form)+",\"executed\":"+bit(returned)+",\"native_entered\":"+bit(entered)+",\"native_returned\":"+bit(returned)+",\"native_calls\":"+calls+",\"error\":"+failure+",\"originals\":"+originals+",\"parsed_controls\":"+render(parsed)+",\"supplied_controls\":"+controls+",\"input_context\":"+context+",\"input\":{\"before\":"+snapshot(false)+",\"after\":"+snapshot(true)+"},\"submitted_options\":"+submitted("options_json")+",\"configured_options\":"+configured_options+",\"supplied_options\":"+render(supplied)+",\"submitted_selector\":"+submitted("selector_json")+",\"configured_selector\":"+configured_selector+",\"result\":"+result_holder+",\"native_output\":"+native_output+",\"artifact_bytes\":"+artifact_map+'}';return response;
}
} // namespace
PassedTimeOptions parse_time_analysis_workbench_passed_time_options_json(std::string_view bytes,std::set<std::string>* supplied){return read_record<PassedTimeOptions>(Parser(bytes).parse(),supplied);}
PassedTimeResult parse_time_analysis_workbench_passed_time_result_json(std::string_view bytes){return read_record<PassedTimeResult>(Parser(bytes).parse());}
std::string serialize_time_analysis_workbench_options_json(const PassedTimeOptions& value){return render(value);}
std::string serialize_time_analysis_workbench_result_json(const PassedTimeResult& value){return render(value);}
ServiceTimeOptions parse_time_analysis_workbench_service_time_options_json(std::string_view bytes,std::set<std::string>* supplied){return read_record<ServiceTimeOptions>(Parser(bytes).parse(),supplied);}
ServiceTimeResult parse_time_analysis_workbench_service_time_result_json(std::string_view bytes){return read_record<ServiceTimeResult>(Parser(bytes).parse());}
std::string serialize_time_analysis_workbench_options_json(const ServiceTimeOptions& value){return render(value);}
std::string serialize_time_analysis_workbench_result_json(const ServiceTimeResult& value){return render(value);}
CycleTimeOptions parse_time_analysis_workbench_cycle_time_options_json(std::string_view bytes,std::set<std::string>* supplied){return read_record<CycleTimeOptions>(Parser(bytes).parse(),supplied);}
CycleTimeResult parse_time_analysis_workbench_cycle_time_result_json(std::string_view bytes){return read_record<CycleTimeResult>(Parser(bytes).parse());}
std::string serialize_time_analysis_workbench_options_json(const CycleTimeOptions& value){return render(value);}
std::string serialize_time_analysis_workbench_result_json(const CycleTimeResult& value){return render(value);}
CaseArrivalOptions parse_time_analysis_workbench_case_arrival_options_json(std::string_view bytes,std::set<std::string>* supplied){return read_record<CaseArrivalOptions>(Parser(bytes).parse(),supplied);}
CaseArrivalResult parse_time_analysis_workbench_case_arrival_result_json(std::string_view bytes){return read_record<CaseArrivalResult>(Parser(bytes).parse());}
std::string serialize_time_analysis_workbench_options_json(const CaseArrivalOptions& value){return render(value);}
std::string serialize_time_analysis_workbench_result_json(const CaseArrivalResult& value){return render(value);}
CaseStatisticsOptions parse_time_analysis_workbench_case_statistics_options_json(std::string_view bytes,std::set<std::string>* supplied){return read_record<CaseStatisticsOptions>(Parser(bytes).parse(),supplied);}
CaseStatisticsResult parse_time_analysis_workbench_case_statistics_result_json(std::string_view bytes){return read_record<CaseStatisticsResult>(Parser(bytes).parse());}
std::string serialize_time_analysis_workbench_options_json(const CaseStatisticsOptions& value){return render(value);}
std::string serialize_time_analysis_workbench_result_json(const CaseStatisticsResult& value){return render(value);}
std::string serialize_time_analysis_workbench_result_json(double value){return render(value);}
TimeAnalysisWorkbenchResponse execute_time_analysis_workbench(const std::vector<DiscoveryWorkbenchPart>& parts,const std::vector<DiscoveryWorkbenchPart>& query){return execute(parts,query);}
TimeAnalysisWorkbenchResponse execute_time_analysis_workbench_multipart(std::string_view content_type,std::string_view body,const std::vector<DiscoveryWorkbenchPart>& query,bool complete){if(!complete)return execute({},query,"multipart transport did not complete");std::vector<DiscoveryWorkbenchPart> parts;try{parts=parse_discovery_workbench_multipart(content_type,body);}catch(const std::exception& e){return execute({},query,e.what());}return execute(parts,query);}
} // namespace minera
