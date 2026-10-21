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
inline void workflows(const std::function<DiscoveryWorkbenchResponse(const std::vector<DiscoveryWorkbenchPart>&)>& run){
  const auto source=source_log();const auto raw=serialize_embedding_workbench_raw_log_json(source);
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
