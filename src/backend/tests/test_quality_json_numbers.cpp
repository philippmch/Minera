#include "minera/json_export.hpp"

#include <bit>
#include <cassert>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <locale>
#include <string>
#include <vector>

namespace {
std::string_view scalar(const std::string& json, const std::string& field) {
  const auto start = json.find('"' + field + "\": ");
  assert(start != std::string::npos);
  const auto first = start + field.size() + 4;
  const auto last = json.find_first_of(",}", first);
  return std::string_view(json).substr(first, last - first);
}
void same_number(const std::string& json, const std::string& field, double value) {
  const auto token = scalar(json, field);
  if (!std::isfinite(value)) { assert(token == "null"); return; }
  double parsed = 0;
  const auto result = std::from_chars(token.data(), token.data() + token.size(), parsed);
  assert(result.ec == std::errc{} && result.ptr == token.data() + token.size());
  assert(std::bit_cast<std::uint64_t>(parsed) == std::bit_cast<std::uint64_t>(value));
  if (value == 0 && std::signbit(value)) assert(token == "-0.0");
}
void quality_numbers_roundtrip() {
  const std::vector<double> values = {1.0 / 3.0, std::nextafter(1.0, 2.0),
      -0.0, 0.0, 0.9, 1e-200, -1e200, std::numeric_limits<double>::denorm_min(),
      std::numeric_limits<double>::min(), std::numeric_limits<double>::max(),
      std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(),
      std::numeric_limits<double>::quiet_NaN()};
  for (const auto value : values) {
    minera::SimplicityResult simplicity;
    simplicity.simplicity = simplicity.complexity = simplicity.value = value;
    const auto sj = minera::simplicity_json(simplicity);
    for (const auto* field : {"simplicity", "complexity", "value"}) same_number(sj, field, value);
    minera::ReplayFitnessResult fitness;
    fitness.fitness = fitness.log_fitness = fitness.average_trace_fitness = value;
    fitness.percentage_fit_traces = fitness.total_cost = value;
    const auto fj = minera::replay_fitness_json(fitness);
    for (const auto* field : {"fitness", "log_fitness", "average_trace_fitness",
                             "percentage_of_fitting_traces", "total_cost"}) same_number(fj, field, value);
    minera::PrecisionResult precision;
    precision.precision = value;
    same_number(minera::precision_json(precision), "precision", value);
    minera::GeneralizationResult generalization;
    generalization.generalization = value;
    same_number(minera::generalization_json(generalization), "score", value);
    minera::CompositeEvaluationResult composite;
    composite.precision = composite.simplicity = composite.generalization = value;
    composite.fitness = composite.metrics_average_weight = composite.fscore = value;
    composite.fitness_weight = composite.precision_weight = value;
    composite.simplicity_weight = composite.generalization_weight = value;
    minera::ModelEvaluation model;
    model.composite = composite;
    const auto cj = minera::model_evaluation_json(model);
    for (const auto* field : {"fitness_score", "precision", "generalization", "simplicity",
                             "metrics_average_weight", "fscore"})
      same_number(cj, field, value);
    const auto weights = cj.substr(cj.find("\"weights\": {"));
    for (const auto* field : {"fitness", "precision", "simplicity", "generalization"})
      same_number(weights, field, value);
    minera::EmdResult emd;
    emd.distance = value;
    same_number(minera::emd_json(emd), "distance", value);
  }
}
struct CommaDecimal final : std::numpunct<char> {
  char do_decimal_point() const override { return ','; }
};
}  // namespace
int main() {
  quality_numbers_roundtrip();
  const auto previous = std::locale();
  std::locale::global(std::locale(previous, new CommaDecimal));
  quality_numbers_roundtrip();
  std::locale::global(previous);
}
