#pragma once
#include "minera/log_variants_filter.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/timestamp.hpp"
#include "polars_ltl_original_call_parser.hpp"
#include <charconv>
#include <algorithm>
#include <map>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace minera::log_variants_original_call_detail {
namespace shared=polars_ltl_original_call_detail;
using Node=shared::Node;
[[noreturn]] inline void invalid(std::string_view message){throw std::invalid_argument("original LOG variants call_json: "+std::string(message));}
inline void exact(const Node& n,std::initializer_list<std::string_view> fields){
  if(n.kind!=Node::kObject||n.fields.size()!=fields.size())invalid("invalid packed fields");
  for(auto name:fields)if(!shared::field(n,name))invalid("missing packed field");
}
inline const Node& at(const Node& n,std::string_view key){const auto* v=shared::field(n,key);if(!v)invalid("missing packed field");return *v;}
inline std::string string(const Node& n){if(n.kind!=Node::kString)invalid("string required");return n.text;}
inline std::string tag(const Node& n){return string(at(n,"type"));}
inline std::string token(std::string_view raw,const Node& n){return std::string(raw.substr(n.begin,n.end-n.begin));}
inline std::string integer_token(std::string_view raw,const Node& n){
  auto s=token(raw,n);if(n.kind!=Node::kNumber||s.empty())invalid("integer token required");
  const auto begin=s[0]=='-'?1U:0U;if(begin==s.size()||s.find_first_not_of("0123456789",begin)!=std::string::npos)invalid("integer token required");return s;
}
inline std::size_t index(std::string_view raw,const Node& n){
  const auto s=integer_token(raw,n);std::size_t value=0;if(s[0]=='-')invalid("trace index must be nonnegative");
  const auto p=std::from_chars(s.data(),s.data()+s.size(),value);if(p.ec!=std::errc{}||p.ptr!=s.data()+s.size())invalid("trace index outside native range");return value;
}
inline EventAttribute real(double value,std::string key={}){EventAttribute a{std::move(key),std::to_string(value),AttributeKind::kFloat};a.typed_value=value;return a;}
inline bool canonical_uuid(std::string_view s){
  if(s.size()!=36)return false;for(std::size_t i=0;i<s.size();++i)if(i==8||i==13||i==18||i==23){if(s[i]!='-')return false;}else if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f')))return false;return true;
}
inline double floating(const std::string& s){
  if(s=="nan")return std::numeric_limits<double>::quiet_NaN();
  if(s=="inf")return std::numeric_limits<double>::infinity();
  if(s=="-inf")return -std::numeric_limits<double>::infinity();
  const auto begin=s.starts_with('-')?1U:0U;
  if(s.size()<=begin+2||s.substr(begin,2)!="0x"||s.find('p',begin+2)==std::string::npos)invalid("float requires Python hexadecimal form");
  char* end=nullptr;const auto value=std::strtod(s.c_str(),&end);
  if(end!=s.c_str()+s.size()||!std::isfinite(value))invalid("invalid hexadecimal float");return value;
}
// Private codec matches the unchanged original generator, including display
// strings, indexed child keys and NativeDate default metadata.
inline EventAttribute packed(std::string_view raw,const Node& n,std::string key={}){
  const auto type=tag(n);
  if(type=="native_na"||type=="nat"){
    exact(n,{"type"});if(type=="native_na")return make_native_na_attribute(std::move(key));
    EventAttribute a{std::move(key),"NaT",AttributeKind::kDate,true};a.typed_value=NativeDate{"NaT",NativeDateResolution::kNanosecond};return a;
  }
  if(type=="NoneType"||type=="str"||type=="bool"||type=="int"||type=="uuid"){
    exact(n,{"type","value"});const auto& v=at(n,"value");
    if(type=="NoneType"){if(v.kind!=Node::kNull)invalid("NoneType requires null");EventAttribute a{std::move(key),""};a.is_null=true;return a;}
    if(type=="str")return EventAttribute{std::move(key),string(v)};
    if(type=="uuid"){auto s=string(v);if(!canonical_uuid(s))invalid("UUID requires canonical lowercase text");return EventAttribute{std::move(key),std::move(s),AttributeKind::kId};}
    if(type=="bool"){if(v.kind!=Node::kBoolean)invalid("bool requires Boolean");EventAttribute a{std::move(key),v.boolean?"true":"false",AttributeKind::kBoolean};a.typed_value=v.boolean;return a;}
    auto s=integer_token(raw,v);EventAttribute a{std::move(key),s,AttributeKind::kInt};
    std::int64_t signed_value=0;const auto sp=std::from_chars(s.data(),s.data()+s.size(),signed_value);
    if(sp.ec==std::errc{}&&sp.ptr==s.data()+s.size()){a.value=std::to_string(signed_value);a.typed_value=signed_value;return a;}
    if(s[0]!='-'){std::uint64_t unsigned_value=0;const auto up=std::from_chars(s.data(),s.data()+s.size(),unsigned_value);if(up.ec==std::errc{}&&up.ptr==s.data()+s.size()){a.value=std::to_string(unsigned_value);a.typed_value=unsigned_value;}}
    return a;
  }
  if(type=="float"){exact(n,{"type","hex"});return real(floating(string(at(n,"hex"))),std::move(key));}
  if(type=="bytes"){
    exact(n,{"type","hex"});const auto s=string(at(n,"hex"));if(s.size()%2)invalid("bytes require even hexadecimal text");std::string bytes;
    const auto digit=[](char c)->int{return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;};
    for(std::size_t i=0;i<s.size();i+=2){const auto a=digit(s[i]),b=digit(s[i+1]);if(a<0||b<0)invalid("bytes require lowercase hexadecimal text");bytes+=static_cast<char>(16*a+b);}return make_bytes_attribute(std::move(key),bytes);
  }
  if(type=="datetime"){
    exact(n,{"type","iso","fold","offset_seconds"});const auto iso=string(at(n,"iso"));const auto fold=integer_token(raw,at(n,"fold"));
    const auto parsed=parse_timestamp_exact(iso);if((fold!="0"&&fold!="1")||!parsed)invalid("invalid datetime metadata");
    const auto& offset=at(n,"offset_seconds");
    if(!parsed->timezone_aware){if(offset.kind!=Node::kNull)invalid("datetime offset contradicts naive ISO");}
    else{if(offset.kind!=Node::kNumber)invalid("aware datetime requires numeric offset");const auto s=token(raw,offset);char* end=nullptr;const auto number=std::strtod(s.c_str(),&end);if(end!=s.c_str()+s.size()||!std::isfinite(number)||number!=static_cast<double>(parsed->offset_minutes)*60.0)invalid("datetime offset contradicts ISO");}
    EventAttribute a{std::move(key),iso,AttributeKind::kDate};a.typed_value=NativeDate{iso};return a;
  }
  if(type=="dict"){
    exact(n,{"type","items"});const auto& items=at(n,"items");if(items.kind!=Node::kArray)invalid("dictionary items require pairs");
    EventAttribute a{std::move(key),""};a.container=AttributeContainerKind::kContainer;std::set<std::string> names;
    for(const auto& pair:items.items){if(pair.kind!=Node::kArray||pair.items.size()!=2)invalid("dictionary items require pairs");const auto& k=pair.items[0];if(tag(k)!="str")invalid("ordinary dictionary keys require strings");exact(k,{"type","value"});auto name=string(at(k,"value"));if(!names.insert(name).second)invalid("duplicate dictionary key");a.children.push_back(packed(raw,pair.items[1],std::move(name)));}return a;
  }
  if(type=="list"||type=="tuple"||type=="set"||type=="frozenset"){
    exact(n,{"type","values"});const auto& values=at(n,"values");if(values.kind!=Node::kArray)invalid("sequence values require an array");EventAttribute a{std::move(key),""};
    a.container=type=="list"?AttributeContainerKind::kList:type=="tuple"?AttributeContainerKind::kTuple:type=="set"?AttributeContainerKind::kSet:AttributeContainerKind::kFrozenSet;
    for(std::size_t i=0;i<values.items.size();++i)a.children.push_back(packed(raw,values.items[i],std::to_string(i)));return a;
  }
  invalid("unsupported packed value or misplaced enum/trace_ref");
}
struct ParameterEntry {EventAttribute key,value;bool enum_key=false;std::string enum_module,enum_name;};
struct Operand {
  enum Kind {kValue,kAdmission,kVariants,kParameters} kind=kValue;
  EventAttribute value;
  std::vector<std::pair<EventAttribute,EventAttribute>> admission;
  LogVariantsAdmissionDictionary native_admission;
  LogVariantsDictionary variants;
  std::vector<std::vector<std::size_t>> trace_indices;
  std::vector<ParameterEntry> parameters;
};
inline Operand operand(std::string_view raw,const Node& n,std::string_view role){
  Operand out;if(tag(n)!="dict"){out.value=packed(raw,n);return out;}
  if(role!="admitted_variants"&&role!="variants"&&role!="parameters"){out.value=packed(raw,n);return out;}
  exact(n,{"type","items"});const auto& pairs=at(n,"items");if(pairs.kind!=Node::kArray)invalid("dictionary items require pairs");
  out.kind=role=="admitted_variants"?Operand::kAdmission:role=="variants"?Operand::kVariants:Operand::kParameters;
  std::set<std::pair<bool,std::string>> identities;
  for(const auto& pair:pairs.items){
    if(pair.kind!=Node::kArray||pair.items.size()!=2)invalid("dictionary items require pairs");const auto& k=pair.items[0];const auto& v=pair.items[1];
    if(out.kind==Operand::kParameters){
      ParameterEntry entry;
      if(tag(k)=="enum"){
        exact(k,{"type","module","name","value"});entry.enum_key=true;entry.enum_module=string(at(k,"module"));entry.enum_name=string(at(k,"name"));
        if(entry.enum_module!="pm4py.algo.filtering.log.variants.variants_filter")invalid("unsupported parameter enum module");
        const std::map<std::string,std::string> known={{"ACTIVITY_KEY","pm4py:param:activity_key"},{"POSITIVE","positive"},{"DECREASING_FACTOR","decreasingFactor"}};
        const auto found=known.find(entry.enum_name);if(found==known.end())invalid("unsupported parameter enum identity");entry.key=packed(raw,at(k,"value"));if(entry.key.kind!=AttributeKind::kString||entry.key.value!=found->second||entry.key.container!=AttributeContainerKind::kScalar||entry.key.is_null)invalid("parameter enum value mismatch");
      }else{if(tag(k)!="str")invalid("parameter keys require string or public enum");entry.key=packed(raw,k);}
      if(!identities.emplace(entry.enum_key,entry.key.value).second)invalid("duplicate exact parameter identity");entry.value=packed(raw,v);out.parameters.push_back(std::move(entry));
    }else if(out.kind==Operand::kAdmission){auto key=packed(raw,k);out.native_admission.keys.push_back(key);out.admission.push_back({std::move(key),packed(raw,v)});
    }else{
      LogVariantsDictionaryEntry entry;entry.variant=packed(raw,k);std::vector<std::size_t> indices;bool traces=false;
      if(tag(v)=="list"){
        exact(v,{"type","values"});const auto& values=at(v,"values");if(values.kind!=Node::kArray)invalid("list values require array");traces=true;
        for(const auto& child:values.items)if(tag(child)!="trace_ref"){traces=false;break;}
        if(traces)for(const auto& child:values.items){exact(child,{"type","source_trace_index"});indices.push_back(index(raw,at(child,"source_trace_index")));}
      }
      if(!traces)entry.ordinary_value=packed(raw,v);out.variants.push_back(std::move(entry));out.trace_indices.push_back(std::move(indices));
    }
  }
  return out;
}
struct Call {
  std::vector<Operand> args;
  std::vector<std::pair<std::string,Operand>> keywords;
  std::vector<std::pair<bool,std::size_t>> bindings;
  bool options_present=true;
  LogVariantsFilterOptions options;
  std::optional<EventAttribute> bound;
  std::string parameter_state="omitted";
  Operand& binding(std::size_t i){const auto [keyword,offset]=bindings.at(i);return keyword?keywords.at(offset).second:args.at(offset);}
  const Operand& binding(std::size_t i)const{const auto [keyword,offset]=bindings.at(i);return keyword?keywords.at(offset).second:args.at(offset);}
};
inline std::vector<std::string> signature(std::string_view operation){
  if(operation=="apply")return {"admitted_variants","parameters"};
  if(operation=="filter_log_variants_percentage")return {"percentage","parameters"};
  if(operation=="filter_variants_by_coverage_percentage")return {"min_coverage_percentage","parameters"};
  if(operation=="filter_variants_by_maximum_coverage_percentage")return {"max_coverage_percentage","parameters"};
  if(operation=="filter_variants_top_k")return {"k","parameters"};
  if(operation=="filter_variants_variants_percentage")return {"variants","variants_percentage"};
  if(operation=="find_auto_threshold")return {"variants","decreasing_factor"};invalid("unsupported operation");
}
inline Call parse(std::string_view raw,std::string_view operation){
  try{
    if(raw.starts_with(std::string_view("\xef\xbb\xbf",3)))raw.remove_prefix(3);
    const auto root=shared::Reader(raw).read();exact(root,{"schema","args","keyword_entries"});if(string(at(root,"schema"))!="minera-log-variants-original-call-v1")invalid("unsupported schema");
    const auto names=signature(operation);const auto& args=at(root,"args");const auto& keywords=at(root,"keyword_entries");if(args.kind!=Node::kArray||keywords.kind!=Node::kArray)invalid("args and keyword_entries require arrays");if(args.items.size()>names.size())invalid("too many positional operands");
    Call call;call.options_present=names[1]=="parameters";call.parameter_state=call.options_present?"omitted":"not_applicable";
    const auto absent=std::numeric_limits<std::size_t>::max();call.bindings.assign(2,{false,absent});
    for(std::size_t i=0;i<args.items.size();++i){call.bindings[i]={false,i};call.args.push_back(operand(raw,args.items[i],names[i]));}
    for(const auto& item:keywords.items){exact(item,{"key","value"});const auto key=string(at(item,"key"));const auto found=std::find(names.begin(),names.end(),key);if(found==names.end())invalid("unknown public keyword");const auto i=static_cast<std::size_t>(found-names.begin());if(call.bindings[i].second!=absent)invalid("multiple bindings for public argument");call.bindings[i]={true,call.keywords.size()};call.keywords.push_back({key,operand(raw,at(item,"value"),key)});}
    if(call.bindings[0].second==absent){if(operation!="filter_log_variants_percentage")invalid("missing required public argument");call.bound=real(.8);}
    else if(operation!="apply"&&call.options_present)call.bound=call.binding(0).value;
    if(!call.options_present){if(call.bindings[0].second==absent)invalid("missing variants argument");if(call.bindings[1].second!=absent)call.bound=call.binding(1).value;else if(operation=="filter_variants_variants_percentage")call.bound=real(.0);else invalid("missing decreasing_factor argument");}
    if(call.options_present&&call.bindings[1].second!=absent){
      const auto& parameters=call.binding(1);if(parameters.kind==Operand::kParameters){call.parameter_state="entries";for(const auto& entry:parameters.parameters){if(entry.key.value=="pm4py:param:activity_key")call.options.activity_key=entry.value;else if(entry.key.value=="positive")call.options.positive=entry.value;}}
      else if(parameters.value.is_null)call.parameter_state="null";else{call.parameter_state="raw";call.options.raw_parameters=parameters.value;}
    }
    return call;
  }catch(const std::invalid_argument& error){const std::string message=error.what();constexpr std::string_view prefix="original Polars call_json: ";if(message.starts_with(prefix))invalid(std::string_view(message).substr(prefix.size()));throw;}
}
inline void materialize(Call& call,const EventLog& source){
  const auto fill=[&](Operand& operand){if(operand.kind!=Operand::kVariants)return;for(std::size_t i=0;i<operand.variants.size();++i){auto& entry=operand.variants[i];entry.traces.clear();for(const auto offset:operand.trace_indices[i]){if(offset>=source.traces.size())invalid("trace reference outside actual source log");entry.traces.push_back(source.traces[offset]);}}};
  for(auto& operand:call.args)fill(operand);for(auto& item:call.keywords)fill(item.second);
}
inline std::string cell_json(const EventAttribute& a,std::string dtype="object"){EventTable t;t.columns={"value"};t.dtypes={std::move(dtype)};t.rows={{a}};return serialize_embedding_workbench_table_json(t);}
inline std::string failure_text_json(std::string_view value,bool failed){return failed?serialize_embedding_workbench_string_json(value):"null";}
inline std::string operand_json(const Operand& o){
  const auto q=[](std::string_view s){return serialize_embedding_workbench_string_json(s);};const auto cell=[&](const EventAttribute& a){return q(cell_json(a));};
  if(o.kind==Operand::kValue)return "{\"kind\":\"value\",\"value_json\":"+cell(o.value)+'}';
  std::string out="{\"kind\":"+q(o.kind==Operand::kAdmission?"admission_dictionary":o.kind==Operand::kVariants?"variants_dictionary":"parameter_dictionary")+",\"entries\":[";bool first=true;
  const auto comma=[&](){if(!first)out+=',';first=false;};
  if(o.kind==Operand::kAdmission)for(std::size_t i=0;i<o.admission.size();++i){comma();out+="{\"key_json\":"+cell(o.native_admission.keys.at(i))+",\"value_json\":"+cell(o.admission[i].second)+'}';}
  else if(o.kind==Operand::kVariants)for(const auto& entry:o.variants){comma();EventLog log;log.traces=entry.traces;out+="{\"key_json\":"+cell(entry.variant)+",\"traces_json\":"+q(serialize_embedding_workbench_raw_log_json(log))+",\"ordinary_value_json\":"+(entry.ordinary_value?cell(*entry.ordinary_value):"null")+'}';}
  else for(const auto& entry:o.parameters){comma();out+="{\"key_json\":"+cell(entry.key)+",\"value_json\":"+cell(entry.value)+",\"enum_key\":"+(entry.enum_key?"true":"false")+",\"enum_module\":"+(entry.enum_key?q(entry.enum_module):"null")+",\"enum_name\":"+(entry.enum_key?q(entry.enum_name):"null")+'}';}
  return out+"]}";
}
inline std::string controls_json(const Call& call){
  const auto q=[](std::string_view s){return serialize_embedding_workbench_string_json(s);};const auto cell=[&](const EventAttribute& a){return q(cell_json(a));};
  std::string out="{\"args\":[";bool first=true;for(const auto& a:call.args){if(!first)out+=',';first=false;out+=operand_json(a);}out+="],\"keyword_entries\":[";first=true;
  for(const auto& [key,value]:call.keywords){if(!first)out+=',';first=false;out+="{\"key\":"+q(key)+",\"value\":"+operand_json(value)+'}';}
  const auto absent=std::numeric_limits<std::size_t>::max();out+="],\"parameter_state\":"+q(call.parameter_state)+",\"parameters\":"+(call.options_present&&call.bindings[1].second!=absent?operand_json(call.binding(1)):"null")+",\"resolved\":{\"options_present\":"+(call.options_present?"true":"false")+",\"activity_key_json\":"+(call.options_present?cell(call.options.activity_key):"null")+",\"positive_json\":"+(call.options_present?cell(call.options.positive):"null")+",\"raw_parameters_json\":"+(call.options.raw_parameters?cell(*call.options.raw_parameters):"null")+",\"bound_json\":"+(call.bound?cell(*call.bound):"null")+"}}";return out;
}
} // namespace minera::log_variants_original_call_detail
