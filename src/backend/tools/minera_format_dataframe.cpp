#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#include "minera/dataframe_format_json.hpp"

int main(int argc, char** argv) try {
  if (argc == 2 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")) {
    std::cout << "Usage: " << argv[0] << " REQUEST.json|- [--output RESULT.json]\n";
    return 0;
  }
  if (argc != 2 && argc != 4) {
    std::cerr << "Usage: " << argv[0] << " REQUEST.json|- [--output RESULT.json]\n";
    return 2;
  }
  if (argc == 4 && std::string(argv[2]) != "--output")
    throw std::invalid_argument("expected --output RESULT.json");
  std::ifstream file;
  std::istream* input = &std::cin;
  if (std::string(argv[1]) != "-") {
    file.open(argv[1], std::ios::binary);
    if (!file) throw std::invalid_argument("cannot open dataframe request");
    input = &file;
  }
  std::string request;
  char chunk[8192];
  while (input->read(chunk, sizeof(chunk)) || input->gcount()) {
    request.append(chunk, static_cast<std::size_t>(input->gcount()));
    if (request.size() > 32u * 1024u * 1024u)
      throw std::invalid_argument("dataframe request exceeds 32 MiB");
  }
  if (!input->eof()) throw std::runtime_error("cannot read dataframe request");
  const auto result = minera::format_dataframe_json(request);
  std::ofstream output;
  std::ostream* destination = &std::cout;
  if (argc == 4) {
    output.open(argv[3], std::ios::binary | std::ios::trunc);
    if (!output) throw std::runtime_error("cannot open dataframe output");
    destination = &output;
  }
  *destination << result.body << '\n';
  destination->flush();
  if (!*destination) throw std::runtime_error("cannot write dataframe output");
  return result.success ? 0 : 1;
} catch (const std::exception& error) {
  std::cerr << "error: " << error.what() << '\n';
  return 1;
}
