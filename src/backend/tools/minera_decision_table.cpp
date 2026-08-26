#include <exception>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>

#include "minera/csv_event_log_importer.hpp"
#include "minera/decision_table.hpp"
#include "minera/pnml.hpp"

namespace {

int usage(const char* program) {
  std::cerr << "Usage: " << program
            << " MODEL.pnml INPUT.csv [OUTPUT.json]\n";
  return 2;
}

}  // namespace

int main(int argc, char** argv) {
  try {
    if (argc < 3 || argc > 4) return usage(argv[0]);
    const auto net = minera::import_pnml(argv[1]);
    const auto log = minera::import_csv_event_log(argv[2]);
    const auto result = minera::mine_decision_tables(log, net);
    const auto payload = minera::serialize_decision_tables_json(result) + '\n';
    if (argc == 3) {
      std::cout << payload;
    } else {
      std::ofstream output(argv[3], std::ios::binary | std::ios::trunc);
      if (!output) throw std::runtime_error("cannot open decision table output");
      output << payload;
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "error: " << error.what() << '\n';
    return 1;
  }
}
