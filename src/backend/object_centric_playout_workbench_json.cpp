#include "minera/object_centric_playout_workbench_json.hpp"
#include "minera/embedding_workbench_json.hpp"
#include <algorithm>
#include <array>
#include <optional>
#include <set>
#include <tuple>
#include <type_traits>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <initializer_list>
#include <limits>
#include <locale>
#include <map>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <variant>
namespace minera { namespace {
[[noreturn]] void invalid(std::string message) {
  throw std::invalid_argument("Object-centric playout workbench JSON: " + message);
}
struct Number { std::string text; };
struct Json {
  using Array = std::vector<Json>;
  using Object = std::map<std::string, Json, std::less<>>;
  std::variant<std::nullptr_t, bool, Number, std::string, Array, Object> value;
  bool null() const { return std::holds_alternative<std::nullptr_t>(value); }
  const Object& object() const {
    if (const auto* result = std::get_if<Object>(&value)) return *result;
    invalid("expected an object");
  }
  const Array& array() const {
    if (const auto* result = std::get_if<Array>(&value)) return *result;
    invalid("expected an array");
  }
  const std::string& string() const {
    if (const auto* result = std::get_if<std::string>(&value)) return *result;
    invalid("expected a string");
  }
  bool boolean() const {
    if (const auto* result = std::get_if<bool>(&value)) return *result;
    invalid("expected a boolean");
  }
};

void append_utf8(std::string& output, std::uint32_t value) {
  if (value <= 0x7f) output += static_cast<char>(value);
  else if (value <= 0x7ff) {
    output += static_cast<char>(0xc0 | (value >> 6));
    output += static_cast<char>(0x80 | (value & 63));
  } else if (value <= 0xffff) {
    output += static_cast<char>(0xe0 | (value >> 12));
    output += static_cast<char>(0x80 | ((value >> 6) & 63));
    output += static_cast<char>(0x80 | (value & 63));
  } else {
    output += static_cast<char>(0xf0 | (value >> 18));
    output += static_cast<char>(0x80 | ((value >> 12) & 63));
    output += static_cast<char>(0x80 | ((value >> 6) & 63));
    output += static_cast<char>(0x80 | (value & 63));
  }
}

// This parser retains numeric tokens rather than rounding JSON integers to
// double. Its grammar, UTF-8, duplicate keys, size and recursion are checked.
class Parser {
 public:
  explicit Parser(std::string_view text, bool escaped_lone_surrogates = true,
                  bool bounded_transport = true)
      : text_(text), escaped_lone_surrogates_(escaped_lone_surrogates),
        bounded_transport_(bounded_transport) {
    if (bounded_transport_ && text.size() > 32u * 1024u * 1024u)
      invalid("request exceeds 32 MiB");
  }
  Json parse() {
    auto result = value(0);
    whitespace();
    if (position_ != text_.size()) invalid("trailing data");
    return result;
  }
 private:
  bool digit() const { return position_ < text_.size() && text_[position_] >= '0' && text_[position_] <= '9'; }
  void whitespace() {
    while (position_ < text_.size() && (text_[position_] == ' ' || text_[position_] == '\t' || text_[position_] == '\n' || text_[position_] == '\r')) ++position_;
  }
  bool take(char c) {
    whitespace();
    if (position_ < text_.size() && text_[position_] == c) { ++position_; return true; }
    return false;
  }
  void require(char c) { if (!take(c)) invalid("unexpected character"); }
  std::uint32_t hex4() {
    std::uint32_t result = 0;
    for (int i = 0; i < 4; ++i) {
      if (position_ == text_.size()) invalid("short Unicode escape");
      const char c = text_[position_++];
      const int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
      if (d < 0) invalid("invalid Unicode escape");
      result = (result << 4) | static_cast<unsigned>(d);
    }
    return result;
  }
  std::string string() {
    require('"');
    std::string result;
    while (position_ < text_.size()) {
      const auto c = static_cast<unsigned char>(text_[position_++]);
      if (c == '"') return result;
      if (c < 0x20) invalid("unescaped control character");
      if (c >= 0x80) {
        const int count = c >= 0xc2 && c <= 0xdf ? 1 : c >= 0xe0 && c <= 0xef ? 2 : c >= 0xf0 && c <= 0xf4 ? 3 : -1;
        if (count < 0) invalid("invalid UTF-8");
        std::uint32_t code = c & (count == 1 ? 31 : count == 2 ? 15 : 7);
        for (int i = 0; i < count; ++i) {
          if (position_ == text_.size()) invalid("short UTF-8 sequence");
          const auto next = static_cast<unsigned char>(text_[position_++]);
          if ((next & 0xc0) != 0x80) invalid("invalid UTF-8 continuation");
          code = (code << 6) | (next & 63);
        }
        if ((count == 1 && code < 0x80) || (count == 2 && code < 0x800) || (count == 3 && code < 0x10000) || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) invalid("invalid UTF-8 scalar");
        append_utf8(result, code);
        continue;
      }
      if (c != '\\') { result += static_cast<char>(c); continue; }
      if (position_ == text_.size()) invalid("unterminated escape");
      switch (text_[position_++]) {
        case '"': result += '"'; break;
        case '\\': result += '\\'; break;
        case '/': result += '/'; break;
        case 'b': result += '\b'; break;
        case 'f': result += '\f'; break;
        case 'n': result += '\n'; break;
        case 'r': result += '\r'; break;
        case 't': result += '\t'; break;
        case 'u': {
          auto code = hex4();
          if (code >= 0xd800 && code <= 0xdbff) {
            if (position_ + 2 <= text_.size() && text_[position_] == '\\' && text_[position_ + 1] == 'u') {
              const auto saved = position_;
              position_ += 2;
              const auto low = hex4();
              if (low >= 0xdc00 && low <= 0xdfff)
                code = 0x10000 + ((code - 0xd800) << 10) + low - 0xdc00;
              else {
                if (!escaped_lone_surrogates_) invalid("unpaired Unicode surrogate");
                position_ = saved;
              }
            } else if (!escaped_lone_surrogates_) invalid("unpaired Unicode surrogate");
          } else if (code >= 0xdc00 && code <= 0xdfff && !escaped_lone_surrogates_)
            invalid("unpaired Unicode surrogate");
          append_utf8(result, code);
          break;
        }
        default: invalid("unknown escape");
      }
    }
    invalid("unterminated string");
  }
  Json value(unsigned depth) {
    if (bounded_transport_ && depth > 64) invalid("nesting exceeds 64 levels");
    if (++nodes_ > 1000000 && bounded_transport_) invalid("request exceeds one million values");
    whitespace();
    if (position_ == text_.size()) invalid("unexpected end of input");
    const auto c = text_[position_];
    if (c == '"') return Json{string()};
    if (take('{')) {
      Json::Object result;
      if (!take('}')) {
        do {
          auto key = string();
          require(':');
          if (!result.emplace(std::move(key), value(depth + 1)).second) invalid("duplicate object key");
        } while (take(','));
        require('}');
      }
      return Json{std::move(result)};
    }
    if (take('[')) {
      Json::Array result;
      if (!take(']')) {
        do { result.push_back(value(depth + 1)); } while (take(','));
        require(']');
      }
      return Json{std::move(result)};
    }
    for (const auto token : {std::string_view("null"), std::string_view("true"), std::string_view("false")}) {
      if (text_.substr(position_, token.size()) == token) {
        position_ += token.size();
        if (token == "null") return Json{nullptr};
        return Json{token == "true"};
      }
    }
    const auto start = position_;
    if (text_[position_] == '-') ++position_;
    if (!digit()) invalid("expected a value");
    if (text_[position_] == '0') ++position_;
    else while (digit()) ++position_;
    if (position_ < text_.size() && text_[position_] == '.') {
      ++position_;
      if (!digit()) invalid("missing fractional digits");
      while (digit()) ++position_;
    }
    if (position_ < text_.size() && (text_[position_] == 'e' || text_[position_] == 'E')) {
      ++position_;
      if (position_ < text_.size() && (text_[position_] == '+' || text_[position_] == '-')) ++position_;
      if (!digit()) invalid("missing exponent digits");
      while (digit()) ++position_;
    }
    return Json{Number{std::string(text_.substr(start, position_ - start))}};
  }
  std::string_view text_;
  bool escaped_lone_surrogates_ = false;
  bool bounded_transport_ = true;
  std::size_t position_ = 0, nodes_ = 0;
};

// FEATURE67_EXACT_STORAGE_CODEC
template<class T, class M> struct Member { std::string_view name; M T::* pointer; };
template<class T, class M> auto member(std::string_view name, M T::* pointer) {
  return Member<T,M>{name,pointer};
}
template<class T> struct Record;
#define F(name) member(#name, &V::name)
#define RECORD(Type, ...) template<> struct Record<Type> { \
  static auto members() { using V=Type; return std::make_tuple(__VA_ARGS__); } };
RECORD(PetriArcWeight,F(magnitude),F(negative))
RECORD(PetriTransition,F(activity),F(inputs),F(outputs),F(reset_inputs),F(inhibitor_inputs),F(reset_weights),F(inhibitor_weights),F(input_weights),F(output_weights))
RECORD(PetriMarkingPlace,F(kind),F(id))
RECORD(PetriMarkingEntry,F(place),F(tokens))
RECORD(PetriDetachedMarkingPlace,F(id),F(name))
RECORD(PetriDiscoveryMarkings,F(detached_places),F(initial),F(final))
RECORD(PetriNetLiteralStorage,F(place_count),F(source_storage),F(sink_storage),F(has_source),F(has_sink),F(activities),F(transitions),F(discovery_markings),F(transition_names),F(place_names))
// The workbench carrier writes the nine storage fields every workbench client
// validates; place names stay native-only (a carrier that supplies them is
// still read).
struct PetriNetWorkbenchCarrier : PetriNetLiteralStorage {};
RECORD(PetriNetWorkbenchCarrier,F(place_count),F(source_storage),F(sink_storage),F(has_source),F(has_sink),F(activities),F(transitions),F(discovery_markings),F(transition_names))
RECORD(OcdfgActivityMetric,F(events),F(unique_objects),F(event_objects),F(total_objects))
RECORD(OcdfgEdgeMetric,F(event_couples),F(unique_objects),F(event_objects),F(total_objects),F(performance),F(performance_event_couples),F(performance_total_objects))
RECORD(OcdfgGraph,F(activities),F(object_types),F(activities_indep),F(activities_ot),F(start_activities),F(end_activities),F(edges),F(performance_computed))
RECORD(OcpnTokenReplayPlaceDiagnostics,F(missing),F(remaining),F(consumed),F(produced))
RECORD(OcpnTokenReplayDiagnostics,F(enabled),F(trace_count),F(fitting_traces),F(places),F(transition_activations))
RECORD(OcpnGraph,F(activities),F(object_types),F(petri_nets),F(activity_object_types),F(activity_variable_object_types),F(double_arcs_on_activity),F(annotations),F(annotated),F(tbr_results))
RECORD(OcCausalMarker,F(related_activity),F(object_type),F(min_cardinality),F(max_cardinality),F(marker_key))
RECORD(OcCausalMarkerGroup,F(markers),F(support_count))
RECORD(OcCausalActivity,F(activity),F(input_marker_groups),F(output_marker_groups))
RECORD(OcCausalNet,F(workflow_projection),F(object_types),F(activities))
RECORD(OcpnPlayoutOptions,F(max_bindings_per_activity),F(branching_factor_transitions),F(branching_factor_bindings),F(branching_factor_activities),F(max_traces),F(max_states),F(objects),F(initial_markings),F(final_markings),F(objects_unique_per_trace),F(event_binding),F(exists_trace),F(return_sequences),F(return_traces),F(seed),F(start),F(case_interval),F(event_interval))
RECORD(OcelAttribute,F(key),F(value))
RECORD(OcelEvent,F(id),F(activity),F(timestamp),F(attributes))
RECORD(OcelObject,F(id),F(type),F(attributes))
RECORD(OcelRelation,F(event_id),F(object_id),F(qualifier))
RECORD(OcelO2oRelation,F(source_id),F(target_id),F(qualifier))
RECORD(OcelE2eRelation,F(source_id),F(target_id),F(qualifier))
RECORD(OcelObjectChange,F(object_id),F(object_type),F(timestamp),F(field),F(value))
RECORD(OcelTableColumns,F(events),F(objects),F(relations))
RECORD(Ocel,F(events),F(objects),F(relations),F(o2o),F(e2e),F(object_changes),F(globals),F(event_type_attributes),F(object_type_attributes),F(table_columns))
RECORD(OcpnBinding,F(activity),F(objects))
RECORD(OcpnFiredTransition,F(object_type),F(object_id),F(transition_index))
RECORD(OcpnTraceEvent,F(activity),F(objects),F(transitions))
RECORD(OcpnPlayoutResult,F(log),F(sequences),F(traces),F(exists_trace_requested),F(return_sequences_requested),F(return_traces_requested),F(trace_exists))
#undef RECORD
#undef F

template<class T> struct Optional:std::false_type {};
template<class T> struct Optional<std::optional<T>>:std::true_type {using Item=T;};
template<class T> struct Vector:std::false_type {};
template<class T,class A> struct Vector<std::vector<T,A>>:std::true_type {using Item=T;};
template<class T> struct Map:std::false_type {};
template<class K,class V,class C,class A> struct Map<std::map<K,V,C,A>>:std::true_type {using Key=K;using Item=V;};
template<class T> struct Pair:std::false_type {};
template<class A,class B> struct Pair<std::pair<A,B>>:std::true_type {using First=A;using Second=B;};
template<class T> struct Tuple:std::false_type {};
template<class... T> struct Tuple<std::tuple<T...>>:std::true_type {};
template<class T> struct Variant:std::false_type {};
template<class... T> struct Variant<std::variant<T...>>:std::true_type {};

const Json* field(const Json::Object& object,std::string_view name) {
  const auto found=object.find(name);return found==object.end()?nullptr:&found->second;
}
const Json& required(const Json::Object& object,std::string_view name) {
  if(const auto* value=field(object,name))return *value;
  invalid("missing field "+std::string(name));
}
void allowed(const Json::Object& object,std::initializer_list<std::string_view> names) {
  for(const auto& [name,value]:object){(void)value;if(std::find(names.begin(),names.end(),name)==names.end())invalid("unknown field "+name);}
}
const std::string& integer_text(const Json& value,bool signed_value) {
  const auto* number=std::get_if<Number>(&value.value);
  if(!number||number->text.empty())invalid("expected exact JSON integer token");
  const auto& text=number->text;
  const auto start=signed_value&&text.front()=='-'?std::size_t{1}:std::size_t{0};
  if(start==text.size()||!std::all_of(text.begin()+start,text.end(),[](char c){return c>='0'&&c<='9';}))invalid("expected exact JSON integer token");
  return text;
}
template<class T> T integral(const Json& value) {
  const auto& text=integer_text(value,std::is_signed_v<T>);T result{};
  const auto parsed=std::from_chars(text.data(),text.data()+text.size(),result);
  if(parsed.ec!=std::errc{}||parsed.ptr!=text.data()+text.size())invalid("integer exceeds native storage");
  return result;
}
std::string native_string(const Json& value) {
  if(const auto* text=std::get_if<std::string>(&value.value))return *text;
  const auto& object=value.object();allowed(object,{"native_bytes","data"});
  if(required(object,"native_bytes").string()!="hex-v1")invalid("unknown native byte string version");
  const auto& hex=required(object,"data").string();if(hex.size()%2)invalid("odd hex byte string length");
  const auto digit=[](char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;};
  std::string bytes;bytes.reserve(hex.size()/2);
  for(std::size_t i=0;i<hex.size();i+=2){const auto a=digit(hex[i]),b=digit(hex[i+1]);if(a<0||b<0)invalid("invalid hex byte string");bytes+=static_cast<char>((a<<4)|b);}
  return bytes;
}
double real_value(const Json& value) {
  if(const auto* tag=std::get_if<std::string>(&value.value)){
    if(*tag=="NaN")return std::numeric_limits<double>::quiet_NaN();
    if(*tag=="INF")return std::numeric_limits<double>::infinity();
    if(*tag=="-INF")return -std::numeric_limits<double>::infinity();
    invalid("unknown IEEE double tag");
  }
  const auto* number=std::get_if<Number>(&value.value);if(!number)invalid("expected double or IEEE tag");
  double result=0;const auto parsed=std::from_chars(number->text.data(),number->text.data()+number->text.size(),result);
  if(parsed.ec!=std::errc{}||parsed.ptr!=number->text.data()+number->text.size()||!std::isfinite(result))invalid("numeric token exceeds finite double storage");
  return result;
}
void real_json(std::ostream& out,double value) {
  if(std::isnan(value))out<<"\"NaN\"";
  else if(std::isinf(value))out<<(value<0?"\"-INF\"":"\"INF\"");
  else if(value==0&&std::signbit(value))out<<"-0.0";
  else{std::ostringstream scalar;scalar.imbue(std::locale::classic());scalar<<std::setprecision(std::numeric_limits<double>::max_digits10)<<value;auto text=scalar.str();if(text.find_first_of(".eE")==std::string::npos)text+=".0";out<<text;}
}
std::string_view binding_name(std::int32_t index) {
  switch(index){case 0:return "independent";case 1:return "shared_activity";case 2:return "synchronized";default:return {};}
}
OcpnEventBinding binding_value(const Json& value) {
  if(const auto* text=std::get_if<std::string>(&value.value)){
    for(std::int32_t i=0;i<3;++i)if(*text==binding_name(i))return static_cast<OcpnEventBinding>(i);
    invalid("unknown event binding name");
  }
  if(const auto* object=std::get_if<Json::Object>(&value.value)){
    allowed(*object,{"index","name"});const auto index=integral<std::int32_t>(required(*object,"index"));
    if(const auto* name=field(*object,"name")){const auto known=binding_name(index);if(known.empty()){if(!name->null())invalid("unknown binding index requires null name");}else if(name->string()!=known)invalid("event binding name differs from index");}
    return static_cast<OcpnEventBinding>(index);
  }
  return static_cast<OcpnEventBinding>(integral<std::int32_t>(value));
}

template<class T> T read(const Json&);
template<class T> void write(std::ostream&,const T&);
template<class T,std::size_t I=0> T variant_read(std::size_t index,const Json& value) {
  if constexpr(I==std::variant_size_v<T>)invalid("variant index exceeds native alternatives");
  else{if(index==I)return T{std::in_place_index<I>,read<std::variant_alternative_t<I,T>>(value)};return variant_read<T,I+1>(index,value);}
}
template<class T,std::size_t... I> T tuple_read(const Json::Array& values,std::index_sequence<I...>) {
  if(values.size()!=sizeof...(I))invalid("tuple length differs from native storage");
  return T{read<std::tuple_element_t<I,T>>(values[I])...};
}
template<class T> T record_read(const Json& value) {
  const auto& object=value.object();const auto members=Record<T>::members();
  for(const auto& [name,unused]:object){(void)unused;const bool known=std::apply([&](const auto&... entry){return ((name==entry.name)||...);},members);if(!known)invalid("unknown field "+name);}
  T result{};
  std::apply([&](const auto&... entry){([&]{if(const auto* stored=field(object,entry.name)){using M=std::remove_cvref_t<decltype(result.*(entry.pointer))>;result.*(entry.pointer)=read<M>(*stored);}}(),...);},members);
  return result;
}
template<class T> T read(const Json& value) {
  if constexpr(std::is_same_v<T,std::nullptr_t>){if(!value.null())invalid("expected null variant payload");return nullptr;}
  else if constexpr(std::is_same_v<T,bool>){const auto* result=std::get_if<bool>(&value.value);if(!result)invalid("expected boolean");return *result;}
  else if constexpr(std::is_same_v<T,std::string>)return native_string(value);
  else if constexpr(std::is_same_v<T,double>)return real_value(value);
  else if constexpr(std::is_same_v<T,TokenInteger>)return TokenInteger(integer_text(value,true));
  else if constexpr(std::is_same_v<T,OcpnEventBinding>)return binding_value(value);
  else if constexpr(std::is_enum_v<T>)return static_cast<T>(integral<std::int32_t>(value));
  else if constexpr(std::is_integral_v<T>)return integral<T>(value);
  else if constexpr(Optional<T>::value){if(value.null())return std::nullopt;return T{read<typename Optional<T>::Item>(value)};}
  else if constexpr(Vector<T>::value){T result;result.reserve(value.array().size());for(const auto& item:value.array())result.push_back(read<typename Vector<T>::Item>(item));return result;}
  else if constexpr(Map<T>::value){T result;for(const auto& item:value.array()){const auto& entry=item.object();allowed(entry,{"key","value"});auto key=read<typename Map<T>::Key>(required(entry,"key"));auto stored=read<typename Map<T>::Item>(required(entry,"value"));if(!result.emplace(std::move(key),std::move(stored)).second)invalid("duplicate decoded map key");}return result;}
  else if constexpr(Pair<T>::value){const auto& values=value.array();if(values.size()!=2)invalid("pair length differs from native storage");return {read<typename Pair<T>::First>(values[0]),read<typename Pair<T>::Second>(values[1])};}
  else if constexpr(Tuple<T>::value)return tuple_read<T>(value.array(),std::make_index_sequence<std::tuple_size_v<T>>{});
  else if constexpr(Variant<T>::value){const auto& object=value.object();allowed(object,{"index","value"});return variant_read<T>(integral<std::size_t>(required(object,"index")),required(object,"value"));}
  else if constexpr(std::is_same_v<T,HeuristicStructuredValue>){return {read<HeuristicStructuredValue::Value>(value)};}
  else if constexpr(std::is_same_v<T,PetriNet>)return PetriNet::from_literal_storage(read<PetriNetLiteralStorage>(value));
  else return record_read<T>(value);
}
template<class T> void record_write(std::ostream& out,const T& value) {
  out<<'{';bool first=true;
  std::apply([&](const auto&... entry){([&]{if(!first)out<<',';first=false;out<<serialize_embedding_workbench_string_json(entry.name)<<':';write(out,value.*(entry.pointer));}(),...);},Record<T>::members());
  out<<'}';
}
template<class T> void write(std::ostream& out,const T& value) {
  if constexpr(std::is_same_v<T,std::nullptr_t>)out<<"null";
  else if constexpr(std::is_same_v<T,bool>)out<<(value?"true":"false");
  else if constexpr(std::is_same_v<T,std::string>)out<<serialize_embedding_workbench_string_json(value);
  else if constexpr(std::is_same_v<T,double>)real_json(out,value);
  else if constexpr(std::is_same_v<T,TokenInteger>)out<<value.decimal();
  else if constexpr(std::is_same_v<T,OcpnEventBinding>){const auto index=static_cast<std::int32_t>(value);const auto name=binding_name(index);out<<"{\"name\":";if(name.empty())out<<"null";else out<<serialize_embedding_workbench_string_json(name);out<<",\"index\":"<<index<<'}';}
  else if constexpr(std::is_enum_v<T>)out<<static_cast<std::int32_t>(value);
  else if constexpr(std::is_integral_v<T>)out<<value;
  else if constexpr(Optional<T>::value){if(value)write(out,*value);else out<<"null";}
  else if constexpr(Vector<T>::value){out<<'[';bool first=true;for(const auto& item:value){if(!first)out<<',';first=false;write(out,item);}out<<']';}
  else if constexpr(Map<T>::value){out<<'[';bool first=true;for(const auto& [key,item]:value){if(!first)out<<',';first=false;out<<"{\"key\":";write(out,key);out<<",\"value\":";write(out,item);out<<'}';}out<<']';}
  else if constexpr(Pair<T>::value){out<<'[';write(out,value.first);out<<',';write(out,value.second);out<<']';}
  else if constexpr(Tuple<T>::value){out<<'[';bool first=true;std::apply([&](const auto&... item){([&]{if(!first)out<<',';first=false;write(out,item);}(),...);},value);out<<']';}
  else if constexpr(Variant<T>::value){out<<"{\"index\":"<<value.index()<<",\"value\":";std::visit([&](const auto& item){write(out,item);},value);out<<'}';}
  else if constexpr(std::is_same_v<T,HeuristicStructuredValue>)write(out,value.value);
  else if constexpr(std::is_same_v<T,PetriNet>){PetriNetWorkbenchCarrier stored;stored.place_count=value.place_count();stored.source_storage=value.source();stored.sink_storage=value.sink();stored.has_source=value.has_source();stored.has_sink=value.has_sink();stored.activities=value.activities();stored.transitions=value.transitions();stored.discovery_markings=value.discovery_markings();stored.transition_names=value.transition_names();write(out,stored);}
  else record_write(out,value);
}
template<class T> T parse(std::string_view source) {return read<T>(Parser(source,true).parse());}
template<class T> std::string render(const T& value) {std::ostringstream out;out.imbue(std::locale::classic());write(out,value);return out.str();}
}  // namespace

