#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include "minera/event_log.hpp"
#include "minera/temporal_profile.hpp"

namespace minera {

using DfgActivityPair = std::pair<std::string, std::string>;
using DfgActivityTriple =
    std::tuple<std::string, std::string, std::string>;
using DfgFrequency = std::map<DfgActivityPair, std::uint64_t>;
using DfgTripleFrequency = std::map<DfgActivityTriple, std::uint64_t>;

// Updates only edges already present in `target`; replacement-only edges are
// deliberately ignored. Returning the target reference supports both in-place
// and expression-oriented use without copying a graph.
template <typename Value>
std::map<DfgActivityPair, Value>& replace_dfg_values(
    std::map<DfgActivityPair, Value>& target,
    const std::map<DfgActivityPair, Value>& replacements) {
  for (auto& [edge, value] : target) {
    const auto found = replacements.find(edge);
    if (found != replacements.end()) value = found->second;
  }
  return target;
}

enum class DfgStatisticalVariant {
  kNative,
  kFrequency = kNative,
  kFrequencyGreedy = kNative,
  kFrequencyTriples,
  kPerformance,
  kPerformanceGreedy = kPerformance,
  kCaseAttributes,
  kClean,
  kCleanPolars,
  kCleanTime,

  NATIVE = kNative,
  FREQUENCY = kFrequency,
  FREQUENCY_GREEDY = kFrequencyGreedy,
  FREQ_TRIPLES = kFrequencyTriples,
  PERFORMANCE = kPerformance,
  PERFORMANCE_GREEDY = kPerformanceGreedy,
  CASE_ATTRIBUTES = kCaseAttributes,
  CLEAN = kClean,
  CLEAN_POLARS = kCleanPolars,
  CLEAN_TIME = kCleanTime,
};

enum class DfgPerformanceAggregation {
  kMean,
  kMedian,
  kMinimum,
  kMaximum,
  kStandardDeviation,
  kSum,
  kRawValues,
  kAll,

  MEAN = kMean,
  MEDIAN = kMedian,
  MIN = kMinimum,
  MAX = kMaximum,
  STDEV = kStandardDeviation,
  SUM = kSum,
  RAW_VALUES = kRawValues,
  ALL = kAll,
};

struct DfgPerformanceSummary {
  std::uint64_t count = 0;
  double mean = 0.0;
  double median = 0.0;
  double minimum = 0.0;
  double maximum = 0.0;
  double standard_deviation = 0.0;
  double sum = 0.0;
};

using DfgPerformanceValue =
    std::variant<double, std::vector<double>, DfgPerformanceSummary>;

struct DfgPerformanceStatistics {
  DfgPerformanceSummary summary;
  std::vector<double> raw_values;
  // The selected result: scalar for mean/median/min/max/stdev/sum,
  // the observation vector for raw_values, and the full summary for all.
  DfgPerformanceValue value = 0.0;
};

using DfgPerformance =
    std::map<DfgActivityPair, DfgPerformanceStatistics>;
using DfgAttributeValueCounts = std::map<std::string, std::uint64_t>;
using DfgNamedAttributeCounts =
    std::map<std::string, DfgAttributeValueCounts>;

struct DfgCaseAttributeAnalysis {
  std::map<DfgActivityPair, DfgNamedAttributeCounts> edges;
  std::map<std::string, DfgNamedAttributeCounts> nodes;
};

struct DfgCleanAnalysis {
  DfgFrequency graph;
  std::map<std::string, std::uint64_t> start_activities;
  std::map<std::string, std::uint64_t> end_activities;
};

using DfgCleanTime = std::map<std::string, double>;

struct DfgStatisticalOptions {
  DfgStatisticalVariant variant = DfgStatisticalVariant::kNative;
  // Zero is intentional: every event contributes its own (activity, activity)
  // pair. Positive values connect positions i-window and i.
  std::size_t window = 1;
  bool keep_once_per_case = false;

  // Event attributes used by the graph variants. The standard XES names map
  // to Event::activity and Event::timestamp; any other name is read from the
  // event's generic attribute list.
  std::string activity_attribute = "concept:name";
  std::string timestamp_attribute = "time:timestamp";
  std::string start_timestamp_attribute = "start_timestamp";
  DfgPerformanceAggregation performance_aggregation =
      DfgPerformanceAggregation::kMean;
  bool use_business_hours = false;
  // Empty selects Monday-Friday 07:00-17:00.
  std::vector<BusinessHourSlot> business_hour_slots;

  // The default is the XES case id, concept:name. Minera also accepts
  // case_id as a spelling for Trace::case_id.
  std::vector<std::string> case_attributes = {"concept:name"};
  bool return_node_attributes = false;
};

// A single transport-friendly result. Exactly the artifact corresponding to
// `variant` is populated (CLEAN also uses frequency_dfg for its graph).
struct DfgStatisticalAnalysis {
  DfgStatisticalVariant variant = DfgStatisticalVariant::kNative;
  DfgFrequency frequency_dfg;
  DfgTripleFrequency frequency_triples;
  DfgPerformance performance_dfg;
  DfgCaseAttributeAnalysis case_attributes;
  DfgCleanAnalysis clean;
  DfgCleanTime clean_time;
};

struct DfgNamedStatisticalAnalysis {
  std::string name;
  DfgStatisticalAnalysis analysis;
};

[[nodiscard]] DfgFrequency discover_frequency_dfg(
    const EventLog& log, const DfgStatisticalOptions& options = {});

[[nodiscard]] DfgTripleFrequency discover_frequency_triples(
    const EventLog& log, const DfgStatisticalOptions& options = {});

[[nodiscard]] DfgPerformance discover_performance_dfg(
    const EventLog& log, const DfgStatisticalOptions& options = {});

[[nodiscard]] DfgCaseAttributeAnalysis discover_case_attribute_dfg(
    const EventLog& log, const DfgStatisticalOptions& options = {});

[[nodiscard]] DfgCleanAnalysis discover_clean_dfg(
    const EventLog& log, const DfgStatisticalOptions& options = {});

// The lazy-frame implementation has its own boundary and adjacency contract.
// EventLog callers use the same deterministic ordering and projection here,
// without requiring a columnar runtime.
[[nodiscard]] DfgCleanAnalysis discover_clean_polars_dfg(
    const EventLog& log, const DfgStatisticalOptions& options = {});

[[nodiscard]] DfgCleanTime discover_clean_time_dfg(
    const EventLog& log, const DfgStatisticalOptions& options = {});

[[nodiscard]] DfgStatisticalAnalysis discover_dfg_statistics(
    const EventLog& log, const DfgStatisticalOptions& options = {});

enum class DfgCausalVariant {
  kAlpha,
  kHeuristic,
  ALPHA = kAlpha,
  HEURISTIC = kHeuristic,
};

struct DfgCausalAnalysis {
  DfgCausalVariant variant = DfgCausalVariant::kAlpha;
  std::map<DfgActivityPair, double> relations;
};

struct DfgNamedCausalAnalysis {
  std::string name;
  DfgCausalAnalysis analysis;
};

// Catalog-oriented transport wrapper: aliases can remain individually named
// even when they intentionally share the same native implementation enum.
struct DfgVariantsAnalysis {
  std::vector<DfgNamedStatisticalAnalysis> statistical;
  std::vector<DfgNamedCausalAnalysis> causal;
};

[[nodiscard]] DfgCausalAnalysis discover_dfg_causality(
    const DfgFrequency& dfg,
    DfgCausalVariant variant = DfgCausalVariant::kAlpha);

}  // namespace minera
