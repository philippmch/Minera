#include "table_attribute_repetition_test_support.hpp"
#include "minera/table_attributes_filter_workbench.hpp"
#include "../polars_attributes_original_call_parser.hpp"
#include <bit>
#include <cstdint>
#include <iostream>

namespace {
using namespace table_repetition_test;
namespace original=minera::polars_attributes_original_call_detail;
using Parts=std::vector<DiscoveryWorkbenchPart>;
std::string call(std::string keywords="[]",bool eager=false){return "{\"schema\":\"minera-polars-attributes-original-call-v1\",\"input_kind\":\""+std::string(eager?"DataFrame":"LazyFrame")+"\",\"keyword_entries\":"+keywords+'}';}
Parts request(const EventTable& source,const std::string& operation,const std::string& json){return {field("engine","polars"),field("operation",operation),{"source_file","source.json",serialize_embedding_workbench_table_json(source),true},field("call_json",json)};}
void preserved(const DiscoveryWorkbenchResponse& answer,const EventTable& source,const std::string& json,bool eager){
  require(text(answer.json,"call_json")==json,"raw call echo changed");
  const auto state=text(answer.json,"controls_before_json");require(state.has_value()&&state==text(answer.json,"controls_after_json"),"actual parsed controls differ");
  require(text(*state,"input_kind")==std::string(eager?"DataFrame":"LazyFrame"),"actual source kind lost");
  contains(answer,"\"controls_unchanged\":true");contains(answer,"\"source_unchanged\":true");contains(answer,"\"case_id_key\":\"\"");contains(answer,"\"attribute_key\":\"\"");
  const auto before=serialize_embedding_workbench_table_json(source);require(text(answer.json,"source_table_json")==before&&text(answer.json,"source_after_table_json")==before,"full source state changed");
}
void controls(){
  const auto source=source_table("polars");
  const std::string members=R"([{"key":"values","value":{"type":"list","value":[{"type":"string","value":"A"}]}}])";
  for(bool eager:{false,true}){
    const auto json="\r\n "+call(members,eager)+"\r\n";const auto answer=execute_table_attributes_filter(request(source,"events",json));require(answer.status==200,"eager/lazy original events failed: "+answer.json);preserved(answer,source,json,eager);
    auto expected=source;expected.rows={source.rows[0],source.rows[2]};require(text(answer.json,"filtered_table_json")==serialize_embedding_workbench_table_json(expected),"full original event result changed");
    const auto reopened=parse_embedding_workbench_table_json(*text(answer.json,"filtered_table_json"));require(serialize_embedding_workbench_table_json(reopened)==serialize_embedding_workbench_table_json(expected),"native result reopen differs");
    const auto absent=execute_table_attributes_filter(request(source,"events",call("[]",eager)));require(absent.status==400,"missing required keyword accepted");contains(absent,"\"phase\":\"apply\"");preserved(absent,source,call("[]",eager),eager);
  }
  const auto recursive=original::parse(call(R"([{"key":"values","value":{"type":"list","value":[{"type":"decimal","value":"-0.0"},{"type":"tuple","value":[{"type":"decimal","value":"0.33333333333333331"}]}]}},{"key":"parameters","value":{"type":"parameter_entries","entries":[{"key":"decreasingFactor","enum_key":false,"value":{"type":"decimal","value":"0.5"}}]}}])"),"events");
  const auto& a=recursive.native.values->children[0];require(a.key.empty()&&a.value=="-0.000000","Polars decimal native lexical/child key changed");require(std::bit_cast<std::uint64_t>(std::get<double>(a.typed_value))==std::bit_cast<std::uint64_t>(-0.0),"signed zero bits changed");
  const auto& nested=recursive.native.values->children[1].children[0];require(nested.key.empty()&&nested.value==std::to_string(1.0/3.0),"recursive native constructor changed");
  require(recursive.native.parameters->at(0).value.key.empty()&&recursive.native.parameters->at(0).value.value=="0.500000","parameter value native key/decimal differs");
  const auto date=original::parse(call(R"([{"key":"values","value":{"type":"date","value":"2020-01-01T00:00:00+00:00"}}])"),"events");const auto& temporal=std::get<NativeDate>(date.native.values->typed_value);require(temporal.resolution==NativeDateResolution::kMicrosecond&&!temporal.timezone,"Polars date constructor metadata changed");
  const auto ordered=original::parse(call(R"([{"key":"act_count0","value":{"type":"map","value":{"X":{"type":"integer","value":"4"},"A":{"type":"decimal","value":"2.5"}}}},{"key":"most_common_variant","value":{"type":"tuple","value":[{"type":"string","value":"X"}]}}])"),"threshold");
  require(ordered.native.count_cache&&std::holds_alternative<EventAttribute>(*ordered.native.count_cache),"original count dictionary union changed");const auto& dictionary=std::get<EventAttribute>(*ordered.native.count_cache);require(dictionary.children[0].key=="X"&&dictionary.children[1].key=="A","literal map order lost");
  const auto snapshot=original::shared::Reader(original::controls_json(ordered)).read();
  const auto* arguments=original::shared::field(snapshot,"arguments");const auto* options=original::shared::field(snapshot,"options");require(arguments&&arguments->fields.size()==8&&options&&options->fields.size()==10,"full native controls omitted fields");
  const auto* most=original::shared::field(*arguments,"most_common_variant");require(most&&most->kind==original::shared::Node::kString,"most_common_variant missing from actual state");
  const auto reopened=parse_embedding_workbench_table_json(most->text);require(reopened.rows[0][0]==ordered.native.most_common_variant,"actual most-common cell artifact differs");
  for(const auto& parameters:{std::string(R"({"type":"null"})"),std::string(R"({"type":"list","value":[]})"),std::string(R"({"type":"parameter_entries","entries":[]})")}){
    const auto json=call("[{\"key\":\"parameters\",\"value\":"+parameters+"},{\"key\":\"values\",\"value\":{\"type\":\"list\",\"value\":[{\"type\":\"string\",\"value\":\"A\"}]}}]");
    const auto answer=execute_table_attributes_filter(request(source,"events",json));require(answer.status==200,"null/empty raw/empty dictionary was collapsed or rejected");preserved(answer,source,json,false);
    require(text(*text(answer.json,"controls_before_json"),"parameter_state")==std::string(parameters.find("parameter_entries")!=std::string::npos?"entries":"raw"),"parameter presence state changed");
  }
  const auto counts=execute_table_attributes_filter(request(source,"counts",call(R"([{"key":"attribute_key","value":{"type":"string","value":"concept:name"}}])")));require(counts.status==200&&text(counts.json,"result_kind")=="counts"&&text(counts.json,"counts_table_json").has_value(),"original typed counts unavailable");
  for(const std::string keywords:{R"([{"key":"values","value":{"type":"list","value":[]}},{"key":"values","value":{"type":"list","value":[]}}])",R"([{"key":"parameters","value":{"type":"map","value":{}}}])",R"([{"key":"attribute_key","value":{"type":"string","value":"x"}}])",R"([{"key":"values","value":{"type":"frozenset","value":[]}}])",R"([{"key":"parameters","value":{"type":"parameter_entries","entries":[{"key":"positive","enum_key":false,"value":{"type":"boolean","value":true}},{"key":"positive","enum_key":false,"value":{"type":"boolean","value":false}}]}}])"}){
    const auto json=call(keywords);const auto answer=execute_table_attributes_filter(request(source,"events",json));require(answer.status==400,"malformed original controls accepted");contains(answer,"\"phase\":\"control\"");require(text(answer.json,"call_json")==json,"control failure lost raw call");
  }
  auto friendly=request(source,"events",call(members));friendly.push_back(field("positive","true"));const auto mixed=execute_table_attributes_filter(friendly);require(mixed.status==400,"mixed friendly controls accepted");contains(mixed,"\"phase\":\"control\"");
  auto broken_source=request(source,"events",call(members));broken_source[2].bytes="bad native table";const auto broken=execute_table_attributes_filter(broken_source);require(broken.status==400,"invalid source accepted");contains(broken,"\"phase\":\"source_import\"");contains(broken,"\"controls_unchanged\":true");
  auto bytes=request(source,"events",call(members));bytes.back().file=true;bytes.back().filename="call_json.txt";require(execute_table_attributes_filter(bytes).status==200,"canonical byte File rejected");
  bytes.back().filename="other.json";const auto wrong_file=execute_table_attributes_filter(bytes);require(wrong_file.status==400,"noncanonical byte File accepted");contains(wrong_file,"\"call_mode\":null");
  const auto duplicate=execute_table_attributes_filter(request(source,"events",call(members)),{field("call_json",call(members))});require(duplicate.status==400,"duplicate call accepted");contains(duplicate,"\"call_json\":null");
}
}
int main(){try{controls();std::cout<<"Polars original-call strict controls, native constructors and eager/lazy artifacts passed\n";}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
