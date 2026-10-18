#include "minera/root_cause_workbench_json.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/footprints_workbench_json.hpp"
#include "minera/literal_controls_json.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
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
// Inspect complete public JSON output independently of the production walker.
// Numeric tokens stay strings: these assertions never round size_t/int64.
std::map<std::string, std::string> fields(const std::string& text) {
  assert(text.size() >= 2 && text.front() == '{' && text.back() == '}');
  std::map<std::string, std::string> result; std::size_t at = 1;
  while (at + 1 < text.size()) {
    assert(text[at++] == '"'); const auto begin = at;
    while (text[at] != '"') ++at;
    const auto key = text.substr(begin, at - begin); assert(text[++at] == ':'); ++at;
    const auto value = at; int depth = 0; bool string = false;
    for (; at < text.size(); ++at) {
      const auto c = text[at];
      if (string) { if (c == '\\') ++at; else if (c == '"') string = false; continue; }
      if (c == '"') string = true;
      else if (c == '{' || c == '[') ++depth;
      else if (c == '}' || c == ']') { if (depth == 0) break; --depth; }
      else if (c == ',' && depth == 0) break;
    }
    assert(result.emplace(key, text.substr(value, at - value)).second);
    if (text[at] == ',') ++at;
  }
  return result;
}
void expected(const std::string& text, const std::map<std::string, std::string>& values) { assert(fields(text) == values); }
std::string word(const std::string& bytes) { return serialize_embedding_workbench_string_json(bytes); }
template<class F> void rejects(F function) { bool thrown = false; try { function(); } catch (const std::invalid_argument&) { thrown = true; } assert(thrown); }
std::string bytes(const std::string& artifact) {
  const auto data = fields(artifact); const auto text = parse_literal_string_vector_json("[" + data.at("data") + "]").front();
  std::string result;
  if (data.at("encoding") == "\"utf8\"") result = text;
  else {
    assert(data.at("encoding") == "\"hex\"" && text.size() % 2 == 0);
    for (std::size_t i = 0; i < text.size(); i += 2) result += static_cast<char>(std::stoul(text.substr(i, 2), nullptr, 16));
  }
  assert(data.at("bytes") == std::to_string(result.size()));
  assert(data.at("sha256") == word(footprints_workbench_sha256(result)));
  return result;
}

