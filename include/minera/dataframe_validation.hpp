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
};

// Validate the public dataframe schema and selected columns without mutation.
// Declared dtypes are authoritative. Native tables without dtypes use a basic
// scalar-kind inference; exhaustive pandas reader inference is not implied.
void check_dataframe_columns(const EventTable& table,
                             const DataFrameColumnOptions& options = {});
}  // namespace minera
