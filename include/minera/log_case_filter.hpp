#pragma once
#include "minera/event_log.hpp"

namespace minera {
class LogCaseFilterError : public std::invalid_argument {
 public:
  LogCaseFilterError(std::string type,std::string message)
      :std::invalid_argument(message),type(std::move(type)),message(std::move(message)){}
  const std::string type;
  const std::string message;
};
struct LogCasePerformanceOptions {
  EventAttribute timestamp_key{"","time:timestamp"};
};
using LogCasePerformancePredicate=std::variant<bool,NativeNA>;
[[nodiscard]] EventLog apply_log_case_filter(const EventLog&,
    const EventAttribute& parameters=EventAttribute{"","",AttributeKind::kString,true});
[[nodiscard]] EventLog apply_log_case_auto_filter(const EventLog&,
    const EventAttribute& parameters=EventAttribute{"","",AttributeKind::kString,true});
[[nodiscard]] EventLog filter_log_on_case_size(const EventLog&,
    const EventAttribute& minimum=EventAttribute{"","2",AttributeKind::kInt},
    const EventAttribute& maximum=EventAttribute{"","",AttributeKind::kString,true});
[[nodiscard]] EventLog filter_log_on_ncases(const EventLog&,
    const EventAttribute& maximum=EventAttribute{"","1000",AttributeKind::kInt});
[[nodiscard]] LogCasePerformancePredicate log_case_satisfies_performance(const Trace&,
    const EventAttribute& minimum,const EventAttribute& maximum,
    const EventAttribute& timestamp_key=EventAttribute{"","time:timestamp"});
[[nodiscard]] EventLog filter_log_on_case_performance(const EventLog&,
    const EventAttribute& minimum,const EventAttribute& maximum,
    const LogCasePerformanceOptions& = {});
[[nodiscard]] EventLog filter_log_case_performance(const EventLog&,
    const EventAttribute& minimum,const EventAttribute& maximum,
    const LogCasePerformanceOptions& = {});
} // namespace minera
