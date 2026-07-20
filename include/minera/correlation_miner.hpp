#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "minera/dfg_variants.hpp"

namespace minera {

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
  std::string start_timestamp_attribute;
  bool exact_time_matching = false;
  std::size_t sample_size = 100000;
};

using CorrelationPerformanceDfg = std::map<DfgActivityPair, double>;

struct CorrelationMinerResult {
  CorrelationVariant variant = CorrelationVariant::kClassic;
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

[[nodiscard]] CorrelationMinerResult discover_correlation_miner(
    const EventLog& log, const CorrelationMinerOptions& options = {});

}  // namespace minera
