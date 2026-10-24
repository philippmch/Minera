// Batch driver for the public repetition workbenches, used by offline parity
// audits. Each request on stdin is a route line ("log" or "table"), a part
// count line, and per part: name, filename, file flag (0/1) and byte length
// lines followed by exactly that many bytes. Each response on stdout is its
// byte length on one line followed by the workbench JSON envelope.
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "minera/reference_log_filter_workbench.hpp"
#include "minera/table_attribute_repetition_workbench.hpp"

namespace {
std::string line(std::istream& input) {
  std::string value;
  if (!std::getline(input, value)) throw std::runtime_error("truncated repetition batch request");
  return value;
}
std::size_t count(std::istream& input) { return static_cast<std::size_t>(std::stoull(line(input))); }
}  // namespace

int main(int argc, char** argv) try {
  if (argc != 1) {
    std::cerr << "Usage: " << argv[0] << " < REQUESTS\n";
    return 2;
  }
  std::ios::sync_with_stdio(false);
  std::string route;
  while (std::getline(std::cin, route)) {
    if (route.empty()) continue;
    if (route != "log" && route != "table") throw std::invalid_argument("unknown repetition batch route: " + route);
    std::vector<minera::DiscoveryWorkbenchPart> parts(count(std::cin));
    for (auto& part : parts) {
      part.name = line(std::cin);
      part.filename = line(std::cin);
      part.file = line(std::cin) == "1";
      part.bytes.resize(count(std::cin));
      if (!std::cin.read(part.bytes.data(), static_cast<std::streamsize>(part.bytes.size())))
        throw std::runtime_error("truncated repetition batch part");
    }
    const auto response = route == "log" ? minera::execute_reference_log_filter(parts)
                                         : minera::execute_table_attribute_repetition(parts);
    std::cout << response.json.size() << '\n' << response.json;
    std::cout.flush();
  }
  return 0;
} catch (const std::exception& error) {
  std::cerr << "error: " << error.what() << '\n';
  return 1;
}
