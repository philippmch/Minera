#include <fstream>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "minera/ocel.hpp"
#include "minera/ocel_csv.hpp"
#include "minera/ocel_sqlite.hpp"
#include "minera/ocel_xml.hpp"
#include "minera/oc_causal_net.hpp"
#include "minera/ocel_features.hpp"
#include "minera/ocel_filter.hpp"
#include "minera/ocel_statistics.hpp"
#include "minera/ocel_playout.hpp"
#include "minera/ocel_transform.hpp"
#include "minera/saw_nets.hpp"

int main(int argc, char** argv) {
  try {
    if (argc < 2) {
      std::cerr << "usage: minera_ocel INPUT.json|INPUT.csv|INPUT.xml|INPUT.sqlite [--csv] [--csv-objects OBJECTS.csv] [--xml] [--sqlite] [--sqlite-output OUTPUT.sqlite] [--format classic|standard|xml|xml_standard|sqlite] [--filter-object-count TYPE:N ...] [--filter-object-types TYPE1,TYPE2] [--filter-object-ids ID1,ID2] [--filter-object-level N] [--filter-object-negative] [--filter-start-events TYPE] [--filter-end-events TYPE] [--filter-activities-object-type TYPE] [--filter-cc-object ID] [--filter-cc-length MIN:MAX] [--filter-cc-object-type TYPE] [--filter-cc-object-type-negative] [--filter-cc-activity ACTIVITY] [--statistics object_type_activities|objects_ot_count|activity_associations|object_type_activity_associations|edge_associations] [--statistics-prefilter none|start|end] [--statistics-business-hours] [--statistics-business-hour-slots START-END,...] [--describe text|tuples|variant1|variant2] [--no-description-timestamps] [--features events|objects|events_objects] [--features-max N] [--discover etot|ocdfg|ocpn|otg|saw] [--ocpn-annotation annotated|none] [--enrich interaction,descendants,inheritance,cobirth,codeath] [--enrich-no-lifecycle] [--split connected_components|ancestors_descendants] [--split-object-type TYPE] [--split-max-objects N] [--split-max-degree-centrality F] [--ocpn-playout --ocpn-max-bindings N --ocpn-branching-transitions N --ocpn-branching-bindings N --ocpn-max-traces N --ocpn-max-states N --ocpn-objects TYPE:ID1,ID2 --ocpn-unique-objects --ocpn-event-binding independent|shared_activity|synchronized --ocpn-exists-trace --ocpn-return-sequences] [--oc-causal-net --ocpn-max-bindings N --ocpn-branching-transitions N --ocpn-branching-bindings N --ocpn-max-traces N --ocpn-max-states N --ocpn-objects TYPE:ID1,ID2 --ocpn-unique-objects --ocpn-event-binding independent|shared_activity|synchronized --ocpn-exists-trace --ocpn-return-sequences] [--conformance etot|ocdfg|otg --normative MODEL.json]\n";
      return 2;
    }
    std::string format = "classic", describe, features, discover, enrich, split, conformance,
                        statistics, statistics_prefilter = "none",
                        split_object_type, normative_path, csv_objects_path,
                        normative_csv_objects_path;
    std::map<std::string, std::size_t> filter_object_counts;
    std::vector<std::string> filter_object_types, filter_object_ids;
    std::size_t filter_object_level = 1;
    bool filter_object_level_set = false, filter_object_negative = false;
    std::string filter_start_events_type, filter_end_events_type;
    std::string filter_activities_object_type, filter_cc_object,
        filter_cc_object_type, filter_cc_activity;
    std::optional<std::pair<std::size_t, std::size_t>> filter_cc_length;
    bool filter_cc_object_type_negative = false;
    bool csv_input = false, xml_input = false, sqlite_input = false;
    std::string sqlite_output_path;
    minera::Ocel log;
    bool ocpn_playout = false;
    bool oc_causal_net_playout = false;
    bool ocpn_unique_objects = false;
    bool ocpn_event_binding_used = false;
    minera::OcpnPlayoutOptions ocpn_playout_options;
    minera::OcelFeatureOptions feature_options;
    minera::OcelEnrichmentOptions enrichment_options;
    bool enrich_no_lifecycle = false;
    std::size_t split_max_objects = std::numeric_limits<std::size_t>::max();
    std::optional<double> split_max_degree_centrality;
    bool compute_edges_performance = true;
    bool statistics_business_hours = false;
    std::vector<minera::BusinessHourSlot> statistics_business_hour_slots;
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
    const auto business_slots = [](const std::string& value) {
      std::vector<minera::BusinessHourSlot> slots;
      for (const auto& part : [&] {
             std::vector<std::string> parts;
             std::size_t begin = 0;
             while (begin <= value.size()) {
               const auto end = value.find(',', begin);
               parts.push_back(value.substr(begin, end == std::string::npos
                                                     ? std::string::npos
                                                     : end - begin));
               if (end == std::string::npos) break;
               begin = end + 1;
             }
             return parts;
           }()) {
        const auto separator = part.find(':') == std::string::npos
                                   ? part.find('-')
                                   : part.find(':');
        if (separator == std::string::npos || separator == 0 ||
            separator + 1 == part.size())
          throw std::invalid_argument(
              "--statistics-business-hour-slots expects START-END pairs");
        const auto parse = [&](std::string_view text) {
          const std::string owned(text);
          char* end = nullptr;
          const double parsed = std::strtod(owned.c_str(), &end);
          if (end != owned.c_str() + owned.size() || !std::isfinite(parsed))
            throw std::invalid_argument(
                "--statistics-business-hour-slots expects finite seconds");
          return parsed;
        };
        const double start = parse(std::string_view(part).substr(0, separator));
        const double end = parse(std::string_view(part).substr(separator + 1));
        if (start < 0.0 || start >= end || end > 604800.0)
          throw std::invalid_argument(
              "--statistics-business-hour-slots requires 0 <= start < end <= 604800");
        slots.push_back({start, end});
      }
      if (slots.empty())
        throw std::invalid_argument(
            "--statistics-business-hour-slots expects a non-empty list");
      return slots;
    };
    for (int i = 2; i < argc; ++i) {
      const std::string option = argv[i];
      if (option == "--csv") {
        csv_input = true;
      } else if (option == "--xml") {
        xml_input = true;
      } else if (option == "--sqlite") {
        sqlite_input = true;
      } else if (option == "--sqlite-output" && i + 1 < argc) {
        sqlite_output_path = argv[++i];
        if (sqlite_output_path.empty())
          throw std::invalid_argument("--sqlite-output expects a path");
        format = "sqlite";
      } else if (option == "--csv-objects" && i + 1 < argc) {
        csv_objects_path = argv[++i];
        csv_input = true;
      } else if (option == "--normative-csv-objects" && i + 1 < argc) {
        normative_csv_objects_path = argv[++i];
      } else if (option == "--filter-object-count" && i + 1 < argc) {
        const std::string spec = argv[++i];
        const auto separator = spec.find(':');
        if (separator == std::string::npos || separator == 0 ||
            separator + 1 >= spec.size()) {
          throw std::invalid_argument(
              "--filter-object-count expects TYPE:N");
        }
        const auto type = spec.substr(0, separator);
        const auto count_text = spec.substr(separator + 1);
        const auto parsed = number(count_text, "--filter-object-count");
        if (parsed < 0.0 || std::floor(parsed) != parsed ||
            parsed > static_cast<double>(std::numeric_limits<std::size_t>::max())) {
          throw std::invalid_argument(
              "--filter-object-count expects a non-negative integer count");
        }
        if (!filter_object_counts.emplace(type, static_cast<std::size_t>(parsed)).second) {
          throw std::invalid_argument(
              "--filter-object-count repeats an object type");
        }
      } else if ((option == "--filter-object-types" ||
                  option == "--filter-object-ids") && i + 1 < argc) {
        const auto values = list(argv[++i]);
        if (values.empty()) {
          throw std::invalid_argument(option + " expects a non-empty list");
        }
        if (option == "--filter-object-types")
          filter_object_types = values;
        else
          filter_object_ids = values;
      } else if (option == "--filter-object-level" && i + 1 < argc) {
        const auto parsed = number(argv[++i], "--filter-object-level");
        if (std::floor(parsed) != parsed ||
            parsed > static_cast<double>(std::numeric_limits<std::size_t>::max())) {
          throw std::invalid_argument(
              "--filter-object-level expects a non-negative integer");
        }
        filter_object_level = static_cast<std::size_t>(parsed);
        filter_object_level_set = true;
      } else if (option == "--filter-object-negative") {
        filter_object_negative = true;
      } else if (option == "--filter-start-events" && i + 1 < argc) {
        if (!filter_start_events_type.empty()) {
          throw std::invalid_argument("--filter-start-events may be specified once");
        }
        filter_start_events_type = argv[++i];
        if (filter_start_events_type.empty()) {
          throw std::invalid_argument("--filter-start-events expects a type");
        }
      } else if (option == "--filter-end-events" && i + 1 < argc) {
        if (!filter_end_events_type.empty()) {
          throw std::invalid_argument("--filter-end-events may be specified once");
        }
        filter_end_events_type = argv[++i];
        if (filter_end_events_type.empty()) {
          throw std::invalid_argument("--filter-end-events expects a type");
        }
      } else if (option == "--filter-activities-object-type" && i + 1 < argc) {
        if (!filter_activities_object_type.empty()) {
          throw std::invalid_argument(
              "--filter-activities-object-type may be specified once");
        }
        filter_activities_object_type = argv[++i];
        if (filter_activities_object_type.empty()) {
          throw std::invalid_argument(
              "--filter-activities-object-type expects a type");
        }
      } else if (option == "--filter-cc-object" && i + 1 < argc) {
        if (!filter_cc_object.empty()) {
          throw std::invalid_argument("--filter-cc-object may be specified once");
        }
        filter_cc_object = argv[++i];
        if (filter_cc_object.empty())
          throw std::invalid_argument("--filter-cc-object expects an id");
      } else if (option == "--filter-cc-length" && i + 1 < argc) {
        const std::string spec = argv[++i];
        const auto separator = spec.find(':');
        if (separator == std::string::npos || separator == 0 ||
            separator + 1 >= spec.size()) {
          throw std::invalid_argument(
              "--filter-cc-length expects MIN:MAX");
        }
        const auto parse_size = [&](const std::string& text) {
          const auto parsed = number(text, "--filter-cc-length");
          if (std::floor(parsed) != parsed ||
              parsed > static_cast<double>(std::numeric_limits<std::size_t>::max())) {
            throw std::invalid_argument(
                "--filter-cc-length expects non-negative integer bounds");
          }
          return static_cast<std::size_t>(parsed);
        };
        filter_cc_length = std::make_pair(
            parse_size(spec.substr(0, separator)),
            parse_size(spec.substr(separator + 1)));
      } else if (option == "--filter-cc-object-type" && i + 1 < argc) {
        if (!filter_cc_object_type.empty()) {
          throw std::invalid_argument(
              "--filter-cc-object-type may be specified once");
        }
        filter_cc_object_type = argv[++i];
        if (filter_cc_object_type.empty())
          throw std::invalid_argument("--filter-cc-object-type expects a type");
      } else if (option == "--filter-cc-object-type-negative") {
        filter_cc_object_type_negative = true;
      } else if (option == "--filter-cc-activity" && i + 1 < argc) {
        if (!filter_cc_activity.empty()) {
          throw std::invalid_argument(
              "--filter-cc-activity may be specified once");
        }
        filter_cc_activity = argv[++i];
        if (filter_cc_activity.empty())
          throw std::invalid_argument("--filter-cc-activity expects an activity");
      } else if (option == "--statistics" && i + 1 < argc) {
        if (!statistics.empty())
          throw std::invalid_argument("--statistics may be specified once");
        statistics = argv[++i];
      } else if (option == "--statistics-prefilter" && i + 1 < argc) {
        if (statistics_prefilter != "none")
          throw std::invalid_argument("--statistics-prefilter may be specified once");
        statistics_prefilter = argv[++i];
      } else if (option == "--statistics-business-hours") {
        statistics_business_hours = true;
      } else if (option == "--statistics-business-hour-slots" && i + 1 < argc) {
        if (!statistics_business_hour_slots.empty())
          throw std::invalid_argument(
              "--statistics-business-hour-slots may be specified once");
        statistics_business_hour_slots = business_slots(argv[++i]);
        statistics_business_hours = true;
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
      } else if (option == "--enrich" && i + 1 < argc) {
        if (!enrich.empty())
          throw std::invalid_argument("--enrich may be specified once");
        enrich = argv[++i];
      } else if (option == "--enrich-no-lifecycle") {
        enrich_no_lifecycle = true;
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
      } else if (option == "--ocpn-exists-trace") {
        ocpn_playout_options.exists_trace = true;
      } else if (option == "--ocpn-return-sequences") {
        ocpn_playout_options.return_sequences = true;
      } else if (option == "--ocpn-event-binding" && i + 1 < argc) {
        ocpn_event_binding_used = true;
        const auto binding = std::string(argv[++i]);
        if (binding == "independent") {
          ocpn_playout_options.event_binding =
              minera::OcpnEventBinding::kIndependent;
        } else if (binding == "shared_activity") {
          ocpn_playout_options.event_binding =
              minera::OcpnEventBinding::kSharedActivity;
        } else if (binding == "synchronized") {
          ocpn_playout_options.event_binding =
              minera::OcpnEventBinding::kSynchronized;
        } else {
          throw std::invalid_argument(
              "--ocpn-event-binding expects independent, shared_activity, or synchronized");
        }
      } else if ((option == "--ocpn-max-bindings" ||
                  option == "--ocpn-branching-transitions" ||
                  option == "--ocpn-branching-bindings" ||
                  option == "--ocpn-max-traces" ||
                  option == "--ocpn-max-states") && i + 1 < argc) {
        const auto parsed = number(argv[++i], option.c_str());
        if (parsed > static_cast<double>(std::numeric_limits<std::size_t>::max()) ||
            std::floor(parsed) != parsed)
          throw std::invalid_argument(option + " expects a non-negative integer");
        const auto value = static_cast<std::size_t>(parsed);
        if (option == "--ocpn-max-bindings")
          ocpn_playout_options.max_bindings_per_activity = value;
        else if (option == "--ocpn-branching-transitions")
          ocpn_playout_options.branching_factor_transitions = value;
        else if (option == "--ocpn-branching-bindings")
          ocpn_playout_options.branching_factor_bindings = value;
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
    if (format != "classic" && format != "standard" && format != "xml" &&
        format != "xml_standard" && format != "sqlite") {
      throw std::invalid_argument(
          "format must be classic, standard, xml, xml_standard, or sqlite");
    }
    if (format == "sqlite" && sqlite_output_path.empty())
      throw std::invalid_argument(
          "--format sqlite requires --sqlite-output OUTPUT.sqlite");
    if (!sqlite_output_path.empty() && format != "sqlite")
      throw std::invalid_argument(
          "--sqlite-output requires --format sqlite");
    if (!enrich.empty() && format != "classic")
      throw std::invalid_argument(
          "--enrich emits the OCEL_ENRICHMENT JSON envelope and requires the default format");
    if (enrich_no_lifecycle && enrich.empty())
      throw std::invalid_argument("--enrich-no-lifecycle requires --enrich");
    if (!enrich.empty()) {
      const auto values = list(enrich);
      if (values.empty())
        throw std::invalid_argument("--enrich expects a non-empty graph list");
      enrichment_options.include_interaction = false;
      enrichment_options.include_descendants = false;
      enrichment_options.include_inheritance = false;
      enrichment_options.include_cobirth = false;
      enrichment_options.include_codeath = false;
      for (const auto& value : values) {
        if (value == "all") {
          enrichment_options.include_interaction = true;
          enrichment_options.include_descendants = true;
          enrichment_options.include_inheritance = true;
          enrichment_options.include_cobirth = true;
          enrichment_options.include_codeath = true;
        } else if (value == "interaction" || value == "object_interaction_graph") {
          enrichment_options.include_interaction = true;
        } else if (value == "descendants" || value == "object_descendants_graph") {
          enrichment_options.include_descendants = true;
        } else if (value == "inheritance" || value == "object_inheritance_graph") {
          enrichment_options.include_inheritance = true;
        } else if (value == "cobirth" || value == "object_cobirth_graph") {
          enrichment_options.include_cobirth = true;
        } else if (value == "codeath" || value == "object_codeath_graph") {
          enrichment_options.include_codeath = true;
        } else {
          throw std::invalid_argument(
              "--enrich expects interaction, descendants, inheritance, cobirth, codeath, or all");
        }
      }
      enrichment_options.include_lifecycle_qualifiers = !enrich_no_lifecycle;
    }
    if (!describe.empty() && describe != "text" && describe != "tuples" &&
        describe != "variant1" && describe != "variant2") {
      throw std::invalid_argument(
          "describe must be text, tuples, variant1, or variant2");
    }
    if (!features.empty() && features != "events" && features != "objects" &&
        features != "events_objects") {
      throw std::invalid_argument(
          "features must be events, objects, or events_objects");
    }
    if (!statistics.empty() && statistics != "object_type_activities" &&
        statistics != "objects_ot_count" &&
        statistics != "activity_associations" &&
        statistics != "object_type_activity_associations" &&
        statistics != "edge_associations") {
      throw std::invalid_argument(
          "statistics must be object_type_activities, objects_ot_count, activity_associations, object_type_activity_associations, or edge_associations");
    }
    if (statistics_prefilter != "none" && statistics_prefilter != "start" &&
        statistics_prefilter != "end")
      throw std::invalid_argument(
          "statistics-prefilter must be none, start, or end");
    if (statistics_prefilter != "none" &&
        statistics != "activity_associations" &&
        statistics != "object_type_activity_associations")
      throw std::invalid_argument(
          "statistics-prefilter applies only to association statistics");
    if ((statistics_business_hours || !statistics_business_hour_slots.empty()) &&
        statistics != "edge_associations")
      throw std::invalid_argument(
          "statistics business-hour options apply only to edge_associations");
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
        (!discover.empty() || !enrich.empty() || !split.empty() || !features.empty() ||
         !statistics.empty() ||
         ocpn_playout || oc_causal_net_playout))
      throw std::invalid_argument(
          "--conformance cannot be combined with discovery, enrichment, or split");
    if ((!discover.empty() && !split.empty()) ||
        (!discover.empty() && !enrich.empty()) ||
        (!split.empty() && !enrich.empty()))
      throw std::invalid_argument(
          "--discover, --enrich, and --split are mutually exclusive");
    if ((ocpn_playout || oc_causal_net_playout) &&
        (!discover.empty() || !enrich.empty() || !split.empty()))
      throw std::invalid_argument(
          "object-centric playout cannot be combined with discovery, enrichment, or split");
    if (ocpn_event_binding_used && !ocpn_playout && !oc_causal_net_playout)
      throw std::invalid_argument(
          "--ocpn-event-binding requires --ocpn-playout or --oc-causal-net");
    if (ocpn_playout && oc_causal_net_playout)
      throw std::invalid_argument(
          "--ocpn-playout and --oc-causal-net are mutually exclusive");
    if (!describe.empty() &&
        (!discover.empty() || !enrich.empty() || !split.empty() || !features.empty() ||
         !statistics.empty() || !conformance.empty() ||
         ocpn_playout || oc_causal_net_playout))
      throw std::invalid_argument(
          "--describe cannot be combined with discovery, enrichment, split, or conformance");
    if (!statistics.empty() &&
        (!discover.empty() || !enrich.empty() || !split.empty() || !features.empty() ||
         !conformance.empty() || ocpn_playout || oc_causal_net_playout ||
         !describe.empty()))
      throw std::invalid_argument(
          "--statistics cannot be combined with other OCEL output modes");
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
    const auto input_extension = lower_extension(input_path);
    csv_input = csv_input || input_extension == ".csv";
    xml_input = xml_input || input_extension == ".xml" || input_extension == ".xmlocel";
    sqlite_input = sqlite_input || input_extension == ".sqlite" ||
                   input_extension == ".sqlite3" || input_extension == ".db";
    if ((csv_input && xml_input) || (csv_input && sqlite_input) ||
        (xml_input && sqlite_input)) {
      throw std::invalid_argument(
          "CSV, XML, and SQLite input modes are mutually exclusive");
    }
    if (!csv_input && !csv_objects_path.empty()) {
      throw std::invalid_argument("--csv-objects requires a CSV input");
    }
    if (!normative_csv_objects_path.empty() && normative_path.empty()) {
      throw std::invalid_argument(
          "--normative-csv-objects requires --normative");
    }
    if (!normative_csv_objects_path.empty() &&
        lower_extension(std::filesystem::path(normative_path)) != ".csv") {
      throw std::invalid_argument(
          "--normative-csv-objects requires a CSV normative log");
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
      } else if (xml_input) {
        log = minera::import_ocel_xml(input_path);
      } else if (sqlite_input) {
        log = minera::import_ocel_sqlite(input_path);
      } else {
        std::ifstream input(input_path, std::ios::binary);
        if (!input) throw std::runtime_error("cannot open input");
        std::ostringstream text;
        text << input.rdbuf();
        log = minera::parse_ocel_json(text.str());
      }
    }
    if (filter_object_level_set && filter_object_types.empty() &&
        filter_object_ids.empty()) {
      throw std::invalid_argument(
          "--filter-object-level requires object type or id filters");
    }
    if (filter_object_negative && filter_object_types.empty() &&
        filter_object_ids.empty()) {
      throw std::invalid_argument(
          "--filter-object-negative requires object type or id filters");
    }
    if (!filter_object_types.empty()) {
      log = minera::filter_ocel_object_types(
          log, filter_object_types, !filter_object_negative, filter_object_level);
    }
    if (!filter_object_ids.empty()) {
      log = minera::filter_ocel_objects(
          log, filter_object_ids, !filter_object_negative, filter_object_level);
    }
    if (!filter_object_counts.empty()) {
      log = minera::filter_ocel_events_object_type_counts(
          log, filter_object_counts);
    }
    if (!filter_start_events_type.empty()) {
      log = minera::filter_ocel_start_events_per_object_type(
          log, filter_start_events_type);
    }
    if (!filter_end_events_type.empty()) {
      log = minera::filter_ocel_end_events_per_object_type(
          log, filter_end_events_type);
    }
    if (!filter_activities_object_type.empty()) {
      log = minera::filter_ocel_activities_connected_object_type(
          log, filter_activities_object_type);
    }
    if (!filter_cc_object.empty()) {
      log = minera::filter_ocel_cc_object(log, filter_cc_object);
    }
    if (filter_cc_length) {
      log = minera::filter_ocel_cc_length(
          log, filter_cc_length->first, filter_cc_length->second);
    }
    if (!filter_cc_object_type.empty()) {
      log = minera::filter_ocel_cc_object_type(
          log, filter_cc_object_type, !filter_cc_object_type_negative);
    } else if (filter_cc_object_type_negative) {
      throw std::invalid_argument(
          "--filter-cc-object-type-negative requires --filter-cc-object-type");
    }
    if (!filter_cc_activity.empty()) {
      log = minera::filter_ocel_cc_activity(log, filter_cc_activity);
    }
    if (!features.empty() &&
        (!discover.empty() || !enrich.empty() || !split.empty() || !conformance.empty() ||
         !statistics.empty() || ocpn_playout || oc_causal_net_playout))
      throw std::invalid_argument(
          "--features cannot be combined with discovery, enrichment, split, conformance, or playout");
    if (!statistics.empty()) {
      const auto prefilter = statistics_prefilter == "start"
                                 ? minera::OcelAssociationPrefilter::kStart
                                 : statistics_prefilter == "end"
                                       ? minera::OcelAssociationPrefilter::kEnd
                                       : minera::OcelAssociationPrefilter::kNone;
      if (statistics == "activity_associations") {
        std::cout << minera::serialize_ocel_statistics_json(
                         minera::activity_associations(log, prefilter))
                  << '\n';
      } else if (statistics == "object_type_activity_associations") {
        std::cout << minera::serialize_ocel_statistics_json(
                         minera::object_type_activity_associations(log, prefilter))
                  << '\n';
      } else if (statistics == "edge_associations") {
        minera::OcelEdgeAssociationOptions edge_options;
        edge_options.use_business_hours = statistics_business_hours;
        edge_options.business_hour_slots = statistics_business_hour_slots;
        std::cout << minera::serialize_ocel_statistics_json(
                         minera::edge_associations(log, edge_options))
                  << '\n';
      } else {
        const auto variant = statistics == "object_type_activities"
                                 ? minera::OcelStatisticsVariant::kObjectTypeActivities
                                 : minera::OcelStatisticsVariant::kObjectsOtCount;
        std::cout << minera::serialize_ocel_statistics_json(log, variant) << '\n';
      }
      return 0;
    }
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
    if (!enrich.empty()) {
      const auto enriched = minera::enrich_ocel(log, enrichment_options);
      std::cout << minera::serialize_ocel_enrichment_json(
                       enriched, enrichment_options)
                << '\n';
      return 0;
    }
    if (ocpn_playout) {
      ocpn_playout_options.objects_unique_per_trace = ocpn_unique_objects;
      const auto result = minera::ocpn_extensive_playout_result(
          minera::discover_ocpn(log), ocpn_playout_options);
      std::cout << minera::serialize_ocpn_playout_json(
                       result, ocpn_playout_options.event_binding)
                << '\n';
      return 0;
    }
    if (oc_causal_net_playout) {
      ocpn_playout_options.objects_unique_per_trace = ocpn_unique_objects;
      const auto model = minera::discover_oc_causal_net(log, ocpn_options);
      const auto result = minera::oc_causal_net_extensive_playout_result(
          model, ocpn_playout_options);
      std::cout << minera::serialize_oc_causal_net_playout_json(
                       result, ocpn_playout_options.event_binding)
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
      } else if (describe == "tuples") {
        std::cout << minera::serialize_ocel_description_json(
                         minera::describe_ocel_tuples(log, options))
                  << '\n';
      } else {
        const auto variant = describe == "variant1"
                                 ? minera::OcelDescriptionVariant::kVariant1
                                 : minera::OcelDescriptionVariant::kVariant2;
        std::cout << minera::serialize_ocel_description_json(log, variant,
                                                               options)
                  << '\n';
      }
      return 0;
    }
    if (!conformance.empty()) {
      const std::filesystem::path normative_file(normative_path);
      minera::Ocel normative;
      const auto normative_extension = lower_extension(normative_file);
      if (normative_extension == ".csv") {
        if (!normative_csv_objects_path.empty()) {
          normative = minera::import_ocel_csv(
              normative_file,
              std::filesystem::path(normative_csv_objects_path));
        } else {
          normative = minera::import_ocel_csv(normative_file);
        }
      } else if (normative_extension == ".xml" || normative_extension == ".xmlocel") {
        normative = minera::import_ocel_xml(normative_file);
      } else if (normative_extension == ".sqlite" ||
                 normative_extension == ".sqlite3" ||
                 normative_extension == ".db") {
        normative = minera::import_ocel_sqlite(normative_file);
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
    if (format == "sqlite") {
      if (sqlite_output_path.empty())
        throw std::invalid_argument(
            "--format sqlite requires --sqlite-output OUTPUT.sqlite");
      minera::export_ocel_sqlite(log, std::filesystem::path(sqlite_output_path));
    } else if (format == "xml" || format == "xml_standard") {
      minera::OcelXmlExportOptions options;
      options.format = format == "xml_standard"
                           ? minera::OcelXmlFormat::kStandard
                           : minera::OcelXmlFormat::kClassic;
      std::cout << minera::serialize_ocel_xml(log, options);
    } else {
      const auto selected = format == "standard" ? minera::OcelJsonFormat::kStandard : minera::OcelJsonFormat::kClassic;
      std::cout << minera::serialize_ocel_json(log, selected) << '\n';
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "minera_ocel: " << error.what() << '\n';
    return 1;
  }
}
