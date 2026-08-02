#include <fstream>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>

#include "minera/ocel.hpp"
#include "minera/ocel_playout.hpp"
#include "minera/ocel_transform.hpp"
#include "minera/saw_nets.hpp"

int main(int argc, char** argv) {
  try {
    if (argc < 2) {
      std::cerr << "usage: minera_ocel INPUT.json [--format classic|standard] [--describe text|tuples] [--no-description-timestamps] [--discover etot|ocdfg|ocpn|otg|saw] [--split connected_components|ancestors_descendants] [--split-object-type TYPE] [--split-max-objects N] [--split-max-degree-centrality F] [--ocpn-playout --ocpn-max-bindings N --ocpn-max-traces N --ocpn-max-states N --ocpn-objects TYPE:ID1,ID2 --ocpn-unique-objects] [--conformance etot|ocdfg|otg --normative MODEL.json]\n";
      return 2;
    }
    std::ifstream input(argv[1], std::ios::binary);
    if (!input) throw std::runtime_error("cannot open input");
    std::ostringstream text; text << input.rdbuf();
    minera::Ocel log = minera::parse_ocel_json(text.str());
    std::string format = "classic", describe, discover, split, conformance,
                        split_object_type, normative_path;
    bool ocpn_playout = false;
    bool ocpn_unique_objects = false;
    minera::OcpnPlayoutOptions ocpn_playout_options;
    std::size_t split_max_objects = std::numeric_limits<std::size_t>::max();
    std::optional<double> split_max_degree_centrality;
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
      if ((option == "--format" || option == "--discover" ||
           option == "--split") && i + 1 < argc) {
        auto& target = option == "--format" ? format
                      : option == "--discover" ? discover
                                                 : split;
        target = argv[++i];
      } else if (option == "--describe" && i + 1 < argc) {
        describe = argv[++i];
      } else if (option == "--split-object-type" && i + 1 < argc) {
        split_object_type = argv[++i];
      } else if (option == "--split-max-objects" && i + 1 < argc) {
        const auto parsed = number(argv[++i], "--split-max-objects");
        if (parsed > static_cast<double>(std::numeric_limits<std::size_t>::max()) ||
            std::floor(parsed) != parsed)
          throw std::invalid_argument(
              "--split-max-objects expects a non-negative integer");
        split_max_objects = static_cast<std::size_t>(parsed);
      } else if (option == "--split-max-degree-centrality" && i + 1 < argc) {
        split_max_degree_centrality =
            number(argv[++i], "--split-max-degree-centrality");
      } else if (option == "--ocpn-playout") {
        ocpn_playout = true;
      } else if (option == "--ocpn-unique-objects") {
        ocpn_unique_objects = true;
      } else if ((option == "--ocpn-max-bindings" ||
                  option == "--ocpn-max-traces" ||
                  option == "--ocpn-max-states") && i + 1 < argc) {
        const auto parsed = number(argv[++i], option.c_str());
        if (parsed > static_cast<double>(std::numeric_limits<std::size_t>::max()) ||
            std::floor(parsed) != parsed)
          throw std::invalid_argument(option + " expects a non-negative integer");
        const auto value = static_cast<std::size_t>(parsed);
        if (option == "--ocpn-max-bindings")
          ocpn_playout_options.max_bindings_per_activity = value;
        else if (option == "--ocpn-max-traces")
          ocpn_playout_options.max_traces = value;
        else
          ocpn_playout_options.max_states = value;
      } else if (option == "--ocpn-objects" && i + 1 < argc) {
        const std::string spec = argv[++i];
        const auto separator = spec.find(':');
        if (separator == std::string::npos || separator == 0 ||
            separator + 1 >= spec.size())
          throw std::invalid_argument(
              "--ocpn-objects expects TYPE:ID1,ID2");
        const auto type = spec.substr(0, separator);
        const auto ids = spec.substr(separator + 1);
        std::size_t begin = 0;
        while (begin <= ids.size()) {
          const auto end = ids.find(',', begin);
          const auto id = ids.substr(begin, end == std::string::npos
                                              ? std::string::npos
                                              : end - begin);
          if (id.empty())
            throw std::invalid_argument("--ocpn-objects contains an empty id");
          ocpn_playout_options.objects[type].push_back(id);
          if (end == std::string::npos) break;
          begin = end + 1;
        }
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
        discover != "ocpn" && discover != "otg" && discover != "saw") {
      throw std::invalid_argument("discover must be etot, ocdfg, ocpn, otg, or saw");
    }
    if (!split.empty() && split != "connected_components" &&
        split != "ancestors_descendants") {
      throw std::invalid_argument(
          "split must be connected_components or ancestors_descendants");
    }
    if (!conformance.empty() && conformance != "etot" && conformance != "ocdfg" &&
        conformance != "otg") {
      throw std::invalid_argument("conformance must be etot, ocdfg, or otg");
    }
    if (!conformance.empty() &&
        (!discover.empty() || !split.empty() || ocpn_playout))
      throw std::invalid_argument(
          "--conformance cannot be combined with discovery or split");
    if (!discover.empty() && !split.empty())
      throw std::invalid_argument("--discover and --split are mutually exclusive");
    if (ocpn_playout && (!discover.empty() || !split.empty()))
      throw std::invalid_argument(
          "--ocpn-playout cannot be combined with discovery or split");
    if (!describe.empty() &&
        (!discover.empty() || !split.empty() || !conformance.empty() ||
         ocpn_playout))
      throw std::invalid_argument(
          "--describe cannot be combined with discovery, split, or conformance");
    if (!conformance.empty() && normative_path.empty())
      throw std::invalid_argument("--normative is required with --conformance");
    if (conformance.empty() && !normative_path.empty())
      throw std::invalid_argument("--normative requires --conformance");
    if (ocpn_playout) {
      ocpn_playout_options.objects_unique_per_trace = ocpn_unique_objects;
      std::cout << minera::serialize_ocpn_playout_json(
                       minera::ocpn_extensive_playout(
                           minera::discover_ocpn(log), ocpn_playout_options))
                << '\n';
      return 0;
    }
    if (!split.empty()) {
      minera::OcelSplitOptions options;
      options.variant = split == "connected_components"
                            ? minera::OcelSplitVariant::kConnectedComponents
                            : minera::OcelSplitVariant::kAncestorsDescendants;
      options.object_type = split_object_type;
      options.max_objects = split_max_objects;
      options.max_degree_centrality = split_max_degree_centrality;
      std::cout << minera::serialize_ocel_split_json(
                       minera::split_ocel(log, options))
                << '\n';
      return 0;
    }
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
    if (discover == "saw") {
      std::cout << minera::saw_nets_json(minera::discover_saw_nets(log))
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
