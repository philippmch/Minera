// Runs the Minera core on a CSV event log and emits the analysis (event log
// plus discovered directly-follows graph) as JSON for the frontend.

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstring>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "minera/alpha_miner.hpp"
#include "minera/bpmn.hpp"
#include "minera/conformance.hpp"
#include "minera/correlation_miner.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/declare.hpp"
#include "minera/declare_playout.hpp"
#include "minera/decision_mining.hpp"
#include "minera/dfg_variants.hpp"
#include "minera/dfg_alignment.hpp"
#include "minera/edit_distance_alignment.hpp"
#include "minera/directly_follows_graph.hpp"
#include "minera/eventually_follows.hpp"
#include "minera/footprints.hpp"
#include "minera/heuristic_miner.hpp"
#include "minera/inductive_miner.hpp"
#include "minera/json_export.hpp"
#include "minera/log_filter.hpp"
#include "minera/log_skeleton.hpp"
#include "minera/model_decoration.hpp"
#include "minera/performance.hpp"
#include "minera/performance_spectrum.hpp"
#include "minera/batches.hpp"
#include "minera/playout.hpp"
#include "minera/pnml.hpp"
#include "minera/powl_conversion.hpp"
#include "minera/powl_discovery.hpp"
#include "minera/ptml.hpp"
#include "minera/process_tree_alignment.hpp"
#include "minera/decomposed_alignment.hpp"
#include "minera/petri_anti_alignment.hpp"
#include "minera/petri_multi_alignment.hpp"
#include "minera/process_tree_reduction.hpp"
#include "minera/resource_analysis.hpp"
#include "minera/request_params.hpp"
#include "minera/temporal_profile.hpp"
#include "minera/minimum_self_distance.hpp"
#include "minera/transition_system.hpp"
#include "minera/xes_export.hpp"
#include "minera/xes_event_log_importer.hpp"

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
    std::string_view value) {
  std::vector<minera::BusinessHourSlot> slots;
  for (const auto& part : split_csv(value)) {
    auto separator = part.find(':');
    if (separator == std::string::npos) separator = part.find('-');
    if (separator == std::string::npos || separator == 0 ||
        separator + 1 == part.size()) {
      throw std::invalid_argument(
          "--dfg-business-hour-slots expects start-end second pairs");
    }
    const auto parse = [&](std::string_view text) {
      std::string owned(text);
      char* end = nullptr;
      const double result = std::strtod(owned.c_str(), &end);
      if (end != owned.c_str() + owned.size() || !std::isfinite(result)) {
        throw std::invalid_argument(
            "--dfg-business-hour-slots expects finite seconds");
      }
      return result;
    };
    const double start =
        parse(std::string_view(part).substr(0, separator));
    const double end =
        parse(std::string_view(part).substr(separator + 1));
    if (start < 0.0 || start >= end || end > 604800.0) {
      throw std::invalid_argument(
          "--dfg-business-hour-slots requires "
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
  if (name == "clean" || name == "clean_polars")
    return minera::DfgStatisticalVariant::kClean;
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
    const minera::EventLog& log, const std::vector<std::string>& variants,
    const minera::CorrelationMinerOptions& shared_options) {
  minera::CorrelationVariantsAnalysis result;
  result.variants.reserve(variants.size());
  for (const auto& name : variants) {
    auto options = shared_options;
    options.variant = correlation_variant(name);
    result.variants.push_back(
        {name, minera::discover_correlation_miner(log, options)});
  }
  return result;
}

minera::FootprintsAnalysis analyze_footprints(
    const minera::EventLog& log, const minera::DirectlyFollowsGraph& graph,
    const minera::ProcessTree& tree, const minera::PowlNode& powl,
    const minera::PetriNet& inductive_net, const minera::PetriNet& alpha_net,
    const minera::PetriNet& heuristic_net, const minera::PetriNet& powl_net,
    const minera::FootprintsOptions& options) {
  minera::FootprintsAnalysis analysis;
  analysis.model = options.model;
  analysis.strict = options.strict;
  std::optional<minera::Footprint> log_footprint;
  std::optional<std::vector<minera::Footprint>> trace_footprints;
  std::optional<minera::Footprint> dfg_footprint;
  std::optional<minera::Footprint> tree_footprint;
  std::optional<minera::Footprint> powl_footprint;
  const auto& log_fp = [&]() -> const minera::Footprint& {
    if (!log_footprint) log_footprint = minera::discover_footprints(log);
    return *log_footprint;
  }();
  const auto traces = [&]() -> const std::vector<minera::Footprint>& {
    if (!trace_footprints) {
      trace_footprints = minera::discover_trace_footprints(log);
    }
    return *trace_footprints;
  };
  const auto dfg_fp = [&]() -> const minera::Footprint& {
    if (!dfg_footprint) {
      dfg_footprint = minera::discover_dfg_footprints(graph);
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
    analysis.model_footprint =
        minera::discover_footprints(*selected_petri);
  }
  if (options.discovery_variants.contains("petri_reach_graph")) {
    if (selected_petri == nullptr) {
      throw std::invalid_argument(
          "petri_reach_graph requires a Petri-capable footprints model");
    }
    analysis.petri_reach_graph =
        minera::discover_footprints(*selected_petri);
  }

  if (options.conformance_variants.contains("log_model")) {
    analysis.log_model = minera::check_footprints_log_model(
        log_fp, analysis.model_footprint, {options.strict});
  }
  if (options.conformance_variants.contains("log_extensive")) {
    analysis.log_extensive = minera::check_footprints_log_extensive(
        log_fp, analysis.model_footprint);
  }
  const auto trace_results = minera::check_footprints_trace_extensive(
      traces(), analysis.model_footprint,
      {options.enable_act_always_executed});
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
               "  --noise-threshold <f>     Inductive Miner – infrequent filter\n"
               "                            in [0,1] (default: 0 = classic)\n"
               "  --inductive-variant <id>  log (default: split the log and\n"
               "                            recurse) or dfg (IMd: recurse on\n"
               "                            the directly-follows graph alone,\n"
               "                            scaling to huge logs)\n"
               "  --alpha-variant <id>      plus (default: short loops become\n"
               "                            self loops and two-way causality)\n"
               "                            or classic (the textbook Alpha)\n"
               "  --heuristic-variant <id>  classic (default) or plusplus\n"
               "                            (interval/eventual-follow semantics)\n"
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
               "                            Miner++ interval-start event attribute\n"
               "                            (default: start_timestamp)\n"
               "  --heuristic-lifecycle-instance-attribute <name>\n"
               "                            Miner++ lifecycle instance attribute\n"
               "                            (default: concept:instance)\n"
               "  --transition-view <id>    Transition-system state view:\n"
               "                            sequence (default), set, or multiset\n"
               "  --transition-direction <id>  Window direction: forward\n"
               "                            (default) or backward\n"
               "  --transition-window <n>   Non-negative state-window size\n"
               "                            (default: 2; 0 collapses all states)\n"
               "  --transition-include-data Include stable trace/event occurrence\n"
               "                            references in the transition system\n"
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
               "  --pnml <path>             Also write a model as PNML to <path>\n"
               "  --check-pnml <path>       Also check the log against the\n"
               "                            workflow net in this PNML file\n"
               "                            (conformance id: imported)\n"
               "  --pnml-miner <id>         Which model to export as PNML:\n"
               "                            inductive (default), alpha, heuristic,\n"
               "                            or powl\n"
               "  --bpmn <path>             Also write a model as BPMN 2.0 to <path>\n"
               "  --check-bpmn <path>       Also check the log against the\n"
               "                            BPMN 2.0 model in this file\n"
               "                            (conformance id: imported)\n"
               "  --bpmn-miner <id>         Which model to export as BPMN:\n"
               "                            inductive (default), alpha, heuristic,\n"
               "                            or powl\n"
               "  --ptml <path>             Also write the inductive process\n"
               "                            tree as PTML to <path>\n"
               "  --check-ptml <path>       Also check the log against the\n"
               "                            process tree in this PTML file\n"
               "                            (conformance id: imported)\n"
               "  --xes <path>              Also write the event log (after any\n"
               "                            filtering) as XES to <path>\n"
               "  --spectrum <a,b,c>        Compute the performance spectrum\n"
               "                            over this activity pattern (2+\n"
               "                            steps): per occurrence, when each\n"
               "                            step happened; embedded under\n"
               "                            \"performance_spectrum\"\n"
               "  --spectrum-variant <id>  connected (default) or disconnected\n"
               "  --spectrum-sample-size <n> Positive spectrum sample size\n"
               "  --spectrum-sort-log <bool> Sort log before spectrum mining\n"
               "  --batches                 Discover temporal event batches\n"
               "  --batch-merge-distance-seconds <f> Non-negative merge distance\n"
               "  --batch-min-size <n>      Minimum batch size (default: 2)\n"
               "  --batch-start-timestamp-attribute <name> Optional start-time attribute\n"
               "  --no-alignments           Skip the optimal alignments in the\n"
               "                            export (token replay and footprints\n"
               "                            stay); alignments dominate runtime\n"
               "  --align-dfg              Embed classic DFG alignments\n"
               "  --dfg-max-states <n>     DFG alignment state bound\n"
               "  --dfg-max-time-ms <n>    DFG alignment time bound\n"
               "  --dfg-sync-cost a=f      Per-activity sync cost\n"
               "  --dfg-model-move-cost a=f  Per-activity model cost\n"
               "  --dfg-log-move-cost a=f  Per-activity log cost\n"
               "  --dfg-internal-log-move-cost a=f  Internal log cost\n"
               "  --dfg-projection <mode>  none, log, or metrics\n"
               "  --align-edit-distance <file>  Align against CSV/XES reference\n"
               "  --edit-anti              Use farthest (anti) alignment\n"
               "  --edit-deviation-cost <n>  Positive edit deviation cost\n"
               "  --edit-projection <mode>  none or log\n"
               "                            on large, badly fitting logs\n"
               "  --align-process-tree     Embed process-tree alignments\n"
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
               "  --alignment-variant <id> state_equation_a_star (default),\n"
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
               "  --align-decomposed        Embed recompos_maximal decomposed alignment\n"
               "  --decomposed-variant <id> recompos_maximal\n"
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
               "  --advanced-petri-miner <id> inductive, alpha, heuristic, or powl\n"
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
               "  --token-replay-stop-immediately-unfit\n"
               "                            Stop a backwards replay trace at its\n"
               "                            first event that cannot be enabled\n"
               "  --log-skeleton-noise-threshold <f> Log-skeleton noise in [0,1]\n"
               "  --log-skeleton-constraints <a,b,...> Constraint families:\n"
               "                            equivalence, always_after, always_before,\n"
               "                            never_together, directly_follows, activ_freq\n"
               "  --declare-considered-activities <a,b,...> Declare activity filter\n"
               "                            (omit = infer; empty = use none)\n"
               "  --declare-allowed-templates <a,b,...> Declare template families\n"
               "                            (omit = all; empty = use none)\n"
               "  --declare-min-support-ratio <f> Declare support ratio in [0,1]\n"
               "  --declare-min-confidence-ratio <f> Declare confidence ratio in [0,1]\n"
               "  --declare-auto-selection-multiplier <f> Declare auto-selection multiplier in [0,1]\n"
               "  --footprints-discovery-variants <a,b,...> Footprint discovery:\n"
               "                            entire_event_log, trace_by_trace, dfg,\n"
               "                            process_tree, powl, petri_reach_graph\n"
               "  --footprints-model <id>  process_tree (default), powl,\n"
               "                            inductive, alpha, heuristic, or dfg\n"
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
               "  --dfg-start-timestamp-attribute <name> Performance start time\n"
               "  --dfg-business-hours     Measure performance in business time\n"
               "  --dfg-business-hour-slots <start-end,...> Weekly seconds\n"
               "  --dfg-case-attributes <a,b,...> Attributes for case DFG\n"
               "  --dfg-return-node-attributes Include case attributes on nodes\n"
               "  --correlation-variants <a,b,...> Correlation miner analyses:\n"
               "                            classic, classic_split, trace_based\n"
               "  --correlation-start-timestamp-attribute <name> Correlation start time\n"
               "  --correlation-exact-time-matching <bool> Exact timestamp matching\n"
               "                            (true, false, 1, or 0)\n"
               "  --correlation-sample-size <n> Positive correlation sample size\n"
               "  --decisions <miner>       Mine the decision points of one\n"
               "                            model (inductive, alpha, heuristic,\n"
               "                            or powl): which attribute values\n"
               "                            steer a case down which branch;\n"
               "                            embedded under \"decisions\"\n"
               "  --playout <path>          Simulate a discovered model to XES\n"
               "  --playout-miner <id>      Model for --playout: inductive\n"
               "                            (default), alpha, heuristic, powl,\n"
               "                            or declare\n"
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
               "  --prefix-activity <a>     Keep each case's head up to (and\n"
               "                            including) the first <a>; cases\n"
               "                            without <a> are dropped\n"
               "  --suffix-activity <a>     Keep each case's tail from the\n"
               "                            first <a> on; cases without <a>\n"
               "                            are dropped\n"
               "  --between-start <a>       With --between-end: cut each trace\n"
               "  --between-end <b>         into the subtraces running from <a>\n"
               "                            to <b>, endpoints included\n"
               "  --start-activities <a,b>  Keep traces starting with one of these\n"
               "  --end-activities <a,b>    Keep traces ending with one of these\n"
               "  --min-length <n>          Drop traces shorter than n events\n"
               "  --max-length <n>          Drop traces longer than n events\n"
               "  --min-variant-frequency <n>  Keep variants seen at least n times\n"
               "  --top-variants <n>        Keep only the n most frequent variants\n"
               "  --variant-coverage <f>    Keep the most frequent variants\n"
               "                            that together cover this share of\n"
               "                            cases, f in (0,1]\n";
  return 2;
}

}  // namespace

int main(int argc, char** argv) {
  std::filesystem::path input;
  std::filesystem::path output;
  std::filesystem::path pnml_output;
  std::filesystem::path check_pnml;
  std::filesystem::path check_bpmn;
  std::filesystem::path bpmn_output;
  std::filesystem::path ptml_output;
  std::filesystem::path check_ptml;
  std::filesystem::path xes_output;
  std::filesystem::path playout_output;
  std::string pnml_miner = "inductive";
  std::string bpmn_miner = "inductive";
  std::string playout_miner = "inductive";
  std::string decisions_miner;  // empty = no decision mining
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
  minera::AlignmentOptions alignment_options;
  bool alignment_option_used = false;
  bool alignment_max_alignments_used = false;
  bool alignment_exponent_used = false;
  bool alignment_synchronous_dijkstra_used = false;
  bool alignment_trace_cost_used = false;
  bool alignment_model_cost_used = false;
  bool alignment_silent_cost_used = false;
  bool alignment_sync_cost_used = false;
  bool align_decomposed = false;
  bool align_petri_anti = false;
  bool align_petri_multi = false;
  std::optional<std::size_t> reduce_trace_index;
  std::string reduction_activity_key;
  std::string advanced_petri_miner = "inductive";
  bool advanced_petri_miner_used = false;
  bool petri_option_used = false;
  minera::DecomposedAlignmentOptions decomposed_options;
  minera::PetriAntiAlignmentOptions petri_anti_options;
  minera::PetriMultiAlignmentOptions petri_multi_options;
  std::vector<std::string> spectrum_activities;  // empty = no spectrum
  bool spectrum_requested = false;
  bool spectrum_option_used = false;
  minera::PerformanceSpectrumOptions spectrum_options;
  bool batches = false;
  bool batch_option_used = false;
  minera::BatchDiscoveryOptions batch_options;
  minera::CsvImportOptions options;
  minera::LogFilterOptions filter;
  minera::InductiveMinerOptions inductive_options;
  minera::AlphaMinerOptions alpha_options;
  minera::HeuristicMinerOptions heuristic_options;
  minera::PowlDiscoveryOptions powl_options;
  minera::TransitionSystemOptions transition_options;
  minera::TokenReplayOptions token_replay_options;
  minera::LogSkeletonOptions log_skeleton_options;
  minera::DeclareDiscoveryOptions declare_options;
  minera::DeclarePlayoutOptions declare_playout_options;
  bool declare_playout_options_used = false;
  minera::FootprintsOptions footprints_options;
  std::vector<std::string> dfg_statistical_variants;
  std::vector<std::string> dfg_causal_variants;
  minera::DfgStatisticalOptions dfg_statistics_options;
  std::vector<std::string> correlation_variants;
  minera::CorrelationMinerOptions correlation_options;

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
          advanced_petri_miner != "powl") {
        return usage(argv[0]);
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
    } else if (arg == "--check-pnml") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      check_pnml = value;
    } else if (arg == "--check-bpmn") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      check_bpmn = value;
    } else if (arg == "--ptml") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      ptml_output = value;
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
    } else if (arg == "--decisions") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      decisions_miner = value;
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
               arg == "--pt-alignment-max-per-trace-time-ms") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      std::string key(arg.substr(2));
      std::replace(key.begin(), key.end(), '-', '_');
      pt_alignment_fields[key] = value;
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
          else if (arg == "--dfg-sync-cost" || arg == "--dfg-model-move-cost" || arg == "--dfg-log-move-cost" || arg == "--dfg-internal-log-move-cost") { const auto kv = parse_dfg_cost(value, arg); if (arg == "--dfg-sync-cost") dfg_alignment_options.sync_cost[kv.first] = kv.second; else if (arg == "--dfg-model-move-cost") dfg_alignment_options.model_move_cost[kv.first] = kv.second; else if (arg == "--dfg-log-move-cost") dfg_alignment_options.log_move_cost[kv.first] = kv.second; else dfg_alignment_options.internal_log_move_cost[kv.first] = kv.second; align_dfg = true; }
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
              ? std::set<std::string>{"entire_event_log", "trace_by_trace",
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
      } catch (const std::invalid_argument& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 2;
      }
    } else if (arg == "--dfg-keep-once-per-case") {
      dfg_statistics_options.keep_once_per_case = true;
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
    } else if (arg == "--dfg-start-timestamp-attribute") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      dfg_statistics_options.start_timestamp_attribute = value;
    } else if (arg == "--dfg-business-hours") {
      dfg_statistics_options.use_business_hours = true;
    } else if (arg == "--dfg-business-hour-slots") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      try {
        dfg_statistics_options.business_hour_slots =
            parse_dfg_business_hour_slots(value);
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
    } else if (arg == "--correlation-start-timestamp-attribute") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      correlation_options.start_timestamp_attribute = value;
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
    } else if (arg == "--spectrum") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      spectrum_activities = split_csv(value);
      spectrum_requested = true;
      if (spectrum_activities.size() < 2) {
        std::cerr << "error: --spectrum expects at least two activities\n";
        return 2;
      }
    } else if (arg == "--spectrum-variant") {
      spectrum_option_used = true;
      const auto* value = next();
      if (!value) return usage(argv[0]);
      if (std::string_view(value) == "connected")
        spectrum_options.variant = minera::PerformanceSpectrumVariant::kConnected;
      else if (std::string_view(value) == "disconnected")
        spectrum_options.variant = minera::PerformanceSpectrumVariant::kDisconnected;
      else {
        std::cerr << "error: --spectrum-variant expects connected or disconnected\n";
        return 2;
      }
    } else if (arg == "--spectrum-sample-size") {
      spectrum_option_used = true;
      const auto* value = next();
      if (!value) return usage(argv[0]);
      try {
        spectrum_options.sample_size = parse_positive_size(value, arg);
      } catch (const std::invalid_argument& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 2;
      }
    } else if (arg == "--spectrum-sort-log") {
      spectrum_option_used = true;
      const auto* value = next();
      if (!value) return usage(argv[0]);
      try {
        spectrum_options.sort_log = parse_boolean(value, arg);
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
        transition_options.window =
            parse_nonnegative_size(value, "--transition-window");
      }
      catch (const std::exception& error) { std::cerr << "error: " << error.what() << "\n"; return 2; }
    } else if (arg == "--transition-include-data") {
      transition_options.include_data = true;
    } else if (arg == "--powl-variant") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      const std::string_view variant = value;
      if (variant == "tree")
        powl_options.variant = minera::PowlDiscoveryVariant::kTree;
      else if (variant == "brute_force")
        powl_options.variant = minera::PowlDiscoveryVariant::kBruteForce;
      else if (variant == "maximal")
        powl_options.variant = minera::PowlDiscoveryVariant::kMaximal;
      else if (variant == "dynamic_clustering")
        powl_options.variant =
            minera::PowlDiscoveryVariant::kDynamicClustering;
      else if (variant == "dynamic_clustering_plain")
        powl_options.variant =
            minera::PowlDiscoveryVariant::kDynamicClusteringPlain;
      else {
        std::cerr << "error: --powl-variant expects tree, brute_force, "
                     "maximal, dynamic_clustering, or "
                     "dynamic_clustering_plain\n";
        return 2;
      }
    } else if (arg == "--powl-filtering-type") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      const std::string_view type = value;
      if (type == "decreasing_factor")
        powl_options.filtering_type =
            minera::PowlFilteringType::kDecreasingFactor;
      else if (type == "dynamic")
        powl_options.filtering_type = minera::PowlFilteringType::kDynamic;
      else {
        std::cerr << "error: --powl-filtering-type expects "
                     "decreasing_factor or dynamic\n";
        return 2;
      }
    } else if (arg == "--powl-filtering-threshold") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      char* end = nullptr;
      powl_options.filtering_threshold = std::strtod(value, &end);
      if (end == value || *end != '\0' ||
          !std::isfinite(powl_options.filtering_threshold) ||
          powl_options.filtering_threshold < 0.0 ||
          powl_options.filtering_threshold >= 1.0) {
        std::cerr << "error: --powl-filtering-threshold expects 0 <= f < 1\n";
        return 2;
      }
    } else if (arg == "--powl-order-frequency-ratio") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      char* end = nullptr;
      powl_options.order_frequency_ratio = std::strtod(value, &end);
      if (end == value || *end != '\0' ||
          !std::isfinite(powl_options.order_frequency_ratio) ||
          powl_options.order_frequency_ratio <= 0.5 ||
          powl_options.order_frequency_ratio > 1.0) {
        std::cerr << "error: --powl-order-frequency-ratio expects "
                     "0.5 < f <= 1\n";
        return 2;
      }
    } else if (arg == "--powl-brute-force-activity-limit") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      try {
        powl_options.brute_force_activity_limit = parse_nonnegative_size(
            value, "--powl-brute-force-activity-limit");
      } catch (const std::exception&) {
        std::cerr << "error: --powl-brute-force-activity-limit expects a "
                     "non-negative integer\n";
        return 2;
      }
    } else if (arg == "--noise-threshold") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      inductive_options.noise_threshold = std::atof(value);
    } else if (arg == "--inductive-variant") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      const std::string_view variant = value;
      if (variant != "log" && variant != "dfg") return usage(argv[0]);
      inductive_options.variant = variant == "dfg"
                                      ? minera::InductiveMinerVariant::kDfg
                                      : minera::InductiveMinerVariant::kLog;
    } else if (arg == "--alpha-variant") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      const std::string_view variant = value;
      if (variant != "classic" && variant != "plus") return usage(argv[0]);
      alpha_options.short_loops = variant == "plus";
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
      filter.variant_coverage = std::atof(value);
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
                 "--spectrum-sort-log require --spectrum\n";
    return 2;
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

  if (input.empty()) {
    return usage(argv[0]);
  }

  try {
    // A .xes file is read as XES; everything else is treated as CSV.
    auto log = ends_with_ci(input.string(), ".xes")
                   ? minera::import_xes_event_log(input)
                   : minera::import_csv_event_log(input, options);
    if (filter.any()) {
      log = minera::filter_event_log(log, filter);
    }
    auto graph = minera::discover_directly_follows_graph(log);
    std::optional<minera::DfgAlignmentLogResult> dfg_alignment_result;
    std::optional<minera::DfgAlignmentProjection> dfg_projection_result;
    std::optional<minera::EventLog> dfg_projected_log;
    if (align_dfg) {
      minera::DfgStatisticalOptions dfg_opts; dfg_opts.window = 1;
      const auto dfg = minera::discover_frequency_dfg(log, dfg_opts);
      std::map<std::string, std::uint64_t> starts, ends;
      for (const auto& t : log.traces) if (!t.events.empty()) { ++starts[t.events.front().activity]; ++ends[t.events.back().activity]; }
      dfg_alignment_result = minera::align_log_on_dfg(log, dfg, starts, ends, dfg_alignment_options);
      if (dfg_projection == "metrics") dfg_projection_result = minera::project_alignments_on_dfg(log, dfg, starts, ends, dfg_alignment_options);
      if (dfg_projection == "log") dfg_projected_log = minera::project_log_on_dfg(log, dfg, starts, ends, dfg_alignment_options);
    }
    std::optional<std::vector<minera::EditDistanceAlignment>> edit_result;
    std::optional<minera::EventLog> edit_projected_log;
    if (!edit_reference.empty()) {
      const auto reference = minera::ends_with_ci(edit_reference.string(), ".xes")
          ? minera::import_xes_event_log(edit_reference)
          : minera::import_csv_event_log(edit_reference, options);
      edit_result = minera::align_edit_distance(log, reference, edit_options);
      if (edit_projection == "log") { edit_projected_log = minera::project_log_on_alignments(log, *edit_result); }
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
      correlation_variants_analysis = analyze_correlation_variants(
          log, correlation_variants, correlation_options);
    }
    const auto temporal_profile = minera::discover_temporal_profile(log);
    const auto minimum_self_distances =
        minera::discover_minimum_self_distances(log);
    const auto transition_system =
        minera::discover_transition_system(log, transition_options);
    const auto tree = minera::discover_process_tree(log, inductive_options);
    std::optional<minera::ProcessTreeAlignmentResult> process_tree_alignment_result;
    if (align_process_tree) {
      process_tree_alignment_result =
          minera::align_process_tree(log, tree, pt_alignment_options);
    }
    const auto net = minera::discover_petri_net(log, alpha_options);
    const auto heuristics =
        minera::discover_heuristics_net(log, heuristic_options);
    const auto powl = minera::discover_powl(log, powl_options);
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
    const auto declare_conformance =
        minera::check_declare_conformance(log, declare);
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
    // unless --no-alignments opted out: optimal alignments dominate the whole
    // run on a large log a model fits badly, where every variant needs a deep
    // search (the marking-equation LP per visited state is the honest price
    // of optimality).
    const auto inductive_net = minera::to_petri_net(tree);
    const auto heuristic_net = minera::to_petri_net(heuristics);
    const auto powl_net = minera::to_petri_net(powl);
    const auto footprints =
        analyze_footprints(log, graph, tree, powl, inductive_net, net,
                           heuristic_net, powl_net, footprints_options);
    if (!no_alignments && log.traces.size() > 1000) {
      std::cerr << "note: computing optimal alignments for every model; on a "
                   "large or badly fitting log this can take minutes "
                   "(--no-alignments skips them)\n";
    }
    const auto model = [&](std::string id, const minera::PetriNet& petri) {
      return minera::ModelConformance{
          std::move(id), minera::final_marking_reachable(petri),
          minera::check_conformance(log, petri, token_replay_options),
          minera::check_footprints(log, petri),
          no_alignments ? std::optional<minera::AlignmentResult>{}
                        : std::optional<minera::AlignmentResult>(
                              alignment_option_used
                                  ? minera::check_alignment(log, petri, alignment_options)
                                  : minera::check_alignment(log, petri))};
    };
    std::vector<minera::ModelConformance> conformance = {
        model("inductive", inductive_net), model("alpha", net),
        model("heuristic", heuristic_net), model("powl", powl_net)};

    // Optionally check the log against a user-supplied model too — a PNML
    // workflow net, a BPMN 2.0 process, or a PTML process tree, all landing
    // under the single "imported" conformance id. Its activities are
    // registered in the id registry first, so the ones the log never shows
    // still resolve to names in the export.
    const int check_flags = (check_pnml.empty() ? 0 : 1) +
                            (check_bpmn.empty() ? 0 : 1) +
                            (check_ptml.empty() ? 0 : 1);
    if (check_flags > 1) {
      std::cerr << "error: pass only one of --check-pnml, --check-bpmn, and "
                   "--check-ptml (they share the conformance id "
                   "\"imported\")\n";
      return 2;
    }
    if (check_flags == 1) {
      const auto imported =
          !check_pnml.empty()   ? minera::import_pnml(check_pnml)
          : !check_bpmn.empty() ? minera::import_bpmn(check_bpmn)
                                : minera::to_petri_net(
                                      minera::import_ptml(check_ptml));
      for (const auto& activity : imported.activities()) {
        graph.ensure_activity(activity);
      }
      conformance.push_back(model("imported", imported));
    }
    const auto dataset = input.filename().string();

    // Optionally mine the decision points of one model's net.
    std::optional<minera::DecisionMiningResult> decisions;
    if (!decisions_miner.empty()) {
      const minera::PetriNet* selected = nullptr;
      if (decisions_miner == "inductive") {
        selected = &inductive_net;
      } else if (decisions_miner == "alpha") {
        selected = &net;
      } else if (decisions_miner == "heuristic") {
        selected = &heuristic_net;
      } else if (decisions_miner == "powl") {
        selected = &powl_net;
      } else {
        std::cerr << "error: unknown --decisions miner '" << decisions_miner
                  << "' (expected inductive, alpha, heuristic, or powl)\n";
        return 2;
      }
      decisions = minera::mine_decisions(log, *selected);
      decisions->miner = decisions_miner;
    }
    const minera::DecisionMiningResult* decisions_ptr =
        decisions ? &*decisions : nullptr;

    // Frequency/performance annotations for every miner's Petri-net view —
    // the inductive net fits by construction, so its overlay always has data
    // even when the alpha net (the classic view) fits nothing of a real log.
    const auto petri_decoration = minera::decorate_model(log, net);
    const auto inductive_decoration = minera::decorate_model(log, inductive_net);
    const auto heuristic_decoration = minera::decorate_model(log, heuristic_net);
    const auto powl_decoration = minera::decorate_model(log, powl_net);

    // Optionally compute the performance spectrum over an activity pattern.
    std::optional<minera::PerformanceSpectrum> spectrum;
    if (!spectrum_activities.empty()) {
      spectrum = minera::compute_performance_spectrum(log, spectrum_activities,
                                                      spectrum_options);
    }
    const minera::PerformanceSpectrum* spectrum_ptr =
        spectrum ? &*spectrum : nullptr;
    std::optional<minera::BatchDiscovery> batch_discovery;
    if (batches) {
      batch_discovery = minera::discover_batches(log, batch_options);
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
    if (align_decomposed) decomposed_result = minera::align_decomposed(log, *advanced_net, decomposed_options);
    if (align_petri_anti) petri_anti_result = minera::anti_align_petri(*advanced_net, log, petri_anti_options);
    if (align_petri_multi) petri_multi_result = minera::multi_align_petri(*advanced_net, log, petri_multi_options);
    if (reduce_trace_index) {
      if (*reduce_trace_index >= log.traces.size()) { std::cerr << "error: --reduce-process-tree trace index out of range\n"; return 2; }
      minera::ProcessTreeReductionOptions ro; ro.activity_key = reduction_activity_key;
      reduced_tree_result = minera::reduce_process_tree(tree, log.traces[*reduce_trace_index], ro);
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
          edit_result ? &*edit_result : nullptr, edit_anti,
          dfg_projection_result ? &*dfg_projection_result : nullptr,
          dfg_projected_log ? &*dfg_projected_log : nullptr,
          edit_projected_log ? &*edit_projected_log : nullptr,
          process_tree_alignment_result ? &*process_tree_alignment_result : nullptr,
          decomposed_result ? &*decomposed_result : nullptr,
          petri_anti_result ? &*petri_anti_result : nullptr,
          petri_multi_result ? &*petri_multi_result : nullptr,
          reduced_tree_result ? &*reduced_tree_result : nullptr,
          reduce_trace_index.value_or(0));
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
          edit_result ? &*edit_result : nullptr, edit_anti,
          dfg_projection_result ? &*dfg_projection_result : nullptr,
          dfg_projected_log ? &*dfg_projected_log : nullptr,
          edit_projected_log ? &*edit_projected_log : nullptr,
          process_tree_alignment_result ? &*process_tree_alignment_result : nullptr,
          decomposed_result ? &*decomposed_result : nullptr,
          petri_anti_result ? &*petri_anti_result : nullptr,
          petri_multi_result ? &*petri_multi_result : nullptr,
          reduced_tree_result ? &*reduced_tree_result : nullptr,
          reduce_trace_index.value_or(0));
    }

    // Optionally emit one discovered model as standard PNML.
    if (!pnml_output.empty()) {
      const minera::PetriNet* selected = nullptr;
      if (pnml_miner == "inductive") {
        selected = &inductive_net;
      } else if (pnml_miner == "alpha") {
        selected = &net;
      } else if (pnml_miner == "heuristic") {
        selected = &heuristic_net;
      } else if (pnml_miner == "powl") {
        selected = &powl_net;
      } else {
        std::cerr << "error: unknown --pnml-miner '" << pnml_miner
                  << "' (expected inductive, alpha, heuristic, or powl)\n";
        return 1;
      }
      std::ofstream pnml_file(pnml_output);
      if (!pnml_file) {
        std::cerr << "error: cannot open PNML output file " << pnml_output
                  << '\n';
        return 1;
      }
      minera::write_pnml(pnml_file, *selected, pnml_miner + " net");
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
        minera::write_bpmn(bpmn_file, tree, dataset);
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

    // Optionally emit the inductive process tree as PTML — the only miner
    // that produces a block-structured tree.
    if (!ptml_output.empty()) {
      std::ofstream ptml_file(ptml_output);
      if (!ptml_file) {
        std::cerr << "error: cannot open PTML output file " << ptml_output
                  << '\n';
        return 1;
      }
      minera::write_ptml(ptml_file, tree, dataset);
    }

    // Optionally emit the (filtered) event log as standard XES, making the
    // CLI double as a CSV-to-XES converter.
    if (!xes_output.empty()) {
      std::ofstream xes_file(xes_output);
      if (!xes_file) {
        std::cerr << "error: cannot open XES output file " << xes_output
                  << '\n';
        return 1;
      }
      minera::write_xes(xes_file, log, dataset);
    }

    if (!playout_output.empty()) {
      std::optional<minera::EventLog> generated;
      const minera::PetriNet* selected = nullptr;
      if (playout_miner == "inductive")
        selected = &inductive_net;
      else if (playout_miner == "alpha")
        selected = &net;
      else if (playout_miner == "heuristic")
        selected = &heuristic_net;
      else if (playout_miner == "powl")
        selected = &powl_net;
      else if (playout_miner == "declare")
        generated =
            minera::declare_playout(declare, declare_playout_options);
      else {
        std::cerr << "error: unknown --playout-miner '" << playout_miner
                  << "' (expected inductive, alpha, heuristic, powl, or "
                     "declare)\n";
        return 1;
      }
      if (!generated) generated = minera::playout(*selected);
      std::ofstream playout_file(playout_output);
      if (!playout_file) {
        std::cerr << "error: cannot open playout output file "
                  << playout_output << '\n';
        return 1;
      }
      minera::write_xes(playout_file, *generated, playout_miner + " playout");
    }
  } catch (const std::exception& error) {
    std::cerr << "error: " << error.what() << '\n';
    return 1;
  }

  return 0;
}
