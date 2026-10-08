#include "case_relations_input.hpp"
// Local application server: serves the built frontend and mines uploaded
// CSV event logs with the native core via POST /api/mine.

#include "pnml_model_options.hpp"
#include "replay_input_options.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cctype>
#include <cerrno>
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
#include <utility>
#include <vector>

#include "httplib.h"
#include "embedding_runtime.hpp"
#include "llm_runtime.hpp"
#include "query_runtime.hpp"
#include "log_cluster_runtime.hpp"
#include "llm_context_options.hpp"
#include "minera/log_query.hpp"
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
#include "minera/data_petri_net.hpp"
#include "minera/declare.hpp"
#include "minera/declare_playout.hpp"
#include "minera/decision_table.hpp"
#include "minera/directly_follows_graph.hpp"
#include "minera/dfg_conversion.hpp"
#include "minera/dfg_dataframe.hpp"
#include "minera/dfg_dataframe_json.hpp"
#include "minera/dfg_filter.hpp"
#include "minera/dfg_io.hpp"
#include "minera/dfg_precision.hpp"
#include "minera/dfg_alignment.hpp"
#include "minera/dfg_playout.hpp"
#include "minera/json_export.hpp"
#include "minera/knowledge_injection.hpp"
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
#include "minera/log_conversion.hpp"
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
#include "minera/dataframe_format_json.hpp"
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
#include "minera/ocel_query.hpp"
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
#include "minera/petri_net_reduction.hpp"
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
#include "minera/stochastic_map.hpp"
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
#include "minera/windows_event_log.hpp"
#include "minera/repository_history.hpp"
#include "minera/workflow_history.hpp"
#include "minera/enterprise_history.hpp"
#include "minera/communication_history.hpp"
#include "repository_history_runtime.hpp"
#include "minera/xes_export.hpp"

