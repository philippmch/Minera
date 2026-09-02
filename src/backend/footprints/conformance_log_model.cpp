#include "conformance_log_model.hpp"

#include <stdexcept>

#include "conformance_tree_visualization.hpp"

namespace minera::footprints_conformance {

FootprintViolations LogModelVariant::apply(const Footprint& log,
                                           const Footprint& model,
                                           FootprintsLogModelOptions options) {
  FootprintViolations result;
  if (options.strict) {
    for (const auto& pair : log.sequence) {
      if (model.sequence.find(pair) == model.sequence.end()) {
        result.insert(pair);
      }
    }
    for (const auto& pair : log.parallel) {
      if (model.parallel.find(pair) == model.parallel.end()) {
        result.insert(pair);
      }
    }
    // Keep the native interleaving relation family strict as well.  It is
    // intentionally checked independently, just as sequence and parallel are,
    // so a relation in the wrong family remains a violation.
    for (const auto& pair : log.atomic_interleaving) {
      if (model.atomic_interleaving.find(pair) == model.atomic_interleaving.end()) {
        result.insert(pair);
      }
    }
  } else {
    const auto log_projection = FootprintRelationProjection::project(log);
    const auto model_projection = FootprintRelationProjection::project(model);
    for (const auto& pair : log_projection.relations) {
      if (model_projection.relations.find(pair) ==
          model_projection.relations.end()) {
        result.insert(pair);
      }
    }
  }
  return result;
}

std::vector<FootprintViolations> LogModelVariant::apply_tracewise(
    const std::vector<Footprint>& log, const Footprint& model,
    FootprintsLogModelOptions options, std::string_view case_id_key) {
  // The reference accepts a case-id parameter for its diagnostics helper;
  // conformance itself operates on footprint objects and therefore has no
  // case-id column to inspect.  Accepting the selector here keeps the native
  // component compatible with callers that share parameter maps.
  (void)case_id_key;
  std::vector<FootprintViolations> result;
  result.reserve(log.size());
  for (const auto& trace : log) {
    result.push_back(apply(trace, model, options));
  }
  return result;
}

std::vector<LogModelDiagnostic> LogModelVariant::diagnostics(
    const EventLog& log, const std::vector<FootprintViolations>& conformance,
    std::string_view case_id_key) {
  if (log.traces.size() != conformance.size()) {
    throw std::invalid_argument(
        "log-model diagnostics must contain one result per trace");
  }
  std::vector<LogModelDiagnostic> result;
  result.reserve(conformance.size());
  for (std::size_t index = 0; index < conformance.size(); ++index) {
    const auto count = conformance[index].size();
    result.push_back({selected_case_id(log.traces[index], index, case_id_key),
                      count, count == 0});
  }
  return result;
}

}  // namespace minera::footprints_conformance
