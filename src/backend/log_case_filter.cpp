#include "minera/log_case_filter.hpp"
#include "minera/timestamp.hpp"
#include "root_cause_trace_key.hpp"
#include "root_cause_unicode.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

namespace minera {
namespace {
[[noreturn]] void fail(std::string type,std::string message) {throw LogCaseFilterError(std::move(type),std::move(message));}
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
    if(first<0x80){}else if(first>=0xc2&&first<=0xdf){code=first&31;n=1;}else if(first>=0xe0&&first<=0xef){code=first&15;n=2;}else if(first>=0xf0&&first<=0xf4){code=first&7;n=3;}else fail("ValueError","case filter selector has invalid UTF-8");
    for(unsigned i=0;i<n;++i){if(at==value.size())fail("ValueError","case filter selector has short UTF-8");const auto next=static_cast<unsigned char>(value[at++]);if((next&0xc0)!=0x80)fail("ValueError","case filter selector has invalid UTF-8 continuation");code=(code<<6)|(next&63);}
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
bool numeric(const EventAttribute& value) {
  const auto kind=type(value);return kind=="int"||kind=="bool"||kind=="float";
}
bool nan(const EventAttribute& value) {
  return type(value)=="float"&&std::isnan(native_attribute_number(value));
}
bool truth(const LogCasePerformancePredicate& value) {
  if(const auto* result=std::get_if<bool>(&value))return *result;
  fail("TypeError","boolean value of NA is ambiguous");
}
LogCasePerformancePredicate compare(const EventAttribute& left,const EventAttribute& right,std::string_view operation) {
  if(is_native_na_attribute(left)||is_native_na_attribute(right))return NativeNA{};
  if(!numeric(left)||!numeric(right))fail("TypeError","'"+std::string(operation)+"' not supported between instances of '"+type(left)+"' and '"+type(right)+"'");
  if(nan(left)||nan(right))return false;
  const auto result=native_scalar_key_compare(left,right);
  if(operation=="<=")return result<=0;
  if(operation==">=")return result>=0;
  return result<0;
}
EventAttribute integer(std::size_t value) {
  EventAttribute result{"",std::to_string(value),AttributeKind::kInt};result.typed_value=static_cast<std::uint64_t>(value);return result;
}
EventAttribute real(double value) {
  EventAttribute result{"","",AttributeKind::kFloat};result.typed_value=value;return result;
}
double duration(const EventAttribute& last,const EventAttribute& first) {
  const auto a=type(last),b=type(first);
  if(a=="NaTType"||b=="NaTType")return std::numeric_limits<double>::quiet_NaN();
  if(a=="datetime.datetime"&&b=="datetime.datetime") {
    const auto parse=[](const EventAttribute& value){
      const auto* native=std::get_if<NativeDate>(&value.typed_value);
      const auto parsed=parse_timestamp_exact(native?native->value:value.value);
      if(!parsed)throw std::invalid_argument("case filter timestamp is not a valid native datetime");
      return *parsed;
    };
    const auto end=parse(last),start=parse(first);
    if(end.timezone_aware!=start.timezone_aware)fail("TypeError","can't subtract offset-naive and offset-aware datetimes");
    const auto seconds=end.seconds-start.seconds;
    const auto nanos=static_cast<std::int64_t>(end.nanoseconds)-start.nanoseconds;
    // Public datetime subtraction is exact in microseconds before conversion
    // to timedelta.total_seconds(). Never subtract two floating epochs.
    return static_cast<double>(seconds*1000000+nanos/1000)/1000000.0;
  }
  std::string difference;
  if(is_native_na_attribute(last)||is_native_na_attribute(first))difference="NAType";
  else if(numeric(last)&&numeric(first))difference=a=="float"||b=="float"?"float":"int";
  else if((last.container==AttributeContainerKind::kSet||last.container==AttributeContainerKind::kFrozenSet)&&
          (first.container==AttributeContainerKind::kSet||first.container==AttributeContainerKind::kFrozenSet))difference=a;
  else fail("TypeError","unsupported operand type(s) for -: '"+a+"' and '"+b+"'");
  fail("AttributeError","'"+difference+"' object has no attribute 'total_seconds'");
}
} // namespace

EventLog apply_log_case_filter(const EventLog&,const EventAttribute&) {
  fail("NotImplementedError","apply method not available for case filter");
}
EventLog apply_log_case_auto_filter(const EventLog&,const EventAttribute&) {
  fail("NotImplementedError","apply_auto_filter method not available for case filter");
}
EventLog filter_log_on_case_size(const EventLog& source,const EventAttribute& minimum,const EventAttribute& maximum) {
  EventLog result;
  for(const auto& trace:source.traces) {
    const auto count=integer(trace.events.size());
    if(truth(compare(count,minimum,">=")) && (type(maximum)=="NoneType"||truth(compare(count,maximum,"<="))))result.traces.push_back(trace);
  }
  return result;
}
EventLog filter_log_on_ncases(const EventLog& source,const EventAttribute& maximum) {
  const auto count=integer(source.traces.size());
  auto selected=source.traces.size();
  // Public min(maximum, len(log)) compares first, then Python slices. A float
  // below the length errors as a slice index; a larger float selects len(log).
  if(truth(compare(maximum,count,"<"))) {
    const auto kind=type(maximum);
    if(kind!="int"&&kind!="bool")fail("TypeError","slice indices must be integers or None or have an __index__ method");
    const auto zero=integer(0);
    if(truth(compare(maximum,zero,"<"))) {
      // Clamp before integer conversion, including values outside int64.
      const auto negative_size=real(-static_cast<double>(source.traces.size()));
      if(truth(compare(maximum,negative_size,"<=")))selected=0;
      else selected=static_cast<std::size_t>(static_cast<std::int64_t>(source.traces.size())+static_cast<std::int64_t>(native_attribute_number(maximum)));
    } else selected=static_cast<std::size_t>(native_attribute_number(maximum));
  }
  EventLog result;result.traces.assign(source.traces.begin(),source.traces.begin()+selected);return result;
}
LogCasePerformancePredicate log_case_satisfies_performance(const Trace& trace,const EventAttribute& minimum,
    const EventAttribute& maximum,const EventAttribute& timestamp_key) {
  if(trace.events.empty())return false;
  // Python evaluates the left subtraction operand (last event) first.
  const auto last=required(trace.events.back(),timestamp_key);
  const auto first=required(trace.events.front(),timestamp_key);
  const auto elapsed=real(duration(last,first));
  const auto lower=compare(minimum,elapsed,"<=");
  if(!truth(lower))return false;
  return compare(elapsed,maximum,"<=");
}
EventLog filter_log_on_case_performance(const EventLog& source,const EventAttribute& minimum,
    const EventAttribute& maximum,const LogCasePerformanceOptions& options) {
  EventLog result;
  for(const auto& trace:source.traces)if(truth(log_case_satisfies_performance(trace,minimum,maximum,options.timestamp_key)))result.traces.push_back(trace);
  return result;
}
EventLog filter_log_case_performance(const EventLog& source,const EventAttribute& minimum,
    const EventAttribute& maximum,const LogCasePerformanceOptions& options) {
  return filter_log_on_case_performance(source,minimum,maximum,options);
}
} // namespace minera
