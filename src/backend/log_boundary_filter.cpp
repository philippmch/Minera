#include "minera/log_boundary_filter.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>

namespace minera {
namespace {
struct Selected { bool present; const std::string* string; };
Selected selected(const EventAttribute& attribute) {
  const bool string = attribute.kind == AttributeKind::kString && !attribute.is_null &&
      attribute.container == AttributeContainerKind::kScalar &&
      !attribute.scalar_wrapper && !attribute.scalar_children_list &&
      attribute.children.empty() &&
      std::holds_alternative<std::monostate>(attribute.typed_value);
  return {true, string ? &attribute.value : nullptr};
}
Selected selected(const Event& event, std::string_view key) {
  for (const auto& attribute : event.attributes)
    if (attribute.key == key) return selected(attribute);
  const auto projected = [&](std::string_view default_key, const std::string& source_key,
      const std::optional<EventAttribute>& attribute, FirstClassAttributeState state,
      const std::string& value) -> Selected {
    const auto actual = !source_key.empty() ? std::string_view(source_key)
        : attribute ? std::string_view(attribute->key) : default_key;
    if (actual != key || (state == FirstClassAttributeState::kMissing && value.empty() && !attribute))
      return {false,nullptr};
    if (state == FirstClassAttributeState::kNull) return {true,nullptr};
    return attribute ? selected(*attribute) : Selected{true,&value};
  };
  for (const auto field : {
      projected("concept:name",event.activity_source_key,event.activity_attribute,event.activity_state,event.activity),
      projected("time:timestamp",event.timestamp_source_key,event.timestamp_attribute,event.timestamp_state,event.timestamp),
      projected("org:resource",event.resource_source_key,event.resource_attribute,event.resource_state,event.resource),
      projected("lifecycle:transition",event.lifecycle_source_key,event.lifecycle_attribute,event.lifecycle_state,event.lifecycle)})
    if (field.present) return field;
  return {false,nullptr};
}
Selected endpoint(const Trace& trace, bool start, std::string_view key) {
  if (trace.events.empty()) throw std::out_of_range("list index out of range");
  const auto value=selected(start ? trace.events.front() : trace.events.back(),key);
  if (!value.present)
    // pm4py reads trace[0][activity_key]: KeyError (round BV).
    throw std::out_of_range("'"+std::string(key)+"'");
  return value;
}
EventLog apply(const EventLog& source, const std::vector<std::string>& admitted,
               const LogBoundaryFilterOptions& options, bool start) {
  const std::set<std::string,std::less<>> allowed(admitted.begin(),admitted.end());
  EventLog result;copy_event_log_context(source,result);
  for (const auto& trace:source.traces) {
    if (trace.events.empty()) continue;
    const auto value=endpoint(trace,start,options.activity_key);
    const bool found=value.string && allowed.contains(*value.string);
    if (found==options.positive) result.traces.push_back(trace);
  }
  return result;
}

// Python compares the exact integer count with a binary64 threshold. Avoid
// rounding a count above 2^53 before that comparison; range-guard the cast.
bool meets(std::uint64_t count,double threshold) {
  if (std::isnan(threshold)) return false;
  if (threshold<=0.0) return true;
  if (threshold>=0x1p64) return false;
  return count>=static_cast<std::uint64_t>(std::ceil(threshold));
}
EventLog by_counts(const OrderedBoundaryActivityCounts& counts,
    const std::vector<LogActivityVariantGroup>& variants,
    const RankedLogActivityVariants& ranked,double threshold,
    std::string_view key,bool start) {
  std::map<std::string,std::uint64_t,std::less<>> frequencies;
  for (const auto& [label,count]:counts)
    if (!frequencies.emplace(label,count).second)
      throw std::invalid_argument("boundary frequency mapping has a duplicate label");
  std::map<LogActivityVariant,const LogActivityVariantGroup*> groups;
  for (const auto& group:variants)
    if (!groups.emplace(group.variant,&group).second)
      throw std::invalid_argument("boundary variant mapping has a duplicate key");
  if (ranked.empty()) throw std::out_of_range("list index out of range");
  const auto& first=*groups.at(ranked.front().first);
  if (first.traces.empty()) throw std::out_of_range("list index out of range");
  const auto protected_value=endpoint(first.traces.front(),start,key);
  if (!protected_value.string)
    throw std::invalid_argument("supplied boundary frequency labels must be strings");
  EventLog result;
  for (const auto& group:variants) {
    if (group.traces.empty()) throw std::out_of_range("list index out of range");
    const auto value=endpoint(group.traces.front(),start,key);
    if (!value.string)
      throw std::invalid_argument("supplied boundary frequency labels must be strings");
    if (*value.string==*protected_value.string || meets(frequencies.at(*value.string),threshold))
      result.traces.insert(result.traces.end(),group.traces.begin(),group.traces.end());
  }
  return result;
}
}  // namespace

EventLog filter_log_start_activities(const EventLog& source,
    const std::vector<std::string>& admitted,const LogBoundaryFilterOptions& options) {
  return apply(source,admitted,options,true);
}
EventLog filter_log_end_activities(const EventLog& source,
    const std::vector<std::string>& admitted,const LogBoundaryFilterOptions& options) {
  return apply(source,admitted,options,false);
}
EventLog filter_log_by_start_activities(const OrderedBoundaryActivityCounts& counts,
    const std::vector<LogActivityVariantGroup>& variants,
    const RankedLogActivityVariants& ranked,double threshold,std::string_view key) {
  return by_counts(counts,variants,ranked,threshold,key,true);
}
EventLog filter_log_by_end_activities(const OrderedBoundaryActivityCounts& counts,
    const std::vector<LogActivityVariantGroup>& variants,
    const RankedLogActivityVariants& ranked,double threshold,std::string_view key) {
  return by_counts(counts,variants,ranked,threshold,key,false);
}
}  // namespace minera
