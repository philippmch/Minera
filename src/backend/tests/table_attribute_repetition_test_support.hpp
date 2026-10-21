#pragma once
#include "minera/table_attribute_repetition_workbench.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/literal_controls_json.hpp"
#include <optional>
#include <stdexcept>

namespace table_repetition_test {
using namespace minera;
inline void require(bool condition, const std::string& message) {if(!condition)throw std::runtime_error(message);}
inline EventAttribute integer(std::string key, std::int64_t value) {EventAttribute a{std::move(key),std::to_string(value),AttributeKind::kInt};a.typed_value=value;return a;}
inline EventAttribute unsigned_integer(std::string key, std::uint64_t value) {EventAttribute a{std::move(key),std::to_string(value),AttributeKind::kInt};a.typed_value=value;return a;}
inline EventTable source_table(const std::string& engine) {
  EventTable table;table.columns={"case:concept:name","concept:name","signed","unsigned"};
  table.dtypes=engine=="polars"?std::vector<std::string>{"String","String","Int64","UInt64"}:std::vector<std::string>{"object","object","int64","uint64"};
  for(const auto& pair:std::vector<std::pair<std::string,std::string>>{{"c1","A"},{"c2","B"},{"c1","A"},{"c1","B"}})
    table.rows.push_back({EventAttribute{table.columns[0],pair.first},EventAttribute{table.columns[1],pair.second},integer("signed",9007199254740993LL),unsigned_integer("unsigned",18446744073709551615ULL)});
  table.attributes={{"opaque",std::string("\xff\0\xed\xa0\x80",5)},integer("signed metadata",42),unsigned_integer("unsigned metadata",42)};
  table.metadata.extensions={{"Concept","concept","urn:concept"}};
  table.metadata.classifiers={{"activity",{"concept:name"}}};
  table.metadata.properties={{"property","original"}};
  table.metadata.compatibility_snapshot_valid=true;
  table.metadata.compatibility_globals_snapshot={{"raw",{integer("signed snapshot",42)}}};
  if(engine=="pandas") {
    EventTableIndex index;index.kind="Index";index.dtype="int64";index.names={"row"};
    for(std::int64_t i=10;i<14;++i)index.labels.push_back({integer("",i)});
    table.index=std::move(index);
    table.dtypes[1]="category";table.categorical_dtypes.resize(4);
    table.categorical_dtypes[1]=EventTableCategoricalDtype{"object",{{"","A"},{"","B"},{"","unused"}},true};
  }
  return table;
}
inline EventTable expected_table(EventTable source) {
  source.rows={source.rows[0],source.rows[2],source.rows[3]};
  if(source.index)source.index->labels={source.index->labels[0],source.index->labels[2],source.index->labels[3]};
  return source;
}
inline DiscoveryWorkbenchPart field(std::string name,std::string value) {return {std::move(name),"",std::move(value),false};}
inline std::vector<DiscoveryWorkbenchPart> request(const std::string& engine,const EventTable& table,std::vector<DiscoveryWorkbenchPart> controls={}) {
  std::vector<DiscoveryWorkbenchPart> parts={field("engine",engine),field("target_json",R"({"type":"string","value":"A"})"),{"source_file","source.json",serialize_embedding_workbench_table_json(table),true}};
  parts.insert(parts.end(),controls.begin(),controls.end());return parts;
}
inline std::optional<std::string> text(const std::string& json,const std::string& name) {
  const auto token="\""+name+"\":";auto at=json.find(token);require(at!=std::string::npos,"missing envelope field "+name);at+=token.size();
  if(json.substr(at,4)=="null")return std::nullopt;
  require(json.at(at)=='"',"expected string envelope field "+name);const auto start=at++;bool escaped=false;
  for(;at<json.size();++at){const char c=json[at];if(escaped)escaped=false;else if(c=='\\')escaped=true;else if(c=='"')return parse_literal_string_vector_json("["+json.substr(start,at-start+1)+"]").at(0);}
  throw std::runtime_error("unterminated envelope string");
}
inline void contains(const DiscoveryWorkbenchResponse& response,const std::string& token) {require(response.json.find(token)!=std::string::npos,"missing JSON token "+token+" in "+response.json);}
inline void success(const DiscoveryWorkbenchResponse& response,const EventTable& source,const EventTable& expected,const std::string& engine) {
  require(response.status==200,"expected success: "+response.json);contains(response,"\"phase\":\"complete\"");contains(response,"\"apply_returned\":true");contains(response,"\"source_unchanged\":true");
  require(text(response.json,"engine")==engine,"wrong engine");require(!text(response.json,"error"),"unexpected error");
  const auto original=text(response.json,"source_table_json"),result=text(response.json,"filtered_table_json");require(original&&result,"missing complete artifacts");
  require(*original==serialize_embedding_workbench_table_json(source),"complete source carrier differs");
  require(*result==serialize_embedding_workbench_table_json(expected),"complete output carrier differs");
  const auto reopened=parse_embedding_workbench_table_json(*result);
  require(serialize_embedding_workbench_table_json(reopened)==*result,"table artifact cannot be reopened losslessly");
  require(std::holds_alternative<std::int64_t>(reopened.attributes.at(1).typed_value),"positive signed metadata narrowed");
  require(std::holds_alternative<std::uint64_t>(reopened.attributes.at(2).typed_value),"unsigned metadata narrowed");
  if(!reopened.rows.empty()) {
    require(std::holds_alternative<std::int64_t>(reopened.rows[0][2]->typed_value),"positive signed cell narrowed");
    require(std::holds_alternative<std::uint64_t>(reopened.rows[0][3]->typed_value),"unsigned cell narrowed");
  }
}
}  // namespace table_repetition_test
