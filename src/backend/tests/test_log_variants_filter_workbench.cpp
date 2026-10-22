#include "minera/log_variants_filter_workbench.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "../log_variants_original_call_parser.hpp"
#include <functional>
#include <iostream>
#include <tuple>

namespace log_variants_workbench_test {
using namespace minera;
namespace detail=minera::log_variants_original_call_detail;
using Parts=std::vector<DiscoveryWorkbenchPart>;
inline void require(bool ok,const std::string& message){if(!ok)throw std::runtime_error(message);}
inline DiscoveryWorkbenchPart field(std::string name,std::string value){return {std::move(name),"",std::move(value),false};}
inline std::optional<std::string> text(const std::string& raw,std::string_view key){const auto root=detail::shared::Reader(raw).read();const auto* value=detail::shared::field(root,key);if(!value||value->kind==detail::Node::kNull)return std::nullopt;require(value->kind==detail::Node::kString,"expected native string: "+std::string(key));return value->text;}
inline EventLog source_log(){
  EventLog source;source.attributes={{"opaque",std::string("\xff\0",2)}};source.metadata.properties={{"retained","context"}};
  for(std::size_t i=0;i<3;++i){Trace t;t.case_id="c"+std::to_string(i);t.case_id_source_key="concept:name";t.case_id_state=FirstClassAttributeState::kPresent;t.case_id_attribute=EventAttribute{"concept:name",t.case_id};t.attributes={{"concept:name",t.case_id}};t.properties={{"retained","trace"}};
    Event e;e.activity=i==2?"B":"A";e.activity_source_key="concept:name";e.activity_state=FirstClassAttributeState::kPresent;e.activity_attribute=EventAttribute{"concept:name",e.activity};e.input_order=900+i;e.attributes={{"integer","9007199254740993",AttributeKind::kInt}};t.events.push_back(e);source.traces.push_back(t);}
  return source;
}
inline std::string call(const std::string& args,const std::string& keywords="[]"){return "{\"schema\":\"minera-log-variants-original-call-v1\",\"args\":"+args+",\"keyword_entries\":"+keywords+'}';}
inline Parts request(const EventLog& source,const std::string& op,const std::string& json){return {field("operation",op),{"source_file","source.csv",serialize_embedding_workbench_raw_log_json(source),true},field("call_json",json)};}
inline void envelope(const DiscoveryWorkbenchResponse& response){
  const auto root=detail::shared::Reader(response.json).read();std::set<std::string> names;for(const auto& [name,value]:root.fields){(void)value;names.insert(name);}
  require(names==std::set<std::string>{"family","operation","http_status","phase","source_format","source","result","source_unchanged","call_mode","call_json","controls_before_json","controls_after_json","controls_unchanged","source_log_json","source_after_log_json","filtered_log_json","filtered_xes","xes_error","value_json","error_type","error"},"response is not exact21field envelope");
}
inline void preserved(const DiscoveryWorkbenchResponse& response,const EventLog& source,const std::string& json){
  envelope(response);require(text(response.json,"call_json")==json,"raw call bytes changed");const auto before=serialize_embedding_workbench_raw_log_json(source);
  require(text(response.json,"source_log_json")==before&&text(response.json,"source_after_log_json")==before,"full source snapshot changed");
  const auto controls=text(response.json,"controls_before_json");require(controls&&controls==text(response.json,"controls_after_json"),"actual complete controls changed");
  require(response.json.find("\"source_unchanged\":true")!=std::string::npos&&response.json.find("\"controls_unchanged\":true")!=std::string::npos,"unchanged flags lost");
}
inline void workflows(const std::function<DiscoveryWorkbenchResponse(const Parts&)>& run){
  const auto source=source_log();const std::string tuple_a=R"({"type":"tuple","values":[{"type":"str","value":"A"}]})";
  const std::string list_a="{\"type\":\"list\",\"values\":["+tuple_a+"]}";
  const std::string half=R"({"type":"float","hex":"0x1.0000000000000p-1"})";
  const std::string one=R"({"type":"int","value":1})";
  const std::string variants="{\"type\":\"dict\",\"items\":[["+tuple_a+R"(,{"type":"list","values":[{"type":"trace_ref","source_trace_index":0},{"type":"trace_ref","source_trace_index":1}]}],[{"type":"tuple","values":[{"type":"str","value":"B"}]},{"type":"list","values":[{"type":"trace_ref","source_trace_index":2}]}]]})";
  const auto invoke=[&](const std::string& operation,const std::string& json){const auto response=run(request(source,operation,json));preserved(response,source,json);return response;};
  const std::vector<std::tuple<std::string,std::string,std::vector<std::size_t>>> successes={
    {"apply","["+list_a+"]",{0,1}},
    {"filter_log_variants_percentage","["+half+"]",{0,1}},
    {"filter_variants_by_coverage_percentage","["+half+"]",{0,1}},
    {"filter_variants_by_maximum_coverage_percentage","["+half+"]",{2}},
    {"filter_variants_top_k","["+one+"]",{0,1}},
    {"filter_variants_variants_percentage","["+variants+"]",{0,1}}};
  for(const auto& [operation,args,indices]:successes){const auto json="\r\n "+call(args)+"\r\n";const auto response=invoke(operation,json);require(response.status==200,"original log outcome failed: "+response.json);EventLog expected;copy_event_log_context(source,expected);for(const auto i:indices)expected.traces.push_back(source.traces[i]);const auto actual=text(response.json,"filtered_log_json");require(actual==serialize_embedding_workbench_raw_log_json(expected),"full log outcome differs: "+operation);require(serialize_embedding_workbench_raw_log_json(parse_embedding_workbench_raw_log_json(*actual))==*actual,"result reopen differs");require(!text(response.json,"value_json"),"log returned scalar artifact");}
  const auto scalar_json=call("["+variants+","+half+"]");const auto scalar=invoke("find_auto_threshold",scalar_json);require(scalar.status==200,"scalar helper failed: "+scalar.json);EventAttribute expected{"",std::to_string(2.0/3.0),AttributeKind::kFloat};expected.typed_value=2.0/3.0;EventTable table;table.columns={"value"};table.dtypes={"float64"};table.rows={{expected}};
  require(text(scalar.json,"value_json")==serialize_embedding_workbench_table_json(table),"full native scalar artifact differs");require(!text(scalar.json,"filtered_log_json")&&!text(scalar.json,"filtered_xes"),"scalar fabricated filtered File/XES");
  const auto omitted=invoke("filter_log_variants_percentage",call("[]"));require(omitted.status==200,"omitted cumulative default failed");const auto controls=detail::shared::Reader(*text(omitted.json,"controls_before_json")).read();require(controls.fields.size()==5&&detail::at(controls,"resolved").fields.size()==5,"actual controls omitted fields");
  const auto bom=invoke("apply",std::string("\xef\xbb\xbf",3)+"\r\n "+call("["+list_a+"]")+"\r\n");require(bom.status==200,"BOM parse-view handling failed");
  for(const auto& parameter:std::vector<std::string>{R"({"type":"NoneType","value":null})",R"({"type":"list","values":[]})",R"({"type":"dict","items":[]})"}){const auto json=call("["+list_a+"]","[{\"key\":\"parameters\",\"value\":"+parameter+"}]");require(invoke("apply",json).status==200,"null/rawlist/emptydictionary parameters failed");}
  const auto bad=invoke("apply",call("["+list_a+"]",R"([{"key":"parameters","value":{"type":"native_na"}}])"));require(bad.status==400&&text(bad.json,"phase")=="filter"&&text(bad.json,"error_type")=="TypeError","raw parameters did not reach native filter");
  for(const auto& json:std::vector<std::string>{"",call("[]"),R"({"schema":"minera-log-variants-original-call-v1","args":[],"\u0061rgs":[],"keyword_entries":[]})",call("["+list_a+"]",R"([{"key":"parameters","value":{"type":"dict","items":[]}},{"key":"parameters","value":{"type":"NoneType","value":null}}])")}){
    const auto response=run(request(source,"apply",json));envelope(response);require(response.status==400&&text(response.json,"phase")=="control"&&text(response.json,"call_json")==json,"malformed call not closed with exact echo");}
  auto bytes=request(source,"apply",call("["+list_a+"]"));bytes.back().file=true;bytes.back().filename="call_json.txt";require(run(bytes).status==200,"canonical call File rejected");
  bytes.back().filename="other.json";const auto wrong=run(bytes);require(wrong.status==400&&!text(wrong.json,"call_json")&&!text(wrong.json,"call_mode"),"invalid File captured exclusive mode");
  auto duplicate=request(source,"apply",call("["+list_a+"]"));duplicate.push_back(duplicate.back());const auto dup=run(duplicate);require(dup.status==400&&!text(dup.json,"call_json"),"duplicate raw call accepted");
  auto missing=request(source,"find_auto_threshold",scalar_json);missing.erase(missing.begin()+1);const auto no_source=run(missing);require(no_source.status==400&&text(no_source.json,"phase")=="control","helper omitted source File");
  auto broken=request(source,"apply",call("["+list_a+"]"));broken[1].bytes="bad native log";const auto import=run(broken);require(import.status==400&&text(import.json,"phase")=="source_import"&&text(import.json,"call_json")==broken.back().bytes,"source import boundary lost");
  auto range=request(source,"filter_variants_variants_percentage",call(R"([{"type":"dict","items":[[{"type":"tuple","values":[]},{"type":"list","values":[{"type":"trace_ref","source_trace_index":999}]}]]}])"));require(run(range).status==400,"out-of-range source reference accepted");
}
}
int main(){try{log_variants_workbench_test::workflows([](const auto& parts){return minera::execute_log_variants_filter(parts);});const auto response=minera::execute_log_variants_filter({}, {},"incomplete multipart");log_variants_workbench_test::require(response.status==400,"transport accepted");std::cout<<"original LOG variants complete workflow controls/artifacts passed\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}return 0;}
