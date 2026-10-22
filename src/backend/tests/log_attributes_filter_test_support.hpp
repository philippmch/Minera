#pragma once
#include "table_attribute_repetition_test_support.hpp"
#include "minera/log_attributes_filter_workbench.hpp"
#include "minera/log_attributes_filter.hpp"
#include "minera/xes_event_log_importer.hpp"
#include <functional>
#include <sstream>
namespace log_attributes_test {
using namespace table_repetition_test;
inline EventLog source_log(){
  EventLog log;log.attributes={{"opaque",std::string("\xff\0",2)}};log.metadata.properties={{"retained","log property"}};
  for(std::size_t i=0;i<3;++i){
    Trace trace;trace.case_id="c"+std::to_string(i);trace.case_id_source_key="concept:name";trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_attribute=EventAttribute{"concept:name",trace.case_id};
    trace.attributes={{"concept:name",trace.case_id},{"channel",i==0?"web":"mail"}};trace.properties={{"retained","trace property"}};
    for(const auto& activity:i==0?std::vector<std::string>{"A","D"}:i==1?std::vector<std::string>{"B"}:std::vector<std::string>{"A"}){
      Event event;event.activity=activity;event.activity_source_key="work";event.activity_state=FirstClassAttributeState::kPresent;event.activity_attribute=EventAttribute{"work",activity};event.input_order=10+i;
      event.attributes={{"cost",i==0?"9007199254740993":"9007199254740992",AttributeKind::kInt},{"stream",i==0?"web":"mail"}};trace.events.push_back(event);
    }log.traces.push_back(trace);
  }return log;
}
inline std::vector<DiscoveryWorkbenchPart> request_log(const EventLog& log,std::string operation,std::vector<DiscoveryWorkbenchPart> controls={}){
  std::vector<DiscoveryWorkbenchPart> parts={field("operation",operation),{"source_file","source.json",serialize_embedding_workbench_raw_log_json(log),true}};
  parts.insert(parts.end(),controls.begin(),controls.end());return parts;
}
inline EventLog output(const DiscoveryWorkbenchResponse& response){
  require(response.status==200,"log attributes failed: "+response.json);contains(response,"\"phase\":\"complete\"");contains(response,"\"family\":\"LOG_ATTRIBUTES_FILTER\"");
  const auto raw=text(response.json,"filtered_log_json");require(raw.has_value(),"missing full filtered log");
  const auto parsed=parse_embedding_workbench_raw_log_json(*raw);require(serialize_embedding_workbench_raw_log_json(parsed)==*raw,"full output lost on reopen");return parsed;
}
inline void original_workflows(const std::function<DiscoveryWorkbenchResponse(const std::vector<DiscoveryWorkbenchPart>&)>& run){
  const auto source=source_log();const auto raw=serialize_embedding_workbench_raw_log_json(source);
  const auto call=[](const std::string& args,const std::string& tail=std::string{}){return "{\"schema\":\"minera-log-attributes-original-call-v1\",\"args\":"+args+tail+'}';};
  const auto invoke=[&](const std::string& operation,const std::string& json,std::vector<DiscoveryWorkbenchPart> extra=std::vector<DiscoveryWorkbenchPart>{}){
    auto parts=request_log(source,operation,{field("call_json",json)});parts.insert(parts.end(),extra.begin(),extra.end());return run(parts);
  };
  const std::string values=R"([{"type":"list","value":[{"type":"string","value":"A"}]}])";
  const std::string params=R"(,"parameter_entries":[{"key":"pm4py:param:attribute_key","enum_key":true,"value":{"type":"string","value":"work"}}])";
  const std::string exact="\r\n "+call(values,params)+"\r\n";
  auto bytes=request_log(source,"traces",{{"call_json","call_json.txt",exact,true}});
  bytes[1].filename="misleading.csv";
  const auto positive=run(bytes);require(output(positive).traces.size()==2,"original enum selector failed");
  require(text(positive.json,"call_json")==exact,"original call text changed");contains(positive,"\"call_mode\":\"original\"");contains(positive,"\"source_format\":\"native_log\"");
  contains(positive,"\"filter_attribute_key\":null");require(text(positive.json,"activity_key")=="work","opening activity role replaced by typed selector");
  require(text(positive.json,"parameters_before_json")==text(positive.json,"parameters_after_json"),"unchanged caller parameters differ");
  contains(positive,"\"threshold_cache_before_json\":null");contains(positive,"\"threshold_cache_after_json\":null");
  const auto omitted=invoke("traces",call(values));require(omitted.status==200,"omitted parameters failed");contains(omitted,"\"parameters_before_json\":null");contains(omitted,"\"parameters_after_json\":null");contains(omitted,"\"parameters_unchanged\":null");
  const auto empty=invoke("traces",call(values,R"(,"parameter_entries":[])"));require(empty.status==200,"empty dictionary failed");require(text(empty.json,"parameters_before_json").has_value(),"empty dictionary collapsed to omission");contains(empty,"\"parameters_unchanged\":true");
  const auto na=invoke("traces",call(values,R"(,"parameter_value":{"type":"na"})"));require(na.status==400,"raw NA parameters accepted");contains(na,"\"phase\":\"apply\"");contains(na,"\"error_type\":\"TypeError\"");contains(na,"\"source_unchanged\":true");
  require(text(na.json,"parameters_before_json").has_value()&&text(na.json,"parameters_before_json")==text(na.json,"parameters_after_json"),"raw parameters lost complete native artifact");
  const auto maximum=invoke("top_activities",call(R"([{"type":"null"}])",R"(,"parameter_entries":[{"key":"pm4py:param:activity_key","enum_key":false,"value":{"type":"string","value":"work"}}])"));
  require(maximum.status==400,"invalid original maximum accepted");contains(maximum,"\"phase\":\"apply\"");contains(maximum,"\"source_unchanged\":true");contains(maximum,"\"parameters_unchanged\":false");
  require(text(maximum.json,"parameters_before_json")!=text(maximum.json,"parameters_after_json"),"maximum pre-error dictionary write hidden");
  const auto full=invoke("top_activities",call(R"([{"type":"integer","value":"100"}])",R"(,"parameter_entries":[])"));
  require(serialize_embedding_workbench_raw_log_json(output(full))==raw,"full maximum altered source");contains(full,"\"source_unchanged\":true");contains(full,"\"parameters_unchanged\":false");
  require(invoke("top_activities",call("[]")).status==200,"omitted maximum default failed");
  const std::string key=R"({"type":"tuple","value":[{"type":"string","value":"CACHE"}]})";
  const std::string count=R"({"type":"attribute_counts","entries":[[{"type":"string","value":"A"},{"type":"integer","value":"0"}],[{"type":"string","value":"B"},{"type":"integer","value":"0"}],[{"type":"string","value":"D"},{"type":"integer","value":"0"}],[{"type":"null"},{"type":"integer","value":"9007199254740993"}]]})";
  const std::string vc="{\"type\":\"list\",\"value\":[{\"type\":\"tuple\",\"value\":["+key+",{\"type\":\"integer\",\"value\":\"1\"}]}]}";
  const auto cache=[&](const std::string& handle){return "{\"type\":\"variant_cache\",\"entries\":[{\"key\":"+key+",\"traces\":["+handle+"]}]}";};
  const auto threshold=[&](const std::string& c,const std::string& v,const std::string& sequence){return call("["+c+","+v+","+sequence+R"(,{"type":"integer","value":"100"},{"type":"string","value":"work"}])");};
  const auto source_cache=cache(R"({"source_trace_index":0})");
  const auto retained=invoke("threshold",threshold(count,source_cache,vc));require(output(retained).traces.size()==2,"actual source trace cache exemption failed");
  const auto before_cache=text(retained.json,"threshold_cache_before_json");require(before_cache&&before_cache==text(retained.json,"threshold_cache_after_json"),"actual complete threshold cache after differs");
  require(before_cache->find("ordinary_value_json")!=std::string::npos&&before_cache->find("key_json")!=std::string::npos&&before_cache->find("traces_json")!=std::string::npos,"cache entry boundaries omitted");
  require(text(retained.json,"threshold_counts_json")==count&&text(retained.json,"threshold_variants_map_json")==source_cache&&text(retained.json,"variant_counts_json")==vc,"raw cache descriptor slices changed");
  EventLog auxiliary;auxiliary.traces={source.traces[0]};auxiliary.metadata.properties={{"aux","independent"}};
  const auto auxiliary_cache=cache(R"({"auxiliary_trace_index":0})");
  const auto aux=invoke("threshold",threshold(count,auxiliary_cache,vc),{{"variants_file","auxiliary.csv",serialize_embedding_workbench_raw_log_json(auxiliary),true}});
  require(output(aux).traces.size()==2,"external full trace cache failed");require(text(aux.json,"variants_source_log_json")==serialize_embedding_workbench_raw_log_json(auxiliary),"auxiliary full context lost");
  require(text(aux.json,"threshold_cache_before_json")==before_cache,"equal actual source/aux copied trace values produced different cache identity");
  const auto bad_vc=invoke("threshold",threshold(count,source_cache,R"({"type":"list","value":[]})"));
  require(bad_vc.status==400,"empty original vc accepted");contains(bad_vc,"\"phase\":\"apply\"");contains(bad_vc,"\"error_type\":\"IndexError\"");
  require(text(bad_vc.json,"threshold_cache_before_json").has_value()&&text(bad_vc.json,"threshold_cache_before_json")==text(bad_vc.json,"threshold_cache_after_json"),"complete cache lost on native error");
  const auto raw_variants=invoke("threshold",threshold(count,R"({"type":"null"})",R"({"type":"list","value":[]})"));
  require(raw_variants.status==400,"malformed original variants accepted");contains(raw_variants,"\"phase\":\"apply\"");require(text(raw_variants.json,"threshold_cache_before_json").has_value(),"raw cache union not snapshotted");
  for(const auto& json:std::vector<std::string>{"",R"({"schema":"bad","args":[]})",R"({"schema":"minera-log-attributes-original-call-v1","args":[],"args":[]})",call("[]"),call(values,R"(,"parameter_value":{"type":"null"},"parameter_entries":[])"),call(values,R"(,"parameter_entries":[{"key":"unknown","enum_key":false,"value":{"type":"null"}}])"),call(values,R"(,"parameter_entries":[{"key":"positive","enum_key":false,"value":{"type":"boolean","value":true}},{"key":"positive","enum_key":true,"value":{"type":"boolean","value":false}}])")}) {
    const auto bad=invoke("traces",json);require(bad.status==400,"malformed original call accepted");contains(bad,"\"phase\":\"control\"");require(text(bad.json,"call_json")==json,"control-error original echo lost");
  }
  for(const auto& extra:std::vector<DiscoveryWorkbenchPart>{field("source_format","native_log"),field("positive","true"),field("attribute_key","work"),field("values_json","[]")}) {
    const auto bad=invoke("traces",exact,{extra});require(bad.status==400,"mixed friendly/original accepted");require(text(bad.json,"call_json")==exact,"mixed control original echo lost");
  }
  const auto missing_aux=invoke("threshold",threshold(count,auxiliary_cache,vc));require(missing_aux.status==400,"missing auxiliary accepted");contains(missing_aux,"\"phase\":\"control\"");
  const auto range=invoke("threshold",threshold(count,cache(R"({"source_trace_index":999})"),vc));require(range.status==400,"out-of-range source handle accepted");require(text(range.json,"source_log_json")==raw,"range error source snapshot lost");
  for(const auto& handle:std::vector<std::string>{R"({"source_trace_index":true})",R"({"source_trace_index":"0"})",R"({"source_trace_index":0.0})",R"({"source_trace_index":0,"auxiliary_trace_index":0})"})require(invoke("threshold",threshold(count,cache(handle),vc)).status==400,"nonexact trace handle accepted");
  auto wrong_file=bytes;wrong_file[2].filename="wrong.json";const auto rejected=run(wrong_file);require(rejected.status==400,"noncanonical call File accepted");contains(rejected,"\"call_mode\":null");contains(rejected,"\"call_json\":null");
  const auto unknown=invoke("traces",exact,{field("unknown","x")});require(unknown.status==400,"unknown original control accepted");contains(unknown,"\"call_mode\":null");contains(unknown,"\"call_json\":null");
}
inline void workflows(const std::function<DiscoveryWorkbenchResponse(const std::vector<DiscoveryWorkbenchPart>&)>& run){
  original_workflows(run);
  const auto source=source_log();const auto raw=serialize_embedding_workbench_raw_log_json(source);
  const std::string byte_literal(" literal\n\0Ω ",13);auto byte_source=source;
  for(auto& trace:byte_source.traces)for(auto& event:trace.events){event.activity_source_key=byte_literal;event.activity_attribute->key=byte_literal;}
  auto byte_request=request_log(byte_source,"traces",{{"attribute_key","attribute_key.txt",byte_literal,true},field("values_json","[\"A\"]")});
  require(output(run(byte_request)).traces.size()==2,"canonical byte log attribute role rejected");
  byte_request[2].filename="unrelated.json";require(run(byte_request).status==400,"unrelated log attributes File accepted as a byte field");
  for(const auto operation:{"traces","events"}){
    const auto answer=run(request_log(source,operation,{field("attribute_key","work"),field("values_json","[\"A\"]")}));const auto filtered=output(answer);
    require(filtered.traces.size()==2,"categorical case selection differs");require(text(answer.json,"source_log_json")==raw,"full source changed before apply");
    contains(answer,operation==std::string("traces")?"\"source_unchanged\":true":"\"source_unchanged\":false");
    require(filtered.traces[0].events.size()==(operation==std::string("traces")?2U:1U),"event/case selection confused");
    contains(answer,"\"activity_key\":\"work\"");require(text(answer.json,"source_after_log_json").has_value(),"missing source after");
    contains(answer,"\"parameters_unchanged\":true");
  }
  const auto negative=output(run(request_log(source,"traces",{field("attribute_key","work"),field("values_json","[\"A\"]"),field("positive","false")})));
  require(negative.traces.size()==1 && negative.traces[0].case_id=="c1","negative case selection differs");
  const auto trace_values=output(run(request_log(source,"trace_values",{field("attribute_key","channel"),field("values_json","[\"web\"]")})));
  require(trace_values.traces.size()==1 && trace_values.traces[0].events.size()==2,"trace value filter lost complete case");
  for(const auto operation:{"numeric_traces","numeric_events"}){
    const auto answer=run(request_log(source,operation,{field("attribute_key","cost"),field("lower","9007199254740993"),field("upper","9007199254740993")}));
    require(output(answer).traces.size()==1,"exact integer interval rounded");contains(answer,"\"source_unchanged\":false");
  }
  const auto streams=output(run(request_log(source,"numeric_traces",{field("attribute_key","cost"),field("lower","9007199254740992"),field("upper","9007199254740993"),field("stream_filter_key1","stream"),field("stream_filter_value1_json",R"({"type":"string","value":"web"})"),field("stream_filter_key2","work"),field("stream_filter_value2_json",R"({"type":"string","value":"D"})")})));
  require(streams.traces.size()==1 && streams.traces[0].case_id=="c0","AND stream controls ignored");
  const auto maximum=run(request_log(source,"top_activities",{field("activity_key","work"),field("criterion","1")}));
  require(output(maximum).traces.size()==2,"maximum activity selection differs");contains(maximum,"\"parameters_unchanged\":false");contains(maximum,"\"source_unchanged\":false");
  const auto malformed_max=run(request_log(source,"top_activities",{field("activity_key","work"),field("criterion","null")}));
  require(malformed_max.status==400,"invalid max accepted");contains(malformed_max,"\"parameters_unchanged\":false");contains(malformed_max,"\"source_unchanged\":true");contains(malformed_max,"\"error_type\":\"TypeError\"");
  for(const auto once:{"true","false"}){
    const auto relative=run(request_log(source,"relative",{field("attribute_key","work"),field("criterion","0.6"),field("keep_once_per_case",once)}));
    const auto selected=output(relative);require(selected.traces.size()==(once==std::string("true")?2U:0U),"relative denominator ignored");
  }
  // Actual auxiliary trace content A/D is independent of ('CACHE',) key.
  EventLog auxiliary=source;auxiliary.traces={source.traces[0]};
  const std::string cache_key=R"({"type":"tuple","value":[{"type":"string","value":"CACHE"}]})";
  const std::string counts=R"({"type":"list","value":[{"type":"tuple","value":[{"type":"string","value":"A"},{"type":"integer","value":"0"}]},{"type":"tuple","value":[{"type":"string","value":"B"},{"type":"integer","value":"0"}]},{"type":"tuple","value":[{"type":"string","value":"D"},{"type":"integer","value":"0"}]}]})";
  const std::string mapping="{\"type\":\"list\",\"value\":[{\"type\":\"tuple\",\"value\":["+cache_key+",{\"type\":\"list\",\"value\":[{\"type\":\"integer\",\"value\":\"0\"}]}]}]}";
  const std::string vc="{\"type\":\"list\",\"value\":[{\"type\":\"tuple\",\"value\":["+cache_key+",{\"type\":\"integer\",\"value\":\"1\"}]}]}";
  auto threshold=request_log(source,"threshold",{field("attribute_key","work"),field("criterion","100"),field("counts_json",counts),{"variants_file","cache.json",serialize_embedding_workbench_raw_log_json(auxiliary),true},field("variant_traces_json",mapping),field("variant_counts_json",vc)});
  const auto cached=run(threshold);const auto selected=output(cached);require(selected.traces.size()==2 && selected.traces[0].events.size()==2,"actual cached trace exemption lost");
  contains(cached,"\"source_unchanged\":true");contains(cached,"\"variants_source_unchanged\":true");
  require(text(cached.json,"variants_source_log_json")==serialize_embedding_workbench_raw_log_json(auxiliary),"auxiliary log context lost");
  require(text(cached.json,"threshold_variants_map_json")==mapping && text(cached.json,"threshold_counts_json")==counts && text(cached.json,"variant_counts_json")==vc,"exact cache controls lost");
  for(auto& part:threshold)if(part.name=="variant_counts_json")part.bytes=R"({"type":"list","value":[]})";
  const auto empty_vc=run(threshold);require(empty_vc.status==400,"empty vc accepted");contains(empty_vc,"\"error_type\":\"IndexError\"");contains(empty_vc,"\"variants_source_unchanged\":true");
  auto missing=source;for(auto& trace:missing.traces){trace.case_id_state=FirstClassAttributeState::kMissing;trace.case_id_attribute.reset();trace.attributes={{"alternate",trace.case_id}};trace.case_id.clear();}
  const auto missing_case=run(request_log(missing,"events",{field("attribute_key","work"),field("values_json","[\"A\"]")}));
  require(missing_case.status==400,"missing canonical case accepted");contains(missing_case,"\"source_unchanged\":false");contains(missing_case,"\"error_type\":\"KeyError\"");
  require(output(run(request_log(missing,"events",{field("attribute_key","work"),field("values_json","[\"A\"]"),field("case_id_key","case:alternate")}))).traces.size()==2,"full custom case selector not forwarded");
  auto literal=source;const std::string key(" \n\0Ω",5);for(auto& trace:literal.traces)for(auto& event:trace.events)event.attributes.push_back({key,"literal"});
  require(output(run(request_log(literal,"traces",{field("attribute_key",key),field("values_json","[\"literal\"]")}))).traces.size()==3,"literal NUL selector lost");
  const auto csv=run({field("operation","events"),field("values_json","[\"A\"]"),{"source_file","source.csv","case_id,activity\nc1,A\nc1,B\nc2,C\n",true}});
  require(output(csv).traces.size()==1,"CSV converter identity missing");contains(csv,"\"activity_key\":\"activity\"");
  const auto empty_name=run({field("operation","traces"),field("attribute_key","work"),field("values_json","[\"A\"]"),{"source_file","",raw,true}});require(output(empty_name).traces.size()==2,"empty File name rejected");
  for(const auto& extra:std::vector<DiscoveryWorkbenchPart>{field("lower","0"),field("keep_once_per_case","true"),field("case_id_glue","x"),field("unknown","x"),field("operation","events")}){
    auto invalid=request_log(source,"traces",{field("attribute_key","work"),field("values_json","[]"),extra});require(run(invalid).status==400,"inactive/duplicate control accepted: "+extra.name);
  }
  auto fake=request_log(source,"traces",{field("values_json","[]")});for(auto& part:fake)if(part.name=="source_file")part.file=false;require(run(fake).status==400,"ordinary source field accepted");
}
}
