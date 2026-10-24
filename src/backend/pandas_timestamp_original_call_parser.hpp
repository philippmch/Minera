#pragma once
#include "minera/pandas_timestamp_filter.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/timestamp.hpp"
#include "polars_ltl_original_call_parser.hpp"
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <map>
#include <set>

namespace minera::pandas_timestamp_original_call_detail {
namespace shared=polars_ltl_original_call_detail;
using Node=shared::Node;
[[noreturn]] inline void invalid(std::string_view message){throw std::invalid_argument("original pandas timestamp call_json: "+std::string(message));}
inline void exact(const Node& n,std::initializer_list<std::string_view> fields){
  if(n.kind!=Node::kObject||n.fields.size()!=fields.size())invalid("invalid packed fields");
  for(auto key:fields)if(!shared::field(n,key))invalid("missing packed field");
}
inline const Node& at(const Node& n,std::string_view key){const auto* p=shared::field(n,key);if(!p)invalid("missing packed field");return *p;}
inline std::string string(const Node& n){if(n.kind!=Node::kString)invalid("string required");return n.text;}
inline std::string tag(const Node& n){return string(at(n,"type"));}
inline EventAttribute null_value(std::string key={}){EventAttribute a{std::move(key),""};a.is_null=true;return a;}
// Public signatures name the DataFrame first; descriptor args hold the remaining positional operands.
inline const std::vector<std::string>& argument_names(std::string_view operation){
  static const std::vector<std::string> none,window={"dt1","dt2"},attribute={"attribute","attribute_value","dt1","dt2"};
  if(operation=="apply"||operation=="apply_auto_filter")return none;
  if(operation=="filter_traces_attribute_in_timeframe")return attribute;
  return window;
}
inline const std::map<std::string,std::string>& native_functions(){
  static const std::map<std::string,std::string> names={{"apply","apply_pandas_timestamp_filter"},{"apply_auto_filter","auto_filter_pandas_timestamp"},
    {"apply_events","filter_pandas_timestamp_events"},{"filter_traces_contained","filter_pandas_timestamp_contained"},{"filter_traces_intersecting","filter_pandas_timestamp_intersecting"},
    {"filter_traces_starting_in_timeframe","filter_pandas_timestamp_starting"},{"filter_traces_completing_in_timeframe","filter_pandas_timestamp_completing"},
    {"filter_traces_attribute_in_timeframe","filter_pandas_timestamp_attribute"}};
  return names;
}
inline std::string integer_token(std::string_view raw,const Node& n){
  auto s=std::string(raw.substr(n.begin,n.end-n.begin));const auto first=!s.empty()&&s[0]=='-'?1U:0U;
  if(n.kind!=Node::kNumber||first==s.size()||s.find_first_not_of("0123456789",first)!=std::string::npos)invalid("integer token required");return s;
}
inline double floating(const std::string& s){
  if(s=="nan")return std::numeric_limits<double>::quiet_NaN();if(s=="inf")return std::numeric_limits<double>::infinity();if(s=="-inf")return -std::numeric_limits<double>::infinity();
  const auto begin=s.starts_with('-')?1U:0U;if(s.size()<=begin+2||s.substr(begin,2)!="0x"||s.find('p',begin+2)==std::string::npos)invalid("float requires Python hexadecimal form");
  char* end=nullptr;const auto v=std::strtod(s.c_str(),&end);if(end!=s.c_str()+s.size()||!std::isfinite(v))invalid("invalid hexadecimal float");return v;
}
inline std::string enum_key(const Node& n){
  exact(n,{"type","module","name","value"});if(tag(n)!="enum"||string(at(n,"module"))!="pm4py.algo.filtering.pandas.timestamp.timestamp_filter")invalid("unsupported enum module");
  static const std::map<std::string,std::string> names={{"TIMESTAMP_KEY","pm4py:param:timestamp_key"},{"CASE_ID_KEY","pm4py:param:case_id_key"},{"KEEP_NAN_VALUES","keep_nan_values"}};
  const auto found=names.find(string(at(n,"name")));if(found==names.end())invalid("unsupported enum identity");const auto& v=at(n,"value");exact(v,{"type","value"});
  if(tag(v)!="str"||string(at(v,"value"))!=found->second)invalid("enum underlying value mismatch");return found->second;
}
inline std::string dictionary_key(const Node& k,bool& is_enum){
  is_enum=tag(k)=="enum";if(is_enum)return enum_key(k);
  if(tag(k)!="str")invalid("dictionary keys require strings or public enum");exact(k,{"type","value"});return string(at(k,"value"));
}
// Private projection of the pandas timestamp producer grammar; other families keep their own.
inline EventAttribute packed(std::string_view raw,const Node& n,std::string key={}){
  const auto type=tag(n);
  if(type=="pandas.NA"||type=="pandas.NaT"){
    exact(n,{"type"});if(type=="pandas.NA")return make_native_na_attribute(std::move(key));
    EventAttribute a{std::move(key),"NaT",AttributeKind::kDate,true};a.typed_value=NativeDate{"NaT",NativeDateResolution::kNanosecond};return a;
  }
  if(type=="enum"){(void)enum_key(n);return packed(raw,at(n,"value"),std::move(key));}
  if(type=="NoneType"||type=="str"||type=="bool"||type=="int"){
    exact(n,{"type","value"});const auto& v=at(n,"value");
    if(type=="NoneType"){if(v.kind!=Node::kNull)invalid("NoneType requires null");return null_value(std::move(key));}
    if(type=="str")return EventAttribute{std::move(key),string(v)};
    if(type=="bool"){if(v.kind!=Node::kBoolean)invalid("bool requires Boolean");EventAttribute a{std::move(key),v.boolean?"true":"false",AttributeKind::kBoolean};a.typed_value=v.boolean;return a;}
    auto s=integer_token(raw,v);EventAttribute a{std::move(key),s,AttributeKind::kInt};std::int64_t signed_value=0;
    const auto sp=std::from_chars(s.data(),s.data()+s.size(),signed_value);if(sp.ec==std::errc{}&&sp.ptr==s.data()+s.size()){a.value=std::to_string(signed_value);a.typed_value=signed_value;return a;}
    if(s[0]!='-'){std::uint64_t unsigned_value=0;const auto up=std::from_chars(s.data(),s.data()+s.size(),unsigned_value);if(up.ec==std::errc{}&&up.ptr==s.data()+s.size()){a.value=std::to_string(unsigned_value);a.typed_value=unsigned_value;}}return a;
  }
  if(type=="float"){exact(n,{"type","hex"});const auto v=floating(string(at(n,"hex")));EventAttribute a{std::move(key),std::to_string(v),AttributeKind::kFloat};a.typed_value=v;return a;}
  if(type=="bytes"){
    exact(n,{"type","hex"});const auto s=string(at(n,"hex"));if(s.size()%2)invalid("bytes require even hexadecimal text");std::string bytes;
    const auto digit=[](char c)->int{return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;};
    for(std::size_t i=0;i<s.size();i+=2){const auto a=digit(s[i]),b=digit(s[i+1]);if(a<0||b<0)invalid("bytes require lowercase hexadecimal text");bytes+=static_cast<char>(16*a+b);}return make_bytes_attribute(std::move(key),bytes);
  }
  if(type=="pandas.Timestamp"||type=="datetime"){
    if(type=="pandas.Timestamp")exact(n,{"type","iso","resolution","timezone"});else exact(n,{"type","iso"});
    const auto iso=string(at(n,"iso"));const auto instant=parse_timestamp_exact(iso);if(!instant)invalid("invalid date ISO value");std::optional<std::string> zone;auto resolution=NativeDateResolution::kMicrosecond;
    if(type=="pandas.Timestamp"){
      const auto unit=string(at(n,"resolution"));if(unit!="s"&&unit!="ms"&&unit!="us"&&unit!="ns")invalid("unsupported Timestamp resolution");resolution=unit=="ns"?NativeDateResolution::kNanosecond:NativeDateResolution::kMicrosecond;
      const auto& z=at(n,"timezone");
      if(z.kind!=Node::kNull){zone=string(z);if(zone->empty())invalid("empty Timestamp timezone is outside the observed control domain");if(!instant->timezone_aware||(*zone=="UTC"&&instant->offset_minutes!=0))invalid("Timestamp timezone contradicts ISO");}
      else if(instant->timezone_aware)invalid("Timestamp null timezone contradicts aware ISO");
    }
    EventAttribute a{std::move(key),iso,AttributeKind::kDate};a.typed_value=NativeDate{iso,resolution,std::move(zone)};return a;
  }
  if(type=="dict"){
    exact(n,{"type","items"});const auto& items=at(n,"items");if(items.kind!=Node::kArray)invalid("dictionary items require pairs");EventAttribute a{std::move(key),""};a.container=AttributeContainerKind::kContainer;std::set<std::pair<bool,std::string>> identities;
    for(const auto& pair:items.items){if(pair.kind!=Node::kArray||pair.items.size()!=2)invalid("dictionary items require pairs");bool is_enum=false;auto name=dictionary_key(pair.items[0],is_enum);
      if(!identities.emplace(is_enum,name).second)invalid("duplicate exact dictionary key");a.children.push_back(packed(raw,pair.items[1],std::move(name)));}return a;
  }
  if(type=="list"||type=="tuple"||type=="set"||type=="frozenset"){
    exact(n,{"type","values"});const auto& values=at(n,"values");if(values.kind!=Node::kArray)invalid("sequence values require array");EventAttribute a{std::move(key),""};
    a.container=type=="list"?AttributeContainerKind::kList:type=="tuple"?AttributeContainerKind::kTuple:type=="set"?AttributeContainerKind::kSet:AttributeContainerKind::kFrozenSet;
    for(std::size_t i=0;i<values.items.size();++i)a.children.push_back(packed(raw,values.items[i],std::to_string(i)));return a;
  }
  invalid("unsupported packed value or misplaced DataFrame reference");
}
struct Call {
  std::vector<EventAttribute> args;
  EventAttribute raw_parameters=null_value();
  std::optional<std::vector<PandasTimestampFilterParameter>> parameters;
  std::string parameter_state="omitted";
  bool keyword_present=false;
};
inline Call parse(std::string_view raw,std::string_view operation){
  try{
    if(raw.starts_with(std::string_view("\xef\xbb\xbf",3)))raw.remove_prefix(3);
    const auto root=shared::Reader(raw).read();exact(root,{"schema","args","keyword_entries"});if(string(at(root,"schema"))!="minera-pandas-timestamp-original-call-v1")invalid("unsupported schema");
    const auto& args=at(root,"args");const auto& keywords=at(root,"keyword_entries");const auto& names=argument_names(operation);
    if(args.kind!=Node::kArray||args.items.size()!=names.size())invalid("args requires exactly "+std::to_string(names.size())+" positional operands after the DataFrame");
    if(keywords.kind!=Node::kArray||keywords.items.size()>1)invalid("keyword_entries requires at most one parameters binding");Call call;
    for(const auto& value:args.items)call.args.push_back(packed(raw,value));
    if(keywords.items.empty())return call;
    const auto& item=keywords.items[0];exact(item,{"key","value"});if(string(at(item,"key"))!="parameters")invalid("unknown keyword");call.keyword_present=true;const auto& parameter=at(item,"value");
    if(tag(parameter)!="dict"){call.raw_parameters=packed(raw,parameter);call.parameter_state=tag(parameter)=="NoneType"?"null":"raw";return call;}
    exact(parameter,{"type","items"});const auto& pairs=at(parameter,"items");if(pairs.kind!=Node::kArray)invalid("parameter dictionary requires pairs");
    std::vector<PandasTimestampFilterParameter> entries;bool any_enum=false;std::set<std::pair<bool,std::string>> identities;
    for(const auto& pair:pairs.items){
      if(pair.kind!=Node::kArray||pair.items.size()!=2)invalid("parameter dictionary requires pairs");bool is_enum=false;auto name=dictionary_key(pair.items[0],is_enum);
      if(!identities.emplace(is_enum,name).second)invalid("duplicate exact parameter identity");any_enum|=is_enum;entries.push_back({packed(raw,pair.items[1],std::move(name)),is_enum});
    }
    call.parameter_state=any_enum?"entries":"mapping";
    // Only the six active filters have a native ordered-entry overload; apply helpers take the raw value.
    if(any_enum&&!names.empty())call.parameters=std::move(entries);
    else{call.raw_parameters=EventAttribute{"",""};call.raw_parameters.container=AttributeContainerKind::kContainer;for(auto& entry:entries)call.raw_parameters.children.push_back(std::move(entry.value));}
    return call;
  }catch(const std::invalid_argument& error){const std::string message=error.what();constexpr std::string_view prefix="original Polars call_json: ";if(message.starts_with(prefix))invalid(std::string_view(message).substr(prefix.size()));throw;}
}
inline std::string cell_json(const EventAttribute& a){EventTable table;table.columns={"value"};table.dtypes={"object"};table.rows={{a}};return serialize_embedding_workbench_table_json(table);}
inline std::string parameters_json(const Call& call){
  const auto q=[](std::string_view x){return serialize_embedding_workbench_string_json(x);};if(!call.parameters)return "{\"kind\":\"value\",\"value_json\":"+q(cell_json(call.raw_parameters))+'}';
  std::string out="{\"kind\":\"parameter_entries\",\"entries\":[";bool first=true;for(const auto& entry:*call.parameters){if(!first)out+=',';first=false;out+="{\"key\":"+q(entry.value.key)+",\"enum_key\":"+(entry.enum_key?"true":"false")+",\"value_json\":"+q(cell_json(entry.value))+'}';}return out+"]}";
}
inline std::string native_arm(const Call& call){return call.parameters?"ordered_parameter_entries":"raw_attribute";}
inline std::string controls_json(const Call& call,std::string_view operation){
  const auto q=[](std::string_view x){return serialize_embedding_workbench_string_json(x);};const auto& names=argument_names(operation);
  std::string args="[";for(std::size_t i=0;i<call.args.size();++i){if(i)args+=',';args+="{\"name\":"+q(names.at(i))+",\"position\":"+std::to_string(i)+",\"value_json\":"+q(cell_json(call.args[i]))+'}';}
  return "{\"schema\":\"minera-pandas-timestamp-native-controls-v1\",\"arguments\":"+args+"],\"parameters\":"+parameters_json(call)+",\"parameter_state\":"+q(call.parameter_state)+",\"parameter_native_arm\":"+q(native_arm(call))+'}';
}
inline std::string projection_json(const Call& call,std::string_view operation){
  const auto q=[](std::string_view x){return serialize_embedding_workbench_string_json(x);};std::string names="[";bool first=true;
  for(const auto& name:argument_names(operation)){if(!first)names+=',';first=false;names+=q(name);}
  return "{\"schema\":\"minera-pandas-timestamp-call-projection-v1\",\"operation\":"+q(operation)+",\"native_function\":"+q(native_functions().at(std::string(operation)))+",\"native_argument_names\":"+names+
    "],\"parameters_descriptor_state\":"+q(call.parameter_state)+",\"parameter_native_arm\":"+q(native_arm(call))+",\"projection_scope\":\"retained_native_mapper_construction\"}";
}
} // namespace minera::pandas_timestamp_original_call_detail
