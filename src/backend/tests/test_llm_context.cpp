#include <cassert>
#include <cstdio>
#include <sstream>
#include "minera/llm_context.hpp"
#include "minera/llm_abstractions.hpp"
#include "minera/original_model_context.hpp"
#include "minera/declare.hpp"
#include "minera/log_skeleton.hpp"
#include "minera/temporal_profile.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/log_conversion.hpp"
int main(){
 std::istringstream csv("case_id,activity,timestamp,resource,cost\nc1,Open,2026-10-01T10:00:00Z,Ana,1\nc1,Review α,2026-10-01T11:00:00Z,Ben,2\nc2,Close,2026-10-02T10:00:00Z,Ana,3\n");const auto log=minera::import_csv_event_log(csv);const auto original=minera::log_conversion_json(minera::convert_event_log(log));
 for(const auto& variant:minera::llm_context_variants())if(!minera::llm_context_is_ocel(variant)){minera::LlmContextOptions options;options.variant=variant;const auto result=minera::build_llm_context(log,options);assert(!result.text.empty()&&result.characters>0);}
 minera::LlmContextOptions options;const auto dfg=minera::build_llm_context(log,options);assert(dfg.text=="If I have a process with flow:\n\nOpen -> Review α ( frequency = 1  performance = 3600.000 )\n\n\n");options.include_performance=false;assert(minera::build_llm_context(log,options).text.find("performance")==std::string::npos);
 options.variant="case";options.include_case_attributes=false;options.include_event_attributes=false;options.include_timestamps=false;const auto plain=minera::build_llm_context(log,options);assert(plain.text.find("cost")==std::string::npos&&plain.text.find("2026-")==std::string::npos&&plain.text.find("Review α")!=std::string::npos);
 // The reference budget is checked before each line, so a one-character
 // budget keeps only the header; the context still reports the cut.
 options.variant="variants";options.max_len=1;const auto bounded=minera::build_llm_context(log,options);assert(bounded.truncated&&bounded.text=="If I have a process with the following process variants:\n\n\n\n");
 options={};options.variant="variants";assert(!minera::build_llm_context(log,options).truncated);
 // Each reference-abstraction variant returns exactly the reference port.
 {minera::LlmAbstractionOptions exact;const auto text=[&](const char* variant){minera::LlmContextOptions o;o.variant=variant;return minera::build_llm_context(log,o).text;};
  assert(text("dfg")==minera::llm_log_dfg_description(log,exact));assert(text("variants")==minera::llm_log_variants_description(log,exact));
  assert(text("event_stream")==minera::llm_event_stream_description(log,exact));assert(text("log_attributes")==minera::llm_log_columns_description(log));
  assert(text("log_features")==minera::llm_log_features_description(log,exact));assert(text("log_skeleton")==minera::llm_log_skeleton_description(log,exact));
  assert(text("declare")==minera::abstract_declare(minera::discover_declare(log)).text);assert(text("temporal_profile")==minera::abstract_temporal_profile(minera::discover_temporal_profile(log)).text);
  minera::LlmContextOptions agg;agg.variant="dfg";agg.relative_frequency=true;agg.primary_aggregation="median";agg.secondary_aggregation="max";agg.include_header=false;
  exact.relative_frequency=true;exact.primary_performance_aggregation="median";exact.secondary_performance_aggregation="max";exact.response_header=false;exact.include_header=false;
  assert(minera::build_llm_context(log,agg).text==minera::llm_log_dfg_description(log,exact));}
 // Reference label (pm4py declare_to_descr): "Responded Existence".
 {minera::DeclareModel model;model.rules.clear();model.rules["responded_existence"]={{"A","B",true,0,0}};assert(minera::abstract_declare(model,false).text=="\nThese are the constraints of the model:\nResponded Existence: ('A', 'B')\n");}
 // Empty traces have no rows in the reference event table: no empty variant,
 // and relative frequencies use the non-empty cases. Expected texts captured
 // from pm4py 2.7.22 algo.querying.llm.abstractions (log_to_variants_descr,
 // log_to_dfg_descr) on the same log.
 {minera::EventLog empty;const auto add=[&](std::string id,std::vector<std::pair<std::string,int>> events){minera::Trace trace;trace.case_id=id;trace.case_id_state=minera::FirstClassAttributeState::kPresent;for(const auto& [activity,second]:events){char stamp[32];std::snprintf(stamp,sizeof stamp,"2024-01-01T00:%02d:%02dZ",second/60,second%60);minera::Event event{id,activity,stamp,"",trace.events.size()};event.activity_state=event.timestamp_state=minera::FirstClassAttributeState::kPresent;event.resource_state=minera::FirstClassAttributeState::kMissing;trace.events.push_back(std::move(event));}empty.traces.push_back(std::move(trace));};
  add("c1",{{"a",0},{"b",60}});add("c2",{{"a",100},{"b",220}});add("c3",{});add("c4",{{"a",300},{"c",310}});
  minera::LlmAbstractionOptions o;assert(minera::llm_log_variants_description(empty,o)=="If I have a process with the following process variants:\n\n a -> b ( frequency = 2  performance = 90.000 )\n a -> c ( frequency = 1  performance = 10.000 )\n\n\n");
  o.relative_frequency=true;assert(minera::llm_log_variants_description(empty,o)=="If I have a process with the following process variants:\n\n a -> b ( frequency = 66\\%  performance = 90.000 )\n a -> c ( frequency = 33\\%  performance = 10.000 )\n\n\n");
  assert(minera::llm_log_dfg_description(empty,o)=="If I have a process with flow:\n\na -> b ( frequency = 66\\%  performance = 90.000 )\na -> c ( frequency = 33\\%  performance = 10.000 )\n\n\n");}
 assert(minera::log_conversion_json(minera::convert_event_log(log))==original);
 bool failed=false;try{options.variant="unknown";minera::build_llm_context(log,options);}catch(const std::exception&){failed=true;}assert(failed);
 minera::Ocel ocel;ocel.events={{"e1","Create","2026-10-01T10:00:00Z",{}},{"e2","Close","2026-10-01T11:00:00Z",{}}};ocel.objects={{"o1","Order",{}}};ocel.relations={{"e1","o1",""},{"e2","o1",""}};
 for(const auto& variant:{"ocel","ocel_features","ocel_ocdfg"}){options={};options.variant=variant;options.object_type="Order";const auto result=minera::build_llm_context(ocel,options);assert(!result.text.empty());}
}
