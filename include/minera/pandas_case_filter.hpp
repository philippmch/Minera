#pragma once

#include <stdexcept>
#include <string>
#include <utility>

#include "minera/event_data.hpp"

namespace minera {

struct PandasCaseFilterOptions {
  EventAttribute case_id_key{"", "case:concept:name"};
  EventAttribute timestamp_key{"", "time:timestamp"};
  EventAttribute business_hours{"", "false", AttributeKind::kBoolean};
  EventAttribute business_hour_slots;
  PandasCaseFilterOptions();
};

class PandasCaseFilterError : public std::invalid_argument {
 public:
  PandasCaseFilterError(std::string kind, std::string detail)
      : std::invalid_argument(detail), type(std::move(kind)), message(std::move(detail)) {}
  const std::string type;
  const std::string message;
};

[[nodiscard]] EventTable filter_pandas_case_size(
    const EventTable& source,
    const EventAttribute& minimum = EventAttribute{"", "2", AttributeKind::kInt},
    const EventAttribute& maximum = EventAttribute{"", "", AttributeKind::kString, true},
    const PandasCaseFilterOptions& options = {});
[[nodiscard]] EventTable filter_pandas_ncases(
    const EventTable& source,
    const EventAttribute& maximum = EventAttribute{"", "1000", AttributeKind::kInt},
    const PandasCaseFilterOptions& options = {});
[[nodiscard]] EventTable filter_pandas_case_performance(
    const EventTable& source,
    const EventAttribute& minimum = EventAttribute{"", "0", AttributeKind::kInt},
    const EventAttribute& maximum = EventAttribute{"", "10000000000", AttributeKind::kInt},
    const PandasCaseFilterOptions& options = {});
// Public filter_case_performance wrapper: literal/enum-value parameter keys.
[[nodiscard]] EventTable filter_pandas_case_performance(
    const EventTable& source, const EventAttribute& minimum,
    const EventAttribute& maximum, const EventAttribute& parameters);

// The pinned public functions reject before inspecting the dataframe/controls.
[[nodiscard]] EventTable apply_pandas_case_filter(
    const EventTable& source,
    const EventAttribute& parameters = EventAttribute{"", "", AttributeKind::kString, true});
[[nodiscard]] EventTable auto_filter_pandas_cases(
    const EventTable& source,
    const EventAttribute& parameters = EventAttribute{"", "", AttributeKind::kString, true});

}  // namespace minera