namespace {

using minera::error_json;
using minera::looks_like_xes;

bool has_table_column(const minera::EventTable& table, std::string_view column) {
  return std::find(table.columns.begin(), table.columns.end(), column) !=
         table.columns.end();
}

void adapt_case_relations_table_timestamp(
    minera::CaseRelationsOptions& options,
    const minera::EventTable& base,
    const minera::EventTable& incoming) {
  // Callers invoke this only when merge_timestamp_key was omitted.  Keep the
  // canonical time:timestamp header first, then bridge the CSV timestamp
  // alias when it exists on either side; the table merge handles absence on
  // the other side.  Explicit selectors remain exact at the call site.
  if (options.timestamp_key != "time:timestamp") return;
  for (const std::string_view candidate : {"time:timestamp", "timestamp"}) {
    if (has_table_column(base, candidate) ||
        has_table_column(incoming, candidate)) {
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
  if (node.kind() == minera::PowlNodeKind::kTransition) {
    graph.ensure_activity(node.label());
  }
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

// The most recently mined log and its discovered nets, kept so the costly
// alignment of a model can be computed on demand (POST /api/align) instead of
// eagerly for every miner on every mine. The app is local and single-user, so
// caching just the latest analysis is enough; a shared_ptr lets a slow
// alignment run on a snapshot without holding the lock.
std::atomic<std::uint64_t> g_query_source_sequence{0};
struct MinedSession {
  std::uint64_t query_source_id=++g_query_source_sequence;
  minera::EventLog log;
  // Exact selector/regrouping/order projection used to discover the inductive
  // model. Model-specific replay must use this alphabet rather than the
  // original upload's first-class activity field.
  minera::EventLog inductive_log;
  // Exact activity projection used to discover the Alpha model. Keeping it in
  // the session makes on-demand conformance use the same alphabet as mining.
  minera::EventLog alpha_log;
  minera::DirectlyFollowsGraph graph;
  std::unordered_map<std::string, minera::PetriNet> nets;  // miner id -> net
  minera::ProcessTree tree;  // the inductive model, for on-demand BPMN export
  minera::DeclareModel declare;  // classic Declare, for on-demand playout
  std::string dataset;       // uploaded file name, echoed into XES exports
  std::optional<minera::PnmlDocument> imported_document;
  minera::TokenReplayOptions token_replay;
  minera::InductiveMinerOptions inductive_options;
  std::string alpha_activity_attribute = "concept:name";
  std::string powl_activity_key = "concept:name";
  std::string heuristic_activity_attribute = "concept:name";
  std::string powl_timestamp_key = "time:timestamp";
  std::string powl_case_id_key = "case:concept:name";
};

const minera::EventLog& cached_model_log(const MinedSession& session,
                                         std::string_view miner) {
  return miner == "inductive" || miner == "process_tree"
             ? session.inductive_log
             : miner == "alpha" ? session.alpha_log : session.log;
}

// Marking defaults belong to the cached supplied document. An explicitly
// supplied request vector wins independently for each marking.
template <typename Options>
void cached_imported_markings(const httplib::Request& request,
                              const MinedSession& session,
                              std::string_view miner, Options& options,
                              std::string_view initial = "initial_marking",
                              std::string_view final = "final_marking") {
  if (miner != "imported" || !session.imported_document) return;
  const auto& document = *session.imported_document;
  if (!request.has_param(std::string(initial))) {
    using Integer = typename decltype(options.initial_marking)::value_type;
    options.initial_marking =
        minera::tools::pnml_marking<Integer>(document.initial_marking);
  }
  if (!request.has_param(std::string(final))) {
    using Integer = typename decltype(options.final_marking)::value_type;
    options.final_marking =
        minera::tools::pnml_marking<Integer>(document.final_marking);
  }
}

void cached_imported_monte_carlo_context(
    const httplib::Request& request, const MinedSession& session,
    std::string_view miner, minera::MonteCarloOptions& options) {
  cached_imported_markings(request, session, miner, options,
                           "monte_carlo_initial_marking", "monte_carlo_final_marking");
  if (miner != "imported" || !session.imported_document) return;
  const auto& document = *session.imported_document;
  options.place_names = document.place_ids;
  options.transition_names = document.transition_ids;
  options.initial_marking_keys = request.has_param("monte_carlo_initial_marking")
      ? std::vector<bool>{} : document.initial_marking_keys;
  options.final_marking_keys = request.has_param("monte_carlo_final_marking")
      ? std::vector<bool>{} : document.final_marking_keys;
}

// A selector is deliberately request-scoped: the cached inductive model and
// its default conversion remain unchanged, while callers that need the
// alternate construction receive a fresh value for the duration of a route.
std::optional<minera::PetriNet> selected_process_tree_net(
    const httplib::Request& request, const MinedSession& session,
    std::string_view miner) {
  if (!request.has_param("process_tree_variant")) return std::nullopt;
  if (miner != "inductive") {
    throw std::invalid_argument(
        "process_tree_variant requires miner=inductive");
  }
  const auto variant =
      minera::parse_process_tree_petri_net_variant(
          request.get_param_value("process_tree_variant"));
  return minera::to_petri_net(session.tree, variant);
}

const minera::PetriNet& select_cached_or_process_tree_net(
    const httplib::Request& request, const MinedSession& session,
    std::string_view miner, std::optional<minera::PetriNet>& converted) {
  converted = selected_process_tree_net(request, session, miner);
  if (converted.has_value()) return *converted;
  const auto it = session.nets.find(std::string(miner));
  if (it == session.nets.end())
    throw std::invalid_argument("unknown miner: " + std::string(miner));
  return it->second;
}

std::shared_ptr<const MinedSession> g_session;
std::mutex g_session_mutex;
bool g_embedding_runtime_enabled = false;
std::string g_llm_runtime_url;
std::string g_query_runtime_url;
std::set<std::string> g_llm_origins;
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
constexpr std::size_t kServerLogConversionMaxGraphNodes = 200'000;
constexpr std::size_t kServerLogConversionMaxGraphEdges = 400'000;
constexpr std::size_t kServerLogConversionMaxVariants = 100'000;
constexpr std::size_t kServerLogConversionMaxRows = 1'000'000;
constexpr std::size_t kServerMonteCarloMaxSimulations = 10'000;
constexpr std::size_t kServerMonteCarloMaxSteps = 10'000;
constexpr std::size_t kServerMonteCarloMaxResourceCapacity = 1'000;
constexpr std::size_t kServerMonteCarloMaxResourceSlots = 1'000'000;
constexpr std::size_t kServerMonteCarloMaxInitialTokens = 1'000'000;
constexpr std::size_t kServerMonteCarloMaxInitialAssignments = 1'000'000;
constexpr std::size_t kServerMonteCarloMaxSimulationSteps = 1'000'000;

void validate_monte_carlo_server_limits(
    const minera::MonteCarloOptions& options, const minera::PetriNet& net) {
  if (options.simulations > kServerMonteCarloMaxSimulations ||
      options.max_steps > kServerMonteCarloMaxSteps ||
      options.default_resources_per_place > kServerMonteCarloMaxResourceCapacity) {
    throw std::invalid_argument(
        "Monte Carlo bounds exceed the server safety limits");
  }
  if (options.simulations != 0 && options.max_steps != 0 &&
      options.simulations >
          kServerMonteCarloMaxSimulationSteps / options.max_steps) {
    throw std::invalid_argument(
        "Monte Carlo simulation budget exceeds the server safety limit");
  }

  for (const auto& [place, capacity] : options.resources_per_place) {
    if (place >= net.place_count()) {
      throw std::invalid_argument(
          "Monte Carlo resource place index is invalid");
    }
    if (capacity > kServerMonteCarloMaxResourceCapacity) {
      throw std::invalid_argument(
          "Monte Carlo resource capacity exceeds the server safety limit");
    }
  }
  std::size_t total_resources = 0;
  for (std::size_t place = 0; place < net.place_count(); ++place) {
    const auto override_capacity = options.resources_per_place.find(place);
    const auto capacity = override_capacity == options.resources_per_place.end()
                              ? options.default_resources_per_place
                              : override_capacity->second;
    if (total_resources > kServerMonteCarloMaxResourceSlots - capacity) {
      throw std::invalid_argument(
          "Monte Carlo total resource capacity exceeds the server safety limit");
    }
    total_resources += capacity;
  }

  if (!options.initial_marking.empty() &&
      options.initial_marking.size() != net.place_count()) {
    throw std::invalid_argument(
        "Monte Carlo initial marking must match the place count");
  }
  std::size_t initial_tokens = options.initial_marking.empty() ? 1 : 0;
  for (const auto tokens : options.initial_marking) {
    if (tokens > kServerMonteCarloMaxInitialTokens ||
        initial_tokens > kServerMonteCarloMaxInitialTokens - tokens) {
      throw std::invalid_argument(
          "Monte Carlo initial marking exceeds the server safety limit");
    }
    initial_tokens += tokens;
  }
  if (initial_tokens != 0 &&
      options.simulations >
          kServerMonteCarloMaxInitialAssignments / initial_tokens) {
    throw std::invalid_argument(
        "Monte Carlo initial assignment budget exceeds the server safety limit");
  }
}

void reject_unknown_monte_carlo_params(const httplib::Request& request) {
  static const std::set<std::string> allowed = {
      "miner", "mode", "monte_carlo_simulations",
      "monte_carlo_activity_key", "monte_carlo_timestamp_key",
      "monte_carlo_initial_marking", "monte_carlo_final_marking",
      "monte_carlo_case_arrival_ratio",
      "monte_carlo_case_arrival_seconds", "monte_carlo_max_steps",
      "monte_carlo_seed", "monte_carlo_start",
      "monte_carlo_default_duration_seconds",
      "monte_carlo_force_distribution",
      "monte_carlo_transition_weights",
      "monte_carlo_transition_durations",
      "monte_carlo_resources_per_place",
      "monte_carlo_default_resources_per_place",
      "monte_carlo_enable_diagnostics", "monte_carlo_stop_at_final",
      "monte_carlo_diagnostic_interval", "monte_carlo_small_scale_factor",
      "monte_carlo_max_thread_execution_time",
      "monte_carlo_token_replay_variant",
      "monte_carlo_provided_stochastic_map",
      "process_tree_variant"};
  for (const auto& [key, value] : request.params) {
    (void)value;
    if (!allowed.contains(key))
      throw std::invalid_argument("unknown Monte Carlo parameter: " + key);
  }
}

void reject_unknown_stochastic_params(const httplib::Request& request) {
  static const std::set<std::string> allowed = {
      "miner", "mode", "stochastic_traces", "stochastic_max_steps",
      "stochastic_seed", "stochastic_transition_weights",
      "stochastic_fit_from_log", "stochastic_stop_at_final",
      "stochastic_return_visited_elements",
      "stochastic_start", "stochastic_case_interval",
      "stochastic_event_interval", "stochastic_initial_marking",
      "stochastic_final_marking", "process_tree_variant"};
  for (const auto& [key, value] : request.params) {
    (void)value;
    if (!allowed.contains(key))
      throw std::invalid_argument("unknown stochastic parameter: " + key);
  }
}

void reject_unknown_stochastic_petri_params(const httplib::Request& request) {
  static const std::set<std::string> allowed = {
      "miner", "stochastic_petri_rates", "stochastic_petri_initial_marking",
      "stochastic_petri_immediate_transitions",
      "stochastic_petri_max_states", "stochastic_petri_analysis",
      "stochastic_petri_time", "stochastic_petri_initial_state",
      "stochastic_petri_initial_probabilities"};
  for (const auto& [key, value] : request.params) {
    (void)value;
    if (!allowed.contains(key))
      throw std::invalid_argument("unknown stochastic Petri parameter: " + key);
  }
}

void reject_unknown_stochastic_map_params(const httplib::Request& request) {
  static const std::set<std::string> allowed = {
      "miner", "stochastic_map_source", "stochastic_map_distribution",
      "stochastic_map_default_parameter",
      "stochastic_map_silent_transitions_immediate",
      "stochastic_map_zero_duration_immediate",
      "stochastic_map_fit_activation_weights", "stochastic_map_fit_timing",
      "stochastic_map_default_activation_weight",
      "stochastic_map_frequency_activation_weights",
      "stochastic_map_conversion_variant", "stochastic_map_max_states",
      "stochastic_map_initial_marking"};
  for (const auto& [key, value] : request.params) {
    (void)value;
    if (!allowed.contains(key))
      throw std::invalid_argument("unknown stochastic map parameter: " + key);
  }
}

std::string form_field(const httplib::Request& request,
                       const std::string& name) {
  return request.has_file(name) ? request.get_file_value(name).content
                                : std::string{};
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

    const bool xes_input = looks_like_xes(dataset, csv);
    std::istringstream input(csv);
    minera::EventLog log;
    std::optional<minera::CsvEventData> csv_data;
    if (xes_input) {
      log = minera::import_xes_event_log(input, params.xes);
    } else if (!params.correlation_variants.empty()) {
      csv_data = minera::import_csv_event_data(input, params.csv);
      log = std::move(csv_data->log);
    } else {
      log = minera::import_csv_event_log(input, params.csv);
    }
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
    register_activity_selector_values(graph, log,
                                      params.token_replay.activity_attribute);
    register_activity_selector_values(graph, log,
                                      params.heuristic.activity_attribute);
    register_activity_selector_values(
        graph, log, params.temporal_profile.activity_attribute);
    register_activity_selector_values(
        graph, log,
        params.transition_system.activity_attribute.empty()
            ? params.transition_system.activity_key
            : params.transition_system.activity_attribute);
    register_activity_selector_values(
        graph, log, params.dfg_statistics.activity_attribute);
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
      if (xes_input) {
        correlation_variants_analysis = analyze_correlation_variants(
            minera::EventDataView(log), params.correlation_variants,
            params.correlation);
      } else {
        const auto correlation_table =
            minera::project_csv_event_table(csv_data->table, log);
        correlation_variants_analysis = analyze_correlation_variants(
            minera::EventDataView(correlation_table),
            params.correlation_variants, params.correlation);
      }
      minera::register_correlation_activities(
          graph, *correlation_variants_analysis);
    }
    const auto temporal_profile =
        minera::discover_temporal_profile(log, params.temporal_profile);
    const auto minimum_self_distances =
        discover_selected_minimum_self_distances(
            log, params.minimum_self_distance);
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
        minera::discover_transition_system(log, params.transition_system);
    for (const auto& transition : transition_system.transitions()) {
      if (transition.label) graph.ensure_activity(*transition.label);
    }
    const auto inductive_log =
        minera::prepare_inductive_event_log(log, params.inductive);
    const auto tree = minera::discover_process_tree(log, params.inductive);
    for (const auto& activity : tree.activities()) {
      graph.ensure_activity(activity);
    }
    const auto inductive_graph =
        minera::discover_directly_follows_graph(inductive_log);
    const auto net = minera::discover_petri_net(log, params.alpha);
    const auto alpha_log =
        minera::normalize_alpha_event_log(log, params.alpha);
    register_activity_selector_values(graph, log,
                                      params.alpha.activity_attribute);
    for (const auto& activity : net.activities())
      graph.ensure_activity(activity);
    const auto heuristics = minera::discover_heuristics_net(log, params.heuristic);
    const auto powl = minera::discover_powl(log, params.powl);
    register_powl_labels(graph, powl);
    const auto powl_log = project_powl_activity(
        log, params.powl.activity_key, params.powl.timestamp_key,
        params.powl.case_id_key);
    const auto powl_graph =
        minera::discover_directly_follows_graph(powl_log);
    const auto inductive_net = minera::to_petri_net(tree);
    const auto heuristic_net = minera::to_petri_net(heuristics);
    const auto powl_net = minera::to_petri_net(powl);
    register_activity_selector_values(
        graph, log, params.footprints.activity_attribute);
    const auto footprints =
        analyze_footprints(log, graph, alpha_log, tree, powl,
                           inductive_net, net,
                           heuristic_net, powl_net, params.footprints);
    const auto performance = minera::compute_performance_metrics(log, graph);
    const auto variant_durations =
        minera::compute_variant_durations(log, graph);
    const auto resources = minera::discover_resource_network(log);
    const auto eventually = minera::discover_eventually_follows(log, graph);
    std::optional<minera::PerformanceSpectrum> spectrum;
    if (!params.spectrum_activities.empty()) {
      spectrum = discover_selected_performance_spectrum(
          log, params.spectrum_activities, params.spectrum);
    }
    std::optional<minera::BatchDiscovery> batch_discovery;
    if (params.batches) {
      batch_discovery = discover_selected_batches(log, params.batch_options);
    }
    const auto log_skeleton =
        minera::discover_log_skeleton(log, params.log_skeleton);
    const auto log_skeleton_conformance = minera::check_log_skeleton_conformance(
        log, log_skeleton, params.log_skeleton);
    const auto declare =
        minera::discover_declare(log, params.declare_options);
    const auto declare_conformance =
        minera::check_declare_conformance(
            log, declare, params.declare_conformance_options);
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
      const bool is_powl = id == "powl";
      const bool is_inductive = id == "inductive";
      const bool is_alpha = id == "alpha";
      auto replay_options = params.token_replay;
      if (!params.token_replay_activity_attribute_supplied) {
        if (is_inductive)
          replay_options.activity_attribute = params.inductive.activity_key;
        else if (is_alpha)
          replay_options.activity_attribute = params.alpha.activity_attribute;
      }
      if (id == "heuristic" && !params.token_replay_activity_attribute_supplied &&
          replay_options.activity_attribute == "concept:name") {
        replay_options.activity_attribute = params.heuristic.activity_attribute;
      }
      if (is_powl && !params.token_replay_activity_attribute_supplied) {
        replay_options.activity_attribute = params.powl.activity_key;
      }
      std::optional<minera::EventLog> projected_powl_log;
      const minera::EventLog* conformance_log =
          is_inductive ? &inductive_log : &log;
      if (is_alpha) conformance_log = &alpha_log;
      if (is_powl) {
        projected_powl_log = project_powl_activity(
            log, replay_options.activity_attribute, params.powl.timestamp_key,
            params.powl.case_id_key);
        conformance_log = &*projected_powl_log;
      }
      replay_options = minera::tools::default_replay_input_options(
          *conformance_log, std::move(replay_options),
          params.token_replay_activity_attribute_supplied);
      return minera::ModelConformance{std::move(id),
                                      minera::final_marking_reachable(petri),
                                      minera::check_conformance(
                                           *conformance_log, petri,
                                           replay_options),
                                      is_powl
                                          ? minera::check_footprints(powl_log,
                                                                      petri)
                                          : is_inductive
                                                ? minera::check_footprints(
                                                      inductive_log, petri)
                                                : is_alpha
                                                      ? minera::check_footprints(
                                                            alpha_log, petri)
                                                : minera::check_footprints(log,
                                                                          petri),
                                      std::nullopt};
    };
    const std::vector<minera::ModelConformance> conformance = {
        model("inductive", inductive_net), model("alpha", net),
        model("heuristic", heuristic_net), model("powl", powl_net)};

    // Frequency/performance annotations for every miner's Petri-net view:
    // one more token replay per net, cheap enough to compute up front.
    const auto petri_decoration = minera::decorate_model(alpha_log, net);
    const auto inductive_decoration =
        minera::decorate_model(inductive_log, inductive_net);
    const auto heuristic_decoration = minera::decorate_model(
        log, heuristic_net, params.heuristic.activity_attribute,
        params.heuristic.timestamp_attribute);
    const auto powl_decoration = minera::decorate_model(
        powl_log, powl_net, params.powl.activity_key, params.powl.timestamp_key);

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
    session->inductive_log = inductive_log;
    session->alpha_log = alpha_log;
    session->graph = graph;
    session->nets.emplace("inductive", inductive_net);
    session->nets.emplace("alpha", net);
    session->nets.emplace("heuristic", heuristic_net);
    session->nets.emplace("powl", powl_net);
    session->tree = tree;
    session->declare = declare;
    session->dataset = dataset;
    session->token_replay = minera::tools::default_replay_input_options(
        log, params.token_replay, params.token_replay_activity_attribute_supplied);
    session->inductive_options = params.inductive;
    session->alpha_activity_attribute = params.alpha.activity_attribute;
    session->heuristic_activity_attribute = params.heuristic.activity_attribute;
    session->powl_activity_key = params.powl.activity_key;
    session->powl_timestamp_key = params.powl.timestamp_key;
    session->powl_case_id_key = params.powl.case_id_key;
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

double ocel_conformance_query_number(const httplib::Request& request,
                                     std::string_view name, double fallback) {
  const auto key = std::string(name);
  if (!request.has_param(key)) return fallback;
  const auto value = request.get_param_value(key);
  char* end = nullptr;
  const auto parsed = std::strtod(value.c_str(), &end);
  if (end == value.c_str() || end != value.c_str() + value.size())
    throw std::invalid_argument(key + ": expected a number");
  return parsed;
}

std::size_t ocel_query_size(const httplib::Request& request,
                            std::string_view name, std::size_t fallback) {
  if (!request.has_param(std::string(name))) return fallback;
  const auto value = request.get_param_value(std::string(name));
  if (value.empty() || value.front() == '-')
    throw std::invalid_argument(std::string(name) + ": expected a non-negative integer");
  errno = 0;
  char* end = nullptr;
  const auto parsed = std::strtoull(value.c_str(), &end, 10);
  if (end != value.c_str() + value.size() ||
      errno == ERANGE ||
      parsed > std::numeric_limits<std::size_t>::max()) {
    throw std::invalid_argument(std::string(name) + ": expected a non-negative integer");
  }
  return static_cast<std::size_t>(parsed);
}

std::vector<std::uint32_t> parse_playout_marking(
    const httplib::Request& request, std::string_view name) {
  const auto key = std::string(name);
  if (!request.has_param(key)) return {};
  const auto value = request.get_param_value(key);
  if (value.empty())
    throw std::invalid_argument(key + ": expected a non-empty marking");
  std::vector<std::uint32_t> result;
  std::size_t begin = 0;
  for (std::size_t index = 0; index <= value.size(); ++index) {
    if (index != value.size() && value[index] != ',') continue;
    const auto token = value.substr(begin, index - begin);
    if (token.empty())
      throw std::invalid_argument(key + ": expected non-negative entries");
    std::uint64_t parsed = 0;
    const auto converted = std::from_chars(
        token.data(), token.data() + token.size(), parsed);
    if (converted.ec != std::errc{} ||
        converted.ptr != token.data() + token.size() ||
        parsed > std::numeric_limits<std::uint32_t>::max())
      throw std::invalid_argument(key + ": expected uint32 entries");
    result.push_back(static_cast<std::uint32_t>(parsed));
    begin = index + 1;
  }
  return result;
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

struct ParsedOcelCsvOptions {
  minera::OcelCsvImportOptions import_options;
  minera::OcelCsvExportOptions export_options;
};

ParsedOcelCsvOptions parse_ocel_csv_options(const httplib::Request& request) {
  ParsedOcelCsvOptions result;
  const auto assign_string = [&](std::string_view name, std::string& import_value,
                                 std::string& export_value) {
    const auto key = std::string(name);
    if (!request.has_param(key)) return;
    const auto value = request.get_param_value(key);
    if (value.empty())
      throw std::invalid_argument(key + ": expected a non-empty value");
    import_value = value;
    export_value = value;
  };
  assign_string("ocel_csv_event_id_column",
                result.import_options.event_id_column,
                result.export_options.event_id_column);
  assign_string("ocel_csv_activity_column", result.import_options.activity_column,
                result.export_options.activity_column);
  assign_string("ocel_csv_timestamp_column",
                result.import_options.timestamp_column,
                result.export_options.timestamp_column);
  assign_string("ocel_csv_object_type_prefix",
                result.import_options.object_type_prefix,
                result.export_options.object_type_prefix);
  assign_string("ocel_csv_object_id_column",
                result.import_options.object_id_column,
                result.export_options.object_id_column);
  assign_string("ocel_csv_object_type_column",
                result.import_options.object_type_column,
                result.export_options.object_type_column);
  if (request.has_param("ocel_csv_delimiter")) {
    const auto value = request.get_param_value("ocel_csv_delimiter");
    if (value.size() != 1)
      throw std::invalid_argument(
          "ocel_csv_delimiter: expected exactly one character");
    result.import_options.delimiter = value.front();
    result.export_options.delimiter = value.front();
  }
  result.import_options.trim_fields = ocel_query_bool(
      request, "ocel_csv_trim_fields", result.import_options.trim_fields);
  result.import_options.reject_unsupported = ocel_query_bool(
      request, "ocel_csv_reject_unsupported",
      result.import_options.reject_unsupported);
  result.import_options.sort_events_by_timestamp = ocel_query_bool(
      request, "ocel_csv_sort_events_by_timestamp",
      result.import_options.sort_events_by_timestamp);
  result.export_options.allow_lossy = ocel_query_bool(
      request, "ocel_csv_allow_lossy", result.export_options.allow_lossy);
  return result;
}

std::string ocel_request_body(const httplib::Request& request,
                              std::string_view field) {
  if (request.is_multipart_form_data()) {
    return form_field(request, std::string(field));
  }
  return request.body;
}

std::vector<std::string> split_ocel_playout_value(std::string_view value,
                                                  char delimiter,
                                                  bool final_level = true) {
  return minera::split_ocel_query_value(value, delimiter, final_level);
}

std::map<std::string, std::vector<std::string>> parse_ocel_playout_objects(
    const httplib::Request& request) {
  std::map<std::string, std::vector<std::string>> result;
  if (!request.has_param("objects")) return result;
  const auto entries = split_ocel_playout_value(
      request.get_param_value("objects"), ';', false);
  for (const auto& entry : entries) {
    if (entry.empty()) throw std::invalid_argument("objects: empty entry");
    const auto fields = split_ocel_playout_value(entry, ':', false);
    if (fields.size() != 2 || fields.front().empty())
      throw std::invalid_argument(
          "objects: expected TYPE:ID1,ID2 entries");
    std::vector<std::string> ids;
    for (const auto& id : split_ocel_playout_value(fields[1], ',', true)) {
      if (id.empty()) throw std::invalid_argument(
          "objects: expected non-empty comma-separated ids");
      ids.push_back(id);
    }
    auto& target = result[fields.front()];
    target.insert(target.end(), ids.begin(), ids.end());
  }
  return result;
}

std::map<std::string, std::vector<std::uint32_t>>
parse_ocel_playout_markings(const httplib::Request& request,
                            std::string_view name) {
  std::map<std::string, std::vector<std::uint32_t>> result;
  const auto key = std::string(name);
  if (!request.has_param(key)) return result;
  const auto entries = split_ocel_playout_value(
      request.get_param_value(key), ';', false);
  for (const auto& entry : entries) {
    if (entry.empty())
      throw std::invalid_argument(key + ": empty entry");
    const auto fields = split_ocel_playout_value(entry, ':', false);
    if (fields.size() != 2 || fields.front().empty())
      throw std::invalid_argument(key + ": expected TYPE:T0,T1 entries");
    if (result.contains(fields.front()))
      throw std::invalid_argument(key +
                                  ": each object type may occur once");
    std::vector<std::uint32_t> marking;
    for (const auto& token : split_ocel_playout_value(fields[1], ',', true)) {
      if (token.empty())
        throw std::invalid_argument(key + ": expected non-empty token counts");
      errno = 0;
      char* end = nullptr;
      const auto parsed = std::strtoull(token.c_str(), &end, 10);
      if (end != token.c_str() + token.size() || errno == ERANGE ||
          parsed > std::numeric_limits<std::uint32_t>::max())
        throw std::invalid_argument(key + ": token counts must be uint32 values");
      marking.push_back(static_cast<std::uint32_t>(parsed));
    }
    result.emplace(fields.front(), std::move(marking));
  }
  return result;
}

std::vector<std::string> split_ocel_filter_value(std::string_view value,
                                                 char delimiter,
                                                 bool final_level = true);

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
                                                 char delimiter,
                                                 bool final_level) {
  return minera::split_ocel_query_value(value, delimiter, final_level);
}

std::vector<minera::OcelValue> parse_ocel_filter_values(
    const httplib::Request& request) {
  std::vector<minera::OcelValue> result;
  if (!request.has_param("attribute_values")) return result;
  for (const auto& encoded : split_ocel_filter_value(
           request.get_param_value("attribute_values"), ',', false)) {
    const auto fields = split_ocel_filter_value(encoded, ':', true);
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
      errno = 0;
      char* end = nullptr;
      const auto parsed = std::strtoll(value.c_str(), &end, 10);
      if (end != value.c_str() + value.size() || errno == ERANGE)
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
  for (const auto& item : split_ocel_filter_value(raw, ',', false)) {
    const auto fields = split_ocel_filter_value(item, ':', true);
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
    errno = 0;
    char* end_pointer = nullptr;
    const auto parsed = std::strtoull(count.c_str(), &end_pointer, 10);
    if (end_pointer != count.c_str() + count.size() ||
        errno == ERANGE ||
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
  for (const auto& item : split_ocel_filter_value(raw, ';', false)) {
    const auto fields = split_ocel_filter_value(item, ':', false);
    if (fields.size() != 2 || fields[0].empty() || fields[1].empty()) {
      throw std::invalid_argument(
          "allowed expects TYPE:ACTIVITY|ACTIVITY entries");
    }
    const auto& type = fields[0];
    for (const auto& activity : split_ocel_filter_value(fields[1], '|', true)) {
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
        "mode", "format", "reject_unsupported", "include_declaration",
        "ocel_csv_event_id_column", "ocel_csv_activity_column",
        "ocel_csv_timestamp_column", "ocel_csv_object_type_prefix",
        "ocel_csv_object_id_column", "ocel_csv_object_type_column",
        "ocel_csv_delimiter", "ocel_csv_trim_fields",
        "ocel_csv_sort_events_by_timestamp", "ocel_csv_reject_unsupported",
        "ocel_csv_allow_lossy"};
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

    const bool csv_format = format == "csv" || format == "csv_objects";
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!csv_format && key.rfind("ocel_csv_", 0) == 0)
        throw std::invalid_argument(
            "OCEL CSV options require csv or csv_objects format");
    }
    const auto csv_options = csv_format
                                 ? parse_ocel_csv_options(request)
                                 : ParsedOcelCsvOptions{};

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
        payload = minera::serialize_ocel_csv(log, csv_options.export_options);
        content_type = "text/csv";
        filename = "ocel.csv";
      } else {
        payload = minera::serialize_ocel_objects_csv(
            log, csv_options.export_options);
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
                ? minera::parse_ocel_csv(body, csv_options.import_options)
                : minera::parse_ocel_csv(body, object_body,
                                         csv_options.import_options);
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
        "variant", "compute_edges_performance", "ocpn_annotation",
        "diagnostics_with_token_based_replay", "ocpn_activity_attribute",
        "ocpn_object_type_attribute", "ocdfg_activity_attribute",
        "ocdfg_object_type_attribute", "saw_activity_attribute",
        "saw_object_type_attribute"};
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
      discovery.diagnostics_with_token_based_replay = ocel_query_bool(
          request, "diagnostics_with_token_based_replay", false);
      if (request.has_param("ocpn_activity_attribute")) {
        discovery.activity_attribute =
            request.get_param_value("ocpn_activity_attribute");
        if (discovery.activity_attribute.empty())
          throw std::invalid_argument(
              "ocpn_activity_attribute expects a non-empty value");
      }
      if (request.has_param("ocpn_object_type_attribute")) {
        discovery.object_type_attribute =
            request.get_param_value("ocpn_object_type_attribute");
        if (discovery.object_type_attribute.empty())
          throw std::invalid_argument(
              "ocpn_object_type_attribute expects a non-empty value");
      }
      payload = minera::serialize_ocpn_json(
          minera::discover_ocpn(log, discovery));
    } else if (variant == "saw") {
      minera::SawNetDiscoveryOptions options;
      if (request.has_param("saw_activity_attribute")) {
        options.activity_attribute =
            request.get_param_value("saw_activity_attribute");
        if (options.activity_attribute.empty())
          throw std::invalid_argument(
              "saw_activity_attribute expects a non-empty value");
      }
      if (request.has_param("saw_object_type_attribute")) {
        options.object_type_attribute =
            request.get_param_value("saw_object_type_attribute");
        if (options.object_type_attribute.empty())
          throw std::invalid_argument(
              "saw_object_type_attribute expects a non-empty value");
      }
      payload = minera::saw_nets_json(
          minera::discover_saw_nets(log, options));
    } else {
      minera::OcdfgDiscoveryOptions options;
      options.compute_edges_performance =
          ocel_query_bool(request, "compute_edges_performance", true);
      if (request.has_param("ocdfg_activity_attribute")) {
        options.activity_attribute = trim_text(
            request.get_param_value("ocdfg_activity_attribute"));
        if (options.activity_attribute.empty())
          throw std::invalid_argument(
              "ocdfg_activity_attribute expects a non-empty value");
      }
      if (request.has_param("ocdfg_object_type_attribute")) {
        options.object_type_attribute = trim_text(
            request.get_param_value("ocdfg_object_type_attribute"));
        if (options.object_type_attribute.empty())
          throw std::invalid_argument(
              "ocdfg_object_type_attribute expects a non-empty value");
      }
      payload = minera::serialize_ocdfg_json(
          minera::discover_ocdfg_classic_with_options(log, options));
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

void handle_ocel_enrichment(const httplib::Request& request,
                            httplib::Response& response) {
  try {
    static const std::set<std::string> allowed = {
        "graphs", "lifecycle_qualifiers"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument("unknown OCEL enrichment parameter: " + key);
    }
    minera::OcelEnrichmentOptions options;
    if (request.has_param("graphs")) {
      const auto values = ocel_query_list(request, "graphs");
      options.include_interaction = false;
      options.include_descendants = false;
      options.include_inheritance = false;
      options.include_cobirth = false;
      options.include_codeath = false;
      for (const auto& value : values) {
        if (value == "all") {
          options.include_interaction = true;
          options.include_descendants = true;
          options.include_inheritance = true;
          options.include_cobirth = true;
          options.include_codeath = true;
        } else if (value == "interaction" || value == "object_interaction_graph") {
          options.include_interaction = true;
        } else if (value == "descendants" || value == "object_descendants_graph") {
          options.include_descendants = true;
        } else if (value == "inheritance" || value == "object_inheritance_graph") {
          options.include_inheritance = true;
        } else if (value == "cobirth" || value == "object_cobirth_graph") {
          options.include_cobirth = true;
        } else if (value == "codeath" || value == "object_codeath_graph") {
          options.include_codeath = true;
        } else {
          throw std::invalid_argument(
              "graphs expects interaction, descendants, inheritance, cobirth, codeath, or all");
        }
      }
    }
    options.include_lifecycle_qualifiers = ocel_query_bool(
        request, "lifecycle_qualifiers", options.include_lifecycle_qualifiers);
    const auto body = ocel_request_body(request, "log");
    if (body.empty()) throw std::invalid_argument("missing OCEL JSON body");
    const auto enriched = minera::enrich_ocel(minera::parse_ocel_json(body), options);
    response.set_content(
        minera::serialize_ocel_enrichment_json(enriched, options),
        "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_ocel_graph(const httplib::Request& request,
                       httplib::Response& response) {
  try {
    static const std::set<std::string> allowed = {"variant"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument("unknown OCEL graph parameter: " + key);
    }
    const auto value = request.has_param("variant")
                           ? request.get_param_value("variant")
                           : std::string("interaction");
    minera::OcelEnrichmentGraph graph;
    if (value == "interaction" || value == "object_interaction_graph" ||
        value == "INTERACTION") {
      graph = minera::OcelEnrichmentGraph::kInteraction;
    } else if (value == "descendants" ||
               value == "object_descendants_graph" || value == "DESCENDANTS") {
      graph = minera::OcelEnrichmentGraph::kDescendants;
    } else if (value == "inheritance" ||
               value == "object_inheritance_graph" || value == "INHERITANCE") {
      graph = minera::OcelEnrichmentGraph::kInheritance;
    } else if (value == "cobirth" || value == "object_cobirth_graph" ||
               value == "COBIRTH") {
      graph = minera::OcelEnrichmentGraph::kCobirth;
    } else if (value == "codeath" || value == "object_codeath_graph" ||
               value == "CODEATH") {
      graph = minera::OcelEnrichmentGraph::kCodeath;
    } else {
      throw std::invalid_argument(
          "variant expects interaction, descendants, inheritance, cobirth, or codeath");
    }
    const auto body = ocel_request_body(request, "log");
    if (body.empty()) throw std::invalid_argument("missing OCEL JSON body");
    response.set_content(
        minera::serialize_ocel_object_graph_json(
            minera::discover_ocel_object_graph(minera::parse_ocel_json(body),
                                                graph)),
        "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_ocel_event_object_graph(const httplib::Request& request,
                                    httplib::Response& response) {
  try {
    static const std::set<std::string> allowed = {
        "variant", "include_df", "include_object_changes"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument(
            "unknown OCEL event/object graph parameter: " + key);
    }
    if (request.has_param("variant")) {
      const auto variant = request.get_param_value("variant");
      if (variant != "heterogeneous" && variant != "HETEROGENEOUS" &&
          variant != "event_object") {
        throw std::invalid_argument(
            "variant expects heterogeneous for the event/object graph");
      }
    }
    minera::OcelHeterogeneousGraphOptions options;
    options.include_df = ocel_query_bool(request, "include_df", true);
    options.include_object_changes =
        ocel_query_bool(request, "include_object_changes", true);
    const auto body = ocel_request_body(request, "log");
    if (body.empty()) throw std::invalid_argument("missing OCEL JSON body");
    response.set_content(
        minera::serialize_ocel_heterogeneous_graph_json(
            minera::discover_ocel_heterogeneous_graph(
                minera::parse_ocel_json(body), options)),
        "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_ocel_flatten(const httplib::Request& request,
                         httplib::Response& response) {
  try {
    static const std::set<std::string> allowed = {
        "object_type", "event_activity_attribute",
        "event_timestamp_attribute"};
    const auto all_whitespace = [](std::string_view value) {
      return !value.empty() && std::all_of(
          value.begin(), value.end(), [](unsigned char character) {
            return std::isspace(character) != 0;
          });
    };
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument(
            "unknown OCEL flatten parameter: " + key);
    }
    if (!request.has_param("object_type"))
      throw std::invalid_argument(
          "object_type is required for OCEL flattening");
    const auto object_type = request.get_param_value("object_type");
    if (object_type.empty() || all_whitespace(object_type))
      throw std::invalid_argument(
          "object_type must be a non-blank value");
    minera::OcelFlattenOptions options;
    options.object_type = object_type;
    if (request.has_param("event_activity_attribute")) {
      options.event_activity_attribute =
          request.get_param_value("event_activity_attribute");
      if (options.event_activity_attribute.empty() ||
          all_whitespace(options.event_activity_attribute))
        throw std::invalid_argument(
            "event_activity_attribute must be a non-blank value");
    }
    if (request.has_param("event_timestamp_attribute")) {
      options.event_timestamp_attribute =
          request.get_param_value("event_timestamp_attribute");
      if (options.event_timestamp_attribute.empty() ||
          all_whitespace(options.event_timestamp_attribute))
        throw std::invalid_argument(
            "event_timestamp_attribute must be a non-blank value");
    }
    const auto body = ocel_request_body(request, "log");
    if (body.empty()) throw std::invalid_argument("missing OCEL JSON body");
    const auto log = minera::parse_ocel_json(body);
    response.set_content(
        minera::to_xes(minera::flatten_ocel_to_event_log(log, options),
                       "ocel flatten"),
        "application/xml");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_ocel_playout(const httplib::Request& request,
                         httplib::Response& response) {
  try {
    static const std::set<std::string> allowed = {
        "model", "variant", "max_bindings_per_activity", "branching_factor_transitions",
        "branching_factor_bindings", "branching_factor_activities", "max_traces", "max_states", "objects",
        "initial_marking", "final_marking",
        "objects_unique_per_trace", "event_binding", "exists_trace", "return_sequences",
        "return_traces",
        "seed", "start", "case_interval", "event_interval", "ocpn_variant",
        "double_arc_threshold", "noise_threshold", "ocpn_annotation",
        "ocpn_activity_attribute", "ocpn_object_type_attribute"};
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
    options.branching_factor_transitions = ocel_query_number(
        request, "branching_factor_transitions", options.branching_factor_transitions);
    options.branching_factor_bindings = ocel_query_number(
        request, "branching_factor_bindings", options.branching_factor_bindings);
    options.branching_factor_activities = ocel_query_number(
        request, "branching_factor_activities", options.branching_factor_activities);
    if (request.has_param("branching_factor_activities") &&
        model != "causal_net")
      throw std::invalid_argument(
          "branching_factor_activities requires model=causal_net");
    options.max_traces = ocel_query_size(request, "max_traces", options.max_traces);
    options.max_states = ocel_query_size(request, "max_states", options.max_states);
    options.seed = ocel_query_size(request, "seed", options.seed);
    options.objects = parse_ocel_playout_objects(request);
    options.initial_markings =
        parse_ocel_playout_markings(request, "initial_marking");
    options.final_markings =
        parse_ocel_playout_markings(request, "final_marking");
    options.objects_unique_per_trace = ocel_query_bool(
        request, "objects_unique_per_trace", options.objects_unique_per_trace);
    options.exists_trace = ocel_query_bool(request, "exists_trace", options.exists_trace);
    options.return_sequences = ocel_query_bool(
        request, "return_sequences", options.return_sequences);
    const auto return_traces_lookup = [&](std::string_view name) {
      return name == "return_traces" && request.has_param("return_traces")
                 ? request.get_param_value("return_traces")
                 : std::string{};
    };
    const auto return_traces_present = [&](std::string_view name) {
      return name == "return_traces" && request.has_param("return_traces");
    };
    options.return_traces = minera::parse_ocpn_playout_params(
                                return_traces_lookup, return_traces_present)
                                .return_traces;
    if (request.has_param("event_binding")) {
      const auto binding = request.get_param_value("event_binding");
      if (binding == "independent") {
        options.event_binding = minera::OcpnEventBinding::kIndependent;
      } else if (binding == "shared_activity") {
        options.event_binding = minera::OcpnEventBinding::kSharedActivity;
      } else if (binding == "synchronized") {
        options.event_binding = minera::OcpnEventBinding::kSynchronized;
      } else {
        throw std::invalid_argument(
            "event_binding expects independent, shared_activity, or synchronized");
      }
    }
    options.start = ocel_query_number(request, "start", options.start);
    options.case_interval =
        ocel_query_number(request, "case_interval", options.case_interval);
    options.event_interval =
        ocel_query_number(request, "event_interval", options.event_interval);
    if (options.max_bindings_per_activity == 0 ||
        !std::isfinite(options.branching_factor_transitions) ||
        options.branching_factor_transitions < 0.0 ||
        !std::isfinite(options.branching_factor_bindings) ||
        options.branching_factor_bindings < 0.0 ||
        !std::isfinite(options.branching_factor_activities) ||
        options.branching_factor_activities < 0.0 ||
        options.max_bindings_per_activity > 100 ||
        options.branching_factor_transitions > 100 ||
        options.branching_factor_bindings > 100 ||
        options.branching_factor_activities > 100 || options.max_traces > 10000 ||
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
    if (request.has_param("ocpn_activity_attribute")) {
      discovery.activity_attribute =
          request.get_param_value("ocpn_activity_attribute");
      if (discovery.activity_attribute.empty())
        throw std::invalid_argument(
            "ocpn_activity_attribute expects a non-empty value");
    }
    if (request.has_param("ocpn_object_type_attribute")) {
      discovery.object_type_attribute =
          request.get_param_value("ocpn_object_type_attribute");
      if (discovery.object_type_attribute.empty())
        throw std::invalid_argument(
            "ocpn_object_type_attribute expects a non-empty value");
    }
    discovery.double_arc_threshold = ocel_query_number(
        request, "double_arc_threshold", discovery.double_arc_threshold);
    discovery.noise_threshold =
        ocel_query_number(request, "noise_threshold", discovery.noise_threshold);
    const auto log = minera::parse_ocel_json(body);
    if (model == "causal_net") {
      const auto causal = minera::discover_oc_causal_net(log, discovery);
      const auto result = minera::oc_causal_net_extensive_playout_result(causal,
                                                                           options);
      response.set_content(minera::serialize_oc_causal_net_playout_json(
                               result,
                               options.event_binding),
                           "application/json");
    } else {
      const auto graph = minera::discover_ocpn(log, discovery);
      const auto result = minera::ocpn_extensive_playout_result(graph, options);
      response.set_content(minera::serialize_ocpn_playout_json(
                               result,
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
        "variant", "feature_name_style", "interaction_graph_source",
        "object_neighbor_graph", "max_features", "include_activity_features",
        "include_timestamp_features", "include_relation_features",
        "include_event_num_rel_objs_features",
        "include_event_num_rel_objs_type_features",
        "include_endpoint_features", "include_attribute_counts",
        "include_event_start_ot_features", "include_event_end_ot_features",
        "include_interaction_features", "include_new_interactions_features",
        "include_degree_centrality_features",
        "include_all_object_features",
        "include_lifecycle_features",
        "include_lifecycle_length_features",
        "include_lifecycle_unique_activity_features",
        "include_lifecycle_duration_features",
        "include_lifecycle_activity_features",
        "include_lifecycle_wip_features",
        "include_interaction_by_type_features",
        "include_general_interaction_features",
        "include_general_descendants_features",
        "include_general_inheritance_features", "include_cobirth_features",
        "include_codeath_features",
        "include_related_events_features",
        "include_related_activities_features",
        "include_related_objects_features",
        "include_obj_con_in_graph_features",
        "include_prefix_features", "include_prefix_length_features",
        "include_prefix_timediff_features", "include_prefix_one_hot_features",
        "include_event_pointwise_features", "include_object_pointwise_features",
        "include_lifecycle_path_features",
        "ocel_features_filter_per_type",
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
    if (request.has_param("feature_name_style")) {
      const auto style = request.get_param_value("feature_name_style");
      if (style == "native") {
        options.feature_name_style = minera::OcelFeatureNameStyle::kNative;
      } else if (style == "compatibility") {
        options.feature_name_style =
            minera::OcelFeatureNameStyle::kCompatibility;
      } else {
        throw std::invalid_argument(
            "feature_name_style expects native or compatibility");
      }
      if (options.feature_name_style ==
              minera::OcelFeatureNameStyle::kCompatibility &&
          variant == minera::OcelFeatureVariant::kEventsObjects)
        throw std::invalid_argument(
            "compatibility feature names apply only to events or objects variants");
    }
    if (request.has_param("interaction_graph_source")) {
      const auto source = request.get_param_value("interaction_graph_source");
      if (source == "event_cooccurrence") {
        options.interaction_graph_source =
            minera::OcelInteractionGraphSource::kEventCooccurrence;
      } else if (source == "event_and_o2o") {
        options.interaction_graph_source =
            minera::OcelInteractionGraphSource::kEventAndO2O;
      } else {
        throw std::invalid_argument(
            "interaction_graph_source expects event_cooccurrence or event_and_o2o");
      }
    }
    if (request.has_param("object_neighbor_graph")) {
      const auto graph = request.get_param_value("object_neighbor_graph");
      if (graph == "interaction") {
        options.object_neighbor_graph =
            minera::OcelObjectNeighborGraph::kInteraction;
      } else if (graph == "cobirth") {
        options.object_neighbor_graph =
            minera::OcelObjectNeighborGraph::kCobirth;
      } else if (graph == "codeath") {
        options.object_neighbor_graph =
            minera::OcelObjectNeighborGraph::kCodeath;
      } else {
        throw std::invalid_argument(
            "object_neighbor_graph expects interaction, cobirth, or codeath");
      }
    }
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
    if (request.has_param("include_event_num_rel_objs_features"))
      options.include_event_num_rel_objs_features = ocel_query_bool(
          request, "include_event_num_rel_objs_features", false);
    if (request.has_param("include_event_num_rel_objs_type_features"))
      options.include_event_num_rel_objs_type_features = ocel_query_bool(
          request, "include_event_num_rel_objs_type_features", false);
    options.include_endpoint_features = ocel_query_bool(
        request, "include_endpoint_features", options.include_endpoint_features);
    if (request.has_param("include_event_start_ot_features"))
      options.include_event_start_ot_features = ocel_query_bool(
          request, "include_event_start_ot_features", false);
    if (request.has_param("include_event_end_ot_features"))
      options.include_event_end_ot_features = ocel_query_bool(
          request, "include_event_end_ot_features", false);
    options.include_attribute_counts = ocel_query_bool(
        request, "include_attribute_counts", options.include_attribute_counts);
    options.include_interaction_features = ocel_query_bool(
        request, "include_interaction_features", options.include_interaction_features);
    if (request.has_param("include_new_interactions_features"))
      options.include_new_interactions_features = ocel_query_bool(
          request, "include_new_interactions_features", false);
    if (request.has_param("include_degree_centrality_features")) {
      options.include_degree_centrality_features = ocel_query_bool(
          request, "include_degree_centrality_features", false);
    }
    if (request.has_param("include_all_object_features")) {
      options.include_all_object_features = ocel_query_bool(
          request, "include_all_object_features", false);
    }
    if (request.has_param("include_interaction_by_type_features"))
      options.include_interaction_by_type_features = ocel_query_bool(
          request, "include_interaction_by_type_features", false);
    if (request.has_param("include_general_interaction_features"))
      options.include_general_interaction_features = ocel_query_bool(
          request, "include_general_interaction_features", false);
    if (request.has_param("include_general_descendants_features"))
      options.include_general_descendants_features = ocel_query_bool(
          request, "include_general_descendants_features", false);
    if (request.has_param("include_general_inheritance_features"))
      options.include_general_inheritance_features = ocel_query_bool(
          request, "include_general_inheritance_features", false);
    if (request.has_param("include_cobirth_features"))
      options.include_cobirth_features = ocel_query_bool(
          request, "include_cobirth_features", false);
    if (request.has_param("include_codeath_features"))
      options.include_codeath_features = ocel_query_bool(
          request, "include_codeath_features", false);
    if (request.has_param("include_lifecycle_features"))
      options.include_lifecycle_features = ocel_query_bool(
          request, "include_lifecycle_features", true);
    if (request.has_param("include_lifecycle_length_features"))
      options.include_lifecycle_length_features = ocel_query_bool(
          request, "include_lifecycle_length_features", true);
    if (request.has_param("include_lifecycle_unique_activity_features"))
      options.include_lifecycle_unique_activity_features = ocel_query_bool(
          request, "include_lifecycle_unique_activity_features", true);
    if (request.has_param("include_lifecycle_duration_features"))
      options.include_lifecycle_duration_features = ocel_query_bool(
          request, "include_lifecycle_duration_features", true);
    if (request.has_param("include_lifecycle_activity_features"))
      options.include_lifecycle_activity_features = ocel_query_bool(
          request, "include_lifecycle_activity_features", true);
    if (request.has_param("include_lifecycle_path_features"))
      options.include_lifecycle_path_features = ocel_query_bool(
          request, "include_lifecycle_path_features", false);
    if (request.has_param("include_lifecycle_wip_features"))
      options.include_lifecycle_wip_features = ocel_query_bool(
          request, "include_lifecycle_wip_features", false);
    options.include_related_events_features = ocel_query_bool(
        request, "include_related_events_features",
        options.include_related_events_features);
    options.include_related_activities_features = ocel_query_bool(
        request, "include_related_activities_features",
        options.include_related_activities_features);
    options.include_related_objects_features = ocel_query_bool(
        request, "include_related_objects_features",
        options.include_related_objects_features);
    options.include_obj_con_in_graph_features = ocel_query_bool(
        request, "include_obj_con_in_graph_features",
        options.include_obj_con_in_graph_features);
    options.include_prefix_features = ocel_query_bool(
        request, "include_prefix_features", options.include_prefix_features);
    if (request.has_param("include_prefix_length_features"))
      options.include_prefix_length_features = ocel_query_bool(
          request, "include_prefix_length_features", false);
    if (request.has_param("include_prefix_timediff_features"))
      options.include_prefix_timediff_features = ocel_query_bool(
          request, "include_prefix_timediff_features", false);
    if (request.has_param("include_prefix_one_hot_features"))
      options.include_prefix_one_hot_features = ocel_query_bool(
          request, "include_prefix_one_hot_features", false);
    if (request.has_param("include_event_pointwise_features"))
      options.include_event_pointwise_features = ocel_query_bool(
          request, "include_event_pointwise_features", false);
    if (request.has_param("include_object_pointwise_features"))
      options.include_object_pointwise_features = ocel_query_bool(
          request, "include_object_pointwise_features", false);
    const auto object_only_control_supplied =
        request.has_param("include_related_events_features") ||
        request.has_param("include_related_activities_features") ||
        request.has_param("include_obj_con_in_graph_features") ||
        request.has_param("include_lifecycle_features") ||
        request.has_param("include_lifecycle_length_features") ||
        request.has_param("include_lifecycle_unique_activity_features") ||
        request.has_param("include_lifecycle_duration_features") ||
        request.has_param("include_lifecycle_activity_features") ||
        request.has_param("include_lifecycle_wip_features") ||
        request.has_param("include_lifecycle_path_features");
    const auto related_object_control_supplied =
        request.has_param("include_related_objects_features");
    const auto event_control_supplied =
        request.has_param("include_event_num_rel_objs_features") ||
        request.has_param("include_event_num_rel_objs_type_features") ||
        request.has_param("include_event_start_ot_features") ||
        request.has_param("include_event_end_ot_features") ||
        request.has_param("include_new_interactions_features");
    const auto related_object_event_context =
        options.include_related_objects_features &&
        variant == minera::OcelFeatureVariant::kEvents;
    const auto degree_control_supplied =
        request.has_param("include_degree_centrality_features");
    const auto all_object_control_supplied =
        request.has_param("include_all_object_features");
    const auto object_graph_control_supplied =
        request.has_param("include_interaction_by_type_features") ||
        request.has_param("include_general_interaction_features") ||
        request.has_param("include_general_descendants_features") ||
        request.has_param("include_general_inheritance_features") ||
        request.has_param("include_cobirth_features") ||
        request.has_param("include_codeath_features");
    const auto interaction_graph_source_supplied =
        request.has_param("interaction_graph_source");
    const auto object_neighbor_graph_supplied =
        request.has_param("object_neighbor_graph");
    if (all_object_control_supplied &&
        variant != minera::OcelFeatureVariant::kObjects)
      throw std::invalid_argument(
          "all-object features apply only to the objects variant");
    if (degree_control_supplied && variant != minera::OcelFeatureVariant::kObjects &&
        !related_object_event_context)
      throw std::invalid_argument(
          "degree centrality features apply only to the objects variant");
    if (object_graph_control_supplied &&
        variant != minera::OcelFeatureVariant::kObjects &&
        !related_object_event_context)
      throw std::invalid_argument(
          "object graph feature controls apply only to the objects variant");
    if (object_neighbor_graph_supplied &&
        variant != minera::OcelFeatureVariant::kObjects &&
        !related_object_event_context)
      throw std::invalid_argument(
          "neighbor graph selection applies only to the objects variant");
    if (object_neighbor_graph_supplied &&
        !options.include_obj_con_in_graph_features)
      throw std::invalid_argument(
          "object_neighbor_graph requires include_obj_con_in_graph_features");
    if (interaction_graph_source_supplied &&
        variant != minera::OcelFeatureVariant::kObjects &&
        !related_object_event_context)
      throw std::invalid_argument(
          "interaction graph source applies only to the objects variant");
    if (object_only_control_supplied &&
        variant != minera::OcelFeatureVariant::kObjects &&
        !related_object_event_context)
      throw std::invalid_argument(
          "related-event, neighbor, and lifecycle metric options apply only to the objects variant");
    if (related_object_control_supplied &&
        variant != minera::OcelFeatureVariant::kEvents)
      throw std::invalid_argument(
          "related-object features apply only to the events variant");
    if (event_control_supplied && variant != minera::OcelFeatureVariant::kEvents)
      throw std::invalid_argument(
          "independent event feature controls apply only to the events variant");
    const auto prefix_control_supplied =
        request.has_param("include_prefix_features") ||
        request.has_param("include_prefix_length_features") ||
        request.has_param("include_prefix_timediff_features") ||
        request.has_param("include_prefix_one_hot_features");
    const auto pointwise_control_supplied =
        request.has_param("include_event_pointwise_features") ||
        request.has_param("include_object_pointwise_features");
    if (prefix_control_supplied && variant != minera::OcelFeatureVariant::kEventsObjects)
      throw std::invalid_argument(
          "prefix features apply only to the events_objects variant");
    if (pointwise_control_supplied &&
        variant != minera::OcelFeatureVariant::kEventsObjects)
      throw std::invalid_argument(
          "pointwise features apply only to the events_objects variant");
    options.numeric_event_attributes =
        ocel_query_list(request, "numeric_event_attributes");
    options.string_event_attributes =
        ocel_query_list(request, "string_event_attributes");
    options.numeric_object_attributes =
        ocel_query_list(request, "numeric_object_attributes");
    options.string_object_attributes =
        ocel_query_list(request, "string_object_attributes");
    if (request.has_param("ocel_features_filter_per_type")) {
      options.filter_per_type =
          request.get_param_value("ocel_features_filter_per_type");
    }
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
        "otg_theta", "otg_theta_interaction", "otg_theta_descendants",
        "otg_theta_inheritance", "otg_theta_cobirth", "otg_theta_codeath",
        "ocdfg_theta_activity", "ocdfg_theta_flow", "real",
        "real_graph_kind", "ocdfg_activity_attribute",
        "ocdfg_object_type_attribute"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument(
            "unknown OCEL conformance parameter: " + key);
    }
    const auto real_mode = request.has_param("real")
                               ? lower_ascii(request.get_param_value("real"))
                               : std::string("ocel");
    if (real_mode != "ocel" && real_mode != "graph") {
      throw std::invalid_argument("real expects ocel or graph");
    }
    const auto requested_graph_kind = request.has_param("real_graph_kind")
                                          ? lower_ascii(request.get_param_value(
                                                "real_graph_kind"))
                                          : std::string{};
    const auto variant = request.has_param("variant")
                             ? request.get_param_value("variant")
                             : (real_mode == "graph" &&
                                        !requested_graph_kind.empty()
                                    ? requested_graph_kind
                                    : std::string("ocdfg"));
    if (variant != "etot" && variant != "ocdfg" && variant != "otg") {
      throw std::invalid_argument("variant expects etot, ocdfg, or otg");
    }
    const auto real_graph_kind = requested_graph_kind.empty()
                                     ? variant
                                     : requested_graph_kind;
    if (real_graph_kind != "etot" && real_graph_kind != "ocdfg" &&
        real_graph_kind != "otg") {
      throw std::invalid_argument(
          "real_graph_kind expects etot, ocdfg, or otg");
    }
    if (real_mode == "ocel" && request.has_param("real_graph_kind")) {
      throw std::invalid_argument(
          "real_graph_kind requires real=graph");
    }
    if (real_mode == "graph" && real_graph_kind != variant) {
      throw std::invalid_argument(
          "real_graph_kind must match the conformance variant");
    }
    if (!request.is_multipart_form_data()) {
      throw std::invalid_argument(
          "OCEL conformance requires multipart fields log or real_graph and normative");
    }
    const auto real_body = form_field(
        request, real_mode == "graph" ? "real_graph" : "log");
    const auto normative_body = form_field(request, "normative");
    if (real_body.empty() || normative_body.empty()) {
      throw std::invalid_argument(
          "OCEL conformance requires multipart fields log or real_graph and normative");
    }
    std::optional<minera::Ocel> real_ocel;
    if (real_mode == "ocel") real_ocel = minera::parse_ocel_json(real_body);
    std::optional<minera::Ocel> normative_ocel;
    const auto normative_log = [&]() -> const minera::Ocel& {
      if (!normative_ocel) normative_ocel = minera::parse_ocel_json(normative_body);
      return *normative_ocel;
    };
    const auto alpha = ocel_conformance_query_number(request, "alpha", 1.0);
    const auto beta = ocel_conformance_query_number(request, "beta", 1.0);
    const auto gamma = ocel_conformance_query_number(request, "gamma", 1.0);
    std::string payload;
    if (variant == "etot") {
      minera::EtotConformanceOptions options;
      options.alpha = alpha;
      options.beta = beta;
      options.gamma = gamma;
      options.theta_relative =
          ocel_conformance_query_number(request, "etot_theta", options.theta_relative);
      std::optional<minera::EtotGraph> real_graph;
      if (real_mode == "graph")
        real_graph = minera::parse_etot_json(real_body);
      std::optional<minera::EtotGraph> graph;
      try {
        graph = minera::parse_etot_json(normative_body);
      } catch (const std::exception&) {
        // Multipart normative inputs may also be regular OCEL documents.
      }
      if (real_graph) {
        if (graph) {
          payload = minera::etot_conformance_json(
              minera::compare_etot(*real_graph, *graph, options));
        } else {
          payload = minera::etot_conformance_json(minera::compare_etot(
              *real_graph, minera::discover_etot_classic(normative_log()),
              options));
        }
      } else if (graph) {
        payload = minera::etot_conformance_json(
            minera::compare_etot(*real_ocel, *graph, options));
      } else {
        payload = minera::etot_conformance_json(minera::compare_etot(
            minera::discover_etot_classic(*real_ocel),
            minera::discover_etot_classic(normative_log()), options));
      }
    } else if (variant == "otg") {
      minera::OtgConformanceOptions options;
      options.alpha = alpha;
      options.beta = beta;
      options.gamma = gamma;
      options.theta =
          ocel_conformance_query_number(request, "otg_theta", options.theta);
      const std::array<std::pair<std::string_view, std::string_view>, 5>
          relation_thresholds = {{
              {"otg_theta_interaction", "object_interaction"},
              {"otg_theta_descendants", "object_descendants"},
              {"otg_theta_inheritance", "object_inheritance"},
              {"otg_theta_cobirth", "object_cobirth"},
              {"otg_theta_codeath", "object_codeath"},
          }};
      for (const auto& [parameter, relation] : relation_thresholds) {
        if (request.has_param(std::string(parameter))) {
          options.theta_by_relation[std::string(relation)] =
              ocel_conformance_query_number(request, parameter, options.theta);
        }
      }
      std::optional<minera::OtgGraph> real_graph;
      if (real_mode == "graph")
        real_graph = minera::parse_otg_json(real_body);
      std::optional<minera::OtgGraph> graph;
      try {
        graph = minera::parse_otg_json(normative_body);
      } catch (const std::exception&) {
        // Multipart normative inputs may also be regular OCEL documents.
      }
      if (real_graph) {
        if (graph) {
          payload = minera::otg_conformance_json(
              minera::compare_otg(*real_graph, *graph, options));
        } else {
          payload = minera::otg_conformance_json(minera::compare_otg(
              *real_graph, minera::discover_otg_classic(normative_log()),
              options));
        }
      } else if (graph) {
        payload = minera::otg_conformance_json(
            minera::compare_otg(*real_ocel, *graph, options));
      } else {
        payload = minera::otg_conformance_json(minera::compare_otg(
            minera::discover_otg_classic(*real_ocel),
            minera::discover_otg_classic(normative_log()), options));
      }
    } else {
      minera::OcdfgConformanceOptions options;
      options.alpha = alpha;
      options.beta = beta;
      options.gamma = gamma;
      options.delta = ocel_conformance_query_number(request, "delta", options.delta);
      options.theta_activity = ocel_conformance_query_number(
          request, "ocdfg_theta_activity", options.theta_activity);
      options.theta_flow =
          ocel_conformance_query_number(request, "ocdfg_theta_flow", options.theta_flow);
      if (request.has_param("ocdfg_activity_attribute")) {
        options.activity_attribute =
            trim_text(request.get_param_value("ocdfg_activity_attribute"));
        if (options.activity_attribute.empty())
          throw std::invalid_argument(
              "ocdfg_activity_attribute expects a non-empty value");
      }
      if (request.has_param("ocdfg_object_type_attribute")) {
        options.object_type_attribute =
            trim_text(
                request.get_param_value("ocdfg_object_type_attribute"));
        if (options.object_type_attribute.empty())
          throw std::invalid_argument(
              "ocdfg_object_type_attribute expects a non-empty value");
      }
      std::optional<minera::OcdfgGraph> real_graph;
      if (real_mode == "graph")
        real_graph = minera::parse_ocdfg_json(real_body);
      std::optional<minera::OcdfgGraph> graph;
      try {
        graph = minera::parse_ocdfg_json(normative_body);
      } catch (const std::exception&) {
        // Multipart normative inputs may also be regular OCEL documents.
      }
      if (real_graph) {
        if (graph) {
          payload = minera::ocdfg_conformance_json(
              minera::compare_ocdfg(*real_graph, *graph, options));
        } else {
          minera::OcdfgDiscoveryOptions discovery;
          discovery.compute_edges_performance = false;
          discovery.activity_attribute = options.activity_attribute;
          discovery.object_type_attribute = options.object_type_attribute;
          payload = minera::ocdfg_conformance_json(minera::compare_ocdfg(
              *real_graph,
              minera::discover_ocdfg_classic_with_options(normative_log(),
                                                          discovery),
              options));
        }
      } else if (graph) {
        payload = minera::ocdfg_conformance_json(
            minera::compare_ocdfg(*real_ocel, *graph, options));
      } else {
        minera::OcdfgDiscoveryOptions discovery;
        discovery.compute_edges_performance = false;
        discovery.activity_attribute = options.activity_attribute;
        discovery.object_type_attribute = options.object_type_attribute;
        payload = minera::ocdfg_conformance_json(minera::compare_ocdfg(
            minera::discover_ocdfg_classic_with_options(*real_ocel,
                                                        discovery),
            minera::discover_ocdfg_classic_with_options(normative_log(),
                                                        discovery),
            options));
      }
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
        "ilp_activity_key", "ilp_artificial_start_activity",
        "ilp_artificial_end_activity", "ilp_alpha", "ilp_causal_relation",
        "ilp_noise_threshold", "ilp_max_nodes"};
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
void handle_genetic_request(const httplib::Request& request,
                            httplib::Response& response,
                            bool convert_matrix) {
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
        "genetic_timestamp_key", "genetic_case_id_key",
        "genetic_generations", "genetic_elitism_rate",
        "genetic_crossover_rate", "genetic_mutation_rate",
        "genetic_elitism_min_sample", "genetic_max_activities",
        "genetic_max_states", "genetic_conversion_variant"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument("unknown genetic parameter: " + key);
      if (!convert_matrix && key == "genetic_conversion_variant")
        throw std::invalid_argument(
            "genetic_conversion_variant requires the conversion endpoint");
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
    if (convert_matrix) {
      const auto variant = request.has_param("genetic_conversion_variant")
          ? minera::parse_genetic_matrix_conversion_variant(
                request.get_param_value("genetic_conversion_variant"))
          : minera::GeneticMatrixConversionVariant::kToPetriNet;
      response.set_content(minera::genetic_matrix_conversion_json(
                               minera::convert_genetic_matrix(result.matrix,
                                                              variant)),
                           "application/json");
    } else {
      response.set_content(minera::genetic_miner_json(result),
                           "application/json");
    }
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_genetic_mine(const httplib::Request& request,
                         httplib::Response& response) {
  handle_genetic_request(request, response, false);
}

void handle_genetic_matrix_conversion(const httplib::Request& request,
                                      httplib::Response& response) {
  handle_genetic_request(request, response, true);
}

// Cached-session adapter for native event-data conversion views.
void handle_log_conversion(const httplib::Request& request,
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
        "log_conversion_variant",
        "log_conversion_include_case_attributes",
        "log_conversion_case_attribute_prefix",
        "log_conversion_case_id_key",
        "log_conversion_stream_postprocessing",
        "log_conversion_max_rows_to_materialize",
        "log_conversion_deep_copy",
        "log_conversion_compress",
        "log_conversion_lazy_output",
        "log_conversion_graph_case_id_attribute",
        "log_conversion_include_df",
        "log_conversion_case_attributes_as_nodes",
        "log_conversion_event_attributes_as_nodes",
        "log_conversion_return_variants",
        "log_conversion_max_variants_to_return",
        "log_conversion_max_graph_nodes",
        "log_conversion_max_graph_edges"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument("unknown log conversion parameter: " +
                                    key);
    }
    for (const auto& [key, value] : request.files) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument("unknown log conversion parameter: " +
                                    key);
    }

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
    auto options = minera::parse_log_conversion_params(lookup, present);
    if (!options.max_rows_to_materialize) {
      options.max_rows_to_materialize = kServerLogConversionMaxRows;
    } else if (*options.max_rows_to_materialize >
               kServerLogConversionMaxRows) {
      throw std::invalid_argument(
          "log_conversion_max_rows_to_materialize exceeds the server safety cap");
    }
    if (options.max_graph_nodes > kServerLogConversionMaxGraphNodes) {
      if (present("log_conversion_max_graph_nodes"))
        throw std::invalid_argument(
            "log_conversion_max_graph_nodes exceeds the server safety cap");
      options.max_graph_nodes = kServerLogConversionMaxGraphNodes;
    }
    if (options.max_graph_edges > kServerLogConversionMaxGraphEdges) {
      if (present("log_conversion_max_graph_edges"))
        throw std::invalid_argument(
            "log_conversion_max_graph_edges exceeds the server safety cap");
      options.max_graph_edges = kServerLogConversionMaxGraphEdges;
    }
    if (!options.max_variants_to_return ||
        *options.max_variants_to_return == 0) {
      options.max_variants_to_return = kServerLogConversionMaxVariants;
    } else if (*options.max_variants_to_return >
               kServerLogConversionMaxVariants) {
      throw std::invalid_argument(
          "log_conversion_max_variants_to_return exceeds the server safety cap");
    }
    const auto result = minera::convert_event_log(session->log, options);
    response.set_content(minera::log_conversion_json(result),
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
        "sna_case_id_attribute",
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
        "link_analysis_sorting_column", "link_analysis_index_column",
        "link_analysis_look_forward",
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

void handle_knowledge_injection(const httplib::Request& request,
                                httplib::Response& response) {
  try {
    static const std::set<std::string> allowed = {
        "knowledge_variant", "knowledge_description", "knowledge_query",
        "knowledge_database_schema", "knowledge_max_input_bytes",
        "knowledge_max_prompt_bytes"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument("unknown knowledge parameter: " + key);
    }
    for (const auto& [key, value] : request.files) {
      if (!allowed.contains(key)) throw std::invalid_argument("unknown knowledge field: " + key);
      if (request.has_param(key)) throw std::invalid_argument("duplicate knowledge field: " + key);
      if (!value.filename.empty()) throw std::invalid_argument("knowledge fields must be text");
    }
    const auto lookup = [&](std::string_view name) {
      const auto key = std::string(name);
      if (request.has_file(key)) return form_field(request, key);
      return request.has_param(key) ? request.get_param_value(key)
                                    : std::string{};
    };
    const auto present = [&](std::string_view name) {
      return request.has_param(std::string(name)) || request.has_file(std::string(name));
    };
    response.set_content(
        minera::knowledge_injection_json(minera::build_knowledge_prompt(
            minera::parse_knowledge_injection_params(lookup, present))),
        "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

bool llm_origin_allowed(const httplib::Request& request, httplib::Response& response) {
  if (!request.has_header("Origin")) return true;
  const auto origin = request.get_header_value("Origin");
  if (!g_llm_origins.contains(origin)) {
    response.status = 403;
    response.set_content(error_json("LLM requests must come from the local Minera application"), "application/json");
    return false;
  }
  response.set_header("Access-Control-Allow-Origin", origin);
  return true;
}

void handle_llm_status(const httplib::Request& request, httplib::Response& response) {
  if (!llm_origin_allowed(request, response)) return;
  if (g_llm_runtime_url.empty()) {
    response.set_content("{\"family\":\"LLM_RUNTIME\",\"ready\":false,\"providers\":[],\"error\":\"Start the optional LLM runtime and configure --llm-runtime-url\"}", "application/json");
    return;
  }
  try {
    if (!request.params.empty()) throw std::invalid_argument("LLM status does not accept query parameters");
    const auto result = minera::llm_runtime::request(g_llm_runtime_url, "/health");
    response.status = result->status;
    response.set_content(result->body, "application/json");
  } catch (const std::exception& error) {
    response.status = 502; response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_llm_complete(const httplib::Request& request, httplib::Response& response) {
  if (!llm_origin_allowed(request, response)) return;
  if (g_llm_runtime_url.empty()) {
    response.status = 503;
    response.set_content(error_json("No LLM runtime configured; start it and pass --llm-runtime-url"), "application/json");
    return;
  }
  try {
    if (!request.params.empty()) throw std::invalid_argument("LLM completion accepts JSON body fields, not query parameters");
    const auto content_type = request.get_header_value("Content-Type");
    if (content_type.substr(0, content_type.find(';')) != "application/json") {
      response.status = 415; response.set_content(error_json("LLM completion requires application/json"), "application/json"); return;
    }
    if (request.body.size() > minera::llm_runtime::max_input_bytes) {
      response.status = 413; response.set_content(error_json("LLM request exceeds the application input bound"), "application/json"); return;
    }
    const auto result = minera::llm_runtime::request(g_llm_runtime_url, "/complete", &request.body);
    response.status = result->status;
    response.set_content(result->body, "application/json");
  } catch (const std::exception& error) {
    response.status = 502; response.set_content(error_json(error.what()), "application/json");
  }
}

std::shared_ptr<const MinedSession> query_session(const httplib::Request& request){
  std::shared_ptr<const MinedSession> session;{std::lock_guard<std::mutex> lock(g_session_mutex);session=g_session;}
  if(!session)throw std::invalid_argument("no cached log; open a CSV or XES log first");
  if(request.has_param("source_id")&&request.get_param_value("source_id")!=std::to_string(session->query_source_id))throw std::invalid_argument("the source log changed; refresh the query workspace");
  return session;
}
void validate_query_fields(const httplib::Request& request){
  static const std::set<std::string> allowed={"engine","limit","source_id","mode","format"};
  for(const auto& [key,value]:request.params){(void)value;if(!allowed.contains(key))throw std::invalid_argument("unknown query parameter: "+key);}
  if(request.body.empty()||request.body.size()>1024u*1024u)throw std::invalid_argument("SQL must be nonempty and within 1 MiB");
}
std::string query_engine(const httplib::Request& request){const auto engine=request.has_param("engine")?request.get_param_value("engine"):"sqlite";if(engine!="sqlite"&&engine!="duckdb")throw std::invalid_argument("query engine must be sqlite or duckdb");return engine;}
std::size_t query_limit(const httplib::Request& request,std::size_t fallback){
  if(!request.has_param("limit"))return fallback;
  const auto text=request.get_param_value("limit");std::size_t limit=0;const auto parsed=std::from_chars(text.data(),text.data()+text.size(),limit);
  if(parsed.ec!=std::errc{}||parsed.ptr!=text.data()+text.size()||limit<1||limit>1000000)throw std::invalid_argument("query limit must be from 1 through 1000000");return limit;
}
std::string duckdb_query_body(const minera::LogQueryTable& table,std::string_view sql,std::size_t limit,std::string_view source){
  const auto quote=minera::embedding_runtime::detail::escape_json;
  return "{\"sql\":"+quote(sql)+",\"max_rows\":"+std::to_string(limit)+",\"source_id\":"+quote(source)+",\"data\":"+minera::log_query_table_json(table)+'}';
}
void handle_log_query_schema(const httplib::Request& request,httplib::Response& response){
 try{
  const auto session=query_session(request);const auto table=minera::log_query_table(session->log);const auto quote=minera::embedding_runtime::detail::escape_json;
  std::string body="{\"family\":\"LOG_QUERY_SCHEMA\",\"source_id\":"+quote(std::to_string(session->query_source_id))+",\"event_count\":"+std::to_string(session->log.event_count())+",\"case_count\":"+std::to_string(session->log.trace_count())+",\"table_name\":\"dataframe\",\"event_index_column\":"+quote(table.event_index_column)+",\"sqlite_available\":"+(minera::sqlite_log_query_available()?"true":"false")+",\"duckdb_configured\":"+(!g_query_runtime_url.empty()?"true":"false")+",\"columns\":[";
  for(std::size_t c=0;c<table.table.columns.size();++c){if(c)body+=',';body+=quote(table.table.columns[c]);}body+="],\"cluster_runtime_configured\":"+std::string(!g_query_runtime_url.empty()?"true":"false")+"}";response.set_content(body,"application/json");
 }catch(const std::exception& error){response.status=400;response.set_content(error_json(error.what()),"application/json");}
}
void handle_log_query(const httplib::Request& request,httplib::Response& response){
 try{
  validate_query_fields(request);const auto session=query_session(request);const auto table=minera::log_query_table(session->log);const auto engine=query_engine(request);const auto limit=query_limit(request,1000);const auto source=std::to_string(session->query_source_id);
  if(engine=="sqlite"){
   minera::LogQueryOptions options;options.max_rows=limit;const auto result=minera::query_log_sqlite(table,request.body,options);auto body=minera::log_query_json(result);body.pop_back();body+=",\"source_id\":\""+source+"\"}";response.set_content(body,"application/json");
  }else{
   if(g_query_runtime_url.empty())throw std::invalid_argument("DuckDB requires --query-runtime-url and scripts/query_runtime.py");
   const auto body=duckdb_query_body(table,request.body,limit,source);const auto result=minera::query_runtime::request(g_query_runtime_url,"/query",&body);response.status=result->status;response.set_content(result->body,"application/json");
  }
 }catch(const std::exception& error){response.status=400;response.set_content(error_json(error.what()),"application/json");}
}
void handle_log_query_export(const httplib::Request& request,httplib::Response& response){
 try{
  validate_query_fields(request);if(!request.has_param("source_id"))throw std::invalid_argument("source_id is required for filtered log export");
  const auto session=query_session(request);const auto table=minera::log_query_table(session->log);const auto engine=query_engine(request);std::vector<std::size_t> indices;
  if(engine=="sqlite"){
   minera::LogQueryOptions options;options.max_rows=1000000;const auto result=minera::query_log_sqlite(table,request.body,options);if(!result.can_filter||result.output_truncated)throw std::invalid_argument("filter SQL must return valid event indices for a complete selection");indices=result.selected_event_indices;
  }else{
   if(g_query_runtime_url.empty())throw std::invalid_argument("DuckDB requires --query-runtime-url");
   const auto body=duckdb_query_body(table,request.body,1000000,std::to_string(session->query_source_id));const auto result=minera::query_runtime::request(g_query_runtime_url,"/select",&body);if(result->status!=200){response.status=result->status;response.set_content(result->body,"application/json");return;}indices=minera::query_runtime::parse_indices(result->body,session->log.event_count());
  }
  const auto mode=request.has_param("mode")?request.get_param_value("mode"):"cases";if(mode!="cases"&&mode!="events")throw std::invalid_argument("filter mode must be cases or events");
  const auto filtered=minera::filter_query_events(session->log,indices,mode=="cases");const auto format=request.has_param("format")?request.get_param_value("format"):"xes";
  response.set_header("X-Minera-Event-Count",std::to_string(filtered.event_count()));response.set_header("X-Minera-Case-Count",std::to_string(filtered.trace_count()));
  if(format=="xes"){minera::XesExportOptions options;options.preserve_nulls=true;response.set_content(minera::serialize_xes(filtered,options),"application/xml");}
  else if(format=="json"){minera::LogConversionOptions options;options.variant=minera::LogConversionVariant::kToDataFrame;response.set_content(minera::log_conversion_json(minera::convert_event_log(filtered,options)),"application/json");}
  else throw std::invalid_argument("filter export format must be xes or json");
 }catch(const std::exception& error){response.status=400;response.set_content(error_json(error.what()),"application/json");}
}

void handle_log_cluster(const httplib::Request& request,httplib::Response& response){
 try{
  static const std::set<std::string> allowed={"source_id","name","format","activity_attribute"};
  for(const auto& [key,value]:request.params){(void)value;if(!allowed.contains(key)||request.params.count(key)!=1)throw std::invalid_argument("unknown or duplicate cluster parameter: "+key);}
  if(!request.has_param("source_id"))throw std::invalid_argument("source_id is required for case clustering");
  if(request.body.size()>1024u*1024u)throw std::invalid_argument("cluster regex exceeds 1 MiB");
  if(g_query_runtime_url.empty()){response.status=503;response.set_content(error_json("case clustering requires --query-runtime-url and scripts/query_runtime.py"),"application/json");return;}
  const auto session=query_session(request);const auto name=request.has_param("name")?request.get_param_value("name"):"";const auto activity=request.has_param("activity_attribute")?request.get_param_value("activity_attribute"):"";
  const auto indices=minera::log_cluster_runtime::select(g_query_runtime_url,session->log,request.body,activity);const auto filtered=minera::filter_query_events(session->log,indices,true);
  response.set_header("X-Minera-Event-Count",std::to_string(filtered.event_count()));response.set_header("X-Minera-Case-Count",std::to_string(filtered.trace_count()));
  const auto format=request.has_param("format")?request.get_param_value("format"):"summary";
  if(format=="summary")response.set_content(minera::log_cluster_runtime::summary(filtered,name,request.body,indices,std::to_string(session->query_source_id)),"application/json");
  else if(format=="xes"){minera::XesExportOptions options;options.preserve_nulls=true;response.set_content(minera::serialize_xes(filtered,options),"application/xml");}
  else if(format=="json")response.set_content(minera::log_cluster_runtime::table_json(session->log,indices),"application/json");
  else throw std::invalid_argument("cluster format must be summary, xes or json");
 }catch(const std::exception& error){response.status=400;response.set_content(error_json(error.what()),"application/json");}
}

void handle_llm_context(const httplib::Request& request,httplib::Response& response){
 try{
  for(const auto& [key,value]:request.params){(void)value;if(!minera::context_tools::fields.contains(key)||request.params.count(key)!=1)throw std::invalid_argument("unknown or duplicate context parameter: "+key);}
  for(const auto& [key,file]:request.files){(void)file;if(key!="ocel"&&!minera::context_tools::fields.contains(key))throw std::invalid_argument("unknown context field: "+key);if(request.files.count(key)!=1||request.has_param(key))throw std::invalid_argument("duplicate context field: "+key);}
  const auto get=[&](std::string_view name){const auto key=std::string(name);return request.has_param(key)?request.get_param_value(key):request.get_file_value(key).content;};
  const auto has=[&](std::string_view name){return request.has_param(std::string(name))||request.has_file(std::string(name));};const auto options=minera::context_tools::options(get,has);minera::LlmContextResult result;std::string source;
  if(minera::llm_context_is_ocel(options.variant)){
   if(!request.has_file("ocel"))throw std::invalid_argument("OCEL context requires its original OCEL JSON file");const auto& body=request.get_file_value("ocel").content;result=minera::build_llm_context(minera::parse_ocel_json(body),options);source=has("source_id")?get("source_id"):"ocel-upload";
  }else{
   if(request.has_file("ocel"))throw std::invalid_argument("traditional context cannot use an OCEL file");if(!has("source_id"))throw std::invalid_argument("source_id is required for a cached-log context");const auto session=query_session(request);source=std::to_string(session->query_source_id);if(get("source_id")!=source)throw std::invalid_argument("the source log changed; refresh the context source");
   const minera::PetriNet* net=nullptr;const minera::PnmlDocument* document=nullptr;
   if(options.variant=="petri_net"){const auto miner=has("miner")?get("miner"):"inductive";const auto found=session->nets.find(miner);if(found==session->nets.end())throw std::invalid_argument("the requested context model is unavailable");net=&found->second;if(miner=="imported"&&session->imported_document)document=&*session->imported_document;}
   result=minera::build_llm_context(session->log,options,net,document);
  }
  response.set_content(minera::llm_context_json(result,source),"application/json");
 }catch(const std::exception& error){response.status=400;response.set_content(error_json(error.what()),"application/json");}
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
    static const std::set<std::string> allowed={"clustering_trace_attribute","clustering_activity_attribute","clustering_variant","clustering_alpha","clustering_percent","clustering_max_groups","clustering_max_output_nodes"};
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
        "profile_variant", "profile_cluster_count", "profile_max_iterations", "profile_max_traces",
        "profile_max_features", "profile_seed", "profile_tolerance",
        "profile_activity_attribute",
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

// Standalone full replay accepts exact uploaded log/model inputs. It shares the
// CLI's tagged result carrier and does not require a discovered-model session.
void handle_token_replay(const httplib::Request& request,
                         httplib::Response& response) {
  try {
    if (!request.is_multipart_form_data() || !request.has_file("log") ||
        !request.has_file("model"))
      throw std::invalid_argument("token replay requires multipart fields log and model");
    const auto lookup = [&](std::string_view name) {
      const std::string key(name);
      return request.has_file(key) ? form_field(request, key)
             : request.has_param(key) ? request.get_param_value(key) : std::string{};
    };
    const auto present = [&](std::string_view name) {
      const std::string key(name);
      return request.has_file(key) || request.has_param(key);
    };
    const auto params = minera::parse_mine_params(lookup, present);
    const auto& file = request.get_file_value("log");
    std::istringstream input(file.content);
    const auto log = looks_like_xes(file.filename, file.content)
        ? minera::import_xes_event_log(input, params.xes)
        : minera::import_csv_event_log(input, params.csv);
    auto import_options = minera::tools::pnml_model_import_options();
    import_options.import_stochastic_map = false;
    const auto document = minera::parse_pnml_document(
        request.get_file_value("model").content, import_options);
    const auto options = minera::tools::default_replay_input_options(
        log, minera::tools::pnml_replay_options(document, params.token_replay),
        params.token_replay_activity_attribute_supplied);
    response.set_content(minera::token_replay_result_json(
        minera::replay_token_result(log, document.net, options)), "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_replay_fitness(const httplib::Request& request,
                           httplib::Response& response) {
  try {
    if (!request.is_multipart_form_data() || !request.has_file("log") ||
        !request.has_file("model"))
      throw std::invalid_argument("replay fitness requires multipart fields log and model");
    const auto lookup = [&](std::string_view name) {
      const std::string key(name);
      return request.has_file(key) ? form_field(request, key)
             : request.has_param(key) ? request.get_param_value(key) : std::string{};
    };
    const auto present = [&](std::string_view name) {
      const std::string key(name);
      return request.has_file(key) || request.has_param(key);
    };
    const auto imports = minera::parse_mine_params(lookup, present);
    const auto params = minera::parse_evaluation_params(lookup, present);
    if (present("fitness_variant") &&
        params.replay_fitness_variant != minera::ReplayFitnessVariant::TOKEN_BASED)
      throw std::invalid_argument("standalone replay fitness supports token-based fitness");
    const auto& file = request.get_file_value("log");
    std::istringstream input(file.content);
    const auto log = looks_like_xes(file.filename, file.content)
        ? minera::import_xes_event_log(input, imports.xes)
        : minera::import_csv_event_log(input, imports.csv);
    auto model_options = minera::tools::pnml_model_import_options();
    model_options.import_stochastic_map = false;
    model_options.public_marking_semantics = true;
    const auto document = minera::parse_pnml_document(
        request.get_file_value("model").content, model_options);
    response.set_content(minera::token_replay_fitness_result_json(
        minera::evaluate_replay_fitness(log, document,
            minera::ReplayFitnessVariant::TOKEN_BASED, params.replay_fitness_options)),
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
    std::optional<minera::PnmlDocument> document;
    bool public_markings = false;
    if (request.has_param("pnml_public_markings") || request.has_file("pnml_public_markings")) {
      const auto value = request.has_param("pnml_public_markings")
          ? request.get_param_value("pnml_public_markings")
          : form_field(request, "pnml_public_markings");
      if (value == "true" || value == "1") public_markings = true;
      else if (value != "false" && value != "0")
        throw std::invalid_argument("pnml_public_markings expects true, false, 1, or 0");
    }
    const auto imported =
        minera::looks_like_bpmn(model_xml) ? minera::parse_bpmn(model_xml)
        : minera::looks_like_ptml(model_xml)
            ? minera::to_petri_net(minera::parse_ptml(model_xml))
            : (document = minera::tools::parse_pnml_model(model_xml, public_markings),
               document->net);

    auto next_session = std::make_shared<MinedSession>(*session);
    for (const auto& activity : imported.activities()) {
      next_session->graph.ensure_activity(activity);
    }
    next_session->nets.insert_or_assign("imported", imported);
    next_session->imported_document = document;
    const auto replay_options = document
        ? minera::tools::pnml_replay_options(*document, session->token_replay)
        : session->token_replay;
    const auto gated = minera::tools::pnml_conformance_gate(
        imported, document ? &*document : nullptr);
    const minera::ModelConformance conformance = gated ? *gated : minera::ModelConformance{
        "imported", document
            ? minera::final_marking_reachable(imported,
                  replay_options.initial_marking, replay_options.final_marking)
            : minera::final_marking_reachable(imported),
        minera::check_conformance(session->log, imported, replay_options),
        document ? minera::check_footprints(session->log, imported,
                                             replay_options.initial_marking)
                 : minera::check_footprints(session->log, imported),
        std::nullopt};
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
  try {
    std::optional<minera::PetriNet> converted;
    const auto& selected = select_cached_or_process_tree_net(
        request, *session, miner, converted);
    const auto lookup = [&](std::string_view name) {
      return request.has_param(std::string(name))
                 ? request.get_param_value(std::string(name))
                 : std::string{};
    };
    const auto present = [&](std::string_view name) {
      return request.has_param(std::string(name));
    };
    auto options = minera::parse_alignment_params(lookup, present);
    if (miner == "imported" && session->imported_document)
      options = minera::tools::pnml_alignment_options(
          *session->imported_document, options);
    std::optional<minera::EventLog> projected_powl_log;
    const minera::EventLog* alignment_log =
        &cached_model_log(*session, miner);
    if (miner == "powl" &&
        !request.has_param("alignment_activity_attribute")) {
      options.activity_attribute = session->powl_activity_key;
    }
    if (miner == "powl") {
      projected_powl_log = project_powl_activity(
          session->log, options.activity_attribute,
          session->powl_timestamp_key, session->powl_case_id_key);
      alignment_log = &*projected_powl_log;
    }
    const auto alignment =
        minera::check_alignment(*alignment_log, selected, options);
    auto graph = session->graph;
    register_activity_selector_values(graph, *alignment_log,
                                      options.activity_attribute);
    response.set_content(minera::alignment_json(alignment, graph),
                         "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_reduce_petri_net(const httplib::Request& request,
                             httplib::Response& response) {
  try {
    static const std::set<std::string> allowed = {
        "petri_reduction_initial_marking",
        "petri_reduction_final_marking",
        "petri_reduction_parallel_place_fusion",
        "petri_reduction_abstraction"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument(
            "unknown Petri reduction parameter: " + key);
    }
    const std::string model_xml = request.has_file("model")
                                      ? request.get_file_value("model").content
                                      : request.body;
    if (model_xml.empty())
      throw std::invalid_argument("missing form field: model");
    const auto lookup = [&](std::string_view name) {
      const auto key = std::string(name);
      return request.has_param(key) ? request.get_param_value(key)
                                    : std::string{};
    };
    const auto present = [&](std::string_view name) {
      return request.has_param(std::string(name));
    };
    auto options = minera::parse_petri_net_reduction_params(lookup, present);
    auto document = minera::parse_pnml_document(model_xml);
    if (!present("petri_reduction_initial_marking"))
      options.initial_marking = minera::tools::pnml_marking<std::int64_t>(document.initial_marking);
    if (!present("petri_reduction_final_marking"))
      options.final_marking = minera::tools::pnml_marking<std::int64_t>(document.final_marking);
    response.set_content(
        minera::petri_net_reduction_json(
            minera::reduce_petri_net(document.net, options)),
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
        "drift_variant", "drift_sub_log_size", "drift_window_size",
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

void handle_browser_history(const httplib::Request& request,
                            httplib::Response& response) {
  try {
    static const std::set<std::string> allowed = {
        "browser_history_variant", "browser_history_path",
        "browser_history_max_events"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument("unknown browser history parameter: " + key);
    }
    const auto lookup = [&](std::string_view name) {
      const auto key = std::string(name);
      return request.has_param(key) ? request.get_param_value(key) : std::string{};
    };
    const auto present = [&](std::string_view name) {
      return request.has_param(std::string(name));
    };
    const auto options = minera::parse_browser_history_params(lookup, present);
    response.set_content(
        minera::browser_history_json(
            minera::import_browser_history(options), options.variant),
        "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_windows_events(const httplib::Request& request,
                           httplib::Response& response) {
  try {
    static const std::set<std::string> allowed = {
        "windows_events_path", "windows_events_max_events"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument("unknown Windows events parameter: " + key);
    }
    const auto lookup = [&](std::string_view name) {
      const auto key = std::string(name);
      return request.has_param(key) ? request.get_param_value(key) : std::string{};
    };
    const auto present = [&](std::string_view name) {
      return request.has_param(std::string(name));
    };
    const auto options = minera::parse_windows_event_log_params(lookup, present);
    response.set_content(minera::windows_event_log_json(
                             minera::import_windows_event_log(options)),
                         "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_repository_history(const httplib::Request& request,
                               httplib::Response& response) {
  try {
    static const std::set<std::string> allowed = {
        "repository_owner", "repository_name", "repository_api_base_url",
        "repository_max_pages", "repository_max_issues", "repository_max_events"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument("unknown repository history parameter: " + key);
    }
    const auto lookup = [&](std::string_view name) {
      const auto key = std::string(name);
      return request.has_param(key) ? request.get_param_value(key) : std::string{};
    };
    const auto present = [&](std::string_view name) {
      return request.has_param(std::string(name));
    };
    auto options = minera::parse_repository_history_params(lookup, present);
    const auto authorization = request.get_header_value("Authorization");
    if (!authorization.empty()) {
      constexpr std::string_view prefix = "Bearer ";
      if (authorization.rfind(prefix, 0) != 0 || authorization.size() == prefix.size())
        throw std::invalid_argument("Authorization must contain a non-empty bearer token");
      options.auth_token = authorization.substr(prefix.size());
    }
    options.transport = minera::tools::repository_history_http_transport();
    response.set_content(minera::repository_history_json(
                             minera::import_repository_history(options),
                             options.owner, options.repository),
                         "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_workflow_history(const httplib::Request& request,
                             httplib::Response& response) {
  try {
    static const std::set<std::string> allowed = {"workflow_max_events"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument("unknown workflow history parameter: " + key);
    }
    const auto lookup = [&](std::string_view name) {
      const auto key = std::string(name);
      return request.has_param(key) ? request.get_param_value(key) : std::string{};
    };
    const auto present = [&](std::string_view name) { return request.has_param(std::string(name)); };
    const auto options = minera::parse_workflow_history_params(lookup, present);
    response.set_content(minera::workflow_history_json(
                             minera::import_workflow_history_csv(request.body, options)),
                         "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}
void handle_enterprise_history(const httplib::Request& request,
                               httplib::Response& response) {
  try {
    static const std::set<std::string> allowed = {"enterprise_variant", "enterprise_max_events"};
    for (const auto& [key, value] : request.params) {
      (void)value; if (!allowed.contains(key)) throw std::invalid_argument("unknown enterprise history parameter: " + key);
    }
    const auto lookup = [&](std::string_view name) {
      const auto key = std::string(name); return request.has_param(key) ? request.get_param_value(key) : std::string{};
    };
    const auto present = [&](std::string_view name) { return request.has_param(std::string(name)); };
    const auto options = minera::parse_enterprise_history_params(lookup, present);
    response.set_content(minera::enterprise_history_json(
        minera::import_enterprise_history_csv(request.body, options), options.variant), "application/json");
  } catch (const std::exception& error) {
    response.status = 400; response.set_content(error_json(error.what()), "application/json");
  }
}
void handle_communication_history(const httplib::Request& request, httplib::Response& response) {
  try {
    static const std::set<std::string> allowed = {"communication_variant", "communication_include_reminders", "communication_current_time", "communication_max_items", "communication_max_events"};
    for (const auto& [key, value] : request.params) { (void)value; if (!allowed.contains(key)) throw std::invalid_argument("unknown communication history parameter: " + key); }
    const auto lookup = [&](std::string_view name) { const auto key = std::string(name); return request.has_param(key) ? request.get_param_value(key) : std::string{}; };
    const auto present = [&](std::string_view name) { return request.has_param(std::string(name)); };
    const auto options = minera::parse_communication_history_params(lookup, present);
    response.set_content(minera::communication_history_json(minera::import_communication_history_csv(request.body, options), options.variant), "application/json");
  } catch (const std::exception& error) { response.status = 400; response.set_content(error_json(error.what()), "application/json"); }
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
        "features_variant", "features_activity_key", "features_case_id_key", "features_timestamp_key",
        "features_start_timestamp_key", "features_resource_key",
        "features_arrival_rate", "features_finish_rate", "features_service_time",
        "features_waiting_time", "features_sojourn_time", "features_diff_start_end", "features_grouper_freq",
        "features_window_seconds",
        "features_default_not_present", "features_event_string_attributes",
        "features_event_numeric_attributes", "features_case_string_attributes",
        "features_case_numeric_attributes", "features_event_attributes",
        "features_case_attributes", "features_event_successor_attributes",
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
        "feature_waiting_time", "feature_sojourn_time", "feature_diff_start_end", "feature_grouper_freq",
        "feature_window_seconds",
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
        "embedding_variant", "embedding_provider", "embedding_model", "embedding_attribute_key",
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
      if (request.has_param(key)) return request.get_param_value(key);
      if (g_embedding_runtime_enabled && key == "embedding_provider")
        return std::string("transformer");
      return std::string{};
    };
    const auto present = [&](std::string_view name) {
      return request.has_param(std::string(name)) ||
             (g_embedding_runtime_enabled && name == "embedding_provider");
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
        "merge_variant", "merge_timestamp_key", "merge_case_id_key",
        "merge_left_suffix", "merge_right_suffix", "merge_left_column",
        "merge_right_column"};
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
    const auto value = [&](std::string_view key,
                           std::string fallback) {
      return request.has_param(std::string(key))
                 ? request.get_param_value(std::string(key))
                 : std::move(fallback);
    };
    const auto lookup = [&](std::string_view key) {
      return request.has_param(std::string(key))
                 ? request.get_param_value(std::string(key))
                 : std::string{};
    };
    const auto present = [&](std::string_view key) {
      return request.has_param(std::string(key));
    };
    auto options =
        minera::parse_case_relations_params(lookup, present);
    const auto left_column = value(
        "merge_left_column", options.case_id_key + options.left_suffix);
    const auto right_column = value(
        "merge_right_column", options.case_id_key + options.right_suffix);
    const bool base_is_xes = looks_like_xes(base_file.filename, base_file.content);
    const bool incoming_is_xes =
        looks_like_xes(incoming_file.filename, incoming_file.content);
    if (!base_is_xes && !incoming_is_xes) {
      const auto relation_table =
          minera::parse_case_relations_table_csv(relation_input);
      options.relation_left_column = left_column;
      options.relation_right_column = right_column;
      auto base_data = minera::import_csv_event_data(base_input);
      auto incoming_data = minera::import_csv_event_data(incoming_input);
      normalize_csv_timestamp_missingness(base_data.table);
      normalize_csv_timestamp_missingness(incoming_data.table);
      if (!request.has_param("merge_timestamp_key")) {
        adapt_case_relations_table_timestamp(options, base_data.table,
                                              incoming_data.table);
      }
      response.set_content(
          minera::case_relations_json(minera::merge_case_relations(
              base_data.table, incoming_data.table, relation_table, options), options),
          "application/json");
    } else {
      std::optional<decltype(minera::import_csv_event_data(base_input))> base_csv;
      std::optional<decltype(minera::import_csv_event_data(incoming_input))> incoming_csv;
      if (!base_is_xes) base_csv = minera::import_csv_event_data(base_input);
      if (!incoming_is_xes) incoming_csv = minera::import_csv_event_data(incoming_input);
      const auto* csv_table = base_csv ? &base_csv->table
                                      : (incoming_csv ? &incoming_csv->table : nullptr);
      const bool explicit_case = request.has_param("merge_case_id_key");
      const bool explicit_timestamp = request.has_param("merge_timestamp_key");
      if (csv_table && !explicit_case)
        options.case_id_key = minera::tools::relation_role_column(
            *csv_table, minera::FirstClassAttributeRole::kCaseId, "case_id");
      if (csv_table && !explicit_timestamp)
        options.timestamp_key = minera::tools::relation_role_column(
            *csv_table, minera::FirstClassAttributeRole::kTimestamp, "timestamp");
      auto base_table = base_is_xes
          ? minera::tools::relation_xes_table(minera::import_xes_event_log(base_input), options,
                                            explicit_case, explicit_timestamp)
          : std::move(base_csv->table);
      auto incoming_table = incoming_is_xes
          ? minera::tools::relation_xes_table(minera::import_xes_event_log(incoming_input), options,
                                            explicit_case, explicit_timestamp)
          : std::move(incoming_csv->table);
      if (!base_is_xes) { normalize_csv_timestamp_missingness(base_table); minera::tools::relation_csv_dates(base_table, options.timestamp_key); }
      if (!incoming_is_xes) { normalize_csv_timestamp_missingness(incoming_table); minera::tools::relation_csv_dates(incoming_table, options.timestamp_key); }
      options.relation_left_column = left_column;
      options.relation_right_column = right_column;
      const auto relation_table = minera::parse_case_relations_table_csv(relation_input);
      response.set_content(minera::case_relations_json(minera::merge_case_relations(
          base_table, incoming_table, relation_table, options), options), "application/json");
    }
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_format_dataframe(const httplib::Request& request,
                             httplib::Response& response) {
  try {
    if (!request.params.empty())
      throw std::invalid_argument("formatter options belong in the JSON request body");
    const auto result = minera::format_dataframe_json(request.body);
    response.status = result.success ? 200 : 400;
    response.set_content(result.body, "application/json");
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
    const auto models = minera::generate_random_process_trees(params);
    response.set_content(
        minera::random_tree_json(models, params.variant, params.activities),
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
    const auto& source_log = cached_model_log(*session, miner);
    constexpr std::size_t kMaxComparisonTraces = 10'000;
    constexpr std::size_t kMaxComparisonEvents = 1'000'000;
    constexpr std::size_t kMaxComparisonPlaces = 50'000;
    constexpr std::size_t kMaxComparisonTransitions = 10'000;
    constexpr std::size_t kMaxComparisonWork = 500'000'000;
    constexpr std::size_t kSilentRoutingStateCap = 4'096;
    minera::PetriUsageComparisonOptions comparison_options;
    if (miner == "imported" && session->imported_document)
      comparison_options = minera::tools::pnml_comparison_options(
          *session->imported_document, comparison_options);
    const auto comparison_bool = [&](std::string_view name, bool fallback) {
      const auto key = std::string(name);
      std::string value;
      if (request.has_param(key)) {
        value = request.get_param_value(key);
      } else if (request.has_file(key)) {
        value = request.get_file_value(key).content;
      } else {
        return fallback;
      }
      value = lower_ascii(trim_text(value));
      if (value == "true" || value == "1") return true;
      if (value == "false" || value == "0") return false;
      throw std::invalid_argument(
          key + " expects true, false, 1, or 0");
    };
    if (request.has_param("activity_attribute")) {
      comparison_options.activity_attribute =
          trim_text(request.get_param_value("activity_attribute"));
    } else if (request.has_file("activity_attribute")) {
      comparison_options.activity_attribute =
          trim_text(request.get_file_value("activity_attribute").content);
    }
    if (comparison_options.activity_attribute.empty() &&
        (request.has_param("activity_attribute") ||
         request.has_file("activity_attribute"))) {
      throw std::invalid_argument(
          "activity_attribute: expected a non-empty name when supplied");
    }
    if (miner == "powl" && comparison_options.activity_attribute.empty())
      comparison_options.activity_attribute = session->powl_activity_key;
    if (miner == "alpha" && comparison_options.activity_attribute.empty())
      comparison_options.activity_attribute = session->alpha_activity_attribute;
    comparison_options.walk_through_hidden_trans = comparison_bool(
        "walk_through_hidden_trans",
        comparison_options.walk_through_hidden_trans);
    comparison_options.try_to_reach_final_marking_through_hidden =
        comparison_bool(
            "try_to_reach_final_marking_through_hidden",
            comparison_options.try_to_reach_final_marking_through_hidden);
    comparison_options.stop_immediately_unfit = comparison_bool(
        "stop_immediately_unfit",
        comparison_options.stop_immediately_unfit);
    comparison_options.exhaustive_invisible_exploration = comparison_bool(
        "exhaustive_invisible_exploration", comparison_options.exhaustive_invisible_exploration);
    comparison_options.cleaning_token_flood = comparison_bool(
        "cleaning_token_flood", comparison_options.cleaning_token_flood);
    const auto selector = comparison_options.activity_attribute;
    minera::EventLog reference;
    if (minera::looks_like_xes(reference_file.filename, reference_file.content)) {
      std::istringstream input(reference_file.content);
      minera::XesImportOptions reference_options;
      if (miner == "inductive") {
        reference_options.activity_attribute =
            session->inductive_options.activity_key;
        reference_options.timestamp_attribute =
            session->inductive_options.timestamp_key;
        reference_options.case_attribute =
            session->inductive_options.case_id_key;
      }
      if (!selector.empty() && selector != "concept:name")
        reference_options.activity_attribute = selector;
      reference = minera::import_xes_event_log(input, reference_options);
    } else {
      std::istringstream input(reference_file.content);
      minera::CsvImportOptions reference_options;
      reference_options.allow_case_id_aliases = true;
      if (miner == "powl") {
        reference_options.case_id_column = session->powl_case_id_key;
        reference_options.timestamp_column = session->powl_timestamp_key;
      } else if (miner == "inductive") {
        reference_options.case_id_column =
            session->inductive_options.case_id_key;
        reference_options.timestamp_column =
            session->inductive_options.timestamp_key;
        reference_options.activity_column =
            session->inductive_options.activity_key;
      } else if (miner == "alpha") {
        reference_options.activity_column =
            session->alpha_activity_attribute;
      }
      if (!selector.empty() && selector != "activity")
        reference_options.activity_column = selector;
      reference = minera::import_csv_event_log(input, reference_options);
    }
    if (miner == "inductive") {
      reference = minera::prepare_inductive_event_log(
          reference, session->inductive_options);
    } else if (miner == "alpha") {
      minera::AlphaMinerOptions alpha_projection_options;
      alpha_projection_options.activity_attribute =
          comparison_options.activity_attribute;
      reference = minera::normalize_alpha_event_log(
          reference, alpha_projection_options);
    }
    if (source_log.trace_count() > kMaxComparisonTraces ||
        source_log.event_count() > kMaxComparisonEvents ||
        reference.trace_count() > kMaxComparisonTraces ||
        reference.event_count() > kMaxComparisonEvents ||
        it->second.place_count() > kMaxComparisonPlaces ||
        it->second.transitions().size() > kMaxComparisonTransitions) {
      throw std::invalid_argument(
          "Petri usage comparison log exceeds the server safety limits");
    }
    const auto replay_units = source_log.event_count() +
                              reference.event_count() +
                              source_log.trace_count() + reference.trace_count();
    const auto worst_case_scan = replay_units * kSilentRoutingStateCap;
    if (worst_case_scan != 0 &&
        it->second.transitions().size() > kMaxComparisonWork / worst_case_scan) {
      throw std::invalid_argument(
          "Petri usage comparison exceeds the server work budget");
    }
    if (request.has_param("initial_marking")) {
      comparison_options.initial_marking = parse_playout_marking(request, "initial_marking");
      comparison_options.initial_marking_keys.clear();
    }
    if (request.has_param("final_marking")) {
      comparison_options.final_marking = parse_playout_marking(request, "final_marking");
      comparison_options.final_marking_keys.clear();
    }
    std::optional<minera::EventLog> projected_powl_log;
    const minera::EventLog* comparison_log =
        &cached_model_log(*session, miner);
    if (miner == "powl") {
      projected_powl_log = project_powl_activity(
          session->log, comparison_options.activity_attribute,
          session->powl_timestamp_key, session->powl_case_id_key);
      comparison_log = &*projected_powl_log;
      reference = project_powl_activity(
          reference, comparison_options.activity_attribute,
          session->powl_timestamp_key, session->powl_case_id_key);
    }
    const auto comparison = minera::compare_petri_usage(
        it->second, *comparison_log, reference, comparison_options);
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
  try {
    std::optional<minera::PetriNet> converted;
    const auto& selected = select_cached_or_process_tree_net(
        request, *session, miner, converted);
    const auto lookup = [&](std::string_view name) {
      const auto key = std::string(name);
      return request.has_param(key) ? request.get_param_value(key)
                                    : std::string{};
    };
    const auto present = [&](std::string_view name) {
      return request.has_param(std::string(name));
    };
    auto params = minera::parse_evaluation_params(lookup, present);
    const auto* evaluation_document = miner == "imported" && session->imported_document && !converted
        ? &*session->imported_document : nullptr;
    std::optional<minera::EventLog> projected_powl_log;
    const minera::EventLog* evaluation_log =
        &cached_model_log(*session, miner);
    if (miner == "powl") {
      projected_powl_log = project_powl_activity(
          session->log, session->powl_activity_key,
          session->powl_timestamp_key, session->powl_case_id_key);
      evaluation_log = &*projected_powl_log;
    }
    const std::string_view model_activity = miner == "inductive"
        ? std::string_view(session->inductive_options.activity_key)
        : miner == "alpha" ? std::string_view(session->alpha_activity_attribute)
        : miner == "heuristic" ? std::string_view(session->heuristic_activity_attribute)
        : miner == "powl" ? std::string_view(session->powl_activity_key)
        : std::string_view("concept:name");
    params = minera::tools::default_evaluation_input_params(
        *evaluation_log, std::move(params),
        request.has_param("evaluation_activity_attribute"), model_activity);
    const auto evaluation_input = minera::tools::evaluation_input_log(
        *evaluation_log, params.replay_fitness_options.activity_attribute);
    if (evaluation_input) evaluation_log = &*evaluation_input;
    minera::ModelEvaluation result;
    result.miner = miner;
    switch (params.family) {
      case minera::EvaluationFamily::kReplayFitness:
        result.replay_fitness = evaluation_document
            ? minera::evaluate_replay_fitness(
                  *evaluation_log, *evaluation_document, params.replay_fitness_variant,
                  params.replay_fitness_options)
            : minera::evaluate_replay_fitness(
            *evaluation_log, selected, params.replay_fitness_variant,
            params.replay_fitness_options);
        break;
      case minera::EvaluationFamily::kPrecision:
        if (evaluation_document && params.precision_variant !=
            minera::PrecisionVariant::ETCONFORMANCE_TOKEN)
          cached_imported_markings(request, *session, miner, params.precision_options,
                                   "evaluation_initial_marking", "evaluation_final_marking");
        result.precision = evaluation_document && params.precision_variant ==
            minera::PrecisionVariant::ETCONFORMANCE_TOKEN
            ? minera::evaluate_precision(
                  *evaluation_log, *evaluation_document, params.precision_variant,
                  params.precision_options)
            : minera::evaluate_precision(
            *evaluation_log, selected, params.precision_variant,
            params.precision_options);
        break;
      case minera::EvaluationFamily::kGeneralization:
        result.generalization = evaluation_document
            ? minera::evaluate_generalization(
                  *evaluation_log, *evaluation_document, params.generalization_variant,
                  params.generalization_options)
            : minera::evaluate_generalization(
            *evaluation_log, selected, params.generalization_variant,
            params.generalization_options);
        break;
      case minera::EvaluationFamily::kSimplicity:
        result.simplicity = miner == "imported" && session->imported_document && !converted
            ? minera::evaluate_simplicity(
                  *session->imported_document, params.simplicity_variant,
                  params.simplicity_options)
            : minera::evaluate_simplicity(
                  selected, params.simplicity_variant, params.simplicity_options);
        break;
      case minera::EvaluationFamily::kComposite:
        result.composite = miner == "imported" && session->imported_document && !converted
            ? minera::evaluate_composite(
                  *evaluation_log, *session->imported_document, params.composite_options)
            : minera::evaluate_composite(
                  *evaluation_log, selected, params.composite_options);
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
    auto params = minera::parse_petri_analysis_params(lookup, present);
    cached_imported_markings(request, *session, params.miner, params,
                             "petri_analysis_initial_marking", "petri_analysis_final_marking");
    std::optional<minera::PetriNet> converted;
    const auto& net = select_cached_or_process_tree_net(
        request, *session, params.miner, converted);
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
        std::optional<minera::EventLog> projected_powl_log;
        const minera::EventLog* analysis_log =
            &cached_model_log(*session, params.miner);
        if (params.miner == "powl") {
          projected_powl_log = project_powl_activity(
              session->log, session->powl_activity_key,
              session->powl_timestamp_key, session->powl_case_id_key);
          analysis_log = &*projected_powl_log;
        }
        if (params.trace_index >= analysis_log->traces.size())
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
        for (const auto& event : analysis_log->traces[params.trace_index].events)
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
        options.invariant_search_limit = params.invariant_search_limit;
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
    // Select the public default explicitly at the HTTP boundary. The exact
    // native solver remains available through an explicit query selector.
    minera::EmdOptions options;
    options.variant = minera::EmdVariant::PYEMD;
    if (request.has_param("emd_variant")) {
      const auto value = lower_ascii(request.get_param_value("emd_variant"));
      if (value == "exact") {
        options.variant = minera::EmdVariant::EXACT;
      } else if (value == "pyemd") {
        options.variant = minera::EmdVariant::PYEMD;
      } else {
        throw std::invalid_argument(
            "emd_variant expects exact or pyemd");
      }
    }
    options.normalize_masses =
        parse_boolean("normalize_masses", options.normalize_masses);
    options.strict_equal_mass =
        parse_boolean("strict_equal_mass", options.strict_equal_mass);
    options.use_fast_emd =
        parse_boolean("use_fast_emd", options.use_fast_emd);

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
    static const std::set<std::string> allowed = {
        "pt_alignment_variant", "pt_alignment_max_states",
        "pt_alignment_max_time_ms", "pt_alignment_max_total_time_ms",
        "pt_alignment_use_global_cache", "pt_alignment_batch_size",
        "pt_alignment_clear_cache", "pt_alignment_max_trace_length",
        "pt_alignment_max_process_tree_height",
        "pt_alignment_max_trace_time_ms",
        "pt_alignment_max_per_trace_time_ms",
        "pt_alignment_activity_attribute"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument(
            "unknown process-tree alignment parameter: " + key);
    }
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
    const auto result = minera::align_process_tree(session->inductive_log,
                                                   session->tree,
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
  try {
    static const std::set<std::string> allowed = {
        "miner", "variant", "threshold_border_agreement",
        "max_time_trace", "max_time_total", "max_states",
        "max_total_states", "use_cache", "clear_cache", "log_move_cost",
        "model_move_cost", "tau_move_cost", "tau_model_cost",
        "sync_move_cost", "sync_cost", "activity_log_costs",
        "activity_model_costs", "activity_sync_costs", "initial_marking",
        "final_marking", "activity_attribute"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument(
            "unknown decomposed alignment parameter: " + key);
    }
    const auto lookup=[&](std::string_view n){const auto k=std::string(n); return request.has_param(k)?request.get_param_value(k):std::string{};}; const auto present=[&](std::string_view n){return request.has_param(std::string(n));}; auto o=minera::parse_decomposed_alignment_params(lookup,present); cached_imported_markings(request, *session, miner, o); std::optional<minera::EventLog> projected_powl_log; const minera::EventLog* alignment_log=&cached_model_log(*session,miner); if(miner=="powl"){if(!request.has_param("activity_attribute")) o.activity_attribute=session->powl_activity_key; projected_powl_log=project_powl_activity(session->log,o.activity_attribute,session->powl_timestamp_key,session->powl_case_id_key); alignment_log=&*projected_powl_log;} const auto r=minera::align_decomposed(*alignment_log,it->second,o); response.set_content(minera::decomposed_alignment_json(r),"application/json"); }
  catch(const std::exception& e){response.status=400; response.set_content(error_json(e.what()),"application/json");}
}

void handle_align_petri_anti(const httplib::Request& request, httplib::Response& response) {
  std::shared_ptr<const MinedSession> session; { std::lock_guard<std::mutex> lock(g_session_mutex); session = g_session; }
  if (!session) { response.status=409; response.set_content(error_json("no mined log; mine a log first"),"application/json"); return; }
  const auto miner=request.get_param_value("miner"); const auto it=session->nets.find(miner); if(it==session->nets.end()){response.status=400;response.set_content(error_json("unknown miner: "+miner),"application/json");return;}
  try { static const std::set<std::string> allowed = {"miner", "variant", "exponent", "epsilon", "marking_limit", "max_states", "initial_marking", "final_marking", "activity_attribute"}; for (const auto& [key, value] : request.params) { (void)value; if (!allowed.contains(key)) throw std::invalid_argument("unknown Petri anti-alignment parameter: " + key); } const auto lookup=[&](std::string_view n){const auto k=std::string(n);return request.has_param(k)?request.get_param_value(k):std::string{};}; const auto present=[&](std::string_view n){return request.has_param(std::string(n));}; auto o=minera::parse_petri_anti_alignment_params(lookup,present); cached_imported_markings(request, *session, miner, o); std::optional<minera::EventLog> projected_powl_log; const minera::EventLog* alignment_log=&cached_model_log(*session,miner); if(miner=="powl"){if(!request.has_param("activity_attribute")) o.activity_attribute=session->powl_activity_key; projected_powl_log=project_powl_activity(session->log,o.activity_attribute,session->powl_timestamp_key,session->powl_case_id_key); alignment_log=&*projected_powl_log;} const auto r=minera::anti_align_petri(it->second,*alignment_log,o); response.set_content(minera::petri_anti_alignment_json(r),"application/json"); } catch(const std::exception&e){response.status=400;response.set_content(error_json(e.what()),"application/json");}
}

void handle_align_petri_multi(const httplib::Request& request, httplib::Response& response) {
  std::shared_ptr<const MinedSession> session; { std::lock_guard<std::mutex> lock(g_session_mutex); session = g_session; }
  if (!session) { response.status=409; response.set_content(error_json("no mined log; mine a log first"),"application/json"); return; }
  const auto miner=request.get_param_value("miner"); const auto it=session->nets.find(miner); if(it==session->nets.end()){response.status=400;response.set_content(error_json("unknown miner: "+miner),"application/json");return;}
  try { static const std::set<std::string> allowed = {"miner", "variant", "exponent", "marking_limit", "max_states", "initial_marking", "final_marking", "activity_attribute"}; for (const auto& [key, value] : request.params) { (void)value; if (!allowed.contains(key)) throw std::invalid_argument("unknown Petri multi-alignment parameter: " + key); } const auto lookup=[&](std::string_view n){const auto k=std::string(n);return request.has_param(k)?request.get_param_value(k):std::string{};}; const auto present=[&](std::string_view n){return request.has_param(std::string(n));}; auto o=minera::parse_petri_multi_alignment_params(lookup,present); cached_imported_markings(request, *session, miner, o); std::optional<minera::EventLog> projected_powl_log; const minera::EventLog* alignment_log=&cached_model_log(*session,miner); if(miner=="powl"){if(!request.has_param("activity_attribute")) o.activity_attribute=session->powl_activity_key; projected_powl_log=project_powl_activity(session->log,o.activity_attribute,session->powl_timestamp_key,session->powl_case_id_key); alignment_log=&*projected_powl_log;} const auto r=minera::multi_align_petri(it->second,*alignment_log,o); response.set_content(minera::petri_multi_alignment_json(r),"application/json"); } catch(const std::exception&e){response.status=400;response.set_content(error_json(e.what()),"application/json");}
}

void handle_reduce_process_tree(const httplib::Request& request, httplib::Response& response) {
  std::shared_ptr<const MinedSession> session; { std::lock_guard<std::mutex> lock(g_session_mutex); session = g_session; }
  if (!session) { response.status=409; response.set_content(error_json("no mined log; mine a log first"),"application/json"); return; }
  try { if(!request.has_param("trace_index")) throw std::invalid_argument("trace_index: expected a non-negative integer"); const auto text=request.get_param_value("trace_index"); std::size_t idx=0; const auto r=std::from_chars(text.data(),text.data()+text.size(),idx); if(r.ec!=std::errc{}||r.ptr!=text.data()+text.size()||idx>=session->inductive_log.traces.size()) throw std::invalid_argument("trace_index: out of range"); const auto lookup=[&](std::string_view n){const auto k=std::string(n);return request.has_param(k)?request.get_param_value(k):std::string{};}; const auto present=[&](std::string_view n){return request.has_param(std::string(n));}; const auto o=minera::parse_process_tree_reduction_params(lookup,present); const auto reduced=minera::reduce_process_tree(session->tree,session->inductive_log.traces[idx],o); response.set_content(minera::process_tree_reduction_json(reduced,session->graph,idx),"application/json"); } catch(const std::exception&e){response.status=400;response.set_content(error_json(e.what()),"application/json");}
}

void handle_alpha_plus_result(const httplib::Request& request, httplib::Response& response) {
  try {
    static const std::set<std::string> allowed{
        "alpha_activity_attribute", "alpha_remove_unconnected",
        "alpha_max_rows_to_materialize"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key)) throw std::invalid_argument("unknown Alpha+ result parameter: " + key);
    }
    for (const auto& [key, value] : request.files) {
      (void)value;
      if (key != "log" && !allowed.contains(key))
        throw std::invalid_argument("unknown Alpha+ result field: " + key);
    }
    const auto lookup = [&](std::string_view name) {
      const auto key = std::string(name);
      if (request.has_param(key)) return request.get_param_value(key);
      if (request.has_file(key)) return request.get_file_value(key).content;
      return std::string{};
    };
    const auto present = [&](std::string_view name) {
      return request.has_param(std::string(name)) || request.has_file(std::string(name));
    };
    const auto options = minera::parse_mine_params(lookup, present).alpha;
    minera::EventLog log;
    if (request.has_file("log") || (!request.is_multipart_form_data() && !request.body.empty())) {
      const auto body = request.has_file("log") ? request.get_file_value("log").content : request.body;
      const auto filename = request.has_file("log") ? request.get_file_value("log").filename : std::string{};
      std::stringstream input(body);
      log = minera::looks_like_xes(filename, body) ? minera::import_xes_event_log(input)
                                                 : minera::import_csv_event_log(input);
    } else {
      std::shared_ptr<const MinedSession> session;
      { std::lock_guard<std::mutex> lock(g_session_mutex); session = g_session; }
      if (!session) {
        response.status = 409;
        response.set_content(error_json("no mined log; upload a log or mine one first"), "application/json");
        return;
      }
      log = session->log;
    }
    response.set_content(minera::alpha_plus_result_json(
        minera::discover_alpha_plus_result(log, options)), "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_align_dfg(const httplib::Request& request, httplib::Response& response) {
  std::shared_ptr<const MinedSession> session; { std::lock_guard<std::mutex> lock(g_session_mutex); session = g_session; }
  if (!session) { response.status = 409; response.set_content(error_json("no mined log; mine a log first"), "application/json"); return; }
  try {
    static const std::set<std::string> allowed = {
        "dfg_variant", "variant", "dfg_max_states", "dfg_max_time_ms",
        "max_visited_states", "max_time_ms", "dfg_activity_attribute",
        "dfg_timestamp_attribute", "dfg_sync_cost", "dfg_sync_costs",
        "dfg_model_move_cost", "dfg_model_move_costs", "dfg_model_cost",
        "dfg_log_move_cost", "dfg_log_move_costs", "dfg_log_cost",
        "dfg_internal_log_move_cost", "dfg_internal_log_move_costs",
        "dfg_internal_cost", "sync_cost", "sync_costs", "model_move_cost",
        "model_costs", "log_move_cost", "log_costs", "internal_log_move_cost",
        "internal_costs", "projection"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument("unknown DFG alignment parameter: " + key);
    }
    const auto lookup = [&](std::string_view n) { const auto key=std::string(n); if (request.has_param(key)) return request.get_param_value(key); if (request.has_file(key)) return request.get_file_value(key).content; return std::string{}; };
    const auto present = [&](std::string_view n) { const auto key=std::string(n); return request.has_param(key) || request.has_file(key); };
    auto options = minera::parse_dfg_alignment_params(lookup, present);
    const auto projection = present("projection") ? lookup("projection") : std::string("none");
    if (projection != "none" && projection != "log" && projection != "metrics") throw std::invalid_argument("projection expects none, log, or metrics");
    minera::DfgStatisticalOptions dopt; dopt.window = 1; dopt.activity_attribute = options.activity_attribute; dopt.timestamp_attribute = options.timestamp_attribute; const auto dfg = minera::discover_frequency_dfg(session->log, dopt);
    std::map<std::string,std::uint64_t> starts, ends; for (const auto& t:session->log.traces) if(!t.events.empty()){ const auto* first = minera::dfg_event_field_value(t.events.front(), options.activity_attribute); const auto* last = minera::dfg_event_field_value(t.events.back(), options.activity_attribute); if (!first || !last) throw std::invalid_argument("DFG alignment: event is missing activity attribute '" + options.activity_attribute + "'"); ++starts[*first];++ends[*last]; }
    const auto result = minera::align_log_on_dfg(session->log, dfg, starts, ends, options);
    if (projection == "log") { const auto projected = minera::project_log_on_dfg(session->log, dfg, starts, ends, result, options); response.set_content(minera::dfg_alignment_json(result, nullptr, &projected), "application/json"); }
    else if (projection == "metrics") { const auto metrics = minera::project_alignments_on_dfg(session->log, dfg, starts, ends, result, options); response.set_content(minera::dfg_alignment_json(result, &metrics), "application/json"); }
    else response.set_content(minera::dfg_alignment_json(result), "application/json");
  } catch (const std::exception& error) { response.status = 400; response.set_content(error_json(error.what()), "application/json"); }
}

void handle_align_edit_distance(const httplib::Request& request, httplib::Response& response) {
  std::shared_ptr<const MinedSession> session; { std::lock_guard<std::mutex> lock(g_session_mutex); session = g_session; }
  if (!session) { response.status = 409; response.set_content(error_json("no mined log; mine a log first"), "application/json"); return; }
  try {
    static const std::set<std::string> allowed = {
        "edit_variant", "variant", "anti", "perform_anti_alignment",
        "deviation_cost", "edit_deviation_cost", "edit_activity_attribute",
        "edit_timestamp_attribute", "projection"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument(
            "unknown edit-distance parameter: " + key);
    }
    if (!request.is_multipart_form_data() || !request.has_file("reference_log")) throw std::invalid_argument("missing form field: reference_log");
    const auto& file = request.get_file_value("reference_log");
    std::stringstream input(file.content);
    const auto reference = minera::looks_like_xes(file.filename, file.content) ? minera::import_xes_event_log(input) : minera::import_csv_event_log(input);
    const auto lookup = [&](std::string_view n) { const auto key=std::string(n); if (request.has_param(key)) return request.get_param_value(key); if (request.has_file(key)) return request.get_file_value(key).content; return std::string{}; };
    const auto present = [&](std::string_view n) { const auto key=std::string(n); return request.has_param(key) || request.has_file(key); };
    const auto options = minera::parse_edit_distance_params(lookup, present);
    const auto projection = present("projection") ? lookup("projection") : std::string("none");
    if (projection != "none" && projection != "log") throw std::invalid_argument("projection expects none or log");
    const auto result =
        minera::align_typed_edit_distance(session->log, reference, options);
    if (projection == "log") { const auto projected = minera::project_log_on_typed_alignments(session->log, result, options); response.set_content(minera::typed_edit_distance_json(result, options.perform_anti_alignment, &projected), "application/json"); }
    else response.set_content(minera::typed_edit_distance_json(result, options.perform_anti_alignment), "application/json");
  } catch (const std::exception& error) { response.status = 400; response.set_content(error_json(error.what()), "application/json"); }
}

minera::DecisionMiningOptions decision_mining_options(
    const httplib::Request& request, bool allow_table_lookback = false) {
  static const std::set<std::string> allowed = {
      "miner", "decision_max_depth", "decision_min_split",
      "decision_min_leaf", "decision_max_categories",
      "decision_max_thresholds", "decision_min_gain",
      "decision_include_trace_attributes", "decision_attributes",
      "decision_activity_attribute",
      "process_tree_variant"};
  for (const auto& [key, value] : request.params) {
    (void)value;
    if (!allowed.contains(key) &&
        !(allow_table_lookback && key == "decision_lookback"))
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
  if (request.has_param("decision_activity_attribute")) {
    options.activity_attribute =
        trim_text(request.get_param_value("decision_activity_attribute"));
    if (options.activity_attribute.empty())
      throw std::invalid_argument(
          "decision_activity_attribute: expected a non-empty name");
  }
  return options;
}

std::size_t decision_table_lookback(const httplib::Request& request) {
  const auto lookback = ocel_query_size(request, "decision_lookback", 1);
  if (lookback == 0 || lookback > minera::kMaxDecisionLookback)
    throw std::invalid_argument(
        "decision_lookback must be an integer from 1 through 64");
  return lookback;
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
  try {
    std::optional<minera::PetriNet> converted;
    const auto& selected = select_cached_or_process_tree_net(
        request, *session, miner, converted);
    auto options = decision_mining_options(request);
    const bool use_default_powl_decision_selector =
        miner == "powl" && !request.has_param("decision_activity_attribute");
    if (use_default_powl_decision_selector)
      options.activity_attribute = session->powl_activity_key;
    std::optional<minera::EventLog> projected_powl_log;
    const minera::EventLog* decision_log =
        &cached_model_log(*session, miner);
    if (miner == "powl") {
      projected_powl_log = project_powl_activity(
          session->log, options.activity_attribute,
          session->powl_timestamp_key, session->powl_case_id_key);
      decision_log = &*projected_powl_log;
    }
    auto decisions = minera::mine_decisions(*decision_log, selected, options);
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
  try {
    std::optional<minera::PetriNet> converted;
    const auto& selected = select_cached_or_process_tree_net(
        request, *session, miner, converted);
    response.set_content(miner == "imported" && session->imported_document
                             ? minera::to_pnml(*session->imported_document)
                             : minera::to_pnml(selected, miner + " net"),
                         "application/xml");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

// Returns the selected process-tree conversion as structured JSON. Omitting
// the selector preserves the standard construction.
void handle_process_tree_petri(const httplib::Request& request,
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
      if (key != "variant" && key != "process_tree_variant")
        throw std::invalid_argument(
            "unknown process-tree conversion parameter: " + key);
    }
    if (request.has_param("variant") &&
        request.has_param("process_tree_variant")) {
      throw std::invalid_argument(
          "variant and process_tree_variant cannot both be supplied");
    }
    const std::string value = request.has_param("process_tree_variant")
                                  ? request.get_param_value("process_tree_variant")
                                  : request.has_param("variant")
                                        ? request.get_param_value("variant")
                                        : std::string{};
    const auto variant =
        minera::parse_process_tree_petri_net_variant(value);
    const auto net = minera::to_petri_net(session->tree, variant);
    response.set_content(
        minera::serialize_process_tree_petri_net_json(net, variant),
        "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

// Exports one model from the most recent mine as BPMN 2.0. The `miner` query
// parameter picks it (default "inductive"). The inductive model converts from
// its block-structured process tree, which maps cleanly onto BPMN; any other
// model ("alpha" | "heuristic" | "powl" | "imported") converts from its workflow net
// by the net's token semantics.
// Exports a process tree as PTML. The default is the inductive tree; a
// selected workflow net is reduced strictly when it is block structured.
void handle_ptml(const httplib::Request& request,
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
  const auto miner = request.has_param("miner")
                         ? request.get_param_value("miner")
                         : std::string("inductive");
  try {
    if (miner == "inductive") {
      response.set_content(minera::to_ptml(session->tree, session->dataset),
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
    response.set_content(
        minera::to_ptml(minera::to_process_tree(it->second),
                        session->dataset + " " + miner),
        "application/xml");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
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
  try {
    if (miner == "inductive" &&
        !request.has_param("process_tree_variant")) {
      response.set_content(minera::to_bpmn(session->tree, "process"),
                           "application/xml");
      return;
    }
    std::optional<minera::PetriNet> converted;
    const auto& selected = select_cached_or_process_tree_net(
        request, *session, miner, converted);
    response.set_content(minera::to_bpmn(selected, "process"),
                         "application/xml");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
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
  std::optional<minera::PetriNet> converted;
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
    const auto& selected = select_cached_or_process_tree_net(
        request, *session, selected_miner, converted);
    const auto map_record_limit = std::min(
        selected.transitions().size(),
        minera::kMonteCarloRequestMapMaxRecords);
    auto options = minera::parse_monte_carlo_params(
        lookup, present, map_record_limit);
    cached_imported_monte_carlo_context(request, *session, selected_miner, options);
    validate_monte_carlo_server_limits(options, selected);
    std::optional<minera::EventLog> projected_powl_log;
    const minera::EventLog* simulation_log =
        &cached_model_log(*session, selected_miner);
    if (selected_miner == "powl") {
      if (!request.has_param("monte_carlo_activity_key"))
        options.activity_key = session->powl_activity_key;
      if (!request.has_param("monte_carlo_timestamp_key"))
        options.timestamp_key = session->powl_timestamp_key;
      projected_powl_log = project_powl_activity(
          session->log, options.activity_key, options.timestamp_key,
          session->powl_case_id_key);
      simulation_log = &*projected_powl_log;
    }
    const auto result = minera::monte_carlo(*simulation_log, selected, options);
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
  try {
    std::optional<minera::PetriNet> converted;
    const auto& selected = select_cached_or_process_tree_net(
        request, *session, miner, converted);
    auto mining = decision_mining_options(request, true);
    const bool use_default_powl_decision_selector =
        miner == "powl" && !request.has_param("decision_activity_attribute");
    if (use_default_powl_decision_selector)
      mining.activity_attribute = session->powl_activity_key;
    minera::DecisionTableOptions options;
    options.mining = mining;
    options.include_trace_attributes = mining.include_trace_attributes;
    options.attributes = mining.attributes;
    options.lookback = decision_table_lookback(request);
    std::optional<minera::EventLog> projected_powl_log;
    const minera::EventLog* decision_log =
        &cached_model_log(*session, miner);
    if (miner == "powl") {
      projected_powl_log = project_powl_activity(
          session->log, mining.activity_attribute,
          session->powl_timestamp_key, session->powl_case_id_key);
      decision_log = &*projected_powl_log;
    }
    const auto result =
        minera::mine_decision_tables(*decision_log, selected, options);
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
  try {
    std::optional<minera::PetriNet> converted;
    const auto& selected = select_cached_or_process_tree_net(
        request, *session, miner, converted);
    minera::DataPetriNetOptions options;
    options.decision_options = decision_mining_options(request, true);
    const bool use_default_powl_decision_selector =
        miner == "powl" && !request.has_param("decision_activity_attribute");
    if (use_default_powl_decision_selector)
      options.decision_options.activity_attribute = session->powl_activity_key;
    options.decision_table_lookback = decision_table_lookback(request);
    std::optional<minera::EventLog> projected_powl_log;
    const minera::EventLog* decision_log =
        &cached_model_log(*session, miner);
    if (miner == "powl") {
      projected_powl_log = project_powl_activity(
          session->log, options.decision_options.activity_attribute,
          session->powl_timestamp_key, session->powl_case_id_key);
      decision_log = &*projected_powl_log;
    }
    const auto result =
        minera::mine_data_petri_net(*decision_log, selected, options);
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
// stochastic_transition_weights, stochastic_fit_from_log, and timing controls;
// mode=monte_carlo selects
// the bounded virtual-time simulator and accepts monte_carlo_* controls.
void handle_stochastic_petri(const httplib::Request& request,
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
    reject_unknown_stochastic_petri_params(request);
    const auto lookup = [&](std::string_view name) {
      const auto key = std::string(name);
      return request.has_param(key) ? request.get_param_value(key)
                                    : std::string{};
    };
    const auto present = [&](std::string_view name) {
      return request.has_param(std::string(name));
    };
    auto options = minera::parse_stochastic_petri_params(lookup, present);
    if (miner == "imported" && session->imported_document &&
        !present("stochastic_petri_initial_marking"))
      options.initial_marking = minera::tools::pnml_marking<std::uint32_t>(
          session->imported_document->initial_marking);
    if (options.max_states > minera::kStochasticPetriMaxGeneratorStates)
      throw std::invalid_argument(
          "stochastic Petri max_states exceeds the dense generator safety limit");
    const auto reachability =
        minera::stochastic_petri_reachability(it->second, options);
    if (options.analysis == minera::StochasticPetriAnalysisVariant::kReachability) {
      response.set_content(minera::serialize_stochastic_petri_json(reachability),
                           "application/json");
    } else if (options.analysis ==
               minera::StochasticPetriAnalysisVariant::kTransient) {
      const auto initial_state =
          options.analysis_initial_state.has_value()
              ? options.analysis_initial_state
              : reachability.initial_state;
      const auto& derived_probabilities = reachability.initial_probabilities;
      const bool has_derived_probabilities =
          !derived_probabilities.empty() &&
          derived_probabilities.size() == reachability.states.size();
      if (options.analysis_initial_probabilities.empty() &&
          !initial_state.has_value() && !has_derived_probabilities)
        throw std::invalid_argument(
            "stochastic Petri transient analysis could not retain the initial state; provide an initial probability vector");
      const auto distribution =
          !options.analysis_initial_probabilities.empty()
              ? minera::stochastic_petri_transient(
                    reachability, options.analysis_time_seconds,
                    options.analysis_initial_probabilities)
              : initial_state.has_value()
                    ? minera::stochastic_petri_transient(
                          reachability, options.analysis_time_seconds,
                          *initial_state)
                    : minera::stochastic_petri_transient(
                          reachability, options.analysis_time_seconds,
                          derived_probabilities);
      response.set_content(
          minera::serialize_stochastic_petri_distribution_json(distribution),
          "application/json");
    } else {
      response.set_content(minera::serialize_stochastic_petri_distribution_json(
                               minera::stochastic_petri_steady_state(reachability)),
                           "application/json");
    }
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_stochastic_map(const httplib::Request& request,
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
    reject_unknown_stochastic_map_params(request);
    const auto lookup = [&](std::string_view name) {
      const auto key = std::string(name);
      return request.has_param(key) ? request.get_param_value(key)
                                    : std::string{};
    };
    const auto present = [&](std::string_view name) {
      return request.has_param(std::string(name));
    };
    auto params = minera::parse_stochastic_map_params(lookup, present);
    const auto miner = request.has_param("miner")
                           ? request.get_param_value("miner")
                           : std::string("inductive");
    std::optional<minera::PetriNet> owned_net;
    minera::StochasticMap map;
    std::size_t replayed = 0;
    std::size_t skipped = 0;
    std::string net_json;
    if (params.source == minera::StochasticMapSource::kLog) {
      const auto it = session->nets.find(miner);
      if (it == session->nets.end())
        throw std::invalid_argument("unknown miner: " + miner);
      std::optional<minera::EventLog> projected_powl_log;
      const minera::EventLog* fit_log =
          &cached_model_log(*session, miner);
      if (miner == "powl") {
        projected_powl_log = project_powl_activity(
            session->log, session->powl_activity_key,
            session->powl_timestamp_key, session->powl_case_id_key);
        fit_log = &*projected_powl_log;
      }
      params.fit.replay = minera::tools::stochastic_map_replay_options(
          params.fit.replay, params.initial_marking,
          miner == "imported" && session->imported_document
              ? &*session->imported_document : nullptr);
      const auto report = minera::fit_stochastic_map_with_report(
          *fit_log, it->second, params.fit);
      map = report.map;
      replayed = report.replayed_traces;
      skipped = report.skipped_traces;
      owned_net = it->second;
    } else {
      const auto performance = minera::discover_performance_dfg(session->log);
      const auto model = minera::stochastic_map_from_performance_dfg(
          session->graph, performance, params.performance);
      map = model.map;
      owned_net = model.net;
      net_json = minera::serialize_dfg_petri_net_json(
          model.net, params.performance.conversion.variant);
    }
    if (params.source == minera::StochasticMapSource::kLog &&
        miner == "imported" && session->imported_document &&
        !present("stochastic_map_initial_marking"))
      params.initial_marking = minera::tools::pnml_marking<std::uint32_t>(
          session->imported_document->initial_marking);
    const auto reachability = minera::tools::stochastic_map_reachability(
        *owned_net, map, params.fit, params.initial_marking, params.max_states);
    std::ostringstream out;
    out << "{\"family\":\"STOCHASTIC_MAP\",\"source\":\""
        << (params.source == minera::StochasticMapSource::kLog ? "log"
                                                               : "performance_dfg")
        << "\",\"map\":"
        << minera::serialize_stochastic_map_json(map);
    if (!net_json.empty()) out << ",\"net\":" << net_json;
    if (params.source == minera::StochasticMapSource::kLog)
      out << ",\"diagnostics\":{\"replayed_traces\":"
          << replayed << ",\"skipped_traces\":" << skipped << '}';
    out << ",\"reachability\":"
        << minera::serialize_stochastic_petri_json(reachability) << '}';
    response.set_content(out.str(), "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

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
  if (request.has_param("process_tree_variant") &&
      miner != "inductive" && miner != "stochastic" &&
      miner != "monte_carlo") {
    response.status = 400;
    response.set_content(
        error_json("process_tree_variant requires an inductive Petri miner"),
        "application/json");
    return;
  }
  const bool is_declare = miner == "declare";
  const std::string mode = request.has_param("mode")
                               ? request.get_param_value("mode")
                               : std::string{};
  const bool is_stochastic = mode == "stochastic" || miner == "stochastic";
  const bool is_monte_carlo =
      mode == "monte_carlo" || miner == "monte_carlo";
  const bool is_extensive = mode == "extensive";
  if ((request.has_param("return_elements") ||
       request.has_param("max_marking_occ")) && !is_extensive) {
    response.status = 400;
    response.set_content(
        error_json("return_elements and max_marking_occ require extensive Petri playout"),
        "application/json");
    return;
  }
  if (request.has_param("return_visited_elements") &&
      (is_declare || is_stochastic || is_monte_carlo ||
       miner == "process_tree" || miner == "dfg")) {
    response.status = 400;
    response.set_content(
        error_json("return_visited_elements requires basic Petri playout"),
        "application/json");
    return;
  }
  if (request.has_param("return_set_strings") && miner != "process_tree") {
    response.status = 400;
    response.set_content(
        error_json("return_set_strings requires process-tree extensive playout"),
        "application/json");
    return;
  }
  if (is_monte_carlo) {
    const std::string selected_miner =
        miner == "monte_carlo" ? "inductive" : miner;
    std::optional<minera::PetriNet> converted;
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
      const auto& selected = select_cached_or_process_tree_net(
          request, *session, selected_miner, converted);
      const auto map_record_limit = std::min(
          selected.transitions().size(),
          minera::kMonteCarloRequestMapMaxRecords);
      auto options = minera::parse_monte_carlo_params(
          lookup, present, map_record_limit);
      cached_imported_monte_carlo_context(request, *session, selected_miner, options);
      validate_monte_carlo_server_limits(options, selected);
      std::optional<minera::EventLog> projected_powl_log;
      const minera::EventLog* simulation_log =
          &cached_model_log(*session, selected_miner);
      if (selected_miner == "powl") {
        if (!request.has_param("monte_carlo_activity_key"))
          options.activity_key = session->powl_activity_key;
        if (!request.has_param("monte_carlo_timestamp_key"))
          options.timestamp_key = session->powl_timestamp_key;
        projected_powl_log = project_powl_activity(
            session->log, options.activity_key, options.timestamp_key,
            session->powl_case_id_key);
        simulation_log = &*projected_powl_log;
      }
      const auto result =
          minera::monte_carlo(*simulation_log, selected, options);
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
    std::optional<minera::PetriNet> converted;
    const auto reject = [&](std::string message) {
      response.status = 400;
      response.set_content(error_json(message), "application/json");
    };
    try {
      reject_unknown_stochastic_params(request);
      const auto lookup = [&](std::string_view name) {
        const std::string key(name);
        return request.has_param(key) ? request.get_param_value(key) : std::string{};
      };
      const auto present = [&](std::string_view name) {
        return request.has_param(std::string(name));
      };
      auto options = minera::parse_stochastic_playout_params(lookup, present);
      cached_imported_markings(request, *session, selected_miner, options,
                               "stochastic_initial_marking", "stochastic_final_marking");
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
      std::optional<minera::EventLog> projected_powl_log;
      const minera::EventLog* fit_log =
          &cached_model_log(*session, selected_miner);
      if (options.fit_from_log && selected_miner == "powl") {
        projected_powl_log = project_powl_activity(
            session->log, session->powl_activity_key,
            session->powl_timestamp_key, session->powl_case_id_key);
        fit_log = &*projected_powl_log;
      }
      if (options.return_visited_elements) {
        const auto& selected = select_cached_or_process_tree_net(
            request, *session, selected_miner, converted);
        const auto visited = options.fit_from_log
                                 ? minera::stochastic_playout_visited(
                                       selected, *fit_log, options)
                                 : minera::stochastic_playout_visited(
                                       selected, options);
        response.set_content(
            minera::serialize_stochastic_playout_visited_json(selected,
                                                                visited),
            "application/json");
      } else {
        const auto& selected = select_cached_or_process_tree_net(
            request, *session, selected_miner, converted);
        const auto log = options.fit_from_log
                             ? minera::stochastic_playout(selected,
                                                         *fit_log, options)
                             : minera::stochastic_playout(selected, options);
        response.set_content(
            minera::to_xes(log, selected_miner + " stochastic playout"),
            "application/xml");
      }
    } catch (const std::exception& error) {
      reject(error.what());
    }
    return;
  }
  if (miner == "process_tree") {
    if (request.has_param("process_tree_variant")) {
      response.status = 400;
      response.set_content(
          error_json("process_tree_variant requires a Petri miner selection"),
          "application/json");
      return;
    }
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
      const auto result =
          minera::process_tree_playout_result(session->tree, options);
      if (options.return_set_strings) {
        response.set_content(
            minera::serialize_process_tree_playout_json(result),
            "application/json");
      } else {
        response.set_content(minera::to_xes(result.log, "process_tree playout"),
                             "application/xml");
      }
    } catch (const std::exception& error) {
      reject(error.what());
    }
    return;
  }
  std::optional<minera::PetriNet> converted;
  const minera::PetriNet* selected = nullptr;
  if (!is_declare && miner != "dfg") {
    try {
      selected = &select_cached_or_process_tree_net(
          request, *session, miner, converted);
    } catch (const std::exception& error) {
      response.status = 400;
      response.set_content(error_json(error.what()), "application/json");
      return;
    }
  }
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
      const auto result = minera::dfg_playout_result(
          frequency, starts, ends, options,
          performance ? &*performance : nullptr);
      if (options.return_variants &&
          options.variant == minera::DfgPlayoutVariant::kClassic) {
        response.set_content(minera::dfg_playout_json(result),
                             "application/json");
      } else {
        response.set_content(minera::to_xes(result.log, "dfg playout"),
                             "application/xml");
      }
    } catch (const std::exception& error) {
      reject(error.what());
    }
    return;
  }
  if (!is_declare && miner != "dfg" && selected == nullptr) {
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
    try {
      const auto lookup = [&](std::string_view name) {
        return request.has_param(std::string(name))
                   ? request.get_param_value(std::string(name))
                   : std::string{};
      };
      const auto present = [&](std::string_view name) {
        return request.has_param(std::string(name));
      };
      const auto options = minera::parse_declare_playout_params(lookup, present);
      const auto log = minera::declare_playout(session->declare, options);
      response.set_content(minera::to_xes(log, "declare playout"),
                           "application/xml");
    } catch (const std::exception& error) {
      reject(error.what());
    }
    return;
  }

  if (request.has_param("min_length") ||
      (request.has_param("max_length") && mode != "extensive")) {
    reject("min_length and max_length are Declare-only unless max_length "
           "is used with Petri mode=extensive");
    return;
  }
  minera::PlayoutOptions options;
  if (request.has_param("traces")) {
    const auto parsed = parse_bounded("traces", 1, 10000);
    if (!parsed) return;
    options.traces = static_cast<std::size_t>(*parsed);
  }
  if (request.has_param("max_steps")) {
    const auto parsed = parse_bounded("max_steps", 0, 10000);
    if (!parsed) return;
    options.max_steps = static_cast<std::size_t>(*parsed);
  }
  if (request.has_param("seed")) {
    const auto parsed = parse_bounded(
        "seed", 0, std::numeric_limits<std::uint64_t>::max());
    if (!parsed) return;
    options.seed = *parsed;
  }
  if (mode == "extensive" &&
      (request.has_param("max_steps") || request.has_param("seed") ||
       request.has_param("start") || request.has_param("case_interval") ||
       request.has_param("event_interval") ||
       request.has_param("add_only_if_fm_is_reached") ||
       request.has_param("fm_leq_accepted") ||
       request.has_param("return_visited_elements"))) {
    reject("basic Petri playout controls require the default sampling mode");
    return;
  }
  try {
    options.start = ocel_query_number(request, "start", options.start);
    options.case_interval =
        ocel_query_number(request, "case_interval", options.case_interval);
    options.event_interval =
        ocel_query_number(request, "event_interval", options.event_interval);
    if (options.case_interval < 0.0 || options.event_interval < 0.0) {
      reject("case_interval and event_interval must be non-negative");
      return;
    }
    options.add_only_if_fm_is_reached = ocel_query_bool(
        request, "add_only_if_fm_is_reached", false);
    options.fm_leq_accepted =
        ocel_query_bool(request, "fm_leq_accepted", false);
    options.return_visited_elements =
        ocel_query_bool(request, "return_visited_elements", false);
  } catch (const std::exception& error) {
    reject(error.what());
    return;
  }
  options.initial_marking = parse_playout_marking(request, "initial_marking");
  options.final_marking = parse_playout_marking(request, "final_marking");
  cached_imported_markings(request, *session, miner, options);

  minera::ExtensivePlayoutOptions extensive;
  if (is_extensive) {
    const auto lookup = [&](std::string_view name) {
      const std::string wire =
          name == "extensive_return_elements" ? "return_elements"
                                               : "max_marking_occ";
      return request.has_param(wire) ? request.get_param_value(wire)
                                     : std::string{};
    };
    const auto present = [&](std::string_view name) {
      const std::string wire =
          name == "extensive_return_elements" ? "return_elements"
                                               : "max_marking_occ";
      return request.has_param(wire);
    };
    try {
      extensive = minera::parse_extensive_playout_params(lookup, present);
    } catch (const std::exception& error) {
      reject(error.what());
      return;
    }
    extensive.max_traces = options.traces;
    extensive.initial_marking = options.initial_marking;
    extensive.final_marking = options.final_marking;
  }

  constexpr std::size_t kMaxGeneratedEvents = 1'000'000;
  if (options.traces != 0 && options.max_steps != 0 &&
      options.traces > kMaxGeneratedEvents / options.max_steps) {
    reject("Petri playout event budget exceeds the server safety limit");
    return;
  }

  if (options.return_visited_elements) {
    response.set_content(
        minera::serialize_playout_visited_json(
            *selected, minera::playout_visited(*selected, options)),
        "application/json");
    return;
  }

  // mode=extensive enumerates the model's distinct traces (shortest first,
  // capped by `traces` and `max_length`) instead of sampling randomly.
  if (mode == "extensive") {
    if (request.has_param("max_length")) {
      const auto parsed = parse_bounded("max_length", 1, 1000);
      if (!parsed) return;
      extensive.max_length = static_cast<std::size_t>(*parsed);
    }
    if (extensive.return_elements) {
      response.set_content(
          minera::serialize_extensive_playout_visited_json(
              *selected,
              minera::extensive_playout_visited(*selected, extensive)),
          "application/json");
    } else {
      const auto log = minera::extensive_playout(*selected, extensive);
      response.set_content(minera::to_xes(log, miner + " playout"),
                           "application/xml");
    }
    return;
  }

  const auto log = minera::playout(*selected, options);
  response.set_content(minera::to_xes(log, miner + " playout"),
                       "application/xml");
}

// Exports the most recently mined event log — after any server-side filtering
// applied at mine time — as standard XES.
void handle_xes(const httplib::Request& request, httplib::Response& response) {
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
    minera::XesExportOptions options;
    options.name = session->dataset;
    if (request.has_param("xes_export_variant")) {
      options.variant = minera::parse_xes_export_variant(
          request.get_param_value("xes_export_variant"));
    }
    if (request.has_param("xes_compress")) {
      const auto value = request.get_param_value("xes_compress");
      if (value == "true" || value == "1") options.compress = true;
      else if (value == "false" || value == "0") options.compress = false;
      else throw std::invalid_argument(
          "xes_compress: expected true, false, 1, or 0");
    }
    options.infer_metadata =
        ocel_query_bool(request, "xes_infer_metadata", false);
    options.preserve_nulls =
        ocel_query_bool(request, "xes_preserve_nulls", false);
    const bool compressed = options.compress.value_or(false);
    response.set_content(minera::serialize_xes(session->log, options),
                         compressed ? "application/gzip" : "application/xml");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
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
      if (key != "noise_threshold" && key != "most_common_paths" &&
          key != "activities_percentage" && key != "paths_percentage" &&
          key != "keep_connected" && key != "keep_all_activities" &&
          key != "to_activity" && key != "from_activity" &&
          key != "contain_activity") {
        throw std::invalid_argument("unknown DFG export parameter: " + key);
      }
    }
    if (request.has_param("most_common_paths") &&
        !request.has_param("noise_threshold")) {
      throw std::invalid_argument(
          "most_common_paths requires noise_threshold");
    }

    const bool keep_all_activities =
        ocel_query_bool(request, "keep_all_activities", false);
    const bool has_paths_percentage = request.has_param("paths_percentage");
    const bool has_keep_connected = request.has_param("keep_connected");
    if (keep_all_activities && !has_paths_percentage && !has_keep_connected) {
      throw std::invalid_argument(
          "keep_all_activities requires paths_percentage or keep_connected");
    }

    auto filtered = minera::make_dfg_filter_data(session->graph);
    if (request.has_param("noise_threshold")) {
      std::set<std::pair<std::string, std::string>> most_common_paths;
      if (request.has_param("most_common_paths")) {
        for (const auto& pair : minera::parse_activity_pairs(
                 request.get_param_value("most_common_paths"))) {
          most_common_paths.emplace(pair.predecessor, pair.successor);
        }
      }
      filtered = minera::clean_dfg_based_on_noise_threshold(
          filtered,
          ocel_query_number(request, "noise_threshold", 0.0),
          most_common_paths);
    }
    if (request.has_param("activities_percentage")) {
      filtered = minera::filter_dfg_on_activities_percentage(
          filtered, ocel_query_number(request, "activities_percentage", 0.0));
    }
    if (has_paths_percentage) {
      filtered = minera::filter_dfg_on_paths_percentage(
          filtered, ocel_query_number(request, "paths_percentage", 0.0),
          keep_all_activities);
    }
    if (has_keep_connected) {
      filtered = minera::filter_dfg_keep_connected(
          filtered, ocel_query_number(request, "keep_connected", 0.0),
          keep_all_activities);
    }

    const auto query_activity = [&](std::string_view name) {
      const auto key = std::string(name);
      const auto value = request.get_param_value(key);
      if (value.empty())
        throw std::invalid_argument(key + " must not be empty");
      return value;
    };
    if (request.has_param("to_activity")) {
      filtered = minera::filter_dfg_to_activity(
          filtered, query_activity("to_activity"));
    }
    if (request.has_param("from_activity")) {
      filtered = minera::filter_dfg_from_activity(
          filtered, query_activity("from_activity"));
    }
    if (request.has_param("contain_activity")) {
      filtered = minera::filter_dfg_contain_activity(
          filtered, query_activity("contain_activity"));
    }
    const auto analysis = minera::DfgCleanAnalysis{
        filtered.edges, filtered.start_activities, filtered.end_activities};
    response.set_content(minera::to_dfg(analysis), "text/plain");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_dfg_dataframe(const httplib::Request& request,
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
    static const std::set<std::string> allowed = {
        "dfg_operation", "dfg_measure", "dfg_backend",
        "dfg_activity_attribute", "dfg_target_activity_attribute",
        "dfg_timestamp_attribute", "dfg_start_timestamp_attribute",
        "dfg_cost_attribute", "dfg_case_id_attribute", "dfg_window",
        "dfg_keep_once_per_case", "dfg_performance_aggregation",
        "dfg_business_hours", "dfg_business_hour_slots",
        "dfg_sort_caseid_required", "dfg_sort_timestamp_along_case_id",
        "dfg_reduce_columns", "dfg_workcalendar",
        "dfg_event_index_attribute", "dfg_max_start_column",
        "dfg_min_complete_column", "dfg_overlap_column", "dfg_strict",
        "dfg_keep_first_following"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument("unknown dataframe DFG parameter: " +
                                    key);
    }
    const auto lookup = [&](std::string_view name) {
      const auto key = std::string(name);
      return request.has_param(key) ? request.get_param_value(key)
                                    : std::string{};
    };
    const auto present = [&](std::string_view name) {
      return request.has_param(std::string(name));
    };
    const auto params = minera::parse_mine_params(lookup, present);
    const auto table = minera::event_log_to_table(session->log);
    if (params.dfg_operation == minera::DfgTableOperation::kGraph) {
      if (params.dfg_backend == minera::DfgTableBackend::kPandas) {
        response.set_content(
            minera::serialize_dfg_dataframe_json(
                minera::discover_pandas_dfg(table, params.dfg_table_options)),
            "application/json");
      } else {
        const auto lazy = minera::LazyEventTable::from_table(table);
        response.set_content(
            minera::serialize_dfg_dataframe_json(
                minera::discover_polars_dfg(lazy, params.dfg_table_options)),
            "application/json");
      }
    } else if (params.dfg_operation ==
               minera::DfgTableOperation::kPartialOrder) {
      if (params.dfg_backend == minera::DfgTableBackend::kPandas) {
        response.set_content(
            minera::serialize_dfg_partial_order_json(
                minera::discover_pandas_partial_order(
                    table, params.dfg_table_options)),
            "application/json");
      } else {
        const auto lazy = minera::LazyEventTable::from_table(table);
        response.set_content(
            minera::serialize_dfg_partial_order_json(
                minera::discover_polars_partial_order(
                    lazy, params.dfg_table_options)),
            "application/json");
      }
    } else {
      if (params.dfg_backend == minera::DfgTableBackend::kPandas) {
        response.set_content(
            minera::serialize_dfg_concurrent_events_json(
                minera::discover_pandas_concurrent_events(
                    table, params.dfg_table_options)),
            "application/json");
      } else {
        const auto lazy = minera::LazyEventTable::from_table(table);
        response.set_content(
            minera::serialize_dfg_concurrent_events_json(
                minera::discover_polars_concurrent_events(
                    lazy, params.dfg_table_options)),
            "application/json");
      }
    }
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

minera::TemporalProfile parse_temporal_profile_parameter(
    std::string_view profile_text, std::string_view parameter_name) {
  if (profile_text.empty()) {
    throw std::invalid_argument(std::string(parameter_name) +
                                " must not be empty");
  }
  minera::TemporalProfile profile;
  std::size_t begin = 0;
  while (begin <= profile_text.size()) {
    const auto end = profile_text.find(',', begin);
    const auto item = profile_text.substr(
        begin, end == std::string_view::npos ? std::string_view::npos
                                             : end - begin);
    const auto arrow = item.find("->");
    const auto first_colon = item.find(':');
    const auto second_colon = item.find(
        ':', first_colon == std::string_view::npos ? 0 : first_colon + 1);
    if (arrow == std::string_view::npos ||
        first_colon == std::string_view::npos ||
        second_colon == std::string_view::npos || arrow == 0 ||
        first_colon <= arrow + 2 || second_colon <= first_colon + 1 ||
        second_colon + 1 >= item.size()) {
      throw std::invalid_argument(
          std::string(parameter_name) +
          " entries must use source->target:mean:stddev");
    }
    const auto mean_text = std::string(
        item.substr(first_colon + 1, second_colon - first_colon - 1));
    const auto stddev_text = std::string(item.substr(second_colon + 1));
    char* mean_end = nullptr;
    char* stddev_end = nullptr;
    const double mean = std::strtod(mean_text.c_str(), &mean_end);
    const double stddev = std::strtod(stddev_text.c_str(), &stddev_end);
    if (mean_end != mean_text.c_str() + mean_text.size() ||
        stddev_end != stddev_text.c_str() + stddev_text.size() ||
        !std::isfinite(mean) || mean < 0.0 || !std::isfinite(stddev) ||
        stddev < 0.0) {
      throw std::invalid_argument(std::string(parameter_name) +
                                  " timings must be finite and non-negative");
    }
    profile.entries.push_back(
        {std::string(item.substr(0, arrow)),
         std::string(item.substr(arrow + 2, first_colon - arrow - 2)), 1,
         mean, stddev});
    if (end == std::string_view::npos) break;
    begin = end + 1;
  }
  return profile;
}

void handle_temporal_conformance(const httplib::Request& request,
                                 httplib::Response& response) {
  try {
    static const std::set<std::string> allowed = {
        "temporal_profile",
        "temporal_variant",
        "temporal_activity_attribute",
        "temporal_case_id_attribute",
        "temporal_source_timestamp_attribute",
        "temporal_target_timestamp_attribute",
        "temporal_business_hours",
        "temporal_business_hour_slots",
        "temporal_strict_attributes",
        "temporal_zeta"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key)) {
        throw std::invalid_argument(
            "unknown temporal conformance parameter: " + key);
      }
    }
    if (!request.has_param("temporal_profile")) {
      throw std::invalid_argument("temporal_profile is required");
    }

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

    const auto lookup = [&](std::string_view name) {
      const auto key = std::string(name);
      return request.has_param(key) ? request.get_param_value(key)
                                    : std::string{};
    };
    const auto present = [&](std::string_view name) {
      return request.has_param(std::string(name));
    };
    auto options = minera::parse_mine_params(lookup, present).temporal_profile;
    // This route is an explicit conformance request, unlike bundled discovery.
    // Preserve the strict standalone component default unless it is overridden.
    if (!present("temporal_strict_attributes")) {
      options.strict_attributes = true;
    }
    const auto profile = parse_temporal_profile_parameter(
        request.get_param_value("temporal_profile"), "temporal_profile");
    const auto variant = present("temporal_variant")
                             ? request.get_param_value("temporal_variant")
                             : std::string{"log"};
    minera::TemporalConformanceResult result;
    if (variant == "log") {
      result = minera::conform_temporal_profile_log(session->log, profile,
                                                    options);
    } else if (variant == "dataframe") {
      result = minera::conform_temporal_profile_dataframe(
          minera::event_log_to_table(session->log), profile, options);
    } else {
      throw std::invalid_argument(
          "temporal_variant must be log or dataframe");
    }
    response.set_content(minera::temporal_conformance_json(result),
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
    auto profile = parse_temporal_profile_parameter(
        request.get_param_value("streaming_temporal_profile"),
        "streaming_temporal_profile");
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
               " (default: src/frontend/dist next to the working directory)\n"
               "  --embedding-runtime-url <url> HTTP transformer runtime (POST /embed)\n"
               "  --llm-runtime-url <url> Optional language-model runtime (POST /complete)\n"
               "  --query-runtime-url <url> Optional DuckDB query runtime (POST /query)\n";
  return 2;
}

}  // namespace

int main(int argc, char** argv) {
  int port = 8484;
  std::filesystem::path frontend_dir = "src/frontend/dist";
  std::string embedding_runtime_url;

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
    } else if (arg == "--embedding-runtime-url") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      embedding_runtime_url = value;
    } else if (arg == "--llm-runtime-url") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      g_llm_runtime_url = value;
    } else if (arg == "--query-runtime-url") {
      const auto* value = next();if(!value)return usage(argv[0]);g_query_runtime_url=value;
    } else {
      return usage(argv[0]);
    }
  }

  try {
    if (!embedding_runtime_url.empty())
      minera::set_default_embedding_provider(
          minera::embedding_runtime::make_http_provider(embedding_runtime_url));
    g_embedding_runtime_enabled = !embedding_runtime_url.empty();
    if (!g_llm_runtime_url.empty()) minera::llm_runtime::validate_url(g_llm_runtime_url);
    if (!g_query_runtime_url.empty()) minera::llm_runtime::validate_url(g_query_runtime_url);
    for (const auto local_port : {port, 5173, 4173}) {
      g_llm_origins.insert("http://127.0.0.1:" + std::to_string(local_port));
      g_llm_origins.insert("http://localhost:" + std::to_string(local_port));
    }
  } catch (const std::exception& error) {
    std::cerr << "error: " << error.what() << '\n';
    return 2;
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
  server.Options("/api/log-query", preflight);
  server.Options("/api/log-query/export", preflight);
  server.Options("/api/log-cluster", preflight);
  server.Options("/api/llm-context", preflight);
  server.Options("/api/llm/complete", [](const httplib::Request& request, httplib::Response& response) {
    if (!llm_origin_allowed(request, response)) return;
    response.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    response.set_header("Access-Control-Allow-Headers", "Content-Type");
    response.status = 204;
  });
  server.Options("/api/ocel/discover", preflight);
  server.Options("/api/ocel/split", preflight);
  server.Options("/api/ocel/enrich", preflight);
  server.Options("/api/ocel/graph", preflight);
  server.Options("/api/ocel/event-object-graph", preflight);
  server.Options("/api/ocel/flatten", preflight);
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
  server.Options("/api/genetic-matrix-convert", preflight);
  server.Options("/api/log-convert", preflight);
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
  server.Options("/api/browser-history", preflight);
  server.Options("/api/windows-events", preflight);
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
  server.Options("/api/format-dataframe", preflight);
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
  server.Options("/api/reduce-petri-net", preflight);
  server.Options("/api/align-dfg", preflight);
  server.Options("/api/discover-alpha-plus", preflight);
  server.Options("/api/align-edit-distance", preflight);
  server.Options("/api/decide", preflight);
  server.Options("/api/decision-tables", preflight);
  server.Options("/api/data-petri-net", preflight);
  server.Options("/api/check-model", preflight);
  server.Options("/api/token-replay", preflight);
  server.Options("/api/replay-fitness", preflight);
  server.Options("/api/process-tree-petri", preflight);
  server.Options("/api/playout", preflight);
  server.Options("/api/monte-carlo", preflight);
  server.Options("/api/dfg", preflight);
  server.Options("/api/dfg-dataframe", preflight);
  server.Options("/api/dfg-precision", preflight);
  server.Options("/api/dfg-to-petri", preflight);
  server.Options("/api/process-cube", preflight);
  server.Options("/api/streaming-dfg", preflight);
  server.Options("/api/streaming-footprints", preflight);
  server.Options("/api/streaming-temporal", preflight);
  server.Options("/api/temporal-conformance", preflight);
  server.Options("/api/streaming-tbr", preflight);
  server.Options("/api/knowledge-injection", preflight);

  server.Post("/api/mine", handle_mine);
  server.Post("/api/ocel/discover", handle_ocel_discovery);
  server.Post("/api/ocel/split", handle_ocel_split);
  server.Post("/api/ocel/enrich", handle_ocel_enrichment);
  server.Post("/api/ocel/graph", handle_ocel_graph);
  server.Post("/api/ocel/event-object-graph", handle_ocel_event_object_graph);
  server.Post("/api/ocel/flatten", handle_ocel_flatten);
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
  server.Post("/api/genetic-matrix-convert", handle_genetic_matrix_conversion);
  server.Post("/api/log-convert", handle_log_conversion);
  server.Post("/api/organizational-sna", handle_organizational_sna);
  server.Post("/api/sna", handle_organizational_sna);
  server.Post("/api/network-analysis", handle_network_analysis);
  server.Post("/api/link-analysis", handle_link_analysis);
  server.Post("/api/roles", handle_roles);
  server.Post("/api/resource-profiles", handle_resource_profiles);
  server.Post("/api/knowledge-injection", handle_knowledge_injection);
  server.Get("/api/llm/status", handle_llm_status);
  server.Post("/api/llm/complete", handle_llm_complete);
  server.Get("/api/log-query/schema",handle_log_query_schema);
  server.Post("/api/log-query",handle_log_query);
  server.Post("/api/log-query/export",handle_log_query_export);
  server.Post("/api/log-cluster",handle_log_cluster);
  server.Post("/api/llm-context",handle_llm_context);
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
  server.Post("/api/browser-history", handle_browser_history);
  server.Post("/api/windows-events", handle_windows_events);
  server.Post("/api/repository-history", handle_repository_history);
  server.Post("/api/workflow-history", handle_workflow_history);
  server.Post("/api/enterprise-history", handle_enterprise_history);
  server.Post("/api/communication-history", handle_communication_history);
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
  server.Post("/api/format-dataframe", handle_format_dataframe);
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
  server.Post("/api/reduce-petri-net", handle_reduce_petri_net);
  server.Post("/api/align-dfg", handle_align_dfg);
  server.Post("/api/discover-alpha-plus", handle_alpha_plus_result);
  server.Post("/api/align-edit-distance", handle_align_edit_distance);
  server.Post("/api/decide", handle_decide);
  server.Post("/api/decision-tables", handle_decision_tables);
  server.Post("/api/data-petri-net", handle_data_petri_net);
  server.Post("/api/check-model", handle_check_model);
  server.Post("/api/token-replay", handle_token_replay);
  server.Post("/api/replay-fitness", handle_replay_fitness);
  server.Get("/api/pnml", handle_pnml);
  server.Get("/api/process-tree-petri", handle_process_tree_petri);
  server.Get("/api/bpmn", handle_bpmn);
  server.Get("/api/ptml", handle_ptml);
  server.Get("/api/xes", handle_xes);
  server.Get("/api/dfg", handle_dfg);
  server.Get("/api/dfg-dataframe", handle_dfg_dataframe);
  server.Post("/api/dfg-dataframe", handle_dfg_dataframe);
  server.Post("/api/dfg", handle_dfg_import);
  server.Post("/api/dfg-precision", handle_dfg_precision);
  server.Post("/api/dfg-to-petri", handle_dfg_conversion);
  server.Post("/api/process-cube", handle_process_cube);
  server.Post("/api/streaming-dfg", handle_streaming_dfg);
  server.Post("/api/streaming-footprints", handle_streaming_footprints);
  server.Post("/api/streaming-temporal", handle_streaming_temporal);
  server.Post("/api/temporal-conformance", handle_temporal_conformance);
  server.Post("/api/streaming-tbr", handle_streaming_tbr);
  server.Get("/api/playout", handle_playout);
  server.Post("/api/playout", handle_playout);
  server.Get("/api/stochastic-petri", handle_stochastic_petri);
  server.Get("/api/stochastic-map", handle_stochastic_map);
  server.Get("/api/monte-carlo", handle_monte_carlo_json);
  server.Post("/api/monte-carlo", handle_monte_carlo_json);

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
