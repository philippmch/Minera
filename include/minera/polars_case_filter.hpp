#pragma once

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "minera/event_data.hpp"

namespace minera {

struct PolarsCaseFilterOptions {
  EventAttribute case_id_key{"", "case:concept:name"};
  EventAttribute timestamp_key{"", "time:timestamp"};
  EventAttribute business_hours{"", "false", AttributeKind::kBoolean};
  EventAttribute business_hour_slots = [] { EventAttribute v; v.is_null = true; return v; }();
};

class PolarsCaseFilterError : public std::invalid_argument {
 public:
  PolarsCaseFilterError(std::string type, std::string phase, std::string message)
      : std::invalid_argument(message), type(std::move(type)), phase(std::move(phase)), message(std::move(message)) {}
  const std::string type, phase, message;
};

// Count-ranked selection retains complete rows in source order. Equal-count
// groups use encounter order natively; the public grouping tie order is not
// specified, and null groups can consume a limit slot before the inner join.
[[nodiscard]] EventTable filter_polars_ncases(const EventTable&, const EventAttribute& maximum = {"", "1000", AttributeKind::kInt}, const PolarsCaseFilterOptions& = {});
[[nodiscard]] LazyEventTable filter_polars_ncases(const LazyEventTable&, const EventAttribute& maximum = {"", "1000", AttributeKind::kInt}, const PolarsCaseFilterOptions& = {});
[[nodiscard]] EventTable filter_polars_case_size(const EventTable&, const EventAttribute& minimum = {"", "2", AttributeKind::kInt}, const EventAttribute& maximum = [] { EventAttribute v; v.is_null = true; return v; }(), const PolarsCaseFilterOptions& = {});
[[nodiscard]] LazyEventTable filter_polars_case_size(const LazyEventTable&, const EventAttribute& minimum = {"", "2", AttributeKind::kInt}, const EventAttribute& maximum = [] { EventAttribute v; v.is_null = true; return v; }(), const PolarsCaseFilterOptions& = {});
[[nodiscard]] EventTable filter_polars_case_performance(const EventTable&, const EventAttribute& minimum = {"", "0", AttributeKind::kInt}, const EventAttribute& maximum = {"", "10000000000", AttributeKind::kInt}, const PolarsCaseFilterOptions& = {});
[[nodiscard]] LazyEventTable filter_polars_case_performance(const LazyEventTable&, const EventAttribute& minimum = {"", "0", AttributeKind::kInt}, const EventAttribute& maximum = {"", "10000000000", AttributeKind::kInt}, const PolarsCaseFilterOptions& = {});

// Preserve the public wrapper's enum-key precedence without encoding Python
// enum identities into table metadata. Keys use public Parameters.value text.
struct PolarsCaseFilterParameter { std::string key; EventAttribute value; bool enum_key = false; };
[[nodiscard]] PolarsCaseFilterOptions resolve_polars_case_filter_parameters(const std::vector<PolarsCaseFilterParameter>&);
[[nodiscard]] EventTable filter_polars_case_performance_parameters(const EventTable&, const EventAttribute&, const EventAttribute&, const std::vector<PolarsCaseFilterParameter>& = {});
[[nodiscard]] LazyEventTable filter_polars_case_performance_parameters(const LazyEventTable&, const EventAttribute&, const EventAttribute&, const std::vector<PolarsCaseFilterParameter>& = {});
[[nodiscard]] EventTable filter_polars_case_performance_parameter_value(const EventTable&, const EventAttribute&, const EventAttribute&, const EventAttribute&);
[[nodiscard]] LazyEventTable filter_polars_case_performance_parameter_value(const LazyEventTable&, const EventAttribute&, const EventAttribute&, const EventAttribute&);

// These public entry points explicitly reject before scanning their input.
[[nodiscard]] EventTable apply_polars_case_filter(const EventTable&, const EventAttribute& = {});
[[nodiscard]] LazyEventTable apply_polars_case_filter(const LazyEventTable&, const EventAttribute& = {});
[[nodiscard]] EventTable auto_filter_polars_cases(const EventTable&, const EventAttribute& = {});
[[nodiscard]] LazyEventTable auto_filter_polars_cases(const LazyEventTable&, const EventAttribute& = {});

}  // namespace minera