std::vector<std::string> elements(const std::string& text){
 assert(text.size()>=2&&text.front()=='['&&text.back()==']');std::vector<std::string> result;std::size_t at=1;
 while(at+1<text.size()){const auto begin=at;int depth=0;bool string=false;for(;at<text.size();++at){const char c=text[at];if(string){if(c=='\\')++at;else if(c=='"')string=false;continue;}if(c=='"')string=true;else if(c=='{'||c=='[')++depth;else if(c=='}'||c==']'){if(depth==0)break;--depth;}else if(c==','&&depth==0)break;}result.push_back(text.substr(begin,at-begin));if(text[at]==',')++at;}return result;
}
std::string event_holder(const Event& event){EventLog holder;Trace trace;trace.events.push_back(event);holder.traces.push_back(std::move(trace));return serialize_embedding_workbench_raw_log_json(holder);}
std::string scalar_holder(const NativeAttributeValue& scalar){Event event;EventAttribute attribute;attribute.typed_value=scalar;event.attributes.push_back(std::move(attribute));return event_holder(event);}
std::string typed(const std::string& type,const std::string& value){return "{\"type\":"+word(type)+",\"value\":"+value+'}';}
Event numeric(double number){Event event;EventAttribute cost{"cost",std::to_string(number),AttributeKind::kFloat};cost.typed_value=number;event.attributes.push_back(std::move(cost));return event;}
std::string observation(std::size_t trace,const Event& context){return "{\"trace_index\":"+std::to_string(trace)+",\"context\":"+event_holder(context)+'}';}
std::string cohorts(const std::vector<Event>& fit,const std::vector<Event>& problem){
 const auto list=[](const auto& values){std::string out="[";for(std::size_t i=0;i<values.size();++i){if(out.size()>1)out+=',';out+=observation(i,values[i]);}return out+']';};return typed("RootCauseCohorts","{\"fit\":"+list(fit)+",\"problem\":"+list(problem)+'}');
}
RootCauseWorkbenchResponse execute(const std::string& operation,const std::string& input,const std::optional<std::string>& arguments=std::nullopt){
 std::vector<DiscoveryWorkbenchPart> parts{{"operation","",operation,false},{"input_format","","exact",false}};
 if(arguments)parts.push_back({"arguments_json","",*arguments,false});parts.push_back({"input_file","input.json",input,true});return execute_root_cause_workbench(parts);
}
std::map<std::string,std::string> success(const RootCauseWorkbenchResponse& response,const std::string& result_type){
 assert(response.status==200);const auto root=fields(response.json);assert(root.size()==20);assert(root.at("error")=="null"&&root.at("executed")=="true"&&root.at("native_entered")=="true"&&root.at("native_returned")=="true");
 const auto result=fields(root.at("result"));assert(result.at("type")==word(result_type));const auto input=fields(root.at("input"));assert(input.at("before")==input.at("after"));const auto context=fields(root.at("input_context"));assert(context.at("size_t_bytes")==std::to_string(sizeof(std::size_t))&&context.at("int_bytes")==std::to_string(sizeof(int)));assert(context.at("input_unchanged")=="true");
 const auto artifacts=fields(root.at("artifact_bytes"));assert(artifacts.size()==6);assert(bytes(artifacts.at("source_input_json"))==input.at("before"));assert(bytes(artifacts.at("after_input_json"))==input.at("after"));assert(bytes(artifacts.at("result_json"))==root.at("result"));
 const auto calls=elements(root.at("native_calls"));assert(!calls.empty());const auto primary=fields(calls.front());assert(primary.at("stage")=="\"primary\""&&primary.at("returned")=="true"&&primary.at("error")=="null");return root;
}
void failure(const RootCauseWorkbenchResponse& response,const std::string& phase,bool entered,const std::string& type,const std::string& message={}){
 assert(response.status==400);const auto root=fields(response.json);assert(root.at("native_entered")==std::string(entered?"true":"false")&&root.at("native_returned")=="false"&&root.at("executed")=="false"&&root.at("result")=="null");const auto error=fields(root.at("error"));assert(error.at("phase")==word(phase)&&error.at("exception_type")==word(type));if(!message.empty())assert(error.at("message")==word(message));const auto input=fields(root.at("input"));if(entered)assert(input.at("before")!="null"&&input.at("before")==input.at("after"));else assert(input.at("after")=="null");
}
std::string graph_input(const std::string& type,const std::string& values,const std::string& contexts,const std::string& tail){return typed(type,"{\"graph\":{\"values\":"+values+",\"contexts\":"+contexts+'}'+tail+'}');}
std::string result_value(const std::map<std::string,std::string>& root){return fields(root.at("result")).at("value");}

