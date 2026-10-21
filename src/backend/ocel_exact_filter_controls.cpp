#include "minera/ocel_exact_filter_controls.hpp"
#include "minera/literal_controls_json.hpp"
#include <charconv>
#include <limits>
#include <stdexcept>

namespace minera {
namespace {
struct Value {
  enum Kind { null, boolean, number, string, array, object } kind=null;
  std::string text;
  std::vector<Value> items;
  std::map<std::string,Value> fields;
};
class Reader {
 public:
  explicit Reader(std::string_view input):input_(input){}
  Value read() { auto result=value(0);space();if(at_!=input_.size())fail();return result; }
 private:
  std::string_view input_;std::size_t at_=0;
  [[noreturn]] void fail() const { throw std::invalid_argument("Exact OCEL filter controls require valid JSON"); }
  void space(){while(at_<input_.size() && (input_[at_]==' '||input_[at_]=='\r'||input_[at_]=='\n'||input_[at_]=='\t'))++at_;}
  bool take(char c){space();if(at_<input_.size()&&input_[at_]==c){++at_;return true;}return false;}
  void require(char c){if(!take(c))fail();}
  bool digit()const{return at_<input_.size()&&input_[at_]>='0'&&input_[at_]<='9';}
  std::string text(){
    space();const auto begin=at_;require('"');bool escaped=false;
    while(at_<input_.size()){
      const auto c=input_[at_++];
      if(escaped)escaped=false;else if(c=='\\')escaped=true;
      else if(c=='"')return parse_literal_string_vector_json("["+std::string(input_.substr(begin,at_-begin))+"]").at(0);
    }fail();
  }
  Value value(unsigned depth){
    if(depth>256)fail();space();if(at_==input_.size())fail();Value v;
    if(input_[at_]=='"'){v.kind=Value::string;v.text=text();return v;}
    if(take('[')){
      v.kind=Value::array;if(take(']'))return v;
      do{v.items.push_back(value(depth+1));if(take(']'))return v;require(',');}while(true);
    }
    if(take('{')){
      v.kind=Value::object;if(take('}'))return v;
      do{auto key=text();require(':');auto entry=value(depth+1);
        if(!v.fields.emplace(std::move(key),std::move(entry)).second)
          throw std::invalid_argument("Exact OCEL filter controls repeat an object key");
        if(take('}'))return v;require(',');}while(true);
    }
    for(const auto token:{"null","true","false"}){
      const std::string_view word=token;
      if(input_.substr(at_,word.size())==word){at_+=word.size();v.kind=word=="null"?Value::null:Value::boolean;v.text=word;return v;}
    }
    const auto begin=at_;if(input_[at_]=='-')++at_;
    if(!digit())fail();if(input_[at_]=='0')++at_;else while(digit())++at_;
    if(at_<input_.size()&&input_[at_]=='.'){++at_;if(!digit())fail();while(digit())++at_;}
    if(at_<input_.size()&&(input_[at_]=='e'||input_[at_]=='E')){
      ++at_;if(at_<input_.size()&&(input_[at_]=='+'||input_[at_]=='-'))++at_;
      if(!digit())fail();while(digit())++at_;
    }
    v.kind=Value::number;v.text=input_.substr(begin,at_-begin);return v;
  }
};
const Value& member(const Value& v,const std::string& key){
  const auto found=v.fields.find(key);
  if(found==v.fields.end())throw std::invalid_argument("Exact OCEL typed value requires "+key);
  return found->second;
}
std::string type(const Value& v){
  if(v.kind!=Value::object)throw std::invalid_argument("Exact OCEL values require typed objects");
  for(const auto& [key,unused]:v.fields){(void)unused;if(key!="type"&&key!="value")throw std::invalid_argument("Unknown exact OCEL typed value field: "+key);}
  const auto& tag=member(v,"type");if(tag.kind!=Value::string)throw std::invalid_argument("Exact OCEL value type must be a string");return tag.text;
}
double real(const Value& v){
  if(v.kind!=Value::number&&v.kind!=Value::string)throw std::invalid_argument("Exact OCEL decimal requires a numeric token or string");
  if(v.text=="NaN")return std::numeric_limits<double>::quiet_NaN();
  if(v.text=="INF"||v.text=="+INF")return std::numeric_limits<double>::infinity();
  if(v.text=="-INF")return -std::numeric_limits<double>::infinity();
  double result=0;const auto parsed=std::from_chars(v.text.data(),v.text.data()+v.text.size(),result);
  if(v.text.empty()||parsed.ec!=std::errc{}||parsed.ptr!=v.text.data()+v.text.size())throw std::invalid_argument("Invalid exact OCEL decimal");return result;
}
ParsedExactTimestamp instant(const Value& v){
  if(v.kind!=Value::string)throw std::invalid_argument("Exact OCEL timestamp requires a string");
  const auto result=parse_timestamp_exact(v.text);if(!result)throw std::invalid_argument("Invalid exact OCEL timestamp");return *result;
}
OcelEventAttributeFilterValue scalar(const Value& v){
  const auto tag=type(v);
  if(tag=="null"){
    const auto found=v.fields.find("value");if(found!=v.fields.end()&&found->second.kind!=Value::null)throw std::invalid_argument("Null exact OCEL value must be null");return nullptr;
  }
  if(tag=="nat"){
    const auto found=v.fields.find("value");
    if(found!=v.fields.end()&&found->second.kind!=Value::null)throw std::invalid_argument("NaT exact OCEL value must omit value or use null");
    return OcelAttributeNaT{};
  }
  const auto& data=member(v,"value");
  if(tag=="string"&&data.kind==Value::string)return data.text;
  if(tag=="boolean"&&data.kind==Value::boolean)return data.text=="true";
  if(tag=="decimal")return real(data);
  if(tag=="timestamp")return instant(data);
  if(tag=="integer"&&(data.kind==Value::number||data.kind==Value::string)){
    std::int64_t result=0;const auto parsed=std::from_chars(data.text.data(),data.text.data()+data.text.size(),result);
    if(!data.text.empty()&&parsed.ec==std::errc{}&&parsed.ptr==data.text.data()+data.text.size())return result;
  }
  throw std::invalid_argument("Invalid exact OCEL typed scalar (integers must fit int64)");
}
}
std::vector<OcelEventAttributeFilterValue> parse_ocel_event_filter_values_json(std::string_view input){
  const auto root=Reader(input).read();if(root.kind!=Value::array)throw std::invalid_argument("Exact OCEL attribute values must be a JSON array");
  std::vector<OcelEventAttributeFilterValue> result;for(const auto& value:root.items)result.push_back(scalar(value));return result;
}
OcelEventTimestampBound parse_ocel_event_timestamp_bound_json(std::string_view input){
  const auto root=Reader(input).read();const auto tag=type(root);const auto& data=member(root,"value");
  if(tag=="string"&&data.kind==Value::string)return data.text;
  if(tag=="timestamp")return instant(data);
  throw std::invalid_argument("Exact OCEL timestamp bounds require string or timestamp type");
}
std::map<std::string,OcelObjectTypeCountThreshold> parse_ocel_exact_minimum_counts_json(std::string_view input){
  const auto root=Reader(input).read();if(root.kind!=Value::object)throw std::invalid_argument("Exact OCEL minimum counts must be a JSON object");
  std::map<std::string,OcelObjectTypeCountThreshold> result;
  for(const auto& [key,value]:root.fields){
    if(value.kind==Value::boolean)result.emplace(key,TokenInteger(value.text=="true"?1:0));
    else if(value.kind==Value::number&&value.text.find_first_of(".eE")==std::string::npos)result.emplace(key,TokenInteger(value.text));
    else if(value.kind==Value::number||(value.kind==Value::string&&(value.text=="NaN"||value.text=="INF"||value.text=="+INF"||value.text=="-INF")))result.emplace(key,real(value));
    else throw std::invalid_argument("Exact OCEL counts require numbers, booleans, NaN, INF or -INF");
  }return result;
}
std::map<std::string,std::vector<std::string>> parse_ocel_exact_activity_type_correspondence_json(std::string_view input){
  const auto root=Reader(input).read();
  if(root.kind!=Value::object)throw std::invalid_argument("Exact OCEL activity/type correspondence must be a JSON object");
  std::map<std::string,std::vector<std::string>> result;
  for(const auto& [key,values]:root.fields){
    if(values.kind!=Value::array)throw std::invalid_argument("Exact OCEL activity/type correspondence values must be string arrays");
    auto& target=result[key];
    for(const auto& value:values.items){
      if(value.kind!=Value::string)throw std::invalid_argument("Exact OCEL activity/type correspondence activities must be strings");
      target.push_back(value.text);
    }
  }
  return result;
}
OcelEventAttributeColumnKind parse_ocel_event_attribute_column_kind(std::string_view text){
  if(text=="auto")return OcelEventAttributeColumnKind::kAutomatic;
  if(text=="scalar")return OcelEventAttributeColumnKind::kScalar;
  if(text=="timestamp")return OcelEventAttributeColumnKind::kTimestamp;
  throw std::invalid_argument("column_kind requires auto, scalar or timestamp");
}
std::string serialize_ocel_exact_filter_json(const Ocel& log,OcelJsonFormat format){
  auto json=serialize_ocel_json(log,format);
  json.insert(json.size()-1,",\"minera:literalNames\":true");
  return json;
}
}
