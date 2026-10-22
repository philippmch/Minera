#include "table_variants_filter_test_support.hpp"
#include "../polars_variants_original_call_parser.hpp"
#include <iostream>
namespace polars_variants_original_test {
using namespace table_variants_test;
namespace o=minera::polars_variants_original_call_detail;
using Parts=std::vector<DiscoveryWorkbenchPart>;
inline std::string call(const std::string& keywords="[]",const std::string& kind="LazyFrame"){return "{\"schema\":\"minera-polars-variants-original-call-v1\",\"input_kind\":\""+kind+"\",\"keyword_entries\":"+keywords+'}';}
inline Parts request(const EventTable& source,const std::string& operation,const std::string& json){return {field("engine","polars"),field("operation",operation),{"source_file","source.json",serialize_embedding_workbench_table_json(source),true},field("call_json",json)};}
inline const std::string admitted=R"({"type":"list","values":[{"type":"tuple","values":[{"type":"str","value":"A"},{"type":"str","value":"B"}]}]})";
inline std::string keywords(const std::string& key,const std::string& value,const std::string& tail={}){return "[{\"key\":\""+key+"\",\"value\":"+value+'}'+tail+']';}
inline void envelope(const DiscoveryWorkbenchResponse& response){
  const auto node=o::shared::Reader(response.json).read();std::set<std::string> names;for(const auto& [name,value]:node.fields){(void)value;names.insert(name);}
  require(names==std::set<std::string>{"family","engine","operation","http_status","phase","apply_returned","source","result","source_unchanged","case_id_key","attribute_key","timestamp_key","source_table_json","filtered_table_json","variants_table_json","variants_source_unchanged","error_type","error","call_mode","call_json","controls_before_json","controls_after_json","controls_unchanged","source_after_table_json","returned_table_metadata_json"},"Polars original response is not exact25");
}
inline void preserved(const DiscoveryWorkbenchResponse& response,const EventTable& source,const std::string& json){
  envelope(response);require(text(response.json,"call_json")==json,"original raw call bytes changed");const auto expected=serialize_embedding_workbench_table_json(source);
  require(text(response.json,"source_table_json")==expected&&text(response.json,"source_after_table_json")==expected,"complete source before/after changed");
  const auto reopened=parse_embedding_workbench_table_json(text(response.json,"source_after_table_json").value());require(serialize_embedding_workbench_table_json(reopened)==expected,"source-after cannot reopen exactly");
  require(text(response.json,"controls_before_json")==text(response.json,"controls_after_json"),"actual caller artifact changed");contains(response,"\"controls_unchanged\":true");contains(response,"\"source_unchanged\":true");
  contains(response,"\"case_id_key\":null,\"attribute_key\":null,\"timestamp_key\":null");contains(response,"\"variants_table_json\":null,\"variants_source_unchanged\":null");
  const auto controls=o::shared::Reader(text(response.json,"controls_after_json").value()).read();require(controls.fields.size()==8,"caller controls are not exact8");
  const auto reopen_cell=[&](const o::Node& value){if(value.kind==o::Node::kString){const auto table=parse_embedding_workbench_table_json(value.text);require(table.columns==std::vector<std::string>{"value"}&&table.rows.size()==1,"caller full cell artifact cannot reopen");}};
  reopen_cell(o::at(o::at(controls,"argument"),"value_json"));const auto& parameters=o::at(controls,"parameters");reopen_cell(o::at(parameters,"raw_json"));for(const auto& entry:o::at(parameters,"entries").items)reopen_cell(o::at(entry,"value_json"));
}
inline void metadata(const DiscoveryWorkbenchResponse& response,const EventTable& context,const std::string& kind){
  const auto artifact=text(response.json,"returned_table_metadata_json");require(artifact.has_value(),"actual returned metadata missing");const auto node=o::shared::Reader(*artifact).read();
  require(node.fields.size()==4&&o::at(node,"kind").text==kind,"metadata field count/kind changed");auto header=context;header.rows.clear();
  const auto encoded=o::at(node,"table_context_json").text;require(encoded==serialize_embedding_workbench_table_json(header),"whole native header context changed");
  auto reopened=parse_embedding_workbench_table_json(encoded);reopened.rows=context.rows;require(serialize_embedding_workbench_table_json(reopened)==serialize_embedding_workbench_table_json(context),"whole header context cannot restore complete native table");
  if(kind=="EventTable")require(o::at(node,"row_count_known").boolean,"eager row hint is unknown");
  else require(o::at(node,"row_count_known").kind==o::Node::kBoolean,"lazy row hint presence missing");
}
inline void result(const DiscoveryWorkbenchResponse& response,const EventTable& expected){
  require(response.status==200,"expected native success: "+response.json);const auto artifact=text(response.json,"filtered_table_json");require(artifact==serialize_embedding_workbench_table_json(expected),"complete native filtered result changed");
  require(serialize_embedding_workbench_table_json(parse_embedding_workbench_table_json(artifact.value()))==artifact,"filtered artifact cannot reopen");contains(response,"\"apply_returned\":true");
}
inline void native_error(const DiscoveryWorkbenchResponse& response,const std::function<void()>& expected,bool returned){
  bool caught=false;try{expected();}catch(const PolarsVariantsFilterError& error){caught=true;require(response.status==400&&text(response.json,"error_type")==error.type&&text(response.json,"error")==error.message&&text(response.json,"phase")==error.phase,"complete native type/phase/message changed: "+response.json);}
  require(caught,"test expected native error did not occur");contains(response,returned?"\"apply_returned\":true":"\"apply_returned\":false");
}
inline void workflows(const std::function<DiscoveryWorkbenchResponse(const Parts&)>& run){
  const auto source=between_source("polars");EventAttribute control{"",""};control.container=AttributeContainerKind::kList;EventAttribute inner{"",""};inner.container=AttributeContainerKind::kTuple;inner.children={{"","A"},{"","B"}};control.children={inner};const auto none=o::null_value();
  for(const auto& kind:std::vector<std::string>{"DataFrame","LazyFrame"}){
    const auto json=call(keywords("admitted_variants",admitted),kind);const auto response=run(request(source,"membership",json));preserved(response,source,json);result(response,subset_rows(source,{3,4}));metadata(response,subset_rows(source,{3,4}),kind=="DataFrame"?"EventTable":"LazyEventTable");
    for(const auto& operation:std::vector<std::string>{"from_list","auto"}){
      const auto unavailable_json=call(operation=="auto"?"[]":keywords("variants",R"({"type":"list","values":[{"type":"str","value":"A,B"}]})"),kind);const auto unavailable=run(request(source,operation,unavailable_json));preserved(unavailable,source,unavailable_json);
      EventAttribute serialized{"",""};serialized.container=AttributeContainerKind::kList;serialized.children={{"","A,B"}};
      native_error(unavailable,[&]{if(kind=="DataFrame"){if(operation=="auto")(void)auto_filter_polars_variants_parameter_value(source,none);else(void)filter_polars_variants_from_list_parameter_value(source,serialized,none);}else{const auto input=LazyEventTable::from_table(source);if(operation=="auto")(void)auto_filter_polars_variants_parameter_value(input,none);else(void)filter_polars_variants_from_list_parameter_value(input,serialized,none);}},false);
      require(!text(unavailable.json,"returned_table_metadata_json"),"throwing invocation invented metadata");
    }
    const auto top_json=call(keywords("k",R"({"type":"int","value":1})"),kind);const auto top=run(request(source,"top_k",top_json));preserved(top,source,top_json);
    if(kind=="DataFrame"){native_error(top,[&]{EventAttribute k{"","1",AttributeKind::kInt};k.typed_value=std::int64_t{1};(void)filter_polars_variants_top_k_parameter_value(source,k,none);},false);require(!text(top.json,"returned_table_metadata_json"),"eager top-k throw invented metadata");}
    else{result(top,subset_rows(source,{0,1,2}));metadata(top,subset_rows(source,{0,1,2}),"LazyEventTable");}
    for(const auto& operation:std::vector<std::string>{"membership","from_list","top_k"}){
      const auto missing_json=call(R"([{"key":"parameters","value":{"type":"list","values":[{"type":"bool","value":false}]}}])",kind);const auto missing=run(request(source,operation,missing_json));preserved(missing,source,missing_json);
      native_error(missing,[&]{if(kind=="DataFrame"){if(operation=="membership")(void)filter_polars_variants(source,std::nullopt);else if(operation=="from_list")(void)filter_polars_variants_from_list(source,std::nullopt);else(void)filter_polars_variants_top_k(source,std::nullopt);}else{const auto input=LazyEventTable::from_table(source);if(operation=="membership")(void)filter_polars_variants(input,std::nullopt);else if(operation=="from_list")(void)filter_polars_variants_from_list(input,std::nullopt);else(void)filter_polars_variants_top_k(input,std::nullopt);}},false);
      const auto snapshot=o::shared::Reader(text(missing.json,"controls_before_json").value()).read();require(o::at(snapshot,"invoked_arm").text=="missing_required_default_options"&&!o::at(snapshot,"parameters_passed").boolean&&o::at(snapshot,"parameter_state").text=="raw","unused actual malformed kwargs passed to default overload");
    }
  }
  const auto none_json=call(keywords("admitted_variants",R"({"type":"NoneType","value":null})"));const auto null_control=run(request(source,"membership",none_json));preserved(null_control,source,none_json);native_error(null_control,[&]{(void)filter_polars_variants_parameter_value(LazyEventTable::from_table(source),none,none);},false);
  const auto raw_json=call(keywords("admitted_variants",admitted,R"(,{"key":"parameters","value":{"type":"str","value":"bad"}})"));const auto raw=run(request(source,"membership",raw_json));preserved(raw,source,raw_json);native_error(raw,[&]{(void)filter_polars_variants_parameter_value(LazyEventTable::from_table(source),control,EventAttribute{"","bad"});},false);
  const std::string enum_positive=R"({"type":"enum","module":"pm4py.algo.filtering.polars.variants.variants_filter","name":"POSITIVE","value":{"type":"str","value":"positive"}})";
  const auto first="["+enum_positive+R"(,{"type":"bool","value":false}])";const std::string last=R"([{"type":"str","value":"positive"},{"type":"bool","value":true}])";
  for(const bool reverse:{false,true}){const auto ordered_json=call(keywords("admitted_variants",admitted,",{\"key\":\"parameters\",\"value\":{\"type\":\"dict\",\"items\":["+(reverse?last+','+first:first+','+last)+"]}}"));const auto ordered=run(request(source,"membership",ordered_json));preserved(ordered,source,ordered_json);result(ordered,subset_rows(source,{0,1,2}));}
  auto no_time=source;no_time.columns.back()="other_time";for(auto& row:no_time.rows)row.back()->key="other_time";const auto collect_json=call(keywords("admitted_variants",admitted));const auto collect=run(request(no_time,"membership",collect_json));preserved(collect,no_time,collect_json);
  native_error(collect,[&]{const auto selected=filter_polars_variants_parameter_value(LazyEventTable::from_table(no_time),control,none);(void)selected.materialize();},true);metadata(collect,no_time,"LazyEventTable");
  auto unsafe=source;unsafe.attributes.push_back({"raw-byte",std::string("\xff\0",2)});unsafe.metadata.compatibility_snapshot_valid=true;const auto unsafe_json=call(keywords("admitted_variants",admitted));const auto unsafe_result=run(request(unsafe,"membership",unsafe_json));preserved(unsafe_result,unsafe,unsafe_json);const auto unsafe_expected=subset_rows(unsafe,{3,4});result(unsafe_result,unsafe_expected);metadata(unsafe_result,unsafe_expected,"LazyEventTable");
  const auto unsafe_header=o::shared::Reader(text(unsafe_result.json,"returned_table_metadata_json").value()).read();require(o::at(unsafe_header,"table_context_json").text.find("event-table-raw-v1")!=std::string::npos,"raw fallback header marker stripped");
  const auto exact=std::string("\xef\xbb\xbf",3)+"\r\n"+call(keywords("k",R"({"type":"int","value":9007199254740993})"))+"\r\n";auto bytes=request(source,"top_k",exact);bytes.back().file=true;bytes.back().filename="call_json.txt";const auto imported=run(bytes);preserved(imported,source,exact);result(imported,source);bytes.back().filename="call.json";require(run(bytes).status==400,"noncanonical raw call File accepted");
  auto reuse=request(source,"membership",call(keywords("admitted_variants",admitted)));reuse[2].bytes=text(unsafe_result.json,"filtered_table_json").value();const auto reused=run(reuse);result(reused,unsafe_expected);require(text(reused.json,"source_table_json")==text(unsafe_result.json,"filtered_table_json"),"filtered data File bytes not reused exactly");
  for(const auto& extra:std::vector<std::string>{"positive","variants_file","header_file"}){auto inactive=request(source,"membership",collect_json);inactive.push_back(field(extra,""));const auto rejected=run(inactive);envelope(rejected);require(rejected.status==400&&text(rejected.json,"phase")=="control","inactive/header upload field accepted");}
  auto duplicate=request(source,"membership",collect_json);duplicate.push_back(duplicate.back());const auto duplicated=run(duplicate);envelope(duplicated);require(duplicated.status==400,"duplicate raw call accepted");
  for(const auto& malformed:std::vector<std::string>{"",R"({"schema":"minera-polars-variants-original-call-v1","input_kind":"LazyFrame","keyword_entries":[],"\u0069nput_kind":"DataFrame"})"}){const auto rejected=run(request(source,"membership",malformed));envelope(rejected);require(rejected.status==400&&text(rejected.json,"call_json")==malformed&&text(rejected.json,"phase")=="control","malformed exact call echo lost");}
}
}
#ifndef MINERA_POLARS_VARIANTS_ORIGINAL_ROUTE_TEST
int main(){try{polars_variants_original_test::workflows([](const auto& parts){return minera::execute_table_variants_filter(parts);});std::cout<<"original Polars variants exact25 and complete artifacts passed\n";}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}return 0;}
#endif
