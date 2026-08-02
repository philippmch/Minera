#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "minera/event_log.hpp"

namespace minera {

struct RoleDiscoveryOptions {
  double threshold = 0.65;
  std::size_t max_resources = 256;
  std::size_t max_activities = 10000;
};

struct RoleResourceCount { std::string name; std::size_t count = 0; };
struct DiscoveredRole {
  std::vector<std::string> activities;
  std::vector<RoleResourceCount> resources;
};
struct RoleDiscoveryDiagnostics {
  bool computed = false;
  bool output_truncated = false;
  std::size_t trace_count = 0, event_count = 0;
  std::size_t activity_count = 0, resource_count = 0, role_count = 0;
  std::string message;
};
struct RoleDiscoveryResult {
  std::vector<DiscoveredRole> roles;
  RoleDiscoveryDiagnostics diagnostics;
};

[[nodiscard]] RoleDiscoveryResult discover_roles(
    const EventLog& log, const RoleDiscoveryOptions& options = {});
inline RoleDiscoveryResult mine_roles(const EventLog& log,
                                      const RoleDiscoveryOptions& options = {}) {
  return discover_roles(log, options);
}

}  // namespace minera
