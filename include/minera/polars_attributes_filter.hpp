#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "minera/event_data.hpp"

namespace minera {
struct PolarsAttributesFilterOptions {
  EventAttribute attribute_key{"", "concept:name"};
  EventAttribute activity_key{"", "concept:name"};
  EventAttribute case_id_key{"", "case:concept:name"};
  EventAttribute positive{"", "true", AttributeKind::kBoolean};
  // Omission differs from explicit false for relative occurrence.
  std::optional<EventAttribute> keep_once_per_case;
  EventAttribute decreasing_factor{"", "0.6", AttributeKind::kFloat};
  std::optional<EventAttribute> stream_filter_key1, stream_filter_value1;
  std::optional<EventAttribute> stream_filter_key2, stream_filter_value2;
  bool operator==(const PolarsAttributesFilterOptions&) const = default;
};
struct PolarsAttributeCount { EventAttribute value, count; bool operator==(const PolarsAttributeCount&) const = default; };
using PolarsAttributeCounts = std::vector<PolarsAttributeCount>;
// A real typed mapping is distinct from a malformed public scalar/list control.
using PolarsAttributeCountCache = std::variant<EventAttribute, PolarsAttributeCounts>;
class PolarsAttributesFilterError : public std::invalid_argument {
 public:
  PolarsAttributesFilterError(std::string type, std::string phase, std::string message)
      : std::invalid_argument(message), type(std::move(type)), phase(std::move(phase)), message(std::move(message)) {}
  const std::string type, phase, message;
};
struct PolarsAttributesFilterParameter { std::string key; EventAttribute value; bool enum_key = false; bool operator==(const PolarsAttributesFilterParameter&) const = default; };
enum class PolarsAttributesFilterOperation {
  kTraces, kEvents, kNumericTraces, kNumericEvents, kThreshold,
  kTopActivities, kOnValues, kRelative, kCounts
};
// Lossless public call carrier also represents omitted required arguments and
// malformed parameter controls. Friendly functions below accept ordinary calls.
struct PolarsAttributesFilterCall {
  PolarsAttributesFilterOperation operation = PolarsAttributesFilterOperation::kTraces;
  PolarsAttributesFilterOptions options;
  std::optional<EventAttribute> values, lower, upper, threshold, maximum, stake, attribute;
  std::optional<PolarsAttributeCountCache> count_cache;
  std::optional<EventAttribute> most_common_variant;
  std::optional<std::vector<PolarsAttributesFilterParameter>> parameters;
  std::optional<EventAttribute> parameter_value;
  bool operator==(const PolarsAttributesFilterCall&) const = default;
};
using PolarsAttributesFilterResult = std::variant<EventTable, PolarsAttributeCounts>;
using LazyPolarsAttributesFilterResult = std::variant<LazyEventTable, PolarsAttributeCounts>;
[[nodiscard]] PolarsAttributesFilterResult invoke_polars_attributes_filter(const EventTable&, const PolarsAttributesFilterCall&);
[[nodiscard]] LazyPolarsAttributesFilterResult invoke_polars_attributes_filter(const LazyEventTable&, const PolarsAttributesFilterCall&);
[[nodiscard]] EventTable filter_polars_attributes(const EventTable&, const EventAttribute&, const PolarsAttributesFilterOptions& = {});
[[nodiscard]] LazyEventTable filter_polars_attributes(const LazyEventTable&, const EventAttribute&, const PolarsAttributesFilterOptions& = {});
[[nodiscard]] EventTable filter_polars_attributes_events(const EventTable&, const EventAttribute&, const PolarsAttributesFilterOptions& = {});
[[nodiscard]] LazyEventTable filter_polars_attributes_events(const LazyEventTable&, const EventAttribute&, const PolarsAttributesFilterOptions& = {});
[[nodiscard]] EventTable filter_polars_attributes_numeric(const EventTable&, const EventAttribute&, const EventAttribute&, const PolarsAttributesFilterOptions& = {});
[[nodiscard]] LazyEventTable filter_polars_attributes_numeric(const LazyEventTable&, const EventAttribute&, const EventAttribute&, const PolarsAttributesFilterOptions& = {});
[[nodiscard]] EventTable filter_polars_attributes_numeric_events(const EventTable&, const EventAttribute&, const EventAttribute&, const PolarsAttributesFilterOptions& = {});
[[nodiscard]] LazyEventTable filter_polars_attributes_numeric_events(const LazyEventTable&, const EventAttribute&, const EventAttribute&, const PolarsAttributesFilterOptions& = {});
[[nodiscard]] EventTable filter_polars_attributes_on_values(const EventTable&, const EventAttribute&, const PolarsAttributesFilterOptions& = {});
[[nodiscard]] LazyEventTable filter_polars_attributes_on_values(const LazyEventTable&, const EventAttribute&, const PolarsAttributesFilterOptions& = {});
[[nodiscard]] EventTable filter_polars_attributes_threshold(const EventTable&, const EventAttribute&, const PolarsAttributesFilterOptions& = {}, const std::optional<PolarsAttributeCountCache>& = {}, const std::optional<EventAttribute>& = {});
[[nodiscard]] LazyEventTable filter_polars_attributes_threshold(const LazyEventTable&, const EventAttribute&, const PolarsAttributesFilterOptions& = {}, const std::optional<PolarsAttributeCountCache>& = {}, const std::optional<EventAttribute>& = {});
[[nodiscard]] EventTable filter_polars_attributes_top_activities(const EventTable&, const EventAttribute& maximum = {"", "25", AttributeKind::kInt}, const PolarsAttributesFilterOptions& = {});
[[nodiscard]] LazyEventTable filter_polars_attributes_top_activities(const LazyEventTable&, const EventAttribute& maximum = {"", "25", AttributeKind::kInt}, const PolarsAttributesFilterOptions& = {});
[[nodiscard]] EventTable filter_polars_attributes_relative(const EventTable&, const EventAttribute&, const PolarsAttributesFilterOptions& = {});
[[nodiscard]] LazyEventTable filter_polars_attributes_relative(const LazyEventTable&, const EventAttribute&, const PolarsAttributesFilterOptions& = {});
[[nodiscard]] PolarsAttributeCounts get_polars_attribute_values(const EventTable&, const EventAttribute&, const PolarsAttributesFilterOptions& = {});
[[nodiscard]] PolarsAttributeCounts get_polars_attribute_values(const LazyEventTable&, const EventAttribute&, const PolarsAttributesFilterOptions& = {});
}  // namespace minera
