#include "minera/case_description_workbench_json.hpp"
#include "minera/embedding_workbench_json.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <variant>
namespace minera { namespace {
[[noreturn]] void invalid(std::string message){throw std::invalid_argument("Case description JSON: "+message);}
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

const Json& required(const Json::Object& object,std::string_view key){const auto it=object.find(key);if(it==object.end())invalid("missing field "+std::string(key));return it->second;}
void allowed(const Json::Object& object,std::initializer_list<std::string_view> names){for(const auto& [name,value]:object){(void)value;if(std::find(names.begin(),names.end(),name)==names.end())invalid("unknown field "+name);}}
std::string q(std::string_view text){return serialize_embedding_workbench_string_json(text);}
std::string hex_bytes(std::string_view value){constexpr char digits[]="0123456789abcdef";std::string out;out.reserve(value.size()*2);for(unsigned char c:value){out+=digits[c>>4];out+=digits[c&15];}return out;}
std::string unhex(std::string_view value){if(value.size()%2)invalid("odd byte hex length");const auto digit=[](char c)->int{return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;};std::string out;for(std::size_t i=0;i<value.size();i+=2){const int a=digit(value[i]),b=digit(value[i+1]);if(a<0||b<0)invalid("invalid byte hexadecimal value");out+=static_cast<char>((a<<4)|b);}return out;}
std::string integer(const Json& value){const auto* number=std::get_if<Number>(&value.value);if(!number)invalid("expected exact integer token");auto digits=std::string_view(number->text);if(digits.starts_with('-'))digits.remove_prefix(1);if(digits.empty()||digits.find_first_not_of("0123456789")!=std::string_view::npos)invalid("expected integer token");return number->text;}
double real_hex(std::string text){
 if(text=="inf")return std::numeric_limits<double>::infinity();if(text=="-inf")return -std::numeric_limits<double>::infinity();if(text=="nan")return std::numeric_limits<double>::quiet_NaN();
 const auto prefix=text.starts_with('-')?1u:0u;if(text.substr(prefix,2)!="0x")invalid("float requires hexadecimal form");text.erase(prefix,2);double value=0;const auto r=std::from_chars(text.data(),text.data()+text.size(),value,std::chars_format::hex);if(r.ec!=std::errc{}||r.ptr!=text.data()+text.size())invalid("invalid hexadecimal binary64 value");return value;
}
std::string real_hex(double value){if(std::isnan(value))return "nan";if(std::isinf(value))return value<0?"-inf":"inf";char buffer[128];const auto r=std::to_chars(buffer,buffer+sizeof(buffer),value,std::chars_format::hex);if(r.ec!=std::errc{})invalid("cannot serialize binary64");std::string text(buffer,r.ptr);text.insert(text.starts_with('-')?1:0,"0x");return text;}
CaseDescriptionValue read_value(const Json& input){
 const auto& object=input.object();const auto type=required(object,"type").string();CaseDescriptionValue out;
 if(type=="builtins.NoneType"){allowed(object,{"type","value"});if(!required(object,"value").null())invalid("NoneType requires null");}
 else if(type=="builtins.str"){allowed(object,{"type","value"});out.kind=CaseValueKind::kString;out.text=required(object,"value").string();}
 else if(type=="builtins.bool"){allowed(object,{"type","value"});out.kind=CaseValueKind::kBoolean;out.boolean=required(object,"value").boolean();}
 else if(type=="builtins.int"){allowed(object,{"type","value"});out.kind=CaseValueKind::kInteger;out.text=integer(required(object,"value"));}
 else if(type=="builtins.float"){allowed(object,{"type","hex","finite"});out.kind=CaseValueKind::kFloat;out.real=real_hex(required(object,"hex").string());const auto it=object.find("finite");if(it!=object.end()&&it->second.boolean()!=std::isfinite(out.real))invalid("float finite tag disagrees with value");}
 else if(type=="datetime.datetime"){allowed(object,{"type","iso","fold"});out.kind=CaseValueKind::kDate;out.text=required(object,"iso").string();const auto it=object.find("fold");if(it!=object.end()){const auto fold=integer(it->second);if(fold!="0"&&fold!="1")invalid("datetime fold must be zero or one");out.boolean=fold=="1";}}
 else if(type=="builtins.bytes"){allowed(object,{"type","hex"});out.kind=CaseValueKind::kBytes;out.text=unhex(required(object,"hex").string());}
 else if(type=="builtins.list"||type=="builtins.tuple"||type=="builtins.dict"){
  allowed(object,{"type","items"});out.kind=type=="builtins.list"?CaseValueKind::kList:type=="builtins.tuple"?CaseValueKind::kTuple:CaseValueKind::kDictionary;
  for(const auto& item:required(object,"items").array()){
   if(out.kind==CaseValueKind::kDictionary){const auto& pair=item.array();if(pair.size()!=2)invalid("dictionary item must have two elements");out.items.push_back(read_value(pair[0]));out.items.push_back(read_value(pair[1]));}
   else out.items.push_back(read_value(item));
  }
 }else invalid("unsupported supplied case value type "+type);
 return out;
}
std::string render_value(const CaseDescriptionValue& value){
 const auto kind=value.kind;
 switch(kind){
 case CaseValueKind::kNull:return "{\"type\":\"builtins.NoneType\",\"value\":null}";
 case CaseValueKind::kString:return "{\"type\":\"builtins.str\",\"value\":"+q(value.text)+"}";
 case CaseValueKind::kBoolean:return std::string("{\"type\":\"builtins.bool\",\"value\":")+(value.boolean?"true":"false")+"}";
 case CaseValueKind::kInteger:{const auto token=integer(Json{Number{value.text}});return "{\"type\":\"builtins.int\",\"value\":"+token+"}";}
 case CaseValueKind::kFloat:return "{\"type\":\"builtins.float\",\"hex\":"+q(real_hex(value.real))+",\"finite\":"+(std::isfinite(value.real)?"true":"false")+"}";
 case CaseValueKind::kDate:return "{\"type\":\"datetime.datetime\",\"iso\":"+q(value.text)+",\"fold\":"+(value.boolean?"1":"0")+"}";
 case CaseValueKind::kBytes:return "{\"type\":\"builtins.bytes\",\"hex\":"+q(hex_bytes(value.text))+"}";
 default:break;
 }
 const bool dict=kind==CaseValueKind::kDictionary;std::string out="{\"type\":"+q(dict?"builtins.dict":kind==CaseValueKind::kList?"builtins.list":"builtins.tuple")+",\"items\":[";
 if(dict&&value.items.size()%2)invalid("dictionary requires pairs");
 for(std::size_t i=0;i<value.items.size();i+=dict?2:1){if(i)out+=',';if(dict)out+='[';out+=render_value(value.items[i]);if(dict)out+=","+render_value(value.items[i+1])+"]";}return out+"]}";
}
CaseDescriptionAttributes attributes(const Json& input){auto value=read_value(input);if(value.kind!=CaseValueKind::kDictionary)invalid("case/event attributes require dictionary");CaseDescriptionAttributes out;std::set<std::string> keys;for(std::size_t i=0;i<value.items.size();i+=2){if(value.items[i].kind!=CaseValueKind::kString)invalid("case/event attribute names require strings");auto key=std::move(value.items[i].text);if(!keys.insert(key).second)invalid("duplicate case/event attribute key");out.emplace_back(std::move(key),std::move(value.items[i+1]));}return out;}
std::string render_attributes(const CaseDescriptionAttributes& values){CaseDescriptionValue out;out.kind=CaseValueKind::kDictionary;for(const auto& [key,value]:values){CaseDescriptionValue name;name.kind=CaseValueKind::kString;name.text=key;out.items.push_back(std::move(name));out.items.push_back(value);}return render_value(out);}
std::string parts_json(const std::vector<DiscoveryWorkbenchPart>& parts){std::string out="[";for(const auto& part:parts){if(out.size()>1)out+=',';out+="{\"name\":"+q(part.name)+",\"filename\":"+q(part.filename)+",\"file\":"+(part.file?"true":"false")+",\"bytes\":"+q(part.bytes)+"}";}return out+"]";}
} // namespace
CaseDescription parse_case_description_json(std::string_view text){const auto parsed=Parser(text).parse();const auto& object=parsed.object();allowed(object,{"attributes","events"});CaseDescription out;out.attributes=attributes(required(object,"attributes"));for(const auto& event:required(object,"events").array())out.events.push_back(attributes(event));return out;}
std::string serialize_case_description_json(const CaseDescription& value){std::string out="{\"attributes\":"+render_attributes(value.attributes)+",\"events\":[";bool first=true;for(const auto& event:value.events){if(!first)out+=',';first=false;out+=render_attributes(event);}return out+"]}";}
CaseDescriptionOptions parse_case_description_options_json(std::string_view text){const auto parsed=Parser(text).parse();const auto& object=parsed.object();allowed(object,{"include_case_attributes","include_event_attributes","include_timestamp","include_header","activity_key","timestamp_key"});CaseDescriptionOptions out;
 for(const auto& [key,value]:object){if(key=="include_case_attributes")out.include_case_attributes=value.boolean();else if(key=="include_event_attributes")out.include_event_attributes=value.boolean();else if(key=="include_timestamp")out.include_timestamp=value.boolean();else if(key=="include_header")out.include_header=value.boolean();else if(key=="activity_key")out.activity_key=value.string();else if(key=="timestamp_key")out.timestamp_key=value.string();}return out;
}
std::string serialize_case_description_options_json(const CaseDescriptionOptions& value){return std::string("{\"include_case_attributes\":")+(value.include_case_attributes?"true":"false")+",\"include_event_attributes\":"+(value.include_event_attributes?"true":"false")+",\"include_timestamp\":"+(value.include_timestamp?"true":"false")+",\"include_header\":"+(value.include_header?"true":"false")+",\"activity_key\":"+q(value.activity_key)+",\"timestamp_key\":"+q(value.timestamp_key)+"}";}
DiscoveryWorkbenchResponse execute_supplied_case_context(const std::vector<DiscoveryWorkbenchPart>& parts,const std::vector<DiscoveryWorkbenchPart>& query,std::string_view input_order,std::string_view transport_error){
 std::vector<DiscoveryWorkbenchPart> all=query;all.insert(all.end(),parts.begin(),parts.end());std::map<std::string,const DiscoveryWorkbenchPart*> controls;
 const DiscoveryWorkbenchPart* file=nullptr;for(const auto& p:all)if(p.name=="case_file"){file=&p;break;}
 const std::string original=file?file->bytes:"",filename=file?file->filename:"";
 std::string phase="configuration_error",error,error_type,error_key,before="null",after="null";bool entered=false,returned=false;
 std::optional<CaseDescription> model;std::optional<CaseDescriptionOptions> options;std::optional<LlmContextResult> result;
 try{
  if(!transport_error.empty()){phase="transport_error";throw std::invalid_argument(std::string(transport_error));}
  const std::set<std::string> names={"variant","case_file","case_options_json","case_options_file"};
  for(const auto& p:all){if(!names.contains(p.name))invalid("unknown supplied-case control "+p.name);if(!controls.emplace(p.name,&p).second)invalid("duplicate supplied-case control "+p.name);if(p.name.ends_with("_file")&&!p.file)invalid(p.name+" requires actual File input");}
  if(!file)invalid("supplied case context requires case_file");
  if(controls.contains("variant")&&controls.at("variant")->bytes!="case")invalid("supplied case variant must be case");
  if(controls.contains("case_options_json")&&controls.contains("case_options_file"))invalid("case options JSON and File are mutually exclusive");
  options=controls.contains("case_options_json")?parse_case_description_options_json(controls.at("case_options_json")->bytes):controls.contains("case_options_file")?parse_case_description_options_json(controls.at("case_options_file")->bytes):CaseDescriptionOptions{};
  phase="case_parse_error";model=parse_case_description_json(original);before=serialize_case_description_json(*model);after=before;
  phase="native_error";entered=true;result=abstract_case(*model,*options);returned=true;
 }catch(const CaseDescriptionKeyError& e){error=e.what();error_type="KeyError";error_key=q(e.key);}
 catch(const std::exception& e){error=e.what();error_type=dynamic_cast<const std::invalid_argument*>(&e)?"invalid_argument":"native_error";}
 if(model)after=serialize_case_description_json(*model);
 std::string value="null",provider="null";if(result){value="{\"variant\":\"case\",\"text\":"+q(result->text)+",\"characters\":"+std::to_string(result->characters)+",\"truncated\":false}";const auto text=q(result->text);if(text.starts_with('"'))provider=text;}
 const std::string errors=error_type.empty()?"null":"{\"phase\":"+q(phase)+",\"type\":"+q(error_type)+",\"message\":"+q(error)+",\"key\":"+(error_key.empty()?"null":error_key)+"}";
 const std::string response=std::string("{\"family\":\"LLM_CONTEXT\",\"operation\":\"supplied_case\",\"variant\":\"case\",\"native_entered\":")+(entered?"true":"false")+",\"native_returned\":"+(returned?"true":"false")+",\"executed\":"+(returned?"true":"false")+",\"error\":"+errors+",\"input_order\":"+q(input_order)+",\"input_context\":{\"case_format\":\"case_value_json\",\"filename\":"+q(filename)+",\"original_case_bytes\":"+std::to_string(original.size())+"},\"original_case\":"+q(original)+",\"options\":"+(options?serialize_case_description_options_json(*options):"null")+",\"snapshots\":{\"case_before\":"+before+",\"case_after\":"+after+"},\"case_unchanged\":"+(model?(before==after?"true":"false"):"null")+",\"result\":"+value+",\"text_bytes\":"+(result?std::to_string(result->text.size()):"null")+",\"provider_text\":"+provider+",\"original_inputs\":"+parts_json(parts)+",\"query_inputs\":"+parts_json(query)+",\"artifacts\":{\"source_case_json\":"+(model?q(before):"null")+",\"case_after_json\":"+(model?q(after):"null")+",\"native_text_bytes\":"+(result?q(result->text):"null")+"}}";
 return {error_type.empty()?200:400,response};
}
} // namespace minera
