#include <iostream>
#include <stdexcept>
#include <string>

#include "minera/dfg_io.hpp"

int main(int argc, char** argv) {
  try {
    if (argc < 2 || argc > 3) {
      std::cerr << "Usage: " << argv[0] << " INPUT.dfg [OUTPUT.dfg]\n";
      return 2;
    }
    const auto dfg = minera::import_dfg(argv[1]);
    if (argc == 3) {
      minera::export_dfg(argv[2], dfg);
    } else {
      std::cout << minera::to_dfg(dfg);
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "error: " << error.what() << '\n';
    return 1;
  }
}
