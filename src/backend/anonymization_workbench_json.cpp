#include "minera/anonymization_workbench_json.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/footprints_workbench_json.hpp"
#include "minera/literal_controls_json.hpp"
#include "minera/xes_export.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <limits>
#include <locale>
#include <map>
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
[[noreturn]] void invalid(std::string_view message) {
  throw std::invalid_argument("anonymization workbench JSON: " + std::string(message));
}
std::string q(std::string_view value) { return serialize_embedding_workbench_string_json(value); }
std::ostringstream output() { std::ostringstream out; out.imbue(std::locale::classic()); return out; }

// A bounded structural index into submitted JSON. Native string/float parsing
// delegates to the existing public codecs; this index never parses output.
struct Json {
  enum class Kind { object, array, string, number, boolean, null } kind;
  std::string_view raw;
  std::map<std::string, Json> fields;
  std::vector<Json> items;
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
template<class T,class M>struct Member{std::string_view name;M T::*pointer;};
template<class T,class M>auto member(std::string_view name,M T::*pointer){return Member<T,M>{name,pointer};}
template<class T>struct Record;
#define F(name) member(#name,&V::name)
#define RECORD(Type,...) template<>struct Record<Type>{static auto members(){using V=Type;return std::make_tuple(__VA_ARGS__);}};
RECORD(TraceVariantQueryOptions,F(variant),F(epsilon),F(max_prefix_length),F(prune_threshold),F(seed),F(secure_random),F(max_output_traces),F(max_output_variants))
RECORD(PripelOptions,F(epsilon),F(blocklist),F(seed),F(secure_random),F(max_output_traces),F(max_distance))
RECORD(TraceVariantQueryDiagnostics,F(variant),F(epsilon),F(max_prefix_length),F(prune_threshold),F(seed),F(secure_random),F(candidate_count),F(selected_violation_count),F(retained_variant_count),F(released_trace_count),F(output_truncated),F(message))
RECORD(PripelDiagnostics,F(epsilon),F(seed),F(secure_random),F(query_trace_count),F(released_trace_count),F(exact_matches),F(nearest_matches),F(synthesized_traces),F(synthesized_events),F(output_truncated),F(message))
RECORD(TraceVariantQueryResult,F(log),F(frequencies),F(diagnostics))
RECORD(PripelResult,F(log),F(diagnostics))
#undef RECORD
#undef F

std::string_view variant_name(TraceVariantQueryVariant value){switch(value){case TraceVariantQueryVariant::kLaplace:return "laplace";case TraceVariantQueryVariant::kSacofa:return "sacofa";}return {};}
TraceVariantQueryVariant variant_value(const Json& value){
 if(value.kind==Json::Kind::number)return static_cast<TraceVariantQueryVariant>(integer<int>(value));
 if(value.kind==Json::Kind::object&&field(value,"index")){
  allowed(value,{"name","index"});const auto result=static_cast<TraceVariantQueryVariant>(integer<int>(required(value,"index")));const auto& name=required(value,"name");const auto expected=variant_name(result);
  if(expected.empty()){if(name.kind!=Json::Kind::null)invalid("unknown enum index requires null name metadata");}
  else if(native_string(name)!=expected)invalid("enum name metadata differs from index");return result;
 }
 const auto name=native_string(value);if(name=="laplace")return TraceVariantQueryVariant::kLaplace;if(name=="sacofa")return TraceVariantQueryVariant::kSacofa;invalid("unsupported trace-variant query variant");
}
template<class T>T read_record(const Json& value,std::set<std::string>* supplied=nullptr);
template<class T>T read_value(const Json& value){
 if constexpr(std::is_same_v<T,bool>)return boolean(value);
 else if constexpr(std::is_integral_v<T>)return integer<T>(value);
 else if constexpr(std::is_same_v<T,double>){auto values=parse_embedding_workbench_vector_json("["+std::string(value.raw)+"]");if(values.size()!=1)invalid("expected one double");return values.front();}
 else if constexpr(std::is_same_v<T,std::string>)return native_string(value);
 else if constexpr(std::is_same_v<T,std::vector<std::string>>)return parse_literal_string_vector_json(value.raw);
 else if constexpr(std::is_same_v<T,TraceVariantQueryVariant>)return variant_value(value);
 else return read_record<T>(value);
}
template<class T>T read_record(const Json& value,std::set<std::string>* supplied){
 object(value);T result{};std::set<std::string> names;
 std::apply([&](const auto&... entry){(names.insert(std::string(entry.name)),...);},Record<T>::members());
 for(const auto& [name,item]:value.fields){(void)item;if(!names.contains(name))invalid("unknown field "+name);}
 std::apply([&](const auto&... entry){([&]{if(const auto* item=field(value,entry.name)){using M=std::remove_cvref_t<decltype(result.*(entry.pointer))>;result.*(entry.pointer)=read_value<M>(*item);}}(),...);},Record<T>::members());
 if(supplied){std::set<std::string> actual;for(const auto& [name,item]:value.fields){(void)item;actual.insert(name);}*supplied=std::move(actual);}return result;
}
// The one authoritative record walk also captures exact returned log and
// diagnostics fragments. The route reuses them, without a second walk/parse.
struct Fragments{std::optional<std::string> log,diagnostics;};
template<class T>void write(std::ostream&,const T&,Fragments* =nullptr);
template<class T>std::string render(const T& value,Fragments* fragments=nullptr){auto out=output();write(out,value,fragments);return out.str();}
template<class T>void write(std::ostream& out,const T& value,Fragments* fragments){
 if constexpr(std::is_same_v<T,bool>)out<<(value?"true":"false");
 else if constexpr(std::is_integral_v<T>)out<<value;
 else if constexpr(std::is_same_v<T,std::string>)out<<q(value);
 else if constexpr(std::is_same_v<T,double>){const auto text=serialize_embedding_workbench_vector_json({value});out.write(text.data()+1,static_cast<std::streamsize>(text.size()-2));}
 else if constexpr(std::is_same_v<T,TraceVariantQueryVariant>){const auto name=variant_name(value);out<<"{\"name\":"<<(name.empty()?"null":q(name))<<",\"index\":"<<static_cast<int>(value)<<'}';}
 else if constexpr(std::is_same_v<T,EventLog>){const auto text=serialize_embedding_workbench_raw_log_json(value);if(fragments)fragments->log=text;out<<text;}
 else if constexpr(std::is_same_v<T,TraceVariantQueryFrequencies>){out<<'[';bool first=true;for(const auto& [activities,count]:value){if(!first)out<<',';first=false;out<<"{\"activities\":";write(out,activities);out<<",\"count\":"<<count<<'}';}out<<']';}
 else if constexpr(Vector<T>::value){out<<'[';bool first=true;for(const auto& item:value){if(!first)out<<',';first=false;write(out,item);}out<<']';}
 else{
  if constexpr(std::is_same_v<T,TraceVariantQueryDiagnostics>||std::is_same_v<T,PripelDiagnostics>){if(fragments){const auto text=render(value);fragments->diagnostics=text;out<<text;return;}}
  out<<'{';bool first=true;std::apply([&](const auto&... entry){([&]{if(!first)out<<',';first=false;out<<q(entry.name)<<':';write(out,value.*(entry.pointer),fragments);}(),...);},Record<T>::members());out<<'}';
 }
}
std::string typed(std::string_view type,std::string_view value){return "{\"type\":"+q(type)+",\"value\":"+std::string(value)+'}';}
std::string hex(std::string_view value){constexpr char digits[]="0123456789abcdef";std::string out;for(unsigned char c:value){out+=digits[c>>4];out+=digits[c&15];}return out;}
std::string carrier(std::string_view dataset,std::string_view bytes){const bool utf8=q(bytes).starts_with('"');return "{\"dataset\":"+q(dataset)+",\"encoding\":"+q(utf8?"utf8":"hex")+",\"data\":"+q(utf8?bytes:std::string_view(hex(bytes)))+",\"bytes\":"+std::to_string(bytes.size())+",\"sha256\":"+q(footprints_workbench_sha256(bytes))+'}';}
std::string exception_type(const std::exception& error){if(dynamic_cast<const std::invalid_argument*>(&error))return "invalid_argument";if(dynamic_cast<const std::domain_error*>(&error))return "domain_error";if(dynamic_cast<const std::out_of_range*>(&error))return "out_of_range";if(dynamic_cast<const std::overflow_error*>(&error))return "overflow_error";if(dynamic_cast<const std::length_error*>(&error))return "length_error";if(dynamic_cast<const std::runtime_error*>(&error))return "runtime_error";return "exception";}
std::string error_json(std::string_view phase,const std::exception& error){return "{\"phase\":"+q(phase)+",\"exception_type\":"+q(exception_type(error))+",\"message\":"+q(error.what())+'}';}
std::string strings(const std::set<std::string>& names){return render(std::vector<std::string>(names.begin(),names.end()));}


struct Spec{std::string_view operation,call,input,result;int options;};
constexpr Spec specs[]={
 {"anonymize_trace_variant_query","anonymize_trace_variant_query(source, options)","EventLog","TraceVariantQueryResult",1},
 {"query_trace_variants","query_trace_variants(source, options)","EventLog","TraceVariantQueryResult",1},
 {"discover_trace_variant_query","discover_trace_variant_query(source, options)","EventLog","TraceVariantQueryResult",1},
 {"anonymize_trace_variants","anonymize_trace_variants(source, options)","EventLog","TraceVariantQueryResult",1},
 {"anonymize_pripel","anonymize_pripel(source, released_query, options)","PripelInputPair","PripelResult",2},
 {"pripel_anonymize","pripel_anonymize(source, released_query, options)","PripelInputPair","PripelResult",2},
 {"anonymize_pripel_log","anonymize_pripel_log(source, released_query, options)","PripelInputPair","PripelResult",2},
 {"pripel_diagnostics_to_json","diagnostics.to_json()","PripelDiagnostics","std::string",0}
};
using Options=std::variant<std::monostate,TraceVariantQueryOptions,PripelOptions>;
using Result=std::variant<std::monostate,TraceVariantQueryResult,PripelResult,std::string>;
std::string options_json(const Options& value){return std::visit([](const auto& item)->std::string{if constexpr(std::is_same_v<std::decay_t<decltype(item)>,std::monostate>)return "null";else return render(item);},value);}
bool input_role(std::string_view name){return name=="source_file"||name=="query_file"||name=="diagnostics_file";}
std::string snapshots(const std::array<std::string,3>& values){return "{\"source\":"+values[0]+",\"query\":"+values[1]+",\"diagnostics\":"+values[2]+'}';}
AnonymizationWorkbenchResponse execute(const std::vector<DiscoveryWorkbenchPart>& parts,const std::vector<DiscoveryWorkbenchPart>& query,const std::optional<std::string>& transport_error=std::nullopt){
 std::map<std::string,std::vector<const DiscoveryWorkbenchPart*>> by_name;std::string originals="[",controls="[";
 const auto add=[&](const DiscoveryWorkbenchPart& part,bool query_part){by_name[part.name].push_back(&part);if(!query_part){if(originals.size()>1)originals+=',';originals+="{\"field\":"+q(part.name)+",\"filename\":"+(part.file?q(part.filename):"null")+",\"file\":"+(part.file?"true":"false")+",\"original\":"+carrier(part.file?part.filename:part.name,part.bytes)+'}';}if(query_part||!input_role(part.name)){if(controls.size()>1)controls+=',';controls+="{\"name\":"+q(part.name)+",\"origin\":"+q(query_part?"query":"multipart")+'}';}};
 for(const auto& part:query)add(part,true);for(const auto& part:parts)add(part,false);originals+=']';controls+=']';
 const auto find=[&](std::string_view name)->const DiscoveryWorkbenchPart*{const auto found=by_name.find(std::string(name));return found==by_name.end()||found->second.size()!=1?nullptr:found->second.front();};
 const auto* operation_part=find("operation");const std::string operation=operation_part?operation_part->bytes:"anonymize_trace_variant_query";
 const Spec* spec=nullptr;Options options;Result result;std::optional<EventLog> source,released_query;std::optional<PripelDiagnostics> diagnostics;
 std::array<std::string,3> before{"null","null","null"},after=before;std::array<bool,3> prepared{false,false,false};
 std::set<std::string> supplied;std::vector<std::string> parsed;std::string configured="null",phase="configuration_error",failure="null",result_holder="null",calls="[",artifact_errors="[";
 bool entered=false,returned=false;std::optional<bool> input_unchanged,options_unchanged;
 std::map<std::string,std::optional<std::string>> artifacts;for(const auto* name:{"source_log_json","query_log_json","diagnostics_input_json","source_after_json","query_after_json","diagnostics_after_json","configured_options_json","full_result_json","released_log_json","result_diagnostics_json","native_text","result_xes"})artifacts.emplace(name,std::nullopt);
 const auto call=[&](std::string_view name,std::string_view stage,std::string_view kind,const auto& function){if(calls.size()>1)calls+=',';const auto prefix="{\"name\":"+q(name)+",\"stage\":"+q(stage)+",\"input_kind\":"+q(kind);try{function();calls+=prefix+",\"returned\":true,\"error\":null}";}catch(const std::bad_alloc&){throw;}catch(const std::exception& error){calls+=prefix+",\"returned\":false,\"error\":"+error_json(stage=="export"?"export_error":"native_error",error)+'}';throw;}};
 const auto snapshot_after=[&]{
  if(!entered)return;
  if(source){after[0]=serialize_embedding_workbench_raw_log_json(*source);artifacts["source_after_json"]=after[0];}
  if(released_query){after[1]=serialize_embedding_workbench_raw_log_json(*released_query);artifacts["query_after_json"]=after[1];}
  if(diagnostics){after[2]=render(*diagnostics);artifacts["diagnostics_after_json"]=after[2];}
  input_unchanged=before==after;if(options.index())options_unchanged=configured==options_json(options);
 };
 try{
  if(transport_error)invalid(*transport_error);
  for(const auto& [name,values]:by_name){if(values.size()!=1)invalid("duplicate control or upload: "+name);if(name!="operation"&&name!="options_json"&&!input_role(name))invalid("unknown control or upload: "+name);}
  for(const auto& part:query)if(input_role(part.name))invalid("input upload cannot be a query control");
  for(const auto& item:specs)if(operation==item.operation)spec=&item;if(!spec)invalid("unknown public operation");
  if((spec->options==0&&(find("source_file")||find("query_file")||find("options_json")))||(spec->options!=0&&find("diagnostics_file"))||(spec->options==1&&find("query_file")))invalid("inapplicable input role or options control");
  parsed.push_back("operation");
  if(spec->options){const auto* submitted=find("options_json");const auto text=submitted?submitted->bytes:"{}";const auto document=Parser(text).parse();
   if(spec->options==1)options=read_record<TraceVariantQueryOptions>(document,&supplied);else options=read_record<PripelOptions>(document,&supplied);
   phase="result_serialization_error";configured=options_json(options);artifacts["configured_options_json"]=configured;phase="configuration_error";if(submitted)parsed.push_back("options_json");
  }
  phase="preparation_error";
  const auto prepare_log=[&](std::string_view name,std::optional<EventLog>& value,std::size_t index,std::string_view artifact){const auto* part=find(name);if(!part)invalid(std::string(name)+" is required");value=parse_embedding_workbench_raw_log_json(part->bytes);phase="result_serialization_error";before[index]=serialize_embedding_workbench_raw_log_json(*value);artifacts[std::string(artifact)]=before[index];prepared[index]=true;phase="preparation_error";};
  if(spec->options){prepare_log("source_file",source,0,"source_log_json");if(spec->options==2)prepare_log("query_file",released_query,1,"query_log_json");}
  else{const auto* part=find("diagnostics_file");if(!part)invalid("diagnostics_file is required");diagnostics=read_record<PripelDiagnostics>(Parser(part->bytes).parse());phase="result_serialization_error";before[2]=render(*diagnostics);artifacts["diagnostics_input_json"]=before[2];prepared[2]=true;}
  phase="native_error";
  try{call(spec->call,"primary",spec->input,[&]{entered=true;
   if(operation=="anonymize_trace_variant_query")result=anonymize_trace_variant_query(*source,std::get<TraceVariantQueryOptions>(options));
   else if(operation=="query_trace_variants")result=query_trace_variants(*source,std::get<TraceVariantQueryOptions>(options));
   else if(operation=="discover_trace_variant_query")result=discover_trace_variant_query(*source,std::get<TraceVariantQueryOptions>(options));
   else if(operation=="anonymize_trace_variants")result=anonymize_trace_variants(*source,std::get<TraceVariantQueryOptions>(options));
   else if(operation=="anonymize_pripel")result=anonymize_pripel(*source,*released_query,std::get<PripelOptions>(options));
   else if(operation=="pripel_anonymize")result=pripel_anonymize(*source,*released_query,std::get<PripelOptions>(options));
   else if(operation=="anonymize_pripel_log")result=anonymize_pripel_log(*source,*released_query,std::get<PripelOptions>(options));
   else result=diagnostics->to_json();returned=true;
  });}catch(...){phase="result_serialization_error";snapshot_after();phase="native_error";throw;}
  phase="result_serialization_error";snapshot_after();Fragments fragments;std::optional<std::string> native_text;const EventLog* result_log=nullptr;
  const auto body=std::visit([&](const auto& value)->std::string{using T=std::decay_t<decltype(value)>;
   if constexpr(std::is_same_v<T,std::monostate>)return "null";
   else if constexpr(std::is_same_v<T,std::string>){native_text=value;return q(value);}
   else{result_log=&value.log;return render(value,&fragments);}
  },result);
  result_holder=typed(spec->result,body);artifacts["full_result_json"]=result_holder;artifacts["released_log_json"]=std::move(fragments.log);artifacts["result_diagnostics_json"]=std::move(fragments.diagnostics);artifacts["native_text"]=std::move(native_text);
  if(result_log){try{call("serialize_xes(result.log)","export","EventLog",[&]{artifacts["result_xes"]=serialize_xes(*result_log);});}catch(const std::bad_alloc&){throw;}catch(const std::exception& error){if(artifact_errors.size()>1)artifact_errors+=',';artifact_errors+="{\"name\":\"result_xes\",\"error\":"+error_json("export_error",error)+'}';}}
 }catch(const std::bad_alloc&){throw;}catch(const std::exception& error){failure=error_json(phase,error);}
 calls+=']';artifact_errors+=']';std::string artifact_map="{";for(const auto& [name,bytes]:artifacts){if(artifact_map.size()>1)artifact_map+=',';const auto dataset=name=="result_xes"?"result.xes":name=="native_text"?"native-diagnostics.txt":name+".json";artifact_map+=q(name)+':'+(bytes?carrier(dataset,*bytes):"null");}artifact_map+='}';
 const auto maybe=[](const std::optional<bool>& value){return value?(*value?"true":"false"):"null";};const auto* submitted=find("options_json");
 AnonymizationWorkbenchResponse response;response.status=failure=="null"?200:phase=="result_serialization_error"?500:400;
 response.json="{\"family\":\"ORIGINAL_ANONYMIZATION_WORKBENCH\",\"version\":\"original-anonymization-workbench-v1\",\"operation\":"+q(operation)+",\"executed\":"+(returned?"true":"false")+",\"native_entered\":"+(entered?"true":"false")+",\"native_returned\":"+(returned?"true":"false")+",\"native_calls\":"+calls+",\"error\":"+failure+",\"originals\":"+originals+",\"parsed_controls\":"+render(parsed)+",\"supplied_controls\":"+controls
  +",\"input_context\":{\"size_t_bytes\":"+std::to_string(sizeof(std::size_t))+",\"int_bytes\":"+std::to_string(sizeof(int))+",\"input_type\":"+(spec?q(spec->input):"null")+",\"source_prepared\":"+(prepared[0]?"true":"false")+",\"query_prepared\":"+(prepared[1]?"true":"false")+",\"diagnostics_prepared\":"+(prepared[2]?"true":"false")+",\"input_unchanged\":"+maybe(input_unchanged)+",\"options_unchanged\":"+maybe(options_unchanged)+'}'
  +",\"input\":{\"before\":"+snapshots(before)+",\"after\":"+snapshots(after)+"},\"submitted_options\":"+(submitted?carrier(submitted->file?submitted->filename:submitted->name,submitted->bytes):"null")+",\"configured_options\":"+configured+",\"supplied_options\":"+strings(supplied)+",\"result\":"+result_holder+",\"artifact_bytes\":"+artifact_map+",\"artifact_errors\":"+artifact_errors+'}';return response;
}
} // namespace
TraceVariantQueryOptions parse_anonymization_tvq_options_json(std::string_view value,std::set<std::string>* supplied){return read_record<TraceVariantQueryOptions>(Parser(value).parse(),supplied);}
PripelOptions parse_anonymization_pripel_options_json(std::string_view value,std::set<std::string>* supplied){return read_record<PripelOptions>(Parser(value).parse(),supplied);}
PripelDiagnostics parse_anonymization_pripel_diagnostics_json(std::string_view value){return read_record<PripelDiagnostics>(Parser(value).parse());}
std::string serialize_anonymization_workbench_options_json(const TraceVariantQueryOptions& value){return render(value);}
std::string serialize_anonymization_workbench_options_json(const PripelOptions& value){return render(value);}
std::string serialize_anonymization_workbench_diagnostics_json(const TraceVariantQueryDiagnostics& value){return render(value);}
std::string serialize_anonymization_workbench_diagnostics_json(const PripelDiagnostics& value){return render(value);}
std::string serialize_anonymization_workbench_result_json(const TraceVariantQueryResult& value){return render(value);}
std::string serialize_anonymization_workbench_result_json(const PripelResult& value){return render(value);}
AnonymizationWorkbenchResponse execute_anonymization_workbench(const std::vector<DiscoveryWorkbenchPart>& parts,const std::vector<DiscoveryWorkbenchPart>& query){return execute(parts,query);}
AnonymizationWorkbenchResponse execute_anonymization_workbench_multipart(std::string_view content_type,std::string_view body,const std::vector<DiscoveryWorkbenchPart>& query,bool complete){if(!complete)return execute({},query,"multipart transport did not complete");std::vector<DiscoveryWorkbenchPart> parts;try{parts=parse_discovery_workbench_multipart(content_type,body);}catch(const std::bad_alloc&){throw;}catch(const std::exception& error){return execute({},query,error.what());}return execute(parts,query);}
} // namespace minera
