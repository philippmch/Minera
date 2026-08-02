#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "minera/ocel.hpp"

int main(int argc, char** argv) {
  try {
    if (argc < 2) {
      std::cerr << "usage: minera_ocel INPUT.json [--format classic|standard] [--discover etot|otg]\n";
      return 2;
    }
    std::ifstream input(argv[1], std::ios::binary);
    if (!input) throw std::runtime_error("cannot open input");
    std::ostringstream text; text << input.rdbuf();
    minera::Ocel log = minera::parse_ocel_json(text.str());
    std::string format = "classic", discover;
    for (int i = 2; i < argc; ++i) {
      const std::string option = argv[i];
      if ((option == "--format" || option == "--discover") && i + 1 < argc) {
        (option == "--format" ? format : discover) = argv[++i];
      } else throw std::invalid_argument("unknown option: " + option);
    }
    if (format != "classic" && format != "standard") {
      throw std::invalid_argument("format must be classic or standard");
    }
    if (!discover.empty() && discover != "etot" && discover != "otg") {
      throw std::invalid_argument("discover must be etot or otg");
    }
    if (discover == "etot") {
      std::cout << minera::etot_json(minera::discover_etot_classic(log)) << '\n';
      return 0;
    }
    if (discover == "otg") {
      std::cout << minera::otg_json(minera::discover_otg_classic(log)) << '\n';
      return 0;
    }
    const auto selected = format == "standard" ? minera::OcelJsonFormat::kStandard : minera::OcelJsonFormat::kClassic;
    std::cout << minera::serialize_ocel_json(log, selected) << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "minera_ocel: " << error.what() << '\n';
    return 1;
  }
}
