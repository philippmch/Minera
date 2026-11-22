#pragma once

#include <optional>
#include <string>
#include "minera/event_data.hpp"

namespace minera {
struct DataFrameColumnOptions {
  std::optional<std::string> activity_key;
  std::optional<std::string> case_id_key;
  std::optional<std::string> timestamp_key;
  std::optional<std::string> start_timestamp_key;
  // pm4py's check on a Polars frame: a Categorical or Enum column (a pandas
  // "category" once converted) counts as text and a Date column as a date
  // (round FV, observed black-box).
  bool polars = false;
};

// Validate the public dataframe schema and selected columns without mutation.
// Declared dtypes are authoritative. Native tables without dtypes use a basic
// scalar-kind inference; exhaustive pandas reader inference is not implied.
// pandas' "Available columns: [...]" list: the names sorted, each in
// Python's repr.
[[nodiscard]] std::string dataframe_available_columns(std::vector<std::string> columns);
void check_dataframe_columns(const EventTable& table,
                             const DataFrameColumnOptions& options = {});
}  // namespace minera
