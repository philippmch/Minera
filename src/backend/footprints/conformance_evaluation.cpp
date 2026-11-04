#include "conformance_evaluation.hpp"

#include <cstdint>
#include <limits>
#include <stdexcept>

#include "conformance_tree_visualization.hpp"

namespace minera::footprints_conformance {

Footprint EvaluationUtility::flatten(const std::vector<Footprint>& traces) {
  Footprint result;
  for (const auto& trace : traces) {
    for (const auto& [pair, count] : trace.dfg) {
      // Saturating addition preserves the monotone frequency ordering even
      // for a hand-built object whose counts approach the uint64 limit.
      if (std::numeric_limits<std::uint64_t>::max() - result.dfg[pair] < count) {
        result.dfg[pair] = std::numeric_limits<std::uint64_t>::max();
      } else {
        result.dfg[pair] += count;
      }
    }
    result.sequence.insert(trace.sequence.begin(), trace.sequence.end());
    result.parallel.insert(trace.parallel.begin(), trace.parallel.end());
    result.atomic_interleaving.insert(trace.atomic_interleaving.begin(),
                                     trace.atomic_interleaving.end());
    result.start_activities.insert(trace.start_activities.begin(),
                                  trace.start_activities.end());
    result.end_activities.insert(trace.end_activities.begin(),
                                 trace.end_activities.end());
    result.activities.insert(trace.activities.begin(), trace.activities.end());
  }
  return result;
}

FootprintsExtensiveResult EvaluationUtility::flatten_conformance(
    const std::vector<FootprintsExtensiveResult>& conformance) {
  FootprintsExtensiveResult result;
  for (const auto& item : conformance) {
    result.footprints.insert(item.footprints.begin(), item.footprints.end());
    result.start_activities.insert(item.start_activities.begin(),
                                  item.start_activities.end());
    result.end_activities.insert(item.end_activities.begin(),
                                 item.end_activities.end());
    result.activities_always_happening.insert(
        item.activities_always_happening.begin(),
        item.activities_always_happening.end());
    result.min_length_fit = result.min_length_fit && item.min_length_fit;
  }
  result.is_footprints_fit =
      result.footprints.empty() && result.start_activities.empty() &&
      result.end_activities.empty() &&
      result.activities_always_happening.empty() && result.min_length_fit;
  result.is_fit = result.is_footprints_fit;
  return result;
}

double EvaluationUtility::precision(const Footprint& log,
                                    const Footprint& model) {
  const auto model_configurations =
      FootprintRelationProjection::relation_configurations(model);
  if (model_configurations.empty()) return 1.0;
  const auto log_configurations =
      FootprintRelationProjection::relation_configurations(log);
  std::size_t shared = 0;
  for (const auto& pair : log_configurations) {
    if (model_configurations.find(pair) != model_configurations.end()) {
      ++shared;
    }
  }
  return static_cast<double>(shared) /
         static_cast<double>(model_configurations.size());
}

double EvaluationUtility::fitness(
    const Footprint& log, const Footprint& /*model*/,
    const FootprintsExtensiveResult& conformance) {
  if (log.dfg.empty()) return 1.0;

  // The reference sums the integer counts exactly, converts each sum to a
  // double once and evaluates the formula in double. Integer sums followed
  // by the same double steps reproduce that on every platform; long double
  // arithmetic rounded differently on x86 only.
  std::uint64_t dfg_total = 0;
  for (const auto& [pair, count] : log.dfg) {
    (void)pair;
    dfg_total += static_cast<std::uint64_t>(count);
  }
  if (dfg_total == 0) return 1.0;

  std::uint64_t deviation_total = 0;
  for (const auto& pair : conformance.footprints) {
    const auto it = log.dfg.find(pair);
    if (it != log.dfg.end()) {
      deviation_total += static_cast<std::uint64_t>(it->second);
    }
  }

  const std::uint64_t number_relations = log.sequence.size() +
                                         log.parallel.size() +
                                         log.atomic_interleaving.size();
  const std::uint64_t number_boundary =
      log.start_activities.size() + log.end_activities.size();
  const std::uint64_t denominator = number_relations + number_boundary;
  if (denominator == 0) return 1.0;

  const double sum_dfg = static_cast<double>(dfg_total);
  const double sum_deviations = static_cast<double>(deviation_total);
  const double relation_fit =
      (1.0 - sum_deviations / sum_dfg) * static_cast<double>(number_relations);
  const auto boundary_fit = static_cast<double>(
      static_cast<std::int64_t>(number_boundary) -
      static_cast<std::int64_t>(conformance.start_activities.size()) -
      static_cast<std::int64_t>(conformance.end_activities.size()));
  return (relation_fit + boundary_fit) / static_cast<double>(denominator);
}

FootprintsFitnessSummary EvaluationUtility::summarize(
    const std::vector<Footprint>& log, const Footprint& model,
    const std::vector<FootprintsExtensiveResult>& conformance) {
  if (log.size() != conformance.size()) {
    throw std::invalid_argument(
        "footprint fitness requires one conformance result per trace");
  }
  if (conformance.empty()) return FootprintsFitnessSummary{};
  FootprintsFitnessSummary result;
  std::size_t fit = 0;
  for (const auto& item : conformance) {
    if (item.is_footprints_fit) ++fit;
  }
  result.perc_fit_traces =
      100.0 * static_cast<double>(fit) /
      static_cast<double>(conformance.size());

  const auto flattened_conformance = flatten_conformance(conformance);
  result.log_fitness =
      fitness(flatten(log), model, flattened_conformance);
  return result;
}

}  // namespace minera::footprints_conformance
