#include <cassert>
#include <sstream>
#include "minera/log_query.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/xes_export.hpp"
#include "minera/log_conversion.hpp"

template<class F> void rejects(F&& function){bool failed=false;try{function();}catch(const std::exception&){failed=true;}assert(failed);}
int main(){
 if(!minera::sqlite_log_query_available())return 0;
 std::istringstream csv("case_id,activity,timestamp,resource,cost\nC1,Open,2026-10-01T09:00:00Z,Ana,10\nC1,Review α,2026-10-01T10:00:00Z,Ben,20\nC2,Close,2026-10-02T09:00:00Z,Ana,30\n");
 auto log=minera::import_csv_event_log(csv);auto table=minera::log_query_table(log);assert(table.table.rows.size()==3);
 const auto result=minera::query_log_sqlite(table,"SELECT COUNT(*) AS events, SUM(CAST(cost AS INTEGER)) AS total FROM dataframe");
 assert(std::get<std::int64_t>(result.rows[0][0])==3);assert(std::get<std::int64_t>(result.rows[0][1])==60);assert(!result.can_filter);
 const auto selected=minera::query_log_sqlite(table,"SELECT * FROM dataframe WHERE activity = 'Review α'");assert(selected.can_filter);assert(selected.selected_event_indices==std::vector<std::size_t>{1});
 const auto events=minera::filter_query_events(log,selected.selected_event_indices);assert(events.event_count()==1&&events.traces.size()==1);auto expected=log;expected.traces.resize(1);expected.traces[0].events={log.traces[0].events[1]};assert(minera::log_conversion_json(minera::convert_event_log(events))==minera::log_conversion_json(minera::convert_event_log(expected)));
 const auto cases=minera::filter_query_events(log,selected.selected_event_indices,true);assert(cases.event_count()==2);expected=log;expected.traces.resize(1);assert(minera::log_conversion_json(minera::convert_event_log(cases))==minera::log_conversion_json(minera::convert_event_log(expected)));
 minera::LogQueryOptions small;small.max_rows=1;const auto limited=minera::query_log_sqlite(table,"SELECT * FROM dataframe",small);assert(limited.output_truncated&&limited.can_filter&&limited.selected_event_indices.empty());
 const auto empty=minera::query_log_sqlite(table,"SELECT * FROM dataframe WHERE 0");assert(empty.can_filter&&empty.rows.empty());assert(minera::filter_query_events(log,{}).event_count()==0);
 const auto window=minera::query_log_sqlite(table,"WITH counts AS (SELECT activity, ROW_NUMBER() OVER (ORDER BY cost DESC) AS rank FROM dataframe) SELECT activity FROM counts WHERE rank=1");assert(std::get<std::string>(window.rows[0][0])=="Close");
 const auto precise=minera::query_log_sqlite(table,"SELECT 9223372036854775807 AS large");assert(minera::log_query_json(precise).find("\"type\":\"integer\"")!=std::string::npos);
 const auto ambiguous=minera::query_log_sqlite(table,"SELECT __minera_event_index,__minera_event_index FROM dataframe");assert(!ambiguous.can_filter&&ambiguous.selected_event_indices.empty());
 minera::LogQueryOptions parameters;parameters.parameters={std::string("Review α")};assert(minera::query_log_sqlite(table,"SELECT * FROM dataframe WHERE activity=?",parameters).selected_event_indices==std::vector<std::size_t>{1});
 for(const auto* sql:{"DELETE FROM dataframe","DROP TABLE dataframe","ATTACH DATABASE 'unwanted.db' AS other","SELECT load_extension('unwanted')","SELECT 1;SELECT 2"})rejects([&]{minera::query_log_sqlite(table,sql);});
 minera::LogQueryOptions bound;bound.max_vm_steps=1000;rejects([&]{minera::query_log_sqlite(table,"WITH RECURSIVE n(x) AS (SELECT 1 UNION ALL SELECT x+1 FROM n WHERE x<10000000) SELECT SUM(x) FROM n",bound);});
 rejects([&]{minera::filter_query_events(log,{999});});
 auto collision=log;collision.traces[0].events[0].attributes.push_back({"__minera_event_index","user field"});auto collided=minera::log_query_table(collision);assert(collided.event_index_column!="__minera_event_index");
 minera::XesExportOptions options;options.preserve_nulls=true;assert(minera::serialize_xes(cases,options).find("Review α")!=std::string::npos);
}
