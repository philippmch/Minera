#include "table_attribute_repetition_test_support.hpp"
#include "minera/table_attributes_filter_workbench.hpp"
#include "minera/attributes_filter_controls.hpp"
#include <algorithm>
#include <functional>
#include <map>
#include <iostream>
namespace table_attributes_test {
using namespace table_repetition_test;
using Parts=std::vector<DiscoveryWorkbenchPart>;
inline EventTable subset(EventTable source,const std::vector<std::size_t>& rows){
 const auto original=source;source.rows.clear();if(source.index)source.index->labels.clear();
 for(auto i:rows){source.rows.push_back(original.rows[i]);if(source.index)source.index->labels.push_back(original.index->labels[i]);}return source;
}
inline Parts attributes_request(const std::string& engine,const EventTable& source,const std::string& operation="events",Parts extra={},std::string values="[\"A\"]"){
 Parts result={field("engine",engine),field("operation",operation),{"source_file","source.json",serialize_embedding_workbench_table_json(source),true}};
 if(operation=="traces"||operation=="events"||operation=="on_values")result.push_back(field("values_json",values));
 if(operation=="numeric_traces"||operation=="numeric_events"){result.push_back(field("lower","9007199254740993"));result.push_back(field("upper","9007199254740993"));}
 if(operation=="threshold")result.push_back(field("criterion","3"));
 if(operation=="top_activities")result.push_back(field("criterion","0"));
 if(operation=="relative")result.push_back(field("criterion","0.75"));
 result.insert(result.end(),extra.begin(),extra.end());return result;
}
inline void replace(Parts& parts,const std::string& name,std::string value){for(auto& part:parts)if(part.name==name){part.bytes=std::move(value);return;}throw std::runtime_error("missing replaced field");}
inline void table_success(const DiscoveryWorkbenchResponse& answer,const EventTable& source,const EventTable& expected,const std::string& engine){
 success(answer,source,expected,engine);contains(answer,"\"family\":\"TABLE_ATTRIBUTES_FILTER\"");contains(answer,"\"result_kind\":\"table\"");
 require(text(answer.json,"source_after_table_json")==serialize_embedding_workbench_table_json(source),"post-operation complete source changed");
 require(!text(answer.json,"counts_table_json"),"table operation returned unrelated count artifact");
}
inline void workflows(const std::function<DiscoveryWorkbenchResponse(const Parts&)>& run){
 const std::string pairs=R"({"type":"list","value":[{"type":"tuple","value":[{"type":"string","value":"A"},{"type":"integer","value":"100"}]},{"type":"tuple","value":[{"type":"string","value":"B"},{"type":"integer","value":"0"}]}]})";
 const std::string common=R"({"type":"list","value":[{"type":"string","value":"B"}]})";
 for(const std::string engine:{"pandas","polars"}){
  const auto source=source_table(engine);
  table_success(run(attributes_request(engine,source)),source,subset(source,{0,2}),engine);
  for(const auto mode:{"traces","on_values"})table_success(run(attributes_request(engine,source,mode)),source,subset(source,{0,2,3}),engine);
  table_success(run(attributes_request(engine,source,"traces",{field("positive","false")})),source,subset(source,{1}),engine);
  table_success(run(attributes_request(engine,source,"events",{field("positive","false")})),source,subset(source,{1,3}),engine);
  auto typed=attributes_request(engine,source);typed.erase(std::remove_if(typed.begin(),typed.end(),[](const auto& p){return p.name=="values_json";}),typed.end());
  typed.push_back(field("typed_values_json",R"({"type":"tuple","value":[{"type":"string","value":"A"}]})"));
  table_success(run(typed),source,subset(source,{0,2}),engine);
  auto numeric=source;for(std::size_t i=0;i<numeric.size();++i){numeric.rows[i][2]=integer("signed",9007199254740992LL+(i==0?0:i==2?2:1));numeric.rows[i][3]=unsigned_integer("unsigned",18446744073709551615ULL-(i==0?1:i==2?2:0));}
  table_success(run(attributes_request(engine,numeric,"numeric_events",{field("attribute_key","signed")})),numeric,subset(numeric,{1,3}),engine);
  table_success(run(attributes_request(engine,numeric,"numeric_traces",{field("attribute_key","signed")})),numeric,numeric,engine);
  auto unsigned_request=attributes_request(engine,numeric,"numeric_events",{field("attribute_key","unsigned")});replace(unsigned_request,"lower","18446744073709551615");replace(unsigned_request,"upper","18446744073709551615");
  table_success(run(unsigned_request),numeric,subset(numeric,{1,3}),engine);
  auto exact=attributes_request(engine,numeric,"events",{field("attribute_key","signed")});exact.erase(std::remove_if(exact.begin(),exact.end(),[](const auto& p){return p.name=="values_json";}),exact.end());exact.push_back(field("typed_values_json",R"({"type":"list","value":[{"type":"integer","value":"9007199254740993"}]})"));
  table_success(run(exact),numeric,subset(numeric,{1,3}),engine);
  auto stream=attributes_request(engine,numeric,"numeric_traces",{field("attribute_key","signed"),field("stream_filter_key1","concept:name"),field("stream_filter_value1_json",R"({"type":"string","value":"A"})")});replace(stream,"lower","9007199254740992");replace(stream,"upper","9007199254740994");
  table_success(run(stream),numeric,subset(numeric,{0,2,3}),engine);
  stream.push_back(field("stream_filter_key2","concept:name"));stream.push_back(field("stream_filter_value2_json",R"({"type":"string","value":"B"})"));
  table_success(run(stream),numeric,subset(numeric,{}),engine);
  table_success(run(attributes_request(engine,source,"threshold")),source,subset(source,{}),engine);
  table_success(run(attributes_request(engine,source,"threshold",{field("counts_json",pairs)})),source,subset(source,{0,2}),engine);
  table_success(run(attributes_request(engine,source,"threshold",{field("counts_json",pairs),field("most_common_json",common)})),source,source,engine);
  table_success(run(attributes_request(engine,source,"threshold",{field("counts_value_json",R"({"type":"null"})")})),source,subset(source,{}),engine);
  table_success(run(attributes_request(engine,source,"top_activities")),source,subset(source,engine=="pandas"?std::vector<std::size_t>{0,1,2,3}:std::vector<std::size_t>{}),engine);
  table_success(run(attributes_request(engine,source,"relative")),source,subset(source,engine=="pandas"?std::vector<std::size_t>{1,3}:std::vector<std::size_t>{0,1,2,3}),engine);
  table_success(run(attributes_request(engine,source,"relative",{field("keep_once_per_case","false")})),source,subset(source,{}),engine);
  table_success(run(attributes_request(engine,source,"relative",{field("keep_once_per_case","true")})),source,subset(source,{1,3}),engine);
  auto custom=source;const std::string key(" literal\n\0Ω ",13);custom.columns[0]="";custom.columns[1]=key;for(auto& row:custom.rows){row[0]->key="";row[1]->key=key;}
  table_success(run(attributes_request(engine,custom,"traces",{field("case_id_key",""),field("attribute_key",key)})),custom,subset(custom,{0,2,3}),engine);
  auto byte_request=attributes_request(engine,custom,"traces",{{"case_id_key","case_id_key.txt","",true},{"attribute_key","attribute_key.txt",key,true}});
  table_success(run(byte_request),custom,subset(custom,{0,2,3}),engine);
  byte_request.back().filename="unrelated.json";require(run(byte_request).status==400,"unrelated attributes File accepted as a byte field");
  const auto missing=run(attributes_request(engine,source,"events",{field("attribute_key","missing")}));require(missing.status==400,"missing matched column accepted");contains(missing,"\"source_unchanged\":true");require(text(missing.json,"source_after_table_json")==serialize_embedding_workbench_table_json(source),"failure mutated source");
  if(engine=="pandas"){
   table_success(run(attributes_request(engine,source,"threshold",{field("counts_value_json",R"({"type":"map","value":{"A":{"type":"integer","value":"100"},"B":{"type":"integer","value":"0"}}})")})),source,subset(source,{0,2}),engine);
   auto nullable=source;nullable.rows[0][1]=EventAttribute{"concept:name","",AttributeKind::kString,true};
   table_success(run(attributes_request(engine,nullable,"events",{field("keep_nan_values","true")})),nullable,subset(nullable,{0,2}),engine);
   table_success(run(attributes_request(engine,nullable,"events",{field("keep_nan_values","true"),field("positive","false")})),nullable,subset(nullable,{1,3}),engine);
   // Raw entries must behave like a Python dict, not a multimap.
   const std::string duplicate=R"({"type":"list","value":[{"type":"tuple","value":[{"type":"integer","value":"1"},{"type":"integer","value":"100"}]},{"type":"tuple","value":[{"type":"boolean","value":true},{"type":"integer","value":"0"}]}]})";
   auto numeric_labels=source;numeric_labels.dtypes[1]="int64";numeric_labels.categorical_dtypes.clear();for(auto& row:numeric_labels.rows)row[1]=integer("concept:name",1);
   table_success(run(attributes_request(engine,numeric_labels,"threshold",{field("counts_json",duplicate)})),numeric_labels,subset(numeric_labels,{}),engine);
  }else{
   for(const std::string once:{"default","false","true"}){
    auto request=attributes_request(engine,source,"counts");if(once!="default")request.push_back(field("keep_once_per_case",once));
    const auto answer=run(request);require(answer.status==200,"count projection failed: "+answer.json);contains(answer,"\"result_kind\":\"counts\"");contains(answer,"\"source_unchanged\":true");require(!text(answer.json,"filtered_table_json"),"count operation returned unrelated frame");
    const auto artifact=text(answer.json,"counts_table_json");require(artifact.has_value(),"count projection missing");const auto counts=parse_embedding_workbench_table_json(*artifact);require(counts.columns==std::vector<std::string>{"value","count"}&&counts.size()==2,"count projection shape changed");
    std::map<std::string,std::string> values;for(const auto& row:counts.rows)values[row[0]->value]=row[1]->value;require(values["A"]==(once=="true"?"1":"2")&&values["B"]=="2","tri-state count projection changed");
    require(text(answer.json,"source_table_json")==serialize_embedding_workbench_table_json(source)&&text(answer.json,"source_after_table_json")==serialize_embedding_workbench_table_json(source),"count scan lost complete source");
   }
  }
 }
}
} // namespace table_attributes_test
#ifndef MINERA_ATTRIBUTES_ROUTE_TEST
int main(){try{
 using namespace table_attributes_test;
 workflows([](const auto& parts){return execute_table_attributes_filter(parts);});
 const auto source=source_table("pandas");
 for(const auto& invalid:Parts{field("engine","pandas"),field("unknown","x"),field("positive","yes"),field("keep_once_per_case","default"),field("case_id_key","inactive"),field("stream_filter_key1","concept:name"),field("activity_key","inactive"),field("counts_json","null")})require(execute_table_attributes_filter(attributes_request("pandas",source,"events",{invalid})).status==400,"invalid/inactive control accepted");
 require(execute_table_attributes_filter(attributes_request("pandas",source,"relative",{field("keep_once_per_case","default")})).status==400,"explicit default token accepted as relative Boolean control");
 require(execute_table_attributes_filter(attributes_request("pandas",source),{field("engine","pandas")}).status==400,"query/multipart duplicate accepted");
 auto scalar=attributes_request("pandas",source);scalar.back()=field("typed_values_json",R"({"type":"null"})");const auto bad=execute_table_attributes_filter(scalar);require(bad.status==400&&text(bad.json,"error_type")=="TypeError","typed scalar algorithm diagnostic lost");contains(bad,"\"phase\":\"apply\"");
 for(const auto malformed:{"null","{}","[1]","[\"A\",]","[\"A\"]x"}){bool rejected=false;try{(void)parse_attributes_filter_values_json(malformed,false);}catch(const std::invalid_argument&){rejected=true;}require(rejected,"bad literal values accepted");}
 std::cout<<"complete attributes table workbench passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
#endif
