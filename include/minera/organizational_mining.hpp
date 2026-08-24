#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "minera/event_log.hpp"

namespace minera {

enum class OrganizationalSnaVariant {
  kHandover,
  kWorkingTogether,
  kSubcontracting,
  kJointActivities,
  kHandoverPandas,
  kWorkingTogetherPandas,
  kSubcontractingPandas,
  kJointActivitiesPandas,
  HANDOVER = kHandover,
  WORKING_TOGETHER = kWorkingTogether,
  SUBCONTRACTING = kSubcontracting,
  JOINTACTIVITIES = kJointActivities,
  HANDOVER_PANDAS = kHandoverPandas,
  WORKING_TOGETHER_PANDAS = kWorkingTogetherPandas,
  SUBCONTRACTING_PANDAS = kSubcontractingPandas,
  JOINTACTIVITIES_PANDAS = kJointActivitiesPandas,
};

struct OrganizationalSnaOptions {
  OrganizationalSnaVariant variant = OrganizationalSnaVariant::kHandover;
  // Selectors follow the standard event attribute names by default.  A
  // non-standard key is read from Event::attributes, which keeps the log and
  // dataframe-style variants on one typed surface.
  std::string activity_attribute = "concept:name";
  std::string resource_attribute = "org:resource";
  // Case selector used to regroup events for the sequence-based metrics.
  // The default preserves the EventLog trace boundaries.
  std::string case_id_attribute = "case:concept:name";
  double beta = 0.0;          // distance decay for handover weights
  std::size_t n = 2;          // maximum subcontracting gap
  bool metric_normalization = false;
  std::size_t max_resources = 256;
  std::size_t max_edges = 10000;
};

struct OrganizationalSnaEdge {
  std::size_t source = 0;
  std::size_t target = 0;
  double weight = 0.0;
};

struct OrganizationalSnaDiagnostics {
  bool computed = false;
  bool output_truncated = false;
  std::size_t trace_count = 0;
  std::size_t event_count = 0;
  std::size_t resource_count = 0;
  std::size_t edge_count = 0;
  std::string message;
};

struct OrganizationalSnaResult {
  OrganizationalSnaVariant variant = OrganizationalSnaVariant::kHandover;
  bool directed = true;
  std::vector<std::string> resources;
  std::vector<OrganizationalSnaEdge> edges;
  OrganizationalSnaDiagnostics diagnostics;
};

[[nodiscard]] OrganizationalSnaResult discover_organizational_sna(
    const EventLog& log, const OrganizationalSnaOptions& options = {});

inline OrganizationalSnaResult mine_organizational_sna(
    const EventLog& log, const OrganizationalSnaOptions& options = {}) {
  return discover_organizational_sna(log, options);
}

inline OrganizationalSnaResult discover_sna(
    const EventLog& log, const OrganizationalSnaOptions& options = {}) {
  return discover_organizational_sna(log, options);
}

using OrganizationalSnaResultVariant = OrganizationalSnaVariant;
using OrganizationalSnaEdgeRecord = OrganizationalSnaEdge;

}  // namespace minera
