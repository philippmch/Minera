#pragma once
#include "minera/pandas_variants_filter.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/timestamp.hpp"
#include "polars_ltl_original_call_parser.hpp"
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <map>
#include <set>

namespace minera::pandas_variants_original_call_detail {
namespace shared=polars_ltl_original_call_detail;
using Node=shared::Node;
[[noreturn]] inline void invalid(std::string_view message){throw std::invalid_argument("original pandas variants call_json: "+std::string(message));}
inline void exact(const Node& n,std::initializer_list<std::string_view> fields){
  if(n.kind!=Node::kObject||n.fields.size()!=fields.size())invalid("invalid packed fields");
  for(auto key:fields)if(!shared::field(n,key))invalid("missing packed field");
}
inline const Node& at(const Node& n,std::string_view key){const auto* p=shared::field(n,key);if(!p)invalid("missing packed field");return *p;}
inline std::string string(const Node& n){if(n.kind!=Node::kString)invalid("string required");return n.text;}
inline std::string tag(const Node& n){return string(at(n,"type"));}
inline EventAttribute null_value(std::string key={}){EventAttribute a{std::move(key),""};a.is_null=true;return a;}
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
  exact(n,{"type","module","name","value"});if(tag(n)!="enum"||string(at(n,"module"))!="pm4py.algo.filtering.pandas.variants.variants_filter")invalid("unsupported enum module");
  static const std::map<std::string,std::string> names={{"CASE_ID_KEY","pm4py:param:case_id_key"},{"ACTIVITY_KEY","pm4py:param:activity_key"},{"DECREASING_FACTOR","decreasingFactor"},{"POSITIVE","positive"}};
  const auto found=names.find(string(at(n,"name")));if(found==names.end())invalid("unsupported enum identity");const auto& v=at(n,"value");exact(v,{"type","value"});
  if(tag(v)!="str"||string(at(v,"value"))!=found->second)invalid("enum underlying value mismatch");return found->second;
}
// This private projection follows the original pandas variants generator.
// It deliberately does not change or reuse other families' scalar projection.
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
    for(const auto& pair:items.items){if(pair.kind!=Node::kArray||pair.items.size()!=2)invalid("dictionary items require pairs");const auto& k=pair.items[0];const bool is_enum=tag(k)=="enum";std::string name;
      if(is_enum)name=enum_key(k);else{if(tag(k)!="str")invalid("ordinary dictionary keys require strings or public enum");exact(k,{"type","value"});name=string(at(k,"value"));}
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
  EventAttribute criterion;
  std::optional<PandasVariantsAdmittedMapping> admission;
  EventAttribute raw_parameters=null_value();
  std::optional<std::vector<PandasVariantsFilterParameter>> parameters;
  std::optional<std::size_t> frame_entry;
  std::string parameter_state="omitted";
  bool keyword_present=false;
};
inline Call parse(std::string_view raw,std::string_view operation){
  try{
    if(raw.starts_with(std::string_view("\xef\xbb\xbf",3)))raw.remove_prefix(3);
    const auto root=shared::Reader(raw).read();exact(root,{"schema","args","keyword_entries"});if(string(at(root,"schema"))!="minera-pandas-variants-original-call-v1")invalid("unsupported schema");
    const auto& args=at(root,"args");const auto& keywords=at(root,"keyword_entries");if(args.kind!=Node::kArray||args.items.size()!=1)invalid("args requires exactly one operand");if(keywords.kind!=Node::kArray||keywords.items.size()>1)invalid("keyword_entries requires at most one parameters binding");Call call;
    const auto& criterion=args.items[0];if(operation=="membership"&&tag(criterion)=="dict"){
      exact(criterion,{"type","items"});const auto& pairs=at(criterion,"items");if(pairs.kind!=Node::kArray)invalid("admission dictionary requires pairs");call.admission.emplace();
      for(const auto& pair:pairs.items){if(pair.kind!=Node::kArray||pair.items.size()!=2)invalid("admission dictionary requires pairs");call.admission->entries.emplace_back(packed(raw,pair.items[0]),packed(raw,pair.items[1]));}
    }else call.criterion=packed(raw,criterion);
    if(keywords.items.empty())return call;
    const auto& item=keywords.items[0];exact(item,{"key","value"});if(string(at(item,"key"))!="parameters")invalid("unknown keyword");call.keyword_present=true;const auto& parameter=at(item,"value");
    if(tag(parameter)!="dict"){call.raw_parameters=packed(raw,parameter);call.parameter_state=tag(parameter)=="NoneType"?"null":"raw";return call;}
    exact(parameter,{"type","items"});const auto& pairs=at(parameter,"items");if(pairs.kind!=Node::kArray)invalid("parameter dictionary requires pairs");call.parameter_state="entries";
    std::vector<PandasVariantsFilterParameter> entries;bool vector_arm=false;std::set<std::pair<bool,std::string>> identities;
    for(const auto& pair:pairs.items){
      if(pair.kind!=Node::kArray||pair.items.size()!=2)invalid("parameter dictionary requires pairs");const auto& k=pair.items[0];const auto& v=pair.items[1];const bool is_enum=tag(k)=="enum";std::string name;
      if(is_enum)name=enum_key(k);else{if(tag(k)!="str")invalid("parameter keys require string or public enum");exact(k,{"type","value"});name=string(at(k,"value"));}
      if(!identities.emplace(is_enum,name).second)invalid("duplicate exact parameter identity");vector_arm|=is_enum;
      if(tag(v)=="pandas.DataFrame"){
        exact(v,{"type","file"});if(is_enum||name!="variants_df"||string(at(v,"file"))!="variants_file"||call.frame_entry)invalid("DataFrame requires one direct literal variants_df reference");
        call.frame_entry=entries.size();vector_arm=true;entries.push_back({null_value(name),false,std::nullopt});
      }else entries.push_back({packed(raw,v,name),is_enum,std::nullopt});
    }
    if(vector_arm)call.parameters=std::move(entries);
    else{call.raw_parameters=EventAttribute{"",""};call.raw_parameters.container=AttributeContainerKind::kContainer;for(auto& entry:entries)call.raw_parameters.children.push_back(std::move(entry.value));}
    return call;
  }catch(const std::invalid_argument& error){const std::string message=error.what();constexpr std::string_view prefix="original Polars call_json: ";if(message.starts_with(prefix))invalid(std::string_view(message).substr(prefix.size()));throw;}
}
inline std::string cell_json(const EventAttribute& a){EventTable table;table.columns={"value"};table.dtypes={"object"};table.rows={{a}};return serialize_embedding_workbench_table_json(table);}
inline std::string parameters_json(const Call& call){
  const auto q=[](std::string_view x){return serialize_embedding_workbench_string_json(x);};if(!call.parameters)return "{\"kind\":\"value\",\"value_json\":"+q(cell_json(call.raw_parameters))+'}';
  std::string out="{\"kind\":\"parameter_entries\",\"entries\":[";bool first=true;for(const auto& entry:*call.parameters){if(!first)out+=',';first=false;out+="{\"key\":"+q(entry.value.key)+",\"enum_key\":"+(entry.enum_key?"true":"false")+",\"value_json\":"+q(cell_json(entry.value))+",\"table_json\":"+(entry.table?q(serialize_embedding_workbench_table_json(*entry.table)):"null")+'}';}return out+"]}";
}
inline std::string controls_json(const Call& call){
  const auto q=[](std::string_view x){return serialize_embedding_workbench_string_json(x);};std::string operand;
  if(!call.admission)operand="{\"kind\":\"value\",\"value_json\":"+q(cell_json(call.criterion))+'}';
  else{operand="{\"kind\":\"admission_dictionary\",\"entries\":[";bool first=true;for(const auto& [key,value]:call.admission->entries){if(!first)operand+=',';first=false;operand+="{\"key_json\":"+q(cell_json(key))+",\"value_json\":"+q(cell_json(value))+'}';}operand+="]}";}
  const auto parameters=parameters_json(call);return "{\"args\":["+operand+"],\"keyword_entries\":["+(call.keyword_present?"{\"key\":\"parameters\",\"value\":"+parameters+'}':std::string{})+"],\"parameter_state\":"+q(call.parameter_state)+",\"parameter_native_arm\":"+q(call.parameters?"ordered_parameter_entries":"raw_attribute")+",\"parameters\":"+parameters+",\"native_options_present\":false}";
}
inline const EventTable* variants_table(const Call& call){return call.frame_entry&&call.parameters&&call.parameters->at(*call.frame_entry).table?&*call.parameters->at(*call.frame_entry).table:nullptr;}
} // namespace minera::pandas_variants_original_call_detail
