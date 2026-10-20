#include "minera/log_rework_filter.hpp"

#include <charconv>
#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>
#include <type_traits>

namespace minera {
namespace {
enum class Scalar { null, string, integer, real, other };
Scalar kind(const EventAttribute& a) {
  if(a.container!=AttributeContainerKind::kScalar || a.scalar_wrapper ||
      a.scalar_children_list || !a.children.empty()) return Scalar::other;
  if(a.is_null) return Scalar::null;
  if(std::holds_alternative<bool>(a.typed_value) ||
      std::holds_alternative<std::int64_t>(a.typed_value) ||
      std::holds_alternative<std::uint64_t>(a.typed_value)) return Scalar::integer;
  if(std::holds_alternative<double>(a.typed_value)) return Scalar::real;
  if(a.kind==AttributeKind::kInt || a.kind==AttributeKind::kBoolean) return Scalar::integer;
  if(a.kind==AttributeKind::kFloat) return Scalar::real;
  if(a.kind==AttributeKind::kString && std::holds_alternative<std::monostate>(a.typed_value))
    return Scalar::string;
  return Scalar::other;
}
std::string integer(const EventAttribute& a) {
  if(const auto* n=std::get_if<std::int64_t>(&a.typed_value))return std::to_string(*n);
  if(const auto* n=std::get_if<std::uint64_t>(&a.typed_value))return std::to_string(*n);
  if(const auto* n=std::get_if<bool>(&a.typed_value))return *n?"1":"0";
  if(a.kind==AttributeKind::kBoolean) {
    if(a.value=="true" || a.value=="True" || a.value=="1")return "1";
    if(a.value=="false" || a.value=="False" || a.value=="0")return "0";
    throw std::invalid_argument("invalid rework boolean activity");
  }
  auto text=a.value;bool negative=false;std::size_t at=0;
  if(!text.empty() && (text[0]=='+' || text[0]=='-')) { negative=text[0]=='-';++at; }
  if(at==text.size())throw std::invalid_argument("invalid rework integer activity");
  for(std::size_t i=at;i<text.size();++i)
    if(text[i]<'0' || text[i]>'9')throw std::invalid_argument("invalid rework integer activity");
  while(at+1<text.size() && text[at]=='0')++at;
  text=text.substr(at);return negative && text!="0" ? "-"+text:text;
}
double real(const EventAttribute& a) {
  if(const auto* n=std::get_if<double>(&a.typed_value))return *n;
  std::string_view text=a.value;
  const auto space=[](char c){return c==' ' || c=='\t' || c=='\n' || c=='\r' || c=='\f' || c=='\v';};
  while(!text.empty() && space(text.front()))text.remove_prefix(1);
  while(!text.empty() && space(text.back()))text.remove_suffix(1);
  if(!text.empty() && text.front()=='+')text.remove_prefix(1);
  std::string spelling(text);
  for(auto& c:spelling)if(c>='A' && c<='Z')c=static_cast<char>(c-'A'+'a');
  if(spelling=="inf" || spelling=="infinity")return std::numeric_limits<double>::infinity();
  if(spelling=="-inf" || spelling=="-infinity")return -std::numeric_limits<double>::infinity();
  if(spelling=="nan" || spelling=="-nan")return std::numeric_limits<double>::quiet_NaN();
  double result=0;
  const auto parsed=std::from_chars(text.data(),text.data()+text.size(),result,std::chars_format::general);
  if(text.empty() || parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size())
    throw std::invalid_argument("invalid rework float activity");
  return result;
}
bool equal(const EventAttribute& a,const EventAttribute& b) {
  const auto x=kind(a),y=kind(b);
  if(x==Scalar::other || y==Scalar::other)return false;
  if(x==Scalar::null || y==Scalar::null)return x==y;
  if(x==Scalar::string || y==Scalar::string)return x==y && a.value==b.value;
  if(x==Scalar::integer && y==Scalar::integer)return integer(a)==integer(b);
  if(x==Scalar::real && y==Scalar::real)return real(a)==real(b);
  const auto value=real(x==Scalar::real?a:b);
  if(!std::isfinite(value) || std::trunc(value)!=value)return false;
  if(value==0)return integer(x==Scalar::integer?a:b)=="0";
  char buffer[512];const auto formatted=std::to_chars(buffer,buffer+sizeof(buffer),value,std::chars_format::fixed,0);
  if(formatted.ec!=std::errc{})return false;
  return integer(x==Scalar::integer?a:b)==std::string(buffer,formatted.ptr);
}
EventAttribute selected(const Event& event,std::string_view key) {
  for(const auto& attribute:event.attributes)if(attribute.key==key)return attribute;
  const auto projected=[&](std::string_view default_key,const std::string& source_key,
      const std::optional<EventAttribute>& attribute,FirstClassAttributeState state,
      const std::string& value)->std::optional<EventAttribute> {
    const auto actual=!source_key.empty()?std::string_view(source_key):
        attribute?std::string_view(attribute->key):default_key;
    if(actual!=key || (state==FirstClassAttributeState::kMissing && value.empty() && !attribute))
      return std::nullopt;
    if(state==FirstClassAttributeState::kNull) {
      EventAttribute result{std::string(key),""};result.is_null=true;return result;
    }
    return attribute?*attribute:EventAttribute{std::string(key),value};
  };
  for(const auto value:{
      projected("concept:name",event.activity_source_key,event.activity_attribute,event.activity_state,event.activity),
      projected("time:timestamp",event.timestamp_source_key,event.timestamp_attribute,event.timestamp_state,event.timestamp),
      projected("org:resource",event.resource_source_key,event.resource_attribute,event.resource_state,event.resource),
      projected("lifecycle:transition",event.lifecycle_source_key,event.lifecycle_attribute,event.lifecycle_state,event.lifecycle)})
    if(value)return *value;
  throw std::out_of_range("'"+std::string(key)+"'");
}
bool compare(std::uint64_t count,const LogReworkFilterOptions& options) {
  return std::visit([&](const auto& threshold)->bool {
    using T=std::decay_t<decltype(threshold)>;
    if constexpr(std::is_same_v<T,std::monostate> || std::is_same_v<T,std::string>) {
      const auto type=std::is_same_v<T,std::monostate>?"NoneType":"str";
      throw std::invalid_argument(std::string(options.positive?"'>='":"'<'")+
          " not supported between instances of 'int' and '"+type+"'");
    } else if constexpr(std::is_same_v<T,double>) {
      // Independent >= and < preserve NaN's two false comparisons. Guard
      // conversion before ceil so integer counts are never rounded to double.
      if(std::isnan(threshold))return false;
      const bool ge=threshold<=0 || (threshold<0x1p64 &&
          count>=static_cast<std::uint64_t>(std::ceil(threshold)));
      return options.positive?ge:!ge;
    } else if constexpr(std::is_same_v<T,std::int64_t>) {
      const bool ge=threshold<=0 || count>=static_cast<std::uint64_t>(threshold);
      return options.positive?ge:!ge;
    } else {
      const bool ge=count>=static_cast<std::uint64_t>(threshold);
      return options.positive?ge:!ge;
    }
  },options.min_occurrences);
}
}  // namespace
EventLog filter_log_rework(const EventLog& source,const EventAttribute& activity,
                          const LogReworkFilterOptions& options) {
  if(kind(activity)==Scalar::other)
    throw std::invalid_argument("rework target must be a primitive scalar");
  EventLog result;copy_event_log_context(source,result);
  result.traces.reserve(source.traces.size());
  for(const auto& trace:source.traces) {
    std::uint64_t count=0;
    for(const auto& event:trace.events)if(equal(selected(event,options.activity_key),activity))++count;
    const bool keep=count?compare(count,options):!options.positive;
    if(keep)result.traces.push_back(trace);
  }
  return result;
}
EventLog filter_log_rework(const EventLog& source,std::string_view activity,
                          const LogReworkFilterOptions& options) {
  return filter_log_rework(source,EventAttribute{"",std::string(activity)},options);
}
}  // namespace minera
