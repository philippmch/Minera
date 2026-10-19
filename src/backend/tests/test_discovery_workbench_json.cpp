#include "minera/discovery_workbench_json.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/literal_controls_json.hpp"
#include "minera/object_centric_playout_workbench_json.hpp"
#include "minera/genetic_matrix_workbench_json.hpp"
#include "minera/log_conversion.hpp"
#include "minera/xes_export.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <vector>
using namespace minera;
namespace {
using Part=DiscoveryWorkbenchPart;
std::size_t end_value(std::string_view s,std::size_t start){
 if(s[start]=='"'){for(std::size_t i=start+1;i<s.size();++i){if(s[i]=='\\'){++i;continue;}if(s[i]=='"')return i+1;}assert(false);}
 if(s[start]=='['||s[start]=='{'){std::vector<char> stack{s[start]};bool quote=false;for(std::size_t i=start+1;i<s.size();++i){if(quote){if(s[i]=='\\')++i;else if(s[i]=='"')quote=false;continue;}if(s[i]=='"'){quote=true;continue;}if(s[i]=='['||s[i]=='{')stack.push_back(s[i]);else if(s[i]==']'||s[i]=='}'){assert(!stack.empty());stack.pop_back();if(stack.empty())return i+1;}}assert(false);}
 const auto end=s.find_first_of(",}] \r\n\t",start);return end==std::string_view::npos?s.size():end;
}
std::string member(std::string_view s,std::string_view name){assert(!s.empty()&&s[0]=='{');std::size_t i=1;while(i<s.size()&&s[i]!='}'){const auto key_end=end_value(s,i);const auto key=s.substr(i+1,key_end-i-2);i=key_end;assert(s[i++]==':');const auto value_end=end_value(s,i);if(key==name)return std::string(s.substr(i,value_end-i));i=value_end;if(s[i]==',')++i;}assert(false);return {};}
std::string first(std::string_view array){assert(array[0]=='['&&array[1]!=']');return std::string(array.substr(1,end_value(array,1)-1));}
std::size_t length(std::string_view array){assert(array[0]=='[');std::size_t i=1,n=0;while(array[i]!=']'){++n;i=end_value(array,i);if(array[i]==',')++i;}return n;}
Part field(std::string key,std::string value){return {std::move(key),"",std::move(value),false};}
Part file(std::string key,std::string value,std::string filename="exact.json"){return {std::move(key),std::move(filename),std::move(value),true};}

DiscoveryWorkbenchResponse okay(const std::vector<Part>& parts){const auto result=execute_discovery_workbench(parts);if(result.status!=200)std::cerr<<result.json<<'\n';assert(result.status==200);assert(member(result.json,"executed")=="true"&&member(result.json,"returned")=="true");assert(member(result.json,"error")=="null");assert(length(member(result.json,"native_calls"))==1);assert(member(result.json,"inputs_before_native")==member(result.json,"inputs_after_native"));return result;}
EventLog log(){EventLog value;Trace trace;trace.case_id="c";trace.case_id_state=FirstClassAttributeState::kPresent;trace.attributes.push_back({"stored","trace"});for(const auto* activity:{"A","B","C"}){Event event;event.case_id="c";event.case_id_state=FirstClassAttributeState::kPresent;event.activity=activity;event.activity_state=FirstClassAttributeState::kPresent;event.attributes.push_back({"stored","event"});trace.events.push_back(std::move(event));}value.traces.push_back(std::move(trace));value.attributes.push_back({"stored","log"});return value;}
std::string raw(const EventLog& value){return serialize_embedding_workbench_raw_log_json(value);}
// The public stream inspection serializer may return {legacy_view,snapshot};
// this route intentionally accepts only the complete raw constructor grammar.
// These finite scalar-UTF8 fixtures retain native attribute/metadata storage via
// the full raw-log codec rather than passing an inspection envelope as input.
std::string stream_constructor(const EventStream& stream){
 EventLog holder;holder.attributes=stream.attributes;holder.metadata=stream.metadata;
 const auto root=raw(holder);std::string result="{\"attributes\":"+member(root,"attributes")+",\"metadata\":"+member(root,"metadata")+",\"events\":[";
 for(const auto& event:stream.events){if(result.back()!='[')result+=',';EventLog fields;fields.attributes=event.fields;result+="{\"fields\":"+member(raw(fields),"attributes")+'}';}
 return result+"]}";
}
std::vector<Part> source(std::string op,const EventLog& value){return {field("operation",std::move(op)),field("source_format","raw_log"),file("source_file",raw(value))};}
std::string artifact(std::string_view envelope,std::string_view name){auto values=member(envelope,"artifacts");std::size_t i=1;while(values[i]!=']'){const auto end=end_value(values,i);const auto item=values.substr(i,end-i);if(member(item,"name")==serialize_embedding_workbench_string_json(name))return item;i=end;if(values[i]==',')++i;}assert(false);return {};}
void net_and_markings(std::string_view actual,const PetriNet& net,const WorkflowMarkings& markings){auto parsed=parse_oc_playout_workbench_petri_net_json(member(actual,"net"));assert(serialize_oc_playout_workbench_petri_net_json(parsed)==serialize_oc_playout_workbench_petri_net_json(net));assert(length(member(member(actual,"net"),"transitions"))==net.transitions().size());const auto expected=serialize_genetic_matrix_workbench_markings_json(markings);assert(member(member(actual,"markings"),"initial")==member(expected,"initial"));assert(member(member(actual,"markings"),"final")==member(expected,"final"));for(const auto* name:{"place_count","source_storage","sink_storage","has_source","has_sink","activities","transitions","discovery_markings","transition_names"})(void)member(member(actual,"net"),name);}
void five_public_entrypoints_and_defaults(){
 const auto input=log();
 for(const auto* op:{"mine_ilp","discover_ilp_miner","mine_genetic","discover_genetic_miner","discover_genetic"}){
  const auto response=okay(source(op,input));const auto result=member(response.json,"native_result");const auto configured=member(response.json,"configured_options");
  assert(member(first(member(response.json,"native_calls")),"name")==serialize_embedding_workbench_string_json(std::string(op)+"(log,options)"));assert(member(response.json,"options_supplied")=="[]");assert(length(member(response.json,"export_calls"))==3);
  if(std::string(op).find("ilp")!=std::string::npos){const auto direct=std::string(op)=="mine_ilp"?mine_ilp(input):discover_ilp_miner(input);net_and_markings(result,direct.net,direct.markings);const auto expected=serialize_literal_controls_result_json(direct);for(const auto* key:{"computed","exhausted","visited_nodes","lp_solved","branch_and_bound_nodes","message"})assert(member(member(result,"diagnostics"),key)==member(member(expected,"diagnostics"),key));assert(member(configured,"max_nodes")=="100000"&&member(configured,"alpha")=="1.0");assert(&direct.petri_net()==&direct.net&&&direct.workflow_markings()==&direct.markings);}
  else {const auto direct=std::string(op)=="mine_genetic"?mine_genetic(input):std::string(op)=="discover_genetic_miner"?discover_genetic_miner(input):discover_genetic(input);net_and_markings(result,direct.net,direct.markings);assert(member(result,"matrix")==serialize_genetic_matrix_workbench_model_json(direct.matrix));const auto d=member(result,"diagnostics");assert(member(d,"computed")=="true"&&member(d,"seed")=="0"&&member(d,"activity_count")=="3");for(const auto* key:{"exhausted","generations_completed","candidates_evaluated","population_size","best_fitness","message"})(void)member(d,key);assert(member(configured,"population_size")=="32"&&member(configured,"generations")=="20");assert(&direct.petri_net()==&direct.net&&&direct.workflow_markings()==&direct.markings);}
  for(const auto* name:{"configured_options.json","source_constructor.json","prepared_log.json","inputs_before_native.json","inputs_after_native.json","native_result.json","net.json","markings.json","diagnostics.json","legacy_result.json","source.xes","model.pnml"})(void)artifact(response.json,name);
 }
 // Omitted operation is the actual first public ILP function, before any mining cache.
 auto p=source("mine_ilp",input);p.erase(p.begin());const auto response=okay(p);assert(member(response.json,"operation")=="\"mine_ilp\"");
}
void exact_options_and_native_limits(){
 auto p=source("discover_ilp_miner",log());p.push_back(field("options_json",R"({"activity_key":{"native_bytes":"hex-v1","data":"ff00"},"artificial_start_activity":"","artificial_end_activity":"","alpha":-0.0,"noise_threshold":-0.0,"causal_relation":[["A","B"],["A","B"]],"max_nodes":18446744073709551615})"));auto response=okay(p);auto c=member(response.json,"configured_options");assert(length(member(response.json,"options_supplied"))==7);assert(member(c,"alpha")=="-0.0"&&member(c,"noise_threshold")=="-0.0");assert(member(c,"causal_relation")==R"([["A","B"],["A","B"]])");assert(member(c,"max_nodes")=="18446744073709551615");assert(member(c,"activity_key")==R"({"data":"ff00","native_bytes":"hex-v1"})");
 p=source("discover_genetic",log());p.push_back(field("options_json",R"({"activity_key":"","timestamp_key":"","case_id_key":"","seed":18446744073709551615,"population_size":4,"generations":0,"elitism_rate":-0.0,"crossover_rate":1.0,"mutation_rate":0.0,"elitism_min_sample":1,"max_activities":18446744073709551615,"max_states":1})"));response=okay(p);c=member(response.json,"configured_options");assert(length(member(response.json,"options_supplied"))==12);assert(member(c,"seed")=="18446744073709551615"&&member(c,"max_activities")=="18446744073709551615");assert(member(c,"elitism_rate")=="-0.0");const auto d=member(member(response.json,"native_result"),"diagnostics");assert(member(d,"seed")=="18446744073709551615"&&member(d,"exhausted")=="true"&&member(d,"candidates_evaluated")=="1");assert(execute_discovery_workbench(p).json==response.json);
 for(const auto* bad:{R"({"seed":18446744073709551616})",R"({"max_states":-1})",R"({"population_size":1.0})",R"({"seed":0,"seed":1})",R"({"unknown":0})"}){auto q=source("mine_genetic",log());q.push_back(field("options_json",bad));const auto failure=execute_discovery_workbench(q);assert(failure.status==400&&member(failure.json,"executed")=="false");assert(member(member(failure.json,"error"),"phase")=="\"configuration_error\"");assert(member(failure.json,"result_type")=="\"GeneticMinerResult\"");}
 for(const auto* bad:{R"({"population_size":1})",R"({"generations":10001})",R"({"max_states":0})",R"({"elitism_rate":"NaN"})",R"({"mutation_rate":"INF"})",R"({"crossover_rate":"-INF"})",R"({"elitism_min_sample":0})"}){auto q=source("mine_genetic",log());q.push_back(field("options_json",bad));const auto failure=execute_discovery_workbench(q);assert(failure.status==400&&member(failure.json,"executed")=="true"&&member(failure.json,"returned")=="false");assert(member(member(failure.json,"error"),"phase")=="\"native_error\"");assert(member(failure.json,"configured_options")!="null");}
 for(const auto* bad:{R"({"alpha":"NaN"})",R"({"noise_threshold":"INF"})",R"({"max_nodes":0})",R"({"artificial_start_activity":"x","artificial_end_activity":"x"})"}){auto q=source("mine_ilp",log());q.push_back(field("options_json",bad));const auto failure=execute_discovery_workbench(q);assert(failure.status==400&&member(failure.json,"executed")=="true"&&member(failure.json,"returned")=="false");assert(member(member(failure.json,"error"),"phase")=="\"native_error\"");}
 auto exhausted=source("mine_ilp",log());exhausted.push_back(field("options_json",R"({"max_nodes":1})"));response=okay(exhausted);assert(member(member(member(response.json,"native_result"),"diagnostics"),"exhausted")=="true");
 // Empty traces and zero traces have different actual native semantics.
 EventLog empty_trace;empty_trace.traces.emplace_back();response=okay(source("mine_ilp",empty_trace));assert(member(member(member(response.json,"native_result"),"diagnostics"),"computed")=="true");assert(execute_discovery_workbench(source("mine_ilp",EventLog{})).status==400);
}
void source_shapes_and_real_imports(){
 const auto input=log();const auto table=event_data_to_table(EventDataView(input));const auto stream=event_data_to_event_stream(EventDataView(input));
 for(const auto* format:{"raw_table","raw_stream","raw_lazy"}){
  const std::string f=format;auto p=source("mine_ilp",input);p[1]=field("source_format",f);p[2]=file("source_file",f=="raw_table"?serialize_embedding_workbench_table_json(table):f=="raw_stream"?stream_constructor(stream):"{\"table\":"+serialize_embedding_workbench_table_json(table)+",\"valid\":true,\"row_count\":null}");const auto rejected=execute_discovery_workbench(p);assert(rejected.status==400&&member(rejected.json,"executed")=="false");assert(member(member(rejected.json,"error"),"phase")=="\"preparation_error\"");p.push_back(field("source_shape","log"));const auto response=okay(p);assert(member(member(member(response.json,"input_context"),"source"),"selected_kind")=="\"log\"");assert(length(member(member(member(response.json,"input_context"),"source"),"preparation_calls"))==1);
 }
 // The old inspection carrier is not silently reinterpreted as a constructor.
 auto inspection=source("mine_ilp",input);inspection[1]=field("source_format","raw_stream");inspection[2]=file("source_file",serialize_embedding_workbench_stream_json(stream));inspection.push_back(field("source_shape","log"));
 const auto rejected_inspection=execute_discovery_workbench(inspection);assert(rejected_inspection.status==400&&member(rejected_inspection.json,"executed")=="false");assert(member(member(rejected_inspection.json,"error"),"phase")=="\"preparation_error\"");assert(member(member(rejected_inspection.json,"error"),"message")=="\"discovery workbench JSON: unknown field legacy_view\"");
 std::vector<Part> csv={field("source_format","csv"),file("source_file","case_id,activity\nc,A\nc,B\n","input.csv"),field("source_import_options_json",R"({"require_timestamp_column":false})")};auto response=okay(csv);assert(member(member(member(response.json,"input_context"),"source"),"original_kind")=="\"log\"");
 for(bool gzip:{false,true}){XesExportOptions options;options.compress=gzip;std::vector<Part> p={field("operation","mine_genetic"),field("source_format","xes"),file("source_file",serialize_xes(input,options),gzip?"in.xes.gz":"in.xes")};response=okay(p);assert(member(member(member(response.json,"input_context"),"source"),"original_kind")=="\"log\"");}
 auto p=source("mine_ilp",input);p.push_back(field("source_import_options_json","{}"));assert(execute_discovery_workbench(p).status==400);p=source("mine_ilp",input);p.push_back(field("source_preparation_options_json","{}"));assert(execute_discovery_workbench(p).status==400);p=source("mine_ilp",input);p.push_back(field("source_id","invented"));assert(execute_discovery_workbench(p).status==400);
}
void byte_results_and_independent_export_failures(){
 auto input=log();input.traces.front().events.front().activity=std::string("A\0",2);input.traces.front().events.front().attributes.push_back(make_native_na_attribute("unused_native_na"));
 auto response=okay(source("mine_ilp",input));assert(member(response.json,"artifact_errors")!="[]");assert(member(response.json,"returned")=="true"&&member(response.json,"native_result")!="null");
 input=log();input.traces.front().events.front().activity=std::string("\xff\0",2);auto p=source("mine_genetic",input);p.push_back(field("options_json",R"({"population_size":4,"generations":0,"elitism_min_sample":1})"));input.traces.front().events.front().attributes.push_back(make_native_na_attribute("unused_native_na"));p[2]=file("source_file",raw(input));response=okay(p);assert(member(response.json,"artifact_errors")!="[]");const auto matrix=member(member(response.json,"native_result"),"matrix");assert(matrix.find("native_bytes")!=std::string::npos);assert(matrix.find("ff00")!=std::string::npos);assert(artifact(response.json,"matrix.json").find("ff00")!=std::string::npos);
}
void physical_multipart_and_failures(){
 const std::string body="--b\r\nContent-Disposition: form-data; name=\"source_format\"\r\n\r\nraw_log\r\n--b\r\nContent-Disposition: form-data; name=\"source_file\"; filename=\"\"\r\n\r\n"+raw(log())+"\r\n--b--\r\n";
 auto parsed=parse_discovery_workbench_multipart("multipart/form-data; boundary=b",body);assert(parsed.size()==2&&!parsed[0].file&&parsed[1].file&&parsed[1].filename.empty());auto response=execute_discovery_workbench_multipart("multipart/form-data; boundary=b",body);assert(response.status==200&&member(response.json,"returned")=="true");assert(member(member(response.json,"transport_original"),"bytes")==std::to_string(body.size()));
 parsed[1].file=false;assert(execute_discovery_workbench(parsed).status==400);parsed[1].file=true;parsed.push_back(parsed[0]);assert(execute_discovery_workbench(parsed).status==400);
 for(const auto& bad:{std::string{},body.substr(0,body.size()-7)}){response=execute_discovery_workbench_multipart("multipart/form-data; boundary=b",bad);assert(response.status==400&&member(response.json,"executed")=="false");assert(member(member(response.json,"error"),"phase")=="\"transport_error\"");assert(member(member(response.json,"transport_original"),"bytes")==std::to_string(bad.size()));}
 response=execute_discovery_workbench_multipart("multipart/form-data; boundary=b",body,{},false);assert(response.status==400&&member(response.json,"executed")=="false");
 for(const auto* control:{"rng_json","parameters_json","construction_copies_json"}){auto p=source("mine_ilp",log());p.push_back(field(control,"{}"));response=execute_discovery_workbench(p);assert(response.status==400&&member(response.json,"executed")=="false");}
}
// Feature82: compare full transport state explicitly. Native metadata equality
// intentionally ignores these synchronization baselines, including when they
// affect a subsequent native XES export.
std::string metadata82(const EventDataMetadata& value){EventLog holder;holder.metadata=value;return serialize_embedding_workbench_raw_log_json(holder);}
void assert_metadata82(const EventDataMetadata& actual,const EventDataMetadata& expected){
 assert(actual.compatibility_snapshot_valid==expected.compatibility_snapshot_valid);
 assert(actual.compatibility_globals_snapshot.size()==expected.compatibility_globals_snapshot.size());
 assert(actual.compatibility_trace_globals_snapshot.size()==expected.compatibility_trace_globals_snapshot.size());
 assert(actual.compatibility_event_globals_snapshot.size()==expected.compatibility_event_globals_snapshot.size());
 assert(metadata82(actual)==metadata82(expected));
}
EventDataMetadata private_metadata82(int mode){
 EventDataMetadata m;EventAttribute scalar{"retained","literal"};
 if(mode==0||mode==4)m.compatibility_snapshot_valid=true;
 if(mode==1||mode==4)m.compatibility_globals_snapshot={{"baseline",{scalar}}};
 if(mode==2||mode==4)m.compatibility_trace_globals_snapshot={scalar};
 if(mode==3||mode==4)m.compatibility_event_globals_snapshot={scalar};
 if(mode==4){
  const auto bytes=std::string("byte")+static_cast<char>(0)+static_cast<char>(255);
  EventAttribute rich{bytes,bytes};EventAttribute integer{"integer","lexical"};integer.typed_value=std::numeric_limits<std::int64_t>::min();EventAttribute wide{"wide","lexical"};wide.typed_value=std::numeric_limits<std::uint64_t>::max();EventAttribute nan{"nan","lexical"};nan.typed_value=std::numeric_limits<double>::quiet_NaN();EventAttribute minuszero{"minuszero","lexical"};minuszero.typed_value=-0.0;rich.children={integer,wide,nan,minuszero};
  m.extensions={{bytes,bytes,bytes}};m.classifiers={{bytes,{bytes,"",bytes}}};m.trace_globals={rich};m.event_globals={rich};m.properties={rich};m.globals={{bytes,{rich}}};
  m.compatibility_globals_snapshot={{bytes,{rich}}};m.compatibility_trace_globals_snapshot={rich};m.compatibility_event_globals_snapshot={rich};
 }
 return m;
}
std::string empty_scanner_error82(const EventDataMetadata& metadata){
 try{(void)LazyEventTable(std::vector<std::string>{},LazyEventTable::Scanner{},std::vector<EventAttribute>{},metadata,std::nullopt);}catch(const std::invalid_argument& error){return error.what();}
 assert(false);return {};
}

std::string default_lazy_conversion_error82(){
 const LazyEventTable value;
 try{(void)event_data_to_event_log(EventDataView(value),LogConversionOptions{});}catch(const std::invalid_argument& error){return error.what();}
 assert(false);return {};
}

std::string artifact_text82(std::string_view envelope,std::string_view wanted){
 const auto values=member(envelope,"artifacts");std::size_t i=1;while(values[i]!=']'){
  const auto end=end_value(values,i);const auto item=std::string_view(values).substr(i,end-i);
  if(member(item,"name")==serialize_embedding_workbench_string_json(wanted)){
   assert(member(item,"encoding")=="\"utf8\"");return parse_embedding_workbench_result_json("{\"embedding_model\":"+member(item,"data")+'}').embedding_model;
  }
  i=end;if(values[i]==',')++i;
 }
 assert(false);return {};
}

void shared_table_private_metadata82(){
 const auto base=event_log_to_table(log());
 for(int mode=0;mode<5;++mode){
  auto table=base;table.metadata=private_metadata82(mode);const auto raw_table=serialize_embedding_workbench_table_json(table);auto p=source("mine_ilp",log());p[1]=field("source_format","raw_table");p[2]=file("source_file",raw_table);p.push_back(field("source_shape","log"));auto response=okay(p);
  const auto context=member(member(response.json,"input_context"),"source");assert_metadata82(parse_embedding_workbench_table_json(member(context,"original_constructor")).metadata,table.metadata);
  const auto native_prepared=event_data_to_event_log(EventDataView(table),LogConversionOptions{});for(const auto* key:{"inputs_before_native","inputs_after_native"})assert_metadata82(parse_embedding_workbench_raw_log_json(member(member(response.json,key),"source")).metadata,native_prepared.metadata);
  assert(artifact_text82(response.json,"inputs_after_native.json")==member(response.json,"inputs_after_native"));assert_metadata82(parse_embedding_workbench_raw_log_json(artifact_text82(response.json,"prepared_log.json")).metadata,native_prepared.metadata);
  p[1]=field("source_format","raw_lazy");p[2]=file("source_file","{\"table\":"+raw_table+",\"valid\":true,\"row_count\":"+std::to_string(table.rows.size())+'}');response=okay(p);assert_metadata82(parse_embedding_workbench_table_json(member(member(member(member(response.json,"input_context"),"source"),"original_constructor"),"table")).metadata,table.metadata);
  EventTable dormant;dormant.metadata=private_metadata82(mode);p[2]=file("source_file","{\"table\":"+serialize_embedding_workbench_table_json(dormant)+",\"valid\":false,\"row_count\":null}");response=execute_discovery_workbench(p);assert(response.status==400&&member(response.json,"executed")=="false");assert(member(member(response.json,"error"),"message")==serialize_embedding_workbench_string_json(empty_scanner_error82(dormant.metadata)));
 }
 auto p=source("mine_ilp",log());p[1]=field("source_format","raw_lazy");p[2]=file("source_file",R"({"table":{"columns":[],"rows":[]},"valid":false,"row_count":null})");p.push_back(field("source_shape","log"));const auto invalid=execute_discovery_workbench(p);assert(invalid.status==400&&member(invalid.json,"executed")=="false");assert(member(member(invalid.json,"error"),"message")==serialize_embedding_workbench_string_json(default_lazy_conversion_error82()));
 const auto calls=member(member(member(invalid.json,"input_context"),"source"),"preparation_calls");assert(calls.size()>2);const auto end=end_value(calls,1);assert(end+1==calls.size()&&calls[end]==']');const auto call=std::string_view(calls).substr(1,end-1);assert(member(call,"name")==serialize_embedding_workbench_string_json("event_data_to_log(source,options)"));assert(member(call,"returned")=="false");assert(member(member(call,"error"),"phase")=="\"preparation_error\"");
}

}
int main(){shared_table_private_metadata82();five_public_entrypoints_and_defaults();exact_options_and_native_limits();source_shapes_and_real_imports();byte_results_and_independent_export_failures();physical_multipart_and_failures();std::cout<<"Original discovery workbench: all5 public calls, all19 options, full native results, exact sources and physical transport preserved\n";}
