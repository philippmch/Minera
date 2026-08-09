#include <iostream>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "minera/dfg_conversion.hpp"
#include "minera/dfg_io.hpp"

int main(int argc, char** argv) {
  try {
    if (argc >= 2 && std::string_view(argv[1]) == "--to-petri") {
      if (argc < 4 || argc > 5) {
        std::cerr << "Usage: " << argv[0]
                  << " --to-petri <activity_defines_place|invisibles_no_duplicates> "
                     "INPUT.dfg [OUTPUT.json]\n";
        return 2;
      }
      const auto variant =
          minera::parse_dfg_petri_conversion_variant(argv[2]);
      const auto dfg = minera::import_dfg(argv[3]);
      const auto net = minera::convert_dfg_to_petri_net(
          dfg, minera::DfgPetriConversionOptions{variant});
      const auto json = minera::serialize_dfg_petri_net_json(net, variant);
      if (argc == 5) {
        std::ofstream output(argv[4], std::ios::binary | std::ios::trunc);
        if (!output) throw std::runtime_error("cannot open output file");
        output << json << '\n';
      } else {
        std::cout << json << '\n';
      }
      return 0;
    }
    if (argc < 2 || argc > 3) {
      std::cerr << "Usage: " << argv[0]
                << " INPUT.dfg [OUTPUT.dfg]\n"
                   "       "
                << argv[0]
                << " --to-petri <variant> INPUT.dfg [OUTPUT.json]\n";
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
