#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "minera/dfg_variants.hpp"
#include "minera/event_data.hpp"

namespace minera {

class DirectlyFollowsGraph;
struct CorrelationKeyObjects;  // correlation_key_objects.hpp (internal)

enum class CorrelationVariant {
  kClassic,
  kClassicSplit,
  kTraceBased,
  CLASSIC = kClassic,
  CLASSIC_SPLIT = kClassicSplit,
  TRACE_BASED = kTraceBased,
};

struct CorrelationMinerOptions {
  CorrelationVariant variant = CorrelationVariant::kClassic;
  // Selectors for the fields used by the three matrix builders.  Canonical
  // aliases (concept:name/activity, time:timestamp/timestamp, and
  // case:concept:name/case_id/case:id/case) use first-class projections when
  // those projections carry a custom source spelling.
  std::string activity_attribute = "concept:name";
  std::string timestamp_attribute = "time:timestamp";
  std::string start_timestamp_attribute;
  std::string case_id_attribute = "case:concept:name";
  // The completion key pm4py is given on the same input (an upload's
  // formatted frame reads time:timestamp where `timestamp_attribute` names
  // its own column). When set, the trace-based variant models pm4py's
  // projected log, which keeps only the standard keys (round FS). Empty (the
  // default) keeps the native contract: the selectors are read as given.
  std::string reference_timestamp_key;
  bool exact_time_matching = false;
  std::size_t sample_size = 100000;
  // Round HI: the activity key's Python objects, one per input event, where
  // pm4py's classic variants read a key holding other values than text
  // (an XES upload's legacy objects, a CSV frame's typed or gapped cells).
  // The classic variants then group, order and name the activities as
  // pm4py's sorted set of those objects does; null keeps the text contract.
  // The trace-based variant never reads it (it reads concept:name).
  std::shared_ptr<const CorrelationKeyObjects> key_objects;
};

using CorrelationPerformanceDfg = std::map<DfgActivityPair, double>;

struct CorrelationMinerResult {
  CorrelationVariant variant = CorrelationVariant::kClassic;
  // Observed event counts, independent of the balanced transportation
  // solution. Every activity in the selected input appears exactly once.
  std::map<std::string, std::uint64_t> activity_counts;
  DfgFrequency frequency_dfg;
  CorrelationPerformanceDfg performance_dfg;
  // Directed precede/succeed probability matrix, keyed by activity pair.
  std::map<DfgActivityPair, double> precede_succeed_probability;
  std::map<DfgActivityPair, double> duration_matrix;
};

struct NamedCorrelationMinerResult {
  std::string name;
  CorrelationMinerResult analysis;
};
struct CorrelationVariantsAnalysis {
  std::vector<NamedCorrelationMinerResult> variants;
};

// Makes every projected correlation label addressable by shared JSON models
// without altering the native directly-follows counts.
void register_correlation_activities(
    DirectlyFollowsGraph& graph,
    const CorrelationVariantsAnalysis& analyses);

[[nodiscard]] CorrelationMinerResult discover_correlation_miner(
    const EventLog& log, const CorrelationMinerOptions& options = {});
[[nodiscard]] CorrelationMinerResult discover_correlation_miner(
    const EventStream& stream, const CorrelationMinerOptions& options = {});
[[nodiscard]] CorrelationMinerResult discover_correlation_miner(
    const EventTable& table, const CorrelationMinerOptions& options = {});
[[nodiscard]] CorrelationMinerResult discover_correlation_miner(
    const LazyEventTable& table, const CorrelationMinerOptions& options = {});
[[nodiscard]] CorrelationMinerResult discover_correlation_miner(
    const EventDataView& data, const CorrelationMinerOptions& options = {});

}  // namespace minera
