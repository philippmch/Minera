#include "minera/root_cause_workbench_json.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/footprints_workbench_json.hpp"
#include "minera/literal_controls_json.hpp"
#include "minera/xes_export.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <limits>
#include <locale>
#include <map>
#include <memory>
#include <new>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

namespace minera {
namespace {
[[noreturn]] void invalid(std::string_view message) {
  throw std::invalid_argument("root cause workbench JSON: " + std::string(message));
}
std::string q(std::string_view value) { return serialize_embedding_workbench_string_json(value); }
std::ostringstream output() { std::ostringstream out; out.imbue(std::locale::classic()); return out; }

// A bounded structural index into submitted JSON. Native string/float parsing
// delegates to the existing public codecs; this index never parses output.
struct Json {
  enum class Kind { object, array, string, number, boolean, null } kind;
  std::string_view raw;
  std::map<std::string, Json> fields;
  std::vector<Json> items;
};
std::string string_token(std::string_view raw) {
  auto values = parse_literal_string_vector_json("[" + std::string(raw) + "]");
  if (values.size() != 1) invalid("expected one JSON string");
  return std::move(values.front());
}
class Parser {
 public:
  explicit Parser(std::string_view text) : text_(text) {
    if (text.size() > 32u * 1024u * 1024u) invalid("request exceeds 32 MiB");
  }
  Json parse() { auto result = value(0); whitespace(); if (at_ != text_.size()) invalid("data after JSON value"); return result; }
 private:
  std::string_view text_; std::size_t at_ = 0, nodes_ = 0;
  void whitespace() { while (at_ < text_.size() && (text_[at_] == ' ' || text_[at_] == '\t' || text_[at_] == '\r' || text_[at_] == '\n')) ++at_; }
  bool take(char c) { whitespace(); if (at_ < text_.size() && text_[at_] == c) { ++at_; return true; } return false; }
  std::string_view quoted() {
    whitespace(); const auto begin = at_;
    if (at_ == text_.size() || text_[at_++] != '"') invalid("expected JSON string");
    bool closed = false;
    while (at_ < text_.size()) {
      const auto c = text_[at_++];
      if (c == '"') { closed = true; break; }
      if (c == '\\') { if (at_ == text_.size()) invalid("short string escape"); ++at_; }
    }
    if (!closed) invalid("unterminated string");
    const auto token = text_.substr(begin, at_ - begin); (void)string_token(token); return token;
  }
  Json value(unsigned depth) {
    whitespace(); if (depth > 64) invalid("nesting exceeds 64 levels");
    if (++nodes_ > 1000000) invalid("request exceeds one million values");
    if (at_ == text_.size()) invalid("missing JSON value");
    const auto begin = at_; Json result{Json::Kind::null, {}, {}, {}};
    if (text_[at_] == '{') {
      result.kind = Json::Kind::object; ++at_;
      if (!take('}')) do {
        const auto key = string_token(quoted()); if (!take(':')) invalid("missing object colon");
        if (!result.fields.emplace(key, value(depth + 1)).second) invalid("duplicate object member");
        if (take('}')) break;
        if (!take(',')) invalid("missing object comma");
      } while (true);
    } else if (text_[at_] == '[') {
      result.kind = Json::Kind::array; ++at_;
      if (!take(']')) do {
        result.items.push_back(value(depth + 1)); if (take(']')) break;
        if (!take(',')) invalid("missing array comma");
      } while (true);
    } else if (text_[at_] == '"') { result.kind = Json::Kind::string; (void)quoted(); }
    else if (text_.substr(at_, 4) == "true" || text_.substr(at_, 5) == "false") {
      result.kind = Json::Kind::boolean; at_ += text_[at_] == 't' ? 4 : 5;
    } else if (text_.substr(at_, 4) == "null") { at_ += 4; }
    else {
      result.kind = Json::Kind::number;
      if (text_[at_] == '-') ++at_;
      if (at_ == text_.size()) invalid("short number");
      if (text_[at_] == '0') ++at_;
      else { if (text_[at_] < '1' || text_[at_] > '9') invalid("invalid JSON value"); while (at_ < text_.size() && text_[at_] >= '0' && text_[at_] <= '9') ++at_; }
      if (at_ < text_.size() && text_[at_] == '.') {
        const auto digits = ++at_; while (at_ < text_.size() && text_[at_] >= '0' && text_[at_] <= '9') ++at_;
        if (digits == at_) invalid("missing fractional digits");
      }
      if (at_ < text_.size() && (text_[at_] == 'e' || text_[at_] == 'E')) {
        ++at_; if (at_ < text_.size() && (text_[at_] == '+' || text_[at_] == '-')) ++at_;
        const auto digits = at_; while (at_ < text_.size() && text_[at_] >= '0' && text_[at_] <= '9') ++at_;
        if (digits == at_) invalid("missing exponent digits");
      }
    }
    result.raw = text_.substr(begin, at_ - begin); return result;
  }
};
const Json* field(const Json& value, std::string_view name) {
  const auto found = value.fields.find(std::string(name)); return found == value.fields.end() ? nullptr : &found->second;
}
template<class T> T integer(const Json& value) {
  if (value.kind != Json::Kind::number || value.raw.empty()) invalid("expected an integer JSON token");
  auto digits = value.raw;
  if constexpr (std::is_signed_v<T>) { if (digits.front() == '-') digits.remove_prefix(1); }
  if (digits.empty() || !std::all_of(digits.begin(), digits.end(), [](char c) { return c >= '0' && c <= '9'; })) invalid("expected an integer JSON token");
  T result{}; const auto parsed = std::from_chars(value.raw.data(), value.raw.data() + value.raw.size(), result);
  if (parsed.ec != std::errc{} || parsed.ptr != value.raw.data() + value.raw.size()) invalid("integer exceeds native storage range");
  return result;
}

void object(const Json& value) { if(value.kind!=Json::Kind::object) invalid("expected object"); }
void allowed(const Json& value,std::initializer_list<std::string_view> names) {
  object(value);for(const auto& [name,item]:value.fields){(void)item;if(std::find(names.begin(),names.end(),name)==names.end())invalid("unknown field "+name);}
}
const Json& required(const Json& value,std::string_view name){object(value);const auto* item=field(value,name);if(!item)invalid("missing field "+std::string(name));return *item;}
const std::vector<Json>& array(const Json& value){if(value.kind!=Json::Kind::array)invalid("expected array");return value.items;}
std::string native_string(const Json& value){return string_token(value.raw);}
bool boolean(const Json& value){if(value.kind!=Json::Kind::boolean)invalid("expected boolean");return value.raw=="true";}

template<class T>struct Vector:std::false_type{};
template<class T>struct Vector<std::vector<T>>:std::true_type{using Item=T;};
template<class T>struct Optional:std::false_type{};
template<class T>struct Optional<std::optional<T>>:std::true_type{};
template<class T>struct Array:std::false_type{};
template<class T,std::size_t N>struct Array<std::array<T,N>>:std::true_type{};
template<class T,class M>struct Member{std::string_view name;M T::*pointer;};
template<class T,class M>auto member(std::string_view name,M T::*pointer){return Member<T,M>{name,pointer};}
template<class T>struct Record;
#define F(name) member(#name,&V::name)
#define RECORD(Type,...) template<>struct Record<Type>{static auto members(){using V=Type;return std::make_tuple(__VA_ARGS__);}};
RECORD(RootCauseOptions,F(string_attributes),F(numeric_attributes),F(enable_multiplier))
RECORD(RootCauseRepresentation,F(feature_names),F(data),F(target),F(source_trace_indices))
RECORD(RootCauseTreeNode,F(feature_index),F(threshold),F(left),F(right),F(class_counts),F(missing_go_to_left),F(missing_only))
RECORD(RootCauseModel,F(feature_names),F(classes),F(data),F(target),F(source_trace_indices),F(tree),F(fit_count),F(problem_count))
struct MultiplierArguments{bool enable_multiplier=false;};
struct RepresentationArguments{RootCauseOptions options;};
struct FitArguments{RootCauseOptions options;std::string problem_class="underfed";};
RECORD(MultiplierArguments,F(enable_multiplier))
RECORD(RepresentationArguments,F(options))
RECORD(FitArguments,F(options),F(problem_class))
#undef RECORD
#undef F

template<class T>T read_record(const Json& value,std::set<std::string>* supplied=nullptr);
template<class T>T read_value(const Json& value){
  if constexpr(std::is_same_v<T,bool>)return boolean(value);
  else if constexpr(std::is_integral_v<T>)return integer<T>(value);
  else if constexpr(std::is_same_v<T,std::string>)return native_string(value);
  else if constexpr(std::is_same_v<T,std::vector<std::string>>)return parse_literal_string_vector_json(value.raw);
  else return read_record<T>(value);
}
template<class T>T read_record(const Json& value,std::set<std::string>* supplied){
  object(value);T result{};std::set<std::string> names;
  std::apply([&](const auto&... entry){(names.insert(std::string(entry.name)),...);},Record<T>::members());
  for(const auto& [name,item]:value.fields){(void)item;if(!names.contains(name))invalid("unknown field "+name);}
  std::apply([&](const auto&... entry){([&]{if(const auto* item=field(value,entry.name)){using M=std::remove_cvref_t<decltype(result.*(entry.pointer))>;result.*(entry.pointer)=read_value<M>(*item);}}(),...);},Record<T>::members());
  if(supplied){std::set<std::string> actual;for(const auto& [name,item]:value.fields){(void)item;actual.insert(name);}*supplied=std::move(actual);}return result;
}
template<class T>void write(std::ostream& out,const T& value){
  if constexpr(std::is_same_v<T,bool>)out<<(value?"true":"false");
  else if constexpr(std::is_integral_v<T>)out<<value;
  else if constexpr(std::is_same_v<T,std::string>)out<<q(value);
  else if constexpr(std::is_same_v<T,double>){const auto text=serialize_embedding_workbench_vector_json({value});out.write(text.data()+1,static_cast<std::streamsize>(text.size()-2));}
  else if constexpr(Optional<T>::value){if(value)write(out,*value);else out<<"null";}
  else if constexpr(std::is_same_v<T,std::vector<double>>)out<<serialize_embedding_workbench_vector_json(value);
  else if constexpr(Vector<T>::value||Array<T>::value){out<<'[';bool first=true;for(const auto& item:value){if(!first)out<<',';first=false;write(out,item);}out<<']';}
  else{out<<'{';bool first=true;std::apply([&](const auto&... entry){([&]{if(!first)out<<',';first=false;out<<q(entry.name)<<':';write(out,value.*(entry.pointer));}(),...);},Record<T>::members());out<<'}';}
}
template<class T>std::string render(const T& value){auto out=output();write(out,value);return out.str();}
std::string typed(std::string_view type,std::string_view value){return "{\"type\":"+q(type)+",\"value\":"+std::string(value)+'}';}

Event event_value(const Json& value){
  auto holder=parse_embedding_workbench_raw_log_json(value.raw);
  if(holder.traces.size()!=1||holder.traces.front().events.size()!=1)invalid("Event holder requires exactly one trace and one event");
  return std::move(holder.traces.front().events.front());
}
NativeAttributeValue scalar_value(const Json& value){
  auto holder=parse_embedding_workbench_raw_log_json(value.raw);
  if(holder.traces.size()!=1||holder.traces.front().events.size()!=1||holder.traces.front().events.front().attributes.size()!=1)invalid("typed_value holder requires exactly one trace, event and attribute");
  return std::move(holder.traces.front().events.front().attributes.front().typed_value);
}
std::string event_json(const Event& value){EventLog holder;Trace trace;trace.events.push_back(value);holder.traces.push_back(std::move(trace));return serialize_embedding_workbench_raw_log_json(holder);}
std::string scalar_json(const NativeAttributeValue& value){Event event;EventAttribute attribute;attribute.typed_value=value;event.attributes.push_back(std::move(attribute));return event_json(event);}

struct ValueRecord{std::size_t id;std::shared_ptr<RootCauseSharedValue> value;};
struct ContextRecord{std::size_t id;RootCauseSharedContext value;};
struct Arena{
  std::vector<ValueRecord> values;std::vector<ContextRecord> contexts;
  std::map<std::size_t,std::shared_ptr<RootCauseSharedValue>> by_value;
  std::map<std::size_t,std::size_t> by_context;
  // The arena exclusively constructs these objects. Break owned cycles only
  // after the response and all snapshots have been captured (also on unwind).
  ~Arena(){for(auto& item:values)item.value->children.clear();}
  const RootCauseSharedContext& context(std::size_t id)const{const auto found=by_context.find(id);if(found==by_context.end())invalid("unknown context ID");return contexts[found->second].value;}
};
RootCauseSharedBinding binding_value(const Json& value,const Arena& arena){
  allowed(value,{"key","value"});RootCauseSharedBinding result;
  if(const auto* item=field(value,"key"))result.key=native_string(*item);
  if(const auto* item=field(value,"value");item&&item->kind!=Json::Kind::null){const auto found=arena.by_value.find(integer<std::size_t>(*item));if(found==arena.by_value.end())invalid("unknown value ID");result.value=found->second;}return result;
}
std::vector<RootCauseSharedBinding> bindings_value(const Json& value,const Arena& arena){std::vector<RootCauseSharedBinding> result;for(const auto& item:array(value))result.push_back(binding_value(item,arena));return result;}
std::unique_ptr<Arena> graph_value(const Json* value){
  auto result=std::make_unique<Arena>();if(!value)return result;allowed(*value,{"values","contexts"});
  const auto* values=field(*value,"values");
  if(values)for(const auto& item:array(*values)){
    allowed(item,{"id","value","kind_index","null","typed_value","container_index","children","first_class_role_index","first_class_state_index","first_class_source_key"});
    const auto id=integer<std::size_t>(required(item,"id"));auto native=std::make_shared<RootCauseSharedValue>();
    if(!result->by_value.emplace(id,native).second)invalid("duplicate value ID");result->values.push_back({id,std::move(native)});
  }
  if(values)for(std::size_t index=0;index<values->items.size();++index){
    const auto& item=values->items[index];auto& native=*result->values[index].value;
    if(const auto* v=field(item,"value"))native.value=native_string(*v);
    if(const auto* v=field(item,"kind_index"))native.kind=static_cast<AttributeKind>(integer<int>(*v));
    if(const auto* v=field(item,"null"))native.is_null=boolean(*v);
    if(const auto* v=field(item,"typed_value"))native.typed_value=scalar_value(*v);
    if(const auto* v=field(item,"container_index"))native.container=static_cast<AttributeContainerKind>(integer<int>(*v));
    if(const auto* v=field(item,"children"))native.children=bindings_value(*v,*result);
    if(const auto* v=field(item,"first_class_role_index"))native.first_class_role=static_cast<FirstClassAttributeRole>(integer<int>(*v));
    if(const auto* v=field(item,"first_class_state_index"))native.first_class_state=static_cast<FirstClassAttributeState>(integer<int>(*v));
    if(const auto* v=field(item,"first_class_source_key"))native.first_class_source_key=native_string(*v);
  }
  if(const auto* contexts=field(*value,"contexts"))for(const auto& item:array(*contexts)){
    allowed(item,{"id","prototype","attributes","activity_attribute","timestamp_attribute","resource_attribute","lifecycle_attribute"});
    const auto id=integer<std::size_t>(required(item,"id"));if(!result->by_context.emplace(id,result->contexts.size()).second)invalid("duplicate context ID");
    RootCauseSharedContext native;if(const auto* v=field(item,"prototype"))native.prototype=event_value(*v);
    if(const auto* v=field(item,"attributes"))native.attributes=bindings_value(*v,*result);
    const auto optional_binding=[&](std::string_view name,auto& target){if(const auto* v=field(item,name);v&&v->kind!=Json::Kind::null)target=binding_value(*v,*result);};
    optional_binding("activity_attribute",native.activity_attribute);optional_binding("timestamp_attribute",native.timestamp_attribute);optional_binding("resource_attribute",native.resource_attribute);optional_binding("lifecycle_attribute",native.lifecycle_attribute);
    result->contexts.push_back({id,std::move(native)});
  }return result;
}

class GraphWriter{
 public:
  explicit GraphWriter(const Arena* source){if(source){for(const auto& item:source->values){values_.push_back(item);ids_.emplace(item.value.get(),item.id);used_values_.insert(item.id);}for(const auto& item:source->contexts){contexts_.push_back({item.id,&item.value});used_contexts_.insert(item.id);}}}
  std::size_t append(const RootCauseSharedContext& value){const auto id=next(used_contexts_);contexts_.push_back({id,&value});return id;}
  std::string json(){
    // Iterative discovery handles long and cyclic finite graphs without
    // recursively expanding their reference structure or inventing IDs.
    for(const auto& item:contexts_){for(const auto& binding:item.second->attributes)remember(binding.value);for(const auto* optional:{&item.second->activity_attribute,&item.second->timestamp_attribute,&item.second->resource_attribute,&item.second->lifecycle_attribute})if(*optional)remember((*optional)->value);}
    for(std::size_t i=0;i<values_.size();++i){const auto pointer=values_[i].value;for(const auto& child:pointer->children)remember(child.value);}
    std::string out="{\"values\":[";bool first=true;
    for(const auto& item:values_){if(!first)out+=',';first=false;const auto& v=*item.value;out+="{\"id\":"+std::to_string(item.id)+",\"value\":"+q(v.value)+",\"kind_index\":"+std::to_string(static_cast<int>(v.kind))+",\"null\":"+(v.is_null?"true":"false")+",\"typed_value\":"+scalar_json(v.typed_value)+",\"container_index\":"+std::to_string(static_cast<int>(v.container))+",\"children\":"+bindings(v.children)+",\"first_class_role_index\":"+std::to_string(static_cast<int>(v.first_class_role))+",\"first_class_state_index\":"+std::to_string(static_cast<int>(v.first_class_state))+",\"first_class_source_key\":"+q(v.first_class_source_key)+'}';}
    out+="],\"contexts\":[";first=true;for(const auto& item:contexts_){if(!first)out+=',';first=false;const auto& v=*item.second;out+="{\"id\":"+std::to_string(item.first)+",\"prototype\":"+event_json(v.prototype)+",\"attributes\":"+bindings(v.attributes)+",\"activity_attribute\":"+optional(v.activity_attribute)+",\"timestamp_attribute\":"+optional(v.timestamp_attribute)+",\"resource_attribute\":"+optional(v.resource_attribute)+",\"lifecycle_attribute\":"+optional(v.lifecycle_attribute)+'}';}return out+"]}";
  }
 private:
  std::vector<ValueRecord> values_;std::vector<std::pair<std::size_t,const RootCauseSharedContext*>> contexts_;
  std::map<const RootCauseSharedValue*,std::size_t> ids_;std::set<std::size_t> used_values_,used_contexts_;
  static std::size_t next(std::set<std::size_t>& used){std::size_t id=0;while(used.contains(id)){if(id==std::numeric_limits<std::size_t>::max())invalid("graph ID space exhausted");++id;}used.insert(id);return id;}
  void remember(const std::shared_ptr<RootCauseSharedValue>& value){if(value&&!ids_.contains(value.get())){const auto id=next(used_values_);ids_.emplace(value.get(),id);values_.push_back({id,value});}}
  std::string binding(const RootCauseSharedBinding& value)const{return "{\"key\":"+q(value.key)+",\"value\":"+(value.value?std::to_string(ids_.at(value.value.get())):"null")+'}';}
  std::string bindings(const std::vector<RootCauseSharedBinding>& values)const{std::string out="[";for(const auto& item:values){if(out.size()>1)out+=',';out+=binding(item);}return out+']';}
  std::string optional(const std::optional<RootCauseSharedBinding>& value)const{return value?binding(*value):"null";}
};

struct Spec{const char* operation;const char* call;const char* input;int arguments;const char* result;};
constexpr Spec specs[]={
 {"make_shared_root_cause_context","make_shared_root_cause_context(event)","Event",0,"RootCauseSharedContext"},
 {"materialize_root_cause_context","materialize_root_cause_context(context)","RootCauseSharedContext",0,"Event"},
 {"build_shared_root_cause_log","build_shared_root_cause_log(fit, problem, enable_multiplier)","RootCauseSharedCohorts",1,"RootCauseSharedLog"},
 {"materialize_root_cause_log","materialize_root_cause_log(log)","RootCauseSharedLog",0,"EventLog"},
 {"build_shared_root_cause_representation","build_shared_root_cause_representation(fit, problem, options)","RootCauseSharedCohorts",2,"RootCauseRepresentation"},
 {"build_root_cause_log","build_root_cause_log(fit, problem, enable_multiplier)","RootCauseCohorts",1,"EventLog"},
 {"build_root_cause_representation","build_root_cause_representation(fit, problem, options)","RootCauseCohorts",2,"RootCauseRepresentation"},
 {"fit_root_cause_model","fit_root_cause_model(fit, problem, options, problem_class)","RootCauseCohorts",3,"std::optional<RootCauseModel>"}};
using Arguments=std::variant<std::monostate,MultiplierArguments,RepresentationArguments,FitArguments>;
using Result=std::variant<std::monostate,RootCauseSharedContext,Event,RootCauseSharedLog,EventLog,RootCauseRepresentation,std::optional<RootCauseModel>>;
struct Prepared{
 std::string type;Event event;std::vector<RootCauseObservation> fit,problem;std::unique_ptr<Arena> arena;
 std::vector<RootCauseSharedObservation> shared_fit,shared_problem;std::vector<std::pair<std::size_t,std::size_t>> fit_refs,problem_refs;
 std::size_t context_id=0;RootCauseSharedLog shared_log;std::vector<std::size_t> log_refs;
};
std::vector<RootCauseObservation> observations(const Json* value){std::vector<RootCauseObservation> out;if(value)for(const auto& item:array(*value)){allowed(item,{"trace_index","context"});RootCauseObservation v;if(const auto* p=field(item,"trace_index"))v.trace_index=integer<std::size_t>(*p);if(const auto* p=field(item,"context"))v.context=event_value(*p);out.push_back(std::move(v));}return out;}
Prepared prepare(const Json& value,const Spec& spec){
 allowed(value,{"type","value"});if(native_string(required(value,"type"))!=spec.input)invalid("input type does not match selected public function");
 Prepared out;out.type=spec.input;const auto& body=required(value,"value");
 if(out.type=="Event")out.event=event_value(body);
 else if(out.type=="RootCauseCohorts"){allowed(body,{"fit","problem"});out.fit=observations(field(body,"fit"));out.problem=observations(field(body,"problem"));}
 else{
  if(out.type=="RootCauseSharedContext")allowed(body,{"graph","context"});
  else if(out.type=="RootCauseSharedCohorts")allowed(body,{"graph","fit","problem"});
  else allowed(body,{"graph","contexts"});
  out.arena=graph_value(field(body,"graph"));
  if(out.type=="RootCauseSharedContext"){out.context_id=integer<std::size_t>(required(body,"context"));(void)out.arena->context(out.context_id);}
  else if(out.type=="RootCauseSharedCohorts"){
   const auto read=[&](std::string_view name,auto& target,auto& refs){if(const auto* list=field(body,name))for(const auto& item:array(*list)){allowed(item,{"trace_index","context"});const auto* trace=field(item,"trace_index");const auto index=trace?integer<std::size_t>(*trace):0;const auto id=integer<std::size_t>(required(item,"context"));target.push_back(RootCauseSharedObservation{index,out.arena->context(id)});refs.push_back(std::make_pair(index,id));}};
   read("fit",out.shared_fit,out.fit_refs);read("problem",out.shared_problem,out.problem_refs);
  }else if(const auto* list=field(body,"contexts"))for(const auto& item:array(*list)){const auto id=integer<std::size_t>(item);out.shared_log.contexts.push_back(out.arena->context(id));out.log_refs.push_back(id);}
 }return out;
}
std::string observations_json(const std::vector<RootCauseObservation>& values){std::string out="[";for(const auto& item:values){if(out.size()>1)out+=',';out+="{\"trace_index\":"+std::to_string(item.trace_index)+",\"context\":"+event_json(item.context)+'}';}return out+']';}
std::string refs_json(const std::vector<std::pair<std::size_t,std::size_t>>& values){std::string out="[";for(const auto& [trace,context]:values){if(out.size()>1)out+=',';out+="{\"trace_index\":"+std::to_string(trace)+",\"context\":"+std::to_string(context)+'}';}return out+']';}
std::string input_json(const Prepared& value){
 std::string body;if(value.type=="Event")body=event_json(value.event);
 else if(value.type=="RootCauseCohorts")body="{\"fit\":"+observations_json(value.fit)+",\"problem\":"+observations_json(value.problem)+'}';
 else{GraphWriter writer(value.arena.get());body="{\"graph\":"+writer.json();if(value.type=="RootCauseSharedContext")body+=",\"context\":"+std::to_string(value.context_id);else if(value.type=="RootCauseSharedCohorts")body+=",\"fit\":"+refs_json(value.fit_refs)+",\"problem\":"+refs_json(value.problem_refs);else body+=",\"contexts\":"+render(value.log_refs);body+='}';}return typed(value.type,body);
}
std::string arguments_json(const Arguments& value){return std::visit([](const auto& v)->std::string{if constexpr(std::is_same_v<std::decay_t<decltype(v)>,std::monostate>)return "null";else return render(v);},value);}
std::string result_json(const Result& value,const Prepared& input,const Spec& spec,std::optional<std::string>& raw_log){
 const auto body=std::visit([&](const auto& v)->std::string{using T=std::decay_t<decltype(v)>;
  if constexpr(std::is_same_v<T,std::monostate>)return "null";
  else if constexpr(std::is_same_v<T,Event>)return event_json(v);
  else if constexpr(std::is_same_v<T,EventLog>){raw_log=serialize_embedding_workbench_raw_log_json(v);return *raw_log;}
  else if constexpr(std::is_same_v<T,RootCauseSharedContext>){GraphWriter writer(input.arena.get());const auto id=writer.append(v);return "{\"graph\":"+writer.json()+",\"context\":"+std::to_string(id)+'}';}
  else if constexpr(std::is_same_v<T,RootCauseSharedLog>){GraphWriter writer(input.arena.get());std::vector<std::size_t> refs;for(const auto& context:v.contexts)refs.push_back(writer.append(context));return "{\"graph\":"+writer.json()+",\"contexts\":"+render(refs)+'}';}
  else return render(v);
 },value);return typed(spec.result,body);
}
std::string hex(std::string_view value){constexpr char digits[]="0123456789abcdef";std::string out;for(unsigned char c:value){out+=digits[c>>4];out+=digits[c&15];}return out;}
std::string carrier(std::string_view dataset,std::string_view bytes){const bool utf8=q(bytes).starts_with('"');return "{\"dataset\":"+q(dataset)+",\"encoding\":"+q(utf8?"utf8":"hex")+",\"data\":"+q(utf8?bytes:std::string_view(hex(bytes)))+",\"bytes\":"+std::to_string(bytes.size())+",\"sha256\":"+q(footprints_workbench_sha256(bytes))+'}';}
std::string exception_type(const std::exception& error){if(dynamic_cast<const std::invalid_argument*>(&error))return "invalid_argument";if(dynamic_cast<const std::domain_error*>(&error))return "domain_error";if(dynamic_cast<const std::out_of_range*>(&error))return "out_of_range";if(dynamic_cast<const std::overflow_error*>(&error))return "overflow_error";if(dynamic_cast<const std::length_error*>(&error))return "length_error";if(dynamic_cast<const std::runtime_error*>(&error))return "runtime_error";return "exception";}
std::string error_json(std::string_view phase,const std::exception& error){return "{\"phase\":"+q(phase)+",\"exception_type\":"+q(exception_type(error))+",\"message\":"+q(error.what())+'}';}
std::string strings(const std::set<std::string>& names){return render(std::vector<std::string>(names.begin(),names.end()));}

RootCauseWorkbenchResponse execute(const std::vector<DiscoveryWorkbenchPart>& parts,const std::vector<DiscoveryWorkbenchPart>& query,const std::optional<std::string>& transport_error=std::nullopt){
 std::map<std::string,std::vector<const DiscoveryWorkbenchPart*>> by_name;std::string originals="[",controls="[";
 const auto add=[&](const DiscoveryWorkbenchPart& part,bool query_part){by_name[part.name].push_back(&part);if(!query_part){if(originals.size()>1)originals+=',';originals+="{\"field\":"+q(part.name)+",\"filename\":"+(part.file?q(part.filename):"null")+",\"file\":"+(part.file?"true":"false")+",\"original\":"+carrier(part.file?part.filename:part.name,part.bytes)+'}';}if(query_part||part.name!="input_file"){if(controls.size()>1)controls+=',';controls+="{\"name\":"+q(part.name)+",\"origin\":"+q(query_part?"query":"multipart")+'}';}};
 for(const auto& part:query)add(part,true);for(const auto& part:parts)add(part,false);originals+=']';controls+=']';
 const auto find=[&](std::string_view name)->const DiscoveryWorkbenchPart*{const auto it=by_name.find(std::string(name));return it==by_name.end()||it->second.size()!=1?nullptr:it->second.front();};
 const auto control=[&](std::string_view name,std::string_view fallback){const auto* part=find(name);return part?part->bytes:std::string(fallback);};
 const auto operation=control("operation","build_root_cause_representation"),format=control("input_format","exact");
 const Spec* spec=nullptr;Arguments arguments;Result result;std::optional<Prepared> prepared;std::set<std::string> supplied_arguments,supplied_options;std::vector<std::string> parsed;
 std::string phase="configuration_error",failure="null",configured="null",before="null",after="null",result_holder="null",calls="[",artifact_errors="[";
 bool entered=false,returned=false;std::optional<bool> input_unchanged,arguments_unchanged;
 std::map<std::string,std::optional<std::string>> artifacts;for(const auto* key:{"source_input_json","after_input_json","configured_arguments_json","result_json","result_log_json","result_xes"})artifacts.emplace(key,std::nullopt);
 const auto call=[&](std::string_view name,std::string_view stage,std::string_view kind,const auto& function){if(calls.size()>1)calls+=',';const auto prefix="{\"name\":"+q(name)+",\"stage\":"+q(stage)+",\"input_kind\":"+q(kind);try{function();calls+=prefix+",\"returned\":true,\"error\":null}";}catch(const std::bad_alloc&){throw;}catch(const std::exception& error){calls+=prefix+",\"returned\":false,\"error\":"+error_json(stage=="export"?"export_error":"native_error",error)+'}';throw;}};
 const auto snapshot_after=[&]{if(!entered)return;after=input_json(*prepared);artifacts["after_input_json"]=after;input_unchanged=before==after;if(arguments.index())arguments_unchanged=configured==arguments_json(arguments);};
 try{
  if(transport_error)invalid(*transport_error);
  for(const auto& [name,values]:by_name){if(values.size()!=1)invalid("duplicate control or upload: "+name);if(name!="operation"&&name!="input_format"&&name!="arguments_json"&&name!="input_file")invalid("unknown control or upload: "+name);}
  for(const auto& part:query)if(part.name=="input_file")invalid("input upload cannot be a query control");
  for(const auto& item:specs)if(operation==item.operation)spec=&item;if(!spec)invalid("unknown public operation");parsed.push_back("operation");
  if(format!="exact")invalid("input_format must be exact");parsed.push_back("input_format");
  const auto* submitted=find("arguments_json");if(!spec->arguments&&submitted)invalid("this public function has no arguments object");
  if(spec->arguments){const auto text=submitted?submitted->bytes:"{}";const auto document=Parser(text).parse();
   if(spec->arguments==1)arguments=read_record<MultiplierArguments>(document,&supplied_arguments);
   else if(spec->arguments==2)arguments=read_record<RepresentationArguments>(document,&supplied_arguments);
   else arguments=read_record<FitArguments>(document,&supplied_arguments);
   if(const auto* options=field(document,"options"))for(const auto& [name,value]:options->fields){(void)value;supplied_options.insert(name);}
   configured=arguments_json(arguments);artifacts["configured_arguments_json"]=configured;if(submitted)parsed.push_back("arguments_json");
  }
  phase="preparation_error";const auto* file=find("input_file");if(!file)invalid("input_file is required");
  prepared=prepare(Parser(file->bytes).parse(),*spec);before=input_json(*prepared);artifacts["source_input_json"]=before;
  phase="native_error";
  try{call(spec->call,"primary",spec->input,[&]{entered=true;
   if(operation=="make_shared_root_cause_context")result=make_shared_root_cause_context(prepared->event);
   else if(operation=="materialize_root_cause_context")result=materialize_root_cause_context(prepared->arena->context(prepared->context_id));
   else if(operation=="build_shared_root_cause_log")result=build_shared_root_cause_log(prepared->shared_fit,prepared->shared_problem,std::get<MultiplierArguments>(arguments).enable_multiplier);
   else if(operation=="materialize_root_cause_log")result=materialize_root_cause_log(prepared->shared_log);
   else if(operation=="build_shared_root_cause_representation")result=build_shared_root_cause_representation(prepared->shared_fit,prepared->shared_problem,std::get<RepresentationArguments>(arguments).options);
   else if(operation=="build_root_cause_log")result=build_root_cause_log(prepared->fit,prepared->problem,std::get<MultiplierArguments>(arguments).enable_multiplier);
   else if(operation=="build_root_cause_representation")result=build_root_cause_representation(prepared->fit,prepared->problem,std::get<RepresentationArguments>(arguments).options);
   else{const auto& config=std::get<FitArguments>(arguments);result=fit_root_cause_model(prepared->fit,prepared->problem,config.options,config.problem_class);}returned=true;});}
  catch(...){phase="result_serialization_error";snapshot_after();phase="native_error";throw;}
  phase="result_serialization_error";snapshot_after();
  std::optional<std::string> raw_log;result_holder=result_json(result,*prepared,*spec,raw_log);artifacts["result_json"]=result_holder;
  if(const auto* log=std::get_if<EventLog>(&result)){
   artifacts["result_log_json"]=std::move(raw_log);
   try{call("serialize_xes(result)","export","EventLog",[&]{artifacts["result_xes"]=serialize_xes(*log);});}
   catch(const std::bad_alloc&){throw;}catch(const std::exception& error){if(artifact_errors.size()>1)artifact_errors+=',';artifact_errors+="{\"name\":\"result_xes\",\"error\":"+error_json("export_error",error)+'}';}
  }
 }catch(const std::bad_alloc&){throw;}catch(const std::exception& error){failure=error_json(phase,error);}
 calls+=']';artifact_errors+=']';std::string artifact_map="{";for(const auto& [name,bytes]:artifacts){if(artifact_map.size()>1)artifact_map+=',';artifact_map+=q(name)+':'+(bytes?carrier(name=="result_xes"?"result.xes":name+".json",*bytes):"null");}artifact_map+='}';
 const auto maybe=[](const std::optional<bool>& value){return value?(*value?"true":"false"):"null";};const auto* submitted=find("arguments_json");
 RootCauseWorkbenchResponse response;response.status=failure=="null"?200:phase=="result_serialization_error"?500:400;
 response.json="{\"family\":\"ORIGINAL_ROOT_CAUSE_WORKBENCH\",\"version\":\"original-root-cause-workbench-v1\",\"operation\":"+q(operation)+",\"executed\":"+(returned?"true":"false")+",\"native_entered\":"+(entered?"true":"false")+",\"native_returned\":"+(returned?"true":"false")+",\"native_calls\":"+calls+",\"error\":"+failure+",\"originals\":"+originals+",\"parsed_controls\":"+render(parsed)+",\"supplied_controls\":"+controls
  +",\"input_context\":{\"size_t_bytes\":"+std::to_string(sizeof(std::size_t))+",\"int_bytes\":"+std::to_string(sizeof(int))+",\"input_format\":"+q(format)+",\"input_type\":"+(prepared?q(prepared->type):"null")+",\"graph_values\":"+(prepared&&prepared->arena?std::to_string(prepared->arena->values.size()):"null")+",\"graph_contexts\":"+(prepared&&prepared->arena?std::to_string(prepared->arena->contexts.size()):"null")+",\"input_unchanged\":"+maybe(input_unchanged)+",\"arguments_unchanged\":"+maybe(arguments_unchanged)+'}'
  +",\"input\":{\"before\":"+before+",\"after\":"+after+"},\"submitted_arguments\":"+(submitted?carrier(submitted->file?submitted->filename:submitted->name,submitted->bytes):"null")+",\"configured_arguments\":"+configured+",\"supplied_arguments\":"+strings(supplied_arguments)+",\"supplied_options\":"+strings(supplied_options)+",\"result\":"+result_holder+",\"artifact_bytes\":"+artifact_map+",\"artifact_errors\":"+artifact_errors+'}';return response;
}
} // namespace

RootCauseOptions parse_root_cause_workbench_options_json(std::string_view value,std::set<std::string>* supplied){return read_record<RootCauseOptions>(Parser(value).parse(),supplied);}
std::string serialize_root_cause_workbench_options_json(const RootCauseOptions& value){return render(value);}
std::string serialize_root_cause_workbench_result_json(const RootCauseRepresentation& value){return render(value);}
std::string serialize_root_cause_workbench_result_json(const RootCauseModel& value){return render(value);}
std::string serialize_root_cause_workbench_result_json(const std::optional<RootCauseModel>& value){return render(value);}
RootCauseWorkbenchResponse execute_root_cause_workbench(const std::vector<DiscoveryWorkbenchPart>& parts,const std::vector<DiscoveryWorkbenchPart>& query){return execute(parts,query);}
RootCauseWorkbenchResponse execute_root_cause_workbench_multipart(std::string_view type,std::string_view body,const std::vector<DiscoveryWorkbenchPart>& query,bool complete){if(!complete)return execute({},query,"multipart transport did not complete");std::vector<DiscoveryWorkbenchPart> parts;try{parts=parse_discovery_workbench_multipart(type,body);}catch(const std::bad_alloc&){throw;}catch(const std::exception& error){return execute({},query,error.what());}return execute(parts,query);}
} // namespace minera
