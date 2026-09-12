#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "minera/pnml.hpp"
#include "minera/monte_carlo.hpp"
#include "minera/stochastic_map.hpp"
#include "minera/xes_event_log_importer.hpp"
#include "../tools/pnml_model_options.hpp"

namespace {
std::optional<minera::StochasticMapDistribution> distribution(const std::string& name) {
  using D = minera::StochasticMapDistribution;
  if (name == "automatic") return std::nullopt;
  if (name == "EXPONENTIAL") return D::kExponential;
  if (name == "NORMAL") return D::kNormal;
  if (name == "UNIFORM") return D::kUniform;
  if (name == "LOGNORMAL") return D::kLogNormal;
  if (name == "GAMMA") return D::kGamma;
  if (name == "DETERMINISTIC") return D::kDeterministic;
  return D::kImmediate;
}
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
  if (argc != 3 && argc != 4) {
    std::cerr << "usage: stochastic_fit_probe model.pnml log.xes [distribution]\n"
                 "       stochastic_fit_probe --samples distribution comma-separated-values\n";
    return 2;
  }
  try {
    if (argc == 4 && std::string(argv[1]) == "--samples") {
      std::vector<double> values;
      std::istringstream input(argv[3]);
      std::string value;
      while (std::getline(input, value, ',')) values.push_back(std::stod(value));
      minera::StochasticMap map;
      map.transitions.push_back(minera::fit_stochastic_distribution(values, distribution(argv[2])));
      std::cout << minera::serialize_stochastic_map_json(map) << '\n';
      return 0;
    }
    const auto document = minera::import_pnml_document(
        argv[1], minera::tools::pnml_model_import_options());
    const auto log = minera::import_xes_event_log(argv[2]);
    minera::StochasticMapFitOptions options;
    options.distribution = distribution(argc == 4 ? argv[3] : "EXPONENTIAL");
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
    std::cout << '}';
    if (!options.distribution || options.distribution == minera::StochasticMapDistribution::kExponential ||
        options.distribution == minera::StochasticMapDistribution::kNormal ||
        options.distribution == minera::StochasticMapDistribution::kUniform) {
      minera::MonteCarloOptions mc;
      mc.initial_marking = minera::tools::pnml_marking<std::uint32_t>(document.initial_marking);
      mc.final_marking = minera::tools::pnml_marking<std::uint32_t>(document.final_marking);
      mc.place_names = document.place_ids;
      mc.transition_names = document.transition_ids;
      mc.initial_marking_keys = document.initial_marking_keys;
      mc.final_marking_keys = document.final_marking_keys;
      if (options.distribution == minera::StochasticMapDistribution::kExponential)
        mc.force_distribution = minera::MonteCarloDurationDistribution::kExponential;
      if (options.distribution == minera::StochasticMapDistribution::kNormal)
        mc.force_distribution = minera::MonteCarloDurationDistribution::kNormal;
      if (options.distribution == minera::StochasticMapDistribution::kUniform)
        mc.force_distribution = minera::MonteCarloDurationDistribution::kUniform;
      const auto fitted = minera::fit_monte_carlo_options(log, document.net, mc);
      if (!fitted.stochastic_map) throw std::runtime_error("Monte Carlo did not retain fitted map");
      std::cout << ",\"monte_carlo_map\":" << minera::serialize_stochastic_map_json(*fitted.stochastic_map);
    }
    std::cout << "}\n";
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
