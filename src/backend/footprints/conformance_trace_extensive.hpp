#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "minera/footprints.hpp"

namespace minera::footprints_conformance {

struct TraceExtensiveDiagnostic {
  std::string case_id;
  std::size_t footprints_violations = 0;
  std::size_t start_activities_violations = 0;
  std::size_t end_activities_violations = 0;
  std::size_t act_always_happening_violations = 0;
  bool min_length_fit = true;
  bool is_fit = true;
};

class TraceExtensiveVariant {
 public:
  [[nodiscard]] static std::vector<FootprintsExtensiveResult> apply(
      const std::vector<Footprint>& log, const Footprint& model,
      FootprintsTraceExtensiveOptions options = {},
      std::string_view case_id_key = {});

  [[nodiscard]] static std::vector<TraceExtensiveDiagnostic> diagnostics(
      const EventLog& log,
      const std::vector<FootprintsExtensiveResult>& conformance,
      std::string_view case_id_key = {});
};

}  // namespace minera::footprints_conformance
