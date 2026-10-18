#include "minera/edit_distance_workbench_json.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/log_conversion.hpp"
#include "minera/xes_export.hpp"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
using namespace minera;
namespace {
using Part=EditDistanceWorkbenchPart;
// Independent response-field slicing preserves exact numeric tokens and bytes.
// This helper does not parse/normalize the product's native constructor schema.
std::size_t end_value(std::string_view s,std::size_t start){
 if(s[start]=='"'){for(std::size_t i=start+1;i<s.size();++i){if(s[i]=='\\'){++i;continue;}if(s[i]=='"')return i+1;}assert(false);}
 if(s[start]=='['||s[start]=='{'){std::vector<char> stack{s[start]};bool quote=false;for(std::size_t i=start+1;i<s.size();++i){if(quote){if(s[i]=='\\')++i;else if(s[i]=='"')quote=false;continue;}if(s[i]=='"'){quote=true;continue;}if(s[i]=='['||s[i]=='{')stack.push_back(s[i]);else if(s[i]==']'||s[i]=='}'){assert(!stack.empty());stack.pop_back();if(stack.empty())return i+1;}}assert(false);}
 auto end=s.find_first_of(",}] \r\n\t",start);return end==std::string_view::npos?s.size():end;
}
std::string member(std::string_view s,std::string_view name){assert(s[0]=='{');std::size_t i=1;while(i<s.size()&&s[i]!='}'){const auto key_end=end_value(s,i);const auto key=s.substr(i+1,key_end-i-2);i=key_end;assert(s[i++]==':');const auto value_end=end_value(s,i);if(key==name)return std::string(s.substr(i,value_end-i));i=value_end;if(s[i]==',')++i;}assert(false);return {};}
std::string first(std::string_view array){assert(array[0]=='['&&array[1]!=']');const auto end=end_value(array,1);return std::string(array.substr(1,end-1));}
template<class F>void rejects(F make){bool caught=false;try{make();}catch(const std::invalid_argument&){caught=true;}assert(caught);}
Part field(std::string key,std::string value){return {std::move(key),"",std::move(value),false};}
Part file(std::string key,std::string value,std::string filename="exact.json"){return {std::move(key),std::move(filename),std::move(value),true};}
EventLog log(std::vector<std::string> words={"A","B"}){EventLog result;Trace trace;trace.case_id="case";for(std::size_t i=0;i<words.size();++i){Event e;e.case_id="case";e.activity=words[i];e.timestamp=i?"2024-01-01T01:00:00Z":"2024-01-01T00:00:00Z";e.input_order=i;trace.events.push_back(std::move(e));}result.traces.push_back(std::move(trace));return result;}
std::string raw(const EventLog& value){return serialize_embedding_workbench_raw_log_json(value);}
std::string trace(const Trace& value){EventLog holder;holder.traces.push_back(value);return first(member(raw(holder),"traces"));}
std::vector<Part> whole(std::string operation,const EventLog& source,const EventLog& reference){return {field("operation",std::move(operation)),field("source_format","raw_log"),field("reference_format","raw_log"),file("source_file",raw(source)),file("reference_file",raw(reference))};}
EditDistanceWorkbenchResponse okay(const std::vector<Part>& parts){auto result=execute_edit_distance_workbench(parts);if(result.status!=200)std::cerr<<result.json<<'\n';assert(result.status==200);assert(member(result.json,"executed")=="true"&&member(result.json,"returned")=="true");assert(member(result.json,"error")=="null");return result;}
void option_storage_and_transactionality(){
 std::set<std::string> supplied={"historical"};auto options=parse_edit_distance_workbench_options_json(R"({"variant":-2147483648,"perform_anti_alignment":true,"deviation_cost":18446744073709551615,"activity_attribute":"","timestamp_attribute":"  ","case_id_attribute":{"native_bytes":"hex-v1","data":"ff00eda080edb080"}})",&supplied);
 assert(static_cast<std::int32_t>(options.variant)==std::numeric_limits<std::int32_t>::min());assert(options.deviation_cost==std::numeric_limits<std::uint64_t>::max()&&options.activity_attribute.empty()&&options.timestamp_attribute=="  ");assert(supplied.size()==6);const auto round=parse_edit_distance_workbench_options_json(serialize_edit_distance_workbench_options_json(options));assert(round.case_id_attribute==options.case_id_attribute&&round.deviation_cost==options.deviation_cost);
 supplied={"historical"};for(const auto* bad:{R"({"deviation_cost":18446744073709551616})",R"({"deviation_cost":1.0})",R"({"deviation_cost":-1})",R"({"variant":2147483648})",R"({"variant":{"index":5,"name":"edit_distance"}})",R"({"activity_attribute":"A","activity_attribute":"B"})",R"({"unknown":0})"}){rejects([&]{(void)parse_edit_distance_workbench_options_json(bad,&supplied);});assert(supplied==std::set<std::string>{"historical"});}
 assert(parse_edit_distance_workbench_options_json("{}").deviation_cost==10000);
}
void full_result_constructor_roundtrips(){
 auto values=parse_edit_distance_workbench_alignments_json(R"([{"alignment":[{"type":-99,"log_activity":"","model_activity":"M","activity":"inactive","log":"strange","model":{"native_bytes":"hex-v1","data":"ff00"}}],"moves":[{"type":2,"activity":"different"}],"source_variant":[""," A ",""],"target_variant":["B"],"target_index":18446744073709551615,"cost":18446744073709551615,"fitness":"NaN","best_worst_cost":1,"bwc":2,"denominator":3}])");
 assert(values.size()==1&&values[0].alignment[0].type==static_cast<EditDistanceMoveType>(-99));assert(values[0].moves[0].activity=="different"&&std::isnan(values[0].fitness));const auto encoded=serialize_edit_distance_workbench_alignments_json(values);const auto round=parse_edit_distance_workbench_alignments_json(encoded);assert(round[0].alignment[0].model==std::string("\xff\0",2)&&round[0].bwc==2&&round[0].denominator==3&&round[0].target_index==std::numeric_limits<std::size_t>::max());
 const auto typed=parse_edit_distance_workbench_typed_alignments_json(R"([{"alignment":[{"type":0,"log_activity":null,"model_activity":{"key":"x","null":true}}],"moves":[{"type":-2147483648}],"fitness":-0.0,"source_variant":[{"key":"","value":"lexical","kind_index":2,"typed_value":{"index":2,"value":"NaN"}}],"target_variant":[]}])");
 assert(!typed[0].alignment[0].log_activity&&typed[0].alignment[0].model_activity&&typed[0].alignment[0].model_activity->is_null&&std::signbit(typed[0].fitness));const auto typed_round=parse_edit_distance_workbench_typed_alignments_json(serialize_edit_distance_workbench_alignments_json(typed));assert(typed_round[0].source_variant[0].value=="lexical"&&std::isnan(std::get<double>(typed_round[0].source_variant[0].typed_value))&&typed_round[0].moves[0].type==static_cast<EditDistanceMoveType>(std::numeric_limits<std::int32_t>::min()));
}
void all_whole_overloads_and_eleven_inline_aliases(){
 const auto source=log({"A","C"}),reference=log({"A","B"});
 for(const auto& operation:{"align","align_view"})for(const auto& alias:{"align_edit_distance","apply_edit_distance","apply"}){auto request=whole(operation,source,reference);request.push_back(field("function_alias",alias));const auto response=okay(request);const auto native=parse_edit_distance_workbench_alignments_json(member(response.json,"native_result"));assert(native.size()==1&&native[0].cost==20000&&native[0].alignment.size()==3);assert(serialize_edit_distance_workbench_alignments_json(native)==serialize_edit_distance_workbench_alignments_json(align_edit_distance(source,reference)));}
 for(const auto& operation:{"align_typed","align_typed_view"})for(const auto& alias:{"align_typed_edit_distance","apply_typed_edit_distance"}){auto request=whole(operation,source,reference);request.push_back(field("function_alias",alias));const auto response=okay(request);const auto native=parse_edit_distance_workbench_typed_alignments_json(member(response.json,"native_result"));assert(native.size()==1&&native[0].cost==20000);assert(serialize_edit_distance_workbench_alignments_json(native)==serialize_edit_distance_workbench_alignments_json(align_typed_edit_distance(source,reference)));}
 const auto trace_input=trace(source.traces[0]);for(const bool typed:{false,true}){const auto operation=typed?"trace_typed":"trace";const std::vector<std::string> aliases=typed?std::vector<std::string>{"apply_typed_edit_distance_trace","apply_typed_trace","align_typed_trace"}:std::vector<std::string>{"apply_edit_distance_trace","apply_trace","align_trace","align_edit_distance_trace"};const auto mapping=typed?R"([[{"value":"A"},"a"],[{"value":"C"},"c"],[{"value":"B"},"b"]])":R"([["A","a"],["C","c"],["B","b"]])";for(const auto& alias:aliases){const auto response=okay({field("operation",operation),field("function_alias",alias),file("trace_file",trace_input),file("direct_input_file",std::string("{\"list_encodings\":[\"ab\"],\"set_encodings\":[],\"mapping\":")+mapping+",\"cache\":[]}")});const auto handle=member(response.json,"native_result");assert(member(handle,"null_handle")=="false"&&member(handle,"has_value")=="true");assert(member(member(handle,"value"),"cost")=="20000");assert(member(handle,"type")==std::string("\"")+(typed?"typed_trace_result":"trace_result")+'"');}}
}
void all_projection_signatures_and_supplied_results(){
 const auto source=log({"A","C"});const std::vector<std::string> target={"A","B"};const auto reference=log(target);const auto alignments=align_edit_distance(source,reference);const auto typed=align_typed_edit_distance(source,reference);
 for(const auto& operation:{"project_variant","project_variant_view","project_typed_variant","project_typed_variant_view","project_alignments_default","project_alignments","project_typed_alignments"}){const std::string op=operation;std::vector<Part> request={field("operation",op),field("source_format","raw_log"),file("source_file",raw(source))};EventLog expected;
  if(op.find("variant")!=std::string::npos){const bool typed_variant=op.find("typed")!=std::string::npos;request.push_back(file("variant_file",typed_variant?R"([{"value":"A"},{"value":"B"}])":R"(["A","B"])"));if(typed_variant){std::vector<EventAttribute> values={{"","A"},{"","B"}};expected=project_log_on_typed_variant(source,values);}else expected=project_log_on_variant(source,target);}
  else if(op=="project_typed_alignments"){request.push_back(file("alignments_file",serialize_edit_distance_workbench_alignments_json(typed)));expected=project_log_on_typed_alignments(source,typed);}
  else{auto provided=alignments;provided[0].moves.clear();provided[0].fitness=-73;provided[0].bwc=18446744073709551615ULL;request.push_back(file("alignments_file",serialize_edit_distance_workbench_alignments_json(provided)));expected=project_log_on_alignments(source,provided);}
  const auto response=okay(request);assert(raw(parse_embedding_workbench_raw_log_json(member(response.json,"native_result")))==raw(expected));
 }
 auto invalid=std::vector<Part>{field("operation","project_alignments_default"),field("options_json","{}"),field("source_format","raw_log"),file("source_file",raw(source)),file("alignments_file",serialize_edit_distance_workbench_alignments_json(alignments))};const auto rejected=execute_edit_distance_workbench(invalid);assert(rejected.status==400&&member(rejected.json,"executed")=="false");
}
void finite_cache_payload_identity_and_actual_short_circuit(){
 const auto trace_input=trace(log({"A"}).traces[0]);
 for(const auto& payload:{R"({"id":"existing","type":null})",R"({"id":"existing","type":"empty_any"})",R"({"id":"existing","type":"string","value":{"native_bytes":"hex-v1","data":"ff00"}})",R"({"id":"existing","type":"int64","value":-9223372036854775808})",R"({"id":"existing","type":"uint64","value":18446744073709551615})",R"({"id":"existing","type":"double","value":"NaN"})",R"({"id":"existing","type":"bool","value":true})",R"({"id":"existing","type":"trace_result","value":{"cost":57,"alignment":[]}})",R"({"id":"existing","type":"typed_trace_result","value":{"cost":58,"alignment":[]}})"}){
  const auto input=std::string(R"({"list_encodings":[{"native_bytes":"hex-v1","data":"ff"}],"mapping":[["A","a"],["A","z"]],"set_encodings":["b","b","a"],"handles":[)")+payload+R"(],"cache":[{"encoding":"a","handle_ref":"existing"},{"encoding":"a","handle_ref":null}]})";
  const auto response=okay({field("operation","trace"),field("options_json",R"({"deviation_cost":0})"),file("trace_file",trace_input),file("direct_input_file",input)});const auto value=member(response.json,"native_result");const bool null=std::string(payload).find("\"type\":null")!=std::string::npos;assert(member(value,"null_handle")==std::string(null?"true":"false"));if(!null)assert(member(value,"id")=="\"existing\"");
 }
 const auto unknown=execute_edit_distance_workbench({field("operation","trace"),file("trace_file",trace_input),file("direct_input_file",R"({"handles":[{"id":"x","type":"opaque_fake_json","value":{}}]})")});assert(unknown.status==400&&member(unknown.json,"executed")=="false");
 const auto empty_cache=execute_edit_distance_workbench({field("operation","trace"),field("options_json",R"({"deviation_cost":0})"),file("trace_file",trace_input),file("direct_input_file",R"({"mapping":[["A","a"]],"list_encodings":["b"],"cache":[]})")});assert(empty_cache.status==400&&member(empty_cache.json,"executed")=="true");
}
void original_typed_nan_copy_identity(){
 auto source=log({"display"});EventAttribute activity;activity.key="concept:name";activity.value="NaN";activity.kind=AttributeKind::kFloat;activity.typed_value=std::numeric_limits<double>::quiet_NaN();source.traces[0].events[0].activity_attribute=activity;const auto reference=source;
 auto request=whole("align_typed",source,reference);const auto independent=okay(request);assert(parse_edit_distance_workbench_typed_alignments_json(member(independent.json,"native_result"))[0].cost==20000);
 request.push_back(field("construction_copies_json",R"([{"from":{"owner":"source","path":["traces",0,"events",0,"activity_attribute"]},"to":{"owner":"reference","path":["traces",0,"events",0,"activity_attribute"]},"member":"typed_value"}])"));const auto copied=okay(request);assert(parse_edit_distance_workbench_typed_alignments_json(member(copied.json,"native_result"))[0].cost==0);assert(member(member(copied.json,"identity_proof"),"steps")!="[]");
 request.back()=field("construction_copies_json",R"([{"from":{"owner":"source","path":["traces",0,"events",0,"activity_attribute"]},"to":{"owner":"reference","path":["traces",99,"events",0,"activity_attribute"]},"member":"attribute"}])");const auto invalid=execute_edit_distance_workbench(request);assert(invalid.status==400&&member(invalid.json,"executed")=="false");
}
void independent_mixed_original_shapes_and_lazy_boundaries(){
 const auto source=log(),reference=log();const auto table=event_log_to_table(source);const auto table_json=serialize_embedding_workbench_table_json(table);
 for(const auto& format:{"raw_table","raw_lazy","raw_stream"}){auto request=whole("align_typed_view",source,reference);request[1]=field("source_format",format);if(std::string(format)=="raw_table")request[3]=file("source_file",table_json);else if(std::string(format)=="raw_lazy")request[3]=file("source_file","{\"table\":"+table_json+",\"row_count\":999,\"valid\":true}");else{request[3]=file("source_file",R"({"attributes":[],"events":[{"fields":[{"key":"case:concept:name","value":"case"},{"key":"concept:name","value":"A"}]},{"fields":[{"key":"case:concept:name","value":"case"},{"key":"concept:name","value":"B"}]}],"metadata":{}})");}const auto response=okay(request);const auto result=parse_edit_distance_workbench_typed_alignments_json(member(response.json,"native_result"));assert(result.size()==1&&result[0].cost==0);const auto snapshot=member(member(response.json,"inputs_after_native"),"source");auto reopen=request;reopen[3]=file("source_file",snapshot);assert(member(okay(reopen).json,"native_result")==member(response.json,"native_result"));}
 auto invalid=whole("align_typed_view",source,reference);invalid[1]=field("source_format","raw_lazy");invalid[3]=file("source_file",R"({"table":{"columns":[],"rows":[]},"valid":false,"row_count":null})");const auto default_invalid=execute_edit_distance_workbench(invalid);assert(default_invalid.status==400&&member(default_invalid.json,"executed")=="true");invalid[3]=file("source_file",R"({"table":{"columns":["x"],"rows":[]},"valid":false,"row_count":null})");const auto nondefault_invalid=execute_edit_distance_workbench(invalid);assert(nondefault_invalid.status==400&&member(nondefault_invalid.json,"executed")=="false");
 auto wrong_signature=whole("align_typed",source,reference);wrong_signature[1]=field("source_format","raw_table");wrong_signature[3]=file("source_file",table_json);assert(member(execute_edit_distance_workbench(wrong_signature).json,"executed")=="false");wrong_signature.push_back(field("source_shape","log"));const auto prepared=okay(wrong_signature);assert(parse_edit_distance_workbench_typed_alignments_json(member(prepared.json,"native_result"))[0].cost==0);
}
void independent_importers_prefix_errors_and_original_bytes(){
 // Independent importer selectors never overwrite native edit options.
 // Both original inputs must actually supply the explicitly selected phase.
 auto reference=log({" A "});auto& reference_event=reference.traces[0].events[0];
 reference_event.activity_source_key="phase";
 reference_event.activity_state=FirstClassAttributeState::kPresent;
 reference_event.activity_attribute=EventAttribute{"phase"," A ",AttributeKind::kString};
 const auto source=log();auto request=whole("align_typed_view",source,reference);
 request[1]=field("source_format","csv");
 request[3]=file("source_file","case;phase\nc; A \n","original.csv");
 request.push_back(field("source_import_options_json",R"({"case_id_column":"case","activity_column":"phase","delimiter":";","trim_fields":false})"));
 const auto default_selector=execute_edit_distance_workbench(request);
 assert(default_selector.status==400&&member(default_selector.json,"executed")=="true");
 assert(member(member(default_selector.json,"configured_options"),"activity_attribute")=="\"concept:name\"");
 assert(member(member(default_selector.json,"error"),"exception_type")=="\"out_of_range\"");
 request.push_back(field("options_json",R"({"activity_attribute":"phase"})"));
 const auto imported=okay(request);
 assert(member(member(imported.json,"configured_options"),"activity_attribute")=="\"phase\"");
 assert(parse_edit_distance_workbench_typed_alignments_json(member(imported.json,"native_result"))[0].cost==0);
 request.push_back(field("reference_import_options_json","{"));const auto raw_inapplicable=execute_edit_distance_workbench(request);assert(raw_inapplicable.status==400&&member(raw_inapplicable.json,"executed")=="false");const auto prefix=member(raw_inapplicable.json,"parsed_controls");assert(prefix.find("source_import_options_json")!=std::string::npos&&prefix.find("source_file")!=std::string::npos&&prefix.find("reference_import_options_json")==std::string::npos);
 auto malformed=whole("align_typed",source,reference);malformed.push_back(field("options_json",R"({"activity_attribute":"A","deviation_cost":false})"));const auto rejected=execute_edit_distance_workbench(malformed);assert(rejected.status==400&&member(rejected.json,"parsed_controls").find("options_json")==std::string::npos);assert(member(rejected.json,"originals").find("sha256")!=std::string::npos);
 for(int variant=0;variant<6;++variant){auto xes=whole("align_typed_view",source,source);xes[2]=field("reference_format","xes");xes[4]=file("reference_file",serialize_xes(source),"original.xes");xes.push_back(field("reference_import_options_json","{\"variant\":"+std::to_string(variant)+'}'));const auto response=okay(xes);assert(parse_edit_distance_workbench_typed_alignments_json(member(response.json,"native_result"))[0].cost==0);}
}
void ambiguous_unknown_and_ordered_copy_errors(){
 auto request=whole("align_typed",log(),log());request.push_back(field("operation","align"));auto result=execute_edit_distance_workbench(request);assert(result.status==400&&member(result.json,"originals").find("operation")!=std::string::npos);
 request=whole("align_typed",log(),log());request.push_back(file("variant_file","[]"));result=execute_edit_distance_workbench(request);assert(result.status==400&&member(result.json,"executed")=="false");
 request=whole("align_typed",log(),log());request.push_back(field("construction_copies_json",R"([{"from":{"owner":"source","path":["traces",0,"events",0,"activity_attribute"]},"to":{"owner":"reference","path":["traces",0,"events",0,"activity_attribute"]},"member":"attribute"}])"));result=execute_edit_distance_workbench(request);assert(result.status==400&&member(result.json,"executed")=="false");
}
void multipart_filename_presence_order_and_binary_bytes(){
 const std::string type="multipart/form-data; boundary=\"Owned69Boundary\"";
 const std::string body=std::string("--Owned69Boundary\r\nContent-Disposition: form-data; name=\"operation\"\r\n\r\ntrace\r\n--Owned69Boundary\r\nContent-Disposition: form-data; name=\"options_json\"; filename=\"\"\r\n\r\n{}\r\n--Owned69Boundary\r\nContent-Disposition: form-data; name=\"trace_file\"; filename=\"a; %22%0D%0A.json\"\r\n\r\n")+std::string("\xff\0",2)+"\r\n--Owned69Boundary--\r\n";
 const auto parts=parse_edit_distance_workbench_multipart(type,body);assert(parts.size()==3);assert(parts[0].name=="operation"&&!parts[0].file&&parts[0].filename.empty()&&parts[0].bytes=="trace");assert(parts[1].file&&parts[1].filename.empty()&&parts[1].bytes=="{}");assert(parts[2].file&&parts[2].filename=="a; %22%0D%0A.json"&&parts[2].bytes==std::string("\xff\0",2));
 const auto extended=parse_edit_distance_workbench_multipart(type,"--Owned69Boundary\r\nContent-Disposition: form-data; name=\"x\"; filename=\"fallback\"; filename*=UTF-8''a%20b%F0%9F%98%80\r\n\r\nz\r\n--Owned69Boundary--\r\n");assert(extended.size()==1&&extended[0].file&&extended[0].filename=="a b\xf0\x9f\x98\x80");
 const auto trace_input=trace(log({"A"}).traces[0]);const auto direct=R"({"list_encodings":["a"],"set_encodings":["a"],"mapping":[["A","a"]]})";
 const auto request=[&](bool filename){return std::string("--Owned69Boundary\r\nContent-Disposition: form-data; name=\"operation\"\r\n\r\ntrace\r\n--Owned69Boundary\r\nContent-Disposition: form-data; name=\"trace_file\"")+(filename?"; filename=\"\"":"")+"\r\n\r\n"+trace_input+"\r\n--Owned69Boundary\r\nContent-Disposition: form-data; name=\"direct_input_file\"; filename=\"\"\r\n\r\n"+direct+"\r\n--Owned69Boundary--\r\n";};
 const auto accepted=execute_edit_distance_workbench_multipart(type,request(true));assert(accepted.status==200&&member(accepted.json,"returned")=="true");const auto absent=execute_edit_distance_workbench_multipart(type,request(false));assert(absent.status==400&&member(absent.json,"executed")=="false");
 const auto ambiguous=execute_edit_distance_workbench_multipart(type,request(true),{field("operation","trace")});assert(ambiguous.status==400&&member(ambiguous.json,"executed")=="false");
 const auto incomplete=execute_edit_distance_workbench_multipart(type,request(true),{},false);assert(incomplete.status==400&&member(incomplete.json,"executed")=="false"&&member(member(incomplete.json,"error"),"phase")=="\"transport_error\"");assert(member(incomplete.json,"transport_original").find("sha256")!=std::string::npos);
 const auto malformed=execute_edit_distance_workbench_multipart(type,"--Owned69Boundary\r\ninvalid");assert(malformed.status==400&&member(member(malformed.json,"error"),"phase")=="\"transport_error\"");
 rejects([&]{(void)parse_edit_distance_workbench_multipart(type,"--Wrong--\r\n");});rejects([&]{(void)parse_edit_distance_workbench_multipart("multipart/form-data; boundary=a; boundary=b","--a--\r\n");});
}

}
int main(){multipart_filename_presence_order_and_binary_bytes();option_storage_and_transactionality();full_result_constructor_roundtrips();all_whole_overloads_and_eleven_inline_aliases();all_projection_signatures_and_supplied_results();finite_cache_payload_identity_and_actual_short_circuit();original_typed_nan_copy_identity();independent_mixed_original_shapes_and_lazy_boundaries();independent_importers_prefix_errors_and_original_bytes();ambiguous_unknown_and_ordered_copy_errors();std::cout<<"edit-distance workbench native contract sections passed\n";}
