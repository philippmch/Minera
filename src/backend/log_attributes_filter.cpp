#include "minera/log_attributes_filter.hpp"
#include "minera/timestamp.hpp"
#include "minera/token_integer.hpp"
#include "event_attribute_erase.hpp"
#include "root_cause_trace_key.hpp"
#include "root_cause_unicode.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <optional>

namespace minera {
namespace {
[[noreturn]] void fail(std::string type,std::string message) {throw LogAttributesFilterError(std::move(type),std::move(message));}
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
    if(first<0x80){}else if(first>=0xc2&&first<=0xdf){code=first&31;n=1;}else if(first>=0xe0&&first<=0xef){code=first&15;n=2;}else if(first>=0xf0&&first<=0xf4){code=first&7;n=3;}else fail("ValueError","attributes filter selector has invalid UTF-8");
    for(unsigned i=0;i<n;++i){if(at==value.size())fail("ValueError","attributes filter selector has short UTF-8");const auto next=static_cast<unsigned char>(value[at++]);if((next&0xc0)!=0x80)fail("ValueError","attributes filter selector has invalid UTF-8 continuation");code=(code<<6)|(next&63);}
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
ParsedExactTimestamp instant(const EventAttribute& v){const auto* n=std::get_if<NativeDate>(&v.typed_value);auto result=parse_timestamp_exact(n?n->value:v.value);if(!result)throw std::invalid_argument("attributes filter contains invalid native datetime");return *result;}
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
bool dictionary_equal(const EventAttribute&a,const EventAttribute&b){if(is_native_na_attribute(a)||is_native_na_attribute(b))return is_native_na_attribute(a)&&is_native_na_attribute(b);return equal(a,b);}
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
bool contains(const EventAttribute& values,const EventAttribute& value){
 if(type(values)=="NoneType")fail("TypeError","argument of type 'NoneType' is not iterable");
 if(values.container==AttributeContainerKind::kList||values.container==AttributeContainerKind::kTuple)return std::any_of(values.children.begin(),values.children.end(),[&](const auto& v){return equal(v,value);});
 if(values.container==AttributeContainerKind::kSet||values.container==AttributeContainerKind::kFrozenSet){hash_key(value);return std::any_of(values.children.begin(),values.children.end(),[&](const auto&v){return dictionary_equal(v,value);});}
 if(values.container==AttributeContainerKind::kContainer){hash_key(value);return text(value)&&std::any_of(values.children.begin(),values.children.end(),[&](const auto&v){return v.key==value.value;});}
 if(text(values)){if(!text(value))fail("TypeError","'in <string>' requires string as left operand, not "+type(value));return values.value.find(value.value)!=std::string::npos;}
 if(values.kind==AttributeKind::kBytes){if(value.kind==AttributeKind::kBytes)return decode_bytes_attribute(values).find(decode_bytes_attribute(value))!=std::string::npos;if(numeric(value)){const auto n=native_attribute_number(value);if(n<0||n>255||std::trunc(n)!=n)fail("ValueError","byte must be in range(0, 256)");return decode_bytes_attribute(values).find(static_cast<char>(n))!=std::string::npos;}fail("TypeError","a bytes-like object is required, not '"+type(value)+"'");}
 fail("TypeError","argument of type '"+type(values)+"' is not iterable");
}
const EventAttribute& selected_key(const LogAttributesFilterOptions&o){return o.enum_attribute_key?*o.enum_attribute_key:o.attribute_key;}
void check_parameters(const LogAttributesFilterOptions&o,bool maximum=false){if(!o.raw_parameters)return;const auto&v=*o.raw_parameters;if(v.is_null||(v.container==AttributeContainerKind::kList&&v.children.empty()))return;fail("TypeError",maximum?"argument of type '"+type(v)+"' is not iterable":"'"+type(v)+"' object is not iterable");}
std::optional<EventAttribute> trace_value(const Trace&t,const EventAttribute&key){hash_key(key);if(!text(key))return{};for(auto i=t.attributes.rbegin();i!=t.attributes.rend();++i)if(i->key==key.value)return present(*i);const auto name=!t.case_id_source_key.empty()?t.case_id_source_key:t.case_id_attribute?t.case_id_attribute->key:"concept:name";if(name==key.value&&(t.case_id_attribute||!t.case_id.empty()||t.case_id_state!=FirstClassAttributeState::kMissing)){auto v=t.case_id_attribute?present(*t.case_id_attribute):std::make_optional(EventAttribute{key.value,t.case_id});if(v&&t.case_id_state==FirstClassAttributeState::kNull)v->is_null=true;return v;}return{};}
void upsert(std::vector<EventAttribute>&attrs,EventAttribute value){for(auto&old:attrs)if(old.key==value.key){old=std::move(value);return;}attrs.push_back(std::move(value));}
void inject(EventLog&source){for(auto&t:source.traces){auto attrs=t.attributes;const auto name=!t.case_id_source_key.empty()?t.case_id_source_key:t.case_id_attribute?t.case_id_attribute->key:"concept:name";if(std::none_of(attrs.begin(),attrs.end(),[&](const auto&a){return a.key==name;}))if(auto v=trace_value(t,EventAttribute{"",name})){v->key=name;attrs.push_back(*v);}for(auto&e:t.events)for(auto a:attrs){if(!present(a))continue;a.key="case:"+a.key;a.first_class_role=FirstClassAttributeRole::kNone;a.first_class_state=FirstClassAttributeState::kMissing;a.first_class_source_key.clear();upsert(e.attributes,std::move(a));}}}
void case_projection(Trace&t){for(const auto&a:t.attributes)if(a.key=="concept:name"){t.case_id=a.value;t.case_id_attribute=a;t.case_id_source_key=a.key;t.case_id_state=a.is_null?FirstClassAttributeState::kNull:FirstClassAttributeState::kPresent;return;}}
EventLog regroup(const EventLog&source,const std::vector<Event>&events,const EventAttribute&case_key){EventLog result;copy_event_log_context(source,result);std::vector<EventAttribute>ids;for(auto e:events){auto key=required(e,case_key);hash_key(key);auto at=std::find_if(ids.begin(),ids.end(),[&](const auto&v){return dictionary_equal(v,key);});const auto index=static_cast<std::size_t>(at-ids.begin());if(at==ids.end()){Trace t;for(auto a:e.attributes)if(a.key.starts_with("case:")){a.key.erase(0,5);t.attributes.push_back(std::move(a));}if(std::none_of(t.attributes.begin(),t.attributes.end(),[](const auto&a){return a.key=="concept:name";})){auto canonical=key;canonical.key="concept:name";t.attributes.push_back(std::move(canonical));}case_projection(t);result.traces.push_back(std::move(t));ids.push_back(std::move(key));}detail::erase_event_attributes(e,[](const auto&a){return a.key.starts_with("case:");});result.traces[index].events.push_back(std::move(e));}return result;}
EventAttribute event_case_key(const LogAttributesFilterOptions&o){return o.case_id_key.value_or(EventAttribute{"","case:concept:name"});}
bool in_range(const EventAttribute&v,const EventAttribute&lo,const EventAttribute&hi){return compare(lo,v,"<=")&&compare(v,hi,"<=");}
// Positive keeps lo <= v <= hi; negative keeps v < lo or v > hi (so NaN is never kept).
bool numeric_kept(const EventAttribute&v,const EventAttribute&lo,const EventAttribute&hi,const EventAttribute&positive){if(truth(positive))return in_range(v,lo,hi);return compare(v,lo,"<")||compare(v,hi,">");}
bool stream_matches(const Event&e,const LogAttributesFilterOptions&o){for(const auto pair:{std::pair{&o.stream_filter_key1,&o.stream_filter_value1},std::pair{&o.stream_filter_key2,&o.stream_filter_value2}})if(*pair.first&&!(*pair.first)->is_null){const auto v=event_value(e,**pair.first);if(!v||!equal(*v,*pair.second,false))return false;}return true;}
struct Count{EventAttribute key;std::size_t value=0;};
std::vector<Count> counts(const EventLog&source,const EventAttribute&key,bool once){std::vector<Count>result;for(const auto&t:source.traces){std::vector<EventAttribute>seen;for(const auto&e:t.events)if(auto v=event_value(e,key)){hash_key(*v);if(once&&std::any_of(seen.begin(),seen.end(),[&](const auto&w){return dictionary_equal(w,*v);}))continue;seen.push_back(*v);auto at=std::find_if(result.begin(),result.end(),[&](const auto&w){return dictionary_equal(w.key,*v);});if(at==result.end())result.push_back({*v,1});else++at->value;}}return result;}
// Python's glue + case_id_key, which builds the stream case key.
TokenInteger whole_number(const EventAttribute&v){if(type(v)=="bool")return TokenInteger(truth(v)?1:0);if(const auto* s=std::get_if<std::int64_t>(&v.typed_value))return TokenInteger(*s);if(const auto* u=std::get_if<std::uint64_t>(&v.typed_value))return TokenInteger(*u);return TokenInteger(v.value);}
EventAttribute plus(const EventAttribute&a,const EventAttribute&b){
 if(text(a)){if(text(b))return EventAttribute{"",a.value+b.value};fail("TypeError","can only concatenate str (not \""+type(b)+"\") to str");}
 if(numeric(a)&&numeric(b)){if(type(a)=="float"||type(b)=="float")return real(native_attribute_number(a)+native_attribute_number(b));return EventAttribute{"",(whole_number(a)+whole_number(b)).decimal(),AttributeKind::kInt};}
 fail("TypeError","unsupported operand type(s) for +: '"+type(a)+"' and '"+type(b)+"'");
}
EventAttribute multiplied(const EventAttribute&v,std::size_t n){if(is_native_na_attribute(v))return v;if(numeric(v)){if(type(v)=="float")return real(native_attribute_number(v)*static_cast<double>(n));TokenInteger integer;if(type(v)=="bool")integer=TokenInteger(truth(v)?1:0);else if(const auto* signed_value=std::get_if<std::int64_t>(&v.typed_value))integer=TokenInteger(*signed_value);else if(const auto* unsigned_value=std::get_if<std::uint64_t>(&v.typed_value))integer=TokenInteger(*unsigned_value);else integer=TokenInteger(v.value);return EventAttribute{"",(integer*TokenInteger(n)).decimal(),AttributeKind::kInt};}if(text(v)||v.kind==AttributeKind::kBytes||v.container==AttributeContainerKind::kList||v.container==AttributeContainerKind::kTuple)return v;fail("TypeError","unsupported operand type(s) for *: '"+type(v)+"' and 'int'");}
EventAttribute sequence(std::vector<EventAttribute>values,AttributeContainerKind kind=AttributeContainerKind::kList){EventAttribute v;v.container=kind;v.children=std::move(values);return v;}
void set_parameter(LogAttributesFilterParameters&params,std::string key,EventAttribute value){for(auto&p:params)if(!p.enum_key&&p.key==key){p.value=std::move(value);return;}params.push_back({std::move(key),std::move(value),false});}
EventAttribute index_zero(const EventAttribute&v){if(v.container==AttributeContainerKind::kList||v.container==AttributeContainerKind::kTuple){if(v.children.empty())fail("IndexError","list index out of range");return v.children.front();}if(text(v)){if(v.value.empty())fail("IndexError","string index out of range");auto n=std::size_t{1};while(n<v.value.size()&&(static_cast<unsigned char>(v.value[n])&0xc0)==0x80)++n;return EventAttribute{"",v.value.substr(0,n)};}fail("TypeError","'"+type(v)+"' object is not subscriptable");}
} // namespace
LogAttributesFilterOptions resolve_log_attributes_filter_parameters(const LogAttributesFilterParameters&params){
 LogAttributesFilterOptions o;
 const auto value=[&](std::string_view key,bool enums=true)->std::optional<EventAttribute>{if(enums)for(auto i=params.rbegin();i!=params.rend();++i)if(i->enum_key&&i->key==key)return i->value;for(auto i=params.rbegin();i!=params.rend();++i)if(!i->enum_key&&i->key==key)return i->value;return{};};
 if(auto v=value("pm4py:param:attribute_key",false))o.attribute_key=*v;
 for(auto i=params.rbegin();i!=params.rend();++i)if(i->enum_key&&i->key=="pm4py:param:attribute_key"){o.enum_attribute_key=i->value;break;}
 if(auto v=value("pm4py:param:activity_key"))o.activity_key=*v;
 if(auto v=value("positive"))o.positive=*v;
 o.case_id_key=value("pm4py:param:case_id_key");
 if(auto v=value("case_id_glue"))o.case_id_glue=*v;
 o.stream_filter_key1=value("stream_filter_key1");o.stream_filter_key2=value("stream_filter_key2");
 if(auto v=value("stream_filter_value1"))o.stream_filter_value1=*v;
 if(auto v=value("stream_filter_value2"))o.stream_filter_value2=*v;
 o.keep_once_per_case=value("keep_once_per_case");return o;
}
EventLog filter_log_attributes_traces(const EventLog&source,const EventAttribute&values,const LogAttributesFilterOptions&o){
 check_parameters(o);EventLog out;copy_event_log_context(source,out);
 for(const auto&t:source.traces){if(t.events.empty())continue;bool found=false;for(const auto&e:t.events)if(const auto v=event_value(e,selected_key(o));v&&contains(values,*v)){found=true;break;}if(found==truth(o.positive))out.traces.push_back(t);}return out;
}
EventLog filter_log_attributes_trace_values(const EventLog&source,const EventAttribute&values,const LogAttributesFilterOptions&o){
 check_parameters(o);EventLog out;copy_event_log_context(source,out);
 for(const auto&t:source.traces){const auto v=trace_value(t,selected_key(o));const bool found=v&&contains(values,*v);if(found==truth(o.positive))out.traces.push_back(t);}return out;
}
EventLog filter_log_attributes_events(EventLog&source,const EventAttribute&values,const LogAttributesFilterOptions&o){
 check_parameters(o);inject(source);std::vector<Event>events;
 for(const auto&t:source.traces)for(const auto&e:t.events){const bool matched=contains(values,required(e,selected_key(o)));if(matched==truth(o.positive))events.push_back(e);}
 return regroup(source,events,event_case_key(o));
}
EventLog filter_log_attributes_events(const EventLog&source,const EventAttribute&values,const LogAttributesFilterOptions&o){auto copy=source;return filter_log_attributes_events(copy,values,o);}
EventLog filter_log_attributes_numeric_events(EventLog&source,const EventAttribute&lower,const EventAttribute&upper,const LogAttributesFilterOptions&o){
 check_parameters(o);inject(source);std::vector<Event>events;
 for(const auto&t:source.traces)for(const auto&e:t.events)if(const auto v=event_value(e,selected_key(o))){if(numeric_kept(*v,lower,upper,o.positive))events.push_back(e);}
 return regroup(source,events,event_case_key(o));
}
EventLog filter_log_attributes_numeric_events(const EventLog&source,const EventAttribute&lower,const EventAttribute&upper,const LogAttributesFilterOptions&o){auto copy=source;return filter_log_attributes_numeric_events(copy,lower,upper,o);}
EventLog filter_log_attributes_numeric(EventLog&source,const EventAttribute&lower,const EventAttribute&upper,const LogAttributesFilterOptions&o){
 check_parameters(o);inject(source);const auto trace_key=o.case_id_key.value_or(EventAttribute{"","concept:name"});
 std::optional<EventAttribute>stream_key;std::vector<EventAttribute>ids;
 for(const auto&t:source.traces)for(const auto&e:t.events)if(stream_matches(e,o))if(const auto v=event_value(e,selected_key(o))){if(numeric_kept(*v,lower,upper,o.positive)){if(!stream_key)stream_key=plus(o.case_id_glue,trace_key);auto id=required(e,*stream_key);hash_key(id);if(std::none_of(ids.begin(),ids.end(),[&](const auto&v){return dictionary_equal(v,id);}))ids.push_back(std::move(id));}}
 EventLog out;copy_event_log_context(source,out);
 for(const auto&t:source.traces){const auto id=trace_value(t,trace_key);if(!id)fail("KeyError",key_repr(trace_key));hash_key(*id);if(std::any_of(ids.begin(),ids.end(),[&](const auto&v){return dictionary_equal(v,*id);}))out.traces.push_back(t);}return out;
}
EventLog filter_log_attributes_numeric(const EventLog&source,const EventAttribute&lower,const EventAttribute&upper,const LogAttributesFilterOptions&o){auto copy=source;return filter_log_attributes_numeric(copy,lower,upper,o);}
EventLog filter_log_attributes_relative_occurrence(EventLog&source,const EventAttribute&minimum,const LogAttributesFilterOptions&o){
 check_parameters(o);const bool once=o.keep_once_per_case&&truth(*o.keep_once_per_case);
 const bool case_denominator=!o.keep_once_per_case||once;
 const auto values=counts(source,selected_key(o),once);std::size_t total=source.traces.size();if(!case_denominator){total=0;for(const auto&t:source.traces)total+=t.events.size();}
 // The threshold is only computed when there is a counted value to compare.
 std::vector<EventAttribute>admitted;
 if(!values.empty()){const auto threshold=multiplied(minimum,total);
 for(const auto&v:values)if(compare(number(v.value),threshold,">="))admitted.push_back(v.key);}
 return filter_log_attributes_events(source,sequence(std::move(admitted),AttributeContainerKind::kSet),o);
}
EventLog filter_log_attributes_relative_occurrence(const EventLog&source,const EventAttribute&minimum,const LogAttributesFilterOptions&o){auto copy=source;return filter_log_attributes_relative_occurrence(copy,minimum,o);}
EventLog filter_log_attributes_max_activities(EventLog&source,const EventAttribute&maximum,LogAttributesFilterParameters&params){
 // The public function reads the activity key under its plain string name only
 // (an ACTIVITY_KEY enum member is ignored) and never reads keep_once_per_case.
 EventAttribute activity_key{"","concept:name"};
 for(auto i=params.rbegin();i!=params.rend();++i)if(!i->enum_key&&i->key=="pm4py:param:activity_key"){activity_key=i->value;break;}
 set_parameter(params,"pm4py:param:attribute_key",activity_key);
 const auto o=resolve_log_attributes_filter_parameters(params);
 auto values=counts(source,activity_key,false);
 if(!compare(maximum,number(values.size()),"<"))return source;
 if(type(maximum)!="int"&&type(maximum)!="bool")fail("TypeError","slice indices must be integers or None or have an __index__ method");
 // A log without activity values is returned whole, even for negative maxima.
 if(values.empty())return source;
 std::stable_sort(values.begin(),values.end(),[](const auto&a,const auto&b){return a.value>b.value;});
 EventAttribute negative_size{"","-"+std::to_string(values.size()),AttributeKind::kInt};std::size_t size=values.size();
 if(native_scalar_key_compare(maximum,number(0))<0&&native_scalar_key_compare(maximum,negative_size)<=0)size=0;
 else{const auto n=native_attribute_number(maximum);size=n<0?size-static_cast<std::size_t>(-n):static_cast<std::size_t>(n);}
 std::vector<EventAttribute>admitted;for(std::size_t i=0;i<size;++i)admitted.push_back(values[i].key);
 return filter_log_attributes_events(source,sequence(std::move(admitted)),o);
}
EventLog filter_log_attributes_max_activities(EventLog&source,const EventAttribute&maximum,const LogAttributesFilterOptions&o){
 check_parameters(o,true);LogAttributesFilterParameters params={{"pm4py:param:attribute_key",o.attribute_key,false},{"pm4py:param:activity_key",o.activity_key,false},{"positive",o.positive,false}};
 if(o.enum_attribute_key)params.push_back({"pm4py:param:attribute_key",*o.enum_attribute_key,true});if(o.case_id_key)params.push_back({"pm4py:param:case_id_key",*o.case_id_key,false});if(o.keep_once_per_case)params.push_back({"keep_once_per_case",*o.keep_once_per_case,false});return filter_log_attributes_max_activities(source,maximum,params);
}
EventLog filter_log_attributes_max_activities(const EventLog&source,const EventAttribute&maximum,const LogAttributesFilterOptions&o){auto copy=source;return filter_log_attributes_max_activities(copy,maximum,o);}
EventLog filter_log_attributes_threshold(const EventLog&source,const LogAttributesCountInput&attributes,const LogAttributesVariantInput&variants,const EventAttribute&vc,const EventAttribute&threshold,const EventAttribute&key){
 const auto variant_key=index_zero(index_zero(vc));hash_key(variant_key);const auto* cache=std::get_if<LogVariantsDictionary>(&variants);
 if(!cache)fail("TypeError","'"+type(std::get<EventAttribute>(variants))+"' object is not subscriptable");
 const auto entry=std::find_if(cache->begin(),cache->end(),[&](const auto&v){return dictionary_equal(v.variant,variant_key);});if(entry==cache->end())fail("KeyError",key_repr(variant_key));
 if(entry->traces.empty())fail("IndexError","list index out of range");std::vector<EventAttribute>exempt;
 // Events of the protected trace without the key are skipped, not an error.
 for(const auto&e:entry->traces.front().events)if(auto value=event_value(e,key))exempt.push_back(std::move(*value));
 const auto allowed=sequence(std::move(exempt));EventLog out;copy_event_log_context(source,out);
 for(const auto&t:source.traces){Trace filtered=t;filtered.events.clear();filtered.properties.clear();for(const auto&e:t.events)if(const auto value=event_value(e,key)){
   std::optional<EventAttribute>count;
   if(const auto* map=std::get_if<LogAttributesCounts>(&attributes)){hash_key(*value);const auto at=std::find_if(map->begin(),map->end(),[&](const auto&pair){return dictionary_equal(pair.first,*value);});if(at!=map->end())count=at->second;}
   else {const auto&raw=std::get<EventAttribute>(attributes);if(type(raw)=="NoneType")fail("TypeError","argument of type 'NoneType' is not iterable");if(contains(raw,*value)){if(raw.container==AttributeContainerKind::kContainer){for(const auto&v:raw.children)if(text(*value)&&v.key==value->value)count=v;}else fail("TypeError","list indices must be integers or slices, not str");}}
   // Protected values are kept before the threshold is compared.
   if(count&&(contains(allowed,*value)||compare(*count,threshold,">=")))filtered.events.push_back(e);
  }if(!filtered.events.empty())out.traces.push_back(std::move(filtered));}return out;
}
} // namespace minera