void options_and_full_storage(){
 std::set<std::string> supplied{"untouched"};const auto defaults=parse_root_cause_workbench_options_json("{}",&supplied);assert(supplied.empty());expected(serialize_root_cause_workbench_options_json(defaults),{{"string_attributes","[]"},{"numeric_attributes","[]"},{"enable_multiplier","false"}});
 std::string all;for(int i=0;i<256;++i)all+=static_cast<char>(i);
 const auto configured=parse_root_cause_workbench_options_json("{\"string_attributes\":["+word(all)+","+word(all)+"],\"numeric_attributes\":[\"cost\",\"cost\"],\"enable_multiplier\":true}",&supplied);
 assert(supplied==std::set<std::string>({"string_attributes","numeric_attributes","enable_multiplier"}));assert(configured.string_attributes==std::vector<std::string>({all,all})&&configured.numeric_attributes==std::vector<std::string>({"cost","cost"})&&configured.enable_multiplier);
 assert(serialize_root_cause_workbench_options_json(parse_root_cause_workbench_options_json(serialize_root_cause_workbench_options_json(configured)))==serialize_root_cause_workbench_options_json(configured));
 for(const auto* field:{"string_attributes","numeric_attributes","enable_multiplier"}){rejects([&]{(void)parse_root_cause_workbench_options_json("{\""+std::string(field)+"\":null}",&supplied);});assert(supplied.size()==3);}
 rejects([]{(void)parse_root_cause_workbench_options_json("{\"enable_multiplier\":true,\"enable_multiplier\":false}");});rejects([]{(void)parse_root_cause_workbench_options_json("{\"unknown\":0}");});
 const double nan=std::numeric_limits<double>::quiet_NaN(),inf=std::numeric_limits<double>::infinity();const auto maximum=std::numeric_limits<std::size_t>::max();
 RootCauseRepresentation representation{{all,"duplicate","duplicate"},{{-0.0,nan,inf,-inf,1e39}}, {-1,std::numeric_limits<int>::max()}, {0,maximum}};
 const auto repr=fields(serialize_root_cause_workbench_result_json(representation));assert(repr.size()==4);assert(repr.at("data")=="["+serialize_embedding_workbench_vector_json(representation.data[0])+"]");assert(repr.at("source_trace_indices")=="[0,"+std::to_string(maximum)+"]");
 RootCauseTreeNode node;node.feature_index=std::numeric_limits<int>::min();node.threshold=-inf;node.left=std::numeric_limits<int>::max();node.right=-1;node.class_counts={maximum,0};node.missing_go_to_left=true;node.missing_only=true;
 RootCauseModel model;model.feature_names=representation.feature_names;model.classes={all,""};model.data=representation.data;model.target=representation.target;model.source_trace_indices=representation.source_trace_indices;model.tree={node};model.fit_count=maximum;model.problem_count=0;
 const auto raw=serialize_root_cause_workbench_result_json(model);const auto stored=fields(raw);assert(stored.size()==8&&stored.at("fit_count")==std::to_string(maximum)&&stored.at("data")==repr.at("data"));const auto tree=fields(elements(stored.at("tree")).front());assert(tree.size()==7&&tree.at("threshold")=="\"-INF\""&&tree.at("class_counts")=="["+std::to_string(maximum)+",0]"&&tree.at("feature_index")==std::to_string(node.feature_index));
 assert(serialize_root_cause_workbench_result_json(std::optional<RootCauseModel>{})=="null");assert(serialize_root_cause_workbench_result_json(std::optional<RootCauseModel>{model})==raw);assert(serialize_root_cause_workbench_result_json(std::optional<RootCauseModel>{RootCauseModel{}})!="null");
 representation.feature_names={std::string(32u*1024u*1024u+1u,'x')};assert(serialize_root_cause_workbench_result_json(representation).size()>32u*1024u*1024u);
}

