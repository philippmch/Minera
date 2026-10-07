#include "case_relations_input.hpp"
// Runs the Minera core on a CSV event log and emits the analysis (event log
// plus discovered directly-follows graph) as JSON for the frontend.

#include <algorithm>
#include <array>
#include <charconv>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <type_traits>
#include <vector>

#include "pnml_model_options.hpp"
#include "replay_input_options.hpp"

#include "minera/alpha_miner.hpp"
#include "minera/alpha_result_json.hpp"
#include "minera/bpmn.hpp"
#include "minera/attribute_kde.hpp"
#include "minera/attribute_statistics.hpp"
#include "minera/attribute_distribution.hpp"
#include "minera/boundary_activities.hpp"
#include "minera/browser_history.hpp"
#include "minera/conformance.hpp"
#include "minera/concept_drift.hpp"
#include "minera/case_arrival.hpp"
#include "minera/case_statistics.hpp"
#include "minera/cycle_time.hpp"
#include "minera/correlation_miner.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/declare.hpp"
#include "minera/declare_playout.hpp"
#include "minera/decision_mining.hpp"
#include "minera/decision_table.hpp"
#include "minera/dfg_variants.hpp"
#include "minera/dfg_dataframe.hpp"
#include "minera/dfg_dataframe_json.hpp"
#include "minera/dfg_io.hpp"
#include "minera/dfg_playout.hpp"
#include "minera/dfg_alignment.hpp"
#include "minera/edit_distance_alignment.hpp"
#include "minera/directly_follows_graph.hpp"
#include "minera/eventually_follows.hpp"
#include "minera/footprints.hpp"
#include "minera/heuristic_miner.hpp"
#include "minera/inductive_miner.hpp"
#include "minera/ilp_miner.hpp"
#include "minera/genetic_miner.hpp"
#include "minera/genetic_matrix_conversion.hpp"
#include "minera/trace_variant_query.hpp"
#include "minera/pripel.hpp"
#include "minera/organizational_mining.hpp"
#include "minera/network_analysis.hpp"
#include "minera/resource_profiles.hpp"
#include "minera/local_diagnostics.hpp"
#include "minera/clustering.hpp"
#include "minera/role_discovery.hpp"
#include "minera/json_export.hpp"
#include "minera/log_filter.hpp"
#include "minera/log_conversion.hpp"
#include "minera/ltl_filter.hpp"
#include "minera/log_skeleton.hpp"
#include "minera/log_to_target.hpp"
#include "minera/log_to_features.hpp"
#include "minera/label_splitting.hpp"
#include "minera/link_analysis.hpp"
#include "minera/trie.hpp"
#include "minera/trie_conversion.hpp"
#include "minera/interval_tree.hpp"
#include "minera/case_relations.hpp"
#include "minera/random_tree.hpp"
#include "minera/model_decoration.hpp"
#include "minera/monte_carlo.hpp"
#include "minera/performance.hpp"
#include "minera/performance_spectrum.hpp"
#include "minera/batches.hpp"
#include "minera/chaotic_activities.hpp"
#include "minera/passed_time.hpp"
#include "minera/service_time.hpp"
#include "minera/rework.hpp"
#include "minera/playout.hpp"
#include "minera/pnml.hpp"
#include "minera/powl_conversion.hpp"
#include "minera/powl_discovery.hpp"
#include "minera/ptml.hpp"
#include "minera/process_tree_alignment.hpp"
#include "minera/process_tree_playout.hpp"
#include "minera/decomposed_alignment.hpp"
#include "minera/petri_anti_alignment.hpp"
#include "minera/petri_multi_alignment.hpp"
#include "minera/petri_usage_comparison.hpp"
#include "minera/petri_net_reduction.hpp"
#include "minera/petri_analysis_json.hpp"
#include "minera/process_tree_reduction.hpp"
#include "minera/resource_analysis.hpp"
#include "minera/request_params.hpp"
#include "minera/stochastic_map.hpp"
#include "minera/segment_filter.hpp"
#include "minera/path_filter.hpp"
#include "minera/temporal_profile.hpp"
#include "minera/timestamp_interleavings.hpp"
#include "minera/minimum_self_distance.hpp"
#include "minera/transition_system.hpp"
#include "minera/xes_export.hpp"
#include "minera/xes_event_log_importer.hpp"
#include "minera/windows_event_log.hpp"
#include "minera/repository_history.hpp"
#include "minera/workflow_history.hpp"
#include "minera/enterprise_history.hpp"
#include "minera/communication_history.hpp"
#include "repository_history_runtime.hpp"
#include "embedding_runtime.hpp"

namespace {

bool ends_with_ci(std::string_view value, std::string_view suffix) {
  if (value.size() < suffix.size()) return false;
  const auto tail = value.substr(value.size() - suffix.size());
  return std::equal(tail.begin(), tail.end(), suffix.begin(), [](char a, char b) {
    return std::tolower(static_cast<unsigned char>(a)) == b;
  });
}

// Splits a comma-separated option value into trimmed, non-empty activity names.
std::vector<std::string> split_csv(std::string_view value) {
  std::vector<std::string> parts;
  std::size_t start = 0;
  for (std::size_t i = 0; i <= value.size(); ++i) {
    if (i == value.size() || value[i] == ',') {
      const auto token = value.substr(start, i - start);
      const auto first = token.find_first_not_of(" \t\r\n");
      if (first != std::string_view::npos) {
        const auto last = token.find_last_not_of(" \t\r\n");
        parts.emplace_back(token.substr(first, last - first + 1));
      }
      start = i + 1;
    }
  }
  return parts;
}

bool is_xes_path(const std::filesystem::path& path) {
  const auto value = path.string();
  return ends_with_ci(value, ".xes") || ends_with_ci(value, ".xes.gz");
}

bool has_table_column(const minera::EventTable& table, std::string_view column) {
  return std::find(table.columns.begin(), table.columns.end(), column) !=
         table.columns.end();
}

std::string csv_case_column(const minera::EventTable& table,
                            const minera::CsvImportOptions& options) {
  if (has_table_column(table, options.case_id_column))
    return options.case_id_column;
  for (std::size_t index = 0; index < table.columns.size(); ++index) {
    const auto marked = std::any_of(
        table.rows.begin(), table.rows.end(), [&](const auto& row) {
          return row[index] &&
                 row[index]->first_class_role ==
                     minera::FirstClassAttributeRole::kCaseId;
        });
    if (marked) return table.columns[index];
  }
  return options.case_id_column;
}

void adapt_case_relations_table_timestamp(
    minera::CaseRelationsOptions& options,
    const minera::EventTable& base,
    const minera::EventTable& incoming,
    const minera::CsvImportOptions& csv_options) {
  // Callers invoke this only when merge_timestamp_key was omitted.  The
  // public default is the XES spelling; CSV import resolves the established
  // timestamp/time:timestamp aliases, so use the first known source header
  // even when it is present on only one side and let the table merge handle
  // the missing side.  An explicitly supplied selector remains exact.
  if (options.timestamp_key != "time:timestamp") return;
  const std::array<std::string_view, 3> candidates = {
      "time:timestamp", csv_options.timestamp_column, "timestamp"};
  for (const auto candidate : candidates) {
    if (!candidate.empty() &&
        (has_table_column(base, candidate) ||
         has_table_column(incoming, candidate))) {
      options.timestamp_key = std::string(candidate);
      return;
    }
  }
}

void normalize_csv_timestamp_missingness(minera::EventTable& table) {
  for (auto& row : table.rows) {
    for (auto& cell : row) {
      if (!cell || cell->first_class_role !=
                       minera::FirstClassAttributeRole::kTimestamp)
        continue;
      if (cell->first_class_state ==
          minera::FirstClassAttributeState::kMissing) {
        cell.reset();
      } else if (cell->first_class_state ==
                 minera::FirstClassAttributeState::kNull) {
        cell->is_null = true;
        cell->value.clear();
        cell->typed_value = minera::NativeAttributeValue{};
      }
    }
  }
}

std::string trim_text(std::string_view value) {
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string_view::npos) return {};
  const auto last = value.find_last_not_of(" \t\r\n");
  return std::string(value.substr(first, last - first + 1));
}

void register_activity_selector_values(
    minera::DirectlyFollowsGraph& graph, const minera::EventLog& log,
    std::string_view activity_attribute) {
  if (activity_attribute.empty() || activity_attribute == "concept:name" ||
      activity_attribute == "activity") {
    return;
  }
  for (const auto& trace : log.traces) {
    for (const auto& event : trace.events) {
      if (const auto* value = minera::dfg_event_field_value(
              event, activity_attribute);
          value != nullptr && !trim_text(*value).empty())
        graph.ensure_activity(*value);
    }
  }
}

void register_powl_labels(minera::DirectlyFollowsGraph& graph,
                          const minera::PowlNode& node) {
  if (node.kind() == minera::PowlNodeKind::kTransition)
    graph.ensure_activity(node.label());
  for (const auto& child : node.children())
    register_powl_labels(graph, child);
}

minera::EventLog project_powl_activity(const minera::EventLog& source,
                                       std::string_view activity_key,
                                       std::string_view timestamp_key,
                                       std::string_view case_id_key =
                                           "case:concept:name") {
  minera::PowlDiscoveryOptions options;
  options.activity_key = std::string(activity_key);
  options.timestamp_key = std::string(timestamp_key);
  options.case_id_key = std::string(case_id_key);
  return minera::normalize_powl_event_log(source, options);
}

const minera::PetriNet* select_decision_model(
    std::string_view miner, const minera::PetriNet& inductive,
    const minera::PetriNet& alpha, const minera::PetriNet& heuristic,
    const minera::PetriNet& powl) {
  if (miner == "inductive") return &inductive;
  if (miner == "alpha") return &alpha;
  if (miner == "heuristic") return &heuristic;
  if (miner == "powl") return &powl;
  return nullptr;
}

std::vector<std::string> canonical_dfg_variants(
    std::string_view value, const std::vector<std::string>& catalog,
    std::string_view option) {
  const auto requested = split_csv(value);
  if (requested.empty()) {
    throw std::invalid_argument(std::string(option) +
                                " expects a non-empty list");
  }
  for (const auto& name : requested) {
    if (std::find(catalog.begin(), catalog.end(), name) == catalog.end()) {
      throw std::invalid_argument(std::string(option) +
                                  " unknown variant '" + name + "'");
    }
  }
  std::vector<std::string> result;
  for (const auto& name : catalog) {
    if (std::find(requested.begin(), requested.end(), name) !=
        requested.end()) {
      result.push_back(name);
    }
  }
  return result;
}

std::vector<minera::BusinessHourSlot> parse_dfg_business_hour_slots(
    std::string_view value,
    std::string_view option_name = "--dfg-business-hour-slots") {
  std::vector<minera::BusinessHourSlot> slots;
  for (const auto& part : split_csv(value)) {
    auto separator = part.find(':');
    if (separator == std::string::npos) separator = part.find('-');
    if (separator == std::string::npos || separator == 0 ||
        separator + 1 == part.size()) {
      throw std::invalid_argument(
          std::string(option_name) + " expects start-end second pairs");
    }
    const auto parse = [&](std::string_view text) {
      std::string owned(text);
      char* end = nullptr;
      const double result = std::strtod(owned.c_str(), &end);
      if (end != owned.c_str() + owned.size() || !std::isfinite(result)) {
        throw std::invalid_argument(
            std::string(option_name) + " expects finite seconds");
      }
      return result;
    };
    const double start =
        parse(std::string_view(part).substr(0, separator));
    const double end =
        parse(std::string_view(part).substr(separator + 1));
    if (start < 0.0 || start >= end || end > 604800.0) {
      throw std::invalid_argument(
          std::string(option_name) + " requires "
          "0 <= start < end <= 604800");
    }
    slots.push_back({start, end});
  }
  return slots;
}

minera::DfgStatisticalVariant dfg_statistical_variant(
    std::string_view name) {
  if (name == "frequency_triples")
    return minera::DfgStatisticalVariant::kFrequencyTriples;
  if (name == "performance" || name == "performance_greedy")
    return minera::DfgStatisticalVariant::kPerformance;
  if (name == "case_attributes")
    return minera::DfgStatisticalVariant::kCaseAttributes;
  if (name == "clean")
    return minera::DfgStatisticalVariant::kClean;
  if (name == "clean_polars")
    return minera::DfgStatisticalVariant::kCleanPolars;
  if (name == "clean_time")
    return minera::DfgStatisticalVariant::kCleanTime;
  return minera::DfgStatisticalVariant::kNative;
}

minera::DfgVariantsAnalysis analyze_dfg_variants(
    const minera::EventLog& log,
    const std::vector<std::string>& statistical_variants,
    const std::vector<std::string>& causal_variants,
    const minera::DfgStatisticalOptions& shared_options) {
  minera::DfgVariantsAnalysis result;
  result.statistical.reserve(statistical_variants.size());
  for (const auto& name : statistical_variants) {
    auto options = shared_options;
    options.variant = dfg_statistical_variant(name);
    result.statistical.push_back(
        {name, minera::discover_dfg_statistics(log, options)});
  }
  if (!causal_variants.empty()) {
    const auto frequency = minera::discover_frequency_dfg(log, shared_options);
    result.causal.reserve(causal_variants.size());
    for (const auto& name : causal_variants) {
      const auto variant = name == "heuristic"
                               ? minera::DfgCausalVariant::kHeuristic
                               : minera::DfgCausalVariant::kAlpha;
      result.causal.push_back(
          {name, minera::discover_dfg_causality(frequency, variant)});
    }
  }
  return result;
}

minera::CorrelationVariant correlation_variant(std::string_view name) {
  if (name == "classic") return minera::CorrelationVariant::kClassic;
  if (name == "classic_split") return minera::CorrelationVariant::kClassicSplit;
  return minera::CorrelationVariant::kTraceBased;
}

minera::CorrelationVariantsAnalysis analyze_correlation_variants(
    const minera::EventDataView& data, const std::vector<std::string>& variants,
    const minera::CorrelationMinerOptions& shared_options) {
  minera::CorrelationVariantsAnalysis result;
  result.variants.reserve(variants.size());
  for (const auto& name : variants) {
    auto options = shared_options;
    options.variant = correlation_variant(name);
    result.variants.push_back(
        {name, minera::discover_correlation_miner(data, options)});
  }
  return result;
}

minera::FootprintsDiscoveryOptions footprints_discovery_options(
    const minera::FootprintsOptions& options) {
  minera::FootprintsDiscoveryOptions result;
  result.activity_attribute = options.activity_attribute;
  result.case_id_attribute = options.case_id_attribute;
  result.timestamp_attribute = options.timestamp_attribute;
  result.start_timestamp_attribute = options.start_timestamp_attribute;
  result.index_attribute = options.index_attribute;
  result.sort_required = options.sort_required;
  result.initial_marking = options.initial_marking;
  return result;
}

minera::FootprintsAnalysis analyze_footprints(
    const minera::EventLog& log, const minera::DirectlyFollowsGraph& graph,
    const minera::EventLog& alpha_log,
    const minera::ProcessTree& tree, const minera::PowlNode& powl,
    const minera::PetriNet& inductive_net, const minera::PetriNet& alpha_net,
    const minera::PetriNet& heuristic_net, const minera::PetriNet& powl_net,
    const minera::FootprintsOptions& options) {
  minera::FootprintsAnalysis analysis;
  analysis.model = options.model;
  analysis.strict = options.strict;
  const auto discovery_options = footprints_discovery_options(options);
  const auto& selected_log = options.model == "alpha" ? alpha_log : log;
  analysis.activity_attribute = discovery_options.activity_attribute;
  analysis.case_id_attribute = discovery_options.case_id_attribute;
  analysis.trace_case_ids = minera::resolve_footprints_case_ids(
      selected_log, discovery_options.case_id_attribute);
  const auto projected_log =
      minera::prepare_footprints_event_log(selected_log, discovery_options);
  const auto projected_graph =
      minera::discover_directly_follows_graph(projected_log);
  std::optional<minera::Footprint> log_footprint;
  std::optional<std::vector<minera::Footprint>> trace_footprints;
  std::optional<minera::Footprint> dfg_footprint;
  std::optional<minera::Footprint> tree_footprint;
  std::optional<minera::Footprint> powl_footprint;
  std::optional<minera::EventTable> dataframe;
  std::optional<minera::LazyEventTable> lazy_dataframe;
  const auto& log_fp = [&]() -> const minera::Footprint& {
    if (!log_footprint)
      log_footprint =
          minera::discover_footprints(projected_log, discovery_options);
    return *log_footprint;
  }();
  const auto traces = [&]() -> const std::vector<minera::Footprint>& {
    if (!trace_footprints) {
      trace_footprints =
          minera::discover_trace_footprints(projected_log, discovery_options);
    }
    return *trace_footprints;
  };
  const auto dfg_fp = [&]() -> const minera::Footprint& {
    if (!dfg_footprint) {
      dfg_footprint = minera::discover_dfg_footprints(projected_graph);
    }
    return *dfg_footprint;
  };
  const auto tree_fp = [&]() -> const minera::Footprint& {
    if (!tree_footprint) tree_footprint = minera::discover_footprints(tree);
    return *tree_footprint;
  };
  const auto powl_fp = [&]() -> const minera::Footprint& {
    if (!powl_footprint) powl_footprint = minera::discover_footprints(powl);
    return *powl_footprint;
  };

  if (options.discovery_variants.contains("entire_event_log")) {
    analysis.entire_event_log = log_fp;
  }
  if (options.discovery_variants.contains("entire_dataframe")) {
    if (!dataframe) {
      dataframe =
          minera::prepare_footprints_event_table(selected_log, discovery_options);
    }
    analysis.entire_dataframe = minera::discover_footprints(
        *dataframe, discovery_options);
  }
  if (options.discovery_variants.contains("polars_lazyframes")) {
    if (!lazy_dataframe) {
      lazy_dataframe =
          minera::prepare_footprints_lazy_table(selected_log, discovery_options);
    }
    analysis.polars_lazyframes = minera::discover_footprints(
        *lazy_dataframe, discovery_options);
  }
  if (options.discovery_variants.contains("trace_by_trace")) {
    analysis.trace_by_trace = traces();
  }
  if (options.discovery_variants.contains("dfg")) analysis.dfg = dfg_fp();
  if (options.discovery_variants.contains("process_tree")) {
    analysis.process_tree = tree_fp();
  }
  if (options.discovery_variants.contains("powl")) analysis.powl = powl_fp();

  const minera::PetriNet* selected_petri = nullptr;
  if (options.model == "process_tree" || options.model == "inductive") {
    selected_petri = &inductive_net;
  } else if (options.model == "powl") {
    selected_petri = &powl_net;
  } else if (options.model == "alpha") {
    selected_petri = &alpha_net;
  } else if (options.model == "heuristic") {
    selected_petri = &heuristic_net;
  }
  if (options.model == "process_tree") {
    analysis.model_footprint = tree_fp();
  } else if (options.model == "powl") {
    analysis.model_footprint = powl_fp();
  } else if (options.model == "dfg") {
    analysis.model_footprint = dfg_fp();
  } else {
    analysis.model_footprint = minera::discover_footprints(
        *selected_petri, discovery_options);
  }
  if (options.discovery_variants.contains("petri_reach_graph")) {
    if (selected_petri == nullptr) {
      throw std::invalid_argument(
          "petri_reach_graph requires a Petri-capable footprints model");
    }
    analysis.petri_reach_graph = minera::discover_footprints(
        *selected_petri, discovery_options);
  }

  if (options.conformance_variants.contains("log_model")) {
    minera::FootprintsLogModelOptions conformance_options;
    conformance_options.strict = options.strict;
    conformance_options.case_id_attribute = options.case_id_attribute;
    analysis.log_model = minera::check_footprints_log_model(
        log_fp, analysis.model_footprint, conformance_options);
  }
  if (options.conformance_variants.contains("log_extensive")) {
    analysis.log_extensive = minera::check_footprints_log_extensive(
        log_fp, analysis.model_footprint);
  }
  minera::FootprintsTraceExtensiveOptions trace_options;
  trace_options.enable_act_always_executed =
      options.enable_act_always_executed;
  trace_options.case_id_attribute = options.case_id_attribute;
  const auto trace_results = minera::check_footprints_trace_extensive(
      traces(), analysis.model_footprint, trace_options);
  if (options.conformance_variants.contains("trace_extensive")) {
    analysis.trace_extensive = trace_results;
  }
  analysis.precision = minera::fp_precision(log_fp, analysis.model_footprint);
  const auto fitness =
      minera::fp_fitness(traces(), analysis.model_footprint, trace_results);
  analysis.log_fitness = fitness.log_fitness;
  analysis.perc_fit_traces = fitness.perc_fit_traces;
  return analysis;
}

int usage(const char* program) {
  std::cerr << "Usage: " << program
            << " <event_log.csv|.xes> [options]\n"
               "  A .xes file is imported as XES; any other file as CSV.\n"
               "  -o <path>                 Write JSON to <path> instead of stdout\n"
               "  --case-column <name>      Case id column (default: case_id)\n"
               "  --activity-column <name>  Activity column (default: activity)\n"
               "  --timestamp-column <name> Timestamp column (default: timestamp)\n"
               "  --resource-column <name>  Resource column (default: resource)\n"
               "  --delimiter <char>        Field delimiter (default: ,)\n"
               "  --xes-import-variant <id> iterparse, line_by_line,\n"
               "                            iterparse_mem_compressed, iterparse_20,\n"
               "                            chunk_regex, or rustxes\n"
               "  --xes-return-legacy-log-object <bool> Emit XES import result JSON\n"
               "  --xes-return-pl-lazyframe <bool> Select lazy raw RUSTXES table\n"
               "  --xes-conversion-case-id-key <key> RUSTXES legacy grouping column\n"
               "  --xes-conversion-case-attribute-prefix <prefix> RUSTXES trace fields\n"
               "  --xes-conversion-null-case-id-key <bool> Use a null grouping key\n"
               "  --xes-conversion-null-case-attribute-prefix <bool> Use a null prefix\n"
               "  --xes-conversion-stream-postprocessing <bool> RUSTXES compatibility flag\n"
               "  --xes-conversion-deepcopy <bool> RUSTXES compatibility flag\n"
               "  --xes-conversion-include-case-attributes <bool> RUSTXES compatibility flag\n"
               "  --xes-conversion-compress <bool> RUSTXES compatibility flag\n"
               "  --xes-conversion-infer-extensions <bool> Infer or clear RUSTXES extensions\n"
               "  --xes-conversion-extension <name> <prefix> <uri> Add an explicit extension\n"
               "  --xes-timestamp-sort <bool> Sort events and traces by timestamp\n"
               "  --xes-reverse-sort <bool> Reverse requested timestamp ordering\n"
               "  --xes-max-traces <n>      Import at most n traces\n"
               "  --xes-attributes-to-read <keys> Optional scalar key projection\n"
               "  --xes-case-attribute <key> Trace case-id selector\n"
               "  --xes-activity-attribute <key> Event activity selector\n"
               "  --xes-timestamp-attribute <key> Event timestamp selector\n"
               "  --xes-resource-attribute <key> Event resource selector\n"
               "  --xes-lifecycle-attribute <key> Event lifecycle selector\n"
               "  --xes-require-timestamp <bool> Reject logs without timestamps\n"
               "  --xes-max-decompressed-bytes <n> Gzip expansion safety bound\n"
               "  --xes-export-variant <id> line_by_line or etree\n"
               "  --xes-compress <bool>     Override .gz output detection\n"
               "  --xes-infer-metadata <bool> Add standard inferred declarations\n"
               "  --xes-preserve-nulls <bool> Emit native explicit-null markers\n"
               "  --log-convert <id>       Event-log conversion selector\n"
               "                            to_event_log, to_event_stream,\n"
               "                            to_data_frame, to_nx, df_to_event_log_1v,\n"
               "                            or df_to_event_log_nv\n"
               "  --log-conversion-* <v>   Conversion attributes, flags, and bounds\n"
               "  --log-conversion-stream-postprocessing <bool> Drop null fields\n"
               "  --log-conversion-lazy-output <bool> Use restartable table output\n"
               "  --log-conversion-max-rows-to-materialize <n> Table row cap\n"
               "  --log-conversion-max-variants-to-return <n> Variant output cap\n"
               "  --noise-threshold <f>     Infrequent-variant filter in [0,1]\n"
               "                            (used only with --inductive-variant imf)\n"
               "  --inductive-variant <id>  log (default), imf (infrequent),\n"
               "                            or dfg (IMd: graph recursion)\n"
               "  --inductive-activity-key <name> Event activity selector\n"
               "  --inductive-timestamp-key <name> Event timestamp selector\n"
               "  --inductive-case-id-key <name> Case identifier selector\n"
               "  --inductive-sort-events-by-timestamp <bool>\n"
               "                            Sort selected events chronologically\n"
               "  --inductive-disable-strict-sequence-cut <bool>\n"
               "                            Use the normal sequence cut\n"
               "  --inductive-disable-fallthroughs <bool>\n"
               "                            Suppress specialized fall-throughs\n"
               "  --inductive-multiprocessing <bool>\n"
               "                            Evaluate large fall-through candidate sets with two workers\n"
               "  --ilp-mine                Emit standalone classic ILP JSON\n"
               "  --ilp-activity-key <name> Event attribute used as activity\n"
               "  --ilp-artificial-start-activity <name> Artificial start label\n"
               "  --ilp-artificial-end-activity <name> Artificial end label\n"
               "  --ilp-alpha <f>            Sequence-encoding threshold in [0,1]\n"
               "  --ilp-causal-relation <A=>B,...> Optional causal edges\n"
               "  --ilp-noise-threshold <f> ILP threshold in [0,1]\n"
               "  --ilp-max-nodes <n>       Positive ILP search bound\n"
               "  --genetic-mine            Emit standalone classic genetic JSON\n"
               "  --genetic-matrix-convert  Convert the discovered causal matrix\n"
               "  --genetic-conversion-variant <id> to_petri_net\n"
               "  --genetic-activity-key <name> Event attribute used as activity\n"
               "  --genetic-timestamp-key <name> Event timestamp attribute\n"
               "  --genetic-case-id-key <name> Case id or event/trace attribute\n"
               "  --genetic-seed <n>        Deterministic search seed\n"
               "  --genetic-population-size <n> Population bound (>=2)\n"
               "  --genetic-generations <n> Generation bound\n"
               "  --genetic-elitism-rate <f> Elitism rate in [0,1]\n"
               "  --genetic-crossover-rate <f> Crossover rate in [0,1]\n"
               "  --genetic-mutation-rate <f> Mutation rate in [0,1]\n"
               "  --genetic-elitism-min-sample <n> Tournament sample\n"
               "  --genetic-max-activities <n> Activity safety bound\n"
               "  --genetic-max-states <n> Candidate safety bound\n"
               "  --organizational-sna   Emit organizational SNA JSON\n"
               "  --sna-variant <id>     four log variants or their _pandas variants\n"
               "  --sna-activity-attribute <key> Activity selector\n"
               "  --sna-resource-attribute <key> Resource selector\n"
               "  --sna-case-id-attribute <key> Case selector for pandas variants\n"
               "  --sna-beta <f>         Handover beta in [0,1]\n"
               "  --sna-n <n>            Subcontracting distance\n"
               "  --sna-metric-normalization <bool> Normalize edge weights\n"
               "  --sna-max-resources <n> Resource safety bound\n"
               "  --sna-max-edges <n>    Edge safety bound\n"
               "  --network-analysis     Emit organizational network analysis JSON\n"
               "  --network-analysis-variant <id> dataframe (default)\n"
               "  --network-analysis-* <v> Network selectors, performance, hours, and bounds\n"
               "  --link-analysis       Emit classic link-analysis JSON\n"
               "  --link-analysis-* <v> Relation columns, ordering, propagation, and bounds\n"
               "  --roles                Emit deterministic role discovery JSON\n"
               "  --roles-variant <id>   log (default) or pandas\n"
               "  --roles-activity-attribute <key> Activity selector\n"
               "  --roles-resource-attribute <key> Resource selector\n"
               "  --roles-threshold <f>  Merge threshold in [0,1]\n"
               "  --roles-max-resources <n> Resource safety bound\n"
               "  --roles-max-activities <n> Activity safety bound\n"
               "  --resource-profiles      Emit resource profile JSON\n"
               "  --resource-profile-* <v> Resource profile metric, attributes, slots, selectors, and bounds\n"
               "  --local-diagnostics      Emit bounded local diagnostics JSON\n"
               "  --local-diagnostics-* <v> Group attributes, explicit groups, and bounds\n"
               "  --clustering             Emit trace-attribute clustering JSON\n"
               "  --clustering-* <v>       Attribute, variant, alpha, percent, and bounds\n"
               "  --profile-clustering      Emit profile-based clustering JSON\n"
               "  --profile-clustering-* <v>  Variant, K-means bounds, seed, representations, and attributes\n"
               "  --trace-variant-query    Emit trace-variant query JSON\n"
               "  --tvq-variant <id>       laplace or sacofa (default: sacofa)\n"
               "  --tvq-epsilon <f>        Positive privacy parameter\n"
               "  --tvq-k <n>              Maximum prefix length (0 = empty)\n"
               "  --tvq-p <n>              Minimum released frequency\n"
               "  --tvq-seed <n>           Deterministic noise seed\n"
               "  --tvq-secure-random <bool> Use OS entropy for noise\n"
               "  --tvq-max-output-traces <n> Safety trace cap\n"
               "  --tvq-max-output-variants <n> Candidate cap\n"
               "  --pripel <query.csv|.xes>  Emit PRIPEL JSON using a query log\n"
               "  --pripel-epsilon <f>      Positive privacy parameter\n"
               "  --pripel-blocklist <a,b>  Attributes to remove\n"
               "  --pripel-seed <n>         Deterministic anonymization seed\n"
               "  --pripel-secure-random <bool> Use OS entropy for noise\n"
               "  --pripel-max-output-traces <n> Output trace cap\n"
               "  --pripel-max-distance <n>  Nearest-variant distance cap\n"
               "                            recurse) or dfg (IMd: recurse on\n"
               "                            the directly-follows graph alone,\n"
               "                            scaling to huge logs)\n"
               "  --alpha-variant <id>      classic (default) or plus (short loops)\n"
               "  --alpha-plus-result      Emit Alpha+ graph with live/detached markings\n"
               "  --alpha-activity-attribute <name> Event field used as activity\n"
               "  --alpha-remove-unconnected <bool> Drop unconnected Alpha transitions\n"
               "  --alpha-max-rows-to-materialize <n> Alpha input event cap\n"
               "  --heuristic-variant <id>  classic (default) or plusplus\n"
               "                            (interval/eventual-follow semantics)\n"
               "  --heuristic-activity-attribute <name>\n"
               "                            Event field used as activity label\n"
               "                            (default: concept:name)\n"
               "  --heuristic-timestamp-attribute <name>\n"
               "                            Event field used as completion time\n"
               "                            (default: time:timestamp)\n"
               "  --dependency-threshold <f>  Heuristic Miner: minimum dependency\n"
               "                            measure (default: 0.5)\n"
               "  --and-threshold <f>       Heuristic Miner: minimum AND measure\n"
               "                            for a parallel split/join (default:\n"
               "                            0.65)\n"
               "  --min-edge-frequency <n>  Heuristic Miner: minimum path\n"
               "                            occurrences (default: 1)\n"
               "  --heuristic-min-activity-count <n>\n"
               "                            Minimum activity occurrences\n"
               "                            (default: 1)\n"
               "  --heuristic-decoration <id> frequency (default) or performance\n"
               "  --heuristic-performance-aggregation <id>\n"
               "                            mean (default), median, min, max,\n"
               "                            stdev, sum, raw_values, or all\n"
               "  --heuristic-dfg-pre-cleaning-noise-threshold <f>\n"
               "                            Classic DFG pre-clean noise\n"
               "                            (default: 0.05)\n"
               "  --heuristic-loop-length-two-threshold <f>\n"
               "                            Classic length-two-loop threshold\n"
               "                            (default: 0.5)\n"
               "  --heuristic-keep-once-per-case\n"
               "                            Count each classic DFG path once/case\n"
               "  --heuristic-most-common-paths <a->b,...>\n"
               "                            Exempt classic paths from pre-cleaning\n"
               "  --heuristic-start-timestamp-attribute <name>\n"
               "                            Heuristics interval-start event attribute\n"
               "                            (default: CLASSIC completion; PLUSPLUS start_timestamp if present)\n"
               "  --heuristic-business-hours\n"
               "                            Measure heuristic waits/sojourns in business time\n"
               "  --heuristic-business-hour-slots <start-end,...>\n"
               "                            Weekly business slots; 'none' disables the schedule\n"
               "  --heuristic-lifecycle-instance-attribute <name>\n"
               "                            Miner++ lifecycle instance attribute\n"
               "                            (default: concept:instance)\n"
               "  --transition-view <id>    Transition-system state view:\n"
               "                            sequence (default), set, or multiset\n"
               "  --transition-direction <id>  Window direction: forward\n"
               "                            (default) or backward\n"
               "  --transition-window <n>   Signed state-window size\n"
               "                            (default: 2; 0 collapses all states)\n"
               "  --transition-include-data Include stable trace/event occurrence\n"
               "                            references in the transition system\n"
               "  --transition-activity-attribute <name> Activity selector\n"
               "  --transition-case-id-attribute <name> Case-id selector\n"
               "  --powl-variant <id>       POWL discovery: tree, brute_force,\n"
               "                            maximal (default), dynamic_clustering,\n"
               "                            or dynamic_clustering_plain\n"
               "  --powl-filtering-type <id> POWL filtering: decreasing_factor\n"
               "                            (default) or dynamic\n"
               "  --powl-filtering-threshold <f> POWL filter threshold in [0,1)\n"
               "                            (default: 0 = disabled)\n"
               "  --powl-order-frequency-ratio <f> Dynamic-clustering order\n"
               "                            ratio in (0.5,1] (default: 1)\n"
               "  --powl-brute-force-activity-limit <n> Maximum local alphabet\n"
                "                            for brute force (0 = unlimited)\n"
                "  --powl-activity-key <name> Event field used as the POWL activity\n"
                "                            label (default: concept:name)\n"
                "  --powl-timestamp-key <name> Timestamp selector retained for\n"
                "                            tabular parity (default: time:timestamp)\n"
                "  --powl-case-id-key <name> Case selector retained for tabular\n"
                "                            parity (default: case:concept:name)\n"
                "  --pnml <path>             Also write a model as PNML to <path>\n"
               "  --check-pnml <path>       Also check the log against the\n"
               "                            marked net in this PNML file\n"
               "                            (conformance id: imported)\n"
               "  --pnml-public-markings   Use public positive-only PNML markings\n"
               "  --pnml-miner <id>         Which model to export as PNML:\n"
               "                            inductive (default), alpha, heuristic,\n"
               "                            powl, or imported\n"
               "  --process-tree-variant <id>\n"
               "                            Process-tree Petri conversion:\n"
               "                            standard (default) or transition_bordered\n"
               "  --bpmn <path>             Also write a model as BPMN 2.0 to <path>\n"
               "  --check-bpmn <path>       Also check the log against the\n"
               "                            BPMN 2.0 model in this file\n"
               "                            (conformance id: imported)\n"
               "  --bpmn-miner <id>         Which model to export as BPMN:\n"
               "                            inductive (default), alpha, heuristic,\n"
               "                            or powl\n"
               "  --ptml <path>             Also write the inductive process\n"
               "                            tree as PTML to <path>\n"
               "  --powl-ptml <path>        Convert the discovered POWL workflow\n"
               "                            net strictly to PTML; reject\n"
               "                            non-block-structured models\n"
               "  --pnml-to-ptml <path>    Convert an imported PNML workflow\n"
               "                            net to PTML without an event log\n"
               "  --bpmn-to-ptml <path>    Convert an imported BPMN process\n"
               "                            to PTML without an event log\n"
               "  --check-ptml <path>       Also check the log against the\n"
               "                            process tree in this PTML file\n"
               "                            (conformance id: imported)\n"
               "  --xes <path>              Also write the event log (after any\n"
               "                            filtering) as XES to <path>\n"
               "  --dfg <path>              Also write the classic directly-follows\n"
               "                            graph (after any filtering) to <path>\n"
               "  --ltl-variant <id>        eventually_follows, a_next_b_next_c,\n"
               "                            four_eyes_principle, or attr_value_different_persons\n"
               "  --ltl-attribute-key <name> Event attribute key (default: activity)\n"
               "  --ltl-attribute-values <a,b,...> Ordered LTL value pattern\n"
               "  --ltl-positive <bool>     Keep matching traces (default: true)\n"
               "  --ltl-enable-timestamp <bool> Enable eventually-follows bounds\n"
               "  --ltl-timestamp-diff-boundaries <min:max,...> Inclusive bounds\n"
               "  --ltl-resource-key <name> Resource attribute key (default: resource)\n"
               "  --segment-prefix-activity <name> Native segment head anchor\n"
               "  --segment-prefix-strict <bool> Exclude the prefix anchor\n"
               "  --segment-prefix-occurrence <first|last> Anchor occurrence\n"
               "  --segment-suffix-activity <name> Native segment tail anchor\n"
               "  --segment-suffix-strict <bool> Include the suffix anchor\n"
               "  --segment-suffix-occurrence <first|last> Anchor occurrence\n"
               "  --segment-between-starts <a,b,...> Segment start activities\n"
               "  --segment-between-ends <a,b,...> Segment end activities\n"
               "  --segment-between-separator <text> Segment case-id separator\n"
               "  --activity-split <a,b,...> Split cases at anchor activities\n"
               "  --activity-split-separator <text> Activity-split case-id separator\n"
               "  --activity-split-cut-mode <this|next> Anchor/new-event split mode\n"
               "  --repetition-value <value> Keep cases with repeated value\n"
               "  --repetition-attribute-key <name> Attribute to count\n"
               "  --repetition-min <n>      Inclusive repetition lower bound\n"
               "  --repetition-max <n>      Inclusive repetition upper bound\n"
               "  --consecutive-case-grouping <first|last> Collapse activity runs\n"
               "  --consecutive-case-grouping-activity-key <name> Run key\n"
               "  --timestamp-case-grouping <first|last|concat> Collapse timestamps\n"
               "  --timestamp-case-grouping-activity-key <name> Activity key\n"
               "  --timestamp-case-grouping-timestamp-key <name> Timestamp key\n"
               "  --timestamp-case-grouping-separator <text> Concat separator\n"
               "  --path-predecessor <name> Adjacent-path predecessor activity\n"
               "  --path-successor <name>  Adjacent-path successor activity\n"
               "  --path-min-performance <seconds> Inclusive lower elapsed bound\n"
               "  --path-max-performance <seconds> Inclusive upper elapsed bound\n"
               "  --path-activity-attribute <key> Activity attribute key\n"
               "  --path-timestamp-attribute <key> Timestamp attribute key\n"
               "  --path-positive <bool>   Keep matching adjacent paths (default: true)\n"
               "  --spectrum <a,b,c>        Compute the performance spectrum\n"
               "                            over this activity pattern (2+\n"
               "                            steps): per occurrence, when each\n"
               "                            step happened; embedded under\n"
               "                            \"performance_spectrum\"\n"
               "  --spectrum-variant <id>  connected/disconnected or log/dataframe/lazyframe variants\n"
               "  --spectrum-sample-size <n> Non-negative spectrum sample size\n"
               "  --spectrum-sort-log <bool> Sort log before spectrum mining\n"
               "  --spectrum-activity-key <name> Activity attribute\n"
               "  --spectrum-timestamp-key <name> Timestamp attribute\n"
               "  --spectrum-case-id-key <name> Case identifier attribute\n"
               "  --concept-drift          Emit standalone concept-drift JSON\n"
               "  --drift-variant <id>    bose (default)\n"
               "  --drift-sub-log-size <n> Positive raw sub-log size\n"
               "  --drift-window-size <n> Positive comparison window size\n"
               "  --drift-num-permutations <n> Permutation count\n"
               "  --drift-threshold <f>    Threshold in [0,1]\n"
               "  --drift-max-no-change-points <n> Result cap\n"
               "  --drift-seed <n>         Non-negative RNG seed\n"
               "  --drift-include-timestamps <bool> Include timestamps\n"
               "  --drift-activity-attribute <name> Event activity selector\n"
               "  --drift-timestamp-attribute <name> Event timestamp selector\n"
               "  --drift-case-id-attribute <name> Case identifier selector\n"
               "  --browser-history <id>   Import chrome_history or firefox_history SQLite\n"
               "  --browser-history-max-events <n> Positive import bound\n"
               "  --windows-events         Import an explicitly exported event XML file\n"
               "  --windows-events-max-events <n> Positive import bound\n"
               "  --repository-history <owner/name> Import paginated issue history\n"
               "  --repository-api-base-url <url> Override the repository API origin\n"
               "  --repository-auth-token-env <name> Read bearer token from an environment variable\n"
               "  --repository-max-pages|issues|events <n> Positive safety bounds\n"
               "  --workflow-history       Import a normalized workflow-history CSV\n"
               "  --workflow-max-events <n> Positive import bound\n"
               "  --enterprise-history <order_to_cash|accounting> Import normalized enterprise CSV\n"
               "  --enterprise-max-events <n> Positive import bound\n"
               "  --communication-history <mailbox|calendar> Import an explicit communication CSV\n"
               "  --communication-* <value> Reminder, current-time, and safety controls\n"
               "  --transform-target       Emit standalone target JSON\n"
               "  --target-variant <id>    next_activity (default), next_time,\n"
               "                            or remaining_time\n"
               "  --target-activity-key <name> Event activity attribute\n"
               "  --target-timestamp-key <name> Event timestamp attribute\n"
               "  --target-enable-padding <bool> Pad rows to target size\n"
               "  --target-pad-size <n>    Padded row count\n"
               "  --target-activities <a,b,c> Explicit classes (\\, escapes commas)\n"
               "  --log-to-features         Emit standalone feature-table JSON\n"
               "  --features-variant <id>  event_based (default), trace_based, temporal\n"
               "  --features-activity-key <name> Activity attribute\n"
               "  --features-case-id-key <name> Trace identifier attribute\n"
               "  --features-timestamp-key <name> Timestamp attribute\n"
               "  --features-grouper-freq <id> Calendar temporal frequency\n"
               "                            (default: W)\n"
               "  --features-window-seconds <f> Fixed temporal bucket width\n"
               "  --embeddings              Emit case/event embeddings JSON\n"
               "  --embedding-variant <id>  cases (default) or events\n"
               "  --embedding-provider <id> hashing (default) or transformer (mock_transformer model for CLI)\n"
               "  --embedding-runtime-url <url> HTTP transformer runtime (POST /embed)\n"
               "  --embedding-model <name>  Deterministic encoder label\n"
               "  --embedding-attribute-key <name> Text attribute\n"
               "  --embedding-event-id-key <name> Optional event id attribute\n"
               "  --embedding-dimensions <n> Vector dimension (0 infers transformer width)\n"
               "  --embedding-max-items <n> Safety bound\n"
               "  --embedding-target <text> Emit cosine top-k matches\n"
               "  --embedding-top-k <n>    Match count (zero means all)\n"
               "  --embedding-keep-cases   Keep complete cases for event matches\n"
               "  --label-splitting         Emit contextual label-splitting JSON\n"
               "  --label-splitting-variant <id> contextual\n"
               "  --label-splitting-activity-key <name> Source activity attribute\n"
               "  --label-splitting-target-key <name> Destination activity attribute\n"
               "  --label-splitting-prefix-length <n> Prefix context length\n"
               "  --label-splitting-suffix-length <n> Suffix context length\n"
               "  --label-splitting-min-edge-weight <f> Similarity edge threshold\n"
               "  --label-splitting-activities-suffix <text> Replacement suffix\n"
               "  --label-splitting-target-activities <a,b> Optional target labels\n"
               "  --trie                   Emit standalone log-trie JSON\n"
               "  --trie-to-petri          Convert the discovered trie to a Petri net\n"
               "  --trie-activity-key <name> Event activity attribute\n"
               "  --trie-max-path-length <n> Positive variant path limit\n"
               "  --interval-tree          Emit open-path interval JSON\n"
               "  --interval-activity-key <name> Activity attribute\n"
               "  --interval-start-timestamp-key <name> Start-time attribute\n"
               "  --interval-timestamp-key <name> Completion-time attribute\n"
               "  --interval-epsilon <f>   Non-negative boundary epsilon\n"
               "  --interval-filter-activity-couple <a,b> Optional pair filter\n"
               "  --interval-query-point <f> Point diagnostic query\n"
               "  --interval-query-start <f> Range diagnostic lower bound\n"
               "  --interval-query-end <f> Range diagnostic upper bound\n"
               "  --reduce-petri-net <file> Reduce a PNML model and emit JSON\n"
               "  --inspect-pnml <file>      Parse a PNML model and emit JSON\n"
               "                            With --pnml <file>, also export its document metadata\n"
               "  --pnml-infer-initial-marking <bool>\n"
               "  --pnml-infer-final-marking <bool>\n"
               "  --pnml-require-workflow-boundaries <bool> (default true)\n"
               "  --pnml-import-stochastic-map <bool>\n"
               "  --petri-reduction-initial-marking <m> Comma-separated marking\n"
               "  --petri-reduction-final-marking <m> Comma-separated marking\n"
               "  --petri-reduction-parallel-place-fusion <bool>\n"
               "  --petri-reduction-abstraction <bool>\n"
               "  --merge-case-relations <file> Merge incoming CSV/XES log\n"
               "  --case-relations <file>       Relation CSV for merge\n"
               "  --merge-timestamp-key <name>  Sort timestamp attribute\n"
               "  --merge-case-id-key <name>   Case id/trace attribute selector\n"
               "  --merge-left-suffix <text>    Relation left-column suffix\n"
               "  --merge-right-suffix <text>   Relation right-column suffix\n"
               "  --merge-variant <id>          pandas (default)\n"
               "  --merge-left-column <name>    Relation left-case column\n"
               "  --merge-right-column <name>   Relation right-case column\n"
               "  --timestamp-interleavings <file> Right CSV/XES log for interleavings\n"
               "  --interleavings-timestamp-key <name> Timestamp attribute\n"
               "  --interleavings-max-results <n> Result safety bound\n"
               "  --random-tree               Emit one or more seeded random process trees\n"
               "  --random-tree-variant <id> basic or ptandloggenerator\n"
               "  --random-tree-rec-depth <n> Starting recursion depth\n"
               "  --random-tree-min-rec-depth <n> Minimum recursion depth\n"
               "  --random-tree-max-rec-depth <n> Maximum recursion depth\n"
               "  --random-tree-prob-leaf <f> Leaf probability\n"
               "  --random-tree-mode <n>    PT-and-log activity-count mode\n"
               "  --random-tree-min <n>     PT-and-log minimum activity count\n"
               "  --random-tree-max <n>     PT-and-log maximum activity count\n"
               "  --random-tree-sequence <f> PT-and-log sequence probability\n"
               "  --random-tree-choice <f>  PT-and-log choice probability\n"
               "  --random-tree-parallel <f> PT-and-log parallel probability\n"
               "  --random-tree-loop <f>    PT-and-log loop probability\n"
               "  --random-tree-or <f>      PT-and-log OR probability\n"
               "  --random-tree-silent <f>  PT-and-log silent probability\n"
               "  --random-tree-duplicate <f> PT-and-log duplicate probability\n"
               "  --random-tree-no-models <n> PT-and-log model count (zero or more)\n"
                "  --random-tree-seed <n>      Random seed\n"
                "  --random-tree-activities <a,b> Activity labels\n"
                "  --temporal-activity-attribute <name> Temporal-profile activity attribute\n"
                "  --temporal-case-id-attribute <name> Flat temporal-profile case attribute\n"
                "  --temporal-source-timestamp-attribute <name> Source completion attribute\n"
                "  --temporal-target-timestamp-attribute <name> Target start attribute\n"
                "  --temporal-business-hours <bool> Use business-hour elapsed time\n"
                "  --temporal-business-hour-slots <a-b,...|none> Weekly active slots\n"
                "  --temporal-strict-attributes <bool> Fail on missing/malformed fields\n"
                "  --batches                 Discover temporal event batches\n"
               "  --batch-variant <id>      log (default), pandas, or polars\n"
               "  --minimum-self-distance-variant <id> log (default), pandas, or polars\n"
               "  --minimum-self-distance-activity-key <name> Activity attribute\n"
               "  --minimum-self-distance-case-id-key <name> Case-id attribute\n"
               "  --overlap                 Compute interval/case/concurrent overlap statistics\n"
               "  --overlap-activity-attribute <name> Optional concurrent activity attribute\n"
               "  --overlap-start-attribute <name> Optional interval start timestamp attribute\n"
               "  --overlap-completion-attribute <name> Optional completion timestamp attribute\n"
               "  --overlap-epsilon <f>     Non-negative overlap boundary epsilon\n"
               "  --overlap-strict          Exclude endpoint-only overlap\n"
               "  --chaotic-activities      Emit entropy-based activity-chaos JSON\n"
               "  --chaotic-activity-key <name> Activity attribute\n"
               "  --chaotic-alpha <f>       Non-negative smoothing parameter\n"
               "  --chaotic-max-activities <n> Activity safety bound\n"
               "  --chaotic-max-events <n> Event safety bound\n"
               "  --passed-time              Emit passed-time statistics JSON\n"
               "  --passed-time-variant <id> pre, post, or prepost\n"
               "  --passed-time-activity <name> Target activity\n"
               "  --passed-time-activity-key <name> Activity attribute\n"
               "  --passed-time-timestamp-key <name> Completion timestamp attribute\n"
               "  --passed-time-start-timestamp-key <name> Start timestamp attribute\n"
               "  --passed-time-aggregation <id> mean, median, min, max, stdev, sum\n"
               "  --passed-time-business-hours <bool> Use business-hour elapsed time\n"
               "  --passed-time-business-hour-slots <a-b,...> Business-hour slots\n"
               "  --service-time             Emit per-activity service-time JSON\n"
               "  --service-time-activity-key <name> Activity attribute\n"
               "  --service-time-start-timestamp-key <name> Start timestamp attribute\n"
               "  --service-time-timestamp-key <name> Completion timestamp attribute\n"
               "  --service-time-aggregation <id> mean, median, min, max, or sum\n"
               "  --service-time-business-hours <bool> Use business-hour elapsed time\n"
               "  --service-time-business-hour-slots <a-b,...> Business-hour slots\n"
               "  --rework                  Emit rework statistics JSON\n"
               "  --rework-variant <id>     activities or cases\n"
               "  --rework-activity-key <name> Activity attribute\n"
               "  --rework-case-id-key <name> Case identifier attribute\n"
               "  --cycle-time              Emit cycle-time statistics JSON\n"
               "  --cycle-time-timestamp-key <name> Completion timestamp attribute\n"
               "  --cycle-time-start-timestamp-key <name> Start timestamp attribute\n"
               "  --case-arrival            Emit case-arrival/dispersion JSON\n"
               "  --case-arrival-timestamp-key <name> Timestamp attribute\n"
               "  --case-arrival-business-hours <bool> Use business-hour gaps\n"
               "  --case-arrival-business-hour-slots <a-b,...> Business-hour slots\n"
               "  --case-statistics          Emit per-case timing rows JSON\n"
               "  --case-statistics-case-id-key <name> Case identifier attribute\n"
               "  --case-statistics-timestamp-key <name> Timestamp attribute\n"
               "  --case-statistics-enable-sort <bool> Sort returned cases\n"
               "  --case-statistics-sort-by <field> case_id, start_time, end_time, duration\n"
               "  --case-statistics-sort-ascending <bool> Sort direction\n"
               "  --case-statistics-max-cases <n> Limit returned cases\n"
               "  --case-statistics-business-hours <bool> Use business-hour duration\n"
               "  --case-statistics-business-hour-slots <a-b,...> Business-hour slots\n"
               "  --attribute-statistics    Emit event/trace attribute counts JSON\n"
               "  --attribute-statistics-key <name> Attribute to count\n"
               "  --attribute-statistics-scope <events|traces> Counting scope\n"
               "  --attribute-statistics-keep-once-per-case <bool> Deduplicate per trace\n"
               "  --attribute-distribution   Emit calendar event distribution JSON\n"
               "  --attribute-distribution-type <name> days_month, months, years, hours, days_week, weeks\n"
               "  --attribute-distribution-timestamp-key <name> Timestamp attribute\n"
               "  --attribute-kde            Emit numeric/date KDE JSON\n"
               "  --attribute-kde-key <name> Attribute to sample\n"
               "  --attribute-kde-variant <numeric|date> KDE variant\n"
               "  --attribute-kde-graph-points <n> Number of plot points\n"
               "  --attribute-kde-points-to-sample <n> Date sample bound\n"
               "  --boundary-activities     Emit start/end activity counts JSON\n"
               "  --boundary-activities-variant <start|end|both> Boundary selection\n"
               "  --boundary-activities-activity-key <name> Activity attribute\n"
               "  --variant-statistics      Emit variant counts/probabilities JSON\n"
               "  --variant-statistics-activity-key <name> Activity attribute\n"
               "  --variant-statistics-max-variants <n> Limit returned variants\n"
               "  --batch-merge-distance-seconds <f> Non-negative merge distance\n"
               "  --batch-min-size <n>      Minimum batch size (default: 2)\n"
               "  --batch-activity-attribute <name> Activity selector\n"
               "  --batch-resource-attribute <name> Resource selector\n"
               "  --batch-timestamp-attribute <name> Completion timestamp selector\n"
               "  --batch-case-id-attribute <name> Case-id selector\n"
               "  --batch-start-timestamp-attribute <name> Optional start-time attribute\n"
               "  --batch-event-id-attribute <name> Optional table event identity\n"
               "  --no-alignments           Skip the standard per-model alignment\n"
               "                            section; token replay, footprints, and\n"
               "                            explicitly selected evaluations stay\n"
               "  --align-dfg              Embed classic DFG alignments\n"
               "  --dfg-max-states <n>     DFG alignment state bound\n"
               "  --dfg-max-time-ms <n>    DFG alignment time bound\n"
               "  --dfg-activity-attribute <name> Activity selector\n"
               "  --dfg-timestamp-attribute <name> Timestamp selector\n"
               "  --dfg-sync-cost a=f      Per-activity sync cost\n"
               "  --dfg-model-move-cost a=f  Per-activity model cost\n"
               "  --dfg-log-move-cost a=f  Per-activity log cost\n"
               "  --dfg-internal-log-move-cost a=f  Internal log cost\n"
               "  --dfg-projection <mode>  none, log, or metrics\n"
               "  --align-edit-distance <file>  Align against CSV/XES reference\n"
               "  --edit-activity-attribute <name> Activity selector\n"
               "  --edit-timestamp-attribute <name> Timestamp selector\n"
               "  --edit-anti              Use farthest (anti) alignment\n"
               "  --edit-deviation-cost <n>  Positive edit deviation cost\n"
               "  --edit-projection <mode>  none or log\n"
               "  --replay-fitness-model <file> Standalone token fitness for a PNML model.\n"
               "  --fitness-cleaning-token-flood <bool> Clean flooding in token fitness.\n"
               "  --evaluation-family <id> Model evaluation: replay_fitness,\n"
               "                            precision, generalization, simplicity,\n"
               "                            or composite\n"
                "  --fitness-variant <id>   auto, token_based, or alignment_based\n"
                "  --evaluation-activity-attribute <name> Evaluator activity selector\n"
                "  --evaluation-case-id-attribute <name> Evaluator case-id selector\n"
                "  --evaluation-max-rows-to-materialize <n> Flat-input row cap\n"
                "  --evaluation-initial-marking <n,...> Common evaluator initial marking\n"
               "  --evaluation-final-marking <n,...> Common evaluator final marking\n"
               "  --evaluation-token-replay-variant <id> forward or backwards\n"
               "  --fitness-consider-unknown-activities <bool> Include unknown labels in forward fitness\n"
               "  --evaluation-alignment-variant <id> Petri alignment variant or recompos_maximal\n"
               "  --evaluation-alignment-max-states <n> Positive alignment state limit\n"
               "  --evaluation-alignment-max-total-states <n> Decomposed total-state limit\n"
               "  --evaluation-alignment-max-time <f> Whole-log seconds (0 = unlimited)\n"
               "  --evaluation-alignment-max-time-trace <f> Per-trace seconds\n"
               "  --evaluation-alignment-max-alignments <n> Generator result limit\n"
               "  --evaluation-alignment-exponent <f> Discounted A* exponent\n"
               "  --evaluation-alignment-synchronous-dijkstra <bool> Discounted scheduling\n"
               "  --evaluation-alignment-cost-profile <id> standard or legacy\n"
               "  --evaluation-alignment-trace-cost <f> Default log-move cost\n"
               "  --evaluation-alignment-model-cost <f> Default model-move cost\n"
               "  --evaluation-alignment-silent-model-cost <f> Silent model cost\n"
               "  --evaluation-alignment-sync-cost <f> Synchronous move cost\n"
               "  --evaluation-alignment-threshold-border-agreement <f> Decomposed merge threshold\n"
               "  --evaluation-alignment-use-cache <bool> Cache decomposed components\n"
               "  --evaluation-alignment-clear-cache <bool> Clear decomposed call cache\n"
               "  --precision-variant <id> auto, etconformance_token,\n"
               "                            align_etconformance, or automaton_after_align\n"
               "  --precision-use-task-ids <bool> Distinguish duplicate task labels\n"
               "  --precision-max-states <n> Positive precision search limit\n"
               "  --generalization-variant <id> generalization_token\n"
               "  --simplicity-variant <id> arc_degree, extended_cardoso,\n"
               "                            or extended_cyclomatic\n"
               "  --simplicity-arc-degree-k <f> Arc-degree target (allows nan/inf)\n"
               "  --simplicity-max-states <n> Positive simplicity search limit\n"
               "  --simplicity-initial-marking <n,...> Initial place marking\n"
               "  --simplicity-final-marking <n,...> Final place marking\n"
               "  --fitness-weight <f>     Composite fitness weight (default: 0.25)\n"
               "  --precision-weight <f>   Composite precision weight (default: 0.25)\n"
               "  --simplicity-weight <f>  Composite simplicity weight (default: 0.25)\n"
               "  --generalization-weight <f> Composite generalization weight\n"
               "  --petri-analysis-family <id> marking_equation,\n"
               "                            extended_marking_equation,\n"
               "                            workflow_net, soundness, or woflan\n"
               "  --petri-analysis-miner <id> Model id (default: alpha)\n"
               "  --petri-analysis-trace-index <n> Extended-equation trace\n"
               "  --petri-analysis-initial-marking <n,...> Initial marking\n"
               "  --petri-analysis-final-marking <n,...> Final marking\n"
               "  --petri-analysis-transition-costs <n,...> Classic costs\n"
               "  --petri-analysis-model-costs <n,...> Model-move costs\n"
               "  --petri-analysis-sync-costs <n,...> Sync costs\n"
               "  --petri-analysis-log-costs <n,...> Log-move costs\n"
               "  --petri-analysis-split-indices <n,...> Extended splits\n"
               "  --petri-analysis-max-k <n> Default split count\n"
               "  --petri-analysis-max-states <n> Solver state bound\n"
               "  --petri-analysis-full-bootstrap <bool> Require bootstrap\n"
               "  --petri-analysis-diagnostics <bool> Include diagnostics\n"
               "  --petri-analysis-return-asap-when-unsound <bool> Stop at first unsound witness\n"
               "  --emd-reference <file>   Compare trace languages with CSV/XES log\n"
               "  --emd-variant <id>       pyemd (default) or exact\n"
               "  --emd-normalize-masses <bool> Normalize unequal language masses\n"
               "  --emd-strict-equal-mass <bool> Reject unequal language masses\n"
               "  --emd-use-fast-emd <bool> Select accelerated EMD when available\n"
               "  --compare-petri-reference <file> Compare Petri element usage with CSV/XES log\n"
               "  --compare-petri-model <file>    Use a supplied PNML net and its markings\n"
               "  --token-replay-model <file>     Standalone CSV/XES replay on general PNML; emit trace JSON\n"
               "  --compare-petri-miner <id>      inductive (default), alpha, heuristic, powl, or imported\n"
               "  --compare-petri-activity-attribute <name>\n"
               "                            Event attribute used as the activity label\n"
               "  --compare-petri-initial-marking <n,...> Initial place marking\n"
               "  --compare-petri-final-marking <n,...>   Final place marking\n"
               "  --compare-petri-walk-through-hidden <bool> Route silently before visible events\n"
               "  --compare-petri-reach-final-through-hidden <bool> Route silently after a trace\n"
               "  --compare-petri-stop-immediately-unfit <bool> Stop a trace after its first deviation\n"
               "  --compare-petri-cleaning-token-flood <bool> Clean excess tokens during comparison replay (default false)\n"
               "  --compare-petri-exhaustive-invisible-exploration <bool> Exhaustive hidden exploration (default false)\n"
               "  --align-process-tree     Embed process-tree alignments\n"
               "  --pt-alignment-activity-attribute <name> Activity selector\n"
               "  --pt-alignment-variant <id> SEARCH_GRAPH_PT (default),\n"
               "                            DYNAMIC_PROGRAMMING, MILP, or\n"
               "                            APPROXIMATED_ORIGINAL\n"
               "  --pt-alignment-max-states <n> State bound (0 = unlimited)\n"
               "  --pt-alignment-max-time-ms <n> Per-trace time (0 = unlimited)\n"
               "  --pt-alignment-max-total-time-ms <n> Whole-log time\n"
               "  --pt-alignment-use-global-cache <bool>\n"
               "  --pt-alignment-batch-size <n> Positive batch size\n"
               "  --pt-alignment-clear-cache <bool>\n"
               "  --pt-alignment-max-trace-length <n> Approximation threshold\n"
               "  --pt-alignment-max-process-tree-height <n> Approximation threshold\n"
               "  --pt-alignment-max-trace-time-ms <n> Approximation trace time\n"
               "  --pt-alignment-max-per-trace-time-ms <n> Approximation per-trace time\n"
               "  --alignment-variant <id> dijkstra_less_memory (default),\n"
               "                            dijkstra_no_heuristics,\n"
               "                            dijkstra_less_memory, discounted_a_star,\n"
               "                            generator_dijkstra_no_heuristics, or\n"
               "                            generator_dijkstra_less_memory\n"
               "  --alignment-max-states <n> Positive state search limit\n"
               "  --alignment-max-align-time <f> Whole-log time limit (seconds)\n"
               "  --alignment-max-align-time-trace <f> Per-trace time limit\n"
               "  --alignment-max-alignments <n> Equal-optimal generator limit\n"
               "  --alignment-exponent <f> Discounted A* exponent (>1)\n"
               "  --alignment-synchronous-dijkstra <bool> Discounted sync Dijkstra\n"
               "  --alignment-cost-profile <id> legacy (default) or standard\n"
               "  --alignment-trace-cost <f> Log move cost\n"
               "  --alignment-model-cost <f> Visible model move cost\n"
               "  --alignment-silent-model-cost <f> Silent model move cost\n"
               "  --alignment-sync-cost <f> Synchronous move cost\n"
               "  --alignment-activity-attribute <name> Event field for variants\n"
               "  --align-decomposed        Embed recompos_maximal decomposed alignment\n"
               "  --decomposed-variant <id> recompos_maximal\n"
               "  --decomposed-activity-attribute <name> Activity selector\n"
               "  --decomposed-max-states <n> State bound\n"
               "  --decomposed-max-total-states <n> Total state bound\n"
               "  --decomposed-max-time-trace <f> Per-trace seconds\n"
               "  --decomposed-max-time-total <f> Whole-log seconds\n"
               "  --decomposed-threshold-border-agreement <f>\n"
               "  --decomposed-log-move-cost <f>\n"
               "  --decomposed-model-move-cost <f>\n"
               "  --decomposed-tau-model-cost <f>\n"
               "  --decomposed-sync-cost <f>\n"
               "  --decomposed-activity-log-cost a=f\n"
               "  --decomposed-activity-model-cost a=f\n"
               "  --decomposed-activity-sync-cost a=f\n"
               "  --decomposed-initial-marking <p0,p1,...>\n"
               "  --decomposed-final-marking <p0,p1,...>\n"
               "  --decomposed-use-cache <bool>  --decomposed-clear-cache <bool>\n"
               "  --align-petri-anti         Embed version_discounted_a_star anti-alignment\n"
               "  --align-petri-multi        Embed version_discounted_a_star multi-alignment\n"
               "  --advanced-petri-miner <id> inductive, alpha, heuristic, powl, or imported\n"
               "  --petri-activity-attribute <name> Activity selector for anti/multi\n"
               "  --petri-exponent <f>        Discount exponent (>1)\n"
               "  --petri-epsilon <f>         Anti-alignment epsilon\n"
               "  --petri-marking-limit <n>   Positive marking bound\n"
               "  --petri-max-states <n>      Zero uses the marking limit\n"
               "  --petri-initial-marking <p0,p1,...>\n"
               "  --petri-final-marking <p0,p1,...>\n"
               "  --reduce-process-tree <n>   Embed trace-based reduction for trace index\n"
               "  --process-tree-reduction-activity-key <name>\n"
               "  --token-replay-variant <id> Token replay: forward (default)\n"
               "                            or backwards\n"
               "  --token-replay-result      Full forward result as tagged object JSON\n"
               "  --token-replay-places-shortest-path-by-hidden <json>\n"
               "                            Place-ID path matrix; {} disables, null computes\n"
               "  --token-replay-enable-pltr-fitness <bool>\n"
               "                            Include place/transition/context maps\n"
               "  --token-replay-disable-variants <bool>\n"
               "                            Retain source traces instead of activity variants\n"
               "  --token-replay-return-names <bool>\n"
               "                            Project trace markings and transitions to names\n"
               "  --token-replay-consider-remaining-in-fitness <bool>\n"
               "                            Include leftover tokens in fit classification\n"
               "  --token-replay-try-to-reach-final-marking-through-hidden <bool>\n"
               "                            Route silently to the final marking\n"
               "  --token-replay-consider-activities-not-in-model-in-fitness <bool>\n"
               "                            Count unknown activities as deviations\n"
               "  --token-replay-walk-through-hidden-transitions <bool>\n"
               "                            Use silent routing between visible events\n"
               "  --token-replay-cleaning-token-flood <bool>\n"
               "                            Clean stale markings after forced forward firings\n"
               "  --token-replay-exhaustive-invisible-exploration <bool>\n"
               "                            Select exhaustive hidden exploration (default false)\n"
               "  --token-replay-include-trace-diagnostics <bool>\n"
               "                            Include per-case replay details in JSON\n"
               "  --token-replay-include-duration-diagnostics <bool>\n"
               "                            Compare case durations by replay deviation\n"
               "  --token-replay-timestamp-attribute <name> Case-duration timestamp field\n"
               "  --token-replay-include-root-cause-diagnostics <bool>\n"
               "                            Train attribute explanations for replay deviations\n"
               "  --token-replay-root-cause-string-attributes <names> Comma-separated categories\n"
               "  --token-replay-root-cause-numeric-attributes <names> Comma-separated numeric fields\n"
               "  --token-replay-root-cause-enable-multiplier <bool> Repeat the smaller cohort\n"
               "  --token-replay-case-id-attribute <name> Flat-input case selector\n"
               "  --token-replay-max-rows-to-materialize <n> Replay event cap\n"
               "  --token-replay-stop-immediately-unfit\n"
               "                            Stop a backwards replay trace at its\n"
               "                            first event that cannot be enabled\n"
               "  --token-replay-activity-attribute <name> Event field for variants\n"
               "  --log-skeleton-noise-threshold <f> Log-skeleton noise in [0,1]\n"
               "  --log-skeleton-activity-attribute <name> Activity selector\n"
               "  --log-skeleton-constraints <a,b,...> Constraint families:\n"
               "                            equivalence, always_after, always_before,\n"
               "                            never_together, directly_follows, activ_freq\n"
               "  --declare-considered-activities <a,b,...> Declare activity filter\n"
               "                            (omit = infer; empty = use none)\n"
               "  --declare-activity-attribute <name> Activity selector\n"
               "  --declare-allowed-templates <a,b,...> Declare template families\n"
               "                            (omit = all; empty = use none)\n"
               "  --declare-min-support-ratio <f> Declare support ratio in [0,1]\n"
               "  --declare-min-confidence-ratio <f> Declare confidence ratio in [0,1]\n"
               "  --declare-auto-selection-multiplier <f> Declare auto-selection multiplier in [0,1]\n"
               "  --footprints-discovery-variants <a,b,...> Footprint discovery:\n"
               "                            entire_event_log, entire_dataframe,\n"
               "                            polars_lazyframes, trace_by_trace, dfg,\n"
               "                            process_tree, powl, petri_reach_graph\n"
               "  --footprints-model <id>  process_tree (default), powl,\n"
               "                            inductive, alpha, heuristic, or dfg\n"
               "  --footprints-activity-attribute <name> Activity selector\n"
               "  --footprints-case-id-attribute <name> Case-id selector\n"
               "  --footprints-start-timestamp-attribute <name> Optional start timestamp\n"
               "  --footprints-timestamp-attribute <name> Completion timestamp\n"
               "  --footprints-index-attribute <name> Compatibility row-index selector\n"
               "  --footprints-sort-required <bool> Sort table rows (default true)\n"
               "  --footprints-initial-marking <n,...> Explicit Petri initial marking\n"
               "  --footprints-conformance-variants <a,b,...> Footprint checks:\n"
               "                            log_model, log_extensive, trace_extensive\n"
               "  --footprints-strict <bool> Strict log-model footprint checking\n"
               "  --footprints-enable-act-always-executed <bool>\n"
               "                            Check always-executed trace activities\n"
               "  --dfg-statistical-variants <a,b,...> Statistical DFG analyses:\n"
               "                            native, frequency, frequency_greedy,\n"
               "                            frequency_triples, performance,\n"
               "                            performance_greedy, case_attributes,\n"
               "                            clean, clean_polars, clean_time\n"
               "  --dfg-causal-variants <a,b> Causality: alpha, heuristic\n"
               "  --dfg-window <n>          Frequency window, including 0\n"
               "  --dfg-keep-once-per-case Count each frequency edge once/case\n"
               "  --dfg-performance-aggregation <id> mean, median, min, max,\n"
               "                            stdev, sum, raw_values, or all\n"
               "  --dfg-activity-attribute <name> Use a custom event activity field\n"
               "  --dfg-timestamp-attribute <name> Use a custom event timestamp field\n"
               "  --dfg-start-timestamp-attribute <name> Performance start time\n"
               "  --dfg-business-hours     Measure performance in business time\n"
               "  --dfg-business-hour-slots <start-end,...> Weekly seconds\n"
               "  --dfg-case-attributes <a,b,...> Attributes for case DFG\n"
               "  --dfg-return-node-attributes Include case attributes on nodes\n"
               "  --dfg-dataframe-output <path> Write dataframe DFG JSON\n"
               "  --dfg-operation <id>      graph, partial_order, or concurrent_events\n"
               "  --dfg-measure <id>        frequency, performance, both, or cost\n"
               "  --dfg-backend <id>        pandas or polars\n"
               "  --dfg-target-activity-attribute <name> Target-side activity field\n"
               "  --dfg-cost-attribute <name> Cost field for the cost measure\n"
               "  --dfg-case-id-attribute <name> Case-id field for table rows\n"
               "  --dfg-sort-caseid-required <bool> Sort rows by case (default true)\n"
               "  --dfg-sort-timestamp-along-case-id <bool> Timestamp tie-breaker\n"
               "  --dfg-reduce-columns <bool> Preserve/reduce table columns\n"
               "  --dfg-workcalendar <token> Opaque work-calendar token\n"
               "  --dfg-event-index-attribute <name> Pair-row index field\n"
               "  --dfg-max-start-column <name> Concurrent max-start field\n"
               "  --dfg-min-complete-column <name> Concurrent min-complete field\n"
               "  --dfg-overlap-column <name> Concurrent overlap field\n"
               "  --dfg-strict <bool>       Require strictly positive overlap\n"
               "  --dfg-keep-first-following <bool> Keep first following pair\n"
               "  --correlation-variants <a,b,...> Correlation miner analyses:\n"
               "                            classic, classic_split, trace_based\n"
               "  --correlation-activity-attribute <name> Activity field\n"
               "  --correlation-timestamp-attribute <name> Completion time field\n"
               "  --correlation-start-timestamp-attribute <name> Correlation start time\n"
               "  --correlation-case-id-attribute <name> Case-id field\n"
               "  --correlation-exact-time-matching <bool> Exact timestamp matching\n"
               "                            (true, false, 1, or 0)\n"
               "  --correlation-sample-size <n> Positive correlation sample size\n"
               "  --decisions <miner>       Mine the decision points of one\n"
               "                            model (inductive, alpha, heuristic,\n"
               "                            or powl): which attribute values\n"
               "                            steer a case down which branch;\n"
               "                            embedded under \"decisions\"\n"
               "  --decision-tables <miner> Mine encoded decision tables for one\n"
               "                            model (inductive, alpha, heuristic,\n"
               "                            or powl); embedded under\n"
               "                            \"decision_tables\"\n"
               "  --decision-max-depth <n> Decision tree depth (0..64)\n"
               "  --decision-min-split <n> Minimum observations before a split\n"
               "  --decision-min-leaf <n>  Minimum observations per child\n"
               "  --decision-max-categories <n> Equality candidates per key\n"
               "  --decision-max-thresholds <n> Numeric threshold candidates\n"
               "  --decision-min-gain <f>  Minimum Gini gain\n"
               "  --decision-no-trace-attributes Exclude case attributes\n"
               "  --decision-attributes <a,b,...> Restrict context keys\n"
               "  --decision-activity-attribute <name> Event field for replay\n"
               "  --decision-lookback <n>  Prior context snapshots for tables (1..64)\n"
               "  --playout <path>          Simulate a discovered model to XES\n"
               "  --playout-miner <id>      Model for --playout: inductive\n"
               "                            (default), alpha, heuristic, powl,\n"
               "                            imported, declare, process_tree, dfg,\n"
               "                            stochastic, or monte_carlo\n"
               "  --playout-variant <id>   Process-tree variant: TOPBOTTOM (default),\n"
               "                            BASIC_PLAYOUT, EXTENSIVE, or stochastic\n"
               "  --playout-traces <n>     Process-tree sampled cases\n"
               "  --playout-max-steps <n> Visible sampling step cap\n"
               "  --playout-seed <u64>     Process-tree random seed\n"
               "  --playout-min-length <n> Minimum extensive trace length\n"
               "  --playout-max-length <n> Maximum extensive trace length\n"
               "  --playout-max-loop-occurrences <n> Loop re-do limit\n"
               "  --playout-max-traces <n> Extensive trace cap\n"
               "  --playout-max-trace-cap <n> Alias for max-traces\n"
               "  --playout-return-set-strings <bool> Extensive activity-word JSON\n"
               "  --petri-playout-traces <n> Petri sampled cases\n"
               "  --petri-playout-max-steps <n> Petri firing cap per case\n"
               "  --petri-playout-seed <u64> Petri random seed\n"
               "  --petri-playout-initial-marking <n,...> Initial marker vector\n"
               "  --petri-playout-final-marking <n,...> Final marker vector\n"
               "  --petri-playout-start <f> First event epoch seconds\n"
               "  --petri-playout-case-interval <f> Case start interval\n"
               "  --petri-playout-event-interval <f> Event interval\n"
               "  --petri-playout-add-only-if-fm-is-reached <bool> Keep accepted cases only\n"
               "  --petri-playout-fm-leq-accepted <bool> Accept final-marking supersets\n"
               "  --petri-playout-return-visited-elements <bool> Emit visited markings/transitions as JSON\n"
               "  --petri-playout-mode <basic|extensive> Petri playout algorithm\n"
               "  --petri-playout-max-length <n> Extensive visible trace bound\n"
               "  --petri-playout-return-elements <bool> Extensive visited JSON\n"
               "  --petri-playout-max-marking-occ <n> Extensive marking repetition bound\n"
               "  --stochastic-traces <n>  Weighted Petri cases\n"
               "  --stochastic-max-steps <n> Firing cap per case\n"
               "  --stochastic-seed <u64>  Weighted Petri random seed\n"
               "  --stochastic-transition-weights <w,...> Transition weights\n"
               "  --stochastic-fit-from-log <bool> Fit weights from input log\n"
               "  --stochastic-stop-at-final <bool> Stop at final marking\n"
               "  --stochastic-return-visited-elements <bool> Emit visited markings/transitions as JSON\n"
               "  --stochastic-initial-marking <n,...> Initial marker vector\n"
               "  --stochastic-final-marking <n,...> Final marker vector\n"
               "  --stochastic-petri <bool> Emit tangible CTMC JSON with --playout\n"
               "  --stochastic-petri-rates <r,...> Exponential transition rates\n"
               "  --stochastic-petri-initial-marking <n,...> Initial marking\n"
               "  --stochastic-petri-immediate-transitions <n,...> Immediate indices\n"
               "  --stochastic-petri-max-states <n> Reachability state cap\n"
               "  --stochastic-petri-analysis <name> reachability, transient, or steady_state\n"
               "  --stochastic-petri-time <f> Transient time in seconds\n"
               "  --stochastic-petri-initial-state <n> Transient source state\n"
               "  --stochastic-petri-initial-probabilities <p,...> Transient vector\n"
               "  --stochastic-map <path> Emit a fitted stochastic-map JSON document\n"
               "  --stochastic-map-source <name> log or performance_dfg\n"
               "  --stochastic-map-miner <id> Petri miner or imported model for log fitting\n"
               "  --stochastic-map-distribution <name> Distribution family\n"
               "  --stochastic-map-default-parameter <f> Default duration/rate\n"
               "  --stochastic-map-conversion-variant <name> DFG Petri conversion\n"
               "  --stochastic-map-max-states <n> CTMC state cap\n"
               "  --monte-carlo-simulations <n> Virtual-time cases\n"
               "  --monte-carlo-activity-key <name> Generated activity attribute\n"
               "  --monte-carlo-timestamp-key <name> Generated timestamp attribute\n"
               "  --monte-carlo-initial-marking <n,...> Initial marker vector\n"
               "  --monte-carlo-final-marking <n,...> Final marker vector\n"
               "  --monte-carlo-max-steps <n> Firing cap per case\n"
               "  --monte-carlo-seed <u64> RNG seed\n"
               "  --monte-carlo-case-arrival-ratio <f> Arrival ratio\n"
               "  --monte-carlo-case-arrival-seconds <f> Case arrival interval\n"
               "  --monte-carlo-start <f> First case arrival epoch seconds\n"
               "  --monte-carlo-default-duration-seconds <f> Default duration\n"
               "  --monte-carlo-force-distribution <name> Fitted duration distribution\n"
               "  --monte-carlo-transition-weights <w,...> Transition weights\n"
               "  --monte-carlo-transition-durations <d:v,...> Durations\n"
               "  --monte-carlo-resources-per-place <p=n,...> Resource pools\n"
               "  --monte-carlo-default-resources-per-place <n> Default pool size\n"
               "  --monte-carlo-enable-diagnostics <bool> Include diagnostics\n"
               "  --monte-carlo-stop-at-final <bool> Stop at final marking\n"
               "  --monte-carlo-provided-stochastic-map <records|empty> Explicit map\n"
               "  --monte-carlo-diagnostic-interval <n> Diagnostic cadence\n"
               "  --monte-carlo-small-scale-factor <f> Timing scale factor\n"
               "  --monte-carlo-max-thread-execution-time <f> Runtime cap\n"
               "  --monte-carlo-token-replay-variant <forward|backwards> Replay mode\n"
               "  --monte-carlo-json        Emit Monte Carlo diagnostics as JSON\n"
               "  --dfg-playout-variant <name> classic or performance\n"
               "  --dfg-playout-traces <n> DFG generated cases\n"
               "  --dfg-playout-max-events-per-trace <n> Event cap\n"
               "  --dfg-playout-max-variants <n> Variant cap\n"
               "  --dfg-playout-min-weighted-probability <f> Probability floor\n"
               "  --dfg-playout-max-occurrences-per-activity <n> Activity cap\n"
               "  --dfg-playout-max-execution-time-ms <n> Time cap\n"
               "  --dfg-playout-case-arrival-rate <f> Performance arrival rate\n"
               "  --dfg-playout-seed <u64> DFG random seed\n"
               "  --dfg-playout-start-time <iso8601> DFG start timestamp\n"
               "  --dfg-playout-activity-key <key> Generated activity attribute\n"
               "  --dfg-playout-timestamp-key <key> Generated timestamp attribute\n"
               "  --dfg-playout-case-id-key <key> Generated case-id attribute\n"
               "  --dfg-playout-artificial-start-activity <label> Boundary label\n"
               "  --dfg-playout-artificial-end-activity <label> Boundary label\n"
               "  --declare-playout-traces <n> Declare cases (default: 1000)\n"
               "  --declare-playout-min-length <n> Minimum events (default: 3)\n"
               "  --declare-playout-max-length <n> Maximum events (default: 15)\n"
               "  --declare-playout-seed <u64> Deterministic Declare seed\n"
               "\n  Log filtering (applied before discovery):\n"
               "  --time-from <iso8601>     Keep only behaviour at/after this instant\n"
               "  --time-to <iso8601>       Keep only behaviour at/before this instant\n"
                "  --time-mode <mode>        How the time range reads a trace:\n"
                "                            events (default: keep events in range),\n"
                "                            traces_contained, or traces_intersecting\n"
                "  --case-duration-min <s>  Inclusive lower complete-case duration\n"
                "  --case-duration-max <s>  Inclusive upper complete-case duration\n"
               "  --max-cases <n>           Keep the first n surviving cases\n"
               "  --max-activities <n>     Keep events from the n most frequent\n"
               "                            activities (0 drops all events)\n"
               "  --top-activities <n>     Alias for --max-activities\n"
               "  --top-activities-attribute-key <k>\n"
               "                            Rank a generic event attribute\n"
               "  --relative-attribute-stake <f>\n"
               "                            Keep values in at least this share\n"
               "                            of cases (or events)\n"
               "  --relative-attribute-key <k>\n"
               "                            Attribute for relative occurrence\n"
               "  --relative-attribute-keep-once-per-case <bool>\n"
               "                            Count each value once per case\n"
               "  --attribute-occurrence-key <k>\n"
               "                            Absolute-frequency attribute (blank = activity)\n"
               "  --attribute-occurrence-min <n> Keep values seen at least n times\n"
               "  --start-activities <a,b>  Keep traces starting with one of these\n"
               "  --start-activities-positive <bool> Keep (or exclude) listed starts\n"
               "  --end-activities <a,b>    Keep traces ending with one of these\n"
               "  --end-activities-positive <bool> Keep (or exclude) listed ends\n"
               "  --min-length <n>          Drop traces shorter than n events\n"
               "  --max-length <n>          Drop traces longer than n events\n"
               "  --keep-activities <a,b>   Keep only these activities\n"
               "  --remove-activities <a,b> Drop these activities\n"
               "  --keep-resources <a,b>    Keep only events by these resources\n"
               "  --remove-resources <a,b>  Drop events by these resources\n"
               "  --keep-lifecycles <a,b>   Keep only events with one of these\n"
               "                            lifecycle transitions (e.g.\n"
               "                            complete, to make a start/complete\n"
               "                            log atomic for discovery)\n"
               "  --remove-lifecycles <a,b> Drop events with these transitions\n"
               "  --attribute-key <k>       Filter on this event attribute (an\n"
               "                            extra CSV column or non-standard\n"
               "                            XES key); alone it keeps only\n"
               "                            events carrying the attribute\n"
               "  --keep-attribute-values <a,b>   Keep only events whose\n"
               "                            attribute has one of these values\n"
               "  --remove-attribute-values <a,b> Drop events with these values\n"
               "  --attribute-mode <mode>   events (default: filter events) or\n"
               "                            traces (keep/drop whole cases by\n"
               "                            the same attribute match)\n"
               "  --case-attribute-key <k>  Filter cases on this case attribute\n"
               "                            (an XES trace attribute or a\n"
               "                            `case:` CSV column); alone it keeps\n"
               "                            only cases carrying the attribute\n"
               "  --keep-case-attribute-values <a,b>   Keep only cases whose\n"
               "                            attribute has one of these values\n"
               "  --remove-case-attribute-values <a,b> Drop cases with these\n"
               "                            values\n"
               "  --numeric-attribute-key <k> Numeric event attribute to range-filter\n"
               "  --numeric-attribute-min <f> Inclusive numeric lower bound\n"
               "  --numeric-attribute-max <f> Inclusive numeric upper bound\n"
               "  --numeric-attribute-mode <mode> events (default) or traces\n"
               "  --numeric-attribute-positive <bool> Keep in-range values/cases\n"
               "  --rework-activity <a>     Keep only traces where <a> repeats\n"
               "  --rework-min <n>          Repetitions that count as rework\n"
               "                            (default: 2)\n"
               "  --keep-directly-follows <a->b,c->d>\n"
               "                            Keep only cases where some listed\n"
               "                            activity is directly followed by\n"
               "                            its partner\n"
               "  --remove-directly-follows <a->b,...>  Drop such cases\n"
               "  --keep-eventually-follows <a->b,...>  Like the directly-\n"
               "                            follows pair, but the partner may\n"
               "                            occur anywhere later in the case\n"
                "  --remove-eventually-follows <a->b,...>  Drop such cases\n"
                "  --trace-patterns <p1;p2> Keep traces matching an exact/prefix/\n"
                "                            suffix/infix pattern; use ... as a wildcard\n"
                "  --trace-pattern-positive <bool> Keep matching (default: true)\n"
                "  --admitted-variants <v1;v2> Keep/drop exact variants; comma-\n"
                "                            separate activities, [] means empty\n"
                "  --admitted-variants-positive <bool> Keep (default) or drop\n"
                "                            admitted variants\n"
               "  --prefix-activity <a>     Keep each case's head up to (and\n"
               "                            including) the first <a>; cases\n"
               "                            without <a> are dropped\n"
               "  --prefix-strict <bool>   Include the prefix anchor (default true)\n"
               "  --prefix-occurrence <id> first (default) or last anchor\n"
               "  --suffix-activity <a>     Keep each case's tail from the\n"
               "                            first <a> on; cases without <a>\n"
               "                            are dropped\n"
               "  --suffix-strict <bool>   Include the suffix anchor (default true)\n"
               "  --suffix-occurrence <id> first (default) or last anchor\n"
               "  --between-start <a>       With --between-end: cut each trace\n"
               "  --between-end <b>         into the subtraces running from <a>\n"
               "                            to <b>, endpoints included\n"
               "  --min-variant-frequency <n>  Keep variants seen at least n times\n"
               "  --top-variants <n>        Keep only the n most frequent variants\n"
               "  --variant-coverage <f>    Keep the most frequent variants\n"
               "                            that together cover this share of\n"
               "                            cases, f in (0,1]\n"
               "  --variant-min-coverage <f> Keep variants with at least this\n"
               "                            individual case share, f in [0,1]\n"
               "  --variant-max-coverage <f> Keep variants with at most this\n"
               "                            individual case share, f in [0,1]\n"
               "  --variant-percentage <f>  Keep top variants covering f cases;\n"
               "                            ties at the boundary are retained\n";
  return 2;
}

minera::DfgPlayoutResult generate_dfg_playout(
    const minera::EventLog& log, const minera::DfgPlayoutOptions& options) {
  const auto frequency = minera::discover_frequency_dfg(log);
  minera::DfgActivityCounts starts, ends;
  for (const auto& trace : log.traces) {
    if (!trace.events.empty()) {
      ++starts[trace.events.front().activity];
      ++ends[trace.events.back().activity];
    }
  }
  std::optional<minera::DfgPerformance> performance;
  if (options.variant == minera::DfgPlayoutVariant::kPerformance)
    performance = minera::discover_performance_dfg(log);
  return minera::dfg_playout_result(frequency, starts, ends, options,
                                    performance ? &*performance : nullptr);
}

enum class PowlCliParseStatus {
  kNotMatched,
  kParsed,
  kInvalid,
  kMissingValue,
};

PowlCliParseStatus parse_powl_cli_option(
    std::string_view arg, int argc, char** argv, int& index,
    minera::PowlDiscoveryOptions& options) {
  const bool is_selector = arg == "--powl-activity-key" ||
                           arg == "--powl-timestamp-key" ||
                           arg == "--powl-case-id-key";
  const bool matched = arg == "--powl-variant" ||
                       arg == "--powl-filtering-type" ||
                       arg == "--powl-filtering-threshold" ||
                       arg == "--powl-order-frequency-ratio" ||
                       arg == "--powl-brute-force-activity-limit" ||
                       is_selector;
  if (!matched) return PowlCliParseStatus::kNotMatched;
  if (index + 1 >= argc) return PowlCliParseStatus::kMissingValue;
  const char* value = argv[++index];

  if (arg == "--powl-variant") {
    const std::string_view variant = value;
    if (variant == "tree")
      options.variant = minera::PowlDiscoveryVariant::kTree;
    else if (variant == "brute_force")
      options.variant = minera::PowlDiscoveryVariant::kBruteForce;
    else if (variant == "maximal")
      options.variant = minera::PowlDiscoveryVariant::kMaximal;
    else if (variant == "dynamic_clustering")
      options.variant = minera::PowlDiscoveryVariant::kDynamicClustering;
    else if (variant == "dynamic_clustering_plain")
      options.variant = minera::PowlDiscoveryVariant::kDynamicClusteringPlain;
    else {
      std::cerr << "error: --powl-variant expects tree, brute_force, "
                   "maximal, dynamic_clustering, or dynamic_clustering_plain\n";
      return PowlCliParseStatus::kInvalid;
    }
    return PowlCliParseStatus::kParsed;
  }

  if (arg == "--powl-filtering-type") {
    const std::string_view type = value;
    if (type == "decreasing_factor")
      options.filtering_type = minera::PowlFilteringType::kDecreasingFactor;
    else if (type == "dynamic")
      options.filtering_type = minera::PowlFilteringType::kDynamic;
    else {
      std::cerr << "error: --powl-filtering-type expects decreasing_factor or "
                   "dynamic\n";
      return PowlCliParseStatus::kInvalid;
    }
    return PowlCliParseStatus::kParsed;
  }

  if (arg == "--powl-filtering-threshold" ||
      arg == "--powl-order-frequency-ratio") {
    char* end = nullptr;
    const double parsed = std::strtod(value, &end);
    if (end == value || *end != '\0' || !std::isfinite(parsed)) {
      std::cerr << "error: " << arg << " expects a finite number\n";
      return PowlCliParseStatus::kInvalid;
    }
    if (arg == "--powl-filtering-threshold") {
      if (parsed < 0.0 || parsed >= 1.0) {
        std::cerr
            << "error: --powl-filtering-threshold expects 0 <= f < 1\n";
        return PowlCliParseStatus::kInvalid;
      }
      options.filtering_threshold = parsed;
    } else {
      if (parsed <= 0.5 || parsed > 1.0) {
        std::cerr
            << "error: --powl-order-frequency-ratio expects 0.5 < f <= 1\n";
        return PowlCliParseStatus::kInvalid;
      }
      options.order_frequency_ratio = parsed;
    }
    return PowlCliParseStatus::kParsed;
  }

  if (arg == "--powl-brute-force-activity-limit") {
    std::uint64_t parsed = 0;
    const char* end = value + std::strlen(value);
    const auto result = std::from_chars(value, end, parsed);
    if (result.ec != std::errc{} || result.ptr != end ||
        parsed > std::numeric_limits<std::size_t>::max()) {
      std::cerr << "error: --powl-brute-force-activity-limit expects a "
                   "non-negative integer\n";
      return PowlCliParseStatus::kInvalid;
    }
    options.brute_force_activity_limit = static_cast<std::size_t>(parsed);
    return PowlCliParseStatus::kParsed;
  }

  const auto name = trim_text(value);
  if (name.empty()) {
    std::cerr << "error: " << arg << " expects a non-empty name\n";
    return PowlCliParseStatus::kInvalid;
  }
  if (arg == "--powl-activity-key")
    options.activity_key = name;
  else if (arg == "--powl-timestamp-key")
    options.timestamp_key = name;
  else
    options.case_id_key = name;
  return PowlCliParseStatus::kParsed;
}

// Parse the footprint table/Petri selectors outside the large command-line
// dispatch function. Keeping this small option family in a helper avoids
// pushing the MSVC parser's nesting limit as more CLI surfaces are added.
bool parse_footprints_extra_option(std::string_view arg, const char* value,
                                   minera::FootprintsOptions& options,
                                   std::string& error) {
  const auto selector_error = [&] {
    error = std::string(arg) + " expects a non-empty selector";
    return true;
  };
  if (arg == "--footprints-activity-attribute" ||
      arg == "--footprints-case-id-attribute" ||
      arg == "--footprints-timestamp-attribute") {
    const auto selector = trim_text(value);
    if (selector.empty()) return selector_error();
    if (arg == "--footprints-activity-attribute")
      options.activity_attribute = selector;
    else if (arg == "--footprints-case-id-attribute")
      options.case_id_attribute = selector;
    else
      options.timestamp_attribute = selector;
    return true;
  }

  if (arg == "--footprints-index-attribute") {
    options.index_attribute = trim_text(value);
    return true;
  }

  if (arg == "--footprints-start-timestamp-attribute") {
    const auto selector = trim_text(value);
    if (selector.empty())
      options.start_timestamp_attribute.reset();
    else
      options.start_timestamp_attribute = selector;
    return true;
  }

  if (arg == "--footprints-sort-required") {
    const std::string_view boolean = value;
    if (boolean == "true" || boolean == "1") {
      options.sort_required = true;
    } else if (boolean == "false" || boolean == "0") {
      options.sort_required = false;
    } else {
      error = std::string(arg) + " expects true, false, 1, or 0";
    }
    return true;
  }

  if (arg == "--footprints-initial-marking") {
    const std::string text = value;
    if (text.empty()) {
      error = std::string(arg) + " expects a non-empty marking";
      return true;
    }
    std::vector<std::uint64_t> marking;
    std::size_t begin = 0;
    for (std::size_t index = 0; index <= text.size(); ++index) {
      if (index != text.size() && text[index] != ',') continue;
      const auto token = trim_text(text.substr(begin, index - begin));
      if (token.empty()) {
        error = std::string(arg) + " expects non-empty uint64 entries";
        return true;
      }
      std::uint64_t parsed = 0;
      const auto conversion = std::from_chars(
          token.data(), token.data() + token.size(), parsed);
      if (conversion.ec != std::errc{} ||
          conversion.ptr != token.data() + token.size()) {
        error = std::string(arg) + " expects non-negative uint64 entries";
        return true;
      }
      marking.push_back(parsed);
      begin = index + 1;
    }
    options.initial_marking = std::move(marking);
    return true;
  }
  return false;
}

minera::MinimumSelfDistances discover_selected_minimum_self_distances(
    const minera::EventLog& log,
    const minera::MinimumSelfDistanceOptions& options) {
  switch (options.variant) {
    case minera::MinimumSelfDistanceVariant::kLog:
      return minera::MinimumSelfDistanceLogDiscovery::apply(log, options);
    case minera::MinimumSelfDistanceVariant::kPandas:
      return minera::MinimumSelfDistancePandasDiscovery::apply(
          minera::event_log_to_table(log), options);
    case minera::MinimumSelfDistanceVariant::kPolars:
      return minera::MinimumSelfDistancePolarsDiscovery::apply(
          minera::event_log_to_lazy_table(log), options);
  }
  throw std::invalid_argument("minimum self-distance variant is invalid");
}

minera::PerformanceSpectrum discover_selected_performance_spectrum(
    const minera::EventLog& log, const std::vector<std::string>& activities,
    const minera::PerformanceSpectrumOptions& options) {
  switch (options.variant) {
    case minera::PerformanceSpectrumVariant::kDataframe:
      return minera::discover_performance_spectrum_dataframe(
          minera::event_log_to_table(log), activities, options);
    case minera::PerformanceSpectrumVariant::kDataframeDisconnected:
      return minera::discover_performance_spectrum_dataframe_disconnected(
          minera::event_log_to_table(log), activities, options);
    case minera::PerformanceSpectrumVariant::kLazyframe:
      return minera::discover_performance_spectrum_lazyframe(
          minera::event_log_to_lazy_table(log), activities, options);
    case minera::PerformanceSpectrumVariant::kLazyframeDisconnected:
      return minera::discover_performance_spectrum_lazyframe_disconnected(
          minera::event_log_to_lazy_table(log), activities, options);
    case minera::PerformanceSpectrumVariant::kConnected:
    case minera::PerformanceSpectrumVariant::kDisconnected:
    case minera::PerformanceSpectrumVariant::kLog:
    case minera::PerformanceSpectrumVariant::kLogDisconnected:
      return minera::compute_performance_spectrum(log, activities, options);
  }
  throw std::invalid_argument("performance spectrum variant is invalid");
}

minera::BatchDiscovery discover_selected_batches(
    const minera::EventLog& log, const minera::BatchDiscoveryOptions& options) {
  switch (options.variant) {
    case minera::BatchDiscoveryVariant::kLog:
      return minera::BatchLogDiscovery::apply(log, options);
    case minera::BatchDiscoveryVariant::kPandas:
      return minera::BatchPandasDiscovery::apply(
          minera::event_log_to_table(log), options);
    case minera::BatchDiscoveryVariant::kPolars:
      return minera::BatchPolarsDiscovery::apply(
          minera::event_log_to_lazy_table(log), options);
  }
  throw std::invalid_argument("batch discovery variant is invalid");
}

}  // namespace

int main(int argc, char** argv) {
  std::filesystem::path input;
  std::filesystem::path output;
  std::filesystem::path pnml_output;
  std::filesystem::path check_pnml;
  bool pnml_public_markings = false;
  std::filesystem::path check_bpmn;
  std::filesystem::path bpmn_output;
  std::filesystem::path ptml_output;
  std::filesystem::path powl_ptml_output;
  std::filesystem::path pnml_to_ptml_input;
  std::filesystem::path petri_reduction_input;
  std::map<std::string, std::string> petri_reduction_fields;
  std::filesystem::path pnml_inspection_input;
  std::map<std::string, std::string> pnml_inspection_fields;
  std::filesystem::path bpmn_to_ptml_input;
  std::filesystem::path check_ptml;
  std::filesystem::path xes_output;
  std::filesystem::path dfg_output;
  std::filesystem::path dfg_dataframe_output;
  std::filesystem::path playout_output;
  bool log_conversion_requested = false;
  std::map<std::string, std::string> log_conversion_fields;
  std::string pnml_miner = "inductive";
  std::string bpmn_miner = "inductive";
  minera::ProcessTreePetriNetVariant process_tree_variant =
      minera::ProcessTreePetriNetVariant::kStandard;
  std::string playout_miner = "inductive";
  std::string decisions_miner;  // empty = no decision mining
  std::string decision_tables_miner;  // empty = no decision-table mining
  minera::DecisionMiningOptions decision_options;
  std::size_t decision_lookback = 1;
  bool decision_lookback_used = false;
  bool decision_option_used = false;
  bool decision_activity_option_used = false;
  bool no_alignments = false;
  bool align_process_tree = false;
  std::map<std::string, std::string> pt_alignment_fields;
  minera::ProcessTreeAlignmentOptions pt_alignment_options;
  bool align_dfg = false;
  std::string dfg_projection = "none";
  minera::DfgAlignmentOptions dfg_alignment_options;
  std::filesystem::path edit_reference;
  bool edit_anti = false;
  bool edit_option_used = false;
  std::string edit_projection = "none";
  minera::EditDistanceOptions edit_options;
  std::map<std::string, std::string> evaluation_fields;
  bool evaluation_requested = false;
  std::optional<minera::EvaluationParams> evaluation_params;
  std::map<std::string, std::string> petri_analysis_fields;
  bool petri_analysis_requested = false;
  std::filesystem::path emd_reference;
  std::filesystem::path compare_petri_reference;
  std::filesystem::path compare_petri_model;
  std::filesystem::path token_replay_model;
  bool token_replay_result_requested = false;
  bool replay_fitness_result_requested = false;
  std::string compare_petri_miner = "inductive";
  bool compare_petri_miner_used = false;
  std::string compare_petri_activity_attribute;
  std::vector<std::uint32_t> compare_petri_initial_marking;
  std::vector<std::uint32_t> compare_petri_final_marking;
  bool compare_petri_initial_marking_used = false;
  bool compare_petri_final_marking_used = false;
  bool compare_petri_walk_through_hidden = true;
  bool compare_petri_reach_final_through_hidden = true;
  bool compare_petri_stop_immediately_unfit = false;
  bool compare_petri_cleaning_token_flood = false;
  bool compare_petri_exhaustive_invisible_exploration = false;
  bool compare_petri_replay_option_used = false;
  // Select the public default explicitly at the CLI boundary. EXACT remains
  // available when --emd-variant exact is supplied.
  minera::EmdOptions emd_options;
  emd_options.variant = minera::EmdVariant::PYEMD;
  bool emd_option_used = false;
  minera::AlignmentOptions alignment_options;
  bool alignment_option_used = false;
  bool alignment_activity_option_used = false;
  bool token_replay_activity_option_used = false;
  bool alignment_max_alignments_used = false;
  bool alignment_exponent_used = false;
  bool alignment_synchronous_dijkstra_used = false;
  bool alignment_trace_cost_used = false;
  bool alignment_model_cost_used = false;
  bool alignment_silent_cost_used = false;
  bool alignment_sync_cost_used = false;
  bool align_decomposed = false;
  bool decomposed_activity_option_used = false;
  bool align_petri_anti = false;
  bool align_petri_multi = false;
  std::optional<std::size_t> reduce_trace_index;
  std::string reduction_activity_key;
  std::string advanced_petri_miner = "inductive";
  bool advanced_petri_miner_used = false;
  bool petri_option_used = false;
  bool petri_activity_option_used = false;
  minera::DecomposedAlignmentOptions decomposed_options;
  minera::PetriAntiAlignmentOptions petri_anti_options;
  minera::PetriMultiAlignmentOptions petri_multi_options;
  std::vector<std::string> spectrum_activities;  // empty = no spectrum
  bool spectrum_requested = false;
  bool spectrum_option_used = false;
  minera::PerformanceSpectrumOptions spectrum_options;
  minera::TemporalProfileOptions temporal_profile_options;
  // Temporal discovery is bundled into the normal export. Do not make
  // timestamp-less logs fail unless the caller requests strict validation.
  temporal_profile_options.strict_attributes = false;
  bool batches = false;
  bool batch_option_used = false;
  minera::BatchDiscoveryOptions batch_options;
  minera::MinimumSelfDistanceOptions minimum_self_distance_options;
  bool overlap_requested = false;
  bool chaotic_activities_requested = false;
  bool passed_time_requested = false;
  bool service_time_requested = false;
  bool rework_requested = false;
  bool cycle_time_requested = false;
  bool case_arrival_requested = false;
  bool case_statistics_requested = false;
  bool attribute_statistics_requested = false;
  bool attribute_distribution_requested = false;
  bool attribute_kde_requested = false;
  bool boundary_activities_requested = false;
  bool variant_statistics_requested = false;
  bool activity_split_option_used = false;
  bool repetition_option_used = false;
  bool case_grouping_option_used = false;
  std::map<std::string, std::string> overlap_fields;
  std::map<std::string, std::string> chaotic_activities_fields;
  std::map<std::string, std::string> passed_time_fields;
  std::map<std::string, std::string> service_time_fields;
  std::map<std::string, std::string> rework_fields;
  std::map<std::string, std::string> cycle_time_fields;
  std::map<std::string, std::string> case_arrival_fields;
  std::map<std::string, std::string> case_statistics_fields;
  std::map<std::string, std::string> attribute_statistics_fields;
  std::map<std::string, std::string> attribute_distribution_fields;
  std::map<std::string, std::string> attribute_kde_fields;
  std::map<std::string, std::string> boundary_activities_fields;
  std::map<std::string, std::string> variant_statistics_fields;
  minera::CsvImportOptions options;
  minera::XesImportOptions xes_options;
  bool xes_import_result_requested = false;
  minera::XesExportOptions xes_export_options;
  minera::LogFilterOptions filter;
  std::map<std::string, std::string> ltl_fields;
  std::map<std::string, std::string> segment_fields;
  std::map<std::string, std::string> path_fields;
  std::map<std::string, std::string> repetition_fields;
  std::map<std::string, std::string> case_grouping_fields;
  minera::InductiveMinerOptions inductive_options;
  minera::AlphaMinerOptions alpha_options;
  bool alpha_plus_result = false;
  minera::HeuristicMinerOptions heuristic_options;
  minera::PowlDiscoveryOptions powl_options;
  minera::TransitionSystemOptions transition_options;
  minera::TokenReplayOptions token_replay_options;
  minera::LogSkeletonOptions log_skeleton_options;
  minera::DeclareDiscoveryOptions declare_options;
  minera::DeclarePlayoutOptions declare_playout_options;
  bool declare_playout_options_used = false;
  std::map<std::string, std::string> process_tree_playout_fields;
  bool process_tree_playout_options_used = false;
  minera::ProcessTreePlayoutOptions process_tree_playout_options;
  std::map<std::string, std::string> basic_playout_fields;
  bool basic_playout_options_used = false;
  minera::PlayoutOptions basic_playout_options;
  std::map<std::string, std::string> extensive_playout_fields;
  bool extensive_playout_options_used = false;
  std::string petri_playout_mode = "basic";
  minera::ExtensivePlayoutOptions extensive_playout_options;
  std::map<std::string, std::string> dfg_playout_fields;
  bool dfg_playout_options_used = false;
  minera::DfgPlayoutOptions dfg_playout_options;
  std::map<std::string, std::string> stochastic_playout_fields;
  bool stochastic_playout_requested = false;
  minera::StochasticPlayoutOptions stochastic_playout_options;
  std::map<std::string, std::string> stochastic_petri_fields;
  bool stochastic_petri_requested = false;
  minera::StochasticPetriOptions stochastic_petri_options;
  std::filesystem::path stochastic_map_output;
  std::map<std::string, std::string> stochastic_map_fields;
  bool stochastic_map_requested = false;
  std::string stochastic_map_miner = "inductive";
  minera::StochasticMapParams stochastic_map_params;
  std::map<std::string, std::string> monte_carlo_fields;
  bool monte_carlo_requested = false;
  bool monte_carlo_json_requested = false;
  minera::MonteCarloOptions monte_carlo_options;
  minera::FootprintsOptions footprints_options;
  std::vector<std::string> dfg_statistical_variants;
  std::vector<std::string> dfg_causal_variants;
  minera::DfgStatisticalOptions dfg_statistics_options;
  std::map<std::string, std::string> dfg_dataframe_fields;
  bool dfg_dataframe_option_used = false;
  std::vector<std::string> correlation_variants;
  minera::CorrelationMinerOptions correlation_options;
  bool concept_drift_requested = false;
  std::map<std::string, std::string> concept_drift_fields;
  bool browser_history_requested = false;
  std::map<std::string, std::string> browser_history_fields;
  bool windows_events_requested = false;
  std::map<std::string, std::string> windows_events_fields;
  bool repository_history_requested = false;
  std::map<std::string, std::string> repository_history_fields;
  std::string repository_auth_token_env;
  bool workflow_history_requested = false;
  std::map<std::string, std::string> workflow_history_fields;
  bool enterprise_history_requested = false;
  std::map<std::string, std::string> enterprise_history_fields;
  bool communication_history_requested = false;
  std::map<std::string, std::string> communication_history_fields;
  bool target_requested = false;
  std::map<std::string, std::string> target_fields;
  bool features_requested = false;
  std::map<std::string, std::string> features_fields;
  bool embeddings_requested = false;
  std::string embedding_runtime_url;
  std::map<std::string, std::string> embedding_fields;
  bool embedding_target_requested = false;
  std::string embedding_target;
  std::size_t embedding_top_k = 0;
  bool label_splitting_requested = false;
  std::map<std::string, std::string> label_splitting_fields;
  bool trie_requested = false;
  bool trie_to_petri_requested = false;
  std::map<std::string, std::string> trie_fields;
  bool interval_requested = false;
  std::map<std::string, std::string> interval_fields;
  bool merge_requested = false;
  std::filesystem::path merge_incoming_path;
  std::filesystem::path merge_relations_path;
  std::map<std::string, std::string> merge_fields;
  bool timestamp_interleavings_requested = false;
  std::filesystem::path timestamp_interleavings_right_path;
  std::map<std::string, std::string> timestamp_interleavings_fields;
  bool random_tree_requested = false;
  std::map<std::string, std::string> random_tree_fields;
  bool ilp_mine_requested = false;
  std::map<std::string, std::string> ilp_fields;
  bool genetic_mine_requested = false;
  bool genetic_matrix_conversion_requested = false;
  std::map<std::string, std::string> genetic_fields;
  bool organizational_sna_requested = false;
  std::map<std::string, std::string> organizational_sna_fields;
  bool network_analysis_requested = false;
  std::map<std::string, std::string> network_analysis_fields;
  bool link_analysis_requested = false;
  std::map<std::string, std::string> link_analysis_fields;
  bool roles_requested = false;
  std::map<std::string, std::string> roles_fields;
  bool resource_profiles_requested = false;
  std::map<std::string, std::string> resource_profiles_fields;
  bool local_diagnostics_requested = false;
  std::map<std::string, std::string> local_diagnostics_fields;
  bool clustering_requested = false;
  std::map<std::string, std::string> clustering_fields;
  bool profile_clustering_requested = false;
  std::map<std::string, std::string> profile_clustering_fields;
  bool trace_variant_query_requested = false;
  std::map<std::string, std::string> trace_variant_query_fields;
  bool pripel_requested = false;
  std::filesystem::path pripel_query_path;
  std::map<std::string, std::string> pripel_fields;

  // Succession pairs are parsed while the try block below is not yet open, so
  // report a malformed value as a clean usage error rather than a terminate.
  const auto parse_pairs = [](const char* value) {
    try {
      return minera::parse_activity_pairs(value);
    } catch (const std::exception& error) {
      std::cerr << "error: " << error.what() << '\n';
      std::exit(2);
    }
  };
  const auto parse_nonnegative_size =
      [](const char* value, std::string_view option) -> std::size_t {
    std::size_t parsed = 0;
    const auto end = value + std::strlen(value);
    const auto result = std::from_chars(value, end, parsed);
    if (result.ec != std::errc{} || result.ptr != end)
      throw std::invalid_argument(std::string(option) +
                                  " expects a non-negative integer");
    return parsed;
  };
  const auto parse_transition_window = [](const char* value) {
    std::int64_t parsed = 0;
    const auto end = value + std::strlen(value);
    const auto result = std::from_chars(value, end, parsed);
    if (value == end || result.ec != std::errc{} || result.ptr != end)
      throw std::invalid_argument("--transition-window expects a signed integer");
    return parsed;
  };
  const auto parse_uint32_marking = [&](const char* value,
                                        std::string_view option) {
    std::vector<std::uint32_t> result;
    const std::string text = value;
    std::size_t begin = 0;
    for (std::size_t index = 0; index <= text.size(); ++index) {
      if (index != text.size() && text[index] != ',') continue;
      const auto token = text.substr(begin, index - begin);
      const auto parsed = parse_nonnegative_size(token.c_str(), option);
      if (parsed > std::numeric_limits<std::uint32_t>::max()) {
        throw std::invalid_argument(std::string(option) +
                                    " expects uint32 entries");
      }
      result.push_back(static_cast<std::uint32_t>(parsed));
      begin = index + 1;
    }
    return result;
  };
  const auto parse_positive_size =
      [](const char* value, std::string_view option) -> std::size_t {
    std::size_t parsed = 0;
    const auto end = value + std::strlen(value);
    const auto result = std::from_chars(value, end, parsed);
    if (result.ec != std::errc{} || result.ptr != end || parsed == 0)
      throw std::invalid_argument(std::string(option) +
                                  " expects a positive integer");
      return parsed;
  };
  const auto parse_boolean = [](const char* value, std::string_view option) {
    const std::string_view text = value;
    if (text == "true" || text == "1") return true;
    if (text == "false" || text == "0") return false;
    throw std::invalid_argument(std::string(option) +
                                " expects true, false, 1, or 0");
  };
  const auto parse_finite_double = [](const char* value,
                                      std::string_view option) {
    char* end = nullptr;
    errno = 0;
    const double parsed = std::strtod(value, &end);
    if (end == value || end != value + std::strlen(value) || errno == ERANGE ||
        !std::isfinite(parsed)) {
      throw std::invalid_argument(std::string(option) +
                                  " expects a finite number");
    }
    return parsed;
  };
  const auto parse_dfg_cost = [](const char* value, std::string_view option) {
    const std::string text = value; const auto sep = text.rfind('=');
    if (sep == std::string::npos || sep == 0 || sep + 1 >= text.size()) throw std::invalid_argument(std::string(option)+" expects activity=value");
    char* end = nullptr; const double cost = std::strtod(text.c_str()+sep+1, &end);
    if (end == text.c_str()+sep+1 || *end != '\0' || !std::isfinite(cost) || cost < 0.0) throw std::invalid_argument(std::string(option)+" expects a finite non-negative value");
    const auto activity = text.substr(0, sep);
    if (activity == ">>") throw std::invalid_argument(std::string(option)+" does not accept the gap activity");
    return std::pair<std::string,double>{activity, cost};
  };
  const auto parse_declare_playout_int =
      [](const char* value, std::string_view name,
         bool require_nonnegative) -> int {
    int parsed = 0;
    const auto end = value + std::strlen(value);
    const auto result = std::from_chars(value, end, parsed);
    if (result.ec != std::errc{} || result.ptr != end ||
        (require_nonnegative && parsed < 0)) {
      throw std::invalid_argument(std::string(name) +
                                  (require_nonnegative
                                       ? " expects a non-negative integer"
                                       : " expects a signed integer"));
    }
    return parsed;
  };
  const auto parse_declare_playout_seed =
      [](const char* value) -> std::uint64_t {
    std::uint64_t parsed = 0;
    const auto end = value + std::strlen(value);
    const auto result = std::from_chars(value, end, parsed);
    if (result.ec != std::errc{} || result.ptr != end) {
      throw std::invalid_argument(
          "--declare-playout-seed expects an unsigned 64-bit integer");
    }
    return parsed;
  };

  for (int i = 1; i < argc; ++i) {
    const std::string_view arg = argv[i];
    const auto next = [&]() -> const char* {
      return i + 1 < argc ? argv[++i] : nullptr;
    };
    if (arg == "--heuristic-business-hours") {
      heuristic_options.use_business_hours = true;
      continue;
    }
    if (arg == "--heuristic-business-hour-slots") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      try {
        heuristic_options.business_hour_slots_explicit = true;
        if (std::string_view(value).empty() ||
            std::string_view(value) == "none") {
          heuristic_options.business_hour_slots.clear();
        } else {
          heuristic_options.business_hour_slots =
              parse_dfg_business_hour_slots(
                  value, "--heuristic-business-hour-slots");
        }
      } catch (const std::invalid_argument& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 2;
      }
      continue;
    }
    const auto powl_parse =
        parse_powl_cli_option(arg, argc, argv, i, powl_options);
    if (powl_parse == PowlCliParseStatus::kParsed) continue;
    if (powl_parse == PowlCliParseStatus::kInvalid) return 2;
    if (powl_parse == PowlCliParseStatus::kMissingValue)
      return usage(argv[0]);
    if (arg == "--token-replay-max-rows-to-materialize" ||
        arg == "--token-replay-case-id-attribute" ||
        arg == "--token-replay-timestamp-attribute") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      try {
        if (arg == "--token-replay-max-rows-to-materialize")
          token_replay_options.max_rows_to_materialize = parse_nonnegative_size(value, arg);
        else {
          auto& selector = arg == "--token-replay-case-id-attribute"
                               ? token_replay_options.case_id_attribute
                               : token_replay_options.timestamp_attribute;
          selector = trim_text(value);
          if (selector.empty())
            throw std::invalid_argument(std::string(arg) + " expects a non-empty name");
        }
      } catch (const std::invalid_argument& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 2;
      }
      continue;
    }
    if (arg == "--token-replay-root-cause-string-attributes" ||
        arg == "--token-replay-root-cause-numeric-attributes") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      const bool strings = arg == "--token-replay-root-cause-string-attributes";
      const std::string_view name = strings ? "token_replay_root_cause_string_attributes"
                                            : "token_replay_root_cause_numeric_attributes";
      try {
        const auto parsed = minera::parse_token_replay_params(
            [&](std::string_view field) { return field == name ? std::string(value) : std::string{}; },
            [&](std::string_view field) { return field == name; });
        if (strings) token_replay_options.root_cause.string_attributes = parsed.root_cause.string_attributes;
        else token_replay_options.root_cause.numeric_attributes = parsed.root_cause.numeric_attributes;
      } catch (const std::invalid_argument& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 2;
      }
      continue;
    }
    bool* replay_policy = nullptr;
    if (arg == "--token-replay-result") {
      token_replay_result_requested = true;
      continue;
    }
    if (arg == "--token-replay-places-shortest-path-by-hidden") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      constexpr std::string_view name = "token_replay_places_shortest_path_by_hidden";
      try {
        const auto parsed = minera::parse_token_replay_params(
            [&](std::string_view field) { return field == name ? std::string(value) : std::string{}; },
            [&](std::string_view field) { return field == name; });
        token_replay_options.places_shortest_path_by_hidden = parsed.places_shortest_path_by_hidden;
      } catch (const std::invalid_argument& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 2;
      }
      continue;
    }
    if (arg == "--token-replay-disable-variants") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      try { token_replay_options.disable_variants = parse_boolean(value, arg); }
      catch (const std::invalid_argument& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 2;
      }
      token_replay_result_requested = true;
      continue;
    }
    for (const auto& [name, destination] : {
             std::pair<std::string_view, bool*>{
                 "--token-replay-consider-remaining-in-fitness", &token_replay_options.consider_remaining_in_fitness},
             {"--token-replay-try-to-reach-final-marking-through-hidden", &token_replay_options.try_to_reach_final_marking_through_hidden},
             {"--token-replay-consider-activities-not-in-model-in-fitness", &token_replay_options.consider_activities_not_in_model_in_fitness},
             {"--token-replay-walk-through-hidden-transitions", &token_replay_options.walk_through_hidden_transitions},
             {"--token-replay-cleaning-token-flood", &token_replay_options.cleaning_token_flood},
             {"--token-replay-enable-pltr-fitness", &token_replay_options.enable_pltr_fitness},
             {"--token-replay-return-names", &token_replay_options.return_names},
             {"--token-replay-exhaustive-invisible-exploration", &token_replay_options.exhaustive_invisible_exploration},
             {"--token-replay-include-trace-diagnostics", &token_replay_options.include_trace_diagnostics},
             {"--token-replay-include-duration-diagnostics", &token_replay_options.include_duration_diagnostics},
             {"--token-replay-include-root-cause-diagnostics", &token_replay_options.include_root_cause_diagnostics},
             {"--token-replay-root-cause-enable-multiplier", &token_replay_options.root_cause.enable_multiplier}}) {
      if (arg == name) replay_policy = destination;
    }
    if (replay_policy) {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      try { *replay_policy = parse_boolean(value, arg); }
      catch (const std::invalid_argument& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 2;
      }
      if (arg == "--token-replay-enable-pltr-fitness" || arg == "--token-replay-return-names")
        token_replay_result_requested = true;
      continue;
    }
    if (arg == "--alignment-activity-attribute" ||
        arg == "--token-replay-activity-attribute") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      const auto trimmed = trim_text(value);
      if (trimmed.empty()) {
        std::cerr << "error: " << arg
                  << " expects a non-empty name\n";
        return 2;
      }
      if (arg == "--alignment-activity-attribute") {
        alignment_options.activity_attribute = trimmed;
        alignment_option_used = true;
        alignment_activity_option_used = true;
      } else {
        token_replay_options.activity_attribute = trimmed;
        token_replay_activity_option_used = true;
      }
      continue;
    }
    if (arg == "--heuristic-activity-attribute" ||
        arg == "--heuristic-timestamp-attribute") {
      const auto* value = next();
      const auto trimmed = value == nullptr ? std::string{} : trim_text(value);
      if (trimmed.empty()) {
        std::cerr << "error: " << arg
                  << " expects a non-empty name\n";
        return 2;
      }
      if (arg == "--heuristic-activity-attribute")
        heuristic_options.activity_attribute = trimmed;
      else
        heuristic_options.timestamp_attribute = trimmed;
      continue;
    }
    if (arg == "--batch-activity-attribute" ||
        arg == "--batch-resource-attribute" ||
        arg == "--batch-timestamp-attribute" ||
        arg == "--batch-case-id-attribute" ||
        arg == "--batch-event-id-attribute") {
      batch_option_used = true;
      const auto* value = next();
      if (!value || std::string_view(value).empty()) {
        std::cerr << "error: " << arg << " expects a non-empty attribute name\n";
        return 2;
      }
      if (arg == "--batch-activity-attribute")
        batch_options.activity_attribute = value;
      else if (arg == "--batch-resource-attribute")
        batch_options.resource_attribute = value;
      else if (arg == "--batch-timestamp-attribute")
        batch_options.timestamp_attribute = value;
      else if (arg == "--batch-case-id-attribute")
        batch_options.case_id_attribute = value;
      else
        batch_options.event_id_attribute = value;
      continue;
    }
    if (arg == "--transition-activity-attribute" ||
        arg == "--transition-case-id-attribute") {
      const auto* value = next();
      if (!value || std::string_view(value).empty()) {
        std::cerr << "error: " << arg
                  << " expects a non-empty attribute name\n";
        return 2;
      }
      if (arg == "--transition-activity-attribute")
        transition_options.activity_attribute = value;
      else
        transition_options.case_id_attribute = value;
      continue;
    }
    if (arg == "--pnml-to-ptml" || arg == "--bpmn-to-ptml") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      if (arg == "--pnml-to-ptml") pnml_to_ptml_input = value;
      else bpmn_to_ptml_input = value;
      continue;
    }
    if (arg == "--reduce-petri-net") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      petri_reduction_input = value;
      continue;
    }
    if (arg == "--inspect-pnml") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      pnml_inspection_input = value;
      continue;
    }
    if (arg == "--pnml-infer-initial-marking" ||
        arg == "--pnml-infer-final-marking" ||
        arg == "--pnml-require-workflow-boundaries" ||
        arg == "--pnml-import-stochastic-map") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      pnml_inspection_fields.insert_or_assign(std::string(arg), value);
      continue;
    }
    if (arg == "--petri-reduction-initial-marking" ||
        arg == "--petri-reduction-final-marking" ||
        arg == "--petri-reduction-parallel-place-fusion" ||
        arg == "--petri-reduction-abstraction") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      const std::string key =
          arg == "--petri-reduction-initial-marking"
              ? "petri_reduction_initial_marking"
          : arg == "--petri-reduction-final-marking"
              ? "petri_reduction_final_marking"
          : arg == "--petri-reduction-parallel-place-fusion"
              ? "petri_reduction_parallel_place_fusion"
              : "petri_reduction_abstraction";
      petri_reduction_fields.insert_or_assign(key, value);
      continue;
    }
    if (arg == "--ltl-variant" || arg == "--ltl-attribute-key" ||
        arg == "--ltl-attribute-values" || arg == "--ltl-positive" ||
        arg == "--ltl-enable-timestamp" ||
        arg == "--ltl-timestamp-diff-boundaries" ||
        arg == "--ltl-resource-key") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      const std::string key =
          arg == "--ltl-variant" ? "ltl_variant" :
          arg == "--ltl-attribute-key" ? "ltl_attribute_key" :
          arg == "--ltl-attribute-values" ? "ltl_attribute_values" :
          arg == "--ltl-positive" ? "ltl_positive" :
          arg == "--ltl-enable-timestamp" ? "ltl_enable_timestamp" :
          arg == "--ltl-timestamp-diff-boundaries"
              ? "ltl_timestamp_diff_boundaries" : "ltl_resource_key";
      ltl_fields[key] = value;
      continue;
    }
    if (arg == "--temporal-activity-attribute" ||
        arg == "--temporal-case-id-attribute" ||
        arg == "--temporal-source-timestamp-attribute" ||
        arg == "--temporal-target-timestamp-attribute" ||
        arg == "--temporal-business-hours" ||
        arg == "--temporal-business-hour-slots" ||
        arg == "--temporal-strict-attributes") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      try {
        if (arg == "--temporal-activity-attribute" ||
            arg == "--temporal-case-id-attribute" ||
            arg == "--temporal-source-timestamp-attribute" ||
            arg == "--temporal-target-timestamp-attribute") {
          if (std::string_view(value).empty()) return usage(argv[0]);
          if (arg == "--temporal-activity-attribute")
            temporal_profile_options.activity_attribute = value;
          else if (arg == "--temporal-case-id-attribute")
            temporal_profile_options.case_id_attribute = value;
          else if (arg == "--temporal-source-timestamp-attribute")
            temporal_profile_options.source_timestamp_attribute = value;
          else
            temporal_profile_options.target_timestamp_attribute = value;
        } else if (arg == "--temporal-business-hours") {
          temporal_profile_options.use_business_hours =
              parse_boolean(value, arg);
        } else if (arg == "--temporal-strict-attributes") {
          temporal_profile_options.strict_attributes =
              parse_boolean(value, arg);
        } else {
          temporal_profile_options.business_hour_slots_explicit = true;
          if (std::string_view(value).empty() ||
              std::string_view(value) == "none") {
            temporal_profile_options.business_hour_slots.clear();
          } else {
            temporal_profile_options.business_hour_slots =
                parse_dfg_business_hour_slots(value);
          }
        }
      } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 2;
      }
      continue;
    }
    if (arg == "--spectrum" || arg == "--spectrum-variant" ||
        arg == "--spectrum-sample-size" || arg == "--spectrum-sort-log" ||
        arg == "--spectrum-activity-key" ||
        arg == "--spectrum-timestamp-key" ||
        arg == "--spectrum-case-id-key") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      if (arg == "--spectrum") {
        spectrum_activities = split_csv(value);
        spectrum_requested = true;
        if (spectrum_activities.size() < 2) {
          std::cerr << "error: --spectrum expects at least two activities\n";
          return 2;
        }
      } else if (arg == "--spectrum-variant") {
        spectrum_option_used = true;
        const auto variant = std::string_view(value);
        if (variant == "connected")
          spectrum_options.variant = minera::PerformanceSpectrumVariant::kConnected;
        else if (variant == "disconnected")
          spectrum_options.variant = minera::PerformanceSpectrumVariant::kDisconnected;
        else if (variant == "dataframe")
          spectrum_options.variant = minera::PerformanceSpectrumVariant::kDataframe;
        else if (variant == "log")
          spectrum_options.variant = minera::PerformanceSpectrumVariant::kLog;
        else if (variant == "dataframe_disconnected")
          spectrum_options.variant = minera::PerformanceSpectrumVariant::kDataframeDisconnected;
        else if (variant == "log_disconnected")
          spectrum_options.variant = minera::PerformanceSpectrumVariant::kLogDisconnected;
        else if (variant == "lazyframe")
          spectrum_options.variant = minera::PerformanceSpectrumVariant::kLazyframe;
        else if (variant == "lazyframe_disconnected")
          spectrum_options.variant = minera::PerformanceSpectrumVariant::kLazyframeDisconnected;
        else {
          std::cerr << "error: --spectrum-variant expects connected, disconnected, dataframe, log, dataframe_disconnected, log_disconnected, lazyframe, or lazyframe_disconnected\n";
          return 2;
        }
      } else if (arg == "--spectrum-sample-size") {
        spectrum_option_used = true;
        try {
          spectrum_options.sample_size = parse_nonnegative_size(value, arg);
        } catch (const std::invalid_argument& error) {
          std::cerr << "error: " << error.what() << '\n';
          return 2;
        }
      } else if (arg == "--spectrum-sort-log") {
        spectrum_option_used = true;
        try {
          spectrum_options.sort_log = parse_boolean(value, arg);
        } catch (const std::invalid_argument& error) {
          std::cerr << "error: " << error.what() << '\n';
          return 2;
        }
      } else {
        spectrum_option_used = true;
        if (std::string_view(value).empty()) return usage(argv[0]);
        if (arg == "--spectrum-activity-key")
          spectrum_options.activity_attribute = value;
        else if (arg == "--spectrum-timestamp-key")
          spectrum_options.timestamp_attribute = value;
        else
          spectrum_options.case_id_attribute = value;
      }
      continue;
    }
    if (arg == "--batch-variant") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      const auto variant = std::string_view(value);
      if (variant == "log")
        batch_options.variant = minera::BatchDiscoveryVariant::kLog;
      else if (variant == "pandas")
        batch_options.variant = minera::BatchDiscoveryVariant::kPandas;
      else if (variant == "polars")
        batch_options.variant = minera::BatchDiscoveryVariant::kPolars;
      else {
        std::cerr << "error: --batch-variant expects log, pandas, or polars\n";
        return 2;
      }
      batch_option_used = true;
      continue;
    }
    if (arg == "--minimum-self-distance-variant") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      const auto variant = std::string_view(value);
      if (variant == "log") {
        minimum_self_distance_options.variant =
            minera::MinimumSelfDistanceVariant::kLog;
      } else if (variant == "pandas") {
        minimum_self_distance_options.variant =
            minera::MinimumSelfDistanceVariant::kPandas;
      } else if (variant == "polars") {
        minimum_self_distance_options.variant =
            minera::MinimumSelfDistanceVariant::kPolars;
      } else {
        std::cerr << "error: --minimum-self-distance-variant expects log, pandas, or polars\n";
        return 2;
      }
      continue;
    }
    if (arg == "--minimum-self-distance-activity-key" ||
        arg == "--minimum-self-distance-case-id-key") {
      const auto* value = next();
      if (!value || std::string_view(value).empty()) return usage(argv[0]);
      if (arg == "--minimum-self-distance-activity-key")
        minimum_self_distance_options.activity_key = value;
      else
        minimum_self_distance_options.case_id_key = value;
      continue;
    }
    if (arg == "--trace-patterns" || arg == "--trace-pattern") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.trace_patterns = minera::parse_trace_patterns(value);
      if (filter.trace_patterns.empty()) return usage(argv[0]);
      continue;
    }
    if (arg == "--trace-pattern-positive") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.trace_pattern_positive = parse_boolean(value, arg);
      continue;
    }
    if (arg == "--admitted-variants") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      try {
        filter.admitted_variants = minera::parse_admitted_variants(value);
      } catch (const std::invalid_argument& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 2;
      }
      if (filter.admitted_variants.empty()) return usage(argv[0]);
      continue;
    }
    if (arg == "--admitted-variants-positive") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      try {
        filter.admitted_variants_positive = parse_boolean(value, arg);
      } catch (const std::invalid_argument& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 2;
      }
      continue;
    }
    if (arg == "--case-duration-min" || arg == "--case-duration-max") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      const double parsed = parse_finite_double(value, arg);
      if (arg == "--case-duration-min")
        filter.min_case_duration_seconds = parsed;
      else
        filter.max_case_duration_seconds = parsed;
      continue;
    }
    if (arg == "--numeric-attribute-key" ||
        arg == "--numeric-attribute-min" ||
        arg == "--numeric-attribute-max" ||
        arg == "--numeric-attribute-mode" ||
        arg == "--numeric-attribute-positive") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      if (!filter.numeric_attribute)
        filter.numeric_attribute = minera::NumericAttributeFilterOptions{};
      auto& numeric = *filter.numeric_attribute;
      if (arg == "--numeric-attribute-key") {
        numeric.attribute_key = value;
      } else if (arg == "--numeric-attribute-min") {
        numeric.min_value = parse_finite_double(value, arg);
      } else if (arg == "--numeric-attribute-max") {
        numeric.max_value = parse_finite_double(value, arg);
      } else if (arg == "--numeric-attribute-mode") {
        if (std::string_view(value) == "events")
          numeric.mode = minera::AttributeFilterMode::events;
        else if (std::string_view(value) == "traces")
          numeric.mode = minera::AttributeFilterMode::traces;
        else
          throw std::invalid_argument(
              "--numeric-attribute-mode expects events or traces");
      } else {
        numeric.positive = parse_boolean(value, arg);
      }
      continue;
    }
    if (arg == "--max-activities" || arg == "--max-no-activities" ||
        arg == "--top-activities" ||
        arg == "--top-activities-attribute-key") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      if (!filter.top_activities)
        filter.top_activities = minera::TopActivitiesFilterOptions{};
      auto& top = *filter.top_activities;
      if (arg == "--top-activities-attribute-key") {
        top.attribute_key = value;
      } else {
        top.max_activities = parse_nonnegative_size(value, arg);
      }
      continue;
    }
    if (arg == "--relative-attribute-key" ||
        arg == "--relative-attribute-stake" ||
        arg == "--relative-attribute-keep-once-per-case" ||
        arg == "--relative-attribute-keep-once") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      if (!filter.relative_attribute_occurrence)
        filter.relative_attribute_occurrence =
            minera::RelativeAttributeOccurrenceFilterOptions{};
      auto& relative = *filter.relative_attribute_occurrence;
      if (arg == "--relative-attribute-key") {
        relative.attribute_key = value;
      } else if (arg == "--relative-attribute-stake") {
        relative.min_relative_stake = parse_finite_double(value, arg);
        if (relative.min_relative_stake < 0.0 ||
            relative.min_relative_stake > 1.0)
          throw std::invalid_argument(
              "--relative-attribute-stake expects a number in [0, 1]");
      } else {
        relative.keep_once_per_case = parse_boolean(value, arg);
      }
      continue;
    }
    if (arg == "--attribute-occurrence-key" ||
        arg == "--attribute-occurrence-min" ||
        arg == "--attribute-threshold-key" ||
        arg == "--attribute-threshold") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      if (!filter.attribute_occurrence_threshold)
        filter.attribute_occurrence_threshold =
            minera::AttributeOccurrenceThresholdFilterOptions{};
      auto& threshold = *filter.attribute_occurrence_threshold;
      if (arg == "--attribute-occurrence-key" ||
          arg == "--attribute-threshold-key") {
        threshold.attribute_key = value;
      } else {
        threshold.min_occurrences = parse_nonnegative_size(value, arg);
      }
      continue;
    }
    if (arg == "--start-activities-positive" ||
        arg == "--end-activities-positive") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      const bool positive = parse_boolean(value, arg);
      if (arg == "--start-activities-positive")
        filter.start_activities_positive = positive;
      else
        filter.end_activities_positive = positive;
      continue;
    }
    if (arg == "--variant-min-coverage" ||
        arg == "--variant-max-coverage" ||
        arg == "--variant-percentage") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      const double parsed = parse_finite_double(value, arg);
      if (parsed < 0.0 || parsed > 1.0)
        throw std::invalid_argument(
            std::string(arg) + " expects a number in [0, 1]");
      if (arg == "--variant-min-coverage")
        filter.variant_min_coverage = parsed;
      else if (arg == "--variant-max-coverage")
        filter.variant_max_coverage = parsed;
      else
        filter.variant_percentage = parsed;
      continue;
    }
    if (arg == "--prefix-strict" || arg == "--prefix-occurrence" ||
        arg == "--suffix-strict" || arg == "--suffix-occurrence") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      const std::string_view text = value;
      if (arg == "--prefix-strict") {
        filter.prefix_strict = parse_boolean(value, arg);
      } else if (arg == "--suffix-strict") {
        filter.suffix_strict = parse_boolean(value, arg);
      } else if (text == "first" || text == "last") {
        const auto occurrence = text == "first"
                                    ? minera::FilterAnchorOccurrence::first
                                    : minera::FilterAnchorOccurrence::last;
        if (arg == "--prefix-occurrence")
          filter.prefix_occurrence = occurrence;
        else
          filter.suffix_occurrence = occurrence;
      } else {
        throw std::invalid_argument(
            std::string(arg) + " expects first or last");
      }
      continue;
    }
    if (arg == "--max-cases" || arg == "--max-no-cases" ||
        arg == "--case-limit" || arg == "--min-length" ||
        arg == "--max-length") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      const auto parsed = parse_nonnegative_size(value, arg);
      if (arg == "--max-cases" || arg == "--max-no-cases" ||
          arg == "--case-limit") {
        filter.max_cases = parsed;
      } else if (arg == "--min-length") {
        filter.min_length = parsed;
      } else {
        filter.max_length = parsed;
      }
      continue;
    }
    if (arg == "--segment-prefix-activity" ||
        arg == "--segment-prefix-strict" ||
        arg == "--segment-prefix-occurrence" ||
        arg == "--segment-suffix-activity" ||
        arg == "--segment-suffix-strict" ||
        arg == "--segment-suffix-occurrence" ||
        arg == "--segment-between-starts" ||
        arg == "--segment-between-ends" ||
        arg == "--segment-between-separator") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      std::string key(arg.substr(2));
      std::replace(key.begin(), key.end(), '-', '_');
      segment_fields[key] = value;
      continue;
    }
    if (arg == "--path-predecessor" || arg == "--path-successor" ||
        arg == "--path-min-performance" || arg == "--path-max-performance" ||
        arg == "--path-positive" || arg == "--path-activity-attribute" ||
        arg == "--path-timestamp-attribute") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      std::string key(arg.substr(2));
      std::replace(key.begin(), key.end(), '-', '_');
      path_fields[key] = value;
      continue;
    }
    if (arg == "--activity-split" ||
        arg == "--activity-split-separator" ||
        arg == "--activity-split-cut-mode") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      activity_split_option_used = true;
      if (arg == "--activity-split") {
        filter.activity_split_activities = split_csv(value);
      } else if (arg == "--activity-split-separator") {
        if (value[0] == '\0') return usage(argv[0]);
        filter.activity_split_separator = value;
      } else if (std::string_view(value) == "this") {
        filter.activity_split_cut_mode = minera::ActivitySplitCutMode::kThis;
      } else if (std::string_view(value) == "next") {
        filter.activity_split_cut_mode = minera::ActivitySplitCutMode::kNext;
      } else {
        std::cerr << "error: --activity-split-cut-mode expects this or next\n";
        return 2;
      }
      continue;
    }
    if (arg == "--repetition-value" ||
        arg == "--attribute-value-repetition" ||
        arg == "--attr-value-repetition" ||
        arg == "--repetition-attribute-key" || arg == "--repetition-min" ||
        arg == "--repetition-max") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      std::string key(arg.substr(2));
      std::replace(key.begin(), key.end(), '-', '_');
      repetition_fields[key] = value;
      repetition_option_used = true;
      continue;
    }
    if (arg == "--consecutive-case-grouping" ||
        arg == "--consecutive-case-grouping-activity-key" ||
        arg == "--timestamp-case-grouping" ||
        arg == "--timestamp-case-grouping-activity-key" ||
        arg == "--timestamp-case-grouping-timestamp-key" ||
        arg == "--timestamp-case-grouping-separator") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      std::string key(arg.substr(2));
      std::replace(key.begin(), key.end(), '-', '_');
      case_grouping_fields[key] = value;
      case_grouping_option_used = true;
      continue;
    }
    if (arg == "--overlap") {
      overlap_requested = true;
      overlap_fields["overlap"] = "true";
      continue;
    }
    if (arg == "--chaotic-activities") {
      chaotic_activities_requested = true;
      continue;
    }
    if (arg == "--chaotic-activity-key" || arg == "--chaotic-alpha" ||
        arg == "--chaotic-max-activities" || arg == "--chaotic-max-events") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      std::string key(arg.substr(2));
      std::replace(key.begin(), key.end(), '-', '_');
      chaotic_activities_fields[key] = value;
      chaotic_activities_requested = true;
      continue;
    }
    if (arg == "--passed-time") {
      passed_time_requested = true;
      continue;
    }
    if (arg == "--passed-time-variant" ||
        arg == "--passed-time-activity" ||
        arg == "--passed-time-activity-key" ||
        arg == "--passed-time-timestamp-key" ||
        arg == "--passed-time-start-timestamp-key" ||
        arg == "--passed-time-aggregation" ||
        arg == "--passed-time-business-hours" ||
        arg == "--passed-time-business-hour-slots") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      std::string key(arg.substr(2));
      std::replace(key.begin(), key.end(), '-', '_');
      passed_time_fields[key] = value;
      passed_time_requested = true;
      continue;
    }
    if (arg == "--service-time") {
      service_time_requested = true;
      continue;
    }
    if (arg == "--service-time-activity-key" ||
        arg == "--service-time-start-timestamp-key" ||
        arg == "--service-time-timestamp-key" ||
        arg == "--service-time-aggregation" ||
        arg == "--service-time-business-hours" ||
        arg == "--service-time-business-hour-slots") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      std::string key(arg.substr(2));
      std::replace(key.begin(), key.end(), '-', '_');
      service_time_fields[key] = value;
      service_time_requested = true;
      continue;
    }
    if (arg == "--rework") {
      rework_requested = true;
      continue;
    }
    if (arg == "--rework-variant" || arg == "--rework-activity-key" ||
        arg == "--rework-case-id-key") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      std::string key(arg.substr(2));
      std::replace(key.begin(), key.end(), '-', '_');
      rework_fields[key] = value;
      rework_requested = true;
      continue;
    }
    if (arg == "--cycle-time") {
      cycle_time_requested = true;
      continue;
    }
    if (arg == "--cycle-time-timestamp-key" ||
        arg == "--cycle-time-start-timestamp-key") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      std::string key(arg.substr(2));
      std::replace(key.begin(), key.end(), '-', '_');
      cycle_time_fields[key] = value;
      cycle_time_requested = true;
      continue;
    }
    if (arg == "--case-arrival") {
      case_arrival_requested = true;
      continue;
    }
    if (arg == "--case-arrival-timestamp-key" ||
        arg == "--case-arrival-business-hours" ||
        arg == "--case-arrival-business-hour-slots") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      std::string key(arg.substr(2));
      std::replace(key.begin(), key.end(), '-', '_');
      case_arrival_fields[key] = value;
      case_arrival_requested = true;
      continue;
    }
    if (arg == "--case-statistics") {
      case_statistics_requested = true;
      continue;
    }
    if (arg == "--case-statistics-case-id-key" ||
        arg == "--case-statistics-timestamp-key" ||
        arg == "--case-statistics-enable-sort" ||
        arg == "--case-statistics-sort-by" ||
        arg == "--case-statistics-sort-ascending" ||
        arg == "--case-statistics-max-cases" ||
        arg == "--case-statistics-business-hours" ||
        arg == "--case-statistics-business-hour-slots") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      std::string key(arg.substr(2));
      std::replace(key.begin(), key.end(), '-', '_');
      case_statistics_fields[key] = value;
      case_statistics_requested = true;
      continue;
    }
    if (arg == "--attribute-statistics") {
      attribute_statistics_requested = true;
      continue;
    }
    if (arg == "--attribute-statistics-key" ||
        arg == "--attribute-statistics-scope" ||
        arg == "--attribute-statistics-keep-once-per-case") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      std::string key(arg.substr(2));
      std::replace(key.begin(), key.end(), '-', '_');
      attribute_statistics_fields[key] = value;
      attribute_statistics_requested = true;
      continue;
    }
    if (arg == "--attribute-distribution") {
      attribute_distribution_requested = true;
      continue;
    }
    if (arg == "--attribute-distribution-type" ||
        arg == "--attribute-distribution-timestamp-key") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      std::string key(arg.substr(2));
      std::replace(key.begin(), key.end(), '-', '_');
      attribute_distribution_fields[key] = value;
      attribute_distribution_requested = true;
      continue;
    }
    if (arg == "--attribute-kde") {
      attribute_kde_requested = true;
      continue;
    }
    if (arg == "--attribute-kde-key" ||
        arg == "--attribute-kde-variant" ||
        arg == "--attribute-kde-graph-points" ||
        arg == "--attribute-kde-points-to-sample") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      std::string key(arg.substr(2));
      std::replace(key.begin(), key.end(), '-', '_');
      attribute_kde_fields[key] = value;
      attribute_kde_requested = true;
      continue;
    }
    if (arg == "--boundary-activities") {
      boundary_activities_requested = true;
      continue;
    }
    if (arg == "--boundary-activities-variant" ||
        arg == "--boundary-activities-activity-key") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      std::string key(arg.substr(2));
      std::replace(key.begin(), key.end(), '-', '_');
      boundary_activities_fields[key] = value;
      boundary_activities_requested = true;
      continue;
    }
    if (arg == "--variant-statistics") {
      variant_statistics_requested = true;
      continue;
    }
    if (arg == "--variant-statistics-activity-key" ||
        arg == "--variant-statistics-max-variants") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      std::string key(arg.substr(2));
      std::replace(key.begin(), key.end(), '-', '_');
      variant_statistics_fields[key] = value;
      variant_statistics_requested = true;
      continue;
    }
    if (arg == "--overlap-strict") {
      overlap_requested = true;
      overlap_fields["overlap"] = "true";
      overlap_fields["overlap_strict"] = "true";
      if (i + 1 < argc) {
        const std::string_view next_value = argv[i + 1];
        if (next_value == "true" || next_value == "false" ||
            next_value == "1" || next_value == "0") {
          overlap_fields["overlap_strict"] = std::string(next_value);
          ++i;
        }
      }
      continue;
    }
    if (arg == "--overlap-activity-attribute" ||
        arg == "--overlap-start-attribute" ||
        arg == "--overlap-completion-attribute" ||
        arg == "--overlap-epsilon") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      std::string key(arg.substr(2));
      std::replace(key.begin(), key.end(), '-', '_');
      overlap_fields[key] = value;
      overlap_requested = true;
      continue;
    }
    if (arg == "--dfg") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      dfg_output = value;
      continue;
    }
    if (arg == "--log-convert") {
      const auto* value = next();
      if (!value || *value == '\0') return usage(argv[0]);
      log_conversion_fields.insert_or_assign("log_conversion_variant", value);
      log_conversion_requested = true;
      continue;
    }
    if (arg.rfind("--log-conversion-", 0) == 0) {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      std::string key(arg.substr(2));
      std::replace(key.begin(), key.end(), '-', '_');
      static const std::set<std::string> names = {
          "log_conversion_variant", "log_conversion_include_case_attributes",
          "log_conversion_case_attribute_prefix", "log_conversion_case_id_key",
          "log_conversion_stream_postprocessing",
          "log_conversion_max_rows_to_materialize",
          "log_conversion_deep_copy", "log_conversion_compress",
          "log_conversion_lazy_output",
          "log_conversion_graph_case_id_attribute", "log_conversion_include_df",
          "log_conversion_case_attributes_as_nodes",
          "log_conversion_event_attributes_as_nodes",
          "log_conversion_return_variants",
          "log_conversion_max_variants_to_return",
          "log_conversion_max_graph_nodes",
          "log_conversion_max_graph_edges"};
      if (!names.contains(key)) return usage(argv[0]);
      log_conversion_fields.insert_or_assign(std::move(key), value);
      log_conversion_requested = true;
      continue;
    }
    if (arg == "--ilp-mine") {
      ilp_mine_requested = true;
      continue;
    }
    if (arg.rfind("--ilp-", 0) == 0) {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      std::string key(arg.substr(2));
      std::replace(key.begin(), key.end(), '-', '_');
      static const std::set<std::string> ilp_option_names = {
          "ilp_activity_key", "ilp_artificial_start_activity",
          "ilp_artificial_end_activity", "ilp_alpha", "ilp_causal_relation",
          "ilp_noise_threshold", "ilp_max_nodes"};
      if (!ilp_option_names.contains(key)) return usage(argv[0]);
      ilp_fields.insert_or_assign(std::move(key), value);
      ilp_mine_requested = true;
      continue;
    }
    if (arg == "--genetic-mine") {
      genetic_mine_requested = true;
      continue;
    }
    if (arg == "--genetic-matrix-convert") {
      genetic_mine_requested = true;
      genetic_matrix_conversion_requested = true;
      continue;
    }
    if (arg.rfind("--genetic-", 0) == 0) {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      std::string key(arg.substr(2));
      std::replace(key.begin(), key.end(), '-', '_');
      static const std::set<std::string> genetic_option_names = {
          "genetic_activity_key", "genetic_timestamp_key", "genetic_case_id_key",
          "genetic_seed", "genetic_population_size",
          "genetic_generations", "genetic_elitism_rate",
          "genetic_crossover_rate", "genetic_mutation_rate",
          "genetic_elitism_min_sample", "genetic_max_activities",
          "genetic_max_states", "genetic_conversion_variant"};
      if (!genetic_option_names.contains(key)) return usage(argv[0]);
      genetic_fields.insert_or_assign(std::move(key), value);
      genetic_mine_requested = true;
      continue;
    }
    if (arg == "--organizational-sna" || arg == "--sna") {
      organizational_sna_requested = true;
      continue;
    }
    if (arg == "--network-analysis") { network_analysis_requested = true; continue; }
    if (arg.rfind("--network-analysis-", 0) == 0) {
      const auto* value = next(); if (!value) return usage(argv[0]);
      std::string key(arg.substr(2)); std::replace(key.begin(), key.end(), '-', '_');
      static const std::set<std::string> names = {
          "network_analysis_variant", "network_analysis_sorting_attribute", "network_analysis_timestamp_attribute",
          "network_analysis_source_node_attribute", "network_analysis_target_node_attribute",
          "network_analysis_edge_attribute", "network_analysis_edge_reference",
          "network_analysis_out_relation_attribute", "network_analysis_in_relation_attribute",
          "network_analysis_include_performance", "network_analysis_business_hours",
          "network_analysis_business_hour_slots", "network_analysis_max_rows",
          "network_analysis_max_values"};
      if (!names.contains(key)) return usage(argv[0]);
      network_analysis_fields.insert_or_assign(std::move(key), value);
      network_analysis_requested = true; continue;
    }
    if (arg == "--link-analysis") { link_analysis_requested = true; continue; }
    if (arg.rfind("--link-analysis-", 0) == 0) {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      std::string key(arg.substr(2));
      std::replace(key.begin(), key.end(), '-', '_');
      static const std::set<std::string> names = {
          "link_analysis_out_column", "link_analysis_in_column",
          "link_analysis_sorting_column", "link_analysis_index_column",
          "link_analysis_look_forward",
          "link_analysis_keep_first_occurrence", "link_analysis_propagate",
          "link_analysis_max_links"};
      if (!names.contains(key)) return usage(argv[0]);
      link_analysis_fields.insert_or_assign(std::move(key), value);
      link_analysis_requested = true;
      continue;
    }
    if (arg == "--roles") { roles_requested = true; continue; }
    if (arg == "--resource-profiles") { resource_profiles_requested = true; continue; }
    if (arg == "--local-diagnostics") { local_diagnostics_requested = true; continue; }
    if (arg == "--clustering") { clustering_requested = true; continue; }
    if (arg.rfind("--clustering-", 0) == 0) {
      const auto* value = next(); if (!value) return usage(argv[0]);
      std::string key(arg.substr(2)); std::replace(key.begin(), key.end(), '-', '_');
      static const std::set<std::string> names = {"clustering_trace_attribute", "clustering_activity_attribute", "clustering_variant", "clustering_alpha", "clustering_percent", "clustering_max_groups", "clustering_max_output_nodes"};
      if (!names.contains(key)) return usage(argv[0]); clustering_fields.insert_or_assign(std::move(key), value); clustering_requested = true; continue;
    }
    if (arg == "--profile-clustering") {
      profile_clustering_requested = true;
      continue;
    }
    if (arg.rfind("--profile-clustering-", 0) == 0) {
      const auto* value = next(); if (!value) return usage(argv[0]);
      std::string key(arg.substr(2));
      std::replace(key.begin(), key.end(), '-', '_');
      if (key.rfind("profile_clustering_", 0) == 0)
        key = "profile_" + key.substr(std::string("profile_clustering_").size());
      static const std::set<std::string> names = {
          "profile_variant", "profile_cluster_count", "profile_max_iterations", "profile_max_traces",
          "profile_max_features", "profile_seed", "profile_tolerance",
          "profile_activity_attribute",
          "profile_include_activity", "profile_include_succession",
          "profile_trace_string_attributes", "profile_event_string_attributes",
          "profile_trace_numeric_attributes", "profile_event_numeric_attributes",
          "profile_feature_names"};
      if (!names.contains(key)) return usage(argv[0]);
      profile_clustering_fields.insert_or_assign(std::move(key), value);
      profile_clustering_requested = true;
      continue;
    }
    if (arg.rfind("--local-diagnostics-", 0) == 0) {
      const auto* value = next(); if (!value) return usage(argv[0]);
      std::string key(arg.substr(2)); std::replace(key.begin(), key.end(), '-', '_');
      static const std::set<std::string> names = {"local_diagnostics_group_attribute", "local_diagnostics_resource_attribute", "local_diagnostics_activity_attribute", "local_diagnostics_groups", "local_diagnostics_max_groups", "local_diagnostics_max_resources", "local_diagnostics_max_rows"};
      if (!names.contains(key)) return usage(argv[0]);
      local_diagnostics_fields.insert_or_assign(std::move(key), value); local_diagnostics_requested = true; continue;
    }
    if (arg.rfind("--resource-profile-", 0) == 0) {
      const auto* value = next(); if (!value) return usage(argv[0]);
      std::string key("resource_profiles_"); key += std::string(arg.substr(19));
      static const std::set<std::string> names = {"resource_profiles_metric", "resource_profiles_activity_attribute", "resource_profiles_resource_attribute", "resource_profiles_timestamp_attribute", "resource_profiles_start_timestamp_attribute", "resource_profiles_case_id_attribute", "resource_profiles_t1", "resource_profiles_t2", "resource_profiles_resource", "resource_profiles_activity", "resource_profiles_resource2", "resource_profiles_max_resources", "resource_profiles_max_rows"};
      if (!names.contains(key)) throw std::invalid_argument("unknown resource profile option: " + key);
      resource_profiles_fields.insert_or_assign(key, value); resource_profiles_requested = true; continue;
    }
    if (arg.rfind("--roles-", 0) == 0) {
      const auto* value = next(); if (!value) return usage(argv[0]);
      std::string key(arg.substr(2)); std::replace(key.begin(), key.end(), '-', '_');
      static const std::set<std::string> names = {"roles_variant", "roles_activity_attribute", "roles_resource_attribute", "roles_threshold", "roles_max_resources", "roles_max_activities"};
      if (!names.contains(key)) return usage(argv[0]);
      roles_fields.insert_or_assign(std::move(key), value); roles_requested = true; continue;
    }
    if (arg.rfind("--sna-", 0) == 0) {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      std::string key(arg.substr(2));
      std::replace(key.begin(), key.end(), '-', '_');
      static const std::set<std::string> names = {
          "sna_variant", "sna_activity_attribute", "sna_resource_attribute",
          "sna_case_id_attribute",
          "sna_beta", "sna_n", "sna_metric_normalization",
          "sna_max_resources", "sna_max_edges"};
      if (!names.contains(key)) return usage(argv[0]);
      organizational_sna_fields.insert_or_assign(std::move(key), value);
      organizational_sna_requested = true;
      continue;
    }
    if (arg == "--trace-variant-query") {
      trace_variant_query_requested = true;
      continue;
    }
    if (arg.rfind("--tvq-", 0) == 0) {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      std::string key(arg.substr(2));
      std::replace(key.begin(), key.end(), '-', '_');
      static const std::set<std::string> names = {
          "tvq_variant", "tvq_epsilon", "tvq_k", "tvq_p", "tvq_seed",
          "tvq_secure_random", "tvq_max_output_traces",
          "tvq_max_output_variants"};
      if (!names.contains(key)) return usage(argv[0]);
      trace_variant_query_fields.insert_or_assign(std::move(key), value);
      trace_variant_query_requested = true;
      continue;
    }
    if (arg == "--pripel") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      pripel_query_path = value;
      pripel_requested = true;
      continue;
    }
    if (arg.rfind("--pripel-", 0) == 0) {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      std::string key(arg.substr(2));
      std::replace(key.begin(), key.end(), '-', '_');
      static const std::set<std::string> names = {
          "pripel_epsilon", "pripel_blocklist", "pripel_seed",
          "pripel_secure_random", "pripel_max_output_traces",
          "pripel_max_distance"};
      if (!names.contains(key)) return usage(argv[0]);
      pripel_fields.insert_or_assign(std::move(key), value);
      pripel_requested = true;
      continue;
    }
    if (arg == "--concept-drift") { concept_drift_requested = true; continue; }
    if (arg == "--browser-history") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      browser_history_fields.insert_or_assign("browser_history_variant", value);
      browser_history_requested = true;
      continue;
    }
    if (arg == "--browser-history-max-events") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      browser_history_fields.insert_or_assign("browser_history_max_events", value);
      browser_history_requested = true;
      continue;
    }
    if (arg == "--windows-events") {
      windows_events_requested = true;
      continue;
    }
    if (arg == "--windows-events-max-events") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      windows_events_fields.insert_or_assign("windows_events_max_events", value);
      windows_events_requested = true;
      continue;
    }
    if (arg == "--repository-history") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      const std::string selection(value);
      const auto separator = selection.find('/');
      if (separator == std::string::npos || separator == 0 || separator + 1 == selection.size())
        return usage(argv[0]);
      repository_history_fields["repository_owner"] = selection.substr(0, separator);
      repository_history_fields["repository_name"] = selection.substr(separator + 1);
      repository_history_requested = true;
      continue;
    }
    if (arg == "--repository-auth-token-env") {
      const auto* value = next(); if (!value) return usage(argv[0]);
      repository_auth_token_env = value; repository_history_requested = true; continue;
    }
    if (arg.rfind("--repository-", 0) == 0) {
      const auto* value = next(); if (!value) return usage(argv[0]);
      std::string key(arg.substr(2)); std::replace(key.begin(), key.end(), '-', '_');
      static const std::set<std::string> names = {"repository_api_base_url", "repository_max_pages", "repository_max_issues", "repository_max_events"};
      if (!names.contains(key)) return usage(argv[0]);
      repository_history_fields.insert_or_assign(std::move(key), value);
      repository_history_requested = true; continue;
    }
    if (arg == "--workflow-history") { workflow_history_requested = true; continue; }
    if (arg == "--workflow-max-events") {
      const auto* value = next(); if (!value) return usage(argv[0]);
      workflow_history_fields["workflow_max_events"] = value;
      workflow_history_requested = true; continue;
    }
    if (arg == "--enterprise-history") {
      const auto* value = next(); if (!value) return usage(argv[0]);
      enterprise_history_fields["enterprise_variant"] = value;
      enterprise_history_requested = true; continue;
    }
    if (arg == "--enterprise-max-events") {
      const auto* value = next(); if (!value) return usage(argv[0]);
      enterprise_history_fields["enterprise_max_events"] = value;
      enterprise_history_requested = true; continue;
    }
    if (arg == "--communication-history") { const auto* value = next(); if (!value) return usage(argv[0]); communication_history_fields["communication_variant"] = value; communication_history_requested = true; continue; }
    if (arg.rfind("--communication-", 0) == 0) { const auto* value = next(); if (!value) return usage(argv[0]); std::string key(arg.substr(2)); std::replace(key.begin(), key.end(), '-', '_'); static const std::set<std::string> names = {"communication_include_reminders", "communication_current_time", "communication_max_items", "communication_max_events"}; if (!names.contains(key)) return usage(argv[0]); communication_history_fields[std::move(key)] = value; communication_history_requested = true; continue; }
    if (arg.rfind("--drift-", 0) == 0) {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      std::string key(arg.substr(2));
      std::replace(key.begin(), key.end(), '-', '_');
      static const std::set<std::string> names = {
          "drift_variant", "drift_sub_log_size", "drift_window_size",
          "drift_num_permutations", "drift_threshold",
          "drift_max_no_change_points", "drift_seed",
          "drift_include_timestamps", "drift_activity_attribute",
          "drift_timestamp_attribute", "drift_case_id_attribute"};
      if (!names.contains(key)) return usage(argv[0]);
      concept_drift_fields.insert_or_assign(std::move(key), value);
      concept_drift_requested = true;
      continue;
    }
    if (arg == "--transform-target") { target_requested = true; continue; }
    if (arg.rfind("--target-", 0) == 0) {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      std::string key(arg.substr(2));
      std::replace(key.begin(), key.end(), '-', '_');
      static const std::set<std::string> target_option_names = {
          "target_variant",       "target_activity_key",
          "target_timestamp_key", "target_enable_padding",
          "target_pad_size",      "target_activities"};
      if (!target_option_names.contains(key)) return usage(argv[0]);
      target_fields.insert_or_assign(std::move(key), value);
      target_requested = true;
      continue;
    }
    if (arg == "--log-to-features" || arg == "--features") { features_requested = true; continue; }
    if (arg.rfind("--features-", 0) == 0 || arg.rfind("--feature-", 0) == 0) {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      std::string key(arg.substr(2));
      std::replace(key.begin(), key.end(), '-', '_');
      static const std::set<std::string> feature_option_names = {
          "features_variant", "features_activity_key", "features_case_id_key", "features_timestamp_key",
          "features_start_timestamp_key", "features_resource_key",
          "features_arrival_rate", "features_finish_rate", "features_service_time",
          "features_waiting_time", "features_sojourn_time", "features_diff_start_end",
          "features_grouper_freq", "features_window_seconds",
          "features_default_not_present",
          "features_event_string_attributes", "features_event_numeric_attributes",
          "features_case_string_attributes", "features_case_numeric_attributes",
          "features_event_attributes", "features_case_attributes", "features_event_successor_attributes",
          "features_feature_names", "features_enable_activity_def_representation",
          "features_enable_succ_def_representation", "features_enable_all_extra_features",
          "features_enable_case_duration", "features_add_case_identifier_column",
          "features_enable_times_from_first_occurrence", "features_enable_times_from_last_occurrence",
          "features_enable_direct_paths_times_last_occ", "features_enable_indirect_paths_times_last_occ",
          "features_enable_work_in_progress", "features_enable_resource_workload",
          "features_enable_first_last_activity_index", "features_enable_max_concurrent_events",
          "features_enable_max_concurrent_events_per_activity", "features_epsilon",
          "features_min_num_different_string_values", "features_max_num_different_string_values",
          "feature_variant", "feature_activity_key", "feature_case_id_key", "feature_timestamp_key",
          "feature_start_timestamp_key", "feature_resource_key",
          "feature_arrival_rate", "feature_finish_rate", "feature_service_time",
          "feature_waiting_time", "feature_sojourn_time", "feature_diff_start_end", "feature_window_seconds",
          "feature_grouper_freq",
          "feature_default_not_present", "feature_event_string_attributes",
          "feature_event_numeric_attributes", "feature_case_string_attributes",
          "feature_case_numeric_attributes", "feature_event_attributes",
          "feature_case_attributes", "feature_event_successor_attributes", "feature_feature_names",
          "feature_enable_activity_def_representation", "feature_enable_succ_def_representation",
          "feature_enable_all_extra_features", "feature_enable_case_duration",
          "feature_add_case_identifier_column", "feature_enable_times_from_first_occurrence",
          "feature_enable_times_from_last_occurrence", "feature_enable_direct_paths_times_last_occ",
          "feature_enable_indirect_paths_times_last_occ", "feature_enable_work_in_progress",
          "feature_enable_resource_workload", "feature_enable_first_last_activity_index",
          "feature_enable_max_concurrent_events", "feature_enable_max_concurrent_events_per_activity",
          "feature_epsilon", "feature_min_num_different_string_values",
          "feature_max_num_different_string_values"};
      if (!feature_option_names.contains(key)) return usage(argv[0]);
      features_fields.insert_or_assign(std::move(key), value);
      features_requested = true;
      continue;
    }
    if (arg == "--embeddings" || arg == "--to-embeddings") {
      embeddings_requested = true;
      continue;
    }
    if (arg == "--embedding-runtime-url") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      embedding_runtime_url = value;
      if (!embedding_fields.contains("embedding_provider"))
        embedding_fields.insert_or_assign("embedding_provider", "transformer");
      embeddings_requested = true;
      continue;
    }
    if (arg == "--embedding-target") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      embedding_target = value;
      embedding_target_requested = true;
      embeddings_requested = true;
      continue;
    }
    if (arg == "--embedding-top-k") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      try {
        embedding_top_k = parse_nonnegative_size(value, arg);
      } catch (const std::invalid_argument& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 2;
      }
      embeddings_requested = true;
      continue;
    }
    if (arg == "--embedding-keep-cases") {
      embedding_fields.insert_or_assign("embedding_keep_cases", "true");
      embeddings_requested = true;
      continue;
    }
    if (arg.rfind("--embedding-", 0) == 0) {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      std::string key(arg.substr(2));
      std::replace(key.begin(), key.end(), '-', '_');
      static const std::set<std::string> names = {
          "embedding_variant", "embedding_provider", "embedding_model", "embedding_attribute_key",
          "embedding_event_id_key", "embedding_dimensions",
          "embedding_max_items", "embedding_keep_cases"};
      if (!names.contains(key)) return usage(argv[0]);
      embedding_fields.insert_or_assign(std::move(key), value);
      embeddings_requested = true;
      continue;
    }
    if (arg == "--label-splitting") { label_splitting_requested = true; continue; }
    if (arg.rfind("--label-splitting-", 0) == 0) {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      std::string key(arg.substr(2));
      std::replace(key.begin(), key.end(), '-', '_');
      static const std::set<std::string> names = {
          "label_splitting_variant", "label_splitting_activity_key",
          "label_splitting_target_key", "label_splitting_prefix_length",
          "label_splitting_suffix_length", "label_splitting_min_edge_weight",
          "label_splitting_activities_suffix", "label_splitting_target_activities"};
      if (!names.contains(key)) return usage(argv[0]);
      label_splitting_fields.insert_or_assign(std::move(key), value);
      label_splitting_requested = true;
      continue;
    }
    if (arg == "--trie") { trie_requested = true; continue; }
    if (arg == "--trie-to-petri") {
      trie_requested = true;
      trie_to_petri_requested = true;
      continue;
    }
    if (arg.rfind("--trie-", 0) == 0) {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      std::string key(arg.substr(2));
      std::replace(key.begin(), key.end(), '-', '_');
      static const std::set<std::string> trie_option_names = {
          "trie_activity_key", "trie_max_path_length"};
      if (!trie_option_names.contains(key)) return usage(argv[0]);
      trie_fields.insert_or_assign(std::move(key), value);
      trie_requested = true;
      continue;
    }
    if (arg == "--interval-tree") { interval_requested = true; continue; }
    if (arg.rfind("--interval-", 0) == 0) {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      std::string key(arg.substr(2));
      std::replace(key.begin(), key.end(), '-', '_');
      static const std::set<std::string> interval_option_names = {
          "interval_activity_key", "interval_start_timestamp_key",
          "interval_timestamp_key", "interval_epsilon",
          "interval_filter_activity_couple", "interval_query_point",
          "interval_query_start", "interval_query_end"};
      if (!interval_option_names.contains(key)) return usage(argv[0]);
      interval_fields.insert_or_assign(std::move(key), value);
      interval_requested = true;
      continue;
    }
    if (arg == "--merge-case-relations") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      merge_incoming_path = value;
      merge_requested = true;
      continue;
    }
    if (arg == "--timestamp-interleavings") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      timestamp_interleavings_right_path = value;
      timestamp_interleavings_requested = true;
      continue;
    }
    if (arg == "--interleavings-timestamp-key" ||
        arg == "--interleavings-max-results") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      std::string key(arg.substr(2));
      std::replace(key.begin(), key.end(), '-', '_');
      timestamp_interleavings_fields.insert_or_assign(std::move(key), value);
      timestamp_interleavings_requested = true;
      continue;
    }
    if (arg == "--case-relations") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      merge_relations_path = value;
      merge_requested = true;
      continue;
    }
    if (arg == "--merge-variant" || arg == "--merge-timestamp-key" ||
        arg == "--merge-case-id-key" || arg == "--merge-left-suffix" ||
        arg == "--merge-right-suffix" || arg == "--merge-left-column" ||
        arg == "--merge-right-column") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      std::string key(arg.substr(2));
      std::replace(key.begin(), key.end(), '-', '_');
      merge_fields.insert_or_assign(std::move(key), value);
      merge_requested = true;
      continue;
    }
    if (arg == "--random-tree") {
      random_tree_requested = true;
      continue;
    }
    if (arg.rfind("--random-tree-", 0) == 0) {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      std::string key(arg.substr(2));
      std::replace(key.begin(), key.end(), '-', '_');
      static const std::set<std::string> random_tree_option_names = {
          "random_tree_variant",        "random_tree_rec_depth",
          "random_tree_min_rec_depth",
          "random_tree_max_rec_depth",   "random_tree_prob_leaf",
          "random_tree_mode",            "random_tree_min",
          "random_tree_max",             "random_tree_sequence",
          "random_tree_choice",          "random_tree_parallel",
          "random_tree_loop",            "random_tree_or",
          "random_tree_silent",           "random_tree_duplicate",
          "random_tree_no_models",        "random_tree_seed",
          "random_tree_activities"};
      if (!random_tree_option_names.contains(key)) return usage(argv[0]);
      random_tree_fields.insert_or_assign(std::move(key), value);
      random_tree_requested = true;
      continue;
    }

    if (arg == "--evaluation-family" || arg == "--fitness-variant" ||
        arg == "--evaluation-activity-attribute" ||
        arg == "--evaluation-case-id-attribute" ||
        arg == "--evaluation-max-rows-to-materialize" ||
        arg == "--evaluation-initial-marking" ||
        arg == "--evaluation-final-marking" ||
        arg == "--evaluation-token-replay-variant" ||
        arg == "--fitness-consider-unknown-activities" ||
        arg == "--fitness-cleaning-token-flood" ||
        arg == "--evaluation-alignment-variant" ||
        arg == "--evaluation-alignment-max-states" ||
        arg == "--evaluation-alignment-max-total-states" ||
        arg == "--evaluation-alignment-max-time" ||
        arg == "--evaluation-alignment-max-time-trace" ||
        arg == "--evaluation-alignment-max-alignments" ||
        arg == "--evaluation-alignment-exponent" ||
        arg == "--evaluation-alignment-theta" ||
        arg == "--evaluation-alignment-synchronous-dijkstra" ||
        arg == "--evaluation-alignment-cost-profile" ||
        arg == "--evaluation-alignment-trace-cost" ||
        arg == "--evaluation-alignment-model-cost" ||
        arg == "--evaluation-alignment-silent-model-cost" ||
        arg == "--evaluation-alignment-sync-cost" ||
        arg == "--evaluation-alignment-threshold-border-agreement" ||
        arg == "--evaluation-alignment-use-cache" ||
        arg == "--evaluation-alignment-clear-cache" ||
        arg == "--precision-variant" ||
        arg == "--precision-use-task-ids" ||
        arg == "--precision-max-states" ||
        arg == "--generalization-variant" ||
        arg == "--simplicity-variant" ||
        arg == "--simplicity-arc-degree-k" ||
        arg == "--simplicity-max-states" ||
        arg == "--simplicity-initial-marking" ||
        arg == "--simplicity-final-marking" ||
        arg == "--fitness-weight" || arg == "--precision-weight" ||
        arg == "--simplicity-weight" || arg == "--generalization-weight") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      std::string key(arg.substr(2));
      std::replace(key.begin(), key.end(), '-', '_');
      evaluation_fields.insert_or_assign(std::move(key), value);
      evaluation_requested = true;
      continue;
    }
    if (arg.rfind("--petri-analysis-", 0) == 0) {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      std::string key(arg.substr(2));
      std::replace(key.begin(), key.end(), '-', '_');
      petri_analysis_fields.insert_or_assign(std::move(key), value);
      petri_analysis_requested = true;
      continue;
    }
    if (arg == "--emd-reference") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      emd_reference = value;
      continue;
    }
    if (arg == "--emd-variant") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      std::string variant(value);
      std::transform(variant.begin(), variant.end(), variant.begin(),
                     [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                     });
      if (variant == "exact") {
        emd_options.variant = minera::EmdVariant::EXACT;
      } else if (variant == "pyemd") {
        emd_options.variant = minera::EmdVariant::PYEMD;
      } else {
        std::cerr << "error: --emd-variant expects exact or pyemd\n";
        return 2;
      }
      emd_option_used = true;
      continue;
    }
    if (arg == "--compare-petri-reference") {
      const auto* value = next();
      if (!value || *value == '\0') return usage(argv[0]);
      compare_petri_reference = value;
      continue;
    }
    if (arg == "--token-replay-model" || arg == "--replay-fitness-model") {
      const auto* value = next();
      if (!value || *value == '\0') return usage(argv[0]);
      token_replay_model = value;
      if (arg == "--replay-fitness-model") {
        replay_fitness_result_requested = true;
        evaluation_requested = true;
      }
      continue;
    }
    if (arg == "--compare-petri-model") {
      const auto* value = next();
      if (!value || *value == '\0') return usage(argv[0]);
      compare_petri_model = value;
      continue;
    }
    if (arg == "--compare-petri-miner") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      compare_petri_miner = value;
      compare_petri_miner_used = true;
      if (compare_petri_miner != "inductive" && compare_petri_miner != "alpha" &&
          compare_petri_miner != "heuristic" && compare_petri_miner != "powl" &&
          compare_petri_miner != "imported") {
        std::cerr << "error: --compare-petri-miner expects inductive, alpha, heuristic, powl, or imported\n";
        return 2;
      }
      continue;
    }
    if (arg == "--compare-petri-activity-attribute") {
      const auto* value = next();
      if (!value) {
        std::cerr << "error: --compare-petri-activity-attribute expects a non-empty name\n";
        return 2;
      }
      compare_petri_activity_attribute = trim_text(value);
      if (compare_petri_activity_attribute.empty()) {
        std::cerr << "error: --compare-petri-activity-attribute expects a non-empty name\n";
        return 2;
      }
      continue;
    }
    if (arg == "--compare-petri-initial-marking" ||
        arg == "--compare-petri-final-marking") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      try {
        const auto marking = parse_uint32_marking(value, arg);
        if (arg == "--compare-petri-initial-marking") {
          compare_petri_initial_marking = marking;
          compare_petri_initial_marking_used = true;
        } else {
          compare_petri_final_marking = marking;
          compare_petri_final_marking_used = true;
        }
      } catch (const std::invalid_argument& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 2;
      }
      continue;
    }
    if (arg == "--compare-petri-walk-through-hidden" ||
        arg == "--compare-petri-reach-final-through-hidden" ||
        arg == "--compare-petri-stop-immediately-unfit" ||
        arg == "--compare-petri-cleaning-token-flood" ||
        arg == "--compare-petri-exhaustive-invisible-exploration") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      try {
        const bool parsed = parse_boolean(value, arg);
        if (arg == "--compare-petri-walk-through-hidden") {
          compare_petri_walk_through_hidden = parsed;
        } else if (arg == "--compare-petri-reach-final-through-hidden") {
          compare_petri_reach_final_through_hidden = parsed;
        } else if (arg == "--compare-petri-stop-immediately-unfit") {
          compare_petri_stop_immediately_unfit = parsed;
        } else if (arg == "--compare-petri-cleaning-token-flood") {
          compare_petri_cleaning_token_flood = parsed;
        } else {
          compare_petri_exhaustive_invisible_exploration = parsed;
        }
        compare_petri_replay_option_used = true;
      } catch (const std::invalid_argument& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 2;
      }
      continue;
    }
    if (arg == "--emd-normalize-masses" ||
        arg == "--emd-strict-equal-mass" ||
        arg == "--emd-use-fast-emd") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      try {
        const bool parsed = parse_boolean(value, arg);
        if (arg == "--emd-normalize-masses") {
          emd_options.normalize_masses = parsed;
        } else if (arg == "--emd-strict-equal-mass") {
          emd_options.strict_equal_mass = parsed;
        } else {
          emd_options.use_fast_emd = parsed;
        }
        emd_option_used = true;
      } catch (const std::invalid_argument& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 2;
      }
      continue;
    }

    // Keep the decision activity selector outside the legacy parser chain,
    // which is already close to MSVC's block-nesting limit.
    if (arg == "--decision-activity-attribute") {
      const auto* value = next();
      if (!value || trim_text(value).empty()) {
        std::cerr << "error: " << arg
                  << " expects a non-empty name\n";
        return 2;
      }
      decision_options.activity_attribute = trim_text(value);
      decision_option_used = true;
      decision_activity_option_used = true;
      continue;
    }

    if (arg == "--dfg-dataframe-output") {
      const auto* value = next();
      if (!value || std::string_view(value).empty()) return usage(argv[0]);
      dfg_dataframe_output = value;
      continue;
    }
    if (arg == "--dfg-operation" || arg == "--dfg-measure" ||
        arg == "--dfg-backend" ||
        arg == "--dfg-target-activity-attribute" ||
        arg == "--dfg-cost-attribute" || arg == "--dfg-case-id-attribute" ||
        arg == "--dfg-sort-caseid-required" ||
        arg == "--dfg-sort-timestamp-along-case-id" ||
        arg == "--dfg-reduce-columns" || arg == "--dfg-workcalendar" ||
        arg == "--dfg-event-index-attribute" ||
        arg == "--dfg-max-start-column" ||
        arg == "--dfg-min-complete-column" ||
        arg == "--dfg-overlap-column" || arg == "--dfg-strict" ||
        arg == "--dfg-keep-first-following") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      std::string key(arg.substr(2));
      std::replace(key.begin(), key.end(), '-', '_');
      dfg_dataframe_fields.insert_or_assign(std::move(key), value);
      dfg_dataframe_option_used = true;
      continue;
    }
    if (arg == "--dfg-playout-variant" || arg == "--dfg-playout-traces" ||
        arg == "--dfg-playout-max-events-per-trace" ||
        arg == "--dfg-playout-max-steps" ||
        arg == "--dfg-playout-max-variants" ||
        arg == "--dfg-playout-min-weighted-probability" ||
        arg == "--dfg-playout-max-occurrences-per-activity" ||
        arg == "--dfg-playout-max-execution-time-ms" ||
        arg == "--dfg-playout-case-arrival-rate" ||
        arg == "--dfg-playout-seed" || arg == "--dfg-playout-start-time" ||
        arg == "--dfg-playout-activity-key" ||
        arg == "--dfg-playout-timestamp-key" ||
        arg == "--dfg-playout-case-id-key" ||
        arg == "--dfg-playout-artificial-start-activity" ||
        arg == "--dfg-playout-artificial-end-activity" ||
        arg == "--dfg-playout-min-variant-occurrences" ||
        arg == "--dfg-playout-interrupt-when-dfg-complete" ||
        arg == "--dfg-playout-add-trace-if-takes-new-edges" ||
        arg == "--dfg-playout-return-variants" ||
        arg == "--dfg-playout-return-only-if-complete") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      std::string key(arg.substr(2));
      std::replace(key.begin(), key.end(), '-', '_');
      dfg_playout_fields.insert_or_assign(std::move(key), value);
      dfg_playout_options_used = true;
      continue;
    }

    if (arg == "--petri-playout-traces" ||
        arg == "--petri-playout-max-steps" ||
        arg == "--petri-playout-seed" ||
        arg == "--petri-playout-initial-marking" ||
        arg == "--petri-playout-final-marking" ||
        arg == "--petri-playout-start" ||
        arg == "--petri-playout-case-interval" ||
        arg == "--petri-playout-event-interval" ||
        arg == "--petri-playout-add-only-if-fm-is-reached" ||
        arg == "--petri-playout-fm-leq-accepted" ||
        arg == "--petri-playout-return-visited-elements") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      std::string key(arg.substr(std::string_view("--petri-playout-").size()));
      std::replace(key.begin(), key.end(), '-', '_');
      basic_playout_fields.insert_or_assign(std::move(key), value);
      basic_playout_options_used = true;
      continue;
    }
    if (arg == "--petri-playout-mode" ||
        arg == "--petri-playout-max-length" ||
        arg == "--petri-playout-return-elements" ||
        arg == "--petri-playout-max-marking-occ") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      if (arg == "--petri-playout-mode") {
        petri_playout_mode = value;
        if (petri_playout_mode != "basic" &&
            petri_playout_mode != "extensive") {
          std::cerr << "error: --petri-playout-mode expects basic or extensive\n";
          return 2;
        }
      } else {
        std::string key = arg == "--petri-playout-return-elements"
                              ? "extensive_return_elements"
                              : arg == "--petri-playout-max-length"
                                    ? "extensive_max_length"
                                    : "extensive_max_marking_occ";
        extensive_playout_fields.insert_or_assign(std::move(key), value);
      }
      extensive_playout_options_used = true;
      continue;
    }

    if (arg == "--stochastic-traces" || arg == "--stochastic-max-steps" ||
        arg == "--stochastic-seed" ||
        arg == "--stochastic-transition-weights" ||
        arg == "--stochastic-fit-from-log" ||
        arg == "--stochastic-stop-at-final" ||
        arg == "--stochastic-return-visited-elements" ||
        arg == "--stochastic-start" ||
        arg == "--stochastic-case-interval" ||
        arg == "--stochastic-event-interval" ||
        arg == "--stochastic-initial-marking" ||
        arg == "--stochastic-final-marking") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      std::string key(arg.substr(2));
      std::replace(key.begin(), key.end(), '-', '_');
      stochastic_playout_fields.insert_or_assign(std::move(key), value);
      stochastic_playout_requested = true;
      continue;
    }
    if (arg == "--stochastic-petri" ||
        arg == "--stochastic-petri-rates" ||
        arg == "--stochastic-petri-initial-marking" ||
        arg == "--stochastic-petri-immediate-transitions" ||
        arg == "--stochastic-petri-max-states" ||
        arg == "--stochastic-petri-analysis" ||
        arg == "--stochastic-petri-time" ||
        arg == "--stochastic-petri-initial-state" ||
        arg == "--stochastic-petri-initial-probabilities") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      if (arg == "--stochastic-petri") {
        if (std::string_view(value) != "true" &&
            std::string_view(value) != "false" &&
            std::string_view(value) != "1" &&
            std::string_view(value) != "0") {
          std::cerr << "error: --stochastic-petri expects true or false\n";
          return 2;
        }
        if (std::string_view(value) == "true" ||
            std::string_view(value) == "1")
          stochastic_petri_requested = true;
      } else {
        std::string key(arg.substr(2));
        std::replace(key.begin(), key.end(), '-', '_');
        stochastic_petri_fields.insert_or_assign(std::move(key), value);
        stochastic_petri_requested = true;
      }
      continue;
    }
    if (arg == "--stochastic-map") {
      const auto* value = next();
      if (!value || *value == '\0') return usage(argv[0]);
      stochastic_map_output = value;
      stochastic_map_requested = true;
      continue;
    }
    if (arg == "--stochastic-map-source" ||
        arg == "--stochastic-map-miner" ||
        arg == "--stochastic-map-distribution" ||
        arg == "--stochastic-map-default-parameter" ||
        arg == "--stochastic-map-silent-transitions-immediate" ||
        arg == "--stochastic-map-zero-duration-immediate" ||
        arg == "--stochastic-map-fit-activation-weights" ||
        arg == "--stochastic-map-fit-timing" ||
        arg == "--stochastic-map-default-activation-weight" ||
        arg == "--stochastic-map-frequency-activation-weights" ||
        arg == "--stochastic-map-conversion-variant" ||
        arg == "--stochastic-map-max-states" ||
        arg == "--stochastic-map-initial-marking") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      if (arg == "--stochastic-map-miner") {
        stochastic_map_miner = value;
      } else {
        std::string key(arg.substr(2));
        std::replace(key.begin(), key.end(), '-', '_');
        stochastic_map_fields.insert_or_assign(std::move(key), value);
      }
      stochastic_map_requested = true;
      continue;
    }
    if (arg == "--monte-carlo") {
      monte_carlo_requested = true;
      playout_miner = "monte_carlo";
      continue;
    }
    if (arg == "--monte-carlo-json") {
      monte_carlo_requested = true;
      monte_carlo_json_requested = true;
      continue;
    }
    if (arg == "--monte-carlo-simulations" ||
        arg == "--monte-carlo-activity-key" ||
        arg == "--monte-carlo-timestamp-key" ||
        arg == "--monte-carlo-initial-marking" ||
        arg == "--monte-carlo-final-marking" ||
        arg == "--monte-carlo-case-arrival-seconds" ||
        arg == "--monte-carlo-case-arrival-ratio" ||
        arg == "--monte-carlo-max-steps" || arg == "--monte-carlo-seed" ||
        arg == "--monte-carlo-start" ||
        arg == "--monte-carlo-default-duration-seconds" ||
        arg == "--monte-carlo-force-distribution" ||
        arg == "--monte-carlo-transition-weights" ||
        arg == "--monte-carlo-transition-durations" ||
        arg == "--monte-carlo-resources-per-place" ||
        arg == "--monte-carlo-default-resources-per-place" ||
        arg == "--monte-carlo-enable-diagnostics" ||
        arg == "--monte-carlo-stop-at-final" ||
        arg == "--monte-carlo-provided-stochastic-map" ||
        arg == "--monte-carlo-diagnostic-interval" ||
        arg == "--monte-carlo-small-scale-factor" ||
        arg == "--monte-carlo-max-thread-execution-time" ||
        arg == "--monte-carlo-token-replay-variant") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      std::string key(arg.substr(2));
      std::replace(key.begin(), key.end(), '-', '_');
      monte_carlo_fields.insert_or_assign(std::move(key), value);
      monte_carlo_requested = true;
      continue;
    }
    if (arg == "--playout-variant") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      if (std::string_view(value) == "stochastic") {
        stochastic_playout_requested = true;
      } else if (std::string_view(value) == "monte_carlo") {
        monte_carlo_requested = true;
        playout_miner = "monte_carlo";
      } else {
        process_tree_playout_fields.insert_or_assign("playout_variant", value);
        process_tree_playout_options_used = true;
      }
      continue;
    }

    // Native alignment adapters are handled before the legacy option chain to
    // keep this already-large parser within MSVC's block nesting limit.
    if (arg == "--process-tree-reduction-activity-key") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      reduction_activity_key = value;
      continue;
    }
    if (arg == "--advanced-petri-miner") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      advanced_petri_miner = value;
      advanced_petri_miner_used = true;
      if (advanced_petri_miner != "inductive" &&
          advanced_petri_miner != "alpha" &&
          advanced_petri_miner != "heuristic" &&
          advanced_petri_miner != "powl" &&
          advanced_petri_miner != "imported") {
        return usage(argv[0]);
      }
      continue;
    }
    if (arg == "--dfg-activity-attribute" ||
        arg == "--dfg-timestamp-attribute" ||
        arg == "--edit-activity-attribute" ||
        arg == "--edit-timestamp-attribute" ||
        arg == "--decomposed-activity-attribute" ||
        arg == "--petri-activity-attribute" ||
        arg == "--log-skeleton-activity-attribute" ||
        arg == "--declare-activity-attribute") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      const auto selector = trim_text(value);
      if (selector.empty()) {
        std::cerr << "error: " << arg
                  << " expects a non-empty attribute name\n";
        return 2;
      }
      if (arg == "--dfg-activity-attribute") {
        dfg_alignment_options.activity_attribute = selector;
        dfg_statistics_options.activity_attribute = selector;
        dfg_dataframe_fields["dfg_activity_attribute"] = selector;
      } else if (arg == "--dfg-timestamp-attribute") {
        dfg_alignment_options.timestamp_attribute = selector;
        dfg_statistics_options.timestamp_attribute = selector;
        dfg_dataframe_fields["dfg_timestamp_attribute"] = selector;
      } else if (arg == "--edit-activity-attribute") {
        edit_options.activity_attribute = selector;
        edit_option_used = true;
      } else if (arg == "--edit-timestamp-attribute") {
        edit_options.timestamp_attribute = selector;
        edit_option_used = true;
      } else if (arg == "--decomposed-activity-attribute") {
        decomposed_options.activity_attribute = selector;
        align_decomposed = true;
        decomposed_activity_option_used = true;
      } else if (arg == "--petri-activity-attribute") {
        petri_anti_options.activity_attribute = selector;
        petri_multi_options.activity_attribute = selector;
        petri_option_used = true;
        petri_activity_option_used = true;
      } else if (arg == "--log-skeleton-activity-attribute") {
        log_skeleton_options.activity_key = selector;
      } else {
        declare_options.activity_key = selector;
      }
      continue;
    }
    if (arg == "--align-decomposed" || arg == "--align-petri-anti" || arg == "--align-petri-multi" || arg == "--reduce-process-tree") {
      if (arg == "--align-decomposed") align_decomposed = true;
      if (arg == "--align-petri-anti") align_petri_anti = true;
      if (arg == "--align-petri-multi") align_petri_multi = true;
      if (arg == "--reduce-process-tree") { const auto* v=next(); if(!v) return usage(argv[0]); std::size_t idx=0; const auto* e=v+std::strlen(v); const auto r=std::from_chars(v,e,idx); if(r.ec!=std::errc{}||r.ptr!=e) return usage(argv[0]); reduce_trace_index=idx; }
      continue;
    }
    if (arg == "--decomposed-activity-log-cost" ||
        arg == "--decomposed-activity-model-cost" ||
        arg == "--decomposed-activity-sync-cost") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      try {
        const auto costs = minera::parse_activity_costs(value, arg);
        auto* destination = &decomposed_options.activity_log_costs;
        if (arg == "--decomposed-activity-model-cost") {
          destination = &decomposed_options.activity_model_costs;
        } else if (arg == "--decomposed-activity-sync-cost") {
          destination = &decomposed_options.activity_sync_costs;
        }
        for (const auto& [activity, cost] : costs) {
          if (activity == ">>") return usage(argv[0]);
          destination->insert_or_assign(activity, cost);
        }
      } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 2;
      }
      align_decomposed = true;
      continue;
    }
    if (arg == "--decomposed-variant" || arg == "--decomposed-initial-marking" || arg == "--decomposed-final-marking" || arg == "--decomposed-max-states" || arg == "--decomposed-max-total-states" || arg == "--decomposed-max-time-trace" || arg == "--decomposed-max-time-total" || arg == "--decomposed-threshold-border-agreement" || arg == "--decomposed-use-cache" || arg == "--decomposed-clear-cache" || arg == "--decomposed-log-move-cost" || arg == "--decomposed-model-move-cost" || arg == "--decomposed-tau-model-cost" || arg == "--decomposed-sync-cost" || arg == "--petri-exponent" || arg == "--petri-epsilon" || arg == "--petri-marking-limit" || arg == "--petri-max-states" || arg == "--petri-initial-marking" || arg == "--petri-final-marking") {
      try {
      const auto* v=next(); if(!v) return usage(argv[0]); const std::string_view a=arg;
      if(a=="--decomposed-variant"){if(std::string_view(v)!="recompos_maximal")return usage(argv[0]);align_decomposed=true;}
      if(a=="--decomposed-max-states"){decomposed_options.max_states=parse_positive_size(v,arg);align_decomposed=true;}
      if(a=="--decomposed-max-total-states"){decomposed_options.max_total_states=parse_positive_size(v,arg);align_decomposed=true;}
      if(a=="--decomposed-max-time-trace"||a=="--decomposed-max-time-total"||a=="--decomposed-threshold-border-agreement"){char*e=nullptr;const double d=std::strtod(v,&e);if(e==v||*e!='\0'||!std::isfinite(d)||d<0)return usage(argv[0]);if(a=="--decomposed-max-time-trace")decomposed_options.max_time_trace=d;if(a=="--decomposed-max-time-total")decomposed_options.max_time_total=d;if(a=="--decomposed-threshold-border-agreement")decomposed_options.threshold_border_agreement=d;align_decomposed=true;}
      if(a=="--decomposed-use-cache"){decomposed_options.use_cache=parse_boolean(v,arg);align_decomposed=true;} if(a=="--decomposed-clear-cache"){decomposed_options.clear_cache=parse_boolean(v,arg);align_decomposed=true;}
      if(a=="--decomposed-log-move-cost"||a=="--decomposed-model-move-cost"||a=="--decomposed-tau-model-cost"||a=="--decomposed-sync-cost"){char*e=nullptr;const double d=std::strtod(v,&e);if(e==v||*e!='\0'||!std::isfinite(d)||d<0)return usage(argv[0]);if(a=="--decomposed-log-move-cost")decomposed_options.log_move_cost=d;if(a=="--decomposed-model-move-cost")decomposed_options.model_move_cost=d;if(a=="--decomposed-tau-model-cost")decomposed_options.tau_model_cost=d;if(a=="--decomposed-sync-cost")decomposed_options.sync_cost=d;align_decomposed=true;}
      if(a=="--decomposed-initial-marking"||a=="--decomposed-final-marking"){std::vector<std::size_t> marks;std::size_t b=0;const std::string s=v;for(std::size_t j=0;j<=s.size();++j)if(j==s.size()||s[j]==','){marks.push_back(parse_nonnegative_size(s.substr(b,j-b).c_str(),arg));b=j+1;}if(a=="--decomposed-initial-marking")decomposed_options.initial_marking=marks;else decomposed_options.final_marking=marks;align_decomposed=true;}
      if(a=="--petri-exponent"){petri_option_used=true;char*e=nullptr;const double d=std::strtod(v,&e);if(e==v||*e!='\0'||!std::isfinite(d)||d<=1)return usage(argv[0]);petri_anti_options.exponent=d;petri_multi_options.exponent=d;}
      if(a=="--petri-epsilon"){petri_option_used=true;char*e=nullptr;const double d=std::strtod(v,&e);if(e==v||*e!='\0'||!std::isfinite(d)||d<0)return usage(argv[0]);petri_anti_options.epsilon=d;}
      if(a=="--petri-marking-limit"){petri_option_used=true;petri_anti_options.marking_limit=petri_multi_options.marking_limit=parse_positive_size(v,arg);}
      if(a=="--petri-max-states"){petri_option_used=true;const auto n=parse_nonnegative_size(v,arg);petri_anti_options.max_states=petri_multi_options.max_states=n;}
      if(a=="--petri-initial-marking"||a=="--petri-final-marking"){petri_option_used=true;std::vector<std::size_t> marks;std::size_t b=0;const std::string s=v;for(std::size_t j=0;j<=s.size();++j)if(j==s.size()||s[j]==','){marks.push_back(parse_nonnegative_size(s.substr(b,j-b).c_str(),arg));b=j+1;}if(a=="--petri-initial-marking"){petri_anti_options.initial_marking=marks;petri_multi_options.initial_marking=marks;}else{petri_anti_options.final_marking=marks;petri_multi_options.final_marking=marks;}}
      } catch (const std::exception& e) { std::cerr << "error: " << e.what() << '\n'; return 2; }
      continue;
    }
    // Keep this option outside the legacy chain below; that chain is close to
    // the compiler's block-nesting limit on Windows.
    if (arg == "--process-tree-variant") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      try {
        process_tree_variant =
            minera::parse_process_tree_petri_net_variant(value);
      } catch (const std::invalid_argument& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 2;
      }
      continue;
    }

    if (arg == "--xes-import-variant" || arg == "--xes-timestamp-sort" ||
        arg == "--xes-return-legacy-log-object" ||
        arg == "--xes-return-pl-lazyframe" ||
        arg == "--xes-conversion-case-id-key" ||
        arg == "--xes-conversion-case-attribute-prefix" ||
        arg == "--xes-conversion-null-case-id-key" ||
        arg == "--xes-conversion-null-case-attribute-prefix" ||
        arg == "--xes-conversion-stream-postprocessing" ||
        arg == "--xes-conversion-deepcopy" ||
        arg == "--xes-conversion-include-case-attributes" ||
        arg == "--xes-conversion-compress" ||
        arg == "--xes-conversion-infer-extensions" ||
        arg == "--xes-conversion-extension" ||
        arg == "--xes-reverse-sort" || arg == "--xes-max-traces" ||
        arg == "--xes-attributes-to-read" || arg == "--xes-case-attribute" ||
        arg == "--xes-activity-attribute" ||
        arg == "--xes-timestamp-attribute" ||
        arg == "--xes-resource-attribute" ||
        arg == "--xes-lifecycle-attribute" ||
        arg == "--xes-require-timestamp" ||
        arg == "--xes-max-decompressed-bytes" ||
        arg == "--xes-export-variant" || arg == "--xes-compress" ||
        arg == "--xes-infer-metadata" || arg == "--xes-preserve-nulls") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      try {
        if (arg == "--xes-return-legacy-log-object") {
          xes_options.return_legacy_log_object = parse_boolean(value, arg);
          xes_import_result_requested = true;
        }
        if (arg == "--xes-return-pl-lazyframe") {
          xes_options.return_pl_lazyframe = parse_boolean(value, arg);
          xes_import_result_requested = true;
        }
        if (arg == "--xes-conversion-case-id-key")
          xes_options.conversion_case_id_key = value;
        if (arg == "--xes-conversion-case-attribute-prefix")
          xes_options.conversion_case_attribute_prefix = value;
        if (arg == "--xes-conversion-null-case-id-key")
          xes_options.conversion_case_id_key = parse_boolean(value, arg)
              ? std::nullopt : std::optional<std::string>{"case:concept:name"};
        if (arg == "--xes-conversion-null-case-attribute-prefix")
          xes_options.conversion_case_attribute_prefix = parse_boolean(value, arg)
              ? std::nullopt : std::optional<std::string>{"case:"};
        if (arg == "--xes-conversion-stream-postprocessing")
          xes_options.conversion_stream_postprocessing = parse_boolean(value, arg);
        if (arg == "--xes-conversion-deepcopy")
          xes_options.conversion_deepcopy = parse_boolean(value, arg);
        if (arg == "--xes-conversion-include-case-attributes")
          xes_options.conversion_include_case_attributes = parse_boolean(value, arg);
        if (arg == "--xes-conversion-compress")
          xes_options.conversion_compress = parse_boolean(value, arg);
        if (arg == "--xes-conversion-infer-extensions") {
          if (parse_boolean(value, arg)) xes_options.conversion_extensions.reset();
          else xes_options.conversion_extensions = std::vector<minera::LogExtension>{};
        }
        if (arg == "--xes-conversion-extension") {
          const auto* prefix = next();
          if (!prefix) return usage(argv[0]);
          const auto* uri = next();
          if (!uri) return usage(argv[0]);
          if (!xes_options.conversion_extensions) xes_options.conversion_extensions.emplace();
          xes_options.conversion_extensions->push_back({value, prefix, uri});
        }
        if (arg == "--xes-import-variant")
          xes_options.variant = minera::parse_xes_import_variant(value);
        if (arg == "--xes-timestamp-sort")
          xes_options.timestamp_sort = parse_boolean(value, arg);
        if (arg == "--xes-reverse-sort")
          xes_options.reverse_sort = parse_boolean(value, arg);
        if (arg == "--xes-max-traces")
          xes_options.max_traces = parse_nonnegative_size(value, arg);
        if (arg == "--xes-attributes-to-read")
          xes_options.attributes_to_read = split_csv(value);
        if (arg == "--xes-case-attribute") xes_options.case_attribute = value;
        if (arg == "--xes-activity-attribute")
          xes_options.activity_attribute = value;
        if (arg == "--xes-timestamp-attribute")
          xes_options.timestamp_attribute = value;
        if (arg == "--xes-resource-attribute")
          xes_options.resource_attribute = value;
        if (arg == "--xes-lifecycle-attribute")
          xes_options.lifecycle_attribute = value;
        if (arg == "--xes-require-timestamp")
          xes_options.require_timestamp = parse_boolean(value, arg);
        if (arg == "--xes-max-decompressed-bytes")
          xes_options.max_decompressed_bytes = parse_positive_size(value, arg);
        if (arg == "--xes-export-variant")
          xes_export_options.variant = minera::parse_xes_export_variant(value);
        if (arg == "--xes-compress")
          xes_export_options.compress = parse_boolean(value, arg);
        if (arg == "--xes-infer-metadata")
          xes_export_options.infer_metadata = parse_boolean(value, arg);
        if (arg == "--xes-preserve-nulls")
          xes_export_options.preserve_nulls = parse_boolean(value, arg);
      } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 2;
      }
      continue;
    }

    if (arg == "-o") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      output = value;
    } else if (arg == "--case-column") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      options.case_id_column = value;
    } else if (arg == "--activity-column") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      options.activity_column = value;
    } else if (arg == "--timestamp-column") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      options.timestamp_column = value;
    } else if (arg == "--resource-column") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      options.resource_column = value;
    } else if (arg == "--pnml") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      pnml_output = value;
    } else if (arg == "--check-pnml" || arg == "--pnml-public-markings") {
      if (arg == "--pnml-public-markings") {
        pnml_public_markings = true;
      } else {
        const auto* value = next();
        if (!value) return usage(argv[0]);
        check_pnml = value;
      }
    } else if (arg == "--check-bpmn") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      check_bpmn = value;
    } else if (arg == "--ptml") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      ptml_output = value;
    } else if (arg == "--powl-ptml") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      powl_ptml_output = value;
    } else if (arg == "--check-ptml") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      check_ptml = value;
    } else if (arg == "--bpmn") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      bpmn_output = value;
    } else if (arg == "--xes") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      xes_output = value;
    } else if (arg == "--playout") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      playout_output = value;
    } else if (arg == "--pnml-miner") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      pnml_miner = value;
    } else if (arg == "--bpmn-miner") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      bpmn_miner = value;
    } else if (arg == "--playout-miner") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      playout_miner = value;
      if (playout_miner == "monte_carlo") monte_carlo_requested = true;
    } else if (arg == "--declare-playout-traces" ||
               arg == "--declare-playout-min-length" ||
               arg == "--declare-playout-max-length") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      try {
        const int parsed = parse_declare_playout_int(
            value, arg, arg == "--declare-playout-traces");
        if (arg == "--declare-playout-traces") {
          declare_playout_options.n_traces = parsed;
        } else if (arg == "--declare-playout-min-length") {
          declare_playout_options.min_length = parsed;
        } else {
          declare_playout_options.max_length = parsed;
        }
      } catch (const std::invalid_argument& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 2;
      }
      declare_playout_options_used = true;
    } else if (arg == "--declare-playout-seed") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      try {
        declare_playout_options.seed = parse_declare_playout_seed(value);
      } catch (const std::invalid_argument& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 2;
      }
      declare_playout_options_used = true;
    } else if (arg == "--playout-traces" ||
               arg == "--playout-max-steps" ||
               arg == "--playout-seed" || arg == "--playout-min-length" ||
               arg == "--playout-max-length" ||
               arg == "--playout-max-loop-occurrences" ||
               arg == "--playout-max-traces" ||
               arg == "--playout-max-trace-cap" ||
               arg == "--playout-return-set-strings") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      std::string key(arg.substr(2));
      std::replace(key.begin(), key.end(), '-', '_');
      process_tree_playout_fields.insert_or_assign(std::move(key), value);
      process_tree_playout_options_used = true;
    } else if (arg == "--decision-no-trace-attributes") {
      decision_options.include_trace_attributes = false;
      decision_option_used = true;
    } else if (arg == "--decision-attributes") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      decision_options.attributes = split_csv(value);
      if (decision_options.attributes.empty() ||
          std::any_of(decision_options.attributes.begin(),
                      decision_options.attributes.end(),
                      [](const std::string& item) { return item.empty(); })) {
        throw std::invalid_argument(
            "--decision-attributes expects non-empty names");
      }
      decision_option_used = true;
    } else if (arg == "--decision-max-depth" ||
               arg == "--decision-min-split" ||
               arg == "--decision-min-leaf" ||
               arg == "--decision-max-categories" ||
               arg == "--decision-max-thresholds" ||
               arg == "--decision-min-gain" ||
               arg == "--decision-lookback") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      decision_option_used = true;
      if (arg == "--decision-lookback") {
        decision_lookback = parse_positive_size(value, arg);
        if (decision_lookback > minera::kMaxDecisionLookback)
          throw std::invalid_argument(
              "--decision-lookback expects an integer from 1 through 64");
        decision_lookback_used = true;
      } else if (arg == "--decision-max-depth") {
        decision_options.max_depth = parse_nonnegative_size(value, arg);
        if (decision_options.max_depth > 64)
          throw std::invalid_argument(
              "--decision-max-depth expects an integer from 0 through 64");
      } else if (arg == "--decision-min-split") {
        decision_options.min_split = parse_positive_size(value, arg);
      } else if (arg == "--decision-min-leaf") {
        decision_options.min_leaf = parse_positive_size(value, arg);
      } else if (arg == "--decision-max-categories") {
        decision_options.max_categories = parse_positive_size(value, arg);
      } else if (arg == "--decision-max-thresholds") {
        decision_options.max_thresholds = parse_positive_size(value, arg);
      } else {
        decision_options.min_gain = parse_finite_double(value, arg);
        if (decision_options.min_gain < 0.0)
          throw std::invalid_argument(
              "--decision-min-gain expects a non-negative number");
      }
    } else if (arg == "--decisions" || arg == "--decision-tables") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      if (arg == "--decisions") {
        decisions_miner = value;
      } else {
        decision_tables_miner = value;
      }
    } else if (arg == "--no-alignments") {
      no_alignments = true;
    } else if (arg == "--align-process-tree") {
      align_process_tree = true;
    } else if (arg == "--pt-alignment-variant" ||
               arg == "--pt-alignment-max-states" ||
               arg == "--pt-alignment-max-time-ms" ||
               arg == "--pt-alignment-max-total-time-ms" ||
               arg == "--pt-alignment-use-global-cache" ||
               arg == "--pt-alignment-batch-size" ||
               arg == "--pt-alignment-clear-cache" ||
               arg == "--pt-alignment-max-trace-length" ||
               arg == "--pt-alignment-max-process-tree-height" ||
               arg == "--pt-alignment-max-trace-time-ms" ||
               arg == "--pt-alignment-max-per-trace-time-ms" ||
               arg == "--pt-alignment-activity-attribute") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      std::string key(arg.substr(2));
      std::replace(key.begin(), key.end(), '-', '_');
      if (arg == "--pt-alignment-activity-attribute") {
        const auto selector = trim_text(value);
        if (selector.empty()) {
          std::cerr << "error: " << arg
                    << " expects a non-empty attribute name\n";
          return 2;
        }
        pt_alignment_fields[key] = selector;
      } else {
        pt_alignment_fields[key] = value;
      }
      align_process_tree = true;
    } else if (arg == "--align-dfg" || arg == "--dfg-max-states" || arg == "--dfg-max-time-ms" || arg == "--dfg-projection" || arg == "--dfg-sync-cost" || arg == "--dfg-model-move-cost" || arg == "--dfg-log-move-cost" || arg == "--dfg-internal-log-move-cost" || arg == "--align-edit-distance" || arg == "--edit-anti" || arg == "--edit-deviation-cost" || arg == "--edit-projection") {
      if (arg == "--align-dfg") { align_dfg = true; }
      else if (arg == "--edit-anti") { edit_anti = true; edit_options.perform_anti_alignment = true; edit_option_used = true; }
      else {
        const auto* value = next(); if (!value) return usage(argv[0]);
        try {
          if (arg == "--dfg-max-states") { dfg_alignment_options.max_visited_states = parse_nonnegative_size(value, arg); align_dfg = true; }
          else if (arg == "--dfg-max-time-ms") { dfg_alignment_options.max_time_ms = static_cast<std::uint64_t>(parse_nonnegative_size(value, arg)); align_dfg = true; }
          else if (arg == "--dfg-projection") { dfg_projection = value; if (dfg_projection != "none" && dfg_projection != "log" && dfg_projection != "metrics") throw std::invalid_argument("--dfg-projection expects none, log, or metrics"); align_dfg = true; }
          else if (arg == "--dfg-sync-cost" || arg == "--dfg-model-move-cost" || arg == "--dfg-log-move-cost" || arg == "--dfg-internal-log-move-cost") {
            auto* costs = &dfg_alignment_options.sync_cost;
            auto* supplied = &dfg_alignment_options.sync_cost_supplied;
            if (arg == "--dfg-model-move-cost") {
              costs = &dfg_alignment_options.model_move_cost;
              supplied = &dfg_alignment_options.model_move_cost_supplied;
            } else if (arg == "--dfg-log-move-cost") {
              costs = &dfg_alignment_options.log_move_cost;
              supplied = &dfg_alignment_options.log_move_cost_supplied;
            } else if (arg == "--dfg-internal-log-move-cost") {
              costs = &dfg_alignment_options.internal_log_move_cost;
              supplied = &dfg_alignment_options.internal_log_move_cost_supplied;
            }
            if (std::string_view(value) == "{}") costs->clear();
            else {
              const auto kv = parse_dfg_cost(value, arg);
              (*costs)[kv.first] = kv.second;
            }
            *supplied = true;
            align_dfg = true;
          }
          else if (arg == "--align-edit-distance") edit_reference = value;
          else if (arg == "--edit-deviation-cost") { edit_options.deviation_cost = static_cast<std::uint64_t>(parse_positive_size(value, arg)); edit_option_used = true; }
          else { edit_projection = value; if (edit_projection != "none" && edit_projection != "log") throw std::invalid_argument("--edit-projection expects none or log"); edit_option_used = true; }
        } catch (const std::invalid_argument& error) { std::cerr << "error: " << error.what() << '\n'; return 2; }
      }
    } else if (arg == "--alignment-variant") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      const std::string_view v = value;
      if (v == "state_equation_a_star" || v == "VERSION_STATE_EQUATION_A_STAR") alignment_options.variant = minera::AlignmentVariant::kStateEquationAStar;
      else if (v == "dijkstra_no_heuristics" || v == "VERSION_DIJKSTRA_NO_HEURISTICS") alignment_options.variant = minera::AlignmentVariant::kDijkstraNoHeuristics;
      else if (v == "dijkstra_less_memory" || v == "VERSION_DIJKSTRA_LESS_MEMORY") alignment_options.variant = minera::AlignmentVariant::kDijkstraLessMemory;
      else if (v == "discounted_a_star" || v == "VERSION_DISCOUNTED_A_STAR") alignment_options.variant = minera::AlignmentVariant::kDiscountedAStar;
      else if (v == "generator_dijkstra_no_heuristics") alignment_options.variant = minera::AlignmentVariant::kGeneratorDijkstraNoHeuristics;
      else if (v == "generator_dijkstra_less_memory") alignment_options.variant = minera::AlignmentVariant::kGeneratorDijkstraLessMemory;
      else { std::cerr << "error: --alignment-variant: unknown variant\n"; return 2; }
      alignment_option_used = true;
    } else if (arg == "--alignment-max-states" || arg == "--alignment-max-alignments") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      std::size_t parsed = 0;
      try { parsed = parse_positive_size(value, arg); }
      catch (const std::invalid_argument& error) { std::cerr << "error: " << error.what() << '\n'; return 2; }
      if (arg == "--alignment-max-states") alignment_options.max_states = parsed;
      else { alignment_options.max_alignments = parsed; alignment_max_alignments_used = true; }
      alignment_option_used = true;
    } else if (arg == "--alignment-max-align-time" || arg == "--alignment-max-align-time-trace" ||
               arg == "--alignment-exponent" || arg == "--alignment-trace-cost" ||
               arg == "--alignment-model-cost" || arg == "--alignment-silent-model-cost" ||
               arg == "--alignment-sync-cost") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      char* end = nullptr;
      const double parsed = std::strtod(value, &end);
      if (end == value || *end != '\0' || !std::isfinite(parsed) || parsed < 0.0)
      { std::cerr << "error: " << arg << " expects a finite non-negative number\n"; return 2; }
      if (arg == "--alignment-max-align-time") alignment_options.max_total_time_seconds = parsed;
      else if (arg == "--alignment-max-align-time-trace") alignment_options.max_time_seconds = parsed;
      else if (arg == "--alignment-exponent") { if (parsed <= 1.0) { std::cerr << "error: --alignment-exponent expects > 1\n"; return 2; } alignment_options.exponent = parsed; alignment_exponent_used = true; }
      else if (arg == "--alignment-trace-cost") { alignment_options.default_trace_cost = parsed; alignment_trace_cost_used = true; }
      else if (arg == "--alignment-model-cost") { alignment_options.default_model_cost = parsed; alignment_model_cost_used = true; }
      else if (arg == "--alignment-silent-model-cost") { alignment_options.default_silent_cost = parsed; alignment_silent_cost_used = true; }
      else { alignment_options.default_sync_cost = parsed; alignment_sync_cost_used = true; }
      alignment_option_used = true;
    } else if (arg == "--alignment-synchronous-dijkstra") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      try { alignment_options.synchronous_dijkstra = parse_boolean(value, arg); }
      catch (const std::invalid_argument& error) { std::cerr << "error: " << error.what() << '\n'; return 2; }
      alignment_option_used = true;
      alignment_synchronous_dijkstra_used = true;
    } else if (arg == "--alignment-cost-profile") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      if (std::string_view(value) == "legacy") alignment_options.cost_profile = minera::AlignmentCostProfile::kLegacyUnit;
      else if (std::string_view(value) == "standard") alignment_options.cost_profile = minera::AlignmentCostProfile::kStandard;
      else { std::cerr << "error: --alignment-cost-profile expects legacy or standard\n"; return 2; }
      alignment_option_used = true;
    } else if (arg == "--token-replay-variant") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      const std::string_view variant = value;
      if (variant == "forward")
        token_replay_options.variant = minera::TokenReplayVariant::kForward;
      else if (variant == "backwards")
        token_replay_options.variant = minera::TokenReplayVariant::kBackwards;
      else {
        std::cerr << "error: --token-replay-variant expects forward or "
                     "backwards\n";
        return 2;
      }
    } else if (arg == "--token-replay-stop-immediately-unfit") {
      token_replay_options.stop_immediately_unfit = true;
    } else if (arg == "--log-skeleton-noise-threshold") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      char* end = nullptr;
      log_skeleton_options.noise_threshold = std::strtod(value, &end);
      if (end == value || *end != '\0' ||
          !std::isfinite(log_skeleton_options.noise_threshold) ||
          log_skeleton_options.noise_threshold < 0.0 ||
          log_skeleton_options.noise_threshold > 1.0) {
        std::cerr << "error: --log-skeleton-noise-threshold expects 0 <= f <= 1\n";
        return 2;
      }
    } else if (arg == "--log-skeleton-constraints") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      const std::string raw = value;
      std::vector<std::string> constraints;
      std::size_t start = 0;
      while (start <= raw.size()) {
        const auto comma = raw.find(',', start);
        const auto token = raw.substr(start, comma == std::string::npos
                                               ? std::string::npos
                                               : comma - start);
        const auto first = token.find_first_not_of(" \t\r\n");
        if (first != std::string::npos) {
          const auto last = token.find_last_not_of(" \t\r\n");
          constraints.push_back(token.substr(first, last - first + 1));
        }
        if (comma == std::string::npos) break;
        start = comma + 1;
      }
      static constexpr std::string_view allowed[] = {
          "equivalence", "always_after", "always_before", "never_together",
          "directly_follows", "activ_freq"};
      for (const auto& constraint : constraints) {
        if (std::find(std::begin(allowed), std::end(allowed), constraint) ==
            std::end(allowed)) {
          std::cerr << "error: --log-skeleton-constraints unknown constraint '"
                    << constraint << "'\n";
          return 2;
        }
      }
      std::sort(constraints.begin(), constraints.end());
      constraints.erase(std::unique(constraints.begin(), constraints.end()),
                        constraints.end());
      log_skeleton_options.considered_constraints = std::move(constraints);
      log_skeleton_options.considered_constraints_present = true;
    } else if (arg == "--declare-considered-activities") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      auto activities = split_csv(value);
      declare_options.considered_activities = std::move(activities);
    } else if (arg == "--declare-allowed-templates") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      auto templates = split_csv(value);
      static constexpr std::string_view allowed[] = {
          minera::kDeclareExistence, minera::kDeclareExactlyOne,
          minera::kDeclareInit, minera::kDeclareRespondedExistence,
          minera::kDeclareResponse, minera::kDeclarePrecedence,
          minera::kDeclareSuccession, minera::kDeclareAltResponse,
          minera::kDeclareAltPrecedence, minera::kDeclareAltSuccession,
          minera::kDeclareChainResponse, minera::kDeclareChainPrecedence,
          minera::kDeclareChainSuccession, minera::kDeclareAbsence,
          minera::kDeclareCoexistence, minera::kDeclareNonCoexistence,
          minera::kDeclareNonSuccession, minera::kDeclareNonChainSuccession};
      for (const auto& name : templates) {
        if (std::find(std::begin(allowed), std::end(allowed), name) ==
            std::end(allowed)) {
          std::cerr << "error: --declare-allowed-templates unknown template '"
                    << name << "'\n";
          return 2;
        }
      }
      std::sort(templates.begin(), templates.end());
      templates.erase(std::unique(templates.begin(), templates.end()),
                      templates.end());
      declare_options.allowed_templates = std::move(templates);
    } else if (arg == "--declare-min-support-ratio" ||
               arg == "--declare-min-confidence-ratio" ||
               arg == "--declare-auto-selection-multiplier") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      char* end = nullptr;
      const double parsed = std::strtod(value, &end);
      if (end == value || *end != '\0' || !std::isfinite(parsed) ||
          parsed < 0.0 || parsed > 1.0) {
        std::cerr << "error: " << arg << " expects a finite value in [0,1]\n";
        return 2;
      }
      if (arg == "--declare-min-support-ratio") {
        declare_options.min_support_ratio = parsed;
        declare_options.has_min_support_ratio = true;
      } else if (arg == "--declare-min-confidence-ratio") {
        declare_options.min_confidence_ratio = parsed;
        declare_options.has_min_confidence_ratio = true;
      } else {
        declare_options.auto_selection_multiplier = parsed;
      }
    } else if (arg == "--footprints-discovery-variants" ||
               arg == "--footprints-conformance-variants") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      const auto parts = split_csv(value);
      if (parts.empty()) {
        std::cerr << "error: " << arg << " expects a non-empty list\n";
        return 2;
      }
      const std::set<std::string> allowed =
          arg == "--footprints-discovery-variants"
              ? std::set<std::string>{"entire_event_log", "entire_dataframe",
                                      "polars_lazyframes", "trace_by_trace",
                                      "dfg", "process_tree", "powl",
                                      "petri_reach_graph"}
              : std::set<std::string>{"log_model", "log_extensive",
                                      "trace_extensive"};
      std::set<std::string> parsed;
      for (const auto& part : parts) {
        if (!allowed.contains(part)) {
          std::cerr << "error: " << arg << " unknown variant '" << part
                    << "'\n";
          return 2;
        }
        parsed.insert(part);
      }
      if (arg == "--footprints-discovery-variants") {
        footprints_options.discovery_variants = std::move(parsed);
      } else {
        footprints_options.conformance_variants = std::move(parsed);
      }
    } else if (arg == "--footprints-model") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      static const std::set<std::string> allowed = {
          "process_tree", "powl", "inductive", "alpha", "heuristic", "dfg"};
      if (!allowed.contains(value)) {
        std::cerr << "error: --footprints-model unknown model '" << value
                  << "'\n";
        return 2;
      }
      footprints_options.model = value;
    } else if (arg == "--footprints-strict" ||
               arg == "--footprints-enable-act-always-executed") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      const std::string_view boolean = value;
      bool parsed = false;
      if (boolean == "true" || boolean == "1") {
        parsed = true;
      } else if (boolean != "false" && boolean != "0") {
        std::cerr << "error: " << arg
                  << " expects true, false, 1, or 0\n";
        return 2;
      }
      if (arg == "--footprints-strict") {
        footprints_options.strict = parsed;
      } else {
        footprints_options.enable_act_always_executed = parsed;
      }
    } else if (arg == "--footprints-activity-attribute" ||
               arg == "--footprints-case-id-attribute" ||
               arg == "--footprints-start-timestamp-attribute" ||
               arg == "--footprints-timestamp-attribute" ||
               arg == "--footprints-index-attribute" ||
               arg == "--footprints-sort-required" ||
               arg == "--footprints-initial-marking") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      std::string error;
      (void)parse_footprints_extra_option(arg, value, footprints_options,
                                          error);
      if (!error.empty()) {
        std::cerr << "error: " << error << '\n';
        return 2;
      }
    } else if (arg == "--dfg-statistical-variants" ||
               arg == "--dfg-causal-variants") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      try {
        if (arg == "--dfg-statistical-variants") {
          dfg_statistical_variants = canonical_dfg_variants(
              value,
              {"native", "frequency", "frequency_greedy",
               "frequency_triples", "performance", "performance_greedy",
               "case_attributes", "clean", "clean_polars", "clean_time"},
              arg);
        } else {
          dfg_causal_variants = canonical_dfg_variants(
              value, {"alpha", "heuristic"}, arg);
        }
      } catch (const std::invalid_argument& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 2;
      }
    } else if (arg == "--dfg-window") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      try {
        dfg_statistics_options.window =
            parse_nonnegative_size(value, "--dfg-window");
        dfg_dataframe_fields["dfg_window"] = value;
      } catch (const std::invalid_argument& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 2;
      }
    } else if (arg == "--dfg-keep-once-per-case") {
      dfg_statistics_options.keep_once_per_case = true;
      dfg_dataframe_fields["dfg_keep_once_per_case"] = "true";
    } else if (arg == "--dfg-performance-aggregation") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      const std::string_view aggregation = value;
      if (aggregation == "mean")
        dfg_statistics_options.performance_aggregation =
            minera::DfgPerformanceAggregation::kMean;
      else if (aggregation == "median")
        dfg_statistics_options.performance_aggregation =
            minera::DfgPerformanceAggregation::kMedian;
      else if (aggregation == "min")
        dfg_statistics_options.performance_aggregation =
            minera::DfgPerformanceAggregation::kMinimum;
      else if (aggregation == "max")
        dfg_statistics_options.performance_aggregation =
            minera::DfgPerformanceAggregation::kMaximum;
      else if (aggregation == "stdev")
        dfg_statistics_options.performance_aggregation =
            minera::DfgPerformanceAggregation::kStandardDeviation;
      else if (aggregation == "sum")
        dfg_statistics_options.performance_aggregation =
            minera::DfgPerformanceAggregation::kSum;
      else if (aggregation == "raw_values")
        dfg_statistics_options.performance_aggregation =
            minera::DfgPerformanceAggregation::kRawValues;
      else if (aggregation == "all")
        dfg_statistics_options.performance_aggregation =
            minera::DfgPerformanceAggregation::kAll;
      else {
        std::cerr << "error: --dfg-performance-aggregation expects mean, "
                     "median, min, max, stdev, sum, raw_values, or all\n";
        return 2;
      }
      dfg_dataframe_fields["dfg_performance_aggregation"] = value;
    } else if (arg == "--dfg-start-timestamp-attribute") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      dfg_statistics_options.start_timestamp_attribute = value;
      dfg_dataframe_fields["dfg_start_timestamp_attribute"] = value;
    } else if (arg == "--dfg-business-hours") {
      dfg_statistics_options.use_business_hours = true;
      dfg_dataframe_fields["dfg_business_hours"] = "true";
    } else if (arg == "--dfg-business-hour-slots") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      try {
        if (std::string_view(value) == "none") {
          dfg_statistics_options.business_hour_slots.clear();
          dfg_statistics_options.business_hour_slots_explicit = true;
        } else {
          dfg_statistics_options.business_hour_slots =
              parse_dfg_business_hour_slots(value);
          dfg_statistics_options.business_hour_slots_explicit = true;
        }
        dfg_dataframe_fields["dfg_business_hour_slots"] = value;
      } catch (const std::invalid_argument& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 2;
      }
    } else if (arg == "--dfg-case-attributes") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      auto attributes = split_csv(value);
      std::sort(attributes.begin(), attributes.end());
      attributes.erase(std::unique(attributes.begin(), attributes.end()),
                       attributes.end());
      dfg_statistics_options.case_attributes = std::move(attributes);
    } else if (arg == "--dfg-return-node-attributes") {
      dfg_statistics_options.return_node_attributes = true;
    } else if (arg == "--correlation-variants") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      try {
        correlation_variants = canonical_dfg_variants(
            value, {"classic", "classic_split", "trace_based"}, arg);
      } catch (const std::invalid_argument& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 2;
      }
    } else if (arg == "--correlation-activity-attribute" ||
               arg == "--correlation-timestamp-attribute" ||
               arg == "--correlation-start-timestamp-attribute" ||
               arg == "--correlation-case-id-attribute") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      const auto selector = trim_text(value);
      if (selector.empty() &&
          arg != "--correlation-start-timestamp-attribute") {
        std::cerr << "error: " << arg << " expects a non-empty name\n";
        return 2;
      }
      auto* destination = &correlation_options.case_id_attribute;
      if (arg == "--correlation-activity-attribute")
        destination = &correlation_options.activity_attribute;
      if (arg == "--correlation-timestamp-attribute")
        destination = &correlation_options.timestamp_attribute;
      if (arg == "--correlation-start-timestamp-attribute")
        destination = &correlation_options.start_timestamp_attribute;
      *destination = selector;
    } else if (arg == "--correlation-exact-time-matching") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      const std::string_view boolean = value;
      if (boolean == "true" || boolean == "1") {
        correlation_options.exact_time_matching = true;
      } else if (boolean == "false" || boolean == "0") {
        correlation_options.exact_time_matching = false;
      } else {
        std::cerr << "error: --correlation-exact-time-matching expects true, "
                     "false, 1, or 0\n";
        return 2;
      }
    } else if (arg == "--correlation-sample-size") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      try {
        correlation_options.sample_size = parse_positive_size(value, arg);
      } catch (const std::invalid_argument& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 2;
      }
    } else if (arg == "--batches") {
      batches = true;
    } else if (arg == "--batch-merge-distance-seconds") {
      batch_option_used = true;
      const auto* value = next();
      if (!value) return usage(argv[0]);
      char* end = nullptr;
      const double parsed = std::strtod(value, &end);
      if (end != value + std::strlen(value) || !std::isfinite(parsed) || parsed < 0.0) {
        std::cerr << "error: --batch-merge-distance-seconds expects a finite non-negative number\n";
        return 2;
      }
      batch_options.merge_distance_seconds = parsed;
    } else if (arg == "--batch-min-size") {
      batch_option_used = true;
      const auto* value = next();
      if (!value) return usage(argv[0]);
      try {
        batch_options.min_batch_size = parse_positive_size(value, arg);
        if (batch_options.min_batch_size < 2) throw std::invalid_argument("--batch-min-size expects an integer >= 2");
      } catch (const std::invalid_argument& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 2;
      }
    } else if (arg == "--batch-start-timestamp-attribute") {
      batch_option_used = true;
      const auto* value = next();
      if (!value) return usage(argv[0]);
      batch_options.start_timestamp_attribute = value;
    } else if (arg == "--transition-view") {
      const auto* value = next(); if (!value) return usage(argv[0]);
      if (std::string_view(value) == "sequence") transition_options.view = minera::TransitionView::kSequence;
      else if (std::string_view(value) == "set") transition_options.view = minera::TransitionView::kSet;
      else if (std::string_view(value) == "multiset") transition_options.view = minera::TransitionView::kMultiset;
      else { std::cerr << "error: --transition-view expects sequence, set, or multiset\n"; return 2; }
    } else if (arg == "--transition-direction") {
      const auto* value = next(); if (!value) return usage(argv[0]);
      if (std::string_view(value) == "forward") transition_options.direction = minera::TransitionDirection::kForward;
      else if (std::string_view(value) == "backward") transition_options.direction = minera::TransitionDirection::kBackward;
      else { std::cerr << "error: --transition-direction expects forward or backward\n"; return 2; }
    } else if (arg == "--transition-window") {
      const auto* value = next(); if (!value) return usage(argv[0]);
      try {
        transition_options.window = parse_transition_window(value);
      }
      catch (const std::exception& error) { std::cerr << "error: " << error.what() << "\n"; return 2; }
    } else if (arg == "--transition-include-data") {
      transition_options.include_data = true;
    } else if (arg == "--noise-threshold") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      try {
        const double parsed = parse_finite_double(value, arg);
        if (parsed < 0.0 || parsed > 1.0) {
          throw std::invalid_argument(
              "--noise-threshold expects a number in [0, 1]");
        }
        inductive_options.noise_threshold = parsed;
      } catch (const std::invalid_argument& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 2;
      }
    } else if (arg == "--inductive-variant") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      const std::string_view variant = value;
      if (variant != "log" && variant != "imf" && variant != "dfg")
        return usage(argv[0]);
      inductive_options.variant = variant == "dfg"
                                      ? minera::InductiveMinerVariant::kDfg
                                      : variant == "imf"
                                            ? minera::InductiveMinerVariant::kImf
                                            : minera::InductiveMinerVariant::kLog;
    } else if (arg == "--inductive-disable-strict-sequence-cut" ||
               arg == "--inductive-disable-fallthroughs" ||
               arg == "--inductive-multiprocessing" ||
               arg == "--inductive-sort-events-by-timestamp") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      try {
        const bool parsed = parse_boolean(value, arg);
        if (arg == "--inductive-disable-strict-sequence-cut") {
          inductive_options.disable_strict_sequence_cut = parsed;
        } else if (arg == "--inductive-disable-fallthroughs") {
          inductive_options.disable_fallthroughs = parsed;
        } else if (arg == "--inductive-multiprocessing") {
          inductive_options.multiprocessing = parsed;
        } else {
          inductive_options.sort_events_by_timestamp = parsed;
        }
      } catch (const std::invalid_argument& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 2;
      }
    } else if (arg == "--inductive-activity-key" ||
               arg == "--inductive-timestamp-key" ||
               arg == "--inductive-case-id-key") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      const auto selector = trim_text(value);
      if (selector.empty()) {
        std::cerr << "error: " << arg << " expects a non-empty name\n";
        return 2;
      }
      if (arg == "--inductive-activity-key") {
        inductive_options.activity_key = selector;
      } else if (arg == "--inductive-timestamp-key") {
        inductive_options.timestamp_key = selector;
      } else {
        inductive_options.case_id_key = selector;
      }
    } else if (arg == "--alpha-variant" || arg == "--alpha-plus-result") {
      if (arg == "--alpha-plus-result") alpha_plus_result = true;
      else {
        const auto* value = next();
        if (!value) return usage(argv[0]);
        const std::string_view variant = value;
        if (variant != "classic" && variant != "plus") return usage(argv[0]);
        alpha_options.short_loops = variant == "plus";
      }
    } else if (arg == "--alpha-activity-attribute") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      const auto selector = trim_text(value);
      if (selector.empty()) {
        std::cerr << "error: --alpha-activity-attribute expects a non-empty name\n";
        return 2;
      }
      alpha_options.activity_attribute = selector;
    } else if (arg == "--alpha-max-rows-to-materialize" ||
               arg == "--alpha-remove-unconnected") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      try {
        if (arg == "--alpha-max-rows-to-materialize") {
          alpha_options.max_rows_to_materialize = parse_nonnegative_size(value, arg);
        } else {
          alpha_options.remove_unconnected = parse_boolean(value, arg);
        }
      } catch (const std::invalid_argument& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 2;
      }
    } else if (arg == "--heuristic-variant") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      const std::string_view variant = value;
      if (variant != "classic" && variant != "plusplus")
        return usage(argv[0]);
      heuristic_options.variant =
          variant == "plusplus" ? minera::HeuristicMinerVariant::kPlusPlus
                                : minera::HeuristicMinerVariant::kClassic;
    } else if (arg == "--dependency-threshold") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      heuristic_options.dependency_threshold = std::atof(value);
    } else if (arg == "--and-threshold") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      heuristic_options.and_threshold = std::atof(value);
    } else if (arg == "--min-edge-frequency") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      heuristic_options.min_frequency =
          static_cast<std::uint64_t>(std::atoll(value));
    } else if (arg == "--heuristic-min-activity-count") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      heuristic_options.min_activity_count =
          static_cast<std::uint64_t>(std::atoll(value));
    } else if (arg == "--heuristic-decoration") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      const std::string_view decoration = value;
      if (decoration != "frequency" && decoration != "performance")
        return usage(argv[0]);
      heuristic_options.decoration =
          decoration == "performance"
              ? minera::HeuristicNetDecoration::kPerformance
              : minera::HeuristicNetDecoration::kFrequency;
    } else if (arg == "--heuristic-performance-aggregation") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      const std::string_view aggregation = value;
      if (aggregation == "mean")
        heuristic_options.performance_aggregation =
            minera::HeuristicPerformanceAggregation::kMean;
      else if (aggregation == "median")
        heuristic_options.performance_aggregation =
            minera::HeuristicPerformanceAggregation::kMedian;
      else if (aggregation == "min")
        heuristic_options.performance_aggregation =
            minera::HeuristicPerformanceAggregation::kMinimum;
      else if (aggregation == "max")
        heuristic_options.performance_aggregation =
            minera::HeuristicPerformanceAggregation::kMaximum;
      else if (aggregation == "stdev")
        heuristic_options.performance_aggregation =
            minera::HeuristicPerformanceAggregation::kStandardDeviation;
      else if (aggregation == "sum")
        heuristic_options.performance_aggregation =
            minera::HeuristicPerformanceAggregation::kSum;
      else if (aggregation == "raw_values")
        heuristic_options.performance_aggregation =
            minera::HeuristicPerformanceAggregation::kRawValues;
      else if (aggregation == "all")
        heuristic_options.performance_aggregation =
            minera::HeuristicPerformanceAggregation::kAll;
      else
        return usage(argv[0]);
    } else if (arg ==
               "--heuristic-dfg-pre-cleaning-noise-threshold") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      heuristic_options.dfg_pre_cleaning_noise_threshold = std::atof(value);
    } else if (arg == "--heuristic-loop-length-two-threshold") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      heuristic_options.loop_length_two_threshold = std::atof(value);
    } else if (arg == "--heuristic-keep-once-per-case") {
      heuristic_options.keep_once_per_case = true;
    } else if (arg == "--heuristic-most-common-paths") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      for (const auto& pair : parse_pairs(value)) {
        heuristic_options.most_common_paths.emplace(pair.predecessor,
                                                    pair.successor);
      }
    } else if (arg == "--heuristic-start-timestamp-attribute") {
      const auto* value = next();
      if (!value || value[0] == '\0') return usage(argv[0]);
      heuristic_options.start_timestamp_attribute = value;
    } else if (arg == "--heuristic-lifecycle-instance-attribute") {
      const auto* value = next();
      if (!value || value[0] == '\0') return usage(argv[0]);
      heuristic_options.lifecycle_instance_attribute = value;
    } else if (arg == "--delimiter") {
      const auto* value = next();
      if (!value || value[0] == '\0') return usage(argv[0]);
      options.delimiter = value[0];
    } else if (arg == "--time-from") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.time_from = value;
    } else if (arg == "--time-to") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.time_to = value;
    } else if (arg == "--time-mode") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      const auto mode = minera::parse_time_filter_mode(value);
      if (!mode) {
        std::cerr << "error: unknown --time-mode '" << value
                  << "' (expected events, traces_contained, or "
                     "traces_intersecting)\n";
        return 2;
      }
      filter.time_mode = *mode;
    } else if (arg == "--keep-activities") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.keep_activities = split_csv(value);
    } else if (arg == "--remove-activities") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.remove_activities = split_csv(value);
    } else if (arg == "--keep-resources") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.keep_resources = split_csv(value);
    } else if (arg == "--remove-resources") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.remove_resources = split_csv(value);
    } else if (arg == "--keep-lifecycles") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.keep_lifecycles = split_csv(value);
    } else if (arg == "--remove-lifecycles") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.remove_lifecycles = split_csv(value);
    } else if (arg == "--attribute-key") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.attribute_key = value;
    } else if (arg == "--keep-attribute-values") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.keep_attribute_values = split_csv(value);
    } else if (arg == "--remove-attribute-values") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.remove_attribute_values = split_csv(value);
    } else if (arg == "--attribute-mode") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      const auto mode = minera::parse_attribute_filter_mode(value);
      if (!mode) {
        std::cerr << "error: unknown --attribute-mode '" << value
                  << "' (expected events or traces)\n";
        return 2;
      }
      filter.attribute_mode = *mode;
    } else if (arg == "--case-attribute-key") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.case_attribute_key = value;
    } else if (arg == "--keep-case-attribute-values") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.keep_case_attribute_values = split_csv(value);
    } else if (arg == "--remove-case-attribute-values") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.remove_case_attribute_values = split_csv(value);
    } else if (arg == "--rework-activity") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.rework_activity = value;
    } else if (arg == "--rework-min") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.rework_min_occurrences =
          static_cast<std::uint64_t>(std::atoll(value));
    } else if (arg == "--keep-directly-follows") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.keep_directly_follows = parse_pairs(value);
    } else if (arg == "--remove-directly-follows") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.remove_directly_follows = parse_pairs(value);
    } else if (arg == "--keep-eventually-follows") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.keep_eventually_follows = parse_pairs(value);
    } else if (arg == "--remove-eventually-follows") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.remove_eventually_follows = parse_pairs(value);
    } else if (arg == "--prefix-activity") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.prefix_activity = value;
    } else if (arg == "--suffix-activity") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.suffix_activity = value;
    } else if (arg == "--between-start") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.between_start = value;
    } else if (arg == "--between-end") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.between_end = value;
    } else if (arg == "--start-activities") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.start_activities = split_csv(value);
    } else if (arg == "--end-activities") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.end_activities = split_csv(value);
    } else if (arg == "--min-length") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.min_length = static_cast<std::size_t>(std::atoll(value));
    } else if (arg == "--max-length") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.max_length = static_cast<std::size_t>(std::atoll(value));
    } else if (arg == "--min-variant-frequency") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.min_variant_frequency =
          static_cast<std::uint64_t>(std::atoll(value));
    } else if (arg == "--top-variants") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.top_variants = static_cast<std::size_t>(std::atoll(value));
    } else if (arg == "--variant-coverage") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      filter.variant_coverage = parse_finite_double(value, arg);
      if (filter.variant_coverage < 0.0 || filter.variant_coverage > 1.0)
        throw std::invalid_argument(
            "--variant-coverage expects a number in [0, 1]");
    } else if (!arg.empty() && arg.front() == '-') {
      return usage(argv[0]);
    } else if (input.empty()) {
      input = arg;
    } else {
      return usage(argv[0]);
    }
  }

  if (spectrum_option_used && !spectrum_requested) {
    std::cerr << "error: --spectrum-variant/--spectrum-sample-size/"
                 "--spectrum-sort-log/--spectrum-*-key require --spectrum\n";
    return 2;
  }
  if (dfg_dataframe_option_used && dfg_dataframe_output.empty()) {
    std::cerr << "error: --dfg-operation/--dfg-measure/--dfg-backend and "
                 "other dataframe DFG options require --dfg-dataframe-output\n";
    return 2;
  }
  if (decision_option_used && decisions_miner.empty() &&
      decision_tables_miner.empty()) {
    std::cerr << "error: --decision-* options require --decisions or "
                 "--decision-tables\n";
    return 2;
  }
  if (decision_lookback_used && decision_tables_miner.empty()) {
    std::cerr << "error: --decision-lookback requires --decision-tables\n";
    return 2;
  }
  if (activity_split_option_used && filter.activity_split_activities.empty()) {
    std::cerr << "error: --activity-split requires at least one activity\n";
    return 2;
  }
  if (repetition_option_used) {
    const auto lookup = [&](std::string_view name) {
      const auto it = repetition_fields.find(std::string(name));
      return it == repetition_fields.end() ? std::string{} : it->second;
    };
    const auto present = [&](std::string_view name) {
      return repetition_fields.contains(std::string(name));
    };
    try {
      const auto parsed = minera::parse_mine_params(lookup, present);
      if (!parsed.filter.attribute_value_repetition) {
        std::cerr << "error: repetition filtering requires a value\n";
        return 2;
      }
      filter.attribute_value_repetition =
          parsed.filter.attribute_value_repetition;
    } catch (const std::exception& error) {
      std::cerr << "error: " << error.what() << '\n';
      return 2;
    }
  }
  if (case_grouping_option_used) {
    const auto lookup = [&](std::string_view name) {
      const auto it = case_grouping_fields.find(std::string(name));
      return it == case_grouping_fields.end() ? std::string{} : it->second;
    };
    const auto present = [&](std::string_view name) {
      return case_grouping_fields.contains(std::string(name));
    };
    try {
      const auto parsed = minera::parse_mine_params(lookup, present);
      if (!parsed.filter.consecutive_activity_grouping &&
          !parsed.filter.timestamp_case_grouping) {
        std::cerr << "error: case grouping requires a grouping type\n";
        return 2;
      }
      filter.consecutive_activity_grouping =
          parsed.filter.consecutive_activity_grouping;
      filter.timestamp_case_grouping = parsed.filter.timestamp_case_grouping;
    } catch (const std::exception& error) {
      std::cerr << "error: " << error.what() << '\n';
      return 2;
    }
  }
  if (petri_option_used && !align_petri_anti && !align_petri_multi) {
    std::cerr << "error: --petri-* options require --align-petri-anti or --align-petri-multi\n";
    return 2;
  }
  if (advanced_petri_miner_used && !align_decomposed && !align_petri_anti &&
      !align_petri_multi) {
    std::cerr << "error: --advanced-petri-miner requires an advanced "
                 "alignment request\n";
    return 2;
  }
  if (!reduction_activity_key.empty() && !reduce_trace_index) {
    std::cerr << "error: --process-tree-reduction-activity-key requires --reduce-process-tree\n";
    return 2;
  }
  if (batch_option_used && !batches) {
    std::cerr << "error: --batch-* options require --batches\n";
    return 2;
  }
  if (!batch_options.event_id_attribute.empty() &&
      batch_options.variant == minera::BatchDiscoveryVariant::kLog) {
    std::cerr << "error: --batch-event-id-attribute requires the pandas or polars batch variant\n";
    return 2;
  }
  if (alignment_option_used) {
    if (no_alignments) {
      std::cerr << "error: alignment options cannot be combined with --no-alignments\n";
      return 2;
    }
    const bool generator =
        alignment_options.variant == minera::AlignmentVariant::kGeneratorDijkstraNoHeuristics ||
        alignment_options.variant == minera::AlignmentVariant::kGeneratorDijkstraLessMemory;
    if (alignment_max_alignments_used && !generator) {
      std::cerr << "error: --alignment-max-alignments requires a generator alignment variant\n";
      return 2;
    }
    if (alignment_exponent_used &&
        alignment_options.variant != minera::AlignmentVariant::kDiscountedAStar) {
      std::cerr << "error: --alignment-exponent requires --alignment-variant discounted_a_star\n";
      return 2;
    }
    if (alignment_synchronous_dijkstra_used &&
        alignment_options.variant != minera::AlignmentVariant::kDiscountedAStar) {
      std::cerr << "error: --alignment-synchronous-dijkstra requires --alignment-variant discounted_a_star\n";
      return 2;
    }
  }
  if (edit_option_used && edit_reference.empty()) {
    std::cerr << "error: edit-distance options require --align-edit-distance <file>\n";
    return 2;
  }
  if (emd_option_used && emd_reference.empty()) {
    std::cerr << "error: EMD options require --emd-reference <file>\n";
    return 2;
  }
  if (token_replay_result_requested && token_replay_model.empty()) {
    std::cerr << "error: --token-replay-result requires --token-replay-model\n";
    return 2;
  }
  if (token_replay_result_requested && replay_fitness_result_requested) {
    std::cerr << "error: choose a token replay result or replay fitness result\n";
    return 2;
  }
  if (!token_replay_model.empty() &&
      (!compare_petri_reference.empty() || !compare_petri_model.empty() ||
       compare_petri_miner_used || compare_petri_replay_option_used ||
       compare_petri_initial_marking_used || compare_petri_final_marking_used ||
       !compare_petri_activity_attribute.empty() || !check_pnml.empty() ||
       xes_import_result_requested || log_conversion_requested || alpha_plus_result ||
       browser_history_requested || windows_events_requested || repository_history_requested ||
       workflow_history_requested || enterprise_history_requested || communication_history_requested ||
       random_tree_requested || merge_requested || timestamp_interleavings_requested ||
       !check_bpmn.empty() || !check_ptml.empty() || !pnml_to_ptml_input.empty() ||
       !bpmn_to_ptml_input.empty() || !petri_reduction_input.empty() || !pnml_inspection_input.empty() ||
       !dfg_dataframe_output.empty())) {
    std::cerr << "error: --token-replay-model cannot be combined with another model or standalone output mode\n";
    return 2;
  }
  if (compare_petri_reference.empty() &&
      (!compare_petri_model.empty() || compare_petri_miner != "inductive" ||
       !compare_petri_activity_attribute.empty() ||
       compare_petri_initial_marking_used || compare_petri_final_marking_used ||
       compare_petri_replay_option_used)) {
    std::cerr << "error: --compare-petri options require --compare-petri-reference <file>\n";
    return 2;
  }
  if (!compare_petri_model.empty() && compare_petri_miner_used) {
    std::cerr << "error: choose --compare-petri-model or --compare-petri-miner\n";
    return 2;
  }
  if (evaluation_requested) {
    const auto lookup = [&](std::string_view name) {
      const auto it = evaluation_fields.find(std::string(name));
      return it == evaluation_fields.end() ? std::string{} : it->second;
    };
    const auto present = [&](std::string_view name) {
      return evaluation_fields.contains(std::string(name));
    };
    try {
      evaluation_params = minera::parse_evaluation_params(lookup, present);
    } catch (const std::invalid_argument& error) {
      std::cerr << "error: " << error.what() << '\n';
      return 2;
    }
  }
  std::optional<minera::PetriAnalysisParams> petri_analysis_params;
  if (petri_analysis_requested) {
    const auto lookup = [&](std::string_view name) {
      const auto it = petri_analysis_fields.find(std::string(name));
      return it == petri_analysis_fields.end() ? std::string{} : it->second;
    };
    const auto present = [&](std::string_view name) {
      return petri_analysis_fields.contains(std::string(name));
    };
    try {
      petri_analysis_params =
          minera::parse_petri_analysis_params(lookup, present);
    } catch (const std::invalid_argument& error) {
      std::cerr << "error: " << error.what() << '\n';
      return 2;
    }
  }
  if (align_process_tree) {
    const auto lookup = [&](std::string_view name) {
      const auto it = pt_alignment_fields.find(std::string(name));
      return it == pt_alignment_fields.end() ? std::string{} : it->second;
    };
    try {
      pt_alignment_options = minera::parse_process_tree_alignment_params(lookup);
    } catch (const std::invalid_argument& error) {
      std::cerr << "error: " << error.what() << '\n';
      return 2;
    }
  }

  if (footprints_options.model == "dfg" &&
      footprints_options.discovery_variants.contains("petri_reach_graph")) {
    std::cerr << "error: --footprints-model dfg cannot be combined with "
                 "petri_reach_graph discovery\n";
    return 2;
  }
  if (declare_playout_options.min_length >
      declare_playout_options.max_length) {
    std::cerr << "error: --declare-playout-min-length must not exceed "
                 "--declare-playout-max-length\n";
    return 2;
  }
  if (declare_playout_options_used && playout_miner != "declare") {
    std::cerr << "error: --declare-playout-* options require "
                 "--playout-miner declare\n";
    return 2;
  }
  if (process_tree_playout_options_used && playout_miner != "process_tree") {
    std::cerr << "error: --playout-* process-tree options require "
                 "--playout-miner process_tree\n";
    return 2;
  }
  if (process_tree_playout_options_used) {
    const auto lookup = [&](std::string_view name) {
      const auto it = process_tree_playout_fields.find(std::string(name));
      return it == process_tree_playout_fields.end() ? std::string{} : it->second;
    };
    const auto present = [&](std::string_view name) {
      return process_tree_playout_fields.contains(std::string(name));
    };
    try {
      process_tree_playout_options =
          minera::parse_process_tree_playout_params(lookup, present);
    } catch (const std::invalid_argument& error) {
      std::cerr << "error: " << error.what() << '\n';
      return 2;
    }
  }

  if (basic_playout_options_used &&
      (playout_miner == "declare" || playout_miner == "process_tree" ||
       playout_miner == "dfg" || playout_miner == "stochastic" ||
       playout_miner == "monte_carlo")) {
    std::cerr << "error: --petri-playout-* options require a Petri "
                 "--playout-miner\n";
    return 2;
  }
  if (petri_playout_mode == "extensive" &&
      (playout_miner == "declare" || playout_miner == "process_tree" ||
       playout_miner == "dfg" || playout_miner == "stochastic" ||
       playout_miner == "monte_carlo")) {
    std::cerr << "error: extensive Petri playout requires a Petri model\n";
    return 2;
  }
  if (basic_playout_options_used) {
    const auto lookup = [&](std::string_view name) {
      const auto it = basic_playout_fields.find(std::string(name));
      return it == basic_playout_fields.end() ? std::string{} : it->second;
    };
    const auto present = [&](std::string_view name) {
      return basic_playout_fields.contains(std::string(name));
    };
    try {
      if (present("traces"))
        basic_playout_options.traces = parse_positive_size(
            lookup("traces").c_str(), "--petri-playout-traces");
      if (present("max_steps"))
        basic_playout_options.max_steps = parse_nonnegative_size(
            lookup("max_steps").c_str(), "--petri-playout-max-steps");
      if (present("seed"))
        basic_playout_options.seed = static_cast<std::uint64_t>(
            parse_nonnegative_size(lookup("seed").c_str(),
                                   "--petri-playout-seed"));
      if (present("initial_marking"))
        basic_playout_options.initial_marking = parse_uint32_marking(
            lookup("initial_marking").c_str(),
            "--petri-playout-initial-marking");
      if (present("final_marking"))
        basic_playout_options.final_marking = parse_uint32_marking(
            lookup("final_marking").c_str(),
            "--petri-playout-final-marking");
      if (present("start"))
        basic_playout_options.start = parse_finite_double(
            lookup("start").c_str(), "--petri-playout-start");
      if (present("case_interval"))
        basic_playout_options.case_interval = parse_finite_double(
            lookup("case_interval").c_str(),
            "--petri-playout-case-interval");
      if (present("event_interval"))
        basic_playout_options.event_interval = parse_finite_double(
            lookup("event_interval").c_str(),
            "--petri-playout-event-interval");
      if (basic_playout_options.case_interval < 0.0 ||
          basic_playout_options.event_interval < 0.0) {
        throw std::invalid_argument(
            "Petri playout intervals must be non-negative");
      }
      if (present("add_only_if_fm_is_reached"))
        basic_playout_options.add_only_if_fm_is_reached = parse_boolean(
            lookup("add_only_if_fm_is_reached").c_str(),
            "--petri-playout-add-only-if-fm-is-reached");
      if (present("fm_leq_accepted"))
        basic_playout_options.fm_leq_accepted = parse_boolean(
            lookup("fm_leq_accepted").c_str(),
            "--petri-playout-fm-leq-accepted");
      if (present("return_visited_elements"))
        basic_playout_options.return_visited_elements = parse_boolean(
            lookup("return_visited_elements").c_str(),
            "--petri-playout-return-visited-elements");
    } catch (const std::invalid_argument& error) {
      std::cerr << "error: " << error.what() << '\n';
      return 2;
    }
  }

  if (extensive_playout_options_used) {
    if (petri_playout_mode != "extensive" &&
        !extensive_playout_fields.empty()) {
      std::cerr << "error: extensive Petri controls require "
                   "--petri-playout-mode extensive\n";
      return 2;
    }
    if (petri_playout_mode == "extensive") {
      if (basic_playout_fields.contains("max_steps")) {
        std::cerr << "error: --petri-playout-max-steps requires basic mode; "
                     "use --petri-playout-max-length for extensive mode\n";
        return 2;
      }
      const auto lookup = [&](std::string_view name) {
        const auto it = extensive_playout_fields.find(std::string(name));
        return it == extensive_playout_fields.end() ? std::string{} : it->second;
      };
      const auto present = [&](std::string_view name) {
        return extensive_playout_fields.contains(std::string(name));
      };
      try {
        extensive_playout_options =
            minera::parse_extensive_playout_params(lookup, present);
      } catch (const std::invalid_argument& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 2;
      }
    }
  }

  if (dfg_playout_options_used && playout_miner != "dfg") {
    std::cerr << "error: --dfg-playout-* options require --playout-miner dfg\n";
    return 2;
  }
  if (stochastic_playout_requested &&
      (playout_miner == "declare" || playout_miner == "process_tree" ||
       playout_miner == "dfg")) {
    std::cerr << "error: stochastic playout requires a Petri --playout-miner\n";
    return 2;
  }
  if (monte_carlo_requested && stochastic_playout_requested) {
    std::cerr << "error: Monte Carlo and stochastic playout options cannot be combined\n";
    return 2;
  }
  if (stochastic_petri_requested && playout_output.empty()) {
    std::cerr << "error: --stochastic-petri requires --playout <path>\n";
    return 2;
  }
  if (stochastic_map_requested && stochastic_map_output.empty()) {
    std::cerr << "error: --stochastic-map requires an output path\n";
    return 2;
  }
  if (stochastic_petri_requested &&
      (playout_miner == "declare" || playout_miner == "process_tree" ||
       playout_miner == "dfg" || playout_miner == "stochastic" ||
       playout_miner == "monte_carlo")) {
    std::cerr << "error: --stochastic-petri requires a Petri model miner\n";
    return 2;
  }
  if (monte_carlo_requested &&
      (playout_miner == "declare" || playout_miner == "process_tree" ||
       playout_miner == "dfg")) {
    std::cerr << "error: Monte Carlo playout requires a Petri --playout-miner\n";
    return 2;
  }
  if (stochastic_playout_requested) {
    const auto lookup = [&](std::string_view name) {
      const auto it = stochastic_playout_fields.find(std::string(name));
      return it == stochastic_playout_fields.end() ? std::string{} : it->second;
    };
    const auto present = [&](std::string_view name) {
      return stochastic_playout_fields.contains(std::string(name));
    };
    try {
      stochastic_playout_options =
          minera::parse_stochastic_playout_params(lookup, present);
    } catch (const std::invalid_argument& error) {
      std::cerr << "error: " << error.what() << '\n';
      return 2;
    }
  }
  if (stochastic_petri_requested) {
    const auto lookup = [&](std::string_view name) {
      const auto it = stochastic_petri_fields.find(std::string(name));
      return it == stochastic_petri_fields.end() ? std::string{} : it->second;
    };
    const auto present = [&](std::string_view name) {
      return stochastic_petri_fields.contains(std::string(name));
    };
    try {
      stochastic_petri_options =
          minera::parse_stochastic_petri_params(lookup, present);
    } catch (const std::invalid_argument& error) {
      std::cerr << "error: " << error.what() << '\n';
      return 2;
    }
  }
  if (stochastic_map_requested) {
    const auto lookup = [&](std::string_view name) {
      const auto it = stochastic_map_fields.find(std::string(name));
      return it == stochastic_map_fields.end() ? std::string{} : it->second;
    };
    const auto present = [&](std::string_view name) {
      return stochastic_map_fields.contains(std::string(name));
    };
    try {
      stochastic_map_params =
          minera::parse_stochastic_map_params(lookup, present);
    } catch (const std::invalid_argument& error) {
      std::cerr << "error: " << error.what() << '\n';
      return 2;
    }
  }
  if (stochastic_petri_requested &&
      (petri_playout_mode != "basic" || basic_playout_options_used ||
       extensive_playout_options_used || stochastic_playout_requested)) {
    std::cerr << "error: --stochastic-petri cannot be combined with other Petri playout controls\n";
    return 2;
  }
  if (monte_carlo_requested) {
    const auto lookup = [&](std::string_view name) {
      const auto it = monte_carlo_fields.find(std::string(name));
      return it == monte_carlo_fields.end() ? std::string{} : it->second;
    };
    const auto present = [&](std::string_view name) {
      return monte_carlo_fields.contains(std::string(name));
    };
    try {
      monte_carlo_options =
          minera::parse_monte_carlo_params(lookup, present);
    } catch (const std::invalid_argument& error) {
      std::cerr << "error: " << error.what() << '\n';
      return 2;
    }
  }
  const int reverse_tree_inputs = (pnml_to_ptml_input.empty() ? 0 : 1) +
                                  (bpmn_to_ptml_input.empty() ? 0 : 1);
  if (reverse_tree_inputs > 1) {
    std::cerr << "error: pass only one of --pnml-to-ptml and "
                 "--bpmn-to-ptml\n";
    return 2;
  }
  if (reverse_tree_inputs != 0 && (!input.empty() || random_tree_requested)) {
    std::cerr << "error: workflow-net-to-PTML conversion cannot be combined "
                 "with an event-log input or random-tree generation\n";
    return 2;
  }
  if (!petri_reduction_input.empty() &&
      (!input.empty() || random_tree_requested || reverse_tree_inputs != 0)) {
    std::cerr << "error: Petri-net reduction cannot be combined with an "
                 "event-log input, random-tree generation, or reverse conversion\n";
    return 2;
  }
  if (!petri_reduction_fields.empty() && petri_reduction_input.empty()) {
    std::cerr << "error: --petri-reduction-* options require "
                 "--reduce-petri-net\n";
    return 2;
  }
  if (!pnml_inspection_fields.empty() && pnml_inspection_input.empty()) {
    std::cerr << "error: --pnml-* import options require --inspect-pnml\n";
    return 2;
  }
  if (!pnml_inspection_input.empty() &&
      (!input.empty() || random_tree_requested || reverse_tree_inputs != 0 ||
       !petri_reduction_input.empty())) {
    std::cerr << "error: PNML inspection cannot be combined with another input mode\n";
    return 2;
  }
  if (input.empty() && !random_tree_requested && reverse_tree_inputs == 0 &&
      petri_reduction_input.empty() && pnml_inspection_input.empty()) {
    return usage(argv[0]);
  }
  if (process_tree_variant !=
          minera::ProcessTreePetriNetVariant::kStandard &&
      !pnml_output.empty() && pnml_miner != "inductive") {
    std::cerr << "error: --process-tree-variant requires "
                 "--pnml-miner inductive\n";
    return 2;
  }
  if (process_tree_variant !=
          minera::ProcessTreePetriNetVariant::kStandard &&
      !bpmn_output.empty() && bpmn_miner != "inductive") {
    std::cerr << "error: --process-tree-variant requires "
                 "--bpmn-miner inductive\n";
    return 2;
  }
  if (timestamp_interleavings_requested) {
    if (timestamp_interleavings_right_path.empty() ||
        merge_relations_path.empty()) {
      std::cerr << "error: timestamp interleavings require the right log and "
                   "--case-relations\n";
      return 2;
    }
    if (!merge_incoming_path.empty()) {
      std::cerr << "error: timestamp interleavings cannot be combined with "
                   "--merge-case-relations\n";
      return 2;
    }
    // --case-relations and --merge-* column selectors share the historical
    // parser fields.  They are consumed by the interleavings branch below.
    merge_requested = false;
  } else if (merge_requested && (merge_incoming_path.empty() ||
                                 merge_relations_path.empty())) {
    std::cerr << "error: case-relation merge requires --merge-case-relations "
                 "and --case-relations\n";
    return 2;
  }

  try {
    if (!embedding_runtime_url.empty())
      minera::set_default_embedding_provider(
          minera::embedding_runtime::make_http_provider(embedding_runtime_url));
    if (reverse_tree_inputs != 0) {
      const auto net = !pnml_to_ptml_input.empty()
                           ? minera::import_pnml(pnml_to_ptml_input)
                           : minera::import_bpmn(bpmn_to_ptml_input);
      const auto tree = minera::to_process_tree(net);
      const auto body = minera::to_ptml(tree, "workflow net");
      if (output.empty()) {
        std::cout << body;
      } else {
        std::ofstream file(output, std::ios::binary);
        if (!file) {
          std::cerr << "error: cannot open PTML output file " << output
                    << '\n';
          return 1;
        }
        file << body;
      }
      return 0;
    }
    if (!petri_reduction_input.empty()) {
      const auto lookup = [&](std::string_view name) {
        const auto it = petri_reduction_fields.find(std::string(name));
        return it == petri_reduction_fields.end() ? std::string{} : it->second;
      };
      const auto present = [&](std::string_view name) {
        return petri_reduction_fields.contains(std::string(name));
      };
      auto options = minera::parse_petri_net_reduction_params(lookup, present);
      auto document = minera::import_pnml_document(petri_reduction_input);
      if (!present("petri_reduction_initial_marking"))
        options.initial_marking = minera::tools::pnml_marking<std::int64_t>(document.initial_marking);
      if (!present("petri_reduction_final_marking"))
        options.final_marking = minera::tools::pnml_marking<std::int64_t>(document.final_marking);
      const auto result = minera::reduce_petri_net(document.net, options);
      std::cout << minera::petri_net_reduction_json(result) << '\n';
      return 0;
    }
    if (!pnml_inspection_input.empty()) {
      const auto boolean = [&](const char* key, bool fallback) {
        const auto it = pnml_inspection_fields.find(key);
        if (it == pnml_inspection_fields.end()) return fallback;
        if (it->second == "true" || it->second == "1") return true;
        if (it->second == "false" || it->second == "0") return false;
        throw std::invalid_argument(std::string(key) + ": expected true or false");
      };
      minera::PnmlImportOptions options;
      options.infer_initial_marking =
          boolean("--pnml-infer-initial-marking", true);
      options.infer_final_marking =
          boolean("--pnml-infer-final-marking", true);
      options.import_stochastic_map =
          boolean("--pnml-import-stochastic-map", false);
      options.require_workflow_boundaries =
          boolean("--pnml-require-workflow-boundaries", true);
      const auto document = minera::import_pnml_document(pnml_inspection_input, options);
      const auto json = minera::pnml_document_json(document);
      if (!pnml_output.empty()) {
        const auto xml = minera::to_pnml(document);
        std::ofstream file(pnml_output);
        if (!file) throw std::runtime_error("Cannot open PNML output file " + pnml_output.string());
        file << xml;
        if (!file) throw std::runtime_error("Cannot write PNML output file " + pnml_output.string());
      }
      std::cout << json << '\n';
      return 0;
    }
    if (random_tree_requested) {
      const auto lookup = [&](std::string_view name) {
        const auto it = random_tree_fields.find(std::string(name));
        return it == random_tree_fields.end() ? std::string{} : it->second;
      };
      const auto present = [&](std::string_view name) {
        return random_tree_fields.contains(std::string(name));
      };
      const auto params = minera::parse_random_tree_params(lookup, present);
      const auto models = minera::generate_random_process_trees(params);
      std::cout << minera::random_tree_json(models, params.variant,
                                            params.activities)
                << '\n';
      return 0;
    }
    if (dfg_playout_options_used) {
      const auto lookup = [&](std::string_view name) {
        const auto it = dfg_playout_fields.find(std::string(name));
        return it == dfg_playout_fields.end() ? std::string{} : it->second;
      };
      const auto present = [&](std::string_view name) {
        return dfg_playout_fields.contains(std::string(name));
      };
      dfg_playout_options = minera::parse_dfg_playout_params(lookup, present);
    }
    minera::EventLog log;
    std::optional<minera::CsvEventData> csv_data;
    if (browser_history_requested) {
      browser_history_fields.insert_or_assign("browser_history_path",
                                               input.string());
      const auto lookup = [&](std::string_view name) {
        const auto it = browser_history_fields.find(std::string(name));
        return it == browser_history_fields.end() ? std::string{} : it->second;
      };
      const auto present = [&](std::string_view name) {
        return browser_history_fields.contains(std::string(name));
      };
      const auto browser_options =
          minera::parse_browser_history_params(lookup, present);
      log = minera::import_browser_history(browser_options);
      std::cout << minera::browser_history_json(log, browser_options.variant)
                << '\n';
      return 0;
    }
    if (windows_events_requested) {
      windows_events_fields.insert_or_assign("windows_events_path", input.string());
      const auto lookup = [&](std::string_view name) {
        const auto it = windows_events_fields.find(std::string(name));
        return it == windows_events_fields.end() ? std::string{} : it->second;
      };
      const auto present = [&](std::string_view name) {
        return windows_events_fields.contains(std::string(name));
      };
      const auto event_options =
          minera::parse_windows_event_log_params(lookup, present);
      std::cout << minera::windows_event_log_json(
                       minera::import_windows_event_log(event_options))
                << '\n';
      return 0;
    }
    if (repository_history_requested) {
      const auto lookup = [&](std::string_view name) {
        const auto it = repository_history_fields.find(std::string(name));
        return it == repository_history_fields.end() ? std::string{} : it->second;
      };
      const auto present = [&](std::string_view name) {
        return repository_history_fields.contains(std::string(name));
      };
      auto repository_options =
          minera::parse_repository_history_params(lookup, present);
      if (!repository_auth_token_env.empty()) {
        const auto* token = std::getenv(repository_auth_token_env.c_str());
        if (!token)
          throw std::invalid_argument("repository auth token environment variable is not set");
        repository_options.auth_token = token;
      }
      repository_options.transport =
          minera::tools::repository_history_http_transport();
      const auto repository_log =
          minera::import_repository_history(repository_options);
      std::cout << minera::repository_history_json(
                       repository_log, repository_options.owner,
                       repository_options.repository)
                << '\n';
      return 0;
    }
    if (workflow_history_requested) {
      const auto lookup = [&](std::string_view name) {
        const auto it = workflow_history_fields.find(std::string(name));
        return it == workflow_history_fields.end() ? std::string{} : it->second;
      };
      std::ifstream stream(input, std::ios::binary);
      if (!stream) throw std::invalid_argument("workflow history input does not exist");
      const std::string csv((std::istreambuf_iterator<char>(stream)),
                            std::istreambuf_iterator<char>());
      const auto options = minera::parse_workflow_history_params(lookup);
      std::cout << minera::workflow_history_json(
                       minera::import_workflow_history_csv(csv, options))
                << '\n';
      return 0;
    }
    if (enterprise_history_requested) {
      const auto lookup = [&](std::string_view name) {
        const auto it = enterprise_history_fields.find(std::string(name));
        return it == enterprise_history_fields.end() ? std::string{} : it->second;
      };
      std::ifstream stream(input, std::ios::binary);
      if (!stream) throw std::invalid_argument("enterprise history input does not exist");
      const std::string csv((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
      const auto options = minera::parse_enterprise_history_params(lookup);
      std::cout << minera::enterprise_history_json(
                       minera::import_enterprise_history_csv(csv, options), options.variant)
                << '\n';
      return 0;
    }
    if (communication_history_requested) {
      const auto lookup = [&](std::string_view name) { const auto it = communication_history_fields.find(std::string(name)); return it == communication_history_fields.end() ? std::string{} : it->second; };
      std::ifstream stream(input, std::ios::binary); if (!stream) throw std::invalid_argument("communication history input does not exist");
      const std::string csv((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
      const auto options = minera::parse_communication_history_params(lookup);
      std::cout << minera::communication_history_json(minera::import_communication_history_csv(csv, options), options.variant) << '\n'; return 0;
    }
    // A .xes file is read as XES; everything else is treated as CSV.
    const bool input_is_xes = is_xes_path(input);
    if (xes_import_result_requested) {
      if (!input_is_xes)
        throw std::invalid_argument("XES import result requires an XES input");
      const auto body = minera::xes_import_result_json(
          minera::import_xes_result(input, xes_options));
      if (output.empty()) std::cout << body << '\n';
      else {
        std::ofstream file(output);
        if (!file) throw std::runtime_error("Cannot open output file " + output.string());
        file << body << '\n';
        file.flush();
        if (!file) throw std::runtime_error("Cannot write output file " + output.string());
      }
      return 0;
    }
    if (input_is_xes) {
      log = minera::import_xes_event_log(input, xes_options);
    } else if (!correlation_variants.empty() || merge_requested) {
      csv_data = minera::import_csv_event_data(input, options);
      log = std::move(csv_data->log);
    } else {
      log = minera::import_csv_event_log(input, options);
    }
    if (log_conversion_requested) {
      const auto lookup = [&](std::string_view name) {
        const auto it = log_conversion_fields.find(std::string(name));
        return it == log_conversion_fields.end() ? std::string{} : it->second;
      };
      const auto present = [&](std::string_view name) {
        return log_conversion_fields.contains(std::string(name));
      };
      const auto conversion_options =
          minera::parse_log_conversion_params(lookup, present);
      const auto body = minera::log_conversion_json(
          minera::convert_event_log(log, conversion_options));
      if (output.empty()) {
        std::cout << body << '\n';
      } else {
        std::ofstream file(output);
        if (!file) {
          std::cerr << "error: cannot open output file " << output << '\n';
          return 1;
        }
        file << body << '\n';
      }
      return 0;
    }
    if (timestamp_interleavings_requested) {
      const auto right = is_xes_path(timestamp_interleavings_right_path)
                             ? minera::import_xes_event_log(
                                   timestamp_interleavings_right_path)
                             : minera::import_csv_event_log(
                                   timestamp_interleavings_right_path, options);
      std::ifstream relation_input(merge_relations_path);
      if (!relation_input)
        throw std::invalid_argument("cannot open case relations file");
      const auto left_column = merge_fields.contains("merge_left_column")
                                   ? merge_fields.at("merge_left_column")
                                   : std::string("case_id_LEFT");
      const auto right_column = merge_fields.contains("merge_right_column")
                                    ? merge_fields.at("merge_right_column")
                                    : std::string("case_id_RIGHT");
      const auto relations = minera::parse_case_relations_csv(
          relation_input, left_column, right_column);
      const auto lookup = [&](std::string_view name) {
        const auto it = timestamp_interleavings_fields.find(std::string(name));
        return it == timestamp_interleavings_fields.end() ? std::string{} : it->second;
      };
      const auto present = [&](std::string_view name) {
        return timestamp_interleavings_fields.contains(std::string(name));
      };
      const auto params = minera::parse_timestamp_interleavings_params(
          lookup, present);
      const auto body = minera::timestamp_interleavings_json(
          minera::discover_timestamp_interleavings(log, right, relations,
                                                   params));
      if (output.empty()) {
        std::cout << body << '\n';
      } else {
        std::ofstream file(output);
        if (!file) {
          std::cerr << "error: cannot open output file " << output << '\n';
          return 1;
        }
        file << body << '\n';
      }
      return 0;
    }
    if (merge_requested) {
      std::ifstream relation_input(merge_relations_path);
      if (!relation_input)
        throw std::invalid_argument("cannot open case relations file");
      const auto lookup = [&](std::string_view name) {
        const auto it = merge_fields.find(std::string(name));
        return it == merge_fields.end() ? std::string{} : it->second;
      };
      const auto present = [&](std::string_view name) {
        return merge_fields.contains(std::string(name));
      };
      auto params = minera::parse_case_relations_params(lookup, present);
      const bool merge_case_key_supplied =
          merge_fields.contains("merge_case_id_key");
      const auto relation_case_key = params.case_id_key;
      const auto left = merge_fields.contains("merge_left_column")
                            ? merge_fields.at("merge_left_column")
                            : relation_case_key + params.left_suffix;
      const auto right = merge_fields.contains("merge_right_column")
                             ? merge_fields.at("merge_right_column")
                             : relation_case_key + params.right_suffix;
      const bool incoming_is_xes = is_xes_path(merge_incoming_path);
      std::string body;
      if (!input_is_xes && !incoming_is_xes) {
        if (!csv_data)
          throw std::logic_error("CSV merge did not retain its source table");
        auto incoming_csv =
            minera::import_csv_event_data(merge_incoming_path, options);
        const auto relation_table =
            minera::parse_case_relations_table_csv(relation_input);
        params.relation_left_column = left;
        params.relation_right_column = right;
        auto base_table = csv_data->table;
        auto incoming_table = std::move(incoming_csv.table);
        normalize_csv_timestamp_missingness(base_table);
        normalize_csv_timestamp_missingness(incoming_table);
        if (!merge_case_key_supplied)
          params.case_id_key = csv_case_column(base_table, options);
        if (!merge_fields.contains("merge_timestamp_key")) {
          adapt_case_relations_table_timestamp(params, base_table,
                                                incoming_table, options);
        }
        const auto merged = minera::merge_case_relations(
            base_table, incoming_table, relation_table, params);
        body = minera::case_relations_json(merged, params);
      } else {
        std::optional<decltype(minera::import_csv_event_data(merge_incoming_path, options))> incoming_csv;
        if (!incoming_is_xes)
          incoming_csv = minera::import_csv_event_data(merge_incoming_path, options);
        const auto* csv_table = !input_is_xes ? &csv_data->table
                                             : (incoming_csv ? &incoming_csv->table : nullptr);
        if (csv_table && !merge_case_key_supplied)
          params.case_id_key = minera::tools::relation_role_column(
              *csv_table, minera::FirstClassAttributeRole::kCaseId, csv_case_column(*csv_table, options));
        const bool explicit_timestamp = merge_fields.contains("merge_timestamp_key");
        if (csv_table && !explicit_timestamp)
          params.timestamp_key = minera::tools::relation_role_column(
              *csv_table, minera::FirstClassAttributeRole::kTimestamp, options.timestamp_column);
        auto base_table = input_is_xes
            ? minera::tools::relation_xes_table(log, params, merge_case_key_supplied, explicit_timestamp)
            : csv_data->table;
        auto incoming_table = incoming_is_xes
            ? minera::tools::relation_xes_table(
                  minera::import_xes_event_log(merge_incoming_path, xes_options), params,
                  merge_case_key_supplied, explicit_timestamp)
            : std::move(incoming_csv->table);
        if (!input_is_xes) { normalize_csv_timestamp_missingness(base_table); minera::tools::relation_csv_dates(base_table, params.timestamp_key); }
        if (!incoming_is_xes) { normalize_csv_timestamp_missingness(incoming_table); minera::tools::relation_csv_dates(incoming_table, params.timestamp_key); }
        params.relation_left_column = left;
        params.relation_right_column = right;
        const auto relation_table = minera::parse_case_relations_table_csv(relation_input);
        body = minera::case_relations_json(
            minera::merge_case_relations(base_table, incoming_table, relation_table, params), params);
      }
      if (output.empty()) {
        std::cout << body << '\n';
      } else {
        std::ofstream file(output);
        if (!file) {
          std::cerr << "error: cannot open output file " << output << '\n';
          return 1;
        }
        file << body << '\n';
        file.flush();
        if (!file) {
          std::cerr << "error: cannot write output file " << output << '\n';
          return 1;
        }
      }
      return 0;
    }
    if (filter.any()) {
      log = minera::filter_event_log(log, filter);
    }
    if (!segment_fields.empty()) {
      const auto lookup = [&](std::string_view name) {
        const auto it = segment_fields.find(std::string(name));
        return it == segment_fields.end() ? std::string{} : it->second;
      };
      const auto present = [&](std::string_view name) {
        return segment_fields.contains(std::string(name));
      };
      const auto params = minera::parse_mine_params(lookup, present);
      if (params.segment_enabled && params.segment)
        log = minera::filter_segments(log, *params.segment);
    }
    if (!path_fields.empty()) {
      const auto lookup = [&](std::string_view name) {
        const auto it = path_fields.find(std::string(name));
        return it == path_fields.end() ? std::string{} : it->second;
      };
      const auto present = [&](std::string_view name) {
        return path_fields.contains(std::string(name));
      };
      const auto params = minera::parse_mine_params(lookup, present);
      if (params.path_enabled && params.path)
        log = minera::filter_adjacent_path(log, *params.path);
    }
    if (!ltl_fields.empty()) {
      const auto lookup = [&](std::string_view name) {
        const auto it = ltl_fields.find(std::string(name));
        return it == ltl_fields.end() ? std::string{} : it->second;
      };
      const auto present = [&](std::string_view name) {
        return ltl_fields.contains(std::string(name));
      };
      const auto params = minera::parse_mine_params(lookup, present);
      if (params.ltl_enabled && params.ltl)
        log = minera::filter_ltl_event_log(log, *params.ltl);
    }
    if (!token_replay_model.empty()) {
      if (!ends_with_ci(input.string(), ".csv") && !is_xes_path(input))
        throw std::invalid_argument("--token-replay-model expects CSV or XES input");
      minera::PnmlImportOptions model_options;
      model_options.infer_initial_marking = false;
      model_options.infer_final_marking = false;
      model_options.require_workflow_boundaries = false;
      model_options.public_marking_semantics = replay_fitness_result_requested;
      const auto document = minera::import_pnml_document(token_replay_model, model_options);
      const auto replay_options = minera::tools::default_replay_input_options(
          log, minera::tools::pnml_replay_options(document, token_replay_options),
          token_replay_activity_option_used);
      std::string body;
      if (replay_fitness_result_requested) {
        if (evaluation_fields.contains("fitness_variant") &&
            evaluation_params->replay_fitness_variant != minera::ReplayFitnessVariant::TOKEN_BASED)
          throw std::invalid_argument("standalone replay fitness supports token-based fitness");
        body = minera::token_replay_fitness_result_json(minera::evaluate_replay_fitness(
            log, document, minera::ReplayFitnessVariant::TOKEN_BASED,
            evaluation_params->replay_fitness_options));
      } else if (token_replay_result_requested) {
        body = minera::token_replay_result_json(minera::replay_token_result(log, document.net, replay_options));
      } else {
        body = minera::token_replay_traces_json(
            minera::replay_token_traces(log, document.net, replay_options), replay_options.variant);
      }
      if (output.empty()) std::cout << body << '\n';
      else {
        std::ofstream file(output);
        if (!file) throw std::runtime_error("Cannot open output file " + output.string());
        file << body << '\n';
        if (!file) throw std::runtime_error("Cannot write output file " + output.string());
      }
      return 0;
    }
    if (alpha_plus_result) {
      const auto body = minera::alpha_plus_result_json(
          minera::discover_alpha_plus_result(log, alpha_options));
      if (output.empty()) std::cout << body << '\n';
      else {
        std::ofstream file(output);
        if (!file) throw std::runtime_error("Cannot open output file " + output.string());
        file << body << '\n';
        if (!file) throw std::runtime_error("Cannot write output file " + output.string());
      }
      return 0;
    }
    if (!dfg_dataframe_output.empty()) {
      const auto lookup = [&](std::string_view name) {
        const auto it = dfg_dataframe_fields.find(std::string(name));
        return it == dfg_dataframe_fields.end() ? std::string{} : it->second;
      };
      const auto present = [&](std::string_view name) {
        return dfg_dataframe_fields.contains(std::string(name));
      };
      const auto params = minera::parse_mine_params(lookup, present);
      const auto table = minera::event_log_to_table(log);
      std::string body;
      if (params.dfg_operation == minera::DfgTableOperation::kGraph) {
        if (params.dfg_backend == minera::DfgTableBackend::kPandas) {
          body = minera::serialize_dfg_dataframe_json(
              minera::discover_pandas_dfg(table, params.dfg_table_options));
        } else {
          body = minera::serialize_dfg_dataframe_json(
              minera::discover_polars_dfg(
                  minera::LazyEventTable::from_table(table),
                  params.dfg_table_options));
        }
      } else if (params.dfg_operation ==
                 minera::DfgTableOperation::kPartialOrder) {
        if (params.dfg_backend == minera::DfgTableBackend::kPandas) {
          body = minera::serialize_dfg_partial_order_json(
              minera::discover_pandas_partial_order(
                  table, params.dfg_table_options));
        } else {
          body = minera::serialize_dfg_partial_order_json(
              minera::discover_polars_partial_order(
                  minera::LazyEventTable::from_table(table),
                  params.dfg_table_options));
        }
      } else {
        if (params.dfg_backend == minera::DfgTableBackend::kPandas) {
          body = minera::serialize_dfg_concurrent_events_json(
              minera::discover_pandas_concurrent_events(
                  table, params.dfg_table_options));
        } else {
          body = minera::serialize_dfg_concurrent_events_json(
              minera::discover_polars_concurrent_events(
                  minera::LazyEventTable::from_table(table),
                  params.dfg_table_options));
        }
      }
      std::ofstream dataframe_file(dfg_dataframe_output,
                                   std::ios::binary | std::ios::trunc);
      if (!dataframe_file) {
        std::cerr << "error: cannot open dataframe DFG output file "
                  << dfg_dataframe_output << '\n';
        return 1;
      }
      dataframe_file << body << '\n';
      if (!dataframe_file) {
        std::cerr << "error: failed writing dataframe DFG output file "
                  << dfg_dataframe_output << '\n';
        return 1;
      }
      return 0;
    }
    std::optional<minera::IntervalEventOverlapCounts> overlap_interval_events;
    std::optional<minera::CaseOverlapCounts> overlap_cases;
    std::optional<minera::ConcurrentActivityOverlapResult>
        overlap_concurrent_activities;
    if (overlap_requested) {
      const auto lookup = [&](std::string_view name) {
        const auto it = overlap_fields.find(std::string(name));
        return it == overlap_fields.end() ? std::string{} : it->second;
      };
      const auto present = [&](std::string_view name) {
        return overlap_fields.contains(std::string(name));
      };
      const auto params = minera::parse_mine_params(lookup, present);
      if (params.overlap) {
        overlap_interval_events =
            minera::discover_interval_event_overlap_counts(log,
                                                             params.overlap_options);
        overlap_cases = minera::discover_case_overlap_counts(log,
                                                              params.overlap_options);
        overlap_concurrent_activities = minera::discover_concurrent_activity_overlap(
            log, params.overlap_options);
      }
    }
    if (chaotic_activities_requested) {
      const auto lookup = [&](std::string_view name) {
        const auto it = chaotic_activities_fields.find(std::string(name));
        return it == chaotic_activities_fields.end() ? std::string{} : it->second;
      };
      const auto present = [&](std::string_view name) {
        return chaotic_activities_fields.contains(std::string(name));
      };
      const auto params = minera::parse_chaotic_activities_params(lookup, present);
      const auto body = minera::chaotic_activities_json(
          minera::discover_chaotic_activities(log, params));
      if (output.empty()) {
        std::cout << body << '\n';
      } else {
        std::ofstream file(output);
        if (!file) {
          std::cerr << "error: cannot open output file " << output << '\n';
          return 1;
        }
        file << body << '\n';
      }
      return 0;
    }
    if (passed_time_requested) {
      const auto lookup = [&](std::string_view name) {
        const auto it = passed_time_fields.find(std::string(name));
        return it == passed_time_fields.end() ? std::string{} : it->second;
      };
      const auto present = [&](std::string_view name) {
        return passed_time_fields.contains(std::string(name));
      };
      const auto params = minera::parse_passed_time_params(lookup, present);
      const auto body = minera::passed_time_json(
          minera::discover_passed_time(log, params));
      if (output.empty()) {
        std::cout << body << '\n';
      } else {
        std::ofstream file(output);
        if (!file) {
          std::cerr << "error: cannot open output file " << output << '\n';
          return 1;
        }
        file << body << '\n';
      }
      return 0;
    }
    if (service_time_requested) {
      const auto lookup = [&](std::string_view name) {
        const auto it = service_time_fields.find(std::string(name));
        return it == service_time_fields.end() ? std::string{} : it->second;
      };
      const auto present = [&](std::string_view name) {
        return service_time_fields.contains(std::string(name));
      };
      const auto params = minera::parse_service_time_params(lookup, present);
      const auto body = minera::service_time_json(
          minera::discover_service_time(log, params));
      if (output.empty()) {
        std::cout << body << '\n';
      } else {
        std::ofstream file(output);
        if (!file) {
          std::cerr << "error: cannot open output file " << output << '\n';
          return 1;
        }
        file << body << '\n';
      }
      return 0;
    }
    if (rework_requested) {
      const auto lookup = [&](std::string_view name) {
        const auto it = rework_fields.find(std::string(name));
        return it == rework_fields.end() ? std::string{} : it->second;
      };
      const auto present = [&](std::string_view name) {
        return rework_fields.contains(std::string(name));
      };
      const auto params = minera::parse_rework_params(lookup, present);
      const auto body = minera::rework_json(
          minera::discover_rework(log, params));
      if (output.empty()) {
        std::cout << body << '\n';
      } else {
        std::ofstream file(output);
        if (!file) {
          std::cerr << "error: cannot open output file " << output << '\n';
          return 1;
        }
        file << body << '\n';
      }
      return 0;
    }
    if (cycle_time_requested) {
      const auto lookup = [&](std::string_view name) {
        const auto it = cycle_time_fields.find(std::string(name));
        return it == cycle_time_fields.end() ? std::string{} : it->second;
      };
      const auto present = [&](std::string_view name) {
        return cycle_time_fields.contains(std::string(name));
      };
      const auto params = minera::parse_cycle_time_params(lookup, present);
      const auto body = minera::cycle_time_json(
          minera::discover_cycle_time(log, params));
      if (output.empty()) {
        std::cout << body << '\n';
      } else {
        std::ofstream file(output);
        if (!file) {
          std::cerr << "error: cannot open output file " << output << '\n';
          return 1;
        }
        file << body << '\n';
      }
      return 0;
    }
    if (case_arrival_requested) {
      const auto lookup = [&](std::string_view name) {
        const auto it = case_arrival_fields.find(std::string(name));
        return it == case_arrival_fields.end() ? std::string{} : it->second;
      };
      const auto present = [&](std::string_view name) {
        return case_arrival_fields.contains(std::string(name));
      };
      const auto params = minera::parse_case_arrival_params(lookup, present);
      const auto body = minera::case_arrival_json(
          minera::discover_case_arrival(log, params));
      if (output.empty()) {
        std::cout << body << '\n';
      } else {
        std::ofstream file(output);
        if (!file) {
          std::cerr << "error: cannot open output file " << output << '\n';
          return 1;
        }
        file << body << '\n';
      }
      return 0;
    }
    if (case_statistics_requested) {
      const auto lookup = [&](std::string_view name) {
        const auto it = case_statistics_fields.find(std::string(name));
        return it == case_statistics_fields.end() ? std::string{} : it->second;
      };
      const auto present = [&](std::string_view name) {
        return case_statistics_fields.contains(std::string(name));
      };
      const auto params = minera::parse_case_statistics_params(lookup, present);
      const auto body = minera::case_statistics_json(
          minera::discover_case_statistics(log, params));
      if (output.empty()) {
        std::cout << body << '\n';
      } else {
        std::ofstream file(output);
        if (!file) {
          std::cerr << "error: cannot open output file " << output << '\n';
          return 1;
        }
        file << body << '\n';
      }
      return 0;
    }
    if (attribute_statistics_requested) {
      const auto lookup = [&](std::string_view name) {
        const auto it = attribute_statistics_fields.find(std::string(name));
        return it == attribute_statistics_fields.end() ? std::string{} : it->second;
      };
      const auto present = [&](std::string_view name) {
        return attribute_statistics_fields.contains(std::string(name));
      };
      const auto params =
          minera::parse_attribute_statistics_params(lookup, present);
      const auto body = minera::attribute_statistics_json(
          minera::discover_attribute_statistics(log, params));
      if (output.empty()) {
        std::cout << body << '\n';
      } else {
        std::ofstream file(output);
        if (!file) {
          std::cerr << "error: cannot open output file " << output << '\n';
          return 1;
        }
        file << body << '\n';
      }
      return 0;
    }
    if (attribute_distribution_requested) {
      const auto lookup = [&](std::string_view name) {
        const auto it = attribute_distribution_fields.find(std::string(name));
        return it == attribute_distribution_fields.end() ? std::string{} : it->second;
      };
      const auto present = [&](std::string_view name) {
        return attribute_distribution_fields.contains(std::string(name));
      };
      const auto params =
          minera::parse_attribute_distribution_params(lookup, present);
      const auto body = minera::attribute_distribution_json(
          minera::discover_event_distribution(log, params));
      if (output.empty()) {
        std::cout << body << '\n';
      } else {
        std::ofstream file(output);
        if (!file) {
          std::cerr << "error: cannot open output file " << output << '\n';
          return 1;
        }
        file << body << '\n';
      }
      return 0;
    }
    if (attribute_kde_requested) {
      const auto lookup = [&](std::string_view name) {
        const auto it = attribute_kde_fields.find(std::string(name));
        return it == attribute_kde_fields.end() ? std::string{} : it->second;
      };
      const auto present = [&](std::string_view name) {
        return attribute_kde_fields.contains(std::string(name));
      };
      const auto params = minera::parse_attribute_kde_params(lookup, present);
      const auto body = minera::attribute_kde_json(
          minera::discover_attribute_kde(log, params));
      if (output.empty()) {
        std::cout << body << '\n';
      } else {
        std::ofstream file(output);
        if (!file) {
          std::cerr << "error: cannot open output file " << output << '\n';
          return 1;
        }
        file << body << '\n';
      }
      return 0;
    }
    if (boundary_activities_requested) {
      const auto lookup = [&](std::string_view name) {
        const auto it = boundary_activities_fields.find(std::string(name));
        return it == boundary_activities_fields.end() ? std::string{} : it->second;
      };
      const auto present = [&](std::string_view name) {
        return boundary_activities_fields.contains(std::string(name));
      };
      const auto params =
          minera::parse_boundary_activities_params(lookup, present);
      const auto body = minera::boundary_activities_json(
          minera::discover_boundary_activities(log, params));
      if (output.empty()) {
        std::cout << body << '\n';
      } else {
        std::ofstream file(output);
        if (!file) {
          std::cerr << "error: cannot open output file " << output << '\n';
          return 1;
        }
        file << body << '\n';
      }
      return 0;
    }
    if (variant_statistics_requested) {
      const auto lookup = [&](std::string_view name) {
        const auto it = variant_statistics_fields.find(std::string(name));
        return it == variant_statistics_fields.end() ? std::string{} : it->second;
      };
      const auto present = [&](std::string_view name) {
        return variant_statistics_fields.contains(std::string(name));
      };
      const auto params = minera::parse_variant_statistics_params(lookup, present);
      const auto body = minera::variant_statistics_json(
          minera::discover_variant_statistics(log, params));
      if (output.empty()) {
        std::cout << body << '\n';
      } else {
        std::ofstream file(output);
        if (!file) {
          std::cerr << "error: cannot open output file " << output << '\n';
          return 1;
        }
        file << body << '\n';
      }
      return 0;
    }
    if (ilp_mine_requested) {
      const auto lookup = [&](std::string_view name) {
        const auto it = ilp_fields.find(std::string(name));
        return it == ilp_fields.end() ? std::string{} : it->second;
      };
      const auto present = [&](std::string_view name) {
        return ilp_fields.contains(std::string(name));
      };
      const auto options = minera::parse_ilp_miner_params(lookup, present);
      const auto body = minera::ilp_miner_json(minera::mine_ilp(log, options));
      if (output.empty()) {
        std::cout << body << '\n';
      } else {
        std::ofstream file(output);
        if (!file) {
          std::cerr << "error: cannot open output file " << output << '\n';
          return 1;
        }
        file << body << '\n';
      }
      return 0;
    }
    if (genetic_mine_requested) {
      const auto lookup = [&](std::string_view name) {
        const auto it = genetic_fields.find(std::string(name));
        return it == genetic_fields.end() ? std::string{} : it->second;
      };
      const auto present = [&](std::string_view name) {
        return genetic_fields.contains(std::string(name));
      };
      if (!genetic_matrix_conversion_requested &&
          genetic_fields.contains("genetic_conversion_variant"))
        throw std::invalid_argument(
            "--genetic-conversion-variant requires --genetic-matrix-convert");
      const auto options = minera::parse_genetic_miner_params(lookup, present);
      const auto mined = minera::mine_genetic(log, options);
      const auto body = [&] {
        if (!genetic_matrix_conversion_requested)
          return minera::genetic_miner_json(mined);
        const auto variant = genetic_fields.contains("genetic_conversion_variant")
                                 ? minera::parse_genetic_matrix_conversion_variant(
                                       genetic_fields.at("genetic_conversion_variant"))
                                 : minera::GeneticMatrixConversionVariant::kToPetriNet;
        return minera::genetic_matrix_conversion_json(
            minera::convert_genetic_matrix(mined.matrix, variant));
      }();
      if (output.empty()) {
        std::cout << body << '\n';
      } else {
        std::ofstream file(output);
        if (!file) {
          std::cerr << "error: cannot open output file " << output << '\n';
          return 1;
        }
        file << body << '\n';
      }
      return 0;
    }
    if (organizational_sna_requested) {
      const auto lookup = [&](std::string_view name) {
        const auto it = organizational_sna_fields.find(std::string(name));
        return it == organizational_sna_fields.end() ? std::string{} : it->second;
      };
      const auto present = [&](std::string_view name) {
        return organizational_sna_fields.contains(std::string(name));
      };
      const auto params = minera::parse_organizational_sna_params(lookup, present);
      const auto body = minera::organizational_sna_json(
          minera::discover_organizational_sna(log, params));
      if (output.empty()) std::cout << body << '\n';
      else {
        std::ofstream file(output);
        if (!file) { std::cerr << "error: cannot open output file " << output << '\n'; return 1; }
        file << body << '\n';
      }
      return 0;
    }
    if (network_analysis_requested) {
      const auto lookup = [&](std::string_view name) { const auto it = network_analysis_fields.find(std::string(name)); return it == network_analysis_fields.end() ? std::string{} : it->second; };
      const auto present = [&](std::string_view name) { return network_analysis_fields.contains(std::string(name)); };
      const auto params = minera::parse_network_analysis_params(lookup, present);
      const auto body = minera::network_analysis_json(minera::discover_network_analysis(log, params));
      if (output.empty()) std::cout << body << '\n';
      else { std::ofstream file(output); if (!file) { std::cerr << "error: cannot open output file " << output << '\n'; return 1; } file << body << '\n'; }
      return 0;
    }
    if (link_analysis_requested) {
      const auto lookup = [&](std::string_view name) {
        const auto it = link_analysis_fields.find(std::string(name));
        return it == link_analysis_fields.end() ? std::string{} : it->second;
      };
      const auto present = [&](std::string_view name) {
        return link_analysis_fields.contains(std::string(name));
      };
      const auto params = minera::parse_link_analysis_params(lookup, present);
      const auto body = minera::link_analysis_json(
          minera::discover_link_analysis(log, params));
      if (output.empty()) std::cout << body << '\n';
      else {
        std::ofstream file(output);
        if (!file) {
          std::cerr << "error: cannot open output file " << output << '\n';
          return 1;
        }
        file << body << '\n';
      }
      return 0;
    }
    if (roles_requested) {
      const auto lookup = [&](std::string_view name) { const auto it = roles_fields.find(std::string(name)); return it == roles_fields.end() ? std::string{} : it->second; };
      const auto present = [&](std::string_view name) { return roles_fields.contains(std::string(name)); };
      const auto params = minera::parse_role_discovery_params(lookup, present);
      const auto body = minera::role_discovery_json(minera::discover_roles(log, params));
      if (output.empty()) std::cout << body << '\n';
      else { std::ofstream file(output); if (!file) { std::cerr << "error: cannot open output file " << output << '\n'; return 1; } file << body << '\n'; }
      return 0;
    }
    if (resource_profiles_requested) {
      const auto lookup = [&](std::string_view name) { const auto it = resource_profiles_fields.find(std::string(name)); return it == resource_profiles_fields.end() ? std::string{} : it->second; };
      const auto present = [&](std::string_view name) { return resource_profiles_fields.contains(std::string(name)); };
      const auto params = minera::parse_resource_profiles_params(lookup, present);
      const auto body = minera::resource_profiles_json(minera::discover_resource_profiles(log, params));
      if (output.empty()) std::cout << body << '\n';
      else { std::ofstream file(output); if (!file) { std::cerr << "error: cannot open output file " << output << '\n'; return 1; } file << body << '\n'; }
      return 0;
    }
    if (local_diagnostics_requested) {
      const auto lookup = [&](std::string_view name) { const auto it = local_diagnostics_fields.find(std::string(name)); return it == local_diagnostics_fields.end() ? std::string{} : it->second; };
      const auto present = [&](std::string_view name) { return local_diagnostics_fields.contains(std::string(name)); };
      const auto params = minera::parse_local_diagnostics_params(lookup, present);
      const auto body = minera::local_diagnostics_json(minera::discover_local_diagnostics(log, params));
      if (output.empty()) std::cout << body << '\n';
      else { std::ofstream file(output); if (!file) { std::cerr << "error: cannot open output file " << output << '\n'; return 1; } file << body << '\n'; }
      return 0;
    }
    if (clustering_requested) {
      const auto lookup = [&](std::string_view name) { const auto it = clustering_fields.find(std::string(name)); return it == clustering_fields.end() ? std::string{} : it->second; };
      const auto present = [&](std::string_view name) { return clustering_fields.contains(std::string(name)); };
      const auto params = minera::parse_clustering_params(lookup, present);
      const auto body = minera::trace_attribute_clustering_json(minera::discover_trace_attribute_clustering(log, params));
      if (output.empty()) std::cout << body << '\n';
      else { std::ofstream file(output); if (!file) { std::cerr << "error: cannot open output file " << output << '\n'; return 1; } file << body << '\n'; }
      return 0;
    }
    if (profile_clustering_requested) {
      const auto lookup = [&](std::string_view name) {
        const auto it = profile_clustering_fields.find(std::string(name));
        return it == profile_clustering_fields.end() ? std::string{} : it->second;
      };
      const auto present = [&](std::string_view name) {
        return profile_clustering_fields.contains(std::string(name));
      };
      const auto params = minera::parse_profile_clustering_params(lookup, present);
      const auto body = minera::profile_clustering_json(
          minera::discover_profile_clustering(log, params));
      if (output.empty()) std::cout << body << '\n';
      else { std::ofstream file(output); if (!file) { std::cerr << "error: cannot open output file " << output << '\n'; return 1; } file << body << '\n'; }
      return 0;
    }
    if (trace_variant_query_requested) {
      const auto lookup = [&](std::string_view name) {
        const auto it = trace_variant_query_fields.find(std::string(name));
        return it == trace_variant_query_fields.end() ? std::string{} : it->second;
      };
      const auto present = [&](std::string_view name) {
        return trace_variant_query_fields.contains(std::string(name));
      };
      const auto params = minera::parse_trace_variant_query_params(lookup, present);
      const auto body = minera::trace_variant_query_json(
          minera::anonymize_trace_variant_query(log, params));
      if (output.empty()) {
        std::cout << body << '\n';
      } else {
        std::ofstream file(output);
        if (!file) {
          std::cerr << "error: cannot open output file " << output << '\n';
          return 1;
        }
        file << body << '\n';
      }
      return 0;
    }
    if (pripel_requested) {
      if (pripel_query_path.empty())
        throw std::invalid_argument("--pripel requires a query log path");
      const auto query = is_xes_path(pripel_query_path)
                             ? minera::import_xes_event_log(pripel_query_path)
                             : minera::import_csv_event_log(pripel_query_path, options);
      const auto lookup = [&](std::string_view name) {
        const auto it = pripel_fields.find(std::string(name));
        return it == pripel_fields.end() ? std::string{} : it->second;
      };
      const auto present = [&](std::string_view name) {
        return pripel_fields.contains(std::string(name));
      };
      const auto params = minera::parse_pripel_params(lookup, present);
      const auto body = minera::pripel_json(
          minera::anonymize_pripel(log, query, params));
      if (output.empty()) {
        std::cout << body << '\n';
      } else {
        std::ofstream file(output);
        if (!file) {
          std::cerr << "error: cannot open output file " << output << '\n';
          return 1;
        }
        file << body << '\n';
      }
      return 0;
    }
    if (concept_drift_requested) {
      const auto lookup = [&](std::string_view name) {
        const auto it = concept_drift_fields.find(std::string(name));
        return it == concept_drift_fields.end() ? std::string{} : it->second;
      };
      const auto present = [&](std::string_view name) {
        return concept_drift_fields.contains(std::string(name));
      };
      const auto params = minera::parse_concept_drift_params(lookup, present);
      std::cout << minera::concept_drift_json(
          minera::detect_concept_drift(log, params)) << '\n';
      return 0;
    }
    if (target_requested) {
      const auto lookup = [&](std::string_view name) {
        const auto it = target_fields.find(std::string(name));
        return it == target_fields.end() ? std::string{} : it->second;
      };
      const auto present = [&](std::string_view name) {
        return target_fields.contains(std::string(name));
      };
      const auto params = minera::parse_log_to_target_params(lookup, present);
      std::cout << minera::log_to_target_json(
          minera::log_to_target(log, params)) << '\n';
      return 0;
    }
    if (features_requested) {
      const auto lookup = [&](std::string_view name) {
        const auto it = features_fields.find(std::string(name));
        return it == features_fields.end() ? std::string{} : it->second;
      };
      const auto present = [&](std::string_view name) {
        return features_fields.contains(std::string(name));
      };
      const auto params = minera::parse_log_to_features_params(lookup, present);
      std::cout << minera::log_to_features_json(minera::log_to_features(log, params)) << '\n';
      return 0;
    }
    if (embeddings_requested) {
      const auto lookup = [&](std::string_view name) {
        const auto it = embedding_fields.find(std::string(name));
        return it == embedding_fields.end() ? std::string{} : it->second;
      };
      const auto present = [&](std::string_view name) {
        return embedding_fields.contains(std::string(name));
      };
      const auto options = minera::parse_embedding_params(lookup, present);
      const auto result = minera::to_embeddings(log, options);
      if (embedding_target_requested) {
        if (embedding_target.empty())
          throw std::invalid_argument("--embedding-target expects non-empty text");
        const auto matches = minera::embedding_similarity(
            embedding_target, result, options, embedding_top_k);
        std::cout << minera::serialize_embedding_similarity_json(
            embedding_target, result, matches, embedding_top_k)
                  << '\n';
      } else {
        std::cout << minera::serialize_embeddings_json(result) << '\n';
      }
      return 0;
    }
    if (label_splitting_requested) {
      const auto lookup = [&](std::string_view name) {
        const auto it = label_splitting_fields.find(std::string(name));
        return it == label_splitting_fields.end() ? std::string{} : it->second;
      };
      const auto present = [&](std::string_view name) {
        return label_splitting_fields.contains(std::string(name));
      };
      const auto params = minera::parse_label_splitting_params(lookup, present);
      std::cout << minera::label_splitting_json(
          minera::split_activity_labels(log, params)) << '\n';
      return 0;
    }
    if (trie_requested) {
      const auto lookup = [&](std::string_view name) {
        const auto it = trie_fields.find(std::string(name));
        return it == trie_fields.end() ? std::string{} : it->second;
      };
      const auto present = [&](std::string_view name) {
        return trie_fields.contains(std::string(name));
      };
      const auto params = minera::parse_trie_params(lookup, present);
      const auto trie = minera::discover_trie(log, params);
      if (trie_to_petri_requested) {
        const auto net = minera::convert_trie_to_petri_net(trie);
        std::cout << minera::serialize_trie_petri_net_json(net) << '\n';
      } else {
        std::cout << minera::trie_json(trie) << '\n';
      }
      return 0;
    }
    if (interval_requested) {
      const auto lookup = [&](std::string_view name) {
        const auto it = interval_fields.find(std::string(name));
        return it == interval_fields.end() ? std::string{} : it->second;
      };
      const auto present = [&](std::string_view name) {
        return interval_fields.contains(std::string(name));
      };
      const auto params = minera::parse_interval_tree_params(lookup, present);
      const auto intervals =
          minera::discover_open_path_intervals(log, params);
      const auto query = minera::query_open_path_intervals(
          intervals, minera::interval_tree_query_options(params));
      std::cout << minera::interval_tree_json(query) << '\n';
      return 0;
    }
    if (!compare_petri_model.empty()) {
      if (!ends_with_ci(compare_petri_reference.string(), ".csv") &&
          !is_xes_path(compare_petri_reference))
        throw std::invalid_argument("--compare-petri-reference expects a .csv or .xes file");
      minera::PnmlImportOptions model_options;
      model_options.infer_initial_marking = false;
      model_options.infer_final_marking = false;
      model_options.require_workflow_boundaries = false;
      const auto document = minera::import_pnml_document(compare_petri_model, model_options);
      const auto reference = [&]() {
        if (is_xes_path(compare_petri_reference)) {
          minera::XesImportOptions reference_options;
          if (!compare_petri_activity_attribute.empty())
            reference_options.activity_attribute = compare_petri_activity_attribute;
          return minera::import_xes_event_log(compare_petri_reference, reference_options);
        }
        auto reference_options = options;
        reference_options.allow_case_id_aliases = true;
        if (!compare_petri_activity_attribute.empty() &&
            compare_petri_activity_attribute != "activity")
          reference_options.activity_column = compare_petri_activity_attribute;
        return minera::import_csv_event_log(compare_petri_reference, reference_options);
      }();
      const auto marking = [](const minera::TokenMarking& source) {
        std::vector<std::uint32_t> result;
        for (const auto count : source) {
          if (count < 0 || count > std::numeric_limits<std::uint32_t>::max())
            throw std::invalid_argument("PNML comparison marking exceeds the supported token range");
          result.push_back(static_cast<std::uint32_t>(count));
        }
        return result;
      };
      minera::PetriUsageComparisonOptions comparison_options;
      comparison_options.place_names = document.place_ids;
      comparison_options.transition_names = document.transition_ids;
      comparison_options.activity_attribute = compare_petri_activity_attribute;
      comparison_options.walk_through_hidden_trans = compare_petri_walk_through_hidden;
      comparison_options.try_to_reach_final_marking_through_hidden = compare_petri_reach_final_through_hidden;
      comparison_options.stop_immediately_unfit = compare_petri_stop_immediately_unfit;
      comparison_options.cleaning_token_flood = compare_petri_cleaning_token_flood;
      comparison_options.exhaustive_invisible_exploration = compare_petri_exhaustive_invisible_exploration;
      comparison_options.initial_marking = compare_petri_initial_marking_used
          ? compare_petri_initial_marking : marking(document.initial_marking);
      comparison_options.final_marking = compare_petri_final_marking_used
          ? compare_petri_final_marking : marking(document.final_marking);
      if (!compare_petri_initial_marking_used)
        comparison_options.initial_marking_keys = document.initial_marking_keys;
      if (!compare_petri_final_marking_used)
        comparison_options.final_marking_keys = document.final_marking_keys;
      const auto body = minera::petri_usage_comparison_json(
          minera::compare_petri_usage(document.net, log, reference, comparison_options));
      if (output.empty()) std::cout << body << '\n';
      else {
        std::ofstream file(output);
        if (!file) throw std::runtime_error("Cannot open output file " + output.string());
        file << body << '\n';
      }
      return 0;
    }
    auto graph = minera::discover_directly_follows_graph(log);
    register_activity_selector_values(graph, log,
                                      token_replay_options.activity_attribute);
    register_activity_selector_values(graph, log,
                                      alignment_options.activity_attribute);
    register_activity_selector_values(graph, log,
                                      heuristic_options.activity_attribute);
    register_activity_selector_values(
        graph, log, temporal_profile_options.activity_attribute);
    register_activity_selector_values(
        graph, log,
        transition_options.activity_attribute.empty()
            ? transition_options.activity_key
            : transition_options.activity_attribute);
    register_activity_selector_values(
        graph, log, dfg_statistics_options.activity_attribute);
    std::optional<minera::DfgAlignmentLogResult> dfg_alignment_result;
    std::optional<minera::DfgAlignmentProjection> dfg_projection_result;
    std::optional<minera::EventLog> dfg_projected_log;
    if (align_dfg) {
      minera::DfgStatisticalOptions dfg_opts;
      dfg_opts.window = 1;
      dfg_opts.activity_attribute = dfg_alignment_options.activity_attribute;
      dfg_opts.timestamp_attribute = dfg_alignment_options.timestamp_attribute;
      const auto dfg = minera::discover_frequency_dfg(log, dfg_opts);
      std::map<std::string, std::uint64_t> starts, ends;
      for (const auto& trace : log.traces) {
        if (trace.events.empty()) continue;
        const auto* first = minera::dfg_event_field_value(
            trace.events.front(), dfg_alignment_options.activity_attribute);
        const auto* last = minera::dfg_event_field_value(
            trace.events.back(), dfg_alignment_options.activity_attribute);
        if (first == nullptr || last == nullptr) {
          throw std::invalid_argument(
              "DFG alignment: event is missing activity attribute '" +
              dfg_alignment_options.activity_attribute + "'");
        }
        ++starts[*first];
        ++ends[*last];
      }
      dfg_alignment_result = minera::align_log_on_dfg(log, dfg, starts, ends, dfg_alignment_options);
      if (dfg_projection == "metrics")
        dfg_projection_result = minera::project_alignments_on_dfg(
            log, dfg, starts, ends, *dfg_alignment_result,
            dfg_alignment_options);
      if (dfg_projection == "log")
        dfg_projected_log = minera::project_log_on_dfg(
            log, dfg, starts, ends, *dfg_alignment_result,
            dfg_alignment_options);
    }
    std::optional<std::vector<minera::EditDistanceTypedAlignment>>
        typed_edit_result;
    std::optional<minera::EventLog> edit_projected_log;
    if (!edit_reference.empty()) {
      const auto reference = is_xes_path(edit_reference)
          ? minera::import_xes_event_log(edit_reference)
          : minera::import_csv_event_log(edit_reference, options);
      typed_edit_result =
          minera::align_typed_edit_distance(log, reference, edit_options);
      if (edit_projection == "log") {
        edit_projected_log = minera::project_log_on_typed_alignments(
            log, *typed_edit_result, edit_options);
      }
    }
    std::optional<minera::EmdResult> emd_result;
    if (!emd_reference.empty()) {
      const auto reference = is_xes_path(emd_reference)
          ? minera::import_xes_event_log(emd_reference)
          : minera::import_csv_event_log(emd_reference, options);
      emd_result = minera::evaluate_emd(
          minera::derive_stochastic_language(log),
          minera::derive_stochastic_language(reference), emd_options);
    }
    std::optional<minera::DfgVariantsAnalysis> dfg_variants_analysis;
    if (!dfg_statistical_variants.empty() || !dfg_causal_variants.empty()) {
      dfg_variants_analysis =
          analyze_dfg_variants(log, dfg_statistical_variants,
                               dfg_causal_variants, dfg_statistics_options);
    }
    std::optional<minera::CorrelationVariantsAnalysis>
        correlation_variants_analysis;
    if (!correlation_variants.empty()) {
      if (input_is_xes) {
        correlation_variants_analysis = analyze_correlation_variants(
            minera::EventDataView(log), correlation_variants,
            correlation_options);
      } else {
        const auto correlation_table =
            minera::project_csv_event_table(csv_data->table, log);
        correlation_variants_analysis = analyze_correlation_variants(
            minera::EventDataView(correlation_table), correlation_variants,
            correlation_options);
      }
      minera::register_correlation_activities(
          graph, *correlation_variants_analysis);
    }
    const auto temporal_profile =
        minera::discover_temporal_profile(log, temporal_profile_options);
    const auto minimum_self_distances =
        discover_selected_minimum_self_distances(
            log, minimum_self_distance_options);
    // Selected activity columns can produce labels that are absent from the
    // native activity graph; register them before the JSON adapter resolves
    // minimum-self-distance ids.
    for (const auto& result : minimum_self_distances) {
      if (!result.activity_is_null) graph.ensure_activity(result.activity);
      for (const auto& witness : result.witnesses) {
        graph.ensure_activity(witness);
      }
    }
    const auto transition_system =
        minera::discover_transition_system(log, transition_options);
    for (const auto& transition : transition_system.transitions()) {
      if (transition.label) graph.ensure_activity(*transition.label);
    }
    const auto inductive_log =
        minera::prepare_inductive_event_log(log, inductive_options);
    const auto tree = minera::discover_process_tree(log, inductive_options);
    for (const auto& activity : tree.activities())
      graph.ensure_activity(activity);
    const auto inductive_graph =
        minera::discover_directly_follows_graph(inductive_log);
    std::optional<minera::ProcessTreeAlignmentResult> process_tree_alignment_result;
    if (align_process_tree) {
      process_tree_alignment_result =
          minera::align_process_tree(inductive_log, tree, pt_alignment_options);
    }
    const auto net = minera::discover_petri_net(log, alpha_options);
    const auto alpha_log =
        minera::normalize_alpha_event_log(log, alpha_options);
    register_activity_selector_values(graph, log,
                                      alpha_options.activity_attribute);
    for (const auto& activity : net.activities())
      graph.ensure_activity(activity);
    const auto heuristics =
        minera::discover_heuristics_net(log, heuristic_options);
    const auto powl = minera::discover_powl(log, powl_options);
    register_powl_labels(graph, powl);
    const auto powl_log = project_powl_activity(
        log, powl_options.activity_key, powl_options.timestamp_key,
        powl_options.case_id_key);
    const auto powl_graph =
        minera::discover_directly_follows_graph(powl_log);
    const auto performance = minera::compute_performance_metrics(log, graph);
    const auto variant_durations =
        minera::compute_variant_durations(log, graph);
    const auto resources = minera::discover_resource_network(log);
    const auto eventually = minera::discover_eventually_follows(log, graph);
    const auto log_skeleton =
        minera::discover_log_skeleton(log, log_skeleton_options);
    const auto log_skeleton_conformance = minera::check_log_skeleton_conformance(
        log, log_skeleton, log_skeleton_options);
    const auto declare = minera::discover_declare(log, declare_options);
    minera::DeclareConformanceOptions declare_conformance_options;
    declare_conformance_options.activity_key = declare_options.activity_key;
    const auto declare_conformance = minera::check_declare_conformance(
        log, declare, declare_conformance_options);
    for (const auto& [family, rules] : declare.rules) {
      (void)family;
      for (const auto& rule : rules) {
        graph.ensure_activity(rule.first);
        if (rule.binary) graph.ensure_activity(rule.second);
      }
    }
    for (const auto& result : declare_conformance) {
      for (const auto& deviation : result.deviations) {
        graph.ensure_activity(deviation.first);
        if (deviation.binary) graph.ensure_activity(deviation.second);
      }
    }

    // Check every discovered model both ways, keyed by miner id. The export is
    // offline, so embed the alignments too (the server defers them instead)
    // unless --no-alignments opted out of this standard export section.
    // Evaluations are independent and may still select an alignment-based
    // metric explicitly.
    const auto inductive_net =
        minera::to_petri_net(tree, process_tree_variant);
    const auto heuristic_net = minera::to_petri_net(heuristics);
    const auto powl_net = minera::to_petri_net(powl);
    register_activity_selector_values(
        graph, log, footprints_options.activity_attribute);
    const auto footprints =
        analyze_footprints(log, graph, alpha_log, tree, powl,
                           inductive_net, net,
                           heuristic_net, powl_net, footprints_options);
    if (!no_alignments && log.traces.size() > 1000) {
      std::cerr << "note: computing optimal alignments for every model; on a "
                   "large or badly fitting log this can take minutes "
                   "(--no-alignments skips this export section)\n";
    }
    const auto model = [&](std::string id, const minera::PetriNet& petri,
                           const minera::PnmlDocument* document = nullptr) {
      if (auto gated = minera::tools::pnml_conformance_gate(petri,document,id))
        return *gated;
      const bool is_powl = id == "powl";
      const bool is_inductive = id == "inductive";
      const bool is_alpha = id == "alpha";
      auto replay_options = token_replay_options;
      if (!token_replay_activity_option_used) {
        if (is_inductive)
          replay_options.activity_attribute = inductive_options.activity_key;
        else if (is_alpha)
          replay_options.activity_attribute = alpha_options.activity_attribute;
      }
      if (id == "heuristic" && !token_replay_activity_option_used &&
          replay_options.activity_attribute == "concept:name") {
          replay_options.activity_attribute = heuristic_options.activity_attribute;
      }
      if (is_powl && !token_replay_activity_option_used)
        replay_options.activity_attribute = powl_options.activity_key;
      auto model_alignment_options = alignment_options;
      if (is_powl && !alignment_activity_option_used)
        model_alignment_options.activity_attribute = powl_options.activity_key;
      if (document) {
        replay_options = minera::tools::pnml_replay_options(*document, replay_options);
        model_alignment_options = minera::tools::pnml_alignment_options(
            *document, model_alignment_options);
      }
      std::optional<minera::EventLog> replay_powl_log;
      std::optional<minera::EventLog> alignment_powl_log;
      const minera::EventLog* conformance_log = &log;
      const minera::EventLog* alignment_log = &log;
      if (is_inductive) {
        conformance_log = &inductive_log;
        alignment_log = &inductive_log;
      } else if (is_alpha) {
        conformance_log = &alpha_log;
        alignment_log = &alpha_log;
      } else if (is_powl) {
        replay_powl_log = project_powl_activity(
            log, replay_options.activity_attribute, powl_options.timestamp_key,
            powl_options.case_id_key);
        conformance_log = &*replay_powl_log;
        alignment_powl_log = project_powl_activity(
            log, model_alignment_options.activity_attribute,
            powl_options.timestamp_key, powl_options.case_id_key);
        alignment_log = &*alignment_powl_log;
      }
      replay_options = minera::tools::default_replay_input_options(
          *conformance_log, std::move(replay_options),
          token_replay_activity_option_used);
      return minera::ModelConformance{
          std::move(id), minera::final_marking_reachable(
              petri, model_alignment_options.initial_marking,
              model_alignment_options.final_marking),
           minera::check_conformance(*conformance_log, petri, replay_options),
           is_powl ? minera::check_footprints(powl_log, petri)
           : is_inductive ? minera::check_footprints(inductive_log, petri)
           : is_alpha ? minera::check_footprints(alpha_log, petri)
                      : minera::check_footprints(log, petri, replay_options.initial_marking),
           no_alignments ? std::optional<minera::AlignmentResult>{}
                         : std::optional<minera::AlignmentResult>(
                               (is_powl || is_inductive || is_alpha ||
                                alignment_option_used || document)
                                   ? minera::check_alignment(
                                         *alignment_log, petri,
                                         model_alignment_options)
                                   : minera::check_alignment(log, petri))};
    };
    std::vector<minera::ModelConformance> conformance = {
        model("inductive", inductive_net), model("alpha", net),
        model("heuristic", heuristic_net), model("powl", powl_net)};
    std::vector<std::pair<std::string, const minera::PetriNet*>>
        available_models = {{"inductive", &inductive_net},
                            {"alpha", &net},
                            {"heuristic", &heuristic_net},
                            {"powl", &powl_net}};

    // Optionally check the log against a user-supplied model too — a PNML
    // marked net, a BPMN 2.0 process, or a PTML process tree, all landing
    // under the single "imported" conformance id. Its activities are
    // registered in the id registry first, so the ones the log never shows
    // still resolve to names in the export.
    const int check_flags = (check_pnml.empty() ? 0 : 1) +
                            (check_bpmn.empty() ? 0 : 1) +
                            (check_ptml.empty() ? 0 : 1);
    std::optional<minera::PetriNet> imported_net;
    std::optional<minera::PnmlDocument> imported_document;
    // Explicit nonempty caller vectors, including all-zero markings, win.
    const auto apply_imported_markings = [&](auto& options) {
      if (!imported_document) return;
      using Integer = typename std::decay_t<decltype(options.initial_marking)>::value_type;
      if (options.initial_marking.empty())
        options.initial_marking = minera::tools::pnml_marking<Integer>(
            imported_document->initial_marking);
      if (options.final_marking.empty())
        options.final_marking = minera::tools::pnml_marking<Integer>(
            imported_document->final_marking);
    };
    const auto apply_imported_monte_carlo_context = [&](minera::MonteCarloOptions& options) {
      apply_imported_markings(options);
      if (!imported_document) return;
      options.place_names = imported_document->place_ids;
      options.transition_names = imported_document->transition_ids;
      options.initial_marking_keys = monte_carlo_fields.contains("monte_carlo_initial_marking")
          ? std::vector<bool>{} : imported_document->initial_marking_keys;
      options.final_marking_keys = monte_carlo_fields.contains("monte_carlo_final_marking")
          ? std::vector<bool>{} : imported_document->final_marking_keys;
    };
    if (check_flags > 1) {
      std::cerr << "error: pass only one of --check-pnml, --check-bpmn, and "
                   "--check-ptml (they share the conformance id "
                   "\"imported\")\n";
      return 2;
    }
    if (check_flags == 1) {
      if (!check_pnml.empty()) {
        imported_document = minera::tools::import_pnml_model(check_pnml, pnml_public_markings);
        imported_net = imported_document->net;
      } else {
        imported_net = !check_bpmn.empty() ? minera::import_bpmn(check_bpmn)
            : minera::to_petri_net(minera::import_ptml(check_ptml));
      }
      for (const auto& activity : imported_net->activities()) {
        graph.ensure_activity(activity);
      }
      conformance.push_back(model("imported", *imported_net,
          imported_document ? &*imported_document : nullptr));
      available_models.emplace_back("imported", &*imported_net);
    }
    if (stochastic_map_requested) {
      std::optional<minera::PetriNet> stochastic_map_net;
      minera::StochasticMap stochastic_map;
      std::size_t replayed = 0;
      std::size_t skipped = 0;
      std::string converted_net_json;
      if (stochastic_map_params.source ==
          minera::StochasticMapSource::kPerformanceDfg) {
        const auto performance_dfg = minera::discover_performance_dfg(log);
        const auto model = minera::stochastic_map_from_performance_dfg(
            graph, performance_dfg, stochastic_map_params.performance);
        stochastic_map_net = model.net;
        stochastic_map = model.map;
        converted_net_json = minera::serialize_dfg_petri_net_json(
            model.net, stochastic_map_params.performance.conversion.variant);
      } else {
        const minera::PetriNet* selected = nullptr;
        if (stochastic_map_miner == "inductive") selected = &inductive_net;
        else if (stochastic_map_miner == "alpha") selected = &net;
        else if (stochastic_map_miner == "heuristic") selected = &heuristic_net;
        else if (stochastic_map_miner == "powl") selected = &powl_net;
        else if (stochastic_map_miner == "imported" && imported_net)
          selected = &*imported_net;
        else {
          std::cerr << "error: unknown --stochastic-map-miner '"
                    << stochastic_map_miner
                    << "' (expected inductive, alpha, heuristic, powl, or a supplied imported model)\n";
          return 2;
        }
        const auto& fit_log =
            stochastic_map_miner == "powl"
                ? powl_log
                : stochastic_map_miner == "inductive"
                      ? inductive_log
                      : stochastic_map_miner == "alpha" ? alpha_log : log;
        stochastic_map_params.fit.replay = minera::tools::stochastic_map_replay_options(
            stochastic_map_params.fit.replay, stochastic_map_params.initial_marking,
            stochastic_map_miner == "imported" && imported_document
                ? &*imported_document : nullptr);
        const auto report = minera::fit_stochastic_map_with_report(
            fit_log, *selected, stochastic_map_params.fit);
        stochastic_map_net = *selected;
        stochastic_map = report.map;
        replayed = report.replayed_traces;
        skipped = report.skipped_traces;
      }
      if (stochastic_map_params.source == minera::StochasticMapSource::kLog &&
          stochastic_map_miner == "imported" && imported_document &&
          stochastic_map_params.initial_marking.empty())
        stochastic_map_params.initial_marking = minera::tools::pnml_marking<std::uint32_t>(
            imported_document->initial_marking);
      const auto reachability = minera::tools::stochastic_map_reachability(
          *stochastic_map_net, stochastic_map, stochastic_map_params.fit,
          stochastic_map_params.initial_marking, stochastic_map_params.max_states);
      std::ostringstream stochastic_map_json;
      stochastic_map_json
          << "{\"family\":\"STOCHASTIC_MAP\",\"source\":\""
          << (stochastic_map_params.source ==
                      minera::StochasticMapSource::kLog
                  ? "log"
                  : "performance_dfg")
          << "\",\"map\":"
          << minera::serialize_stochastic_map_json(stochastic_map);
      if (!converted_net_json.empty())
        stochastic_map_json << ",\"net\":" << converted_net_json;
      if (stochastic_map_params.source == minera::StochasticMapSource::kLog)
        stochastic_map_json << ",\"diagnostics\":{\"replayed_traces\":"
                            << replayed << ",\"skipped_traces\":" << skipped
                            << '}';
      stochastic_map_json << ",\"reachability\":"
                          << minera::serialize_stochastic_petri_json(
                                 reachability)
                          << '}';
      std::ofstream file(stochastic_map_output);
      if (!file) {
        std::cerr << "error: cannot open stochastic-map output file "
                  << stochastic_map_output << '\n';
        return 1;
      }
      file << stochastic_map_json.str() << '\n';
    }
    if (!compare_petri_reference.empty()) {
      if (!ends_with_ci(compare_petri_reference.string(), ".csv") &&
          !is_xes_path(compare_petri_reference)) {
        std::cerr << "error: --compare-petri-reference expects a .csv or .xes file\n";
        return 2;
      }
      const auto model_it = std::find_if(
          available_models.begin(), available_models.end(),
          [&](const auto& item) { return item.first == compare_petri_miner; });
      if (model_it == available_models.end()) {
        std::cerr << "error: unknown --compare-petri-miner '"
                  << compare_petri_miner << "'\n";
        return 2;
      }
      auto selector = compare_petri_activity_attribute.empty() &&
                              compare_petri_miner == "powl"
                          ? powl_options.activity_key
                          : compare_petri_activity_attribute;
      if (selector.empty() && compare_petri_miner == "alpha")
        selector = alpha_options.activity_attribute;
      auto reference = [&]() {
        if (is_xes_path(compare_petri_reference)) {
          minera::XesImportOptions reference_options;
          if (!selector.empty() && selector != "concept:name")
            reference_options.activity_attribute = selector;
          return minera::import_xes_event_log(compare_petri_reference,
                                              reference_options);
        }
        auto reference_options = options;
        reference_options.allow_case_id_aliases = true;
        if (!selector.empty() && selector != "activity")
          reference_options.activity_column = selector;
        return minera::import_csv_event_log(compare_petri_reference,
                                            reference_options);
      }();
      minera::PetriUsageComparisonOptions comparison_options;
      comparison_options.activity_attribute = selector;
      comparison_options.walk_through_hidden_trans =
          compare_petri_walk_through_hidden;
      comparison_options.try_to_reach_final_marking_through_hidden =
          compare_petri_reach_final_through_hidden;
      comparison_options.stop_immediately_unfit =
          compare_petri_stop_immediately_unfit;
      comparison_options.cleaning_token_flood = compare_petri_cleaning_token_flood;
      comparison_options.exhaustive_invisible_exploration = compare_petri_exhaustive_invisible_exploration;
      if (compare_petri_miner == "imported" && imported_document)
        comparison_options = minera::tools::pnml_comparison_options(
            *imported_document, comparison_options);
      if (compare_petri_initial_marking_used) {
        comparison_options.initial_marking = compare_petri_initial_marking;
        comparison_options.initial_marking_keys.clear();
      }
      if (compare_petri_final_marking_used) {
        comparison_options.final_marking = compare_petri_final_marking;
        comparison_options.final_marking_keys.clear();
      }
      std::optional<minera::EventLog> normalized_comparison_log;
      const minera::EventLog* comparison_log = &log;
      if (compare_petri_miner == "powl") {
        normalized_comparison_log = project_powl_activity(
            log, selector, powl_options.timestamp_key,
            powl_options.case_id_key);
        comparison_log = &*normalized_comparison_log;
        reference = project_powl_activity(
            reference, selector, powl_options.timestamp_key,
            powl_options.case_id_key);
      } else if (compare_petri_miner == "inductive") {
        normalized_comparison_log =
            minera::prepare_inductive_event_log(reference, inductive_options);
        comparison_log = &inductive_log;
        reference = std::move(*normalized_comparison_log);
      } else if (compare_petri_miner == "alpha") {
        auto alpha_projection_options = alpha_options;
        alpha_projection_options.activity_attribute = selector;
        normalized_comparison_log =
            minera::normalize_alpha_event_log(log, alpha_projection_options);
        comparison_log = &*normalized_comparison_log;
        reference = minera::normalize_alpha_event_log(
            reference, alpha_projection_options);
      }
      const auto comparison = minera::compare_petri_usage(
          *model_it->second, *comparison_log, reference, comparison_options);
      const auto body = minera::petri_usage_comparison_json(comparison);
      if (output.empty()) {
        std::cout << body << '\n';
      } else {
        std::ofstream file(output);
        if (!file) {
          std::cerr << "error: cannot open output file " << output << '\n';
          return 1;
        }
        file << body << '\n';
      }
      return 0;
    }
    if (petri_analysis_params) {
      const auto& request = *petri_analysis_params;
      const auto model_it = std::find_if(
          available_models.begin(), available_models.end(),
          [&](const auto& item) { return item.first == request.miner; });
      if (model_it == available_models.end()) {
        std::cerr << "error: unknown --petri-analysis-miner '"
                  << request.miner << "'\n";
        return 2;
      }
      std::string result_json;
      switch (request.family) {
        case minera::PetriAnalysisFamily::kMarkingEquation: {
          minera::MarkingEquationOptions options;
          options.initial_marking = request.initial_marking;
          options.final_marking = request.final_marking;
          if (request.miner == "imported") apply_imported_markings(options);
          options.transition_costs = request.transition_costs;
          options.realizability_limit = request.max_states;
          result_json = minera::petri_analysis_json(
              minera::solve_marking_equation(*model_it->second, options));
          break;
        }
        case minera::PetriAnalysisFamily::kExtendedMarkingEquation: {
          const auto& analysis_log =
              request.miner == "powl"
                  ? powl_log
                  : request.miner == "inductive"
                        ? inductive_log
                        : request.miner == "alpha" ? alpha_log : log;
          if (request.trace_index >= analysis_log.traces.size()) {
            std::cerr << "error: --petri-analysis-trace-index out of range\n";
            return 2;
          }
          minera::ExtendedMarkingEquationOptions options;
          options.initial_marking = request.initial_marking;
          options.final_marking = request.final_marking;
          if (request.miner == "imported") apply_imported_markings(options);
          options.model_transition_costs = request.model_transition_costs;
          options.synchronous_transition_costs =
              request.synchronous_transition_costs;
          options.log_move_costs = request.log_move_costs;
          options.split_indices = request.split_indices;
          options.max_k_value = request.max_k_value;
          options.full_bootstrap_required = request.full_bootstrap_required;
          options.realizability_limit = request.max_states;
          std::vector<std::string> trace;
          for (const auto& event : analysis_log.traces[request.trace_index].events)
            trace.push_back(event.activity);
          result_json = minera::petri_analysis_json(
              minera::solve_extended_marking_equation(
                  trace, *model_it->second, options));
          break;
        }
        case minera::PetriAnalysisFamily::kWorkflowNet:
          result_json = minera::petri_analysis_json(
              minera::check_workflow_net(*model_it->second));
          break;
        case minera::PetriAnalysisFamily::kSoundness:
        case minera::PetriAnalysisFamily::kWoflan: {
          minera::SoundnessOptions options;
          options.variant = request.family == minera::PetriAnalysisFamily::kWoflan
                                ? minera::SoundnessVariant::kWoflan
                                : minera::SoundnessVariant::kSoundness;
          options.initial_marking = request.initial_marking;
          options.final_marking = request.final_marking;
          if (request.miner == "imported") apply_imported_markings(options);
          options.max_states = request.max_states;
          options.invariant_search_limit = request.invariant_search_limit;
          options.diagnostics = request.diagnostics;
          options.return_asap_when_unsound = request.return_asap_when_unsound;
          result_json = minera::petri_analysis_json(
              minera::analyze_soundness(*model_it->second, options));
          break;
        }
      }
      if (output.empty()) {
        std::cout << result_json << '\n';
      } else {
        std::ofstream file(output);
        if (!file) {
          std::cerr << "error: cannot open output file " << output << '\n';
          return 1;
        }
        file << result_json << '\n';
      }
      return 0;
    }
    std::optional<std::vector<minera::ModelEvaluation>> evaluation;
    if (evaluation_params) {
      evaluation.emplace();
      evaluation->reserve(available_models.size());
      for (const auto& [miner, petri] : available_models) {
        const auto& evaluation_log =
            miner == "powl"
                ? powl_log
                : miner == "inductive"
                      ? inductive_log
                      : miner == "alpha" ? alpha_log : log;
        minera::ModelEvaluation result;
        result.miner = miner;
        auto model_evaluation = *evaluation_params;
        const auto* evaluation_document = miner == "imported" && imported_document
            ? &*imported_document : nullptr;
        switch (model_evaluation.family) {
          case minera::EvaluationFamily::kReplayFitness:
            result.replay_fitness = evaluation_document
                ? minera::evaluate_replay_fitness(
                      evaluation_log, *evaluation_document,
                      model_evaluation.replay_fitness_variant,
                      model_evaluation.replay_fitness_options)
                : minera::evaluate_replay_fitness(
                evaluation_log, *petri,
                model_evaluation.replay_fitness_variant,
                model_evaluation.replay_fitness_options);
            break;
          case minera::EvaluationFamily::kPrecision:
            if (evaluation_document && model_evaluation.precision_variant !=
                minera::PrecisionVariant::ETCONFORMANCE_TOKEN)
              apply_imported_markings(model_evaluation.precision_options);
            result.precision = evaluation_document && model_evaluation.precision_variant ==
                minera::PrecisionVariant::ETCONFORMANCE_TOKEN
                ? minera::evaluate_precision(
                      evaluation_log, *evaluation_document, model_evaluation.precision_variant,
                      model_evaluation.precision_options)
                : minera::evaluate_precision(
                evaluation_log, *petri, model_evaluation.precision_variant,
                model_evaluation.precision_options);
            break;
          case minera::EvaluationFamily::kGeneralization:
            result.generalization = evaluation_document
                ? minera::evaluate_generalization(
                      evaluation_log, *evaluation_document,
                      model_evaluation.generalization_variant,
                      model_evaluation.generalization_options)
                : minera::evaluate_generalization(
                evaluation_log, *petri,
                model_evaluation.generalization_variant,
                model_evaluation.generalization_options);
            break;
          case minera::EvaluationFamily::kSimplicity:
            result.simplicity = miner == "imported" && imported_document
                ? minera::evaluate_simplicity(
                      *imported_document, model_evaluation.simplicity_variant,
                      model_evaluation.simplicity_options)
                : minera::evaluate_simplicity(
                      *petri, model_evaluation.simplicity_variant,
                      model_evaluation.simplicity_options);
            break;
          case minera::EvaluationFamily::kComposite:
            result.composite = miner == "imported" && imported_document
                ? minera::evaluate_composite(
                      evaluation_log, *imported_document, model_evaluation.composite_options)
                : minera::evaluate_composite(
                      evaluation_log, *petri, model_evaluation.composite_options);
            break;
        }
        evaluation->push_back(std::move(result));
      }
    }
    const auto dataset = input.filename().string();

    // Optionally mine the decision points of one model's net.
    if ((decisions_miner == "powl" || decision_tables_miner == "powl") &&
        !decision_activity_option_used) {
      decision_options.activity_attribute = powl_options.activity_key;
    }
    std::optional<minera::DecisionMiningResult> decisions;
    if (!decisions_miner.empty()) {
      const auto* selected = select_decision_model(
          decisions_miner, inductive_net, net, heuristic_net, powl_net);
      if (selected == nullptr) {
        std::cerr << "error: unknown --decisions miner '" << decisions_miner
                  << "' (expected inductive, alpha, heuristic, or powl)\n";
        return 2;
      }
      std::optional<minera::EventLog> projected_decision_log;
      const minera::EventLog* decision_log = &log;
      if (decisions_miner == "powl") {
        projected_decision_log = project_powl_activity(
            log, decision_options.activity_attribute,
            powl_options.timestamp_key, powl_options.case_id_key);
        decision_log = &*projected_decision_log;
      } else if (decisions_miner == "inductive") {
        decision_log = &inductive_log;
      } else if (decisions_miner == "alpha") {
        decision_log = &alpha_log;
      }
      decisions = minera::mine_decisions(*decision_log, *selected,
                                         decision_options);
      decisions->miner = decisions_miner;
    }
    const minera::DecisionMiningResult* decisions_ptr =
        decisions ? &*decisions : nullptr;
    std::optional<minera::DecisionTablesResult> decision_tables;
    if (!decision_tables_miner.empty()) {
      const auto* selected = select_decision_model(
          decision_tables_miner, inductive_net, net, heuristic_net, powl_net);
      if (selected == nullptr) {
        std::cerr << "error: unknown --decision-tables miner '"
                  << decision_tables_miner
                  << "' (expected inductive, alpha, heuristic, or powl)\n";
        return 2;
      }
      minera::DecisionTableOptions table_options;
      table_options.mining = decision_options;
      table_options.include_trace_attributes =
          decision_options.include_trace_attributes;
      table_options.attributes = decision_options.attributes;
      table_options.lookback = decision_lookback;
      std::optional<minera::EventLog> projected_decision_log;
      const minera::EventLog* decision_log = &log;
      if (decision_tables_miner == "powl") {
        projected_decision_log = project_powl_activity(
            log, decision_options.activity_attribute,
            powl_options.timestamp_key, powl_options.case_id_key);
        decision_log = &*projected_decision_log;
      } else if (decision_tables_miner == "inductive") {
        decision_log = &inductive_log;
      } else if (decision_tables_miner == "alpha") {
        decision_log = &alpha_log;
      }
      decision_tables =
          minera::mine_decision_tables(*decision_log, *selected, table_options);
    }
    const auto* decision_tables_ptr =
        decision_tables ? &*decision_tables : nullptr;

    // Frequency/performance annotations for every miner's Petri-net view —
    // the inductive net fits by construction, so its overlay always has data
    // even when the alpha net (the classic view) fits nothing of a real log.
    const auto petri_decoration = minera::decorate_model(alpha_log, net);
    const auto inductive_decoration =
        minera::decorate_model(inductive_log, inductive_net);
    const auto heuristic_decoration = minera::decorate_model(
        log, heuristic_net, heuristic_options.activity_attribute,
        heuristic_options.timestamp_attribute);
    const auto powl_decoration = minera::decorate_model(
        powl_log, powl_net, powl_options.activity_key,
        powl_options.timestamp_key);

    // Optionally compute the performance spectrum over an activity pattern.
    std::optional<minera::PerformanceSpectrum> spectrum;
    if (!spectrum_activities.empty()) {
      spectrum = discover_selected_performance_spectrum(
          log, spectrum_activities, spectrum_options);
    }
    const minera::PerformanceSpectrum* spectrum_ptr =
        spectrum ? &*spectrum : nullptr;
    std::optional<minera::BatchDiscovery> batch_discovery;
    if (batches) {
      batch_discovery = discover_selected_batches(log, batch_options);
    }
    const minera::BatchDiscovery* batch_ptr =
        batch_discovery ? &*batch_discovery : nullptr;

    std::optional<minera::DecomposedAlignmentResult> decomposed_result;
    std::optional<minera::PetriAntiAlignmentResult> petri_anti_result;
    std::optional<minera::PetriMultiAlignmentResult> petri_multi_result;
    std::optional<minera::ProcessTree> reduced_tree_result;
    const minera::PetriNet* advanced_net = &inductive_net;
    if (advanced_petri_miner == "alpha") advanced_net = &net;
    else if (advanced_petri_miner == "heuristic") advanced_net = &heuristic_net;
    else if (advanced_petri_miner == "powl") advanced_net = &powl_net;
    else if (advanced_petri_miner == "imported") {
      if (!imported_net) throw std::invalid_argument(
          "--advanced-petri-miner imported requires a supplied model");
      advanced_net = &*imported_net;
      apply_imported_markings(decomposed_options);
      apply_imported_markings(petri_anti_options);
      apply_imported_markings(petri_multi_options);
    }
    const minera::EventLog* advanced_log =
        advanced_petri_miner == "powl"
            ? &powl_log
            : advanced_petri_miner == "inductive"
                  ? &inductive_log
                  : advanced_petri_miner == "alpha" ? &alpha_log : &log;
    std::optional<minera::EventLog> decomposed_powl_log;
    std::optional<minera::EventLog> petri_powl_log;
    const minera::EventLog* decomposed_alignment_log = advanced_log;
    const minera::EventLog* petri_alignment_log = advanced_log;
    if (advanced_petri_miner == "powl") {
      if (align_decomposed && decomposed_activity_option_used) {
        decomposed_powl_log = project_powl_activity(
            log, decomposed_options.activity_attribute,
            powl_options.timestamp_key, powl_options.case_id_key);
        decomposed_alignment_log = &*decomposed_powl_log;
      }
      if ((align_petri_anti || align_petri_multi) &&
          petri_activity_option_used) {
        petri_powl_log = project_powl_activity(
            log, petri_anti_options.activity_attribute,
            powl_options.timestamp_key, powl_options.case_id_key);
        petri_alignment_log = &*petri_powl_log;
      }
    }
    if (align_decomposed)
      decomposed_result =
          minera::align_decomposed(*decomposed_alignment_log, *advanced_net,
                                   decomposed_options);
    if (align_petri_anti)
      petri_anti_result = minera::anti_align_petri(
          *advanced_net, *petri_alignment_log, petri_anti_options);
    if (align_petri_multi)
      petri_multi_result = minera::multi_align_petri(
          *advanced_net, *petri_alignment_log, petri_multi_options);
    if (reduce_trace_index) {
      if (*reduce_trace_index >= inductive_log.traces.size()) { std::cerr << "error: --reduce-process-tree trace index out of range\n"; return 2; }
      minera::ProcessTreeReductionOptions ro; ro.activity_key = reduction_activity_key;
      reduced_tree_result = minera::reduce_process_tree(tree, inductive_log.traces[*reduce_trace_index], ro);
    }

    if (monte_carlo_json_requested) {
      const minera::PetriNet* selected = &inductive_net;
      if (playout_miner == "alpha") selected = &net;
      else if (playout_miner == "heuristic") selected = &heuristic_net;
      else if (playout_miner == "powl") selected = &powl_net;
      else if (playout_miner == "imported" && imported_net) selected = &*imported_net;
      else if (playout_miner != "inductive" &&
               playout_miner != "monte_carlo") {
        std::cerr << "error: unknown Monte Carlo model '" << playout_miner
                  << "' (expected inductive, alpha, heuristic, powl, or imported)\n";
        return 2;
      }
      auto effective_options = monte_carlo_options;
      if (playout_miner == "imported") apply_imported_monte_carlo_context(effective_options);
      std::optional<minera::EventLog> projected_monte_carlo_log;
      const minera::EventLog* simulation_log = &log;
      if (playout_miner == "powl") {
        if (!monte_carlo_fields.contains("monte_carlo_activity_key"))
          effective_options.activity_key = powl_options.activity_key;
        if (!monte_carlo_fields.contains("monte_carlo_timestamp_key"))
          effective_options.timestamp_key = powl_options.timestamp_key;
        projected_monte_carlo_log = project_powl_activity(
            log, effective_options.activity_key,
            effective_options.timestamp_key, powl_options.case_id_key);
        simulation_log = &*projected_monte_carlo_log;
      } else if (playout_miner == "inductive" ||
                 playout_miner == "monte_carlo") {
        simulation_log = &inductive_log;
      } else if (playout_miner == "alpha") {
        simulation_log = &alpha_log;
      }
      const auto body = minera::monte_carlo_json(
          minera::monte_carlo(*simulation_log, *selected, effective_options));
      if (output.empty()) {
        std::cout << body << '\n';
      } else {
        std::ofstream file(output);
        if (!file) {
          std::cerr << "error: cannot open output file " << output << '\n';
          return 1;
        }
        file << body << '\n';
      }
      return 0;
    }

    if (output.empty()) {
      minera::write_analysis_json(
          std::cout, dataset, log, graph, &tree, &net, &heuristics,
          &conformance, &performance, &resources, &variant_durations,
          &eventually, decisions_ptr, &petri_decoration, spectrum_ptr,
          &inductive_net, &inductive_decoration, &heuristic_net,
          &heuristic_decoration, &temporal_profile, &minimum_self_distances,
          &transition_system, &powl, &powl_net, &powl_decoration,
          &log_skeleton, &log_skeleton_conformance, &declare,
          &declare_conformance, &footprints,
          dfg_variants_analysis ? &*dfg_variants_analysis : nullptr,
          correlation_variants_analysis ? &*correlation_variants_analysis
                                        : nullptr,
          batch_ptr,
          dfg_alignment_result ? &*dfg_alignment_result : nullptr,
          nullptr, edit_anti,
          dfg_projection_result ? &*dfg_projection_result : nullptr,
          dfg_projected_log ? &*dfg_projected_log : nullptr,
          edit_projected_log ? &*edit_projected_log : nullptr,
          process_tree_alignment_result ? &*process_tree_alignment_result : nullptr,
          decomposed_result ? &*decomposed_result : nullptr,
          petri_anti_result ? &*petri_anti_result : nullptr,
          petri_multi_result ? &*petri_multi_result : nullptr,
          reduced_tree_result ? &*reduced_tree_result : nullptr,
          reduce_trace_index.value_or(0),
          evaluation ? &*evaluation : nullptr,
          emd_result ? &*emd_result : nullptr,
          overlap_interval_events ? &*overlap_interval_events : nullptr,
          overlap_cases ? &*overlap_cases : nullptr,
          overlap_concurrent_activities ? &*overlap_concurrent_activities
                                        : nullptr,
          decision_tables_ptr, minimum_self_distance_options.variant,
          typed_edit_result ? &*typed_edit_result : nullptr);
    } else {
      std::ofstream file(output);
      if (!file) {
        std::cerr << "error: cannot open output file " << output << '\n';
        return 1;
      }
      minera::write_analysis_json(
          file, dataset, log, graph, &tree, &net, &heuristics, &conformance,
          &performance, &resources, &variant_durations, &eventually,
          decisions_ptr, &petri_decoration, spectrum_ptr, &inductive_net,
          &inductive_decoration, &heuristic_net, &heuristic_decoration,
          &temporal_profile, &minimum_self_distances, &transition_system,
          &powl, &powl_net, &powl_decoration, &log_skeleton,
          &log_skeleton_conformance, &declare, &declare_conformance,
          &footprints,
          dfg_variants_analysis ? &*dfg_variants_analysis : nullptr,
          correlation_variants_analysis ? &*correlation_variants_analysis
                                        : nullptr,
          batch_ptr,
          dfg_alignment_result ? &*dfg_alignment_result : nullptr,
          nullptr, edit_anti,
          dfg_projection_result ? &*dfg_projection_result : nullptr,
          dfg_projected_log ? &*dfg_projected_log : nullptr,
          edit_projected_log ? &*edit_projected_log : nullptr,
          process_tree_alignment_result ? &*process_tree_alignment_result : nullptr,
          decomposed_result ? &*decomposed_result : nullptr,
          petri_anti_result ? &*petri_anti_result : nullptr,
          petri_multi_result ? &*petri_multi_result : nullptr,
          reduced_tree_result ? &*reduced_tree_result : nullptr,
          reduce_trace_index.value_or(0),
          evaluation ? &*evaluation : nullptr,
          emd_result ? &*emd_result : nullptr,
          overlap_interval_events ? &*overlap_interval_events : nullptr,
          overlap_cases ? &*overlap_cases : nullptr,
          overlap_concurrent_activities ? &*overlap_concurrent_activities
                                        : nullptr,
          decision_tables_ptr, minimum_self_distance_options.variant,
          typed_edit_result ? &*typed_edit_result : nullptr);
    }

    // Optionally emit one discovered model as standard PNML.
    if (!pnml_output.empty()) {
      const minera::PetriNet* selected = nullptr;
      std::optional<minera::PetriNet> converted_process_tree;
      if (pnml_miner == "inductive") {
        if (process_tree_variant ==
            minera::ProcessTreePetriNetVariant::kStandard) {
          selected = &inductive_net;
        } else {
          converted_process_tree =
              minera::to_petri_net(tree, process_tree_variant);
          selected = &*converted_process_tree;
        }
      } else if (pnml_miner == "alpha") {
        selected = &net;
      } else if (pnml_miner == "heuristic") {
        selected = &heuristic_net;
      } else if (pnml_miner == "powl") {
        selected = &powl_net;
      } else if (pnml_miner == "imported" && imported_net) {
        selected = &*imported_net;
      } else {
        std::cerr << "error: unknown --pnml-miner '" << pnml_miner
                  << "' (expected inductive, alpha, heuristic, powl, or imported)\n";
        return 1;
      }
      std::ofstream pnml_file(pnml_output);
      if (!pnml_file) {
        std::cerr << "error: cannot open PNML output file " << pnml_output
                  << '\n';
        return 1;
      }
      const std::string pnml_name =
          process_tree_variant ==
                  minera::ProcessTreePetriNetVariant::kStandard
              ? pnml_miner + " net"
              : pnml_miner + " net (transition_bordered)";
      if (pnml_miner == "imported" && imported_document)
        minera::write_pnml(pnml_file, *imported_document, pnml_name);
      else
        minera::write_pnml(pnml_file, *selected, pnml_name);
    }

    // Optionally emit one discovered model as BPMN. The inductive tree maps
    // block by block; the other miners' workflow nets convert by their token
    // semantics.
    if (!bpmn_output.empty()) {
      std::ofstream bpmn_file(bpmn_output);
      if (!bpmn_file) {
        std::cerr << "error: cannot open BPMN output file " << bpmn_output
                  << '\n';
        return 1;
      }
      if (bpmn_miner == "inductive") {
        if (process_tree_variant ==
            minera::ProcessTreePetriNetVariant::kStandard) {
          minera::write_bpmn(bpmn_file, tree, dataset);
        } else {
          const auto converted =
              minera::to_petri_net(tree, process_tree_variant);
          minera::write_bpmn(bpmn_file, converted, dataset);
        }
      } else if (bpmn_miner == "alpha") {
        minera::write_bpmn(bpmn_file, net, dataset);
      } else if (bpmn_miner == "heuristic") {
        minera::write_bpmn(bpmn_file, heuristic_net, dataset);
      } else if (bpmn_miner == "powl") {
        minera::write_bpmn(bpmn_file, powl_net, dataset);
      } else {
        std::cerr << "error: unknown --bpmn-miner '" << bpmn_miner
                  << "' (expected inductive, alpha, heuristic, or powl)\n";
        return 1;
      }
    }

    // Optionally emit the inductive process tree as PTML.
    if (!ptml_output.empty()) {
      std::ofstream ptml_file(ptml_output);
      if (!ptml_file) {
        std::cerr << "error: cannot open PTML output file " << ptml_output
                  << '\n';
        return 1;
      }
      minera::write_ptml(ptml_file, tree, dataset);
    }

    if (!powl_ptml_output.empty()) {
      std::ofstream ptml_file(powl_ptml_output);
      if (!ptml_file) {
        std::cerr << "error: cannot open POWL PTML output file "
                  << powl_ptml_output << '\n';
        return 1;
      }
      // Keep CLI semantics aligned with the server: POWL is first materialized
      // as its workflow net, then reduced only when the structure is exactly
      // block structured. Non-block partial orders fail instead of being
      // silently approximated.
      minera::write_ptml(ptml_file, minera::to_process_tree(powl_net), dataset);
    }

    // Optionally emit the (filtered) event log as standard XES, making the
    // CLI double as a CSV-to-XES converter.
    if (!xes_output.empty()) {
      xes_export_options.name = dataset;
      (void)minera::export_xes_event_log(xes_output, log,
                                         xes_export_options);
    }

    if (!dfg_output.empty()) {
      minera::export_dfg(dfg_output, minera::to_dfg_analysis(graph));
    }

    if (!playout_output.empty()) {
      std::optional<minera::EventLog> generated;
      std::optional<std::string> generated_json;
      const minera::PetriNet* selected = nullptr;
      const bool stochastic = stochastic_playout_requested || playout_miner == "stochastic";
      const bool monte_carlo_mode =
          monte_carlo_requested || playout_miner == "monte_carlo";
      const bool monte_carlo_uses_inductive_default =
          monte_carlo_mode && playout_miner == "monte_carlo";
      const std::string model_miner =
          (playout_miner == "stochastic" ||
           monte_carlo_uses_inductive_default)
              ? "inductive"
              : playout_miner;
      if (model_miner == "inductive")
        selected = &inductive_net;
      else if (model_miner == "alpha")
        selected = &net;
      else if (model_miner == "heuristic")
        selected = &heuristic_net;
      else if (model_miner == "powl")
        selected = &powl_net;
      else if (model_miner == "imported" && imported_net) {
        selected = &*imported_net;
        apply_imported_markings(basic_playout_options);
        apply_imported_markings(extensive_playout_options);
        apply_imported_markings(stochastic_playout_options);
        apply_imported_monte_carlo_context(monte_carlo_options);
        if (imported_document && stochastic_petri_options.initial_marking.empty())
          stochastic_petri_options.initial_marking =
              minera::tools::pnml_marking<std::uint32_t>(imported_document->initial_marking);
      }
      else if (model_miner == "declare")
        generated =
            minera::declare_playout(declare, declare_playout_options);
      else if (model_miner == "process_tree") {
        const auto result = minera::process_tree_playout_result(
            tree, process_tree_playout_options);
        if (process_tree_playout_options.return_set_strings)
          generated_json = minera::serialize_process_tree_playout_json(result);
        else
          generated = result.log;
      }
      else if (model_miner == "dfg") {
        const auto result = generate_dfg_playout(log, dfg_playout_options);
        if (dfg_playout_options.return_variants &&
            dfg_playout_options.variant == minera::DfgPlayoutVariant::kClassic)
          generated_json = minera::dfg_playout_json(result);
        else
          generated = result.log;
      }
      else {
        std::cerr << "error: unknown --playout-miner '" << playout_miner
                  << "' (expected inductive, alpha, heuristic, powl, imported, "
                     "declare, process_tree, or dfg)\n";
        return 1;
      }
      std::optional<minera::EventLog> projected_playout_log;
      const minera::EventLog* fit_log = &log;
      if (model_miner == "inductive") {
        fit_log = &inductive_log;
      } else if (model_miner == "powl" && stochastic) {
        projected_playout_log = project_powl_activity(
            log, powl_options.activity_key, powl_options.timestamp_key,
            powl_options.case_id_key);
        fit_log = &*projected_playout_log;
      } else if (model_miner == "alpha") {
        fit_log = &alpha_log;
      }
      if (selected != nullptr && petri_playout_mode == "extensive") {
        if (basic_playout_fields.contains("traces"))
          extensive_playout_options.max_traces = basic_playout_options.traces;
        if (basic_playout_fields.contains("initial_marking"))
          extensive_playout_options.initial_marking =
              basic_playout_options.initial_marking;
        if (basic_playout_fields.contains("final_marking"))
          extensive_playout_options.final_marking =
              basic_playout_options.final_marking;
        if (extensive_playout_options.return_elements) {
          generated_json = minera::serialize_extensive_playout_visited_json(
              *selected,
              minera::extensive_playout_visited(
                  *selected, extensive_playout_options));
        } else {
          generated = minera::extensive_playout(
              *selected, extensive_playout_options);
        }
      }
      if (selected != nullptr && stochastic_petri_requested) {
        const auto reachability = minera::stochastic_petri_reachability(
            *selected, stochastic_petri_options);
        if (stochastic_petri_options.analysis ==
            minera::StochasticPetriAnalysisVariant::kReachability) {
          generated_json = minera::serialize_stochastic_petri_json(reachability);
        } else if (stochastic_petri_options.analysis ==
                   minera::StochasticPetriAnalysisVariant::kTransient) {
          const auto initial_state =
              stochastic_petri_options.analysis_initial_state.has_value()
                  ? stochastic_petri_options.analysis_initial_state
                  : reachability.initial_state;
          const auto& derived_probabilities = reachability.initial_probabilities;
          const bool has_derived_probabilities =
              !derived_probabilities.empty() &&
              derived_probabilities.size() == reachability.states.size();
          if (stochastic_petri_options.analysis_initial_probabilities.empty() &&
              !initial_state.has_value() && !has_derived_probabilities) {
            throw std::invalid_argument(
                "stochastic Petri transient analysis could not retain the initial state; provide an initial probability vector");
          }
          const auto distribution =
              !stochastic_petri_options.analysis_initial_probabilities.empty()
                  ? minera::stochastic_petri_transient(
                        reachability,
                        stochastic_petri_options.analysis_time_seconds,
                        stochastic_petri_options.analysis_initial_probabilities)
                  : initial_state.has_value()
                        ? minera::stochastic_petri_transient(
                              reachability,
                              stochastic_petri_options.analysis_time_seconds,
                              *initial_state)
                        : minera::stochastic_petri_transient(
                              reachability,
                              stochastic_petri_options.analysis_time_seconds,
                              derived_probabilities);
          generated_json =
              minera::serialize_stochastic_petri_distribution_json(distribution);
        } else {
          generated_json = minera::serialize_stochastic_petri_distribution_json(
              minera::stochastic_petri_steady_state(reachability));
        }
      }
      if (selected != nullptr && petri_playout_mode == "basic" &&
          basic_playout_options.return_visited_elements)
        generated_json = minera::serialize_playout_visited_json(
            *selected, minera::playout_visited(*selected, basic_playout_options));
      if (selected != nullptr && stochastic_playout_options.return_visited_elements)
        generated_json = minera::serialize_stochastic_playout_visited_json(
            *selected, stochastic_playout_options.fit_from_log
                             ? minera::stochastic_playout_visited(
                                   *selected, *fit_log,
                                   stochastic_playout_options)
                             : minera::stochastic_playout_visited(
                                   *selected, stochastic_playout_options));
      if (generated_json) {
        // JSON is the explicit return-variants representation; no XES
        // conversion is performed for that request.
      } else if (monte_carlo_mode) {
        if (selected == nullptr) {
          std::cerr << "error: Monte Carlo playout requires a Petri model\n";
          return 2;
        }
        auto effective_options = monte_carlo_options;
        if (model_miner == "powl") {
          if (!monte_carlo_fields.contains("monte_carlo_activity_key"))
            effective_options.activity_key = powl_options.activity_key;
          if (!monte_carlo_fields.contains("monte_carlo_timestamp_key"))
            effective_options.timestamp_key = powl_options.timestamp_key;
          if (!projected_playout_log) {
            projected_playout_log = project_powl_activity(
                log, effective_options.activity_key,
                effective_options.timestamp_key, powl_options.case_id_key);
            fit_log = &*projected_playout_log;
          }
        } else if (model_miner == "inductive") {
          fit_log = &inductive_log;
        } else if (model_miner == "alpha") {
          fit_log = &alpha_log;
        }
        generated = minera::monte_carlo(*fit_log, *selected, effective_options).log;
      } else if (stochastic) {
        if (selected == nullptr) {
          std::cerr << "error: stochastic playout requires a Petri model\n";
          return 2;
        }
        generated = stochastic_playout_options.fit_from_log
                        ? minera::stochastic_playout(
                              *selected, *fit_log, stochastic_playout_options)
                        : minera::stochastic_playout(
                              *selected, stochastic_playout_options);
      } else if (!generated) {
        generated = minera::playout(*selected, basic_playout_options);
      }
      std::ofstream playout_file(playout_output);
      if (!playout_file) {
        std::cerr << "error: cannot open playout output file "
                  << playout_output << '\n';
        return 1;
      }
      if (generated_json) {
        playout_file << *generated_json << '\n';
      } else {
        minera::write_xes(playout_file, *generated,
                          (monte_carlo_mode ? model_miner + " Monte Carlo playout"
                           : stochastic ? model_miner + " stochastic playout"
                                      : playout_miner + " playout"));
      }
    }
  } catch (const std::exception& error) {
    std::cerr << "error: " << error.what() << '\n';
    return 1;
  }

  return 0;
}
