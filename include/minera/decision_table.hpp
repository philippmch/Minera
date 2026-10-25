#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "minera/conformance.hpp"
#include "minera/event_log.hpp"
#include "minera/decision_mining.hpp"
#include "minera/petri_net.hpp"

namespace minera {

// Bound retained history so a malformed request cannot multiply rows without
// limit.  The value is deliberately shared by native and adapter validation.
inline constexpr std::size_t kMaxDecisionLookback = 64;

// Controls the deterministic table extracted from the observations that are
// available immediately before a model choice.
struct DecisionTableOptions {
  // The same tree controls are used when the table also carries decision
  // diagnostics, keeping on-demand table and decision requests consistent.
  DecisionMiningOptions mining;
  // The reference table uses event attributes by default. Case attributes can
  // be included explicitly without changing the source log.
  bool include_trace_attributes = false;
  // Number of preceding visible-event context snapshots retained before each
  // choice. k=1 keeps the latest context and is the default used by guarded
  // models; an empty feature map still occupies its event snapshot slot.
  std::size_t lookback = 1;
  // Empty means all attributes observed in the decision contexts. When set,
  // only these event/context keys are encoded, in lexical order.
  std::vector<std::string> attributes;
};

struct DecisionTableRow {
  // Numeric columns contain a value or null for a missing attribute. One-hot
  // columns always contain 0 or 1.
  std::vector<std::optional<double>> features;
  std::size_t target = 0;
};

struct EncodedDecisionTable {
  std::size_t place = 0;
  std::vector<std::string> classes;
  std::vector<std::string> feature_names;
  std::vector<DecisionTableRow> rows;
};

struct DecisionTablesResult {
  std::uint64_t observed_traces = 0;
  std::uint64_t replayed_traces = 0;
  std::uint64_t aligned_traces = 0;
  std::uint64_t alignment_failures = 0;
  std::uint64_t trace_count = 0;
  std::vector<EncodedDecisionTable> tables;
  // Field used to form replay variants, retained for export provenance.
  std::string activity_attribute = "concept:name";
};

[[nodiscard]] DecisionTablesResult mine_decision_tables(
    const EventLog& log, const PetriNet& net,
    const DecisionTableOptions& options = {});

[[nodiscard]] inline DecisionTablesResult extract_decision_tables(
    const EventLog& log, const PetriNet& net,
    const DecisionTableOptions& options = {}) {
  return mine_decision_tables(log, net, options);
}

[[nodiscard]] std::string serialize_decision_tables_json(
    const DecisionTablesResult& result);

// Column storage of the reference-shaped table: one-hot string columns are
// bool, numeric columns int64 when every row carries an integer, else float64.
enum class ReferenceDecisionColumnType { kBool, kInt64, kFloat64 };

// The public reference encoding of one decision point (the X, y and classes
// of algo.decision_mining.apply). Token replay supplies each trace's fired
// transitions; every fired transition whose label (null for silent ones)
// occurs among the labels of the place's outgoing transitions yields one row
// carrying the attributes of the last visible event replayed before it, and
// firings before the first event yield none. Traces are visited per activity
// variant in first-appearance order, then in log order. Only event attributes
// present in every trace are encoded (string attributes one-hot per observed
// value, then numeric ones; booleans, dates, concept:name, time:timestamp and
// lifecycle:transition are skipped), each group in lexical key order.
struct ReferenceDecisionTable {
  std::vector<std::string> columns;
  std::vector<ReferenceDecisionColumnType> column_types;
  // One-hot cells are 0 or 1; numeric cells are null when the event lacks
  // the attribute.
  std::vector<std::vector<std::optional<double>>> rows;
  std::vector<std::size_t> targets;  // indices into classes
  // Fired labels in first-appearance order; null for silent transitions.
  std::vector<std::optional<std::string>> classes;
};

// Throws std::invalid_argument when the point yields no rows or no string
// column, where the reference raises ValueError.
[[nodiscard]] ReferenceDecisionTable reference_decision_table(
    const EventLog& log, const PetriNet& net, std::size_t place,
    const TokenReplayOptions& replay = {});

}  // namespace minera
