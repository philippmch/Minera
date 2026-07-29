#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "minera/directly_follows_graph.hpp"
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
};

Footprint discover_footprints(const EventLog& log);
std::vector<Footprint> discover_trace_footprints(const EventLog& log);
Footprint discover_dfg_footprints(
    const std::map<FootprintPair, std::uint64_t>& dfg);
Footprint discover_dfg_footprints(const DirectlyFollowsGraph& graph);

// Model-discovery variants from the footprints algorithm. These return
// only the fields defined by the corresponding variant; absent fields are
// signalled by the *_present flags (or a disengaged optional).
Footprint discover_footprints(const PetriNet& net);
Footprint discover_footprints(const ProcessTree& tree);
Footprint discover_footprints(const PowlNode& model);

struct FootprintsLogModelOptions {
  bool strict = false;
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
};

// Aggregate transport object. Discovery fields are engaged only when their
// corresponding variant was requested; conformance fields follow the same
// rule. `model_footprint` is retained for validation and metric provenance.
struct FootprintsAnalysis {
  std::optional<Footprint> entire_event_log;
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
};

}  // namespace minera
