#pragma once
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "minera/event_data.hpp"
namespace minera {
struct PandasTimestampFilterOptions {
  EventAttribute timestamp_key{"", "time:timestamp"};
  EventAttribute case_id_key{"", "case:concept:name"};
  EventAttribute positive{"", "true", AttributeKind::kBoolean};
  EventAttribute keep_nan_values{"", "false", AttributeKind::kBoolean};
};
struct PandasTimestampFilterParameter { EventAttribute value; bool enum_key = false; };
class PandasTimestampFilterError : public std::invalid_argument {
 public:
  PandasTimestampFilterError(std::string kind, std::string detail)
      :std::invalid_argument(detail),type(std::move(kind)),message(std::move(detail)){}
  const std::string type;
  const std::string message;
};
[[nodiscard]] EventTable filter_pandas_timestamp_events(const EventTable&, const EventAttribute& lower, const EventAttribute& upper, const PandasTimestampFilterOptions& = {});
[[nodiscard]] EventTable filter_pandas_timestamp_contained(const EventTable&, const EventAttribute& lower, const EventAttribute& upper, const PandasTimestampFilterOptions& = {});
[[nodiscard]] EventTable filter_pandas_timestamp_intersecting(const EventTable&, const EventAttribute& lower, const EventAttribute& upper, const PandasTimestampFilterOptions& = {});
[[nodiscard]] EventTable filter_pandas_timestamp_starting(const EventTable&, const EventAttribute& lower, const EventAttribute& upper, const PandasTimestampFilterOptions& = {});
[[nodiscard]] EventTable filter_pandas_timestamp_completing(const EventTable&, const EventAttribute& lower, const EventAttribute& upper, const PandasTimestampFilterOptions& = {});
[[nodiscard]] EventTable filter_pandas_timestamp_attribute(const EventTable&, const EventAttribute& attribute, const EventAttribute& value, const EventAttribute& lower, const EventAttribute& upper, const PandasTimestampFilterOptions& = {});
[[nodiscard]] EventTable filter_pandas_timestamp_events(const EventTable&, const EventAttribute&, const EventAttribute&, const EventAttribute& parameters);
[[nodiscard]] EventTable filter_pandas_timestamp_contained(const EventTable&, const EventAttribute&, const EventAttribute&, const EventAttribute& parameters);
[[nodiscard]] EventTable filter_pandas_timestamp_intersecting(const EventTable&, const EventAttribute&, const EventAttribute&, const EventAttribute& parameters);
[[nodiscard]] EventTable filter_pandas_timestamp_starting(const EventTable&, const EventAttribute&, const EventAttribute&, const EventAttribute& parameters);
[[nodiscard]] EventTable filter_pandas_timestamp_completing(const EventTable&, const EventAttribute&, const EventAttribute&, const EventAttribute& parameters);
[[nodiscard]] EventTable filter_pandas_timestamp_attribute(const EventTable&, const EventAttribute&, const EventAttribute&, const EventAttribute&, const EventAttribute&, const EventAttribute& parameters);
[[nodiscard]] EventTable filter_pandas_timestamp_events(const EventTable&, const EventAttribute&, const EventAttribute&, const std::vector<PandasTimestampFilterParameter>&);
[[nodiscard]] EventTable filter_pandas_timestamp_contained(const EventTable&, const EventAttribute&, const EventAttribute&, const std::vector<PandasTimestampFilterParameter>&);
[[nodiscard]] EventTable filter_pandas_timestamp_intersecting(const EventTable&, const EventAttribute&, const EventAttribute&, const std::vector<PandasTimestampFilterParameter>&);
[[nodiscard]] EventTable filter_pandas_timestamp_starting(const EventTable&, const EventAttribute&, const EventAttribute&, const std::vector<PandasTimestampFilterParameter>&);
[[nodiscard]] EventTable filter_pandas_timestamp_completing(const EventTable&, const EventAttribute&, const EventAttribute&, const std::vector<PandasTimestampFilterParameter>&);
[[nodiscard]] EventTable filter_pandas_timestamp_attribute(const EventTable&, const EventAttribute&, const EventAttribute&, const EventAttribute&, const EventAttribute&, const std::vector<PandasTimestampFilterParameter>&);
[[nodiscard]] EventTable apply_pandas_timestamp_filter(const EventTable&, const EventAttribute& parameters = EventAttribute{"", "", AttributeKind::kString, true});
[[nodiscard]] EventTable auto_filter_pandas_timestamp(const EventTable&, const EventAttribute& parameters = EventAttribute{"", "", AttributeKind::kString, true});
}  // namespace minera
