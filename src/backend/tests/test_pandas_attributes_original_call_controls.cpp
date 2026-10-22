#include "table_attribute_repetition_test_support.hpp"
#include "minera/table_attributes_filter_workbench.hpp"
#include "minera/pandas_attributes_filter.hpp"
#include <iostream>

namespace {
using namespace table_repetition_test;
using Parts=std::vector<DiscoveryWorkbenchPart>;
std::string call(const std::string& args,const std::string& tail={}){return "{\"schema\":\"minera-pandas-attributes-original-call-v1\",\"args\":"+args+tail+'}';}
Parts request(const EventTable& source,const std::string& operation,const std::string& json){return {field("engine","pandas"),field("operation",operation),{"source_file","misleading.csv",serialize_embedding_workbench_table_json(source),true},field("call_json",json)};}
DiscoveryWorkbenchResponse run(const EventTable& source,const std::string& operation,const std::string& json){return execute_table_attributes_filter(request(source,operation,json));}
void preserved(const DiscoveryWorkbenchResponse& answer,const EventTable& source){
  const auto before=text(answer.json,"controls_before_json"),after=text(answer.json,"controls_after_json");require(before.has_value()&&before==after,"actual parsed controls snapshots differ");contains(answer,"\"controls_unchanged\":true");
  const auto original=serialize_embedding_workbench_table_json(source);require(text(answer.json,"source_table_json")==original&&text(answer.json,"source_after_table_json")==original,"full source before/after differs");
  contains(answer,"\"source_unchanged\":true");contains(answer,"\"case_id_key\":\"\"");contains(answer,"\"attribute_key\":\"\"");contains(answer,"\"call_mode\":\"original\"");
}
void result_is(const DiscoveryWorkbenchResponse& answer,const EventTable& expected){require(answer.status==200,"original controls failed: "+answer.json);require(text(answer.json,"filtered_table_json")==serialize_embedding_workbench_table_json(expected),"complete original result mismatch");}
EventTable operands(const DiscoveryWorkbenchResponse& answer){const auto state=text(answer.json,"controls_before_json");require(state.has_value(),"missing controls snapshot");const auto artifact=text(*state,"args_json");require(artifact.has_value(),"missing actual operand artifact");return parse_embedding_workbench_table_json(*artifact);}
void controls(){
  const auto source=source_table("pandas");
  const std::string members=R"([{"type":"list","value":[{"type":"string","value":"A"}]}])";
  for(const auto& [mode,tail]:std::vector<std::pair<std::string,std::string>>{{"omitted",""},{"raw",R"(,"parameter_value":{"type":"null"})"},{"raw",R"(,"parameter_value":{"type":"list","value":[]})"},{"entries",R"(,"parameter_entries":[])"}}){
    const auto json=call(members,tail);const auto answer=run(source,"events",json);PandasAttributesFilterOptions options;EventAttribute values;values.container=AttributeContainerKind::kList;values.children={{"0","A"}};
    result_is(answer,filter_pandas_attributes_events(source,values,options));preserved(answer,source);require(text(answer.json,"call_json")==json,"original raw echo differs");require(text(*text(answer.json,"controls_before_json"),"parameter_state")==mode,"omission/raw/entries collapsed");
  }
  const auto raw_error=run(source,"events",call(members,R"(,"parameter_value":{"type":"string","value":"invalid"})"));
  require(raw_error.status==400,"invalid raw parameters accepted");contains(raw_error,"\"phase\":\"apply\"");require(text(raw_error.json,"error")=="string indices must be integers, not 'str'","raw parameter diagnostic changed");preserved(raw_error,source);
  const auto literal=[](const std::string& name,const std::string& value,bool is_enum){return "{\"key\":\""+name+"\",\"enum_key\":"+(is_enum?"true":"false")+",\"value\":{\"type\":\"string\",\"value\":\""+value+"\"}}";};
  for(const std::string operation:{"traces","events","numeric_traces","numeric_events","relative"})for(bool enum_first:{false,true}){
    const bool numeric=operation.starts_with("numeric_");const auto a=literal("pm4py:param:attribute_key","missing",false),b=literal("pm4py:param:attribute_key",numeric?"signed":"concept:name",true);
    const auto json=call(numeric?R"([{"type":"integer","value":"9007199254740993"},{"type":"integer","value":"9007199254740993"}])":operation=="relative"?R"([{"type":"decimal","value":"0.5"}])":members,",\"parameter_entries\":["+(enum_first?b+","+a:a+","+b)+"]");
    const auto answer=run(source,operation,json);preserved(answer,source);
    if(operation=="relative"){require(answer.status==400,"relative literal precedence ignored");require(text(answer.json,"error_type")=="KeyError","relative precedence diagnostic changed");}
    else{require(answer.status==200,"apply enum precedence ignored");contains(answer,"\"apply_returned\":true");}
    require(text(answer.json,"call_json")==json,"ordered enum collision echo changed");
  }
  const std::string pairs=R"({"type":"attribute_counts","entries":[[{"type":"integer","value":"1"},{"type":"integer","value":"1"}],[{"type":"boolean","value":true},{"type":"integer","value":"3"}]]})";
  const auto threshold=run(source,"threshold",call(R"([{"type":"integer","value":"2"}])",",\"keyword_entries\":[{\"key\":\"act_count0\",\"value\":"+pairs+"}]"));
  result_is(threshold,source);preserved(threshold,source);require(text(*text(threshold.json,"controls_before_json"),"parameter_state")=="not_applicable","direct helper fabricated parameters");require(text(threshold.json,"controls_before_json")->find("count_dictionary")!=std::string::npos,"actual counter union boundaries absent");
  const auto bad_counter=run(source,"threshold",call(R"([{"type":"integer","value":"2"}])",R"(,"keyword_entries":[{"key":"act_count0","value":{"type":"list","value":[]}}])"));require(bad_counter.status==400,"raw malformed count accepted");contains(bad_counter,"\"phase\":\"apply\"");preserved(bad_counter,source);
  result_is(run(source,"top_activities",call("[]")),source);
  const auto negative=run(source,"on_values",call(members,R"(,"keyword_entries":[{"key":"positive","value":{"type":"null"}}])"));require(negative.status==200,"typed direct null positive rejected");preserved(negative,source);
  const std::string timestamp_us=R"({"type":"pandas_timestamp","value":"2026-06-02T00:00:00+00:00","resolution":"us","timezone":"UTC"})";
  const std::string timestamp_ns=R"({"type":"pandas_timestamp","value":"2026-06-02T00:00:00.000000001+00:00","resolution":"ns","timezone":"UTC"})";
  EventTable dates;dates.columns={"time"};dates.dtypes={"datetime64[ns, UTC]"};EventAttribute date{"time","2026-06-02T00:00:00.000000001+00:00",AttributeKind::kDate};date.typed_value=NativeDate{date.value,NativeDateResolution::kNanosecond,std::optional<std::string>{"UTC"}};dates.rows={{date}};
  const auto temporal=run(dates,"numeric_events",call("["+timestamp_us+","+timestamp_ns+"]",R"(,"parameter_entries":[{"key":"pm4py:param:attribute_key","enum_key":false,"value":{"type":"string","value":"time"}}])"));
  require(temporal.status==200,"exact Timestamp controls rejected");preserved(temporal,dates);const auto input=operands(temporal);
  const auto& us=std::get<NativeDate>(input.rows[0][0]->typed_value);const auto& ns=std::get<NativeDate>(input.rows[0][1]->typed_value);
  require(us.resolution==NativeDateResolution::kMicrosecond&&ns.resolution==NativeDateResolution::kNanosecond&&us.timezone=="UTC"&&ns.timezone=="UTC","Timestamp precision/timezone metadata collapsed");
  const auto nested=run(source,"threshold",call(R"([{"type":"integer","value":"0"}])",",\"keyword_entries\":[{\"key\":\"act_count0\",\"value\":{\"type\":\"attribute_counts\",\"entries\":[[{\"type\":\"tuple\",\"value\":["+timestamp_us+"]},{\"type\":\"integer\",\"value\":\"1\"}]]}}]"));
  result_is(nested,source);const auto key_artifact=text(*text(nested.json,"controls_before_json"),"key_json");require(key_artifact.has_value(),"nested timestamp count key absent");const auto key_table=parse_embedding_workbench_table_json(*key_artifact);const auto& nested_date=std::get<NativeDate>(key_table.rows[0][0]->children[0].typed_value);require(nested_date.resolution==NativeDateResolution::kMicrosecond&&nested_date.timezone=="UTC","recursive timestamp metadata collapsed");
  const std::string exact="\r\n "+call(R"([{"type":"integer","value":9007199254740993},{"type":"integer","value":"9007199254740993"}])",R"(,"parameter_entries":[{"key":"pm4py:param:attribute_key","enum_key":false,"value":{"type":"string","value":"signed"}}])")+"\r\n";
  auto bytes=request(source,"numeric_events",exact);bytes.back().file=true;bytes.back().filename="call_json.txt";const auto byte_answer=execute_table_attributes_filter(bytes);require(byte_answer.status==200&&text(byte_answer.json,"call_json")==exact,"canonical byte field or large token/CRLF changed");
  for(const auto& json:std::vector<std::string>{"",R"({"schema":"bad","args":[]})",R"({"schema":"minera-pandas-attributes-original-call-v1","args":[],"args":[]})",call("[]"),call(members,R"(,"parameter_value":{"type":"null"},"parameter_entries":[])"),call(members,R"(,"keyword_entries":[])"),call(members,R"(,"parameter_entries":[{"key":"positive","enum_key":false,"value":{"type":"boolean","value":true}},{"key":"positive","enum_key":false,"value":{"type":"boolean","value":false}}])")}){
    const auto bad=run(source,"events",json);require(bad.status==400,"malformed original call accepted");contains(bad,"\"phase\":\"control\"");require(text(bad.json,"call_json")==json,"malformed call echo lost");contains(bad,"\"controls_before_json\":null");
  }
  for(const std::string bad_timestamp:{R"({"type":"pandas_timestamp","value":"2026-06-02T00:00:00+00:00","resolution":"ms","timezone":"UTC"})",R"({"type":"pandas_timestamp","value":"2026-06-02T00:00:00.000000001+00:00","resolution":"us","timezone":"UTC"})",R"({"type":"pandas_timestamp","value":"2026-06-02T00:00:00+01:00","resolution":"ns","timezone":"UTC"})"})require(run(source,"events",call("["+bad_timestamp+"]")).status==400,"incoherent Timestamp metadata accepted");
  for(const auto& [op,json]:std::vector<std::pair<std::string,std::string>>{{"top_activities",call(R"([{"type":"integer","value":"1"}])")},{"threshold",call(R"([{"type":"integer","value":"1"}])",R"(,"parameter_entries":[])" )},{"threshold",call(R"([{"type":"integer","value":"1"}])",R"(,"keyword_entries":[{"key":"act_count0","value":{"type":"map","value":{}}}])")}})require(run(source,op,json).status==400,"unsupported direct binding accepted");
  auto mixed=request(source,"events",call(members));mixed.push_back(field("positive","true"));const auto mixed_answer=execute_table_attributes_filter(mixed);require(mixed_answer.status==400&&text(mixed_answer.json,"call_json")==call(members),"mixed friendly controls accepted/echo lost");
  auto wrong_engine=request(source,"events",call(members));wrong_engine[0].bytes="polars";const auto polars=execute_table_attributes_filter(wrong_engine);require(polars.status==400&&text(polars.json,"call_json")==call(members),"Polars original mode accepted/echo lost");
  auto unknown=request(source,"events",call(members));unknown.push_back(field("unknown","x"));const auto invalid=execute_table_attributes_filter(unknown);require(invalid.status==400,"unknown control accepted");contains(invalid,"\"call_mode\":null");contains(invalid,"\"call_json\":null");
  auto noncanonical=bytes;noncanonical.back().filename="other.json";const auto wrong_file=execute_table_attributes_filter(noncanonical);require(wrong_file.status==400,"noncanonical call File accepted");contains(wrong_file,"\"call_mode\":null");
  const auto duplicate=execute_table_attributes_filter(request(source,"events",call(members)),{field("call_json",call(members))});require(duplicate.status==400,"duplicate query call accepted");contains(duplicate,"\"call_json\":null");
  auto invalid_source=request(source,"events",call(members));invalid_source[2].bytes="bad native table";const auto source_error=execute_table_attributes_filter(invalid_source);require(source_error.status==400,"bad source accepted");contains(source_error,"\"phase\":\"source_import\"");contains(source_error,"\"controls_unchanged\":true");
  for(const std::string engine:{"pandas","polars"}){const auto friendly=execute_table_attributes_filter({field("engine",engine),field("operation","events"),field("values_json","[\"A\"]"),{"source_file","source.json",serialize_embedding_workbench_table_json(source_table(engine)),true}});require(friendly.status==200,"friendly attributes regression failed");contains(friendly,"\"call_mode\":\"friendly\"");contains(friendly,"\"call_json\":null");contains(friendly,"\"controls_before_json\":null");contains(friendly,"\"controls_unchanged\":null");}
}
}
int main(){try{controls();std::cout<<"pandas attributes original-call strict controls, temporal carriers and full artifacts passed\n";}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
