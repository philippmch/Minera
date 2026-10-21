#include "minera/log_timestamp_filter.hpp"
#include "minera/timestamp.hpp"
#include "root_cause_trace_key.hpp"
#include "root_cause_unicode.hpp"

#include <algorithm>
#include <cmath>
#include <regex>
#include <limits>
#include <map>
#include <optional>

namespace minera {
namespace {
[[noreturn]] void fail(std::string type,std::string message) {throw LogTimestampFilterError(std::move(type),std::move(message));}
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
    if(first<0x80){}else if(first>=0xc2&&first<=0xdf){code=first&31;n=1;}else if(first>=0xe0&&first<=0xef){code=first&15;n=2;}else if(first>=0xf0&&first<=0xf4){code=first&7;n=3;}else fail("ValueError","timestamp filter selector has invalid UTF-8");
    for(unsigned i=0;i<n;++i){if(at==value.size())fail("ValueError","timestamp filter selector has short UTF-8");const auto next=static_cast<unsigned char>(value[at++]);if((next&0xc0)!=0x80)fail("ValueError","timestamp filter selector has invalid UTF-8 continuation");code=(code<<6)|(next&63);}
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
const EventAttribute& selected_key(const LogTimestampFilterOptions& options) {return options.enum_timestamp_key?*options.enum_timestamp_key:options.timestamp_key;}
bool truth(const EventAttribute& value) {
  if(type(value)=="NoneType")return false;
  if(is_native_na_attribute(value))fail("TypeError","boolean value of NA is ambiguous");
  if(value.container!=AttributeContainerKind::kScalar)return !value.children.empty();
  if(type(value)=="int"||type(value)=="bool"||type(value)=="float")return native_attribute_number(value)!=0;
  if(text(value)||value.kind==AttributeKind::kBytes)return !value.value.empty();return true;
}
bool numeric(const EventAttribute& value) {return type(value)=="int"||type(value)=="bool"||type(value)=="float";}
bool equals(const EventAttribute& a,const EventAttribute& b,bool nested=false) {
  if(type(a)=="NoneType"||type(b)=="NoneType")return type(a)==type(b);
  if(is_native_na_attribute(a)||is_native_na_attribute(b)) {
    if(nested&&is_native_na_attribute(a)&&is_native_na_attribute(b))return true;
    fail("TypeError","boolean value of NA is ambiguous");
  }
  const auto is_set=[](const EventAttribute& value){return value.container==AttributeContainerKind::kSet||value.container==AttributeContainerKind::kFrozenSet;};
  if(is_set(a)||is_set(b)) {
    if(!is_set(a)||!is_set(b))return false;
    const auto left=root_cause_set_members(a),right=root_cause_set_members(b);
    if(left.size()!=right.size())return false;
    for(const auto* member:left)if(std::none_of(right.begin(),right.end(),[&](const auto* other){return equals(*member,*other,true);}))return false;
    return true;
  }
  if(a.container!=b.container)return false;
  if(a.container==AttributeContainerKind::kList||a.container==AttributeContainerKind::kTuple) {
    if(a.children.size()!=b.children.size())return false;
    for(std::size_t i=0;i<a.children.size();++i)if(!equals(a.children[i],b.children[i],true))return false;
    return true;
  }
  if(a.container==AttributeContainerKind::kContainer) {
    std::map<std::string,const EventAttribute*> left,right;
    for(const auto& child:a.children)left[child.key]=&child;
    for(const auto& child:b.children)right[child.key]=&child;
    if(left.size()!=right.size())return false;
    for(const auto& [key,value]:left) {const auto found=right.find(key);if(found==right.end()||!equals(*value,*found->second,true))return false;}
    return true;
  }
  if(numeric(a)&&numeric(b)) {
    if(!nested&&((type(a)=="float"&&std::isnan(native_attribute_number(a)))||(type(b)=="float"&&std::isnan(native_attribute_number(b)))))return false;
    return native_scalar_key_compare(a,b)==0;
  }
  if(type(a)!=type(b))return false;
  if(type(a)=="NaTType")return false;
  return root_cause_scalar_keys_equal(a,b);
}
std::string lexical(const EventAttribute& value) {if(const auto* date=std::get_if<NativeDate>(&value.typed_value))return date->value;return value.value;}
EventAttribute normalized(const EventAttribute& value) {
  const auto kind=type(value);
  if(kind=="NaTType")return value;
  if(kind!="datetime.datetime") {
    if(kind=="str")fail("TypeError","str.replace() takes no keyword arguments");
    fail("AttributeError","'"+kind+"' object has no attribute 'replace'");
  }
  auto raw=lexical(value);if(raw.size()<19)raw=raw.substr(0,10)+"T00:00:00";
  std::size_t end=19;if(end<raw.size()&&raw[end]=='.'){++end;while(end<raw.size()&&raw[end]>='0'&&raw[end]<='9')++end;}
  raw=raw.substr(0,end)+'Z';EventAttribute result{"",raw,AttributeKind::kDate};result.typed_value=NativeDate{raw};return result;
}
EventAttribute bound(const EventAttribute& value) {
  if(!text(value))return normalized(value);
  // Public parsing accepts the datetime.strptime format, not ISO guessing.
  static const std::regex shape(R"(^([0-9]{4})-([0-9]{1,2})-([0-9]{1,2})\s+([0-9]{1,2}):([0-9]{1,2}):([0-9]{1,2}))");
  std::smatch fields;
  const auto mismatch=[&](){fail("ValueError","time data "+quoted(value.value)+" does not match format '%Y-%m-%d %H:%M:%S'");};
  if(!std::regex_search(value.value,fields,shape))mismatch();
  const int year=std::stoi(fields[1]),month=std::stoi(fields[2]),day=std::stoi(fields[3]),hour=std::stoi(fields[4]),minute=std::stoi(fields[5]),second=std::stoi(fields[6]);
  if(month<1||month>12||day<1||day>31||hour>23||minute>59||second>61)mismatch();
  if(static_cast<std::size_t>(fields.length())<value.value.size())fail("ValueError","unconverted data remains: "+value.value.substr(fields.length()));
  if(year==0)fail("ValueError","year 0 is out of range");
  if(second>59)fail("ValueError","second must be in 0..59");
  const auto two=[](int v){auto result=std::to_string(v);return result.size()==1?'0'+result:result;};
  auto raw=fields[1].str()+'-'+two(month)+'-'+two(day)+'T'+two(hour)+':'+two(minute)+':'+two(second)+'Z';
  if(!parse_timestamp_exact(raw))fail("ValueError","day is out of range for month");
  EventAttribute result{"",raw,AttributeKind::kDate};result.typed_value=NativeDate{raw};return result;
}
bool compare(const EventAttribute& a,const EventAttribute& b,std::string_view operation) {
  if(type(a)=="NaTType"||type(b)=="NaTType")return false;
  if(type(a)!="datetime.datetime"||type(b)!="datetime.datetime")fail("TypeError","'"+std::string(operation)+"' not supported between instances of '"+type(a)+"' and '"+type(b)+"'");
  const auto left=parse_timestamp_exact(lexical(a)),right=parse_timestamp_exact(lexical(b));
  if(!left||!right)throw std::invalid_argument("timestamp filter contains an invalid native datetime");
  if(left->timezone_aware!=right->timezone_aware)fail("TypeError","can't compare offset-naive and offset-aware datetimes");
  const int order=left->seconds<right->seconds?-1:left->seconds>right->seconds?1:left->nanoseconds<right->nanoseconds?-1:left->nanoseconds>right->nanoseconds?1:0;
  return operation==">="?order>=0:order<=0;
}
bool inside(const EventAttribute& timestamp,const EventAttribute& lower,const EventAttribute& upper) {
  const auto value=normalized(timestamp);return compare(lower,value,"<=")&&compare(value,upper,"<=");
}
std::optional<EventAttribute> trace_value(const Trace& trace,const EventAttribute& key) {
  hash_key(key);if(!text(key))return std::nullopt;
  for(auto at=trace.attributes.rbegin();at!=trace.attributes.rend();++at)if(at->key==key.value)return present(*at);
  const auto primary=!trace.case_id_source_key.empty()?trace.case_id_source_key:trace.case_id_attribute?trace.case_id_attribute->key:"concept:name";
  if(primary==key.value&&(trace.case_id_attribute||!trace.case_id.empty()||trace.case_id_state!=FirstClassAttributeState::kMissing))return trace.case_id_attribute?present(*trace.case_id_attribute):std::make_optional(EventAttribute{key.value,trace.case_id});
  return std::nullopt;
}
void upsert(std::vector<EventAttribute>& attributes,EventAttribute value) {
  for(auto& old:attributes)if(old.key==value.key){old=std::move(value);return;}attributes.push_back(std::move(value));
}
void inject_trace_attributes(EventLog& source) {
  for(auto& trace:source.traces) {
    auto attributes=trace.attributes;
    const auto primary=!trace.case_id_source_key.empty()?trace.case_id_source_key:trace.case_id_attribute?trace.case_id_attribute->key:"concept:name";
    if(std::none_of(attributes.begin(),attributes.end(),[&](const auto& value){return value.key==primary;})&&(trace.case_id_attribute||!trace.case_id.empty()||trace.case_id_state!=FirstClassAttributeState::kMissing)) {
      if(auto value=trace_value(trace,EventAttribute{"",primary})){value->key=primary;attributes.push_back(*value);}
    }
    for(auto& event:trace.events)for(auto value:attributes) {
      if(!present(value))continue;
      value.key="case:"+value.key;value.first_class_role=FirstClassAttributeRole::kNone;value.first_class_state=FirstClassAttributeState::kMissing;value.first_class_source_key.clear();upsert(event.attributes,std::move(value));
    }
  }
}
bool dictionary_equal(const EventAttribute& a,const EventAttribute& b) {
  if(is_native_na_attribute(a)||is_native_na_attribute(b))return is_native_na_attribute(a)&&is_native_na_attribute(b);
  return equals(a,b);
}
void set_case_projection(Trace& trace) {
  for(const auto& value:trace.attributes)if(value.key=="concept:name") {
    trace.case_id=value.value;trace.case_id_attribute=value;trace.case_id_state=value.is_null?FirstClassAttributeState::kNull:FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";return;
  }
}
} // namespace

EventLog apply_log_timestamp_filter(const EventLog&,const EventAttribute&) {fail("Exception","apply method not available for timestamp filter");}
EventLog apply_log_timestamp_auto_filter(const EventLog&,const EventAttribute&) {fail("Exception","apply_auto_filter method not available for timestamp filter");}
bool log_timestamp_is_contained(const Trace& trace,const EventAttribute& dt1,const EventAttribute& dt2,const EventAttribute& key) {
  return !trace.events.empty()&&compare(normalized(required(trace.events.front(),key)),dt1,">=")&&compare(normalized(required(trace.events.back(),key)),dt2,"<=");
}
bool log_timestamp_is_intersecting(const Trace& trace,const EventAttribute& dt1,const EventAttribute& dt2,const EventAttribute& key) {
  if(trace.events.empty())return false;
  // The public intersecting predicate reads both endpoints before testing
  // overlap. Unlike containment, an inside start cannot hide a missing end.
  const auto first=normalized(required(trace.events.front(),key));
  const auto last=normalized(required(trace.events.back(),key));
  return (compare(dt1,first,"<=")&&compare(first,dt2,"<=")) ||
         (compare(dt1,last,"<=")&&compare(last,dt2,"<=")) ||
         (compare(first,dt1,"<=")&&compare(dt1,last,"<=")) ||
         (compare(first,dt2,"<=")&&compare(dt2,last,"<="));
}
bool log_timestamp_trace_attribute_is_contained(const Trace& trace,const EventAttribute& dt1,const EventAttribute& dt2,const EventAttribute& key) {
  const auto value=trace_value(trace,key);return value&&inside(*value,dt1,dt2);
}
bool log_timestamp_has_attribute_in_timeframe(const Trace& trace,const EventAttribute& attribute,const EventAttribute& value,
    const EventAttribute& dt1,const EventAttribute& dt2,const EventAttribute& key) {
  for(const auto& event:trace.events)if(const auto found=event_value(event,attribute);found&&equals(*found,value)&&inside(required(event,key),dt1,dt2))return true;
  return false;
}
EventLog filter_log_timestamp_events(EventLog& source,const EventAttribute& dt1,const EventAttribute& dt2,const LogTimestampFilterOptions& options) {
  const auto lower=bound(dt1),upper=bound(dt2);inject_trace_attributes(source);
  std::vector<Event> kept;
  for(const auto& trace:source.traces)for(const auto& event:trace.events)if(inside(required(event,selected_key(options)),lower,upper))kept.push_back(event);
  EventLog result;copy_event_log_context(source,result);std::vector<EventAttribute> case_keys;
  for(auto event:kept) {
    const auto key=required(event,EventAttribute{"","case:concept:name"});hash_key(key);
    auto found=std::find_if(case_keys.begin(),case_keys.end(),[&](const auto& old){return dictionary_equal(old,key);});
    std::size_t index=static_cast<std::size_t>(found-case_keys.begin());
    if(found==case_keys.end()) {
      Trace trace;for(const auto& field:event.attributes)if(field.key.starts_with("case:")){auto value=field;value.key.erase(0,5);trace.attributes.push_back(std::move(value));}
      set_case_projection(trace);result.traces.push_back(std::move(trace));case_keys.push_back(key);
    }
    event.attributes.erase(std::remove_if(event.attributes.begin(),event.attributes.end(),[](const auto& value){return value.key.starts_with("case:");}),event.attributes.end());
    result.traces[index].events.push_back(std::move(event));
  }
  return result;
}
EventLog filter_log_timestamp_events(const EventLog& source,const EventAttribute& dt1,const EventAttribute& dt2,const LogTimestampFilterOptions& options) {
  auto copied=source;return filter_log_timestamp_events(copied,dt1,dt2,options);
}
EventLog filter_log_timestamp_contained(const EventLog& source,const EventAttribute& dt1,const EventAttribute& dt2,const LogTimestampFilterOptions& options) {
  const auto lower=bound(dt1),upper=bound(dt2);EventLog result;copy_event_log_context(source,result);
  for(const auto& trace:source.traces)if(log_timestamp_is_contained(trace,lower,upper,selected_key(options)))result.traces.push_back(trace);return result;
}
EventLog filter_log_timestamp_intersecting(const EventLog& source,const EventAttribute& dt1,const EventAttribute& dt2,const LogTimestampFilterOptions& options) {
  const auto lower=bound(dt1),upper=bound(dt2);EventLog result;copy_event_log_context(source,result);
  for(const auto& trace:source.traces)if(log_timestamp_is_intersecting(trace,lower,upper,options.timestamp_key))result.traces.push_back(trace);return result;
}
EventLog filter_log_timestamp_starting(const EventLog& source,const EventAttribute& dt1,const EventAttribute& dt2,const LogTimestampFilterOptions& options) {
  const auto lower=bound(dt1),upper=bound(dt2);EventLog result;copy_event_log_context(source,result);
  for(const auto& trace:source.traces){const bool matches=!trace.events.empty()&&inside(required(trace.events.front(),selected_key(options)),lower,upper);if(matches==truth(options.positive))result.traces.push_back(trace);}return result;
}
EventLog filter_log_timestamp_completing(const EventLog& source,const EventAttribute& dt1,const EventAttribute& dt2,const LogTimestampFilterOptions& options) {
  const auto lower=bound(dt1),upper=bound(dt2);EventLog result;copy_event_log_context(source,result);
  for(const auto& trace:source.traces){const bool matches=!trace.events.empty()&&inside(required(trace.events.back(),selected_key(options)),lower,upper);if(matches==truth(options.positive))result.traces.push_back(trace);}return result;
}
EventLog filter_log_timestamp_trace_attribute(const EventLog& source,const EventAttribute& dt1,const EventAttribute& dt2,const LogTimestampFilterOptions& options) {
  const auto lower=bound(dt1),upper=bound(dt2);EventLog result;copy_event_log_context(source,result);
  for(const auto& trace:source.traces)if(log_timestamp_trace_attribute_is_contained(trace,lower,upper,selected_key(options)))result.traces.push_back(trace);return result;
}
EventLog filter_log_timestamp_attribute(const EventLog& source,const EventAttribute& attribute,const EventAttribute& value,
    const EventAttribute& dt1,const EventAttribute& dt2,const LogTimestampFilterOptions& options) {
  const auto lower=bound(dt1),upper=bound(dt2);EventLog result;copy_event_log_context(source,result);result.metadata.properties.clear();
  for(const auto& trace:source.traces)if(log_timestamp_has_attribute_in_timeframe(trace,attribute,value,lower,upper,selected_key(options)))result.traces.push_back(trace);return result;
}
} // namespace minera
