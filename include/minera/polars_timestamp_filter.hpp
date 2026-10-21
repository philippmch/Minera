#pragma once

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "minera/event_data.hpp"

namespace minera {
struct PolarsTimestampFilterOptions {
  EventAttribute timestamp_key{"", "time:timestamp"};
  EventAttribute case_id_key{"", "case:concept:name"};
};
class PolarsTimestampFilterError : public std::invalid_argument {
 public:
  PolarsTimestampFilterError(std::string type, std::string phase, std::string message)
      : std::invalid_argument(message), type(std::move(type)), phase(std::move(phase)), message(std::move(message)) {}
  const std::string type, phase, message;
};

[[nodiscard]] EventTable filter_polars_timestamp_events(const EventTable&, const EventAttribute& lower, const EventAttribute& upper, const PolarsTimestampFilterOptions& = {});
[[nodiscard]] LazyEventTable filter_polars_timestamp_events(const LazyEventTable&, const EventAttribute& lower, const EventAttribute& upper, const PolarsTimestampFilterOptions& = {});
[[nodiscard]] EventTable filter_polars_timestamp_contained(const EventTable&, const EventAttribute& lower, const EventAttribute& upper, const PolarsTimestampFilterOptions& = {});
[[nodiscard]] LazyEventTable filter_polars_timestamp_contained(const LazyEventTable&, const EventAttribute& lower, const EventAttribute& upper, const PolarsTimestampFilterOptions& = {});
[[nodiscard]] EventTable filter_polars_timestamp_intersecting(const EventTable&, const EventAttribute& lower, const EventAttribute& upper, const PolarsTimestampFilterOptions& = {});
[[nodiscard]] LazyEventTable filter_polars_timestamp_intersecting(const LazyEventTable&, const EventAttribute& lower, const EventAttribute& upper, const PolarsTimestampFilterOptions& = {});
[[nodiscard]] EventTable filter_polars_timestamp_attribute(const EventTable&, const EventAttribute& attribute, const EventAttribute& value, const EventAttribute& lower, const EventAttribute& upper, const PolarsTimestampFilterOptions& = {});
[[nodiscard]] LazyEventTable filter_polars_timestamp_attribute(const LazyEventTable&, const EventAttribute& attribute, const EventAttribute& value, const EventAttribute& lower, const EventAttribute& upper, const PolarsTimestampFilterOptions& = {});
[[nodiscard]] EventTable apply_polars_timestamp_filter(const EventTable&, const EventAttribute& parameters = {});
[[nodiscard]] LazyEventTable apply_polars_timestamp_filter(const LazyEventTable&, const EventAttribute& parameters = {});
[[nodiscard]] EventTable auto_filter_polars_timestamp(const EventTable&, const EventAttribute& parameters = {});
[[nodiscard]] LazyEventTable auto_filter_polars_timestamp(const LazyEventTable&, const EventAttribute& parameters = {});

struct PolarsTimestampFilterParameter { std::string key; EventAttribute value; bool enum_key = false; };
[[nodiscard]] PolarsTimestampFilterOptions resolve_polars_timestamp_filter_parameters(const std::vector<PolarsTimestampFilterParameter>&);
[[nodiscard]] EventTable filter_polars_timestamp_events_parameters(const EventTable&, const EventAttribute&, const EventAttribute&, const std::vector<PolarsTimestampFilterParameter>&);
[[nodiscard]] LazyEventTable filter_polars_timestamp_events_parameters(const LazyEventTable&, const EventAttribute&, const EventAttribute&, const std::vector<PolarsTimestampFilterParameter>&);
[[nodiscard]] EventTable filter_polars_timestamp_contained_parameters(const EventTable&, const EventAttribute&, const EventAttribute&, const std::vector<PolarsTimestampFilterParameter>&);
[[nodiscard]] LazyEventTable filter_polars_timestamp_contained_parameters(const LazyEventTable&, const EventAttribute&, const EventAttribute&, const std::vector<PolarsTimestampFilterParameter>&);
[[nodiscard]] EventTable filter_polars_timestamp_intersecting_parameters(const EventTable&, const EventAttribute&, const EventAttribute&, const std::vector<PolarsTimestampFilterParameter>&);
[[nodiscard]] LazyEventTable filter_polars_timestamp_intersecting_parameters(const LazyEventTable&, const EventAttribute&, const EventAttribute&, const std::vector<PolarsTimestampFilterParameter>&);
[[nodiscard]] EventTable filter_polars_timestamp_attribute_parameters(const EventTable&, const EventAttribute&, const EventAttribute&, const EventAttribute&, const EventAttribute&, const std::vector<PolarsTimestampFilterParameter>&);
[[nodiscard]] LazyEventTable filter_polars_timestamp_attribute_parameters(const LazyEventTable&, const EventAttribute&, const EventAttribute&, const EventAttribute&, const EventAttribute&, const std::vector<PolarsTimestampFilterParameter>&);
[[nodiscard]] EventTable filter_polars_timestamp_events_parameter_value(const EventTable&, const EventAttribute&, const EventAttribute&, const EventAttribute&);
[[nodiscard]] LazyEventTable filter_polars_timestamp_events_parameter_value(const LazyEventTable&, const EventAttribute&, const EventAttribute&, const EventAttribute&);
[[nodiscard]] EventTable filter_polars_timestamp_contained_parameter_value(const EventTable&, const EventAttribute&, const EventAttribute&, const EventAttribute&);
[[nodiscard]] LazyEventTable filter_polars_timestamp_contained_parameter_value(const LazyEventTable&, const EventAttribute&, const EventAttribute&, const EventAttribute&);
[[nodiscard]] EventTable filter_polars_timestamp_intersecting_parameter_value(const EventTable&, const EventAttribute&, const EventAttribute&, const EventAttribute&);
[[nodiscard]] LazyEventTable filter_polars_timestamp_intersecting_parameter_value(const LazyEventTable&, const EventAttribute&, const EventAttribute&, const EventAttribute&);
[[nodiscard]] EventTable filter_polars_timestamp_attribute_parameter_value(const EventTable&, const EventAttribute&, const EventAttribute&, const EventAttribute&, const EventAttribute&, const EventAttribute&);
[[nodiscard]] LazyEventTable filter_polars_timestamp_attribute_parameter_value(const LazyEventTable&, const EventAttribute&, const EventAttribute&, const EventAttribute&, const EventAttribute&, const EventAttribute&);
}  // namespace minera
