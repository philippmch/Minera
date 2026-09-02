#pragma once

#include "minera/footprints.hpp"

namespace minera::footprints_conformance {

class LogExtensiveVariant {
 public:
  [[nodiscard]] static FootprintsExtensiveResult apply(
      const Footprint& log, const Footprint& model);

  [[nodiscard]] static FootprintsExtensiveResult compare(
      const Footprint& log, const Footprint& model,
      bool check_act_always_executed);

  static void finalize(FootprintsExtensiveResult& result);
};

}  // namespace minera::footprints_conformance
