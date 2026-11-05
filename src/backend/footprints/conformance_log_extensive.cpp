#include "conformance_log_extensive.hpp"

#include "conformance_tree_visualization.hpp"

namespace minera::footprints_conformance {

FootprintsExtensiveResult LogExtensiveVariant::apply(const Footprint& log,
                                                     const Footprint& model) {
  return compare(log, model, false);
}

FootprintsExtensiveResult LogExtensiveVariant::compare(
    const Footprint& log, const Footprint& model,
    bool check_act_always_executed) {
  return compare(log, model,
                 FootprintRelationProjection::relation_configurations(model),
                 check_act_always_executed);
}

FootprintsExtensiveResult LogExtensiveVariant::compare(
    const Footprint& log, const Footprint& model,
    const std::set<FootprintPair>& model_relations,
    bool check_act_always_executed) {
  FootprintsExtensiveResult result;
  // Only the projections' relation sets are compared.
  const auto log_relations = FootprintRelationProjection::relation_configurations(log);
  for (const auto& pair : log_relations) {
    if (model_relations.find(pair) == model_relations.end()) {
      result.footprints.insert(pair);
    }
  }

  // A missing model field means that the corresponding check is not part of
  // that model variant.  The log field is treated as an empty set when it is
  // absent, which keeps hand-built adapter objects deterministic and safe.
  if (model.start_activities_present) {
    for (const auto& activity : log.start_activities) {
      if (model.start_activities.find(activity) == model.start_activities.end()) {
        result.start_activities.insert(activity);
      }
    }
  }
  if (model.end_activities_present) {
    for (const auto& activity : log.end_activities) {
      if (model.end_activities.find(activity) == model.end_activities.end()) {
        result.end_activities.insert(activity);
      }
    }
  }
  if (check_act_always_executed && model.activities_always_happening) {
    for (const auto& activity : *model.activities_always_happening) {
      if (log.activities.find(activity) == log.activities.end()) {
        result.activities_always_happening.insert(activity);
      }
    }
  }

  if (log.min_trace_length_present && model.min_trace_length_present &&
      log.min_trace_length && model.min_trace_length) {
    result.min_length_fit = *log.min_trace_length >= *model.min_trace_length;
  }
  finalize(result);
  return result;
}

void LogExtensiveVariant::finalize(FootprintsExtensiveResult& result) {
  result.is_footprints_fit =
      result.footprints.empty() && result.start_activities.empty() &&
      result.end_activities.empty() &&
      result.activities_always_happening.empty() && result.min_length_fit;
  result.is_fit = result.is_footprints_fit;
}

}  // namespace minera::footprints_conformance
