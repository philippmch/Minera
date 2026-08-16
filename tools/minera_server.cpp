// Local application server: serves the built frontend and mines uploaded
// CSV event logs with the native core via POST /api/mine.

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <set>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

#include "httplib.h"
#include "minera/alpha_miner.hpp"
#include "minera/bpmn.hpp"
#include "minera/attribute_kde.hpp"
#include "minera/attribute_statistics.hpp"
#include "minera/attribute_distribution.hpp"
#include "minera/boundary_activities.hpp"
#include "minera/conformance.hpp"
#include "minera/concept_drift.hpp"
#include "minera/case_arrival.hpp"
#include "minera/case_statistics.hpp"
#include "minera/cycle_time.hpp"
#include "minera/correlation_miner.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/data_petri_net.hpp"
#include "minera/declare.hpp"
#include "minera/declare_playout.hpp"
#include "minera/decision_table.hpp"
#include "minera/directly_follows_graph.hpp"
#include "minera/dfg_conversion.hpp"
#include "minera/dfg_filter.hpp"
#include "minera/dfg_io.hpp"
#include "minera/dfg_precision.hpp"
#include "minera/dfg_alignment.hpp"
#include "minera/dfg_playout.hpp"
#include "minera/edit_distance_alignment.hpp"
#include "minera/dfg_variants.hpp"
#include "minera/eventually_follows.hpp"
#include "minera/footprints.hpp"
#include "minera/heuristic_miner.hpp"
#include "minera/inductive_miner.hpp"
#include "minera/genetic_miner.hpp"
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
#include "minera/ltl_filter.hpp"
#include "minera/log_skeleton.hpp"
#include "minera/log_to_target.hpp"
#include "minera/log_to_features.hpp"
#include "minera/log_util.hpp"
#include "minera/log_util_json.hpp"
#include "minera/embeddings.hpp"
#include "minera/label_splitting.hpp"
#include "minera/link_analysis.hpp"
#include "minera/trie.hpp"
#include "minera/trie_conversion.hpp"
#include "minera/interval_tree.hpp"
#include "minera/case_relations.hpp"
#include "minera/random_tree.hpp"
#include "minera/model_decoration.hpp"
#include "minera/monte_carlo.hpp"
#include "minera/oc_causal_net.hpp"
#include "minera/ocel.hpp"
#include "minera/ocel_csv.hpp"
#include "minera/ocel_features.hpp"
#include "minera/ocel_filter.hpp"
#include "minera/ocel_sqlite.hpp"
#include "minera/ocel_statistics.hpp"
#include "minera/ocel_playout.hpp"
#include "minera/ocel_transform.hpp"
#include "minera/ocel_xml.hpp"
#include "minera/saw_nets.hpp"
#include "minera/performance.hpp"
#include "minera/passed_time.hpp"
#include "minera/service_time.hpp"
#include "minera/rework.hpp"
#include "minera/performance_spectrum.hpp"
#include "minera/batches.hpp"
#include "minera/petri_net.hpp"
#include "minera/petri_analysis_json.hpp"
#include "minera/playout.hpp"
#include "minera/pnml.hpp"
#include "minera/powl_conversion.hpp"
#include "minera/powl_discovery.hpp"
#include "minera/ptml.hpp"
#include "minera/process_tree.hpp"
#include "minera/process_tree_playout.hpp"
#include "minera/process_tree_alignment.hpp"
#include "minera/request_params.hpp"
#include "minera/segment_filter.hpp"
#include "minera/streaming_dfg.hpp"
#include "minera/streaming_footprints.hpp"
#include "minera/streaming_temporal.hpp"
#include "minera/streaming_tbr.hpp"
#include "minera/path_filter.hpp"
#include "minera/decomposed_alignment.hpp"
#include "minera/petri_anti_alignment.hpp"
#include "minera/petri_multi_alignment.hpp"
#include "minera/process_tree_reduction.hpp"
#include "minera/process_cube.hpp"
#include "minera/resource_analysis.hpp"
#include "minera/temporal_profile.hpp"
#include "minera/timestamp_interleavings.hpp"
#include "minera/minimum_self_distance.hpp"
#include "minera/transition_system.hpp"
#include "minera/version.hpp"
#include "minera/xes_event_log_importer.hpp"
#include "minera/xes_export.hpp"

