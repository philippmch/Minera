#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

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

}  // namespace minera
