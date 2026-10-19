#include "minera/organizational_workbench_json.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/footprints_workbench_json.hpp"
#include "minera/literal_controls_json.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
using namespace minera;
namespace {
// An independent fragment slicer, not the implementation's bounded parser.
// Canonical outputs may be deeper/larger than accepted input documents.
std::map<std::string,std::string> fields(const std::string& text){
 assert(text.size()>=2&&text.front()=='{'&&text.back()=='}');std::map<std::string,std::string> out;std::size_t at=1;
 while(at+1<text.size()){
  assert(text[at++]=='"');const auto begin=at;while(text[at]!='"')++at;const auto name=text.substr(begin,at-begin);assert(text[++at]==':');++at;
  const auto start=at;int depth=0;bool quoted=false;
  for(;at<text.size();++at){const auto c=text[at];if(quoted){if(c=='\\')++at;else if(c=='"')quoted=false;continue;}if(c=='"')quoted=true;else if(c=='{'||c=='[')++depth;else if(c=='}'||c==']'){if(!depth)break;--depth;}else if(c==','&&!depth)break;}
  assert(out.emplace(name,text.substr(start,at-start)).second);if(text[at]==',')++at;
 }
 return out;
}
std::vector<std::string> elements(const std::string& text){
 assert(text.size()>=2&&text.front()=='['&&text.back()==']');std::vector<std::string> out;std::size_t at=1;
 while(at+1<text.size()){const auto start=at;int depth=0;bool quoted=false;for(;at<text.size();++at){const auto c=text[at];if(quoted){if(c=='\\')++at;else if(c=='"')quoted=false;continue;}if(c=='"')quoted=true;else if(c=='{'||c=='[')++depth;else if(c=='}'||c==']'){if(!depth)break;--depth;}else if(c==','&&!depth)break;}out.push_back(text.substr(start,at-start));if(text[at]==',')++at;}
 return out;
}
std::string q(const std::string& value){return serialize_embedding_workbench_string_json(value);}
std::string text(const std::string& value){return parse_literal_string_vector_json("["+value+"]").front();}
std::string bytes(const std::string& carrier){
 const auto c=fields(carrier);const auto data=text(c.at("data"));std::string value;
 if(c.at("encoding")=="\"utf8\"")value=data;
 else{assert(c.at("encoding")=="\"hex\""&&data.size()%2==0);for(std::size_t i=0;i<data.size();i+=2)value+=static_cast<char>(std::stoul(data.substr(i,2),nullptr,16));}
 assert(c.at("bytes")==std::to_string(value.size())&&c.at("sha256")==q(footprints_workbench_sha256(value)));return value;
}
std::string raw(const EventLog& log){return serialize_embedding_workbench_raw_log_json(log);}
std::string all_bytes(){std::string value;for(int i=0;i<256;++i)value+=static_cast<char>(i);return value;}
struct Direct {bool returned=false;std::string value,message,type;};
Direct native(const std::function<std::string()>& function){
 try{return {true,function(),{}, {}};}catch(const std::invalid_argument& e){return {false,{},e.what(),"invalid_argument"};}catch(const std::out_of_range& e){return {false,{},e.what(),"out_of_range"};}catch(const std::length_error& e){return {false,{},e.what(),"length_error"};}catch(const std::runtime_error& e){return {false,{},e.what(),"runtime_error"};}
}
template<class F>void rejects(F function){bool rejected=false;try{function();}catch(const std::invalid_argument&){rejected=true;}assert(rejected);}
EventLog log_sample(){
 EventLog log;log.attributes={{"log-only","retained"}};log.metadata.extensions={{"ext","prefix","urn:source"}};log.metadata.classifiers={{"classifier",{"concept:name","extra"}}};
 log.metadata.trace_globals={{"global-trace","retained"}};log.metadata.event_globals={{"global-event","retained"}};log.metadata.properties={{"property","retained"}};log.metadata.globals={{"event",{{"ordered","one"}}},{"event",{{"ordered","two"}}}};
 log.metadata.compatibility_snapshot_valid=true;log.metadata.compatibility_globals_snapshot={{"old",{{"snapshot","global"}}}};log.metadata.compatibility_trace_globals_snapshot={{"snapshot","trace"}};log.metadata.compatibility_event_globals_snapshot={{"snapshot","event"}};
 for(std::size_t i=0;i<6;++i){Trace t;t.case_id=i<2?"repeated":"case-"+std::to_string(i);t.attributes={{"group",i%2?"left":"right"},{"number",std::to_string(i),AttributeKind::kInt}};t.properties={{"trace-property","kept"}};
  const auto sequence=i<3?std::vector<std::string>{"A","B"}:std::vector<std::string>{"B","A","C"};for(std::size_t e=0;e<sequence.size();++e){Event v;v.case_id="event-id-not-trace-id";v.activity=sequence[e];v.timestamp=std::to_string(e);v.resource="resource-"+std::to_string(i%2);v.lifecycle=e%2?"complete":"start";v.input_order=e;v.attributes={{"alternate",sequence[e]},{"amount",std::to_string(i+e),AttributeKind::kFloat},{"choice",i%2?"L":"R"}};t.events.push_back(std::move(v));}log.traces.push_back(std::move(t));}
 return log;
}
const std::array<std::string_view,19> operations={"discover_organizational_sna","mine_organizational_sna","discover_sna","organizational_sna_json","discover_roles","mine_roles","role_discovery_json","discover_resource_profiles","mine_resource_profiles","resource_profiles_json","resource_profile_metric_name","resource_profile_metric_from_string","discover_network_analysis","mine_network_analysis","network_analysis_json","discover_local_diagnostics","apply_from_group_attribute","apply_from_clustering_or_roles","local_diagnostics_json"};
const std::array<std::string_view,19> signatures={"discover_organizational_sna(const EventLog&,const OrganizationalSnaOptions&)","mine_organizational_sna(const EventLog&,const OrganizationalSnaOptions&)","discover_sna(const EventLog&,const OrganizationalSnaOptions&)","organizational_sna_json(const OrganizationalSnaResult&)","discover_roles(const EventLog&,const RoleDiscoveryOptions&)","mine_roles(const EventLog&,const RoleDiscoveryOptions&)","role_discovery_json(const RoleDiscoveryResult&)","discover_resource_profiles(const EventLog&,const ResourceProfilesOptions&)","mine_resource_profiles(const EventLog&,const ResourceProfilesOptions&)","resource_profiles_json(const ResourceProfilesResult&)","resource_profile_metric_name(ResourceProfileMetric)","resource_profile_metric_from_string(std::string_view)","discover_network_analysis(const EventLog&,const NetworkAnalysisOptions&)","mine_network_analysis(const EventLog&,const NetworkAnalysisOptions&)","network_analysis_json(const NetworkAnalysisResult&)","discover_local_diagnostics(const EventLog&,const LocalDiagnosticsOptions&)","apply_from_group_attribute(const EventLog&,const LocalDiagnosticsOptions&)","apply_from_clustering_or_roles(const EventLog&,const std::map<std::string,std::vector<std::string>>&,LocalDiagnosticsOptions)","local_diagnostics_json(const LocalDiagnosticsResult&)"};
const std::array<std::string_view,19> input_types={"EventLog","EventLog","EventLog","OrganizationalSnaResult","EventLog","EventLog","RoleDiscoveryResult","EventLog","EventLog","ResourceProfilesResult","ResourceProfileMetric","std::string_view","EventLog","EventLog","NetworkAnalysisResult","EventLog","EventLog","EventLog","LocalDiagnosticsResult"};
const std::array<std::string_view,19> result_types={"OrganizationalSnaResult","OrganizationalSnaResult","OrganizationalSnaResult","NativeString","RoleDiscoveryResult","RoleDiscoveryResult","NativeString","ResourceProfilesResult","ResourceProfilesResult","NativeString","NativeString","ResourceProfileMetric","NetworkAnalysisResult","NetworkAnalysisResult","NativeString","LocalDiagnosticsResult","LocalDiagnosticsResult","LocalDiagnosticsResult","NativeString"};
std::set<std::pair<std::size_t,bool>> observed_forms;
using Parts=std::vector<DiscoveryWorkbenchPart>;
Parts request(std::size_t index,const std::string& input,const std::optional<std::string>& options=std::nullopt,bool defaults=false,const std::optional<std::string>& groups=std::nullopt){
 const std::string role=index==10?"metric_json":index==11?"name_json":index==3||index==6||index==9||index==14||index==18?"result_file":"log_file";
 Parts p{{"operation","",std::string(operations[index]),false},{role,"input.json",input,true}};
 if(options)p.push_back({"options_json","",*options,false});if(defaults)p.push_back({"call_form","","default_options",false});if(groups)p.push_back({"groups_json","",*groups,false});return p;
}
const std::array<std::string,11> artifact_names={"log_input_json","log_after_json","result_input_json","result_after_json","configured_options_json","groups_input_json","groups_after_json","metric_input_json","name_input_json","full_result_json","native_output_bytes"};
std::map<std::string,std::string> check(const OrganizationalWorkbenchResponse& response,const Direct& direct,std::size_t index,bool defaults=false){
 observed_forms.emplace(index,defaults);const auto r=fields(response.json);assert(r.size()==27&&r.at("family")=="\"ORIGINAL_ORGANIZATIONAL_WORKBENCH\""&&r.at("version")=="\"original-organizational-workbench-v1\"");
 assert(r.at("native_entered")=="true"&&r.at("native_returned")== (direct.returned?"true":"false")&&r.at("executed")==r.at("native_returned"));
 std::size_t primary=0;for(const auto& row:elements(r.at("native_calls"))){const auto c=fields(row);assert(c.size()==7&&c.at("entered")=="true");if(c.at("stage")=="\"primary\""){++primary;assert(c.at("name")==q(std::string(signatures[index]))&&c.at("input_kind")==q(index==17?"EventLog+NativeStringVectorMap":std::string(input_types[index]))&&c.at("default_arguments_used")== (defaults?"true":"false")&&c.at("returned")==r.at("native_returned")&&c.at("error")== (direct.returned?"null":q(direct.message)));}}
 assert(primary==1);
 if(direct.returned){assert(response.status==200&&r.at("error")=="null");const auto result=fields(r.at("result"));assert(result.at("type")==q(std::string(result_types[index]))&&result.at("value")==direct.value);}
 else{assert(response.status==400&&r.at("result")=="null"&&r.at("native_output")=="null");const auto e=fields(r.at("error"));assert(e.at("phase")=="\"native_error\""&&e.at("type")==q(direct.type)&&e.at("message")==q(direct.message));}
 const auto input=fields(r.at("input")),ctx=fields(r.at("input_context"));assert(input.at("before")==input.at("after"));assert(ctx.at("size_t_bytes")==std::to_string(sizeof(std::size_t))&&ctx.at("int_bytes")==std::to_string(sizeof(int)));
 const auto a=fields(r.at("artifact_bytes"));assert(a.size()==11&&r.at("artifact_errors")=="[]");for(const auto& n:artifact_names){assert(a.contains(n));if(a.at(n)!="null"){assert(text(fields(a.at(n)).at("dataset"))==n+(n=="native_output_bytes"?".bin":".json"));(void)bytes(a.at(n));}}
 const auto before=fields(input.at("before")),after=fields(input.at("after"));
 for(const auto* key:{"log","result","groups","metric","name"})if(ctx.at(std::string(key)+"_prepared")=="true"){
  assert(ctx.at(std::string(key)+"_unchanged")=="true");const std::string artifact=std::string(key)+"_input_json";assert(bytes(a.at(artifact))==before.at(key));if(std::string(key)!="metric"&&std::string(key)!="name")assert(bytes(a.at(std::string(key)+"_after_json"))==after.at(key));
 }
 if(ctx.at("options_prepared")=="true"){assert(ctx.at("options_unchanged")== (defaults?"null":"true"));assert(bytes(a.at("configured_options_json"))==r.at("configured_options"));assert(ctx.at("options_parameter_mode")==q(defaults?"default_temporary":index==17?"value":"const_reference"));}
 else assert(ctx.at("options_parameter_mode")=="null");
 if(direct.returned){assert(bytes(a.at("full_result_json"))==direct.value);if(result_types[index]=="NativeString"){assert(bytes(a.at("native_output_bytes"))==text(direct.value)&&bytes(r.at("native_output"))==text(direct.value));}else assert(a.at("native_output_bytes")=="null"&&r.at("native_output")=="null");}
 else assert(a.at("full_result_json")=="null"&&a.at("native_output_bytes")=="null");
 return r;
}
void host_error(const OrganizationalWorkbenchResponse& response,const std::string& phase){const auto r=fields(response.json);assert(response.status==400&&r.at("native_entered")=="false"&&r.at("native_returned")=="false"&&r.at("result")=="null");assert(fields(r.at("error")).at("phase")==q(phase));for(const auto& c:elements(r.at("native_calls")))assert(fields(c).at("stage")!="\"primary\"");assert(fields(r.at("artifact_bytes")).size()==11);}
void compare_metric(ResourceProfileMetric metric){const auto direct=native([&]{return q(std::string(resource_profile_metric_name(metric)));});check(execute_organizational_workbench(request(10,serialize_organizational_workbench_metric_json(metric))),direct,10);}
void compare_name(const std::string& value){const auto direct=native([&]{return serialize_organizational_workbench_metric_json(resource_profile_metric_from_string(std::string_view(value.data(),value.size())));});check(execute_organizational_workbench(request(11,q(value))),direct,11);}
template<class T,class F>void compare_result(std::size_t index,const T& value,F function){const auto input=serialize_organizational_workbench_result_json(value);const auto direct=native([&]{return q(function(value));});check(execute_organizational_workbench(request(index,input)),direct,index);}
EventLog full_log(){
 auto log=log_sample();for(auto& t:log.traces)for(std::size_t i=0;i<t.events.size();++i){auto& e=t.events[i];e.timestamp=std::array<std::string,3>{"2026-10-19T08:00:00Z","2026-10-19T09:00:00Z","2026-10-19T10:00:00Z"}[i];e.attributes.push_back({"org:group",i%2?"G2":"G1"});e.attributes.push_back({"case:custom","joined"});e.attributes.push_back({"resource-x",e.resource});e.attributes.push_back({"activity-x",e.activity});e.attributes.push_back({"stamp-x",e.timestamp});e.attributes.push_back({"start-x","2026-10-19T07:00:00Z"});}
 const auto octets=all_bytes();log.attributes.push_back({octets,octets});auto& a=log.attributes.back();a.kind=static_cast<AttributeKind>(-11);a.container=static_cast<AttributeContainerKind>(-7);a.scalar_wrapper=true;a.scalar_children_list=true;a.first_class_role=static_cast<FirstClassAttributeRole>(-9);a.first_class_state=static_cast<FirstClassAttributeState>(-3);a.first_class_source_key=octets;a.children={{"i","inactive",AttributeKind::kInt},{"u","inactive",AttributeKind::kInt},{"nan","NaN",AttributeKind::kFloat},{"zero","-0",AttributeKind::kFloat},{"date",octets,AttributeKind::kDate},{"bool","false",AttributeKind::kBoolean}};
 a.children[0].typed_value=std::numeric_limits<std::int64_t>::max();a.children[1].typed_value=std::numeric_limits<std::uint64_t>::max();a.children[2].typed_value=std::numeric_limits<double>::quiet_NaN();a.children[3].typed_value=-0.0;a.children[4].typed_value=NativeDate{octets,static_cast<NativeDateResolution>(-4),octets};a.children[5].typed_value=false;
 log.metadata.compatibility_globals_snapshot.push_back({octets,{{octets,octets}}});log.metadata.compatibility_trace_globals_snapshot.push_back(a);log.metadata.compatibility_event_globals_snapshot.push_back(a);
 const auto raw=serialize_embedding_workbench_raw_log_json(log);assert(raw.starts_with("{\"native_byte_strings\":\"hex-v1\""));const auto parsed=parse_embedding_workbench_raw_log_json(raw);assert(serialize_embedding_workbench_raw_log_json(parsed)==raw);
 const auto metadata=fields(fields(fields(raw).at("value")).at("metadata"));assert(metadata.size()==10&&metadata.at("compatibility_snapshot_valid")=="true");
 assert(elements(metadata.at("compatibility_globals_snapshot")).size()==2&&elements(metadata.at("compatibility_trace_globals_snapshot")).size()==2&&elements(metadata.at("compatibility_event_globals_snapshot")).size()==2);
 return log;
}
std::map<std::string,std::string> compare_discover_organizational_sna(const EventLog& log,const OrganizationalSnaOptions& options={},bool defaults=false,bool omit=false){
 const auto expected=native([&]{return serialize_organizational_workbench_result_json(defaults?discover_organizational_sna(log):discover_organizational_sna(log,options));});
 const auto opts=defaults||omit?std::nullopt:std::make_optional(serialize_organizational_workbench_options_json(options));
 const auto r=check(execute_organizational_workbench(request(0,raw(log),opts,defaults,std::nullopt)),expected,0,defaults);
 assert(r.at("configured_options")==serialize_organizational_workbench_options_json(defaults?OrganizationalSnaOptions{}:options));
 return r;
}
std::map<std::string,std::string> compare_mine_organizational_sna(const EventLog& log,const OrganizationalSnaOptions& options={},bool defaults=false,bool omit=false){
 const auto expected=native([&]{return serialize_organizational_workbench_result_json(defaults?mine_organizational_sna(log):mine_organizational_sna(log,options));});
 const auto opts=defaults||omit?std::nullopt:std::make_optional(serialize_organizational_workbench_options_json(options));
 const auto r=check(execute_organizational_workbench(request(1,raw(log),opts,defaults,std::nullopt)),expected,1,defaults);
 assert(r.at("configured_options")==serialize_organizational_workbench_options_json(defaults?OrganizationalSnaOptions{}:options));
 return r;
}
std::map<std::string,std::string> compare_discover_sna(const EventLog& log,const OrganizationalSnaOptions& options={},bool defaults=false,bool omit=false){
 const auto expected=native([&]{return serialize_organizational_workbench_result_json(defaults?discover_sna(log):discover_sna(log,options));});
 const auto opts=defaults||omit?std::nullopt:std::make_optional(serialize_organizational_workbench_options_json(options));
 const auto r=check(execute_organizational_workbench(request(2,raw(log),opts,defaults,std::nullopt)),expected,2,defaults);
 assert(r.at("configured_options")==serialize_organizational_workbench_options_json(defaults?OrganizationalSnaOptions{}:options));
 return r;
}
std::map<std::string,std::string> compare_discover_roles(const EventLog& log,const RoleDiscoveryOptions& options={},bool defaults=false,bool omit=false){
 const auto expected=native([&]{return serialize_organizational_workbench_result_json(defaults?discover_roles(log):discover_roles(log,options));});
 const auto opts=defaults||omit?std::nullopt:std::make_optional(serialize_organizational_workbench_options_json(options));
 const auto r=check(execute_organizational_workbench(request(4,raw(log),opts,defaults,std::nullopt)),expected,4,defaults);
 assert(r.at("configured_options")==serialize_organizational_workbench_options_json(defaults?RoleDiscoveryOptions{}:options));
 return r;
}
std::map<std::string,std::string> compare_mine_roles(const EventLog& log,const RoleDiscoveryOptions& options={},bool defaults=false,bool omit=false){
 const auto expected=native([&]{return serialize_organizational_workbench_result_json(defaults?mine_roles(log):mine_roles(log,options));});
 const auto opts=defaults||omit?std::nullopt:std::make_optional(serialize_organizational_workbench_options_json(options));
 const auto r=check(execute_organizational_workbench(request(5,raw(log),opts,defaults,std::nullopt)),expected,5,defaults);
 assert(r.at("configured_options")==serialize_organizational_workbench_options_json(defaults?RoleDiscoveryOptions{}:options));
 return r;
}
std::map<std::string,std::string> compare_discover_resource_profiles(const EventLog& log,const ResourceProfilesOptions& options={},bool defaults=false,bool omit=false){
 const auto expected=native([&]{return serialize_organizational_workbench_result_json(defaults?discover_resource_profiles(log):discover_resource_profiles(log,options));});
 const auto opts=defaults||omit?std::nullopt:std::make_optional(serialize_organizational_workbench_options_json(options));
 const auto r=check(execute_organizational_workbench(request(7,raw(log),opts,defaults,std::nullopt)),expected,7,defaults);
 assert(r.at("configured_options")==serialize_organizational_workbench_options_json(defaults?ResourceProfilesOptions{}:options));
 return r;
}
std::map<std::string,std::string> compare_mine_resource_profiles(const EventLog& log,const ResourceProfilesOptions& options={},bool defaults=false,bool omit=false){
 const auto expected=native([&]{return serialize_organizational_workbench_result_json(defaults?mine_resource_profiles(log):mine_resource_profiles(log,options));});
 const auto opts=defaults||omit?std::nullopt:std::make_optional(serialize_organizational_workbench_options_json(options));
 const auto r=check(execute_organizational_workbench(request(8,raw(log),opts,defaults,std::nullopt)),expected,8,defaults);
 assert(r.at("configured_options")==serialize_organizational_workbench_options_json(defaults?ResourceProfilesOptions{}:options));
 return r;
}
std::map<std::string,std::string> compare_discover_network_analysis(const EventLog& log,const NetworkAnalysisOptions& options={},bool defaults=false,bool omit=false){
 const auto expected=native([&]{return serialize_organizational_workbench_result_json(defaults?discover_network_analysis(log):discover_network_analysis(log,options));});
 const auto opts=defaults||omit?std::nullopt:std::make_optional(serialize_organizational_workbench_options_json(options));
 const auto r=check(execute_organizational_workbench(request(12,raw(log),opts,defaults,std::nullopt)),expected,12,defaults);
 assert(r.at("configured_options")==serialize_organizational_workbench_options_json(defaults?NetworkAnalysisOptions{}:options));
 return r;
}
std::map<std::string,std::string> compare_mine_network_analysis(const EventLog& log,const NetworkAnalysisOptions& options={},bool defaults=false,bool omit=false){
 const auto expected=native([&]{return serialize_organizational_workbench_result_json(defaults?mine_network_analysis(log):mine_network_analysis(log,options));});
 const auto opts=defaults||omit?std::nullopt:std::make_optional(serialize_organizational_workbench_options_json(options));
 const auto r=check(execute_organizational_workbench(request(13,raw(log),opts,defaults,std::nullopt)),expected,13,defaults);
 assert(r.at("configured_options")==serialize_organizational_workbench_options_json(defaults?NetworkAnalysisOptions{}:options));
 return r;
}
std::map<std::string,std::string> compare_discover_local_diagnostics(const EventLog& log,const LocalDiagnosticsOptions& options={},bool defaults=false,bool omit=false){
 const auto expected=native([&]{return serialize_organizational_workbench_result_json(defaults?discover_local_diagnostics(log):discover_local_diagnostics(log,options));});
 const auto opts=defaults||omit?std::nullopt:std::make_optional(serialize_organizational_workbench_options_json(options));
 const auto r=check(execute_organizational_workbench(request(15,raw(log),opts,defaults,std::nullopt)),expected,15,defaults);
 assert(r.at("configured_options")==serialize_organizational_workbench_options_json(defaults?LocalDiagnosticsOptions{}:options));
 return r;
}
std::map<std::string,std::string> compare_apply_from_group_attribute(const EventLog& log,const LocalDiagnosticsOptions& options={},bool defaults=false,bool omit=false){
 const auto expected=native([&]{return serialize_organizational_workbench_result_json(defaults?apply_from_group_attribute(log):apply_from_group_attribute(log,options));});
 const auto opts=defaults||omit?std::nullopt:std::make_optional(serialize_organizational_workbench_options_json(options));
 const auto r=check(execute_organizational_workbench(request(16,raw(log),opts,defaults,std::nullopt)),expected,16,defaults);
 assert(r.at("configured_options")==serialize_organizational_workbench_options_json(defaults?LocalDiagnosticsOptions{}:options));
 return r;
}
std::map<std::string,std::string> compare_apply_from_clustering_or_roles(const EventLog& log,const LocalDiagnosticsOptions& options={},bool defaults=false,bool omit=false,const OrganizationalWorkbenchGroups& groups={}){
 const auto expected=native([&]{return serialize_organizational_workbench_result_json(defaults?apply_from_clustering_or_roles(log,groups):apply_from_clustering_or_roles(log,groups,options));});
 const auto opts=defaults||omit?std::nullopt:std::make_optional(serialize_organizational_workbench_options_json(options));
 const auto r=check(execute_organizational_workbench(request(17,raw(log),opts,defaults,std::make_optional(serialize_organizational_workbench_groups_json(groups)))),expected,17,defaults);
 assert(r.at("configured_options")==serialize_organizational_workbench_options_json(defaults?LocalDiagnosticsOptions{}:options));
 return r;
}
void roundtrip_OrganizationalSnaOptions(){const OrganizationalSnaOptions value{};const auto encoded=serialize_organizational_workbench_options_json(value);assert(fields(encoded).size()==9);std::set<std::string> supplied;const auto parsed=parse_organizational_sna_options_json(encoded,&supplied);assert(serialize_organizational_workbench_options_json(parsed)==encoded);assert(supplied.size()==9);}
void roundtrip_OrganizationalSnaResult(){const OrganizationalSnaResult value{};const auto encoded=serialize_organizational_workbench_result_json(value);assert(fields(encoded).size()==5);std::set<std::string> supplied;const auto parsed=parse_organizational_sna_result_json(encoded);assert(serialize_organizational_workbench_result_json(parsed)==encoded);}
void roundtrip_RoleDiscoveryOptions(){const RoleDiscoveryOptions value{};const auto encoded=serialize_organizational_workbench_options_json(value);assert(fields(encoded).size()==6);std::set<std::string> supplied;const auto parsed=parse_organizational_roles_options_json(encoded,&supplied);assert(serialize_organizational_workbench_options_json(parsed)==encoded);assert(supplied.size()==6);}
void roundtrip_RoleDiscoveryResult(){const RoleDiscoveryResult value{};const auto encoded=serialize_organizational_workbench_result_json(value);assert(fields(encoded).size()==3);std::set<std::string> supplied;const auto parsed=parse_organizational_roles_result_json(encoded);assert(serialize_organizational_workbench_result_json(parsed)==encoded);}
void roundtrip_ResourceProfilesOptions(){const ResourceProfilesOptions value{};const auto encoded=serialize_organizational_workbench_options_json(value);assert(fields(encoded).size()==13);std::set<std::string> supplied;const auto parsed=parse_organizational_profiles_options_json(encoded,&supplied);assert(serialize_organizational_workbench_options_json(parsed)==encoded);assert(supplied.size()==13);}
void roundtrip_ResourceProfilesResult(){const ResourceProfilesResult value{};const auto encoded=serialize_organizational_workbench_result_json(value);assert(fields(encoded).size()==3);std::set<std::string> supplied;const auto parsed=parse_organizational_profiles_result_json(encoded);assert(serialize_organizational_workbench_result_json(parsed)==encoded);}
void roundtrip_NetworkAnalysisOptions(){const NetworkAnalysisOptions value{};const auto encoded=serialize_organizational_workbench_options_json(value);assert(fields(encoded).size()==14);std::set<std::string> supplied;const auto parsed=parse_organizational_network_options_json(encoded,&supplied);assert(serialize_organizational_workbench_options_json(parsed)==encoded);assert(supplied.size()==14);}
void roundtrip_NetworkAnalysisResult(){const NetworkAnalysisResult value{};const auto encoded=serialize_organizational_workbench_result_json(value);assert(fields(encoded).size()==3);std::set<std::string> supplied;const auto parsed=parse_organizational_network_result_json(encoded);assert(serialize_organizational_workbench_result_json(parsed)==encoded);}
void roundtrip_LocalDiagnosticsOptions(){const LocalDiagnosticsOptions value{};const auto encoded=serialize_organizational_workbench_options_json(value);assert(fields(encoded).size()==7);std::set<std::string> supplied;const auto parsed=parse_organizational_local_options_json(encoded,&supplied);assert(serialize_organizational_workbench_options_json(parsed)==encoded);assert(supplied.size()==7);}
void roundtrip_LocalDiagnosticsResult(){const LocalDiagnosticsResult value{};const auto encoded=serialize_organizational_workbench_result_json(value);assert(fields(encoded).size()==5);std::set<std::string> supplied;const auto parsed=parse_organizational_local_result_json(encoded);assert(serialize_organizational_workbench_result_json(parsed)==encoded);}
void test_all_forms(){
 const auto log=full_log();
 compare_discover_organizational_sna(log);compare_discover_organizational_sna(log,{},true);
 compare_mine_organizational_sna(log);compare_mine_organizational_sna(log,{},true);
 compare_discover_sna(log);compare_discover_sna(log,{},true);
 compare_discover_roles(log);compare_discover_roles(log,{},true);compare_mine_roles(log);compare_mine_roles(log,{},true);
 compare_discover_resource_profiles(log);compare_discover_resource_profiles(log,{},true);compare_mine_resource_profiles(log);compare_mine_resource_profiles(log,{},true);
 compare_discover_network_analysis(log);compare_discover_network_analysis(log,{},true);compare_mine_network_analysis(log);compare_mine_network_analysis(log,{},true);
 compare_discover_local_diagnostics(log);compare_discover_local_diagnostics(log,{},true);compare_apply_from_group_attribute(log);compare_apply_from_group_attribute(log,{},true);
 compare_apply_from_clustering_or_roles(log);compare_apply_from_clustering_or_roles(log,{},true);
 compare_result(3,OrganizationalSnaResult{},organizational_sna_json);compare_result(6,RoleDiscoveryResult{},role_discovery_json);compare_result(9,ResourceProfilesResult{},resource_profiles_json);compare_result(14,NetworkAnalysisResult{},network_analysis_json);compare_result(18,LocalDiagnosticsResult{},local_diagnostics_json);
 compare_metric(ResourceProfileMetric{});compare_name("distinct_activities");assert(observed_forms.size()==31);
 // Explicit argument with omitted options storage is distinct from compiler
 // default argument use, although both begin from the same public defaults.
 compare_discover_organizational_sna(log,{},false,true);compare_discover_roles(log,{},false,true);compare_discover_resource_profiles(log,{},false,true);compare_discover_network_analysis(log,{},false,true);compare_discover_local_diagnostics(log,{},false,true);compare_apply_from_clustering_or_roles(log,{},false,true);
 for(const auto& source:{EventLog{},log}){compare_discover_organizational_sna(source);compare_discover_roles(source);compare_discover_resource_profiles(source);compare_discover_network_analysis(source);compare_discover_local_diagnostics(source);}
}
void test_options_and_native_domains(){
 const auto log=full_log();OrganizationalSnaOptions s;s.activity_attribute="activity-x";s.resource_attribute="resource-x";s.beta=0.5;s.n=3;s.metric_normalization=true;s.max_resources=100;s.max_edges=1;
 for(int v=0;v<8;++v){s.variant=static_cast<OrganizationalSnaVariant>(v);compare_discover_organizational_sna(log,s);}
 s.variant=static_cast<OrganizationalSnaVariant>(std::numeric_limits<int>::min());compare_discover_organizational_sna(log,s);s.case_id_attribute="case:custom";compare_discover_organizational_sna(log,s);
 for(const auto v:{OrganizationalSnaVariant::kHandoverPandas,OrganizationalSnaVariant::kWorkingTogetherPandas,OrganizationalSnaVariant::kSubcontractingPandas}){s.variant=v;compare_discover_organizational_sna(log,s);s.case_id_attribute="missing";compare_discover_organizational_sna(log,s);s.case_id_attribute="case:custom";}
 for(const auto beta:{-1.0,2.0,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity(),-0.0}){s={};s.beta=beta;compare_discover_organizational_sna(log,s);}
 s={};s.n=0;compare_discover_organizational_sna(log,s);s.n=10001;compare_discover_organizational_sna(log,s);s={};s.max_resources=1;compare_discover_organizational_sna(log,s);s={};s.max_edges=1000001;compare_discover_organizational_sna(log,s);
 RoleDiscoveryOptions r;r.threshold=0.25;r.variant=RoleDiscoveryVariant::kPandas;r.activity_attribute="activity-x";r.resource_attribute="resource-x";r.max_resources=100;r.max_activities=100;
 compare_discover_roles(log,r);compare_mine_roles(log,r);r.variant=static_cast<RoleDiscoveryVariant>(-1);compare_discover_roles(log,r);r={};r.threshold=std::numeric_limits<double>::quiet_NaN();compare_discover_roles(log,r);r.threshold=1.0;compare_discover_roles(log,r);r.threshold=0.0;compare_discover_roles(log,r);r.max_resources=0;compare_discover_roles(log,r);r={};r.max_activities=1;compare_discover_roles(log,r);
 ResourceProfilesOptions p;p.activity_attribute="activity-x";p.resource_attribute="resource-x";p.timestamp_attribute="stamp-x";p.start_timestamp_attribute="start-x";p.case_id_attribute="case:custom";p.t1="2026-10-19T06:00:00Z";p.t2="2026-10-19T11:00:00Z";p.resource="resource-0";p.activity="A";p.resource2="resource-1";p.max_resources=100;p.max_rows=1;
 for(int v=0;v<11;++v){p.metric=static_cast<ResourceProfileMetric>(v);compare_discover_resource_profiles(log,p);compare_metric(p.metric);compare_name(resource_profile_metric_name(p.metric));}
 p.metric=static_cast<ResourceProfileMetric>(std::numeric_limits<int>::max());compare_discover_resource_profiles(log,p);compare_mine_resource_profiles(log,p);compare_metric(p.metric);compare_metric(static_cast<ResourceProfileMetric>(std::numeric_limits<int>::min()));
 for(const auto value:{std::string("unknown"),std::string("DISTINCT_ACTIVITIES"),std::string("distinct_activities\0suffix",26),std::string{},all_bytes()})compare_name(value);
 p={};p.metric=ResourceProfileMetric::kAverageWorkload;compare_discover_resource_profiles(log,p);p.metric=ResourceProfileMetric::kMultitasking;compare_discover_resource_profiles(log,p);p.metric=ResourceProfileMetric::kInteractionTwoResources;compare_discover_resource_profiles(log,p);p={};p.t1="bad";compare_discover_resource_profiles(log,p);p.t1="2026-10-19T11:00:00Z";p.t2=p.t1;compare_discover_resource_profiles(log,p);p={};p.max_resources=1;compare_discover_resource_profiles(log,p);p={};p.max_rows=0;compare_discover_resource_profiles(log,p);
 NetworkAnalysisOptions n;n.sorting_attribute="stamp-x";n.timestamp_attribute="stamp-x";n.source_node_attribute="resource-x";n.target_node_attribute="resource-x";n.edge_attribute="activity-x";n.out_relation_attribute="case:custom";n.in_relation_attribute="case:custom";n.include_performance=true;n.business_hours=true;n.business_hour_slots={{0,100},{50,200},{200,604800}};n.max_rows=1;n.max_values=1;
 for(auto v:{NetworkAnalysisEdgeReference::kOut,NetworkAnalysisEdgeReference::kIn}){n.edge_reference=v;compare_discover_network_analysis(log,n);compare_mine_network_analysis(log,n);}
 n.edge_reference=static_cast<NetworkAnalysisEdgeReference>(-1);compare_discover_network_analysis(log,n);n={};n.variant=static_cast<NetworkAnalysisVariant>(1);compare_discover_network_analysis(log,n);n={};n.max_rows=0;compare_discover_network_analysis(log,n);n={};n.max_values=10000001;compare_discover_network_analysis(log,n);
 n={};n.business_hour_slots={{std::numeric_limits<double>::quiet_NaN(),-1}};compare_discover_network_analysis(log,n);n.business_hours=true;compare_discover_network_analysis(log,n);compare_discover_network_analysis({},n);n.business_hour_slots={};compare_discover_network_analysis(log,n);
 LocalDiagnosticsOptions l;l.group_attribute="org:group";l.activity_attribute="activity-x";l.resource_attribute="resource-x";l.explicit_groups={{"Ignored",{"resource-0","resource-0"," ",""}},{all_bytes(),{"resource-1"}}};l.max_groups=10;l.max_resources=100;l.max_rows=2;
 const OrganizationalWorkbenchGroups g={{"Role 1",{"resource-1","resource-1",""}},{all_bytes(),{"resource-0"}}};
 compare_discover_local_diagnostics(log,l);compare_apply_from_group_attribute(log,l);compare_apply_from_clustering_or_roles(log,l,false,false,g);compare_apply_from_clustering_or_roles(log,l,false,false,{});
 l.explicit_groups={{" ",{"resource-0"}}};compare_discover_local_diagnostics(log,l);compare_apply_from_group_attribute(log,l);compare_apply_from_clustering_or_roles(log,l,false,false,g);compare_apply_from_clustering_or_roles(log,l,false,false,{{" ",{"resource-1"}}});
 l={};l.max_groups=0;compare_discover_local_diagnostics(log,l);l={};l.max_resources=1;compare_discover_local_diagnostics(log,l);l={};l.max_rows=1;compare_discover_local_diagnostics(log,l);
}
void test_full_stored_results(){
 const auto octets=all_bytes();const auto wide=std::numeric_limits<std::size_t>::max();const auto u=std::numeric_limits<std::uint64_t>::max();
 OrganizationalSnaResult s;s.variant=OrganizationalSnaVariant::kJointActivities;s.directed=false;s.resources={"",octets};s.edges={{0,1,-3.5},{0,1,-0.0}};s.diagnostics={false,true,wide,wide,2,2,octets};
 auto encoded=serialize_organizational_workbench_result_json(s);assert(serialize_organizational_workbench_result_json(parse_organizational_sna_result_json(encoded))==encoded);compare_result(3,s,organizational_sna_json);
 s.variant=static_cast<OrganizationalSnaVariant>(-4);compare_result(3,s,organizational_sna_json);s.variant=OrganizationalSnaVariant::kJointActivities;s.edges[0].weight=std::numeric_limits<double>::quiet_NaN();encoded=serialize_organizational_workbench_result_json(s);assert(serialize_organizational_workbench_result_json(parse_organizational_sna_result_json(encoded))==encoded);compare_result(3,s,organizational_sna_json);
 RoleDiscoveryResult r;r.variant=RoleDiscoveryVariant::kPandas;r.roles={{{octets},{{octets,wide},{octets,wide}}}};r.diagnostics={false,true,wide,wide,1,1,1,octets};encoded=serialize_organizational_workbench_result_json(r);assert(serialize_organizational_workbench_result_json(parse_organizational_roles_result_json(encoded))==encoded);compare_result(6,r,role_discovery_json);r.roles[0].activities.push_back(octets);compare_result(6,r,role_discovery_json);r.roles[0].activities.pop_back();r.roles[0].resources[0].count=0;compare_result(6,r,role_discovery_json);r.variant=static_cast<RoleDiscoveryVariant>(-2);compare_result(6,r,role_discovery_json);
 ResourceProfilesResult p;p.metric=static_cast<ResourceProfileMetric>(-99);p.rows={{octets,octets,"other",-3.5,u}};p.diagnostics={false,true,wide,wide,2,octets};encoded=serialize_organizational_workbench_result_json(p);assert(serialize_organizational_workbench_result_json(parse_organizational_profiles_result_json(encoded))==encoded);compare_result(9,p,resource_profiles_json);p.rows.push_back(p.rows.front());compare_result(9,p,resource_profiles_json);p.rows.resize(1);p.rows[0].value=std::numeric_limits<double>::infinity();compare_result(9,p,resource_profiles_json);p.metric=ResourceProfileMetric::kSocialPosition;p.rows={{octets,"","",-0.0,0}};p.diagnostics.resource_count=0;compare_result(9,p,resource_profiles_json);
 NetworkAnalysisResult n;n.edges={{octets,"target",octets,u,{-3.5,-0.0}},{octets,"target",octets,2,{}}};n.diagnostics={false,false,wide,wide,1,2,2,octets};encoded=serialize_organizational_workbench_result_json(n);assert(serialize_organizational_workbench_result_json(parse_organizational_network_result_json(encoded))==encoded);compare_result(14,n,network_analysis_json);n.diagnostics.link_count=0;compare_result(14,n,network_analysis_json);n.diagnostics.link_count=1;n.edges[0].performance.push_back(std::numeric_limits<double>::quiet_NaN());compare_result(14,n,network_analysis_json);
 LocalDiagnosticsResult l;l.group_relative_focus[""][octets]=0.0;l.group_relative_stake[""][octets]=1.0;l.group_coverage[octets][""]=-0.0;l.group_member_contribution[octets][""][octets]=u;l.group_member_contribution[octets][""]["zero"]=0;l.diagnostics={false,true,wide,wide,2,1,5,wide,octets};encoded=serialize_organizational_workbench_result_json(l);assert(serialize_organizational_workbench_result_json(parse_organizational_local_result_json(encoded))==encoded);compare_result(18,l,local_diagnostics_json);l.group_relative_focus[""][octets]=std::numeric_limits<double>::quiet_NaN();encoded=serialize_organizational_workbench_result_json(l);assert(serialize_organizational_workbench_result_json(parse_organizational_local_result_json(encoded))==encoded);compare_result(18,l,local_diagnostics_json);l.group_relative_focus[""][octets]=0.0;l.diagnostics.row_count=0;compare_result(18,l,local_diagnostics_json);
}
void test_host_transport_and_artifact_reopen(){
 const auto log=full_log();const auto source=raw(log);
 const auto r=fields(execute_organizational_workbench(request(17,source,std::string("{}"),false,std::string("[]"))).json);const auto a=fields(r.at("artifact_bytes"));
 auto reopen=request(17,bytes(a.at("log_after_json")),bytes(a.at("configured_options_json")),false,bytes(a.at("groups_after_json")));assert(execute_organizational_workbench(reopen).status==200);
 const auto roles=fields(execute_organizational_workbench(request(4,source)).json);const auto role_artifacts=fields(roles.at("artifact_bytes"));const auto serialized=execute_organizational_workbench(request(6,bytes(role_artifacts.at("full_result_json"))));assert(serialized.status==200);
 const auto sr=fields(serialized.json),sa=fields(sr.at("artifact_bytes"));assert(execute_organizational_workbench(request(6,bytes(sa.at("result_input_json")))).json==execute_organizational_workbench(request(6,bytes(sa.at("result_after_json")))).json);
 const auto named=fields(execute_organizational_workbench(request(10,"0")).json),na=fields(named.at("artifact_bytes"));assert(execute_organizational_workbench(request(11,bytes(na.at("full_result_json")))).status==200);assert(execute_organizational_workbench(request(10,bytes(na.at("metric_input_json")))).status==200);
 const auto enum_result=fields(execute_organizational_workbench(request(11,q("activity_frequency"))).json),ea=fields(enum_result.at("artifact_bytes"));assert(execute_organizational_workbench(request(10,bytes(ea.at("full_result_json")))).status==200);assert(execute_organizational_workbench(request(11,bytes(ea.at("name_input_json")))).status==200);
 host_error(execute_organizational_workbench(request(6,bytes(sa.at("native_output_bytes")))),"input_error");
 host_error(execute_organizational_workbench({{"operation","","apply_from_clustering_or_roles",false},{"log_file","",source,false}}),"configuration_error");
 host_error(execute_organizational_workbench(request(17,source,{},false,std::string("[{\"key\":\"A\",\"value\":[]},{\"key\":{\"native_bytes\":\"hex-v1\",\"data\":\"41\"},\"value\":[]}]"))),"configuration_error");
 host_error(execute_organizational_workbench(request(17,source,{},false,std::string("{}"))),"configuration_error");
 host_error(execute_organizational_workbench(request(0,source,std::string("{\"n\":18446744073709551616}"))),"configuration_error");
 host_error(execute_organizational_workbench(request(0,source,std::string("{\"n\":1.0}"))),"configuration_error");
 host_error(execute_organizational_workbench(request(0,source,std::string("{\"beta\":0,\"beta\":1}"))),"configuration_error");
 host_error(execute_organizational_workbench(request(3,"{\"unexpected\":0}")),"input_error");host_error(execute_organizational_workbench(request(0,"[]")),"input_error");
 auto duplicate=request(0,source);duplicate.push_back({"operation","","discover_sna",false});host_error(execute_organizational_workbench(duplicate),"configuration_error");
 auto wrong=request(10,"0");wrong.push_back({"log_file","",source,false});host_error(execute_organizational_workbench(wrong),"configuration_error");
 host_error(execute_organizational_workbench(request(0,source),{{"operation","","discover_organizational_sna",false}}),"configuration_error");
 host_error(execute_organizational_workbench({{"operation","","discover_organizational_sna",false}},{{"log_file","",source,false}}),"configuration_error");
 auto nofilename=request(0,source);nofilename[1].file=false;nofilename[1].filename.clear();assert(execute_organizational_workbench(nofilename).status==200);nofilename[1].file=true;assert(execute_organizational_workbench(nofilename).status==200);
 const std::string boundary="native78";const std::string body="--"+boundary+"\r\nContent-Disposition: form-data; name=\"operation\"\r\n\r\ndiscover_roles\r\n--"+boundary+"\r\nContent-Disposition: form-data; name=\"log_file\"; filename=\"\"\r\nContent-Type: application/x-misleading\r\n\r\n"+source+"\r\n--"+boundary+"--\r\n";
 const auto response=execute_organizational_workbench_multipart("multipart/form-data; boundary="+boundary,body);assert(response.status==200);const auto originals=elements(fields(response.json).at("originals"));assert(fields(originals[1]).at("file")=="true"&&fields(originals[1]).at("filename")=="\"\"");host_error(execute_organizational_workbench_multipart("multipart/form-data; boundary="+boundary,body,{},false),"transport_error");host_error(execute_organizational_workbench_multipart("text/plain",body),"transport_error");
}
} // namespace
int main(){
 roundtrip_OrganizationalSnaOptions();
 roundtrip_RoleDiscoveryOptions();
 roundtrip_ResourceProfilesOptions();
 roundtrip_NetworkAnalysisOptions();
 roundtrip_LocalDiagnosticsOptions();
 roundtrip_OrganizationalSnaResult();
 roundtrip_RoleDiscoveryResult();
 roundtrip_ResourceProfilesResult();
 roundtrip_NetworkAnalysisResult();
 roundtrip_LocalDiagnosticsResult();
 test_all_forms();test_options_and_native_domains();test_full_stored_results();test_host_transport_and_artifact_reopen();
}
