#pragma once
#include "minera/event_log.hpp"
#include <optional>
#include <stdexcept>

namespace minera {
class LogLtlFilterError : public std::invalid_argument {
 public:
  LogLtlFilterError(std::string type,std::string message)
      : std::invalid_argument(message),type(std::move(type)),message(std::move(message)) {}
  const std::string type;
  const std::string message;
};
struct LogLtlFilterOptions {
  EventAttribute attribute_key{"","concept:name"};
  EventAttribute resource_key{"","org:resource"};
  EventAttribute timestamp_key{"","time:timestamp"};
  EventAttribute positive{"","true",AttributeKind::kBoolean};
  EventAttribute enable_timestamp{"","false",AttributeKind::kBoolean};
  std::optional<EventAttribute> timestamp_diff_boundaries;
  std::optional<EventAttribute> raw_parameters;
};
// Mutable primaries preserve any observed public source changes; const overloads copy.
[[nodiscard]] EventLog filter_log_ltl_eventually_follows(EventLog&,const EventAttribute& values,const LogLtlFilterOptions& = {});
[[nodiscard]] EventLog filter_log_ltl_eventually_follows(const EventLog&,const EventAttribute& values,const LogLtlFilterOptions& = {});
[[nodiscard]] EventLog filter_log_ltl_next(EventLog&,const EventAttribute& a,const EventAttribute& b,const EventAttribute& c,const LogLtlFilterOptions& = {});
[[nodiscard]] EventLog filter_log_ltl_next(const EventLog&,const EventAttribute& a,const EventAttribute& b,const EventAttribute& c,const LogLtlFilterOptions& = {});
[[nodiscard]] EventLog filter_log_ltl_four_eyes(EventLog&,const EventAttribute& a,const EventAttribute& b,const LogLtlFilterOptions& = {});
[[nodiscard]] EventLog filter_log_ltl_four_eyes(const EventLog&,const EventAttribute& a,const EventAttribute& b,const LogLtlFilterOptions& = {});
[[nodiscard]] EventLog filter_log_ltl_different_persons(EventLog&,const EventAttribute& a,const LogLtlFilterOptions& = {});
[[nodiscard]] EventLog filter_log_ltl_different_persons(const EventLog&,const EventAttribute& a,const LogLtlFilterOptions& = {});
// Typed helper results preserve the complete observed public value shape.
[[nodiscard]] EventAttribute log_ltl_timestamp_list_is_ge(const EventAttribute& a,const EventAttribute& b);
[[nodiscard]] EventAttribute log_ltl_timestamp_list_is_le(const EventAttribute& a,const EventAttribute& b);
} // namespace minera