OcpnGraph parse_oc_playout_workbench_ocpn_json(std::string_view value){return parse<OcpnGraph>(value);}
std::string serialize_oc_playout_workbench_ocpn_json(const OcpnGraph& value){return render(value);}
OcCausalNet parse_oc_playout_workbench_causal_net_json(std::string_view value){return parse<OcCausalNet>(value);}
std::string serialize_oc_playout_workbench_causal_net_json(const OcCausalNet& value){return render(value);}
OcpnPlayoutOptions parse_oc_playout_workbench_options_json(std::string_view value,std::set<std::string>* supplied){const auto document=Parser(value,true).parse();auto result=read<OcpnPlayoutOptions>(document);if(supplied){std::set<std::string> members;for(const auto& [name,unused]:document.object()){(void)unused;members.insert(name);}*supplied=std::move(members);}return result;}
std::string serialize_oc_playout_workbench_options_json(const OcpnPlayoutOptions& value){return render(value);}
Ocel parse_oc_playout_workbench_ocel_json(std::string_view value){return parse<Ocel>(value);}
std::string serialize_oc_playout_workbench_ocel_json(const Ocel& value){return render(value);}
OcpnPlayoutResult parse_oc_playout_workbench_result_json(std::string_view value){return parse<OcpnPlayoutResult>(value);}
std::string serialize_oc_playout_workbench_result_json(const OcpnPlayoutResult& value){return render(value);}
PetriNet parse_oc_playout_workbench_petri_net_json(std::string_view value){return parse<PetriNet>(value);}
std::string serialize_oc_playout_workbench_petri_net_json(const PetriNet& value){return render(value);}
}  // namespace minera
