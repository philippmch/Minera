#pragma once

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

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

// One distinct value under Python dict-key equality: 1, 1.0 and True (and
// 0, 0.0 and False) are one key, while the string '1' stays apart. `value`
// and `type` (int, float, bool, str or date) are those of the first cell seen
// for that key, as a Python dict keeps its first key.
struct AttributeValueCount {
  std::string value;
  std::string type;
  std::uint64_t count = 0;
};

struct AttributeStatisticsResult {
  AttributeStatisticsScope scope = AttributeStatisticsScope::kEvents;
  std::string attribute_key;
  bool keep_once_per_case = false;
  // Counts by the first-seen text of each key. Distinct keys that only share
  // a text (the string '1' and the int 1) add up here; `entries` keeps them
  // apart.
  std::map<std::string, std::uint64_t> values;
  // Every distinct key in the reference DataFrame's value_counts order:
  // descending count, equal counts in first appearance over the rows (with
  // keep_once_per_case, over the sorted [case id, value] groups).
  std::vector<AttributeValueCount> entries;
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

[[nodiscard]] std::set<std::string> get_all_event_attributes_from_log(
    const EventLog& log);
[[nodiscard]] std::set<std::string> get_all_trace_attributes_from_log(
    const EventLog& log);

}  // namespace minera
