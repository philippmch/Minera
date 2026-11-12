#include "minera/rework.hpp"
#include "reference_activity_selector.hpp"

#include <map>
#include <stdexcept>
#include <string_view>
#include <unordered_map>

namespace minera {
namespace {

std::string activity_value(const Event& event, std::string_view key) {
  if (key.empty() || key == "activity" || key == "concept:name")
    return event.activity;
  if (key == "resource") return event.resource;
  if (const auto* value = event.attribute(key)) return *value;
  // A CSV's own header of a selected column (its source key; round BP).
  if (const auto* value = reference_selected_field(event, key)) return *value;
  throw std::invalid_argument("rework: event is missing activity attribute '" +
                              std::string(key) + "'");
}

std::string case_value(const Trace& trace, std::string_view key) {
  if (key.empty() || key == "case_id" || key == "concept:name")
    return trace.case_id;
  if (const auto* value = trace.attribute(key)) return *value;
  throw std::invalid_argument("rework: trace is missing case id attribute '" +
                              std::string(key) + "'");
}

}  // namespace

const char* rework_variant_name(ReworkVariant variant) {
  switch (variant) {
    case ReworkVariant::kActivities: return "ACTIVITIES";
    case ReworkVariant::kCases: return "CASES";
  }
  throw std::invalid_argument("rework: unknown variant");
}

ReworkResult discover_rework(const EventLog& log,
                             const ReworkOptions& options) {
  ReworkResult result;
  result.variant = options.variant;
  if (options.variant == ReworkVariant::kActivities) {
    for (const auto& trace : log.traces) {
      std::map<std::string, std::uint64_t> counts;
      for (const auto& event : trace.events)
        ++counts[activity_value(event, options.activity_key)];
      for (const auto& [activity, count] : counts)
        if (count > 1) ++result.activity_cases[activity];
    }
    return result;
  }
  if (options.variant == ReworkVariant::kCases) {
    for (const auto& trace : log.traces) {
      std::map<std::string, std::uint64_t> counts;
      for (const auto& event : trace.events)
        ++counts[activity_value(event, options.activity_key)];
      const auto case_id = case_value(trace, options.case_id_key);
      ReworkCaseStats stats;
      stats.number_activities = trace.events.size();
      std::uint64_t unique = 0;
      for (const auto& [unused, count] : counts) {
        (void)unused;
        if (count != 0) ++unique;
      }
      stats.rework = stats.number_activities - unique;
      result.cases[case_id] = stats;
    }
    return result;
  }
  throw std::invalid_argument("rework: unknown variant");
}

}  // namespace minera
