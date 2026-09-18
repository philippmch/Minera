#pragma once

#include <optional>
#include <string>

#include "minera/event_data.hpp"

namespace minera {

struct DataFrameFormatOptions {
  std::string case_id = "case:concept:name";
  std::string activity_key = "concept:name";
  std::string timestamp_key = "time:timestamp";
  // An empty optional disables start timestamp projection.  The default
  // follows PM4Py's public selector; callers can pass std::nullopt when a
  // source frame has no start timestamp column.
  std::optional<std::string> start_timestamp_key =
      std::string{"start_timestamp"};
  std::optional<std::string> timestamp_format;
};

// Format a native table using the public dataframe formatting contract.
// The mutable input exposes changes made before the returned table is formed.
[[nodiscard]] EventTable format_dataframe(
    EventTable& input, const DataFrameFormatOptions& options = {});

}  // namespace minera
