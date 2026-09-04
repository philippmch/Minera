#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "minera/dfg_variants.hpp"
#include "minera/event_data.hpp"
#include "minera/event_log.hpp"

namespace minera {

struct DfgPrecisionOptions {
  // "activity" and "concept:name" select Event::activity. Other names are
  // resolved against retained first-class source metadata or generic fields.
  std::string activity_attribute = "concept:name";
  // Case field used by flat event-data adapters. It is ignored for an
  // already grouped EventLog.
  std::string case_id_attribute = "case:concept:name";
  // Optional hard cap for grouped or flat input materialization.
  std::optional<std::size_t> max_rows_to_materialize;
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
[[nodiscard]] DfgPrecisionResult evaluate_dfg_precision(
    const EventDataView& data, const DfgCleanAnalysis& model,
    const DfgPrecisionOptions& options = {});

[[nodiscard]] inline DfgPrecisionResult evaluate_dfg_precision(
    const EventStream& stream, const DfgCleanAnalysis& model,
    const DfgPrecisionOptions& options = {}) {
  return evaluate_dfg_precision(EventDataView(stream), model, options);
}
[[nodiscard]] inline DfgPrecisionResult evaluate_dfg_precision(
    const EventTable& table, const DfgCleanAnalysis& model,
    const DfgPrecisionOptions& options = {}) {
  return evaluate_dfg_precision(EventDataView(table), model, options);
}
[[nodiscard]] inline DfgPrecisionResult evaluate_dfg_precision(
    const LazyEventTable& table, const DfgCleanAnalysis& model,
    const DfgPrecisionOptions& options = {}) {
  return evaluate_dfg_precision(EventDataView(table), model, options);
}

[[nodiscard]] inline DfgPrecisionResult discover_dfg_precision(
    const EventLog& log, const DfgCleanAnalysis& model,
    const DfgPrecisionOptions& options = {}) {
  return evaluate_dfg_precision(log, model, options);
}

[[nodiscard]] inline DfgPrecisionResult discover_dfg_precision(
    const EventDataView& data, const DfgCleanAnalysis& model,
    const DfgPrecisionOptions& options = {}) {
  return evaluate_dfg_precision(data, model, options);
}

[[nodiscard]] std::string serialize_dfg_precision_json(
    const DfgPrecisionResult& result);

[[nodiscard]] inline std::string dfg_precision_json(
    const DfgPrecisionResult& result) {
  return serialize_dfg_precision_json(result);
}

}  // namespace minera
