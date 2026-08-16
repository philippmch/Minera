#pragma once

#include <cstdint>
#include <string>

#include "minera/dfg_variants.hpp"
#include "minera/event_log.hpp"

namespace minera {

struct DfgPrecisionOptions {
  // "activity" and "concept:name" select Event::activity. Other names are
  // resolved against generic event attributes.
  std::string activity_attribute = "activity";
};

struct DfgPrecisionResult {
  bool computed = false;
  double precision = 1.0;
  std::uint64_t prefix_occurrences = 0;
  std::uint64_t allowed_prefix_occurrences = 0;
  std::uint64_t activated_transitions = 0;
  std::uint64_t escaping_transitions = 0;
  std::string error;
};

[[nodiscard]] DfgPrecisionResult evaluate_dfg_precision(
    const EventLog& log, const DfgCleanAnalysis& model,
    const DfgPrecisionOptions& options = {});

[[nodiscard]] inline DfgPrecisionResult discover_dfg_precision(
    const EventLog& log, const DfgCleanAnalysis& model,
    const DfgPrecisionOptions& options = {}) {
  return evaluate_dfg_precision(log, model, options);
}

[[nodiscard]] std::string serialize_dfg_precision_json(
    const DfgPrecisionResult& result);

[[nodiscard]] inline std::string dfg_precision_json(
    const DfgPrecisionResult& result) {
  return serialize_dfg_precision_json(result);
}

}  // namespace minera
