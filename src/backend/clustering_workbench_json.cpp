#include "minera/clustering_workbench_json.hpp"
#include "minera/embedding_workbench_json.hpp"
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
[[noreturn]] void invalid(std::string_view message){throw std::invalid_argument("clustering workbench JSON: "+std::string(message));}
std::string q(std::string_view value){return serialize_embedding_workbench_string_json(value);}
std::ostringstream output(){std::ostringstream out;out.imbue(std::locale::classic());return out;}
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
RECORD(TraceAttributeClusteringOptions,F(trace_attribute),F(activity_attribute),F(variant),F(alpha),F(percent),F(max_groups),F(max_output_nodes))
RECORD(TraceAttributeClusteringNode,F(name),F(children),F(distance),F(size),F(leaf))
RECORD(TraceAttributeClusteringDiagnostics,F(computed),F(output_truncated),F(trace_count),F(group_count),F(node_count),F(message))
RECORD(TraceAttributeClusteringResult,F(tree),F(leaf_names),F(diagnostics))
RECORD(TraceAttributeClusteringGroup,F(name),F(trace_indices))
RECORD(ProfileClusteringOptions,F(variant),F(activity_attribute),F(cluster_count),F(max_iterations),F(max_traces),F(max_features),F(seed),F(tolerance),F(include_activity_representation),F(include_succession_representation),F(trace_string_attributes),F(event_string_attributes),F(trace_numeric_attributes),F(event_numeric_attributes),F(feature_names))
RECORD(ProfileClusteringDiagnostics,F(computed),F(converged),F(trace_count),F(feature_count),F(cluster_count),F(iterations),F(message))
RECORD(ProfileClusteringResult,F(variant),F(labels),F(clusters),F(centroids),F(feature_names),F(inertia),F(diagnostics))
#undef RECORD
#undef F
template<class T>concept HasRecord=requires{Record<T>::members();};
template<class T>struct EnumNames;
template<>struct EnumNames<TraceAttributeClusteringVariant>{
 static constexpr std::array<std::string_view,5> names={"DMM_LEVEN","AVG_LEVEN","DMM_VEC","AVG_VEC","DFG"};
 static constexpr std::array<std::string_view,5> aliases={"kDmmLeven","kAvgLeven","kDmmVec","kAvgVec","kDfg"};
};
template<>struct EnumNames<ProfileClusteringVariant>{
 static constexpr std::array<std::string_view,1> names={"SKLEARN_PROFILES"};
 static constexpr std::array<std::string_view,1> aliases={"kSklearnProfiles"};
};
template<class T>std::string_view enum_name(T value){const auto index=static_cast<int>(value);const auto& names=EnumNames<T>::names;return index>=0&&static_cast<std::size_t>(index)<names.size()?names[static_cast<std::size_t>(index)]:std::string_view{};}
template<class T>T enum_value(const Json& value){
 if(value.kind==Json::Kind::number)return static_cast<T>(integer<int>(value));
 if(value.kind==Json::Kind::object){allowed(value,{"name","index"});const auto result=static_cast<T>(integer<int>(required(value,"index")));const auto& name=required(value,"name");if(name.kind!=Json::Kind::null){const auto expected=enum_name(result);if(expected.empty()||native_string(name)!=expected)invalid("enum name differs from index");}return result;}
 const auto name=native_string(value);for(std::size_t i=0;i<EnumNames<T>::names.size();++i)if(name==EnumNames<T>::names[i]||name==EnumNames<T>::aliases[i])return static_cast<T>(i);invalid("unknown enum name");
}
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
 else if constexpr(std::is_enum_v<T>)return enum_value<T>(value);
 else if constexpr(std::is_same_v<T,double>){auto values=parse_embedding_workbench_vector_json("["+std::string(value.raw)+"]");if(values.size()!=1)invalid("expected one double");return values.front();}
 else if constexpr(std::is_same_v<T,std::string>)return native_string(value);
 else if constexpr(Vector<T>::value){if(value.kind!=Json::Kind::array)invalid("expected array");T result;for(const auto& item:value.items)result.push_back(read_value<typename Vector<T>::Item>(item));return result;}
 else return read_record<T>(value);
}
template<class T>void write(std::ostream& out,const T& value){
 if constexpr(std::is_same_v<T,bool>)out<<(value?"true":"false");
 else if constexpr(std::is_integral_v<T>)out<<value;
 else if constexpr(std::is_same_v<T,std::string>)out<<q(value);
 else if constexpr(std::is_same_v<T,double>){const auto text=serialize_embedding_workbench_vector_json({value});out.write(text.data()+1,static_cast<std::streamsize>(text.size()-2));}
 else if constexpr(std::is_enum_v<T>){const auto name=enum_name(value);out<<"{\"name\":"<<(name.empty()?"null":q(name))<<",\"index\":"<<static_cast<int>(value)<<'}';}
 else if constexpr(Vector<T>::value){out<<'[';bool first=true;for(const auto& item:value){if(!first)out<<',';first=false;write(out,item);}out<<']';}
 else{out<<'{';bool first=true;std::apply([&](const auto&...entry){([&]{if(!first)out<<',';first=false;out<<q(entry.name)<<':';write(out,value.*(entry.pointer));}(),...);},Record<T>::members());out<<'}';}
}
std::string hex(std::string_view bytes){constexpr char digits[]="0123456789abcdef";std::string result;result.reserve(bytes.size()*2);for(unsigned char c:bytes){result+=digits[c>>4];result+=digits[c&15];}return result;}
std::string carrier(std::string_view dataset,std::string_view bytes){const bool utf8=q(bytes).starts_with('"');return "{\"dataset\":"+q(dataset)+",\"encoding\":"+q(utf8?"utf8":"hex")+",\"data\":"+q(utf8?bytes:std::string_view(hex(bytes)))+",\"bytes\":"+std::to_string(bytes.size())+",\"sha256\":"+q(footprints_workbench_sha256(bytes))+'}';}
std::string exception_type(const std::exception& error){if(dynamic_cast<const std::invalid_argument*>(&error))return "invalid_argument";if(dynamic_cast<const std::domain_error*>(&error))return "domain_error";if(dynamic_cast<const std::out_of_range*>(&error))return "out_of_range";if(dynamic_cast<const std::overflow_error*>(&error))return "overflow_error";if(dynamic_cast<const std::length_error*>(&error))return "length_error";if(dynamic_cast<const std::runtime_error*>(&error))return "runtime_error";if(dynamic_cast<const std::bad_alloc*>(&error))return "bad_alloc";return "exception";}
std::string error_json(std::string_view phase,const std::exception& error){return "{\"phase\":"+q(phase)+",\"type\":"+q(exception_type(error))+",\"message\":"+q(error.what())+'}';}
std::string typed(std::string_view name,std::string_view value){return "{\"type\":"+q(name)+",\"value\":"+std::string(value)+'}';}
constexpr std::array<std::string_view,6> operations={"trace_attribute_clustering_groups","discover_trace_attribute_clustering","cluster_traces_by_attribute","trace_attribute_clustering_json","discover_profile_clustering","profile_clustering_json"};
constexpr std::array<std::string_view,6> signatures={"trace_attribute_clustering_groups(const EventLog&,const std::string&)","discover_trace_attribute_clustering(const EventLog&,const TraceAttributeClusteringOptions&)","cluster_traces_by_attribute(const EventLog&,const TraceAttributeClusteringOptions&)","trace_attribute_clustering_json(const TraceAttributeClusteringResult&)","discover_profile_clustering(const EventLog&,const ProfileClusteringOptions&)","profile_clustering_json(const ProfileClusteringResult&)"};
constexpr std::array<std::string_view,6> input_types={"EventLog","EventLog","EventLog","TraceAttributeClusteringResult","EventLog","ProfileClusteringResult"};
constexpr std::array<std::string_view,6> result_types={"TraceAttributeClusteringGroups","TraceAttributeClusteringResult","TraceAttributeClusteringResult","NativeString","ProfileClusteringResult","NativeString"};
using Options=std::variant<std::monostate,TraceAttributeClusteringOptions,ProfileClusteringOptions>;
using StoredInput=std::variant<std::monostate,TraceAttributeClusteringResult,ProfileClusteringResult>;
using Returned=std::variant<std::monostate,std::vector<TraceAttributeClusteringGroup>,TraceAttributeClusteringResult,ProfileClusteringResult,std::string>;
template<class...T>std::string variant_json(const std::variant<T...>& value){return std::visit([](const auto& item)->std::string{if constexpr(std::is_same_v<std::decay_t<decltype(item)>,std::monostate>)return "null";else return render(item);},value);}
std::string snapshots(const std::array<std::string,2>& values){return "{\"log\":"+values[0]+",\"result\":"+values[1]+'}';}
bool input_role(std::string_view name){return name=="log_file"||name=="result_file";}
ClusteringWorkbenchResponse execute(const std::vector<DiscoveryWorkbenchPart>& parts,const std::vector<DiscoveryWorkbenchPart>& query,const std::optional<std::string>& transport_error=std::nullopt){
 std::map<std::string,std::vector<const DiscoveryWorkbenchPart*>> by_name;
 std::string originals="[",controls="[";
 const auto add=[&](const DiscoveryWorkbenchPart& part,bool query_part){
  by_name[part.name].push_back(&part);
  if(!query_part){if(originals.size()>1)originals+=',';originals+="{\"field\":"+q(part.name)+",\"filename\":"+(part.file?q(part.filename):"null")+",\"file\":"+(part.file?"true":"false")+",\"original\":"+carrier(part.file?part.filename:part.name,part.bytes)+'}';}
  if(query_part||!input_role(part.name)){if(controls.size()>1)controls+=',';controls+="{\"name\":"+q(part.name)+",\"origin\":"+q(query_part?"query":"multipart")+'}';}
 };
 for(const auto& part:query)add(part,true);for(const auto& part:parts)add(part,false);originals+=']';controls+=']';
 const auto find=[&](std::string_view name)->const DiscoveryWorkbenchPart*{const auto found=by_name.find(std::string(name));return found==by_name.end()||found->second.size()!=1?nullptr:found->second.front();};
 const auto control=[&](std::string_view name,std::string_view fallback){const auto* part=find(name);return part?part->bytes:std::string(fallback);};
 const auto operation=control("operation","");
 int selected_index=-1;for(std::size_t index=0;index<operations.size();++index)if(operation==operations[index])selected_index=static_cast<int>(index);
 const bool has_options=selected_index==1||selected_index==2||selected_index==4;
 const auto form=control("call_form",has_options?"with_options":"direct");const bool defaults=form=="default_options";
 const bool serializer=selected_index==3||selected_index==5;
 bool selected=false,log_prepared=false,result_prepared=false,options_prepared=false,selector_prepared=false,entered=false,returned=false;
 EventLog log;StoredInput stored;Options options;Returned result;std::string selector;
 std::array<std::string,2> before={"null","null"},after=before;
 std::optional<bool> log_unchanged,result_unchanged,options_unchanged,selector_unchanged;
 std::string configured="null",configured_selector="null",phase="configuration_error",failure="null",calls="[",result_holder="null",native_output="null";
 std::vector<std::string> parsed;std::set<std::string> supplied;
 std::map<std::string,std::optional<std::string>> artifacts;
 for(const auto* key:{"log_input_json","log_after_json","result_input_json","result_after_json","configured_options_json","trace_attribute_json","full_result_json","native_output_bytes"})artifacts.emplace(key,std::nullopt);
 const auto call=[&](std::string_view name,std::string_view stage,std::string_view kind,bool default_used,const auto& function){
  if(calls.size()>1)calls+=',';
  const auto prefix="{\"name\":"+q(name)+",\"stage\":"+q(stage)+",\"input_kind\":"+q(kind)+",\"default_arguments_used\":"+(default_used?"true":"false")+",\"entered\":true";
  try{function();calls+=prefix+",\"returned\":true,\"error\":null}";}
  catch(const std::exception& error){calls+=prefix+",\"returned\":false,\"error\":"+q(error.what())+'}';throw;}
 };
 const auto snapshot_after=[&]{
  if(!entered)return;
  if(log_prepared){after[0]=serialize_embedding_workbench_raw_log_json(log);artifacts["log_after_json"]=after[0];log_unchanged=after[0]==before[0];}
  if(result_prepared){after[1]=variant_json(stored);artifacts["result_after_json"]=after[1];result_unchanged=after[1]==before[1];}
  // The compiler-created default-argument temporary is not the prepared host
  // inspection copy. Only an actual explicitly passed options object is compared.
  if(options_prepared&&!defaults)options_unchanged=variant_json(options)==configured;
  if(selector_prepared)selector_unchanged=q(selector)==configured_selector;
 };
 try{
  if(transport_error){phase="transport_error";invalid(*transport_error);}
  for(const auto& [name,values]:by_name){
   if(values.size()!=1)invalid("duplicate control or upload: "+name);
   if(name!="operation"&&name!="call_form"&&name!="options_json"&&name!="trace_attribute_json"&&!input_role(name))invalid("unknown control or upload: "+name);
  }
  for(const auto& part:query)if(input_role(part.name))invalid("log/result byte roles cannot be query controls");
  if(!find("operation"))invalid("operation is required");
  if(selected_index<0)invalid("unknown public operation");
  if(has_options){if(form!="with_options"&&!defaults)invalid("unsupported options call form");}
  else if(form!="direct")invalid("operation requires direct call form");
  if(serializer){if(!find("result_file"))invalid("result_file is required");if(find("log_file"))invalid("log_file is inapplicable to result serializer");}
  else{if(!find("log_file"))invalid("log_file is required");if(find("result_file"))invalid("result_file is inapplicable to log operation");}
  if(!has_options&&find("options_json"))invalid("options_json is inapplicable to operation");
  if(defaults&&find("options_json"))invalid("options_json is inapplicable to default argument form");
  if(selected_index==0){if(!find("trace_attribute_json"))invalid("trace_attribute_json is required");}
  else if(find("trace_attribute_json"))invalid("trace_attribute_json is groups-only");
  selected=true;parsed.push_back("operation");parsed.push_back("call_form");
  if(has_options){
   const auto* part=find("options_json");const std::string bytes=part?part->bytes:"{}";
   call(selected_index==4?"prepare_profile_options":"prepare_hierarchy_options","preparation",selected_index==4?"ProfileClusteringOptions":"TraceAttributeClusteringOptions",false,[&]{
    const auto document=Parser(bytes).parse();
    if(selected_index==4)options=read_record<ProfileClusteringOptions>(document,&supplied);else options=read_record<TraceAttributeClusteringOptions>(document,&supplied);
    options_prepared=true;
   });
   if(part)parsed.push_back("options_json");phase="result_serialization_error";configured=variant_json(options);artifacts["configured_options_json"]=configured;phase="configuration_error";
  }
  if(selected_index==0){
   call("prepare_trace_attribute","preparation","NativeString",false,[&]{selector=native_string(Parser(find("trace_attribute_json")->bytes).parse());selector_prepared=true;});
   parsed.push_back("trace_attribute_json");phase="result_serialization_error";configured_selector=q(selector);artifacts["trace_attribute_json"]=configured_selector;phase="configuration_error";
  }
  phase="input_error";
  if(serializer){
   call(selected_index==3?"prepare_hierarchy_result":"prepare_profile_result","preparation",input_types[static_cast<std::size_t>(selected_index)],false,[&]{
    const auto document=Parser(find("result_file")->bytes).parse();
    if(selected_index==3)stored=read_record<TraceAttributeClusteringResult>(document);else stored=read_record<ProfileClusteringResult>(document);
    result_prepared=true;
   });
   parsed.push_back("result_file");phase="result_serialization_error";before[1]=variant_json(stored);artifacts["result_input_json"]=before[1];
  }else{
   call("parse_embedding_workbench_raw_log_json","preparation","EventLog",false,[&]{log=parse_embedding_workbench_raw_log_json(find("log_file")->bytes);log_prepared=true;});
   parsed.push_back("log_file");phase="result_serialization_error";before[0]=serialize_embedding_workbench_raw_log_json(log);artifacts["log_input_json"]=before[0];
  }
  phase="native_error";
  try{
   call(signatures[static_cast<std::size_t>(selected_index)],"primary",selected_index==0?"EventLog+NativeString":input_types[static_cast<std::size_t>(selected_index)],defaults,[&]{
    entered=true;
    switch(selected_index){
     case 0:result=trace_attribute_clustering_groups(log,selector);break;
     case 1:result=defaults?discover_trace_attribute_clustering(log):discover_trace_attribute_clustering(log,std::get<TraceAttributeClusteringOptions>(options));break;
     case 2:result=defaults?cluster_traces_by_attribute(log):cluster_traces_by_attribute(log,std::get<TraceAttributeClusteringOptions>(options));break;
     case 3:result=trace_attribute_clustering_json(std::get<TraceAttributeClusteringResult>(stored));break;
     case 4:result=defaults?discover_profile_clustering(log):discover_profile_clustering(log,std::get<ProfileClusteringOptions>(options));break;
     case 5:result=profile_clustering_json(std::get<ProfileClusteringResult>(stored));break;
     default:invalid("unreachable public operation");
    }
    returned=true;
   });
  }catch(const std::exception&){const auto original=std::current_exception();phase="result_serialization_error";snapshot_after();phase="native_error";std::rethrow_exception(original);}
  phase="result_serialization_error";snapshot_after();
  const auto complete=variant_json(result);result_holder=typed(result_types[static_cast<std::size_t>(selected_index)],complete);artifacts["full_result_json"]=complete;
  if(serializer){const auto& bytes=std::get<std::string>(result);native_output=carrier("native_output_bytes.bin",bytes);artifacts["native_output_bytes"]=bytes;}
 }catch(const std::exception& error){failure=error_json(phase,error);}
 calls+=']';std::string artifact_map="{";
 for(const auto& [name,bytes]:artifacts){if(artifact_map.size()>1)artifact_map+=',';artifact_map+=q(name)+':'+(bytes?carrier(name+(name=="native_output_bytes"?".bin":".json"),*bytes):"null");}artifact_map+='}';
 const auto maybe=[](const std::optional<bool>& value){return value?(*value?"true":"false"):"null";};
 const auto submitted=[&](std::string_view name){const auto* part=find(name);return part?carrier(part->file?part->filename:part->name,part->bytes):std::string("null");};
 ClusteringWorkbenchResponse response;response.status=failure=="null"?200:phase=="result_serialization_error"?500:400;
 response.json="{\"family\":\"ORIGINAL_CLUSTERING_WORKBENCH\",\"version\":\"original-clustering-workbench-v1\",\"operation\":"+q(operation)+",\"call_form\":"+q(form)+",\"executed\":"+(returned?"true":"false")+",\"native_entered\":"+(entered?"true":"false")+",\"native_returned\":"+(returned?"true":"false")+",\"native_calls\":"+calls+",\"error\":"+failure+",\"originals\":"+originals+",\"parsed_controls\":"+render(parsed)+",\"supplied_controls\":"+controls
  +",\"input_context\":{\"size_t_bytes\":"+std::to_string(sizeof(std::size_t))+",\"int_bytes\":"+std::to_string(sizeof(int))+",\"input_type\":"+(selected?q(input_types[static_cast<std::size_t>(selected_index)]):"null")+",\"log_prepared\":"+(log_prepared?"true":"false")+",\"result_prepared\":"+(result_prepared?"true":"false")+",\"options_prepared\":"+(options_prepared?"true":"false")+",\"trace_attribute_prepared\":"+(selector_prepared?"true":"false")+",\"log_unchanged\":"+maybe(log_unchanged)+",\"result_unchanged\":"+maybe(result_unchanged)+",\"options_unchanged\":"+maybe(options_unchanged)+",\"trace_attribute_unchanged\":"+maybe(selector_unchanged)+'}'
  +",\"input\":{\"before\":"+snapshots(before)+",\"after\":"+snapshots(after)+"},\"submitted_options\":"+submitted("options_json")+",\"configured_options\":"+configured+",\"supplied_options\":"+render(std::vector<std::string>(supplied.begin(),supplied.end()))+",\"submitted_trace_attribute\":"+submitted("trace_attribute_json")+",\"configured_trace_attribute\":"+configured_selector+",\"result\":"+result_holder+",\"native_output\":"+native_output+",\"artifact_bytes\":"+artifact_map+",\"artifact_errors\":[]}";
 return response;
}
} // namespace
TraceAttributeClusteringOptions parse_clustering_hierarchy_options_json(std::string_view value,std::set<std::string>* supplied){return read_record<TraceAttributeClusteringOptions>(Parser(value).parse(),supplied);}
ProfileClusteringOptions parse_clustering_profile_options_json(std::string_view value,std::set<std::string>* supplied){return read_record<ProfileClusteringOptions>(Parser(value).parse(),supplied);}
std::string serialize_clustering_workbench_options_json(const TraceAttributeClusteringOptions& value){return render(value);}
std::string serialize_clustering_workbench_options_json(const ProfileClusteringOptions& value){return render(value);}
TraceAttributeClusteringResult parse_clustering_workbench_hierarchy_result_json(std::string_view value){return read_record<TraceAttributeClusteringResult>(Parser(value).parse());}
ProfileClusteringResult parse_clustering_workbench_profile_result_json(std::string_view value){return read_record<ProfileClusteringResult>(Parser(value).parse());}
std::string serialize_clustering_workbench_result_json(const TraceAttributeClusteringResult& value){return render(value);}
std::string serialize_clustering_workbench_result_json(const ProfileClusteringResult& value){return render(value);}
std::string serialize_clustering_workbench_groups_json(const std::vector<TraceAttributeClusteringGroup>& value){return render(value);}
ClusteringWorkbenchResponse execute_clustering_workbench(const std::vector<DiscoveryWorkbenchPart>& parts,const std::vector<DiscoveryWorkbenchPart>& query){return execute(parts,query);}
ClusteringWorkbenchResponse execute_clustering_workbench_multipart(std::string_view content_type,std::string_view body,const std::vector<DiscoveryWorkbenchPart>& query,bool complete){
 if(!complete)return execute({},query,"multipart transport did not complete");
 std::vector<DiscoveryWorkbenchPart> parts;
 try{parts=parse_discovery_workbench_multipart(content_type,body);}catch(const std::exception& error){return execute({},query,error.what());}
 return execute(parts,query);
}
} // namespace minera
