#pragma once
#include "minera/event_log.hpp"

namespace minera {
class LogTimestampFilterError:public std::invalid_argument {
 public:
  LogTimestampFilterError(std::string type,std::string message):std::invalid_argument(message),type(std::move(type)),message(std::move(message)){}
  const std::string type;
  const std::string message;
};
struct LogTimestampFilterOptions {
  EventAttribute timestamp_key{"","time:timestamp"};
  // Retains public enum-key resolution, including the observed intersecting
  // function's literal-only dictionary lookup. Product controls use literals.
  std::optional<EventAttribute> enum_timestamp_key;
  EventAttribute positive{"","true",AttributeKind::kBoolean};
};
[[nodiscard]] EventLog apply_log_timestamp_filter(const EventLog&,const EventAttribute& parameters=EventAttribute{});
[[nodiscard]] EventLog apply_log_timestamp_auto_filter(const EventLog&,const EventAttribute& parameters=EventAttribute{});
// Mutable overload preserves public case-prefixed source-event injection,
// including mutations that precede a later exception. Const overload copies.
[[nodiscard]] EventLog filter_log_timestamp_events(EventLog&,const EventAttribute& dt1,const EventAttribute& dt2,const LogTimestampFilterOptions& = {});
[[nodiscard]] EventLog filter_log_timestamp_events(const EventLog&,const EventAttribute& dt1,const EventAttribute& dt2,const LogTimestampFilterOptions& = {});
[[nodiscard]] EventLog filter_log_timestamp_contained(const EventLog&,const EventAttribute& dt1,const EventAttribute& dt2,const LogTimestampFilterOptions& = {});
[[nodiscard]] EventLog filter_log_timestamp_intersecting(const EventLog&,const EventAttribute& dt1,const EventAttribute& dt2,const LogTimestampFilterOptions& = {});
[[nodiscard]] EventLog filter_log_timestamp_starting(const EventLog&,const EventAttribute& dt1,const EventAttribute& dt2,const LogTimestampFilterOptions& = {});
[[nodiscard]] EventLog filter_log_timestamp_completing(const EventLog&,const EventAttribute& dt1,const EventAttribute& dt2,const LogTimestampFilterOptions& = {});
[[nodiscard]] EventLog filter_log_timestamp_trace_attribute(const EventLog&,const EventAttribute& dt1,const EventAttribute& dt2,const LogTimestampFilterOptions& = {});
[[nodiscard]] EventLog filter_log_timestamp_attribute(const EventLog&,const EventAttribute& attribute,const EventAttribute& value,const EventAttribute& dt1,const EventAttribute& dt2,const LogTimestampFilterOptions& = {});
[[nodiscard]] bool log_timestamp_is_contained(const Trace&,const EventAttribute& dt1,const EventAttribute& dt2,const EventAttribute& timestamp_key);
[[nodiscard]] bool log_timestamp_is_intersecting(const Trace&,const EventAttribute& dt1,const EventAttribute& dt2,const EventAttribute& timestamp_key);
[[nodiscard]] bool log_timestamp_trace_attribute_is_contained(const Trace&,const EventAttribute& dt1,const EventAttribute& dt2,const EventAttribute& trace_attribute);
[[nodiscard]] bool log_timestamp_has_attribute_in_timeframe(const Trace&,const EventAttribute& attribute,const EventAttribute& value,const EventAttribute& dt1,const EventAttribute& dt2,const EventAttribute& timestamp_key);
} // namespace minera
