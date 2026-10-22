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
inline void common_success(const DiscoveryWorkbenchResponse& answer,const std::string& source,const std::string& expected,const std::string& engine,const std::string& input_kind="LazyFrame"){
 require(answer.status==200,"relationship success expected: "+answer.json);contains(answer,"\"family\":\"REFERENCE_LTL_FILTER\"");contains(answer,"\"phase\":\"complete\"");contains(answer,"\"apply_returned\":true");contains(answer,"\"source_unchanged\":true");require(text(answer.json,"engine")==engine,"relationship engine changed");require(text(answer.json,"source_json")==source,"relationship full parsed source changed");require(text(answer.json,"source_after_json")==source,"relationship full source-after changed");require(text(answer.json,"result_json")==expected,"relationship complete result mismatch");require(!text(answer.json,"error"),"relationship unexpected diagnostic");
 require(text(answer.json,"input_kind")== (engine=="polars"?std::optional<std::string>{input_kind}:std::nullopt),"relationship effective Polars input kind changed");
 require(text(answer.json,"call_mode")=="friendly" && !native_text(answer.json,"call_json"),"friendly request changed call mode or acquired original call text");
}
inline Parts original_request(const std::string& source,const std::string& raw,const std::string& operation="eventually",const std::string& kind="LazyFrame"){
 return {field("engine","polars"),field("operation",operation),field("input_kind",kind),{"source_file","source.json",source,true},field("call_json",raw)};
}
inline void original_echo(const DiscoveryWorkbenchResponse& answer,const std::string& raw){
 require(text(answer.json,"call_mode")=="original" && native_text(answer.json,"call_json")==raw,"original call bytes or mode changed");
 for(const std::string name:{"case_id_key","attribute_key","resource_key","timestamp_key"})require(!text(answer.json,name),"original response fabricated friendly role "+name);
}
inline void original_polars_calls(const std::function<DiscoveryWorkbenchResponse(const Parts&)>& run){
 const auto table=sample_table("polars");const auto source=serialize_embedding_workbench_table_json(table);
 const std::string schema=R"("schema":"minera-polars-ltl-original-call-v1")";
 const std::string values=R"("values":{"type":"list","value":[{"type":"string","value":"A"},{"type":"string","value":"B"},{"type":"string","value":"C"}]})";
 const auto call=[&](const std::string& extra){return "{"+schema+","+values+extra+"}";};
 const auto expected=serialize_embedding_workbench_table_json(subset(table,{0,1,2}));
 const auto success=[&](const DiscoveryWorkbenchResponse& answer,const std::string& raw,const std::string& result){
  require(answer.status==200,"original call success expected: "+answer.json);original_echo(answer,raw);contains(answer,"\"phase\":\"complete\"");contains(answer,"\"apply_returned\":true");contains(answer,"\"source_unchanged\":true");
  require(text(answer.json,"source_json")==source && text(answer.json,"source_after_json")==source && text(answer.json,"result_json")==result && !text(answer.json,"error"),"original full source/result artifacts changed");
 };
 for(const std::string kind:{"LazyFrame","DataFrame"}){
  const auto raw=call("");const auto answer=run(original_request(source,raw,"eventually",kind));success(answer,raw,expected);require(text(answer.json,"input_kind")==kind,"original effective input kind changed");
 }
 success(run(original_request(source,call(",\"parameter_entries\":[]"))),call(",\"parameter_entries\":[]"),expected);
 for(const std::string parameter:{R"({"type":"null"})",R"({"type":"list","value":[]})"}){
  const auto raw=call(",\"parameter_value\":"+parameter);success(run(original_request(source,raw)),raw,expected);
 }
 // Enum/string identities coexist in either insertion order; native enum wins.
 const std::string literal=R"({"key":"positive","enum_key":false,"value":{"type":"boolean","value":false}})",enumerated=R"({"key":"positive","enum_key":true,"value":{"type":"boolean","value":true}})";
 for(const bool reverse:{false,true}){
  const auto raw=call(",\"parameter_entries\":["+(reverse?enumerated+","+literal:literal+","+enumerated)+"]");success(run(original_request(source,raw)),raw,expected);
 }
 const auto negative=call(",\"parameter_entries\":["+literal+"]");success(run(original_request(source,negative)),negative,serialize_embedding_workbench_table_json(subset(table,{3,4,5,6,7})));
 // Raw non-map parameters are native semantics, including false and zero.
 for(const auto& pair:std::vector<std::pair<std::string,std::string>>{
  {R"({"type":"boolean","value":false})","'bool' object is not iterable"},{R"({"type":"boolean","value":true})","'bool' object is not iterable"},
  {R"({"type":"integer","value":"0"})","'int' object is not iterable"},{R"({"type":"string","value":"x"})","string indices must be integers, not 'str'"},
  {R"({"type":"list","value":[{"type":"integer","value":"1"}]})","list indices must be integers or slices, not str"}}){
  const auto raw=call(",\"parameter_value\":"+pair.first);const auto answer=run(original_request(source,raw));original_echo(answer,raw);require(answer.status==400 && text(answer.json,"error_type")=="TypeError" && text(answer.json,"error")==pair.second,"raw parameter diagnostic was coerced or lost");contains(answer,"\"phase\":\"apply\"");contains(answer,"\"apply_returned\":false");require(text(answer.json,"source_json")==source && text(answer.json,"source_after_json")==source,"native raw parameter failure lost imported source");
 }
 for(const std::string operation:{"eventually","next","four_eyes","different_persons"}){
  const auto raw="{"+schema+"}";const auto answer=run(original_request(source,raw,operation));original_echo(answer,raw);require(answer.status==400 && text(answer.json,"error_type")=="TypeError" && text(answer.json,"error")->find("missing 1 required positional argument")!=std::string::npos,"missing operand did not reach native call");contains(answer,"\"phase\":\"apply\"");contains(answer,"\"apply_returned\":false");require(text(answer.json,"source_json")==source && text(answer.json,"source_after_json")==source,"missing operand rejected before import");
 }
 const auto explicit_null="{"+schema+",\"values\":{\"type\":\"null\"}}";const auto null_answer=run(original_request(source,explicit_null));original_echo(null_answer,explicit_null);require(null_answer.status==400 && text(null_answer.json,"error")->find("missing 1 required positional argument")==std::string::npos,"present null became missing operand");contains(null_answer,"\"phase\":\"apply\"");
 const auto eager_next=run(original_request(source,call(""),"next","DataFrame"));original_echo(eager_next,call(""));require(eager_next.status==400 && text(eager_next.json,"error")=="'DataFrame' object has no attribute 'collect'","original eager call silently made lazy");contains(eager_next,"\"apply_returned\":false");
 const auto missing_column=call(R"(,"parameter_entries":[{"key":"pm4py:param:attribute_key","enum_key":true,"value":{"type":"string","value":"missing"}}])");
 for(const std::string kind:{"LazyFrame","DataFrame"}){
  const auto answer=run(original_request(source,missing_column,"eventually",kind));original_echo(answer,missing_column);require(answer.status==400,"original missing column succeeded");contains(answer,kind=="LazyFrame"?"\"phase\":\"collect\"":"\"phase\":\"apply\"");contains(answer,kind=="LazyFrame"?"\"apply_returned\":true":"\"apply_returned\":false");require(text(answer.json,"source_json")==source && text(answer.json,"source_after_json")==source,"original column error lost full source");
 }
 // Preserve CRLF, NUL and escaped Unicode exactly, including malformed bytes.
 const auto exact="{\r\n"+schema+","+values+R"(,"parameter_entries":[{"key":"ignored\u0000\u03a9","enum_key":false,"value":{"type":"string","value":"literal\r\n\u0000\u03a9"}}])"+"\r\n}";
 auto file=original_request(source,exact);file.back()={"call_json","call_json.txt",exact,true};success(run(file),exact,expected);
 const auto malformed_bytes=std::string("{\r\n\0",4);file.back().bytes=malformed_bytes;const auto malformed=run(file);require(malformed.status==400 && !text(malformed.json,"source_json"),"malformed original control reached source import");original_echo(malformed,malformed_bytes);
 const std::vector<std::string> invalid={"","null","[]","{}","{"+schema+",}","{"+schema+R"(,"unknown":null})",
  "{"+schema+R"(,"schema":"minera-polars-ltl-original-call-v1"})","{"+schema+R"(,"\u0073chema":"minera-polars-ltl-original-call-v1"})",
  "{"+schema+R"(,"values":{"type":"list","value":[{"type":"string","value":"A","\u0076alue":"B"}]}})",
  call(R"(,"parameter_value":{"type":"null"},"parameter_entries":[])"),call(R"(,"parameter_value":{"type":"map","value":{}})"),
  call(R"(,"parameter_entries":null)"),call(R"(,"parameter_entries":[{"key":"unknown","enum_key":true,"value":{"type":"null"}}])"),
  call(R"(,"parameter_entries":[{"key":7,"enum_key":false,"value":{"type":"null"}}])"),
  call(R"(,"parameter_entries":[{"key":"positive","enum_key":false,"value":{"type":"null"}},{"key":"positive","enum_key":false,"value":{"type":"boolean","value":true}}])"),
  "{"+schema+R"(,"values":{"type":"map","value":{"B":{"type":"null"},"A":{"type":"null"}}}})",
  "{"+schema+R"(,"values":{"type":"decimal","value":NaN}})","{"+schema+R"(,"values":{"type":"integer","value":01}})"};
 for(const auto& raw:invalid){const auto answer=run(original_request("invalid native source",raw));original_echo(answer,raw);require(answer.status==400 && !text(answer.json,"source_json"),"invalid original call reached source import");contains(answer,"\"phase\":\"control\"");}
 for(const std::string name:{"values_json","typed_values_json","target_json","a_json","b_json","c_json","case_id_key","attribute_key","resource_key","timestamp_key","positive","enable_timestamp","timestamp_diff_boundaries_json","helper_a_json","helper_b_json","source_format","activity_key","case_id_column","activity_column","timestamp_column","resource_column","delimiter","trim_fields"}){
  auto parts=original_request("invalid native source",call(""));parts.push_back(field(name,""));const auto answer=run(parts);original_echo(answer,call(""));require(answer.status==400 && !text(answer.json,"source_json"),"empty friendly control accepted with original call");contains(answer,"\"phase\":\"control\"");
 }
 for(const std::string engine:{"log","pandas"}){
  auto parts=original_request("invalid native source",call(""));parts[0].bytes=engine;parts.erase(parts.begin()+2);const auto answer=run(parts);original_echo(answer,call(""));require(answer.status==400 && text(answer.json,"error")=="call_json requires the polars engine" && !text(answer.json,"source_json"),"inactive original mode reached source import");
 }
 auto wrong=original_request(source,call(""));wrong.back()={"call_json","wrong.txt",call(""),true};const auto wrong_answer=run(wrong);require(wrong_answer.status==400 && !text(wrong_answer.json,"call_mode") && !native_text(wrong_answer.json,"call_json"),"noncanonical original File accepted or captured");
 auto duplicate=original_request(source,call(""));duplicate.push_back(field("call_json",call("")));require(run(duplicate).status==400,"duplicate original call accepted");
 const auto helper=run(original_request("invalid native source",call(""),"timestamp_ge"));original_echo(helper,call(""));require(helper.status==400 && !text(helper.json,"source_json"),"original mode accepted helper operation");
}
inline void polars_input_kinds(const std::function<DiscoveryWorkbenchResponse(const Parts&)>& run){
 auto table=sample_table("polars");
 // The retained ordinary eager/lazy pair distinguishes case-sorted output from
 // original row order. This independent transport fixture has the same split.
 for(std::size_t i=0;i<6;++i)table.rows[i][0]->value=i<3?"z":"a";
 const auto source=serialize_embedding_workbench_table_json(table);
 const auto sequence=[&](Parts extra){auto parts=request("polars",source,"eventually",std::move(extra));for(auto& part:parts)if(part.name=="values_json")part.bytes="[\"A\",\"B\"]";return parts;};
 const auto lazy_expected=serialize_embedding_workbench_table_json(subset(table,{0,1,2,3,4,5}));
 const auto eager_expected=serialize_embedding_workbench_table_json(subset(table,{3,4,5,0,1,2}));
 const auto implicit=run(sequence({})),explicit_lazy=run(sequence({field("input_kind","LazyFrame")}));
 common_success(implicit,source,lazy_expected,"polars");common_success(explicit_lazy,source,lazy_expected,"polars");
 require(implicit.json==explicit_lazy.json,"explicit lazy selection changed default response bytes");
 common_success(run(sequence({field("input_kind","DataFrame")})),source,eager_expected,"polars","DataFrame");
 common_success(run(sequence({{"input_kind","input_kind.txt","DataFrame",true}})),source,eager_expected,"polars","DataFrame");
 const auto ordinary=serialize_embedding_workbench_table_json(sample_table("polars"));
 for(const std::string operation:{"four_eyes","different_persons"}){
  const auto selected=operation=="different_persons"?std::vector<std::size_t>{6,7}:std::vector<std::size_t>{0,1,2};
  common_success(run(request("polars",ordinary,operation,{field("input_kind","DataFrame")})),ordinary,serialize_embedding_workbench_table_json(subset(sample_table("polars"),selected)),"polars","DataFrame");
 }
 const auto eager_next=run(request("polars",ordinary,"next",{field("input_kind","DataFrame")}));
 require(eager_next.status==400,"eager Polars NEXT silently converted to lazy input");contains(eager_next,"\"phase\":\"apply\"");contains(eager_next,"\"apply_returned\":false");contains(eager_next,"\"source_unchanged\":true");
 require(text(eager_next.json,"input_kind")=="DataFrame" && text(eager_next.json,"error_type")=="AttributeError" && text(eager_next.json,"error")=="'DataFrame' object has no attribute 'collect'","eager Polars NEXT exact apply diagnostic changed");
 require(text(eager_next.json,"source_json")==ordinary && text(eager_next.json,"source_after_json")==ordinary && !text(eager_next.json,"result_json"),"eager apply error lost complete source or fabricated result");
 const auto lazy_next=run(request("polars",ordinary,"next",{field("input_kind","LazyFrame")}));require(lazy_next.status==200 && text(lazy_next.json,"input_kind")=="LazyFrame","lazy NEXT no longer succeeds");
 for(const std::string kind:{"DataFrame","LazyFrame"}){
  const auto answer=run(request("polars",ordinary,"eventually",{field("input_kind",kind),field("attribute_key","missing")}));
  require(answer.status==400 && text(answer.json,"input_kind")==kind,"native diagnostic lost effective input kind");contains(answer,kind=="DataFrame"?"\"phase\":\"apply\"":"\"phase\":\"collect\"");contains(answer,kind=="DataFrame"?"\"apply_returned\":false":"\"apply_returned\":true");
  require(text(answer.json,"source_json")==ordinary && text(answer.json,"source_after_json")==ordinary,"typed failure lost source snapshots");
  auto malformed=request("polars","not native table JSON","eventually",{field("input_kind",kind)});const auto imported=run(malformed);
  require(imported.status==400 && text(imported.json,"input_kind")==kind,"source import error lost validated input kind");contains(imported,"\"phase\":\"source_import\"");
 }
 for(const std::string invalid:{"","eager","lazy","dataframe","DataFrame\n"}){
  const auto answer=run(request("polars","not native table JSON","eventually",{field("input_kind",invalid)}));
  require(answer.status==400 && !text(answer.json,"input_kind"),"invalid Polars input kind accepted or reported effective");contains(answer,"\"phase\":\"control\"");require(text(answer.json,"error")=="input_kind must be LazyFrame or DataFrame","invalid input kind did not reject before source import");
 }
 for(const std::string engine:{"log","pandas"}){
  const auto answer=run(request(engine,"not native source JSON","eventually",{field("input_kind","DataFrame")}));
  require(answer.status==400 && !text(answer.json,"input_kind"),"inactive input kind accepted for non-Polars engine");contains(answer,"\"phase\":\"control\"");require(text(answer.json,"error")=="input_kind requires the polars engine","inactive input kind reached source import");
 }
 const auto duplicate=run(request("polars",ordinary,"eventually",{field("input_kind","DataFrame"),field("input_kind","LazyFrame")}));require(duplicate.status==400 && !text(duplicate.json,"input_kind"),"duplicate input kind accepted");contains(duplicate,"\"phase\":\"control\"");
 const auto wrong_file=run(sequence({{"input_kind","wrong.txt","DataFrame",true}}));require(wrong_file.status==400 && !text(wrong_file.json,"input_kind"),"noncanonical input-kind File accepted");
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
 polars_input_kinds(run);
 original_polars_calls(run);
}
}
#ifndef MINERA_LTL_ROUTE_TEST
int main(){try{using namespace ltl_workbench_test;workflows([](const auto& parts){return execute_ltl_filter_workbench(parts);});const auto source=serialize_embedding_workbench_table_json(sample_table("polars"));
 for(const auto& invalid:Parts{field("unknown","x"),field("engine","polars"),field("positive","yes"),field("resource_key","inactive"),field("timestamp_key","inactive"),field("a_json","inactive"),field("helper_a_json","inactive"),field("source_format","native_log")})require(execute_ltl_filter_workbench(request("polars",source,"eventually",{invalid})).status==400,"invalid/inactive relationship control accepted");
 require(execute_ltl_filter_workbench(request("polars",source),{field("engine","polars")}).status==400,"duplicate query/multipart relationship control accepted");
 const auto duplicate_kind=execute_ltl_filter_workbench(request("polars",source,"eventually",{field("input_kind","DataFrame")}),{field("input_kind","LazyFrame")});require(duplicate_kind.status==400 && !text(duplicate_kind.json,"input_kind"),"duplicate query/multipart input kind accepted");
 const auto duplicate_call=execute_ltl_filter_workbench(original_request(source,"{}"),{field("call_json","{}")});require(duplicate_call.status==400 && !text(duplicate_call.json,"call_mode") && !native_text(duplicate_call.json,"call_json"),"duplicate query/multipart original call accepted or captured");
 auto scalar=request("polars",source);scalar[2].file=false;require(execute_ltl_filter_workbench(scalar).status==400,"plain source text accepted as File");
 std::cout<<"full relationship artifacts and strict control validation passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
#endif
