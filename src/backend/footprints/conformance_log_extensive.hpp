#pragma once

#include <set>

#include "minera/footprints.hpp"

namespace minera::footprints_conformance {

class LogExtensiveVariant {
 public:
  [[nodiscard]] static FootprintsExtensiveResult apply(
      const Footprint& log, const Footprint& model);

  [[nodiscard]] static FootprintsExtensiveResult compare(
      const Footprint& log, const Footprint& model,
      bool check_act_always_executed);
  // The same comparison with the model's relation configurations
  // (FootprintRelationProjection::relation_configurations(model)) supplied,
  // so comparing many traces against one model builds them once.
  [[nodiscard]] static FootprintsExtensiveResult compare(
      const Footprint& log, const Footprint& model,
      const std::set<FootprintPair>& model_relations,
      bool check_act_always_executed);

  static void finalize(FootprintsExtensiveResult& result);
};

}  // namespace minera::footprints_conformance
