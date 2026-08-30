#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace minera {

enum class KnowledgeInjectionVariant {
  kDescription,
  kProcessMining,
  kDatabase,
};

struct KnowledgeInjectionOptions {
  KnowledgeInjectionVariant variant = KnowledgeInjectionVariant::kDescription;
  std::string description;
  std::string query;
  std::string database_schema;
  std::vector<std::string> assumptions;
  std::map<std::string, std::string> context;
  std::size_t max_input_bytes = 1024 * 1024;
  std::size_t max_prompt_bytes = 2 * 1024 * 1024;
};

struct KnowledgeInjectionResult {
  KnowledgeInjectionVariant variant = KnowledgeInjectionVariant::kDescription;
  bool computed = false;
  std::string prompt;
  std::size_t input_bytes = 0;
  std::string error;
};

[[nodiscard]] KnowledgeInjectionResult build_knowledge_prompt(
    const KnowledgeInjectionOptions& options);
[[nodiscard]] std::string knowledge_injection_json(
    const KnowledgeInjectionResult& result);
[[nodiscard]] KnowledgeInjectionVariant knowledge_injection_variant_from_string(
    std::string_view value);
[[nodiscard]] const char* knowledge_injection_variant_name(
    KnowledgeInjectionVariant variant);

}  // namespace minera
