#include "minera/exact_log_query_json.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/literal_controls_json.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <source_location>

using namespace minera;
void has(std::string_view text,std::string_view fragment,std::source_location where=std::source_location::current()){
  if(text.find(fragment)==std::string_view::npos){std::cerr<<where.line()<<": missing "<<fragment<<" in "<<text.substr(0,4096)<<'\n';assert(false);}
}
template<class F>void rejects(F f,std::string_view message){bool rejected=false;try{f();}catch(const std::exception& e){rejected=true;has(e.what(),message);}assert(rejected);}
std::string defaults(){return R"({"table_name":"dataframe","max_rows":1000,"max_vm_steps":50000000,"parameters":[]})";}
std::vector<DiscoveryWorkbenchPart> controls(std::string sql="SELECT 1",std::string options=defaults()){
  return {{"source_id","","source-1",false},{"sql","",std::move(sql),false},{"query_options_json","",std::move(options),false}};
}
EventAttribute cell(std::string key,std::string value){EventAttribute a;a.key=std::move(key);a.value=std::move(value);return a;}
EventAttribute integer_cell(std::string key,std::int64_t value){auto a=cell(std::move(key),std::to_string(value));a.kind=AttributeKind::kInt;a.typed_value=value;return a;}
LogQueryTable source(){
  LogQueryTable t;t.event_index_column="event_index";t.table.columns={"event_index","activity"};
  t.table.rows={{integer_cell("event_index",0),cell("activity"," A,? ")},{integer_cell("event_index",1),cell("activity","B")}};
  t.table.attributes={cell(std::string("meta\0\xff",6),"stored")};
  t.table.metadata.compatibility_snapshot_valid=true;t.table.metadata.properties={cell("p",std::string("\xed\xa0\x80",3))};
  t.table.metadata.compatibility_event_globals_snapshot={integer_cell("large",std::numeric_limits<std::int64_t>::max())};
  t.table.dtypes={"int64","object"};t.table.categorical_dtypes={std::nullopt,std::nullopt};
  EventTableIndex index;index.kind="MultiIndex";index.dtype="object";index.names={"first",std::nullopt};
  for(int i=0;i<2;++i){EventTableIndexLabel a{integer_cell("idx",i)},b{cell("raw",std::string("\0\xff",2))};index.labels.push_back({std::vector<EventTableIndexLabel>{a,b}});}t.table.index=index;
  return t;
}
void codec_domains(){
  const auto raw=R"({"table_name":{"native_bytes":"hex-v1","data":"2000ffeda080"},"max_rows":"18446744073709551615","max_vm_steps":"0","parameters":[{"index":0,"value":null},{"index":1,"value":"-9223372036854775808"},{"index":1,"value":"9223372036854775807"},{"index":2,"value":-0.0},{"index":2,"value":5e-324},{"index":2,"value":1.7976931348623157e308},{"index":2,"value":"NaN"},{"index":2,"value":"INF"},{"index":2,"value":"-INF"},{"index":3,"value":{"native_bytes":"hex-v1","data":"00ffeda080"}},{"index":4,"value":{"hex":{"native_bytes":"hex-v1","data":"6e6f742068657800ff"}}}]})";
  const auto o=parse_exact_log_query_options_json(raw);
  assert(o.value.table_name==std::string(" \0\xff\xed\xa0\x80",6));assert(o.value.max_rows==std::numeric_limits<std::size_t>::max());assert(o.value.max_vm_steps==0);
  assert(o.value.parameters.size()==11&&o.value.parameters[0].index()==0);
  assert(std::get<std::int64_t>(o.value.parameters[1])==std::numeric_limits<std::int64_t>::min());
  assert(std::signbit(std::get<double>(o.value.parameters[3])));
  assert(std::get<double>(o.value.parameters[4])==std::numeric_limits<double>::denorm_min());
  assert(std::get<double>(o.value.parameters[5])==std::numeric_limits<double>::max());
  assert(std::isnan(std::get<double>(o.value.parameters[6])));
  const auto encoded=serialize_exact_log_query_options_json(o);const auto again=parse_exact_log_query_options_json(encoded);
  assert(again.value.table_name==o.value.table_name&&again.value.parameters.size()==11);
  assert(std::get<QueryBlob>(again.value.parameters[10]).hex==std::string("not hex\0\xff",9));
  rejects([]{parse_exact_log_query_options_json(R"({"table_name":"x","max_rows":1,"parameters":[],"max_vm_steps":null})","duckdb");},"SQLite-only");
  rejects([]{parse_exact_log_query_options_json(R"({"table_name":"x","max_rows":1,"max_vm_steps":1,"parameters":[{"index":1,"value":true}]})");},"decimal integer");
  rejects([]{parse_exact_log_query_options_json(R"({"table_name":"x","max_rows":1,"max_vm_steps":1,"parameters":[{"index":4,"value":"00ff"}]})");},"object");
  rejects([]{parse_exact_log_query_options_json(R"({"table_name":"x","max_rows":1,"max_vm_steps":1,"parameters":[],"max_\u0072ows":2})");},"duplicate object key");
  rejects([]{parse_exact_log_query_options_json(R"({"table_name":"x","max_rows":1e0,"max_vm_steps":1,"parameters":[]})");},"decimal integer");
  const auto wtf=std::string("{\"table_name\":\"")+std::string("\xed\xa0\x80",3)+"\",\"max_rows\":1,\"max_vm_steps\":1,\"parameters\":[]}";
  assert(parse_exact_log_query_options_json(wtf).value.table_name==std::string("\xed\xa0\x80",3));
}
void request_provenance(){
  ExactLogQueryReport r;auto p=controls();p.back()={"query_options_file","",defaults(),true};
  const auto request=prepare_exact_log_query(r,p);assert(request.source_id=="source-1"&&r.source_id.empty());
  has(r.options_context,"\"filename\":\"\"");has(r.json(),"\"source_id\":null");
  auto bad=p;bad.back().file=false;ExactLogQueryReport invalid;
  rejects([&]{prepare_exact_log_query(invalid,bad);},"filename presence");has(invalid.options_context,"\"filename\":null");has(invalid.json(),"\"phase\":\"control\"");
  bad=p;bad.push_back({"query_options_json","","",false});ExactLogQueryReport conflict;
  rejects([&]{prepare_exact_log_query(conflict,bad);},"exactly one query_options");assert(conflict.original_inputs.size()==4);
  bad=p;bad.push_back({"limit","","",false});ExactLogQueryReport limit;
  rejects([&]{prepare_exact_log_query(limit,bad);},"conflict with limit");
  ExactLogQueryReport duplicate;
  rejects([&]{prepare_exact_log_query(duplicate,p,{{"source_id","","source-1",false}});},"duplicate");
  ExactLogQueryReport transport;
  rejects([&]{prepare_exact_log_query(transport,p,{},true,"multipart required");},"multipart required");has(transport.json(),"\"phase\":\"transport\"");
}
void sqlite_native_results(){
  if(!sqlite_log_query_available())return;
  auto table=source();const auto before=serialize_exact_log_query_table_json(table);
  ExactLogQueryReport r;auto request=prepare_exact_log_query(r,controls("SELECT ?2 AS high,?1 AS low,?3 AS text",R"({"table_name":" literal,? \" alias ","max_rows":1000,"max_vm_steps":50000000,"parameters":[{"index":1,"value":"-9223372036854775808"},{"index":1,"value":"9223372036854775807"},{"index":3,"value":{"native_bytes":"hex-v1","data":"00ffeda080"}}]})"));
  const auto value=execute_exact_sqlite_query(r,table,request.sql,request.options);
  assert(r.query_call.entered&&r.query_call.returned);assert(std::get<std::int64_t>(value.rows[0][0])==std::numeric_limits<std::int64_t>::max());
  assert(std::get<std::string>(value.rows[0][2])==std::string("\0\xff\xed\xa0\x80",5));assert(serialize_exact_log_query_table_json(table)==before);
  assert(r.optional_errors["legacy_transport"]!="null");has(r.json(),"\"type\":\"LogQueryResult\"");assert(r.artifacts["canonical_result_json"].bytes);
  ExactLogQueryReport blob;auto blob_request=prepare_exact_log_query(blob,controls("SELECT ?",R"({"table_name":"dataframe","max_rows":1,"max_vm_steps":50000,"parameters":[{"index":4,"value":{"hex":"not hex"}}]})"));
  rejects([&]{execute_exact_sqlite_query(blob,table,blob_request.sql,blob_request.options);},"blob query parameters");assert(blob.query_call.entered&&!blob.query_call.returned);assert(blob.snapshots["options_before"]!="null");
  ExactLogQueryReport zero;auto zero_request=prepare_exact_log_query(zero,controls("SELECT 1",R"({"table_name":"dataframe","max_rows":0,"max_vm_steps":1,"parameters":[]})"));
  rejects([&]{execute_exact_sqlite_query(zero,table,zero_request.sql,zero_request.options);},"invalid SQL query limits");assert(zero.query_call.entered);
  ExactLogQueryReport small;auto small_request=prepare_exact_log_query(small,controls("SELECT * FROM dataframe",R"({"table_name":"dataframe","max_rows":1,"max_vm_steps":50000,"parameters":[]})"));
  const auto truncated=execute_exact_sqlite_query(small,table,small_request.sql,small_request.options);assert(truncated.output_truncated&&truncated.can_filter&&truncated.selected_event_indices.empty());
  small_request.options.value.max_rows=1000;ExactLogQueryReport full;const auto complete=execute_exact_sqlite_query(full,table,small_request.sql,small_request.options);assert(complete.selected_event_indices==std::vector<std::size_t>({0,1}));
  ExactLogQueryReport bytes;const auto result=query_log_sqlite(table,"SELECT x'' AS empty_blob,x'00ff' AS raw");const auto canonical=serialize_exact_log_query_result_json(result);has(canonical,"\"hex\":\"00ff\"");has(canonical,"\"hex\":\"\"");
}
void runtime_observation_and_artifacts(){
  ExactLogQueryReport unknown;unknown.engine="duckdb";unknown.query_call.known=false;
  has(unknown.json(),"\"entered\":null,\"returned\":null");
  ExactLogQueryReport success;success.engine="duckdb";success.query_call.known=false;
  const std::string body=R"({"family":"LOG_QUERY","engine":"duckdb","sql":"SELECT 1","source_id":"1","event_index_column":"i","columns":["v"],"rows":[[1]],"output_truncated":false,"can_filter":false,"selected_event_indices":[],"exact_call":{"entered":true,"returned":true,"error":null}})";
  record_exact_duck_query(success,body,200);assert(success.query_call.known&&success.query_call.entered&&success.query_call.returned);assert(success.artifacts["legacy_result_json"].bytes==body);
  assert(parse_exact_duck_query_selection(body,2).indices.empty());
  ExactLogQueryReport failure;failure.engine="duckdb";failure.query_call.known=false;
  rejects([&]{record_exact_duck_query(failure,R"({"error":"bad parameters","exact_call":{"entered":true,"returned":false,"error":{"type":"ValueError","message":"bad parameters"}}})",400);},"bad parameters");
  assert(failure.query_call.known&&failure.query_call.entered&&!failure.query_call.returned);has(failure.query_call.error,"ValueError");
  ExactLogQueryReport missing;missing.engine="duckdb";missing.query_call.known=false;
  rejects([&]{record_exact_duck_query(missing,R"({"error":"framework error"})",400);},"missing field exact_call");assert(!missing.query_call.known);
  ExactLogQueryReport artifacts;const auto table=source();record_exact_query_filtered(artifacts,table.table);assert(artifacts.artifacts["filtered_table_json"].bytes);record_exact_query_output(artifacts,"xes","application/xml",std::string("\0\xff",2));assert(artifacts.artifacts["export_bytes"].bytes==std::string("\0\xff",2));
  has(artifacts.json(),"\"cli\":null");assert(!literal_controls_transport_error(artifacts.json()));
}
int main(){codec_domains();request_provenance();sqlite_native_results();runtime_observation_and_artifacts();}
