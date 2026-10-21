#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "minera/event_data.hpp"

namespace minera {

struct PolarsLtlFilterOptions {
  EventAttribute case_id_key{"", "case:concept:name"};
  EventAttribute attribute_key{"", "concept:name"};
  EventAttribute timestamp_key{"", "time:timestamp"};
  EventAttribute resource_key{"", "org:resource"};
  EventAttribute positive{"", "true", AttributeKind::kBoolean};
  EventAttribute enable_timestamp{"", "false", AttributeKind::kBoolean};
  std::optional<EventAttribute> timestamp_diff_boundaries;
  bool operator==(const PolarsLtlFilterOptions&) const = default;
};

class PolarsLtlFilterError : public std::invalid_argument {
 public:
  PolarsLtlFilterError(std::string type, std::string phase, std::string message)
      : std::invalid_argument(message), type(std::move(type)),
        phase(std::move(phase)), message(std::move(message)) {}
  const std::string type, phase, message;
};

enum class PolarsLtlFilterOperation {
  kEventuallyFollows, kNext, kFourEyes, kDifferentPersons
};
struct PolarsLtlFilterParameter {
  std::string key;
  EventAttribute value;
  bool enum_key = false;
  bool operator==(const PolarsLtlFilterParameter&) const = default;
};
// The generic call preserves missing required operands and raw parameter values
// for public diagnostic compatibility. Friendly operations require their operand.
struct PolarsLtlFilterCall {
  PolarsLtlFilterOperation operation = PolarsLtlFilterOperation::kEventuallyFollows;
  std::optional<EventAttribute> values;
  PolarsLtlFilterOptions options;
  std::optional<std::vector<PolarsLtlFilterParameter>> parameters;
  std::optional<EventAttribute> parameter_value;
  bool operator==(const PolarsLtlFilterCall&) const = default;
};

[[nodiscard]] EventTable invoke_polars_ltl_filter(const EventTable&, const PolarsLtlFilterCall&);
[[nodiscard]] LazyEventTable invoke_polars_ltl_filter(const LazyEventTable&, const PolarsLtlFilterCall&);
[[nodiscard]] EventTable filter_polars_ltl_eventually_follows(const EventTable&, const EventAttribute& attribute_values, const PolarsLtlFilterOptions& = {});
[[nodiscard]] LazyEventTable filter_polars_ltl_eventually_follows(const LazyEventTable&, const EventAttribute& attribute_values, const PolarsLtlFilterOptions& = {});
[[nodiscard]] EventTable filter_polars_ltl_next(const EventTable&, const EventAttribute& attribute_values, const PolarsLtlFilterOptions& = {});
[[nodiscard]] LazyEventTable filter_polars_ltl_next(const LazyEventTable&, const EventAttribute& attribute_values, const PolarsLtlFilterOptions& = {});
[[nodiscard]] EventTable filter_polars_ltl_four_eyes(const EventTable&, const EventAttribute& attribute_values, const PolarsLtlFilterOptions& = {});
[[nodiscard]] LazyEventTable filter_polars_ltl_four_eyes(const LazyEventTable&, const EventAttribute& attribute_values, const PolarsLtlFilterOptions& = {});
[[nodiscard]] EventTable filter_polars_ltl_different_persons(const EventTable&, const EventAttribute& attribute_value, const PolarsLtlFilterOptions& = {});
[[nodiscard]] LazyEventTable filter_polars_ltl_different_persons(const LazyEventTable&, const EventAttribute& attribute_value, const PolarsLtlFilterOptions& = {});

}  // namespace minera
