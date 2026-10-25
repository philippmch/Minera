// Replays the PM4Py 2.7.22 LLM knowledge-injection bank
// (docs/llm-injection-bank.json.gz) against the native templates.
#include "minera/llm_injection.hpp"

#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
using namespace minera;

struct BankCall {
  const char* function;
  const char* source;
  const char* parameters;
  const char* variant;
  const char* expected;
};

const BankCall kCalls[] = {
#include "llm_injection_bank_cases.inc"
};

LlmKnowledgeSource source_of(const std::string& name) {
  if (name == "dataframe") return LlmKnowledgeSource::kDataFrame;
  if (name == "sqlite") return LlmKnowledgeSource::kSqlite;
  if (name == "ocel") return LlmKnowledgeSource::kOcel;
  if (name == "event_log") return LlmKnowledgeSource::kEventLog;
  return LlmKnowledgeSource::kOther;
}

LlmInjectionParameters parameters_of(const std::string& name) {
  LlmInjectionParameters parameters;
  if (name == "activity") parameters.activity_key = "Activity";
  if (name == "timestamp") parameters.timestamp_key = "When";
  if (name == "case") parameters.case_id_key = "Case ID";
  if (name == "resource") parameters.resource_key = "who";
  if (name == "table") parameters.table_name = "events_2024";
  if (name == "all") {
    parameters.activity_key = "it's";
    parameters.timestamp_key = "ts \"utc\"";
    parameters.case_id_key = "c\303\244se";
    parameters.resource_key = "";
    parameters.table_name = "my table";
  }
  return parameters;
}

std::size_t compared = 0;

void expect(const BankCall& call, const std::string& actual) {
  ++compared;
  if (actual != call.expected)
    throw std::runtime_error(std::string{call.function} + "/" + call.source +
                             "/" + call.parameters + "/" + call.variant +
                             ": native text differs");
}

template <class Predicate, class Native>
void replay(Predicate select, Native native) {
  for (const auto& call : kCalls)
    if (select(std::string{call.function}, std::string{call.variant}))
      expect(call, native(source_of(call.source), parameters_of(call.parameters)));
}

void injection_case() {
  replay([](const std::string& function, const std::string&) {
    return function == "injection";
  }, [](LlmKnowledgeSource source, const LlmInjectionParameters& parameters) {
    return llm_domain_knowledge(source, parameters);
  });
  replay([](const std::string& function, const std::string& variant) {
    return function == "db_knowledge" && variant.empty();
  }, [](LlmKnowledgeSource source, const LlmInjectionParameters& parameters) {
    return llm_database_knowledge(source, LlmDatabaseKnowledgeVariant::kAutomatic,
                                  parameters);
  });
  replay([](const std::string& function, const std::string& variant) {
    return function == "pm_knowledge" && variant.empty();
  }, [](LlmKnowledgeSource source, const LlmInjectionParameters& parameters) {
    return llm_process_knowledge(source, LlmProcessKnowledgeVariant::kAutomatic,
                                 parameters);
  });
}

void duckdb_case() {
  replay([](const std::string& function, const std::string& variant) {
    return function == "db_knowledge" && variant == "pandas_duckdb";
  }, [](LlmKnowledgeSource source, const LlmInjectionParameters& parameters) {
    return llm_database_knowledge(source, LlmDatabaseKnowledgeVariant::kPandasDuckdb,
                                  parameters);
  });
}

void sqlite_case() {
  replay([](const std::string& function, const std::string& variant) {
    return function == "db_knowledge" && variant == "sqlite3_traditional";
  }, [](LlmKnowledgeSource source, const LlmInjectionParameters& parameters) {
    return llm_database_knowledge(
        source, LlmDatabaseKnowledgeVariant::kSqlite3Traditional, parameters);
  });
}

void traditional_case() {
  replay([](const std::string& function, const std::string& variant) {
    return function == "pm_knowledge" && variant == "traditional";
  }, [](LlmKnowledgeSource source, const LlmInjectionParameters& parameters) {
    return llm_process_knowledge(source, LlmProcessKnowledgeVariant::kTraditional,
                                 parameters);
  });
}

void ocel20_case() {
  replay([](const std::string& function, const std::string& variant) {
    return function == "pm_knowledge" && variant == "ocel20";
  }, [](LlmKnowledgeSource source, const LlmInjectionParameters& parameters) {
    return llm_process_knowledge(source, LlmProcessKnowledgeVariant::kOcel20,
                                 parameters);
  });
}
}  // namespace

int main() {
  try {
    injection_case();
    duckdb_case();
    sqlite_case();
    traditional_case();
    ocel20_case();
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
  std::cout << compared << " calls compared\n";
  return compared == 245 ? 0 : 1;
}
