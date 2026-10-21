#pragma once
#include "minera/event_log.hpp"

namespace minera {
class LogPathsFilterError:public std::invalid_argument {
 public:
  LogPathsFilterError(std::string type,std::string message):std::invalid_argument(message),type(std::move(type)),message(std::move(message)){}
  const std::string type;
  const std::string message;
};
struct LogPathsFilterOptions {
  EventAttribute attribute_key{"","concept:name"};
  EventAttribute positive{"","true",AttributeKind::kBoolean};
};
struct LogPathsPerformanceOptions:LogPathsFilterOptions {
  EventAttribute timestamp_key{"","time:timestamp"};
  EventAttribute minimum{"","0",AttributeKind::kInt};
  // Every nonnegative duration in the captured full Python datetime domain
  // is admitted by the public default; no private numeric sentinel is read.
  EventAttribute maximum{"","inf",AttributeKind::kFloat};
};
using LogPathsDictionary=std::vector<std::pair<EventAttribute,EventAttribute>>;
struct LogPathsVariant {EventAttribute key;std::vector<Trace> traces;};
using LogPathsVariants=std::vector<LogPathsVariant>;
[[nodiscard]] EventLog filter_log_paths(const EventLog&,const EventAttribute& paths,const LogPathsFilterOptions& = {});
[[nodiscard]] EventLog filter_log_paths_performance(const EventLog&,const EventAttribute& provided_path,const LogPathsPerformanceOptions& = {});
[[nodiscard]] LogPathsDictionary get_log_paths_from_log(const EventLog&,const EventAttribute& attribute_key=EventAttribute{"","concept:name"});
[[nodiscard]] EventAttribute get_sorted_log_paths_list(const LogPathsDictionary&);
[[nodiscard]] EventAttribute get_sorted_log_paths_list(const EventAttribute& ordinary_dictionary);
[[nodiscard]] EventAttribute get_log_paths_threshold(const EventAttribute& plist,const EventAttribute& decreasing_factor);
[[nodiscard]] EventLog filter_log_by_paths(const EventLog&,const LogPathsDictionary&,const LogPathsVariants&,
    const EventAttribute& vc,const EventAttribute& threshold,const EventAttribute& attribute_key=EventAttribute{"","concept:name"});
} // namespace minera