void ordinary_calls_and_artifacts(){
 const auto fit=std::vector<Event>{numeric(1),numeric(2)},problem=std::vector<Event>{numeric(7),numeric(8),numeric(9),numeric(10),numeric(11)};const auto input=cohorts(fit,problem);
 auto root=success(execute("build_root_cause_log",input,"{\"enable_multiplier\":true}"),"EventLog");const auto log=parse_embedding_workbench_raw_log_json(result_value(root));assert(log.traces.size()==9&&event_holder(log.traces[0].events[0])==event_holder(fit[0])&&event_holder(log.traces[2].events[0])==event_holder(fit[0])&&event_holder(log.traces[4].events[0])==event_holder(problem[0]));
 auto artifacts=fields(root.at("artifact_bytes"));assert(bytes(artifacts.at("result_log_json"))==result_value(root)&&artifacts.at("result_xes")!="null");assert(elements(root.at("native_calls")).size()==2);assert(root.at("supplied_arguments")=="[\"enable_multiplier\"]"&&root.at("supplied_options")=="[]");
 const auto arguments=std::string("{\"options\":{\"numeric_attributes\":[\"cost\",\"cost\"],\"string_attributes\":[],\"enable_multiplier\":true}}");root=success(execute("build_root_cause_representation",input,arguments),"RootCauseRepresentation");const auto repr=fields(result_value(root));assert(elements(repr.at("data")).size()==9&&repr.at("feature_names")=="[\"event:cost\",\"event:cost\"]");assert(root.at("supplied_arguments")=="[\"options\"]"&&root.at("supplied_options")=="[\"enable_multiplier\",\"numeric_attributes\",\"string_attributes\"]");artifacts=fields(root.at("artifact_bytes"));assert(artifacts.at("result_log_json")=="null"&&artifacts.at("result_xes")=="null");
 const auto reopened=success(execute("build_root_cause_representation",bytes(artifacts.at("source_input_json")),bytes(artifacts.at("configured_arguments_json"))),"RootCauseRepresentation");assert(reopened.at("result")==root.at("result"));
 std::string custom="problem";custom+=static_cast<char>(0xff);custom+='\0';root=success(execute("fit_root_cause_model",input,"{\"options\":{\"numeric_attributes\":[\"cost\"]},\"problem_class\":"+word(custom)+'}'),"std::optional<RootCauseModel>");const auto model=fields(result_value(root));assert(model.size()==8&&model.at("classes")=="[\"fit\","+word(custom)+"]"&&model.at("fit_count")=="2"&&model.at("problem_count")=="5");assert(!elements(model.at("tree")).empty());
 // Compare native fit invariants; native private RNG may choose equal splits.
 assert(model.at("source_trace_indices")=="[0,1,0,1,2,3,4]");
 root=success(execute("fit_root_cause_model",cohorts({},problem),"{\"options\":{\"enable_multiplier\":true}}"),"std::optional<RootCauseModel>");assert(result_value(root)=="null"&&root.at("result")!="null");
 const auto empty=cohorts({},{});root=success(execute("build_root_cause_representation",empty),"RootCauseRepresentation");assert(fields(result_value(root)).at("data")=="[]");assert(root.at("supplied_arguments")=="[]"&&root.at("parsed_controls")=="[\"operation\",\"input_format\"]");
 failure(execute("build_root_cause_log",empty,"{\"enable_multiplier\":true}"),"native_error",true,"domain_error","root cause: cannot balance an empty cohort");
 failure(execute("build_root_cause_representation",empty,"{\"options\":{\"enable_multiplier\":true}}"),"native_error",true,"domain_error","root cause: cannot balance an empty cohort");
 failure(execute("fit_root_cause_model",input),"native_error",true,"invalid_argument","root cause: select at least one attribute");
 const auto floating=cohorts({numeric(-0.0),numeric(std::numeric_limits<double>::infinity()),numeric(1e39),numeric(std::numeric_limits<double>::quiet_NaN())},{});root=success(execute("build_root_cause_representation",floating,"{\"options\":{\"numeric_attributes\":[\"cost\"]}}"),"RootCauseRepresentation");const auto data=fields(result_value(root)).at("data");assert(data.find("\"INF\"")!=std::string::npos&&data.find("\"NaN\"")!=std::string::npos&&data.find("-0")!=std::string::npos);
 failure(execute("fit_root_cause_model",cohorts({numeric(1e39)},{numeric(2)}),"{\"options\":{\"numeric_attributes\":[\"cost\"]}}"),"native_error",true,"invalid_argument","root cause: numeric attribute outside float32 range");
 root=success(execute("fit_root_cause_model",cohorts({numeric(std::numeric_limits<double>::quiet_NaN())},{numeric(2)}),"{\"options\":{\"numeric_attributes\":[\"cost\"]}}"),"std::optional<RootCauseModel>");assert(fields(result_value(root)).at("data").find("\"NaN\"")!=std::string::npos);
 Event bad=numeric(1);bad.attributes[0].is_null=true;failure(execute("build_root_cause_representation",cohorts({bad},{}),"{\"options\":{\"numeric_attributes\":[\"cost\"]}}"),"native_error",true,"invalid_argument");
 bad=Event{};bad.attributes.push_back({"broken","invalid",AttributeKind::kDate});root=success(execute("build_root_cause_log",cohorts({bad},{})),"EventLog");artifacts=fields(root.at("artifact_bytes"));assert(artifacts.at("result_json")!="null"&&artifacts.at("result_log_json")!="null"&&artifacts.at("result_xes")=="null");assert(elements(root.at("artifact_errors")).size()==1&&elements(root.at("native_calls")).size()==2);
}

