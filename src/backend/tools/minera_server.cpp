#include "minera/exact_filter_discovery_json.hpp"
#include "minera/cancellation.hpp"
#include "minera/case_list_order.hpp"
#include "minera/case_description_workbench_json.hpp"
#include "minera/reference_log_filter_workbench.hpp"
#include "minera/table_attribute_repetition_workbench.hpp"
#include "minera/table_between_filter_workbench.hpp"
#include "minera/table_case_filter_workbench.hpp"
#include "minera/table_paths_filter_workbench.hpp"
#include "minera/table_timestamp_filter_workbench.hpp"
#include "minera/table_variants_filter_workbench.hpp"
#include "minera/table_attributes_filter_workbench.hpp"
#include "minera/log_attributes_filter_workbench.hpp"
#include "minera/log_variants_filter_workbench.hpp"
#include "minera/log_timestamp_filter_workbench.hpp"
#include "minera/ltl_filter_workbench.hpp"
#include "minera/ocel_exact_filter_controls.hpp"
#include "minera/ocel_activity_type_matching.hpp"
#include "ocdfg_performance_route.hpp"
#include "ocdfg_edge_metrics_route.hpp"
#include "case_relations_input.hpp"
#include "case_relations_typed_input.hpp"
// Local application server: serves the built frontend and mines uploaded
// CSV event logs with the native core via POST /api/mine.

#include "pnml_model_options.hpp"
#include "imported_alignment.hpp"
#include "replay_input_options.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <initializer_list>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <random>
#include <sstream>
#include <set>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
#if defined(__GLIBC__)
#include <malloc.h>
#endif

// A slowly dripping client may hold a worker thread only this long (see the
// MINERA PATCH notes in third_party/httplib.h): the request line and headers
// within 10 s, the body within 120 s and, after its first 5 s, at an average
// of at least 64 KiB/s (loopback uploads run far faster), on top of the 5 s
// per-read timeout.
#define CPPHTTPLIB_HEADER_READ_TIMEOUT_SECOND 10
#define CPPHTTPLIB_BODY_READ_TIMEOUT_SECOND 120
#define CPPHTTPLIB_BODY_MIN_RATE_BYTES_PER_SECOND 65536
#include "httplib.h"
#include "../secure_temp.hpp"
#include "../gzip_codec.hpp"
#include "../json_utf8.hpp"
#include "embedding_runtime.hpp"
#include "llm_runtime.hpp"
#include "query_runtime.hpp"
#include "minera/exact_log_query_json.hpp"
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
#include "minera/dfg_filtering.hpp"
#include "minera/dfg_io.hpp"
#include "minera/dfg_precision.hpp"
#include "minera/dfg_alignment.hpp"
#include "minera/dfg_playout.hpp"
#include "minera/json_export.hpp"
#include "minera/analysis_json_traces.hpp"
#include "minera/event_table_json.hpp"
#include "minera/knowledge_injection.hpp"
#include "minera/knowledge_exact_json.hpp"
#include "minera/edit_distance_alignment.hpp"
#include "minera/dfg_variants.hpp"
#include "minera/eventually_follows.hpp"
#include "minera/footprints.hpp"
#include "minera/heuristic_miner.hpp"
#include "minera/inductive_miner.hpp"
#include "minera/inductive_input_json.hpp"
#include "minera/process_tree_alignment_workbench_json.hpp"
#include "minera/petri_anti_multi_workbench_json.hpp"
#include "minera/model_conversion_workbench_json.hpp"
#include "minera/tree_reduction_workbench_json.hpp"
#include "minera/original_model_context_workbench_json.hpp"
#include "minera/original_features_workbench_json.hpp"
#include "minera/original_label_splitting_workbench_json.hpp"
#include "minera/original_log_query_workbench_json.hpp"
#include "minera/active_event_source.hpp"
#include "minera/monte_carlo_workbench_json.hpp"
#include "minera/playout_workbench_json.hpp"
#include "minera/decision_workbench_json.hpp"
#include "minera/declare_workbench_json.hpp"
#include "minera/log_skeleton_workbench_json.hpp"
#include "minera/footprints_workbench_json.hpp"
#include "minera/genetic_matrix_workbench_json.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/emd_workbench_json.hpp"
#include "minera/object_centric_playout_workbench_json.hpp"
#include "minera/literal_controls_json.hpp"
#include "minera/replay_duration_workbench_json.hpp"
#include "minera/token_replay_duration_error.hpp"
#include "minera/token_replay_error.hpp"
#include "minera/edit_distance_workbench_json.hpp"
#include "minera/bose_workbench_json.hpp"
#include "minera/discovery_workbench_json.hpp"
#include "minera/petri_analysis_workbench_json.hpp"
#include "minera/root_cause_workbench_json.hpp"
#include "minera/anonymization_workbench_json.hpp"
#include "minera/quality_workbench_json.hpp"
#include "minera/clustering_workbench_json.hpp"
#include "minera/organizational_workbench_json.hpp"
#include "minera/temporal_workbench_json.hpp"
#include "minera/timing_workbench_json.hpp"
#include "minera/time_analysis_workbench_json.hpp"
#include "minera/structural_inputs_json.hpp"
#include "minera/genetic_matrix_conversion.hpp"
#include "../footprints/conformance_log_model.hpp"
#include "../footprints/conformance_log_extensive.hpp"
#include "../footprints/conformance_trace_extensive.hpp"
#include "../footprints/conformance_evaluation.hpp"
#include "../footprints/conformance_tree_visualization.hpp"
#include "minera/dfg_playout_input_json.hpp"
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
#include "minera/workflow_discovery_input_json.hpp"
#include "minera/workflow_input_result_json.hpp"
#include "minera/graph_alignment_workbench_json.hpp"
#include "minera/saved_dfg_projection_json.hpp"
#include "minera/random_tree.hpp"
#include "minera/model_decoration.hpp"
#include "minera/monte_carlo.hpp"
#include "minera/oc_causal_net.hpp"
#include "minera/ocel.hpp"
#include "minera/otg_threshold_json.hpp"
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
#include "minera/process_tree_petri.hpp"
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
#include "minera/streaming_session.hpp"
#include "minera/reader_stream_session.hpp"
#include "minera/source_acquisition.hpp"
#include "minera/path_filter.hpp"
#include "minera/decomposed_alignment.hpp"
#include "minera/petri_anti_alignment.hpp"
#include "minera/petri_multi_alignment.hpp"
#include "minera/process_tree_reduction.hpp"
#include "minera/process_cube.hpp"
#include "minera/resource_analysis.hpp"
#include "minera/temporal_profile.hpp"
#include "minera/temporal_profile_json.hpp"
#include "minera/precomputed_replay_json.hpp"
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

// Upload and download limits (README "Limits").  Minera holds about 17 times
// an XES document's size in memory, so a 128 MiB request (or a .xes.gz that
// inflates to the 256 MiB XES default) stays within a 8-16 GB machine.  Every
// OCEL route parses up to the same 128 MiB.
constexpr std::size_t kMaxRequestPayloadBytes = 128u * 1024u * 1024u;
constexpr std::size_t kMaxResponseBytes = 128u * 1024u * 1024u;

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
    const minera::MinimumSelfDistanceOptions& options,
    const minera::ActiveEventSource* active_source = nullptr) {
  switch (options.variant) {
    case minera::MinimumSelfDistanceVariant::kLog:
      return minera::MinimumSelfDistanceLogDiscovery::apply(active_source ? active_source->checked_log() : log, options);
    case minera::MinimumSelfDistanceVariant::kPandas:
      return minera::MinimumSelfDistancePandasDiscovery::apply(
          active_source ? active_source->checked_table() : minera::event_log_to_table(log), options);
    case minera::MinimumSelfDistanceVariant::kPolars:
      return minera::MinimumSelfDistancePolarsDiscovery::apply(
          active_source ? active_source->checked_lazy() : minera::event_log_to_lazy_table(log), options);
  }
  throw std::invalid_argument("minimum self-distance variant is invalid");
}

minera::PerformanceSpectrum discover_selected_performance_spectrum(
    const minera::EventLog& log, const std::vector<std::string>& activities,
    const minera::PerformanceSpectrumOptions& options,
    const minera::ActiveEventSource* active_source = nullptr) {
  switch (options.variant) {
    case minera::PerformanceSpectrumVariant::kDataframe:
      return minera::discover_performance_spectrum_dataframe(
          active_source ? active_source->checked_table() : minera::event_log_to_table(log), activities, options);
    case minera::PerformanceSpectrumVariant::kDataframeDisconnected:
      return minera::discover_performance_spectrum_dataframe_disconnected(
          active_source ? active_source->checked_table() : minera::event_log_to_table(log), activities, options);
    case minera::PerformanceSpectrumVariant::kLazyframe:
      return minera::discover_performance_spectrum_lazyframe(
          active_source ? active_source->checked_lazy() : minera::event_log_to_lazy_table(log), activities, options);
    case minera::PerformanceSpectrumVariant::kLazyframeDisconnected:
      return minera::discover_performance_spectrum_lazyframe_disconnected(
          active_source ? active_source->checked_lazy() : minera::event_log_to_lazy_table(log), activities, options);
    case minera::PerformanceSpectrumVariant::kConnected:
    case minera::PerformanceSpectrumVariant::kDisconnected:
    case minera::PerformanceSpectrumVariant::kLog:
    case minera::PerformanceSpectrumVariant::kLogDisconnected:
      return minera::compute_performance_spectrum(active_source ? active_source->checked_log() : log, activities, options);
  }
  throw std::invalid_argument("performance spectrum variant is invalid");
}

minera::BatchDiscovery discover_selected_batches(
    const minera::EventLog& log, const minera::BatchDiscoveryOptions& options,
    const minera::ActiveEventSource* active_source = nullptr) {
  switch (options.variant) {
    case minera::BatchDiscoveryVariant::kLog:
      return minera::BatchLogDiscovery::apply(active_source ? active_source->checked_log() : log, options);
    case minera::BatchDiscoveryVariant::kPandas:
      return minera::BatchPandasDiscovery::apply(
          active_source ? active_source->checked_table() : minera::event_log_to_table(log), options);
    case minera::BatchDiscoveryVariant::kPolars:
      return minera::BatchPolarsDiscovery::apply(
          active_source ? active_source->checked_lazy() : minera::event_log_to_lazy_table(log), options);
  }
  throw std::invalid_argument("batch discovery variant is invalid");
}

// The most recently mined log and its discovered nets, kept so the costly
// alignment of a model can be computed on demand (POST /api/align) instead of
// eagerly for every miner on every mine. The app is local and single-user, so
// caching just the latest analysis is enough; a shared_ptr lets a slow
// alignment run on a snapshot without holding the lock.
std::atomic<std::uint64_t> g_query_source_sequence{0};
// Each model import gets a fresh id (0 = none). A client that sends the id it
// was given in the X-Minera-Imported-Model header has its request refused
// when the server's imported model is not that one for the whole request.
std::atomic<std::uint64_t> g_imported_model_sequence{0};
struct MinedSession {
  std::uint64_t query_source_id=++g_query_source_sequence;
  std::uint64_t imported_model_id = 0;
  std::shared_ptr<const minera::ActiveEventSource> active_source;
  // Literal/typed/native-flat mining binds defaults to the retained source;
  // legacy grouped-log mining keeps its existing native selector defaults.
  bool active_source_native_dispatch = false;
  bool log_ready = true;
  bool alpha_log_ready = true;
  bool inductive_log_ready = true;
  bool tree_ready = true;
  bool graph_ready = true;
  bool declare_ready = true;
  std::string mining_request_id;
  std::string paired_csv_import_error;
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
  std::optional<minera::PowlNode> powl;  // exact model retained for footprint comparisons
  minera::DeclareModel declare;  // classic Declare, for on-demand playout
  std::string dataset;       // uploaded file name, echoed into XES exports
  std::optional<minera::PnmlDocument> imported_document;
  std::string imported_activity_attribute = "concept:name";
  minera::TokenReplayOptions token_replay;
  minera::InductiveMinerOptions inductive_options;
  std::string alpha_activity_attribute = "concept:name";
  std::string alpha_case_id_attribute = "case:concept:name";
  std::string powl_activity_key = "concept:name";
  std::string heuristic_activity_attribute = "concept:name";
  std::string heuristic_case_id_attribute = "case:concept:name";
  std::string powl_timestamp_key = "time:timestamp";
  std::string powl_case_id_key = "case:concept:name";
  // When set, the prepared logs are the active source's own checked log
  // (shared, not copied): they point into active_source, which this session
  // and every copy of it keep alive. The owned members above are then empty.
  const minera::EventLog* log_view = nullptr;
  const minera::EventLog* inductive_log_view = nullptr;
  const minera::EventLog* alpha_log_view = nullptr;
};

const minera::EventLog& session_log(const MinedSession& session) {
  return session.log_view ? *session.log_view : session.log;
}
const minera::EventLog& session_inductive_log(const MinedSession& session) {
  return session.inductive_log_view ? *session.inductive_log_view : session.inductive_log;
}
const minera::EventLog& session_alpha_log(const MinedSession& session) {
  return session.alpha_log_view ? *session.alpha_log_view : session.alpha_log;
}

// True when a miner's prepared log (normalize_alpha_event_log,
// prepare_inductive_event_log) came back equal to its input in every field
// those preparations can set, so the session can share the input instead of
// keeping a second copy. Event order, grouping and all attributes are
// compared too; any difference keeps the prepared copy.
bool prepared_log_is_identity(const minera::EventLog& prepared, const minera::EventLog& source) {
  if (prepared.traces.size() != source.traces.size() ||
      !(prepared.attributes == source.attributes) || !(prepared.metadata == source.metadata))
    return false;
  for (std::size_t t = 0; t < source.traces.size(); ++t) {
    const auto& a = prepared.traces[t];
    const auto& b = source.traces[t];
    if (a.case_id != b.case_id || a.case_id_state != b.case_id_state ||
        a.case_id_source_key != b.case_id_source_key || !(a.case_id_attribute == b.case_id_attribute) ||
        a.case_id_source_position != b.case_id_source_position ||
        !(a.attributes == b.attributes) || !(a.properties == b.properties) ||
        a.events.size() != b.events.size())
      return false;
    for (std::size_t e = 0; e < b.events.size(); ++e) {
      const auto& x = a.events[e];
      const auto& y = b.events[e];
      if (x.input_order != y.input_order || x.case_id != y.case_id || x.activity != y.activity ||
          x.timestamp != y.timestamp || x.resource != y.resource || x.lifecycle != y.lifecycle ||
          x.case_id_state != y.case_id_state || x.activity_state != y.activity_state ||
          x.timestamp_state != y.timestamp_state || x.resource_state != y.resource_state ||
          x.lifecycle_state != y.lifecycle_state ||
          x.case_id_source_key != y.case_id_source_key || x.activity_source_key != y.activity_source_key ||
          x.timestamp_source_key != y.timestamp_source_key || x.resource_source_key != y.resource_source_key ||
          x.lifecycle_source_key != y.lifecycle_source_key ||
          !(x.activity_attribute == y.activity_attribute) || !(x.timestamp_attribute == y.timestamp_attribute) ||
          !(x.resource_attribute == y.resource_attribute) || !(x.lifecycle_attribute == y.lifecycle_attribute) ||
          x.first_class_source_positions != y.first_class_source_positions ||
          !(x.attributes == y.attributes) || !(x.case_row_attributes == y.case_row_attributes))
        return false;
    }
  }
  return true;
}

const minera::EventLog& require_prepared_log(const MinedSession& session) {
  if (!session.log_ready) throw std::invalid_argument(
      "the active source has no prepared EventLog; inspect its preparation error");
  return session_log(session);
}

const minera::ProcessTree& require_process_tree(const MinedSession& session) {
  if (!session.tree_ready) throw std::invalid_argument(
      "the active source has no successfully discovered process tree");
  return session.tree;
}

const minera::DirectlyFollowsGraph& require_graph(const MinedSession& session) {
  if (!session.graph_ready) throw std::invalid_argument(
      "the active source has no successfully discovered directly-follows graph");
  return session.graph;
}

minera::EventDataView cached_source_view(const MinedSession& session,
                                       minera::EventDataKind kind) {
  if (session.active_source) return session.active_source->view(kind);
  if (kind == minera::EventDataKind::kEventLog)
    return minera::EventDataView(require_prepared_log(session));
  throw std::invalid_argument("this cached source has no retained flat native input");
}

