#pragma once
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>
#include "minera/event_data.hpp"

namespace minera {
struct PandasAttributesCountMap {
  std::vector<std::pair<EventAttribute, EventAttribute>> entries;
};
[[nodiscard]] PandasAttributesCountMap normalize_pandas_attributes_count_map(const PandasAttributesCountMap&);
struct PandasAttributesFilterOptions {
  EventAttribute case_id_key{"", "case:concept:name"};
  EventAttribute attribute_key{"", "concept:name"};
  EventAttribute activity_key{"", "concept:name"};
  EventAttribute positive{"", "true", AttributeKind::kBoolean};
  EventAttribute keep_nan_values{"", "false", AttributeKind::kBoolean};
  EventAttribute keep_once_per_case{"", "true", AttributeKind::kBoolean};
  std::optional<EventAttribute> stream_filter_key1;
  std::optional<EventAttribute> stream_filter_value1;
  std::optional<EventAttribute> stream_filter_key2;
  std::optional<EventAttribute> stream_filter_value2;
  std::optional<std::variant<EventAttribute, PandasAttributesCountMap>> act_count0;
  std::optional<EventAttribute> most_common_variant;
};
struct PandasAttributesFilterParameter { EventAttribute value; bool enum_key = false; };
class PandasAttributesFilterError : public std::invalid_argument {
 public:
  PandasAttributesFilterError(std::string kind, std::string detail)
      :std::invalid_argument(detail),type(std::move(kind)),message(std::move(detail)){}
  const std::string type;
  const std::string message;
};

[[nodiscard]] EventTable filter_pandas_attributes(const EventTable&, const EventAttribute& values, const PandasAttributesFilterOptions& = {});
[[nodiscard]] EventTable filter_pandas_attributes_events(const EventTable&, const EventAttribute& values, const PandasAttributesFilterOptions& = {});
[[nodiscard]] EventTable filter_pandas_attributes_numeric(const EventTable&, const EventAttribute& lower, const EventAttribute& upper, const PandasAttributesFilterOptions& = {});
[[nodiscard]] EventTable filter_pandas_attributes_numeric_events(const EventTable&, const EventAttribute& lower, const EventAttribute& upper, const PandasAttributesFilterOptions& = {});
[[nodiscard]] EventTable filter_pandas_attributes_threshold(const EventTable&, const EventAttribute& threshold, const PandasAttributesFilterOptions& = {});
[[nodiscard]] EventTable filter_pandas_attributes_top_activities(const EventTable&, const EventAttribute& maximum = EventAttribute{"", "25", AttributeKind::kInt}, const PandasAttributesFilterOptions& = {});
[[nodiscard]] EventTable filter_pandas_attributes_on_values(const EventTable&, const EventAttribute& values, const PandasAttributesFilterOptions& = {});
[[nodiscard]] EventTable filter_pandas_attributes_relative_occurrence(const EventTable&, const EventAttribute& stake, const PandasAttributesFilterOptions& = {});
[[nodiscard]] EventTable filter_pandas_attributes(const EventTable&, const EventAttribute&, const EventAttribute& parameters);
[[nodiscard]] EventTable filter_pandas_attributes_events(const EventTable&, const EventAttribute&, const EventAttribute& parameters);
[[nodiscard]] EventTable filter_pandas_attributes_numeric(const EventTable&, const EventAttribute&, const EventAttribute&, const EventAttribute& parameters);
[[nodiscard]] EventTable filter_pandas_attributes_numeric_events(const EventTable&, const EventAttribute&, const EventAttribute&, const EventAttribute& parameters);
[[nodiscard]] EventTable filter_pandas_attributes_relative_occurrence(const EventTable&, const EventAttribute&, const EventAttribute& parameters);
[[nodiscard]] EventTable filter_pandas_attributes(const EventTable&, const EventAttribute&, const std::vector<PandasAttributesFilterParameter>&);
[[nodiscard]] EventTable filter_pandas_attributes_events(const EventTable&, const EventAttribute&, const std::vector<PandasAttributesFilterParameter>&);
[[nodiscard]] EventTable filter_pandas_attributes_numeric(const EventTable&, const EventAttribute&, const EventAttribute&, const std::vector<PandasAttributesFilterParameter>&);
[[nodiscard]] EventTable filter_pandas_attributes_numeric_events(const EventTable&, const EventAttribute&, const EventAttribute&, const std::vector<PandasAttributesFilterParameter>&);
[[nodiscard]] EventTable filter_pandas_attributes_relative_occurrence(const EventTable&, const EventAttribute&, const std::vector<PandasAttributesFilterParameter>&);
}  // namespace minera