void shared_calls_complete_graph_and_reopening(){
 std::string all;for(int i=0;i<256;++i)all+=static_cast<char>(i);
 Event rich=numeric(-0.0);rich.case_id=all;rich.activity=all;rich.timestamp="bad-date";rich.resource=all;rich.lifecycle=all;rich.input_order=std::numeric_limits<std::size_t>::max();rich.activity_state=static_cast<FirstClassAttributeState>(-5);rich.timestamp_state=FirstClassAttributeState::kNull;rich.resource_state=FirstClassAttributeState::kPresent;rich.case_id_source_key=all;rich.activity_source_key=all;rich.timestamp_source_key=all;rich.resource_source_key=all;rich.lifecycle_source_key=all;
 EventAttribute retained{all,all,static_cast<AttributeKind>(29),true};retained.container=static_cast<AttributeContainerKind>(-8);retained.first_class_role=static_cast<FirstClassAttributeRole>(-11);retained.first_class_state=static_cast<FirstClassAttributeState>(17);retained.first_class_source_key=all;retained.scalar_wrapper=true;retained.scalar_children_list=true;retained.typed_value=NativeDate{all,static_cast<NativeDateResolution>(-7),all};retained.children.push_back({"nested",all});rich.attributes.push_back(retained);rich.activity_attribute=retained;rich.timestamp_attribute=EventAttribute{};rich.resource_attribute=retained;rich.lifecycle_attribute=retained;
 const auto made=success(execute("make_shared_root_cause_context",typed("Event",event_holder(rich))),"RootCauseSharedContext");assert(elements(made.at("native_calls")).size()==1&&fields(made.at("artifact_bytes")).at("result_xes")=="null");
 const auto materialized=success(execute("materialize_root_cause_context",made.at("result")),"Event");const auto recovered=parse_embedding_workbench_raw_log_json(result_value(materialized));assert(recovered.traces.size()==1&&recovered.traces[0].events.size()==1);const auto& restored=recovered.traces[0].events[0];assert(restored.case_id==all&&restored.input_order==rich.input_order&&restored.activity_state==rich.activity_state&&restored.resource==all&&restored.lifecycle_attribute.has_value());assert(restored.attributes[1].typed_value==retained.typed_value&&restored.attributes[1].children==retained.children);
 // Native factory intentionally has no EventAttribute wrapper flags in its
 // 9-field shared value; do not demand flags absent from that native type.
 const auto arena_body=fields(result_value(made));const auto graph=fields(arena_body.at("graph"));const auto nodes=elements(graph.at("values"));assert(!nodes.empty());for(const auto& node:nodes)assert(fields(node).size()==10);const auto contexts=elements(graph.at("contexts"));assert(contexts.size()==1&&fields(contexts[0]).size()==7);
 const auto maximum=std::to_string(std::numeric_limits<std::size_t>::max());
 std::vector<NativeAttributeValue> scalars{NativeAttributeValue{},NativeAttributeValue{std::in_place_type<std::int64_t>,std::numeric_limits<std::int64_t>::min()},NativeAttributeValue{std::in_place_type<double>,-0.0},NativeAttributeValue{std::in_place_type<bool>,true},NativeAttributeValue{std::in_place_type<NativeDate>,NativeDate{all,static_cast<NativeDateResolution>(-9),std::string{}}},NativeAttributeValue{std::in_place_type<std::uint64_t>,std::numeric_limits<std::uint64_t>::max()},NativeAttributeValue{std::in_place_type<NativeNA>,NativeNA{}}};
 std::string values="[";for(std::size_t i=0;i<scalars.size();++i){if(i)values+=',';values+="{\"id\":"+std::to_string(i)+",\"value\":"+word(all)+",\"kind_index\":-19,\"null\":true,\"typed_value\":"+scalar_holder(scalars[i])+",\"container_index\":31,\"children\":[{\"key\":"+word(all)+",\"value\":null}],\"first_class_role_index\":-17,\"first_class_state_index\":29,\"first_class_source_key\":"+word(all)+'}';}values+=",{\"id\":"+maximum+",\"value\":\"unused\",\"children\":[{\"value\":"+maximum+"}]}]";
 const std::string ctx="[{\"id\":9,\"prototype\":"+event_holder(rich)+",\"attributes\":[{\"key\":\"x\",\"value\":0},{\"key\":\"x\",\"value\":0},{\"key\":\"equal\",\"value\":1}],\"activity_attribute\":null,\"timestamp_attribute\":{\"value\":null},\"resource_attribute\":{\"value\":2},\"lifecycle_attribute\":{\"value\":3}}]";
 auto root=success(execute("build_shared_root_cause_log",graph_input("RootCauseSharedCohorts",values,ctx,",\"fit\":[{\"trace_index\":"+maximum+",\"context\":9}],\"problem\":[]")),"RootCauseSharedLog");
 const auto source=fields(fields(root.at("input")).at("before"));const auto source_graph=fields(fields(source.at("value")).at("graph"));const auto stored_nodes=elements(source_graph.at("values"));assert(stored_nodes.size()==8);for(std::size_t i=0;i<scalars.size();++i){const auto node=fields(stored_nodes[i]);assert(node.size()==10&&node.at("kind_index")=="-19"&&node.at("container_index")=="31"&&node.at("first_class_role_index")=="-17"&&node.at("first_class_state_index")=="29"&&node.at("value")==word(all));const auto holder=parse_embedding_workbench_raw_log_json(node.at("typed_value"));assert(holder.traces.front().events.front().attributes.front().typed_value==scalars[i]);if(i==2)assert(std::signbit(std::get<double>(holder.traces.front().events.front().attributes.front().typed_value)));if(i==4){const auto& date=std::get<NativeDate>(holder.traces.front().events.front().attributes.front().typed_value);assert(static_cast<int>(date.resolution)==-9&&date.timezone.has_value()&&date.timezone->empty());}}
 const auto built_body=fields(result_value(root));const auto built_graph=fields(built_body.at("graph"));assert(elements(built_graph.at("values")).size()==8&&elements(built_graph.at("contexts")).size()==2);const auto returned_contexts=elements(built_body.at("contexts"));assert(returned_contexts==std::vector<std::string>{"0"});const auto built_context=fields(elements(built_graph.at("contexts"))[1]);assert(built_context.at("attributes")==fields(elements(source_graph.at("contexts"))[0]).at("attributes"));assert(fields(fields(root.at("input")).at("after")).at("value")==source.at("value"));
 // The deliberately inactive null/cycle state is valid for copy-only building
 // but the materializer must perform its actual native handle validation.
 failure(execute("materialize_root_cause_log",root.at("result")),"native_error",true,"invalid_argument","root cause: null shared value handle");
 const std::string valid_values="[{\"id\":7,\"value\":\"a\"},{\"id\":8,\"value\":\"a\"}]";
 const std::string valid_ctx="[{\"id\":3,\"attributes\":[{\"key\":\"group\",\"value\":7},{\"key\":\"alias\",\"value\":7},{\"key\":\"different\",\"value\":8}]}]";
 root=success(execute("build_shared_root_cause_log",graph_input("RootCauseSharedCohorts",valid_values,valid_ctx,",\"fit\":[{\"context\":3}],\"problem\":[{\"context\":3},{\"context\":3}]"),"{\"enable_multiplier\":true}"),"RootCauseSharedLog");const auto returned=fields(result_value(root));assert(elements(returned.at("contexts")).size()==4);const auto copied_graph=fields(returned.at("graph"));assert(elements(copied_graph.at("values")).size()==2&&elements(copied_graph.at("contexts")).size()==5);for(const auto& copied:elements(copied_graph.at("contexts")))assert(fields(copied).at("attributes")==fields(elements(copied_graph.at("contexts"))[0]).at("attributes"));
 const auto materialized_log=success(execute("materialize_root_cause_log",root.at("result")),"EventLog");const auto log=parse_embedding_workbench_raw_log_json(result_value(materialized_log));assert(log.traces.size()==4&&log.traces.front().events.front().attributes[0].value=="a");
 const auto represented=success(execute("build_shared_root_cause_representation",graph_input("RootCauseSharedCohorts",valid_values,valid_ctx,",\"fit\":[{\"trace_index\":"+maximum+",\"context\":3}],\"problem\":[]"),"{\"options\":{\"string_attributes\":[\"group\"]}}"),"RootCauseRepresentation");assert(fields(result_value(represented)).at("source_trace_indices")=="["+maximum+"]");
 // Reopening a complete graph with an unused cycle performs no hidden native
 // materialization, and result graph ID aliases remain exactly explicit.
 const auto reopened=success(execute("build_shared_root_cause_log",bytes(fields(root.at("artifact_bytes")).at("source_input_json")),bytes(fields(root.at("artifact_bytes")).at("configured_arguments_json"))),"RootCauseSharedLog");assert(reopened.at("result")==root.at("result"));
 const std::string cycle_values="[{\"id\":0,\"children\":[{\"key\":\"self\",\"value\":0}]}]",cycle_context="[{\"id\":0,\"attributes\":[{\"key\":\"group\",\"value\":0}]}]";
 failure(execute("materialize_root_cause_context",graph_input("RootCauseSharedContext",cycle_values,cycle_context,",\"context\":0")),"native_error",true,"invalid_argument","root cause: cyclic shared value cannot be materialized");
 root=success(execute("build_shared_root_cause_representation",graph_input("RootCauseSharedCohorts",cycle_values,cycle_context,",\"fit\":[{\"context\":0}],\"problem\":[]"),"{\"options\":{\"string_attributes\":[\"group\"]}}"),"RootCauseRepresentation");assert(result_value(root).find("{...}")!=std::string::npos);
 for(const auto& [bad_graph,message]:std::vector<std::pair<std::string,std::string>>{{"{\"values\":[{\"id\":0},{\"id\":0}]}","duplicate value ID"},{"{\"contexts\":[{\"id\":0},{\"id\":0}]}","duplicate context ID"},{"{\"values\":[{\"id\":0,\"children\":[{\"value\":99}]}]}","unknown value ID"}}){failure(execute("build_shared_root_cause_log",typed("RootCauseSharedCohorts","{\"graph\":"+bad_graph+'}')),"preparation_error",false,"invalid_argument","root cause workbench JSON: "+message);}
}

