#include "minera/log_ltl_filter.hpp"
#include "minera/timestamp.hpp"
#include "minera/token_integer.hpp"
#include "root_cause_trace_key.hpp"
#include "root_cause_unicode.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <ctime>
#include <map>
#include <optional>

namespace minera {
namespace {
[[noreturn]] void fail(std::string type,std::string message) {throw LogLtlFilterError(std::move(type),std::move(message));}
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
    if(first<0x80){}else if(first>=0xc2&&first<=0xdf){code=first&31;n=1;}else if(first>=0xe0&&first<=0xef){code=first&15;n=2;}else if(first>=0xf0&&first<=0xf4){code=first&7;n=3;}else fail("ValueError","LTL filter selector has invalid UTF-8");
    for(unsigned i=0;i<n;++i){if(at==value.size())fail("ValueError","LTL filter selector has short UTF-8");const auto next=static_cast<unsigned char>(value[at++]);if((next&0xc0)!=0x80)fail("ValueError","LTL filter selector has invalid UTF-8 continuation");code=(code<<6)|(next&63);}
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
bool numeric(const EventAttribute& v){const auto t=type(v);return t=="bool"||t=="int"||t=="float";}
EventAttribute number(std::size_t n){EventAttribute v{"",std::to_string(n),AttributeKind::kInt};v.typed_value=static_cast<std::uint64_t>(n);return v;}
EventAttribute real(double n){EventAttribute v{"","",AttributeKind::kFloat};v.typed_value=n;return v;}
bool truth(const EventAttribute& v){if(type(v)=="NoneType")return false;if(is_native_na_attribute(v))fail("TypeError","boolean value of NA is ambiguous");if(v.container!=AttributeContainerKind::kScalar)return !v.children.empty();if(numeric(v))return native_attribute_number(v)!=0;if(text(v)||v.kind==AttributeKind::kBytes)return !v.value.empty();return true;}
ParsedExactTimestamp instant(const EventAttribute& v){const auto* n=std::get_if<NativeDate>(&v.typed_value);auto result=parse_timestamp_exact(n?n->value:v.value);if(!result)throw std::invalid_argument("LTL filter contains invalid native datetime");return *result;}
bool equal(const EventAttribute& a,const EventAttribute& b,bool identity=true){
 if(type(a)=="NoneType"||type(b)=="NoneType")return type(a)==type(b);
 if(is_native_na_attribute(a)||is_native_na_attribute(b)){if(identity&&is_native_na_attribute(a)&&is_native_na_attribute(b))return true;fail("TypeError","boolean value of NA is ambiguous");}
 const auto set=[](const EventAttribute& v){return v.container==AttributeContainerKind::kSet||v.container==AttributeContainerKind::kFrozenSet;};
 if(set(a)||set(b)){if(!set(a)||!set(b))return false;const auto x=root_cause_set_members(a),y=root_cause_set_members(b);if(x.size()!=y.size())return false;for(const auto* v:x)if(std::none_of(y.begin(),y.end(),[&](const auto* w){return equal(*v,*w);}))return false;return true;}
 if(a.container!=b.container)return false;
 if(a.container==AttributeContainerKind::kList||a.container==AttributeContainerKind::kTuple){if(a.children.size()!=b.children.size())return false;for(std::size_t i=0;i<a.children.size();++i)if(!equal(a.children[i],b.children[i]))return false;return true;}
 if(a.container==AttributeContainerKind::kContainer){std::map<std::string,const EventAttribute*>x,y;for(const auto&v:a.children)x[v.key]=&v;for(const auto&v:b.children)y[v.key]=&v;if(x.size()!=y.size())return false;for(const auto&[k,v]:x){const auto w=y.find(k);if(w==y.end()||!equal(*v,*w->second))return false;}return true;}
 if(type(a)=="NaTType"||type(b)=="NaTType")return identity&&type(a)==type(b);
 if(numeric(a)&&numeric(b)){if(!identity&&((type(a)=="float"&&std::isnan(native_attribute_number(a)))||(type(b)=="float"&&std::isnan(native_attribute_number(b)))))return false;return root_cause_scalar_keys_equal(a,b);}
 if(type(a)!=type(b))return false;
 if(type(a)=="datetime.datetime"){const auto x=instant(a),y=instant(b);return x.timezone_aware==y.timezone_aware&&x.seconds==y.seconds&&x.nanoseconds==y.nanoseconds;}
 return root_cause_scalar_keys_equal(a,b);
}
bool compare(const EventAttribute&a,const EventAttribute&b,std::string_view op){
 if(is_native_na_attribute(a)||is_native_na_attribute(b))fail("TypeError","boolean value of NA is ambiguous");
 if(type(a)=="NaTType"||type(b)=="NaTType")return false;
 int order=0;
 if(numeric(a)&&numeric(b)){if((type(a)=="float"&&std::isnan(native_attribute_number(a)))||(type(b)=="float"&&std::isnan(native_attribute_number(b))))return false;order=native_scalar_key_compare(a,b);}
 else if(type(a)=="datetime.datetime"&&type(b)=="datetime.datetime"){const auto x=instant(a),y=instant(b);if(x.timezone_aware!=y.timezone_aware)fail("TypeError","can't compare offset-naive and offset-aware datetimes");order=x.seconds!=y.seconds?(x.seconds<y.seconds?-1:1):x.nanoseconds<y.nanoseconds?-1:x.nanoseconds>y.nanoseconds?1:0;}
 else if(type(a)=="str"&&type(b)=="str")order=a.value<b.value?-1:a.value>b.value?1:0;
 else if(type(a)=="bytes"&&type(b)=="bytes")order=native_scalar_key_compare(a,b);
 else if(a.container==b.container&&(a.container==AttributeContainerKind::kList||a.container==AttributeContainerKind::kTuple)){for(std::size_t i=0;i<std::min(a.children.size(),b.children.size());++i)if(!equal(a.children[i],b.children[i]))return compare(a.children[i],b.children[i],op);order=a.children.size()<b.children.size()?-1:a.children.size()>b.children.size()?1:0;}
 else fail("TypeError","'"+std::string(op)+"' not supported between instances of '"+type(a)+"' and '"+type(b)+"'");
 return op=="<="?order<=0:op==">="?order>=0:op=="<"?order<0:order>0;
}

std::vector<EventAttribute> iterable(const EventAttribute& value) {
  if(value.container==AttributeContainerKind::kList || value.container==AttributeContainerKind::kTuple ||
     value.container==AttributeContainerKind::kSet || value.container==AttributeContainerKind::kFrozenSet)
    return value.children;
  if(value.container==AttributeContainerKind::kContainer) {
    std::vector<EventAttribute> result;
    for(const auto& child:value.children)result.push_back(EventAttribute{"",child.key});
    return result;
  }
  std::vector<EventAttribute> result;
  if(text(value)) {
    for(std::size_t at=0;at<value.value.size();) {
      const auto begin=at;const auto first=static_cast<unsigned char>(value.value[at++]);
      at+=first<0x80?0:first<0xe0?1:first<0xf0?2:3;
      result.push_back(EventAttribute{"",value.value.substr(begin,at-begin)});
    }
    return result;
  }
  if(value.kind==AttributeKind::kBytes) {
    for(const unsigned char byte:decode_bytes_attribute(value))result.push_back(number(byte));
    return result;
  }
  fail("TypeError","'"+type(value)+"' object is not iterable");
}
std::size_t length(const EventAttribute& value) {
  if(value.container!=AttributeContainerKind::kScalar || text(value) || value.kind==AttributeKind::kBytes)
    return iterable(value).size();
  fail("TypeError","object of type '"+type(value)+"' has no len()");
}
EventAttribute indexed(const EventAttribute& value,std::size_t index) {
  if(value.container==AttributeContainerKind::kContainer) {
    const auto name=std::to_string(index);
    // An ordinary native map has string keys, so integer indexing cannot find it.
    fail("KeyError",name);
  }
  if(value.container==AttributeContainerKind::kList || value.container==AttributeContainerKind::kTuple || text(value) || value.kind==AttributeKind::kBytes) {
    const auto values=iterable(value);
    if(index<values.size())return values[index];
    fail("IndexError",(text(value)?"string":value.kind==AttributeKind::kBytes?"index":value.container==AttributeContainerKind::kTuple?"tuple":"list")+std::string(value.kind==AttributeKind::kBytes?" out of range":" index out of range"));
  }
  fail("TypeError","'"+type(value)+"' object is not subscriptable");
}
bool timestamp_compare(const EventAttribute& a,const EventAttribute& b,bool lower) {
  const auto count=length(a);
  for(std::size_t i=0;i<count;++i) {
    const auto left=indexed(a,i);
    const auto pair=indexed(b,i);
    const auto bound=indexed(pair,lower?0:1);
    if(compare(left,bound,lower?"<":">"))return false;
  }
  return true;
}
void check_parameters(const LogLtlFilterOptions& options) {
  if(!options.raw_parameters || options.raw_parameters->is_null)return;
  (void)iterable(*options.raw_parameters);
}
bool set_equal(const EventAttribute& a,const EventAttribute& b) {
  if(is_native_na_attribute(a)||is_native_na_attribute(b))return is_native_na_attribute(a)&&is_native_na_attribute(b);
  if(type(a)=="NaTType"||type(b)=="NaTType")return type(a)==type(b);
  // Distinct float NaN values do not coalesce in the captured resource sets.
  if((type(a)=="float"&&std::isnan(native_attribute_number(a))) ||
     (type(b)=="float"&&std::isnan(native_attribute_number(b))))return false;
  return equal(a,b);
}
std::vector<EventAttribute> resources(const Trace& trace,const EventAttribute& target,const LogLtlFilterOptions& options) {
  std::vector<EventAttribute> result;
  for(const auto& event:trace.events) {
    const auto activity=event_value(event,options.attribute_key);
    const auto resource=event_value(event,options.resource_key);
    if(!activity || !resource || !equal(*activity,target,false))continue;
    hash_key(*resource);
    if(std::none_of(result.begin(),result.end(),[&](const auto& old){return set_equal(old,*resource);}))result.push_back(*resource);
  }
  return result;
}
double epoch_timestamp(const EventAttribute& value) {
  if(type(value)=="NaTType")fail("ValueError","NaTType does not support timestamp");
  if(type(value)!="datetime.datetime")fail("AttributeError","'"+type(value)+"' object has no attribute 'timestamp'");
  const auto parsed=instant(value);
  if(parsed.timezone_aware)return static_cast<double>(parsed.seconds)+static_cast<double>(parsed.nanoseconds)/1e9;
  const auto* date=std::get_if<NativeDate>(&value.typed_value);
  const auto& source=date?date->value:value.value;
  std::tm local{};local.tm_year=std::stoi(source.substr(0,4))-1900;local.tm_mon=std::stoi(source.substr(5,2))-1;
  local.tm_mday=std::stoi(source.substr(8,2));local.tm_hour=source.size()>12?std::stoi(source.substr(11,2)):0;
  local.tm_min=source.size()>15?std::stoi(source.substr(14,2)):0;local.tm_sec=source.size()>18?std::stoi(source.substr(17,2)):0;local.tm_isdst=-1;
  return static_cast<double>(std::mktime(&local))+static_cast<double>(parsed.nanoseconds)/1e9;
}
EventAttribute boolean(bool value) {
  EventAttribute result{"",value?"true":"false",AttributeKind::kBoolean};result.typed_value=value;return result;
}
} // namespace

EventAttribute log_ltl_timestamp_list_is_ge(const EventAttribute& a,const EventAttribute& b) {
  return boolean(timestamp_compare(a,b,true));
}
EventAttribute log_ltl_timestamp_list_is_le(const EventAttribute& a,const EventAttribute& b) {
  return boolean(timestamp_compare(a,b,false));
}

EventLog filter_log_ltl_eventually_follows(EventLog& source,const EventAttribute& values,const LogLtlFilterOptions& options) {
  check_parameters(options);
  const auto pattern=iterable(values);
  EventLog result;copy_event_log_context(source,result);
  for(const auto& trace:source.traces) {
    std::vector<std::size_t> positions;std::size_t begin=0;
    for(const auto& target:pattern) {
      bool found=false;
      for(std::size_t i=begin;i<trace.events.size();++i) {
        const auto value=event_value(trace.events[i],options.attribute_key);
        if(value && equal(*value,target,false)){positions.push_back(i);begin=i+1;found=true;break;}
      }
      if(!found)break;
    }
    bool matched=positions.size()==pattern.size();
    if(matched && truth(options.enable_timestamp) && options.timestamp_diff_boundaries && truth(*options.timestamp_diff_boundaries) && positions.size()>1) {
      EventAttribute differences;differences.container=AttributeContainerKind::kList;
      for(std::size_t i=1;i<positions.size();++i) {
        const auto left=epoch_timestamp(required(trace.events[i-1],options.timestamp_key));
        const auto right=epoch_timestamp(required(trace.events[i],options.timestamp_key));
        differences.children.push_back(real(right-left));
      }
      matched=timestamp_compare(differences,*options.timestamp_diff_boundaries,true)&&timestamp_compare(differences,*options.timestamp_diff_boundaries,false);
    }
    if(matched==truth(options.positive))result.traces.push_back(trace);
  }
  return result;
}
EventLog filter_log_ltl_next(EventLog& source,const EventAttribute& a,const EventAttribute& b,const EventAttribute& c,const LogLtlFilterOptions& options) {
  check_parameters(options);EventLog result;copy_event_log_context(source,result);
  for(const auto& trace:source.traces) {
    std::vector<std::size_t> first,second,third;
    for(std::size_t i=0;i<trace.events.size();++i)if(auto value=event_value(trace.events[i],options.attribute_key)) {
      if(equal(*value,a,false))first.push_back(i);
      if(equal(*value,b,false))second.push_back(i);
      if(equal(*value,c,false))third.push_back(i);
    }
    bool matched=false;
    for(const auto i:first)if(std::find(second.begin(),second.end(),i+1)!=second.end()&&std::find(third.begin(),third.end(),i+2)!=third.end()){matched=true;break;}
    if(matched==truth(options.positive))result.traces.push_back(trace);
  }
  return result;
}
EventLog filter_log_ltl_four_eyes(EventLog& source,const EventAttribute& a,const EventAttribute& b,const LogLtlFilterOptions& options) {
  check_parameters(options);EventLog result;copy_event_log_context(source,result);
  for(const auto& trace:source.traces) {
    const auto first=resources(trace,a,options),second=resources(trace,b,options);
    if(first.empty()||second.empty())continue;
    bool disjoint=true;
    for(const auto& x:first)if(std::any_of(second.begin(),second.end(),[&](const auto& y){return set_equal(x,y);})){disjoint=false;break;}
    if(disjoint==truth(options.positive))result.traces.push_back(trace);
  }
  return result;
}
EventLog filter_log_ltl_different_persons(EventLog& source,const EventAttribute& a,const LogLtlFilterOptions& options) {
  check_parameters(options);EventLog result;copy_event_log_context(source,result);
  for(const auto& trace:source.traces)if((resources(trace,a,options).size()>1)==truth(options.positive))result.traces.push_back(trace);
  return result;
}
EventLog filter_log_ltl_eventually_follows(const EventLog& source,const EventAttribute& values,const LogLtlFilterOptions& options){auto copy=source;return filter_log_ltl_eventually_follows(copy,values,options);}
EventLog filter_log_ltl_next(const EventLog& source,const EventAttribute& a,const EventAttribute& b,const EventAttribute& c,const LogLtlFilterOptions& options){auto copy=source;return filter_log_ltl_next(copy,a,b,c,options);}
EventLog filter_log_ltl_four_eyes(const EventLog& source,const EventAttribute& a,const EventAttribute& b,const LogLtlFilterOptions& options){auto copy=source;return filter_log_ltl_four_eyes(copy,a,b,options);}
EventLog filter_log_ltl_different_persons(const EventLog& source,const EventAttribute& a,const LogLtlFilterOptions& options){auto copy=source;return filter_log_ltl_different_persons(copy,a,options);}
} // namespace minera
