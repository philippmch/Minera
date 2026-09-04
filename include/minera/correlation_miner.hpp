#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "minera/dfg_variants.hpp"
#include "minera/event_data.hpp"

namespace minera {

class DirectlyFollowsGraph;

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
  bool exact_time_matching = false;
  std::size_t sample_size = 100000;
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
