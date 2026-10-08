#include <cassert>
#include <sstream>
#include "minera/llm_context.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/log_conversion.hpp"
int main(){
 std::istringstream csv("case_id,activity,timestamp,resource,cost\nc1,Open,2026-10-01T10:00:00Z,Ana,1\nc1,Review α,2026-10-01T11:00:00Z,Ben,2\nc2,Close,2026-10-02T10:00:00Z,Ana,3\n");const auto log=minera::import_csv_event_log(csv);const auto original=minera::log_conversion_json(minera::convert_event_log(log));
 for(const auto& variant:minera::llm_context_variants())if(!minera::llm_context_is_ocel(variant)){minera::LlmContextOptions options;options.variant=variant;const auto result=minera::build_llm_context(log,options);assert(!result.text.empty()&&result.characters>0);}
 minera::LlmContextOptions options;const auto dfg=minera::build_llm_context(log,options);assert(dfg.text=="If I have a process with flow:\n\nOpen -> Review α ( frequency = 1  performance = 3600.000 )\n\n\n");options.include_performance=false;assert(minera::build_llm_context(log,options).text.find("performance")==std::string::npos);
 options.variant="case";options.include_case_attributes=false;options.include_event_attributes=false;options.include_timestamps=false;const auto plain=minera::build_llm_context(log,options);assert(plain.text.find("cost")==std::string::npos&&plain.text.find("2026-")==std::string::npos&&plain.text.find("Review α")!=std::string::npos);
 options.variant="variants";options.max_len=1;const auto bounded=minera::build_llm_context(log,options);assert(bounded.truncated&&bounded.text.find("Review α")!=std::string::npos);
 assert(minera::log_conversion_json(minera::convert_event_log(log))==original);
 bool failed=false;try{options.variant="unknown";minera::build_llm_context(log,options);}catch(const std::exception&){failed=true;}assert(failed);
 minera::Ocel ocel;ocel.events={{"e1","Create","2026-10-01T10:00:00Z",{}},{"e2","Close","2026-10-01T11:00:00Z",{}}};ocel.objects={{"o1","Order",{}}};ocel.relations={{"e1","o1",""},{"e2","o1",""}};
 for(const auto& variant:{"ocel","ocel_features","ocel_ocdfg"}){options={};options.variant=variant;options.object_type="Order";const auto result=minera::build_llm_context(ocel,options);assert(!result.text.empty());}
}
