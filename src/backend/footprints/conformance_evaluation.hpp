#pragma once

#include <vector>

#include "minera/footprints.hpp"

namespace minera::footprints_conformance {

class EvaluationUtility {
 public:
  [[nodiscard]] static Footprint flatten(
      const std::vector<Footprint>& traces);

  [[nodiscard]] static FootprintsExtensiveResult flatten_conformance(
      const std::vector<FootprintsExtensiveResult>& conformance);

  [[nodiscard]] static double precision(const Footprint& log,
                                        const Footprint& model);

  [[nodiscard]] static double fitness(
      const Footprint& log, const Footprint& model,
      const FootprintsExtensiveResult& conformance);

  [[nodiscard]] static FootprintsFitnessSummary summarize(
      const std::vector<Footprint>& log, const Footprint& model,
      const std::vector<FootprintsExtensiveResult>& conformance);
};

}  // namespace minera::footprints_conformance
