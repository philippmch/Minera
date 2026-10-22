#pragma once

#include "minera/polars_variants_filter.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/timestamp.hpp"
#include "polars_ltl_original_call_parser.hpp"
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <map>
#include <optional>
#include <set>

namespace minera::polars_variants_original_call_detail {
namespace shared=polars_ltl_original_call_detail;
using Node=shared::Node;
[[noreturn]] inline void invalid(std::string_view message){throw std::invalid_argument("original Polars variants call_json: "+std::string(message));}
inline void exact(const Node& node,std::initializer_list<std::string_view> fields){
  if(node.kind!=Node::kObject||node.fields.size()!=fields.size())invalid("invalid packed fields");
  for(const auto name:fields)if(!shared::field(node,name))invalid("missing packed field");
}
inline const Node& at(const Node& node,std::string_view name){const auto* value=shared::field(node,name);if(!value)invalid("missing packed field");return *value;}
inline std::string string(const Node& node){if(node.kind!=Node::kString)invalid("string required");return node.text;}
inline std::string tag(const Node& node){return string(at(node,"type"));}
inline EventAttribute null_value(std::string key={}){EventAttribute result{std::move(key),""};result.is_null=true;return result;}
inline std::string integer_token(std::string_view raw,const Node& node){
  const auto token=std::string(raw.substr(node.begin,node.end-node.begin));const auto first=token.starts_with('-')?1U:0U;
  if(node.kind!=Node::kNumber||token.size()==first||token.find_first_not_of("0123456789",first)!=std::string::npos)invalid("integer token required");return token;
}
inline double floating(const std::string& token){
  if(token=="nan")return std::numeric_limits<double>::quiet_NaN();if(token=="inf")return std::numeric_limits<double>::infinity();if(token=="-inf")return -std::numeric_limits<double>::infinity();
  const auto first=token.starts_with('-')?1U:0U;
  if(token.size()<=first+2||token.substr(first,2)!="0x"||token.find('p',first+2)==std::string::npos)invalid("float requires Python hexadecimal form");
  char* end=nullptr;const auto result=std::strtod(token.c_str(),&end);if(end!=token.c_str()+token.size()||!std::isfinite(result))invalid("invalid hexadecimal float");return result;
}
inline std::string enum_key(const Node& node){
  exact(node,{"type","module","name","value"});
  if(tag(node)!="enum"||string(at(node,"module"))!="pm4py.algo.filtering.polars.variants.variants_filter")invalid("unsupported public enum module");
  static const std::map<std::string,std::string> names={{"CASE_ID_KEY","pm4py:param:case_id_key"},{"ACTIVITY_KEY","pm4py:param:activity_key"},{"POSITIVE","positive"}};
  const auto found=names.find(string(at(node,"name")));if(found==names.end())invalid("unsupported public enum identity");
  const auto& value=at(node,"value");exact(value,{"type","value"});if(tag(value)!="str"||string(at(value,"value"))!=found->second)invalid("enum underlying value mismatch");return found->second;
}
// Private producer projection: Microsecond/null-zone dates, std::to_string
// floating lexical carriers and EMPTY sequence/value keys match the Polars GEN.
inline EventAttribute packed(std::string_view raw,const Node& node,std::string key={}){
  const auto type=tag(node);
  if(type=="NoneType"||type=="str"||type=="bool"||type=="int"){
    exact(node,{"type","value"});const auto& value=at(node,"value");
    if(type=="NoneType"){if(value.kind!=Node::kNull)invalid("NoneType requires null");return null_value(std::move(key));}
    if(type=="str")return EventAttribute{std::move(key),string(value)};
    if(type=="bool"){if(value.kind!=Node::kBoolean)invalid("bool requires Boolean");EventAttribute result{std::move(key),value.boolean?"true":"false",AttributeKind::kBoolean};result.typed_value=value.boolean;return result;}
    const auto token=integer_token(raw,value);EventAttribute result{std::move(key),token,AttributeKind::kInt};std::int64_t signed_value=0;
    const auto signed_parse=std::from_chars(token.data(),token.data()+token.size(),signed_value);
    if(signed_parse.ec==std::errc{}&&signed_parse.ptr==token.data()+token.size()){result.value=std::to_string(signed_value);result.typed_value=signed_value;return result;}
    if(!token.starts_with('-')){std::uint64_t unsigned_value=0;const auto parsed=std::from_chars(token.data(),token.data()+token.size(),unsigned_value);if(parsed.ec==std::errc{}&&parsed.ptr==token.data()+token.size()){result.value=std::to_string(unsigned_value);result.typed_value=unsigned_value;return result;}}
    invalid("integer outside native signed/unsigned64 carrier");
  }
  if(type=="float"){exact(node,{"type","hex"});const auto value=floating(string(at(node,"hex")));EventAttribute result{std::move(key),std::to_string(value),AttributeKind::kFloat};result.typed_value=value;return result;}
  if(type=="date"||type=="datetime"){
    if(type=="date")exact(node,{"type","iso"});else{
      exact(node,{"type","iso","fold","offset_seconds"});const auto fold=integer_token(raw,at(node,"fold"));if(fold!="0"&&fold!="1")invalid("datetime fold must be zero or one");
      const auto& offset=at(node,"offset_seconds");if(offset.kind!=Node::kNull){
        if(offset.kind!=Node::kNumber)invalid("datetime offset requires number or null");const auto token=std::string(raw.substr(offset.begin,offset.end-offset.begin));char* end=nullptr;const auto value=std::strtod(token.c_str(),&end);
        if(end!=token.c_str()+token.size()||!std::isfinite(value))invalid("datetime offset requires finite number");
      }
    }
    const auto iso=string(at(node,"iso"));if(!parse_timestamp_exact(iso))invalid("invalid ISO date value");EventAttribute result{std::move(key),iso,AttributeKind::kDate};result.typed_value=NativeDate{iso};return result;
  }
  if(type=="list"||type=="tuple"||type=="set"){
    exact(node,{"type","values"});const auto& values=at(node,"values");if(values.kind!=Node::kArray)invalid("sequence values require array");EventAttribute result{std::move(key),""};
    result.container=type=="list"?AttributeContainerKind::kList:type=="tuple"?AttributeContainerKind::kTuple:AttributeContainerKind::kSet;
    for(const auto& value:values.items)result.children.push_back(packed(raw,value));return result;
  }
  if(type=="dict"){
    exact(node,{"type","items"});const auto& values=at(node,"items");if(values.kind!=Node::kArray)invalid("dictionary items require pairs");EventAttribute result{std::move(key),""};result.container=AttributeContainerKind::kContainer;std::set<std::string> names;
    for(const auto& pair:values.items){if(pair.kind!=Node::kArray||pair.items.size()!=2)invalid("dictionary items require pairs");const auto& name=pair.items[0];exact(name,{"type","value"});if(tag(name)!="str")invalid("ordinary dictionary keys require strings");auto literal=string(at(name,"value"));if(!names.insert(literal).second)invalid("duplicate dictionary key");result.children.push_back(packed(raw,pair.items[1],std::move(literal)));}return result;
  }
  invalid("unsupported packed value or enum outside parameter key");
}
struct Call {
  bool eager=false;
  std::string operation,argument_key,parameter_state="omitted";
  std::vector<std::string> keyword_order;
  std::optional<EventAttribute> argument;
  EventAttribute raw_parameters=null_value();
  std::optional<std::vector<PolarsVariantsFilterParameter>> parameters;
};
inline std::string argument_key(std::string_view operation){
  if(operation=="membership")return "admitted_variants";if(operation=="from_list")return "variants";if(operation=="top_k")return "k";if(operation=="auto")return {};invalid("unsupported operation");
}
inline Call parse(std::string_view raw,std::string_view operation){
  try{
    if(raw.starts_with(std::string_view("\xef\xbb\xbf",3)))raw.remove_prefix(3);
    const auto root=shared::Reader(raw).read();exact(root,{"schema","input_kind","keyword_entries"});if(string(at(root,"schema"))!="minera-polars-variants-original-call-v1")invalid("unsupported schema");
    const auto kind=string(at(root,"input_kind"));if(kind!="DataFrame"&&kind!="LazyFrame")invalid("input_kind must be DataFrame or LazyFrame");
    Call call;call.eager=kind=="DataFrame";call.operation=operation;call.argument_key=argument_key(operation);const auto& keywords=at(root,"keyword_entries");if(keywords.kind!=Node::kArray)invalid("keyword_entries requires array");std::set<std::string> seen;
    for(const auto& entry:keywords.items){
      exact(entry,{"key","value"});const auto name=string(at(entry,"key"));if((name!="parameters"&&(call.argument_key.empty()||name!=call.argument_key))||!seen.insert(name).second)invalid("unknown or duplicate keyword");call.keyword_order.push_back(name);const auto& value=at(entry,"value");
      if(name!="parameters"){call.argument=packed(raw,value);continue;}
      if(tag(value)!="dict"){call.raw_parameters=packed(raw,value);call.parameter_state=tag(value)=="NoneType"?"null":"raw";continue;}
      exact(value,{"type","items"});const auto& pairs=at(value,"items");if(pairs.kind!=Node::kArray)invalid("parameter dictionary requires pairs");call.parameters.emplace();call.parameter_state="entries";std::set<std::pair<std::string,bool>> identities;
      for(const auto& pair:pairs.items){
        if(pair.kind!=Node::kArray||pair.items.size()!=2)invalid("parameter dictionary requires pairs");const auto& key=pair.items[0];const bool is_enum=tag(key)=="enum";std::string literal;
        if(is_enum)literal=enum_key(key);else{exact(key,{"type","value"});if(tag(key)!="str")invalid("parameter key requires string or public enum");literal=string(at(key,"value"));}
        if(!identities.insert({literal,is_enum}).second)invalid("duplicate exact parameter identity");call.parameters->push_back({std::move(literal),packed(raw,pair.items[1]),is_enum});
      }
    }return call;
  }catch(const std::invalid_argument& error){const std::string message=error.what();constexpr std::string_view prefix="original Polars call_json: ";if(message.starts_with(prefix))invalid(std::string_view(message).substr(prefix.size()));throw;}
}
inline bool missing_required(const Call& call){return !call.argument_key.empty()&&!call.argument;}
inline std::string invoked_arm(const Call& call){if(missing_required(call))return "missing_required_default_options";return std::string(call.argument_key.empty()?"auto_":"criterion_")+(call.parameters?"ordered_parameters":"raw_parameters");}
inline std::string cell_json(const EventAttribute& value){EventTable table;table.columns={"value"};table.dtypes={"object"};table.rows={{value}};return serialize_embedding_workbench_table_json(table);}
inline std::string controls_json(const Call& call){
  const auto q=[](std::string_view value){return serialize_embedding_workbench_string_json(value);};std::string result="{\"input_kind\":"+q(call.eager?"DataFrame":"LazyFrame")+",\"keyword_order\":[";bool first=true;
  for(const auto& name:call.keyword_order){if(!first)result+=',';first=false;result+=q(name);}result+="],\"argument\":{\"key\":"+(call.argument_key.empty()?std::string("null"):q(call.argument_key))+",\"present\":"+(call.argument?"true":"false")+",\"value_json\":"+(call.argument?q(cell_json(*call.argument)):"null")+"},\"parameter_state\":"+q(call.parameter_state)+",\"parameters\":{\"native_arm\":"+q(call.parameters?"ordered_parameter_entries":"raw_attribute")+",\"raw_json\":"+(call.parameters?"null":q(cell_json(call.raw_parameters)))+",\"entries\":[";
  first=true;if(call.parameters)for(const auto& entry:*call.parameters){if(!first)result+=',';first=false;result+="{\"key\":"+q(entry.key)+",\"enum_key\":"+(entry.enum_key?"true":"false")+",\"value_json\":"+q(cell_json(entry.value))+'}';}
  return result+"]},\"invoked_arm\":"+q(invoked_arm(call))+",\"parameters_passed\":"+(missing_required(call)?"false":"true")+",\"native_options_present\":false}";
}
inline bool same_call(const Call& before,const Call& after){
  if(before.eager!=after.eager||before.operation!=after.operation||before.argument_key!=after.argument_key||before.keyword_order!=after.keyword_order||before.argument!=after.argument||before.parameter_state!=after.parameter_state||before.raw_parameters!=after.raw_parameters||before.parameters.has_value()!=after.parameters.has_value())return false;
  if(before.parameters){if(before.parameters->size()!=after.parameters->size())return false;for(std::size_t i=0;i<before.parameters->size();++i){const auto& a=before.parameters->at(i);const auto& b=after.parameters->at(i);if(a.key!=b.key||a.enum_key!=b.enum_key||a.value!=b.value)return false;}}
  return controls_json(before)==controls_json(after);
}
template<class Source> auto invoke(const Source& source,const Call& call){
  if(missing_required(call)){
    if(call.operation=="membership")return filter_polars_variants(source,std::nullopt);
    if(call.operation=="from_list")return filter_polars_variants_from_list(source,std::nullopt);
    return filter_polars_variants_top_k(source,std::nullopt);
  }
  if(call.parameters){
    if(call.operation=="membership")return filter_polars_variants_parameters(source,*call.argument,*call.parameters);
    if(call.operation=="from_list")return filter_polars_variants_from_list_parameters(source,*call.argument,*call.parameters);
    if(call.operation=="top_k")return filter_polars_variants_top_k_parameters(source,*call.argument,*call.parameters);
    return auto_filter_polars_variants_parameters(source,*call.parameters);
  }
  if(call.operation=="membership")return filter_polars_variants_parameter_value(source,*call.argument,call.raw_parameters);
  if(call.operation=="from_list")return filter_polars_variants_from_list_parameter_value(source,*call.argument,call.raw_parameters);
  if(call.operation=="top_k")return filter_polars_variants_top_k_parameter_value(source,*call.argument,call.raw_parameters);
  return auto_filter_polars_variants_parameter_value(source,call.raw_parameters);
}
inline std::string returned_metadata(const EventTable& value){
  EventTable header;header.columns=value.columns;header.dtypes=value.dtypes;header.attributes=value.attributes;header.metadata=value.metadata;header.index=value.index;header.categorical_dtypes=value.categorical_dtypes;const auto q=[](std::string_view s){return serialize_embedding_workbench_string_json(s);};
  return "{\"kind\":\"EventTable\",\"row_count_known\":true,\"row_count\":"+std::to_string(value.rows.size())+",\"table_context_json\":"+q(serialize_embedding_workbench_table_json(header))+'}';
}
inline std::string returned_metadata(const LazyEventTable& value){
  EventTable header;header.columns=value.columns();header.dtypes=value.dtypes();header.attributes=value.attributes();header.metadata=value.metadata();header.index=value.index();header.categorical_dtypes=value.categorical_dtypes();const auto q=[](std::string_view s){return serialize_embedding_workbench_string_json(s);};const auto count=value.row_count();
  return "{\"kind\":\"LazyEventTable\",\"row_count_known\":"+std::string(count?"true":"false")+",\"row_count\":"+(count?std::to_string(*count):"null")+",\"table_context_json\":"+q(serialize_embedding_workbench_table_json(header))+'}';
}
} // namespace minera::polars_variants_original_call_detail
