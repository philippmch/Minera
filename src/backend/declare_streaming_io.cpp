#include "minera/declare_streaming_io.hpp"
#include <charconv>
#include <cmath>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <variant>

namespace minera {
namespace {
struct Json {
  using Array = std::vector<Json>;
  using Object = std::map<std::string, Json, std::less<>>;
  std::variant<std::nullptr_t, bool, double, std::string, Array, Object> value;
};

class JsonReader {
 public:
  explicit JsonReader(std::string_view input) : input_(input) {
    if (input.size() > 16 * 1024 * 1024)
      fail("response exceeds 16 MiB");
  }
  Json read() {
    auto result = value(0);
    space();
    if (position_ != input_.size()) fail("trailing content");
    return result;
  }

 private:
  [[noreturn]] void fail(std::string_view reason) const {
    throw std::invalid_argument("Declare stream model: invalid JSON (" +
                                std::string(reason) + ")");
  }
  void space() {
    while (position_ < input_.size() &&
           (input_[position_] == ' ' || input_[position_] == '\n' ||
            input_[position_] == '\r' || input_[position_] == '\t'))
      ++position_;
  }
  char take() {
    if (position_ == input_.size()) fail("unexpected end");
    return input_[position_++];
  }
  bool consume(std::string_view token) {
    if (input_.substr(position_, token.size()) != token) return false;
    position_ += token.size();
    return true;
  }
  static void append_utf8(std::string& out, unsigned code) {
    if (code <= 0x7f) out.push_back(static_cast<char>(code));
    else if (code <= 0x7ff) {
      out.push_back(static_cast<char>(0xc0 | code >> 6));
      out.push_back(static_cast<char>(0x80 | (code & 0x3f)));
    } else if (code <= 0xffff) {
      out.push_back(static_cast<char>(0xe0 | code >> 12));
      out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3f)));
      out.push_back(static_cast<char>(0x80 | (code & 0x3f)));
    } else {
      out.push_back(static_cast<char>(0xf0 | code >> 18));
      out.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3f)));
      out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3f)));
      out.push_back(static_cast<char>(0x80 | (code & 0x3f)));
    }
  }
  unsigned hex4() {
    unsigned result = 0;
    for (int i = 0; i < 4; ++i) {
      const char c = take();
      result <<= 4;
      if (c >= '0' && c <= '9') result += c - '0';
      else if (c >= 'a' && c <= 'f') result += c - 'a' + 10;
      else if (c >= 'A' && c <= 'F') result += c - 'A' + 10;
      else fail("invalid Unicode escape");
    }
    return result;
  }
  std::string string() {
    if (take() != '"') fail("expected string");
    std::string out;
    while (true) {
      const char c = take();
      if (c == '"') return out;
      if (static_cast<unsigned char>(c) < 0x20) fail("control in string");
      if (c != '\\') { out.push_back(c); continue; }
      const char escape = take();
      if (escape == '"' || escape == '\\' || escape == '/') out.push_back(escape);
      else if (escape == 'b') out.push_back('\b');
      else if (escape == 'f') out.push_back('\f');
      else if (escape == 'n') out.push_back('\n');
      else if (escape == 'r') out.push_back('\r');
      else if (escape == 't') out.push_back('\t');
      else if (escape == 'u') {
        unsigned code = hex4();
        if (code >= 0xd800 && code <= 0xdbff) {
          if (!consume("\\u")) fail("unpaired surrogate");
          const unsigned low = hex4();
          if (low < 0xdc00 || low > 0xdfff) fail("unpaired surrogate");
          code = 0x10000 + ((code - 0xd800) << 10) + (low - 0xdc00);
        } else if (code >= 0xdc00 && code <= 0xdfff) fail("unpaired surrogate");
        append_utf8(out, code);
      } else fail("invalid escape");
      if (out.size() > 1024 * 1024) fail("string exceeds 1 MiB");
    }
  }
  Json value(std::size_t depth) {
    if (depth > 64) fail("nesting exceeds 64 levels");
    if (++nodes_ > 1'000'000) fail("too many values");
    space();
    if (position_ == input_.size()) fail("unexpected end");
    if (input_[position_] == '"') return Json{string()};
    if (consume("null")) return Json{nullptr};
    if (consume("true")) return Json{true};
    if (consume("false")) return Json{false};
    if (input_[position_] == '[') {
      ++position_; Json::Array array; space();
      if (position_ < input_.size() && input_[position_] == ']') { ++position_; return Json{std::move(array)}; }
      while (true) {
        array.push_back(value(depth + 1)); space();
        const char separator = take();
        if (separator == ']') return Json{std::move(array)};
        if (separator != ',') fail("expected comma");
      }
    }
    if (input_[position_] == '{') {
      ++position_; Json::Object object; space();
      if (position_ < input_.size() && input_[position_] == '}') { ++position_; return Json{std::move(object)}; }
      while (true) {
        space(); if (position_ == input_.size() || input_[position_] != '"') fail("expected object key");
        auto key = string(); space(); if (take() != ':') fail("expected colon");
        if (!object.emplace(std::move(key), value(depth + 1)).second) fail("duplicate object key");
        space(); const char separator = take();
        if (separator == '}') return Json{std::move(object)};
        if (separator != ',') fail("expected comma");
      }
    }
    const auto start = position_;
    if (input_[position_] == '-') ++position_;
    if (position_ == input_.size()) fail("invalid number");
    if (input_[position_] == '0') ++position_;
    else {
      if (input_[position_] < '1' || input_[position_] > '9') fail("invalid value");
      while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9') ++position_;
    }
    if (position_ < input_.size() && input_[position_] == '.') {
      ++position_; const auto digits = position_;
      while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9') ++position_;
      if (digits == position_) fail("invalid number");
    }
    if (position_ < input_.size() && (input_[position_] == 'e' || input_[position_] == 'E')) {
      ++position_; if (position_ < input_.size() && (input_[position_] == '+' || input_[position_] == '-')) ++position_;
      const auto digits = position_;
      while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9') ++position_;
      if (digits == position_) fail("invalid number");
    }
    double number = 0;
    const auto token = input_.substr(start, position_ - start);
    const auto parsed = std::from_chars(token.data(), token.data() + token.size(), number);
    if (parsed.ec != std::errc{} || !std::isfinite(number)) fail("invalid number");
    return Json{number};
  }
  std::string_view input_;
  std::size_t position_ = 0;
  std::size_t nodes_ = 0;
};

