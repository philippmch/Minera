#include "minera/clustering_workbench_json.hpp"
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
using Parts=std::vector<DiscoveryWorkbenchPart>;
Parts request(const std::string& operation,const std::string& input,const std::optional<std::string>& options=std::nullopt,const std::string& form="",const std::optional<std::string>& selector=std::nullopt){
 const bool serializer=operation=="trace_attribute_clustering_json"||operation=="profile_clustering_json";
 Parts parts{{"operation","",operation,false},{serializer?"result_file":"log_file","input.json",input,true}};
 if(options)parts.push_back({"options_json","",*options,false});if(!form.empty())parts.push_back({"call_form","",form,false});if(selector)parts.push_back({"trace_attribute_json","",*selector,false});return parts;
}
const std::array<std::string,8> artifact_names={"log_input_json","log_after_json","result_input_json","result_after_json","configured_options_json","trace_attribute_json","full_result_json","native_output_bytes"};
std::map<std::string,std::string> check(const ClusteringWorkbenchResponse& response,const Direct& direct,const std::string& kind){
 const auto root=fields(response.json);assert(root.at("family")=="\"ORIGINAL_CLUSTERING_WORKBENCH\""&&root.at("version")=="\"original-clustering-workbench-v1\"");
 assert(root.at("native_entered")=="true");assert(root.at("native_returned")==(direct.returned?"true":"false"));assert(root.at("executed")==root.at("native_returned"));
 const auto calls=elements(root.at("native_calls"));std::size_t primaries=0;
 for(const auto& row:calls){const auto c=fields(row);assert(c.size()==7&&c.at("entered")=="true");if(c.at("stage")=="\"primary\""){++primaries;assert(c.at("input_kind")==q(kind)&&c.at("returned")==(direct.returned?"true":"false"));assert(c.at("error")==(direct.returned?"null":q(direct.message)));}}
 assert(primaries==1);
 if(direct.returned){assert(response.status==200&&root.at("error")=="null");assert(fields(root.at("result")).at("value")==direct.value);}
 else{assert(response.status==400&&root.at("result")=="null"&&root.at("native_output")=="null");const auto error=fields(root.at("error"));assert(error.at("phase")=="\"native_error\""&&error.at("type")==q(direct.type)&&error.at("message")==q(direct.message));}
 const auto input=fields(root.at("input"));assert(input.at("before")==input.at("after"));const auto context=fields(root.at("input_context"));assert(context.at("size_t_bytes")==std::to_string(sizeof(std::size_t))&&context.at("int_bytes")==std::to_string(sizeof(int)));
 const auto artifacts=fields(root.at("artifact_bytes"));assert(artifacts.size()==8&&root.at("artifact_errors")=="[]");
 for(const auto& name:artifact_names){assert(artifacts.contains(name));if(artifacts.at(name)!="null"){const auto c=fields(artifacts.at(name));assert(text(c.at("dataset"))==name+(name=="native_output_bytes"?".bin":".json"));(void)bytes(artifacts.at(name));}}
 const auto before=fields(input.at("before")),after=fields(input.at("after"));
 if(context.at("log_prepared")=="true"){assert(context.at("log_unchanged")=="true");assert(bytes(artifacts.at("log_input_json"))==before.at("log")&&bytes(artifacts.at("log_after_json"))==after.at("log"));}
 if(context.at("result_prepared")=="true"){assert(context.at("result_unchanged")=="true");assert(bytes(artifacts.at("result_input_json"))==before.at("result")&&bytes(artifacts.at("result_after_json"))==after.at("result"));}
 if(context.at("options_prepared")=="true")assert(bytes(artifacts.at("configured_options_json"))==root.at("configured_options"));
 if(context.at("trace_attribute_prepared")=="true"){assert(context.at("trace_attribute_unchanged")=="true");assert(bytes(artifacts.at("trace_attribute_json"))==root.at("configured_trace_attribute"));}
 if(direct.returned)assert(bytes(artifacts.at("full_result_json"))==direct.value);else assert(artifacts.at("full_result_json")=="null"&&artifacts.at("native_output_bytes")=="null");
 return root;
}
void host_error(const ClusteringWorkbenchResponse& response,const std::string& phase){const auto root=fields(response.json);assert(response.status==400&&root.at("native_entered")=="false"&&root.at("native_returned")=="false"&&root.at("executed")=="false");assert(fields(root.at("error")).at("phase")==q(phase));for(const auto& row:elements(root.at("native_calls")))assert(fields(row).at("stage")!="\"primary\"");assert(fields(root.at("artifact_bytes")).size()==8);}
EventLog log_sample(){
 EventLog log;log.attributes={{"log-only","retained"}};log.metadata.extensions={{"ext","prefix","urn:source"}};log.metadata.classifiers={{"classifier",{"concept:name","extra"}}};
 log.metadata.trace_globals={{"global-trace","retained"}};log.metadata.event_globals={{"global-event","retained"}};log.metadata.properties={{"property","retained"}};log.metadata.globals={{"event",{{"ordered","one"}}},{"event",{{"ordered","two"}}}};
 log.metadata.compatibility_snapshot_valid=true;log.metadata.compatibility_globals_snapshot={{"old",{{"snapshot","global"}}}};log.metadata.compatibility_trace_globals_snapshot={{"snapshot","trace"}};log.metadata.compatibility_event_globals_snapshot={{"snapshot","event"}};
 for(std::size_t i=0;i<6;++i){Trace t;t.case_id=i<2?"repeated":"case-"+std::to_string(i);t.attributes={{"group",i%2?"left":"right"},{"number",std::to_string(i),AttributeKind::kInt}};t.properties={{"trace-property","kept"}};
  const auto sequence=i<3?std::vector<std::string>{"A","B"}:std::vector<std::string>{"B","A","C"};for(std::size_t e=0;e<sequence.size();++e){Event v;v.case_id="event-id-not-trace-id";v.activity=sequence[e];v.timestamp=std::to_string(e);v.resource="resource-"+std::to_string(i%2);v.lifecycle=e%2?"complete":"start";v.input_order=e;v.attributes={{"alternate",sequence[e]},{"amount",std::to_string(i+e),AttributeKind::kFloat},{"choice",i%2?"L":"R"}};t.events.push_back(std::move(v));}log.traces.push_back(std::move(t));}
 return log;
}
std::map<std::string,std::string> compare_hierarchy(const EventLog& log,const TraceAttributeClusteringOptions& options,const std::string& op="discover_trace_attribute_clustering",bool defaults=false,bool omit=false){
 const auto direct=native([&]{const auto value=op=="cluster_traces_by_attribute"?(defaults?cluster_traces_by_attribute(log):cluster_traces_by_attribute(log,options)):(defaults?discover_trace_attribute_clustering(log):discover_trace_attribute_clustering(log,options));return serialize_clustering_workbench_result_json(value);});
 const auto root=check(execute_clustering_workbench(request(op,raw(log),defaults||omit?std::nullopt:std::make_optional(serialize_clustering_workbench_options_json(options)),defaults?"default_options":"with_options")),direct,"EventLog");
 const auto context=fields(root.at("input_context"));assert(context.at("options_prepared")=="true"&&context.at("options_unchanged")== (defaults?"null":"true"));
 std::size_t count=0;for(const auto& row:elements(root.at("native_calls"))){const auto c=fields(row);if(c.at("stage")=="\"primary\""){++count;assert(c.at("name")==q(op+"(const EventLog&,const TraceAttributeClusteringOptions&)"));assert(c.at("default_arguments_used")==(defaults?"true":"false"));}}
 assert(count==1);return root;
}
std::map<std::string,std::string> compare_profile(const EventLog& log,const ProfileClusteringOptions& options,bool defaults=false,bool omit=false){
 const auto direct=native([&]{return serialize_clustering_workbench_result_json(defaults?discover_profile_clustering(log):discover_profile_clustering(log,options));});
 const auto root=check(execute_clustering_workbench(request("discover_profile_clustering",raw(log),defaults||omit?std::nullopt:std::make_optional(serialize_clustering_workbench_options_json(options)),defaults?"default_options":"with_options")),direct,"EventLog");
 assert(fields(root.at("input_context")).at("options_unchanged")==(defaults?"null":"true"));return root;
}
std::map<std::string,std::string> compare_groups(const EventLog& log,const std::string& selector){const auto direct=native([&]{return serialize_clustering_workbench_groups_json(trace_attribute_clustering_groups(log,selector));});return check(execute_clustering_workbench(request("trace_attribute_clustering_groups",raw(log),std::nullopt,"direct",q(selector))),direct,"EventLog+NativeString");}
std::map<std::string,std::string> compare_serializer(const TraceAttributeClusteringResult& stored){
 const auto direct=native([&]{return q(trace_attribute_clustering_json(stored));});const auto root=check(execute_clustering_workbench(request("trace_attribute_clustering_json",serialize_clustering_workbench_result_json(stored))),direct,"TraceAttributeClusteringResult");
 if(direct.returned){const auto actual=trace_attribute_clustering_json(stored);assert(bytes(root.at("native_output"))==actual&&bytes(fields(root.at("artifact_bytes")).at("native_output_bytes"))==actual);}
 return root;
}
std::map<std::string,std::string> compare_serializer(const ProfileClusteringResult& stored){
 const auto direct=native([&]{return q(profile_clustering_json(stored));});const auto root=check(execute_clustering_workbench(request("profile_clustering_json",serialize_clustering_workbench_result_json(stored))),direct,"ProfileClusteringResult");
 if(direct.returned){const auto actual=profile_clustering_json(stored);assert(bytes(root.at("native_output"))==actual&&bytes(fields(root.at("artifact_bytes")).at("native_output_bytes"))==actual);}
 return root;
}
void call_forms(){
 const auto log=log_sample();TraceAttributeClusteringOptions hierarchy;hierarchy.trace_attribute="group";ProfileClusteringOptions profile;
 compare_groups(log,"group");
 for(const auto& op:{"discover_trace_attribute_clustering","cluster_traces_by_attribute"}){compare_hierarchy(log,hierarchy,op);compare_hierarchy(log,{},op,true);compare_hierarchy(log,{},op,false,true);}
 compare_profile(log,profile);compare_profile(log,{},true);compare_profile(log,{},false,true);
 const auto h=discover_trace_attribute_clustering(log,hierarchy);const auto p=discover_profile_clustering(log,profile);compare_serializer(h);compare_serializer(p);
 // Canonical full outputs retain centroids absent from actual native text.
 const auto full=fields(serialize_clustering_workbench_result_json(p));const auto centroids=elements(full.at("centroids"));assert(full.size()==7&&centroids.size()==p.centroids.size());
 for(std::size_t row=0;row<centroids.size();++row)assert(centroids[row]==serialize_embedding_workbench_vector_json(p.centroids[row]));
 assert(profile_clustering_json(p).find("\"centroids\"")==std::string::npos);
}
void all_options_storage(){
 std::set<std::string> supplied;
 const auto hierarchy=parse_clustering_hierarchy_options_json(R"({"trace_attribute":{"native_bytes":"hex-v1","data":"ff006b"},"activity_attribute":"","variant":-2147483648,"alpha":"NaN","percent":"-INF","max_groups":18446744073709551615,"max_output_nodes":0})",&supplied);
 const auto h=fields(serialize_clustering_workbench_options_json(hierarchy));assert(h.size()==supplied.size()&&h.size()==7);assert(hierarchy.trace_attribute==std::string("\xff\0k",3)&&hierarchy.activity_attribute.empty()&&std::isnan(hierarchy.alpha)&&std::isinf(hierarchy.percent));assert(h.at("max_groups")=="18446744073709551615"&&h.at("variant")=="{\"name\":null,\"index\":-2147483648}");
 assert(serialize_clustering_workbench_options_json(parse_clustering_hierarchy_options_json(serialize_clustering_workbench_options_json(hierarchy)))==serialize_clustering_workbench_options_json(hierarchy));
 const auto profile=parse_clustering_profile_options_json(R"({"variant":2147483647,"activity_attribute":"","cluster_count":0,"max_iterations":18446744073709551615,"max_traces":18446744073709551615,"max_features":18446744073709551615,"seed":18446744073709551615,"tolerance":-0.0,"include_activity_representation":false,"include_succession_representation":false,"trace_string_attributes":["","dup","dup"],"event_string_attributes":[{"native_bytes":"hex-v1","data":"ff"}],"trace_numeric_attributes":["num"],"event_numeric_attributes":["last"],"feature_names":["", ""]})",&supplied);
 const auto canonical=serialize_clustering_workbench_options_json(profile);const auto p=fields(canonical);assert(p.size()==supplied.size()&&p.size()==15&&profile.seed==std::numeric_limits<std::uint64_t>::max()&&std::signbit(profile.tolerance));assert(profile.trace_string_attributes==std::vector<std::string>({"","dup","dup"})&&profile.event_string_attributes==std::vector<std::string>({std::string("\xff",1)}));assert(p.at("tolerance")=="-0.0"&&p.at("seed")=="18446744073709551615");assert(serialize_clustering_workbench_options_json(parse_clustering_profile_options_json(canonical))==canonical);
 for(const auto& name:{"DMM_LEVEN","AVG_LEVEN","DMM_VEC","AVG_VEC","DFG","kDmmLeven","kAvgLeven","kDmmVec","kAvgVec","kDfg"}){const auto value=parse_clustering_hierarchy_options_json("{\"variant\":"+q(name)+'}');assert(static_cast<int>(value.variant)>=0&&static_cast<int>(value.variant)<5);}
 assert(parse_clustering_profile_options_json("{\"variant\":\"kSklearnProfiles\"}").variant==ProfileClusteringVariant::kSklearnProfiles);
 assert(parse_clustering_profile_options_json("{\"variant\":{\"name\":null,\"index\":0}}").variant==ProfileClusteringVariant::kSklearnProfiles);
 rejects([]{(void)parse_clustering_profile_options_json("{\"seed\":18446744073709551616}");});rejects([]{(void)parse_clustering_profile_options_json("{\"seed\":9007199254740993.0}");});rejects([]{(void)parse_clustering_hierarchy_options_json("{\"variant\":2147483648}");});rejects([]{(void)parse_clustering_hierarchy_options_json("{\"alpha\":1e999}");});rejects([]{(void)parse_clustering_hierarchy_options_json("{\"alpha\":0,\"alpha\":1}");});rejects([]{(void)parse_clustering_profile_options_json("{\"unknown\":1}");});rejects([]{(void)parse_clustering_profile_options_json("{\"include_activity_representation\":1}");});
}
void groups_and_hierarchy(){
 const auto log=log_sample();
 for(const auto& key:{"group","case:group","concept:name","case:concept:name","id","case:id","case_id","case:case:id","missing",""})compare_groups(log,key);
 const auto grouped=trace_attribute_clustering_groups(log,"group");assert(grouped.size()==2&&grouped[0].name=="right"&&grouped[0].trace_indices==std::vector<std::size_t>({0,2,4}));
 auto duplicate=log;duplicate.traces[0].attributes={{"group","ignored",AttributeKind::kString,true},{"group","later"},{"case:group","fallback"}};duplicate.traces[1].attributes={{"group",""}};compare_groups(duplicate,"group");
 auto bytes_log=log;const auto key=all_bytes();bytes_log.traces[0].attributes={{key,key}};compare_groups(bytes_log,key);
 for(int variant=0;variant<5;++variant){TraceAttributeClusteringOptions options;options.trace_attribute="group";options.variant=static_cast<TraceAttributeClusteringVariant>(variant);for(const auto alpha:{0.0,0.5,1.0})for(const auto percent:{0.01,0.5,1.0}){options.alpha=alpha;options.percent=percent;compare_hierarchy(log,options);}}
 TraceAttributeClusteringOptions options;options.trace_attribute="group";options.variant=static_cast<TraceAttributeClusteringVariant>(-7);compare_hierarchy(log,options);options.variant=static_cast<TraceAttributeClusteringVariant>(2147483647);compare_hierarchy(log,options);
 for(const auto& selector:{"","concept:name","activity","timestamp","time:timestamp","resource","org:resource","lifecycle","lifecycle:transition","alternate","absent"}){options={};options.trace_attribute="group";options.activity_attribute=selector;compare_hierarchy(log,options);}
 options={};options.trace_attribute="group";for(double alpha:{-1.0,2.0,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()}){options.alpha=alpha;compare_hierarchy(log,options);}
 options={};options.trace_attribute="group";for(double percent:{0.0,-0.0,2.0,std::numeric_limits<double>::quiet_NaN(),-std::numeric_limits<double>::infinity()}){options.percent=percent;compare_hierarchy(log,options);}
 for(const auto groups:{std::size_t{0},std::size_t{1},std::size_t{100001}}){options={};options.trace_attribute="group";options.max_groups=groups;compare_hierarchy(log,options);}
 for(const auto nodes:{std::size_t{0},std::size_t{1},std::size_t{2},std::size_t{3},std::size_t{200001}}){options={};options.trace_attribute="group";options.max_output_nodes=nodes;compare_hierarchy(log,options);}
 options={};options.trace_attribute="missing";compare_hierarchy(log,options);compare_hierarchy({},options);compare_groups({},"group");auto one=log;one.traces.resize(1);options.trace_attribute="group";options.max_output_nodes=1;compare_hierarchy(one,options);
 // Group-only callable has no discovery option bounds, including hard groups.
 EventLog many;many.traces.resize(100001);for(std::size_t i=0;i<many.traces.size();++i)many.traces[i].case_id=std::to_string(i);const auto memberships=trace_attribute_clustering_groups(many,"concept:name");assert(memberships.size()==100001&&memberships.back().trace_indices==std::vector<std::size_t>({100000}));
}
void profiles(){
 const auto log=log_sample();ProfileClusteringOptions options;
 for(const auto seed:{std::uint64_t{0},std::uint64_t{1},std::uint64_t{9007199254740993ULL},std::numeric_limits<std::uint64_t>::max()})for(const auto k:{std::size_t{1},std::size_t{2},std::size_t{6}}){options={};options.seed=seed;options.cluster_count=k;compare_profile(log,options);}
 options={};options.max_iterations=1;compare_profile(log,options);
 for(const auto tolerance:{0.0,-0.0,1e-6,1e100,-1.0,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()}){options={};options.tolerance=tolerance;compare_profile(log,options);}
 for(int control=0;control<4;++control){options={};if(control==0)options.cluster_count=0;if(control==1)options.max_iterations=0;if(control==2)options.max_traces=0;if(control==3)options.max_features=0;compare_profile(log,options);compare_profile({},options);}
 options={};options.variant=static_cast<ProfileClusteringVariant>(-1);options.cluster_count=0;compare_profile(log,options);
 options={};options.cluster_count=7;compare_profile(log,options);options={};options.max_traces=5;compare_profile(log,options);options={};options.max_features=1;compare_profile(log,options);
 for(const auto& selector:{"","concept:name","activity","timestamp","resource","lifecycle","alternate","absent"}){options={};options.activity_attribute=selector;compare_profile(log,options);}
 options={};options.trace_string_attributes={"group"};options.event_string_attributes={"choice"};options.trace_numeric_attributes={"number"};options.event_numeric_attributes={"amount"};compare_profile(log,options);
 options.feature_names={"trace:group@right","event:choice@L","trace:number","event:amount","not-generated"};compare_profile(log,options);
 // Lists retain native validation/order even when feature_names overrides them.
 for(int control=0;control<5;++control){options={};if(control==0)options.trace_string_attributes={"dup","dup"};if(control==1)options.event_string_attributes={""};if(control==2)options.trace_numeric_attributes={"missing"};if(control==3)options.event_numeric_attributes={"missing"};if(control==4)options.feature_names={"same","same"};compare_profile(log,options);compare_profile({},options);}
 options={};options.include_activity_representation=false;options.include_succession_representation=false;options.trace_string_attributes={"group"};compare_profile(log,options);options.feature_names={"isolated"};compare_profile(log,options);
 auto no_features=EventLog{{Trace{"one",{}},Trace{"two",{}}}};options={};options.include_activity_representation=false;options.include_succession_representation=false;compare_profile(no_features,options);options.feature_names={"explicit-zero"};compare_profile(no_features,options);
 for(const auto& lexical:{"true","TRUE","True","false","FALSE","False"," 1","+2.5","NaN","1x",""}){auto changed=log;for(auto& t:changed.traces)t.attributes={{"numeric",lexical,AttributeKind::kString}};options={};options.trace_string_attributes={"missing-string"};options.trace_numeric_attributes={"numeric"};compare_profile(changed,options);}
 auto mismatch=log;for(auto& t:mismatch.traces){t.attributes[1].typed_value=std::numeric_limits<std::int64_t>::max();for(auto& e:t.events)e.attributes[1].typed_value=std::numeric_limits<double>::quiet_NaN();}compare_profile(mismatch,{});
 // Native arithmetic can return a stored result that its own text helper
 // subsequently rejects. Discovery must not silently invoke that helper.
 auto overflow=log;for(auto& t:overflow.traces)t.attributes={{"huge","1e308",AttributeKind::kFloat}};options={};options.cluster_count=1;options.include_activity_representation=false;options.include_succession_representation=false;options.trace_string_attributes={"missing"};options.trace_numeric_attributes={"huge"};const auto overflow_root=compare_profile(overflow,options);assert(overflow_root.at("native_output")=="null");const auto overflow_value=discover_profile_clustering(overflow,options);assert(!std::isfinite(overflow_value.inertia));compare_serializer(overflow_value);
 auto repeated=log;for(auto& t:repeated.traces){t.attributes.insert(t.attributes.begin(),{"group","null-first",AttributeKind::kString,true});t.events[0].attributes.insert(t.events[0].attributes.begin(),{"amount","null-first",AttributeKind::kFloat,true});}compare_profile(repeated,{});
 // Empty-log early native return keeps otherwise dormant invalid selectors/lists.
 options={};options.cluster_count=std::numeric_limits<std::size_t>::max();options.max_iterations=std::numeric_limits<std::size_t>::max();options.max_features=std::numeric_limits<std::size_t>::max();options.trace_string_attributes={"",""};options.event_numeric_attributes={"missing"};options.feature_names={"",""};compare_profile({},options);
}
void supplied_results(){
 TraceAttributeClusteringOptions options;options.trace_attribute="group";const auto good=discover_trace_attribute_clustering(log_sample(),options);compare_serializer(good);compare_serializer(TraceAttributeClusteringResult{});
 TraceAttributeClusteringResult permissive;permissive.tree={"leaf",{},-0.0,123,true};permissive.leaf_names={"leaf","leaf"};permissive.diagnostics={false,true,999,2,1,all_bytes()};compare_serializer(permissive);
 auto hierarchy=good;hierarchy.tree.children.push_back(hierarchy.tree.children.front());hierarchy.diagnostics.node_count++;compare_serializer(hierarchy);
 hierarchy=good;hierarchy.tree.size=std::numeric_limits<std::size_t>::max();hierarchy.diagnostics.trace_count=std::numeric_limits<std::size_t>::max();hierarchy.diagnostics.computed=false;hierarchy.diagnostics.output_truncated=true;hierarchy.diagnostics.message=all_bytes();compare_serializer(hierarchy);
 for(int malformed=0;malformed<7;++malformed){hierarchy=good;if(malformed==0)hierarchy.tree.distance=std::numeric_limits<double>::quiet_NaN();if(malformed==1)hierarchy.tree.distance=-1;if(malformed==2)hierarchy.tree.leaf=true;if(malformed==3)hierarchy.tree.children.clear();if(malformed==4)hierarchy.leaf_names.clear();if(malformed==5)hierarchy.diagnostics.group_count++;if(malformed==6)hierarchy.diagnostics.node_count++;compare_serializer(hierarchy);}
 TraceAttributeClusteringResult empty;empty.tree={all_bytes(),{},0.0,0,true};empty.diagnostics.node_count=1;compare_serializer(empty);
 const auto p=discover_profile_clustering(log_sample());compare_serializer(p);compare_serializer(ProfileClusteringResult{});
 auto profile=p;profile.diagnostics.computed=false;profile.diagnostics.converged=false;profile.diagnostics.iterations=std::numeric_limits<std::size_t>::max();profile.diagnostics.message=all_bytes();profile.feature_names.assign(profile.feature_names.size(),all_bytes());compare_serializer(profile);
 for(int malformed=0;malformed<12;++malformed){profile=p;if(malformed==0)profile.variant=static_cast<ProfileClusteringVariant>(-1);if(malformed==1)profile.inertia=std::numeric_limits<double>::quiet_NaN();if(malformed==2)profile.inertia=-1;if(malformed==3)profile.centroids.pop_back();if(malformed==4)profile.diagnostics.trace_count++;if(malformed==5)profile.diagnostics.feature_count++;if(malformed==6)profile.diagnostics.cluster_count++;if(malformed==7)profile.centroids[0].push_back(0);if(malformed==8)profile.centroids[0][0]=std::numeric_limits<double>::infinity();if(malformed==9)profile.clusters[0].push_back(profile.labels.size());if(malformed==10){for(auto& c:profile.clusters)c.clear();}if(malformed==11)profile.labels[0]=-1;compare_serializer(profile);}
 // Full constructor storage accepts arbitrary inactive/invalid fields before
 // the selected native serializer decides whether that object is consumable.
 const std::string stored=R"({"variant":-2147483648,"labels":[-2147483648,2147483647],"clusters":[[18446744073709551615,18446744073709551615],[]],"centroids":[["NaN","INF","-INF",-0.0],[]],"feature_names":["",{"native_bytes":"hex-v1","data":"ff00"}],"inertia":"NaN","diagnostics":{"computed":false,"converged":true,"trace_count":18446744073709551615,"feature_count":18446744073709551615,"cluster_count":18446744073709551615,"iterations":18446744073709551615,"message":{"native_bytes":"hex-v1","data":"ff00"}}})";
 profile=parse_clustering_workbench_profile_result_json(stored);const auto snapshot=serialize_clustering_workbench_result_json(profile);assert(fields(snapshot).size()==7&&fields(fields(snapshot).at("diagnostics")).size()==7&&profile.labels[0]==std::numeric_limits<int>::min()&&std::isnan(profile.centroids[0][0])&&std::signbit(profile.centroids[0][3])&&profile.clusters[0][0]==std::numeric_limits<std::size_t>::max());assert(serialize_clustering_workbench_result_json(parse_clustering_workbench_profile_result_json(snapshot))==snapshot);compare_serializer(profile);
 hierarchy=parse_clustering_workbench_hierarchy_result_json(R"({"tree":{"name":"","children":[{"name":{"native_bytes":"hex-v1","data":"ff00"},"children":[],"distance":"INF","size":18446744073709551615,"leaf":true}],"distance":"NaN","size":18446744073709551615,"leaf":false},"leaf_names":["dup","dup"],"diagnostics":{"computed":false,"output_truncated":true,"trace_count":18446744073709551615,"group_count":2,"node_count":2,"message":"latent"}})");const auto hs=serialize_clustering_workbench_result_json(hierarchy);assert(fields(hs).size()==3&&fields(fields(hs).at("tree")).size()==5&&fields(fields(hs).at("diagnostics")).size()==6);assert(serialize_clustering_workbench_result_json(parse_clustering_workbench_hierarchy_result_json(hs))==hs);compare_serializer(hierarchy);
 // Outputs do not go back through an input depth or node-count validator.
 hierarchy={};hierarchy.tree={"leaf",{},0,1,true};for(int i=0;i<90;++i){TraceAttributeClusteringNode parent{"parent",{},0,1,false};parent.children.push_back(std::move(hierarchy.tree));hierarchy.tree=std::move(parent);}hierarchy.leaf_names={"leaf"};hierarchy.diagnostics.group_count=1;hierarchy.diagnostics.node_count=91;const auto deep=serialize_clustering_workbench_result_json(hierarchy);assert(deep.size()>6000&&trace_attribute_clustering_json(hierarchy).size()>6000);rejects([&]{(void)parse_clustering_workbench_hierarchy_result_json(deep);});
}
void complete_raw_storage(){
 auto log=log_sample();const auto octets=all_bytes();log.attributes.push_back({octets,octets});auto& attribute=log.attributes.back();attribute.kind=static_cast<AttributeKind>(-11);attribute.container=static_cast<AttributeContainerKind>(-7);attribute.scalar_wrapper=true;attribute.scalar_children_list=true;attribute.first_class_role=static_cast<FirstClassAttributeRole>(-9);attribute.first_class_state=static_cast<FirstClassAttributeState>(-3);attribute.first_class_source_key=octets;attribute.children={{"i","9223372036854775807",AttributeKind::kInt},{"u","18446744073709551615",AttributeKind::kInt},{"nan","NaN",AttributeKind::kFloat},{"zero","-0",AttributeKind::kFloat},{"date",octets,AttributeKind::kDate},{"bool","false",AttributeKind::kBoolean}};
 attribute.children[0].typed_value=std::numeric_limits<std::int64_t>::max();attribute.children[1].typed_value=std::numeric_limits<std::uint64_t>::max();attribute.children[2].typed_value=std::numeric_limits<double>::quiet_NaN();attribute.children[3].typed_value=-0.0;attribute.children[4].typed_value=NativeDate{octets,static_cast<NativeDateResolution>(-4),octets};attribute.children[5].typed_value=false;
 log.metadata.compatibility_globals_snapshot[0].scope=octets;log.metadata.compatibility_trace_globals_snapshot.push_back(attribute);log.metadata.compatibility_event_globals_snapshot.push_back(make_native_na_attribute("NA"));
 log.traces[0].case_id_state=static_cast<FirstClassAttributeState>(-4);log.traces[0].case_id_source_key=octets;log.traces[0].case_id_attribute=attribute;
 auto& event=log.traces[0].events[0];event.input_order=std::numeric_limits<std::size_t>::max();event.activity_state=FirstClassAttributeState::kNull;event.activity_source_key=octets;event.timestamp_attribute=attribute;event.activity_attribute=attribute;event.resource_attribute=attribute;event.lifecycle_attribute=attribute;
 const auto source=raw(log);assert(source.starts_with("{\"native_byte_strings\":\"hex-v1\""));assert(raw(parse_embedding_workbench_raw_log_json(source))==source);
 const auto root=compare_groups(log,"group");const auto before=fields(fields(root.at("input")).at("before")).at("log");assert(before==source);const auto value=fields(fields(before).at("value"));assert(value.size()==3&&fields(value.at("metadata")).size()==10);const auto metadata=fields(value.at("metadata"));assert(metadata.at("compatibility_snapshot_valid")=="true"&&!elements(metadata.at("compatibility_trace_globals_snapshot")).empty());
 const auto artifacts=fields(root.at("artifact_bytes"));const auto reopened=check(execute_clustering_workbench(request("trace_attribute_clustering_groups",bytes(artifacts.at("log_after_json")),std::nullopt,"direct",bytes(artifacts.at("trace_attribute_json")))),native([&]{return serialize_clustering_workbench_groups_json(trace_attribute_clustering_groups(log,"group"));}),"EventLog+NativeString");assert(fields(fields(reopened.at("input")).at("before")).at("log")==source);
 // ASCII-only input retains all four compatibility fields on the ordinary
 // public raw-log writer branch too, without needing an unsafe-byte fallback.
 const auto ordinary=compare_groups(log_sample(),"group");const auto plain=fields(fields(fields(ordinary.at("input")).at("before")).at("log"));assert(plain.size()==3&&fields(plain.at("metadata")).size()==10);
}
std::string multipart(const Parts& parts){std::string body;for(const auto& p:parts){body+="--clustering77-boundary\r\nContent-Disposition: form-data; name=\""+p.name+'"';if(p.file)body+="; filename=\""+p.filename+'"';body+="\r\nContent-Type: application/octet-stream\r\n\r\n"+p.bytes+"\r\n";}return body+"--clustering77-boundary--\r\n";}
void transport_and_errors(){
 const auto log=log_sample();auto parts=request("trace_attribute_clustering_groups",raw(log),std::nullopt,"direct",q("group"));const auto direct=native([&]{return serialize_clustering_workbench_groups_json(trace_attribute_clustering_groups(log,"group"));});
 for(const auto file:{false,true})for(const auto& filename:{"","misleading.csv"}){auto altered=parts;altered[1].file=file;altered[1].filename=filename;const auto body=multipart(altered);const auto root=check(execute_clustering_workbench_multipart("multipart/form-data; boundary=clustering77-boundary",body),direct,"EventLog+NativeString");const auto original=fields(elements(root.at("originals"))[1]);assert(original.at("file")==(file?"true":"false")&&original.at("filename")==(file?q(filename):"null")&&bytes(original.at("original"))==raw(log));}
 Parts query{{"operation","","trace_attribute_clustering_groups",false},{"call_form","","direct",false},{"trace_attribute_json","",q("group"),false}};Parts upload{{"log_file","",raw(log),false}};check(execute_clustering_workbench(upload,query),direct,"EventLog+NativeString");check(execute_clustering_workbench_multipart("multipart/form-data; boundary=clustering77-boundary",multipart(upload),query),direct,"EventLog+NativeString");
 auto duplicate=parts;duplicate.push_back(parts.front());host_error(execute_clustering_workbench(duplicate),"configuration_error");host_error(execute_clustering_workbench(parts,{{"operation","","trace_attribute_clustering_groups",false}}),"configuration_error");host_error(execute_clustering_workbench(parts,{{"log_file","",raw(log),false}}),"configuration_error");
 for(const auto& name:{"source_id","cache_id","unknown","result_file","options_json"}){auto bad=parts;bad.push_back({name,"","{}",false});host_error(execute_clustering_workbench(bad),"configuration_error");}
 host_error(execute_clustering_workbench({}),"configuration_error");host_error(execute_clustering_workbench({{"operation","","missing",false}}),"configuration_error");
 auto missing=parts;missing.erase(missing.begin()+1);missing.back().bytes="not-json";host_error(execute_clustering_workbench(missing),"configuration_error");
 auto bad=parts;bad[1].bytes="not-json";host_error(execute_clustering_workbench(bad),"input_error");bad=parts;bad.back().bytes="null";host_error(execute_clustering_workbench(bad),"configuration_error");
 host_error(execute_clustering_workbench(request("discover_trace_attribute_clustering",raw(log),"{\"alpha\":1,\"alpha\":2}")),"configuration_error");
 host_error(execute_clustering_workbench(request("discover_profile_clustering",raw(log),"{}","default_options")),"configuration_error");
 host_error(execute_clustering_workbench_multipart("application/json","{}"),"transport_error");host_error(execute_clustering_workbench_multipart("multipart/form-data; boundary=x","--x\r\n"),"transport_error");host_error(execute_clustering_workbench_multipart("multipart/form-data; boundary=clustering77-boundary",multipart(parts),{},false),"transport_error");
 // Actual canonical discovery values reopen the matching serializer without
 // replacing its own validation or reconstructing native result text.
 TraceAttributeClusteringOptions options;options.trace_attribute="group";const auto hroot=compare_hierarchy(log,options);const auto hartifacts=fields(hroot.at("artifact_bytes"));const auto hvalue=discover_trace_attribute_clustering(log,options);check(execute_clustering_workbench(request("trace_attribute_clustering_json",bytes(hartifacts.at("full_result_json")))),native([&]{return q(trace_attribute_clustering_json(hvalue));}),"TraceAttributeClusteringResult");
 const auto proot=compare_profile(log,{});const auto partifacts=fields(proot.at("artifact_bytes"));const auto pvalue=discover_profile_clustering(log);check(execute_clustering_workbench(request("profile_clustering_json",bytes(partifacts.at("full_result_json")))),native([&]{return q(profile_clustering_json(pvalue));}),"ProfileClusteringResult");
 host_error(execute_clustering_workbench(request("profile_clustering_json",bytes(fields(compare_groups(log,"group").at("artifact_bytes")).at("full_result_json")))),"input_error");
 const auto serializer=compare_serializer(pvalue);host_error(execute_clustering_workbench(request("profile_clustering_json",bytes(fields(serializer.at("artifact_bytes")).at("full_result_json")))),"input_error");
}
} // namespace
int main(){call_forms();all_options_storage();groups_and_hierarchy();profiles();supplied_results();complete_raw_storage();transport_and_errors();}
