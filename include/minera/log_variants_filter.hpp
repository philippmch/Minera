#pragma once
#include "minera/event_log.hpp"

namespace minera {
class LogVariantsFilterError:public std::invalid_argument {
 public:
  LogVariantsFilterError(std::string type,std::string message):std::invalid_argument(message),type(std::move(type)),message(std::move(message)){}
  const std::string type;
  const std::string message;
};
struct LogVariantsFilterOptions {
  EventAttribute activity_key{"","concept:name"};
  EventAttribute positive{"","true",AttributeKind::kBoolean};
  // Retains malformed ordinary public parameter inputs for diagnostic fidelity.
  std::optional<EventAttribute> raw_parameters;
};
// Typed public dictionary membership keys, including tuple sequences.
struct LogVariantsAdmissionDictionary { std::vector<EventAttribute> keys; };
struct LogVariantsDictionaryEntry {
  EventAttribute variant;
  std::vector<Trace> traces;
  // A typed non-trace collection/value can retain public length diagnostics.
  std::optional<EventAttribute> ordinary_value;
};
using LogVariantsDictionary=std::vector<LogVariantsDictionaryEntry>;
[[nodiscard]] EventLog filter_log_variants(const EventLog&,const EventAttribute& admitted,const LogVariantsFilterOptions& = {});
[[nodiscard]] EventLog filter_log_variants(const EventLog&,const LogVariantsAdmissionDictionary&,const LogVariantsFilterOptions& = {});
[[nodiscard]] EventLog filter_log_variants_cumulative(const EventLog&,const EventAttribute& percentage=EventAttribute{"","0.8",AttributeKind::kFloat},const LogVariantsFilterOptions& = {});
[[nodiscard]] EventLog filter_log_variants_min_coverage(const EventLog&,const EventAttribute& minimum,const LogVariantsFilterOptions& = {});
[[nodiscard]] EventLog filter_log_variants_max_coverage(const EventLog&,const EventAttribute& maximum,const LogVariantsFilterOptions& = {});
[[nodiscard]] EventLog filter_log_variants_top_k(const EventLog&,const EventAttribute& k,const LogVariantsFilterOptions& = {});
[[nodiscard]] EventLog filter_log_variants_dictionary_percentage(const EventLog&,const LogVariantsDictionary&,const EventAttribute& percentage=EventAttribute{"","0.0",AttributeKind::kFloat});
[[nodiscard]] EventLog filter_log_variants_dictionary_percentage(const EventLog&,const EventAttribute& variants,const EventAttribute& percentage=EventAttribute{"","0.0",AttributeKind::kFloat});
[[nodiscard]] double find_log_variants_auto_threshold(const EventLog&,const LogVariantsDictionary&,const EventAttribute& decreasing_factor);
[[nodiscard]] double find_log_variants_auto_threshold(const EventLog&,const EventAttribute& variants,const EventAttribute& decreasing_factor);
} // namespace minera