const Json::Object& object(const Json& value, std::string_view context) {
  const auto* result = std::get_if<Json::Object>(&value.value);
  if (!result) throw std::invalid_argument("Declare stream model: expected object in " + std::string(context));
  return *result;
}
const Json::Array& array(const Json& value, std::string_view context) {
  const auto* result = std::get_if<Json::Array>(&value.value);
  if (!result) throw std::invalid_argument("Declare stream model: expected array in " + std::string(context));
  return *result;
}
const Json* member(const Json::Object& value, std::string_view key) {
  const auto it = value.find(key); return it == value.end() ? nullptr : &it->second;
}
std::string text(const Json::Object& value, std::string_view key, bool required = true) {
  const auto* item = member(value, key);
  if (!item) {
    if (!required) return {};
    throw std::invalid_argument("Declare stream model: missing field " + std::string(key));
  }
  const auto* result = std::get_if<std::string>(&item->value);
  if (!result) throw std::invalid_argument("Declare stream model: field " + std::string(key) + " must be a string");
  return *result;
}
} // namespace
namespace {
const Json& required(const Json::Object& o,std::string_view key) {
  const auto* value=member(o,key);if(!value)throw std::invalid_argument("Declare stream model: missing field "+std::string(key));return *value;
}
int integer(const Json::Object& o,std::string_view key,int fallback=0) {
  const auto* v=member(o,key);if(!v)return fallback;const auto* n=std::get_if<double>(&v->value);
  if(!n||std::trunc(*n)!=*n||*n<std::numeric_limits<int>::min()||*n>std::numeric_limits<int>::max())throw std::invalid_argument("Declare stream model: "+std::string(key)+" must be an int32 value");return static_cast<int>(*n);
}
std::string json_quote(std::string_view value) {
  std::ostringstream out;out<<'"';for(unsigned char c:value){if(c=='"')out<<"\\\"";else if(c=='\\')out<<"\\\\";else if(c<0x20)out<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<unsigned(c)<<std::dec;else out<<char(c);}out<<'"';return out.str();
}
void identity(std::ostream& out,std::size_t index,const std::string& family,const DeclareConstraint& rule) {
  out<<"{\"constraint_index\":"<<index<<",\"template\":"<<json_quote(family)<<",\"first\":"<<json_quote(rule.first)<<",\"second\":"<<json_quote(rule.second)<<",\"binary\":"<<(rule.binary?"true":"false");
}
void event_identity(std::ostream& out,const DeclareStreamingConstraintIdentity& rule) {
  identity(out,rule.constraint_index,rule.template_name,{rule.first,rule.second,rule.binary,0,0});out<<'}';
}
}
DeclareModel parse_declare_stream_model_json(std::string_view json) {
  const auto root=JsonReader(json).read();const auto& o=object(root,"model");DeclareModel model;
  if(const auto* rules=member(o,"rules")) {
    for(const auto& item:array(*rules,"rules")) {
      const auto& r=object(item,"rule");const auto& flag=required(r,"binary");const auto* binary=std::get_if<bool>(&flag.value);if(!binary)throw std::invalid_argument("Declare stream model: binary must be boolean");
      model.add(text(r,"template"),{text(r,"first"),text(r,"second",false),*binary,integer(r,"support"),integer(r,"confidence")});
    }
  } else {
    const auto& activities=array(required(o,"activities"),"activities");std::map<int,std::string> labels;
    for(std::size_t i=0;i<activities.size();++i) {
      if(const auto* label=std::get_if<std::string>(&activities[i].value))labels.emplace(static_cast<int>(i),*label);
      else {const auto& a=object(activities[i],"activity");const auto id=integer(a,"id",static_cast<int>(i));if(id<0||!labels.emplace(id,text(a,"name")).second)throw std::invalid_argument("Declare stream model: duplicate or negative activity ID");}
    }
    const auto& families=object(required(o,"declare"),"declare");
    const auto label=[&](const Json::Object& rule,std::string_view key){(void)required(rule,key);const int id=integer(rule,key);const auto it=labels.find(id);if(it==labels.end())throw std::invalid_argument("Declare stream model: unknown activity ID");return it->second;};
    for(const auto& [family,values]:families)for(const auto& item:array(values,"family rules")) {
      const auto& r=object(item,"rule");const bool unary=member(r,"activity")!=nullptr;
      if(unary&&(member(r,"source")||member(r,"target")))throw std::invalid_argument("Declare stream model: ambiguous rule arity");
      model.add(family,{label(r,unary?"activity":"source"),unary?std::string{}:label(r,"target"),!unary,integer(r,"support"),integer(r,"confidence")});
    }
  }
  // Native construction validates known arities and preserves unknown inert rules.
  DeclareStreamingConformance validation(model);return model;
}
std::string serialize_declare_stream_model_json(const DeclareModel& model) {
  std::ostringstream out;out<<"{\"family\":\"DECLARE_STREAM_MODEL\",\"rule_count\":"<<model.size()<<",\"rules\":[";std::size_t index=0;
  for(const auto& [family,rules]:model.rules)for(const auto& rule:rules){if(index)out<<',';identity(out,index++,family,rule);out<<",\"support\":"<<rule.support<<",\"confidence\":"<<rule.confidence<<'}';}out<<"]}";return out.str();
}
std::string serialize_declare_streaming_json(const DeclareStreamingSnapshot& snapshot,const DeclareModel& model) {
  std::ostringstream out;out<<"{\"family\":\"STREAMING_DECLARE\",\"total_events_processed\":"<<snapshot.total_events_processed<<",\"total_deviations\":"<<snapshot.total_deviations<<",\"model\":"<<serialize_declare_stream_model_json(model)<<",\"deviations_per_time\":[";
  for(std::size_t i=0;i<snapshot.deviations_per_time.size();++i){const auto& d=snapshot.deviations_per_time[i];if(i)out<<',';out<<"{\"timestamp\":"<<json_quote(d.timestamp)<<",\"deviations\":"<<d.deviations<<'}';}out<<"],\"cases\":{";bool first=true;
  for(const auto& [id,c]:snapshot.cases){if(!first)out<<',';first=false;out<<json_quote(id)<<":{\"events\":"<<c.events<<",\"deviations\":"<<c.deviations<<",\"constraint_states\":[";
    for(std::size_t i=0;i<c.constraint_states.size();++i){const auto& s=c.constraint_states[i];if(i)out<<',';identity(out,s.constraint_index,s.template_name,{s.first,s.second,s.binary,0,0});out<<",\"state\":"<<json_quote(s.state)<<'}';}out<<"]}";}
  out<<"}}";return out.str();
}
std::string serialize_declare_stream_event_json(const DeclareStreamingEventResult& event) {
  std::ostringstream out;out<<"{\"case_id\":"<<json_quote(event.case_id)<<",\"activity\":"<<json_quote(event.activity)<<",\"timestamp\":"<<json_quote(event.timestamp)<<",\"event_index\":"<<event.event_index<<",\"deviations\":"<<event.deviations<<",\"violated_constraints\":[";
  for(std::size_t i=0;i<event.violated_constraints.size();++i){if(i)out<<',';event_identity(out,event.violated_constraints[i]);}out<<"]}";return out.str();
}
} // namespace minera
