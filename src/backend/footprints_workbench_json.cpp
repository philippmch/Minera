#include "minera/footprints_workbench_json.hpp"
#include "minera/declare_workbench_json.hpp"
#include "footprints/conformance_log_model.hpp"
#include "footprints/conformance_trace_extensive.hpp"
#include "footprints/conformance_tree_visualization.hpp"
#include <array>
#include <bit>
#include "minera/graph_alignment_workbench_json.hpp"

#include <algorithm>
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
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace minera {
namespace {
[[noreturn]] void invalid(std::string message) {
  throw std::invalid_argument("Footprints workbench JSON: " + message);
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

template <class T, class Convert>
std::vector<T> vector_value(const Json& value, Convert convert) {
  std::vector<T> result;
  result.reserve(value.array().size());
  for (const auto& item : value.array()) result.push_back(convert(item));
  return result;
}

void quoted(std::ostream& out, const std::string& value) {
  const auto array = serialize_alignment_trace_words_json({value});
  out.write(array.data() + 1, static_cast<std::streamsize>(array.size() - 2));
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

void set_supplied(const Json::Object& object, std::set<std::string>* supplied) {
  if (!supplied) return;
  std::set<std::string> actual;
  for (const auto& [key, value] : object) { (void)value; actual.insert(key); }
  *supplied = std::move(actual);
}

std::string json_text(const Json& value) {
  return std::visit([&](const auto& item)->std::string {
    using T=std::decay_t<decltype(item)>;
    if constexpr(std::is_same_v<T,std::nullptr_t>)return "null";
    else if constexpr(std::is_same_v<T,bool>)return item?"true":"false";
    else if constexpr(std::is_same_v<T,Number>)return item.text;
    else if constexpr(std::is_same_v<T,std::string>){auto out=output();quoted(out,item);return out.str();}
    else if constexpr(std::is_same_v<T,Json::Array>){std::string out="[";for(const auto& v:item){if(out.size()>1)out+=',';out+=json_text(v);}return out+']';}
    else {std::string out="{";for(const auto& [key,v]:item){if(out.size()>1)out+=',';out+=json_text(Json{key})+':'+json_text(v);}return out+'}';}
  },value.value);
}
FootprintPair pair_value(const Json& value) {const auto& o=value.object();allowed_fields(o,{"source","target"});return {required(o,"source").string(),required(o,"target").string()};}
void pair_json(std::ostream& out,const FootprintPair& value){out<<"{\"source\":";quoted(out,value.first);out<<",\"target\":";quoted(out,value.second);out<<'}';}
std::set<FootprintPair> pairs(const Json& value){std::set<FootprintPair> result;for(const auto& item:value.array())result.insert(pair_value(item));return result;}
std::set<std::string> strings(const Json& value){std::set<std::string> result;for(const auto& item:value.array())result.insert(item.string());return result;}
std::vector<std::string> words(const Json& value){return vector_value<std::string>(value,[](const Json& v){return v.string();});}
template<class T,class Write>void set_json(std::ostream& out,const std::set<T>& values,Write write){out<<'[';bool comma=false;for(const auto& v:values){if(comma)out<<',';comma=true;write(out,v);}out<<']';}
void pairs_json(std::ostream& out,const FootprintViolations& values){set_json(out,values,pair_json);}
void strings_json(std::ostream& out,const std::set<std::string>& values){set_json(out,values,quoted);}
Footprint footprint_value(const Json& value){
 const auto& o=value.object();allowed_fields(o,{"dfg","dfg_present","sequence","parallel","atomic_interleaving","start_activities","end_activities","start_activities_present","end_activities_present","activities","skippable","activities_always_happening","min_trace_length","min_trace_length_present","max_trace_length_without_loops","trace"});Footprint r;
 if(const auto* v=field(o,"dfg"))for(const auto& entry:v->array()){const auto& e=entry.object();allowed_fields(e,{"source","target","count"});const FootprintPair key{required(e,"source").string(),required(e,"target").string()};if(!r.dfg.emplace(key,unsigned_integer(required(e,"count"))).second)invalid("duplicate DFG map edge");}
 if(const auto* v=field(o,"dfg_present"))r.dfg_present=v->boolean();
 if(const auto* v=field(o,"sequence"))r.sequence=pairs(*v);
 if(const auto* v=field(o,"parallel"))r.parallel=pairs(*v);
 if(const auto* v=field(o,"atomic_interleaving"))r.atomic_interleaving=pairs(*v);
 if(const auto* v=field(o,"start_activities"))r.start_activities=strings(*v);
 if(const auto* v=field(o,"end_activities"))r.end_activities=strings(*v);
 if(const auto* v=field(o,"start_activities_present"))r.start_activities_present=v->boolean();
 if(const auto* v=field(o,"end_activities_present"))r.end_activities_present=v->boolean();
 if(const auto* v=field(o,"activities"))r.activities=strings(*v);
 if(const auto* v=field(o,"skippable"))r.skippable=v->null()?std::nullopt:std::optional<bool>{v->boolean()};
 if(const auto* v=field(o,"activities_always_happening"))r.activities_always_happening=v->null()?std::nullopt:std::optional<std::set<std::string>>{strings(*v)};
 if(const auto* v=field(o,"min_trace_length"))r.min_trace_length=v->null()?std::nullopt:std::optional<std::size_t>{native_unsigned<std::size_t>(*v)};
 if(const auto* v=field(o,"min_trace_length_present"))r.min_trace_length_present=v->boolean();
 if(const auto* v=field(o,"max_trace_length_without_loops"))r.max_trace_length_without_loops=v->null()?std::nullopt:std::optional<std::size_t>{native_unsigned<std::size_t>(*v)};
 if(const auto* v=field(o,"trace"))r.trace=v->null()?std::nullopt:std::optional<std::vector<std::string>>{words(*v)};
 return r;
}
void footprint_json(std::ostream& out,const Footprint& r){
 out<<"{\"dfg\":[";bool comma=false;for(const auto& [p,count]:r.dfg){if(comma)out<<',';comma=true;out<<"{\"source\":";quoted(out,p.first);out<<",\"target\":";quoted(out,p.second);out<<",\"count\":"<<count<<'}';}out<<"],\"dfg_present\":";boolean(out,r.dfg_present);
 out<<",\"sequence\":";pairs_json(out,r.sequence);out<<",\"parallel\":";pairs_json(out,r.parallel);out<<",\"atomic_interleaving\":";pairs_json(out,r.atomic_interleaving);
 out<<",\"start_activities\":";strings_json(out,r.start_activities);out<<",\"end_activities\":";strings_json(out,r.end_activities);
 out<<",\"start_activities_present\":";boolean(out,r.start_activities_present);out<<",\"end_activities_present\":";boolean(out,r.end_activities_present);out<<",\"activities\":";strings_json(out,r.activities);
 out<<",\"skippable\":";if(r.skippable)boolean(out,*r.skippable);else out<<"null";
 out<<",\"activities_always_happening\":";if(r.activities_always_happening)strings_json(out,*r.activities_always_happening);else out<<"null";
 out<<",\"min_trace_length\":";if(r.min_trace_length)out<<*r.min_trace_length;else out<<"null";
 out<<",\"min_trace_length_present\":";boolean(out,r.min_trace_length_present);
 out<<",\"max_trace_length_without_loops\":";if(r.max_trace_length_without_loops)out<<*r.max_trace_length_without_loops;else out<<"null";
 out<<",\"trace\":";if(r.trace)array_json(out,*r.trace,quoted);else out<<"null";out<<'}';
}
FootprintsExtensiveResult extensive_value(const Json& value){const auto& o=value.object();allowed_fields(o,{"footprints","start_activities","end_activities","activities_always_happening","min_length_fit","is_footprints_fit","is_fit"});FootprintsExtensiveResult r;r.footprints=pairs(required(o,"footprints"));r.start_activities=strings(required(o,"start_activities"));r.end_activities=strings(required(o,"end_activities"));r.activities_always_happening=strings(required(o,"activities_always_happening"));r.min_length_fit=required(o,"min_length_fit").boolean();r.is_footprints_fit=required(o,"is_footprints_fit").boolean();r.is_fit=required(o,"is_fit").boolean();return r;}
void extensive_json(std::ostream& out,const FootprintsExtensiveResult& r){out<<"{\"footprints\":";pairs_json(out,r.footprints);out<<",\"start_activities\":";strings_json(out,r.start_activities);out<<",\"end_activities\":";strings_json(out,r.end_activities);out<<",\"activities_always_happening\":";strings_json(out,r.activities_always_happening);out<<",\"min_length_fit\":";boolean(out,r.min_length_fit);out<<",\"is_footprints_fit\":";boolean(out,r.is_footprints_fit);out<<",\"is_fit\":";boolean(out,r.is_fit);out<<'}';}
std::vector<EventAttribute> attributes_value(const Json& value){return parse_declare_workbench_trace_json("{\"attributes\":"+json_text(value)+'}').attributes;}
std::string attributes_json(const std::vector<EventAttribute>& attributes){Trace holder;holder.attributes=attributes;const auto doc=Parser(serialize_declare_workbench_trace_json(holder),true,false).parse();return json_text(required(doc.object(),"attributes"));}
LogGlobal global_value(const Json& value){const auto& o=value.object();allowed_fields(o,{"scope","attributes"});LogGlobal r;if(const auto* v=field(o,"scope"))r.scope=v->string();if(const auto* v=field(o,"attributes"))r.attributes=attributes_value(*v);return r;}
void global_json(std::ostream& out,const LogGlobal& r){out<<"{\"scope\":";quoted(out,r.scope);out<<",\"attributes\":"<<attributes_json(r.attributes)<<'}';}
EventDataMetadata metadata_value(const Json& value){const auto& o=value.object();allowed_fields(o,{"extensions","classifiers","trace_globals","event_globals","properties","globals","compatibility_snapshot_valid","compatibility_globals_snapshot","compatibility_trace_globals_snapshot","compatibility_event_globals_snapshot"});EventDataMetadata r;
 if(const auto* v=field(o,"extensions"))for(const auto& x:v->array()){const auto& e=x.object();allowed_fields(e,{"name","prefix","uri"});LogExtension a;if(const auto* y=field(e,"name"))a.name=y->string();if(const auto* y=field(e,"prefix"))a.prefix=y->string();if(const auto* y=field(e,"uri"))a.uri=y->string();r.extensions.push_back(std::move(a));}
 if(const auto* v=field(o,"classifiers"))for(const auto& x:v->array()){const auto& e=x.object();allowed_fields(e,{"name","keys"});LogClassifier a;if(const auto* y=field(e,"name"))a.name=y->string();if(const auto* y=field(e,"keys"))a.keys=words(*y);r.classifiers.push_back(std::move(a));}
 if(const auto* v=field(o,"trace_globals"))r.trace_globals=attributes_value(*v);if(const auto* v=field(o,"event_globals"))r.event_globals=attributes_value(*v);if(const auto* v=field(o,"properties"))r.properties=attributes_value(*v);
 if(const auto* v=field(o,"globals"))r.globals=vector_value<LogGlobal>(*v,global_value);if(const auto* v=field(o,"compatibility_snapshot_valid"))r.compatibility_snapshot_valid=v->boolean();if(const auto* v=field(o,"compatibility_globals_snapshot"))r.compatibility_globals_snapshot=vector_value<LogGlobal>(*v,global_value);if(const auto* v=field(o,"compatibility_trace_globals_snapshot"))r.compatibility_trace_globals_snapshot=attributes_value(*v);if(const auto* v=field(o,"compatibility_event_globals_snapshot"))r.compatibility_event_globals_snapshot=attributes_value(*v);return r;
}
void metadata_json(std::ostream& out,const EventDataMetadata& r){out<<"{\"extensions\":[";for(std::size_t i=0;i<r.extensions.size();++i){if(i)out<<',';const auto& x=r.extensions[i];out<<"{\"name\":";quoted(out,x.name);out<<",\"prefix\":";quoted(out,x.prefix);out<<",\"uri\":";quoted(out,x.uri);out<<'}';}out<<"],\"classifiers\":[";for(std::size_t i=0;i<r.classifiers.size();++i){if(i)out<<',';out<<"{\"name\":";quoted(out,r.classifiers[i].name);out<<",\"keys\":";array_json(out,r.classifiers[i].keys,quoted);out<<'}';}out<<"],\"trace_globals\":"<<attributes_json(r.trace_globals)<<",\"event_globals\":"<<attributes_json(r.event_globals)<<",\"properties\":"<<attributes_json(r.properties)<<",\"globals\":";array_json(out,r.globals,global_json);out<<",\"compatibility_snapshot_valid\":";boolean(out,r.compatibility_snapshot_valid);out<<",\"compatibility_globals_snapshot\":";array_json(out,r.compatibility_globals_snapshot,global_json);out<<",\"compatibility_trace_globals_snapshot\":"<<attributes_json(r.compatibility_trace_globals_snapshot)<<",\"compatibility_event_globals_snapshot\":"<<attributes_json(r.compatibility_event_globals_snapshot)<<'}';}
}  // namespace
Footprint parse_footprints_workbench_footprint_json(std::string_view input){return footprint_value(Parser(input).parse());}
std::vector<Footprint> parse_footprints_workbench_footprints_json(std::string_view input){return vector_value<Footprint>(Parser(input).parse(),footprint_value);}
FootprintsExtensiveResult parse_footprints_workbench_extensive_json(std::string_view input){return extensive_value(Parser(input).parse());}
std::vector<FootprintsExtensiveResult> parse_footprints_workbench_extensives_json(std::string_view input){return vector_value<FootprintsExtensiveResult>(Parser(input).parse(),extensive_value);}
FootprintViolations parse_footprints_workbench_relations_json(std::string_view input){return pairs(Parser(input).parse());}
std::vector<FootprintViolations> parse_footprints_workbench_violations_json(std::string_view input){return vector_value<FootprintViolations>(Parser(input).parse(),pairs);}
FootprintsWorkbenchDecorationSets parse_footprints_workbench_decoration_json(std::string_view input){const auto doc=Parser(input).parse();const auto& o=doc.object();allowed_fields(o,{"relations","start_activities","end_activities","activities_always_happening"});return {pairs(required(o,"relations")),strings(required(o,"start_activities")),strings(required(o,"end_activities")),strings(required(o,"activities_always_happening"))};}
std::string serialize_footprints_workbench_json(const Footprint& v){auto out=output();footprint_json(out,v);return out.str();}
std::string serialize_footprints_workbench_json(const std::vector<Footprint>& v){auto out=output();array_json(out,v,footprint_json);return out.str();}
std::string serialize_footprints_workbench_json(const FootprintsExtensiveResult& v){auto out=output();extensive_json(out,v);return out.str();}
std::string serialize_footprints_workbench_json(const std::vector<FootprintsExtensiveResult>& v){auto out=output();array_json(out,v,extensive_json);return out.str();}
std::string serialize_footprints_workbench_json(const FootprintViolations& v){auto out=output();pairs_json(out,v);return out.str();}
std::string serialize_footprints_workbench_json(const std::vector<FootprintViolations>& v){auto out=output();array_json(out,v,pairs_json);return out.str();}
std::string serialize_footprints_workbench_json(const std::set<std::string>& v){auto out=output();strings_json(out,v);return out.str();}
std::string serialize_footprints_workbench_json(const FootprintsWorkbenchDecorationSets& v){auto out=output();out<<"{\"relations\":";pairs_json(out,v.relations);out<<",\"start_activities\":";strings_json(out,v.start_activities);out<<",\"end_activities\":";strings_json(out,v.end_activities);out<<",\"activities_always_happening\":";strings_json(out,v.activities_always_happening);out<<'}';return out.str();}
std::string serialize_footprints_workbench_json(double v){auto out=output();number(out,v);return out.str();}
std::string serialize_footprints_workbench_json(const FootprintsFitnessSummary& v){auto out=output();out<<"{\"perc_fit_traces\":";number(out,v.perc_fit_traces);out<<",\"log_fitness\":";number(out,v.log_fitness);out<<'}';return out.str();}
std::string serialize_footprints_workbench_json(const std::vector<footprints_conformance::LogModelDiagnostic>& values){auto out=output();array_json(out,values,[](std::ostream& o,const auto& v){o<<"{\"case_id\":";quoted(o,v.case_id);o<<",\"num_violations\":"<<v.num_violations<<",\"is_fit\":";boolean(o,v.is_fit);o<<'}';});return out.str();}
std::string serialize_footprints_workbench_json(const std::vector<footprints_conformance::TraceExtensiveDiagnostic>& values){auto out=output();array_json(out,values,[](std::ostream& o,const auto& v){o<<"{\"case_id\":";quoted(o,v.case_id);o<<",\"footprints_violations\":"<<v.footprints_violations<<",\"start_activities_violations\":"<<v.start_activities_violations<<",\"end_activities_violations\":"<<v.end_activities_violations<<",\"act_always_happening_violations\":"<<v.act_always_happening_violations<<",\"min_length_fit\":";boolean(o,v.min_length_fit);o<<",\"is_fit\":";boolean(o,v.is_fit);o<<'}';});return out.str();}
std::string serialize_footprints_workbench_json(const footprints_conformance::FootprintProjection& v){auto out=output();out<<"{\"relations\":";pairs_json(out,v.relations);out<<",\"activities\":";strings_json(out,v.activities);out<<",\"start_activities\":";strings_json(out,v.start_activities);out<<",\"end_activities\":";strings_json(out,v.end_activities);out<<'}';return out.str();}
std::string serialize_footprints_workbench_json(const std::vector<footprints_conformance::TreeNodeDecoration>& values){auto out=output();array_json(out,values,[](std::ostream& o,const auto& v){o<<"{\"preorder_index\":"<<v.preorder_index<<",\"operation_index\":"<<static_cast<int>(v.operation)<<",\"activity\":";quoted(o,v.activity);o<<",\"color\":";quoted(o,v.color);o<<'}';});return out.str();}
FootprintsLogModelOptions parse_footprints_workbench_log_model_options(std::string_view input,std::set<std::string>* supplied){const auto doc=Parser(input).parse();const auto& o=doc.object();allowed_fields(o,{"strict","case_id_attribute"});FootprintsLogModelOptions r;if(const auto* v=field(o,"strict"))r.strict=v->boolean();if(const auto* v=field(o,"case_id_attribute"))r.case_id_attribute=v->string();set_supplied(o,supplied);return r;}
FootprintsTraceExtensiveOptions parse_footprints_workbench_trace_extensive_options(std::string_view input,std::set<std::string>* supplied){const auto doc=Parser(input).parse();const auto& o=doc.object();allowed_fields(o,{"enable_act_always_executed","case_id_attribute"});FootprintsTraceExtensiveOptions r;if(const auto* v=field(o,"enable_act_always_executed"))r.enable_act_always_executed=v->boolean();if(const auto* v=field(o,"case_id_attribute"))r.case_id_attribute=v->string();set_supplied(o,supplied);return r;}
FootprintsWorkbenchDiagnosticsOptions parse_footprints_workbench_diagnostics_options(std::string_view input,std::set<std::string>* supplied){const auto doc=Parser(input).parse();const auto& o=doc.object();allowed_fields(o,{"case_id_key"});FootprintsWorkbenchDiagnosticsOptions r;if(const auto* v=field(o,"case_id_key"))r.case_id_key=v->string();set_supplied(o,supplied);return r;}
FootprintsWorkbenchCompareOptions parse_footprints_workbench_compare_options(std::string_view input,std::set<std::string>* supplied){const auto doc=Parser(input).parse();const auto& o=doc.object();allowed_fields(o,{"check_act_always_executed"});FootprintsWorkbenchCompareOptions r;if(const auto* v=field(o,"check_act_always_executed"))r.check_act_always_executed=v->boolean();set_supplied(o,supplied);return r;}
FootprintsDiscoveryOptions parse_footprints_workbench_discovery_options(std::string_view input,std::set<std::string>* supplied){const auto doc=Parser(input).parse();const auto& o=doc.object();allowed_fields(o,{"activity_attribute","case_id_attribute","timestamp_attribute","start_timestamp_attribute","index_attribute","sort_required","initial_marking"});FootprintsDiscoveryOptions r;if(const auto* v=field(o,"activity_attribute"))r.activity_attribute=v->string();if(const auto* v=field(o,"case_id_attribute"))r.case_id_attribute=v->string();if(const auto* v=field(o,"timestamp_attribute"))r.timestamp_attribute=v->string();if(const auto* v=field(o,"start_timestamp_attribute"))r.start_timestamp_attribute=v->null()?std::nullopt:std::optional<std::string>{v->string()};if(const auto* v=field(o,"index_attribute"))r.index_attribute=v->string();if(const auto* v=field(o,"sort_required"))r.sort_required=v->boolean();if(const auto* v=field(o,"initial_marking"))r.initial_marking=v->null()?std::nullopt:std::optional<std::vector<std::uint64_t>>{vector_value<std::uint64_t>(*v,unsigned_integer)};set_supplied(o,supplied);return r;}
std::string serialize_footprints_workbench_options_json(const FootprintsLogModelOptions& v){auto out=output();out<<"{\"strict\":";boolean(out,v.strict);out<<",\"case_id_attribute\":";quoted(out,v.case_id_attribute);out<<'}';return out.str();}
std::string serialize_footprints_workbench_options_json(const FootprintsTraceExtensiveOptions& v){auto out=output();out<<"{\"enable_act_always_executed\":";boolean(out,v.enable_act_always_executed);out<<",\"case_id_attribute\":";quoted(out,v.case_id_attribute);out<<'}';return out.str();}
std::string serialize_footprints_workbench_options_json(const FootprintsWorkbenchDiagnosticsOptions& v){auto out=output();out<<"{\"case_id_key\":";quoted(out,v.case_id_key);out<<'}';return out.str();}
std::string serialize_footprints_workbench_options_json(const FootprintsWorkbenchCompareOptions& v){return std::string("{\"check_act_always_executed\":")+(v.check_act_always_executed?"true}":"false}");}
std::string serialize_footprints_workbench_options_json(const FootprintsDiscoveryOptions& v){auto out=output();out<<"{\"activity_attribute\":";quoted(out,v.activity_attribute);out<<",\"case_id_attribute\":";quoted(out,v.case_id_attribute);out<<",\"timestamp_attribute\":";quoted(out,v.timestamp_attribute);out<<",\"start_timestamp_attribute\":";if(v.start_timestamp_attribute)quoted(out,*v.start_timestamp_attribute);else out<<"null";out<<",\"index_attribute\":";quoted(out,v.index_attribute);out<<",\"sort_required\":";boolean(out,v.sort_required);out<<",\"initial_marking\":";if(v.initial_marking)array_json(out,*v.initial_marking,integer<std::uint64_t>);else out<<"null";out<<'}';return out.str();}
EventLog parse_footprints_workbench_raw_log_json(std::string_view input){const auto doc=Parser(input).parse();const auto& o=doc.object();allowed_fields(o,{"attributes","metadata","traces"});EventLog r;if(const auto* v=field(o,"attributes"))r.attributes=attributes_value(*v);if(const auto* v=field(o,"metadata"))r.metadata=metadata_value(*v);if(const auto* v=field(o,"traces"))r.traces=vector_value<Trace>(*v,[](const Json& x){return parse_declare_workbench_trace_json(json_text(x));});return r;}
std::string serialize_footprints_workbench_raw_log_json(const EventLog& v){auto out=output();out<<"{\"attributes\":"<<attributes_json(v.attributes)<<",\"metadata\":";metadata_json(out,v.metadata);out<<",\"traces\":[";for(std::size_t i=0;i<v.traces.size();++i){if(i)out<<',';out<<serialize_declare_workbench_trace_json(v.traces[i]);}out<<"]}";return out.str();}
std::set<std::string> footprints_workbench_supplied_members(std::string_view input){const auto doc=Parser(input).parse();std::set<std::string> r;if(const auto* o=std::get_if<Json::Object>(&doc.value))for(const auto& [key,value]:*o){(void)value;r.insert(key);}return r;}
std::string footprints_workbench_sha256(std::string_view input){
 constexpr std::array<std::uint32_t,64> k={0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
 std::array<std::uint32_t,8> h={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};std::string bytes(input);bytes.push_back(static_cast<char>(0x80));while(bytes.size()%64!=56)bytes.push_back(0);const auto bits=static_cast<std::uint64_t>(input.size())*8;for(int shift=56;shift>=0;shift-=8)bytes.push_back(static_cast<char>((bits>>shift)&255));
 for(std::size_t offset=0;offset<bytes.size();offset+=64){std::array<std::uint32_t,64>w{};for(std::size_t i=0;i<16;++i)for(std::size_t j=0;j<4;++j)w[i]=(w[i]<<8)|static_cast<unsigned char>(bytes[offset+i*4+j]);for(std::size_t i=16;i<64;++i){const auto a=std::rotr(w[i-15],7)^std::rotr(w[i-15],18)^(w[i-15]>>3);const auto b=std::rotr(w[i-2],17)^std::rotr(w[i-2],19)^(w[i-2]>>10);w[i]=w[i-16]+a+w[i-7]+b;}auto a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],j=h[7];for(std::size_t i=0;i<64;++i){const auto s1=std::rotr(e,6)^std::rotr(e,11)^std::rotr(e,25);const auto ch=(e&f)^((~e)&g);const auto t1=j+s1+ch+k[i]+w[i];const auto s0=std::rotr(a,2)^std::rotr(a,13)^std::rotr(a,22);const auto maj=(a&b)^(a&c)^(b&c);const auto t2=s0+maj;j=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;}h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=j;}
 constexpr char hex[]="0123456789abcdef";std::string result;result.reserve(64);for(const auto value:h)for(int shift=28;shift>=0;shift-=4)result+=hex[(value>>shift)&15];return result;
}
}  // namespace minera
