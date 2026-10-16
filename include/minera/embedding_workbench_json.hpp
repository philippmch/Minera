#pragma once
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "minera/embeddings.hpp"
#include "minera/event_data.hpp"

namespace minera {
// All8 value fields retain native defaults; host callback stays empty. A host
// function is not JSON. The route reports actual binding presence separately.
[[nodiscard]] EmbeddingOptions parse_embedding_workbench_options_json(
    std::string_view, std::set<std::string>* supplied = nullptr);
[[nodiscard]] std::string serialize_embedding_workbench_options_json(const EmbeddingOptions&);
// Exact stored result carriers never apply the stricter legacy export/native
// algorithm validators. Unknown enums, inactive fields and duplicate IDs stay.
[[nodiscard]] EmbeddingResult parse_embedding_workbench_result_json(
    std::string_view, std::set<std::string>* supplied = nullptr);
[[nodiscard]] std::string serialize_embedding_workbench_result_json(const EmbeddingResult&);
[[nodiscard]] std::vector<EmbeddingSimilarity> parse_embedding_workbench_matches_json(std::string_view);
[[nodiscard]] std::string serialize_embedding_workbench_matches_json(const std::vector<EmbeddingSimilarity>&);
[[nodiscard]] std::vector<double> parse_embedding_workbench_vector_json(std::string_view);
[[nodiscard]] std::string serialize_embedding_workbench_vector_json(const std::vector<double>&);
// Byte-safe native strings and complete raw constructor fallback carriers.
[[nodiscard]] std::string serialize_embedding_workbench_string_json(std::string_view);
[[nodiscard]] EventLog parse_embedding_workbench_raw_log_json(std::string_view);
[[nodiscard]] std::string serialize_embedding_workbench_raw_log_json(const EventLog&);
[[nodiscard]] EventTable parse_embedding_workbench_table_json(std::string_view);
[[nodiscard]] std::string serialize_embedding_workbench_table_json(const EventTable&);
[[nodiscard]] std::string serialize_embedding_workbench_stream_json(const EventStream&);
// Multipart size field: decimal digits only, no trimming/sign/exponent/cap.
[[nodiscard]] std::size_t parse_embedding_workbench_size(std::string_view);
}  // namespace minera
