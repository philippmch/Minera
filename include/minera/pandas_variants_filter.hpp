#pragma once
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>
#include "minera/event_data.hpp"

namespace minera {
struct PandasVariantsFilterOptions {
  EventAttribute case_id_key{"", "case:concept:name"};
  EventAttribute activity_key{"", "concept:name"};
  EventAttribute positive{"", "true", AttributeKind::kBoolean};
  std::optional<EventAttribute> decreasing_factor;
  std::optional<std::variant<EventAttribute, EventTable>> variants_df;
};
struct PandasVariantsFilterParameter {
  EventAttribute value;
  bool enum_key = false;
  std::optional<EventTable> table;
};
struct PandasVariantsAdmittedMapping {
  std::vector<std::pair<EventAttribute, EventAttribute>> entries;
};
class PandasVariantsFilterError : public std::invalid_argument {
 public:
  PandasVariantsFilterError(std::string kind, std::string detail)
      :std::invalid_argument(detail),type(std::move(kind)),message(std::move(detail)){}
  const std::string type;
  const std::string message;
};

[[nodiscard]] EventTable filter_pandas_variants(const EventTable&, const EventAttribute& admitted, const PandasVariantsFilterOptions& = {});
[[nodiscard]] EventTable filter_pandas_variants_top_k(const EventTable&, const EventAttribute& k, const PandasVariantsFilterOptions& = {});
[[nodiscard]] EventTable filter_pandas_variants_min_coverage(const EventTable&, const EventAttribute& minimum, const PandasVariantsFilterOptions& = {});
[[nodiscard]] EventTable filter_pandas_variants_max_coverage(const EventTable&, const EventAttribute& maximum, const PandasVariantsFilterOptions& = {});
[[nodiscard]] EventTable filter_pandas_variants(const EventTable&, const EventAttribute&, const EventAttribute& parameters);
[[nodiscard]] EventTable filter_pandas_variants_top_k(const EventTable&, const EventAttribute&, const EventAttribute& parameters);
[[nodiscard]] EventTable filter_pandas_variants_min_coverage(const EventTable&, const EventAttribute&, const EventAttribute& parameters);
[[nodiscard]] EventTable filter_pandas_variants_max_coverage(const EventTable&, const EventAttribute&, const EventAttribute& parameters);
[[nodiscard]] EventTable filter_pandas_variants(const EventTable&, const EventAttribute&, const std::vector<PandasVariantsFilterParameter>&);
[[nodiscard]] EventTable filter_pandas_variants_top_k(const EventTable&, const EventAttribute&, const std::vector<PandasVariantsFilterParameter>&);
[[nodiscard]] EventTable filter_pandas_variants_min_coverage(const EventTable&, const EventAttribute&, const std::vector<PandasVariantsFilterParameter>&);
[[nodiscard]] EventTable filter_pandas_variants_max_coverage(const EventTable&, const EventAttribute&, const std::vector<PandasVariantsFilterParameter>&);
[[nodiscard]] EventTable filter_pandas_variants(const EventTable&, const PandasVariantsAdmittedMapping&, const PandasVariantsFilterOptions& = {});
[[nodiscard]] EventTable filter_pandas_variants(const EventTable&, const PandasVariantsAdmittedMapping&, const EventAttribute& parameters);
[[nodiscard]] EventTable filter_pandas_variants(const EventTable&, const PandasVariantsAdmittedMapping&, const std::vector<PandasVariantsFilterParameter>&);
}  // namespace minera
