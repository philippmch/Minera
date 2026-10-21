#include "table_attribute_repetition_test_support.hpp"
#include "minera/ltl_filter_workbench.hpp"
#include "minera/xes_event_log_importer.hpp"
#include <functional>
#include <iostream>
#include <sstream>
namespace ltl_workbench_test {
using namespace table_repetition_test;using Parts=std::vector<DiscoveryWorkbenchPart>;
inline std::optional<std::string> native_text(const std::string& json,const std::string& name){
 const auto token="\""+name+"\":";const auto found=json.find(token);require(found!=std::string::npos,"missing native string "+name);const auto begin=found+token.size();if(json.at(begin)!='{')return text(json,name);
 const auto end=json.find('}',begin);require(end!=std::string::npos,"unterminated native string "+name);const auto object=json.substr(begin,end-begin+1);require(text(object,"native_bytes")=="hex-v1","unknown native string wrapper");const auto hex=*text(object,"data");require(hex.size()%2==0,"odd native string bytes");
 const auto digit=[](char c){require(c>='0'&&c<='9'||c>='a'&&c<='f',"invalid native string hex");return c<='9'?c-'0':c-'a'+10;};std::string result;for(std::size_t i=0;i<hex.size();i+=2)result.push_back(static_cast<char>(digit(hex[i])*16+digit(hex[i+1])));return result;
}
inline std::vector<std::vector<std::string>> sample_rows(){return {{"c1","A","r1"},{"c1","B","r2"},{"c1","C","r1"},{"c2","A","r1"},{"c2","C","r1"},{"c2","B","r1"},{"c3","A","r1"},{"c3","A","r2"}};}
inline EventTable sample_table(const std::string& engine){
 auto table=source_table(engine);table.columns={"case:concept:name","concept:name","org:resource","signed","unsigned"};table.dtypes=engine=="polars"?std::vector<std::string>{"String","String","String","Int64","UInt64"}:std::vector<std::string>{"object","object","object","int64","uint64"};table.columns.push_back("time:timestamp");table.dtypes.push_back(engine=="polars"?"Datetime(time_unit='us', time_zone='UTC')":"datetime64[ns, UTC]");table.categorical_dtypes.clear();table.rows.clear();
 for(const auto& row:sample_rows())table.rows.push_back({EventAttribute{table.columns[0],row[0]},EventAttribute{table.columns[1],row[1]},EventAttribute{table.columns[2],row[2]},integer("signed",9007199254740993LL),unsigned_integer("unsigned",18446744073709551615ULL)});
 for(std::size_t i=0;i<table.rows.size();++i){const auto iso="2026-01-01T00:00:0"+std::to_string(i)+"Z";EventAttribute stamp{"time:timestamp",iso,AttributeKind::kDate};stamp.typed_value=NativeDate{iso,engine=="polars"?NativeDateResolution::kMicrosecond:NativeDateResolution::kNanosecond,std::string("UTC")};table.rows[i].push_back(stamp);}
 if(table.index){table.index->labels.clear();for(std::int64_t i=0;i<8;++i)table.index->labels.push_back({integer("",9007199254740993LL+i)});}return table;
}
inline EventLog sample_log(){
 EventLog log;log.attributes={{"opaque",std::string("\xff\0",2)},integer("signed",9007199254740993LL),unsigned_integer("unsigned",18446744073709551615ULL)};log.metadata.extensions={{"Concept","concept","urn:ltl"}};log.metadata.classifiers={{"activity",{"concept:name"}}};log.metadata.properties={{"context","full"}};
 for(const auto& row:sample_rows()){
  if(log.traces.empty()||log.traces.back().case_id!=row[0]){Trace trace;trace.case_id=row[0];trace.case_id_source_key="concept:name";trace.case_id_attribute=EventAttribute{"concept:name",row[0]};trace.properties={{"trace context","full"}};log.traces.push_back(std::move(trace));}
  Event event;event.activity=row[1];event.activity_source_key="concept:name";event.activity_attribute=EventAttribute{"concept:name",row[1]};event.resource=row[2];event.resource_source_key="org:resource";event.resource_attribute=EventAttribute{"org:resource",row[2]};event.attributes={integer("signed",9007199254740993LL),unsigned_integer("unsigned",18446744073709551615ULL),EventAttribute{"payload",std::string("\xff\0",2)}};log.traces.back().events.push_back(std::move(event));
 }return log;
}
inline Parts request(const std::string& engine,const std::string& source,const std::string& operation="eventually",Parts extra={}){
 Parts parts={field("engine",engine),field("operation",operation),{"source_file","source.json",source,true}};
 if(operation=="eventually" || engine=="polars"&&(operation=="next" || operation=="four_eyes"))parts.push_back(field("values_json",operation=="four_eyes"?"[\"A\",\"B\"]":"[\"A\",\"B\",\"C\"]"));
 else if(operation=="different_persons")parts.push_back(field("target_json",R"({"type":"string","value":"A"})"));
 else{parts.push_back(field("a_json",R"({"type":"string","value":"A"})"));parts.push_back(field("b_json",R"({"type":"string","value":"B"})"));if(operation=="next")parts.push_back(field("c_json",R"({"type":"string","value":"C"})"));}
 parts.insert(parts.end(),extra.begin(),extra.end());return parts;
}
inline EventTable subset(EventTable source,const std::vector<std::size_t>& indices){const auto original=source;source.rows.clear();if(source.index)source.index->labels.clear();for(auto i:indices){source.rows.push_back(original.rows.at(i));if(source.index)source.index->labels.push_back(original.index->labels.at(i));}return source;}
inline void common_success(const DiscoveryWorkbenchResponse& answer,const std::string& source,const std::string& expected,const std::string& engine){
 require(answer.status==200,"relationship success expected: "+answer.json);contains(answer,"\"family\":\"REFERENCE_LTL_FILTER\"");contains(answer,"\"phase\":\"complete\"");contains(answer,"\"apply_returned\":true");contains(answer,"\"source_unchanged\":true");require(text(answer.json,"engine")==engine,"relationship engine changed");require(text(answer.json,"source_json")==source,"relationship full parsed source changed");require(text(answer.json,"source_after_json")==source,"relationship full source-after changed");require(text(answer.json,"result_json")==expected,"relationship complete result mismatch");require(!text(answer.json,"error"),"relationship unexpected diagnostic");
}
inline void workflows(const std::function<DiscoveryWorkbenchResponse(const Parts&)>& run){
 for(const std::string engine:{"log","pandas","polars"}){
  const auto table=sample_table(engine=="log"?"pandas":engine);const auto log=sample_log();const auto source=engine=="log"?serialize_embedding_workbench_raw_log_json(log):serialize_embedding_workbench_table_json(table);
  for(const std::string operation:{"eventually","next","four_eyes","different_persons"}){
   const auto selected=operation=="different_persons"?std::vector<std::size_t>{6,7}:std::vector<std::size_t>{0,1,2};auto expected_log=log;expected_log.traces=operation=="different_persons"?std::vector<Trace>{log.traces[2]}:std::vector<Trace>{log.traces[0]};
   const auto expected=engine=="log"?serialize_embedding_workbench_raw_log_json(expected_log):serialize_embedding_workbench_table_json(subset(table,selected));
   const auto answer=run(request(engine,source,operation));common_success(answer,source,expected,engine);
   if(engine=="log"){
    const auto xes=native_text(answer.json,"filtered_xes");require(xes.has_value(),"filtered relationship XES missing");std::istringstream stream(*xes);const auto imported=import_xes_event_log(stream);require(imported.traces.size()==1,"relationship XES projection changed traces");require(imported.traces[0].events.size()==selected.size(),"relationship XES projection changed events");
   }else require(serialize_embedding_workbench_table_json(parse_embedding_workbench_table_json(*text(answer.json,"result_json")))==expected,"relationship table artifact cannot reopen");
  }
  const std::string literal(" literal\n\0Ω ",13);
  if(engine=="log"){
   auto custom=log;for(auto& trace:custom.traces)for(auto& event:trace.events){event.activity_source_key=literal;event.activity_attribute->key=literal;}
   auto expected=custom;expected.traces={custom.traces[0]};
   common_success(run(request(engine,serialize_embedding_workbench_raw_log_json(custom),"eventually",{{"attribute_key","attribute_key.txt",literal,true}})),serialize_embedding_workbench_raw_log_json(custom),serialize_embedding_workbench_raw_log_json(expected),engine);
  }else{
   auto custom=table;custom.columns[1]=literal;for(auto& row:custom.rows)row[1]->key=literal;
   const auto raw=serialize_embedding_workbench_table_json(custom);
   common_success(run(request(engine,raw,"eventually",{{"attribute_key","attribute_key.txt",literal,true}})),raw,serialize_embedding_workbench_table_json(subset(custom,{0,1,2})),engine);
  }
  if(engine=="polars"){
   auto custom=table;custom.columns[5]=literal;for(auto& row:custom.rows)row[5]->key=literal;const auto raw=serialize_embedding_workbench_table_json(custom);
   for(const std::string operation:{"eventually","next"})common_success(run(request(engine,raw,operation,{{"timestamp_key","timestamp_key.txt",literal,true}})),raw,serialize_embedding_workbench_table_json(subset(custom,{0,1,2})),engine);
  }
  const std::string rejecting=R"({"type":"list","value":[{"type":"tuple","value":[{"type":"integer","value":"9"},{"type":"integer","value":"9"}]},{"type":"tuple","value":[{"type":"integer","value":"9"},{"type":"integer","value":"9"}]}]})";
  if(engine!="polars"){
   auto expected_log=log;expected_log.traces={log.traces[0]};const auto ordinary=engine=="log"?serialize_embedding_workbench_raw_log_json(expected_log):serialize_embedding_workbench_table_json(subset(table,{0,1,2}));
   common_success(run(request(engine,source,"eventually",{field("enable_timestamp","false"),field("timestamp_diff_boundaries_json",rejecting)})),source,ordinary,engine);
   if(engine=="pandas")common_success(run(request(engine,source,"eventually",{field("timestamp_diff_boundaries_json",rejecting)})),source,serialize_embedding_workbench_table_json(subset(table,{})),engine);
  }
  auto missing=request(engine,source,"eventually",{field("attribute_key","missing")});const auto answer=run(missing);
  // Logs can treat absent activity keys as nonmatches; tables diagnose lookup.
  if(engine!="log"){require(answer.status==400,"missing relationship table column accepted");require(text(answer.json,"source_json")==source && text(answer.json,"source_after_json")==source,"relationship error lost source snapshots");}
 }
 const std::string differences=R"({"type":"list","value":[{"type":"integer","value":"1"},{"type":"integer","value":"4"}]})",intervals=R"({"type":"list","value":[{"type":"tuple","value":[{"type":"integer","value":"1"},{"type":"integer","value":"2"}]},{"type":"tuple","value":[{"type":"integer","value":"3"},{"type":"integer","value":"4"}]}]})";
 for(const std::string operation:{"timestamp_ge","timestamp_le"}){
  const auto answer=run({field("engine","log"),field("operation",operation),field("helper_a_json",differences),field("helper_b_json",intervals)});
  require(answer.status==200,"nonempty interval helper failed: "+answer.json);const auto result=parse_embedding_workbench_table_json(*text(answer.json,"helper_result_json"));require(result.rows.size()==1 && result.rows[0].size()==1 && result.rows[0][0] && result.rows[0][0]->kind==AttributeKind::kBoolean && result.rows[0][0]->value=="true","inclusive helper interval endpoints changed");
 }
}
}
#ifndef MINERA_LTL_ROUTE_TEST
int main(){try{using namespace ltl_workbench_test;workflows([](const auto& parts){return execute_ltl_filter_workbench(parts);});const auto source=serialize_embedding_workbench_table_json(sample_table("polars"));
 for(const auto& invalid:Parts{field("unknown","x"),field("engine","polars"),field("positive","yes"),field("resource_key","inactive"),field("timestamp_key","inactive"),field("a_json","inactive"),field("helper_a_json","inactive"),field("source_format","native_log")})require(execute_ltl_filter_workbench(request("polars",source,"eventually",{invalid})).status==400,"invalid/inactive relationship control accepted");
 require(execute_ltl_filter_workbench(request("polars",source),{field("engine","polars")}).status==400,"duplicate query/multipart relationship control accepted");
 auto scalar=request("polars",source);scalar[2].file=false;require(execute_ltl_filter_workbench(scalar).status==400,"plain source text accepted as File");
 std::cout<<"full relationship artifacts and strict control validation passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
#endif
