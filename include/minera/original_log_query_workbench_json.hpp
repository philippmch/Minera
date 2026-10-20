#pragma once
#include "minera/discovery_workbench_json.hpp"
#include "minera/log_query.hpp"

namespace minera {
// Structural constructor readers: native consumers own semantic validation.
[[nodiscard]] LogQueryTable parse_original_log_query_table_json(std::string_view);
[[nodiscard]] LogQueryResult parse_original_log_query_result_json(std::string_view);
[[nodiscard]] std::vector<std::size_t> parse_original_log_query_indices_json(std::string_view);
[[nodiscard]] std::string serialize_original_log_query_table_json(const LogQueryTable&);
[[nodiscard]] std::string serialize_original_log_query_indices_json(const std::vector<std::size_t>&);
[[nodiscard]] DiscoveryWorkbenchResponse execute_original_log_query_workbench(
    const std::vector<DiscoveryWorkbenchPart>&,
    const std::vector<DiscoveryWorkbenchPart>& query = {},
    std::string_view input_order = "provided_order",
    std::string_view transport_error = {},
    const std::vector<std::string>* cli_argv = nullptr);
} // namespace minera
