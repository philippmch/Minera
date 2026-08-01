// Local application server: serves the built frontend and mines uploaded
// CSV event logs with the native core via POST /api/mine.

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "httplib.h"
#include "minera/alpha_miner.hpp"
#include "minera/bpmn.hpp"
#include "minera/conformance.hpp"
#include "minera/correlation_miner.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/declare.hpp"
#include "minera/declare_playout.hpp"
#include "minera/directly_follows_graph.hpp"
#include "minera/dfg_alignment.hpp"
#include "minera/edit_distance_alignment.hpp"
#include "minera/dfg_variants.hpp"
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
#include "minera/decomposed_alignment.hpp"
#include "minera/petri_anti_alignment.hpp"
#include "minera/petri_multi_alignment.hpp"
#include "minera/process_tree_reduction.hpp"
#include "minera/resource_analysis.hpp"
#include "minera/temporal_profile.hpp"
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
    }

    std::istringstream input(csv);
    auto log = looks_like_xes(dataset, csv)
                   ? minera::import_xes_event_log(input)
                   : minera::import_csv_event_log(input, params.csv);
    if (params.filter.any()) {
      log = minera::filter_event_log(log, params.filter);
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
        minera::discover_minimum_self_distances(log);
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
        batch_discovery ? &*batch_discovery : nullptr);

    // Cache this analysis so alignments can be served on demand for the model
    // the user actually opens, rather than computed for all three up front.
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

// Checks the most recently mined log against a user-supplied model — a PNML
// workflow net, a BPMN 2.0 process, or a PTML process tree, told apart by the
// document's root element (multipart field `model`, or the raw body). The imported net joins
// the cached session under the id "imported", so POST /api/align?miner=imported
// can align against it on demand like any discovered model; the session's
// registry is extended with the net's activities so ids stay resolvable.
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
      case minera::PetriAnalysisFamily::kSoundness: {
        minera::SoundnessOptions options;
        options.initial_marking = params.initial_marking;
        options.final_marking = params.final_marking;
        options.max_states = params.max_states;
        options.diagnostics = params.diagnostics;
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
    auto decisions = minera::mine_decisions(session->log, it->second);
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

// Simulates one model from the most recent mine into a synthetic event log,
// served as XES. The `miner` query parameter picks a Petri model
// ("inductive" | "alpha" | "heuristic" | "powl" | "imported", default
// inductive) or the classic Declare model ("declare"). `traces` defaults to
// 100 for Petri models and 1000 for Declare, capped at 10000. Declare also
// accepts min_length, max_length, and seed; process_tree accepts variant,
// traces, max_steps, seed, min_length, max_length, max_loop_occurrences and
// max_trace_cap.
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
  const std::string mode = request.has_param("mode")
                               ? request.get_param_value("mode")
                               : std::string{};
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

  // The app is local-first; permissive CORS keeps the Vite dev server (a
  // different port) working against this backend.
  server.set_default_headers({{"Access-Control-Allow-Origin", "*"}});
  const auto preflight = [](const httplib::Request&, httplib::Response& response) {
    response.set_header("Access-Control-Allow-Methods", "POST, OPTIONS");
    response.set_header("Access-Control-Allow-Headers", "Content-Type");
    response.status = 204;
  };
  server.Options("/api/mine", preflight);
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
  server.Options("/api/check-model", preflight);

  server.Post("/api/mine", handle_mine);
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
  server.Post("/api/check-model", handle_check_model);
  server.Get("/api/pnml", handle_pnml);
  server.Get("/api/bpmn", handle_bpmn);
  server.Get("/api/ptml", handle_ptml);
  server.Get("/api/xes", handle_xes);
  server.Get("/api/playout", handle_playout);

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