namespace {

using minera::error_json;
using minera::looks_like_xes;

// The most recently mined log and its discovered nets, kept so the costly
// alignment of a model can be computed on demand (POST /api/align) instead of
// eagerly for every miner on every mine. The app is local and single-user, so
// caching just the latest analysis is enough; a shared_ptr lets a slow
// alignment run on a snapshot without holding the lock.
struct MinedSession {
  minera::EventLog log;
  minera::DirectlyFollowsGraph graph;
  std::unordered_map<std::string, minera::PetriNet> nets;  // miner id -> net
  minera::ProcessTree tree;  // the inductive model, for on-demand BPMN export
  minera::DeclareModel declare;  // classic Declare, for on-demand playout
  std::string dataset;       // uploaded file name, echoed into XES exports
  minera::TokenReplayOptions token_replay;
};
std::shared_ptr<const MinedSession> g_session;
std::mutex g_session_mutex;
constexpr std::size_t kServerIlpMaxNodes = 1'000'000;
constexpr std::size_t kServerGeneticMaxPopulation = 10'000;
constexpr std::size_t kServerGeneticMaxGenerations = 10'000;
constexpr std::size_t kServerGeneticMaxStates = 1'000'000;
constexpr std::size_t kServerTvqMaxOutputTraces = 100'000;
constexpr std::size_t kServerTvqMaxOutputVariants = 100'000;
constexpr std::size_t kServerPripelMaxOutputTraces = 100'000;
constexpr std::size_t kServerOcelFilterMaxLevel = 32;
constexpr std::size_t kServerChaoticMaxActivities = 100'000;
constexpr std::size_t kServerChaoticMaxEvents = 1'000'000;
constexpr std::size_t kServerProfileMaxClusters = 10'000;
constexpr std::size_t kServerProfileMaxIterations = 10'000;
constexpr std::size_t kServerProfileMaxTraces = 100'000;
constexpr std::size_t kServerProfileMaxFeatures = 10'000;
constexpr std::size_t kServerAttributeKdeMaxGraphPoints = 10'000;
constexpr std::size_t kServerAttributeKdeMaxSamples = 100'000;

void reject_unknown_monte_carlo_params(const httplib::Request& request) {
  static const std::set<std::string> allowed = {
      "miner", "mode", "monte_carlo_simulations",
      "monte_carlo_case_arrival_seconds", "monte_carlo_max_steps",
      "monte_carlo_seed", "monte_carlo_start",
      "monte_carlo_default_duration_seconds",
      "monte_carlo_transition_weights",
      "monte_carlo_transition_durations",
      "monte_carlo_resources_per_place",
      "monte_carlo_default_resources_per_place",
      "monte_carlo_enable_diagnostics", "monte_carlo_stop_at_final"};
  for (const auto& [key, value] : request.params) {
    (void)value;
    if (!allowed.contains(key))
      throw std::invalid_argument("unknown Monte Carlo parameter: " + key);
  }
}

std::string form_field(const httplib::Request& request,
                       const std::string& name) {
  return request.has_file(name) ? request.get_file_value(name).content
                                : std::string{};
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
  if (options.discovery_variants.contains("entire_dataframe")) {
    analysis.entire_dataframe = log_fp;
  }
  if (options.discovery_variants.contains("polars_lazyframes")) {
    analysis.polars_lazyframes = log_fp;
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

void handle_mine(const httplib::Request& request, httplib::Response& response) {
  try {
    std::string csv;
    std::string dataset = "event log";
    minera::MineParams params;

    if (request.is_multipart_form_data()) {
      if (!request.has_file("log")) {
        response.status = 400;
        response.set_content(error_json("missing form field: log"),
                             "application/json");
        return;
      }
      const auto& file = request.get_file_value("log");
      csv = file.content;
      if (!file.filename.empty()) dataset = file.filename;

      params = minera::parse_mine_params(
          [&](std::string_view name) {
            return form_field(request, std::string(name));
          },
          [&](std::string_view name) {
            return request.has_file(std::string(name));
          });
    } else {
      csv = request.body;
      params = minera::parse_mine_params(
          [&](std::string_view name) {
            const auto key = std::string(name);
            return request.has_param(key) ? request.get_param_value(key)
                                          : std::string{};
          },
          [&](std::string_view name) {
            return request.has_param(std::string(name));
          });
    }

    std::istringstream input(csv);
    auto log = looks_like_xes(dataset, csv)
                   ? minera::import_xes_event_log(input)
                   : minera::import_csv_event_log(input, params.csv);
    if (params.filter.any()) {
      log = minera::filter_event_log(log, params.filter);
    }
    if (params.segment_enabled && params.segment) {
      log = minera::filter_segments(log, *params.segment);
    }
    if (params.path_enabled && params.path) {
      log = minera::filter_adjacent_path(log, *params.path);
    }
    if (params.ltl_enabled && params.ltl) {
      log = minera::filter_ltl_event_log(log, *params.ltl);
    }
    auto graph = minera::discover_directly_follows_graph(log);
    std::optional<minera::DfgVariantsAnalysis> dfg_variants_analysis;
    if (!params.dfg_statistical_variants.empty() ||
        !params.dfg_causal_variants.empty()) {
      dfg_variants_analysis = analyze_dfg_variants(
          log, params.dfg_statistical_variants, params.dfg_causal_variants,
          params.dfg_statistics);
    }
    std::optional<minera::CorrelationVariantsAnalysis>
        correlation_variants_analysis;
    if (!params.correlation_variants.empty()) {
      correlation_variants_analysis = analyze_correlation_variants(
          log, params.correlation_variants, params.correlation);
    }
    const auto temporal_profile = minera::discover_temporal_profile(log);
    const auto minimum_self_distances =
        minera::discover_minimum_self_distances(
            log, params.minimum_self_distance);
    const auto transition_system =
        minera::discover_transition_system(log, params.transition_system);
    const auto tree = minera::discover_process_tree(log, params.inductive);
    const auto net = minera::discover_petri_net(log, params.alpha);
    const auto heuristics = minera::discover_heuristics_net(log, params.heuristic);
    const auto powl = minera::discover_powl(log, params.powl);
    const auto inductive_net = minera::to_petri_net(tree);
    const auto heuristic_net = minera::to_petri_net(heuristics);
    const auto powl_net = minera::to_petri_net(powl);
    const auto footprints =
        analyze_footprints(log, graph, tree, powl, inductive_net, net,
                           heuristic_net, powl_net, params.footprints);
    const auto performance = minera::compute_performance_metrics(log, graph);
    const auto variant_durations =
        minera::compute_variant_durations(log, graph);
    const auto resources = minera::discover_resource_network(log);
    const auto eventually = minera::discover_eventually_follows(log, graph);
    std::optional<minera::PerformanceSpectrum> spectrum;
    if (!params.spectrum_activities.empty()) {
      spectrum = minera::compute_performance_spectrum(
          log, params.spectrum_activities, params.spectrum);
    }
    std::optional<minera::BatchDiscovery> batch_discovery;
    if (params.batches) {
      batch_discovery = minera::discover_batches(log, params.batch_options);
    }
    const auto log_skeleton =
        minera::discover_log_skeleton(log, params.log_skeleton);
    const auto log_skeleton_conformance = minera::check_log_skeleton_conformance(
        log, log_skeleton, params.log_skeleton);
    const auto declare =
        minera::discover_declare(log, params.declare_options);
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

    std::optional<minera::IntervalEventOverlapCounts> overlap_interval_events;
    std::optional<minera::CaseOverlapCounts> overlap_cases;
    std::optional<minera::ConcurrentActivityOverlapResult>
        overlap_concurrent_activities;
    if (params.overlap) {
      overlap_interval_events = minera::discover_interval_event_overlap_counts(
          log, params.overlap_options);
      overlap_cases = minera::discover_case_overlap_counts(
          log, params.overlap_options);
      overlap_concurrent_activities =
          minera::discover_concurrent_activity_overlap(log,
                                                       params.overlap_options);
    }

    // Token replay is cheap and robust, so run it for every model now; defer the
    // costlier alignment to POST /api/align, which reuses the cached nets below.
    const auto model = [&](std::string id, const minera::PetriNet& petri) {
      return minera::ModelConformance{std::move(id),
                                      minera::final_marking_reachable(petri),
                                      minera::check_conformance(
                                          log, petri, params.token_replay),
                                      minera::check_footprints(log, petri),
                                      std::nullopt};
    };
    const std::vector<minera::ModelConformance> conformance = {
        model("inductive", inductive_net), model("alpha", net),
        model("heuristic", heuristic_net), model("powl", powl_net)};

    // Frequency/performance annotations for every miner's Petri-net view:
    // one more token replay per net, cheap enough to compute up front.
    const auto petri_decoration = minera::decorate_model(log, net);
    const auto inductive_decoration = minera::decorate_model(log, inductive_net);
    const auto heuristic_decoration = minera::decorate_model(log, heuristic_net);
    const auto powl_decoration = minera::decorate_model(log, powl_net);

    const std::string body = minera::analysis_json(
        dataset, log, graph, &tree, &net, &heuristics, &conformance,
        &performance, &resources, &variant_durations, &eventually,
        /*decisions=*/nullptr, &petri_decoration,
        spectrum ? &*spectrum : nullptr,
        &inductive_net, &inductive_decoration, &heuristic_net,
        &heuristic_decoration, &temporal_profile, &minimum_self_distances,
        &transition_system, &powl, &powl_net, &powl_decoration,
        &log_skeleton, &log_skeleton_conformance, &declare,
        &declare_conformance, &footprints,
        dfg_variants_analysis ? &*dfg_variants_analysis : nullptr,
        correlation_variants_analysis ? &*correlation_variants_analysis
                                      : nullptr,
        batch_discovery ? &*batch_discovery : nullptr,
        /* dfg_alignment */ nullptr, /* edit_distance */ nullptr,
        /* edit_anti */ false, /* dfg_projection */ nullptr,
        /* dfg_projected_log */ nullptr, /* edit_projected_log */ nullptr,
        /* process_tree_alignment */ nullptr, /* decomposed_alignment */ nullptr,
        /* petri_anti_alignment */ nullptr, /* petri_multi_alignment */ nullptr,
        /* reduced_process_tree */ nullptr, /* reduced_trace_index */ 0,
        /* evaluation */ nullptr, /* earth_movers_distance */ nullptr,
        overlap_interval_events ? &*overlap_interval_events : nullptr,
        overlap_cases ? &*overlap_cases : nullptr,
        overlap_concurrent_activities ? &*overlap_concurrent_activities
                                      : nullptr,
        /* decision_tables */ nullptr,
        params.minimum_self_distance.variant);

    // Cache this analysis so alignments can be served on demand for the model
    // the user actually opens, rather than computed for all models up front.
    auto session = std::make_shared<MinedSession>();
    session->log = log;
    session->graph = graph;
    session->nets.emplace("inductive", inductive_net);
    session->nets.emplace("alpha", net);
    session->nets.emplace("heuristic", heuristic_net);
    session->nets.emplace("powl", powl_net);
    session->tree = tree;
    session->declare = declare;
    session->dataset = dataset;
    session->token_replay = params.token_replay;
    {
      std::lock_guard<std::mutex> lock(g_session_mutex);
      g_session = std::move(session);
    }

    response.set_content(body, "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

double ocel_query_number(const httplib::Request& request,
                         std::string_view name, double fallback) {
  const auto key = std::string(name);
  if (!request.has_param(key)) return fallback;
  const auto value = request.get_param_value(key);
  char* end = nullptr;
  const auto parsed = std::strtod(value.c_str(), &end);
  if (end != value.c_str() + value.size() || !std::isfinite(parsed)) {
    throw std::invalid_argument(key + ": expected a finite number");
  }
  return parsed;
}

std::size_t ocel_query_size(const httplib::Request& request,
                            std::string_view name, std::size_t fallback) {
  if (!request.has_param(std::string(name))) return fallback;
  const auto value = request.get_param_value(std::string(name));
  if (value.empty() || value.front() == '-')
    throw std::invalid_argument(std::string(name) + ": expected a non-negative integer");
  char* end = nullptr;
  const auto parsed = std::strtoull(value.c_str(), &end, 10);
  if (end != value.c_str() + value.size() ||
      parsed > std::numeric_limits<std::size_t>::max()) {
    throw std::invalid_argument(std::string(name) + ": expected a non-negative integer");
  }
  return static_cast<std::size_t>(parsed);
}

bool ocel_query_bool(const httplib::Request& request, std::string_view name,
                     bool fallback) {
  const auto key = std::string(name);
  if (!request.has_param(key)) return fallback;
  const auto value = request.get_param_value(key);
  if (value == "true" || value == "1") return true;
  if (value == "false" || value == "0") return false;
  throw std::invalid_argument(key + ": expected true, false, 1, or 0");
}

std::string ocel_request_body(const httplib::Request& request,
                              std::string_view field) {
  if (request.is_multipart_form_data()) {
    return form_field(request, std::string(field));
  }
  return request.body;
}

std::vector<std::string> split_ocel_playout_value(std::string_view value,
                                                  char delimiter) {
  std::vector<std::string> parts;
  std::string current;
  bool escaped = false;
  for (const char c : value) {
    if (escaped) {
      if (c != '\\' && c != ',' && c != ';' && c != ':')
        throw std::invalid_argument(
            "objects: unsupported escape (use \\\\, \\\\; \\\\: or \\\\\\)");
      current.push_back(c);
      escaped = false;
    } else if (c == '\\') {
      escaped = true;
    } else if (c == delimiter) {
      parts.push_back(std::move(current));
      current.clear();
    } else {
      current.push_back(c);
    }
  }
  if (escaped) throw std::invalid_argument("objects: dangling escape");
  parts.push_back(std::move(current));
  return parts;
}

std::map<std::string, std::vector<std::string>> parse_ocel_playout_objects(
    const httplib::Request& request) {
  std::map<std::string, std::vector<std::string>> result;
  if (!request.has_param("objects")) return result;
  const auto entries = split_ocel_playout_value(
      request.get_param_value("objects"), ';');
  for (const auto& entry : entries) {
    if (entry.empty()) throw std::invalid_argument("objects: empty entry");
    const auto fields = split_ocel_playout_value(entry, ':');
    if (fields.size() != 2 || fields.front().empty())
      throw std::invalid_argument(
          "objects: expected TYPE:ID1,ID2 entries");
    std::vector<std::string> ids;
    for (const auto& id : split_ocel_playout_value(fields[1], ',')) {
      if (id.empty()) throw std::invalid_argument(
          "objects: expected non-empty comma-separated ids");
      ids.push_back(id);
    }
    auto& target = result[fields.front()];
    target.insert(target.end(), ids.begin(), ids.end());
  }
  return result;
}

std::vector<std::string> split_ocel_filter_value(std::string_view value,
                                                 char delimiter);

std::vector<std::string> ocel_query_list(const httplib::Request& request,
                                         std::string_view name) {
  const auto key = std::string(name);
  if (!request.has_param(key)) return {};
  const auto result = split_ocel_filter_value(
      request.get_param_value(key), ',');
  if (result.empty() ||
      std::any_of(result.begin(), result.end(),
                  [](const std::string& item) { return item.empty(); })) {
    throw std::invalid_argument(
        key + ": expected non-empty comma-separated names");
  }
  return result;
}

std::vector<double> process_cube_query_bins(const httplib::Request& request,
                                             std::string_view name) {
  const auto values = ocel_query_list(request, name);
  std::vector<double> result;
  result.reserve(values.size());
  for (const auto& value : values) {
    char* end = nullptr;
    const double parsed = std::strtod(value.c_str(), &end);
    if (end != value.c_str() + value.size() || !std::isfinite(parsed))
      throw std::invalid_argument(std::string(name) + ": expected finite numeric edges");
    result.push_back(parsed);
  }
  if (!result.empty() && result.size() < 2)
    throw std::invalid_argument(std::string(name) + ": expected at least two edges");
  return result;
}

std::vector<minera::BusinessHourSlot> ocel_business_hour_slots(
    const httplib::Request& request) {
  const auto values = ocel_query_list(request, "business_hour_slots");
  std::vector<minera::BusinessHourSlot> slots;
  slots.reserve(values.size());
  for (const auto& value : values) {
    auto separator = value.find(':');
    if (separator == std::string::npos) separator = value.find('-');
    if (separator == std::string::npos || separator == 0 ||
        separator + 1 == value.size())
      throw std::invalid_argument(
          "business_hour_slots expects START-END pairs");
    const auto parse = [&](std::string_view text) {
      const std::string owned(text);
      char* end = nullptr;
      const double parsed = std::strtod(owned.c_str(), &end);
      if (end != owned.c_str() + owned.size() || !std::isfinite(parsed))
        throw std::invalid_argument(
            "business_hour_slots expects finite seconds");
      return parsed;
    };
    const double start = parse(std::string_view(value).substr(0, separator));
    const double end = parse(std::string_view(value).substr(separator + 1));
    if (start < 0.0 || start >= end || end > 604800.0)
      throw std::invalid_argument(
          "business_hour_slots requires 0 <= start < end <= 604800");
    slots.push_back({start, end});
  }
  if (request.has_param("business_hour_slots") && slots.empty())
    throw std::invalid_argument("business_hour_slots expects a non-empty list");
  return slots;
}

minera::ProcessCubeAggregation process_cube_aggregation(std::string_view value) {
  if (value == "mean") return minera::ProcessCubeAggregation::kMean;
  if (value == "sum") return minera::ProcessCubeAggregation::kSum;
  if (value == "min") return minera::ProcessCubeAggregation::kMin;
  if (value == "max") return minera::ProcessCubeAggregation::kMax;
  if (value == "count") return minera::ProcessCubeAggregation::kCount;
  if (value == "median") return minera::ProcessCubeAggregation::kMedian;
  if (value == "first") return minera::ProcessCubeAggregation::kFirst;
  if (value == "last") return minera::ProcessCubeAggregation::kLast;
  throw std::invalid_argument(
      "aggregation expects mean, sum, min, max, count, median, first, or last");
}

std::string ocel_discovery_response(std::string_view variant,
                                    std::string payload) {
  return std::string("{\"family\":\"OCEL_DISCOVERY\",\"variant\":\"") +
         std::string(variant) + "\",\"result\":" + std::move(payload) +
         "}";
}

std::string ocel_conformance_response(std::string_view variant,
                                      std::string payload) {
  return std::string("{\"family\":\"OCEL_CONFORMANCE\",\"variant\":\"") +
         std::string(variant) + "\",\"result\":" + std::move(payload) +
         "}";
}

std::string lower_ascii(std::string value) {
  for (auto& character : value) {
    character = static_cast<char>(
        std::tolower(static_cast<unsigned char>(character)));
  }
  return value;
}

std::vector<std::string> split_ocel_filter_value(std::string_view value,
                                                 char delimiter) {
  std::vector<std::string> result;
  std::string current;
  bool escaped = false;
  for (const char character : value) {
    if (escaped) {
      if (character != '\\' && character != ',' && character != ':' &&
          character != ';' && character != '|') {
        throw std::invalid_argument(
            "OCEL filter values use \\\\, \\\\: , \\\\; , or \\\\| escapes");
      }
      current.push_back(character);
      escaped = false;
    } else if (character == '\\') {
      escaped = true;
    } else if (character == delimiter) {
      result.push_back(std::move(current));
      current.clear();
    } else {
      current.push_back(character);
    }
  }
  if (escaped) throw std::invalid_argument("OCEL filter value has dangling escape");
  result.push_back(std::move(current));
  return result;
}

std::vector<minera::OcelValue> parse_ocel_filter_values(
    const httplib::Request& request) {
  std::vector<minera::OcelValue> result;
  if (!request.has_param("attribute_values")) return result;
  for (const auto& encoded : split_ocel_filter_value(
           request.get_param_value("attribute_values"), ',')) {
    const auto fields = split_ocel_filter_value(encoded, ':');
    if (fields.size() == 1) {
      result.emplace_back(fields.front());
      continue;
    }
    if (fields.size() != 2) {
      throw std::invalid_argument(
          "attribute_values expects typed scalar entries");
    }
    const auto& type = fields[0];
    const auto& value = fields[1];
    if (type == "s") {
      result.emplace_back(value);
    } else if (type == "i") {
      if (value.empty() || value.front() == '+')
        throw std::invalid_argument("attribute_values has an invalid integer");
      char* end = nullptr;
      const auto parsed = std::strtoll(value.c_str(), &end, 10);
      if (end != value.c_str() + value.size())
        throw std::invalid_argument("attribute_values has an invalid integer");
      result.emplace_back(static_cast<std::int64_t>(parsed));
    } else if (type == "d") {
      char* end = nullptr;
      const auto parsed = std::strtod(value.c_str(), &end);
      if (end != value.c_str() + value.size() || !std::isfinite(parsed))
        throw std::invalid_argument("attribute_values has an invalid number");
      result.emplace_back(parsed);
    } else if (type == "b") {
      if (value == "true" || value == "1") {
        result.emplace_back(true);
      } else if (value == "false" || value == "0") {
        result.emplace_back(false);
      } else {
        throw std::invalid_argument("attribute_values has an invalid boolean");
      }
    } else if (type == "n" && value.empty()) {
      result.emplace_back(nullptr);
    } else {
      throw std::invalid_argument("attribute_values has an unknown scalar type");
    }
  }
  return result;
}

std::map<std::string, std::size_t> parse_ocel_filter_counts(
    const httplib::Request& request) {
  std::map<std::string, std::size_t> result;
  if (!request.has_param("minimum_counts")) return result;
  const auto raw = request.get_param_value("minimum_counts");
  for (const auto& item : split_ocel_filter_value(raw, ',')) {
    const auto fields = split_ocel_filter_value(item, ':');
    if (fields.size() != 2 || fields[0].empty() || fields[1].empty()) {
      throw std::invalid_argument(
          "minimum_counts expects TYPE:COUNT entries");
    }
    const auto& type = fields[0];
    const auto& count = fields[1];
    if (count.empty() || count.front() == '-') {
      throw std::invalid_argument(
          "minimum_counts expects non-negative integer counts");
    }
    char* end_pointer = nullptr;
    const auto parsed = std::strtoull(count.c_str(), &end_pointer, 10);
    if (end_pointer != count.c_str() + count.size() ||
        parsed > std::numeric_limits<std::size_t>::max()) {
      throw std::invalid_argument(
          "minimum_counts expects non-negative integer counts");
    }
    if (!result.emplace(type, static_cast<std::size_t>(parsed)).second) {
      throw std::invalid_argument("minimum_counts repeats an object type");
    }
  }
  return result;
}

std::map<std::string, std::vector<std::string>> parse_ocel_filter_allowed(
    const httplib::Request& request) {
  std::map<std::string, std::vector<std::string>> result;
  if (!request.has_param("allowed")) return result;
  const auto raw = request.get_param_value("allowed");
  for (const auto& item : split_ocel_filter_value(raw, ';')) {
    const auto fields = split_ocel_filter_value(item, ':');
    if (fields.size() != 2 || fields[0].empty() || fields[1].empty()) {
      throw std::invalid_argument(
          "allowed expects TYPE:ACTIVITY|ACTIVITY entries");
    }
    const auto& type = fields[0];
    for (const auto& activity : split_ocel_filter_value(fields[1], '|')) {
      if (activity.empty()) {
        throw std::invalid_argument("allowed contains an empty activity");
      }
      result[type].push_back(activity);
    }
  }
  return result;
}

void handle_ocel_filter(const httplib::Request& request,
                        httplib::Response& response) {
  try {
    static const std::set<std::string> allowed = {
        "variant", "attribute_key", "attribute_values", "positive", "types",
        "ids", "event_ids", "from", "to", "object_type", "activity",
        "minimum_counts", "level", "object_id", "min_size", "max_size",
        "allowed"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key)) {
        throw std::invalid_argument("unknown OCEL filter parameter: " + key);
      }
    }
    const auto variant = lower_ascii(
        request.has_param("variant") ? request.get_param_value("variant")
                                      : std::string("object_count"));
    const auto body = ocel_request_body(request, "log");
    if (body.empty()) throw std::invalid_argument("missing OCEL JSON body");
    const auto source = minera::parse_ocel_json(body);
    const bool positive = ocel_query_bool(request, "positive", true);
    const auto object_type = request.has_param("object_type")
                                 ? request.get_param_value("object_type")
                                 : std::string{};
    const auto activity = request.has_param("activity")
                              ? request.get_param_value("activity")
                              : std::string{};
    const auto level = ocel_query_size(request, "level", 1);
    if (level > kServerOcelFilterMaxLevel) {
      throw std::invalid_argument(
          "level exceeds the OCEL filter server safety limit");
    }
    minera::Ocel filtered;
    std::string canonical;
    if (variant == "event_attribute") {
      if (!request.has_param("attribute_key"))
        throw std::invalid_argument("event_attribute requires attribute_key");
      filtered = minera::filter_ocel_event_attribute(
          source, request.get_param_value("attribute_key"),
          parse_ocel_filter_values(request), positive);
      canonical = "EVENT_ATTRIBUTE";
    } else if (variant == "object_attribute") {
      if (!request.has_param("attribute_key"))
        throw std::invalid_argument("object_attribute requires attribute_key");
      filtered = minera::filter_ocel_object_attribute(
          source, request.get_param_value("attribute_key"),
          parse_ocel_filter_values(request), positive);
      canonical = "OBJECT_ATTRIBUTE";
    } else if (variant == "object_types") {
      filtered = minera::filter_ocel_object_types(
          source, ocel_query_list(request, "types"), positive, level);
      canonical = "OBJECT_TYPES";
    } else if (variant == "objects") {
      filtered = minera::filter_ocel_objects(
          source, ocel_query_list(request, "ids"), positive, level);
      canonical = "OBJECTS";
    } else if (variant == "events") {
      filtered = minera::filter_ocel_events(
          source, ocel_query_list(request, "event_ids"), positive);
      canonical = "EVENTS";
    } else if (variant == "timestamp") {
      filtered = minera::filter_ocel_events_timestamp(
          source, request.has_param("from") ? request.get_param_value("from")
                                             : std::string{},
          request.has_param("to") ? request.get_param_value("to")
                                   : std::string{});
      canonical = "TIMESTAMP";
    } else if (variant == "activity_type_matching") {
      filtered = minera::filter_ocel_activity_type_matching(
          source, parse_ocel_filter_allowed(request));
      canonical = "ACTIVITY_TYPE_MATCHING";
    } else if (variant == "object_count") {
      filtered = minera::filter_ocel_events_object_type_counts(
          source, parse_ocel_filter_counts(request));
      canonical = "OBJECT_COUNT";
    } else if (variant == "start_events" || variant == "end_events") {
      if (object_type.empty())
        throw std::invalid_argument(variant + " requires object_type");
      filtered = variant == "start_events"
                     ? minera::filter_ocel_start_events_per_object_type(
                           source, object_type)
                     : minera::filter_ocel_end_events_per_object_type(
                           source, object_type);
      canonical = variant == "start_events" ? "START_EVENTS" : "END_EVENTS";
    } else if (variant == "activities_object_type") {
      if (object_type.empty())
        throw std::invalid_argument(
            "activities_object_type requires object_type");
      filtered = minera::filter_ocel_activities_connected_object_type(
          source, object_type);
      canonical = "ACTIVITIES_OBJECT_TYPE";
    } else if (variant == "cc_object") {
      if (!request.has_param("object_id"))
        throw std::invalid_argument("cc_object requires object_id");
      filtered = minera::filter_ocel_cc_object(
          source, request.get_param_value("object_id"));
      canonical = "CC_OBJECT";
    } else if (variant == "cc_length") {
      if (!request.has_param("min_size") || !request.has_param("max_size"))
        throw std::invalid_argument("cc_length requires min_size and max_size");
      filtered = minera::filter_ocel_cc_length(
          source, ocel_query_size(request, "min_size", 0),
          ocel_query_size(request, "max_size", 0));
      canonical = "CC_LENGTH";
    } else if (variant == "cc_object_type") {
      if (object_type.empty())
        throw std::invalid_argument("cc_object_type requires object_type");
      filtered = minera::filter_ocel_cc_object_type(source, object_type, positive);
      canonical = "CC_OBJECT_TYPE";
    } else if (variant == "cc_activity") {
      if (activity.empty())
        throw std::invalid_argument("cc_activity requires activity");
      filtered = minera::filter_ocel_cc_activity(source, activity);
      canonical = "CC_ACTIVITY";
    } else {
      throw std::invalid_argument("variant is not a supported OCEL filter");
    }
    response.set_content(
        std::string("{\"family\":\"OCEL_FILTER\",\"variant\":\"") +
            canonical + "\",\"log\":" +
            minera::serialize_ocel_json(filtered) + "}",
        "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_ocel_sqlite(const httplib::Request& request,
                        httplib::Response& response) {
  std::filesystem::path temporary;
  try {
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (key != "mode")
        throw std::invalid_argument("unknown OCEL SQLite parameter: " + key);
    }
    const auto mode = request.has_param("mode")
                          ? request.get_param_value("mode")
                          : std::string("import");
    if (mode != "import" && mode != "export")
      throw std::invalid_argument("OCEL SQLite mode expects import or export");
    std::string body;
    if (mode == "export") {
      body = ocel_request_body(request, "log");
      if (body.empty()) throw std::invalid_argument("missing OCEL JSON body");
    } else {
      if (!request.is_multipart_form_data() || !request.has_file("log"))
        throw std::invalid_argument(
            "OCEL SQLite import requires multipart field log");
      const auto file = request.get_file_value("log");
      body = file.content;
      if (body.empty()) throw std::invalid_argument("empty SQLite upload");
    }
    const auto stamp = std::chrono::high_resolution_clock::now()
                           .time_since_epoch().count();
    const auto thread = std::hash<std::thread::id>{}(std::this_thread::get_id());
    temporary = std::filesystem::temp_directory_path() /
                ("minera_ocel_" + std::to_string(stamp) + "_" +
                 std::to_string(thread) + ".sqlite");
    if (mode == "import") {
      std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
      if (!output) throw std::runtime_error("cannot create temporary SQLite upload");
      output.write(body.data(),
                   static_cast<std::streamsize>(body.size()));
      if (!output) throw std::runtime_error("cannot write temporary SQLite upload");
    }
    if (mode == "export") {
      minera::export_ocel_sqlite(minera::parse_ocel_json(body), temporary);
      std::ifstream input(temporary, std::ios::binary | std::ios::ate);
      if (!input) throw std::runtime_error("cannot read temporary SQLite output");
      const auto size = input.tellg();
      constexpr std::streamoff kMaxSqliteResponseBytes = 64 * 1024 * 1024;
      if (size < 16 || size > kMaxSqliteResponseBytes)
        throw std::invalid_argument("SQLite export exceeds the server response limit");
      std::string sqlite(static_cast<std::size_t>(size), '\0');
      input.seekg(0);
      input.read(sqlite.data(), static_cast<std::streamsize>(sqlite.size()));
      if (!input) throw std::runtime_error("cannot read temporary SQLite output");
      std::error_code ignored;
      std::filesystem::remove(temporary, ignored);
      temporary.clear();
      response.set_header("Content-Disposition",
                          "attachment; filename=\"ocel.sqlite\"");
      response.set_content(std::move(sqlite), "application/vnd.sqlite3");
      return;
    }
    const auto log = minera::import_ocel_sqlite(temporary);
    std::error_code ignored;
    std::filesystem::remove(temporary, ignored);
    temporary.clear();
    response.set_content(
        std::string("{\"family\":\"OCEL_SQLITE\",\"variant\":\"IMPORT\",\"log\":") +
            minera::serialize_ocel_json(log) + "}",
        "application/json");
  } catch (const std::exception& error) {
    if (!temporary.empty()) {
      std::error_code ignored;
      std::filesystem::remove(temporary, ignored);
    }
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_ocel_exchange(const httplib::Request& request,
                          httplib::Response& response) {
  try {
    static const std::set<std::string> allowed = {
        "mode", "format", "reject_unsupported", "include_declaration"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument("unknown OCEL exchange parameter: " + key);
    }

    const auto mode = lower_ascii(request.has_param("mode")
                                      ? request.get_param_value("mode")
                                      : std::string("import"));
    const auto format = lower_ascii(request.has_param("format")
                                        ? request.get_param_value("format")
                                        : std::string("json"));
    if (mode != "import" && mode != "export")
      throw std::invalid_argument("OCEL exchange mode expects import or export");
    if (format != "json" && format != "json_standard" && format != "xml" &&
        format != "xml_standard" && format != "csv" &&
        format != "csv_objects") {
      throw std::invalid_argument(
          "OCEL exchange format expects json, json_standard, xml, xml_standard, csv, or csv_objects");
    }

    const auto body = ocel_request_body(request, "log");
    if (body.empty()) throw std::invalid_argument("missing OCEL exchange body");

    if (mode == "export") {
      const auto log = minera::parse_ocel_json(body);
      std::string payload;
      std::string content_type;
      std::string filename;
      if (format == "json" || format == "json_standard") {
        payload = minera::serialize_ocel_json(
            log, format == "json_standard" ? minera::OcelJsonFormat::kStandard
                                             : minera::OcelJsonFormat::kClassic);
        content_type = "application/json";
        filename = "ocel.json";
      } else if (format == "xml" || format == "xml_standard") {
        minera::OcelXmlExportOptions options;
        options.format = format == "xml_standard"
                             ? minera::OcelXmlFormat::kStandard
                             : minera::OcelXmlFormat::kClassic;
        if (request.has_param("include_declaration")) {
          options.include_declaration = ocel_query_bool(
              request, "include_declaration", true);
        }
        payload = minera::serialize_ocel_xml(log, options);
        content_type = "application/xml";
        filename = "ocel.xml";
      } else if (format == "csv") {
        payload = minera::serialize_ocel_csv(log);
        content_type = "text/csv";
        filename = "ocel.csv";
      } else {
        payload = minera::serialize_ocel_objects_csv(log);
        content_type = "text/csv";
        filename = "ocel_objects.csv";
      }
      constexpr std::size_t kMaxExchangeResponseBytes = 32u * 1024u * 1024u;
      if (payload.size() > kMaxExchangeResponseBytes)
        throw std::invalid_argument("OCEL exchange export exceeds the response limit");
      response.set_header("Content-Disposition",
                          "attachment; filename=\"" + filename + "\"");
      response.set_content(std::move(payload), content_type);
      return;
    }

    minera::Ocel log;
    if (format == "json" || format == "json_standard") {
      log = minera::parse_ocel_json(body);
    } else if (format == "xml" || format == "xml_standard") {
      minera::OcelXmlImportOptions options;
      if (request.has_param("reject_unsupported")) {
        options.reject_unsupported = ocel_query_bool(
            request, "reject_unsupported", false);
      }
      log = minera::parse_ocel_xml(body, options);
    } else if (format == "csv") {
      const auto object_body = request.is_multipart_form_data()
                                   ? form_field(request, "objects")
                                   : std::string{};
      log = object_body.empty()
                ? minera::parse_ocel_csv(body)
                : minera::parse_ocel_csv(body, object_body);
    } else {
      throw std::invalid_argument(
          "csv_objects is an export-only format; upload csv event data");
    }
    std::string variant;
    if (format == "json") variant = "IMPORT_JSON";
    else if (format == "json_standard") variant = "IMPORT_JSON_STANDARD";
    else if (format == "xml") variant = "IMPORT_XML";
    else if (format == "xml_standard") variant = "IMPORT_XML_STANDARD";
    else variant = "IMPORT_CSV";
    response.set_content(
        std::string("{\"family\":\"OCEL_IO\",\"variant\":\"") +
            variant + "\",\"log\":" + minera::serialize_ocel_json(log) + "}",
        "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_ocel_discovery(const httplib::Request& request,
                           httplib::Response& response) {
  try {
    static const std::set<std::string> allowed = {
        "variant", "compute_edges_performance", "ocpn_annotation"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument("unknown OCEL discovery parameter: " + key);
    }
    const auto variant = request.has_param("variant")
                             ? request.get_param_value("variant")
                             : std::string("ocdfg");
    if (variant != "etot" && variant != "ocdfg" && variant != "ocpn" &&
        variant != "otg" && variant != "saw") {
      throw std::invalid_argument(
          "variant expects etot, ocdfg, ocpn, otg, or saw");
    }
    const auto body = ocel_request_body(request, "log");
    if (body.empty()) throw std::invalid_argument("missing OCEL JSON body");
    const auto log = minera::parse_ocel_json(body);
    std::string payload;
    if (variant == "etot") {
      payload = minera::etot_json(minera::discover_etot_classic(log));
    } else if (variant == "otg") {
      payload = minera::otg_json(minera::discover_otg_classic(log));
    } else if (variant == "ocpn") {
      minera::OcpnDiscoveryOptions discovery;
      if (request.has_param("ocpn_annotation")) {
        const auto value = request.get_param_value("ocpn_annotation");
        if (value == "annotated" || value == "classic") {
          discovery.annotated = true;
        } else if (value == "none" || value == "wo_annotation" ||
                   value == "without") {
          discovery.annotated = false;
        } else {
          throw std::invalid_argument(
              "ocpn_annotation expects annotated or none");
        }
      }
      payload = minera::serialize_ocpn_json(
          minera::discover_ocpn(log, discovery));
    } else if (variant == "saw") {
      payload = minera::saw_nets_json(minera::discover_saw_nets(log));
    } else {
      const bool performance =
          ocel_query_bool(request, "compute_edges_performance", true);
      payload = minera::serialize_ocdfg_json(
          minera::discover_ocdfg_classic(log, performance));
    }
    response.set_content(ocel_discovery_response(variant, std::move(payload)),
                         "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_ocel_split(const httplib::Request& request,
                       httplib::Response& response) {
  try {
    static const std::set<std::string> allowed = {
        "variant", "object_type", "max_objects", "max_degree_centrality"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument("unknown OCEL split parameter: " + key);
    }
    const auto variant = request.has_param("variant")
                             ? request.get_param_value("variant")
                             : std::string("connected_components");
    minera::OcelSplitOptions options;
    if (variant == "connected_components" || variant == "CONNECTED_COMPONENTS") {
      options.variant = minera::OcelSplitVariant::kConnectedComponents;
    } else if (variant == "ancestors_descendants" ||
               variant == "ANCESTORS_DESCENDANTS") {
      options.variant = minera::OcelSplitVariant::kAncestorsDescendants;
    } else {
      throw std::invalid_argument(
          "variant expects connected_components or ancestors_descendants");
    }
    options.object_type = request.has_param("object_type")
                              ? request.get_param_value("object_type")
                              : std::string{};
    options.max_objects = ocel_query_size(
        request, "max_objects", std::numeric_limits<std::size_t>::max());
    if (request.has_param("max_degree_centrality")) {
      options.max_degree_centrality =
          ocel_query_number(request, "max_degree_centrality", 1.0);
    }
    const auto body = ocel_request_body(request, "log");
    if (body.empty()) throw std::invalid_argument("missing OCEL JSON body");
    const auto result = minera::split_ocel(minera::parse_ocel_json(body), options);
    response.set_content(minera::serialize_ocel_split_json(result),
                         "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_ocel_playout(const httplib::Request& request,
                         httplib::Response& response) {
  try {
    static const std::set<std::string> allowed = {
        "model", "variant", "max_bindings_per_activity", "max_traces", "max_states",
        "objects", "objects_unique_per_trace", "event_binding", "start", "case_interval",
        "event_interval", "ocpn_variant", "double_arc_threshold",
        "noise_threshold", "ocpn_annotation"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument("unknown OCEL playout parameter: " + key);
    }
    const auto variant = request.has_param("variant")
                             ? request.get_param_value("variant")
                             : std::string("extensive");
    if (variant != "extensive" && variant != "EXTENSIVE")
      throw std::invalid_argument("variant expects extensive");
    const auto model = request.has_param("model")
                           ? request.get_param_value("model")
                           : std::string("ocpn");
    if (model != "ocpn" && model != "causal_net")
      throw std::invalid_argument("model expects ocpn or causal_net");

    minera::OcpnPlayoutOptions options;
    options.max_bindings_per_activity = ocel_query_size(
        request, "max_bindings_per_activity", options.max_bindings_per_activity);
    options.max_traces = ocel_query_size(request, "max_traces", options.max_traces);
    options.max_states = ocel_query_size(request, "max_states", options.max_states);
    options.objects = parse_ocel_playout_objects(request);
    options.objects_unique_per_trace = ocel_query_bool(
        request, "objects_unique_per_trace", options.objects_unique_per_trace);
    if (request.has_param("event_binding")) {
      const auto binding = request.get_param_value("event_binding");
      if (binding == "independent") {
        options.event_binding = minera::OcpnEventBinding::kIndependent;
      } else if (binding == "shared_activity") {
        options.event_binding = minera::OcpnEventBinding::kSharedActivity;
      } else {
        throw std::invalid_argument(
            "event_binding expects independent or shared_activity");
      }
    }
    options.start = ocel_query_number(request, "start", options.start);
    options.case_interval =
        ocel_query_number(request, "case_interval", options.case_interval);
    options.event_interval =
        ocel_query_number(request, "event_interval", options.event_interval);
    if (options.max_bindings_per_activity == 0 ||
        options.max_bindings_per_activity > 100 || options.max_traces > 10000 ||
        options.max_states > 1'000'000) {
      throw std::invalid_argument(
          "OCEL playout bounds exceed the server safety limits");
    }
    constexpr std::size_t kMaxGeneratedEvents = 1'000'000;
    if (options.max_traces != 0 && options.max_bindings_per_activity != 0 &&
        options.max_traces > kMaxGeneratedEvents /
                                  options.max_bindings_per_activity) {
      throw std::invalid_argument(
          "OCEL playout event budget exceeds the server safety limit");
    }
    const auto body = ocel_request_body(request, "log");
    if (body.empty()) throw std::invalid_argument("missing OCEL JSON body");
    minera::OcpnDiscoveryOptions discovery;
    if (request.has_param("ocpn_variant")) {
      const auto value = request.get_param_value("ocpn_variant");
      if (value == "log" || value == "im") {
        discovery.variant = minera::OcpnInductiveVariant::kLog;
      } else if (value == "dfg" || value == "imd") {
        discovery.variant = minera::OcpnInductiveVariant::kDfg;
      } else {
        throw std::invalid_argument("ocpn_variant expects log or dfg");
      }
    }
    if (request.has_param("ocpn_annotation")) {
      const auto value = request.get_param_value("ocpn_annotation");
      if (value == "annotated" || value == "classic") {
        discovery.annotated = true;
      } else if (value == "none" || value == "wo_annotation" ||
                 value == "without") {
        discovery.annotated = false;
      } else {
        throw std::invalid_argument(
            "ocpn_annotation expects annotated or none");
      }
    }
    discovery.double_arc_threshold = ocel_query_number(
        request, "double_arc_threshold", discovery.double_arc_threshold);
    discovery.noise_threshold =
        ocel_query_number(request, "noise_threshold", discovery.noise_threshold);
    const auto log = minera::parse_ocel_json(body);
    if (model == "causal_net") {
      const auto causal = minera::discover_oc_causal_net(log, discovery);
      response.set_content(minera::serialize_oc_causal_net_playout_json(
                               minera::oc_causal_net_extensive_playout(causal,
                                                                       options),
                           options.event_binding),
                           "application/json");
    } else {
      const auto graph = minera::discover_ocpn(log, discovery);
      response.set_content(minera::serialize_ocpn_playout_json(
                               minera::ocpn_extensive_playout(graph, options),
                               options.event_binding),
                           "application/json");
    }
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_ocel_features(const httplib::Request& request,
                          httplib::Response& response) {
  try {
    static const std::set<std::string> allowed = {
        "variant", "max_features", "include_activity_features",
        "include_timestamp_features", "include_relation_features",
        "include_endpoint_features", "include_attribute_counts",
        "include_interaction_features", "include_lifecycle_features",
        "numeric_event_attributes", "string_event_attributes",
        "numeric_object_attributes", "string_object_attributes"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument("unknown OCEL feature parameter: " + key);
    }
    const auto value = request.has_param("variant")
                           ? request.get_param_value("variant")
                           : std::string("events");
    minera::OcelFeatureVariant variant;
    if (value == "events" || value == "EVENTS") {
      variant = minera::OcelFeatureVariant::kEvents;
    } else if (value == "objects" || value == "OBJECTS") {
      variant = minera::OcelFeatureVariant::kObjects;
    } else if (value == "events_objects" || value == "EVENTS_OBJECTS") {
      variant = minera::OcelFeatureVariant::kEventsObjects;
    } else {
      throw std::invalid_argument(
          "variant expects events, objects, or events_objects");
    }
    minera::OcelFeatureOptions options;
    options.max_features =
        ocel_query_size(request, "max_features", options.max_features);
    if (options.max_features == 0 || options.max_features > 10000)
      throw std::invalid_argument("max_features exceeds the server safety limit");
    options.include_activity_features = ocel_query_bool(
        request, "include_activity_features", options.include_activity_features);
    options.include_timestamp_features = ocel_query_bool(
        request, "include_timestamp_features", options.include_timestamp_features);
    options.include_relation_features = ocel_query_bool(
        request, "include_relation_features", options.include_relation_features);
    options.include_endpoint_features = ocel_query_bool(
        request, "include_endpoint_features", options.include_endpoint_features);
    options.include_attribute_counts = ocel_query_bool(
        request, "include_attribute_counts", options.include_attribute_counts);
    options.include_interaction_features = ocel_query_bool(
        request, "include_interaction_features", options.include_interaction_features);
    options.include_lifecycle_features = ocel_query_bool(
        request, "include_lifecycle_features", options.include_lifecycle_features);
    options.numeric_event_attributes =
        ocel_query_list(request, "numeric_event_attributes");
    options.string_event_attributes =
        ocel_query_list(request, "string_event_attributes");
    options.numeric_object_attributes =
        ocel_query_list(request, "numeric_object_attributes");
    options.string_object_attributes =
        ocel_query_list(request, "string_object_attributes");
    const auto body = ocel_request_body(request, "log");
    if (body.empty()) throw std::invalid_argument("missing OCEL JSON body");
    response.set_content(minera::serialize_ocel_features_json(
                             minera::extract_ocel_features(
                                 minera::parse_ocel_json(body), variant, options)),
                         "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_ocel_statistics(const httplib::Request& request,
                            httplib::Response& response) {
  try {
    static const std::set<std::string> allowed = {
        "variant", "prefilter", "business_hours", "business_hour_slots"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument("unknown OCEL statistics parameter: " + key);
    }
    const auto value = request.has_param("variant")
                           ? request.get_param_value("variant")
                           : std::string("object_type_activities");
    minera::OcelStatisticsVariant variant;
    const auto prefilter_value = request.has_param("prefilter")
                                     ? request.get_param_value("prefilter")
                                     : std::string("none");
    if (prefilter_value != "none" && prefilter_value != "start" &&
        prefilter_value != "end")
      throw std::invalid_argument("prefilter expects none, start, or end");
    const auto prefilter = prefilter_value == "start"
                               ? minera::OcelAssociationPrefilter::kStart
                               : prefilter_value == "end"
                                     ? minera::OcelAssociationPrefilter::kEnd
                                     : minera::OcelAssociationPrefilter::kNone;
    const bool business_hours =
        request.has_param("business_hours")
            ? ocel_query_bool(request, "business_hours", false)
            : false;
    const auto business_hour_slots = ocel_business_hour_slots(request);
    if ((business_hours || !business_hour_slots.empty()) &&
        value != "edge_associations" && value != "EDGE_ASSOCIATIONS")
      throw std::invalid_argument(
          "business-hour options apply only to edge_associations");
    const auto body = ocel_request_body(request, "log");
    if (body.empty()) throw std::invalid_argument("missing OCEL JSON body");
    const auto log = minera::parse_ocel_json(body);
    if (value == "object_type_activities" ||
        value == "OBJECT_TYPE_ACTIVITIES") {
      if (prefilter != minera::OcelAssociationPrefilter::kNone)
        throw std::invalid_argument(
            "prefilter applies only to association statistics");
      variant = minera::OcelStatisticsVariant::kObjectTypeActivities;
      response.set_content(minera::serialize_ocel_statistics_json(log, variant),
                           "application/json");
    } else if (value == "objects_ot_count" || value == "OBJECTS_OT_COUNT") {
      if (prefilter != minera::OcelAssociationPrefilter::kNone)
        throw std::invalid_argument(
            "prefilter applies only to association statistics");
      variant = minera::OcelStatisticsVariant::kObjectsOtCount;
      response.set_content(minera::serialize_ocel_statistics_json(log, variant),
                           "application/json");
    } else if (value == "activity_associations" ||
               value == "ACTIVITY_ASSOCIATIONS") {
      response.set_content(
          minera::serialize_ocel_statistics_json(
              minera::activity_associations(log, prefilter)),
          "application/json");
    } else if (value == "object_type_activity_associations" ||
               value == "OBJECT_TYPE_ACTIVITY_ASSOCIATIONS") {
      response.set_content(
          minera::serialize_ocel_statistics_json(
              minera::object_type_activity_associations(log, prefilter)),
          "application/json");
    } else if (value == "edge_associations" || value == "EDGE_ASSOCIATIONS") {
      if (prefilter != minera::OcelAssociationPrefilter::kNone)
        throw std::invalid_argument(
            "prefilter applies only to association statistics");
      minera::OcelEdgeAssociationOptions edge_options;
      edge_options.use_business_hours = business_hours;
      edge_options.business_hour_slots = business_hour_slots;
      response.set_content(
          minera::serialize_ocel_statistics_json(
              minera::edge_associations(log, edge_options)),
          "application/json");
    } else {
      throw std::invalid_argument(
          "variant expects object_type_activities, objects_ot_count, activity_associations, object_type_activity_associations, or edge_associations");
    }
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_ocel_description(const httplib::Request& request,
                             httplib::Response& response) {
  try {
    static const std::set<std::string> allowed = {"variant",
                                                  "include_timestamps"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument("unknown OCEL description parameter: " +
                                    key);
    }
    const auto raw_variant = lower_ascii(
        request.has_param("variant") ? request.get_param_value("variant")
                                      : std::string("variant1"));
    minera::OcelDescriptionVariant variant;
    if (raw_variant == "variant1" || raw_variant == "text" ||
        raw_variant == "variant_1") {
      variant = minera::OcelDescriptionVariant::kVariant1;
    } else if (raw_variant == "variant2" || raw_variant == "tuples" ||
               raw_variant == "variant_2") {
      variant = minera::OcelDescriptionVariant::kVariant2;
    } else {
      throw std::invalid_argument(
          "variant expects variant1 or variant2");
    }
    minera::OcelDescriptionOptions options;
    options.include_timestamps = request.has_param("include_timestamps")
                                     ? ocel_query_bool(request,
                                                       "include_timestamps", true)
                                     : true;
    const auto body = ocel_request_body(request, "log");
    if (body.empty()) throw std::invalid_argument("missing OCEL JSON body");
    const auto log = minera::parse_ocel_json(body);
    response.set_content(
        minera::serialize_ocel_description_json(log, variant, options),
        "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_ocel_conformance(const httplib::Request& request,
                             httplib::Response& response) {
  try {
    static const std::set<std::string> allowed = {
        "variant", "alpha", "beta", "gamma", "delta", "etot_theta",
        "otg_theta", "ocdfg_theta_activity", "ocdfg_theta_flow"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument(
            "unknown OCEL conformance parameter: " + key);
    }
    const auto variant = request.has_param("variant")
                             ? request.get_param_value("variant")
                             : std::string("ocdfg");
    if (variant != "etot" && variant != "ocdfg" && variant != "otg") {
      throw std::invalid_argument("variant expects etot, ocdfg, or otg");
    }
    if (!request.is_multipart_form_data()) {
      throw std::invalid_argument(
          "OCEL conformance requires multipart fields log and normative");
    }
    const auto real_body = form_field(request, "log");
    const auto normative_body = form_field(request, "normative");
    if (real_body.empty() || normative_body.empty()) {
      throw std::invalid_argument(
          "OCEL conformance requires multipart fields log and normative");
    }
    const auto real = minera::parse_ocel_json(real_body);
    const auto normative = minera::parse_ocel_json(normative_body);
    const auto alpha = ocel_query_number(request, "alpha", 1.0);
    const auto beta = ocel_query_number(request, "beta", 1.0);
    const auto gamma = ocel_query_number(request, "gamma", 1.0);
    std::string payload;
    if (variant == "etot") {
      minera::EtotConformanceOptions options;
      options.alpha = alpha;
      options.beta = beta;
      options.gamma = gamma;
      options.theta_relative =
          ocel_query_number(request, "etot_theta", options.theta_relative);
      payload = minera::etot_conformance_json(minera::compare_etot(
          minera::discover_etot_classic(real),
          minera::discover_etot_classic(normative), options));
    } else if (variant == "otg") {
      minera::OtgConformanceOptions options;
      options.alpha = alpha;
      options.beta = beta;
      options.gamma = gamma;
      options.theta =
          ocel_query_number(request, "otg_theta", options.theta);
      payload = minera::otg_conformance_json(minera::compare_otg(
          minera::discover_otg_classic(real),
          minera::discover_otg_classic(normative), options));
    } else {
      minera::OcdfgConformanceOptions options;
      options.alpha = alpha;
      options.beta = beta;
      options.gamma = gamma;
      options.delta = ocel_query_number(request, "delta", options.delta);
      options.theta_activity = ocel_query_number(
          request, "ocdfg_theta_activity", options.theta_activity);
      options.theta_flow =
          ocel_query_number(request, "ocdfg_theta_flow", options.theta_flow);
      payload = minera::ocdfg_conformance_json(minera::compare_ocdfg(
          minera::discover_ocdfg_classic(real, false),
          minera::discover_ocdfg_classic(normative, false), options));
    }
    response.set_content(ocel_conformance_response(variant, std::move(payload)),
                         "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

// Checks the most recently mined log against a user-supplied model — a PNML
// workflow net, a BPMN 2.0 process, or a PTML process tree, told apart by the
// document's root element (multipart field `model`, or the raw body). The imported net joins
// the cached session under the id "imported", so POST /api/align?miner=imported
// can align against it on demand like any discovered model; the session's
// registry is extended with the net's activities so ids stay resolvable.
// Hidden cached-session adapter for the bounded classic ILP miner. The log is
// intentionally taken from the latest /api/mine session so this endpoint does
// not accept uploads or perform any implicit re-importing.
void handle_ilp_mine(const httplib::Request& request,
                     httplib::Response& response) {
  try {
    std::shared_ptr<const MinedSession> session;
    {
      std::lock_guard<std::mutex> lock(g_session_mutex);
      session = g_session;
    }
    if (!session) {
      response.status = 409;
      response.set_content(error_json("no cached log; run /api/mine first"),
                           "application/json");
      return;
    }
    static const std::set<std::string> allowed = {
        "ilp_activity_key", "ilp_noise_threshold", "ilp_max_nodes"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key)) {
        throw std::invalid_argument("unknown ILP parameter: " + key);
      }
    }
    const auto lookup = [&](std::string_view name) {
      const auto key = std::string(name);
      return request.has_param(key) ? request.get_param_value(key)
                                    : std::string{};
    };
    const auto present = [&](std::string_view name) {
      return request.has_param(std::string(name));
    };
    const auto options = minera::parse_ilp_miner_params(lookup, present);
    if (options.max_nodes > kServerIlpMaxNodes) {
      throw std::invalid_argument("ilp_max_nodes exceeds the server safety cap");
    }
    const auto result = minera::mine_ilp(session->log, options);
    response.set_content(minera::ilp_miner_json(result), "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

// Hidden cached-session adapter for bounded classic genetic discovery. The
// source log is the latest /api/mine session and all evolutionary limits are
// checked again at the server boundary.
void handle_genetic_mine(const httplib::Request& request,
                         httplib::Response& response) {
  try {
    std::shared_ptr<const MinedSession> session;
    {
      std::lock_guard<std::mutex> lock(g_session_mutex);
      session = g_session;
    }
    if (!session) {
      response.status = 409;
      response.set_content(error_json("no cached log; run /api/mine first"),
                           "application/json");
      return;
    }
    static const std::set<std::string> allowed = {
        "genetic_activity_key", "genetic_seed", "genetic_population_size",
        "genetic_generations", "genetic_elitism_rate",
        "genetic_crossover_rate", "genetic_mutation_rate",
        "genetic_elitism_min_sample", "genetic_max_activities",
        "genetic_max_states"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument("unknown genetic parameter: " + key);
    }
    const auto lookup = [&](std::string_view name) {
      const auto key = std::string(name);
      return request.has_param(key) ? request.get_param_value(key)
                                    : std::string{};
    };
    const auto present = [&](std::string_view name) {
      return request.has_param(std::string(name));
    };
    const auto options = minera::parse_genetic_miner_params(lookup, present);
    if (options.population_size > kServerGeneticMaxPopulation ||
        options.generations > kServerGeneticMaxGenerations ||
        options.max_states > kServerGeneticMaxStates) {
      throw std::invalid_argument("genetic safety bound exceeded");
    }
    const auto result = minera::mine_genetic(session->log, options);
    response.set_content(minera::genetic_miner_json(result),
                         "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

// Hidden cached-session adapter for bounded organizational SNA metrics.
void handle_organizational_sna(const httplib::Request& request,
                               httplib::Response& response) {
  try {
    std::shared_ptr<const MinedSession> session;
    { std::lock_guard<std::mutex> lock(g_session_mutex); session = g_session; }
    if (!session) {
      response.status = 409;
      response.set_content(error_json("no cached log; run /api/mine first"),
                           "application/json");
      return;
    }
    static const std::set<std::string> allowed = {
        "sna_variant", "sna_activity_attribute", "sna_resource_attribute",
        "sna_beta", "sna_n", "sna_metric_normalization",
        "sna_max_resources", "sna_max_edges"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument("unknown organizational SNA parameter: " + key);
    }
    const auto lookup = [&](std::string_view name) {
      const auto key = std::string(name);
      return request.has_param(key) ? request.get_param_value(key) : std::string{};
    };
    const auto present = [&](std::string_view name) {
      return request.has_param(std::string(name));
    };
    const auto options = minera::parse_organizational_sna_params(lookup, present);
    const auto result = minera::discover_organizational_sna(session->log, options);
    response.set_content(minera::organizational_sna_json(result),
                         "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_network_analysis(const httplib::Request& request, httplib::Response& response) {
  try {
    std::shared_ptr<const MinedSession> session;
    { std::lock_guard<std::mutex> lock(g_session_mutex); session = g_session; }
    if (!session) { response.status = 409; response.set_content(error_json("no cached log; run /api/mine first"), "application/json"); return; }
    static const std::set<std::string> allowed = {
      "network_analysis_variant", "network_analysis_sorting_attribute", "network_analysis_timestamp_attribute",
      "network_analysis_source_node_attribute", "network_analysis_target_node_attribute",
      "network_analysis_edge_attribute", "network_analysis_edge_reference",
      "network_analysis_out_relation_attribute", "network_analysis_in_relation_attribute",
      "network_analysis_include_performance", "network_analysis_business_hours",
      "network_analysis_business_hour_slots", "network_analysis_max_rows", "network_analysis_max_values"};
    for (const auto& [key, value] : request.params) { (void)value; if (!allowed.contains(key)) throw std::invalid_argument("unknown network analysis parameter: " + key); }
    const auto lookup = [&](std::string_view n) { const auto k = std::string(n); return request.has_param(k) ? request.get_param_value(k) : std::string{}; };
    const auto present = [&](std::string_view n) { return request.has_param(std::string(n)); };
    const auto options = minera::parse_network_analysis_params(lookup, present);
    response.set_content(minera::network_analysis_json(minera::discover_network_analysis(session->log, options)), "application/json");
  } catch (const std::exception& error) { response.status = 400; response.set_content(error_json(error.what()), "application/json"); }
}

void handle_link_analysis(const httplib::Request& request,
                          httplib::Response& response) {
  try {
    std::shared_ptr<const MinedSession> session;
    { std::lock_guard<std::mutex> lock(g_session_mutex); session = g_session; }
    if (!session) {
      response.status = 409;
      response.set_content(error_json("no cached log; run /api/mine first"),
                           "application/json");
      return;
    }
    static const std::set<std::string> allowed = {
        "link_analysis_out_column", "link_analysis_in_column",
        "link_analysis_sorting_column", "link_analysis_look_forward",
        "link_analysis_keep_first_occurrence", "link_analysis_propagate",
        "link_analysis_max_links"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument("unknown link analysis parameter: " + key);
    }
    const auto lookup = [&](std::string_view name) {
      const auto key = std::string(name);
      return request.has_param(key) ? request.get_param_value(key)
                                    : std::string{};
    };
    const auto present = [&](std::string_view name) {
      return request.has_param(std::string(name));
    };
    const auto options = minera::parse_link_analysis_params(lookup, present);
    response.set_content(
        minera::link_analysis_json(
            minera::discover_link_analysis(session->log, options)),
        "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_roles(const httplib::Request& request, httplib::Response& response) {
  try {
    std::shared_ptr<const MinedSession> session;
    { std::lock_guard<std::mutex> lock(g_session_mutex); session = g_session; }
    if (!session) { response.status = 409; response.set_content(error_json("no cached log; run /api/mine first"), "application/json"); return; }
    static const std::set<std::string> allowed = {"roles_variant", "roles_activity_attribute", "roles_resource_attribute", "roles_threshold", "roles_max_resources", "roles_max_activities"};
    for (const auto& [key, value] : request.params) { (void)value; if (!allowed.contains(key)) throw std::invalid_argument("unknown roles parameter: " + key); }
    const auto lookup = [&](std::string_view n) { const auto k = std::string(n); return request.has_param(k) ? request.get_param_value(k) : std::string{}; };
    const auto present = [&](std::string_view n) { return request.has_param(std::string(n)); };
    const auto options = minera::parse_role_discovery_params(lookup, present);
    response.set_content(minera::role_discovery_json(minera::discover_roles(session->log, options)), "application/json");
  } catch (const std::exception& error) { response.status = 400; response.set_content(error_json(error.what()), "application/json"); }
}

void handle_resource_profiles(const httplib::Request& request, httplib::Response& response) {
  try {
    std::shared_ptr<const MinedSession> session;
    { std::lock_guard<std::mutex> lock(g_session_mutex); session = g_session; }
    if (!session) { response.status = 409; response.set_content(error_json("no cached log; run /api/mine first"), "application/json"); return; }
    static const std::set<std::string> allowed = {"resource_profiles_metric", "resource_profiles_activity_attribute", "resource_profiles_resource_attribute", "resource_profiles_timestamp_attribute", "resource_profiles_start_timestamp_attribute", "resource_profiles_case_id_attribute", "resource_profiles_t1", "resource_profiles_t2", "resource_profiles_resource", "resource_profiles_activity", "resource_profiles_resource2", "resource_profiles_max_resources", "resource_profiles_max_rows"};
    for (const auto& [key, value] : request.params) { (void)value; if (!allowed.contains(key)) throw std::invalid_argument("unknown resource profile parameter: " + key); }
    const auto lookup = [&](std::string_view n) { const auto k = std::string(n); return request.has_param(k) ? request.get_param_value(k) : std::string{}; };
    const auto present = [&](std::string_view n) { return request.has_param(std::string(n)); };
    const auto options = minera::parse_resource_profiles_params(lookup, present);
    response.set_content(minera::resource_profiles_json(minera::discover_resource_profiles(session->log, options)), "application/json");
  } catch (const std::exception& error) { response.status = 400; response.set_content(error_json(error.what()), "application/json"); }
}

void handle_local_diagnostics(const httplib::Request& request, httplib::Response& response) {
  try {
    std::shared_ptr<const MinedSession> session;
    { std::lock_guard<std::mutex> lock(g_session_mutex); session = g_session; }
    if (!session) { response.status = 409; response.set_content(error_json("no cached log; run /api/mine first"), "application/json"); return; }
    static const std::set<std::string> allowed = {"local_diagnostics_group_attribute", "local_diagnostics_resource_attribute", "local_diagnostics_activity_attribute", "local_diagnostics_groups", "local_diagnostics_max_groups", "local_diagnostics_max_resources", "local_diagnostics_max_rows"};
    for (const auto& [key, value] : request.params) { (void)value; if (!allowed.contains(key)) throw std::invalid_argument("unknown local diagnostics parameter: " + key); }
    const auto lookup = [&](std::string_view n) { const auto k = std::string(n); return request.has_param(k) ? request.get_param_value(k) : std::string{}; };
    const auto present = [&](std::string_view n) { return request.has_param(std::string(n)); };
    const auto options = minera::parse_local_diagnostics_params(lookup, present);
    response.set_content(minera::local_diagnostics_json(minera::discover_local_diagnostics(session->log, options)), "application/json");
  } catch (const std::exception& error) { response.status = 400; response.set_content(error_json(error.what()), "application/json"); }
}

void handle_clustering(const httplib::Request& request, httplib::Response& response) {
  try {
    std::shared_ptr<const MinedSession> session;
    { std::lock_guard<std::mutex> lock(g_session_mutex); session = g_session; }
    if (!session) { response.status=409; response.set_content(error_json("no cached log; run /api/mine first"),"application/json"); return; }
    static const std::set<std::string> allowed={"clustering_trace_attribute","clustering_variant","clustering_alpha","clustering_percent","clustering_max_groups","clustering_max_output_nodes"};
    for (const auto& [k,v] : request.params) { (void)v; if(!allowed.contains(k)) throw std::invalid_argument("unknown clustering parameter: "+k); }
    const auto lookup=[&](std::string_view n){const auto k=std::string(n); return request.has_param(k)?request.get_param_value(k):std::string{};};
    const auto present=[&](std::string_view n){return request.has_param(std::string(n));};
    const auto o=minera::parse_clustering_params(lookup,present);
    response.set_content(minera::trace_attribute_clustering_json(minera::discover_trace_attribute_clustering(session->log,o)),"application/json");
  } catch (const std::exception& e) { response.status=400; response.set_content(error_json(e.what()),"application/json"); }
}

void handle_profile_clustering(const httplib::Request& request,
                               httplib::Response& response) {
  try {
    std::shared_ptr<const MinedSession> session;
    { std::lock_guard<std::mutex> lock(g_session_mutex); session = g_session; }
    if (!session) {
      response.status = 409;
      response.set_content(error_json("no cached log; run /api/mine first"),
                           "application/json");
      return;
    }
    static const std::set<std::string> allowed = {
        "profile_cluster_count", "profile_max_iterations", "profile_max_traces",
        "profile_max_features", "profile_seed", "profile_tolerance",
        "profile_include_activity", "profile_include_succession",
        "profile_trace_string_attributes", "profile_event_string_attributes",
        "profile_trace_numeric_attributes", "profile_event_numeric_attributes",
        "profile_feature_names"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument("unknown profile clustering parameter: " + key);
    }
    const auto lookup = [&](std::string_view name) {
      const auto key = std::string(name);
      return request.has_param(key) ? request.get_param_value(key)
                                    : std::string{};
    };
    const auto present = [&](std::string_view name) {
      return request.has_param(std::string(name));
    };
    const auto options = minera::parse_profile_clustering_params(lookup, present);
    if (options.cluster_count > kServerProfileMaxClusters ||
        options.max_iterations > kServerProfileMaxIterations ||
        options.max_traces > kServerProfileMaxTraces ||
        options.max_features > kServerProfileMaxFeatures) {
      throw std::invalid_argument("profile clustering safety bound exceeded");
    }
    response.set_content(
        minera::profile_clustering_json(
            minera::discover_profile_clustering(session->log, options)),
        "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_chaotic_activities(const httplib::Request& request,
                               httplib::Response& response) {
  try {
    std::shared_ptr<const MinedSession> session;
    { std::lock_guard<std::mutex> lock(g_session_mutex); session = g_session; }
    if (!session) {
      response.status = 409;
      response.set_content(error_json("no cached log; run /api/mine first"),
                           "application/json");
      return;
    }
    static const std::set<std::string> allowed = {
        "chaotic_activity_key", "chaotic_alpha", "chaotic_max_activities",
        "chaotic_max_events"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument("unknown chaotic-activities parameter: " + key);
    }
    const auto lookup = [&](std::string_view name) {
      const auto key = std::string(name);
      return request.has_param(key) ? request.get_param_value(key)
                                    : std::string{};
    };
    const auto present = [&](std::string_view name) {
      return request.has_param(std::string(name));
    };
    auto options = minera::parse_chaotic_activities_params(lookup, present);
    if (options.max_activities > kServerChaoticMaxActivities ||
        options.max_events > kServerChaoticMaxEvents)
      throw std::invalid_argument("chaotic-activities safety bound exceeded");
    response.set_content(
        minera::chaotic_activities_json(
            minera::discover_chaotic_activities(session->log, options)),
        "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_passed_time(const httplib::Request& request,
                        httplib::Response& response) {
  try {
    std::shared_ptr<const MinedSession> session;
    { std::lock_guard<std::mutex> lock(g_session_mutex); session = g_session; }
    if (!session) {
      response.status = 409;
      response.set_content(error_json("no cached log; run /api/mine first"),
                           "application/json");
      return;
    }
    static const std::set<std::string> allowed = {
        "passed_time_activity", "passed_time_variant",
        "passed_time_activity_key", "passed_time_timestamp_key",
        "passed_time_start_timestamp_key", "passed_time_aggregation",
        "passed_time_business_hours", "passed_time_business_hour_slots"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument("unknown passed-time parameter: " + key);
    }
    const auto lookup = [&](std::string_view name) {
      const auto key = std::string(name);
      return request.has_param(key) ? request.get_param_value(key)
                                    : std::string{};
    };
    const auto present = [&](std::string_view name) {
      return request.has_param(std::string(name));
    };
    const auto options = minera::parse_passed_time_params(lookup, present);
    response.set_content(
        minera::passed_time_json(
            minera::discover_passed_time(session->log, options)),
        "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_service_time(const httplib::Request& request,
                         httplib::Response& response) {
  try {
    std::shared_ptr<const MinedSession> session;
    { std::lock_guard<std::mutex> lock(g_session_mutex); session = g_session; }
    if (!session) {
      response.status = 409;
      response.set_content(error_json("no cached log; run /api/mine first"),
                           "application/json");
      return;
    }
    static const std::set<std::string> allowed = {
        "service_time_activity_key", "service_time_start_timestamp_key",
        "service_time_timestamp_key", "service_time_aggregation",
        "service_time_business_hours", "service_time_business_hour_slots"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument("unknown service-time parameter: " + key);
    }
    const auto lookup = [&](std::string_view name) {
      const auto key = std::string(name);
      return request.has_param(key) ? request.get_param_value(key)
                                    : std::string{};
    };
    const auto present = [&](std::string_view name) {
      return request.has_param(std::string(name));
    };
    const auto options = minera::parse_service_time_params(lookup, present);
    response.set_content(
        minera::service_time_json(
            minera::discover_service_time(session->log, options)),
        "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_rework(const httplib::Request& request,
                   httplib::Response& response) {
  try {
    std::shared_ptr<const MinedSession> session;
    { std::lock_guard<std::mutex> lock(g_session_mutex); session = g_session; }
    if (!session) {
      response.status = 409;
      response.set_content(error_json("no cached log; run /api/mine first"),
                           "application/json");
      return;
    }
    static const std::set<std::string> allowed = {
        "rework_variant", "rework_activity_key", "rework_case_id_key"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument("unknown rework parameter: " + key);
    }
    const auto lookup = [&](std::string_view name) {
      const auto key = std::string(name);
      return request.has_param(key) ? request.get_param_value(key)
                                    : std::string{};
    };
    const auto present = [&](std::string_view name) {
      return request.has_param(std::string(name));
    };
    const auto options = minera::parse_rework_params(lookup, present);
    response.set_content(
        minera::rework_json(minera::discover_rework(session->log, options)),
        "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_cycle_time(const httplib::Request& request,
                       httplib::Response& response) {
  try {
    std::shared_ptr<const MinedSession> session;
    { std::lock_guard<std::mutex> lock(g_session_mutex); session = g_session; }
    if (!session) {
      response.status = 409;
      response.set_content(error_json("no cached log; run /api/mine first"),
                           "application/json");
      return;
    }
    static const std::set<std::string> allowed = {
        "cycle_time_timestamp_key", "cycle_time_start_timestamp_key"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument("unknown cycle-time parameter: " + key);
    }
    const auto lookup = [&](std::string_view name) {
      const auto key = std::string(name);
      return request.has_param(key) ? request.get_param_value(key)
                                    : std::string{};
    };
    const auto present = [&](std::string_view name) {
      return request.has_param(std::string(name));
    };
    const auto options = minera::parse_cycle_time_params(lookup, present);
    response.set_content(
        minera::cycle_time_json(
            minera::discover_cycle_time(session->log, options)),
        "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_case_arrival(const httplib::Request& request,
                         httplib::Response& response) {
  try {
    std::shared_ptr<const MinedSession> session;
    { std::lock_guard<std::mutex> lock(g_session_mutex); session = g_session; }
    if (!session) {
      response.status = 409;
      response.set_content(error_json("no cached log; run /api/mine first"),
                           "application/json");
      return;
    }
    static const std::set<std::string> allowed = {
        "case_arrival_timestamp_key", "case_arrival_business_hours",
        "case_arrival_business_hour_slots"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument("unknown case-arrival parameter: " + key);
    }
    const auto lookup = [&](std::string_view name) {
      const auto key = std::string(name);
      return request.has_param(key) ? request.get_param_value(key)
                                    : std::string{};
    };
    const auto present = [&](std::string_view name) {
      return request.has_param(std::string(name));
    };
    const auto options = minera::parse_case_arrival_params(lookup, present);
    response.set_content(
        minera::case_arrival_json(
            minera::discover_case_arrival(session->log, options)),
        "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_case_statistics(const httplib::Request& request,
                            httplib::Response& response) {
  try {
    std::shared_ptr<const MinedSession> session;
    { std::lock_guard<std::mutex> lock(g_session_mutex); session = g_session; }
    if (!session) {
      response.status = 409;
      response.set_content(error_json("no cached log; run /api/mine first"),
                           "application/json");
      return;
    }
    static const std::set<std::string> allowed = {
        "case_statistics_case_id_key", "case_statistics_timestamp_key",
        "case_statistics_enable_sort", "case_statistics_sort_by",
        "case_statistics_sort_ascending", "case_statistics_max_cases",
        "case_statistics_business_hours",
        "case_statistics_business_hour_slots"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument(
            "unknown case-statistics parameter: " + key);
    }
    const auto lookup = [&](std::string_view name) {
      const auto key = std::string(name);
      return request.has_param(key) ? request.get_param_value(key)
                                    : std::string{};
    };
    const auto present = [&](std::string_view name) {
      return request.has_param(std::string(name));
    };
    const auto options = minera::parse_case_statistics_params(lookup, present);
    response.set_content(
        minera::case_statistics_json(
            minera::discover_case_statistics(session->log, options)),
        "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_attribute_statistics(const httplib::Request& request,
                                 httplib::Response& response) {
  try {
    std::shared_ptr<const MinedSession> session;
    { std::lock_guard<std::mutex> lock(g_session_mutex); session = g_session; }
    if (!session) {
      response.status = 409;
      response.set_content(error_json("no cached log; run /api/mine first"),
                           "application/json");
      return;
    }
    static const std::set<std::string> allowed = {
        "attribute_statistics_key", "attribute_statistics_scope",
        "attribute_statistics_keep_once_per_case"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument(
            "unknown attribute-statistics parameter: " + key);
    }
    const auto lookup = [&](std::string_view name) {
      const auto key = std::string(name);
      return request.has_param(key) ? request.get_param_value(key)
                                    : std::string{};
    };
    const auto present = [&](std::string_view name) {
      return request.has_param(std::string(name));
    };
    const auto options =
        minera::parse_attribute_statistics_params(lookup, present);
    response.set_content(
        minera::attribute_statistics_json(
            minera::discover_attribute_statistics(session->log, options)),
        "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_attribute_distribution(const httplib::Request& request,
                                   httplib::Response& response) {
  try {
    std::shared_ptr<const MinedSession> session;
    { std::lock_guard<std::mutex> lock(g_session_mutex); session = g_session; }
    if (!session) {
      response.status = 409;
      response.set_content(error_json("no cached log; run /api/mine first"),
                           "application/json");
      return;
    }
    static const std::set<std::string> allowed = {
        "attribute_distribution_type",
        "attribute_distribution_timestamp_key"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument(
            "unknown attribute-distribution parameter: " + key);
    }
    const auto lookup = [&](std::string_view name) {
      const auto key = std::string(name);
      return request.has_param(key) ? request.get_param_value(key)
                                    : std::string{};
    };
    const auto present = [&](std::string_view name) {
      return request.has_param(std::string(name));
    };
    const auto options =
        minera::parse_attribute_distribution_params(lookup, present);
    response.set_content(
        minera::attribute_distribution_json(
            minera::discover_event_distribution(session->log, options)),
        "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_attribute_kde(const httplib::Request& request,
                          httplib::Response& response) {
  try {
    std::shared_ptr<const MinedSession> session;
    { std::lock_guard<std::mutex> lock(g_session_mutex); session = g_session; }
    if (!session) {
      response.status = 409;
      response.set_content(error_json("no cached log; run /api/mine first"),
                           "application/json");
      return;
    }
    static const std::set<std::string> allowed = {
        "attribute_kde_key", "attribute_kde_variant",
        "attribute_kde_graph_points", "attribute_kde_points_to_sample"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument(
            "unknown attribute-kde parameter: " + key);
    }
    const auto lookup = [&](std::string_view name) {
      const auto key = std::string(name);
      return request.has_param(key) ? request.get_param_value(key)
                                    : std::string{};
    };
    const auto present = [&](std::string_view name) {
      return request.has_param(std::string(name));
    };
    const auto options = minera::parse_attribute_kde_params(lookup, present);
    if (options.graph_points > kServerAttributeKdeMaxGraphPoints ||
        options.points_to_sample > kServerAttributeKdeMaxSamples) {
      throw std::invalid_argument("attribute KDE safety bound exceeded");
    }
    response.set_content(
        minera::attribute_kde_json(
            minera::discover_attribute_kde(session->log, options)),
        "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_boundary_activities(const httplib::Request& request,
                                httplib::Response& response) {
  try {
    std::shared_ptr<const MinedSession> session;
    { std::lock_guard<std::mutex> lock(g_session_mutex); session = g_session; }
    if (!session) {
      response.status = 409;
      response.set_content(error_json("no cached log; run /api/mine first"),
                           "application/json");
      return;
    }
    static const std::set<std::string> allowed = {
        "boundary_activities_variant", "boundary_activities_activity_key"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument(
            "unknown boundary-activities parameter: " + key);
    }
    const auto lookup = [&](std::string_view name) {
      const auto key = std::string(name);
      return request.has_param(key) ? request.get_param_value(key)
                                    : std::string{};
    };
    const auto present = [&](std::string_view name) {
      return request.has_param(std::string(name));
    };
    const auto options =
        minera::parse_boundary_activities_params(lookup, present);
    response.set_content(
        minera::boundary_activities_json(
            minera::discover_boundary_activities(session->log, options)),
        "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_variant_statistics(const httplib::Request& request,
                               httplib::Response& response) {
  try {
    std::shared_ptr<const MinedSession> session;
    { std::lock_guard<std::mutex> lock(g_session_mutex); session = g_session; }
    if (!session) {
      response.status = 409;
      response.set_content(error_json("no cached log; run /api/mine first"),
                           "application/json");
      return;
    }
    static const std::set<std::string> allowed = {
        "variant_statistics_activity_key", "variant_statistics_max_variants"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument(
            "unknown variant-statistics parameter: " + key);
    }
    const auto lookup = [&](std::string_view name) {
      const auto key = std::string(name);
      return request.has_param(key) ? request.get_param_value(key)
                                    : std::string{};
    };
    const auto present = [&](std::string_view name) {
      return request.has_param(std::string(name));
    };
    const auto options =
        minera::parse_variant_statistics_params(lookup, present);
    response.set_content(
        minera::variant_statistics_json(
            minera::discover_variant_statistics(session->log, options)),
        "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

// Hidden cached-session adapter for trace-variant query anonymization. The
// source log remains in the latest mined session; only the released variant
// distribution is returned.
void handle_trace_variant_query(const httplib::Request& request,
                                httplib::Response& response) {
  try {
    std::shared_ptr<const MinedSession> session;
    {
      std::lock_guard<std::mutex> lock(g_session_mutex);
      session = g_session;
    }
    if (!session) {
      response.status = 409;
      response.set_content(error_json("no cached log; run /api/mine first"),
                           "application/json");
      return;
    }
    static const std::set<std::string> allowed = {
        "tvq_variant", "tvq_epsilon", "tvq_k", "tvq_p", "tvq_seed",
        "tvq_secure_random", "tvq_max_output_traces",
        "tvq_max_output_variants"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument("unknown trace-variant query parameter: " + key);
    }
    const auto lookup = [&](std::string_view name) {
      const auto key = std::string(name);
      return request.has_param(key) ? request.get_param_value(key)
                                    : std::string{};
    };
    const auto present = [&](std::string_view name) {
      return request.has_param(std::string(name));
    };
    const auto options = minera::parse_trace_variant_query_params(lookup, present);
    if (options.max_output_traces > kServerTvqMaxOutputTraces)
      throw std::invalid_argument("tvq_max_output_traces exceeds the server safety cap");
    if (options.max_output_variants > kServerTvqMaxOutputVariants)
      throw std::invalid_argument("tvq_max_output_variants exceeds the server safety cap");
    response.set_content(
        minera::trace_variant_query_json(
            minera::anonymize_trace_variant_query(session->log, options)),
        "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

// Cached-session PRIPEL adapter. The released trace-variant query is supplied
// as a multipart `trace_variant_query` file or as the raw request body.
void handle_pripel(const httplib::Request& request,
                   httplib::Response& response) {
  try {
    std::shared_ptr<const MinedSession> session;
    {
      std::lock_guard<std::mutex> lock(g_session_mutex);
      session = g_session;
    }
    if (!session) {
      response.status = 409;
      response.set_content(error_json("no cached log; run /api/mine first"),
                           "application/json");
      return;
    }
    static const std::set<std::string> allowed = {
        "pripel_epsilon", "pripel_blocklist", "pripel_seed",
        "pripel_secure_random", "pripel_max_output_traces",
        "pripel_max_distance"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument("unknown PRIPEL parameter: " + key);
    }
    std::string query_content;
    std::string query_name;
    if (request.has_file("trace_variant_query")) {
      const auto file = request.get_file_value("trace_variant_query");
      query_content = file.content;
      query_name = file.filename;
    } else if (request.has_file("query_log")) {
      const auto file = request.get_file_value("query_log");
      query_content = file.content;
      query_name = file.filename;
    } else {
      query_content = request.body;
    }
    if (query_content.empty())
      throw std::invalid_argument("PRIPEL requires a non-empty trace-variant query log");
    std::istringstream input(query_content);
    const auto query = looks_like_xes(query_name, query_content)
                           ? minera::import_xes_event_log(input)
                           : minera::import_csv_event_log(input);
    const auto lookup = [&](std::string_view name) {
      const auto key = std::string(name);
      return request.has_param(key) ? request.get_param_value(key)
                                    : std::string{};
    };
    const auto present = [&](std::string_view name) {
      return request.has_param(std::string(name));
    };
    const auto options = minera::parse_pripel_params(lookup, present);
    if (options.max_output_traces > kServerPripelMaxOutputTraces)
      throw std::invalid_argument("pripel_max_output_traces exceeds the server safety cap");
    response.set_content(
        minera::pripel_json(minera::anonymize_pripel(session->log, query, options)),
        "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_check_model(const httplib::Request& request,
                        httplib::Response& response) {
  std::shared_ptr<const MinedSession> session;
  {
    std::lock_guard<std::mutex> lock(g_session_mutex);
    session = g_session;
  }
  if (!session) {
    response.status = 409;  // nothing mined yet this run
    response.set_content(error_json("no mined log; mine a log first"),
                         "application/json");
    return;
  }

  try {
    const std::string model_xml = request.has_file("model")
                                      ? request.get_file_value("model").content
                                      : request.body;
    if (model_xml.empty()) {
      response.status = 400;
      response.set_content(error_json("missing form field: model"),
                           "application/json");
      return;
    }
    const auto imported =
        minera::looks_like_bpmn(model_xml) ? minera::parse_bpmn(model_xml)
        : minera::looks_like_ptml(model_xml)
            ? minera::to_petri_net(minera::parse_ptml(model_xml))
            : minera::parse_pnml(model_xml);

    auto next_session = std::make_shared<MinedSession>(*session);
    for (const auto& activity : imported.activities()) {
      next_session->graph.ensure_activity(activity);
    }
    next_session->nets.insert_or_assign("imported", imported);

    const minera::ModelConformance conformance{
        "imported", minera::final_marking_reachable(imported),
        minera::check_conformance(session->log, imported,
                                  session->token_replay),
        minera::check_footprints(session->log, imported), std::nullopt};
    const auto body =
        minera::imported_model_json(conformance, next_session->graph);

    {
      std::lock_guard<std::mutex> lock(g_session_mutex);
      // Only publish if no re-mine replaced the session in the meantime.
      if (g_session == session) g_session = std::move(next_session);
    }
    response.set_content(body, "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

// On-demand alignment of one model from the most recent mine. Expects a `miner`
// query parameter ("inductive" | "alpha" | "heuristic" | "powl").
void handle_align(const httplib::Request& request, httplib::Response& response) {
  std::shared_ptr<const MinedSession> session;
  {
    std::lock_guard<std::mutex> lock(g_session_mutex);
    session = g_session;
  }
  if (!session) {
    response.status = 409;  // nothing mined yet this run
    response.set_content(error_json("no mined log; mine a log first"),
                         "application/json");
    return;
  }

  const std::string miner = request.get_param_value("miner");
  const auto it = session->nets.find(miner);
  if (it == session->nets.end()) {
    response.status = 400;
    response.set_content(error_json("unknown miner: " + miner),
                         "application/json");
    return;
  }

  try {
    const auto lookup = [&](std::string_view name) {
      return request.has_param(std::string(name))
                 ? request.get_param_value(std::string(name))
                 : std::string{};
    };
    const auto present = [&](std::string_view name) {
      return request.has_param(std::string(name));
    };
    const auto options = minera::parse_alignment_params(lookup, present);
    const auto alignment = minera::check_alignment(session->log, it->second, options);
    response.set_content(minera::alignment_json(alignment, session->graph),
                         "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_concept_drift(const httplib::Request& request,
                          httplib::Response& response) {
  try {
    std::shared_ptr<const MinedSession> session;
    {
      std::lock_guard<std::mutex> lock(g_session_mutex);
      session = g_session;
    }
    if (!session) {
      response.status = 409;
      response.set_content(error_json("no cached log; run /api/mine first"),
                           "application/json");
      return;
    }
    static const std::set<std::string> allowed = {
        "drift_sub_log_size", "drift_window_size",
        "drift_num_permutations", "drift_threshold",
        "drift_max_no_change_points", "drift_seed",
        "drift_include_timestamps", "drift_activity_attribute",
        "drift_timestamp_attribute", "drift_case_id_attribute"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument("unknown concept drift parameter: " + key);
    }
    const auto lookup = [&](std::string_view name) {
      const auto key = std::string(name);
      return request.has_param(key) ? request.get_param_value(key) : std::string{};
    };
    const auto present = [&](std::string_view name) {
      return request.has_param(std::string(name));
    };
    const auto options = minera::parse_concept_drift_params(lookup, present);
    const auto result = minera::detect_concept_drift(session->log, options);
    response.set_content(minera::concept_drift_json(result), "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_log_to_target(const httplib::Request& request,
                          httplib::Response& response) {
  try {
    std::shared_ptr<const MinedSession> session;
    {
      std::lock_guard<std::mutex> lock(g_session_mutex);
      session = g_session;
    }
    if (!session) {
      response.status = 409;
      response.set_content(error_json("no cached log; run /api/mine first"),
                           "application/json");
      return;
    }
    static const std::set<std::string> allowed = {
        "target_variant",       "target_activity_key",
        "target_timestamp_key", "target_enable_padding",
        "target_pad_size",      "target_activities"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument("unknown target parameter: " + key);
    }
    const auto lookup = [&](std::string_view name) {
      const auto key = std::string(name);
      return request.has_param(key) ? request.get_param_value(key) : std::string{};
    };
    const auto present = [&](std::string_view name) {
      return request.has_param(std::string(name));
    };
    const auto options = minera::parse_log_to_target_params(lookup, present);
    const auto result = minera::log_to_target(session->log, options);
    response.set_content(minera::log_to_target_json(result),
                         "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_log_to_features(const httplib::Request& request,
                            httplib::Response& response) {
  try {
    std::shared_ptr<const MinedSession> session;
    { std::lock_guard<std::mutex> lock(g_session_mutex); session = g_session; }
    if (!session) { response.status = 409; response.set_content(error_json("no cached log; run /api/mine first"), "application/json"); return; }
    static const std::set<std::string> allowed = {
        "features_variant", "features_activity_key", "features_timestamp_key",
        "features_start_timestamp_key", "features_resource_key", "features_window_seconds",
        "features_default_not_present", "features_event_string_attributes",
        "features_event_numeric_attributes", "features_case_string_attributes",
        "features_case_numeric_attributes", "features_event_attributes",
        "features_case_attributes", "features_feature_names",
        "feature_variant", "feature_activity_key", "feature_timestamp_key",
        "feature_start_timestamp_key", "feature_resource_key", "feature_window_seconds",
        "feature_default_not_present", "feature_event_string_attributes",
        "feature_event_numeric_attributes", "feature_case_string_attributes",
        "feature_case_numeric_attributes", "feature_event_attributes",
        "feature_case_attributes", "feature_feature_names"};
    for (const auto& [key, value] : request.params) { (void)value; if (!allowed.contains(key)) throw std::invalid_argument("unknown features parameter: " + key); }
    const auto lookup = [&](std::string_view name) { const auto key = std::string(name); return request.has_param(key) ? request.get_param_value(key) : std::string{}; };
    const auto present = [&](std::string_view name) { return request.has_param(std::string(name)); };
    const auto options = minera::parse_log_to_features_params(lookup, present);
    response.set_content(minera::log_to_features_json(minera::log_to_features(session->log, options)), "application/json");
  } catch (const std::exception& error) { response.status = 400; response.set_content(error_json(error.what()), "application/json"); }
}

void handle_log_util(const httplib::Request& request,
                     httplib::Response& response) {
  try {
    std::shared_ptr<const MinedSession> session;
    { std::lock_guard<std::mutex> lock(g_session_mutex); session = g_session; }
    if (!session) {
      response.status = 409;
      response.set_content(error_json("no cached log; run /api/mine first"),
                           "application/json");
      return;
    }
    static const std::set<std::string> allowed = {
        "variant", "train_percentage", "seed", "prefix_length",
        "activity", "activity_key", "timestamp_key", "activities",
        "feature_min_different_values", "feature_max_different_values",
        "feature_consider_all_attributes", "feature_mandatory_attributes"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument("unknown log utility parameter: " + key);
    }
    const auto value = [&](std::string_view name) {
      const auto key = std::string(name);
      return request.has_param(key) ? request.get_param_value(key)
                                    : std::string{};
    };
    const auto parse_size = [&](std::string_view name, std::size_t limit) {
      const auto text = value(name);
      if (text.empty() || text.front() == '-')
        throw std::invalid_argument(std::string(name) +
                                    " expects a non-negative integer");
      std::size_t parsed = 0;
      const auto result = std::from_chars(text.data(), text.data() + text.size(),
                                          parsed);
      if (result.ec != std::errc{} || result.ptr != text.data() + text.size() ||
          parsed > limit)
        throw std::invalid_argument(std::string(name) + " exceeds the safety limit");
      return parsed;
    };
    const auto split_escaped = [&](std::string_view text,
                                   std::string_view name) {
      std::vector<std::string> values;
      std::string current;
      bool escaped = false;
      for (const char ch : text) {
        if (escaped) {
          if (ch != '\\' && ch != ',')
            throw std::invalid_argument(std::string(name) +
                                        " supports only comma and backslash escapes");
          current.push_back(ch);
          escaped = false;
        } else if (ch == '\\') {
          escaped = true;
        } else if (ch == ',') {
          if (current.empty())
            throw std::invalid_argument(std::string(name) + " contains an empty item");
          values.push_back(std::move(current));
          current.clear();
        } else {
          current.push_back(ch);
        }
      }
      if (escaped || current.empty())
        throw std::invalid_argument(std::string(name) + " contains an invalid item");
      values.push_back(std::move(current));
      return values;
    };
    const auto variant_text = request.has_param("variant")
                                  ? value("variant")
                                  : std::string("split_train_test");
    minera::LogUtilityOptions options;
    if (variant_text == "split_train_test") {
      options.variant = minera::LogUtilityVariant::kSplitTrainTest;
      if (request.has_param("train_percentage")) {
        const auto text = value("train_percentage");
        char* end = nullptr;
        const double parsed = std::strtod(text.c_str(), &end);
        if (end != text.c_str() + text.size() || !std::isfinite(parsed) ||
            parsed < 0.0 || parsed > 1.0)
          throw std::invalid_argument(
              "train_percentage expects a finite number in [0,1]");
        options.train_percentage = parsed;
      }
      if (request.has_param("seed")) options.seed = parse_size("seed", std::numeric_limits<std::size_t>::max());
      const auto result = minera::split_log_train_test(
          session->log, options.train_percentage, options.seed);
      response.set_content(minera::log_train_test_json(result),
                           "application/json");
    } else if (variant_text == "fixed_prefixes") {
      options.variant = minera::LogUtilityVariant::kFixedPrefixes;
      options.prefix_length = parse_size("prefix_length", 100000);
      response.set_content(minera::log_prefix_json(
                               minera::get_log_prefixes(session->log,
                                                        options.prefix_length),
                               options.variant),
                           "application/json");
    } else if (variant_text == "all_prefixes") {
      if (session->log.event_count() > 50000)
        throw std::invalid_argument("all_prefixes input exceeds the safety limit");
      const auto result = minera::get_log_with_prefixes(session->log);
      if (result.log.traces.size() > 100000)
        throw std::invalid_argument("all_prefixes output exceeds the safety limit");
      response.set_content(minera::log_prefix_json(
                               result.log, minera::LogUtilityVariant::kAllPrefixes,
                               result.change_indexes),
                           "application/json");
    } else if (variant_text == "until_activity") {
      options.variant = minera::LogUtilityVariant::kUntilActivity;
      options.activity = value("activity");
      options.activity_key = request.has_param("activity_key")
                                 ? value("activity_key")
                                 : "activity";
      options.timestamp_key = request.has_param("timestamp_key")
                                  ? value("timestamp_key")
                                  : "timestamp";
      const auto result = minera::get_log_traces_until_activity(
          session->log, options.activity, options);
      response.set_content(minera::log_until_activity_json(result),
                           "application/json");
    } else if (variant_text == "to_activities") {
      if (!request.has_param("activities"))
        throw std::invalid_argument("activities is required for to_activities");
      options.variant = minera::LogUtilityVariant::kToActivities;
      options.activity_key = request.has_param("activity_key")
                                 ? value("activity_key")
                                 : "activity";
      options.timestamp_key = request.has_param("timestamp_key")
                                  ? value("timestamp_key")
                                  : "timestamp";
      const auto result = minera::get_log_traces_to_activities(
          session->log, split_escaped(value("activities"), "activities"), options);
      response.set_content(minera::log_activity_targets_json(result),
                           "application/json");
    } else if (variant_text == "feature_selection") {
      minera::LogFeatureSelectionOptions feature_options;
      if (request.has_param("feature_min_different_values"))
        feature_options.min_different_values =
            parse_size("feature_min_different_values", 100000);
      if (request.has_param("feature_max_different_values"))
        feature_options.max_different_values =
            parse_size("feature_max_different_values", 100000);
      if (feature_options.min_different_values >
          feature_options.max_different_values)
        throw std::invalid_argument("feature minimum exceeds maximum");
      if (request.has_param("feature_consider_all_attributes")) {
        const auto text = value("feature_consider_all_attributes");
        if (text == "true" || text == "1")
          feature_options.consider_all_attributes = true;
        else if (text == "false" || text == "0")
          feature_options.consider_all_attributes = false;
        else
          throw std::invalid_argument(
              "feature_consider_all_attributes expects true or false");
      }
      if (request.has_param("feature_mandatory_attributes"))
        feature_options.mandatory_attributes =
            split_escaped(value("feature_mandatory_attributes"),
                          "feature_mandatory_attributes");
      response.set_content(minera::log_feature_selection_json(
                               minera::select_log_features(session->log,
                                                           feature_options)),
                           "application/json");
    } else {
      throw std::invalid_argument(
          "variant expects split_train_test, fixed_prefixes, all_prefixes, until_activity, to_activities, or feature_selection");
    }
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_embeddings(const httplib::Request& request,
                       httplib::Response& response) {
  try {
    std::shared_ptr<const MinedSession> session;
    {
      std::lock_guard<std::mutex> lock(g_session_mutex);
      session = g_session;
    }
    if (!session) {
      response.status = 409;
      response.set_content(error_json("no cached log; run /api/mine first"),
                           "application/json");
      return;
    }
    static const std::set<std::string> allowed = {
        "embedding_variant", "embedding_model", "embedding_attribute_key",
        "embedding_event_id_key", "embedding_dimensions",
        "embedding_max_items", "embedding_keep_cases", "embedding_target",
        "embedding_top_k"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument("unknown embedding parameter: " + key);
    }
    const auto lookup = [&](std::string_view name) {
      const auto key = std::string(name);
      return request.has_param(key) ? request.get_param_value(key)
                                    : std::string{};
    };
    const auto present = [&](std::string_view name) {
      return request.has_param(std::string(name));
    };
    const auto options = minera::parse_embedding_params(lookup, present);
    const auto result = minera::to_embeddings(session->log, options);
    if (request.has_param("embedding_target")) {
      const auto target = request.get_param_value("embedding_target");
      if (target.empty())
        throw std::invalid_argument("embedding_target: expected a value");
      const auto top_k = ocel_query_size(request, "embedding_top_k", 0);
      if (top_k > 10000)
        throw std::invalid_argument("embedding_top_k exceeds the server safety limit");
      const auto matches = minera::embedding_similarity(
          target, result, options, top_k);
      response.set_content(minera::serialize_embedding_similarity_json(
                               target, result, matches, top_k),
                           "application/json");
    } else {
      if (request.has_param("embedding_top_k"))
        throw std::invalid_argument(
            "embedding_top_k requires embedding_target");
      response.set_content(minera::serialize_embeddings_json(result),
                           "application/json");
    }
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

// On-demand transformation of the cached session. This route is intentionally
// not advertised by the frontend navigation, but remains available to clients
// that need deterministic contextual label splitting.
void handle_label_splitting(const httplib::Request& request,
                            httplib::Response& response) {
  try {
    std::shared_ptr<const MinedSession> session;
    { std::lock_guard<std::mutex> lock(g_session_mutex); session = g_session; }
    if (!session) { response.status = 409; response.set_content(error_json("no cached log; run /api/mine first"), "application/json"); return; }
    static const std::set<std::string> allowed = {
        "label_splitting_variant", "label_splitting_activity_key",
        "label_splitting_target_key", "label_splitting_prefix_length",
        "label_splitting_suffix_length", "label_splitting_min_edge_weight",
        "label_splitting_activities_suffix", "label_splitting_target_activities"};
    for (const auto& [key, value] : request.params) { (void)value; if (!allowed.contains(key)) throw std::invalid_argument("unknown label_splitting parameter: " + key); }
    const auto lookup = [&](std::string_view name) { const auto key = std::string(name); return request.has_param(key) ? request.get_param_value(key) : std::string{}; };
    const auto present = [&](std::string_view name) { return request.has_param(std::string(name)); };
    const auto options = minera::parse_label_splitting_params(lookup, present);
    response.set_content(minera::label_splitting_json(minera::split_activity_labels(session->log, options)), "application/json");
  } catch (const std::exception& error) { response.status = 400; response.set_content(error_json(error.what()), "application/json"); }
}

void handle_trie(const httplib::Request& request,
                 httplib::Response& response) {
  try {
    std::shared_ptr<const MinedSession> session;
    {
      std::lock_guard<std::mutex> lock(g_session_mutex);
      session = g_session;
    }
    if (!session) {
      response.status = 409;
      response.set_content(error_json("no cached log; run /api/mine first"),
                           "application/json");
      return;
    }
    static const std::set<std::string> allowed = {
        "trie_activity_key", "trie_max_path_length"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument("unknown trie parameter: " + key);
    }
    const auto lookup = [&](std::string_view name) {
      const auto key = std::string(name);
      return request.has_param(key) ? request.get_param_value(key) : std::string{};
    };
    const auto present = [&](std::string_view name) {
      return request.has_param(std::string(name));
    };
    const auto options = minera::parse_trie_params(lookup, present);
    response.set_content(minera::trie_json(
                             minera::discover_trie(session->log, options)),
                         "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_trie_to_petri(const httplib::Request& request,
                          httplib::Response& response) {
  try {
    std::shared_ptr<const MinedSession> session;
    {
      std::lock_guard<std::mutex> lock(g_session_mutex);
      session = g_session;
    }
    if (!session) {
      response.status = 409;
      response.set_content(error_json("no cached log; run /api/mine first"),
                           "application/json");
      return;
    }
    static const std::set<std::string> allowed = {
        "trie_activity_key", "trie_max_path_length"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument("unknown trie parameter: " + key);
    }
    const auto lookup = [&](std::string_view name) {
      const auto key = std::string(name);
      return request.has_param(key) ? request.get_param_value(key)
                                    : std::string{};
    };
    const auto present = [&](std::string_view name) {
      return request.has_param(std::string(name));
    };
    const auto options = minera::parse_trie_params(lookup, present);
    const auto net = minera::convert_trie_to_petri_net(
        minera::discover_trie(session->log, options));
    response.set_content(minera::serialize_trie_petri_net_json(net),
                         "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_interval_tree(const httplib::Request& request,
                          httplib::Response& response) {
  try {
    std::shared_ptr<const MinedSession> session;
    {
      std::lock_guard<std::mutex> lock(g_session_mutex);
      session = g_session;
    }
    if (!session) {
      response.status = 409;
      response.set_content(error_json("no cached log; run /api/mine first"),
                           "application/json");
      return;
    }
    static const std::set<std::string> allowed = {
        "interval_activity_key", "interval_start_timestamp_key",
        "interval_timestamp_key", "interval_epsilon",
        "interval_filter_activity_couple", "interval_query_point",
        "interval_query_start", "interval_query_end"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument("unknown interval parameter: " + key);
    }
    const auto lookup = [&](std::string_view name) {
      const auto key = std::string(name);
      return request.has_param(key) ? request.get_param_value(key) : std::string{};
    };
    const auto present = [&](std::string_view name) {
      return request.has_param(std::string(name));
    };
    const auto options = minera::parse_interval_tree_params(lookup, present);
    const auto intervals =
        minera::discover_open_path_intervals(session->log, options);
    const auto query = minera::query_open_path_intervals(
        intervals, minera::interval_tree_query_options(options));
    response.set_content(minera::interval_tree_json(query), "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_case_relations(const httplib::Request& request,
                           httplib::Response& response) {
  try {
    if (!request.is_multipart_form_data() ||
        !request.has_file("base_log") ||
        !request.has_file("incoming_log") ||
        !request.has_file("case_relations")) {
      throw std::invalid_argument(
          "required multipart fields: base_log, incoming_log, case_relations");
    }
    static const std::set<std::string> allowed = {
        "merge_variant", "merge_timestamp_key", "merge_left_column", "merge_right_column"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument("unknown merge parameter: " + key);
    }
    const auto& base_file = request.get_file_value("base_log");
    const auto& incoming_file = request.get_file_value("incoming_log");
    const auto& relation_file = request.get_file_value("case_relations");
    std::istringstream base_input(base_file.content);
    std::istringstream incoming_input(incoming_file.content);
    std::istringstream relation_input(relation_file.content);
    const auto base = looks_like_xes(base_file.filename, base_file.content)
                          ? minera::import_xes_event_log(base_input)
                          : minera::import_csv_event_log(base_input);
    const auto incoming =
        looks_like_xes(incoming_file.filename, incoming_file.content)
            ? minera::import_xes_event_log(incoming_input)
            : minera::import_csv_event_log(incoming_input);
    const auto value = [&](std::string_view key,
                           std::string fallback) {
      return request.has_param(std::string(key))
                 ? request.get_param_value(std::string(key))
                 : std::move(fallback);
    };
    const auto timestamp_key = value("merge_timestamp_key", "time:timestamp");
    const auto left_column = value("merge_left_column", "case_id_LEFT");
    const auto right_column = value("merge_right_column", "case_id_RIGHT");
    minera::CaseRelationsOptions options;
    const auto variant = value("merge_variant", "pandas");
    if (variant != "pandas" && variant != "PANDAS")
      throw std::invalid_argument("merge_variant: expected pandas");
    options.timestamp_key = timestamp_key;
    const auto relations = minera::parse_case_relations_csv(
        relation_input, left_column, right_column);
    response.set_content(
        minera::case_relations_json(minera::merge_case_relations(
            base, incoming, relations, options), options.variant),
        "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_timestamp_interleavings(const httplib::Request& request,
                                    httplib::Response& response) {
  try {
    if (!request.is_multipart_form_data() || !request.has_file("left") ||
        !request.has_file("right") || !request.has_file("relations")) {
      throw std::invalid_argument(
          "required multipart fields: left, right, relations");
    }
    static const std::set<std::string> allowed = {
        "interleavings_timestamp_key", "interleavings_max_results",
        "interleavings_left_column", "interleavings_right_column"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument(
            "unknown timestamp interleavings parameter: " + key);
    }
    const auto& left_file = request.get_file_value("left");
    const auto& right_file = request.get_file_value("right");
    const auto& relation_file = request.get_file_value("relations");
    std::istringstream left_input(left_file.content);
    std::istringstream right_input(right_file.content);
    std::istringstream relation_input(relation_file.content);
    const auto left = looks_like_xes(left_file.filename, left_file.content)
                          ? minera::import_xes_event_log(left_input)
                          : minera::import_csv_event_log(left_input);
    const auto right = looks_like_xes(right_file.filename, right_file.content)
                           ? minera::import_xes_event_log(right_input)
                           : minera::import_csv_event_log(right_input);
    const auto value = [&](std::string_view key, std::string fallback) {
      return request.has_param(std::string(key))
                 ? request.get_param_value(std::string(key))
                 : std::move(fallback);
    };
    const auto left_column =
        value("interleavings_left_column", "case_id_LEFT");
    const auto right_column =
        value("interleavings_right_column", "case_id_RIGHT");
    const auto relations = minera::parse_case_relations_csv(
        relation_input, left_column, right_column);
    const auto lookup = [&](std::string_view key) {
      return request.has_param(std::string(key))
                 ? request.get_param_value(std::string(key))
                 : std::string{};
    };
    const auto present = [&](std::string_view key) {
      return request.has_param(std::string(key));
    };
    const auto options =
        minera::parse_timestamp_interleavings_params(lookup, present);
    response.set_content(
        minera::timestamp_interleavings_json(
            minera::discover_timestamp_interleavings(left, right, relations,
                                                     options)),
        "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_random_tree(const httplib::Request& request,
                        httplib::Response& response) {
  try {
    static const std::set<std::string> allowed = {
        "random_tree_variant",       "random_tree_rec_depth",
        "random_tree_min_rec_depth", "random_tree_max_rec_depth",
        "random_tree_prob_leaf",     "random_tree_mode",
        "random_tree_min",           "random_tree_max",
        "random_tree_sequence",      "random_tree_choice",
        "random_tree_parallel",      "random_tree_loop",
        "random_tree_or",             "random_tree_silent",
        "random_tree_duplicate",      "random_tree_no_models",
        "random_tree_seed",           "random_tree_activities"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument("unknown random-tree parameter: " + key);
    }
    const auto lookup = [&](std::string_view name) {
      const auto key = std::string(name);
      return request.has_param(key) ? request.get_param_value(key)
                                    : std::string{};
    };
    const auto present = [&](std::string_view name) {
      return request.has_param(std::string(name));
    };
    const auto params = minera::parse_random_tree_params(lookup, present);
    response.set_content(
        minera::random_tree_json(minera::generate_random_process_tree(params),
                                 params.variant),
        "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

// Compares Petri-net element usage between the cached session log and an
// uploaded reference log. The multipart field `reference_log` is required;
// `miner` is optional and defaults to inductive.
void handle_compare_petri(const httplib::Request& request,
                          httplib::Response& response) {
  std::shared_ptr<const MinedSession> session;
  {
    std::lock_guard<std::mutex> lock(g_session_mutex);
    session = g_session;
  }
  if (!session) {
    response.status = 409;
    response.set_content(error_json("no mined log; mine a log first"),
                         "application/json");
    return;
  }
  try {
    if (!request.has_file("reference_log")) {
      response.status = 400;
      response.set_content(error_json("missing form field: reference_log"),
                           "application/json");
      return;
    }
    const auto& reference_file = request.get_file_value("reference_log");
    std::string miner = request.has_param("miner") ? request.get_param_value("miner") : "";
    if (miner.empty() && request.has_file("miner")) miner = request.get_file_value("miner").content;
    if (miner.empty()) miner = "inductive";
    const auto it = session->nets.find(miner);
    if (it == session->nets.end()) {
      response.status = 400;
      response.set_content(error_json("unknown miner: " + miner),
                           "application/json");
      return;
    }
    constexpr std::size_t kMaxComparisonTraces = 10'000;
    constexpr std::size_t kMaxComparisonEvents = 1'000'000;
    constexpr std::size_t kMaxComparisonPlaces = 50'000;
    constexpr std::size_t kMaxComparisonTransitions = 10'000;
    constexpr std::size_t kMaxComparisonWork = 500'000'000;
    constexpr std::size_t kSilentRoutingStateCap = 4'096;
    minera::EventLog reference;
    if (minera::looks_like_xes(reference_file.filename, reference_file.content)) {
      std::istringstream input(reference_file.content);
      reference = minera::import_xes_event_log(input);
    } else {
      std::istringstream input(reference_file.content);
      reference = minera::import_csv_event_log(input);
    }
    if (session->log.trace_count() > kMaxComparisonTraces ||
        session->log.event_count() > kMaxComparisonEvents ||
        reference.trace_count() > kMaxComparisonTraces ||
        reference.event_count() > kMaxComparisonEvents ||
        it->second.place_count() > kMaxComparisonPlaces ||
        it->second.transitions().size() > kMaxComparisonTransitions) {
      throw std::invalid_argument(
          "Petri usage comparison log exceeds the server safety limits");
    }
    const auto replay_units = session->log.event_count() +
                              reference.event_count() +
                              session->log.trace_count() + reference.trace_count();
    const auto worst_case_scan = replay_units * kSilentRoutingStateCap;
    if (worst_case_scan != 0 &&
        it->second.transitions().size() > kMaxComparisonWork / worst_case_scan) {
      throw std::invalid_argument(
          "Petri usage comparison exceeds the server work budget");
    }
    const auto comparison = minera::compare_petri_usage(it->second,
                                                         session->log,
                                                         reference);
    response.set_content(minera::petri_usage_comparison_json(comparison),
                         "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_evaluate(const httplib::Request& request,
                     httplib::Response& response) {
  std::shared_ptr<const MinedSession> session;
  {
    std::lock_guard<std::mutex> lock(g_session_mutex);
    session = g_session;
  }
  if (!session) {
    response.status = 409;
    response.set_content(error_json("no mined log; mine a log first"),
                         "application/json");
    return;
  }

  const std::string miner = request.get_param_value("miner");
  const auto it = session->nets.find(miner);
  if (it == session->nets.end()) {
    response.status = 400;
    response.set_content(error_json("unknown miner: " + miner),
                         "application/json");
    return;
  }

  try {
    const auto lookup = [&](std::string_view name) {
      const auto key = std::string(name);
      return request.has_param(key) ? request.get_param_value(key)
                                    : std::string{};
    };
    const auto present = [&](std::string_view name) {
      return request.has_param(std::string(name));
    };
    const auto params = minera::parse_evaluation_params(lookup, present);
    minera::ModelEvaluation result;
    result.miner = miner;
    switch (params.family) {
      case minera::EvaluationFamily::kReplayFitness:
        result.replay_fitness = minera::evaluate_replay_fitness(
            session->log, it->second, params.replay_fitness_variant);
        break;
      case minera::EvaluationFamily::kPrecision:
        result.precision = minera::evaluate_precision(
            session->log, it->second, params.precision_variant,
            params.precision_options);
        break;
      case minera::EvaluationFamily::kGeneralization:
        result.generalization = minera::evaluate_generalization(
            session->log, it->second, params.generalization_variant);
        break;
      case minera::EvaluationFamily::kSimplicity:
        result.simplicity = minera::evaluate_simplicity(
            it->second, params.simplicity_variant, params.simplicity_options);
        break;
    }
    response.set_content(
        minera::model_evaluation_json(result, &session->graph),
        "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_petri_analysis(const httplib::Request& request,
                           httplib::Response& response) {
  std::shared_ptr<const MinedSession> session;
  {
    std::lock_guard<std::mutex> lock(g_session_mutex);
    session = g_session;
  }
  if (!session) {
    response.status = 409;
    response.set_content(error_json("no mined log; mine a log first"),
                         "application/json");
    return;
  }
  try {
    const auto lookup = [&](std::string_view name) {
      const auto key = std::string(name);
      return request.has_param(key) ? request.get_param_value(key)
                                    : std::string{};
    };
    const auto present = [&](std::string_view name) {
      return request.has_param(std::string(name));
    };
    const auto params = minera::parse_petri_analysis_params(lookup, present);
    const auto it = session->nets.find(params.miner);
    if (it == session->nets.end())
      throw std::invalid_argument("petri_analysis_miner: unknown model '" +
                                  params.miner + "'");
    const auto& net = it->second;
    switch (params.family) {
      case minera::PetriAnalysisFamily::kMarkingEquation: {
        minera::MarkingEquationOptions options;
        options.initial_marking = params.initial_marking;
        options.final_marking = params.final_marking;
        options.transition_costs = params.transition_costs;
        options.realizability_limit = params.max_states;
        response.set_content(minera::petri_analysis_json(
                                 minera::solve_marking_equation(net, options)),
                             "application/json");
        return;
      }
      case minera::PetriAnalysisFamily::kExtendedMarkingEquation: {
        if (params.trace_index >= session->log.traces.size())
          throw std::invalid_argument(
              "petri_analysis_trace_index: out of range");
        minera::ExtendedMarkingEquationOptions options;
        options.initial_marking = params.initial_marking;
        options.final_marking = params.final_marking;
        options.model_transition_costs = params.model_transition_costs;
        options.synchronous_transition_costs =
            params.synchronous_transition_costs;
        options.log_move_costs = params.log_move_costs;
        options.split_indices = params.split_indices;
        options.max_k_value = params.max_k_value;
        options.full_bootstrap_required = params.full_bootstrap_required;
        options.realizability_limit = params.max_states;
        std::vector<std::string> trace;
        for (const auto& event : session->log.traces[params.trace_index].events)
          trace.push_back(event.activity);
        response.set_content(minera::petri_analysis_json(
                                 minera::solve_extended_marking_equation(
                                     trace, net, options)),
                             "application/json");
        return;
      }
      case minera::PetriAnalysisFamily::kWorkflowNet:
        response.set_content(
            minera::petri_analysis_json(minera::check_workflow_net(net)),
            "application/json");
        return;
      case minera::PetriAnalysisFamily::kSoundness:
      case minera::PetriAnalysisFamily::kWoflan: {
        minera::SoundnessOptions options;
        options.variant = params.family == minera::PetriAnalysisFamily::kWoflan
                              ? minera::SoundnessVariant::kWoflan
                              : minera::SoundnessVariant::kSoundness;
        options.initial_marking = params.initial_marking;
        options.final_marking = params.final_marking;
        options.max_states = params.max_states;
        options.diagnostics = params.diagnostics;
        options.return_asap_when_unsound = params.return_asap_when_unsound;
        response.set_content(
            minera::petri_analysis_json(minera::analyze_soundness(net, options)),
            "application/json");
        return;
      }
    }
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_evaluate_emd(const httplib::Request& request,
                         httplib::Response& response) {
  std::shared_ptr<const MinedSession> session;
  {
    std::lock_guard<std::mutex> lock(g_session_mutex);
    session = g_session;
  }
  if (!session) {
    response.status = 409;
    response.set_content(error_json("no mined log; mine a log first"),
                         "application/json");
    return;
  }

  try {
    if (!request.is_multipart_form_data() ||
        !request.has_file("reference_log")) {
      throw std::invalid_argument("missing form field: reference_log");
    }
    const auto parse_boolean = [&](std::string_view name, bool fallback) {
      const auto key = std::string(name);
      if (!request.has_param(key)) return fallback;
      const auto value = request.get_param_value(key);
      if (value == "true" || value == "1") return true;
      if (value == "false" || value == "0") return false;
      throw std::invalid_argument(key +
                                  " expects true, false, 1, or 0");
    };
    minera::EmdOptions options;
    options.normalize_masses =
        parse_boolean("normalize_masses", options.normalize_masses);
    options.strict_equal_mass =
        parse_boolean("strict_equal_mass", options.strict_equal_mass);

    const auto& file = request.get_file_value("reference_log");
    std::istringstream input(file.content);
    const auto reference =
        minera::looks_like_xes(file.filename, file.content)
            ? minera::import_xes_event_log(input)
            : minera::import_csv_event_log(input);
    const auto result = minera::evaluate_emd(
        minera::derive_stochastic_language(session->log),
        minera::derive_stochastic_language(reference), options);
    response.set_content(minera::emd_json(result), "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

// On-demand alignment directly against the latest inductive process tree.
void handle_align_process_tree(const httplib::Request& request,
                               httplib::Response& response) {
  std::shared_ptr<const MinedSession> session;
  {
    std::lock_guard<std::mutex> lock(g_session_mutex);
    session = g_session;
  }
  if (!session) {
    response.status = 409;
    response.set_content(error_json("no mined log; mine a log first"),
                         "application/json");
    return;
  }
  try {
    const auto lookup = [&](std::string_view name) {
      const auto key = std::string(name);
      if (request.has_param(key)) return request.get_param_value(key);
      if (request.has_file(key)) return request.get_file_value(key).content;
      return std::string{};
    };
    const auto present = [&](std::string_view name) {
      const auto key = std::string(name);
      return request.has_param(key) || request.has_file(key);
    };
    const auto options = minera::parse_process_tree_alignment_params(lookup, present);
    const auto result = minera::align_process_tree(session->log, session->tree,
                                                   options);
    response.set_content(minera::process_tree_alignment_json(result),
                         "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_align_decomposed(const httplib::Request& request, httplib::Response& response) {
  std::shared_ptr<const MinedSession> session; { std::lock_guard<std::mutex> lock(g_session_mutex); session = g_session; }
  if (!session) { response.status=409; response.set_content(error_json("no mined log; mine a log first"),"application/json"); return; }
  const auto miner = request.get_param_value("miner"); const auto it=session->nets.find(miner);
  if (it==session->nets.end()) { response.status=400; response.set_content(error_json("unknown miner: "+miner),"application/json"); return; }
  try { const auto lookup=[&](std::string_view n){const auto k=std::string(n); return request.has_param(k)?request.get_param_value(k):std::string{};}; const auto present=[&](std::string_view n){return request.has_param(std::string(n));}; const auto o=minera::parse_decomposed_alignment_params(lookup,present); const auto r=minera::align_decomposed(session->log,it->second,o); response.set_content(minera::decomposed_alignment_json(r),"application/json"); }
  catch(const std::exception& e){response.status=400; response.set_content(error_json(e.what()),"application/json");}
}

void handle_align_petri_anti(const httplib::Request& request, httplib::Response& response) {
  std::shared_ptr<const MinedSession> session; { std::lock_guard<std::mutex> lock(g_session_mutex); session = g_session; }
  if (!session) { response.status=409; response.set_content(error_json("no mined log; mine a log first"),"application/json"); return; }
  const auto miner=request.get_param_value("miner"); const auto it=session->nets.find(miner); if(it==session->nets.end()){response.status=400;response.set_content(error_json("unknown miner: "+miner),"application/json");return;}
  try { const auto lookup=[&](std::string_view n){const auto k=std::string(n);return request.has_param(k)?request.get_param_value(k):std::string{};}; const auto present=[&](std::string_view n){return request.has_param(std::string(n));}; const auto o=minera::parse_petri_anti_alignment_params(lookup,present); const auto r=minera::anti_align_petri(it->second,session->log,o); response.set_content(minera::petri_anti_alignment_json(r),"application/json"); } catch(const std::exception&e){response.status=400;response.set_content(error_json(e.what()),"application/json");}
}

void handle_align_petri_multi(const httplib::Request& request, httplib::Response& response) {
  std::shared_ptr<const MinedSession> session; { std::lock_guard<std::mutex> lock(g_session_mutex); session = g_session; }
  if (!session) { response.status=409; response.set_content(error_json("no mined log; mine a log first"),"application/json"); return; }
  const auto miner=request.get_param_value("miner"); const auto it=session->nets.find(miner); if(it==session->nets.end()){response.status=400;response.set_content(error_json("unknown miner: "+miner),"application/json");return;}
  try { const auto lookup=[&](std::string_view n){const auto k=std::string(n);return request.has_param(k)?request.get_param_value(k):std::string{};}; const auto present=[&](std::string_view n){return request.has_param(std::string(n));}; const auto o=minera::parse_petri_multi_alignment_params(lookup,present); const auto r=minera::multi_align_petri(it->second,session->log,o); response.set_content(minera::petri_multi_alignment_json(r),"application/json"); } catch(const std::exception&e){response.status=400;response.set_content(error_json(e.what()),"application/json");}
}

void handle_reduce_process_tree(const httplib::Request& request, httplib::Response& response) {
  std::shared_ptr<const MinedSession> session; { std::lock_guard<std::mutex> lock(g_session_mutex); session = g_session; }
  if (!session) { response.status=409; response.set_content(error_json("no mined log; mine a log first"),"application/json"); return; }
  try { if(!request.has_param("trace_index")) throw std::invalid_argument("trace_index: expected a non-negative integer"); const auto text=request.get_param_value("trace_index"); std::size_t idx=0; const auto r=std::from_chars(text.data(),text.data()+text.size(),idx); if(r.ec!=std::errc{}||r.ptr!=text.data()+text.size()||idx>=session->log.traces.size()) throw std::invalid_argument("trace_index: out of range"); const auto lookup=[&](std::string_view n){const auto k=std::string(n);return request.has_param(k)?request.get_param_value(k):std::string{};}; const auto present=[&](std::string_view n){return request.has_param(std::string(n));}; const auto o=minera::parse_process_tree_reduction_params(lookup,present); const auto reduced=minera::reduce_process_tree(session->tree,session->log.traces[idx],o); response.set_content(minera::process_tree_reduction_json(reduced,session->graph,idx),"application/json"); } catch(const std::exception&e){response.status=400;response.set_content(error_json(e.what()),"application/json");}
}

void handle_align_dfg(const httplib::Request& request, httplib::Response& response) {
  std::shared_ptr<const MinedSession> session; { std::lock_guard<std::mutex> lock(g_session_mutex); session = g_session; }
  if (!session) { response.status = 409; response.set_content(error_json("no mined log; mine a log first"), "application/json"); return; }
  try {
    const auto lookup = [&](std::string_view n) { const auto key=std::string(n); if (request.has_param(key)) return request.get_param_value(key); if (request.has_file(key)) return request.get_file_value(key).content; return std::string{}; };
    const auto present = [&](std::string_view n) { const auto key=std::string(n); return request.has_param(key) || request.has_file(key); };
    auto options = minera::parse_dfg_alignment_params(lookup, present);
    const auto projection = present("projection") ? lookup("projection") : std::string("none");
    if (projection != "none" && projection != "log" && projection != "metrics") throw std::invalid_argument("projection expects none, log, or metrics");
    minera::DfgStatisticalOptions dopt; dopt.window = 1; const auto dfg = minera::discover_frequency_dfg(session->log, dopt);
    std::map<std::string,std::uint64_t> starts, ends; for (const auto& t:session->log.traces) if(!t.events.empty()){++starts[t.events.front().activity];++ends[t.events.back().activity];}
    const auto result = minera::align_log_on_dfg(session->log, dfg, starts, ends, options);
    if (projection == "log") { const auto projected = minera::project_log_on_dfg(session->log, dfg, starts, ends, options); response.set_content(minera::dfg_alignment_json(result, nullptr, &projected), "application/json"); }
    else if (projection == "metrics") { const auto metrics = minera::project_alignments_on_dfg(session->log, dfg, starts, ends, options); response.set_content(minera::dfg_alignment_json(result, &metrics), "application/json"); }
    else response.set_content(minera::dfg_alignment_json(result), "application/json");
  } catch (const std::exception& error) { response.status = 400; response.set_content(error_json(error.what()), "application/json"); }
}

void handle_align_edit_distance(const httplib::Request& request, httplib::Response& response) {
  std::shared_ptr<const MinedSession> session; { std::lock_guard<std::mutex> lock(g_session_mutex); session = g_session; }
  if (!session) { response.status = 409; response.set_content(error_json("no mined log; mine a log first"), "application/json"); return; }
  try {
    if (!request.is_multipart_form_data() || !request.has_file("reference_log")) throw std::invalid_argument("missing form field: reference_log");
    const auto& file = request.get_file_value("reference_log");
    std::stringstream input(file.content);
    const auto reference = minera::looks_like_xes(file.filename, file.content) ? minera::import_xes_event_log(input) : minera::import_csv_event_log(input);
    const auto lookup = [&](std::string_view n) { const auto key=std::string(n); if (request.has_param(key)) return request.get_param_value(key); if (request.has_file(key)) return request.get_file_value(key).content; return std::string{}; };
    const auto present = [&](std::string_view n) { const auto key=std::string(n); return request.has_param(key) || request.has_file(key); };
    const auto options = minera::parse_edit_distance_params(lookup, present);
    const auto projection = present("projection") ? lookup("projection") : std::string("none");
    if (projection != "none" && projection != "log") throw std::invalid_argument("projection expects none or log");
    const auto result = minera::align_edit_distance(session->log, reference, options);
    if (projection == "log") { const auto projected = minera::project_log_on_alignments(session->log, result); response.set_content(minera::edit_distance_json(result, options.perform_anti_alignment, &projected), "application/json"); }
    else response.set_content(minera::edit_distance_json(result, options.perform_anti_alignment), "application/json");
  } catch (const std::exception& error) { response.status = 400; response.set_content(error_json(error.what()), "application/json"); }
}

minera::DecisionMiningOptions decision_mining_options(
    const httplib::Request& request) {
  static const std::set<std::string> allowed = {
      "miner", "decision_max_depth", "decision_min_split",
      "decision_min_leaf", "decision_max_categories",
      "decision_max_thresholds", "decision_min_gain",
      "decision_include_trace_attributes", "decision_attributes"};
  for (const auto& [key, value] : request.params) {
    (void)value;
    if (!allowed.contains(key))
      throw std::invalid_argument("unknown decision parameter: " + key);
  }
  minera::DecisionMiningOptions options;
  options.max_depth = ocel_query_size(request, "decision_max_depth",
                                      options.max_depth);
  if (options.max_depth > 64)
    throw std::invalid_argument("decision_max_depth must be at most 64");
  options.min_split = ocel_query_size(request, "decision_min_split",
                                       options.min_split);
  options.min_leaf = ocel_query_size(request, "decision_min_leaf",
                                      options.min_leaf);
  options.max_categories = ocel_query_size(
      request, "decision_max_categories", options.max_categories);
  options.max_thresholds = ocel_query_size(
      request, "decision_max_thresholds", options.max_thresholds);
  options.min_gain = ocel_query_number(request, "decision_min_gain",
                                       options.min_gain);
  options.include_trace_attributes = ocel_query_bool(
      request, "decision_include_trace_attributes",
      options.include_trace_attributes);
  if (request.has_param("decision_attributes"))
    options.attributes = ocel_query_list(request, "decision_attributes");
  return options;
}

// On-demand decision mining of one model from the most recent mine: which
// attribute values steer a case down which branch of the net. Expects a
// `miner` query parameter like /api/align.
void handle_decide(const httplib::Request& request,
                   httplib::Response& response) {
  std::shared_ptr<const MinedSession> session;
  {
    std::lock_guard<std::mutex> lock(g_session_mutex);
    session = g_session;
  }
  if (!session) {
    response.status = 409;  // nothing mined yet this run
    response.set_content(error_json("no mined log; mine a log first"),
                         "application/json");
    return;
  }

  const std::string miner = request.get_param_value("miner");
  const auto it = session->nets.find(miner);
  if (it == session->nets.end()) {
    response.status = 400;
    response.set_content(error_json("unknown miner: " + miner),
                         "application/json");
    return;
  }

  try {
    const auto options = decision_mining_options(request);
    auto decisions = minera::mine_decisions(session->log, it->second, options);
    decisions.miner = miner;
    response.set_content(minera::decisions_json(decisions),
                         "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

// Exports one model from the most recent mine as standard PNML. Expects a
// `miner` query parameter ("inductive" | "alpha" | "heuristic" | "powl").
void handle_pnml(const httplib::Request& request, httplib::Response& response) {
  std::shared_ptr<const MinedSession> session;
  {
    std::lock_guard<std::mutex> lock(g_session_mutex);
    session = g_session;
  }
  if (!session) {
    response.status = 409;  // nothing mined yet this run
    response.set_content(error_json("no mined log; mine a log first"),
                         "application/json");
    return;
  }

  const std::string miner = request.get_param_value("miner");
  const auto it = session->nets.find(miner);
  if (it == session->nets.end()) {
    response.status = 400;
    response.set_content(error_json("unknown miner: " + miner),
                         "application/json");
    return;
  }

  response.set_content(minera::to_pnml(it->second, miner + " net"),
                       "application/xml");
}

// Exports one model from the most recent mine as BPMN 2.0. The `miner` query
// parameter picks it (default "inductive"). The inductive model converts from
// its block-structured process tree, which maps cleanly onto BPMN; any other
// model ("alpha" | "heuristic" | "powl" | "imported") converts from its workflow net
// by the net's token semantics.
// Exports the most recent mine's inductive process tree as PTML — the only
// miner producing a block-structured tree, so no `miner` parameter.
void handle_ptml(const httplib::Request&, httplib::Response& response) {
  std::shared_ptr<const MinedSession> session;
  {
    std::lock_guard<std::mutex> lock(g_session_mutex);
    session = g_session;
  }
  if (!session) {
    response.status = 409;  // nothing mined yet this run
    response.set_content(error_json("no mined log; mine a log first"),
                         "application/json");
    return;
  }
  response.set_content(minera::to_ptml(session->tree, session->dataset),
                       "application/xml");
}

void handle_bpmn(const httplib::Request& request, httplib::Response& response) {
  std::shared_ptr<const MinedSession> session;
  {
    std::lock_guard<std::mutex> lock(g_session_mutex);
    session = g_session;
  }
  if (!session) {
    response.status = 409;  // nothing mined yet this run
    response.set_content(error_json("no mined log; mine a log first"),
                         "application/json");
    return;
  }

  const std::string miner = request.has_param("miner")
                                ? request.get_param_value("miner")
                                : std::string("inductive");
  if (miner == "inductive") {
    response.set_content(minera::to_bpmn(session->tree, "process"),
                         "application/xml");
    return;
  }
  const auto it = session->nets.find(miner);
  if (it == session->nets.end()) {
    response.status = 400;
    response.set_content(error_json("unknown miner: " + miner),
                         "application/json");
    return;
  }
  response.set_content(minera::to_bpmn(it->second, "process"),
                       "application/xml");
}

void handle_monte_carlo_json(const httplib::Request& request,
                             httplib::Response& response) {
  std::shared_ptr<const MinedSession> session;
  {
    std::lock_guard<std::mutex> lock(g_session_mutex);
    session = g_session;
  }
  if (!session) {
    response.status = 409;
    response.set_content(error_json("no mined log; mine a log first"),
                         "application/json");
    return;
  }
  const std::string miner = request.has_param("miner")
                                ? request.get_param_value("miner")
                                : std::string("inductive");
  const std::string selected_miner =
      miner == "monte_carlo" ? "inductive" : miner;
  const auto it = session->nets.find(selected_miner);
  if (it == session->nets.end()) {
    response.status = 400;
    response.set_content(error_json("unknown miner: " + selected_miner),
                         "application/json");
    return;
  }
  const auto reject = [&](std::string message) {
    response.status = 400;
    response.set_content(error_json(message), "application/json");
  };
  try {
    reject_unknown_monte_carlo_params(request);
    const auto lookup = [&](std::string_view name) {
      const std::string key(name);
      return request.has_param(key) ? request.get_param_value(key)
                                    : std::string{};
    };
    const auto present = [&](std::string_view name) {
      return request.has_param(std::string(name));
    };
    const auto options = minera::parse_monte_carlo_params(lookup, present);
    if (options.simulations > 10000 || options.max_steps > 10000 ||
        options.default_resources_per_place > 1000) {
      reject("Monte Carlo bounds exceed the server safety limits");
      return;
    }
    constexpr std::size_t kMaxSimulationSteps = 1'000'000;
    if (options.simulations != 0 && options.max_steps != 0 &&
        options.simulations > kMaxSimulationSteps / options.max_steps) {
      reject("Monte Carlo simulation budget exceeds the server safety limit");
      return;
    }
    const auto result =
        minera::monte_carlo(session->log, it->second, options);
    response.set_content(minera::monte_carlo_json(result), "application/json");
  } catch (const std::exception& error) {
    reject(error.what());
  }
}

// Returns the deterministic encoded decision tables for one cached model.
void handle_decision_tables(const httplib::Request& request,
                            httplib::Response& response) {
  std::shared_ptr<const MinedSession> session;
  {
    std::lock_guard<std::mutex> lock(g_session_mutex);
    session = g_session;
  }
  if (!session) {
    response.status = 409;
    response.set_content(error_json("no mined log; mine a log first"),
                         "application/json");
    return;
  }

  const std::string miner = request.has_param("miner")
                                ? request.get_param_value("miner")
                                : std::string("inductive");
  const auto it = session->nets.find(miner);
  if (it == session->nets.end()) {
    response.status = 400;
    response.set_content(error_json("unknown miner: " + miner),
                         "application/json");
    return;
  }
  try {
    const auto mining = decision_mining_options(request);
    minera::DecisionTableOptions options;
    options.mining = mining;
    options.include_trace_attributes = mining.include_trace_attributes;
    options.attributes = mining.attributes;
    const auto result =
        minera::mine_decision_tables(session->log, it->second, options);
    response.set_content(minera::serialize_decision_tables_json(result),
                         "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

// Materializes the learned decision rules as guards on a workflow net from
// the most recent mine. The response keeps the decision-tree diagnostics and
// emits a deterministic data-Petri-net model envelope.
void handle_data_petri_net(const httplib::Request& request,
                           httplib::Response& response) {
  std::shared_ptr<const MinedSession> session;
  {
    std::lock_guard<std::mutex> lock(g_session_mutex);
    session = g_session;
  }
  if (!session) {
    response.status = 409;
    response.set_content(error_json("no mined log; mine a log first"),
                         "application/json");
    return;
  }
  const std::string miner = request.has_param("miner")
                                ? request.get_param_value("miner")
                                : std::string("inductive");
  const auto it = session->nets.find(miner);
  if (it == session->nets.end()) {
    response.status = 400;
    response.set_content(error_json("unknown miner: " + miner),
                         "application/json");
    return;
  }
  try {
    minera::DataPetriNetOptions options;
    options.decision_options = decision_mining_options(request);
    const auto result =
        minera::mine_data_petri_net(session->log, it->second, options);
    response.set_content(minera::serialize_data_petri_net_json(result, miner),
                         "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

// Simulates one model from the most recent mine into a synthetic event log,
// served as XES. The `miner` query parameter picks a Petri model
// ("inductive" | "alpha" | "heuristic" | "powl" | "imported", default
// inductive) or the classic Declare model ("declare"). `traces` defaults to
// 100 for Petri models and 1000 for Declare, capped at 10000. Declare also
// accepts min_length, max_length, and seed; process_tree accepts variant,
// traces, max_steps, seed, min_length, max_length, max_loop_occurrences and
// max_trace_cap; mode=stochastic selects weighted Petri playout and accepts
// stochastic_traces, stochastic_max_steps, stochastic_seed,
// stochastic_transition_weights, and timing controls; mode=monte_carlo selects
// the bounded virtual-time simulator and accepts monte_carlo_* controls.
void handle_playout(const httplib::Request& request,
                    httplib::Response& response) {
  std::shared_ptr<const MinedSession> session;
  {
    std::lock_guard<std::mutex> lock(g_session_mutex);
    session = g_session;
  }
  if (!session) {
    response.status = 409;  // nothing mined yet this run
    response.set_content(error_json("no mined log; mine a log first"),
                         "application/json");
    return;
  }

  const std::string miner = request.has_param("miner")
                                ? request.get_param_value("miner")
                                : std::string("inductive");
  const bool is_declare = miner == "declare";
  const std::string mode = request.has_param("mode")
                               ? request.get_param_value("mode")
                               : std::string{};
  const bool is_stochastic = mode == "stochastic" || miner == "stochastic";
  const bool is_monte_carlo =
      mode == "monte_carlo" || miner == "monte_carlo";
  if (is_monte_carlo) {
    const std::string selected_miner =
        miner == "monte_carlo" ? "inductive" : miner;
    const auto it = session->nets.find(selected_miner);
    if (it == session->nets.end()) {
      response.status = 400;
      response.set_content(error_json("unknown miner: " + selected_miner),
                           "application/json");
      return;
    }
    const auto reject = [&](std::string message) {
      response.status = 400;
      response.set_content(error_json(message), "application/json");
    };
    try {
      reject_unknown_monte_carlo_params(request);
      const auto lookup = [&](std::string_view name) {
        const std::string key(name);
        return request.has_param(key) ? request.get_param_value(key)
                                      : std::string{};
      };
      const auto present = [&](std::string_view name) {
        return request.has_param(std::string(name));
      };
      const auto options = minera::parse_monte_carlo_params(lookup, present);
      if (options.simulations > 10000 || options.max_steps > 10000 ||
          options.default_resources_per_place > 1000) {
        reject("Monte Carlo bounds exceed the server safety limits");
        return;
      }
      constexpr std::size_t kMaxSimulationSteps = 1'000'000;
      if (options.simulations != 0 && options.max_steps != 0 &&
          options.simulations > kMaxSimulationSteps / options.max_steps) {
        reject("Monte Carlo simulation budget exceeds the server safety limit");
        return;
      }
      const auto result =
          minera::monte_carlo(session->log, it->second, options);
      response.set_content(
          minera::to_xes(result.log, selected_miner + " Monte Carlo playout"),
          "application/xml");
    } catch (const std::exception& error) {
      reject(error.what());
    }
    return;
  }
  if (is_stochastic) {
    const std::string selected_miner = miner == "stochastic" ? "inductive" : miner;
    const auto it = session->nets.find(selected_miner);
    if (it == session->nets.end()) {
      response.status = 400;
      response.set_content(error_json("unknown miner: " + selected_miner),
                           "application/json");
      return;
    }
    const auto reject = [&](std::string message) {
      response.status = 400;
      response.set_content(error_json(message), "application/json");
    };
    try {
      const auto lookup = [&](std::string_view name) {
        const std::string key(name);
        return request.has_param(key) ? request.get_param_value(key) : std::string{};
      };
      const auto present = [&](std::string_view name) {
        return request.has_param(std::string(name));
      };
      auto options = minera::parse_stochastic_playout_params(lookup, present);
      if (options.traces > 10000 || options.max_steps > 10000) {
        reject("stochastic Petri playout bounds exceed the server safety limits");
        return;
      }
      constexpr std::size_t kMaxGeneratedEvents = 1'000'000;
      if (options.traces != 0 && options.max_steps != 0 &&
          options.traces > kMaxGeneratedEvents / options.max_steps) {
        reject("stochastic Petri playout event budget exceeds the server "
               "safety limit");
        return;
      }
      const auto log = minera::stochastic_playout(it->second, options);
      response.set_content(minera::to_xes(log, selected_miner + " stochastic playout"),
                           "application/xml");
    } catch (const std::exception& error) {
      reject(error.what());
    }
    return;
  }
  if (miner == "process_tree") {
    const auto lookup = [&](std::string_view name) {
      const std::string key(name.substr(std::string_view("playout_").size()));
      return request.has_param(key) ? request.get_param_value(key) : std::string{};
    };
    const auto present = [&](std::string_view name) {
      const std::string key(name.substr(std::string_view("playout_").size()));
      return request.has_param(key);
    };
    const auto reject = [&](std::string message) {
      response.status = 400;
      response.set_content(error_json(message), "application/json");
    };
    try {
      auto options = minera::parse_process_tree_playout_params(lookup, present);
      if (options.traces > 10000 || options.max_steps > 10000 ||
          options.max_loop_occurrences > 100 ||
          (request.has_param("max_length") && options.max_length > 64)) {
        reject("process-tree playout bounds exceed the server safety limits");
        return;
      }
      // The native extensive default is intentionally generous for local
      // callers. The server uses a bounded default when the client did not
      // provide a cap, so an ordinary default request remains usable.
      if (!request.has_param("max_length")) options.max_length = 64;
      if (options.variant == minera::ProcessTreePlayoutVariant::kExtensive &&
          options.max_traces > 10000) {
        if (request.has_param("max_trace_cap") ||
            request.has_param("max_traces")) {
          reject("max_trace_cap must not exceed 10000");
          return;
        }
        options.max_traces = 10000;
      }
      if (options.variant == minera::ProcessTreePlayoutVariant::kExtensive &&
          options.max_length > 64) {
        reject("max_length must not exceed 64");
        return;
      }
      const auto log = minera::process_tree_playout(session->tree, options);
      response.set_content(minera::to_xes(log, "process_tree playout"),
                           "application/xml");
    } catch (const std::exception& error) {
      reject(error.what());
    }
    return;
  }
  const auto it = session->nets.find(miner);
  if (miner == "dfg") {
    const auto reject = [&](std::string message) {
      response.status = 400;
      response.set_content(error_json(message), "application/json");
    };
    try {
      const auto lookup = [&](std::string_view name) {
        const auto key = std::string(name);
        return request.has_param(key) ? request.get_param_value(key)
                                      : std::string{};
      };
      const auto present = [&](std::string_view name) {
        return request.has_param(std::string(name));
      };
      auto options = minera::parse_dfg_playout_params(lookup, present);
      if (options.traces > 10000 || options.max_events_per_trace > 10000 ||
          options.max_variants > 100000 ||
          options.max_occurrences_per_activity > 100 ||
          options.max_execution_time_ms > 600000) {
        reject("DFG playout bounds exceed the server safety limits");
        return;
      }
      constexpr std::size_t kMaxGeneratedEvents = 1'000'000;
      if (options.variant == minera::DfgPlayoutVariant::kPerformance &&
          options.traces != 0 && options.max_events_per_trace != 0 &&
          options.traces > kMaxGeneratedEvents /
                              options.max_events_per_trace) {
        reject("DFG performance playout event budget exceeds the server "
               "safety limit");
        return;
      }
      // A caller may omit the native wall-clock bound. Keep the local
      // endpoint responsive on highly cyclic graphs while preserving an
      // explicit zero/unbounded setting for direct native callers.
      if (options.max_execution_time_ms == 0)
        options.max_execution_time_ms = 5000;
      minera::DfgStatisticalOptions dfg_options;
      const auto frequency = minera::discover_frequency_dfg(session->log, dfg_options);
      minera::DfgActivityCounts starts, ends;
      for (const auto& trace : session->log.traces) {
        if (!trace.events.empty()) {
          ++starts[trace.events.front().activity];
          ++ends[trace.events.back().activity];
        }
      }
      std::optional<minera::DfgPerformance> performance;
      if (options.variant == minera::DfgPlayoutVariant::kPerformance) {
        performance = minera::discover_performance_dfg(session->log, dfg_options);
      }
      const auto log = minera::dfg_playout(
          frequency, starts, ends, options,
          performance ? &*performance : nullptr);
      response.set_content(minera::to_xes(log, "dfg playout"),
                           "application/xml");
    } catch (const std::exception& error) {
      reject(error.what());
    }
    return;
  }
  if (!is_declare && it == session->nets.end()) {
    response.status = 400;
    response.set_content(error_json("unknown miner: " + miner),
                         "application/json");
    return;
  }

  const auto reject = [&](std::string message) {
    response.status = 400;
    response.set_content(error_json(message), "application/json");
  };
  const auto parse_bounded =
      [&](std::string_view name, std::uint64_t minimum,
          std::uint64_t maximum) -> std::optional<std::uint64_t> {
    const auto value = request.get_param_value(std::string(name));
    std::uint64_t parsed = 0;
    const auto result =
        std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (value.empty() || result.ec != std::errc{} ||
        result.ptr != value.data() + value.size() || parsed < minimum ||
        parsed > maximum) {
      reject(std::string(name) + " must be between " +
             std::to_string(minimum) + " and " + std::to_string(maximum));
      return std::nullopt;
    }
    return parsed;
  };
  if (is_declare) {
    if (mode == "extensive") {
      reject("mode=extensive is not supported for Declare playout");
      return;
    }
    minera::DeclarePlayoutOptions options;
    if (request.has_param("traces")) {
      const auto parsed = parse_bounded("traces", 1, 10000);
      if (!parsed) return;
      options.n_traces = static_cast<int>(*parsed);
    }
    if (request.has_param("min_length")) {
      const auto parsed = parse_bounded("min_length", 0, 1000);
      if (!parsed) return;
      options.min_length = static_cast<int>(*parsed);
    }
    if (request.has_param("max_length")) {
      const auto parsed = parse_bounded("max_length", 0, 1000);
      if (!parsed) return;
      options.max_length = static_cast<int>(*parsed);
    }
    if (request.has_param("seed")) {
      const auto parsed =
          parse_bounded("seed", 0, std::numeric_limits<std::uint64_t>::max());
      if (!parsed) return;
      options.seed = *parsed;
    }
    if (options.min_length > options.max_length) {
      reject("min_length must not exceed max_length");
      return;
    }
    try {
      const auto log = minera::declare_playout(session->declare, options);
      response.set_content(minera::to_xes(log, "declare playout"),
                           "application/xml");
    } catch (const std::exception& error) {
      reject(error.what());
    }
    return;
  }

  if (request.has_param("min_length") || request.has_param("seed") ||
      (request.has_param("max_length") && mode != "extensive")) {
    reject("min_length, max_length, and seed are Declare-only unless "
           "max_length is used with Petri mode=extensive");
    return;
  }
  minera::PlayoutOptions options;
  if (request.has_param("traces")) {
    const auto parsed = parse_bounded("traces", 1, 10000);
    if (!parsed) return;
    options.traces = static_cast<std::size_t>(*parsed);
  }

  // mode=extensive enumerates the model's distinct traces (shortest first,
  // capped by `traces` and `max_length`) instead of sampling randomly.
  if (mode == "extensive") {
    minera::ExtensivePlayoutOptions extensive;
    extensive.max_traces = options.traces;
    if (request.has_param("max_length")) {
      const auto parsed = parse_bounded("max_length", 1, 1000);
      if (!parsed) return;
      extensive.max_length = static_cast<std::size_t>(*parsed);
    }
    const auto log = minera::extensive_playout(it->second, extensive);
    response.set_content(minera::to_xes(log, miner + " playout"),
                         "application/xml");
    return;
  }

  const auto log = minera::playout(it->second, options);
  response.set_content(minera::to_xes(log, miner + " playout"),
                       "application/xml");
}

// Exports the most recently mined event log — after any server-side filtering
// applied at mine time — as standard XES.
void handle_xes(const httplib::Request&, httplib::Response& response) {
  std::shared_ptr<const MinedSession> session;
  {
    std::lock_guard<std::mutex> lock(g_session_mutex);
    session = g_session;
  }
  if (!session) {
    response.status = 409;  // nothing mined yet this run
    response.set_content(error_json("no mined log; mine a log first"),
                         "application/json");
    return;
  }

  response.set_content(minera::to_xes(session->log, session->dataset),
                       "application/xml");
}

void handle_dfg(const httplib::Request& request, httplib::Response& response) {
  std::shared_ptr<const MinedSession> session;
  {
    std::lock_guard<std::mutex> lock(g_session_mutex);
    session = g_session;
  }
  if (!session) {
    response.status = 409;
    response.set_content(error_json("no mined log; mine a log first"),
                         "application/json");
    return;
  }
  try {
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (key != "noise_threshold" && key != "most_common_paths") {
        throw std::invalid_argument("unknown DFG export parameter: " + key);
      }
    }
    auto analysis = minera::to_dfg_analysis(session->graph);
    if (request.has_param("most_common_paths") &&
        !request.has_param("noise_threshold")) {
      throw std::invalid_argument(
          "most_common_paths requires noise_threshold");
    }
    if (request.has_param("noise_threshold")) {
      std::set<std::pair<std::string, std::string>> most_common_paths;
      if (request.has_param("most_common_paths")) {
        for (const auto& pair : minera::parse_activity_pairs(
                 request.get_param_value("most_common_paths"))) {
          most_common_paths.emplace(pair.predecessor, pair.successor);
        }
      }
      const auto filtered = minera::clean_dfg_based_on_noise_threshold(
          minera::make_dfg_filter_data(session->graph),
          ocel_query_number(request, "noise_threshold", 0.0),
          most_common_paths);
      analysis = {filtered.edges, filtered.start_activities,
                  filtered.end_activities};
    }
    response.set_content(minera::to_dfg(analysis), "text/plain");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_dfg_import(const httplib::Request& request,
                       httplib::Response& response) {
  try {
    const auto body = ocel_request_body(request, "dfg");
    if (body.empty()) throw std::invalid_argument("missing DFG text body");
    response.set_content(
        minera::to_dfg(minera::parse_dfg(body)), "text/plain");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_dfg_precision(const httplib::Request& request,
                          httplib::Response& response) {
  std::shared_ptr<const MinedSession> session;
  {
    std::lock_guard<std::mutex> lock(g_session_mutex);
    session = g_session;
  }
  if (!session) {
    response.status = 409;
    response.set_content(error_json("no mined log; mine a log first"),
                         "application/json");
    return;
  }
  try {
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (key != "activity_attribute")
        throw std::invalid_argument("unknown DFG precision parameter: " + key);
    }
    minera::DfgPrecisionOptions options;
    if (request.has_param("activity_attribute"))
      options.activity_attribute = request.get_param_value("activity_attribute");
    if (options.activity_attribute.empty())
      throw std::invalid_argument("activity_attribute must not be empty");
    const auto body = ocel_request_body(request, "dfg");
    if (body.empty()) throw std::invalid_argument("missing DFG text body");
    response.set_content(
        minera::serialize_dfg_precision_json(
            minera::evaluate_dfg_precision(
                session->log, minera::parse_dfg(body), options)),
        "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_dfg_conversion(const httplib::Request& request,
                           httplib::Response& response) {
  try {
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (key != "variant" && key != "artificial_start_activity" &&
          key != "artificial_end_activity") {
        throw std::invalid_argument("unknown DFG conversion parameter: " + key);
      }
    }
    const auto body = ocel_request_body(request, "dfg");
    if (body.empty()) throw std::invalid_argument("missing DFG text body");
    minera::DfgPetriConversionOptions options;
    if (request.has_param("variant")) {
      options.variant = minera::parse_dfg_petri_conversion_variant(
          request.get_param_value("variant"));
    }
    if (request.has_param("artificial_start_activity"))
      options.artificial_start_activity =
          request.get_param_value("artificial_start_activity");
    if (request.has_param("artificial_end_activity"))
      options.artificial_end_activity =
          request.get_param_value("artificial_end_activity");
    const auto net = minera::convert_dfg_to_petri_net(
        minera::parse_dfg(body), options);
    response.set_content(
        minera::serialize_dfg_petri_net_json(net, options.variant),
        "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_streaming_dfg(const httplib::Request& request,
                          httplib::Response& response) {
  try {
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (key != "streaming_case_id_column" &&
          key != "streaming_activity_column") {
        throw std::invalid_argument("unknown streaming DFG parameter: " + key);
      }
    }
    minera::CsvImportOptions import_options;
    if (request.has_param("streaming_case_id_column"))
      import_options.case_id_column =
          request.get_param_value("streaming_case_id_column");
    if (request.has_param("streaming_activity_column"))
      import_options.activity_column =
          request.get_param_value("streaming_activity_column");
    if (import_options.case_id_column.empty() ||
        import_options.activity_column.empty()) {
      throw std::invalid_argument(
          "streaming DFG column names must not be empty");
    }
    const auto body = ocel_request_body(request, "log");
    if (body.empty()) throw std::invalid_argument("missing CSV log body");
    std::istringstream input(body);
    const auto log = minera::import_csv_event_log(input, import_options);
    minera::StreamingDfgDiscovery stream(
        {import_options.activity_column, import_options.case_id_column});
    for (const auto& trace : log.traces)
      for (const auto& event : trace.events)
        stream.receive(event, trace.case_id);
    response.set_content(
        minera::serialize_streaming_dfg_json(stream.snapshot()),
        "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_streaming_footprints(const httplib::Request& request,
                                 httplib::Response& response) {
  try {
    static const std::set<std::string> allowed = {
        "streaming_footprints_case_id_column",
        "streaming_footprints_activity_column",
        "streaming_footprints_activities",
        "streaming_footprints_start_activities",
        "streaming_footprints_end_activities",
        "streaming_footprints_sequence",
        "streaming_footprints_parallel",
        "streaming_footprints_terminate_all"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument(
            "unknown streaming footprints parameter: " + key);
    }
    minera::CsvImportOptions import_options;
    if (request.has_param("streaming_footprints_case_id_column"))
      import_options.case_id_column =
          request.get_param_value("streaming_footprints_case_id_column");
    if (request.has_param("streaming_footprints_activity_column"))
      import_options.activity_column =
          request.get_param_value("streaming_footprints_activity_column");
    if (import_options.case_id_column.empty() ||
        import_options.activity_column.empty()) {
      throw std::invalid_argument(
          "streaming footprints column names must not be empty");
    }
    if (!request.has_param("streaming_footprints_activities"))
      throw std::invalid_argument(
          "streaming footprints activities are required");

    const auto split_list = [](std::string_view text) {
      std::set<std::string> result;
      std::size_t begin = 0;
      while (begin <= text.size()) {
        const auto end = text.find(',', begin);
        auto token = text.substr(
            begin, end == std::string_view::npos ? text.size() - begin
                                                   : end - begin);
        const auto left = token.find_first_not_of(' ');
        const auto right = token.find_last_not_of(' ');
        if (left == std::string_view::npos)
          throw std::invalid_argument(
              "streaming footprints activity lists cannot be empty");
        token = token.substr(left, right - left + 1);
        if (token.empty())
          throw std::invalid_argument(
              "streaming footprints activity lists cannot be empty");
        result.emplace(token);
        if (end == std::string_view::npos) break;
        begin = end + 1;
      }
      return result;
    };
    minera::Footprint model;
    model.start_activities_present =
        request.has_param("streaming_footprints_start_activities");
    model.end_activities_present =
        request.has_param("streaming_footprints_end_activities");
    model.activities = split_list(
        request.get_param_value("streaming_footprints_activities"));
    if (request.has_param("streaming_footprints_start_activities"))
      model.start_activities = split_list(
          request.get_param_value("streaming_footprints_start_activities"));
    if (request.has_param("streaming_footprints_end_activities"))
      model.end_activities = split_list(
          request.get_param_value("streaming_footprints_end_activities"));
    if (request.has_param("streaming_footprints_sequence")) {
      for (const auto& pair : minera::parse_activity_pairs(
               request.get_param_value("streaming_footprints_sequence")))
        model.sequence.emplace(pair.predecessor, pair.successor);
    }
    if (request.has_param("streaming_footprints_parallel")) {
      for (const auto& pair : minera::parse_activity_pairs(
               request.get_param_value("streaming_footprints_parallel")))
        model.parallel.emplace(pair.predecessor, pair.successor);
    }
    const auto body = ocel_request_body(request, "log");
    if (body.empty()) throw std::invalid_argument("missing CSV log body");
    std::istringstream input(body);
    const auto log = minera::import_csv_event_log(input, import_options);
    minera::StreamingFootprintsConformance monitor(
        std::move(model),
        {import_options.activity_column, import_options.case_id_column});
    for (const auto& trace : log.traces)
      for (const auto& event : trace.events)
        (void)monitor.receive(event, trace.case_id);
    if (ocel_query_bool(request, "streaming_footprints_terminate_all", false))
      monitor.terminate_all();
    response.set_content(
        minera::serialize_streaming_footprints_json(monitor.snapshot()),
        "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_streaming_temporal(const httplib::Request& request,
                               httplib::Response& response) {
  try {
    static const std::set<std::string> allowed = {
        "streaming_temporal_profile",
        "streaming_temporal_case_id_column",
        "streaming_temporal_activity_column",
        "streaming_temporal_start_timestamp_column",
        "streaming_temporal_timestamp_column",
        "streaming_temporal_zeta"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument(
            "unknown streaming temporal parameter: " + key);
    }
    if (!request.has_param("streaming_temporal_profile"))
      throw std::invalid_argument("streaming temporal profile is required");
    minera::TemporalProfile profile;
    const auto profile_text =
        request.get_param_value("streaming_temporal_profile");
    std::size_t begin = 0;
    while (begin <= profile_text.size()) {
      const auto end = profile_text.find(',', begin);
      const auto item = profile_text.substr(
          begin, end == std::string::npos ? std::string::npos : end - begin);
      const auto arrow = item.find("->");
      const auto first_colon = item.find(':');
      const auto second_colon = item.find(':', first_colon == std::string::npos
                                                     ? 0
                                                     : first_colon + 1);
      if (arrow == std::string::npos || first_colon == std::string::npos ||
          second_colon == std::string::npos || arrow == 0 ||
          first_colon <= arrow + 2 || second_colon <= first_colon + 1 ||
          second_colon + 1 >= item.size()) {
        throw std::invalid_argument(
            "streaming temporal profile entries must use source->target:mean:stddev");
      }
      char* mean_end = nullptr;
      char* stddev_end = nullptr;
      const auto mean_text = item.substr(first_colon + 1,
                                         second_colon - first_colon - 1);
      const auto stddev_text = item.substr(second_colon + 1);
      const double mean = std::strtod(mean_text.c_str(), &mean_end);
      const double stddev = std::strtod(stddev_text.c_str(), &stddev_end);
      if (mean_end != mean_text.c_str() + mean_text.size() ||
          stddev_end != stddev_text.c_str() + stddev_text.size() ||
          !std::isfinite(mean) || mean < 0.0 || !std::isfinite(stddev) ||
          stddev < 0.0) {
        throw std::invalid_argument(
            "streaming temporal profile timings must be finite and non-negative");
      }
      profile.entries.push_back(
          {item.substr(0, arrow),
           item.substr(arrow + 2, first_colon - arrow - 2), 1, mean, stddev});
      if (end == std::string::npos) break;
      begin = end + 1;
    }
    minera::CsvImportOptions import_options;
    if (request.has_param("streaming_temporal_case_id_column"))
      import_options.case_id_column =
          request.get_param_value("streaming_temporal_case_id_column");
    if (request.has_param("streaming_temporal_activity_column"))
      import_options.activity_column =
          request.get_param_value("streaming_temporal_activity_column");
    if (request.has_param("streaming_temporal_timestamp_column"))
      import_options.timestamp_column =
          request.get_param_value("streaming_temporal_timestamp_column");
    if (import_options.case_id_column.empty() ||
        import_options.activity_column.empty() ||
        import_options.timestamp_column.empty()) {
      throw std::invalid_argument(
          "streaming temporal column names must not be empty");
    }
    minera::StreamingTemporalOptions options;
    options.activity_attribute = import_options.activity_column;
    options.case_id_attribute = import_options.case_id_column;
    options.timestamp_attribute = import_options.timestamp_column;
    if (request.has_param("streaming_temporal_start_timestamp_column"))
      options.start_timestamp_attribute = request.get_param_value(
          "streaming_temporal_start_timestamp_column");
    options.zeta = ocel_query_number(request, "streaming_temporal_zeta",
                                     options.zeta);
    const auto body = ocel_request_body(request, "log");
    if (body.empty()) throw std::invalid_argument("missing CSV log body");
    std::istringstream input(body);
    const auto log = minera::import_csv_event_log(input, import_options);
    minera::StreamingTemporalConformance monitor(std::move(profile), options);
    for (const auto& trace : log.traces)
      for (const auto& event : trace.events)
        (void)monitor.receive(event, trace.case_id);
    response.set_content(
        minera::serialize_streaming_temporal_json(monitor.snapshot()),
        "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_streaming_tbr(const httplib::Request& request,
                          httplib::Response& response) {
  try {
    static const std::set<std::string> allowed = {
        "streaming_tbr_variant", "streaming_tbr_case_id_column",
        "streaming_tbr_activity_column", "streaming_tbr_max_invisible",
        "streaming_tbr_stop_immediately"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument("unknown streaming TBR parameter: " + key);
    }
    if (!request.is_multipart_form_data() || !request.has_file("model") ||
        !request.has_file("log")) {
      throw std::invalid_argument(
          "streaming TBR requires multipart model and log fields");
    }
    minera::StreamingTbrOptions options;
    if (request.has_param("streaming_tbr_variant")) {
      const auto variant = request.get_param_value("streaming_tbr_variant");
      if (variant == "forward")
        options.variant = minera::StreamingTbrVariant::kForward;
      else if (variant == "backwards")
        options.variant = minera::StreamingTbrVariant::kBackwards;
      else
        throw std::invalid_argument(
            "streaming_tbr_variant must be forward or backwards");
    }
    minera::CsvImportOptions import_options;
    if (request.has_param("streaming_tbr_case_id_column"))
      import_options.case_id_column =
          request.get_param_value("streaming_tbr_case_id_column");
    if (request.has_param("streaming_tbr_activity_column"))
      import_options.activity_column =
          request.get_param_value("streaming_tbr_activity_column");
    if (import_options.case_id_column.empty() ||
        import_options.activity_column.empty())
      throw std::invalid_argument(
          "streaming TBR column names must not be empty");
    options.case_id_attribute = import_options.case_id_column;
    options.activity_attribute = import_options.activity_column;
    options.maximum_iterations_invisibles = ocel_query_size(
        request, "streaming_tbr_max_invisible",
        options.maximum_iterations_invisibles);
    options.stop_immediately_unfit =
        ocel_query_bool(request, "streaming_tbr_stop_immediately", false);
    const auto model_body = form_field(request, "model");
    const auto log_body = form_field(request, "log");
    if (model_body.empty() || log_body.empty())
      throw std::invalid_argument("streaming TBR model and log cannot be empty");
    const auto net = minera::parse_pnml(model_body);
    std::istringstream input(log_body);
    const auto log = minera::import_csv_event_log(input, import_options);
    minera::StreamingTokenReplay replay(std::move(net), options);
    for (const auto& trace : log.traces)
      for (const auto& event : trace.events)
        (void)replay.receive(event, trace.case_id);
    response.set_content(
        minera::serialize_streaming_tbr_json(replay.snapshot()),
        "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_process_cube(const httplib::Request& request,
                         httplib::Response& response) {
  try {
    static const std::set<std::string> allowed = {
        "x_col", "x_cols", "y_col", "y_cols", "agg_col", "aggregation",
        "max_divisions_x", "max_divisions_y", "x_bins", "y_bins"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument("unknown process cube parameter: " + key);
    }
    const auto body = ocel_request_body(request, "table");
    if (body.empty()) throw std::invalid_argument("missing process cube table body");
    minera::ProcessCubeOptions options;
    if (request.has_param("x_col")) options.x_col = request.get_param_value("x_col");
    if (request.has_param("x_cols")) options.x_columns = ocel_query_list(request, "x_cols");
    if (request.has_param("y_col")) options.y_col = request.get_param_value("y_col");
    if (request.has_param("y_cols")) options.y_columns = ocel_query_list(request, "y_cols");
    if (request.has_param("agg_col"))
      options.aggregation_column = request.get_param_value("agg_col");
    if (request.has_param("aggregation"))
      options.aggregation = process_cube_aggregation(
          request.get_param_value("aggregation"));
    options.max_divisions_x = ocel_query_size(
        request, "max_divisions_x", options.max_divisions_x);
    options.max_divisions_y = ocel_query_size(
        request, "max_divisions_y", options.max_divisions_y);
    if (options.max_divisions_x == 0 || options.max_divisions_x > 10000 ||
        options.max_divisions_y == 0 || options.max_divisions_y > 10000)
      throw std::invalid_argument(
          "process cube division limits must be from 1 through 10000");
    options.x_bins = process_cube_query_bins(request, "x_bins");
    options.y_bins = process_cube_query_bins(request, "y_bins");
    response.set_content(minera::serialize_process_cube_json(
                             minera::process_cube(
                                 minera::parse_process_cube_table_json(body),
                                 options)),
                         "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

int usage(const char* program) {
  std::cerr << "Usage: " << program
            << " [options]\n"
               "  --port <port>      Listen port (default: 8484)\n"
               "  --frontend <dir>   Static frontend directory"
               " (default: frontend/dist next to the working directory)\n";
  return 2;
}

}  // namespace

int main(int argc, char** argv) {
  int port = 8484;
  std::filesystem::path frontend_dir = "frontend/dist";

  for (int i = 1; i < argc; ++i) {
    const std::string_view arg = argv[i];
    const auto next = [&]() -> const char* {
      return i + 1 < argc ? argv[++i] : nullptr;
    };

    if (arg == "--port") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      port = std::atoi(value);
      if (port <= 0 || port > 65535) return usage(argv[0]);
    } else if (arg == "--frontend") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      frontend_dir = value;
    } else {
      return usage(argv[0]);
    }
  }

  httplib::Server server;
  // Bound multipart bodies before the HTTP library buffers or parses them.
  server.set_payload_max_length(32u * 1024u * 1024u);

  // The app is local-first; permissive CORS keeps the Vite dev server (a
  // different port) working against this backend.
  server.set_default_headers({{"Access-Control-Allow-Origin", "*"}});
  const auto preflight = [](const httplib::Request&, httplib::Response& response) {
    response.set_header("Access-Control-Allow-Methods", "POST, OPTIONS");
    response.set_header("Access-Control-Allow-Headers", "Content-Type");
    response.status = 204;
  };
  server.Options("/api/mine", preflight);
  server.Options("/api/ocel/discover", preflight);
  server.Options("/api/ocel/split", preflight);
  server.Options("/api/ocel/playout", preflight);
  server.Options("/api/ocel/features", preflight);
  server.Options("/api/ocel/statistics", preflight);
  server.Options("/api/ocel/description", preflight);
  server.Options("/api/ocel/filter", preflight);
  server.Options("/api/ocel/sqlite", preflight);
  server.Options("/api/ocel/io", preflight);
  server.Options("/api/ocel/conformance", preflight);
  server.Options("/api/ilp-mine", preflight);
  server.Options("/api/mine-ilp", preflight);
  server.Options("/api/genetic-mine", preflight);
  server.Options("/api/mine-genetic", preflight);
  server.Options("/api/organizational-sna", preflight);
  server.Options("/api/sna", preflight);
  server.Options("/api/network-analysis", preflight);
  server.Options("/api/link-analysis", preflight);
  server.Options("/api/roles", preflight);
  server.Options("/api/resource-profiles", preflight);
  server.Options("/api/local-diagnostics", preflight);
  server.Options("/api/clustering", preflight);
  server.Options("/api/profile-clustering", preflight);
  server.Options("/api/chaotic-activities", preflight);
  server.Options("/api/passed-time", preflight);
  server.Options("/api/service-time", preflight);
  server.Options("/api/rework", preflight);
  server.Options("/api/cycle-time", preflight);
  server.Options("/api/case-arrival", preflight);
  server.Options("/api/case-statistics", preflight);
  server.Options("/api/attribute-statistics", preflight);
  server.Options("/api/attribute-distribution", preflight);
  server.Options("/api/attribute-kde", preflight);
  server.Options("/api/boundary-activities", preflight);
  server.Options("/api/variant-statistics", preflight);
  server.Options("/api/trace-variant-query", preflight);
  server.Options("/api/anonymize-trace-variants", preflight);
  server.Options("/api/pripel", preflight);
  server.Options("/api/anonymize-pripel", preflight);
  server.Options("/api/concept-drift", preflight);
  server.Options("/api/transform-target", preflight);
  server.Options("/api/log-to-features", preflight);
  server.Options("/api/features", preflight);
  server.Options("/api/transform-features", preflight);
  server.Options("/api/log-util", preflight);
  server.Options("/api/embeddings", preflight);
  server.Options("/api/to-embeddings", preflight);
  server.Options("/api/label-splitting", preflight);
  server.Options("/api/transform-label-splitting", preflight);
  server.Options("/api/trie", preflight);
  server.Options("/api/trie-to-petri", preflight);
  server.Options("/api/interval-tree", preflight);
  server.Options("/api/merge-case-relations", preflight);
  server.Options("/api/timestamp-interleavings", preflight);
  server.Options("/api/random-tree", preflight);
  server.Options("/api/compare-petri", preflight);
  server.Options("/api/align", preflight);
  server.Options("/api/evaluate", preflight);
  server.Options("/api/petri-analysis", preflight);
  server.Options("/api/evaluate-emd", preflight);
  server.Options("/api/align-process-tree", preflight);
  server.Options("/api/align-decomposed", preflight);
  server.Options("/api/align-petri-anti", preflight);
  server.Options("/api/align-petri-multi", preflight);
  server.Options("/api/reduce-process-tree", preflight);
  server.Options("/api/align-dfg", preflight);
  server.Options("/api/align-edit-distance", preflight);
  server.Options("/api/decide", preflight);
  server.Options("/api/decision-tables", preflight);
  server.Options("/api/data-petri-net", preflight);
  server.Options("/api/check-model", preflight);
  server.Options("/api/monte-carlo", preflight);
  server.Options("/api/dfg", preflight);
  server.Options("/api/dfg-precision", preflight);
  server.Options("/api/dfg-to-petri", preflight);
  server.Options("/api/process-cube", preflight);
  server.Options("/api/streaming-dfg", preflight);
  server.Options("/api/streaming-footprints", preflight);
  server.Options("/api/streaming-temporal", preflight);
  server.Options("/api/streaming-tbr", preflight);

  server.Post("/api/mine", handle_mine);
  server.Post("/api/ocel/discover", handle_ocel_discovery);
  server.Post("/api/ocel/split", handle_ocel_split);
  server.Post("/api/ocel/playout", handle_ocel_playout);
  server.Post("/api/ocel/features", handle_ocel_features);
  server.Post("/api/ocel/statistics", handle_ocel_statistics);
  server.Post("/api/ocel/description", handle_ocel_description);
  server.Post("/api/ocel/filter", handle_ocel_filter);
  server.Post("/api/ocel/sqlite", handle_ocel_sqlite);
  server.Post("/api/ocel/io", handle_ocel_exchange);
  server.Post("/api/ocel/conformance", handle_ocel_conformance);
  server.Post("/api/ilp-mine", handle_ilp_mine);
  server.Post("/api/mine-ilp", handle_ilp_mine);
  server.Post("/api/genetic-mine", handle_genetic_mine);
  server.Post("/api/mine-genetic", handle_genetic_mine);
  server.Post("/api/organizational-sna", handle_organizational_sna);
  server.Post("/api/sna", handle_organizational_sna);
  server.Post("/api/network-analysis", handle_network_analysis);
  server.Post("/api/link-analysis", handle_link_analysis);
  server.Post("/api/roles", handle_roles);
  server.Post("/api/resource-profiles", handle_resource_profiles);
  server.Post("/api/local-diagnostics", handle_local_diagnostics);
  server.Post("/api/clustering", handle_clustering);
  server.Post("/api/profile-clustering", handle_profile_clustering);
  server.Post("/api/chaotic-activities", handle_chaotic_activities);
  server.Post("/api/passed-time", handle_passed_time);
  server.Post("/api/service-time", handle_service_time);
  server.Post("/api/rework", handle_rework);
  server.Post("/api/cycle-time", handle_cycle_time);
  server.Post("/api/case-arrival", handle_case_arrival);
  server.Post("/api/case-statistics", handle_case_statistics);
  server.Post("/api/attribute-statistics", handle_attribute_statistics);
  server.Post("/api/attribute-distribution", handle_attribute_distribution);
  server.Post("/api/attribute-kde", handle_attribute_kde);
  server.Post("/api/boundary-activities", handle_boundary_activities);
  server.Post("/api/variant-statistics", handle_variant_statistics);
  server.Post("/api/trace-variant-query", handle_trace_variant_query);
  server.Post("/api/anonymize-trace-variants", handle_trace_variant_query);
  server.Post("/api/pripel", handle_pripel);
  server.Post("/api/anonymize-pripel", handle_pripel);
  server.Post("/api/concept-drift", handle_concept_drift);
  server.Post("/api/transform-target", handle_log_to_target);
  server.Post("/api/log-to-features", handle_log_to_features);
  server.Post("/api/features", handle_log_to_features);
  server.Post("/api/transform-features", handle_log_to_features);
  server.Post("/api/log-util", handle_log_util);
  server.Post("/api/embeddings", handle_embeddings);
  server.Post("/api/to-embeddings", handle_embeddings);
  server.Post("/api/label-splitting", handle_label_splitting);
  server.Post("/api/transform-label-splitting", handle_label_splitting);
  server.Get("/api/trie", handle_trie);
  server.Get("/api/trie-to-petri", handle_trie_to_petri);
  server.Get("/api/interval-tree", handle_interval_tree);
  server.Post("/api/merge-case-relations", handle_case_relations);
  server.Post("/api/timestamp-interleavings", handle_timestamp_interleavings);
  server.Get("/api/random-tree", handle_random_tree);
  server.Post("/api/compare-petri", handle_compare_petri);
  server.Post("/api/align", handle_align);
  server.Post("/api/evaluate", handle_evaluate);
  server.Post("/api/petri-analysis", handle_petri_analysis);
  server.Post("/api/evaluate-emd", handle_evaluate_emd);
  server.Post("/api/align-process-tree", handle_align_process_tree);
  server.Post("/api/align-decomposed", handle_align_decomposed);
  server.Post("/api/align-petri-anti", handle_align_petri_anti);
  server.Post("/api/align-petri-multi", handle_align_petri_multi);
  server.Post("/api/reduce-process-tree", handle_reduce_process_tree);
  server.Post("/api/align-dfg", handle_align_dfg);
  server.Post("/api/align-edit-distance", handle_align_edit_distance);
  server.Post("/api/decide", handle_decide);
  server.Post("/api/decision-tables", handle_decision_tables);
  server.Post("/api/data-petri-net", handle_data_petri_net);
  server.Post("/api/check-model", handle_check_model);
  server.Get("/api/pnml", handle_pnml);
  server.Get("/api/bpmn", handle_bpmn);
  server.Get("/api/ptml", handle_ptml);
  server.Get("/api/xes", handle_xes);
  server.Get("/api/dfg", handle_dfg);
  server.Post("/api/dfg", handle_dfg_import);
  server.Post("/api/dfg-precision", handle_dfg_precision);
  server.Post("/api/dfg-to-petri", handle_dfg_conversion);
  server.Post("/api/process-cube", handle_process_cube);
  server.Post("/api/streaming-dfg", handle_streaming_dfg);
  server.Post("/api/streaming-footprints", handle_streaming_footprints);
  server.Post("/api/streaming-temporal", handle_streaming_temporal);
  server.Post("/api/streaming-tbr", handle_streaming_tbr);
  server.Get("/api/playout", handle_playout);
  server.Get("/api/monte-carlo", handle_monte_carlo_json);

  server.Get("/api/version", [](const httplib::Request&, httplib::Response& response) {
    std::ostringstream out;
    out << "{\"generator\": \"minera " << minera::version_major << '.'
        << minera::version_minor << '.' << minera::version_patch << "\"}";
    response.set_content(out.str(), "application/json");
  });

  const bool has_frontend = std::filesystem::is_directory(frontend_dir);
  if (has_frontend) {
    server.set_mount_point("/", frontend_dir.string());
  }

  std::cout << "minera " << minera::version_major << '.' << minera::version_minor
            << '.' << minera::version_patch << " — listening on http://localhost:"
            << port << '\n';
  if (!has_frontend) {
    std::cout << "note: frontend directory not found (" << frontend_dir.string()
              << "); serving the API only\n";
  }

  if (!server.listen("127.0.0.1", port)) {
    std::cerr << "error: cannot listen on port " << port << '\n';
    return 1;
  }
  return 0;
}
