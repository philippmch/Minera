#include "minera/timing_workbench_json.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/quality_workbench_json.hpp"
#include "minera/active_event_source.hpp"
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
[[noreturn]] void invalid(std::string_view message){throw std::invalid_argument("timing workbench JSON: "+std::string(message));}
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
// omit_false members are written only when true, so stored results from
// before the member existed keep their exact bytes; absent reads as false.
template<class T,class M>struct Member{std::string_view name;M T::*pointer;bool omit_false=false;};
template<class T,class M>auto member(std::string_view name,M T::*pointer,bool omit_false=false){return Member<T,M>{name,pointer,omit_false};}
template<class T>struct Record;
#define F(name) member(#name,&V::name)
#define O(name) member(#name,&V::name,true)
#define RECORD(Type,...) template<>struct Record<Type>{static auto members(){using V=Type;return std::make_tuple(__VA_ARGS__);}};
RECORD(PerformanceSpectrumOptions,F(variant),F(sample_size),F(sort_log),F(activity_attribute),F(timestamp_attribute),F(case_id_attribute))
RECORD(SpectrumObservation,F(case_id),F(instants),F(activities),F(pattern_offset))
RECORD(PerformanceSpectrum,F(activities),F(observations),F(variant),F(activity_attribute),F(timestamp_attribute),F(case_id_attribute))
RECORD(BatchEvent,F(start),F(complete),F(case_id),F(trace_index),F(event_index),F(event_id))
RECORD(Batch,F(type),F(start),F(complete),F(events))
RECORD(BatchGroup,F(activity),F(resource),F(batch_count),F(event_count),F(batches),O(resource_is_null))
RECORD(BatchDiscoveryOptions,F(variant),F(merge_distance_seconds),F(min_batch_size),F(start_timestamp_attribute),F(activity_attribute),F(resource_attribute),F(timestamp_attribute),F(case_id_attribute),F(event_id_attribute))
RECORD(BatchDiscovery,F(options),F(variant),F(groups))
template<class T>struct Enum;
template<>struct Enum<PerformanceSpectrumVariant>{static constexpr std::array<std::string_view,8> names={"CONNECTED","DISCONNECTED","DATAFRAME","LOG","DATAFRAME_DISCONNECTED","LOG_DISCONNECTED","LAZYFRAME","LAZYFRAME_DISCONNECTED"};static constexpr std::array<std::string_view,8> aliases={"kConnected","kDisconnected","kDataframe","kLog","kDataframeDisconnected","kLogDisconnected","kLazyframe","kLazyframeDisconnected"};};
template<>struct Enum<BatchDiscoveryVariant>{static constexpr std::array<std::string_view,3> names={"LOG","PANDAS","POLARS"};static constexpr std::array<std::string_view,3> aliases={"kLog","kPandas","kPolars"};};
template<>struct Enum<BatchType>{static constexpr std::array<std::string_view,5> names={"simultaneous","batching_on_start","batching_on_end","sequential","concurrent"};static constexpr std::array<std::string_view,5> aliases={"kSimultaneous","kBatchingOnStart","kBatchingOnEnd","kSequential","kConcurrent"};};
template<class T>std::string enum_name(T value){const auto index=static_cast<int>(value);return index>=0&&static_cast<std::size_t>(index)<Enum<T>::names.size()?q(Enum<T>::names[static_cast<std::size_t>(index)]):"null";}
template<class T>T read_enum(const Json& value){
 if(value.kind==Json::Kind::number)return static_cast<T>(integer<int>(value));
 if(value.kind==Json::Kind::string){const auto text=native_string(value);for(std::size_t i=0;i<Enum<T>::names.size();++i)if(text==Enum<T>::names[i]||text==Enum<T>::aliases[i])return static_cast<T>(i);invalid("unknown enum spelling");}
 allowed(value,{"name","index"});const auto result=static_cast<T>(integer<int>(required(value,"index")));const auto& name=required(value,"name");if(name.kind!=Json::Kind::null&&read_enum<T>(name)!=result)invalid("enum name/index mismatch");return result;
}
#undef RECORD
#undef F
#undef O
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
 else{out<<'{';bool first=true;std::apply([&](const auto&...entry){([&]{if constexpr(std::is_same_v<std::remove_cvref_t<decltype(value.*(entry.pointer))>,bool>){if(entry.omit_false&&!(value.*(entry.pointer)))return;}if(!first)out<<',';first=false;out<<q(entry.name)<<':';write(out,value.*(entry.pointer));}(),...);},Record<T>::members());out<<'}';}
}
std::string hex(std::string_view bytes){constexpr char digits[]="0123456789abcdef";std::string result;result.reserve(bytes.size()*2);for(unsigned char c:bytes){result+=digits[c>>4];result+=digits[c&15];}return result;}
std::string carrier(std::string_view dataset,std::string_view bytes){const bool utf8=q(bytes).starts_with('"');return "{\"dataset\":"+q(dataset)+",\"encoding\":"+q(utf8?"utf8":"hex")+",\"data\":"+q(utf8?bytes:std::string_view(hex(bytes)))+",\"bytes\":"+std::to_string(bytes.size())+",\"sha256\":"+q(footprints_workbench_sha256(bytes))+'}';}
std::string exception_type(const std::exception& error){if(dynamic_cast<const std::invalid_argument*>(&error))return "invalid_argument";if(dynamic_cast<const std::domain_error*>(&error))return "domain_error";if(dynamic_cast<const std::out_of_range*>(&error))return "out_of_range";if(dynamic_cast<const std::overflow_error*>(&error))return "overflow_error";if(dynamic_cast<const std::length_error*>(&error))return "length_error";if(dynamic_cast<const std::runtime_error*>(&error))return "runtime_error";if(dynamic_cast<const std::bad_alloc*>(&error))return "bad_alloc";return "exception";}
std::string error_json(std::string_view phase,const std::exception& error){return "{\"phase\":"+q(phase)+",\"type\":"+q(exception_type(error))+",\"message\":"+q(error.what())+'}';}
std::string typed(std::string_view name,std::string_view value){return "{\"type\":"+q(name)+",\"value\":"+std::string(value)+'}';}
constexpr std::array<std::string_view,10> artifact_names={"source_input_json","source_after_json","activities_input_json","activities_after_json","configured_options_json","result_input_json","result_after_json","full_result_json","native_output_bytes","lazy_scans_json"};
struct Declaration{std::string_view operation,signature,input_type,result_type,source_kind;};
constexpr std::array<Declaration,27> declarations={{
 {"compute_performance_spectrum","PerformanceSpectrum compute_performance_spectrum( const EventLog& log, std::vector<std::string> activities, PerformanceSpectrumOptions options)","EventLog","PerformanceSpectrum","log"},
 {"discover_performance_spectrum_log","PerformanceSpectrum discover_performance_spectrum_log( const EventLog& log, std::vector<std::string> activities, PerformanceSpectrumOptions options)","EventLog","PerformanceSpectrum","log"},
 {"discover_performance_spectrum_dataframe","PerformanceSpectrum discover_performance_spectrum_dataframe( const EventTable& table, std::vector<std::string> activities, PerformanceSpectrumOptions options)","EventTable","PerformanceSpectrum","table"},
 {"discover_performance_spectrum_dataframe_disconnected","PerformanceSpectrum discover_performance_spectrum_dataframe_disconnected( const EventTable& table, std::vector<std::string> activities, PerformanceSpectrumOptions options)","EventTable","PerformanceSpectrum","table"},
 {"discover_performance_spectrum_log_disconnected","PerformanceSpectrum discover_performance_spectrum_log_disconnected( const EventLog& log, std::vector<std::string> activities, PerformanceSpectrumOptions options)","EventLog","PerformanceSpectrum","log"},
 {"discover_performance_spectrum_lazyframe","PerformanceSpectrum discover_performance_spectrum_lazyframe( const LazyEventTable& table, std::vector<std::string> activities, PerformanceSpectrumOptions options)","LazyEventTable","PerformanceSpectrum","lazy"},
 {"discover_performance_spectrum_lazyframe_disconnected","PerformanceSpectrum discover_performance_spectrum_lazyframe_disconnected( const LazyEventTable& table, std::vector<std::string> activities, PerformanceSpectrumOptions options)","LazyEventTable","PerformanceSpectrum","lazy"},
 {"compute_performance_spectrum","PerformanceSpectrum compute_performance_spectrum( const EventTable& table, std::vector<std::string> activities, PerformanceSpectrumOptions options)","EventTable","PerformanceSpectrum","table"},
 {"compute_performance_spectrum","PerformanceSpectrum compute_performance_spectrum( const EventStream& stream, std::vector<std::string> activities, PerformanceSpectrumOptions options)","EventStream","PerformanceSpectrum","stream"},
 {"compute_performance_spectrum","PerformanceSpectrum compute_performance_spectrum( const LazyEventTable& table, std::vector<std::string> activities, PerformanceSpectrumOptions options)","LazyEventTable","PerformanceSpectrum","lazy"},
 {"compute_performance_spectrum","PerformanceSpectrum compute_performance_spectrum( const EventDataView& data, std::vector<std::string> activities, PerformanceSpectrumOptions options)","EventDataView","PerformanceSpectrum","view"},
 {"discover_performance_spectrum","inline PerformanceSpectrum discover_performance_spectrum( const EventLog& log, std::vector<std::string> activities, PerformanceSpectrumOptions options)","EventLog","PerformanceSpectrum","log"},
 {"discover_performance_spectrum","inline PerformanceSpectrum discover_performance_spectrum( const EventTable& table, std::vector<std::string> activities, PerformanceSpectrumOptions options)","EventTable","PerformanceSpectrum","table"},
 {"discover_performance_spectrum","inline PerformanceSpectrum discover_performance_spectrum( const EventStream& stream, std::vector<std::string> activities, PerformanceSpectrumOptions options)","EventStream","PerformanceSpectrum","stream"},
 {"discover_performance_spectrum","inline PerformanceSpectrum discover_performance_spectrum( const LazyEventTable& table, std::vector<std::string> activities, PerformanceSpectrumOptions options)","LazyEventTable","PerformanceSpectrum","lazy"},
 {"discover_performance_spectrum","inline PerformanceSpectrum discover_performance_spectrum( const EventDataView& data, std::vector<std::string> activities, PerformanceSpectrumOptions options)","EventDataView","PerformanceSpectrum","view"},
 {"discover_batches","BatchDiscovery discover_batches( const EventLog& log, BatchDiscoveryOptions options)","EventLog","BatchDiscovery","log"},
 {"discover_batches","BatchDiscovery discover_batches( const EventTable& table, BatchDiscoveryOptions options)","EventTable","BatchDiscovery","table"},
 {"discover_batches","BatchDiscovery discover_batches( const LazyEventTable& table, BatchDiscoveryOptions options)","LazyEventTable","BatchDiscovery","lazy"},
 {"BatchLogDiscovery::apply","static BatchDiscovery apply( const EventLog& log, BatchDiscoveryOptions options)","EventLog","BatchDiscovery","log"},
 {"BatchPandasDiscovery::apply","static BatchDiscovery apply( const EventTable& table, BatchDiscoveryOptions options)","EventTable","BatchDiscovery","table"},
 {"BatchPolarsDiscovery::apply","static BatchDiscovery apply( const LazyEventTable& table, BatchDiscoveryOptions options)","LazyEventTable","BatchDiscovery","lazy"},
 {"serialize_active_source_spectrum_json","std::string serialize_active_source_spectrum_json(const PerformanceSpectrum& result);","PerformanceSpectrum","NativeString",""},
 {"serialize_active_source_batches_json","std::string serialize_active_source_batches_json(const BatchDiscovery& result);","BatchDiscovery","NativeString",""},
 {"LogBatchDiscovery::apply","static BatchDiscovery apply( const EventLog& log, BatchDiscoveryOptions options)","EventLog","BatchDiscovery","log"},
 {"PandasBatchDiscovery::apply","static BatchDiscovery apply( const EventTable& table, BatchDiscoveryOptions options)","EventTable","BatchDiscovery","table"},
 {"PolarsBatchDiscovery::apply","static BatchDiscovery apply( const LazyEventTable& table, BatchDiscoveryOptions options)","LazyEventTable","BatchDiscovery","lazy"},
}};
struct Scan {std::size_t invocation=0;std::vector<std::size_t> yielded_rows;bool completed=false;std::optional<std::string> error;};
std::string scans_json(const std::vector<Scan>& scans){std::string out="[";for(const auto& s:scans){if(out.size()>1)out+=',';out+="{\"invocation\":"+std::to_string(s.invocation)+",\"yielded_rows\":"+render(s.yielded_rows)+",\"completed\":"+(s.completed?"true":"false")+",\"error\":"+(s.error?q(*s.error):"null")+'}';}return out+']';}
// Assemble known full-writer fragments without parsing or validating output.
// The all-hex writer emits a fixed framing; rows is the actual empty getter
// holder, never materialized or claimed to be stored in the native lazy object.
std::string lazy_getters_json(const LazyEventTable& lazy){
 EventTable getters;getters.columns=lazy.columns();getters.attributes=lazy.attributes();getters.metadata=lazy.metadata();getters.dtypes=lazy.dtypes();getters.index=lazy.index();getters.categorical_dtypes=lazy.categorical_dtypes();
 const auto raw=serialize_quality_workbench_table_json(getters);
 constexpr std::string_view prefix="{\"native_byte_strings\":\"hex-v1\",\"native_schema\":\"event-table-raw-v1\",\"value\":{";
 if(!raw.starts_with(prefix)||!raw.ends_with("}}"))throw std::logic_error("full table writer framing changed");
 auto fields=raw.substr(prefix.size(),raw.size()-prefix.size()-2);const auto at=fields.find(",\"rows\":[]");if(at==std::string::npos)throw std::logic_error("empty getter holder framing changed");fields.erase(at,std::string_view(",\"rows\":[]").size());
 const auto count=lazy.row_count();return "{\"native_byte_strings\":\"hex-v1\",\"valid\":"+std::string(lazy.valid()?"true":"false")+",\"row_count\":"+(count?std::to_string(*count):"null")+','+fields+'}';
}
struct TimingWorkbenchSource {
 std::string kind;EventLog log;EventStream stream;EventTable table;LazyEventTable lazy;bool requested_valid=true;std::optional<std::size_t> declared_count;
 std::shared_ptr<std::vector<Scan>> scans=std::make_shared<std::vector<Scan>>();
 EventDataView view()const{if(kind=="log")return EventDataView(log);if(kind=="stream")return EventDataView(stream);if(kind=="table")return EventDataView(table);return EventDataView(lazy);}
 std::string binding()const{return "{\"table\":"+serialize_quality_workbench_table_json(table)+",\"valid\":"+(requested_valid?"true":"false")+",\"row_count\":"+(declared_count?std::to_string(*declared_count):"null")+'}';}
 std::string value()const{if(kind=="log")return serialize_embedding_workbench_raw_log_json(log);if(kind=="stream")return serialize_quality_workbench_stream_json(stream);if(kind=="table")return serialize_quality_workbench_table_json(table);return "{\"binding\":"+binding()+",\"native\":"+lazy_getters_json(lazy)+'}';}
 std::string snapshot()const{return "{\"kind\":"+q(kind)+",\"value\":"+value()+'}';}
 std::string artifact()const{return kind=="lazy"?binding():value();}
 template<class Recorder>void prepare(std::string_view selected,std::string_view bytes,const Recorder& call){
  kind=std::string(selected);
  if(kind=="log")call("parse_embedding_workbench_raw_log_json","preparation","EventLog",false,[&]{log=parse_embedding_workbench_raw_log_json(bytes);});
  else if(kind=="stream")call("parse_quality_workbench_stream_json","preparation","EventStream",false,[&]{stream=parse_quality_workbench_stream_json(bytes);});
  else if(kind=="table")call("parse_embedding_workbench_table_json","preparation","EventTable",false,[&]{table=parse_embedding_workbench_table_json(bytes);});
  else{
   const auto doc=Parser(bytes).parse();allowed(doc,{"table","valid","row_count"});
   call("parse_embedding_workbench_table_json","preparation","EventTable",false,[&]{table=parse_embedding_workbench_table_json(required(doc,"table").raw);});
   requested_valid=field(doc,"valid")?boolean(required(doc,"valid")):true;declared_count=requested_valid?std::make_optional(table.rows.size()):std::nullopt;
   if(const auto* count=field(doc,"row_count"))declared_count=count->kind==Json::Kind::null?std::nullopt:std::make_optional(integer<std::size_t>(*count));
   auto state=table;state.rows.clear();
   if(!requested_valid&&!declared_count&&serialize_quality_workbench_table_json(state)==serialize_quality_workbench_table_json(EventTable{})){
    call("LazyEventTable::LazyEventTable()","preparation","LazyEventTable",false,[&]{lazy=LazyEventTable{};});return;
   }
   LazyEventTable::Scanner scanner;const auto owned=std::make_shared<const EventTable>(table);const auto history=scans;
   if(requested_valid)scanner=[owned,history](const LazyEventTable::RowConsumer& consumer){const auto index=history->size();history->push_back({index,{},false,std::nullopt});try{for(std::size_t row=0;row<owned->rows.size();++row){history->at(index).yielded_rows.push_back(row);consumer(owned->rows[row]);}history->at(index).completed=true;}catch(const std::exception& error){history->at(index).error=error.what();throw;}};
   call("LazyEventTable::LazyEventTable(columns,scanner,attributes,metadata,row_count,dtypes,index,categorical_dtypes)","preparation","LazyEventTable",false,[&]{lazy=LazyEventTable(table.columns,std::move(scanner),table.attributes,table.metadata,declared_count,table.dtypes,table.index,table.categorical_dtypes);});
  }
 }
};
using Returned=std::variant<std::monostate,PerformanceSpectrum,BatchDiscovery,std::string>;
template<class...T>std::string variant_json(const std::variant<T...>& value){return std::visit([](const auto& item)->std::string{if constexpr(std::is_same_v<std::decay_t<decltype(item)>,std::monostate>)return "null";else return render(item);},value);}
bool input_role(std::string_view name){return name=="source_file"||name=="result_file";}
bool source_name(std::string_view name){return name=="log"||name=="stream"||name=="table"||name=="lazy"||name=="view";}
std::string_view source_type(std::string_view name){if(name=="log")return "EventLog";if(name=="stream")return "EventStream";if(name=="table")return "EventTable";if(name=="lazy")return "LazyEventTable";return {};}
TimingWorkbenchResponse execute(const std::vector<DiscoveryWorkbenchPart>& parts,const std::vector<DiscoveryWorkbenchPart>& query,const std::optional<std::string>& transport_error=std::nullopt){
 std::map<std::string,std::vector<const DiscoveryWorkbenchPart*>> by_name;std::string originals="[",controls="[";
 const auto add=[&](const DiscoveryWorkbenchPart& part,bool from_query){by_name[part.name].push_back(&part);const bool file=!from_query&&part.file;if(originals.size()>1)originals+=',';originals+="{\"field\":"+q(part.name)+",\"filename\":"+(file?q(part.filename):"null")+",\"file\":"+(file?"true":"false")+",\"original\":"+carrier(file?part.filename:part.name,part.bytes)+'}';if(from_query||!input_role(part.name)){if(controls.size()>1)controls+=',';controls+="{\"name\":"+q(part.name)+",\"origin\":"+q(from_query?"query":"multipart")+'}';}};
 for(const auto& p:query)add(p,true);for(const auto& p:parts)add(p,false);originals+=']';controls+=']';
 const auto find=[&](std::string_view name)->const DiscoveryWorkbenchPart*{const auto it=by_name.find(std::string(name));return it==by_name.end()||it->second.size()!=1?nullptr:it->second.front();};
 const auto control=[&](std::string_view name,std::string_view fallback){const auto* p=find(name);return p?p->bytes:std::string(fallback);};
 const auto operation=control("operation","");std::string source_kind=control("source_kind",""),view_kind=control("view_kind","");
 const bool serializer=operation=="serialize_active_source_spectrum_json"||operation=="serialize_active_source_batches_json";
 const bool spectrum=operation.find("performance_spectrum")!=std::string::npos||operation=="serialize_active_source_spectrum_json";
 const auto form=control("call_form",serializer?"direct":"with_options");const bool defaults=form=="default_options";
 int index=-1;bool selected=false,entered=false,returned=false,source_prepared=false,activities_prepared=false,result_prepared=false,options_prepared=false;
 std::optional<bool> source_unchanged,activities_unchanged,result_unchanged,options_unchanged;
 std::string before_source="null",before_activities="null",before_result="null",after_source="null",after_activities="null",after_result="null";
 TimingWorkbenchSource source;std::vector<std::string> activities;PerformanceSpectrumOptions spectrum_options;BatchDiscoveryOptions batch_options;PerformanceSpectrum supplied_spectrum;BatchDiscovery supplied_batches;
 Returned result;std::vector<std::string> parsed,supplied;std::map<std::string,std::optional<std::string>> artifacts;for(const auto name:artifact_names)artifacts.emplace(name,std::nullopt);
 std::string phase="configuration_error",failure="null",calls="[",configured_options="null",result_holder="null",native_output="null";
 const auto call=[&](std::string_view name,std::string_view stage,std::string_view kind,bool defaults_used,const auto& function){if(calls.size()>1)calls+=',';const auto prefix="{\"name\":"+q(name)+",\"stage\":"+q(stage)+",\"input_kind\":"+q(kind)+",\"default_arguments_used\":"+(defaults_used?"true":"false")+",\"entered\":true";try{function();}catch(const std::exception& error){calls+=prefix+",\"returned\":false,\"error\":"+q(error.what())+'}';throw;}calls+=prefix+",\"returned\":true,\"error\":null}";};
 const auto options_json=[&]{return spectrum?render(spectrum_options):render(batch_options);};
 const auto stored_input=[&]{return spectrum?render(supplied_spectrum):render(supplied_batches);};
 const auto snapshot_after=[&]{if(!entered)return;if(source_prepared){after_source=source.snapshot();artifacts["source_after_json"]=source.artifact();source_unchanged=before_source==after_source;}if(activities_prepared){after_activities=render(activities);artifacts["activities_after_json"]=after_activities;activities_unchanged=before_activities==after_activities;}if(result_prepared){const auto raw=stored_input();after_result=typed(spectrum?"PerformanceSpectrum":"BatchDiscovery",raw);artifacts["result_after_json"]=raw;result_unchanged=before_result==after_result;}if(options_prepared&&!defaults)options_unchanged=configured_options==options_json();};
 try{
  if(transport_error){phase="transport_error";invalid(*transport_error);}
  constexpr std::array<std::string_view,8> accepted={"operation","source_kind","view_kind","call_form","source_file","result_file","options_json","activities_json"};
  for(const auto& [name,values]:by_name){if(values.size()!=1)invalid("duplicate control or upload: "+name);if(std::find(accepted.begin(),accepted.end(),name)==accepted.end())invalid("unknown control or upload: "+name);}
  for(const auto& p:query)if(input_role(p.name))invalid("input byte roles cannot be query controls");
  if(!find("operation"))invalid("operation is required");bool known=false;for(const auto& d:declarations)if(d.operation==operation)known=true;if(!known)invalid("unknown public operation");
  if(!serializer){
   const bool generic=operation=="compute_performance_spectrum"||operation=="discover_performance_spectrum"||operation=="discover_batches";
   if(generic){if(!find("source_kind"))invalid("source_kind is required");}
   else{const auto it=std::find_if(declarations.begin(),declarations.end(),[&](const auto& d){return d.operation==operation;});if(!find("source_kind"))source_kind=std::string(it->source_kind);else if(source_kind!=it->source_kind)invalid("source_kind differs from declared source");}
   if(!source_name(source_kind))invalid("unsupported source_kind");
   if(source_kind=="view"){if(!find("view_kind")||view_kind=="view"||!source_name(view_kind))invalid("view_kind requires a concrete source");}else if(find("view_kind"))invalid("view_kind requires source_kind=view");
   if(form!="with_options"&&!defaults)invalid("unsupported options call form");
  }else{if(find("source_kind")||find("view_kind"))invalid("source_kind/view_kind inapplicable to selected operation");if(form!="direct")invalid("operation requires direct call form");}
  for(std::size_t i=0;i<declarations.size();++i){const auto& d=declarations[i];if(d.operation==operation&&(serializer||d.source_kind==source_kind)){index=static_cast<int>(i);break;}}
  if(index<0)invalid("no matching public declaration");const auto& declaration=declarations[static_cast<std::size_t>(index)];
  if(serializer){if(!find("result_file"))invalid("result_file is required");if(find("source_file")||find("options_json")||find("activities_json"))invalid("discovery input inapplicable to serializer");}
  else{if(!find("source_file"))invalid("source_file is required");if(find("result_file"))invalid("result_file is inapplicable");if(defaults&&find("options_json"))invalid("options_json is inapplicable to default_options");if(spectrum&&!find("activities_json"))invalid("activities_json is required");if(!spectrum&&find("activities_json"))invalid("activities_json is inapplicable");}
  selected=true;parsed.push_back("operation");if(!serializer)parsed.push_back("source_kind");if(source_kind=="view")parsed.push_back("view_kind");parsed.push_back("call_form");
  if(!serializer){if(const auto* part=find("options_json")){const auto doc=Parser(part->bytes).parse();if(spectrum)spectrum_options=read_record<PerformanceSpectrumOptions>(doc);else batch_options=read_record<BatchDiscoveryOptions>(doc);supplied=doc.order;parsed.push_back("options_json");}options_prepared=true;phase="result_serialization_error";configured_options=options_json();artifacts["configured_options_json"]=configured_options;phase="configuration_error";}
  if(!serializer&&spectrum){activities=read_value<std::vector<std::string>>(Parser(find("activities_json")->bytes).parse());activities_prepared=true;parsed.push_back("activities_json");phase="result_serialization_error";before_activities=render(activities);artifacts["activities_input_json"]=before_activities;}
  phase="input_error";
  if(!serializer){source.prepare(source_kind=="view"?view_kind:source_kind,find("source_file")->bytes,call);source_prepared=true;parsed.push_back("source_file");phase="result_serialization_error";before_source=source.snapshot();artifacts["source_input_json"]=source.artifact();}
  else{call(spectrum?"parse_timing_workbench_spectrum_json":"parse_timing_workbench_batches_json","preparation",declaration.input_type,false,[&]{if(spectrum)supplied_spectrum=read_record<PerformanceSpectrum>(Parser(find("result_file")->bytes).parse());else supplied_batches=read_record<BatchDiscovery>(Parser(find("result_file")->bytes).parse());result_prepared=true;});parsed.push_back("result_file");phase="result_serialization_error";const auto raw=stored_input();before_result=typed(declaration.input_type,raw);artifacts["result_input_json"]=raw;}
  phase="native_error";
  try{call(declaration.signature,"primary",declaration.input_type,defaults,[&]{entered=true;switch(index){
    case 0:result=defaults?compute_performance_spectrum(source.log,activities):compute_performance_spectrum(source.log,activities,spectrum_options);break;
    case 1:result=defaults?discover_performance_spectrum_log(source.log,activities):discover_performance_spectrum_log(source.log,activities,spectrum_options);break;
    case 2:result=defaults?discover_performance_spectrum_dataframe(source.table,activities):discover_performance_spectrum_dataframe(source.table,activities,spectrum_options);break;
    case 3:result=defaults?discover_performance_spectrum_dataframe_disconnected(source.table,activities):discover_performance_spectrum_dataframe_disconnected(source.table,activities,spectrum_options);break;
    case 4:result=defaults?discover_performance_spectrum_log_disconnected(source.log,activities):discover_performance_spectrum_log_disconnected(source.log,activities,spectrum_options);break;
    case 5:result=defaults?discover_performance_spectrum_lazyframe(source.lazy,activities):discover_performance_spectrum_lazyframe(source.lazy,activities,spectrum_options);break;
    case 6:result=defaults?discover_performance_spectrum_lazyframe_disconnected(source.lazy,activities):discover_performance_spectrum_lazyframe_disconnected(source.lazy,activities,spectrum_options);break;
    case 7:result=defaults?compute_performance_spectrum(source.table,activities):compute_performance_spectrum(source.table,activities,spectrum_options);break;
    case 8:result=defaults?compute_performance_spectrum(source.stream,activities):compute_performance_spectrum(source.stream,activities,spectrum_options);break;
    case 9:result=defaults?compute_performance_spectrum(source.lazy,activities):compute_performance_spectrum(source.lazy,activities,spectrum_options);break;
    case 10:{const auto view=source.view();result=defaults?compute_performance_spectrum(view,activities):compute_performance_spectrum(view,activities,spectrum_options);break;}
    case 11:result=defaults?discover_performance_spectrum(source.log,activities):discover_performance_spectrum(source.log,activities,spectrum_options);break;
    case 12:result=defaults?discover_performance_spectrum(source.table,activities):discover_performance_spectrum(source.table,activities,spectrum_options);break;
    case 13:result=defaults?discover_performance_spectrum(source.stream,activities):discover_performance_spectrum(source.stream,activities,spectrum_options);break;
    case 14:result=defaults?discover_performance_spectrum(source.lazy,activities):discover_performance_spectrum(source.lazy,activities,spectrum_options);break;
    case 15:{const auto view=source.view();result=defaults?discover_performance_spectrum(view,activities):discover_performance_spectrum(view,activities,spectrum_options);break;}
    case 16:result=defaults?discover_batches(source.log):discover_batches(source.log,batch_options);break;
    case 17:result=defaults?discover_batches(source.table):discover_batches(source.table,batch_options);break;
    case 18:result=defaults?discover_batches(source.lazy):discover_batches(source.lazy,batch_options);break;
    case 19:result=defaults?BatchLogDiscovery::apply(source.log):BatchLogDiscovery::apply(source.log,batch_options);break;
    case 20:result=defaults?BatchPandasDiscovery::apply(source.table):BatchPandasDiscovery::apply(source.table,batch_options);break;
    case 21:result=defaults?BatchPolarsDiscovery::apply(source.lazy):BatchPolarsDiscovery::apply(source.lazy,batch_options);break;
    case 22:result=serialize_active_source_spectrum_json(supplied_spectrum);break;
    case 23:result=serialize_active_source_batches_json(supplied_batches);break;
    case 24:result=defaults?LogBatchDiscovery::apply(source.log):LogBatchDiscovery::apply(source.log,batch_options);break;
    case 25:result=defaults?PandasBatchDiscovery::apply(source.table):PandasBatchDiscovery::apply(source.table,batch_options);break;
    case 26:result=defaults?PolarsBatchDiscovery::apply(source.lazy):PolarsBatchDiscovery::apply(source.lazy,batch_options);break;
    default:throw std::logic_error("unreachable public declaration");
   }returned=true;phase="result_serialization_error";});}catch(const std::exception&){const auto original=std::current_exception();const auto original_phase=phase;phase="result_serialization_error";snapshot_after();phase=original_phase;std::rethrow_exception(original);}
  phase="result_serialization_error";snapshot_after();const auto full=variant_json(result);result_holder=typed(declaration.result_type,full);artifacts["full_result_json"]=full;if(serializer){const auto& bytes=std::get<std::string>(result);native_output=carrier("native_output_bytes.bin",bytes);artifacts["native_output_bytes"]=bytes;}
 }catch(const std::exception& error){failure=error_json(phase,error);}
 if(source_prepared&&source.kind=="lazy")artifacts["lazy_scans_json"]=scans_json(*source.scans);
 calls+=']';std::string artifact_map="{";for(const auto name:artifact_names){if(artifact_map.size()>1)artifact_map+=',';const auto& bytes=artifacts.at(std::string(name));artifact_map+=q(name)+':'+(bytes?carrier(std::string(name)+(name=="native_output_bytes"?".bin":".json"),*bytes):"null");}artifact_map+='}';
 if(!source_name(source_kind))source_kind.clear();if(view_kind=="view"||!source_name(view_kind))view_kind.clear();
 const auto bit=[](bool value){return value?"true":"false";};const auto maybe=[](const std::optional<bool>& value){return value?(*value?"true":"false"):"null";};
 const auto submitted=[&](std::string_view name){const auto* part=find(name);return part?carrier(part->file?part->filename:part->name,part->bytes):std::string("null");};
 const auto snapshot=[&](bool after){return "{\"source\":"+(after?after_source:before_source)+",\"activities\":"+(after?after_activities:before_activities)+",\"result\":"+(after?after_result:before_result)+'}';};
 std::string context="{\"size_t_bytes\":"+std::to_string(sizeof(std::size_t))+",\"int_bytes\":"+std::to_string(sizeof(int))+",\"input_type\":"+(selected?q(declarations[static_cast<std::size_t>(index)].input_type):"null")+",\"concrete_source_type\":"+(source_prepared?q(source_type(source.kind)):"null")+",\"options_parameter_mode\":"+(selected&&!serializer?q(defaults?"default_temporary":"caller_copy"):"null")+",\"source_prepared\":"+bit(source_prepared)+",\"activities_prepared\":"+bit(activities_prepared)+",\"result_prepared\":"+bit(result_prepared)+",\"options_prepared\":"+bit(options_prepared)+",\"source_unchanged\":"+maybe(source_unchanged)+",\"activities_unchanged\":"+maybe(activities_unchanged)+",\"result_unchanged\":"+maybe(result_unchanged)+",\"options_unchanged\":"+maybe(options_unchanged)+",\"lazy_scans\":"+scans_json(*source.scans)+'}';
 TimingWorkbenchResponse response;response.status=failure=="null"?200:phase=="result_serialization_error"?500:400;
 response.json="{\"family\":\"ORIGINAL_TIMING_WORKBENCH\",\"version\":\"original-timing-workbench-v1\",\"operation\":"+q(operation)+",\"source_kind\":"+(source_kind.empty()?"null":q(source_kind))+",\"view_kind\":"+(view_kind.empty()?"null":q(view_kind))+",\"call_form\":"+q(form)+",\"executed\":"+bit(returned)+",\"native_entered\":"+bit(entered)+",\"native_returned\":"+bit(returned)+",\"native_calls\":"+calls+",\"error\":"+failure+",\"originals\":"+originals+",\"parsed_controls\":"+render(parsed)+",\"supplied_controls\":"+controls+",\"input_context\":"+context+",\"input\":{\"before\":"+snapshot(false)+",\"after\":"+snapshot(true)+"},\"submitted_options\":"+submitted("options_json")+",\"configured_options\":"+configured_options+",\"supplied_options\":"+render(supplied)+",\"result\":"+result_holder+",\"native_output\":"+native_output+",\"artifact_bytes\":"+artifact_map+'}';return response;
}
} // namespace
PerformanceSpectrumOptions parse_timing_workbench_spectrum_options_json(std::string_view bytes,std::set<std::string>* supplied){return read_record<PerformanceSpectrumOptions>(Parser(bytes).parse(),supplied);}
BatchDiscoveryOptions parse_timing_workbench_batch_options_json(std::string_view bytes,std::set<std::string>* supplied){return read_record<BatchDiscoveryOptions>(Parser(bytes).parse(),supplied);}
PerformanceSpectrum parse_timing_workbench_spectrum_json(std::string_view bytes){return read_record<PerformanceSpectrum>(Parser(bytes).parse());}
BatchDiscovery parse_timing_workbench_batches_json(std::string_view bytes){return read_record<BatchDiscovery>(Parser(bytes).parse());}
std::string serialize_timing_workbench_options_json(const PerformanceSpectrumOptions& value){return render(value);}
std::string serialize_timing_workbench_options_json(const BatchDiscoveryOptions& value){return render(value);}
std::string serialize_timing_workbench_result_json(const PerformanceSpectrum& value){return render(value);}
std::string serialize_timing_workbench_result_json(const BatchDiscovery& value){return render(value);}
TimingWorkbenchResponse execute_timing_workbench(const std::vector<DiscoveryWorkbenchPart>& parts,const std::vector<DiscoveryWorkbenchPart>& query){return execute(parts,query);}
TimingWorkbenchResponse execute_timing_workbench_multipart(std::string_view content_type,std::string_view body,const std::vector<DiscoveryWorkbenchPart>& query,bool complete){if(!complete)return execute({},query,"multipart transport did not complete");std::vector<DiscoveryWorkbenchPart> parts;try{parts=parse_discovery_workbench_multipart(content_type,body);}catch(const std::exception& error){return execute({},query,error.what());}return execute(parts,query);}
} // namespace minera
