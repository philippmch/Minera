#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

#include "minera/pnml.hpp"
#include "minera/stochastic_map.hpp"
#include "minera/xes_event_log_importer.hpp"
#include "../tools/pnml_model_options.hpp"

namespace {
std::string quote(const std::string& value) {
  constexpr char hex[] = "0123456789abcdef";
  std::string result = "\"";
  for (const unsigned char c : value) {
    if (c == '"' || c == '\\') { result += '\\'; result += static_cast<char>(c); }
    else if (c < 0x20) {
      result += "\\u00";
      result += hex[c >> 4]; result += hex[c & 15];
    } else result += static_cast<char>(c);
  }
  return result + '"';
}
template <class T>
void array(const std::vector<T>& values) {
  std::cout << '[';
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i) std::cout << ',';
    std::cout << values[i];
  }
  std::cout << ']';
}
}

int main(int argc, char** argv) {
  if (argc != 3) { std::cerr << "usage: stochastic_fit_probe model.pnml log.xes\n"; return 2; }
  try {
    const auto document = minera::import_pnml_document(
        argv[1], minera::tools::pnml_model_import_options());
    const auto log = minera::import_xes_event_log(argv[2]);
    minera::StochasticMapFitOptions options;
    options.replay = minera::tools::pnml_replay_options(document);
    const auto result = minera::fit_stochastic_map_with_report(log, document.net, options);
    std::cout << std::setprecision(17) << "{\"map\":"
              << minera::serialize_stochastic_map_json(result.map)
              << ",\"transition_ids\":[";
    for (std::size_t i = 0; i < document.transition_ids.size(); ++i) {
      if (i) std::cout << ',';
      std::cout << quote(document.transition_ids[i]);
    }
    std::cout << "],\"place_ids\":[";
    for (std::size_t i = 0; i < document.place_ids.size(); ++i) {
      if (i) std::cout << ',';
      std::cout << quote(document.place_ids[i]);
    }
    std::cout << "],\"statistics\":{";
    bool first = true;
    for (const auto& statistics : result.statistics) {
      if (!first) std::cout << ',';
      first = false;
      std::cout << quote(document.transition_ids.at(statistics.transition))
                << ":{\"count\":" << statistics.activations
                << ",\"no_of_times_activated\":" << statistics.activations
                << ",\"no_of_times_enabled\":" << statistics.enablings
                << ",\"performance\":";
      array(statistics.performance);
      std::cout << ",\"log_idx\":";
      array(statistics.log_indices);
      std::cout << '}';
    }
    std::cout << "}}\n";
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
