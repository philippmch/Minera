#include "minera/log_variants_filter.hpp"
#include "minera/timestamp.hpp"
#include "root_cause_trace_key.hpp"
#include "root_cause_unicode.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <optional>

namespace minera {
namespace {
[[noreturn]] void fail(std::string type,std::string message) {throw LogVariantsFilterError(std::move(type),std::move(message));}
bool text(const EventAttribute& value) {
  return !value.is_null && value.container==AttributeContainerKind::kScalar &&
    !value.scalar_wrapper && !value.scalar_children_list && value.children.empty() &&
    value.kind==AttributeKind::kString &&
    std::holds_alternative<std::monostate>(value.typed_value);
}
std::string type(const EventAttribute& value) {
  if(const auto* date=std::get_if<NativeDate>(&value.typed_value);date&&date->value=="NaT")return "NaTType";
  if(value.kind==AttributeKind::kDate&&value.value=="NaT")return "NaTType";
  if(value.is_null)return "NoneType";
  if(value.scalar_wrapper || value.scalar_children_list)return "dict";
  switch(value.container) {
    case AttributeContainerKind::kList:return "list";
    case AttributeContainerKind::kTuple:return "tuple";
    case AttributeContainerKind::kContainer:return "dict";
    case AttributeContainerKind::kSet:return "set";
    case AttributeContainerKind::kFrozenSet:return "frozenset";
    case AttributeContainerKind::kScalar:break;
  }
  if(is_native_na_attribute(value))return "NAType";
  if(value.kind==AttributeKind::kBoolean || std::holds_alternative<bool>(value.typed_value))return "bool";
  if(value.kind==AttributeKind::kInt || std::holds_alternative<std::int64_t>(value.typed_value) || std::holds_alternative<std::uint64_t>(value.typed_value))return "int";
  if(value.kind==AttributeKind::kFloat || std::holds_alternative<double>(value.typed_value))return "float";
  if(value.kind==AttributeKind::kBytes)return "bytes";
  if(value.kind==AttributeKind::kId)return "UUID";
  if(value.kind==AttributeKind::kDate || std::holds_alternative<NativeDate>(value.typed_value))return "datetime.datetime";
  return "str";
}
std::string quoted(std::string_view value) {
  const char quote=value.find('\'')!=std::string_view::npos && value.find('"')==std::string_view::npos?'"':'\'';
  constexpr char hex[]="0123456789abcdef";std::string result(1,quote);
  for(std::size_t at=0;at<value.size();) {
    const auto begin=at;const auto first=static_cast<unsigned char>(value[at++]);char32_t code=first;unsigned n=0;
    if(first<0x80){}else if(first>=0xc2&&first<=0xdf){code=first&31;n=1;}else if(first>=0xe0&&first<=0xef){code=first&15;n=2;}else if(first>=0xf0&&first<=0xf4){code=first&7;n=3;}else fail("ValueError","variants filter selector has invalid UTF-8");
    for(unsigned i=0;i<n;++i){if(at==value.size())fail("ValueError","variants filter selector has short UTF-8");const auto next=static_cast<unsigned char>(value[at++]);if((next&0xc0)!=0x80)fail("ValueError","variants filter selector has invalid UTF-8 continuation");code=(code<<6)|(next&63);}
    if(code==static_cast<unsigned char>(quote)||code=='\\'){result+='\\';result+=static_cast<char>(code);}
    else if(code=='\n')result+="\\n";else if(code=='\r')result+="\\r";else if(code=='\t')result+="\\t";
    else if(!root_cause_unicode_printable(code)){const int digits=code<=0xff?2:code<=0xffff?4:8;result+=digits==2?"\\x":digits==4?"\\u":"\\U";for(int shift=(digits-1)*4;shift>=0;shift-=4)result+=hex[(code>>shift)&15];}
    else result.append(value.substr(begin,at-begin));
  }
  return result+quote;
}
std::string key_repr(const EventAttribute& key) {
  if(text(key))return quoted(key.value);
  if(type(key)=="NaTType")return "NaT";
  if(key.kind==AttributeKind::kId && !key.is_null)return "UUID("+quoted(key.value)+")";
  if(key.is_null)return "None";
  if(type(key)=="bool")return native_attribute_number(key)?"True":"False";
  if(key.container==AttributeContainerKind::kTuple){std::string result="(";for(const auto& child:key.children){if(result.size()>1)result+=", ";result+=key_repr(child);}if(key.children.size()==1)result+=',';return result+')';}
  if(key.container==AttributeContainerKind::kFrozenSet){
    const auto members=root_cause_set_members(key);if(members.empty())return "frozenset()";
    std::string result="frozenset({";for(const auto* child:members){if(result.size()>11)result+=", ";result+=key_repr(*child);}return result+"})";
  }
  if(is_native_na_attribute(key))return "<NA>";
  if(type(key)=="datetime.datetime") {
    // The shared native formatter renders datetime constructors when nested.
    // A temporary tuple supplies that context without changing the value.
    EventAttribute tuple;tuple.container=AttributeContainerKind::kTuple;tuple.children={key};
    const auto nested=root_cause_attribute_category(tuple);return nested.substr(1,nested.size()-3);
  }
  if(type(key)=="float"||type(key)=="int"||type(key)=="bytes")return root_cause_attribute_category(key);
  return key.value;
}
void hash_key(const EventAttribute& key) {
  if(root_cause_key_is_hashable(key))return;
  if(key.container==AttributeContainerKind::kTuple || key.container==AttributeContainerKind::kFrozenSet)
    for(const auto& child:key.children)hash_key(child);
  fail("TypeError","unhashable type: '"+type(key)+"'");
}
std::optional<EventAttribute> present(const EventAttribute& stored) {
  if(stored.first_class_role!=FirstClassAttributeRole::kNone && stored.first_class_state==FirstClassAttributeState::kMissing)return std::nullopt;
  auto value=stored;if(stored.first_class_state==FirstClassAttributeState::kNull)value.is_null=true;return value;
}
std::optional<EventAttribute> event_value(const Event& event,const EventAttribute& key) {
  hash_key(key);if(!text(key))return std::nullopt;
  for(auto at=event.attributes.rbegin();at!=event.attributes.rend();++at)if(at->key==key.value)return present(*at);
  const auto projected=[&](std::string_view canonical,const std::string& source,const std::optional<EventAttribute>& stored,FirstClassAttributeState state,const std::string& value)->std::optional<EventAttribute>{
    const auto actual=!source.empty()?std::string_view(source):stored&&!stored->first_class_source_key.empty()?std::string_view(stored->first_class_source_key):stored?std::string_view(stored->key):canonical;
    if(actual!=key.value || (state==FirstClassAttributeState::kMissing&&value.empty()&&!stored))return std::nullopt;
    auto result=stored?present(*stored):std::make_optional(EventAttribute{key.value,value});
    if(result&&!stored&&canonical=="time:timestamp"){result->kind=AttributeKind::kDate;result->typed_value=NativeDate{value};}
    if(result&&state==FirstClassAttributeState::kNull)result->is_null=true;return result;
  };
  for(const auto value:{projected("concept:name",event.activity_source_key,event.activity_attribute,event.activity_state,event.activity),projected("time:timestamp",event.timestamp_source_key,event.timestamp_attribute,event.timestamp_state,event.timestamp),projected("org:resource",event.resource_source_key,event.resource_attribute,event.resource_state,event.resource),projected("lifecycle:transition",event.lifecycle_source_key,event.lifecycle_attribute,event.lifecycle_state,event.lifecycle)})if(value)return value;
  return std::nullopt;
}
EventAttribute required(const Event& event,const EventAttribute& key) {
  if(auto value=event_value(event,key))return *value;fail("KeyError",key_repr(key));
}
ParsedExactTimestamp date_instant(const EventAttribute& value){
  const auto* native=std::get_if<NativeDate>(&value.typed_value);
  const auto parsed=parse_timestamp_exact(native?native->value:value.value);
  if(!parsed)throw std::invalid_argument("variants filter contains an invalid native datetime");
  return *parsed;
}
bool numeric(const EventAttribute& value){const auto name=type(value);return name=="bool"||name=="int"||name=="float";}
EventAttribute integer(std::size_t value){EventAttribute out{"",std::to_string(value),AttributeKind::kInt};out.typed_value=static_cast<std::uint64_t>(value);return out;}
EventAttribute real(double value){EventAttribute out{"","",AttributeKind::kFloat};out.typed_value=value;return out;}
bool truth(const EventAttribute& value){
  if(type(value)=="NoneType")return false;
  if(is_native_na_attribute(value))fail("TypeError","boolean value of NA is ambiguous");
  if(value.container!=AttributeContainerKind::kScalar)return !value.children.empty();
  if(numeric(value))return native_attribute_number(value)!=0;
  if(text(value)||value.kind==AttributeKind::kBytes)return !value.value.empty();return true;
}
// Dictionary key equality uses hash-compatible ordinary values. Sequential
// membership also evaluates ambiguous equality against unlike NA cells.
bool equal(const EventAttribute& a,const EventAttribute& b,bool dictionary=false){
  if(type(a)=="NoneType"||type(b)=="NoneType")return type(a)==type(b);
  if(is_native_na_attribute(a)||is_native_na_attribute(b)){
    if(is_native_na_attribute(a)&&is_native_na_attribute(b))return true;
    if(dictionary)return false;fail("TypeError","boolean value of NA is ambiguous");
  }
  if(a.container!=b.container)return false;
  if(a.container==AttributeContainerKind::kTuple){
    if(a.children.size()!=b.children.size())return false;
    for(std::size_t i=0;i<a.children.size();++i)if(!equal(a.children[i],b.children[i],dictionary))return false;
    return true;
  }
  if(type(a)=="NaTType"||type(b)=="NaTType")return type(a)==type(b);
  if(numeric(a)&&numeric(b))return root_cause_scalar_keys_equal(a,b);
  if(type(a)!=type(b))return false;
  if(type(a)=="datetime.datetime"){
    const auto left=date_instant(a),right=date_instant(b);
    return left.timezone_aware==right.timezone_aware&&left.seconds==right.seconds&&left.nanoseconds==right.nanoseconds;
  }
  return root_cause_scalar_keys_equal(a,b);
}
bool less(const EventAttribute& a,const EventAttribute& b){
  if(is_native_na_attribute(a)||is_native_na_attribute(b))fail("TypeError","boolean value of NA is ambiguous");
  if(a.container==AttributeContainerKind::kTuple&&b.container==AttributeContainerKind::kTuple){
    for(std::size_t i=0;i<std::min(a.children.size(),b.children.size());++i)
      if(!equal(a.children[i],b.children[i]))return less(a.children[i],b.children[i]);
    return a.children.size()<b.children.size();
  }
  if(type(a)=="NaTType"||type(b)=="NaTType")return false;
  if(numeric(a)&&numeric(b)){
    if((type(a)=="float"&&std::isnan(native_attribute_number(a)))||(type(b)=="float"&&std::isnan(native_attribute_number(b))))return false;
    return native_scalar_key_compare(a,b)<0;
  }
  if(type(a)==type(b)&&type(a)=="str")return a.value<b.value;
  if(type(a)==type(b)&&type(a)=="UUID")return a.value<b.value;
  if(type(a)==type(b)&&type(a)=="bytes")return native_scalar_key_compare(a,b)<0;
  if(type(a)==type(b)&&type(a)=="datetime.datetime"){
    const auto left=date_instant(a),right=date_instant(b);
    if(left.timezone_aware!=right.timezone_aware)fail("TypeError","can't compare offset-naive and offset-aware datetimes");
    return left.seconds!=right.seconds?left.seconds<right.seconds:left.nanoseconds<right.nanoseconds;
  }
  fail("TypeError","'<' not supported between instances of '"+type(a)+"' and '"+type(b)+"'");
}
bool compare(const EventAttribute& a,const EventAttribute& b,std::string_view operation){
  if(is_native_na_attribute(a)||is_native_na_attribute(b))fail("TypeError","boolean value of NA is ambiguous");
  if(!numeric(a)||!numeric(b))fail("TypeError","'"+std::string(operation)+"' not supported between instances of '"+type(a)+"' and '"+type(b)+"'");
  if((type(a)=="float"&&std::isnan(native_attribute_number(a)))||(type(b)=="float"&&std::isnan(native_attribute_number(b))))return false;
  const auto order=native_scalar_key_compare(a,b);
  return operation==">="?order>=0:operation=="<="?order<=0:operation==">"?order>0:order<0;
}
void parameters(const LogVariantsFilterOptions& options){
  if(!options.raw_parameters)return;
  const auto& value=*options.raw_parameters;
  if(value.is_null||(value.container==AttributeContainerKind::kList&&value.children.empty()))return;
  fail("TypeError","'"+type(value)+"' object is not iterable");
}
LogVariantsDictionary grouped(const EventLog& source,const EventAttribute& activity_key){
  LogVariantsDictionary variants;
  for(const auto& trace:source.traces){
    EventAttribute key;key.container=AttributeContainerKind::kTuple;
    for(const auto& event:trace.events){auto value=required(event,activity_key);hash_key(value);value.key=std::to_string(key.children.size());key.children.push_back(std::move(value));}
    auto found=std::find_if(variants.begin(),variants.end(),[&](const auto& old){return equal(old.variant,key,true);});
    if(found==variants.end())variants.push_back({std::move(key),{trace},{}});else found->traces.push_back(trace);
  }
  return variants;
}
std::size_t length(const LogVariantsDictionaryEntry& entry){
  if(!entry.ordinary_value)return entry.traces.size();
  const auto& value=*entry.ordinary_value;
  if(value.container!=AttributeContainerKind::kScalar)return value.children.size();
  if(text(value)){std::size_t result=0;for(const auto byte:value.value)if((static_cast<unsigned char>(byte)&0xc0)!=0x80)++result;return result;}
  if(value.kind==AttributeKind::kBytes)return decode_bytes_attribute(value).size();
  fail("TypeError","object of type '"+type(value)+"' has no len()");
}
struct Ranked{const LogVariantsDictionaryEntry* entry;std::size_t count;};
std::vector<Ranked> ranked(const LogVariantsDictionary& variants){
  std::vector<Ranked> ordered;for(const auto& entry:variants)ordered.push_back({&entry,length(entry)});
  // Stable reverse sorting compares typed variant keys only at frequency ties.
  // Reverse-before/after preserves the public descending comparison direction.
  std::reverse(ordered.begin(),ordered.end());
  for(std::size_t i=1;i<ordered.size();++i){
    const auto current=ordered[i];auto at=i;
    const auto lower=[&](const Ranked& a,const Ranked& b){return a.count!=b.count?a.count<b.count:less(a.entry->variant,b.entry->variant);};
    while(at&&lower(current,ordered[at-1])){ordered[at]=ordered[at-1];--at;}ordered[at]=current;
  }
  std::reverse(ordered.begin(),ordered.end());return ordered;
}
bool contains(const EventAttribute& admitted,const EventAttribute& key){
  if(admitted.is_null)fail("TypeError","argument of type 'NoneType' is not iterable");
  if(admitted.container==AttributeContainerKind::kList||admitted.container==AttributeContainerKind::kTuple)
    return std::any_of(admitted.children.begin(),admitted.children.end(),[&](const auto& old){return equal(old,key);});
  if(admitted.container==AttributeContainerKind::kContainer){
    // Generic dictionary attributes expose string keys; typed dictionary keys
    // are represented through a tuple/set collection in ordinary admission.
    return false;
  }
  if(admitted.container==AttributeContainerKind::kSet||admitted.container==AttributeContainerKind::kFrozenSet)
    return std::any_of(admitted.children.begin(),admitted.children.end(),[&](const auto& old){return equal(old,key,true);});
  if(text(admitted))fail("TypeError","'in <string>' requires string as left operand, not tuple");
  if(admitted.kind==AttributeKind::kBytes)fail("TypeError","a bytes-like object is required, not 'tuple'");
  fail("TypeError","argument of type '"+type(admitted)+"' is not iterable");
}
EventLog selected(const EventLog& source,const LogVariantsDictionary& variants,const EventAttribute& admitted,const EventAttribute& positive){
  EventLog result;copy_event_log_context(source,result);
  for(const auto& entry:variants){const bool keep=truth(positive);if(contains(admitted,entry.variant)==keep)result.traces.insert(result.traces.end(),entry.traces.begin(),entry.traces.end());}
  return result;
}
EventAttribute tuple_collection(const std::vector<const LogVariantsDictionaryEntry*>& entries){
  EventAttribute result;result.container=AttributeContainerKind::kList;
  for(const auto* entry:entries)result.children.push_back(entry->variant);return result;
}
EventAttribute multiplied(const EventAttribute& value,std::size_t count){
  if(is_native_na_attribute(value))return value;
  const auto kind=type(value);
  if(numeric(value)){
    if(kind!="float"){
      // Integral percentages can only straddle a case count at zero and one.
      // Keeping their exact sign/magnitude avoids a lossy large-int product.
      if(count==0)return integer(0);
      const auto zero=integer(0),one=integer(1);
      if(native_scalar_key_compare(value,zero)<0)return real(-std::numeric_limits<double>::infinity());
      if(native_scalar_key_compare(value,one)>0)return real(std::numeric_limits<double>::infinity());
      if(native_scalar_key_compare(value,zero)==0)return zero;
      return integer(count);
    }
    return real(native_attribute_number(value)*static_cast<double>(count));
  }
  if(kind=="str"||kind=="bytes"||kind=="list"||kind=="tuple")return value;
  fail("TypeError","unsupported operand type(s) for *: '"+kind+"' and 'int'");
}
EventLog coverage(const EventLog& source,const EventAttribute& limit,const LogVariantsFilterOptions& options,bool maximum){
  parameters(options);const auto variants=grouped(source,options.activity_key);std::vector<const LogVariantsDictionaryEntry*> admitted;
  for(const auto& entry:variants)if(compare(integer(entry.traces.size()),multiplied(limit,source.traces.size()),maximum?"<=":">="))admitted.push_back(&entry);
  return selected(source,variants,tuple_collection(admitted),options.positive);
}
LogVariantsDictionary ordinary_dictionary(const EventAttribute& value){
  if(value.container==AttributeContainerKind::kList&&value.children.empty())return {};
  fail("TypeError","'"+type(value)+"' object is not iterable");
}
} // namespace
EventLog filter_log_variants(const EventLog& source,const EventAttribute& admitted,const LogVariantsFilterOptions& options){
  parameters(options);return selected(source,grouped(source,options.activity_key),admitted,options.positive);
}
EventLog filter_log_variants(const EventLog& source,const LogVariantsAdmissionDictionary& dictionary,const LogVariantsFilterOptions& options){
  EventAttribute admitted;admitted.container=AttributeContainerKind::kSet;
  for(const auto& key:dictionary.keys){hash_key(key);admitted.children.push_back(key);}
  return filter_log_variants(source,admitted,options);
}
EventLog filter_log_variants_cumulative(const EventLog& source,const EventAttribute& percentage,const LogVariantsFilterOptions& options){
  parameters(options);return filter_log_variants_dictionary_percentage(source,grouped(source,options.activity_key),percentage);
}
EventLog filter_log_variants_min_coverage(const EventLog& source,const EventAttribute& minimum,const LogVariantsFilterOptions& options){return coverage(source,minimum,options,false);}
EventLog filter_log_variants_max_coverage(const EventLog& source,const EventAttribute& maximum,const LogVariantsFilterOptions& options){return coverage(source,maximum,options,true);}
EventLog filter_log_variants_top_k(const EventLog& source,const EventAttribute& k,const LogVariantsFilterOptions& options){
  parameters(options);const auto variants=grouped(source,options.activity_key);const auto ordered=ranked(variants);
  const auto size=integer(ordered.size());std::size_t count=ordered.size();
  if(!compare(size,k,"<")){
    if(type(k)!="int"&&type(k)!="bool")fail("TypeError","slice indices must be integers or None or have an __index__ method");
    EventAttribute negative_size{"","-"+std::to_string(ordered.size()),AttributeKind::kInt};
    if(native_scalar_key_compare(k,integer(0))<0&&native_scalar_key_compare(k,negative_size)<=0)count=0;
    else {const auto n=native_attribute_number(k);count=n<0?ordered.size()-static_cast<std::size_t>(-n):static_cast<std::size_t>(n);}
  }
  std::vector<const LogVariantsDictionaryEntry*> admitted;for(std::size_t i=0;i<count;++i)admitted.push_back(ordered[i].entry);
  return selected(source,variants,tuple_collection(admitted),options.positive);
}
EventLog filter_log_variants_dictionary_percentage(const EventLog& source,const LogVariantsDictionary& variants,const EventAttribute& percentage){
  EventLog result;copy_event_log_context(source,result);const auto ordered=ranked(variants);
  bool stop=false;std::size_t previous=0,added=0;
  for(const auto& entry:ordered){
    if(stop&&entry.count<previous)break;
    if(entry.entry->ordinary_value)throw std::invalid_argument("variants percentage output requires native trace values");
    result.traces.insert(result.traces.end(),entry.entry->traces.begin(),entry.entry->traces.end());added+=entry.count;
    if(source.traces.empty())fail("ZeroDivisionError","division by zero");
    if(compare(real(static_cast<double>(added)/source.traces.size()),percentage,">="))stop=true;
    previous=entry.count;
  }
  return result;
}
EventLog filter_log_variants_dictionary_percentage(const EventLog& source,const EventAttribute& variants,const EventAttribute& percentage){return filter_log_variants_dictionary_percentage(source,ordinary_dictionary(variants),percentage);}
double find_log_variants_auto_threshold(const EventLog& source,const LogVariantsDictionary& variants,const EventAttribute& factor){
  const auto ordered=ranked(variants);std::size_t added=0,previous=0;
  for(const auto& entry:ordered){
    if(added&& !compare(integer(entry.count),multiplied(factor,previous),">"))break;
    added+=entry.count;previous=entry.count;
  }
  if(source.traces.empty())fail("ZeroDivisionError","division by zero");
  return static_cast<double>(added)/source.traces.size();
}
double find_log_variants_auto_threshold(const EventLog& source,const EventAttribute& variants,const EventAttribute& factor){return find_log_variants_auto_threshold(source,ordinary_dictionary(variants),factor);}
} // namespace minera
