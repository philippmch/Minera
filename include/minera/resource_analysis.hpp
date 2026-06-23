#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "minera/event_log.hpp"

namespace minera {

// A handover of work from one resource to another: when an event performed by
// `source` is directly followed (within a case) by an event performed by
// `target`. Indices refer to ResourceNetwork::resources.
struct ResourceHandover {
  std::size_t source = 0;
  std::size_t target = 0;
  std::uint64_t count = 0;
};

// The organizational (resource) perspective of a log: who does the work and how
// work is handed between them. The handover-of-work social network counts, for
// every directly-following pair of events inside a case, a handover from the
// first event's resource to the second's — the classic measure of how often one
// worker passes a case to another.
struct ResourceNetwork {
  std::vector<std::string> resources;       // distinct resource names
  std::vector<std::uint64_t> event_counts;  // events handled, per resource
  std::vector<ResourceHandover> handovers;  // handover-of-work edges
  std::uint64_t total_handovers = 0;        // sum of all handover counts
};

// Builds the handover-of-work network from a log's resource attribute. Events
// without a resource are ignored; a handover is counted only when both adjacent
// events carry a resource (self-handovers, where a worker continues the case,
// are included).
[[nodiscard]] ResourceNetwork discover_resource_network(const EventLog& log);

}  // namespace minera
