#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "minera/alpha_miner.hpp"
#include "minera/conformance.hpp"
#include "minera/correlation_miner.hpp"
#include "minera/declare.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/dfg_variants.hpp"
#include "minera/footprints.hpp"
#include "minera/heuristic_miner.hpp"
#include "minera/inductive_miner.hpp"
#include "minera/log_filter.hpp"
#include "minera/log_skeleton.hpp"
#include "minera/powl_discovery.hpp"
#include "minera/transition_system.hpp"
#include "minera/performance_spectrum.hpp"
#include "minera/batches.hpp"

// Pure request-parsing helpers shared by the local server (tools/minera_server)
// and its tests. Nothing here depends on the HTTP layer: form fields arrive
// through a plain string-keyed lookup, so the parsing can be exercised directly.
namespace minera {

// Case-insensitive suffix match (the suffix is assumed already lowercase).
[[nodiscard]] bool ends_with_ci(std::string_view value, std::string_view suffix);

// Decides whether an upload is XES rather than CSV: a ".xes" name, or content
// that begins (after an optional UTF-8 BOM and whitespace) with "<?xml" or
// "<log" so extension-less uploads still route to the XES importer.
[[nodiscard]] bool looks_like_xes(std::string_view filename,
                                  std::string_view content);

// Splits a comma-separated field into trimmed, non-empty parts. Spaces
// surrounding each part are dropped (so "a , b" yields "a","b"); empty parts
// (e.g. from "a,,b" or a trailing comma) are skipped entirely.
[[nodiscard]] std::vector<std::string> split_csv(std::string_view value);

// Serializes a message as a one-key JSON error object, escaping quotes and
// backslashes so the message can never break the JSON.
[[nodiscard]] std::string error_json(std::string_view message);

// The discovery options parsed from a mine request's form fields.
struct MineParams {
  CsvImportOptions csv;
  LogFilterOptions filter;
  InductiveMinerOptions inductive;
  AlphaMinerOptions alpha;
  HeuristicMinerOptions heuristic;
  PowlDiscoveryOptions powl;
  TransitionSystemOptions transition_system;
  TokenReplayOptions token_replay;
  LogSkeletonOptions log_skeleton;
  DeclareDiscoveryOptions declare_options;
  FootprintsOptions footprints;
  // Empty vectors keep these analyses opt-in. String ids intentionally retain
  // PM4Py aliases that map to the same native enum value.
  std::vector<std::string> dfg_statistical_variants;
  std::vector<std::string> dfg_causal_variants;
  DfgStatisticalOptions dfg_statistics;
  // Correlation mining is strictly opt-in; an empty list preserves the
  // historical request behaviour and avoids requiring timestamps by default.
  std::vector<std::string> correlation_variants;
  CorrelationMinerOptions correlation;
  // Opt-in temporal/behavioral discovery options.
  std::vector<std::string> spectrum_activities;
  PerformanceSpectrumOptions spectrum;
  bool batches = false;
  BatchDiscoveryOptions batch_options;
};

// A lookup returning a form field's value, or "" when the field is absent.
using FieldLookup = std::function<std::string(std::string_view)>;
// Optional companion lookup that distinguishes an omitted field from a
// present field whose value is empty. This matters for options where an
// explicit empty list has different semantics from using the default.
using FieldPresence = std::function<bool(std::string_view)>;

// Parses alignment search settings shared by the HTTP server, CLI and tests.
// Fields use the wire names documented by /api/align (alignment_variant,
// max_states, max_align_time, ...).  Invalid or dependent settings throw
// std::invalid_argument rather than silently falling back to defaults.
[[nodiscard]] AlignmentOptions parse_alignment_params(const FieldLookup& field);
[[nodiscard]] AlignmentOptions parse_alignment_params(const FieldLookup& field,
                                                       const FieldPresence& present);

// Builds the discovery options from a mine request's form fields. The
// single-callback overload treats absent and empty fields alike, preserving the
// original defaults. With FieldPresence, explicitly present empty Declare list
// fields become engaged empty filters, while footprint and DFG variant lists
// can reject an explicit empty list; omitted fields retain their defaults. A
// `time_mode` value that is not one of the three known modes throws
// std::invalid_argument, so the caller can report it instead of silently mining
// with the wrong filter.
[[nodiscard]] MineParams parse_mine_params(const FieldLookup& field);
[[nodiscard]] MineParams parse_mine_params(const FieldLookup& field,
                                           const FieldPresence& present);

}  // namespace minera
