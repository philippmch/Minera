#include "minera/reference_log_filter_workbench.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/log_attribute_repetition_controls.hpp"
#include "minera/literal_controls_json.hpp"
#include "minera/xes_event_log_importer.hpp"
#include <cmath>
#include <stdexcept>
#include <cassert>
#include <sstream>

namespace {
using namespace minera;
const std::string csv="case_id,activity\nc1,A\nc1,B\nc1,A\nc2,B\nc3,\"A,B\"\n";
DiscoveryWorkbenchResponse run(std::string operation,std::vector<DiscoveryWorkbenchPart> controls={},std::string bytes=csv,std::string name="source.csv") {
  std::vector<DiscoveryWorkbenchPart> parts={{"operation","",std::move(operation),false},{"source_file",std::move(name),std::move(bytes),true}};
  parts.insert(parts.end(),controls.begin(),controls.end());return execute_reference_log_filter(parts);
}
DiscoveryWorkbenchPart field(std::string name,std::string value) { return {std::move(name),"",std::move(value),false}; }
void has(const DiscoveryWorkbenchResponse& response,const std::string& text) { assert(response.json.find(text)!=std::string::npos); }
void csv_filter_workflow() {
  for(const auto& operation:{"start","end"}) {
    const auto result=run(operation,{field("admitted_json","[\"A\"]")});assert(result.status==200);
    has(result,"\"source\":{\"traces\":3,\"events\":5}");has(result,"\"result\":{\"traces\":1,\"events\":3}");
    has(result,"\"activity_key\":\"activity\"");has(result,"\"source_unchanged\":true");has(result,"\"xes_error\":null");
  }
  auto result=run("prefix",{field("activity","A")});assert(result.status==200);has(result,"\"result\":{\"traces\":1,\"events\":0}");
  result=run("prefix",{field("activity","A"),field("occurrence","last")});assert(result.status==200);has(result,"\"result\":{\"traces\":1,\"events\":2}");
  result=run("suffix",{field("activity","A")});assert(result.status==200);has(result,"\"result\":{\"traces\":1,\"events\":2}");
  result=run("rework",{field("activity","A"),field("min_occurrences","2")});assert(result.status==200);has(result,"\"result\":{\"traces\":1,\"events\":3}");
  result=run("traces",{field("patterns_json","[[\"A\",\"B\"]]")});assert(result.status==200);has(result,"\"result\":{\"traces\":1,\"events\":1}");
  std::istringstream input(csv);auto original=import_csv_event_log(input);auto expected=original;
  expected.traces={original.traces[2]};
  has(result,"\"filtered_log_json\":"+serialize_embedding_workbench_string_json(serialize_embedding_workbench_raw_log_json(expected)));
  result=run("traces",{field("patterns_json","[]"),field("positive","false")});assert(result.status==200);has(result,"\"result\":{\"traces\":0,\"events\":0}");
  result=run("traces",{field("patterns_json","[[\"A\",\"...\",\"A\"]]")});assert(result.status==200);has(result,"\"result\":{\"traces\":1,\"events\":3}");
  result=run("rework",{field("activity","A"),field("min_occurrences",std::string(400,'9'))});assert(result.status==200);has(result,"\"result\":{\"traces\":0,\"events\":0}");
  result=run("rework",{field("activity","A"),field("min_occurrences","-"+std::string(400,'9'))});assert(result.status==200);has(result,"\"result\":{\"traces\":1,\"events\":3}");
  result=run("start",{field("admitted_json","[\" A \"]"),field("case_id_column","case"),field("activity_column","work"),field("delimiter",";")},"case;work\nc; A \n","custom.csv");
  assert(result.status==200);has(result,"\"activity_key\":\"work\"");has(result,"\"result\":{\"traces\":1,\"events\":1}");
}
void native_literal_and_export_workflow() {
  EventLog log;log.attributes={{"kept","original"}};
  Trace trace;trace.case_id="case";trace.attributes={{"literal","context"}};
  Event event;event.activity_state=FirstClassAttributeState::kPresent;event.attributes={{" work\n"," A "}};
  trace.events.push_back(event);log.traces.push_back(trace);
  auto raw=serialize_embedding_workbench_raw_log_json(log);
  auto result=run("prefix",{field("activity",""),field("strict","false")},raw,"original.json");assert(result.status==200);
  has(result,"\"filtered_log_json\":"+serialize_embedding_workbench_string_json(raw));
  result=run("rework",{field("activity"," A "),field("activity_key"," work\n"),field("min_occurrences","1")},raw,"original.json");
  assert(result.status==200);has(result,"\"filtered_log_json\":"+serialize_embedding_workbench_string_json(raw));
  result=run("start",{field("admitted_json","[\"\"]"),field("activity_key","missing")},raw,"original.json");
  assert(result.status==400);has(result,"\"phase\":\"filter\"");has(result,"\"source_unchanged\":true");
  log.traces[0].events[0].attributes.push_back({"invalid-date","not-a-date",AttributeKind::kDate});
  result=run("traces",{field("patterns_json","[]")},serialize_embedding_workbench_raw_log_json(log),"original.json");
  assert(result.status==200);has(result,"\"error\":null");has(result,"\"filtered_xes\":null");assert(result.json.find("\"xes_error\":null")==std::string::npos);
}
void repetition_workflow_and_typed_controls() {
  const std::string selected="case_id,activity,channel\nc1,A,web\nc1,B,web\nc2,C,mail\n";
  const auto target=field("target_json",R"({"type":"string","value":"web"})");
  auto result=run("repetition",{target,field("activity_key","channel")},selected);
  assert(result.status==200);has(result,"\"result\":{\"traces\":1,\"events\":2}");
  has(result,"\"filter_attribute_key\":\"channel\"");has(result,"\"activity_key\":\"activity\"");
  has(result,"\"source_unchanged\":true");
  result=run("repetition",{target,field("activity_key","channel"),field("min_rep","0"),field("max_rep","0")},selected);
  assert(result.status==200);has(result,"\"result\":{\"traces\":1,\"events\":1}");
  result=run("repetition",{target,field("activity_key","channel"),field("min_rep","3"),field("max_rep","2")},selected);
  assert(result.status==200);has(result,"\"result\":{\"traces\":0,\"events\":0}");
  result=run("repetition",{target,field("activity_key","channel"),field("min_rep","3"),field("max_rep","null")},selected);
  assert(result.status==200);has(result,"\"result\":{\"traces\":0,\"events\":0}");
  result=run("repetition",{target,field("activity_key","channel"),field("min_rep","null")},selected);
  assert(result.status==400);has(result,"\"phase\":\"filter\"");has(result,"\"source_unchanged\":true");
  EventLog log;Trace trace;Event event;event.activity="A";event.activity_source_key="work";
  event.activity_attribute=EventAttribute{"work","A"};event.activity_state=FirstClassAttributeState::kPresent;
  auto value=parse_log_attribute_repetition_target_json(R"({"type":"tuple","value":[{"type":"integer","value":"9007199254740993"},{"type":"null"}]})");
  value.key=" selected\n";event.attributes={value};trace.events={event,event};log.traces={trace};
  const auto raw=serialize_embedding_workbench_raw_log_json(log);
  result=run("repetition",{field("activity_key",value.key),field("target_json",R"({"type":"tuple","value":[{"type":"integer","value":9007199254740993},{"type":"null"}]})")},raw,"source.json");
  assert(result.status==200);has(result,"\"filtered_log_json\":"+serialize_embedding_workbench_string_json(raw));
  has(result,"\"activity_key\":\"work\"");has(result,"\"filter_attribute_key\":\" selected\\n\"");
  for(const auto& text:{R"({"type":"string","value":""})",R"({"type":"integer","value":"18446744073709551616"})",R"({"type":"decimal","value":"NaN"})",R"({"type":"decimal","value":"INF"})",R"({"type":"boolean","value":false})",R"({"type":"date","value":"2026-03-03T04:05:06.000000001Z"})",R"({"type":"map","value":{"x":{"type":"integer","value":1}}})",R"({"type":"set","value":[{"type":"integer","value":1}]})",R"({"type":"frozenset","value":[{"type":"integer","value":1}]})",R"({"type":"na"})"})
    (void)parse_log_attribute_repetition_target_json(text);
  for(const auto& text:{"-99999999999999999999999999999999999999","1.5","false","null","\"2\""," NaN ","INF","-INF"})
    (void)parse_log_attribute_repetition_bound_json(text);
  for(const auto& text:{R"({"type":"integer","value":"1.5"})",R"({"type":"null","value":false})",R"({"type":"date","value":"invalid"})",R"({"type":"set","value":[{"type":"list","value":[]}]})",R"({"type":"string","type":"string","value":"x"})",R"({"type":"string","value":"x","extra":0})",R"({"type":"string","value":"x"} trailing)",R"({"type":"decimal","value":"garbage"})"}) {
    bool failed=false;try{(void)parse_log_attribute_repetition_target_json(text);}catch(const std::invalid_argument&){failed=true;}assert(failed);
  }
  Event special=event;special.attributes={EventAttribute{std::string("key\0",4),std::string("A\0B",3)}};
  log.traces.front().events={special,special};
  result=run("repetition",{field("activity_key",std::string("key\0",4)),field("target_json",R"({"type":"string","value":"A\u0000B"})")},serialize_embedding_workbench_raw_log_json(log),"source.json");
  assert(result.status==200);has(result,"\"result\":{\"traces\":1,\"events\":2}");
  auto nan=parse_log_attribute_repetition_target_json(R"({"type":"decimal","value":"NaN"})");nan.key="selected";
  log.traces.front().events.front().attributes={nan};log.traces.front().events.back().attributes={nan};
  result=run("repetition",{field("activity_key","selected"),field("target_json",R"({"type":"decimal","value":"NaN"})"),field("min_rep","0"),field("max_rep","0")},serialize_embedding_workbench_raw_log_json(log),"source.json");
  assert(result.status==200);has(result,"\"result\":{\"traces\":1,\"events\":2}");
  log.traces.front().events.front().attributes={make_native_na_attribute("selected")};
  result=run("repetition",{field("activity_key","selected"),field("target_json",R"({"type":"na"})")},serialize_embedding_workbench_raw_log_json(log),"source.json");
  assert(result.status==400);has(result,"boolean value of NA is ambiguous");has(result,"\"source_unchanged\":true");
  auto nested=parse_log_attribute_repetition_target_json(R"({"type":"map","value":{"x":{"type":"list","value":[{"type":"na"}]}}})");nested.key="selected";
  log.traces.front().events.front().attributes={nested};log.traces.front().events.back().attributes={nested};
  result=run("repetition",{field("activity_key","selected"),field("target_json",R"({"type":"map","value":{"x":{"type":"list","value":[{"type":"na"}]}}})")},serialize_embedding_workbench_raw_log_json(log),"source.json");
  assert(result.status==200);has(result,"\"result\":{\"traces\":1,\"events\":2}");
  result=run("repetition",{target},"{invalid","source.json");
  assert(result.status==400);has(result,"\"phase\":\"source_import\"");has(result,"\"activity_key\":null");has(result,"\"filter_attribute_key\":null");
  const auto byte_value=parse_log_attribute_repetition_target_json(R"({"type":"bytes","value":"FF00"})");
  assert(byte_value==make_bytes_attribute("",std::string("\xff\0",2)));
  log.traces.front().events.front().attributes={make_bytes_attribute("selected",std::string("\xff\0",2))};
  log.traces.front().events.back().attributes=log.traces.front().events.front().attributes;
  result=run("repetition",{field("activity_key","selected"),field("target_json",R"({"type":"bytes","value":"ff00"})")},serialize_embedding_workbench_raw_log_json(log),"source.json");
  assert(result.status==200);has(result,"\"result\":{\"traces\":1,\"events\":2}");
  for(const auto* invalid:{R"({"type":"bytes","value":"f"})",R"({"type":"bytes","value":"gg"})"}) {
    bool failed=false;try{(void)parse_log_attribute_repetition_target_json(invalid);}catch(const std::invalid_argument&){failed=true;}assert(failed);
  }
  assert(run("repetition").status==400);
  assert(run("repetition",{target,field("positive","true")}).status==400);
  assert(run("repetition",{target,field("activity","A")}).status==400);
  assert(run("repetition",{target,field("min_rep","[]")}).status==400);
  assert(run("rework",{field("activity","A"),target}).status==400);
}
std::string artifact(const DiscoveryWorkbenchResponse& response,const std::string& name) {
  const auto token="\""+name+"\":";auto at=response.json.find(token);assert(at!=std::string::npos);at+=token.size();assert(response.json[at]=='"');const auto start=at++;bool escaped=false;
  for(;at<response.json.size();++at){const char c=response.json[at];if(escaped)escaped=false;else if(c=='\\')escaped=true;else if(c=='"')return parse_literal_string_vector_json("["+response.json.substr(start,at-start+1)+"]").at(0);}
  throw std::runtime_error("Unterminated artifact");
}
void between_workflow() {
  auto result=run("between",{field("act1_json","\"A\""),field("act2_json","[\"B\"]")});assert(result.status==200);
  has(result,"\"result\":{\"traces\":1,\"events\":2}");has(result,"\"case_id_key\":\"case_id\"");has(result,"\"activity_key\":\"activity\"");
  const auto filtered=parse_embedding_workbench_raw_log_json(artifact(result,"filtered_log_json"));
  assert(filtered.traces.size()==1&&filtered.traces[0].case_id=="c1##@@0"&&filtered.traces[0].events.size()==2);
  assert(artifact(result,"full_result_json").find("log-between-result-v1")!=std::string::npos);
  std::istringstream input(artifact(result,"filtered_xes"));const auto reopened=import_xes_event_log(input);
  assert(reopened.traces.size()==1&&reopened.traces[0].case_id=="c1##@@0"&&reopened.traces[0].events.size()==2);
  result=run("between",{field("act1_json","[\"A\"]"),field("act2_json","\"B\""),field("subcase_concat_str",""),field("case_id_column","case"),field("activity_column","work"),field("delimiter",";")},"case;work\nid;A\nid;B\nid;A\nid;B\n","source.csv");
  assert(result.status==200);has(result,"\"result\":{\"traces\":2,\"events\":4}");has(result,"\"activity_key\":\"work\"");has(result,"\"case_id_key\":\"case\"");
  const auto custom=parse_embedding_workbench_raw_log_json(artifact(result,"filtered_log_json"));assert(custom.traces[0].case_id=="id0"&&custom.traces[1].case_id=="id1");
  result=run("between",{field("act1_json","[]"),field("act2_json","[]")});assert(result.status==200);has(result,"\"result\":{\"traces\":0,\"events\":0}");
  assert(run("between").status==400);assert(run("between",{field("act1_json","\"A\""),field("act2_json","[1]")}).status==400);
  assert(run("between",{field("act1_json","[]"),field("act2_json","[]"),field("positive","true")}).status==400);
  assert(run("repetition",{field("target_json",R"({"type":"null"})"),field("case_id_key","case")}).status==400);
}
void invalid_controls() {
  assert(run("unknown").status==400);
  assert(run("prefix").status==400);
  assert(run("prefix",{field("activity","A"),field("positive","true")}).status==400);
  assert(run("start",{field("admitted_json","[]"),field("strict","true")}).status==400);
  assert(run("traces",{field("patterns_json","[[\"A\"],]")}).status==400);
  assert(run("traces",{field("patterns_json","[[42]]")}).status==400);
  assert(run("traces",{field("patterns_json","[] trailing")}).status==400);
  assert(run("rework",{field("activity","A"),field("min_occurrences","2x")}).status==400);
  assert(execute_reference_log_filter({{"operation","","start",false},{"source_file","",csv,false}}).status==400);
  assert(execute_reference_log_filter({{"operation","","start",false},{"operation","","end",false}}).status==400);
}
}
int main(){csv_filter_workflow();native_literal_and_export_workflow();repetition_workflow_and_typed_controls();between_workflow();invalid_controls();}
