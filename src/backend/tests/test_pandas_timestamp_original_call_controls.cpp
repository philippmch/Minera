#include "../pandas_timestamp_original_call_workbench.hpp"
#include "../polars_ltl_original_call_parser.hpp"
#include <iostream>
namespace {
namespace o=minera::pandas_timestamp_original_call_detail;
using namespace minera;
void require(bool v,const char* m){if(!v)throw std::runtime_error(m);}
const std::string enum_key=R"({"type":"enum","module":"pm4py.algo.filtering.pandas.timestamp.timestamp_filter","name":"TIMESTAMP_KEY","value":{"type":"str","value":"pm4py:param:timestamp_key"}})";
std::string call(const std::string& args,const std::string& keywords="[]"){return "{\"schema\":\"minera-pandas-timestamp-original-call-v1\",\"args\":["+args+"],\"keyword_entries\":"+keywords+'}';}
const std::string window=R"({"type":"str","value":"2026-05-04 00:00:00"},{"type":"str","value":"2026-05-04 00:01:00"})";
std::string table(){
  EventTable t;t.columns={"case:concept:name","time:timestamp"};t.dtypes={"object","datetime64[ns, UTC]"};
  for(const auto& [c,time]:std::vector<std::pair<std::string,std::string>>{{"a","2026-05-04T00:00:10+00:00"},{"b","2026-05-04T00:02:00+00:00"}}){EventAttribute d{"time:timestamp",time,AttributeKind::kDate};d.typed_value=NativeDate{time,NativeDateResolution::kNanosecond,"UTC"};t.rows.push_back({EventAttribute{"case:concept:name",c},d});}
  return serialize_embedding_workbench_table_json(t);
}
DiscoveryWorkbenchResponse run(const std::string& operation,const std::string& descriptor,std::vector<DiscoveryWorkbenchPart> extra={}){
  std::vector<DiscoveryWorkbenchPart> parts={{"engine","","pandas",false},{"operation","",operation,false},{"source_file","source.json",table(),true},{"call_json","call_json.txt",descriptor,true}};
  for(auto& p:extra)parts.push_back(std::move(p));return execute_table_timestamp_filter(parts);
}
void rejected(const std::string& operation,const std::string& descriptor,const char* message,std::vector<DiscoveryWorkbenchPart> extra={}){
  const auto r=run(operation,descriptor,std::move(extra));require(r.status==400&&r.json.find("\"phase\":\"control\"")!=std::string::npos&&r.json.find("\"error_type\":\"native_error\"")!=std::string::npos,message);
}
void check(){
  const auto omitted=o::parse(call(window),"apply_events");require(omitted.args.size()==2&&omitted.raw_parameters.is_null&&!omitted.parameters&&omitted.parameter_state=="omitted","omitted parameters changed");
  const auto mapping=o::parse(call(window,R"([{"key":"parameters","value":{"type":"dict","items":[[{"type":"str","value":"pm4py:param:timestamp_key"},{"type":"str","value":"time:timestamp"}]]}}])"),"apply_events");
  require(!mapping.parameters&&mapping.parameter_state=="mapping"&&mapping.raw_parameters.children.at(0).key=="pm4py:param:timestamp_key","string mapping lost raw container arm");
  const auto entries=o::parse(call(window,"[{\"key\":\"parameters\",\"value\":{\"type\":\"dict\",\"items\":[["+enum_key+R"(,{"type":"str","value":"time:timestamp"}],[{"type":"str","value":"pm4py:param:timestamp_key"},{"type":"str","value":"missing"}]]}}])"),"apply_events");
  require(entries.parameters&&entries.parameters->size()==2&&entries.parameters->at(0).enum_key&&!entries.parameters->at(1).enum_key&&entries.parameter_state=="entries","ordered enum identity changed");
  const auto helper=o::parse(call("","[{\"key\":\"parameters\",\"value\":{\"type\":\"dict\",\"items\":[["+enum_key+R"(,{"type":"str","value":"x"}]]}}])"),"apply");
  require(!helper.parameters&&helper.parameter_state=="entries"&&helper.raw_parameters.container==AttributeContainerKind::kContainer,"apply helper must keep its raw value arm");
  const auto projection=o::shared::Reader(o::projection_json(helper,"apply")).read();require(o::at(projection,"parameter_native_arm").text=="raw_attribute"&&o::at(projection,"native_function").text=="apply_pandas_timestamp_filter","projection changed");
  const auto bom=o::parse(std::string("\xef\xbb\xbf",3)+"\r\n"+call(R"({"type":"str","value":"concept:name"},{"type":"int","value":9007199254740993},)"+window),"filter_traces_attribute_in_timeframe");
  require(bom.args.at(1).value=="9007199254740993","BOM or wide integer slice changed");
  for(const auto& bad:std::vector<std::pair<std::string,std::string>>{
      {"apply_events",call(R"({"type":"str","value":"2026-05-04 00:00:00"})")},
      {"apply",call(window)},
      {"apply_events",call(window,R"([{"key":"params","value":{"type":"NoneType","value":null}}])")},
      {"apply_events",call(window,R"([{"key":"parameters","value":{"type":"NoneType","value":null}},{"key":"parameters","value":{"type":"NoneType","value":null}}])")},
      {"apply_events",call(window,R"([{"key":"parameters","value":{"type":"dict","items":[[{"type":"int","value":1},{"type":"str","value":"x"}]]}}])")},
      {"apply_events",call(window,R"([{"key":"parameters","value":{"type":"dict","items":[[{"type":"enum","module":"pm4py.algo.filtering.pandas.variants.variants_filter","name":"TIMESTAMP_KEY","value":{"type":"str","value":"pm4py:param:timestamp_key"}},{"type":"str","value":"x"}]]}}])")},
      {"apply_events",call(window,R"([{"key":"parameters","value":{"type":"pandas.DataFrame","file":"source_file"}}])")},
      {"apply_events",R"({"schema":"minera-pandas-variants-original-call-v1","args":[],"keyword_entries":[]})"}}){
    bool failed=false;try{(void)o::parse(bad.second,bad.first);}catch(const std::invalid_argument&){failed=true;}require(failed,"malformed original pandas timestamp descriptor accepted");
  }
  const auto ok=run("apply_events",call(window));require(ok.status==200&&ok.json.find("\"call_mode\":\"original\"")!=std::string::npos&&ok.json.find("\"phase\":\"complete\"")!=std::string::npos&&ok.json.find("\"rows\":1,")!=std::string::npos,"window call failed");
  const auto apply=run("apply",call(""));require(apply.status==400&&apply.json.find("\"phase\":\"apply\"")!=std::string::npos&&apply.json.find("\"error_type\":\"native_error\"")==std::string::npos,"unavailable helper must keep its native diagnostic");
  rejected("apply_events",call(window),"friendly control accepted in original mode",{{"lower","","2026-05-04 00:00:00",false}});
  rejected("filter_traces_starting",call(window),"unknown operation accepted");
  rejected("apply_events","{","malformed JSON accepted");
  const auto polars=execute_table_timestamp_filter({{"engine","","polars",false},{"operation","","apply_events",false},{"source_file","source.json",table(),true},{"call_json","call_json.txt",call(window),true}});
  require(polars.status==400&&polars.json.find("\"engine\":null")!=std::string::npos&&polars.json.find("\"call_json\":")!=std::string::npos,"foreign engine accepted or raw call lost");
  const auto query=execute_table_timestamp_filter({{"engine","","pandas",false},{"operation","","apply_events",false},{"call_json","call_json.txt",call(window),true}},{{"source_file","","{}",false}});
  require(query.status==400&&query.json.find("must be a multipart File")!=std::string::npos,"query source accepted");
}
}
int main(){try{check();std::cout<<"original pandas timestamp descriptors, arms and workbench controls passed\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}return 0;}
