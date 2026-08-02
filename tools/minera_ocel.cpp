#include <fstream>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "minera/ocel.hpp"
#include "minera/ocel_csv.hpp"
#include "minera/oc_causal_net.hpp"
#include "minera/ocel_features.hpp"
#include "minera/ocel_playout.hpp"
#include "minera/ocel_transform.hpp"
#include "minera/saw_nets.hpp"

int main(int argc, char** argv) {
  try {
    if (argc < 2) {
      std::cerr << "usage: minera_ocel INPUT.json|INPUT.csv [--csv] [--csv-objects OBJECTS.csv] [--format classic|standard] [--describe text|tuples] [--features events|objects|events_objects] [--features-max N] [--discover etot|ocdfg|ocpn|otg|saw] [--ocpn-annotation annotated|none] [--split connected_components|ancestors_descendants] [--split-object-type TYPE] [--split-max-objects N] [--split-max-degree-centrality F] [--ocpn-playout --ocpn-max-bindings N --ocpn-max-traces N --ocpn-max-states N --ocpn-objects TYPE:ID1,ID2 --ocpn-unique-objects] [--oc-causal-net --ocpn-max-bindings N --ocpn-max-traces N --ocpn-max-states N --ocpn-objects TYPE:ID1,ID2 --ocpn-unique-objects] [--conformance etot|ocdfg|otg --normative MODEL.json]\n";
      return 2;
    }
    std::string format = "classic", describe, features, discover, split, conformance,
                        split_object_type, normative_path, csv_objects_path,
                        normative_csv_objects_path;
    bool csv_input = false;
    minera::Ocel log;
    bool ocpn_playout = false;
    bool oc_causal_net_playout = false;
    bool ocpn_unique_objects = false;
    minera::OcpnPlayoutOptions ocpn_playout_options;
    minera::OcelFeatureOptions feature_options;
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
    const auto list = [](const std::string& value) {
      std::vector<std::string> result;
      std::size_t begin = 0;
      while (begin <= value.size()) {
        const auto end = value.find(',', begin);
        const auto item = value.substr(begin, end == std::string::npos
                                                ? std::string::npos
                                                : end - begin);
        if (!item.empty()) result.push_back(item);
        if (end == std::string::npos) break;
        begin = end + 1;
      }
      return result;
    };
    for (int i = 2; i < argc; ++i) {
      const std::string option = argv[i];
      if (option == "--csv") {
        csv_input = true;
      } else if (option == "--csv-objects" && i + 1 < argc) {
        csv_objects_path = argv[++i];
        csv_input = true;
      } else if (option == "--normative-csv-objects" && i + 1 < argc) {
        normative_csv_objects_path = argv[++i];
      } else if ((option == "--format" || option == "--features" ||
           option == "--discover" ||
           option == "--split") && i + 1 < argc) {
        auto& target = option == "--format" ? format
                      : option == "--features" ? features
                      : option == "--discover" ? discover
                                                 : split;
        target = argv[++i];
      } else if (option == "--features-max" && i + 1 < argc) {
        const auto parsed = number(argv[++i], "--features-max");
        if (parsed > static_cast<double>(std::numeric_limits<std::size_t>::max()) ||
            std::floor(parsed) != parsed || parsed == 0.0)
          throw std::invalid_argument("--features-max expects a positive integer");
        feature_options.max_features = static_cast<std::size_t>(parsed);
      } else if ((option == "--features-numeric-events" ||
                  option == "--features-string-events" ||
                  option == "--features-numeric-objects" ||
                  option == "--features-string-objects") && i + 1 < argc) {
        const auto values = list(argv[++i]);
        if (option == "--features-numeric-events")
          feature_options.numeric_event_attributes = values;
        else if (option == "--features-string-events")
          feature_options.string_event_attributes = values;
        else if (option == "--features-numeric-objects")
          feature_options.numeric_object_attributes = values;
        else
          feature_options.string_object_attributes = values;
      } else if (option == "--features-no-activity") {
        feature_options.include_activity_features = false;
      } else if (option == "--features-no-timestamps") {
        feature_options.include_timestamp_features = false;
      } else if (option == "--features-no-relations") {
        feature_options.include_relation_features = false;
      } else if (option == "--features-no-endpoints") {
        feature_options.include_endpoint_features = false;
      } else if (option == "--features-no-attribute-counts") {
        feature_options.include_attribute_counts = false;
      } else if (option == "--features-no-interactions") {
        feature_options.include_interaction_features = false;
      } else if (option == "--features-no-lifecycle") {
        feature_options.include_lifecycle_features = false;
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
      } else if (option == "--oc-causal-net") {
        oc_causal_net_playout = true;
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
      } else if (option == "--ocpn-annotation" && i + 1 < argc) {
        const std::string annotation = argv[++i];
        if (annotation == "annotated" || annotation == "classic") {
          ocpn_options.annotated = true;
        } else if (annotation == "none" || annotation == "wo_annotation" ||
                   annotation == "without") {
          ocpn_options.annotated = false;
        } else {
          throw std::invalid_argument(
              "--ocpn-annotation expects annotated or none");
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
    if (!features.empty() && features != "events" && features != "objects" &&
        features != "events_objects") {
      throw std::invalid_argument(
          "features must be events, objects, or events_objects");
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
        (!discover.empty() || !split.empty() || !features.empty() ||
         ocpn_playout || oc_causal_net_playout))
      throw std::invalid_argument(
          "--conformance cannot be combined with discovery or split");
    if (!discover.empty() && !split.empty())
      throw std::invalid_argument("--discover and --split are mutually exclusive");
    if ((ocpn_playout || oc_causal_net_playout) &&
        (!discover.empty() || !split.empty()))
      throw std::invalid_argument(
          "object-centric playout cannot be combined with discovery or split");
    if (ocpn_playout && oc_causal_net_playout)
      throw std::invalid_argument(
          "--ocpn-playout and --oc-causal-net are mutually exclusive");
    if (!describe.empty() &&
        (!discover.empty() || !split.empty() || !features.empty() || !conformance.empty() ||
         ocpn_playout || oc_causal_net_playout))
      throw std::invalid_argument(
          "--describe cannot be combined with discovery, split, or conformance");
    if (!conformance.empty() && normative_path.empty())
      throw std::invalid_argument("--normative is required with --conformance");
    if (conformance.empty() && !normative_path.empty())
      throw std::invalid_argument("--normative requires --conformance");
    const std::filesystem::path input_path(argv[1]);
    const auto lower_extension = [](const std::filesystem::path& path) {
      std::string extension = path.extension().string();
      for (auto& ch : extension) {
        if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
      }
      return extension;
    };
    csv_input = csv_input || lower_extension(input_path) == ".csv";
    if (!csv_input && !csv_objects_path.empty()) {
      throw std::invalid_argument("--csv-objects requires a CSV input");
    }
    if (!normative_csv_objects_path.empty() && normative_path.empty()) {
      throw std::invalid_argument(
          "--normative-csv-objects requires --normative");
    }
    {
      minera::OcelCsvImportOptions csv_options;
      if (csv_input) {
        if (!csv_objects_path.empty()) {
          log = minera::import_ocel_csv(input_path,
                                        std::filesystem::path(csv_objects_path),
                                        csv_options);
        } else {
          log = minera::import_ocel_csv(input_path, csv_options);
        }
      } else {
        std::ifstream input(input_path, std::ios::binary);
        if (!input) throw std::runtime_error("cannot open input");
        std::ostringstream text;
        text << input.rdbuf();
        log = minera::parse_ocel_json(text.str());
      }
    }
    if (!features.empty() &&
        (!discover.empty() || !split.empty() || !conformance.empty() ||
         ocpn_playout || oc_causal_net_playout))
      throw std::invalid_argument(
          "--features cannot be combined with discovery, split, conformance, or playout");
    if (!features.empty()) {
      const auto variant = features == "events"
                               ? minera::OcelFeatureVariant::kEvents
                               : features == "objects"
                                     ? minera::OcelFeatureVariant::kObjects
                                     : minera::OcelFeatureVariant::kEventsObjects;
      std::cout << minera::serialize_ocel_features_json(
                       minera::extract_ocel_features(log, variant,
                                                     feature_options))
                << '\n';
      return 0;
    }
    if (ocpn_playout) {
      ocpn_playout_options.objects_unique_per_trace = ocpn_unique_objects;
      std::cout << minera::serialize_ocpn_playout_json(
                       minera::ocpn_extensive_playout(
                           minera::discover_ocpn(log), ocpn_playout_options))
                << '\n';
      return 0;
    }
    if (oc_causal_net_playout) {
      ocpn_playout_options.objects_unique_per_trace = ocpn_unique_objects;
      const auto model = minera::discover_oc_causal_net(log, ocpn_options);
      std::cout << minera::serialize_oc_causal_net_playout_json(
                       minera::oc_causal_net_extensive_playout(
                           model, ocpn_playout_options))
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
      const std::filesystem::path normative_file(normative_path);
      minera::Ocel normative;
      if (lower_extension(normative_file) == ".csv") {
        if (!normative_csv_objects_path.empty()) {
          normative = minera::import_ocel_csv(
              normative_file,
              std::filesystem::path(normative_csv_objects_path));
        } else {
          normative = minera::import_ocel_csv(normative_file);
        }
      } else {
        std::ifstream normative_input(normative_file, std::ios::binary);
        if (!normative_input) throw std::runtime_error("cannot open normative input");
        std::ostringstream normative_text;
        normative_text << normative_input.rdbuf();
        normative = minera::parse_ocel_json(normative_text.str());
      }
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
