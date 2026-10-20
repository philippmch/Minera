#pragma once
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "minera/knowledge_injection.hpp"
namespace minera {
// Transport provenance, independent of native kernel options. name is present
// for the File role (*_file); transport_filename independently preserves every
// multipart filename parameter, including an empty filename and Blob byte fields.
struct KnowledgeExactInput {std::string field;std::optional<std::string> name;std::string content;std::optional<std::string> transport_filename = std::nullopt;};
[[nodiscard]] bool knowledge_exact_control(std::string_view);
// Typed JSON contains no independent byte/node cap; the HTTP server retains its
// existing 32 MiB payload limit. A 64-level syntax guard cannot exclude a valid
// typed input (the deepest valid NativeString structure has only a few levels).
// NativeStrings accept UTF8/WTF8 and hex-v1 byte carriers. No scalar is trimmed.
[[nodiscard]] std::vector<std::string> parse_knowledge_exact_assumptions_json(std::string_view);
[[nodiscard]] std::map<std::string,std::string> parse_knowledge_exact_context_json(std::string_view);
// Exactly all 8 fields are required. Bounds are exact native size_t, including 0;
// build_knowledge_prompt retains responsibility for rejecting zero bounds.
[[nodiscard]] KnowledgeInjectionOptions parse_knowledge_exact_options_json(std::string_view);
[[nodiscard]] KnowledgeInjectionOptions parse_knowledge_exact_inputs(const std::vector<KnowledgeExactInput>&);
[[nodiscard]] std::string serialize_knowledge_exact_options_json(const KnowledgeInjectionOptions&);
[[nodiscard]] std::string serialize_knowledge_exact_result_json(const KnowledgeInjectionResult&);
[[nodiscard]] std::string serialize_knowledge_exact_envelope_json(
 const std::optional<KnowledgeInjectionOptions>&,const std::optional<KnowledgeInjectionResult>&,
 const std::vector<KnowledgeExactInput>&,std::string_view error = {});
}  // namespace minera
