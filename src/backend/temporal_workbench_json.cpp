#include "minera/temporal_workbench_json.hpp"
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
[[noreturn]] void invalid(std::string_view message){throw std::invalid_argument("temporal workbench JSON: "+std::string(message));}
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
RECORD(BusinessHourSlot,F(start_seconds),F(end_seconds))
RECORD(TemporalProfileEntry,F(source),F(target),F(count),F(mean_seconds),F(standard_deviation_seconds))
RECORD(TemporalProfile,F(entries))
RECORD(TemporalProfileOptions,F(activity_attribute),F(case_id_attribute),F(source_timestamp_attribute),F(target_timestamp_attribute),F(source_completion_timestamp_attribute),F(target_start_timestamp_attribute),F(use_business_hours),F(business_hour_slots),F(business_hour_slots_explicit),F(strict_attributes),F(zeta))
RECORD(TemporalDeviation,F(case_id),F(source),F(target),F(elapsed_seconds),F(standard_deviation_distance))
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
 else if constexpr(std::is_integral_v<T>)return integer<T>(value);
 else if constexpr(std::is_same_v<T,double>){auto values=parse_embedding_workbench_vector_json("["+std::string(value.raw)+"]");if(values.size()!=1)invalid("expected one double");return values.front();}
 else if constexpr(std::is_same_v<T,std::string>)return native_string(value);
 else if constexpr(Vector<T>::value){if(value.kind!=Json::Kind::array)invalid("expected array");T result;for(const auto& item:value.items)result.push_back(read_value<typename Vector<T>::Item>(item));return result;}
 else if constexpr(Map<T>::value){if(value.kind!=Json::Kind::array)invalid("expected byte-key map entry array");T result;for(const auto& item:value.items){allowed(item,{"key","value"});auto key=native_string(required(item,"key"));auto entry=read_value<typename Map<T>::Item>(required(item,"value"));if(!result.emplace(std::move(key),std::move(entry)).second)invalid("duplicate decoded map key");}return result;}
 else return read_record<T>(value);
}
template<class T>void write(std::ostream& out,const T& value){
 if constexpr(std::is_same_v<T,bool>)out<<(value?"true":"false");
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
constexpr std::array<std::string_view,18> artifact_names={"source_input_json","source_after_json","profile_input_json","profile_after_json","receiver_input_json","receiver_after_json","conformance_input_json","conformance_after_json","configured_options_json","zeta_input_json","find_source_json","find_target_json","text_input_bytes","text_input_json","full_result_json","found_entry_json","native_output_bytes","lazy_scans_json"};
struct Declaration {std::string_view operation,signature,input_type,result_type,source_kind;bool options;};
// DECLARATIONS are generated verbatim from the accepted public-overload bank.
constexpr std::array<Declaration,21> declarations={{
 {"discover_temporal_profile","discover_temporal_profile(const EventLog&,const TemporalProfileOptions&)","EventLog","TemporalProfile","log",true},
 {"discover_temporal_profile_log","discover_temporal_profile_log(const EventLog&,const TemporalProfileOptions&)","EventLog","TemporalProfile","log",true},
 {"discover_temporal_profile_dataframe","discover_temporal_profile_dataframe(const EventTable&,const TemporalProfileOptions&)","EventTable","TemporalProfile","table",true},
 {"discover_temporal_profile","discover_temporal_profile(const EventStream&,const TemporalProfileOptions&)","EventStream","TemporalProfile","stream",true},
 {"discover_temporal_profile","discover_temporal_profile(const EventTable&,const TemporalProfileOptions&)","EventTable","TemporalProfile","table",true},
 {"discover_temporal_profile","discover_temporal_profile(const LazyEventTable&,const TemporalProfileOptions&)","LazyEventTable","TemporalProfile","lazy",true},
 {"discover_temporal_profile","discover_temporal_profile(const EventDataView&,const TemporalProfileOptions&)","EventDataView","TemporalProfile","view",true},
 {"check_temporal_profile_conformance","check_temporal_profile_conformance(const EventLog&,const TemporalProfile&,const TemporalProfileOptions&)","EventLog","TemporalDeviationVector","log",true},
 {"conform_temporal_profile_log","conform_temporal_profile_log(const EventLog&,const TemporalProfile&,const TemporalProfileOptions&)","EventLog","TemporalConformanceResult","log",true},
 {"conform_temporal_profile_dataframe","conform_temporal_profile_dataframe(const EventTable&,const TemporalProfile&,const TemporalProfileOptions&)","EventTable","TemporalConformanceResult","table",true},
 {"conform_temporal_profile","conform_temporal_profile(const EventDataView&,const TemporalProfile&,const TemporalProfileOptions&)","EventDataView","TemporalConformanceResult","view",true},
 {"check_temporal_profile_conformance","check_temporal_profile_conformance(const EventLog&,const TemporalProfile&,double)","EventLog","TemporalDeviationVector","log",false},
 {"conformance_temporal_profile","conformance_temporal_profile(const EventLog&,const TemporalProfile&,const TemporalProfileOptions&)","EventLog","TemporalDeviationVector","log",true},
 {"parse_temporal_profile_json","parse_temporal_profile_json(std::string_view)","std::string_view","TemporalProfile","",false},
 {"temporal_profile_json","temporal_profile_json(const TemporalProfile&)","TemporalProfile","NativeString","",false},
 {"temporal_conformance_json","temporal_conformance_json(const TemporalConformanceResult&)","TemporalConformanceResult","NativeString","",false},
 {"serialize_active_source_temporal_profile_json","serialize_active_source_temporal_profile_json(const TemporalProfile&)","TemporalProfile","NativeString","",false},
 {"serialize_active_source_temporal_conformance_json","serialize_active_source_temporal_conformance_json(const TemporalConformanceResult&)","TemporalConformanceResult","NativeString","",false},
 {"TemporalProfileEntry::stddev","TemporalProfileEntry::stddev() const noexcept","TemporalProfileEntry","NativeFloat64","",false},
 {"TemporalDeviation::distance","TemporalDeviation::distance() const noexcept","TemporalDeviation","NativeFloat64","",false},
 {"TemporalProfile::find","TemporalProfile::find(const std::string&,const std::string&) const noexcept","TemporalProfile","TemporalProfileEntryPointer","",false},
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
struct TemporalWorkbenchSource {
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
using Returned=std::variant<std::monostate,TemporalProfile,std::vector<TemporalDeviation>,TemporalConformanceResult,double,std::string>;
template<class...T>std::string variant_json(const std::variant<T...>& value){return std::visit([](const auto& item)->std::string{if constexpr(std::is_same_v<std::decay_t<decltype(item)>,std::monostate>)return "null";else return render(item);},value);}
constexpr std::array<std::string_view,8> snapshot_names={"source","profile","receiver","conformance","text","zeta","find_source","find_target"};
std::string snapshots(const std::array<std::string,8>& values){std::string out="{";for(std::size_t i=0;i<values.size();++i){if(i)out+=',';out+=q(snapshot_names[i])+':'+values[i];}return out+'}';}
bool input_role(std::string_view name){return name=="source_file"||name=="profile_file"||name=="receiver_file"||name=="conformance_file"||name=="text_file";}
bool source_name(std::string_view name){return name=="log"||name=="stream"||name=="table"||name=="lazy"||name=="view";}
std::string_view source_type(std::string_view name){if(name=="log")return "EventLog";if(name=="stream")return "EventStream";if(name=="table")return "EventTable";if(name=="lazy")return "LazyEventTable";return {};}
TemporalWorkbenchResponse execute(const std::vector<DiscoveryWorkbenchPart>& parts,const std::vector<DiscoveryWorkbenchPart>& query,const std::optional<std::string>& transport_error=std::nullopt){
 std::map<std::string,std::vector<const DiscoveryWorkbenchPart*>> by_name;std::string originals="[",controls="[";
 // Query originals precede physical multipart originals. Query values retain
 // actual decoded bytes even when typed selector normalization returns null.
 const auto add=[&](const DiscoveryWorkbenchPart& part,bool from_query){by_name[part.name].push_back(&part);{const bool actual_file=!from_query&&part.file;if(originals.size()>1)originals+=',';originals+="{\"field\":"+q(part.name)+",\"filename\":"+(actual_file?q(part.filename):"null")+",\"file\":"+(actual_file?"true":"false")+",\"original\":"+carrier(actual_file?part.filename:part.name,part.bytes)+'}';}if(from_query||!input_role(part.name)){if(controls.size()>1)controls+=',';controls+="{\"name\":"+q(part.name)+",\"origin\":"+q(from_query?"query":"multipart")+'}';}};
 for(const auto& p:query)add(p,true);for(const auto& p:parts)add(p,false);originals+=']';controls+=']';
 const auto find=[&](std::string_view name)->const DiscoveryWorkbenchPart*{const auto it=by_name.find(std::string(name));return it==by_name.end()||it->second.size()!=1?nullptr:it->second.front();};
 const auto control=[&](std::string_view name,std::string_view fallback){const auto* p=find(name);return p?p->bytes:std::string(fallback);};
 const auto operation=control("operation","");std::string source_kind=control("source_kind",""),view_kind=control("view_kind","");
 const bool source_operation=operation=="discover_temporal_profile"||operation=="discover_temporal_profile_log"||operation=="discover_temporal_profile_dataframe"||operation=="check_temporal_profile_conformance"||operation=="conform_temporal_profile_log"||operation=="conform_temporal_profile_dataframe"||operation=="conform_temporal_profile"||operation=="conformance_temporal_profile";
 const auto form=control("call_form",source_operation?"with_options":"direct");const bool defaults=form=="default_options",with_zeta=form=="with_zeta";
 int index=-1;bool selected=false,entered=false,returned=false,options_prepared=false,find_prepared=false,pointer_observed=false;
 std::array<bool,8> prepared{};std::array<std::optional<bool>,8> unchanged{};std::optional<bool> options_unchanged;
 std::array<std::string,8> before,after;before.fill("null");after=before;
 TemporalWorkbenchSource source;TemporalProfile profile;TemporalProfileEntry entry;TemporalDeviation deviation;TemporalConformanceResult conformance;TemporalProfileOptions options;double zeta=0;std::string find_source,find_target,text;
 const TemporalProfileEntry* returned_pointer=nullptr;std::optional<std::size_t> pointer_index;Returned result;
 std::vector<std::string> parsed,supplied;std::map<std::string,std::optional<std::string>> artifacts;for(const auto name:artifact_names)artifacts.emplace(name,std::nullopt);
 std::string phase="configuration_error",failure="null",calls="[",configured_options="null",configured_zeta="null",configured_find="null",result_holder="null",native_output="null";
 const auto call=[&](std::string_view name,std::string_view stage,std::string_view kind,bool defaults_used,const auto& function){
  if(calls.size()>1)calls+=',';const auto prefix="{\"name\":"+q(name)+",\"stage\":"+q(stage)+",\"input_kind\":"+q(kind)+",\"default_arguments_used\":"+(defaults_used?"true":"false")+",\"entered\":true";
  try{function();}catch(const std::exception& e){calls+=prefix+",\"returned\":false,\"error\":"+q(e.what())+'}';throw;}calls+=prefix+",\"returned\":true,\"error\":null}";
 };
 const auto snapshot_after=[&]{if(!entered)return;
  if(prepared[0]){after[0]=source.snapshot();artifacts["source_after_json"]=source.artifact();}
  if(prepared[1]){after[1]=render(profile);artifacts["profile_after_json"]=after[1];}
  if(prepared[2]){after[2]=typed(index==18?"TemporalProfileEntry":"TemporalDeviation",index==18?render(entry):render(deviation));artifacts["receiver_after_json"]=index==18?render(entry):render(deviation);}
  if(prepared[3]){after[3]=render(conformance);artifacts["conformance_after_json"]=after[3];}
  if(prepared[4])after[4]=q(text);if(prepared[5])after[5]=render(zeta);if(prepared[6])after[6]=q(find_source);if(prepared[7])after[7]=q(find_target);
  for(std::size_t i=0;i<prepared.size();++i)if(prepared[i])unchanged[i]=before[i]==after[i];
  if(options_prepared&&!defaults)options_unchanged=render(options)==configured_options;
 };
 try{
  if(transport_error){phase="transport_error";invalid(*transport_error);}
  constexpr std::array<std::string_view,13> accepted={"operation","source_kind","view_kind","call_form","source_file","profile_file","receiver_file","conformance_file","text_file","options_json","zeta_json","find_source_json","find_target_json"};
  for(const auto& [name,values]:by_name){if(values.size()!=1)invalid("duplicate control or upload: "+name);if(std::find(accepted.begin(),accepted.end(),name)==accepted.end())invalid("unknown control or upload: "+name);}
  for(const auto& p:query)if(input_role(p.name))invalid("input byte roles cannot be query controls");
  if(!find("operation"))invalid("operation is required");
  bool known=false;for(const auto& d:declarations)if(d.operation==operation)known=true;if(!known)invalid("unknown public operation");
  if(source_operation){
   if(operation=="discover_temporal_profile"){if(source_kind.empty())invalid("source_kind is required");}
   else{const std::string inferred=operation=="discover_temporal_profile_dataframe"||operation=="conform_temporal_profile_dataframe"?"table":operation=="conform_temporal_profile"?"view":"log";if(source_kind.empty())source_kind=inferred;else if(source_kind!=inferred)invalid("source_kind differs from declared source");}
   if(!source_name(source_kind))invalid("unsupported source_kind");
   if(source_kind=="view"){if(view_kind.empty()||view_kind=="view"||!source_name(view_kind))invalid("view_kind requires a concrete source");}else if(find("view_kind"))invalid("view_kind requires source_kind=view");
  }else if(find("source_kind")||find("view_kind"))invalid("source_kind/view_kind inapplicable to selected operation");
  if(source_operation){if(form!="with_options"&&!defaults&&!(operation=="check_temporal_profile_conformance"&&with_zeta))invalid("unsupported options call form");}else if(form!="direct")invalid("operation requires direct call form");
  for(std::size_t i=0;i<declarations.size();++i){const auto& d=declarations[i];if(d.operation!=operation)continue;if(source_operation&&d.source_kind!=source_kind)continue;if(with_zeta!=(i==11))continue;index=static_cast<int>(i);break;}
  if(index<0)invalid("no matching public declaration");
  const auto& declaration=declarations[static_cast<std::size_t>(index)];
  const bool needs_profile=(index>=7&&index<=12)||index==14||index==16||index==20;
  const bool needs_receiver=index==18||index==19,needs_conformance=index==15||index==17,needs_text=index==13,needs_find=index==20;
  for(const auto role:{"source_file","profile_file","receiver_file","conformance_file","text_file"}){const bool required_role=std::string_view(role)=="source_file"?source_operation:std::string_view(role)=="profile_file"?needs_profile:std::string_view(role)=="receiver_file"?needs_receiver:std::string_view(role)=="conformance_file"?needs_conformance:needs_text;if(required_role&&!find(role))invalid(std::string(role)+" is required");if(!required_role&&find(role))invalid(std::string(role)+" is inapplicable");}
  if(find("options_json")&&(!declaration.options||defaults))invalid("options_json is inapplicable to selected call form");
  if(with_zeta&&!find("zeta_json"))invalid("zeta_json is required");if(!with_zeta&&find("zeta_json"))invalid("zeta_json is inapplicable");
  for(const auto name:{"find_source_json","find_target_json"}){if(needs_find&&!find(name))invalid(std::string(name)+" is required");if(!needs_find&&find(name))invalid(std::string(name)+" is inapplicable");}
  selected=true;parsed.push_back("operation");if(source_operation)parsed.push_back("source_kind");if(source_kind=="view")parsed.push_back("view_kind");parsed.push_back("call_form");
  if(declaration.options){if(const auto* part=find("options_json")){const auto doc=Parser(part->bytes).parse();options=read_record<TemporalProfileOptions>(doc);supplied=doc.order;parsed.push_back("options_json");}options_prepared=true;phase="result_serialization_error";configured_options=render(options);artifacts["configured_options_json"]=configured_options;phase="configuration_error";}
  if(with_zeta){zeta=read_value<double>(Parser(find("zeta_json")->bytes).parse());prepared[5]=true;parsed.push_back("zeta_json");phase="result_serialization_error";configured_zeta=render(zeta);before[5]=configured_zeta;artifacts["zeta_input_json"]=before[5];phase="configuration_error";}
  if(needs_find){auto parsed_source=native_string(Parser(find("find_source_json")->bytes).parse());parsed.push_back("find_source_json");auto parsed_target=native_string(Parser(find("find_target_json")->bytes).parse());parsed.push_back("find_target_json");find_source=std::move(parsed_source);find_target=std::move(parsed_target);find_prepared=true;prepared[6]=prepared[7]=true;phase="result_serialization_error";before[6]=q(find_source);before[7]=q(find_target);configured_find="{\"source\":"+before[6]+",\"target\":"+before[7]+'}';artifacts["find_source_json"]=before[6];artifacts["find_target_json"]=before[7];phase="configuration_error";}
  phase="input_error";
  if(source_operation){source.prepare(source_kind=="view"?view_kind:source_kind,find("source_file")->bytes,call);prepared[0]=true;parsed.push_back("source_file");phase="result_serialization_error";before[0]=source.snapshot();artifacts["source_input_json"]=source.artifact();phase="input_error";}
  if(needs_profile){call("parse_temporal_workbench_profile_json","preparation","TemporalProfile",false,[&]{profile=parse_temporal_workbench_profile_json(find("profile_file")->bytes);prepared[1]=true;});parsed.push_back("profile_file");phase="result_serialization_error";before[1]=render(profile);artifacts["profile_input_json"]=before[1];phase="input_error";}
  if(needs_receiver){call(index==18?"parse_temporal_workbench_entry_json":"parse_temporal_workbench_deviation_json","preparation",declaration.input_type,false,[&]{if(index==18)entry=parse_temporal_workbench_entry_json(find("receiver_file")->bytes);else deviation=parse_temporal_workbench_deviation_json(find("receiver_file")->bytes);prepared[2]=true;});parsed.push_back("receiver_file");phase="result_serialization_error";const auto value=index==18?render(entry):render(deviation);before[2]=typed(index==18?"TemporalProfileEntry":"TemporalDeviation",value);artifacts["receiver_input_json"]=value;phase="input_error";}
  if(needs_conformance){call("parse_temporal_workbench_conformance_json","preparation","TemporalConformanceResult",false,[&]{conformance=parse_temporal_workbench_conformance_json(find("conformance_file")->bytes);prepared[3]=true;});parsed.push_back("conformance_file");phase="result_serialization_error";before[3]=render(conformance);artifacts["conformance_input_json"]=before[3];phase="input_error";}
  if(needs_text){text=find("text_file")->bytes;if(text.size()>32u*1024u*1024u)invalid("request exceeds 32 MiB");prepared[4]=true;parsed.push_back("text_file");phase="result_serialization_error";before[4]=q(text);artifacts["text_input_bytes"]=text;artifacts["text_input_json"]=before[4];}
  phase="native_error";
  try{call(declaration.signature,"primary",declaration.input_type,defaults,[&]{entered=true;
   switch(index){
    case 0:result=defaults?discover_temporal_profile(source.log):discover_temporal_profile(source.log,options);break;
    case 1:result=defaults?discover_temporal_profile_log(source.log):discover_temporal_profile_log(source.log,options);break;
    case 2:result=defaults?discover_temporal_profile_dataframe(source.table):discover_temporal_profile_dataframe(source.table,options);break;
    case 3:result=defaults?discover_temporal_profile(source.stream):discover_temporal_profile(source.stream,options);break;
    case 4:result=defaults?discover_temporal_profile(source.table):discover_temporal_profile(source.table,options);break;
    case 5:result=defaults?discover_temporal_profile(source.lazy):discover_temporal_profile(source.lazy,options);break;
    case 6:{const auto view=source.view();result=defaults?discover_temporal_profile(view):discover_temporal_profile(view,options);break;}
    case 7:result=defaults?check_temporal_profile_conformance(source.log,profile):check_temporal_profile_conformance(source.log,profile,options);break;
    case 8:result=defaults?conform_temporal_profile_log(source.log,profile):conform_temporal_profile_log(source.log,profile,options);break;
    case 9:result=defaults?conform_temporal_profile_dataframe(source.table,profile):conform_temporal_profile_dataframe(source.table,profile,options);break;
    case 10:{const auto view=source.view();result=defaults?conform_temporal_profile(view,profile):conform_temporal_profile(view,profile,options);break;}
    case 11:result=check_temporal_profile_conformance(source.log,profile,zeta);break;
    case 12:result=defaults?conformance_temporal_profile(source.log,profile):conformance_temporal_profile(source.log,profile,options);break;
    case 13:result=parse_temporal_profile_json(std::string_view(text.data(),text.size()));break;
    case 14:result=temporal_profile_json(profile);break;
    case 15:result=temporal_conformance_json(conformance);break;
    case 16:result=serialize_active_source_temporal_profile_json(profile);break;
    case 17:result=serialize_active_source_temporal_conformance_json(conformance);break;
    case 18:result=entry.stddev();break;
    case 19:result=deviation.distance();break;
    case 20:returned_pointer=profile.find(find_source,find_target);break;
    default:throw std::logic_error("unreachable public declaration");
   }
   returned=true;phase="result_serialization_error";
  });}catch(const std::exception&){const auto original=std::current_exception();const auto original_phase=phase;phase="result_serialization_error";snapshot_after();phase=original_phase;std::rethrow_exception(original);}
  phase="result_serialization_error";
  // Only scalar observations precede copying the actual returned pointer.
  if(index==20){pointer_observed=true;if(returned_pointer)pointer_index=static_cast<std::size_t>(returned_pointer-profile.entries.data());}
  snapshot_after();std::string full;
  if(index==20){std::optional<TemporalProfileEntry> copied;if(returned_pointer)copied=*returned_pointer;const auto copied_json=copied?render(*copied):"null";full="{\"index\":"+(pointer_index?std::to_string(*pointer_index):"null")+",\"entry\":"+copied_json+'}';if(copied)artifacts["found_entry_json"]=copied_json;}
  else full=variant_json(result);
  result_holder=typed(declaration.result_type,full);artifacts["full_result_json"]=full;
  if(index>=14&&index<=17){const auto& bytes=std::get<std::string>(result);native_output=carrier("native_output_bytes.bin",bytes);artifacts["native_output_bytes"]=bytes;}
 }catch(const std::exception& error){failure=error_json(phase,error);}
 if(prepared[0]&&source.kind=="lazy")artifacts["lazy_scans_json"]=scans_json(*source.scans);
 calls+=']';std::string artifact_map="{";for(const auto name:artifact_names){if(artifact_map.size()>1)artifact_map+=',';const auto& bytes=artifacts.at(std::string(name));const bool binary=name=="text_input_bytes"||name=="native_output_bytes";artifact_map+=q(name)+':'+(bytes?carrier(std::string(name)+(binary?".bin":".json"),*bytes):"null");}artifact_map+='}';
 // Validation above used the actual raw bytes/presence. The envelope's
 // selectors are typed enums even on error; originals still retain all bytes.
 if(!source_name(source_kind))source_kind.clear();if(view_kind=="view"||!source_name(view_kind))view_kind.clear();
 const auto bit=[](bool value){return value?"true":"false";};const auto maybe=[](const std::optional<bool>& value){return value?(*value?"true":"false"):"null";};
 const auto submitted=[&](std::string_view name){const auto* part=find(name);return part?carrier(part->file?part->filename:part->name,part->bytes):std::string("null");};
 std::string input_context="{\"size_t_bytes\":"+std::to_string(sizeof(std::size_t))+",\"int_bytes\":"+std::to_string(sizeof(int))+",\"input_type\":"+(selected?q(declarations[static_cast<std::size_t>(index)].input_type):"null")+",\"concrete_source_type\":"+(prepared[0]?q(source_type(source.kind)):"null")+",\"options_parameter_mode\":"+(selected&&source_operation?q(with_zeta?"callee_internal":defaults?"default_temporary":"const_reference"):"null");
 for(std::size_t i=0;i<6;++i)input_context+=","+q(std::string(snapshot_names[i])+"_prepared")+':'+bit(prepared[i]);input_context+=",\"options_prepared\":"+std::string(bit(options_prepared))+",\"find_prepared\":"+bit(find_prepared);
 for(std::size_t i=0;i<6;++i)input_context+=","+q(std::string(snapshot_names[i])+"_unchanged")+':'+maybe(unchanged[i]);input_context+=",\"options_unchanged\":"+std::string(maybe(options_unchanged))+",\"find_unchanged\":"+maybe(entered&&find_prepared?std::make_optional(before[6]==after[6]&&before[7]==after[7]):std::nullopt)+",\"lazy_scans\":"+scans_json(*source.scans)+'}';
 TemporalWorkbenchResponse response;response.status=failure=="null"?200:phase=="result_serialization_error"?500:400;
 response.json="{\"family\":\"ORIGINAL_TEMPORAL_WORKBENCH\",\"version\":\"original-temporal-workbench-v1\",\"operation\":"+q(operation)+",\"source_kind\":"+(source_kind.empty()?"null":q(source_kind))+",\"view_kind\":"+(view_kind.empty()?"null":q(view_kind))+",\"call_form\":"+q(form)+",\"executed\":"+bit(returned)+",\"native_entered\":"+bit(entered)+",\"native_returned\":"+bit(returned)+",\"native_calls\":"+calls+",\"error\":"+failure+",\"originals\":"+originals+",\"parsed_controls\":"+render(parsed)+",\"supplied_controls\":"+controls+",\"input_context\":"+input_context+",\"input\":{\"before\":"+snapshots(before)+",\"after\":"+snapshots(after)+"},\"submitted_options\":"+submitted("options_json")+",\"configured_options\":"+configured_options+",\"supplied_options\":"+render(supplied)+",\"submitted_zeta\":"+submitted("zeta_json")+",\"configured_zeta\":"+configured_zeta+",\"submitted_find\":"+(find("find_source_json")&&find("find_target_json")?"{\"source\":"+submitted("find_source_json")+",\"target\":"+submitted("find_target_json")+'}':"null")+",\"configured_find\":"+configured_find+",\"pointer_observation\":"+(pointer_observed?"{\"is_null\":"+std::string(bit(!returned_pointer))+",\"entry_index\":"+(pointer_index?std::to_string(*pointer_index):"null")+'}':"null")+",\"result\":"+result_holder+",\"native_output\":"+native_output+",\"artifact_bytes\":"+artifact_map+'}';
 return response;
}
} // namespace
TemporalProfileOptions parse_temporal_workbench_options_json(std::string_view bytes,std::set<std::string>* supplied){return read_record<TemporalProfileOptions>(Parser(bytes).parse(),supplied);}
std::string serialize_temporal_workbench_options_json(const TemporalProfileOptions& value){return render(value);}
TemporalProfile parse_temporal_workbench_profile_json(std::string_view bytes){return read_record<TemporalProfile>(Parser(bytes).parse());}
TemporalProfileEntry parse_temporal_workbench_entry_json(std::string_view bytes){return read_record<TemporalProfileEntry>(Parser(bytes).parse());}
TemporalDeviation parse_temporal_workbench_deviation_json(std::string_view bytes){return read_record<TemporalDeviation>(Parser(bytes).parse());}
std::vector<TemporalDeviation> parse_temporal_workbench_deviations_json(std::string_view bytes){return read_value<std::vector<TemporalDeviation>>(Parser(bytes).parse());}
TemporalConformanceResult parse_temporal_workbench_conformance_json(std::string_view bytes){return read_value<TemporalConformanceResult>(Parser(bytes).parse());}
std::string serialize_temporal_workbench_result_json(const TemporalProfile& value){return render(value);}
std::string serialize_temporal_workbench_result_json(const TemporalProfileEntry& value){return render(value);}
std::string serialize_temporal_workbench_result_json(const TemporalDeviation& value){return render(value);}
std::string serialize_temporal_workbench_result_json(const std::vector<TemporalDeviation>& value){return render(value);}
std::string serialize_temporal_workbench_result_json(const TemporalConformanceResult& value){return render(value);}
std::string serialize_temporal_workbench_scalar_json(double value){return render(value);}

TemporalWorkbenchResponse execute_temporal_workbench(const std::vector<DiscoveryWorkbenchPart>& parts,const std::vector<DiscoveryWorkbenchPart>& query){return execute(parts,query);}
TemporalWorkbenchResponse execute_temporal_workbench_multipart(std::string_view content_type,std::string_view body,const std::vector<DiscoveryWorkbenchPart>& query,bool complete){
 if(!complete)return execute({},query,"multipart transport did not complete");std::vector<DiscoveryWorkbenchPart> parts;try{parts=parse_discovery_workbench_multipart(content_type,body);}catch(const std::exception& error){return execute({},query,error.what());}return execute(parts,query);
}
} // namespace minera
