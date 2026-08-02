#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "minera/event_log.hpp"

namespace minera {

struct LocalDiagnosticsOptions {
  std::string group_attribute = "org:group";
  std::string resource_attribute; // empty uses Event::resource
  std::string activity_attribute; // empty uses Event::activity
  std::map<std::string, std::vector<std::string>> explicit_groups;
  std::size_t max_groups = 1000;
  std::size_t max_resources = 10000;
  std::size_t max_rows = 100000;
};

struct LocalDiagnosticsDiagnostics {
  bool computed = false;
  bool output_truncated = false;
  std::size_t trace_count = 0;
  std::size_t event_count = 0;
  std::size_t group_count = 0;
  std::size_t resource_count = 0;
  std::size_t row_count = 0;
  std::size_t skipped_events = 0;
  std::string message;
};

struct LocalDiagnosticsResult {
  std::map<std::string, std::map<std::string, double>> group_relative_focus;
  std::map<std::string, std::map<std::string, double>> group_relative_stake;
  std::map<std::string, std::map<std::string, double>> group_coverage;
  std::map<std::string, std::map<std::string, std::map<std::string, std::uint64_t>>> group_member_contribution;
  LocalDiagnosticsDiagnostics diagnostics;
};

[[nodiscard]] LocalDiagnosticsResult discover_local_diagnostics(
    const EventLog& log, const LocalDiagnosticsOptions& options = {});

// Named entry points mirroring the two reference application modes.
[[nodiscard]] LocalDiagnosticsResult apply_from_group_attribute(
    const EventLog& log, const LocalDiagnosticsOptions& options = {});
[[nodiscard]] LocalDiagnosticsResult apply_from_clustering_or_roles(
    const EventLog& log,
    const std::map<std::string, std::vector<std::string>>& groups,
    LocalDiagnosticsOptions options = {});

[[nodiscard]] std::string local_diagnostics_json(const LocalDiagnosticsResult& result);

}  // namespace minera
