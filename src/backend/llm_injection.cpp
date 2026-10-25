#include "minera/llm_injection.hpp"

namespace minera {
namespace {

std::string table_description(const std::string& engine,
                              const LlmInjectionParameters& parameters) {
  return "\nThe underlying database engine is " + engine +
         ".\nThe table is called " + parameters.table_name +
         ".\nEach row of the table is corresponding to an event, along with "
         "its attributes. There is no separate table containing the process "
         "variant.\nPlease consider the following information: the case "
         "identifier is called '" +
         parameters.case_id_key +
         "', the activity is stored inside the attribute '" +
         parameters.activity_key +
         "', the timestamp is stored inside the attribute '" +
         parameters.timestamp_key +
         "', the resource is stored inside the attribute '" +
         parameters.resource_key + "'.\n";
}

const char* const kSortedEvents =
    "The events are already sorted by timestamp, moreover you cannot use "
    "ORDER BY inside a concatenation operator.\n    ";

}  // namespace

std::string llm_database_knowledge(LlmKnowledgeSource source,
                                   LlmDatabaseKnowledgeVariant variant,
                                   const LlmInjectionParameters& parameters) {
  if (variant == LlmDatabaseKnowledgeVariant::kAutomatic) {
    if (source == LlmKnowledgeSource::kDataFrame)
      variant = LlmDatabaseKnowledgeVariant::kPandasDuckdb;
    else if (source == LlmKnowledgeSource::kSqlite)
      variant = LlmDatabaseKnowledgeVariant::kSqlite3Traditional;
    else
      return "\n\n";
  }
  if (variant == LlmDatabaseKnowledgeVariant::kPandasDuckdb)
    return table_description("DuckDB", parameters) +
           "You should use the EPOCH function of DuckDB to get the timestamp "
           "from the date.\n" +
           kSortedEvents;
  return table_description("SQLite", parameters) +
         "If you need to compute timestamp differences, please use the "
         "\"julianday\" function which gets the fractional number of days "
         "since noon in Greenwhich on November 24, 4714 B.C.\nIn SQLite, "
         "there is no STDDEV function but you need to compute it as: "
         "SQRT(AVG(col*col) - AVG(col)*AVG(col)).\n" +
         kSortedEvents;
}

std::string llm_process_knowledge(LlmKnowledgeSource source,
                                  LlmProcessKnowledgeVariant variant,
                                  const LlmInjectionParameters&) {
  if (variant == LlmProcessKnowledgeVariant::kAutomatic) {
    if (source == LlmKnowledgeSource::kDataFrame ||
        source == LlmKnowledgeSource::kSqlite)
      variant = LlmProcessKnowledgeVariant::kTraditional;
    else if (source == LlmKnowledgeSource::kOcel)
      variant = LlmProcessKnowledgeVariant::kOcel20;
    else
      return "\n\n";
  }
  if (variant == LlmProcessKnowledgeVariant::kTraditional)
    return "\nIf you need it, the process variant for a case can be obtained "
           "as concatenation of the activities of a case.\nIf you need it, the "
           "duration of a case can be obtained as difference between the "
           "timestamp of the first and the last event.\n    ";
  return "\nIf you need to compute the duration of a lifecycle of an object, "
         "compute the difference between the timestamp of the last and the "
         "first event of the lifecycle.\nIf you need to compute the variant "
         "for an object, aggregate the names of the activities.\n    ";
}

std::string llm_domain_knowledge(LlmKnowledgeSource source,
                                 const LlmInjectionParameters& parameters) {
  return "\n\n" +
         llm_database_knowledge(source, LlmDatabaseKnowledgeVariant::kAutomatic,
                                parameters) +
         llm_process_knowledge(source, LlmProcessKnowledgeVariant::kAutomatic,
                               parameters);
}

}  // namespace minera
