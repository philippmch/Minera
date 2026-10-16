#include "minera/emd_workbench_json.hpp"
#include "minera/embedding_workbench_json.hpp"
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
#include <utility>
#include <variant>
namespace minera { namespace {
[[noreturn]] void invalid(std::string message) {
  throw std::invalid_argument("EMD workbench JSON: " + message);
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

void quoted(std::ostream& out,const std::string& value){out<<serialize_embedding_workbench_string_json(value);}

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


std::int32_t signed_index(const Json& value) {
  const auto* token = std::get_if<Number>(&value.value);
  if (!token || token->text.empty()) invalid("expected a signed int32 JSON integer token");
  const auto& text = token->text;
  const auto start = text.front() == '-' ? std::size_t{1} : std::size_t{0};
  if (start == text.size() || !std::all_of(text.begin() + start, text.end(),
      [](char c) { return c >= '0' && c <= '9'; }))
    invalid("expected a signed int32 JSON integer token");
  std::int32_t result = 0;
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
  if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
    invalid("variant index exceeds int32 storage");
  return result;
}


void supplied_members(const Json::Object& object,std::set<std::string>* supplied) {
  if(!supplied)return;
  std::set<std::string> members;
  for(const auto& [name,unused]:object){(void)unused;members.insert(name);}
  *supplied=std::move(members);
}
const Json& required(const Json::Object& object,std::string_view name){
  if(const auto* v=field(object,name))return *v;
  invalid("missing field "+std::string(name));
}
double real_value(const Json& value){
  if(const auto* tag=std::get_if<std::string>(&value.value)){
    if(*tag=="NaN")return std::numeric_limits<double>::quiet_NaN();
    if(*tag=="INF")return std::numeric_limits<double>::infinity();
    if(*tag=="-INF")return -std::numeric_limits<double>::infinity();
    invalid("unknown IEEE double tag");
  }
  const auto* token=std::get_if<Number>(&value.value);
  if(!token)invalid("expected numeric double or IEEE tag");
  double result=0.0;
  const auto parsed=std::from_chars(token->text.data(),token->text.data()+token->text.size(),result);
  if(parsed.ec!=std::errc{}||parsed.ptr!=token->text.data()+token->text.size()||!std::isfinite(result))
    invalid("numeric token exceeds finite double storage");
  return result;
}
std::vector<double> real_vector(const Json& value){return vector_value<double>(value,real_value);}
void real_json(std::ostream& out,double value){
  if(std::isnan(value))out<<"\"NaN\"";
  else if(std::isinf(value))out<<(value<0?"\"-INF\"":"\"INF\"");
  else if(value==0&&std::signbit(value))out<<"-0.0";
  else{auto scalar=output();scalar<<std::setprecision(std::numeric_limits<double>::max_digits10)<<value;auto text=scalar.str();if(text.find_first_of(".eE")==std::string::npos)text+=".0";out<<text;}
}

std::string native_string(const Json& value){
 if(const auto* text=std::get_if<std::string>(&value.value))return *text;
 const auto& o=value.object();allowed_fields(o,{"native_bytes","data"});
 if(required(o,"native_bytes").string()!="hex-v1")invalid("unknown native byte-string encoding");
 const auto& hex=required(o,"data").string();if(hex.size()%2)invalid("odd hex byte-string length");
 const auto digit=[](char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;};
 std::string bytes;bytes.reserve(hex.size()/2);
 for(std::size_t i=0;i<hex.size();i+=2){const int a=digit(hex[i]),b=digit(hex[i+1]);if(a<0||b<0)invalid("invalid hex byte-string digit");bytes+=static_cast<char>((a<<4)|b);}
 return bytes;
}
std::string_view variant_name(std::int32_t value){return value==0?"exact":value==1?"pyemd":"";}
std::int32_t variant_value(const Json& value){
 if(const auto* text=std::get_if<std::string>(&value.value)){if(*text=="exact")return 0;if(*text=="pyemd")return 1;invalid("unknown EMD variant name");}
 if(const auto* o=std::get_if<Json::Object>(&value.value)){allowed_fields(*o,{"index","name"});const auto index=signed_index(required(*o,"index"));if(const auto* name=field(*o,"name")){const auto expected=variant_name(index);if(expected.empty()){if(!name->null())invalid("unknown EMD index requires null name");}else if(name->string()!=expected)invalid("EMD enum name metadata differs from index");}return index;}
 return signed_index(value);
}
void variant_json(std::ostream& out,EmdVariant variant){const auto index=static_cast<std::int32_t>(variant);const auto name=variant_name(index);out<<"{\"name\":";if(name.empty())out<<"null";else quoted(out,std::string(name));out<<",\"index\":"<<index<<'}';}
StochasticLanguage language_value(const Json& value){
 const auto& o=value.object();allowed_fields(o,{"traces","total_mass"});StochasticLanguage result;
 if(const auto* traces=field(o,"traces"))for(const auto& item:traces->array()){
  const auto& t=item.object();allowed_fields(t,{"activities","weight"});WeightedTrace trace;
  if(const auto* activities=field(t,"activities"))trace.activities=vector_value<std::string>(*activities,native_string);
  if(const auto* weight=field(t,"weight"))trace.weight=real_value(*weight);
  result.traces.push_back(std::move(trace));
 }
 if(const auto* mass=field(o,"total_mass"))result.total_mass=real_value(*mass);
 return result;
}
void language_json(std::ostream& out,const StochasticLanguage& value){
 out<<"{\"traces\":";array_json(out,value.traces,[](auto& target,const WeightedTrace& trace){target<<"{\"activities\":";array_json(target,trace.activities,quoted);target<<",\"weight\":";real_json(target,trace.weight);target<<'}';});out<<",\"total_mass\":";real_json(out,value.total_mass);out<<'}';
}
constexpr std::array<std::string_view,4> slot_names={"distance","encoded_distance","string_distance","utf8_string_distance"};
EmdWorkbenchCallbackKey key_value(const Json& value,std::size_t slot){
 if(slot==0)return vector_value<std::string>(value,native_string);
 if(slot==3)return native_string(value);
 std::u32string result;for(const auto& item:value.array())result.push_back(static_cast<char32_t>(native_unsigned<std::uint32_t>(item)));return result;
}
void key_json(std::ostream& out,const EmdWorkbenchCallbackKey& key){
 if(const auto* original=std::get_if<std::vector<std::string>>(&key))array_json(out,*original,quoted);
 else if(const auto* encoded=std::get_if<std::u32string>(&key)){out<<'[';for(std::size_t i=0;i<encoded->size();++i){if(i)out<<',';out<<static_cast<std::uint32_t>((*encoded)[i]);}out<<']';}
 else quoted(out,std::get<std::string>(key));
}
EmdWorkbenchCallbackOutcome outcome_value(const Json& value){
 const auto& o=value.object();allowed_fields(o,{"value","throw"});const auto* result=field(o,"value");const auto* error=field(o,"throw");if(bool(result)==bool(error))invalid("callback outcome requires exactly one value or throw member");if(result)return real_value(*result);return native_string(*error);
}
void outcome_json(std::ostream& out,const EmdWorkbenchCallbackOutcome& value){if(const auto* result=std::get_if<double>(&value)){out<<"{\"value\":";real_json(out,*result);}else{out<<"{\"throw\":";quoted(out,std::get<std::string>(value));}out<<'}';}
EmdWorkbenchCallbackTable table_value(const Json& value,std::size_t slot){
 const auto& o=value.object();allowed_fields(o,{"entries","fallback"});EmdWorkbenchCallbackTable result;
 if(const auto* entries=field(o,"entries"))for(const auto& item:entries->array()){const auto& e=item.object();allowed_fields(e,{"left","right","outcome"});result.entries.push_back({key_value(required(e,"left"),slot),key_value(required(e,"right"),slot),outcome_value(required(e,"outcome"))});}
 if(const auto* fallback=field(o,"fallback");fallback&&!fallback->null())result.fallback=outcome_value(*fallback);
 return result;
}
void table_json(std::ostream& out,const EmdWorkbenchCallbackTable& table){out<<"{\"entries\":";array_json(out,table.entries,[](auto& target,const EmdWorkbenchCallbackEntry& entry){target<<"{\"left\":";key_json(target,entry.left);target<<",\"right\":";key_json(target,entry.right);target<<",\"outcome\":";outcome_json(target,entry.outcome);target<<'}';});out<<",\"fallback\":";if(table.fallback)outcome_json(out,*table.fallback);else out<<"null";out<<'}';}
double adapter_call(const EmdWorkbenchCallbackTables& tables,std::size_t slot,EmdWorkbenchCallbackKey left,EmdWorkbenchCallbackKey right,const std::shared_ptr<std::vector<EmdWorkbenchCallbackInvocation>>& trace){
 const auto& table=*tables.slots[slot];EmdWorkbenchCallbackInvocation call;call.slot=slot;call.left=std::move(left);call.right=std::move(right);
 for(std::size_t i=0;i<table.entries.size();++i)if(table.entries[i].left==call.left&&table.entries[i].right==call.right){call.entry_index=i;call.outcome=table.entries[i].outcome;break;}
 if(!call.entry_index){if(table.fallback){call.used_fallback=true;call.outcome=*table.fallback;}else{call.missing_entry=true;call.outcome="EMD finite cost-table adapter missing entry for "+std::string(slot_names[slot]);}}
 if(trace)trace->push_back(call);
 if(const auto* value=std::get_if<double>(&call.outcome))return *value;
 throw std::runtime_error(std::get<std::string>(call.outcome));
}
} // namespace

StochasticLanguage parse_emd_workbench_language_json(std::string_view text,std::set<std::string>* supplied){const auto value=Parser(text).parse();auto result=language_value(value);supplied_members(value.object(),supplied);return result;}
std::string serialize_emd_workbench_language_json(const StochasticLanguage& value){auto out=output();language_json(out,value);return out.str();}
EmdOptions parse_emd_workbench_options_json(std::string_view text,std::set<std::string>* supplied){
 const auto value=Parser(text).parse();const auto& o=value.object();allowed_fields(o,{"normalize_masses","strict_equal_mass","variant","use_fast_emd"});EmdOptions result;
 if(const auto* v=field(o,"normalize_masses"))result.normalize_masses=v->boolean();if(const auto* v=field(o,"strict_equal_mass"))result.strict_equal_mass=v->boolean();if(const auto* v=field(o,"variant"))result.variant=static_cast<EmdVariant>(variant_value(*v));if(const auto* v=field(o,"use_fast_emd"))result.use_fast_emd=v->boolean();supplied_members(o,supplied);return result;
}
std::string serialize_emd_workbench_options_json(const EmdOptions& value){auto out=output();out<<"{\"normalize_masses\":";boolean(out,value.normalize_masses);out<<",\"strict_equal_mass\":";boolean(out,value.strict_equal_mass);out<<",\"variant\":";variant_json(out,value.variant);out<<",\"use_fast_emd\":";boolean(out,value.use_fast_emd);out<<'}';return out.str();}
std::string serialize_emd_workbench_callback_presence_json(const EmdOptions& value){auto out=output();out<<"{\"distance\":";boolean(out,bool(value.distance));out<<",\"encoded_distance\":";boolean(out,bool(value.encoded_distance));out<<",\"string_distance\":";boolean(out,bool(value.string_distance));out<<",\"utf8_string_distance\":";boolean(out,bool(value.utf8_string_distance));out<<'}';return out.str();}
EmdResult parse_emd_workbench_result_json(std::string_view text){const auto value=Parser(text).parse();const auto& o=value.object();allowed_fields(o,{"distance","computed","normalized","error","variant","use_fast_emd"});EmdResult result;if(const auto* v=field(o,"distance"))result.distance=real_value(*v);if(const auto* v=field(o,"computed"))result.computed=v->boolean();if(const auto* v=field(o,"normalized"))result.normalized=v->boolean();if(const auto* v=field(o,"error"))result.error=native_string(*v);if(const auto* v=field(o,"variant"))result.variant=static_cast<EmdVariant>(variant_value(*v));if(const auto* v=field(o,"use_fast_emd"))result.use_fast_emd=v->boolean();return result;}
std::string serialize_emd_workbench_result_json(const EmdResult& value){auto out=output();out<<"{\"distance\":";real_json(out,value.distance);out<<",\"computed\":";boolean(out,value.computed);out<<",\"normalized\":";boolean(out,value.normalized);out<<",\"error\":";quoted(out,value.error);out<<",\"variant\":";variant_json(out,value.variant);out<<",\"use_fast_emd\":";boolean(out,value.use_fast_emd);out<<'}';return out.str();}
std::string serialize_emd_workbench_scalar_json(double value){auto out=output();real_json(out,value);return out.str();}
bool parse_emd_workbench_normalize_json(std::string_view text){return Parser(text).parse().boolean();}
EmdWorkbenchCallbackTables parse_emd_workbench_callback_tables_json(std::string_view text,std::set<std::string>* supplied){const auto value=Parser(text).parse();const auto& o=value.object();allowed_fields(o,{"distance","encoded_distance","string_distance","utf8_string_distance"});EmdWorkbenchCallbackTables result;for(std::size_t slot=0;slot<slot_names.size();++slot)if(const auto* table=field(o,slot_names[slot]);table&&!table->null())result.slots[slot]=table_value(*table,slot);supplied_members(o,supplied);return result;}
std::string serialize_emd_workbench_callback_tables_json(const EmdWorkbenchCallbackTables& value){auto out=output();out<<'{';for(std::size_t slot=0;slot<slot_names.size();++slot){if(slot)out<<',';quoted(out,std::string(slot_names[slot]));out<<':';if(value.slots[slot])table_json(out,*value.slots[slot]);else out<<"null";}out<<'}';return out.str();}
std::string serialize_emd_workbench_callback_invocations_json(const std::vector<EmdWorkbenchCallbackInvocation>& values){auto out=output();array_json(out,values,[](auto& target,const EmdWorkbenchCallbackInvocation& value){target<<"{\"slot\":";quoted(target,std::string(slot_names.at(value.slot)));target<<",\"left\":";key_json(target,value.left);target<<",\"right\":";key_json(target,value.right);target<<",\"entry_index\":";if(value.entry_index)target<<*value.entry_index;else target<<"null";target<<",\"used_fallback\":";boolean(target,value.used_fallback);target<<",\"missing_entry\":";boolean(target,value.missing_entry);target<<",\"outcome\":";outcome_json(target,value.outcome);target<<'}';});return out.str();}
void bind_emd_workbench_callback_tables(EmdOptions& options,const EmdWorkbenchCallbackTables& tables,std::shared_ptr<std::vector<EmdWorkbenchCallbackInvocation>> trace){
 const auto owned=std::make_shared<const EmdWorkbenchCallbackTables>(tables);
 options.distance={};options.encoded_distance={};options.string_distance={};options.utf8_string_distance={};
 if(tables.slots[0])options.distance=[owned,trace](const auto& left,const auto& right){return adapter_call(*owned,0,left,right,trace);};
 if(tables.slots[1])options.encoded_distance=[owned,trace](const auto& left,const auto& right){return adapter_call(*owned,1,left,right,trace);};
 if(tables.slots[2])options.string_distance=[owned,trace](const auto& left,const auto& right){return adapter_call(*owned,2,left,right,trace);};
 if(tables.slots[3])options.utf8_string_distance=[owned,trace](const auto& left,const auto& right){return adapter_call(*owned,3,left,right,trace);};
}
} // namespace minera
