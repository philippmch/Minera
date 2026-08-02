#include <fstream>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "minera/ocel.hpp"

int main(int argc, char** argv) {
  try {
    if (argc < 2) {
      std::cerr << "usage: minera_ocel INPUT.json [--format classic|standard] [--describe text|tuples] [--no-description-timestamps] [--discover etot|ocdfg|ocpn|otg] [--conformance etot|ocdfg|otg --normative MODEL.json]\n";
      return 2;
    }
    std::ifstream input(argv[1], std::ios::binary);
    if (!input) throw std::runtime_error("cannot open input");
    std::ostringstream text; text << input.rdbuf();
    minera::Ocel log = minera::parse_ocel_json(text.str());
    std::string format = "classic", describe, discover, conformance, normative_path;
    bool compute_edges_performance = true;
    bool include_description_timestamps = true;
    double alpha = 1.0, beta = 1.0, gamma = 1.0, delta = 1.0;
    double etot_theta = 0.1, otg_theta = 0.2;
    double ocdfg_theta_activity = 0.0, ocdfg_theta_flow = 0.0;
    minera::OcpnDiscoveryOptions ocpn_options;
    const auto number = [](const std::string& value, const char* option) {
      char* end = nullptr;
      const double result = std::strtod(value.c_str(), &end);
      if (end != value.c_str() + value.size() || !std::isfinite(result) ||
          result < 0.0)
        throw std::invalid_argument(std::string(option) + " expects a finite non-negative number");
      return result;
    };
    for (int i = 2; i < argc; ++i) {
      const std::string option = argv[i];
      if ((option == "--format" || option == "--discover") && i + 1 < argc) {
        (option == "--format" ? format : discover) = argv[++i];
      } else if (option == "--describe" && i + 1 < argc) {
        describe = argv[++i];
      } else if ((option == "--conformance" || option == "--normative") &&
                 i + 1 < argc) {
        (option == "--conformance" ? conformance : normative_path) = argv[++i];
      } else if (option == "--alpha" && i + 1 < argc) {
        alpha = number(argv[++i], "--alpha");
      } else if (option == "--beta" && i + 1 < argc) {
        beta = number(argv[++i], "--beta");
      } else if (option == "--gamma" && i + 1 < argc) {
        gamma = number(argv[++i], "--gamma");
      } else if (option == "--delta" && i + 1 < argc) {
        delta = number(argv[++i], "--delta");
      } else if (option == "--etot-theta" && i + 1 < argc) {
        etot_theta = number(argv[++i], "--etot-theta");
      } else if (option == "--otg-theta" && i + 1 < argc) {
        otg_theta = number(argv[++i], "--otg-theta");
      } else if (option == "--ocdfg-theta-activity" && i + 1 < argc) {
        ocdfg_theta_activity = number(argv[++i], "--ocdfg-theta-activity");
      } else if (option == "--ocdfg-theta-flow" && i + 1 < argc) {
        ocdfg_theta_flow = number(argv[++i], "--ocdfg-theta-flow");
      } else if (option == "--ocpn-variant" && i + 1 < argc) {
        const std::string variant = argv[++i];
        if (variant == "im") {
          ocpn_options.variant = minera::OcpnInductiveVariant::kLog;
        } else if (variant == "imd") {
          ocpn_options.variant = minera::OcpnInductiveVariant::kDfg;
        } else {
          throw std::invalid_argument("--ocpn-variant expects im or imd");
        }
      } else if (option == "--ocpn-double-arc-threshold" && i + 1 < argc) {
        ocpn_options.double_arc_threshold =
            number(argv[++i], "--ocpn-double-arc-threshold");
      } else if (option == "--ocpn-noise-threshold" && i + 1 < argc) {
        ocpn_options.noise_threshold = number(argv[++i], "--ocpn-noise-threshold");
      } else if (option == "--no-edge-performance") {
        compute_edges_performance = false;
      } else if (option == "--no-description-timestamps") {
        include_description_timestamps = false;
      } else throw std::invalid_argument("unknown option: " + option);
    }
    if (format != "classic" && format != "standard") {
      throw std::invalid_argument("format must be classic or standard");
    }
    if (!describe.empty() && describe != "text" && describe != "tuples") {
      throw std::invalid_argument("describe must be text or tuples");
    }
    if (!discover.empty() && discover != "etot" && discover != "ocdfg" &&
        discover != "ocpn" && discover != "otg") {
      throw std::invalid_argument("discover must be etot, ocdfg, ocpn, or otg");
    }
    if (!conformance.empty() && conformance != "etot" && conformance != "ocdfg" &&
        conformance != "otg") {
      throw std::invalid_argument("conformance must be etot, ocdfg, or otg");
    }
    if (!conformance.empty() && discover != "")
      throw std::invalid_argument("--discover and --conformance are mutually exclusive");
    if (!describe.empty() && (!discover.empty() || !conformance.empty()))
      throw std::invalid_argument("--describe cannot be combined with discovery or conformance");
    if (!conformance.empty() && normative_path.empty())
      throw std::invalid_argument("--normative is required with --conformance");
    if (conformance.empty() && !normative_path.empty())
      throw std::invalid_argument("--normative requires --conformance");
    if (!describe.empty()) {
      minera::OcelDescriptionOptions options;
      options.include_timestamps = include_description_timestamps;
      if (describe == "text") {
        std::cout << minera::describe_ocel_text(log, options) << '\n';
      } else {
        std::cout << minera::serialize_ocel_description_json(
                         minera::describe_ocel_tuples(log, options))
                  << '\n';
      }
      return 0;
    }
    if (!conformance.empty()) {
      std::ifstream normative_input(normative_path, std::ios::binary);
      if (!normative_input) throw std::runtime_error("cannot open normative input");
      std::ostringstream normative_text;
      normative_text << normative_input.rdbuf();
      const auto normative = minera::parse_ocel_json(normative_text.str());
      if (conformance == "etot") {
        minera::EtotConformanceOptions options;
        options.alpha = alpha;
        options.beta = beta;
        options.gamma = gamma;
        options.theta_relative = etot_theta;
        std::cout << minera::etot_conformance_json(
                         minera::compare_etot(minera::discover_etot_classic(log),
                                              minera::discover_etot_classic(normative),
                                              options))
                  << '\n';
      } else if (conformance == "otg") {
        minera::OtgConformanceOptions options;
        options.alpha = alpha;
        options.beta = beta;
        options.gamma = gamma;
        options.theta = otg_theta;
        std::cout << minera::otg_conformance_json(
                         minera::compare_otg(minera::discover_otg_classic(log),
                                             minera::discover_otg_classic(normative),
                                             options))
                  << '\n';
      } else {
        minera::OcdfgConformanceOptions options;
        options.alpha = alpha;
        options.beta = beta;
        options.gamma = gamma;
        options.delta = delta;
        options.theta_activity = ocdfg_theta_activity;
        options.theta_flow = ocdfg_theta_flow;
        std::cout << minera::ocdfg_conformance_json(
                         minera::compare_ocdfg(
                             minera::discover_ocdfg_classic(log, false),
                             minera::discover_ocdfg_classic(normative, false),
                             options))
                  << '\n';
      }
      return 0;
    }
    if (discover == "etot") {
      std::cout << minera::etot_json(minera::discover_etot_classic(log)) << '\n';
      return 0;
    }
    if (discover == "otg") {
      std::cout << minera::otg_json(minera::discover_otg_classic(log)) << '\n';
      return 0;
    }
    if (discover == "ocdfg") {
      std::cout
          << minera::serialize_ocdfg_json(minera::discover_ocdfg_classic(
                 log, compute_edges_performance))
          << '\n';
      return 0;
    }
    if (discover == "ocpn") {
      std::cout << minera::serialize_ocpn_json(
                       minera::discover_ocpn(log, ocpn_options))
                << '\n';
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
