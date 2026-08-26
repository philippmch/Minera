#include "minera/resource_analysis.hpp"

#include <limits>
#include <optional>
#include <stdexcept>
#include <unordered_map>

namespace minera {
namespace {

std::uint64_t handover_key(std::size_t source, std::size_t target) {
  constexpr auto max_u32 =
      static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max());
  if (source > max_u32 || target > max_u32) {
    throw std::overflow_error(
        "Resource network supports up to 2^32 distinct resources");
  }
  return (static_cast<std::uint64_t>(source) << 32U) |
         static_cast<std::uint64_t>(target);
}

}  // namespace

ResourceNetwork discover_resource_network(const EventLog& log) {
  ResourceNetwork network;
  std::unordered_map<std::string, std::size_t> ids;
  std::unordered_map<std::uint64_t, std::size_t> handover_ids;

  const auto intern = [&](const std::string& resource) {
    auto [it, inserted] = ids.emplace(resource, network.resources.size());
    if (inserted) {
      network.resources.push_back(resource);
      network.event_counts.push_back(0);
    }
    return it->second;
  };

  for (const auto& trace : log.traces) {
    std::optional<std::size_t> previous;
    for (const auto& event : trace.events) {
      if (event.resource.empty()) {
        // An untimed gap in the resource attribute breaks the handover chain
        // rather than silently bridging across the missing worker.
        previous.reset();
        continue;
      }
      const auto resource = intern(event.resource);
      ++network.event_counts[resource];

      if (previous) {
        const auto key = handover_key(*previous, resource);
        auto [it, inserted] =
            handover_ids.emplace(key, network.handovers.size());
        if (inserted) {
          network.handovers.push_back(
              ResourceHandover{*previous, resource, 0});
        }
        ++network.handovers[it->second].count;
        ++network.total_handovers;
      }
      previous = resource;
    }
  }

  return network;
}

}  // namespace minera
