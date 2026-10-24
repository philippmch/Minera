#include <atomic>
#include <chrono>
#include <functional>
#include <sstream>
#include <thread>
#include <variant>
#include "httplib.h"
#include "../tools/log_timestamp_filter_route.hpp"
#include "../polars_ltl_original_call_parser.hpp"
#include "minera/xes_export.hpp"
#include "minera/xes_event_log_importer.hpp"
// Unchanged admitted native oracle supplies its independently mapped constructors.
// Rename only the entry point; execute its actual case driver explicitly below.
#define main log_timestamp_original_oracle_entry_not_invoked
#include "test_log_timestamp_filter.cpp"
#undef main

namespace {
namespace tj=minera::polars_ltl_original_call_detail;
using TimestampInvoke=std::function<DiscoveryWorkbenchResponse(const std::vector<DiscoveryWorkbenchPart>&)>;
using TimestampNativeResult=std::variant<EventLog,bool>;
void timestamp_need(bool value,const std::string& message){if(!value)throw std::runtime_error(message);}
const tj::Node& timestamp_at(const tj::Node& n,std::string_view key){const auto* v=tj::field(n,key);timestamp_need(v!=nullptr,"missing envelope field "+std::string(key));return *v;}
std::string timestamp_q(std::string_view s){return serialize_embedding_workbench_string_json(s);}
std::string timestamp_cell(const EventAttribute& a){EventTable table;table.columns={a.key};table.dtypes={"object"};table.rows={{a}};return serialize_embedding_workbench_table_json(table);}
std::string timestamp_text(const tj::Node& node){
 if(node.kind==tj::Node::kString)return node.text;
 timestamp_need(node.kind==tj::Node::kObject&&node.fields.size()==2,"not a native string");
 timestamp_need(timestamp_at(node,"native_bytes").text=="hex-v1","not native hex bytes");
 const auto& hex=timestamp_at(node,"data").text;timestamp_need(hex.size()%2==0,"bad native hex length");std::string out;
 for(std::size_t i=0;i<hex.size();i+=2){const auto digit=[](char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;throw std::runtime_error("bad native hex");};out+=static_cast<char>(16*digit(hex[i])+digit(hex[i+1]));}return out;
}
bool timestamp_equal(const tj::Node& a,std::string_view ar,const tj::Node& b,std::string_view br){
 if(a.kind!=b.kind)return false;
 if(a.kind==tj::Node::kString)return a.text==b.text;
 if(a.kind==tj::Node::kBoolean)return a.boolean==b.boolean;
 if(a.kind==tj::Node::kNull)return true;
 if(a.kind==tj::Node::kNumber)return ar.substr(a.begin,a.end-a.begin)==br.substr(b.begin,b.end-b.begin);
 if(a.kind==tj::Node::kArray){if(a.items.size()!=b.items.size())return false;for(std::size_t i=0;i<a.items.size();++i)if(!timestamp_equal(a.items[i],ar,b.items[i],br))return false;return true;}
 if(a.fields.size()!=b.fields.size())return false;
 for(const auto& [k,v]:a.fields){const auto* other=tj::field(b,k);if(!other||!timestamp_equal(v,ar,*other,br))return false;}return true;
}
void timestamp_json_equal(const std::string& a,const std::string& b,const std::string& label){timestamp_need(timestamp_equal(tj::Reader(a).read(),a,tj::Reader(b).read(),b),label);}
std::string timestamp_controls(const EventLog& source,const std::vector<EventAttribute>& operands,const LogTimestampFilterOptions* options,const std::optional<std::size_t>& selected,const std::vector<std::string>& names){
 std::string source_argument;
 if(selected){EventLog wrapper;wrapper.traces={source.traces.at(*selected)};source_argument="{\"kind\":\"trace\",\"source_trace_index\":"+std::to_string(*selected)+",\"log_json\":null,\"trace_json\":"+timestamp_q(serialize_embedding_workbench_raw_log_json(wrapper))+'}';}
 else source_argument="{\"kind\":\"log\",\"source_trace_index\":null,\"log_json\":"+timestamp_q(serialize_embedding_workbench_raw_log_json(source))+",\"trace_json\":null}";
 std::string args="[";for(std::size_t i=0;i<operands.size();++i){if(i)args+=',';args+="{\"name\":"+timestamp_q(names.at(i))+",\"position\":"+std::to_string(i)+",\"value_json\":"+timestamp_q(timestamp_cell(operands[i]))+'}';}args+=']';
 std::string opt="null";if(options)opt="{\"timestamp_key_json\":"+timestamp_q(timestamp_cell(options->timestamp_key))+",\"enum_timestamp_key_json\":"+(options->enum_timestamp_key?timestamp_q(timestamp_cell(*options->enum_timestamp_key)):"null")+",\"positive_json\":"+timestamp_q(timestamp_cell(options->positive))+'}';
 return "{\"schema\":\"minera-log-timestamp-native-controls-v1\",\"source_argument\":"+source_argument+",\"arguments\":"+args+",\"options\":"+opt+'}';
}
void timestamp_reopen_log(const std::string& text){timestamp_need(serialize_embedding_workbench_raw_log_json(parse_embedding_workbench_raw_log_json(text))==text,"complete native log reopen changed");}
void timestamp_reopen_cell(const std::string& text){timestamp_need(serialize_embedding_workbench_table_json(parse_embedding_workbench_table_json(text))==text,"complete native cell reopen changed");}
void timestamp_reopen_controls(const std::string& raw){
 const auto controls=tj::Reader(raw).read();const auto& source=timestamp_at(controls,"source_argument");
 for(const auto name:{"log_json","trace_json"}){const auto& n=timestamp_at(source,name);if(n.kind!=tj::Node::kNull)timestamp_reopen_log(timestamp_text(n));}
 for(const auto& a:timestamp_at(controls,"arguments").items)timestamp_reopen_cell(timestamp_text(timestamp_at(a,"value_json")));
 const auto& options=timestamp_at(controls,"options");if(options.kind!=tj::Node::kNull)for(const auto& [name,value]:options.fields){(void)name;if(value.kind!=tj::Node::kNull)timestamp_reopen_cell(timestamp_text(value));}
}
void timestamp_transport_check(const TimestampInvoke& invoke,std::size_t index,const std::string& operation,const std::string& call,EventLog& source,std::vector<EventAttribute>& operands,LogTimestampFilterOptions* options,std::optional<std::size_t> selected,const std::vector<std::string>& names,const std::string& projection,const EventLog& expected_after,const std::optional<EventLog>& expected_log,const std::optional<bool>& expected_bool,const std::optional<std::string>& expected_type,const std::optional<std::string>& expected_message,const std::function<TimestampNativeResult()>& native){
 const auto label=std::to_string(index)+" "+operation+": ";
 const auto before=serialize_embedding_workbench_raw_log_json(source),controls_before=timestamp_controls(source,operands,options,selected,names);
 std::optional<TimestampNativeResult> actual;std::optional<std::string> error_type,error;
 try{actual=native();}catch(const LogTimestampFilterError& e){error_type=e.type;error=e.message;}
 const auto after=serialize_embedding_workbench_raw_log_json(source),controls_after=timestamp_controls(source,operands,options,selected,names);
 timestamp_need(after==serialize_embedding_workbench_raw_log_json(expected_after),label+"independent native source-after mismatch");
 timestamp_need(error_type==expected_type&&error==expected_message,label+"independent native full error mismatch");
 timestamp_need(actual.has_value()==!expected_type.has_value(),label+"independent native return state mismatch");
 if(expected_log)timestamp_need(actual&&std::holds_alternative<EventLog>(*actual)&&serialize_embedding_workbench_raw_log_json(std::get<EventLog>(*actual))==serialize_embedding_workbench_raw_log_json(*expected_log),label+"independent full native returned log mismatch");
 if(expected_bool)timestamp_need(actual&&std::holds_alternative<bool>(*actual)&&std::get<bool>(*actual)==*expected_bool,label+"independent native Boolean mismatch");
 const auto response=invoke({{"operation","",operation,false},{"source_file","actual-native.json",before,true},{"call_json","call_json.txt",call,true}});
 const auto root=tj::Reader(response.json).read();const std::set<std::string> keys={"family","operation","http_status","phase","source_format","source","result","source_unchanged","call_mode","call_json","controls_before_json","controls_after_json","controls_unchanged","source_log_json","source_after_log_json","filtered_log_json","filtered_xes","xes_error","value_json","error_type","error","native_returned","result_kind","call_projection_json","inspection_error"};
 timestamp_need(root.fields.size()==keys.size(),label+"not exact25 envelope");for(const auto& [name,value]:root.fields){(void)value;timestamp_need(keys.contains(name),label+"unknown envelope key");}
 const auto node=[&](std::string_view key)->const tj::Node&{return timestamp_at(root,key);};
 const auto text=[&](std::string_view key){return timestamp_text(node(key));};
 const auto nil=[&](std::string_view key){timestamp_need(node(key).kind==tj::Node::kNull,label+"expected null "+std::string(key));};
 timestamp_need(response.status==(expected_type?400:200)&&node("http_status").kind==tj::Node::kNumber&&response.json.substr(node("http_status").begin,node("http_status").end-node("http_status").begin)==std::to_string(response.status),label+"wrong status");
 timestamp_need(text("family")=="REFERENCE_LOG_TIMESTAMP"&&text("operation")==operation&&text("source_format")=="native_log"&&text("call_mode")=="original"&&text("call_json")==call,label+"family/call echo mismatch");
 timestamp_need(text("phase")== (expected_type?"native_call":"complete")&&node("native_returned").kind==tj::Node::kBoolean&&node("native_returned").boolean==actual.has_value(),label+"wrong native return phase");
 timestamp_need(text("source_log_json")==before&&text("source_after_log_json")==after,label+"full backing log before/after mismatch");
 timestamp_need(node("source_unchanged").kind==tj::Node::kBoolean&&node("source_unchanged").boolean==(before==after),label+"source comparison mismatch");
 timestamp_json_equal(text("controls_before_json"),controls_before,label+"actual controls-before mismatch");timestamp_json_equal(text("controls_after_json"),controls_after,label+"actual controls-after mismatch");
 timestamp_need(node("controls_unchanged").kind==tj::Node::kBoolean&&node("controls_unchanged").boolean==(controls_before==controls_after),label+"controls comparison mismatch");
 timestamp_json_equal(text("call_projection_json"),projection,label+"full projection metadata mismatch");nil("inspection_error");
 const auto counts=[](const EventLog& log){return "{\"traces\":"+std::to_string(log.trace_count())+",\"events\":"+std::to_string(log.event_count())+'}';};
 timestamp_json_equal(std::string(response.json.substr(node("source").begin,node("source").end-node("source").begin)),counts(source),label+"source count mismatch");
 if(expected_type){timestamp_need(text("error_type")==*expected_type&&text("error")==*expected_message,label+"full native diagnostic mismatch");for(const auto key:{"result","result_kind","filtered_log_json","filtered_xes","xes_error","value_json"})nil(key);}
 else{nil("error_type");nil("error");if(expected_log){
   timestamp_need(text("result_kind")=="log"&&text("filtered_log_json")==serialize_embedding_workbench_raw_log_json(*expected_log),label+"full returned native log mismatch");nil("value_json");
   timestamp_json_equal(std::string(response.json.substr(node("result").begin,node("result").end-node("result").begin)),counts(*expected_log),label+"returned count mismatch");
   std::optional<std::string> expected_xes,expected_xes_error;try{XesExportOptions opt;opt.preserve_nulls=true;expected_xes=serialize_xes(*expected_log,opt);}catch(const std::exception& e){expected_xes_error=e.what();}
   if(expected_xes){timestamp_need(text("filtered_xes")==*expected_xes,label+"complete XES mismatch");nil("xes_error");std::istringstream stream(*expected_xes);(void)import_xes_event_log(stream);}
   else{nil("filtered_xes");timestamp_need(text("xes_error")==*expected_xes_error,label+"full XES diagnostic mismatch");}
   timestamp_reopen_log(text("filtered_log_json"));
  }else{timestamp_need(text("result_kind")=="bool",label+"wrong result kind");for(const auto key:{"result","filtered_log_json","filtered_xes","xes_error"})nil(key);EventAttribute value{"",*expected_bool?"true":"false",AttributeKind::kBoolean};value.typed_value=*expected_bool;timestamp_need(text("value_json")==timestamp_cell(value),label+"complete typed bool mismatch");timestamp_reopen_cell(text("value_json"));}
 }
 timestamp_reopen_log(text("source_log_json"));timestamp_reopen_log(text("source_after_log_json"));timestamp_reopen_controls(text("controls_before_json"));timestamp_reopen_controls(text("controls_after_json"));
 // Reopen every original-call descriptor and projection as JSON, retaining raw call bytes separately.
 (void)tj::Reader(text("call_json")).read();(void)tj::Reader(text("call_projection_json")).read();
}
#include "log_timestamp_workbench_public_vectors.inc"
class TimestampOwnedHttp {
 public:
 TimestampOwnedHttp(){server.Post("/api/reference-log-timestamp",handle_log_timestamp_filter);port=server.bind_to_any_port("127.0.0.1");timestamp_need(port>0,"timestamp HTTP bind failed");worker=std::thread([&]{listened=server.listen_after_bind();finished=true;});const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);while(!server.is_running()&&!finished&&std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));if(!server.is_running()){stop();throw std::runtime_error("timestamp listener did not start");}}
 ~TimestampOwnedHttp(){stop();}void stop(){server.stop();if(worker.joinable())worker.join();}
 httplib::Server server;std::thread worker;int port=-1;std::atomic<bool> listened=false,finished=false;
};
DiscoveryWorkbenchResponse timestamp_http(httplib::Client& client,const std::vector<DiscoveryWorkbenchPart>& parts){
 const std::string boundary="timestamp-retained342-owned-boundary";std::string body;
 for(const auto& part:parts){body+="--"+boundary+"\r\nContent-Disposition: form-data; name=\""+part.name+'"';if(part.file)body+="; filename=\""+part.filename+'"';body+="\r\nContent-Type: application/octet-stream\r\n\r\n"+part.bytes+"\r\n";}body+="--"+boundary+"--\r\n";
 const auto response=client.Post("/api/reference-log-timestamp",body,"multipart/form-data; boundary="+boundary);timestamp_need(response&&response->get_header_value("Content-Type").starts_with("application/json"),"timestamp owned HTTP response missing");return {response->status,response->body};
}
}
int main(){try{
 retained_public_timestamp_filter_cases();native_timestamp_filter_storage_controls();
 retained_timestamp_transport_cases([](const auto& parts){return execute_log_timestamp_filter(parts);});
 TimestampOwnedHttp owned;httplib::Client client("127.0.0.1",owned.port);client.set_connection_timeout(5,0);client.set_read_timeout(10,0);
 retained_timestamp_transport_cases([&](const auto& parts){return timestamp_http(client,parts);});owned.stop();timestamp_need(owned.listened,"owned timestamp listener failed");
 std::cout<<"342 original native +342 direct +342 owned HTTP timestamp calls; all25 fields, actual controls and artifact reopen passed\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}return 0;}
