#include "minera/original_label_splitting_workbench_json.hpp"
#include "minera/exact_filter_discovery_json.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/literal_controls_json.hpp"
#include "minera/json_export.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <locale>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>

namespace minera { namespace {
[[noreturn]] void invalid(std::string message) {
  throw std::invalid_argument("Original label splitting JSON: " + message);
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
  bool escaped_lone_surrogates_ = true;
  std::size_t position_ = 0;
};

const auto q = serialize_embedding_workbench_string_json;
const Json& required(const Json::Object& object, std::string_view key) {
  const auto found = object.find(key);
  if (found == object.end()) invalid("missing field " + std::string(key));
  return found->second;
}
void exact_fields(const Json::Object& object,
                  std::initializer_list<std::string_view> names) {
  for (const auto& [key, value] : object) {
    (void)value;
    if (std::find(names.begin(), names.end(), key) == names.end())
      invalid("unknown field " + key);
  }
  for (const auto name : names) (void)required(object, name);
}
std::string raw_json(const Json& value) {
  if (value.null()) return "null";
  if (const auto* v = std::get_if<bool>(&value.value)) return *v ? "true" : "false";
  if (const auto* v = std::get_if<Number>(&value.value)) return v->text;
  if (const auto* v = std::get_if<std::string>(&value.value)) return q(*v);
  std::string out;
  if (const auto* values = std::get_if<Json::Array>(&value.value)) {
    out = "[";
    for (const auto& item : *values) {
      if (out.size() > 1) out += ',';
      out += raw_json(item);
    }
    return out + ']';
  }
  out = "{";
  for (const auto& [key, item] : value.object()) {
    if (out.size() > 1) out += ',';
    out += q(key) + ':' + raw_json(item);
  }
  return out + '}';
}
template<class Integer> Integer integer(const Json& value) {
  Number string_token;
  const auto* token = std::get_if<Number>(&value.value);
  if (const auto* text = std::get_if<std::string>(&value.value)) {
    string_token.text = *text;
    token = &string_token;
  }
  if (!token || token->text.empty() || token->text.find_first_of(".eE") != std::string::npos)
    invalid("expected an exact native decimal integer");
  if constexpr (std::is_unsigned_v<Integer>) {
    if (!std::all_of(token->text.begin(), token->text.end(),
        [](char c) { return c >= '0' && c <= '9'; }))
      invalid("expected unsigned decimal integer token");
  }
  Integer result{};
  const auto parsed = std::from_chars(token->text.data(),
      token->text.data() + token->text.size(), result);
  if (parsed.ec != std::errc{} || parsed.ptr != token->text.data() + token->text.size())
    invalid("integer exceeds native storage range");
  return result;
}
double real(const Json& value) {
  if (const auto* tag = std::get_if<std::string>(&value.value)) {
    if (*tag == "NaN") return std::numeric_limits<double>::quiet_NaN();
    if (*tag == "INF") return std::numeric_limits<double>::infinity();
    if (*tag == "-INF") return -std::numeric_limits<double>::infinity();
    invalid("unknown IEEE double tag");
  }
  const auto* token = std::get_if<Number>(&value.value);
  if (!token) invalid("expected double token or IEEE tag");
  double result{};
  const auto parsed = std::from_chars(token->text.data(),
      token->text.data() + token->text.size(), result, std::chars_format::general);
  if (parsed.ec == std::errc{} && parsed.ptr == token->text.data() + token->text.size())
    return result;
  if (parsed.ec == std::errc::result_out_of_range) {
    std::istringstream input(token->text);
    input.imbue(std::locale::classic());
    input >> result;
    if (result == 0.0 && input.eof())
      return token->text.front() == '-' ? -0.0 : 0.0;
  }
  invalid("double token exceeds finite native storage; use an explicit IEEE tag");
}
LabelSplittingVariant enum_value(const Json& value) {
  if (!std::holds_alternative<Json::Object>(value.value))
    return static_cast<LabelSplittingVariant>(integer<std::int32_t>(value));
  const auto& object=value.object();
  exact_fields(object,{"name","index"});
  const auto index=integer<std::int32_t>(required(object,"index"));
  const auto& name=required(object,"name");
  if (index==0 ? name.null() || name.string()!="contextual" : !name.null())
    invalid("variant name must match its native index");
  return static_cast<LabelSplittingVariant>(index);
}
std::string enum_json(LabelSplittingVariant variant) {
  const auto index=static_cast<std::int32_t>(variant);
  return "{\"name\":"+(index==0?q("contextual"):std::string("null"))+
      ",\"index\":"+std::to_string(index)+'}';
}
std::string real_json(double value) {
  if (std::isnan(value)) return q("NaN");
  if (std::isinf(value)) return q(value<0?"-INF":"INF");
  std::ostringstream out;
  out.imbue(std::locale::classic());
  out.precision(std::numeric_limits<double>::max_digits10);
  out<<value;
  return out.str();
}
std::string parts_json(const std::vector<DiscoveryWorkbenchPart>& parts) {
  std::string out="[";
  for (const auto& part : parts) {
    if (out.size()>1) out+=',';
    out+="{\"name\":"+q(part.name)+",\"filename\":"+(part.file?q(part.filename):"null")+
        ",\"bytes\":"+q(part.bytes)+",\"file\":"+(part.file?"true":"false")+'}';
  }
  return out+']';
}
struct Artifact { std::string bytes,error; bool available=false; };
std::string artifact_json(const Artifact& artifact) {
  return "{\"bytes\":"+(artifact.available?q(artifact.bytes):"null")+
      ",\"error\":"+(artifact.error.empty()?"null":q(artifact.error))+'}';
}
std::string nullable(std::string_view value) { return value.empty()?"null":q(value); }
} // namespace

LabelSplittingOptions parse_original_label_splitting_options_json(std::string_view text) {
  const auto parsed=Parser(text).parse();
  const auto& object=parsed.object();
  exact_fields(object,{"variant","activity_key","target_key","activities_suffix",
      "prefix_length","suffix_length","min_edge_weight","target_activities"});
  LabelSplittingOptions options;
  options.variant=enum_value(required(object,"variant"));
  options.activity_key=required(object,"activity_key").string();
  options.target_key=required(object,"target_key").string();
  options.activities_suffix=required(object,"activities_suffix").string();
  options.prefix_length=integer<std::size_t>(required(object,"prefix_length"));
  options.suffix_length=integer<std::size_t>(required(object,"suffix_length"));
  options.min_edge_weight=real(required(object,"min_edge_weight"));
  for (const auto& value : required(object,"target_activities").array())
    options.target_activities.push_back(value.string());
  return options;
}
LabelSplittingResult parse_original_label_splitting_result_json(std::string_view text) {
  const auto parsed=Parser(text).parse();
  const auto& object=parsed.object();
  exact_fields(object,{"variant","log"});
  LabelSplittingResult result;
  result.variant=enum_value(required(object,"variant"));
  result.log=parse_exact_ltl_log_json(raw_json(required(object,"log")));
  return result;
}
std::string serialize_original_label_splitting_options_json(const LabelSplittingOptions& options) {
  std::string targets="[";
  for (const auto& value : options.target_activities) {
    if (targets.size()>1) targets+=',';
    targets+=q(value);
  }
  return "{\"variant\":"+enum_json(options.variant)+",\"activity_key\":"+q(options.activity_key)+
      ",\"target_key\":"+q(options.target_key)+",\"activities_suffix\":"+q(options.activities_suffix)+
      ",\"prefix_length\":"+std::to_string(options.prefix_length)+
      ",\"suffix_length\":"+std::to_string(options.suffix_length)+
      ",\"min_edge_weight\":"+real_json(options.min_edge_weight)+
      ",\"target_activities\":"+targets+"]}";
}
std::string serialize_original_label_splitting_result_json(const LabelSplittingResult& result) {
  return "{\"variant\":"+enum_json(result.variant)+",\"log\":"+
      serialize_embedding_workbench_raw_log_json(result.log)+'}';
}

DiscoveryWorkbenchResponse execute_original_label_splitting_workbench(
    const std::vector<DiscoveryWorkbenchPart>& parts,
    const std::vector<DiscoveryWorkbenchPart>& query,
    std::string_view input_order, std::string_view transport_error) {
  std::map<std::string,std::vector<const DiscoveryWorkbenchPart*>> fields;
  for (const auto& part : query) fields[part.name].push_back(&part);
  for (const auto& part : parts) fields[part.name].push_back(&part);
  const auto unique = [&](std::string_view name) -> const DiscoveryWorkbenchPart* {
    const auto found=fields.find(std::string(name));
    return found!=fields.end() && found->second.size()==1 ? found->second.front() : nullptr;
  };
  const auto supplied = [&](std::string_view name) { return fields.contains(std::string(name)); };
  const auto role_context = [&](std::string_view name) {
    const auto* part=unique(name);
    return part ? "{\"filename\":"+(part->file?q(part->filename):"null")+
        ",\"bytes\":"+q(part->bytes)+'}' : std::string("null");
  };
  std::string operation;
  if (const auto* part=unique("operation"); part &&
      (part->bytes=="split_activity_labels" || part->bytes=="contextual_label_splitting" || part->bytes=="serialize_result"))
    operation=part->bytes;
  std::string mode,options_original,options_filename;
  bool options_bytes_available=false,options_filename_available=false;
  if (unique("label_splitting_options_json") && !supplied("label_splitting_options_file")) {
    mode="inline_json"; options_original=unique("label_splitting_options_json")->bytes;
    options_bytes_available=true;
    options_filename=unique("label_splitting_options_json")->filename;
    options_filename_available=unique("label_splitting_options_json")->file;
  } else if (unique("label_splitting_options_file") && !supplied("label_splitting_options_json")) {
    mode="file"; const auto* part=unique("label_splitting_options_file");
    options_original=part->bytes; options_bytes_available=true;
    options_filename=part->filename; options_filename_available=part->file;
  } else if ((operation=="split_activity_labels" || operation=="contextual_label_splitting") &&
      !supplied("label_splitting_options_json") && !supplied("label_splitting_options_file")) mode="native_default";

  std::map<std::string,std::string> snapshots={
      {"source_before","null"},{"source_after","null"},
      {"input_result_before","null"},{"input_result_after","null"},
      {"options_before","null"},{"options_after","null"}};
  std::map<std::string,std::string> optional_errors;
  for (const auto* name : {"source_before","source_after","input_result_before",
      "input_result_after","options_before","options_after","result","legacy_transport"})
    optional_errors.emplace(name,"");
  std::map<std::string,Artifact> artifacts;
  for (const auto* name : {"canonical_source_json","canonical_options_json",
      "canonical_result_json","legacy_result_json","result_log_json"}) artifacts.emplace(name,Artifact{});
  std::optional<EventLog> source;
  std::optional<LabelSplittingOptions> options;
  std::optional<LabelSplittingResult> input_result,computed;
  std::string phase="control",error_type,error_message,call_function,argument_form;
  bool entered=false,returned=false;
  std::string native_error_type,native_error_message,result_json="null";
  int status=200;

  const auto attempt_snapshot = [&](const std::string& name, auto make) {
    try { snapshots[name]=make(); }
    catch (const std::exception& error) { optional_errors[name]=error.what(); }
  };
  const auto capture = [&](bool after) {
    const std::string suffix=after?"_after":"_before";
    if (source) attempt_snapshot("source"+suffix,[&] { return exact_source_snapshot(EventDataView{*source}); });
    if (options) attempt_snapshot("options"+suffix,[&] { return serialize_original_label_splitting_options_json(*options); });
    if (input_result) attempt_snapshot("input_result"+suffix,[&] {
      return "{\"type\":\"LabelSplittingResult\",\"value\":"+
          serialize_original_label_splitting_result_json(*input_result)+'}';
    });
  };
  const auto attempt_artifact = [&](const std::string& name, auto make) {
    try { artifacts[name].bytes=make(); artifacts[name].available=true; }
    catch (const std::exception& error) { artifacts[name].error=error.what(); }
  };
  try {
    if (!transport_error.empty()) {
      phase="transport";
      throw std::invalid_argument(std::string(transport_error));
    }
    static const std::set<std::string> allowed={"operation","source_file","source_format",
        "result_file","label_splitting_options_json","label_splitting_options_file"};
    for (const auto& [name,values] : fields) {
      if (values.size()!=1) throw std::invalid_argument("duplicate original label splitting control: "+name);
      if (!allowed.contains(name)) throw std::invalid_argument("unknown original label splitting control: "+name);
    }
    if (operation.empty()) throw std::invalid_argument("missing or unsupported original label splitting operation");
    for (const auto& part : query) {
      if (part.name=="source_file" || part.name=="result_file" || part.name=="label_splitting_options_file")
        throw std::invalid_argument("original label splitting File role cannot be a query control: "+part.name);
    }
    const bool extract=operation!="serialize_result";
    if (extract!=supplied("source_file") || extract==supplied("result_file"))
      throw std::invalid_argument("operation requires exactly its original source_file or result_file");
    if (!extract && (supplied("source_format") || supplied("label_splitting_options_json") ||
        supplied("label_splitting_options_file")))
      throw std::invalid_argument("source/options controls are inapplicable to serialize_result");
    for (const auto* name : {"source_file","result_file","label_splitting_options_file"})
      if (const auto* part=unique(name);part && !part->file)
        throw std::invalid_argument(std::string(name)+" requires actual filename presence");
    if (supplied("source_format") && unique("source_format")->bytes!="native_log")
      throw std::invalid_argument("source_format must be native_log");
    const int option_controls=static_cast<int>(supplied("label_splitting_options_json"))+
        static_cast<int>(supplied("label_splitting_options_file"));
    if (option_controls>1)
      throw std::invalid_argument("label_splitting_options_json, label_splitting_options_file conflict by presence");
    if (extract) {
      phase="options_parse";
      options.emplace();
      // Publish parsed options only after the complete structural parse succeeds.
      if (mode=="inline_json" || mode=="file") {
        options.reset();
        options=parse_original_label_splitting_options_json(options_original);
      }
      phase="source_parse";
      source=parse_exact_ltl_log_json(unique("source_file")->bytes);
      capture(false);
      call_function=operation;
      argument_form=mode=="native_default"?"native_default":"explicit_options";
      phase="native";
      entered=true;
      if (operation=="contextual_label_splitting") {
        if (mode=="native_default") computed=contextual_label_splitting(*source);
        else computed=contextual_label_splitting(*source,*options);
      } else {
        if (mode=="native_default") computed=split_activity_labels(*source);
        else computed=split_activity_labels(*source,*options);
      }
      returned=true;
      capture(true);
      result_json="{\"type\":\"LabelSplittingResult\",\"value\":null}";
      try {
        result_json="{\"type\":\"LabelSplittingResult\",\"value\":"+
            serialize_original_label_splitting_result_json(*computed)+'}';
      } catch (const std::exception& error) { optional_errors["result"]=error.what(); }
      attempt_artifact("legacy_result_json",[&] { return label_splitting_json(*computed); });
    } else {
      phase="result_parse";
      input_result=parse_original_label_splitting_result_json(unique("result_file")->bytes);
      capture(false);
      call_function="label_splitting_json";
      argument_form="original_result";
      phase="native";
      entered=true;
      auto legacy=label_splitting_json(*input_result);
      returned=true;
      capture(true);
      artifacts["legacy_result_json"]={std::move(legacy),"",true};
      result_json="{\"type\":\"string\",\"value\":"+q(artifacts["legacy_result_json"].bytes)+'}';
    }
  } catch (const std::exception& error) {
    status=400;
    error_type=dynamic_cast<const std::invalid_argument*>(&error)?"invalid_argument":"native_error";
    error_message=error.what();
    if (entered && !returned) {
      native_error_type=error_type;native_error_message=error_message;
      if (operation=="serialize_result") artifacts["legacy_result_json"].error=error_message;
    }
    capture(true);
    // A structural source failure still retains any fully parsed option record.
    if (options && snapshots["options_before"]=="null")
      attempt_snapshot("options_before",[&] { return serialize_original_label_splitting_options_json(*options); });
  }
  if (source) attempt_artifact("canonical_source_json",[&] { return exact_source_snapshot(EventDataView{*source}); });
  if (options) attempt_artifact("canonical_options_json",[&] { return serialize_original_label_splitting_options_json(*options); });
  if (computed || input_result) attempt_artifact("canonical_result_json",[&] {
    return serialize_original_label_splitting_result_json(computed?*computed:*input_result);
  });
  if (computed || input_result) attempt_artifact("result_log_json",[&] {
    return serialize_embedding_workbench_raw_log_json(computed?computed->log:input_result->log);
  });
  if (artifacts["legacy_result_json"].available) {
    if (const auto error=literal_controls_transport_error(artifacts["legacy_result_json"].bytes))
      optional_errors["legacy_transport"]=*error;
  }
  const auto object = [](const auto& values, auto convert) {
    std::string out="{";
    for (const auto& [name,value] : values) {
      if (out.size()>1) out+=',';
      out+=q(name)+':'+convert(value);
    }
    return out+'}';
  };
  const auto unchanged = [&](const std::string& name) {
    const auto& before=snapshots[name+"_before"];
    const auto& after=snapshots[name+"_after"];
    return before=="null" || after=="null" ? std::string("null") : std::string(before==after?"true":"false");
  };
  const auto exception_json = [&](std::string_view type,std::string_view message) {
    return "{\"type\":"+q(type)+",\"message\":"+q(message)+'}';
  };
  std::string out="{\"family\":\"ORIGINAL_LABEL_SPLITTING\",\"operation\":"+nullable(operation)+
      ",\"http_status\":"+std::to_string(status)+",\"input_order\":"+q(input_order)+
      ",\"original_inputs\":"+parts_json(parts)+",\"query_inputs\":"+parts_json(query)+
      ",\"input_context\":{\"source\":"+role_context("source_file")+
      ",\"result\":"+role_context("result_file")+",\"options\":{\"mode\":"+nullable(mode)+
      ",\"filename\":"+(options_filename_available?q(options_filename):"null")+
      ",\"bytes\":"+(options_bytes_available?q(options_original):"null")+"}},\"snapshots\":"+
      object(snapshots,[](const auto& value) { return value; })+
      ",\"unchanged\":{\"source\":"+unchanged("source")+",\"input_result\":"+unchanged("input_result")+
      ",\"options\":"+unchanged("options")+"},\"result\":"+result_json+
      ",\"native_call\":{\"function\":"+nullable(call_function)+",\"argument_form\":"+nullable(argument_form)+
      ",\"entered\":"+(entered?"true":"false")+",\"returned\":"+(returned?"true":"false")+
      ",\"error\":"+(native_error_type.empty()?"null":exception_json(native_error_type,native_error_message))+
      "},\"artifacts\":"+object(artifacts,artifact_json)+",\"optional_errors\":"+
      object(optional_errors,[](const auto& value) { return nullable(value); })+",\"error\":";
  if (status==200) out+="null";
  else out+="{\"phase\":"+q(phase)+",\"type\":"+q(error_type)+",\"message\":"+q(error_message)+'}';
  out+='}';
  return {status,std::move(out)};
}
} // namespace minera
