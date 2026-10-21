#pragma once
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "minera/event_data.hpp"

namespace minera {
struct PandasLtlFilterOptions {
  EventAttribute case_id_key{"", "case:concept:name"};
  EventAttribute attribute_key{"", "concept:name"};
  EventAttribute timestamp_key{"", "time:timestamp"};
  EventAttribute resource_key{"", "org:resource"};
  EventAttribute positive{"", "true", AttributeKind::kBoolean};
  std::optional<EventAttribute> enable_timestamp;
  std::optional<EventAttribute> timestamp_diff_boundaries;
};
struct PandasLtlFilterParameter { EventAttribute value; bool enum_key = false; };
class PandasLtlFilterError : public std::invalid_argument {
 public:
  PandasLtlFilterError(std::string kind, std::string detail)
      : std::invalid_argument(detail), type(std::move(kind)), message(std::move(detail)) {}
  const std::string type;
  const std::string message;
};
[[nodiscard]] EventTable filter_pandas_ltl_eventually_follows(const EventTable&, const EventAttribute& pattern, const PandasLtlFilterOptions& = {});
[[nodiscard]] EventTable filter_pandas_ltl_next(const EventTable&, const EventAttribute& a, const EventAttribute& b, const EventAttribute& c, const PandasLtlFilterOptions& = {});
[[nodiscard]] EventTable filter_pandas_ltl_four_eyes(const EventTable&, const EventAttribute& a, const EventAttribute& b, const PandasLtlFilterOptions& = {});
[[nodiscard]] EventTable filter_pandas_ltl_different_persons(const EventTable&, const EventAttribute& a, const PandasLtlFilterOptions& = {});
[[nodiscard]] EventTable filter_pandas_ltl_eventually_follows(const EventTable&, const EventAttribute&, const EventAttribute& parameters);
[[nodiscard]] EventTable filter_pandas_ltl_next(const EventTable&, const EventAttribute&, const EventAttribute&, const EventAttribute&, const EventAttribute& parameters);
[[nodiscard]] EventTable filter_pandas_ltl_four_eyes(const EventTable&, const EventAttribute&, const EventAttribute&, const EventAttribute& parameters);
[[nodiscard]] EventTable filter_pandas_ltl_different_persons(const EventTable&, const EventAttribute&, const EventAttribute& parameters);
[[nodiscard]] EventTable filter_pandas_ltl_eventually_follows(const EventTable&, const EventAttribute&, const std::vector<PandasLtlFilterParameter>&);
[[nodiscard]] EventTable filter_pandas_ltl_next(const EventTable&, const EventAttribute&, const EventAttribute&, const EventAttribute&, const std::vector<PandasLtlFilterParameter>&);
[[nodiscard]] EventTable filter_pandas_ltl_four_eyes(const EventTable&, const EventAttribute&, const EventAttribute&, const std::vector<PandasLtlFilterParameter>&);
[[nodiscard]] EventTable filter_pandas_ltl_different_persons(const EventTable&, const EventAttribute&, const std::vector<PandasLtlFilterParameter>&);
}  // namespace minera
