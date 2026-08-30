#include <charconv>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "minera/knowledge_injection.hpp"

namespace {

std::size_t positive_size(std::string_view text, std::string_view option) {
  std::size_t value = 0;
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
  if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || value == 0)
    throw std::invalid_argument(std::string(option) + " expects a positive integer");
  return value;
}

int usage(const char* program) {
  std::cerr << "Usage: " << program
            << " --variant description|process_mining|database"
               " [--description TEXT] [--query TEXT] [--database-schema TEXT]"
               " [--max-input-bytes N] [--max-prompt-bytes N]\n";
  return 2;
}

}  // namespace

int main(int argc, char** argv) {
  try {
    minera::KnowledgeInjectionOptions options;
    for (int index = 1; index < argc; ++index) {
      const std::string option = argv[index];
      if (index + 1 >= argc) return usage(argv[0]);
      const std::string value = argv[++index];
      if (option == "--variant")
        options.variant = minera::knowledge_injection_variant_from_string(value);
      else if (option == "--description") options.description = value;
      else if (option == "--query") options.query = value;
      else if (option == "--database-schema") options.database_schema = value;
      else if (option == "--max-input-bytes")
        options.max_input_bytes = positive_size(value, option);
      else if (option == "--max-prompt-bytes")
        options.max_prompt_bytes = positive_size(value, option);
      else return usage(argv[0]);
    }
    std::cout << minera::knowledge_injection_json(
                     minera::build_knowledge_prompt(options))
              << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "error: " << error.what() << '\n';
    return 1;
  }
}
