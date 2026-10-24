#include "../polars_timestamp_original_call_workbench.hpp"
#include "../polars_ltl_original_call_parser.hpp"
#include <iostream>
namespace {
namespace o=minera::polars_timestamp_original_call_detail;
using namespace minera;
void require(bool v,const char* m){if(!v)throw std::runtime_error(m);}
const std::string enum_key=R"({"type":"enum","module":"pm4py.algo.filtering.polars.timestamp.timestamp_filter","name":"TIMESTAMP_KEY","value":{"type":"str","value":"pm4py:param:timestamp_key"}})";
const std::string dt1=R"({"key":"dt1","value":{"type":"str","value":"2039-06-01 08:00:00"}})",dt2=R"({"key":"dt2","value":{"type":"str","value":"2039-06-01 08:01:00"}})";
std::string call(const std::string& kind,const std::string& entries){return "{\"schema\":\"minera-polars-timestamp-original-call-v1\",\"source_kind\":\""+kind+"\",\"keyword_entries\":["+entries+"]}";}
std::string table(){
  EventTable t;t.columns={"case:concept:name","time:timestamp"};t.dtypes={"String","Datetime(time_unit='us', time_zone='UTC')"};
  for(const auto& [c,time]:std::vector<std::pair<std::string,std::string>>{{"a","2039-06-01T08:00:10+00:00"},{"b","2039-06-01T08:02:00+00:00"}}){EventAttribute d{"time:timestamp",time,AttributeKind::kDate};d.typed_value=NativeDate{time};t.rows.push_back({EventAttribute{"case:concept:name",c},d});}
  return serialize_embedding_workbench_table_json(t);
}
DiscoveryWorkbenchResponse run(const std::string& operation,const std::string& descriptor,std::vector<DiscoveryWorkbenchPart> extra={}){
  std::vector<DiscoveryWorkbenchPart> parts={{"engine","","polars",false},{"operation","",operation,false},{"source_file","source.json",table(),true},{"call_json","call_json.txt",descriptor,true}};
  for(auto& p:extra)parts.push_back(std::move(p));return execute_table_timestamp_filter(parts);
}
void check(){
  const auto lazy=o::parse(call("LazyFrame",dt2+","+dt1),"apply_events");require(!lazy.eager&&lazy.keyword_order==std::vector<std::string>{"dt2","dt1"}&&lazy.raw_parameters.is_null&&lazy.parameter_state=="omitted","keyword order or omitted parameters changed");
  const auto mapping=o::parse(call("DataFrame",dt1+","+dt2+R"(,{"key":"parameters","value":{"type":"dict","items":[[{"type":"str","value":"positive"},{"type":"bool","value":false}]]}})"),"filter_traces_contained");
  require(mapping.eager&&mapping.parameters&&mapping.parameters->size()==1&&!mapping.parameters->at(0).enum_key&&mapping.parameter_state=="mapping","string dictionary must use the ordered binding vector");
  const auto helper=o::parse(call("LazyFrame","{\"key\":\"parameters\",\"value\":{\"type\":\"dict\",\"items\":[["+enum_key+R"(,{"type":"str","value":"x"}]]}})"),"apply");
  require(!helper.parameters&&helper.raw_parameters.children.at(0).key=="pm4py:param:timestamp_key"&&helper.parameter_state=="entries","helper must flatten enum keys to one raw value");
  const auto wide=o::parse(call("LazyFrame",R"({"key":"attribute","value":{"type":"str","value":"n"}},{"key":"attribute_value","value":{"type":"int","value":18446744073709551615}},)"+dt1+","+dt2),"filter_traces_attribute_in_timeframe");
  require(std::get<std::uint64_t>(wide.operands.at("attribute_value").typed_value)==UINT64_MAX,"uint64 operand changed");
  const auto projection=o::shared::Reader(o::projection_json(mapping,"filter_traces_contained")).read();require(o::shared::field(projection,"native_function")->text=="filter_polars_timestamp_contained_parameters","projection native function changed");
  for(const auto& bad:std::vector<std::pair<std::string,std::string>>{
      {"apply_events",call("LazyFrame",dt1)},{"apply_events",call("Series",dt1+","+dt2)},{"apply_events",call("LazyFrame",dt1+","+dt1+","+dt2)},
      {"apply_events",call("LazyFrame",dt1+","+dt2+R"(,{"key":"attribute","value":{"type":"str","value":"x"}})")},
      {"apply",call("LazyFrame",dt1)},{"apply_events",call("LazyFrame",R"({"key":"dt1","value":{"type":"int","value":18446744073709551616}},)"+dt2)},
      {"apply_events",call("LazyFrame",R"({"key":"dt1","value":{"type":"date","iso":"2039-6-1"}},)"+dt2)},
      {"apply_events",R"({"schema":"minera-pandas-timestamp-original-call-v1","args":[],"keyword_entries":[]})"}}){
    bool failed=false;try{(void)o::parse(bad.second,bad.first);}catch(const std::invalid_argument&){failed=true;}require(failed,"malformed original Polars timestamp descriptor accepted");
  }
  const auto ok=run("apply_events",call("LazyFrame",dt1+","+dt2));require(ok.status==200&&ok.json.find("\"engine\":\"polars\"")!=std::string::npos&&ok.json.find("\"phase\":\"complete\"")!=std::string::npos&&ok.json.find("\"result\":{\"rows\":1,")!=std::string::npos,"lazy window call failed");
  const auto eager=run("filter_traces_contained",call("DataFrame",dt1+","+dt2));require(eager.status==200&&eager.json.find("\"result\":{\"rows\":1,")!=std::string::npos,"eager window call failed");
  const auto helper_run=run("apply",call("LazyFrame",""));require(helper_run.status==400&&helper_run.json.find("\"error_type\":\"Exception\"")!=std::string::npos&&helper_run.json.find("\"apply_returned\":false")!=std::string::npos,"helper diagnostic changed");
  const auto inactive=run("apply_events",call("LazyFrame",dt1+","+dt2),{{"lower","","x",false}});require(inactive.status==400&&inactive.json.find("\"phase\":\"control\"")!=std::string::npos,"friendly control accepted in original mode");
  const auto starting=run("filter_traces_starting_in_timeframe",call("LazyFrame",dt1+","+dt2));require(starting.status==400&&starting.json.find("\"operation\":null")!=std::string::npos,"pandas-only operation accepted for Polars");
}
}
int main(){try{check();std::cout<<"original Polars timestamp descriptors, arms and workbench controls passed\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}return 0;}
