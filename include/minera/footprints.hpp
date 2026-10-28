#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "minera/directly_follows_graph.hpp"
#include "minera/event_data.hpp"
#include "minera/event_log.hpp"
#include "minera/petri_net.hpp"
#include "minera/process_tree.hpp"
#include "minera/powl.hpp"

namespace minera {

using FootprintPair = std::pair<std::string, std::string>;

// The footprint dictionary, represented with ordered containers so JSON
// and diagnostics are deterministic. Optional fields are deliberately kept
// optional: the DFG variant, for example, omits dfg and min_trace_length.
struct Footprint {
  std::map<FootprintPair, std::uint64_t> dfg;
  bool dfg_present = true;
  std::set<FootprintPair> sequence;
  std::set<FootprintPair> parallel;
  // Both child orders are allowed, but only one whole child executes at a
  // time. This is distinct from event-level parallelism.
  std::set<FootprintPair> atomic_interleaving;
  std::set<std::string> start_activities;
  std::set<std::string> end_activities;
  bool start_activities_present = true;
  bool end_activities_present = true;
  std::set<std::string> activities;
  std::optional<bool> skippable;
  std::optional<std::set<std::string>> activities_always_happening;
  std::optional<std::size_t> min_trace_length;
  bool min_trace_length_present = true;
  std::optional<std::size_t> max_trace_length_without_loops;
  std::optional<std::vector<std::string>> trace;
  // The Polars lazy adapter keeps a null activity as an activity (and as a
  // start or end activity); it never takes part in a directly-follows pair.
  bool activities_include_null = false;
  bool start_activities_include_null = false;
  bool end_activities_include_null = false;
};

// Selectors shared by the event-log, table, lazy-table, and Petri discovery
// adapters.  The value objects deliberately use attribute names rather than
// importer-specific aliases so a caller can use the same options for every
// input representation.  A missing start selector means that no separate
// start-time column is required; completion timestamps remain the ordering
// key for the eager adapter.
struct FootprintsDiscoveryOptions {
  std::string activity_attribute = "concept:name";
  std::string case_id_attribute = "case:concept:name";
  std::string timestamp_attribute = "time:timestamp";
  std::optional<std::string> start_timestamp_attribute;
  std::string index_attribute = "@@index";
  bool sort_required = true;
  // When engaged, this must contain one token count for every Petri place.
  // An omitted marking uses the workflow source place's initial token.
  std::optional<std::vector<std::uint64_t>> initial_marking;
};

Footprint discover_footprints(const EventLog& log);
Footprint discover_footprints(
    const EventLog& log, const FootprintsDiscoveryOptions& options);
std::vector<Footprint> discover_trace_footprints(const EventLog& log);
std::vector<Footprint> discover_trace_footprints(
    const EventLog& log, const FootprintsDiscoveryOptions& options);
Footprint discover_footprints(
    const EventTable& table,
    const FootprintsDiscoveryOptions& options = {});
Footprint discover_footprints(
    const LazyEventTable& table,
    const FootprintsDiscoveryOptions& options = {});

// Build the selector projection used by the aggregate adapters.  The
// projection keeps the imported trace/event order and materializes selected
// first-class values under the requested table column names only when a real
// source column with that name is absent.
EventLog prepare_footprints_event_log(
    const EventLog& log, const FootprintsDiscoveryOptions& options = {});
EventTable prepare_footprints_event_table(
    const EventLog& log, const FootprintsDiscoveryOptions& options = {});
LazyEventTable prepare_footprints_lazy_table(
    const EventLog& log, const FootprintsDiscoveryOptions& options = {});

// Resolve the case identifiers that trace diagnostics should expose.  A
// custom selector must resolve to a present, non-null value for every event
// in a trace; canonical selectors retain the trace's intrinsic identifier.
std::vector<std::string> resolve_footprints_case_ids(
    const EventLog& log, std::string_view case_id_attribute);
Footprint discover_dfg_footprints(
    const std::map<FootprintPair, std::uint64_t>& dfg);
Footprint discover_dfg_footprints(const DirectlyFollowsGraph& graph);

// Model-discovery variants from the footprints algorithm. These return
// only the fields defined by the corresponding variant; absent fields are
// signalled by the *_present flags (or a disengaged optional).
Footprint discover_footprints(const PetriNet& net);
Footprint discover_footprints(
    const PetriNet& net, const FootprintsDiscoveryOptions& options);
Footprint discover_footprints(const ProcessTree& tree);
Footprint discover_footprints(const PowlNode& model);

struct FootprintsLogModelOptions {
  bool strict = false;
  std::string case_id_attribute = "case:concept:name";
};

using FootprintViolations = std::set<FootprintPair>;

FootprintViolations check_footprints_log_model(
    const Footprint& log,
    const Footprint& model,
    FootprintsLogModelOptions options = {});
std::vector<FootprintViolations> check_footprints_log_model(
    const std::vector<Footprint>& log,
    const Footprint& model,
    FootprintsLogModelOptions options = {});

struct FootprintsExtensiveResult {
  std::set<FootprintPair> footprints;
  std::set<std::string> start_activities;
  std::set<std::string> end_activities;
  std::set<std::string> activities_always_happening;
  bool min_length_fit = true;
  bool is_footprints_fit = true;
  bool is_fit = true;
};

FootprintsExtensiveResult check_footprints_log_extensive(
    const Footprint& log, const Footprint& model);

struct FootprintsTraceExtensiveOptions {
  bool enable_act_always_executed = true;
  std::string case_id_attribute = "case:concept:name";
};

std::vector<FootprintsExtensiveResult> check_footprints_trace_extensive(
    const std::vector<Footprint>& log,
    const Footprint& model,
    FootprintsTraceExtensiveOptions options = {});

struct FootprintsFitnessSummary {
  double perc_fit_traces = 0.0;
  double log_fitness = 1.0;
};

double fp_precision(const Footprint& log, const Footprint& model);
double fp_fitness(const Footprint& log,
                  const Footprint& model,
                  const FootprintsExtensiveResult& conformance);
FootprintsFitnessSummary fp_fitness(
    const std::vector<Footprint>& log,
    const Footprint& model,
    const std::vector<FootprintsExtensiveResult>& conformance);

// End-to-end selection used by request, CLI, server, and JSON integration.
// Ordered sets make parsing and serialization deterministic.
struct FootprintsOptions {
  std::set<std::string> discovery_variants = {
      "dfg", "entire_event_log", "powl", "process_tree"};
  std::string model = "process_tree";
  std::set<std::string> conformance_variants = {"log_model"};
  bool strict = false;
  bool enable_act_always_executed = true;
  // Event/table selectors used by the selected footprint discovery adapters.
  std::string activity_attribute = "concept:name";
  std::string case_id_attribute = "case:concept:name";
  std::string timestamp_attribute = "time:timestamp";
  std::optional<std::string> start_timestamp_attribute;
  std::string index_attribute = "@@index";
  bool sort_required = true;
  std::optional<std::vector<std::uint64_t>> initial_marking;
};

// Aggregate transport object. Discovery fields are engaged only when their
// corresponding variant was requested; conformance fields follow the same
// rule. `model_footprint` is retained for validation and metric provenance.
struct FootprintsAnalysis {
  std::optional<Footprint> entire_event_log;
  std::optional<Footprint> entire_dataframe;
  std::optional<Footprint> polars_lazyframes;
  std::optional<std::vector<Footprint>> trace_by_trace;
  std::optional<Footprint> dfg;
  std::optional<Footprint> process_tree;
  std::optional<Footprint> powl;
  std::optional<Footprint> petri_reach_graph;

  std::string model = "process_tree";
  bool strict = false;
  Footprint model_footprint;
  std::optional<FootprintViolations> log_model;
  std::optional<FootprintsExtensiveResult> log_extensive;
  std::optional<std::vector<FootprintsExtensiveResult>> trace_extensive;

  double precision = 1.0;
  double log_fitness = 1.0;
  double perc_fit_traces = 0.0;

  // Selector provenance for aggregate diagnostics.  The case-id vector is
  // aligned with the imported trace order and is used by JSON serializers
  // instead of silently falling back to intrinsic trace ids.
  std::string activity_attribute = "concept:name";
  std::string case_id_attribute = "case:concept:name";
  std::vector<std::string> trace_case_ids;
};

}  // namespace minera
