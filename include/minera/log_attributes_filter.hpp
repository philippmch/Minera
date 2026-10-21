#pragma once
#include "minera/event_log.hpp"
#include "minera/log_variants_filter.hpp"

namespace minera {
class LogAttributesFilterError:public std::invalid_argument {
 public:
  LogAttributesFilterError(std::string type,std::string message):std::invalid_argument(message),type(std::move(type)),message(std::move(message)){}
  const std::string type;
  const std::string message;
};
struct LogAttributesFilterParameter {std::string key;EventAttribute value;bool enum_key=false;};
using LogAttributesFilterParameters=std::vector<LogAttributesFilterParameter>;
struct LogAttributesFilterOptions {
  EventAttribute attribute_key{"","concept:name"};
  std::optional<EventAttribute> enum_attribute_key;
  EventAttribute activity_key{"","concept:name"};
  EventAttribute positive{"","true",AttributeKind::kBoolean};
  // Events use the full event cell name; numeric cases prefix a trace key.
  std::optional<EventAttribute> case_id_key;
  EventAttribute case_id_glue{"","case:"};
  std::optional<EventAttribute> stream_filter_key1,stream_filter_key2;
  EventAttribute stream_filter_value1{"","",AttributeKind::kString,true};
  EventAttribute stream_filter_value2{"","",AttributeKind::kString,true};
  std::optional<EventAttribute> keep_once_per_case;
  std::optional<EventAttribute> raw_parameters;
};
using LogAttributesCounts=std::vector<std::pair<EventAttribute,EventAttribute>>;
using LogAttributesCountInput=std::variant<LogAttributesCounts,EventAttribute>;
using LogAttributesVariantInput=std::variant<LogVariantsDictionary,EventAttribute>;
[[nodiscard]] LogAttributesFilterOptions resolve_log_attributes_filter_parameters(const LogAttributesFilterParameters&);
[[nodiscard]] EventLog filter_log_attributes_traces(const EventLog&,const EventAttribute& values,const LogAttributesFilterOptions& = {});
[[nodiscard]] EventLog filter_log_attributes_trace_values(const EventLog&,const EventAttribute& values,const LogAttributesFilterOptions& = {});
// Mutable overloads retain public converter injection, including before errors.
[[nodiscard]] EventLog filter_log_attributes_events(EventLog&,const EventAttribute& values,const LogAttributesFilterOptions& = {});
[[nodiscard]] EventLog filter_log_attributes_events(const EventLog&,const EventAttribute& values,const LogAttributesFilterOptions& = {});
[[nodiscard]] EventLog filter_log_attributes_numeric(EventLog&,const EventAttribute& lower,const EventAttribute& upper,const LogAttributesFilterOptions& = {});
[[nodiscard]] EventLog filter_log_attributes_numeric(const EventLog&,const EventAttribute& lower,const EventAttribute& upper,const LogAttributesFilterOptions& = {});
[[nodiscard]] EventLog filter_log_attributes_numeric_events(EventLog&,const EventAttribute& lower,const EventAttribute& upper,const LogAttributesFilterOptions& = {});
[[nodiscard]] EventLog filter_log_attributes_numeric_events(const EventLog&,const EventAttribute& lower,const EventAttribute& upper,const LogAttributesFilterOptions& = {});
[[nodiscard]] EventLog filter_log_attributes_max_activities(EventLog&,const EventAttribute& maximum,LogAttributesFilterParameters&);
[[nodiscard]] EventLog filter_log_attributes_max_activities(EventLog&,const EventAttribute& maximum=EventAttribute{"","25",AttributeKind::kInt},const LogAttributesFilterOptions& = {});
[[nodiscard]] EventLog filter_log_attributes_max_activities(const EventLog&,const EventAttribute& maximum=EventAttribute{"","25",AttributeKind::kInt},const LogAttributesFilterOptions& = {});
[[nodiscard]] EventLog filter_log_attributes_relative_occurrence(EventLog&,const EventAttribute& minimum,const LogAttributesFilterOptions& = {});
[[nodiscard]] EventLog filter_log_attributes_relative_occurrence(const EventLog&,const EventAttribute& minimum,const LogAttributesFilterOptions& = {});
[[nodiscard]] EventLog filter_log_attributes_threshold(const EventLog&,const LogAttributesCountInput&,const LogAttributesVariantInput&,const EventAttribute& vc,const EventAttribute& threshold,const EventAttribute& attribute_key=EventAttribute{"","concept:name"});
} // namespace minera
