#include <fstream>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

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
#include "minera/xes_export.hpp"

int run_ocel(int argc, char** argv) {
  try {
    if (argc < 2) {
      std::cerr << "OCEL CSV controls: --format csv [--ocel-csv-output EVENTS.csv] [--ocel-csv-objects-output OBJECTS.csv] "
                   "--ocel-csv-event-id-column NAME --ocel-csv-activity-column NAME "
                   "--ocel-csv-timestamp-column NAME --ocel-csv-object-type-prefix PREFIX "
                   "--ocel-csv-object-id-column NAME --ocel-csv-object-type-column NAME "
                   "--ocel-csv-delimiter CHAR --ocel-csv-trim-fields BOOL "
                   "--ocel-csv-sort-events-by-timestamp BOOL --ocel-csv-reject-unsupported BOOL "
                   "--ocel-csv-emit-objects BOOL --ocel-csv-allow-lossy BOOL\n";
      std::cerr << "usage: minera_ocel INPUT.json|INPUT.csv|INPUT.xml|INPUT.sqlite [--csv] [--csv-objects OBJECTS.csv] [--xml] [--sqlite] [--sqlite-output OUTPUT.sqlite] [--format classic|standard|csv|xml|xml_standard|sqlite] [--flatten-object-type TYPE] [--flatten-event-activity-attribute NAME] [--flatten-event-timestamp-attribute NAME] [--ocel-csv-output EVENTS.csv] [--ocel-csv-objects-output OBJECTS.csv] [--ocel-csv-event-id-column NAME] [--ocel-csv-activity-column NAME] [--ocel-csv-timestamp-column NAME] [--ocel-csv-object-type-prefix PREFIX] [--ocel-csv-object-id-column NAME] [--ocel-csv-object-type-column NAME] [--ocel-csv-delimiter CHAR] [--ocel-csv-trim-fields BOOL] [--ocel-csv-sort-events-by-timestamp BOOL] [--ocel-csv-reject-unsupported BOOL] [--ocel-csv-emit-objects BOOL] [--ocel-csv-allow-lossy BOOL] [--filter-object-count TYPE:N ...] [--filter-object-types TYPE1,TYPE2] [--filter-object-ids ID1,ID2] [--filter-object-level N] [--filter-object-negative] [--filter-start-events TYPE] [--filter-end-events TYPE] [--filter-activities-object-type TYPE] [--filter-cc-object ID] [--filter-cc-length MIN:MAX] [--filter-cc-object-type TYPE] [--filter-cc-object-type-negative] [--filter-cc-activity ACTIVITY] [--statistics object_type_activities|objects_ot_count|activity_associations|object_type_activity_associations|edge_associations] [--statistics-prefilter none|start|end] [--statistics-business-hours] [--statistics-business-hour-slots START-END,...] [--describe text|tuples|variant1|variant2] [--no-description-timestamps] [--features events|objects|events_objects] [--features-name-style native|compatibility] [--features-interaction-graph-source event_cooccurrence|event_and_o2o] [--features-object-neighbor-graph interaction|cobirth|codeath] [--features-filter-per-type TYPE] [--features-related-object-counts|--features-no-related-object-counts] [--features-related-object-counts-by-type|--features-no-related-object-counts-by-type] [--features-lifecycle-start|--features-no-lifecycle-start] [--features-lifecycle-end|--features-no-lifecycle-end] [--features-new-interactions|--features-no-new-interactions] [--features-interactions-by-type|--features-no-interactions-by-type] [--features-no-degree-centrality] [--features-all-object-features|--features-no-all-object-features] [--features-general-interaction|--features-no-general-interaction] [--features-general-descendants|--features-no-general-descendants] [--features-general-inheritance|--features-no-general-inheritance] [--features-cobirth|--features-no-cobirth] [--features-codeath|--features-no-codeath] [--features-related-events] [--features-related-activities] [--features-related-objects] [--features-object-neighbor-features] [--features-event-pointwise|--features-no-event-pointwise] [--features-object-pointwise|--features-no-object-pointwise] [--features-no-prefix] [--features-no-prefix-length] [--features-no-prefix-timediff] [--features-no-prefix-activities] [--features-no-lifecycle] [--features-no-lifecycle-length] [--features-no-lifecycle-unique-activities] [--features-no-lifecycle-duration] [--features-no-lifecycle-activities] [--features-lifecycle-paths|--features-no-lifecycle-paths] [--features-lifecycle-wip|--features-no-lifecycle-wip] [--features-max N] [--discover etot|ocdfg|ocpn|otg|saw] [--ocpn-annotation annotated|none] [--ocdfg-activity-attribute NAME] [--ocdfg-object-type-attribute NAME] [--saw-activity-attribute NAME] [--saw-object-type-attribute NAME] [--ocpn-activity-attribute NAME] [--ocpn-object-type-attribute NAME] [--ocpn-diagnostics-with-token-based-replay] [--enrich interaction,descendants,inheritance,cobirth,codeath] [--graph interaction|descendants|inheritance|cobirth|codeath|heterogeneous] [--graph-include-df BOOL] [--graph-include-object-changes BOOL] [--enrich-no-lifecycle] [--split connected_components|ancestors_descendants] [--split-object-type TYPE] [--split-max-objects N] [--split-max-degree-centrality F] [--ocpn-playout --ocpn-max-bindings N --ocpn-branching-transitions F --ocpn-branching-bindings F --ocpn-seed N --ocpn-max-traces N --ocpn-max-states N --ocpn-objects TYPE:ID1,ID2 --ocpn-initial-marking TYPE:T0,T1 --ocpn-final-marking TYPE:T0,T1 --ocpn-unique-objects --ocpn-event-binding independent|shared_activity|synchronized --ocpn-exists-trace --ocpn-return-sequences --ocpn-return-traces] [--oc-causal-net --ocpn-max-bindings N --ocpn-branching-transitions F --ocpn-branching-bindings F --ocpn-branching-activities F --ocpn-seed N --ocpn-max-traces N --ocpn-max-states N --ocpn-objects TYPE:ID1,ID2 --ocpn-initial-marking TYPE:T0,T1 --ocpn-final-marking TYPE:T0,T1 --ocpn-unique-objects --ocpn-event-binding independent|shared_activity|synchronized --ocpn-exists-trace --ocpn-return-sequences --ocpn-return-traces] [--conformance etot|ocdfg|otg --real ocel|graph --real-graph-kind etot|ocdfg|otg --normative OCEL.json|GRAPH.json --otg-theta-interaction F --otg-theta-descendants F --otg-theta-inheritance F --otg-theta-cobirth F --otg-theta-codeath F]\n";
      std::cerr << "prefix feature controls: --features-prefix-length | --features-no-prefix-length | --features-prefix-timediff | --features-no-prefix-timediff | --features-prefix-activities | --features-no-prefix-activities\n";
      std::cerr << "OCPN discovery policy: --ocpn-disable-strict-sequence-cut BOOL --ocpn-disable-fallthroughs BOOL (true, false, 1, or 0; default false; applies to discovery and model discovery before playout)\n";
      std::cerr << "lifecycle feature controls: --features-lifecycle-paths | --features-no-lifecycle-paths\n";
      std::cerr << "flatten options: --flatten-object-type TYPE [--flatten-event-activity-attribute NAME] [--flatten-event-timestamp-attribute NAME]\n";
      return 2;
    }
    std::string format = "classic", describe, features, discover, enrich, graph, split, conformance,
                        statistics, statistics_prefilter = "none",
                        split_object_type, flatten_object_type, flatten_event_activity,
                        flatten_event_timestamp, normative_path, csv_objects_path,
                        normative_csv_objects_path, csv_output_path,
                        csv_objects_output_path, real_mode = "ocel",
                        real_graph_kind;
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
    bool flatten_object_type_set = false;
    bool flatten_event_activity_set = false;
    bool flatten_event_timestamp_set = false;
    std::string sqlite_output_path;
    minera::OcelCsvImportOptions csv_import_options;
    minera::OcelCsvExportOptions csv_export_options;
    minera::Ocel log;
    bool ocpn_playout = false;
    bool oc_causal_net_playout = false;
    bool ocpn_unique_objects = false;
    bool ocpn_event_binding_used = false;
    bool ocpn_activity_branching_used = false;
    minera::OcpnPlayoutOptions ocpn_playout_options;
    minera::OcelFeatureOptions feature_options;
    bool lifecycle_object_control_used = false;
    bool prefix_control_used = false;
    bool degree_centrality_control_used = false;
    bool all_object_control_used = false;
    bool object_graph_control_used = false;
    bool interaction_graph_source_used = false;
    bool object_neighbor_graph_control_used = false;
    bool related_object_control_used = false;
    bool event_control_used = false;
    bool pointwise_control_used = false;
    minera::OcelEnrichmentOptions enrichment_options;
    bool enrich_no_lifecycle = false;
    minera::OcelHeterogeneousGraphOptions graph_options;
    bool graph_include_df_used = false;
    bool graph_include_object_changes_used = false;
    std::size_t split_max_objects = std::numeric_limits<std::size_t>::max();
    std::optional<double> split_max_degree_centrality;
    bool compute_edges_performance = true;
    bool statistics_business_hours = false;
    std::vector<minera::BusinessHourSlot> statistics_business_hour_slots;
    bool include_description_timestamps = true;
    double alpha = 1.0, beta = 1.0, gamma = 1.0, delta = 1.0;
    double etot_theta = 0.1, otg_theta = 0.2;
    std::map<std::string, double> otg_theta_by_relation;
    double ocdfg_theta_activity = 0.0, ocdfg_theta_flow = 0.0;
    minera::OcpnDiscoveryOptions ocpn_options;
    minera::OcdfgDiscoveryOptions ocdfg_options;
    minera::SawNetDiscoveryOptions saw_options;
    const auto number = [](const std::string& value, const char* option) {
      char* end = nullptr;
      const double result = std::strtod(value.c_str(), &end);
      if (end != value.c_str() + value.size() || !std::isfinite(result) ||
          result < 0.0)
        throw std::invalid_argument(std::string(option) + " expects a finite non-negative number");
      return result;
    };
    const auto conformance_number = [](const std::string& value, const char* option) {
      char* end = nullptr;
      const double result = std::strtod(value.c_str(), &end);
      if (end == value.c_str() || end != value.c_str() + value.size())
        throw std::invalid_argument(std::string(option) + " expects a number");
      return result;
    };
    const auto boolean = [](const std::string& value, const char* option) {
      if (value == "true" || value == "1" || value == "yes" || value == "on")
        return true;
      if (value == "false" || value == "0" || value == "no" || value == "off")
        return false;
      throw std::invalid_argument(std::string(option) +
                                  " expects true or false");
    };
    const auto required_value = [&](int& index, const char* option) {
      if (index + 1 >= argc || std::string(argv[index + 1]).empty())
        throw std::invalid_argument(std::string(option) + " expects a value");
      return std::string(argv[++index]);
    };
    const auto all_whitespace = [](const std::string& value) {
      return !value.empty() && std::all_of(
          value.begin(), value.end(), [](unsigned char character) {
            return std::isspace(character) != 0;
          });
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
    const auto feature_attribute_list = [](const std::string& value) {
      std::vector<std::string> result;
      std::string item;
      for (std::size_t i = 0; i < value.size(); ++i) {
        const auto ch = value[i];
        if (ch == '\\' && i + 1 < value.size() &&
            std::string_view(",\\:;|").find(value[i + 1]) != std::string_view::npos) {
          item.push_back(value[++i]);
        } else if (ch == ',') {
          if (!item.empty()) result.push_back(std::move(item));
          item.clear();
        } else {
          item.push_back(ch);
        }
      }
      if (!item.empty()) result.push_back(std::move(item));
      return result;
    };
    const auto object_graph_variant = [](const std::string& value) {
      if (value == "interaction" || value == "object_interaction_graph")
        return minera::OcelEnrichmentGraph::kInteraction;
      if (value == "descendants" || value == "object_descendants_graph")
        return minera::OcelEnrichmentGraph::kDescendants;
      if (value == "inheritance" || value == "object_inheritance_graph")
        return minera::OcelEnrichmentGraph::kInheritance;
      if (value == "cobirth" || value == "object_cobirth_graph")
        return minera::OcelEnrichmentGraph::kCobirth;
      if (value == "codeath" || value == "object_codeath_graph")
        return minera::OcelEnrichmentGraph::kCodeath;
      throw std::invalid_argument(
          "graph expects interaction, descendants, inheritance, cobirth, or codeath");
    };
    const auto heterogeneous_graph = [](const std::string& value) {
      return value == "heterogeneous" || value == "HETEROGENEOUS" ||
             value == "event_object" || value == "ocel_graph";
    };
    const auto marker = [](const std::string& value, const char* option) {
      const auto separator = value.find(':');
      if (separator == std::string::npos || separator == 0 ||
          separator + 1 >= value.size())
        throw std::invalid_argument(std::string(option) +
                                    " expects TYPE:T0,T1,...");
      const auto type = value.substr(0, separator);
      std::vector<std::uint32_t> tokens;
      std::size_t begin = separator + 1;
      while (begin <= value.size()) {
        const auto end = value.find(',', begin);
        const auto token = value.substr(
            begin, end == std::string::npos ? std::string::npos : end - begin);
        if (token.empty())
          throw std::invalid_argument(std::string(option) +
                                      " contains an empty token");
        errno = 0;
        char* parsed_end = nullptr;
        const auto parsed = std::strtoull(token.c_str(), &parsed_end, 10);
        if (parsed_end != token.c_str() + token.size() || errno == ERANGE ||
            parsed > std::numeric_limits<std::uint32_t>::max())
          throw std::invalid_argument(std::string(option) +
                                      " expects uint32 token counts");
        tokens.push_back(static_cast<std::uint32_t>(parsed));
        if (end == std::string::npos) break;
        begin = end + 1;
      }
      return std::pair<std::string, std::vector<std::uint32_t>>{
          type, std::move(tokens)};
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
      } else if (option == "--ocel-csv-output") {
        csv_output_path = required_value(i, "--ocel-csv-output");
        format = "csv";
      } else if (option == "--ocel-csv-objects-output") {
        csv_objects_output_path = required_value(i, "--ocel-csv-objects-output");
        format = "csv";
      } else if (option == "--ocel-csv-event-id-column" ||
                 option == "--ocel-csv-activity-column" ||
                 option == "--ocel-csv-timestamp-column" ||
                 option == "--ocel-csv-object-type-prefix" ||
                 option == "--ocel-csv-object-id-column" ||
                 option == "--ocel-csv-object-type-column") {
        const auto value = required_value(i, option.c_str());
        if (option == "--ocel-csv-event-id-column") {
          csv_import_options.event_id_column = value;
          csv_export_options.event_id_column = value;
        } else if (option == "--ocel-csv-activity-column") {
          csv_import_options.activity_column = value;
          csv_export_options.activity_column = value;
        } else if (option == "--ocel-csv-timestamp-column") {
          csv_import_options.timestamp_column = value;
          csv_export_options.timestamp_column = value;
        } else if (option == "--ocel-csv-object-type-prefix") {
          csv_import_options.object_type_prefix = value;
          csv_export_options.object_type_prefix = value;
        } else if (option == "--ocel-csv-object-id-column") {
          csv_import_options.object_id_column = value;
          csv_export_options.object_id_column = value;
        } else {
          csv_import_options.object_type_column = value;
          csv_export_options.object_type_column = value;
        }
      } else if (option == "--ocel-csv-delimiter") {
        const auto value = required_value(i, "--ocel-csv-delimiter");
        if (value.size() != 1 || value.front() == '"' ||
            value.front() == '\r' || value.front() == '\n')
          throw std::invalid_argument(
              "--ocel-csv-delimiter expects one character other than quote or newline");
        csv_import_options.delimiter = value.front();
        csv_export_options.delimiter = value.front();
      } else if (option == "--ocel-csv-trim-fields" ||
                 option == "--ocel-csv-sort-events-by-timestamp" ||
                 option == "--ocel-csv-reject-unsupported" ||
                 option == "--ocel-csv-emit-objects" ||
                 option == "--ocel-csv-allow-lossy") {
        const auto value = required_value(i, option.c_str());
        const auto parsed = boolean(value, option.c_str());
        if (option == "--ocel-csv-trim-fields")
          csv_import_options.trim_fields = parsed;
        else if (option == "--ocel-csv-sort-events-by-timestamp")
          csv_import_options.sort_events_by_timestamp = parsed;
        else if (option == "--ocel-csv-reject-unsupported")
          csv_import_options.reject_unsupported = parsed;
        else if (option == "--ocel-csv-emit-objects")
          csv_export_options.emit_objects = parsed;
        else
          csv_export_options.allow_lossy = parsed;
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
           option == "--discover" || option == "--graph" ||
           option == "--split") && i + 1 < argc) {
        auto& target = option == "--format" ? format
                      : option == "--features" ? features
                      : option == "--discover" ? discover
                      : option == "--graph" ? graph
                                             : split;
        target = argv[++i];
      } else if (option == "--graph-include-df" && i + 1 < argc) {
        graph_options.include_df = boolean(argv[++i], "--graph-include-df");
        graph_include_df_used = true;
      } else if (option == "--graph-include-object-changes" && i + 1 < argc) {
        graph_options.include_object_changes =
            boolean(argv[++i], "--graph-include-object-changes");
        graph_include_object_changes_used = true;
      } else if (option == "--features-max" && i + 1 < argc) {
        const auto parsed = number(argv[++i], "--features-max");
        if (parsed > static_cast<double>(std::numeric_limits<std::size_t>::max()) ||
            std::floor(parsed) != parsed || parsed == 0.0)
          throw std::invalid_argument("--features-max expects a positive integer");
        feature_options.max_features = static_cast<std::size_t>(parsed);
      } else if (option == "--features-name-style" && i + 1 < argc) {
        const auto style = std::string(argv[++i]);
        if (style == "native") {
          feature_options.feature_name_style =
              minera::OcelFeatureNameStyle::kNative;
        } else if (style == "compatibility") {
          feature_options.feature_name_style =
              minera::OcelFeatureNameStyle::kCompatibility;
        } else {
          throw std::invalid_argument(
              "--features-name-style expects native or compatibility");
        }
      } else if (option == "--features-filter-per-type" && i + 1 < argc) {
        feature_options.filter_per_type = argv[++i];
      } else if (option == "--features-interaction-graph-source" && i + 1 < argc) {
        const auto source = std::string(argv[++i]);
        if (source == "event_cooccurrence") {
          feature_options.interaction_graph_source =
              minera::OcelInteractionGraphSource::kEventCooccurrence;
        } else if (source == "event_and_o2o") {
          feature_options.interaction_graph_source =
              minera::OcelInteractionGraphSource::kEventAndO2O;
        } else {
          throw std::invalid_argument(
              "--features-interaction-graph-source expects event_cooccurrence or event_and_o2o");
        }
        interaction_graph_source_used = true;
      } else if (option == "--features-object-neighbor-graph") {
        const auto graph_name = required_value(i, "--features-object-neighbor-graph");
        if (graph_name == "interaction") {
          feature_options.object_neighbor_graph =
              minera::OcelObjectNeighborGraph::kInteraction;
        } else if (graph_name == "cobirth") {
          feature_options.object_neighbor_graph =
              minera::OcelObjectNeighborGraph::kCobirth;
        } else if (graph_name == "codeath") {
          feature_options.object_neighbor_graph =
              minera::OcelObjectNeighborGraph::kCodeath;
        } else {
          throw std::invalid_argument(
              "--features-object-neighbor-graph expects interaction, cobirth, or codeath");
        }
        object_neighbor_graph_control_used = true;
      } else if (option == "--features-related-object-counts") {
        feature_options.include_event_num_rel_objs_features = true;
        event_control_used = true;
      } else if (option == "--features-no-related-object-counts") {
        feature_options.include_event_num_rel_objs_features = false;
        event_control_used = true;
      } else if (option == "--features-related-object-counts-by-type") {
        feature_options.include_event_num_rel_objs_type_features = true;
        event_control_used = true;
      } else if (option == "--features-no-related-object-counts-by-type") {
        feature_options.include_event_num_rel_objs_type_features = false;
        event_control_used = true;
      } else if (option == "--features-lifecycle-start") {
        feature_options.include_event_start_ot_features = true;
        event_control_used = true;
      } else if (option == "--features-no-lifecycle-start") {
        feature_options.include_event_start_ot_features = false;
        event_control_used = true;
      } else if (option == "--features-lifecycle-end") {
        feature_options.include_event_end_ot_features = true;
        event_control_used = true;
      } else if (option == "--features-no-lifecycle-end") {
        feature_options.include_event_end_ot_features = false;
        event_control_used = true;
      } else if (option == "--features-new-interactions") {
        feature_options.include_new_interactions_features = true;
        event_control_used = true;
      } else if (option == "--features-no-new-interactions") {
        feature_options.include_new_interactions_features = false;
        event_control_used = true;
      } else if (option == "--features-interactions-by-type" ||
                 option == "--features-no-interactions-by-type") {
        feature_options.include_interaction_by_type_features =
            option == "--features-interactions-by-type";
        object_graph_control_used = true;
      } else if (option == "--features-no-degree-centrality") {
        feature_options.include_degree_centrality_features = false;
        degree_centrality_control_used = true;
      } else if (option == "--features-all-object-features" ||
                 option == "--features-no-all-object-features") {
        feature_options.include_all_object_features =
            option == "--features-all-object-features";
        all_object_control_used = true;
      } else if (option == "--features-general-interaction" ||
                 option == "--features-no-general-interaction") {
        feature_options.include_general_interaction_features =
            option == "--features-general-interaction";
        object_graph_control_used = true;
      } else if (option == "--features-general-descendants" ||
                 option == "--features-no-general-descendants") {
        feature_options.include_general_descendants_features =
            option == "--features-general-descendants";
        object_graph_control_used = true;
      } else if (option == "--features-general-inheritance" ||
                 option == "--features-no-general-inheritance") {
        feature_options.include_general_inheritance_features =
            option == "--features-general-inheritance";
        object_graph_control_used = true;
      } else if (option == "--features-cobirth" ||
                 option == "--features-no-cobirth") {
        feature_options.include_cobirth_features = option == "--features-cobirth";
        object_graph_control_used = true;
      } else if (option == "--features-codeath" ||
                 option == "--features-no-codeath") {
        feature_options.include_codeath_features = option == "--features-codeath";
        object_graph_control_used = true;
      } else if (option == "--features-related-events") {
        feature_options.include_related_events_features = true;
      } else if (option == "--features-related-activities") {
        feature_options.include_related_activities_features = true;
      } else if (option == "--features-related-objects") {
        feature_options.include_related_objects_features = true;
        related_object_control_used = true;
      } else if (option == "--features-object-neighbor-features") {
        feature_options.include_obj_con_in_graph_features = true;
      } else if (option == "--features-event-pointwise" ||
                 option == "--features-no-event-pointwise") {
        feature_options.include_event_pointwise_features =
            option == "--features-event-pointwise";
        pointwise_control_used = true;
      } else if (option == "--features-object-pointwise" ||
                 option == "--features-no-object-pointwise") {
        feature_options.include_object_pointwise_features =
            option == "--features-object-pointwise";
        pointwise_control_used = true;
      } else if (option == "--features-no-prefix") {
        feature_options.include_prefix_features = false;
        prefix_control_used = true;
      } else if (option == "--features-prefix-length" ||
                 option == "--features-no-prefix-length" ||
                 option == "--features-prefix-timediff" ||
                 option == "--features-no-prefix-timediff" ||
                 option == "--features-prefix-activities" ||
                 option == "--features-no-prefix-activities") {
        const bool enabled = option.rfind("--features-no-", 0) != 0;
        if (option.ends_with("prefix-length"))
          feature_options.include_prefix_length_features = enabled;
        else if (option.ends_with("prefix-timediff"))
          feature_options.include_prefix_timediff_features = enabled;
        else
          feature_options.include_prefix_one_hot_features = enabled;
        prefix_control_used = true;
      } else if ((option == "--features-numeric-events" ||
                  option == "--features-string-events" ||
                  option == "--features-numeric-objects" ||
                  option == "--features-string-objects") && i + 1 < argc) {
        const auto values = feature_attribute_list(argv[++i]);
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
        lifecycle_object_control_used = true;
      } else if (option == "--features-no-lifecycle-length") {
        feature_options.include_lifecycle_length_features = false;
        lifecycle_object_control_used = true;
      } else if (option == "--features-no-lifecycle-unique-activities") {
        feature_options.include_lifecycle_unique_activity_features = false;
        lifecycle_object_control_used = true;
      } else if (option == "--features-no-lifecycle-duration") {
        feature_options.include_lifecycle_duration_features = false;
        lifecycle_object_control_used = true;
      } else if (option == "--features-no-lifecycle-activities") {
        feature_options.include_lifecycle_activity_features = false;
        lifecycle_object_control_used = true;
      } else if (option == "--features-lifecycle-paths") {
        feature_options.include_lifecycle_path_features = true;
        lifecycle_object_control_used = true;
      } else if (option == "--features-no-lifecycle-paths") {
        feature_options.include_lifecycle_path_features = false;
        lifecycle_object_control_used = true;
      } else if (option == "--features-lifecycle-wip") {
        feature_options.include_lifecycle_wip_features = true;
        lifecycle_object_control_used = true;
      } else if (option == "--features-no-lifecycle-wip") {
        feature_options.include_lifecycle_wip_features = false;
        lifecycle_object_control_used = true;
      } else if (option == "--describe" && i + 1 < argc) {
        describe = argv[++i];
      } else if (option == "--enrich" && i + 1 < argc) {
        if (!enrich.empty())
          throw std::invalid_argument("--enrich may be specified once");
        enrich = argv[++i];
      } else if (option == "--enrich-no-lifecycle") {
        enrich_no_lifecycle = true;
      } else if (option == "--flatten-object-type") {
        if (flatten_object_type_set)
          throw std::invalid_argument(
              "--flatten-object-type may be specified once");
        flatten_object_type = required_value(i, "--flatten-object-type");
        if (all_whitespace(flatten_object_type))
          throw std::invalid_argument(
              "--flatten-object-type expects a non-blank value");
        flatten_object_type_set = true;
      } else if (option == "--flatten-event-activity-attribute" ||
                 option == "--flatten-event-activity") {
        if (flatten_event_activity_set)
          throw std::invalid_argument(
              "event activity selector may be specified once");
        flatten_event_activity = required_value(
            i, "--flatten-event-activity-attribute");
        if (flatten_event_activity.empty() ||
            all_whitespace(flatten_event_activity))
          throw std::invalid_argument(
              "--flatten-event-activity-attribute expects a non-blank value");
        flatten_event_activity_set = true;
      } else if (option == "--flatten-event-timestamp-attribute" ||
                 option == "--flatten-event-timestamp") {
        if (flatten_event_timestamp_set)
          throw std::invalid_argument(
              "event timestamp selector may be specified once");
        flatten_event_timestamp = required_value(
            i, "--flatten-event-timestamp-attribute");
        if (flatten_event_timestamp.empty() ||
            all_whitespace(flatten_event_timestamp))
          throw std::invalid_argument(
              "--flatten-event-timestamp-attribute expects a non-blank value");
        flatten_event_timestamp_set = true;
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
      } else if (option == "--ocpn-return-traces") {
        ocpn_playout_options.return_traces = true;
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
                  option == "--ocpn-branching-activities" ||
                  option == "--ocpn-seed" || option == "--ocpn-max-traces" ||
                  option == "--ocpn-max-states") && i + 1 < argc) {
        const auto parsed = number(argv[++i], option.c_str());
        const bool fractional = option == "--ocpn-branching-transitions" ||
                                option == "--ocpn-branching-bindings" ||
                                option == "--ocpn-branching-activities";
        if ((!fractional &&
             (parsed > static_cast<double>(std::numeric_limits<std::size_t>::max()) ||
              std::floor(parsed) != parsed)) ||
            (fractional && parsed > 100.0))
          throw std::invalid_argument(
              option + (fractional ? " expects a finite number in [0,100]"
                                   : " expects a non-negative integer"));
        const auto value = static_cast<std::size_t>(parsed);
        if (option == "--ocpn-max-bindings")
          ocpn_playout_options.max_bindings_per_activity = value;
        else if (option == "--ocpn-branching-transitions")
          ocpn_playout_options.branching_factor_transitions = parsed;
        else if (option == "--ocpn-branching-bindings")
          ocpn_playout_options.branching_factor_bindings = parsed;
        else if (option == "--ocpn-branching-activities") {
          ocpn_activity_branching_used = true;
          ocpn_playout_options.branching_factor_activities = parsed;
        }
        else if (option == "--ocpn-seed")
          ocpn_playout_options.seed = static_cast<std::uint64_t>(value);
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
      } else if ((option == "--ocpn-initial-marking" ||
                  option == "--ocpn-final-marking") && i + 1 < argc) {
        const auto parsed = marker(argv[++i], option.c_str());
        auto& target = option == "--ocpn-initial-marking"
                           ? ocpn_playout_options.initial_markings
                           : ocpn_playout_options.final_markings;
        if (target.contains(parsed.first))
          throw std::invalid_argument(std::string(option) +
                                      " may be specified once per type");
        target.emplace(parsed.first, std::move(parsed.second));
      } else if ((option == "--conformance" || option == "--normative") &&
                 i + 1 < argc) {
        (option == "--conformance" ? conformance : normative_path) = argv[++i];
      } else if (option == "--real" && i + 1 < argc) {
        real_mode = argv[++i];
      } else if (option == "--real-graph") {
        real_mode = "graph";
      } else if (option == "--real-graph-kind" && i + 1 < argc) {
        real_graph_kind = argv[++i];
      } else if (option == "--alpha" && i + 1 < argc) {
        alpha = conformance_number(argv[++i], "--alpha");
      } else if (option == "--beta" && i + 1 < argc) {
        beta = conformance_number(argv[++i], "--beta");
      } else if (option == "--gamma" && i + 1 < argc) {
        gamma = conformance_number(argv[++i], "--gamma");
      } else if (option == "--delta" && i + 1 < argc) {
        delta = conformance_number(argv[++i], "--delta");
      } else if (option == "--etot-theta" && i + 1 < argc) {
        etot_theta = conformance_number(argv[++i], "--etot-theta");
      } else if (option == "--otg-theta" && i + 1 < argc) {
        otg_theta = conformance_number(argv[++i], "--otg-theta");
      } else if ((option == "--otg-theta-interaction" ||
                  option == "--otg-theta-descendants" ||
                  option == "--otg-theta-inheritance" ||
                  option == "--otg-theta-cobirth" ||
                  option == "--otg-theta-codeath") && i + 1 < argc) {
        const auto relation =
            option == "--otg-theta-interaction"
                ? "object_interaction"
                : option == "--otg-theta-descendants"
                    ? "object_descendants"
                    : option == "--otg-theta-inheritance"
                        ? "object_inheritance"
                        : option == "--otg-theta-cobirth"
                            ? "object_cobirth"
                            : "object_codeath";
        otg_theta_by_relation[relation] = conformance_number(argv[++i], option.c_str());
      } else if (option == "--ocdfg-theta-activity" && i + 1 < argc) {
        ocdfg_theta_activity = conformance_number(argv[++i], "--ocdfg-theta-activity");
      } else if (option == "--ocdfg-theta-flow" && i + 1 < argc) {
        ocdfg_theta_flow = conformance_number(argv[++i], "--ocdfg-theta-flow");
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
      } else if ((option == "--ocpn-activity-attribute" ||
                  option == "--ocpn-object-type-attribute") &&
                 i + 1 < argc) {
        const auto value = required_value(i, option.c_str());
        if (option == "--ocpn-activity-attribute")
          ocpn_options.activity_attribute = value;
        else
          ocpn_options.object_type_attribute = value;
      } else if ((option == "--ocdfg-activity-attribute" ||
                  option == "--ocdfg-object-type-attribute") &&
                 i + 1 < argc) {
        const auto value = required_value(i, option.c_str());
        if (option == "--ocdfg-activity-attribute")
          ocdfg_options.activity_attribute = value;
        else
          ocdfg_options.object_type_attribute = value;
      } else if ((option == "--saw-activity-attribute" ||
                  option == "--saw-object-type-attribute") &&
                 i + 1 < argc) {
        const auto value = required_value(i, option.c_str());
        if (option == "--saw-activity-attribute")
          saw_options.activity_attribute = value;
        else
          saw_options.object_type_attribute = value;
      } else if (option == "--ocpn-disable-strict-sequence-cut" ||
                 option == "--ocpn-disable-fallthroughs") {
        const auto value = required_value(i, option.c_str());
        bool parsed = false;
        if (value == "true" || value == "1") {
          parsed = true;
        } else if (value != "false" && value != "0") {
          throw std::invalid_argument(option +
                                      " expects true, false, 1, or 0");
        }
        if (option == "--ocpn-disable-strict-sequence-cut")
          ocpn_options.disable_strict_sequence_cut = parsed;
        else
          ocpn_options.disable_fallthroughs = parsed;
      } else if (option == "--ocpn-diagnostics-with-token-based-replay") {
        ocpn_options.diagnostics_with_token_based_replay = true;
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
    if (format != "classic" && format != "standard" && format != "csv" && format != "xml" &&
        format != "xml_standard" && format != "sqlite") {
      throw std::invalid_argument(
          "format must be classic, standard, csv, xml, xml_standard, or sqlite");
    }
    if (format == "sqlite" && sqlite_output_path.empty())
      throw std::invalid_argument(
          "--format sqlite requires --sqlite-output OUTPUT.sqlite");
    if (flatten_object_type_set && format != "classic")
      throw std::invalid_argument(
          "--flatten-object-type emits XES and requires the default format");
    if ((flatten_event_activity_set || flatten_event_timestamp_set) &&
        !flatten_object_type_set)
      throw std::invalid_argument(
          "flatten event attribute selectors require --flatten-object-type");
    if (!sqlite_output_path.empty() && format != "sqlite")
      throw std::invalid_argument(
          "--sqlite-output requires --format sqlite");
    if (!csv_output_path.empty() && format != "csv")
      throw std::invalid_argument(
          "--ocel-csv-output requires --format csv");
    if (!csv_objects_output_path.empty() && format != "csv")
      throw std::invalid_argument(
          "--ocel-csv-objects-output requires --format csv");
    if (!csv_objects_output_path.empty() && csv_output_path.empty())
      throw std::invalid_argument(
          "--ocel-csv-objects-output requires --ocel-csv-output");
    if ((graph_include_df_used || graph_include_object_changes_used) &&
        graph.empty())
      throw std::invalid_argument(
          "graph option controls require --graph");
    if ((graph_include_df_used || graph_include_object_changes_used) &&
        !graph.empty() && !heterogeneous_graph(graph))
      throw std::invalid_argument(
          "graph option controls require --graph heterogeneous");
    if (!graph.empty() && format != "classic")
      throw std::invalid_argument(
          "--graph emits a graph JSON envelope and requires the default format");
    if (!graph.empty() && !heterogeneous_graph(graph))
      (void)object_graph_variant(graph);
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
        if (value == "none" && values.size() == 1) {
          continue;
        } else if (value == "all") {
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
              "--enrich expects interaction, descendants, inheritance, cobirth, codeath, all, or none alone");
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
    if (feature_options.feature_name_style ==
            minera::OcelFeatureNameStyle::kCompatibility &&
        features == "events_objects")
      throw std::invalid_argument(
          "compatibility feature names apply only to events or objects variants");
    if (feature_options.filter_per_type && features.empty()) {
      throw std::invalid_argument(
          "--features-filter-per-type requires --features objects");
    }
    if (feature_options.filter_per_type && !features.empty() &&
        features != "objects") {
      throw std::invalid_argument(
          "--features-filter-per-type applies only to --features objects");
    }
    const auto related_object_event_context =
        related_object_control_used && features == "events";
    if (object_graph_control_used && features.empty() &&
        !related_object_event_context)
      throw std::invalid_argument(
          "object graph feature controls require --features objects");
    if (object_graph_control_used && !features.empty() &&
        features != "objects" && !related_object_event_context)
      throw std::invalid_argument(
          "object graph feature controls apply only to --features objects");
    if (interaction_graph_source_used && features.empty() &&
        !related_object_event_context)
      throw std::invalid_argument(
          "interaction graph source requires --features objects");
    if (interaction_graph_source_used && !features.empty() &&
        features != "objects" && !related_object_event_context)
      throw std::invalid_argument(
          "interaction graph source applies only to --features objects");
    if (object_neighbor_graph_control_used && features.empty() &&
        !related_object_event_context)
      throw std::invalid_argument(
          "neighbor graph selection requires --features objects");
    if (object_neighbor_graph_control_used && !features.empty() &&
        features != "objects" && !related_object_event_context)
      throw std::invalid_argument(
          "neighbor graph selection applies only to --features objects");
    if (object_neighbor_graph_control_used &&
        !feature_options.include_obj_con_in_graph_features)
      throw std::invalid_argument(
          "--features-object-neighbor-graph requires --features-object-neighbor-features");
    if (degree_centrality_control_used && features.empty() &&
        !related_object_event_context)
      throw std::invalid_argument(
          "degree centrality controls require --features objects");
    if (degree_centrality_control_used && !features.empty() &&
        features != "objects" && !related_object_event_context)
      throw std::invalid_argument(
          "degree centrality controls apply only to --features objects");
    if (all_object_control_used && features.empty())
      throw std::invalid_argument(
          "all-object feature controls require --features objects");
    if (all_object_control_used && !features.empty() && features != "objects")
      throw std::invalid_argument(
          "all-object feature controls apply only to --features objects");
    if (related_object_control_used && features.empty())
      throw std::invalid_argument(
          "related-object feature controls require --features events");
    if (related_object_control_used && !features.empty() &&
        features != "events")
      throw std::invalid_argument(
          "related-object feature controls apply only to --features events");
    if (event_control_used && features.empty())
      throw std::invalid_argument(
          "independent event feature controls require --features events");
    if (event_control_used && !features.empty() && features != "events")
      throw std::invalid_argument(
          "independent event feature controls apply only to --features events");
    if (pointwise_control_used && features.empty())
      throw std::invalid_argument(
          "pointwise feature controls require --features events_objects");
    if (pointwise_control_used && !features.empty() && features != "events_objects")
      throw std::invalid_argument(
          "pointwise feature controls apply only to --features events_objects");
    const auto object_relation_features =
        feature_options.include_related_events_features ||
        feature_options.include_related_activities_features ||
        feature_options.include_obj_con_in_graph_features;
    if (object_relation_features && features.empty() &&
        !related_object_event_context)
      throw std::invalid_argument(
          "related-event and neighbor feature options require --features objects");
    if (object_relation_features && !features.empty() && features != "objects" &&
        !related_object_event_context)
      throw std::invalid_argument(
          "related-event and neighbor feature options apply only to --features objects");
    if (lifecycle_object_control_used && features.empty() &&
        !related_object_event_context)
      throw std::invalid_argument(
          "lifecycle metric controls require --features objects");
    if (lifecycle_object_control_used && !features.empty() &&
        features != "objects" && !related_object_event_context)
      throw std::invalid_argument(
          "lifecycle metric controls apply only to --features objects");
    if (prefix_control_used && features.empty())
      throw std::invalid_argument(
          "prefix feature controls require --features events_objects");
    if (prefix_control_used && !features.empty() && features != "events_objects")
      throw std::invalid_argument(
          "prefix feature controls apply only to --features events_objects");
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
    if (real_mode != "ocel" && real_mode != "graph") {
      throw std::invalid_argument("real must be ocel or graph");
    }
    if (!real_graph_kind.empty() && real_graph_kind != "etot" &&
        real_graph_kind != "ocdfg" && real_graph_kind != "otg") {
      throw std::invalid_argument(
          "real-graph-kind must be etot, ocdfg, or otg");
    }
    if (real_mode == "graph" && conformance.empty()) {
      throw std::invalid_argument("real graph mode requires --conformance");
    }
    if (real_mode == "ocel" && !real_graph_kind.empty()) {
      throw std::invalid_argument(
          "real-graph-kind requires --real graph");
    }
    if (real_mode == "graph" && !real_graph_kind.empty() &&
        real_graph_kind != conformance) {
      throw std::invalid_argument(
          "real-graph-kind must match --conformance");
    }
    if (!conformance.empty() &&
        (!discover.empty() || !enrich.empty() || !graph.empty() ||
         !split.empty() || !features.empty() || !statistics.empty() ||
         ocpn_playout || oc_causal_net_playout ||
         flatten_object_type_set))
      throw std::invalid_argument(
          "--conformance cannot be combined with discovery, enrichment, graph, or split");
    if ((!discover.empty() && !split.empty()) ||
        (!discover.empty() && !enrich.empty()) ||
        (!split.empty() && !enrich.empty()) ||
        (!graph.empty() && (!discover.empty() || !enrich.empty() ||
                            !split.empty())) ||
        (flatten_object_type_set &&
         (!discover.empty() || !enrich.empty() || !graph.empty() ||
          !split.empty())))
      throw std::invalid_argument(
          "--discover, --enrich, --graph, and --split are mutually exclusive");
    if ((ocpn_playout || oc_causal_net_playout) &&
        (!discover.empty() || !enrich.empty() || !graph.empty() ||
         !split.empty() || flatten_object_type_set))
      throw std::invalid_argument(
          "object-centric playout cannot be combined with discovery, enrichment, graph, or split");
    if (ocpn_event_binding_used && !ocpn_playout && !oc_causal_net_playout)
      throw std::invalid_argument(
          "--ocpn-event-binding requires --ocpn-playout or --oc-causal-net");
    if (ocpn_playout && oc_causal_net_playout)
      throw std::invalid_argument(
          "--ocpn-playout and --oc-causal-net are mutually exclusive");
    if (ocpn_activity_branching_used && !oc_causal_net_playout)
      throw std::invalid_argument(
          "--ocpn-branching-activities requires --oc-causal-net");
    if (!describe.empty() &&
        (!discover.empty() || !enrich.empty() || !graph.empty() ||
         !split.empty() || !features.empty() ||
         !statistics.empty() || !conformance.empty() ||
         ocpn_playout || oc_causal_net_playout ||
         flatten_object_type_set))
      throw std::invalid_argument(
          "--describe cannot be combined with discovery, enrichment, graph, split, or conformance");
    if (!statistics.empty() &&
        (!discover.empty() || !enrich.empty() || !graph.empty() ||
         !split.empty() || !features.empty() ||
         !conformance.empty() || ocpn_playout || oc_causal_net_playout ||
         !describe.empty() || flatten_object_type_set))
      throw std::invalid_argument(
          "--statistics cannot be combined with other OCEL output modes");
    if (!conformance.empty() && normative_path.empty())
      throw std::invalid_argument("--normative is required with --conformance");
    if (conformance.empty() && !normative_path.empty())
      throw std::invalid_argument("--normative requires --conformance");
    if (real_mode == "graph" &&
        (!filter_object_counts.empty() || !filter_object_types.empty() ||
         !filter_object_ids.empty() || filter_object_level_set ||
         filter_object_negative || !filter_start_events_type.empty() ||
         !filter_end_events_type.empty() ||
         !filter_activities_object_type.empty() || !filter_cc_object.empty() ||
         filter_cc_length.has_value() || !filter_cc_object_type.empty() ||
         filter_cc_object_type_negative || !filter_cc_activity.empty())) {
      throw std::invalid_argument(
          "real graph mode cannot be combined with OCEL filters");
    }
    const auto input_path = std::filesystem::u8path(argv[1]);
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
    if (real_mode == "graph" && (csv_input || xml_input || sqlite_input)) {
      throw std::invalid_argument(
          "real graph mode requires a JSON graph input");
    }
    if (!csv_input && !csv_objects_path.empty()) {
      throw std::invalid_argument("--csv-objects requires a CSV input");
    }
    if (!normative_csv_objects_path.empty() && normative_path.empty()) {
      throw std::invalid_argument(
          "--normative-csv-objects requires --normative");
    }
    if (!normative_csv_objects_path.empty() &&
        lower_extension(std::filesystem::u8path(normative_path)) != ".csv") {
      throw std::invalid_argument(
          "--normative-csv-objects requires a CSV normative log");
    }
    std::string real_graph_json;
    {
      if (real_mode == "graph") {
        std::ifstream input(input_path, std::ios::binary);
        if (!input) throw std::runtime_error("cannot open real graph input");
        std::ostringstream text;
        text << input.rdbuf();
        real_graph_json = text.str();
      } else if (csv_input) {
        if (!csv_objects_path.empty()) {
          log = minera::import_ocel_csv(input_path,
                                        std::filesystem::u8path(csv_objects_path),
                                        csv_import_options);
        } else {
          log = minera::import_ocel_csv(input_path, csv_import_options);
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
        (!discover.empty() || !enrich.empty() || !graph.empty() ||
         !split.empty() || !conformance.empty() ||
         !statistics.empty() || ocpn_playout || oc_causal_net_playout ||
         flatten_object_type_set))
      throw std::invalid_argument(
          "--features cannot be combined with discovery, enrichment, graph, split, conformance, or playout");
    if (flatten_object_type_set && !normative_path.empty())
      throw std::invalid_argument(
          "--flatten-object-type cannot be combined with --normative");
    if (!graph.empty()) {
      if (heterogeneous_graph(graph)) {
        std::cout << minera::serialize_ocel_heterogeneous_graph_json(
                         minera::discover_ocel_heterogeneous_graph(
                             log, graph_options))
                  << '\n';
      } else {
        std::cout << minera::serialize_ocel_object_graph_json(
                         minera::discover_ocel_object_graph(
                             log, object_graph_variant(graph)))
                  << '\n';
      }
      return 0;
    }
    if (flatten_object_type_set) {
      minera::OcelFlattenOptions flatten_options;
      flatten_options.object_type = flatten_object_type;
      if (flatten_event_activity_set)
        flatten_options.event_activity_attribute = flatten_event_activity;
      if (flatten_event_timestamp_set)
        flatten_options.event_timestamp_attribute = flatten_event_timestamp;
      std::cout << minera::to_xes(
          minera::flatten_ocel_to_event_log(log, flatten_options),
          "ocel flatten");
      return 0;
    }
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
          minera::discover_ocpn(log, ocpn_options), ocpn_playout_options);
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
      const auto normative_file = std::filesystem::u8path(normative_path);
      minera::Ocel normative;
      std::string normative_json;
      bool normative_is_json = false;
      const auto normative_extension = lower_extension(normative_file);
      if (normative_extension == ".csv") {
        if (!normative_csv_objects_path.empty()) {
          normative = minera::import_ocel_csv(
              normative_file, std::filesystem::u8path(normative_csv_objects_path),
              csv_import_options);
        } else {
          normative = minera::import_ocel_csv(normative_file, csv_import_options);
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
        normative_json = normative_text.str();
        normative_is_json = true;
        // A conformance model may be either a serialized OCEL or the
        // variant-specific graph produced by the discovery command.  Graph
        // parsing is attempted first because its envelope is unambiguous;
        // ordinary OCEL JSON falls back to the existing parser below.
      }
      if (conformance == "etot") {
        minera::EtotConformanceOptions options;
        options.alpha = alpha;
        options.beta = beta;
        options.gamma = gamma;
        options.theta_relative = etot_theta;
        std::optional<minera::EtotGraph> real_graph;
        if (real_mode == "graph")
          real_graph = minera::parse_etot_json(real_graph_json);
        std::optional<minera::EtotGraph> graph;
        if (normative_is_json) {
          try {
            graph = minera::parse_etot_json(normative_json);
          } catch (const std::exception&) {
            // The input may be a regular OCEL document; parse it below.
          }
        }
        if (real_graph) {
          if (graph) {
            std::cout << minera::etot_conformance_json(
                             minera::compare_etot(*real_graph, *graph, options))
                      << '\n';
          } else {
            if (normative_is_json)
              normative = minera::parse_ocel_json(normative_json);
            std::cout << minera::etot_conformance_json(
                             minera::compare_etot(
                                 *real_graph,
                                 minera::discover_etot_classic(normative),
                                 options))
                      << '\n';
          }
        } else if (graph) {
          std::cout << minera::etot_conformance_json(
                           minera::compare_etot(log, *graph, options))
                    << '\n';
        } else {
          if (normative_is_json)
            normative = minera::parse_ocel_json(normative_json);
          std::cout << minera::etot_conformance_json(
                           minera::compare_etot(
                               minera::discover_etot_classic(log),
                               minera::discover_etot_classic(normative), options))
                    << '\n';
        }
      } else if (conformance == "otg") {
        minera::OtgConformanceOptions options;
        options.alpha = alpha;
        options.beta = beta;
        options.gamma = gamma;
        options.theta = otg_theta;
        options.theta_by_relation = otg_theta_by_relation;
        std::optional<minera::OtgGraph> real_graph;
        if (real_mode == "graph")
          real_graph = minera::parse_otg_json(real_graph_json);
        std::optional<minera::OtgGraph> graph;
        if (normative_is_json) {
          try {
            graph = minera::parse_otg_json(normative_json);
          } catch (const std::exception&) {
            // The input may be a regular OCEL document; parse it below.
          }
        }
        if (real_graph) {
          if (graph) {
            std::cout << minera::otg_conformance_json(
                             minera::compare_otg(*real_graph, *graph, options))
                      << '\n';
          } else {
            if (normative_is_json)
              normative = minera::parse_ocel_json(normative_json);
            std::cout << minera::otg_conformance_json(
                             minera::compare_otg(
                                 *real_graph,
                                 minera::discover_otg_classic(normative),
                                 options))
                      << '\n';
          }
        } else if (graph) {
          std::cout << minera::otg_conformance_json(
                           minera::compare_otg(log, *graph, options))
                    << '\n';
        } else {
          if (normative_is_json)
            normative = minera::parse_ocel_json(normative_json);
          std::cout << minera::otg_conformance_json(
                           minera::compare_otg(
                               minera::discover_otg_classic(log),
                               minera::discover_otg_classic(normative), options))
                    << '\n';
        }
      } else {
        minera::OcdfgConformanceOptions options;
        options.alpha = alpha;
        options.beta = beta;
        options.gamma = gamma;
        options.delta = delta;
        options.theta_activity = ocdfg_theta_activity;
        options.theta_flow = ocdfg_theta_flow;
        std::optional<minera::OcdfgGraph> real_graph;
        if (real_mode == "graph")
          real_graph = minera::parse_ocdfg_json(real_graph_json);
        std::optional<minera::OcdfgGraph> graph;
        if (normative_is_json) {
          try {
            graph = minera::parse_ocdfg_json(normative_json);
          } catch (const std::exception&) {
            // The input may be a regular OCEL document; parse it below.
          }
        }
        if (real_graph) {
          if (graph) {
            std::cout << minera::ocdfg_conformance_json(
                             minera::compare_ocdfg(*real_graph, *graph, options))
                      << '\n';
          } else {
            if (normative_is_json)
              normative = minera::parse_ocel_json(normative_json);
            std::cout << minera::ocdfg_conformance_json(
                             minera::compare_ocdfg(
                                 *real_graph,
                                 minera::discover_ocdfg_classic(normative, false),
                                 options))
                      << '\n';
          }
        } else if (graph) {
          std::cout << minera::ocdfg_conformance_json(
                           minera::compare_ocdfg(log, *graph, options))
                    << '\n';
        } else {
          if (normative_is_json)
            normative = minera::parse_ocel_json(normative_json);
          std::cout << minera::ocdfg_conformance_json(
                           minera::compare_ocdfg(
                               minera::discover_ocdfg_classic(log, false),
                               minera::discover_ocdfg_classic(normative, false),
                               options))
                    << '\n';
        }
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
      ocdfg_options.compute_edges_performance = compute_edges_performance;
      std::cout
          << minera::serialize_ocdfg_json(minera::discover_ocdfg_classic_with_options(
                 log, ocdfg_options))
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
      std::cout << minera::saw_nets_json(minera::discover_saw_nets(log, saw_options))
                << '\n';
      return 0;
    }
    if (format == "sqlite") {
      if (sqlite_output_path.empty())
        throw std::invalid_argument(
            "--format sqlite requires --sqlite-output OUTPUT.sqlite");
      minera::export_ocel_sqlite(log, std::filesystem::u8path(sqlite_output_path));
    } else if (format == "csv") {
      if (!csv_output_path.empty() && !csv_objects_output_path.empty()) {
        minera::export_ocel_csv(
            log, std::filesystem::u8path(csv_output_path),
            std::filesystem::u8path(csv_objects_output_path), csv_export_options);
      } else if (!csv_output_path.empty()) {
        minera::export_ocel_csv(log, std::filesystem::u8path(csv_output_path),
                                csv_export_options);
      } else {
        std::cout << minera::serialize_ocel_csv(log, csv_export_options);
      }
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

#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
  std::vector<std::string> values;
  values.reserve(argc);
  for (int i = 0; i < argc; ++i) {
    const auto size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argv[i], -1,
                                        nullptr, 0, nullptr, nullptr);
    if (size <= 0) { std::cerr << "error: invalid Unicode argument\n"; return 2; }
    std::string value(size, '\0');
    if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argv[i], -1,
                           value.data(), size, nullptr, nullptr)) return 2;
    value.pop_back(); values.push_back(std::move(value));
  }
  std::vector<char*> arguments;
  for (auto& value : values) arguments.push_back(value.data());
  arguments.push_back(nullptr);
  return run_ocel(argc, arguments.data());
}
#else
int main(int argc, char** argv) { return run_ocel(argc, argv); }
#endif
