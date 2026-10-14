#pragma once

#include <string>
#include <string_view>

#include "minera/event_data.hpp"

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

// Decode/encode the bare native table carrier without formatting, projection
// or mutation. Missing cells, present nulls and all declared table metadata
// retain their input meaning. Unlike formatter error reporting, these strict
// entry points validate the table's shape and declared schema.
// Typed integer attributes opt into integer_storage=int64|uint64 so positive
// signed values retain their native type in cells, indices and metadata.
// The established formatter carrier does not accept this additional field.
// Input limits remain 32 MiB/depth 64/one million values. Serialization does
// not apply those input limits to complete native before/after tables.
[[nodiscard]] EventTable parse_event_table_json(std::string_view input);
[[nodiscard]] std::string serialize_event_table_input_json(
    const EventTable& input);

}  // namespace minera
