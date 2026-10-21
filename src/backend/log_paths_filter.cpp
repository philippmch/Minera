#include "minera/log_paths_filter.hpp"
#include "minera/token_integer.hpp"
#include "minera/timestamp.hpp"
#include "root_cause_trace_key.hpp"
#include "root_cause_unicode.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <limits>
#include <optional>

namespace minera {
namespace {
[[noreturn]] void fail(std::string type,std::string message) {throw LogPathsFilterError(std::move(type),std::move(message));}
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
    if(first<0x80){}else if(first>=0xc2&&first<=0xdf){code=first&31;n=1;}else if(first>=0xe0&&first<=0xef){code=first&15;n=2;}else if(first>=0xf0&&first<=0xf4){code=first&7;n=3;}else fail("ValueError","paths filter selector has invalid UTF-8");
    for(unsigned i=0;i<n;++i){if(at==value.size())fail("ValueError","paths filter selector has short UTF-8");const auto next=static_cast<unsigned char>(value[at++]);if((next&0xc0)!=0x80)fail("ValueError","paths filter selector has invalid UTF-8 continuation");code=(code<<6)|(next&63);}
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
bool numeric(const EventAttribute& value) {const auto t=type(value);return t=="int"||t=="bool"||t=="float";}
bool is_nan(const EventAttribute& value) {return type(value)=="float"&&std::isnan(native_attribute_number(value));}
bool truth(const EventAttribute& value) {
  if(type(value)=="NoneType")return false;
  if(is_native_na_attribute(value))fail("TypeError","boolean value of NA is ambiguous");
  if(value.container!=AttributeContainerKind::kScalar)return !value.children.empty();
  if(numeric(value))return native_attribute_number(value)!=0;
  if(text(value)||value.kind==AttributeKind::kBytes)return !value.value.empty();
  return true;
}
bool equal(const EventAttribute& a,const EventAttribute& b) {
  if(type(a)=="NoneType"||type(b)=="NoneType")return type(a)==type(b);
  if(is_native_na_attribute(a)||is_native_na_attribute(b)) {
    if(is_native_na_attribute(a)&&is_native_na_attribute(b))return true;
    fail("TypeError","boolean value of NA is ambiguous");
  }
  if(numeric(a)&&numeric(b))return !is_nan(a)&&!is_nan(b)&&native_scalar_key_compare(a,b)==0;
  if(type(a)!=type(b))return false;
  if(a.container==AttributeContainerKind::kTuple||a.container==AttributeContainerKind::kList) {
    if(a.children.size()!=b.children.size())return false;
    for(std::size_t i=0;i<a.children.size();++i)if(!equal(a.children[i],b.children[i]))return false;
    return true;
  }
  if(type(a)=="NaTType")return false;
  if(a.container!=AttributeContainerKind::kScalar)return root_cause_scalar_keys_equal(a,b);
  return root_cause_scalar_keys_equal(a,b);
}
bool compare(const EventAttribute& a,const EventAttribute& b,std::string_view operation) {
  if(is_native_na_attribute(a)||is_native_na_attribute(b))fail("TypeError","boolean value of NA is ambiguous");
  if(is_nan(a)||is_nan(b))return false;
  int result=0;
  if((numeric(a)&&numeric(b)) || (native_scalar_order_kind(a)!=NativeScalarOrderKind::kUnsupported&&native_scalar_order_kind(a)==native_scalar_order_kind(b)))result=native_scalar_key_compare(a,b);
  else fail("TypeError","'"+std::string(operation)+"' not supported between instances of '"+type(a)+"' and '"+type(b)+"'");
  if(operation=="<=")return result<=0;if(operation==">=")return result>=0;if(operation==">")return result>0;return result<0;
}
EventAttribute whole(std::int64_t value) {EventAttribute result{"",std::to_string(value),AttributeKind::kInt};result.typed_value=value;return result;}
EventAttribute real(double value) {EventAttribute result{"","",AttributeKind::kFloat};result.typed_value=value;return result;}
EventAttribute sequence(AttributeContainerKind kind,std::vector<EventAttribute> children) {
  EventAttribute result;result.container=kind;result.children=std::move(children);
  for(std::size_t i=0;i<result.children.size();++i)result.children[i].key=std::to_string(i);return result;
}
const EventAttribute& index(const EventAttribute& value,std::size_t position) {
  if(value.container!=AttributeContainerKind::kList&&value.container!=AttributeContainerKind::kTuple)fail("TypeError","'"+type(value)+"' object is not subscriptable");
  if(position>=value.children.size())fail("IndexError",type(value)+" index out of range");
  return value.children[position];
}
EventAttribute pair_for(const Event& a,const Event& b,const EventAttribute& key) {
  auto first=required(a,key);auto second=required(b,key);return sequence(AttributeContainerKind::kTuple,{std::move(first),std::move(second)});
}
bool contains(const EventAttribute& collection,const EventAttribute& value) {
  if(text(collection))fail("TypeError","'in <string>' requires string as left operand, not "+type(value));
  if(collection.container!=AttributeContainerKind::kScalar) {
    for(const auto& child:collection.children)if(equal(value,child))return true;return false;
  }
  fail("TypeError","argument of type '"+type(collection)+"' is not iterable");
}
EventAttribute join(const EventAttribute& left,const EventAttribute& right) {
  if(is_native_na_attribute(left)||is_native_na_attribute(right)) {
    if(type(left)=="NoneType"||type(right)=="NoneType")fail("TypeError","unsupported operand type(s) for +: '"+type(left)+"' and '"+type(right)+"'");
    return make_native_na_attribute("");
  }
  if(text(left)&&text(right))return EventAttribute{"",left.value+right.value};
  const auto a=type(left),b=type(right);
  if(a=="str")fail("TypeError","can only concatenate str (not \""+b+"\") to str");
  if(a=="bytes")fail("TypeError","can't concat "+b+" to bytes");
  if(a=="list"||a=="tuple")fail("TypeError","can only concatenate "+a+" (not \""+b+"\") to "+a);
  fail("TypeError","unsupported operand type(s) for +: '"+a+"' and '"+b+"'");
}
EventAttribute path_name(const EventAttribute& left,const EventAttribute& right) {return join(join(left,EventAttribute{"",","}),right);}
bool key_equal(const EventAttribute& a,const EventAttribute& b) {
  // Dictionary lookup does not ask NA == an unrelated string key: their
  // hash buckets differ. NA itself retains its singleton key identity.
  if(is_native_na_attribute(a)||is_native_na_attribute(b))return is_native_na_attribute(a)&&is_native_na_attribute(b);
  return equal(a,b);
}
const EventAttribute* lookup(const LogPathsDictionary& values,const EventAttribute& key) {
  for(const auto& [candidate,value]:values)if(key_equal(candidate,key))return &value;return nullptr;
}
TokenInteger integral(const EventAttribute& value) {
  if(type(value)=="bool")return TokenInteger(static_cast<int>(native_attribute_number(value)));
  return TokenInteger(root_cause_attribute_category(value));
}
EventAttribute multiply(const EventAttribute& a,const EventAttribute& b) {
  if(is_native_na_attribute(a)||is_native_na_attribute(b))return make_native_na_attribute("");
  if(numeric(a)&&numeric(b)) {
    if(type(a)=="float"||type(b)=="float")return real(native_attribute_number(a)*native_attribute_number(b));
    return EventAttribute{"",(integral(a)*integral(b)).decimal(),AttributeKind::kInt};
  }
  if((type(a)=="int"||type(a)=="bool")&&text(b)) {
    std::string result;const auto count=native_attribute_number(a);
    for(std::size_t i=0;i<count;++i)result+=b.value;return EventAttribute{"",std::move(result)};
  }
  fail("TypeError","unsupported operand type(s) for *: '"+type(a)+"' and '"+type(b)+"'");
}
double timestamp(const EventAttribute& value) {
  if(type(value)=="NaTType")fail("ValueError","NaTType does not support timestamp");
  if(type(value)!="datetime.datetime")fail("AttributeError","'"+type(value)+"' object has no attribute 'timestamp'");
  const auto* native=std::get_if<NativeDate>(&value.typed_value);const auto& text=native?native->value:value.value;
  const auto parsed=parse_timestamp_exact(text);if(!parsed)throw std::invalid_argument("paths filter timestamp is invalid");
  auto seconds=parsed->seconds;
  if(!parsed->timezone_aware) {
    // Public datetime.timestamp() uses the host local zone for naive values.
    std::tm local{};local.tm_year=std::stoi(text.substr(0,4))-1900;local.tm_mon=std::stoi(text.substr(5,2))-1;local.tm_mday=std::stoi(text.substr(8,2));
    if(text.size()>=19){local.tm_hour=std::stoi(text.substr(11,2));local.tm_min=std::stoi(text.substr(14,2));local.tm_sec=std::stoi(text.substr(17,2));}
    local.tm_isdst=-1;const auto resolved=std::mktime(&local);if(resolved==static_cast<std::time_t>(-1))throw std::invalid_argument("paths filter naive timestamp is outside the host calendar");seconds=static_cast<std::int64_t>(resolved);
  }
  return static_cast<double>(seconds*1000000+parsed->nanoseconds/1000)/1000000.0;
}
} // namespace

EventLog filter_log_paths(const EventLog& source,const EventAttribute& paths,const LogPathsFilterOptions& options) {
  EventLog result;copy_event_log_context(source,result);
  for(const auto& trace:source.traces) {
    bool found=false;
    for(std::size_t i=1;i<trace.events.size();++i)if(contains(paths,pair_for(trace.events[i-1],trace.events[i],options.attribute_key))){found=true;break;}
    if(found==truth(options.positive))result.traces.push_back(trace);
  }
  return result;
}
EventLog filter_log_paths_performance(const EventLog& source,const EventAttribute& provided_path,const LogPathsPerformanceOptions& options) {
  EventLog result;copy_event_log_context(source,result);
  for(const auto& trace:source.traces) {
    bool found=false;
    for(std::size_t i=1;i<trace.events.size();++i) {
      if(!equal(pair_for(trace.events[i-1],trace.events[i],options.attribute_key),provided_path))continue;
      const auto end=timestamp(required(trace.events[i],options.timestamp_key));
      const auto start=timestamp(required(trace.events[i-1],options.timestamp_key));
      const auto elapsed=real(end-start);
      if(compare(options.minimum,elapsed,"<=")&&compare(elapsed,options.maximum,"<=")){found=true;break;}
    }
    if(found==truth(options.positive))result.traces.push_back(trace);
  }
  return result;
}
LogPathsDictionary get_log_paths_from_log(const EventLog& source,const EventAttribute& attribute_key) {
  LogPathsDictionary result;
  for(const auto& trace:source.traces)for(std::size_t i=1;i<trace.events.size();++i) {
    const auto left=event_value(trace.events[i-1],attribute_key),right=event_value(trace.events[i],attribute_key);
    if(!left||!right)continue;
    const auto key=path_name(*left,*right);
    auto found=std::find_if(result.begin(),result.end(),[&](const auto& entry){return key_equal(entry.first,key);});
    if(found==result.end())result.emplace_back(key,whole(1));else found->second=whole(static_cast<std::int64_t>(native_attribute_number(found->second))+1);
  }
  return result;
}
EventAttribute get_sorted_log_paths_list(const LogPathsDictionary& values) {
  auto sorted=values;std::reverse(sorted.begin(),sorted.end());
  for(std::size_t i=1;i<sorted.size();++i){auto current=sorted[i];auto j=i;while(j&&compare(current.second,sorted[j-1].second,"<")){sorted[j]=sorted[j-1];--j;}sorted[j]=std::move(current);}
  std::reverse(sorted.begin(),sorted.end());std::vector<EventAttribute> rows;
  for(const auto& [key,value]:sorted)rows.push_back(sequence(AttributeContainerKind::kList,{key,value}));return sequence(AttributeContainerKind::kList,std::move(rows));
}
EventAttribute get_sorted_log_paths_list(const EventAttribute& dictionary) {
  LogPathsDictionary values;
  if(dictionary.container==AttributeContainerKind::kContainer)for(const auto& child:dictionary.children){auto value=child;value.key.clear();values.emplace_back(EventAttribute{"",child.key},std::move(value));}
  else if(dictionary.container!=AttributeContainerKind::kList&&dictionary.container!=AttributeContainerKind::kTuple)fail("TypeError","'"+type(dictionary)+"' object is not iterable");
  else if(!dictionary.children.empty())throw std::invalid_argument("paths sort requires an ordinary mapping");
  return get_sorted_log_paths_list(values);
}
EventAttribute get_log_paths_threshold(const EventAttribute& plist,const EventAttribute& decreasing_factor) {
  auto threshold=index(index(plist,0),1);
  for(std::size_t i=1;i<plist.children.size();++i) {
    const auto& count=index(index(plist,i),1);if(!compare(count,multiply(threshold,decreasing_factor),">"))break;threshold=count;
  }
  threshold.key.clear();return threshold;
}
EventLog filter_log_by_paths(const EventLog& source,const LogPathsDictionary& paths,const LogPathsVariants& variants,
    const EventAttribute& vc,const EventAttribute& threshold,const EventAttribute& attribute_key) {
  const auto& first_key=index(index(vc,0),0);
  const auto variant=std::find_if(variants.begin(),variants.end(),[&](const auto& value){return equal(value.key,first_key);});
  if(variant==variants.end())fail("KeyError",key_repr(first_key));
  if(variant->traces.empty())fail("IndexError","list index out of range");
  EventLog first_log;first_log.traces.push_back(variant->traces.front());const auto protected_paths=get_log_paths_from_log(first_log,attribute_key);
  EventLog result;copy_event_log_context(source,result);
  for(const auto& trace:source.traces) {
    if(trace.events.empty())continue;
    Trace filtered;copy_trace_context(trace,filtered);filtered.properties.clear();filtered.events.push_back(trace.events.front());
    for(std::size_t i=1;i+1<trace.events.size();++i) {
      const auto key=path_name(required(trace.events[i],attribute_key),required(trace.events[i+1],attribute_key));
      const auto* count=lookup(paths,key);
      if(count&&(compare(*count,threshold,">=")||lookup(protected_paths,key))){filtered.events.push_back(trace.events[i]);filtered.events.push_back(trace.events[i+1]);}
    }
    if(trace.events.size()>1)filtered.events.push_back(trace.events.back());result.traces.push_back(std::move(filtered));
  }
  return result;
}
} // namespace minera
