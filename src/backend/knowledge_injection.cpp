#include "minera/knowledge_injection.hpp"

#include <limits>
#include <sstream>
#include <stdexcept>

namespace minera {
namespace {

void add_size(std::size_t& total, std::size_t value) {
  if (value > std::numeric_limits<std::size_t>::max() - total)
    throw std::overflow_error("knowledge injection input size overflow");
  total += value;
}

void append_section(std::string& prompt, std::string_view title,
                    std::string_view value) {
  if (value.empty()) return;
  if (!prompt.empty()) prompt += "\n\n";
  prompt += title;
  prompt += "\n";
  prompt += value;
}

std::string quote(std::string_view value) {
  std::ostringstream out;
  out << '"';
  static constexpr char hex[] = "0123456789abcdef";
  for (const unsigned char character : value) {
    switch (character) {
      case '"': out << "\\\""; break;
      case '\\': out << "\\\\"; break;
      case '\b': out << "\\b"; break;
      case '\f': out << "\\f"; break;
      case '\n': out << "\\n"; break;
      case '\r': out << "\\r"; break;
      case '\t': out << "\\t"; break;
      default:
        if (character < 0x20) {
          out << "\\u00" << hex[character >> 4] << hex[character & 0x0f];
        } else {
          out << static_cast<char>(character);
        }
    }
  }
  out << '"';
  return out.str();
}

}  // namespace

const char* knowledge_injection_variant_name(KnowledgeInjectionVariant variant) {
  switch (variant) {
    case KnowledgeInjectionVariant::kDescription: return "description";
    case KnowledgeInjectionVariant::kProcessMining: return "process_mining";
    case KnowledgeInjectionVariant::kDatabase: return "database";
  }
  throw std::invalid_argument("unsupported knowledge injection variant");
}

KnowledgeInjectionVariant knowledge_injection_variant_from_string(
    std::string_view value) {
  if (value == "description") return KnowledgeInjectionVariant::kDescription;
  if (value == "process_mining" || value == "process-mining")
    return KnowledgeInjectionVariant::kProcessMining;
  if (value == "database" || value == "db")
    return KnowledgeInjectionVariant::kDatabase;
  throw std::invalid_argument("unknown knowledge injection variant");
}

KnowledgeInjectionResult build_knowledge_prompt(
    const KnowledgeInjectionOptions& options) {
  if (options.max_input_bytes == 0 || options.max_prompt_bytes == 0)
    throw std::invalid_argument("knowledge injection bounds must be positive");
  KnowledgeInjectionResult result;
  result.variant = options.variant;
  add_size(result.input_bytes, options.description.size());
  add_size(result.input_bytes, options.query.size());
  add_size(result.input_bytes, options.database_schema.size());
  for (const auto& assumption : options.assumptions)
    add_size(result.input_bytes, assumption.size());
  for (const auto& [name, value] : options.context) {
    add_size(result.input_bytes, name.size());
    add_size(result.input_bytes, value.size());
  }
  if (result.input_bytes > options.max_input_bytes) {
    result.error = "knowledge injection input exceeds configured bound";
    return result;
  }

  switch (options.variant) {
    case KnowledgeInjectionVariant::kDescription:
      append_section(result.prompt, "Description", options.description);
      break;
    case KnowledgeInjectionVariant::kProcessMining:
      append_section(result.prompt, "Process-mining task", options.description);
      append_section(result.prompt, "Question", options.query);
      break;
    case KnowledgeInjectionVariant::kDatabase:
      if (options.database_schema.empty()) {
        result.error = "database knowledge injection requires a schema";
        return result;
      }
      append_section(result.prompt, "Database schema", options.database_schema);
      append_section(result.prompt, "Question", options.query);
      break;
  }
  if (!options.assumptions.empty()) {
    std::string assumptions;
    for (const auto& assumption : options.assumptions) {
      if (!assumptions.empty()) assumptions += '\n';
      assumptions += "- ";
      assumptions += assumption;
    }
    append_section(result.prompt, "Assumptions", assumptions);
  }
  if (!options.context.empty()) {
    std::string context;
    for (const auto& [name, value] : options.context) {
      if (!context.empty()) context += '\n';
      context += name;
      context += ": ";
      context += value;
    }
    append_section(result.prompt, "Context", context);
  }
  if (result.prompt.size() > options.max_prompt_bytes) {
    result.prompt.clear();
    result.error = "knowledge injection prompt exceeds configured bound";
    return result;
  }
  result.computed = true;
  return result;
}

std::string knowledge_injection_json(const KnowledgeInjectionResult& result) {
  std::ostringstream out;
  out << "{\"family\":\"KNOWLEDGE_INJECTION\",\"variant\":"
      << quote(knowledge_injection_variant_name(result.variant))
      << ",\"computed\":" << (result.computed ? "true" : "false")
      << ",\"prompt\":" << quote(result.prompt)
      << ",\"input_bytes\":" << result.input_bytes
      << ",\"error\":" << quote(result.error) << '}';
  return out.str();
}

}  // namespace minera
