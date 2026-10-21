#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "minera/event_data.hpp"

namespace minera {
struct PolarsPathsFilterOptions {
  EventAttribute case_id_key{"", "case:concept:name"};
  EventAttribute attribute_key{"", "concept:name"};
  EventAttribute timestamp_key{"", "time:timestamp"};
  // Plain apply defaults to the selected source attribute; performance ignores it.
  std::optional<EventAttribute> target_attribute_key;
  EventAttribute positive{"", "true", AttributeKind::kBoolean};
  EventAttribute min_performance{"", "0", AttributeKind::kInt};
  EventAttribute max_performance{"", "9223372036854775807", AttributeKind::kInt};
};
class PolarsPathsFilterError : public std::invalid_argument {
 public:
  PolarsPathsFilterError(std::string type, std::string phase, std::string message)
      : std::invalid_argument(message), type(std::move(type)), phase(std::move(phase)), message(std::move(message)) {}
  const std::string type, phase, message;
};

// Both operations sort case/timestamp before adjacency and return whole cases
// in that sorted order. Path labels retain the public comma-concatenation
// semantics, including delimiter collisions. Complete row/schema/dictionaries
// survive; native attributes and provenance are copied independently.
[[nodiscard]] EventTable filter_polars_paths(const EventTable&, const EventAttribute& paths, const PolarsPathsFilterOptions& = {});
[[nodiscard]] LazyEventTable filter_polars_paths(const LazyEventTable&, const EventAttribute& paths, const PolarsPathsFilterOptions& = {});
[[nodiscard]] EventTable filter_polars_path_performance(const EventTable&, const EventAttribute& path, const PolarsPathsFilterOptions& = {});
[[nodiscard]] LazyEventTable filter_polars_path_performance(const LazyEventTable&, const EventAttribute& path, const PolarsPathsFilterOptions& = {});

struct PolarsPathsFilterParameter { std::string key; EventAttribute value; bool enum_key = false; };
[[nodiscard]] PolarsPathsFilterOptions resolve_polars_paths_filter_parameters(const std::vector<PolarsPathsFilterParameter>&);
[[nodiscard]] EventTable filter_polars_paths_parameters(const EventTable&, const EventAttribute&, const std::vector<PolarsPathsFilterParameter>&);
[[nodiscard]] LazyEventTable filter_polars_paths_parameters(const LazyEventTable&, const EventAttribute&, const std::vector<PolarsPathsFilterParameter>&);
[[nodiscard]] EventTable filter_polars_path_performance_parameters(const EventTable&, const EventAttribute&, const std::vector<PolarsPathsFilterParameter>&);
[[nodiscard]] LazyEventTable filter_polars_path_performance_parameters(const LazyEventTable&, const EventAttribute&, const std::vector<PolarsPathsFilterParameter>&);
[[nodiscard]] EventTable filter_polars_paths_parameter_value(const EventTable&, const EventAttribute&, const EventAttribute& parameters);
[[nodiscard]] LazyEventTable filter_polars_paths_parameter_value(const LazyEventTable&, const EventAttribute&, const EventAttribute& parameters);
[[nodiscard]] EventTable filter_polars_path_performance_parameter_value(const EventTable&, const EventAttribute&, const EventAttribute& parameters);
[[nodiscard]] LazyEventTable filter_polars_path_performance_parameter_value(const LazyEventTable&, const EventAttribute&, const EventAttribute& parameters);

// A Python dictionary may have tuple keys, which the string-key attribute
// container cannot carry. This explicit input carrier keeps typed keys/values
// separate without changing the shared EventAttribute ABI or table metadata.
struct PolarsPathsFilterDictionary { std::vector<std::pair<EventAttribute, EventAttribute>> items; };
[[nodiscard]] EventTable filter_polars_paths(const EventTable&, const PolarsPathsFilterDictionary&, const PolarsPathsFilterOptions& = {});
[[nodiscard]] LazyEventTable filter_polars_paths(const LazyEventTable&, const PolarsPathsFilterDictionary&, const PolarsPathsFilterOptions& = {});
}  // namespace minera
