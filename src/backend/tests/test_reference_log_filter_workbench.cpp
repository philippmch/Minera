#include "minera/reference_log_filter_workbench.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/csv_event_log_importer.hpp"
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
int main(){csv_filter_workflow();native_literal_and_export_workflow();invalid_controls();}
