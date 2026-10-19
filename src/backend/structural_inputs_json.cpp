#include "minera/structural_inputs_json.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/footprints_workbench_json.hpp"
#include "minera/literal_controls_json.hpp"
#include "minera/object_centric_playout_workbench_json.hpp"
#include "minera/trie_conversion.hpp"
#include "minera/json_export.hpp"
#include "minera/pnml.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <exception>
#include <limits>
#include <locale>
#include <map>
#include <new>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
namespace minera { namespace {
[[noreturn]] void invalid(std::string_view message){throw std::invalid_argument("saved structures JSON: "+std::string(message));}
std::string q(std::string_view value){return serialize_embedding_workbench_string_json(value);}
std::ostringstream output(){std::ostringstream out;out.imbue(std::locale::classic());return out;}
// A bounded structural index into submitted JSON. Native string/float parsing
// delegates to the existing public codecs; this index never parses output.
struct Json {
  enum class Kind { object, array, string, number, boolean, null } kind;
  std::string_view raw;
  std::map<std::string, Json> fields;
  std::vector<Json> items;
  std::vector<std::string> order;
};
std::string string_token(std::string_view raw) {
  auto values = parse_literal_string_vector_json("[" + std::string(raw) + "]");
  if (values.size() != 1) invalid("expected one JSON string");
  return std::move(values.front());
}
class Parser {
 public:
  explicit Parser(std::string_view text) : text_(text) {

  }
  Json parse() { auto result = value(0); whitespace(); if (at_ != text_.size()) invalid("data after JSON value"); return result; }
 private:
  std::string_view text_; std::size_t at_ = 0;
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
    whitespace();
    if (at_ == text_.size()) invalid("missing JSON value");
    const auto begin = at_; Json result{Json::Kind::null, {}, {}, {}};
    if (text_[at_] == '{') {
      result.kind = Json::Kind::object; ++at_;
      if (!take('}')) do {
        const auto key = string_token(quoted()); if (!take(':')) invalid("missing object colon");
        if (!result.fields.emplace(key, value(depth + 1)).second) invalid("duplicate object member");
        result.order.push_back(key);
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
std::string native_string(const Json& value){return string_token(value.raw);}
bool boolean(const Json& value){if(value.kind!=Json::Kind::boolean)invalid("expected boolean");return value.raw=="true";}




template<class T>struct Vector:std::false_type{};
template<class T>struct Vector<std::vector<T>>:std::true_type{using Item=T;};
template<class T>struct Optional:std::false_type{};
template<class T>struct Optional<std::optional<T>>:std::true_type{using Item=T;};
template<class T>struct Pair:std::false_type{};
template<class A,class B>struct Pair<std::pair<A,B>>:std::true_type{using First=A;using Second=B;};
template<class T,class M>struct Member{std::string_view name;M T::*pointer;};
template<class T,class M>auto member(std::string_view name,M T::*pointer){return Member<T,M>{name,pointer};}
template<class T>struct Record;
#define F(name) member(#name,&V::name)
#define RECORD(Type,...) template<>struct Record<Type>{static auto members(){using V=Type;return std::make_tuple(__VA_ARGS__);}};
RECORD(TrieNode,F(label),F(children),F(depth),F(final))
RECORD(TrieModel,F(root),F(input_traces),F(distinct_variants),F(node_count),F(truncated))
RECORD(OpenPathInterval,F(start),F(end),F(case_id),F(source_activity),F(target_activity),F(trace_index),F(source_event_index),F(target_event_index))
RECORD(IntervalTreeQueryOptions,F(point),F(range))
RECORD(IntervalTreeQueryResult,F(point),F(range),F(intervals),F(point_matches),F(range_matches),F(interval_count))
#undef RECORD
#undef F
template<class T>T read(const Json&);
template<class T>void write(std::ostream&,const T&);
template<class T>std::string render(const T& v){auto out=output();write(out,v);return out.str();}
template<class T>T read_record(const Json& v){
 object(v);T result{};const auto members=Record<T>::members();
 for(const auto& [name,item]:v.fields){(void)item;if(!std::apply([&](const auto&...m){return ((name==m.name)||...);},members))invalid("unknown field "+name);}
 std::apply([&](const auto&...m){([&]{if(const auto* item=field(v,m.name))result.*(m.pointer)=read<std::remove_cvref_t<decltype(result.*(m.pointer))>>(*item);}(),...);},members);return result;
}
template<class T>T read(const Json& v){
 if constexpr(std::is_same_v<T,bool>)return boolean(v);
 else if constexpr(std::is_integral_v<T>)return integer<T>(v);
 else if constexpr(std::is_same_v<T,std::string>)return native_string(v);
 else if constexpr(std::is_same_v<T,double>){const auto values=parse_embedding_workbench_vector_json("["+std::string(v.raw)+"]");if(values.size()!=1)invalid("expected one double");return values.front();}
 else if constexpr(Optional<T>::value){if(v.kind==Json::Kind::null)return std::nullopt;return read<typename Optional<T>::Item>(v);}
 else if constexpr(Pair<T>::value){if(v.kind!=Json::Kind::array||v.items.size()!=2)invalid("expected two-element pair");return {read<typename Pair<T>::First>(v.items[0]),read<typename Pair<T>::Second>(v.items[1])};}
 else if constexpr(Vector<T>::value){if(v.kind!=Json::Kind::array)invalid("expected array");T result;for(const auto& item:v.items)result.push_back(read<typename Vector<T>::Item>(item));return result;}
 else return read_record<T>(v);
}
template<class T>void write(std::ostream& out,const T& v){
 if constexpr(std::is_same_v<T,bool>)out<<(v?"true":"false");
 else if constexpr(std::is_integral_v<T>)out<<v;
 else if constexpr(std::is_same_v<T,std::string>)out<<q(v);
 else if constexpr(std::is_same_v<T,double>){const auto text=serialize_embedding_workbench_vector_json({v});out.write(text.data()+1,static_cast<std::streamsize>(text.size()-2));}
 else if constexpr(Optional<T>::value){if(v)write(out,*v);else out<<"null";}
 else if constexpr(Pair<T>::value){out<<'[';write(out,v.first);out<<',';write(out,v.second);out<<']';}
 else if constexpr(Vector<T>::value){out<<'[';bool first=true;for(const auto& item:v){if(!first)out<<',';first=false;write(out,item);}out<<']';}
 else{out<<'{';bool first=true;std::apply([&](const auto&...m){([&]{if(!first)out<<',';first=false;out<<q(m.name)<<':';write(out,v.*(m.pointer));}(),...);},Record<T>::members());out<<'}';}
}
std::string hex(std::string_view b){constexpr char chars[]="0123456789abcdef";std::string result;for(unsigned char c:b){result+=chars[c>>4];result+=chars[c&15];}return result;}
std::string carrier(std::string_view dataset,std::string_view bytes){const bool utf8=q(bytes).starts_with('"');return "{\"dataset\":"+q(dataset)+",\"bytes\":"+std::to_string(bytes.size())+",\"encoding\":"+q(utf8?"utf8":"hex-v1")+",\"data\":"+q(utf8?bytes:std::string_view(hex(bytes)))+",\"error\":null}";}
std::string exception_type(const std::exception& e){if(dynamic_cast<const std::invalid_argument*>(&e))return "invalid_argument";if(dynamic_cast<const std::out_of_range*>(&e))return "out_of_range";if(dynamic_cast<const std::domain_error*>(&e))return "domain_error";if(dynamic_cast<const std::overflow_error*>(&e))return "overflow_error";if(dynamic_cast<const std::length_error*>(&e))return "length_error";if(dynamic_cast<const std::runtime_error*>(&e))return "runtime_error";if(dynamic_cast<const std::bad_alloc*>(&e))return "bad_alloc";return "exception";}
std::string error_json(std::string_view phase,const std::exception& e){return "{\"phase\":"+q(phase)+",\"type\":"+q(exception_type(e))+",\"message\":"+q(e.what())+'}';}
std::string failed_artifact(std::string_view name,std::string_view error){return "{\"dataset\":"+q(name)+",\"bytes\":null,\"encoding\":null,\"data\":null,\"error\":"+std::string(error)+'}';}
bool family_is(const Json& v,std::string_view name){const auto* f=field(v,"family");return f&&native_string(*f)==name;}
struct Selection {const Json* value;std::vector<std::string> path;bool strip_family=false;};
Selection select_input(const Json& input,std::string_view format,bool trie){
 if(format=="direct")return {&input,{},false};
 const Json* selected=&input;std::vector<std::string> path;
 if(format=="legacy_report"){
  if(!family_is(*selected,"STRUCTURAL_REPORT"))invalid("expected STRUCTURAL_REPORT");
  selected=&required(*selected,"response");path.push_back("response");
 }
 if(format=="legacy_transformation"||format=="legacy_report"){
  if(!family_is(*selected,"STRUCTURAL_TRANSFORMATION"))invalid("expected STRUCTURAL_TRANSFORMATION");
  const auto& result=required(*selected,"result");
  if(trie&&family_is(result,"TRIE_CONVERSION")){selected=&required(*selected,"trie");path.push_back("trie");}
  else {selected=&result;path.push_back("result");}
  if(!family_is(*selected,trie?"LOG_TO_TRIE":"LOG_TO_INTERVAL_TREE"))invalid("incompatible structural result family");
  if(trie)return {selected,std::move(path),true};
  selected=&required(*selected,"intervals");path.push_back("intervals");return {selected,std::move(path),false};
 }
 if(format=="legacy_trie"){
  if(!trie||!family_is(input,"LOG_TO_TRIE"))invalid("expected compatible LOG_TO_TRIE");return {&input,{},true};
 }
 if(format=="legacy_interval"){
  if(trie||!family_is(input,"LOG_TO_INTERVAL_TREE"))invalid("expected compatible LOG_TO_INTERVAL_TREE");return {&required(input,"intervals"),{"intervals"},false};
 }
 if(format=="query_result"){
  if(trie)invalid("query_result is only an interval input");
  allowed(input,{"point","range","intervals","point_matches","range_matches","interval_count"});
  for(const auto key:{"point","range","intervals","point_matches","range_matches","interval_count"})(void)required(input,key);
  // Validate complete saved result storage, but only its interval vector is the new native argument.
  (void)read<IntervalTreeQueryResult>(input);
  const auto& complete_intervals=required(input,"intervals");
  for(const auto& interval:complete_intervals.items)for(const auto key:{"start","end","case_id","source_activity","target_activity","trace_index","source_event_index","target_event_index"})(void)required(interval,key);
  return {&required(input,"intervals"),{"intervals"},false};
 }
 invalid("unknown input_format");
}
constexpr std::array<std::string_view,5> artifact_names={"constructor_json","options_json","result_json","native_output_bytes","pnml_bytes"};
constexpr std::array<std::string_view,5> datasets={"constructor.json","options.json","result.json","native-output.bin","net.pnml"};
StructuralInputsResponse execute(const std::vector<DiscoveryWorkbenchPart>& parts,const std::vector<DiscoveryWorkbenchPart>& query,const std::optional<std::string>& transport_error=std::nullopt){
 std::map<std::string,std::vector<const DiscoveryWorkbenchPart*>> by_name;std::string originals="[",controls="[";
 const auto add=[&](const DiscoveryWorkbenchPart& p,bool from_query){by_name[p.name].push_back(&p);const bool file=!from_query&&p.file;if(originals.size()>1)originals+=',';originals+="{\"field\":"+q(p.name)+",\"filename\":"+(file?q(p.filename):"null")+",\"file\":"+(file?"true":"false")+",\"original\":"+carrier(file?p.filename:p.name,p.bytes)+'}';if(from_query||p.name!="input_file"){if(controls.size()>1)controls+=',';controls+="{\"name\":"+q(p.name)+",\"origin\":"+q(from_query?"query":"multipart")+'}';}};
 for(const auto& p:query)add(p,true);for(const auto& p:parts)add(p,false);originals+=']';controls+=']';
 const auto find=[&](std::string_view name)->const DiscoveryWorkbenchPart*{const auto it=by_name.find(std::string(name));return it==by_name.end()||it->second.size()!=1?nullptr:it->second.front();};
 const auto control=[&](std::string_view name,std::string_view fallback){const auto* p=find(name);return p?p->bytes:std::string(fallback);};
 const auto operation=control("operation","");const bool trie=operation=="trie_to_petri";const auto format=control("input_format","");const auto form=control("call_form",trie?"direct":find("options_json")?"with_options":"default_options");
 std::string phase="configuration",failure="null",before="null",after="null",selection="null",input_type="null",result="null",configured_options="null",option_before="null",option_after="null",calls="[";
 std::array<std::string,5> artifacts={"null","null","null","null","null"};std::vector<std::string> parsed;
 bool entered=false,returned=false,prepared=false,options_prepared=false;std::optional<bool> unchanged,options_unchanged;
 TrieModel model;std::vector<OpenPathInterval> intervals;IntervalTreeQueryOptions options;
 const auto bit=[](bool x){return x?"true":"false";};const auto maybe=[](std::optional<bool> x){return x?(*x?"true":"false"):"null";};
 const auto snapshot_after=[&]{if(!entered)return;after=trie?render(model):render(intervals);unchanged=after==before;if(options_prepared){option_after=render(options);if(form=="with_options")options_unchanged=option_after==option_before;}};
 const auto call=[&](std::string_view name,std::string_view input,auto&& fn){if(calls.size()>1)calls+=',';std::string error="null";bool did_return=false;try{fn();did_return=true;}catch(const std::exception& e){error=error_json(phase,e);calls+="{\"name\":"+q(name)+",\"input_type\":"+q(input)+",\"entered\":true,\"returned\":false,\"error\":"+error+'}';throw;}calls+="{\"name\":"+q(name)+",\"input_type\":"+q(input)+",\"entered\":true,\"returned\":"+bit(did_return)+",\"error\":"+error+'}';};
 const auto artifact=[&](std::size_t index,std::string_view name,std::string_view input,auto&& producer){try{std::string bytes;call(name,input,[&]{bytes=producer();});artifacts[index]=carrier(datasets[index],bytes);}catch(const std::exception& e){artifacts[index]=failed_artifact(datasets[index],error_json("artifact",e));}};
 try{
  if(transport_error)invalid(*transport_error);
  for(const auto& p:query)if(p.name=="input_file")invalid("input_file must be a multipart part");
  for(const auto& [name,values]:by_name){if(name!="operation"&&name!="input_format"&&name!="call_form"&&name!="options_json"&&name!="input_file")invalid("unknown part "+name);if(values.size()!=1)invalid("duplicate part "+name);}
  if(!find("operation")||(!trie&&operation!="query_intervals"))invalid("unknown or missing operation");parsed.push_back("operation");
  if(!find("input_format")||(format!="direct"&&format!="legacy_trie"&&format!="legacy_interval"&&format!="legacy_transformation"&&format!="legacy_report"&&format!="query_result"))invalid("unknown or missing input_format");parsed.push_back("input_format");
  if(trie){if(form!="direct")invalid("trie conversion requires direct call_form");if(find("options_json"))invalid("trie conversion does not accept options_json");}
  else if(form!="default_options"&&form!="with_options")invalid("unknown interval call_form");
  else if(form=="default_options"&&find("options_json"))invalid("default_options cannot accept options_json");
  parsed.push_back("call_form");
  if(!trie){if(const auto* p=find("options_json")){options=read<IntervalTreeQueryOptions>(Parser(p->bytes).parse());parsed.push_back("options_json");}options_prepared=true;option_before=render(options);artifacts[1]=carrier(datasets[1],option_before);}
  phase="input";const auto* source=find("input_file");if(!source)invalid("missing input_file");const auto root=Parser(source->bytes).parse();auto selected=select_input(root,format,trie);
  if(trie){if(selected.strip_family){Json record=*selected.value;record.fields.erase("family");model=read<TrieModel>(record);}else model=read<TrieModel>(*selected.value);input_type=q("TrieModel");}
  else {intervals=read<std::vector<OpenPathInterval>>(*selected.value);input_type=q("IntervalVector");}
  prepared=true;before=trie?render(model):render(intervals);selection=render(selected.path);artifacts[0]=carrier(datasets[0],before);parsed.push_back("input_file");
  phase="native";entered=true;
  if(trie){PetriNet net;call("convert_trie_to_petri_net","TrieModel",[&]{net=convert_trie_to_petri_net(model);returned=true;});snapshot_after();const auto full=serialize_oc_playout_workbench_petri_net_json(net);result="{\"type\":\"PetriNet\",\"value\":"+full+'}';artifacts[2]=carrier(datasets[2],full);phase="artifact";artifact(3,"serialize_trie_petri_net_json","PetriNet",[&]{return serialize_trie_petri_net_json(net);});artifact(4,"to_pnml","PetriNet",[&]{return to_pnml(net);});}
  else {IntervalTreeQueryResult value;call("query_open_path_intervals","IntervalVector",[&]{value=form=="default_options"?query_open_path_intervals(intervals):query_open_path_intervals(intervals,options);returned=true;});snapshot_after();const auto full=render(value);result="{\"type\":\"IntervalTreeQueryResult\",\"value\":"+full+'}';artifacts[2]=carrier(datasets[2],full);phase="artifact";artifact(3,"interval_tree_json","IntervalTreeQueryResult",[&]{return interval_tree_json(value);});}
 }catch(const std::exception& e){snapshot_after();failure=error_json(phase,e);}
 calls+=']';if(options_prepared)configured_options="{\"before\":"+option_before+",\"after\":"+option_after+'}';
 std::string amap="{";for(std::size_t i=0;i<artifact_names.size();++i){if(i)amap+=',';amap+=q(artifact_names[i])+':'+artifacts[i];}amap+='}';
 StructuralInputsResponse response;response.status=failure=="null"?200:returned?500:400;
 response.json="{\"family\":\"SAVED_STRUCTURAL_INPUTS\",\"operation\":"+q(operation)+",\"input_format\":"+q(format)+",\"call_form\":"+q(form)+",\"originals\":"+originals+",\"supplied_controls\":"+controls+",\"parsed_controls\":"+render(parsed)+",\"input\":{\"type\":"+input_type+",\"before\":"+before+",\"after\":"+after+",\"selection\":"+selection+"},\"configured_options\":"+configured_options+",\"result\":"+result+",\"native_entered\":"+bit(entered)+",\"native_returned\":"+bit(returned)+",\"input_unchanged\":"+maybe(unchanged)+",\"options_unchanged\":"+maybe(options_unchanged)+",\"native_calls\":"+calls+",\"artifacts\":"+amap+",\"error\":"+failure+'}';(void)prepared;return response;
}
} // namespace
TrieModel parse_structural_input_trie_json(std::string_view b){return read<TrieModel>(Parser(b).parse());}
std::vector<OpenPathInterval> parse_structural_input_intervals_json(std::string_view b){return read<std::vector<OpenPathInterval>>(Parser(b).parse());}
IntervalTreeQueryOptions parse_structural_input_query_options_json(std::string_view b){return read<IntervalTreeQueryOptions>(Parser(b).parse());}
std::string serialize_structural_input_json(const TrieModel& v){return render(v);}
std::string serialize_structural_input_json(const std::vector<OpenPathInterval>& v){return render(v);}
std::string serialize_structural_input_json(const IntervalTreeQueryOptions& v){return render(v);}
std::string serialize_structural_input_json(const IntervalTreeQueryResult& v){return render(v);}
StructuralInputsResponse execute_structural_inputs(const std::vector<DiscoveryWorkbenchPart>& p,const std::vector<DiscoveryWorkbenchPart>& qv){return execute(p,qv);}
StructuralInputsResponse execute_structural_inputs_multipart(std::string_view type,std::string_view body,const std::vector<DiscoveryWorkbenchPart>& query,bool complete){
 if(!complete)return execute({},query,"incomplete multipart transport");try{return execute(parse_discovery_workbench_multipart(type,body),query);}catch(const std::exception& e){return execute({},query,e.what());}
}
} // namespace minera
