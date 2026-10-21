#pragma once
#include "minera/event_log.hpp"

namespace minera {
struct LogBetweenFilterOptions {
  EventAttribute activity_key{"", "concept:name"};
  EventAttribute case_id_key{"", "concept:name"};
  EventAttribute subcase_separator{"", "##@@"};
};
class LogBetweenFilterError : public std::invalid_argument {
 public:
  LogBetweenFilterError(std::string type, std::string message)
      : std::invalid_argument(message), type(std::move(type)), message(std::move(message)) {}
  const std::string type;
  const std::string message;
};
struct LogBetweenTypedTraceAttribute {
  std::size_t trace_index = 0;
  EventAttribute key;
  EventAttribute value;
  bool operator==(const LogBetweenTypedTraceAttribute&) const = default;
};
struct LogBetweenFilterResult {
  // String-key attributes retain their original insertion order. Any appended
  // nonstring dictionary key is represented explicitly in the ordered sidecar.
  EventLog string_key_log;
  std::vector<LogBetweenTypedTraceAttribute> typed_trace_attributes;
  [[nodiscard]] EventLog project_event_log() const;
};
[[nodiscard]] LogBetweenFilterResult filter_log_between_result(
    const EventLog&, const EventAttribute& act1, const EventAttribute& act2,
    const LogBetweenFilterOptions& = {});
[[nodiscard]] EventLog filter_log_between(
    const EventLog&, const EventAttribute& act1, const EventAttribute& act2,
    const LogBetweenFilterOptions& = {});
[[nodiscard]] EventLog filter_log_between(
    const EventLog&, std::string_view act1, std::string_view act2,
    const LogBetweenFilterOptions& = {});
// Complete byte-safe log plus typed trace-key sidecar, never a silent projection.
[[nodiscard]] std::string serialize_log_between_filter_result_json(const LogBetweenFilterResult&);
}  // namespace minera
