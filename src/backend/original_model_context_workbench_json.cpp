#include "minera/original_model_context_workbench_json.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/object_centric_playout_workbench_json.hpp"
#include "minera/quality_workbench_json.hpp"
#include "minera/declare_workbench_json.hpp"
#include "minera/log_skeleton_workbench_json.hpp"
#include "minera/temporal_workbench_json.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <initializer_list>
#include <limits>
#include <locale>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
namespace minera { namespace {
[[noreturn]] void invalid(std::string message) {
  throw std::invalid_argument("Original model context JSON: " + message);
}

struct Number { std::string text; };
struct Json {
  using Array = std::vector<Json>;
  using Object = std::map<std::string, Json, std::less<>>;
  std::variant<std::nullptr_t, bool, Number, std::string, Array, Object> value;
  std::vector<std::string> object_order; // Source order for ordered dictionary consumers.
  bool null() const { return std::holds_alternative<std::nullptr_t>(value); }
  const Object& object() const {
    if (const auto* result = std::get_if<Object>(&value)) return *result;
    invalid("expected an object");
  }
  const Array& array() const {
    if (const auto* result = std::get_if<Array>(&value)) return *result;
    invalid("expected an array");
  }
  std::string string() const {
    if (const auto* result = std::get_if<std::string>(&value)) return *result;
    const auto* object = std::get_if<Object>(&value);
    if(!object||object->size()!=2||!object->contains("native_bytes")||!object->contains("data"))invalid("expected a NativeString");
    const auto* tag=std::get_if<std::string>(&object->at("native_bytes").value);const auto* hex=std::get_if<std::string>(&object->at("data").value);
    if(!tag||*tag!="hex-v1"||!hex||hex->size()%2)invalid("invalid NativeString byte carrier");
    const auto digit=[](char c)->int{return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;};std::string out;out.reserve(hex->size()/2);
    for(std::size_t i=0;i<hex->size();i+=2){const int a=digit((*hex)[i]),b=digit((*hex)[i+1]);if(a<0||b<0)invalid("invalid NativeString hexadecimal bytes");out+=static_cast<char>((a<<4)|b);}return out;
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
// double. Grammar, UTF8/WTF8 and duplicate keys are checked; there is no
// independent byte/node/depth cap beyond transport and process resources.
class Parser {
 public:
  explicit Parser(std::string_view text):text_(text){}
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
        if ((count == 1 && code < 0x80) || (count == 2 && code < 0x800) || (count == 3 && code < 0x10000) || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff && !escaped_lone_surrogates_)) invalid("invalid UTF-8 scalar");
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
    whitespace();
    if (position_ == text_.size()) invalid("unexpected end of input");
    const auto c = text_[position_];
    if (c == '"') return Json{string()};
    if (take('{')) {
      Json::Object result;std::vector<std::string> order;
      if (!take('}')) {
        do {
          auto key = string();
          require(':');
          auto item=value(depth+1);
          if (!result.emplace(key,std::move(item)).second) invalid("duplicate object key");
          order.push_back(std::move(key));
        } while (take(','));
        require('}');
      }
      return Json{std::move(result),std::move(order)};
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
  bool escaped_lone_surrogates_ = true;
  std::size_t position_ = 0;
};

void allowed_fields(const Json::Object& object,
                    std::initializer_list<std::string_view> allowed) {
  for (const auto& [name, value] : object) {
    (void)value;
    if (std::find(allowed.begin(), allowed.end(), name) == allowed.end())
      invalid("unknown field " + name);
  }
}

const Json* field(const Json::Object& object, std::string_view name) {
  const auto found = object.find(name);
  return found == object.end() ? nullptr : &found->second;
}

std::uint64_t unsigned_integer(const Json& value) {
  const auto* number = std::get_if<Number>(&value.value);
  if (!number || number->text.empty() ||
      !std::all_of(number->text.begin(), number->text.end(),
                   [](char c) { return c >= '0' && c <= '9'; }))
    invalid("expected an unsigned JSON integer token");
  std::uint64_t result = 0;
  const auto parsed = std::from_chars(number->text.data(),
      number->text.data() + number->text.size(), result);
  if (parsed.ec != std::errc{} || parsed.ptr != number->text.data() + number->text.size())
    invalid("unsigned integer exceeds uint64 range");
  return result;
}

template <class Integer>
Integer native_unsigned(const Json& value) {
  const auto result = unsigned_integer(value);
  if (result > std::numeric_limits<Integer>::max())
    invalid("integer exceeds native unsigned storage range");
  return static_cast<Integer>(result);
}

int native_int(const Json& value) {
  const auto* number = std::get_if<Number>(&value.value);
  if (!number || number->text.empty() ||
      number->text.find_first_of(".eE") != std::string::npos)
    invalid("expected an exact native int token");
  int result = 0;
  const auto parsed = std::from_chars(number->text.data(),
      number->text.data() + number->text.size(), result);
  if (parsed.ec != std::errc{} || parsed.ptr != number->text.data() + number->text.size())
    invalid("integer exceeds native int storage range");
  return result;
}

double ieee_double(const Json& value) {
  if (const auto* tag = std::get_if<std::string>(&value.value)) {
    if (*tag == "NaN") return std::numeric_limits<double>::quiet_NaN();
    if (*tag == "INF") return std::numeric_limits<double>::infinity();
    if (*tag == "-INF") return -std::numeric_limits<double>::infinity();
    invalid("expected an IEEE double tag NaN, INF, or -INF");
  }
  const auto* number = std::get_if<Number>(&value.value);
  if (!number) invalid("expected a numeric double or IEEE tag");
  double result = 0;
  const auto parsed = std::from_chars(number->text.data(),
      number->text.data() + number->text.size(), result,
      std::chars_format::general);
  if (parsed.ec == std::errc{} && parsed.ptr == number->text.data() + number->text.size())
    return result;
  // from_chars reports range errors for decimal tokens that round below the
  // smallest subnormal as well as for overflow. Preserve signed underflow
  // zero without accepting numeric overflow as an implicit infinity tag.
  if (parsed.ec == std::errc::result_out_of_range) {
    std::istringstream input(number->text);
    input.imbue(std::locale::classic());
    input >> result;
    if (result == 0.0 && input.eof())
      return !number->text.empty() && number->text.front() == '-' ? -0.0 : 0.0;
  }
  invalid("double token exceeds finite native storage; use an explicit IEEE tag");
}

template <class T, class Convert>
std::vector<T> vector_value(const Json& value, Convert convert) {
  std::vector<T> result;
  result.reserve(value.array().size());
  for (const auto& item : value.array()) result.push_back(convert(item));
  return result;
}

void quoted(std::ostream& out, const std::string& value) {
  out << serialize_embedding_workbench_string_json(value);
}

void number(std::ostream& out, double value) {
  if (std::isnan(value)) out << "\"NaN\"";
  else if (std::isinf(value)) out << (value < 0 ? "\"-INF\"" : "\"INF\"");
  else if (value == 0.0 && std::signbit(value)) out << "-0.0";
  else out << std::setprecision(std::numeric_limits<double>::max_digits10) << value;
}

template <class T, class Write>
void array_json(std::ostream& out, const std::vector<T>& values, Write write) {
  out << '[';
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i) out << ',';
    write(out, values[i]);
  }
  out << ']';
}

template <class Integer>
void integer(std::ostream& out, Integer value) { out << value; }

void boolean(std::ostream& out, bool value) { out << (value ? "true" : "false"); }

std::ostringstream output() {
  std::ostringstream result;
  result.imbue(std::locale::classic());
  return result;
}

const Json& required(const Json::Object& object, std::string_view name) {
  if (const auto* value = field(object, name)) return *value;
  invalid("missing field " + std::string(name));
}

std::int64_t signed_integer(const Json& value) {
  const auto* token = std::get_if<Number>(&value.value);
  if (!token || token->text.empty() || token->text.find_first_of(".eE") != std::string::npos)
    invalid("expected an exact int64 integer token");
  std::int64_t result = 0;
  const auto parsed = std::from_chars(token->text.data(), token->text.data() + token->text.size(), result);
  if (parsed.ec != std::errc{} || parsed.ptr != token->text.data() + token->text.size())
    invalid("integer exceeds int64 storage range");
  return result;
}


std::string q(std::string_view value){return serialize_embedding_workbench_string_json(value);}
template<class T>struct Vector:std::false_type{};
template<class T,class A>struct Vector<std::vector<T,A>>:std::true_type{using Item=T;};
// TokenMarking is a vector-derived native carrier, not a vector alias.
template<>struct Vector<TokenMarking>:std::true_type{using Item=TokenInteger;};
template<class T>struct Optional:std::false_type{};
template<class T>struct Optional<std::optional<T>>:std::true_type{using Item=T;};
template<class T>struct Map:std::false_type{};
template<class K,class V,class C,class A>struct Map<std::map<K,V,C,A>>:std::true_type{using Key=K;using Item=V;};
template<class T>struct Variant:std::false_type{};
template<class...T>struct Variant<std::variant<T...>>:std::true_type{};
template<class Owner,class Type>struct Member{std::string_view name;Type Owner::*pointer;};
template<class Owner,class Type>auto member(std::string_view name,Type Owner::*p){return Member<Owner,Type>{name,p};}
template<class T>struct Record;
#define F(name) member(#name,&V::name)
#define RECORD(Type,...) template<>struct Record<Type>{static auto members(){using V=Type;return std::make_tuple(__VA_ARGS__);}};
RECORD(DeclareConstraint,F(first),F(second),F(binary),F(support),F(confidence))
RECORD(DeclareModel,F(rules))
RECORD(OrderedDeclareFamily,F(key),F(value))
RECORD(OrderedDeclareModel,F(rules))
RECORD(LogSkeletonPair,F(source),F(target))
RECORD(LogSkeletonModel,F(equivalence),F(always_after),F(always_before),F(never_together),F(directly_follows),F(activ_freq))
RECORD(TemporalProfileEntry,F(source),F(target),F(count),F(mean_seconds),F(standard_deviation_seconds))
RECORD(TemporalProfile,F(entries))
RECORD(PetriNetLiteralStorage,F(place_count),F(source_storage),F(sink_storage),F(has_source),F(has_sink),F(transitions),F(activities),F(discovery_markings),F(transition_names))
RECORD(PetriTransition,F(activity),F(inputs),F(outputs),F(reset_inputs),F(inhibitor_inputs),F(reset_weights),F(inhibitor_weights),F(input_weights),F(output_weights))
RECORD(PetriArcWeight,F(magnitude),F(negative))
RECORD(PetriDiscoveryMarkings,F(detached_places),F(initial),F(final))
RECORD(PetriMarkingPlace,F(kind),F(id))
RECORD(PetriMarkingEntry,F(place),F(tokens))
RECORD(PetriDetachedMarkingPlace,F(id),F(name))
RECORD(PnmlDocument,F(net),F(initial_marking),F(final_marking),F(stochastic_map),F(place_ids),F(transition_ids),F(initial_marking_keys),F(final_marking_keys),F(arc_occurrences))
RECORD(PnmlArcOccurrence,F(place),F(transition),F(input),F(type),F(weight),F(id),F(negative_weight))
RECORD(StochasticMap,F(transitions))
RECORD(StochasticMapTransition,F(transition),F(timing),F(distribution),F(parameter),F(secondary_parameter),F(tertiary_parameter),F(activation_weight),F(priority))
RECORD(PnmlImportOptions,F(infer_initial_marking),F(infer_final_marking),F(import_stochastic_map),F(require_workflow_boundaries),F(public_marking_semantics))
#undef RECORD
#undef F
template<class T>struct EnumNames;
template<>struct EnumNames<PetriMarkingPlaceKind>{static constexpr std::array<std::string_view,2> names={"ROOT","DETACHED"};};
template<>struct EnumNames<PetriArcType>{static constexpr std::array<std::string_view,3> names={"NORMAL","RESET","INHIBITOR"};};
template<>struct EnumNames<StochasticMapTiming>{static constexpr std::array<std::string_view,2> names={"TIMED","IMMEDIATE"};};
template<>struct EnumNames<StochasticMapDistribution>{static constexpr std::array<std::string_view,8> names={"DETERMINISTIC","EXPONENTIAL","CONSTANT0","IMMEDIATE","NORMAL","UNIFORM","LOGNORMAL","GAMMA"};};
template<class T>std::string_view enum_name(T value){const auto i=static_cast<int>(value);return i>=0&&static_cast<std::size_t>(i)<EnumNames<T>::names.size()?EnumNames<T>::names[static_cast<std::size_t>(i)]:std::string_view{};}
template<class T>T read(const Json&);
template<class T>void write(std::ostream&,const T&);
template<class T>std::string render(const T& value){auto out=output();write(out,value);return out.str();}
template<class T>T enum_read(const Json& value){
 if(std::holds_alternative<Number>(value.value))return static_cast<T>(native_int(value));
 if(const auto* object=std::get_if<Json::Object>(&value.value)){allowed_fields(*object,{"name","index"});const auto result=static_cast<T>(native_int(required(*object,"index")));const auto name=enum_name(result);const auto& tag=required(*object,"name");if(name.empty()){if(!tag.null())invalid("unknown enum requires null name");}else if(tag.string()!=name)invalid("enum name differs from index");return result;}
 const auto name=value.string();for(std::size_t i=0;i<EnumNames<T>::names.size();++i)if(name==EnumNames<T>::names[i])return static_cast<T>(i);invalid("unknown enum name");
}
template<class T>T record_read(const Json& value){
 const auto& object=value.object();const auto members=Record<T>::members();
 for(const auto& [name,unused]:object){(void)unused;if(!std::apply([&](const auto&...entry){return ((name==entry.name)||...);},members))invalid("unknown field "+name);}
 T result{};
 std::apply([&](const auto&...entry){([&]{if(const auto* item=field(object,entry.name)){using M=std::remove_cvref_t<decltype(result.*(entry.pointer))>;result.*(entry.pointer)=read<M>(*item);}}(),...);},members);
 return result;
}
template<class T,std::size_t I=0>T variant_read(std::size_t index,const Json& value){if constexpr(I==std::variant_size_v<T>)invalid("variant index exceeds native alternatives");else{if(index==I)return T{std::in_place_index<I>,read<std::variant_alternative_t<I,T>>(value)};return variant_read<T,I+1>(index,value);}}
std::string exact_integer_text(const Json& value){const auto* token=std::get_if<Number>(&value.value);if(!token||token->text.empty())invalid("expected exact integer token");const auto& text=token->text;const auto begin=text.front()=='-'?std::size_t{1}:std::size_t{0};if(begin==text.size()||!std::all_of(text.begin()+begin,text.end(),[](char c){return c>='0'&&c<='9';}))invalid("expected exact integer token");return text;}
template<class T>T read(const Json& value){
 if constexpr(std::is_same_v<T,std::nullptr_t>){if(!value.null())invalid("expected null");return nullptr;}
 else if constexpr(std::is_same_v<T,bool>)return value.boolean();
 else if constexpr(std::is_same_v<T,int>)return native_int(value);
 else if constexpr(std::is_integral_v<T>){if constexpr(std::is_signed_v<T>)return static_cast<T>(signed_integer(value));else return native_unsigned<T>(value);}
 else if constexpr(std::is_enum_v<T>)return enum_read<T>(value);
 else if constexpr(std::is_same_v<T,double>)return ieee_double(value);
 else if constexpr(std::is_same_v<T,std::string>)return value.string();
 else if constexpr(std::is_same_v<T,TokenInteger>)return TokenInteger(exact_integer_text(value));
 else if constexpr(Optional<T>::value){if(value.null())return std::nullopt;return T{read<typename Optional<T>::Item>(value)};}
 else if constexpr(Vector<T>::value){T result;result.reserve(value.array().size());for(const auto& item:value.array())result.push_back(read<typename Vector<T>::Item>(item));return result;}
 else if constexpr(Map<T>::value){T result;if(const auto* object=std::get_if<Json::Object>(&value.value)){for(const auto& [key,item]:*object){if(!result.emplace(key,read<typename Map<T>::Item>(item)).second)invalid("duplicate decoded map key");}}else for(const auto& item:value.array()){const auto& entry=item.object();allowed_fields(entry,{"key","value"});auto key=read<typename Map<T>::Key>(required(entry,"key"));auto stored=read<typename Map<T>::Item>(required(entry,"value"));if(!result.emplace(std::move(key),std::move(stored)).second)invalid("duplicate decoded map key");}return result;}
 else if constexpr(Variant<T>::value){const auto& o=value.object();allowed_fields(o,{"index","value"});return variant_read<T>(native_unsigned<std::size_t>(required(o,"index")),required(o,"value"));}
 else if constexpr(std::is_same_v<T,HeuristicStructuredValue>)return {read<HeuristicStructuredValue::Value>(value)};
 else if constexpr(std::is_same_v<T,PetriNet>)return PetriNet::from_literal_storage(read<PetriNetLiteralStorage>(value));
 else if constexpr(std::is_same_v<T,DeclareConstraint>){const auto& o=value.object();for(const auto* key:{"first","second","binary","support","confidence"})(void)required(o,key);return record_read<T>(value);}
 else if constexpr(std::is_same_v<T,LogSkeletonPair>){const auto& o=value.object();(void)required(o,"source");(void)required(o,"target");return record_read<T>(value);}
 else return record_read<T>(value);
}
template<class T>void write(std::ostream& out,const T& value){
 if constexpr(std::is_same_v<T,std::nullptr_t>)out<<"null";
 else if constexpr(std::is_same_v<T,bool>)out<<(value?"true":"false");
 else if constexpr(std::is_integral_v<T>)out<<value;
 else if constexpr(std::is_same_v<T,double>)number(out,value);
 else if constexpr(std::is_same_v<T,std::string>)out<<q(value);
 else if constexpr(std::is_same_v<T,TokenInteger>)out<<value.decimal();
 else if constexpr(std::is_enum_v<T>)out<<"{\"name\":"<<(enum_name(value).empty()?"null":q(enum_name(value)))<<",\"index\":"<<static_cast<int>(value)<<'}';
 else if constexpr(Optional<T>::value){if(value)write(out,*value);else out<<"null";}
 else if constexpr(Vector<T>::value){out<<'[';for(std::size_t i=0;i<value.size();++i){if(i)out<<',';if constexpr(std::is_same_v<T,std::vector<bool>>)write(out,static_cast<bool>(value[i]));else write(out,value[i]);}out<<']';}
 else if constexpr(Map<T>::value){out<<'[';bool first=true;for(const auto& [key,item]:value){if(!first)out<<',';first=false;out<<"{\"key\":";write(out,key);out<<",\"value\":";write(out,item);out<<'}';}out<<']';}
 else if constexpr(Variant<T>::value){out<<"{\"index\":"<<value.index()<<",\"value\":";std::visit([&](const auto& item){write(out,item);},value);out<<'}';}
 else if constexpr(std::is_same_v<T,HeuristicStructuredValue>)write(out,value.value);
 else if constexpr(std::is_same_v<T,PetriNet>)out<<serialize_oc_playout_workbench_petri_net_json(value);
 else if constexpr(std::is_same_v<T,PnmlDocument>)out<<serialize_quality_workbench_document_json(value);
 else{out<<'{';bool first=true;std::apply([&](const auto&...entry){([&]{if(!first)out<<',';first=false;out<<q(entry.name)<<':';write(out,value.*(entry.pointer));}(),...);},Record<T>::members());out<<'}';}
}
std::string parts_json(const std::vector<DiscoveryWorkbenchPart>& parts){auto out=output();out<<'[';bool first=true;for(const auto& part:parts){if(!first)out<<',';first=false;out<<"{\"name\":"<<q(part.name)<<",\"filename\":"<<q(part.filename)<<",\"bytes\":"<<q(part.bytes)<<",\"file\":"<<(part.file?"true":"false")<<'}';}out<<']';return out.str();}
using Model=std::variant<OrderedDeclareModel,LogSkeletonModel,TemporalProfile,PetriNet,PnmlDocument>;
std::string snapshot(const Model& value){return std::visit([](const auto& x){return render(x);},value);}
std::string type_name(const Model& value){static const char* names[]={"DeclareModel","LogSkeletonModel","TemporalProfile","PetriNet","PnmlDocument"};return names[value.index()];}
bool header_text(std::string_view value){if(value=="true"||value=="1")return true;if(value=="false"||value=="0")return false;invalid("header expects true/false or 1/0");}
bool header_options(std::string_view value){const auto doc=Parser(value).parse();const auto& o=doc.object();allowed_fields(o,{"include_header"});if(o.size()!=1)invalid("context options require exactly include_header");return required(o,"include_header").boolean();}
PnmlImportOptions import_options(std::string_view value){const auto doc=Parser(value).parse();const auto& o=doc.object();if(o.size()!=5)invalid("PNML import options require all five native fields");for(const auto* key:{"infer_initial_marking","infer_final_marking","import_stochastic_map","require_workflow_boundaries","public_marking_semantics"})(void)required(o,key);return read<PnmlImportOptions>(doc);}
} // namespace
DeclareModel parse_original_context_declare_json(std::string_view input){const auto doc=Parser(input).parse();const auto& o=doc.object();allowed_fields(o,{"rules"});DeclareModel model;model.rules=read<decltype(model.rules)>(required(o,"rules"));return model;}
OrderedDeclareModel parse_original_context_ordered_declare_json(std::string_view input){
 const auto doc=Parser(input).parse();const auto& root=doc.object();allowed_fields(root,{"rules"});
 const auto& rules=required(root,"rules");OrderedDeclareModel model;std::set<std::string> seen;
 const auto append=[&](std::string family,const Json& entries){if(!seen.insert(family).second)invalid("duplicate decoded map key");model.rules.push_back({std::move(family),read<std::vector<DeclareConstraint>>(entries)});};
 if(const auto* object=std::get_if<Json::Object>(&rules.value)){
  for(const auto& family:rules.object_order)append(family,object->at(family));
 }else for(const auto& item:rules.array()){
  const auto& entry=item.object();allowed_fields(entry,{"key","value"});append(required(entry,"key").string(),required(entry,"value"));
 }
 return model;
}
LogSkeletonModel parse_original_context_skeleton_json(std::string_view input){return read<LogSkeletonModel>(Parser(input).parse());}
TemporalProfile parse_original_context_temporal_json(std::string_view input){return read<TemporalProfile>(Parser(input).parse());}
PetriNet parse_original_context_net_json(std::string_view input){return read<PetriNet>(Parser(input).parse());}
PnmlDocument parse_original_context_document_json(std::string_view input){return read<PnmlDocument>(Parser(input).parse());}
TokenMarking parse_original_context_marking_json(std::string_view input){return read<TokenMarking>(Parser(input).parse());}
std::string serialize_original_context_model_json(const DeclareModel& value){return render(value);}
std::string serialize_original_context_model_json(const OrderedDeclareModel& value){return render(value);}
std::string serialize_original_context_model_json(const LogSkeletonModel& value){return render(value);}
std::string serialize_original_context_model_json(const TemporalProfile& value){return render(value);}
std::string serialize_original_context_model_json(const PetriNet& value){return render(value);}
std::string serialize_original_context_model_json(const PnmlDocument& value){return render(value);}

DiscoveryWorkbenchResponse execute_original_model_context(const std::vector<DiscoveryWorkbenchPart>& parts,const std::vector<DiscoveryWorkbenchPart>& query,std::string_view input_order,std::string_view transport_error){
 std::vector<DiscoveryWorkbenchPart> all=query;all.insert(all.end(),parts.begin(),parts.end());
 const auto first=[&](std::string_view name)->const DiscoveryWorkbenchPart*{for(const auto& p:all)if(p.name==name)return &p;return nullptr;};
 const auto original=first("model_file");const std::string original_bytes=original?original->bytes:"",filename=original?original->filename:"";
 std::map<std::string,const DiscoveryWorkbenchPart*> controls;
 const auto has=[&](std::string_view name){return controls.contains(std::string(name));};
 const auto value=[&](std::string_view name)->const std::string&{return controls.at(std::string(name))->bytes;};
 std::string variant=first("variant")?first("variant")->bytes:"",format=first("model_format")?first("model_format")->bytes:"auto",detected=format,phase="configuration_error",error,error_type;
 std::optional<bool> header;std::optional<PnmlImportOptions> imports;std::optional<Model> model,effective;std::optional<LlmContextResult> result;
 std::optional<TokenMarking> initial_override,final_override,initial_before,final_before,initial_after,final_after;
 std::string initial_origin,final_origin,before="null",after="null",effective_before="null",effective_after="null";bool entered=false,returned=false;
 std::map<std::string,std::string> artifacts,artifact_errors;
 try{
  if(!transport_error.empty()){phase="transport_error";throw std::invalid_argument(std::string(transport_error));}
  static const std::set<std::string> allowed={"variant","model_file","model_format","include_header","response_header","context_options_json","context_options_file","initial_marking_json","initial_marking_file","final_marking_json","final_marking_file","pnml_import_options_json","pnml_import_options_file"};
  for(const auto& p:all){if(!allowed.contains(p.name))invalid("unknown original context control: "+p.name);if(!controls.emplace(p.name,&p).second)invalid("duplicate original context control: "+p.name);if(p.name.ends_with("_file")&&!p.file)invalid(p.name+" requires an actual File input");}
  if(!has("model_file"))invalid("original model context requires model_file");
  if(variant!="declare"&&variant!="log_skeleton"&&variant!="temporal_profile"&&variant!="petri_net")invalid("variant must be declare, log_skeleton, temporal_profile or petri_net");
  for(const auto* role:{"context_options","initial_marking","final_marking","pnml_import_options"})if(has(std::string(role)+"_json")&&has(std::string(role)+"_file"))invalid(std::string(role)+" JSON and File are mutually exclusive");
  if(has("include_header")&&has("response_header"))invalid("include_header and response_header are mutually exclusive");
  if(has("context_options_json")||has("context_options_file")){
   if(has("include_header")||has("response_header"))invalid("complete context options conflict with scalar header controls");
   header=header_options(value(has("context_options_json")?"context_options_json":"context_options_file"));
  }else header=has("include_header")?header_text(value("include_header")):has("response_header")?header_text(value("response_header")):true;
  const bool any_marking=has("initial_marking_json")||has("initial_marking_file")||has("final_marking_json")||has("final_marking_file");
  if(variant!="petri_net"&&any_marking)invalid("marking controls require petri_net");
  phase="model_parse_error";
  if(detected=="auto"){
   if(variant!="petri_net")detected=variant=="declare"?"declare_json":variant=="log_skeleton"?"log_skeleton_json":"temporal_profile_json";
   else{const auto pos=original_bytes.find_first_not_of(" \t\r\n");if(pos!=std::string::npos&&original_bytes[pos]=='{'){const auto doc=Parser(original_bytes).parse();detected=doc.object().contains("net")?"native_document":"native_net";}else detected="pnml";}
  }
  if(variant=="declare"&&detected=="declare_json")model=parse_original_context_ordered_declare_json(original_bytes);
  else if(variant=="log_skeleton"&&detected=="log_skeleton_json")model=parse_original_context_skeleton_json(original_bytes);
  else if(variant=="temporal_profile"&&detected=="temporal_profile_json")model=parse_original_context_temporal_json(original_bytes);
  else if(variant=="petri_net"&&detected=="native_net")model=parse_original_context_net_json(original_bytes);
  else if(variant=="petri_net"&&detected=="native_document")model=parse_original_context_document_json(original_bytes);
  else if(variant=="petri_net"&&detected=="pnml"){
   imports=has("pnml_import_options_json")||has("pnml_import_options_file")?import_options(value(has("pnml_import_options_json")?"pnml_import_options_json":"pnml_import_options_file")):PnmlImportOptions{};
   model=parse_pnml_document(original_bytes,*imports);
  }else invalid("model_format is incompatible with original context variant");
  before=snapshot(*model);after=before;
  phase="configuration_error";
  if(detected!="pnml"&&(has("pnml_import_options_json")||has("pnml_import_options_file")))invalid("PNML import options require pnml model_format");
  if(variant=="petri_net"){
   const auto marking=[&](std::string_view role,std::optional<TokenMarking>& supplied,std::string& origin){const auto json=std::string(role)+"_marking_json",file=std::string(role)+"_marking_file";if(has(json)||has(file)){supplied=parse_original_context_marking_json(value(has(json)?json:file));origin=has(json)?"explicit_json":"explicit_file";}};
   marking("initial",initial_override,initial_origin);marking("final",final_override,final_origin);
   if(detected=="native_net"&&(!initial_override||!final_override))invalid("native_net requires explicit initial and final marking carriers");
   PnmlDocument document;
   if(const auto* net=std::get_if<PetriNet>(&*model))document.net=*net;else document=std::get<PnmlDocument>(*model);
   if(initial_override){document.initial_marking=*initial_override;document.initial_marking_keys.assign(initial_override->size(),true);}else initial_origin=detected=="pnml"?"pnml":"native_document";
   if(final_override){document.final_marking=*final_override;document.final_marking_keys.assign(final_override->size(),true);}else final_origin=detected=="pnml"?"pnml":"native_document";
   initial_before=document.initial_marking;final_before=document.final_marking;initial_after=initial_before;final_after=final_before;effective=std::move(document);
  }else effective=*model;
  effective_before=snapshot(*effective);effective_after=effective_before;
  phase="native_error";entered=true;
  std::visit([&](const auto& input){using T=std::decay_t<decltype(input)>;
   if constexpr(std::is_same_v<T,OrderedDeclareModel>)result=abstract_declare(input,*header);
   else if constexpr(std::is_same_v<T,LogSkeletonModel>)result=abstract_log_skeleton(input,*header);
   else if constexpr(std::is_same_v<T,TemporalProfile>)result=abstract_temporal_profile(input,*header);
   else if constexpr(std::is_same_v<T,PnmlDocument>)result=abstract_petri_net(input,*header);
   else throw std::logic_error("effective Petri context argument must retain its explicit markings");
  },*effective);returned=true;
 }catch(const std::exception& e){error=e.what();error_type=dynamic_cast<const std::out_of_range*>(&e)?"out_of_range":dynamic_cast<const std::invalid_argument*>(&e)?"invalid_argument":dynamic_cast<const std::length_error*>(&e)?"length_error":"native_error";}
 if(model)after=snapshot(*model);
 if(effective){effective_after=snapshot(*effective);if(const auto* doc=std::get_if<PnmlDocument>(&*effective)){initial_after=doc->initial_marking;final_after=doc->final_marking;}}
 const auto artifact=[&](const std::string& name,const auto& produce){try{artifacts[name]=produce();}catch(const std::exception& e){artifact_errors[name]=e.what();}};
 if(model){artifacts["source_model_json"]=before;artifacts["model_after_json"]=after;if(const auto* doc=std::get_if<PnmlDocument>(&*model))artifact("source_pnml",[&]{return to_pnml(*doc,"Original model context source");});}
 if(effective){artifacts["effective_model_json"]=effective_before;artifacts["effective_model_after_json"]=effective_after;if(const auto* doc=std::get_if<PnmlDocument>(&*effective))artifact("effective_pnml",[&]{return to_pnml(*doc,"Original model context effective input");});}
 if(initial_before)artifacts["initial_marking_json"]=render(*initial_before);if(final_before)artifacts["final_marking_json"]=render(*final_before);
 if(result){artifacts["native_text_bytes"]=result->text;artifact("legacy_result_json",[&]{return llm_context_json(*result);});}
 const auto opt_text=[&](const auto& map,const std::string& name){const auto it=map.find(name);return it==map.end()?std::string("null"):q(it->second);};
 const auto marking_text=[](const std::optional<TokenMarking>& value){return value?render(*value):std::string("null");};
 const auto nullable=[](const std::string& value){return value.empty()?std::string("null"):q(value);};
 std::string native_result="null",provider="null";if(result){native_result="{\"variant\":"+q(result->variant)+",\"text\":"+q(result->text)+",\"characters\":"+std::to_string(result->characters)+",\"truncated\":"+(result->truncated?"true":"false")+'}';const auto text=q(result->text);if(text.starts_with('"'))provider=text;}
 auto out=output();out<<"{\"family\":\"LLM_CONTEXT\",\"operation\":\"original_model\",\"variant\":"<<q(variant)<<",\"native_entered\":"<<(entered?"true":"false")<<",\"native_returned\":"<<(returned?"true":"false")<<",\"executed\":"<<(returned?"true":"false")<<",\"error\":"<<(error.empty()?"null":"{\"phase\":"+q(phase)+",\"type\":"+q(error_type)+",\"message\":"+q(error)+"}")<<",\"input_order\":"<<q(input_order)<<",\"input_context\":{\"model_format\":"<<q(format)<<",\"detected_format\":"<<q(detected)<<",\"model_type\":"<<(model?q(type_name(*model)):"null")<<",\"filename\":"<<q(filename)<<",\"original_model_bytes\":"<<original_bytes.size()<<"},\"original_model\":"<<q(original_bytes)<<",\"options\":"<<(header?"{\"include_header\":"+std::string(*header?"true":"false")+"}":"null")<<",\"pnml_import_options\":"<<(imports?render(*imports):"null")<<",\"snapshots\":{\"model_before\":"<<before<<",\"model_after\":"<<after<<",\"effective_model_before\":"<<effective_before<<",\"effective_model_after\":"<<effective_after<<"},\"model_unchanged\":"<<(model?(before==after?"true":"false"):"null")<<",\"effective_model_unchanged\":"<<(effective?(effective_before==effective_after?"true":"false"):"null")<<",\"markings\":{\"initial_before\":"<<marking_text(initial_before)<<",\"initial_after\":"<<marking_text(initial_after)<<",\"final_before\":"<<marking_text(final_before)<<",\"final_after\":"<<marking_text(final_after)<<",\"initial_origin\":"<<nullable(initial_origin)<<",\"final_origin\":"<<nullable(final_origin)<<",\"initial_override\":"<<marking_text(initial_override)<<",\"final_override\":"<<marking_text(final_override)<<",\"unchanged\":"<<(initial_before&&final_before?(*initial_before==*initial_after&&*final_before==*final_after?"true":"false"):"null")<<"},\"result\":"<<native_result<<",\"text_bytes\":"<<(result?std::to_string(result->text.size()):"null")<<",\"provider_text\":"<<provider<<",\"original_inputs\":"<<parts_json(parts)<<",\"query_inputs\":"<<parts_json(query)<<",\"artifacts\":{";
 bool comma=false;for(const auto* name:{"source_model_json","model_after_json","effective_model_json","effective_model_after_json","initial_marking_json","final_marking_json","native_text_bytes","legacy_result_json","source_pnml","effective_pnml"}){if(comma)out<<',';comma=true;out<<q(name)<<':'<<opt_text(artifacts,name)<<','<<q(std::string(name)+"_error")<<':'<<opt_text(artifact_errors,name);}out<<"}}";
 return {error.empty()?200:400,out.str()};
}
} // namespace minera
