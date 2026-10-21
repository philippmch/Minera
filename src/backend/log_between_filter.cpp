#include "minera/log_between_filter.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "root_cause_trace_key.hpp"
#include "root_cause_unicode.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

namespace minera {
namespace {
[[noreturn]] void fail(std::string type,std::string message) {throw LogBetweenFilterError(std::move(type),std::move(message));}
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
    if(first<0x80){}else if(first>=0xc2&&first<=0xdf){code=first&31;n=1;}else if(first>=0xe0&&first<=0xef){code=first&15;n=2;}else if(first>=0xf0&&first<=0xf4){code=first&7;n=3;}else fail("ValueError","between selector has invalid UTF-8");
    for(unsigned i=0;i<n;++i){if(at==value.size())fail("ValueError","between selector has short UTF-8");const auto next=static_cast<unsigned char>(value[at++]);if((next&0xc0)!=0x80)fail("ValueError","between selector has invalid UTF-8 continuation");code=(code<<6)|(next&63);}
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
    auto result=stored?present(*stored):std::make_optional(EventAttribute{key.value,value});if(result&&state==FirstClassAttributeState::kNull)result->is_null=true;return result;
  };
  for(const auto value:{projected("concept:name",event.activity_source_key,event.activity_attribute,event.activity_state,event.activity),projected("time:timestamp",event.timestamp_source_key,event.timestamp_attribute,event.timestamp_state,event.timestamp),projected("org:resource",event.resource_source_key,event.resource_attribute,event.resource_state,event.resource),projected("lifecycle:transition",event.lifecycle_source_key,event.lifecycle_attribute,event.lifecycle_state,event.lifecycle)})if(value)return value;
  return std::nullopt;
}
EventAttribute required(const Event& event,const EventAttribute& key) {
  if(auto value=event_value(event,key))return *value;fail("KeyError",key_repr(key));
}
bool member_equal(const EventAttribute& candidate,const EventAttribute& target) {
  if(is_native_na_attribute(candidate) || is_native_na_attribute(target)) {
    if(candidate.is_null||target.is_null)return false;
    fail("TypeError","boolean value of NA is ambiguous");
  }
  return text(candidate)&&text(target)&&candidate.value==target.value;
}
bool contains(const EventAttribute& endpoint,const EventAttribute& candidate) {
  if(text(endpoint))return member_equal(candidate,endpoint);
  if(endpoint.container==AttributeContainerKind::kContainer) {
    for(const auto& child:endpoint.children)if(member_equal(candidate,EventAttribute{"",child.key}))return true;
    return false;
  }
  if(endpoint.container!=AttributeContainerKind::kScalar) {
    for(const auto& child:endpoint.children)if(member_equal(candidate,child))return true;
    return false;
  }
  if(endpoint.kind==AttributeKind::kBytes)fail("TypeError","a bytes-like object is required, not '"+type(candidate)+"'");
  fail("TypeError","argument of type '"+type(endpoint)+"' is not iterable");
}
bool same_endpoints(const EventAttribute& left,const EventAttribute& right) {
  if(text(left)&&text(right))return left.value==right.value;
  if(left.container!=AttributeContainerKind::kList || right.container!=AttributeContainerKind::kList || left.children.size()!=right.children.size())return false;
  for(std::size_t index=0;index<left.children.size();++index) {
    const auto& a=left.children[index];const auto& b=right.children[index];
    if(!text(a)||!text(b)||a.value!=b.value)return false;
  }
  return true;
}
std::string primary_key(const Trace& trace) {
  return !trace.case_id_source_key.empty()?trace.case_id_source_key:trace.case_id_attribute?trace.case_id_attribute->key:"concept:name";
}
EventAttribute case_value(const Trace& trace,const EventAttribute& key,std::size_t position) {
  hash_key(key);
  if(text(key)) {
    for(auto at=trace.attributes.rbegin();at!=trace.attributes.rend();++at)if(at->key==key.value){if(auto value=present(*at))return *value;break;}
    if(primary_key(trace)==key.value && (trace.case_id_attribute || !trace.case_id.empty() || trace.case_id_state!=FirstClassAttributeState::kMissing)) {
      auto value=trace.case_id_attribute?present(*trace.case_id_attribute):std::make_optional(EventAttribute{key.value,trace.case_id});
      if(value){if(trace.case_id_state==FirstClassAttributeState::kNull)value->is_null=true;return *value;}
    }
  }
  return EventAttribute{"",std::to_string(position)};
}
EventAttribute concatenate(const EventAttribute& left,const EventAttribute& right) {
  const auto a=type(left),b=type(right);
  if(is_native_na_attribute(left)||is_native_na_attribute(right)) {
    if(a=="NoneType"||b=="NoneType")fail("TypeError","unsupported operand type(s) for +: '"+a+"' and '"+b+"'");
    return make_native_na_attribute("");
  }
  if(text(left)&&text(right))return EventAttribute{"",left.value+right.value};
  // Successful first additions still fail at the subsequent string suffix.
  // Preserve their result type so the public error comes from that second
  // operation, rather than rejecting a valid numeric/container first addition.
  const auto numeric=[](const std::string& value){return value=="bool"||value=="int"||value=="float";};
  if(numeric(a)&&numeric(b)) {
    EventAttribute value{"","0",a=="float"||b=="float"?AttributeKind::kFloat:AttributeKind::kInt};
    if(value.kind==AttributeKind::kFloat)value.typed_value=0.0;else value.typed_value=std::int64_t{0};
    return value;
  }
  if(a==b && (a=="bytes"||a=="list"||a=="tuple"))return left;
  if(a=="str")fail("TypeError","can only concatenate str (not \""+b+"\") to str");
  if(a=="bytes")fail("TypeError","can't concat "+b+" to bytes");
  if(a=="list"||a=="tuple")fail("TypeError","can only concatenate "+a+" (not \""+b+"\") to "+a);
  fail("TypeError","unsupported operand type(s) for +: '"+a+"' and '"+b+"'");
}
Trace open_cut(const Trace& source,const EventAttribute& key,const EventAttribute& separator,std::size_t position,std::size_t counter,EventAttribute& renamed) {
  renamed=concatenate(concatenate(case_value(source,key,position),separator),EventAttribute{"",std::to_string(counter)});
  Trace result;copy_trace_context(source,result);result.properties.clear();
  if(text(key)) {
    const auto retain_provenance=[](EventAttribute value,const EventAttribute& old) {
      value.first_class_role=old.first_class_role;
      value.first_class_source_key=old.first_class_source_key;
      if(old.first_class_role!=FirstClassAttributeRole::kNone || old.first_class_state!=FirstClassAttributeState::kMissing)
        value.first_class_state=value.is_null?FirstClassAttributeState::kNull:FirstClassAttributeState::kPresent;
      return value;
    };
    renamed.key=key.value;bool replaced=false;
    for(auto at=result.attributes.rbegin();at!=result.attributes.rend();++at)if(at->key==key.value){*at=retain_provenance(renamed,*at);replaced=true;break;}
    if(!replaced)result.attributes.push_back(renamed);
    if(primary_key(source)==key.value){result.case_id=renamed.value;result.case_id_state=renamed.is_null?FirstClassAttributeState::kNull:FirstClassAttributeState::kPresent;result.case_id_source_key=key.value;result.case_id_attribute=source.case_id_attribute?retain_provenance(renamed,*source.case_id_attribute):renamed;}
  }
  return result;
}
}  // namespace

