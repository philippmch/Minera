#pragma once
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "minera/event_data.hpp"
namespace minera {
struct PandasPathsFilterOptions {
  EventAttribute case_id_key{"", "case:concept:name"};
  EventAttribute attribute_key{"", "concept:name"};
  std::optional<EventAttribute> target_attribute_key;
  EventAttribute timestamp_key{"", "time:timestamp"};
  EventAttribute positive{"", "true", AttributeKind::kBoolean};
  EventAttribute min_performance{"", "0", AttributeKind::kInt};
  EventAttribute max_performance{"", "9223372036854775807", AttributeKind::kInt};
};
// Public enum keys and literal keys are distinct before parameter resolution.
struct PandasPathsFilterParameter { EventAttribute value; bool enum_key = false; };
class PandasPathsFilterError : public std::invalid_argument {
 public:
  PandasPathsFilterError(std::string kind,std::string detail)
      :std::invalid_argument(detail),type(std::move(kind)),message(std::move(detail)){}
  const std::string type;
  const std::string message;
};
[[nodiscard]] EventTable filter_pandas_paths(const EventTable&,const EventAttribute& paths,const PandasPathsFilterOptions& = {});
[[nodiscard]] EventTable filter_pandas_path_performance(const EventTable&,const EventAttribute& path,const PandasPathsFilterOptions& = {});
[[nodiscard]] EventTable filter_pandas_paths(const EventTable&,const EventAttribute& paths,const EventAttribute& parameters);
[[nodiscard]] EventTable filter_pandas_path_performance(const EventTable&,const EventAttribute& path,const EventAttribute& parameters);
[[nodiscard]] EventTable filter_pandas_paths(const EventTable&,const EventAttribute& paths,const std::vector<PandasPathsFilterParameter>&);
[[nodiscard]] EventTable filter_pandas_path_performance(const EventTable&,const EventAttribute& path,const std::vector<PandasPathsFilterParameter>&);
}  // namespace minera
