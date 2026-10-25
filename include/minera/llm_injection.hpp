#pragma once

#include <string>

namespace minera {

// The kind of event data handed to the knowledge-injection functions. The
// automatic variants pick a template from it; unsupported data yields empty
// knowledge.
enum class LlmKnowledgeSource {
  kDataFrame,
  kSqlite,
  kOcel,
  kEventLog,
  kOther,
};

enum class LlmDatabaseKnowledgeVariant {
  kAutomatic,
  kPandasDuckdb,
  kSqlite3Traditional,
};

enum class LlmProcessKnowledgeVariant {
  kAutomatic,
  kTraditional,
  kOcel20,
};

// Column names quoted in the database knowledge, and the table name.
struct LlmInjectionParameters {
  std::string activity_key = "concept:name";
  std::string timestamp_key = "time:timestamp";
  std::string case_id_key = "case:concept:name";
  std::string resource_key = "org:resource";
  std::string table_name = "dataframe";
};

// Database-level knowledge: the engine, table and column names, and the
// engine's timestamp and aggregation idioms.
[[nodiscard]] std::string llm_database_knowledge(
    LlmKnowledgeSource source,
    LlmDatabaseKnowledgeVariant variant = LlmDatabaseKnowledgeVariant::kAutomatic,
    const LlmInjectionParameters& parameters = {});

// Process-mining knowledge: how variants and durations are derived.
[[nodiscard]] std::string llm_process_knowledge(
    LlmKnowledgeSource source,
    LlmProcessKnowledgeVariant variant = LlmProcessKnowledgeVariant::kAutomatic,
    const LlmInjectionParameters& parameters = {});

// Both kinds of knowledge with the automatic variants.
[[nodiscard]] std::string llm_domain_knowledge(
    LlmKnowledgeSource source, const LlmInjectionParameters& parameters = {});

}  // namespace minera
