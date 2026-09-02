#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "minera/footprints.hpp"

namespace minera::footprints_conformance {

struct LogModelDiagnostic {
  std::string case_id;
  std::size_t num_violations = 0;
  bool is_fit = true;
};

class LogModelVariant {
 public:
  [[nodiscard]] static FootprintViolations apply(
      const Footprint& log, const Footprint& model,
      FootprintsLogModelOptions options = {});

  [[nodiscard]] static std::vector<FootprintViolations> apply_tracewise(
      const std::vector<Footprint>& log, const Footprint& model,
      FootprintsLogModelOptions options = {},
      std::string_view case_id_key = {});

  [[nodiscard]] static std::vector<LogModelDiagnostic> diagnostics(
      const EventLog& log, const std::vector<FootprintViolations>& conformance,
      std::string_view case_id_key = {});
};

}  // namespace minera::footprints_conformance
