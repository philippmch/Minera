#pragma once

#include <string>
#include <string_view>

namespace minera {

struct DataFrameFormatJsonResponse {
  bool success = false;
  std::string body;
};

// Decode {"table": <native table>, "options": <public formatter options>}.
// Successful and rejected formatting calls include input_after. Malformed
// transport data throws before calling the formatter. Limits: 32 MiB, depth 64.
[[nodiscard]] DataFrameFormatJsonResponse format_dataframe_json(
    std::string_view request);

}  // namespace minera
