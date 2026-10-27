#include "minera/organizational_workbench_json.hpp"
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
[[noreturn]] void invalid(std::string_view message){throw std::invalid_argument("organizational workbench JSON: "+std::string(message));}
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
template<class T>struct Map:std::false_type{};
template<class T>struct Map<std::map<std::string,T>>:std::true_type{using Item=T;};
template<class T,class M>struct Member{std::string_view name;M T::*pointer;};
template<class T,class M>auto member(std::string_view name,M T::*pointer){return Member<T,M>{name,pointer};}
template<class T>struct Record;
#define F(name) member(#name,&V::name)
#define RECORD(Type,...) template<>struct Record<Type>{static auto members(){using V=Type;return std::make_tuple(__VA_ARGS__);}};
RECORD(OrganizationalSnaOptions,F(variant),F(activity_attribute),F(resource_attribute),F(case_id_attribute),F(beta),F(n),F(metric_normalization),F(max_resources),F(max_edges))
RECORD(OrganizationalSnaEdge,F(source),F(target),F(weight))
RECORD(OrganizationalSnaDiagnostics,F(computed),F(output_truncated),F(trace_count),F(event_count),F(resource_count),F(edge_count),F(message))
RECORD(OrganizationalSnaResult,F(variant),F(directed),F(resources),F(edges),F(diagnostics))
RECORD(RoleDiscoveryOptions,F(threshold),F(variant),F(activity_attribute),F(resource_attribute),F(max_resources),F(max_activities))
RECORD(RoleResourceCount,F(name),F(count))
RECORD(DiscoveredRole,F(activities),F(resources))
RECORD(RoleDiscoveryDiagnostics,F(computed),F(output_truncated),F(trace_count),F(event_count),F(activity_count),F(resource_count),F(role_count),F(message))
RECORD(RoleDiscoveryResult,F(variant),F(roles),F(diagnostics))
RECORD(ResourceProfilesOptions,F(metric),F(activity_attribute),F(resource_attribute),F(timestamp_attribute),F(start_timestamp_attribute),F(case_id_attribute),F(t1),F(t2),F(resource),F(activity),F(resource2),F(max_resources),F(max_rows),F(resource_given),F(activity_given),F(resource2_given))
RECORD(ResourceProfileRow,F(resource),F(activity),F(resource2),F(value),F(count))
RECORD(ResourceProfilesDiagnostics,F(computed),F(output_truncated),F(trace_count),F(event_count),F(resource_count),F(message))
RECORD(ResourceProfilesResult,F(metric),F(rows),F(diagnostics))
RECORD(BusinessHourSlot,F(start_seconds),F(end_seconds))
RECORD(NetworkAnalysisOptions,F(variant),F(sorting_attribute),F(timestamp_attribute),F(source_node_attribute),F(target_node_attribute),F(edge_attribute),F(edge_reference),F(out_relation_attribute),F(in_relation_attribute),F(include_performance),F(business_hours),F(business_hour_slots),F(max_rows),F(max_values))
RECORD(NetworkAnalysisEdge,F(source),F(target),F(activity),F(count),F(performance))
RECORD(NetworkAnalysisDiagnostics,F(computed),F(output_truncated),F(trace_count),F(event_count),F(link_count),F(row_count),F(performance_value_count),F(message))
RECORD(NetworkAnalysisResult,F(variant),F(edges),F(diagnostics))
RECORD(LocalDiagnosticsOptions,F(group_attribute),F(resource_attribute),F(activity_attribute),F(explicit_groups),F(max_groups),F(max_resources),F(max_rows))
RECORD(LocalDiagnosticsDiagnostics,F(computed),F(output_truncated),F(trace_count),F(event_count),F(group_count),F(resource_count),F(row_count),F(skipped_events),F(message))
RECORD(LocalDiagnosticsResult,F(group_relative_focus),F(group_relative_stake),F(group_coverage),F(group_member_contribution),F(diagnostics))
#undef RECORD
#undef F
template<class T>concept HasRecord=requires{Record<T>::members();};
template<class T>struct EnumNames;
template<>struct EnumNames<OrganizationalSnaVariant>{
 static constexpr std::array<std::string_view,8> names={"HANDOVER","WORKING_TOGETHER","SUBCONTRACTING","JOINTACTIVITIES","HANDOVER_PANDAS","WORKING_TOGETHER_PANDAS","SUBCONTRACTING_PANDAS","JOINTACTIVITIES_PANDAS"};
 static constexpr std::array<std::string_view,8> aliases={"kHandover","kWorkingTogether","kSubcontracting","kJointActivities","kHandoverPandas","kWorkingTogetherPandas","kSubcontractingPandas","kJointActivitiesPandas"};
};
template<>struct EnumNames<RoleDiscoveryVariant>{
 static constexpr std::array<std::string_view,2> names={"LOG","PANDAS"};
 static constexpr std::array<std::string_view,2> aliases={"kLog","kPandas"};
};
template<>struct EnumNames<ResourceProfileMetric>{
 static constexpr std::array<std::string_view,11> names={"DISTINCT_ACTIVITIES","ACTIVITY_FREQUENCY","ACTIVITY_COMPLETIONS","CASE_COMPLETIONS","FRACTION_CASE_COMPLETIONS","AVERAGE_WORKLOAD","MULTITASKING","AVERAGE_DURATION_ACTIVITY","AVERAGE_CASE_DURATION","INTERACTION_TWO_RESOURCES","SOCIAL_POSITION"};
 static constexpr std::array<std::string_view,11> aliases={"kDistinctActivities","kActivityFrequency","kActivityCompletions","kCaseCompletions","kFractionCaseCompletions","kAverageWorkload","kMultitasking","kAverageDurationActivity","kAverageCaseDuration","kInteractionTwoResources","kSocialPosition"};
};
template<>struct EnumNames<NetworkAnalysisVariant>{
 static constexpr std::array<std::string_view,1> names={"DATAFRAME"};
 static constexpr std::array<std::string_view,1> aliases={"kDataframe"};
};
template<>struct EnumNames<NetworkAnalysisEdgeReference>{
 static constexpr std::array<std::string_view,2> names={"kOut","kIn"};
 static constexpr std::array<std::string_view,2> aliases={"kOut","kIn"};
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
 else if constexpr(Map<T>::value){if(value.kind!=Json::Kind::array)invalid("expected byte-key map entry array");T result;for(const auto& item:value.items){allowed(item,{"key","value"});auto key=native_string(required(item,"key"));auto entry=read_value<typename Map<T>::Item>(required(item,"value"));if(!result.emplace(std::move(key),std::move(entry)).second)invalid("duplicate decoded map key");}return result;}
 else return read_record<T>(value);
}
template<class T>void write(std::ostream& out,const T& value){
 if constexpr(std::is_same_v<T,bool>)out<<(value?"true":"false");
 else if constexpr(std::is_integral_v<T>)out<<value;
 else if constexpr(std::is_same_v<T,std::string>)out<<q(value);
 else if constexpr(std::is_same_v<T,double>){const auto text=serialize_embedding_workbench_vector_json({value});out.write(text.data()+1,static_cast<std::streamsize>(text.size()-2));}
 else if constexpr(std::is_enum_v<T>){const auto name=enum_name(value);out<<"{\"name\":"<<(name.empty()?"null":q(name))<<",\"index\":"<<static_cast<int>(value)<<'}';}
 else if constexpr(Vector<T>::value){out<<'[';bool first=true;for(const auto& item:value){if(!first)out<<',';first=false;write(out,item);}out<<']';}
 else if constexpr(Map<T>::value){out<<'[';bool first=true;for(const auto& [key,item]:value){if(!first)out<<',';first=false;out<<"{\"key\":"<<q(key)<<",\"value\":";write(out,item);out<<'}';}out<<']';}
 else{out<<'{';bool first=true;std::apply([&](const auto&...entry){([&]{if(!first)out<<',';first=false;out<<q(entry.name)<<':';write(out,value.*(entry.pointer));}(),...);},Record<T>::members());out<<'}';}
}
std::string hex(std::string_view bytes){constexpr char digits[]="0123456789abcdef";std::string result;result.reserve(bytes.size()*2);for(unsigned char c:bytes){result+=digits[c>>4];result+=digits[c&15];}return result;}
std::string carrier(std::string_view dataset,std::string_view bytes){const bool utf8=q(bytes).starts_with('"');return "{\"dataset\":"+q(dataset)+",\"encoding\":"+q(utf8?"utf8":"hex")+",\"data\":"+q(utf8?bytes:std::string_view(hex(bytes)))+",\"bytes\":"+std::to_string(bytes.size())+",\"sha256\":"+q(footprints_workbench_sha256(bytes))+'}';}
std::string exception_type(const std::exception& error){if(dynamic_cast<const std::invalid_argument*>(&error))return "invalid_argument";if(dynamic_cast<const std::domain_error*>(&error))return "domain_error";if(dynamic_cast<const std::out_of_range*>(&error))return "out_of_range";if(dynamic_cast<const std::overflow_error*>(&error))return "overflow_error";if(dynamic_cast<const std::length_error*>(&error))return "length_error";if(dynamic_cast<const std::runtime_error*>(&error))return "runtime_error";if(dynamic_cast<const std::bad_alloc*>(&error))return "bad_alloc";return "exception";}
std::string error_json(std::string_view phase,const std::exception& error){return "{\"phase\":"+q(phase)+",\"type\":"+q(exception_type(error))+",\"message\":"+q(error.what())+'}';}
std::string typed(std::string_view name,std::string_view value){return "{\"type\":"+q(name)+",\"value\":"+std::string(value)+'}';}
constexpr std::array<std::string_view,19> operations={"discover_organizational_sna","mine_organizational_sna","discover_sna","organizational_sna_json","discover_roles","mine_roles","role_discovery_json","discover_resource_profiles","mine_resource_profiles","resource_profiles_json","resource_profile_metric_name","resource_profile_metric_from_string","discover_network_analysis","mine_network_analysis","network_analysis_json","discover_local_diagnostics","apply_from_group_attribute","apply_from_clustering_or_roles","local_diagnostics_json"};
constexpr std::array<std::string_view,19> signatures={"discover_organizational_sna(const EventLog&,const OrganizationalSnaOptions&)","mine_organizational_sna(const EventLog&,const OrganizationalSnaOptions&)","discover_sna(const EventLog&,const OrganizationalSnaOptions&)","organizational_sna_json(const OrganizationalSnaResult&)","discover_roles(const EventLog&,const RoleDiscoveryOptions&)","mine_roles(const EventLog&,const RoleDiscoveryOptions&)","role_discovery_json(const RoleDiscoveryResult&)","discover_resource_profiles(const EventLog&,const ResourceProfilesOptions&)","mine_resource_profiles(const EventLog&,const ResourceProfilesOptions&)","resource_profiles_json(const ResourceProfilesResult&)","resource_profile_metric_name(ResourceProfileMetric)","resource_profile_metric_from_string(std::string_view)","discover_network_analysis(const EventLog&,const NetworkAnalysisOptions&)","mine_network_analysis(const EventLog&,const NetworkAnalysisOptions&)","network_analysis_json(const NetworkAnalysisResult&)","discover_local_diagnostics(const EventLog&,const LocalDiagnosticsOptions&)","apply_from_group_attribute(const EventLog&,const LocalDiagnosticsOptions&)","apply_from_clustering_or_roles(const EventLog&,const std::map<std::string,std::vector<std::string>>&,LocalDiagnosticsOptions)","local_diagnostics_json(const LocalDiagnosticsResult&)"};
constexpr std::array<std::string_view,19> input_types={"EventLog","EventLog","EventLog","OrganizationalSnaResult","EventLog","EventLog","RoleDiscoveryResult","EventLog","EventLog","ResourceProfilesResult","ResourceProfileMetric","std::string_view","EventLog","EventLog","NetworkAnalysisResult","EventLog","EventLog","EventLog","LocalDiagnosticsResult"};
constexpr std::array<std::string_view,19> result_types={"OrganizationalSnaResult","OrganizationalSnaResult","OrganizationalSnaResult","NativeString","RoleDiscoveryResult","RoleDiscoveryResult","NativeString","ResourceProfilesResult","ResourceProfilesResult","NativeString","NativeString","ResourceProfileMetric","NetworkAnalysisResult","NetworkAnalysisResult","NativeString","LocalDiagnosticsResult","LocalDiagnosticsResult","LocalDiagnosticsResult","NativeString"};
using Options=std::variant<std::monostate,OrganizationalSnaOptions,RoleDiscoveryOptions,ResourceProfilesOptions,NetworkAnalysisOptions,LocalDiagnosticsOptions>;
using StoredInput=std::variant<std::monostate,OrganizationalSnaResult,RoleDiscoveryResult,ResourceProfilesResult,NetworkAnalysisResult,LocalDiagnosticsResult>;
using Returned=std::variant<std::monostate,OrganizationalSnaResult,RoleDiscoveryResult,ResourceProfilesResult,NetworkAnalysisResult,LocalDiagnosticsResult,ResourceProfileMetric,std::string>;
template<class...T>std::string variant_json(const std::variant<T...>& value){return std::visit([](const auto& item)->std::string{if constexpr(std::is_same_v<std::decay_t<decltype(item)>,std::monostate>)return "null";else return render(item);},value);}
std::string snapshots(const std::array<std::string,5>& v){return "{\"log\":"+v[0]+",\"result\":"+v[1]+",\"groups\":"+v[2]+",\"metric\":"+v[3]+",\"name\":"+v[4]+'}';}
bool input_role(std::string_view n){return n=="log_file"||n=="result_file";}
int option_kind(int i){if(i>=0&&i<=2)return 1;if(i==4||i==5)return 2;if(i==7||i==8)return 3;if(i==12||i==13)return 4;if(i>=15&&i<=17)return 5;return 0;}
bool serializer_kind(int i){return i==3||i==6||i==9||i==14||i==18;}
OrganizationalWorkbenchResponse execute(const std::vector<DiscoveryWorkbenchPart>& parts,const std::vector<DiscoveryWorkbenchPart>& query,const std::optional<std::string>& transport_error=std::nullopt){
 std::map<std::string,std::vector<const DiscoveryWorkbenchPart*>> by_name;
 std::string originals="[",controls="[";
 const auto add=[&](const DiscoveryWorkbenchPart& part,bool from_query){
  by_name[part.name].push_back(&part);
  if(!from_query){if(originals.size()>1)originals+=',';originals+="{\"field\":"+q(part.name)+",\"filename\":"+(part.file?q(part.filename):"null")+",\"file\":"+(part.file?"true":"false")+",\"original\":"+carrier(part.file?part.filename:part.name,part.bytes)+'}';}
  if(from_query||!input_role(part.name)){if(controls.size()>1)controls+=',';controls+="{\"name\":"+q(part.name)+",\"origin\":"+q(from_query?"query":"multipart")+'}';}
 };
 for(const auto& p:query)add(p,true);for(const auto& p:parts)add(p,false);originals+=']';controls+=']';
 const auto find=[&](std::string_view name)->const DiscoveryWorkbenchPart*{const auto it=by_name.find(std::string(name));return it==by_name.end()||it->second.size()!=1?nullptr:it->second.front();};
 const auto control=[&](std::string_view name,std::string_view fallback){const auto* p=find(name);return p?p->bytes:std::string(fallback);};
 const auto operation=control("operation","");int index=-1;for(std::size_t i=0;i<operations.size();++i)if(operation==operations[i])index=static_cast<int>(i);
 const int okind=option_kind(index);const bool serializer=serializer_kind(index),helper=index==10||index==11;
 const auto form=control("call_form",okind?"with_options":"direct");const bool defaults=form=="default_options";
 bool selected=false,entered=false,returned=false,log_prepared=false,result_prepared=false,options_prepared=false,groups_prepared=false,metric_prepared=false,name_prepared=false;
 EventLog log;StoredInput stored;Options options;Returned result;OrganizationalWorkbenchGroups groups;ResourceProfileMetric metric{};std::string name;const char* returned_pointer=nullptr;
 std::array<std::string,5> before={"null","null","null","null","null"},after=before;
 std::optional<bool> log_unchanged,result_unchanged,options_unchanged,groups_unchanged,metric_unchanged,name_unchanged;
 std::string configured="null",configured_groups="null",configured_metric="null",configured_name="null",phase="configuration_error",failure="null",calls="[",result_holder="null",native_output="null";
 std::vector<std::string> parsed;std::set<std::string> supplied;std::map<std::string,std::optional<std::string>> artifacts;
 for(const auto* key:{"log_input_json","log_after_json","result_input_json","result_after_json","configured_options_json","groups_input_json","groups_after_json","metric_input_json","name_input_json","full_result_json","native_output_bytes"})artifacts.emplace(key,std::nullopt);
 const auto call=[&](std::string_view call_name,std::string_view stage,std::string_view kind,bool default_used,const auto& function){
  if(calls.size()>1)calls+=',';
  const auto prefix="{\"name\":"+q(call_name)+",\"stage\":"+q(stage)+",\"input_kind\":"+q(kind)+",\"default_arguments_used\":"+(default_used?"true":"false")+",\"entered\":true";
  try{function();}catch(const std::exception& e){calls+=prefix+",\"returned\":false,\"error\":"+q(e.what())+'}';throw;}
  calls+=prefix+",\"returned\":true,\"error\":null}";
 };
 const auto snapshot_after=[&]{
  if(!entered)return;
  if(log_prepared){after[0]=serialize_embedding_workbench_raw_log_json(log);artifacts["log_after_json"]=after[0];log_unchanged=after[0]==before[0];}
  if(result_prepared){after[1]=variant_json(stored);artifacts["result_after_json"]=after[1];result_unchanged=after[1]==before[1];}
  if(groups_prepared){after[2]=render(groups);artifacts["groups_after_json"]=after[2];groups_unchanged=after[2]==before[2];}
  if(metric_prepared){after[3]=render(metric);metric_unchanged=after[3]==before[3];}
  if(name_prepared){after[4]=q(name);name_unchanged=after[4]==before[4];}
  // Comparison covers the prepared caller object. A BYVALUE parameter's
  // internal copy is deliberately not substituted for that caller snapshot.
  if(options_prepared&&!defaults)options_unchanged=variant_json(options)==configured;
 };
 try{
  if(transport_error){phase="transport_error";invalid(*transport_error);}
  for(const auto& [n,v]:by_name){if(v.size()!=1)invalid("duplicate control or upload: "+n);if(n!="operation"&&n!="call_form"&&n!="options_json"&&n!="groups_json"&&n!="metric_json"&&n!="name_json"&&!input_role(n))invalid("unknown control or upload: "+n);}
  for(const auto& p:query)if(input_role(p.name))invalid("log/result byte roles cannot be query controls");
  if(!find("operation"))invalid("operation is required");if(index<0)invalid("unknown public operation");
  if(okind){if(form!="with_options"&&!defaults)invalid("unsupported options call form");}else if(form!="direct")invalid("operation requires direct call form");
  if(find("options_json")&&(!okind||defaults))invalid("options_json is inapplicable to selected call form");
  for(const auto* role:{"log_file","result_file","groups_json","metric_json","name_json"}){
   const bool required_role=std::string_view(role)=="log_file"?!serializer&&!helper:std::string_view(role)=="result_file"?serializer:std::string_view(role)=="groups_json"?index==17:std::string_view(role)=="metric_json"?index==10:index==11;
   if(required_role&&!find(role))invalid(std::string(role)+" is required");if(!required_role&&find(role))invalid(std::string(role)+" is inapplicable to selected operation");
  }
  selected=true;parsed={"operation","call_form"};
  if(okind){
   const auto* part=find("options_json");const auto bytes=part?part->bytes:std::string("{}");
   const std::array<std::string_view,5> kinds={"OrganizationalSnaOptions","RoleDiscoveryOptions","ResourceProfilesOptions","NetworkAnalysisOptions","LocalDiagnosticsOptions"};
   const std::array<std::string_view,5> labels={"prepare_organizational_sna_options","prepare_organizational_roles_options","prepare_organizational_profiles_options","prepare_organizational_network_options","prepare_organizational_local_options"};
   call(labels[static_cast<std::size_t>(okind-1)],"preparation",kinds[static_cast<std::size_t>(okind-1)],false,[&]{
    const auto doc=Parser(bytes).parse();switch(okind){case 1:options=read_record<OrganizationalSnaOptions>(doc,&supplied);break;case 2:options=read_record<RoleDiscoveryOptions>(doc,&supplied);break;case 3:options=read_record<ResourceProfilesOptions>(doc,&supplied);break;case 4:options=read_record<NetworkAnalysisOptions>(doc,&supplied);break;case 5:options=read_record<LocalDiagnosticsOptions>(doc,&supplied);break;default:invalid("unreachable options type");}options_prepared=true;
   });
   if(part)parsed.push_back("options_json");phase="result_serialization_error";configured=variant_json(options);artifacts["configured_options_json"]=configured;phase="configuration_error";
  }
  if(index==17){call("prepare_organizational_groups","preparation","NativeStringVectorMap",false,[&]{groups=read_value<OrganizationalWorkbenchGroups>(Parser(find("groups_json")->bytes).parse());groups_prepared=true;});parsed.push_back("groups_json");phase="result_serialization_error";before[2]=configured_groups=render(groups);artifacts["groups_input_json"]=before[2];phase="configuration_error";}
  if(index==10){call("prepare_organizational_metric","preparation","ResourceProfileMetric",false,[&]{metric=read_value<ResourceProfileMetric>(Parser(find("metric_json")->bytes).parse());metric_prepared=true;});parsed.push_back("metric_json");phase="result_serialization_error";before[3]=configured_metric=render(metric);artifacts["metric_input_json"]=before[3];phase="configuration_error";}
  if(index==11){call("prepare_organizational_name","preparation","NativeString",false,[&]{name=native_string(Parser(find("name_json")->bytes).parse());name_prepared=true;});parsed.push_back("name_json");phase="result_serialization_error";before[4]=configured_name=q(name);artifacts["name_input_json"]=before[4];phase="configuration_error";}
  phase="input_error";
  if(serializer){
   call("prepare_organizational_result","preparation",input_types[static_cast<std::size_t>(index)],false,[&]{
    const auto doc=Parser(find("result_file")->bytes).parse();switch(index){case 3:stored=read_record<OrganizationalSnaResult>(doc);break;case 6:stored=read_record<RoleDiscoveryResult>(doc);break;case 9:stored=read_record<ResourceProfilesResult>(doc);break;case 14:stored=read_record<NetworkAnalysisResult>(doc);break;case 18:stored=read_record<LocalDiagnosticsResult>(doc);break;default:invalid("unreachable stored result type");}result_prepared=true;
   });parsed.push_back("result_file");phase="result_serialization_error";before[1]=variant_json(stored);artifacts["result_input_json"]=before[1];
  }else if(!helper){call("parse_embedding_workbench_raw_log_json","preparation","EventLog",false,[&]{log=parse_embedding_workbench_raw_log_json(find("log_file")->bytes);log_prepared=true;});parsed.push_back("log_file");phase="result_serialization_error";before[0]=serialize_embedding_workbench_raw_log_json(log);artifacts["log_input_json"]=before[0];}
  phase="native_error";
  try{
   call(signatures[static_cast<std::size_t>(index)],"primary",index==17?"EventLog+NativeStringVectorMap":input_types[static_cast<std::size_t>(index)],defaults,[&]{
    entered=true;
    switch(index){
     case 0:result=defaults?discover_organizational_sna(log):discover_organizational_sna(log,std::get<OrganizationalSnaOptions>(options));break;
     case 1:result=defaults?mine_organizational_sna(log):mine_organizational_sna(log,std::get<OrganizationalSnaOptions>(options));break;
     case 2:result=defaults?discover_sna(log):discover_sna(log,std::get<OrganizationalSnaOptions>(options));break;
     case 3:result=organizational_sna_json(std::get<OrganizationalSnaResult>(stored));break;
     case 4:result=defaults?discover_roles(log):discover_roles(log,std::get<RoleDiscoveryOptions>(options));break;
     case 5:result=defaults?mine_roles(log):mine_roles(log,std::get<RoleDiscoveryOptions>(options));break;
     case 6:result=role_discovery_json(std::get<RoleDiscoveryResult>(stored));break;
     case 7:result=defaults?discover_resource_profiles(log):discover_resource_profiles(log,std::get<ResourceProfilesOptions>(options));break;
     case 8:result=defaults?mine_resource_profiles(log):mine_resource_profiles(log,std::get<ResourceProfilesOptions>(options));break;
     case 9:result=resource_profiles_json(std::get<ResourceProfilesResult>(stored));break;
     case 10:returned_pointer=resource_profile_metric_name(metric);break;
     case 11:result=resource_profile_metric_from_string(std::string_view(name.data(),name.size()));break;
     case 12:result=defaults?discover_network_analysis(log):discover_network_analysis(log,std::get<NetworkAnalysisOptions>(options));break;
     case 13:result=defaults?mine_network_analysis(log):mine_network_analysis(log,std::get<NetworkAnalysisOptions>(options));break;
     case 14:result=network_analysis_json(std::get<NetworkAnalysisResult>(stored));break;
     case 15:result=defaults?discover_local_diagnostics(log):discover_local_diagnostics(log,std::get<LocalDiagnosticsOptions>(options));break;
     case 16:result=defaults?apply_from_group_attribute(log):apply_from_group_attribute(log,std::get<LocalDiagnosticsOptions>(options));break;
     case 17:result=defaults?apply_from_clustering_or_roles(log,groups):apply_from_clustering_or_roles(log,groups,std::get<LocalDiagnosticsOptions>(options));break;
     case 18:result=local_diagnostics_json(std::get<LocalDiagnosticsResult>(stored));break;
     default:invalid("unreachable public operation");
    }
    // A static C-string pointer is a trivial native return. Its subsequent
    // allocating byte copy belongs to the host serialization phase below.
    returned=true;phase="result_serialization_error";
   });
  }catch(const std::exception&){const auto original=std::current_exception();const auto original_phase=phase;phase="result_serialization_error";snapshot_after();phase=original_phase;std::rethrow_exception(original);}
  phase="result_serialization_error";snapshot_after();
  if(index==10)result=std::string(returned_pointer);
  const auto full=variant_json(result);result_holder=typed(result_types[static_cast<std::size_t>(index)],full);artifacts["full_result_json"]=full;
  if(serializer||index==10){const auto& bytes=std::get<std::string>(result);native_output=carrier("native_output_bytes.bin",bytes);artifacts["native_output_bytes"]=bytes;}
 }catch(const std::exception& e){failure=error_json(phase,e);}
 calls+=']';std::string artifact_map="{";
 for(const auto& [n,bytes]:artifacts){if(artifact_map.size()>1)artifact_map+=',';artifact_map+=q(n)+':'+(bytes?carrier(n+(n=="native_output_bytes"?".bin":".json"),*bytes):"null");}artifact_map+='}';
 const auto maybe=[](const std::optional<bool>& v){return v?(*v?"true":"false"):"null";};
 const auto bit=[](bool v){return v?"true":"false";};
 const auto submitted=[&](std::string_view n){const auto* part=find(n);return part?carrier(part->file?part->filename:part->name,part->bytes):std::string("null");};
 const std::string parameter_mode=selected&&okind?q(defaults?"default_temporary":index==17?"value":"const_reference"):"null";
 OrganizationalWorkbenchResponse response;response.status=failure=="null"?200:phase=="result_serialization_error"?500:400;
 response.json="{\"family\":\"ORIGINAL_ORGANIZATIONAL_WORKBENCH\",\"version\":\"original-organizational-workbench-v1\",\"operation\":"+q(operation)+",\"call_form\":"+q(form)+",\"executed\":"+bit(returned)+",\"native_entered\":"+bit(entered)+",\"native_returned\":"+bit(returned)+",\"native_calls\":"+calls+",\"error\":"+failure+",\"originals\":"+originals+",\"parsed_controls\":"+render(parsed)+",\"supplied_controls\":"+controls
  +",\"input_context\":{\"size_t_bytes\":"+std::to_string(sizeof(std::size_t))+",\"int_bytes\":"+std::to_string(sizeof(int))+",\"input_type\":"+(selected?q(input_types[static_cast<std::size_t>(index)]):"null")+",\"options_parameter_mode\":"+parameter_mode+",\"log_prepared\":"+bit(log_prepared)+",\"result_prepared\":"+bit(result_prepared)+",\"options_prepared\":"+bit(options_prepared)+",\"groups_prepared\":"+bit(groups_prepared)+",\"metric_prepared\":"+bit(metric_prepared)+",\"name_prepared\":"+bit(name_prepared)+",\"log_unchanged\":"+maybe(log_unchanged)+",\"result_unchanged\":"+maybe(result_unchanged)+",\"options_unchanged\":"+maybe(options_unchanged)+",\"groups_unchanged\":"+maybe(groups_unchanged)+",\"metric_unchanged\":"+maybe(metric_unchanged)+",\"name_unchanged\":"+maybe(name_unchanged)+'}'
  +",\"input\":{\"before\":"+snapshots(before)+",\"after\":"+snapshots(after)+"},\"submitted_options\":"+submitted("options_json")+",\"configured_options\":"+configured+",\"supplied_options\":"+render(std::vector<std::string>(supplied.begin(),supplied.end()))+",\"submitted_groups\":"+submitted("groups_json")+",\"configured_groups\":"+configured_groups+",\"submitted_metric\":"+submitted("metric_json")+",\"configured_metric\":"+configured_metric+",\"submitted_name\":"+submitted("name_json")+",\"configured_name\":"+configured_name+",\"result\":"+result_holder+",\"native_output\":"+native_output+",\"artifact_bytes\":"+artifact_map+",\"artifact_errors\":[]}";
 return response;
}
} // namespace
OrganizationalSnaOptions parse_organizational_sna_options_json(std::string_view v,std::set<std::string>* supplied){return read_record<OrganizationalSnaOptions>(Parser(v).parse(),supplied);}
std::string serialize_organizational_workbench_options_json(const OrganizationalSnaOptions& v){return render(v);}
OrganizationalSnaResult parse_organizational_sna_result_json(std::string_view v){return read_record<OrganizationalSnaResult>(Parser(v).parse());}
std::string serialize_organizational_workbench_result_json(const OrganizationalSnaResult& v){return render(v);}
RoleDiscoveryOptions parse_organizational_roles_options_json(std::string_view v,std::set<std::string>* supplied){return read_record<RoleDiscoveryOptions>(Parser(v).parse(),supplied);}
std::string serialize_organizational_workbench_options_json(const RoleDiscoveryOptions& v){return render(v);}
RoleDiscoveryResult parse_organizational_roles_result_json(std::string_view v){return read_record<RoleDiscoveryResult>(Parser(v).parse());}
std::string serialize_organizational_workbench_result_json(const RoleDiscoveryResult& v){return render(v);}
ResourceProfilesOptions parse_organizational_profiles_options_json(std::string_view v,std::set<std::string>* supplied){return read_record<ResourceProfilesOptions>(Parser(v).parse(),supplied);}
std::string serialize_organizational_workbench_options_json(const ResourceProfilesOptions& v){return render(v);}
ResourceProfilesResult parse_organizational_profiles_result_json(std::string_view v){return read_record<ResourceProfilesResult>(Parser(v).parse());}
std::string serialize_organizational_workbench_result_json(const ResourceProfilesResult& v){return render(v);}
NetworkAnalysisOptions parse_organizational_network_options_json(std::string_view v,std::set<std::string>* supplied){return read_record<NetworkAnalysisOptions>(Parser(v).parse(),supplied);}
std::string serialize_organizational_workbench_options_json(const NetworkAnalysisOptions& v){return render(v);}
NetworkAnalysisResult parse_organizational_network_result_json(std::string_view v){return read_record<NetworkAnalysisResult>(Parser(v).parse());}
std::string serialize_organizational_workbench_result_json(const NetworkAnalysisResult& v){return render(v);}
LocalDiagnosticsOptions parse_organizational_local_options_json(std::string_view v,std::set<std::string>* supplied){return read_record<LocalDiagnosticsOptions>(Parser(v).parse(),supplied);}
std::string serialize_organizational_workbench_options_json(const LocalDiagnosticsOptions& v){return render(v);}
LocalDiagnosticsResult parse_organizational_local_result_json(std::string_view v){return read_record<LocalDiagnosticsResult>(Parser(v).parse());}
std::string serialize_organizational_workbench_result_json(const LocalDiagnosticsResult& v){return render(v);}
OrganizationalWorkbenchGroups parse_organizational_workbench_groups_json(std::string_view v){return read_value<OrganizationalWorkbenchGroups>(Parser(v).parse());}
std::string serialize_organizational_workbench_groups_json(const OrganizationalWorkbenchGroups& v){return render(v);}
ResourceProfileMetric parse_organizational_workbench_metric_json(std::string_view v){return read_value<ResourceProfileMetric>(Parser(v).parse());}
std::string serialize_organizational_workbench_metric_json(ResourceProfileMetric v){return render(v);}
std::string parse_organizational_workbench_name_json(std::string_view v){return native_string(Parser(v).parse());}
OrganizationalWorkbenchResponse execute_organizational_workbench(const std::vector<DiscoveryWorkbenchPart>& p,const std::vector<DiscoveryWorkbenchPart>& query){return execute(p,query);}
OrganizationalWorkbenchResponse execute_organizational_workbench_multipart(std::string_view content,std::string_view body,const std::vector<DiscoveryWorkbenchPart>& query,bool complete){
 if(!complete)return execute({},query,"multipart transport did not complete");
 std::vector<DiscoveryWorkbenchPart> p;try{p=parse_discovery_workbench_multipart(content,body);}catch(const std::exception& e){return execute({},query,e.what());}return execute(p,query);
}
} // namespace minera
