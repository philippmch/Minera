#pragma once
#include "minera/polars_timestamp_filter.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/timestamp.hpp"
#include "polars_ltl_original_call_parser.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <map>
#include <set>

namespace minera::polars_timestamp_original_call_detail {
namespace shared=polars_ltl_original_call_detail;
using Node=shared::Node;
[[noreturn]] inline void invalid(std::string_view message){throw std::invalid_argument("original Polars timestamp call_json: "+std::string(message));}
inline void exact(const Node& n,std::initializer_list<std::string_view> fields){
  if(n.kind!=Node::kObject||n.fields.size()!=fields.size())invalid("invalid packed fields");
  for(auto key:fields)if(!shared::field(n,key))invalid("missing packed field");
}
inline const Node& at(const Node& n,std::string_view key){const auto* p=shared::field(n,key);if(!p)invalid("missing packed field");return *p;}
inline std::string string(const Node& n){if(n.kind!=Node::kString)invalid("string required");return n.text;}
inline std::string tag(const Node& n){return string(at(n,"type"));}
inline EventAttribute null_value(std::string key={}){EventAttribute a{std::move(key),""};a.is_null=true;return a;}
// The public functions take every operand after the frame as a keyword.
inline const std::vector<std::string>& keyword_names(std::string_view operation){
  static const std::vector<std::string> none,window={"dt1","dt2"},attribute={"attribute","attribute_value","dt1","dt2"};
  if(operation=="apply"||operation=="apply_auto_filter")return none;
  if(operation=="filter_traces_attribute_in_timeframe")return attribute;
  return window;
}
inline bool helper(std::string_view operation){return operation=="apply"||operation=="apply_auto_filter";}
inline bool supported(std::string_view operation){
  static const std::set<std::string,std::less<>> names={"apply","apply_auto_filter","apply_events","filter_traces_contained","filter_traces_intersecting","filter_traces_attribute_in_timeframe"};
  return names.contains(operation);
}
inline std::string enum_key(const Node& n){
  exact(n,{"type","module","name","value"});if(tag(n)!="enum"||string(at(n,"module"))!="pm4py.algo.filtering.polars.timestamp.timestamp_filter")invalid("unsupported enum module");
  static const std::map<std::string,std::string> names={{"TIMESTAMP_KEY","pm4py:param:timestamp_key"},{"CASE_ID_KEY","pm4py:param:case_id_key"}};
  const auto found=names.find(string(at(n,"name")));if(found==names.end())invalid("unsupported enum identity");const auto& v=at(n,"value");exact(v,{"type","value"});
  if(tag(v)!="str"||string(at(v,"value"))!=found->second)invalid("enum underlying value mismatch");return found->second;
}
inline bool calendar_date(std::string_view s){
  if(s.size()!=10||s[4]!='-'||s[7]!='-')return false;
  for(std::size_t i:{0U,1U,2U,3U,5U,6U,8U,9U})if(s[i]<'0'||s[i]>'9')return false;return true;
}
// Private projection of the Polars timestamp producer grammar; other families keep their own.
inline EventAttribute packed(std::string_view raw,const Node& n,std::string key={}){
  const auto type=tag(n);
  if(type=="NoneType"||type=="str"||type=="bool"||type=="int"){
    exact(n,{"type","value"});const auto& v=at(n,"value");
    if(type=="NoneType"){if(v.kind!=Node::kNull)invalid("NoneType requires null");return null_value(std::move(key));}
    if(type=="str")return EventAttribute{std::move(key),string(v)};
    if(type=="bool"){if(v.kind!=Node::kBoolean)invalid("bool requires Boolean");EventAttribute a{std::move(key),v.boolean?"true":"false",AttributeKind::kBoolean};a.typed_value=v.boolean;return a;}
    const auto s=std::string(raw.substr(v.begin,v.end-v.begin));if(v.kind!=Node::kNumber||s.empty()||s.find_first_not_of("-0123456789")!=std::string::npos)invalid("integer token required");
    EventAttribute a{std::move(key),"",AttributeKind::kInt};std::int64_t signed_value=0;
    if(const auto r=std::from_chars(s.data(),s.data()+s.size(),signed_value);r.ec==std::errc{}&&r.ptr==s.data()+s.size()){a.value=std::to_string(signed_value);a.typed_value=signed_value;return a;}
    std::uint64_t unsigned_value=0;
    if(const auto r=std::from_chars(s.data(),s.data()+s.size(),unsigned_value);s[0]!='-'&&r.ec==std::errc{}&&r.ptr==s.data()+s.size()){a.value=std::to_string(unsigned_value);a.typed_value=unsigned_value;return a;}
    invalid("integer outside the signed/unsigned 64-bit carrier");
  }
  if(type=="float"){
    exact(n,{"type","hex"});const auto s=string(at(n,"hex"));double v=0;
    if(s=="nan")v=std::numeric_limits<double>::quiet_NaN();else if(s=="inf")v=std::numeric_limits<double>::infinity();else if(s=="-inf")v=-std::numeric_limits<double>::infinity();
    else{const auto begin=s.starts_with('-')?1U:0U;if(s.size()<=begin+2||s.substr(begin,2)!="0x"||s.find('p',begin+2)==std::string::npos)invalid("float requires Python hexadecimal form");
      char* end=nullptr;v=std::strtod(s.c_str(),&end);if(end!=s.c_str()+s.size()||!std::isfinite(v))invalid("invalid hexadecimal float");}
    EventAttribute a{std::move(key),std::to_string(v),AttributeKind::kFloat};a.typed_value=v;return a;
  }
  if(type=="date"||type=="datetime"){
    if(type=="date"){exact(n,{"type","iso"});if(!calendar_date(string(at(n,"iso"))))invalid("invalid date ISO value");}
    else{exact(n,{"type","iso","fold","offset_seconds"});if(!parse_timestamp_exact(string(at(n,"iso"))))invalid("invalid datetime ISO value");
      const auto& fold=at(n,"fold");const auto& offset=at(n,"offset_seconds");if(fold.kind!=Node::kNumber||(offset.kind!=Node::kNumber&&offset.kind!=Node::kNull))invalid("invalid datetime metadata");}
    const auto iso=string(at(n,"iso"));EventAttribute a{std::move(key),iso,AttributeKind::kDate};a.typed_value=NativeDate{iso};return a;
  }
  if(type=="dict"){
    exact(n,{"type","items"});const auto& items=at(n,"items");if(items.kind!=Node::kArray)invalid("dictionary items require pairs");EventAttribute a{std::move(key),""};a.container=AttributeContainerKind::kContainer;std::set<std::string> names;
    for(const auto& pair:items.items){if(pair.kind!=Node::kArray||pair.items.size()!=2)invalid("dictionary items require pairs");const auto& k=pair.items[0];if(tag(k)!="str")invalid("nested dictionary keys require strings");exact(k,{"type","value"});
      auto name=string(at(k,"value"));if(!names.insert(name).second)invalid("duplicate exact dictionary key");a.children.push_back(packed(raw,pair.items[1],std::move(name)));}return a;
  }
  if(type=="list"||type=="tuple"||type=="set"){
    exact(n,{"type","values"});const auto& values=at(n,"values");if(values.kind!=Node::kArray)invalid("sequence values require array");EventAttribute a{std::move(key),""};
    a.container=type=="list"?AttributeContainerKind::kList:type=="tuple"?AttributeContainerKind::kTuple:AttributeContainerKind::kSet;
    for(const auto& value:values.items)a.children.push_back(packed(raw,value));return a;
  }
  invalid("unsupported packed value or misplaced frame reference");
}
struct Call {
  bool eager=false;
  std::vector<std::string> keyword_order;
  std::map<std::string,EventAttribute> operands;
  EventAttribute raw_parameters=null_value();
  std::optional<std::vector<PolarsTimestampFilterParameter>> parameters;
  std::string parameter_state="omitted";
  bool keyword_present=false;
};
inline Call parse(std::string_view raw,std::string_view operation){
  try{
    if(raw.starts_with(std::string_view("\xef\xbb\xbf",3)))raw.remove_prefix(3);
    const auto root=shared::Reader(raw).read();exact(root,{"schema","source_kind","keyword_entries"});if(string(at(root,"schema"))!="minera-polars-timestamp-original-call-v1")invalid("unsupported schema");
    Call call;const auto kind=string(at(root,"source_kind"));if(kind!="DataFrame"&&kind!="LazyFrame")invalid("source_kind requires DataFrame or LazyFrame");call.eager=kind=="DataFrame";
    const auto& keywords=at(root,"keyword_entries");if(keywords.kind!=Node::kArray)invalid("keyword_entries requires an array");
    const auto& names=keyword_names(operation);const std::set<std::string> required(names.begin(),names.end());const Node* parameter=nullptr;
    for(const auto& item:keywords.items){
      exact(item,{"key","value"});auto name=string(at(item,"key"));
      if(std::find(call.keyword_order.begin(),call.keyword_order.end(),name)!=call.keyword_order.end())invalid("duplicate keyword");
      if(name=="parameters")parameter=&at(item,"value");else if(required.contains(name))call.operands.emplace(name,packed(raw,at(item,"value")));else invalid("unknown keyword for this operation");
      call.keyword_order.push_back(std::move(name));
    }
    if(call.operands.size()!=required.size())invalid("missing required keyword operand");
    if(!parameter)return call;
    call.keyword_present=true;
    if(tag(*parameter)!="dict"){call.raw_parameters=packed(raw,*parameter);call.parameter_state=tag(*parameter)=="NoneType"?"null":"raw";return call;}
    exact(*parameter,{"type","items"});const auto& pairs=at(*parameter,"items");if(pairs.kind!=Node::kArray)invalid("parameter dictionary requires pairs");
    std::vector<PolarsTimestampFilterParameter> entries;bool any_enum=false;std::set<std::pair<bool,std::string>> identities;
    for(const auto& pair:pairs.items){
      if(pair.kind!=Node::kArray||pair.items.size()!=2)invalid("parameter dictionary requires pairs");const auto& k=pair.items[0];const bool is_enum=tag(k)=="enum";std::string name;
      if(is_enum)name=enum_key(k);else{if(tag(k)!="str")invalid("parameter keys require string or public enum");exact(k,{"type","value"});name=string(at(k,"value"));}
      if(!identities.emplace(is_enum,name).second)invalid("duplicate exact parameter identity");any_enum|=is_enum;entries.push_back({name,packed(raw,pair.items[1]),is_enum});
    }
    call.parameter_state=any_enum?"entries":"mapping";
    // Every active filter takes the ordered binding vector; the helpers flatten keys to one raw value.
    if(!helper(operation))call.parameters=std::move(entries);
    else{call.raw_parameters=EventAttribute{"",""};call.raw_parameters.container=AttributeContainerKind::kContainer;for(auto& entry:entries){entry.value.key=entry.key;call.raw_parameters.children.push_back(std::move(entry.value));}}
    return call;
  }catch(const std::invalid_argument& error){const std::string message=error.what();constexpr std::string_view prefix="original Polars call_json: ";if(message.starts_with(prefix))invalid(std::string_view(message).substr(prefix.size()));throw;}
}
inline std::string cell_json(const EventAttribute& a){EventTable table;table.columns={"value"};table.dtypes={"object"};table.rows={{a}};return serialize_embedding_workbench_table_json(table);}
inline std::string native_arm(const Call& call){return call.parameters?"ordered_parameter_entries":"raw_attribute";}
inline std::string controls_json(const Call& call,std::string_view operation){
  const auto q=[](std::string_view x){return serialize_embedding_workbench_string_json(x);};std::string args="[";bool first=true;
  for(const auto& name:keyword_names(operation)){if(!first)args+=',';first=false;args+="{\"name\":"+q(name)+",\"value_json\":"+q(cell_json(call.operands.at(name)))+'}';}
  std::string parameters;
  if(!call.parameters)parameters="{\"kind\":\"value\",\"value_json\":"+q(cell_json(call.raw_parameters))+'}';
  else{parameters="{\"kind\":\"parameter_entries\",\"entries\":[";first=true;for(const auto& e:*call.parameters){if(!first)parameters+=',';first=false;parameters+="{\"key\":"+q(e.key)+",\"enum_key\":"+(e.enum_key?"true":"false")+",\"value_json\":"+q(cell_json(e.value))+'}';}parameters+="]}";}
  return "{\"schema\":\"minera-polars-timestamp-native-controls-v1\",\"source_kind\":"+q(call.eager?"DataFrame":"LazyFrame")+",\"arguments\":"+args+"],\"parameters\":"+parameters+",\"parameter_state\":"+q(call.parameter_state)+",\"parameter_native_arm\":"+q(native_arm(call))+'}';
}
inline std::string native_function(const Call& call,std::string_view operation){
  if(operation=="apply")return "apply_polars_timestamp_filter";if(operation=="apply_auto_filter")return "auto_filter_polars_timestamp";
  const std::string base=operation=="apply_events"?"events":operation=="filter_traces_contained"?"contained":operation=="filter_traces_intersecting"?"intersecting":"attribute";
  return "filter_polars_timestamp_"+base+(call.parameters?"_parameters":"_parameter_value");
}
inline std::string projection_json(const Call& call,std::string_view operation){
  const auto q=[](std::string_view x){return serialize_embedding_workbench_string_json(x);};std::string names="[",order="[";bool first=true;
  for(const auto& name:keyword_names(operation)){if(!first)names+=',';first=false;names+=q(name);}
  first=true;for(const auto& name:call.keyword_order){if(!first)order+=',';first=false;order+=q(name);}
  return "{\"schema\":\"minera-polars-timestamp-call-projection-v1\",\"operation\":"+q(operation)+",\"native_function\":"+q(native_function(call,operation))+",\"source_kind\":"+q(call.eager?"DataFrame":"LazyFrame")+
    ",\"native_argument_names\":"+names+"],\"keyword_order\":"+order+"],\"parameters_descriptor_state\":"+q(call.parameter_state)+",\"parameter_native_arm\":"+q(native_arm(call))+",\"projection_scope\":\"retained_native_mapper_construction\"}";
}
} // namespace minera::polars_timestamp_original_call_detail
