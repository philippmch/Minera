#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <string_view>

#include "minera/event_log.hpp"

namespace minera {

enum class AttributeStatisticsScope {
  kEvents,
  kTraces,

  EVENTS = kEvents,
  TRACES = kTraces,
};

struct AttributeStatisticsOptions {
  AttributeStatisticsScope scope = AttributeStatisticsScope::kEvents;
  std::string attribute_key;
  bool keep_once_per_case = false;
};

struct AttributeStatisticsResult {
  AttributeStatisticsScope scope = AttributeStatisticsScope::kEvents;
  std::string attribute_key;
  bool keep_once_per_case = false;
  std::map<std::string, std::uint64_t> values;
};

[[nodiscard]] AttributeStatisticsResult discover_attribute_statistics(
    const EventLog& log, const AttributeStatisticsOptions& options);

[[nodiscard]] inline AttributeStatisticsResult attribute_statistics(
    const EventLog& log, const AttributeStatisticsOptions& options) {
  return discover_attribute_statistics(log, options);
}

[[nodiscard]] inline std::map<std::string, std::uint64_t>
get_attribute_values(const EventLog& log, std::string_view attribute_key,
                     bool keep_once_per_case = false) {
  AttributeStatisticsOptions options;
  options.scope = AttributeStatisticsScope::kEvents;
  options.attribute_key = std::string(attribute_key);
  options.keep_once_per_case = keep_once_per_case;
  return discover_attribute_statistics(log, options).values;
}

[[nodiscard]] inline std::map<std::string, std::uint64_t>
get_trace_attribute_values(const EventLog& log, std::string_view attribute_key) {
  AttributeStatisticsOptions options;
  options.scope = AttributeStatisticsScope::kTraces;
  options.attribute_key = std::string(attribute_key);
  return discover_attribute_statistics(log, options).values;
}

[[nodiscard]] const char* attribute_statistics_scope_name(
    AttributeStatisticsScope scope);

}  // namespace minera