EventLog LogBetweenFilterResult::project_event_log() const {
  if(!typed_trace_attributes.empty())throw std::invalid_argument("Between result has nonstring trace keys; download the full typed result instead of an EventLog projection");
  return string_key_log;
}
LogBetweenFilterResult filter_log_between_result(const EventLog& source,const EventAttribute& act1,const EventAttribute& act2,const LogBetweenFilterOptions& options) {
  LogBetweenFilterResult result;copy_event_log_context(source,result.string_key_log);
  for(std::size_t position=0;position<source.traces.size();++position) {
    const auto& trace=source.traces[position];std::optional<Trace> cut;EventAttribute renamed;std::size_t counter=0;
    for(const auto& event:trace.events) {
      const auto value=required(event,options.activity_key);
      if(!cut) {
        if(contains(act1,value)){cut=open_cut(trace,options.case_id_key,options.subcase_separator,position,counter,renamed);cut->events.push_back(event);}
        continue;
      }
      cut->events.push_back(event);
      if(!contains(act2,value))continue;
      const auto index=result.string_key_log.traces.size();result.string_key_log.traces.push_back(std::move(*cut));cut.reset();
      if(!text(options.case_id_key))result.typed_trace_attributes.push_back({index,options.case_id_key,renamed});
      ++counter;
      if(same_endpoints(act1,act2)){cut=open_cut(trace,options.case_id_key,options.subcase_separator,position,counter,renamed);cut->events.push_back(event);}
    }
  }
  return result;
}
EventLog filter_log_between(const EventLog& source,const EventAttribute& act1,const EventAttribute& act2,const LogBetweenFilterOptions& options) {return filter_log_between_result(source,act1,act2,options).project_event_log();}
EventLog filter_log_between(const EventLog& source,std::string_view act1,std::string_view act2,const LogBetweenFilterOptions& options) {return filter_log_between(source,EventAttribute{"",std::string(act1)},EventAttribute{"",std::string(act2)},options);}
std::string serialize_log_between_filter_result_json(const LogBetweenFilterResult& result) {
  EventTable sidecar;sidecar.columns={"trace_index","key","value"};
  for(const auto& entry:result.typed_trace_attributes) {
    EventAttribute index{"trace_index",std::to_string(entry.trace_index),AttributeKind::kInt};index.typed_value=static_cast<std::uint64_t>(entry.trace_index);
    auto key=entry.key,value=entry.value;key.key="key";value.key="value";sidecar.rows.push_back({std::move(index),std::move(key),std::move(value)});
  }
  return "{\"schema\":\"log-between-result-v1\",\"string_key_log\":"+serialize_embedding_workbench_raw_log_json(result.string_key_log)+",\"typed_trace_attributes\":"+serialize_embedding_workbench_table_json(sidecar)+'}';
}
}  // namespace minera