const minera::EventLog& cached_model_log(const MinedSession& session,
                                         std::string_view miner) {
  if (miner == "inductive" || miner == "process_tree") {
    if (!session.inductive_log_ready)
      throw std::invalid_argument("the cached Inductive prepared log is unavailable");
    return session_inductive_log(session);
  }
  if (miner == "alpha") {
    if (!session.alpha_log_ready)
      throw std::invalid_argument("the cached Alpha prepared log is unavailable");
    return session_alpha_log(session);
  }
  return require_prepared_log(session);
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

// Monte Carlo fits its stochastic map by token replay on the model's own log,
// selecting events by activity_key. Without an explicit key, select the
// activity the way mining replays that log (a CSV log keeps its activity
// column as the source key, so "concept:name" would not resolve); "activity"
// and "concept:name" both name the playout's own activity field.
void default_monte_carlo_activity_key(const httplib::Request& request,
                                      const minera::EventLog& log,
                                      minera::MonteCarloOptions& options) {
  if (request.has_param("monte_carlo_activity_key")) return;
  minera::TokenReplayOptions probe;
  probe.activity_attribute = options.activity_key;
  options.activity_key =
      minera::tools::default_replay_input_options(log, std::move(probe)).activity_attribute;
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
  return minera::to_petri_net(require_process_tree(session), variant);
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

struct ClusteringAnalysis {
  std::string id;
  std::shared_ptr<const MinedSession> source;
  std::optional<minera::TraceAttributeClusteringResult> hierarchy;
  std::optional<minera::ProfileClusteringResult> profiles;
  std::vector<minera::TraceAttributeClusteringGroup> groups;
};
std::atomic<std::uint64_t> g_clustering_sequence{0};
std::shared_ptr<const ClusteringAnalysis> g_clustering_analysis;
bool g_embedding_runtime_enabled = false;
// Assigned only during startup; request-owned options receive a callback copy.
minera::EmbeddingProviderCallback g_embedding_explicit_runtime;
std::string g_llm_runtime_url;
std::string g_query_runtime_url;
std::set<std::string> g_allowed_origins;
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

void reject_unknown_monte_carlo_params(const httplib::Request& request,
                                      bool allow_workspace = false) {
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
      "monte_carlo_numpy_legacy_seed", "monte_carlo_numpy_choice_order",
      "monte_carlo_provided_stochastic_map",
      "process_tree_variant", "source_id"};
  for (const auto& [key, value] : request.params) {
    (void)value;
    if (!allowed.contains(key) && !(allow_workspace && key == "include_workspace"))
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
      "stochastic_final_marking", "process_tree_variant", "source_id"};
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

// Records which multipart fields a handler consulted, so a handler that
// reads its fields lazily can still reject the ones it never looked at
// (misspelled or inapplicable controls) instead of silently ignoring them.
struct FormFieldAudit {
  std::set<std::string> consulted;
};
thread_local FormFieldAudit* g_form_field_audit = nullptr;
void audit_form_field(std::string_view name) {
  if (g_form_field_audit) g_form_field_audit->consulted.emplace(name);
}
class FormFieldAuditScope {
 public:
  FormFieldAuditScope() : previous_(g_form_field_audit) { g_form_field_audit = &audit_; }
  ~FormFieldAuditScope() { g_form_field_audit = previous_; }
  FormFieldAuditScope(const FormFieldAuditScope&) = delete;
  FormFieldAuditScope& operator=(const FormFieldAuditScope&) = delete;
  // The first supplied multipart field that was never consulted, if any.
  std::optional<std::string> unconsulted(const httplib::Request& request) const {
    for (const auto& [name, field] : request.files) {
      (void)field;
      if (!audit_.consulted.contains(name)) return name;
    }
    return std::nullopt;
  }

 private:
  FormFieldAudit audit_;
  FormFieldAudit* previous_;
};

std::string form_field(const httplib::Request& request,
                       const std::string& name) {
  audit_form_field(name);
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
    const minera::FootprintsOptions& options,
    const minera::ActiveEventSource* active_source = nullptr) {
  minera::FootprintsAnalysis analysis;
  analysis.model = options.model;
  analysis.strict = options.strict;
  const auto discovery_options = footprints_discovery_options(options);
  const auto& selected_log = options.model == "alpha" ? alpha_log : active_source ? active_source->checked_log() : log;
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
          active_source ? active_source->checked_table() : minera::prepare_footprints_event_table(selected_log, discovery_options);
    }
    analysis.entire_dataframe = minera::discover_footprints(
        *dataframe, discovery_options);
  }
  if (options.discovery_variants.contains("polars_lazyframes")) {
    if (!lazy_dataframe) {
      lazy_dataframe =
          active_source ? active_source->checked_lazy() : minera::prepare_footprints_lazy_table(selected_log, discovery_options);
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

// pm4py.discover_performance_dfg on a CSV or XES import runs on a DataFrame;
// only a native table source is an EventLog. Performance numbers (process
// map, DFG performance variant) follow the matching arithmetic.
bool dataframe_performance_source(const minera::ActiveEventSource* source) {
  return source != nullptr && source->file().format != "table";
}

// The XES downloads of a CSV-sourced log write the case id, activity and
// timestamp under pm4py's standard keys, as read_csv, format_dataframe and
// write_xes do; an XES source keeps its own keys, as read_xes and write_xes
// keep them.
bool standard_keys_for_session(const MinedSession& session) {
  return session.active_source && session.active_source->file().format == "csv";
}

minera::DfgStatisticalOptions dfg_statistics_for_source(
    minera::DfgStatisticalOptions options,
    const minera::ActiveEventSource* source) {
  options.dataframe_arithmetic = dataframe_performance_source(source);
  return options;
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

// A client that gave up (closed the tab, aborted the fetch) gets nothing from
// a result. Long searches poll this callback and stop instead of finishing
// for no one; it is empty when the transport cannot tell.
std::function<bool()> connection_cancel_hook(const httplib::Request& request) {
  if (!request.is_connection_closed) return {};
  return [&request] { return request.is_connection_closed(); };
}

// Server bounds of stochastic Petri playout (handle_playout and the playout
// workbench). Silent firings do not count towards max_steps, so the visible
// budget (traces * max_steps <= 1,000,000) does not bound the work: every
// firing of a run counts, and a disconnect cancels. A visited-elements run
// is held to 512 MiB of peak memory per request: its buffers are bounded by
// that while it runs (max_visited_bytes), and before its JSON is built the
// exact document size, times the copies that live beside the result, must
// fit in what the result leaves (check_server_stochastic_visited_peak).
constexpr std::size_t kServerStochasticFirings = 10'000'000;
constexpr std::size_t kServerStochasticVisitedBytes = std::size_t{512} << 20;

void bound_server_stochastic_playout(minera::StochasticPlayoutOptions& options,
                                     const minera::PetriNet&, bool visited,
                                     const httplib::Request& request) {
  options.max_total_firings = kServerStochasticFirings;
  if (visited) options.max_visited_bytes = kServerStochasticVisitedBytes;
  options.cancelled = connection_cancel_hook(request);
}

// `copies` is how many documents of `json_bytes` live at the peak: the
// playout route's body (1), its UTF-8 repair copy and the gzip output (3 in
// all); the workbench also wraps and concatenates it (5).
void check_server_stochastic_visited_peak(const minera::PlayoutVisitedResult& visited,
                                          std::size_t json_bytes, std::size_t copies) {
  const auto held = minera::playout_visited_bytes(visited);
  if (held > kServerStochasticVisitedBytes ||
      json_bytes > (kServerStochasticVisitedBytes - held) / copies)
    throw std::length_error(
        "stochastic playout: the visited elements and their " + std::to_string(json_bytes) +
        "-byte JSON would exceed the server's 512 MiB response budget; lower "
        "stochastic_traces or stochastic_max_steps");
}

#include "active_source_route.hpp"
#include "mine_limits.hpp"
#include "mine_progress.hpp"
#include "active_source_options.hpp"
// The order in which pm4py's DataFrame log skeleton and Declare meet and
// list the cases of an XES input (read_xes keeps the file order; these group
// the frame by case id), for the data they run on: the log's traces, or a
// flat shape's groups by case_id_key. Null for other inputs, whose data
// already follows the reference (a formatted CSV frame is sorted by case id;
// a native log keeps its order), and for an event stream, which pm4py groups
// in first-appearance order.
std::optional<std::vector<std::size_t>> constraint_case_order(
    const minera::ActiveEventSource& source, const minera::EventDataView& data,
    std::string_view activity_key, std::string_view case_id_key) {
  if (source.file().format != "xes" || data.kind() == minera::EventDataKind::kEventStream)
    return std::nullopt;
  std::vector<std::size_t> order;
  try {
    minera::LogSkeletonOptions keys;
    keys.activity_key = std::string(activity_key);
    keys.case_id_key = std::string(case_id_key);
    order = minera::log_skeleton_case_order(data, keys);
  } catch (const std::exception&) {
    return std::nullopt;  // the constraint call itself reports the selector error
  }
  bool identity = true;
  for (std::size_t i = 0; i < order.size() && identity; ++i) identity = order[i] == i;
  if (identity) return std::nullopt;
  return order;
}

std::optional<std::vector<std::size_t>> constraint_case_order(
    const minera::ActiveEventSource& source, const minera::EventLog& log) {
  return constraint_case_order(source, minera::EventDataView(log), "concept:name",
                               "case:concept:name");
}

// The order of the per-case token-replay and alignment lists of a session's
// log (pm4py's frame diagnostics list an XES frame's cases in case-id order);
// null for other sources and for a source mined as an event stream.
std::optional<std::vector<std::size_t>> session_case_order(
    const minera::ActiveEventSource* source, const minera::EventLog& log) {
  if (source == nullptr || source->selected_kind() == minera::EventDataKind::kEventStream)
    return std::nullopt;
  return constraint_case_order(*source, log);
}

// The results in that order (unchanged when the order is null or does not
// list every result).
template <class Result>
std::vector<Result> in_constraint_order(const std::vector<Result>& results,
                                        const std::optional<std::vector<std::size_t>>& order) {
  if (!order || order->size() != results.size()) return results;
  std::vector<Result> ordered;
  ordered.reserve(results.size());
  for (const auto index : *order) ordered.push_back(results.at(index));
  return ordered;
}

#include "active_source_mining.hpp"
#include "traces_route.hpp"

void handle_mine(const httplib::Request& request, httplib::Response& response) {
  // A multipart mine reads its controls from form fields only, so a query
  // string next to the upload would be silently ignored; reject it. (The
  // exact-control route moves its query controls into fields itself.)
  const bool audit_fields = request.is_multipart_form_data() &&
                            !minera::exact_filter_discovery_recording();
  if (audit_fields && !request.params.empty()) {
    response.status = 400;
    response.set_content(
        error_json("query-string parameter " + request.params.begin()->first +
                   " is not accepted with a multipart /api/mine upload; send it as a form field"),
        "application/json");
    return;
  }
  FormFieldAuditScope field_audit;
  // The running mine's ticket, so a stage stopped by its cancel answers as a
  // cancel between stages does.
  std::shared_ptr<ActiveMiningTicket> running_ticket;
  // However the request ends, a later cancel-mine for it can undo only a
  // published session (handle_cancel_mine).
  struct TicketEnd {
    const std::shared_ptr<ActiveMiningTicket>& ticket;
    ~TicketEnd() {
      if (!ticket) return;
      std::lock_guard<std::mutex> lock(g_session_mutex);
      ticket->ended = true;
    }
  } ticket_end{running_ticket};
  try {
    // include_traces=false leaves the "traces" array out of the analysis (a
    // "trace_summary" takes its place) and GET /api/traces serves it on
    // demand; absent or true keeps the complete response.
    const bool include_traces = !active_source_supplied(request, "include_traces") || [&] {
      const auto value = active_source_field(request, "include_traces");
      if (value != "true" && value != "false")
        throw std::invalid_argument("include_traces expects true or false");
      return value == "true";
    }();
    const minera::OmitAnalysisTraces omit_traces(!include_traces);
    const auto ticket = begin_active_mining(request);
    running_ticket = ticket;
    // GET /api/mine-progress reports these stages (kMineStages in all; the
    // native-source dispatch reports its own families instead).
    constexpr std::size_t kMineStages = 26;
    MineProgress progress(ticket->id, kMineStages, "Reading the event log");
    // The upload is read in place: a copy would hold the file twice.
    const std::string* csv = &request.body;
    std::string dataset = "event log";
    minera::MineParams params;

    if (request.is_multipart_form_data()) {
      if (!request.has_file("log")) {
        response.status = 400;
        response.set_content(error_json("missing form field: log"),
                             "application/json");
        return;
      }
      audit_form_field("log");
      // get_file_value returns a copy; the entry itself outlives this block.
      const auto& file = request.files.find("log")->second;
      csv = &file.content;
      if (!file.filename.empty()) dataset = file.filename;

      params = minera::parse_mine_params(
          [&](std::string_view name) {
            return form_field(request, std::string(name));
          },
          [&](std::string_view name) {
            audit_form_field(name);
            return request.has_file(std::string(name));
          });
    } else {
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

    const auto exact_heuristic_options=params.heuristic;
    std::string paired_csv_import_error;
    // An unknown field is refused before the log is read: the controls the
    // source preparation and the steps below consult are consulted here
    // without effect first. (The audit after them stays.)
    if (audit_fields) {
      for (const char* name : {"mining_input_format", "mining_input_shape", "mining_selectors_json"}) audit_form_field(name);
      auto probe = params;
      overlay_active_source_selectors(probe, minera::ActiveSourceSelectors{},
          [&](std::string_view key) {return active_source_supplied(request, key);});
      (void)minera::exact_heuristic_options_requested([&](std::string_view key) {return active_source_supplied(request, key);});
      if (const auto unknown = field_audit.unconsulted(request))
        throw std::invalid_argument("unknown or inapplicable /api/mine form field: " + *unknown);
    }
    check_mine_size_limit(request, *csv, dataset, params);
    // A cancel-mine (or a newer source request, or a disconnect) stops the
    // read of the upload part-way (source_read_cancel.hpp).
    const auto read_cancelled = mining_cancel_hook(ticket, connection_cancel_hook(request));
    const SourceReadCancelScope read_cancel(read_cancelled);
    auto active_source = std::make_shared<const minera::ActiveEventSource>(
        prepare_active_event_source(request, *csv, dataset, params, &paired_csv_import_error, true));
    // A canceled or superseded request stops between stages with the same
    // answer publish_active_mining gives; the stages themselves are unchanged.
    // A client that disconnected stops there too, and inside the decorations.
    // The long stages poll `cancelled`: a disconnect, or a cancel-mine (or a
    // newer source request) for this mine, stops them part-way.
    const auto disconnected = connection_cancel_hook(request);
    const auto cancelled = mining_cancel_hook(ticket, disconnected);
    // Between stages; `next` is the stage that follows.
    const auto checkpoint = [&](std::string_view next) {
      abort_if_mining_canceled(ticket);
      minera::throw_if_cancelled(disconnected, "mining request was cancelled: the client disconnected");
      mine_progress_stage(next);
    };
    checkpoint("Directly-follows graph");
    if(minera::exact_heuristic_options_requested([&](std::string_view key){return active_source_supplied(request,key);}))params.heuristic=exact_heuristic_options;
    minera::record_exact_effective_options(params);
    const auto literal_source_selectors = active_source_supplied(request, "mining_selectors_json")
        ? minera::parse_workflow_selector_strings_json(active_source_field(request, "mining_selectors_json"))
        : std::map<std::string, std::string>{};
    // Every control has been read by now; a field nothing consulted is
    // unknown (or inapplicable) and would otherwise be dropped silently.
    if (audit_fields) {
      if (const auto unknown = field_audit.unconsulted(request))
        throw std::invalid_argument("unknown or inapplicable /api/mine form field: " + *unknown);
    }
    if (active_source->file().format == "table" ||
        active_source->selected_kind() != minera::EventDataKind::kEventLog || !literal_source_selectors.empty() || minera::exact_filter_discovery_recording()) {
      ticket->native_source_dispatch = true;
      mine_progress_total(0);
      handle_active_source_mining(request, response, dataset, params, active_source, ticket, paired_csv_import_error);
      return;
    }
    const bool xes_input = active_source->file().format == "xes";
    // The checked log lives in active_source, which the session keeps; it is
    // read in place rather than copied.
    const auto& log = active_source->checked_log();
    std::optional<minera::CsvEventData> csv_data;
    if (!xes_input && !params.correlation_variants.empty())
      csv_data = minera::CsvEventData{active_source->original_log(), active_source->original_table()};
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
          dfg_statistics_for_source(params.dfg_statistics,
                                    active_source.get()));
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
    checkpoint("Temporal profile and transition system");
    // Quadratic in the trace length: one long case can keep it busy for
    // minutes, so a disconnect (or a cancel) stops it.
    const auto temporal_profile =
        minera::discover_temporal_profile(log, params.temporal_profile, cancelled);
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
    checkpoint("Inductive miner");
    // A preparation that changed nothing is dropped and the checked log
    // shared, so the session keeps one copy instead of three.
    auto inductive_prepared =
        minera::prepare_inductive_event_log(log, params.inductive);
    const bool inductive_is_log = prepared_log_is_identity(inductive_prepared, log);
    if (inductive_is_log) inductive_prepared = minera::EventLog{};
    const minera::EventLog& inductive_log = inductive_is_log ? log : inductive_prepared;
    // Its activity-concurrency fall-through tries every activity of a sublog
    // in turn (minutes on logs with hundreds of activities), so a disconnect
    // stops it.
    auto inductive_discovery = params.inductive;
    inductive_discovery.cancelled = cancelled;
    const auto tree = minera::discover_process_tree(log, inductive_discovery);
    checkpoint("Alpha miner");
    for (const auto& activity : tree.activities()) {
      graph.ensure_activity(activity);
    }
    const auto inductive_graph =
        minera::discover_directly_follows_graph(inductive_log);
    const auto net = minera::discover_petri_net(log, params.alpha);
    checkpoint("Heuristics miner");
    auto alpha_prepared =
        minera::normalize_alpha_event_log(log, params.alpha);
    const bool alpha_is_log = prepared_log_is_identity(alpha_prepared, log);
    if (alpha_is_log) alpha_prepared = minera::EventLog{};
    const minera::EventLog& alpha_log = alpha_is_log ? log : alpha_prepared;
    register_activity_selector_values(graph, log,
                                      params.alpha.activity_attribute);
    for (const auto& activity : net.activities())
      graph.ensure_activity(activity);
    const auto heuristics = minera::discover_heuristics_net(log, params.heuristic);
    checkpoint("POWL miner");
    // Its fall-throughs try every activity of a sublog in turn (minutes on
    // logs with hundreds of activities), so a disconnect stops it.
    const auto powl = minera::discover_powl(log, params.powl, cancelled);
    checkpoint("Model footprints");
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
    checkpoint("Performance and resources");
    const auto performance = minera::compute_performance_metrics(
        log, graph, dataframe_performance_source(active_source.get()));
    const auto variant_durations =
        minera::compute_variant_durations(log, graph);
    const auto resources = minera::discover_resource_network(log);
    const auto eventually =
        minera::discover_eventually_follows(log, graph, params.eventually_follows);
    std::optional<minera::PerformanceSpectrum> spectrum;
    if (!params.spectrum_activities.empty()) {
      spectrum = discover_selected_performance_spectrum(
          log, params.spectrum_activities, params.spectrum);
    }
    std::optional<minera::BatchDiscovery> batch_discovery;
    if (params.batches) {
      batch_discovery = discover_selected_batches(log, params.batch_options);
    }
    checkpoint("Log skeleton");
    // pm4py's DataFrame log skeleton and Declare meet an XES frame's cases in
    // case-id order and list their conformance in it (a CSV log is already
    // in that order); the native results stay indexed by trace.
    const auto constraint_order = constraint_case_order(*active_source, log);
    const auto log_skeleton = constraint_order
        ? minera::discover_log_skeleton(log, params.log_skeleton, *constraint_order)
        : minera::discover_log_skeleton(log, params.log_skeleton);
    const auto log_skeleton_conformance = minera::check_log_skeleton_conformance(
        log, log_skeleton, params.log_skeleton);
    checkpoint("Declare constraints");
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

    checkpoint("Overlap analysis");
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
      replay_options.cancelled = cancelled;
      const std::string net_name = is_inductive ? "Inductive net" : is_alpha ? "Alpha net"
                                   : is_powl ? "POWL net" : "Heuristics net";
      checkpoint(net_name + ": reachability");
      // In the order the braced initializer evaluated them.
      const auto reachability = minera::final_marking_reachability(petri, cancelled);
      mine_progress_stage(net_name + ": token replay");
      auto token_replay = minera::check_conformance(*conformance_log, petri, replay_options);
      // pm4py lists the frame's cases in the case-id order of the replayed
      // log (its own case key; see session_case_order).
      minera::order_trace_diagnostics(token_replay,
          session_case_order(active_source.get(), *conformance_log));
      mine_progress_stage(net_name + ": footprints");
      // The model's marking exploration can take seconds; a disconnect (or a
      // cancel) stops it.
      auto model_footprints = minera::check_footprints(
          is_powl ? powl_log
          : is_inductive ? inductive_log
          : is_alpha ? alpha_log
                     : log,
          petri, std::vector<int>{}, cancelled);
      minera::ModelConformance result{std::move(id), true, std::move(token_replay),
                                      std::move(model_footprints), std::nullopt};
      minera::set_model_soundness(result, reachability);
      return result;
    };
    const std::vector<minera::ModelConformance> conformance = {
        model("inductive", inductive_net), model("alpha", net),
        model("heuristic", heuristic_net), model("powl", powl_net)};

    // Frequency/performance annotations for every miner's Petri-net view:
    // one more token replay per net, cheap enough to compute up front.
    checkpoint("Model decorations");
    const auto petri_decoration = minera::decorate_model(
        alpha_log, net, "concept:name", "time:timestamp", cancelled);
    const auto inductive_decoration = minera::decorate_model(
        inductive_log, inductive_net, "concept:name", "time:timestamp", cancelled);
    const auto heuristic_decoration = minera::decorate_model(
        log, heuristic_net, params.heuristic.activity_attribute,
        params.heuristic.timestamp_attribute, cancelled);
    const auto powl_decoration = minera::decorate_model(
        powl_log, powl_net, params.powl.activity_key, params.powl.timestamp_key, cancelled);

    checkpoint("Writing the response");
    const minera::AnalysisConstraintCaseOrder constraint_list_order(
        constraint_order ? &*constraint_order : nullptr);
    std::string body = minera::analysis_json(
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
    session->active_source = active_source;
    session->paired_csv_import_error = paired_csv_import_error;
    // The response body is complete. The checked log stays in active_source
    // and the session points at it; a prepared log that differs from it
    // moves into the session.
    session->log_view = &log;
    if (inductive_is_log) session->inductive_log_view = &log;
    else session->inductive_log = std::move(inductive_prepared);
    if (alpha_is_log) session->alpha_log_view = &log;
    else session->alpha_log = std::move(alpha_prepared);
    session->graph = graph;
    session->nets.emplace("inductive", inductive_net);
    session->nets.emplace("alpha", net);
    session->nets.emplace("heuristic", heuristic_net);
    session->nets.emplace("powl", powl_net);
    session->tree = tree;
    session->powl = powl;
    session->declare = declare;
    session->dataset = dataset;
    session->token_replay = minera::tools::default_replay_input_options(
        session_log(*session), params.token_replay, params.token_replay_activity_attribute_supplied);
    session->inductive_options = params.inductive;
    session->alpha_activity_attribute = params.alpha.activity_attribute;
    session->alpha_case_id_attribute = params.alpha.case_id_attribute;
    session->heuristic_activity_attribute = params.heuristic.activity_attribute;
    session->heuristic_case_id_attribute = params.heuristic.case_id_attribute;
    session->powl_activity_key = params.powl.activity_key;
    session->powl_timestamp_key = params.powl.timestamp_key;
    session->powl_case_id_key = params.powl.case_id_key;
    const auto body_end = body.find_last_not_of(" \t\r\n");
    if (body_end == std::string::npos || body[body_end] != '}')
      throw std::logic_error("mining response is not a JSON object");
    const auto bound_tail = ",\"source_id\":" +
        minera::embedding_runtime::detail::escape_json(std::to_string(session->query_source_id)) +
        ",\"active_source\":" + active_source_snapshot_json(*session, false) +
        (params.filter.sampling
             ? ",\"sampling\":" +
                   minera::sampling_options_json(*params.filter.sampling)
             : std::string{}) +
        '}';
    // The tail replaces the closing brace in place when the buffer has room
    // (the stream's spare capacity usually does), with no second body copy;
    // otherwise one exact-size copy, never a doubled buffer.
    if (body.capacity() >= body.size() - 1 + bound_tail.size()) {
      body.replace(body_end, 1, bound_tail);
    } else {
      std::string bound_body;
      bound_body.reserve(body.size() - 1 + bound_tail.size());
      bound_body.append(body, 0, body_end);
      bound_body += bound_tail;
      bound_body.append(body, body_end + 1);
      body = std::move(bound_body);
    }
    publish_active_mining(ticket, session);

    response.set_content(std::move(body), "application/json");
  } catch (const MineLimitError& error) {
    response.status = 413;
    response.set_content(error.body, "application/json");
  } catch (const minera::OperationCancelled& error) {
    // A stage stopped part-way by this mine's cancel answers as the stage
    // boundary would have (abort_if_mining_canceled).
    response.status = 400;
    response.set_content(error_json(mining_ticket_canceled(running_ticket)
                                        ? "mining request was canceled or replaced by a newer source request"
                                        : error.what()),
                         "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

// Model-export and alignment routes default to the inductive model, as
// /api/bpmn and the CLI do; an explicit empty or unknown miner is still
// rejected by the caller's lookup.
std::string requested_miner(const httplib::Request& request) {
  return request.has_param("miner") ? request.get_param_value("miner")
                                    : std::string("inductive");
}

double ocel_query_number(const httplib::Request& request,
                         std::string_view name, double fallback) {
  const auto key = std::string(name);
  if (!request.has_param(key)) return fallback;
  const auto value = request.get_param_value(key);
  char* end = nullptr;
  // strtod accepts an empty or space-led text as 0 / the trailing number;
  // a supplied control must be the complete number.
  const auto parsed = std::strtod(value.c_str(), &end);
  if (value.empty() || std::isspace(static_cast<unsigned char>(value.front())) ||
      end != value.c_str() + value.size() || !std::isfinite(parsed)) {
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
  if (end == value.c_str() || end != value.c_str() + value.size() ||
      std::isspace(static_cast<unsigned char>(value.front())))
    throw std::invalid_argument(key + ": expected a number");
  return parsed;
}

std::size_t ocel_query_size(const httplib::Request& request,
                            std::string_view name, std::size_t fallback) {
  if (!request.has_param(std::string(name))) return fallback;
  const auto value = request.get_param_value(std::string(name));
  // The complete text must be decimal digits: no sign, space or suffix.
  std::size_t parsed = 0;
  const auto result =
      std::from_chars(value.data(), value.data() + value.size(), parsed);
  if (value.empty() || result.ec != std::errc{} ||
      result.ptr != value.data() + value.size()) {
    throw std::invalid_argument(std::string(name) + ": expected a non-negative integer");
  }
  return parsed;
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
    if (value.empty() || std::isspace(static_cast<unsigned char>(value.front())) ||
        end != value.c_str() + value.size() || !std::isfinite(parsed))
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
      if (end != value.c_str() + value.size())
        throw std::invalid_argument("attribute_values has an invalid integer");
      // Declared (review Z1-3): pm4py takes Python ints of any size.
      if (errno == ERANGE)
        throw std::invalid_argument(
            "attribute_values integer " + value +
            " is outside the signed 64-bit range this filter supports");
      result.emplace_back(static_cast<std::int64_t>(parsed));
    } else if (type == "d") {
      char* end = nullptr;
      const auto parsed = std::strtod(value.c_str(), &end);
      if (value.empty() || std::isspace(static_cast<unsigned char>(value.front())) ||
          end != value.c_str() + value.size() || !std::isfinite(parsed))
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

#include "ocel_exact_filter_route.hpp"

void handle_ocel_filter(const httplib::Request& request,
                        httplib::Response& response) {
  try {
    static const std::set<std::string> allowed = {
        "variant", "attribute_key", "attribute_values", "positive", "types",
        "ids", "event_ids", "from", "to", "object_type", "activity",
        "minimum_counts", "level", "object_id", "min_size", "max_size",
        "allowed", "include_open_source", "attribute_values_json", "column_kind",
        "timestamp_key", "from_json", "to_json", "minimum_counts_json",
        "allowed_json", "activity_key", "object_type_key", "temporary_column", "temporary_separator"};
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
    const bool exact_names=variant=="event_attribute_exact" || variant=="object_attribute_exact" ||
        variant=="timestamp_exact" || variant=="object_count_exact" || variant=="activity_type_matching_exact";
    const auto source = exact_names?minera::parse_ocel_json_literal_names(body):minera::parse_ocel_json(body);
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
    if (auto exact = dispatch_ocel_exact_filter(source,request,variant,positive)) {
      filtered=std::move(exact->log);canonical=std::move(exact->variant);
    } else if (variant == "event_attribute") {
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
    const auto output_json=[&](minera::OcelJsonFormat format){return exact_names?
        minera::serialize_ocel_exact_filter_json(filtered,format):minera::serialize_ocel_json(filtered,format);};
    auto payload = std::string("{\"family\":\"OCEL_FILTER\",\"variant\":\"") +
                   canonical + "\",\"log\":" + output_json(minera::OcelJsonFormat::kClassic);
    if (ocel_query_bool(request, "include_open_source", false))
      payload += ",\"open_log\":" + output_json(minera::OcelJsonFormat::kStandard);
    response.set_content(payload + "}", "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

// The OCEL JSON an import returns (classic as "log", standard as
// "open_log"). The browser posts it back to every other OCEL route, so an import is refused unless that JSON passes the OCEL
// parse limits and, with room for the multipart framing, the upload limit.
std::string importable_ocel_json(
    const minera::Ocel& log,
    minera::OcelJsonFormat format = minera::OcelJsonFormat::kClassic) {
  auto json = minera::serialize_ocel_json(log, format);
  constexpr std::size_t kFramingBytes = 1u << 20;
  minera::OcelJsonLimits limits;
  limits.max_bytes = std::min(limits.max_bytes, kMaxRequestPayloadBytes - kFramingBytes);
  if (json.size() > limits.max_bytes)
    throw std::invalid_argument(
        "the imported OCEL is " + std::to_string((json.size() + (1u << 20) - 1) >> 20) +
        " MiB as OCEL JSON, larger than the " + minera::ocel_limit_text(limits.max_bytes) +
        " the other OCEL routes accept (the " + minera::ocel_limit_text(kMaxRequestPayloadBytes) +
        " upload limit less multipart framing)");
  if (const auto violation = minera::ocel_json_limit_violation(json, limits))
    throw std::invalid_argument("the imported OCEL does not fit the other OCEL routes: " + *violation);
  return json;
}

void handle_ocel_sqlite(const httplib::Request& request,
                        httplib::Response& response) {
  // A private directory (mkdtemp) holds the database and its journal; it is
  // removed when this handler returns, and on SIGTERM/SIGINT.
  std::optional<minera::secure_temp::PrivateDirectory> temporary_directory;
  std::filesystem::path temporary;
  try {
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (key != "mode" && key != "include_open_source" &&
          key != "validate_references" && key != "readable_type_table_names")
        throw std::invalid_argument("unknown OCEL SQLite parameter: " + key);
    }
    const auto mode = request.has_param("mode")
                          ? request.get_param_value("mode")
                          : std::string("import");
    if (mode != "import" && mode != "export")
      throw std::invalid_argument("OCEL SQLite mode expects import or export");
    minera::OcelSqliteOptions options;
    options.validate_references = ocel_query_bool(
        request, "validate_references", options.validate_references);
    options.readable_type_table_names = ocel_query_bool(
        request, "readable_type_table_names", options.readable_type_table_names);
    const bool include_open_source = ocel_query_bool(request, "include_open_source", false);
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
    temporary_directory.emplace("minera_ocel_");
    temporary = temporary_directory->file("ocel.sqlite");
    if (mode == "import") {
      std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
      if (!output) throw std::runtime_error("cannot create temporary SQLite upload");
      output.write(body.data(),
                   static_cast<std::streamsize>(body.size()));
      if (!output) throw std::runtime_error("cannot write temporary SQLite upload");
    }
    if (mode == "export") {
      minera::export_ocel_sqlite(minera::parse_ocel_json(body), temporary, options);
      std::ifstream input(temporary, std::ios::binary | std::ios::ate);
      if (!input) throw std::runtime_error("cannot read temporary SQLite output");
      const auto size = input.tellg();
      constexpr auto kMaxSqliteResponseBytes = static_cast<std::streamoff>(kMaxResponseBytes);
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
    const auto log = minera::import_ocel_sqlite(temporary, options);
    std::error_code ignored;
    std::filesystem::remove(temporary, ignored);
    temporary.clear();
    response.set_content(
        std::string("{\"family\":\"OCEL_SQLITE\",\"variant\":\"IMPORT\",\"log\":") +
            importable_ocel_json(log) +
            (include_open_source ? ",\"open_log\":" + importable_ocel_json(
                log, minera::OcelJsonFormat::kStandard) : "") + "}",
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
  std::optional<minera::secure_temp::PrivateDirectory> csv_directory;
  std::filesystem::path csv_temporary;
  const auto cleanup_csv = [&]() {
    csv_directory.reset();
    csv_temporary.clear();
  };
  try {
    static const std::set<std::string> allowed = {
        "mode", "format", "reject_unsupported", "include_declaration",
        "include_open_source", "include_objects", "allow_lossy", "max_bytes",
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
    const bool include_open_source = ocel_query_bool(request, "include_open_source", false);
    const bool include_objects = ocel_query_bool(request, "include_objects", false);
    if (include_objects && (mode != "export" || format != "csv"))
      throw std::invalid_argument("include_objects requires CSV event export");

    const auto body = ocel_request_body(request, "log");
    if (body.empty()) throw std::invalid_argument("missing OCEL exchange body");

    if (mode == "export") {
      const auto log = minera::parse_ocel_json(body);
      std::string payload;
      std::string content_type;
      std::string filename;
      if (format == "json" || format == "json_standard") {
        // A download: without the classic transport markers, so the file
        // reads as the reference's read_ocel_json reads it.
        payload = minera::serialize_ocel_json(
            log, format == "json_standard" ? minera::OcelJsonFormat::kStandard
                                             : minera::OcelJsonFormat::kClassic,
            false);
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
        options.allow_lossy = ocel_query_bool(request, "allow_lossy", options.allow_lossy);
        payload = minera::serialize_ocel_xml(log, options);
        content_type = "application/xml";
        filename = "ocel.xml";
      } else if (format == "csv") {
        if (include_objects) {
          csv_directory.emplace("minera_ocel_csv_");
          csv_temporary = csv_directory->path();
          minera::export_ocel_csv(log, csv_temporary / "events.csv",
                                 csv_temporary / "objects.csv", csv_options.export_options);
          const auto read_csv = [&](const char* name) {
            std::ifstream input(csv_temporary / name, std::ios::binary | std::ios::ate);
            if (!input) throw std::runtime_error("cannot read OCEL CSV output");
            const auto size = input.tellg();
            if (size < 0 || static_cast<std::size_t>(size) > kMaxResponseBytes)
              throw std::invalid_argument("OCEL CSV output exceeds the response limit");
            std::string text(static_cast<std::size_t>(size), '\0');
            input.seekg(0);
            input.read(text.data(), static_cast<std::streamsize>(text.size()));
            if (!input) throw std::runtime_error("cannot read OCEL CSV output");
            return text;
          };
          const auto events = read_csv("events.csv"), objects = read_csv("objects.csv");
          cleanup_csv();
          const auto quote = minera::embedding_runtime::detail::escape_json;
          payload = "{\"family\":\"OCEL_IO_EXPORT\",\"variant\":\"CSV_PAIR\",\"events_csv\":" +
                    quote(events) + ",\"objects_csv\":" + quote(objects) + "}";
          content_type = "application/json";
          filename = "ocel_csv_pair.json";
        } else {
          payload = minera::serialize_ocel_csv(log, csv_options.export_options);
          content_type = "text/csv";
          filename = "ocel.csv";
        }
      } else {
        payload = minera::serialize_ocel_objects_csv(
            log, csv_options.export_options);
        content_type = "text/csv";
        filename = "ocel_objects.csv";
      }
      constexpr std::size_t kMaxExchangeResponseBytes = kMaxResponseBytes;
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
      options.max_bytes = ocel_query_size(request, "max_bytes", options.max_bytes);
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
            variant + "\",\"log\":" + importable_ocel_json(log) +
            (include_open_source ? ",\"open_log\":" + importable_ocel_json(
                log, minera::OcelJsonFormat::kStandard) : "") + "}",
        "application/json");
  } catch (const std::exception& error) {
    cleanup_csv();
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_ocel_discovery(const httplib::Request& request,
                           httplib::Response& response) {
  try {
    static const std::set<std::string> allowed = {
        "variant", "compute_edges_performance", "ocpn_annotation",
        "diagnostics_with_token_based_replay", "ocpn_disable_strict_sequence_cut",
        "ocpn_disable_fallthroughs", "ocpn_activity_attribute",
        "ocpn_object_type_attribute", "ocdfg_activity_attribute",
        "ocdfg_object_type_attribute", "ocdfg_business_hours",
        "ocdfg_business_hour_slots_json", "ocdfg_event_id_attribute",
        "ocdfg_timestamp_attribute", "ocdfg_workcalendar_json", "saw_activity_attribute",
        "saw_object_type_attribute", "include_edge_metrics",
        "ocdfg_edge_metrics_event_id_key", "ocdfg_edge_metrics_object_id_key",
        "ocdfg_edge_metrics_object_type_key", "ocdfg_edge_metrics_activity_key",
        "ocdfg_edge_metrics_timestamp_key"};
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
    const auto include_edge_metrics = ocel_query_bool(request, "include_edge_metrics", false);
    if (include_edge_metrics && variant != "ocdfg")
      throw std::invalid_argument("include_edge_metrics requires the ocdfg variant");
    const auto body = ocel_request_body(request, "log");
    if (body.empty()) throw std::invalid_argument("missing OCEL JSON body");
    const auto log = include_edge_metrics ? minera::parse_ocel_json_literal_names(body)
                                         : minera::parse_ocel_json(body);
    std::string payload;
    if (variant == "etot") {
      payload = minera::etot_json(minera::discover_etot_classic(log));
    } else if (variant == "otg") {
      payload = minera::otg_json(minera::discover_otg_classic(log));
    } else if (variant == "ocpn") {
      minera::OcpnDiscoveryOptions discovery;
      // Omitted flags keep the native defaults (true), as the reference
      // OCPN discovery mines without fall-throughs or the strict sequence cut.
      discovery.disable_strict_sequence_cut = ocel_query_bool(
          request, "ocpn_disable_strict_sequence_cut",
          discovery.disable_strict_sequence_cut);
      discovery.disable_fallthroughs = ocel_query_bool(
          request, "ocpn_disable_fallthroughs", discovery.disable_fallthroughs);
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
      options.edge_performance_options =
          ocdfg_performance_options_from_request(request);
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
    auto envelope = ocel_discovery_response(variant, std::move(payload));
    if (include_edge_metrics) {
      envelope.pop_back();
      envelope += ",\"edge_metrics\":" + ocdfg_edge_metrics_report_from_request(log, request) + '}';
    }
    response.set_content(envelope, "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_ocel_split(const httplib::Request& request,
                       httplib::Response& response) {
  try {
    static const std::set<std::string> allowed = {
        "variant", "object_type", "max_objects", "max_degree_centrality",
        "include_open_source"};
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
    auto payload = minera::serialize_ocel_split_json(result);
    if (ocel_query_bool(request, "include_open_source", false)) {
      payload.pop_back();
      payload += ",\"open_logs\":[";
      for (std::size_t i = 0; i < result.logs.size(); ++i) {
        if (i) payload += ',';
        payload += minera::serialize_ocel_json(result.logs[i],
                                               minera::OcelJsonFormat::kStandard);
      }
      payload += "]}";
    }
    response.set_content(payload, "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_ocel_enrichment(const httplib::Request& request,
                            httplib::Response& response) {
  try {
    static const std::set<std::string> allowed = {
        "graphs", "lifecycle_qualifiers", "include_open_source"};
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
        if (value == "none" && values.size() == 1) {
          continue;
        } else if (value == "all") {
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
              "graphs expects interaction, descendants, inheritance, cobirth, codeath, all, or none alone");
        }
      }
    }
    options.include_lifecycle_qualifiers = ocel_query_bool(
        request, "lifecycle_qualifiers", options.include_lifecycle_qualifiers);
    const auto body = ocel_request_body(request, "log");
    if (body.empty()) throw std::invalid_argument("missing OCEL JSON body");
    const auto enriched = minera::enrich_ocel(minera::parse_ocel_json(body), options);
    auto payload = minera::serialize_ocel_enrichment_json(enriched, options);
    if (ocel_query_bool(request, "include_open_source", false)) {
      payload.pop_back();
      payload += ",\"open_log\":" + minera::serialize_ocel_json(
          enriched, minera::OcelJsonFormat::kStandard) + "}";
    }
    response.set_content(payload, "application/json");
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
        "ocpn_disable_strict_sequence_cut", "ocpn_disable_fallthroughs",
        "ocpn_activity_attribute", "ocpn_object_type_attribute",
        "include_open_source", "include_model"};
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
      } else if (binding == "reference") {
        options.event_binding = minera::OcpnEventBinding::kReference;
      } else {
        throw std::invalid_argument(
            "event_binding expects independent, shared_activity, synchronized, or reference");
      }
    } else {
      // Without an explicit binding, run the reference playout unless the
      // request uses a control that only the native bindings understand.
      static const std::vector<std::string> native_only = {
          "branching_factor_transitions", "branching_factor_bindings",
          "branching_factor_activities", "max_states", "objects", "initial_marking",
          "final_marking", "seed", "start", "case_interval", "event_interval"};
      bool native = std::any_of(native_only.begin(), native_only.end(),
                                [&](const auto& key) { return request.has_param(key); });
      if (model == "causal_net")
        native = native || options.exists_trace || options.return_sequences ||
                 options.return_traces;
      if (!native) {
        options.event_binding = minera::OcpnEventBinding::kReference;
        if (!request.has_param("max_bindings_per_activity"))
          options.max_bindings_per_activity = 1;
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
    discovery.disable_strict_sequence_cut = ocel_query_bool(
        request, "ocpn_disable_strict_sequence_cut",
        discovery.disable_strict_sequence_cut);
    discovery.disable_fallthroughs = ocel_query_bool(
        request, "ocpn_disable_fallthroughs", discovery.disable_fallthroughs);
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
    const bool include_open_source = ocel_query_bool(request, "include_open_source", false);
    const bool include_model = ocel_query_bool(request, "include_model", false);
    const auto publish = [&](std::string payload, const minera::Ocel& generated,
                             const std::string& discovered_model) {
      if (include_open_source || include_model) {
        payload.pop_back();
        if (include_open_source)
          payload += ",\"open_log\":" + minera::serialize_ocel_json(
              generated, minera::OcelJsonFormat::kStandard);
        if (include_model) payload += ",\"model\":" + discovered_model;
        payload += '}';
      }
      response.set_content(payload, "application/json");
    };
    if (model == "causal_net") {
      // The reference binding plays out the causal net the reference converts
      // from the mined OCPN (START_/END_<type> included); the native bindings
      // keep the log-context marker model.
      const auto causal =
          options.event_binding == minera::OcpnEventBinding::kReference
              ? minera::oc_causal_net_from_ocpn(minera::discover_ocpn(log, discovery))
              : minera::discover_oc_causal_net(log, discovery);
      const auto result = minera::oc_causal_net_extensive_playout_result(causal,
                                                                           options);
      publish(minera::serialize_oc_causal_net_playout_json(result, options.event_binding),
              result.log, include_model ? minera::serialize_oc_causal_net_json(causal) : "");
    } else {
      const auto graph = minera::discover_ocpn(log, discovery);
      const auto result = minera::ocpn_extensive_playout_result(graph, options);
      publish(minera::serialize_ocpn_playout_json(result, options.event_binding),
              result.log, include_model ? minera::serialize_ocpn_json(graph) : "");
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

#include "otg_threshold_route.hpp"

// Checks the most recently mined log against a user-supplied model â€” a PNML
// workflow net, a BPMN 2.0 process, or a PTML process tree, told apart by the
// document's root element (multipart field `model`, or the raw body). The imported net joins
// the cached session under the id "imported", so POST /api/align?miner=imported
// can align against it on demand like any discovered model; the session's
// registry is extended with the net's activities so ids stay resolvable.
bool discovery_model_requested(const httplib::Request& request,
                               const MinedSession& session) {
  if (request.has_param("source_id") && request.get_param_value("source_id") !=
      std::to_string(session.query_source_id))
    throw std::invalid_argument("the source log changed; refresh the discovery source");
  if (!request.has_param("include_pnml")) return false;
  const auto value = request.get_param_value("include_pnml");
  if (value != "true" && value != "false")
    throw std::invalid_argument("include_pnml must be true or false");
  if (value == "false") return false;
  if (!request.has_param("source_id"))
    throw std::invalid_argument("source_id is required for a discovery model");
  return true;
}

std::string discovery_model_json(std::string result_json,
                                 const minera::PetriNet& net,
                                 const minera::WorkflowMarkings& markings,
                                 const MinedSession& session,
                                 bool computed) {
  const auto quote = minera::embedding_runtime::detail::escape_json;
  return "{\"family\":\"DISCOVERY_MODEL\",\"source_id\":" +
         quote(std::to_string(session.query_source_id)) + ",\"result\":" +
         result_json + ",\"pnml\":" + (computed
             ? quote(minera::to_pnml(net, markings.initial, markings.final, "discovered"))
             : std::string("null")) + '}';
}

// Independent statistical and causal DFG discovery on the cached source.
// include_traces=false leaves the "traces" array out of a discovery route's
// analysis result; absent or true keeps the complete result.
bool analysis_include_traces(const httplib::Request& request) {
  if (!request.has_param("include_traces")) return true;
  const auto value = request.get_param_value("include_traces");
  if (value == "true") return true;
  if (value == "false") return false;
  throw std::invalid_argument("include_traces expects true or false");
}

void handle_dfg_discovery(const httplib::Request& request,
                          httplib::Response& response) {
  try {
    std::shared_ptr<const MinedSession> session;
    { std::lock_guard<std::mutex> lock(g_session_mutex); session = g_session; }
    if (!session) { response.status = 409; response.set_content(error_json("no mined log; mine a log first"), "application/json"); return; }
    if (!request.has_param("source_id") || request.get_param_value("source_id") != std::to_string(session->query_source_id))
      throw std::invalid_argument("the source log changed; refresh the DFG discovery source");
    static const std::set<std::string> allowed = {"source_id", "dfg_statistical_variants", "dfg_causal_variants", "dfg_window", "dfg_keep_once_per_case", "dfg_activity_attribute", "dfg_timestamp_attribute", "dfg_start_timestamp_attribute", "dfg_performance_aggregation", "dfg_business_hours", "dfg_business_hour_slots", "dfg_case_attributes", "dfg_case_attributes_json", "dfg_return_node_attributes", "include_traces"};
    for (const auto& [key, value] : request.params) { (void)value; if (!allowed.contains(key)) throw std::invalid_argument("unknown DFG discovery parameter: " + key); }
    const auto lookup = [&](std::string_view name) { return request.has_param(std::string(name)) ? request.get_param_value(std::string(name)) : std::string{}; };
    const auto present = [&](std::string_view name) { return request.has_param(std::string(name)); };
    const auto params = minera::parse_mine_params(lookup, present);
    if (params.dfg_statistical_variants.empty() && params.dfg_causal_variants.empty()) throw std::invalid_argument("select at least one statistical or causal DFG variant");
    auto graph = minera::discover_directly_follows_graph(require_prepared_log(*session));
    register_activity_selector_values(graph, require_prepared_log(*session), params.dfg_statistics.activity_attribute);
    const auto variants = analyze_dfg_variants(require_prepared_log(*session), params.dfg_statistical_variants, params.dfg_causal_variants, dfg_statistics_for_source(params.dfg_statistics, session->active_source.get()));
    std::optional<minera::OmitAnalysisTraces> omit_traces(std::in_place, !analysis_include_traces(request));
    const auto result = minera::analysis_json(
      /* dataset */ session->dataset,
      /* event_log */ require_prepared_log(*session),
      /* graph */ graph,
      /* tree */ nullptr,
      /* net */ nullptr,
      /* heuristics */ nullptr,
      /* conformance */ nullptr,
      /* performance */ nullptr,
      /* resources */ nullptr,
      /* variant_durations */ nullptr,
      /* eventually_follows */ nullptr,
      /* decisions */ nullptr,
      /* petri_decoration */ nullptr,
      /* spectrum */ nullptr,
      /* inductive_net */ nullptr,
      /* inductive_decoration */ nullptr,
      /* heuristic_petri */ nullptr,
      /* heuristic_decoration */ nullptr,
      /* temporal_profile */ nullptr,
      /* minimum_self_distances */ nullptr,
      /* transition_system */ nullptr,
      /* powl */ nullptr,
      /* powl_petri */ nullptr,
      /* powl_decoration */ nullptr,
      /* log_skeleton */ nullptr,
      /* log_skeleton_conformance */ nullptr,
      /* declare */ nullptr,
      /* declare_conformance */ nullptr,
      /* footprints */ nullptr,
      /* dfg_variants */ &variants,
      /* correlation_mining */ nullptr,
      /* batch_discovery */ nullptr,
      /* dfg_alignment */ nullptr,
      /* edit_distance */ nullptr,
      /* edit_anti */ false,
      /* dfg_projection */ nullptr,
      /* dfg_projected_log */ nullptr,
      /* edit_projected_log */ nullptr,
      /* process_tree_alignment */ nullptr,
      /* decomposed_alignment */ nullptr,
      /* petri_anti_alignment */ nullptr,
      /* petri_multi_alignment */ nullptr,
      /* reduced_process_tree */ nullptr,
      /* reduced_trace_index */ 0,
      /* evaluation */ nullptr,
      /* earth_movers_distance */ nullptr,
      /* interval_event_overlap_counts */ nullptr,
      /* case_overlap_counts */ nullptr,
      /* concurrent_activity_overlap */ nullptr,
      /* decision_tables */ nullptr,
      /* minimum_self_distance_variant */ params.minimum_self_distance.variant,
      /* typed_edit_distance */ nullptr);
    omit_traces.reset();
    const auto quote = minera::embedding_runtime::detail::escape_json;
    const auto list = [&](const std::vector<std::string>& names) { std::string value = "["; for (std::size_t i = 0; i < names.size(); ++i) { if (i) value += ','; value += quote(names[i]); } return value + ']'; };
    response.set_content("{\"family\":\"DFG_DISCOVERY\",\"source_id\":" + quote(std::to_string(session->query_source_id)) + ",\"dataset\":" + quote(session->dataset) +
      ",\"source\":{\"case_count\":" + std::to_string(require_prepared_log(*session).trace_count()) + ",\"event_count\":" + std::to_string(require_prepared_log(*session).event_count()) + "},\"input_origin\":\"cached_source_log\",\"statistical_variants\":" + list(params.dfg_statistical_variants) + ",\"causal_variants\":" + list(params.dfg_causal_variants) + ",\"result\":" + result + '}', "application/json");
  } catch (const std::exception& error) { response.status = 400; response.set_content(error_json(error.what()), "application/json"); }
}

// Source-bound behavioral discovery and temporal profile conformance.
void handle_behavior_analysis(const httplib::Request& request,
                              httplib::Response& response) {
  try {
    std::shared_ptr<const MinedSession> session;
    { std::lock_guard<std::mutex> lock(g_session_mutex); session = g_session; }
    if (!session) { response.status = 409; response.set_content(error_json("no mined log; mine a log first"), "application/json"); return; }
    if (!request.has_param("source_id") || request.get_param_value("source_id") != std::to_string(session->query_source_id))
      throw std::invalid_argument("the source log changed; refresh the behavior source");
    const auto family = request.get_param_value("family");
    const bool temporal = family == "temporal_profile" || family == "temporal_conformance";
    const bool conform = family == "temporal_conformance";
    if (!temporal && family != "minimum_self_distance" && family != "correlation")
      throw std::invalid_argument("unsupported behavior analysis family");
    std::set<std::string> allowed = {"family", "source_id", "variant", "include_traces"};
    if (temporal) {
      allowed.insert({"temporal_activity_attribute", "temporal_case_id_attribute", "temporal_source_timestamp_attribute", "temporal_target_timestamp_attribute", "temporal_business_hours", "temporal_business_hour_slots", "temporal_strict_attributes"});
      if (conform) allowed.insert({"temporal_profile_json", "temporal_zeta"});
    } else if (family == "minimum_self_distance")
      allowed.insert({"minimum_self_distance_activity_key", "minimum_self_distance_case_id_key"});
    else
      allowed.insert({"adapter", "correlation_activity_attribute", "correlation_timestamp_attribute", "correlation_start_timestamp_attribute", "correlation_case_id_attribute", "correlation_exact_time_matching", "correlation_sample_size"});
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key)) throw std::invalid_argument("unknown behavior analysis parameter: " + key);
    }
    analysis_include_traces(request);  // rejects an invalid value before any work
    auto variant = request.has_param("variant") ? request.get_param_value("variant") : (family == "correlation" ? "classic" : "log");
    if (family == "correlation" && variant != "classic" && variant != "classic_split" && variant != "trace_based")
      throw std::invalid_argument("correlation variant expects classic, classic_split, or trace_based");
    const auto lookup = [&](std::string_view name) {
      if (name == "minimum_self_distance_variant" && family == "minimum_self_distance") return variant;
      if (name == "correlation_variants" && family == "correlation") return variant;
      return request.has_param(std::string(name)) ? request.get_param_value(std::string(name)) : std::string{};
    };
    const auto present = [&](std::string_view name) {
      return (name == "minimum_self_distance_variant" && family == "minimum_self_distance") || (name == "correlation_variants" && family == "correlation") || request.has_param(std::string(name));
    };
    auto params = minera::parse_mine_params(lookup, present);
    if (temporal && !present("temporal_strict_attributes")) params.temporal_profile.strict_attributes = true;
    std::string adapter = family == "correlation" ? (request.has_param("adapter") ? request.get_param_value("adapter") : "dataframe") : variant;
    if (family == "minimum_self_distance") {
      variant = params.minimum_self_distance.variant == minera::MinimumSelfDistanceVariant::kLog ? "log" : params.minimum_self_distance.variant == minera::MinimumSelfDistanceVariant::kPandas ? "pandas" : "polars";
      adapter = params.minimum_self_distance.variant == minera::MinimumSelfDistanceVariant::kLog ? "log" : params.minimum_self_distance.variant == minera::MinimumSelfDistanceVariant::kPandas ? "dataframe" : "lazyframe";
    } else if (adapter != "log" && adapter != "stream" && adapter != "dataframe" && adapter != "lazyframe")
      throw std::invalid_argument("behavior adapter expects log, stream, dataframe, or lazyframe");
    overlay_cached_source_selectors(request, *session, params, adapter != "log");
    const auto input = [&]() -> minera::EventDataView {
      const auto kind = adapter == "log" ? minera::EventDataKind::kEventLog :
          adapter == "stream" ? minera::EventDataKind::kEventStream :
          adapter == "dataframe" ? minera::EventDataKind::kEventTable : minera::EventDataKind::kLazyEventTable;
      return cached_source_view(*session,kind);
    };
    auto graph = session->log_ready ? minera::discover_directly_follows_graph(require_prepared_log(*session)) : minera::DirectlyFollowsGraph{};
    std::optional<minera::TemporalProfile> profile;
    std::optional<minera::MinimumSelfDistances> distances;
    std::optional<minera::CorrelationVariantsAnalysis> correlations;
    std::optional<minera::TemporalConformanceResult> conformance;
    if (temporal) {
      if (conform) {
        if (!request.has_param("temporal_profile_json")) throw std::invalid_argument("temporal_profile_json is required");
        profile = minera::parse_temporal_profile_json(request.get_param_value("temporal_profile_json"));
        conformance = minera::conform_temporal_profile(input(), *profile, params.temporal_profile);
      } else profile = minera::discover_temporal_profile(input(), params.temporal_profile);
      for (const auto& entry : profile->entries) { graph.ensure_activity(entry.source); graph.ensure_activity(entry.target); }
    } else if (family == "minimum_self_distance") {
      distances = [&]() -> minera::MinimumSelfDistances {
        switch (params.minimum_self_distance.variant) {
          case minera::MinimumSelfDistanceVariant::kLog:
            return minera::MinimumSelfDistanceLogDiscovery::apply(cached_source_view(*session,minera::EventDataKind::kEventLog),params.minimum_self_distance);
          case minera::MinimumSelfDistanceVariant::kPandas:
            return minera::MinimumSelfDistancePandasDiscovery::apply(cached_source_view(*session,minera::EventDataKind::kEventTable),params.minimum_self_distance);
          case minera::MinimumSelfDistanceVariant::kPolars:
            return minera::MinimumSelfDistancePolarsDiscovery::apply(cached_source_view(*session,minera::EventDataKind::kLazyEventTable),params.minimum_self_distance);
        }
        throw std::invalid_argument("minimum self-distance variant is invalid");
      }();
      for (const auto& distance : *distances) {
        if (!distance.activity_is_null) graph.ensure_activity(distance.activity);
        for (const auto& witness : distance.witnesses) graph.ensure_activity(witness);
      }
    } else {
      correlations = analyze_correlation_variants(input(), params.correlation_variants, params.correlation);
      minera::register_correlation_activities(graph, *correlations);
    }
    std::string result = "null", result_error;
    if (session->log_ready) try { const minera::OmitAnalysisTraces omit_traces(!analysis_include_traces(request)); result = minera::analysis_json(
      /* dataset */ session->dataset,
      /* event_log */ require_prepared_log(*session),
      /* graph */ graph,
      /* tree */ nullptr,
      /* net */ nullptr,
      /* heuristics */ nullptr,
      /* conformance */ nullptr,
      /* performance */ nullptr,
      /* resources */ nullptr,
      /* variant_durations */ nullptr,
      /* eventually_follows */ nullptr,
      /* decisions */ nullptr,
      /* petri_decoration */ nullptr,
      /* spectrum */ nullptr,
      /* inductive_net */ nullptr,
      /* inductive_decoration */ nullptr,
      /* heuristic_petri */ nullptr,
      /* heuristic_decoration */ nullptr,
      /* temporal_profile */ profile ? &*profile : nullptr,
      /* minimum_self_distances */ distances ? &*distances : nullptr,
      /* transition_system */ nullptr,
      /* powl */ nullptr,
      /* powl_petri */ nullptr,
      /* powl_decoration */ nullptr,
      /* log_skeleton */ nullptr,
      /* log_skeleton_conformance */ nullptr,
      /* declare */ nullptr,
      /* declare_conformance */ nullptr,
      /* footprints */ nullptr,
      /* dfg_variants */ nullptr,
      /* correlation_mining */ correlations ? &*correlations : nullptr,
      /* batch_discovery */ nullptr,
      /* dfg_alignment */ nullptr,
      /* edit_distance */ nullptr,
      /* edit_anti */ false,
      /* dfg_projection */ nullptr,
      /* dfg_projected_log */ nullptr,
      /* edit_projected_log */ nullptr,
      /* process_tree_alignment */ nullptr,
      /* decomposed_alignment */ nullptr,
      /* petri_anti_alignment */ nullptr,
      /* petri_multi_alignment */ nullptr,
      /* reduced_process_tree */ nullptr,
      /* reduced_trace_index */ 0,
      /* evaluation */ nullptr,
      /* earth_movers_distance */ nullptr,
      /* interval_event_overlap_counts */ nullptr,
      /* case_overlap_counts */ nullptr,
      /* concurrent_activity_overlap */ nullptr,
      /* decision_tables */ nullptr,
      /* minimum_self_distance_variant */ params.minimum_self_distance.variant,
      /* typed_edit_distance */ nullptr); } catch (const std::exception& error) { result_error = error.what(); }
    const auto quote = minera::embedding_runtime::detail::escape_json;
    auto body = "{\"family\":\"BEHAVIOR_ANALYSIS\",\"method\":" + quote(family) + ",\"variant\":" + quote(variant) + ",\"adapter\":" + quote(adapter) + ",\"source_id\":" + quote(std::to_string(session->query_source_id)) +
      ",\"dataset\":" + quote(session->dataset) + ",\"source\":{\"case_count\":" + (session->log_ready ? std::to_string(require_prepared_log(*session).trace_count()) : std::string("null")) + ",\"event_count\":" + (session->active_source && session->active_source->checked_table_if_available() ? std::to_string(session->active_source->checked_table().rows.size()) : session->log_ready ? std::to_string(require_prepared_log(*session).event_count()) : std::string("null")) + "},\"input_origin\":\"cached_source_log_conversion\",\"result\":" + result;
    std::string native_result = "null";
    if (profile) native_result = minera::serialize_active_source_temporal_profile_json(*profile);
    if (conformance) native_result = minera::serialize_active_source_temporal_conformance_json(*conformance);
    if (distances) native_result = minera::serialize_active_source_distances_json(*distances);
    if (correlations) native_result = minera::serialize_active_source_correlation_json(*correlations);
    body += ",\"native_result\":" + native_result + ",\"result_error\":" + (result_error.empty() ? "null" : quote(result_error));
    if (profile) body += ",\"profile\":" + minera::temporal_profile_json(*profile);
    if (conformance) body += ",\"conformance\":" + minera::temporal_conformance_json(*conformance);
    response.set_content(body + '}', "application/json");
  } catch (const std::exception& error) { response.status = 400; response.set_content(error_json(error.what()), "application/json"); }
}

// Variant-specific discovery runs without replacing the active analysis.
void handle_timing_discovery(const httplib::Request& request,
                             httplib::Response& response) {
  try {
    std::shared_ptr<const MinedSession> session;
    { std::lock_guard<std::mutex> lock(g_session_mutex); session = g_session; }
    if (!session) { response.status = 409; response.set_content(error_json("no mined log; mine a log first"), "application/json"); return; }
    if (!request.has_param("source_id") || request.get_param_value("source_id") != std::to_string(session->query_source_id))
      throw std::invalid_argument("the source log changed; refresh the timing source");
    const auto family = request.get_param_value("family");
    if (family != "spectrum" && family != "batches") throw std::invalid_argument("timing discovery family expects spectrum or batches");
    static const std::set<std::string> spectrum_fields = {"spectrum_activities", "spectrum_variant", "spectrum_sample_size", "spectrum_sort_log", "spectrum_activity_key", "spectrum_timestamp_key", "spectrum_case_id_key"};
    static const std::set<std::string> batch_fields = {"batch_variant", "batch_merge_distance_seconds", "batch_min_size", "batch_start_timestamp_attribute", "batch_activity_attribute", "batch_resource_attribute", "batch_timestamp_attribute", "batch_case_id_attribute", "batch_event_id_attribute"};
    const auto& fields = family == "spectrum" ? spectrum_fields : batch_fields;
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (key != "family" && key != "source_id" && key != "include_traces" && !fields.contains(key)) throw std::invalid_argument("unknown timing discovery parameter: " + key);
    }
    analysis_include_traces(request);  // rejects an invalid value before any work
    const auto lookup = [&](std::string_view name) { if (name == "batches" && family == "batches") return std::string("true"); return request.has_param(std::string(name)) ? request.get_param_value(std::string(name)) : std::string{}; };
    const auto present = [&](std::string_view name) { return (name == "batches" && family == "batches") || request.has_param(std::string(name)); };
    auto params = minera::parse_mine_params(lookup, present);
    const bool flat_timing = family == "spectrum"
        ? params.spectrum.variant == minera::PerformanceSpectrumVariant::kDataframe ||
          params.spectrum.variant == minera::PerformanceSpectrumVariant::kDataframeDisconnected ||
          params.spectrum.variant == minera::PerformanceSpectrumVariant::kLazyframe ||
          params.spectrum.variant == minera::PerformanceSpectrumVariant::kLazyframeDisconnected
        : params.batch_options.variant != minera::BatchDiscoveryVariant::kLog;
    overlay_cached_source_selectors(request, *session, params, flat_timing);
    auto graph = session->log_ready ? minera::discover_directly_follows_graph(require_prepared_log(*session)) : minera::DirectlyFollowsGraph{};
    std::optional<minera::PerformanceSpectrum> spectrum;
    std::optional<minera::BatchDiscovery> batches;
    if (family == "spectrum") {
      if (params.spectrum_activities.empty()) throw std::invalid_argument("spectrum_activities requires at least two activities");
      spectrum = [&]() {
        const auto kind = params.spectrum.variant == minera::PerformanceSpectrumVariant::kDataframe || params.spectrum.variant == minera::PerformanceSpectrumVariant::kDataframeDisconnected
            ? minera::EventDataKind::kEventTable : params.spectrum.variant == minera::PerformanceSpectrumVariant::kLazyframe || params.spectrum.variant == minera::PerformanceSpectrumVariant::kLazyframeDisconnected
            ? minera::EventDataKind::kLazyEventTable : minera::EventDataKind::kEventLog;
        return minera::compute_performance_spectrum(cached_source_view(*session,kind),params.spectrum_activities,params.spectrum);
      }();
      for (const auto& activity : spectrum->activities) graph.ensure_activity(activity);
    } else {
      batches = [&]() -> minera::BatchDiscovery {
        switch (params.batch_options.variant) {
          case minera::BatchDiscoveryVariant::kLog:
            return minera::BatchLogDiscovery::apply(*cached_source_view(*session,minera::EventDataKind::kEventLog).log(),params.batch_options);
          case minera::BatchDiscoveryVariant::kPandas:
            return minera::BatchPandasDiscovery::apply(*cached_source_view(*session,minera::EventDataKind::kEventTable).table(),params.batch_options);
          case minera::BatchDiscoveryVariant::kPolars:
            return minera::BatchPolarsDiscovery::apply(*cached_source_view(*session,minera::EventDataKind::kLazyEventTable).lazy_table(),params.batch_options);
        }
        throw std::invalid_argument("batch discovery variant is invalid");
      }();
      if (session->log_ready) register_activity_selector_values(graph, require_prepared_log(*session), params.batch_options.activity_attribute);
    }
    std::string result = "null", result_error;
    if (session->log_ready) try { const minera::OmitAnalysisTraces omit_traces(!analysis_include_traces(request)); result = minera::analysis_json(session->dataset, require_prepared_log(*session), graph,
      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
      spectrum ? &*spectrum : nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, batches ? &*batches : nullptr); } catch (const std::exception& error) { result_error = error.what(); }
    const auto quote = minera::embedding_runtime::detail::escape_json;
    std::string variant;
    if (spectrum) {
      switch (spectrum->variant) {
        case minera::PerformanceSpectrumVariant::kConnected: variant = "connected"; break;
        case minera::PerformanceSpectrumVariant::kDisconnected: variant = "disconnected"; break;
        case minera::PerformanceSpectrumVariant::kDataframe: variant = "dataframe"; break;
        case minera::PerformanceSpectrumVariant::kLog: variant = "log"; break;
        case minera::PerformanceSpectrumVariant::kDataframeDisconnected: variant = "dataframe_disconnected"; break;
        case minera::PerformanceSpectrumVariant::kLogDisconnected: variant = "log_disconnected"; break;
        case minera::PerformanceSpectrumVariant::kLazyframe: variant = "lazyframe"; break;
        case minera::PerformanceSpectrumVariant::kLazyframeDisconnected: variant = "lazyframe_disconnected"; break;
      }
    } else variant = batches->variant == minera::BatchDiscoveryVariant::kLog ? "log" : batches->variant == minera::BatchDiscoveryVariant::kPandas ? "pandas" : "polars";
    const auto native_result = spectrum ? minera::serialize_active_source_spectrum_json(*spectrum) : minera::serialize_active_source_batches_json(*batches);
    response.set_content("{\"family\":\"TIMING_DISCOVERY\",\"method\":" + quote(family) + ",\"variant\":" + quote(variant) + ",\"source_id\":" + quote(std::to_string(session->query_source_id)) +
      ",\"dataset\":" + quote(session->dataset) + ",\"source\":{\"case_count\":" + (session->log_ready ? std::to_string(require_prepared_log(*session).trace_count()) : std::string("null")) + ",\"event_count\":" + (session->active_source && session->active_source->checked_table_if_available() ? std::to_string(session->active_source->checked_table().rows.size()) : session->log_ready ? std::to_string(require_prepared_log(*session).event_count()) : std::string("null")) + "},\"result\":" + result + ",\"result_error\":" + (result_error.empty() ? "null" : quote(result_error)) + ",\"native_result\":" + native_result + '}', "application/json");
  } catch (const std::exception& error) { response.status = 400; response.set_content(error_json(error.what()), "application/json"); }
}

void handle_graph_discovery(const httplib::Request& request,
                            httplib::Response& response) {
  try {
    std::shared_ptr<const MinedSession> session;
    { std::lock_guard<std::mutex> lock(g_session_mutex); session = g_session; }
    if (!session) { response.status = 409; response.set_content(error_json("no mined log; mine a log first"), "application/json"); return; }
    if (!request.has_param("source_id") || request.get_param_value("source_id") != std::to_string(session->query_source_id))
      throw std::invalid_argument("the source log changed; refresh the graph discovery source");
    const auto family = request.get_param_value("family");
    if (family != "footprints" && family != "transition_system")
      throw std::invalid_argument("graph discovery family expects footprints or transition_system");
    static const std::set<std::string> footprint_fields = {
      "footprints_discovery_variants", "footprints_model", "footprints_conformance_variants",
      "footprints_strict", "footprints_enable_act_always_executed", "footprints_activity_attribute",
      "footprints_case_id_attribute", "footprints_timestamp_attribute", "footprints_index_attribute",
      "footprints_start_timestamp_attribute", "footprints_sort_required", "footprints_initial_marking"};
    static const std::set<std::string> transition_fields = {
      "transition_view", "transition_direction", "transition_window", "transition_include_data",
      "transition_activity_attribute", "transition_case_id_attribute"};
    const auto& fields = family == "footprints" ? footprint_fields : transition_fields;
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (key != "family" && key != "source_id" && key != "include_traces" && !fields.contains(key))
        throw std::invalid_argument("unknown graph discovery parameter: " + key);
    }
    analysis_include_traces(request);  // rejects an invalid value before any work
    const auto lookup = [&](std::string_view name) { return request.has_param(std::string(name)) ? request.get_param_value(std::string(name)) : std::string{}; };
    const auto present = [&](std::string_view name) { return request.has_param(std::string(name)); };
    auto params = minera::parse_mine_params(lookup, present);
    const bool flat_footprints = family == "footprints" &&
        (params.footprints.discovery_variants.contains("entire_dataframe") ||
         params.footprints.discovery_variants.contains("polars_lazyframes"));
    overlay_cached_source_selectors(request, *session, params, flat_footprints);
    const minera::EventLog* input = nullptr;
    if (family == "footprints") input = params.footprints.model == "alpha" ? &cached_model_log(*session,"alpha") : &require_prepared_log(*session);
    else if (session->log_ready) input = &require_prepared_log(*session);
    auto graph = input ? minera::discover_directly_follows_graph(*input) : minera::DirectlyFollowsGraph{};
    if (session->graph_ready) for (const auto& activity : require_graph(*session).activities()) graph.ensure_activity(activity);
    std::optional<minera::FootprintsAnalysis> footprints;
    std::optional<minera::TransitionSystem> transitions;
    std::string model_footprint, model_origin;
    if (family == "footprints") {
      if (!session->powl) throw std::invalid_argument("cached POWL model is unavailable; mine the source again");
      register_activity_selector_values(graph, *input, params.footprints.activity_attribute);
      footprints = analyze_footprints(require_prepared_log(*session), graph, cached_model_log(*session,"alpha"),
        require_process_tree(*session), *session->powl, session->nets.at("inductive"), session->nets.at("alpha"),
        session->nets.at("heuristic"), session->nets.at("powl"), params.footprints, session->active_source.get());
      model_footprint = minera::footprint_json(footprints->model_footprint, graph);
      model_origin = params.footprints.model == "dfg" ? "selected_source_dfg" : "cached_analysis_model";
    } else {
      transitions = minera::discover_transition_system(session->active_source ? session->active_source->selected_view() : minera::EventDataView(require_prepared_log(*session)), params.transition_system);
      for (const auto& state : transitions->states()) for (const auto& activity : state.payload) if (activity) graph.ensure_activity(*activity);
      for (const auto& transition : transitions->transitions()) if (transition.label) graph.ensure_activity(*transition.label);
    }
    std::string result = "null", result_error;
    if (input) try { const minera::OmitAnalysisTraces omit_traces(!analysis_include_traces(request)); result = minera::analysis_json(session->dataset, *input, graph,
      footprints ? &require_process_tree(*session) : nullptr, footprints ? &session->nets.at("alpha") : nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
      footprints ? &session->nets.at("inductive") : nullptr, nullptr,
      footprints ? &session->nets.at("heuristic") : nullptr, nullptr, nullptr, nullptr,
      transitions ? &*transitions : nullptr, footprints ? &*session->powl : nullptr,
      footprints ? &session->nets.at("powl") : nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, footprints ? &*footprints : nullptr); } catch (const std::exception& error) { result_error = error.what(); }
    const auto quote = minera::embedding_runtime::detail::escape_json;
    response.set_content("{\"family\":\"GRAPH_DISCOVERY\",\"method\":" + quote(family) +
      ",\"variant\":" + quote(family == "footprints" ? "aggregate" : "view_based") +
      ",\"source_id\":" + quote(std::to_string(session->query_source_id)) + ",\"dataset\":" + quote(session->dataset) +
      ",\"source\":{\"case_count\":" + (session->log_ready ? std::to_string(require_prepared_log(*session).trace_count()) : std::string("null")) + ",\"event_count\":" + (session->active_source && session->active_source->checked_table_if_available() ? std::to_string(session->active_source->checked_table().rows.size()) : session->log_ready ? std::to_string(require_prepared_log(*session).event_count()) : std::string("null")) +
      "},\"analysis_source\":{\"case_count\":" + (input ? std::to_string(input->trace_count()) : std::string("null")) + ",\"event_count\":" + (input ? std::to_string(input->event_count()) : std::string("null")) +
      ",\"origin\":" + quote(family == "footprints" && params.footprints.model == "alpha" ? "cached_alpha_projection" : "cached_source") +
      "},\"model_origin\":" + (model_origin.empty() ? "null" : quote(model_origin)) +
      ",\"model_footprint\":" + (model_footprint.empty() ? "null" : model_footprint) + ",\"result\":" + result + ",\"result_error\":" + (result_error.empty() ? "null" : quote(result_error)) + ",\"native_result\":" + (transitions ? minera::serialize_active_source_transition_json(*transitions) : std::string("null")) + '}', "application/json");
  } catch (const std::exception& error) { response.status = 400; response.set_content(error_json(error.what()), "application/json"); }
}

void handle_constraint_discovery(const httplib::Request& request,
                                 httplib::Response& response) {
  try {
    std::shared_ptr<const MinedSession> session;
    { std::lock_guard<std::mutex> lock(g_session_mutex); session = g_session; }
    if (!session) { response.status = 409; response.set_content(error_json("no mined log; mine a log first"), "application/json"); return; }
    if (!request.has_param("source_id") || request.get_param_value("source_id") != std::to_string(session->query_source_id))
      throw std::invalid_argument("the source log changed; refresh the constraint source");
    const auto family = request.get_param_value("family");
    if (family != "declare" && family != "log_skeleton")
      throw std::invalid_argument("constraint family expects declare or log_skeleton");
    static const std::set<std::string> declare_fields = {
      "declare_activity_attribute", "declare_considered_activities", "declare_considered_activities_json", "declare_allowed_templates",
      "declare_min_support_ratio", "declare_min_confidence_ratio", "declare_auto_selection_multiplier",
      "generate_log", "declare_playout_traces", "declare_playout_min_length", "declare_playout_max_length", "declare_playout_seed"};
    static const std::set<std::string> skeleton_fields = {
      "log_skeleton_activity_attribute", "log_skeleton_noise_threshold", "log_skeleton_constraints"};
    const auto& fields = family == "declare" ? declare_fields : skeleton_fields;
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (key != "family" && key != "source_id" && key != "conformance_activity_key" && key != "diagnostics_case_id_key" && key != "include_traces" && !fields.contains(key))
        throw std::invalid_argument("unknown constraint discovery parameter: " + key);
    }
    analysis_include_traces(request);  // rejects an invalid value before any work
    const auto lookup = [&](std::string_view name) { return request.has_param(std::string(name)) ? request.get_param_value(std::string(name)) : std::string{}; };
    const auto present = [&](std::string_view name) { return request.has_param(std::string(name)); };
    auto params = minera::parse_mine_params(lookup, present);
    overlay_cached_source_selectors(request, *session, params);
    const auto activity = family == "declare" ? params.declare_options.activity_key : params.log_skeleton.activity_key;
    const auto conformance_activity = request.has_param("conformance_activity_key") ? trim_text(request.get_param_value("conformance_activity_key")) : activity;
    const auto diagnostics_case = request.has_param("diagnostics_case_id_key") ? trim_text(request.get_param_value("diagnostics_case_id_key")) : "case:concept:name";
    if (conformance_activity.empty() || diagnostics_case.empty()) throw std::invalid_argument("constraint selectors must not be empty");
    std::optional<minera::DeclareModel> declare;
    std::optional<minera::LogSkeletonModel> skeleton;
    std::optional<std::vector<minera::DeclareConformance>> declare_conformance;
    std::optional<std::vector<minera::LogSkeletonConformance>> skeleton_conformance;
    auto graph = session->log_ready ? minera::discover_directly_follows_graph(require_prepared_log(*session)) : minera::DirectlyFollowsGraph{};
    std::string diagnostics, diagnostics_error, playout_xes, playout_result, playout_error;
    // An XES source's cases in pm4py's DataFrame order (see constraint_case_order);
    // the diagnostics table keeps the frame's (file) order, as the reference's does.
    const auto constraint_order = session->active_source && session->log_ready
        ? (family == "declare"
               ? constraint_case_order(*session->active_source, session->active_source->selected_view(),
                     params.declare_conformance_options.activity_key, params.declare_conformance_options.case_id_key)
               : constraint_case_order(*session->active_source, session->active_source->selected_view(),
                     params.log_skeleton.activity_key, params.log_skeleton.case_id_key))
        : std::nullopt;
    if (family == "declare") {
      declare = minera::discover_declare(session->active_source ? session->active_source->selected_view() : minera::EventDataView(require_prepared_log(*session)), params.declare_options);
      auto options = params.declare_conformance_options; options.activity_key = conformance_activity;
      declare_conformance = minera::check_declare_conformance(session->active_source ? session->active_source->selected_view() : minera::EventDataView(require_prepared_log(*session)), *declare, options);
      for (const auto& [name, rules] : declare->rules) for (const auto& rule : rules) {
        (void)name; graph.ensure_activity(rule.first); if (rule.binary) graph.ensure_activity(rule.second);
      }
      for (const auto& trace : *declare_conformance) for (const auto& deviation : trace.deviations) {
        graph.ensure_activity(deviation.first); if (deviation.binary) graph.ensure_activity(deviation.second);
      }
      try { diagnostics = minera::serialize_event_table_input_json(minera::declare_conformance_diagnostics(session->active_source ? session->active_source->selected_view() : minera::EventDataView(require_prepared_log(*session)), *declare_conformance, {diagnostics_case})); }
      catch (const std::exception& error) { diagnostics_error = error.what(); }
      if (ocel_query_bool(request, "generate_log", false)) {
        try {
          const auto plookup = [&](std::string_view name) { return lookup("declare_playout_" + std::string(name)); };
          const auto ppresent = [&](std::string_view name) { return present("declare_playout_" + std::string(name)); };
          const auto playout_options = minera::parse_declare_playout_params(plookup, ppresent);
          if (playout_options.max_length && playout_options.n_traces > 1000000 / playout_options.max_length)
            throw std::invalid_argument("Declare playout exceeds the server event budget");
          const auto generated = minera::declare_playout(*declare, playout_options);
          auto generated_graph = minera::discover_directly_follows_graph(generated);
          for (const auto& [name, rules] : declare->rules) for (const auto& rule : rules) {
            (void)name; generated_graph.ensure_activity(rule.first); if (rule.binary) generated_graph.ensure_activity(rule.second);
          }
          const auto checked = minera::check_declare_conformance(generated, *declare);
          playout_xes = minera::to_xes(generated, "Declare discovered-model playout");
          playout_result = minera::analysis_json("Declare discovered-model playout", generated, generated_graph,
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
            nullptr, nullptr, &*declare, &checked);
        } catch (const std::exception& error) { playout_error = error.what(); }
      }
    } else {
      skeleton = constraint_order
          ? minera::discover_log_skeleton(session->active_source->selected_view(), params.log_skeleton, *constraint_order)
          : minera::discover_log_skeleton(session->active_source ? session->active_source->selected_view() : minera::EventDataView(require_prepared_log(*session)), params.log_skeleton);
      auto options = params.log_skeleton; options.activity_key = conformance_activity;
      skeleton_conformance = minera::check_log_skeleton_conformance(session->active_source ? session->active_source->selected_view() : minera::EventDataView(require_prepared_log(*session)), *skeleton, options);
      for (const auto* relations : {&skeleton->equivalence, &skeleton->always_after, &skeleton->always_before, &skeleton->never_together, &skeleton->directly_follows})
        for (const auto& relation : *relations) { graph.ensure_activity(relation.source); graph.ensure_activity(relation.target); }
      for (const auto& [name, frequencies] : skeleton->activ_freq) { (void)frequencies; graph.ensure_activity(name); }
      for (const auto& trace : *skeleton_conformance) for (const auto& deviation : trace.deviations) {
        if (deviation.constraint == "activ_freq") graph.ensure_activity(deviation.activity);
        else { graph.ensure_activity(deviation.relation.source); graph.ensure_activity(deviation.relation.target); }
      }
      try { diagnostics = minera::serialize_event_table_input_json(minera::log_skeleton_conformance_diagnostics(session->active_source ? session->active_source->selected_view() : minera::EventDataView(require_prepared_log(*session)), *skeleton_conformance, {diagnostics_case})); }
      catch (const std::exception& error) { diagnostics_error = error.what(); }
    }
    std::string result = "null", result_error;
    if (session->log_ready) try { const minera::OmitAnalysisTraces omit_traces(!analysis_include_traces(request)); const minera::AnalysisConstraintCaseOrder constraint_list_order(constraint_order ? &*constraint_order : nullptr); result = minera::analysis_json(session->dataset, require_prepared_log(*session), graph,
      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
      skeleton ? &*skeleton : nullptr, skeleton_conformance ? &*skeleton_conformance : nullptr,
      declare ? &*declare : nullptr, declare_conformance ? &*declare_conformance : nullptr); } catch (const std::exception& error) { result_error = error.what(); }
    const auto quote = minera::embedding_runtime::detail::escape_json;
    const auto native_result = declare
        ? [&] { const auto ordered = in_constraint_order(*declare_conformance, constraint_order); return minera::serialize_active_source_declare_json(*declare, &ordered); }()
        : [&] { const auto ordered = in_constraint_order(*skeleton_conformance, constraint_order); return minera::serialize_active_source_skeleton_json(*skeleton, &ordered); }();
    const auto optional_json = [](const std::string& value) { return value.empty() ? std::string("null") : value; };
    const auto optional_text = [&](const std::string& value) { return value.empty() ? std::string("null") : quote(value); };
    response.set_content("{\"family\":\"CONSTRAINT_DISCOVERY\",\"method\":" + quote(family) +
      ",\"variant\":\"classic\",\"source_id\":" + quote(std::to_string(session->query_source_id)) +
      ",\"dataset\":" + quote(session->dataset) + ",\"activity_key\":" + quote(activity) +
      ",\"conformance_activity_key\":" + quote(conformance_activity) + ",\"diagnostics_case_id_key\":" + quote(diagnostics_case) +
      ",\"source\":{\"case_count\":" + (session->log_ready ? std::to_string(require_prepared_log(*session).trace_count()) : std::string("null")) + ",\"event_count\":" + (session->active_source && session->active_source->checked_table_if_available() ? std::to_string(session->active_source->checked_table().rows.size()) : session->log_ready ? std::to_string(require_prepared_log(*session).event_count()) : std::string("null")) +
      "},\"result\":" + result + ",\"result_error\":" + (result_error.empty() ? "null" : quote(result_error)) + ",\"native_result\":" + native_result + ",\"diagnostics\":" + optional_json(diagnostics) + ",\"diagnostics_error\":" + optional_text(diagnostics_error) +
      ",\"playout_result\":" + optional_json(playout_result) + ",\"playout_xes\":" + optional_text(playout_xes) + ",\"playout_error\":" + optional_text(playout_error) + '}', "application/json");
  } catch (const std::exception& error) { response.status = 400; response.set_content(error_json(error.what()), "application/json"); }
}

void handle_structured_discovery(const httplib::Request& request,
                                httplib::Response& response,
                                const MinedSession& session,
                                const std::string& family) {
  static const std::set<std::string> inductive_fields = {
      "inductive_variant", "noise_threshold", "inductive_disable_strict_sequence_cut",
      "inductive_disable_fallthroughs", "inductive_multiprocessing",
      "inductive_sort_events_by_timestamp", "inductive_activity_key",
      "inductive_timestamp_key", "inductive_case_id_key", "discovery_tree_variant"};
  static const std::set<std::string> powl_fields = {
      "powl_variant", "powl_filtering_type", "powl_filtering_threshold",
      "powl_order_frequency_ratio", "powl_brute_force_activity_limit",
      "powl_activity_key", "powl_timestamp_key", "powl_case_id_key"};
  const auto& allowed = family == "inductive" ? inductive_fields : powl_fields;
  for (const auto& [key, value] : request.params) {
    (void)value;
    if (key != "family" && key != "source_id" &&
        key != "discovery_playout_traces" && key != "discovery_playout_max_steps" &&
        key != "discovery_playout_seed" && !allowed.contains(key))
      throw std::invalid_argument("unknown structured discovery parameter: " + key);
  }
  const auto lookup = [&](std::string_view name) { return request.has_param(std::string(name)) ? request.get_param_value(std::string(name)) : std::string{}; };
  const auto present = [&](std::string_view name) { return request.has_param(std::string(name)); };
  auto params = minera::parse_mine_params(lookup, present);
  overlay_cached_source_selectors(request,session,params);
  std::optional<minera::ProcessTree> tree;
  std::optional<minera::PowlNode> powl;
  minera::EventLog discovery_log;
  minera::PetriNet net;
  std::string variant, activity;
  if (family == "inductive") {
    tree = minera::discover_process_tree(session.active_source ? session.active_source->selected_view() : minera::EventDataView(require_prepared_log(session)), params.inductive);
    discovery_log = minera::prepare_inductive_event_log(session.active_source ? session.active_source->selected_view() : minera::EventDataView(require_prepared_log(session)), params.inductive);
    const auto conversion = minera::parse_process_tree_petri_net_variant(
        request.has_param("discovery_tree_variant") ? request.get_param_value("discovery_tree_variant") : "standard");
    net = minera::to_petri_net(*tree, conversion);
    variant = params.inductive.variant == minera::InductiveMinerVariant::kLog ? "log" :
        params.inductive.variant == minera::InductiveMinerVariant::kImf ? "imf" : "dfg";
    activity = params.inductive.activity_key;
  } else {
    powl = minera::discover_powl(require_prepared_log(session), params.powl);
    discovery_log = minera::normalize_powl_event_log(require_prepared_log(session), params.powl);
    net = minera::to_petri_net(*powl);
    switch (params.powl.variant) {
      case minera::PowlDiscoveryVariant::kTree: variant = "tree"; break;
      case minera::PowlDiscoveryVariant::kBruteForce: variant = "brute_force"; break;
      case minera::PowlDiscoveryVariant::kMaximal: variant = "maximal"; break;
      case minera::PowlDiscoveryVariant::kDynamicClustering: variant = "dynamic_clustering"; break;
      case minera::PowlDiscoveryVariant::kDynamicClusteringPlain: variant = "dynamic_clustering_plain"; break;
    }
    activity = params.powl.activity_key;
  }
  auto graph = minera::discover_directly_follows_graph(discovery_log);
  for (const auto& name : net.activities()) graph.ensure_activity(name);
  const auto result = minera::analysis_json(session.dataset, discovery_log, graph,
      tree ? &*tree : nullptr, nullptr, nullptr, // tree, Alpha, heuristics
      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
      tree ? &net : nullptr, nullptr, nullptr, nullptr, // Inductive and heuristic nets/decorations
      nullptr, nullptr, nullptr, // temporal, minimum-self-distance, transition system
      powl ? &*powl : nullptr, powl ? &net : nullptr);
  std::string pnml, bpmn, ptml, bpmn_error, ptml_error, playout, playout_error;
  pnml = minera::to_pnml(net, family + " " + variant + " discovery");
  try { bpmn = tree ? minera::to_bpmn(*tree) : minera::to_bpmn(net); }
  catch (const std::exception& error) { bpmn_error = error.what(); }
  // POWL converts directly, as pm4py's convert_to_process_tree(powl) does
  // (a one-activity model is the leaf 'a'; its one-transition workflow net
  // would raise in the net conversion).
  try { ptml = minera::to_ptml(tree ? *tree : minera::to_process_tree(*powl), family + " " + variant + " discovery"); }
  catch (const std::exception& error) { ptml_error = error.what(); }
  const auto traces = ocel_query_size(request, "discovery_playout_traces", 0);
  const auto steps = ocel_query_size(request, "discovery_playout_max_steps", 1000);
  const auto seed = ocel_query_size(request, "discovery_playout_seed", 1);
  if (traces > 10000 || steps > 10000 || (steps && traces > 1000000 / steps))
    throw std::invalid_argument("discovery playout exceeds the server firing budget");
  if (traces) {
    try { minera::PlayoutOptions options; options.traces = traces; options.max_steps = steps; options.seed = seed;
      playout = minera::to_xes(minera::playout(net, options), family + " " + variant + " discovery playout"); }
    catch (const std::exception& error) { playout_error = error.what(); }
  }
  const auto quote = minera::embedding_runtime::detail::escape_json;
  std::string xes_case = "concept:name", xes_activity = "concept:name", xes_timestamp = "time:timestamp";
  if (!discovery_log.traces.empty()) {
    const auto& first = discovery_log.traces.front();
    if (!first.case_id_source_key.empty()) xes_case = first.case_id_source_key;
    for (const auto& trace : discovery_log.traces) if (!trace.events.empty()) {
      const auto& event = trace.events.front();
      if (!event.activity_source_key.empty()) xes_activity = event.activity_source_key;
      if (!event.timestamp_source_key.empty()) xes_timestamp = event.timestamp_source_key;
      break;
    }
  }
  response.set_content("{\"family\":\"WORKFLOW_DISCOVERY\",\"method\":" + quote(family) +
      ",\"variant\":" + quote(variant) + ",\"activity_key\":" + quote(activity) +
      ",\"source_id\":" + quote(std::to_string(session.query_source_id)) +
      ",\"dataset\":" + quote(session.dataset) + ",\"source\":{\"case_count\":" +
      (session.log_ready ? std::to_string(require_prepared_log(session).trace_count()) : std::string("null")) + ",\"event_count\":" + (session.active_source && session.active_source->checked_table_if_available() ? std::to_string(session.active_source->checked_table().rows.size()) : session.log_ready ? std::to_string(require_prepared_log(session).event_count()) : std::string("null")) +
      "},\"discovery_source\":{\"case_count\":" + std::to_string(discovery_log.trace_count()) +
      ",\"event_count\":" + std::to_string(discovery_log.event_count()) + "},\"source_xes\":" +
      quote(minera::to_xes(discovery_log, family + " adapted discovery source")) +
      ",\"source_xes_options\":{\"xesActivityAttribute\":" + quote(xes_activity) +
      ",\"xesCaseAttribute\":" + quote(xes_case) + ",\"xesTimestampAttribute\":" + quote(xes_timestamp) +
      ",\"xesTimestampSort\":false}" +
      ",\"result\":" + result + ",\"alpha_discovery\":null,\"pnml\":" + quote(pnml) +
      ",\"pnml_error\":null,\"bpmn\":" + (bpmn.empty() ? "null" : quote(bpmn)) +
      ",\"bpmn_error\":" + (bpmn_error.empty() ? "null" : quote(bpmn_error)) +
      ",\"ptml\":" + (ptml.empty() ? "null" : quote(ptml)) +
      ",\"ptml_error\":" + (ptml_error.empty() ? "null" : quote(ptml_error)) +
      ",\"playout_xes\":" + (playout.empty() ? "null" : quote(playout)) +
      ",\"playout_error\":" + (playout_error.empty() ? "null" : quote(playout_error)) + '}', "application/json");
}

void handle_workflow_discovery(const httplib::Request& request,
                               httplib::Response& response) {
  try {
    std::shared_ptr<const MinedSession> session;
    { std::lock_guard<std::mutex> lock(g_session_mutex); session = g_session; }
    if (!session) {
      response.status = 409;
      response.set_content(error_json("no mined log; mine a log first"), "application/json");
      return;
    }
    if (!request.has_param("source_id"))
      throw std::invalid_argument("source_id is required for workflow discovery");
    if (request.get_param_value("source_id") != std::to_string(session->query_source_id))
      throw std::invalid_argument("the source log changed; refresh the discovery source");
    const auto family = request.get_param_value("family");
    if (family == "inductive" || family == "powl") {
      handle_structured_discovery(request, response, *session, family);
      return;
    }
    if (family != "alpha" && family != "heuristic")
      throw std::invalid_argument("workflow discovery family must be alpha or heuristic");
    static const std::set<std::string> alpha_fields = {
      "alpha_variant", "alpha_activity_attribute", "alpha_remove_unconnected",
      "alpha_max_rows_to_materialize"};
    static const std::set<std::string> heuristic_fields = {
      "heuristic_variant", "heuristic_activity_attribute", "heuristic_timestamp_attribute",
      "dependency_threshold", "and_threshold", "min_edge_frequency",
      "heuristic_min_activity_count", "heuristic_decoration",
      "heuristic_performance_aggregation", "heuristic_business_hours",
      "heuristic_business_hour_slots", "heuristic_start_timestamp_attribute",
      "heuristic_lifecycle_instance_attribute", "heuristic_dfg_pre_cleaning_noise_threshold",
      "heuristic_loop_length_two_threshold", "heuristic_keep_once_per_case",
      "heuristic_options_json", "heuristic_options_file", "heuristic_most_common_paths_json", "heuristic_most_common_paths_file",
      "heuristic_most_common_paths", "heuristic_plusplus_decoration_selector",
      "heuristic_plusplus_decoration_value", "heuristic_connect_all"};
    const auto& allowed = family == "alpha" ? alpha_fields : heuristic_fields;
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (key != "family" && key != "source_id" &&
          key != "discovery_playout_traces" && key != "discovery_playout_max_steps" &&
          key != "discovery_playout_seed" && !allowed.contains(key))
        throw std::invalid_argument("unknown workflow discovery parameter: " + key);
    }
    const auto lookup = [&](std::string_view name) {
      return request.has_param(std::string(name)) ? request.get_param_value(std::string(name)) : std::string{};
    };
    const auto present = [&](std::string_view name) { return request.has_param(std::string(name)); };
    auto params = minera::parse_mine_params(lookup, present);
    const auto exact_cached_heuristic=params.heuristic;
    overlay_cached_source_selectors(request,*session,params);
    if(minera::exact_heuristic_options_requested(present))params.heuristic=exact_cached_heuristic;
    minera::record_exact_effective_options(params);
    const auto discovery_input = session->active_source ? session->active_source->selected_view() : minera::EventDataView(require_prepared_log(*session));
    if (family == "heuristic")
      params.heuristic.connect_all = ocel_query_bool(request, "heuristic_connect_all", params.heuristic.connect_all);
    std::optional<minera::HeuristicsNet> heuristic;
    std::optional<minera::AlphaPlusDiscoveryResult> alpha_plus;
    minera::PetriNet net;
    const auto variant = family == "alpha" ? (params.alpha.short_loops ? "plus" : "classic") :
      (params.heuristic.variant == minera::HeuristicMinerVariant::kPlusPlus ? "plusplus" : "classic");
    if (family == "alpha") {
      if (params.alpha.short_loops) {
        alpha_plus = minera::discover_alpha_plus_result(discovery_input, params.alpha);
        net = alpha_plus->net;
      } else net = minera::discover_petri_net(discovery_input, params.alpha);
    } else {
      heuristic = minera::observe_exact_heuristic_call(discovery_input,params.heuristic,[&]{return minera::discover_heuristics_net(discovery_input, params.heuristic);});
      try{net = minera::to_petri_net(*heuristic);minera::record_exact_petri_net(net);}
      catch(const std::exception& e){minera::record_exact_optional_artifact("conversion",e.what(),true);throw;}
    }
    auto graph = session->log_ready ? minera::discover_directly_follows_graph(require_prepared_log(*session)) : minera::DirectlyFollowsGraph{};
    for (const auto& activity : net.activities()) graph.ensure_activity(activity);
    if (heuristic) for (const auto& activity : heuristic->activities) graph.ensure_activity(activity);
    std::string result = "null", result_error;
    if (session->log_ready) try { result = minera::analysis_json(session->dataset, require_prepared_log(*session), graph,
      nullptr, family == "alpha" ? &net : nullptr, heuristic ? &*heuristic : nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, family == "heuristic" ? &net : nullptr); } catch (const std::exception& error) { result_error = error.what(); }
    std::string pnml, pnml_error;
    try {
      if (alpha_plus) {
        if (!alpha_plus->detached_initial_marking.empty() || !alpha_plus->detached_final_marking.empty())
          throw std::invalid_argument("Detached Alpha+ markings cannot be represented by the current PNML workflow exporter; the complete discovery JSON retains them.");
        minera::PnmlDocument document; document.net = net;
        for (const auto tokens : alpha_plus->initial_marking) document.initial_marking.push_back(tokens);
        for (const auto tokens : alpha_plus->final_marking) document.final_marking.push_back(tokens);
        pnml = minera::to_pnml(document, "Alpha+ discovery");
      } else pnml = minera::to_pnml(net, family + " " + variant + " discovery");
    } catch (const std::exception& error) { pnml_error = error.what(); }
    std::string bpmn, bpmn_error, ptml, ptml_error, playout, playout_error;
    try { bpmn = minera::to_bpmn(net, family + " " + variant + " discovery"); }
    catch (const std::exception& error) { bpmn_error = error.what(); }
    try { ptml = minera::to_ptml(minera::to_process_tree(net), family + " " + variant + " discovery"); }
    catch (const std::exception& error) { ptml_error = error.what(); }
    const auto playout_traces = ocel_query_size(request, "discovery_playout_traces", 0);
    const auto playout_steps = ocel_query_size(request, "discovery_playout_max_steps", 1000);
    const auto playout_seed = ocel_query_size(request, "discovery_playout_seed", 1);
    if (playout_traces > 10000 || playout_steps > 10000 ||
        (playout_steps && playout_traces > 1000000 / playout_steps))
      throw std::invalid_argument("discovery playout exceeds the server firing budget");
    if (playout_traces) {
      try {
        if (pnml.empty()) throw std::invalid_argument("Discovery playout requires representable workflow markings; inspect the native discovery result.");
        minera::PlayoutOptions options; options.traces = playout_traces;
        options.max_steps = playout_steps; options.seed = playout_seed;
        if (alpha_plus) {
          for (const auto tokens : alpha_plus->initial_marking) options.initial_marking.push_back(static_cast<std::uint32_t>(tokens));
          for (const auto tokens : alpha_plus->final_marking) options.final_marking.push_back(static_cast<std::uint32_t>(tokens));
        }
        playout = minera::to_xes(minera::playout(net, options), family + " " + variant + " discovery playout");
      } catch (const std::exception& error) { playout_error = error.what(); }
    }
    const auto quote = minera::embedding_runtime::detail::escape_json;
    const auto activity = family == "alpha" ? params.alpha.activity_attribute : params.heuristic.activity_attribute;
    response.set_content("{\"family\":\"WORKFLOW_DISCOVERY\",\"method\":" + quote(family) +
      ",\"variant\":" + quote(variant) + ",\"activity_key\":" + quote(activity) +
      ",\"source_id\":" + quote(std::to_string(session->query_source_id)) +
      ",\"dataset\":" + quote(session->dataset) + ",\"source\":{\"case_count\":" +
      (session->log_ready ? std::to_string(require_prepared_log(*session).trace_count()) : std::string("null")) + ",\"event_count\":" +
      (session->active_source && session->active_source->checked_table_if_available() ? std::to_string(session->active_source->checked_table().rows.size()) : session->log_ready ? std::to_string(require_prepared_log(*session).event_count()) : std::string("null")) + "},\"result\":" + result +
      ",\"result_error\":" + (result_error.empty() ? "null" : quote(result_error)) + ",\"native_result\":{\"petri_net\":" + minera::workflow_input_petri_json(net) + ",\"heuristics\":" + (heuristic ? minera::workflow_input_heuristics_json(*heuristic) : "null") + "}" +
      ",\"alpha_discovery\":" + (alpha_plus ? minera::alpha_plus_result_json(*alpha_plus) : "null") +
      ",\"pnml\":" + (pnml.empty() ? "null" : quote(pnml)) +
      ",\"pnml_error\":" + (pnml_error.empty() ? "null" : quote(pnml_error)) +
      ",\"bpmn\":" + (bpmn.empty() ? "null" : quote(bpmn)) +
      ",\"bpmn_error\":" + (bpmn_error.empty() ? "null" : quote(bpmn_error)) +
      ",\"ptml\":" + (ptml.empty() ? "null" : quote(ptml)) +
      ",\"ptml_error\":" + (ptml_error.empty() ? "null" : quote(ptml_error)) +
      ",\"playout_xes\":" + (playout.empty() ? "null" : quote(playout)) +
      ",\"playout_error\":" + (playout_error.empty() ? "null" : quote(playout_error)) + '}', "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

#include "workflow_input_route.hpp"
#include "graph_alignment_route.hpp"
#include "saved_dfg_projection_route.hpp"
#include "inductive_input_route.hpp"
#include "process_tree_alignment_route.hpp"
#include "petri_anti_multi_route.hpp"
#include "model_conversion_route.hpp"
#include "tree_reduction_route.hpp"
#include "original_model_context_route.hpp"
#include "original_features_route.hpp"
#include "original_label_splitting_route.hpp"
#include "original_log_query_route.hpp"
#include "case_description_route.hpp"
#include "reference_log_filter_route.hpp"
#include "table_attribute_repetition_route.hpp"
#include "table_between_filter_route.hpp"
#include "table_case_filter_route.hpp"
#include "table_paths_filter_route.hpp"
#include "table_timestamp_filter_route.hpp"
#include "table_variants_filter_route.hpp"
#include "table_attributes_filter_route.hpp"
#include "log_attributes_filter_route.hpp"
#include "log_variants_filter_route.hpp"
#include "log_timestamp_filter_route.hpp"
#include "ltl_filter_route.hpp"
#include "monte_carlo_route.hpp"
#include "playout_route.hpp"
#include "original_decision_route.hpp"
#include "original_declare_route.hpp"
#include "original_log_skeleton_route.hpp"
#include "original_footprints_route.hpp"
#include "original_genetic_matrix_route.hpp"
#include "original_embedding_route.hpp"
#include "exact_filter_discovery_route.hpp"
#include "original_edit_distance_route.hpp"
#include "original_bose_route.hpp"
#include "original_discovery_route.hpp"
#include "original_petri_analysis_route.hpp"
#include "original_root_cause_route.hpp"
#include "original_anonymization_route.hpp"
#include "original_quality_route.hpp"
#include "original_clustering_route.hpp"
#include "original_organizational_route.hpp"
#include "original_temporal_route.hpp"
#include "original_timing_route.hpp"
#include "original_time_analysis_route.hpp"
#include "original_structural_inputs_route.hpp"
#include "original_emd_route.hpp"
#include "original_object_centric_playout_route.hpp"

bool handle_literal_controls_request(const httplib::Request&, httplib::Response&, std::string_view);

// Cached-session adapter for the bounded classic ILP miner. The log is
// intentionally taken from the latest /api/mine session so this endpoint does
// not accept uploads or perform any implicit re-importing.
void handle_ilp_mine(const httplib::Request& request,
                     httplib::Response& response) {
  if (handle_literal_controls_request(request, response, "ilp")) return;
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
        "ilp_noise_threshold", "ilp_max_nodes", "source_id", "include_pnml"};
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
    const bool include_model = discovery_model_requested(request, *session);
    auto options = minera::parse_ilp_miner_params(lookup, present);
    options.cancelled = connection_cancel_hook(request);
    if (options.max_nodes > kServerIlpMaxNodes) {
      throw std::invalid_argument("ilp_max_nodes exceeds the server safety cap");
    }
    const auto result = minera::mine_ilp(require_prepared_log(*session), options);
    auto body = minera::ilp_miner_json(result);
    if (include_model) body = discovery_model_json(std::move(body), result.net,
        result.markings, *session, result.diagnostics.computed);
    response.set_content(body, "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

// Cached-session adapter for bounded classic genetic discovery. The
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
        "genetic_max_states", "genetic_conversion_variant", "source_id", "include_pnml"};
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
    const bool include_model = discovery_model_requested(request, *session);
    auto options = minera::parse_genetic_miner_params(lookup, present);
    options.cancelled = connection_cancel_hook(request);
    if (options.population_size > kServerGeneticMaxPopulation ||
        options.generations > kServerGeneticMaxGenerations ||
        options.max_states > kServerGeneticMaxStates) {
      throw std::invalid_argument("genetic safety bound exceeded");
    }
    const auto result = minera::mine_genetic(require_prepared_log(*session), options);
    if (convert_matrix) {
      const auto variant = request.has_param("genetic_conversion_variant")
          ? minera::parse_genetic_matrix_conversion_variant(
                request.get_param_value("genetic_conversion_variant"))
          : minera::GeneticMatrixConversionVariant::kToPetriNet;
      const auto converted = minera::convert_genetic_matrix(result.matrix, variant);
      auto body = minera::genetic_matrix_conversion_json(converted);
      if (include_model) body = discovery_model_json(std::move(body), converted.net,
          converted.markings, *session, result.diagnostics.computed);
      response.set_content(body, "application/json");
    } else {
      auto body = minera::genetic_miner_json(result);
      if (include_model) body = discovery_model_json(std::move(body), result.net,
          result.markings, *session, result.diagnostics.computed);
      response.set_content(body, "application/json");
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
bool transformation_source_requested(const httplib::Request& request,
                                      const MinedSession& session);

struct PreparationLogView {
  std::string id;
  std::string name;
  const minera::EventLog* log;
};

// An XES export embedded in a larger response, as `"<field>":<xes>,
// "<field>_error":null`. When the writer refuses the log (text XML cannot
// carry, a native NA attribute) the XES is null and the error field holds the
// message, so the route keeps its main result (as the workbench routes do with
// xes_error).
template <class Write>
std::string embedded_xes_fields(const std::string& field, Write&& write) {
  const auto quote = minera::embedding_runtime::detail::escape_json;
  std::string xes;
  try {
    xes = write();
  } catch (const std::exception& error) {
    return "\"" + field + "\":null,\"" + field + "_error\":" + quote(error.what());
  }
  return "\"" + field + "\":" + quote(xes) + ",\"" + field + "_error\":null";
}

std::string preparation_json(const MinedSession& session, std::string result,
                             const std::vector<PreparationLogView>& outputs = {}) {
  const auto quote = minera::embedding_runtime::detail::escape_json;
  std::string body = "{\"family\":\"LOG_PREPARATION\",\"source_id\":" +
      quote(std::to_string(session.query_source_id)) + ",\"result\":" + result +
      ",\"outputs\":[";
  for (std::size_t i = 0; i < outputs.size(); ++i) {
    if (i) body += ',';
    const auto& output = outputs[i];
    std::string activity_attribute = "concept:name";
    bool complete = true, first = true;
    for (const auto& trace : output.log->traces) for (const auto& event : trace.events) {
      const auto key = event.activity_source_key.empty() ? "concept:name" : event.activity_source_key;
      if (first) { activity_attribute = key; first = false; }
      complete = complete && !event.activity.empty() && key == activity_attribute;
    }
    minera::XesExportOptions options;  // nulls omitted, as pm4py writes None/NaN
    body += "{\"id\":" + quote(output.id) + ",\"name\":" + quote(output.name) +
        ",\"case_count\":" + std::to_string(output.log->trace_count()) +
        ",\"event_count\":" + std::to_string(output.log->event_count()) +
        ",\"activity_attribute\":" + quote(activity_attribute) +
        ",\"complete_activity_field\":" + std::string(complete ? "true" : "false") + ',' +
        embedded_xes_fields("xes", [&] { return minera::serialize_xes(*output.log, options); }) + '}';
  }
  body += "]}";
  std::lock_guard<std::mutex> lock(g_session_mutex);
  if (!g_session || g_session->query_source_id != session.query_source_id)
    throw std::invalid_argument("the source log changed during preparation; run again");
  return body;
}

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
        "log_conversion_max_graph_edges", "source_id", "include_source"};
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
    const bool include_source = transformation_source_requested(request, *session);
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
    const auto result = [&]() -> minera::LogConversionResult {
      if (!session->active_source) return minera::convert_event_log(require_prepared_log(*session),options);
      const auto data = session->active_source->selected_view();
      minera::LogConversionResult converted; converted.variant = options.variant;
      switch (options.variant) {
        case minera::LogConversionVariant::kToEventLog: converted.log = minera::event_data_to_event_log(data,options); break;
        case minera::LogConversionVariant::kToEventStream: converted.stream = minera::event_data_to_event_stream(data,options); break;
        case minera::LogConversionVariant::kToDataFrame:
          converted.table = minera::event_data_to_table(data,options);
          if(options.lazy_output) converted.lazy_table = minera::event_data_to_lazy_table(data,options);
          break;
        default: return minera::convert_event_log(require_prepared_log(*session),options);
      }
      return converted;
    }();
    auto body = minera::log_conversion_json(result);
    if (session->active_source && result.variant == minera::LogConversionVariant::kToDataFrame) {
      body.pop_back(); body += ",\"native_table\":" + minera::serialize_event_table_input_json(result.table) + '}';
    }
    if (include_source) {
      const bool log_output = result.variant == minera::LogConversionVariant::kToEventLog ||
          result.variant == minera::LogConversionVariant::kOneTracePerVariant ||
          result.variant == minera::LogConversionVariant::kAllTracesPerVariant;
      body = preparation_json(*session, std::move(body), log_output
          ? std::vector<PreparationLogView>{{"log", "Converted log", &result.log}}
          : std::vector<PreparationLogView>{});
    }
    response.set_content(body, "application/json");
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
    const auto result = minera::discover_organizational_sna(require_prepared_log(*session), options);
    response.set_content(minera::organizational_sna_json(result),
                         "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

bool organizational_source_requested(const httplib::Request& request,
                                     const MinedSession& session) {
  if (request.has_param("source_id") && request.get_param_value("source_id") !=
      std::to_string(session.query_source_id))
    throw std::invalid_argument("the source log changed; refresh the organizational source");
  if (!request.has_param("include_source")) return false;
  const auto value = request.get_param_value("include_source");
  if (value != "true" && value != "false")
    throw std::invalid_argument("include_source must be true or false");
  if (value == "true" && !request.has_param("source_id"))
    throw std::invalid_argument("source_id is required for organizational analysis");
  return value == "true";
}

std::string organizational_response(const httplib::Request& request,
                                   const MinedSession& session,
                                   std::string payload, bool include_source) {
  if (request.has_param("source_id")) {
    std::lock_guard<std::mutex> lock(g_session_mutex);
    if (!g_session || g_session->query_source_id != session.query_source_id)
      throw std::invalid_argument("the source log changed during organizational analysis; refresh and run again");
  }
  if (!include_source) return payload;
  if (payload.empty() || payload.back() != '}')
    throw std::logic_error("organizational response is not a JSON object");
  const auto quote = minera::embedding_runtime::detail::escape_json;
  payload.pop_back();
  return payload + ",\"input_context\":{\"source_id\":" + quote(std::to_string(session.query_source_id)) +
      ",\"dataset\":" + quote(session.dataset) + ",\"case_count\":" + std::to_string(require_prepared_log(session).trace_count()) +
      ",\"event_count\":" + std::to_string(require_prepared_log(session).event_count()) + "}}";
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
      "network_analysis_business_hour_slots", "network_analysis_max_rows", "network_analysis_max_values", "source_id", "include_source"};
    for (const auto& [key, value] : request.params) { (void)value; if (!allowed.contains(key)) throw std::invalid_argument("unknown network analysis parameter: " + key); }
    const auto lookup = [&](std::string_view n) { const auto k = std::string(n); return request.has_param(k) ? request.get_param_value(k) : std::string{}; };
    const auto present = [&](std::string_view n) { return request.has_param(std::string(n)); };
    const auto options = minera::parse_network_analysis_params(lookup, present);
    const bool include_source = organizational_source_requested(request, *session);
    response.set_content(organizational_response(request, *session,
        minera::network_analysis_json(minera::discover_network_analysis(require_prepared_log(*session), options)), include_source), "application/json");
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
        "link_analysis_max_links", "source_id", "include_source"};
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
    const bool include_source = transformation_source_requested(request, *session);
    const auto options = minera::parse_link_analysis_params(lookup, present);
    auto payload = minera::link_analysis_json(
        minera::discover_link_analysis(require_prepared_log(*session), options));
    if (include_source) {
      payload.pop_back();
      payload += ",\"source_id\":" + minera::embedding_runtime::detail::escape_json(
          std::to_string(session->query_source_id)) + '}';
      std::lock_guard<std::mutex> lock(g_session_mutex);
      if (!g_session || g_session->query_source_id != session->query_source_id)
        throw std::invalid_argument("the source log changed during link analysis; run again");
    }
    response.set_content(payload, "application/json");
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
    static const std::set<std::string> allowed = {"roles_variant", "roles_activity_attribute", "roles_resource_attribute", "roles_threshold", "roles_max_resources", "roles_max_activities", "source_id", "include_source"};
    for (const auto& [key, value] : request.params) { (void)value; if (!allowed.contains(key)) throw std::invalid_argument("unknown roles parameter: " + key); }
    const auto lookup = [&](std::string_view n) { const auto k = std::string(n); return request.has_param(k) ? request.get_param_value(k) : std::string{}; };
    const auto present = [&](std::string_view n) { return request.has_param(std::string(n)); };
    const auto options = minera::parse_role_discovery_params(lookup, present);
    const bool include_source = organizational_source_requested(request, *session);
    response.set_content(organizational_response(request, *session,
        minera::role_discovery_json(minera::discover_roles(require_prepared_log(*session), options)), include_source), "application/json");
  } catch (const std::exception& error) { response.status = 400; response.set_content(error_json(error.what()), "application/json"); }
}

void handle_resource_profiles(const httplib::Request& request, httplib::Response& response) {
  try {
    std::shared_ptr<const MinedSession> session;
    { std::lock_guard<std::mutex> lock(g_session_mutex); session = g_session; }
    if (!session) { response.status = 409; response.set_content(error_json("no cached log; run /api/mine first"), "application/json"); return; }
    static const std::set<std::string> allowed = {"resource_profiles_metric", "resource_profiles_activity_attribute", "resource_profiles_resource_attribute", "resource_profiles_timestamp_attribute", "resource_profiles_start_timestamp_attribute", "resource_profiles_case_id_attribute", "resource_profiles_t1", "resource_profiles_t2", "resource_profiles_resource", "resource_profiles_activity", "resource_profiles_resource2", "resource_profiles_max_resources", "resource_profiles_max_rows", "source_id", "include_source"};
    for (const auto& [key, value] : request.params) { (void)value; if (!allowed.contains(key)) throw std::invalid_argument("unknown resource profile parameter: " + key); }
    const auto lookup = [&](std::string_view n) { const auto k = std::string(n); return request.has_param(k) ? request.get_param_value(k) : std::string{}; };
    const auto present = [&](std::string_view n) { return request.has_param(std::string(n)); };
    const auto options = minera::parse_resource_profiles_params(lookup, present);
    const bool include_source = organizational_source_requested(request, *session);
    response.set_content(organizational_response(request, *session,
        minera::resource_profiles_json(minera::discover_resource_profiles(require_prepared_log(*session), options)), include_source), "application/json");
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

#include "knowledge_exact_route.hpp"

// Relays a runtime reply only when it is a JSON object with a client-facing
// status; anything else (HTML error pages, truncated JSON, redirects) becomes
// a 502 JSON error that names the runtime and its status.
void forward_runtime_response(httplib::Response& response, const httplib::Result& result,
                              std::string_view runtime) {
  const int status = result->status;
  const bool usable_status = (status >= 200 && status < 300) || (status >= 400 && status <= 599);
  if (!usable_status || !minera::llm_runtime::is_json_object(result->body)) {
    response.status = 502;
    response.set_content(error_json(std::string(runtime) + " returned an invalid response (HTTP " +
                                    std::to_string(status) + ", expected a JSON object)"),
                         "application/json");
    return;
  }
  response.status = status;
  response.set_content(result->body, "application/json");
}

// Response headers a cross-origin page may read (fetch hides every header
// outside the CORS-safelisted ones unless the server exposes it): the
// process-tree playout truncation flag and the filtered-log counts.
constexpr const char* kExposedResponseHeaders =
    "X-Minera-Truncated, X-Minera-Case-Count, X-Minera-Event-Count";

bool llm_origin_allowed(const httplib::Request& request, httplib::Response& response) {
  if (!request.has_header("Origin")) return true;
  const auto origin = request.get_header_value("Origin");
  if (!g_allowed_origins.contains(origin)) {
    response.status = 403;
    response.set_content(error_json("LLM requests must come from the local Minera application"), "application/json");
    return false;
  }
  response.set_header("Access-Control-Allow-Origin", origin);
  response.set_header("Access-Control-Expose-Headers", kExposedResponseHeaders);
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
    forward_runtime_response(response, result, "LLM runtime");
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
    forward_runtime_response(response, result, "LLM runtime");
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
  static const std::set<std::string> allowed={"engine","limit","source_id","mode","format","preserve_nulls"};
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
#include "exact_log_query_route.hpp"

void handle_log_query_schema(const httplib::Request& request,httplib::Response& response){
 try{
  const auto session=query_session(request);const auto table=source_query_schema(*session);const auto quote=minera::embedding_runtime::detail::escape_json;
  std::string body="{\"family\":\"LOG_QUERY_SCHEMA\",\"source_id\":"+quote(std::to_string(session->query_source_id))+",\"dataset\":"+quote(session->dataset)+",\"event_count\":"+std::to_string(table.row_count)+",\"case_count\":"+(session->log_ready ? std::to_string(require_prepared_log(*session).trace_count()) : std::string("null"))+",\"table_name\":\"dataframe\",\"event_index_column\":"+quote(table.event_index_column)+",\"sqlite_available\":"+(minera::sqlite_log_query_available()?"true":"false")+",\"duckdb_configured\":"+(!g_query_runtime_url.empty()?"true":"false")+",\"columns\":[";
  for(std::size_t c=0;c<table.columns.size();++c){if(c)body+=',';body+=quote(table.columns[c]);}body+="],\"cluster_runtime_configured\":"+std::string(!g_query_runtime_url.empty()?"true":"false")+"}";response.set_content(body,"application/json");
 }catch(const std::exception& error){response.status=400;response.set_content(error_json(error.what()),"application/json");}
}
void handle_log_query(const httplib::Request& request,httplib::Response& response){
 if(exact_log_query_requested(request)){handle_exact_log_query(request,response,false);return;}
 try{
  validate_query_fields(request);const auto session=query_session(request);const auto table=cached_source_query_table(*session);const auto engine=query_engine(request);const auto limit=query_limit(request,1000);const auto source=std::to_string(session->query_source_id);
  if(engine=="sqlite"){
   minera::LogQueryOptions options;options.max_rows=limit;const auto result=minera::query_log_sqlite(table,request.body,options);auto body=minera::log_query_json(result);body.pop_back();body+=",\"source_id\":\""+source+"\"}";response.set_content(body,"application/json");
  }else{
   if(g_query_runtime_url.empty())throw std::invalid_argument("DuckDB requires --query-runtime-url and scripts/query_runtime.py");
   const auto body=duckdb_query_body(table,request.body,limit,source);const auto result=minera::query_runtime::request(g_query_runtime_url,"/query",&body);forward_runtime_response(response,result,"query runtime");
  }
 }catch(const std::exception& error){response.status=400;response.set_content(error_json(error.what()),"application/json");}
}
void handle_log_query_export(const httplib::Request& request,httplib::Response& response){
 if(exact_log_query_requested(request)){handle_exact_log_query(request,response,true);return;}
 try{
  validate_query_fields(request);if(!request.has_param("source_id"))throw std::invalid_argument("source_id is required for filtered log export");
  const auto session=query_session(request);const auto table=cached_source_query_table(*session);const auto engine=query_engine(request);std::vector<std::size_t> indices;
  if(engine=="sqlite"){
   minera::LogQueryOptions options;options.max_rows=1000000;options.max_output_bytes=256u*1024u*1024u;const auto result=minera::query_log_sqlite(table,request.body,options);if(!result.can_filter||result.output_truncated)throw std::invalid_argument("filter SQL must return valid event indices for a complete selection");indices=result.selected_event_indices;
  }else{
   if(g_query_runtime_url.empty())throw std::invalid_argument("DuckDB requires --query-runtime-url");
   const auto body=duckdb_query_body(table,request.body,1000000,std::to_string(session->query_source_id));const auto result=minera::query_runtime::request(g_query_runtime_url,"/select",&body);if(result->status!=200){forward_runtime_response(response,result,"query runtime");if(response.status<400){response.status=502;response.set_content(error_json("query runtime returned HTTP "+std::to_string(result->status)+" for a filter selection"),"application/json");}return;}indices=minera::query_runtime::parse_indices(result->body,table.table.rows.size());
  }
  const auto mode=request.has_param("mode")?request.get_param_value("mode"):"cases";if(mode!="cases"&&mode!="events")throw std::invalid_argument("filter mode must be cases or events");
  const auto format=request.has_param("format")?request.get_param_value("format"):"xes";
  if(format=="json"){
    const auto body=active_source_table_json(*session,indices,mode=="cases");
    response.set_header("X-Minera-Event-Count",std::to_string(active_source_selected_row_count(*session,indices,mode=="cases")));
    if(session->log_ready){const auto selected=select_active_source_rows(*session,indices,mode=="cases");response.set_header("X-Minera-Case-Count",std::to_string(selected.trace_count()));}
    response.set_content(body,"application/json");
  }else if(format=="xes"){
    const auto filtered=select_active_source_rows(*session,indices,mode=="cases");
    response.set_header("X-Minera-Event-Count",std::to_string(filtered.event_count()));response.set_header("X-Minera-Case-Count",std::to_string(filtered.trace_count()));
    minera::XesExportOptions options;options.preserve_nulls=ocel_query_bool(request,"preserve_nulls",false);options.standard_keys=standard_keys_for_session(*session);options.utc_timestamps=options.standard_keys;/* pandas-normalized UTC clock (W1-2) */response.set_content(minera::serialize_xes(filtered,options),"application/xml");
  }else throw std::invalid_argument("filter export format must be xes or json");
 }catch(const std::exception& error){response.status=400;response.set_content(error_json(error.what()),"application/json");}
}

void handle_log_cluster(const httplib::Request& request,httplib::Response& response){
 try{
  static const std::set<std::string> allowed={"source_id","name","format","activity_attribute","preserve_nulls"};
  for(const auto& [key,value]:request.params){(void)value;if(!allowed.contains(key)||request.params.count(key)!=1)throw std::invalid_argument("unknown or duplicate cluster parameter: "+key);}
  if(!request.has_param("source_id"))throw std::invalid_argument("source_id is required for case clustering");
  if(request.body.size()>1024u*1024u)throw std::invalid_argument("cluster regex exceeds 1 MiB");
  if(g_query_runtime_url.empty()){response.status=503;response.set_content(error_json("case clustering requires --query-runtime-url and scripts/query_runtime.py"),"application/json");return;}
  const auto session=query_session(request);const auto name=request.has_param("name")?request.get_param_value("name"):"";const auto activity=request.has_param("activity_attribute")?request.get_param_value("activity_attribute"):"";
  const auto indices=minera::log_cluster_runtime::select(g_query_runtime_url,require_prepared_log(*session),request.body,activity);const auto filtered=minera::filter_query_events(require_prepared_log(*session),indices,true);
  response.set_header("X-Minera-Event-Count",std::to_string(filtered.event_count()));response.set_header("X-Minera-Case-Count",std::to_string(filtered.trace_count()));
  const auto format=request.has_param("format")?request.get_param_value("format"):"summary";
  if(format=="summary")response.set_content(minera::log_cluster_runtime::summary(filtered,name,request.body,indices,std::to_string(session->query_source_id)),"application/json");
  else if(format=="xes"){minera::XesExportOptions options;options.preserve_nulls=ocel_query_bool(request,"preserve_nulls",false);options.standard_keys=standard_keys_for_session(*session);options.utc_timestamps=options.standard_keys;/* pandas-normalized UTC clock (W1-2) */response.set_content(minera::serialize_xes(filtered,options),"application/xml");}
  else if(format=="json")response.set_content(active_source_table_json_from_log_indices(*session,indices,true),"application/json");
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
   result=minera::build_llm_context(require_prepared_log(*session),options,net,document);
  }
  response.set_content(minera::llm_context_json(result,source),"application/json");
 }catch(const std::exception& error){response.status=400;response.set_content(error_json(error.what()),"application/json");}
}

void handle_local_diagnostics(const httplib::Request& request, httplib::Response& response) {
  try {
    std::shared_ptr<const MinedSession> session;
    { std::lock_guard<std::mutex> lock(g_session_mutex); session = g_session; }
    if (!session) { response.status = 409; response.set_content(error_json("no cached log; run /api/mine first"), "application/json"); return; }
    static const std::set<std::string> allowed = {"local_diagnostics_group_attribute", "local_diagnostics_resource_attribute", "local_diagnostics_activity_attribute", "local_diagnostics_groups", "local_diagnostics_groups_json", "local_diagnostics_max_groups", "local_diagnostics_max_resources", "local_diagnostics_max_rows", "source_id", "include_source"};
    for (const auto& [key, value] : request.params) { (void)value; if (!allowed.contains(key)) throw std::invalid_argument("unknown local diagnostics parameter: " + key); }
    const auto lookup = [&](std::string_view n) { const auto k = std::string(n); return request.has_param(k) ? request.get_param_value(k) : std::string{}; };
    const auto present = [&](std::string_view n) { return request.has_param(std::string(n)); };
    const auto options = minera::parse_local_diagnostics_params(lookup, present);
    const bool include_source = organizational_source_requested(request, *session);
    response.set_content(organizational_response(request, *session,
        minera::local_diagnostics_json(minera::discover_local_diagnostics(require_prepared_log(*session), options)), include_source), "application/json");
  } catch (const std::exception& error) { response.status = 400; response.set_content(error_json(error.what()), "application/json"); }
}

bool clustering_cases_requested(const httplib::Request& request,
                                const MinedSession& session) {
  if (request.has_param("source_id") && request.get_param_value("source_id") !=
      std::to_string(session.query_source_id))
    throw std::invalid_argument("the source log changed; refresh the clustering source");
  if (!request.has_param("include_cases")) return false;
  const auto value = request.get_param_value("include_cases");
  if (value != "true" && value != "false")
    throw std::invalid_argument("include_cases must be true or false");
  if (value == "false") return false;
  if (!request.has_param("source_id"))
    throw std::invalid_argument("source_id is required for clustering case analysis");
  return true;
}

std::string clustering_analysis_json(std::shared_ptr<ClusteringAnalysis> analysis,
                                     std::string result_json) {
  const auto quote = minera::embedding_runtime::detail::escape_json;
  if (analysis->profiles) {
    // The optional workflow includes native centroids; legacy JSON stays stable.
    std::ostringstream centroids;
    centroids.precision(std::numeric_limits<double>::max_digits10);
    centroids << '[';
    const auto& rows = analysis->profiles->centroids;
    for (std::size_t i = 0; i < rows.size(); ++i) {
      if (i) centroids << ',';
      centroids << '[';
      for (std::size_t j = 0; j < rows[i].size(); ++j) {
        if (j) centroids << ',';
        centroids << rows[i][j];
      }
      centroids << ']';
    }
    centroids << ']';
    result_json.pop_back();
    result_json += ",\"centroids\":" + centroids.str() + '}';
  }
  analysis->id = std::to_string(++g_clustering_sequence);
  std::string body = "{\"family\":\"CLUSTER_ANALYSIS\",\"analysis_id\":" + quote(analysis->id) +
      ",\"source_id\":" + quote(std::to_string(analysis->source->query_source_id)) +
      ",\"result\":" + result_json + ",\"cases\":[";
  for (std::size_t i = 0; i < require_prepared_log(*analysis->source).traces.size(); ++i) {
    if (i) body += ',';
    const auto& trace = require_prepared_log(*analysis->source).traces[i];
    body += "{\"index\":" + std::to_string(i) + ",\"case_id\":" + quote(trace.case_id) +
        ",\"event_count\":" + std::to_string(trace.events.size()) + '}';
  }
  body += "],\"groups\":[";
  for (std::size_t i = 0; i < analysis->groups.size(); ++i) {
    if (i) body += ',';
    const auto& group = analysis->groups[i];
    body += "{\"name\":" + quote(group.name) + ",\"trace_indices\":[";
    for (std::size_t j = 0; j < group.trace_indices.size(); ++j) {
      if (j) body += ',';
      body += std::to_string(group.trace_indices[j]);
    }
    body += "]}";
  }
  body += "]}";
  std::lock_guard<std::mutex> lock(g_session_mutex);
  if (!g_session || g_session->query_source_id != analysis->source->query_source_id)
    throw std::invalid_argument("the source log changed during clustering; run again");
  g_clustering_analysis = std::move(analysis);
  return body;
}

std::size_t clustering_index(std::string_view text) {
  std::size_t value = 0;
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
  if (text.empty() || parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
    throw std::invalid_argument("invalid clustering selection index");
  return value;
}

void handle_clustering_export(const httplib::Request& request,
                              httplib::Response& response) {
  try {
    static const std::set<std::string> allowed = {"source_id", "analysis_id", "selection", "format", "preserve_nulls"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key)) throw std::invalid_argument("unknown clustering export parameter: " + key);
    }
    for (const auto key : {"source_id", "analysis_id", "selection"})
      if (!request.has_param(key)) throw std::invalid_argument(std::string(key) + " is required for clustering export");
    std::shared_ptr<const ClusteringAnalysis> analysis;
    {
      std::lock_guard<std::mutex> lock(g_session_mutex);
      analysis = g_clustering_analysis;
      if (!analysis || analysis->id != request.get_param_value("analysis_id"))
        throw std::invalid_argument("the clustering analysis was replaced; run clustering again");
      if (!g_session || request.get_param_value("source_id") != std::to_string(g_session->query_source_id) ||
          g_session->query_source_id != analysis->source->query_source_id)
        throw std::invalid_argument("the source log changed; run clustering again");
    }
    const auto selection = request.get_param_value("selection");
    std::vector<std::size_t> indices;
    if (analysis->hierarchy) {
      if (selection.empty() || selection[0] != 'r') throw std::invalid_argument("invalid hierarchy selection");
      const auto* node = &analysis->hierarchy->tree;
      std::size_t position = 1;
      while (position < selection.size()) {
        if (selection[position++] != '/') throw std::invalid_argument("invalid hierarchy path");
        const auto end = selection.find('/', position);
        const auto index = clustering_index(std::string_view(selection).substr(position, end == std::string::npos ? end : end - position));
        if (index >= node->children.size()) throw std::invalid_argument("hierarchy selection is out of range");
        node = &node->children[index];
        position = end == std::string::npos ? selection.size() : end;
      }
      std::set<std::string> leaves;
      std::vector<const minera::TraceAttributeClusteringNode*> pending{node};
      while (!pending.empty()) {
        const auto* current = pending.back(); pending.pop_back();
        if (current->leaf && current->size) leaves.insert(current->name);
        for (const auto& child : current->children) pending.push_back(&child);
      }
      for (const auto& group : analysis->groups) if (leaves.contains(group.name))
        indices.insert(indices.end(), group.trace_indices.begin(), group.trace_indices.end());
    } else {
      if (selection.rfind("c/", 0) != 0) throw std::invalid_argument("invalid profile cluster selection");
      const auto index = clustering_index(std::string_view(selection).substr(2));
      if (!analysis->profiles || index >= analysis->profiles->clusters.size())
        throw std::invalid_argument("profile cluster selection is out of range");
      indices = analysis->profiles->clusters[index];
    }
    std::sort(indices.begin(), indices.end());
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
    auto selected = require_prepared_log(*analysis->source);
    selected.traces.clear();
    for (const auto index : indices) selected.traces.push_back(require_prepared_log(*analysis->source).traces.at(index));
    const auto format = request.has_param("format") ? request.get_param_value("format") : "xes";
    response.set_header("X-Minera-Case-Count", std::to_string(selected.trace_count()));
    response.set_header("X-Minera-Event-Count", std::to_string(selected.event_count()));
    if (format == "xes") {
      minera::XesExportOptions options;
      options.preserve_nulls = ocel_query_bool(request, "preserve_nulls", false);
      // A download read by pm4py and reopened with default selectors.
      options.standard_keys = standard_keys_for_session(*analysis->source);
      options.utc_timestamps = options.standard_keys;  // pandas-normalized UTC clock
      response.set_content(minera::serialize_xes(selected, options), "application/xml");
    } else if (format == "json") {
      std::vector<std::size_t> event_indices;
      std::size_t ordinal = 0;
      for (std::size_t trace_index = 0; trace_index < require_prepared_log(*analysis->source).traces.size(); ++trace_index) {
        const auto& trace = require_prepared_log(*analysis->source).traces[trace_index];
        const bool included = std::binary_search(indices.begin(), indices.end(), trace_index);
        for (std::size_t event_index = 0; event_index < trace.events.size(); ++event_index, ++ordinal)
          if (included) event_indices.push_back(ordinal);
      }
      response.set_content(active_source_table_json_from_log_indices(*analysis->source,event_indices,true), "application/json");
    } else throw std::invalid_argument("clustering export format must be xes or json");
  } catch (const std::exception& error) {
    response.status = 400; response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_clustering(const httplib::Request& request, httplib::Response& response) {
  try {
    std::shared_ptr<const MinedSession> session;
    { std::lock_guard<std::mutex> lock(g_session_mutex); session = g_session; }
    if (!session) { response.status=409; response.set_content(error_json("no cached log; run /api/mine first"),"application/json"); return; }
    static const std::set<std::string> allowed={"clustering_trace_attribute","clustering_activity_attribute","clustering_variant","clustering_alpha","clustering_percent","clustering_max_groups","clustering_max_output_nodes","source_id","include_cases"};
    for (const auto& [k,v] : request.params) { (void)v; if(!allowed.contains(k)) throw std::invalid_argument("unknown clustering parameter: "+k); }
    const auto lookup=[&](std::string_view n){const auto k=std::string(n); return request.has_param(k)?request.get_param_value(k):std::string{};};
    const auto present=[&](std::string_view n){return request.has_param(std::string(n));};
    const bool include_cases = clustering_cases_requested(request, *session);
    const auto o=minera::parse_clustering_params(lookup,present);
    const auto result = minera::discover_trace_attribute_clustering(require_prepared_log(*session),o);
    auto body = minera::trace_attribute_clustering_json(result);
    if (include_cases) {
      auto analysis = std::make_shared<ClusteringAnalysis>();
      analysis->source = session; analysis->hierarchy = result;
      analysis->groups = minera::trace_attribute_clustering_groups(require_prepared_log(*session), o.trace_attribute);
      body = clustering_analysis_json(std::move(analysis), std::move(body));
    }
    response.set_content(body,"application/json");
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
        "profile_feature_names", "profile_engine", "source_id", "include_cases"};
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
    const bool include_cases = clustering_cases_requested(request, *session);
    // The reference SKLEARN_PROFILES pipeline unless profile_engine=native.
    auto profile_request = minera::parse_profile_clustering_request(lookup, present);
    profile_request.options.standard_keys = standard_keys_for_session(*session);
    const auto& options = profile_request.options;
    if (options.cluster_count > kServerProfileMaxClusters ||
        options.max_iterations > kServerProfileMaxIterations ||
        options.max_traces > kServerProfileMaxTraces ||
        options.max_features > kServerProfileMaxFeatures) {
      throw std::invalid_argument("profile clustering safety bound exceeded");
    }
    const auto result = minera::run_profile_clustering(require_prepared_log(*session), profile_request);
    auto body = minera::profile_clustering_json(result);
    if (include_cases) {
      auto analysis = std::make_shared<ClusteringAnalysis>();
      analysis->source = session; analysis->profiles = result;
      body = clustering_analysis_json(std::move(analysis), std::move(body));
    }
    response.set_content(body, "application/json");
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
            minera::discover_chaotic_activities(require_prepared_log(*session), options)),
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
    auto options = minera::parse_passed_time_params(lookup, present);
    // A CSV or XES upload counts business hours on pm4py's DataFrame (UTC)
    // clock; a native table source on each timestamp's own wall clock.
    options.dataframe = dataframe_performance_source(session->active_source.get());
    response.set_content(
        minera::passed_time_json(
            minera::discover_passed_time(require_prepared_log(*session), options)),
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
    auto options = minera::parse_service_time_params(lookup, present);
    options.dataframe = dataframe_performance_source(session->active_source.get());
    response.set_content(
        minera::service_time_json(
            minera::discover_service_time(require_prepared_log(*session), options)),
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
        minera::rework_json(minera::discover_rework(require_prepared_log(*session), options)),
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
            minera::discover_cycle_time(require_prepared_log(*session), options)),
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
    auto options = minera::parse_case_arrival_params(lookup, present);
    // A CSV or XES upload follows pm4py's DataFrame path; a native table
    // source follows the EventLog path.
    options.dataframe = !session->active_source ||
                        session->active_source->file().format == "csv" ||
                        session->active_source->file().format == "xes";
    response.set_content(
        minera::case_arrival_json(
            minera::discover_case_arrival(require_prepared_log(*session), options)),
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
    auto options = minera::parse_case_statistics_params(lookup, present);
    options.dataframe = dataframe_performance_source(session->active_source.get());
    response.set_content(
        minera::case_statistics_json(
            minera::discover_case_statistics(require_prepared_log(*session), options)),
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
    auto options =
        minera::parse_attribute_statistics_params(lookup, present);
    // CSV and XES uploads reach pm4py as a DataFrame (column dtypes); a typed
    // table source already carries each cell's frame type, and its nullable
    // integer columns keep ints (review Z1-7).
    options.dataframe_column_types =
        !session->active_source || session->active_source->file().format != "table";
    response.set_content(
        minera::attribute_statistics_json(
            minera::discover_attribute_statistics(require_prepared_log(*session), options)),
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
            minera::discover_event_distribution(require_prepared_log(*session), options)),
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
            minera::discover_attribute_kde(require_prepared_log(*session), options)),
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
            minera::discover_boundary_activities(require_prepared_log(*session), options)),
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
            minera::discover_variant_statistics(require_prepared_log(*session), options)),
        "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

bool anonymization_release_requested(const httplib::Request& request,
                                     const MinedSession& session) {
  if (request.has_param("source_id") &&
      request.get_param_value("source_id") !=
          std::to_string(session.query_source_id))
    throw std::invalid_argument("the source log changed; refresh the anonymization workspace");
  if (!request.has_param("include_xes")) return false;
  const auto value = request.get_param_value("include_xes");
  if (value != "true" && value != "false")
    throw std::invalid_argument("include_xes must be true or false");
  if (value == "false") return false;
  if (!request.has_param("source_id"))
    throw std::invalid_argument("source_id is required for an anonymization release");
  return true;
}

// Serialize the exact output of one run. Exporting or reopening a secure-random
// release must never rerun the algorithm and draw a different output.
std::string anonymization_release_json(std::string result_json,
                                       const minera::EventLog& log,
                                       const MinedSession& session) {
  const auto quote = minera::embedding_runtime::detail::escape_json;
  minera::XesExportOptions options;  // nulls omitted, as pm4py writes None/NaN
  options.standard_keys = standard_keys_for_session(session);  // downloaded, and reopened with default selectors
  options.utc_timestamps = options.standard_keys;  // pandas-normalized UTC clock
  return "{\"family\":\"ANONYMIZATION_RELEASE\",\"source_id\":" +
         quote(std::to_string(session.query_source_id)) + ",\"result\":" +
         result_json + ',' +
         embedded_xes_fields("released_xes", [&] { return minera::serialize_xes(log, options); }) + '}';
}

// Cached-session trace-variant query adapter; the source session stays intact.
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
        "tvq_max_output_variants", "source_id", "include_xes"};
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
    const bool release = anonymization_release_requested(request, *session);
    const auto options = minera::parse_trace_variant_query_params(lookup, present);
    if (options.max_output_traces > kServerTvqMaxOutputTraces)
      throw std::invalid_argument("tvq_max_output_traces exceeds the server safety cap");
    if (options.max_output_variants > kServerTvqMaxOutputVariants)
      throw std::invalid_argument("tvq_max_output_variants exceeds the server safety cap");
    const auto result = minera::anonymize_trace_variant_query(require_prepared_log(*session), options);
    auto body = minera::trace_variant_query_json(result);
    if (release) body = anonymization_release_json(std::move(body), result.log, *session);
    response.set_content(body, "application/json");
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
        "pripel_max_distance", "source_id", "include_xes"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument("unknown PRIPEL parameter: " + key);
    }
    const bool release = anonymization_release_requested(request, *session);
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
    // PRIPEL's trace_variant_query parameter is an EventLog object (the
    // reference reads its traces as given), so its XES keeps every trace.
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
    const auto result = minera::anonymize_pripel(require_prepared_log(*session), query, options);
    auto body = minera::pripel_json(result);
    if (release) body = anonymization_release_json(std::move(body), result.log, *session);
    response.set_content(body, "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

// An imported XES frame's cases in case-id order (order_cases_by_id).
minera::EventLog xes_frame_cases_by_id(minera::EventLog log) {
  minera::order_cases_by_id(log);
  return log;
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
    // pm4py replays a frame's cases in case-id order (a CSV import already
    // lists them so) and lists the results in it.
    const auto log = looks_like_xes(file.filename, file.content)
        ? xes_frame_cases_by_id(minera::import_xes_dataframe_log(input, params.xes))
        : minera::import_csv_event_log(input, params.csv);
    auto import_options = minera::tools::pnml_model_import_options();
    import_options.import_stochastic_map = false;
    const auto document = minera::parse_pnml_document(
        request.get_file_value("model").content, import_options);
    auto options = minera::tools::default_replay_input_options(
        log, minera::tools::pnml_replay_options(document, params.token_replay),
        params.token_replay_activity_attribute_supplied);
    options.cancelled = connection_cancel_hook(request);
    response.set_content(minera::token_replay_result_json(
        minera::replay_token_result(log, document.net, options)), "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

// Independent replay keeps the original uploads and never changes the cached
// mining session. CSV flat inputs use the parser's original rectangular rows.
void handle_token_replay_workbench(const httplib::Request& request,
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
    const auto adapter = minera::parse_token_replay_input_adapter(lookup, present);
    const auto& file = request.get_file_value("log");
    const auto& model_file = request.get_file_value("model");
    auto source_params = params;
    const auto owned_source = prepare_active_event_source(request, file.content, file.filename, source_params);
    const bool typed_source = owned_source.file().format == "table";
    const bool is_xes = owned_source.file().format == "xes";
    const auto source_log = owned_source.checked_log();
    std::optional<minera::EventTable> source_table;
    if (!is_xes) source_table = owned_source.checked_table();
    auto import_options = minera::tools::pnml_model_import_options();
    import_options.import_stochastic_map = false;
    const auto document = minera::parse_pnml_document(model_file.content, import_options);
    auto options = minera::tools::default_replay_input_options(
        source_log, minera::tools::pnml_replay_options(document, params.token_replay),
        params.token_replay_activity_attribute_supplied);
    options.cancelled = connection_cancel_hook(request);
    if (typed_source || present("mining_selectors_json")) {
      if (!present("token_replay_activity_attribute")) options.activity_attribute = owned_source.selectors().activity_key;
      if (!present("token_replay_case_id_attribute")) options.case_id_attribute = owned_source.selectors().case_id_key;
    }
    // Header-only CSV inputs still declare their imported activity field.
    // With no events, the ordinary default helper has no source cell to inspect.
    if (!typed_source && source_table && source_log.event_count() == 0 &&
        !params.token_replay_activity_attribute_supplied) {
      auto selected = std::find(source_table->columns.begin(), source_table->columns.end(),
                                params.csv.activity_column);
      if (selected == source_table->columns.end()) {
        const auto alias = params.csv.activity_column == "activity" ? "concept:name"
            : params.csv.activity_column == "concept:name" ? "activity" : "";
        selected = std::find(source_table->columns.begin(), source_table->columns.end(), alias);
      }
      if (selected != source_table->columns.end()) options.activity_attribute = *selected;
    }
    if (!typed_source && !present("mining_selectors_json") && !is_xes && !present("token_replay_case_id_attribute"))
      options.case_id_attribute = params.csv.case_id_column;
    const bool forward = options.variant == minera::TokenReplayVariant::kForward;
    if (!forward && (options.enable_pltr_fitness || options.return_names ||
                    present("token_replay_disable_variants")))
      throw std::invalid_argument(
          "place/transition fitness, return-names and variant grouping require forward replay");
    std::optional<minera::EventStream> stream;
    std::optional<minera::EventTable> table;
    std::optional<minera::LazyEventTable> lazy;
    minera::EventLog replay_log;
    if (adapter == "log") replay_log = source_log;
    else {
      if (adapter == "stream") {
        stream = source_table ? minera::event_table_to_event_stream(*source_table)
                              : minera::event_log_to_event_stream(source_log);
        replay_log = minera::normalize_token_replay_event_log(minera::EventDataView(*stream), options);
      } else if (adapter == "dataframe") {
        table = source_table ? *source_table : minera::event_log_to_table(source_log);
        replay_log = minera::normalize_token_replay_event_log(minera::EventDataView(*table), options);
      } else {
        lazy = source_table ? minera::LazyEventTable::from_table(*source_table)
                            : minera::event_log_to_lazy_table(source_log);
        replay_log = minera::normalize_token_replay_event_log(minera::EventDataView(*lazy), options);
      }
    }
    std::vector<minera::TokenReplayTraceDiagnostics> rows;
    std::string full_result = "null";
    if (forward) {
      const auto result = minera::replay_token_result(replay_log, document.net, options);
      full_result = minera::token_replay_result_json(result);
      rows = result.traces;
    } else rows = minera::replay_token_traces(replay_log, document.net, options);
    const auto quote = minera::embedding_runtime::detail::escape_json;
    std::string body = "{\"family\":\"TOKEN_REPLAY_WORKFLOW\",\"variant\":" +
        quote(forward ? "forward" : "backwards") + ",\"input_context\":{\"adapter\":" + quote(adapter) +
        ",\"origin\":" + quote(typed_source ? "uploaded_typed_table" : source_table && adapter != "log" ? "uploaded_csv_rows" : "uploaded_grouped_log") +
        ",\"detected_format\":" + quote(owned_source.file().format) +
        ",\"dataset\":" + quote(file.filename) + ",\"model\":" + quote(model_file.filename) +
        ",\"log_bytes\":" + std::to_string(file.content.size()) +
        ",\"model_bytes\":" + std::to_string(model_file.content.size()) +
        ",\"source_case_count\":" + std::to_string(source_log.trace_count()) +
        ",\"source_event_count\":" + std::to_string(source_log.event_count()) +
        ",\"evaluated_case_count\":" + std::to_string(replay_log.trace_count()) +
        ",\"evaluated_event_count\":" + std::to_string(replay_log.event_count()) +
        ",\"activity_attribute\":" + quote(options.activity_attribute) +
        ",\"case_id_attribute\":" + quote(options.case_id_attribute) +
        "},\"model_context\":{\"places\":[";
    for (std::size_t p = 0; p < document.net.place_count(); ++p) {
      if (p) body += ',';
      body += "{\"index\":" + std::to_string(p) + ",\"id\":" + quote(document.place_ids.at(p)) +
          ",\"initial_tokens\":" + minera::TokenInteger(document.initial_marking.at(p)).decimal() +
          ",\"final_tokens\":" + minera::TokenInteger(document.final_marking.at(p)).decimal() +
          ",\"initial_key\":" + (document.initial_marking_keys.at(p) ? "true" : "false") +
          ",\"final_key\":" + (document.final_marking_keys.at(p) ? "true" : "false") + '}';
    }
    body += "],\"transitions\":[";
    for (std::size_t t = 0; t < document.net.transitions().size(); ++t) {
      if (t) body += ',';
      const auto& transition = document.net.transitions()[t];
      body += "{\"index\":" + std::to_string(t) + ",\"id\":" + quote(document.transition_ids.at(t)) +
          ",\"label\":" + (transition.activity ? quote(document.net.activities().at(*transition.activity)) : "null") + '}';
    }
    body += "]},\"full_result\":" + full_result + ",\"diagnostics\":" +
        minera::token_replay_traces_json(rows, options.variant) +
        ",\"input_before\":" + (source_table ? minera::serialize_event_table_input_json(*source_table) : "null") +
        ",\"input_after\":" + (source_table ? minera::serialize_event_table_input_json(*source_table) : "null") +
        ",\"prepared_log\":" + minera::serialize_active_source_log_json(replay_log) + '}';
    response.set_content(active_source_safe_json(body), "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

#include "replay_duration_route.hpp"

// Whether a serialized JSON document holds a number JSON cannot carry: an
// ostream writes an overflowed or undefined double as inf or nan. Outside
// strings the only letters JSON allows are true, false, null and exponents,
// none of which is an i or an n except null.
bool json_has_nonfinite_number(std::string_view json) {
  for (std::size_t i = 0; i < json.size(); ++i) {
    const char c = json[i];
    if (c == '"') {
      for (++i; i < json.size() && json[i] != '"'; ++i)
        if (json[i] == '\\') ++i;
    } else if (c == 'n' && json.substr(i, 4) == "null") {
      i += 3;
    } else if (c == 'i' || c == 'n' || c == 'I' || c == 'N') {
      return true;
    }
  }
  return false;
}

// Alignment costs are finite when parsed, but their totals can overflow.
void reject_nonfinite_alignment_costs(std::string_view json) {
  if (json_has_nonfinite_number(json))
    throw std::invalid_argument(
        "alignment costs are too large: a cost total overflows a double; use smaller costs");
}

// Alignment uses independent original inputs; the cached mining session is
// neither required nor changed. Flat CSV adapters retain the parser's rows.
void handle_petri_alignment_workbench(const httplib::Request& request,
                                      httplib::Response& response) {
  try {
    if (!request.is_multipart_form_data() || !request.has_file("log") ||
        !request.has_file("model"))
      throw std::invalid_argument("Petri alignment requires multipart fields log and model");
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
    auto options = minera::parse_alignment_params(lookup, present);
    options.cancelled = connection_cancel_hook(request);
    const auto adapter = minera::parse_alignment_input_adapter(lookup, present);
    const auto& file = request.get_file_value("log");
    const auto& model_file = request.get_file_value("model");
    auto source_params = params;
    const auto owned_source = prepare_active_event_source(request, file.content, file.filename, source_params);
    const bool typed_source = owned_source.file().format == "table";
    const bool is_xes = owned_source.file().format == "xes";
    const auto source_log = owned_source.checked_log();
    std::optional<minera::EventTable> source_table;
    if (!is_xes) source_table = owned_source.checked_table();
    auto import_options = minera::tools::pnml_model_import_options();
    import_options.import_stochastic_map = false;
    const auto document = minera::parse_pnml_document(model_file.content, import_options);
    if (!present("alignment_initial_marking"))
      options.initial_marking = minera::tools::pnml_marking<int>(document.initial_marking);
    if (!present("alignment_final_marking"))
      options.final_marking = minera::tools::pnml_marking<int>(document.final_marking);
    if (options.initial_marking.size() != document.net.place_count() ||
        options.final_marking.size() != document.net.place_count())
      throw std::invalid_argument("alignment markings require one token count per PNML place");

    const auto input_key = [](std::string_view name) -> std::string_view {
      if (name == "token_replay_activity_attribute") return "alignment_activity_attribute";
      if (name == "token_replay_case_id_attribute") return "alignment_case_id_attribute";
      if (name == "token_replay_max_rows_to_materialize") return "alignment_max_rows_to_materialize";
      return {};
    };
    auto projection = minera::parse_token_replay_params(
        [&](std::string_view name) { const auto key = input_key(name); return key.empty() ? std::string{} : lookup(key); },
        [&](std::string_view name) { const auto key = input_key(name); return !key.empty() && present(key); });
    projection = minera::tools::default_replay_input_options(
        source_log, projection, present("alignment_activity_attribute"));
    if (typed_source || present("mining_selectors_json")) {
      if (!present("alignment_activity_attribute")) projection.activity_attribute = owned_source.selectors().activity_key;
      if (!present("alignment_case_id_attribute")) projection.case_id_attribute = owned_source.selectors().case_id_key;
    }
    if (!typed_source && source_table && source_log.event_count() == 0 && !present("alignment_activity_attribute")) {
      auto selected = std::find(source_table->columns.begin(), source_table->columns.end(), params.csv.activity_column);
      if (selected == source_table->columns.end()) {
        const auto alias = params.csv.activity_column == "activity" ? "concept:name"
            : params.csv.activity_column == "concept:name" ? "activity" : "";
        selected = std::find(source_table->columns.begin(), source_table->columns.end(), alias);
      }
      if (selected != source_table->columns.end()) projection.activity_attribute = *selected;
    }
    if (!typed_source && !present("mining_selectors_json") && !is_xes && !present("alignment_case_id_attribute"))
      projection.case_id_attribute = params.csv.case_id_column;
    options.activity_attribute = projection.activity_attribute;
    minera::EventLog evaluated;
    if (adapter == "log") {
      if (projection.max_rows_to_materialize && source_log.event_count() > *projection.max_rows_to_materialize)
        throw std::length_error("alignment: event materialization cap exceeded");
      evaluated = source_log;
    }
    else if (adapter == "stream") {
      const auto stream = source_table ? minera::event_table_to_event_stream(*source_table)
                                      : minera::event_log_to_event_stream(source_log);
      evaluated = minera::normalize_token_replay_event_log(minera::EventDataView(stream), projection);
    } else if (adapter == "dataframe") {
      const auto table = source_table ? *source_table : minera::event_log_to_table(source_log);
      evaluated = minera::normalize_token_replay_event_log(minera::EventDataView(table), projection);
    } else {
      const auto lazy = source_table ? minera::LazyEventTable::from_table(*source_table)
                                    : minera::event_log_to_lazy_table(source_log);
      evaluated = minera::normalize_token_replay_event_log(minera::EventDataView(lazy), projection);
    }
    auto attribute_projection = minera::tools::evaluation_input_log(evaluated, options.activity_attribute);
    const auto& alignment_log = attribute_projection ? *attribute_projection : evaluated;
    const auto result = minera::check_alignment(alignment_log, document.net, options);
    minera::DirectlyFollowsGraph graph;
    for (const auto& label : document.net.activities()) graph.ensure_activity(label);
    std::vector<std::vector<std::string>> traces;
    for (const auto& trace : alignment_log.traces) {
      std::vector<std::string> names;
      for (const auto& event : trace.events) {
        const auto* field = options.activity_attribute == "activity" || options.activity_attribute == "concept:name"
            ? &event.activity : event.attribute(options.activity_attribute);
        if (!field) throw std::invalid_argument("alignment input is missing activity attribute: " + options.activity_attribute);
        names.push_back(*field);
        graph.ensure_activity(*field);
      }
      traces.push_back(std::move(names));
    }
    const auto quote = minera::embedding_runtime::detail::escape_json;
    std::ostringstream body;
    body.precision(std::numeric_limits<double>::max_digits10);
    body << "{\"family\":\"PETRI_ALIGNMENT_WORKFLOW\",\"input_context\":{\"adapter\":" << quote(adapter)
         << ",\"origin\":" << quote(typed_source ? "uploaded_typed_table" : source_table && adapter != "log" ? "uploaded_csv_rows" : "uploaded_grouped_log")
         << ",\"detected_format\":" << quote(owned_source.file().format)
         << ",\"dataset\":" << quote(file.filename) << ",\"model\":" << quote(model_file.filename)
         << ",\"log_bytes\":" << file.content.size() << ",\"model_bytes\":" << model_file.content.size()
         << ",\"source_case_count\":" << source_log.trace_count() << ",\"source_event_count\":" << source_log.event_count()
         << ",\"evaluated_case_count\":" << alignment_log.trace_count() << ",\"evaluated_event_count\":" << alignment_log.event_count()
         << ",\"activity_attribute\":" << quote(options.activity_attribute)
         << ",\"case_id_attribute\":" << quote(projection.case_id_attribute) << "},\"model_context\":{\"places\":[";
    for (std::size_t p = 0; p < document.net.place_count(); ++p) {
      if (p) body << ',';
      body << "{\"index\":" << p << ",\"id\":" << quote(document.place_ids.at(p))
           << ",\"pnml_initial_tokens\":" << minera::TokenInteger(document.initial_marking.at(p)).decimal()
           << ",\"pnml_final_tokens\":" << minera::TokenInteger(document.final_marking.at(p)).decimal()
           << ",\"effective_initial_tokens\":" << options.initial_marking.at(p)
           << ",\"effective_final_tokens\":" << options.final_marking.at(p)
           << ",\"initial_key\":" << (document.initial_marking_keys.at(p) ? "true" : "false")
           << ",\"final_key\":" << (document.final_marking_keys.at(p) ? "true" : "false") << '}';
    }
    body << "],\"transitions\":[";
    for (std::size_t t = 0; t < document.net.transitions().size(); ++t) {
      if (t) body << ',';
      const auto& transition = document.net.transitions()[t];
      body << "{\"index\":" << t << ",\"id\":" << quote(document.transition_ids.at(t))
           << ",\"label\":" << (transition.activity ? quote(document.net.activities().at(*transition.activity)) : "null") << '}';
    }
    body << "]},\"activities\":[";
    for (std::size_t i = 0; i < graph.activities().size(); ++i) { if (i) body << ','; body << quote(graph.activities()[i]); }
    body << "],\"result\":" << minera::alignment_json(result, graph, std::numeric_limits<double>::max_digits10)
         << ",\"variant_details\":[";
    const auto write_transition = [&](std::size_t transition) {
      if (transition == static_cast<std::size_t>(-1)) body << "null"; else body << transition;
    };
    const auto write_moves = [&](const std::vector<minera::AlignmentMove>& moves) {
      body << '[';
      for (std::size_t i = 0; i < moves.size(); ++i) {
        if (i) body << ',';
        const auto& move = moves[i];
        const bool silent = move.type == minera::AlignmentMoveType::kModel &&
            move.transition < document.net.transitions().size() && document.net.transitions()[move.transition].silent();
        body << "{\"type\":" << quote(move.type == minera::AlignmentMoveType::kSynchronous ? "sync" : move.type == minera::AlignmentMoveType::kLog ? "log" : "model")
             << ",\"activity_name\":" << (silent ? "null" : quote(move.activity)) << ",\"transition\":";
        write_transition(move.transition); body << '}';
      }
      body << ']';
    };
    const auto write_marking = [&](const std::vector<int>& marking) {
      body << '['; for (std::size_t i = 0; i < marking.size(); ++i) { if (i) body << ','; body << marking[i]; } body << ']';
    };
    for (std::size_t i = 0; i < result.variants.size(); ++i) {
      if (i) body << ',';
      const auto& variant = result.variants[i];
      body << "{\"variant_index\":" << i << ",\"worst_cost\":" << variant.worst_cost << ",\"complete_moves\":";
      write_moves(variant.complete_moves);
      body << ",\"projected_model_moves\":[";
      for (std::size_t j = 0; j < variant.projected_model_moves.size(); ++j) {
        if (j) body << ',';
        const auto& move = variant.projected_model_moves[j];
        body << "{\"activity_name\":" << quote(move.activity) << ",\"transition\":";
        write_transition(move.transition); body << ",\"marking_before\":"; write_marking(move.marking_before);
        body << ",\"marking_after\":"; write_marking(move.marking_after); body << '}';
      }
      body << "],\"optimal_alignments\":[";
      for (std::size_t j = 0; j < variant.optimal_alignments.size(); ++j) { if (j) body << ','; write_moves(variant.optimal_alignments[j]); }
      body << "]}";
    }
    body << "],\"trace_groups\":[";
    for (std::size_t i = 0; i < alignment_log.traces.size(); ++i) {
      if (i) body << ',';
      const auto& trace = alignment_log.traces[i];
      body << "{\"trace_index\":" << i << ",\"case_id\":" << quote(trace.case_id)
           << ",\"event_count\":" << trace.events.size() << ",\"activities\":[";
      for (std::size_t j = 0; j < traces[i].size(); ++j) { if (j) body << ','; body << quote(traces[i][j]); }
      body << "],\"variant_index\":";
      const auto match = std::find_if(result.variants.begin(), result.variants.end(), [&](const auto& variant) { return variant.activities == traces[i]; });
      if (match == result.variants.end()) body << "null"; else body << std::distance(result.variants.begin(), match);
      body << '}';
    }
    body << "],\"input_before\":" << (source_table ? minera::serialize_event_table_input_json(*source_table) : "null")
         << ",\"input_after\":" << (source_table ? minera::serialize_event_table_input_json(*source_table) : "null")
         << ",\"prepared_log\":" << minera::serialize_active_source_log_json(alignment_log) << '}';
    reject_nonfinite_alignment_costs(body.str());
    response.set_content(active_source_safe_json(body.str()), "application/json");
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
    // pm4py's token fitness of a frame sums its cases in case-id order.
    const auto log = looks_like_xes(file.filename, file.content)
        ? xes_frame_cases_by_id(minera::import_xes_dataframe_log(input, imports.xes))
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
    if (request.has_param("source_id") && request.get_param_value("source_id") !=
        std::to_string(session->query_source_id))
      throw std::invalid_argument("the source log changed; rediscover the model before checking it");
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
            : (document = request.has_param("pnml_infer_legacy_boundaries")
                  ? minera::tools::resolve_pnml_model(
                      minera::parse_pnml_document(model_xml,
                          minera::tools::pnml_model_import_options(public_markings)),
                      ocel_query_bool(request, "pnml_infer_legacy_boundaries", true) &&
                          !public_markings)
                  : minera::tools::parse_pnml_model(model_xml, public_markings),
               document->net);

    auto next_session = std::make_shared<MinedSession>(*session);
    // A later model import acknowledges the source. A stale cancellation of
    // its original upload must not roll back this subsequent model edit.
    next_session->mining_request_id.clear();
    for (const auto& activity : imported.activities()) {
      next_session->graph.ensure_activity(activity);
    }
    next_session->nets.insert_or_assign("imported", imported);
    next_session->imported_document = document;
    auto replay_options = document
        ? minera::tools::pnml_replay_options(*document, session->token_replay)
        : session->token_replay;
    if (request.has_param("activity_attribute")) {
      const auto selector = request.get_param_value("activity_attribute");
      if (selector.empty()) throw std::invalid_argument("activity_attribute must not be empty");
      replay_options.activity_attribute = selector;
    }
    next_session->imported_activity_attribute = replay_options.activity_attribute;
    std::optional<minera::EventLog> footprint_input;
    if (replay_options.activity_attribute != "activity" &&
        replay_options.activity_attribute != "concept:name") {
      footprint_input = require_prepared_log(*session);
      for (auto& trace : footprint_input->traces) for (auto& event : trace.events) {
        const auto* activity = minera::dfg_event_field_value(event, replay_options.activity_attribute);
        if (!activity) throw std::invalid_argument("missing model activity field: " + replay_options.activity_attribute);
        event.activity = *activity;
      }
    }
    const auto& footprint_log = footprint_input ? *footprint_input : require_prepared_log(*session);
    const auto gated = minera::tools::pnml_conformance_gate(
        imported, document ? &*document : nullptr);
    const auto imported_reachability = gated
        ? minera::FinalMarkingReachability::kReachable
        : document ? minera::final_marking_reachability(imported,
                         replay_options.initial_marking, replay_options.final_marking)
                   : minera::final_marking_reachability(imported);
    minera::ModelConformance conformance = gated ? *gated : minera::ModelConformance{
        "imported", true,
        minera::check_conformance(require_prepared_log(*session), imported, replay_options),
        document ? minera::check_footprints(footprint_log, imported,
                                             replay_options.initial_marking)
                 : minera::check_footprints(footprint_log, imported),
        std::nullopt};
    if (!gated) minera::set_model_soundness(conformance, imported_reachability);
    // pm4py's token replay of an XES frame lists its cases in case-id order
    // (the trace diagnostics of the replayed log; see session_case_order).
    if (!gated)
      minera::order_trace_diagnostics(conformance.token_replay,
          session_case_order(session->active_source.get(), require_prepared_log(*session)));
    auto body =
        minera::imported_model_json(conformance, next_session->graph);
    const auto imported_model_id = ++g_imported_model_sequence;
    next_session->imported_model_id = imported_model_id;

    // Rollback state that pinned the replaced session; destroyed after unlock.
    std::vector<std::shared_ptr<const void>> rollback_garbage;
    {
      std::lock_guard<std::mutex> lock(g_session_mutex);
      // Only publish if no re-mine replaced the session in the meantime. An
      // unpublished import is refused either way: its id never reaches the
      // server's session, so reporting success would mislead the caller.
      if (g_session == session) {
        g_session = std::move(next_session);
        // A mining ticket still holding the replaced session as "published"
        // can no longer roll anything back meaningfully; drop its copies so
        // the old session is freed rather than retained beside the new one.
        forget_mining_rollback_locked(session, rollback_garbage);
      } else {
        response.status = 409;
        response.set_content(error_json(request.has_param("source_id")
            ? "the active session changed during model checking; retry the handoff"
            : "the active session changed during model checking; check the model again"),
                             "application/json");
        return;
      }
    }
    if (!rollback_garbage.empty()) {
      rollback_garbage.clear();
      session.reset();
      minera_release_free_memory();
    }
    // Runs on the imported model echo this id (X-Minera-Imported-Model) so a
    // model replaced while they are pending is detected by the server.
    if (body.size() > 2 && body.front() == '{')
      body.insert(1, "\"imported_model_id\":" + std::to_string(imported_model_id) + ",");
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

  const std::string miner = requested_miner(request);
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
    options.cancelled = connection_cancel_hook(request);
    // A supplied model aligns with the reference's public default variant
    // (state-equation A*) unless alignment_variant chose another.
    if (miner == "imported" && !present("alignment_variant"))
      options.variant = minera::AlignmentVariant::kStateEquationAStar;
    if (miner == "imported" && !request.has_param("alignment_activity_attribute"))
      options.activity_attribute = session->imported_activity_attribute;
    if (miner == "imported" && session->imported_document) {
      if (!present("alignment_initial_marking"))
        options.initial_marking = minera::tools::pnml_marking<int>(session->imported_document->initial_marking);
      if (!present("alignment_final_marking"))
        options.final_marking = minera::tools::pnml_marking<int>(session->imported_document->final_marking);
    }
    std::optional<minera::EventLog> projected_powl_log;
    const minera::EventLog* alignment_log =
        &cached_model_log(*session, miner);
    if (miner == "powl" &&
        !request.has_param("alignment_activity_attribute")) {
      options.activity_attribute = session->powl_activity_key;
    }
    if (miner == "powl") {
      projected_powl_log = project_powl_activity(
          require_prepared_log(*session), options.activity_attribute,
          session->powl_timestamp_key, session->powl_case_id_key);
      alignment_log = &*projected_powl_log;
    }
    const auto alignment =
        miner == "imported"
            ? minera::tools::imported_model_alignment(*alignment_log, selected, options)
            : minera::check_alignment(*alignment_log, selected, options);
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
        "petri_reduction_abstraction",
        "petri_reduction_preserve_join_bridges"};
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
    auto options = minera::parse_concept_drift_params(lookup, present);
    // The route reports partition sizes only, so no sub-log is copied.
    options.materialize_sublogs = false;
    options.cancelled = connection_cancel_hook(request);
    const auto result = minera::detect_concept_drift(require_prepared_log(*session), options);
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

bool transformation_source_requested(const httplib::Request& request,
                                      const MinedSession& session) {
  if (request.has_param("source_id") && request.get_param_value("source_id") !=
      std::to_string(session.query_source_id))
    throw std::invalid_argument("the source log changed; refresh the transformation source");
  if (!request.has_param("include_source")) return false;
  const auto value = request.get_param_value("include_source");
  if (value != "true" && value != "false")
    throw std::invalid_argument("include_source must be true or false");
  if (value == "true" && !request.has_param("source_id"))
    throw std::invalid_argument("source_id is required for a transformation workflow");
  return value == "true";
}

std::string transformation_json(const MinedSession& session,
                                std::string result_json,
                                const minera::LabelSplittingResult* split = nullptr,
                                const minera::LabelSplittingOptions* options = nullptr) {
  const auto quote = minera::embedding_runtime::detail::escape_json;
  std::string body = "{\"family\":\"LOG_TRANSFORMATION\",\"source_id\":" +
      quote(std::to_string(session.query_source_id)) + ",\"cases\":[";
  for (std::size_t i = 0; i < require_prepared_log(session).traces.size(); ++i) {
    if (i) body += ',';
    const auto& trace = require_prepared_log(session).traces[i];
    body += "{\"index\":" + std::to_string(i) + ",\"case_id\":" + quote(trace.case_id) +
        ",\"event_count\":" + std::to_string(trace.events.size()) + '}';
  }
  body += "],\"result\":" + result_json + ',';
  if (split && options) {
    minera::XesExportOptions export_options;  // nulls omitted, as pm4py writes None/NaN
    body += embedded_xes_fields("transformed_xes",
        [&] { return minera::serialize_xes(split->log, export_options); });
    const bool projected = options->target_key == "activity" || options->target_key == "concept:name";
    std::string activity_attribute = projected ? "concept:name" : options->target_key;
    if (projected) for (const auto& trace : split->log.traces) if (!trace.events.empty()) {
      if (!trace.events.front().activity_source_key.empty()) activity_attribute = trace.events.front().activity_source_key;
      break;
    }
    const auto value = [](const minera::Event& event, const std::string& key) {
      if (key == "activity" || key == "concept:name" ||
          (!event.activity_source_key.empty() && key == event.activity_source_key)) return event.activity;
      const auto* attribute = event.attribute(key);
      return attribute ? *attribute : std::string{};
    };
    body += ",\"activity_attribute\":" + quote(activity_attribute) + ",\"label_rows\":[";
    bool first = true, complete = true;
    for (std::size_t i = 0; i < split->log.traces.size(); ++i) {
      for (std::size_t j = 0; j < split->log.traces[i].events.size(); ++j) {
        if (!first) body += ','; first = false;
        const auto& before = require_prepared_log(session).traces[i].events[j];
        const auto& after = split->log.traces[i].events[j];
        const auto output = value(after, options->target_key);
        complete = complete && !output.empty();
        body += "{\"case_index\":" + std::to_string(i) + ",\"event_index\":" + std::to_string(j) +
            ",\"input\":" + quote(value(before, options->activity_key)) +
            ",\"previous\":" + quote(value(before, options->target_key)) +
            ",\"output\":" + quote(output) + '}';
      }
    }
    body += "],\"complete_activity_field\":" + std::string(complete ? "true" : "false");
  } else body += "\"transformed_xes\":null";
  body += '}';
  std::lock_guard<std::mutex> lock(g_session_mutex);
  if (!g_session || g_session->query_source_id != session.query_source_id)
    throw std::invalid_argument("the source log changed during transformation; run again");
  return body;
}

#include "literal_controls_route.hpp"

void handle_log_to_target(const httplib::Request& request,
                          httplib::Response& response) {
  if (handle_literal_controls_request(request, response, "target")) return;
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
        "target_pad_size",      "target_activities", "source_id", "include_source"};
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
    const bool include_source = transformation_source_requested(request, *session);
    const auto options = minera::parse_log_to_target_params(lookup, present);
    if (include_source && options.pad_size > 100000)
      throw std::invalid_argument("target padding exceeds the workflow limit of 100000 rows per case");
    const auto result = minera::log_to_target(require_prepared_log(*session), options);
    auto body = minera::log_to_target_json(result);
    if (include_source) body = transformation_json(*session, std::move(body));
    response.set_content(body, "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_log_to_features(const httplib::Request& request,
                            httplib::Response& response) {
  if (handle_literal_controls_request(request, response, "features")) return;
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
        "feature_max_num_different_string_values", "source_id", "include_source"};
    for (const auto& [key, value] : request.params) { (void)value; if (!allowed.contains(key)) throw std::invalid_argument("unknown features parameter: " + key); }
    const auto lookup = [&](std::string_view name) { const auto key = std::string(name); return request.has_param(key) ? request.get_param_value(key) : std::string{}; };
    const auto present = [&](std::string_view name) { return request.has_param(std::string(name)); };
    const bool include_source = transformation_source_requested(request, *session);
    auto options = minera::parse_log_to_features_params(lookup, present);
    options.standard_keys = standard_keys_for_session(*session);
    auto body = minera::log_to_features_json(minera::log_to_features(require_prepared_log(*session), options));
    if (include_source) body = transformation_json(*session, std::move(body));
    response.set_content(body, "application/json");
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
        "variant", "train_percentage", "seed", "split_input", "prefix_length",
        "activity", "activity_key", "timestamp_key", "activities",
        "feature_min_different_values", "feature_max_different_values",
        "feature_consider_all_attributes", "feature_mandatory_attributes", "source_id", "include_source"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument("unknown log utility parameter: " + key);
    }
    const bool include_source = transformation_source_requested(request, *session);
    const auto publish = [&](std::string result, const std::vector<PreparationLogView>& outputs = {}) {
      if (include_source) result = preparation_json(*session, std::move(result), outputs);
      response.set_content(result, "application/json");
    };
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
        if (text.empty() || std::isspace(static_cast<unsigned char>(text.front())) ||
            end != text.c_str() + text.size() || !std::isfinite(parsed) ||
            parsed < 0.0 || parsed > 1.0)
          throw std::invalid_argument(
              "train_percentage expects a finite number in [0,1]");
        options.train_percentage = parsed;
      }
      if (request.has_param("seed")) options.seed = parse_size("seed", std::numeric_limits<std::size_t>::max());
      auto split_input = minera::SplitTrainTestInput::kDataFrame;
      if (request.has_param("split_input")) {
        const auto kind = value("split_input");
        if (kind == "event_log") split_input = minera::SplitTrainTestInput::kEventLog;
        else if (kind != "dataframe")
          throw std::invalid_argument("split_input expects dataframe or event_log");
      }
      const auto result = minera::split_log_train_test(
          require_prepared_log(*session), options.train_percentage, options.seed, split_input);
      publish(minera::log_train_test_json(result), {{"training", "Training log", &result.training},
                                                  {"testing", "Testing log", &result.testing}});
    } else if (variant_text == "fixed_prefixes") {
      options.variant = minera::LogUtilityVariant::kFixedPrefixes;
      options.prefix_length = parse_size("prefix_length", 100000);
      const auto result = minera::get_log_prefixes(require_prepared_log(*session), options.prefix_length);
      publish(minera::log_prefix_json(result, options.variant), {{"log", "Fixed prefixes", &result}});
    } else if (variant_text == "all_prefixes") {
      if (require_prepared_log(*session).event_count() > 50000)
        throw std::invalid_argument("all_prefixes input exceeds the safety limit");
      const auto result = minera::get_log_with_prefixes(require_prepared_log(*session));
      if (result.log.traces.size() > 100000)
        throw std::invalid_argument("all_prefixes output exceeds the safety limit");
      publish(minera::log_prefix_json(result.log, minera::LogUtilityVariant::kAllPrefixes,
                                     result.change_indexes), {{"log", "All prefixes", &result.log}});
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
          require_prepared_log(*session), options.activity, options);
      publish(minera::log_until_activity_json(result), {{"log", "Cases before activity", &result.log}});
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
          require_prepared_log(*session), split_escaped(value("activities"), "activities"), options);
      std::vector<PreparationLogView> outputs;
      for (std::size_t i = 0; i < result.logs.size(); ++i)
        outputs.push_back({"target/" + std::to_string(i), result.activities[i], &result.logs[i]});
      publish(minera::log_activity_targets_json(result), outputs);
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
      publish(minera::log_feature_selection_json(minera::select_log_features(require_prepared_log(*session),
                                                                            feature_options)));
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
    const auto result = minera::to_embeddings(require_prepared_log(*session), options);
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

// On-demand transformation of the cached session, also used by Transform.
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
        "label_splitting_activities_suffix", "label_splitting_target_activities", "source_id", "include_source"};
    for (const auto& [key, value] : request.params) { (void)value; if (!allowed.contains(key)) throw std::invalid_argument("unknown label_splitting parameter: " + key); }
    const auto lookup = [&](std::string_view name) { const auto key = std::string(name); return request.has_param(key) ? request.get_param_value(key) : std::string{}; };
    const auto present = [&](std::string_view name) { return request.has_param(std::string(name)); };
    const bool include_source = transformation_source_requested(request, *session);
    const auto options = minera::parse_label_splitting_params(lookup, present);
    const auto result = minera::split_activity_labels(require_prepared_log(*session), options);
    auto body = minera::label_splitting_json(result);
    if (include_source) body = transformation_json(*session, std::move(body), &result, &options);
    response.set_content(body, "application/json");
  } catch (const std::exception& error) { response.status = 400; response.set_content(error_json(error.what()), "application/json"); }
}

std::string structural_json(const MinedSession& session, std::string result,
                            const std::string& activity_key,
                            const minera::TrieModel* trie = nullptr,
                            const minera::PetriNet* net = nullptr) {
  const auto quote = minera::embedding_runtime::detail::escape_json;
  std::string body = "{\"family\":\"STRUCTURAL_TRANSFORMATION\",\"source_id\":" +
      quote(std::to_string(session.query_source_id)) + ",\"activity_key\":" +
      quote(activity_key) + ",\"result\":" + result + ",\"cases\":[";
  for (std::size_t i = 0; i < require_prepared_log(session).traces.size(); ++i) {
    if (i) body += ',';
    const auto& trace = require_prepared_log(session).traces[i];
    body += "{\"index\":" + std::to_string(i) + ",\"case_id\":" + quote(trace.case_id) +
        ",\"event_count\":" + std::to_string(trace.events.size()) + '}';
  }
  body += "],\"trie\":" + (trie ? minera::trie_json(*trie) : "null") +
      ",\"pnml\":" + (net ? quote(minera::to_pnml(*net, "trie")) : "null") + '}';
  std::lock_guard<std::mutex> lock(g_session_mutex);
  if (!g_session || g_session->query_source_id != session.query_source_id)
    throw std::invalid_argument("the source log changed during structural transformation; run again");
  return body;
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
        "trie_activity_key", "trie_max_path_length", "source_id", "include_source"};
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
    const bool include_source = transformation_source_requested(request, *session);
    const auto options = minera::parse_trie_params(lookup, present);
    const auto trie = minera::discover_trie(require_prepared_log(*session), options);
    auto body = minera::trie_json(trie);
    if (include_source) body = structural_json(*session, std::move(body), options.activity_key);
    response.set_content(body, "application/json");
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
        "trie_activity_key", "trie_max_path_length", "source_id", "include_source"};
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
    const bool include_source = transformation_source_requested(request, *session);
    const auto options = minera::parse_trie_params(lookup, present);
    const auto trie = minera::discover_trie(require_prepared_log(*session), options);
    const auto net = minera::convert_trie_to_petri_net(trie);
    auto body = minera::serialize_trie_petri_net_json(net);
    if (include_source) body = structural_json(*session, std::move(body), options.activity_key, &trie, &net);
    response.set_content(body, "application/json");
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
        "interval_query_start", "interval_query_end", "source_id", "include_source"};
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
    const bool include_source = transformation_source_requested(request, *session);
    const auto options = minera::parse_interval_tree_params(lookup, present);
    const auto intervals =
        minera::discover_open_path_intervals(require_prepared_log(*session), options);
    const auto query = minera::query_open_path_intervals(
        intervals, minera::interval_tree_query_options(options));
    auto body = minera::interval_tree_json(query);
    if (include_source) body = structural_json(*session, std::move(body), options.activity_key);
    response.set_content(body, "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_case_relations(const httplib::Request& request,
                           httplib::Response& response) {
  const bool typed = request.has_param("merge_input_format") &&
      request.get_param_value("merge_input_format") == "event_table_json";
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
        "merge_right_column", "include_open_log", "include_format_table",
        "merge_input_format", "merge_timestamp_key_json", "merge_case_id_key_json",
        "merge_left_suffix_json", "merge_right_suffix_json",
        "merge_left_column_json", "merge_right_column_json"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument("unknown merge parameter: " + key);
    }
    if (request.has_param("merge_input_format") &&
        request.get_param_value("merge_input_format") != "auto" && !typed)
      throw std::invalid_argument("merge_input_format: expected auto or event_table_json");
    if (!typed) for (const auto* name : {"merge_timestamp_key_json", "merge_case_id_key_json",
        "merge_left_suffix_json", "merge_right_suffix_json", "merge_left_column_json",
        "merge_right_column_json"})
      if (request.has_param(name))
        throw std::invalid_argument("JSON merge selectors require merge_input_format=event_table_json");
    if (typed) {
      const auto lookup = [&](std::string_view key) {
        return request.get_param_value(std::string(key));
      };
      const auto present = [&](std::string_view key) {
        return request.has_param(std::string(key));
      };
      const auto options = minera::tools::typed_case_relations_options(lookup, present);
      const auto payload = minera::tools::execute_typed_case_relations_json(
          request.get_file_value("base_log").content,
          request.get_file_value("incoming_log").content,
          request.get_file_value("case_relations").content, options,
          ocel_query_bool(request, "include_format_table", false),
          ocel_query_bool(request, "include_open_log", false));
      response.set_content(payload, "application/json");
      return;
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
    const bool include_open_log = ocel_query_bool(request, "include_open_log", false);
    const bool include_format_table = ocel_query_bool(request, "include_format_table", false);
    const auto publish_merge = [&](const minera::EventTable& table) {
      auto payload = minera::case_relations_json(table, options);
      if (include_open_log || include_format_table) {
        const auto quote = minera::embedding_runtime::detail::escape_json;
        payload.pop_back();
        if (include_format_table)
          payload += ",\"format_table\":" + minera::serialize_event_table_json(table);
        if (include_open_log) try {
          const auto log = minera::case_relations_projected_log(table, options);
          std::string activity = "concept:name";
          bool complete = true, first = true;
          for (const auto& trace : log.traces) for (const auto& event : trace.events) {
            const auto key = event.activity_source_key.empty() ? "concept:name" : event.activity_source_key;
            if (first) { activity = key; first = false; }
            complete = complete && !event.activity.empty() && key == activity;
          }
          minera::XesExportOptions export_options;  // nulls omitted, as pm4py writes None/NaN
          payload += ",\"open_log\":{\"xes\":" + quote(minera::serialize_xes(log, export_options)) +
              ",\"case_count\":" + std::to_string(log.trace_count()) +
              ",\"event_count\":" + std::to_string(log.event_count()) +
              ",\"activity_attribute\":" + quote(activity) +
              ",\"complete_activity_field\":" + std::string(complete ? "true" : "false") + '}';
        } catch (const std::exception& error) {
          payload += ",\"open_log\":null,\"open_log_error\":" + quote(error.what());
        }
        payload += '}';
      }
      response.set_content(payload, "application/json");
    };
    const auto left_column = value(
        "merge_left_column", options.case_id_key + options.left_suffix);
    const auto right_column = value(
        "merge_right_column", options.case_id_key + options.right_suffix);
    const bool base_is_xes = looks_like_xes(base_file.filename, base_file.content);
    const bool incoming_is_xes =
        looks_like_xes(incoming_file.filename, incoming_file.content);
    if (!base_is_xes && !incoming_is_xes) {
      auto relation_table =
          minera::parse_case_relations_table_csv(relation_input);
      options.relation_left_column = left_column;
      options.relation_right_column = right_column;
      auto base_data = minera::import_csv_event_data(base_input);
      auto incoming_data = minera::import_csv_event_data(incoming_input);
      // Keys joining a CSV case-id column follow its typed column.
      minera::type_csv_case_relation_keys(
          relation_table, left_column, right_column,
          minera::tools::csv_case_id_key(base_data.table, options.case_id_key)
              ? &base_data.table : nullptr,
          minera::tools::csv_case_id_key(incoming_data.table, options.case_id_key)
              ? &incoming_data.table : nullptr,
          options.case_id_key);
      normalize_csv_timestamp_missingness(base_data.table);
      normalize_csv_timestamp_missingness(incoming_data.table);
      if (!request.has_param("merge_timestamp_key")) {
        adapt_case_relations_table_timestamp(options, base_data.table,
                                              incoming_data.table);
      }
      publish_merge(minera::merge_case_relations(
          base_data.table, incoming_data.table, relation_table, options));
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
      // The relation merger reads the read_xes frame's rows (file order, each
      // row's own trace cells), which the imported traces give one to one.
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
      auto relation_table = minera::parse_case_relations_table_csv(relation_input);
      // Keys joining a CSV case-id column follow its typed column; XES keys
      // stay literal.
      minera::type_csv_case_relation_keys(
          relation_table, left_column, right_column,
          !base_is_xes && minera::tools::csv_case_id_key(base_table, options.case_id_key)
              ? &base_table : nullptr,
          !incoming_is_xes && minera::tools::csv_case_id_key(incoming_table, options.case_id_key)
              ? &incoming_table : nullptr,
          options.case_id_key);
      publish_merge(minera::merge_case_relations(
          base_table, incoming_table, relation_table, options));
    }
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(typed
        ? "{\"family\":\"MERGE_CASE_RELATIONS\",\"input_format\":\"event_table_json\",\"error\":" +
            minera::serialize_embedding_workbench_string_json(error.what()) + '}'
        : error_json(error.what()), "application/json");
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
                          ? minera::import_xes_dataframe_log(left_input)
                          : minera::import_csv_event_log(left_input);
    const auto right = looks_like_xes(right_file.filename, right_file.content)
                           ? minera::import_xes_dataframe_log(right_input)
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
    auto relations = minera::parse_case_relations_csv(
        relation_input, left_column, right_column);
    // Keys joining a CSV log follow its typed case-id column.
    minera::type_case_relations_like_csv_logs(
        relations,
        looks_like_xes(left_file.filename, left_file.content) ? nullptr : &left,
        looks_like_xes(right_file.filename, right_file.content) ? nullptr : &right);
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

// Generate independent models, formats and optional logs in one native run.
// This workflow does not read or replace the cached mining session.
void handle_tree_generation(const httplib::Request& request,
                            httplib::Response& response) {
  try {
    static const std::set<std::string> allowed = {
        "random_tree_variant", "random_tree_rec_depth", "random_tree_min_rec_depth",
        "random_tree_max_rec_depth", "random_tree_prob_leaf", "random_tree_mode",
        "random_tree_min", "random_tree_max", "random_tree_sequence", "random_tree_choice",
        "random_tree_parallel", "random_tree_loop", "random_tree_or", "random_tree_silent",
        "random_tree_duplicate", "random_tree_no_models", "random_tree_seed", "random_tree_activities",
        "tree_variant", "generate_log", "playout_variant", "playout_traces", "playout_max_steps",
        "playout_seed", "playout_min_length", "playout_max_length", "playout_max_loop_occurrences",
        "playout_max_traces", "playout_return_set_strings", "playout_cap_sampled_loops", "playout_start",
        "playout_case_interval", "playout_event_interval"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key)) throw std::invalid_argument("unknown tree-generation parameter: " + key);
    }
    const auto lookup = [&](std::string_view name) {
      const auto key = std::string(name);
      return request.has_param(key) ? request.get_param_value(key) : std::string{};
    };
    const auto present = [&](std::string_view name) { return request.has_param(std::string(name)); };
    const auto params = minera::parse_random_tree_params(lookup, present);
    const auto conversion = minera::parse_process_tree_petri_net_variant(
        request.has_param("tree_variant") ? request.get_param_value("tree_variant") : "standard");
    const auto with_log = ocel_query_bool(request, "generate_log", false);
    auto playout_options = minera::parse_process_tree_playout_params(lookup, present);
    // The reference playout log has no case ids or timestamps (BASIC has its
    // own fixed ones); giving any of these opts in to Minera's envelope.
    playout_options.add_envelope = request.has_param("playout_start") ||
                                   request.has_param("playout_case_interval") ||
                                   request.has_param("playout_event_interval");
    playout_options.start = ocel_query_number(request, "playout_start", playout_options.start);
    playout_options.case_interval = ocel_query_number(request, "playout_case_interval", playout_options.case_interval);
    playout_options.event_interval = ocel_query_number(request, "playout_event_interval", playout_options.event_interval);
    const auto trees = minera::generate_random_process_trees(params);
    if (with_log) {
      const auto extensive = playout_options.variant == minera::ProcessTreePlayoutVariant::kExtensive;
      const auto length = extensive
          ? playout_options.max_length.value_or(playout_options.max_steps)
          : playout_options.max_steps;
      const auto cases = extensive ? playout_options.max_traces : playout_options.traces;
      if (length > 10000 || cases > 10000 || playout_options.max_loop_occurrences.value_or(0) > 1000 ||
          (length && cases > 1000000 / length) ||
          (length && cases && trees.size() > 1000000 / length / cases))
        throw std::invalid_argument("generated logs exceed the one-million event budget");
    }
    const auto quote = minera::embedding_runtime::detail::escape_json;
    std::ostringstream out;
    out << "{\"family\":\"TREE_GENERATION\",\"tree_variant\":"
        << quote(request.has_param("tree_variant") ? request.get_param_value("tree_variant") : "standard")
        << ",\"generator\":" << minera::random_tree_json(trees, params.variant, params.activities)
        << ",\"models\":[";
    for (std::size_t i = 0; i < trees.size(); ++i) {
      if (i) out << ',';
      const auto& tree = trees[i];
      const auto name = "Generated model " + std::to_string(i);
      std::string ptml, ptml_error, pnml, pnml_error, bpmn, bpmn_error;
      std::string net_json, log_json, xes, playout_error;
      try { ptml = minera::to_ptml(tree, name); }
      catch (const std::exception& error) { ptml_error = error.what(); }
      try {
        const auto net = minera::to_petri_net(tree, conversion);
        pnml = minera::to_pnml(net, name);
        net_json = minera::pnml_document_json(minera::parse_pnml_document(pnml));
      } catch (const std::exception& error) { pnml_error = error.what(); }
      try { bpmn = minera::to_bpmn(tree, name); }
      catch (const std::exception& error) { bpmn_error = error.what(); }
      if (with_log) {
        try {
          const auto generated = minera::process_tree_playout_result(tree, playout_options);
          const auto playout_variant = playout_options.variant == minera::ProcessTreePlayoutVariant::kExtensive
              ? "EXTENSIVE" : playout_options.variant == minera::ProcessTreePlayoutVariant::kBasicPlayout
              ? "BASIC_PLAYOUT" : "TOPBOTTOM";
          minera::LogConversionResult converted; converted.log = generated.log;
          log_json = "{\"family\":\"GENERATED_TREE_PLAYOUT\",\"variant\":" + quote(playout_variant) +
              ",\"return_set_strings\":" + (playout_options.return_set_strings ? "true" : "false") +
              ",\"truncated\":" + (generated.truncated ? "true" : "false") +
              ",\"case_count\":" + std::to_string(generated.log.trace_count()) +
              ",\"event_count\":" + std::to_string(generated.log.event_count()) +
              ",\"log\":" + minera::log_conversion_json(converted) +
              ",\"language\":" + (playout_options.return_set_strings ? minera::serialize_process_tree_playout_json(generated) : "null") + '}';
          xes = minera::to_xes(generated.log, name + " playout");
        } catch (const std::exception& error) { playout_error = error.what(); }
      }
      const auto optional = [&](const char* key, const std::string& value) {
        out << ",\"" << key << "\":" << (value.empty() ? "null" : quote(value));
      };
      out << "{\"model_index\":" << i << ",\"tree\":" << minera::process_tree_json(tree)
          << ",\"net\":" << (net_json.empty() ? "null" : net_json)
          << ",\"playout\":" << (log_json.empty() ? "null" : log_json);
      optional("ptml", ptml); optional("ptml_error", ptml_error);
      optional("pnml", pnml); optional("pnml_error", pnml_error);
      optional("bpmn", bpmn); optional("bpmn_error", bpmn_error);
      optional("playout_xes", xes); optional("playout_error", playout_error);
      out << '}';
    }
    out << "]}";
    response.set_content(out.str(), "application/json");
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
          request.get_param_value("activity_attribute");
    } else if (request.has_file("activity_attribute")) {
      comparison_options.activity_attribute =
          request.get_file_value("activity_attribute").content;
    }
    if (trim_text(comparison_options.activity_attribute).empty() &&
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
      reference = minera::import_xes_dataframe_log(input, reference_options);
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
          require_prepared_log(*session), comparison_options.activity_attribute,
          session->powl_timestamp_key, session->powl_case_id_key);
      comparison_log = &*projected_powl_log;
      reference = project_powl_activity(
          reference, comparison_options.activity_attribute,
          session->powl_timestamp_key, session->powl_case_id_key);
    }
    const auto comparison = minera::compare_petri_usage_reference(
        it->second, *comparison_log, reference, comparison_options);
    response.set_content(minera::petri_usage_comparison_json(comparison),
                         "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

void handle_precomputed_replay_fitness(const httplib::Request& request,
                                      httplib::Response& response) {
  try {
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (key != "input_kind" && key != "fitness_variant")
        throw std::invalid_argument("unknown replay-result fitness parameter: " + key);
    }
    const auto kind = request.has_param("input_kind")
        ? request.get_param_value("input_kind") : std::string("token");
    std::optional<minera::ReplayFitnessVariant> variant;
    if (request.has_param("fitness_variant")) {
      const auto params = minera::parse_evaluation_params(
          [&](std::string_view name) { return name == "fitness_variant"
              ? request.get_param_value("fitness_variant") : std::string{}; },
          [&](std::string_view name) { return name == "fitness_variant"; });
      variant = params.replay_fitness_variant;
    }
    response.set_content(minera::precomputed_replay_fitness_json(
        request.body, kind, variant), "application/json");
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
    if (request.has_param("source_id") && request.get_param_value("source_id") != std::to_string(session->query_source_id))
      throw std::invalid_argument("the source log changed; refresh the model evaluation source");
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
    // Alignment-based fitness and precision stop when the client disconnects.
    params.replay_fitness_options.alignment_options.cancelled = connection_cancel_hook(request);
    params.precision_options.cancelled = connection_cancel_hook(request);
    if (params.replay_fitness_options.decomposed_alignment_options)
      params.replay_fitness_options.decomposed_alignment_options->cancelled = connection_cancel_hook(request);
    const auto adapter = minera::parse_evaluation_input_adapter(lookup, present);
    const auto* evaluation_document = miner == "imported" && session->imported_document && !converted
        ? &*session->imported_document : nullptr;
    std::optional<minera::EventLog> projected_powl_log;
    const bool model_only = params.family == minera::EvaluationFamily::kSimplicity;
    const minera::EventLog* evaluation_log = nullptr;
    if (!model_only && adapter == "log") evaluation_log = &cached_model_log(*session, miner);
    if (!model_only && adapter == "log" && miner == "powl") {
      projected_powl_log = project_powl_activity(
          require_prepared_log(*session), session->powl_activity_key,
          session->powl_timestamp_key, session->powl_case_id_key);
      evaluation_log = &*projected_powl_log;
    }
    const std::string_view model_activity = miner == "inductive"
        ? std::string_view(session->inductive_options.activity_key)
        : miner == "alpha" ? std::string_view(session->alpha_activity_attribute)
        : miner == "heuristic" ? std::string_view(session->heuristic_activity_attribute)
        : miner == "powl" ? std::string_view(session->powl_activity_key)
        : miner == "imported" ? std::string_view(session->imported_activity_attribute)
        : std::string_view("concept:name");
    std::optional<minera::EventLog> evaluation_input;
    if (!model_only && adapter == "log") {
    params = minera::tools::default_evaluation_input_params(
        *evaluation_log, std::move(params),
        request.has_param("evaluation_activity_attribute"), model_activity);
    evaluation_input = minera::tools::evaluation_input_log(
        *evaluation_log, params.replay_fitness_options.activity_attribute);
    if (evaluation_input) evaluation_log = &*evaluation_input;
    // pm4py's replay fitness of an XES frame sums the per-case values in
    // case-id order (see session_case_order); the log is read in that order
    // rather than copied.
    if (params.family == minera::EvaluationFamily::kReplayFitness)
      params.replay_fitness_options.trace_order =
          session_case_order(session->active_source.get(), *evaluation_log);
    }
    if (!model_only && adapter != "log") {
      overlay_cached_source_selectors(request, *session, params, true);
      if (!request.has_param("evaluation_activity_attribute") && session->active_source) {
        const auto& source = *session->active_source;
        const bool original_flat_mining = source.selected_kind() != minera::EventDataKind::kEventLog;
        const auto activity = original_flat_mining || (model_activity != "concept:name" && model_activity != "activity")
            ? std::string(model_activity) : source.selectors().activity_key;
        params.replay_fitness_options.activity_attribute = activity;
        params.precision_options.activity_attribute = activity;
        params.generalization_options.activity_attribute = activity;
        params.composite_options.activity_attribute = activity;
      }
      if (!request.has_param("evaluation_case_id_attribute") && session->active_source &&
          session->active_source->selected_kind() != minera::EventDataKind::kEventLog) {
        const auto& case_id = miner == "alpha" ? session->alpha_case_id_attribute :
            miner == "heuristic" ? session->heuristic_case_id_attribute :
            miner == "inductive" ? session->inductive_options.case_id_key :
            miner == "powl" ? session->powl_case_id_key : session->active_source->selectors().case_id_key;
        params.replay_fitness_options.case_id_attribute = case_id;
        params.precision_options.case_id_attribute = case_id;
        params.generalization_options.case_id_attribute = case_id;
        params.composite_options.case_id_attribute = case_id;
      }
    }
    minera::ModelEvaluation result;
    result.miner = miner;
    const auto evaluate_input = [&](const auto& input) {
    switch (params.family) {
      case minera::EvaluationFamily::kReplayFitness:
        result.replay_fitness = evaluation_document
            ? minera::evaluate_replay_fitness(
                  input, *evaluation_document, params.replay_fitness_variant,
                  params.replay_fitness_options)
            : minera::evaluate_replay_fitness(
            input, selected, params.replay_fitness_variant,
            params.replay_fitness_options);
        break;
      case minera::EvaluationFamily::kPrecision:
        if (evaluation_document && params.precision_variant !=
            minera::PrecisionVariant::ETCONFORMANCE_TOKEN)
          cached_imported_markings(request, *session, miner, params.precision_options,
                                   "evaluation_initial_marking", "evaluation_final_marking");
        // Every variant takes the document entry point: alignment-based
        // precision needs its PNML transition IDs for the reference's
        // name-ordered eventually-enabled scan.
        result.precision = evaluation_document
            ? minera::evaluate_precision(
                  input, *evaluation_document, params.precision_variant,
                  params.precision_options)
            : minera::evaluate_precision(
            input, selected, params.precision_variant,
            params.precision_options);
        break;
      case minera::EvaluationFamily::kGeneralization:
        result.generalization = evaluation_document
            ? minera::evaluate_generalization(
                  input, *evaluation_document, params.generalization_variant,
                  params.generalization_options)
            : minera::evaluate_generalization(
            input, selected, params.generalization_variant,
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
                  input, *session->imported_document, params.composite_options)
            : minera::evaluate_composite(
                  input, selected, params.composite_options);
        break;
    }
    };
    // A BPMN or PTML import has no PNML document: its conversion's initial
    // marking is the one token on its source (pm4py's convert_to_petri_net
    // im), not one token on every input-free place, which a BPMN conversion
    // can leave in dangling gateways or disconnected parts (review V2-3).
    if (miner == "imported" && !evaluation_document && selected.has_source() &&
        params.simplicity_options.initial_marking.empty() &&
        selected.source() < selected.place_count()) {
      params.simplicity_options.initial_marking.assign(selected.place_count(), 0);
      params.simplicity_options.initial_marking[selected.source()] = 1;
    }
    if (model_only) {
      result.simplicity = evaluation_document
          ? minera::evaluate_simplicity(*evaluation_document,params.simplicity_variant,params.simplicity_options)
          : minera::evaluate_simplicity(selected,params.simplicity_variant,params.simplicity_options);
    } else if (adapter == "log") {
      evaluate_input(*evaluation_log);
    } else {
      const auto kind = adapter == "stream" ? minera::EventDataKind::kEventStream :
          adapter == "dataframe" ? minera::EventDataKind::kEventTable : minera::EventDataKind::kLazyEventTable;
      evaluate_input(cached_source_view(*session,kind));
    }
    const auto quote = minera::embedding_runtime::detail::escape_json;
    auto body = minera::model_evaluation_json(result, session->graph_ready ? &session->graph : nullptr);
    body.pop_back();
    body += ",\"input_context\":{\"adapter\":" + quote(model_only ? "model_only" : adapter) +
      ",\"origin\":" + quote(model_only ? "model_only" : "cached_model_log") +
      ",\"source_id\":" + quote(std::to_string(session->query_source_id)) + ",\"dataset\":" + quote(session->dataset) +
      ",\"case_count\":" + (evaluation_log ? std::to_string(evaluation_log->trace_count()) : std::string("null")) + ",\"event_count\":" + (evaluation_log ? std::to_string(evaluation_log->event_count()) : std::string("null")) +
      ",\"activity_attribute\":" + quote(model_only ? "" : params.replay_fitness_options.activity_attribute) +
      ",\"case_id_attribute\":" + quote(model_only ? "" : params.replay_fitness_options.case_id_attribute) + "}}";
    response.set_content(body, "application/json");
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
              require_prepared_log(*session), session->powl_activity_key,
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
            ? minera::import_xes_dataframe_log(input)
            : minera::import_csv_event_log(input);
    const auto result = minera::evaluate_emd(
        // The reference compares trace-variant probabilities (get_language).
        minera::derive_stochastic_language(require_prepared_log(*session), true),
        minera::derive_stochastic_language(reference, true), options);
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
    auto options = minera::parse_process_tree_alignment_params(lookup, present);
    options.cancelled = connection_cancel_hook(request);
    const auto& alignment_log = cached_model_log(*session, "inductive");
    auto result = minera::align_process_tree(alignment_log,
                                             require_process_tree(*session),
                                             options);
    // pm4py's alignments of an XES frame list its cases in case-id order
    // (one entry per trace of the aligned log).
    if (const auto case_order = session_case_order(session->active_source.get(), alignment_log))
      minera::apply_case_order(result.traces, *case_order);
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
  const auto miner = requested_miner(request); const auto it=session->nets.find(miner);
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
    const auto lookup=[&](std::string_view n){const auto k=std::string(n); return request.has_param(k)?request.get_param_value(k):std::string{};}; const auto present=[&](std::string_view n){return request.has_param(std::string(n));}; auto o=minera::parse_decomposed_alignment_params(lookup,present); o.cancelled=connection_cancel_hook(request); cached_imported_markings(request, *session, miner, o); std::optional<minera::EventLog> projected_powl_log; const minera::EventLog* alignment_log=&cached_model_log(*session,miner); if(miner=="powl"){if(!request.has_param("activity_attribute")) o.activity_attribute=session->powl_activity_key; projected_powl_log=project_powl_activity(require_prepared_log(*session),o.activity_attribute,session->powl_timestamp_key,session->powl_case_id_key); alignment_log=&*projected_powl_log;} const auto r=minera::align_decomposed(*alignment_log,it->second,o); auto body=minera::decomposed_alignment_json(r); reject_nonfinite_alignment_costs(body); response.set_content(std::move(body),"application/json"); }
  catch(const std::exception& e){response.status=400; response.set_content(error_json(e.what()),"application/json");}
}

void handle_align_petri_anti(const httplib::Request& request, httplib::Response& response) {
  std::shared_ptr<const MinedSession> session; { std::lock_guard<std::mutex> lock(g_session_mutex); session = g_session; }
  if (!session) { response.status=409; response.set_content(error_json("no mined log; mine a log first"),"application/json"); return; }
  const auto miner=requested_miner(request); const auto it=session->nets.find(miner); if(it==session->nets.end()){response.status=400;response.set_content(error_json("unknown miner: "+miner),"application/json");return;}
  try { static const std::set<std::string> allowed = {"miner", "variant", "exponent", "epsilon", "marking_limit", "max_states", "initial_marking", "final_marking", "activity_attribute"}; for (const auto& [key, value] : request.params) { (void)value; if (!allowed.contains(key)) throw std::invalid_argument("unknown Petri anti-alignment parameter: " + key); } const auto lookup=[&](std::string_view n){const auto k=std::string(n);return request.has_param(k)?request.get_param_value(k):std::string{};}; const auto present=[&](std::string_view n){return request.has_param(std::string(n));}; auto o=minera::parse_petri_anti_alignment_params(lookup,present); cached_imported_markings(request, *session, miner, o); std::optional<minera::EventLog> projected_powl_log; const minera::EventLog* alignment_log=&cached_model_log(*session,miner); if(miner=="powl"){if(!request.has_param("activity_attribute")) o.activity_attribute=session->powl_activity_key; projected_powl_log=project_powl_activity(require_prepared_log(*session),o.activity_attribute,session->powl_timestamp_key,session->powl_case_id_key); alignment_log=&*projected_powl_log;} const auto r=minera::anti_align_petri(it->second,*alignment_log,o); response.set_content(minera::petri_anti_alignment_json(r),"application/json"); } catch(const std::exception&e){response.status=400;response.set_content(error_json(e.what()),"application/json");}
}

void handle_align_petri_multi(const httplib::Request& request, httplib::Response& response) {
  std::shared_ptr<const MinedSession> session; { std::lock_guard<std::mutex> lock(g_session_mutex); session = g_session; }
  if (!session) { response.status=409; response.set_content(error_json("no mined log; mine a log first"),"application/json"); return; }
  const auto miner=requested_miner(request); const auto it=session->nets.find(miner); if(it==session->nets.end()){response.status=400;response.set_content(error_json("unknown miner: "+miner),"application/json");return;}
  try { static const std::set<std::string> allowed = {"miner", "variant", "exponent", "marking_limit", "max_states", "initial_marking", "final_marking", "activity_attribute"}; for (const auto& [key, value] : request.params) { (void)value; if (!allowed.contains(key)) throw std::invalid_argument("unknown Petri multi-alignment parameter: " + key); } const auto lookup=[&](std::string_view n){const auto k=std::string(n);return request.has_param(k)?request.get_param_value(k):std::string{};}; const auto present=[&](std::string_view n){return request.has_param(std::string(n));}; auto o=minera::parse_petri_multi_alignment_params(lookup,present); cached_imported_markings(request, *session, miner, o); std::optional<minera::EventLog> projected_powl_log; const minera::EventLog* alignment_log=&cached_model_log(*session,miner); if(miner=="powl"){if(!request.has_param("activity_attribute")) o.activity_attribute=session->powl_activity_key; projected_powl_log=project_powl_activity(require_prepared_log(*session),o.activity_attribute,session->powl_timestamp_key,session->powl_case_id_key); alignment_log=&*projected_powl_log;} const auto r=minera::multi_align_petri(it->second,*alignment_log,o); response.set_content(minera::petri_multi_alignment_json(r),"application/json"); } catch(const std::exception&e){response.status=400;response.set_content(error_json(e.what()),"application/json");}
}

void handle_reduce_process_tree(const httplib::Request& request, httplib::Response& response) {
  std::shared_ptr<const MinedSession> session; { std::lock_guard<std::mutex> lock(g_session_mutex); session = g_session; }
  if (!session) { response.status=409; response.set_content(error_json("no mined log; mine a log first"),"application/json"); return; }
  try { if(!request.has_param("trace_index")) throw std::invalid_argument("trace_index: expected a non-negative integer"); const auto text=request.get_param_value("trace_index"); std::size_t idx=0; const auto r=std::from_chars(text.data(),text.data()+text.size(),idx); if(r.ec!=std::errc{}||r.ptr!=text.data()+text.size()||idx>=cached_model_log(*session,"inductive").traces.size()) throw std::invalid_argument("trace_index: out of range"); const auto lookup=[&](std::string_view n){const auto k=std::string(n);return request.has_param(k)?request.get_param_value(k):std::string{};}; const auto present=[&](std::string_view n){return request.has_param(std::string(n));}; const auto o=minera::parse_process_tree_reduction_params(lookup,present); const auto reduced=minera::reduce_process_tree(require_process_tree(*session),cached_model_log(*session,"inductive").traces[idx],o); response.set_content(minera::process_tree_reduction_json(reduced,session->graph,idx),"application/json"); } catch(const std::exception&e){response.status=400;response.set_content(error_json(e.what()),"application/json");}
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
    auto params = minera::parse_mine_params(lookup, present);
    auto options = params.alpha;
    minera::EventLog log;
    if (request.has_file("log") || (!request.is_multipart_form_data() && !request.body.empty())) {
      const auto body = request.has_file("log") ? request.get_file_value("log").content : request.body;
      const auto filename = request.has_file("log") ? request.get_file_value("log").filename : std::string{};
      std::stringstream input(body);
      log = minera::looks_like_xes(filename, body) ? minera::import_xes_dataframe_log(input)
                                                 : minera::import_csv_event_log(input);
    } else {
      std::shared_ptr<const MinedSession> session;
      { std::lock_guard<std::mutex> lock(g_session_mutex); session = g_session; }
      if (!session) {
        response.status = 409;
        response.set_content(error_json("no mined log; upload a log or mine one first"), "application/json");
        return;
      }
      overlay_cached_source_selectors(request,*session,params);
      options = params.alpha;
      response.set_content(minera::alpha_plus_result_json(minera::discover_alpha_plus_result(session->active_source ? session->active_source->selected_view() : minera::EventDataView(require_prepared_log(*session)),options)),"application/json");
      return;
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
    options.cancelled = connection_cancel_hook(request);
    const auto projection = present("projection") ? lookup("projection") : std::string("none");
    if (projection != "none" && projection != "log" && projection != "metrics") throw std::invalid_argument("projection expects none, log, or metrics");
    minera::DfgStatisticalOptions dopt; dopt.window = 1; dopt.activity_attribute = options.activity_attribute; dopt.timestamp_attribute = options.timestamp_attribute; const auto dfg = minera::discover_frequency_dfg(require_prepared_log(*session), dopt);
    std::map<std::string,std::uint64_t> starts, ends; for (const auto& t:require_prepared_log(*session).traces) if(!t.events.empty()){ const auto* first = minera::dfg_event_field_value(t.events.front(), options.activity_attribute); const auto* last = minera::dfg_event_field_value(t.events.back(), options.activity_attribute); if (!first || !last) throw std::invalid_argument("DFG alignment: event is missing activity attribute '" + options.activity_attribute + "'"); ++starts[*first];++ends[*last]; }
    auto result = minera::align_log_on_dfg(require_prepared_log(*session), dfg, starts, ends, options);
    // pm4py's DFG alignment of an XES frame (algo.conformance.alignments.dfg
    // apply on the read_xes frame) lists its cases in case-id order. The
    // projections read the result by trace position, so they come first.
    const auto case_order = session_case_order(session->active_source.get(), require_prepared_log(*session));
    const auto listed = [&]() -> const minera::DfgAlignmentLogResult& {
      if (case_order) minera::apply_case_order(result.alignments, *case_order);
      return result;
    };
    if (projection == "log") { const auto projected = minera::project_log_on_dfg(require_prepared_log(*session), dfg, starts, ends, result, options); response.set_content(minera::dfg_alignment_json(listed(), nullptr, &projected), "application/json"); }
    else if (projection == "metrics") { const auto metrics = minera::project_alignments_on_dfg(require_prepared_log(*session), dfg, starts, ends, result, options); response.set_content(minera::dfg_alignment_json(listed(), &metrics), "application/json"); }
    else response.set_content(minera::dfg_alignment_json(listed()), "application/json");
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
    const auto reference = minera::looks_like_xes(file.filename, file.content) ? minera::import_xes_dataframe_log(input) : minera::import_csv_event_log(input);
    const auto lookup = [&](std::string_view n) { const auto key=std::string(n); if (request.has_param(key)) return request.get_param_value(key); if (request.has_file(key)) return request.get_file_value(key).content; return std::string{}; };
    const auto present = [&](std::string_view n) { const auto key=std::string(n); return request.has_param(key) || request.has_file(key); };
    auto options = minera::parse_edit_distance_params(lookup, present);
    options.cancelled = connection_cancel_hook(request);
    if (cached_source_uses_bound_defaults(*session)) {
      const auto& selectors = session->active_source->selectors();
      if (!present("edit_activity_attribute")) options.activity_attribute = selectors.activity_key;
      if (!present("edit_timestamp_attribute")) options.timestamp_attribute = selectors.timestamp_key;
      options.case_id_attribute = selectors.case_id_key;
    }
    const auto projection = present("projection") ? lookup("projection") : std::string("none");
    if (projection != "none" && projection != "log") throw std::invalid_argument("projection expects none or log");
    const auto result =
        minera::align_typed_edit_distance(session->active_source ? session->active_source->selected_view() : minera::EventDataView(require_prepared_log(*session)), minera::EventDataView(reference), options);
    if (projection == "log") { const auto projected = [&]() {
      minera::LogConversionOptions conversion; conversion.case_id_key = options.case_id_attribute;
      const auto projection_source = minera::event_data_to_event_log(session->active_source ? session->active_source->selected_view() : minera::EventDataView(require_prepared_log(*session)), conversion);
      return minera::project_log_on_typed_alignments(projection_source,result,options);
    }(); response.set_content(minera::typed_edit_distance_json(result, options.perform_anti_alignment, &projected), "application/json"); }
    else response.set_content(minera::typed_edit_distance_json(result, options.perform_anti_alignment), "application/json");
  } catch (const std::exception& error) { response.status = 400; response.set_content(error_json(error.what()), "application/json"); }
}

minera::DecisionMiningOptions decision_mining_options(
    const httplib::Request& request, bool allow_table_lookback = false,
    bool allow_workspace = false) {
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
        !(allow_table_lookback && key == "decision_lookback") &&
        !(allow_workspace && (key == "source_id" || key == "include_workspace" ||
                              key == "include_decision_diagnostics")))
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
          require_prepared_log(*session), options.activity_attribute,
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

  const std::string miner = requested_miner(request);
  try {
    std::optional<minera::PetriNet> converted;
    const auto& selected = select_cached_or_process_tree_net(
        request, *session, miner, converted);
    // The standard inductive net exports the reference node names.
    const bool standard_tree_net =
        miner == "inductive" && session->tree_ready &&
        (!request.has_param("process_tree_variant") ||
         minera::parse_process_tree_petri_net_variant(
             request.get_param_value("process_tree_variant")) ==
             minera::ProcessTreePetriNetVariant::kStandard);
    response.set_content(
        miner == "imported" && session->imported_document
            ? minera::to_pnml(*session->imported_document)
        : standard_tree_net
            ? minera::to_pnml(minera::process_tree_pnml_document(session->tree),
                              miner + " net")
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
    const auto net = minera::to_petri_net(require_process_tree(*session), variant);
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
      response.set_content(minera::to_ptml(require_process_tree(*session), session->dataset),
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
    // POWL converts from the model itself, as pm4py's
    // convert_to_process_tree(powl); the other miners reduce their nets.
    response.set_content(
        minera::to_ptml(miner == "powl" && session->powl
                            ? minera::to_process_tree(*session->powl)
                            : minera::to_process_tree(it->second),
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
      response.set_content(minera::to_bpmn(require_process_tree(*session), "process"),
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

std::string simulation_workspace_json(const minera::MonteCarloResult& result,
    const minera::PetriNet& net, const minera::MonteCarloOptions& options,
    const MinedSession& session, const std::string& miner) {
  const auto quote = minera::embedding_runtime::detail::escape_json;
  minera::LogConversionResult simulated;
  simulated.log = result.log;
  minera::PnmlDocument document;
  if (miner == "imported" && session.imported_document)
    document = *session.imported_document;
  document.net = net;
  if (options.initial_marking.empty() && options.final_marking.empty() &&
      miner != "imported") {
    const auto markings = minera::default_workflow_markings(net);
    document.initial_marking = markings.initial;
    document.final_marking = markings.final;
  } else {
    document.initial_marking.clear();
    document.final_marking.clear();
    for (const auto value : options.initial_marking)
      document.initial_marking.push_back(value);
    for (const auto value : options.final_marking)
      document.final_marking.push_back(value);
    if (options.initial_marking.empty()) {
      document.initial_marking.assign(net.place_count(), 0);
      document.initial_marking.at(net.source()) = 1;
    }
    if (options.final_marking.empty()) {
      document.final_marking.assign(net.place_count(), 0);
      document.final_marking.at(net.sink()) = 1;
    }
  }
  document.initial_marking_keys = options.initial_marking_keys;
  document.final_marking_keys = options.final_marking_keys;
  std::string model = "{\"place_count\":" + std::to_string(net.place_count()) +
      ",\"source\":" + std::to_string(net.source()) +
      ",\"sink\":" + std::to_string(net.sink()) + ",\"activities\":[";
  for (std::size_t i = 0; i < net.activities().size(); ++i) {
    if (i) model += ',';
    model += quote(net.activities()[i]);
  }
  model += "],\"transitions\":[";
  const auto indices = [](const std::vector<std::size_t>& values) {
    std::string json = "[";
    for (std::size_t i = 0; i < values.size(); ++i) {
      if (i) json += ',';
      json += std::to_string(values[i]);
    }
    return json + ']';
  };
  for (std::size_t i = 0; i < net.transitions().size(); ++i) {
    if (i) model += ',';
    const auto& t = net.transitions()[i];
    model += "{\"index\":" + std::to_string(i) + ",\"activity\":" +
        (t.activity ? std::to_string(*t.activity) : "null") +
        ",\"inputs\":" + indices(t.inputs) + ",\"outputs\":" + indices(t.outputs) +
        ",\"reset_inputs\":" + indices(t.reset_inputs) +
        ",\"inhibitor_inputs\":" + indices(t.inhibitor_inputs) + '}';
  }
  model += "]}";
  return "{\"family\":\"SIMULATION_WORKSPACE\",\"source_id\":" +
      quote(std::to_string(session.query_source_id)) + ",\"dataset\":" +
      quote(session.dataset) + ",\"miner\":" + quote(miner) +
      ",\"source\":{\"case_count\":" + (session.log_ready ? std::to_string(require_prepared_log(session).trace_count()) : std::string("null")) +
      ",\"event_count\":" + (session.active_source && session.active_source->checked_table_if_available() ? std::to_string(session.active_source->checked_table().rows.size()) : session.log_ready ? std::to_string(require_prepared_log(session).event_count()) : std::string("null")) +
      "},\"result\":" + minera::monte_carlo_json(result) +
      ",\"simulated_log\":" + minera::log_conversion_json(simulated) +
      ',' + embedded_xes_fields("xes", [&] { return minera::to_xes(result.log, miner + " Monte Carlo playout"); }) +
      ",\"model\":" + model + ",\"underlying_pnml\":" +
      quote(minera::to_pnml(document, "simulation underlying model")) + '}';
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
    reject_unknown_monte_carlo_params(request, true);
    if (request.has_param("source_id") && request.get_param_value("source_id") !=
        std::to_string(session->query_source_id))
      throw std::invalid_argument("the source log changed; refresh the simulation source");
    const bool workspace = ocel_query_bool(request, "include_workspace", false);
    if (workspace && !request.has_param("source_id"))
      throw std::invalid_argument("source_id is required for a simulation workspace");
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
    options.cancelled = connection_cancel_hook(request);
    std::optional<minera::EventLog> projected_powl_log;
    const minera::EventLog* simulation_log =
        &cached_model_log(*session, selected_miner);
    if (selected_miner == "powl") {
      if (!request.has_param("monte_carlo_activity_key"))
        options.activity_key = session->powl_activity_key;
      if (!request.has_param("monte_carlo_timestamp_key"))
        options.timestamp_key = session->powl_timestamp_key;
      projected_powl_log = project_powl_activity(
          require_prepared_log(*session), options.activity_key, options.timestamp_key,
          session->powl_case_id_key);
      simulation_log = &*projected_powl_log;
    }
    default_monte_carlo_activity_key(request, *simulation_log, options);
    const auto result = minera::monte_carlo(*simulation_log, selected, options);
    response.set_content(workspace ? simulation_workspace_json(
        result, selected, options, *session, selected_miner) :
        minera::monte_carlo_json(result), "application/json");
  } catch (const std::exception& error) {
    reject(error.what());
  }
}

// Returns the deterministic encoded decision tables for one cached model.
bool decision_workspace_requested(const httplib::Request& request,
                                  const MinedSession& session) {
  if(request.has_param("source_id")&&request.get_param_value("source_id")!=std::to_string(session.query_source_id))
    throw std::invalid_argument("the source log changed; refresh the decision source");
  const bool included=ocel_query_bool(request,"include_workspace",false);
  if(included&&!request.has_param("source_id"))throw std::invalid_argument("source_id is required for a decision workspace");
  return included;
}
std::string decision_workspace_json(const std::string& result,const minera::PetriNet& net,
                                    const MinedSession& session,const std::string& miner,
                                    const std::string& mode) {
  const auto quote=minera::embedding_runtime::detail::escape_json;
  std::string pnml;
  if(miner=="imported"&&session.imported_document){auto document=*session.imported_document;document.net=net;pnml=minera::to_pnml(document,"decision underlying model");}
  else {const auto markings=minera::default_workflow_markings(net);pnml=minera::to_pnml(net,markings.initial,markings.final,"decision underlying model");}
  return "{\"family\":\"DECISION_WORKSPACE\",\"source_id\":"+quote(std::to_string(session.query_source_id))+",\"dataset\":"+quote(session.dataset)+",\"miner\":"+quote(miner)+",\"mode\":"+quote(mode)+",\"source\":{\"case_count\":"+(session.log_ready ? std::to_string(require_prepared_log(session).trace_count()) : std::string("null"))+",\"event_count\":"+(session.active_source && session.active_source->checked_table_if_available() ? std::to_string(session.active_source->checked_table().rows.size()) : session.log_ready ? std::to_string(require_prepared_log(session).event_count()) : std::string("null"))+"},\"result\":"+result+",\"underlying_pnml\":"+quote(pnml)+"}";
}
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
    const bool workspace=decision_workspace_requested(request,*session);
    if(request.has_param("include_decision_diagnostics"))throw std::invalid_argument("include_decision_diagnostics requires a guarded model");
    auto mining = decision_mining_options(request, true, true);
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
          require_prepared_log(*session), mining.activity_attribute,
          session->powl_timestamp_key, session->powl_case_id_key);
      decision_log = &*projected_powl_log;
    }
    const auto result =
        minera::mine_decision_tables(*decision_log, selected, options);
    const auto json=minera::serialize_decision_tables_json(result);
    response.set_content(workspace?decision_workspace_json(json,selected,*session,miner,"tables"):json,
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
    const bool workspace=decision_workspace_requested(request,*session);
    options.decision_options = decision_mining_options(request, true, true);
    if(request.has_param("include_decision_diagnostics"))options.include_decision_diagnostics=ocel_query_bool(request,"include_decision_diagnostics",true);
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
          require_prepared_log(*session), options.decision_options.activity_attribute,
          session->powl_timestamp_key, session->powl_case_id_key);
      decision_log = &*projected_powl_log;
    }
    const auto result =
        minera::mine_data_petri_net(*decision_log, selected, options);
    const auto json=minera::serialize_data_petri_net_json(result,miner);
    response.set_content(workspace?decision_workspace_json(json,result.net,*session,miner,"guarded"):json,
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
            require_prepared_log(*session), session->powl_activity_key,
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
      const auto performance = minera::discover_performance_dfg(
          require_prepared_log(*session),
          dfg_statistics_for_source({}, session->active_source.get()));
      const auto model = minera::stochastic_map_from_performance_dfg(
          require_graph(*session), performance, params.performance);
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

// pm4py plays out 1000 traces whatever the per-trace length cap. The server
// keeps the worst case (traces x per-trace cap) under its 1,000,000-event
// safety budget, so when the caller omits the trace count it shrinks to
// min(default, budget / cap) instead of failing (declared in
// docs/reference-2.7.22-parity.md). An explicit count is checked as given.
constexpr std::size_t kPlayoutEventBudget = 1'000'000;
std::size_t default_playout_traces(std::size_t per_trace_cap,
                                   std::size_t reference_default) {
  if (per_trace_cap == 0) return reference_default;
  return std::max<std::size_t>(
      1, std::min(reference_default, kPlayoutEventBudget / per_trace_cap));
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

  // The page names the mined source it shows; refuse to play out a session
  // that has since been replaced (another tab re-mined a different log).
  if (request.has_param("source_id") &&
      request.get_param_value("source_id") != std::to_string(session->query_source_id)) {
    response.status = 409;
    response.set_content(
        error_json("the mined log changed since this analysis was shown; "
                   "re-open its event log to play out its models"),
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
            require_prepared_log(*session), options.activity_key, options.timestamp_key,
            session->powl_case_id_key);
        simulation_log = &*projected_powl_log;
      }
      default_monte_carlo_activity_key(request, *simulation_log, options);
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
      if (!present("stochastic_traces"))
        options.traces = default_playout_traces(options.max_steps, options.traces);
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
      const minera::EventLog* fit_log = nullptr;
      if (options.fit_from_log) fit_log = &cached_model_log(*session, selected_miner);
      if (options.fit_from_log && selected_miner == "powl") {
        projected_powl_log = project_powl_activity(
            require_prepared_log(*session), session->powl_activity_key,
            session->powl_timestamp_key, session->powl_case_id_key);
        fit_log = &*projected_powl_log;
      }
      if (options.return_visited_elements) {
        const auto& selected = select_cached_or_process_tree_net(
            request, *session, selected_miner, converted);
        // The visible budget above does not bound silent firings.
        bound_server_stochastic_playout(options, selected, true, request);
        const auto visited = options.fit_from_log
                                 ? minera::stochastic_playout_visited(
                                       selected, *fit_log, options)
                                 : minera::stochastic_playout_visited(
                                       selected, options);
        check_server_stochastic_visited_peak(
            visited, minera::stochastic_playout_visited_json_bytes(selected, visited), 3);
        response.set_content(
            minera::serialize_stochastic_playout_visited_json(selected,
                                                                visited),
            "application/json");
      } else {
        const auto& selected = select_cached_or_process_tree_net(
            request, *session, selected_miner, converted);
        bound_server_stochastic_playout(options, selected, false, request);
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
          options.max_loop_occurrences.value_or(0) > 100 ||
          options.max_length.value_or(0) > 64) {
        reject("process-tree playout bounds exceed the server safety limits");
        return;
      }
      const auto& tree = require_process_tree(*session);
      if (options.variant == minera::ProcessTreePlayoutVariant::kExtensive) {
        // An omitted max_length or max_loop_occurrences takes the reference
        // default (the tree's shortest word length, half of it); the
        // defaults meet the same limits as explicit values and are refused
        // the same way.
        const auto resolved = minera::resolve_process_tree_extensive_bounds(tree, options);
        if (resolved.max_length > 64 || resolved.max_loop_occurrences > 100) {
          reject("process-tree playout bounds exceed the server safety limits: the default "
                 "max_length, the tree's shortest word length, is " +
                 std::to_string(resolved.max_length) +
                 ", above 64, so no word fits any allowed max_length; use a sampled variant");
          return;
        }
        if (options.max_traces > 10000) {
          if (request.has_param("max_trace_cap") ||
              request.has_param("max_traces")) {
            reject("max_trace_cap must not exceed 10000");
            return;
          }
          options.max_traces = 10000;
        }
        // Server bound: the result keeps at most max_traces words (the
        // reference caps only a parallel cut's words) and says truncated.
        options.output_trace_cap = options.max_traces;
      }
      options.cancelled = connection_cancel_hook(request);
      const auto result = minera::process_tree_playout_result(tree, options);
      // The XES download has no truncated member, so the header says it
      // (the JSON answer carries "truncated" as well).
      response.set_header("X-Minera-Truncated", result.truncated ? "true" : "false");
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
      if (options.variant == minera::DfgPlayoutVariant::kPerformance &&
          !present("dfg_playout_traces"))
        options.traces = default_playout_traces(options.max_events_per_trace,
                                                options.traces);
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
      // CSV and XES sources take the DataFrame performance arithmetic.
      const auto dfg_options = dfg_statistics_for_source({}, session->active_source.get());
      const auto frequency = minera::discover_frequency_dfg(require_prepared_log(*session), dfg_options);
      minera::DfgActivityCounts starts, ends;
      // The reference's start-activity dict lists activities in first-
      // occurrence order; its PERFORMANCE start choice follows that order.
      for (const auto& trace : require_prepared_log(*session).traces) {
        if (!trace.events.empty()) {
          if (!starts.contains(trace.events.front().activity))
            options.start_activity_order.push_back(trace.events.front().activity);
          ++starts[trace.events.front().activity];
          ++ends[trace.events.back().activity];
        }
      }
      std::optional<minera::DfgPerformance> performance;
      if (options.variant == minera::DfgPlayoutVariant::kPerformance) {
        performance = minera::discover_performance_dfg(require_prepared_log(*session), dfg_options);
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
      const auto log = minera::declare_playout([&]() -> const minera::DeclareModel& {
        if (!session->declare_ready) throw std::invalid_argument("the cached Declare model is unavailable");
        return session->declare;
      }(), options);
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
  // pm4py's basic Petri playout generates 1000 traces by default
  // (play_out(net, im, fm) returns 1000 cases); the native struct keeps its
  // historical 100 for the hashed workbench captures.
  options.traces = 1000;
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
  if (!request.has_param("traces"))
    options.traces = default_playout_traces(options.max_steps, options.traces);
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
    // The reference envelope (cases "0".., epoch 10,000,000, one second per
    // event) unless the caller asks for its own timing.
    options.add_envelope = request.has_param("start") ||
                           request.has_param("case_interval") ||
                           request.has_param("event_interval");
    // A server-side bound on add_only_if_fm_is_reached resampling; the
    // reference has none.
    options.max_attempts = 100 * options.traces + 1000;
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
    // A malformed marking is the caller's error (400), not an escaped throw.
    options.initial_marking = parse_playout_marking(request, "initial_marking");
    options.final_marking = parse_playout_marking(request, "final_marking");
  } catch (const std::exception& error) {
    reject(error.what());
    return;
  }
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
    // pm4py's extensive playout has no trace cap; keep the native distinct
    // trace budget unless the caller asks for fewer.
    if (request.has_param("traces")) extensive.max_traces = options.traces;
    extensive.initial_marking = options.initial_marking;
    extensive.final_marking = options.final_marking;
  }

  constexpr std::size_t kMaxGeneratedEvents = 1'000'000;
  if (options.traces != 0 && options.max_steps != 0 &&
      options.traces > kMaxGeneratedEvents / options.max_steps) {
    reject("Petri playout event budget exceeds the server safety limit");
    return;
  }

  // The playout safety bounds (max_attempts, max_firings_per_trace) and the
  // XES writer's refusal of text XML cannot carry are reported as 400 JSON,
  // like the other playout branches.
  try {
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
  } catch (const std::exception& error) {
    reject(error.what());
  }
}

// Exports the most recently mined event log â€” after any server-side filtering
// applied at mine time â€” as standard XES.
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
    // A CSV log's download is standard XES: its case_id/activity/timestamp
    // columns go out as concept:name/time:timestamp, the keys pm4py reads as
    // case:concept:name, concept:name and time:timestamp. An XES source
    // keeps its keys.
    options.standard_keys = standard_keys_for_session(*session);
    options.utc_timestamps = options.standard_keys;  // pandas-normalized UTC clock
    const bool compressed = options.compress.value_or(false);
    response.set_content(minera::serialize_xes(require_prepared_log(*session), options),
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

    // The reference port (algo.filtering.dfg.dfg_filtering) over the mined
    // DFG with the log's real activity counts.
    minera::DfgFilteringRequest filters;
    if (request.has_param("noise_threshold")) {
      filters.noise_threshold = ocel_query_number(request, "noise_threshold", 0.0);
      if (request.has_param("most_common_paths")) {
        std::vector<minera::DfgFilteringEdge> most_common_paths;
        // Labels are literal (review Y1): only the separator padding goes.
        for (const auto& pair : minera::parse_activity_pairs(
                 request.get_param_value("most_common_paths"), true)) {
          most_common_paths.emplace_back(pair.predecessor, pair.successor);
        }
        filters.most_common_paths = std::move(most_common_paths);
      }
    }
    if (request.has_param("activities_percentage")) {
      filters.activities_percentage =
          ocel_query_number(request, "activities_percentage", 0.0);
    }
    if (has_paths_percentage) {
      filters.paths_percentage =
          ocel_query_number(request, "paths_percentage", 0.0);
    }
    if (has_keep_connected) {
      filters.keep_connected = ocel_query_number(request, "keep_connected", 0.0);
    }
    filters.keep_all_activities = keep_all_activities;

    // Labels are literal: an empty value names the activity "" (pm4py takes
    // it as any other label; review Y1).
    const auto query_activity = [&](std::string_view name) {
      return request.get_param_value(std::string(name));
    };
    if (request.has_param("to_activity"))
      filters.to_activity = query_activity("to_activity");
    if (request.has_param("from_activity"))
      filters.from_activity = query_activity("from_activity");
    if (request.has_param("contain_activity"))
      filters.contain_activity = query_activity("contain_activity");
    minera::DfgFilteringData filtered;
    try {
      filtered = minera::apply_dfg_filtering_request(
          minera::dfg_filtering_data_from_graph(require_graph(*session)), filters);
    } catch (const minera::FilteringCommonError& error) {
      throw std::invalid_argument(error.type + ": " + error.message);
    }
    const auto analysis = minera::to_dfg_clean_analysis(filtered);
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
    auto params = minera::parse_mine_params(lookup, present);
    overlay_cached_source_selectors(request, *session, params, true);
    const auto table_view = cached_source_view(*session,minera::EventDataKind::kEventTable);
    const auto& table = *table_view.table();
    if (params.dfg_operation == minera::DfgTableOperation::kGraph) {
      if (params.dfg_backend == minera::DfgTableBackend::kPandas) {
        response.set_content(
            minera::serialize_dfg_dataframe_json(
                minera::discover_pandas_dfg(table, params.dfg_table_options)),
            "application/json");
      } else {
        const auto lazy_view = cached_source_view(*session,minera::EventDataKind::kLazyEventTable);
        const auto& lazy = *lazy_view.lazy_table();
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
        const auto lazy_view = cached_source_view(*session,minera::EventDataKind::kLazyEventTable);
        const auto& lazy = *lazy_view.lazy_table();
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
        const auto lazy_view = cached_source_view(*session,minera::EventDataKind::kLazyEventTable);
        const auto& lazy = *lazy_view.lazy_table();
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

void handle_model_exchange(const httplib::Request& request,
                           httplib::Response& response) {
  try {
    static const std::set<std::string> allowed = {
        "format", "output", "tree_variant", "dfg_variant",
        "artificial_start_activity", "artificial_end_activity",
        "pnml_infer_initial_marking", "pnml_infer_final_marking",
        "pnml_import_stochastic_map", "pnml_require_workflow_boundaries",
        "pnml_public_marking_semantics"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument("unknown model exchange parameter: " + key);
    }
    const auto format = request.get_param_value("format");
    if (format != "pnml" && format != "bpmn" && format != "ptml" && format != "dfg")
      throw std::invalid_argument("format must be pnml, bpmn, ptml or dfg");
    const auto output = request.has_param("output")
        ? request.get_param_value("output") : std::string("inspect");
    if (output != "inspect" && output != "pnml" && output != "bpmn" &&
        output != "ptml" && output != "dfg")
      throw std::invalid_argument("output must be inspect, pnml, bpmn, ptml or dfg");
    const auto body = ocel_request_body(request, "model");
    if (body.empty()) throw std::invalid_argument("missing model document");
    const auto quote = minera::embedding_runtime::detail::escape_json;
    std::optional<minera::PnmlDocument> document;
    std::optional<minera::ProcessTree> tree;
    std::optional<minera::DfgCleanAnalysis> dfg;
    std::string primary, tree_data = "null", arcs = "[]", pnml, canonical;
    std::string conversion_error;
    const auto tree_variant = minera::parse_process_tree_petri_net_variant(
        request.has_param("tree_variant") ? request.get_param_value("tree_variant") : "standard");
    minera::DfgPetriConversionOptions dfg_options;
    if (request.has_param("dfg_variant"))
      dfg_options.variant = minera::parse_dfg_petri_conversion_variant(
          request.get_param_value("dfg_variant"));
    if (request.has_param("artificial_start_activity"))
      dfg_options.artificial_start_activity = request.get_param_value("artificial_start_activity");
    if (request.has_param("artificial_end_activity"))
      dfg_options.artificial_end_activity = request.get_param_value("artificial_end_activity");
    if (format == "pnml") {
      minera::PnmlImportOptions options;
      options.infer_initial_marking = ocel_query_bool(request, "pnml_infer_initial_marking", true);
      options.infer_final_marking = ocel_query_bool(request, "pnml_infer_final_marking", true);
      options.import_stochastic_map = ocel_query_bool(request, "pnml_import_stochastic_map", false);
      options.require_workflow_boundaries = ocel_query_bool(request, "pnml_require_workflow_boundaries", true);
      options.public_marking_semantics = ocel_query_bool(request, "pnml_public_marking_semantics", false);
      document = minera::parse_pnml_document(body, options);
      pnml = canonical = minera::to_pnml(*document);
      primary = minera::pnml_document_json(*document);
    } else if (format == "ptml") {
      tree = minera::parse_ptml(body);
      primary = tree_data = minera::process_tree_json(*tree);
      canonical = minera::to_ptml(*tree);
      pnml = minera::to_pnml(minera::process_tree_pnml_document(*tree, tree_variant));
    } else if (format == "bpmn") {
      const auto net = minera::parse_bpmn(body);
      pnml = minera::to_pnml(net);
      // The native reader exposes a net projection, not a BPMN DOM. Keep the
      // validated original separately from a requested semantic re-export.
      canonical = body;
    } else {
      dfg = minera::parse_dfg(body);
      canonical = minera::to_dfg(*dfg);
      std::ostringstream data;
      data << "{\"family\":\"DFG_DOCUMENT\",\"graph\":[";
      bool first = true;
      for (const auto& [pair, count] : dfg->graph) {
        if (!first) data << ',';
        first = false;
        data << "{\"source\":" << quote(pair.first) << ",\"target\":"
             << quote(pair.second) << ",\"count\":" << count << '}';
      }
      const auto boundaries = [&](const char* name, const auto& values) {
        data << "],\"" << name << "\":[";
        bool start = true;
        for (const auto& [activity, count] : values) {
          if (!start) data << ',';
          start = false;
          data << "{\"activity\":" << quote(activity) << ",\"count\":" << count << '}';
        }
      };
      boundaries("start_activities", dfg->start_activities);
      boundaries("end_activities", dfg->end_activities);
      data << "]}";
      primary = data.str();
      if (output != "inspect" && output != "dfg") {
        try { pnml = minera::to_pnml(minera::convert_dfg_to_petri_net(*dfg, dfg_options)); }
        catch (const std::exception& error) { conversion_error = error.what(); }
      }
    }
    if (!document && !pnml.empty()) {
      // A converted DFG that the PNML reader cannot take back is a failed
      // conversion; the response keeps the DFG data.
      if (format == "dfg") {
        try { document = minera::parse_pnml_document(pnml); }
        catch (const std::exception& error) { conversion_error = error.what(); pnml.clear(); }
      } else {
        document = minera::parse_pnml_document(pnml);
      }
    }
    if (format == "bpmn") primary = minera::pnml_document_json(*document);
    if (document && document->arc_occurrences) {
      std::ostringstream data; data << '[';
      for (std::size_t i = 0; i < document->arc_occurrences->size(); ++i) {
        if (i) data << ',';
        const auto& arc = document->arc_occurrences->at(i);
        data << "{\"id\":" << quote(arc.id) << ",\"place\":" << arc.place
             << ",\"transition\":" << arc.transition
             << ",\"input\":" << (arc.input ? "true" : "false")
             << ",\"type\":\""
             << (arc.type == minera::PetriArcType::kNormal ? "normal" :
                 arc.type == minera::PetriArcType::kReset ? "reset" : "inhibitor")
             << "\",\"weight\":" << (arc.negative_weight && arc.weight ? "-" : "") << arc.weight << '}';
      }
      data << ']'; arcs = data.str();
    }
    std::string exported;
    if (output != "inspect" && conversion_error.empty()) {
      try {
        if (output == format && format != "bpmn") exported = canonical;
        else if (output == "pnml") exported = pnml;
        else if (output == "dfg") throw std::invalid_argument("DFG export requires a DFG input");
        else {
          // These converters consume workflow topology, not arbitrary document markings.
          if (!tree) {
            if (!document) throw std::invalid_argument("no converted workflow net");
            const auto single = [&](const auto& marking, std::size_t boundary) {
              return boundary < marking.size() && marking[boundary] == 1 &&
                  std::all_of(marking.begin(), marking.end(), [&](const auto& count) {
                    return &count == &marking[boundary] || count == 0;
                  });
            };
            if (!single(document->initial_marking, document->net.source()) ||
                !single(document->final_marking, document->net.sink()))
              throw std::invalid_argument("BPMN/PTML conversion requires single source/sink token markings; the full PNML remains available");
            if (document->net.has_extended_arcs() ||
                std::any_of(document->arc_occurrences->begin(), document->arc_occurrences->end(),
                    [](const auto& arc) { return arc.weight != 1 || arc.negative_weight; }))
              throw std::invalid_argument("BPMN/PTML conversion cannot preserve this document's extended or weighted arcs; the full PNML remains available");
          }
          if (output == "bpmn")
            exported = tree ? minera::to_bpmn(*tree) : minera::to_bpmn(document->net);
          else {
            if (!tree) tree = minera::to_process_tree(document->net);
            tree_data = minera::process_tree_json(*tree);
            exported = minera::to_ptml(*tree);
          }
        }
        if (exported.empty()) throw std::invalid_argument("native conversion returned no document");
      } catch (const std::exception& error) { conversion_error = error.what(); exported.clear(); }
    }
    response.set_content(
        "{\"family\":\"MODEL_EXCHANGE\",\"format\":" + quote(format) +
        ",\"output_format\":" + quote(output) + ",\"data\":" + primary +
        ",\"net\":" + (document ? minera::pnml_document_json(*document) : "null") +
        ",\"tree\":" + tree_data + ",\"arcs\":" + arcs +
        ",\"canonical\":" + quote(canonical) +
        ",\"pnml\":" + (pnml.empty() ? "null" : quote(pnml)) +
        ",\"exported\":" + (exported.empty() ? "null" : quote(exported)) +
        ",\"conversion_error\":" + (conversion_error.empty() ? "null" : quote(conversion_error)) + "}",
        "application/json");
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
  if (handle_literal_controls_request(request, response, "dfg_precision")) return;
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
    if (cached_source_uses_bound_defaults(*session) && !request.has_param("activity_attribute")) options.activity_attribute = session->active_source->selectors().activity_key;
    if (cached_source_uses_bound_defaults(*session)) options.case_id_attribute = session->active_source->selectors().case_id_key;
    if (options.activity_attribute.empty())
      throw std::invalid_argument("activity_attribute must not be empty");
    const auto body = ocel_request_body(request, "dfg");
    if (body.empty()) throw std::invalid_argument("missing DFG text body");
    response.set_content(
        minera::serialize_dfg_precision_json(
            minera::evaluate_dfg_precision(
                session->active_source ? session->active_source->selected_view() : minera::EventDataView(require_prepared_log(*session)), minera::parse_dfg(body), options)),
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

// Streaming uploads use a fresh monitor per request. Keep the historical
// importer order by default; callers can replay parser-assigned CSV input order.
// The DFG, footprints, temporal and TBR routes skip an event without a case
// id, as live sessions do (the CSV importer marks every id present, so a CSV
// upload keeps all its events); parsed_events still counts every event.
std::vector<const minera::Event*> streaming_events(
    const minera::EventLog& log, const httplib::Request& request) {
  const auto order = request.has_param("streaming_event_order")
      ? request.get_param_value("streaming_event_order") : "imported";
  if (order != "input" && order != "imported")
    throw std::invalid_argument("streaming_event_order must be input or imported");
  std::vector<const minera::Event*> events;
  for (const auto& trace : log.traces)
    for (const auto& event : trace.events) events.push_back(&event);
  if (order == "input")
    std::stable_sort(events.begin(), events.end(), [](const auto* a, const auto* b) {
      return a->input_order < b->input_order;
    });
  return events;
}

std::string streaming_diagnostics_json(
    std::string payload, const httplib::Request& request,
    std::size_t parsed_events, const std::string& extra = {}) {
  if (!ocel_query_bool(request, "streaming_include_diagnostics", false))
    return payload;
  const auto order = request.has_param("streaming_event_order")
      ? request.get_param_value("streaming_event_order") : "imported";
  payload.pop_back();
  return payload + ",\"input\":{\"event_order\":\"" + order +
      "\",\"parsed_events\":" + std::to_string(parsed_events) + "}" + extra + "}";
}

void handle_streaming_dfg(const httplib::Request& request,
                          httplib::Response& response) {
  try {
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (key != "streaming_case_id_column" &&
          key != "streaming_activity_column" &&
          key != "streaming_event_order" &&
          key != "streaming_include_diagnostics") {
        throw std::invalid_argument("unknown streaming DFG parameter: " + key);
      }
    }
    minera::CsvImportOptions import_options;
    import_options.keep_na_first_class_values = true;  // stream reader rows
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
    const auto events = streaming_events(log, request);
    for (const auto* event : events)
      if (minera::streaming_monitor_takes_event(*event, minera::StreamingMonitorKind::kDfg)) stream.receive(*event, event->case_id);
    response.set_content(
        streaming_diagnostics_json(
            minera::serialize_streaming_dfg_json(stream.snapshot()), request,
            events.size()),
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
        "streaming_footprints_terminate_all", "streaming_event_order",
        "streaming_include_diagnostics"};
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (!allowed.contains(key))
        throw std::invalid_argument(
            "unknown streaming footprints parameter: " + key);
    }
    minera::CsvImportOptions import_options;
    import_options.keep_na_first_class_values = true;  // stream reader rows
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
    const auto events = streaming_events(log, request);
    for (const auto* event : events)
      if (minera::streaming_monitor_takes_event(*event, minera::StreamingMonitorKind::kFootprints)) (void)monitor.receive(*event, event->case_id);
    std::string extra;
    if (ocel_query_bool(request, "streaming_footprints_terminate_all", false)) {
      const auto prefix = monitor.snapshot();
      std::ostringstream terminated;
      terminated << ",\"prefix\":"
                 << minera::serialize_streaming_footprints_json(prefix)
                 << ",\"terminated_cases\":[";
      bool first = true;
      for (const auto& [case_id, state] : prefix.cases) {
        (void)state;
        if (!first) terminated << ',';
        first = false;
        const bool fit = monitor.terminate_case(case_id);
        terminated << "{\"case_id\":"
                   << minera::embedding_runtime::detail::escape_json(case_id)
                   << ",\"is_fit\":" << (fit ? "true" : "false") << '}';
      }
      terminated << ']';
      extra = terminated.str();
    }
    response.set_content(
        streaming_diagnostics_json(
            minera::serialize_streaming_footprints_json(monitor.snapshot()),
            request, events.size(), extra),
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
    auto params = minera::parse_mine_params(lookup, present);
    const auto variant = present("temporal_variant")
                             ? request.get_param_value("temporal_variant")
                             : std::string{"log"};
    overlay_cached_source_selectors(request, *session, params, variant == "dataframe");
    auto options = params.temporal_profile;
    // This route is an explicit conformance request, unlike bundled discovery.
    // Preserve the strict standalone component default unless it is overridden.
    if (!present("temporal_strict_attributes")) {
      options.strict_attributes = true;
    }
    const auto profile = parse_temporal_profile_parameter(
        request.get_param_value("temporal_profile"), "temporal_profile");
    minera::TemporalConformanceResult result;
    if (variant == "log") {
      result = minera::conform_temporal_profile_log(require_prepared_log(*session), profile,
                                                    options);
    } else if (variant == "dataframe") {
      result = minera::conform_temporal_profile_dataframe(
          *cached_source_view(*session,minera::EventDataKind::kEventTable).table(), profile, options);
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
        "streaming_temporal_zeta", "streaming_event_order",
        "streaming_include_diagnostics"};
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
    import_options.keep_na_first_class_values = true;  // stream reader rows
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
    const auto events = streaming_events(log, request);
    for (const auto* event : events)
      if (minera::streaming_monitor_takes_event(*event, minera::StreamingMonitorKind::kTemporal)) (void)monitor.receive(*event, event->case_id);
    response.set_content(
        streaming_diagnostics_json(
            minera::serialize_streaming_temporal_json(monitor.snapshot()),
            request, events.size()),
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
        "streaming_tbr_stop_immediately", "streaming_tbr_terminate_all",
        "streaming_event_order", "streaming_include_diagnostics"};
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
    import_options.keep_na_first_class_values = true;  // stream reader rows
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
    const auto events = streaming_events(log, request);
    for (const auto* event : events)
      if (minera::streaming_monitor_takes_event(*event, minera::StreamingMonitorKind::kTbr)) (void)replay.receive(*event, event->case_id);
    std::string extra;
    if (ocel_query_bool(request, "streaming_tbr_terminate_all", false)) {
      const auto prefix = replay.snapshot();
      std::ostringstream terminated;
      terminated << ",\"prefix\":"
                 << minera::serialize_streaming_tbr_json(prefix)
                 << ",\"terminated_cases\":[";
      bool first = true;
      for (const auto& [case_id, state] : prefix.cases) {
        (void)state;
        const auto result = replay.terminate_case(case_id);
        if (!result) throw std::logic_error("streaming case disappeared");
        if (!first) terminated << ',';
        first = false;
        terminated << "{\"case_id\":"
                   << minera::embedding_runtime::detail::escape_json(case_id)
                   << ",\"missing\":" << result->missing
                   << ",\"remaining\":" << result->remaining
                   << ",\"is_fit\":" << (result->is_fit ? "true" : "false")
                   << ",\"marking\":[";
        for (std::size_t i = 0; i < result->marking.size(); ++i) {
          if (i) terminated << ',';
          terminated << result->marking[i];
        }
        terminated << "]}";
      }
      terminated << ']';
      extra = terminated.str();
    }
    response.set_content(
        streaming_diagnostics_json(
            minera::serialize_streaming_tbr_json(replay.snapshot()),
            request, events.size(), extra),
        "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

#include "live_stream_routes.hpp"
#include "reader_stream_routes.hpp"
#include "source_acquisition_routes.hpp"

void handle_streaming_declare(const httplib::Request& request,httplib::Response& response) {
  live_response(response,[&]{
    live_params(request,{"streaming_declare_case_id_column","streaming_declare_activity_column","streaming_declare_timestamp_column","streaming_event_order","streaming_include_diagnostics","format"});
    const auto model_body=form_field(request,"model"),log_body=form_field(request,"log");if(model_body.empty()||log_body.empty())throw std::invalid_argument("Declare replay requires original model and event uploads");
    const auto model=minera::parse_declare_stream_model_json(model_body);minera::DeclareStreamingConformance monitor(model);minera::CsvImportOptions csv;csv.keep_na_first_class_values=true;
    if(request.has_param("streaming_declare_case_id_column"))csv.case_id_column=request.get_param_value("streaming_declare_case_id_column");if(request.has_param("streaming_declare_activity_column"))csv.activity_column=request.get_param_value("streaming_declare_activity_column");if(request.has_param("streaming_declare_timestamp_column"))csv.timestamp_column=request.get_param_value("streaming_declare_timestamp_column");
    const auto format=request.has_param("format")?request.get_param_value("format"):"csv";std::istringstream input(log_body);minera::EventLog log;
    if(format=="csv")log=minera::import_csv_event_log(input,csv);else if(format=="xes")log=minera::import_xes_dataframe_log(input);else throw std::invalid_argument("Declare events require CSV or XES");
    const auto events=streaming_events(log,request);std::ostringstream rows;rows<<'[';for(std::size_t i=0;i<events.size();++i){if(i)rows<<',';rows<<minera::serialize_declare_stream_event_json(monitor.receive(*events[i]));}rows<<']';
    auto result=minera::serialize_declare_streaming_json(monitor.snapshot(),model);result.pop_back();result+=",\"event_results\":"+rows.str()+"}";
    response.set_content(streaming_diagnostics_json(std::move(result),request,events.size()),"application/json");
  });
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

// ---- /api route registry -------------------------------------------------
// Every /api route is registered through api_get/api_post so the server
// knows which methods a path serves (for 405 responses) and so a repeated
// scalar control is rejected instead of silently keeping its first value.
std::map<std::string, std::set<std::string>> g_api_route_methods;

// Routes whose own contract owns repeated controls: the exact/original
// workbenches and reference adapters record every submitted part and report
// duplicates inside their structured envelopes, and the live/reader/source
// session routes validate repeats themselves.
bool api_route_owns_repeated_controls(const std::string& path,
                                      const httplib::Request& request) {
  if (path.find("workbench") != std::string::npos) return true;
  for (const std::string_view prefix :
       {"/api/original-", "/api/reference-", "/api/structural-inputs",
        "/api/source-acquisition/", "/api/live-stream/", "/api/stream-reader/",
        "/api/discover-inductive-input", "/api/ocel/conformance",
        "/api/knowledge-injection", "/api/log-query", "/api/llm-context",
        "/api/log-cluster"})
    if (std::string_view(path).starts_with(prefix)) return true;
  // The exact-control adapter of these routes reports duplicates itself.
  if ((path == "/api/mine" || path == "/api/discover-workflow" ||
       path == "/api/discover-workflow-input") &&
      exact_filter_discovery_route::requested(request))
    return true;
  return false;
}

// The first control supplied more than once, counting query parameters and
// multipart fields together (a key in both places is also a repeat).
std::optional<std::string> repeated_request_control(const httplib::Request& request) {
  std::map<std::string, std::size_t> counts;
  for (const auto& [key, value] : request.params) {
    (void)value;
    if (++counts[key] > 1) return key;
  }
  for (const auto& [key, value] : request.files) {
    (void)value;
    if (++counts[key] > 1) return key;
  }
  return std::nullopt;
}

// Adds `token` to the response's Vary list unless it is already named, so the
// CORS (Origin) and compression (Accept-Encoding) variations both reach a cache.
void add_vary(httplib::Response& response, const std::string& token) {
  const auto lower = [](std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
  };
  std::string merged;
  for (const auto& [name, value] : response.headers) {
    if (lower(name) != "vary") continue;
    std::istringstream items(value);
    for (std::string item; std::getline(items, item, ',');) {
      const auto first = item.find_first_not_of(" \t");
      if (first == std::string::npos) continue;
      item = item.substr(first, item.find_last_not_of(" \t") - first + 1);
      if (item == "*" || lower(item) == lower(token)) return;
    }
    merged += (merged.empty() ? "" : ", ") + value;
  }
  for (auto it = response.headers.begin(); it != response.headers.end();)
    it = lower(it->first) == "vary" ? response.headers.erase(it) : std::next(it);
  response.set_header("Vary", merged.empty() ? token : merged + ", " + token);
}

std::uint64_t current_imported_model_id() {
  std::lock_guard<std::mutex> lock(g_session_mutex);
  return g_session ? g_session->imported_model_id : 0;
}

void reject_stale_imported_model(httplib::Response& response) {
  // Drop what the handler produced (body, downloads, providers) but keep the
  // CORS headers the pre-routing handler set.
  httplib::Response fresh;
  for (const char* name : {"Access-Control-Allow-Origin", "Access-Control-Expose-Headers", "Vary"})
    if (response.has_header(name))
      fresh.set_header(name, response.get_header_value(name));
  // Response's defaulted move-assignment drops a content-provider releaser
  // without running it (a published mining ticket would then never be
  // released), so move the handler's response aside: its destructor runs the
  // releaser, reporting the body as unsent.
  httplib::Response discarded = std::move(response);
  response.content_provider_resource_releaser_ = nullptr;
  response = std::move(fresh);
  response.status = 409;
  response.set_content(
      // The text matches STALE_IMPORTED_MODEL_MESSAGE in the frontend's
      // imported-model-binding.ts, which recognises this refusal.
      error_json("The server's imported model is no longer the one this page checked; "
                 "check the model again, then rerun this request."),
      "application/json");
}

// Every /api JSON body leaves as valid UTF-8 (json_utf8.hpp): results that
// carry a log's own bytes (a Latin-1 CSV, WTF-8 surrogates) are repaired. The
// repair must run before httplib frames the response: apply_ranges sets
// Content-Length (and slices a Range request) before the post-routing
// handler runs, so a repair there would grow the body past its declared
// length. Handlers, the pre-routing and the error handler call this; the
// check is a single pass and clean bodies are left untouched.
// XML and text bodies (PNML, BPMN, DFG, CSV exports) take the same U+FFFD
// policy as raw characters (repair_text_utf8), so they stay well-formed UTF-8;
// a body that names another charset, and binary types, are left alone.
// Returns whether the body changed.
bool repair_api_text_body(std::string& body, const std::string& content_type) {
  if (body.empty()) return false;
  std::string type = content_type;
  std::transform(type.begin(), type.end(), type.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  if (type.starts_with("application/json")) {
    if (minera::detail::is_clean_utf8(body)) return false;
    body = active_source_safe_json(body);
    return true;
  }
  const auto media = type.substr(0, type.find(';'));
  const bool xml = media == "application/xml" || media == "text/xml" || media.ends_with("+xml");
  if (!xml && !media.starts_with("text/")) return false;
  if (const auto charset = type.find("charset="); charset != std::string::npos) {
    auto name = type.substr(charset + 8);
    name = name.substr(0, name.find_first_of("; "));
    if (!name.empty() && name.front() == '"') name = name.substr(1, name.find('"', 1) - 1);
    if (name != "utf-8" && name != "utf8") return false;
  }
  if (minera::detail::is_clean_text_utf8(body, xml)) return false;
  body = minera::detail::repair_text_utf8(body, xml);
  return true;
}

void repair_api_json_body(httplib::Response& response) {
  repair_api_text_body(response.body, response.get_header_value("Content-Type"));
}

// Handlers answer any std::exception with 400 and its message, so the
// exception's type is gone by the time the route wrapper sees the response.
// The new-handler (which every standard library's throwing operator new calls
// before giving up) therefore throws a std::bad_alloc whose message carries a
// marker, and the wrapper looks for the marker in the 400 body. A failed
// nothrow allocation (new (std::nothrow), std::get_temporary_buffer's size
// probes) may run the new-handler too, but its exception is caught inside
// the allocation and never reaches a response, so it leaves no trace; no
// per-thread state is kept.
constexpr const char* kOutOfMemoryMarker = "std::bad_alloc (minera: out of memory)";

class MarkedOutOfMemory : public std::bad_alloc {
 public:
  const char* what() const noexcept override { return kOutOfMemoryMarker; }
};

void record_allocation_failure() { throw MarkedOutOfMemory(); }

// A 400 that carries the marked out-of-memory message, or exactly the
// message of a std::bad_alloc thrown elsewhere ("std::bad_alloc", MSVC's
// "bad allocation").
bool out_of_memory_response(const httplib::Response& response) {
  if (response.status != 400) return false;
  static const std::string what = std::bad_alloc().what();
  return response.body == error_json(what) ||
         response.body.find(kOutOfMemoryMarker) != std::string::npos;
}

// Running out of memory is the server's condition, not a malformed request.
void report_out_of_memory(httplib::Response& response) {
  if (!out_of_memory_response(response)) return;
  response.status = 503;
  response.set_content(
      error_json("the server ran out of memory for this request; retry with a smaller input "
                 "or give minera_server more memory"),
      "application/json");
}

httplib::Server::Handler api_route_handler(std::string path,
                                           httplib::Server::Handler handler) {
  return [path = std::move(path), handler = std::move(handler)](
             const httplib::Request& request, httplib::Response& response) {
    struct RepairOnExit {
      httplib::Response& response;
      ~RepairOnExit() { try { report_out_of_memory(response); repair_api_json_body(response); } catch (...) {} }
    } repair{response};
    if (!api_route_owns_repeated_controls(path, request)) {
      if (const auto repeated = repeated_request_control(request)) {
        response.status = 400;
        response.set_content(
            error_json("parameter " + *repeated +
                       " is supplied more than once; send each control once"),
            "application/json");
        return;
      }
    }
    // A request on the newly mined source confirms its mining response.
    confirm_mining_source(request);
    // A request bound to an imported model runs only when that model is the
    // server's imported model both before and after the handler: ids are
    // never reused, so the handler's own session snapshot held it too.
    // Mining replaces the session (and so the imported model) by design, so a
    // stale header must not turn a published mine into a 409.
    if (path != "/api/check-model" && path != "/api/mine" &&
        request.has_header("X-Minera-Imported-Model")) {
      const auto expected = request.get_header_value("X-Minera-Imported-Model");
      if (expected != std::to_string(current_imported_model_id())) {
        reject_stale_imported_model(response);
        return;
      }
      handler(request, response);
      if (expected != std::to_string(current_imported_model_id()))
        reject_stale_imported_model(response);
      return;
    }
    handler(request, response);
    // A handler that read or wrote a large body leaves its working set on the
    // allocator's free lists; small requests skip the (process-wide) trim.
    constexpr std::size_t large_body = std::size_t{8} << 20;
    std::size_t uploaded = request.body.size();
    for (const auto& [name, file] : request.files) {(void)name; uploaded += file.content.size();}
    if (uploaded >= large_body || response.body.size() >= large_body)
      minera_release_free_memory();
  };
}

void api_get(httplib::Server& server, const std::string& path,
             httplib::Server::Handler handler) {
  g_api_route_methods[path].insert("GET");
  server.Get(path, api_route_handler(path, std::move(handler)));
}

void api_post(httplib::Server& server, const std::string& path,
              httplib::Server::Handler handler) {
  g_api_route_methods[path].insert("POST");
  server.Post(path, api_route_handler(path, std::move(handler)));
}

// Streaming-body routes read their own upload; they are workbench routes
// whose envelopes own repeated controls, so only the method is recorded.
void api_post(httplib::Server& server, const std::string& path,
              httplib::Server::HandlerWithContentReader handler) {
  g_api_route_methods[path].insert("POST");
  server.Post(path, [handler = std::move(handler)](const httplib::Request& request,
                                                   httplib::Response& response,
                                                   const httplib::ContentReader& reader) {
    handler(request, response, reader);
    report_out_of_memory(response);
    repair_api_json_body(response);
  });
}

// Gives /api error statuses without a body (unknown routes, wrong methods,
// oversized payloads) the same JSON error shape as the handlers' errors.
httplib::Server::HandlerResponse api_error_body(const httplib::Request& request,
                                                httplib::Response& response) {
  if (!request.path.starts_with("/api/") || !response.body.empty())
    return httplib::Server::HandlerResponse::Unhandled;
  std::string message;
  const auto route = g_api_route_methods.find(request.path);
  if (response.status == 404 && route != g_api_route_methods.end() &&
      !route->second.contains(request.method == "HEAD" ? "GET" : request.method)) {
    std::string allow;
    for (const auto& method : route->second) allow += (allow.empty() ? "" : ", ") + method;
    response.status = 405;
    response.set_header("Allow", allow);
    message = "method " + request.method + " is not allowed for " + request.path +
              "; use " + allow;
  } else if (response.status == 404 && route == g_api_route_methods.end()) {
    message = "unknown API route: " + request.path;
  } else if (response.status == 408) {
    message = "the request arrived too slowly: the server reads the request head within " +
              std::to_string(CPPHTTPLIB_HEADER_READ_TIMEOUT_SECOND) + " s and the body within " +
              std::to_string(CPPHTTPLIB_BODY_READ_TIMEOUT_SECOND) + " s, at a minimum average of " +
              std::to_string(CPPHTTPLIB_BODY_MIN_RATE_BYTES_PER_SECOND / 1024) +
              " KiB/s after the first " + std::to_string(CPPHTTPLIB_BODY_MIN_RATE_GRACE_SECOND) +
              " s";
  } else if (response.status == 413) {
    message = "the upload is larger than the " + std::to_string(kMaxRequestPayloadBytes >> 20) +
              (request.path.starts_with("/api/ocel") ? " MiB server limit"
                                                      : " MiB server limit; compress an XES log as .xes.gz");
  } else if (response.status == 414) {
    message = "the request line is longer than the server's " +
              std::to_string(CPPHTTPLIB_REQUEST_URI_MAX_LENGTH) +
              "-byte limit; send long controls in the request body";
  } else if (response.status == 400 && request.has_header("Content-Length") &&
             request.has_header("Transfer-Encoding")) {
    message = "a request must not carry both Transfer-Encoding and Content-Length";
  } else if (response.status == 400 && httplib::detail::has_ambiguous_framing(request.headers)) {
    message = "the request carries Content-Length headers that disagree";
  } else if (response.status == 400 && request.has_header("Content-Length") &&
             !httplib::detail::content_length_is_decimal(
                 request.get_header_value("Content-Length"), false)) {
    message = "the Content-Length header must be a non-negative decimal number";
  } else {
    message = std::string(httplib::status_message(response.status));
    if (message.empty()) message = "HTTP " + std::to_string(response.status);
  }
  response.set_content(error_json(message), "application/json");
  return httplib::Server::HandlerResponse::Handled;
}

int usage(const char* program) {
  std::cerr << "Usage: " << program
            << " [options]\n"
               "  --port <port>      Listen port (default: 8484)\n"
               "  --frontend <dir>   Static frontend directory"
               " (default: src/frontend/dist, relative to the working directory)\n"
               "  --embedding-runtime-url <url> HTTP transformer runtime (POST /embed)\n"
               "  --llm-runtime-url <url> Optional language-model runtime (POST /complete)\n"
               "  --query-runtime-url <url> Optional DuckDB query runtime (POST /query)\n"
               "  --max-xes-decompressed-bytes <n> Highest XES size (plain or after gzip\n"
               "                     expansion) a request may ask for with\n"
               "                     xes_max_decompressed_bytes (default: 2147483648;\n"
               "                     requests default to 268435456)\n"
               "  --mine-memory-budget <bytes> Memory a single /api/mine may plan\n"
               "                     for (suffix K, M, G or T allowed; default: half of\n"
               "                     the physical memory, or of a lower cgroup or\n"
               "                     address-space limit); a larger log gets\n"
               "                     413 MINE_LIMIT before it is parsed\n"
               "  --max-mine-events <n> Most events a single /api/mine accepts\n"
               "                     (default: no limit besides the memory budget)\n"
               "  --allow-origin <origin> Extra browser origin allowed to call /api"
               " (repeatable; the server's own port and the Vite ports 5173/4173"
               " on 127.0.0.1 and localhost are always allowed)\n";
  return 2;
}

// True when another socket already holds 127.0.0.1:<port>: a probe socket with
// the server's own options fails to bind with "address in use" (Windows reports
// a port held under SO_EXCLUSIVEADDRUSE as access denied).
bool loopback_port_in_use(int port) {
  const auto sock = ::socket(AF_INET, SOCK_STREAM, 0);
  if (sock == INVALID_SOCKET) return false;
  httplib::default_socket_options(sock);
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(static_cast<std::uint16_t>(port));
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  const bool in_use =
      ::bind(sock, reinterpret_cast<const sockaddr*>(&address),
             sizeof(address)) != 0 &&
#ifdef _WIN32
      (WSAGetLastError() == WSAEADDRINUSE || WSAGetLastError() == WSAEACCES);
#else
      errno == EADDRINUSE;
#endif
  httplib::detail::close_socket(sock);
  return in_use;
}

}  // namespace

int main(int argc, char** argv) {
#if defined(__GLIBC__)
  // Bound the per-thread arenas and map large buffers (logs, response bodies)
  // directly, so memory freed after a request returns to the system. Four
  // arenas let the alignment searches that run side by side on a large log
  // (up to four threads, see check_alignment) allocate without queueing on
  // one arena lock: with two, four threads ran about as fast as one.
#if defined(M_ARENA_MAX)
  mallopt(M_ARENA_MAX, 4);
#endif
#if defined(M_MMAP_THRESHOLD)
  mallopt(M_MMAP_THRESHOLD, 1 << 20);
#endif
#endif
  // A failed allocation throws a marked std::bad_alloc, so the route
  // wrapper can answer 503 (report_out_of_memory).
  std::set_new_handler(record_allocation_failure);
  minera::set_analysis_trace_run_id(new_analysis_trace_run_id());
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
      // The whole value must be the port number; "8484abc" is not 8484.
      const std::string_view text = value;
      const auto parsed = std::from_chars(text.data(), text.data() + text.size(), port);
      if (text.empty() || parsed.ec != std::errc{} ||
          parsed.ptr != text.data() + text.size() || port <= 0 || port > 65535) {
        std::cerr << "error: --port expects an integer from 1 through 65535, got '"
                  << text << "'\n";
        return usage(argv[0]);
      }
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
    } else if (arg == "--max-xes-decompressed-bytes") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      const std::string_view text = value;
      std::size_t ceiling = 0;
      const auto parsed = std::from_chars(text.data(), text.data() + text.size(), ceiling);
      const auto floor = minera::XesImportOptions{}.max_decompressed_bytes;
      if (text.empty() || parsed.ec != std::errc{} ||
          parsed.ptr != text.data() + text.size() || ceiling < floor) {
        std::cerr << "error: --max-xes-decompressed-bytes expects a byte count of at least "
                  << floor << " (the per-request default), got '" << text << "'\n";
        return usage(argv[0]);
      }
      minera::set_xes_decompressed_ceiling(ceiling);
    } else if (arg == "--mine-memory-budget" || arg == "--max-mine-events") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      std::string_view text = value;
      std::uint64_t scale = 1;
      if (arg == "--mine-memory-budget" && !text.empty()) {
        const char unit = static_cast<char>(std::toupper(static_cast<unsigned char>(text.back())));
        const int shift = unit == 'K' ? 10 : unit == 'M' ? 20 : unit == 'G' ? 30 : unit == 'T' ? 40 : 0;
        if (shift != 0) {scale = std::uint64_t{1} << shift; text.remove_suffix(1);}
      }
      std::uint64_t amount = 0;
      const auto parsed = std::from_chars(text.data(), text.data() + text.size(), amount);
      if (text.empty() || parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() ||
          amount == 0 || amount > UINT64_MAX / scale) {
        std::cerr << "error: " << arg << " expects a positive "
                  << (arg == "--max-mine-events" ? "event count" : "byte count (suffix K, M, G or T allowed)")
                  << ", got '" << value << "'\n";
        return usage(argv[0]);
      }
      (arg == "--max-mine-events" ? g_max_mine_events : g_mine_memory_budget) = amount * scale;
    } else if (arg == "--allow-origin") {
      const auto* value = next();
      if (!value || !*value || std::string_view(value) == "*") return usage(argv[0]);
      std::string origin = value;
      while (origin.ends_with('/')) origin.pop_back();
      // Browsers send Origin (and Host) lowercased and without the scheme's
      // default port, so store the origin in that form to match both.
      std::transform(origin.begin(), origin.end(), origin.begin(),
                     [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
      if (origin.starts_with("http://") && origin.ends_with(":80"))
        origin.resize(origin.size() - 3);
      else if (origin.starts_with("https://") && origin.ends_with(":443"))
        origin.resize(origin.size() - 4);
      g_allowed_origins.insert(origin);
    } else {
      return usage(argv[0]);
    }
  }

  try {
    if (!embedding_runtime_url.empty()) {
      auto callback = minera::embedding_runtime::make_http_provider(embedding_runtime_url);
      minera::set_default_embedding_provider(callback);
      g_embedding_explicit_runtime = std::move(callback);
    }
    g_embedding_runtime_enabled = !embedding_runtime_url.empty();
    if (!g_llm_runtime_url.empty()) minera::llm_runtime::validate_url(g_llm_runtime_url);
    if (!g_query_runtime_url.empty()) minera::llm_runtime::validate_url(g_query_runtime_url);
    for (const auto local_port : {port, 5173, 4173}) {
      g_allowed_origins.insert("http://127.0.0.1:" + std::to_string(local_port));
      g_allowed_origins.insert("http://localhost:" + std::to_string(local_port));
    }
  } catch (const std::exception& error) {
    std::cerr << "error: " << error.what() << '\n';
    return 2;
  }

  httplib::Server server;
  // Bound multipart bodies before the HTTP library buffers or parses them.
  server.set_payload_max_length(kMaxRequestPayloadBytes);
  // JSON, XML and text bodies are repaired before httplib frames them (repair_api_json_body
  // in the route wrappers, the pre-routing and the error handler). This last
  // step is a safety net that keeps the framing honest: a body still holding
  // ill-formed bytes is repaired and its Content-Length recomputed. A Range
  // response (Content-Range) is a slice of the already repaired body and may
  // end inside a character, so it is left exactly as sliced.
  server.set_post_routing_handler([](const httplib::Request& request, httplib::Response& response) {
#if defined(CPPHTTPLIB_ZLIB_SUPPORT)
    // Text and JSON bodies are gzipped for a client that asks, so the
    // representation varies with Accept-Encoding (merged with Vary: Origin).
    if (httplib::detail::can_compress_content_type(response.get_header_value("Content-Type")))
      add_vary(response, "Accept-Encoding");
#endif
    // A gzipped body was repaired before it was compressed (repair_api_json_body).
    if (!request.path.starts_with("/api/") || response.has_header("Content-Range") ||
        response.has_header("Content-Encoding") ||
        !repair_api_text_body(response.body, response.get_header_value("Content-Type")))
      return;
    if (response.has_header("Content-Length")) {
      response.headers.erase("Content-Length");  // set_header adds, not replaces
      response.set_header("Content-Length", std::to_string(response.body.size()));
    }
  });
  server.set_error_handler([](const httplib::Request& request, httplib::Response& response) {
    const auto handled = api_error_body(request, response);
    repair_api_json_body(response);
    return handled;
  });

  // The app is local-first. Browsers attach Origin to cross-origin and
  // same-origin POSTs; only the app's own origins (this port, the Vite dev and
  // preview ports, and any --allow-origin) may call the API, so a page open
  // elsewhere in the browser can neither read responses nor trigger routes.
  // Same-origin GETs carry no Origin, so a DNS-rebinding page would otherwise
  // read the API as if it were the app. The Host header must name this server
  // (or an allowed origin's host), which a rebound attacker host cannot.
  std::set<std::string> allowed_hosts;
  for (const auto& origin : g_allowed_origins) {
    const auto scheme = origin.find("://");
    allowed_hosts.insert(scheme == std::string::npos ? origin : origin.substr(scheme + 3));
  }
  allowed_hosts.insert("[::1]:" + std::to_string(port));
  if (port == 80) allowed_hosts.insert({"127.0.0.1", "localhost", "[::1]"});
  server.set_pre_routing_handler([allowed_hosts](const httplib::Request& request, httplib::Response& response) {
    if (request.path.starts_with("/api/") && request.has_header("Host")) {
      auto host = request.get_header_value("Host");
      std::transform(host.begin(), host.end(), host.begin(),
                     [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
      if (!allowed_hosts.contains(host)) {
        response.status = 421;
        response.set_content(error_json("Host " + host + " is not served by this Minera server"), "application/json");
        repair_api_json_body(response);
        return httplib::Server::HandlerResponse::Handled;
      }
    }
    if (!request.has_header("Origin")) return httplib::Server::HandlerResponse::Unhandled;
    const auto origin = request.get_header_value("Origin");
    add_vary(response, "Origin");
    if (g_allowed_origins.contains(origin)) {
      response.set_header("Access-Control-Allow-Origin", origin);
      response.set_header("Access-Control-Expose-Headers", kExposedResponseHeaders);
      return httplib::Server::HandlerResponse::Unhandled;
    }
    if (!request.path.starts_with("/api/")) return httplib::Server::HandlerResponse::Unhandled;
    response.status = 403;
    response.set_content(error_json("Origin " + origin + " is not allowed to call the Minera API; start the server with --allow-origin to permit it"), "application/json");
    repair_api_json_body(response);
    return httplib::Server::HandlerResponse::Handled;
  });
  const auto preflight = [](const httplib::Request&, httplib::Response& response) {
    response.set_header("Access-Control-Allow-Methods", "POST, OPTIONS");
    response.set_header("Access-Control-Allow-Headers", "Content-Type, X-Minera-Imported-Model");
    response.status = 204;
  };
  server.Options("/api/mine", preflight);
  server.Options("/api/log-query", preflight);
  server.Options("/api/log-query/export", preflight);
  server.Options("/api/reference-log-filter", preflight);
  server.Options("/api/log-cluster", preflight);
  server.Options("/api/llm-context", preflight);
  server.Options("/api/original-log-to-features", preflight);
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
  server.Options("/api/clustering/export", preflight);
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
  server.Options("/api/generate-trees", preflight);
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
  server.Options("/api/discover-constraints", preflight);
  server.Options("/api/discover-graph", preflight);
  server.Options("/api/discover-timing", preflight);
  server.Options("/api/analyze-behavior", preflight);
  server.Options("/api/discover-dfg", preflight);
  server.Options("/api/evaluate-replay-results", preflight);
  server.Options("/api/align-edit-distance", preflight);
  server.Options("/api/decide", preflight);
  server.Options("/api/decision-tables", preflight);
  server.Options("/api/data-petri-net", preflight);
  server.Options("/api/check-model", preflight);
  server.Options("/api/token-replay", preflight);
  server.Options("/api/replay-token-workbench", preflight);
  server.Options("/api/analyze-replay-duration-workbench", preflight);
  server.Options("/api/align-petri-workbench", preflight);
  server.Options("/api/align-graph-workbench", preflight);
  server.Options("/api/discover-inductive-input", preflight);
  server.Options("/api/align-process-tree-workbench", preflight);
  server.Options("/api/align-petri-anti-multi-workbench", preflight);
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
  server.Options("/api/streaming-declare", preflight);
  for(const auto* route:{"create","snapshot","append","action","list","source","release"})server.Options(std::string("/api/live-stream/")+route,preflight);
  for(const auto* route:{"create","snapshot","read","action","list","source","export","release"})server.Options(std::string("/api/stream-reader/")+route,preflight);
  auto source_preflight=[&](const httplib::Request& request,httplib::Response& response){preflight(request,response);response.set_header("Access-Control-Allow-Methods","GET, POST, OPTIONS");response.set_header("Access-Control-Allow-Headers","Content-Type, Authorization, X-Minera-Database-Connection");};
  for(const auto* route:{"acquire","snapshot","original","capture","table","export","list","release"})server.Options(std::string("/api/source-acquisition/")+route,source_preflight);
  server.Options("/api/knowledge-injection", preflight);

  api_post(server, "/api/mine", [](const httplib::Request& request,httplib::Response& response){
    // One mine at a time; the slot is freed once the response is prepared.
    MineSlot slot;
    if (!slot.held()) {answer_mine_busy(response); return;}
    t_published_mining_ticket.reset();
    exact_filter_discovery_route::integrated(request,response,"mine",handle_mine);
    deliver_published_mining(request, response);
  });
  api_post(server, "/api/cancel-mine", handle_cancel_mine);
  api_get(server, "/api/mine-progress", handle_mine_progress);
  api_post(server, "/api/mine-ack", handle_mine_ack);
  api_get(server, "/api/active-source", handle_active_source);
  api_get(server, "/api/active-source-file", handle_active_source_file);
  api_get(server, "/api/traces", handle_traces);
  api_post(server, "/api/ocel/discover", handle_ocel_discovery);
  api_post(server, "/api/ocel/split", handle_ocel_split);
  api_post(server, "/api/ocel/enrich", handle_ocel_enrichment);
  api_post(server, "/api/ocel/graph", handle_ocel_graph);
  api_post(server, "/api/ocel/event-object-graph", handle_ocel_event_object_graph);
  api_post(server, "/api/ocel/flatten", handle_ocel_flatten);
  api_post(server, "/api/ocel/playout", handle_ocel_playout);
  api_post(server, "/api/ocel/features", handle_ocel_features);
  api_post(server, "/api/ocel/statistics", handle_ocel_statistics);
  api_post(server, "/api/ocel/description", handle_ocel_description);
  api_post(server, "/api/ocel/filter", handle_ocel_filter);
  api_post(server, "/api/ocel/sqlite", handle_ocel_sqlite);
  api_post(server, "/api/ocel/io", handle_ocel_exchange);
  api_post(server, "/api/ocel/conformance", [](const httplib::Request& request, httplib::Response& response) {
    if(request.has_param("otg_theta_by_relation_json")||request.has_file("otg_theta_by_relation_json"))handle_exact_otg_conformance(request,response);
    else handle_ocel_conformance(request,response);
  });
  api_post(server, "/api/ilp-mine", handle_ilp_mine);
  api_post(server, "/api/mine-ilp", handle_ilp_mine);
  api_post(server, "/api/genetic-mine", handle_genetic_mine);
  api_post(server, "/api/mine-genetic", handle_genetic_mine);
  api_post(server, "/api/genetic-matrix-convert", handle_genetic_matrix_conversion);
  api_post(server, "/api/log-convert", handle_log_conversion);
  api_post(server, "/api/organizational-sna", handle_organizational_sna);
  api_post(server, "/api/sna", handle_organizational_sna);
  api_post(server, "/api/network-analysis", handle_network_analysis);
  api_post(server, "/api/link-analysis", handle_link_analysis);
  api_post(server, "/api/roles", handle_roles);
  api_post(server, "/api/resource-profiles", handle_resource_profiles);
  api_post(server, "/api/knowledge-injection", [](const httplib::Request& request,httplib::Response& response){
    for(const auto& [name,value]:request.params){(void)value;if(minera::knowledge_exact_control(name)){handle_knowledge_exact(request,response);return;}}
    for(const auto& [name,value]:request.files){(void)value;if(minera::knowledge_exact_control(name)){handle_knowledge_exact(request,response);return;}}
    handle_knowledge_injection(request,response);
  });
  api_get(server, "/api/llm/status", handle_llm_status);
  api_post(server, "/api/llm/complete", handle_llm_complete);
  api_get(server, "/api/log-query/schema",handle_log_query_schema);
  api_post(server, "/api/log-query",handle_log_query);
  api_post(server, "/api/log-query/export",handle_log_query_export);
  api_post(server, "/api/log-cluster",handle_log_cluster);
  api_post(server, "/api/llm-context",[](const httplib::Request& request,httplib::Response& response){
    if(request_uses_supplied_case_context(request))handle_supplied_case_context(request,response);
    else if(request_uses_original_model_context(request))handle_original_model_context(request,response);
    else handle_llm_context(request,response);
  });
  api_post(server, "/api/local-diagnostics", handle_local_diagnostics);
  api_post(server, "/api/clustering", handle_clustering);
  api_post(server, "/api/profile-clustering", handle_profile_clustering);
  api_post(server, "/api/clustering/export", handle_clustering_export);
  api_post(server, "/api/chaotic-activities", handle_chaotic_activities);
  api_post(server, "/api/passed-time", handle_passed_time);
  api_post(server, "/api/service-time", handle_service_time);
  api_post(server, "/api/rework", handle_rework);
  api_post(server, "/api/cycle-time", handle_cycle_time);
  api_post(server, "/api/case-arrival", handle_case_arrival);
  api_post(server, "/api/case-statistics", handle_case_statistics);
  api_post(server, "/api/attribute-statistics", handle_attribute_statistics);
  api_post(server, "/api/attribute-distribution", handle_attribute_distribution);
  api_post(server, "/api/attribute-kde", handle_attribute_kde);
  api_post(server, "/api/boundary-activities", handle_boundary_activities);
  api_post(server, "/api/variant-statistics", handle_variant_statistics);
  api_post(server, "/api/trace-variant-query", handle_trace_variant_query);
  api_post(server, "/api/anonymize-trace-variants", handle_trace_variant_query);
  api_post(server, "/api/pripel", handle_pripel);
  api_post(server, "/api/anonymize-pripel", handle_pripel);
  api_post(server, "/api/concept-drift", handle_concept_drift);
  api_post(server, "/api/browser-history", handle_browser_history);
  api_post(server, "/api/windows-events", handle_windows_events);
  api_post(server, "/api/repository-history", handle_repository_history);
  api_post(server, "/api/workflow-history", handle_workflow_history);
  api_post(server, "/api/enterprise-history", handle_enterprise_history);
  api_post(server, "/api/communication-history", handle_communication_history);
  api_post(server, "/api/transform-target", handle_log_to_target);
  api_post(server, "/api/log-to-features", handle_log_to_features);
  api_post(server, "/api/features", handle_log_to_features);
  api_post(server, "/api/transform-features", handle_log_to_features);
  api_post(server, "/api/log-util", handle_log_util);
  api_post(server, "/api/embeddings", handle_embeddings);
  api_post(server, "/api/to-embeddings", handle_embeddings);
  api_post(server, "/api/label-splitting", handle_label_splitting);
  api_post(server, "/api/transform-label-splitting", handle_label_splitting);
  api_get(server, "/api/trie", handle_trie);
  api_get(server, "/api/trie-to-petri", handle_trie_to_petri);
  api_get(server, "/api/interval-tree", handle_interval_tree);
  api_post(server, "/api/merge-case-relations", handle_case_relations);
  api_post(server, "/api/format-dataframe", handle_format_dataframe);
  api_post(server, "/api/timestamp-interleavings", handle_timestamp_interleavings);
  api_get(server, "/api/random-tree", handle_random_tree);
  api_post(server, "/api/generate-trees", handle_tree_generation);
  api_post(server, "/api/compare-petri", handle_compare_petri);
  api_post(server, "/api/align", handle_align);
  api_post(server, "/api/evaluate", handle_evaluate);
  api_post(server, "/api/petri-analysis", handle_petri_analysis);
  api_post(server, "/api/evaluate-emd", handle_evaluate_emd);
  api_post(server, "/api/align-process-tree", handle_align_process_tree);
  api_post(server, "/api/align-decomposed", handle_align_decomposed);
  api_post(server, "/api/align-petri-anti", handle_align_petri_anti);
  api_post(server, "/api/align-petri-multi", handle_align_petri_multi);
  api_post(server, "/api/reduce-process-tree", handle_reduce_process_tree);
  api_post(server, "/api/reduce-petri-net", handle_reduce_petri_net);
  api_post(server, "/api/align-dfg", handle_align_dfg);
  api_post(server, "/api/discover-alpha-plus", handle_alpha_plus_result);
  api_post(server, "/api/discover-workflow", [](const httplib::Request& request,httplib::Response& response){exact_filter_discovery_route::integrated(request,response,"discover_workflow",handle_workflow_discovery);});
  api_post(server, "/api/discover-workflow-input", [](const httplib::Request& request,httplib::Response& response){exact_filter_discovery_route::integrated(request,response,"discover_workflow_input",handle_workflow_input_discovery);});
  api_post(server, "/api/discover-inductive-input", handle_inductive_input_discovery);
  api_post(server, "/api/align-process-tree-workbench", handle_process_tree_alignment_workbench);
  api_post(server, "/api/align-petri-anti-multi-workbench", handle_petri_anti_multi_workbench);
  api_post(server, "/api/model-conversion-workbench", [](const httplib::Request& request, httplib::Response& response) {
    if (request_uses_tree_reduction(request)) handle_tree_reduction_workbench(request, response);
    else handle_model_conversion_workbench(request, response);
  });
  api_post(server, "/api/monte-carlo-workbench", handle_monte_carlo_workbench);
  api_post(server, "/api/playout-workbench", handle_playout_workbench);
  api_post(server, "/api/decision-workbench", handle_original_decision_workbench);
  api_post(server, "/api/declare-workbench", handle_original_declare_workbench);
  api_post(server, "/api/log-skeleton-workbench", handle_original_log_skeleton_workbench);
  api_post(server, "/api/footprints-workbench", handle_original_footprints_workbench);
  api_post(server, "/api/genetic-matrix-workbench", handle_original_genetic_matrix_workbench);
  api_post(server, "/api/embedding-workbench", exact_filter_discovery_route::embedding);
  api_post(server, "/api/original-log-to-features", handle_original_features_workbench);
  api_post(server, "/api/original-label-splitting-workbench", handle_original_label_splitting_workbench);
  api_post(server, "/api/original-log-query-workbench", handle_original_log_query_workbench);
  api_post(server, "/api/reference-log-filter", handle_reference_log_filter);
  api_post(server, "/api/reference-table-repetition", handle_table_attribute_repetition);
  api_post(server, "/api/reference-table-between", handle_table_between_filter);
  api_post(server, "/api/reference-table-cases", handle_table_case_filter);
  api_post(server, "/api/reference-table-paths", handle_table_paths_filter);
  api_post(server, "/api/reference-table-timestamp", handle_table_timestamp_filter);
  api_post(server, "/api/reference-table-variants", handle_table_variants_filter);
  api_post(server, "/api/reference-table-attributes", handle_table_attributes_filter);
  api_post(server, "/api/reference-log-attributes", handle_log_attributes_filter);
  api_post(server, "/api/reference-log-variants", handle_log_variants_filter);
  api_post(server, "/api/reference-log-timestamp", handle_log_timestamp_filter);
  api_post(server, "/api/reference-ltl-filter", handle_reference_ltl_filter);
  server.Options("/api/emd-workbench", preflight);
  api_post(server, "/api/emd-workbench", handle_original_emd_workbench);
  server.Options("/api/object-centric-playout-workbench", preflight);
  api_post(server, "/api/object-centric-playout-workbench", handle_original_object_centric_playout_workbench);
  api_post(server, "/api/discover-constraints", handle_constraint_discovery);
  api_post(server, "/api/discover-graph", handle_graph_discovery);
  api_post(server, "/api/discover-timing", handle_timing_discovery);
  api_post(server, "/api/analyze-behavior", handle_behavior_analysis);
  api_post(server, "/api/discover-dfg", handle_dfg_discovery);
  api_post(server, "/api/evaluate-replay-results", handle_precomputed_replay_fitness);
  server.Options("/api/edit-distance-workbench", preflight);
  api_post(server, "/api/edit-distance-workbench", handle_original_edit_distance_workbench);
  api_post(server, "/api/bose-workbench", handle_original_bose_workbench);
  server.Options("/api/discovery-workbench", preflight);
  api_post(server, "/api/discovery-workbench", handle_original_discovery_workbench);
  server.Options("/api/petri-analysis-workbench", preflight);
  api_post(server, "/api/petri-analysis-workbench", handle_original_petri_analysis_workbench);
  server.Options("/api/root-cause-workbench", preflight);
  api_post(server, "/api/root-cause-workbench", handle_original_root_cause_workbench);
  server.Options("/api/anonymization-workbench", preflight);
  api_post(server, "/api/anonymization-workbench", handle_original_anonymization_workbench);
  server.Options("/api/quality-workbench", preflight);
  api_post(server, "/api/quality-workbench", handle_original_quality_workbench);
  server.Options("/api/clustering-workbench", preflight);
  api_post(server, "/api/clustering-workbench", handle_original_clustering_workbench);
  server.Options("/api/organizational-workbench", preflight);
  api_post(server, "/api/organizational-workbench", handle_original_organizational_workbench);
  server.Options("/api/temporal-workbench", preflight);
  api_post(server, "/api/temporal-workbench", handle_original_temporal_workbench);
  api_post(server, "/api/timing-workbench", handle_original_timing_workbench);
  api_post(server, "/api/time-analysis-workbench", handle_original_time_analysis_workbench);
  api_post(server, "/api/structural-inputs", handle_original_structural_inputs);
  api_post(server, "/api/align-edit-distance", handle_align_edit_distance);
  api_post(server, "/api/decide", handle_decide);
  api_post(server, "/api/decision-tables", handle_decision_tables);
  api_post(server, "/api/data-petri-net", handle_data_petri_net);
  api_post(server, "/api/check-model", handle_check_model);
  api_post(server, "/api/token-replay", handle_token_replay);
  api_post(server, "/api/replay-token-workbench", handle_token_replay_workbench);
  api_post(server, "/api/analyze-replay-duration-workbench", handle_replay_duration_workbench);
  api_post(server, "/api/align-petri-workbench", handle_petri_alignment_workbench);
  api_post(server, "/api/align-graph-workbench", [](const httplib::Request& request,httplib::Response& response) {
    const auto operation=request.has_file("operation")?form_field(request,"operation"):request.has_param("operation")?request.get_param_value("operation"):std::string{};
    if(operation=="project_saved_dfg")handle_saved_dfg_projection_workbench(request,response);
    else handle_graph_alignment_workbench(request,response);
  });
  api_post(server, "/api/replay-fitness", handle_replay_fitness);
  api_get(server, "/api/pnml", handle_pnml);
  api_get(server, "/api/process-tree-petri", handle_process_tree_petri);
  api_get(server, "/api/bpmn", handle_bpmn);
  api_get(server, "/api/ptml", handle_ptml);
  api_get(server, "/api/xes", handle_xes);
  api_get(server, "/api/dfg", handle_dfg);
  api_get(server, "/api/dfg-dataframe", handle_dfg_dataframe);
  api_post(server, "/api/dfg-dataframe", handle_dfg_dataframe);
  api_post(server, "/api/dfg", handle_dfg_import);
  api_post(server, "/api/model-exchange", handle_model_exchange);
  api_post(server, "/api/dfg-precision", handle_dfg_precision);
  api_post(server, "/api/dfg-to-petri", handle_dfg_conversion);
  api_post(server, "/api/process-cube", handle_process_cube);
  api_post(server, "/api/streaming-dfg", handle_streaming_dfg);
  api_post(server, "/api/streaming-footprints", handle_streaming_footprints);
  api_post(server, "/api/streaming-temporal", handle_streaming_temporal);
  api_post(server, "/api/temporal-conformance", handle_temporal_conformance);
  api_post(server, "/api/streaming-tbr", handle_streaming_tbr);
  api_post(server, "/api/streaming-declare", handle_streaming_declare);
  api_post(server, "/api/live-stream/create",handle_live_create);
  api_post(server, "/api/stream-reader/create",handle_reader_create);
  api_post(server, "/api/source-acquisition/acquire",handle_acquire_source);
  api_get(server, "/api/source-acquisition/snapshot",handle_acquired_snapshot);
  api_get(server, "/api/source-acquisition/original",handle_acquired_original);
  api_get(server, "/api/source-acquisition/capture",handle_acquired_capture);
  api_get(server, "/api/source-acquisition/table",handle_acquired_database_table);
  api_get(server, "/api/source-acquisition/export",handle_acquired_export);
  api_post(server, "/api/source-acquisition/export",handle_acquired_export);
  api_get(server, "/api/source-acquisition/list",handle_acquired_list);
  api_post(server, "/api/source-acquisition/release",handle_acquired_release);
  api_get(server, "/api/stream-reader/snapshot",handle_reader_snapshot);
  api_post(server, "/api/stream-reader/read",handle_reader_read);
  api_post(server, "/api/stream-reader/action",handle_reader_action);
  api_get(server, "/api/stream-reader/list",handle_reader_list);
  api_get(server, "/api/stream-reader/source",handle_reader_source);
  api_get(server, "/api/stream-reader/export",handle_reader_export);
  api_post(server, "/api/stream-reader/release",handle_reader_release);
  api_get(server, "/api/live-stream/snapshot",handle_live_snapshot);
  api_post(server, "/api/live-stream/append",handle_live_append);
  api_post(server, "/api/live-stream/action",handle_live_action);
  api_get(server, "/api/live-stream/list",handle_live_list);
  api_get(server, "/api/live-stream/source",handle_live_source);
  api_post(server, "/api/live-stream/release",handle_live_release);
  api_get(server, "/api/playout", handle_playout);
  api_post(server, "/api/playout", handle_playout);
  api_get(server, "/api/stochastic-petri", handle_stochastic_petri);
  api_get(server, "/api/stochastic-map", handle_stochastic_map);
  api_get(server, "/api/monte-carlo", handle_monte_carlo_json);
  api_post(server, "/api/monte-carlo", handle_monte_carlo_json);

  api_get(server, "/api/version", [](const httplib::Request&, httplib::Response& response) {
    std::ostringstream out;
    out << "{\"generator\": \"minera " << minera::version_major << '.'
        << minera::version_minor << '.' << minera::version_patch << "\"}";
    response.set_content(out.str(), "application/json");
  });

  const bool has_frontend = std::filesystem::is_directory(frontend_dir);
  if (has_frontend) {
    server.set_mount_point("/", frontend_dir.string());
  }

  // The port belongs to this server alone (the vendored httplib binds without
  // SO_REUSEPORT): a second server on a busy port stops here instead of
  // sharing its connections with the first.
  if (!server.bind_to_port("127.0.0.1", port)) {
    if (loopback_port_in_use(port)) {
      std::cerr << "error: port " << port
                << " is already in use; stop the other server or pass --port\n";
#ifdef _WIN32
      // SO_EXCLUSIVEADDRUSE also keeps the port while a just-stopped
      // server's accepted connections are still closing (TIME_WAIT).
      std::cerr << "note: on Windows a server that has just stopped can hold the port"
                   " for up to a few minutes while its connections close; retry"
                   " shortly or pass another --port\n";
#endif
    } else
      std::cerr << "error: cannot listen on port " << port << '\n';
    return 1;
  }
  std::cout << "minera " << minera::version_major << '.' << minera::version_minor
            << '.' << minera::version_patch << " - listening on http://localhost:"
            << port << '\n';
  if (!has_frontend) {
    std::cout << "note: frontend directory not found (" << frontend_dir.string()
              << "); serving the API only\n";
  }

  // SIGTERM/SIGINT remove in-flight temporary uploads, then end the process
  // as the default action would.
  minera::secure_temp::install_signal_cleanup();
  // Frees live and reader sessions nobody has used for the idle expiry.
  const StreamIdleReaper stream_idle_reaper;
  if (!server.listen_after_bind()) {
    std::cerr << "error: cannot listen on port " << port << '\n';
    return 1;
  }
  return 0;
}
