#include <cassert>
#include <sstream>
#include "minera/csv_event_log_importer.hpp"
#include "minera/log_conversion.hpp"
#include "log_cluster_runtime.hpp"
int main(){
 std::istringstream csv("case_id,activity,timestamp,custom\nc1,Open,2026-10-01T09:00:00Z,A\nc1,Review α,2026-10-01T10:00:00Z,B\nc2,Close,2026-10-02T09:00:00Z,C\n");const auto log=minera::import_csv_event_log(csv);
 const auto body=minera::log_cluster_runtime::selection_body(log,"Review α");assert(body.find("Open -> Review α")!=std::string::npos&&body.find("\"indices\":[0,1]")!=std::string::npos);
 assert(minera::log_cluster_runtime::selection_body(log,"B","custom").find("A -> B")!=std::string::npos);
 const auto filtered=minera::filter_query_events(log,{1},true);assert(filtered.event_count()==2);
 const auto summary=minera::log_cluster_runtime::summary(filtered,"Review α",".*Review.*",{0,1},"7");assert(summary.find("\"source_id\":\"7\"")!=std::string::npos&&summary.find("\"case_count\":1")!=std::string::npos);
 auto expected=log;expected.traces.resize(1);assert(minera::log_conversion_json(minera::convert_event_log(filtered))==minera::log_conversion_json(minera::convert_event_log(expected)));
 const auto empty=minera::log_cluster_runtime::table_json(log,{});assert(empty.find("\"columns\":[\"activity\"")!=std::string::npos&&empty.find("\"rows\":[]")!=std::string::npos);
 bool rejected=false;try{minera::log_cluster_runtime::selection_body(log,"x","missing");}catch(const std::exception&){rejected=true;}assert(rejected);
}
