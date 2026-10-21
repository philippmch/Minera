#include "table_attribute_repetition_test_support.hpp"
#include "minera/pandas_attribute_repetition_filter.hpp"
#include <iostream>

namespace {
using namespace table_repetition_test;
void complete_table_workflows() {
  for(const std::string engine:{"pandas","polars"}) {
    const auto source=source_table(engine),expected=expected_table(source);
    success(execute_table_attribute_repetition(request(engine,source)),source,expected,engine);
    auto empty=source;empty.rows.clear();if(empty.index)empty.index->labels.clear();
    success(execute_table_attribute_repetition(request(engine,source,{field("min_rep","0"),field("max_rep","0")})),source,empty,engine);
    auto literal=source;
    const std::string key(" counted\n\0",10),value(" A\n\0",4);
    literal.columns[1]=key;
    for(auto& row:literal.rows){row[1]->key=key;row[1]->value=row[0]->value=="c1"?value:"B";}
    // The literal test uses ordinary string dtypes rather than categorical casts.
    literal.dtypes[1]=engine=="polars"?"String":"object";literal.categorical_dtypes.clear();
    const auto target="{\"type\":\"string\",\"value\":"+serialize_embedding_workbench_string_json(value)+"}";
    auto parts=request(engine,literal,{field("attribute_key",key)});parts[1].bytes=target;
    success(execute_table_attribute_repetition(parts),literal,expected_table(literal),engine);
  }
}
void complete_carrier_roundtrip() {
  auto source=source_table("pandas");
  source.attributes[0].value="safe source metadata";
  source.metadata.compatibility_snapshot_valid=false;
  source.metadata.compatibility_globals_snapshot.clear();
  source.metadata.event_globals={integer("signed global",42),unsigned_integer("unsigned global",42)};
  const auto serialized=serialize_embedding_workbench_table_json(source);
  require(serialized.find("native_byte_strings")==std::string::npos,"safe table unexpectedly required raw byte fallback");
  const auto reopened=parse_embedding_workbench_table_json(serialized);
  require(serialize_embedding_workbench_table_json(reopened)==serialized,"strict table carrier roundtrip differs");
  require(std::holds_alternative<std::int64_t>(reopened.rows[0][2]->typed_value),"strict table positive signed cell narrowed");
  require(std::holds_alternative<std::uint64_t>(reopened.rows[0][3]->typed_value),"strict table unsigned cell narrowed");
  require(std::holds_alternative<std::int64_t>(std::get<EventAttribute>(reopened.index->labels[0].value).typed_value),"strict table signed index narrowed");
  require(std::holds_alternative<std::int64_t>(reopened.metadata.event_globals[0].typed_value),"strict table signed global narrowed");
  require(reopened.categorical_dtypes==source.categorical_dtypes,"category declaration narrowed");
}
void engine_error_phases_and_control_validation() {
  for(const std::string engine:{"pandas","polars"}) {
    const auto source=source_table(engine);
    auto result=execute_table_attribute_repetition(request(engine,source,{field("attribute_key","missing")}));
    require(result.status==400,"missing column succeeded");contains(result,engine=="polars"?"\"phase\":\"collect\"":"\"phase\":\"apply\"");
    contains(result,engine=="polars"?"\"apply_returned\":true":"\"apply_returned\":false");contains(result,"\"source_unchanged\":true");
    require(text(result.json,"error_type")== (engine=="polars"?"ColumnNotFoundError":"KeyError"),"native error type lost");
    require(text(result.json,"source_table_json")==serialize_embedding_workbench_table_json(source),"error lost typed source artifact");require(!text(result.json,"filtered_table_json"),"error exposed result");
    result=execute_table_attribute_repetition(request(engine,source,{field("min_rep","[]")}));require(result.status==400,"array bound accepted");contains(result,"\"phase\":\"control\"");
    result=execute_table_attribute_repetition(request(engine,source),{field("engine",engine)});require(result.status==400,"duplicate control accepted");
    auto parts=request(engine,source);parts[2].file=false;require(execute_table_attribute_repetition(parts).status==400,"source text field accepted");
    parts=request(engine,source);parts[2].bytes="{invalid";result=execute_table_attribute_repetition(parts);require(result.status==400,"bad source accepted");contains(result,"\"phase\":\"source_import\"");require(!text(result.json,"source_table_json"),"bad source artifact exposed");
  }
  const std::string diagnostic("full\0native diagnostic",22);
  const PandasAttributeRepetitionError error("NativeCarrierTest",diagnostic);
  require(error.message==diagnostic,"pandas exception full diagnostic carrier truncates NUL");
  auto source=source_table("polars");source.columns[3]=std::string("unsigned\0tail",13);for(auto& row:source.rows)row[3]->key=source.columns[3];
  auto response=execute_table_attribute_repetition(request("polars",source,{field("attribute_key","missing")}));
  const auto message=text(response.json,"error");require(message&&message->find(std::string("\0tail",5))!=std::string::npos,"observed Polars plan diagnostic truncated during JSON transport");
  auto unsupported=source_table("polars");unsupported.index=source_table("pandas").index;
  response=execute_table_attribute_repetition(request("polars",unsupported));require(response.status==400,"Polars discarded dataframe index metadata");contains(response,"\"phase\":\"collect\"");
}
}  // namespace
int main(){try{complete_table_workflows();complete_carrier_roundtrip();engine_error_phases_and_control_validation();std::cout<<"complete pandas/Polars table workbench workflows passed\n";}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
