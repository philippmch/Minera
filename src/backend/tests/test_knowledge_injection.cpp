#include "minera/knowledge_injection.hpp"

#include <cassert>
#include <stdexcept>

using namespace minera;

int main() {
  KnowledgeInjectionOptions generic;
  generic.description = "Explain the model.";
  generic.assumptions = {"Use UTC", "Treat tau as silent"};
  generic.context = {{"activity", "Approve"}, {"case", "42"}};
  const auto description = build_knowledge_prompt(generic);
  assert(description.computed && description.error.empty());
  assert(description.prompt ==
         "Description\nExplain the model.\n\nAssumptions\n- Use UTC\n"
         "- Treat tau as silent\n\nContext\nactivity: Approve\ncase: 42");

  KnowledgeInjectionOptions process;
  process.variant = KnowledgeInjectionVariant::kProcessMining;
  process.description = "A directly-follows graph with a loop.";
  process.query = "Where is the rework?";
  const auto process_result = build_knowledge_prompt(process);
  assert(process_result.computed);
  assert(process_result.prompt.find("Process-mining task") == 0);
  assert(process_result.prompt.find("Where is the rework?") != std::string::npos);

  KnowledgeInjectionOptions database;
  database.variant = knowledge_injection_variant_from_string("db");
  database.database_schema = "events(case_id, activity, timestamp)";
  database.query = "Count completed cases.";
  const auto database_result = build_knowledge_prompt(database);
  assert(database_result.computed);
  assert(database_result.prompt.find("Database schema") == 0);

  database.database_schema.clear();
  const auto missing = build_knowledge_prompt(database);
  assert(!missing.computed && !missing.error.empty() && missing.prompt.empty());

  generic.max_input_bytes = 3;
  const auto bounded = build_knowledge_prompt(generic);
  assert(!bounded.computed && bounded.prompt.empty());

  generic.max_input_bytes = 1000;
  generic.max_prompt_bytes = 5;
  const auto prompt_bounded = build_knowledge_prompt(generic);
  assert(!prompt_bounded.computed && prompt_bounded.prompt.empty());

  bool threw = false;
  try { (void)knowledge_injection_variant_from_string("unknown"); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);

  KnowledgeInjectionResult escaped;
  escaped.variant = KnowledgeInjectionVariant::kDescription;
  escaped.computed = true;
  escaped.prompt = "a\n\"b";
  const auto json = knowledge_injection_json(escaped);
  assert(json.find("KNOWLEDGE_INJECTION") != std::string::npos);
  assert(json.find("a\\n\\\"b") != std::string::npos);
}
