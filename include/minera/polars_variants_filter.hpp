#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "minera/event_data.hpp"

namespace minera {
struct PolarsVariantsFilterOptions {
  EventAttribute case_id_key{"", "case:concept:name"};
  EventAttribute activity_key{"", "concept:name"};
  // Only top-k statistics uses this selector. Membership sorts canonical time.
  EventAttribute timestamp_key{"", "time:timestamp"};
  EventAttribute positive{"", "true", AttributeKind::kBoolean};
};
class PolarsVariantsFilterError : public std::invalid_argument {
 public:
  PolarsVariantsFilterError(std::string type, std::string phase, std::string message)
      : std::invalid_argument(message), type(std::move(type)), phase(std::move(phase)), message(std::move(message)) {}
  const std::string type, phase, message;
};
struct PolarsVariantsFilterParameter { std::string key; EventAttribute value; bool enum_key = false; };
[[nodiscard]] PolarsVariantsFilterOptions resolve_polars_variants_filter_parameters(const std::vector<PolarsVariantsFilterParameter>&);
[[nodiscard]] EventTable filter_polars_variants(const EventTable&, const EventAttribute&, const PolarsVariantsFilterOptions& = {});
[[nodiscard]] EventTable filter_polars_variants_parameters(const EventTable&, const EventAttribute&, const std::vector<PolarsVariantsFilterParameter>&);
[[nodiscard]] EventTable filter_polars_variants_parameter_value(const EventTable&, const EventAttribute&, const EventAttribute&);
[[nodiscard]] EventTable filter_polars_variants(const EventTable&, std::nullopt_t, const PolarsVariantsFilterOptions& = {});
[[nodiscard]] LazyEventTable filter_polars_variants(const LazyEventTable&, const EventAttribute&, const PolarsVariantsFilterOptions& = {});
[[nodiscard]] LazyEventTable filter_polars_variants_parameters(const LazyEventTable&, const EventAttribute&, const std::vector<PolarsVariantsFilterParameter>&);
[[nodiscard]] LazyEventTable filter_polars_variants_parameter_value(const LazyEventTable&, const EventAttribute&, const EventAttribute&);
[[nodiscard]] LazyEventTable filter_polars_variants(const LazyEventTable&, std::nullopt_t, const PolarsVariantsFilterOptions& = {});
[[nodiscard]] EventTable filter_polars_variants_from_list(const EventTable&, const EventAttribute&, const PolarsVariantsFilterOptions& = {});
[[nodiscard]] EventTable filter_polars_variants_from_list_parameters(const EventTable&, const EventAttribute&, const std::vector<PolarsVariantsFilterParameter>&);
[[nodiscard]] EventTable filter_polars_variants_from_list_parameter_value(const EventTable&, const EventAttribute&, const EventAttribute&);
[[nodiscard]] EventTable filter_polars_variants_from_list(const EventTable&, std::nullopt_t, const PolarsVariantsFilterOptions& = {});
[[nodiscard]] LazyEventTable filter_polars_variants_from_list(const LazyEventTable&, const EventAttribute&, const PolarsVariantsFilterOptions& = {});
[[nodiscard]] LazyEventTable filter_polars_variants_from_list_parameters(const LazyEventTable&, const EventAttribute&, const std::vector<PolarsVariantsFilterParameter>&);
[[nodiscard]] LazyEventTable filter_polars_variants_from_list_parameter_value(const LazyEventTable&, const EventAttribute&, const EventAttribute&);
[[nodiscard]] LazyEventTable filter_polars_variants_from_list(const LazyEventTable&, std::nullopt_t, const PolarsVariantsFilterOptions& = {});
[[nodiscard]] EventTable filter_polars_variants_top_k(const EventTable&, const EventAttribute&, const PolarsVariantsFilterOptions& = {});
[[nodiscard]] EventTable filter_polars_variants_top_k_parameters(const EventTable&, const EventAttribute&, const std::vector<PolarsVariantsFilterParameter>&);
[[nodiscard]] EventTable filter_polars_variants_top_k_parameter_value(const EventTable&, const EventAttribute&, const EventAttribute&);
[[nodiscard]] EventTable filter_polars_variants_top_k(const EventTable&, std::nullopt_t, const PolarsVariantsFilterOptions& = {});
[[nodiscard]] LazyEventTable filter_polars_variants_top_k(const LazyEventTable&, const EventAttribute&, const PolarsVariantsFilterOptions& = {});
[[nodiscard]] LazyEventTable filter_polars_variants_top_k_parameters(const LazyEventTable&, const EventAttribute&, const std::vector<PolarsVariantsFilterParameter>&);
[[nodiscard]] LazyEventTable filter_polars_variants_top_k_parameter_value(const LazyEventTable&, const EventAttribute&, const EventAttribute&);
[[nodiscard]] LazyEventTable filter_polars_variants_top_k(const LazyEventTable&, std::nullopt_t, const PolarsVariantsFilterOptions& = {});
[[nodiscard]] EventTable auto_filter_polars_variants(const EventTable&, const PolarsVariantsFilterOptions& = {});
[[nodiscard]] EventTable auto_filter_polars_variants_parameters(const EventTable&, const std::vector<PolarsVariantsFilterParameter>&);
[[nodiscard]] EventTable auto_filter_polars_variants_parameter_value(const EventTable&, const EventAttribute&);
[[nodiscard]] LazyEventTable auto_filter_polars_variants(const LazyEventTable&, const PolarsVariantsFilterOptions& = {});
[[nodiscard]] LazyEventTable auto_filter_polars_variants_parameters(const LazyEventTable&, const std::vector<PolarsVariantsFilterParameter>&);
[[nodiscard]] LazyEventTable auto_filter_polars_variants_parameter_value(const LazyEventTable&, const EventAttribute&);
}  // namespace minera
