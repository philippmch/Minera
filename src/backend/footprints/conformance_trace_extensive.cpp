#include "conformance_trace_extensive.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>

#include "conformance_log_extensive.hpp"
#include "conformance_tree_visualization.hpp"

namespace minera::footprints_conformance {

std::vector<FootprintsExtensiveResult> TraceExtensiveVariant::apply(
    const std::vector<Footprint>& log, const Footprint& model,
    FootprintsTraceExtensiveOptions options, std::string_view case_id_key) {
  (void)case_id_key;
  std::vector<FootprintsExtensiveResult> result;
  result.reserve(log.size());

  // Trace footprints carry the activity sequence.  Caching exact duplicate
  // sequences matches the reference's variant-level reuse while retaining the
  // original vector order and count of diagnostics.
  std::map<std::vector<std::string>, FootprintsExtensiveResult> cache;
  // The model's relations are the same for every trace: built once (a large
  // model has a quadratic number of them).
  const auto model_relations = log.empty()
      ? std::set<FootprintPair>{}
      : FootprintRelationProjection::relation_configurations(model);
  for (const auto& trace : log) {
    if (trace.trace) {
      const auto [it, inserted] = cache.emplace(
          *trace.trace, FootprintsExtensiveResult{});
      if (inserted) {
        it->second = LogExtensiveVariant::compare(
            trace, model, model_relations, options.enable_act_always_executed);
      }
      result.push_back(it->second);
    } else {
      result.push_back(LogExtensiveVariant::compare(
          trace, model, model_relations, options.enable_act_always_executed));
    }
  }
  return result;
}

std::vector<TraceExtensiveDiagnostic> TraceExtensiveVariant::diagnostics(
    const EventLog& log,
    const std::vector<FootprintsExtensiveResult>& conformance,
    std::string_view case_id_key) {
  if (log.traces.size() != conformance.size()) {
    throw std::invalid_argument(
        "trace conformance diagnostics must contain one result per trace");
  }
  const auto count = log.traces.size();
  std::vector<TraceExtensiveDiagnostic> result;
  result.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    const auto& item = conformance[index];
    result.push_back({selected_case_id(log.traces[index], index, case_id_key),
                      item.footprints.size(), item.start_activities.size(),
                      item.end_activities.size(),
                      item.activities_always_happening.size(),
                      item.min_length_fit, item.is_footprints_fit});
  }
  return result;
}

}  // namespace minera::footprints_conformance