void transport_presence_validation_order_and_bytes(){
 const auto input=cohorts({},{});const auto plain=execute_root_cause_workbench({{"input_file","",input,false}});auto root=success(plain,"RootCauseRepresentation");const auto original=fields(elements(root.at("originals")).front());assert(original.at("file")=="false"&&original.at("filename")=="null"&&fields(original.at("original")).at("dataset")=="\"input_file\"");assert(bytes(original.at("original"))==input);
 const auto named=execute_root_cause_workbench({{"input_file","",input,true}});root=success(named,"RootCauseRepresentation");assert(fields(elements(root.at("originals")).front()).at("filename")=="\"\"");
 const auto absent=execute_root_cause_workbench({});failure(absent,"preparation_error",false,"invalid_argument","root cause workbench JSON: input_file is required");assert(fields(absent.json).at("configured_arguments")!="null");
 failure(execute("make_shared_root_cause_context",typed("Event",event_holder(Event{})),"{}"),"configuration_error",false,"invalid_argument","root cause workbench JSON: this public function has no arguments object");
 failure(execute("build_root_cause_log",input,"{\"enable_multiplier\":null}"),"configuration_error",false,"invalid_argument");failure(execute("build_root_cause_representation",input,"{\"options\":null}"),"configuration_error",false,"invalid_argument");failure(execute("fit_root_cause_model",input,"{\"problem_class\":null}"),"configuration_error",false,"invalid_argument");
 failure(execute("build_root_cause_representation",typed("Event",event_holder(Event{}))),"preparation_error",false,"invalid_argument","root cause workbench JSON: input type does not match selected public function");
 failure(execute("make_shared_root_cause_context",typed("Event","{}")),"preparation_error",false,"invalid_argument","root cause workbench JSON: Event holder requires exactly one trace and one event");
 failure(execute_root_cause_workbench({{"input_file","",input,false},{"operation","","build_root_cause_log",false},{"operation","","fit_root_cause_model",false}}),"configuration_error",false,"invalid_argument");
 failure(execute_root_cause_workbench({},{{"input_file","",input,false}}),"configuration_error",false,"invalid_argument","root cause workbench JSON: input upload cannot be a query control");
 std::string all;for(int i=0;i<256;++i)all+=static_cast<char>(i);auto failed=execute_root_cause_workbench({{all,"",input,false}});const auto failed_root=fields(failed.json);assert(fields(elements(failed_root.at("originals")).front()).at("field")==word(all));assert(fields(failed_root.at("error")).at("message")==word("root cause workbench JSON: unknown control or upload: "));
 const auto odd=std::string("\xff\0tail",6);failed=execute_root_cause_workbench({{odd,"",input,false}});assert(fields(fields(failed.json).at("error")).at("message")==word(std::string("root cause workbench JSON: unknown control or upload: ")+static_cast<char>(0xff)));
 const std::string boundary="root-cause-native-test";const auto wire=[&](bool filename){return "--"+boundary+"\r\nContent-Disposition: form-data; name=\"input_file\""+(filename?"; filename=\"\"":"")+"\r\n\r\n"+input+"\r\n--"+boundary+"--\r\n";};
 for(bool filename:{false,true}){root=success(execute_root_cause_workbench_multipart("multipart/form-data; boundary="+boundary,wire(filename)),"RootCauseRepresentation");const auto part=fields(elements(root.at("originals")).front());assert(part.at("file")==std::string(filename?"true":"false"));assert(bytes(part.at("original"))==input);}
 failure(execute_root_cause_workbench_multipart("text/plain",input),"configuration_error",false,"invalid_argument");failure(execute_root_cause_workbench_multipart("multipart/form-data; boundary="+boundary,wire(false),{},false),"configuration_error",false,"invalid_argument","root cause workbench JSON: multipart transport did not complete");
 // Public option storage is transactional; whole submitted strings retain the
 // existing byte codec's sixbyte WTF8 versus scalar Unicode distinction.
 const std::string pair="\xed\xa0\x80\xed\xb0\x80",scalar="\xf0\x90\x80\x80";const auto options=parse_root_cause_workbench_options_json("{\"string_attributes\":["+word(pair)+","+word(scalar)+"]}");assert(options.string_attributes[0]==pair&&options.string_attributes[1]==scalar&&pair!=scalar);
}
} // namespace
int main(){options_and_full_storage();ordinary_calls_and_artifacts();shared_calls_complete_graph_and_reopening();transport_presence_validation_order_and_bytes();}
